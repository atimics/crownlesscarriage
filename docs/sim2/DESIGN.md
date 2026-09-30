# sim2: a small, clean world for training and testing NPC brains

Status: proposal, for review. Nothing is built yet.

## Why

The current simulation (`src/sim/cc_sim.c`, one file of about 15K lines) is the game. It grew by
patches, and a brain can only be attached at ten hook sites. Most of what matters is fixed rules
with no decision point: bandit camps, goblin tribute and hoard raids, the dragon's life cycle,
carriage ambushes. Training one decision against a hand-made score was brittle (see `LIFE.md`:
the L4 trade-off, the reward-hack guard). Co-evolution showed the town side has little to learn
when it is treated as one brain.

sim2 is a separate, small world built so that **a whole role can be a brain, and the world judges
it by whether it survives and reproduces.** The shipped sim stays as it is. sim2 may replace it
later, or never.

## Scope (hard stop)

The world is **entities and locations**, with deciding abstracted away. There are no role types.
An entity is a person, goblin, dragon, carriage or caravan: a bundle of needs, inventory, traits,
a location and an affiliation. A location is a town, camp, road, tunnel, mine, the capital or the
hoard. **The map is fixed** (the real Crownless map, exported as data from the current sim), so
geography decides how factions arise.

Everything an entity can do is a **verb** with legal-option rules and a deterministic outcome:
move, take, trade, make, fight, dig, join, leave, pay tribute, lend, repay. The rules list which
verbs are legal for an entity right now. The **decider** picks one. A "baker" is just an entity
that keeps choosing to make bread, and a "bandit" is one that keeps choosing to ambush carriages.
Roles are labels we put on the log afterwards, not types in the code.

Not in scope: rendering, dialogue text, the royal court, kingdoms, the full event catalogue of
the current sim. sim2 does not try to match the old sim line by line. It has to reproduce the same
kinds of behaviour at a similar size: ambushes, smuggling chains, famine, a dragon that burns towns.

## Rules of the world

- **Bandits emerge.** A hungry, poor entity can choose to join a band and waylay carriages. Towns
  are not raided.
- **Goblins** (three factions) buy, beg, borrow or steal crowns, treasures and tomes, move them
  camp to camp in caravans, and dig tunnels toward the dragon hoard.
- **Tribute selects goblin tribes.** The dragon kills the goblin tribes that brought it the least
  tribute. This is a fixed rule, not a decision, and it is the world's own selection pressure:
  it culls the worst, not at random.
- **Money.** Crowns are minted only in the capital, from silver ingots that come from Silverwick.
  Ingots are the faucet. The hoard is the sink. Money supply is therefore bounded by the mine and
  the road between Silverwick and the capital.
- **Credit.** Some towns have scribes who trade paper and promises recorded in tomes. A promise is
  an entity (issuer, holder, amount, due tick) that can be traded or defaulted on. Scribes come in a
  later phase, after the crown-only baseline is stable.
- **Guards** oppose bandits and are paid from a tax on trade. A guard is an entity that chose to
  stand watch.
- **Births and deaths are emergent.** An entity dies when its energy reaches 0. It reproduces when
  energy is above a threshold and a legal partner or site is present, splitting energy with the
  child. The child inherits traits with small mutations.

## Architecture

From `research/02-ecs-events.md`, with the sources' limits noted there.

- **Storage:** sparse-set style dense struct-of-arrays pools, one per component. Entity id is a
  24-bit index plus an 8-bit generation. Roles change often (a hungry townsperson becomes a
  bandit), and archetype tables make that move costly.
- **State is one fixed arena addressed by offsets, never pointers.** Clone is a `memcpy`. The
  event log is separate and append-only, and branches share the log prefix. This is what makes
  counterfactual replay cheap.
- **Time:** a coarse integer tick with a fixed phase order. Each actor has a `next_wake_tick`, so
  idle actors cost nothing. Events sit in one timer wheel, ordered by (tick, priority, event_seq)
  so ties are deterministic. Events are queued and drained per tick, not applied on emit.
- **Determinism:** integer and fixed-point state only. No libm or float in anything that affects
  a decision. One PCG32 stream per entity, seeded from hash(world_seed, id, generation), so adding
  an unrelated actor does not shift anyone else's draws. A 64-bit hash of the arena every N ticks
  goes through the same Linux, macOS and WASM check the current sim uses.
- **Parallelism:** one world per thread, many worlds in parallel. Structural changes go through
  command buffers applied in entity-id order.
- **Level of detail:** caravans, raids and camps act at group level. A band or caravan becomes
  full actors only when it meets something. Dwarf Fortress shows the cost of not doing this: unit
  turns took over 60% of frame time (`research/01`).
- **Caps everywhere:** cap perception radius and neighbour counts, and cap or decay every entity
  type (items, corpses, animals).

## Events and causes

Every state change goes through one `emit()`. Records are fixed-size:
`{seq, tick, type, subject, object, a, b, cause_seq, reason_tag}`. Only exogenous events (seeding,
a director nudge) may have no cause, and a test enforces that. Decisions also log the option
index, so a counterfactual can override exactly one. Fitness is a fold over the log.

Nobody in the projects surveyed logs a cause for every outcome, so this is our own work, and we
will measure what it costs.

Knowledge (`research/06`): ground truth is the log. Each actor has a separate belief store keyed
by event: owner, about-event, claim, strength, evidence, learned_tick. Gossip runs only when
actors meet. Brains see beliefs with age, strength and source, never the truth. Rumours can be
mutated from a small authored table. Crowds get coarse knowledge, not per-fact stores.

## Decisions and brains

One interface: `decide(species, observation, options) -> index` over a fixed-size masked option
list. A rule and a net answer the same slots (`research/04`). A decision is requested only when a
goal ends, is invalidated, or a need crosses a threshold.

**One decider per species** (human, goblin, dragon), not per role. Within a species, behaviour
differs only through the observation: needs, inventory, location, affiliation, inherited traits and
beliefs. This is the Neural MMO setup, and it is the "one brain with everyone's different contexts"
idea from earlier. It also removes the L4 problem of one model serving conflicting objectives
(raider against villager): all entities share one fitness, survival and reproduction, and the
conflict lives in the world.

Two timescales of evolution:
1. **Across worlds:** net weights are trained by evolution strategies against survival and
   reproduction read from the log.
2. **Within a world:** a small inherited trait vector (personality) mutates at birth, so lineages
   can drift and be selected. It feeds the observation.

The dragon starts as a structured rule. It can become a decider later.

The research warned against one net shared across fixed roles, since roles had conflicting
objectives. Roles are no longer fixed, so that warning applies differently. We will watch for one
net failing to serve very different entities, and split by species if needed.

## Economy and ecology (starting values, to be tuned; see `research/05`)

- Price = base x clamp((target / stock)^0.5, 0.5, 3), per good per town.
- Resource nodes: (amount, max, regrow).
- Agents burn energy each tick, die at 0, reproduce only above a threshold, and split energy with
  the child. This gives carrying capacity with no tuned cap.
- Buy/sell spread 140%/60%, to limit arbitrage. Food is the master good.
- Bandit join: hunger minus risk-aversion x P(caught) above a threshold, with
  P(caught) = 1 - exp(-k x guards / bandits). Attack if P(win) x loot covers the expected loss.
  Do not respawn killed bandits instantly. Scale recruitment by recent losses.
- Crowns enter only from the capital mint (silver ingots from Silverwick). Hoard intake is
  proportional to (circulating money - target), so the faucet and the sink stay in balance.

## Keeping the ecology alive while training (`research/04`)

No world resets. Cull the worst or oldest, not random. Train at deployment density. Regrowth
spreads from neighbours plus a little spontaneous spawn. Keep a founder floor and scripted
backstops per role, because extinction in a small population is absorbing.

## How we judge it

1. **Baseline:** rule brains alone keep the ecology alive for N years over 1000 seeds, with the
   causes of every death and birth on the log. If this fails, sim2 is too brittle to train in.
2. **Determinism:** the hash agrees on Linux, macOS and WASM.
3. **Speed:** world-years per hour per core, compared with the current sim.
4. **Plausibility:** same kinds of behaviour as the old sim at similar rates.
5. **Only then:** swap in one role's brain and ask whether the world survives, and whether the
   brain's lineage does.

## Decided

- Crowns are minted only in the capital from Silverwick silver ingots. Some towns have scribes who
  trade paper and promises.
- The map is fixed. Factions arise from geography. The dragon kills the goblin tribes that paid the
  least tribute.
- Human and goblin behaviour and reproduction are emergent. No role types. Deciding is abstracted
  into one interface, with structured rules around it.

## Open decisions

1. Tick length and trade-cycle length. Suggestion: one tick = one hour, 24 a day, a trade cycle of a
   few days.
2. One decider per species (human, goblin, dragon), or one for all? Suggestion: per species. The
   action sets differ.
3. Can an entity leave a band? Suggestion: yes, after a quiet period, or the bandit pool only grows.
4. Do prices spread only through caravans? Suggestion: yes.
5. Promises: model as tradable debt entities, added after the crown-only baseline. Agree?
6. What stays scripted as a backstop? Suggestion: the dragon's tribute rule and a founder floor per
   species. We measure the rest in the baseline.
7. Fitness of an extinct species: suggestion is that extinction scores worst and the run ends.

## Risks

- Second-system effect. Mitigation: hard scope above, and a baseline test before any brain work.
- The cause log may be costly. Mitigation: fixed-size records and a measured budget.
- The ecology may not stabilise. Mitigation: the baseline test comes first and gates the rest.

## First build steps, if approved

1. Core: arena, pools, event queue, RNG, hash, clone, with cross-platform hash in CI.
2. Locations, entities, verbs and a rule decider on the fixed map: food, prices, famine.
3. Add the mint, carriages and emergent bandits and guards. Run the baseline.
4. Add goblins, tunnels and the hoard. Re-run the baseline.
5. Add the dragon. Then the survival experiments.

Research notes with sources and unverified items are in `docs/sim2/research/`.
