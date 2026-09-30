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

## A week, and a crowd

`--days 7` has the pair meet once a day for a week (the world advances a day
between meetings) and scores hungry days for both people. In pairs the teacher
still ties the alternatives: generous feeds the hungry person better (1.86
hungry days against 2.57) but leaves the helper hungrier (1.23 against 0.56);
total hunger is 3.09 against 3.13. Reserve-a-meal variants of the teacher land
exactly on it, because the hungry side already counters any offer above one unit
when the store is short.

`--crowd` has one helper meet each of several hungry people in turn, on real
saves. The store and the helper's purse carry over between conversations.
Natural crowds are common (1,872 places with two or more hungry people and a
solvent helper in 399 seeds), but 76% have an empty food store, so
`--only-feasible` keeps those with stock to divide (375 of 1,500 here).

| Policy | Score | People relieved (of 3.1) | Crowns | Units |
| --- | --- | --- | --- | --- |
| one unit each | 0.507 | 2.88 | 18.2 | 2.88 |
| teacher | 0.477 | 2.86 | 20.4 | 3.44 |
| generous | 0.094 | 2.39 | 38.4 | 6.22 |
| random | -0.582 | 0.44 | 4.7 | 0.74 |
| stingy | -0.752 | 0.00 | 0 | 0 |

Rationing matters: generous drains the store and the helper's purse on the first
buyer and relieves fewer people. Paired over the same 375 crowds, one unit each
beats the teacher by +0.032 +- 0.005 (z = 6.8): better in 66 crowds, worse in 7,
never relieving fewer people, and spending 2.2 fewer crowns. The teacher buys
extra units for someone who needed one in about 15% of crowds. That is the
first measured case where the authored rule is beatable, and it is small.
`--baseline teacher` prints these paired differences.

## Learning from what each option causes

`counterfactual.py` runs a crowd with a base policy and, at every decision with
more than one legal option, clones the world, replays each other option from the
clone with the base policy continuing, and scores the whole crowd. The best
measured option becomes the label; if nothing beats the base choice by more than
1e-6, the label stays with it. Rows are written in the C trainer's format
(`train.txt`) and with all utilities (`rows.jsonl`). Training is the ordinary
`ctrain` run on the procedural rows (anchor) plus these rows, in two extra
sampling pools.

215 feasible crowds gave 1,949 decisions in 15 minutes; 167 were relabelled
(mean gain +0.096). Evaluated on 189 crowds the trainer never saw (paired
against the teacher, `--offset 900`):

| Policy | Score | People relieved | Crowns | Paired vs teacher |
| --- | --- | --- | --- | --- |
| teacher | 0.447 | 2.83 | 20.4 | - |
| buy one unit (hand-written) | 0.479 | 2.85 | 18.1 | +0.032 +- 0.006 |
| learned, all relabels | 0.396 | 2.67 | 16.8 | -0.051 +- 0.021 |
| learned, refusals reverted | 0.479 | 2.85 | 18.1 | +0.032 +- 0.006 (better in 31, worse in 0) |

- Learning from outcomes rediscovers the one-unit rule: 134 relabels reduce the
  quantity offered or countered, and a model trained on them matches the
  hand-written rule on unseen crowds without being told it.
- Relabels to declining or ending (33) hurt: 21 held-out crowds lost relief. Each
  was better in the crowd where it was measured, but the model sees only the
  current pair, cannot tell when others are waiting, and applies the refusal
  everywhere. `train_label` reverts them by default (`--keep-refusals` to teach
  them). Teaching them correctly needs the crowd in the model's view.
- The learned policy ties the hand-written rule; it does not beat it. One seed.

## In-process worlds

`tools/crowdsim.c` (CMake target `crowdsim`) keeps a food-relief world in memory
and exposes the probe operations as function calls; `fastworld.py` wraps it with
the interface of `world_dialogue.World`, and `crowd.py` holds the shared `Crowd`
class (a helper meeting each hungry person in turn, resumable and cloneable).
A snapshot takes 0.2 ms instead of about 300 ms, and a clone is a 0.4 ms memory
copy of the 1.7 MB world. On 24 crowds and four policies (96 episodes) the results
are identical to the probe-driven reference, at 53 times the speed; the probes
remain available (`fast=False`). Processes load private copies of a save, because
SQLite locks a save that several processes open at once.

## Evolving one brain

`evolve.py` evolves a small option-scoring network (53 features, 24 hidden units,
1,321 weights) with evolution strategies: antithetic sampling, rank-shaped
fitness, Adam. It never sees a teacher's choice or a rule; fitness is the mean
crowd score over 60 crowds per generation, the same crowds for every candidate.
One weight vector plays both the helper and the hungry person; individuals differ
only through their inputs (purse, hunger, trust, how many are waiting, what was
said). 300 generations of 64 candidates took 15 minutes.

From random weights, with no labels, the evolved policy reaches the same result
as the hand-written one-unit rule on the 189 held-out crowds: score 0.481,
+0.032 +- 0.006 over the teacher, better in 31 crowds, worse in 0, identical on
every reported component. Validation stopped improving at generation 30 (110 s).
Its inputs included the queue count.

Three routes now agree on the ceiling for this view and objective:

| Route | Held-out score | Paired vs teacher |
| --- | --- | --- |
| hand-written one unit | 0.479 | +0.032 +- 0.006 |
| counterfactual labels, refusals reverted | 0.479 | +0.032 +- 0.006 |
| evolved from random weights | 0.481 | +0.032 +- 0.006 |

## The queue count

The `queue` input style adds how many others are still waiting. With it, teaching
the refusal relabels no longer hurts: -0.051 (z = -2.5) without the queue,
+0.012 +- 0.009 (z = 1.3) with it, though still below the +0.032 of leaving
them out. Evolution, which optimises the average outcome directly, did not pick
up refusals even with the queue in view. In this food loop the extra information
did not open headroom above the one-unit rule.
