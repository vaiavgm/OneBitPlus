#pragma once
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
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

// `preQuantizer`, if given, receives the signal after the effects and before the quantizer (for analysis / display).
inline static std::vector<int8_t> ReduceToOneBit(const std::vector<int32_t>& inputBuffer, uint32_t sampleRate, const AudioPipeline& pipeline = AudioPipeline::CreateDefaultPipeline(), std::vector<double>* preQuantizer = nullptr)
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

  if (preQuantizer)
    *preQuantizer = workingBuffer;

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
// Turns a file path into a valid C++ identifier ("C:/x/808 Snare.wav" -> "s_808_Snare").
inline std::string MakeCppIdentifier(const std::string& path)
{
  size_t lastSlash = path.find_last_of("/\\");
  std::string name = (lastSlash == std::string::npos) ? path : path.substr(lastSlash + 1);

  size_t lastDot = name.find_last_of('.');
  if (lastDot != std::string::npos)
    name = name.substr(0, lastDot);

  for (char& c : name)
  {
    if (!std::isalnum(static_cast<unsigned char>(c)))
      c = '_';
  }

  if (name.empty())
    return "kSample";
  if (std::isdigit(static_cast<unsigned char>(name[0])))
    name = "s_" + name;
  return name;
}

// Builds a C++ array definition for SampleData.h from packed 1-bit data.
// Bytes >= 0x80 are written as int8_t(0x..) so the array compiles without narrowing errors on every compiler.
inline std::string ToCppArray(const std::string& path, const std::vector<int8_t>& packed1Bit)
{
  std::string code = "inline constexpr int8_t " + MakeCppIdentifier(path) + "[] = {";
  for (size_t i = 0; i < packed1Bit.size(); ++i)
  {
    if (i % 16 == 0)
      code += "\n  ";
    char hexBuf[24];
    const auto byte = static_cast<uint8_t>(packed1Bit[i]);
    std::snprintf(hexBuf, sizeof(hexBuf), byte >= 0x80 ? "int8_t(0x%02X)" : "0x%02X", byte);
    code += hexBuf;
    if (i + 1 < packed1Bit.size())
      code += ", ";
  }
  code += "\n};";
  return code;
}
}; // namespace SampleTools
