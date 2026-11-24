# Quality spends one fixed budget; Fidelity lifts the glass, not the window

The hardware's four Quality settings are one number wearing four hats: 32 kHz /
16-bit / stereo gives 1 s, 32 kHz / 16-bit / mono 2 s, 16 kHz / 8-bit µ-law stereo
4 s, mono 8 s. Every row is the same 128 KB. Quality is therefore not a knob
beside Duration — it *is* the decision about how far back the effect can listen.

We keep that budget intact in the Authentic path, and the HQ switch changes
fidelity only: same selected duration, native rate, no µ-law companding. We
rejected a separate duration/budget control, because two knobs that both decide
"how much past can I reach" is the sort of surface that gets users asking which
one is lying.

## Consequences

- Mono is a Quality dimension (double reach, one channel), which is a different
  thing from Summed Input — see the glossary; conflating them is the easiest
  mistake available in this design.
- The four rows are labelled by what they buy — `1 s stereo`, `2 s mono`,
  `4 s stereo`, `8 s mono` — so the control can never be read as a hi-fi dial
  sitting beside a duration dial, and the only thing a user can verify by ear is
  the thing the label claims.
- HQ costs memory the hardware never had (a few MB, trivially), so the constraint
  we inherited is a choice we are keeping on purpose. Anyone tempted to "fix" the
  128 KB is fixing the design, not a bug.
