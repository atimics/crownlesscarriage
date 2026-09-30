# L1: villagers' daily life

The simulation makes a few choices for road-going people (scouts, travellers,
refugees, couriers; about 60 per world) by fixed rule. `src/sim/cc_policy.h` adds an
optional decision hook at those points: the rule computes its legal options and its
own choice, then a process-wide hook may pick another legal option. With no hook, or
one that returns the default, behaviour and the state hash are unchanged
(`tests/policy_hook_tests.c`). The hook is not part of `CcSim`, so it is never saved.

| Decision | Options | Rule |
| --- | --- | --- |
| travel destination (weekly) | neighbouring towns, or stay | leave with probability 25% (60% when away), then a random town; head home first when away |
| meal (daily) | buy the cheapest, or go without | buy when affordable |
| lodging (daily) | pay for the inn, or sleep rough | pay when affordable |
| bandit join (weekly, after 3 hungry or homeless days) | join a camp, or hold out | join |

`tools/crowdsim.c` runs the world in process and holds a 529-weight scoring network
(31 inputs, 16 hidden units) in C; zero weights reproduce the rule exactly.
`cs_run` totals hunger, shelter, outlawry, stress and movement per day.
`evolve_life.py` evolves the weights with evolution strategies against welfare
(hungry, plus half of unsheltered, plus outlawed, per road person-day) over a season,
60 training worlds, 30 for validation, 100 held out.

## A reward hack, and its fix

The first search reported a spectacular gain (hunger 21.7% to 0.5%, all 100 worlds
better). The brain had stopped travelling: residents at home are reset to fed each day,
and it had also learned to hold out of every bandit camp. Town changes fell from
222-323 to 3-7 per world. Now the rule keeps the decision to stay or go, the brain only
chooses where, bandit recruitment stays with the rule, and movement is reported as an
invariant.

## Result (three evolution seeds, 100 held-out worlds each)

| | Rule | Evolved (range over seeds) |
| --- | --- | --- |
| hungry, share of person-days | 21.7% | 16.5% to 17.0% (-4.8 to -5.2 points, z about -16) |
| outlawed | 14.1% | about 5% (-8.8 to -9.4 points, z about -26) |
| unsheltered | 4.1% | -0.2 to +0.2 points on two seeds, -1.9 on one |
| stress (mean) | 40.3 | +0.3 to +1.3 (z 1.5 to 5.5) |
| town changes per person per season | 4.3 | +12% to +17% |
| welfare | -0.379 | +0.136 to +0.155, better in 100 of 100 worlds every seed |

The gain comes only from destination choice: the network changes 30% of travel picks
and none of the meal or lodging choices, so the rule is already optimal there. I have not
analysed which destinations it prefers.

```sh
python3 tools/dialogue/evolve_life.py --output /tmp/life --generations 150 --workers 10
```
