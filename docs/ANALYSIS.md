# Bug, Gap & Performance Analysis

Scope: everything under `src/`, `CompressionAnalyzer/`, `config.h` and project config, on `main` @ `2a93f6b`.
iPlug2 itself is not reviewed. I only looked into it (VoiceAllocator, MidiSynth, VST2 wrapper) to confirm how OneBitPlus code behaves.

Severity: 🔴 high (wrong output, crash/UB, data loss) · 🟠 medium (feature broken, or likely trouble) · 🟡 low (cleanup, polish, robustness).

---

## Summary: top 10 to fix first

| # | Sev | Issue | Where |
|---|-----|-------|-------|
| B1 | 🔴 | `ImportSampleAndPrintBits` never returns a value, so the caller gets an undefined value (UB) | `VaiaOneBitPlus.cpp:956-980` |
| B2 | 🔴 | User samples are wiped on the first `OnReset` and on every sample-rate change, including samples just restored from a saved state | `VaiaOneBitPlus.cpp:870-882`, `:1038-1045` |
| B3 | 🔴 | Data race / use-after-free: the UI thread adds or clears samples while the audio thread is reading them | `SampleData.h:96-139`, `VaiaOneBitPlus_DSP.h:253-254` |
| B4 | 🔴 | The sample rate is dropped at `BindSample`, so samples play at 44.1 kHz pitch no matter what rate they were rendered at | `VaiaOneBitPlus_DSP.h:254` |
| B5 | 🔴 | A dangling pointer is kept from the `.c_str()` of a temporary string | `VaiaOneBitPlus.cpp:678` |
| B6 | 🟠 | Velocity→sample mapping is 1:1 (sample *n* only plays at velocity *n*). This contradicts the README, and the click-preview (vel 127) plays silence | `SampleData.h:115-139`, `VaiaOneBitPlus.cpp:683-694` |
| B7 | 🟠 | The click-preview sends MIDI **out to the host** (`SendMidiMsg`) instead of to the plugin's own DSP | `VaiaOneBitPlus.cpp:683-694` |
| B8 | 🟠 | Tempo is never passed to the DSP, so tempo-synced LFOs always run at 120 BPM | `VaiaOneBitPlus.cpp:865` |
| B9 | 🟠 | Release stages are inaudible: the gate drops to 0 on note-off and the voice is muted at once | `VaiaOneBitPlus_DSP.h:366, 413-435` |
| B10 | 🟠 | Output is unipolar (0 / +gain), which puts a large DC offset on the output and wastes half the headroom | `VaiaOneBitPlus_DSP.h:533-555` |

---

## 1. Bugs

### 1.1 Plugin / state (`VaiaOneBitPlus.cpp`, `VaiaOneBitPlus.h`)

- 🔴 **B1: missing return.** `int ImportSampleAndPrintBits(...)` falls off its end (`:980`). The drop callback then uses that undefined index (`:675-677`). Fix: `return sampleIdx;`. Also, `ImportSample` and `ImportSampleAndPrintBits` are ~90 % duplicated. Make one call the other.
- 🔴 **B2: samples wiped on reset.** `mLastLoadedSampleRate` starts at `0`, so the very first `OnReset()` always calls `ReloadSamples()`. That function calls `ClearDynamicSamples()` and clears `mSamplePaths`/`mSamplePipelines`, and then imports nothing (every import is commented out, `:1112-1148`). Hosts usually call `setState` before or around activation, so samples restored by `UnserializeState` are lost. Any later sample-rate change also throws away every user-dropped sample. Fix: keep the source PCM (or the file paths plus pipeline) and **re-render** on a rate change instead of clearing.
- 🔴 **B5: dangling pointer.** `kMyString = const_cast<char*>(PrintBits(...).c_str());` (`:678`) stores a pointer into a temporary that is destroyed at the end of the statement. In addition, `char* kMyString = "Hello, World!";` (`VaiaOneBitPlus.h:214`) is ill-formed in C++11+ (it only compiles under MSVC permissive mode). The member is otherwise unused, so remove it.
- 🟠 **B7: preview MIDI goes the wrong way.** `SendMidiMsg()` from the UI lambda (`:683-694`) is the DSP→host MIDI-out path. It also isn't safe to call from the UI thread. Use `SendMidiMsgFromUI()` to reach `ProcessMidiMsg`.
- 🟠 **B8: tempo not forwarded.** `mDSP.ProcessBlock(inputs, outputs, 2, nFrames, mTimeInfo.mPPQPos, mTimeInfo.mTransportIsRunning)` (`:865`) leaves out `mTimeInfo.mTempo`, so the default `120.` is always used.
- 🟠 **VST2 never saves custom state.** `PLUG_DOES_STATE_CHUNKS 0` (`config.h:24`) means the VST2 wrapper doesn't set `effFlagsProgramChunks`, so `SerializeState` (samples, paths) is never called there. Set it to `1` if VST2 matters. VST3/AU call `SerializeState` regardless.
- 🟠 **`UnserializeState` trusts its input.** `dataSize`/`numSlots`/`pathCount` (`:817-850`) are used without bounds checks, and `chunk.Get*` failures (return value < 0) are ignored. A corrupt or truncated chunk can trigger huge allocations or read garbage. It also clears samples on whatever thread the host uses for state (see B3).
- 🟡 `PrintBits` output goes through `MY_PRINTF`, whose buffer is 512 bytes (`Debug.h`). The generated C array is **truncated** for any sample over ~80 bytes, which makes the "print bits" workflow unusable. It is also computed twice per drop (`:675` internally and again at `:678`). Write the array to a file, or loop `OutputDebugStringA` over chunks.
- 🟡 `mSamplePathsBuffer` is never sized, so `UpdateSampleListBuffer()` always returns early. The sample list UI is dead code (it is commented out at `:635-653`, `:736-741`, `:810-815`).
- 🟡 The comment in `UpdateSampleListBuffer` (`:715`) documents the 1:1 velocity mapping (B6). The README says the opposite.
- 🟡 LFO-sync visibility is only updated in `SetAnimationEndActionFunction` (`:279-283` etc.). After loading a preset or state with `Sync = off`, the wrong rate knob (Hz vs tempo) stays visible.

### 1.2 DSP / voice (`VaiaOneBitPlus_DSP.h`)

- 🔴 **B3: thread-safety of samples.** `Voice::Trigger` (audio thread) calls `mSampleManager.GetSampleForVelocity()` and keeps a raw pointer into `mDynamicSlots[i].data`. Meanwhile `AddSample` (`push_back`, UI thread) and `ClearDynamicSamples` (UI/state thread) change that vector. This is a data race, and a voice can keep playing freed memory after `Clear`. Fix: publish an immutable sample-bank snapshot (e.g. `std::shared_ptr<const Bank>` swapped atomically, or a lock-free double buffer) and free old banks off the audio thread.
- 🔴 **B4: sample rate lost.** `mSampleOsc.BindSample(sampleInfo.data, sampleInfo.size);` (`:254`) omits `sampleInfo.sampleRate`, so the 44 100 default is used. Samples are rendered at the host rate (`ImportSample` → `targetSampleRate`), so at 48 kHz they play about 8 % slow and flat.
- 🟠 **B9: release is inaudible.** The VoiceAllocator sets `kVoiceControlGate` to 0 on note-off (iPlug2 `VoiceAllocator::StopVoice`). The voice computes `velocity` from the gate's `endValue` (`:366`), and every branch at `:415-434` requires `velocity > 0`. The voice therefore goes silent immediately on note-off, while `GetBusy()` keeps it allocated through the PWM and pitch release. The four PWM and four pitch *Release* parameters have no audible effect. Store the trigger velocity in `Trigger()` and gate on envelope state instead.
- 🟠 **Gate is block-quantized.** Using `.endValue` for the gate and pitch means that inside a MidiSynth sub-block (default 32 samples) a note-off silences the whole sub-block, even the samples before the event offset.
- 🟠 **B10: DC offset.** Voices only ever write `>= 0` (noise is clamped to 0 / 1.1, oscillators write 1.0, the sampler returns 0/1). The output stage maps `>0 → +gain` and `≈0 → 0` (`:533-555`). The result is a 0/+g pulse train: a large DC component, and only half the bipolar range used. Map to ±gain while any voice is active, add a DC blocker, or both.
- 🟠 **Noise uses `rand()`.** `ProcessNoise1/2/3` (`:279-314`) call `rand()`, which is not real-time safe (global state, possibly locked by the CRT), is shared across voices and instances, and only gives 15 bits. A per-voice `Rand()` LCG already exists (`:463`) but is never used.
- 🟠 **Noise operator precedence.** `outputs[0][i] + (rand() % 255) > noiseThreshold ? -0.9 : 1.1` parses as `((acc + r) > thr) ? …`. The *accumulated output of other voices* (0 or 1.1) feeds into this voice's random threshold, so noise density changes with whatever else is playing. Almost certainly unintended.
- 🟠 **Noise S&H resets each block.** `clock4 = i / 4` and `clock8 = i / 8` use the in-block index, and `lastClock*` are locals. Hold phases restart at every MidiSynth sub-block boundary. Keep a per-voice sample counter instead.
- 🟠 **Sampler ignores pitch and has no one-shot mode.** `sampleOsc->Process(440)` (`:321`) is hard-coded, so neither pitch-bend nor pitch envelopes reach the sampler. Combined with B9, a drum hit stops dead on note-off. The comment at `:320` (“returns 1.0 or -1.0”) is wrong: it returns 0/1. The `outputVal < 0.1f` guard (`:325`) is odd and makes sampler output depend on the other voices.
- 🟠 **`pwmKeyTrack` is `bool`** (`:143`) but is assigned from a `double` param. That's fine as long as the param stays a bool, but it is inconsistent with `OscPreset::pwmKeyTrack` (`double`).
- 🟠 **Pitch Keytrack does nothing.** `pitchKeyTrackStrength` is stored (`:146, :241, :708`) but never read in `ProcessSamplesAccumulating`, so the four "Pitch Keytrack" knobs are dead.
- 🟡 `midiNote = pitch * 12.0 + 69.1` (`:368`): the `+0.1` is a fudge factor to make the `< 1/2/3/4` comparisons robust. Use the voice's integer key (`mKey`) instead.
- 🟡 `GetBusy()` returns `false` for the noise/sampler modes, but `mMode` is always set to `kSynth*` in `Trigger`, so the check is dead. The mode is chosen per sample from `midiNote`, not from `mMode`. Decide the voice type once in `Trigger` (the noise/sampler branch can be chosen from `mKey`).
- 🟡 Both envelopes are constructed with the name `"gain"` (`:182-183`), which makes debugging confusing.
- 🟡 `MY_PRINTF` in `Trigger()` (`:249`) runs on the audio thread. Whenever a debugger is attached it calls `snprintf` and `OutputDebugStringA` (a syscall) on every note-on.

### 1.3 Oscillators

- 🟠 **`VaiaOscillator` PWM edge cases** (`VaiaOscillator.h:39-46`): `pwm == 1.0` exactly isn't wrapped, so the output is DC high. The `while` loops step by 1 per iteration: with key-tracking (`pwm * f/440`) at high notes and `PWM Mod` up to ±2 this can be dozens of iterations per sample per unison voice. Use `pwm - std::floor(pwm)`. PWM wrapping also produces audible jumps from wide to narrow. Decide whether that's a feature (fold or clamp otherwise).
- 🟡 `mPhase` is used unqualified inside a class template derived from a dependent base (`:34`). This only compiles with MSVC permissive. Use `this->mPhase`.
- 🟡 `int temp = squareLookup[pos % 2];` then `return temp;`: a needless `double → int → T` round-trip.
- 🟡 Output conventions differ: `VaiaOscillator` returns ±1, `SampleOscillator` returns 0/1.

### 1.4 Sample import (`SampleLoader.h`, `SampleTools.h`, `IAudioEffect.h`)

- 🟠 **WAVE_FORMAT_EXTENSIBLE (0xFFFE) is rejected** (`SampleLoader.h:195`). Many 24-bit and multichannel WAVs from DAWs use it, so the import fails silently. Read the sub-format GUID.
- 🟠 **More than 2 channels are not handled**: they are treated as mono, interleaved (`:194`). The result plays N× too long and is garbage.
- 🟠 **Truncated or streaming WAVs** (data chunk size larger than the file, or `0xFFFFFFFF`) hit the `break` at `:168` before `data` is found, so they are rejected. The clamp at `:200` is then unreachable for the `data` chunk.
- 🟠 **No user feedback on failure.** Errors go to `std::cerr`, which is invisible in a plugin.
- 🟡 `SampleTools::Normalize` takes `std::abs(INT32_MIN)`, which is UB. `SampleTools::Saturate` converts `tanh(..)*2^31` to `int32`, which overflows. Both functions, and `SampleTools::LowPassFilter`, are **unused** duplicates of the `IAudioEffect` classes. Delete them.
- 🟡 `BiquadFilterEffect` cascades identical Q = 0.707 sections (`IAudioEffect.h:303`). That is not a Butterworth response for order > 2 (it is −6 dB at fc for order 4, etc.). Use the per-stage Butterworth Q values.
- 🟡 `snarePipeline2` (HP 400 Hz → LP 100 Hz) is effectively a null filter. It is unused, but the presets in `ReloadSamples` are worth reviewing before re-enabling them.
- 🟡 Missing includes: `IAudioEffect.h` uses `std::clamp`/`std::pow`/`std::tanh` without `<algorithm>`/`<cmath>`. `TrellisQuantizer.h` uses `std::numeric_limits` without `<limits>`. There is a circular include `IAudioEffect.h ↔ TrellisQuantizer.h`, and `TrellisQuantizer.h` doesn't need `IAudioEffect.h`.
- 🟡 `RawQuantizer`/`DeltaSigmaQuantizer`: unused `quantized`/`error` locals, and `ToString()` is missing `override`.

### 1.5 Quantizer (`TrellisQuantizer.h`)

- 🟠 **The algorithm changes with sample length.** `SuggestTrellisConfig` with the 64 MiB default gives:
  - 257 states up to 10 880 samples (~0.23 s @ 48 k)
  - 129 states up to ~21 676 samples (~0.45 s)
  - 65 states up to ~43 018 samples (~0.9 s)
  - beyond that, *chunked* mode

  Chunked mode runs the **first-order** trellis, while the non-chunked path runs the **3rd-order psychoacoustic** trellis (`:498-506`). Any sample longer than ~0.9 s therefore gets a completely different noise shaper. The RAM budget is also meaningless for the psychoacoustic path, which always uses 32-sample chunks (`:350`), and the 24-byte node estimate is wrong for its ~40-byte `Node`.
- 🟠 **The psychoacoustic trellis isn't a true Viterbi.** States are binned on `current_error` only (`:417`), but the path cost depends on `(e1, e2, e3)`. Paths with different histories are merged arbitrarily. Combined with a 3rd-order NTF and inputs normalized and saturated to ±1, the modulator runs in overload. The exact errors are unbounded while the bin index clamps at ±3, so it can go unstable and produce limit cycles. Consider bounding the input (e.g. ×0.5-0.7) or using a stable NTF.
- 🟡 Three near-identical trellis implementations exist (`SinglePass`, `Chunked`, `ApplyTrellisQuantization`). Two of them are unused.

### 1.6 Compression analyzer (`BitCompression.h`, `CompressionAnalyzer/`)

- 🟠 `for (size_t i = 0; i <= totalBits - L; i += L)` (`BitCompression.h:109, :275`) underflows when `totalBits < L`, which loops out of bounds.
- 🟠 `k_bits = (K==4)?2:((K==8)?3:4)` (`:337`) is wrong for K = 32/64: indices ≥ 16 don't fit into 4 bits. The reported r32/r64 compression rates are therefore **too optimistic**, and the analyzer's graphs are misleading. Use `ceil(log2(K))`.
- 🟡 There is no decoder, so the round trip is never verified. `MaskHash` is unused. The dictionary is padded with duplicate `0` entries.
- 🟡 The analyzer is Win32-only (`<windows.h>`, GDI). That's fine for a dev tool, but it is worth noting in its docs.

### 1.7 GUI controls

- 🟡 `DragDropWaveformDisplay::SetWaveformData`: when `totalBits < width`, `samplesPerPixel < 1` leaves empty pixel ranges with `min = 1, max = -1`, which draw inverted lines. The waveform is not restored after loading state, and multi-file drops (`OnDropMultiple`) are not supported.
- 🟡 `IScrollContainer` isn't attached anywhere (dead code). If it is revived, the child receives no mouse events and is never attached to the graphics context.

---

## 2. Gaps (missing or half-implemented features)

| Area | Gap |
|---|---|
| Params | `kInputDither` and `kInputProtect` are declared and shown in the UI, but the DSP ignores them (`SetParam`: “not implemented”). |
| Params | Pitch Keytrack has no effect (see above). The four Release params are inaudible (B9). |
| Params | Host-visible names are identical across the 4 instruments ("Attack", "LFO Rate", …), and "LFO Rate" is even duplicated *within* one instrument (Hz vs tempo). Automation lanes are indistinguishable. Prefix the names with "T1 PWM Attack" etc. |
| Presets | `kNumPresets = 1`, and no factory presets or `MakePreset` calls exist. |
| Samples | No sample management: you can't remove, reorder or replace samples. Every drop appends (max 127). There's no list UI and no waveform after reload. |
| Samples | There is no velocity distribution mode matching the README ("equally distributed"). Velocity is mapped 1:1 to slots. |
| Samples | There is no pipeline/quantizer selection UI: it is hard-coded in the drop lambda (`:669-673`). |
| Samples | Heavy import (Lanczos plus trellis, which can take seconds) runs **synchronously on the UI thread** and freezes the editor. |
| MIDI/MPE | `PLUG_DOES_MPE 1`, but timbre and pressure are ignored (`mTimbreBuffer` is written and never read). Program change is routed but unhandled. All MIDI is echoed to MIDI out, which can loop back in some hosts. |
| UI | No keyboard, pitch-bend wheel, meter, LFO visualiser or scope, even though the senders and control tags exist (`mMeterSender`, `mLFOVisSender`, `mOscilloscopeVisSender`, `kCtrlTagBender`, …). `OnIdle()` is empty, and `OnMessage` handles a bender that doesn't exist. |
| UI | The layout is hand-copied 4× (~370 lines). The three `IVStyle`s differ only in font sizes. |
| Config | Template leftovers: `PLUG_URL_STR` (iplug2.github.io), `AAX_TYPE_IDS 'IPI1','IPI2'` (collide with every other iPlug2 template plugin), `AAX_PLUG_NAME_STR "OneBitPlus\nIPIS"`, placeholder `changelog.txt`/`known-issues.txt`. |
| Config | `PLUG_CHANNEL_IO "2-2"` on an instrument (`PLUG_TYPE 1`). It should be `"0-2"`: the inputs are never used, and hosts may treat it as an effect. |
| Portability | Effectively **Windows/MSVC only**. `Debug.h` declares `__declspec(dllimport)` Win32 functions unconditionally. `VaiaOscillator.h` includes `<windows.h>` and `<format>`. Code relies on MSVC permissive mode (`char*` from a literal, unqualified dependent-base members, `EParams` used in `VaiaOneBitPlus_DSP.h` before it is declared because of the circular include with `VaiaOneBitPlus.h`). The macOS/iOS/web project files exist but almost certainly don't build, and the macOS Xcode project doesn't list most `src/` headers. VST3 Debug is `stdcpp17` while Release is `stdcpp20` (`<format>` needs C++20). |
| Tests | No tests at all. The quantizers, loader, resampler, `BitCompression` and the velocity mapping are pure functions that are easy to unit-test. |
| Docs | The README describes behaviour that doesn't match the code (sample velocity distribution). The manual (`manual/*.tex`) is not checked against the parameters. |

---

## 3. Performance

Hot path: per sample × per voice (32) × per unison (up to 8).

| # | Sev | Issue | Where | Suggestion |
|---|---|---|---|---|
| P1 | 🟠 | `pow(2.0, offsetCents/1200.0)` per unison voice per sample: up to 32 × 8 × fs `pow` calls per second (~12 M/s @ 48 k) | `DSP.h:340` | Precompute the detune ratios when unison or detune change, or once per block |
| P2 | 🟠 | `440 * pow(2.0, …)` per voice per sample | `DSP.h:396` | Only the LFO and envelope terms change per sample. Use `exp2` on the summed exponent, or update per sub-block / control rate (e.g. every 16-32 samples) |
| P3 | 🟠 | `VaiaOscillator::Process` calls `SetFreqCPS` (a divide) every sample, and its PWM `while` loops are unbounded | `VaiaOscillator.h:29-46` | Pass the increment, and use `floor` for wrapping |
| P4 | 🟠 | `rand()` in noise voices (CRT lock/TLS, not RT-safe) | `DSP.h:281-308` | Use the per-voice LCG (`Rand()`) |
| P5 | 🟡 | `mInputs[kVoiceControlTimbre].Write(...)` per voice per block, never read | `DSP.h:370` | Remove |
| P6 | 🟡 | The param smoother processes all 17 modulation lanes, including the 8 LFO lanes that are immediately overwritten | `DSP.h:507` | Smooth only the gain and sustain lanes |
| P7 | 🟡 | `SetParam` → `ForEachVoice` + `dynamic_cast` per voice per param change. `OnReset` pushes all 108 params, i.e. 108 × 32 casts | `DSP.h:618-633` | Use `static_cast` (all voices are `Voice`), or keep a typed voice array |
| P8 | 🟡 | 8 LFO members plus 4 hand-written `ProcessBlock`/`SetSampleRate` calls | `DSP.h:507-527, 563-573` | Use `std::array<LFO<T>,4>` loops (also removes duplication) |
| P9 | 🟠 | Trellis: `std::vector<std::vector<Node>>` is allocated **per 32-sample chunk** (≈1 500 chunks × 33 vectors for 1 s @ 48 k) | `TrellisQuantizer.h:379` | Allocate one flat buffer and reuse it across chunks |
| P10 | 🟠 | The whole import runs on the UI thread: read whole file → int32 → Lanczos (2 `sin` per tap, ~7-14 taps) → double → effects → trellis | `VaiaOneBitPlus.cpp:662-680` | Use a worker thread with a progress indicator, and hand the result to the DSP via the bank swap from B3 |
| P11 | 🟡 | Redundant `int32 ↔ double` conversions (the resampler works in int32, the pipeline in double) | `SampleTools.h:21-128, 194-199` | Convert to double once, after loading |
| P12 | 🟡 | `ReloadSamples` builds 10 pipelines (with `mt19937` instances) that are never used | `VaiaOneBitPlus.cpp:1047-1134` | Delete them, or move them into a preset table |
| P13 | 🟡 | `GetDynamicSampleData()` returns the vector **by value** (it copies whole samples in `SerializeState` and the drop callback) | `SampleData.h:107` | Return a `const&` or a span |
| P14 | 🟡 | `BitCompression`: `std::find` over the dictionary per chunk, O(K) | `BitCompression.h:215, 352` | Use a map from key to index (it is only an offline tool) |

---

## 4. Suggested clean-up order

1. **Correctness quick wins** (each a few lines): B1, B4, B5, B7, B8, the missing `<limits>`/`<cmath>` includes, the `BitCompression` underflow and `k_bits`.
2. **Sample lifecycle**: an immutable sample bank with an atomic swap (B3), re-rendering instead of clearing on a rate change (B2), state chunks on, validation in `UnserializeState`, a background import worker.
3. **Voice semantics**: store the velocity in `Trigger`, decide the voice type from `mKey`, make release audible (B9), a bipolar output or a DC blocker (B10), RT-safe noise, sampler pitch and a one-shot option.
4. **Dedup and structure**: generate the 4 instrument param blocks and UI rows in loops (`for osc in 0..3`, `paramBase = kParamPwmAttack1 + osc*kParamsPerOsc`), use LFO arrays, delete the dead code (`SampleTools` helpers, the unused trellis variants, `IScrollContainer`, `kMyString`, the commented-out imports).
5. **Performance**: P1-P3 (control-rate pitch, cached detune ratios), P9.
6. **Portability and release hygiene**: guard `Debug.h` with `#ifdef _WIN32`, drop `<windows.h>`/`<format>` from `VaiaOscillator.h`, fix the template IDs and URLs, `PLUG_CHANNEL_IO "0-2"`, unique param names.
7. **Tests**: a small header-only test target for the loader, resampler, quantizers and velocity mapping.
