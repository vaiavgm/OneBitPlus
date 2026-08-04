#pragma once

#include <vector>
#include "src/IQuantizer.h"

// --- 4. Standard Delta-Sigma Quantizer ---
class DeltaSigmaQuantizer : public IQuantizer
{
public:
  std::vector<bool> Quantize(const std::vector<double>& input, uint32_t sampleRate) override
  {
    std::vector<bool> outBits(input.size());
    double error = 0.0;

    for (size_t i = 0; i < input.size(); ++i)
    {
      double v = input[i] + error;
      bool bit = (v >= 0.0);
      double quantized = bit ? 1.0 : -1.0;

      error = v - quantized;
      outBits[i] = bit;
    }

    return outBits;
  }
  std::string ToString() const { return std::string("DeltaSigmaQuantizer"); }
};