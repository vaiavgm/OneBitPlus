# Bug, Gap & Performance Analysis

Scope: everything under `src/`, `CompressionAnalyzer/`, `config.h` and project config, on `main` @ `2a93f6b`.
iPlug2 itself is not reviewed. I only looked into it (VoiceAllocator, MidiSynth, VST2 wrapper) to confirm how OneBitPlus code behaves.

Severity: 🔴 high (wrong output, crash/UB, data loss) · 🟠 medium (feature broken, or likely trouble) · 🟡 low (cleanup, polish, robustness).

---

## Summary: top 10 to fix first

| # | Sev | Issue | Where |
|---|-----|-------|-------|
| ~~B1~~ ✅ | 🔴 | `ImportSampleAndPrintBits` never returns a value, so the caller gets an undefined value (UB) | `VaiaOneBitPlus.cpp:956-980` |
| ~~B2~~ ✅ | 🔴 | User samples are wiped on the first `OnReset` and on every sample-rate change, including samples just restored from a saved state | `VaiaOneBitPlus.cpp:870-882`, `:1038-1045` |
| ~~B3~~ ✅ | 🔴 | Data race / use-after-free: the UI thread adds or clears samples while the audio thread is reading them | `SampleData.h:96-139`, `VaiaOneBitPlus_DSP.h:253-254` |
| ~~B4~~ ✅ | 🔴 | The sample rate is dropped at `BindSample`, so samples play at 44.1 kHz pitch no matter what rate they were rendered at | `VaiaOneBitPlus_DSP.h:254` |
| ~~B5~~ ✅ | 🔴 | A dangling pointer is kept from the `.c_str()` of a temporary string | `VaiaOneBitPlus.cpp:678` |
| ~~B6~~ ✅ | 🟠 | *Phase 1: mapping kept on purpose, README corrected, preview fixed.* Velocity→sample mapping is 1:1 (sample *n* only plays at velocity *n*). This contradicts the README, and the click-preview (vel 127) plays silence | `SampleData.h:115-139`, `VaiaOneBitPlus.cpp:683-694` |
| ~~B7~~ ✅ | 🟠 | The click-preview sends MIDI **out to the host** (`SendMidiMsg`) instead of to the plugin's own DSP | `VaiaOneBitPlus.cpp:683-694` |
| ~~B8~~ ✅ | 🟠 | Tempo is never passed to the DSP, so tempo-synced LFOs always run at 120 BPM | `VaiaOneBitPlus.cpp:865` |
| ~~B9~~ ✅ | 🟠 | Release stages are inaudible: the gate drops to 0 on note-off and the voice is muted at once | `VaiaOneBitPlus_DSP.h:366, 413-435` |
| ~~B10~~ ✅ | 🟠 | Output is unipolar (0 / +gain), which puts a large DC offset on the output and wastes half the headroom | `VaiaOneBitPlus_DSP.h:533-555` |

---

## 1. Bugs

### 1.1 Plugin / state (`VaiaOneBitPlus.cpp`, `VaiaOneBitPlus.h`)

- ~~🔴 **B1: missing return.** `int ImportSampleAndPrintBits(...)` falls off its end (`:980`). The drop callback then uses that undefined index (`:675-677`). Fix: `return sampleIdx;`. Also, `ImportSample` and `ImportSampleAndPrintBits` are ~90 % duplicated. Make one call the other.~~ ✅ fixed in Phase 1 (`ImportSample(..., printBits)`; `ImportSampleAndPrintBits` is now a thin wrapper)
- ~~🔴 **B2: samples wiped on reset.** `mLastLoadedSampleRate` starts at `0`, so the very first `OnReset()` always calls `ReloadSamples()`. That function calls `ClearDynamicSamples()` and clears `mSamplePaths`/`mSamplePipelines`, and then imports nothing (every import is commented out, `:1112-1148`). Hosts usually call `setState` before or around activation, so samples restored by `UnserializeState` are lost. Any later sample-rate change also throws away every user-dropped sample. Fix: keep the source PCM (or the file paths plus pipeline) and **re-render** on a rate change instead of clearing.~~ ✅ fixed in Phase 2: samples are kept, because each one stores its render rate. `ReloadSamples` is gone, and its pipeline recipes moved to `src/PipelinePresets.h`. Playback at the stored rate needs B4 (Phase 6 🎧).
- ~~🔴 **B5: dangling pointer.** `kMyString = const_cast<char*>(PrintBits(...).c_str());` (`:678`) stores a pointer into a temporary that is destroyed at the end of the statement. In addition, `char* kMyString = "Hello, World!";` (`VaiaOneBitPlus.h:214`) is ill-formed in C++11+ (it only compiles under MSVC permissive mode). The member is otherwise unused, so remove it.~~ ✅ fixed in Phase 1
- ~~🟠 **B7: preview MIDI goes the wrong way.** `SendMidiMsg()` from the UI lambda (`:683-694`) is the DSP→host MIDI-out path. It also isn't safe to call from the UI thread. Use `SendMidiMsgFromUI()` to reach `ProcessMidiMsg`.~~ ✅ fixed in Phase 1 (`SendMidiMsgFromUI`, plays the last dropped sample at its own velocity)
- ~~🟠 **B8: tempo not forwarded.** `mDSP.ProcessBlock(inputs, outputs, 2, nFrames, mTimeInfo.mPPQPos, mTimeInfo.mTransportIsRunning)` (`:865`) leaves out `mTimeInfo.mTempo`, so the default `120.` is always used.~~ ✅ fixed in Phase 6: the host tempo is passed on. Projects saved before this keep the old fixed 120 BPM through the "Legacy timing" switch (see below).
- 🟠 **VST2 never saves custom state.** *(Kept as-is for compatibility: turning on chunks changes how VST2 hosts store existing projects. VST3/AU are unaffected.)* `PLUG_DOES_STATE_CHUNKS 0` (`config.h:24`) means the VST2 wrapper doesn't set `effFlagsProgramChunks`, so `SerializeState` (samples, paths) is never called there. Set it to `1` if VST2 matters. VST3/AU call `SerializeState` regardless.
- ~~🟠 **`UnserializeState` trusts its input.** `dataSize`/`numSlots`/`pathCount` (`:817-850`) are used without bounds checks, and `chunk.Get*` failures (return value < 0) are ignored. A corrupt or truncated chunk can trigger huge allocations or read garbage. It also clears samples on whatever thread the host uses for state (see B3).~~ ✅ fixed in Phase 2: `src/StateFormat.h` reads with bounds and size limits, and a corrupt block is dropped as a whole
- ~~🟡 `PrintBits` output goes through `MY_PRINTF`, whose buffer is 512 bytes (`Debug.h`). The generated C array is **truncated** for any sample over ~80 bytes, which makes the "print bits" workflow unusable. It is also computed twice per drop (`:675` internally and again at `:678`). Write the array to a file, or loop `OutputDebugStringA` over chunks.~~ ✅ fixed in Phase 1 (`SampleTools::ToCppArray` + `DebugPrintLong`; bytes ≥ 0x80 are emitted as `int8_t(0x..)` so they compile without narrowing errors)
- ~~🟡 `mSamplePathsBuffer` is never sized, so `UpdateSampleListBuffer()` always returns early. The sample list UI is dead code (it is commented out at `:635-653`, `:736-741`, `:810-815`).~~ ✅ removed in Phase 2. Paths and pipelines now live in the sample slots.
- ~~🟡 The comment in `UpdateSampleListBuffer` (`:715`) documents the 1:1 velocity mapping (B6). The README says the opposite.~~ ✅ resolved in Phase 1: the 1:1 mapping is kept on purpose and the README now documents it
- ~~🟡 LFO-sync visibility is only updated in `SetAnimationEndActionFunction` (`:279-283` etc.). After loading a preset or state with `Sync = off`, the wrong rate knob (Hz vs tempo) stays visible.~~ ✅ fixed in Phase 3 (`OnParamChangeUI` + `UpdateLfoRateVisibility`)

### 1.2 DSP / voice (`VaiaOneBitPlus_DSP.h`)

- ~~🔴 **B3: thread-safety of samples.** `Voice::Trigger` (audio thread) calls `mSampleManager.GetSampleForVelocity()` and keeps a raw pointer into `mDynamicSlots[i].data`. Meanwhile `AddSample` (`push_back`, UI thread) and `ClearDynamicSamples` (UI/state thread) change that vector. This is a data race, and a voice can keep playing freed memory after `Clear`. Fix: publish an immutable sample-bank snapshot (e.g. `std::shared_ptr<const Bank>` swapped atomically, or a lock-free double buffer) and free old banks off the audio thread.~~ ✅ fixed in Phase 2: `SampleManager` publishes immutable banks through an atomic pointer. The audio thread reports the oldest bank generation still in use, and retired banks are freed off the audio thread. Covered by unit tests and checked with TSan.
- ~~🔴 **B4: sample rate lost.** `mSampleOsc.BindSample(sampleInfo.data, sampleInfo.size);` (`:254`) omits `sampleInfo.sampleRate`, so the 44 100 default is used. Samples are rendered at the host rate (`ImportSample` → `targetSampleRate`), so at 48 kHz they play about 8 % slow and flat.~~ ✅ fixed in Phase 6: samples play at their own rate. Projects saved before this keep the old 44.1 kHz-speed playback through the "Legacy timing" switch.
- ~~🟠 **B9: release is inaudible.** The VoiceAllocator sets `kVoiceControlGate` to 0 on note-off (iPlug2 `VoiceAllocator::StopVoice`). The voice computes `velocity` from the gate's `endValue` (`:366`), and every branch at `:415-434` requires `velocity > 0`. The voice therefore goes silent immediately on note-off, while `GetBusy()` keeps it allocated through the PWM and pitch release. The four PWM and four pitch *Release* parameters have no audible effect. Store the trigger velocity in `Trigger()` and gate on envelope state instead.~~ ✅ fixed in Phase 6: synth notes keep sounding while an envelope releases. Noise and sampler notes still stop at note-off (by design). Saved states from before get all 8 Release values reset to 0, which reproduces the old sound bit for bit.
- 🟠 **Gate is block-quantized.** Using `.endValue` for the gate and pitch means that inside a MidiSynth sub-block (default 32 samples) a note-off silences the whole sub-block, even the samples before the event offset.
- ~~🟠 **B10: DC offset.** Voices only ever write `>= 0` (noise is clamped to 0 / 1.1, oscillators write 1.0, the sampler returns 0/1). The output stage maps `>0 → +gain` and `≈0 → 0` (`:533-555`). The result is a 0/+g pulse train: a large DC component, and only half the bipolar range used. Map to ±gain while any voice is active, add a DC blocker, or both.~~ ✅ fixed in Phase 6: a 10 Hz one-pole DC blocker on the output. The audible part is unchanged (peak level is the same too, but the signal is now centred around 0). It also applies to legacy projects.
- 🟠 **Noise uses `rand()`.** `ProcessNoise1/2/3` (`:279-314`) call `rand()`, which is not real-time safe (global state, possibly locked by the CRT), is shared across voices and instances, and only gives 15 bits. A per-voice `Rand()` LCG already exists (`:463`) but is never used. **Decision: noise stays as it is** (see below). `rand()` is believed to be lock-free on MSVC (per-thread state), so this is left alone.
- 🟠 **Noise operator precedence.** `outputs[0][i] + (rand() % 255) > noiseThreshold ? -0.9 : 1.1` parses as `((acc + r) > thr) ? …`. The *accumulated output of other voices* (0 or 1.1) feeds into this voice's random threshold, so noise density changes with whatever else is playing. Almost certainly unintended. **Decision: kept on purpose.** Because the other voices feed the threshold, a noise note can overlap a note or be muted by one, which is a feature.
- 🟠 **Noise S&H resets each block.** `clock4 = i / 4` and `clock8 = i / 8` use the in-block index, and `lastClock*` are locals. Hold phases restart at every MidiSynth sub-block boundary. Keep a per-voice sample counter instead. **Decision: noise stays as it is.**
- 🟠 **Sampler ignores pitch and has no one-shot mode.** `sampleOsc->Process(440)` (`:321`) is hard-coded, so neither pitch-bend nor pitch envelopes reach the sampler. Combined with B9, a drum hit stops dead on note-off. The comment at `:320` (“returns 1.0 or -1.0”) is wrong: it returns 0/1. The `outputVal < 0.1f` guard (`:325`) is odd and makes sampler output depend on the other voices. **Decision: the sampler keeps stopping on release (no one-shot).** ✅ Following pitch is now an opt-in **Sampler Pitch** switch (off by default, so nothing changes for existing projects): with it on, pitch bend, the pitch envelope and the pitch LFO speed the sample up or down. The key itself does not (the sampler note is fixed).
- 🟠 **`pwmKeyTrack` is `bool`** (`:143`) but is assigned from a `double` param. That's fine as long as the param stays a bool, but it is inconsistent with `OscPreset::pwmKeyTrack` (`double`).
- ~~🟠 **Pitch Keytrack does nothing.** `pitchKeyTrackStrength` is stored (`:146, :241, :708`) but never read in `ProcessSamplesAccumulating`, so the four "Pitch Keytrack" knobs are dead.~~ ✅ implemented in Phase 6 as extra octaves per octave from A440 (`pitch * (1 + keytrack)` in the exponent; 0 = off). Saved states from before get Pitch Keytrack reset to 0, because it had no effect then.
- 🟡 `midiNote = pitch * 12.0 + 69.1` (`:368`): the `+0.1` is a fudge factor to make the `< 1/2/3/4` comparisons robust. Use the voice's integer key (`mKey`) instead. Phase 6: kept as is. The voice type is still picked from the pitch.
- 🟡 `GetBusy()` returns `false` for the noise/sampler modes, but `mMode` is always set to `kSynth*` in `Trigger`, so the check is dead. The mode is chosen per sample from `midiNote`, not from `mMode`. Decide the voice type once in `Trigger` (the noise/sampler branch can be chosen from `mKey`).
- ~~🟡 **Smoothing time ignores the sample rate.** `LogParamSmooth::SetSmoothTime()` is never called, so the gain and sustain smoothers always use iPlug2's default coefficients, which are right for 48 kHz only (8 % too slow at 44.1 kHz, twice as fast at 96 kHz).~~ ✅ fixed in Phase 6 (legacy projects keep the old coefficients through the "Legacy timing" switch).
- 🟡 Both envelopes are constructed with the name `"gain"` (`:182-183`), which makes debugging confusing.
- 🟡 `MY_PRINTF` in `Trigger()` (`:249`) runs on the audio thread. Whenever a debugger is attached it calls `snprintf` and `OutputDebugStringA` (a syscall) on every note-on. still open (compiled out unless a debugger is attached).

### 1.3 Oscillators

- 🟠 **`VaiaOscillator` PWM edge cases** (`VaiaOscillator.h:39-46`): `pwm == 1.0` exactly isn't wrapped, so the output is DC high. The `while` loops step by 1 per iteration: with key-tracking (`pwm * f/440`) at high notes and `PWM Mod` up to ±2 this can be dozens of iterations per sample per unison voice. Use `pwm - std::floor(pwm)`. PWM wrapping also produces audible jumps from wide to narrow. Decide whether that's a feature (fold or clamp otherwise). Phase 6: the loops are gone (closed-form wrap with identical results). A width of exactly 100 % stays constant-high, because that is what 100 % means.
- ~~🟡 `mPhase` is used unqualified inside a class template derived from a dependent base (`:34`). This only compiles with MSVC permissive. Use `this->mPhase`.~~ ✅ fixed in Phase 0
- 🟡 `int temp = squareLookup[pos % 2];` then `return temp;`: a needless `double → int → T` round-trip.
- 🟡 Output conventions differ: `VaiaOscillator` returns ±1, `SampleOscillator` returns 0/1.

### 1.4 Sample import (`SampleLoader.h`, `SampleTools.h`, `IAudioEffect.h`)

- 🟠 **WAVE_FORMAT_EXTENSIBLE (0xFFFE) is rejected** (`SampleLoader.h:195`). Many 24-bit and multichannel WAVs from DAWs use it, so the import fails silently. Read the sub-format GUID.
- 🟠 **More than 2 channels are not handled**: they are treated as mono, interleaved (`:194`). The result plays N× too long and is garbage.
- 🟠 **Truncated or streaming WAVs** (data chunk size larger than the file, or `0xFFFFFFFF`) hit the `break` at `:168` before `data` is found, so they are rejected. The clamp at `:200` is then unreachable for the `data` chunk.
- ~~🔴 **32-bit float WAV: full scale flips sign.** `f * 2147483647.0f` rounds to 2^31 in float, which overflows `int32` (UB, in practice `INT32_MIN`), so every full-scale positive peak was imported as full-scale negative.~~ ✅ found by the Phase 0 tests, fixed in Phase 1 (scale in double)
- 🟠 **No user feedback on failure.** Errors go to `std::cerr`, which is invisible in a plugin.
- ~~🟡 `SampleTools::Normalize` takes `std::abs(INT32_MIN)`, which is UB. `SampleTools::Saturate` converts `tanh(..)*2^31` to `int32`, which overflows. Both functions, and `SampleTools::LowPassFilter`, are **unused** duplicates of the `IAudioEffect` classes. Delete them.~~ ✅ removed in Phase 3
- 🟡 `BiquadFilterEffect` cascades identical Q = 0.707 sections (`IAudioEffect.h:303`). That is not a Butterworth response for order > 2 (it is −6 dB at fc for order 4, etc.). Use the per-stage Butterworth Q values.
- 🟡 `snarePipeline2` (HP 400 Hz → LP 100 Hz) is effectively a null filter. It is unused, but the presets in `ReloadSamples` are worth reviewing before re-enabling them.
- ~~🟡 Missing includes: `IAudioEffect.h` uses `std::clamp`/`std::pow`/`std::tanh` without `<algorithm>`/`<cmath>`. `TrellisQuantizer.h` uses `std::numeric_limits` without `<limits>`. There is a circular include `IAudioEffect.h ↔ TrellisQuantizer.h`, and `TrellisQuantizer.h` doesn't need `IAudioEffect.h`.~~ ✅ fixed in Phase 0
- ~~🟡 `RawQuantizer`/`DeltaSigmaQuantizer`: unused `quantized`/`error` locals, and `ToString()` is missing `override`.~~ ✅ fixed in Phase 0

### 1.5 Quantizer (`TrellisQuantizer.h`)

- 🟠 **The algorithm changes with sample length.** `SuggestTrellisConfig` with the 64 MiB default gives:
  - 257 states up to 10 880 samples (~0.23 s @ 48 k)
  - 129 states up to ~21 676 samples (~0.45 s)
  - 65 states up to ~43 018 samples (~0.9 s)
  - beyond that, *chunked* mode

  Chunked mode runs the **first-order** trellis, while the non-chunked path runs the **3rd-order psychoacoustic** trellis (`:498-506`). Any sample longer than ~0.9 s therefore gets a completely different noise shaper. The RAM budget is also meaningless for the psychoacoustic path, which always uses 32-sample chunks (`:350`), and the 24-byte node estimate is wrong for its ~40-byte `Node`.
- 🟠 **The psychoacoustic trellis isn't a true Viterbi.** States are binned on `current_error` only (`:417`), but the path cost depends on `(e1, e2, e3)`. Paths with different histories are merged arbitrarily. Combined with a 3rd-order NTF and inputs normalized and saturated to ±1, the modulator runs in overload. The exact errors are unbounded while the bin index clamps at ±3, so it can go unstable and produce limit cycles. Consider bounding the input (e.g. ×0.5-0.7) or using a stable NTF.
- ~~🟡 Three near-identical trellis implementations exist (`SinglePass`, `Chunked`, `ApplyTrellisQuantization`). Two of them are unused.~~ ✅ removed in Phase 3 (the two unused variants)

### 1.6 Compression analyzer (`BitCompression.h`, `CompressionAnalyzer/`)

- ~~🟠 `for (size_t i = 0; i <= totalBits - L; i += L)` (`BitCompression.h:109, :275`) underflows when `totalBits < L`, which loops out of bounds.~~ ✅ fixed in Phase 1
- ~~🟠 `k_bits = (K==4)?2:((K==8)?3:4)` (`:337`) is wrong for K = 32/64: indices ≥ 16 don't fit into 4 bits. The reported r32/r64 compression rates are therefore **too optimistic**, and the analyzer's graphs are misleading. Use `ceil(log2(K))`.~~ ✅ fixed in Phase 1 (`ceil(log2(K))`; analyzer graphs for K ≥ 32 will now show higher, correct rates)
- 🟡 There is no decoder, so the round trip is never verified. `MaskHash` is unused. The dictionary is padded with duplicate `0` entries.
- 🟡 The analyzer is Win32-only (`<windows.h>`, GDI). That's fine for a dev tool, but it is worth noting in its docs.

### 1.7 GUI controls

- 🟡 *(Phase 2: the waveform is now restored after loading state or reopening the editor.)* `DragDropWaveformDisplay::SetWaveformData`: when `totalBits < width`, `samplesPerPixel < 1` leaves empty pixel ranges with `min = 1, max = -1`, which draw inverted lines. The waveform is not restored after loading state, and multi-file drops (`OnDropMultiple`) are not supported.
- ~~🟡 `IScrollContainer` isn't attached anywhere (dead code). If it is revived, the child receives no mouse events and is never attached to the graphics context.~~ ✅ removed in Phase 3

---

## 2. Gaps (missing or half-implemented features)

| Area | Gap |
|---|---|
| Params | ~~`kInputDither` and `kInputProtect` are declared and shown in the UI, but the DSP ignores them.~~ ✅ Phase 6: removed from the UI and the DSP. The two parameter slots stay as hidden "(unused)" placeholders (not automatable), because iPlug2 saves parameters as a fixed-length run and removing them would break the saved state of every existing project. Dither, if wanted, belongs in the import pipeline (`DitherEffect` already exists), not in a global setting. A DC filter is an output stage and is added separately. |
| Params | Pitch Keytrack has no effect (see above). The four Release params are inaudible (B9). |
| Params | ✅ *Phase 3: names are now "T1 PWM Attack", "T1 PWM LFO Rate Hz" / "… Rate Sync", etc. Indices, ranges and defaults are unchanged.* Originally: host-visible names were identical across the 4 instruments ("Attack", "LFO Rate", …), and "LFO Rate" is even duplicated *within* one instrument (Hz vs tempo). Automation lanes are indistinguishable. Prefix the names with "T1 PWM Attack" etc. |
| Presets | `kNumPresets = 1`, and no factory presets or `MakePreset` calls exist. |
| Samples | No sample management: you can't remove, reorder or replace samples. Every drop appends (max 127). There's no list UI and no waveform after reload. |
| Samples | There is no velocity distribution mode matching the README ("equally distributed"). Velocity is mapped 1:1 to slots. |
| Samples | There is no pipeline/quantizer selection UI: it is hard-coded in the drop lambda (`:669-673`). |
| Samples | ~~Heavy import (Lanczos plus trellis, which can take seconds) runs **synchronously on the UI thread** and freezes the editor.~~ ✅ Phase 2: `SampleImportWorker` renders on a background thread, and `OnIdle` updates the drop box. |
| MIDI/MPE | `PLUG_DOES_MPE 1`, but timbre and pressure are ignored (`mTimbreBuffer` is written and never read). Program change is routed but unhandled. All MIDI is echoed to MIDI out, which can loop back in some hosts. |
| UI | ✅ *Phase 5: added a keyboard (C1-C7, with QWERTY input), pitch-bend and mod wheels, an output scope, a level meter, and PWM/pitch LFO displays per type. Unused tags were removed.* Originally: No keyboard, pitch-bend wheel, meter, LFO visualiser or scope, even though the senders and control tags exist (`mMeterSender`, `mLFOVisSender`, `mOscilloscopeVisSender`, `kCtrlTagBender`, …). `OnIdle()` is empty, and `OnMessage` handles a bender that doesn't exist. |
| UI | ~~The layout is hand-copied 4× (~370 lines). The three `IVStyle`s differ only in font sizes.~~ ✅ Phase 3: one loop over the types and a `MakeStyle` helper (the unused `LARGER_LABEL` was dropped). |
| Config | Template leftovers: `PLUG_URL_STR` (iplug2.github.io), `AAX_TYPE_IDS 'IPI1','IPI2'` (collide with every other iPlug2 template plugin), `AAX_PLUG_NAME_STR "OneBitPlus\nIPIS"`, placeholder `changelog.txt`/`known-issues.txt`. |
| Config | `PLUG_CHANNEL_IO "2-2"` on an instrument (`PLUG_TYPE 1`). It should be `"0-2"`: the inputs are never used, and hosts may treat it as an effect. |
| Portability | ✅ *Phase 0: `Debug.h` is guarded with `_WIN32`, `<windows.h>`/`<format>` are gone from `VaiaOscillator.h`, `this->`-qualified members are used, the enums moved to `src/Params.h` (which removes the circular include), unused narrowing `int8_t` arrays were removed, and the sources pass a clang syntax check. Remaining: the macOS/iOS project files still don't list the `src/` headers, and the Debug/Release C++ standard mismatch.* Originally: effectively **Windows/MSVC only**. `Debug.h` declares `__declspec(dllimport)` Win32 functions unconditionally. `VaiaOscillator.h` includes `<windows.h>` and `<format>`. Code relies on MSVC permissive mode (`char*` from a literal, unqualified dependent-base members, `EParams` used in `VaiaOneBitPlus_DSP.h` before it is declared because of the circular include with `VaiaOneBitPlus.h`). The macOS/iOS/web project files exist but almost certainly don't build, and the macOS Xcode project doesn't list most `src/` headers. VST3 Debug is `stdcpp17` while Release is `stdcpp20` (`<format>` needs C++20). |
| Tests | ~~No tests at all.~~ ✅ Phase 0 added `tests/` (loader, resampler, packing, quantizers, effects, velocity mapping, `BitCompression`). |
| Docs | ~~The README describes behaviour that doesn't match the code (sample velocity distribution).~~ ✅ Phase 1. The manual (`manual/*.tex`) is not checked against the parameters. |

---

## 3. Performance

Hot path: per sample × per voice (32) × per unison (up to 8).

| # | Sev | Issue | Where | Suggestion |
|---|---|---|---|---|
| ~~P1~~ ✅ P4 | 🟠 | `pow(2.0, offsetCents/1200.0)` per unison voice per sample: up to 32 × 8 × fs `pow` calls per second (~12 M/s @ 48 k) | `DSP.h:340` | Precompute the detune ratios when unison or detune change, or once per block |
| P2 | 🟠 *(open: it would change the sound slightly)* | `440 * pow(2.0, …)` per voice per sample | `DSP.h:396` | Only the LFO and envelope terms change per sample. Use `exp2` on the summed exponent, or update per sub-block / control rate (e.g. every 16-32 samples) |
| P3 | 🟠 *(PWM loops done in Phase 6, the rest open)* | `VaiaOscillator::Process` calls `SetFreqCPS` (a divide) every sample, and its PWM `while` loops are unbounded | `VaiaOscillator.h:29-46` | Pass the increment, and use `floor` for wrapping |
| P4 | 🟠 | `rand()` in noise voices (CRT lock/TLS, not RT-safe) | `DSP.h:281-308` | Use the per-voice LCG (`Rand()`) |
| ~~P5~~ ✅ P4 | 🟡 | `mInputs[kVoiceControlTimbre].Write(...)` per voice per block, never read | `DSP.h:370` | Remove |
| ~~P6~~ ✅ P4 | 🟡 | The param smoother processes all 17 modulation lanes, including the 8 LFO lanes that are immediately overwritten | `DSP.h:507` | Smooth only the gain and sustain lanes |
| ~~P7~~ ✅ P4 | 🟡 | `SetParam` → `ForEachVoice` + `dynamic_cast` per voice per param change. `OnReset` pushes all 108 params, i.e. 108 × 32 casts | `DSP.h:618-633` | Use `static_cast` (all voices are `Voice`), or keep a typed voice array |
| ~~P8~~ ✅ P3 | 🟡 | 8 LFO members plus 4 hand-written `ProcessBlock`/`SetSampleRate` calls | `DSP.h:507-527, 563-573` | Use `std::array<LFO<T>,4>` loops (also removes duplication) |
| ~~P9~~ ✅ P4 | 🟠 | Trellis: `std::vector<std::vector<Node>>` is allocated **per 32-sample chunk** (≈1 500 chunks × 33 vectors for 1 s @ 48 k) | `TrellisQuantizer.h:379` | Allocate one flat buffer and reuse it across chunks |
| ~~P10~~ ✅ P2 | 🟠 | The whole import runs on the UI thread: read whole file → int32 → Lanczos (2 `sin` per tap, ~7-14 taps) → double → effects → trellis | `VaiaOneBitPlus.cpp:662-680` | Use a worker thread with a progress indicator, and hand the result to the DSP via the bank swap from B3 |
| P11 | 🟡 | Redundant `int32 ↔ double` conversions (the resampler works in int32, the pipeline in double) | `SampleTools.h:21-128, 194-199` | Convert to double once, after loading |
| ~~P12~~ ✅ P2 | 🟡 | `ReloadSamples` builds 10 pipelines (with `mt19937` instances) that are never used | `VaiaOneBitPlus.cpp:1047-1134` | Delete them, or move them into a preset table |
| P13 | 🟡 | `GetDynamicSampleData()` returns the vector **by value** (it copies whole samples in `SerializeState` and the drop callback) | `SampleData.h:107` | Return a `const&` or a span |
| P14 | 🟡 | `BitCompression`: `std::find` over the dictionary per chunk, O(K) | `BitCompression.h:215, 352` | Use a map from key to index (it is only an offline tool) |

---

## 4. Agreed clean-up plan & status

Decisions (from review):
- DC: add a DC-blocking filter on the output.
- Velocity→sample mapping stays **1:1**, so adding samples never shifts existing velocity bands. The README gets fixed instead.
- The sampler **stops on release**.
- **Backwards compatibility** first: no ID changes, and old states are migrated when loaded.
- Wire up the meter, scope and LFO visualiser. They had no on-screen controls before.
- Changes that keep the sound the same come first. Sound-changing changes (🎧) are batched last, to be checked by listening.

| Phase | Content | Sound change? | Status |
|---|---|---|---|
| 0 | Safety net: unit tests, includes and portability, clang syntax check | no | ✅ done |
| 1 | Quick correctness fixes (B1, B5, B7, loader float overflow, BitCompression, PrintBits, README) | no (new imports of full-scale float WAVs are now correct) | ✅ done |
| 2 | Sample lifecycle: immutable sample bank (B3), keep samples on a rate change (B2), state v2 + migration hook (`dspVersion`, `MigrateParams`), validation, background import. VST2 chunks deliberately left off. | no (keeps samples that used to be lost) | ✅ done |
| 3 | Dedup: param/UI generation in loops, LFO arrays, dead-code removal, unique param *display* names (indices unchanged) | no | ✅ done |
| 4 | Performance with bit-identical output: detune ratios once per block, `static_cast`, smoothing only the 9 smoothed lanes, no unused timbre write, trellis grid reuse (~35 % faster import). Added the render regression harness (`tests/render/`), which shows **identical output from `main` through Phase 4**. | no (verified by render hash) | ✅ done |
| 5 | UI: meter, scope, 8 LFO displays, keyboard, pitch and mod wheel | no | ✅ done |
| 6 🎧 | Sound changes, one commit each: Dither/Input Protect removed (6.1), PWM wrap without loops (6.2), host tempo + sample rate + legacy switch + state v3 (6.3), smoothing follows the sample rate (6.4), audible release + migration (6.5), Pitch Keytrack + migration (6.6), DC blocker (6.7) | **yes** | ✅ done, waiting for the listening session |

## 5. Legacy projects and migration (Phase 6)

Saved states carry a sound-behaviour version (`dspVersion`, see `src/StateFormat.h`). Everything saved by the original code (and by clean-up builds before Phase 6) counts as version 1.

| What changed | Legacy project (loaded, version 1) | New instance |
|---|---|---|
| Release stages (B9) | the 8 Release values are reset to 0 on load, so the sound is identical | release works |
| Pitch Keytrack | the 4 values are reset to 0 on load (the knob did nothing before) | works |
| Tempo for synced LFOs (B8) | **"Legacy timing" switch on**: fixed 120 BPM | host tempo |
| Sample playback speed (B4) | **"Legacy timing" switch on**: as if every sample was 44.1 kHz | the sample's own rate |
| Smoothing time | **"Legacy timing" switch on**: the old (48 kHz) coefficients | follows the sample rate |
| Dither / Input Protection | removed; the two parameter slots stay so the saved parameter run keeps its length: one is a hidden placeholder, the other is now Sampler Pitch (below) | same |
| Sampler Pitch (new, `dspVersion` 3) | loads **off**: the slot used to be the retired Dither knob, so states before version 3 get it reset to 0 | off by default; it is an ordinary parameter, so it is saved with each project and later changes of the default can't affect saved projects |
| DC blocker | applies to all projects (the audible part of the signal is unchanged) | on |

The switch is saved with the project. A migrated project keeps sounding like before until the user turns the switch off, and what to do with an old project then is up to them. **Proof:** the render harness (`tests/render/`) shows that the new code in legacy mode with the DC blocker off reproduces the original code bit for bit (host tempo 140, transport stopping halfway, tempo-synced LFOs, sampler, unison, 48 and 44.1 kHz).

**Limits:**
- Hosts that restore the plugin from parameter values only (VST2 without state chunks, some preset systems) never hand over the state block, so those projects can't be recognised as legacy and get the new behaviour.
- The 120 BPM bug mostly showed while the transport was stopped. While it runs, synced LFOs follow the host position and were already right; only the interpolation inside each block was slightly off.

## 6. To discuss later

- **A. FX-chain pipeline builder.** A minimal GUI for picking the import effects and quantizer, with their parameters, instead of the hard-coded drop lambda. The chain would be stored per sample (it already is, as a string: `Slot::pipeline`). Needs: an effect factory from that string, and a small list/editor control. `src/PipelinePresets.h` has the recipes tried so far. If dither is wanted, it is a pipeline effect (`DitherEffect`) there.
- **B. Open sound items:** pitch is computed with `pow()` per sample (control-rate pitch would be faster but slightly different).
- **C. Hygiene kept for compatibility:** `PLUG_CHANNEL_IO "0-2"`, the template AAX IDs and URL, and VST2 state chunks all change how hosts treat existing projects.
