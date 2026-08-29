# The OSR Release Cut: Audit, Loot, and a Viable Release

This document is the ruthless simplification pass. It audits what the code
actually implements against what the manual promises, cuts the design to a
shippable loop, specifies D&D/OSR-style loot roll tables that respect the
material economy, and defines the first viable release.

It amends [12-story-quests-and-narration.md](12-story-quests-and-narration.md):
fronts, curiosities, and the narration contract are **deferred**, not
cancelled. The Empty Granary situation chain *is* the first front; it needs
no new system to ship.

## Part 1 — Aspirations vs. code

Audited against `src/sim`, `src/metagame`, `src/client`, and the test suite
on `main`. Core health: **24/24 tests pass**, including save/load round-trip,
deterministic replay, long-run OSR balance, and causal history retention.

### What actually works (keep — this is the game)

| System | Code evidence |
| --- | --- |
| Deterministic kernel, calendar, hashing | `cc_sim.c`, `osr_long_run_balance`, `sqlite_round_trip` tests |
| Material economy: goods, treasuries, war chests, Iron Ledger | `CcMoney` ledger fields, explicit sources/sinks |
| 5 situation kinds: relief, route repair, monster expedition, black market, courier | `CcSituationKind` |
| Bandit raids, goblin tribute chains, dragon Crown Cycle + debt/omens/fire | `dragon_cycle_tests`, `dragon_ecology_tests` |
| Couriers, war, alliance vs dragon | `war` verbs, `causal_history_tests` |
| ~35-verb text metagame: look/causes/people/rumors/charters/maps/treasures/travel/road/dungeon/debrief/save | `cc_metagame.c` |
| SQLite append-only journal with checkpoints and replay | `cc_save.c`, `persistence_tests` |
| 3D waystation client with climbing, combat, market interior | `cc_local3d.c` |

### What the manual promises and the code does not have

| Aspiration | Reality |
| --- | --- |
| Passengers, crew, modules, manifests | Not implemented. Cargo goods only. |
| **Loot / combat rewards** | **Nothing.** Combat costs carriage damage + medical crowns and yields zero. The only treasure acquisition is dragon-steal commands. |
| Monster ecology, migrations | One `CcMonsterPopulation` pressure number per region. |
| Dungeons as spaces | One `dungeon` state-change command. No expedition play. |
| City grammars, interiors, recurring cast | One market interior, NPC appearance system. |
| Fronts, clocks, curiosities, narration, chronicle | Not implemented (docs only — including doc 12). |
| Named-character promotion, memory | Sponsor/affected names on situations; no persistent cast. |

### What is actually broken

1. **The root `build/` directory is stale and fails** (raylib include path,
   corrupted parallel build state). The preset build in `out/build/` works.
   Delete `build/`.
2. **Long-lived feature branches drift.** `codex/terrain-level-design` is
   many merges behind `main`. Branches must stay short-lived.
3. **Documentation velocity exceeds implementation velocity.** The manual
   now specifies roughly ten years of work. This document is the correction.

## Part 2 — The ruthless cut

### Keep (the release is made of these)

- The deterministic kernel, journal, and replay — the project's real asset.
- The material economy exactly as scoped: Food, Iron, Tools, Weapons, Gold,
  Gems, named treasures, cargo slots.
- The five situation kinds. No new kinds for release.
- The dragon debt chain (theft → omens → fire → restitution). It is the
  signature system and it is *done*.
- The text metagame as the primary release surface.
- The 3D client rides along as the **demo reel**, not a release gate.

### Cut (deferred, one-line reason each)

| Cut | Why |
| --- | --- |
| Fronts as a system | The Granary chain already plays as one. Ship it, then generalize. |
| Curiosities | Max one, post-release. Authored content is expensive. |
| LLM narration, chronicle | Needs the packet extractor; build after release. |
| Passengers/crew/modules | Cargo slots already force the manifest dilemma. |
| New city grammars, interiors | One market is enough for the loop. |
| Monster ecology depth | One pressure number already feeds situations. |
| Persistent dungeon spaces | Keep the state machine; drop spatial expedition. |
| Named-character memory system | Sponsor names carry enough identity for now. |
| Refactor `cc_local3d.c` (18.6k lines) | Demo surface; frozen for release. |

### Cut rule going forward

A system enters the release only if it touches the journey loop below.
Everything else waits. The roadmap's Stage 8 discipline applies *early*.

## Part 3 — Loot roll tables

OSR principles, adapted to the material economy:

1. **Loot is carried, never minted.** You may only take what the defeated
   party physically holds. The ledger already tracks: bandit supplies and
   carried raid loot, goblin carried tribute and lair stores, raider carried
   loot, dragon hoard. A loot roll moves existing inventory to the player,
   with provenance, or it does not happen.
2. **The ledger decides what exists; the table decides what you recover.**
   Roll *after* resolving the encounter, against the defeated party's actual
   carried value.
3. **Loot occupies carriage slots.** Winning is a logistics problem; the
   OSR encumbrance dilemma already exists as cargo slots.
4. **Wealth is XP, banked on delivery.** Carriage progression advances only
   when loot is sold or banked at a settlement market. This defeats
   save-scumming and forces the return journey — the loop's second half.

### Encounter loot table (2d6, rolled on victory)

| 2d6 | Result |
| --- | --- |
| 2–3 | Driven off. Recover nothing. The carriage already paid the damage cost. |
| 4–5 | Recover a fraction of the party's carried supplies: roll 1d6 × 10%, rounded down. |
| 6–8 | Recover half the party's carried supplies (Food/Tools/Weapons by their real mix). |
| 9–10 | Recover all carried supplies **and** 1d6 × 10% of carried coin. |
| 11 | All carried supplies and coin, plus their map/chart if any (goes to the map case). |
| 12 | All of the above, plus one *trophy*: a named item with provenance, sellable as a curio at any market for 2d6 crowns. |

Negotiated passage (the existing bargain) never rolls loot — you paid
instead. Refusal never rolls. **Fighting is the only path to loot**, which
finally makes combat a real alternative to the bargain instead of the
strictly worse option it is today.

### Trophies (the first magic items, done cheaply)

A trophy is a generated named object: `trophy of the <battle name>`, taken
from `<defeated party leader>`, `<place>`, `<day>`. It uses the existing
named-treasure schema and occupies one slot. No powers for release. Later,
trophies become curiosities.

### Camp and cache table (1d6, after breaking a camp or ending a raid chain)

| 1d6 | Cache contents (from the camp's real stores) |
| --- | --- |
| 1 | Nothing recoverable. |
| 2 | 1d6 Food. |
| 3 | 1d3 Tools. |
| 4 | 1d3 Weapons. |
| 5 | 2d6 crowns from the camp's coin. |
| 6 | One named offering the camp had prepared (goblin camps: dragon tribute; bandit camps: a hostage's goods) — sellable, or returnable for reputation. |

### Hoard objects

Dragon hoard looting already exists (`dragon steal-treasure`) and stays
exactly as designed: named objects, one slot each, deeper memory wounds than
coin, no replacement-money rule. It is the endgame loot table, already built.

### Accounting rules

- Every table result moves real inventory with a journaled source; nothing is
  minted.
- Loot respects cargo slot capacity at pickup — the player may leave loot.
- All rolls consume the deterministic RNG stream; replays reproduce them.
- Banked wealth (crowns sold at a market) is the only progression currency.
  Upgrade costs: reinforced axle (30), armed escort berth (60), second cargo
  bay (120), charter reputation tier (200).

## Part 4 — The viable release

**Title:** *Crownless Carriage: The Empty Granary*.
**Surface:** the text metagame (plus the 3D client as an attract-mode demo).
**Pitch:** one playable season of an OSR carriage economy — witness the
famine, choose charters, manifest cargo, run roads, fight or bargain, haul
loot home, bank it, upgrade the carriage, and face the dragon's debt.

### The loop (already 80% built)

```
witness crisis → accept charter → manifest cargo → journey
   → crisis on the road → resolve (fight = loot roll / bargain / avoid)
   → deliver → bank wealth → carriage upgrade → bigger charter → repeat
```

### Release additions (all small, all kernel-shaped)

1. **Loot tables** (Part 3): one new event kind, three roll sites, trophies.
2. **Carriage progression:** four upgrades, priced in banked crowns.
3. **Campaign end states** for the Granary season, evaluated on day 90:
   famine averted / survived / catastrophic, plus the kingdom-level outcomes
   (bridge, night road, dungeon tunnel) already simulated.
4. **Debrief polish:** the `debrief` verb becomes the ending screen.
5. **Housekeeping:** delete the stale `build/` directory, rebase
   `codex/terrain-level-design` onto `main`, fix the test-target raylib
   include path.

### Exit criteria

- A new player reaches a campaign end state in 2–4 hours without reading
  the manual.
- All four interventions from the vertical slice doc are completable
  start-to-debrief.
- Combat, bargain, and refusal each produce measurably different end states.
- Determinism, replay, and save/load tests still pass with loot enabled.
- One full campaign journal replays bit-identically.

### Explicitly not in this release

Multi-campaign worlds, new situation kinds, monsters beyond pressure
numbers, spatial dungeons, passengers, narration, fronts as a system,
curiosities.

## Part 5 — After release, in order

1. **Narration packet extractor** (pure code, testable, feeds LLMs later).
2. **Second settlement grammar** + named-character persistence.
3. **Fronts** generalized from two shipped campaigns' worth of situation data.
4. **Trophies become curiosities** (first authored bindings).
5. **Spatial dungeon** for the mine, with expedition cargo.
