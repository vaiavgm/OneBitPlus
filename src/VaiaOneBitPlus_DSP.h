#pragma once

#define MY_PRINTF(...)                                                                                                                                                                                 \
  {                                                                                                                                                                                                    \
    char buf[512];                                                                                                                                                                                     \
    sprintf(buf, __VA_ARGS__);                                                                                                                                                                         \
    OutputDebugString(buf);                                                                                                                                                                            \
  }

#include <IPlugLogger.h>
#include <IPlugMidi.h>

#include <IPlugConstants.h>
#include <SynthVoice.h>
#include <VoiceAllocator.h>
#include <array>
#include <heapbuf.h>
#include <ptrlist.h>
#include <stdlib.h>


#include <ADSREnvelope.h>
#include <LFO.h>
#include <MidiSynth.h>
#include <Smoothers.h>
#include <cstdint>

#include "VaiaOneBitPlus.h"
#include "SampleData.h"
#include "SampleOscillator.h"
#include "VaiaOscillator.h"

#include <algorithm>
#include <cstring>


constexpr unsigned int osc_count = 32U; // update this when adding more OSCs
constexpr unsigned int env_count = osc_count * 2;

enum EModulations
{
  kModGainSmoother = 0,

  // the grouping is important! first all pitch sustains etc., because we use offsets based on OSC id
  kModPitchSustainSmoother1,
  kModPitchSustainSmoother2,
  kModPitchSustainSmoother3,
  kModPitchSustainSmoother4,

  kModPwmSustainSmoother1,
  kModPwmSustainSmoother2,
  kModPwmSustainSmoother3,
  kModPwmSustainSmoother4,

  kModPitchLFO1,
  kModPitchLFO2,
  kModPitchLFO3,
  kModPitchLFO4,

  kModPwmLFO1,
  kModPwmLFO2,
  kModPwmLFO3,
  kModPwmLFO4,

  kNumModulations,
};

enum class OSC_Algorithm
{
  ALGO_FLIP_ONE, // This one is not quite correct, as it outputs some zeroes where it shouldn't
  ALGO_MOD_TWO,
  ALGO_PIN_PULSE, // just add and clamp
};

const OSC_Algorithm used_algo = OSC_Algorithm::ALGO_PIN_PULSE;




template <typename T>
class OneBitPlusDSP
{
public:
  int mMaxVoices;
  SampleData::SampleManager mSampleManager;




#pragma mark - Voice
  class Voice : public SynthVoice
  {
  private:
    OneBitPlusDSP& mParentDSP;

    // --- NEW: Voice State Caching ---
    enum class VoiceMode
    {
      kNoise1,
      kNoise2,
      kNoise3,
      kSampler,
      kSynth
    };
    VoiceMode mMode = VoiceMode::kSynth;
    int mActiveOscId = 0;

    double mCachedNoiseThreshold = 0.0;
    int mLastClock4 = -1;
    double mTempNoise4 = 0.0;
    int mLastClock8 = -1;
    double mTempNoise8 = 0.0;

  public:
    // Type 1
    std::array<VaiaOscillator<T>, 8> mUnisonOsc1{};
    ADSREnvelope<T> mPwmEnv1;
    ADSREnvelope<T> mPitchEnv1;

    // Type 2
    std::array<VaiaOscillator<T>, 8> mUnisonOsc2{};
    ADSREnvelope<T> mPwmEnv2;
    ADSREnvelope<T> mPitchEnv2;

    // Type 3
    std::array<VaiaOscillator<T>, 8> mUnisonOsc3{};
    ADSREnvelope<T> mPwmEnv3;
    ADSREnvelope<T> mPitchEnv3;

    // Type 4
    std::array<VaiaOscillator<T>, 8> mUnisonOsc4{};
    ADSREnvelope<T> mPwmEnv4;
    ADSREnvelope<T> mPitchEnv4;

    // sample osc
    SampleOscillator<T> mSampleOsc{};

    int mDebugSampleCounter = 0;

    std::array<VaiaOscillator<T>*, osc_count> all_oscs{&mUnisonOsc1[0], &mUnisonOsc1[1], &mUnisonOsc1[2], &mUnisonOsc1[3], &mUnisonOsc1[4], &mUnisonOsc1[5], &mUnisonOsc1[6], &mUnisonOsc1[7],
                                                       &mUnisonOsc2[0], &mUnisonOsc2[1], &mUnisonOsc2[2], &mUnisonOsc2[3], &mUnisonOsc2[4], &mUnisonOsc2[5], &mUnisonOsc2[6], &mUnisonOsc2[7],
                                                       &mUnisonOsc3[0], &mUnisonOsc3[1], &mUnisonOsc3[2], &mUnisonOsc3[3], &mUnisonOsc3[4], &mUnisonOsc3[5], &mUnisonOsc3[6], &mUnisonOsc3[7],
                                                       &mUnisonOsc4[0], &mUnisonOsc4[1], &mUnisonOsc4[2], &mUnisonOsc4[3], &mUnisonOsc4[4], &mUnisonOsc4[5], &mUnisonOsc4[6], &mUnisonOsc4[7]};

    std::array<ADSREnvelope<T>*, env_count> all_envs{&mPwmEnv1, &mPwmEnv2, &mPwmEnv3, &mPwmEnv4, &mPitchEnv1, &mPitchEnv2, &mPitchEnv3, &mPitchEnv4};

    std::array<bool, osc_count> pwmKeyTracks{};
    std::array<double, osc_count> pwmModStrengths{};
    std::array<double, osc_count> pwmOffsetStrengths{};
    std::array<double, osc_count> pitchKeyTrackStrengths{};
    std::array<double, osc_count> pitchModStrengths{};
    std::array<double, osc_count> pitchOffsetStrengths{};

    std::array<int, osc_count> extraUnisonCounts{};
    std::array<double, osc_count> extraDetuneCents{};

    OSC_Algorithm algo = used_algo;
    WDL_TypedBuf<float> mTimbreBuffer;
    double mModWheel{0.0};
    uint32_t mRandSeed = 0;

    // Restored the exact lambda constructors to prevent compiler errors
    Voice(OneBitPlusDSP& parent)
      : mParentDSP(parent)
      , mPwmEnv1("gain",
                 [&]() {
                   for (auto& o : mUnisonOsc1)
                     o.Reset();
                 })
      , mPitchEnv1("gain",
                   [&]() {
                     for (auto& o : mUnisonOsc1)
                       o.Reset();
                   })
      , mPwmEnv2("gain",
                 [&]() {
                   for (auto& o : mUnisonOsc2)
                     o.Reset();
                 })
      , mPitchEnv2("gain",
                   [&]() {
                     for (auto& o : mUnisonOsc2)
                       o.Reset();
                   })
      , mPwmEnv3("gain",
                 [&]() {
                   for (auto& o : mUnisonOsc3)
                     o.Reset();
                 })
      , mPitchEnv3("gain",
                   [&]() {
                     for (auto& o : mUnisonOsc3)
                       o.Reset();
                   })
      , mPwmEnv4("gain",
                 [&]() {
                   for (auto& o : mUnisonOsc4)
                     o.Reset();
                 })
      , mPitchEnv4("gain", [&]() {
        for (auto& o : mUnisonOsc4)
          o.Reset();
      })
    {
      for (auto& c : extraUnisonCounts)
      {
        c = 1;
      }
      for (auto& d : extraDetuneCents)
      {
        d = 0.0;
      }
    }

    bool GetBusy() const override
    {
      if (mMode == VoiceMode::kSynth)
      {
        return all_envs[mActiveOscId]->GetBusy();
      }
      return SynthVoice::GetBusy();
    }

    void Trigger(double level, bool isRetrigger) override
    {
      SynthVoice::Trigger(level, isRetrigger);

      double pitch = mInputs[kVoiceControlPitch].endValue;
      double midiNote = (pitch * 12.0) + 69.1;
      double velocity = level * 127.f;

      mCachedNoiseThreshold = velocity * (1.0 - mModWheel);

      if (midiNote < 1)
        mMode = VoiceMode::kNoise1;
      else if (midiNote < 2)
        mMode = VoiceMode::kNoise2;
      else if (midiNote < 3)
        mMode = VoiceMode::kNoise3;
      else if (midiNote < 4)
      {
        mMode = VoiceMode::kSampler;
        mSampleOsc.Reset();
        auto sampleInfo = mParentDSP.mSampleManager.GetSampleForVelocity(level);
        mSampleOsc.BindSample(sampleInfo.data, sampleInfo.size);
      }
      else
      {
        mMode = VoiceMode::kSynth;
        int velInt = static_cast<int>(std::round(velocity));
        mActiveOscId = std::clamp((velInt > 0 ? velInt - 1 : 0) / 32, 0, 3);

        switch (mActiveOscId)
        {
        case 0:
          for (auto& o : mUnisonOsc1)
            o.Reset();
          break;
        case 1:
          for (auto& o : mUnisonOsc2)
            o.Reset();
          break;
        case 2:
          for (auto& o : mUnisonOsc3)
            o.Reset();
          break;
        case 3:
          for (auto& o : mUnisonOsc4)
            o.Reset();
          break;
        }

        if (isRetrigger)
        {
          all_envs[mActiveOscId]->Retrigger(1);
          all_envs[mActiveOscId + 4]->Retrigger(1);
        }
        else
        {
          all_envs[mActiveOscId]->Start(1);
          all_envs[mActiveOscId + 4]->Start(1);
        }
      }
    }

    void Release() override
    {
      if (mMode == VoiceMode::kSynth)
      {
        all_envs[mActiveOscId]->Release();
        all_envs[mActiveOscId + 4]->Release();
      }

      SynthVoice::Release();
    }

    // Restored exact rand() logic
    inline void ProcessNoise1(T** outputs, int i, double noiseThreshold)
    {
      double tempNoise = outputs[0][i] + (rand() % 255) > noiseThreshold ? -0.9 : 1.1;
      if (tempNoise < 0.0f)
        tempNoise = 0.0f;
      outputs[0][i] = outputs[1][i] = tempNoise;
    }

    inline void ProcessNoise2(T** outputs, int i, double noiseThreshold, int& lastClock, double& tempNoise)
    {
      int clock = i / 4;
      if (clock != lastClock)
      {
        lastClock = clock;
        double newNoise = outputs[0][i] + (rand() % 255) > noiseThreshold ? -0.9 : 1.1;
        if (newNoise < 0.0f)
          newNoise = 0.0f;
        tempNoise = newNoise;
      }
      outputs[0][i] = outputs[1][i] = tempNoise;
    }

    inline void ProcessNoise3(T** outputs, int i, double noiseThreshold, int& lastClock, double& tempNoise)
    {
      int clock = i / 8;
      if (clock != lastClock)
      {
        lastClock = clock;
        double newNoise = outputs[0][i] + (rand() % 255) > noiseThreshold ? -0.9 : 1.1;
        if (newNoise < 0.0f)
          newNoise = 0.0f;
        tempNoise = newNoise;
      }
      outputs[0][i] = outputs[1][i] = tempNoise;
    }

    // Restored your exact hardcoded 440 oscFreq to avoid parameter mismatch
    inline void ProcessSampleOscillator(T** outputs, int i, double oscFreq, SampleOscillator<T>* sampleOsc)
    {
      if (sampleOsc && algo == OSC_Algorithm::ALGO_PIN_PULSE)
      {
        T sampleVal = sampleOsc->Process(440);
        T outputVal = outputs[0][i];

        if (outputVal < 0.1f)
        {
          outputs[0][i] = std::max(outputs[0][i], sampleVal);
          outputs[1][i] = std::max(outputs[1][i], sampleVal);
        }
      }
    }

    inline void ProcessStandardOscillators(T** outputs, int i, double oscFreq, double pwmFunc, double velocity, int unison, double detuneRange, std::array<VaiaOscillator<T>, 8>* arr)
    {
      bool anyHigh = false;
      for (int u = 0; u < unison; ++u)
      {
        double offsetCents = (unison == 1) ? 0.0 : (-detuneRange * 0.5 + (detuneRange * static_cast<double>(u)) / static_cast<double>(unison - 1));
        double freq = oscFreq * pow(2.0, offsetCents / 1200.0);

        auto& uosc = (*arr)[u];
        uosc.SetPWM(pwmFunc);
        if (uosc.Process(freq) > 0.0)
        {
          anyHigh = true;
        }
      }

      double base = anyHigh ? 1.0 : -1.0;
      if (algo == OSC_Algorithm::ALGO_PIN_PULSE)
      {
        if (base > 0.0f)
        {
          outputs[0][i] = 1.0f;
          outputs[1][i] = 1.0f;
        }
      }
    }

    void ProcessSamplesAccumulating(T** inputs, T** outputs, int nInputs, int nOutputs, int startIdx, int nFrames) override
    {
      mInputs[kVoiceControlTimbre].Write(mTimbreBuffer.Get(), startIdx, nFrames);

      switch (mMode)
      {
      case VoiceMode::kNoise1:
        for (auto i = startIdx; i < startIdx + nFrames; ++i)
          ProcessNoise1(outputs, i, mCachedNoiseThreshold);
        break;

      case VoiceMode::kNoise2:
        for (auto i = startIdx; i < startIdx + nFrames; ++i)
          ProcessNoise2(outputs, i, mCachedNoiseThreshold, mLastClock4, mTempNoise4);
        break;

      case VoiceMode::kNoise3:
        for (auto i = startIdx; i < startIdx + nFrames; ++i)
          ProcessNoise3(outputs, i, mCachedNoiseThreshold, mLastClock8, mTempNoise8);
        break;

      case VoiceMode::kSampler:
        for (auto i = startIdx; i < startIdx + nFrames; ++i)
          ProcessSampleOscillator(outputs, i, 440.0, &mSampleOsc);
        break;

      case VoiceMode::kSynth: {
        // Renamed to avoid WShadow compiler errors
        ADSREnvelope<T>& activePwmEnv = *(all_envs[mActiveOscId]);
        ADSREnvelope<T>& activePitchEnv = *(all_envs[mActiveOscId + 4]);

        double pitch = mInputs[kVoiceControlPitch].endValue;
        double pitchBend = mInputs[kVoiceControlPitchBend].endValue;
        double velocity = mInputs[kVoiceControlGate].endValue * 127.f;

        double pitchModStrength = pitchModStrengths[mActiveOscId];
        double pitchOffsetStrength = pitchOffsetStrengths[mActiveOscId];
        double pwmModStrength = pwmModStrengths[mActiveOscId];
        double pwmOffsetStrength = pwmOffsetStrengths[mActiveOscId];
        bool pwmKeyTrack = pwmKeyTracks[mActiveOscId];

        std::array<VaiaOscillator<T>, 8>* arr = nullptr;
        switch (mActiveOscId)
        {
        case 0:
          arr = &mUnisonOsc1;
          break;
        case 1:
          arr = &mUnisonOsc2;
          break;
        case 2:
          arr = &mUnisonOsc3;
          break;
        case 3:
          arr = &mUnisonOsc4;
          break;
        }

        int unison = std::clamp(extraUnisonCounts[mActiveOscId], 1, 8);
        double detuneRange = extraDetuneCents[mActiveOscId];

        for (auto i = startIdx; i < startIdx + nFrames; ++i)
        {
          auto pitch_value = activePitchEnv.Process(inputs[kModPitchSustainSmoother1 + mActiveOscId][i]) * pitchModStrength + pitchOffsetStrength;
          auto pwm_value = (activePwmEnv.Process(inputs[kModPwmSustainSmoother1 + mActiveOscId][i]) + mModWheel + inputs[kModPwmLFO1 + mActiveOscId][i]) * pwmModStrength + pwmOffsetStrength;

          double oscFreq = 440.0 * pow(2.0, pitch + pitchBend + inputs[kModPitchLFO1 + mActiveOscId][i] + pitch_value);
          double pwmFunc = pwmKeyTrack ? (pwm_value * (oscFreq / 440.0f)) : pwm_value;

          ProcessStandardOscillators(outputs, i, oscFreq, pwmFunc, velocity, unison, detuneRange, arr);
        }
        break;
      }
      }
    }

    void SetSampleRateAndBlockSize(double sampleRate, int blockSize) override
    {
      for (auto& o : mUnisonOsc1)
        o.SetSampleRate(sampleRate);
      for (auto& o : mUnisonOsc2)
        o.SetSampleRate(sampleRate);
      for (auto& o : mUnisonOsc3)
        o.SetSampleRate(sampleRate);
      for (auto& o : mUnisonOsc4)
        o.SetSampleRate(sampleRate);

      mSampleOsc.SetSampleRate(sampleRate);

      mPitchEnv1.SetSampleRate(sampleRate);
      mPitchEnv2.SetSampleRate(sampleRate);
      mPitchEnv3.SetSampleRate(sampleRate);
      mPitchEnv4.SetSampleRate(sampleRate);

      mPwmEnv1.SetSampleRate(sampleRate);
      mPwmEnv2.SetSampleRate(sampleRate);
      mPwmEnv3.SetSampleRate(sampleRate);
      mPwmEnv4.SetSampleRate(sampleRate);

      mTimbreBuffer.Resize(blockSize);
    }

    void SetControl(int controlNumber, float value) override
    {
      if (controlNumber == 1)
      {
        mModWheel = value;
      }
    }

    float Rand()
    {
      mRandSeed = mRandSeed * 0x0019660D + 0x3C6EF35F;
      uint32_t temp = ((mRandSeed >> 9) & 0x007FFFFF) | 0x3F800000;
      return (*reinterpret_cast<float*>(&temp)) * 2.f - 3.f;
    }
  };
};
