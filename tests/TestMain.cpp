// Standalone unit tests for the pure (iPlug2-independent) helpers in src/.
// Build:  cmake -S tests -B build-tests && cmake --build build-tests && ./build-tests/OneBitPlusTests
// Or:     g++ -std=c++17 -I. tests/TestMain.cpp -o OneBitPlusTests   (run from the repo root)

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <chrono>
#include <functional>
#include <thread>
#include <numeric>
#include <string>
#include <vector>

#include "src/BitCompression.h"
#include "src/IAudioEffect.h"
#include "src/SampleData.h"
#include "src/SampleImportWorker.h"
#include "src/StateFormat.h"
#include "src/PipelinePresets.h"
#include "src/PipelineModel.h"
#include "src/SampleRender.h"
#include "src/Spectrogram.h"
#include "src/ChainPresets.h"
#include "src/SamplesModel.h"
#include "src/SampleLoader.h"
#include "src/SampleTools.h"

// ---------------------------------------------------------------- mini test framework
static int gFailures = 0;
static int gChecks = 0;

#define CHECK(cond)                                                                \
  do                                                                               \
  {                                                                                \
    ++gChecks;                                                                     \
    if (!(cond))                                                                   \
    {                                                                              \
      ++gFailures;                                                                 \
      std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                \
    }                                                                              \
  } while (0)

#define CHECK_NEAR(a, b, eps) CHECK(std::fabs(static_cast<double>(a) - static_cast<double>(b)) <= (eps))

struct TestCase
{
  const char* name;
  std::function<void()> fn;
};

static std::vector<TestCase>& TestRegistry()
{
  static std::vector<TestCase> r;
  return r;
}

struct Registrar
{
  Registrar(const char* n, std::function<void()> f) { TestRegistry().push_back({n, std::move(f)}); }
};

#define TEST(name)                                     \
  static void name();                                  \
  static Registrar name##_registrar(#name, name);      \
  static void name()

// ---------------------------------------------------------------- helpers
static void PutU16(std::vector<uint8_t>& b, uint16_t v)
{
  b.push_back(v & 0xFF);
  b.push_back(v >> 8);
}
static void PutU32(std::vector<uint8_t>& b, uint32_t v)
{
  for (int i = 0; i < 4; ++i)
    b.push_back((v >> (8 * i)) & 0xFF);
}

// Writes a minimal RIFF/WAVE file. `data` is the raw interleaved sample payload.
static std::string WriteWav(const std::string& name, uint16_t format, uint16_t channels, uint32_t rate, uint16_t bits, const std::vector<uint8_t>& data)
{
  std::vector<uint8_t> b;
  b.insert(b.end(), {'R', 'I', 'F', 'F'});
  PutU32(b, static_cast<uint32_t>(36 + data.size()));
  b.insert(b.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
  PutU32(b, 16);
  PutU16(b, format);
  PutU16(b, channels);
  PutU32(b, rate);
  PutU32(b, rate * channels * bits / 8);
  PutU16(b, static_cast<uint16_t>(channels * bits / 8));
  PutU16(b, bits);
  b.insert(b.end(), {'d', 'a', 't', 'a'});
  PutU32(b, static_cast<uint32_t>(data.size()));
  b.insert(b.end(), data.begin(), data.end());
  std::ofstream f(name, std::ios::binary);
  f.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
  return name;
}

static double MeanBipolar(const std::vector<bool>& bits, size_t from = 0)
{
  double sum = 0.0;
  for (size_t i = from; i < bits.size(); ++i)
    sum += bits[i] ? 1.0 : -1.0;
  return sum / static_cast<double>(bits.size() - from);
}

// ---------------------------------------------------------------- SampleLoader
TEST(Loader_16bitMono)
{
  std::vector<uint8_t> d;
  for (int16_t v : {int16_t(0), int16_t(16384), int16_t(-16384), int16_t(32767)})
    PutU16(d, static_cast<uint16_t>(v));
  auto path = WriteWav("t_16m.wav", 1, 1, 44100, 16, d);
  LoadedSample s = SampleLoader().load_file_static(path.c_str());
  CHECK(s.sampleBuffer.size() == 4);
  CHECK(s.header.SamplesPerSec == 44100);
  CHECK(s.sampleBuffer[1] == (16384 << 16));
  CHECK(s.sampleBuffer[2] == static_cast<int32_t>(-16384 * 65536));
  std::remove(path.c_str());
}

TEST(Loader_24bitSignExtension)
{
  std::vector<uint8_t> d = {0xFF, 0xFF, 0xFF, /* -1 */ 0x00, 0x00, 0x40 /* 0x400000 */};
  auto path = WriteWav("t_24m.wav", 1, 1, 48000, 24, d);
  LoadedSample s = SampleLoader().load_file_static(path.c_str());
  CHECK(s.sampleBuffer.size() == 2);
  CHECK(s.sampleBuffer[0] == -256); // -1 << 8
  CHECK(s.sampleBuffer[1] == (0x400000 << 8));
  std::remove(path.c_str());
}

TEST(Loader_FloatStereoToMono)
{
  std::vector<uint8_t> d;
  for (float f : {0.5f, -0.5f, 0.75f, 0.75f})
  {
    uint32_t u;
    std::memcpy(&u, &f, 4);
    PutU32(d, u);
  }
  auto path = WriteWav("t_f32s.wav", 3, 2, 44100, 32, d);
  LoadedSample s = SampleLoader().load_file_static(path.c_str());
  CHECK(s.isStereo);
  CHECK(s.sampleBuffer.size() == 2);
  CHECK(std::abs(s.sampleBuffer[0]) <= 1); // (0.5 + -0.5) / 2
  CHECK(std::abs(s.sampleBuffer[1] - static_cast<int32_t>(0.75 * 2147483647.0)) < 256);
  std::remove(path.c_str());
}

TEST(Loader_FloatFullScaleDoesNotOverflow)
{
  std::vector<uint8_t> d;
  for (float f : {1.0f, -1.0f, 2.0f})
  {
    uint32_t u;
    std::memcpy(&u, &f, 4);
    PutU32(d, u);
  }
  auto path = WriteWav("t_f32full.wav", 3, 1, 44100, 32, d);
  LoadedSample s = SampleLoader().load_file_static(path.c_str());
  CHECK(s.sampleBuffer.size() == 3);
  CHECK(s.sampleBuffer[0] == 2147483647);  // was INT32_MIN (sign flip) before the fix
  CHECK(s.sampleBuffer[1] == -2147483647);
  CHECK(s.sampleBuffer[2] == 2147483647);  // clipped
  std::remove(path.c_str());
}

TEST(Loader_RejectsNonWav)
{
  LoadedSample s = SampleLoader().load_file_static("does_not_exist.txt");
  CHECK(s.sampleBuffer.empty());
  s = SampleLoader().load_file_static(nullptr);
  CHECK(s.sampleBuffer.empty());
}

// ---------------------------------------------------------------- Resampler
TEST(Resample_IdentityAndLength)
{
  std::vector<int32_t> in(1000, 1 << 20);
  CHECK(SampleTools::Resample(in, 44100, 44100, ResampleAlgo::Lanczos) == in);
  for (auto algo : {ResampleAlgo::Nearest, ResampleAlgo::Linear, ResampleAlgo::Lanczos})
  {
    auto up = SampleTools::Resample(in, 44100, 88200, algo);
    auto down = SampleTools::Resample(in, 48000, 24000, algo);
    CHECK(up.size() == 2000);
    CHECK(down.size() == 500);
    // DC must be preserved
    CHECK(std::abs(up[1000] - (1 << 20)) <= 2);
    CHECK(std::abs(down[250] - (1 << 20)) <= 2);
  }
}

// ---------------------------------------------------------------- 1-bit reduction
TEST(ReduceToOneBit_PacksMsbFirst)
{
  AudioPipeline p;
  p.quantizer = std::make_shared<RawQuantizer>();
  // + - - - - - - -   + + - - - - - -   + (padded)
  std::vector<int32_t> in = {1, -1, -1, -1, -1, -1, -1, -1, 1, 1, -1, -1, -1, -1, -1, -1, 1};
  auto bytes = SampleTools::ReduceToOneBit(in, 44100, p);
  CHECK(bytes.size() == 3);
  CHECK(static_cast<uint8_t>(bytes[0]) == 0x80);
  CHECK(static_cast<uint8_t>(bytes[1]) == 0xC0);
  CHECK(static_cast<uint8_t>(bytes[2]) == 0x80);
}

TEST(ReduceToOneBit_EmptyOrNoQuantizer)
{
  AudioPipeline p;
  CHECK(SampleTools::ReduceToOneBit({}, 44100, p).empty());
  p.quantizer = nullptr;
  CHECK(SampleTools::ReduceToOneBit({1, 2, 3}, 44100, p).empty());
}

// ---------------------------------------------------------------- Quantizers
TEST(DeltaSigma_TracksDc)
{
  std::vector<double> in(4000, 0.25);
  auto bits = DeltaSigmaQuantizer().Quantize(in, 44100);
  CHECK(bits.size() == in.size());
  CHECK_NEAR(MeanBipolar(bits), 0.25, 0.01);
}

TEST(Trellis_LengthAndDc)
{
  // short input -> psychoacoustic (single pass) path, long input -> chunked path
  for (size_t n : {size_t(2000), size_t(50000)})
  {
    std::vector<double> in(n, -0.3);
    auto bits = TrellisQuantizer().Quantize(in, 44100);
    CHECK(bits.size() == n);
    CHECK_NEAR(MeanBipolar(bits, 100), -0.3, 0.05);
  }
}

TEST(Pipeline_ToString)
{
  AudioPipeline p = CreateDefaultPipeline();
  CHECK(p.ToString() == "NormalizeEffect->SaturateEffect(2)->TrellisQuantizer(maxRam=67108864)");
}

// ---------------------------------------------------------------- Effects
TEST(Effects_NormalizeAndClip)
{
  std::vector<double> b = {0.1, -0.5, 0.25};
  NormalizeEffect().Process(b, 44100);
  CHECK_NEAR(b[1], -1.0, 1e-12);
  CHECK_NEAR(b[0], 0.2, 1e-12);
  ClippingEffect(0.5, false).Process(b, 44100);
  CHECK_NEAR(b[1], -0.5, 1e-12);
}

TEST(Effects_Trim)
{
  std::vector<double> b(1000, 1.0);
  TrimEffect(10.0, 5.0).Process(b, 10000); // start 100 samples, length 50
  CHECK(b.size() == 50);
}

// ---------------------------------------------------------------- SampleManager
TEST(SampleManager_VelocityMapsOneToOne)
{
  SampleData::SampleManager m;
  const int8_t a[] = {1, 2};
  const int8_t b[] = {3};
  CHECK(m.AddSample(a, 2, 48000) == 0);
  CHECK(m.AddSample(b, 1, 44100) == 1);
  auto s1 = m.GetSampleForVelocity(1.0 / 127.0);
  CHECK(s1.size == 2 && s1.data[1] == 2 && s1.sampleRate == 48000);
  auto s2 = m.GetSampleForVelocity(2.0 / 127.0);
  CHECK(s2.size == 1 && s2.data[0] == 3);
  auto s3 = m.GetSampleForVelocity(3.0 / 127.0); // unused slot -> silence
  CHECK(s3.size == 1 && s3.data[0] == 0);
  m.ClearDynamicSamples();
  CHECK(m.DynamicCount() == 0);
}

TEST(SampleManager_SlotMetadataAndLimit)
{
  SampleData::SampleManager m;
  const int8_t a[] = {1};
  CHECK(m.AddSample(a, 1, 48000, "kick.wav", "Normalize->Trellis") == 0);
  auto slots = m.CopyDynamicSlots();
  CHECK(slots.size() == 1 && slots[0].path == "kick.wav" && slots[0].pipeline == "Normalize->Trellis" && slots[0].sampleRate == 48000);
  for (size_t i = 1; i < SampleData::SampleManager::kMaxDynamicSlots; ++i)
    CHECK(m.AddSample(a, 1) >= 0);
  CHECK(m.AddSample(a, 1) == -1); // velocities 1..127 are all taken
  CHECK(m.GetSampleForVelocity(1.0).size == 1); // velocity 127 -> last slot
}

TEST(SampleManager_SlotOperationsKeepVelocities)
{
  SampleData::SampleManager m;
  const int8_t a[] = {1}, b[] = {2}, c[] = {3}, d[] = {4};
  CHECK(m.AddSample(a, 1) == 0 && m.AddSample(b, 1) == 1 && m.AddSample(c, 1) == 2);

  // clearing the middle slot leaves a hole: the slot after it keeps its velocity
  CHECK(m.ClearSlot(1));
  CHECK(m.DynamicCount() == 3 && m.FirstFreeSlot() == 1);
  CHECK(m.GetSampleForVelocity(2.0 / 127.0).data == SampleData::kSilence); // velocity 2 is silent now
  CHECK(m.GetSampleForVelocity(3.0 / 127.0).data[0] == 3);                 // velocity 3 is unchanged

  // the next sample fills the hole instead of being appended
  CHECK(m.AddSample(d, 1) == 1 && m.DynamicCount() == 3);
  CHECK(m.GetSampleForVelocity(2.0 / 127.0).data[0] == 4);

  // assign to a specific slot far away: slots in between stay free
  SampleData::Slot s;
  s.data = {9};
  s.path = "far.wav";
  CHECK(m.SetSlot(9, s));
  CHECK(m.DynamicCount() == 10 && m.GetSlot(5)->Empty() && m.GetSlot(9)->path == "far.wav");
  CHECK(m.FirstFreeSlot() == 3);
  CHECK(m.GetSampleForVelocity(10.0 / 127.0).data[0] == 9);
  CHECK(!m.SetSlot(-1, s) && !m.SetSlot(static_cast<int>(SampleData::SampleManager::kMaxDynamicSlots), s));

  // replacing in place keeps the index; clearing the last slots trims the trailing holes
  s.data = {7, 7};
  CHECK(m.SetSlot(9, s) && m.GetSlot(9)->data.size() == 2);
  CHECK(m.ClearSlot(9) && m.DynamicCount() == 3);
  CHECK(m.GetSlot(3) == nullptr && !m.ClearSlot(50));
}

TEST(SampleManager_SlotKeepsSourceAndEmbedFlag)
{
  SampleData::SampleManager m;
  SampleData::Slot s;
  s.data = {1, 2};
  s.source = std::make_shared<SampleData::SourceAudio>();
  s.embedSource = true;
  CHECK(m.AddSlot(s) == 0);
  auto got = m.GetSlot(0);
  CHECK(got->embedSource && got->source == s.source);
  CHECK(m.CopyDynamicSlots()[0].source == s.source); // shared, not copied
}

TEST(SampleManager_BankKeptWhileInUse)
{
  SampleData::SampleManager m;
  const int8_t a[] = {42, 43};
  m.AddSample(a, 2);

  // "audio thread": a voice binds the sample at velocity 1 and keeps the raw pointer
  SampleData::SampleInfo playing = m.GetSampleForVelocity(1.0 / 127.0);
  CHECK(playing.bankGeneration == m.CurrentGeneration());
  m.ReportOldestGenerationInUse(playing.bankGeneration);

  // "UI thread": samples are replaced while the voice still plays
  m.ClearDynamicSamples();
  m.ReplaceDynamicSamples({});
  CHECK(m.RetiredBankCount() >= 1);         // the bank the voice uses is not freed...
  CHECK(playing.data[0] == 42 && playing.data[1] == 43); // ...so its data is still valid

  // the voice stops: audio thread reports only the current bank in use -> old banks are freed
  m.ReportOldestGenerationInUse(m.CurrentGeneration());
  m.CollectGarbage();
  CHECK(m.RetiredBankCount() == 0);
  CHECK(m.GetSampleForVelocity(1.0 / 127.0).data == SampleData::kSilence);
}

TEST(SampleManager_ConcurrentEditsAndReads)
{
  SampleData::SampleManager m;
  std::atomic<bool> stop{false};
  std::atomic<long> reads{0};
  std::thread audio([&] {
    while (!stop.load())
    {
      auto info = m.GetSampleForVelocity(1.0 / 127.0);
      volatile int8_t v = info.data ? info.data[0] : 0;
      (void)v;
      m.ReportOldestGenerationInUse(info.bankGeneration ? info.bankGeneration : m.CurrentGeneration());
      ++reads;
    }
  });
  const int8_t a[] = {7};
  for (int i = 0; i < 2000; ++i)
  {
    m.ClearDynamicSamples();
    m.AddSample(a, 1);
  }
  stop = true;
  audio.join();
  CHECK(reads.load() > 0);
  CHECK(m.DynamicCount() == 1);
}

// ---------------------------------------------------------------- StateFormat (with an IByteChunk-like fake)
struct FakeStr
{
  std::string s;
  const char* Get() const { return s.c_str(); }
};

struct FakeChunk
{
  std::vector<uint8_t> bytes;
  int PutBytes(const void* p, int n)
  {
    auto* b = static_cast<const uint8_t*>(p);
    bytes.insert(bytes.end(), b, b + n);
    return Size();
  }
  template <class T>
  int Put(const T* v) { return PutBytes(v, sizeof(T)); }
  int PutStr(const char* str)
  {
    int len = static_cast<int>(std::strlen(str));
    Put(&len);
    return PutBytes(str, len);
  }
  int GetBytes(void* dst, int n, int pos) const
  {
    if (pos < 0 || n < 0 || pos + n > Size())
      return -1;
    std::memcpy(dst, bytes.data() + pos, n);
    return pos + n;
  }
  template <class T>
  int Get(T* v, int pos) const { return GetBytes(v, sizeof(T), pos); }
  int GetStr(FakeStr& str, int pos) const
  {
    int len = 0;
    pos = Get(&len, pos);
    if (pos < 0 || len < 0 || pos + len > Size())
      return -1;
    str.s.assign(reinterpret_cast<const char*>(bytes.data() + pos), len);
    return pos + len;
  }
  int Size() const { return static_cast<int>(bytes.size()); }
};

static SampleData::Slot MakeSlot(std::vector<int8_t> d, uint32_t sr, std::string path, std::string pipe)
{
  SampleData::Slot s;
  s.data = std::move(d);
  s.sampleRate = sr;
  s.path = std::move(path);
  s.pipeline = std::move(pipe);
  return s;
}

TEST(State_V2RoundTrip)
{
  FakeChunk c;
  int marker = 1234;
  c.Put(&marker); // stands in for the serialized params
  OneBitState::Write(c, {MakeSlot({1, 2, 3}, 48000, "a.wav", "P1"), MakeSlot({4}, 44100, "b.wav", "")});
  int trailer = 99;
  c.Put(&trailer);

  OneBitState::Data d;
  int pos = OneBitState::Read<FakeStr>(c, 4, d);
  CHECK(d.valid && d.version == OneBitState::kLatestVersion && d.dspVersion == OneBitState::kLatestDspVersion && d.compatFlags == 0);
  CHECK(d.slots.size() == 2);
  CHECK(d.slots[0].data == std::vector<int8_t>({1, 2, 3}) && d.slots[0].sampleRate == 48000 && d.slots[0].path == "a.wav" && d.slots[0].pipeline == "P1");
  CHECK(d.slots[1].path == "b.wav");
  int readTrailer = 0;
  CHECK(c.Get(&readTrailer, pos) > 0 && readTrailer == 99);
}

TEST(State_V3CarriesCompatFlags)
{
  FakeChunk c;
  OneBitState::Write(c, {MakeSlot({5}, 44100, "a.wav", "")}, 1, OneBitState::kCompatLegacyTiming);
  OneBitState::Data d;
  OneBitState::Read<FakeStr>(c, 0, d);
  CHECK(d.valid && d.dspVersion == 1 && (d.compatFlags & OneBitState::kCompatLegacyTiming) && d.slots.size() == 1);

  // a v2 block (written by clean-up builds before the sound changes) has no flags and counts as dspVersion 1
  FakeChunk v2;
  uint32_t version = 2, dsp = 1, n = 1, sr = 48000, size = 1;
  v2.Put(&version);
  v2.Put(&dsp);
  v2.Put(&n);
  v2.Put(&sr);
  v2.Put(&size);
  int8_t byte = 3;
  v2.PutBytes(&byte, 1);
  v2.PutStr("p.wav");
  v2.PutStr("");
  OneBitState::Read<FakeStr>(v2, 0, d);
  CHECK(d.valid && d.version == 2 && d.dspVersion == 1 && d.compatFlags == 0 && d.slots.size() == 1 && d.slots[0].path == "p.wav");
}

TEST(State_V4KeepsHolesAndEmbeddedSource)
{
  SampleData::Slot a = MakeSlot({1, 2}, 44100, "a.wav", "normalize->deltasigma");
  SampleData::Slot hole; // free slot between two samples: the second keeps its velocity
  SampleData::Slot b = MakeSlot({3}, 48000, "b.wav", "");
  b.embedSource = true;
  auto bSource = std::make_shared<SampleData::SourceAudio>();
  bSource->sampleRate = 22050;
  bSource->pcm = {0, 65536 * 100, -65536 * 100, 2147418112, -2147483647 - 1};
  b.source = bSource;
  SampleData::Slot c = MakeSlot({4}, 44100, "c.wav", "");
  auto cSource = std::make_shared<SampleData::SourceAudio>(); // in memory but not embedded: only the path is saved
  cSource->pcm = {1, 2, 3};
  c.source = cSource;

  FakeChunk ch;
  OneBitState::Write(ch, {a, hole, b, c});
  OneBitState::Data d;
  CHECK(OneBitState::Read<FakeStr>(ch, 0, d) == ch.Size());
  CHECK(d.valid && d.version == 4 && d.slots.size() == 4);
  CHECK(d.slots[1].Empty() && d.slots[2].path == "b.wav"); // the hole is kept
  CHECK(d.slots[0].source == nullptr && !d.slots[0].embedSource);
  CHECK(d.slots[2].embedSource && d.slots[2].source && d.slots[2].source->sampleRate == 22050);
  const auto& pcm = d.slots[2].source->pcm;
  CHECK(pcm.size() == 5 && pcm[0] == 0 && pcm[1] == 65536 * 100 && pcm[2] == -65536 * 100 && pcm[3] == 32767 * 65536 && pcm[4] == -32768 * 65536);
  CHECK(!d.slots[3].embedSource && d.slots[3].source == nullptr);
  CHECK(d.slots[0].pipeline == "normalize->deltasigma");

  // a truncated embedded source is rejected as a whole
  ch.bytes.resize(ch.bytes.size() - 20);
  CHECK(OneBitState::Read<FakeStr>(ch, 0, d) == 0 && !d.valid);
}

TEST(State_ReadsV1)
{
  // exactly what the original SerializeState wrote
  FakeChunk c;
  uint32_t version = 1, numSlots = 2;
  c.Put(&version);
  c.Put(&numSlots);
  for (auto& [sr, bytes] : std::vector<std::pair<uint32_t, std::vector<int8_t>>>{{44100, {9, 8}}, {48000, {7}}})
  {
    uint32_t size = static_cast<uint32_t>(bytes.size());
    c.Put(&sr);
    c.Put(&size);
    c.PutBytes(bytes.data(), static_cast<int>(size));
  }
  uint32_t pathCount = 2;
  c.Put(&pathCount);
  c.PutStr("x.wav");
  c.PutStr("PX");
  c.PutStr("y.wav");
  c.PutStr("");

  OneBitState::Data d;
  int pos = OneBitState::Read<FakeStr>(c, 0, d);
  CHECK(pos == c.Size());
  CHECK(d.valid && d.version == 1 && d.dspVersion == OneBitState::kDspVersionLegacy);
  CHECK(d.slots.size() == 2 && d.slots[0].data == std::vector<int8_t>({9, 8}) && d.slots[0].path == "x.wav" && d.slots[0].pipeline == "PX");
  CHECK(d.slots[1].sampleRate == 48000 && d.slots[1].path == "y.wav");
}

TEST(State_V0AndCorrupt)
{
  FakeChunk empty;
  OneBitState::Data d;
  CHECK(OneBitState::Read<FakeStr>(empty, 0, d) == 0);
  CHECK(d.version == 0 && d.valid && d.slots.empty() && d.dspVersion == OneBitState::kDspVersionLegacy);

  // truncated v2 block
  FakeChunk c;
  OneBitState::Write(c, {MakeSlot({1, 2, 3, 4}, 44100, "a.wav", "P")});
  c.bytes.resize(c.bytes.size() - 3);
  CHECK(OneBitState::Read<FakeStr>(c, 0, d) == 0);
  CHECK(!d.valid && d.slots.empty());

  // absurd size must be rejected without allocating
  FakeChunk big;
  uint32_t v = 2, dsp = 1, n = 1, sr = 44100, size = 0x7FFFFFFF;
  big.Put(&v);
  big.Put(&dsp);
  big.Put(&n);
  big.Put(&sr);
  big.Put(&size);
  CHECK(OneBitState::Read<FakeStr>(big, 0, d) == 0 && !d.valid);

  // unknown future version
  FakeChunk future;
  uint32_t v9 = 9;
  future.Put(&v9);
  CHECK(OneBitState::Read<FakeStr>(future, 0, d) == 0 && !d.valid);
}

// ---------------------------------------------------------------- SampleImportWorker
TEST(ImportWorker_RunsJobsInOrderOffThread)
{
  const auto mainId = std::this_thread::get_id();
  std::atomic<bool> ranOffThread{true};
  SampleImportWorker<int, int> worker([&](const int& job) {
    if (std::this_thread::get_id() == mainId)
      ranOffThread = false;
    return job * 10;
  });
  for (int i = 1; i <= 5; ++i)
    worker.Enqueue(i);

  std::vector<int> results;
  for (int tries = 0; tries < 500 && results.size() < 5; ++tries)
  {
    for (int r : worker.TakeResults())
      results.push_back(r);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  CHECK(results == std::vector<int>({10, 20, 30, 40, 50}));
  CHECK(ranOffThread.load());
  CHECK(!worker.Busy());
}

TEST(ImportWorker_ShutdownWithPendingJobs)
{
  SampleImportWorker<int, int> worker([](const int& j) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    return j;
  });
  for (int i = 0; i < 20; ++i)
    worker.Enqueue(i);
  worker.Shutdown(); // must not hang or crash
  CHECK(true);
}

TEST(PipelinePresets_Build)
{
  CHECK(PipelinePresets::DropBox().ToString() == "NormalizeEffect->SaturateEffect(10)->DitherEffect(0.1)->TrellisQuantizer(maxRam=67108864)");
  CHECK(PipelinePresets::Kick2().ToString().find("DeltaSigmaQuantizer") != std::string::npos);
}

// ---------------------------------------------------------------- PipelineModel
using namespace PipelineModel;

TEST(Chain_TextRoundTrip)
{
  Chain c = DefaultChain();
  CHECK(ToText(c) == "normalize->saturate(10)->dither(0.1)->trellis(64)");
  Chain back;
  CHECK(ParseChain(ToText(c), back) && back == c);

  // every stage type survives a round trip with non-default values
  Chain all;
  for (const auto& spec : Registry())
  {
    Stage st = DefaultStage(spec.id);
    for (size_t i = 0; i < st.params.size(); ++i)
      st.params[i] = ClampParam(spec.params[i], spec.params[i].min + 0.37 * (spec.params[i].max - spec.params[i].min));
    (spec.isQuantizer ? all.quantizer : all.effects.emplace_back()) = st;
    if (spec.isQuantizer)
    {
      Chain one;
      one.effects = all.effects;
      one.quantizer = st;
      Chain parsed;
      std::string err;
      CHECK(ParseChain(ToText(one), parsed, &err) && parsed == one);
    }
  }
}

TEST(Chain_ParsesOldAudioPipelineText)
{
  // what AudioPipeline::ToString() wrote into existing projects
  Chain c;
  CHECK(ParseChain("NormalizeEffect->SaturateEffect(10)->DitherEffect(0.1)->TrellisQuantizer(maxRam=67108864)", c));
  CHECK(c == DefaultChain());

  // every factory recipe, built from the old pipeline text, equals the same recipe from its new text
  struct { const char* name; AudioPipeline p; } recipes[] = {
    {"Default", PipelinePresets::DropBox()}, {"Snare 1", PipelinePresets::Snare1()}, {"Snare 2 (nearly silent)", PipelinePresets::Snare2()},
    {"Snare 3", PipelinePresets::Snare3()}, {"Snare 4", PipelinePresets::Snare4()}, {"Snare 5", PipelinePresets::Snare5()},
    {"Snare 6", PipelinePresets::Snare6()}, {"Vox 1", PipelinePresets::Vox1()}, {"Vox 2", PipelinePresets::Vox2()},
    {"Kick 1", PipelinePresets::Kick1()}, {"Kick 2", PipelinePresets::Kick2()}};
  for (auto& r : recipes)
  {
    Chain fromOld, fromFactory;
    CHECK(ParseChain(r.p.ToString(), fromOld));
    const NamedChain* f = nullptr;
    for (const auto& fc : FactoryChains())
      if (fc.name == r.name)
        f = &fc;
    CHECK(f != nullptr);
    if (!f)
      continue;
    CHECK(ParseChain(f->text, fromFactory));
    CHECK(Build(fromFactory).ToString() == r.p.ToString()); // builds exactly the old pipeline
    CHECK(Build(fromOld).ToString() == r.p.ToString());     // and the old text reproduces it too
  }
}

TEST(Chain_RejectsBadText)
{
  Chain c = DefaultChain();
  std::string err;
  CHECK(!ParseChain("", c, &err));
  CHECK(!ParseChain("normalize", c, &err) && err.find("quantizer") != std::string::npos);
  CHECK(!ParseChain("bogus->trellis(64)", c, &err) && err.find("unknown") != std::string::npos);
  CHECK(!ParseChain("trellis(64)->normalize->deltasigma", c, &err));
  CHECK(!ParseChain("gain(1,2)->deltasigma", c, &err)); // too many parameters
  CHECK(!ParseChain("gain(x)->deltasigma", c, &err));
  CHECK(c == DefaultChain()); // unchanged on failure

  // out of range values are clamped, missing ones take the default
  CHECK(ParseChain("gain(99)->saturate->deltasigma", c));
  CHECK(c.effects[0].params[0] == 24.0 && c.effects[1].params[0] == 2.0);
}

TEST(Chain_FadeAndDcShiftEffects)
{
  std::vector<double> b(1000, 1.0);
  FadeEffect(10.0, 20.0).Process(b, 10000); // 100 samples in, 200 out
  CHECK(b[0] == 0.0 && b[50] > 0.4 && b[50] < 0.6 && b[300] == 1.0 && b[999] == 0.0 && b[899] > 0.4 && b[899] < 0.6);

  std::vector<double> d = {0.5, -0.5, 0.9};
  DcOffsetEffect(0.3).Process(d, 44100);
  CHECK_NEAR(d[0], 0.8, 1e-12);
  CHECK_NEAR(d[1], -0.2, 1e-12);
  CHECK(d[2] == 1.0); // clipped, not > 1
}

TEST(Chain_QuantizersTrackDc)
{
  std::vector<double> in(6000, 0.4);
  for (const char* id : {"deltasigma", "trellis1", "trellis3"})
  {
    Stage q = DefaultStage(id);
    auto bits = Build(Chain{{}, q}).quantizer->Quantize(in, 44100);
    CHECK(bits.size() == in.size());
    CHECK_NEAR(MeanBipolar(bits, 200), 0.4, 0.05);
  }
}

// ---------------------------------------------------------------- SampleRender
static SampleData::SourceAudio MakeSource(double dc, double amp = 0.3, size_t n = 8000, uint32_t rate = 44100)
{
  SampleData::SourceAudio s;
  s.sampleRate = rate;
  for (size_t i = 0; i < n; ++i)
    s.pcm.push_back(static_cast<int32_t>(2147483647.0 * std::clamp(dc + amp * std::sin(i * 0.05), -1.0, 1.0)));
  return s;
}

TEST(Render_MatchesDirectPipeline)
{
  auto src = MakeSource(0.0);
  auto chain = DefaultChain();
  auto r = SampleRender::Render(src, chain, 44100, true);
  CHECK(r.ok() && r.sampleRate == 44100 && r.bits.size() == 1000 && r.preQuantizer.size() == 8000);
  auto direct = SampleTools::ReduceToOneBit(src.pcm, 44100, Build(chain));
  CHECK(direct == r.bits); // same bits as the old direct path
  CHECK(SampleRender::Render(src, chain, 48000).bits.size() == 8000 * 48000 / 44100 / 8 + (((8000 * 48000 / 44100) % 8) ? 1 : 0));
  CHECK(!SampleRender::Render(SampleData::SourceAudio{}, chain, 44100).ok());
}

TEST(Render_AutoDcCentresTheResult)
{
  auto src = MakeSource(0.35, 0.3); // a lot of DC
  Chain chain;
  CHECK(ParseChain("normalize->dcshift(0)->deltasigma", chain));
  // normalize pushes the peak to full scale, so the DC offset of the 1-bit result is large
  const double before = SampleRender::BitsDc(SampleRender::Render(src, chain, 44100).bits);
  const double offset = SampleRender::FindDcOffset(src, chain, 1, 44100);
  chain.effects[1].params = {offset};
  const double after = SampleRender::BitsDc(SampleRender::Render(src, chain, 44100).bits);
  CHECK(std::fabs(before) > 0.3);
  CHECK(std::fabs(after) < 0.02);
  CHECK(offset < 0.0 && offset >= -1.0);

  // not a dcshift stage / nothing to do: 0
  CHECK(SampleRender::FindDcOffset(src, chain, 0, 44100) == 0.0);
  CHECK(SampleRender::FindDcOffset(src, chain, 9, 44100) == 0.0);
  // an already balanced signal stays about where it is
  auto balanced = MakeSource(0.0, 0.5);
  CHECK(std::fabs(SampleRender::FindDcOffset(balanced, chain, 1, 44100)) < 0.05);
}

TEST(Render_BitsDc)
{
  CHECK(SampleRender::BitsDc({}) == 0.0);
  CHECK(SampleRender::BitsDc({static_cast<int8_t>(0xFF)}) == 1.0);
  CHECK(SampleRender::BitsDc({0x00}) == -1.0);
  CHECK(SampleRender::BitsDc({static_cast<int8_t>(0xF0), 0x0F}) == 0.0);
}

// ---------------------------------------------------------------- Spectrogram
TEST(Spectrogram_FindsTheTone)
{
  const uint32_t sr = 48000;
  std::vector<float> x(sr);
  for (size_t i = 0; i < x.size(); ++i)
    x[i] = static_cast<float>(std::sin(2.0 * 3.14159265358979 * 3000.0 * i / sr));
  auto sg = Spectrogram::Compute(x, sr, 50, 96);
  CHECK(!sg.empty() && sg.cols == 50 && sg.rows == 96);
  // loudest row in the middle column corresponds to 3 kHz on a log axis from 30 Hz to Nyquist (row 0 = top)
  int best = 0;
  for (int r = 0; r < sg.rows; ++r)
    if (sg.At(25, r) > sg.At(25, best))
      best = r;
  const double fAtBest = 30.0 * std::pow(24000.0 / 30.0, static_cast<double>(sg.rows - 1 - best + 0.5) / sg.rows);
  CHECK(fAtBest > 2400 && fAtBest < 3700);
  CHECK(sg.At(25, best) == 255);
  CHECK(Spectrogram::Compute({1.f, 2.f}, sr).empty()); // too short
}

TEST(Spectrogram_FftMatchesDft)
{
  std::vector<std::complex<double>> a(16);
  for (int i = 0; i < 16; ++i)
    a[i] = std::sin(i * 0.7) + 0.2 * i;
  auto fft = a;
  Spectrogram::Fft(fft);
  for (int k = 0; k < 16; ++k)
  {
    std::complex<double> dft = 0;
    for (int n = 0; n < 16; ++n)
      dft += a[n] * std::polar(1.0, -2.0 * 3.14159265358979323846 * k * n / 16);
    CHECK_NEAR(std::abs(fft[k] - dft), 0.0, 1e-9);
  }
}

TEST(Spectrogram_DecodeAndEnvelope)
{
  auto bits = Spectrogram::DecodeBits({static_cast<int8_t>(0xA0)}); // 1010 0000
  CHECK(bits.size() == 8 && bits[0] == 1.f && bits[1] == -1.f && bits[2] == 1.f && bits[3] == -1.f);
  auto env = Spectrogram::Envelope({0.f, 1.f, -1.f, 0.5f}, 2);
  CHECK(env.size() == 2 && env[0].hi == 1.f && env[1].lo == -1.f);
  CHECK(Spectrogram::Envelope({}, 3).size() == 3);
}

// ---------------------------------------------------------------- ChainPresets
TEST(Presets_FactorySaveLoadDelete)
{
  namespace fs = std::filesystem;
  const fs::path base = fs::temp_directory_path() / "obp_preset_test";
  fs::remove_all(base);
  ChainPresets::Store store(base / "user", {base / "shared"});

  auto list = store.List();
  CHECK(list.size() == FactoryChains().size() && list[0].factory && list[0].name == "Default");

  Chain c;
  CHECK(ParseChain("normalize->fade(5,30)->dcshift(-0.12)->trellis3(65)", c));
  std::string err;
  CHECK(store.Save("My / Kick: 1", c, &err));
  CHECK(!store.Save("", c, &err) && !err.empty());

  // a shared folder preset, written by hand (with Windows line ends)
  fs::create_directories(base / "shared");
  std::ofstream(base / "shared" / "pad.obpchain", std::ios::binary) << "# x\r\nname=Pad\r\nchain=normalize->deltasigma\r\n";
  std::ofstream(base / "shared" / "broken.obpchain") << "name=Broken\nchain=nonsense\n";

  list = store.List();
  CHECK(list.size() == FactoryChains().size() + 2); // the broken file is ignored
  const auto& pad = list[FactoryChains().size()];   // sorted by name: "My / Kick: 1" < "Pad"
  CHECK(pad.name == "My / Kick: 1" && pad.user && !pad.factory && pad.chain == c);
  CHECK(list.back().name == "Pad" && !list.back().user);

  CHECK(store.Delete("My / Kick: 1"));
  CHECK(!store.Delete("My / Kick: 1") && !store.Delete("Pad")); // shared presets are never deleted
  CHECK(store.List().size() == FactoryChains().size() + 1);
  CHECK(ChainPresets::FileNameFor("a/b:c ") == "a_b_c.obpchain");
  fs::remove_all(base);
}

// ---------------------------------------------------------------- SamplesModel
static std::shared_ptr<const SampleData::Slot> ModelSlot(const std::string& pipeline, bool withSource)
{
  auto slot = std::make_shared<SampleData::Slot>();
  slot->data = {1, 2, 3};
  slot->path = "kick.wav";
  slot->pipeline = pipeline;
  if (withSource)
    slot->source = std::make_shared<SampleData::SourceAudio>();
  return slot;
}

TEST(SamplesModel_SelectLoadsTheChain)
{
  SamplesModel m;
  m.Select(4, ModelSlot("normalize->gain(3)->deltasigma", true), true);
  CHECK(m.Selected() == 4 && m.HasSample() && m.Editable() && m.Status() == SamplesModel::SourceStatus::InMemory);
  CHECK(m.NumEffects() == 2 && m.StageAt(1).id == "gain" && m.StageAt(2).id == "deltasigma"); // index 2 = the quantizer
  CHECK(!m.ChainUnknown());

  // old project: the chain text is the old format
  m.Select(0, ModelSlot("NormalizeEffect->SaturateEffect(10)->DitherEffect(0.1)->TrellisQuantizer(maxRam=67108864)", false), true);
  CHECK(m.Chain() == PipelineModel::DefaultChain() && !m.ChainUnknown() && m.Status() == SamplesModel::SourceStatus::File);

  // a sample whose chain was never recorded (or is garbage): editing starts from the default chain
  m.Select(0, ModelSlot("<no pipeline>", false), true);
  CHECK(m.ChainUnknown() && m.Chain() == PipelineModel::DefaultChain());

  // source file gone and nothing in memory: can't be edited
  m.Select(0, ModelSlot("normalize->deltasigma", false), false);
  CHECK(m.Status() == SamplesModel::SourceStatus::Missing && !m.Editable() && !m.AddEffect("gain", 0) && !m.NeedsRender(10000));

  // free slot
  m.Select(7, nullptr, false);
  CHECK(!m.HasSample() && m.Status() == SamplesModel::SourceStatus::None && !m.Editable() && m.Selected() == 7);
}

TEST(SamplesModel_EditsAndDebounce)
{
  SamplesModel m;
  m.Select(0, ModelSlot("normalize->gain(3)->deltasigma", true), true);
  CHECK(!m.NeedsRender(1000));

  CHECK(m.SetParam(1, 0, 6.0, 1000)); // gain 3 -> 6 dB
  CHECK(m.StageAt(1).params[0] == 6.0);
  CHECK(!m.SetParam(1, 0, 6.0, 1001)); // no change
  CHECK(m.SetParam(1, 0, 99.0, 1002) && m.StageAt(1).params[0] == 24.0); // clamped
  CHECK(!m.SetParam(1, 5, 1.0, 1003) && !m.SetParam(9, 0, 1.0, 1003));

  // keeps being edited -> no render until it has been quiet for the debounce time
  CHECK(!m.NeedsRender(1002 + SamplesModel::kDebounceMs - 1));
  CHECK(m.NeedsRender(1002 + SamplesModel::kDebounceMs));

  m.RenderStarted();
  CHECK(!m.NeedsRender(99999) && m.Busy());
  CHECK(m.SetParam(1, 0, 2.0, 5000)); // edited during the render
  CHECK(!m.NeedsRender(5000 + SamplesModel::kDebounceMs)); // still running
  m.RenderFinished();
  CHECK(m.NeedsRender(5000 + SamplesModel::kDebounceMs)); // the edit made while it ran triggers the next one
  m.RenderStarted();
  m.RenderFinished("done");
  CHECK(!m.Busy() && m.Message() == "done");
}

TEST(SamplesModel_StructureEdits)
{
  SamplesModel m;
  m.Select(0, ModelSlot("normalize->deltasigma", true), true);
  CHECK(m.AddEffect("fade", 1) && m.AddEffect("dither", 2));
  CHECK(m.NumEffects() == 3 && m.StageAt(1).id == "fade" && m.StageAt(2).id == "dither");
  CHECK(!m.AddEffect("trellis", 3) && !m.AddEffect("nonsense", 3)); // a quantizer isn't an effect
  while (m.NumEffects() < SamplesModel::kMaxEffects)
    CHECK(m.AddEffect("gain", 3));
  CHECK(!m.AddEffect("gain", 3)); // the page has room for kMaxEffects
  while (m.NumEffects() > 3)
    CHECK(m.RemoveEffect(m.NumEffects() - 1, 3));

  CHECK(m.MoveEffect(2, -1, 4));
  CHECK(m.StageAt(1).id == "dither" && m.StageAt(2).id == "fade");
  CHECK(!m.MoveEffect(0, -1, 5) && !m.MoveEffect(2, 1, 5) && !m.MoveEffect(1, 0, 5));

  CHECK(m.RemoveEffect(0, 6) && m.NumEffects() == 2 && m.StageAt(0).id == "dither");
  CHECK(!m.RemoveEffect(7, 7));

  CHECK(m.SetQuantizer("trellis3", 8) && m.StageAt(2).id == "trellis3" && m.StageAt(2).params == std::vector<double>{129});
  CHECK(!m.SetQuantizer("trellis3", 9) && !m.SetQuantizer("gain", 9));

  PipelineModel::Chain preset;
  CHECK(PipelineModel::ParseChain("normalize->saturate(3)->raw", preset));
  CHECK(m.LoadChain(preset, 10) && m.Chain() == preset && !m.LoadChain(preset, 11));
}

TEST(SamplesModel_AutoDcAddsTheStage)
{
  SamplesModel m;
  m.Select(0, ModelSlot("normalize->deltasigma", true), true);
  CHECK(m.FindEffect("dcshift") == -1);
  int index = m.RequestAutoDc(100);
  CHECK(index == 1 && m.StageAt(1).id == "dcshift" && m.NumEffects() == 2);
  CHECK(m.TakePendingAutoDc() == 1 && m.TakePendingAutoDc() == -1);
  CHECK(m.RequestAutoDc(200) == 1 && m.NumEffects() == 2); // reuses the stage that is there
  CHECK(m.SetParam(1, 0, -0.25, 300) && m.StageAt(1).params[0] == -0.25);
}

TEST(SamplesModel_EmbedFlagFollowsTheSlot)
{
  auto slot = std::make_shared<SampleData::Slot>(*ModelSlot("normalize->deltasigma", true));
  slot->embedSource = true;
  SamplesModel m;
  m.Select(0, slot, true);
  CHECK(m.Embed());
  m.SetEmbed(false);
  CHECK(!m.Embed() && !m.NeedsRender(99999)); // not an edit of the sound
}

// ---------------------------------------------------------------- BitCompression
TEST(BitCompression_ConstantStreamCompresses)
{
  std::vector<bool> bits(4096, false);
  double rate = BitStreamAnalyzer::evaluateMetrics(bits, 16, 4, false);
  CHECK(rate < 25.0);
}

TEST(BitCompression_ShortInputDoesNotUnderflow)
{
  std::vector<bool> bits(5, true);
  CHECK(BitStreamAnalyzer::evaluateMetrics(bits, 16, 4, false) == 100.0);
  CHECK(BitStreamAnalyzer::evaluateMetricsEliasGamma(bits, 16, 4, false) == 100.0);
  CHECK(BitStreamAnalyzer::evaluateMetrics({}, 8, 4, false) == 100.0);
}

TEST(BitCompression_IndexBitsScaleWithK)
{
  // 64 distinct 8-bit chunks, each once: every chunk is a dictionary hit with K = 64,
  // so payload = 64 * (1 + 6) bits and header = 64 * 8 bits.
  std::vector<bool> bits;
  for (int v = 0; v < 64; ++v)
    for (int b = 7; b >= 0; --b)
      bits.push_back((v >> b) & 1);
  const double rate = BitStreamAnalyzer::evaluateMetrics(bits, 8, 64, false);
  CHECK_NEAR(rate, 100.0 * (64 * 8 + 64 * 7) / 512.0, 1e-9);
}

// ---------------------------------------------------------------- C++ array export
TEST(ToCppArray_IdentifierAndFormat)
{
  CHECK(SampleTools::MakeCppIdentifier("C:\\x\\808 Snare-1.wav") == "s_808_Snare_1");
  CHECK(SampleTools::MakeCppIdentifier("/a/b/kick.WAV") == "kick");
  CHECK(SampleTools::MakeCppIdentifier("") == "kSample");
  const std::string code = SampleTools::ToCppArray("kick.wav", {0x01, static_cast<int8_t>(0xFF)});
  CHECK(code == "inline constexpr int8_t kick[] = {\n  0x01, int8_t(0xFF)\n};");
  // long arrays are not truncated (MY_PRINTF used to cut at 512 bytes)
  CHECK(SampleTools::ToCppArray("x", std::vector<int8_t>(1000, 0)).size() > 6000);
}

int main()
{
  for (auto& t : TestRegistry())
  {
    int before = gFailures;
    t.fn();
    std::printf("%s %s\n", gFailures == before ? "[ OK ]" : "[FAIL]", t.name);
  }
  std::printf("\n%d checks, %d failures\n", gChecks, gFailures);
  return gFailures == 0 ? 0 : 1;
}
