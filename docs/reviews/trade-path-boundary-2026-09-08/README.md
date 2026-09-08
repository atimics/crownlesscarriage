# Shared trade path boundary

This advances #260 and the shared transport foundation for #248. Base: PR #586 at e6aa2c9. Simulation schema 73, generator 25.

The existing path finder and freight-capacity calculation now live in `cc_trade_path.c`, with an internal header for their callers. Royal route eligibility helpers live alongside the other shared route rules. Trade planning, site carriages, site freight plans, and grain supply use these functions.

The move preserves path costs, available capacity, permission modes, the schema-73 zero-condition gate, tie decisions, and caller outputs when a search fails. Archive caravan selection, payment, and travel remain delivery work in #248.

## Validation

- Strict release headless build and all 111 tests passed.
- Static analysis passed with one reviewed baseline item.
- `probe.c` compares 27,648 queries against the original private path finder. It covers two seeds, schema profiles 72 and 73, generated roads, mixed closed/zero-condition roads, and all-closed roads. Every settlement pair is queried with each royal owner or ordinary trade, border modes, relation modes, required slot counts 1 and 3, and empty or saturated route usage.
- The comparison checks success, first route, first stop, total cost, and available path capacity. Failed queries preserve every caller output. Each complete fixture preserves the entire simulation state. `query-parity.json` records the count and output digest.
- Two 40-year simulations produced 82 complete JSON checkpoints matching the base byte for byte, including hashes. See `simulation-parity.json`.

## Reproduction

```sh
cmake -S . -B out/build/headless -DCC_BUILD_CLIENT=OFF -DCMAKE_BUILD_TYPE=Release -DCC_ENABLE_STRICT_WARNINGS=ON -DCC_WARNINGS_AS_ERRORS=ON
cmake --build out/build/headless -j4
ctest --test-dir out/build/headless --output-on-failure -j4
CC_CPPCHECK_JOBS=4 python3 tools/static_analysis.py
cc -std=c17 -O2 -Isrc docs/reviews/trade-path-boundary-2026-09-08/probe.c out/build/headless/libcrownless_sim.a -lm -o /tmp/trade-path-probe
/tmp/trade-path-probe
out/build/headless/crownless_sim_runner --seed 42 --years 40 --json
out/build/headless/crownless_sim_runner --seed 0x5eed0001 --years 40 --json
```

To build the original private-function probe, define `CC_PARENT_PROBE` and use the base checkout's source include path and simulation library.
