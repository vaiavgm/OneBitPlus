# OneBitPlus

A polyphonic **1-bit synth and sampler** plugin (VST3 first) built on **iPlug2**. Code lives in `src/`. iPlug2 is an external dependency: don't review or edit it, but feel free to read it to understand an API.

## Where to find what

| Need | Go to |
|---|---|
| What the plugin does (user view) | [README.md](README.md) · `manual/VaiaOneBitPlus manual.tex` |
| File map, signal flow, invariants, threading, state format | [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) |
| How to build, supported targets, scripts | [docs/BUILD.md](docs/BUILD.md) |
| Known bugs, gaps, performance issues, clean-up plan | [docs/ANALYSIS.md](docs/ANALYSIS.md) |

## Ground rules

- **Audio thread is sacred**: no allocation, locks, `rand()`, logging or file I/O in `ProcessBlock`, `Voice::*` or `SampleOscillator::Process`.
- **Param blocks**: the 4 instrument blocks in `EParams` must keep identical layouts. `EModulations` lanes must stay grouped per kind. See the [invariants](docs/ARCHITECTURE.md#invariants-you-must-keep).
- 1-bit sample data is packed **MSB first** everywhere.
- Primary toolchain is MSVC on Windows. Write portable C++ anyway (no new Win32-only APIs outside `Debug.h`).
- The repo has no tests. If you touch pure helpers (loader, resampler, quantizers, `BitCompression`), consider adding tests.
- When you fix an item from [docs/ANALYSIS.md](docs/ANALYSIS.md), strike it or remove it there.
