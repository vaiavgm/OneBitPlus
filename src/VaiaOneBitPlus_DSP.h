#pragma once

#define MY_PRINTF(...) {char buf[512]; sprintf(buf, __VA_ARGS__);  OutputDebugString(buf);}

#include <IPlugLogger.h>
#include <IPlugMidi.h>

#include <heapbuf.h>
#include <ptrlist.h>
#include <stdlib.h>
#include <array>
#include <SynthVoice.h>
#include <VoiceAllocator.h>
#include <IPlugConstants.h>


#include <MidiSynth.h>
#include <ADSREnvelope.h>
#include <Smoothers.h>
#include <LFO.h>
#include <cstdint>

#include "VaiaOneBitPlus.h"
#include "VaiaOscillator.h"


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
  ALGO_FLIP_ONE, //This one is not quite correct, as it outputs some zeroes where it shouldn't
  ALGO_MOD_TWO,
  ALGO_PIN_PULSE, // just add and clamp
};

const OSC_Algorithm used_algo = OSC_Algorithm::ALGO_PIN_PULSE;

template<typename T>
class OneBitPlusDSP
{
public:
#pragma mark - Voice
  class Voice : public SynthVoice
  {

  public:
    Voice() : mPwmEnv1("gain", [&]() {  for (auto &o : mUnisonOsc1) o.Reset(); }), mPitchEnv1("gain", [&]() { for (auto &o : mUnisonOsc1) o.Reset(); }),
      mPwmEnv2("gain", [&]() {  for (auto &o : mUnisonOsc2) o.Reset(); }), mPitchEnv2("gain", [&]() {  for (auto &o : mUnisonOsc2) o.Reset(); }),
      mPwmEnv3("gain", [&]() {  for (auto &o : mUnisonOsc3) o.Reset(); }), mPitchEnv3("gain", [&]() {  for (auto &o : mUnisonOsc3) o.Reset(); }),
      mPwmEnv4("gain", [&]() {  for (auto &o : mUnisonOsc4) o.Reset(); }), mPitchEnv4("gain", [&]() {  for (auto &o : mUnisonOsc4) o.Reset(); })
    {
      for (auto &c : extraUnisonCounts) c = 1;
      for (auto &d : extraDetuneCents) d = 0.0;
    }

    bool GetBusy() const override
    {

      return mPwmEnv1.GetBusy();

      //  mPitchEnv1.GetBusy() ||
      //  mPitchEnv2.GetBusy() ||
      //  mPitchEnv3.GetBusy() ||
      //  mPitchEnv4.GetBusy() ||
      //  mPwmEnv1.GetBusy() ||
      //  mPwmEnv2.GetBusy() ||
      //  mPwmEnv3.GetBusy() ||
      //  mPwmEnv4.GetBusy();

    }

    void Trigger(double level, bool isRetrigger) override
    {
      for (auto &o : mUnisonOsc1) o.Reset();
      for (auto &o : mUnisonOsc2) o.Reset();
      for (auto &o : mUnisonOsc3) o.Reset();
      for (auto &o : mUnisonOsc4) o.Reset();


      if (isRetrigger)
      {
        mPitchEnv1.Retrigger(1);
        mPitchEnv2.Retrigger(1);
        mPitchEnv3.Retrigger(1);
        mPitchEnv4.Retrigger(1);
        mPwmEnv1.Retrigger(1);
        mPwmEnv2.Retrigger(1);
        mPwmEnv3.Retrigger(1);
        mPwmEnv4.Retrigger(1);
      }
      else
      {
        mPitchEnv1.Start(1);
        mPitchEnv2.Start(1);
        mPitchEnv3.Start(1);
        mPitchEnv4.Start(1);
        mPwmEnv1.Start(1);
        mPwmEnv2.Start(1);
        mPwmEnv3.Start(1);
        mPwmEnv4.Start(1);
      }
    }

    void Release() override
    {
      mPitchEnv1.Release();
      mPitchEnv2.Release();
      mPitchEnv3.Release();
      mPitchEnv4.Release();
      mPwmEnv1.Release();
      mPwmEnv2.Release();
      mPwmEnv3.Release();
      mPwmEnv4.Release();
    }


void ProcessSamplesAccumulating(T** inputs, T** outputs, int nInputs, int nOutputs, int startIdx, int nFrames) override
    {
      double pitch = mInputs[kVoiceControlPitch].endValue;
      double pitchBend = mInputs[kVoiceControlPitchBend].endValue;
      double velocity = mInputs[kVoiceControlGate].endValue * 127.f;

      mInputs[kVoiceControlTimbre].Write(mTimbreBuffer.Get(), startIdx, nFrames);

      int oscId = (velocity + 1) * 4 / 129; // Assuming 4 core oscillator groups
      if (oscId > 3)
        oscId = 3;

      ADSREnvelope<T>& mPwmEnv = *(all_envs.at(oscId));
      ADSREnvelope<T>& mPitchEnv = *(all_envs.at(oscId + 4));

      double pitchModStrength = pitchModStrengths[oscId];
      double pitchOffsetStrength = pitchOffsetStrengths[oscId];
      double pwmModStrength = pwmModStrengths[oscId];
      double pwmOffsetStrength = pwmOffsetStrengths[oscId];
      bool pwmKeyTrack = pwmKeyTracks[oscId];

      // Cache pointer to the selected unison group array
      std::array<VaiaOscillator<T>, 8>* arr = nullptr;
      switch (oscId)
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
      default:
        arr = &mUnisonOsc4;
        break;
      }

      int unison = extraUnisonCounts[oscId];
      if (unison < 1)
        unison = 1;
      if (unison > 8)
        unison = 8;

      double detuneRange = extraDetuneCents[oscId];

      for (auto i = startIdx; i < startIdx + nFrames; ++i)
      {
        auto pitch_value = mPitchEnv.Process(inputs[kModPitchSustainSmoother1 + oscId][i]) * pitchModStrength + pitchOffsetStrength;
        auto pwm_value = (mPwmEnv.Process(inputs[kModPwmSustainSmoother1 + oscId][i]) + mModWheel + inputs[kModPwmLFO1 + oscId][i]) * pwmModStrength + pwmOffsetStrength;

        double oscFreq = 440. * pow(2., pitch + pitchBend + inputs[kModPitchLFO1 + oscId][0] + pitch_value);
        double pwmFunc = (pwmKeyTrack ? pwm_value * (oscFreq / 440.0f) : pwm_value);

        // Restored original low-pitch noise mappings
        if (pitch < -5.74 && velocity > 1.0)
        {
          double tempNoise = outputs[0][i] + (rand() % 255) > (velocity * (1 - mModWheel)) ? -0.9 : 1.1;
          if (tempNoise < 0.0f)
            tempNoise = 0.0f;
          outputs[0][i] = outputs[1][i] = tempNoise;
        }
        else if (pitch < -5.66 && velocity > 1.0)
        {
          if (i > startIdx && i % 4 == 3)
          {
            double tempNoise = outputs[0][i] + (rand() % 255) > (velocity * (1 - mModWheel)) ? -0.9 : 1.1;
            if (tempNoise < 0.0f)
              tempNoise = 0.0f;
            outputs[0][i] = outputs[1][i] = outputs[0][i - 1] = outputs[1][i - 1] = outputs[0][i - 2] = outputs[1][i - 2] = outputs[0][i - 3] = outputs[1][i - 3] = tempNoise;
          }
        }
        else if (pitch < -5.58 && velocity > 1.0)
        {
          if (i > startIdx && i % 8 == 7)
          {
            double tempNoise = outputs[0][i] + (rand() % 255) > (velocity * (1 - mModWheel)) ? -0.9 : 1.1;
            if (tempNoise < 0.0f)
              tempNoise = 0.0f;
            outputs[0][i] = outputs[1][i] = outputs[0][i - 1] = outputs[1][i - 1] = outputs[0][i - 2] = outputs[1][i - 2] = outputs[0][i - 3] = outputs[1][i - 3] = outputs[0][i - 4] =
              outputs[1][i - 4] = outputs[0][i - 5] = outputs[1][i - 5] = outputs[0][i - 6] = outputs[1][i - 6] = outputs[0][i - 7] = outputs[1][i - 7] = outputs[0][i] = outputs[1][i] = tempNoise;
          }
        }
        else
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
      }
    }

    void SetSampleRateAndBlockSize(double sampleRate, int blockSize) override
    {

      for (auto &o : mUnisonOsc1) o.SetSampleRate(sampleRate);
      for (auto &o : mUnisonOsc2) o.SetSampleRate(sampleRate);
      for (auto &o : mUnisonOsc3) o.SetSampleRate(sampleRate);
      for (auto &o : mUnisonOsc4) o.SetSampleRate(sampleRate);

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

    // this is called by the VoiceAllocator to set generic control values.
    void SetControl(int controlNumber, float value) override
    {
      if (controlNumber == 1)
      {
        mModWheel = value;
      }
    }

    // Type 1
    std::array<VaiaOscillator<T>, 8> mUnisonOsc1{};
    ADSREnvelope<T> mPwmEnv1{};
    ADSREnvelope<T> mPitchEnv1{};

    // Type 2
    std::array<VaiaOscillator<T>, 8> mUnisonOsc2{};
    ADSREnvelope<T> mPwmEnv2{};
    ADSREnvelope<T> mPitchEnv2{};

    // Type 3
    std::array<VaiaOscillator<T>, 8> mUnisonOsc3{};
    ADSREnvelope<T> mPwmEnv3{};
    ADSREnvelope<T> mPitchEnv3{};

    // Type 4
    std::array<VaiaOscillator<T>, 8> mUnisonOsc4{};
    ADSREnvelope<T> mPwmEnv4{};
    ADSREnvelope<T> mPitchEnv4{};



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

    // Unison controls per-voice (1..8) and detune in cents (0..100)
    std::array<int, osc_count> extraUnisonCounts{};
    std::array<double, osc_count> extraDetuneCents{};


    OSC_Algorithm algo = used_algo;
    WDL_TypedBuf<float> mTimbreBuffer;
    double mModWheel{ 0.0 };


    // noise generator for test
    uint32_t mRandSeed = 0;

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
      mSynth.AddVoice(new Voice(), 0);
    }
  }

  void CentralizeLFO(T* pToCentralize, int nFrames, T levelScalar)
  {
    //mPitchLFO1.
    for (int s = 0; s < nFrames; ++s)
    {
      pToCentralize[s] -= levelScalar / 2.0f;
    }
  }

  void ProcessBlock(T** inputs, T** outputs, int nOutputs, int nFrames, double qnPos = 0., bool transportIsRunning = false, double tempo = 120.)
  {
    // clear outputs
    for (auto i = 0; i < nOutputs; i++)
    {
      memset(outputs[i], 0, nFrames * sizeof(T));
    }

    mParamSmoother.ProcessBlock(mParamsToSmooth.data(), mModulations.GetList(), nFrames);
    mPitchLFO1.ProcessBlock(mModulations.GetList()[kModPitchLFO1], nFrames, qnPos, transportIsRunning, tempo);
    mPwmLFO1.ProcessBlock(mModulations.GetList()[kModPwmLFO1], nFrames, qnPos, transportIsRunning, tempo);
    CentralizeLFO(mModulations.GetList()[kModPitchLFO1], nFrames, LFO<T>::GetQNScalar(LFO<T>::k1));
    CentralizeLFO(mModulations.GetList()[kModPwmLFO1], nFrames, LFO<T>::GetQNScalar(LFO<T>::k1));

    mPitchLFO2.ProcessBlock(mModulations.GetList()[kModPitchLFO2], nFrames, qnPos, transportIsRunning, tempo);
    mPwmLFO2.ProcessBlock(mModulations.GetList()[kModPwmLFO2], nFrames, qnPos, transportIsRunning, tempo);
    CentralizeLFO(mModulations.GetList()[kModPitchLFO2], nFrames, LFO<T>::GetQNScalar(LFO<T>::k1));
    CentralizeLFO(mModulations.GetList()[kModPwmLFO2], nFrames, LFO<T>::GetQNScalar(LFO<T>::k1));
   
    mPitchLFO3.ProcessBlock(mModulations.GetList()[kModPitchLFO3], nFrames, qnPos, transportIsRunning, tempo);
    mPwmLFO3.ProcessBlock(mModulations.GetList()[kModPwmLFO3], nFrames, qnPos, transportIsRunning, tempo);
    CentralizeLFO(mModulations.GetList()[kModPitchLFO3], nFrames, LFO<T>::GetQNScalar(LFO<T>::k1));
    CentralizeLFO(mModulations.GetList()[kModPwmLFO3], nFrames, LFO<T>::GetQNScalar(LFO<T>::k1));
   
    mPitchLFO4.ProcessBlock(mModulations.GetList()[kModPitchLFO4], nFrames, qnPos, transportIsRunning, tempo);
    mPwmLFO4.ProcessBlock(mModulations.GetList()[kModPwmLFO4], nFrames, qnPos, transportIsRunning, tempo);
    CentralizeLFO(mModulations.GetList()[kModPitchLFO4], nFrames, LFO<T>::GetQNScalar(LFO<T>::k1));
    CentralizeLFO(mModulations.GetList()[kModPwmLFO4], nFrames, LFO<T>::GetQNScalar(LFO<T>::k1));

    mSynth.ProcessBlock(mModulations.GetList(), outputs, 0, nOutputs, nFrames);

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

  void ProcessMidiMsg(const IMidiMsg& msg)
  {
    mSynth.AddMidiMsgToQueue(msg);
  }

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
    case kParamPwmRelease1:
    {
      EEnvStage stage = static_cast<EEnvStage>(EEnvStage::kAttack + (paramIdx - kParamPwmAttack1));
      mSynth.ForEachVoice([stage, value](SynthVoice& voice)
        {
          dynamic_cast<OneBitPlusDSP::Voice&>(voice).mPwmEnv1.SetStageTime(stage, value);
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
      mSynth.ForEachVoice([value](SynthVoice& voice)
        {
          dynamic_cast<OneBitPlusDSP::Voice&>(voice).pwmModStrengths[0] = value;
        });
      break;
    case kParamPwmOffset1:
      mSynth.ForEachVoice([value](SynthVoice& voice)
        {
          dynamic_cast<OneBitPlusDSP::Voice&>(voice).pwmOffsetStrengths[0] = value;
        });
      break;
    case kParamPwmKeyTrack1:
      mSynth.ForEachVoice([value](SynthVoice& voice)
        {
          dynamic_cast<OneBitPlusDSP::Voice&>(voice).pwmKeyTracks[0] = value > 0.5;
        });
      break;

    case kParamPitchSustain1:
      mParamsToSmooth[kModPitchSustainSmoother1] = (T)value / 100.;
      break;
    case kParamPitchAttack1:
    case kParamPitchDecay1:
    case kParamPitchRelease1:
    {
      EEnvStage stage = static_cast<EEnvStage>(EEnvStage::kAttack + (paramIdx - kParamPitchAttack1));
      mSynth.ForEachVoice([stage, value](SynthVoice& voice)
        {
          dynamic_cast<OneBitPlusDSP::Voice&>(voice).mPitchEnv1.SetStageTime(stage, value);
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
    case kParamPitchModPow1:
      mSynth.ForEachVoice([value](SynthVoice& voice)
        {
          dynamic_cast<OneBitPlusDSP::Voice&>(voice).pitchModStrengths[0] = value;
        });
      break;
    case kParamPitchOffset1:
      mSynth.ForEachVoice([value](SynthVoice& voice)
        {
          dynamic_cast<OneBitPlusDSP::Voice&>(voice).pitchOffsetStrengths[0] = value;
        });
      break;
    case kParamPitchKeyTrack1:
      mSynth.ForEachVoice([value](SynthVoice& voice)
        {
          dynamic_cast<OneBitPlusDSP::Voice&>(voice).pwmKeyTracks[0] = value;
        });
      break;

      // OSC 2
    case kParamPwmSustain2:
      mParamsToSmooth[kModPwmSustainSmoother2] = (T)value / 100.;
      break;
    case kParamPwmAttack2:
    case kParamPwmDecay2:
    case kParamPwmRelease2:
    {
      EEnvStage stage = static_cast<EEnvStage>(EEnvStage::kAttack + (paramIdx - kParamPwmAttack2));
      mSynth.ForEachVoice([stage, value](SynthVoice& voice)
        {
          dynamic_cast<OneBitPlusDSP::Voice&>(voice).mPwmEnv2.SetStageTime(stage, value);
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
      mSynth.ForEachVoice([value](SynthVoice& voice)
        {
          dynamic_cast<OneBitPlusDSP::Voice&>(voice).pwmModStrengths[1] = value;
        });
      break;
    case kParamPwmOffset2:
      mSynth.ForEachVoice([value](SynthVoice& voice)
        {
          dynamic_cast<OneBitPlusDSP::Voice&>(voice).pwmOffsetStrengths[1] = value;
        });
      break;
    case kParamPwmKeyTrack2:
      mSynth.ForEachVoice([value](SynthVoice& voice)
        {
          dynamic_cast<OneBitPlusDSP::Voice&>(voice).pwmKeyTracks[1] = value > 0.5;
        });
      break;

    case kParamPitchSustain2:
      mParamsToSmooth[kModPitchSustainSmoother2] = (T)value / 100.;
      break;
    case kParamPitchAttack2:
    case kParamPitchDecay2:
    case kParamPitchRelease2:
    {
      EEnvStage stage = static_cast<EEnvStage>(EEnvStage::kAttack + (paramIdx - kParamPitchAttack2));
      mSynth.ForEachVoice([stage, value](SynthVoice& voice)
        {
          dynamic_cast<OneBitPlusDSP::Voice&>(voice).mPitchEnv2.SetStageTime(stage, value);
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
    case kParamPitchModPow2:
      mSynth.ForEachVoice([value](SynthVoice& voice)
        {
          dynamic_cast<OneBitPlusDSP::Voice&>(voice).pitchModStrengths[1] = value;
        });
      break;
    case kParamPitchOffset2:
      mSynth.ForEachVoice([value](SynthVoice& voice)
        {
          dynamic_cast<OneBitPlusDSP::Voice&>(voice).pitchOffsetStrengths[1] = value;
        });
      break;
    case kParamPitchKeyTrack2:
      mSynth.ForEachVoice([value](SynthVoice& voice)
        {
          dynamic_cast<OneBitPlusDSP::Voice&>(voice).pwmKeyTracks[1] = value;
        });
      break;
      // OSC 3
    case kParamPwmSustain3:
      mParamsToSmooth[kModPwmSustainSmoother3] = (T)value / 100.;
      break;
    case kParamPwmAttack3:
    case kParamPwmDecay3:
    case kParamPwmRelease3:
    {
      EEnvStage stage = static_cast<EEnvStage>(EEnvStage::kAttack + (paramIdx - kParamPwmAttack3));
      mSynth.ForEachVoice([stage, value](SynthVoice& voice)
        {
          dynamic_cast<OneBitPlusDSP::Voice&>(voice).mPwmEnv3.SetStageTime(stage, value);
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
      mSynth.ForEachVoice([value](SynthVoice& voice)
        {
          dynamic_cast<OneBitPlusDSP::Voice&>(voice).pwmModStrengths[2] = value;
        });
      break;
    case kParamPwmOffset3:
      mSynth.ForEachVoice([value](SynthVoice& voice)
        {
          dynamic_cast<OneBitPlusDSP::Voice&>(voice).pwmOffsetStrengths[2] = value;
        });
      break;
    case kParamPwmKeyTrack3:
      mSynth.ForEachVoice([value](SynthVoice& voice)
        {
          dynamic_cast<OneBitPlusDSP::Voice&>(voice).pwmKeyTracks[2] = value > 0.5;
        });
      break;

    case kParamPitchSustain3:
      mParamsToSmooth[kModPitchSustainSmoother3] = (T)value / 100.;
      break;
    case kParamPitchAttack3:
    case kParamPitchDecay3:
    case kParamPitchRelease3:
    {
      EEnvStage stage = static_cast<EEnvStage>(EEnvStage::kAttack + (paramIdx - kParamPitchAttack3));
      mSynth.ForEachVoice([stage, value](SynthVoice& voice)
        {
          dynamic_cast<OneBitPlusDSP::Voice&>(voice).mPitchEnv3.SetStageTime(stage, value);
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
      mSynth.ForEachVoice([value](SynthVoice& voice)
        {
          dynamic_cast<OneBitPlusDSP::Voice&>(voice).pitchModStrengths[2] = value;
        });
      break;
    case kParamPitchOffset3:
      mSynth.ForEachVoice([value](SynthVoice& voice)
        {
          dynamic_cast<OneBitPlusDSP::Voice&>(voice).pitchOffsetStrengths[2] = value;
        });
      break;
    case kParamPitchKeyTrack3:
      mSynth.ForEachVoice([value](SynthVoice& voice)
        {
          dynamic_cast<OneBitPlusDSP::Voice&>(voice).pitchKeyTrackStrengths[2] = value;
        });
      break;
      // OSC 4
    case kParamPwmSustain4:
      mParamsToSmooth[kModPwmSustainSmoother4] = (T)value / 100.;
      break;
    case kParamPwmAttack4:
    case kParamPwmDecay4:
    case kParamPwmRelease4:
    {
      EEnvStage stage = static_cast<EEnvStage>(EEnvStage::kAttack + (paramIdx - kParamPwmAttack4));
      mSynth.ForEachVoice([stage, value](SynthVoice& voice)
        {
          dynamic_cast<OneBitPlusDSP::Voice&>(voice).mPwmEnv4.SetStageTime(stage, value);
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
      mSynth.ForEachVoice([value](SynthVoice& voice)
        {
          dynamic_cast<OneBitPlusDSP::Voice&>(voice).pwmModStrengths[3] = value;
        });
      break;
    case kParamPwmOffset4:
      mSynth.ForEachVoice([value](SynthVoice& voice)
        {
          dynamic_cast<OneBitPlusDSP::Voice&>(voice).pwmOffsetStrengths[3] = value;
        });
      break;
    case kParamPwmKeyTrack4:
      mSynth.ForEachVoice([value](SynthVoice& voice)
        {
          dynamic_cast<OneBitPlusDSP::Voice&>(voice).pwmKeyTracks[3] = value > 0.5;
        });
      break;

    case kParamPitchSustain4:
      mParamsToSmooth[kModPitchSustainSmoother4] = (T)value / 100.;
      break;
    case kParamPitchAttack4:
    case kParamPitchDecay4:
    case kParamPitchRelease4:
    {
      EEnvStage stage = static_cast<EEnvStage>(EEnvStage::kAttack + (paramIdx - kParamPitchAttack4));
      mSynth.ForEachVoice([stage, value](SynthVoice& voice)
        {
          dynamic_cast<OneBitPlusDSP::Voice&>(voice).mPitchEnv4.SetStageTime(stage, value);
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
      mSynth.ForEachVoice([value](SynthVoice& voice)
        {
          dynamic_cast<OneBitPlusDSP::Voice&>(voice).pitchModStrengths[3] = value;
        });
      break;
    case kParamPitchOffset4:
      mSynth.ForEachVoice([value](SynthVoice& voice)
        {
          dynamic_cast<OneBitPlusDSP::Voice&>(voice).pitchOffsetStrengths[3] = value;
        });
      break;
    case kParamPitchKeyTrack4:
      mSynth.ForEachVoice([value](SynthVoice& voice)
        {
          dynamic_cast<OneBitPlusDSP::Voice&>(voice).pitchKeyTrackStrengths[3] = value;
        });
      break;
    case kParamExtraUnison1:
      mSynth.ForEachVoice([value](SynthVoice& voice)
        {
          int v = static_cast<int>(value);
          if (v < 1) v = 1;
          if (v > 8) v = 8;
          dynamic_cast<OneBitPlusDSP::Voice&>(voice).extraUnisonCounts[0] = v;
        });
      break;
    case kParamExtraDetune1:
      mSynth.ForEachVoice([value](SynthVoice& voice)
        {
          double v = value;
          if (v < 0.0) v = 0.0;
          if (v > 100.0) v = 100.0;
          dynamic_cast<OneBitPlusDSP::Voice&>(voice).extraDetuneCents[0] = v;
        });
      break;
    case kParamExtraUnison2:
      mSynth.ForEachVoice([value](SynthVoice& voice)
        {
          int v = static_cast<int>(value);
          if (v < 1) v = 1;
          if (v > 8) v = 8;
          dynamic_cast<OneBitPlusDSP::Voice&>(voice).extraUnisonCounts[1] = v;
        });
      break;
    case kParamExtraDetune2:
      mSynth.ForEachVoice([value](SynthVoice& voice)
        {
          double v = value;
          if (v < 0.0) v = 0.0;
          if (v > 100.0) v = 100.0;
          dynamic_cast<OneBitPlusDSP::Voice&>(voice).extraDetuneCents[1] = v;
        });
      break;
    case kParamExtraUnison3:
      mSynth.ForEachVoice([value](SynthVoice& voice)
        {
          int v = static_cast<int>(value);
          if (v < 1) v = 1;
          if (v > 8) v = 8;
          dynamic_cast<OneBitPlusDSP::Voice&>(voice).extraUnisonCounts[2] = v;
        });
      break;
    case kParamExtraDetune3:
      mSynth.ForEachVoice([value](SynthVoice& voice)
        {
          double v = value;
          if (v < 0.0) v = 0.0;
          if (v > 100.0) v = 100.0;
          dynamic_cast<OneBitPlusDSP::Voice&>(voice).extraDetuneCents[2] = v;
        });
      break;
    case kParamExtraUnison4:
      mSynth.ForEachVoice([value](SynthVoice& voice)
        {
          int v = static_cast<int>(value);
          if (v < 1) v = 1;
          if (v > 8) v = 8;
          dynamic_cast<OneBitPlusDSP::Voice&>(voice).extraUnisonCounts[3] = v;
        });
      break;
    case kParamExtraDetune4:
      mSynth.ForEachVoice([value](SynthVoice& voice)
        {
          double v = value;
          if (v < 0.0) v = 0.0;
          if (v > 100.0) v = 100.0;
          dynamic_cast<OneBitPlusDSP::Voice&>(voice).extraDetuneCents[3] = v;
        });
      break;
      // DEFAULT
    default:
      break;
    }
  }

public:

  OSC_Algorithm algo = used_algo;

  MidiSynth mSynth{ VoiceAllocator::kPolyModePoly, MidiSynth::kDefaultBlockSize };
  WDL_TypedBuf<T> mModulationsData; // Sample data for global modulations (e.g. smoothed sustain)
  WDL_PtrList<T> mModulations; // Ptrlist for global modulations
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
