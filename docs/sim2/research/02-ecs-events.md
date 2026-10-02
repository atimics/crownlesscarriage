# sim2 research 02: ECS and event-driven simulation architecture

Method note: "Opened" = WebFetch returned real page content. "Snippet" = only a search-result summary (treated as UNVERIFIED detail). Several fetches returned summaries from a small model, so quotes are secondhand. Flecs, Unity and Overwatch details beyond what is listed are UNVERIFIED.

## Sources read
1. https://raw.githubusercontent.com/SanderMertens/flecs/master/docs/Quickstart.md (opened) - flecs: entities with same component set grouped by "type"/archetype; observers fire on OnAdd/OnRemove/OnSet; no snapshot/world-copy feature described there.
2. https://raw.githubusercontent.com/SanderMertens/flecs/master/docs/DesignWithFlecs.md (opened) - systems ordered by phase, then by declaration order within a phase; ordering control is deliberately coarse (module level).
3. https://github.com/SanderMertens/ecs-faq (opened) - archetype = fast iterate, costly add/remove; sparse set = cheap add/remove, extra indirection on iterate.
4. https://bevy-cheatbook.github.io/patterns/component-storage.html (opened) - Bevy lets you choose Table vs SparseSet per component; tables fragment with varied component mixes; sparse set suits transient markers.
5. https://skypjack.github.io/2019-03-07-ecs-baf-part-2/ (opened) - EnTT sparse-set pools (sparse + packed arrays); "groups" pack matching entities at array front for near-archetype iteration.
6. https://docs.unity3d.com/Packages/com.unity.entities@1.3/manual/concepts-archetypes.html (opened) - DOTS: 16 KiB chunks per archetype; structural change moves entities and is expensive.
7. https://www.factorio.com/blog/post/fff-302 (opened) - Factorio: all clients simulate deterministically from inputs; any difference = desync.
8. https://www.factorio.com/blog/post/fff-47 (opened) - CRC of whole map every tick in a debug mode; saves tagged state each tick, replays, compares; first diverging tick usually differs by one variable.
9. https://www.factorio.com/blog/post/fff-188 (opened) - "making a fully deterministic game is not easy"; relies on desync reports; no causes given.
10. Factorio FFF search results (snippet; UNVERIFIED detail) - a cheap "heuristic CRC" (player state, active entity count, stats) every tick, and an STL sort with ambiguous comparator differed across compilers. URLs: https://factorio.com/blog/post/fff-55 , https://www.factorio.com/blog/post/fff-51
11. https://www.gamedeveloper.com/programming/1500-archers-on-a-28-8-network-programming-in-age-of-empires-and-beyond (opened) - AoE lockstep: commands executed 2 turns later; RNG must be seeded and synchronized; tiny differences "multiply over time"; sync bugs hard to trace.
12. https://gafferongames.com/post/floating_point_determinism/ (opened) - float determinism needs same compiler/ISA, strict FP mode, no FMA contraction, no x87, libm varies, SIMD under-specified, debug vs release can differ.
13. https://github.com/WebAssembly/design/blob/main/Nondeterminism.md (opened) - Wasm nondeterminism: NaN bit patterns, relaxed SIMD, threads, resource exhaustion, feature variance.
14. https://apple.github.io/foundationdb/testing.html (opened) - FoundationDB: whole cluster simulated deterministically in one thread, simulated time, fault injection, ~10:1 sim-to-real time, huge CPU-hours of runs.
15. https://risingwave.com/blog/deterministic-simulation-a-new-era-of-distributed-system-testing/ (opened) - DST = fixed seed, single thread, simulated time jumping between events, simulated environment; any missed randomness source breaks it; hard with third-party libs.
16. https://www.snellman.net/blog/archive/2016-07-27-ratas-hierarchical-timer-wheel/ (opened) - timer wheel O(1) insert/cancel; hierarchical wheels for long ranges; finding next event scans empty slots.
17. https://en.wikipedia.org/wiki/Calendar_queue (opened) - calendar queue O(1) average; power-of-two buckets; width re-estimated on resize; bad when event count and bucket count mismatch; tie-breaking not specified.
18. https://www.pcg-random.org/index.html (opened) - PCG: small state, multiple streams, jump-ahead, reproducible; C implementation available.
19. https://martinfowler.com/eaaDev/EventSourcing.html (opened) - event log as source of truth; rebuild by replay; snapshots to avoid replaying everything; retroactive events; separate replay from real side effects.
20. https://www.gdcvault.com/play/1024001/-Overwatch-Gameplay-Architecture-and-Netcode (snippet only) - Overwatch ECS: fixed 16 ms command frames; quantised floats rather than true bitwise determinism; netcode surface is a few systems. UNVERIFIED beyond snippet.
21. https://www.gamedeveloper.com/game-platforms/interview-inside-the-ai-of-i-s-t-a-l-k-e-r-i- (snippet only) - STALKER A-Life: offline behaviour is a cheaper "LoD" of online behaviour. UNVERIFIED detail.
22. https://isocpp.org/blog/2015/01/cppcon-2014-data-oriented-design-and-c-mike-acton (snippet only) - Acton DOD: program = transform data; fit data to cache lines; claims of 10x. UNVERIFIED detail.

Failed/unhelpful fetches: flecs.dev doc pages (redirect stubs), YouTube, TigerBeetle VOPR blog (404). So TigerBeetle/VOPR, Bevy Rust docs, Mike Acton slides, and Dwarf Fortress/RimWorld LOD were NOT read.

## Recommended architecture for sim2

Reasoning from sources plus our needs (clone cheaply, deterministic, C11, thousands of world-years/hour). Design choices below are my recommendations, not claims from sources.

### Component storage: sparse-set-style dense SoA pools, one pool per component, fixed capacity
- Why not archetypes (src 3,6): structural moves on every state change (hungry townsfolk becoming bandit, carriage loading treasure) cost copies; archetype/chunk tables also make a clone a graph of allocations. Our population is small (likely 1e3 to 1e5 actors) and role changes are common, which favours cheap add/remove (src 3,4).
- Layout: entity id = 32 bit (index 24 | generation 8). Per component type: `uint32_t sparse[MAX_ENT]` (index to dense slot or 0xFFFFFFFF), `uint32_t dense_ent[cap]`, and SoA field arrays `float/int32 field_x[cap]`. Remove = swap-with-last, which makes dense order depend on removal history; that is fine because it is deterministic, but systems must never depend on order for results that matter (or must sort by entity id where order matters, see Factorio sort lesson, src 10).
- Use EnTT-style "group" idea (src 5) only if profiling demands it: keep the hot set (position, needs, role) co-packed in one "actor" table indexed by entity slot directly, since every actor has them. Rare components (bandit_band, goblin_tunnel, hoard) live in sparse pools.
- Everything lives in ONE arena (`struct World { header; char arena[] }`) with offsets, not pointers. All arrays fixed-capacity, sized at world creation (max actors by type). No malloc in the tick.

### Clone / snapshot
- If the whole world is a single flat arena with no pointers (offsets/indices only), clone = `memcpy` of the used prefix (or of everything when it is some MB). Counterfactual replay = memcpy + reseed/override one decision + run forward.
- For larger worlds add page-level copy-on-write later (fork(), or software dirty-page bitmap at 4 KB granularity). UNVERIFIED that this pays off here; measure memcpy first. Wasm has no fork, so a dirty-page scheme would be software only.
- The event log is append-only and NOT inside the clone memcpy; a clone records `log_len` and branches get their own log tail (persistent prefix shared, read-only).

### Time model: hybrid, event-driven with a coarse fixed tick
- Fixed tick = 1 sim-hour (or 15 min); each tick runs phases in fixed order (like flecs phases by declaration order, src 2): 1 timers/events due, 2 perceive, 3 decide, 4 act/resolve, 5 consume/needs decay, 6 housekeeping/deaths/births, 7 hash.
- Most actors are idle most of the time (baker sleeping, caravan mid-road, goblin tunnelling). Give each actor a `next_wake_tick`; do not run decide() for every actor every tick. This gives the speed of discrete-event simulation (src 15: jump over empty time) while keeping a tick boundary for hashing, LOD and lockstep-style checks (src 11,14).
- Queue: a single-level timer wheel (src 16) with 2^k slots indexed by tick, plus an overflow min-heap for far events (e.g. dragon naps, seasons). Because the tick is coarse and integer, O(1) insert/cancel matters and calendar-queue width resizing (src 17) is unnecessary complexity. Within a slot, events are a linked list in a preallocated event pool (index links, cloneable). To make ordering total and deterministic, dispatch sorted by `(tick, priority, event_seq)` where `event_seq` is a monotonically increasing u64 assigned at scheduling (calendar queue source does not define tie-breaking, src 17, so we must).
- Cancellation: store `gen` in the event; actor death bumps entity generation; stale events are dropped at dispatch.

### Events, causality, event log
- Two separate things: (a) scheduled events (the queue, future) and (b) the log (past, append-only, fixed-size binary records).
- Log record: `{u64 seq, u32 tick, u16 type, u32 subject, u32 object, i32 a, i32 b, u64 cause_seq}`. `cause_seq` points to the record that caused this one (e.g. "starved" caused by "flour stolen" caused by "bandit raid"). Every outcome (death, birth, trade, theft) must be written by a single `emit(type, subj, obj, cause, args)` call, so no outcome exists without a cause. Enforce with a test that state-changing functions are only called via emit-wrappers.
- Fitness for brains = fold over the log per lineage (survival ticks, offspring count), as the brief requires. Fowler (src 19): log is authoritative; snapshots avoid full replay; keep replay free of external side effects.
- Decisions: log `DECIDE{actor, role, option_index, rng_draw_count}` so a counterfactual can override exactly one decision.

### RNG
- PCG32 (src 18) or SplitMix64/xoshiro, hand-written in C with only integer ops (identical on Linux/macOS/Wasm). One stream per entity: state derived as `hash(world_seed, entity_id, generation)`; plus a few world streams (weather, spawn, worldgen). Per-entity streams mean that removing or adding an unrelated actor does not shift other actors' random draws, which is what makes counterfactual diffs small and readable. (Design rationale mine; AoE src 11 shows the cost of shared unsynchronised RNG.) Store `u64 state,inc` in a component: cloned for free.
- Never use rand(), time(), pointers, or hash-table iteration as a source of order.

### Numerics
- Gameplay state in integers / fixed-point (Q16.16 or Q24.8). Avoid float in anything that affects decisions or is hashed, because FMA, libm, compiler flags and Wasm NaN payloads differ (src 12,13). If float is wanted for neural nets: inference in int8/int16 fixed-point with integer accumulation, or float with strict flags and no libm (only + - * /, no FMA: `-ffp-contract=off`), and round outputs to integers before they touch state. Test identical hashes across Linux, macOS arm64 and Wasm in CI (DST spirit, src 14).
- Explicit sorts: use stable sort or comparators with entity id as final tiebreak (Factorio's ambiguous-comparator lesson, src 10, UNVERIFIED detail).

### Hashing
- Every N ticks (and every tick in debug) compute a 64-bit hash of the arena's logical state (FNV-1a or xxHash/wyhash written out in integer C; hash only used prefix and skip padding: zero-init and use `#pragma pack`/explicit padding fields so padding bytes are deterministic). Record `(tick, hash)` in the log. Factorio does a CRC of the whole map per tick in debug mode and bisects to the first differing tick (src 8): copy that workflow: golden run stores per-tick hashes, candidate run stops at first mismatch and diffs the two arenas field by field. Also hash per-pool so a mismatch names the component.

### LOD simulation
- Tie to the event model: the world is tiled into regions (town, road segment, goblin camp, dragon lair). Regions without "interesting" content (no carriage on the road, nothing threatened) run coarse: aggregate flows (bread production per day, hunger level of population as a count) resolved by one event per region per day instead of per-actor per-hour. Promote to full actors when a carriage, bandit band or goblin caravan enters or when brain-training wants full fidelity. STALKER's offline-as-LoD idea (src 21, UNVERIFIED) is the precedent. Risk: coarse and fine paths must agree statistically; add tests that compare aggregate outcomes (e.g. starvations per year) between modes.
- Decision interface decide(role, observation, options): fixed-size observation struct and option array, so a neural net sees the same bytes that a rule does and the call is loggable/replayable.

### System execution
- Single-threaded per world (src 14,15); parallelise across worlds (thousands of world-years/hour comes from running many independent worlds on all cores, not from threading inside one). Systems = plain C functions in a static table ordered by phase (src 2). Deferred structural changes: systems push add/remove/spawn/despawn commands into a per-phase command buffer applied in entity-id order at phase end, so iteration never sees mutation (Unity/Bevy sync-point idea, src 6; UNVERIFIED for ordering specifics).

## What to avoid
- Archetype/chunk storage with frequent role changes (src 3,6).
- Pointers inside the world state (breaks memcpy clone and hashing).
- Float in decision-affecting state; libm calls (sin/exp); relying on compiler flags that differ between builds (src 12,13).
- Hash-map iteration order, qsort with non-total comparators, uninitialised padding, address-based ordering (src 10, 11).
- A global shared RNG; draws in iteration-order-dependent places (src 11).
- Third-party libs with hidden randomness or threads in the sim core (src 15).
- Running every actor every tick when most are waiting.
- Copying a mature ECS wholesale (flecs/EnTT are C++ or heavy, flecs has no documented snapshot, src 1): take the ideas, not the dependency. (flecs is C, but a full-world memcpy clone is unlikely with its allocator; UNVERIFIED.)
- Calendar queues with adaptive widths (resize cost, unspecified tie-breaks, src 17).
- Log records without `cause`, or logging from outside the emit path.
- Treating Overwatch-style "quantised but not bit-exact" as enough: we need bit-exact for replay/counterfactual (src 20, UNVERIFIED).

## Open questions
1. Tick resolution (hour vs 15 min vs minute) and max entity counts per world: drives pool sizes and whether memcpy clone is cheap enough (expected KB-to-low-MB, to be measured).
2. Neural net numerics: integer-only inference vs strict float; who owns the quantisation spec.
3. Do counterfactual replays need exact log prefix sharing, or is recomputing from the snapshot fine?
4. LOD: which regions go coarse, and how to validate that coarse and fine agree. Does a coarse region still emit per-event causes for the log, or aggregate "cause" records?
5. Event-queue cancellation policy on death (generation check vs eager removal).
6. Hash cost per tick at scale: per-tick vs every N ticks; per-pool hashes vs whole arena.
7. Wasm: confirm no NaN/float divergence in practice with a CI cross-platform hash test; need for 64-bit integer performance on Wasm32.
8. Unread: TigerBeetle VOPR, Dwarf Fortress/RimWorld/Caves of Qud LOD and snapshot practices, Bevy/flecs internals of command buffers.
