#pragma once

#include "src/IQuantizer.h"
#include <vector>

class RawQuantizer : public IQuantizer
{
public:
  std::vector<bool> Quantize(const std::vector<double>& input, uint32_t sampleRate) override
  {
    std::vector<bool> outBits(input.size());
    double error = 0.0;

    for (size_t i = 0; i < input.size(); ++i)
    {

      bool bit = (input[i] >= 0.0);
      double quantized = bit ? 1.0 : -1.0;

      outBits[i] = bit;
    }

    return outBits;
  }
};