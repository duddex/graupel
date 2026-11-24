# Parameters move once per block, and that is the faithful choice

The hardware polled its knobs through an ADC with its own latency and firmware
1.1 deliberately delayed trigger processing by 1 ms so slow-settling CV sources
would behave. So block-rate parameter updates are not a plugin shortcut here —
they are what the module did. We accept JUCE's per-block automation and do not
smooth Position or Pitch internally toward sample-accurate sweeps.

## Consequences

- Fast automated Position sweeps will step where a sampler's would glide. That is
  a documented character, not a bug to fix; if it ever becomes a defect, it
  contradicts this document and should say so.
- Sample-accurate automation stays a known limit rather than a hidden one, and
  belongs to the same conversation as ADR 0002's rejection of CV: this is an
  effect on an audio path, not a modular instrument.
