#pragma once
#include "Oscillator.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

using namespace iplug;

template <typename T>
class SampleOscillator : public IOscillator<T>
{
private:
  static constexpr uint32_t defaultSampleRate = 44100;
  const int8_t* mSampleData = nullptr;
  size_t mNumBytes = 0;
  uint32_t mFileSampleRate = defaultSampleRate; // Dynamically set during BindSample
  double mBitPhase = 0.0;
  bool mLooping = false;
  bool mFinished = false;

public:
  static constexpr size_t kMaxSampleBytes = 1'048'576;
  static constexpr double kReferenceFreqHz = 440.0;

  SampleOscillator(double startPhase = 0., double startFreq = 1.)
    : IOscillator<T>(startPhase, startFreq)
  {
  }

  // Compile-time array overload
  template <size_t N>
  void BindSample(const int8_t (&pData)[N], uint32_t fileSampleRate = defaultSampleRate)
  {
    mSampleData = pData;
    mNumBytes = std::min(static_cast<size_t>(N), kMaxSampleBytes);
    mFileSampleRate = fileSampleRate;
    Reset();
  }

  // Runtime pointer overload for dynamic WAV files
  void BindSample(const int8_t* pData, size_t numBytes, uint32_t fileSampleRate = defaultSampleRate)
  {
    mSampleData = pData;
    mNumBytes = std::min(numBytes, kMaxSampleBytes);
    mFileSampleRate = fileSampleRate;
    Reset();
  }

  void SetLooping(bool loop) { mLooping = loop; }

  void Reset()
  {
    IOscillator<T>::Reset();
    mBitPhase = 0.0;
    mFinished = false;
  }

  inline T Process(double freqHz) override
  {
    if (mSampleData == nullptr || mNumBytes == 0 || mFinished)
      return static_cast<T>(0.0);

    const size_t totalBits = mNumBytes * 8;

    // Nearest-Neighbor fetch: strips the decimal to grab the exact closest bit
    size_t bitIndex = static_cast<size_t>(mBitPhase);

    if (bitIndex >= totalBits)
    {
      if (mLooping)
      {
        mBitPhase = std::fmod(mBitPhase, static_cast<double>(totalBits));
        bitIndex = static_cast<size_t>(mBitPhase);
      }
      else
      {
        mFinished = true;
        return static_cast<T>(0.0);
      }
    }

    // 1-Bit unpacking magic
    const size_t byteIndex = bitIndex / 8;
    const int bitInByte = 7 - static_cast<int>(bitIndex % 8);
    const int8_t byte = mSampleData[byteIndex];
    const bool bitSet = (byte >> bitInByte) & 1;

    // The core math: File Sample Rate * Pitch Multiplier
    // IOscillator<T>::mSampleRate is the DAW's current sample rate
    const double bitRate = static_cast<double>(mFileSampleRate) * (freqHz / kReferenceFreqHz);
    mBitPhase += bitRate / IOscillator<T>::mSampleRate;

    return bitSet ? static_cast<T>(1.0) : static_cast<T>(0.0);
  }


};
