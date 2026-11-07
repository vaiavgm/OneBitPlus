#pragma once

// Parameter and control-tag enums, shared by the plugin (VaiaOneBitPlus.h) and the DSP (VaiaOneBitPlus_DSP.h).
// Invariant: the 4 instrument blocks must have identical layouts (see docs/ARCHITECTURE.md).

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



  // Retired: "Input Protection" (kParamUnused1) did nothing, so its slot stays as a hidden placeholder. iPlug2 saves
  // and restores parameters as a fixed-length run: removing or adding a slot would shift the rest of the saved state
  // of every existing project.
  kParamUnused1,
  // Sampler Pitch (on/off) reuses the slot of the retired "Input Dither" knob. Old states may hold a stray value
  // there, so states saved before dspVersion 3 get it reset to 0 (off) when they are loaded.
  kParamSamplerPitch,

  kNumParams
};



// Tags of controls the plugin talks to (UI only, not saved anywhere)
enum EControlTags
{
  kCtrlTagMeter = 0,
  kCtrlTagScope,
  kCtrlTagKeyboard,
  kCtrlTagBender,
  kCtrlTagLegacyTiming,
  kCtrlTagSamplesTab,
  kCtrlTagSamplesPanel,
  kCtrlTagLFOVisFirst, // kNumTypes x {PWM LFO, pitch LFO}: tag = kCtrlTagLFOVisFirst + 2 * type + (pitch ? 1 : 0)
  kCtrlTagLFOVisLast = kCtrlTagLFOVisFirst + 2 * 4 - 1,
  kNumCtrlTags
};

// --- Instrument ("Type") parameter blocks -------------------------------------------------------------------
// The 4 instrument types share one layout: a block of kParamsPerType params starting at kParamPwmAttack1.
// Use TypeParam(type, kParamXxx1) to address the same param of another type.
constexpr int kNumTypes = 4;
constexpr int kParamsPerType = kParamPwmAttack2 - kParamPwmAttack1;

constexpr int TypeParam(int type, int type1Param) { return type1Param + type * kParamsPerType; }

static_assert(kParamsPerType == 26, "instrument param block changed: update docs/ARCHITECTURE.md and SetParam");
static_assert(TypeParam(1, kParamPwmAttack1) == kParamPwmAttack2 && TypeParam(2, kParamPwmAttack1) == kParamPwmAttack3 && TypeParam(3, kParamPwmAttack1) == kParamPwmAttack4,
              "instrument param blocks must be contiguous");
static_assert(TypeParam(3, kParamExtraDetune1) == kParamExtraDetune4 && TypeParam(1, kParamPitchOffset1) == kParamPitchOffset2,
              "instrument param blocks must have identical layouts");
static_assert(kParamUnused1 == kParamExtraDetune4 + 1, "global params must follow the instrument blocks");
