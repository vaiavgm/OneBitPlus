# Render regression harness

`RenderHash.cpp` renders a fixed patch and MIDI sequence through `OneBitPlusDSP` (with iPlug2's real `MidiSynth`/`VoiceAllocator`) and prints a hash of the output. The script uses notes, chords, mod wheel, pitch bend, noise notes 0-2, the sampler (note 3), unison and envelopes.

- **Refactors and performance work** must keep the hash identical.
- **Intended sound changes** change it: update the reference hashes in `RenderHash.cpp`, and listen.

```sh
tests/render/render_hash.sh /path/to/iPlug2
```

On Windows, add `RenderHash.cpp`, `MidiSynth.cpp` and `VoiceAllocator.cpp` to a console project with the same include paths.
