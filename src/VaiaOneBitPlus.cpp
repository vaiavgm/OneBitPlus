#include "src/VaiaOneBitPlus.h"
#include <IPlug_include_in_plug_src.h>

#include <IControls.h>
#include <IVDisplayControl.h>
#include <IGraphicsStructs.h>
#include <IPlugLogger.h>
#include <LFO.h>

#include "src/SampleLoader.h"
#include "src/SampleData.h"
#include "src/PipelinePresets.h"
#include "src/StateFormat.h"
#include "src/Debug.h"

VaiaOneBitPlus::VaiaOneBitPlus(const InstanceInfo& info)
  : Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  GetParam(kParamGain)->InitDouble("Gain", 50., 0., 100.0, 0.01, "%");
  GetParam(kParamNoteGlideTime)->InitMilliseconds("Note Glide Time", 0., 0.0, 150.);

  GetParam(kInputDither)->InitPercentage("Input Dither", 0., 0., 100.);
  GetParam(kInputProtect)->InitPercentage("Input Protection", 0., 0., 100.);

  // Instrument types I-IV: identical parameter blocks (see Params.h). Ranges, defaults and the order of
  // the parameters are unchanged from the original per-type code. Only the display names got a "T<n>" prefix,
  // so host automation lanes can be told apart. Parameter indices (what hosts store) are unchanged.
  for (int t = 0; t < kNumTypes; ++t)
  {
    auto P = [this, t](int type1Param) { return GetParam(TypeParam(t, type1Param)); };
    const std::string prefix = "T" + std::to_string(t + 1) + " ";
    auto N = [&prefix](const char* name) { return prefix + name; };

    P(kParamPwmAttack1)->InitDouble(N("PWM Attack").c_str(), 0., 0., 2000., 0.1, "ms", IParam::kFlagsNone, "ADSR", IParam::ShapePowCurve(3.));
    P(kParamPwmDecay1)->InitDouble(N("PWM Decay").c_str(), 0., 0., 2000., 0.1, "ms", IParam::kFlagsNone, "ADSR", IParam::ShapePowCurve(3.));
    P(kParamPwmSustain1)->InitDouble(N("PWM Sustain").c_str(), 0., 0., 100., 1, "%", IParam::kFlagsNone, "ADSR");
    P(kParamPwmRelease1)->InitDouble(N("PWM Release").c_str(), 0., 0., 500., 0.1, "ms", IParam::kFlagsNone, "ADSR");
    P(kParamPwmLFOShape1)->InitEnum(N("PWM LFO Shape").c_str(), LFO<>::kTriangle, {LFO_SHAPE_VALIST});
    P(kParamPwmLFORateHz1)->InitFrequency(N("PWM LFO Rate Hz").c_str(), 1., 0.01, 40.);
    P(kParamPwmLFORateTempo1)->InitEnum(N("PWM LFO Rate Sync").c_str(), LFO<>::k1, {LFO_TEMPODIV_VALIST});
    P(kParamPwmLFORateMode1)->InitBool(N("PWM LFO Sync").c_str(), true);
    P(kParamPwmLFODepth1)->InitPercentage(N("PWM LFO Depth").c_str());
    P(kParamPwmModPow1)->InitDouble(N("PWM Mod").c_str(), 1.0, -2.0, 2.0, 0.05);
    P(kParamPwmOffset1)->InitDouble(N("PWM Offset").c_str(), 0.5, 0.0025, 0.9975, 0.01);
    P(kParamPwmKeyTrack1)->InitBool(N("PWM Keytrack").c_str(), false);

    P(kParamPitchAttack1)->InitDouble(N("Pitch Attack").c_str(), 0., 0., 2000., 0.1, "ms", IParam::kFlagsNone, "ADSR", IParam::ShapePowCurve(3.));
    P(kParamPitchDecay1)->InitDouble(N("Pitch Decay").c_str(), 0., 0., 2000., 0.1, "ms", IParam::kFlagsNone, "ADSR", IParam::ShapePowCurve(3.));
    P(kParamPitchSustain1)->InitDouble(N("Pitch Sustain").c_str(), 0., 0., 100., 1, "%", IParam::kFlagsNone, "ADSR");
    P(kParamPitchRelease1)->InitDouble(N("Pitch Release").c_str(), 0., 0., 500., 0.1, "ms", IParam::kFlagsNone, "ADSR");
    P(kParamPitchLFOShape1)->InitEnum(N("Pitch LFO Shape").c_str(), LFO<>::kTriangle, {LFO_SHAPE_VALIST});
    P(kParamPitchLFORateHz1)->InitFrequency(N("Pitch LFO Rate Hz").c_str(), 1., 0.01, 40.);
    P(kParamPitchLFORateTempo1)->InitEnum(N("Pitch LFO Rate Sync").c_str(), LFO<>::k1, {LFO_TEMPODIV_VALIST});
    P(kParamPitchLFORateMode1)->InitBool(N("Pitch LFO Sync").c_str(), true);
    P(kParamPitchLFODepth1)->InitPercentage(N("Pitch LFO Depth").c_str());
    P(kParamPitchModPow1)->InitDouble(N("Pitch Mod").c_str(), 1, -4.0, 4.0, 0.05);
    P(kParamPitchOffset1)->InitDouble(N("Pitch Offset").c_str(), 0.0, -1.0, 1.0, 0.01);
    P(kParamPitchKeyTrack1)->InitDouble(N("Pitch Keytrack").c_str(), 0.0, -2.0, 2.0, 0.05);
    P(kParamExtraUnison1)->InitInt(N("Extra Unison").c_str(), 1, 1, 8);
    P(kParamExtraDetune1)->InitDouble(N("Extra Detune").c_str(), 0.0, 0.0, 100.0, 0.5);
  }

  using namespace igraphics;


#if IPLUG_EDITOR // http://bit.ly/2S64BDd


  mMakeGraphicsFunc = [this]() { return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT)); };


  mLayoutFunc = [&](IGraphics* pGraphics) {
    pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
    pGraphics->AttachPanelBackground(COLOR_GRAY);
    pGraphics->EnableMouseOver(true);
    pGraphics->EnableMultiTouch(true);

    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);

    // All styles share colours and geometry and differ only in label/value font size
    auto MakeStyle = [](float labelSize, float valueSize) {
      return IVStyle{
        true, // Show label
        true, // Show value
        {
          COLOR_TRANSPARENT,   // Background
          COLOR_MID_GRAY,      // Foreground
          COLOR_LIGHT_GRAY,    // Pressed
          COLOR_DARK_GRAY,     // Frame
          COLOR_TRANSLUCENT,   // Highlight
          IColor(60, 0, 0, 0), // Shadow
          COLOR_BLACK,         // Extra 1
          COLOR_GREEN,         // Extra 2
          COLOR_BLUE           // Extra 3
        },
        IText(labelSize, COLOR_BLACK, "Roboto-Regular", EAlign::Center, EVAlign::Top, 0.0),    // Label text
        IText(valueSize, COLOR_BLACK, "Roboto-Regular", EAlign::Center, EVAlign::Bottom, 0.0), // Value text
        true,  // Hide cursor
        true,  // Draw frame
        true,  // Draw shadows
        false, // Emboss
        0.0,   // Roundness
        1.0,   // Frame thickness
        3.0,   // Shadow offset
        1.0,   // Widget frac
        0.0    // Angle
      };
    };

    const IVStyle DEFAULT_STYLE = MakeStyle(19.0, 14.0);
    const IVStyle SMALLER_LABEL = MakeStyle(14.0, 10.0);

    const IText HEADER_TEXT{20.0, "Roboto-Regular"};

    const IText SMALL_TEXT{12.0, "Roboto-Regular"};


    //////////////////////////////
    /// Insert GUI from Editor ///


    //////////////// HEADER LABELS
    pGraphics->AttachControl(new ITextControl(IRECT(170.0, 0.0f, 320.0, 20.0f), "PWM Control", HEADER_TEXT));
    pGraphics->AttachControl(new ITextControl(IRECT(570.0, 0.0f, 720.0, 20.0f), "Pitch Control", HEADER_TEXT));
    pGraphics->AttachControl(new ITextControl(IRECT(1070.0, 0.0f, 1220.0, 20.0f), "Extras", HEADER_TEXT));


    pGraphics->AttachControl(new IVSliderControl(IRECT(00.0, 35.0f + 440, 80.0, 120.0f + 440), kInputDither, "Dither", SMALLER_LABEL, true, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
    pGraphics->AttachControl(
      new IVSlideSwitchControl(IRECT(340.0, 60.0f + 440, 360.0, 90.0f + 440), kInputProtect, "Input Protection", DEFAULT_STYLE.WithShowLabel(false), true, EDirection::Vertical));

    //////////////// INSTRUMENT ROWS (Type I at the bottom, Type IV at the top)
    static const char* kTypeNames[kNumTypes] = {"Type I", "Type II", "Type III", "Type IV"};
    for (int t = 0; t < kNumTypes; ++t)
    {
      const float vert = 330.0f - 110.0f * t;


      pGraphics->AttachControl(new ITextControl(IRECT(0.0, 25.0f + vert, 50.0, 125.0f + vert), kTypeNames[t], SMALL_TEXT));

      pGraphics->AttachControl(new IVGroupControl(IRECT(170.0, 20.0f + vert, 320.0, 120.0f + vert), "LFO", 10.0, SMALLER_LABEL));
      pGraphics->AttachControl(new IVGroupControl(IRECT(330.0, 20.0f + vert, 500.0, 120.0f + vert), "Mods", 10.0, SMALLER_LABEL));
      pGraphics->AttachControl(new IVGroupControl(IRECT(650.0, 20.0f + vert, 800.0, 120.0f + vert), "LFO", 10.0, SMALLER_LABEL));
      pGraphics->AttachControl(new IVGroupControl(IRECT(810.0, 20.0f + vert, 980.0, 120.0f + vert), "Mods", 10.0, SMALLER_LABEL));

      pGraphics->AttachControl(new IVSliderControl(IRECT(40.0, 35.0f + vert, 80.0, 120.0f + vert), TypeParam(t, kParamPwmAttack1), "A", SMALLER_LABEL, true, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
      pGraphics->AttachControl(new IVSliderControl(IRECT(70.0, 35.0f + vert, 110.0, 120.0f + vert), TypeParam(t, kParamPwmDecay1), "D", SMALLER_LABEL, true, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
      pGraphics->AttachControl(
        new IVSliderControl(IRECT(100.0, 35.0f + vert, 140.0, 120.0f + vert), TypeParam(t, kParamPwmSustain1), "S", SMALLER_LABEL, true, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
      pGraphics->AttachControl(
        new IVSliderControl(IRECT(130.0, 35.0f + vert, 170.0, 120.0f + vert), TypeParam(t, kParamPwmRelease1), "R", SMALLER_LABEL, true, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
      pGraphics
        ->AttachControl(
          new IVSlideSwitchControl(IRECT(175.0, 60.0f + vert, 195.0, 90.0f + vert), TypeParam(t, kParamPwmLFORateMode1), "", DEFAULT_STYLE.WithShowValue(false).WithShowLabel(false), false, EDirection::Vertical),
          kNoTag, "LFO");
      pGraphics
        ->AttachControl(new IVKnobControl(
          IRECT(205.0, 35.0f + vert, 240.0, 115.0f + vert), TypeParam(t, kParamPwmLFORateTempo1), "Rate", SMALLER_LABEL, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0))
        ->DisablePrompt(false);
      pGraphics
        ->AttachControl(new IVKnobControl(
          IRECT(205.0, 35.0f + vert, 240.0, 115.0f + vert), TypeParam(t, kParamPwmLFORateHz1), "Rate", SMALLER_LABEL, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
      pGraphics->AttachControl(new IVKnobControl(
        IRECT(240.0, 35.0f + vert, 275.0, 115.0f + vert), TypeParam(t, kParamPwmLFODepth1), "Depth", SMALLER_LABEL, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
      pGraphics
        ->AttachControl(new IVKnobControl(
          IRECT(275.0, 35.0f + vert, 310.0, 115.0f + vert), TypeParam(t, kParamPwmLFOShape1), "Shape", SMALLER_LABEL, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0))
        ->DisablePrompt(false);
      pGraphics->AttachControl(
        new IVSlideSwitchControl(IRECT(340.0, 60.0f + vert, 360.0, 90.0f + vert), TypeParam(t, kParamPwmKeyTrack1), "", DEFAULT_STYLE.WithShowValue(false).WithShowLabel(false), true, EDirection::Vertical));
      pGraphics->AttachControl(
        new IVKnobControl(IRECT(390.0, 35.0f + vert, 435.0, 115.0f + vert), TypeParam(t, kParamPwmModPow1), "Mod Pow", SMALLER_LABEL, true, false, -135.0, 135.0, 67.5, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
      pGraphics->AttachControl(
        new IVKnobControl(IRECT(445.0, 35.0f + vert, 490.0, 115.0f + vert), TypeParam(t, kParamPwmOffset1), "Offset", SMALLER_LABEL, true, false, -135.0, 135.0, 0.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));


      pGraphics->AttachControl(
        new IVSliderControl(IRECT(520.0, 35.0f + vert, 560.0, 120.0f + vert), TypeParam(t, kParamPitchAttack1), "A", SMALLER_LABEL, true, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
      pGraphics->AttachControl(
        new IVSliderControl(IRECT(550.0, 35.0f + vert, 590.0, 120.0f + vert), TypeParam(t, kParamPitchDecay1), "D", SMALLER_LABEL, true, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
      pGraphics->AttachControl(
        new IVSliderControl(IRECT(580.0, 35.0f + vert, 620.0, 120.0f + vert), TypeParam(t, kParamPitchSustain1), "S", SMALLER_LABEL, true, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
      pGraphics->AttachControl(
        new IVSliderControl(IRECT(610.0, 35.0f + vert, 650.0, 120.0f + vert), TypeParam(t, kParamPitchRelease1), "R", SMALLER_LABEL, true, EDirection::Vertical, DEFAULT_GEARING, 5.0, 2.0, false));
      pGraphics
        ->AttachControl(
          new IVSlideSwitchControl(IRECT(655.0, 55.0f + vert, 675.0, 85.0f + vert), TypeParam(t, kParamPitchLFORateMode1), "", DEFAULT_STYLE.WithShowValue(false).WithShowLabel(false), false, EDirection::Vertical),
          kNoTag, "LFO");
      pGraphics
        ->AttachControl(new IVKnobControl(
          IRECT(685.0, 35.0f + vert, 720.0, 115.0f + vert), TypeParam(t, kParamPitchLFORateTempo1), "Rate", SMALLER_LABEL, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0))
        ->DisablePrompt(false);
      pGraphics
        ->AttachControl(new IVKnobControl(
          IRECT(685.0, 35.0f + vert, 720.0, 115.0f + vert), TypeParam(t, kParamPitchLFORateHz1), "Rate", SMALLER_LABEL, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
      pGraphics->AttachControl(new IVKnobControl(
        IRECT(720.0, 35.0f + vert, 755.0, 115.0f + vert), TypeParam(t, kParamPitchLFODepth1), "Depth", SMALLER_LABEL, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
      pGraphics
        ->AttachControl(new IVKnobControl(
          IRECT(755.0, 35.0f + vert, 790.0, 115.0f + vert), TypeParam(t, kParamPitchLFOShape1), "Shape", SMALLER_LABEL, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0))
        ->DisablePrompt(false);
      pGraphics->AttachControl(new IVKnobControl(
        IRECT(815.0, 35.0f + vert, 860.0, 115.0f + vert), TypeParam(t, kParamPitchKeyTrack1), "Key Tr.", SMALLER_LABEL, true, false, -135.0, 135.0, 0.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
      pGraphics->AttachControl(
        new IVKnobControl(IRECT(870.0, 35.0f + vert, 915.0, 115.0f + vert), TypeParam(t, kParamPitchModPow1), "Mod Pow", SMALLER_LABEL, true, false, -135.0, 135.0, 0.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
      pGraphics->AttachControl(
        new IVKnobControl(IRECT(925.0, 35.0f + vert, 970.0, 115.0f + vert), TypeParam(t, kParamPitchOffset1), "Offset", SMALLER_LABEL, true, false, -135.0, 135.0, 0.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));


      pGraphics->AttachControl(new IVNumberBoxControl(IRECT(1000.0, 35.0f + vert, 1075.0, 85.0f + vert), TypeParam(t, kParamExtraUnison1), nullptr, "Unison", DEFAULT_STYLE, false, 1.0, 1.0, 8.0, "%0.0f", false));
      pGraphics->AttachControl(new IVKnobControl(
        IRECT(1075.0, 30.0f + vert, 1130.0, 95.0f + vert), TypeParam(t, kParamExtraDetune1), "Detune", DEFAULT_STYLE, true, false, -135.0, 135.0, -135.0, EDirection::Horizontal, DEFAULT_GEARING, 3.0));
    }

    //////////////// VISUALISATION AND PLAYING SURFACE
    // LFO displays next to each instrument row (PWM LFO on top, pitch LFO below)
    for (int t = 0; t < kNumTypes; ++t)
    {
      const float vert = 330.0f - 110.0f * t;
      pGraphics->AttachControl(new IVDisplayControl(IRECT(1140.0, 22.0f + vert, 1290.0, 70.0f + vert), "PWM LFO", SMALLER_LABEL, EDirection::Horizontal, -1.f, 1.f, 0.f, 256), kCtrlTagLFOVisFirst + 2 * t);
      pGraphics->AttachControl(new IVDisplayControl(IRECT(1140.0, 72.0f + vert, 1290.0, 120.0f + vert), "Pitch LFO", SMALLER_LABEL, EDirection::Horizontal, -1.f, 1.f, 0.f, 256), kCtrlTagLFOVisFirst + 2 * t + 1);
    }

    pGraphics->AttachControl(new IVScopeControl<1>(IRECT(380.0, 470.0, 760.0, 600.0), "Output", SMALLER_LABEL), kCtrlTagScope);
    pGraphics->AttachControl(new IVMeterControl<2>(IRECT(1235.0, 470.0, 1290.0, 600.0), "Level", SMALLER_LABEL), kCtrlTagMeter);

    // pitch bend + mod wheel, keyboard (notes 0-3 = noise/sampler are below its range; use the drop box for the sampler)
    pGraphics->AttachControl(new IWheelControl(IRECT(320.0, 610.0, 345.0, 695.0)), kCtrlTagBender);
    pGraphics->AttachControl(new IWheelControl(IRECT(350.0, 610.0, 375.0, 695.0), IMidiMsg::EControlChangeMsg::kModWheel));
    pGraphics->AttachControl(new IVKeyboardControl(IRECT(380.0, 610.0, 1290.0, 695.0), 36, 96), kCtrlTagKeyboard);
    pGraphics->SetQwertyMidiKeyHandlerFunc([pGraphics](const IMidiMsg& msg) {
      if (auto* pKeyboard = pGraphics->GetControlWithTag(kCtrlTagKeyboard))
        pKeyboard->As<IVKeyboardControl>()->SetNoteFromMidi(msg.NoteNumber(), msg.StatusMsg() == IMidiMsg::kNoteOn);
    });

    IRECT dropBoxRect(25.0f, 500.0f, 300.0f, 650.0f);

    mDropBox = new DragDropWaveformDisplay(
      dropBoxRect,
      // --- 1. THE DROP CALLBACK ---
      [this](const char* filePath) {
        if (!filePath)
          return;

        // Render on the background worker; OnIdle() picks up the result and updates the drop box.
        ImportJob job;
        job.path = filePath;
        job.sampleRate = static_cast<uint32_t>(GetSampleRate());
        job.algo = ResampleAlgo::Lanczos;
        job.pipeline = PipelinePresets::DropBox();
        job.printBits = true;
        mImportWorker.Enqueue(std::move(job));
      },

      // --- 2. MOUSE DOWN (Note On): audition the last dropped sample ---
      [this]() {
        if (mPreviewVelocity <= 0)
          return;
        IMidiMsg msg;
        msg.MakeNoteOnMsg(kSamplerNote, mPreviewVelocity, 0);
        SendMidiMsgFromUI(msg); // UI -> DSP (SendMidiMsg would go to the host's MIDI out)
      },

      // --- 3. MOUSE UP (Note Off) ---
      [this]() {
        if (mPreviewVelocity <= 0)
          return;
        IMidiMsg msg;
        msg.MakeNoteOffMsg(kSamplerNote, 0, 0);
        SendMidiMsgFromUI(msg);
      });

    pGraphics->AttachControl(mDropBox);

    for (int t = 0; t < kNumTypes; ++t)
      UpdateLfoRateVisibility(t);
    ShowLatestSample(); // restore the waveform when the editor is reopened / after loading state
  };

#endif
}

bool VaiaOneBitPlus::SerializeState(IByteChunk& chunk) const
{
  // Parameters first, then the versioned custom block (see src/StateFormat.h)
  bool ok = SerializeParams(chunk);
  OneBitState::Write(chunk, mDSP.mSampleManager.CopyDynamicSlots());
  return ok;
}

int VaiaOneBitPlus::UnserializeState(const IByteChunk& chunk, int startPos)
{
  int pos = UnserializeParams(chunk, startPos);
  if (pos < 0)
    return pos;

  OneBitState::Data data;
  pos = OneBitState::Read<WDL_String>(chunk, pos, data);

  // v0 (no block) and corrupt blocks both end up with no samples, as before
  mDSP.mSampleManager.ReplaceDynamicSamples(std::move(data.slots));
  MigrateParams(data.dspVersion);

  // may be called on any host thread: let OnIdle (main thread) refresh the drop box
  mSamplesChanged.store(true);

  return pos;
}

void VaiaOneBitPlus::MigrateParams(uint32_t savedDspVersion)
{
  // Hook for sound-behaviour migrations. Each step converts params from version N to N+1, e.g.
  //   if (savedDspVersion < 2) { ... set params so the old sound is reproduced ... }
  // Nothing to migrate yet (OneBitState::kLatestDspVersion == 1).
  (void)savedDspVersion;
}

void VaiaOneBitPlus::ShowLatestSample()
{
  const size_t count = mDSP.mSampleManager.DynamicCount();
  if (count == 0)
  {
    mPreviewVelocity = 0;
    if (mDropBox)
      mDropBox->SetWaveformData({});
    return;
  }

  const int index = static_cast<int>(count) - 1;
  // dynamic slot i is played by velocity kPreparedCount + 1 + i (1:1 mapping, see SampleManager)
  mPreviewVelocity = static_cast<int>(SampleData::SampleManager::kPreparedCount) + 1 + index;
  if (mDropBox)
    mDropBox->SetWaveformData(mDSP.mSampleManager.GetDynamicSampleData(index));
}

void VaiaOneBitPlus::OnParamChangeUI(int paramIdx, EParamSource source)
{
  // Show either the Hz or the tempo-division rate knob, whatever changed the sync switch (UI, automation, state)
  if (paramIdx >= kParamPwmAttack1 && paramIdx < TypeParam(kNumTypes, kParamPwmAttack1))
  {
    const int rel = (paramIdx - kParamPwmAttack1) % kParamsPerType + kParamPwmAttack1;
    if (rel == kParamPwmLFORateMode1 || rel == kParamPitchLFORateMode1)
      UpdateLfoRateVisibility((paramIdx - kParamPwmAttack1) / kParamsPerType);
  }
  Plugin::OnParamChangeUI(paramIdx, source);
}

void VaiaOneBitPlus::UpdateLfoRateVisibility(int type)
{
  IGraphics* pGraphics = GetUI();
  if (!pGraphics)
    return;
  const bool pwmSync = GetParam(TypeParam(type, kParamPwmLFORateMode1))->Bool();
  pGraphics->HideControl(TypeParam(type, kParamPwmLFORateHz1), pwmSync);
  pGraphics->HideControl(TypeParam(type, kParamPwmLFORateTempo1), !pwmSync);
  const bool pitchSync = GetParam(TypeParam(type, kParamPitchLFORateMode1))->Bool();
  pGraphics->HideControl(TypeParam(type, kParamPitchLFORateHz1), pitchSync);
  pGraphics->HideControl(TypeParam(type, kParamPitchLFORateTempo1), !pitchSync);
}

void VaiaOneBitPlus::OnUIClose()
{
  mDropBox = nullptr; // IGraphics deletes its controls when the editor closes
  Plugin::OnUIClose();
}


#if IPLUG_DSP
void VaiaOneBitPlus::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  memset(outputs[0], 0, nFrames * sizeof(sample));
  memset(outputs[1], 0, nFrames * sizeof(sample));

  mDSP.ProcessBlock(inputs, outputs, 2, nFrames, mTimeInfo.mPPQPos, mTimeInfo.mTransportIsRunning);

  // visualisation only (lock-free, no allocation)
  mMeterSender.ProcessBlock(outputs, nFrames, kCtrlTagMeter);
  mScopeSender.ProcessBlock(outputs, nFrames, kCtrlTagScope, 1);
  for (int t = 0; t < kNumTypes; ++t)
  {
    mLFOVisSender.PushData({kCtrlTagLFOVisFirst + 2 * t, {static_cast<float>(mDSP.mPwmLFOs[t].GetLastOutput())}});
    mLFOVisSender.PushData({kCtrlTagLFOVisFirst + 2 * t + 1, {static_cast<float>(mDSP.mPitchLFOs[t].GetLastOutput())}});
  }
}

void VaiaOneBitPlus::OnIdle()
{
  // Main thread: pick up finished background imports
  bool imported = false;
  for (const auto& result : mImportWorker.TakeResults())
    imported |= (result.index >= 0);

  if (mSamplesChanged.exchange(false))
    imported = true;

  if (imported)
    ShowLatestSample();

  mMeterSender.TransmitData(*this);
  mScopeSender.TransmitData(*this);
  mLFOVisSender.TransmitData(*this);

  // free sample banks the audio thread no longer uses (also happens on every edit)
  mDSP.mSampleManager.CollectGarbage();
}

void VaiaOneBitPlus::OnReset()
{
  uint32_t currentSampleRate = static_cast<uint32_t>(GetSampleRate());

  // 1. Reset DSP state and buffer allocations
  mDSP.Reset(currentSampleRate, GetBlockSize());
  mMeterSender.Reset(currentSampleRate);

  // Samples are kept on a sample-rate change: each one stores the rate it was rendered at.
  // (They used to be cleared here, which lost every dropped or restored sample.)

  // 2. Push parameter defaults/states to the DSP
  for (int p = 0; p < kNumParams; ++p)
  {
    mDSP.SetParam(p, GetParam(p)->Value());
  }
}

void VaiaOneBitPlus::ProcessMidiMsg(const IMidiMsg& msg)
{
  TRACE;

  switch (msg.StatusMsg())
  {
  case IMidiMsg::kNoteOn:
  case IMidiMsg::kNoteOff:
  case IMidiMsg::kPolyAftertouch:
  case IMidiMsg::kControlChange:
  case IMidiMsg::kProgramChange:
  case IMidiMsg::kChannelAftertouch:
  case IMidiMsg::kPitchWheel:
    mDSP.ProcessMidiMsg(msg);
    SendMidiMsg(msg); // MIDI thru
    break;
  default:
    break;
  }
}

void VaiaOneBitPlus::OnParamChange(int paramIdx) { mDSP.SetParam(paramIdx, GetParam(paramIdx)->Value()); }

bool VaiaOneBitPlus::OnMessage(int msgTag, int ctrlTag, int dataSize, const void* pData)
{
  if (ctrlTag == kCtrlTagBender && msgTag == igraphics::IWheelControl::kMessageTagSetPitchBendRange)
  {
    const int bendRange = *static_cast<const int*>(pData);
    mDSP.mSynth.SetPitchBendRange(bendRange);
  }

  return false;
}


int VaiaOneBitPlus::ImportSample(const char* filePath, uint32_t targetSampleRate, ResampleAlgo resampleAlgo, const AudioPipeline& pipeline, bool printBits)
{
  if (!filePath)
    return -1;

  ImportJob job;
  job.path = filePath;
  job.sampleRate = targetSampleRate;
  job.algo = resampleAlgo;
  job.pipeline = pipeline;
  job.printBits = printBits;
  return RunImport(job).index;
}

VaiaOneBitPlus::ImportResult VaiaOneBitPlus::RunImport(const ImportJob& job)
{
  ImportResult result;

  SampleLoader loader;
  LoadedSample rawWav = loader.load_file_static(job.path.c_str());
  if (rawWav.sampleBuffer.empty())
    return result;

  auto resampled32 = SampleTools::Resample(rawWav.sampleBuffer, rawWav.header.SamplesPerSec, job.sampleRate, job.algo);
  auto packed1Bit = SampleTools::ReduceToOneBit(resampled32, job.sampleRate, job.pipeline);
  if (packed1Bit.empty())
    return result;

  // AddSample is thread-safe (publishes a new sample bank), so this may run on the worker thread
  result.index = mDSP.mSampleManager.AddSample(packed1Bit.data(), packed1Bit.size(), job.sampleRate, job.path, job.pipeline.ToString());

  if (job.printBits)
    PrintBits(job.path.c_str(), packed1Bit);

  return result;
}

int VaiaOneBitPlus::ImportSampleAndPrintBits(const char* filePath, uint32_t targetSampleRate, ResampleAlgo resampleAlgo, const AudioPipeline& pipeline)
{
  return ImportSample(filePath, targetSampleRate, resampleAlgo, pipeline, /*printBits*/ true);
}

std::string VaiaOneBitPlus::PrintBits(const char* filePath, const std::vector<int8_t>& packed1Bit)
{
  std::string code = SampleTools::ToCppArray(filePath ? filePath : "", packed1Bit);
  DebugPrintLong(code);
  return code;
}



#endif
