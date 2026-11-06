#pragma once

// Versioned (de)serialization of the plugin's custom state block (everything after the parameters).
//
// The functions are templates over the chunk type, so the plugin uses them with iplug::IByteChunk and the
// unit tests use a small in-memory fake. The chunk type must provide the IByteChunk subset:
//   Put(const T*), PutBytes(const void*, int), PutStr(const char*)
//   Get(T*, int pos), GetBytes(void*, int n, int pos), GetStr(Str&, int pos), Size()
// where every Get* returns the position after the read value, or a negative value on failure.
//
// Layouts
// -------
//  v0  (implicit) no block at all, only parameters. Read as "no samples".
//  v1  u32 version=1 | u32 numSlots | numSlots x {u32 sr, u32 size, size bytes}
//      | u32 pathCount | pathCount x {str path, str pipeline}
//  v2  u32 version=2 | u32 dspVersion | u32 numSlots
//      | numSlots x {u32 sr, u32 size, size bytes, str path, str pipeline}
//  v3  u32 version=3 | u32 dspVersion | u32 compatFlags | u32 numSlots | (slots as in v2)
//
// dspVersion describes the *sound behaviour* the state was saved with, independent of the container
// layout. Loading a state with an older dspVersion lets the plugin migrate parameters so that existing
// projects keep sounding the same (see VaiaOneBitPlus::ApplyMigrations).
//
//  dspVersion 1: the original behaviour (also every state without a version: v0, v1, and v2 written by
//                clean-up builds before the sound changes).
//  dspVersion 2: audible release, host tempo for synced LFOs, samples at their own rate, pitch keytrack,
//                DC-blocked output.
//  dspVersion 3: the "Sampler Pitch" parameter (it reuses the retired Input Dither slot: states before
//                version 3 get it reset to off).
//
// compatFlags are switches that keep a *behaviour* of an older version for as long as the user wants.
// They are saved with the project (see ECompat).

#include <cstdint>
#include <string>
#include <vector>

#include "src/SampleData.h"

namespace OneBitState
{

constexpr uint32_t kLatestVersion = 3;

// Sound-behaviour versions (see the comment at the top)
constexpr uint32_t kDspVersionLegacy = 1;
constexpr uint32_t kLatestDspVersion = 3;

// Behaviours of older versions that a project can keep using
enum ECompat : uint32_t
{
  // Synced LFOs run as if the host tempo was always 120 BPM, and samples play at 44.1 kHz speed
  // (the original behaviour: tempo and sample rate were not passed on to the DSP).
  kCompatLegacyTiming = 1u << 0,
};

// Sanity limits for reading untrusted data.
constexpr uint32_t kMaxSlots = static_cast<uint32_t>(SampleData::SampleManager::kMaxDynamicSlots);
constexpr uint32_t kMaxSampleBytes = 1'048'576; // matches SampleOscillator::kMaxSampleBytes
constexpr uint32_t kMaxSampleRate = 1'536'000;

struct Data
{
  uint32_t version = 0;                       // container version that was read (0 = no block)
  uint32_t dspVersion = kDspVersionLegacy;    // sound-behaviour version the state was saved with
  uint32_t compatFlags = 0;                   // ECompat bits (container v3 and later; 0 for older states)
  std::vector<SampleData::Slot> slots;
  bool valid = true;                          // false if the block was present but corrupt / truncated
};

template <class Chunk>
void Write(Chunk& chunk, const std::vector<SampleData::Slot>& slots, uint32_t dspVersion = kLatestDspVersion, uint32_t compatFlags = 0)
{
  uint32_t version = kLatestVersion;
  chunk.Put(&version);
  chunk.Put(&dspVersion);
  chunk.Put(&compatFlags);

  uint32_t numSlots = static_cast<uint32_t>(slots.size());
  chunk.Put(&numSlots);
  for (const auto& slot : slots)
  {
    uint32_t sr = slot.sampleRate;
    uint32_t size = static_cast<uint32_t>(slot.data.size());
    chunk.Put(&sr);
    chunk.Put(&size);
    if (size)
      chunk.PutBytes(slot.data.data(), static_cast<int>(size));
    chunk.PutStr(slot.path.c_str());
    chunk.PutStr(slot.pipeline.c_str());
  }
}

namespace detail
{
template <class Chunk>
bool ReadSampleBytes(const Chunk& chunk, int& pos, SampleData::Slot& slot)
{
  uint32_t sr = 0, size = 0;
  if ((pos = chunk.Get(&sr, pos)) < 0 || (pos = chunk.Get(&size, pos)) < 0)
    return false;
  if (size > kMaxSampleBytes || sr > kMaxSampleRate || static_cast<int64_t>(size) > static_cast<int64_t>(chunk.Size()) - pos)
    return false;
  slot.sampleRate = sr;
  slot.data.resize(size);
  if (size && (pos = chunk.GetBytes(slot.data.data(), static_cast<int>(size), pos)) < 0)
    return false;
  return true;
}

template <class Str, class Chunk>
bool ReadString(const Chunk& chunk, int& pos, std::string& out)
{
  Str s;
  if ((pos = chunk.GetStr(s, pos)) < 0)
    return false;
  out = s.Get();
  return true;
}
} // namespace detail

// Reads the custom block starting at `startPos`. Returns the position after the block, or `startPos` if
// there is no block (v0) or it is corrupt (then out.valid == false and out.slots is empty).
template <class Str, class Chunk>
int Read(const Chunk& chunk, int startPos, Data& out)
{
  out = Data{};
  int pos = startPos;

  uint32_t version = 0;
  if (startPos < 0 || startPos >= chunk.Size() || (pos = chunk.Get(&version, pos)) < 0)
    return startPos; // v0: parameters only

  auto fail = [&]() {
    out.slots.clear();
    out.valid = false;
    return startPos;
  };

  out.version = version;
  if (version == 1)
  {
    uint32_t numSlots = 0;
    if ((pos = chunk.Get(&numSlots, pos)) < 0 || numSlots > kMaxSlots)
      return fail();
    out.slots.resize(numSlots);
    for (auto& slot : out.slots)
    {
      if (!detail::ReadSampleBytes(chunk, pos, slot))
        return fail();
    }

    uint32_t pathCount = 0;
    if ((pos = chunk.Get(&pathCount, pos)) < 0 || pathCount > kMaxSlots)
      return fail();
    for (uint32_t i = 0; i < pathCount; ++i)
    {
      std::string path, pipeline;
      if (!detail::ReadString<Str>(chunk, pos, path) || !detail::ReadString<Str>(chunk, pos, pipeline))
        return fail();
      // v1 stored paths in a parallel list; they belong to the sample with the same index
      if (i < out.slots.size())
      {
        out.slots[i].path = std::move(path);
        out.slots[i].pipeline = std::move(pipeline);
      }
    }
  }
  else if (version == 2 || version == 3)
  {
    uint32_t numSlots = 0;
    if ((pos = chunk.Get(&out.dspVersion, pos)) < 0)
      return fail();
    if (version == 3 && (pos = chunk.Get(&out.compatFlags, pos)) < 0)
      return fail();
    if ((pos = chunk.Get(&numSlots, pos)) < 0 || numSlots > kMaxSlots)
      return fail();
    out.slots.resize(numSlots);
    for (auto& slot : out.slots)
    {
      if (!detail::ReadSampleBytes(chunk, pos, slot) || !detail::ReadString<Str>(chunk, pos, slot.path) || !detail::ReadString<Str>(chunk, pos, slot.pipeline))
        return fail();
    }
  }
  else
  {
    return fail(); // unknown (newer) version: don't guess
  }

  // v1 states saved empty slots too; they were never added on load, keep that behaviour
  std::vector<SampleData::Slot> nonEmpty;
  for (auto& s : out.slots)
    if (!s.data.empty())
      nonEmpty.push_back(std::move(s));
  out.slots = std::move(nonEmpty);
  return pos;
}

} // namespace OneBitState
