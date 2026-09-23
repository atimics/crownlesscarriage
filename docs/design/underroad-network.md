# The Underroad as a road network

Status: proposed · 2026-09-20

One sentence: **the Underroad should be a second transport layer under the whole
map — a generated graph of goblin haul roads connecting every settlement — not a
single 24-room dungeon at Silverwick.**

Successor to the standalone growth proof in `tools/underroad_construction.c`
(issue #799) and the first-person requirement in #797.

## Why

Today the Underroad is a point, not a network:

- The sim has **one** dungeon, `sim->dungeons[0]` at Silverwick, a 24-room graph
  (`GenerateUnderroad`, `src/sim/cc_sim.c:1626`).
- The only surface link is `ApplyGoblinTunnelTraversal` (`src/sim/cc_sim.c:18927`),
  an abstract one-day crossing between the goblin lair and the dragon roost.
- The three clans of the Cinder Tithe share that one physical dungeon and BFS
  loot toward room 19 (`src/sim/cc_goblin_politics.inc:109`).

`tools/underroad_construction.c` already proves the harder idea: a real tile
mountain that goblin crews *dig into haul roads over simulated years*. That work
lives outside the sim and belongs to one mountain. This design makes the same
process span the overworld.

Payoff: entering at one settlement and surfacing at another, bypassing surface
routes, at the cost of darkness, tolls, patrols and collapses.

## Concept

The Underroad is a graph coextensive with the overworld.

- **Nodes** are entrances and landmarks: settlement mine mouths/cellars/wells,
  the three clan lairs, carved junctions, shrines, ruins, the hoard.
- **Edges** are **goblin roads**: haul roads with depth, clearance, condition,
  security, toll and an owning clan.
- The network is **generated from the world seed**, then **modified by goblin
  labour over the years** — cut, widened, bridged, flooded, collapsed — exactly
  the thesis of `UrRunYear`. The player explores the goblins' transport history,
  not a dungeon rolled for them.

Two scales, built in order:

- **Macro (sim):** nodes + road edges. Goblins use it; the player travels it.
  Cheap, hashable, persistable.
- **Micro (later):** the tile crawl from the tools, generated lazily per road
  when the player actually walks it. The existing 24-room dungeon becomes the
  Silverwick hub's interior.

## Data model

New structs beside the dungeon structs in `src/sim/cc_sim.h:1378-1440`:

```c
typedef enum CcUnderroadNodeKind {
    CC_UR_NODE_ENTRANCE,   /* settlement mouth: mine / cellar / well */
    CC_UR_NODE_LAIR,       /* goblin clan lair */
    CC_UR_NODE_JUNCTION,   /* carved crossroads */
    CC_UR_NODE_SHRINE,
    CC_UR_NODE_HOARD,
    CC_UR_NODE_RUIN
} CcUnderroadNodeKind;

typedef struct CcUnderroadNode {
    CcId id;
    CcUnderroadNodeKind kind;
    CcId settlement_id;              /* anchor, or 0 */
    int32_t map_x, map_y, depth;
    char name[CC_MAP_NAME_CAPACITY];
    uint32_t flags;                  /* DISCOVERED / KNOWN / SEALED */
    uint32_t seed;
} CcUnderroadNode;

typedef enum CcUnderroadRoadKind {
    CC_UR_ROAD_HAUL,
    CC_UR_ROAD_SMUGGLER,
    CC_UR_ROAD_NATURAL,
    CC_UR_ROAD_FLOODED,
    CC_UR_ROAD_COLLAPSED
} CcUnderroadRoadKind;

typedef struct CcUnderroadRoad {
    CcId id, from_node, to_node;
    CcUnderroadRoadKind kind;
    int32_t depth;                   /* layer 0..CC_UR_LAYERS-1 */
    int32_t length_cells;
    int32_t clearance;               /* porter width */
    int32_t condition;               /* 0..1000, decays */
    int32_t security;                /* encounter and toll pressure */
    int32_t toll_milli;              /* Cinder Tithe cut */
    int32_t dig_progress_milli;      /* labour applied this era */
    int32_t faction_id;              /* owning clan, -1 contested */
    uint32_t seed;
} CcUnderroadRoad;

typedef struct CcUnderroadNetwork {
    uint32_t layout_seed, revision;
    bool generated;
    int32_t node_count, road_count;
    CcUnderroadNode nodes[CC_MAX_UR_NODES];
    CcUnderroadRoad roads[CC_MAX_UR_ROADS];
} CcUnderroadNetwork;
```

Caps mirror the existing style. `sizeof(CcSim)` is a hard static assert
(`cc_sim.h:2115`), so either keep the caps tight (e.g. `CC_MAX_UR_NODES 16`,
`CC_MAX_UR_ROADS 32`) or persist the network in its own table loaded alongside
the sim. First pass keeps it in-sim so goblin logic can path it, and takes the
version bump.

## Generation

1. **Anchor nodes.** One entrance per settlement; the goblin lair; the dragon
   roost; the three `CC_WORLD_SITE_*` markers (`src/world/cc_world.c:625`).
   That alone spans the map.
2. **Candidate edges.** k-nearest over `(map_x, map_y)` + an MST for
   connectivity + a few extra edges for loops — the same "greedy lines
   connecting nodes, some collapsing" as `UrBuildMountain:782`.
3. **Carve each edge.** A seeded random walk across the tile grid, biased
   toward natural caverns and away from hard rock (`UrCarveCavern:415`),
   assigning a depth layer and noting bends and crossings. Crossings become
   bridges or shafts.
4. **Natural first, then works.** Caverns, a canyon, fissures and unstable
   ground come from an **invariant** constant seed (like `UR_MOUNTAIN_SEED`);
   only the roads come from `world_seed`. "The mountain is the same mountain in
   every world; only what the living colonies did to it differs."
5. **Faction territory.** Voronoi over the three clan lairs assigns each road an
   owner; border roads are contested. Feeds the existing `goblin_politics` and
   the Tithe toll.

## Simulation over years

Port the tools' year loop onto the macro edges:

- `UrPlanWork` (crew work orders) → load-aware `UrRoute` Dijkstra → cut / widen /
  bridge / clear → `UrScout` → `UrMaintain` → bridge failures → collapses →
  `UrSpoilsWar` after the dragon dies.
- Sim-side cheap version per road: `dig_progress`, `condition` decay,
  `clearance` growth, plus events (collapse, flood, discovery).
- Hook into `AdvanceGoblinTribute` and `AdvanceGoblinPolitics`. A road's
  condition and clearance decide what porter load classes can pass, which is
  what makes widening strategically useful.
- Replace the single `ApplyGoblinTunnelTraversal` with a path over the graph.

## Player-facing

- Entrances become discoverable world sites (extend `CC_WORLD_SITE_COUNT`,
  `src/world/cc_world.c:625-652`) and route-book knowledge.
- New text verbs beside the existing ones (`src/metagame/cc_metagame.c:2486`):
  `underroad map`, `underroad travel NODE`, `underroad dig`.
- `CcDungeonExpedition` gains `node_id` / `road_id`; travelling a road spends
  turns, light, strain and rations; encounters scale with `security`; tolls are
  paid to the owning clan.
- The payoff mirrors the current tunnel — a shorter, hidden crossing — but
  risky and metered.

## Rendering

- Native automap: extend `DrawDungeonPanel` (`src/client/main.c:3757`).
- First-person micro: reuse the mine pattern (`src/sim/cc_mine.c`,
  `src/client/local3d/mine_scene.inc`).
- Web atlas: extend `tools/underroad-map.html` from 24 zones to the network.
- ASCII: the tools already crawl and print.

## Required plumbing (this codebase is strict)

- `CC_SIM_SCHEMA_VERSION 102 → 103`, `CC_GENERATOR_VERSION 25 → 26`
  (`src/sim/cc_sim.h:70`); recompute the `sizeof(CcSim)` assert.
- Hash block beside `hash_underroad` (`src/sim/cc_sim_hash.c:461-505`).
- Persistence beside `underroad_schema` (`src/persistence/cc_save.c:1383`):
  write (`SaveDungeons`, `:2416`) and read (`ReadUnderroad`, `:4683`).
- `CcSimValidate` dungeon block (`src/sim/cc_sim.c:20422`).
- Versions tables: `src/sim/cc_sim_versions_internal.h`, `cc_sim_versions.c`.
- Defaults in `CcSimInit` / `CcSimInitializeUnderroad`.

## Tests

Same-seed determinism; every entrance reachable; no road through solid rock
without a bridge; porter routing beats surface travel when condition is high;
persistence round-trip; schema migration; no lair disconnected. Clone
`tests/underroad_tests.c` and register in `cmake/tests/60-world-archive.cmake`.

The prototype `tools/underroad_network.c` fixes the generation contract and
carries its own acceptance report before anything is wired into the sim.

## Phasing

- **P0 — commit the tools.** Done: `tools/underroad_construction.c`,
  `tools/underroad_world.c` and their CMake registration were untracked.
- **P1 — macro network.** Generate, hash and persist the graph; goblin porters
  path it. Prototype lands as `tools/underroad_network.c`. No player access yet.
- **P2 — entrances.** World sites + text `underroad map` / `underroad travel`.
- **P3 — micro.** Lazily generate the tile crawl per road for first-person play.
- **P4 — politics.** Faction territory, tolls, war effects on roads.

## Open questions

- Keep the network inside `CcSim` (version bump, size contract) or in a side
  table? Recommendation: in-sim first, side table only if it grows.
- Does the existing 24-room dungeon stay the Silverwick hub, or get regenerated
  as a normal micro region? Recommendation: keep it as the hub's authored
  interior so existing saves, tests and the web atlas stay valid.
- Depth semantics: the tools use 4 layers with vertical links. The sim graph
  uses `depth` on rooms. Reconcile by making `depth` the layer index and
  `DROP`/`SHAFT` links the vertical connectors.