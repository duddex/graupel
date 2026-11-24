# Graupel is an effect: no CV, no MIDI, no transport

The hardware gets its TRIGGER and FREEZE from a digital input pin
(`cv_scaler.h` → `gate_input_.trigger()` / `.freeze()`) and its modulation from
±5 V CV. A plugin has neither, and the temptation is to invent substitutes. We
decided not to: Graupel is an audio effect in the reverb/delay sense — knobs only,
`IS_SYNTH FALSE`, `NEEDS_MIDI_INPUT FALSE`, and no reading of the host clock.
Grains are sown by Density alone; Freeze remains a latching parameter because it
is a state of the effect, not an event.

## Considered options

- MIDI notes sowing grains and setting pitch, as our own instrument ports do
  (Aulos converts CV/gate to MIDI because Aulos *is* an instrument). Rejected for
  an effect: it makes Graupel a device that consumes notes nobody played.
- A momentary Trigger parameter for automation. Rejected: one parameter of surface
  bought with a permanent "what does this do in a render?" question.
- Tempo-synced Density. Rejected: it makes Graupel partly a clocked device.

## Consequences

- The manual's "micro-sample player at 12 o'clock density with pulses on TRIG"
  use is out of reach, on purpose.
- BLEND's four functions become four real parameters (Dry/Wet, Stereo Spread,
  Feedback, Reverberation); no fifth Blend control exists, since a plugin has no
  reason to make one knob serve four masters.
