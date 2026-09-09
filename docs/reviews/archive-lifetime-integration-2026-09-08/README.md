# Archive stack lifetime integration

This PR brings the #624 and #625 repairs into the full archive draft at #622, parent 55af587749e576d5711394addd28136af849acbb. The parent already contains the historical-cast save rule extracted in #623.

The combined simulation includes archive freight, border contracts, seat advice, occupations, funded recruitment, travel, training, named appointments, plain volumes, and the protected staffing-reserve experiment. Schema 86 adds inhabited-town delivery prosperity and linked courier/quest retirement. Earlier archive schemas retain their simulation behavior. Generator 25, SQLite schema 32, and the simulation structure remain unchanged.

## Direct tests

- The Hollowbarrow transition reconstructs day 29458 from ordinal 353. Twelve wheat arrive the next day. Schema 86 preserves population and prosperity at zero while retaining the cargo. Save replay and another week retain equal world hashes.
- The original dispatch checkpoint from ordinal 148 precedes the day-308336 courier retirement. Its ended quest keeps the exact courier ID with the combined repair. Earlier schema behavior reproduces the missing target. The corrected world passes journal restart.
- Schema 85 recruitment journal coverage verifies replay before upgrade to the combined schema.

The courier fixture comes from original dispatch d665eccedeb2f3795edd72dbe401ac2c5b471033. Its generator and source manifest are in the courier-record-lifetimes review on #625. Tests load a copy of the portable fixture.

## Paired long runs

`probe.c` and `run-study.py` compare parent 55af587 against this combined repair. Each arm runs the nine reported #607 seed/year cases and a 32-seed sweep through 1000 years. All annual hashes and failures are retained in `results.json`. This tests the lifetime changes with the full archive funding and freight policy. The run is still in progress.

Build the probe with each source tree's headers and simulation library, plus `-lm`. It accepts ordinal, years, and schema override (zero uses current). Seeds are `(ordinal * 0x9e3779b9) & 0xffffffff`. `legacy-parity.json` records a separate 32-seed, 100-year schema-85 comparison.

## Validation

Strict headless build and all 125 tests passed. Native build and all 15 focused archive, quest-cast, trade, and persistence checks passed. Static analysis passed with one reviewed baseline entry. Long-run and legacy results are being collected. These are local results.

The protected staffing policy retains its measured tradeoffs from #622. This integration checks structural correctness. The broader archive recovery and seat issues remain open.
