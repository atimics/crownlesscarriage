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

Actors: townsfolk by trade (baker, butcher, candlestick maker), town guards, bandits, carriages and
caravans, three goblin factions, dragons. Places: towns, roads, goblin camps, tunnels, the hoard.
Goods: food, tools, wax, crowns, treasures, tomes.

Not in scope: rendering, dialogue text, the royal court, kingdoms, the full event catalogue of
the current sim. sim2 does not try to match the old sim line by line. It has to reproduce the
same kinds of behaviour at a similar size: ambushes, smuggling chains, famine, a dragon that
burns towns.

## Rules of the world

- **Bandits waylay carriages, not towns.** They are recruited from hungry townsfolk.
- **Goblins** buy, beg, borrow or steal crowns, treasures and tomes, move them camp to camp in
  caravans, and dig tunnels toward the dragon hoard. Three factions.
- **The dragon hoard is a sink** sized to the money supply. A dragon with no food raids.
- **Guards** are opposed to bandits and paid from a tax on trade.

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

One interface: `decide(role, observation, options) -> index` over a fixed-size masked option list.
A rule and a net answer the same slots (`research/04`). A decision is requested only when a goal
ends, is invalidated, or a need crosses a threshold. Observations are a needs vector weighted by
personality plus beliefs. One brain per role, never shared across roles. Brains can be batched per
role per tick for net inference.

## Economy and ecology (starting values, to be tuned; see `research/05`)

- Price = base x clamp((target / stock)^0.5, 0.5, 3), per good per town.
- Resource nodes: (amount, max, regrow).
- Agents burn energy each tick, die at 0, reproduce only above a threshold, and split energy with
  the child. This gives carrying capacity with no tuned cap.
- Buy/sell spread 140%/60%, to limit arbitrage. Food is the master good.
- Bandit join: hunger minus risk-aversion x P(caught) above a threshold, with
  P(caught) = 1 - exp(-k x guards / bandits). Attack if P(win) x loot covers the expected loss.
  Do not respawn killed bandits instantly. Scale recruitment by recent losses.
- Hoard intake is proportional to (circulating money - target).

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

## Open decisions (need an answer before code)

1. Tick length and trade-cycle length. Suggestion: one tick = one hour, ticks per day = 24,
   trade cycle = a few days.
2. Who mints crowns? Suggestion: a mint at each town, tied to the tax, plus the hoard as the sink.
3. Can bandits return to town life? Suggestion: yes, after a quiet period. Otherwise the
   bandit pool only grows.
4. Do prices spread only through caravans? Suggestion: yes. It makes caravans and ambushes matter.
5. Scripted fraction per role. No source gives a number, so we measure it in the baseline.
6. Reproduction: mating, or copy-with-mutation of a brain? Suggestion: start with copy-with-mutation.
7. Fitness of an extinct role: suggestion is that extinction scores the worst and the run ends.

## Risks

- Second-system effect. Mitigation: hard scope above, and a baseline test before any brain work.
- The cause log may be costly. Mitigation: fixed-size records and a measured budget.
- The ecology may not stabilise. Mitigation: the baseline test comes first and gates the rest.

## First build steps, if approved

1. Core: arena, pools, event queue, RNG, hash, clone, with cross-platform hash in CI.
2. Town economy only, rule brains: baker, butcher, candlestick maker, prices, famine.
3. Add bandits, guards and carriages. Run the baseline.
4. Add goblins, tunnels and the hoard. Re-run the baseline.
5. Add the dragon. Then the survival experiments.

Research notes with sources and unverified items are in `docs/sim2/research/`.
