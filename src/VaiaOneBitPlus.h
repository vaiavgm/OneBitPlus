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

  // Type 1
  kParamPwmAttack1,
  kParamPwmDecay1,
  kParamPwmSustain1,
  kParamPwmRelease1,

  kParamPwmLFORateMode1,
  kParamPwmLFOShape1,
  kParamPwmLFORateHz1,
  kParamPwmLFORateTempo1,
  kParamPwmLFODepth1,

  kParamPwmKeyTrack1,
  kParamPwmModPow1,
  kParamPwmOffset1,

  kParamPitchAttack1,
  kParamPitchDecay1,
  kParamPitchSustain1,
  kParamPitchRelease1,

  kParamPitchLFORateMode1,
  kParamPitchLFOShape1,
  kParamPitchLFORateHz1,
  kParamPitchLFORateTempo1,
  kParamPitchLFODepth1,

  kParamPitchKeyTrack1,
  kParamPitchModPow1,
  kParamPitchOffset1,

  kParamExtraUnison1,
  kParamExtraDetune1,

  // Type 2
  kParamPwmAttack2,
  kParamPwmDecay2,
  kParamPwmSustain2,
  kParamPwmRelease2,

  kParamPwmLFORateMode2,
  kParamPwmLFOShape2,
  kParamPwmLFORateHz2,
  kParamPwmLFORateTempo2,
  kParamPwmLFODepth2,

  kParamPwmKeyTrack2,
  kParamPwmModPow2,
  kParamPwmOffset2,

  kParamPitchAttack2,
  kParamPitchDecay2,
  kParamPitchSustain2,
  kParamPitchRelease2,

  kParamPitchLFORateMode2,
  kParamPitchLFOShape2,
  kParamPitchLFORateHz2,
  kParamPitchLFORateTempo2,
  kParamPitchLFODepth2,

  kParamPitchKeyTrack2,
  kParamPitchModPow2,
  kParamPitchOffset2,

  kParamExtraUnison2,
  kParamExtraDetune2,

  // Type 3
  kParamPwmAttack3,
  kParamPwmDecay3,
  kParamPwmSustain3,
  kParamPwmRelease3,

  kParamPwmLFORateMode3,
  kParamPwmLFOShape3,
  kParamPwmLFORateHz3,
  kParamPwmLFORateTempo3,
  kParamPwmLFODepth3,

  kParamPwmKeyTrack3,
  kParamPwmModPow3,
  kParamPwmOffset3,

  kParamPitchAttack3,
  kParamPitchDecay3,
  kParamPitchSustain3,
  kParamPitchRelease3,

  kParamPitchLFORateMode3,
  kParamPitchLFOShape3,
  kParamPitchLFORateHz3,
  kParamPitchLFORateTempo3,
  kParamPitchLFODepth3,

  kParamPitchKeyTrack3,
  kParamPitchModPow3,
  kParamPitchOffset3,

  kParamExtraUnison3,
  kParamExtraDetune3,

  // Type 4
  kParamPwmAttack4,
  kParamPwmDecay4,
  kParamPwmSustain4,
  kParamPwmRelease4,

  kParamPwmLFORateMode4,
  kParamPwmLFOShape4,
  kParamPwmLFORateHz4,
  kParamPwmLFORateTempo4,
  kParamPwmLFODepth4,

  kParamPwmKeyTrack4,
  kParamPwmModPow4,
  kParamPwmOffset4,

  kParamPitchAttack4,
  kParamPitchDecay4,
  kParamPitchSustain4,
  kParamPitchRelease4,

  kParamPitchLFORateMode4,
  kParamPitchLFOShape4,
  kParamPitchLFORateHz4,
  kParamPitchLFORateTempo4,
  kParamPitchLFODepth4,

  kParamPitchKeyTrack4,
  kParamPitchModPow4,
  kParamPitchOffset4,

  kParamExtraUnison4,
  kParamExtraDetune4,

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
