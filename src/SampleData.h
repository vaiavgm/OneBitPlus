#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>
#include "SampleTools.h"

namespace SampleData
{
inline constexpr int8_t kType1[] = {
  0xAA, 0x55, 0xF0, 0x0F, 0xCC, 0x33, 0xFF, 0x00, 0x81, 0x42, 0x24, 0x18,
};
inline constexpr int8_t kType2[] = {
  0xF0, 0xF0, 0x0F, 0x0F, 0xAA, 0xAA, 0x55, 0x55, 0xC3, 0x3C, 0x99, 0x66,
};
inline constexpr int8_t kType3[] = {
  0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80,
};
inline constexpr int8_t kType4[] = {
  0xFF, 0x00, 0xFF, 0x00, 0xE7, 0x18, 0xE7, 0x18, 0xBD, 0x42, 0xBD, 0x42, 0x7E, 0x81,
};
inline constexpr int8_t kType5[] = {
  0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0xFF, 0xF0, 0x00, 0xFC, 0xF0, 0xC0, 0x00, 0xF8, 0xE0, 0x80, 0x00, 0xF0, 0xC0, 0x00, 0x00, 0xE0, 0x80, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x00};
inline constexpr int8_t kSilence[] = {0x00};

struct SampleInfo
{
  const int8_t* data;
  size_t size;
  uint32_t sampleRate;

  template <size_t N>
  constexpr SampleInfo(const int8_t (&arr)[N], uint32_t sr = 44100)
    : data(arr)
    , size(N)
    , sampleRate(sr)
  {
  }

  constexpr SampleInfo(const int8_t* pData, size_t numBytes, uint32_t sr = 44100)
    : data(pData)
    , size(numBytes)
    , sampleRate(sr)
  {
  }
};


class SampleManager
{
private:
  struct DynamicSlot
  {
    std::vector<int8_t> data;
    uint32_t sampleRate;
  };

  std::vector<DynamicSlot> mDynamicSlots;

public:
  // Legacy raw insert if you already have pre-packed bytes
  void AddSample(const int8_t* data, size_t size, uint32_t sampleRate = 44100) { mDynamicSlots.push_back(DynamicSlot{std::vector<int8_t>(data, data + size), sampleRate}); }

  // Two-step pipeline integration: Takes a 32-bit buffer, resamples, packs to 1-bit, and stores with its sample rate
  void AddDynamicSampleFrom32Bit(const std::vector<int32_t>& rawBuffer, uint32_t sourceRate, uint32_t targetRate = 44100, ResampleAlgo algo = ResampleAlgo::Nearest)
  {
    auto resampled32 = SampleTools::Resample(rawBuffer, sourceRate, targetRate, algo);
    auto packed1Bit = SampleTools::ReduceToOneBit(resampled32);

    mDynamicSlots.push_back(DynamicSlot{std::move(packed1Bit), targetRate});
  }

  void ClearDynamicSamples() { mDynamicSlots.clear(); }

  SampleInfo GetSampleForVelocity(double level) const
  {
    int velInt = static_cast<int>(std::round(level * 127.0));

    size_t totalSlots = 5 + mDynamicSlots.size();
    int index = (velInt * static_cast<int>(totalSlots)) / 128;
    index = std::clamp(index, 0, static_cast<int>(totalSlots) - 1);

    if (index == 0)
      return SampleInfo(kType1);
    if (index == 1)
      return SampleInfo(kType2);
    if (index == 2)
      return SampleInfo(kType3);
    if (index == 3)
      return SampleInfo(kType4);
    if (index == 4)
      return SampleInfo(kType5);

    size_t dynamicIdx = index - 5;
    if (dynamicIdx < mDynamicSlots.size() && !mDynamicSlots[dynamicIdx].data.empty())
    {
      const auto& slot = mDynamicSlots[dynamicIdx];
      return SampleInfo(slot.data.data(), slot.data.size(), slot.sampleRate);
    }

    return SampleInfo(kSilence);
  }
};

} // namespace SampleData