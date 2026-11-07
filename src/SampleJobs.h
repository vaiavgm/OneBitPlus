#pragma once

// The work the Samples page and the drop box hand to the background thread: importing a WAV, re-rendering a slot
// with an edited chain, searching the DC offset. Pure (no plugin state), so the whole flow is unit-tested.

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "src/PipelineModel.h"
#include "src/SampleData.h"
#include "src/SampleRender.h"
#include "src/SamplesModel.h"

namespace SampleJobs
{

struct Job
{
  enum class Kind
  {
    Import, // read a WAV file, render it, put it into a slot
    Render, // render the slot's source again with a new chain (editing)
    AutoDc, // search the DC offset that leaves the least DC
  };
  Kind kind = Kind::Import;
  int slot = -1;                // Import: target slot or -1 = first free. Render / AutoDc: the slot
  std::string path;             // the source file
  uint32_t sampleRate = 44100;  // render rate (the host rate)
  PipelineModel::Chain chain;
  std::shared_ptr<const SampleData::SourceAudio> source; // already in memory, if it is
  size_t dcStage = 0;           // AutoDc: which effect of the chain
  uint64_t generation = 0;      // Render: dropped if the slot was cleared / reverted / switched meanwhile
};

struct Result
{
  Job::Kind kind = Job::Kind::Import;
  int slot = -1;
  bool ok = false;
  bool stale = false; // superseded while it ran: ignore it
  bool sourceMissing = false;
  std::string error;
  double dcOffset = 0.0;
  size_t dcStage = 0;
  SamplesModel::Preview preview;
};

inline std::string BaseName(const std::string& path)
{
  const size_t slash = path.find_last_of("/\\");
  return slash == std::string::npos ? path : path.substr(slash + 1);
}

// Runs one job. `generation` is the plugin's current edit generation: a Render started under an older one is
// dropped. `onImported` (optional) is called with the packed bits of a finished import.
inline Result Run(SampleData::SampleManager& manager, const std::atomic<uint64_t>& generation, const Job& job, const std::function<void(const std::string&, const std::vector<int8_t>&)>& onImported = nullptr)
{
  Result result;
  result.kind = job.kind;
  result.slot = job.slot;
  result.dcStage = job.dcStage;
  auto fail = [&](const std::string& why) {
    result.ok = false;
    result.error = why;
    return result;
  };

  // the source: in memory, or read from the file (an Import always reads the file)
  std::shared_ptr<const SampleData::SourceAudio> source = job.kind == Job::Kind::Import ? nullptr : job.source;
  if (!source)
    source = SampleRender::LoadSourceFile(job.path);
  if (!source)
  {
    result.sourceMissing = true;
    return fail("Can't read the source file " + BaseName(job.path) + " (a WAV file is needed). Use Locate file.");
  }

  if (job.kind == Job::Kind::AutoDc)
  {
    result.dcOffset = SampleRender::FindDcOffset(*source, job.chain, job.dcStage, job.sampleRate);
    result.ok = true;
    return result;
  }

  SampleRender::Result rendered = SampleRender::Render(*source, job.chain, job.sampleRate, true);
  if (!rendered.ok())
    return fail("The chain left nothing of the sample (a Trim that is too short?).");

  SampleData::Slot slot;
  slot.data = rendered.bits;
  slot.sampleRate = rendered.sampleRate;
  slot.path = job.path;
  slot.pipeline = PipelineModel::ToText(job.chain);
  slot.source = source;

  if (job.kind == Job::Kind::Import)
  {
    // AddSlot / SetSlot are thread-safe (they publish a new sample bank)
    result.slot = job.slot >= 0 ? (manager.SetSlot(job.slot, std::move(slot)) ? job.slot : -1) : manager.AddSlot(std::move(slot));
    if (result.slot < 0)
      return fail("All " + std::to_string(SampleData::SampleManager::kMaxDynamicSlots) + " slots are used. Clear one first.");
    if (onImported)
      onImported(job.path, rendered.bits);
  }
  else
  {
    // keep what the slot says about itself (embed flag, path), replace what was rendered
    const auto current = manager.GetSlot(job.slot);
    if (!current || current->Empty())
    {
      result.stale = true; // cleared meanwhile
      return result;
    }
    slot.embedSource = current->embedSource;
    slot.path = current->path;
    // the slot was cleared, reverted or changed to another sample since this render started: throw the result away
    if (job.generation != generation.load())
    {
      result.stale = true;
      return result;
    }
    manager.SetSlot(job.slot, std::move(slot));
  }

  result.preview = SamplesModel::MakePreview(rendered.bits, rendered.preQuantizer, rendered.sampleRate);
  result.ok = true;
  return result;
}

} // namespace SampleJobs
