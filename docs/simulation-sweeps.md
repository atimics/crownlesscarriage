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

Both paired worlds pass structural validation at startup, each annual boundary,
and the endpoint. The control reaches each boundary through daily steps. The
agent is checked after its first completed action at or beyond a boundary; its
failure report records the scheduled boundary and the actual day. A failure
stops the sweep with a failing exit status and includes the seed, rules versions,
state hash, and reproduction command. Each run still emits one endpoint CSV row
per completed seed. Validation observes the simulation through its read-only API.

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

## Roadside recovery diagnostics

`crownless_sim_runner --detail` emits one `roadside_recovery` snapshot per route.
The snapshot uses the same `CcSimRoadRecoveryPlan` that the daily recovery code
executes. It identifies the route, endpoints, chosen labor base and supplier,
work date, available and required people/materials, planned work, and every
unmet gate. `blocked=ready` means this roadside effort can execute at that exact
snapshot. Fields set to `-1` are unavailable because the route or endpoint is
missing. Entity IDs of `0` are unavailable.

These are engine inspection records. They describe communal roadside recovery;
kingdom-funded repairs, routine upkeep, and recolonization have their own rules.
An annual endpoint can differ from the next work day's state because production,
trade, and other repairs run before the recovery decision. The report identifies
snapshot gates; activity counts require interval accounting.

## Campaign launch diagnostics

Detailed runner output includes a `campaign_launch_snapshot`. The shared
`CcSimCampaignLaunchPlan` supplies the same preparation and departure gates that
execution uses. `prepare_eligible` reflects phase, cooldown, dragon life/age, and
pledges. Held food, Tools, Weapons, patron, hero, and origin describe the current
snapshot. Preparation can name leaders and draw or commission supplies before
execution evaluates departure again. Deep wyrms satisfy the age gate at any age.

The row includes all unmet gates. `attempts` is the existing lifetime departure
count. The diagnostic describes launch prerequisites at inspection time; the
royal diplomacy schedule controls when preparation is called. A ready snapshot
therefore describes readiness at that instant. The same read-only inspection
adds engine information to the runner. Coalition formation, supply availability
above reserves, and ritual eligibility remain separate diagnostic work.

## Successor ritual offerings

`--detail` includes a `ritual_offering_snapshot` for the named cult and lair.
`CcSimRitualOfferingPlan` is the offering check used by clutch revelation after
annual gathering, cult changes, and timer advancement. It reports every unmet
membership, devotion, cohesion, coin, relic, nutrition, Tool, and Weapon threshold.
The planned egg count uses the same rule as execution.

The snapshot also identifies the current ritual phase, timer, afterdeath age,
tribute phase, and existing eggs. `blocked=ready` applies to held offerings;
the annual schedule and ritual stage determine when execution checks them.
Tools and Weapons thresholds include retained equipment: revelation consumes
one of each after requiring two Tools and three Weapons. Food costs twelve
rations and the transfer to the dragon uses 120 coins and two relics.

## Input limits

The runner accepts raw world seeds from 0 through 4294967295, including hexadecimal
notation. Durations and intervals use complete decimal integers. Zero years saves
or inspects the initial or loaded state. A zero report interval retains the annual
report default, and a zero checkpoint interval disables intermediate saves.

Metrics seed indexes range from 1 through 2147483647 and retain the documented
32-bit seed mapping. A requested index range must fit within that bound. Durations
must fit the simulation's signed day counter. Invalid numeric values, unknown
runner options, and missing arguments produce a failing exit status before output
files are written. Resumed chronicle headers identify the world seed from the save.

## Bandit exposure scope

`days_bandit_raid` and `days_bandit_influence_70_plus` count each sampled day
once when any bandit group qualifies. The matching `years_*` columns count
annual endpoint samples with any qualifying group. They measure endpoint
observations, while the `days_*` columns measure daily exposure.

`bandit_raid_group_days` and `bandit_influence_70_plus_group_days` sum each
group's daily activity. Their `*_group_year_samples` counterparts sum qualifying
groups at annual endpoints. Two active groups on one day add one day of exposure
and two group-days. These group totals retain the previous counters' meaning.

`first_bandit_id` identifies the group described by the existing `bandit_influence`,
`bandit_members_end`, `bandit_supplies_end`, `bandit_influence_end`, and
`bandit_raids_end` snapshots. An ID of zero means that group is absent. Per-group
interval tables remain further diagnostic work.
