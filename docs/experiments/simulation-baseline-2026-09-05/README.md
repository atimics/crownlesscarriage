# Simulation baseline: 32 worlds over ten years

Measured on 5 September 2026 using the existing `crownless_sim_metrics` runner.
This is an autonomous baseline with annual observations. Intervention experiments
are specified in [the design proposal](../../design/power-memory-and-crowns.md).

## Provenance

- Source checkout: `29a8c02ea23f5205022e015eb7a772992ea5d2fd`.
- Design review base: `d048f2a71e2d62ee6efd9a1463e01f2e03b464b9`.
- The simulation sources and metrics tool are identical between these revisions.
- `cmake --build out/build/play --target crownless_sim_metrics` confirmed the
  existing binary was current.
- Command: `out/build/play/crownless_sim_metrics --seeds 32 --years 10`.
- Seed numbers 1 through 32 use `(uint32_t)seed_number * 0x9e3779b9`.
- Each report follows 365 simulated days: 3,650 days per world, 320 annual rows.
- The runner returned success and validated the world after each simulated year.
- Raw observations: [annual.csv](annual.csv).

## Observations

Values below are means across the 32 worlds. Reported town averages are already
rounded down to whole numbers by the metrics runner.

| Measurement | Year 1 | Year 10 |
| --- | ---: | ---: |
| Reported average hunger | 9.12 | 37.69 |
| Reported average prosperity | 76.09 | 45.16 |
| Reported average security | 65.44 | 48.69 |
| Reported average legitimacy | 74.69 | 40.06 |
| Crowns in town markets | 931.78 | 680.31 |
| Crowns in the dragon hoard | 147.75 | 1,225.84 |
| Dragon share of tracked crowns | 4.38% | 36.31% |
| Town market share of tracked crowns | 27.59% | 20.16% |
| Iron Ledger reserve | 480.00 | 377.25 |
| Recorded kingdom debt | 0.00 | 116.47 |
| Total population | 12,342.66 | 12,647.53 |
| Worlds with a town at hunger 100 | 0/32 | 21/32 |
| Worlds with an abandoned town | 0/32 | 0/32 |

Each world's tracked crown total is constant across its ten annual samples.
Each world still has six active settlements at year ten.

Year-ten outcomes vary: reported average hunger ranges from 4 to 64, prosperity
from 26 to 76, market crowns from 86 to 939, and debt from 0 to 648.

## Interpretation and limits

The cohort shows rising crown concentration and worsening hunger over time.
A controlled release of hoarded crowns can test whether cash availability
contributes to this pattern. Production, climate, routes, demand, war, and
archive rules may also contribute.

Annual snapshots establish the sampled states. Per-transition money auditing
and daily flow measurements would give stronger conservation and recovery
checks. This file starts at year one; the existing simulation test separately
checks initial versus final crowns for one 3,650-day seed.

The `event_count` and `shipment_slots` columns count retained entries or occupied
slots. Full-history totals and trade volume need their own counters. Current
legitimacy includes a direct archive-based target, so treat its decline in light
of the political rule discussed in the proposal.

## Reproduce

From a checkout of the source revision:

```sh
cmake -S . -B out/build/sim-experiments -DCC_BUILD_CLIENT=OFF -DCC_BUILD_BENCHMARKS=ON
cmake --build out/build/sim-experiments --target crownless_sim_metrics
out/build/sim-experiments/crownless_sim_metrics --seeds 32 --years 10 > annual.csv
```

To check the central counts and shares with Python's standard library:

```python
import csv
from collections import defaultdict
from statistics import mean

rows = list(csv.DictReader(open("annual.csv")))
by_seed = defaultdict(list)
for row in rows:
    by_seed[row["seed_number"]].append(row)
assert len(rows) == 320 and len(by_seed) == 32
assert all(len({r["tracked_gold"] for r in rs}) == 1
           for rs in by_seed.values())
for year in ("1", "10"):
    sample = [r for r in rows if r["year"] == year]
    hunger = mean(int(r["average_hunger"]) for r in sample)
    dragon_share = mean(int(r["dragon_hoard"]) / int(r["tracked_gold"])
                        for r in sample)
    hungry_worlds = sum(int(r["maximum_hunger"]) == 100 for r in sample)
    print(year, hunger, 100 * dragon_share, hungry_worlds)
```
