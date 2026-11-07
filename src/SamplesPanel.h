#pragma once

// The Samples page: draws what SamplesLayout computes and turns mouse events into calls on SamplesModel.
// All decisions live in the model and the layout (both unit-tested); this control only draws and routes input.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "IControl.h"
#include "src/ChainPresets.h"
#include "src/SamplesLayout.h"
#include "src/SamplesModel.h"
#include "src/Spectrogram.h"

using namespace iplug;
using namespace igraphics;

class SamplesPanel : public IControl
{
public:
  // What the panel needs from the plugin (it can't touch the sample bank or the worker itself).
  struct Callbacks
  {
    std::function<SamplesLayout::SlotInfo(int)> slotInfo;
    std::function<void(int)> select;           // a slot was clicked
    std::function<void(bool)> audition;        // true = key down, false = key up
    std::function<void()> clearSlot;
    std::function<void()> revert;
    std::function<void(bool)> setEmbed;
    std::function<void(const std::string&)> locate;   // the user picked the source file of the selected slot
    std::function<void(const std::string&)> dropFile; // a WAV was dropped on the page
  };

  SamplesPanel(const IRECT& bounds, SamplesModel& model, ChainPresets::Store& presets, Callbacks callbacks)
    : IControl(bounds)
    , mModel(model)
    , mPresets(presets)
    , mCb(std::move(callbacks))
  {
  }

  void Draw(IGraphics& g) override
  {
    const IColor bg(255, 34, 36, 42), panel(255, 48, 51, 60), panelOff(255, 40, 42, 48), frame(255, 90, 95, 110), text(255, 225, 228, 235),
      dim(255, 140, 146, 160), accent(255, 70, 120, 200), good(255, 60, 150, 90);
    const IText label(14.f, text, "Roboto-Regular", EAlign::Near, EVAlign::Middle, 0.f);
    const IText hint(12.f, dim, "Roboto-Regular", EAlign::Near, EVAlign::Top, 0.f);
    const IText center(14.f, text, "Roboto-Regular", EAlign::Center, EVAlign::Middle, 0.f);
    const IText centerOff(14.f, dim, "Roboto-Regular", EAlign::Center, EVAlign::Middle, 0.f);

    g.FillRect(bg, mRECT);
    const auto regions = Regions();
    for (const auto& r : regions)
    {
      const IRECT rect = ToRect(r.r);
      switch (r.kind)
      {
      case SamplesLayout::Kind::Label:
        g.DrawText(label, r.text.c_str(), rect);
        break;
      case SamplesLayout::Kind::Hint:
        g.DrawText(hint, r.text.c_str(), rect);
        break;
      case SamplesLayout::Kind::Slot:
        g.FillRect(r.selected ? accent : panel, rect);
        g.DrawText(label, r.text.c_str(), rect.GetPadded(-6.f, 0.f, 0.f, 0.f));
        break;
      case SamplesLayout::Kind::Button:
      case SamplesLayout::Kind::Toggle:
        g.FillRect(r.selected ? good : (r.enabled ? panel : panelOff), rect);
        g.DrawRect(frame, rect);
        g.DrawText(r.enabled ? center : centerOff, r.text.c_str(), rect);
        break;
      case SamplesLayout::Kind::Field: {
        const bool dragging = mDragStage == r.a && mDragParam == r.b;
        g.FillRect(dragging ? accent : panelOff, rect);
        g.DrawRect(frame, rect);
        g.DrawText(r.enabled ? center : centerOff, r.text.c_str(), rect);
        break;
      }
      case SamplesLayout::Kind::Preview:
        DrawPreview(g, r, rect, frame);
        break;
      }
    }
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    const auto regions = Regions();
    const SamplesLayout::Region* hit = SamplesLayout::HitTest(regions, x, y);
    if (!hit)
      return;
    const uint64_t now = NowMs();
    const std::string& action = hit->action;

    if (action == "slot")
    {
      if (mCb.select)
        mCb.select(hit->a);
    }
    else if (action == "audition")
    {
      mAuditioning = true;
      if (mCb.audition)
        mCb.audition(true);
    }
    else if (action == "field")
    {
      mDragStage = hit->a;
      mDragParam = hit->b;
      mDragStartY = y;
      mDragStartValue = mModel.StageAt(static_cast<size_t>(hit->a)).params[static_cast<size_t>(hit->b)];
    }
    else if (action == "moveup")
      mModel.MoveEffect(static_cast<size_t>(hit->a), -1, now);
    else if (action == "movedown")
      mModel.MoveEffect(static_cast<size_t>(hit->a), +1, now);
    else if (action == "remove")
      mModel.RemoveEffect(static_cast<size_t>(hit->a), now);
    else if (action == "autodc")
      mModel.RequestAutoDc(now);
    else if (action == "embed")
    {
      if (mCb.setEmbed)
        mCb.setEmbed(!mModel.Embed());
    }
    else if (action == "clear")
    {
      if (mCb.clearSlot)
        mCb.clearSlot();
    }
    else if (action == "revert")
    {
      if (mCb.revert)
        mCb.revert();
    }
    else if (action == "addeffect")
    {
      mMenu = std::make_unique<IPopupMenu>("");
      mMenuIds.clear();
      for (const auto& spec : PipelineModel::Registry())
        if (!spec.isQuantizer)
        {
          mMenu->AddItem(spec.label);
          mMenuIds.push_back(spec.id);
        }
      GetUI()->CreatePopupMenu(*this, *mMenu, ToRect(hit->r), kMenuAddEffect);
    }
    else if (action == "quantizer")
    {
      mMenu = std::make_unique<IPopupMenu>("");
      mMenuIds.clear();
      for (const auto& spec : PipelineModel::Registry())
        if (spec.isQuantizer)
        {
          mMenu->AddItem(spec.label);
          mMenuIds.push_back(spec.id);
        }
      GetUI()->CreatePopupMenu(*this, *mMenu, ToRect(hit->r), kMenuQuantizer);
    }
    else if (action == "preset_load" || action == "preset_delete")
    {
      const bool forDelete = action == "preset_delete";
      mPresetList.clear();
      mMenu = std::make_unique<IPopupMenu>("");
      for (const auto& p : mPresets.List())
      {
        if (forDelete && !p.user)
          continue;
        mMenu->AddItem((p.name + (p.factory ? "" : (p.user ? "  (mine)" : "  (shared)"))).c_str());
        mPresetList.push_back(p);
      }
      if (mPresetList.empty())
        mMenu->AddItem("(no presets)", -1, IPopupMenu::Item::kDisabled);
      GetUI()->CreatePopupMenu(*this, *mMenu, ToRect(hit->r), forDelete ? kMenuPresetDelete : kMenuPresetLoad);
    }
    else if (action == "preset_save")
    {
      GetUI()->CreateTextEntry(*this, IText(14.f, COLOR_BLACK, "Roboto-Regular"), ToRect(hit->r), "My preset", kTextSavePreset);
    }
    else if (action == "locate")
    {
      WDL_String file, dir;
      GetUI()->PromptForFile(file, dir, EFileAction::Open, "wav", [this](const WDL_String& fileName, const WDL_String&) {
        if (fileName.GetLength() > 0 && mCb.locate)
          mCb.locate(fileName.Get());
      });
    }
    SetDirty(false);
  }

  void OnMouseDrag(float x, float y, float, float, const IMouseMod& mod) override
  {
    if (mDragStage < 0)
      return;
    const PipelineModel::Stage& st = mModel.StageAt(static_cast<size_t>(mDragStage));
    const PipelineModel::StageSpec* spec = PipelineModel::FindSpec(st.id);
    if (!spec || static_cast<size_t>(mDragParam) >= spec->params.size())
      return;
    const PipelineModel::ParamSpec& p = spec->params[static_cast<size_t>(mDragParam)];

    // dragging up raises the value: the whole range over 300 pixels, 10x finer with shift
    const double pixels = static_cast<double>(mDragStartY - y);
    double value = mDragStartValue + pixels * (p.max - p.min) / (mod.S ? 3000.0 : 300.0);
    if (p.step > 0.0)
      value = std::round(value / p.step) * p.step;
    if (mModel.SetParam(static_cast<size_t>(mDragStage), static_cast<size_t>(mDragParam), value, NowMs()))
      SetDirty(false);
  }

  void OnMouseUp(float, float, const IMouseMod&) override
  {
    mDragStage = mDragParam = -1;
    if (mAuditioning)
    {
      mAuditioning = false;
      if (mCb.audition)
        mCb.audition(false);
    }
    SetDirty(false);
  }

  void OnMouseDblClick(float x, float y, const IMouseMod&) override
  {
    // double-click a parameter field to type its value
    const auto regions = Regions();
    const SamplesLayout::Region* hit = SamplesLayout::HitTest(regions, x, y);
    if (!hit || hit->action != "field")
      return;
    mDragStage = mDragParam = -1;
    const PipelineModel::Stage& st = mModel.StageAt(static_cast<size_t>(hit->a));
    const std::string value = PipelineModel::FormatNumber(st.params[static_cast<size_t>(hit->b)]);
    GetUI()->CreateTextEntry(*this, IText(14.f, COLOR_BLACK, "Roboto-Regular"), ToRect(hit->r), value.c_str(), kTextParamBase + hit->a * 8 + hit->b);
  }

  void OnTextEntryCompletion(const char* str, int valIdx) override
  {
    if (!str)
      return;
    if (valIdx == kTextSavePreset)
    {
      std::string error;
      if (!mPresets.Save(str, mModel.Chain(), &error))
        mModel.SetMessage("Can't save the preset: " + error);
      else
        mModel.SetMessage(std::string("Saved preset '") + str + "'");
    }
    else if (valIdx >= kTextParamBase)
    {
      const int stage = (valIdx - kTextParamBase) / 8, param = (valIdx - kTextParamBase) % 8;
      char* end = nullptr;
      const double v = std::strtod(str, &end);
      if (end != str)
        mModel.SetParam(static_cast<size_t>(stage), static_cast<size_t>(param), v, NowMs());
    }
    SetDirty(false);
  }

  void OnPopupMenuSelection(IPopupMenu* pSelectedMenu, int valIdx) override
  {
    if (!pSelectedMenu)
      return;
    const int idx = pSelectedMenu->GetChosenItemIdx();
    if (idx < 0)
      return;
    const uint64_t now = NowMs();
    if (valIdx == kMenuAddEffect && static_cast<size_t>(idx) < mMenuIds.size())
      mModel.AddEffect(mMenuIds[static_cast<size_t>(idx)], now);
    else if (valIdx == kMenuQuantizer && static_cast<size_t>(idx) < mMenuIds.size())
      mModel.SetQuantizer(mMenuIds[static_cast<size_t>(idx)], now);
    else if (valIdx == kMenuPresetLoad && static_cast<size_t>(idx) < mPresetList.size())
      mModel.LoadChain(mPresetList[static_cast<size_t>(idx)].chain, now);
    else if (valIdx == kMenuPresetDelete && static_cast<size_t>(idx) < mPresetList.size())
    {
      mPresets.Delete(mPresetList[static_cast<size_t>(idx)].name);
      mModel.SetMessage("Deleted preset '" + mPresetList[static_cast<size_t>(idx)].name + "'");
    }
    SetDirty(false);
  }

  void OnMouseWheel(float x, float y, const IMouseMod&, float d) override
  {
    if (x > 350.f)
      return;
    SamplesLayout::Input in = Input();
    mFirstSlot = std::clamp(mFirstSlot - (d > 0 ? 3 : -3), 0, SamplesLayout::MaxFirstSlot(in));
    SetDirty(false);
  }

  void OnDrop(const char* str) override
  {
    if (str && mCb.dropFile)
      mCb.dropFile(str);
  }

  // Make sure a slot is visible in the list (after an import, for example).
  void ScrollToSlot(int slot)
  {
    if (slot < mFirstSlot)
      mFirstSlot = slot;
    else if (slot >= mFirstSlot + SamplesLayout::kVisibleSlots)
      mFirstSlot = slot - SamplesLayout::kVisibleSlots + 1;
    SetDirty(false);
  }

  static uint64_t NowMs() { return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()); }

private:
  enum
  {
    kMenuAddEffect = 1,
    kMenuQuantizer,
    kMenuPresetLoad,
    kMenuPresetDelete,
    kTextSavePreset,
    kTextParamBase = 100,
  };

  static IRECT ToRect(const SamplesLayout::Rect& r) { return IRECT(r.l, r.t, r.r, r.b); }

  SamplesLayout::Input Input() const
  {
    SamplesLayout::Input in;
    in.model = &mModel;
    in.firstSlot = mFirstSlot;
    in.slotInfo = mCb.slotInfo;
    return in;
  }
  std::vector<SamplesLayout::Region> Regions() const { return SamplesLayout::Compute(Input()); }

  static IColor HeatColor(uint8_t level)
  {
    const float v = level / 255.f;
    auto ramp = [](float x) { return std::clamp(x, 0.f, 1.f); };
    return IColor(255, static_cast<int>(255 * ramp((v - 0.4f) / 0.6f)), static_cast<int>(255 * ramp((v - 0.7f) / 0.3f)), static_cast<int>(255 * ramp(v / 0.4f) * (1.f - ramp((v - 0.6f) / 0.4f))));
  }

  void DrawSpectrogram(IGraphics& g, const Spectrogram::Data& sg, const IRECT& rect)
  {
    if (sg.empty())
      return;
    const float cw = rect.W() / sg.cols, rh = rect.H() / sg.rows;
    for (int c = 0; c < sg.cols; ++c)
    {
      const float x0 = rect.L + c * cw, x1 = rect.L + (c + 1) * cw;
      for (int r = 0; r < sg.rows;)
      {
        const uint8_t shade = sg.At(c, r) & 0xF0; // 16 shades: merge runs of equal shade into one rectangle
        int r2 = r + 1;
        while (r2 < sg.rows && (sg.At(c, r2) & 0xF0) == shade)
          ++r2;
        g.FillRect(HeatColor(static_cast<uint8_t>(shade + 8)), IRECT(x0, rect.T + r * rh, x1 + 0.5f, rect.T + r2 * rh + 0.5f));
        r = r2;
      }
    }
  }

  void DrawWave(IGraphics& g, const std::vector<float>& wave, const IRECT& rect, uint64_t version, std::vector<Spectrogram::MinMax>& cache, uint64_t& cacheVersion, int& cacheWidth)
  {
    const int width = static_cast<int>(rect.W());
    if (cacheVersion != version || cacheWidth != width)
    {
      cache = Spectrogram::Envelope(wave, width);
      cacheVersion = version;
      cacheWidth = width;
    }
    if (wave.empty())
      return;
    const float mid = rect.MH(), half = rect.H() * 0.5f - 2.f;
    for (int p = 0; p < width; ++p)
    {
      const float x = rect.L + static_cast<float>(p);
      float top = mid - cache[static_cast<size_t>(p)].hi * half, bottom = mid - cache[static_cast<size_t>(p)].lo * half;
      if (bottom - top < 1.f)
        bottom = top + 1.f;
      g.DrawLine(COLOR_WHITE, x, top, x, bottom);
    }
  }

  void DrawPreview(IGraphics& g, const SamplesLayout::Region& r, const IRECT& rect, const IColor& frame)
  {
    g.FillRect(IColor(255, 20, 21, 26), rect);
    g.DrawRect(frame, rect);
    const auto& pv = mModel.GetPreview();
    const uint64_t version = mModel.PreviewVersion();
    const IText hint(12.f, IColor(255, 140, 146, 160), "Roboto-Regular", EAlign::Center, EVAlign::Middle, 0.f);
    if (r.action == "wave_before")
    {
      DrawWave(g, pv.waveBefore, rect, version, mEnvBefore, mEnvBeforeVersion, mEnvBeforeWidth);
      if (pv.waveBefore.empty())
        g.DrawText(hint, "(edit the chain to see this)", rect);
    }
    else if (r.action == "wave_after")
      DrawWave(g, pv.waveAfter, rect, version, mEnvAfter, mEnvAfterVersion, mEnvAfterWidth);
    else if (r.action == "spec_before")
    {
      DrawSpectrogram(g, pv.before, rect);
      if (pv.before.empty())
        g.DrawText(hint, "(edit the chain to see this)", rect);
    }
    else if (r.action == "spec_after")
      DrawSpectrogram(g, pv.after, rect);
  }

  SamplesModel& mModel;
  ChainPresets::Store& mPresets;
  Callbacks mCb;

  int mFirstSlot = 0;
  int mDragStage = -1, mDragParam = -1;
  float mDragStartY = 0.f;
  double mDragStartValue = 0.0;
  bool mAuditioning = false;

  std::unique_ptr<IPopupMenu> mMenu;
  std::vector<std::string> mMenuIds;
  std::vector<ChainPresets::Preset> mPresetList;

  std::vector<Spectrogram::MinMax> mEnvBefore, mEnvAfter;
  uint64_t mEnvBeforeVersion = ~0ull, mEnvAfterVersion = ~0ull;
  int mEnvBeforeWidth = -1, mEnvAfterWidth = -1;
};
