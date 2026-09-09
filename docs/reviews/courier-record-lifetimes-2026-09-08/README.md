# Courier and quest record lifetimes

This PR addresses the linked-record failures in #607 on top of #624.

## Retirement rule

A waiting, travelling, or player-carried courier retains its matching quest record even if the offer has ended. A courier record stays available while any retained courier-delivery quest targets its ID. Once the links are released, the existing oldest-record selection reuses the slot. Both arrays keep their existing bounds. Schema 76 applies these rules; schemas through 75 retain their earlier behavior. Generator 25, SQLite schema 32, and saved fields remain unchanged.

## Direct regressions

- Ordinal 426, year 209: the parent loses a travelling courier's quest. The test reconstructs year 208 under schema 75 and confirms the missing link after one more year. Schema 76 preserves valid links through the same interval, save replay, and journal restart. The test also confirms that released courier slots are reused during that year.
- Ordinal 148 from original dispatch d665eccedeb2f3795edd72dbe401ac2c5b471033: day 308336 reuses a courier still named by an ended quest. The generated fixture captures day 308335. The earlier policy reproduces the missing target on its next day; the repair retains that exact courier ID and passes journal restart.

`tests/fixtures/shipped/schema-75-courier-before-retirement.ccsave` contains generated world data. `checkpoint.c` creates it against the original dispatch library. It advances 844 years, then finds the first daily missing target and exports the preceding valid world with the portable save encoder. `fixture.json` records its source revision and hash. Tests load a copy so the source remains intact.

## Seed study

`results.json` retains all annual hashes for 82 runs. Parent fbd1932 and the repair each run all nine #607 cases plus a 32-seed, 100-year sweep. The parent reproduces 426/209. The repair completes all 41 runs. Both wider sweeps complete all 32 seeds. The original dispatch's six failures were reproduced and retained in the preceding abandoned-town review.

This main-based arm has a different archive policy from the original dispatch arm. The day-308335 fixture provides a direct transition test from the original dispatch state. Integration with the full archive stack still needs its own complete run. #607 stays open for that broader acceptance work.

Build `probe.c` against each revision's simulation library and headers with `-lm`. It takes ordinal, years, and schema override (zero uses current). `run-study.py` lists all cases and binary paths. Ordinals map to world seeds as `(ordinal * 0x9e3779b9) & 0xffffffff`. `legacy-parity.json` records the separate schema-75 comparison.

## Validation

The strict headless build and all 118 tests passed. Code head dc1e514 also passed the native build and all four focused quest-cast, persistence, and trade checks. Static analysis passed with one reviewed baseline entry. All 3200 annual schema-75 hashes match the parent across 32 worlds of 100 years each. The generated fixture hash remains unchanged after testing. These are local results; remote CI is tracked on draft PR #625.
