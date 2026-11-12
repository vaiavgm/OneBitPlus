#include "src/VaiaOneBitPlus.h"
#include <IPlug_include_in_plug_src.h>

#include <IControls.h>
#include <IVDisplayControl.h>
#include <IGraphicsStructs.h>
#include <IPlugLogger.h>
#include <LFO.h>

#include "src/SampleLoader.h"
#include "src/SampleData.h"
#include <IPlugPaths.h>
#include "src/PipelinePresets.h"
#include "src/Debug.h"
#include "src/ParamInit.h"

VaiaOneBitPlus::VaiaOneBitPlus(const InstanceInfo& info)
  : Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  // all parameter definitions and defaults live in ParamInit.h, so tests can use exactly the same ones
  InitParams([this](int paramIdx) { return GetParam(paramIdx); });

  // Chain presets: saved per user (e.g. %APPDATA%/Vaiaphraim/OneBitPlus/Presets), plus a read-only shared folder
  // (e.g. C:/ProgramData/Vaiaphraim/OneBitPlus/Presets) that an installer can fill.
  {
    WDL_String userBase, sharedBase;
    AppSupportPath(userBase, false);
    AppSupportPath(sharedBase, true);
    const std::filesystem::path sub = std::filesystem::path(PLUG_MFR) / PLUG_NAME / "Presets";
    mPresets = ChainPresets::Store(std::filesystem::path(userBase.Get()) / sub, {std::filesystem::path(sharedBase.Get()) / sub});
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
      // --- 1. THE DROP CALLBACK: into the first free slot ---
      [this](const char* filePath) {
        if (filePath)
          ImportFile(filePath, -1);
      },

      // --- 2. MOUSE DOWN (Note On): audition the selected (or last) sample ---
      [this]() { SendAudition(true); },

      // --- 3. MOUSE UP (Note Off) ---
      [this]() { SendAudition(false); });

    pGraphics->AttachControl(mDropBox);

    // The sampler plays at a fixed speed unless this is on; then it follows pitch bend, the pitch envelope and the LFO
    // Output DC filter (10 Hz high-pass), on by default. A switch for now, to compare with the host's own DC filter.
    pGraphics->AttachControl(new IVToggleControl(IRECT(170.0, 655.0, 300.0, 695.0), kParamDcFilter, "DC filter", SMALLER_LABEL, "OFF", "ON"));
    pGraphics->AttachControl(new IVToggleControl(IRECT(25.0, 655.0, 165.0, 695.0), kParamSamplerPitch, "Sampler pitch", SMALLER_LABEL, "OFF", "BENDS"));

    // Switch for projects saved before the sound changes (it is on for them after loading, off for new instances)
    pGraphics->AttachControl(
      new IVToggleControl(IRECT(305.0, 500.0, 375.0, 560.0), [this](IControl* pCaller) { mDSP.mLegacyTiming = pCaller->GetValue() > 0.5; }, "Legacy timing", SMALLER_LABEL, "OFF", "ON"), kCtrlTagLegacyTiming);

    //////////////// SAMPLES PAGE (a tab over the instrument rows; the keyboard below stays visible)
    SamplesPanel::Callbacks cb;
    cb.slotInfo = [this](int slot) { return SlotInfoFor(slot); };
    cb.select = [this](int slot) { SelectSlot(slot); };
    cb.audition = [this](bool down) { SendAudition(down); };
    cb.clearSlot = [this]() { ClearSelectedSlot(); };
    cb.revert = [this]() { RevertSelectedSlot(); };
    cb.setEmbed = [this](bool embed) { SetEmbedSource(embed); };
    cb.locate = [this](const std::string& path) { LocateSource(path); };
    cb.dropFile = [this](const std::string& path) {
      // a WAV dropped on the page goes into the selected slot if that is free, otherwise into the first free one
      const int selected = mSamplesModel.Selected();
      ImportFile(path, selected >= 0 && !mSamplesModel.HasSample() ? selected : -1);
    };
    mSamplesPanel = new SamplesPanel(IRECT(SamplesLayout::kPanel.l, SamplesLayout::kPanel.t, SamplesLayout::kPanel.r, SamplesLayout::kPanel.b), mSamplesModel, mPresets, std::move(cb));
    pGraphics->AttachControl(mSamplesPanel, kCtrlTagSamplesPanel)->Hide(true);
    pGraphics->AttachControl(
      new IVToggleControl(IRECT(5.0, 0.0, 160.0, 24.0), [this](IControl* pCaller) { SetSamplesPageOpen(pCaller->GetValue() > 0.5); }, "", SMALLER_LABEL.WithShowLabel(false), "Samples", "Samples (open)"), kCtrlTagSamplesTab);

    for (int t = 0; t < kNumTypes; ++t)
      UpdateLfoRateVisibility(t);
    SelectSlot(mSamplesModel.Selected()); // restore selection and waveform when the editor is reopened
  };

#endif
}

bool VaiaOneBitPlus::SerializeState(IByteChunk& chunk) const
{
  // Parameters first, then the versioned custom block (see src/StateFormat.h)
  bool ok = SerializeParams(chunk);
  const uint32_t compat = mDSP.mLegacyTiming.load() ? OneBitState::kCompatLegacyTiming : 0u;
  OneBitState::Write(chunk, mDSP.mSampleManager.CopyDynamicSlots(), OneBitState::kLatestDspVersion, compat);
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
  ApplyMigrations(data);

  // may be called on any host thread: let OnIdle (main thread) refresh the drop box
  mSamplesChanged.store(true);

  return pos;
}

void VaiaOneBitPlus::ResetParam(int paramIdx, double value)
{
  GetParam(paramIdx)->Set(value);
  OnParamChange(paramIdx); // DSP; the editor is refreshed by the host's state restore
}

void VaiaOneBitPlus::ApplyMigrations(const OneBitState::Data& data)
{
  if (data.dspVersion < 2)
  {
    // Saved by the original behaviour: keep its timing (fixed 120 BPM for synced LFOs, samples at 44.1 kHz speed).
    // The user can switch this off in the editor.
    mDSP.mLegacyTiming = true;

    // The Release stages of both envelopes (and Pitch Keytrack) did nothing before (the voice was cut off at note-off), so whatever
    // is stored in them was never heard. Reset them to 0, so the sound of the project stays the same now that
    // release works. (A release of 0 ends within a few samples.)
    for (int t = 0; t < kNumTypes; ++t)
    {
      ResetParam(TypeParam(t, kParamPwmRelease1));
      ResetParam(TypeParam(t, kParamPitchRelease1));

      // Pitch Keytrack did nothing before either: reset it, so that it doesn't start to change the pitch now.
      ResetParam(TypeParam(t, kParamPitchKeyTrack1));
    }
  }
  else
  {
    mDSP.mLegacyTiming = (data.compatFlags & OneBitState::kCompatLegacyTiming) != 0;
  }

  // The slot of Sampler Pitch used to be the retired "Input Dither" knob, which could hold any value. Before
  // dspVersion 3 the sampler never followed pitch, so such projects must load with it off.
  if (data.dspVersion < 3)
    ResetParam(kParamSamplerPitch, 0.);

  // The slot of DC Filter used to be the retired "Input Protection" knob, which could hold any value (0 for almost every
  // project). The filter is part of the sound, so projects saved before dspVersion 4 load with it on.
  if (data.dspVersion < 4)
    ResetParam(kParamDcFilter, 1.);
}

SamplesLayout::SlotInfo VaiaOneBitPlus::SlotInfoFor(int slot) const
{
  SamplesLayout::SlotInfo info;
  const auto s = mDSP.mSampleManager.GetSlot(slot);
  if (s && !s->Empty())
  {
    info.used = true;
    info.name = SamplesLayout::BaseName(s->path);
    info.bytes = s->data.size();
  }
  return info;
}

void VaiaOneBitPlus::SelectSlot(int slot)
{
  if (slot != mSamplesModel.Selected())
    ++mSlotGeneration; // drop renders that are still running for the slot we are leaving

  const auto s = slot >= 0 ? mDSP.mSampleManager.GetSlot(slot) : nullptr;
  std::error_code ec;
  const bool fileExists = s && !s->path.empty() && std::filesystem::exists(std::filesystem::path(s->path), ec);
  mSamplesModel.Select(slot, s, fileExists);

  // what is stored can be shown without the source: the spectrogram and waveform of the 1-bit data
  if (s && !s->Empty())
    mSamplesModel.SetPreview(SamplesModel::MakePreview(s->data, {}, s->sampleRate));
  RefreshDropBox();
  if (mSamplesPanel)
    mSamplesPanel->SetDirty(false);
}

void VaiaOneBitPlus::RefreshDropBox()
{
  // the drop box and the audition button follow the selected sample, or the last one if none is selected
  int index = mSamplesModel.HasSample() ? mSamplesModel.Selected() : -1;
  if (index < 0)
    for (int i = static_cast<int>(mDSP.mSampleManager.DynamicCount()) - 1; i >= 0; --i)
      if (const auto s = mDSP.mSampleManager.GetSlot(i); s && !s->Empty())
      {
        index = i;
        break;
      }

  if (index < 0)
  {
    mPreviewVelocity = 0;
    if (mDropBox)
      mDropBox->SetWaveformData({});
    return;
  }
  // dynamic slot i is played by velocity kPreparedCount + 1 + i (1:1 mapping, see SampleManager)
  mPreviewVelocity = static_cast<int>(SampleData::SampleManager::kPreparedCount) + 1 + index;
  if (mDropBox)
    mDropBox->SetWaveformData(mDSP.mSampleManager.GetDynamicSampleData(index));
}

void VaiaOneBitPlus::SendAudition(bool down)
{
  if (mPreviewVelocity <= 0)
    return;
  IMidiMsg msg;
  if (down)
    msg.MakeNoteOnMsg(kSamplerNote, mPreviewVelocity, 0);
  else
    msg.MakeNoteOffMsg(kSamplerNote, 0, 0);
  SendMidiMsgFromUI(msg); // UI -> DSP (SendMidiMsg would go to the host's MIDI out)
}

void VaiaOneBitPlus::SetSamplesPageOpen(bool open)
{
  IGraphics* pGraphics = GetUI();
  if (!pGraphics)
    return;
  if (IControl* panel = pGraphics->GetControlWithTag(kCtrlTagSamplesPanel))
    panel->Hide(!open);

  // The meter, scope and LFO displays under the page are redrawn all the time: hide them while it covers them,
  // or the page would be redrawn with them.
  auto hide = [&](int tag) {
    if (IControl* c = pGraphics->GetControlWithTag(tag))
      c->Hide(open);
  };
  hide(kCtrlTagMeter);
  hide(kCtrlTagScope);
  for (int tag = kCtrlTagLFOVisFirst; tag <= kCtrlTagLFOVisLast; ++tag)
    hide(tag);
  pGraphics->SetAllControlsDirty();
}

void VaiaOneBitPlus::ClearSelectedSlot()
{
  const int slot = mSamplesModel.Selected();
  if (slot < 0)
    return;
  ++mSlotGeneration;
  mDSP.mSampleManager.ClearSlot(slot);
  SelectSlot(slot);
}

void VaiaOneBitPlus::RevertSelectedSlot()
{
  const int slot = mSamplesModel.Selected();
  const auto original = mSamplesModel.Original();
  if (slot < 0 || !original)
    return;
  ++mSlotGeneration;
  mDSP.mSampleManager.SetSlot(slot, *original);
  SelectSlot(slot);
}

void VaiaOneBitPlus::SetEmbedSource(bool embed)
{
  const int slot = mSamplesModel.Selected();
  const auto current = slot >= 0 ? mDSP.mSampleManager.GetSlot(slot) : nullptr;
  if (!current || current->Empty())
    return;

  SampleData::Slot copy = *current;
  copy.embedSource = embed;
  mDSP.mSampleManager.SetSlot(slot, std::move(copy));
  mSamplesModel.SetEmbed(embed);

  // to embed the source it has to be in memory: render once (same chain, same result) to load it
  if (embed && !current->source)
    EnqueueRender(true);
}

void VaiaOneBitPlus::LocateSource(const std::string& path)
{
  const int slot = mSamplesModel.Selected();
  const auto current = slot >= 0 ? mDSP.mSampleManager.GetSlot(slot) : nullptr;
  if (!current || current->Empty() || !SampleRender::LoadSourceFile(path))
  {
    mSamplesModel.SetMessage("That file can't be read as a WAV.");
    return;
  }
  SampleData::Slot copy = *current;
  copy.path = path;
  copy.source = nullptr; // read from the new file when needed
  mDSP.mSampleManager.SetSlot(slot, std::move(copy));
  SelectSlot(slot);
  mSamplesModel.SetMessage("Source file found.");
}

// Render the selected slot again with the chain being edited (on the worker).
void VaiaOneBitPlus::EnqueueRender(bool loadSourceOnly)
{
  const int slot = mSamplesModel.Selected();
  const auto current = slot >= 0 ? mDSP.mSampleManager.GetSlot(slot) : nullptr;
  if (!current || current->Empty())
    return;

  SampleJob job;
  job.kind = SampleJob::Kind::Render;
  job.slot = slot;
  job.path = current->path;
  job.source = current->source;
  job.sampleRate = std::max<uint32_t>(8000u, static_cast<uint32_t>(GetSampleRate()));
  job.chain = mSamplesModel.Chain();
  job.generation = mSlotGeneration.load();
  (void)loadSourceOnly; // same job: loading the source is part of every render that doesn't have it yet
  mSamplesModel.RenderStarted();
  mImportWorker.Enqueue(std::move(job));
}

void VaiaOneBitPlus::HandleJobResults()
{
  for (auto& result : mImportWorker.TakeResults())
  {
    if (result.stale)
      continue;
    const bool isSelected = result.slot == mSamplesModel.Selected();
    switch (result.kind)
    {
    case SampleJob::Kind::Import:
      if (result.ok)
      {
        SelectSlot(result.slot);
        mSamplesModel.SetPreview(std::move(result.preview));
        mSamplesModel.SetMessage("Imported.");
        if (mSamplesPanel)
          mSamplesPanel->ScrollToSlot(result.slot);
      }
      else
      {
        mSamplesModel.SetMessage(result.error);
      }
      break;

    case SampleJob::Kind::Render:
      if (!isSelected)
        break;
      mSamplesModel.RenderFinished(result.ok ? "" : result.error);
      if (result.ok)
      {
        mSamplesModel.SetPreview(std::move(result.preview));
        mSamplesModel.SetStatus(SamplesModel::SourceStatus::InMemory);
        RefreshDropBox();
      }
      else if (result.sourceMissing)
      {
        mSamplesModel.SetStatus(SamplesModel::SourceStatus::Missing);
      }
      break;

    case SampleJob::Kind::AutoDc:
      if (!isSelected)
        break;
      if (result.ok)
      {
        mSamplesModel.SetParam(result.dcStage, 0, result.dcOffset, SamplesPanel::NowMs()); // re-renders with it
        char text[64];
        std::snprintf(text, sizeof(text), "DC offset set to %.3f", result.dcOffset);
        mSamplesModel.SetMessage(text);
      }
      else
      {
        mSamplesModel.SetMessage(result.error);
      }
      break;
    }
  }
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
  mSamplesPanel = nullptr;
  Plugin::OnUIClose();
}


#if IPLUG_DSP
void VaiaOneBitPlus::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  memset(outputs[0], 0, nFrames * sizeof(sample));
  memset(outputs[1], 0, nFrames * sizeof(sample));

#if ONEBIT_DIAGNOSTICS
  mDiag.blocks.fetch_add(1, std::memory_order_relaxed);
  mDiag.frames.fetch_add(static_cast<uint64_t>(nFrames), std::memory_order_relaxed);
  mDiag.lastFrames.store(nFrames, std::memory_order_relaxed);
  mDiag.running.store(mTimeInfo.mTransportIsRunning ? 1 : 0, std::memory_order_relaxed);
  mDiag.tempo.store(mTimeInfo.mTempo, std::memory_order_relaxed);
  mDiag.ppq.store(mTimeInfo.mPPQPos, std::memory_order_relaxed);
  // Event log (a few lines at most: the first block after a reset, and every change of the transport state). This runs on the
  // audio thread, which is fine for a diagnostics build and does not depend on the editor's idle timer.
  {
    const int running = mTimeInfo.mTransportIsRunning ? 1 : 0;
    char line[240];
    if (!mDiag.firstBlockLogged.exchange(true))
    {
      std::snprintf(line, sizeof(line), "OneBitPlus: FIRST BLOCK since reset: %d frames, transport=%s, tempo=%g, ppq=%g", nFrames, running ? "RUNNING" : "stopped", mTimeInfo.mTempo, mTimeInfo.mPPQPos);
      DiagnosticsLog(line);
    }
    if (mDiag.lastLoggedRunning.exchange(running) != running)
    {
      std::snprintf(line, sizeof(line), "OneBitPlus: TRANSPORT -> %s at block %llu (tempo=%g, ppq=%g)", running ? "RUNNING" : "stopped", static_cast<unsigned long long>(mDiag.blocks.load()), mTimeInfo.mTempo, mTimeInfo.mPPQPos);
      DiagnosticsLog(line);
    }
  }
#endif
  // the DSP validates the host's tempo and position (HostTime.h): hosts may leave them unset while stopped
  mDSP.ProcessBlock(inputs, outputs, 2, nFrames, mTimeInfo.mPPQPos, mTimeInfo.mTransportIsRunning, mTimeInfo.mTempo);

#if ONEBIT_DIAGNOSTICS
  {
    double peak = 0.;
    for (int i = 0; i < nFrames; ++i)
      peak = std::max(peak, std::fabs(static_cast<double>(outputs[0][i])));
    if (peak > mDiag.peak.load(std::memory_order_relaxed))
      mDiag.peak.store(peak, std::memory_order_relaxed);
  }
#endif

  // visualisation only (lock-free, no allocation)
  mMeterSender.ProcessBlock(outputs, nFrames, kCtrlTagMeter);
  mScopeSender.ProcessBlock(outputs, nFrames, kCtrlTagScope, 1);
  for (int t = 0; t < kNumTypes; ++t)
  {
    mLFOVisSender.PushData({kCtrlTagLFOVisFirst + 2 * t, {static_cast<float>(mDSP.mPwmLFOs[t].GetLastOutput())}});
    mLFOVisSender.PushData({kCtrlTagLFOVisFirst + 2 * t + 1, {static_cast<float>(mDSP.mPitchLFOs[t].GetLastOutput())}});
  }
}

#if ONEBIT_DIAGNOSTICS
void VaiaOneBitPlus::LogDiagnostics()
{
  const uint64_t now = SamplesPanel::NowMs();
  if (now < mDiag.lastLogMs + 1000)
    return;
  mDiag.lastLogMs = now;

  int busy = 0;
  for (int v = 0; v < static_cast<int>(mDSP.mSynth.NVoices()); ++v)
    busy += mDSP.mSynth.GetVoice(v)->GetBusy() ? 1 : 0;

  const uint64_t blocks = mDiag.blocks.load();
  char line[400];
  std::snprintf(line, sizeof(line),
                "OneBitPlus: blocks=%llu (+%llu in the last second) lastBlock=%d frames  transport=%s tempo=%g ppq=%g | midi noteOn=%llu noteOff=%llu cc=%llu other=%llu | resets=%llu | peak=%.3f busyVoices=%d legacyTiming=%d dcFilter=%d",
                static_cast<unsigned long long>(blocks), static_cast<unsigned long long>(blocks - mDiag.lastLoggedBlocks), mDiag.lastFrames.load(), mDiag.running.load() ? "RUNNING" : "stopped", mDiag.tempo.load(), mDiag.ppq.load(),
                static_cast<unsigned long long>(mDiag.noteOn.load()), static_cast<unsigned long long>(mDiag.noteOff.load()), static_cast<unsigned long long>(mDiag.controllers.load()), static_cast<unsigned long long>(mDiag.otherMidi.load()),
                static_cast<unsigned long long>(mDiag.resets.load()), mDiag.peak.exchange(0.), busy, mDSP.mLegacyTiming.load() ? 1 : 0, mDSP.mDcFilterEnabled.load() ? 1 : 0);
  mDiag.lastLoggedBlocks = blocks;
  DiagnosticsLog(line);
}
#endif

void VaiaOneBitPlus::OnIdle()
{
#if ONEBIT_DIAGNOSTICS
  LogDiagnostics();
#endif

  // Main thread: pick up finished background renders
  HandleJobResults();

  // a state was loaded (host thread): the slots may all be different now
  if (mSamplesChanged.exchange(false))
    SelectSlot(mSamplesModel.Selected());

  // start the background work the page asked for
  if (mSamplesModel.Selected() >= 0)
  {
    const int dcStage = mSamplesModel.TakePendingAutoDc();
    if (dcStage >= 0)
    {
      const auto current = mDSP.mSampleManager.GetSlot(mSamplesModel.Selected());
      if (current && !current->Empty())
      {
        SampleJob job;
        job.kind = SampleJob::Kind::AutoDc;
        job.slot = mSamplesModel.Selected();
        job.path = current->path;
        job.source = current->source;
        job.sampleRate = std::max<uint32_t>(8000u, static_cast<uint32_t>(GetSampleRate()));
        job.chain = mSamplesModel.Chain();
        job.dcStage = static_cast<size_t>(dcStage);
        mSamplesModel.SetMessage("Searching for the DC offset with the least DC...");
        mImportWorker.Enqueue(std::move(job));
      }
    }
    if (mSamplesModel.NeedsRender(SamplesPanel::NowMs()))
      EnqueueRender(false);
  }

  // keep the legacy-timing switch in sync after a state load
  if (IGraphics* pGraphics = GetUI())
  {
    if (IControl* pToggle = pGraphics->GetControlWithTag(kCtrlTagLegacyTiming))
    {
      const double value = mDSP.mLegacyTiming.load() ? 1.0 : 0.0;
      if (pToggle->GetValue() != value)
      {
        pToggle->SetValue(value);
        pToggle->SetDirty(false);
      }
    }
  }

  mMeterSender.TransmitData(*this);
  mScopeSender.TransmitData(*this);
  mLFOVisSender.TransmitData(*this);

  // free sample banks the audio thread no longer uses (also happens on every edit)
  mDSP.mSampleManager.CollectGarbage();
}

void VaiaOneBitPlus::OnReset()
{
  uint32_t currentSampleRate = static_cast<uint32_t>(GetSampleRate());
#if ONEBIT_DIAGNOSTICS
  mDiag.resets.fetch_add(1, std::memory_order_relaxed);
  mDiag.midiLogBudget.store(60); // log the first MIDI messages after every reset one by one
  mDiag.firstBlockLogged.store(false);
  DiagnosticsLog("OneBitPlus: OnReset (host stop/start or setup) sampleRate=" + std::to_string(currentSampleRate) + " maxBlock=" + std::to_string(GetBlockSize()) + " build=" + __DATE__ + " " + __TIME__ + " dspVersion=" + std::to_string(OneBitState::kLatestDspVersion));
#endif

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

#if ONEBIT_DIAGNOSTICS
  switch (msg.StatusMsg())
  {
  case IMidiMsg::kNoteOn: mDiag.noteOn.fetch_add(1, std::memory_order_relaxed); break;
  case IMidiMsg::kNoteOff: mDiag.noteOff.fetch_add(1, std::memory_order_relaxed); break;
  case IMidiMsg::kControlChange: mDiag.controllers.fetch_add(1, std::memory_order_relaxed); break;
  default: mDiag.otherMidi.fetch_add(1, std::memory_order_relaxed); break;
  }
  if (mDiag.midiLogBudget.fetch_sub(1, std::memory_order_relaxed) > 0)
  {
    char line[200];
    std::snprintf(line, sizeof(line), "OneBitPlus: MIDI status=0x%02X data1=%d data2=%d offset=%d channel=%d transport=%s", static_cast<int>(msg.mStatus), static_cast<int>(msg.mData1), static_cast<int>(msg.mData2), msg.mOffset, msg.Channel(), mTimeInfo.mTransportIsRunning ? "RUNNING" : "stopped");
    DiagnosticsLog(line);
  }
#endif

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


void VaiaOneBitPlus::ImportFile(const std::string& path, int slot)
{
  SampleJob job;
  job.kind = SampleJob::Kind::Import;
  job.slot = slot;
  job.path = path;
  job.sampleRate = std::max<uint32_t>(8000u, static_cast<uint32_t>(GetSampleRate()));
  job.chain = PipelineModel::DefaultChain(); // the former drop-box chain
  mImportWorker.Enqueue(std::move(job));
}

VaiaOneBitPlus::SampleJobResult VaiaOneBitPlus::RunJob(const SampleJob& job)
{
  return SampleJobs::Run(mDSP.mSampleManager, mSlotGeneration, job, [this](const std::string& path, const std::vector<int8_t>& bits) { PrintBits(path.c_str(), bits); });
}

std::string VaiaOneBitPlus::PrintBits(const char* filePath, const std::vector<int8_t>& packed1Bit)
{
  std::string code = SampleTools::ToCppArray(filePath ? filePath : "", packed1Bit);
  DebugPrintLong(code);
  return code;
}



#endif
