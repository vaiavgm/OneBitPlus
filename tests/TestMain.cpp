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

static std::vector<TestCase>& Registry()
{
  static std::vector<TestCase> r;
  return r;
}

struct Registrar
{
  Registrar(const char* n, std::function<void()> f) { Registry().push_back({n, std::move(f)}); }
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
  CHECK(d.valid && d.version == 2 && d.dspVersion == OneBitState::kLatestDspVersion);
  CHECK(d.slots.size() == 2);
  CHECK(d.slots[0].data == std::vector<int8_t>({1, 2, 3}) && d.slots[0].sampleRate == 48000 && d.slots[0].path == "a.wav" && d.slots[0].pipeline == "P1");
  CHECK(d.slots[1].path == "b.wav");
  int readTrailer = 0;
  CHECK(c.Get(&readTrailer, pos) > 0 && readTrailer == 99);
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
  for (auto& t : Registry())
  {
    int before = gFailures;
    t.fn();
    std::printf("%s %s\n", gFailures == before ? "[ OK ]" : "[FAIL]", t.name);
  }
  std::printf("\n%d checks, %d failures\n", gChecks, gFailures);
  return gFailures == 0 ? 0 : 1;
}
