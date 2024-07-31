#pragma once

#define MY_PRINTF(...) {char buf[512]; sprintf(buf, __VA_ARGS__);  OutputDebugString(buf);}

#include "MidiSynth.h"
#include "Oscillator.h"
#include "ADSREnvelope.h"
#include "Smoothers.h"
#include "LFO.h"
#include "VaiaOscillator.h"


enum EModulations
{
  kModGainSmoother = 0,
  kModSustainSmoother,
  kModLFO,
  kNumModulations,
};

enum class osc_algorithm
{
  ALGO_FLIP_ONE, //This one is not quite correct, as it outputs some zeroes where it shouldn't
  ALGO_MOD_TWO,
  ALGO_PIN_PULSE, // just add and clamp
};

const osc_algorithm used_algo = osc_algorithm::ALGO_PIN_PULSE;

template<typename T>
class OneBitPlusDSP
{
public:
#pragma mark - Voice
  class Voice : public SynthVoice
  {

  public:
    Voice() : mAMPEnv("gain", [&]() { mOSC.Reset(); }) // capture ok on RT thread?
    {
      //      DBGMSG("new Voice: %i control inputs.\n", static_cast<int>(mInputs.size()));
    }

    bool GetBusy() const override
    {
      return mAMPEnv.GetBusy();
    }

    void Trigger(double level, bool isRetrigger) override
    {
      mOSC.Reset();

      if (isRetrigger)
        mAMPEnv.Retrigger(level);
      else
        mAMPEnv.Start(level);
    }

    void Release() override
    {
      mAMPEnv.Release();
    }


    void ProcessSamplesAccumulating(T** inputs, T** outputs, int nInputs, int nOutputs, int startIdx, int nFrames) override // NOSONAR (hidden function)
    {
      // inputs to the synthesizer can just fetch a value every block, like this:
//      double gate = mInputs[kVoiceControlGate].endValue;
      double pitch = mInputs[kVoiceControlPitch].endValue;
      double pitchBend = mInputs[kVoiceControlPitchBend].endValue;
      double velocity = mInputs[kVoiceControlGate].endValue * 127.f;

      // or write the entire control ramp to a buffer, like this, to get sample-accurate ramps:
      mInputs[kVoiceControlTimbre].Write(mTimbreBuffer.Get(), startIdx, nFrames);

      // convert from "1v/oct" pitch space to frequency in Hertz
      double osc1Freq = 440. * pow(2., pitch + pitchBend + inputs[kModLFO][0]);
      // make sound output for each output channel


      for (auto i = startIdx; i < startIdx + nFrames; ++i)
      {
        auto adsr_envelope = mAMPEnv.Process(inputs[kModSustainSmoother][i]);

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
          case osc_algorithm::ALGO_FLIP_ONE:

            oldOutput = outputs[0][i] > 0;
            newOutput = base > 0;


            if (oldOutput == 0.0f)
            {
              outputs[0][i] = outputs[1][i] = base;
            }

            if (newOutput == oldOutput)
            {
              outputs[0][i] = outputs[1][i] = newOutput * -1.0;
            }
            else
            {
              outputs[0][i] = outputs[1][i] = newOutput;
            }

            if (outputs[0][i] < 0.1 && outputs[0][i]> -0.1)
            {
              outputs[0][i] = outputs[1][i] = base;
            }
            break;
          case osc_algorithm::ALGO_MOD_TWO:
            if (base > 0)
            {
              temp += 0.1;
            }
            if (base < 0)
            {
              temp += 0.1;
            }

            outputs[0][i] = outputs[1][i] = temp;
            break;
          case osc_algorithm::ALGO_PIN_PULSE:

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

      mAMPEnv.SetSampleRate(sampleRate);

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
    ADSREnvelope<T> mAMPEnv;
    osc_algorithm algo = used_algo;
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
    // some MidiSynth API examples:
    // mSynth.SetKeyToPitchFn([](int k){return (k - 69.)/24.;}); // quarter-tone scale
  }

  void ProcessBlock(T** inputs, T** outputs, int nOutputs, int nFrames, double qnPos = 0., bool transportIsRunning = false, double tempo = 120.)
  {
    // clear outputs
    for (auto i = 0; i < nOutputs; i++)
    {
      memset(outputs[i], 0, nFrames * sizeof(T));
    }

    mParamSmoother.ProcessBlock(mParamsToSmooth.data(), mModulations.GetList(), nFrames);
    mLFO.ProcessBlock(mModulations.GetList()[kModLFO], nFrames, qnPos, transportIsRunning, tempo);
    mSynth.ProcessBlock(mModulations.GetList(), outputs, 0, nOutputs, nFrames);

    double eps = 0.001;


    osc_algorithm algo2 = osc_algorithm::ALGO_MOD_TWO;
    switch (algo2)
    {
    case osc_algorithm::ALGO_FLIP_ONE:

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
      break;
    case osc_algorithm::ALGO_MOD_TWO:

      for (int s = 0; s < nFrames; s++)
      {

        auto check_modulo = (int)(abs(outputs[0][s])) % 2;


        T smoothedGain = mModulations.GetList()[kModGainSmoother][s];
        if (abs(outputs[0][s]) < eps)
        {
          outputs[0][s] = outputs[1][s] = 0.0;
        }
        else if (check_modulo == 1)
        {

          outputs[0][s] = 1.0 * smoothedGain;
          outputs[1][s] = 1.0 * smoothedGain;
        }
        else if (check_modulo == 0)
        {
          outputs[0][s] = -1.0 * smoothedGain;
          outputs[1][s] = -1.0 * smoothedGain;
        }
        else
        {
          outputs[0][s] = outputs[1][s] = 0.0;
        }

      }
      break;
    case osc_algorithm::ALGO_PIN_PULSE:
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
    mLFO.SetSampleRate(sampleRate);
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
          dynamic_cast<OneBitPlusDSP::Voice&>(voice).mAMPEnv.SetStageTime(stage, value);
        });
      break;
    }
    case kParamLFODepth:
      mLFO.SetScalar(value / 100.);
      break;
    case kParamLFORateTempo:
      mLFO.SetQNScalarFromDivision(static_cast<int>(value));
      break;
    case kParamLFORateHz:
      mLFO.SetFreqCPS(value);
      break;
    case kParamLFORateMode:
      mLFO.SetRateMode(value > 0.5);
      break;
    case kParamLFOShape:
      mLFO.SetShape(static_cast<int>(value));
      break;
    default:
      break;
    }
  }

public:

  osc_algorithm algo = used_algo;

  MidiSynth mSynth{ VoiceAllocator::kPolyModePoly, MidiSynth::kDefaultBlockSize };
  WDL_TypedBuf<T> mModulationsData; // Sample data for global modulations (e.g. smoothed sustain)
  WDL_PtrList<T> mModulations; // Ptrlist for global modulations
  LogParamSmooth<T, kNumModulations> mParamSmoother;
  std::array<sample, kNumModulations> mParamsToSmooth{};
  LFO<T> mLFO;
};
