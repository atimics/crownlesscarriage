# Shared legacy schema boundary

The schema 98 work exposed two independent lists that needed the previous
schema added: version admission and the runtime upgrade. This draft gives
both an explicit `CC_SIM_NEWEST_LEGACY_SCHEMA` in one internal header. A
compile-time assertion requires that boundary to sit immediately before the
current schema. A future schema change therefore calls for a deliberate
compatibility review at one location.

The runtime upgrade now uses the inclusive range 38 through the shared
boundary for generator 25. The older upgrade paths remain below it. Historical
generator pairings remain explicit in the compatibility table.

Validation evidence:

- Release build with warnings treated as errors and all 135 headless tests pass.
- The changed version module compiles for WebAssembly.

- Compare all 3,193 schema/generator pairs: schemas 0–102 and generators 0–30.
  Every result matches parent `dda793c9`.
- Encode and decode 61 saved worlds, schemas 38–98, after seven days at seed
  42. Every source and loaded hash matches the parent. Each loads as schema 98.
- A trial current schema 99 with legacy boundary 97 fails compilation with
  the review message. Changing that boundary to 98 compiles successfully.

`comparison.txt.gz` retains every pairing decision and save hash. The decoded
hash appears in `comparison.json`. `probe.c` reproduces the comparison: compile
it against the parent and changed persistence/simulation libraries with
`-I src -lsqlite3 -lm`, then compare their output.

The future-schema check used temporary copies of `cc_sim.h` and
`cc_sim_versions_internal.h` in a `sim` directory. Compile `cc_sim_versions.c`
with that directory first in the include path and `-std=c17 -fsyntax-only`.
The captured compiler result is in `next-schema-guard.json`.
