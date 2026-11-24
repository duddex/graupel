# Changing Mode keeps the audio you froze

In the firmware, switching among the three non-spectral modes is a "benign"
change that keeps the buffers, but crossing into or out of Spectral reallocates
and resets them — the frozen texture is lost. On the module that is a button
gesture nobody stumbles on; in a plugin it is a dropdown one click away, and
losing your sound because you poked a selector reads as a defect.

We decided the Recording Buffer survives all four Mode changes: snapshot the
frozen audio, let the mode switch reallocate, restore it. The buffer's content is
the user's material; Mode is only what the effect does with it.

## Consequences

- Mode and Quality reallocations both need a snapshot path, so peak memory is
  roughly double the buffer during a switch.
- Where restoration is genuinely impossible for one transition, that is a bug to
  report against this ADR, not a behaviour to document.
