# One set of DSP sources, two consumers, no JUCE in `dsp/`

There is no `graupel-core` library target. The same sources in `dsp/` are
compiled twice: once into the JUCE plugin, once into `render_wav`, a
WAV-in/WAV-out tool that drives the ported DSP with scripted parameter sweeps
and links no JUCE at all. A third target, `GraupelDspTests`, holds plain
assertions — grain scheduling, buffer arithmetic, the µ-law and resampling
paths — with no audio rendering and no framework.

We chose this over a separate static-library target because the boundary we
actually need is a link-line fact, not a target fact: if `render_wav` builds
without JUCE, then `dsp/` contains no JUCE. That is the Aulos and Quadrilo
pattern, already proven to work on Windows.

## Consequences

- Every fidelity claim in ADR 0004 and ADR 0005 is checkable: Authentic vs HQ is
  a render-diff question, and "the frozen audio survived the Mode switch" is a
  test rather than an anecdote. Without these targets, both claims would rest on
  one phone-sized memory of what the module did.
- `dsp/` may never `#include` a JUCE header, and the shim that replaces the
  STM32 codec, gates and CV scaler lives there — not in `src/`, which is the
  JUCE side (`PluginProcessor`, `PluginEditor`).
- CI runs the tests and pluginval on `windows-latest`; packaging drops
  `.vst3`, the Standalone exe, `README.md`, `LICENSE`, `NOTICE`, `licenses/` and
  `docs/` into a zip named from the tag, or `dev-<sha7>` when there is no tag.
