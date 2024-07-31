#pragma once

#include "IPlug_include_in_plug_hdr.h"
#include "IControls.h"

#if IPLUG_DSP
// will use EParams in OneBitIsEnough_DSP.h
#include "VaiaOneBitPlus_DSP.h"
#endif

const int kNumPresets = 1;

enum EParams
{
  kParamGain = 0,
  kParamNoteGlideTime,
  kParamEnvAttack1,
  kParamEnvDecay1,
  kParamEnvSustain1,
  kParamEnvRelease1,
  kParamLFOShape,
  kParamLFORateHz,
  kParamLFORateTempo,
  kParamLFORateMode,
  kParamLFODepth,
  kNoParameter,
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

using namespace iplug;
using namespace igraphics;

class VaiaOneBitPlus final : public Plugin
{
public:
  VaiaOneBitPlus(const InstanceInfo& info);

#if IPLUG_DSP // http://bit.ly/2S64BDd
public:
  void ProcessBlock(sample** inputs, sample** outputs, int nFrames) override;
  void ProcessMidiMsg(const IMidiMsg& msg) override;
  void OnReset() override;
  void OnParamChange(int paramIdx) override;
  void OnIdle() override;
  bool OnMessage(int msgTag, int ctrlTag, int dataSize, const void* pData) override;

private:
  OneBitPlusDSP<sample> mDSP{ 32 };
  IPeakSender<2> mMeterSender;
  ISender<1> mLFOVisSender;
  ISender<1> mOscilloscopeVisSender;
#endif
};
