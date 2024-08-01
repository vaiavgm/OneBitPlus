#include "VaiaOneBitPlus.h"
#include <IPlug_include_in_plug_src.h>

#include <LFO.h>
#include <IPlugLogger.h>
#include <IControls.h>
#include <IGraphicsStructs.h>


VaiaOneBitPlus::VaiaOneBitPlus(const InstanceInfo& info)
  : Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  GetParam(kParamGain)->InitDouble("Gain", 50., 0., 100.0, 0.01, "%");
  GetParam(kParamNoteGlideTime)->InitMilliseconds("Note Glide Time", 0., 0.0, 150.);

  // OSC 1
  GetParam(kParamPwmAttack1)->InitDouble("Attack", 0., 0., 2000., 0.1, "ms", IParam::kFlagsNone, "ADSR", IParam::ShapePowCurve(3.));
  GetParam(kParamPwmDecay1)->InitDouble("Decay", 0., 0., 2000., 0.1, "ms", IParam::kFlagsNone, "ADSR", IParam::ShapePowCurve(3.));
  GetParam(kParamPwmSustain1)->InitDouble("Sustain", 0., 0., 100., 1, "%", IParam::kFlagsNone, "ADSR");
  GetParam(kParamPwmRelease1)->InitDouble("Release", 0., 0., 500., 0.1, "ms", IParam::kFlagsNone, "ADSR");
  GetParam(kParamPwmLFOShape1)->InitEnum("LFO Shape", LFO<>::kTriangle, { LFO_SHAPE_VALIST });
  GetParam(kParamPwmLFORateHz1)->InitFrequency("LFO Rate", 1., 0.01, 40.);
  GetParam(kParamPwmLFORateTempo1)->InitEnum("LFO Rate", LFO<>::k1, { LFO_TEMPODIV_VALIST });
  GetParam(kParamPwmLFORateMode1)->InitBool("LFO Sync", true);
  GetParam(kParamPwmLFODepth1)->InitPercentage("LFO Depth");
  GetParam(kParamPwmModPow1)->InitDouble("PWM Mod", 1.0, -2.0, 2.0, 0.05);
  GetParam(kParamPwmOffset1)->InitDouble("PWM Offset", 0.5, 0.0025, 0.9975, 0.01);
  GetParam(kParamPwmKeyTrack1)->InitBool("PWM Keytrack", false);

  GetParam(kParamPitchAttack1)->InitDouble("Attack", 0., 0., 2000., 0.1, "ms", IParam::kFlagsNone, "ADSR", IParam::ShapePowCurve(3.));
  GetParam(kParamPitchDecay1)->InitDouble("Decay", 0., 0., 2000., 0.1, "ms", IParam::kFlagsNone, "ADSR", IParam::ShapePowCurve(3.));
  GetParam(kParamPitchSustain1)->InitDouble("Sustain", 0., 0., 100., 1, "%", IParam::kFlagsNone, "ADSR");
  GetParam(kParamPitchRelease1)->InitDouble("Release", 0., 0., 500., 0.1, "ms", IParam::kFlagsNone, "ADSR");
  GetParam(kParamPitchLFOShape1)->InitEnum("Pitch LFO Shape", LFO<>::kTriangle, { LFO_SHAPE_VALIST });
  GetParam(kParamPitchLFORateHz1)->InitFrequency("Pitch LFO Rate", 1., 0.01, 40.);
  GetParam(kParamPitchLFORateTempo1)->InitEnum("Pitch LFO Rate", LFO<>::k1, { LFO_TEMPODIV_VALIST });
  GetParam(kParamPitchLFORateMode1)->InitBool("Pitch LFO Sync", true);
  GetParam(kParamPitchLFODepth1)->InitPercentage("Pitch LFO Depth");
  GetParam(kParamPitchModPow1)->InitDouble("Pitch Mod", 0, -4.0, 4.0, 0.05);
  GetParam(kParamPitchOffset1)->InitDouble("Pitch Offset", 0.0, -1.0, 1.0, 0.01);
  GetParam(kParamPitchKeyTrack1)->InitDouble("Pitch Keytrack", 0.0, -2.0, 2.0, 0.05);

  // OSC 2
  GetParam(kParamPwmAttack2)->InitDouble("Attack", 0., 0., 2000., 0.1, "ms", IParam::kFlagsNone, "ADSR", IParam::ShapePowCurve(3.));
  GetParam(kParamPwmDecay2)->InitDouble("Decay", 0., 0., 2000., 0.1, "ms", IParam::kFlagsNone, "ADSR", IParam::ShapePowCurve(3.));
  GetParam(kParamPwmSustain2)->InitDouble("Sustain", 0., 0., 100., 1, "%", IParam::kFlagsNone, "ADSR");
  GetParam(kParamPwmRelease2)->InitDouble("Release", 0., 0., 500., 0.1, "ms", IParam::kFlagsNone, "ADSR");
  GetParam(kParamPwmLFOShape2)->InitEnum("LFO Shape", LFO<>::kTriangle, { LFO_SHAPE_VALIST });
  GetParam(kParamPwmLFORateHz2)->InitFrequency("LFO Rate", 1., 0.01, 40.);
  GetParam(kParamPwmLFORateTempo2)->InitEnum("LFO Rate", LFO<>::k1, { LFO_TEMPODIV_VALIST });
  GetParam(kParamPwmLFORateMode2)->InitBool("LFO Sync", true);
  GetParam(kParamPwmLFODepth2)->InitPercentage("LFO Depth");
  GetParam(kParamPwmModPow2)->InitDouble("PWM Mod", 1.0, -2.0, 2.0, 0.05);
  GetParam(kParamPwmOffset2)->InitDouble("PWM Offset", 0.5, 0.0025, 0.9975, 0.01);
  GetParam(kParamPwmKeyTrack2)->InitBool("PWM Keytrack", false);

  GetParam(kParamPitchAttack2)->InitDouble("Attack", 0., 0., 2000., 0.1, "ms", IParam::kFlagsNone, "ADSR", IParam::ShapePowCurve(3.));
  GetParam(kParamPitchDecay2)->InitDouble("Decay", 0., 0., 2000., 0.1, "ms", IParam::kFlagsNone, "ADSR", IParam::ShapePowCurve(3.));
  GetParam(kParamPitchSustain2)->InitDouble("Sustain", 0., 0., 100., 1, "%", IParam::kFlagsNone, "ADSR");
  GetParam(kParamPitchRelease2)->InitDouble("Release", 0., 0., 500., 0.1, "ms", IParam::kFlagsNone, "ADSR");
  GetParam(kParamPitchLFOShape2)->InitEnum("Pitch LFO Shape", LFO<>::kTriangle, { LFO_SHAPE_VALIST });
  GetParam(kParamPitchLFORateHz2)->InitFrequency("Pitch LFO Rate", 1., 0.01, 40.);
  GetParam(kParamPitchLFORateTempo2)->InitEnum("Pitch LFO Rate", LFO<>::k1, { LFO_TEMPODIV_VALIST });
  GetParam(kParamPitchLFORateMode2)->InitBool("Pitch LFO Sync", true);
  GetParam(kParamPitchLFODepth2)->InitPercentage("Pitch LFO Depth");
  GetParam(kParamPitchModPow2)->InitDouble("Pitch Mod", 0, -4.0, 4.0, 0.05);
  GetParam(kParamPitchOffset2)->InitDouble("Pitch Offset", 0.0, -1.0, 1.0, 0.01);
  GetParam(kParamPitchKeyTrack2)->InitDouble("Pitch Keytrack", 0.0, -2.0, 2.0, 0.05);

  // OSC 3
  GetParam(kParamPwmAttack3)->InitDouble("Attack", 0., 0., 2000., 0.1, "ms", IParam::kFlagsNone, "ADSR", IParam::ShapePowCurve(3.));
  GetParam(kParamPwmDecay3)->InitDouble("Decay", 0., 0., 2000., 0.1, "ms", IParam::kFlagsNone, "ADSR", IParam::ShapePowCurve(3.));
  GetParam(kParamPwmSustain3)->InitDouble("Sustain", 0., 0., 100., 1, "%", IParam::kFlagsNone, "ADSR");
  GetParam(kParamPwmRelease3)->InitDouble("Release", 0., 0., 500., 0.1, "ms", IParam::kFlagsNone, "ADSR");
  GetParam(kParamPwmLFOShape3)->InitEnum("LFO Shape", LFO<>::kTriangle, { LFO_SHAPE_VALIST });
  GetParam(kParamPwmLFORateHz3)->InitFrequency("LFO Rate", 1., 0.01, 40.);
  GetParam(kParamPwmLFORateTempo3)->InitEnum("LFO Rate", LFO<>::k1, { LFO_TEMPODIV_VALIST });
  GetParam(kParamPwmLFORateMode3)->InitBool("LFO Sync", true);
  GetParam(kParamPwmLFODepth3)->InitPercentage("LFO Depth");
  GetParam(kParamPwmModPow3)->InitDouble("PWM Mod", 1.0, -2.0, 2.0, 0.05);
  GetParam(kParamPwmOffset3)->InitDouble("PWM Offset", 0.5, 0.0025, 0.9975, 0.01);
  GetParam(kParamPwmKeyTrack3)->InitBool("PWM Keytrack", false);

  GetParam(kParamPitchAttack3)->InitDouble("Attack", 0., 0., 2000., 0.1, "ms", IParam::kFlagsNone, "ADSR", IParam::ShapePowCurve(3.));
  GetParam(kParamPitchDecay3)->InitDouble("Decay", 0., 0., 2000., 0.1, "ms", IParam::kFlagsNone, "ADSR", IParam::ShapePowCurve(3.));
  GetParam(kParamPitchSustain3)->InitDouble("Sustain", 0., 0., 100., 1, "%", IParam::kFlagsNone, "ADSR");
  GetParam(kParamPitchRelease3)->InitDouble("Release", 0., 0., 500., 0.1, "ms", IParam::kFlagsNone, "ADSR");
  GetParam(kParamPitchLFOShape3)->InitEnum("Pitch LFO Shape", LFO<>::kTriangle, { LFO_SHAPE_VALIST });
  GetParam(kParamPitchLFORateHz3)->InitFrequency("Pitch LFO Rate", 1., 0.01, 40.);
  GetParam(kParamPitchLFORateTempo3)->InitEnum("Pitch LFO Rate", LFO<>::k1, { LFO_TEMPODIV_VALIST });
  GetParam(kParamPitchLFORateMode3)->InitBool("Pitch LFO Sync", true);
  GetParam(kParamPitchLFODepth3)->InitPercentage("Pitch LFO Depth");
  GetParam(kParamPitchModPow3)->InitDouble("Pitch Mod", 0, -4.0, 4.0, 0.05);
  GetParam(kParamPitchOffset3)->InitDouble("Pitch Offset", 0.0, -1.0, 1.0, 0.01);
  GetParam(kParamPitchKeyTrack3)->InitDouble("Pitch Keytrack", 0.0, -2.0, 2.0, 0.05);

  // OSC 4
  GetParam(kParamPwmAttack4)->InitDouble("Attack", 0., 0., 2000., 0.1, "ms", IParam::kFlagsNone, "ADSR", IParam::ShapePowCurve(3.));
  GetParam(kParamPwmDecay4)->InitDouble("Decay", 0., 0., 2000., 0.1, "ms", IParam::kFlagsNone, "ADSR", IParam::ShapePowCurve(3.));
  GetParam(kParamPwmSustain4)->InitDouble("Sustain", 0., 0., 100., 1, "%", IParam::kFlagsNone, "ADSR");
  GetParam(kParamPwmRelease4)->InitDouble("Release", 0., 0., 500., 0.1, "ms", IParam::kFlagsNone, "ADSR");
  GetParam(kParamPwmLFOShape4)->InitEnum("LFO Shape", LFO<>::kTriangle, { LFO_SHAPE_VALIST });
  GetParam(kParamPwmLFORateHz4)->InitFrequency("LFO Rate", 1., 0.01, 40.);
  GetParam(kParamPwmLFORateTempo4)->InitEnum("LFO Rate", LFO<>::k1, { LFO_TEMPODIV_VALIST });
  GetParam(kParamPwmLFORateMode4)->InitBool("LFO Sync", true);
  GetParam(kParamPwmLFODepth4)->InitPercentage("LFO Depth");
  GetParam(kParamPwmModPow4)->InitDouble("PWM Mod", 1.0, -2.0, 2.0, 0.05);
  GetParam(kParamPwmOffset4)->InitDouble("PWM Offset", 0.5, 0.0025, 0.9975, 0.01);
  GetParam(kParamPwmKeyTrack4)->InitBool("PWM Keytrack", false);

  GetParam(kParamPitchAttack4)->InitDouble("Attack", 0., 0., 2000., 0.1, "ms", IParam::kFlagsNone, "ADSR", IParam::ShapePowCurve(3.));
  GetParam(kParamPitchDecay4)->InitDouble("Decay", 0., 0., 2000., 0.1, "ms", IParam::kFlagsNone, "ADSR", IParam::ShapePowCurve(3.));
  GetParam(kParamPitchSustain4)->InitDouble("Sustain", 0., 0., 100., 1, "%", IParam::kFlagsNone, "ADSR");
  GetParam(kParamPitchRelease4)->InitDouble("Release", 0., 0., 500., 0.1, "ms", IParam::kFlagsNone, "ADSR");
  GetParam(kParamPitchLFOShape4)->InitEnum("Pitch LFO Shape", LFO<>::kTriangle, { LFO_SHAPE_VALIST });
  GetParam(kParamPitchLFORateHz4)->InitFrequency("Pitch LFO Rate", 1., 0.01, 40.);
  GetParam(kParamPitchLFORateTempo4)->InitEnum("Pitch LFO Rate", LFO<>::k1, { LFO_TEMPODIV_VALIST });
  GetParam(kParamPitchLFORateMode4)->InitBool("Pitch LFO Sync", true);
  GetParam(kParamPitchLFODepth4)->InitPercentage("Pitch LFO Depth");
  GetParam(kParamPitchModPow4)->InitDouble("Pitch Mod", 0, -4.0, 4.0, 0.05);
  GetParam(kParamPitchOffset4)->InitDouble("Pitch Offset", 0.0, -1.0, 1.0, 0.01);
  GetParam(kParamPitchKeyTrack4)->InitDouble("Pitch Keytrack", 0.0, -2.0, 2.0, 0.05);



  using namespace igraphics;


#if IPLUG_EDITOR // http://bit.ly/2S64BDd




  mMakeGraphicsFunc = [this]()
    {
      return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
    };


  mLayoutFunc = [&](IGraphics* pGraphics)
  {
      pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
      pGraphics->AttachPanelBackground(COLOR_GRAY);
      pGraphics->EnableMouseOver(true);
      pGraphics->EnableMultiTouch(true);

      pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);

      const IVStyle DEFAULT_STYLE{
        true, // Show label
        true, // Show value
        {
          COLOR_TRANSPARENT, // Background
          COLOR_MID_GRAY, // Foreground
          COLOR_LIGHT_GRAY, // Pressed
          COLOR_DARK_GRAY, // Frame
          COLOR_TRANSLUCENT, // Highlight
          IColor(60, 0, 0, 0), // Shadow
          COLOR_BLACK, // Extra 1
          COLOR_GREEN, // Extra 2
          COLOR_BLUE  // Extra 3
        }, // Colors 
        IText(19.0, COLOR_BLACK, "Roboto-Regular", EAlign::Center, EVAlign::Top, 0.0), // Label text
        IText(14.0, COLOR_BLACK, "Roboto-Regular", EAlign::Center, EVAlign::Bottom, 0.0), // Value text
        true, // Hide cursor
        true, // Draw frame
        true, // Draw shadows
        false, // Emboss
        0.0, // Roundness
        1.0, // Frame thickness
        3.0, // Shadow offset
        1.0, // Widget frac
        0.0  // Angle
      };

      const IVStyle LARGER_LABEL{
        true, // Show label
        true, // Show value
        {
          COLOR_TRANSPARENT, // Background
          COLOR_MID_GRAY, // Foreground
          COLOR_LIGHT_GRAY, // Pressed
          COLOR_DARK_GRAY, // Frame
          COLOR_TRANSLUCENT, // Highlight
          IColor(60, 0, 0, 0), // Shadow
          COLOR_BLACK, // Extra 1
          COLOR_GREEN, // Extra 2
          COLOR_BLUE  // Extra 3
        }, // Colors 
        IText(20.0, COLOR_BLACK, "Roboto-Regular", EAlign::Center, EVAlign::Top, 0.0), // Label text
        IText(20.0, COLOR_BLACK, "Roboto-Regular", EAlign::Center, EVAlign::Bottom, 0.0), // Value text
        true, // Hide cursor
        true, // Draw frame
        true, // Draw shadows
        false, // Emboss
        0.0, // Roundness
        1.0, // Frame thickness
        3.0, // Shadow offset
        1.0, // Widget frac
        0.0  // Angle
      };

      const IVStyle SMALLER_LABEL{
        true, // Show label
        true, // Show value
        {
          COLOR_TRANSPARENT, // Background
          COLOR_MID_GRAY, // Foreground
          COLOR_LIGHT_GRAY, // Pressed
          COLOR_DARK_GRAY, // Frame
          COLOR_TRANSLUCENT, // Highlight
          IColor(60, 0, 0, 0), // Shadow
          COLOR_BLACK, // Extra 1
          COLOR_GREEN, // Extra 2
          COLOR_BLUE  // Extra 3
        }, // Colors 
        IText(14.0, COLOR_BLACK, "Roboto-Regular", EAlign::Center, EVAlign::Top, 0.0), // Label text
        IText(10.0, COLOR_BLACK, "Roboto-Regular", EAlign::Center, EVAlign::Bottom, 0.0), // Value text
        true, // Hide cursor
        true, // Draw frame
        true, // Draw shadows
        false, // Emboss
        0.0, // Roundness
        1.0, // Frame thickness
        3.0, // Shadow offset
        1.0, // Widget frac
        0.0  // Angle
      };

      const IText HEADER_TEXT{
        20.0,
        "Roboto-Regular"
      };

      const IText SMALL_TEXT{
        12.0,
        "Roboto-Regular"
      };




    //////////////////////////////
    /// Insert GUI from Editor ///


    //////////////// HEADER LABELS
    pGraphics->AttachControl(new ITextControl(IRECT(170.0, 0.0f, 320.0, 20.0f), "PWM Control", HEADER_TEXT));
    pGraphics->AttachControl(new ITextControl(IRECT(570.0, 0.0f, 720.0, 20.0f), "Pitch Control", HEADER_TEXT));
    pGraphics->AttachControl(new ITextControl(IRECT(1070.0, 0.0f, 1220.0, 20.0f), "Extras", HEADER_TEXT));

    //////////////// OSC 1




    float vert = 0.0;
    pGraphics->AttachControl(new ITextControl(IRECT(0.0, 25.0f+vert, 50.0, 125.0f+vert), "Type I", SMALL_TEXT));

    pGraphics->AttachControl(new IVGroupControl(IRECT(170.0, 20.0f+vert, 320.0, 120.0f+vert), "LFO", 10.0, SMALLER_LABEL));
    pGraphics->AttachControl(new IVGroupControl(IRECT(330.0, 20.0f+vert, 500.0, 120.0f+vert), "Mods", 10.0, SMALLER_LABEL));
    pGraphics->AttachControl(new IVGroupControl(IRECT(650.0, 20.0f+vert, 800.0, 120.0f+vert), "LFO", 10.0, SMALLER_LABEL));
    pGraphics->AttachControl(new IVGroupControl(IRECT(810.0, 20.0f+vert, 980.0, 120.0f+vert), "Mods", 10.0, SMALLER_LABEL));

    pGraphics->AttachControl(new IVSliderControl(IRECT(40.0, 35.0f+vert, 80.0, 120.0f+vert), kParamPwmAttack1, "A", SMALLER_LABEL, true, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
    pGraphics->AttachControl(new IVSliderControl(IRECT(70.0, 35.0f+vert, 110.0, 120.0f+vert), kParamPwmDecay1, "D", SMALLER_LABEL, true, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
    pGraphics->AttachControl(new IVSliderControl(IRECT(100.0, 35.0f+vert, 140.0, 120.0f+vert), kParamPwmSustain1, "S", SMALLER_LABEL, true, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
    pGraphics->AttachControl(new IVSliderControl(IRECT(130.0, 35.0f+vert, 170.0, 120.0f+vert), kParamPwmRelease1, "R", SMALLER_LABEL, true, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
    pGraphics->AttachControl(new IVSlideSwitchControl(IRECT(175.0, 60.0f+vert, 195.0, 90.0f+vert), kParamPwmLFORateMode1, "", DEFAULT_STYLE.WithShowValue(false).WithShowLabel(false), false, EDirection::Vertical), kNoTag, "LFO")->SetAnimationEndActionFunction([pGraphics](const IControl* pControl)
      {
        bool sync = pControl->GetValue() > 0.5;
        pGraphics->HideControl(kParamPwmLFORateHz1, sync);
        pGraphics->HideControl(kParamPwmLFORateTempo1, !sync);
      });
    pGraphics->AttachControl(new IVKnobControl(IRECT(205.0, 35.0f+vert, 240.0, 115.0f+vert), kParamPwmLFORateTempo1, "Rate", SMALLER_LABEL, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0))->DisablePrompt(false);
    pGraphics->AttachControl(new IVKnobControl(IRECT(205.0, 35.0f+vert, 240.0, 115.0f+vert), kParamPwmLFORateHz1, "Rate", SMALLER_LABEL, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0))->Hide(true);
    pGraphics->AttachControl(new IVKnobControl(IRECT(240.0, 35.0f+vert, 275.0, 115.0f+vert), kParamPwmLFODepth1, "Depth", SMALLER_LABEL, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
    pGraphics->AttachControl(new IVKnobControl(IRECT(275.0, 35.0f+vert, 310.0, 115.0f+vert), kParamPwmLFOShape1, "Shape", SMALLER_LABEL, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0))->DisablePrompt(false);
    pGraphics->AttachControl(new IVSlideSwitchControl(IRECT(340.0, 60.0f+vert, 360.0, 90.0f+vert), kParamPwmKeyTrack1, "", DEFAULT_STYLE.WithShowValue(false).WithShowLabel(false), true, EDirection::Vertical)); pGraphics->AttachControl(new IVKnobControl(IRECT(390.0, 35.0f+vert, 435.0, 115.0f+vert), kParamPwmModPow1, "Mod Pow", SMALLER_LABEL, true, false, -135.0, 135.0, 67.5, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
    pGraphics->AttachControl(new IVKnobControl(IRECT(445.0, 35.0f+vert, 490.0, 115.0f+vert), kParamPwmOffset1, "Offset", SMALLER_LABEL, true, false, -135.0, 135.0, 0.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));




    pGraphics->AttachControl(new IVSliderControl(IRECT(520.0, 35.0f+vert, 560.0, 120.0f+vert), kParamPitchAttack1, "A", SMALLER_LABEL, true, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
    pGraphics->AttachControl(new IVSliderControl(IRECT(550.0, 35.0f+vert, 590.0, 120.0f+vert), kParamPitchDecay1, "D", SMALLER_LABEL, true, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
    pGraphics->AttachControl(new IVSliderControl(IRECT(580.0, 35.0f+vert, 620.0, 120.0f+vert), kParamPitchSustain1, "S", SMALLER_LABEL, true, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
    pGraphics->AttachControl(new IVSliderControl(IRECT(610.0, 35.0f+vert, 650.0, 120.0f+vert), kParamPitchRelease1, "R", SMALLER_LABEL, true, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
    pGraphics->AttachControl(new IVSlideSwitchControl(IRECT(655.0, 55.0f+vert, 675.0, 85.0f+vert), kParamPitchLFORateMode1, "", DEFAULT_STYLE.WithShowValue(false).WithShowLabel(false), false, EDirection::Vertical), kNoTag, "LFO")->SetAnimationEndActionFunction([pGraphics](const IControl* pControl)
      {
        bool sync = pControl->GetValue() > 0.5;
        pGraphics->HideControl(kParamPitchLFORateHz1, sync);
        pGraphics->HideControl(kParamPitchLFORateTempo1, !sync);
      });
    pGraphics->AttachControl(new IVKnobControl(IRECT(685.0, 35.0f+vert, 720.0, 115.0f+vert), kParamPitchLFORateTempo1, "Rate", SMALLER_LABEL, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0))->DisablePrompt(false);
    pGraphics->AttachControl(new IVKnobControl(IRECT(685.0, 35.0f+vert, 720.0, 115.0f+vert), kParamPitchLFORateHz1, "Rate", SMALLER_LABEL, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0))->Hide(true);
    pGraphics->AttachControl(new IVKnobControl(IRECT(720.0, 35.0f+vert, 755.0, 115.0f+vert), kParamPitchLFODepth1, "Depth", SMALLER_LABEL, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
    pGraphics->AttachControl(new IVKnobControl(IRECT(755.0, 35.0f+vert, 790.0, 115.0f+vert), kParamPitchLFOShape1, "Shape", SMALLER_LABEL, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0))->DisablePrompt(false);
    pGraphics->AttachControl(new IVKnobControl(IRECT(815.0, 35.0f+vert, 860.0, 115.0f+vert), kParamPitchKeyTrack1, "Key Tr.", SMALLER_LABEL, true, false, -135.0, 135.0, 0.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
    pGraphics->AttachControl(new IVKnobControl(IRECT(870.0, 35.0f+vert, 915.0, 115.0f+vert), kParamPitchModPow1, "Mod Pow", SMALLER_LABEL, true, false, -135.0, 135.0, 0.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
    pGraphics->AttachControl(new IVKnobControl(IRECT(925.0, 35.0f+vert, 970.0, 115.0f+vert), kParamPitchOffset1, "Offset", SMALLER_LABEL, true, false, -135.0, 135.0, 0.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));


    pGraphics->AttachControl(new IVNumberBoxControl(IRECT(1000.0, 35.0f+vert, 1075.0, 85.0f+vert), kParamExtraUnison1, nullptr, "Unison", DEFAULT_STYLE, 1.0, 1.0, 8.0, "%0.0f"));
    pGraphics->AttachControl(new IVKnobControl(IRECT(1075.0, 30.0f+vert, 1130.0, 95.0f+vert), kParamExtraDetune1, "Detune", DEFAULT_STYLE, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));

    //////////////// OSC 2

    vert += 110;

    pGraphics->AttachControl(new ITextControl(IRECT(0.0, 25.0f + vert, 50.0, 125.0f + vert), "Type II", SMALL_TEXT));

    pGraphics->AttachControl(new IVGroupControl(IRECT(170.0, 20.0f + vert, 320.0, 120.0f + vert), "LFO", 10.0, SMALLER_LABEL));
    pGraphics->AttachControl(new IVGroupControl(IRECT(330.0, 20.0f + vert, 500.0, 120.0f + vert), "Mods", 10.0, SMALLER_LABEL));
    pGraphics->AttachControl(new IVGroupControl(IRECT(650.0, 20.0f + vert, 800.0, 120.0f + vert), "LFO", 10.0, SMALLER_LABEL));
    pGraphics->AttachControl(new IVGroupControl(IRECT(810.0, 20.0f + vert, 980.0, 120.0f + vert), "Mods", 10.0, SMALLER_LABEL));

    pGraphics->AttachControl(new IVSliderControl(IRECT(40.0, 35.0f + vert, 80.0, 120.0f + vert), kParamPwmAttack2, "A", SMALLER_LABEL, true, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
    pGraphics->AttachControl(new IVSliderControl(IRECT(70.0, 35.0f + vert, 110.0, 120.0f + vert), kParamPwmDecay2, "D", SMALLER_LABEL, true, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
    pGraphics->AttachControl(new IVSliderControl(IRECT(100.0, 35.0f + vert, 140.0, 120.0f + vert), kParamPwmSustain2, "S", SMALLER_LABEL, true, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
    pGraphics->AttachControl(new IVSliderControl(IRECT(130.0, 35.0f + vert, 170.0, 120.0f + vert), kParamPwmRelease2, "R", SMALLER_LABEL, true, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
    pGraphics->AttachControl(new IVSlideSwitchControl(IRECT(175.0, 60.0f + vert, 195.0, 90.0f + vert), kParamPwmLFORateMode2, "", DEFAULT_STYLE.WithShowValue(false).WithShowLabel(false), false, EDirection::Vertical), kNoTag, "LFO")->SetAnimationEndActionFunction([pGraphics](const IControl* pControl)
      {
        bool sync = pControl->GetValue() > 0.5;
        pGraphics->HideControl(kParamPwmLFORateHz2, sync);
        pGraphics->HideControl(kParamPwmLFORateTempo2, !sync);
      });
    pGraphics->AttachControl(new IVKnobControl(IRECT(205.0, 35.0f + vert, 240.0, 115.0f + vert), kParamPwmLFORateTempo2, "Rate", SMALLER_LABEL, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0))->DisablePrompt(false);
    pGraphics->AttachControl(new IVKnobControl(IRECT(205.0, 35.0f + vert, 240.0, 115.0f + vert), kParamPwmLFORateHz2, "Rate", SMALLER_LABEL, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0))->Hide(true);
    pGraphics->AttachControl(new IVKnobControl(IRECT(240.0, 35.0f + vert, 275.0, 115.0f + vert), kParamPwmLFODepth2, "Depth", SMALLER_LABEL, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
    pGraphics->AttachControl(new IVKnobControl(IRECT(275.0, 35.0f + vert, 310.0, 115.0f + vert), kParamPwmLFOShape2, "Shape", SMALLER_LABEL, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0))->DisablePrompt(false);
    pGraphics->AttachControl(new IVSlideSwitchControl(IRECT(340.0, 60.0f + vert, 360.0, 90.0f + vert), kParamPwmKeyTrack2, "", DEFAULT_STYLE.WithShowValue(false).WithShowLabel(false), true, EDirection::Vertical)); pGraphics->AttachControl(new IVKnobControl(IRECT(390.0, 35.0f+vert, 435.0, 115.0f+vert), kParamPwmModPow2, "Mod Pow", SMALLER_LABEL, true, false, -135.0, 135.0, 67.5, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
    pGraphics->AttachControl(new IVKnobControl(IRECT(445.0, 35.0f + vert, 490.0, 115.0f + vert), kParamPwmOffset2, "Offset", SMALLER_LABEL, true, false, -135.0, 135.0, 0.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));




    pGraphics->AttachControl(new IVSliderControl(IRECT(520.0, 35.0f + vert, 560.0, 120.0f + vert), kParamPitchAttack2, "A", SMALLER_LABEL, true, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
    pGraphics->AttachControl(new IVSliderControl(IRECT(550.0, 35.0f + vert, 590.0, 120.0f + vert), kParamPitchDecay2, "D", SMALLER_LABEL, true, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
    pGraphics->AttachControl(new IVSliderControl(IRECT(580.0, 35.0f + vert, 620.0, 120.0f + vert), kParamPitchSustain2, "S", SMALLER_LABEL, true, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
    pGraphics->AttachControl(new IVSliderControl(IRECT(610.0, 35.0f + vert, 650.0, 120.0f + vert), kParamPitchRelease2, "R", SMALLER_LABEL, true, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
    pGraphics->AttachControl(new IVSlideSwitchControl(IRECT(655.0, 55.0f + vert, 675.0, 85.0f + vert), kParamPitchLFORateMode2, "", DEFAULT_STYLE.WithShowValue(false).WithShowLabel(false), false, EDirection::Vertical), kNoTag, "LFO")->SetAnimationEndActionFunction([pGraphics](const IControl* pControl)
      {
        bool sync = pControl->GetValue() > 0.5;
        pGraphics->HideControl(kParamPitchLFORateHz2, sync);
        pGraphics->HideControl(kParamPitchLFORateTempo2, !sync);
      });
    pGraphics->AttachControl(new IVKnobControl(IRECT(685.0, 35.0f + vert, 720.0, 115.0f + vert), kParamPitchLFORateTempo2, "Rate", SMALLER_LABEL, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0))->DisablePrompt(false);
    pGraphics->AttachControl(new IVKnobControl(IRECT(685.0, 35.0f + vert, 720.0, 115.0f + vert), kParamPitchLFORateHz2, "Rate", SMALLER_LABEL, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0))->Hide(true);
    pGraphics->AttachControl(new IVKnobControl(IRECT(720.0, 35.0f + vert, 755.0, 115.0f + vert), kParamPitchLFODepth2, "Depth", SMALLER_LABEL, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
    pGraphics->AttachControl(new IVKnobControl(IRECT(755.0, 35.0f + vert, 790.0, 115.0f + vert), kParamPitchLFOShape2, "Shape", SMALLER_LABEL, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0))->DisablePrompt(false);
    pGraphics->AttachControl(new IVKnobControl(IRECT(815.0, 35.0f + vert, 860.0, 115.0f + vert), kParamPitchKeyTrack2, "Key Tr.", SMALLER_LABEL, true, false, -135.0, 135.0, 0.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
    pGraphics->AttachControl(new IVKnobControl(IRECT(870.0, 35.0f + vert, 915.0, 115.0f + vert), kParamPitchModPow2, "Mod Pow", SMALLER_LABEL, true, false, -135.0, 135.0, 0.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
    pGraphics->AttachControl(new IVKnobControl(IRECT(925.0, 35.0f + vert, 970.0, 115.0f + vert), kParamPitchOffset2, "Offset", SMALLER_LABEL, true, false, -135.0, 135.0, 0.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));


    pGraphics->AttachControl(new IVNumberBoxControl(IRECT(1000.0, 35.0f + vert, 1075.0, 85.0f + vert), kParamExtraUnison2, nullptr, "Unison", DEFAULT_STYLE, 1.0, 1.0, 8.0, "%0.0f"));
    pGraphics->AttachControl(new IVKnobControl(IRECT(1075.0, 30.0f + vert, 1130.0, 95.0f + vert), kParamExtraDetune2, "Detune", DEFAULT_STYLE, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));

    //////////////// OSC 3

    vert += 110;

    pGraphics->AttachControl(new ITextControl(IRECT(0.0, 25.0f + vert, 50.0, 125.0f + vert), "Type III", SMALL_TEXT));

    pGraphics->AttachControl(new IVGroupControl(IRECT(170.0, 20.0f + vert, 320.0, 120.0f + vert), "LFO", 10.0, SMALLER_LABEL));
    pGraphics->AttachControl(new IVGroupControl(IRECT(330.0, 20.0f + vert, 500.0, 120.0f + vert), "Mods", 10.0, SMALLER_LABEL));
    pGraphics->AttachControl(new IVGroupControl(IRECT(650.0, 20.0f + vert, 800.0, 120.0f + vert), "LFO", 10.0, SMALLER_LABEL));
    pGraphics->AttachControl(new IVGroupControl(IRECT(810.0, 20.0f + vert, 980.0, 120.0f + vert), "Mods", 10.0, SMALLER_LABEL));

    pGraphics->AttachControl(new IVSliderControl(IRECT(40.0, 35.0f + vert, 80.0, 120.0f + vert), kParamPwmAttack3, "A", SMALLER_LABEL, true, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
    pGraphics->AttachControl(new IVSliderControl(IRECT(70.0, 35.0f + vert, 110.0, 120.0f + vert), kParamPwmDecay3, "D", SMALLER_LABEL, true, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
    pGraphics->AttachControl(new IVSliderControl(IRECT(100.0, 35.0f + vert, 140.0, 120.0f + vert), kParamPwmSustain3, "S", SMALLER_LABEL, true, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
    pGraphics->AttachControl(new IVSliderControl(IRECT(130.0, 35.0f + vert, 170.0, 120.0f + vert), kParamPwmRelease3, "R", SMALLER_LABEL, true, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
    pGraphics->AttachControl(new IVSlideSwitchControl(IRECT(175.0, 60.0f + vert, 195.0, 90.0f + vert), kParamPwmLFORateMode3, "", DEFAULT_STYLE.WithShowValue(false).WithShowLabel(false), false, EDirection::Vertical), kNoTag, "LFO")->SetAnimationEndActionFunction([pGraphics](const IControl* pControl)
      {
        bool sync = pControl->GetValue() > 0.5;
        pGraphics->HideControl(kParamPwmLFORateHz3, sync);
        pGraphics->HideControl(kParamPwmLFORateTempo3, !sync);
      });
    pGraphics->AttachControl(new IVKnobControl(IRECT(205.0, 35.0f + vert, 240.0, 115.0f + vert), kParamPwmLFORateTempo3, "Rate", SMALLER_LABEL, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0))->DisablePrompt(false);
    pGraphics->AttachControl(new IVKnobControl(IRECT(205.0, 35.0f + vert, 240.0, 115.0f + vert), kParamPwmLFORateHz3, "Rate", SMALLER_LABEL, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0))->Hide(true);
    pGraphics->AttachControl(new IVKnobControl(IRECT(240.0, 35.0f + vert, 275.0, 115.0f + vert), kParamPwmLFODepth3, "Depth", SMALLER_LABEL, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
    pGraphics->AttachControl(new IVKnobControl(IRECT(275.0, 35.0f + vert, 310.0, 115.0f + vert), kParamPwmLFOShape3, "Shape", SMALLER_LABEL, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0))->DisablePrompt(false);
    pGraphics->AttachControl(new IVSlideSwitchControl(IRECT(340.0, 60.0f + vert, 360.0, 90.0f + vert), kParamPwmKeyTrack3, "", DEFAULT_STYLE.WithShowValue(false).WithShowLabel(false), true, EDirection::Vertical)); pGraphics->AttachControl(new IVKnobControl(IRECT(390.0, 35.0f + vert, 435.0, 115.0f + vert), kParamPwmModPow3, "Mod Pow", SMALLER_LABEL, true, false, -135.0, 135.0, 67.5, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
    pGraphics->AttachControl(new IVKnobControl(IRECT(445.0, 35.0f + vert, 490.0, 115.0f + vert), kParamPwmOffset3, "Offset", SMALLER_LABEL, true, false, -135.0, 135.0, 0.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));




    pGraphics->AttachControl(new IVSliderControl(IRECT(520.0, 35.0f + vert, 560.0, 120.0f + vert), kParamPitchAttack3, "A", SMALLER_LABEL, true, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
    pGraphics->AttachControl(new IVSliderControl(IRECT(550.0, 35.0f + vert, 590.0, 120.0f + vert), kParamPitchDecay3, "D", SMALLER_LABEL, true, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
    pGraphics->AttachControl(new IVSliderControl(IRECT(580.0, 35.0f + vert, 620.0, 120.0f + vert), kParamPitchSustain3, "S", SMALLER_LABEL, true, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
    pGraphics->AttachControl(new IVSliderControl(IRECT(610.0, 35.0f + vert, 650.0, 120.0f + vert), kParamPitchRelease3, "R", SMALLER_LABEL, true, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
    pGraphics->AttachControl(new IVSlideSwitchControl(IRECT(655.0, 55.0f + vert, 675.0, 85.0f + vert), kParamPitchLFORateMode3, "", DEFAULT_STYLE.WithShowValue(false).WithShowLabel(false), false, EDirection::Vertical), kNoTag, "LFO")->SetAnimationEndActionFunction([pGraphics](const IControl* pControl)
      {
        bool sync = pControl->GetValue() > 0.5;
        pGraphics->HideControl(kParamPitchLFORateHz3, sync);
        pGraphics->HideControl(kParamPitchLFORateTempo3, !sync);
      });
    pGraphics->AttachControl(new IVKnobControl(IRECT(685.0, 35.0f + vert, 720.0, 115.0f + vert), kParamPitchLFORateTempo3, "Rate", SMALLER_LABEL, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0))->DisablePrompt(false);
    pGraphics->AttachControl(new IVKnobControl(IRECT(685.0, 35.0f + vert, 720.0, 115.0f + vert), kParamPitchLFORateHz3, "Rate", SMALLER_LABEL, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0))->Hide(true);
    pGraphics->AttachControl(new IVKnobControl(IRECT(720.0, 35.0f + vert, 755.0, 115.0f + vert), kParamPitchLFODepth3, "Depth", SMALLER_LABEL, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
    pGraphics->AttachControl(new IVKnobControl(IRECT(755.0, 35.0f + vert, 790.0, 115.0f + vert), kParamPitchLFOShape3, "Shape", SMALLER_LABEL, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0))->DisablePrompt(false);
    pGraphics->AttachControl(new IVKnobControl(IRECT(815.0, 35.0f + vert, 860.0, 115.0f + vert), kParamPitchKeyTrack3, "Key Tr.", SMALLER_LABEL, true, false, -135.0, 135.0, 0.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
    pGraphics->AttachControl(new IVKnobControl(IRECT(870.0, 35.0f + vert, 915.0, 115.0f + vert), kParamPitchModPow3, "Mod Pow", SMALLER_LABEL, true, false, -135.0, 135.0, 0.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
    pGraphics->AttachControl(new IVKnobControl(IRECT(925.0, 35.0f + vert, 970.0, 115.0f + vert), kParamPitchOffset3, "Offset", SMALLER_LABEL, true, false, -135.0, 135.0, 0.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));


    pGraphics->AttachControl(new IVNumberBoxControl(IRECT(1000.0, 35.0f + vert, 1075.0, 85.0f + vert), kParamExtraUnison3, nullptr, "Unison", DEFAULT_STYLE, 1.0, 1.0, 8.0, "%0.0f"));
    pGraphics->AttachControl(new IVKnobControl(IRECT(1075.0, 30.0f + vert, 1130.0, 95.0f + vert), kParamExtraDetune3, "Detune", DEFAULT_STYLE, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));


    //////////////// OSC 4

    vert += 110;

    pGraphics->AttachControl(new ITextControl(IRECT(0.0, 25.0f + vert, 50.0, 125.0f + vert), "Type IV", SMALL_TEXT));

    pGraphics->AttachControl(new IVGroupControl(IRECT(170.0, 20.0f + vert, 320.0, 120.0f + vert), "LFO", 10.0, SMALLER_LABEL));
    pGraphics->AttachControl(new IVGroupControl(IRECT(330.0, 20.0f + vert, 500.0, 120.0f + vert), "Mods", 10.0, SMALLER_LABEL));
    pGraphics->AttachControl(new IVGroupControl(IRECT(650.0, 20.0f + vert, 800.0, 120.0f + vert), "LFO", 10.0, SMALLER_LABEL));
    pGraphics->AttachControl(new IVGroupControl(IRECT(810.0, 20.0f + vert, 980.0, 120.0f + vert), "Mods", 10.0, SMALLER_LABEL));

    pGraphics->AttachControl(new IVSliderControl(IRECT(40.0, 35.0f + vert, 80.0, 120.0f + vert), kParamPwmAttack4, "A", SMALLER_LABEL, true, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
    pGraphics->AttachControl(new IVSliderControl(IRECT(70.0, 35.0f + vert, 110.0, 120.0f + vert), kParamPwmDecay4, "D", SMALLER_LABEL, true, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
    pGraphics->AttachControl(new IVSliderControl(IRECT(100.0, 35.0f + vert, 140.0, 120.0f + vert), kParamPwmSustain4, "S", SMALLER_LABEL, true, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
    pGraphics->AttachControl(new IVSliderControl(IRECT(130.0, 35.0f + vert, 170.0, 120.0f + vert), kParamPwmRelease4, "R", SMALLER_LABEL, true, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
    pGraphics->AttachControl(new IVSlideSwitchControl(IRECT(175.0, 60.0f + vert, 195.0, 90.0f + vert), kParamPwmLFORateMode4, "", DEFAULT_STYLE.WithShowValue(false).WithShowLabel(false), false, EDirection::Vertical), kNoTag, "LFO")->SetAnimationEndActionFunction([pGraphics](const IControl* pControl)
      {
        bool sync = pControl->GetValue() > 0.5;
        pGraphics->HideControl(kParamPwmLFORateHz4, sync);
        pGraphics->HideControl(kParamPwmLFORateTempo4, !sync);
      });
    pGraphics->AttachControl(new IVKnobControl(IRECT(205.0, 35.0f + vert, 240.0, 115.0f + vert), kParamPwmLFORateTempo4, "Rate", SMALLER_LABEL, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0))->DisablePrompt(false);
    pGraphics->AttachControl(new IVKnobControl(IRECT(205.0, 35.0f + vert, 240.0, 115.0f + vert), kParamPwmLFORateHz4, "Rate", SMALLER_LABEL, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0))->Hide(true);
    pGraphics->AttachControl(new IVKnobControl(IRECT(240.0, 35.0f + vert, 275.0, 115.0f + vert), kParamPwmLFODepth4, "Depth", SMALLER_LABEL, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
    pGraphics->AttachControl(new IVKnobControl(IRECT(275.0, 35.0f + vert, 310.0, 115.0f + vert), kParamPwmLFOShape4, "Shape", SMALLER_LABEL, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0))->DisablePrompt(false);
    pGraphics->AttachControl(new IVSlideSwitchControl(IRECT(340.0, 60.0f + vert, 360.0, 90.0f + vert), kParamPwmKeyTrack4, "", DEFAULT_STYLE.WithShowValue(false).WithShowLabel(false), true, EDirection::Vertical)); pGraphics->AttachControl(new IVKnobControl(IRECT(390.0, 35.0f + vert, 435.0, 115.0f + vert), kParamPwmModPow4, "Mod Pow", SMALLER_LABEL, true, false, -135.0, 135.0, 67.5, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
    pGraphics->AttachControl(new IVKnobControl(IRECT(445.0, 35.0f + vert, 490.0, 115.0f + vert), kParamPwmOffset4, "Offset", SMALLER_LABEL, true, false, -135.0, 135.0, 0.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));




    pGraphics->AttachControl(new IVSliderControl(IRECT(520.0, 35.0f + vert, 560.0, 120.0f + vert), kParamPitchAttack4, "A", SMALLER_LABEL, true, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
    pGraphics->AttachControl(new IVSliderControl(IRECT(550.0, 35.0f + vert, 590.0, 120.0f + vert), kParamPitchDecay4, "D", SMALLER_LABEL, true, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
    pGraphics->AttachControl(new IVSliderControl(IRECT(580.0, 35.0f + vert, 620.0, 120.0f + vert), kParamPitchSustain4, "S", SMALLER_LABEL, true, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
    pGraphics->AttachControl(new IVSliderControl(IRECT(610.0, 35.0f + vert, 650.0, 120.0f + vert), kParamPitchRelease4, "R", SMALLER_LABEL, true, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
    pGraphics->AttachControl(new IVSlideSwitchControl(IRECT(655.0, 55.0f + vert, 675.0, 85.0f + vert), kParamPitchLFORateMode4, "", DEFAULT_STYLE.WithShowValue(false).WithShowLabel(false), false, EDirection::Vertical), kNoTag, "LFO")->SetAnimationEndActionFunction([pGraphics](const IControl* pControl)
      {
        bool sync = pControl->GetValue() > 0.5;
        pGraphics->HideControl(kParamPitchLFORateHz4, sync);
        pGraphics->HideControl(kParamPitchLFORateTempo4, !sync);
      });
    pGraphics->AttachControl(new IVKnobControl(IRECT(685.0, 35.0f + vert, 720.0, 115.0f + vert), kParamPitchLFORateTempo4, "Rate", SMALLER_LABEL, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0))->DisablePrompt(false);
    pGraphics->AttachControl(new IVKnobControl(IRECT(685.0, 35.0f + vert, 720.0, 115.0f + vert), kParamPitchLFORateHz4, "Rate", SMALLER_LABEL, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0))->Hide(true);
    pGraphics->AttachControl(new IVKnobControl(IRECT(720.0, 35.0f + vert, 755.0, 115.0f + vert), kParamPitchLFODepth4, "Depth", SMALLER_LABEL, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
    pGraphics->AttachControl(new IVKnobControl(IRECT(755.0, 35.0f + vert, 790.0, 115.0f + vert), kParamPitchLFOShape4, "Shape", SMALLER_LABEL, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0))->DisablePrompt(false);
    pGraphics->AttachControl(new IVKnobControl(IRECT(815.0, 35.0f + vert, 860.0, 115.0f + vert), kParamPitchKeyTrack4, "Key Tr.", SMALLER_LABEL, true, false, -135.0, 135.0, 0.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
    pGraphics->AttachControl(new IVKnobControl(IRECT(870.0, 35.0f + vert, 915.0, 115.0f + vert), kParamPitchModPow4, "Mod Pow", SMALLER_LABEL, true, false, -135.0, 135.0, 0.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
    pGraphics->AttachControl(new IVKnobControl(IRECT(925.0, 35.0f + vert, 970.0, 115.0f + vert), kParamPitchOffset4, "Offset", SMALLER_LABEL, true, false, -135.0, 135.0, 0.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));


    pGraphics->AttachControl(new IVNumberBoxControl(IRECT(1000.0, 35.0f + vert, 1075.0, 85.0f + vert), kParamExtraUnison4, nullptr, "Unison", DEFAULT_STYLE, 1.0, 1.0, 8.0, "%0.0f"));
    pGraphics->AttachControl(new IVKnobControl(IRECT(1075.0, 30.0f + vert, 1130.0, 95.0f + vert), kParamExtraDetune4, "Detune", DEFAULT_STYLE, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));

  };

    /// End of Insert ////////////
    //////////////////////////////
     
    // pGraphics->AttachControl(new IVSliderControl(IRECT(40.0, 35.0, 80.0, 120.0), kParamPwmAttack1, "A", DEFAULT_STYLE, false, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
    // pGraphics->AttachControl(new IVSliderControl(IRECT(70.0, 35.0, 110.0, 120.0), kParamPwmDecay1, "D", DEFAULT_STYLE, false, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
    // pGraphics->AttachControl(new IVSliderControl(IRECT(100.0, 35.0, 140.0, 120.0), kParamPwmSustain1, "S", DEFAULT_STYLE, false, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
    // pGraphics->AttachControl(new IVSliderControl(IRECT(130.0, 35.0, 170.0, 120.0), kParamPwmRelease1, "R", DEFAULT_STYLE, false, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
    // pGraphics->AttachControl(new IVLabelControl(IRECT(0.0, 70.0, 25.0, 95.0), "1", LARGER_LABEL));
    // pGraphics->AttachControl(new IVLabelControl(IRECT(0.0, 175.0, 25.0, 200.0), "2", LARGER_LABEL));
    // pGraphics->AttachControl(new IVLabelControl(IRECT(0.0, 275.0, 25.0, 300.0), "3", LARGER_LABEL));
    // pGraphics->AttachControl(new IVLabelControl(IRECT(0.0, 375.0, 25.0, 400.0), "4", LARGER_LABEL));
    // pGraphics->AttachControl(new IVLabelControl(IRECT(0.0, 475.0, 25.0, 500.0), "5", LARGER_LABEL));
    // pGraphics->AttachControl(new IVLabelControl(IRECT(0.0, 575.0, 25.0, 600.0), "6", LARGER_LABEL));
    // 
    // pGraphics->AttachControl(new IVKnobControl(IRECT(205.0, 35.0, 240.0, 115.0), kParamPwmLFORateHz1, "Rate", SMALLER_LABEL, false, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0))->Hide(true);
    // pGraphics->AttachControl(new IVKnobControl(IRECT(205.0, 35.0, 240.0, 115.0), kParamPwmLFORateTempo1, "Rate", SMALLER_LABEL, false, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0))->DisablePrompt(false);;
    // pGraphics->AttachControl(new IVKnobControl(IRECT(240.0, 35.0, 275.0, 115.0), kParamPwmLFODepth1, "Depth", SMALLER_LABEL, false, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
    // pGraphics->AttachControl(new IVKnobControl(IRECT(275.0, 35.0, 310.0, 115.0), kParamPwmLFOShape1, "Shape", SMALLER_LABEL, false, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0))->DisablePrompt(false);;
    // 
    // pGraphics->AttachControl(new IVSlideSwitchControl(IRECT(175.0, 60.0, 195.0, 90.0), kParamPwmLFORateMode1, "", DEFAULT_STYLE.WithShowValue(false).WithShowLabel(false), false, EDirection::Vertical), kNoTag, "LFO")->SetAnimationEndActionFunction([pGraphics](const IControl* pControl)
    //   {
    //     bool sync = pControl->GetValue() > 0.5;
    //     pGraphics->HideControl(kParamPwmLFORateHz1, sync);
    //     pGraphics->HideControl(kParamPwmLFORateTempo1, !sync);
    //   });
    // 
    // pGraphics->AttachControl(new IVGroupControl(IRECT(170.0, 20.0, 320.0, 120.0), "LFO", 10.0, SMALLER_LABEL));
    // pGraphics->AttachControl(new IVGroupControl(IRECT(330.0, 20.0, 500.0, 120.0), "Mods", 10.0, SMALLER_LABEL));
    // pGraphics->AttachControl(new IVGroupControl(IRECT(650.0, 20.0, 800.0, 120.0), "LFO", 10.0, SMALLER_LABEL));
    // pGraphics->AttachControl(new IVGroupControl(IRECT(810.0, 20.0, 980.0, 120.0), "Mods", 10.0, SMALLER_LABEL));
    // pGraphics->AttachControl(new IVSliderControl(IRECT(40.0, 35.0, 80.0, 120.0), kParamPwmAttack1, "A", DEFAULT_STYLE, true, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
    // pGraphics->AttachControl(new IVSliderControl(IRECT(70.0, 35.0, 110.0, 120.0), kParamPwmDecay1, "D", DEFAULT_STYLE, true, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
    // pGraphics->AttachControl(new IVSliderControl(IRECT(100.0, 35.0, 140.0, 120.0), kParamPwmSustain1, "S", DEFAULT_STYLE, true, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
    // pGraphics->AttachControl(new IVSliderControl(IRECT(130.0, 35.0, 170.0, 120.0), kParamPwmRelease1, "R", DEFAULT_STYLE, true, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
    // pGraphics->AttachControl(new IVKnobControl(IRECT(205.0, 35.0, 240.0, 115.0), kParamPwmLFORateHz1, "Rate", SMALLER_LABEL, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
    // pGraphics->AttachControl(new IVKnobControl(IRECT(205.0, 35.0, 240.0, 115.0), kParamPwmLFORateTempo1, "Rate", SMALLER_LABEL, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
    // pGraphics->AttachControl(new IVKnobControl(IRECT(240.0, 35.0, 275.0, 115.0), kParamPwmLFODepth1, "Depth", SMALLER_LABEL, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
    // pGraphics->AttachControl(new IVKnobControl(IRECT(275.0, 35.0, 310.0, 115.0), kParamPwmLFOShape1, "Shape", SMALLER_LABEL, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
    // pGraphics->AttachControl(new IVKnobControl(IRECT(335.0, 35.0, 380.0, 115.0), kParamPwmKeyTrack1, "Key Tr.", SMALLER_LABEL, true, false, -135.0, 135.0, 0.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
    // pGraphics->AttachControl(new IVKnobControl(IRECT(390.0, 35.0, 435.0, 115.0), kParamPwmModPow1, "Mod Wh.", SMALLER_LABEL, true, false, -135.0, 135.0, 0.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
    // pGraphics->AttachControl(new IVKnobControl(IRECT(445.0, 35.0, 490.0, 115.0), kParamPwmOffset1, "Offset", SMALLER_LABEL, true, false, -135.0, 135.0, 0.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
    // pGraphics->AttachControl(new IVSliderControl(IRECT(520.0, 35.0, 560.0, 120.0), kParamPitchAttack1, "A", DEFAULT_STYLE, true, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
    // pGraphics->AttachControl(new IVSliderControl(IRECT(550.0, 35.0, 590.0, 120.0), kParamPitchDecay1, "D", DEFAULT_STYLE, true, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
    // pGraphics->AttachControl(new IVSliderControl(IRECT(580.0, 35.0, 620.0, 120.0), kParamPitchSustain1, "S", DEFAULT_STYLE, true, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
    // pGraphics->AttachControl(new IVSliderControl(IRECT(610.0, 35.0, 650.0, 120.0), kParamPitchRelease1, "R", DEFAULT_STYLE, true, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
    // pGraphics->AttachControl(new IVKnobControl(IRECT(685.0, 35.0, 720.0, 115.0), kParamPitchLFORateTempo1, "Rate", SMALLER_LABEL, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
    // pGraphics->AttachControl(new IVKnobControl(IRECT(720.0, 35.0, 755.0, 115.0), kParamPitchLFORateDepth1, "Depth", SMALLER_LABEL, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
    // pGraphics->AttachControl(new IVKnobControl(IRECT(755.0, 35.0, 790.0, 115.0), kParamPitchLFOShape1, "Shape", SMALLER_LABEL, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
    // pGraphics->AttachControl(new IVKnobControl(IRECT(685.0, 35.0, 720.0, 115.0), kParamPitchLFORateHz1, "Rate", SMALLER_LABEL, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
    // 
    // pGraphics->AttachControl(new IVKnobControl(IRECT(815.0, 35.0, 860.0, 115.0), kParamPitchKeyTrack1, "Key Tr.", SMALLER_LABEL, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
    // pGraphics->AttachControl(new IVKnobControl(IRECT(870.0, 35.0, 915.0, 115.0), kParamPitchModPow1, "Mod Wh.", SMALLER_LABEL, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
    // pGraphics->AttachControl(new IVKnobControl(IRECT(925.0, 35.0, 970.0, 115.0), kParamPitchOffset1, "Offset", SMALLER_LABEL, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
    // pGraphics->AttachControl(new IVNumberBoxControl(IRECT(1000.0, 35.0, 1075.0, 85.0), kParamExtraUnison1, nullptr, "Unison", DEFAULT_STYLE, 1.0, 1.0, 8.0, "%0.0f"));
    // pGraphics->AttachControl(new IVKnobControl(IRECT(1075.0, 30.0, 1130.0, 95.0), kParamExtraDetune1, "Detune", DEFAULT_STYLE, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));


#endif
}



#if IPLUG_DSP
void VaiaOneBitPlus::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  mDSP.ProcessBlock(nullptr, outputs, 2, nFrames, mTimeInfo.mPPQPos, mTimeInfo.mTransportIsRunning);
}

void VaiaOneBitPlus::OnIdle()
{

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
