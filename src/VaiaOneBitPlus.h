#pragma once

#if IPLUG_DSP
// will use EParams in OneBitIsEnough_DSP.h
#include "VaiaOneBitPlus_DSP.h"
#endif

#include <IPlug_include_in_plug_hdr.h>
#include <IPlugConstants.h>
#include <IPlugMidi.h>
#include <ISender.h>


const int kNumPresets = 1;

enum EParams
{
  kParamGain = 0,
  kParamNoteGlideTime,

  kParamEnvAttack1,            // TODO: change to kParamPwm... instead of Env
  kParamEnvDecay1,             // TODO: change to kParamPwm... instead of Env
  kParamEnvSustain1,           // TODO: change to kParamPwm... instead of Env
  kParamEnvRelease1,           // TODO: change to kParamPwm... instead of Env
                               // TODO: change to kParamPwm... instead of Env
  kParamEnvLFORateMode1,       // TODO: change to kParamPwm... instead of Env
  kParamEnvLFOShape1,          // TODO: change to kParamPwm... instead of Env
  kParamEnvLFORateHz1,         // TODO: change to kParamPwm... instead of Env
  kParamEnvLFORateTempo1,      // TODO: change to kParamPwm... instead of Env
  kParamEnvLFODepth1,          // TODO: change to kParamPwm... instead of Env
                               // TODO: change to kParamPwm... instead of Env
  kParamEnvKeyTrack1,          // TODO: change to kParamPwm... instead of Env
  kParamEnvModWheel1,          // TODO: change to kParamPwm... instead of Env
  kParamEnvOffset1,            // TODO: change to kParamPwm... instead of Env

  kParamPitchAttack1,
  kParamPitchDecay1,
  kParamPitchSustain1,
  kParamPitchRelease1,

  kParamPitchLFORateMode1,
  kParamPitchLFOShape1,
  kParamPitchLFORateHz1,
  kParamPitchLFORateTempo1,
  kParamPitchLFORateDepth1,

  kParamPitchKeyTrack1,
  kParamPitchModWheel1,
  kParamPitchOffset1,

  kParamExtraUnison1,
  kParamExtraDetune1,

  kNumParams
};



enum EControlTags
{
  kCtrlTagMeter = 0,
  kCtrlTagLFOVis,
  kCtrlTagScope,
  kCtrlTagRTText,
  kCtrlTagKeyboard,
  kCtrlTagBender,
  kCtrlTagOscilloscope,
  kNumCtrlTags
};


class VaiaOneBitPlus final : public Plugin
{
public:
  explicit VaiaOneBitPlus(const InstanceInfo& info);

#if IPLUG_DSP // http://bit.ly/2S64BDd

  void ProcessBlock(sample** inputs, sample** outputs, int nFrames) override;
  void ProcessMidiMsg(const IMidiMsg& msg) override;
  void OnReset() override;
  void OnParamChange(int paramIdx) override;
  void OnIdle() override;
  bool OnMessage(int msgTag, int ctrlTag, int dataSize, const void* pData) override;

  OneBitPlusDSP<sample> mDSP{ 32 };
  IPeakSender<2> mMeterSender;
  ISender<1> mLFOVisSender;
  ISender<1> mOscilloscopeVisSender;
#endif
};
