# Archive convoy recovery on current main

PR #681 combines the convoy completion and automatic decision drafts (#679 and #680) on main e27996ae. Main includes the event-ledger performance repair, larger cast, war parties, and dragons-slain counter. The new work uses schema 96 and keeps schema 95 replay.

Weekly decisions require sustained seat failure, a viable town, a living sponsor, local books, an available carriage, road capacity, crew wheat, and funds. Convoys draw supplies at intermediate stops. Books retain their original owner in transit and gain the new town as owner on viable final delivery. Completion changes the saved seat and clears the old scribe roster. Characters keep their locations and the abbot keeps the office. Ended orders refund unused resources. Named events record funding and completion.

## Validation

The first full run passed 133 of 135 checks. Both failures came from the Deep Wyrm campaign comparing its fixed identity hash under the newly upgraded schema. The repair verifies that identity under its recorded schema 95, then restores the loaded schema. All 17 affected headless checks passed afterward, including both failed checks. All 18 focused native checks passed. Static analysis passed with one reviewed baseline item. The changed simulation module compiled for WebAssembly with strict warnings.

The convoy fixtures cover automatic departure, stable healthy seats, failure timing, later roads, road and food waits, named events, conservation, sponsor death, road loss, refunds, final viability, journal restart, and an active schema 95 order upgrading through completion.

The study ran 32 worlds for 100 years in each of three arms: parent main at schema 95, the new binary at schema 95, and the new binary at schema 96. All 9,600 annual validity checks passed. All 3,200 legacy annual hashes matched the parent. These ordinary-world runs measure validity and parity. Constructed journey fixtures exercise active convoy behavior.

## Performance

Five alternating runs per binary after builds finished all passed `--quick --assert-budget`. Median simulation time was 38,805.3 ns/day for parent main and 38,588.0 for this branch, against the current 90,000 budget. `benchmarks.json` retains every output and exit code.

The initial samples during builds were 84,128.3 for parent main and 94,701.0 for this branch; the latter exceeded the budget. `initial-benchmarks.json` retains both. The older draft base measured 264,386.1 against its 50,000 budget. These local observations support carrying the merged performance repair forward. Remote CI remains separate evidence.

## Reproduce

Build Release in separate worktrees at the parent and implementation revisions. Compile `probe.c` against each worktree's `libcrownless_sim.a`, then run:

```sh
python3 docs/reviews/archive-convoy-recovery-current-2026-09-12/run-study.py /tmp/parent-probe /tmp/current-probe
out/build/headless/crownless_benchmark --quick --assert-budget
ctest --test-dir out/build/headless --output-on-failure -j4
```

`manifest.json` records revisions, compiler, schema, size, and binary hashes. `results.json` retains all runs, hashes, and exit codes.

Persistent rival institutions, carriage travel to the collection town, and rescue after permanent road failure remain follow-up work. Native window rendering remains a device check.
