#!/bin/bash
# Builds the REAL plugin class (src/VaiaOneBitPlus.cpp) without a host and without the editor, and runs the scenarios in
# PluginSession.cpp. Linux / macOS with clang; needs an iPlug2 checkout.
#   usage (from the repo root): tests/headless/build_and_run.sh /path/to/iPlug2 [sessions]
set -e
IP=${1:?path to iPlug2 checkout}
SESSIONS=${2:-150}
R=$(pwd)
HERE=$R/tests/headless
OUT=$R/build-headless
mkdir -p "$OUT"

# A copy of iPlug2's plug-in header that uses our stand-in for the API wrapper (IPlugVST3 / IPlugAPP), and a minimal source header
python3 - "$IP" "$OUT" <<'PY'
import sys
ip, out = sys.argv[1], sys.argv[2]
s = open(ip + '/IPlug/IPlug_include_in_plug_hdr.h').read()
s = s.replace('#elif defined APP_API\n  #include "IPlugAPP.h"\n  #define PLUGIN_API_BASE IPlugAPP', '#elif defined APP_API\n  #include "HeadlessPlugin.h"\n  #define PLUGIN_API_BASE HeadlessPlugin')
assert 'HeadlessPlugin' in s, 'iPlug2 header changed: adjust tests/headless/build_and_run.sh'
s = s.replace('#elif defined OS_LINUX\n  //TODO:', '#elif defined OS_LINUX\n  #define EXPORT\n  #define BUNDLE_ID ""\n  #define APP_GROUP_ID ""')
open(out + '/IPlug_include_in_plug_hdr.h', 'w').write(s)
open(out + '/IPlug_include_in_plug_src.h', 'w').write('''BEGIN_IPLUG_NAMESPACE
static Config MakeConfig(int nParams, int nPresets)
{
  return Config(nParams, nPresets, PLUG_CHANNEL_IO, PLUG_NAME, PLUG_NAME, PLUG_MFR, PLUG_VERSION_HEX, PLUG_UNIQUE_ID, PLUG_MFR_ID, PLUG_LATENCY, PLUG_DOES_MIDI_IN, PLUG_DOES_MIDI_OUT, PLUG_DOES_MPE, PLUG_DOES_STATE_CHUNKS, PLUG_TYPE, PLUG_HAS_UI, PLUG_WIDTH, PLUG_HEIGHT, PLUG_HOST_RESIZE, PLUG_MIN_WIDTH, PLUG_MAX_WIDTH, PLUG_MIN_HEIGHT, PLUG_MAX_HEIGHT, BUNDLE_ID, APP_GROUP_ID);
}
END_IPLUG_NAMESPACE
''')
PY

INC="-I$OUT -I$HERE -I$HERE/stub -I$R -I$R/src -I$IP/IPlug -I$IP/IPlug/Extras -I$IP/IPlug/Extras/Synth -I$IP/IGraphics -I$IP/IGraphics/Controls -I$IP/IGraphics/Platforms -I$IP/IGraphics/Drawing -I$IP/WDL -I$IP/Dependencies/IGraphics/NanoVG/src -I$IP/Dependencies/IGraphics/NanoSVG/src -I$IP/Dependencies/IGraphics/STB"
DEFS="-D__FLOAT_WORD_ORDER__=__ORDER_LITTLE_ENDIAN__ -DIPLUG_DSP=1 -DIPLUG_EDITOR=0 -DAPP_API -DNO_IGRAPHICS -DFONT_DESCRIPTOR_TYPE=void* -DIGRAPHICS_NANOVG -DIGRAPHICS_GL2 $EXTRA_DEFS"
# The editor code is compiled but never run (no editor), so the IGraphics symbols it refers to stay unresolved on purpose.
clang++ -std=c++20 -O1 -g -w -fno-pie -ferror-limit=5 $DEFS -include memory -include "$HERE/shims.h" $INC \
  "$HERE/PluginSession.cpp" "$R/src/VaiaOneBitPlus.cpp" "$HERE/stubs.cpp" \
  "$IP/IPlug/IPlugAPIBase.cpp" "$IP/IPlug/IPlugProcessor.cpp" "$IP/IPlug/IPlugPluginBase.cpp" "$IP/IPlug/IPlugParameter.cpp" \
  "$IP/IPlug/Extras/Synth/MidiSynth.cpp" "$IP/IPlug/Extras/Synth/VoiceAllocator.cpp" \
  -no-pie -Wl,--unresolved-symbols=ignore-all -lpthread -o "$OUT/PluginSession"
"$OUT/PluginSession" "$SESSIONS" 2>&1 | grep -vE "IPLUG|Channel|bus|I/O|chans|^$"
