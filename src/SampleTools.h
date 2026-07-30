#pragma once
#include <cmath>
#include <cstdint>
#include <vector>

// Resampling algorithm selector
enum class ResampleAlgo
{
  Nearest,
  Linear // Left available for future expansion if you want smoother transitions
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

// Step 2: Reduce to 1-Bit using a 1st-Order Delta-Sigma Noise Shaper
inline static std::vector<int8_t> ReduceToOneBit(const std::vector<int32_t>& inputBuffer,
                                            uint32_t sampleRate = 44100,
                                            bool normalize = true,
                                            bool applyLPF = true,
                                            double cutoffFreq = 16000.0, // Default cutoff to tame high-end hash
                                            bool saturate = true,
                                            double saturationDrive = 4.0){if (inputBuffer.empty()) return {};

// Create a working copy so we don't mutate the raw input buffer
std::vector<int32_t> workingBuffer = inputBuffer;

// Apply preprocessing chain
if (normalize)
{
  Normalize(workingBuffer);
}
if (applyLPF)
{
  LowPassFilter(workingBuffer, cutoffFreq, static_cast<double>(sampleRate));
}
if (saturate)
{
  //Saturate(workingBuffer, saturationDrive);
}

size_t totalBits = workingBuffer.size();
size_t totalBytes = (totalBits + 7) / 8;
std::vector<int8_t> packedBytes(totalBytes, 0);

double error = 0.0;

for (size_t i = 0; i < totalBits; ++i)
{
  double sample = static_cast<double>(workingBuffer[i]) / 2147483648.0;
  double v = sample + error;

  bool bit = (v >= 0.0);
  double quantizedOutput = bit ? 1.0 : -1.0;

  error = v - quantizedOutput;

  if (bit)
  {
    size_t byteIndex = i / 8;
    int bitInByte = 7 - static_cast<int>(i % 8);
    packedBytes[byteIndex] |= (1 << bitInByte);
  }
}

return packedBytes;
}
}; // namespace SampleTools