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

## Player-agency treatment

`crownless_agent_sweep` runs a paired control and agent world from the same
seed. The agent is currently a narrow road-steward policy: it uses real travel,
rest, encounter-withdrawal, and cash route-repair commands, with the starting
company purse as its only repair budget. It never mutates route or settlement
state directly.

```sh
out/build/release/crownless_agent_sweep --seeds 8 --years 10
# Run one exact metrics seed:
out/build/release/crownless_agent_sweep --seed 47 --years 100 --wip-limit 1
```

Hunger columns use `CcSimHungerSnapshot`: `control_hunger` and `agent_hunger`
are averages over inhabited settlements. The maximum and population-weighted
columns use that same population. Hunger is `-1` when every settlement is
abandoned; the active and abandoned counts make that case explicit. Prosperity
retains its average over all settlement slots.

Each row includes the numeric world seed, requested target day, actual control
and agent days, schema and generator versions, and final state hashes. The agent
can finish a journey after the target day. Use the actual days when comparing
endpoints. These rows describe whole-policy comparisons. Job and combat counters
cover the run; world columns describe the final snapshots. Keep the build revision
and command alongside captured CSV files.

The output compares population, prosperity, hunger, active settlements, and
closed routes, and records repairs, failed repair attempts, travel, accepted
jobs, completed jobs, and combat decisions/outcomes. The agent has an explicit
WIP limit (default 1), and emits an objective loss score: expired jobs cost 10,
abandoned jobs cost 10, unresolved lifecycle records cost 20, and lost combats
cost 5. An objective pass is a zero-loss row. The agent now accepts
route-repair charters before repairing them, so repair rewards can fund later
work. Relief-quest ranking and cargo delivery remain separate policies rather
than being conflated with road repair. During a journey encounter, it fights
moderate danger with a positive bargain value, negotiates high danger or poor
value, and withdraws from low-value encounters.

The metrics also include political and faction exposure:

- annual hunger thresholds and exact daily war/alliance exposure;
- exact daily dragon-campaign, goblin-raid, and bandit-raid exposure;
- daily bandit high-influence exposure;
- days spent in each dragon life stage;
- end-state goblin membership, devotion, cohesion, defenses, and interceptions;
- end-state bandit membership, supplies, influence, and completed raids;
- direct scriptorium state: scribes, stored/lost lore, stewardship, recording
  date, lore ceiling, tool wear, and abbot presence.

Use `tools/analyze_sweep.py` for group comparisons and
`tools/plot_archetypes.py` for report charts.

## Welfare and cluster review

Metrics version 2 appends precise welfare columns to every row. Existing
`average_prosperity` and `average_security` retain their whole-number averages
over all settlement slots, including ruins. The new `inhabited_hunger`,
`inhabited_prosperity`, and `inhabited_security` average living towns equally.
`weighted_hunger`, `weighted_prosperity`, and `weighted_security` weight each
living town by its population. These six fields keep six decimal places and
use `-1` when every town is abandoned. Report the abandoned-town count alongside
welfare so that collapse remains visible.

`years_population_weighted_hunger_40_plus` counts annual checkpoints with
population-weighted hunger at least 40. `years_without_population` counts
empty-population checkpoints separately. Existing `years_hunger_40_plus` and
`years_hunger_60_plus` use inhabited-town hunger. These counters sample once
every 365 days. Daily war exposure retains its separate daily counter.

Rows also include `day`, `schema_version`, `generator_version`, and `state_hash`.
Keep the source commit, executable hash, command, and sweep exit status with
saved results. September 6 sweeps used an older hunger definition that included
ruins. Regenerate those sweeps for welfare comparisons.

To inspect a world's final decade at weekly intervals:

```sh
out/build/play/crownless_sim_metrics --seed 1 --years 1000 \
  --settlements-csv out/seed-1-settlements.csv \
  --trace-start-day 361350 --trace-every-days 7 > out/seed-1-years.csv
```

Trace days are elapsed days since initialization. The default starts at day
zero and samples every 28 days. An explicit start day, interval boundaries,
and the final day each produce one observation. Each observation records every
town, including ruins, with its raw population, hunger, prosperity, security,
kingdom legitimacy, town size and function, dragon stage, and campaign victories.
`--final-only` controls the main CSV; the separate trace keeps its selected
interval. World validation still runs each year. Check the exit status before
using either output.

Create the welfare charts and a repeatable selection of worlds:

```sh
python3 tools/plot_welfare.py out/sweep-endpoints.csv --out out/welfare
# After recording selected worlds as seed-N-settlements.csv:
python3 tools/plot_welfare.py out/sweep-endpoints.csv \
  --out out/welfare --traces out/welfare/traces
```

The plotter requires metrics version 2 and one endpoint per seed at one common
year. It selects the world nearest each dragon group's median weighted
prosperity, plus the lowest seed at the most common prosperity and security
values. The report distinguishes original survivors, living successors after
campaign victories, and dragons slain at the endpoint. The town-history chart
uses the supplied trace window; the command above supplies the final decade.
