# Upstream lookup tables are consumed, not regenerated

`clouds/resources.cc` — 213 KB of generated window, pitch, filter and
sample-rate-conversion tables — is checked into the `eurorack/` submodule and is
compiled as it stands. We do not run `resources/resources.py`, `lookup_tables.py`
or `src_filters.py` in our build, so no Python toolchain sits between a clone and
a binary, and ADR 0001's read-only rule over the submodule stays intact.

If a table has to differ from upstream, the replacement is written as our own
data beside it in `dsp/`, never as an edit to the generated file — and the reason
it differs becomes a decision someone can argue with, rather than a diff nobody
can explain.
