#pragma once

// Import pipeline recipes (effects + quantizer) that were tried while sound-designing the samples.
// They used to be built (and thrown away) in VaiaOneBitPlus::ReloadSamples on every sample-rate change.
// Kept here as named presets for the planned pipeline builder (see docs/ANALYSIS.md, "To discuss later" A).

#include <memory>

#include "src/IAudioEffect.h"

namespace PipelinePresets
{

// Pipeline used by the drop box.
inline AudioPipeline DropBox()
{
  AudioPipeline p;
  p.effects.push_back(std::make_shared<NormalizeEffect>());
  p.effects.push_back(std::make_shared<SaturateEffect>(10.0));
  p.effects.push_back(std::make_shared<DitherEffect>(0.1));
  p.quantizer = std::make_shared<TrellisQuantizer>();
  return p;
}

inline AudioPipeline Snare1()
{
  AudioPipeline p;
  p.effects.push_back(std::make_shared<NormalizeEffect>());
  p.effects.push_back(std::make_shared<SaturateEffect>(4.0));
  p.effects.push_back(std::make_shared<SampleRateReductionEffect>(8000));
  p.quantizer = std::make_shared<TrellisQuantizer>();
  return p;
}

// Note: high-pass at 400 Hz followed by low-pass at 100 Hz leaves almost nothing (see docs/ANALYSIS.md).
inline AudioPipeline Snare2()
{
  AudioPipeline p;
  p.effects.push_back(std::make_shared<NormalizeEffect>());
  p.effects.push_back(std::make_shared<BiquadFilterEffect>(BiquadFilterEffect::FilterType::HighPass, 400, 8));
  p.effects.push_back(std::make_shared<BiquadFilterEffect>(BiquadFilterEffect::FilterType::LowPass, 100, 8));
  p.quantizer = std::make_shared<TrellisQuantizer>();
  return p;
}

inline AudioPipeline Snare3()
{
  AudioPipeline p;
  p.effects.push_back(std::make_shared<NormalizeEffect>());
  p.effects.push_back(std::make_shared<LowPassFilterEffect>(8000));
  p.effects.push_back(std::make_shared<HighPassFilterEffect>(2000));
  p.quantizer = std::make_shared<TrellisQuantizer>();
  return p;
}

inline AudioPipeline Snare4()
{
  AudioPipeline p;
  p.effects.push_back(std::make_shared<NormalizeEffect>());
  p.effects.push_back(std::make_shared<ClippingEffect>(0.2, true));
  p.effects.push_back(std::make_shared<DitherEffect>(0.2));
  p.quantizer = std::make_shared<TrellisQuantizer>();
  return p;
}

inline AudioPipeline Snare5()
{
  AudioPipeline p;
  p.effects.push_back(std::make_shared<NormalizeEffect>());
  p.effects.push_back(std::make_shared<ClippingEffect>(0.2, true));
  p.effects.push_back(std::make_shared<DitherEffect>(0.02));
  p.quantizer = std::make_shared<TrellisQuantizer>();
  return p;
}

inline AudioPipeline Snare6()
{
  AudioPipeline p;
  p.effects.push_back(std::make_shared<NormalizeEffect>());
  p.effects.push_back(std::make_shared<ClippingEffect>(0.4, true));
  p.effects.push_back(std::make_shared<SampleRateReductionEffect>(1000));
  p.effects.push_back(std::make_shared<DitherEffect>(0.0));
  p.quantizer = std::make_shared<TrellisQuantizer>();
  return p;
}

inline AudioPipeline Vox1()
{
  AudioPipeline p;
  p.effects.push_back(std::make_shared<NormalizeEffect>());
  // also tried: SaturateEffect(10.0), ClippingEffect(0.5, true), Biquad HP 200 Hz order 4, Biquad LP 7 kHz order 6, ClippingEffect(0.3, true)
  p.effects.push_back(std::make_shared<DitherEffect>(0.1));
  p.quantizer = std::make_shared<TrellisQuantizer>();
  return p;
}

inline AudioPipeline Vox2()
{
  AudioPipeline p;
  p.effects.push_back(std::make_shared<NormalizeEffect>());
  p.effects.push_back(std::make_shared<SaturateEffect>(2.0));
  p.effects.push_back(std::make_shared<DitherEffect>(0.01));
  p.quantizer = std::make_shared<TrellisQuantizer>();
  return p;
}

inline AudioPipeline Kick1()
{
  AudioPipeline p;
  p.effects.push_back(std::make_shared<NormalizeEffect>());
  p.effects.push_back(std::make_shared<ClippingEffect>(0.8, true));
  // also tried: LowPassFilterEffect(10000.0)
  p.effects.push_back(std::make_shared<SaturateEffect>(2.0));
  p.effects.push_back(std::make_shared<DitherEffect>(0.015));
  p.quantizer = std::make_shared<TrellisQuantizer>();
  return p;
}

// Was last used with DeltaSigmaQuantizer (TrellisQuantizer before that).
inline AudioPipeline Kick2()
{
  AudioPipeline p;
  // also tried: TrimEffect(0, 235) first, DitherEffect(0.015) last
  p.effects.push_back(std::make_shared<NormalizeEffect>());
  p.effects.push_back(std::make_shared<SaturateEffect>(3.0));
  p.effects.push_back(std::make_shared<GainEffect>(GainEffect::FromDecibels(6.0)));
  p.quantizer = std::make_shared<DeltaSigmaQuantizer>();
  return p;
}

} // namespace PipelinePresets
