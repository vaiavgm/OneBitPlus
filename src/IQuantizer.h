#pragma once

class IQuantizer
{
public:
  virtual ~IQuantizer() = default;
  virtual std::vector<bool> Quantize(const std::vector<double>& input, uint32_t sampleRate) = 0;
};
