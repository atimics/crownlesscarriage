# Road outage observations

This change addresses the outage-duration work in #266. It builds on PR #576 at a83702a, with simulation schema 73, generator 25, and JSON protocol 6.

Each road gains an `outage` object with `run_interval_daily_samples` semantics:

- `current_closed_days`: consecutive closed daily samples ending at this checkpoint.
- `longest_closed_days`: the longest closed run observed during this invocation.
- `closed_inhabited_days`: closed daily samples where both endpoint populations were positive at that sample.

The existing `observation` object defines the start and end days. The runner samples after each daily advance. A road already closed at the start accumulates its observed duration from the first sample. Save loading begins a fresh interval. Sparse reports keep all daily observations between checkpoints. Counters live in runner memory; the saved simulation format stays the same.

## Evidence

Strict release headless build, all 108 tests, and static analysis passed. Static analysis has one reviewed baseline item. Controlled tests check reopening, the longest of two closure runs, abandonment during a closure, fresh observation windows, missing endpoints, and full simulation state preservation. JSON tests check save/load reset and exact equality between annual and sparse reports at matching checkpoints.

`parity.json` records two 40-year comparisons with the base. All existing JSON fields matched at 82 checkpoints, including the initial states. The comparison removes only the new `outage` object. `endpoints.json` preserves the final road measurements. Seeds are numeric 42 and 0x5eed0001; each run begins at day 1 and ends at day 14601.

The longest observed closure was 10,458 days for seed 42 and 8,050 days for seed 0x5eed0001. These measurements provide evidence for the open world-health design work. Repair attempts, exact historical closure causes, company access, network isolation, and health thresholds remain further work in #266.

## Reproduction

```sh
cmake -S . -B out/build/headless -DCC_BUILD_CLIENT=OFF -DCMAKE_BUILD_TYPE=Release -DCC_ENABLE_STRICT_WARNINGS=ON -DCC_WARNINGS_AS_ERRORS=ON
cmake --build out/build/headless -j4
ctest --test-dir out/build/headless --output-on-failure -j4
CC_CPPCHECK_JOBS=4 python3 tools/static_analysis.py
out/build/headless/crownless_sim_runner --seed 42 --years 40 --json
out/build/headless/crownless_sim_runner --seed 0x5eed0001 --years 40 --json
```
