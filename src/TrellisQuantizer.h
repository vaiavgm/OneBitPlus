#pragma once

#include <vector>
#include <algorithm>
#include "src/IAudioEffect.h"
#include "src/IQuantizer.h"



////////////////////////
// Trellis setup
////////////////////////

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

// --- 5. Trellis Quantizer Effect ---
class TrellisQuantizer : public IQuantizer
{
private:
  size_t m_maxRamBytes;

public:
  explicit TrellisQuantizer(size_t maxRamBytes = 64 * 1024 * 1024)
    : m_maxRamBytes(maxRamBytes)
  {
  }

  std::vector<bool> Quantize(const std::vector<double>& input, uint32_t sampleRate) override
  {
    if (input.empty())
      return {};

    size_t totalBits = input.size();
    std::vector<bool> outBits(totalBits, false);

    TrellisConfig config = SuggestTrellisConfig(totalBits, m_maxRamBytes);

    // Debug output via MY_PRINTF macro
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

    if (config.useChunking)
    {
      ApplyTrellisQuantizationChunked(input, outBits, config.numStates, config.chunkSize);
    }
    else
    {
      ApplyTrellisQuantizationSinglePass(input, outBits, config.numStates);
    }

    return outBits;
  }
};