# Architecture

OneBitPlus is a polyphonic 1-bit synth and sampler built on iPlug2. Every voice produces a pulse train. Voices are OR-combined, and the sum is output as `0` or `±gain`.

## File map (`src/`)

| File | Role |
|---|---|
| `config.h` | iPlug2 plugin metadata (name, IDs, channel I/O, UI size). |
| `VaiaOneBitPlus.h/.cpp` | The `Plugin` subclass: parameter definitions (`EParams`), GUI layout (`mLayoutFunc`), state (de)serialization, MIDI routing, sample import (drag & drop). |
| `VaiaOneBitPlus_DSP.h` | `OneBitPlusDSP<T>`: the voice class, the `MidiSynth` with 32 voices, 8 LFOs, param smoothers, the output stage, `SetParam` routing. |
| `VaiaOscillator.h` | Pulse/PWM oscillator (`IOscillator<T>`), ±1 output. |
| `SampleOscillator.h` | Plays packed 1-bit sample data (MSB first), 0/1 output. |
| `SampleData.h` | `SampleManager`: velocity-indexed sample slots (static prepared samples plus up to 127 dynamic ones). |
| `SampleLoader.h` | Minimal RIFF/WAV parser → mono `int32` PCM. |
| `SampleTools.h` | Resampling (nearest/linear/Lanczos-3), `ReduceToOneBit` (pipeline → bits → packed bytes). |
| `IAudioEffect.h` | Offline pre-processing effects (Gain, Trim, Normalize, LP/HP, Biquad, Saturate, Clip, SR-reduce, Dither) and `AudioPipeline`. |
| `IQuantizer.h`, `RawQuantizer.h`, `DeltaSigmaQuantizer.h`, `TrellisQuantizer.h` | Float → 1-bit quantizers. Trellis = Viterbi search (1st-order, or 3rd-order "psychoacoustic"). |
| `BitCompression.h` | Dictionary/Elias-gamma compression metrics for 1-bit streams (used by `CompressionAnalyzer/`). |
| `DragDropWaveformDisplay.h` | Drop target, waveform preview and click-to-audition control. |
| `IScrollContainer.h` | Scrollable text container (currently unused). |
| `Debug.h` | `MY_PRINTF` → `OutputDebugStringA` (Windows only). |

Other top-level folders: `CompressionAnalyzer/` (a Win32 console + GDI tool that graphs compression rates), `projects/` + `config/` (IDE projects per format), `installer/`, `manual/`, `resources/`, `scripts/` (iPlug2 template build/dist scripts), `ReaperProjects/` (test sessions).

## Signal flow

```
MIDI ─► VaiaOneBitPlus::ProcessMidiMsg ─► mDSP.mSynth (MidiSynth / VoiceAllocator, 32 voices)
                                         │
     per block: smoothers + 8 LFOs ──► mModulations[kNumModulations][blockSize]
                                         │ (passed to voices as "inputs")
                                         ▼
Voice::ProcessSamplesAccumulating, per sample:
   midiNote < 1,2,3 → noise 1/2/3 (rand, S&H ÷1/÷4/÷8)
   midiNote < 4     → sampler (SampleOscillator, sample chosen by velocity)
   else             → up to 8 unison VaiaOscillators, OR-ed: any high → 1.0
                                         │
OneBitPlusDSP::ProcessBlock output stage: |x|<eps → 0, x>0 → +gain, x<0 → −gain
```

## Instrument ("Type") selection

The velocity at `Trigger()` picks one of 4 instrument presets: `mOscId = clamp((vel-1)/32, 0, 3)`. The low velocity range uses Type I and the high range Type IV. The UI shows Type I at the bottom. The preset (`mOscPresets[mOscId]`) is copied into the voice at trigger time. Later param changes are applied live only to voices whose `mMode` matches.

## Invariants you must keep

- **`EParams` layout**: each of the 4 instruments has a contiguous block of exactly `kParamsPerOsc = kParamPwmAttack2 - kParamPwmAttack1` (26) params in the *same order*. `SetParam` maps any instrument param back to its Type-1 equivalent with `%`. Adding a param means adding it to all 4 blocks at the same relative offset.
- **`EModulations` ordering**: lanes are grouped per kind (all pitch-sustain, then all PWM-sustain, then pitch-LFO, then PWM-LFO), because voices index lanes as `kModXxx1 + mOscId`.
- Sample bytes are packed **MSB first** (bit 7 = first sample) in `ReduceToOneBit`, `SampleOscillator`, `DragDropWaveformDisplay` and `BitCompression`.
- Dynamic sample slot `i` ↔ MIDI velocity `kPreparedCount + 1 + i` (see `SampleManager::GetSampleForVelocity`).
- Notes 0-3 are special (noise ×3, sampler). Everything else is the synth.

## Threading (as-is)

- Audio thread: `ProcessBlock`, `ProcessMidiMsg`, `Voice::*`.
- UI thread: `mLayoutFunc`, the drop callback → `ImportSample*` → `SampleManager::AddSample`.
- Host-dependent: `OnParamChange` → `mDSP.SetParam`, `OnReset`, `Serialize/UnserializeState`.
- **There is no synchronization** between the UI-side sample bank mutation and the audio-side reads. See [ANALYSIS.md](ANALYSIS.md) B3.

## State format (`SerializeState`)

`[params][u32 version=1][u32 numSlots]{u32 sr, u32 size, bytes}×numSlots[u32 pathCount]{str path, str pipeline}×pathCount`
