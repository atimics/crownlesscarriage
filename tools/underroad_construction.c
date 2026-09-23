/*
 * underroad_construction.c - procedural Underroad, walked as an ASCII crawl.
 *
 * Standalone companion to issue #799 ("Underroad Construction") and the
 * player-facing requirement in #797. Nothing here links against the
 * simulation; it is a self-contained proof of the growth algorithm:
 *
 *   a mountain of natural caverns, a canyon, fissures and unstable ground
 *     -> goblin crews haul known loot toward one dragon hoard
 *       -> hauling friction (clearance, bends, crossings, collapses)
 *         -> scouting, cutting, widening, bridging, bypassing
 *           -> the dungeon is what those years of work leave behind
 *
 * The same persistent tiles are then walked at eye level: first-person view,
 * fog-of-war automap, inspection of actual worksites. No dungeon is rolled
 * for the player; the player explores the goblins' transport history.
 *
 *   cc -std=c17 -O2 -Wall -Wextra -o underroad tools/underroad_construction.c
 *   ./underroad                 # construct, then crawl (w/s/a/d, x, m, q)
 *   ./underroad --report        # headless construction proof + checks
 *   ./underroad --dates         # dated states of the same region
 *   ./underroad --auto          # scripted first-person out-and-back
 *   ./underroad --seed 7 --years 24 --no-inject
 */
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define UR_W 60
#define UR_H 22
#define UR_LAYERS 4
#define UR_LAYER_CELLS (UR_W * UR_H)
#define UR_CELLS (UR_LAYER_CELLS * UR_LAYERS)
#define UR_MAX_BRIDGES 12
#define UR_MAX_NODES 128
#define UR_MAX_EVENTS 512
#define UR_MAX_SNAPS 8
#define UR_MAX_REQUESTS 12
#define UR_FACTIONS 3
#define UR_CREWS UR_FACTIONS
#define UR_SOURCES 5
#define UR_VIEW_DEPTH 4
/* The mountain is the same mountain in every world. Its caverns, canyon,
 * rock and ancient workings come from this constant; only what the living
 * colonies did to it comes from the world's own seed. */
#define UR_MOUNTAIN_SEED UINT32_C(0x4d4f554e)
#define UR_PORTER_LOAD 10
#define UR_CRATE_LOAD 120

/* Tile kinds. Natural void, walkable surface, rock/rubble and built
 * structures stay distinct: empty space is never automatically traversable. */
typedef enum UrKind {
    UR_ROCK = 0,   /* unexcavated stone */
    UR_RUBBLE,     /* collapse or spoil: blocked until cleared */
    UR_VOID,       /* canyon air: open, not traversable */
    UR_CAVERN,     /* natural walkable surface */
    UR_CUT,        /* goblin-cut passage */
    UR_CHAMBER,    /* widened corner / turning chamber */
    UR_RECESS,     /* passing recess beside a passage */
    UR_BRIDGE,     /* built crossing over void */
    UR_CACHE,      /* staging cache for waiting loads */
    UR_SOURCE,     /* known loot cache */
    UR_HOARD,      /* the dragon's hoard, while there is a dragon */
    UR_LAIR,       /* a goblin colony's own camp */
    UR_STRONG,     /* that colony's strongroom: where its own take sits */
    UR_BARRICADE,  /* works thrown up against a rival's porters */
    UR_MOUTH,      /* old human mine mouth, the company's way in */
    UR_KIND_COUNT
} UrKind;

typedef enum UrVert {
    UR_VERT_NONE = 0,
    UR_VERT_FISSURE,  /* natural drop: scouts only */
    UR_VERT_LADDER,   /* built: scouts and porters */
    UR_VERT_RAMP      /* cut stepped ramp: bulky loads too */
} UrVert;

typedef enum UrPurpose {
    UR_PURPOSE_NATURAL = 0,
    UR_PURPOSE_MINE,       /* the old human workings */
    UR_PURPOSE_HAUL,       /* cut to carry loot */
    UR_PURPOSE_BYPASS,     /* cut around a failure */
    UR_PURPOSE_WIDEN,      /* corner opened for a bulky load */
    UR_PURPOSE_RECESS,     /* passing place */
    UR_PURPOSE_DESCENT,    /* worked vertical connection */
    UR_PURPOSE_APPROACH,   /* bridge approach / landing */
    UR_PURPOSE_ABANDONED,  /* a bore that stopped being useful */
    UR_PURPOSE_ANCIENT,    /* the old goblin nation's road, older than anyone */
    UR_PURPOSE_HOME,       /* cut to carry loot home instead of to a dragon */
    UR_PURPOSE_DEFENCE,    /* cut or blocked against a rival colony */
    UR_PURPOSE_COUNT
} UrPurpose;

enum {
    UR_FLAG_UNSTABLE = 1U << 0U,
    UR_FLAG_COLLAPSED = 1U << 1U,
    UR_FLAG_ABANDONED = 1U << 2U,
    UR_FLAG_MARKS = 1U << 3U,     /* fresh tool marks */
    UR_FLAG_ARROW = 1U << 4U,     /* haul mark / painted heading */
    UR_FLAG_SUPPORT = 1U << 5U,   /* timbering */
    UR_FLAG_SPOIL = 1U << 6U,     /* spoil dump */
    UR_FLAG_INJECTED = 1U << 7U   /* failure came from the test, not the sim */
};

/* The three colonies the simulation already knows: Red, Purple, Blue, who
 * compete in tribute while a dragon lives and over the loot itself when one
 * does not. */
typedef enum UrEra { UR_ERA_TRIBUTE = 0, UR_ERA_SPOILS } UrEra;

typedef enum UrLoad {
    UR_LOAD_SCOUT = 0,  /* unladen */
    UR_LOAD_PORTER,     /* porter pack */
    UR_LOAD_CRATE,      /* bulky carried crate */
    UR_LOAD_COUNT
} UrLoad;

/* What the rock is, which decides what it costs and what a wall looks like. */
typedef enum UrStone {
    UR_STONE_SHALE = 0,   /* soft, wet, cheap, unreliable */
    UR_STONE_SANDSTONE,
    UR_STONE_LIMESTONE,   /* where the natural caverns are */
    UR_STONE_GRANITE,     /* expensive; crews go around it */
    UR_STONE_QUARTZ,      /* hardest, and worth looking at */
    UR_STONE_COUNT
} UrStone;

typedef struct UrTile {
    uint8_t kind;
    uint8_t stone;      /* what this rock is */
    uint8_t hardness;   /* 1..12 work units to cut */
    uint8_t stability;  /* 0..9, low ground fails */
    uint8_t clear;      /* 1 squeeze, 2 passage, 3 open to bulky loads */
    uint8_t vert;       /* connection to the layer below */
    uint8_t purpose;
    uint8_t crew;       /* 0 none, else crew index + 1 */
    uint8_t wear;       /* traffic polish, 0..9 */
    uint8_t flags;
    uint8_t goblin_known;  /* bit per colony: who has been here */
    uint8_t seen;       /* the company's automap */
    int16_t year;       /* year worked, -1 natural */
    int16_t bridge;     /* bridge index, -1 none */
    int16_t traffic;    /* loads that crossed */
} UrTile;

typedef struct UrBridge {
    int32_t cells[16];
    int32_t cell_count;
    int32_t condition;  /* 0..100 */
    int32_t capacity;   /* 1 scout, 2 porter, 3 bulky */
    int16_t built_year;
    uint8_t crew;
    uint8_t failed;
} UrBridge;

typedef struct UrNode {
    char name[36];
    int32_t cell;
    uint8_t kind;
} UrNode;

typedef struct UrSource {
    char name[28];
    int32_t cell;
    int32_t crowns;       /* divisible loot, moved in porter packs */
    int32_t crates;       /* indivisible bulky objects */
    int32_t stranded;     /* loads waiting in a cache */
    int32_t worked_by;    /* bitmask of colonies that have drawn from it */
    bool replenished;     /* raids keep bringing more to this one */
} UrSource;

typedef enum UrCrewState {
    UR_CREW_REST = 0, UR_CREW_HAUL, UR_CREW_SCOUT, UR_CREW_WORK, UR_CREW_WAIT
} UrCrewState;

typedef enum UrWorkKind {
    UR_WORK_NONE = 0, UR_WORK_DIG, UR_WORK_WIDEN, UR_WORK_BRIDGE,
    UR_WORK_CLEAR, UR_WORK_DESCENT, UR_WORK_RECESS, UR_WORK_CACHE
} UrWorkKind;

typedef struct UrRequest {
    UrWorkKind kind;
    int32_t faction;
    int32_t from;       /* the loot this job is meant to move */
    int32_t site;       /* reachable face the crew can work from */
    int32_t obstacle;   /* the thing actually in the way */
    int32_t target;     /* where the route wants to arrive */
    int32_t load;       /* load class that failed */
    int32_t priority;
    int32_t cost;
    char problem[112];
} UrRequest;

typedef struct UrCrew {
    char name[24];       /* "the Red colony" */
    const char *colour;  /* Red, Purple, Blue */
    int32_t lair;        /* the camp itself */
    int32_t strong;      /* its strongroom, and its destination with no dragon */
    int32_t workers;
    int32_t hands_max;   /* what the colony grows back to */
    int32_t reserve;     /* workers held back from hauling */
    int32_t supplies;    /* timber, rope, planking */
    int32_t state;
    UrRequest job;
    int32_t heading;     /* committed cardinal direction */
    int32_t remaining;   /* tiles left in the committed heading */
    int32_t at;          /* current working face */
    int32_t commit;      /* years left before reassessing the job */
    int64_t stored;      /* crowns sitting in its own strongroom */
    int64_t tribute;     /* crowns handed to the dragon */
    int32_t deliveries;
    int32_t hunted;      /* workers the dragon took for coming second */
    int32_t raids_made;
    int32_t raids_suffered;
    int32_t tiles_cut;
    int32_t progress;    /* work actually done this year */
    int32_t idle_years;  /* years on this job with nothing to show */
} UrCrew;

typedef struct UrEvent {
    int16_t year;
    uint8_t season;
    uint8_t injected;
    char text[176];
} UrEvent;

typedef struct UrSnap {
    int16_t year;
    uint64_t hash;            /* the mountain's state at that year */
    char label[44];
    uint8_t kind[UR_CELLS];   /* the glyph, as the map draws it */
    uint8_t crew[UR_CELLS];   /* 0 nobody, else the colony that cut it */
    bool dragon;              /* was there still a dragon to pay? */
} UrSnap;

typedef struct UrWorld {
    UrTile tiles[UR_CELLS];
    uint32_t rng;        /* this world's history */
    uint32_t rock;       /* the mountain, identical in every world */
    uint32_t seed;
    int32_t ancient_tiles;
    int32_t snap_at;     /* take a verification snapshot at this year */
    int32_t year;
    int32_t season;
    int32_t canyon_x;
    int32_t mouth_cell;
    int32_t hoard_cell;
    int32_t spoil;
    int64_t delivered;
    int32_t deliveries;
    int32_t crates_delivered;
    int32_t waits;
    int32_t collapses;
    int32_t failure_year;
    int32_t recovered;
    int32_t rebuilds;
    bool hosted;              /* a world simulation owns loot, hands and dragon */
    char host_note[128];      /* what that world was, for the record */
    int32_t dragon_dies;      /* -1 never, 0 no dragon at all, else the year */
    bool dragon_alive;
    int32_t crown;            /* colony wearing the dragon's favour, -1 none */
    int32_t crown_year;
    int32_t raids;
    int32_t barricades;
    int32_t sabotage;
    struct {
        bool ok;
        bool skipped;
        char label[56];
        char detail[100];
    } checks[16];
    int32_t check_count;
    UrBridge bridges[UR_MAX_BRIDGES];
    int32_t bridge_count;
    UrNode nodes[UR_MAX_NODES];
    int32_t node_count;
    UrSource sources[UR_SOURCES];
    UrCrew crews[UR_CREWS];
    UrEvent events[UR_MAX_EVENTS];
    int32_t event_count;
    UrSnap snaps[UR_MAX_SNAPS];
    int32_t snap_count;
    bool inject;
    int32_t inject_year;
    bool injected_done;
} UrWorld;

static const char *const UrSeasonName[4] = {
    "Early Spring", "High Summer", "Late Autumn", "Deep Winter"
};

/* ---------------------------------------------------------------- random */

static uint32_t UrNext(uint32_t *state)
{
    uint32_t x = *state;
    x ^= x << 13U;
    x ^= x >> 17U;
    x ^= x << 5U;
    if (x == 0U) x = 0x1b873593U;
    *state = x;
    return x;
}

static int32_t UrRange(uint32_t *state, int32_t lo, int32_t hi)
{
    if (hi <= lo) return lo;
    return lo + (int32_t)(UrNext(state) % (uint32_t)(hi - lo + 1));
}

static bool UrChance(uint32_t *state, int32_t percent)
{
    return UrRange(state, 1, 100) <= percent;
}

static uint32_t UrHash(int32_t a, int32_t b, int32_t c, uint32_t seed)
{
    uint32_t h = seed ^ 0x9e3779b9U;
    h ^= (uint32_t)(a + 4096) * 0x85ebca6bU;
    h = (h << 13U) | (h >> 19U);
    h ^= (uint32_t)(b + 4096) * 0xc2b2ae35U;
    h = (h << 7U) | (h >> 25U);
    h ^= (uint32_t)(c + 4096) * 0x27d4eb2fU;
    h ^= h >> 15U;
    return h;
}

/* ----------------------------------------------------------------- cells */

static int32_t UrCellAt(int32_t layer, int32_t x, int32_t y)
{
    return (layer * UR_H + y) * UR_W + x;
}

static int32_t UrLayerOf(int32_t cell) { return cell / UR_LAYER_CELLS; }
static int32_t UrXOf(int32_t cell) { return cell % UR_W; }
static int32_t UrYOf(int32_t cell) { return (cell / UR_W) % UR_H; }

static bool UrInside(int32_t layer, int32_t x, int32_t y)
{
    return layer >= 0 && layer < UR_LAYERS && x >= 0 && x < UR_W &&
           y >= 0 && y < UR_H;
}

static const int32_t UrDirX[4] = { 0, 1, 0, -1 };
static const int32_t UrDirY[4] = { -1, 0, 1, 0 };
static const char *const UrDirName[4] = { "north", "east", "south", "west" };

static int32_t UrStep(int32_t cell, int32_t dir)
{
    int32_t x = UrXOf(cell) + UrDirX[dir];
    int32_t y = UrYOf(cell) + UrDirY[dir];
    int32_t layer = UrLayerOf(cell);
    if (!UrInside(layer, x, y)) return -1;
    return UrCellAt(layer, x, y);
}

static int32_t UrBelow(int32_t cell)
{
    int32_t layer = UrLayerOf(cell);
    if (layer + 1 >= UR_LAYERS) return -1;
    return cell + UR_LAYER_CELLS;
}

static int32_t UrAbove(int32_t cell)
{
    if (UrLayerOf(cell) == 0) return -1;
    return cell - UR_LAYER_CELLS;
}

static int32_t UrDistance(int32_t a, int32_t b)
{
    int32_t dx = UrXOf(a) - UrXOf(b);
    int32_t dy = UrYOf(a) - UrYOf(b);
    int32_t dz = UrLayerOf(a) - UrLayerOf(b);
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    if (dz < 0) dz = -dz;
    return dx + dy + dz * 6;
}

/* ------------------------------------------------------------- reporting */

static void UrLog(UrWorld *w, bool injected, const char *fmt, ...);

static void UrLogv(UrWorld *w, bool injected, const char *fmt, va_list args)
{
    if (w->event_count >= UR_MAX_EVENTS) return;
    UrEvent *event = &w->events[w->event_count++];
    event->year = (int16_t)w->year;
    event->season = (uint8_t)w->season;
    event->injected = injected ? 1U : 0U;
    (void)vsnprintf(event->text, sizeof(event->text), fmt, args);
}

static void UrLog(UrWorld *w, bool injected, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    UrLogv(w, injected, fmt, args);
    va_end(args);
}

/* ------------------------------------------------------------ the mountain
 *
 * Before any goblin lifts a pick: hardness, stability, irregular natural
 * caverns, one canyon that cuts three layers, natural fissures between
 * layers, and the old human mine at the top. Layer 0 is the mine level;
 * layer 3 is the deep road where the hoard lies.
 */

static void UrCarveCavern(UrWorld *w, int32_t layer, int32_t cx, int32_t cy,
                          int32_t budget)
{
    int32_t x = cx;
    int32_t y = cy;
    for (int32_t i = 0; i < budget; ++i) {
        if (!UrInside(layer, x, y) || x < 1 || y < 1 || x >= UR_W - 1 ||
            y >= UR_H - 1) {
            x = cx;
            y = cy;
            continue;
        }
        UrTile *tile = &w->tiles[UrCellAt(layer, x, y)];
        if (tile->kind == UR_ROCK) {
            tile->kind = (uint8_t)UR_CAVERN;
            tile->purpose = (uint8_t)UR_PURPOSE_NATURAL;
            tile->year = -1;
        }
        int32_t dir = UrRange(&w->rock, 0, 3);
        x += UrDirX[dir];
        y += UrDirY[dir];
        if (UrChance(&w->rock, 18)) {
            x = cx + UrRange(&w->rock, -3, 3);
            y = cy + UrRange(&w->rock, -2, 2);
        }
    }
}

static bool UrWalkableKind(int32_t kind)
{
    return kind == UR_CAVERN || kind == UR_CUT || kind == UR_CHAMBER ||
           kind == UR_RECESS || kind == UR_BRIDGE || kind == UR_CACHE ||
           kind == UR_SOURCE || kind == UR_HOARD || kind == UR_MOUTH ||
           kind == UR_LAIR || kind == UR_STRONG;
}

static bool UrWalkable(const UrWorld *w, int32_t cell)
{
    if (cell < 0 || cell >= UR_CELLS) return false;
    return UrWalkableKind(w->tiles[cell].kind);
}

static void UrRecomputeClearance(UrWorld *w)
{
    for (int32_t cell = 0; cell < UR_CELLS; ++cell) {
        UrTile *tile = &w->tiles[cell];
        if (tile->kind != UR_CAVERN) continue;
        int32_t open = 0;
        for (int32_t dir = 0; dir < 4; ++dir) {
            int32_t next = UrStep(cell, dir);
            if (next >= 0 && UrWalkable(w, next)) ++open;
        }
        tile->clear = (uint8_t)(open >= 4 ? 3 : (open >= 3 ? 2 : 1));
    }
}

static void UrCarveRoom(UrWorld *w, int32_t layer, int32_t x0, int32_t y0,
                        int32_t x1, int32_t y1, int32_t kind, int32_t clear,
                        int32_t purpose)
{
    for (int32_t y = y0; y <= y1; ++y) {
        for (int32_t x = x0; x <= x1; ++x) {
            if (!UrInside(layer, x, y)) continue;
            UrTile *tile = &w->tiles[UrCellAt(layer, x, y)];
            tile->kind = (uint8_t)kind;
            tile->clear = (uint8_t)clear;
            tile->purpose = (uint8_t)purpose;
            tile->year = -1;
        }
    }
}

static void UrCarveLine(UrWorld *w, int32_t layer, int32_t x0, int32_t y0,
                        int32_t x1, int32_t y1, int32_t kind, int32_t clear,
                        int32_t purpose)
{
    int32_t x = x0;
    int32_t y = y0;
    while (x != x1 || y != y1) {
        if (UrInside(layer, x, y)) {
            UrTile *tile = &w->tiles[UrCellAt(layer, x, y)];
            if (tile->kind == UR_ROCK || tile->kind == UR_RUBBLE) {
                tile->kind = (uint8_t)kind;
                tile->clear = (uint8_t)clear;
                tile->purpose = (uint8_t)purpose;
                tile->year = -1;
            }
        }
        if (x != x1) x += (x1 > x) ? 1 : -1;
        else if (y != y1) y += (y1 > y) ? 1 : -1;
    }
    if (UrInside(layer, x1, y1)) {
        UrTile *tile = &w->tiles[UrCellAt(layer, x1, y1)];
        if (tile->kind == UR_ROCK || tile->kind == UR_RUBBLE) {
            tile->kind = (uint8_t)kind;
            tile->clear = (uint8_t)clear;
            tile->purpose = (uint8_t)purpose;
            tile->year = -1;
        }
    }
}

static void UrAddNode(UrWorld *w, int32_t cell, int32_t kind,
                      const char *name)
{
    if (w->node_count >= UR_MAX_NODES || cell < 0) return;
    for (int32_t i = 0; i < w->node_count; ++i) {
        if (UrDistance(w->nodes[i].cell, cell) <= 3) return;
    }
    UrNode *node = &w->nodes[w->node_count++];
    node->cell = cell;
    node->kind = (uint8_t)kind;
    (void)snprintf(node->name, sizeof(node->name), "%s", name);
}

static void UrPlaceName(char *out, size_t capacity, int32_t cell,
                        uint32_t seed, const char *suffix)
{
    static const char *const first[] = {
        "Pale", "Ash", "Cinder", "Slack", "Bent", "Low", "Rust", "Dead",
        "Salt", "Black", "Long", "Cold", "Broken", "Quiet", "Green"
    };
    static const char *const second[] = {
        "Gallery", "Hollow", "Reach", "Bench", "Turn", "Cut", "Rim",
        "Landing", "Crook", "Stair", "Drift", "Sump"
    };
    uint32_t h = UrHash(UrXOf(cell), UrYOf(cell), UrLayerOf(cell), seed);
    const char *a = first[h % (sizeof(first) / sizeof(first[0]))];
    const char *b = second[(h >> 8U) % (sizeof(second) / sizeof(second[0]))];
    if (suffix != NULL) {
        (void)snprintf(out, capacity, "%s %s %s", a, b, suffix);
    } else {
        (void)snprintf(out, capacity, "%s %s", a, b);
    }
}

static void UrBuildMountain(UrWorld *w)
{
    int32_t mid = UR_H / 2;

    /* Bedded rock: bands that run through the mountain, cut across by the
     * odd hard vein. Crews will follow the soft beds without being told to,
     * because soft rock is simply cheaper to answer a problem with. */
    for (int32_t layer = 0; layer < UR_LAYERS; ++layer) {
        for (int32_t y = 0; y < UR_H; ++y) {
            int32_t bed = (y + layer * 5) / 3;
            uint32_t band = UrHash(bed, layer, 11, UR_MOUNTAIN_SEED);
            int32_t stone = (int32_t)(band % 4U);
            for (int32_t x = 0; x < UR_W; ++x) {
                UrTile *tile = &w->tiles[UrCellAt(layer, x, y)];
                uint32_t h = UrHash(x / 3, y / 2, layer, UR_MOUNTAIN_SEED);
                int32_t local = stone;
                if ((h % 11U) == 0U) local = UR_STONE_QUARTZ;   /* a vein */
                else if ((h % 7U) == 0U) {
                    local = (local + 1) % UR_STONE_GRANITE;
                }
                static const int32_t cost[UR_STONE_COUNT] = { 2, 4, 5, 9, 12 };
                static const int32_t firm[UR_STONE_COUNT] = { 1, 4, 5, 8, 8 };
                tile->stone = (uint8_t)local;
                tile->kind = (uint8_t)UR_ROCK;
                tile->hardness = (uint8_t)(cost[local] + (int32_t)((h >> 5U) % 3U) +
                                           layer / 2);
                tile->stability = (uint8_t)(firm[local] +
                                            (int32_t)((h >> 9U) % 3U) - 1);
                tile->clear = 0;
                tile->year = -1;
                tile->bridge = -1;
                tile->purpose = (uint8_t)UR_PURPOSE_NATURAL;
            }
        }
    }

    /* One named unstable section: bad ground the crews will learn about. */
    for (int32_t y = mid - 5; y <= mid + 2; ++y) {
        for (int32_t x = 34; x <= 44; ++x) {
            if (!UrInside(2, x, y)) continue;
            UrTile *tile = &w->tiles[UrCellAt(2, x, y)];
            tile->stability = 1U;
            tile->flags |= UR_FLAG_UNSTABLE;
        }
    }

    /* Natural caverns: irregular, not on any grid. */
    for (int32_t layer = 0; layer < UR_LAYERS; ++layer) {
        /* Sparse and unconnected on purpose: the spaces between these are
         * what the goblins spend a century answering. */
        int32_t blobs = 5 + layer;
        for (int32_t i = 0; i < blobs; ++i) {
            int32_t cx = UrRange(&w->rock, 6, UR_W - 7);
            int32_t cy = UrRange(&w->rock, 3, UR_H - 4);
            UrCarveCavern(w, layer, cx, cy, UrRange(&w->rock, 6, 22));
        }
        /* Solution pockets: a few tiles of nothing, scattered through the
         * limestone, worth finding and not worth much. */
        for (int32_t i = 0; i < 22; ++i) {
            int32_t cx = UrRange(&w->rock, 3, UR_W - 4);
            int32_t cy = UrRange(&w->rock, 2, UR_H - 3);
            if (w->tiles[UrCellAt(layer, cx, cy)].stone != UR_STONE_LIMESTONE) {
                continue;
            }
            UrCarveCavern(w, layer, cx, cy, UrRange(&w->rock, 2, 6));
        }
    }

    /* The canyon: open air on layers 1 and 2, its floor on layer 3. */
    w->canyon_x = UR_W * 7 / 16;
    int32_t cx = w->canyon_x;
    for (int32_t y = 0; y < UR_H; ++y) {
        uint32_t h = UrHash(y, 0, 77, UR_MOUNTAIN_SEED);
        cx += (int32_t)(h % 3U) - 1;
        if (cx < w->canyon_x - 3) cx = w->canyon_x - 3;
        if (cx > w->canyon_x + 3) cx = w->canyon_x + 3;
        int32_t width = 3 + (int32_t)((h >> 8U) % 4U);
        for (int32_t x = cx; x < cx + width; ++x) {
            for (int32_t layer = 1; layer <= 2; ++layer) {
                if (!UrInside(layer, x, y)) continue;
                UrTile *tile = &w->tiles[UrCellAt(layer, x, y)];
                tile->kind = (uint8_t)UR_VOID;
                tile->clear = 0;
                tile->purpose = (uint8_t)UR_PURPOSE_NATURAL;
            }
            if (UrInside(3, x, y)) {
                UrTile *floor_tile = &w->tiles[UrCellAt(3, x, y)];
                floor_tile->kind = (uint8_t)UR_CAVERN;
                floor_tile->clear = 3U;
                floor_tile->purpose = (uint8_t)UR_PURPOSE_NATURAL;
            }
        }
        /* Ledges along both rims, where the crews will stand and look. */
        for (int32_t layer = 1; layer <= 2; ++layer) {
            if (UrInside(layer, cx - 1, y)) {
                UrTile *tile = &w->tiles[UrCellAt(layer, cx - 1, y)];
                if (tile->kind == UR_ROCK) {
                    tile->kind = (uint8_t)UR_CAVERN;
                    tile->clear = 2U;
                }
            }
            if (UrInside(layer, cx + width, y)) {
                UrTile *tile = &w->tiles[UrCellAt(layer, cx + width, y)];
                if (tile->kind == UR_ROCK) {
                    tile->kind = (uint8_t)UR_CAVERN;
                    tile->clear = 2U;
                }
            }
        }
    }

    /* Natural fissures between layers: a scout can get down one, a porter
     * with a pack cannot, and nobody carries a crate through. */
    for (int32_t layer = 0; layer + 1 < UR_LAYERS; ++layer) {
        int32_t placed = 0;
        for (int32_t attempt = 0; attempt < 1200 && placed < 9; ++attempt) {
            int32_t x = UrRange(&w->rock, 2, UR_W - 3);
            int32_t y = UrRange(&w->rock, 2, UR_H - 3);
            int32_t cell = UrCellAt(layer, x, y);
            int32_t under = UrBelow(cell);
            if (!UrWalkable(w, cell) || under < 0 || !UrWalkable(w, under)) {
                continue;
            }
            if (w->tiles[cell].vert != UR_VERT_NONE) continue;
            w->tiles[cell].vert = (uint8_t)UR_VERT_FISSURE;
            ++placed;
        }
    }

    /* The old human mine: the six-room block the company already knows. */
    UrCarveRoom(w, 0, 1, mid - 1, 5, mid + 1, UR_MOUTH, 3,
                UR_PURPOSE_MINE);
    UrCarveRoom(w, 0, 9, mid - 1, 13, mid + 1, UR_CUT, 3, UR_PURPOSE_MINE);
    UrCarveRoom(w, 0, 16, mid - 4, 20, mid - 2, UR_CUT, 2, UR_PURPOSE_MINE);
    UrCarveRoom(w, 0, 16, mid + 2, 20, mid + 4, UR_CUT, 2, UR_PURPOSE_MINE);
    UrCarveRoom(w, 0, 9, mid - 5, 12, mid - 3, UR_CUT, 2, UR_PURPOSE_MINE);
    UrCarveRoom(w, 0, 23, mid - 1, 26, mid + 1, UR_CHAMBER, 3,
                UR_PURPOSE_MINE);
    UrCarveLine(w, 0, 5, mid, 9, mid, UR_CUT, 3, UR_PURPOSE_MINE);
    UrCarveLine(w, 0, 13, mid, 16, mid - 3, UR_CUT, 2, UR_PURPOSE_MINE);
    UrCarveLine(w, 0, 13, mid, 16, mid + 3, UR_CUT, 2, UR_PURPOSE_MINE);
    UrCarveLine(w, 0, 11, mid - 1, 11, mid - 3, UR_CUT, 2, UR_PURPOSE_MINE);
    UrCarveLine(w, 0, 20, mid - 3, 23, mid, UR_CUT, 2, UR_PURPOSE_MINE);
    UrCarveLine(w, 0, 20, mid + 3, 23, mid, UR_CUT, 2, UR_PURPOSE_MINE);

    w->mouth_cell = UrCellAt(0, 2, mid);
    UrAddNode(w, w->mouth_cell, UR_MOUTH, "Mine Mouth");
    UrAddNode(w, UrCellAt(0, 11, mid), UR_CUT, "Lamp Hall");
    UrAddNode(w, UrCellAt(0, 18, mid - 3), UR_CUT, "Broken Weighhouse");
    UrAddNode(w, UrCellAt(0, 18, mid + 3), UR_CUT, "Sump Gallery");
    UrAddNode(w, UrCellAt(0, 10, mid - 4), UR_CUT, "King's Survey Room");
    UrAddNode(w, UrCellAt(0, 24, mid), UR_CHAMBER, "Winch Landing");

    /* The old winch is the only worked way down out of the human mine. */
    {
        int32_t landing = UrCellAt(0, 24, mid);
        int32_t under = UrBelow(landing);
        if (under >= 0) {
            UrTile *tile = &w->tiles[under];
            if (!UrWalkableKind(tile->kind)) {
                UrCarveRoom(w, 1, 23, mid - 1, 25, mid + 1, UR_CAVERN, 2,
                            UR_PURPOSE_NATURAL);
            }
            w->tiles[landing].vert = (uint8_t)UR_VERT_LADDER;
            w->tiles[landing].purpose = (uint8_t)UR_PURPOSE_DESCENT;
        }
    }

    /* The hoard: far side of the canyon, deepest layer. */
    UrCarveRoom(w, 3, UR_W - 9, mid - 2, UR_W - 4, mid + 2, UR_CAVERN, 3,
                UR_PURPOSE_NATURAL);
    w->hoard_cell = UrCellAt(3, UR_W - 6, mid);
    w->tiles[w->hoard_cell].kind = (uint8_t)UR_HOARD;
    w->tiles[w->hoard_cell].clear = 3U;
    UrAddNode(w, w->hoard_cell, UR_HOARD, "The Hoard");

    UrRecomputeClearance(w);

    /* Where a colony settles: a cavern big enough to camp in, its
     * strongroom at the back. Three colonies, spread across the mountain,
     * so at least one of them has the canyon between it and everything. */
    {
        static const char *const colours[UR_FACTIONS] = { "Red", "Purple", "Blue" };
        int32_t regions[UR_FACTIONS][3] = {
            { 1, 6, w->canyon_x - 7 },
            { 2, w->canyon_x + 6, UR_W - 9 },
            { 3, 7, w->canyon_x - 5 }
        };
        for (int32_t i = 0; i < UR_FACTIONS; ++i) {
            int32_t layer = regions[i][0];
            int32_t site = -1;
            for (int32_t attempt = 0; attempt < 5000 && site < 0; ++attempt) {
                int32_t x = UrRange(&w->rng, regions[i][1] + 1, regions[i][2] - 1);
                int32_t y = UrRange(&w->rng, 3, UR_H - 4);
                int32_t cell = UrCellAt(layer, x, y);
                if (w->tiles[cell].kind == UR_CAVERN) site = cell;
            }
            if (site < 0) {
                site = UrCellAt(layer, (regions[i][1] + regions[i][2]) / 2,
                                UR_H / 2);
            }
            int32_t sx = UrXOf(site);
            int32_t sy = UrYOf(site);
            for (int32_t ly = sy - 1; ly <= sy + 1; ++ly) {
                for (int32_t lx = sx - 2; lx <= sx + 2; ++lx) {
                    if (!UrInside(layer, lx, ly)) continue;
                    UrTile *tile = &w->tiles[UrCellAt(layer, lx, ly)];
                    if (tile->kind == UR_VOID) continue;   /* not over air */
                    if (tile->kind == UR_SOURCE || tile->kind == UR_HOARD) {
                        continue;                          /* nor over loot */
                    }
                    tile->kind = (uint8_t)UR_LAIR;
                    tile->clear = 3U;
                    tile->year = -1;
                }
            }
            w->tiles[site].kind = (uint8_t)UR_STRONG;
            w->tiles[site].clear = 3U;
            w->crews[i].lair = UrCellAt(layer, sx - 2, sy);
            w->crews[i].strong = site;
            w->crews[i].colour = colours[i];
            (void)snprintf(w->crews[i].name, sizeof(w->crews[i].name),
                           "the %s colony", colours[i]);
            char name[36];
            (void)snprintf(name, sizeof(name), "%s Colony", colours[i]);
            UrAddNode(w, w->crews[i].lair, UR_LAIR, name);
            (void)snprintf(name, sizeof(name), "%s Strongroom", colours[i]);
            UrAddNode(w, site, UR_STRONG, name);
        }
    }

    /* The ancient Underroad: a goblin nation bigger than anything living
     * here now drove these ways between the caverns, and left. It is part
     * of the mountain, identical in every world; what the present colonies
     * did to it is not. Without it, twelve goblins would have to have dug a
     * city, which is the arithmetic that does not work. */
    {
        int32_t anchors[24];
        int32_t anchor_count = 0;
        for (int32_t i = 0; i < w->node_count && anchor_count < 24; ++i) {
            anchors[anchor_count++] = w->nodes[i].cell;
        }
        for (int32_t layer = 0; layer < UR_LAYERS && anchor_count < 24; ++layer) {
            for (int32_t attempt = 0; attempt < 400 && anchor_count < 24;
                 ++attempt) {
                int32_t x = UrRange(&w->rock, 4, UR_W - 5);
                int32_t y = UrRange(&w->rock, 2, UR_H - 3);
                int32_t cell = UrCellAt(layer, x, y);
                if (w->tiles[cell].kind == UR_CAVERN) {
                    anchors[anchor_count++] = cell;
                }
            }
        }
        for (int32_t i = 0; i + 1 < anchor_count; ++i) {
            int32_t a = anchors[i];
            int32_t b = anchors[i + 1];
            if (UrLayerOf(a) != UrLayerOf(b)) {
                /* An old winze where the road changes level. */
                int32_t upper = UrLayerOf(a) < UrLayerOf(b) ? a : b;
                if (w->tiles[upper].vert == UR_VERT_NONE) {
                    w->tiles[upper].vert = (uint8_t)UR_VERT_LADDER;
                    w->tiles[upper].purpose = (uint8_t)UR_PURPOSE_ANCIENT;
                }
                continue;
            }
            int32_t at = a;
            int32_t guard = 0;
            while (at != b && guard++ < 70) {
                int32_t dx = UrXOf(b) - UrXOf(at);
                int32_t dy = UrYOf(b) - UrYOf(at);
                int32_t dir;
                if ((dx > 0 ? dx : -dx) >= (dy > 0 ? dy : -dy)) {
                    dir = dx > 0 ? 1 : 3;
                } else {
                    dir = dy > 0 ? 2 : 0;
                }
                if (UrChance(&w->rock, 22)) {
                    dir = (dir + (UrChance(&w->rock, 50) ? 1 : 3)) % 4;
                }
                int32_t next = UrStep(at, dir);
                if (next < 0) break;
                UrTile *tile = &w->tiles[next];
                if (tile->kind == UR_VOID) break;   /* the canyon stopped them too */
                if (tile->kind == UR_ROCK) {
                    tile->kind = (uint8_t)UR_CUT;
                    tile->clear = (uint8_t)(UrChance(&w->rock, 40) ? 3 : 2);
                    tile->purpose = (uint8_t)UR_PURPOSE_ANCIENT;
                    tile->year = -1;
                    tile->crew = 0;
                    w->ancient_tiles += 1;
                    /* Some of it has come down in the meantime. */
                    if (UrChance(&w->rock, 6)) {
                        tile->kind = (uint8_t)UR_RUBBLE;
                        tile->clear = 0;
                        tile->flags |= UR_FLAG_COLLAPSED;
                    }
                }
                at = next;
            }
        }
    }

    UrRecomputeClearance(w);

    /* Known loot: the raiders' stage inside the mine mouth, which keeps
     * being restocked, and four older caches the colonies race for. */
    {
        int32_t stage = UrCellAt(0, 4, mid + 1);
        w->tiles[stage].kind = (uint8_t)UR_SOURCE;
        w->tiles[stage].clear = 3U;
        (void)snprintf(w->sources[0].name, sizeof(w->sources[0].name), "%s",
                       "Raiders' Stage");
        w->sources[0].cell = stage;
        w->sources[0].crowns = 260;
        w->sources[0].crates = 0;
        w->sources[0].replenished = true;
        UrAddNode(w, stage, UR_SOURCE, "Raiders' Stage");

        static const struct {
            const char *name;
            int32_t layer;
            int32_t crowns;
            int32_t crates;
        } caches[UR_SOURCES - 1] = {
            { "The Wreck Below", 2, 190, 2 },
            { "Drowned Paychest", 1, 150, 0 },
            { "Barrow Shelf", 3, 170, 1 },
            { "Tax Cart Spill", 1, 120, 0 }
        };
        for (int32_t i = 0; i < UR_SOURCES - 1; ++i) {
            int32_t cell = -1;
            for (int32_t attempt = 0; attempt < 6000 && cell < 0; ++attempt) {
                int32_t x = UrRange(&w->rock, 4, UR_W - 5);
                int32_t y = UrRange(&w->rock, 2, UR_H - 3);
                int32_t candidate = UrCellAt(caches[i].layer, x, y);
                if (w->tiles[candidate].kind == UR_CAVERN &&
                    w->tiles[candidate].clear >= 2) {
                    cell = candidate;
                }
            }
            if (cell < 0) cell = UrCellAt(caches[i].layer, 10 + i * 8, mid);
            UrCarveRoom(w, caches[i].layer, UrXOf(cell) - 1, UrYOf(cell) - 1,
                        UrXOf(cell) + 1, UrYOf(cell) + 1, UR_CAVERN, 3,
                        UR_PURPOSE_NATURAL);
            w->tiles[cell].kind = (uint8_t)UR_SOURCE;
            w->tiles[cell].clear = 3U;
            UrSource *source = &w->sources[i + 1];
            (void)snprintf(source->name, sizeof(source->name), "%s",
                           caches[i].name);
            source->cell = cell;
            source->crowns = caches[i].crowns;
            source->crates = caches[i].crates;
            UrAddNode(w, cell, UR_SOURCE, caches[i].name);
        }
    }

    /* Name the larger natural caverns so the crawl has places, not tiles. */
    for (int32_t layer = 0; layer < UR_LAYERS; ++layer) {
        for (int32_t y = 2; y < UR_H - 2; y += 4) {
            for (int32_t x = 3; x < UR_W - 3; x += 6) {
                int32_t cell = UrCellAt(layer, x, y);
                if (w->tiles[cell].kind != UR_CAVERN) continue;
                if (w->tiles[cell].clear < 3) continue;
                char name[36];
                UrPlaceName(name, sizeof(name), cell, UR_MOUNTAIN_SEED, NULL);
                UrAddNode(w, cell, UR_CAVERN, name);
            }
        }
    }
}

static const UrNode *UrNodeNear(const UrWorld *w, int32_t cell,
                                int32_t radius)
{
    const UrNode *best = NULL;
    int32_t best_distance = radius + 1;
    for (int32_t i = 0; i < w->node_count; ++i) {
        if (UrLayerOf(w->nodes[i].cell) != UrLayerOf(cell)) continue;
        int32_t distance = UrDistance(w->nodes[i].cell, cell);
        if (distance < best_distance) {
            best_distance = distance;
            best = &w->nodes[i];
        }
    }
    return best;
}

/* ------------------------------------------------- load-aware connectivity
 *
 * Route feasibility depends on the load, not the goblin. An unladen scout
 * squeezes anywhere; a porter needs a cut passage and a built descent; a
 * bulky crate needs a wide passage, a turning chamber to change heading,
 * a ramp instead of a ladder, and a crossing rated to hold it.
 */

static const char *const UrLoadName[UR_LOAD_COUNT] = {
    "scout", "porter pack", "bulky crate"
};

static int32_t UrBridgeCondition(const UrWorld *w, int32_t cell)
{
    int32_t index = w->tiles[cell].bridge;
    if (index < 0 || index >= w->bridge_count) return 0;
    return w->bridges[index].condition;
}

static int32_t UrBridgeCapacity(const UrWorld *w, int32_t cell)
{
    int32_t index = w->tiles[cell].bridge;
    if (index < 0 || index >= w->bridge_count) return 0;
    return w->bridges[index].capacity;
}

/* `known` is a colony's knowledge mask, or 0 for "ask the rock, not the
 * rumour" — the company's own collision uses 0. */
static bool UrEnterOk(const UrWorld *w, int32_t cell, int32_t load,
                      uint32_t known)
{
    if (cell < 0 || cell >= UR_CELLS) return false;
    const UrTile *tile = &w->tiles[cell];
    if (!UrWalkableKind(tile->kind)) return false;
    if (known != 0U && ((uint32_t)tile->goblin_known & known) == 0U) {
        return false;
    }
    if (tile->kind == UR_BRIDGE) {
        int32_t condition = UrBridgeCondition(w, cell);
        int32_t capacity = UrBridgeCapacity(w, cell);
        if (load == UR_LOAD_SCOUT) return condition >= 25;
        if (load == UR_LOAD_PORTER) return condition >= 45 && capacity >= 2;
        return condition >= 65 && capacity >= 3;
    }
    if (load == UR_LOAD_SCOUT) return tile->clear >= 1;
    return tile->clear >= 2;
}

/* A crate can be carried straight down a passage it cannot be turned in. */
static bool UrTurnOk(const UrWorld *w, int32_t cell, int32_t load)
{
    if (load != UR_LOAD_CRATE) return true;
    const UrTile *tile = &w->tiles[cell];
    return tile->clear >= 3;
}

static bool UrVertOk(int32_t vert, int32_t load)
{
    if (vert == UR_VERT_NONE) return false;
    if (load == UR_LOAD_SCOUT) return true;
    if (load == UR_LOAD_PORTER) {
        return vert == UR_VERT_LADDER || vert == UR_VERT_RAMP;
    }
    return vert == UR_VERT_RAMP;
}

#define UR_STATES (UR_CELLS * 4)

static int32_t g_prev[UR_STATES];
static int32_t g_queue[UR_STATES];
static uint8_t g_visited[UR_STATES];

/* Breadth-first over (cell, heading) so a heading change is a real cost for
 * loads that cannot turn in a narrow passage. Returns path length or -1. */
static int32_t UrRoute(const UrWorld *w, int32_t from, int32_t to,
                       int32_t load, uint32_t known, int32_t *path,
                       int32_t path_capacity)
{
    if (from < 0) return -1;   /* to < 0 means: explore, never arrive */
    if (!UrEnterOk(w, from, load, known)) return -1;
    memset(g_visited, 0, sizeof(g_visited));
    int32_t head = 0;
    int32_t tail = 0;
    for (int32_t dir = 0; dir < 4; ++dir) {
        int32_t state = from * 4 + dir;
        g_visited[state] = 1U;
        g_prev[state] = -1;
        g_queue[tail++] = state;
    }
    int32_t found = -1;
    while (head < tail && found < 0) {
        int32_t state = g_queue[head++];
        int32_t cell = state / 4;
        int32_t dir = state % 4;
        if (cell == to) { found = state; break; }
        for (int32_t next_dir = 0; next_dir < 4; ++next_dir) {
            if (next_dir != dir && !UrTurnOk(w, cell, load)) continue;
            int32_t next = UrStep(cell, next_dir);
            if (next < 0 || !UrEnterOk(w, next, load, known)) continue;
            int32_t next_state = next * 4 + next_dir;
            if (g_visited[next_state]) continue;
            g_visited[next_state] = 1U;
            g_prev[next_state] = state;
            g_queue[tail++] = next_state;
        }
        int32_t down = UrBelow(cell);
        if (down >= 0 && UrVertOk(w->tiles[cell].vert, load) &&
            UrEnterOk(w, down, load, known)) {
            int32_t next_state = down * 4 + dir;
            if (!g_visited[next_state]) {
                g_visited[next_state] = 1U;
                g_prev[next_state] = state;
                g_queue[tail++] = next_state;
            }
        }
        int32_t up = UrAbove(cell);
        if (up >= 0 && UrVertOk(w->tiles[up].vert, load) &&
            UrEnterOk(w, up, load, known)) {
            int32_t next_state = up * 4 + dir;
            if (!g_visited[next_state]) {
                g_visited[next_state] = 1U;
                g_prev[next_state] = state;
                g_queue[tail++] = next_state;
            }
        }
    }
    if (found < 0) return -1;
    int32_t length = 0;
    for (int32_t state = found; state >= 0; state = g_prev[state]) ++length;
    if (path != NULL) {
        /* A (cell, heading) path can outlast a plain cell buffer; refuse
         * rather than hand back a length the caller would walk off. */
        if (length > path_capacity) return -1;
        int32_t index = length;
        for (int32_t state = found; state >= 0; state = g_prev[state]) {
            path[--index] = state / 4;
        }
    }
    return length;
}

static void UrReachable(const UrWorld *w, int32_t from, int32_t load,
                        uint32_t known, uint8_t *out)
{
    memset(out, 0, (size_t)UR_CELLS);
    if (from < 0 || !UrEnterOk(w, from, load, known)) return;
    (void)UrRoute(w, from, -2, load, known, NULL, 0);
    for (int32_t cell = 0; cell < UR_CELLS; ++cell) {
        for (int32_t dir = 0; dir < 4; ++dir) {
            if (g_visited[cell * 4 + dir]) { out[cell] = 1U; break; }
        }
    }
}

/* ------------------------------------------------------ digging distance
 *
 * What would it cost to open a way from here to there? Rock costs its
 * hardness, rubble is cheap to clear, open air needs a structure, and a
 * change of layer needs a worked descent. This is the crews' estimate, not
 * a promise: they will still dig a heading at a time and reassess.
 */

static int32_t g_cost[UR_CELLS];
static int32_t g_heap[UR_CELLS + 1];
static int32_t g_heap_size;

static void UrHeapPush(int32_t cell)
{
    int32_t index = ++g_heap_size;
    g_heap[index] = cell;
    while (index > 1 && g_cost[g_heap[index / 2]] > g_cost[g_heap[index]]) {
        int32_t swap = g_heap[index / 2];
        g_heap[index / 2] = g_heap[index];
        g_heap[index] = swap;
        index /= 2;
    }
}

static int32_t UrHeapPop(void)
{
    int32_t top = g_heap[1];
    g_heap[1] = g_heap[g_heap_size--];
    int32_t index = 1;
    for (;;) {
        int32_t left = index * 2;
        int32_t best = index;
        if (left <= g_heap_size && g_cost[g_heap[left]] < g_cost[g_heap[best]]) {
            best = left;
        }
        if (left + 1 <= g_heap_size &&
            g_cost[g_heap[left + 1]] < g_cost[g_heap[best]]) {
            best = left + 1;
        }
        if (best == index) break;
        int32_t swap = g_heap[best];
        g_heap[best] = g_heap[index];
        g_heap[index] = swap;
        index = best;
    }
    return top;
}

/* Crews cost what they know. Unseen ground is guessed at a flat rate, so a
 * heading can run into a canyon nobody reported and have to be answered. */
static uint32_t g_cost_known = 0U;

static int32_t UrWorkCost(const UrWorld *w, int32_t cell, bool vertical)
{
    const UrTile *tile = &w->tiles[cell];
    int32_t cost;
    if (((uint32_t)tile->goblin_known & g_cost_known) == 0U) {
        cost = 7 + (vertical ? 8 : 0);
        return cost;
    }
    switch (tile->kind) {
        case UR_ROCK: cost = tile->hardness + 2; break;
        case UR_RUBBLE: cost = 3; break;
        case UR_VOID: cost = 14; break;   /* wants a bridge */
        default: cost = 1; break;
    }
    if ((tile->flags & UR_FLAG_UNSTABLE) != 0U) cost += 6;
    if (vertical) cost += 8;              /* a descent has to be worked */
    return cost;
}

/* Cost from every cell to `target`, through rock if need be. */
static void UrDigField(const UrWorld *w, int32_t target, uint32_t known)
{
    g_cost_known = known;
    for (int32_t i = 0; i < UR_CELLS; ++i) g_cost[i] = INT32_MAX;
    g_heap_size = 0;
    if (target < 0) return;
    g_cost[target] = 0;
    UrHeapPush(target);
    while (g_heap_size > 0) {
        int32_t cell = UrHeapPop();
        int32_t base = g_cost[cell];
        for (int32_t dir = 0; dir < 4; ++dir) {
            int32_t next = UrStep(cell, dir);
            if (next < 0) continue;
            int32_t cost = base + UrWorkCost(w, next, false);
            if (cost < g_cost[next]) {
                g_cost[next] = cost;
                UrHeapPush(next);
            }
        }
        int32_t down = UrBelow(cell);
        if (down >= 0) {
            int32_t cost = base + UrWorkCost(w, down, true);
            if (cost < g_cost[down]) {
                g_cost[down] = cost;
                UrHeapPush(down);
            }
        }
        int32_t up = UrAbove(cell);
        if (up >= 0) {
            int32_t cost = base + UrWorkCost(w, up, true);
            if (cost < g_cost[up]) {
                g_cost[up] = cost;
                UrHeapPush(up);
            }
        }
    }
}

/* ------------------------------------------------------------ work orders
 *
 * A work request names a concrete transport problem, and is scored against
 * the others the crews could afford instead:
 *
 *   priority = deliveries unlocked + hauling effort saved - cost - risk
 */

static uint8_t g_reach[UR_CELLS];

/* Where this colony's loot has to end up: the dragon's hoard while there is
 * a dragon to buy favour from, its own strongroom once there is not. */
static int32_t UrDestination(const UrWorld *w, int32_t faction)
{
    return w->dragon_alive ? w->hoard_cell : w->crews[faction].strong;
}

/* `source_index` < 0 means the goal is not a cache at all but a rival's
 * strongroom: with no dragon to pay, somebody else's pile is the nearest
 * loot there is, and wanting it is a reason to dig. */
static bool UrPlanWork(UrWorld *w, int32_t faction, int32_t source_index,
                       int32_t load, int32_t raid_goal, UrRequest *out)
{
    UrSource *source = source_index >= 0 ? &w->sources[source_index] : NULL;
    uint32_t mask = (uint32_t)(1U << (uint32_t)faction);
    UrReachable(w, w->crews[faction].lair, load, mask, g_reach);
    /* Either they cannot get to the loot, or they cannot get it home. */
    int32_t goal = raid_goal >= 0 ? raid_goal :
                   (g_reach[source->cell] ? UrDestination(w, faction) :
                                            source->cell);
    UrDigField(w, goal, mask);

    int32_t site = -1;
    int32_t best = INT32_MAX;
    for (int32_t cell = 0; cell < UR_CELLS; ++cell) {
        if (!g_reach[cell]) continue;
        if (g_cost[cell] == INT32_MAX) continue;
        if (g_cost[cell] < best) {
            best = g_cost[cell];
            site = cell;
        }
    }
    if (site < 0) return false;

    /* Step once down the gradient: that first obstacle names the job. */
    int32_t target = -1;
    int32_t target_cost = g_cost[site];
    bool vertical = false;
    for (int32_t dir = 0; dir < 4; ++dir) {
        int32_t next = UrStep(site, dir);
        if (next >= 0 && g_cost[next] < target_cost) {
            target_cost = g_cost[next];
            target = next;
            vertical = false;
        }
    }
    int32_t down = UrBelow(site);
    if (down >= 0 && g_cost[down] < target_cost) {
        target_cost = g_cost[down];
        target = down;
        vertical = true;
    }
    int32_t up = UrAbove(site);
    if (up >= 0 && g_cost[up] < target_cost) {
        target = up;
        vertical = true;
    }
    if (target < 0) return false;

    UrWorkKind kind;
    const UrTile *tile = &w->tiles[target];
    if (vertical) {
        kind = UR_WORK_DESCENT;
    } else if (tile->kind == UR_VOID) {
        kind = UR_WORK_BRIDGE;
    } else if (tile->kind == UR_RUBBLE) {
        kind = UR_WORK_CLEAR;
    } else if (UrWalkableKind(tile->kind)) {
        kind = (load == UR_LOAD_CRATE) ? UR_WORK_WIDEN : UR_WORK_RECESS;
    } else {
        kind = UR_WORK_DIG;
    }

    /* Tunable game weights, not an economic optimizer: what the job
     * unlocks, what it saves later, what the labour costs, what it risks. */
    int32_t deliveries = source != NULL ?
        (source->crowns / UR_PORTER_LOAD + source->crates * 2 +
         source->stranded) : 24;
    if (deliveries > 60) deliveries = 60;
    int32_t unlocked = deliveries * 3;
    int32_t saved = 40 - best / 6;
    int32_t cost = best / 6 + (kind == UR_WORK_BRIDGE ? 10 : 0);
    int32_t risk = 0;
    if ((w->tiles[target].flags & UR_FLAG_UNSTABLE) != 0U) risk += 10;
    if (kind == UR_WORK_BRIDGE) risk += 6;

    out->kind = kind;
    out->faction = faction;
    out->from = source != NULL ? source->cell : w->crews[faction].lair;
    out->site = site;
    out->obstacle = target;
    out->target = goal;
    out->load = load;
    out->cost = cost;
    out->priority = unlocked + saved - cost - risk;

    const UrNode *node = UrNodeNear(w, site, 8);
    const char *where = node != NULL ? node->name : "an unnamed face";
    if (source == NULL) {
        const UrNode *home = UrNodeNear(w, goal, 4);
        (void)snprintf(out->problem, sizeof(out->problem),
                       "no way from %s into %s", where,
                       home != NULL ? home->name : "a rival's strongroom");
        return true;
    }
    switch (kind) {
        case UR_WORK_BRIDGE:
            (void)snprintf(out->problem, sizeof(out->problem),
                           "%s loads from %s stop at the canyon by %s",
                           UrLoadName[load], source->name, where);
            break;
        case UR_WORK_WIDEN:
            (void)snprintf(out->problem, sizeof(out->problem),
                           "a crate from %s jams at the bend by %s",
                           source->name, where);
            break;
        case UR_WORK_CLEAR:
            (void)snprintf(out->problem, sizeof(out->problem),
                           "fallen ground blocks the %s route past %s",
                           UrLoadName[load], where);
            break;
        case UR_WORK_DESCENT:
            (void)snprintf(out->problem, sizeof(out->problem),
                           "%s loads cannot get down past %s",
                           UrLoadName[load], where);
            break;
        default:
            (void)snprintf(out->problem, sizeof(out->problem),
                           "no %s route from %s beyond %s",
                           UrLoadName[load], source->name, where);
            break;
    }
    return true;
}

/* ------------------------------------------------------------ excavation */

static void UrKnow(UrWorld *w, int32_t cell, int32_t faction)
{
    if (cell < 0 || cell >= UR_CELLS) return;
    w->tiles[cell].goblin_known |= (uint8_t)(1U << (uint32_t)faction);
}

/* Spoil is a quantity that has to go somewhere reachable, not particles. */
static bool UrDumpSpoil(UrWorld *w, int32_t cell, int32_t crew_index)
{
    int32_t layer = UrLayerOf(cell);
    for (int32_t other = layer * UR_LAYER_CELLS;
         other < (layer + 1) * UR_LAYER_CELLS; ++other) {
        if ((w->tiles[other].flags & UR_FLAG_SPOIL) != 0U &&
            UrDistance(other, cell) <= 16) {
            w->spoil += 1;
            return true;
        }
    }
    for (int32_t dir = 0; dir < 4; ++dir) {
        int32_t pocket = UrStep(cell, dir);
        if (pocket < 0 || w->tiles[pocket].kind != UR_ROCK) continue;
        UrTile *tile = &w->tiles[pocket];
        tile->kind = (uint8_t)UR_RECESS;
        tile->clear = 1U;
        tile->purpose = (uint8_t)UR_PURPOSE_RECESS;
        tile->flags |= UR_FLAG_SPOIL;
        tile->crew = (uint8_t)(crew_index + 1);
        tile->year = (int16_t)w->year;
        tile->goblin_known = 1U;
        w->spoil += 1;
        return true;
    }
    return false;   /* nowhere to put it: throughput suffers */
}

static void UrCut(UrWorld *w, int32_t cell, int32_t kind, int32_t clear,
                  int32_t purpose, int32_t crew_index)
{
    UrTile *tile = &w->tiles[cell];
    tile->kind = (uint8_t)kind;
    tile->clear = (uint8_t)clear;
    tile->purpose = (uint8_t)purpose;
    tile->crew = (uint8_t)(crew_index + 1);
    tile->year = (int16_t)w->year;
    tile->flags |= UR_FLAG_MARKS;
    tile->flags &= (uint8_t)~UR_FLAG_ABANDONED;
    UrKnow(w, cell, crew_index);
}

static bool UrBuildBridge(UrWorld *w, int32_t crew_index, int32_t from,
                          int32_t dir)
{
    UrCrew *crew = &w->crews[crew_index];
    if (w->bridge_count >= UR_MAX_BRIDGES) return false;
    int32_t span[16];
    int32_t span_count = 0;
    int32_t cell = UrStep(from, dir);
    while (cell >= 0 && w->tiles[cell].kind == UR_VOID && span_count < 16) {
        span[span_count++] = cell;
        cell = UrStep(cell, dir);
    }
    if (span_count == 0 || cell < 0) return false;
    if (!UrWalkableKind(w->tiles[cell].kind)) {
        if (w->tiles[cell].kind != UR_ROCK) return false;
        UrCut(w, cell, UR_CUT, 3, UR_PURPOSE_APPROACH, crew_index);
    }
    int32_t need = span_count * 6;
    if (crew->supplies < need) {
        crew->state = UR_CREW_WAIT;
        w->waits += 1;
        UrLog(w, false,
              "%s wait at the rim: %d planking short of a %d-tile span.",
              crew->name, need - crew->supplies, span_count);
        return false;
    }
    bool invested = crew->supplies >= need * 2 && crew->workers >= 8;
    crew->supplies -= invested ? need * 2 : need;
    UrBridge *bridge = &w->bridges[w->bridge_count];
    bridge->cell_count = span_count;
    for (int32_t i = 0; i < span_count; ++i) {
        bridge->cells[i] = span[i];
        UrTile *tile = &w->tiles[span[i]];
        tile->kind = (uint8_t)UR_BRIDGE;
        tile->clear = (uint8_t)(invested ? 3 : 2);
        tile->purpose = (uint8_t)UR_PURPOSE_APPROACH;
        tile->bridge = (int16_t)w->bridge_count;
        tile->crew = (uint8_t)(crew_index + 1);
        tile->year = (int16_t)w->year;
        UrKnow(w, span[i], crew_index);
    }
    bridge->condition = invested ? 92 : 58;
    bridge->capacity = invested ? 3 : 2;
    bridge->built_year = (int16_t)w->year;
    bridge->crew = (uint8_t)(crew_index + 1);
    bridge->failed = 0U;
    w->tiles[from].purpose = (uint8_t)UR_PURPOSE_APPROACH;
    char name[36];
    UrPlaceName(name, sizeof(name), span[0], w->seed, "Crossing");
    UrAddNode(w, span[0], UR_BRIDGE, name);
    crew->progress += span_count;
    if (w->failure_year > 0) w->rebuilds += 1;
    UrLog(w, false, "%s throw a %s %d-tile span over the canyon (%s)%s.",
          crew->name, invested ? "timbered" : "rickety", span_count, name,
          w->failure_year > 0 ? ", the old crossing having gone" : "");
    w->bridge_count += 1;
    return true;
}

static bool UrWidenCorner(UrWorld *w, int32_t cell, int32_t crew_index)
{
    UrCrew *crew = &w->crews[crew_index];
    if (crew->supplies < 3) {
        crew->state = UR_CREW_WAIT;
        w->waits += 1;
        if ((w->year % 11) == 0) {
            UrLog(w, false,
                  "%s leave the crate where it jammed: no materials to open "
                  "the corner.", crew->name);
        }
        return false;
    }
    crew->supplies -= 3;
    crew->progress += 1;
    UrCut(w, cell, UR_CHAMBER, 3, UR_PURPOSE_WIDEN, crew_index);
    for (int32_t dir = 0; dir < 4; ++dir) {
        int32_t next = UrStep(cell, dir);
        if (next < 0) continue;
        UrTile *tile = &w->tiles[next];
        if (tile->kind == UR_ROCK) {
            UrCut(w, next, UR_CUT, 3, UR_PURPOSE_WIDEN, crew_index);
            (void)UrDumpSpoil(w, next, crew_index);
        } else if (UrWalkableKind(tile->kind) && tile->clear < 3) {
            tile->clear = 3U;
            tile->flags |= UR_FLAG_MARKS;
        }
    }
    char name[36];
    UrPlaceName(name, sizeof(name), cell, w->seed, "Turn");
    UrAddNode(w, cell, UR_CHAMBER, name);
    return true;
}

static void UrWorkDescent(UrWorld *w, int32_t cell, int32_t crew_index)
{
    UrTile *tile = &w->tiles[cell];
    int32_t under = UrBelow(cell);
    if (under < 0) return;
    if (!UrWalkableKind(w->tiles[under].kind)) {
        UrCut(w, under, UR_CAVERN, 3, UR_PURPOSE_DESCENT, crew_index);
        (void)UrDumpSpoil(w, under, crew_index);
    } else if (w->tiles[under].clear < 2U) {
        w->tiles[under].clear = 2U;   /* a landing loads can stand on */
        w->tiles[under].flags |= UR_FLAG_MARKS;
        UrKnow(w, under, crew_index);
    }
    if (tile->vert == UR_VERT_RAMP) return;
    if (tile->vert == UR_VERT_LADDER || tile->clear >= 3) {
        tile->vert = (uint8_t)UR_VERT_RAMP;
        tile->clear = 3U;
    } else {
        tile->vert = (uint8_t)UR_VERT_LADDER;
    }
    tile->purpose = (uint8_t)UR_PURPOSE_DESCENT;
    tile->crew = (uint8_t)(crew_index + 1);
    tile->year = (int16_t)w->year;
    UrKnow(w, cell, crew_index);
    w->crews[crew_index].progress += 1;
    char name[36];
    UrPlaceName(name, sizeof(name), cell, w->seed,
                tile->vert == UR_VERT_RAMP ? "Ramp" : "Ladder");
    UrAddNode(w, cell, UR_CUT, name);
}

/* A crew commits to a short cardinal heading, then reassesses: continue,
 * turn, join what it finds, or stop. No omniscient tunnel is ever planned. */
static void UrAdvanceWork(UrWorld *w, int32_t crew_index)
{
    UrCrew *crew = &w->crews[crew_index];
    UrRequest *job = &crew->job;
    if (job->kind == UR_WORK_NONE) return;
    if (crew->at < 0) crew->at = job->site;

    int32_t hands = crew->workers - crew->reserve;
    if (hands < 1) hands = 1;
    int32_t budget = hands * 3;
    if (hands < 4) budget = hands * 2;   /* a narrow face cannot use a mob */

    while (budget > 0) {
        if (crew->remaining <= 0) {
            UrDigField(w, job->target,
                       (uint32_t)(1U << (uint32_t)crew_index));
            int32_t best_dir = -1;
            int32_t best_cost = g_cost[crew->at];
            for (int32_t dir = 0; dir < 4; ++dir) {
                int32_t next = UrStep(crew->at, dir);
                if (next < 0) continue;
                int32_t cost = g_cost[next];
                if ((w->tiles[next].flags & UR_FLAG_UNSTABLE) != 0U &&
                    (w->tiles[next].goblin_known &
                     (uint8_t)(1U << (uint32_t)crew_index)) != 0U) {
                    cost += 40;   /* known bad ground: they go around it */
                }
                cost += (int32_t)(UrNext(&w->rng) % 3U);   /* they wander */
                if (cost < best_cost) {
                    best_cost = cost;
                    best_dir = dir;
                }
            }
            if (best_dir < 0) {
                int32_t down = UrBelow(crew->at);
                int32_t up = UrAbove(crew->at);
                if (down >= 0 && g_cost[down] < g_cost[crew->at]) {
                    UrWorkDescent(w, crew->at, crew_index);
                    crew->at = down;
                    budget -= 6;
                    continue;
                }
                if (up >= 0 && g_cost[up] < g_cost[crew->at]) {
                    UrWorkDescent(w, up, crew_index);
                    crew->at = up;
                    budget -= 6;
                    continue;
                }
                crew->job.kind = UR_WORK_NONE;   /* nothing useful here */
                crew->state = UR_CREW_REST;
                return;
            }
            crew->heading = best_dir;
            crew->remaining = UrRange(&w->rng, 3, 6);
        }

        int32_t next = UrStep(crew->at, crew->heading);
        if (next < 0) { crew->remaining = 0; continue; }
        UrTile *tile = &w->tiles[next];

        if (tile->kind == UR_VOID) {
            if (!UrBuildBridge(w, crew_index, crew->at, crew->heading)) {
                crew->remaining = 0;
                budget -= 4;
                if (crew->state == UR_CREW_WAIT) return;
                continue;
            }
            crew->at = next;
            budget -= 8;
            continue;
        }
        if (UrWalkableKind(tile->kind)) {
            /* Intersecting the network joins it rather than digging on. */
            if (job->load == UR_LOAD_CRATE && tile->clear < 3) {
                if (!UrWidenCorner(w, next, crew_index)) return;
                budget -= 5;
            } else if (tile->clear < 2) {
                tile->clear = 2U;
                tile->flags |= UR_FLAG_MARKS;
                budget -= 2;
            }
            UrKnow(w, next, crew_index);
            crew->at = next;
            crew->remaining -= 1;
            bool joined = UrRoute(w, job->from, job->target, job->load,
                                  (uint32_t)(1U << (uint32_t)crew_index),
                                  NULL, 0) > 0;
            if (joined) {
                UrLog(w, false, "%s join the workings at %s: %s answered.",
                      crew->name,
                      UrNodeNear(w, next, 8) != NULL ?
                          UrNodeNear(w, next, 8)->name : "a blind face",
                      job->problem);
                crew->job.kind = UR_WORK_NONE;
                crew->state = UR_CREW_REST;
                crew->commit = 0;
                return;
            }
            continue;
        }

        int32_t work = tile->hardness + (tile->kind == UR_RUBBLE ? 0 : 2);
        if (budget < work) break;
        if (crew->supplies < 1) {
            /* No rope, no props, no baskets: the face stops. */
            crew->state = UR_CREW_WAIT;
            w->waits += 1;
            if ((w->year % 9) == 0) {
                UrLog(w, false, "%s withdraw from the face: nothing left to "
                      "timber it with.", crew->name);
            }
            return;
        }
        crew->supplies -= 1;
        budget -= work;
        /* A crate road is a wide cut, not a chamber every tile: chambers
         * are for the corners a crate actually has to turn in. */
        int32_t kind = UR_CUT;
        int32_t clear = (job->load == UR_LOAD_CRATE) ? 3 : 2;
        int32_t purpose = (job->kind == UR_WORK_CLEAR ||
                           job->kind == UR_WORK_BRIDGE) ?
                              UR_PURPOSE_BYPASS :
                              (w->dragon_alive ? UR_PURPOSE_HAUL :
                                                 UR_PURPOSE_HOME);
        if ((tile->flags & UR_FLAG_COLLAPSED) != 0U) {
            purpose = UR_PURPOSE_BYPASS;
        }
        UrCut(w, next, kind, clear, purpose, crew_index);
        if (!UrDumpSpoil(w, next, crew_index)) budget -= 4;
        if ((tile->flags & UR_FLAG_UNSTABLE) != 0U && crew->supplies >= 2) {
            crew->supplies -= 2;
            tile->flags |= UR_FLAG_SUPPORT;
        }
        crew->tiles_cut += 1;
        crew->progress += 1;
        crew->at = next;
        crew->remaining -= 1;
    }
    crew->state = UR_CREW_WORK;
}

/* --------------------------------------------------------- hauling years */

static int32_t g_path[UR_STATES];

static void UrMarkTraffic(UrWorld *w, const int32_t *path, int32_t length,
                          int32_t loads)
{
    for (int32_t i = 0; i < length; ++i) {
        UrTile *tile = &w->tiles[path[i]];
        int32_t traffic = tile->traffic + loads;
        tile->traffic = (int16_t)(traffic > 30000 ? 30000 : traffic);
        int32_t wear = tile->traffic / 4;
        tile->wear = (uint8_t)(wear > 9 ? 9 : wear);
        tile->flags &= (uint8_t)~UR_FLAG_ABANDONED;
        if (tile->traffic > 12 && (tile->flags & UR_FLAG_ARROW) == 0U) {
            int32_t open = 0;
            for (int32_t dir = 0; dir < 4; ++dir) {
                int32_t next = UrStep(path[i], dir);
                if (next >= 0 && UrWalkable(w, next)) ++open;
            }
            if (open >= 3) tile->flags |= UR_FLAG_ARROW;
        }
    }
}

static bool UrDeliver(UrWorld *w, int32_t faction, int32_t source_index,
                      int32_t load)
{
    UrSource *source = &w->sources[source_index];
    UrCrew *crew = &w->crews[faction];
    int32_t dest = UrDestination(w, faction);
    int32_t length = UrRoute(w, source->cell, dest, load,
                             (uint32_t)(1U << (uint32_t)faction), g_path,
                             UR_STATES);
    if (length <= 0) return false;
    source->worked_by |= 1 << faction;
    const char *where = w->dragon_alive ? "the hoard" : "their strongroom";

    if (load == UR_LOAD_CRATE) {
        if (source->crates <= 0) return false;
        source->crates -= 1;
        w->crates_delivered += 1;
        w->delivered += UR_CRATE_LOAD;
        w->deliveries += 1;
        crew->deliveries += 1;
        if (w->dragon_alive) crew->tribute += UR_CRATE_LOAD;
        else crew->stored += UR_CRATE_LOAD;
        if (w->failure_year > 0) w->recovered += 1;
        UrMarkTraffic(w, g_path, length, 4);
        UrLog(w, false,
              "%s carry a bulky crate from %s to %s: %d tiles, %d crowns.",
              crew->name, source->name, where, length, UR_CRATE_LOAD);
        return true;
    }

    int32_t capacity = 3 + crew->workers / 5;
    int32_t loads = capacity;
    if (source->stranded > 0) {
        int32_t freed = source->stranded < loads ? source->stranded : loads;
        source->stranded -= freed;
        w->delivered += freed * UR_PORTER_LOAD;
        w->deliveries += freed;
        crew->deliveries += freed;
        if (w->dragon_alive) crew->tribute += freed * UR_PORTER_LOAD;
        else crew->stored += freed * UR_PORTER_LOAD;
        UrMarkTraffic(w, g_path, length, freed);
        UrLog(w, false, "%d cached loads move again from %s, carried by %s.",
              freed, source->name, crew->name);
        loads -= freed;
    }
    if (loads > source->crowns / UR_PORTER_LOAD) {
        loads = source->crowns / UR_PORTER_LOAD;
    }
    if (loads <= 0) return true;
    source->crowns -= loads * UR_PORTER_LOAD;
    w->delivered += loads * UR_PORTER_LOAD;
    w->deliveries += loads;
    crew->deliveries += loads;
    if (w->dragon_alive) crew->tribute += loads * UR_PORTER_LOAD;
    else crew->stored += loads * UR_PORTER_LOAD;
    if (w->failure_year > 0) w->recovered += loads;
    UrMarkTraffic(w, g_path, length, loads);
    if (w->deliveries <= 3 || (w->year % 9) == 0) {
        UrLog(w, false,
              "%s bring %d porter pack%s from %s to %s along %d tiles.",
              crew->name, loads, loads == 1 ? "" : "s", source->name, where,
              length);
    }
    return true;
}

static void UrScout(UrWorld *w, int32_t faction, int32_t budget)
{
    /* Knowledge spreads by walking, never by knowing the map — and what one
     * colony has walked, its rivals have not. */
    uint8_t mask = (uint8_t)(1U << (uint32_t)faction);
    int32_t gained = 0;
    for (int32_t pass = 0; pass < 3 && gained < budget; ++pass) {
        for (int32_t cell = 0; cell < UR_CELLS && gained < budget; ++cell) {
            if ((w->tiles[cell].goblin_known & mask) == 0U) continue;
            if (!UrWalkable(w, cell)) continue;
            for (int32_t dir = 0; dir < 4; ++dir) {
                int32_t next = UrStep(cell, dir);
                if (next < 0 || (w->tiles[next].goblin_known & mask) != 0U) {
                    continue;
                }
                if (!UrWalkable(w, next)) continue;
                w->tiles[next].goblin_known |= mask;
                if (++gained >= budget) break;
            }
            int32_t down = UrBelow(cell);
            if (down >= 0 && w->tiles[cell].vert != UR_VERT_NONE &&
                UrWalkable(w, down) &&
                (w->tiles[down].goblin_known & mask) == 0U) {
                w->tiles[down].goblin_known |= mask;
                ++gained;
            }
        }
    }
}

static void UrCollapseAround(UrWorld *w, int32_t cell, bool injected,
                             const char *cause)
{
    int32_t fallen = 0;
    for (int32_t dir = 0; dir < 4; ++dir) {
        int32_t next = (dir == 0) ? cell : UrStep(cell, dir - 1);
        if (next < 0 || !UrWalkable(w, next)) continue;
        if (w->tiles[next].kind == UR_HOARD ||
            w->tiles[next].kind == UR_SOURCE) continue;
        UrTile *tile = &w->tiles[next];
        tile->kind = (uint8_t)UR_RUBBLE;
        tile->clear = 0U;
        tile->flags |= UR_FLAG_COLLAPSED;
        if (injected) tile->flags |= UR_FLAG_INJECTED;
        tile->vert = (uint8_t)UR_VERT_NONE;
        tile->year = (int16_t)w->year;
        ++fallen;
    }
    if (fallen == 0) return;
    w->collapses += 1;
    if (w->failure_year == 0) w->failure_year = w->year;
    const UrNode *node = UrNodeNear(w, cell, 10);
    UrLog(w, injected, "%s%s falls in at %s: %d tiles of route gone.",
          injected ? "[injected] " : "", cause,
          node != NULL ? node->name : "an unnamed drift", fallen);
}

static void UrFailBridge(UrWorld *w, int32_t index, bool injected,
                         const char *cause)
{
    UrBridge *bridge = &w->bridges[index];
    if (bridge->failed) return;
    bridge->failed = 1U;
    bridge->condition = 0;
    if (w->failure_year == 0) w->failure_year = w->year;
    int32_t near_side = -1;
    for (int32_t i = 0; i < bridge->cell_count; ++i) {
        int32_t cell = bridge->cells[i];
        UrTile *tile = &w->tiles[cell];
        tile->kind = (uint8_t)UR_VOID;
        tile->clear = 0U;
        tile->bridge = -1;
        tile->flags |= UR_FLAG_COLLAPSED;
        if (injected) tile->flags |= UR_FLAG_INJECTED;
        if (near_side < 0) {
            for (int32_t dir = 0; dir < 4; ++dir) {
                int32_t next = UrStep(cell, dir);
                if (next >= 0 && UrWalkable(w, next)) { near_side = next; break; }
            }
        }
    }
    /* Declared outcome for whatever was on it: cached on the near side. */
    if (near_side >= 0) {
        UrTile *tile = &w->tiles[near_side];
        if (tile->kind == UR_CUT || tile->kind == UR_CAVERN) {
            tile->kind = (uint8_t)UR_CACHE;
            tile->clear = 3U;
        }
        /* The loads that were on the road were somebody's, not new crowns:
         * they come off the source and wait in the cache. */
        for (int32_t i = 0; i < UR_SOURCES; ++i) {
            UrSource *source = &w->sources[i];
            int32_t caught = source->crowns >= 20 ? 20 : source->crowns;
            source->crowns -= caught;
            source->stranded += caught / UR_PORTER_LOAD;
        }
    }
    const UrNode *node = UrNodeNear(w, bridge->cells[0], 12);
    UrLog(w, injected,
          "%s%s: the %s gives way. Loads cached on the near side at %s.",
          injected ? "[injected] " : "", cause,
          node != NULL ? node->name : "span",
          near_side >= 0 ? "the rim" : "nowhere safe");
}

static void UrMaintain(UrWorld *w)
{
    for (int32_t cell = 0; cell < UR_CELLS; ++cell) {
        UrTile *tile = &w->tiles[cell];
        if (tile->year < 0 || !UrWalkableKind(tile->kind)) continue;
        int32_t age = w->year - tile->year;
        if (age > 2) tile->flags &= (uint8_t)~UR_FLAG_MARKS;
        if (tile->traffic > 25 && (tile->flags & UR_FLAG_SUPPORT) == 0U &&
            (w->year % 4) == 0) {
            tile->flags |= UR_FLAG_SUPPORT;
        }
        /* Stale routes stop being maintained; the tunnel does not vanish. */
        if (tile->traffic == 0 && age > 9) {
            tile->flags |= UR_FLAG_ABANDONED;
            if (tile->purpose == UR_PURPOSE_HAUL) {
                tile->purpose = (uint8_t)UR_PURPOSE_ABANDONED;
            }
        }
    }
    for (int32_t i = 0; i < w->bridge_count; ++i) {
        UrBridge *bridge = &w->bridges[i];
        if (bridge->failed) continue;
        int32_t traffic = 0;
        for (int32_t c = 0; c < bridge->cell_count; ++c) {
            traffic += w->tiles[bridge->cells[c]].traffic;
        }
        bridge->condition -= 2 + traffic / 60;
        if (bridge->condition < 0) bridge->condition = 0;
        if (bridge->condition < 55) {
            UrCrew *crew = &w->crews[bridge->crew > 0U ? bridge->crew - 1U : 0];
            if (crew->supplies >= 10) {
                crew->supplies -= 10;
                bridge->condition += 28;
                if (bridge->condition > 100) bridge->condition = 100;
                for (int32_t c = 0; c < bridge->cell_count; ++c) {
                    w->tiles[bridge->cells[c]].flags |= UR_FLAG_SUPPORT;
                    w->tiles[bridge->cells[c]].year = (int16_t)w->year;
                }
                if ((w->year % 12) == 0) {
                    UrLog(w, false,
                          "%s replace planking on the span they built in "
                          "year %d.", crew->name, bridge->built_year);
                }
            } else {
                w->waits += 1;   /* no planking this year; it waits */
            }
        }
        if (bridge->condition < 30 && UrChance(&w->rng, 25)) {
            UrFailBridge(w, i, false, "Deterioration");
        }
    }
}

static char UrGlyph(const UrWorld *w, int32_t cell, bool show_vert);

/* One number for the whole mountain at this instant: geometry, provenance
 * and state. Two runs of the same world agree at the same year, or they do
 * not, and this says which. */
static uint64_t UrHashWorld(const UrWorld *w)
{
    uint64_t h = UINT64_C(0xcbf29ce484222325);
    for (int32_t cell = 0; cell < UR_CELLS; ++cell) {
        const UrTile *t = &w->tiles[cell];
        uint8_t bytes[8];
        bytes[0] = t->kind;
        bytes[1] = t->clear;
        bytes[2] = t->vert;
        bytes[3] = t->crew;
        bytes[4] = t->purpose;
        bytes[5] = t->flags;
        bytes[6] = (uint8_t)(t->year & 0xff);
        bytes[7] = (uint8_t)((uint16_t)t->traffic & 0xff);
        for (int32_t i = 0; i < 8; ++i) {
            h ^= bytes[i];
            h *= UINT64_C(0x100000001b3);
        }
    }
    return h;
}

static void UrSnapshot(UrWorld *w, const char *label)
{
    if (w->snap_count >= UR_MAX_SNAPS) return;
    UrSnap *snap = &w->snaps[w->snap_count++];
    snap->year = (int16_t)w->year;
    (void)snprintf(snap->label, sizeof(snap->label), "%s", label);
    snap->dragon = w->dragon_alive;
    snap->hash = UrHashWorld(w);
    for (int32_t cell = 0; cell < UR_CELLS; ++cell) {
        snap->kind[cell] = (uint8_t)UrGlyph(w, cell, true);
        snap->crew[cell] = w->tiles[cell].crew;
    }
}

/* ------------------------------------------------------- colonies at war
 *
 * With a dragon in the mountain, three colonies compete by tribute and the
 * biggest giver wears its favour. With no dragon, the same loot is simply
 * loot, and the same tunnels carry raids instead of tribute.
 */

static int32_t UrChokepoint(const UrWorld *w, const int32_t *path,
                            int32_t length, int32_t near)
{
    /* The tile on the raided route, closest to home, that a barricade would
     * actually stop: two ways out, not a junction with three. */
    int32_t best = -1;
    int32_t best_distance = INT32_MAX;
    for (int32_t i = 0; i < length; ++i) {
        int32_t cell = path[i];
        const UrTile *tile = &w->tiles[cell];
        if (tile->kind == UR_STRONG || tile->kind == UR_LAIR ||
            tile->kind == UR_SOURCE || tile->kind == UR_HOARD) continue;
        int32_t open = 0;
        for (int32_t dir = 0; dir < 4; ++dir) {
            int32_t next = UrStep(cell, dir);
            if (next >= 0 && UrWalkable(w, next)) ++open;
        }
        if (open > 2) continue;
        int32_t distance = UrDistance(cell, near);
        if (distance < best_distance) {
            best_distance = distance;
            best = cell;
        }
    }
    return best;
}

static void UrSpoilsWar(UrWorld *w)
{
    int32_t rich = 0;
    for (int32_t i = 1; i < UR_FACTIONS; ++i) {
        if (w->crews[i].stored > w->crews[rich].stored) rich = i;
    }
    UrCrew *victim = &w->crews[rich];
    if (victim->stored < 40) return;

    for (int32_t k = 0; k < UR_FACTIONS; ++k) {
        int32_t raider = (w->year + k) % UR_FACTIONS;
        if (raider == rich) continue;
        UrCrew *thief = &w->crews[raider];
        if (thief->workers < 3 || thief->stored >= victim->stored) continue;
        int32_t length = UrRoute(w, thief->lair, victim->strong,
                                 UR_LOAD_PORTER,
                                 (uint32_t)(1U << (uint32_t)raider), g_path,
                                 UR_STATES);
        if (length <= 0) continue;
        if (!UrChance(&w->rng, 45)) continue;

        int32_t taken = (int32_t)(victim->stored / 3);
        if (taken > 70) taken = 70;
        victim->stored -= taken;
        thief->stored += taken;
        thief->raids_made += 1;
        victim->raids_suffered += 1;
        w->raids += 1;
        UrMarkTraffic(w, g_path, length, 3);
        UrLog(w, false,
              "%s come up the workings and take %d crowns out of the %s "
              "strongroom.", thief->name, taken, victim->colour);

        /* The answer is built, not decreed: block the way in, or drop the
         * span you built yourself rather than leave it for a rival. */
        int32_t choke = UrChokepoint(w, g_path, length, victim->strong);
        if (choke >= 0 && victim->supplies >= 8) {
            victim->supplies -= 8;
            UrTile *tile = &w->tiles[choke];
            tile->kind = (uint8_t)UR_BARRICADE;
            tile->clear = 0U;
            tile->purpose = (uint8_t)UR_PURPOSE_DEFENCE;
            tile->crew = (uint8_t)(rich + 1);
            tile->year = (int16_t)w->year;
            w->barricades += 1;
            const UrNode *node = UrNodeNear(w, choke, 10);
            UrLog(w, false,
                  "%s wall the passage at %s with timber and spoil.",
                  victim->name, node != NULL ? node->name : "a narrow place");
            return;
        }
        for (int32_t b = 0; b < w->bridge_count; ++b) {
            if (w->bridges[b].failed) continue;
            if (w->bridges[b].crew != (uint8_t)(rich + 1)) continue;
            bool on_route = false;
            for (int32_t i = 0; i < length && !on_route; ++i) {
                if (w->tiles[g_path[i]].bridge == (int16_t)b) on_route = true;
            }
            if (!on_route) continue;
            w->sabotage += 1;
            UrFailBridge(w, b, false, "Cut loose by its own builders");
            UrLog(w, false,
                  "%s cut their own span rather than leave it under %s feet.",
                  victim->name, thief->colour);
            return;
        }
        return;
    }
}

#ifdef UR_EMBED
/* What a world simulation hands the mountain: loot that actually came back
 * from a raid, and the hands a colony actually has this year. */
static void UrAddLoot(UrWorld *w, int32_t source_index, int32_t crowns,
                      int32_t crates)
{
    if (source_index < 0 || source_index >= UR_SOURCES || crowns < 0) return;
    w->sources[source_index].crowns += crowns;
    w->sources[source_index].crates += crates;
}

static void UrSetHands(UrWorld *w, int32_t faction, int32_t hands)
{
    if (faction < 0 || faction >= UR_FACTIONS) return;
    if (hands < 1) hands = 1;
    if (hands > 16) hands = 16;
    w->crews[faction].workers = hands;
    if (hands > w->crews[faction].hands_max) w->crews[faction].hands_max = hands;
}
#endif /* UR_EMBED */

/* What a colony does in a year when its road already works. Real crews do
 * not sit: they prospect for new ground, cut crosscuts that shorten a long
 * haul, and open a second way past the stretch where porters queue. All
 * three leave passages behind, which is where a mine stops being a corridor
 * and starts being a maze. */
static int32_t UrKnownFace(UrWorld *w, int32_t faction)
{
    uint8_t mask = (uint8_t)(1U << (uint32_t)faction);
    int32_t best = -1;
    int32_t seen = 0;
    for (int32_t cell = 0; cell < UR_CELLS; ++cell) {
        const UrTile *tile = &w->tiles[cell];
        if (!UrWalkableKind(tile->kind)) continue;
        if ((tile->goblin_known & mask) == 0U) continue;
        bool has_rock = false;
        for (int32_t dir = 0; dir < 4 && !has_rock; ++dir) {
            int32_t next = UrStep(cell, dir);
            if (next >= 0 && w->tiles[next].kind == UR_ROCK) has_rock = true;
        }
        if (!has_rock) continue;
        /* Reservoir sample so the choice does not favour low addresses. */
        ++seen;
        if (UrRange(&w->rng, 1, seen) == 1) best = cell;
    }
    return best;
}

static void UrProspect(UrWorld *w, int32_t faction)
{
    UrCrew *crew = &w->crews[faction];
    if (crew->supplies < 8) return;
    int32_t face = UrKnownFace(w, faction);
    if (face < 0) return;

    /* A winze instead of a drift, now and then: down is where the ore and
     * the dragon both are. */
    if (UrChance(&w->rng, 28) && w->tiles[face].vert == UR_VERT_NONE &&
        UrBelow(face) >= 0) {
        crew->supplies -= 5;
        UrWorkDescent(w, face, faction);
        UrLog(w, false, "%s sink a winze at %s to see what is under it.",
              crew->name,
              UrNodeNear(w, face, 8) != NULL ? UrNodeNear(w, face, 8)->name :
                                               "a blind face");
        return;
    }

    int32_t dir = -1;
    for (int32_t attempt = 0; attempt < 8 && dir < 0; ++attempt) {
        int32_t candidate = UrRange(&w->rng, 0, 3);
        int32_t next = UrStep(face, candidate);
        if (next >= 0 && w->tiles[next].kind == UR_ROCK) dir = candidate;
    }
    if (dir < 0) return;

    int32_t length = UrRange(&w->rng, 4, 10);
    int32_t at = face;
    int32_t cut = 0;
    bool struck = false;
    for (int32_t i = 0; i < length && crew->supplies > 2; ++i) {
        int32_t next = UrStep(at, dir);
        if (next < 0) break;
        UrTile *tile = &w->tiles[next];
        if (UrWalkableKind(tile->kind)) {
            struck = true;      /* broke into something already open */
            UrKnow(w, next, faction);
            at = next;
            break;
        }
        if (tile->kind != UR_ROCK) break;
        crew->supplies -= 2;
        UrCut(w, next, UR_CUT, 2, UR_PURPOSE_HAUL, faction);
        crew->tiles_cut += 1;
        ++cut;
        at = next;
        if (UrChance(&w->rng, 22)) {
            dir = (dir + (UrChance(&w->rng, 50) ? 1 : 3)) % 4;   /* a bend */
        }
    }
    if (cut == 0) return;
    if (struck) {
        UrLog(w, false, "%s hole through to open ground after %d tiles of "
              "prospecting.", crew->name, cut);
    } else {
        /* A drift that found nothing is still a drift, and it stays. */
        for (int32_t i = 0; i < 4; ++i) {
            int32_t back = UrStep(at, i);
            if (back >= 0 && w->tiles[back].year == (int16_t)w->year) continue;
        }
        w->tiles[at].purpose = (uint8_t)UR_PURPOSE_ABANDONED;
        w->tiles[at].flags |= UR_FLAG_ABANDONED;
    }
}

/* Two places a colony already walks that are close in rock but far apart by
 * road: cutting between them saves every future haul. */
static void UrCrosscut(UrWorld *w, int32_t faction)
{
    UrCrew *crew = &w->crews[faction];
    uint8_t mask = (uint8_t)(1U << (uint32_t)faction);
    if (crew->supplies < 12) return;

    int32_t used[256];
    int32_t used_count = 0;
    for (int32_t cell = 0; cell < UR_CELLS && used_count < 256; ++cell) {
        const UrTile *tile = &w->tiles[cell];
        if (tile->traffic > 0 && UrWalkableKind(tile->kind) &&
            (tile->goblin_known & mask) != 0U) {
            used[used_count++] = cell;
        }
    }
    if (used_count < 2) return;

    for (int32_t attempt = 0; attempt < 24; ++attempt) {
        int32_t a = used[UrRange(&w->rng, 0, used_count - 1)];
        int32_t b = used[UrRange(&w->rng, 0, used_count - 1)];
        if (a == b || UrLayerOf(a) != UrLayerOf(b)) continue;
        int32_t gap = UrDistance(a, b);
        if (gap < 3 || gap > 8) continue;
        int32_t road = UrRoute(w, a, b, UR_LOAD_PORTER, mask, NULL, 0);
        if (road <= 0 || road < gap * 4) continue;

        int32_t at = a;
        int32_t cut = 0;
        while (at != b && cut < 12 && crew->supplies > 3) {
            int32_t dx = UrXOf(b) - UrXOf(at);
            int32_t dy = UrYOf(b) - UrYOf(at);
            int32_t dir;
            if ((dx > 0 ? dx : -dx) >= (dy > 0 ? dy : -dy)) {
                dir = dx > 0 ? 1 : 3;
            } else {
                dir = dy > 0 ? 2 : 0;
            }
            int32_t next = UrStep(at, dir);
            if (next < 0) break;
            UrTile *tile = &w->tiles[next];
            if (tile->kind == UR_ROCK) {
                crew->supplies -= 2;
                UrCut(w, next, UR_CUT, 2, UR_PURPOSE_HAUL, faction);
                crew->tiles_cut += 1;
                ++cut;
            } else if (!UrWalkableKind(tile->kind)) {
                break;          /* canyon or fallen ground: not this way */
            }
            UrKnow(w, next, faction);
            at = next;
        }
        if (cut > 0) {
            UrLog(w, false,
                  "%s cut a crosscut of %d tiles; the long way round was %d.",
                  crew->name, cut, road);
        }
        return;
    }
}

/* Where the porters queue, open a second way past it. */
static void UrReliefDrift(UrWorld *w, int32_t faction)
{
    UrCrew *crew = &w->crews[faction];
    uint8_t mask = (uint8_t)(1U << (uint32_t)faction);
    if (crew->supplies < 10) return;
    int32_t busiest = -1;
    for (int32_t cell = 0; cell < UR_CELLS; ++cell) {
        const UrTile *tile = &w->tiles[cell];
        if (!UrWalkableKind(tile->kind)) continue;
        if ((tile->goblin_known & mask) == 0U) continue;
        if (tile->traffic < 40) continue;
        if (busiest < 0 || tile->traffic > w->tiles[busiest].traffic) {
            busiest = cell;
        }
    }
    if (busiest < 0) return;
    /* Drive parallel to the busy stretch, two tiles off, and rejoin. */
    int32_t along = -1;
    for (int32_t dir = 0; dir < 4 && along < 0; ++dir) {
        int32_t next = UrStep(busiest, dir);
        if (next >= 0 && UrWalkable(w, next) && w->tiles[next].traffic > 20) {
            along = dir;
        }
    }
    if (along < 0) return;
    int32_t side = (along + 1) % 4;
    int32_t at = busiest;
    int32_t cut = 0;
    int32_t plan[3] = { side, along, (side + 2) % 4 };
    for (int32_t leg = 0; leg < 3; ++leg) {
        int32_t steps = leg == 1 ? 4 : 2;
        for (int32_t i = 0; i < steps && crew->supplies > 3; ++i) {
            int32_t next = UrStep(at, plan[leg]);
            if (next < 0) break;
            UrTile *tile = &w->tiles[next];
            if (tile->kind == UR_ROCK) {
                crew->supplies -= 2;
                UrCut(w, next, UR_CUT, 2, UR_PURPOSE_HAUL, faction);
                crew->tiles_cut += 1;
                ++cut;
            } else if (!UrWalkableKind(tile->kind)) {
                break;
            }
            UrKnow(w, next, faction);
            at = next;
        }
    }
    if (cut > 0) {
        UrLog(w, false,
              "%s open a relief drift of %d tiles past the queue at %s.",
              crew->name, cut,
              UrNodeNear(w, busiest, 8) != NULL ?
                  UrNodeNear(w, busiest, 8)->name : "the busy stretch");
    }
}

static void UrIdleWork(UrWorld *w, int32_t faction)
{
    int32_t roll = UrRange(&w->rng, 0, 9);
    if (roll < 3) UrCrosscut(w, faction);
    else if (roll < 5) UrReliefDrift(w, faction);
    else UrProspect(w, faction);
}

static void UrRunYear(UrWorld *w)
{
    UrRequest requests[UR_MAX_REQUESTS];
    int32_t request_count = 0;
    bool was_alive = w->dragon_alive;

    w->season = (int32_t)(UrNext(&w->rng) % 4U);
    /* Hosted in a world simulation, the dragon, the hands and the loot are
     * that world's business; this only decides what gets dug because of it. */
    if (!w->hosted) {
        w->dragon_alive = w->dragon_dies < 0 ||
                          (w->dragon_dies > 0 && w->year < w->dragon_dies);
    }
    if (was_alive && !w->dragon_alive) {
        UrLog(w, false,
              "The dragon is dead. Nobody is owed tribute, and every colony "
              "starts carrying its take home instead.");
    }

    /* Raids keep arriving at the stage. Demand is what keeps a route
     * maintained; when it stops, the tunnels stay but the upkeep does not. */
    if (!w->hosted && w->sources[0].replenished &&
        w->sources[0].crowns < 240) {
        w->sources[0].crowns += 20;
        if ((w->year % 15) == 0) {
            UrLog(w, false, "Raiders bring another season's take to %s.",
                  w->sources[0].name);
        }
    }

    /* Rotate first access each year so shared treasure has fair contention
     * between the colonies, as the simulation's own porters do daily. */
    for (int32_t k = 0; k < UR_FACTIONS; ++k) {
        int32_t faction = (w->year + k) % UR_FACTIONS;
        bool hauled = false;
        int32_t richest = -1;
        int32_t best_worth = INT32_MIN;
        for (int32_t i = 0; i < UR_SOURCES; ++i) {
            UrSource *source = &w->sources[i];
            if (source->crates > 0 &&
                UrDeliver(w, faction, i, UR_LOAD_CRATE)) {
                hauled = true;
            }
            if ((source->crowns > 0 || source->stranded > 0) &&
                UrDeliver(w, faction, i, UR_LOAD_PORTER)) {
                hauled = true;
                break;   /* one working route per colony per year */
            }
            if (source->crowns > 0) {
                int32_t worth = source->crowns -
                                3 * UrDistance(source->cell,
                                               w->crews[faction].lair);
                if (richest < 0 || worth > best_worth) {
                    best_worth = worth;
                    richest = i;
                }
            }
        }
        if (!hauled && richest >= 0 && request_count < UR_MAX_REQUESTS) {
            int32_t load = w->sources[richest].crates > 0 ? UR_LOAD_CRATE :
                                                            UR_LOAD_PORTER;
            if (UrPlanWork(w, faction, richest, load, -1,
                           &requests[request_count])) {
                ++request_count;
            }
        }
    }

    for (int32_t i = 0; i < UR_CREWS; ++i) {
        UrCrew *crew = &w->crews[i];
        crew->supplies += 5 + crew->workers / 4;
        if (crew->supplies > 48) crew->supplies = 48;
        /* A hunted colony is not a dead one: goblins grow up. */
        if (crew->workers < crew->hands_max && (w->year % 6) == 0) {
            crew->workers += 1;
        }
        /* Porters on the road this year are not at the face. */
        crew->reserve = crew->workers / 3;
        if (crew->reserve < 1) crew->reserve = 1;
        if (crew->job.kind != UR_WORK_NONE) {
            crew->commit -= 1;
            crew->progress = 0;
            UrAdvanceWork(w, i);
            /* A colony with stores to spare drives improvement work beside
             * the job in hand, the way a mine is always being extended. */
            if (crew->supplies > 30) UrIdleWork(w, i);
            crew->idle_years = crew->progress > 0 ? 0 : crew->idle_years + 1;
            if (crew->idle_years >= 3) {
                UrLog(w, false, "%s give up on it after %d years with nothing "
                      "to show: %s.", crew->name, crew->idle_years,
                      crew->job.problem);
                crew->job.kind = UR_WORK_NONE;
                crew->state = UR_CREW_REST;
                crew->idle_years = 0;
            }
            continue;
        }
        int32_t best = -1;
        for (int32_t r = 0; r < request_count; ++r) {
            if (requests[r].kind == UR_WORK_NONE) continue;
            if (requests[r].faction != i) continue;
            if (best < 0 || requests[r].priority > requests[best].priority) {
                best = r;
            }
        }
        if (best < 0) {
            crew->state = UR_CREW_SCOUT;
            UrScout(w, i, 40);
            UrIdleWork(w, i);   /* a year with no crisis is a year of works */
            continue;
        }
        if (requests[best].priority <= 0 ||
            crew->workers - crew->reserve < 3 || crew->supplies < 10) {
            crew->state = UR_CREW_WAIT;
            w->waits += 1;
            if ((w->year % 6) == 0) {
                UrLog(w, false, "%s stand down (%d hands, %d supplies): %s.",
                      crew->name, crew->workers - crew->reserve,
                      crew->supplies, requests[best].problem);
            }
            continue;
        }
        crew->job = requests[best];
        crew->at = requests[best].site;
        crew->remaining = 0;
        crew->commit = 3;
        crew->idle_years = 0;
        crew->progress = 0;
        crew->state = UR_CREW_WORK;
        UrLog(w, false, "%s take the work order: %s (priority %d).",
              crew->name, crew->job.problem, crew->job.priority);
        if (crew->job.kind == UR_WORK_WIDEN) {
            (void)UrWidenCorner(w, crew->job.obstacle, i);
        } else if (crew->job.kind == UR_WORK_DESCENT) {
            UrWorkDescent(w, crew->job.site, i);
        }
        requests[best].kind = UR_WORK_NONE;
        UrAdvanceWork(w, i);
    }

    for (int32_t i = 0; i < UR_FACTIONS; ++i) UrScout(w, i, 25);

    /* One founding tribute contest, then the dragon's favour, then its
     * teeth for whoever came second. */
    if (!w->hosted && w->dragon_alive && w->crown < 0 && w->year >= 12) {
        int32_t winner = 0;
        for (int32_t i = 1; i < UR_FACTIONS; ++i) {
            if (w->crews[i].tribute > w->crews[winner].tribute) winner = i;
        }
        bool unique = w->crews[winner].tribute > 0;
        for (int32_t i = 0; i < UR_FACTIONS; ++i) {
            if (i != winner && w->crews[i].tribute == w->crews[winner].tribute) {
                unique = false;
            }
        }
        if (unique) {
            w->crown = winner;
            w->crown_year = w->year;
            UrLog(w, false,
                  "%s lead the tribute; the dragon wears a %s crown. The "
                  "others keep digging, and hope.", w->crews[winner].name,
                  w->crews[winner].colour);
        }
    }
    if (!w->hosted && w->dragon_alive && w->crown >= 0 &&
        (w->year % 4) == 0) {
        int32_t prey = (w->year / 4) % UR_FACTIONS;
        if (prey != w->crown && w->crews[prey].workers > 3) {
            w->crews[prey].workers -= 1;
            w->crews[prey].hunted += 1;
            UrLog(w, false,
                  "The dragon hunts %s: a hand fewer at the face this year.",
                  w->crews[prey].name);
        }
    }
    if (!w->dragon_alive) {
        int32_t rich = 0;
        for (int32_t i = 1; i < UR_FACTIONS; ++i) {
            if (w->crews[i].stored > w->crews[rich].stored) rich = i;
        }
        for (int32_t i = 0; i < UR_FACTIONS && request_count < UR_MAX_REQUESTS;
             ++i) {
            if (i == rich) continue;
            if (w->crews[rich].stored < w->crews[i].stored + 100) continue;
            if (w->crews[i].job.kind != UR_WORK_NONE) continue;
            if (UrRoute(w, w->crews[i].lair, w->crews[rich].strong,
                        UR_LOAD_PORTER, (uint32_t)(1U << (uint32_t)i), NULL,
                        0) > 0) continue;
            if (UrPlanWork(w, i, -1, UR_LOAD_PORTER, w->crews[rich].strong,
                           &requests[request_count])) {
                UrCrew *crew = &w->crews[i];
                crew->job = requests[request_count];
                crew->at = crew->job.site;
                crew->remaining = 0;
                crew->commit = 3;
                crew->state = UR_CREW_WORK;
                UrLog(w, false, "%s start cutting toward the %s strongroom: "
                      "%s.", crew->name, w->crews[rich].colour,
                      crew->job.problem);
                UrAdvanceWork(w, i);
            }
        }
        UrSpoilsWar(w);
    }

    /* Failures follow causes: bad ground under traffic, tired spans, and
     * the one failure a test is allowed to inject. */
    for (int32_t cell = 0; cell < UR_CELLS; ++cell) {
        UrTile *tile = &w->tiles[cell];
        if (!UrWalkableKind(tile->kind) || tile->kind == UR_BRIDGE) continue;
        if ((tile->flags & UR_FLAG_UNSTABLE) == 0U) continue;
        if (tile->traffic < 6) continue;
        if (!UrChance(&w->rng,
                      (tile->flags & UR_FLAG_SUPPORT) != 0U ? 1 : 7)) continue;
        UrCollapseAround(w, cell, false, "Unsupported ground");
        break;
    }

    if (w->inject && !w->injected_done && w->year >= w->inject_year) {
        int32_t busiest = -1;
        int32_t busiest_traffic = -1;
        for (int32_t i = 0; i < w->bridge_count; ++i) {
            if (w->bridges[i].failed) continue;
            int32_t traffic = 0;
            for (int32_t c = 0; c < w->bridges[i].cell_count; ++c) {
                traffic += w->tiles[w->bridges[i].cells[c]].traffic;
            }
            if (traffic > busiest_traffic) {
                busiest_traffic = traffic;
                busiest = i;
            }
        }
        if (busiest >= 0) {
            UrFailBridge(w, busiest, true, "Test failure");
            w->injected_done = true;
        } else {
            int32_t worked = -1;
            for (int32_t cell = 0; cell < UR_CELLS; ++cell) {
                const UrTile *tile = &w->tiles[cell];
                if (tile->year < 0 || !UrWalkableKind(tile->kind)) continue;
                if (tile->traffic <= 0) continue;
                if (worked < 0 || tile->traffic > w->tiles[worked].traffic) {
                    worked = cell;
                }
            }
            if (worked >= 0) {
                UrCollapseAround(w, worked, true, "Test failure: the shaft");
                for (int32_t i = 0; i < UR_SOURCES; ++i) {
                    UrSource *source = &w->sources[i];
                    int32_t caught = source->crowns >= 20 ? 20 : source->crowns;
                    source->crowns -= caught;
                    source->stranded += caught / UR_PORTER_LOAD;
                }
                w->injected_done = true;
            }
        }
    }

    UrMaintain(w);
}

/* ------------------------------------------------------------- the world */

static void UrSeedKnowledge(UrWorld *w, int32_t from, int32_t budget,
                            int32_t faction)
{
    static int32_t queue[UR_CELLS];
    static uint8_t seen[UR_CELLS];
    memset(seen, 0, sizeof(seen));
    if (from < 0 || !UrWalkable(w, from)) return;
    int32_t head = 0;
    int32_t tail = 0;
    queue[tail++] = from;
    seen[from] = 1U;
    while (head < tail && head < budget) {
        int32_t cell = queue[head++];
        UrKnow(w, cell, faction);
        for (int32_t dir = 0; dir < 4; ++dir) {
            int32_t next = UrStep(cell, dir);
            if (next < 0 || seen[next] || !UrWalkable(w, next)) continue;
            seen[next] = 1U;
            queue[tail++] = next;
        }
        int32_t down = UrBelow(cell);
        if (down >= 0 && w->tiles[cell].vert != UR_VERT_NONE &&
            UrWalkable(w, down) && !seen[down]) {
            seen[down] = 1U;
            queue[tail++] = down;
        }
    }
}

static void UrInit(UrWorld *w, uint32_t seed, bool inject, int32_t inject_year,
                   int32_t dragon_dies)
{
    memset(w, 0, sizeof(*w));
    w->rock = UR_MOUNTAIN_SEED;
    w->dragon_dies = dragon_dies;
    w->dragon_alive = dragon_dies != 0;
    w->crown = -1;
    w->seed = seed != 0U ? seed : 0x5eedfaceU;
    w->rng = w->seed;
    w->inject = inject;
    w->inject_year = inject_year;
    for (int32_t i = 0; i < UR_CELLS; ++i) w->tiles[i].bridge = -1;
    UrBuildMountain(w);

    static const int32_t hands[UR_FACTIONS] = { 9, 7, 6 };
    for (int32_t i = 0; i < UR_FACTIONS; ++i) {
        UrCrew *crew = &w->crews[i];
        crew->workers = hands[i];
        crew->hands_max = hands[i];
        crew->reserve = 1;
        crew->supplies = 30 - i * 4;
        crew->at = -1;
        /* What a colony knows is what it has walked: its own camp and the
         * ground around it, plus the way it has already taken to the dragon. */
        UrSeedKnowledge(w, crew->lair, 300, i);
        UrSeedKnowledge(w, w->hoard_cell, 60, i);
    }
    UrLog(w, false,
          "Natural caverns, one canyon, the old human mine, and three "
          "colonies with nothing dug between them.");
}

#ifndef UR_EMBED
static void UrConstruct(UrWorld *w, int32_t years)
{
    UrSnapshot(w, "Natural caves");
    for (w->year = 1; w->year <= years; ++w->year) {
        UrRunYear(w);
        if (w->snap_at > 0 && w->year == w->snap_at) {
            UrSnapshot(w, "verification point");
        }
        if (w->year == 5) UrSnapshot(w, "First tunnels");
        if (w->year == 20) UrSnapshot(w, "Bridges and bypasses");
        if (w->year == years) UrSnapshot(w, "A true underroad");
    }
    w->year = years;
}

#endif /* UR_EMBED */

/* --------------------------------------------------------------- glyphs */

static char UrGlyph(const UrWorld *w, int32_t cell, bool show_vert)
{
    const UrTile *tile = &w->tiles[cell];
    if (show_vert && UrWalkableKind(tile->kind)) {
        if (tile->vert == UR_VERT_FISSURE) return 'o';
        if (tile->vert == UR_VERT_LADDER) return 'H';
        if (tile->vert == UR_VERT_RAMP) return 'V';
    }
    switch (tile->kind) {
        case UR_ROCK: return '#';
        case UR_RUBBLE: return '%';
        case UR_VOID: return ':';
        case UR_CAVERN: return '.';
        case UR_CUT: return ',';
        case UR_CHAMBER: return 'O';
        case UR_RECESS: return '_';
        case UR_BRIDGE: return '=';
        case UR_CACHE: return 'C';
        case UR_SOURCE: return '*';
        case UR_HOARD: return '$';
        case UR_LAIR: return 'h';
        case UR_STRONG: return '&';
        case UR_BARRICADE: return 'X';
        case UR_MOUTH: return 'A';
        default: return ' ';
    }
}

static char UrSnapGlyph(uint8_t glyph)
{
    return glyph == '#' ? ' ' : (char)glyph;
}

static void UrPrintLegend(void)
{
    (void)printf("  @ you   A mine mouth   . natural cavern   , goblin cut"
                 "   O turning chamber\n"
                 "  = bridge   : canyon air   %% collapse   _ spoil/recess"
                 "   C cache   * loot   $ hoard\n"
                 "  o fissure   H ladder   V ramp   # rock   g crew"
                 "   (blank: not yet explored)\n");
}

/* The company's automap records discovery; it never refreshes itself. */
static void UrPrintMap(const UrWorld *w, int32_t layer, int32_t player_cell,
                       bool omniscient)
{
    (void)printf("\n  Underroad, layer %d %s  (year %d)\n", layer,
                 layer == 0 ? "- the old mine level" :
                 (layer == UR_LAYERS - 1 ? "- the deep road" : "- the workings"),
                 w->year);
    for (int32_t y = 0; y < UR_H; ++y) {
        (void)printf("  ");
        for (int32_t x = 0; x < UR_W; ++x) {
            int32_t cell = UrCellAt(layer, x, y);
            char glyph;
            if (cell == player_cell) {
                glyph = '@';
            } else if (omniscient || w->tiles[cell].seen != 0U) {
                glyph = UrGlyph(w, cell, true);
                if (!omniscient && w->tiles[cell].kind == UR_ROCK) glyph = '#';
                for (int32_t i = 0; i < UR_CREWS && glyph != 'g'; ++i) {
                    if (w->crews[i].at == cell &&
                        w->crews[i].state == UR_CREW_WORK) {
                        glyph = 'g';
                    }
                }
            } else {
                glyph = ' ';
            }
            (void)putchar(glyph);
        }
        (void)putchar('\n');
    }
}

/* ------------------------------------------------------- first person */

#define UR_VIEW_W 49
#define UR_VIEW_H 15

static char g_view[UR_VIEW_H][UR_VIEW_W + 1];
static const int32_t UrViewX0[6] = { 0, 6, 11, 15, 18, 20 };
static const int32_t UrViewX1[6] = { 48, 42, 37, 33, 30, 28 };
static const int32_t UrViewY0[6] = { 0, 2, 3, 4, 5, 6 };
static const int32_t UrViewY1[6] = { 14, 12, 11, 10, 9, 8 };

static void UrPlot(int32_t x, int32_t y, char ch)
{
    if (x < 0 || x >= UR_VIEW_W || y < 0 || y >= UR_VIEW_H) return;
    g_view[y][x] = ch;
}

static void UrFill(int32_t x0, int32_t y0, int32_t x1, int32_t y1, char ch)
{
    for (int32_t y = y0; y <= y1; ++y) {
        for (int32_t x = x0; x <= x1; ++x) UrPlot(x, y, ch);
    }
}

static void UrLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, char ch)
{
    int32_t steps = (x1 - x0 > 0 ? x1 - x0 : x0 - x1);
    int32_t vsteps = (y1 - y0 > 0 ? y1 - y0 : y0 - y1);
    if (vsteps > steps) steps = vsteps;
    if (steps == 0) { UrPlot(x0, y0, ch); return; }
    for (int32_t i = 0; i <= steps; ++i) {
        int32_t x = x0 + (x1 - x0) * i / steps;
        int32_t y = y0 + (y1 - y0) * i / steps;
        UrPlot(x, y, ch);
    }
}

static char UrFloorGlyph(const UrWorld *w, int32_t cell)
{
    const UrTile *tile = &w->tiles[cell];
    switch (tile->kind) {
        case UR_BRIDGE: return '=';
        case UR_VOID: return ':';
        case UR_RUBBLE: return '%';
        case UR_CACHE: return 'C';
        case UR_SOURCE: return '*';
        case UR_HOARD: return '$';
        case UR_LAIR: return 'h';
        case UR_STRONG: return '&';
        case UR_BARRICADE: return 'X';
        default: break;
    }
    if (tile->vert == UR_VERT_FISSURE) return 'o';
    if (tile->vert == UR_VERT_LADDER) return 'H';
    if (tile->vert == UR_VERT_RAMP) return 'V';
    if (tile->wear >= 5) return '~';      /* polished by years of porters */
    if (tile->kind == UR_CUT || tile->kind == UR_CHAMBER) return ',';
    return '.';
}

static bool UrSightBlocked(const UrWorld *w, int32_t cell)
{
    if (cell < 0) return true;
    uint8_t kind = w->tiles[cell].kind;
    return kind == UR_ROCK || kind == UR_RUBBLE;
}

static void UrRenderView(const UrWorld *w, int32_t cell, int32_t facing)
{
    for (int32_t y = 0; y < UR_VIEW_H; ++y) {
        memset(g_view[y], ' ', (size_t)UR_VIEW_W);
        g_view[y][UR_VIEW_W] = '\0';
    }
    int32_t ahead[6];
    int32_t depth = 0;
    int32_t walk = cell;
    for (; depth < 5; ++depth) {
        ahead[depth] = walk;
        if (depth > 0 && UrSightBlocked(w, walk)) break;
        walk = UrStep(walk, facing);
        if (walk < 0) { ahead[++depth] = -1; break; }
    }
    int32_t last = depth < 5 ? depth : 4;

    /* The face that stops the eye. */
    char wall = (ahead[last] >= 0 && w->tiles[ahead[last]].kind == UR_RUBBLE) ?
                    '%' : '#';
    UrFill(UrViewX0[last], UrViewY0[last], UrViewX1[last], UrViewY1[last],
           wall);

    for (int32_t d = last - 1; d >= 0; --d) {
        int32_t here = ahead[d];
        int32_t x0 = UrViewX0[d];
        int32_t x1 = UrViewX1[d];
        int32_t y0 = UrViewY0[d];
        int32_t y1 = UrViewY1[d];
        int32_t nx0 = UrViewX0[d + 1];
        int32_t nx1 = UrViewX1[d + 1];
        int32_t ny0 = UrViewY0[d + 1];
        int32_t ny1 = UrViewY1[d + 1];

        /* Side walls, or the dark of a passage leading off. */
        int32_t left_dir = (facing + 3) % 4;
        int32_t right_dir = (facing + 1) % 4;
        int32_t left = UrStep(here, left_dir);
        int32_t right = UrStep(here, right_dir);
        if (UrSightBlocked(w, left)) {
            UrFill(x0, ny0, nx0, ny1, '|');
        } else {
            UrFill(x0, ny0, nx0, ny1, ' ');
            UrFill(x0, ny1, nx0, ny1, UrFloorGlyph(w, left < 0 ? here : left));
        }
        if (UrSightBlocked(w, right)) {
            UrFill(nx1, ny0, x1, ny1, '|');
        } else {
            UrFill(nx1, ny0, x1, ny1, ' ');
            UrFill(nx1, ny1, x1, ny1,
                   UrFloorGlyph(w, right < 0 ? here : right));
        }

        /* Floor and ceiling of this stretch. */
        char floor_glyph = UrFloorGlyph(w, here);
        UrFill(nx0, ny1 + 1, nx1, y1, floor_glyph);
        UrFill(nx0, y0, nx1, ny0 - 1, ' ');

        UrLine(x0, y0, nx0, ny0, '\\');
        UrLine(x1, y0, nx1, ny0, '/');
        UrLine(x0, y1, nx0, ny1, '/');
        UrLine(x1, y1, nx1, ny1, '\\');

        /* What is actually standing in this stretch of passage, drawn on
         * the floor the player is looking at. */
        int32_t mid = (nx0 + nx1) / 2;
        int32_t band = (ny1 + y1) / 2;
        const UrTile *tile = &w->tiles[here];
        if (tile->kind == UR_HOARD) {
            UrFill(mid - 3, band, mid + 3, band, '$');
        } else if (tile->kind == UR_SOURCE) {
            UrPlot(mid, band, '*');
        } else if (tile->kind == UR_CACHE) {
            UrPlot(mid - 1, band, 'C');
            UrPlot(mid + 1, band, 'C');
        }
        if ((tile->flags & UR_FLAG_SUPPORT) != 0U) {
            UrPlot(nx0 + 1, band, '[');
            UrPlot(nx1 - 1, band, ']');
        }
        if ((tile->flags & UR_FLAG_ARROW) != 0U) {
            UrPlot(mid - 2, band, '>');
        }
        for (int32_t i = 0; i < UR_CREWS; ++i) {
            if (w->crews[i].at == here && w->crews[i].state == UR_CREW_WORK) {
                UrPlot(mid, ny1, 'g');
            }
        }
    }

    for (int32_t y = 0; y < UR_VIEW_H; ++y) (void)printf("  %s\n", g_view[y]);
}

/* ------------------------------------------------- reading the workings */

static void UrDescribe(const UrWorld *w, int32_t cell, bool detailed)
{
    const UrTile *tile = &w->tiles[cell];
    const UrNode *node = UrNodeNear(w, cell, 3);
    (void)printf("\n  %s", node != NULL ? node->name : "Unmapped working");
    switch (tile->kind) {
        case UR_MOUTH: (void)printf(" - the adit, daylight behind you"); break;
        case UR_CAVERN: (void)printf(" - natural cavern floor"); break;
        case UR_CUT: (void)printf(" - a cut passage"); break;
        case UR_CHAMBER: (void)printf(" - a turning chamber"); break;
        case UR_RECESS: (void)printf(" - a recess off the passage"); break;
        case UR_BRIDGE: (void)printf(" - out on the span"); break;
        case UR_CACHE: (void)printf(" - a staging cache"); break;
        case UR_SOURCE: (void)printf(" - a loot cache"); break;
        case UR_HOARD: (void)printf(" - the dragon's hoard"); break;
        case UR_LAIR: (void)printf(" - a goblin colony's camp"); break;
        case UR_STRONG: (void)printf(" - a colony's strongroom"); break;
        case UR_BARRICADE: (void)printf(" - a barricade across the way"); break;
        default: break;
    }
    (void)printf(".\n");

    if (tile->year >= 0) {
        const char *why = "cut for hauling";
        switch (tile->purpose) {
            case UR_PURPOSE_BYPASS: why = "cut around a failure"; break;
            case UR_PURPOSE_WIDEN: why = "widened for bulky loads"; break;
            case UR_PURPOSE_RECESS: why = "cut as a passing place"; break;
            case UR_PURPOSE_DESCENT: why = "worked as a way down"; break;
            case UR_PURPOSE_APPROACH: why = "cut as a crossing approach"; break;
            case UR_PURPOSE_ABANDONED: why = "cut for a route since given up";
                break;
            case UR_PURPOSE_HOME: why = "cut to carry loot home"; break;
            case UR_PURPOSE_DEFENCE: why = "built against a rival colony";
                break;
            case UR_PURPOSE_MINE: why = "old human work"; break;
            default: break;
        }
        (void)printf("  Worked in year %d by %s, %s.\n", tile->year,
                     tile->crew > 0U ? w->crews[tile->crew - 1U].name :
                                       "hands unknown", why);
    } else if (tile->purpose == UR_PURPOSE_MINE) {
        (void)printf("  Square-cut by the old mine company, long before any "
                     "goblin came this way.\n");
    } else if (tile->kind != UR_ROCK) {
        (void)printf("  Nobody cut this. The mountain was already open here."
                     "\n");
    }
    if (tile->wear >= 6) {
        (void)printf("  The floor is worn to a shine down the middle.\n");
    } else if (tile->wear >= 2) {
        (void)printf("  Scuffed floor; something heavy came through often.\n");
    }
    if ((tile->flags & UR_FLAG_MARKS) != 0U) {
        (void)printf("  Fresh tool marks; the dust has not settled.\n");
    }
    if ((tile->flags & UR_FLAG_ARROW) != 0U) {
        (void)printf("  A faded arrow is painted at head height.\n");
    }
    if ((tile->flags & UR_FLAG_SUPPORT) != 0U) {
        (void)printf("  Timbering, replaced more than once.\n");
    }
    if ((tile->flags & UR_FLAG_SPOIL) != 0U) {
        (void)printf("  Spoil is heaped here, tipped from the workings.\n");
    }
    if ((tile->flags & UR_FLAG_ABANDONED) != 0U) {
        (void)printf("  No traffic in years. Nobody maintains this.\n");
    }
    if (tile->kind == UR_BRIDGE) {
        int32_t index = tile->bridge;
        if (index >= 0) {
            const UrBridge *bridge = &w->bridges[index];
            (void)printf("  The span was built in year %d; condition %d/100, "
                         "rated for %s.\n", bridge->built_year,
                         bridge->condition,
                         bridge->capacity >= 3 ? "crates" :
                         (bridge->capacity >= 2 ? "porters" : "one goblin"));
            if (bridge->condition < 45) {
                (void)printf("  It moves underfoot. Planks are missing.\n");
            }
        }
    }
    if (tile->kind == UR_LAIR) {
        (void)printf("  Sleeping shelves, cold cook-fires, and tally marks "
                     "cut by the door.\n");
    }
    if (tile->purpose == UR_PURPOSE_ANCIENT) {
        (void)printf("  This is old road. Nobody alive cut it, and the "
                     "colonies only keep what they use of it.\n");
    }
    {
        static const char *const stone_name[UR_STONE_COUNT] = {
            "wet shale", "sandstone", "limestone", "granite",
            "quartz-veined rock"
        };
        (void)printf("  The rock here is %s.\n", stone_name[tile->stone]);
    }
    if (tile->kind == UR_STRONG) {
        (void)printf("  This is where a colony keeps what it carried home "
                     "rather than gave away.\n");
    }
    if (tile->kind == UR_BARRICADE) {
        (void)printf("  Loads do not pass this. That was the point of it.\n");
    }
    if (tile->vert == UR_VERT_FISSURE) {
        (void)printf("  A natural fissure drops away here (rope needed).\n");
    } else if (tile->vert == UR_VERT_LADDER) {
        (void)printf("  A goblin ladder goes down.\n");
    } else if (tile->vert == UR_VERT_RAMP) {
        (void)printf("  A cut ramp descends, wide enough for a crate.\n");
    }
    if (detailed) {
        for (int32_t dir = 0; dir < 4; ++dir) {
            int32_t next = UrStep(cell, dir);
            if (next < 0) continue;
            const UrTile *other = &w->tiles[next];
            if (other->kind == UR_VOID) {
                (void)printf("  To the %s the floor ends: canyon air.\n",
                             UrDirName[dir]);
            } else if (other->kind == UR_RUBBLE) {
                (void)printf("  To the %s, fallen ground blocks the way%s.\n",
                             UrDirName[dir],
                             (other->flags & UR_FLAG_COLLAPSED) != 0U ?
                                 " (it came down years after it was cut)" : "");
            } else if (UrWalkableKind(other->kind)) {
                (void)printf("  A way %s%s.\n", UrDirName[dir],
                             other->clear >= 3 ? ", wide" :
                             (other->clear <= 1 ? ", a squeeze" : ""));
            }
        }
    }
}

/* ------------------------------------------------------------ the crawl
 *
 * One authoritative action path: the view, the automap and the legal move
 * all ask the same question of the same tiles. Turning is free; a step
 * costs a step and burns lamp oil. Nothing here mutates the mountain.
 */

typedef struct UrPlayer {
    int32_t cell;
    int32_t facing;
    int32_t steps;
    int32_t lamp;
    int32_t entered_cell;
    bool carrying_rope;
} UrPlayer;

static bool UrPlayerMayEnter(const UrWorld *w, int32_t cell)
{
    if (cell < 0 || cell >= UR_CELLS) return false;
    const UrTile *tile = &w->tiles[cell];
    /* A barricade stops porters and crates. One person with no load can get
     * over it, which is the whole reason the company gets anywhere. */
    if (tile->kind == UR_BARRICADE) return true;
    if (!UrWalkableKind(tile->kind)) return false;
    if (tile->kind == UR_BRIDGE) return UrBridgeCondition(w, cell) >= 25;
    return tile->clear >= 1;
}

static void UrRemember(UrWorld *w, const UrPlayer *player)
{
    w->tiles[player->cell].seen = 1U;
    for (int32_t dir = 0; dir < 4; ++dir) {
        int32_t next = UrStep(player->cell, dir);
        if (next >= 0) w->tiles[next].seen = 1U;
    }
    int32_t walk = player->cell;
    for (int32_t depth = 0; depth < 6; ++depth) {
        walk = UrStep(walk, player->facing);
        if (walk < 0) break;
        w->tiles[walk].seen = 1U;
        for (int32_t dir = 0; dir < 4; ++dir) {
            int32_t side = UrStep(walk, dir);
            if (side >= 0) w->tiles[side].seen = 1U;
        }
        if (UrSightBlocked(w, walk)) break;
    }
    int32_t down = UrBelow(player->cell);
    if (down >= 0 && w->tiles[player->cell].vert != UR_VERT_NONE) {
        w->tiles[down].seen = 1U;
    }
}

static void UrStatus(const UrWorld *w, const UrPlayer *player)
{
    (void)printf("  [layer %d  %s-facing  step %d  lamp %d%s  "
                 "year %d of the workings]\n",
                 UrLayerOf(player->cell), UrDirName[player->facing],
                 player->steps, player->lamp,
                 player->lamp < 20 ? "  (guttering)" : "", w->year);
}

static void UrFrame(UrWorld *w, UrPlayer *player, bool detailed)
{
    UrRemember(w, player);
    (void)printf("\n");
    UrRenderView(w, player->cell, player->facing);
    UrDescribe(w, player->cell, detailed);
    UrStatus(w, player);
}

static bool UrTryStep(UrWorld *w, UrPlayer *player, int32_t dir,
                      bool quiet)
{
    int32_t next = UrStep(player->cell, dir);
    if (next < 0) {
        if (!quiet) (void)printf("\n  Solid rock. The mountain does not care.\n");
        return false;
    }
    const UrTile *tile = &w->tiles[next];
    if (tile->kind == UR_VOID) {
        if (!quiet) (void)printf("\n  Nothing that way but air and a long drop. The "
                     "canyon runs below.\n");
        w->tiles[next].seen = 1U;
        return false;
    }
    if (tile->kind == UR_BARRICADE && !quiet) {
        (void)printf("\n  You climb the barricade. Timber, spoil and broken "
                     "cart-boards, stacked by somebody who meant it.\n");
    }
    if (tile->kind == UR_RUBBLE) {
        if (!quiet) (void)printf("\n  Fallen ground, packed tight. Somebody's route "
                     "ended here%s.\n",
                     (tile->flags & UR_FLAG_COLLAPSED) != 0U ?
                         " the year it came down" : "");
        w->tiles[next].seen = 1U;
        return false;
    }
    if (!UrPlayerMayEnter(w, next)) {
        if (quiet) return false;
        if (!UrWalkableKind(tile->kind)) {
            (void)printf("\n  The passage ends in rock here.\n");
        } else if (tile->kind == UR_BRIDGE) {
            (void)printf("\n  What is left of the span will not hold "
                         "anyone.\n");
        } else {
            (void)printf("\n  Too tight to pass with a pack on.\n");
        }
        return false;
    }
    player->cell = next;
    player->steps += 1;
    if (player->lamp > 0) player->lamp -= 1;
    return true;
}

static bool UrTryVertical(UrWorld *w, UrPlayer *player, bool down,
                          bool quiet)
{
    int32_t from = player->cell;
    int32_t target = down ? UrBelow(from) : UrAbove(from);
    int32_t vert = down ? w->tiles[from].vert :
                          (target >= 0 ? w->tiles[target].vert : UR_VERT_NONE);
    if (target < 0 || vert == UR_VERT_NONE) {
        if (!quiet) (void)printf("\n  No way %s from here.\n", down ? "down" : "up");
        return false;
    }
    if (!UrPlayerMayEnter(w, target)) {
        if (!quiet) {
            (void)printf("\n  The %s is choked below.\n",
                         vert == UR_VERT_FISSURE ? "fissure" : "way down");
        }
        return false;
    }
    if (vert == UR_VERT_FISSURE && !player->carrying_rope) {
        if (!quiet) (void)printf("\n  A bare fissure. Not without a rope.\n");
        return false;
    }
    player->cell = target;
    player->steps += 1;
    if (player->lamp > 0) player->lamp -= 1;
    if (!quiet) {
        (void)printf("\n  You %s the %s.\n", down ? "go down" : "climb",
                     vert == UR_VERT_FISSURE ? "fissure on the rope" :
                     (vert == UR_VERT_LADDER ? "goblin ladder" : "cut ramp"));
    }
    return true;
}

static void UrHelp(void)
{
    (void)printf("\n  w step forward   s step back   a turn left   "
                 "d turn right\n"
                 "  e go down        r climb up     x inspect      "
                 "m automap\n"
                 "  l look around    ? help         q leave the Underroad\n");
}

static void UrCrawl(UrWorld *w)
{
    UrPlayer player = {
        .cell = w->mouth_cell, .facing = 1, .steps = 0, .lamp = 240,
        .entered_cell = w->mouth_cell, .carrying_rope = true
    };
    (void)printf("\n  You leave the carriage at the yard and go in at the "
                 "adit.\n  Whatever cut these tunnels was not cutting them "
                 "for you.\n");
    UrHelp();
    UrFrame(w, &player, true);

    char line[64];
    while (fgets(line, sizeof(line), stdin) != NULL) {
        char command = line[0];
        bool moved = false;
        bool quit = false;
        switch (command) {
            case 'w':
                moved = UrTryStep(w, &player, player.facing, false);
                break;
            case 's':
                moved = UrTryStep(w, &player, (player.facing + 2) % 4, false);
                break;
            case 'a':
                player.facing = (player.facing + 3) % 4;
                moved = true;
                break;
            case 'd':
                player.facing = (player.facing + 1) % 4;
                moved = true;
                break;
            case 'e': moved = UrTryVertical(w, &player, true, false); break;
            case 'r': moved = UrTryVertical(w, &player, false, false); break;
            case 'm':
                UrRemember(w, &player);
                UrPrintMap(w, UrLayerOf(player.cell), player.cell, false);
                UrPrintLegend();
                break;
            case 'x':
            case 'l':
                UrDescribe(w, player.cell, true);
                break;
            case '?':
            case 'h': UrHelp(); break;
            case 'q': quit = true; break;
            default: (void)printf("  ? (try w s a d e r x m l q)\n"); break;
        }
        if (quit) break;
        if (moved) UrFrame(w, &player, false);
        if (player.cell == w->hoard_cell) {
            (void)printf("\n  Gold to the ceiling, and the smell of a big "
                         "animal that is not here right now.\n  Every crown "
                         "of it came up a tunnel somebody cut for the "
                         "carrying.\n");
        }
        if (player.lamp == 0) {
            (void)printf("\n  The lamp is out. You feel your way back along "
                         "the worn floor.\n");
            break;
        }
    }
    (void)printf("\n  You came out at step %d with %d lamp left, having seen "
                 "the workings of %d years.\n", player.steps, player.lamp,
                 w->year);
}

/* The deepest thing worth walking to from the adit: the hoard when the
 * goblins' road happens to reach it, otherwise the furthest working the
 * company can actually get to. A severed network is a real outcome, not a
 * generator failure. */
static int32_t UrDeepestReachable(const UrWorld *w)
{
    static uint8_t reach[UR_CELLS];
    if (UrRoute(w, w->mouth_cell, w->hoard_cell, UR_LOAD_SCOUT, 0U, NULL,
                0) > 0) {
        return w->hoard_cell;
    }
    UrReachable(w, w->mouth_cell, UR_LOAD_SCOUT, 0U, reach);
    int32_t best = w->mouth_cell;
    int32_t best_score = -1;
    for (int32_t cell = 0; cell < UR_CELLS; ++cell) {
        if (!reach[cell]) continue;
        int32_t score = UrLayerOf(cell) * 40 + UrDistance(cell, w->mouth_cell);
        if (score > best_score) {
            best_score = score;
            best = cell;
        }
    }
    return best;
}

/* A scripted out-and-back: enter, go as deep as the workings allow, return
 * to the same carriage anchor, using the crawl's own collision rules. */
static bool UrAutoWalk(UrWorld *w, bool quiet)
{
    UrPlayer player = {
        .cell = w->mouth_cell, .facing = 1, .steps = 0, .lamp = 4000,
        .entered_cell = w->mouth_cell, .carrying_rope = true
    };
    int32_t deep = UrDeepestReachable(w);
    int32_t legs[2][2] = {
        { w->mouth_cell, deep }, { deep, w->mouth_cell }
    };
    if (!quiet && deep != w->hoard_cell) {
        (void)printf("\n  No way through to the hoard this year; the road "
                     "the goblins keep open does not run that far.\n");
    }
    for (int32_t leg = 0; leg < 2; ++leg) {
        int32_t length = UrRoute(w, legs[leg][0], legs[leg][1],
                                 UR_LOAD_SCOUT, 0U, g_path, UR_STATES);
        if (length <= 0) {
            if (!quiet) {
                (void)printf("\n  No walkable way %s.\n",
                             leg == 0 ? "in" : "back");
            }
            return false;
        }
        for (int32_t i = 1; i < length; ++i) {
            int32_t from = g_path[i - 1];
            int32_t to = g_path[i];
            if (UrLayerOf(from) != UrLayerOf(to)) {
                bool down = UrLayerOf(to) > UrLayerOf(from);
                if (!UrTryVertical(w, &player, down, quiet)) return false;
            } else {
                for (int32_t dir = 0; dir < 4; ++dir) {
                    if (UrStep(from, dir) == to) { player.facing = dir; break; }
                }
                if (!UrTryStep(w, &player, player.facing, quiet)) return false;
            }
            UrRemember(w, &player);
            if (!quiet && (player.steps % 14) == 0) UrFrame(w, &player, false);
        }
        if (!quiet) {
            (void)printf("\n  --- %s ---\n",
                         leg == 0 ? (deep == w->hoard_cell ?
                                     "The hoard" : "As deep as it goes") :
                                    "Back at the adit");
            UrFrame(w, &player, true);
        }
    }
    return player.cell == player.entered_cell;
}

/* ----------------------------------------------------------- the report */

static int32_t UrCountPurpose(const UrWorld *w, int32_t purpose)
{
    int32_t count = 0;
    for (int32_t cell = 0; cell < UR_CELLS; ++cell) {
        if (w->tiles[cell].year >= 0 && w->tiles[cell].purpose == purpose) {
            ++count;
        }
    }
    return count;
}

static int32_t UrCountFlag(const UrWorld *w, uint32_t flag)
{
    int32_t count = 0;
    for (int32_t cell = 0; cell < UR_CELLS; ++cell) {
        if ((w->tiles[cell].flags & flag) != 0U) ++count;
    }
    return count;
}

static int32_t UrReachCount(const UrWorld *w, int32_t from, int32_t load)
{
    static uint8_t reach[UR_CELLS];
    UrReachable(w, from, load, 0U, reach);
    int32_t count = 0;
    for (int32_t cell = 0; cell < UR_CELLS; ++cell) count += reach[cell];
    return count;
}

static void UrCheck(UrWorld *w, const char *label, bool ok,
                    const char *detail)
{
    if (w->check_count >= 16) return;
    w->checks[w->check_count].ok = ok;
    w->checks[w->check_count].skipped = false;
    (void)snprintf(w->checks[w->check_count].label,
                   sizeof(w->checks[w->check_count].label), "%s", label);
    (void)snprintf(w->checks[w->check_count].detail,
                   sizeof(w->checks[w->check_count].detail), "%s", detail);
    w->check_count += 1;
}

static void UrNote(UrWorld *w, const char *label, const char *detail)
{
    UrCheck(w, label, true, detail);
    w->checks[w->check_count - 1].skipped = true;
}

/* How labyrinthine is it, actually? Junctions are choices, dead ends are
 * answers that stopped being useful, and loops are the difference between a
 * corridor and a maze: edges - nodes + components, counted on the walkable
 * graph the player moves through. */
typedef struct UrTopology {
    int32_t cells;
    int32_t junctions;
    int32_t dead_ends;
    int32_t loops;
    int32_t components;
    int32_t verticals;
} UrTopology;

/* `from` >= 0 measures only what a person can reach from there, which is the
 * dungeon the player actually gets: unreachable pockets are not a maze. */
static UrTopology UrMeasure(const UrWorld *w, int32_t from)
{
    static int32_t parent[UR_CELLS];
    static uint8_t reach[UR_CELLS];
    UrTopology t = { 0, 0, 0, 0, 0, 0 };
    int32_t edges = 0;
    if (from >= 0) UrReachable(w, from, UR_LOAD_SCOUT, 0U, reach);
    for (int32_t i = 0; i < UR_CELLS; ++i) parent[i] = -1;
    for (int32_t cell = 0; cell < UR_CELLS; ++cell) {
        if (!UrWalkable(w, cell)) continue;
        if (from >= 0 && !reach[cell]) continue;
        parent[cell] = cell;
        t.cells += 1;
    }
    for (int32_t cell = 0; cell < UR_CELLS; ++cell) {
        if (parent[cell] < 0) continue;
        int32_t degree = 0;
        for (int32_t dir = 0; dir < 4; ++dir) {
            int32_t next = UrStep(cell, dir);
            if (next < 0 || parent[next] < 0) continue;
            ++degree;
            if (next > cell) ++edges;
        }
        int32_t down = UrBelow(cell);
        if (down >= 0 && parent[down] >= 0 &&
            w->tiles[cell].vert != UR_VERT_NONE) {
            ++degree;
            ++edges;
            t.verticals += 1;
        }
        int32_t up = UrAbove(cell);
        if (up >= 0 && parent[up] >= 0 && w->tiles[up].vert != UR_VERT_NONE) {
            ++degree;
        }
        if (degree >= 3) t.junctions += 1;
        if (degree == 1) t.dead_ends += 1;
    }
    /* Union-find over the same edges, for the component count. */
    for (int32_t cell = 0; cell < UR_CELLS; ++cell) {
        if (parent[cell] < 0) continue;
        for (int32_t dir = 0; dir < 4; ++dir) {
            int32_t next = UrStep(cell, dir);
            if (next < 0 || parent[next] < 0) continue;
            int32_t a = cell;
            while (parent[a] != a) a = parent[a];
            int32_t b = next;
            while (parent[b] != b) b = parent[b];
            if (a != b) parent[a] = b;
        }
        int32_t down = UrBelow(cell);
        if (down >= 0 && parent[down] >= 0 &&
            w->tiles[cell].vert != UR_VERT_NONE) {
            int32_t a = cell;
            while (parent[a] != a) a = parent[a];
            int32_t b = down;
            while (parent[b] != b) b = parent[b];
            if (a != b) parent[a] = b;
        }
    }
    for (int32_t cell = 0; cell < UR_CELLS; ++cell) {
        if (parent[cell] == cell) t.components += 1;
    }
    t.loops = edges - t.cells + t.components;
    return t;
}

static void UrRunChecks(UrWorld *w)
{
    char detail[100];
    UrCheck(w, "loot actually reached the hoard", w->delivered > 0,
            "arrival transfers treasure; digging never does");
    int32_t scout_reach = UrReachCount(w, w->mouth_cell, UR_LOAD_SCOUT);
    int32_t crate_reach = UrReachCount(w, w->mouth_cell, UR_LOAD_CRATE);
    (void)snprintf(detail, sizeof(detail), "scout %d tiles, crate %d tiles",
                   scout_reach, crate_reach);
    UrCheck(w, "load class changes what is passable",
            scout_reach > crate_reach && crate_reach > 0, detail);
    {
        /* Find a bend a porter can take and a crate cannot, and confirm that
         * opening the corner is what changes it. */
        int32_t bend = -1;
        for (int32_t cell = 0; cell < UR_CELLS && bend < 0; ++cell) {
            const UrTile *tile = &w->tiles[cell];
            if (!UrWalkableKind(tile->kind) || tile->clear != 2U) continue;
            if (tile->kind == UR_HOARD || tile->kind == UR_SOURCE ||
                tile->kind == UR_STRONG) continue;
            bool ns = UrWalkable(w, UrStep(cell, 0)) ||
                      UrWalkable(w, UrStep(cell, 2));
            bool ew = UrWalkable(w, UrStep(cell, 1)) ||
                      UrWalkable(w, UrStep(cell, 3));
            if (ns && ew) bend = cell;
        }
        bool jams = bend >= 0 && !UrTurnOk(w, bend, UR_LOAD_CRATE) &&
                    UrTurnOk(w, bend, UR_LOAD_PORTER);
        UrTile saved[5];
        int32_t saved_cells[5];
        int32_t saved_count = 0;
        if (bend >= 0) {
            saved_cells[saved_count] = bend;
            saved[saved_count++] = w->tiles[bend];
            for (int32_t dir = 0; dir < 4; ++dir) {
                int32_t next = UrStep(bend, dir);
                if (next < 0) continue;
                saved_cells[saved_count] = next;
                saved[saved_count++] = w->tiles[next];
            }
        }
        UrCrew keep = w->crews[0];
        w->crews[0].supplies = 40;
        bool freed = bend >= 0 && UrWidenCorner(w, bend, 0) &&
                     UrTurnOk(w, bend, UR_LOAD_CRATE);
        for (int32_t i = 0; i < saved_count; ++i) {
            w->tiles[saved_cells[i]] = saved[i];
        }
        w->crews[0] = keep;
        (void)snprintf(detail, sizeof(detail),
                       "%d turning chambers cut during the run",
                       UrCountPurpose(w, UR_PURPOSE_WIDEN));
        UrCheck(w, "widening is what unjams a crate", jams && freed, detail);
    }
    bool failures = UrCountFlag(w, UR_FLAG_COLLAPSED) > 0;
    (void)snprintf(detail, sizeof(detail),
                   "first failure year %d, %d deliveries after it",
                   w->failure_year, w->recovered);
    if (!failures || w->failure_year == 0) {
        UrNote(w, "failure persists and hauling recovers",
               "nothing fell down in this run: nothing to recover from");
    } else {
        UrCheck(w, "failure persists and hauling recovers", w->recovered > 0,
                detail);
    }
    UrCheck(w, "the wreckage is still there to walk into",
            UrCountFlag(w, UR_FLAG_COLLAPSED) > 0 ||
            UrCountFlag(w, UR_FLAG_ABANDONED) > 0,
            "collapses and dead workings are never deleted");
    /* Probe the rule rather than trusting the run to have hit it: empty the
     * stores and confirm the mountain does not move for free. */
    {
        int32_t probe = -1;
        for (int32_t cell = 0; cell < UR_CELLS && probe < 0; ++cell) {
            const UrTile *tile = &w->tiles[cell];
            if (UrWalkableKind(tile->kind) && tile->clear < 3U &&
                tile->kind != UR_HOARD && tile->kind != UR_SOURCE) {
                probe = cell;
            }
        }
        UrCrew saved = w->crews[0];
        int32_t waits_before = w->waits;
        int32_t events_before = w->event_count;
        uint8_t kind_before = probe >= 0 ? w->tiles[probe].kind : 0U;
        w->crews[0].supplies = 0;
        bool moved = probe >= 0 && UrWidenCorner(w, probe, 0);
        bool refused = probe >= 0 && !moved &&
                       w->tiles[probe].kind == kind_before &&
                       w->waits > waits_before;
        w->crews[0] = saved;
        w->waits = waits_before;
        w->event_count = events_before;
        (void)snprintf(detail, sizeof(detail),
                       "stood down %d times during the run", waits_before);
        UrCheck(w, "no materials means no excavation", refused, detail);
    }

    int64_t home = 0;
    int64_t given = 0;
    for (int32_t i = 0; i < UR_FACTIONS; ++i) {
        home += w->crews[i].stored;
        given += w->crews[i].tribute;
    }
    (void)snprintf(detail, sizeof(detail),
                   "%" PRId64 " crowns given to the dragon, %" PRId64
                   " kept at home", given, home);
    UrCheck(w, "the destination follows the dragon, not the map",
            w->dragon_alive ? given > 0 : home > 0, detail);

    (void)snprintf(detail, sizeof(detail),
                   "%d raids, %d barricades, %d spans cut loose", w->raids,
                   w->barricades, w->sabotage);
    UrCheck(w, "the quarrel leaves works behind it",
            w->raids == 0 || (w->barricades + w->sabotage) > 0, detail);

    int32_t old_road = 0;
    for (int32_t cell = 0; cell < UR_CELLS; ++cell) {
        const UrTile *tile = &w->tiles[cell];
        if (tile->traffic > 0 && UrWalkableKind(tile->kind)) {
            ++old_road;
        }
    }
    if (w->dragon_dies == 0) {
        UrNote(w, "a changed destination does not unmake the mountain",
               "no tribute era in this run: nothing to outlive");
    } else {
        (void)snprintf(detail, sizeof(detail),
                       "%d tiles of tribute road still there to walk",
                       old_road);
        UrCheck(w, "a changed destination does not unmake the mountain",
                old_road > 0, detail);
    }

    bool geometry_ok = true;
    for (int32_t cell = 0; cell < UR_CELLS && geometry_ok; ++cell) {
        const UrTile *tile = &w->tiles[cell];
        if (tile->kind == UR_BRIDGE) {
            if (tile->bridge < 0 || tile->bridge >= w->bridge_count) {
                geometry_ok = false;
            } else if (w->bridges[tile->bridge].failed) {
                geometry_ok = false;
            }
        }
        if (UrWalkableKind(tile->kind) && tile->clear == 0U) geometry_ok = false;
    }
    UrCheck(w, "geometry and connectivity agree", geometry_ok,
            "every walkable tile has clearance; spans have a live structure");

    int32_t deep = UrDeepestReachable(w);
    int32_t in = UrRoute(w, w->mouth_cell, deep, UR_LOAD_SCOUT, 0U, g_path,
                         UR_STATES);
    bool collision_ok = in > 0;
    for (int32_t i = 0; i < in && collision_ok; ++i) {
        if (!UrPlayerMayEnter(w, g_path[i])) collision_ok = false;
    }
    UrCheck(w, "pathfinding uses the crawl's own collision", collision_ok,
            in > 0 ? "every routed tile is one the player may enter" :
                     "no route from the adit");
    bool out_and_back = UrAutoWalk(w, true);
    (void)snprintf(detail, sizeof(detail), "%d tiles in to %s, same way out",
                   in, deep == w->hoard_cell ? "the hoard" :
                       "the deepest working the company can reach");
    UrCheck(w, "first-person out-and-back completes", out_and_back, detail);
}

static void UrReport(UrWorld *w)
{
    UrRunChecks(w);
    (void)printf("\nUNDERROAD CONSTRUCTION - seed %u, %d years\n", w->seed,
                 w->year);
    (void)printf("  %d crowns delivered in %d arrivals (%d bulky crates).\n",
                 (int32_t)w->delivered, w->deliveries, w->crates_delivered);
    (void)printf("  %d tiles cut for hauling, %d cut around failures, "
                 "%d widened for crates,\n  %d recesses/spoil pockets, "
                 "%d tiles abandoned,\n  %d bridges built (%d of them "
                 "replacing one that fell), %d collapses.\n",
                 UrCountPurpose(w, UR_PURPOSE_HAUL),
                 UrCountPurpose(w, UR_PURPOSE_BYPASS),
                 UrCountPurpose(w, UR_PURPOSE_WIDEN),
                 UrCountPurpose(w, UR_PURPOSE_RECESS),
                 UrCountFlag(w, UR_FLAG_ABANDONED), w->bridge_count,
                 w->rebuilds, w->collapses);

    {
        UrTopology all = UrMeasure(w, -1);
        UrTopology walk = UrMeasure(w, w->mouth_cell);
        (void)printf("  %d walkable tiles in the mountain, in %d separate "
                     "networks.\n", all.cells, all.components);
        static uint8_t reach[UR_CELLS];
        UrReachable(w, w->mouth_cell, UR_LOAD_SCOUT, 0U, reach);
        int32_t natural = 0, ancient = 0, human = 0, living = 0;
        for (int32_t cell = 0; cell < UR_CELLS; ++cell) {
            if (!reach[cell]) continue;
            const UrTile *tile = &w->tiles[cell];
            if (tile->crew > 0U) ++living;
            else if (tile->purpose == UR_PURPOSE_ANCIENT) ++ancient;
            else if (tile->purpose == UR_PURPOSE_MINE) ++human;
            else ++natural;
        }
        (void)printf("  Of the walkable ground you can reach: %d natural, "
                     "%d ancient road,\n  %d old human mine, %d cut by the "
                     "colonies living here now.\n", natural, ancient, human,
                     living);
        (void)printf("  Reachable from the adit: %d tiles, %d junctions, "
                     "%d dead ends,\n  %d independent loops, %d ways up or "
                     "down.\n", walk.cells, walk.junctions, walk.dead_ends,
                     walk.loops, walk.verticals);
    }

    (void)printf("\nCOLONIES\n");
    for (int32_t i = 0; i < UR_FACTIONS; ++i) {
        const UrCrew *crew = &w->crews[i];
        (void)printf("  %-7s %2d hands  %3d tiles cut  tribute %5" PRId64
                     "  at home %5" PRId64 "  raids %d made %d suffered"
                     "  %d hunted\n",
                     crew->colour, crew->workers, crew->tiles_cut,
                     crew->tribute, crew->stored, crew->raids_made,
                     crew->raids_suffered, crew->hunted);
    }
    {
        int32_t contested = 0;
        int32_t worked_any = 0;
        for (int32_t i = 0; i < UR_SOURCES; ++i) {
            int32_t colonies = 0;
            for (int32_t f = 0; f < UR_FACTIONS; ++f) {
                if ((w->sources[i].worked_by & (1 << f)) != 0) ++colonies;
            }
            if (colonies > 1) ++contested;
            if (colonies > 0) ++worked_any;
        }
        (void)printf("  %d of the %d caches anyone reached were drawn from by "
                     "more than one colony.\n", contested, worked_any);
    }
    if (w->hosted) {
        (void)printf("  A world simulation owned the dragon, the raids and "
                     "the hands: it was %s at the end of the run.\n",
                     w->dragon_alive ? "still alive" : "dead");
        (void)printf("  %d raids between colonies since, %d barricades, "
                     "%d spans cut loose.\n", w->raids, w->barricades,
                     w->sabotage);
    } else if (w->dragon_dies == 0) {
        (void)printf("  No dragon in the mountain: nobody was owed anything, "
                     "and the loot was only loot.\n");
    } else if (w->dragon_dies < 0) {
        (void)printf("  The dragon outlived the run; tribute never "
                     "stopped.\n");
    } else {
        (void)printf("  The dragon died in year %d", w->dragon_dies);
        if (w->crown >= 0) {
            (void)printf(", and the %s crown it granted in year %d died with "
                         "it", w->crews[w->crown].colour, w->crown_year);
        }
        (void)printf(". %d raids since, %d barricades, %d spans cut "
                     "loose.\n", w->raids, w->barricades, w->sabotage);
    }

    (void)printf("\nWHAT HAPPENED\n");
    for (int32_t i = 0; i < w->event_count; ++i) {
        const UrEvent *event = &w->events[i];
        (void)printf("  Year %-3d %-12s %s\n", event->year,
                     UrSeasonName[event->season], event->text);
    }

    (void)printf("\nACCEPTANCE\n");
    for (int32_t i = 0; i < w->check_count; ++i) {
        (void)printf("  [%s] %-46s %s\n",
                     w->checks[i].skipped ? " -- " :
                         (w->checks[i].ok ? "pass" : "FAIL"),
                     w->checks[i].label, w->checks[i].detail);
    }
}

static void UrJsonString(const char *text)
{
    (void)putchar('"');
    for (const unsigned char *c = (const unsigned char *)text; *c != '\0'; ++c) {
        switch (*c) {
            case '"': (void)fputs("\\\"", stdout); break;
            case '\\': (void)fputs("\\\\", stdout); break;
            case '\n': (void)fputs("\\n", stdout); break;
            default:
                if (*c < 0x20U) (void)printf("\\u%04x", *c);
                else (void)putchar((int)*c);
                break;
        }
    }
    (void)putchar('"');
}

/* Everything the survey page needs: the dated grids, who cut each tile, the
 * colonies, the log and the checks. */
static void UrPrintJson(const UrWorld *w)
{
    (void)printf("{\"seed\":%u,\"years\":%d,\"dragon_dies\":%d,"
                 "\"crown\":%d,\"crown_year\":%d,\"delivered\":%" PRId64
                 ",\"deliveries\":%d,\"raids\":%d,\"barricades\":%d,"
                 "\"sabotage\":%d,\"collapses\":%d,\"bridges\":%d,",
                 w->seed, w->year, w->dragon_dies, w->crown, w->crown_year,
                 w->delivered, w->deliveries, w->raids, w->barricades,
                 w->sabotage, w->collapses, w->bridge_count);

    (void)printf("\"colonies\":[");
    for (int32_t i = 0; i < UR_FACTIONS; ++i) {
        const UrCrew *crew = &w->crews[i];
        (void)printf("%s{\"colour\":\"%s\",\"hands\":%d,\"cut\":%d,"
                     "\"tribute\":%" PRId64 ",\"home\":%" PRId64
                     ",\"raids\":%d,\"suffered\":%d,\"hunted\":%d,"
                     "\"lair\":%d,\"strong\":%d}",
                     i > 0 ? "," : "", crew->colour, crew->workers,
                     crew->tiles_cut, crew->tribute, crew->stored,
                     crew->raids_made, crew->raids_suffered, crew->hunted,
                     crew->lair, crew->strong);
    }

    (void)printf("],\"caches\":[");
    for (int32_t i = 0; i < UR_SOURCES; ++i) {
        (void)printf("%s{\"name\":", i > 0 ? "," : "");
        UrJsonString(w->sources[i].name);
        (void)printf(",\"left\":%d,\"crates\":%d,\"worked_by\":%d}",
                     w->sources[i].crowns, w->sources[i].crates,
                     w->sources[i].worked_by);
    }

    (void)printf("],\"layers\":[");
    for (int32_t layer = 0; layer < UR_LAYERS; ++layer) {
        (void)printf("%s[", layer > 0 ? "," : "");
        for (int32_t i = 0; i < w->snap_count; ++i) {
            const UrSnap *snap = &w->snaps[i];
            (void)printf("%s{\"year\":%d,\"dragon\":%s,\"label\":",
                         i > 0 ? "," : "", snap->year,
                         snap->dragon ? "true" : "false");
            UrJsonString(snap->label);
            (void)printf(",\"rows\":[");
            for (int32_t y = 0; y < UR_H; ++y) {
                char row[UR_W + 1];
                for (int32_t x = 0; x < UR_W; ++x) {
                    row[x] = UrSnapGlyph(snap->kind[UrCellAt(layer, x, y)]);
                }
                row[UR_W] = '\0';
                (void)printf("%s", y > 0 ? "," : "");
                UrJsonString(row);
            }
            (void)printf("],\"crew\":[");
            for (int32_t y = 0; y < UR_H; ++y) {
                char row[UR_W + 1];
                for (int32_t x = 0; x < UR_W; ++x) {
                    row[x] = (char)('0' + snap->crew[UrCellAt(layer, x, y)]);
                }
                row[UR_W] = '\0';
                (void)printf("%s", y > 0 ? "," : "");
                UrJsonString(row);
            }
            (void)printf("]}");
        }
        (void)printf("]");
    }

    {
        UrTopology t = UrMeasure(w, w->mouth_cell);
        (void)printf("],\"topology\":{\"cells\":%d,\"junctions\":%d,"
                     "\"dead_ends\":%d,\"loops\":%d,\"verticals\":%d}",
                     t.cells, t.junctions, t.dead_ends, t.loops, t.verticals);
    }
    (void)printf(",\"host\":");
    UrJsonString(w->host_note);
    (void)printf(",\"events\":[");
    for (int32_t i = 0; i < w->event_count; ++i) {
        (void)printf("%s{\"year\":%d,\"season\":\"%s\",\"injected\":%s,"
                     "\"text\":", i > 0 ? "," : "", w->events[i].year,
                     UrSeasonName[w->events[i].season],
                     w->events[i].injected ? "true" : "false");
        UrJsonString(w->events[i].text);
        (void)printf("}");
    }

    (void)printf("],\"checks\":[");
    for (int32_t i = 0; i < w->check_count; ++i) {
        (void)printf("%s{\"ok\":%s,\"skipped\":%s,\"label\":",
                     i > 0 ? "," : "", w->checks[i].ok ? "true" : "false",
                     w->checks[i].skipped ? "true" : "false");
        UrJsonString(w->checks[i].label);
        (void)printf(",\"detail\":");
        UrJsonString(w->checks[i].detail);
        (void)printf("}");
    }
    (void)printf("]}\n");
}

/* The finished mountain, for something that draws it at eye level: what each
 * tile is, who cut it, how worn it is, what marks it carries, and the names
 * of the places. */
static char UrBase36(int32_t value)
{
    if (value < 0) value = 0;
    if (value > 35) value = 35;
    return value < 10 ? (char)('0' + value) : (char)('a' + value - 10);
}

static void UrPrintGameJson(const UrWorld *w)
{
    (void)printf("{\"w\":%d,\"h\":%d,\"layers\":%d,\"years\":%d,\"seed\":%u,",
                 UR_W, UR_H, UR_LAYERS, w->year, w->seed);
    (void)printf("\"mouth\":{\"l\":%d,\"x\":%d,\"y\":%d},",
                 UrLayerOf(w->mouth_cell), UrXOf(w->mouth_cell),
                 UrYOf(w->mouth_cell));
    (void)printf("\"hoard\":{\"l\":%d,\"x\":%d,\"y\":%d},",
                 UrLayerOf(w->hoard_cell), UrXOf(w->hoard_cell),
                 UrYOf(w->hoard_cell));

    (void)printf("\"grid\":[");
    for (int32_t layer = 0; layer < UR_LAYERS; ++layer) {
        (void)printf("%s{\"glyph\":[", layer > 0 ? "," : "");
        for (int32_t y = 0; y < UR_H; ++y) {
            char row[UR_W + 1];
            for (int32_t x = 0; x < UR_W; ++x) {
                row[x] = UrGlyph(w, UrCellAt(layer, x, y), true);
            }
            row[UR_W] = '\0';
            (void)printf("%s", y > 0 ? "," : "");
            UrJsonString(row);
        }
        (void)printf("],\"crew\":[");
        for (int32_t y = 0; y < UR_H; ++y) {
            char row[UR_W + 1];
            for (int32_t x = 0; x < UR_W; ++x) {
                row[x] = (char)('0' + w->tiles[UrCellAt(layer, x, y)].crew);
            }
            row[UR_W] = '\0';
            (void)printf("%s", y > 0 ? "," : "");
            UrJsonString(row);
        }
        (void)printf("],\"wear\":[");
        for (int32_t y = 0; y < UR_H; ++y) {
            char row[UR_W + 1];
            for (int32_t x = 0; x < UR_W; ++x) {
                row[x] = UrBase36(w->tiles[UrCellAt(layer, x, y)].wear);
            }
            row[UR_W] = '\0';
            (void)printf("%s", y > 0 ? "," : "");
            UrJsonString(row);
        }
        (void)printf("],\"stone\":[");
        for (int32_t y = 0; y < UR_H; ++y) {
            char row[UR_W + 1];
            for (int32_t x = 0; x < UR_W; ++x) {
                row[x] = UrBase36(w->tiles[UrCellAt(layer, x, y)].stone);
            }
            row[UR_W] = '\0';
            (void)printf("%s", y > 0 ? "," : "");
            UrJsonString(row);
        }
        (void)printf("],\"purpose\":[");
        for (int32_t y = 0; y < UR_H; ++y) {
            char row[UR_W + 1];
            for (int32_t x = 0; x < UR_W; ++x) {
                row[x] = UrBase36(w->tiles[UrCellAt(layer, x, y)].purpose);
            }
            row[UR_W] = '\0';
            (void)printf("%s", y > 0 ? "," : "");
            UrJsonString(row);
        }
        (void)printf("],\"flags\":[");
        for (int32_t y = 0; y < UR_H; ++y) {
            char row[UR_W + 1];
            for (int32_t x = 0; x < UR_W; ++x) {
                const UrTile *tile = &w->tiles[UrCellAt(layer, x, y)];
                int32_t bits = 0;
                if ((tile->flags & UR_FLAG_MARKS) != 0U) bits |= 1;
                if ((tile->flags & UR_FLAG_ARROW) != 0U) bits |= 2;
                if ((tile->flags & UR_FLAG_SUPPORT) != 0U) bits |= 4;
                if ((tile->flags & UR_FLAG_SPOIL) != 0U) bits |= 8;
                if ((tile->flags & UR_FLAG_ABANDONED) != 0U) bits |= 16;
                row[x] = UrBase36(bits);
            }
            row[UR_W] = '\0';
            (void)printf("%s", y > 0 ? "," : "");
            UrJsonString(row);
        }
        (void)printf("],\"year\":{");
        bool first = true;
        for (int32_t y = 0; y < UR_H; ++y) {
            for (int32_t x = 0; x < UR_W; ++x) {
                const UrTile *tile = &w->tiles[UrCellAt(layer, x, y)];
                if (tile->year < 0) continue;
                (void)printf("%s\"%d\":%d", first ? "" : ",", y * UR_W + x,
                             tile->year);
                first = false;
            }
        }
        (void)printf("},\"bridge\":{");
        first = true;
        for (int32_t y = 0; y < UR_H; ++y) {
            for (int32_t x = 0; x < UR_W; ++x) {
                const UrTile *tile = &w->tiles[UrCellAt(layer, x, y)];
                if (tile->bridge < 0 || tile->kind != UR_BRIDGE) continue;
                (void)printf("%s\"%d\":%d", first ? "" : ",", y * UR_W + x,
                             w->bridges[tile->bridge].condition);
                first = false;
            }
        }
        (void)printf("}}");
    }

    (void)printf("],\"nodes\":[");
    for (int32_t i = 0; i < w->node_count; ++i) {
        (void)printf("%s{\"l\":%d,\"x\":%d,\"y\":%d,\"name\":",
                     i > 0 ? "," : "", UrLayerOf(w->nodes[i].cell),
                     UrXOf(w->nodes[i].cell), UrYOf(w->nodes[i].cell));
        UrJsonString(w->nodes[i].name);
        (void)printf("}");
    }

    (void)printf("],\"colonies\":[");
    for (int32_t i = 0; i < UR_FACTIONS; ++i) {
        const UrCrew *crew = &w->crews[i];
        (void)printf("%s{\"colour\":\"%s\",\"cut\":%d,\"home\":%" PRId64
                     ",\"tribute\":%" PRId64 ",\"raids\":%d}",
                     i > 0 ? "," : "", crew->colour, crew->tiles_cut,
                     crew->stored, crew->tribute, crew->raids_made);
    }
    (void)printf("],\"host\":");
    UrJsonString(w->host_note);
    {
        UrTopology t = UrMeasure(w, w->mouth_cell);
        (void)printf(",\"topology\":{\"cells\":%d,\"junctions\":%d,"
                     "\"dead_ends\":%d,\"loops\":%d,\"verticals\":%d}",
                     t.cells, t.junctions, t.dead_ends, t.loops, t.verticals);
    }
    (void)printf("}\n");
}

static void UrPrintSnapshots(const UrWorld *w, int32_t layer)
{
    for (int32_t i = 0; i < w->snap_count; ++i) {
        const UrSnap *snap = &w->snaps[i];
        (void)printf("\n  Year %d - %s (layer %d)\n", snap->year, snap->label,
                     layer);
        for (int32_t y = 0; y < UR_H; ++y) {
            (void)printf("  ");
            for (int32_t x = 0; x < UR_W; ++x) {
                (void)putchar(UrSnapGlyph(snap->kind[UrCellAt(layer, x, y)]));
            }
            (void)putchar('\n');
        }
    }
    (void)printf("\n  The same mountain each time. Nothing was rerolled.\n");
}

static UrWorld g_world;

#ifndef UR_EMBED
int main(int argc, char **argv)
{
    uint32_t seed = 0x5eedfaceU;
    int32_t years = 50;
    int32_t layer = 1;
    bool inject = true;
    bool report = false;
    bool dates = false;
    bool automatic = false;
    bool map_only = false;
    bool emit_json = false;
    bool emit_game = false;
    bool emit_hash = false;
    int32_t verify_at = 0;
    int32_t dragon_dies = -2;   /* -2: pick from the run length */

    for (int32_t i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (strcmp(arg, "--seed") == 0 && i + 1 < argc) {
            seed = (uint32_t)strtoul(argv[++i], NULL, 10);
        } else if (strcmp(arg, "--years") == 0 && i + 1 < argc) {
            years = (int32_t)strtol(argv[++i], NULL, 10);
        } else if (strcmp(arg, "--layer") == 0 && i + 1 < argc) {
            layer = (int32_t)strtol(argv[++i], NULL, 10);
        } else if (strcmp(arg, "--no-inject") == 0) {
            inject = false;
        } else if (strcmp(arg, "--no-dragon") == 0) {
            dragon_dies = 0;        /* nobody to pay tribute to, ever */
        } else if (strcmp(arg, "--dragon-lives") == 0) {
            dragon_dies = -1;       /* tribute for the whole run */
        } else if (strcmp(arg, "--dragon-dies") == 0 && i + 1 < argc) {
            dragon_dies = (int32_t)strtol(argv[++i], NULL, 10);
        } else if (strcmp(arg, "--report") == 0) {
            report = true;
        } else if (strcmp(arg, "--dates") == 0) {
            dates = true;
        } else if (strcmp(arg, "--auto") == 0) {
            automatic = true;
        } else if (strcmp(arg, "--map") == 0) {
            map_only = true;
        } else if (strcmp(arg, "--json") == 0) {
            emit_json = true;
        } else if (strcmp(arg, "--game") == 0) {
            emit_game = true;
        } else if (strcmp(arg, "--hash") == 0) {
            emit_hash = true;
        } else if (strcmp(arg, "--verify") == 0 && i + 1 < argc) {
            verify_at = (int32_t)strtol(argv[++i], NULL, 10);
        } else {
            (void)fprintf(stderr,
                          "usage: %s [--seed N] [--years N] [--layer N] "
                          "[--no-inject]\n       [--dragon-dies N | "
                          "--no-dragon | --dragon-lives]\n       "
                          "[--report|--dates|--auto|--map|--json|--game]\n"
                          "       [--hash] [--verify YEAR]\n",
                          argv[0]);
            return 2;
        }
    }
    if (years < 1) years = 1;
    if (layer < 0) layer = 0;
    if (layer >= UR_LAYERS) layer = UR_LAYERS - 1;

    if (dragon_dies == -2) dragon_dies = years * 3 / 5;
    if (verify_at > 0) {
        /* The same world drawn at the same year twice: once by stopping
         * there, once by passing through on the way somewhere later. */
        static UrWorld later;
        UrInit(&g_world, seed, inject, years / 4 + 2, dragon_dies);
        UrConstruct(&g_world, verify_at);
        uint64_t stopped = UrHashWorld(&g_world);
        UrInit(&later, seed, inject, years / 4 + 2, dragon_dies);
        later.snap_at = verify_at;
        UrConstruct(&later, verify_at + 40);
        uint64_t passed = 0;
        for (int32_t i = 0; i < later.snap_count; ++i) {
            if (later.snaps[i].year == (int16_t)verify_at) {
                passed = later.snaps[i].hash;
            }
        }
        (void)printf("year %d drawn by stopping there: %016" PRIx64 "\n",
                     verify_at, stopped);
        (void)printf("year %d drawn in passing to %d:  %016" PRIx64 "\n",
                     verify_at, verify_at + 40, passed);
        (void)printf("%s\n", stopped == passed ?
                     "identical: history can be drawn at any point" :
                     "DIFFERENT: the past is not reconstructible");
        return stopped == passed ? 0 : 1;
    }
    UrInit(&g_world, seed, inject, years / 4 + 2, dragon_dies);
    UrConstruct(&g_world, years);
    if (emit_hash) {
        (void)printf("%016" PRIx64 "\n", UrHashWorld(&g_world));
        return 0;
    }

    if (emit_game) {
        UrPrintGameJson(&g_world);
        return 0;
    }
    if (emit_json) {
        UrRunChecks(&g_world);
        UrPrintJson(&g_world);
        return 0;
    }
    if (report) {
        UrReport(&g_world);
        return 0;
    }
    if (dates) {
        UrPrintSnapshots(&g_world, layer);
        return 0;
    }
    if (map_only) {
        for (int32_t i = 0; i < UR_LAYERS; ++i) {
            UrPrintMap(&g_world, i, -1, true);
        }
        UrPrintLegend();
        return 0;
    }
    if (automatic) {
        (void)UrAutoWalk(&g_world, false);
        UrPrintMap(&g_world, UrLayerOf(g_world.mouth_cell), g_world.mouth_cell,
                   false);
        UrPrintLegend();
        return 0;
    }
    UrCrawl(&g_world);
    return 0;
}
#endif /* UR_EMBED */
