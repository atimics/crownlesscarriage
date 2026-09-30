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

# L2-L4: gossip, trade, raiders

The same hook now covers three more decision families, each with the rule's own
choice as the default and zero weights reproducing it. `evolve_life.py --stage l2|l3|l4`
runs the search; each stage names the decisions the brain takes, the horizon and what is scored.
All three came out as nulls. They are reported as such.

## L2: gossip (share or withhold)

Carriers pass every story a town lacks, and a town keeps the first version it hears, so a
carrier could hold a stale version back for a fresher one. The score is how well informed the
world is: coverage (share of towns knowing the average story) times accuracy (mean confidence of
the version each holds).

| Withhold anything carried below confidence | Informed | Coverage | Accuracy |
| --- | --- | --- | --- |
| never (the rule) | 0.5247 | 67.7% | 77.5% |
| 30 | 0.5248 | 67.5% | 77.8% |
| 50 | 0.5233 | 66.2% | 79.1% |
| 70 | 0.5031 | 61.0% | 82.5% |
| 80 | 0.4411 | 50.6% | 87.1% |

There is no free lunch: accuracy rises with the floor but coverage falls faster, and the combined
score is flat at best. 150 generations of evolution found no candidate that beat "always share"
on validation, so its paired report is exactly zero. The trade-off itself is real, which makes
gossip reliability a candidate personality trait for L7 rather than something to optimise.

## L3: trade (which cargo a carriage takes next)

The planner enumerates every legal (good, source, destination) and takes the argmax of a
hand-written score. The hook offers the same candidates. Baseline over 100 worlds and two years:
town hunger 13.1, famine (hunger 25 or more) in 20.5% of town-days, prosperity 66.3.
Adding weights to the rule's own score, paired over 100 worlds:

| Change | Hunger | Famine | Prosperity |
| --- | --- | --- | --- |
| + hunger bonus 10 / 30 / 100 | +0.11 / +0.16 / +0.43 (worse) | +0.004 to +0.007 | -0.2 to -0.9 |
| + need bonus 5 / 20 | +0.48 / +0.91 (worse) | +0.006 / +0.016 | -0.4 / -1.1 |
| prefer far routes | +1.65 (worse) | +0.027 | -2.3 |

A 40-generation search gave -0.003 +- 0.009 (z = -0.3) on 100 held-out worlds. The rule looks
locally optimal and famine seems driven by production and consumption more than allocation.
Caveats: the search was short and ran on a loaded machine, outcomes vary by 8 hunger points
between worlds, and I have not searched longer or with a bigger population.

## L4: raiders (whether and where to raid)

Bandit raids run every 28 days for eligible bands (about 11 per world per two years); the hook
offers raid-or-hold and the choice of town at each end of the road.

- Raids barely touch the towns: with **no raids at all**, hunger changes by -0.03 +- 0.20, famine by
  -0.0001 +- 0.003, and prosperity rises by 0.9 of 66. The two-sided trade-off worried about in the
  curriculum barely exists in this sim.
- Under an even balance (one goods taken = one point of prosperity) 40 generations gave
  +0.042 +- 0.037 (z = 1.1): 5 more goods taken and 1 point of prosperity lost. No gain.
- The rule already takes the town with the most stock, so raiders have no loot headroom.
- Goblin raids (19 per world) are a separate code path and are not hooked.
