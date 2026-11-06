#pragma once

// The logic behind the Samples page, without any drawing (so it can be unit-tested): which slot is selected, the
// chain being edited, when to re-render, and the preview data. The plugin feeds it with slots and render results;
// SamplesPanel (the control) draws it and turns mouse events into calls on it.
//
// Editing works on a *draft* of the selected slot's chain. Every change re-renders the slot in the background after
// a short pause (so dragging a knob doesn't start a render per mouse move) and swaps the result into the slot, so
// the sample can be auditioned while editing. "Revert" puts the slot back as it was when it was selected.

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "src/PipelineModel.h"
#include "src/SampleData.h"
#include "src/Spectrogram.h"

class SamplesModel
{
public:
  static constexpr uint64_t kDebounceMs = 250;
  static constexpr size_t kMaxEffects = 12; // what fits on the page

  enum class SourceStatus
  {
    None,     // nothing selected / free slot
    InMemory, // the source audio is available (just imported, embedded, or already loaded)
    File,     // not loaded yet, but the file at `path` exists: it will be read when needed
    Missing,  // no source audio and no file: the sample plays, but can't be edited
  };

  struct Preview
  {
    Spectrogram::Data before; // after the effects, before the 1-bit quantizer (empty until the first render)
    Spectrogram::Data after;  // the 1-bit result
    std::vector<float> waveBefore;
    std::vector<float> waveAfter;
  };

  // ------------------------------------------------------------------------------------------ selection

  int Selected() const { return mSelected; }

  // Select a slot (-1 = none). `slot` is what the sample manager holds for it (nullptr or empty for a free slot);
  // `fileExists` tells whether the file at slot->path exists (checked by the caller, which knows the file system).
  void Select(int index, std::shared_ptr<const SampleData::Slot> slot, bool fileExists)
  {
    mSelected = index;
    mOriginal = slot && !slot->Empty() ? std::move(slot) : nullptr;
    mDirty = mInFlight = false;
    mPendingAutoDc = -1;
    mMessage.clear();
    mPreview = Preview{};
    mChainUnknown = false;
    mChain = PipelineModel::DefaultChain();
    mEmbed = false;
    mStatus = SourceStatus::None;
    if (!mOriginal)
      return;

    mEmbed = mOriginal->embedSource;
    mStatus = mOriginal->source ? SourceStatus::InMemory : (fileExists ? SourceStatus::File : SourceStatus::Missing);
    std::string error;
    if (!PipelineModel::ParseChain(mOriginal->pipeline, mChain, &error))
    {
      // an old sample whose chain wasn't recorded: editing starts from the default chain
      mChain = PipelineModel::DefaultChain();
      mChainUnknown = true;
    }
  }

  // The slot as it was when selected (for Revert), nullptr for a free slot.
  const std::shared_ptr<const SampleData::Slot>& Original() const { return mOriginal; }
  bool HasSample() const { return mOriginal != nullptr; }
  SourceStatus Status() const { return mStatus; }
  void SetStatus(SourceStatus status) { mStatus = status; } // e.g. after the file has been located or loaded
  bool Editable() const { return HasSample() && mStatus != SourceStatus::Missing; }

  // ------------------------------------------------------------------------------------------ the draft chain

  const PipelineModel::Chain& Chain() const { return mChain; }
  // The sample's recorded chain was unknown (old project): the draft is the default chain, not what made the sample.
  bool ChainUnknown() const { return mChainUnknown; }
  bool Embed() const { return mEmbed; }
  void SetEmbed(bool embed) { mEmbed = embed; }

  size_t NumEffects() const { return mChain.effects.size(); }

  // A stage by index: 0..NumEffects()-1 are the effects, NumEffects() is the quantizer.
  const PipelineModel::Stage& StageAt(size_t index) const { return index < mChain.effects.size() ? mChain.effects[index] : mChain.quantizer; }

  bool SetParam(size_t stage, size_t param, double value, uint64_t nowMs)
  {
    if (!Editable() || stage > mChain.effects.size())
      return false;
    PipelineModel::Stage& st = stage < mChain.effects.size() ? mChain.effects[stage] : mChain.quantizer;
    const PipelineModel::StageSpec* spec = PipelineModel::FindSpec(st.id);
    if (!spec || param >= spec->params.size() || param >= st.params.size())
      return false;
    const double v = PipelineModel::ClampParam(spec->params[param], value);
    if (v == st.params[param])
      return false;
    st.params[param] = v;
    Edited(nowMs);
    return true;
  }

  bool AddEffect(const std::string& id, uint64_t nowMs)
  {
    const PipelineModel::StageSpec* spec = PipelineModel::FindSpec(id);
    if (!Editable() || !spec || spec->isQuantizer || mChain.effects.size() >= kMaxEffects)
      return false;
    mChain.effects.push_back(PipelineModel::DefaultStage(id));
    Edited(nowMs);
    return true;
  }

  bool RemoveEffect(size_t index, uint64_t nowMs)
  {
    if (!Editable() || index >= mChain.effects.size())
      return false;
    mChain.effects.erase(mChain.effects.begin() + static_cast<std::ptrdiff_t>(index));
    Edited(nowMs);
    return true;
  }

  // delta = -1 moves the effect earlier in the chain, +1 later.
  bool MoveEffect(size_t index, int delta, uint64_t nowMs)
  {
    const long target = static_cast<long>(index) + delta;
    if (!Editable() || index >= mChain.effects.size() || target < 0 || target >= static_cast<long>(mChain.effects.size()) || delta == 0)
      return false;
    std::swap(mChain.effects[index], mChain.effects[static_cast<size_t>(target)]);
    Edited(nowMs);
    return true;
  }

  bool SetQuantizer(const std::string& id, uint64_t nowMs)
  {
    const PipelineModel::StageSpec* spec = PipelineModel::FindSpec(id);
    if (!Editable() || !spec || !spec->isQuantizer || mChain.quantizer.id == id)
      return false;
    mChain.quantizer = PipelineModel::DefaultStage(id);
    Edited(nowMs);
    return true;
  }

  // Replace the whole chain (loading a preset). The sample keeps its own copy: later changes of the preset don't touch it.
  bool LoadChain(const PipelineModel::Chain& chain, uint64_t nowMs)
  {
    if (!Editable() || chain == mChain)
      return false;
    mChain = chain;
    mChainUnknown = false;
    Edited(nowMs);
    return true;
  }

  // Index of the first stage with this id, or -1.
  int FindEffect(const std::string& id) const
  {
    for (size_t i = 0; i < mChain.effects.size(); ++i)
      if (mChain.effects[i].id == id)
        return static_cast<int>(i);
    return -1;
  }

  // Ask for the DC offset that leaves the least DC (the plugin runs the search and calls SetParam with the result).
  // Adds a DC-shift stage first if the chain has none. Returns the stage index, or -1.
  int RequestAutoDc(uint64_t nowMs)
  {
    if (!Editable())
      return -1;
    int index = FindEffect("dcshift");
    if (index < 0)
    {
      AddEffect("dcshift", nowMs);
      index = static_cast<int>(mChain.effects.size()) - 1;
    }
    mPendingAutoDc = index;
    return index;
  }
  // The stage waiting for an auto-DC search, once; -1 if none.
  int TakePendingAutoDc()
  {
    const int index = mPendingAutoDc;
    mPendingAutoDc = -1;
    return index;
  }

  // ------------------------------------------------------------------------------------------ render scheduling

  bool NeedsRender(uint64_t nowMs) const { return mDirty && !mInFlight && nowMs >= mLastEditMs + kDebounceMs; }
  void RenderStarted()
  {
    mDirty = false; // edits made while the render runs set it again
    mInFlight = true;
  }
  void RenderFinished(const std::string& message = {})
  {
    mInFlight = false;
    mMessage = message;
  }
  bool Busy() const { return mInFlight || mDirty; }

  // ------------------------------------------------------------------------------------------ preview and messages

  const Preview& GetPreview() const { return mPreview; }
  void SetPreview(Preview preview) { mPreview = std::move(preview); }
  const std::string& Message() const { return mMessage; }
  void SetMessage(std::string message) { mMessage = std::move(message); }

private:
  void Edited(uint64_t nowMs)
  {
    mDirty = true;
    mLastEditMs = nowMs;
    mMessage.clear();
  }

  int mSelected = -1;
  std::shared_ptr<const SampleData::Slot> mOriginal;
  PipelineModel::Chain mChain = PipelineModel::DefaultChain();
  bool mChainUnknown = false;
  bool mEmbed = false;
  SourceStatus mStatus = SourceStatus::None;
  bool mDirty = false, mInFlight = false;
  uint64_t mLastEditMs = 0;
  int mPendingAutoDc = -1;
  std::string mMessage;
  Preview mPreview;
};
