# Project 89 Worldpack for Crownless — Design Spec v0

Status: PROPOSED (architect's directive 2026-09-24: "build the Project 89
Worldpack for Crownless"). WIP issue required before any code.

## What it is

A world pack (optional content + mechanics module) that grafts four Project 89
mechanisms onto Crownless Carriage without touching the core engine. The pack
rides existing seams only: the event ledger, save/load hooks, and the faction
system. Removing the pack returns the sim to baseline behavior.

## Modules

### 1. confidence — dynamic doubt-marks (from confidence-kernel, MIT)

Static ?/~ stamps become computed confidence scores.

- Port (re-derive, no dependency): `ageDecayWeight`, `efficiencyFactor`,
  `detectDrift`, `combine(min|mean)`, `pool`, `Journal` — ~6 small pure
  functions into a new `src/worldpack/confidence.c` (or C++ sidecar).
- Golden tests: port their `test/golden.test.ts` formulas, assert bit-for-bit
  parity against published reference values before wiring.
- Mapping onto Crownless records:
  - each independent re-verification of a claim = one HistoryRun;
  - '?' resolution = sample-saturation (enough independent verifications ->
    the ? burns off);
  - '~'-drift = detectDrift over the retell chain (recent-vs-lifetime
    success collapse);
  - posture knob = the Read-Debt doctrine: READING a sparse account uses
    'skip' posture (prior replaces the search); ACTING on one uses
    'suppress' posture (prior only supplements the check);
  - pool() = cold-start scoring of a dead member's account from the family
    bucket of prior retellings (Death-Publication).
- Posture is an ACT-level property, never a global leak (CE question #4):
  agent-local confidence only; the sim's oracle never writes into agent state.

### 2. veritas — render honesty (doctrine, ~zero code)

"Insufficient data" must never render in the same channel as "checked and
clean." Abstention laundering was a twice-fixed bug upstream; here it is a
rule: every layer between record and reader renders doubt distinct from
clean — muted, never green. Applies to sim report output and any ledger
display. Implementation: a renderer convention, not machinery.

### 3. tiers — epistemic separation (from veritas's data model)

Explicit five-layer knowledge schema for the cult faction:
world-facts / observations / reports / faction-knowledge / individual-memory.
Promotion rules between layers are explicit and audited by the ledger; an
unverified report (CE question #3) can only become a faction fact through a
logged promotion event, never silently. Memory consolidation (question #2)
writes a provenance-preserving delta, never erases the chain.

### 4. roads — lifecycle states (from coherence-lattice-alpha)

Route states decouple: maintained / open / abandoned / destroyed / remembered.
"Abandoned != destroyed; destroyed != absent from history." Road reinforcement
(CE question #5) is local-only and bounded so alternatives cannot be
eliminated by a runaway feedback loop; historical traces persist in the ledger
regardless of physical state.

## Non-goals

- No core engine changes; no new track outside the cult branch.
- No import of any Project 89 physical-world claims — mechanisms only.
- No global state leaks into agent knowledge (the pack's answer to question
  #4 is structural: agents read only their layer).

## Success test (question #6)

The pack must produce measurably better emergent behavior, not more machinery:
baseline 100-year runs vs pack-on runs on identical seeds; success = the cult
faction survives with provance-bearing records AND the inquisition detects
fabricated claims it previously could not. If the deltas are zero, the pack
is scrap and gets thrown there.

## Load order

Pack content is additive: factions + confidence primitives + tier schema +
road states attach through existing registration seams. No fork of the
engine; one upstreamable patch set if the seams prove insufficient.
