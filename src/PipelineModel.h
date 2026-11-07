#pragma once

// The pipeline ("chain") that turns a source recording into a 1-bit sample, as data.
//
//   Chain = ordered effects + exactly one quantizer, each a Stage { id, parameter values }.
//
// A registry describes every stage type (label, parameters with range / default / unit), so the editor can be
// generated from it and adding an effect is one registry entry. A chain is saved as text:
//
//   normalize->saturate(10)->dither(0.1)->trellis(64)
//
// ParseChain also reads the text older versions saved for each sample (AudioPipeline::ToString()), e.g.
//   NormalizeEffect->SaturateEffect(10)->DitherEffect(0.1)->TrellisQuantizer(maxRam=67108864)
// so samples from existing projects get an editable chain.

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "src/IAudioEffect.h"

namespace PipelineModel
{

struct ParamSpec
{
  const char* name;
  const char* unit;
  double min, max, def;
  double step; // adjustment granularity in the editor
  bool integer;
};

struct StageSpec
{
  const char* id;    // text id, lowercase
  const char* label; // shown in the editor
  bool isQuantizer;
  std::vector<ParamSpec> params;
  std::function<std::shared_ptr<IAudioEffect>(const std::vector<double>&)> makeEffect;
  std::function<std::shared_ptr<IQuantizer>(const std::vector<double>&)> makeQuantizer;
};

struct Stage
{
  std::string id;
  std::vector<double> params;
  bool operator==(const Stage& o) const { return id == o.id && params == o.params; }
  bool operator!=(const Stage& o) const { return !(*this == o); }
};

struct Chain
{
  std::vector<Stage> effects;
  Stage quantizer;
  bool operator==(const Chain& o) const { return effects == o.effects && quantizer == o.quantizer; }
  bool operator!=(const Chain& o) const { return !(*this == o); }
};

inline const std::vector<StageSpec>& Registry()
{
  using P = ParamSpec;
  static const std::vector<StageSpec> registry = {
    // ---- effects ----
    {"gain", "Gain", false, {P{"Gain", "dB", -24, 24, 0, 0.1, false}}, [](const std::vector<double>& p) { return std::make_shared<GainEffect>(std::pow(10.0, p[0] / 20.0)); }, nullptr},
    {"trim", "Trim", false, {P{"Start", "ms", 0, 20000, 0, 1, false}, P{"Length", "ms", 1, 20000, 1000, 1, false}}, [](const std::vector<double>& p) { return std::make_shared<TrimEffect>(p[0], p[1]); }, nullptr},
    {"normalize", "Normalize", false, {}, [](const std::vector<double>&) { return std::make_shared<NormalizeEffect>(); }, nullptr},
    {"fade", "Fade", false, {P{"In", "ms", 0, 2000, 0, 1, false}, P{"Out", "ms", 0, 2000, 20, 1, false}}, [](const std::vector<double>& p) { return std::make_shared<FadeEffect>(p[0], p[1]); }, nullptr},
    {"dcshift", "DC shift", false, {P{"Offset", "", -1, 1, 0, 0.01, false}}, [](const std::vector<double>& p) { return std::make_shared<DcOffsetEffect>(p[0]); }, nullptr},
    {"lowpass", "Low-pass (1-pole)", false, {P{"Cutoff", "Hz", 20, 20000, 16000, 1, false}}, [](const std::vector<double>& p) { return std::make_shared<LowPassFilterEffect>(p[0]); }, nullptr},
    {"highpass", "High-pass (1-pole)", false, {P{"Cutoff", "Hz", 20, 20000, 80, 1, false}}, [](const std::vector<double>& p) { return std::make_shared<HighPassFilterEffect>(p[0]); }, nullptr},
    {"biquadlp",
     "Low-pass (biquad)",
     false,
     {P{"Cutoff", "Hz", 20, 20000, 8000, 1, false}, P{"Order", "", 2, 8, 2, 2, true}, P{"Q", "", 0.1, 10, 0.7071067811865475, 0.01, false}},
     [](const std::vector<double>& p) { return std::make_shared<BiquadFilterEffect>(BiquadFilterEffect::FilterType::LowPass, p[0], static_cast<uint32_t>(p[1]), p[2]); },
     nullptr},
    {"biquadhp",
     "High-pass (biquad)",
     false,
     {P{"Cutoff", "Hz", 20, 20000, 200, 1, false}, P{"Order", "", 2, 8, 2, 2, true}, P{"Q", "", 0.1, 10, 0.7071067811865475, 0.01, false}},
     [](const std::vector<double>& p) { return std::make_shared<BiquadFilterEffect>(BiquadFilterEffect::FilterType::HighPass, p[0], static_cast<uint32_t>(p[1]), p[2]); },
     nullptr},
    {"saturate", "Saturate", false, {P{"Drive", "", 0.1, 50, 2, 0.1, false}}, [](const std::vector<double>& p) { return std::make_shared<SaturateEffect>(p[0]); }, nullptr},
    {"clip", "Clip", false, {P{"Threshold", "", 0.01, 1, 0.5, 0.01, false}, P{"Normalize after", "", 0, 1, 1, 1, true}}, [](const std::vector<double>& p) { return std::make_shared<ClippingEffect>(p[0], p[1] > 0.5); }, nullptr},
    {"srr", "Sample-rate reduce", false, {P{"Rate", "Hz", 100, 48000, 8000, 10, false}}, [](const std::vector<double>& p) { return std::make_shared<SampleRateReductionEffect>(p[0]); }, nullptr},
    {"dither", "Dither", false, {P{"Amount", "", 0, 1, 0.1, 0.005, false}}, [](const std::vector<double>& p) { return std::make_shared<DitherEffect>(p[0]); }, nullptr},
    // ---- quantizers (exactly one, last) ----
    {"raw", "Raw (threshold)", true, {}, nullptr, [](const std::vector<double>&) { return std::make_shared<RawQuantizer>(); }},
    {"deltasigma", "Delta-sigma", true, {}, nullptr, [](const std::vector<double>&) { return std::make_shared<DeltaSigmaQuantizer>(); }},
    {"trellis", "Trellis (by length)", true, {P{"Max RAM", "MB", 8, 512, 64, 8, true}}, nullptr, [](const std::vector<double>& p) { return std::make_shared<TrellisQuantizer>(static_cast<size_t>(p[0]) * 1024 * 1024); }},
    {"trellis1", "Trellis 1st order", true, {P{"States", "", 17, 257, 65, 16, true}}, nullptr, [](const std::vector<double>& p) { return std::make_shared<TrellisFirstOrderQuantizer>(static_cast<int>(p[0])); }},
    {"trellis3", "Trellis 3rd order", true, {P{"States", "", 17, 257, 129, 16, true}}, nullptr, [](const std::vector<double>& p) { return std::make_shared<TrellisThirdOrderQuantizer>(static_cast<int>(p[0])); }},
  };
  return registry;
}

inline const StageSpec* FindSpec(const std::string& id)
{
  for (const auto& spec : Registry())
    if (id == spec.id)
      return &spec;
  return nullptr;
}

inline double ClampParam(const ParamSpec& spec, double v)
{
  v = std::clamp(v, spec.min, spec.max);
  return spec.integer ? std::round(v) : v;
}

// A stage with default parameters. Unknown ids give an empty stage.
inline Stage DefaultStage(const std::string& id)
{
  Stage stage;
  if (const StageSpec* spec = FindSpec(id))
  {
    stage.id = id;
    for (const auto& p : spec->params)
      stage.params.push_back(p.def);
  }
  return stage;
}

// Makes the parameter list match the spec (missing -> default, extra dropped, all clamped).
inline void NormalizeStage(Stage& stage)
{
  const StageSpec* spec = FindSpec(stage.id);
  if (!spec)
    return;
  const size_t given = stage.params.size();
  stage.params.resize(spec->params.size());
  for (size_t i = 0; i < spec->params.size(); ++i)
  {
    const double v = (i < given && std::isfinite(stage.params[i])) ? stage.params[i] : spec->params[i].def;
    stage.params[i] = ClampParam(spec->params[i], v);
  }
}

// Which pipeline new samples get by default: normalize, saturate(2), dither(0.0125), trellis. This is the drop-box
// chain since the WIP of 2026-10-07 (AudioPipeline::CreateDefaultPipeline()); before that the drop box used saturate(10), dither(0.1).
inline Chain DefaultChain()
{
  Chain chain;
  chain.effects = {Stage{"normalize", {}}, Stage{"saturate", {2.0}}, Stage{"dither", {0.0125}}};
  chain.quantizer = DefaultStage("trellis");
  return chain;
}

// ------------------------------------------------------------------------------------------------ text format

// Shortest text that reads back as the same double.
inline std::string FormatNumber(double v)
{
  char buf[40];
  std::snprintf(buf, sizeof(buf), "%.15g", v);
  if (std::strtod(buf, nullptr) != v)
    std::snprintf(buf, sizeof(buf), "%.17g", v);
  return buf;
}

inline std::string StageToText(const Stage& stage)
{
  std::string out = stage.id;
  if (!stage.params.empty())
  {
    out += "(";
    for (size_t i = 0; i < stage.params.size(); ++i)
    {
      if (i)
        out += ",";
      out += FormatNumber(stage.params[i]);
    }
    out += ")";
  }
  return out;
}

inline std::string ToText(const Chain& chain)
{
  std::string out;
  for (const auto& stage : chain.effects)
    out += StageToText(stage) + "->";
  return out + StageToText(chain.quantizer);
}

namespace detail
{
inline std::string Trim(const std::string& s)
{
  size_t a = 0, b = s.size();
  while (a < b && std::isspace(static_cast<unsigned char>(s[a])))
    ++a;
  while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1])))
    --b;
  return s.substr(a, b - a);
}

// "name(a,b,c)" -> name, {a,b,c}. Tokens may carry a "key=" prefix (old format).
inline bool SplitStage(const std::string& text, std::string& name, std::vector<std::string>& args)
{
  const std::string t = Trim(text);
  const size_t open = t.find('(');
  args.clear();
  if (open == std::string::npos)
  {
    name = t;
    return !name.empty();
  }
  if (t.back() != ')')
    return false;
  name = Trim(t.substr(0, open));
  std::string inner = t.substr(open + 1, t.size() - open - 2);
  size_t start = 0;
  while (start <= inner.size())
  {
    size_t comma = inner.find(',', start);
    if (comma == std::string::npos)
      comma = inner.size();
    std::string token = Trim(inner.substr(start, comma - start));
    const size_t eq = token.find('=');
    if (eq != std::string::npos)
      token = Trim(token.substr(eq + 1));
    if (!token.empty())
      args.push_back(token);
    start = comma + 1;
  }
  return !name.empty();
}

inline bool ToNumber(const std::string& s, double& out)
{
  if (s == "true")
    return out = 1.0, true;
  if (s == "false")
    return out = 0.0, true;
  char* end = nullptr;
  out = std::strtod(s.c_str(), &end);
  return end != s.c_str() && *end == '\0' && std::isfinite(out);
}

// Translates one stage of the text older versions saved (class names, linear gain, bytes) into a Stage.
inline bool LegacyStage(const std::string& name, const std::vector<std::string>& args, Stage& out)
{
  auto num = [&](size_t i, double& v) { return i < args.size() && ToNumber(args[i], v); };
  double a = 0, b = 0, c = 0;
  if (name == "NormalizeEffect")
    return out = DefaultStage("normalize"), true;
  if (name == "GainEffect" && num(0, a) && a > 0)
    return out = Stage{"gain", {20.0 * std::log10(a)}}, true;
  if (name == "TrimEffect" && num(0, a) && num(1, b))
    return out = Stage{"trim", {a, b}}, true;
  if (name == "LowPassFilterEffect" && num(0, a))
    return out = Stage{"lowpass", {a}}, true;
  if (name == "HighPassFilterEffect" && num(0, a))
    return out = Stage{"highpass", {a}}, true;
  if (name == "BiquadFilterEffect" && args.size() >= 4 && num(1, a) && num(2, b) && num(3, c))
  {
    if (args[0] == "LowPass")
      return out = Stage{"biquadlp", {a, b, c}}, true;
    if (args[0] == "HighPass")
      return out = Stage{"biquadhp", {a, b, c}}, true;
    return false;
  }
  if (name == "SaturateEffect" && num(0, a))
    return out = Stage{"saturate", {a}}, true;
  if (name == "ClippingEffect" && num(0, a) && num(1, b))
    return out = Stage{"clip", {a, b}}, true;
  if (name == "SampleRateReductionEffect" && num(0, a))
    return out = Stage{"srr", {a}}, true;
  if (name == "DitherEffect" && num(0, a))
    return out = Stage{"dither", {a}}, true;
  if (name == "FadeEffect" && num(0, a) && num(1, b))
    return out = Stage{"fade", {a, b}}, true;
  if (name == "DcOffsetEffect" && num(0, a))
    return out = Stage{"dcshift", {a}}, true;
  if (name == "RawQuantizer")
    return out = DefaultStage("raw"), true;
  if (name == "DeltaSigmaQuantizer")
    return out = DefaultStage("deltasigma"), true;
  if (name == "TrellisQuantizer")
    return out = Stage{"trellis", {num(0, a) ? std::round(a / (1024.0 * 1024.0)) : 64.0}}, true;
  if (name == "TrellisFirstOrder" && num(0, a))
    return out = Stage{"trellis1", {a}}, true;
  if (name == "TrellisThirdOrder" && num(0, a))
    return out = Stage{"trellis3", {a}}, true;
  return false;
}
} // namespace detail

// Reads a chain from text (the format above, or the old class-name format). Parameters are clamped to their
// ranges. Returns false (and a reason in *error) if the text is not a valid chain; `out` is then unchanged.
inline bool ParseChain(const std::string& text, Chain& out, std::string* error = nullptr)
{
  auto fail = [&](const std::string& why) {
    if (error)
      *error = why;
    return false;
  };

  std::vector<Stage> stages;
  size_t start = 0;
  const std::string t = detail::Trim(text);
  if (t.empty())
    return fail("empty chain");

  while (start <= t.size())
  {
    size_t arrow = t.find("->", start);
    if (arrow == std::string::npos)
      arrow = t.size();
    std::string name;
    std::vector<std::string> args;
    if (!detail::SplitStage(t.substr(start, arrow - start), name, args))
      return fail("bad stage near '" + t.substr(start, arrow - start) + "'");

    Stage stage;
    if (const StageSpec* spec = FindSpec(name))
    {
      stage.id = name;
      for (const auto& arg : args)
      {
        double v = 0;
        if (!detail::ToNumber(arg, v))
          return fail("bad number '" + arg + "' in " + name);
        stage.params.push_back(v);
      }
      if (stage.params.size() > spec->params.size())
        return fail("too many parameters for " + name);
    }
    else if (!detail::LegacyStage(name, args, stage))
    {
      return fail("unknown stage '" + name + "'");
    }
    NormalizeStage(stage);
    stages.push_back(std::move(stage));
    start = arrow + 2;
    if (arrow == t.size())
      break;
  }

  const StageSpec* last = FindSpec(stages.back().id);
  if (!last || !last->isQuantizer)
    return fail("a chain must end with a quantizer");
  for (size_t i = 0; i + 1 < stages.size(); ++i)
  {
    const StageSpec* spec = FindSpec(stages[i].id);
    if (!spec || spec->isQuantizer)
      return fail("only the last stage can be a quantizer");
  }

  out.quantizer = stages.back();
  stages.pop_back();
  out.effects = std::move(stages);
  return true;
}

// ------------------------------------------------------------------------------------------------ building

inline AudioPipeline Build(const Chain& chain)
{
  AudioPipeline pipeline;
  pipeline.quantizer = nullptr;
  for (Stage stage : chain.effects)
  {
    NormalizeStage(stage);
    if (const StageSpec* spec = FindSpec(stage.id))
      if (spec->makeEffect)
        pipeline.effects.push_back(spec->makeEffect(stage.params));
  }
  Stage q = chain.quantizer;
  NormalizeStage(q);
  if (const StageSpec* spec = FindSpec(q.id))
    if (spec->makeQuantizer)
      pipeline.quantizer = spec->makeQuantizer(q.params);
  if (!pipeline.quantizer)
    pipeline.quantizer = std::make_shared<DeltaSigmaQuantizer>();
  return pipeline;
}

// ------------------------------------------------------------------------------------------------ factory presets

struct NamedChain
{
  std::string name;
  std::string text;
};

// The recipes that were tried while sound-designing the samples (see also PipelinePresets.h).
inline const std::vector<NamedChain>& FactoryChains()
{
  static const std::vector<NamedChain> chains = {
    {"Default", "normalize->saturate(2)->dither(0.0125)->trellis(64)"},
    {"Snare 1", "normalize->saturate(4)->srr(8000)->trellis(64)"},
    {"Snare 2 (nearly silent)", "normalize->biquadhp(400,8,0.7071067811865475)->biquadlp(100,8,0.7071067811865475)->trellis(64)"},
    {"Snare 3", "normalize->lowpass(8000)->highpass(2000)->trellis(64)"},
    {"Snare 4", "normalize->clip(0.2,1)->dither(0.2)->trellis(64)"},
    {"Snare 5", "normalize->clip(0.2,1)->dither(0.02)->trellis(64)"},
    {"Snare 6", "normalize->clip(0.4,1)->srr(1000)->dither(0)->trellis(64)"},
    {"Vox 1", "normalize->dither(0.1)->trellis(64)"},
    {"Vox 2", "normalize->saturate(2)->dither(0.01)->trellis(64)"},
    {"Kick 1", "normalize->clip(0.8,1)->saturate(2)->dither(0.015)->trellis(64)"},
    {"Kick 2", "normalize->saturate(3)->gain(6)->deltasigma"},
  };
  return chains;
}

} // namespace PipelineModel
