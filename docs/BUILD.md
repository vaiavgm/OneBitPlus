# Build & Run

## Prerequisites

- The repo must sit **inside an iPlug2 checkout**, three levels deep (e.g. `iPlug2/Examples/OneBitPlus`). `config/VaiaOneBitPlus-win.props` sets `IPLUG2_ROOT = $(ProjectDir)..\..\..`, and the code includes iPlug2 headers (`IPlug_include_in_plug_hdr.h`, `MidiSynth.h`, `LFO.h`, …) through those include paths.
- The iPlug2 dependencies (VST3 SDK, etc.) must be set up as described in the iPlug2 docs.

## Supported targets (in practice)

| Target | Status |
|---|---|
| Windows VST3 (`VaiaOneBitPlus.sln` → `VaiaOneBitPlus-vst3`) | Main and maintained target. Debug x64 is set to C++17 and Release x64 to C++20. The code needs C++17. |
| Windows APP / VST2 / AAX | Project files exist. VST2 doesn't save custom state (`PLUG_DOES_STATE_CHUNKS 0`). |
| macOS / iOS / Web | Template project files only. The sources are now portable C++ (they pass a clang syntax check), but the Xcode projects don't list the `src/` headers yet, and nobody has built them. See [ANALYSIS.md › Gaps › Portability](ANALYSIS.md#2-gaps-missing-or-half-implemented-features). |
| `CompressionAnalyzer/` | Standalone Win32 console + GDI tool (`CompressionAnalyzer.vcxproj`). |

## Testing

`tests/` holds a standalone unit-test executable for the iPlug2-independent helpers: the WAV loader, resampler, bit packing, quantizers, effects, `SampleManager` and `BitCompression`. It has no dependencies and works with any C++17 compiler:

```sh
cmake -S tests -B build-tests && cmake --build build-tests && ./build-tests/OneBitPlusTests
# or, from the repo root:
g++ -std=c++17 -I. tests/TestMain.cpp -o OneBitPlusTests && ./OneBitPlusTests
```

Audio behaviour is still tested by hand, using the REAPER sessions in `ReaperProjects/`.

## Release scripts

`scripts/` contains the stock iPlug2 template scripts (`makedist-*`, `update_version.py`, `prepare_resources-*.py`). `installer/` holds the Inno Setup (`.iss`) and macOS Packages (`.pkgproj`) definitions. Their changelog and known-issues files are still placeholders.
