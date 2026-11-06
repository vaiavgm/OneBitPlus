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
#include <atomic>
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

  // Synchronous import (load -> resample -> pipeline -> 1-bit) of a WAV file into the next free sample slot.
  // Thread-safe; returns the dynamic sample index or -1. The drop box uses the background worker instead.
  int ImportSample(const char* filePath, uint32_t targetSampleRate, ResampleAlgo resampleAlgo, const AudioPipeline& pipeline = CreateDefaultPipeline(), bool printBits = false);
  int ImportSampleAndPrintBits(const char* filePath, uint32_t targetSampleRate, ResampleAlgo resampleAlgo, const AudioPipeline& pipeline = CreateDefaultPipeline());
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
  struct ImportJob
  {
    std::string path;
    uint32_t sampleRate = 44100;
    ResampleAlgo algo = ResampleAlgo::Lanczos;
    AudioPipeline pipeline;
    bool printBits = false;
  };

  struct ImportResult
  {
    int index = -1; // dynamic sample index, -1 on failure
  };

  ImportResult RunImport(const ImportJob& job);
  // Makes a loaded state sound like it did when it was saved (see StateFormat.h): converts parameters that
  // changed meaning, and switches on the compatibility behaviours the old version had.
  void ApplyMigrations(const OneBitState::Data& data);
  // Shows the most recently imported sample in the drop box and makes it the one to audition.
  void ShowLatestSample();
  // Shows the Hz or the tempo-sync LFO rate knob of an instrument type, depending on its sync switch.
  void UpdateLfoRateVisibility(int type);

  // Set when samples were replaced off the main thread (state load); OnIdle refreshes the drop box.
  std::atomic<bool> mSamplesChanged{false};

  // Declared last so it is destroyed (and its thread joined) before everything it uses.
  SampleImportWorker<ImportJob, ImportResult> mImportWorker{[this](const ImportJob& job) { return RunImport(job); }};
#endif
};
