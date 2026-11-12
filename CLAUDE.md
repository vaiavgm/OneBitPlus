# OneBitPlus

A polyphonic **1-bit synth and sampler** plugin (VST3 first) built on **iPlug2**. Code lives in `src/`. iPlug2 is an external dependency: don't review or edit it, but feel free to read it to understand an API.

## Where to find what

| Need | Go to |
|---|---|
| What the plugin does (user view) | [README.md](README.md) · `manual/VaiaOneBitPlus manual.tex` |
| File map, signal flow, invariants, threading, state format | [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) |
| How to build, supported targets, scripts, unit tests | [docs/BUILD.md](docs/BUILD.md) · `tests/` |
| Known bugs, gaps, performance issues, clean-up plan | [docs/ANALYSIS.md](docs/ANALYSIS.md) |
| Commands on the user's Windows machine (local repo path `E:\iPlug2\Projects\VaiaOneBitPlus`, build, install, diagnostics). Add new ones there; always use full paths | [cheatsheet.md](cheatsheet.md) |

## Ground rules

- **Audio thread is sacred**: no allocation, locks, `rand()`, logging or file I/O in `ProcessBlock`, `Voice::*` or `SampleOscillator::Process`.
- **Param blocks**: the 4 instrument blocks in `EParams` must keep identical layouts. `EModulations` lanes must stay grouped per kind. See the [invariants](docs/ARCHITECTURE.md#invariants-you-must-keep).
- 1-bit sample data is packed **MSB first** everywhere.
- Primary toolchain is MSVC on Windows. Write portable C++ anyway (no new Win32-only APIs outside `Debug.h`).
- Unit tests for the pure helpers live in `tests/` ([how to run](docs/BUILD.md#testing)). Add or extend tests when you touch the loader, resampler, quantizers, effects or `BitCompression`.
- A change that shouldn't affect the sound must keep the render hash identical (`tests/render/`). If a change alters the sound on purpose, update the reference hashes and say so.
- **Existing projects must keep their sound.** If you change the sound on purpose, bump `OneBitState::kLatestDspVersion` (0 = original plugin, 1 = the whole clean-up), migrate old states in `VaiaOneBitPlus::ApplyMigrations` (or gate the old behaviour with a compat flag), and prove it with the `legacy` render (see [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md#state-format-serializestate)). Never remove or reorder parameters: add a new one only at the end of a block after checking how old states load.
- When you fix an item from [docs/ANALYSIS.md](docs/ANALYSIS.md), strike it or remove it there.
