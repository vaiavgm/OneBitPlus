#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace SampleData
{

// --- 1. Static Raw Sample Data ---
inline constexpr int8_t kType1[] = {0xAA, 0x55, 0xF0, 0x0F, 0xCC, 0x33, 0xFF, 0x00, 0x81, 0x42, 0x24, 0x18};
inline constexpr int8_t kType2[] = {0xF0, 0xF0, 0x0F, 0x0F, 0xAA, 0xAA, 0x55, 0x55, 0xC3, 0x3C, 0x99, 0x66};
inline constexpr int8_t kType3[] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80};
inline constexpr int8_t kType4[] = {0xFF, 0x00, 0xFF, 0x00, 0xE7, 0x18, 0xE7, 0x18, 0xBD, 0x42, 0xBD, 0x42, 0x7E, 0x81};
inline constexpr int8_t kType5[] = {
  0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0xFF, 0xF0, 0x00, 0xFC, 0xF0, 0xC0, 0x00, 0xF8, 0xE0, 0x80, 0x00, 0xF0, 0xC0, 0x00, 0x00, 0xE0, 0x80, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x00};
inline constexpr int8_t kSilence[] = {0x00};

// --- 2. Lightweight Sample Pointer View ---
struct SampleInfo
{
  const int8_t* data = nullptr;
  size_t size = 0;
  uint32_t sampleRate = 44100;

  constexpr SampleInfo() = default;

  // Construct from C-style static array
  template <size_t N>
  constexpr SampleInfo(const int8_t (&arr)[N], uint32_t sr = 44100)
    : data(arr)
    , size(N)
    , sampleRate(sr)
  {
  }

  // Construct from raw pointer and size
  constexpr SampleInfo(const int8_t* pData, size_t numBytes, uint32_t sr = 44100)
    : data(pData)
    , size(numBytes)
    , sampleRate(sr)
  {
  }
};

// --- 3. Unified Sample Manager ---
class SampleManager
{
private:
  struct DynamicSlot
  {
    std::vector<int8_t> data;
    uint32_t sampleRate;
  };

  std::vector<DynamicSlot> mDynamicSlots;

  // Compile-time Generator for 128-velocity Index Mapping
  template <size_t NumSlots>
  static constexpr std::array<uint8_t, 128> GenerateVelocityLUT()
  {
    std::array<uint8_t, 128> lut{};
    if (NumSlots == 0)
      return lut;

    for (int vel = 0; vel < 128; ++vel)
    {
      size_t idx = (static_cast<size_t>(vel) * NumSlots) / 128;
      lut[vel] = static_cast<uint8_t>(std::min(idx, NumSlots - 1));
    }
    return lut;
  }

public:
  // Static prepared sample array evaluated entirely at compile time
  // static constexpr std::array<SampleInfo, 5> kPreparedSamples = {SampleInfo(kType1), SampleInfo(kType2), SampleInfo(kType3), SampleInfo(kType4), SampleInfo(kType5)};
  static constexpr std::array<SampleInfo, 0> kPreparedSamples = {};

  // Static precomputed velocity lookup table
  static constexpr auto kStaticVelocityLUT = GenerateVelocityLUT<kPreparedSamples.size()>();

  // Add packed dynamic samples
  int AddSample(const int8_t* data, size_t size, uint32_t sampleRate = 44100) { mDynamicSlots.push_back(DynamicSlot{std::vector<int8_t>(data, data + size), sampleRate}); return mDynamicSlots.size() - 1; }

  void ClearDynamicSamples() { mDynamicSlots.clear(); }

  std::vector<int8_t> GetDynamicSampleData(int index) const
  {
    if (index < 0 || index >= static_cast<int>(mDynamicSlots.size()))
      return {};
    return mDynamicSlots[index].data;
  }

  // Ultra-fast velocity lookup
  SampleInfo GetSampleForVelocity(double level) const
  {
    int velInt = std::clamp(static_cast<int>(level * 127.0 + 0.5), 0, 127);

    // --- PATH A: Static-Only Fast Path (Pure O(1) Precomputed LUT) ---
    if (mDynamicSlots.empty())
    {
      uint8_t index = kStaticVelocityLUT[velInt];
      return kPreparedSamples[index];
    }

    // --- PATH B: Hybrid Path for Dynamic Slots ---
    size_t totalSlots = kPreparedSamples.size() + mDynamicSlots.size();
    size_t index = (static_cast<size_t>(velInt) * totalSlots) >> 7; // Fast division by 128
    index = std::min(index, totalSlots - 1);

    if (index < kPreparedSamples.size())
    {
      return kPreparedSamples[index];
    }

    size_t dynamicIdx = index - kPreparedSamples.size();
    if (!mDynamicSlots[dynamicIdx].data.empty())
    {
      const auto& slot = mDynamicSlots[dynamicIdx];
      return SampleInfo(slot.data.data(), slot.data.size(), slot.sampleRate);
    }

    return SampleInfo(kSilence);
  }
};

} // namespace SampleData