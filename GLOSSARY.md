# Graupel

Graupel is a studio audio effect that fragments a recording of its own input into
overlapping grains and reassembles them into textures and soundscapes. The design
follows a well-known hardware granular module whose firmware the DSP derives from;
Graupel is not that product, and makes no claim on its maker's name.

## Language

**Graupel**:
The effect itself, and the name under which it is released. Never used as a
descriptor of lineage — the lineage belongs in an attribution note, not the name.

**Recording Buffer**:
The continuously maintained memory of incoming audio from which all grains are
drawn. It exists only because signal has passed through the effect at some point.
_Avoid_: sample, file, tape, source, clip

**Grain**:
A short segment extracted from the Recording Buffer and replayed at a chosen pitch,
loudness curve and point in time. Many grains overlap to form a texture.
_Avoid_: particle, slice, fragment

**Texture**:
The audible result of the overlapping grains, and the name of the control that
shapes the grains' loudness curve. Use "Texture" for the control only when the
hardware naming is being preserved; otherwise call it the Grain Envelope.
_Avoid_: cloud, swarm

**Mode**:
Which of the four ways of playing back the Recording Buffer is active. Exactly
four, chosen from one dropdown: Grain, Stretch, Loop, Spectral. Mode is not a
preset and not a Quality setting.
_Avoid_: program, patch, engine, effect type, playback mode

**Quality**:
How the effect's one fixed hold on the past is spent: better audio for less
history, or thinner, mono history for longer reach. Exactly four settings, named
by what they buy: `1 s stereo`, `2 s mono`, `4 s stereo`, `8 s mono`. Quality and
reach-into-the-past are one decision seen from two sides, never two controls.
_Avoid_: bit depth, resolution, sample rate, fidelity — and "Duration" as the name of a second control

**Mono Buffer**:
Recording one channel instead of two, which doubles how far back the effect can
listen. A Quality choice, not an input choice.
_Avoid_: mono input, summed input, "mono"

**Summed Input**:
Feeding the effect from a single mono source rather than two independent
channels. Nothing to do with how long the Recording Buffer reaches.
_Avoid_: mono buffer, downmix

**Fidelity**:
How faithfully the signal path reproduces the hardware's own dirt — Authentic —
or cleans it away — HQ. Changing Fidelity changes the glass, not the window: the
reach into the past stays as Quality set it.
_Avoid_: quality, mode, hi-res

**Meter**:
The level display, which keeps the hardware's dual meaning: it watches the input
normally, and the output while Frozen. Its top segment is also where Soft Clip
begins — the light and the clip point are one fact.
_Avoid_: VU, LED, clip indicator

**Soft Clip**:
The gentle compression applied to hot input on its way into the Recording Buffer.
It changes what is captured, not what is heard, and has no control of its own.
_Avoid_: saturation, distortion, limiter, drive

**Freeze**:
The state in which the Recording Buffer stops taking in new audio and grains are
drawn only from what is already held. Latching; the effect is granularising the
past instead of the present.
_Avoid_: hold, latch, stop, sustain

**Density**:
How many grains sound, and whether they arrive at an even rate or scattered at
random. At its centre setting no grains are produced at all.
_Avoid_: rate, frequency, overlap

**Position**:
Which part of the Recording Buffer grains are taken from — how far back in time the
effect is listening.
_Avoid_: read head, offset, playhead

**Size**:
How long each grain is.
_Avoid_: length, duration, window

**Dry/Wet**, **Stereo Spread**, **Feedback**, **Reverberation**:
The four Blending Parameters. In the hardware these shared one knob and a selector;
in Graupel each is a control of its own and no fifth "Blend" control exists.
_Avoid_: Blend, BLEND knob, mix

**Input Gain**:
Trim applied to the incoming signal before it enters the Recording Buffer, from
-18 dB to +6 dB. Distinct from Dry/Wet: it changes what is captured, not what is
heard.
_Avoid_: record level, drive

**Pitch**:
How grains are transposed as they are replayed: whole semitones, with a separate
fine control for drift between them. Graupel has no keyboard, so Pitch is a
property of the effect, not a note.
_Avoid_: tune, key, note, V/oct

**Preset**:
A named state of the effect. While Frozen, it also remembers the audio itself;
while not Frozen, it remembers only settings, because a rolling recording is
nobody's material yet.
_Avoid_: patch, bank, slot, snapshot

**Trigger**:
Not a Graupel concept. Grains are sown by Density alone; the hardware's TRIGGER
jack, and the micro-sample-player trick it enabled, belong to a modular system
and are deliberately absent.
_Avoid_: gate, voice, note, sow-on-demand

**CV**:
Not a Graupel concept. Control voltage, attenuverters and modulation slots belong to
the hardware; every control here is a plain, automatable parameter.
_Avoid_: modulation matrix, patchbay, virtual CV, jack
