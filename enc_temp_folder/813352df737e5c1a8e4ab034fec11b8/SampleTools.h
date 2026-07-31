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


struct TrellisConfig
{
  int numStates;
  bool useChunking;
  size_t chunkSize;
};

/**
 * Dynamically suggests optimal Trellis parameters based on sample count
 * and a target maximum RAM budget.
 */
inline static TrellisConfig SuggestTrellisConfig(size_t totalSamples,
                                                 size_t maxRamBytes = 128 * 1024 * 1024) // Default 128MB budget
{
  // Estimated sizeof(Node): 8 (double cost) + 8 (double err) + 4 (int prev) + 1 (bool bit) + 3 (padding) = 24 bytes
  constexpr size_t bytesPerNode = 24;

  // Quality tiers (odd state counts ensure exact 0.0 state center)
  constexpr int HIGH_QUALITY_STATES = 257;
  constexpr int MEDIUM_QUALITY_STATES = 129;
  constexpr int LOW_QUALITY_STATES = 65;

  // 1. Check if High Quality single-pass fits in RAM
  if (totalSamples * HIGH_QUALITY_STATES * bytesPerNode <= maxRamBytes)
  {
    return {HIGH_QUALITY_STATES, false, totalSamples};
  }

  // 2. Check if Medium Quality single-pass fits in RAM
  if (totalSamples * MEDIUM_QUALITY_STATES * bytesPerNode <= maxRamBytes)
  {
    return {MEDIUM_QUALITY_STATES, false, totalSamples};
  }

  // 3. Check if Low Quality single-pass fits in RAM
  if (totalSamples * LOW_QUALITY_STATES * bytesPerNode <= maxRamBytes)
  {
    return {LOW_QUALITY_STATES, false, totalSamples};
  }

  // 4. Fallback for large files: use chunking to strictly bound RAM utilization
  return {LOW_QUALITY_STATES, true, 4096};
}

inline static void ApplyTrellisQuantizationSinglePass(const std::vector<double>& input, std::vector<bool>& outputBits,
                                                      int numStates = 129) // Boosted state count for higher precision
{
  if (input.empty())
    return;

  const size_t totalSamples = input.size();
  const double state_min = -2.0;
  const double state_max = 2.0;
  const double infinity = std::numeric_limits<double>::max();

  auto get_state_index = [&](double state) -> int {
    double normalized = (state - state_min) / (state_max - state_min);
    int idx = static_cast<int>(normalized * (numStates - 1));
    return std::max(0, std::min(idx, numStates - 1));
  };

  struct Node
  {
    double cost;
    double exact_error;
    int prev_idx;
    bool bit;
  };

  // Full-sample Trellis Grid: [Time Step][State Index]
  std::vector<std::vector<Node>> trellis(totalSamples + 1, std::vector<Node>(numStates, {infinity, 0.0, -1, false}));

  // Start at initial zero-error state
  int start_idx = get_state_index(0.0);
  trellis[0][start_idx] = {0.0, 0.0, -1, false};

  // --- Forward Pass (Global Viterbi Search) ---
  for (size_t t = 0; t < totalSamples; ++t)
  {
    double x = input[t];

    for (int s = 0; s < numStates; ++s)
    {
      if (trellis[t][s].cost == infinity)
        continue;

      double current_err = trellis[t][s].exact_error;

      // Evaluate 0 (-1.0) and 1 (+1.0)
      for (int b = 0; b < 2; ++b)
      {
        bool bit_val = (b == 1);
        double y = bit_val ? 1.0 : -1.0;

        // 1st order noise shaping state feedback
        double next_err = current_err + (x - y);

        // Cost function: penalize low-frequency integrator energy
        double step_cost = next_err * next_err;
        double total_cost = trellis[t][s].cost + step_cost;

        int next_s = get_state_index(next_err);

        if (total_cost < trellis[t + 1][next_s].cost)
        {
          trellis[t + 1][next_s] = {total_cost, next_err, s, bit_val};
        }
      }
    }
  }

  // --- Traceback Pass ---
  // Find absolute best end-state across the entire file
  int best_state = 0;
  double min_cost = infinity;
  for (int s = 0; s < numStates; ++s)
  {
    if (trellis[totalSamples][s].cost < min_cost)
    {
      min_cost = trellis[totalSamples][s].cost;
      best_state = s;
    }
  }

  // Recover optimal 1-bit sequence from end to beginning
  int curr_s = best_state;
  for (size_t t = totalSamples; t > 0; --t)
  {
    outputBits[t - 1] = trellis[t][curr_s].bit;
    curr_s = trellis[t][curr_s].prev_idx;
  }
}

inline static void ApplyTrellisQuantizationChunked(const std::vector<double>& input, std::vector<bool>& outputBits, int numStates = 65, size_t chunkSize = 4096)
{
  if (input.empty())
    return;

  const size_t totalSamples = input.size();
  const double state_min = -2.0;
  const double state_max = 2.0;
  const double infinity = std::numeric_limits<double>::max();

  // Helper to map double error state into discrete trellis bin indices
  auto get_state_index = [&](double state) -> int {
    double normalized = (state - state_min) / (state_max - state_min);
    int idx = static_cast<int>(normalized * (numStates - 1));
    return std::clamp(idx, 0, numStates - 1);
  };

  struct Node
  {
    double cost;
    double exact_error;
    int prev_idx;
    bool bit;
  };

  // Continuous error accumulator carried over across chunk boundaries
  double initial_error = 0.0;

  for (size_t offset = 0; offset < totalSamples; offset += chunkSize)
  {
    const size_t current_chunk = std::min(chunkSize, totalSamples - offset);

    // Grid size for current block: [Time step: 0..current_chunk][State Index: 0..numStates-1]
    std::vector<std::vector<Node>> trellis(current_chunk + 1, std::vector<Node>(numStates, {infinity, 0.0, -1, false}));

    // Initialize time t = 0 for this chunk using error state from previous chunk
    int start_idx = get_state_index(initial_error);
    trellis[0][start_idx] = {0.0, initial_error, -1, false};

    // --- Forward Pass (Viterbi) ---
    for (size_t t = 0; t < current_chunk; ++t)
    {
      double x = input[offset + t];

      for (int s = 0; s < numStates; ++s)
      {
        if (trellis[t][s].cost == infinity)
          continue;

        double current_err = trellis[t][s].exact_error;

        // Evaluate output bit 0 (-1.0) and bit 1 (+1.0)
        for (int b = 0; b < 2; ++b)
        {
          bool bit_val = (b == 1);
          double y = bit_val ? 1.0 : -1.0;

          // 1st order Delta-Sigma integrator state ($e_{t+1} = e_t + x_t - y_t$)
          double next_err = current_err + (x - y);

          // Penalize integrated error magnitude (shapes noise to high frequencies)
          double step_cost = next_err * next_err;
          double total_cost = trellis[t][s].cost + step_cost;

          int next_s = get_state_index(next_err);

          // Keep path with the lowest cumulative cost
          if (total_cost < trellis[t + 1][next_s].cost)
          {
            trellis[t + 1][next_s] = {total_cost, next_err, s, bit_val};
          }
        }
      }
    }

    // --- Find Optimal End State for Chunk ---
    int best_state = 0;
    double min_cost = infinity;
    for (int s = 0; s < numStates; ++s)
    {
      if (trellis[current_chunk][s].cost < min_cost)
      {
        min_cost = trellis[current_chunk][s].cost;
        best_state = s;
      }
    }

    // Save continuous exact error to initialize step 0 of the next chunk
    initial_error = trellis[current_chunk][best_state].exact_error;

    // --- Traceback Pass for Chunk ---
    int curr_s = best_state;
    for (size_t t = current_chunk; t > 0; --t)
    {
      outputBits[offset + t - 1] = trellis[t][curr_s].bit;
      curr_s = trellis[t][curr_s].prev_idx;
    }
  }
}

// --- Helper for Trellis Quantization ---
inline static void ApplyTrellisQuantization(const std::vector<double>& input, std::vector<bool>& outputBits, int numStates = 65)
{
  const double state_min = -2.0;
  const double state_max = 2.0;
  const double infinity = std::numeric_limits<double>::max();
  const size_t chunk_size = 4096; // Process in blocks to prevent massive memory allocation

  auto get_state_index = [&](double state) -> int {
    double normalized = (state - state_min) / (state_max - state_min);
    int idx = static_cast<int>(normalized * (numStates - 1));
    return std::max(0, std::min(idx, numStates - 1));
  };

  struct Node
  {
    double cost;
    double exact_error;
    int prev_idx;
    bool bit;
  };

  double initial_error = 0.0;

  for (size_t offset = 0; offset < input.size(); offset += chunk_size)
  {
    size_t current_chunk = std::min(chunk_size, input.size() - offset);

    // Trellis grid: [Time Step][State]
    std::vector<std::vector<Node>> trellis(current_chunk + 1, std::vector<Node>(numStates, {infinity, 0.0, -1, false}));

    // Initialize first step of the chunk
    int start_idx = get_state_index(initial_error);
    trellis[0][start_idx] = {0.0, initial_error, -1, false};

    // Forward Pass (Viterbi)
    for (size_t t = 0; t < current_chunk; ++t)
    {
      double x = input[offset + t];

      for (int s = 0; s < numStates; ++s)
      {
        if (trellis[t][s].cost == infinity)
          continue;

        double current_err = trellis[t][s].exact_error;

        // Evaluate both 0 and 1 output possibilities
        for (int b = 0; b < 2; ++b)
        {
          bool bit_val = (b == 1);
          double y = bit_val ? 1.0 : -1.0;

          // 1st order feedback: error accumulates
          double next_err = current_err + (x - y);

          // Cost function: To shape noise away from low frequencies, we minimize
          // the magnitude of the integrator state (next_err) over time.
          double step_cost = next_err * next_err;
          double total_cost = trellis[t][s].cost + step_cost;

          int next_s = get_state_index(next_err);

          // Keep the path with the lowest cumulative cost
          if (total_cost < trellis[t + 1][next_s].cost)
          {
            trellis[t + 1][next_s] = {total_cost, next_err, s, bit_val};
          }
        }
      }
    }

    // Traceback setup: Find the best final state for this chunk
    int best_state = 0;
    double min_cost = infinity;
    for (int s = 0; s < numStates; ++s)
    {
      if (trellis[current_chunk][s].cost < min_cost)
      {
        min_cost = trellis[current_chunk][s].cost;
        best_state = s;
      }
    }

    // Carry the exact continuous error state over to the next chunk's initial state
    initial_error = trellis[current_chunk][best_state].exact_error;

    // Traceback step: Work backwards to extract the optimal sequence of bits
    int curr_s = best_state;
    for (int t = current_chunk; t > 0; --t)
    {
      outputBits[offset + t - 1] = trellis[t][curr_s].bit;
      curr_s = trellis[t][curr_s].prev_idx;
    }
  }
}

// --- Main Audio Processing Function ---
inline static std::vector<int8_t> ReduceToOneBit(const std::vector<int32_t>& inputBuffer,
                                                 uint32_t sampleRate = 44100,
                                                 bool normalize = true,
                                                 bool applyLPF = true,
                                                 double cutoffFreq = 16000.0,
                                                 bool saturate = true,
                                                 double saturationDrive = 2.0,
                                                 bool useTrellis = false) // <-- New Parameter
{
  if (inputBuffer.empty())
    return {};

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
    Saturate(workingBuffer, saturationDrive);
  }

  size_t totalBits = workingBuffer.size();
  size_t totalBytes = (totalBits + 7) / 8;
  std::vector<int8_t> packedBytes(totalBytes, 0);
  std::vector<bool> outBits(totalBits, false);

  // --- Quantization Step ---
  if (useTrellis)
  {
    std::vector<double> doubleBuf(totalBits);
    for (size_t i = 0; i < totalBits; ++i)
    {
      doubleBuf[i] = static_cast<double>(workingBuffer[i]) / 2147483648.0;
    }

    // Automatically determine optimal processing parameters
    TrellisConfig config = SuggestTrellisConfig(totalBits);


    // ================= DEBUG OUTPUT =================
    constexpr size_t bytesPerNode = 24;
    size_t totalNodesAllocated = config.useChunking ? (config.chunkSize + 1) * config.numStates : (totalBits + 1) * config.numStates;

    double ramMB = static_cast<double>(totalNodesAllocated * bytesPerNode) / (1024.0 * 1024.0);

    MY_PRINTF("=== [Trellis Quantizer Debug] ===\n");
    MY_PRINTF("  Input Samples : %zu (%.2f sec)\n", totalBits, static_cast<double>(totalBits) / sampleRate);
    MY_PRINTF("  Processing    : %s\n", config.useChunking ? "CHUNKED" : "SINGLE-PASS");
    MY_PRINTF("  Num States    : %d\n", config.numStates);

    if (config.useChunking)
    {
      MY_PRINTF("  Chunk Size    : %zu samples\n", config.chunkSize);
    }

    MY_PRINTF("  Est. Peak RAM : %.2f MB\n", ramMB);
    MY_PRINTF("=================================\n");
    // ================================================

    if (config.useChunking)
    {
      ApplyTrellisQuantizationChunked(doubleBuf, outBits, config.numStates, config.chunkSize);
    }
    else
    {
      ApplyTrellisQuantizationSinglePass(doubleBuf, outBits, config.numStates);
    }
  }
  else
  {
    // Standard 1st-Order Delta-Sigma loop
    double error = 0.0;
    for (size_t i = 0; i < totalBits; ++i)
    {
      double sample = static_cast<double>(workingBuffer[i]) / 2147483648.0;
      double v = sample + error;

      bool bit = (v >= 0.0);
      double quantizedOutput = bit ? 1.0 : -1.0;

      error = v - quantizedOutput;
      outBits[i] = bit;
    }
  }

  // --- Bit Packing Step ---
  // Unified bit-packing logic separates the algorithm from the memory structuring
  for (size_t i = 0; i < totalBits; ++i)
  {
    if (outBits[i])
    {
      size_t byteIndex = i / 8;
      int bitInByte = 7 - static_cast<int>(i % 8);
      packedBytes[byteIndex] |= (1 << bitInByte);
    }
  }

  return packedBytes;
}
}; // namespace SampleTools