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


enum EModulations
{
  kModGainSmoother = 0,
  kModSustainSmoother,

  kModPitchLFO1,
  kModPwmLFO1,
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
    Voice() : mPWMEnv1("gain", [&]() { mOSC.Reset(); })
    {}

    bool GetBusy() const override
    {
      return mPWMEnv1.GetBusy();
    }

    void Trigger(double level, bool isRetrigger) override
    {
      mOSC.Reset();

      if (isRetrigger)
        mPWMEnv1.Retrigger(level);
      else
        mPWMEnv1.Start(level);
    }

    void Release() override
    {
      mPWMEnv1.Release();
    }


    void ProcessSamplesAccumulating(T** inputs, T** outputs, int nInputs, int nOutputs, int startIdx, int nFrames) override // NOSONAR (hidden function)
    {
      // inputs to the synthesizer can just fetch a value every block, like this:

      double pitch = mInputs[kVoiceControlPitch].endValue;
      double pitchBend = mInputs[kVoiceControlPitchBend].endValue;
      double velocity = mInputs[kVoiceControlGate].endValue * 127.f;

      // or write the entire control ramp to a buffer, like this, to get sample-accurate ramps:
      mInputs[kVoiceControlTimbre].Write(mTimbreBuffer.Get(), startIdx, nFrames);

      // convert from "1v/oct" pitch space to frequency in Hertz
      double osc1Freq = 440. * pow(2., pitch + pitchBend + inputs[kModPitchLFO1][0]);
      // make sound output for each output channel


      for (auto i = startIdx; i < startIdx + nFrames; ++i)
      {
        auto adsr_envelope = mPWMEnv1.Process(inputs[kModSustainSmoother][i]);

        if (pitch < -5.74 && velocity > 1.0)
        {
          double tempNoise = outputs[0][i] + (rand() % 255) > (velocity * (1 - mModWheel)) ? -0.9 : 1.1;
          if (tempNoise < 0.0f) tempNoise = 0.0f;
          outputs[0][i] = outputs[1][i] = tempNoise;
        }
        else if (pitch < -5.66 && velocity > 1.0)
        {
          if (i > startIdx && i % 4 == 3)
          {
            double tempNoise = outputs[0][i] + (rand() % 255) > (velocity * (1 - mModWheel)) ? -0.9 : 1.1;
            if (tempNoise < 0.0f) tempNoise = 0.0f;
            outputs[0][i] = outputs[1][i] = outputs[0][i - 1] = outputs[1][i - 1] = outputs[0][i - 2] = outputs[1][i - 2] = outputs[0][i - 3] = outputs[1][i - 3] = tempNoise;
          }
        }
        else if (pitch < -5.58 && velocity > 1.0)
        {
          if (i > startIdx && i % 8 == 7)
          {
            double tempNoise = outputs[0][i] + (rand() % 255) > (velocity * (1 - mModWheel)) ? -0.9 : 1.1;
            if (tempNoise < 0.0f) tempNoise = 0.0f;
            outputs[0][i] = outputs[1][i] = outputs[0][i - 1] = outputs[1][i - 1] = outputs[0][i - 2] = outputs[1][i - 2] = outputs[0][i - 3] = outputs[1][i - 3] = outputs[0][i - 4] = outputs[1][i - 4] = outputs[0][i - 5] = outputs[1][i - 5] = outputs[0][i - 6] = outputs[1][i - 6] = outputs[0][i - 7] = outputs[1][i - 7] = outputs[0][i] = outputs[1][i] = tempNoise;
          }
        }
        else
        {

          double pwmFunc = (0.025f - adsr_envelope + 0.756f) * osc1Freq / 440.0f + mModWheel;

          mOSC.SetPWM(pwmFunc);
          auto base = mOSC.Process(osc1Freq);

          bool oldOutput, newOutput;
          double temp = outputs[0][i] + base;

          switch (algo)
          {

          case OSC_Algorithm::ALGO_PIN_PULSE:

            oldOutput = outputs[0][i] > 0.0f;
            newOutput = base > 0.0f;

            if (newOutput)
            {
              outputs[0][i] = 1.0f;
              outputs[1][i] = 1.0f;
            }


            break;
          }
        }
      }
    }

    void SetSampleRateAndBlockSize(double sampleRate, int blockSize) override
    {
      mOSC.SetSampleRate(sampleRate);

      mPWMEnv1.SetSampleRate(sampleRate);

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

    VaiaOscillator<T> mOSC;
    ADSREnvelope<T> mPWMEnv1;
    ADSREnvelope<T> mPitchEnv1;
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

  void ProcessBlock(T** inputs, T** outputs, int nOutputs, int nFrames, double qnPos = 0., bool transportIsRunning = false, double tempo = 120.)
  {
    // clear outputs
    for (auto i = 0; i < nOutputs; i++)
    {
      memset(outputs[i], 0, nFrames * sizeof(T));
    }

    mParamSmoother.ProcessBlock(mParamsToSmooth.data(), mModulations.GetList(), nFrames);
    mPitchLFO1.ProcessBlock(mModulations.GetList()[kModPwmLFO1], nFrames, qnPos, transportIsRunning, tempo);
    mEnvLFO1.ProcessBlock(mModulations.GetList()[kModPitchLFO1], nFrames, qnPos, transportIsRunning, tempo);
    mSynth.ProcessBlock(mModulations.GetList(), outputs, 0, nOutputs, nFrames);

    double eps = 0.001;


    OSC_Algorithm algo2 = OSC_Algorithm::ALGO_MOD_TWO;
    switch (algo2)
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
    mPitchLFO1.SetSampleRate(sampleRate);
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
    case kParamEnvSustain1:
      mParamsToSmooth[kModSustainSmoother] = (T)value / 100.;
      break;
    case kParamEnvAttack1:
    case kParamEnvDecay1:
    case kParamEnvRelease1:
    {
      EEnvStage stage = static_cast<EEnvStage>(EEnvStage::kAttack + (paramIdx - kParamEnvAttack1));
      mSynth.ForEachVoice([stage, value](SynthVoice& voice)
        {
          dynamic_cast<OneBitPlusDSP::Voice&>(voice).mPWMEnv1.SetStageTime(stage, value);
        });
      break;
    }
    case kParamEnvLFODepth1:
      mEnvLFO1.SetScalar(value / 100.);
      break;
    case kParamEnvLFORateTempo1:
      mEnvLFO1.SetQNScalarFromDivision(static_cast<int>(value));
      break;
    case kParamEnvLFORateHz1:
      mEnvLFO1.SetFreqCPS(value);
      break;
    case kParamEnvLFORateMode1:
      mEnvLFO1.SetRateMode(value > 0.5);
      break;
    case kParamEnvLFOShape1:
      mEnvLFO1.SetShape(static_cast<int>(value));
      break;
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
  LFO<T> mEnvLFO1; // TODO: rename to mPwmLFO1
};
