#include "VaiaOneBitPlus.h"
#include <IPlug_include_in_plug_src.h>
#include <LFO.h>
#include <IPlugLogger.h>
#include <IControls.h>


VaiaOneBitPlus::VaiaOneBitPlus(const InstanceInfo& info)
  : Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  GetParam(kParamGain)->InitDouble("Gain", 50., 0., 100.0, 0.01, "%");
  GetParam(kParamNoteGlideTime)->InitMilliseconds("Note Glide Time", 0., 0.0, 150.);
  GetParam(kParamEnvAttack1)->InitDouble("Attack", 150., 1., 1000., 0.1, "ms", IParam::kFlagsNone, "ADSR", IParam::ShapePowCurve(3.));
  GetParam(kParamEnvDecay1)->InitDouble("Decay", 0., 1., 1000., 0.1, "ms", IParam::kFlagsNone, "ADSR", IParam::ShapePowCurve(3.));
  GetParam(kParamEnvSustain1)->InitDouble("Sustain", 100., 0., 100., 1, "%", IParam::kFlagsNone, "ADSR");
  GetParam(kParamEnvRelease1)->InitDouble("Release", 0., 2., 1000., 0.1, "ms", IParam::kFlagsNone, "ADSR");
  GetParam(kParamLFOShape)->InitEnum("LFO Shape", LFO<>::kTriangle, { LFO_SHAPE_VALIST });
  GetParam(kParamLFORateHz)->InitFrequency("LFO Rate", 1., 0.01, 40.);
  GetParam(kParamLFORateTempo)->InitEnum("LFO Rate", LFO<>::k1, { LFO_TEMPODIV_VALIST });
  GetParam(kParamLFORateMode)->InitBool("LFO Sync", true);
  GetParam(kParamLFODepth)->InitPercentage("LFO Depth");

#if IPLUG_EDITOR // http://bit.ly/2S64BDd
  mMakeGraphicsFunc = [this]()
    {
      return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
    };






  using namespace igraphics;


  mLayoutFunc = [&](IGraphics* pGraphics)
    {

      pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
      pGraphics->AttachPanelBackground(COLOR_GRAY);
      pGraphics->EnableMouseOver(true);
      pGraphics->EnableMultiTouch(true);

      pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);
      const IRECT b = pGraphics->GetBounds().GetPadded(-20.f);
      const IRECT lfoPanel = b.GetFromLeft(150.f).GetFromTop(125.f);
      const IRECT oscilloscopePanel = b.GetFromLeft(400.f).GetFromTop(125.f).GetReducedFromLeft(180.f).GetReducedFromRight(50.f).GetReducedFromTop(-10.f).GetReducedFromBottom(-10.f);
      IRECT keyboardBounds = b.GetFromBottom(150);
      IRECT wheelsBounds = keyboardBounds.ReduceFromLeft(100.f).GetPadded(-10.f);
      pGraphics->AttachControl(new IVKeyboardControl(keyboardBounds), kCtrlTagKeyboard);
      pGraphics->AttachControl(new IWheelControl(wheelsBounds.FracRectHorizontal(0.5)), kCtrlTagBender);
      pGraphics->AttachControl(new IWheelControl(wheelsBounds.FracRectHorizontal(0.5, true), IMidiMsg::EControlChangeMsg::kModWheel));
      //    pGraphics->AttachControl(new IVMultiSliderControl<4>(b.GetGridCell(0, 2, 2).GetPadded(-30), "", DEFAULT_STYLE, kParamEnvAttack1, EDirection::Vertical, 0.f, 1.f));
      const IRECT controls = b.GetFromLeft(400.f).GetReducedFromLeft(500.f).GetFromTop(150.f);
      pGraphics->AttachControl(new IVKnobControl(controls.GetGridCell(0, 2, 4).GetReducedFromLeft(50.f).GetCentredInside(90), kParamGain, "Gain"));
      pGraphics->AttachControl(new IVKnobControl(controls.GetGridCell(2, 2, 4).GetCentredInside(90), kParamNoteGlideTime, "Glide"));

      pGraphics->AttachControl(new IVLEDMeterControl<2>(controls.GetFromRight(35).GetPadded(-5)), kCtrlTagMeter);

      pGraphics->AttachControl(new IVKnobControl(lfoPanel.GetGridCell(0, 0, 2, 3).GetCentredInside(60), kParamLFORateHz, "Rate"), kNoTag, "LFO")->Hide(true);
      pGraphics->AttachControl(new IVKnobControl(lfoPanel.GetGridCell(0, 0, 2, 3).GetCentredInside(60), kParamLFORateTempo, "Rate"), kNoTag, "LFO")->DisablePrompt(false);
      pGraphics->AttachControl(new IVKnobControl(lfoPanel.GetGridCell(0, 1, 2, 3).GetCentredInside(60), kParamLFODepth, "Depth"), kNoTag, "LFO");
      pGraphics->AttachControl(new IVKnobControl(lfoPanel.GetGridCell(0, 2, 2, 3).GetCentredInside(60), kParamLFOShape, "Shape"), kNoTag, "LFO")->DisablePrompt(false);
      pGraphics->AttachControl(new IVSlideSwitchControl(lfoPanel.GetGridCell(1, 0, 2, 3).GetFromTop(30).GetMidHPadded(20), kParamLFORateMode, "Sync", DEFAULT_STYLE.WithShowValue(false).WithShowLabel(false).WithWidgetFrac(0.5f).WithDrawShadows(false), false), kNoTag, "LFO")->SetAnimationEndActionFunction([pGraphics](const IControl* pControl)
        {
          bool sync = pControl->GetValue() > 0.5;
          pGraphics->HideControl(kParamLFORateHz, sync);
          pGraphics->HideControl(kParamLFORateTempo, !sync);
        });
      pGraphics->AttachControl(new IVDisplayControl(oscilloscopePanel.GetGridCell(0, 0, 1, 1), "", DEFAULT_STYLE, EDirection::Horizontal, -1.f, 1.f, 0.f, 128), kCtrlTagOscilloscope, "Oscilloscope");

      pGraphics->AttachControl(new IVDisplayControl(lfoPanel.GetGridCell(1, 1, 2, 3).Union(lfoPanel.GetGridCell(1, 2, 2, 3)), "", DEFAULT_STYLE, EDirection::Horizontal, 0.f, 1.f, 0.f, 1024), kCtrlTagLFOVis, "LFO");

      pGraphics->AttachControl(new IVGroupControl("LFO", "LFO", 10.f, 20.f, 10.f, 10.f));

      pGraphics->AttachControl(new IVButtonControl(keyboardBounds.GetFromTRHC(200, 30).GetTranslated(0, -60), SplashClickActionFunc,
        "Show/Hide Keyboard", DEFAULT_STYLE.WithColor(kFG, COLOR_WHITE).WithLabelText({ 15.f, EVAlign::Middle })))->SetAnimationEndActionFunction(
          [pGraphics]([[maybe_unused]] const IControl* pCaller)
          {
            static bool hide = false;
            pGraphics->GetControlWithTag(kCtrlTagKeyboard)->Hide(hide = !hide);
            pGraphics->Resize(PLUG_WIDTH, hide ? PLUG_HEIGHT / 2 : PLUG_HEIGHT, pGraphics->GetDrawScale());
          });

      pGraphics->SetQwertyMidiKeyHandlerFunc([pGraphics](const IMidiMsg& msg)
        {
          pGraphics->GetControlWithTag(kCtrlTagKeyboard)->As<IVKeyboardControl>()->SetNoteFromMidi(msg.NoteNumber(), msg.StatusMsg() == IMidiMsg::kNoteOn);
        });
    };







#endif
}



#if IPLUG_DSP
void VaiaOneBitPlus::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  mDSP.ProcessBlock(nullptr, outputs, 2, nFrames, mTimeInfo.mPPQPos, mTimeInfo.mTransportIsRunning);
  mMeterSender.ProcessBlock(outputs, nFrames, kCtrlTagMeter);

  mLFOVisSender.PushData({ kCtrlTagLFOVis, {float(mDSP.mLFO.GetLastOutput())} });
  for (int i = 0; i < nFrames; ++i)
  {
    mOscilloscopeVisSender.PushData({ kCtrlTagOscilloscope, {float(outputs[0][i])} });
  }
}

void VaiaOneBitPlus::OnIdle()
{
  mMeterSender.TransmitData(*this);
  mLFOVisSender.TransmitData(*this);
  mOscilloscopeVisSender.TransmitData(*this);
}

void VaiaOneBitPlus::OnReset()
{
  mDSP.Reset(GetSampleRate(), GetBlockSize());
}

void VaiaOneBitPlus::ProcessMidiMsg(const IMidiMsg& msg)
{
  TRACE;

  switch (int status = msg.StatusMsg())
  {
  case IMidiMsg::kNoteOn:
  case IMidiMsg::kNoteOff:
  case IMidiMsg::kPolyAftertouch:
  case IMidiMsg::kControlChange:
  case IMidiMsg::kProgramChange:
  case IMidiMsg::kChannelAftertouch:
  case IMidiMsg::kPitchWheel:
  {
    goto handle;
  }
  default:
    return;
  }

handle:
  mDSP.ProcessMidiMsg(msg);
  SendMidiMsg(msg);
}

void VaiaOneBitPlus::OnParamChange(int paramIdx)
{
  mDSP.SetParam(paramIdx, GetParam(paramIdx)->Value());
}

bool VaiaOneBitPlus::OnMessage(int msgTag, int ctrlTag, int dataSize, const void* pData)
{
  if (ctrlTag == kCtrlTagBender && msgTag == igraphics::IWheelControl::kMessageTagSetPitchBendRange)
  {
    const int bendRange = *static_cast<const int*>(pData);
    mDSP.mSynth.SetPitchBendRange(bendRange);
  }

  return false;
}
#endif
