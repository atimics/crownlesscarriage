# NPC training curriculum

Generated from `tools/dialogue/curriculum.json` and `docs/npc-census.json` by `tools/dialogue/render_curriculum.py`. Edit the JSON, not this file.

Census: 60 unattended worlds of 730 days each (`tools/sim_census.c`, `tools/dialogue/sim_census.py`). Per world: 96 people, 6 settlements, 3 kingdoms, 4019 events; 65 of the event kinds never fire without a player.

## Principles

- Order stages by how often the situation arises unattended, how clearly the decision is defined, how measurable the outcome is, and how small the interface change is.
- Every stage keeps the current rule as the baseline and the fallback, and advances only on a paired gain on held-out seeds.
- One shared network per role family; individuals differ through inputs.
- The validator owns legality; the network only chooses among legal options.

## Stages

| Stage | Name | Status | Interface | Depends on | Model |
| --- | --- | --- | --- | --- | --- |
| L0 | Food-relief exchange | done | ready | - | 5M policy |
| L1 | Villager daily life | next | missing | L0 | 5M shared across all villagers, with role, occupation and goal as inputs |
| L2 | Gossip and requests | planned | partial | L0 | 5M |
| L3 | Trade, shipments and production | planned | missing | L1 | 1M to 5M |
| L4 | Raiders and monsters | planned | missing | L3 | 5M |
| L5 | Kingdoms and war | planned | missing | L3, L4 | 5M to 10M |
| L6 | Dragons | planned | missing | L4, L5 | Start at 5M |
| L7 | Lineages and open-ended evolution | later | missing | L1, L2 | One shared brain plus a per-person trait vector |

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

**Status:** next. **Actors:** villager (any role).

**Decision.** What to do today: work, seek aid, buy a meal, lodge, travel, hide, or join a bandit group.

**Sees:**
- role, occupation, goal, activity
- purse, hunger days, unsheltered nights, stress
- local prices, stock and services (inn)
- known routes and their danger

**Chooses among:**
- activity: WORKING, SEEKING_AID, PREPARING, RECOVERING, HIDING, TRAVELLING
- buy the cheapest meal, or go without
- pay for a bed
- accept casual work (6 crowns a day where the market can pay)

**Judged by:**
- days hungry and days unsheltered
- survival and purse over 90 days
- recruitment into bandit groups (17.7 people per world are hiding)
- stress

**Interface (missing).** Read from source: AdvanceTravellerNeeds in cc_sim.c makes these choices by fixed rule each day for travellers and refugees; residents at home have hunger and shelter reset. Needs a daily-decision hook with a legal-option list.

**Unattended, per world:** SHORTAGE 19.2; BANDIT_PRESSURE 14.6; CHARACTER_INTERACTION 11.6.

**Commands:** `CC_COMMAND_PERSONAL_WANT`, `CC_COMMAND_CHARACTER_RESPONSE`.

**Structures:** `CcCharacter`, `CcSettlement`.

**Model size.** 5M shared across all villagers, with role, occupation and goal as inputs.

**First experiment.** Hook the traveller-needs step, offer meal/bed/work/seek-aid/wait as candidates, and evolve the shared scorer on 90-day welfare across 60 seeds; compare with the fixed rule on held-out seeds.

**Gate to advance.** Paired welfare gain over the rule on held-out seeds (z >= 3), with no rise in bandit recruitment and money conserved.

**Risks:**
- Only travellers and refugees (about 34 people per world) face this choice; residents are reset to fed each day.
- A shared brain can homogenise behaviour; add goal and trait inputs early.

**Evidence:** src/sim/cc_sim.c AdvanceTravellerNeeds; docs/npc-census.json activity_per_world.

## L2. Gossip and requests

**Status:** planned. **Actors:** villager, scribe, courier.

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

**Interface (partial).** RUMOR_SHARED is the most frequent event (2,288 per world in two years); the exchange command exists but I have not read how unattended exchanges are generated.

**Unattended, per world:** RUMOR_SHARED 2288.2; LORE_RECORDED 8.3; LORE_LOST 0.5; RELATIONSHIP_CHANGED 11.8; RELATIONSHIP_HISTORY 3.0.

**Commands:** `CC_COMMAND_EXCHANGE_GOSSIP`, `CC_COMMAND_HEARD_STORY`, `CC_COMMAND_CHARACTER_RESPONSE`, `CC_COMMAND_PERSONAL_WANT`.

**Structures:** `CcCharacterKnowledge`, `CcCharacterMemory`, `CcRelationship`.

**Model size.** 5M; the typed fact-selection pilot already targets this.

**First experiment.** Score belief accuracy against the event log after a season of gossip under the rule versus a learned share/withhold policy.

**Gate to advance.** Higher belief accuracy at equal reach, and no fabricated facts (validator rejects any account not held).

**Risks:**
- Fluency is not truth: the validator must own what may be said.
- Scoring accuracy needs a ground-truth event log per world.

**Evidence:** docs/npc-society.md; docs/personal-requests.md; tools/dialogue/FACT-SELECTION.md.

## L3. Trade, shipments and production

**Status:** planned. **Actors:** settlement steward, royal carriage, shipper.

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

**Interface (missing).** Shipments and carriages run by rule in the daily tick (158 departures per world). The trade commands are player-scoped; there is no steward-side option list yet.

**Unattended, per world:** SHIPMENT_DEPARTED 158.0; SHIPMENT_ARRIVED 127.2; SHIPMENT_LOST 17.1; ROYAL_CARRIAGE_REROUTED 185.8; ROYAL_CARRIAGE_BLOCKED 16.0; SHORTAGE 19.2; SMITH_PRODUCTION 120.0; PAPER_MILLED 98.4; WOODLOT_HARVEST 47.1; BAKERY_PRODUCTION 33.9; MASONRY_REPAIR 36.2; QUARRY_OUTPUT 26.0; IRON_LEDGER_LOAN 4.3; IRON_LEDGER_REPAID 28.0; HARVEST_FAILED 1.0; ROUTE_CLOSED 1.0; ROUTE_REPAIRED 0.6.

**Commands:** `CC_COMMAND_TRADE`, `CC_COMMAND_TRADE_SUPPLY`, `CC_COMMAND_FUND_GRAIN_SUPPLY`, `CC_COMMAND_SUPPORT_BAKERY`, `CC_COMMAND_REPAIR_ROUTE`, `CC_COMMAND_SET_ROAD_LINE`.

**Structures:** `CcShipment`, `CcRoyalCarriage`, `CcSettlement`, `CcRoute`.

**Model size.** 1M to 5M; small state, many repeated decisions.

**First experiment.** Expose the shipment planner as options (destination, good, quantity) for one settlement and score shortages and losses over two years.

**Gate to advance.** Fewer shortage and lost-shipment events at held-out seeds, with treasury and stock conserved.

**Risks:**
- Economy tuning is delicate and player-facing; keep the rule as a fallback and shadow-test first.

**Evidence:** src/sim/cc_supply_trade.inc; src/sim/cc_trade_path.c; src/sim/cc_food_economy.c; docs/crown-carriage-roads.md.

## L4. Raiders and monsters

**Status:** planned. **Actors:** bandit group, goblin faction, monster population.

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

**Interface (missing).** Raids run by rule in the daily tick (12 bandit and 19 goblin raids per world); raid_target_id, raid_good and raid_quantity are already state fields, so an option list can be built.

**Unattended, per world:** BANDIT_PRESSURE 14.6; BANDIT_RAID_DEPARTED 12.4; BANDIT_RAID_RETURNED 12.2; SETTLEMENT_RAIDED 12.2; MONSTER_PRESSURE 13.8; GOBLIN_RAID_PREPARED 19.1; GOBLIN_RAID_DEPARTED 19.1; GOBLIN_RAIDED 19.0; GOBLIN_RAID_RETURNED 18.9; GOBLIN_TRIBUTE_DEPARTED 22.6; GOBLIN_TRIBUTE_DELIVERED 22.5; GOBLIN_CULT_RALLIED 13.8; GOBLIN_HOARD_DEFENDED 0.5.

**Commands:** `CC_COMMAND_GOBLIN_TRADE`, `CC_COMMAND_GOBLIN_WARN`, `CC_COMMAND_GOBLIN_INTERCEPT`, `CC_COMMAND_INTERCEPT_DRAGON_TRIBUTE`.

**Structures:** `CcBanditGroup`, `CcGoblinFaction`, `CcGoblinSociety`, `CcMonsterPopulation`.

**Model size.** 5M; two-sided, so raiders and defenders co-evolve.

**First experiment.** Give bandit groups a raid-target choice and evolve it against settlement defence, scoring both sides.

**Gate to advance.** Raiders do better than the rule without collapsing the settlements they raid (both sides in band across seeds).

**Risks:**
- Adversarial: optimising raiders alone will strip the world; needs a two-sided or bounded objective.
- Unattended raids are rare per seed (about one bandit group per world).

**Evidence:** src/sim/cc_goblin_politics.inc StoreGoblinRaid AdvanceGoblinPolitics HuntGoblinFaction.

## L5. Kingdoms and war

**Status:** planned. **Actors:** kingdom, faction, war-party commander.

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

**Interface (missing).** KINGDOM_ACTION (62 per world), FACTION_SHIFT (78) and WAR_CHEST_FUNDED (65) fire by rule. WAR_DECLARED never fired in 60 unattended worlds of two years, so war scenarios must be injected.

**Unattended, per world:** KINGDOM_ACTION 62.3; FACTION_SHIFT 78.0; WAR_CHEST_FUNDED 65.2; WAR_SUPPLY_BOUGHT 15.1; WAR_SUPPLY_SHORTAGE 4.6; INEQUALITY_PRESSURE 0.5; ROYAL_SUCCESSION 3.2; KING_ANOINTED 2.4; PRETENDER_CRISIS 1.3; MONASTIC_SUCCESSION 1.1; PEACE_DECLARED 0.3; SITUATION_CREATED 57.6; SITUATION_FAILED 55.5; FRONT_CREATED 26.8; FRONT_FAILED 25.4.

**Structures:** `CcKingdom`, `CcFaction`, `CcWarParty`, `CcSituation`, `CcFront`.

**Model size.** 5M to 10M; long horizons and sparse rewards.

**First experiment.** Learn the kingdom-action choice on the events that do fire, scored by legitimacy and treasury a year later; add war scenarios by seeding a border dispute.

**Gate to advance.** Better stability than the rule on held-out seeds, and wars still occur at a similar rate (no pacifist collapse of the story).

**Risks:**
- Sparse and delayed reward; needs scenario injection to see enough wars.
- Story-critical: authored beats must not be optimised away.

**Evidence:** src/sim/cc_war.c MarchToward ResolveBattles PartiesHostile; docs/npc-census.json.

## L6. Dragons

**Status:** planned. **Actors:** dragon, dragon cult, dragon campaign.

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

**Interface (missing).** One dragon per world; DRAGON_CROWNED fires about once and DRAGON_RETALIATION 0.4 times per two years. The hoard is stolen unattended at 0.5 per world, the same rate as goblin hoard heists, so I expect retaliation to follow them (trigger code not read). Most dragon events never fire unattended (hunt, battle, brood, mustering), so scenarios must be injected.

**Unattended, per world:** DRAGON_CROWNED 1.0; DRAGON_OMEN 0.5; DRAGON_RETALIATION 0.4; DRAGON_HOARD_STOLEN 0.5; DRAGON_TREASURE_RETURNED 0.4; DRAGON_HUNT (never fires unattended); DRAGON_BROOD (never fires unattended); DRAGON_BATTLE (never fires unattended); DRAGON_MUSTERED (never fires unattended).

**Commands:** `CC_COMMAND_STEAL_DRAGON_HOARD`, `CC_COMMAND_RETURN_DRAGON_TREASURE`, `CC_COMMAND_DELIVER_PROPHECY`.

**Structures:** `CcDragon`, `CcDragonCampaign`, `CcDragonCult`.

**Model size.** Start at 5M; scale to 10M-50M only if the option space and reward become rich. The one measured 5M-to-50M comparison (byte language modelling) gained 11% for ten times the parameters.

**First experiment.** Build accelerated scenarios (a theft, a rival campaign) and score territory stability and crown continuity over 20 years.

**Gate to advance.** Dragon behaviour judged by paired scenario outcomes and authored-story review, not by agreement with the rule.

**Risks:**
- Rarity: little natural data, so scenario design carries the result.
- Highest player impact; keep the validator gate and a safe default action.

**Evidence:** src/sim/cc_sim.h CcDragon, CcDragonCampaign; docs/npc-census.json silent_event_kinds.

## L7. Lineages and open-ended evolution

**Status:** later. **Actors:** all villagers over generations.

**Decision.** How inherited traits bias the shared brain across generations.

**Sees:**
- the L1-L2 inputs plus a small inherited trait vector

**Chooses among:**
- the same options as the stage each individual acts in

**Judged by:**
- lineage survival and prosperity
- diversity of behaviour across the population
- no collapse into one strategy

**Interface (missing).** Births and deaths are rare (0.5 each per world in two years), so this needs centuries per seed; at about 8,000 simulated days per second a century takes a few seconds.

**Unattended, per world:** CHARACTER_BORN 0.5; CHARACTER_DIED 0.5.

**Structures:** `CcCharacter`.

**Model size.** One shared brain plus a per-person trait vector.

**First experiment.** Add a mutating trait vector at birth and measure behaviour diversity and lineage outcomes over 300 simulated years.

**Gate to advance.** Sustained diversity without loss of average welfare on held-out seeds.

**Risks:**
- Emergent exploits; the evolved population can drift away from the intended story.

**Evidence:** docs/npc-census.json.

## Shared tracks

| Track | Status | Goal | Have | Need |
| --- | --- | --- | --- | --- |
| T1 Policy hook | partial | One interface for every stage: observation, legal options, apply, outcome, with the rule as the default. | legal-candidate lists and validators for food relief; in-process food-relief worlds | a generic hook in the daily tick; the in-process world generalised beyond food relief |
| T2 Evaluation | partial | Judge on outcomes, on held-out seeds, paired against the current rule. | outcome scorer; paired comparison with standard errors; held-out crowd slices | one-input sensitivity tests; held-out regions of state; per-stage invariants (conservation, legality) |
| T3 Determinism | planned | Identical choices on arm64, x86-64 and WebAssembly. | byte-exact native parity checks against the float path | an integer forward pass (RoPE, SiLU table, head_dim 32); a cross-platform parity test |
| T4 Runtime and sizes | partial | Load several small models by role. | C trainer; native runtime for one 5M layout | role-keyed model loading; exporter limits above dim 256 and 16 layers for larger models |
| T5 Reward hygiene | planned | Stop optimisers exploiting the score. | paired comparisons; conservation checks | adversarial probes per stage; a KL anchor to the rule when learning from outcomes |
