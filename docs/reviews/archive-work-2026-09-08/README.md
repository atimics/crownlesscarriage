# Archive work plan

This addresses nominal versus working scribe diagnostics in #266 and supports the archive foundation in #240. Base: PR #583 at e67be6a. Schema 73, generator 25, JSON protocol 6.

`CcSimArchiveWorkPlan` evaluates current staffing and held supplies. The archive work step calls it after its existing staffing adjustment, then consumes its wheat requirement. JSON uses the same query for `archive_work`, labeled `held_supply_plan_snapshot`.

The plan reports the seat, grain-supported scribes, wheat requirement, and recording readiness. The JSON also reports nominal scribes. Paper and tools gate recording separately from grain-supported staffing. The snapshot evaluates supplies at the checkpoint; the work step evaluates them at its execution time. The existing staffing, timing, knowledge, and binding rules remain in their respective steps.

Older rules retain their behavior: schemas before 34 support nominal scribes without a seat or wheat charge in this work step; schemas before 58 use the earlier spare-wheat reserve rule.

## Evidence

- Strict release headless build and all 110 tests passed.
- Static analysis passed with one reviewed baseline item.
- Nine controlled JSON cases cover grain-supported staffing, missing grain, restored grain, missing paper, missing tools, zero scribes, absent seat, schema 57 reserves, and schema 33 work. Every emission preserves the entire simulation state. A null query returns an empty plan.
- Save/load checks compare the work snapshot.
- Two 40-year runs match all existing JSON fields at all 82 checkpoints, including state hashes. Only the new `archive_work` object is removed for comparison. See `parity.json` and `fixtures.json`.

## Reproduction

```sh
cmake -S . -B out/build/headless -DCC_BUILD_CLIENT=OFF -DCMAKE_BUILD_TYPE=Release -DCC_ENABLE_STRICT_WARNINGS=ON -DCC_WARNINGS_AS_ERRORS=ON
cmake --build out/build/headless -j4
ctest --test-dir out/build/headless --output-on-failure -j4
CC_CPPCHECK_JOBS=4 python3 tools/static_analysis.py
out/build/headless/crownless_sim_runner --seed 42 --years 40 --json
out/build/headless/crownless_sim_runner --seed 0x5eed0001 --years 40 --json
```
