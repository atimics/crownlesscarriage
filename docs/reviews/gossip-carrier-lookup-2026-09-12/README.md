# Resolve the gossip carrier once per exchange

The Linux Release timing gate on draft #684 measured 93,923.6 ns/day against a 90,000 budget. A single same-head repeat measured 92,565.2. The artifact-tooling tests passed in both runs. This prompted a profile of the existing simulation.

A three-second macOS sample of 300 two-year worlds placed `ExchangeGossip` first among application functions by top-of-stack samples (622). Source inspection found repeated linear character lookups inside its 32-story loop. The change resolves the carrier once after gathering gossip and obtaining its carrier record, then reuses that pointer for retelling and direct craft observations. Event sharing preserves the cast during this loop. Lookup still occurs after gathering, which can update story and carrier records.

## Base and behavior

The change is based on main e27996ae, schema 95, generator 25. The baseline build comes from #684 at 0fa86f0d; its simulation sources match main. The change uses the existing schema and state representation. `manifest.json` records compiler and binary hashes. The implementation commit is recorded in the PR history.

All 135 headless checks passed, including legacy save replay, gossip, character travel, and causal-history retention. Static analysis passed with one reviewed baseline item. The changed simulation module compiled for WebAssembly with strict warnings.

The study ran 32 worlds for 100 years in each arm. All 6,400 annual validity checks passed and all 3,200 paired annual hashes matched. The probe and runner retain every annual hash in `results.json`.

## Timing

Five alternating quick benchmark runs per binary all passed the 90,000 ns/day budget. Median simulation time was 41,695.5 ns/day for the parent and 37,711.5 for the change, a 9.6% reduction. All ten checksums matched. The raw outputs and exit codes are in `benchmarks.json`.

These are local macOS measurements. Remote Linux validation determines whether this repair also clears the failed gate on that runner. Both prior failures remain evidence.

## Reproduce

Build each revision in its own Release worktree. Compile `probe.c` against each worktree's headers and simulation library, then run:

```sh
python3 docs/reviews/gossip-carrier-lookup-2026-09-12/run-study.py /tmp/parent-probe /tmp/changed-probe
out/build/headless/crownless_benchmark --quick --assert-budget
ctest --test-dir out/build/headless --output-on-failure -j4
```

The native profile used `sample` for three seconds at one-millisecond intervals while running `crownless_benchmark --sim-seeds 300 --sim-years 2 --agents 1 --frames 1`. `profile-summary.txt` retains the leading entries.
