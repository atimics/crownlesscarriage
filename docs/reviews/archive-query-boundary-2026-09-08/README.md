# Archive query boundary

This advances the module boundaries in #260 and the archive-seat foundation in #240. Base: PR #582, e1aba65. Schema 73, generator 25.

Archive seat selection, spare grain, the public material-chain snapshot, and blocker names now live in `src/sim/cc_archive_queries.c`. Archive work uses the same internal seat and grain queries through `cc_archive_internal.h`. Public query signatures and selection rules remain the same. General incoming-shipment inspection remains shared through `CcSimIncomingGood`.

## Evidence

- Strict release headless build and all 109 CTest tests passed.
- Static analysis passed with one reviewed baseline item.
- `probe.c` compares every material-chain field and blocker name in 80 controlled cases against the base. It covers two seeds, schema profiles 33, 37, 57, 58, and 73, and eight states: supplied archive, all abandoned, seat-name fallback, missing scribes, missing binding materials, missing tools, missing food, and missing paper. Every query preserves the entire simulation state. These controlled schema profiles exercise query rules.
- Two current-schema simulations ran for 40 years. All 82 complete JSON checkpoints matched the base byte for byte, including state hashes. See `simulation-parity.json`.

Seat scoring, saved relocation, book journeys, and rival sponsorship remain acceptance work in #240. This boundary puts the existing query rules together for that work.

## Reproduction

```sh
cmake -S . -B out/build/headless -DCC_BUILD_CLIENT=OFF -DCMAKE_BUILD_TYPE=Release -DCC_ENABLE_STRICT_WARNINGS=ON -DCC_WARNINGS_AS_ERRORS=ON
cmake --build out/build/headless -j4
ctest --test-dir out/build/headless --output-on-failure -j4
CC_CPPCHECK_JOBS=4 python3 tools/static_analysis.py
cc -std=c17 -O2 -Isrc docs/reviews/archive-query-boundary-2026-09-08/probe.c out/build/headless/libcrownless_sim.a -lm -o /tmp/archive-query-probe
/tmp/archive-query-probe
out/build/headless/crownless_sim_runner --seed 42 --years 40 --json
out/build/headless/crownless_sim_runner --seed 0x5eed0001 --years 40 --json
```
