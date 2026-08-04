#pragma once
#pragma warning(disable : 4309)

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

  // expose a few accessors for persistence / UI



  // Total number of velocity slots (one per MIDI velocity 0-127)
  static constexpr size_t kTotalSlots = 128;

public:
  size_t DynamicCount() const { return mDynamicSlots.size(); }

  uint32_t GetDynamicSampleRate(size_t idx) const
  {
    if (idx >= mDynamicSlots.size())
      return 0;
    return mDynamicSlots[idx].sampleRate;
  }
  // Static prepared sample array evaluated entirely at compile time
  // A few slots can be reserved at compile time. The rest of the 128 velocity slots
  // map to dynamic samples stored in mDynamicSlots.
  // Example: uncomment and adjust to reserve prepared slots:
  // static constexpr std::array<SampleInfo, 5> kPreparedSamples = {SampleInfo(kType1), SampleInfo(kType2), SampleInfo(kType3), SampleInfo(kType4), SampleInfo(kType5)};
  static constexpr std::array<SampleInfo, 0> kPreparedSamples = {};

  static constexpr size_t kPreparedCount = kPreparedSamples.size();

  // static constexpr std::array<SampleInfo, 3> kPreparedSamples = {SampleInfo(HR16_Crash02), SampleInfo(XIL_snare_9), SampleInfo(XIL_kick_6)};




  // Add packed dynamic samples. Dynamic slots occupy the remaining velocity slots
  // after the prepared slots. Returns the dynamic index on success, or -1 if
  // there is no remaining slot available (max kTotalSlots - kPreparedCount).
  int AddSample(const int8_t* data, size_t size, uint32_t sampleRate = 44100)
  {
    const size_t maxDynamic = kTotalSlots - kPreparedCount;
    if (mDynamicSlots.size() >= maxDynamic)
      return -1;
    mDynamicSlots.push_back(DynamicSlot{std::vector<int8_t>(data, data + size), sampleRate});
    return static_cast<int>(mDynamicSlots.size() - 1);
  }

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
    // Map normalized level [0..1] to MIDI velocities 1..127 (no velocity 0).
    // Use level*127 + 0.5 to round to the nearest integer velocity, then clamp to [1,127].
    int velInt = std::clamp(static_cast<int>(level * 127.0 + 0.5), 1, 127);

    // Map velocity directly to a stable slot index.
    // Velocities 1..kPreparedCount map to prepared static samples (index = vel-1).
    // Velocities (kPreparedCount+1)..127 map to dynamic slots at index (vel - kPreparedCount - 1).

    if (velInt <= static_cast<int>(kPreparedCount))
    {
      return kPreparedSamples[velInt - 1];
    }

    size_t dynamicIdx = static_cast<size_t>(velInt) - kPreparedCount - 1;
    if (dynamicIdx < mDynamicSlots.size() && !mDynamicSlots[dynamicIdx].data.empty())
    {
      const auto& slot = mDynamicSlots[dynamicIdx];
      return SampleInfo(slot.data.data(), slot.data.size(), slot.sampleRate);
    }

    // Requested velocity maps to an uninitialized slot -> silence
    return SampleInfo(kSilence);
  }
};

} // namespace SampleData