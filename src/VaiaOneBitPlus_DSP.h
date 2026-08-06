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

#include "SampleData.h"
#include "SampleOscillator.h"
#include "VaiaOneBitPlus.h"
#include "VaiaOscillator.h"

#include <algorithm>
#include <cstring>


constexpr size_t mNumOscPerVoice = 8;


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
  SampleData::SampleManager mSampleManager;


#pragma mark - Voice
  class Voice : public SynthVoice
  {
  private:
    OneBitPlusDSP& mParentDSP;

  public:
    enum class VoiceMode
    {
      kNoise1,
      kNoise2,
      kNoise3,
      kSampler,
      kSynth1,
      kSynth2,
      kSynth3,
      kSynth4
    };
    VoiceMode mMode = VoiceMode::kSynth1;

    double mCachedNoiseThreshold = 0.0;
    int mLastClock4 = -1;
    double mTempNoise4 = 0.0;
    int mLastClock8 = -1;
    double mTempNoise8 = 0.0;


    // Type 1
    std::array<VaiaOscillator<T>, mNumOscPerVoice> mUnisonOsc{}; // up to 8 unisons
    ADSREnvelope<T> mPwmEnv{};
    ADSREnvelope<T> mPitchEnv{};


    // sample osc
    SampleOscillator<T> mSampleOsc{};

    // lightweight debug counter to rate-limit logging inside the voice
    int mDebugSampleCounter = 0;


    bool pwmKeyTrack;
    double pwmModStrength;
    double pwmOffsetStrength;
    double pitchKeyTrackStrength;
    double pitchModStrength;
    double pitchOffsetStrength;

    // Unison controls per-voice (1..8) and detune in cents (0..100)
    int extraUnisonCount = 1;
    double extraDetuneCents = 0.0;


    OSC_Algorithm algo = used_algo;
    WDL_TypedBuf<float> mTimbreBuffer;
    double mModWheel{0.0};


    // noise generator for test
    uint32_t mRandSeed = 0;

    Voice(OneBitPlusDSP& parent)
      : mParentDSP(parent)
      , mPwmEnv("gain",
                [&]() {
                  for (auto& o : mUnisonOsc)
                    o.Reset();
                })
      , mPitchEnv("gain", [&]() {
        for (auto& o : mUnisonOsc)
          o.Reset();
      })


    {
    }

    bool GetBusy() const override
    {
      if (mMode == VoiceMode::kSynth1 || mMode == VoiceMode::kSynth2 || mMode == VoiceMode::kSynth3 || mMode == VoiceMode::kSynth4)
      {
        return mPwmEnv.GetBusy() || mPitchEnv.GetBusy();
      }
      return false;
    }

    void Trigger(double level, bool isRetrigger) override
    {
      for (auto& o : mUnisonOsc)
        o.Reset();

      mSampleOsc.Reset();
      auto sampleInfo = mParentDSP.mSampleManager.GetSampleForVelocity(level);
      mSampleOsc.BindSample(sampleInfo.data, sampleInfo.size);

      if (isRetrigger)
      {
        mPitchEnv.Retrigger(1);
        mPwmEnv.Retrigger(1);
      }
      else
      {
        mPitchEnv.Start(1);
        mPwmEnv.Start(1);
      }
    }

    void Release() override
    {
      mPitchEnv.Release();
      mPwmEnv.Release();
    }

    inline void ProcessNoise1(T** outputs, int i, double noiseThreshold)
    {
      double tempNoise = outputs[0][i] + (rand() % 255) > noiseThreshold ? -0.9 : 1.1;
      if (tempNoise < 0.0f)
        tempNoise = 0.0f;

      outputs[0][i] = outputs[1][i] = tempNoise;
    }

    inline void ProcessNoise2(T** outputs, int i, double noiseThreshold, int& lastClock4, double& tempNoise4)
    {
      int clock4 = i / 4;
      if (clock4 != lastClock4)
      {
        lastClock4 = clock4;
        double tempNoise = outputs[0][i] + (rand() % 255) > noiseThreshold ? -0.9 : 1.1;
        if (tempNoise < 0.0f)
          tempNoise = 0.0f;
        tempNoise4 = tempNoise;
      }
      outputs[0][i] = outputs[1][i] = tempNoise4;
    }

    inline void ProcessNoise3(T** outputs, int i, double noiseThreshold, int& lastClock8, double& tempNoise8)
    {
      int clock8 = i / 8;
      if (clock8 != lastClock8)
      {
        lastClock8 = clock8;
        double tempNoise = outputs[0][i] + (rand() % 255) > noiseThreshold ? -0.9 : 1.1;
        if (tempNoise < 0.0f)
          tempNoise = 0.0f;
        tempNoise8 = tempNoise;
      }
      outputs[0][i] = outputs[1][i] = tempNoise8;
    }

    inline void ProcessSampleOscillator(T** outputs, int i, double oscFreq, SampleOscillator<T>* sampleOsc)
    {
      if (sampleOsc && algo == OSC_Algorithm::ALGO_PIN_PULSE)
      {
        // SampleOscillator already returns 1.0 or -1.0 natively
        T sampleVal = sampleOsc->Process(440); // assume native A

        T outputVal = outputs[0][i];

        if (outputVal < 0.1f)
        {
          outputs[0][i] = std::max(outputs[0][i], sampleVal);
          outputs[1][i] = std::max(outputs[1][i], sampleVal);
        }
      }
    }

    inline void ProcessStandardOscillators(T** outputs, int i, double oscFreq, double pwmFunc, double velocity, int unison, double detuneRange)
    {
      bool anyHigh = false;

      for (int u = 0; u < unison; ++u)
      {
        double offsetCents = (unison == 1) ? 0.0 : (-detuneRange * 0.5 + (detuneRange * static_cast<double>(u)) / static_cast<double>(unison - 1));
        double freq = oscFreq * pow(2.0, offsetCents / 1200.0);

        auto& unisonOsc = mUnisonOsc[u];
        unisonOsc.SetPWM(pwmFunc);
        if (unisonOsc.Process(freq) > 0.0)
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
      double pitch = mInputs[kVoiceControlPitch].endValue;
      double pitchBend = mInputs[kVoiceControlPitchBend].endValue;
      double velocity = mInputs[kVoiceControlGate].endValue * 127.f;

      double midiNote = (pitch * 12.0) + 69.1;

      mInputs[kVoiceControlTimbre].Write(mTimbreBuffer.Get(), startIdx, nFrames);


      int velInt = static_cast<int>(std::round(velocity));
      int oscId = std::clamp((velInt > 0 ? velInt - 1 : 0) / 32, 0, 3);

      int unison = std::clamp(extraUnisonCount, 1, 8);
      double detuneRange = extraDetuneCents;

      double tempNoise4 = 0.0;
      double tempNoise8 = 0.0;
      int lastClock4 = -1;
      int lastClock8 = -1;

      for (auto i = startIdx; i < startIdx + nFrames; ++i)
      {
        auto pitchEnvVal = mPitchEnv.Process(inputs[kModPitchSustainSmoother1 + oscId][i]) * pitchModStrength + pitchOffsetStrength;
        auto pwmEnvVal = (mPwmEnv.Process(inputs[kModPwmSustainSmoother1 + oscId][i]) + mModWheel + inputs[kModPwmLFO1 + oscId][i]) * pwmModStrength + pwmOffsetStrength;

        double oscFreq = 440.0 * pow(2.0, pitch + pitchBend + inputs[kModPitchLFO1 + oscId][i] + pitchEnvVal);
        double pwmFunc = pwmKeyTrack ? (pwmEnvVal * (oscFreq / 440.0f)) : pwmEnvVal;

        /*
        if (++mDebugSampleCounter % 1024 == 0)
        {
          double envVal = mPwmEnv.Process(inputs[kModPwmSustainSmoother1 + oscId][i]);
          double lfoVal = inputs[kModPwmLFO1 + oscId][i];
          double modWheelVal = mModWheel;
          double modPow = pwmModStrength;
          double offsetVal = pwmOffsetStrength;
        }
        */

        double noiseThreshold = velocity * (1.0 - mModWheel);

        if (midiNote < 1 && velocity > 0.0)
        {
          ProcessNoise1(outputs, i, noiseThreshold);
        }
        else if (midiNote < 2 && velocity > 0.0)
        {
          ProcessNoise2(outputs, i, noiseThreshold, lastClock4, tempNoise4);
        }
        else if (midiNote < 3 && velocity > 0.0)
        {
          ProcessNoise3(outputs, i, noiseThreshold, lastClock8, tempNoise8);
        }
        else if (midiNote < 4 && velocity > 0.0)
        {
          ProcessSampleOscillator(outputs, i, oscFreq, &mSampleOsc);
        }
        else if (velocity > 0.0)
        {
          ProcessStandardOscillators(outputs, i, oscFreq, pwmFunc, velocity, unison, detuneRange);
        }
      }
    }


    void SetSampleRateAndBlockSize(double sampleRate, int blockSize) override
    {
      for (auto& o : mUnisonOsc)
        o.SetSampleRate(sampleRate);

      mSampleOsc.SetSampleRate(sampleRate);

      mPitchEnv.SetSampleRate(sampleRate);
      mPwmEnv.SetSampleRate(sampleRate);

      mTimbreBuffer.Resize(blockSize);
    }

    // this is called by the VoiceAllocator to set generic control values.
    void SetControl(int controlNumber, float value) override
    {
      if (controlNumber == 1)
      {
        mModWheel = value;
      }
    }


    // return single-precision floating point number on [-1, 1]
    float Rand()
    {
      mRandSeed = mRandSeed * 0x0019660D + 0x3C6EF35F;
      uint32_t temp = ((mRandSeed >> 9) & 0x007FFFFF) | 0x3F800000;
      return (*reinterpret_cast<float*>(&temp)) * 2.f - 3.f;
    }
  };

public:
#pragma mark -
  explicit OneBitPlusDSP(int nVoices)
  {
    for (auto i = 0; i < nVoices; i++)
    {
      // add a voice to Zone 0.
      mSynth.AddVoice(new Voice(*this), 0);
    }
  }

  void CentralizeLFO(T* pToCentralize, int nFrames, T levelScalar)
  {
    // Center the LFO buffer to have zero mean
    if (nFrames <= 0)
      return;
    T sum = (T)0;
    for (int s = 0; s < nFrames; ++s)
    {
      sum += pToCentralize[s];
    }
    T mean = sum / static_cast<T>(nFrames);
    if (mean != (T)0)
    {
      for (int s = 0; s < nFrames; ++s)
      {
        pToCentralize[s] -= mean;
      }
    }
  }

  
  void ProcessBlock(T** inputs, T** outputs, int nOutputs, int nFrames, double qnPos = 0., bool transportIsRunning = false, double tempo = 120.)
  {


    mParamSmoother.ProcessBlock(mParamsToSmooth.data(), mModulations.GetList(), nFrames);
    mPitchLFO1.ProcessBlock(mModulations.GetList()[kModPitchLFO1], nFrames, qnPos, transportIsRunning, tempo);
    mPwmLFO1.ProcessBlock(mModulations.GetList()[kModPwmLFO1], nFrames, qnPos, transportIsRunning, tempo);
    //CentralizeLFO(mModulations.GetList()[kModPitchLFO1], nFrames, LFO<T>::GetQNScalar(LFO<T>::k1));
    //CentralizeLFO(mModulations.GetList()[kModPwmLFO1], nFrames, LFO<T>::GetQNScalar(LFO<T>::k1));

    mPitchLFO2.ProcessBlock(mModulations.GetList()[kModPitchLFO2], nFrames, qnPos, transportIsRunning, tempo);
    mPwmLFO2.ProcessBlock(mModulations.GetList()[kModPwmLFO2], nFrames, qnPos, transportIsRunning, tempo);
    //CentralizeLFO(mModulations.GetList()[kModPitchLFO2], nFrames, LFO<T>::GetQNScalar(LFO<T>::k1));
    //CentralizeLFO(mModulations.GetList()[kModPwmLFO2], nFrames, LFO<T>::GetQNScalar(LFO<T>::k1));

    mPitchLFO3.ProcessBlock(mModulations.GetList()[kModPitchLFO3], nFrames, qnPos, transportIsRunning, tempo);
    mPwmLFO3.ProcessBlock(mModulations.GetList()[kModPwmLFO3], nFrames, qnPos, transportIsRunning, tempo);
    //CentralizeLFO(mModulations.GetList()[kModPitchLFO3], nFrames, LFO<T>::GetQNScalar(LFO<T>::k1));
    //CentralizeLFO(mModulations.GetList()[kModPwmLFO3], nFrames, LFO<T>::GetQNScalar(LFO<T>::k1));

    mPitchLFO4.ProcessBlock(mModulations.GetList()[kModPitchLFO4], nFrames, qnPos, transportIsRunning, tempo);
    mPwmLFO4.ProcessBlock(mModulations.GetList()[kModPwmLFO4], nFrames, qnPos, transportIsRunning, tempo);
   // CentralizeLFO(mModulations.GetList()[kModPitchLFO4], nFrames, LFO<T>::GetQNScalar(LFO<T>::k1));
   // CentralizeLFO(mModulations.GetList()[kModPwmLFO4], nFrames, LFO<T>::GetQNScalar(LFO<T>::k1));

    mSynth.ProcessBlock(mModulations.GetList(), outputs, 2, nOutputs, nFrames);

    double eps = 0.001;


    switch (algo)
    {
    case OSC_Algorithm::ALGO_PIN_PULSE:
      for (int s = 0; s < nFrames; s++)
      {
        T smoothedGain = mModulations.GetList()[kModGainSmoother][s];

        if (abs(outputs[0][s]) < eps)
        {
          outputs[0][s] = outputs[1][s] = 0.0;
        }
        else if (outputs[0][s] < 0)
        {
          outputs[0][s] = -1.0 * smoothedGain;
          outputs[1][s] = -1.0 * smoothedGain;
        }
        else if (outputs[0][s] > 0)
        {
          outputs[0][s] = 1.0 * smoothedGain;
          outputs[1][s] = 1.0 * smoothedGain;
        }
      }
    }
  }

  void Reset(double sampleRate, int blockSize)
  {
    mSynth.SetSampleRateAndBlockSize(sampleRate, blockSize);
    mSynth.Reset();

    mPwmLFO1.SetSampleRate(sampleRate);
    mPitchLFO1.SetSampleRate(sampleRate);

    mPwmLFO2.SetSampleRate(sampleRate);
    mPitchLFO2.SetSampleRate(sampleRate);

    mPwmLFO3.SetSampleRate(sampleRate);
    mPitchLFO3.SetSampleRate(sampleRate);

    mPwmLFO4.SetSampleRate(sampleRate);
    mPitchLFO4.SetSampleRate(sampleRate);

    mModulationsData.Resize(blockSize * kNumModulations);
    mModulations.Empty();

    for (int i = 0; i < kNumModulations; i++)
    {
      mModulations.Add(mModulationsData.Get() + (blockSize * i));
    }
  }

  void ProcessMidiMsg(const IMidiMsg& msg) { mSynth.AddMidiMsgToQueue(msg); }

  void SetParam(int paramIdx, double value)
  {
    using EEnvStage = ADSREnvelope<sample>::EStage;

    switch (paramIdx)
    {

    case kParamNoteGlideTime:
      mSynth.SetNoteGlideTime(value / 1000.);
      break;
    case kParamGain:
      mParamsToSmooth[kModGainSmoother] = (T)value / 100.;
      break;


    // OSC 1
    case kParamPwmSustain1:
      mParamsToSmooth[kModPwmSustainSmoother1] = (T)value / 100.;
      break;
    case kParamPwmAttack1:
    case kParamPwmDecay1:
    case kParamPwmRelease1: {
      EEnvStage stage = static_cast<EEnvStage>(EEnvStage::kAttack + (paramIdx - kParamPwmAttack1));

      mSynth.ForEachVoice([stage, value](SynthVoice& synthVoice) {
        auto& voice = dynamic_cast<OneBitPlusDSP::Voice&>(synthVoice);

        // Only apply to voices currently in kSynth1 mode
        if (voice.mMode == Voice::VoiceMode::kSynth1)
        {
          voice.mPwmEnv.SetStageTime(stage, value);
        }
      });
      break;
    }
    case kParamPwmLFODepth1:
      mPwmLFO1.SetScalar(value / 100.);
      break;
    case kParamPwmLFORateTempo1:
      mPwmLFO1.SetQNScalarFromDivision(static_cast<int>(value));
      break;
    case kParamPwmLFORateHz1:
      mPwmLFO1.SetFreqCPS(value);
      break;
    case kParamPwmLFORateMode1:
      mPwmLFO1.SetRateMode(value > 0.5);
      break;
    case kParamPwmLFOShape1:
      mPwmLFO1.SetShape(static_cast<int>(value));
      break;

    case kParamPwmModPow1:
      mSynth.ForEachVoice([value](SynthVoice& synthVoice) {
        auto& voice = dynamic_cast<OneBitPlusDSP::Voice&>(synthVoice);

        if (voice.mMode == Voice::VoiceMode::kSynth1)
        {
          voice.pwmModStrength = value;
        }
      });

      break;
    case kParamPwmOffset1:
      mSynth.ForEachVoice([value](SynthVoice& synthVoice) {
        auto& voice = dynamic_cast<OneBitPlusDSP::Voice&>(synthVoice);

        if (voice.mMode == Voice::VoiceMode::kSynth1)
        {
          voice.pwmOffsetStrength = value;
        }
      });
      break;
    case kParamPwmKeyTrack1:
      mSynth.ForEachVoice([value](SynthVoice& synthVoice) {
        auto& voice = dynamic_cast<OneBitPlusDSP::Voice&>(synthVoice);

        if (voice.mMode == Voice::VoiceMode::kSynth1)
        {
          voice.pwmKeyTrack = value;
        }
      });
      break;
    case kParamPitchModPow1:
      mSynth.ForEachVoice([value](SynthVoice& synthVoice) {
        auto& voice = dynamic_cast<OneBitPlusDSP::Voice&>(synthVoice);

        if (voice.mMode == Voice::VoiceMode::kSynth1)
        {
          voice.pitchModStrength = value;
        }
      });

      break;
    case kParamPitchOffset1:
      mSynth.ForEachVoice([value](SynthVoice& synthVoice) {
        auto& voice = dynamic_cast<OneBitPlusDSP::Voice&>(synthVoice);

        if (voice.mMode == Voice::VoiceMode::kSynth1)
        {
          voice.pitchOffsetStrength = value;
        }
      });
      break;
    case kParamPitchKeyTrack1:
      mSynth.ForEachVoice([value](SynthVoice& synthVoice) {
        auto& voice = dynamic_cast<OneBitPlusDSP::Voice&>(synthVoice);

        if (voice.mMode == Voice::VoiceMode::kSynth1)
        {
          voice.pitchKeyTrackStrength = value;
        }
      });
      break;
    case kParamPitchSustain1:
      mParamsToSmooth[kModPitchSustainSmoother1] = (T)value / 100.;
      break;
    case kParamPitchAttack1:
    case kParamPitchDecay1:
    case kParamPitchRelease1: {
      EEnvStage stage = static_cast<EEnvStage>(EEnvStage::kAttack + (paramIdx - kParamPitchAttack1));

      mSynth.ForEachVoice([stage, value](SynthVoice& synthVoice) {
        auto& voice = dynamic_cast<OneBitPlusDSP::Voice&>(synthVoice);

        // Only apply to voices currently in kSynth1 mode
        if (voice.mMode == Voice::VoiceMode::kSynth1)
        {
          voice.mPitchEnv.SetStageTime(stage, value);
        }
      });
      break;
    }
    case kParamPitchLFODepth1:
      mPitchLFO1.SetScalar(value / 100.);
      break;
    case kParamPitchLFORateTempo1:
      mPitchLFO1.SetQNScalarFromDivision(static_cast<int>(value));
      break;
    case kParamPitchLFORateHz1:
      mPitchLFO1.SetFreqCPS(value);
      break;
    case kParamPitchLFORateMode1:
      mPitchLFO1.SetRateMode(value > 0.5);
      break;
    case kParamPitchLFOShape1:
      mPitchLFO1.SetShape(static_cast<int>(value));
      break;


      // OSC 2
    case kParamPwmSustain2:
      mParamsToSmooth[kModPwmSustainSmoother2] = (T)value / 100.;
      break;
    case kParamPwmAttack2:
    case kParamPwmDecay2:
    case kParamPwmRelease2: {
      EEnvStage stage = static_cast<EEnvStage>(EEnvStage::kAttack + (paramIdx - kParamPwmAttack2));

      mSynth.ForEachVoice([stage, value](SynthVoice& synthVoice) {
        auto& voice = dynamic_cast<OneBitPlusDSP::Voice&>(synthVoice);

        // Only apply to voices currently in kSynth2 mode
        if (voice.mMode == Voice::VoiceMode::kSynth2)
        {
          voice.mPwmEnv.SetStageTime(stage, value);
        }
      });
      break;
    }
    case kParamPwmLFODepth2:
      mPwmLFO2.SetScalar(value / 100.);
      break;
    case kParamPwmLFORateTempo2:
      mPwmLFO2.SetQNScalarFromDivision(static_cast<int>(value));
      break;
    case kParamPwmLFORateHz2:
      mPwmLFO2.SetFreqCPS(value);
      break;
    case kParamPwmLFORateMode2:
      mPwmLFO2.SetRateMode(value > 0.5);
      break;
    case kParamPwmLFOShape2:
      mPwmLFO2.SetShape(static_cast<int>(value));
      break;

    case kParamPwmModPow2:
      mSynth.ForEachVoice([value](SynthVoice& synthVoice) {
        auto& voice = dynamic_cast<OneBitPlusDSP::Voice&>(synthVoice);

        if (voice.mMode == Voice::VoiceMode::kSynth2)
        {
          voice.pwmModStrength = value;
        }
      });

      break;
    case kParamPwmOffset2:
      mSynth.ForEachVoice([value](SynthVoice& synthVoice) {
        auto& voice = dynamic_cast<OneBitPlusDSP::Voice&>(synthVoice);

        if (voice.mMode == Voice::VoiceMode::kSynth2)
        {
          voice.pwmOffsetStrength = value;
        }
      });
      break;
    case kParamPwmKeyTrack2:
      mSynth.ForEachVoice([value](SynthVoice& synthVoice) {
        auto& voice = dynamic_cast<OneBitPlusDSP::Voice&>(synthVoice);

        if (voice.mMode == Voice::VoiceMode::kSynth2)
        {
          voice.pwmKeyTrack = value;
        }
      });
      break;

    case kParamPitchModPow2:
      mSynth.ForEachVoice([value](SynthVoice& synthVoice) {
        auto& voice = dynamic_cast<OneBitPlusDSP::Voice&>(synthVoice);

        if (voice.mMode == Voice::VoiceMode::kSynth2)
        {
          voice.pitchModStrength = value;
        }
      });

      break;
    case kParamPitchOffset2:
      mSynth.ForEachVoice([value](SynthVoice& synthVoice) {
        auto& voice = dynamic_cast<OneBitPlusDSP::Voice&>(synthVoice);

        if (voice.mMode == Voice::VoiceMode::kSynth2)
        {
          voice.pitchOffsetStrength = value;
        }
      });
      break;
    case kParamPitchKeyTrack2:
      mSynth.ForEachVoice([value](SynthVoice& synthVoice) {
        auto& voice = dynamic_cast<OneBitPlusDSP::Voice&>(synthVoice);

        if (voice.mMode == Voice::VoiceMode::kSynth2)
        {
          voice.pitchKeyTrackStrength = value;
        }
      });
      break;

    case kParamPitchSustain2:
      mParamsToSmooth[kModPitchSustainSmoother2] = (T)value / 100.;
      break;
    case kParamPitchAttack2:
    case kParamPitchDecay2:
    case kParamPitchRelease2: {
      EEnvStage stage = static_cast<EEnvStage>(EEnvStage::kAttack + (paramIdx - kParamPitchAttack2));

      mSynth.ForEachVoice([stage, value](SynthVoice& synthVoice) {
        auto& voice = dynamic_cast<OneBitPlusDSP::Voice&>(synthVoice);

        // Only apply to voices currently in kSynth2 mode
        if (voice.mMode == Voice::VoiceMode::kSynth2)
        {
          voice.mPitchEnv.SetStageTime(stage, value);
        }
      });
      break;
    }
    case kParamPitchLFODepth2:
      mPitchLFO2.SetScalar(value / 100.);
      break;
    case kParamPitchLFORateTempo2:
      mPitchLFO2.SetQNScalarFromDivision(static_cast<int>(value));
      break;
    case kParamPitchLFORateHz2:
      mPitchLFO2.SetFreqCPS(value);
      break;
    case kParamPitchLFORateMode2:
      mPitchLFO2.SetRateMode(value > 0.5);
      break;
    case kParamPitchLFOShape2:
      mPitchLFO2.SetShape(static_cast<int>(value));
      break;

      // OSC 3
    case kParamPwmSustain3:
      mParamsToSmooth[kModPwmSustainSmoother3] = (T)value / 100.;
      break;
    case kParamPwmAttack3:
    case kParamPwmDecay3:
    case kParamPwmRelease3: {
      EEnvStage stage = static_cast<EEnvStage>(EEnvStage::kAttack + (paramIdx - kParamPwmAttack3));

      mSynth.ForEachVoice([stage, value](SynthVoice& synthVoice) {
        auto& voice = dynamic_cast<OneBitPlusDSP::Voice&>(synthVoice);

        // Only apply to voices currently in kSynth3 mode
        if (voice.mMode == Voice::VoiceMode::kSynth3)
        {
          voice.mPwmEnv.SetStageTime(stage, value);
        }
      });
      break;
    }
    case kParamPwmLFODepth3:
      mPwmLFO3.SetScalar(value / 100.);
      break;
    case kParamPwmLFORateTempo3:
      mPwmLFO3.SetQNScalarFromDivision(static_cast<int>(value));
      break;
    case kParamPwmLFORateHz3:
      mPwmLFO3.SetFreqCPS(value);
      break;
    case kParamPwmLFORateMode3:
      mPwmLFO3.SetRateMode(value > 0.5);
      break;
    case kParamPwmLFOShape3:
      mPwmLFO3.SetShape(static_cast<int>(value));
      break;
    case kParamPwmModPow3:
      mSynth.ForEachVoice([value](SynthVoice& synthVoice) {
        auto& voice = dynamic_cast<OneBitPlusDSP::Voice&>(synthVoice);

        if (voice.mMode == Voice::VoiceMode::kSynth3)
        {
          voice.pwmModStrength = value;
        }
      });

      break;
    case kParamPwmOffset3:
      mSynth.ForEachVoice([value](SynthVoice& synthVoice) {
        auto& voice = dynamic_cast<OneBitPlusDSP::Voice&>(synthVoice);

        if (voice.mMode == Voice::VoiceMode::kSynth3)
        {
          voice.pwmOffsetStrength = value;
        }
      });
      break;
    case kParamPwmKeyTrack3:
      mSynth.ForEachVoice([value](SynthVoice& synthVoice) {
        auto& voice = dynamic_cast<OneBitPlusDSP::Voice&>(synthVoice);

        if (voice.mMode == Voice::VoiceMode::kSynth3)
        {
          voice.pwmKeyTrack = value;
        }
      });
      break;

    case kParamPitchSustain3:
      mParamsToSmooth[kModPitchSustainSmoother3] = (T)value / 100.;
      break;
    case kParamPitchAttack3:
    case kParamPitchDecay3:
    case kParamPitchRelease3: {
      EEnvStage stage = static_cast<EEnvStage>(EEnvStage::kAttack + (paramIdx - kParamPitchAttack3));

      mSynth.ForEachVoice([stage, value](SynthVoice& synthVoice) {
        auto& voice = dynamic_cast<OneBitPlusDSP::Voice&>(synthVoice);

        // Only apply to voices currently in kSynth3 mode
        if (voice.mMode == Voice::VoiceMode::kSynth3)
        {
          voice.mPitchEnv.SetStageTime(stage, value);
        }
      });
      break;
    }
    case kParamPitchLFODepth3:
      mPitchLFO3.SetScalar(value / 100.);
      break;
    case kParamPitchLFORateTempo3:
      mPitchLFO3.SetQNScalarFromDivision(static_cast<int>(value));
      break;
    case kParamPitchLFORateHz3:
      mPitchLFO3.SetFreqCPS(value);
      break;
    case kParamPitchLFORateMode3:
      mPitchLFO3.SetRateMode(value > 0.5);
      break;
    case kParamPitchLFOShape3:
      mPitchLFO3.SetShape(static_cast<int>(value));
      break;
    case kParamPitchModPow3:
      mSynth.ForEachVoice([value](SynthVoice& synthVoice) {
        auto& voice = dynamic_cast<OneBitPlusDSP::Voice&>(synthVoice);

        if (voice.mMode == Voice::VoiceMode::kSynth3)
        {
          voice.pitchModStrength = value;
        }
      });

      break;
    case kParamPitchOffset3:
      mSynth.ForEachVoice([value](SynthVoice& synthVoice) {
        auto& voice = dynamic_cast<OneBitPlusDSP::Voice&>(synthVoice);

        if (voice.mMode == Voice::VoiceMode::kSynth3)
        {
          voice.pitchOffsetStrength = value;
        }
      });
      break;
    case kParamPitchKeyTrack3:
      mSynth.ForEachVoice([value](SynthVoice& synthVoice) {
        auto& voice = dynamic_cast<OneBitPlusDSP::Voice&>(synthVoice);

        if (voice.mMode == Voice::VoiceMode::kSynth3)
        {
          voice.pitchKeyTrackStrength = value;
        }
      });
      break;
      // OSC 4
    case kParamPwmSustain4:
      mParamsToSmooth[kModPwmSustainSmoother4] = (T)value / 100.;
      break;
    case kParamPwmAttack4:
    case kParamPwmDecay4:
    case kParamPwmRelease4: {
      EEnvStage stage = static_cast<EEnvStage>(EEnvStage::kAttack + (paramIdx - kParamPwmAttack4));

      mSynth.ForEachVoice([stage, value](SynthVoice& synthVoice) {
        auto& voice = dynamic_cast<OneBitPlusDSP::Voice&>(synthVoice);

        // Only apply to voices currently in kSynth4 mode
        if (voice.mMode == Voice::VoiceMode::kSynth4)
        {
          voice.mPwmEnv.SetStageTime(stage, value);
        }
      });
      break;
    }
    case kParamPwmLFODepth4:
      mPwmLFO4.SetScalar(value / 100.);
      break;
    case kParamPwmLFORateTempo4:
      mPwmLFO4.SetQNScalarFromDivision(static_cast<int>(value));
      break;
    case kParamPwmLFORateHz4:
      mPwmLFO4.SetFreqCPS(value);
      break;
    case kParamPwmLFORateMode4:
      mPwmLFO4.SetRateMode(value > 0.5);
      break;
    case kParamPwmLFOShape4:
      mPwmLFO4.SetShape(static_cast<int>(value));
      break;
    case kParamPwmModPow4:
      mSynth.ForEachVoice([value](SynthVoice& synthVoice) {
        auto& voice = dynamic_cast<OneBitPlusDSP::Voice&>(synthVoice);

        if (voice.mMode == Voice::VoiceMode::kSynth4)
        {
          voice.pwmModStrength = value;
        }
      });

      break;
    case kParamPwmOffset4:
      mSynth.ForEachVoice([value](SynthVoice& synthVoice) {
        auto& voice = dynamic_cast<OneBitPlusDSP::Voice&>(synthVoice);

        if (voice.mMode == Voice::VoiceMode::kSynth4)
        {
          voice.pwmOffsetStrength = value;
        }
      });
      break;
    case kParamPwmKeyTrack4:
      mSynth.ForEachVoice([value](SynthVoice& synthVoice) {
        auto& voice = dynamic_cast<OneBitPlusDSP::Voice&>(synthVoice);

        if (voice.mMode == Voice::VoiceMode::kSynth4)
        {
          voice.pwmKeyTrack = value;
        }
      });
      break;

    case kParamPitchSustain4:
      mParamsToSmooth[kModPitchSustainSmoother4] = (T)value / 100.;
      break;
    case kParamPitchAttack4:
    case kParamPitchDecay4:
    case kParamPitchRelease4: {
      EEnvStage stage = static_cast<EEnvStage>(EEnvStage::kAttack + (paramIdx - kParamPitchAttack4));

      mSynth.ForEachVoice([stage, value](SynthVoice& synthVoice) {
        auto& voice = dynamic_cast<OneBitPlusDSP::Voice&>(synthVoice);

        // Only apply to voices currently in kSynth4 mode
        if (voice.mMode == Voice::VoiceMode::kSynth4)
        {
          voice.mPitchEnv.SetStageTime(stage, value);
        }
      });
      break;
    }
    case kParamPitchLFODepth4:
      mPitchLFO4.SetScalar(value / 100.);
      break;
    case kParamPitchLFORateTempo4:
      mPitchLFO4.SetQNScalarFromDivision(static_cast<int>(value));
      break;
    case kParamPitchLFORateHz4:
      mPitchLFO4.SetFreqCPS(value);
      break;
    case kParamPitchLFORateMode4:
      mPitchLFO4.SetRateMode(value > 0.5);
      break;
    case kParamPitchLFOShape4:
      mPitchLFO4.SetShape(static_cast<int>(value));
      break;
    case kParamPitchModPow4:
      mSynth.ForEachVoice([value](SynthVoice& synthVoice) {
        auto& voice = dynamic_cast<OneBitPlusDSP::Voice&>(synthVoice);

        if (voice.mMode == Voice::VoiceMode::kSynth4)
        {
          voice.pitchModStrength = value;
        }
      });

      break;
    case kParamPitchOffset4:
      mSynth.ForEachVoice([value](SynthVoice& synthVoice) {
        auto& voice = dynamic_cast<OneBitPlusDSP::Voice&>(synthVoice);

        if (voice.mMode == Voice::VoiceMode::kSynth4)
        {
          voice.pitchOffsetStrength = value;
        }
      });
      break;
    case kParamPitchKeyTrack4:
      mSynth.ForEachVoice([value](SynthVoice& synthVoice) {
        auto& voice = dynamic_cast<OneBitPlusDSP::Voice&>(synthVoice);

        if (voice.mMode == Voice::VoiceMode::kSynth4)
        {
          voice.pitchKeyTrackStrength = value;
        }
      });
      break;
    case kParamExtraUnison1:
      mSynth.ForEachVoice([value](SynthVoice& synthVoice) {
        auto& voice = dynamic_cast<OneBitPlusDSP::Voice&>(synthVoice);

        if (voice.mMode == Voice::VoiceMode::kSynth1)
        {


          int v = static_cast<int>(value);
          if (v < 1)
            v = 1;
          if (v > 8)
            v = 8;
          voice.extraUnisonCount = v;
        }
      });


      break;
    case kParamExtraDetune1:
      mSynth.ForEachVoice([value](SynthVoice& synthVoice) {
        auto& voice = dynamic_cast<OneBitPlusDSP::Voice&>(synthVoice);
        if (voice.mMode == Voice::VoiceMode::kSynth1)
        {

          double v = value;
          if (v < 0.0)
            v = 0.0;
          if (v > 100.0)
            v = 100.0;
          voice.extraDetuneCents = v;
        }
      });

      break;
    case kParamExtraUnison2:
      mSynth.ForEachVoice([value](SynthVoice& synthVoice) {
        auto& voice = dynamic_cast<OneBitPlusDSP::Voice&>(synthVoice);

        if (voice.mMode == Voice::VoiceMode::kSynth2)
        {

          int v = static_cast<int>(value);
          if (v < 1)
            v = 1;
          if (v > 8)
            v = 8;
          voice.extraUnisonCount = v;
        }
      });

      break;
    case kParamExtraDetune2:
      mSynth.ForEachVoice([value](SynthVoice& synthVoice) {
        auto& voice = dynamic_cast<OneBitPlusDSP::Voice&>(synthVoice);

        if (voice.mMode == Voice::VoiceMode::kSynth2)
        {

          double v = value;
          if (v < 0.0)
            v = 0.0;
          if (v > 100.0)
            v = 100.0;
          voice.extraDetuneCents = v;
        }
      });

      break;
    case kParamExtraUnison3:
      mSynth.ForEachVoice([value](SynthVoice& synthVoice) {
        auto& voice = dynamic_cast<OneBitPlusDSP::Voice&>(synthVoice);

        if (voice.mMode == Voice::VoiceMode::kSynth3)
        {

          int v = static_cast<int>(value);
          if (v < 1)
            v = 1;
          if (v > 8)
            v = 8;
          voice.extraUnisonCount = v;
        }
      });

      break;
    case kParamExtraDetune3:
      mSynth.ForEachVoice([value](SynthVoice& synthVoice) {
        auto& voice = dynamic_cast<OneBitPlusDSP::Voice&>(synthVoice);

        if (voice.mMode == Voice::VoiceMode::kSynth3)
        {

          double v = value;
          if (v < 0.0)
            v = 0.0;
          if (v > 100.0)
            v = 100.0;
          voice.extraDetuneCents = v;
        }
      });

      break;
    case kParamExtraUnison4:
      mSynth.ForEachVoice([value](SynthVoice& synthVoice) {
        auto& voice = dynamic_cast<OneBitPlusDSP::Voice&>(synthVoice);

        if (voice.mMode == Voice::VoiceMode::kSynth4)
        {

          int v = static_cast<int>(value);
          if (v < 1)
            v = 1;
          if (v > 8)
            v = 8;
          voice.extraUnisonCount = v;
        }
      });

      break;
    case kParamExtraDetune4:
      mSynth.ForEachVoice([value](SynthVoice& synthVoice) {
        auto& voice = dynamic_cast<OneBitPlusDSP::Voice&>(synthVoice);

        if (voice.mMode == Voice::VoiceMode::kSynth4)
        {

          double v = value;
          if (v < 0.0)
            v = 0.0;
          if (v > 100.0)
            v = 100.0;
          voice.extraDetuneCents = v;
        }
      });

      break;
      // DEFAULT
    default:
      break;
    }
  }

public:
  OSC_Algorithm algo = used_algo;

  MidiSynth mSynth{VoiceAllocator::kPolyModePoly, MidiSynth::kDefaultBlockSize};
  WDL_TypedBuf<T> mModulationsData; // Sample data for global modulations (e.g. smoothed sustain)
  WDL_PtrList<T> mModulations;      // Ptrlist for global modulations
  LogParamSmooth<T, kNumModulations> mParamSmoother;
  std::array<sample, kNumModulations> mParamsToSmooth{};
  LFO<T> mPitchLFO1;
  LFO<T> mPwmLFO1;

  LFO<T> mPitchLFO2;
  LFO<T> mPwmLFO2;

  LFO<T> mPitchLFO3;
  LFO<T> mPwmLFO3;

  LFO<T> mPitchLFO4;
  LFO<T> mPwmLFO4;
};
