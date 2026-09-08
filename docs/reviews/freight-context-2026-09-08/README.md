# Freight snapshot context

This supports identified transport diagnostics in #266 and the transport foundation in #248. Base: PR #587 at c7e77b8. Schema 73, generator 25, JSON protocol 6.

Carriage rows now include kingdom identity, target identity, blocked-since day, next-dispatch day, and the shared readable mode name. Each row is labeled as a snapshot, with trips and losses labeled as stored cumulative counters. Shipment rows are labeled as retained shipment snapshots. Existing shipment IDs link carried goods to the carriage record.

These fields report saved state. A zero waiting date retains its stored meaning. The report leaves future dispatch and passage decisions to their existing rules.

## Validation

- Strict release headless build passed.
- The production JSON and runner resume tests passed. They cover repeated reports, save/load, policy fixtures, and resumed execution.
- The new identity and waiting fields are compared directly with the saved SQLite `royal_carriage` table. Complete carriage and shipment arrays are compared after save/load.
- Static analysis passed with one reviewed baseline item.
- Two 40-year runs match all existing JSON fields at 82 checkpoints, including simulation hashes. Only the new carriage fields and shipment semantics label are removed for comparison. See `parity.json`.

## Reproduction

```sh
cmake -S . -B out/build/headless -DCC_BUILD_CLIENT=OFF -DCMAKE_BUILD_TYPE=Release -DCC_ENABLE_STRICT_WARNINGS=ON -DCC_WARNINGS_AS_ERRORS=ON
cmake --build out/build/headless -j4
ctest --test-dir out/build/headless --output-on-failure -R 'production_json_capture|simulation_runner_resume'
CC_CPPCHECK_JOBS=4 python3 tools/static_analysis.py
out/build/headless/crownless_sim_runner --seed 42 --years 40 --json
out/build/headless/crownless_sim_runner --seed 0x5eed0001 --years 40 --json
```
