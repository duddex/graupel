# AGPL-3.0 for the whole project, MIT ported DSP credited in NOTICE

JUCE is dual-licensed AGPLv3 or a commercial EULA, and that EULA forbids doing
anything that would subject JUCE to a licence requiring source disclosure,
derivative works, or free redistribution. So an MIT-licensed JUCE plugin is not
a thing that exists: we take the AGPLv3 option, and the whole project is
AGPL-3.0. The ported Clouds DSP stays MIT upstream in the `eurorack/` submodule
and is credited in a root `NOTICE` — author, licence, the files it covers, and
the one-way compatibility statement — with its licence text in `licenses/`.

## Considered options

- MIT-licensed DSP layer plus an AGPLv3 plugin, two `LICENSE` files. Rejected:
  the "reusable MIT library" it promised has no build-time boundary to enforce
  it (see ADR 0008), so it would be a licence claim readers could not act on.
- GPL-3.0, as some of our other plugin repos say. Wrong: GPL-3.0 does not match
  JUCE's AGPLv3 option. Aulos and Ladderine are the correct precedent here,
  not Quadrilo or Tangier.
- The free Starter EULA with a closed product. Rejected: it forfeits the MIT
  lineage's spirit for money we are not saving.

## Consequences

- Binaries convey source. The package carries `README.md`, `NOTICE`, and the
  license files; the ADRs remain in the source repository for contributors.
- The README carries the trademark line: this is an unofficial project,
  "Mutable Instruments" is its owner's trademark, nothing is affiliated or
  endorsed, and the product is not named after their module.
