# NPC training curriculum

Generated from `tools/dialogue/curriculum.json` and `docs/npc-census.json` by `tools/dialogue/render_curriculum.py`. Edit the JSON, not this file.

Census: 60 unattended worlds of 730 days each (`tools/sim_census.c`, `tools/dialogue/sim_census.py`). Per world: 96 people, 6 settlements, 3 kingdoms, 4019 events; 65 of the event kinds never fire without a player.

## Principles

- Order stages by how often the situation arises unattended, how clearly the decision is defined, how measurable the outcome is, and how small the interface change is.
- Every stage keeps the current rule as the baseline and the fallback, and advances only on a paired gain on held-out seeds.
- One shared network per role family; individuals differ through inputs.
- The validator owns legality; the network only chooses among legal options.

## What the runs found

- L1 shows a clear gain over the rule and L3 a small one. L2 and L5 are nulls, L4 is a trade between raiders and towns, L6 is a design axis and L7 holds only weakly. The hand-written rules are close to a local optimum for the objectives measured, except where they choose at random (travel destination).
- A reward hack appeared at once: an unconstrained daily-life brain stopped travelling (residents at home are reset to fed) and refused every bandit camp. Fixing the rule's decision to go, leaving story-critical choices with the rule, and tracking movement as an invariant closed it.
- Small effects need many worlds, in both directions. A 100-world hint at L5 reversed on 400 fresh worlds, while a marginal 100-world result at L3 became significant on 400. Town outcomes vary by 8 hunger points between worlds.
- The unattended sim decays: every road closes within about 20 years, two of six towns are abandoned by year 100, and successors are born as age-0 children. Long-horizon experiments are only valid for the first 10 to 15 simulated years.

## Stages

| Stage | Name | Status | Verdict | Interface | Depends on | Model |
| --- | --- | --- | --- | --- | --- | --- |
| L0 | Food-relief exchange | done | - | ready | - | 5M policy |
| L1 | Villager daily life | done | gain | ready | L0 | A shared 833-weight scorer in C (zero weights reproduce the rule) |
| L2 | Gossip and requests | done | null | ready | L0 | 5M |
| L3 | Trade, shipments and production | done | small gain | ready | L1 | 1M to 5M |
| L4 | Raiders and monsters | done | trade-off | ready | L3 | 5M |
| L5 | Kingdoms and war | done | null | ready | L3, L4 | 5M to 10M |
| L6 | Dragons | done | design axis | partial | L4, L5 | Start at 5M |
| L7 | Lineages and open-ended evolution | done | weak | partial | L1, L2 | One shared brain plus a per-person trait vector |

## L0. Food-relief exchange

**Status:** done. **Actors:** villager (hungry), villager (helper).

**Decision.** Ask, offer, counter, accept or decline food between two people who share a place.

**Sees:**
- own purse, hunger days, stress
- trust toward the other person
- store stock, reserve target, unit price
- remembered outcomes
- people still waiting (queue)

**Chooses among:**
- 12 intents (request, offer, counter, accept, decline, condition, recall, thank, end) as at most 16 legal candidates

**Judged by:**
- hunger relieved now and a week later
- crowns spent
- store share left
- helper still fed

**Interface (ready).** In-process crowd harness (crowdsim/fastworld/crowd); probes remain the reference.

**Unattended, per world:** SHORTAGE 19.2; CHARACTER_INTERACTION 11.6; RELATIONSHIP_CHANGED 11.8.

**Commands:** `CC_COMMAND_FOOD_RELIEF_PROPOSE`, `CC_COMMAND_FOOD_RELIEF_ACCEPT`, `CC_COMMAND_FOOD_RELIEF_EXECUTE`.

**Structures:** `CcFoodAgreement`, `CcCharacter`, `CcSettlement`.

**Model size.** 5M policy; a 1,321-weight scorer already matches it.

**First experiment.** Done: hand-written rule, counterfactual labels and evolution all reach +0.032 over the teacher on held-out crowds.

**Gate to advance.** Reached: paired gain over the teacher, better in 31 crowds and worse in 0. The ceiling for this view and objective.

**Risks:**
- Ceiling reached: more work here buys nothing without a richer view or objective.

**Evidence:** tools/dialogue/OUTCOMES.md.

## L1. Villager daily life

**Status:** done; verdict: gain. **Actors:** villager (any role).

**Decision.** What to do today: work, seek aid, buy a meal, lodge, travel, hide, or join a bandit group.

**Sees:**
- role, occupation, goal, activity
- purse, hunger days, unsheltered nights, stress
- local prices, stock and services (inn)
- known routes and their danger

**Chooses among:**
- travel destination among neighbouring towns (only when the rule decides to go)
- buy the cheapest meal or go without
- pay for a bed or sleep rough
- join a bandit camp or hold out (left with the rule: story-critical)

**Judged by:**
- days hungry and days unsheltered
- survival and purse over 90 days
- recruitment into bandit groups (17.7 people per world are hiding)
- stress

**Interface (ready).** CcSimSetPolicy (src/sim/cc_policy.h): the rule computes its legal options and its own choice, then an optional process-wide hook may pick another legal option. Sites: weekly travel destination, meal, lodging, bandit join. With no hook, or one returning the default, the state hash is unchanged (tests/policy_hook_tests.c).

**Unattended, per world:** SHORTAGE 19.2; BANDIT_PRESSURE 14.6; CHARACTER_INTERACTION 11.6.

**Commands:** `CC_COMMAND_PERSONAL_WANT`, `CC_COMMAND_CHARACTER_RESPONSE`.

**Structures:** `CcCharacter`, `CcSettlement`.

**Model size.** A shared 833-weight scorer in C (zero weights reproduce the rule); 5M is not needed.

**First experiment.** Done: evolved a shared scorer against a season of welfare in 60 worlds; three seeds, then again on the final layout, tested on 100 held-out worlds.

**Gate to advance.** Reached: paired welfare gain over the rule on held-out seeds (z >= 3), bandit membership down, money conserved by construction. Watch stress.

**Risks:**
- Reward hacking: an unconstrained brain stopped travelling (residents at home are reset to fed) and refused every bandit camp. Travel frequency now stays with the rule and movement is a tracked invariant.
- Bandit recruitment is story-critical and stays with the rule; the brain lowers it indirectly by lowering hunger.
- Only about 60 road-going people per world face these choices.

**Evidence:** tools/dialogue/LIFE.md; src/sim/cc_policy.h; tests/policy_hook_tests.c; tests/daily_life_tests.py.

## L2. Gossip and requests

**Status:** done; verdict: null. **Actors:** villager, scribe, courier.

**Decision.** What to tell, how to retell it, whom to believe, and which personal request to make or answer.

**Sees:**
- held accounts with source, certainty and age
- relationship and trust
- own occupation and needs (bread, wheat, iron, wood, repairs)
- who is present

**Chooses among:**
- share, withhold or distort an account (retellings change counts, people, directions or motives)
- report, keep confidence, pledge help or listen (four responses)
- ask for or fulfil an item request

**Judged by:**
- accuracy of what the town believes about real events
- trust and relationship change
- requests fulfilled
- rumour reach

**Interface (ready).** Hook at the point a carrier shares a story with a town (share or withhold). Carriers include couriers and carriages, not only characters.

**Unattended, per world:** RUMOR_SHARED 2288.2; LORE_RECORDED 8.3; LORE_LOST 0.5; RELATIONSHIP_CHANGED 11.8; RELATIONSHIP_HISTORY 3.0.

**Commands:** `CC_COMMAND_EXCHANGE_GOSSIP`, `CC_COMMAND_HEARD_STORY`, `CC_COMMAND_CHARACTER_RESPONSE`, `CC_COMMAND_PERSONAL_WANT`.

**Structures:** `CcCharacterKnowledge`, `CcCharacterMemory`, `CcRelationship`.

**Model size.** 5M; the typed fact-selection pilot already targets this.

**First experiment.** Done: hook, informedness metric (coverage times accuracy), a threshold sweep and an evolution run.

**Gate to advance.** Not reached: no policy raises accuracy at equal reach. The rule sits on the frontier. Personal requests and the player-facing responses are not covered.

**Risks:**
- Fluency is not truth: the validator must own what may be said.
- Scoring accuracy needs a ground-truth event log per world.

**Evidence:** tools/dialogue/LIFE.md.

## L3. Trade, shipments and production

**Status:** done; verdict: small gain. **Actors:** settlement steward, royal carriage, shipper.

**Decision.** Which good to move, where, how much, and what to produce; how a carriage picks its next job.

**Sees:**
- stock, reserve target and price by settlement and good
- route capacity, toll and danger
- carriage mode and cargo
- shortages elsewhere

**Chooses among:**
- ship, hold or fund grain supply
- carriage mode: idle, repositioning, delivering, blocked, waiting for capacity, road repair
- route choice (CcTradeFindPath)

**Judged by:**
- shortage events avoided (19 per world)
- shipments lost (17 per world) and delivered (127)
- price stability
- reserve kept above target

**Interface (ready).** Hook at the royal carriage trade planner: it offers every legal (good, source, destination) the rule scored above zero, defaulting to the rule's argmax.

**Unattended, per world:** SHIPMENT_DEPARTED 158.0; SHIPMENT_ARRIVED 127.2; SHIPMENT_LOST 17.1; ROYAL_CARRIAGE_REROUTED 185.8; ROYAL_CARRIAGE_BLOCKED 16.0; SHORTAGE 19.2; SMITH_PRODUCTION 120.0; PAPER_MILLED 98.4; WOODLOT_HARVEST 47.1; BAKERY_PRODUCTION 33.9; MASONRY_REPAIR 36.2; QUARRY_OUTPUT 26.0; IRON_LEDGER_LOAN 4.3; IRON_LEDGER_REPAID 28.0; HARVEST_FAILED 1.0; ROUTE_CLOSED 1.0; ROUTE_REPAIRED 0.6.

**Commands:** `CC_COMMAND_TRADE`, `CC_COMMAND_TRADE_SUPPLY`, `CC_COMMAND_FUND_GRAIN_SUPPLY`, `CC_COMMAND_SUPPORT_BAKERY`, `CC_COMMAND_REPAIR_ROUTE`, `CC_COMMAND_SET_ROAD_LINE`.

**Structures:** `CcShipment`, `CcRoyalCarriage`, `CcSettlement`, `CcRoute`.

**Model size.** 1M to 5M; small state, many repeated decisions.

**First experiment.** Done: hook, town hunger and famine metrics, a bias sweep and evolution runs.

**Gate to advance.** Reached, modestly: a paired gain on 400 held-out worlds (z = 3.6) with the rule's legality unchanged. One search, one seed; outcomes vary by 8 hunger points between worlds.

**Risks:**
- Economy tuning is delicate and player-facing; keep the rule as a fallback and shadow-test first.

**Evidence:** tools/dialogue/LIFE.md.

## L4. Raiders and monsters

**Status:** done; verdict: trade-off. **Actors:** bandit group, goblin faction, monster population.

**Decision.** Whether, when and where to raid; what to take; whether to pay or take tribute.

**Sees:**
- own members, supplies, coins, camp size
- target settlement stock, guards and route danger
- raid phase and days remaining
- goblin motive: hunger, equipment or dragon tribute

**Chooses among:**
- bandit raid phases: idle, scouting, mustering, outbound, returning
- raid target, good and quantity
- goblin tribute phases: idle, outbound, returning, to dragon, preparing

**Judged by:**
- raider survival and take
- settlement harm and shortages caused
- raids completed per year
- balance: neither side collapses

**Interface (ready).** Hooks in the bandit raid launcher (raid or hold) and its choice of town. Goblin raids are a separate path and are not hooked.

**Unattended, per world:** BANDIT_PRESSURE 14.6; BANDIT_RAID_DEPARTED 12.4; BANDIT_RAID_RETURNED 12.2; SETTLEMENT_RAIDED 12.2; MONSTER_PRESSURE 13.8; GOBLIN_RAID_PREPARED 19.1; GOBLIN_RAID_DEPARTED 19.1; GOBLIN_RAIDED 19.0; GOBLIN_RAID_RETURNED 18.9; GOBLIN_TRIBUTE_DEPARTED 22.6; GOBLIN_TRIBUTE_DELIVERED 22.5; GOBLIN_CULT_RALLIED 13.8; GOBLIN_HOARD_DEFENDED 0.5.

**Commands:** `CC_COMMAND_GOBLIN_TRADE`, `CC_COMMAND_GOBLIN_WARN`, `CC_COMMAND_GOBLIN_INTERCEPT`, `CC_COMMAND_INTERCEPT_DRAGON_TRIBUTE`.

**Structures:** `CcBanditGroup`, `CcGoblinFaction`, `CcGoblinSociety`, `CcMonsterPopulation`.

**Model size.** 5M; two-sided, so raiders and defenders co-evolve.

**First experiment.** Done: hooks, a never-raid bound and an evolution run.

**Gate to advance.** Not reached as stated: the aim was raiders doing better without collapsing the towns. The evolved policy does better for raiders by taking more from towns. Which side to favour is a design choice; the hook and objective let either be tuned.

**Risks:**
- Adversarial: optimising raiders alone will strip the world; needs a two-sided or bounded objective.
- Unattended raids are rare per seed (about one bandit group per world).

**Evidence:** tools/dialogue/LIFE.md.

## L5. Kingdoms and war

**Status:** done; verdict: null. **Actors:** kingdom, faction, war-party commander.

**Decision.** What a kingdom does with its treasury and legitimacy, which factions to back, and how a war party is ordered.

**Sees:**
- treasury, iron-ledger debt, legitimacy, sanction
- diplomatic state (peace, war, alliance) with each neighbour
- war chest, supply and shortage
- party members, casualties, order

**Chooses among:**
- war orders: hold, march, control route, withdraw, cease
- fund the war chest, buy supply, back or shift a faction
- succession and pretender responses

**Judged by:**
- kingdom stability (legitimacy, treasury)
- wars avoided or won at low cost
- supply shortage days
- succession without crisis

**Interface (ready).** Hook at the kingdom's grain relief: any hungry town of the kingdom, or hold the treasury. War orders and succession are not hooked.

**Unattended, per world:** KINGDOM_ACTION 62.3; FACTION_SHIFT 78.0; WAR_CHEST_FUNDED 65.2; WAR_SUPPLY_BOUGHT 15.1; WAR_SUPPLY_SHORTAGE 4.6; INEQUALITY_PRESSURE 0.5; ROYAL_SUCCESSION 3.2; KING_ANOINTED 2.4; PRETENDER_CRISIS 1.3; MONASTIC_SUCCESSION 1.1; PEACE_DECLARED 0.3; SITUATION_CREATED 57.6; SITUATION_FAILED 55.5; FRONT_CREATED 26.8; FRONT_FAILED 25.4.

**Structures:** `CcKingdom`, `CcFaction`, `CcWarParty`, `CcSituation`, `CcFront`.

**Model size.** 5M to 10M; long horizons and sparse rewards.

**First experiment.** Done: hook, realm metrics (legitimacy, treasury, hunger, famine) and a two-stage evaluation.

**Gate to advance.** Not reached. War orders are untested and need an injected scenario.

**Risks:**
- Sparse and delayed reward; needs scenario injection to see enough wars.
- Story-critical: authored beats must not be optimised away.

**Evidence:** tools/dialogue/LIFE.md.

## L6. Dragons

**Status:** done; verdict: design axis. **Actors:** dragon, dragon cult, dragon campaign.

**Decision.** Hunt, retaliate, brood or lie dormant; whom to retaliate against; when to campaign.

**Sees:**
- life stage, body condition, crown strength, memory integrity
- territory stability, regional influence, hoard
- stolen treasure and theft actor
- kingdom pledges and alliances

**Chooses among:**
- activity: dormant, hunting, retaliating, brooding, aftermath
- retaliation target
- campaign phase: idle, outbound, returning

**Judged by:**
- territory stability and crown continuity
- hoard kept
- lineage: brood survives
- story pacing: an omen and a reckoning that make sense

**Interface (partial).** Hook at the dragon's choice of town to burn, plus an injected-theft harness (dragons retaliate only 0.4 times per world unattended). Hunting, brooding and campaigns are not hooked.

**Unattended, per world:** DRAGON_CROWNED 1.0; DRAGON_OMEN 0.5; DRAGON_RETALIATION 0.4; DRAGON_HOARD_STOLEN 0.5; DRAGON_TREASURE_RETURNED 0.4; DRAGON_HUNT (never fires unattended); DRAGON_BROOD (never fires unattended); DRAGON_BATTLE (never fires unattended); DRAGON_MUSTERED (never fires unattended).

**Commands:** `CC_COMMAND_STEAL_DRAGON_HOARD`, `CC_COMMAND_RETURN_DRAGON_TREASURE`, `CC_COMMAND_DELIVER_PROPHECY`.

**Structures:** `CcDragon`, `CcDragonCampaign`, `CcDragonCult`.

**Model size.** Start at 5M; scale to 10M-50M only if the option space and reward become rich. The one measured 5M-to-50M comparison (byte language modelling) gained 11% for ten times the parameters.

**First experiment.** Done: injected 300-crown thefts and forced each candidate town over 100 worlds.

**Gate to advance.** Half reached: paired scenario outcomes are measured; the authored-story review (does an omen and a reckoning still make sense?) is not done.

**Risks:**
- Rarity: little natural data, so scenario design carries the result.
- Highest player impact; keep the validator gate and a safe default action.

**Evidence:** tools/dialogue/LIFE.md; tools/dialogue/dragon_probe.py.

## L7. Lineages and open-ended evolution

**Status:** done; verdict: weak. **Actors:** all villagers over generations.

**Decision.** How inherited traits bias the shared brain across generations.

**Sees:**
- the L1-L2 inputs plus a small inherited trait vector

**Chooses among:**
- the same options as the stage each individual acts in

**Judged by:**
- lineage survival and prosperity
- diversity of behaviour across the population
- no collapse into one strategy

**Interface (partial).** Per-person trait rows (rich, cheap, near, home towns) bias the shared brain's travel scores; successors inherit with mutation. Successions in this sim are replacements in the same slot, born as age-0 children, and nothing in the sim selects on traits, so selection is added by the harness.

**Unattended, per world:** CHARACTER_BORN 0.5; CHARACTER_DIED 0.5.

**Structures:** `CcCharacter`.

**Model size.** One shared brain plus a per-person trait vector.

**First experiment.** Done: lineage.py with three conditions (none, drift, selection) over 12 accelerated years and, for comparison, 300 and 1000 natural years.

**Gate to advance.** Weakly reached: diversity sustained without loss of welfare. It does not show that selection helps.

**Risks:**
- Unattended worlds lose every road within about 20 years, so anything about travel is only meaningful for the first 10 to 15 years; a 1,000-year run had 13 generations but no travel after year 50, and so no selection.
- Successors are born as children who cannot travel for 16 years; accelerated generations seat them as adults.
- Traits have a small effect on outcomes; a richer fitness (scarcity, competition) is needed for open-ended evolution.

**Evidence:** tools/dialogue/LIFE.md; tools/dialogue/lineage.py.

## Shared tracks

| Track | Status | Goal | Have | Need |
| --- | --- | --- | --- | --- |
| T1 Policy hook | partial | One interface for every stage: observation, legal options, apply, outcome, with the rule as the default. | CcSimSetPolicy decision hook at 10 sites with a hash-invariance test; in-process food-relief and daily-life worlds (about 28,000 simulated days per second); zero weights reproduce the rule | the hook is process-wide and single-threaded; goblin raids, war orders, hunting, brooding, personal requests |
| T2 Evaluation | partial | Judge on outcomes, on held-out seeds, paired against the current rule. | paired comparisons with standard errors; held-out worlds; a movement invariant that caught a reward hack; nulls reported as nulls | one-input sensitivity tests; held-out regions of state; story review for dragons and kingdoms |
| T3 Determinism | planned | Identical choices on arm64, x86-64 and WebAssembly. | byte-exact native parity checks against the float path | an integer forward pass (RoPE, SiLU table, head_dim 32); a cross-platform parity test |
| T4 Runtime and sizes | partial | Load several small models by role. | C trainer; native runtime for one 5M layout | role-keyed model loading; exporter limits above dim 256 and 16 layers for larger models |
| T5 Reward hygiene | partial | Stop optimisers exploiting the score. | paired comparisons; conservation by construction (only legal options); the movement invariant | adversarial probes per stage; a KL anchor to the rule when learning from outcomes |
