# The Recording Buffer is a recording of the input, never a loaded sample

Clouds "is the exact opposite of a sampler": it always samples what it receives,
except when Frozen. Graupel keeps that, and rejects the granular-plugin convention
of loading a file into the buffer. The buffer's only source is the signal that
passed through the effect.

## Consequences

- A freshly inserted Graupel has nothing to granularise, so it is silent-wet until
  signal arrives. The editor says so with an empty-buffer indicator rather than
  defaulting Dry/Wet to fully dry to hide it.
- Factory defaults on a fresh insert: Mode Grain, Quality `1 s stereo`, Fidelity
  Authentic, Density just past its centre — so grains arrive scattered rather
  than never — Pitch 0, Position, Size and Dry/Wet centred, Stereo Spread half,
  Feedback and Reverberation at zero, Summed Input off, Freeze off.
- Beyond that first insert, Graupel remembers nothing of its own. The module's
  "all settings are automatically saved" becomes the host's preset recall; we do
  not build a second memory that can disagree with it.
- Offline/freezing a render works because the input is rendered too; there is no
  second pass to wait for.
- Saved state carries the buffer only while Frozen (the hardware's own semantics:
  you save a frozen take, never a rolling one). The module's four flash memory
  slots are not reproduced — the host's preset system already is that feature, and
  a second one inside the plugin just gives users two preset systems that disagree.
