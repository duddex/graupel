# Upstream firmware is a pinned, read-only submodule

Graupel's DSP derives from Émilie Gillet's Clouds firmware, which must stay
checkably upstream so that fidelity claims and future updates mean something.
We pull `pichenettes/eurorack` in as a git submodule at `eurorack/`, pinned to
commit `f4cfa7697095a28d292bff758beb51deb49b5a86` (the repo has no tags), and we
never edit a file inside it: `git -C eurorack status` staying clean is a rule,
not a hope. Every adaptation — the codec/driver replacement, the parameter feed,
the buffer allocation — lives in our own sources.

## Considered options

- `git subtree --squash`: one clone, no submodule dance, but upstream history
  lands in our repo and "is this stock?" gets harder to answer at a glance.
- Our own fork as the sub-repo: enables patches, invites drift. Rejected.
- Sparse checkout limited to `clouds/` + `stmlib/`: rejected. It optimises a 55 MB
  download and adds a build-time cleverness to explain, while the whole tree
  being present is useful the moment a table or helper from another module is
  wanted. `eurorack-dsp-experiments` already ships the plain-submodule pattern.

## Consequences

- `git submodule update --init --recursive` is mandatory: `eurorack` itself has
  five nested submodules, and `stmlib` — which the Clouds DSP includes directly —
  is one of them.
- The MIT notices are not redistributed by us (the submodule is a pointer), but
  they are in every binary we build, so they must be carried in our notices file.
- "Mutable Instruments" is a registered trademark and its owner's guidance is not
  to keep the module name either; the product is Graupel, and the lineage appears
  only as attribution.
