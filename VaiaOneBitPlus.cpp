#include "VaiaOneBitPlus.h"
#include "IPlug_include_in_plug_src.h"
#include "LFO.h"

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
  mMakeGraphicsFunc = [&]()
    {
      return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
    };

  mLayoutFunc = [&](IGraphics* pGraphics)
    {
      pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
      pGraphics->AttachPanelBackground(COLOR_GRAY);
      pGraphics->EnableMouseOver(true);
      pGraphics->EnableMultiTouch(true);


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

      mLayoutFunc = [&](IGraphics* pGraphics)
        {
          pGraphics->AttachControl(new IVGroupControl(IRECT(170.0, 20.0, 320.0, 120.0), "LFO", 10.0, SMALLER_LABEL));
          pGraphics->AttachControl(new IVGroupControl(IRECT(330.0, 20.0, 500.0, 120.0), "Mods", 10.0, SMALLER_LABEL));
          pGraphics->AttachControl(new IVGroupControl(IRECT(650.0, 25.0, 800.0, 125.0), "LFO", 10.0, SMALLER_LABEL));
          pGraphics->AttachControl(new IVGroupControl(IRECT(810.0, 25.0, 980.0, 125.0), "Mods", 10.0, SMALLER_LABEL));
          pGraphics->AttachControl(new IVSliderControl(IRECT(40.0, 35.0, 80.0, 120.0), kParamEnvAttack1, "A", DEFAULT_STYLE, false, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
          pGraphics->AttachControl(new IVSliderControl(IRECT(70.0, 35.0, 110.0, 120.0), kParamEnvDecay1, "D", DEFAULT_STYLE, false, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
          pGraphics->AttachControl(new IVSliderControl(IRECT(100.0, 35.0, 140.0, 120.0), kParamEnvSustain1, "S", DEFAULT_STYLE, false, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
          pGraphics->AttachControl(new IVSliderControl(IRECT(130.0, 35.0, 170.0, 120.0), kParamEnvRelease1, "R", DEFAULT_STYLE, false, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
          pGraphics->AttachControl(new IVLabelControl(IRECT(0.0, 70.0, 25.0, 95.0), "1", LARGER_LABEL));
          pGraphics->AttachControl(new IVLabelControl(IRECT(0.0, 175.0, 25.0, 200.0), "2", LARGER_LABEL));
          pGraphics->AttachControl(new IVLabelControl(IRECT(0.0, 275.0, 25.0, 300.0), "3", LARGER_LABEL));
          pGraphics->AttachControl(new IVLabelControl(IRECT(0.0, 375.0, 25.0, 400.0), "4", LARGER_LABEL));
          pGraphics->AttachControl(new IVLabelControl(IRECT(0.0, 475.0, 25.0, 500.0), "5", LARGER_LABEL));
          pGraphics->AttachControl(new IVLabelControl(IRECT(0.0, 575.0, 25.0, 600.0), "6", LARGER_LABEL));
          pGraphics->AttachControl(new IVKnobControl(IRECT(205.0, 35.0, 240.0, 115.0), kNoParameter, "Rate", SMALLER_LABEL, false, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
          pGraphics->AttachControl(new IVKnobControl(IRECT(240.0, 35.0, 275.0, 115.0), kNoParameter, "Depth", SMALLER_LABEL, false, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
          pGraphics->AttachControl(new IVKnobControl(IRECT(275.0, 35.0, 310.0, 115.0), kNoParameter, "Shape", SMALLER_LABEL, false, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
          pGraphics->AttachControl(new IVLabelControl(IRECT(35.0, 0.0, 170.0, 25.0), "Envelope Control", LARGER_LABEL));
          pGraphics->AttachControl(new IVLabelControl(IRECT(515.0, 0.0, 650.0, 25.0), "Pitch Control", LARGER_LABEL));
          pGraphics->AttachControl(new IVSliderControl(IRECT(40.0, 145.0, 80.0, 230.0), kNoParameter, "A", DEFAULT_STYLE, false, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
          pGraphics->AttachControl(new IVSliderControl(IRECT(70.0, 145.0, 110.0, 230.0), kNoParameter, "D", DEFAULT_STYLE, false, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
          pGraphics->AttachControl(new IVSliderControl(IRECT(100.0, 145.0, 140.0, 230.0), kNoParameter, "S", DEFAULT_STYLE, false, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
          pGraphics->AttachControl(new IVSliderControl(IRECT(130.0, 145.0, 170.0, 230.0), kNoParameter, "R", DEFAULT_STYLE, false, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
          pGraphics->AttachControl(new IVKnobControl(IRECT(185.0, 145.0, 220.0, 225.0), kNoParameter, "Rate", DEFAULT_STYLE, false, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
          pGraphics->AttachControl(new IVKnobControl(IRECT(225.0, 145.0, 260.0, 225.0), kNoParameter, "Depth", DEFAULT_STYLE, false, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
          pGraphics->AttachControl(new IVKnobControl(IRECT(265.0, 145.0, 300.0, 225.0), kNoParameter, "Shape", DEFAULT_STYLE, false, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
          pGraphics->AttachControl(new IVSliderControl(IRECT(40.0, 255.0, 80.0, 340.0), kNoParameter, "A", DEFAULT_STYLE, false, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
          pGraphics->AttachControl(new IVSliderControl(IRECT(70.0, 255.0, 110.0, 340.0), kNoParameter, "D", DEFAULT_STYLE, false, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
          pGraphics->AttachControl(new IVSliderControl(IRECT(100.0, 255.0, 140.0, 340.0), kNoParameter, "S", DEFAULT_STYLE, false, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
          pGraphics->AttachControl(new IVSliderControl(IRECT(130.0, 255.0, 170.0, 340.0), kNoParameter, "R", DEFAULT_STYLE, false, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
          pGraphics->AttachControl(new IVKnobControl(IRECT(185.0, 255.0, 220.0, 335.0), kNoParameter, "Rate", DEFAULT_STYLE, false, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
          pGraphics->AttachControl(new IVKnobControl(IRECT(225.0, 255.0, 260.0, 335.0), kNoParameter, "Depth", DEFAULT_STYLE, false, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
          pGraphics->AttachControl(new IVKnobControl(IRECT(265.0, 255.0, 300.0, 335.0), kNoParameter, "Shape", DEFAULT_STYLE, false, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
          pGraphics->AttachControl(new IVSliderControl(IRECT(40.0, 365.0, 80.0, 450.0), kNoParameter, "A", DEFAULT_STYLE, false, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
          pGraphics->AttachControl(new IVSliderControl(IRECT(70.0, 365.0, 110.0, 450.0), kNoParameter, "D", DEFAULT_STYLE, false, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
          pGraphics->AttachControl(new IVSliderControl(IRECT(100.0, 365.0, 140.0, 450.0), kNoParameter, "S", DEFAULT_STYLE, false, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
          pGraphics->AttachControl(new IVSliderControl(IRECT(130.0, 365.0, 170.0, 450.0), kNoParameter, "R", DEFAULT_STYLE, false, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
          pGraphics->AttachControl(new IVKnobControl(IRECT(185.0, 365.0, 220.0, 445.0), kNoParameter, "Rate", DEFAULT_STYLE, false, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
          pGraphics->AttachControl(new IVKnobControl(IRECT(225.0, 365.0, 260.0, 445.0), kNoParameter, "Depth", DEFAULT_STYLE, false, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
          pGraphics->AttachControl(new IVKnobControl(IRECT(265.0, 365.0, 300.0, 445.0), kNoParameter, "Shape", DEFAULT_STYLE, false, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
          pGraphics->AttachControl(new IVSliderControl(IRECT(40.0, 475.0, 80.0, 560.0), kNoParameter, "A", DEFAULT_STYLE, false, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
          pGraphics->AttachControl(new IVSliderControl(IRECT(70.0, 475.0, 110.0, 560.0), kNoParameter, "D", DEFAULT_STYLE, false, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
          pGraphics->AttachControl(new IVSliderControl(IRECT(100.0, 475.0, 140.0, 560.0), kNoParameter, "S", DEFAULT_STYLE, false, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
          pGraphics->AttachControl(new IVSliderControl(IRECT(130.0, 475.0, 170.0, 560.0), kNoParameter, "R", DEFAULT_STYLE, false, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
          pGraphics->AttachControl(new IVKnobControl(IRECT(185.0, 475.0, 220.0, 555.0), kNoParameter, "Rate", DEFAULT_STYLE, false, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
          pGraphics->AttachControl(new IVKnobControl(IRECT(225.0, 475.0, 260.0, 555.0), kNoParameter, "Depth", DEFAULT_STYLE, false, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
          pGraphics->AttachControl(new IVKnobControl(IRECT(265.0, 475.0, 300.0, 555.0), kNoParameter, "Shape", DEFAULT_STYLE, false, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
          pGraphics->AttachControl(new IVSliderControl(IRECT(40.0, 590.0, 80.0, 675.0), kNoParameter, "A", DEFAULT_STYLE, false, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
          pGraphics->AttachControl(new IVSliderControl(IRECT(70.0, 590.0, 110.0, 675.0), kNoParameter, "D", DEFAULT_STYLE, false, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
          pGraphics->AttachControl(new IVSliderControl(IRECT(100.0, 590.0, 140.0, 675.0), kNoParameter, "S", DEFAULT_STYLE, false, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
          pGraphics->AttachControl(new IVSliderControl(IRECT(130.0, 590.0, 170.0, 675.0), kNoParameter, "R", DEFAULT_STYLE, false, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
          pGraphics->AttachControl(new IVKnobControl(IRECT(185.0, 590.0, 220.0, 670.0), kNoParameter, "Rate", DEFAULT_STYLE, false, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
          pGraphics->AttachControl(new IVKnobControl(IRECT(225.0, 590.0, 260.0, 670.0), kNoParameter, "Depth", DEFAULT_STYLE, false, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
          pGraphics->AttachControl(new IVKnobControl(IRECT(265.0, 590.0, 300.0, 670.0), kNoParameter, "Shape", DEFAULT_STYLE, false, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
          pGraphics->AttachControl(new IVKnobControl(IRECT(335.0, 35.0, 380.0, 115.0), kNoParameter, "Key Tr.", SMALLER_LABEL, false, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
          pGraphics->AttachControl(new IVKnobControl(IRECT(390.0, 35.0, 435.0, 115.0), kNoParameter, "Mod Wh.", SMALLER_LABEL, false, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
          pGraphics->AttachControl(new IVSlideSwitchControl(IRECT(175.0, 60.0, 195.0, 90.0), EmptyClickActionFunc, "", DEFAULT_STYLE, true, EDirection::Vertical, 2, 0));
          pGraphics->AttachControl(new IVKnobControl(IRECT(445.0, 35.0, 490.0, 115.0), kNoParameter, "Offset", SMALLER_LABEL, false, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
          pGraphics->AttachControl(new IVSliderControl(IRECT(520.0, 40.0, 560.0, 125.0), kNoParameter, "A", DEFAULT_STYLE, false, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
          pGraphics->AttachControl(new IVSliderControl(IRECT(550.0, 40.0, 590.0, 125.0), kNoParameter, "D", DEFAULT_STYLE, false, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
          pGraphics->AttachControl(new IVSliderControl(IRECT(580.0, 40.0, 620.0, 125.0), kNoParameter, "S", DEFAULT_STYLE, false, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
          pGraphics->AttachControl(new IVSliderControl(IRECT(610.0, 40.0, 650.0, 125.0), kNoParameter, "R", DEFAULT_STYLE, false, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
          pGraphics->AttachControl(new IVKnobControl(IRECT(685.0, 40.0, 720.0, 120.0), kNoParameter, "Rate", SMALLER_LABEL, false, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
          pGraphics->AttachControl(new IVKnobControl(IRECT(720.0, 40.0, 755.0, 120.0), kNoParameter, "Depth", SMALLER_LABEL, false, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
          pGraphics->AttachControl(new IVKnobControl(IRECT(755.0, 40.0, 790.0, 120.0), kNoParameter, "Shape", SMALLER_LABEL, false, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
          pGraphics->AttachControl(new IVKnobControl(IRECT(815.0, 40.0, 860.0, 120.0), kNoParameter, "Key Tr.", SMALLER_LABEL, false, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
          pGraphics->AttachControl(new IVKnobControl(IRECT(870.0, 40.0, 915.0, 120.0), kNoParameter, "Mod Wh.", SMALLER_LABEL, false, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
          pGraphics->AttachControl(new IVSlideSwitchControl(IRECT(655.0, 65.0, 675.0, 95.0), EmptyClickActionFunc, "", DEFAULT_STYLE, true, EDirection::Vertical, 2, 0));
          pGraphics->AttachControl(new IVKnobControl(IRECT(925.0, 40.0, 970.0, 120.0), kNoParameter, "Offset", SMALLER_LABEL, false, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
          pGraphics->AttachControl(new IVLabelControl(IRECT(995.0, 0.0, 1130.0, 25.0), "Extras", LARGER_LABEL));
          pGraphics->AttachControl(new IVNumberBoxControl(IRECT(1000.0, 40.0, 1075.0, 90.0), kNoParameter, nullptr, "Unison", DEFAULT_STYLE, 50.0, 1.0, 4.0, "%0.0f"));
          pGraphics->AttachControl(new IVKnobControl(IRECT(1075.0, 35.0, 1130.0, 100.0), kNoParameter, "Detune", DEFAULT_STYLE, false, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
        };


// #ifdef OS_WEB
//       pGraphics->AttachPopupMenuControl();
// #endif
// 
//       //    pGraphics->EnableLiveEdit(true);
//       pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);
//       const IRECT b = pGraphics->GetBounds().GetPadded(-20.f);
//       const IRECT lfoPanel = b.GetFromLeft(150.f).GetFromTop(125.f);
//       const IRECT oscilloscopePanel = b.GetFromLeft(400.f).GetFromTop(125.f).GetReducedFromLeft(180.f).GetReducedFromRight(50.f).GetReducedFromTop(-10.f).GetReducedFromBottom(-10.f);
//       IRECT keyboardBounds = b.GetFromBottom(150);
//       IRECT wheelsBounds = keyboardBounds.ReduceFromLeft(100.f).GetPadded(-10.f);
//       pGraphics->AttachControl(new IVKeyboardControl(keyboardBounds), kCtrlTagKeyboard);
//       pGraphics->AttachControl(new IWheelControl(wheelsBounds.FracRectHorizontal(0.5)), kCtrlTagBender);
//       pGraphics->AttachControl(new IWheelControl(wheelsBounds.FracRectHorizontal(0.5, true), IMidiMsg::EControlChangeMsg::kModWheel));
//       //    pGraphics->AttachControl(new IVMultiSliderControl<4>(b.GetGridCell(0, 2, 2).GetPadded(-30), "", DEFAULT_STYLE, kParamEnvAttack1, EDirection::Vertical, 0.f, 1.f));
//       const IRECT controls = b.GetFromLeft(400.f).GetReducedFromLeft(500.f).GetFromTop(150.f);
//       pGraphics->AttachControl(new IVKnobControl(controls.GetGridCell(0, 2, 4).GetReducedFromLeft(50.f).GetCentredInside(90), kParamGain, "Gain"));
//       pGraphics->AttachControl(new IVKnobControl(controls.GetGridCell(2, 2, 4).GetCentredInside(90), kParamNoteGlideTime, "Glide"));
// 
//       pGraphics->AttachControl(new IVLEDMeterControl<2>(controls.GetFromRight(35).GetPadded(-5)), kCtrlTagMeter);
// 
//       pGraphics->AttachControl(new IVKnobControl(lfoPanel.GetGridCell(0, 0, 2, 3).GetCentredInside(60), kParamLFORateHz, "Rate"), kNoTag, "LFO")->Hide(true);
//       pGraphics->AttachControl(new IVKnobControl(lfoPanel.GetGridCell(0, 0, 2, 3).GetCentredInside(60), kParamLFORateTempo, "Rate"), kNoTag, "LFO")->DisablePrompt(false);
//       pGraphics->AttachControl(new IVKnobControl(lfoPanel.GetGridCell(0, 1, 2, 3).GetCentredInside(60), kParamLFODepth, "Depth"), kNoTag, "LFO");
//       pGraphics->AttachControl(new IVKnobControl(lfoPanel.GetGridCell(0, 2, 2, 3).GetCentredInside(60), kParamLFOShape, "Shape"), kNoTag, "LFO")->DisablePrompt(false);
//       pGraphics->AttachControl(new IVSlideSwitchControl(lfoPanel.GetGridCell(1, 0, 2, 3).GetFromTop(30).GetMidHPadded(20), kParamLFORateMode, "Sync", DEFAULT_STYLE.WithShowValue(false).WithShowLabel(false).WithWidgetFrac(0.5f).WithDrawShadows(false), false), kNoTag, "LFO")->SetAnimationEndActionFunction([pGraphics](IControl* pControl)
//         {
//           bool sync = pControl->GetValue() > 0.5;
//           pGraphics->HideControl(kParamLFORateHz, sync);
//           pGraphics->HideControl(kParamLFORateTempo, !sync);
//         });
//       pGraphics->AttachControl(new IVDisplayControl(oscilloscopePanel.GetGridCell(0, 0, 1, 1), "", DEFAULT_STYLE, EDirection::Horizontal, -1.f, 1.f, 0.f, 128), kCtrlTagOscilloscope, "Oscilloscope");
// 
//       pGraphics->AttachControl(new IVDisplayControl(lfoPanel.GetGridCell(1, 1, 2, 3).Union(lfoPanel.GetGridCell(1, 2, 2, 3)), "", DEFAULT_STYLE, EDirection::Horizontal, 0.f, 1.f, 0.f, 1024), kCtrlTagLFOVis, "LFO");
// 
//       pGraphics->AttachControl(new IVGroupControl("LFO", "LFO", 10.f, 20.f, 10.f, 10.f));
// 
//       pGraphics->AttachControl(new IVButtonControl(keyboardBounds.GetFromTRHC(200, 30).GetTranslated(0, -60), SplashClickActionFunc,
//         "Show/Hide Keyboard", DEFAULT_STYLE.WithColor(kFG, COLOR_WHITE).WithLabelText({ 15.f, EVAlign::Middle })))->SetAnimationEndActionFunction(
//           [pGraphics](IControl* pCaller)
//           {
//             static bool hide = false;
//             pGraphics->GetControlWithTag(kCtrlTagKeyboard)->Hide(hide = !hide);
//             pGraphics->Resize(PLUG_WIDTH, hide ? PLUG_HEIGHT / 2 : PLUG_HEIGHT, pGraphics->GetDrawScale());
//           });
// #ifdef OS_IOS
//       if (!IsOOPAuv3AppExtension())
//       {
//         pGraphics->AttachControl(new IVButtonControl(b.GetFromTRHC(100, 100), [pGraphics](IControl* pCaller)
//           {
//             dynamic_cast<IGraphicsIOS*>(pGraphics)->LaunchBluetoothMidiDialog(pCaller->GetRECT().L, pCaller->GetRECT().MH());
//             SplashClickActionFunc(pCaller);
//           }, "BTMIDI"));
//       }
// #endif
// 
//       pGraphics->SetQwertyMidiKeyHandlerFunc([pGraphics](const IMidiMsg& msg)
//         {
//           pGraphics->GetControlWithTag(kCtrlTagKeyboard)->As<IVKeyboardControl>()->SetNoteFromMidi(msg.NoteNumber(), msg.StatusMsg() == IMidiMsg::kNoteOn);
//         });
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

  int status = msg.StatusMsg();

  switch (status)
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
  if (ctrlTag == kCtrlTagBender && msgTag == IWheelControl::kMessageTagSetPitchBendRange)
  {
    const int bendRange = *static_cast<const int*>(pData);
    mDSP.mSynth.SetPitchBendRange(bendRange);
  }

  return false;
}
#endif
