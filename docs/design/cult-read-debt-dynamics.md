# Cult of the Read Debt — dynamics improvement spec

Companion to `docs/design/project89-worldpack.md`. This is the first slice:
the doubt-mark epistemics made computable.

## Doctrine (recap)

A record that is not read is a debt; a record that is read is an heir.
Every cult record carries an explicit doubt mark and the mark is earned,
not asserted:

- `?` — unverified: too few independent retellings agree.
- `~` — drift: the retell chain once agreed but recent checks collapse.
- `(none)` — settled: saturated with independent, recent agreement.

## What this slice adds

`src/sim/cc_doubt.{c,h}` — five pure primitives, no simulation state, no
I/O, deterministic:

- `CcDoubtRunWeight` — exponential age decay (half-life in days).
- `CcDoubtSample` — independent-evidence saturation, (0..1].
- `CcDoubtScore` — verified weight fraction times saturation.
- `CcDoubtDetectDrift` — recent-window verification collapse against
  lifetime rate (a record whose recent retellings fail half as often as
  its lifetime rate is drifting).
- `CcDoubtMarkOf` / `CcDoubtGlyph` — the mark and its ledger glyph.

These are re-derivations of the confidence-kernel primitives
(project-89, MIT, 2026-07); verified against golden tests
(`tests/cult_doubt_tests.c`), not imported as a dependency.

## Doctrine mappings

- Reading a sparse account: the score supplements, never replaces, a
  check (suppress posture). Nothing in the API lets confidence replace
  verification.
- Acting on a claim: callers check the mark; `?` claims cannot advance
  cult promotion; `~` claims require re-verification before tribute.
- A member's death does not erase their record: verifications age, so
  the mark decays naturally, and the account can be re-verified by any
  living member from the ledger alone.

## Wiring plan (follow-up slices)

1. Cult records store `CcDoubtRun` histories next to their text.
2. Retell events append runs; disputes append failing runs.
3. Ledger display renders the glyph adjacent to the record, muted,
   never conflated with settled text (render-honesty rule).
4. The inquisition audit reads marks, not guesses.

## Non-goals

No core engine changes in this slice. The primitives are additive and
usable from any faction; the cult integrates them in the follow-up
wiring above.
