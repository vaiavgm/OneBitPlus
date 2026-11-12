# Render regression harness

`RenderHash.cpp` renders a fixed patch and MIDI sequence through `OneBitPlusDSP` (with iPlug2's real `MidiSynth`/`VoiceAllocator`) and prints a hash of the output. The script uses notes, chords, mod wheel, pitch bend, noise notes 0-2, the sampler (note 3), unison and envelopes.

- **Refactors and performance work** must keep the hash identical.
- **Intended sound changes** change it: update the reference hashes in `RenderHash.cpp`, and listen.
- `legacy` renders like a project saved by the original version. With `nodc` (DC blocker off) it must equal the **original** code's hash (see the header of `RenderHash.cpp`). That is the proof that migrated projects sound the same.
- `dump <file>` writes the raw output, to inspect or diff two builds.

```sh
tests/render/render_hash.sh /path/to/iPlug2
```

On Windows, add `RenderHash.cpp`, `MidiSynth.cpp` and `VoiceAllocator.cpp` to a console project with the same include paths.

## Fresh instance under hostile host values

`FreshInstance.cpp` builds every parameter with the plugin's own definitions (`src/ParamInit.h`), pushes them to the DSP like `OnReset`, plays a held note and checks the output is a lively 50 % square wave, with the transport stopped and running and with tempo 0 / negative / NaN / infinity / 1e30 and a position of NaN / infinity / 1e300 (iPlug2's VST3 wrapper doesn't check the host's validity flags). A watchdog reports a hung audio thread as a failure. `render_hash.sh` builds and runs it (it needs `IPlugParameter.cpp` as well) and exits non-zero on a failure.
