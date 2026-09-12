# Production capture records

Use the existing simulation runner to compare the baseline and opened production pilots, each with natural dragon history and the slain-at-day-one policy. Each of the four combinations runs twice. These are whole-policy comparisons.

From a clean checkout:

```sh
cmake -S . -B out/build/capture -DCC_BUILD_CLIENT=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build out/build/capture --target crownless_sim_runner -j4
python3 tools/capture_production.py --runner out/build/capture/crownless_sim_runner --output /tmp/crownless-production-capture --years 40 --seed 0x5EED0001
```

Choose a fresh output directory. The default seed is the direct numeric seed `0x5EED0001`. Annual checkpoints occur on days 1, 366, and so on through day 14,601 for a 40-year run. The runner validates its world at each annual boundary.

Protocol 7 writes `manifest.json` before launching a run and replaces it atomically as results arrive. It records the invoking checkout revision and working-tree status, runner binary hash, build mode when available, commands, checkpoint days, and final state hashes. Build the runner from the recorded checkout for a matching source/binary record.

Each capture has report, save, and stderr filenames. Hashes cover files present when the run finishes. A failed run may leave partial files; use its status and error fields when reviewing them.

- `running`: the last recorded phase was execution. Check the process before deciding how to recover from an interrupted capture.
- `complete`: the runner succeeded, the expected report checkpoints were parsed, and its saved artifact was present.
- `failed`: execution or validation failed. The manifest keeps the exit code when available and the error.
- `interrupted`: the capture caught a keyboard interrupt.

A group's `repeat_match` starts as `null`, then becomes `true` or `false` after both reports are complete. The overall manifest becomes `complete` after all four pairs match. A mismatch produces an overall failure while preserving both successful captures for comparison.

Completed runs remain recorded when a later run fails. Start a new capture in a fresh directory after fixing the cause, and retain the earlier directory as evidence. Manifest checkpoints provide execution records; the existing report protocol and simulation tests establish world validity and accounting.

Validation:

```sh
python3 tests/production_capture_tests.py
python3 tests/production_json_tests.py out/build/capture/crownless_sim_runner
```

The controlled runner tests cover checkpoint visibility at launch, later process failure, malformed reports, wrong checkpoint days, missing saves, repeat mismatch, file hashes, and successful completion. The production JSON tests exercise the real runner and its saved-state/report parity.

Each town includes a `grain_supply` account with its stored purse, spending, orders,
deliveries, losses, redirections, shipment IDs, and dispatch/arrival dates. These
counters cover the saved account history, including activity before a loaded run.
Its `plan` comes from `CcSimGrainDeliveryPlan` at the report day. The planner reports
the first applicable gate and the IDs selected so far. Status values follow
`CcGrainSupplyStatus`: ready (0), inactive (1), contact (2), bakery (3), stocked (4),
transit (5), blocked shipment (6), carriage (7), supplier (8), route (9), funds (10).
Use the snapshot beside stocks, hunger, waste, and road recovery reports when
assessing grain policy. Stored supplier and route IDs describe the account;
IDs inside `plan` describe the current decision.
