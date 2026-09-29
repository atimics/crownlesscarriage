# Scoring policies by outcomes

`outcomes.py` runs participant policies through the native world and scores what
happens, not whether a choice matches the authored preference. A scenario is a
saved world plus two co-located people, one hungry. `play` drives the real
promise, accept and execute commands on a private copy of the save and reads
before and after snapshots.

```sh
python3 tools/dialogue/outcomes.py --food-probe BUILD/crownless_food_relief_probe \
  --participant-probe BUILD/crownless_participant_probe --worlds /tmp/worlds \
  --limit 2000 --seeds 399 --only-feasible \
  --policies teacher random stingy generous \
  "model:afford:/path/model.ccv2:/path/probe"      # STYLE:MODEL:PROBE for a trained checkpoint
```

Components per exchange: hunger relieved, helper still fed, crowns to the market,
units taken, store share left, turns, and a conservation check (money moves only
with stock). `score` is one number for comparison: relief, minus a penalty for a
hungry helper, small costs for crowns and turns, and a penalty for emptying the
store. The components are the evidence; the weights are a choice.

A run on 399 seeds harvests thousands of scenarios, but only about 5% are
feasible (a hungry person, a helper who can pay for a unit, and stock on the
shelf), 96 of 2,000 here. `--only-feasible` keeps those.

## Findings (96 feasible scenarios)

| Policy | Score | Hunger relieved | Crowns spent |
| --- | --- | --- | --- |
| teacher | 0.638 | 100% | 7.05 |
| random legal | 0.068 | 18% | 1.82 |
| stingy (never spend) | -0.04 | 0% | 0 |
| generous (largest purchase) | 0.305 (on the 26-scenario run) | 100% | 19.2 |

Trained checkpoints, same scenarios (relief in the sim / agreement with the rule
on the procedural test):

| Model | Relief | Agreement |
| --- | --- | --- |
| digits, seed 19 | 15% | 95.1% |
| digits, seed 1 / 2 / 3 | 98% / 96% / 76% | 99.4% / 98.8% / 99.4% |
| buckets, seeds 1-3 | 100% each | 99.4-99.9% |
| afford, seeds 1-3 | 100% each | 99.6-99.9% |
| digits and afford, 8x data | 100% | 99.7% / 100% |

- Agreement with the rule does not predict outcomes: digit seed 3 agrees on
  99.4% of turns and leaves a quarter of hungry people unhelped.
- Bucketed inputs match the teacher's outcome on every seed.
- The teacher is at the ceiling for this objective: it relieves every feasible
  case while buying about one unit. Nothing in this food loop can be gained by
  learning from outcomes. A learned policy needs a scenario with a real
  trade-off, such as several hungry people at one store, a helper's future needs,
  or trust that changes over repeated meetings. Those need simulation support.
