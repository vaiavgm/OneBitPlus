# Architecture

OneBitPlus is a polyphonic 1-bit synth and sampler built on iPlug2. Every voice produces a pulse train. Voices are OR-combined, and the sum is output as `0` or `±gain`.

## File map (`src/`)

| File | Role |
|---|---|
| `config.h` | iPlug2 plugin metadata (name, IDs, channel I/O, UI size). |
| `Params.h` | `EParams` and `EControlTags` enums, shared by the plugin and the DSP, plus `TypeParam(type, kParamXxx1)` and compile-time checks of the block layout. |
| `VaiaOneBitPlus.h/.cpp` | The `Plugin` subclass: parameter definitions (`EParams`), GUI layout (`mLayoutFunc`), state (de)serialization, MIDI routing, sample import (drag & drop). |
| `VaiaOneBitPlus_DSP.h` | `OneBitPlusDSP<T>`: the voice class, the `MidiSynth` with 32 voices, 8 LFOs, param smoothers, the output stage, `SetParam` routing. |
| `VaiaOscillator.h` | Pulse/PWM oscillator (`IOscillator<T>`), ±1 output. |
| `SampleOscillator.h` | Plays packed 1-bit sample data (MSB first), 0/1 output. |
| `SampleData.h` | `SampleManager`: velocity-indexed sample slots (static prepared samples plus up to 127 dynamic ones) in immutable, atomically swapped banks. |
| `StateFormat.h` | Versioned read/write of the custom state block (v0/v1/v2, `dspVersion`). |
| `SampleImportWorker.h` | Background thread for sample imports. |
| `PipelinePresets.h` | The factory import pipelines as `AudioPipeline` objects (kept to prove the factory chains in `PipelineModel.h` build the same thing). |
| `PipelineModel.h` | A **chain** (effects + one quantizer) as data: registry of stage types with parameter ranges, text format (also reads the old `AudioPipeline` text of existing projects), `Build()`, factory chains. |
| `SampleRender.h` | Source audio → chain → packed 1-bit data; `BitsDc`; `FindDcOffset` (bisection for the DC shift that leaves the least DC in the result). |
| `SampleJobs.h` | The background jobs: import a WAV, re-render a slot with an edited chain, auto-DC. Pure, so tested end to end. |
| `ChainPresets.h` | Chain presets: factory + `.obpchain` files in a user folder (+ read-only shared folders). |
| `Spectrogram.h` | FFT, log-frequency spectrogram, bit decoding, waveform envelope for the preview. |
| `SamplesModel.h` | Logic of the Samples page: selected slot, the chain being edited (draft), debounced re-render scheduling, preview data. No drawing. |
| `SamplesLayout.h` | Geometry and texts of the Samples page as a list of regions, plus hit-testing. No drawing. |
| `SamplesPanel.h` | The iPlug2 control that draws the layout and turns mouse input into model calls. |
| `SampleLoader.h` | Minimal RIFF/WAV parser → mono `int32` PCM. |
| `SampleTools.h` | Resampling (nearest/linear/Lanczos-3), `ReduceToOneBit` (pipeline → bits → packed bytes). |
| `IAudioEffect.h` | Offline pre-processing effects (Gain, Trim, Normalize, LP/HP, Biquad, Saturate, Clip, SR-reduce, Dither) and `AudioPipeline`. |
| `IQuantizer.h`, `RawQuantizer.h`, `DeltaSigmaQuantizer.h`, `TrellisQuantizer.h` | Float → 1-bit quantizers. Trellis = Viterbi search (1st-order, or 3rd-order "psychoacoustic"). |
| `BitCompression.h` | Dictionary/Elias-gamma compression metrics for 1-bit streams (used by `CompressionAnalyzer/`). |
| `DragDropWaveformDisplay.h` | Drop target, waveform preview and click-to-audition control. |
| `Debug.h` | `MY_PRINTF` → `OutputDebugStringA` (Windows only). |

Other top-level folders: `tests/` (standalone unit tests, see [BUILD.md](BUILD.md#testing)), `CompressionAnalyzer/` (a Win32 console + GDI tool that graphs compression rates), `projects/` + `config/` (IDE projects per format), `installer/`, `manual/`, `resources/`, `scripts/` (iPlug2 template build/dist scripts), `ReaperProjects/` (test sessions).

## Signal flow

```
MIDI ─► VaiaOneBitPlus::ProcessMidiMsg ─► mDSP.mSynth (MidiSynth / VoiceAllocator, 32 voices)
                                         │
     per block: smoothers + 8 LFOs ──► mModulations[kNumModulations][blockSize]
                                         │ (passed to voices as "inputs")
                                         ▼
Voice::ProcessSamplesAccumulating, per sample:
   midiNote < 1,2,3 → noise 1/2/3 (rand, S&H ÷1/÷4/÷8), stops at note-off
   midiNote < 4     → sampler (SampleOscillator, sample chosen by velocity), stops at note-off
   else             → up to 8 unison VaiaOscillators, OR-ed: any high → 1.0; keeps sounding
                      until both envelopes have finished their release
                                         │
OneBitPlusDSP::ProcessBlock output stage: |x|<eps → 0, x>0 → +gain, x<0 → −gain, then a 10 Hz DC filter
```

## Instrument ("Type") selection

The velocity at `Trigger()` picks one of 4 instrument presets: `mOscId = clamp((vel-1)/32, 0, 3)`. The low velocity range uses Type I and the high range Type IV. The UI shows Type I at the bottom. The preset (`mOscPresets[mOscId]`) is copied into the voice at trigger time. Later param changes are applied live only to voices whose `mMode` matches.

## Invariants you must keep

- **`EParams` layout**: each of the 4 instruments has a contiguous block of exactly `kParamsPerType` (26) params in the *same order*. `static_assert`s in `Params.h` enforce this. Parameter init, UI rows and `SetParam` all use `TypeParam(type, kParamXxx1)`. Adding a param means adding it to all 4 blocks at the same relative offset, then once in the init and UI loops. **Never reorder or insert params before existing ones**: hosts store params by index.
- **`EModulations` ordering**: lanes are grouped per kind (all pitch-sustain, then all PWM-sustain, then pitch-LFO, then PWM-LFO), because voices index lanes as `kModXxx1 + mOscId`.
- Sample bytes are packed **MSB first** (bit 7 = first sample) in `ReduceToOneBit`, `SampleOscillator`, `DragDropWaveformDisplay` and `BitCompression`.
- Dynamic sample slot `i` ↔ MIDI velocity `kPreparedCount + 1 + i` (see `SampleManager::GetSampleForVelocity`).
- Notes 0-3 are special (noise ×3, sampler). Everything else is the synth. Noise and sampler notes stop at note-off, synth notes release.
- Parameter slots are never removed or reordered: iPlug2 saves parameters as a fixed-length run, so removing one shifts the rest of the saved state of every old project (that's why `kParamUnused1/2` exist).

## Threading

- **Audio thread**: `ProcessBlock`, `ProcessMidiMsg`, `Voice::*`. It reads samples lock-free through `SampleManager::GetSampleForVelocity`, and calls `OneBitPlusDSP::ReportSampleBankUsage()` once per block.
- **UI/main thread**: `mLayoutFunc`, the drop callback (it only queues a job), and `OnIdle` (collects import results, refreshes the drop box, frees retired sample banks, and sends the meter, scope and LFO data queued by `ProcessBlock` to the controls).
- **Import worker thread** (`SampleImportWorker`): load → resample → pipeline → 1-bit → `SampleManager::AddSample`.
- **Host-dependent**: `OnParamChange` → `mDSP.SetParam`, `OnReset`, `Serialize/UnserializeState`. Sample edits from here are safe too.

### Sample banks

`SampleManager` never modifies a bank that is published. An edit copies the slot list (cheap: the slots are `shared_ptr`s), publishes the new bank with an atomic pointer store and a higher generation, and retires the old one. Each voice remembers the generation it bound in `Trigger()`. After every block, the audio thread stores the oldest generation still used by a busy voice. Retired banks older than that are freed by the next edit, or by `OnIdle`. The audio thread never allocates, frees or locks.

## State format (`SerializeState`)

`[params][custom block]`. The custom block is versioned, see `src/StateFormat.h`:

- **v0**: no block (params only).
- **v1**: `u32 1, u32 n, n×{u32 sr, u32 size, bytes}, u32 m, m×{str path, str pipeline}`. Read-only.
- **v2**: `u32 2, u32 dspVersion, u32 n, n×{u32 sr, u32 size, bytes, str path, str pipeline}`. Read-only.
- **v3**: `u32 3, u32 dspVersion, u32 compatFlags, u32 n, n×{…as v2}`. Read-only.
- **v4** (written now): like v3, and each slot also has `u32 embedded [u32 sourceRate, u32 count, count×int16]`. A slot with no data is a free slot and is **kept**, so later samples keep their velocity (v1-v3 never had free slots).

`dspVersion` is the sound-behaviour version the state was saved with (1 = original behaviour, 2 = Phase 6, 3 = Sampler Pitch). When it is older than 3, the Sampler Pitch parameter is reset to off. When it is older than 2, `VaiaOneBitPlus::ApplyMigrations` also resets the parameters that did nothing before (Release, Pitch Keytrack) and turns on `kCompatLegacyTiming`, which keeps the original tempo (fixed 120 BPM), sample-playback and smoothing behaviour. `compatFlags` are saved with the project, and the "Legacy timing" switch in the editor changes them. **When you change the sound on purpose again**, bump `kLatestDspVersion`, add a migration step, and add the render hash for it. See [ANALYSIS.md](ANALYSIS.md#5-legacy-projects-and-migration-phase-6).

## Samples page (the "Samples" tab)

A sample is rendered from its **source** (a WAV, mono, at its own rate) through a **chain** into packed 1-bit data. The 1-bit data is what plays and what is saved; it is only rendered again when the user edits the chain, which also means that existing projects can't change sound.

- **Slots** are fixed positions: slot *n* plays at velocity *n*. Clearing a slot leaves a free slot (`SampleManager::ClearSlot`), and the next import fills the first free one, so no sample ever changes velocity.
- **Source** of a slot: in memory (`Slot::source`, shared and immutable), or read from `Slot::path` when needed, or embedded in the project when the slot's *Embed source* switch is on (16 bit mono, about 88 KB per second). If neither is available the sample still plays but can't be edited, and *Locate file...* fixes that.
- **Editing** works on a draft chain in `SamplesModel`. After a short pause it renders on the worker thread (`SampleJobs::Run`) and swaps the result into the slot (a new sample bank), so the sample can be auditioned while editing. *Revert* puts the slot back as it was when it was selected. A render that finishes after the slot was cleared, reverted or switched is thrown away (`mSlotGeneration`).
- **Presets**: the factory chains, plus `.obpchain` files in `<AppData>/Vaiaphraim/OneBitPlus/Presets` (read and write) and `<ProgramData>/Vaiaphraim/OneBitPlus/Presets` (read only, for an installer). A sample keeps its own copy of the chain, so presets are only starting points.
- While the page is open the meter, scope and LFO displays under it are hidden, because they are redrawn every frame and would force the page to redraw with them.
