// Standalone unit tests for the pure (iPlug2-independent) helpers in src/.
// Build:  cmake -S tests -B build-tests && cmake --build build-tests && ./build-tests/OneBitPlusTests
// Or:     g++ -std=c++17 -I. tests/TestMain.cpp -o OneBitPlusTests   (run from the repo root)

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <numeric>
#include <string>
#include <vector>

#include "src/BitCompression.h"
#include "src/IAudioEffect.h"
#include "src/SampleData.h"
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
