#pragma once

#include "src/Params.h"

#if IPLUG_DSP
#include "VaiaOneBitPlus_DSP.h"
#endif

#include <IPlug_include_in_plug_hdr.h>
#include <IPlugConstants.h>
#include <IPlugMidi.h>
#include <ISender.h>

#include "src/SampleTools.h" 
#include "src/IAudioEffect.h" 
#include "src/IQuantizer.h"
#include "src/SampleImportWorker.h"
#include "src/StateFormat.h"
#include "src/DragDropWaveformDisplay.h"
#include "src/ChainPresets.h"
#include "src/PipelineModel.h"
#include "src/SampleJobs.h"
#include "src/SampleRender.h"
#include "src/SamplesModel.h"
#include "src/SamplesPanel.h"
#include <atomic>
#include <filesystem>
#include <string.h>

class VaiaOneBitPlus final : public Plugin
{
public:
  explicit VaiaOneBitPlus(const InstanceInfo& info);

#if IPLUG_DSP // http://bit.ly/2S64BDd

  void ProcessBlock(sample** inputs, sample** outputs, int nFrames) override;
  void ProcessMidiMsg(const IMidiMsg& msg) override;
  void OnReset() override;
  void OnParamChange(int paramIdx) override;
  void OnIdle() override;
  bool SerializeState(IByteChunk& chunk) const override;
  int UnserializeState(const IByteChunk& chunk, int startPos) override;
  bool OnMessage(int msgTag, int ctrlTag, int dataSize, const void* pData) override;

  // Imports a WAV file in the background with the default chain: into slot `slot`, or the first free one (-1).
  // The result is picked up in OnIdle, which selects the new slot.
  void ImportFile(const std::string& path, int slot = -1);
  // Builds a C++ array definition of the packed sample (for SampleData.h) and writes it to the debugger output.
  std::string PrintBits(const char* filePath, const std::vector<int8_t>& packed1Bit);

  void OnUIClose() override;
  void OnParamChangeUI(int paramIdx, EParamSource source) override;

  OneBitPlusDSP<sample> mDSP{ 32 };
  // audio -> UI visualisation (lock-free queues, drained in OnIdle)
  IPeakSender<2> mMeterSender;
  ISender<1, 512> mLFOVisSender;   // one value per LFO per block, for 8 LFOs
  IBufferSender<1> mScopeSender;   // L == R, so one channel is enough
  DragDropWaveformDisplay* mDropBox = nullptr; // owned by IGraphics, nullptr while the editor is closed

  // MIDI note that triggers the sampler (notes 0-2 are noise, see Voice::ProcessSamplesAccumulating)
  static constexpr int kSamplerNote = 3;
  // velocity used by the drop box to audition the most recently dropped sample (0 = nothing to audition)
  int mPreviewVelocity = 0;

private:
  // Work for the background thread (see SampleJobs.h)
  using SampleJob = SampleJobs::Job;
  using SampleJobResult = SampleJobs::Result;
  SampleJobResult RunJob(const SampleJob& job);

  // --- the Samples page (all on the main thread) ---
  void SelectSlot(int slot);                 // select a slot in the model, show its preview
  void RefreshDropBox();                     // waveform in the drop box + the velocity the audition plays
  void SetSamplesPageOpen(bool open);
  void SendAudition(bool down);
  void ClearSelectedSlot();
  void RevertSelectedSlot();
  void SetEmbedSource(bool embed);
  void LocateSource(const std::string& path);
  void EnqueueRender(bool loadSourceOnly);
  void HandleJobResults();
  SamplesLayout::SlotInfo SlotInfoFor(int slot) const;

  SamplesModel mSamplesModel;
  ChainPresets::Store mPresets{std::filesystem::path()};
  SamplesPanel* mSamplesPanel = nullptr; // owned by IGraphics, nullptr while the editor is closed
  std::atomic<uint64_t> mSlotGeneration{0};

  // Makes a loaded state sound like it did when it was saved (see StateFormat.h): converts parameters that
  // changed meaning, and switches on the compatibility behaviours the old version had.
  void ApplyMigrations(const OneBitState::Data& data);
  // Sets a parameter to a value (used by migrations) and passes it on to the DSP.
  void ResetParam(int paramIdx, double value = 0.);
  // Shows the Hz or the tempo-sync LFO rate knob of an instrument type, depending on its sync switch.
  void UpdateLfoRateVisibility(int type);

#if ONEBIT_DIAGNOSTICS
  // What the host does, counted on the audio thread and logged once per second from OnIdle (see Debug.h)
  struct Diagnostics
  {
    std::atomic<uint64_t> blocks{0}, frames{0}, noteOn{0}, noteOff{0}, controllers{0}, otherMidi{0}, resets{0};
    std::atomic<int> lastFrames{0}, running{0};
    std::atomic<double> tempo{0.}, ppq{0.}, peak{0.};
    uint64_t lastLoggedBlocks = 0;
    uint64_t lastLogMs = 0;
  } mDiag;
  void LogDiagnostics();
#endif

  // Set when samples were replaced off the main thread (state load); OnIdle refreshes the drop box.
  std::atomic<bool> mSamplesChanged{false};

  // Declared last so it is destroyed (and its thread joined) before everything it uses.
  SampleImportWorker<SampleJob, SampleJobResult> mImportWorker{[this](const SampleJob& job) { return RunJob(job); }};
#endif
};
