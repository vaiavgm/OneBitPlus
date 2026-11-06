# Build & Run

## Prerequisites

- The repo must sit **inside an iPlug2 checkout**, three levels deep (e.g. `iPlug2/Examples/OneBitPlus`). `config/VaiaOneBitPlus-win.props` sets `IPLUG2_ROOT = $(ProjectDir)..\..\..`, and the code includes iPlug2 headers (`IPlug_include_in_plug_hdr.h`, `MidiSynth.h`, `LFO.h`, …) through those include paths.
- The iPlug2 dependencies (VST3 SDK, etc.) must be set up as described in the iPlug2 docs.

## Supported targets (in practice)

| Target | Status |
|---|---|
| Windows VST3 (`VaiaOneBitPlus.sln` → `VaiaOneBitPlus-vst3`) | Main and maintained target. Release builds with C++20. Debug is set to C++17, but `VaiaOscillator.h` includes `<format>`. |
| Windows APP / VST2 / AAX | Project files exist. VST2 doesn't save custom state (`PLUG_DOES_STATE_CHUNKS 0`). |
| macOS / iOS / Web | Template project files only. Not buildable as-is: the code uses Win32 APIs (`Debug.h`, `<windows.h>`) and relies on MSVC permissive mode. See [ANALYSIS.md › Gaps › Portability](ANALYSIS.md#2-gaps-missing-or-half-implemented-features). |
| `CompressionAnalyzer/` | Standalone Win32 console + GDI tool (`CompressionAnalyzer.vcxproj`). |

## Testing

There is no automated test suite. Manual testing uses the REAPER sessions in `ReaperProjects/`.

## Release scripts

`scripts/` contains the stock iPlug2 template scripts (`makedist-*`, `update_version.py`, `prepare_resources-*.py`). `installer/` holds the Inno Setup (`.iss`) and macOS Packages (`.pkgproj`) definitions. Their changelog and known-issues files are still placeholders.
