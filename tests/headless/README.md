# Headless test of the real plugin class

`PluginSession.cpp` runs `VaiaOneBitPlus` itself (real constructor and parameters, `OnReset`, `ProcessMidiMsg`, `ProcessBlock`, `OnIdle`) without a host and without the editor, driven the way iPlug2's VST3 wrapper drives it: time info first, then the MIDI events, then the audio, `OnReset` when the host stops processing. It exists because the DSP-only tests (`tests/render/`) can't see bugs in how the plugin and the host interact.

Scenarios:
- a REAPER-like session: armed track, transport stopped, keys, Play, no MIDI at all;
- the controller messages a host sends around transport changes (all notes off, sustain, reset controllers, ...), which must make no sound by themselves;
- **ghost voices**: after a host stop/start (`OnReset`) no voice may sound again without a note, with and without a release time set;
- random sessions: block sizes 1..8192, 44.1/48/96 kHz, transport toggles, tempo changes, stop/start, one key at a time. Oracle: sound while a key is held, silence once everything is released, never NaN.

```sh
tests/headless/build_and_run.sh /path/to/iPlug2 [sessions]      # from the repo root, needs clang; default 150 sessions
build-headless/PluginSession 800 677                             # replay random session 677 verbosely
```

How it works: `HeadlessPlugin.h` stands in for the API wrapper (`IPlugAPP` is about 30 lines), `build_and_run.sh` generates a copy of iPlug2's `IPlug_include_in_plug_hdr.h` that uses it, and the editor code is compiled but never run (its IGraphics symbols stay unresolved on purpose). `stubs.cpp` replaces the platform function `AppSupportPath`.

Known limits of the oracle: a chord, or a key pressed while the previous one is still in its release, ORs squares together into an almost constant level, which is nearly silent after the DC filter; a mod wheel can legitimately mute a note (PWM at the extremes). The random generator avoids both.
