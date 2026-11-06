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
