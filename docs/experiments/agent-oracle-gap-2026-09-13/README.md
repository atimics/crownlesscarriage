# Agent sweep oracle gap — 13 September 2026 UTC

My view: the road steward's greedy destination rule is its real decision, and it is a bad one. A one-step lookahead over the same command vocabulary beats it on most seeds while making about a tenth of the trips — and on the seeds where lookahead loses, it loses big, which says the value landscape is not exchange-convex: local improvements do not compose into global ones. Also, the steward's core verb never lands: route repairs fail universally in these worlds, which settles the repair-policy question left open by the 8 September sweep.

This report measures source `0625d99f` (plus this experiment's documentation commit). Issue [#729](https://github.com/atimics/crownlesscarriage/issues/729); theory note: [#727](https://github.com/atimics/crownlesscarriage/issues/727). All data is in `agent_oracle_gap.csv`; the run manifest is in `manifest.json`.

## What ran

- 100 paired worlds, metrics seeds 1–100, for 10 years each, work limit one job.
- Three worlds per seed: passive control, the greedy road steward, and the new oracle policy.
- The oracle shares the steward's exact command vocabulary (travel, rest, encounter resolution, withdrawal, cash route repair). It differs in two decisions only: which repair to attempt, and where to travel next. Each candidate action is scored by forking the world, applying it, rolling the greedy steward forward 60 days, and reading `CcSimWelfareSnapshot` (population dominant; welfare means, closed routes, and abandoned towns as tie-breakers).
- Run in a Release build in a separate worktree, ten processes of ten seeds, all 100 seeds passed validation at startup, every annual boundary, and the endpoint. Total wall time about two minutes.

## Headline numbers

| Measure (oracle minus greedy, positive = oracle better) | Mean | p10 | Median | p90 | Better / worse / equal |
| --- | ---: | ---: | ---: | ---: | --- |
| Population | +118 | -542 | +86 | +847 | 56 / 44 / 0 |
| Average hunger | +1.2 | -26 | +1 | +13 | 53 / 40 / 7 |
| Closed routes | +0.1 | -2 | 0 | +1 | 37 / 31 / 32 |
| Average prosperity | +0.5 | -8 | +1 | +6 | 51 / 43 / 6 |

The population gap is heavy-tailed both ways: from -1,511 (seed 26) to +1,702 (seed 23).

## The steward is currently a net negative; the oracle is not

Comparing each policy world against its passive control on endpoint population:

| Policy | Mean difference vs control | Better / worse |
| --- | ---: | --- |
| Greedy steward | -56 | 39 / 61 |
| Oracle | +61 | 60 / 40 |

The greedy steward makes the world worse than doing nothing on 61 of 100 seeds. The same action vocabulary, aimed by lookahead, flips the sign. The mechanism is visible in the travel counts: the greedy steward averages 389 successful journeys per 10 years; the oracle averages 40, and travels less than the steward on 99 of 100 seeds. The steward's most-closed-routes-adjacent rule generates churn — journeys, encounters, and job acceptances whose costs land on the world. Mostly staying home scores better than chasing closed roads.

## Order matters, so the value landscape is not M-convex

The issue asked: if the value function the steward optimizes were M-convex (matroid-exchange convex), greedy local swaps would be globally optimal and lookahead would gain nothing. Lookahead gains on average (+118 population) but loses outright on 44 of 100 seeds, with single-seed losses up to -1,151. If local improvements composed, a one-step improvement over greedy could not underperform greedy this often. They do not compose here. Practical reading: any planner for this world (agent or future game AI) needs either a longer horizon than 60 days or an objective aligned with the endpoint metric; myopic welfare scoring can steer into decade-scale traps.

The exchange probes were inconclusive for a different reason: the probe needs two simultaneously repairable adjacent routes, and 331 attempted probes across 14 seeds completed zero pairs, because the second repair always failed after the first. Which leads to the next finding.

## Repairs never succeed

Across all 100 seeds and both policies, zero route repairs completed. The greedy steward's repair attempts failed 500 times per run on average (they are not free — each failed attempt is a day of company time). Adjacent closed routes existed and candidates were found (the oracle evaluated them, and attempted 331 exchange probes), so the failure is in the repair command itself, not in finding work. With schema 99 / generator 25, `CC_COMMAND_REPAIR_ROUTE` as the steward issues it never lands. This confirms the "repair-policy fix" the 8 September sweep called for, and sharpens it: the steward's core verb is inert in current worlds, so every difference in this experiment comes from movement and job behavior, not repairs. Fixing route repair should precede any balance conclusion about player influence.

## Tool changes shipped with this run

- `crownless_agent_sweep --oracle --oracle-horizon DAYS --exchange-probes COUNT` adds the third world and the gap columns. Without `--oracle` the output is byte-identical to the previous tool (verified across eight seeds).
- Gap columns are oriented so positive always means the oracle ended better.
- The steward policies gained a mine-branch pass handler: journeys pause permanently at an unpassed mine site, and the old policy had no answer, so any world whose route crossed a mine branch stalled the sweep. The handler issues `CC_COMMAND_PASS_ROAD_SITE` only at the mine branch itself; ordinary roadside sites still pass without a command.

## Reproduce

```sh
out/build/release/crownless_agent_sweep \
  --seed 1 --seeds 100 --years 10 \
  --oracle --oracle-horizon 60 --exchange-probes 25
```

Split into ten ranges of ten seeds for the recorded run; see `manifest.json`. World seeds derive from metrics seeds as `seed * 0x9E3779B9 & 0xFFFFFFFF` as documented in `docs/simulation-sweeps.md`.

## Follow-ups

1. Fix or explain route repair failure (schema 99 / generator 25), then re-run this experiment; with repairs live, the gap structure may change completely.
2. Sweep the horizon (30/60/120/365 days) to find where lookahead stops losing seeds — that length is the world's real interaction depth.
3. Re-run the exchange probes once simultaneous multi-route repairs are possible; the order-commutativity measurement needs them.
