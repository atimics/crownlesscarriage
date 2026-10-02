# 03 - Open-source MUDs, roguelikes and world sims

All repos cloned with `--depth 1` on 2026-09-30 and read locally. Paths are relative to each repo root. Line counts are from `wc -l` on the clone. Anything not read is marked UNVERIFIED.

## Projects inspected

1. **tbaMUD (CircleMUD/Diku lineage)** - https://github.com/tbamud/tbamud. C, about 93K lines in `src/`.
   - Loop: `heartbeat(++pulse)` at `src/comm.c:1048`. Each call tests `pulse % N` against constants in `src/structs.h:569-588` (PULSE_ZONE 10s, PULSE_MOBILE 10s, PULSE_VIOLENCE 2s, AUTOSAVE 60s). Missed pulses are replayed in a loop (`comm.c:1005-1023`), capped at 30s.
   - Mobs: `mobile_activity()` (`src/mobact.c:41`) walks the whole `character_list` each pulse. A specproc function pointer runs first. If it returns true, default behaviour is skipped. Otherwise flag-driven defaults run (scavenge, wander, aggro). Randomness is `rand_number`.
   - Respawn: `zone_update()` (`src/db.c:2776`) ages zones once a minute and queues them. `reset_zone()` (`db.c:2859`) runs a data script of commands (`M` load mob if count < max, `O`, `G`, `E`, `R`). Example data: `lib/world/zon/30.zon`. The count check is the population cap.
   - Events: `src/dg_event.c:62` `event_create(func, obj, when)` is a min-queue keyed on pulse. DG Scripts (`dg_scripts.c`, `dg_triggers.c`) add per-mob trigger scripts.
2. **Evennia** - https://github.com/evennia/evennia. Python/Twisted/Django, 178K lines.
   - Loop: no central tick. `TickerHandler` (`evennia/scripts/tickerhandler.py:300`) pools objects by interval on Twisted `ExtendedLoopingCall` (line 170). Scripts (`scripts/scripts.py`) have `at_repeat` with an interval.
   - Persistence: every attribute is pickled into a Django DB row (`typeclasses/attributes.py:24`). Time is wall-clock.
3. **OpenTTD** - https://github.com/OpenTTD/OpenTTD. C++, 181K lines in `src/*.cpp`.
   - Loop: `StateGameLoop()` (`src/openttd.cpp:1201`) runs `RunTileLoop`, `CallVehicleTicks`, `CallLandscapeTick`, then AI scripts. `CallLandscapeTick` (`landscape.cpp:1730`) calls `OnTick_Town`, `OnTick_Station`, `OnTick_Industry`, `OnTick_Companies`.
   - Economy: industries are pool objects iterated each tick (`industry_cmd.cpp:1236`). Daily accumulation is staggered with `(counter + i->index) % DAY_TICKS`. Cargo is discrete `CargoPacket` objects (`cargopacket.cpp`, 891 lines), so every unit has an origin and a path.
   - Determinism: lockstep multiplayer with a sync seed (`network/network_server.cpp:654`). Desync dumps a savegame (`openttd.cpp:1222`). State changes go through a command layer (`command.cpp`).
4. **Brogue CE** - https://github.com/tmewett/BrogueCE. C, 38K lines.
   - Loop: energy scheduler. Each creature has `ticksUntilTurn` (`Time.c:1931`). The loop subtracts the soonest value from all (`Time.c:2395`). A separate 100-tick "environment" beat handles spawn fuse, status decay and fire/gas (`Time.c:2398-2430`).
   - Determinism: one seed. `RNG_SUBSTANTIVE` and `RNG_COSMETIC` streams (`Rogue.h:402`). Per-level seeds (`RogueMain.c:685`). Replays are just the keystroke log (`Recordings.c:133`).
5. **NetHack 3.7** - https://github.com/NetHack/NetHack. C, `src/` 251K lines.
   - Loop: `moveloop_core` (`src/allmain.c:173`). Movement points: the hero gets `u.umovement`. Monsters get `mcalcmove` each turn (`mon.c:1131`), with a random rounding of fractional speed (`mon.c:1162-1164`). Random spawn is `!rn2(70)` per turn (`allmain.c:160-163`). Per-mob AI is `dochug` then `m_move` (`monmove.c:690`, `1717`).
6. **Veloren rtsim** - https://github.com/veloren/veloren. Rust, `rtsim/src` 11.5K lines. Most relevant.
   - Loop: `RtState::tick` (`rtsim/src/lib.rs:~318`) bumps `data.tick` and emits an `OnTick` event. Rules are structs that `bind` handlers to typed events (`lib.rs:236`, `event.rs`). Events include `OnDeath{actor,killer}`, `OnTheft`, `OnHealthChange{cause}`. Emit is immediate, not queued (comment at `lib.rs:298` says so).
   - Actors have `SimulationMode::{Simulated, Loaded}` (`data/actor.rs:43`). Unloaded NPCs are simulated coarsely by rtsim, and loaded ones are ECS entities.
   - AI: `ai/mod.rs` (1200 lines) defines `trait Action::tick -> ControlFlow<R>` with combinators `then`, `repeat`, `casual`. They are resumable coroutines with priority levels. The per-NPC RNG is `ChaChaRng` (`ai/mod.rs:80`). `Sentiment` memory per actor (`data/sentiment.rs`).
7. **Cataclysm: DDA** - https://github.com/CleverRaven/Cataclysm-DDA. C++, `src/*.cpp` 510K lines, about 940 files.
   - Loop: move-point system. `monster::plan()` then `monster::move()` (`monmove.cpp:558`, `976`). `npc::move()` (`npcmove.cpp:1519`) recomputes an `ai_cache` and picks an `npc_action`. A behaviour tree (`behavior::tree`) is now being run beside the legacy `decide_needs()` and compared in debug (`npcmove.cpp:1565-1578`). Sign that the old rule code was hard to replace.
   - Data: monsters, effects, missions, effects-on-condition are JSON under `data/json/`. Timed events are in `timed_event.cpp`.
8. **Freeciv** - https://github.com/freeciv/freeciv. C. `server/` + `common/` + `ai/` = 157K lines.
   - Loop: `srv_running()` (`server/srv_main.c:2941`) calls `begin_turn`, `begin_phase`, `end_phase`, `end_turn`. AI is a pluggable module (`ai/default/dai*.c`, `aiiface.c`): military, city, diplomacy, hunter, each in its own file.
9. **Wesnoth** - https://github.com/wesnoth/wesnoth. C++. `src/ai` 13.5K lines.
   - AI: RCA = candidate actions, each with `evaluate()` returning a score and `execute()`. `stage_rca.cpp:87-115` sorts by max score, evaluates, runs the best, repeats until none remains. Early exit when the next action's upper bound is below the current best. Closest thing in the survey to `decide(role, observation, options)`.
10. **Mindustry** - https://github.com/Anuken/Mindustry. Java, 173K lines.
    - Loop: `Logic.update()` (`core/src/mindustry/core/Logic.java:503`) uses `state.tick += delta*60` from a real-time float (line ~528). `AIController` splits `updateTargeting()` and `updateMovement()` (`ai/types/` is about 2.6K lines: MinerAI, SuicideAI, etc.). Not deterministic.
11. **Micropolis** - https://github.com/SimHacker/MicropolisCore. C++, engine 18K lines.
    - Loop: `simFrame()` gates on speed (`simulate.cpp:108`), then `simulate()` (line 132) is a 16-phase `switch (phaseCycle)`. One scan type per phase, so no phase sees a half-updated map. Scan rates come from tables indexed by speed (lines 134-143).
12. **RanvierMUD** - https://github.com/RanvierMUD/ranviermud. Node.js. The repo holds only bundles and config. Engine core is the npm package `ranvier ^3.0.5` (`package.json:22`), NOT read. UNVERIFIED: its event and behavior internals.

## What to steal

- **Staggered periodic work by index** (OpenTTD `(counter + i->index) % DAY_TICKS`, `industry_cmd.cpp:1245`). Spreads cost and is deterministic.
- **Pulse modulo heartbeat** (tbaMUD `comm.c:1048`). One integer tick and named periods, with missed-tick catch-up capped. Good for a seeded tick: hunger daily, caravans per hour.
- **Zone reset as a declarative table with population caps** (`db.c:2877`, `M` only if `count < max`). Use for goblin camp headcount and bandit cap. Better: make it a demand-driven refill, not a timer.
- **Typed event bus with `cause` field** (Veloren `event.rs`: `OnDeath{killer}`, `OnHealthChange{cause}`). Matches the "every outcome has a recorded cause" requirement. Fitness is then a fold over the log.
- **Sim vs loaded LOD** (Veloren `SimulationMode`). Coarse-simulate far actors, same state struct.
- **Score-and-execute candidate list with upper-bound pruning** (Wesnoth `stage_rca.cpp:87-115`). Natural shape for `decide(role, obs, options)`: a rule brain and a net both return a score per option.
- **Resumable action combinators** (Veloren `ai/mod.rs:150-222`) for multi-tick jobs like "dig tunnel, carry treasure, stash in hoard", so brains decide only at interruption points.
- **Energy scheduler with `ticksUntilTurn`** (Brogue) for mixed speeds: carriage vs goblin vs dragon.
- **Split RNG streams** (Brogue `RNG_COSMETIC` vs `SUBSTANTIVE`) plus input-log replay. Also per-entity seeded RNG (Veloren `ChaChaRng`). Adding a new consumer must not shift other draws.
- **Discrete cargo packets with origin** (OpenTTD `cargopacket.cpp`). Gives a cause chain for every coin and bread loaf.
- **Phase-per-scan update** (Micropolis `simulate.cpp:132`) to avoid order-dependence between systems.
- **Data-driven behaviour defs in JSON** (CDDA `data/json`).
- **Desync dump + sync hash** (OpenTTD) as a determinism test: hash world state every N ticks in CI.

## What to avoid

- **Linear scan of all mobs each pulse with flag soup** (tbaMUD `mobact.c:41-100`, random 1-in-19 wander at line 99). Does not scale and hides cause.
- **Specproc returning bool to short-circuit** (`mobact.c:68`). Hidden control flow, no log.
- **Wall-clock timers and pickled DB state** (Evennia `tickerhandler.py`, `attributes.py:24`). Not reproducible and slow.
- **Float real-time delta for sim** (Mindustry `Logic.java:528`). Non-deterministic. Use integer ticks.
- **Immediate event emit with re-entrant handlers** (Veloren itself flags it as a TODO, `lib.rs:298`). Queue events and drain per tick so order is fixed.
- **Giant god files** (CDDA `game.cpp` 11.6K, `map.cpp` 12K; OpenTTD `station_cmd.cpp` 5.4K). Rotted with the feature count.
- **Two AIs kept in parallel during migration** (CDDA `npcmove.cpp:1565`). Plan the brain interface first.
- **Per-frame random fractional speed** (NetHack `mon.c:1162`) is fine but makes balance hard to reason about. Prefer integer speeds.
- **Bounded event types only as strings/flags.** DG Scripts (`dg_scripts.c`) are an in-game scripting language that became its own maintenance burden.

## Open questions

- Ranvier engine internals not inspected (UNVERIFIED). LPMud/mudlib and KallistiMUD not cloned. Dwarf Fortress-likes, Songs of Syx, Thrive, Vic clones not inspected.
- I did not profile any engine. Speed claims above are structural, not measured.
- Veloren's save/replay determinism was not checked (UNVERIFIED). Its rtsim handlers use `slotmap` and `hashbrown`, so iteration order needs review.
- How to keep an event log cheap: a ring buffer per tick with cause ids, or full log? None of these projects logs causes per outcome; Veloren events are the nearest.
- Whether `decide()` should be batched per role per tick (net inference cost) or per actor. Wesnoth and Veloren both decide per actor.
