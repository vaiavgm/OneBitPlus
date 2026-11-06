#pragma once

// Geometry and texts of the Samples page, computed from the model (pure, no drawing): the panel control draws
// the regions and hit-tests mouse positions against them; the unit tests check them and render a preview picture.
//
//   +-------------+-----------------------------------+-------------------+
//   | slot list   | slot title, source, presets       | preview: after    |
//   | (scrolls)   | chain: one row per effect         |   effects (wave + |
//   |             |   [^][v][x] name  param fields    |   spectrogram)    |
//   |             | + add effect, quantizer           | preview: 1-bit    |
//   |             | [Audition] [Clear] [Revert]       |   result          |
//   +-------------+-----------------------------------+-------------------+

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include "src/PipelineModel.h"
#include "src/SampleData.h"
#include "src/SamplesModel.h"

namespace SamplesLayout
{

struct Rect
{
  float l = 0, t = 0, r = 0, b = 0;
  float W() const { return r - l; }
  float H() const { return b - t; }
  bool Contains(float x, float y) const { return x >= l && x < r && y >= t && y < b; }
};

// the area the panel covers (the keyboard strip below it stays visible)
constexpr Rect kPanel{0.f, 26.f, 1300.f, 605.f};

enum class Kind
{
  Label,   // text only
  Hint,    // dim text only
  Slot,    // a row of the slot list: a = slot index
  Button,  // click: action
  Toggle,  // click: action; selected = on
  Field,   // a parameter: a = stage, b = parameter; drag to change, double-click to type
  Preview, // where a preview picture goes: action says which
};

struct Region
{
  Rect r;
  Kind kind = Kind::Label;
  std::string action;
  int a = 0, b = 0;
  std::string text;
  bool enabled = true;
  bool selected = false;
};

struct SlotInfo
{
  bool used = false;
  std::string name;
  size_t bytes = 0;
};

struct Input
{
  const SamplesModel* model = nullptr;
  int firstSlot = 0;                               // list scroll position
  std::function<SlotInfo(int)> slotInfo;           // what to show for a slot
  int numSlots = static_cast<int>(SampleData::SampleManager::kMaxDynamicSlots);
  int firstVelocity = static_cast<int>(SampleData::SampleManager::kPreparedCount) + 1; // velocity of slot 0
};

constexpr int kRowHeight = 22;
constexpr int kVisibleSlots = 24;

inline std::string BaseName(const std::string& path)
{
  const size_t slash = path.find_last_of("/\\");
  return slash == std::string::npos ? path : path.substr(slash + 1);
}

inline std::string Shorten(const std::string& s, size_t max)
{
  return s.size() <= max ? s : s.substr(0, max - 1) + "~";
}

// "Cutoff 8000 Hz", "Order 4", "Offset -0.12"
inline std::string FieldText(const PipelineModel::ParamSpec& spec, double value)
{
  char buf[64];
  if (spec.integer)
    std::snprintf(buf, sizeof(buf), "%s %d", spec.name, static_cast<int>(std::lround(value)));
  else
    std::snprintf(buf, sizeof(buf), "%s %.4g", spec.name, value);
  std::string out = buf;
  if (spec.unit && *spec.unit)
    out += std::string(" ") + spec.unit;
  return out;
}

inline int MaxFirstSlot(const Input& in) { return std::max(0, in.numSlots - kVisibleSlots); }

inline std::vector<Region> Compute(const Input& in)
{
  using namespace PipelineModel;
  std::vector<Region> out;
  const SamplesModel& m = *in.model;
  auto add = [&](Rect r, Kind kind, std::string action, std::string text, int a = 0, int b = 0, bool enabled = true, bool selected = false) {
    Region g;
    g.r = r;
    g.kind = kind;
    g.action = std::move(action);
    g.text = std::move(text);
    g.a = a;
    g.b = b;
    g.enabled = enabled;
    g.selected = selected;
    out.push_back(std::move(g));
  };

  // ------------------------------------------------------------------ slot list
  const float lx = 10, ly = 36, lw = 330;
  add({lx, ly, lx + lw, ly + 24}, Kind::Label, "", "Slots  (velocity = slot number)");
  for (int i = 0; i < kVisibleSlots; ++i)
  {
    const int slot = in.firstSlot + i;
    if (slot >= in.numSlots)
      break;
    const SlotInfo info = in.slotInfo ? in.slotInfo(slot) : SlotInfo{};
    char text[96];
    std::snprintf(text, sizeof(text), "%3d   %s", in.firstVelocity + slot, info.used ? Shorten(info.name.empty() ? "(unnamed)" : info.name, 28).c_str() : "-");
    const float top = ly + 28 + static_cast<float>(i * kRowHeight);
    add({lx, top, lx + lw, top + kRowHeight - 1}, Kind::Slot, "slot", text, slot, 0, true, slot == m.Selected());
  }
  {
    char scroll[64];
    std::snprintf(scroll, sizeof(scroll), "%d-%d of %d  (mouse wheel scrolls)", in.firstSlot + 1, std::min(in.numSlots, in.firstSlot + kVisibleSlots), in.numSlots);
    add({lx, ly + 28 + static_cast<float>(kVisibleSlots * kRowHeight) + 2, lx + lw, kPanel.b - 2}, Kind::Hint, "", scroll);
  }

  // ------------------------------------------------------------------ editor
  const float ex = 355, ey = 36, ew = 545;
  if (m.Selected() < 0)
  {
    add({ex, ey, ex + ew, ey + 24}, Kind::Label, "", "Select a slot, or drop a WAV file here.");
    if (!m.Message().empty())
      add({ex, ey + 26, ex + ew, ey + 48}, Kind::Label, "", m.Message());
  }
  else if (!m.HasSample())
  {
    char t[96];
    std::snprintf(t, sizeof(t), "Slot %d (velocity %d) is free. Drop a WAV file here to fill it.", m.Selected() + 1, in.firstVelocity + m.Selected());
    add({ex, ey, ex + ew, ey + 24}, Kind::Label, "", t);
    if (!m.Message().empty())
      add({ex, ey + 26, ex + ew, ey + 48}, Kind::Label, "", m.Message());
  }
  else
  {
    const auto& orig = m.Original();
    char t[160];
    std::snprintf(t, sizeof(t), "Slot %d (velocity %d)   %s", m.Selected() + 1, in.firstVelocity + m.Selected(), Shorten(BaseName(orig->path), 40).c_str());
    add({ex, ey, ex + ew, ey + 24}, Kind::Label, "", t);

    // status line
    std::string status;
    switch (m.Status())
    {
    case SamplesModel::SourceStatus::InMemory: status = "Source: in memory"; break;
    case SamplesModel::SourceStatus::File: status = "Source: file (read when you edit)"; break;
    case SamplesModel::SourceStatus::Missing: status = "Source: MISSING. Locate the file to edit this sample."; break;
    default: break;
    }
    if (!m.Message().empty())
      status = m.Message();
    else if (m.Busy())
      status += "   (rendering...)";
    add({ex, ey + 26, ex + ew - 250, ey + 48}, Kind::Label, "", status);
    add({ex + ew - 245, ey + 26, ex + ew - 130, ey + 48}, Kind::Button, "locate", "Locate file...");
    add({ex + ew - 125, ey + 26, ex + ew, ey + 48}, Kind::Toggle, "embed", "Embed source", 0, 0, m.Status() != SamplesModel::SourceStatus::Missing, m.Embed());

    // presets
    add({ex, ey + 54, ex + 150, ey + 76}, Kind::Button, "preset_load", "Load preset...", 0, 0, m.Editable());
    add({ex + 156, ey + 54, ex + 306, ey + 76}, Kind::Button, "preset_save", "Save as preset...", 0, 0, m.HasSample());
    add({ex + 312, ey + 54, ex + 462, ey + 76}, Kind::Button, "preset_delete", "Delete preset...");
    if (m.ChainUnknown())
      add({ex, ey + 80, ex + ew, ey + 96}, Kind::Hint, "", "How this sample was made is not recorded: editing starts from the default chain.");

    // chain
    const float cy = ey + 100, rowH = 30;
    const size_t n = m.NumEffects();
    for (size_t i = 0; i < n; ++i)
    {
      const Stage& st = m.StageAt(i);
      const StageSpec* spec = FindSpec(st.id);
      const float top = cy + static_cast<float>(i) * rowH;
      const bool ed = m.Editable();
      add({ex, top, ex + 24, top + 26}, Kind::Button, "moveup", "^", static_cast<int>(i), 0, ed && i > 0);
      add({ex + 26, top, ex + 50, top + 26}, Kind::Button, "movedown", "v", static_cast<int>(i), 0, ed && i + 1 < n);
      add({ex + 52, top, ex + 76, top + 26}, Kind::Button, "remove", "x", static_cast<int>(i), 0, ed);
      add({ex + 82, top, ex + 232, top + 26}, Kind::Label, "", spec ? spec->label : st.id);
      if (spec)
      {
        for (size_t p = 0; p < spec->params.size() && p < st.params.size(); ++p)
          add({ex + 238 + static_cast<float>(p) * 104, top, ex + 238 + static_cast<float>(p) * 104 + 100, top + 26}, Kind::Field, "field", FieldText(spec->params[p], st.params[p]), static_cast<int>(i), static_cast<int>(p), ed);
        if (st.id == "dcshift")
          add({ex + 238 + 104, top, ex + 238 + 104 + 100, top + 26}, Kind::Button, "autodc", "Auto (least DC)", static_cast<int>(i), 0, ed);
      }
    }
    const float addTop = cy + static_cast<float>(n) * rowH;
    add({ex, addTop, ex + 150, addTop + 26}, Kind::Button, "addeffect", "+ Add effect...", 0, 0, m.Editable() && n < SamplesModel::kMaxEffects);

    const float qTop = addTop + 36;
    const Stage& q = m.Chain().quantizer;
    const StageSpec* qspec = FindSpec(q.id);
    add({ex, qTop, ex + 76, qTop + 26}, Kind::Label, "", "Quantizer");
    add({ex + 82, qTop, ex + 232, qTop + 26}, Kind::Button, "quantizer", qspec ? qspec->label : q.id, 0, 0, m.Editable());
    if (qspec)
      for (size_t p = 0; p < qspec->params.size() && p < q.params.size(); ++p)
        add({ex + 238 + static_cast<float>(p) * 104, qTop, ex + 238 + static_cast<float>(p) * 104 + 100, qTop + 26}, Kind::Field, "field", FieldText(qspec->params[p], q.params[p]), static_cast<int>(n), static_cast<int>(p), m.Editable());

    // bottom buttons
    const float by = kPanel.b - 34;
    add({ex, by, ex + 170, by + 28}, Kind::Button, "audition", "Audition (hold)", 0, 0, true);
    add({ex + 180, by, ex + 330, by + 28}, Kind::Button, "clear", "Clear slot", 0, 0, true);
    add({ex + 340, by, ex + 490, by + 28}, Kind::Button, "revert", "Revert", 0, 0, m.Editable());
  }

  // ------------------------------------------------------------------ preview
  const float px = 915, pw = 375;
  add({px, 36, px + pw, 54}, Kind::Label, "", "After the effects (before the 1-bit quantizer)");
  add({px, 56, px + pw, 116}, Kind::Preview, "wave_before", "");
  add({px, 120, px + pw, 270}, Kind::Preview, "spec_before", "");
  add({px, 282, px + pw, 300}, Kind::Label, "", "1-bit result");
  add({px, 302, px + pw, 362}, Kind::Preview, "wave_after", "");
  add({px, 366, px + pw, 516}, Kind::Preview, "spec_after", "");
  add({px, 522, px + pw, 566}, Kind::Hint, "", "Spectrograms: time across, frequency up (log, 30 Hz to Nyquist). A 1-bit stream always has shaped noise at the top.");
  return out;
}

// The topmost region that reacts to the mouse at (x, y), or nullptr.
inline const Region* HitTest(const std::vector<Region>& regions, float x, float y)
{
  for (auto it = regions.rbegin(); it != regions.rend(); ++it)
    if (it->enabled && it->r.Contains(x, y) && (it->kind == Kind::Slot || it->kind == Kind::Button || it->kind == Kind::Toggle || it->kind == Kind::Field))
      return &*it;
  return nullptr;
}

} // namespace SamplesLayout
