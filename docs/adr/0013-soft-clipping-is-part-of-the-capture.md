# Soft clipping belongs to the capture, and is always on

The hardware's LEDs are an input VU, and "soft-clipping occurs when the last LED
is on" — so hot input is compressed on the way into the Recording Buffer, and a
loud source becomes different *material* rather than a dirtier copy of the same
material. We keep it, in Authentic and HQ both, tied to the same threshold the
meter uses so that the light and the clip point remain one fact.

## Considered options

- Clean headroom, no soft clip; let users drive their own saturator first.
  Rejected: it changes what gets captured, which is the one thing a user cannot
  undo later in the chain.
- Soft clip only in Authentic, bypassed in HQ. Rejected: Fidelity was defined as
  the glass, not the window (ADR 0004), and this would quietly add a second thing
  the switch changes.

## Consequences

- The meter's threshold is a sound-design parameter, not a UI detail. Changing it
  for "better" metering changes the texture people came for.
- There is no user-facing control for it. If one ever appears, it contradicts
  this document and should say so out loud.
