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
#include "src/IScrollContainer.h"
#include "src/DragDropWaveformDisplay.h"
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

  int ImportSample(const char* filePath, uint32_t targetSampleRate, ResampleAlgo resampleAlgo, const AudioPipeline& pipeline = CreateDefaultPipeline());
  int ImportSampleAndPrintBits(const char* filePath, uint32_t targetSampleRate, ResampleAlgo resampleAlgo, const AudioPipeline& pipeline = CreateDefaultPipeline());
  std::string PrintBits(const char* filePath, const std::vector<int8_t>& packed1Bit);

  void ReloadSamples(uint32_t targetSampleRate);
  OneBitPlusDSP<sample> mDSP{ 32 };
  IPeakSender<2> mMeterSender;
  ISender<1> mLFOVisSender;
  ISender<1> mOscilloscopeVisSender;
  DragDropWaveformDisplay* mDropBox = nullptr;

  char* kMyString = "Hello, World!";
  // Keep file paths of imported samples for GUI display and persistence
  std::vector<std::string> mSamplePaths;
  // Persist the pipeline string used to process each sample
  std::vector<std::string> mSamplePipelines;
  // Backing buffer for the sample paths display (fixed capacity to avoid pointer invalidation)
  std::vector<char> mSamplePathsBuffer;

  IScrollContainer* mSampleListContainer = nullptr;
  IMultiLineTextControl* mSampleListControl = nullptr;

private:
  uint32_t mLastLoadedSampleRate = 0;
  void UpdateSampleListBuffer();
#endif
};
