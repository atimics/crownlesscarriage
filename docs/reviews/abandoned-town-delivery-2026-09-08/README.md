# Deliveries to abandoned towns

This PR addresses the abandoned-town group in #607, on top of #623. Main at c826d5a reproduces seed ordinal 353 in year 81 and ordinal 828 in year 73.

## Cause and rule

The day trace for ordinal 353 reaches day 29459. Twelve wheat arrive at abandoned Hollowbarrow. Delivery correctly adds the cargo to its stock, then incorrectly raises prosperity from zero to one. The town validation rule requires prosperity zero after abandonment.

Schema 75 keeps the delivered goods and shipment outcome while awarding prosperity only to inhabited towns. The same rule covers intermediate food unloading. Schemas through 74 retain their earlier simulation behavior. Generator 25, SQLite schema 32, and saved fields remain unchanged.

The transition regression reconstructs day 29458, before this exact arrival. Under schema 74, the next day raises prosperity to one. Under schema 75, the same twelve wheat arrive and prosperity remains zero. The valid world passes journal replay and another week of equal continued hashes.

## Runs

`results.json` retains all annual hashes and failures for 91 runs:

- Parent c826d5a: all nine reported cases plus 32 seeds through 100 years. The town failures reproduce at 353/81 and 828/73. Courier case 426/209 also reproduces. The 32-seed sweep passes.
- Repair: all nine reported cases plus the same 32-seed sweep. Both town regressions pass. The nine reported cases produce one remaining failure: 426/209, `Active courier has no matching situation.` The 32-seed sweep passes.
- Original dispatch d665eccedeb2f3795edd72dbe401ac2c5b471033: all nine reported cases. Its six recorded failures reproduce at 189/191, 197/69, 148/845, 372/884, 392/761, and 726/645. The first two are abandoned-town state; the other four are missing courier targets.

The repair arm uses the current main-based simulation, so it also differs from the original dispatch arm in archive behavior. Integration with the dispatch stack needs its own run. This PR keeps #607 open for courier lifetimes and broader integration.

## Reproduction

Build `probe.c` against each revision's simulation library and headers with `-lm`. The executable accepts ordinal, years, and a schema override (zero uses its current schema). `run-study.py` lists the exact cases and local binary paths. Ordinals map to world seeds as `(ordinal * 0x9e3779b9) & 0xffffffff`. `trace.c` reproduces the arrival day against the parent library. `legacy-parity.json` records the separate schema-74 comparison.

## Checks

Strict headless build and all 118 tests passed. Static analysis passed with one reviewed baseline entry. Native persistence/trade checks and the final legacy comparison are being collected. These are local results.
