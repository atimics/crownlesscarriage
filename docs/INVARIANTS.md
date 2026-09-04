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

The 3D world is also derived from that seed. Settlement anchors, curved
routes, site positions, and terrain heights must match for the same
simulation state. Adjacent chunks must share exact edge samples.

## World streaming

The client may keep no more than `CC_WORLD_STREAM_CAPACITY` terrain chunks
resident on the CPU. The renderer may keep no more than the same number of
chunk meshes on the GPU. A chunk outside the 5 by 5 focus ring is disposable;
the manifest and coordinate are enough to recreate it. Do not move generated
height arrays into authoritative save state.

Open-world carriage presentation must use the generated route pose. Its
position and heading cannot be maintained by a separate visual road path.
Every strategic route incident to a settlement must share that settlement's
authored gate connector and exterior junction. Route dressing begins at the
junction, outside the authored town footprint.
Every strategic route has three saved road-district sites, including one
road house. Side-road geometry is derived from each site's saved route,
position, side, and length. A side road appears only when its main-road point
is revealed. While a site is closed, its tree or rock barrier must remain in
place and its local map stays outside the playable world.
The wide view renders generated corridors along revealed routes; it must not
render a square world floor. Fog is derived from chart knowledge, current
location, and current journey progress.

Authored town play uses `CcLocalPlaceProfile` in legacy-local coordinates.
The open world is carriage-only. Entering an authored town must leave the
world coordinate space before local movement, collision, or session saving.
Road-book events may use authored close road scenes, but returning to travel
must restore the same generated route pose and simulation progress.

## Save compatibility

`CC_SIM_SCHEMA_VERSION` (33) and `CC_GENERATOR_VERSION` (25) version every
save file and command journal. The policy: **every schema version ever
shipped stays loadable**. The legacy table in `cc_sim.c` (`legacy_schema`,
with the per-version branches below it) is the implementation, and
`persistence_tests` is the guard. The exact shipped schema and generator
pairs, their source commits, and their real save fixtures live in
`tests/fixtures/shipped/README.md`. When you bump the schema:

1. Extend the legacy table and add the migration branch for the previous
   version.
2. Keep the assigned `CcCommandKind` and `CcEventKind` values stable. Command
   journals and causal events persist these numeric ids. The same rule applies
   to `CcCollectibleMapSlot` and the good aliases.
3. Add a persistence test that loads a save written by the prior version.

Journal replay uses the rules for the schema that wrote each record. Migration
runs after every stored hash passes. New periodic rules need a schema gate when
they can change a legacy replay result.

The client session has its own small version. Version 4 stores whether a
position is in the shared world or an old scene-local space, plus the selected
world route. Versions 1 through 3 remain readable and are converted at load
time. A restored world route must be incident to the saved settlement and
present in the regenerated manifest.

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

## Windows stack reserve

The deterministic simulation test and headless REPL keep several complete
simulation states in stack storage. Windows executables reserve less stack by
default than the simulation requires, so these two targets must reserve at
least 16 MiB at link time. `cc_large_sim_stack` is the CMake guardrail for
both MinGW and MSVC builds. A Windows stack overflow here is a build
configuration regression, not evidence of nondeterministic simulation logic.

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
