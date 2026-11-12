#pragma once

// Definition (name, range, default) of every parameter. Used by the plugin constructor and by the tests, so that
// the defaults a fresh instance starts with can be tested without a host.

#include <string>

#include <IPlugParameter.h>
#include <LFO.h>

#include "src/Params.h"

// getParam(i) returns the IParam* of parameter i (the plugin: GetParam(i); tests: their own IParam array).
template <class GetParam>
void InitParams(GetParam&& getParam)
{
  using namespace iplug;

  getParam(kParamGain)->InitDouble("Gain", 50., 0., 100.0, 0.01, "%");
  getParam(kParamNoteGlideTime)->InitMilliseconds("Note Glide Time", 0., 0.0, 150.);

  // retired param: kept only so that saved states keep their layout (see Params.h)
  getParam(kParamUnused1)->InitDouble("(unused 1)", 0., 0., 100., 1., "", IParam::kFlagCannotAutomate | IParam::kFlagMeta);
  // Sampler Pitch: off = the sampler always plays at its recorded speed (the original behaviour)
  getParam(kParamSamplerPitch)->InitBool("Sampler Pitch", false);

  // Instrument types I-IV: identical parameter blocks (see Params.h). Ranges, defaults and the order of
  // the parameters are unchanged from the original per-type code. Only the display names got a "T<n>" prefix,
  // so host automation lanes can be told apart. Parameter indices (what hosts store) are unchanged.
  for (int t = 0; t < kNumTypes; ++t)
  {
    auto P = [&getParam, t](int type1Param) { return getParam(TypeParam(t, type1Param)); };
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
}
