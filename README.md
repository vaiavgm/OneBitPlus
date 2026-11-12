# VaiaOneBitPlus
A polyphonic 1-bit synth, made with the iPlug2 framework

## Features
* Play as many notes at the same time as you dare (with a low pulse width, you can pick out quite a few notes in a chord)
* Choose one of (currently) 4 instrument settings, based on the velocity (high velo: top instrument, low velo: bottom instrument)
* Mod wheel and Pitch wheel support
* Automate LFOs and envelopes for pitch and PWM, and some extra controls to fine tune the effects.
* Up to 8 unison OSCs with detune knob

* 3 noise types on the bottom 3 MIDI notes
* -> less velocity means fewer noise "spikes", reducing perceived volume
* 1 bit sampler - crushes WAV into bits. Play the samples back on the 4th MIDI note from bottom.
 * Each imported sample gets its own velocity: the 1st sample plays at velocity 1, the 2nd at velocity 2, and so on (up to 127 samples). Velocities without a sample are silent. The mapping is fixed on purpose, so adding samples later never shifts the samples an existing track uses.
 * Drop a WAV onto the drop box to import it. Click and hold the drop box to audition the last imported sample.

## Good to know
* The output goes through a DC filter (a 10 Hz high-pass), so it is centred around zero. The **DC filter** switch under the drop box turns it off (for now, for comparing with a host's own DC filter). It is on by default and for every existing project.
* Release of the PWM and pitch envelopes lets a synth note keep sounding after you let go. Noise and sampler notes stop at once.
* Pitch Keytrack adds octaves per octave from A440 (0 = off).
* Tempo-synced LFOs follow the host tempo, and samples play at their own sample rate. Projects saved by older versions are loaded with the *Legacy timing* switch on (fixed 120 BPM, samples at 44.1 kHz speed, as before) and keep their old Release and Keytrack behaviour, so they sound the same. Switch it off to use the new timing.
* The old Dither and Input Protection knobs did nothing and are gone (their parameter slots are now Sampler Pitch and DC Filter).
* *Sampler pitch* (button under the drop box, off by default): when on, the sampler follows pitch bend, the pitch envelope and the pitch LFO. When off, samples always play at their recorded speed.

## Samples tab
Open the **Samples** button (top left) to manage the 127 sample slots. Slot *n* plays at velocity *n*.
* Pick a slot to see how it was made: a chain of effects (Gain, Trim, Normalize, Fade, DC shift, filters, Saturate, Clip, Sample-rate reduce, Dither) and a quantizer (Raw, Delta-sigma, Trellis). Change a value by dragging it (hold Shift for fine steps) or double-click to type. The sample is rendered again a moment after your last change, and the preview (waveform and spectrogram, before and after the 1-bit quantizer) updates. Use *Audition* to hear it.
* **Auto (least DC)** on a DC shift effect finds the offset that leaves the least DC in the 1-bit result. The plugin removes DC from its output anyway, so a sample can carry any offset, but a stream with a lot of DC has less room left for the signal.
* **Clear slot** frees a slot without moving the others. A dropped WAV fills the selected slot if it is free, otherwise the first free one.
* **Load preset / Save as preset** keep chains in `Vaiaphraim/OneBitPlus/Presets` in your user folder (plus a read-only shared folder). A sample keeps its own copy of its chain.
* The original file is read when you edit (use **Locate file...** if it moved). **Embed source** stores the source audio inside the project instead (about 88 KB per second), so it can be edited anywhere.
* Samples from older projects keep playing exactly as they were. Their chain is shown if it was recorded.
