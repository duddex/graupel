# Authentic keeps the module's 32 kHz world; we own the resampling

The ported DSP lives in a 32 kHz domain — `codec.Init(master, 32000)`,
`meter.Init(32000)`, buffer at 32 kHz or 16 kHz — and upstream's only resampling
filters are `src_filter_1x_2_*`, a fixed 2× step. A DAW gives us 48 kHz and
44.1 kHz, neither of which is 2× anything the submodule provides. We decided the
Authentic path keeps the 32 kHz domain intact and that we write our own polyphase
host→32 kHz conversion (JUCE `SamplingRateConverter` or hand-rolled), with
44.1 kHz treated as a first-class case rather than an approximation.

## Considered options

- Reproduce the colouration (µ-law, 16 kHz buffer, band-limited windows) at host
  rate and skip the internal 32 kHz domain. Cheaper, and it drops one resampler —
  rejected, because ADR 0004 defines Authentic as the module's own signal path,
  and half a signal path is not that.
- Authentic only at 48 kHz, silently falling back to HQ at other rates. Rejected:
  a switch whose meaning depends on the host's sample rate produces bug reports
  nobody can reproduce.

## Consequences

- The resampler is ours to defend, and it is the first place the render harness
  (ADR 0008) must look for damage: an Authentic render at 44.1 kHz that smears the
  top end is a defect in this decision, not in the granular code.
- HQ is the path that runs at host rate, so the two paths differ in domain as
  well as precision. That is the price of "same window, cleaner glass" being true.
