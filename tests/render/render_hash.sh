#!/bin/bash
# Builds and runs the render regression harness on Linux/macOS with clang.
# usage: tests/render/render_hash.sh <path-to-iPlug2>   (run from the repo root)
set -e
IP=${1:?path to iPlug2 checkout}
OUT=${2:-build-render/RenderHash}
mkdir -p "$(dirname "$OUT")"
# -include memory: iPlug2's VoiceAllocator.h uses std::unique_ptr without including <memory>
clang++ -std=c++20 -O1 -w -DIPLUG_DSP=1 -DAPP_API -include memory -I. -Isrc \
  -I"$IP/IPlug" -I"$IP/IPlug/Extras" -I"$IP/IPlug/Extras/Synth" -I"$IP/WDL" \
  tests/render/RenderHash.cpp "$IP/IPlug/Extras/Synth/MidiSynth.cpp" "$IP/IPlug/Extras/Synth/VoiceAllocator.cpp" -o "$OUT"
"$OUT" 48000
"$OUT" 44100
"$OUT" 48000 legacy
"$OUT" 44100 legacy
