# Saved notice board

Implements Part A of #664 on archive convoy recovery #681. Schema 97 stores twelve notice records beside the twelve situation slots. Each record keeps the situation, posting event, town, sponsor, day, and text. The board view reads the saved posting day. Situations remain the source for offer availability and instructions. Reusing a situation slot clears its prior notice.

A posting still enters the event ledger. Its saved board record survives ledger retention. Ordinary accounts keep the existing 32 gossip slots and carrier masks. The added saved state costs 2,216 bytes per `CcSim` (366,840 to 369,056).

## Older saves

Schemas through 96 replay with their existing notice behavior and hashes. A loaded old save retains that state until its first schema 97 gossip refresh. The refresh moves retained notices into their situation slots and clears their carrier versions, membership, and told bits. Active offers whose old posting has expired can receive a fresh notice. The saved `ready` flag preserves this transition across a save before the first refresh.

## Checks

- All 135 headless checks passed. The first run found two old-format test expectations and a local-server permission failure. The final run includes the repaired expectations and local-server access.
- A further full-pool test confirms a new posting preserves all 32 ordinary accounts and every carrier record. The focused gossip suite passed after that addition.
- Full native client build and all 22 focused native checks passed.
- Save tests cover board round trips, journal replay, old-schema migration, a save before migration, and malformed board rows and IDs. Schema 96 recruitment journal replay matches its recorded legacy hash.
- Static analysis passed with one reviewed baseline item. The changed simulation module compiled for WebAssembly with strict warnings.
- The study ran 32 worlds for 100 years in each of three arms: parent schema 96, changed binary schema 96, and changed binary schema 97. All 9,600 annual validity checks passed. All 3,200 legacy annual hashes matched.
- Three alternating benchmark runs per binary passed the 90,000 ns/day budget. Median simulation time was 37,818.3 ns/day for the parent and 37,275.2 for the change. These are local samples; remote checks provide separate evidence.

## Gossip measurements

These are paired daily observations for seeds 4 and 15 over 7,300 days each. Every observed day also passed world validation. The parent already includes the larger cast and character travel changes, so it supplies a fresh baseline for the older issue report.

| Measure | Parent, seed 4 | New, seed 4 | Parent, seed 15 | New, seed 15 |
|---|---:|---:|---:|---:|
| Notice share of all 32 slot-days | 28.63% | 0% | 22.00% | 0% |
| Mean completed account residence, days | 57.27 | 80.32 | 65.42 | 83.94 |
| Mean town-version retellings | 1.70 | 1.73 | 2.13 | 2.25 |
| Maximum town-version retellings | 10 | 10 | 10 | 10 |
| Town-version days below confidence 40 | 3.31% | 4.29% | 5.31% | 6.87% |
| Mean towns per account-day | 2.32 | 2.31 | 3.19 | 3.44 |

Account residence measures days observed in a slot before eviction; accounts still present at the end are censored from that mean. Town versions are sampled each day, so these are exposure-weighted values. Town reach is averaged across account-days. The probe measures stored town versions; speech output has separate regression coverage.

The change extends account residence by 40% and 28% in these two worlds. Both arms already reach the issue's deep-retelling and weak-confidence milestones. The new arm increases weak-confidence exposure. Part B's capacity decision can therefore use these results while retaining the current 32-slot budget.

## Reproduce

`manifest.json` records source revisions, sizes, compiler, and binary hashes. `results.json` retains annual hashes. `gossip-results.json` contains raw counters. `benchmarks.json` retains outputs and exit codes.

Build Release at each source revision. Compile each probe with that worktree's headers and simulation library:

```sh
cc -O2 -std=c17 -Isrc docs/reviews/saved-notice-board-2026-09-12/probe.c out/build/headless/libcrownless_sim.a -lm -o /tmp/current-probe
cc -O2 -std=c17 -Isrc docs/reviews/saved-notice-board-2026-09-12/gossip-probe.c out/build/headless/libcrownless_sim.a -lm -o /tmp/current-gossip
python3 docs/reviews/saved-notice-board-2026-09-12/run-study.py /tmp/parent-probe /tmp/current-probe
python3 docs/reviews/saved-notice-board-2026-09-12/run-gossip.py /tmp/parent-gossip /tmp/current-gossip
out/build/headless/crownless_benchmark --quick --assert-budget
```
