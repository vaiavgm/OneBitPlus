#pragma once

// Renders a source recording through a chain into packed 1-bit data (pure functions, no plugin state), plus the
// "find the DC offset that leaves the least DC in the result" search.

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "src/PipelineModel.h"
#include "src/SampleData.h"
#include "src/SampleLoader.h"
#include "src/SampleTools.h"

namespace SampleRender
{

// Reads a WAV file as source audio (mono). nullptr if the file is missing or not a supported WAV.
inline std::shared_ptr<const SampleData::SourceAudio> LoadSourceFile(const std::string& path)
{
  if (path.empty())
    return nullptr;
  SampleLoader loader;
  LoadedSample wav = loader.load_file_static(path.c_str());
  if (wav.sampleBuffer.empty())
    return nullptr;
  auto source = std::make_shared<SampleData::SourceAudio>();
  source->pcm = std::move(wav.sampleBuffer);
  source->sampleRate = wav.header.SamplesPerSec;
  return source;
}

struct Result
{
  std::vector<int8_t> bits;          // packed 1-bit data, MSB first; empty if the render produced nothing
  uint32_t sampleRate = 44100;       // the rate the data was rendered for
  std::vector<double> preQuantizer;  // signal after the effects, before the quantizer (only if requested)
  bool ok() const { return !bits.empty(); }
};

// Resample, run the effects, quantize.
inline Result Render(const SampleData::SourceAudio& source, const PipelineModel::Chain& chain, uint32_t targetRate, bool keepPreQuantizer = false, ResampleAlgo algo = ResampleAlgo::Lanczos)
{
  Result result;
  result.sampleRate = targetRate;
  if (source.pcm.empty() || targetRate == 0)
    return result;
  const auto resampled = SampleTools::Resample(source.pcm, source.sampleRate, targetRate, algo);
  result.bits = SampleTools::ReduceToOneBit(resampled, targetRate, PipelineModel::Build(chain), keepPreQuantizer ? &result.preQuantizer : nullptr);
  return result;
}

// Mean of the bits as a bipolar signal: -1 = all zeros, +1 = all ones, 0 = balanced.
inline double BitsDc(const std::vector<int8_t>& packed)
{
  if (packed.empty())
    return 0.0;
  static const int kPop[16] = {0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4};
  size_t ones = 0;
  for (int8_t b : packed)
  {
    const auto u = static_cast<uint8_t>(b);
    ones += kPop[u >> 4] + kPop[u & 15];
  }
  const double total = static_cast<double>(packed.size()) * 8.0;
  return (2.0 * static_cast<double>(ones) - total) / total;
}

// Finds the value of the DC-shift stage `dcStage` (an index into chain.effects, which must be a "dcshift") that
// leaves the least DC in the final 1-bit result, by bisection (the result's DC grows with the offset).
// Runs the whole chain about a dozen times, so call it from a worker thread.
inline double FindDcOffset(const SampleData::SourceAudio& source, PipelineModel::Chain chain, size_t dcStage, uint32_t targetRate)
{
  if (dcStage >= chain.effects.size() || chain.effects[dcStage].id != "dcshift" || source.pcm.empty())
    return 0.0;

  const auto resampled = SampleTools::Resample(source.pcm, source.sampleRate, targetRate, ResampleAlgo::Lanczos);
  auto dcFor = [&](double offset) {
    chain.effects[dcStage].params = {offset};
    return BitsDc(SampleTools::ReduceToOneBit(resampled, targetRate, PipelineModel::Build(chain)));
  };

  double lo = -1.0, hi = 1.0;
  const double dcLo = dcFor(lo), dcHi = dcFor(hi);
  if (dcLo >= 0.0)
    return lo; // even the lowest offset leaves too much DC: that is the best there is
  if (dcHi <= 0.0)
    return hi;

  double best = 0.0, bestAbs = 2.0;
  for (int i = 0; i < 14; ++i)
  {
    const double mid = 0.5 * (lo + hi);
    const double dc = dcFor(mid);
    if (std::fabs(dc) < bestAbs)
    {
      bestAbs = std::fabs(dc);
      best = mid;
    }
    if (bestAbs < 0.002)
      break;
    (dc < 0.0 ? lo : hi) = mid;
  }
  return std::round(best * 1000.0) / 1000.0;
}

} // namespace SampleRender
