#pragma once

#include <vector>
#include <string>
#include <cstdint>

class IQuantizer
{
public:
  virtual ~IQuantizer() = default;
  virtual std::vector<bool> Quantize(const std::vector<double>& input, uint32_t sampleRate) = 0;
  // Human-readable description of quantizer and its parameters
  virtual std::string ToString() const { return "IQuantizer"; }
};
