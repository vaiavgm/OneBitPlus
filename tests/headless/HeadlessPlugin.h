#pragma once
// A stand-in for an API wrapper (IPlugVST3 / IPlugAPP), so that the real plugin class can run without a host.
#include "IPlugAPIBase.h"
#include "IPlugProcessor.h"
#include "IPlugQueue.h"
#include <vector>

BEGIN_IPLUG_NAMESPACE
namespace igraphics { class IGraphics; }

struct InstanceInfo { void* pAppHost = nullptr; };

class HeadlessPlugin : public IPlugAPIBase, public IPlugProcessor
{
public:
  HeadlessPlugin(const InstanceInfo&, const Config& config)
    : IPlugAPIBase(config, kAPIAPP)
    , IPlugProcessor(config, kAPIAPP)
  {
    SetChannelConnections(ERoute::kInput, 0, MaxNChannels(ERoute::kInput), true);
    SetChannelConnections(ERoute::kOutput, 0, MaxNChannels(ERoute::kOutput), true);
    SetBlockSize(DEFAULT_BLOCK_SIZE);
  }
  void BeginInformHostOfParamChange(int) override {}
  void InformHostOfParamChange(int, double) override {}
  void EndInformHostOfParamChange(int) override {}
  void InformHostOfPresetChange() override {}
  bool EditorResize(int, int) override { return false; }
  void SendSysexMsgFromUI(const ISysEx&) override {}
  bool SendMidiMsg(const IMidiMsg& msg) override { mMidiOut.push_back(msg); return true; }
  bool SendSysEx(const ISysEx&) override { return true; }
  void SendMidiMsgFromUI(const IMidiMsg& msg) override { mFromEditor.push_back(msg); }
  igraphics::IGraphics* GetUI() { return nullptr; } // no editor
  using IPlugProcessor::SetSampleRate;
  using IPlugProcessor::SetBlockSize;
  using IPlugProcessor::SetTimeInfo;
  std::vector<IMidiMsg> mMidiOut, mFromEditor;
};

using HeadlessBase = HeadlessPlugin;
END_IPLUG_NAMESPACE
