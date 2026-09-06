# Simulation sweeps

`tools/sim_metrics.c` validates the simulation after every reported year. A
single invalid seed used to abort a multi-seed run, which hid the rest of the
sample. Use `tools/sim_sweep.py` for independent, parallel seed runs; it
continues after failures, reports each failed seed and checkpoint, and exits
non-zero if any seed fails.

```sh
python3 tools/sim_sweep.py \
  --binary out/build/play/crownless_sim_metrics \
  --first-seed 1 --seeds 1000 --years 1000 --jobs 8 \
  --output out/sweep-endpoints.csv
```

The metrics seed number is converted to a deterministic world seed by:

```text
world_seed = (seed_number * 0x9E3779B9) & 0xFFFFFFFF
```

The metrics runner also accepts one explicit seed. `--final-only` keeps
validation after every year but emits only the endpoint row, which avoids doing
annual metrics aggregation and CSV formatting during large sweeps:

```sh
out/build/play/crownless_sim_metrics --seed 10 --years 1000 --final-only
```

The chronicle runner accepts a raw world seed, not a metrics seed number. To
chronicle metrics seed 10, use world seed
`(10 * 0x9E3779B9) & 0xFFFFFFFF = 774553914`.

A sweep's endpoint CSV contains only successful endpoint rows. Failed seeds remain in
the terminal report with their validation error; this keeps aggregate results
usable while making failures impossible to overlook. The sweep uses
`--final-only`; yearly validation still runs for every checkpoint.

The endpoint row also includes trajectory summaries:

- `minimum_active_settlements`: lowest active-settlement count reached;
- `maximum_closed_routes`: highest simultaneous route closure count;
- `years_all_routes_closed`: annual checkpoints where every route was closed;
- `years_with_abandoned_settlement`: annual checkpoints with at least one abandoned settlement;
- `route_closures`: closed-route transitions over the run;
- `settlement_abandonments`: settlement-abandonment transitions over the run.

These distinguish a world that ends in decline from one that spent most of its
history in decline.
