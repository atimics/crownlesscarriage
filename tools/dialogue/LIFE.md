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

`tools/crowdsim.c` runs the world in process and holds a scoring network in C
(833 weights: 48 inputs, 16 hidden units, sized for every decision kind; L1 was first run
with an earlier 529-weight layout and reproduced on this one). Zero weights reproduce the rule exactly.
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
L2 and L4 came out as nulls; L3 gave a small gain once searched properly. Each is reported as found.

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

A short 40-generation search on a loaded machine was level with the rule (-0.003 +- 0.009). A proper one
(120 generations, 64 candidates, 48 worlds per generation) looked marginal on 100 held-out worlds
(+0.008 +- 0.009, z = 0.8), so I tested the same weights on 400 fresh worlds, as at L5:

| Evolved versus the rule, 400 fresh worlds | Difference | z |
| --- | --- | --- |
| town welfare | +0.0167 +- 0.0046 | 3.6 |
| town hunger | -0.52 +- 0.18 | -2.9 |
| famine (share of town-days at hunger 25 or more) | -0.0106 +- 0.0026 (20.5% to about 19.4%) | -4.1 |
| prosperity | +0.42 +- 0.19 | 2.2 |

Better in 239 of 400 worlds. It is a small gain (about 4% of hunger), real on disjoint worlds, and it comes from
a combination of features that the one-parameter probes above could not see. Famine still covers about a fifth of town-days,
so allocation is not the main driver. One search, one seed.

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

# L5: kingdoms (grain relief)

Every 28 days a kingdom whose hungriest town is at hunger 38 or more, with 28 crowns in the
treasury, sends the crowns to that town and gains 2 legitimacy (about 17 decisions per world in two
years). The hook offers any hungry town of the kingdom, or holding the treasury.

- The whole lever is small: with **no relief at all**, hunger rises by 0.63 (of 13.1), famine by 1.2 points
  of town-days and legitimacy falls by 0.9.
- A 60-generation search looked promising on 100 worlds (hunger -0.29, z = -1.3, welfare +0.004 +- 0.007).
  On 400 fresh worlds it was slightly **worse**: welfare -0.007 +- 0.0035 (z = -2.0), legitimacy -0.35, treasury
  +5.6 (it funds less). The earlier hint was noise, so the rule (fund the hungriest) stands.
- A full search (120 generations, 64 candidates, 48 worlds per generation) gave +0.009 +- 0.007 on 100 held-out worlds
  and, on 400 fresh ones, welfare -0.0009 +- 0.0038 (z = -0.2, better in 185 and worse in 185), hunger +0.07 (z = 0.5),
  legitimacy +0.05 (z = 0.3). Its treasury ends 3.9 crowns higher (z = 2.7): a slightly thriftier kingdom, not a better one.
  The null now rests on a full search and a 400-world test, like L3's gain.
- War orders and succession could not be tested: WAR_DECLARED never fires in unattended worlds.

# L6: the dragon (which town to burn)

A dragon owed a theft retaliates after an omen against the recorded target or, failing that, the
"richest" town (4 x prosperity + 8 x services + treasury / 20). The hook offers every town except
its lair. Unattended, a dragon retaliates only 0.4 times per world in two years, so
`dragon_probe.py` injects a 300-crown theft and forces each candidate town in turn
(100 worlds, 120 days):

| Target | Restitution collected | People lost | Prosperity lost | Legitimacy lost |
| --- | --- | --- | --- | --- |
| rule (richest) | 349 | 305 | -8.6 | -5.8 |
| town #0 | 349 | 123 | 11.9 | -5.9 |
| town #1 | 349 | 191 | 13.5 | -5.8 |
| town #2 | 349 | 136 | 4.9 | -2.8 |
| town #3 | 348 | 217 | 23.5 | -5.0 |
| town #4 | 349 | 307 | -8.6 | -5.8 |

Negative prosperity or legitimacy "lost" means it rose over the 120 days.

- The dragon collects the same restitution whichever town burns, so its own payoff gives no
  reason to prefer any target. There is nothing here for optimisation to find on the dragon's side.
- The rule's target is the most damaging one: 40-60% fewer people are lost at the same payoff
  in the alternatives. Whether the dragon should be vengeful or restrained is an authorial
  choice, which makes it a good personality trait for L7 rather than something to optimise.
- Only target choice is hooked. Hunting, brooding and campaigns never fire unattended and are
  not covered; the story-review half of the gate (does an omen and a reckoning still make sense?) is not done.

# L7: lineages and heritable traits

`lineage.py` gives every person four inherited preferences (a taste for rich, cheap, near and home
towns) that shift the shared brain's travel scores, passes them to the successor at each succession
with mutation, and runs three conditions on the same worlds: `none` (the brain alone), `drift`
(inherited and mutated, no selection) and `selection` (with probability 0.5 a successor copies the
traits of the best of three road-going people by their own hunger).

In this sim a "birth" is a **succession**: when someone dies of age, a successor takes the same slot
with the same role, occupation and home, at generation + 1, age 0. Death is scheduled by age, not
caused by hunger, so nothing selects on traits inside the sim; selection here is added by the harness.

## What long runs showed about the simulation

- Long unattended runs freeze. **Every road closes within about 20 years** (open routes fall from 7 or 8
  of 8 to 0 of 8 in the three worlds checked), after which nobody travels; by year 100 two of six
  towns are abandoned. Town hunger also peaks around year 10 (57 under the rule).
- Successors are born as children, and the road population shifts toward under-16s who cannot travel.
- A 1,000-year run with the natural lifespan had about 13 generations but no travel after year 50, so
  traits could not matter and all conditions were identical (z about 0). That run is a null, not evidence.

## Accelerated generations

To get generations inside the window in which roads are open, the harness sets lifespans to 1.5-3
years and seats each successor as an adult. 12 years, 32 worlds, about 4.7 generations:

| | Road hunger (last 2 years) | Trait spread, start to end |
| --- | --- | --- |
| brain alone | 0.2994 | - |
| drift | 0.3066 (+0.007 +- 0.005 vs brain alone, z = 1.4) | 0.494 to 0.582 |
| selection | 0.2972 (-0.002 +- 0.006, z = -0.4) | 0.493 to 0.565 |

- Selection versus drift: -0.0094 +- 0.0053 (z = -1.8), marginally better.
- Diversity is sustained: trait spread rises slightly under both, and selection is only a little lower
  than drift. Nothing collapses to one strategy.
- Average trait values barely move under selection (all within noise), so there is no directional
  evolution to report. The traits have a small effect on outcomes, and 4-5 generations is few.
- The gate (diversity sustained without loss of welfare) holds in the weak sense: no collapse and no
  measurable loss. It does not show that selection helps.

```sh
python3 tools/dialogue/lineage.py --brain /tmp/life/theta_best_validation.npy --output /tmp/lineage.json \
  --years 12 --seeds 32 --lifespan 1.5 3 --cultural 0.5
```
