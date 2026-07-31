#pragma once
#include <cmath>
#include <cstdint>
#include <vector>

#include "src/IAudioEffect.h"

// Resampling algorithm selector
enum class ResampleAlgo
{
  Nearest,
  Linear,
  Lanczos
};

namespace SampleTools
{
// --- STEP 1: Resample 32-bit buffer to target sample rate ---

// Step 1: Resample with Linear Interpolation support
inline static std::vector<int32_t> Resample(const std::vector<int32_t>& inputBuffer, uint32_t sourceRate, uint32_t targetRate, ResampleAlgo algo = ResampleAlgo::Nearest)
{
  if (inputBuffer.empty() || sourceRate == targetRate || targetRate == 0)
  {
    return inputBuffer;
  }

  double ratio = static_cast<double>(targetRate) / sourceRate;
  size_t newSize = static_cast<size_t>(inputBuffer.size() * ratio);
  std::vector<int32_t> resampled;
  resampled.reserve(newSize);

  if (algo == ResampleAlgo::Nearest)
  {
    for (size_t i = 0; i < newSize; ++i)
    {
      size_t oldIndex = static_cast<size_t>(std::round(i / ratio));
      if (oldIndex < inputBuffer.size())
      {
        resampled.push_back(inputBuffer[oldIndex]);
      }
      else
      {
        resampled.push_back(inputBuffer.back());
      }
    }
  }
  else if (algo == ResampleAlgo::Linear)
  {
    for (size_t i = 0; i < newSize; ++i)
    {
      double srcPos = i / ratio;
      size_t idx1 = static_cast<size_t>(srcPos);
      size_t idx2 = std::min(idx1 + 1, inputBuffer.size() - 1);
      double frac = srcPos - idx1;

      int32_t val1 = inputBuffer[idx1];
      int32_t val2 = inputBuffer[idx2];
      // Linear blend retains smooth envelope contours and avoids harsh stepping
      int32_t interpolated = static_cast<int32_t>(val1 + frac * (val2 - val1));
      resampled.push_back(interpolated);
    }
  }
  else if (algo == ResampleAlgo::Lanczos)
  {
    constexpr int a = 3; // Lanczos-3 (3 lobes) is the professional audio standard

    // Scale filter width during downsampling to prevent aliasing above Nyquist
    const double filterScale = (ratio < 1.0) ? ratio : 1.0;
    const double kernelRadius = static_cast<double>(a) / filterScale;

    // Normalized Sinc helper: sinc(x) = sin(pi * x) / (pi * x)
    auto Sinc = [](double x) -> double {
      if (std::abs(x) < 1e-9)
        return 1.0;
      const double pi = 3.14159265358979323846;
      double pix = pi * x;
      return std::sin(pix) / pix;
    };

    // Lanczos windowed kernel evaluation
    auto LanczosKernel = [&](double x) -> double {
      double absX = std::abs(x);
      if (absX >= a)
        return 0.0;
      return Sinc(absX) * Sinc(absX / static_cast<double>(a));
    };

    resampled.reserve(newSize);

    for (size_t i = 0; i < newSize; ++i)
    {
      double srcPos = static_cast<double>(i) / ratio;

      // Calculate sample window bounds in source buffer space
      int minIdx = static_cast<int>(std::ceil(srcPos - kernelRadius));
      int maxIdx = static_cast<int>(std::floor(srcPos + kernelRadius));

      minIdx = std::clamp(minIdx, 0, static_cast<int>(inputBuffer.size()) - 1);
      maxIdx = std::clamp(maxIdx, 0, static_cast<int>(inputBuffer.size()) - 1);

      double accumulatedSample = 0.0;
      double totalWeight = 0.0;

      for (int j = minIdx; j <= maxIdx; ++j)
      {
        // Distance from current sample center, scaled by anti-aliasing factor
        double dx = (srcPos - static_cast<double>(j)) * filterScale;
        double weight = LanczosKernel(dx);

        accumulatedSample += static_cast<double>(inputBuffer[j]) * weight;
        totalWeight += weight;
      }

      // Normalize by total weight sum to preserve DC gain
      if (totalWeight > 0.0)
      {
        accumulatedSample /= totalWeight;
      }

      // Clamp output to guard against minor ringing overshoots
      double clamped = std::clamp(accumulatedSample, -2147483648.0, 2147483647.0);
      resampled.push_back(static_cast<int32_t>(clamped));
    }
  }

  return resampled;
}

inline static void Normalize(std::vector<int32_t>& buffer)
{
  if (buffer.empty())
    return;

  int32_t peak = 0;
  for (int32_t val : buffer)
  {
    peak = std::max(peak, std::abs(val));
  }
  if (peak == 0 || peak == 2147483647)
    return;

  double scale = 2147483647.0 / static_cast<double>(peak);
  for (int32_t& val : buffer)
  {
    double scaled = static_cast<double>(val) * scale;
    val = static_cast<int32_t>(std::clamp(scaled, -2147483648.0, 2147483647.0));
  }
}

// Helper: Boosts and soft-clips the signal to inject warmth/harmonics before 1-bit crushing
inline static void Saturate(std::vector<int32_t>& buffer, double drive = 2.0)
{
  if (buffer.empty())
    return;

  for (int32_t& val : buffer)
  {
    double sample = (static_cast<double>(val) / 2147483648.0) * drive;
    // Soft clipping via hyperbolic tangent (adds rich odd harmonics)
    double saturated = std::tanh(sample);
    val = static_cast<int32_t>(saturated * 2147483648.0);
  }
}

// Helper: Single-pole Low-Pass Filter to remove high-frequency noise & harshness
inline static void LowPassFilter(std::vector<int32_t>& buffer, double cutoffFreq, double sampleRate)
{
  if (buffer.empty() || cutoffFreq <= 0.0 || sampleRate <= 0.0)
    return;

  const double pi = 3.14159265358979323846;
  double rc = 1.0 / (2.0 * pi * cutoffFreq);
  double dt = 1.0 / sampleRate;
  double alpha = dt / (rc + dt);

  double prevOutput = static_cast<double>(buffer[0]) / 2147483648.0;

  for (int32_t& val : buffer)
  {
    double input = static_cast<double>(val) / 2147483648.0;
    prevOutput = prevOutput + alpha * (input - prevOutput);
    val = static_cast<int32_t>(std::clamp(prevOutput * 2147483648.0, -2147483648.0, 2147483647.0));
  }
}



inline static std::vector<int8_t> ReduceToOneBit(const std::vector<int32_t>& inputBuffer, uint32_t sampleRate, const AudioPipeline& pipeline = CreateDefaultPipeline())
{
  if (inputBuffer.empty() || !pipeline.quantizer)
    return {};

  // 1. Convert PCM int32 to double [-1.0, 1.0] once
  std::vector<double> workingBuffer(inputBuffer.size());
  for (size_t i = 0; i < inputBuffer.size(); ++i)
  {
    workingBuffer[i] = static_cast<double>(inputBuffer[i]) / 2147483648.0;
  }

  // 2. Run 0-N pre-processing effects sequentially
  for (const auto& effect : pipeline.effects)
  {
    if (effect)
    {
      effect->Process(workingBuffer, sampleRate);
    }
  }

  // 3. Execute the single terminal quantizer stage
  std::vector<bool> bits = pipeline.quantizer->Quantize(workingBuffer, sampleRate);

  // 4. Pack output bits to bytes
  size_t totalBits = bits.size();
  size_t totalBytes = (totalBits + 7) / 8;
  std::vector<int8_t> packedBytes(totalBytes, 0);

  for (size_t i = 0; i < totalBits; ++i)
  {
    if (bits[i])
    {
      size_t byteIndex = i / 8;
      int bitInByte = 7 - static_cast<int>(i % 8);
      packedBytes[byteIndex] |= (1 << bitInByte);
    }
  }

  return packedBytes;
}
}; // namespace SampleTools
