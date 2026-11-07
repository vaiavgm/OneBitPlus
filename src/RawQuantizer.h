#pragma once

#include "src/IQuantizer.h"
#include <vector>

class RawQuantizer : public IQuantizer
{
public:
  std::vector<bool> Quantize(const std::vector<double>& input, uint32_t /*sampleRate*/) override
  {
    std::vector<bool> outBits(input.size());
    for (size_t i = 0; i < input.size(); ++i)
    {
      outBits[i] = (input[i] >= 0.0);
    }

    return outBits;
  }
  std::string ToString() const override { return std::string("RawQuantizer"); }
};
