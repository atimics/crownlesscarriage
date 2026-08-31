# Invariants

Contracts that the code enforces but whose policy lives here, because no
constant or test name can say it on its own.

## Determinism

The world is seed-driven: `CcSimInit(sim, seed)` derives every population,
situation, and encounter from one 32-bit seed, and the client and every
headless tool accept `--seed`. When touching the sim, the rule is: the same
seed must produce the same world. Tests that pin authored content
(`causal_history_tests`, `world_spine_tests`, `scenario_tests`) are the
guardrail — if your change alters outcomes for a fixed seed, it is a
regression, not a refactor.

Time model: the world runs at 60 ticks/second with 60 subticks per game
minute; travel advances 30 game-minutes per real second, idling advances
0. These constants (`cc_sim.h`) are pacing choices, not tunables.

## Save compatibility

`CC_SIM_SCHEMA_VERSION` (20) and `CC_GENERATOR_VERSION` (19) version every
save file and command journal. The policy: **every schema version ever
shipped stays loadable**. The legacy table in `cc_sim.c` (`legacy_schema`,
with the per-version branches below it) is the implementation, and
`persistence_tests` is the guard. When you bump the schema:

1. Extend the legacy table and add the migration branch for the previous
   version.
2. Keep the `CcCommand` enum append-only — command journals persist the
   numeric values (same for `CcCollectibleMapSlot` and the good aliases).
3. Add a persistence test that loads a save written by the prior version.

## Performance budgets

`tools/benchmark.c` pins two budgets: 50,000 ns per simulated day, and
8,000 ns per locomotion step. They are asserted only when the benchmark
runs with `--assert-budget`, which the release CI job and the
`headless_performance_budget` CTest use. Debug/CI-quick runs use
`--quick` as a functional smoke test; sanitizer runs skip the budget
because instrumentation makes wall-clock budgets meaningless.

If a legitimate change breaks a budget: change the constant in the same
commit, with the reason in the commit message. Do not quietly delete the
assertion.

## Art contract

`tools/art/run_art_check.py` encodes the painterly look as thresholds
(palette discipline, no dominant single color, luminance spread, mood
scenes measurably softer than gameplay scenes). They are hand-tuned
regression catchers; adjust them deliberately, alongside screenshots, via
`make art-check`.

## Style

Strict warnings (`-Wpedantic -Wshadow -Wconversion`, …) are on in every
preset, and CI and presets build with warnings-as-errors. The C code is
kept comment-free except where a comment is load-bearing (schema
migration branches, budget constants, art thresholds, the raylib pin);
prefer naming, tests, and this docs directory over prose in code.
