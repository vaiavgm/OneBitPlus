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

### Render regression hash

`tests/render/` renders a fixed patch and MIDI script through the real DSP and iPlug2 synth engine, and prints a hash of the output. Use it to prove that a refactor doesn't change the sound. See [tests/render/README.md](../tests/render/README.md).

```sh
tests/render/render_hash.sh /path/to/iPlug2
```

### Headless test of the real plugin class

`tests/headless/` runs the real `VaiaOneBitPlus` class without a host or editor, driven like iPlug2's VST3 wrapper drives it (a REAPER-like session, controller messages, ghost voices after stop/start, random sessions). See [tests/headless/README.md](../tests/headless/README.md).

```sh
tests/headless/build_and_run.sh /path/to/iPlug2
```

### Diagnostics when something goes wrong in a host

`ONEBIT_DIAGNOSTICS` (`src/Debug.h`) is **1 during testing**: the plugin writes lines starting with `OneBitPlus:` to the Windows debug output (read them with DebugView from Sysinternals, with "Capture Win32" on). Set it to 0 for a release build; it costs nothing then.

- `OnReset ...`: every host stop/start or setup, with sample rate, maximum block size, the build time and the state version. Use it to check that you are running the build you think you are.
- `FIRST BLOCK since reset`: the first block the host processes, with the transport state, tempo and position as the host reports them. If it never appears, the host does not call the plugin.
- `TRANSPORT -> RUNNING / stopped`: every change of the transport state.
- `MIDI status=...`: the first 60 MIDI messages after each reset, one by one, with their type, data, offset and the transport state. This shows what the host sends at Play and Stop.
- Once per second (needs the editor open, it runs from the editor's idle timer): blocks processed, transport, tempo, position, MIDI counts by type, resets, peak level, busy voices, and the Legacy-timing and DC-filter switches.

Beyond that, audio is tested by listening, using the REAPER sessions in `ReaperProjects/`.

## Release scripts

`scripts/` contains the stock iPlug2 template scripts (`makedist-*`, `update_version.py`, `prepare_resources-*.py`). `installer/` holds the Inno Setup (`.iss`) and macOS Packages (`.pkgproj`) definitions. Their changelog and known-issues files are still placeholders.
