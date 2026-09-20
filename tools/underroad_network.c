/*
 * underroad_network.c - the Underroad as a generated road network.
 *
 * Prototype for docs/design/underroad-network.md (phases P1). Standalone:
 * nothing here links against the simulation, and nothing is written to a save.
 * It fixes the generation contract the sim will later adopt.
 *
 * The shape follows tools/underroad_construction.c: a jittered node layout is
 * genned from the world seed, a connected road graph is grown over it (k-nearest
 * candidates + a minimum spanning tree + a few loop edges), and each road
 * carries the attributes goblin labour will later move (depth, clearance,
 * condition, security, toll, owning clan). The mountain is invariant; the roads
 * differ per world.
 *
 *   cc -std=c17 -O2 -Wall -Wextra -o underroad_network tools/underroad_network.c
 *   ./underroad_network                 # nodes, roads, acceptance report
 *   ./underroad_network --seed 7 --map  # ASCII network map
 *   ./underroad_network --json          # machine-readable graph
 */
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define UR_NET_MAX_NODES 16
#define UR_NET_MAX_ROADS 32
#define UR_NET_LAYERS 4
#define UR_NET_CELL_SCALE 8 /* world cells per unit of map distance */

/* The mountain is the same everywhere; only the digging is seeded. */
#define UR_NET_TERRAIN_SEED UINT32_C(0x4d4f554e)

typedef enum UrNetNodeKind {
    UR_NET_NODE_ENTRANCE = 0, /* settlement mouth: mine / cellar / well */
    UR_NET_NODE_LAIR,         /* goblin clan lair */
    UR_NET_NODE_HOARD,        /* the dragon's threshold */
    UR_NET_NODE_JUNCTION,     /* carved crossroads */
    UR_NET_NODE_KIND_COUNT
} UrNetNodeKind;

typedef enum UrNetRoadKind {
    UR_NET_ROAD_HAUL = 0,
    UR_NET_ROAD_SMUGGLER,
    UR_NET_ROAD_NATURAL,
    UR_NET_ROAD_KIND_COUNT
} UrNetRoadKind;

typedef struct UrNetNode {
    int32_t id;
    UrNetNodeKind kind;
    int32_t settlement_id; /* anchor settlement, or -1 */
    int32_t faction_id;    /* owning clan for lairs, else -1 */
    int32_t map_x;
    int32_t map_y;
    int32_t depth;
    char name[32];
    uint32_t seed;
} UrNetNode;

typedef struct UrNetRoad {
    int32_t id;
    int32_t from_node;
    int32_t to_node;
    UrNetRoadKind kind;
    int32_t depth;
    int32_t length_cells;
    int32_t clearance;
    int32_t condition;
    int32_t security;
    int32_t toll_milli;
    int32_t dig_progress_milli;
    int32_t faction_id;
    uint32_t seed;
} UrNetRoad;

typedef struct UrNetGraph {
    uint32_t layout_seed;
    uint32_t revision;
    bool generated;
    int32_t node_count;
    int32_t road_count;
    UrNetNode nodes[UR_NET_MAX_NODES];
    UrNetRoad roads[UR_NET_MAX_ROADS];
} UrNetGraph;

/* ------------------------------------------------------------------ */
/* deterministic RNG                                                  */
/* ------------------------------------------------------------------ */

static uint32_t UrNetMix(uint32_t x)
{
    x ^= x >> 16;
    x *= UINT32_C(0x7feb352d);
    x ^= x >> 15;
    x *= UINT32_C(0x846ca68b);
    x ^= x >> 16;
    return x;
}

static uint32_t UrNetNext(uint32_t *state)
{
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static int32_t UrNetRange(uint32_t *state, int32_t span)
{
    if (span <= 1) {
        return 0;
    }
    return (int32_t)(UrNetNext(state) % (uint32_t)span);
}

/* ------------------------------------------------------------------ */
/* fixed anchors (mirror CcSimInit in src/sim/cc_sim.c)               */
/* ------------------------------------------------------------------ */

typedef struct UrNetAnchor {
    const char *name;
    int32_t map_x;
    int32_t map_y;
} UrNetAnchor;

static const UrNetAnchor UR_NET_SETTLEMENTS[6] = {
    {"Thornford", 125, 22},
    {"Gloamgate", 355, 445},
    {"Alderwatch", 535, 325},
    {"Silverwick", 755, 455},
    {"Rosespire", 770, 145},
    {"Hollowbarrow", 335, 155},
};

/* The three clans of the Cinder Tithe lair near Silverwick (settlement 3). */
static const UrNetAnchor UR_NET_LAIRS[3] = {
    {"Red Lair", 725, 425},
    {"Purple Lair", 785, 430},
    {"Blue Lair", 760, 495},
};

static const UrNetAnchor UR_NET_HOARD = {"Hoard Threshold", 360, 120};

/* ------------------------------------------------------------------ */
/* union-find                                                         */
/* ------------------------------------------------------------------ */

static int32_t UrNetFind(int32_t *parent, int32_t i)
{
    while (parent[i] != i) {
        parent[i] = parent[parent[i]];
        i = parent[i];
    }
    return i;
}

static bool UrNetUnion(int32_t *parent, int32_t a, int32_t b)
{
    int32_t ra = UrNetFind(parent, a);
    int32_t rb = UrNetFind(parent, b);
    if (ra == rb) {
        return false;
    }
    parent[ra] = rb;
    return true;
}

/* ------------------------------------------------------------------ */
/* construction                                                       */
/* ------------------------------------------------------------------ */

static void UrNetAddNode(UrNetGraph *g, UrNetNodeKind kind, int32_t settlement_id,
                         int32_t faction_id, const char *name, int32_t x,
                         int32_t y, uint32_t *state)
{
    if (g->node_count >= UR_NET_MAX_NODES) {
        return;
    }
    UrNetNode *n = &g->nodes[g->node_count];
    memset(n, 0, sizeof *n);
    n->id = g->node_count;
    n->kind = kind;
    n->settlement_id = settlement_id;
    n->faction_id = faction_id;
    n->map_x = x;
    n->map_y = y;
    n->depth = UrNetRange(state, UR_NET_LAYERS);
    n->seed = UrNetMix((uint32_t)g->node_count * UINT32_C(0x9e3779b9) ^ *state);
    snprintf(n->name, sizeof n->name, "%s", name);
    g->node_count++;
}

static int32_t UrNetDistance(int32_t ax, int32_t ay, int32_t bx, int32_t by)
{
    int64_t dx = (int64_t)ax - (int64_t)bx;
    int64_t dy = (int64_t)ay - (int64_t)by;
    double d = (double)(dx * dx + dy * dy);
    /* integer-ish square root without libm precision games */
    int64_t v = (int64_t)(d + 0.5);
    int64_t r = 0;
    while ((r + 1) * (r + 1) <= v) {
        r++;
    }
    return (int32_t)r;
}

static int32_t UrNetRoadLength(int32_t cells)
{
    int32_t scaled = cells / UR_NET_CELL_SCALE;
    return scaled < 1 ? 1 : scaled;
}

static int32_t UrNetNearestLair(const UrNetGraph *g, int32_t node)
{
    int32_t best = -1;
    int32_t best_d = 0;
    for (int32_t i = 0; i < g->node_count; i++) {
        if (g->nodes[i].kind != UR_NET_NODE_LAIR) {
            continue;
        }
        int32_t d = UrNetDistance(g->nodes[node].map_x, g->nodes[node].map_y,
                                  g->nodes[i].map_x, g->nodes[i].map_y);
        if (best < 0 || d < best_d) {
            best = i;
            best_d = d;
        }
    }
    return best < 0 ? -1 : g->nodes[best].faction_id;
}

static void UrNetAddRoad(UrNetGraph *g, int32_t a, int32_t b, uint32_t *state)
{
    if (g->road_count >= UR_NET_MAX_ROADS || a == b) {
        return;
    }
    for (int32_t i = 0; i < g->road_count; i++) {
        UrNetRoad *e = &g->roads[i];
        if ((e->from_node == a && e->to_node == b) ||
            (e->from_node == b && e->to_node == a)) {
            return;
        }
    }
    uint32_t key = UrNetMix((uint32_t)a * UINT32_C(0x85ebca6b) ^
                            (uint32_t)b * UINT32_C(0xc2b2ae35) ^ *state);
    uint32_t local = key;
    int32_t cells = UrNetDistance(g->nodes[a].map_x, g->nodes[a].map_y,
                                  g->nodes[b].map_x, g->nodes[b].map_y);
    UrNetRoad *r = &g->roads[g->road_count];
    memset(r, 0, sizeof *r);
    r->id = g->road_count;
    r->from_node = a;
    r->to_node = b;
    r->kind = UrNetRange(&local, 100) < 12 ? UR_NET_ROAD_SMUGGLER
                                           : UR_NET_ROAD_HAUL;
    r->depth = UrNetRange(&local, UR_NET_LAYERS);
    r->length_cells = UrNetRoadLength(cells);
    r->clearance = 1 + UrNetRange(&local, 3);
    r->condition = 300 + UrNetRange(&local, 700);
    r->security = UrNetRange(&local, 11);
    r->faction_id = UrNetNearestLair(g, a);
    r->toll_milli = r->kind == UR_NET_ROAD_HAUL ? 40 + UrNetRange(&local, 120) : 0;
    r->dig_progress_milli = UrNetRange(&local, 1000);
    r->seed = key;
    g->road_count++;
    UrNetNext(state);
}

typedef struct UrNetCandidate {
    int32_t a;
    int32_t b;
    int32_t distance;
} UrNetCandidate;

static int UrNetCompareCandidate(const void *lhs, const void *rhs)
{
    const UrNetCandidate *l = (const UrNetCandidate *)lhs;
    const UrNetCandidate *r = (const UrNetCandidate *)rhs;
    if (l->distance != r->distance) {
        return l->distance < r->distance ? -1 : 1;
    }
    if (l->a != r->a) {
        return l->a < r->a ? -1 : 1;
    }
    if (l->b != r->b) {
        return l->b < r->b ? -1 : 1;
    }
    return 0;
}

static void UrNetBuild(UrNetGraph *g, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    g->layout_seed = seed;
    g->revision = 1;

    uint32_t state = UrNetMix(seed ^ UR_NET_TERRAIN_SEED);

    for (int32_t i = 0; i < 6; i++) {
        int32_t jx = UR_NET_SETTLEMENTS[i].map_x + UrNetRange(&state, 41) - 20;
        int32_t jy = UR_NET_SETTLEMENTS[i].map_y + UrNetRange(&state, 41) - 20;
        UrNetAddNode(g, UR_NET_NODE_ENTRANCE, i, -1, UR_NET_SETTLEMENTS[i].name,
                     jx, jy, &state);
    }
    for (int32_t i = 0; i < 3; i++) {
        int32_t jx = UR_NET_LAIRS[i].map_x + UrNetRange(&state, 21) - 10;
        int32_t jy = UR_NET_LAIRS[i].map_y + UrNetRange(&state, 21) - 10;
        UrNetAddNode(g, UR_NET_NODE_LAIR, 3, i, UR_NET_LAIRS[i].name, jx, jy,
                     &state);
    }
    UrNetAddNode(g, UR_NET_NODE_HOARD, 5, -1, UR_NET_HOARD.name,
                 UR_NET_HOARD.map_x, UR_NET_HOARD.map_y, &state);

    /* Candidate edges: every pair, nearest first. */
    UrNetCandidate candidates[UR_NET_MAX_NODES * UR_NET_MAX_NODES];
    int32_t candidate_count = 0;
    for (int32_t a = 0; a < g->node_count; a++) {
        for (int32_t b = a + 1; b < g->node_count; b++) {
            candidates[candidate_count].a = a;
            candidates[candidate_count].b = b;
            candidates[candidate_count].distance =
                UrNetDistance(g->nodes[a].map_x, g->nodes[a].map_y,
                              g->nodes[b].map_x, g->nodes[b].map_y);
            candidate_count++;
        }
    }
    qsort(candidates, (size_t)candidate_count, sizeof candidates[0],
          UrNetCompareCandidate);

    /* Kruskal: connect everything first. */
    int32_t parent[UR_NET_MAX_NODES];
    for (int32_t i = 0; i < g->node_count; i++) {
        parent[i] = i;
    }
    for (int32_t i = 0; i < candidate_count; i++) {
        if (UrNetUnion(parent, candidates[i].a, candidates[i].b)) {
            UrNetAddRoad(g, candidates[i].a, candidates[i].b, &state);
        }
    }
    /* Then the shortest remaining edges add loops, until the road budget or a
     * cap on per-node degree is reached. */
    int32_t degree[UR_NET_MAX_NODES] = {0};
    for (int32_t i = 0; i < g->road_count; i++) {
        degree[g->roads[i].from_node]++;
        degree[g->roads[i].to_node]++;
    }
    for (int32_t i = 0; i < candidate_count && g->road_count < UR_NET_MAX_ROADS;
         i++) {
        int32_t a = candidates[i].a;
        int32_t b = candidates[i].b;
        if (degree[a] >= 4 || degree[b] >= 4) {
            continue;
        }
        int32_t before = g->road_count;
        UrNetAddRoad(g, a, b, &state);
        if (g->road_count > before) {
            degree[a]++;
            degree[b]++;
        }
    }
    g->generated = true;
}

/* ------------------------------------------------------------------ */
/* reporting                                                          */
/* ------------------------------------------------------------------ */

#ifndef CC_UR_NETWORK_NO_MAIN
static const char *UrNetNodeGlyph(UrNetNodeKind kind)
{
    switch (kind) {
        case UR_NET_NODE_ENTRANCE: return "town";
        case UR_NET_NODE_LAIR:     return "lair";
        case UR_NET_NODE_HOARD:    return "hoard";
        case UR_NET_NODE_JUNCTION: return "xroad";
        default:                   return "?";
    }
}
#endif

static uint64_t UrNetHash(const UrNetGraph *g)
{
    uint64_t h = UINT64_C(0xcbf29ce484222325);
    for (int32_t i = 0; i < g->node_count; i++) {
        h = (h ^ (uint64_t)(uint32_t)g->nodes[i].map_x) * UINT64_C(0x100000001b3);
        h = (h ^ (uint64_t)(uint32_t)g->nodes[i].map_y) * UINT64_C(0x100000001b3);
        h = (h ^ (uint64_t)(uint32_t)g->nodes[i].kind) * UINT64_C(0x100000001b3);
    }
    for (int32_t i = 0; i < g->road_count; i++) {
        const UrNetRoad *r = &g->roads[i];
        h = (h ^ (uint64_t)(uint32_t)r->from_node) * UINT64_C(0x100000001b3);
        h = (h ^ (uint64_t)(uint32_t)r->to_node) * UINT64_C(0x100000001b3);
        h = (h ^ (uint64_t)(uint32_t)r->condition) * UINT64_C(0x100000001b3);
        h = (h ^ (uint64_t)(uint32_t)r->faction_id) * UINT64_C(0x100000001b3);
    }
    return h;
}

static int32_t UrNetConnected(const UrNetGraph *g)
{
    int32_t parent[UR_NET_MAX_NODES];
    for (int32_t i = 0; i < g->node_count; i++) {
        parent[i] = i;
    }
    for (int32_t i = 0; i < g->road_count; i++) {
        UrNetUnion(parent, g->roads[i].from_node, g->roads[i].to_node);
    }
    int32_t root = UrNetFind(parent, 0);
    for (int32_t i = 1; i < g->node_count; i++) {
        if (UrNetFind(parent, i) != root) {
            return 0;
        }
    }
    return 1;
}

#ifndef CC_UR_NETWORK_NO_MAIN
static void UrNetPrintMap(const UrNetGraph *g)
{
    enum { COLS = 80, ROWS = 24 };
    char grid[ROWS][COLS];
    for (int32_t y = 0; y < ROWS; y++) {
        for (int32_t x = 0; x < COLS; x++) {
            grid[y][x] = ' ';
        }
    }
    for (int32_t i = 0; i < g->road_count; i++) {
        const UrNetRoad *r = &g->roads[i];
        const UrNetNode *a = &g->nodes[r->from_node];
        const UrNetNode *b = &g->nodes[r->to_node];
        int32_t steps = UrNetRoadLength(UrNetDistance(a->map_x, a->map_y,
                                                      b->map_x, b->map_y));
        if (steps < 1) {
            steps = 1;
        }
        for (int32_t s = 0; s <= steps; s++) {
            int32_t mx = a->map_x + (b->map_x - a->map_x) * s / steps;
            int32_t my = a->map_y + (b->map_y - a->map_y) * s / steps;
            int32_t cx = mx / 13;
            int32_t cy = my / 25;
            if (cx >= 0 && cx < COLS && cy >= 0 && cy < ROWS) {
                grid[cy][cx] = '*';
            }
        }
    }
    for (int32_t i = 0; i < g->node_count; i++) {
        const UrNetNode *n = &g->nodes[i];
        int32_t cx = n->map_x / 13;
        int32_t cy = n->map_y / 25;
        if (cx < 0 || cx >= COLS || cy < 0 || cy >= ROWS) {
            continue;
        }
        char glyph = '?';
        switch (n->kind) {
            case UR_NET_NODE_ENTRANCE: glyph = 'T'; break;
            case UR_NET_NODE_LAIR:     glyph = 'L'; break;
            case UR_NET_NODE_HOARD:    glyph = 'H'; break;
            default:                   glyph = 'x'; break;
        }
        grid[cy][cx] = glyph;
    }
    for (int32_t y = 0; y < ROWS; y++) {
        for (int32_t x = 0; x < COLS; x++) {
            putchar(grid[y][x]);
        }
        putchar('\n');
    }
    printf("legend: T town  L lair  H hoard  * goblin road\n");
}

static void UrNetPrintJson(const UrNetGraph *g)
{
    printf("{\"seed\":%" PRIu32 ",\"revision\":%" PRIu32
           ",\"nodes\":[",
           g->layout_seed, g->revision);
    for (int32_t i = 0; i < g->node_count; i++) {
        const UrNetNode *n = &g->nodes[i];
        printf("%s{\"id\":%" PRId32 ",\"kind\":\"%s\",\"name\":\"%s\","
               "\"x\":%" PRId32 ",\"y\":%" PRId32 ",\"depth\":%" PRId32
               ",\"faction\":%" PRId32 "}",
               i ? "," : "", n->id, UrNetNodeGlyph(n->kind), n->name, n->map_x,
               n->map_y, n->depth, n->faction_id);
    }
    printf("],\"roads\":[");
    for (int32_t i = 0; i < g->road_count; i++) {
        const UrNetRoad *r = &g->roads[i];
        printf("%s{\"id\":%" PRId32 ",\"from\":%" PRId32 ",\"to\":%" PRId32
               ",\"depth\":%" PRId32 ",\"length\":%" PRId32
               ",\"clearance\":%" PRId32 ",\"condition\":%" PRId32
               ",\"security\":%" PRId32 ",\"toll_milli\":%" PRId32
               ",\"faction\":%" PRId32 "}",
               i ? "," : "", r->id, r->from_node, r->to_node, r->depth,
               r->length_cells, r->clearance, r->condition, r->security,
               r->toll_milli, r->faction_id);
    }
    printf("]}\n");
}

static int UrNetReport(const UrNetGraph *g)
{
    int failures = 0;
    printf("UNDERROAD NETWORK  seed=%" PRIu32 "  nodes=%" PRId32
           "  roads=%" PRId32 "\n",
           g->layout_seed, g->node_count, g->road_count);
    for (int32_t i = 0; i < g->node_count; i++) {
        const UrNetNode *n = &g->nodes[i];
        printf("  node %2" PRId32 "  %-9s %-16s (%4" PRId32 ",%4" PRId32
               ") depth=%" PRId32 "%s\n",
               n->id, UrNetNodeGlyph(n->kind), n->name, n->map_x, n->map_y,
               n->depth,
               n->faction_id >= 0 ? "" : "");
    }
    for (int32_t i = 0; i < g->road_count; i++) {
        const UrNetRoad *r = &g->roads[i];
        printf("  road %2" PRId32 "  %2" PRId32 "->%2" PRId32
               "  len=%-3" PRId32 " clear=%" PRId32 " cond=%-4" PRId32
               " sec=%-2" PRId32 " toll=%-4" PRId32 " clan=%" PRId32 "\n",
               r->id, r->from_node, r->to_node, r->length_cells, r->clearance,
               r->condition, r->security, r->toll_milli, r->faction_id);
    }

    printf("\nACCEPTANCE\n");
    bool connected = UrNetConnected(g) == 1;
    printf("  [%s] every entrance reaches every other\n",
           connected ? "pass" : "FAIL");
    failures += connected ? 0 : 1;

    bool capped = g->node_count <= UR_NET_MAX_NODES &&
                  g->road_count <= UR_NET_MAX_ROADS;
    printf("  [%s] node and road caps respected (%" PRId32 "/%" PRId32
           ", %" PRId32 "/%" PRId32 ")\n",
           capped ? "pass" : "FAIL", g->node_count, UR_NET_MAX_NODES,
           g->road_count, UR_NET_MAX_ROADS);
    failures += capped ? 0 : 1;

    int32_t min_degree = UR_NET_MAX_ROADS;
    bool depths_ok = true;
    for (int32_t i = 0; i < g->node_count; i++) {
        int32_t degree = 0;
        for (int32_t j = 0; j < g->road_count; j++) {
            if (g->roads[j].from_node == i || g->roads[j].to_node == i) {
                degree++;
            }
        }
        if (degree < min_degree) {
            min_degree = degree;
        }
    }
    for (int32_t i = 0; i < g->road_count; i++) {
        if (g->roads[i].depth < 0 || g->roads[i].depth >= UR_NET_LAYERS) {
            depths_ok = false;
        }
        if (g->roads[i].from_node == g->roads[i].to_node) {
            depths_ok = false;
        }
    }
    printf("  [%s] no isolated node (min degree %" PRId32 ")\n",
           min_degree >= 1 ? "pass" : "FAIL", min_degree);
    failures += min_degree >= 1 ? 0 : 1;

    printf("  [%s] every road sits on a valid layer\n", depths_ok ? "pass" : "FAIL");
    failures += depths_ok ? 0 : 1;

    UrNetGraph again;
    UrNetBuild(&again, g->layout_seed);
    bool deterministic = UrNetHash(&again) == UrNetHash(g) &&
                         again.road_count == g->road_count;
    printf("  [%s] same seed rebuilds the same network (hash %016" PRIx64 ")\n",
           deterministic ? "pass" : "FAIL", UrNetHash(g));
    failures += deterministic ? 0 : 1;

    printf("\n  %s (%d failed)\n", failures == 0 ? "ALL PASS" : "FAILURES",
           failures);
    return failures;
}
#endif /* CC_UR_NETWORK_NO_MAIN */

/* ------------------------------------------------------------------ */
/* main                                                               */
/* ------------------------------------------------------------------ */

#ifndef CC_UR_NETWORK_NO_MAIN

int main(int argc, char **argv)
{
    uint32_t seed = 1;
    bool map = false;
    bool json = false;
    bool report = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            seed = (uint32_t)strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--map") == 0) {
            map = true;
        } else if (strcmp(argv[i], "--json") == 0) {
            json = true;
        } else if (strcmp(argv[i], "--report") == 0) {
            report = true;
        } else {
            fprintf(stderr, "unknown argument '%s'\n", argv[i]);
            fprintf(stderr,
                    "usage: %s [--seed N] [--map] [--json] [--report]\n",
                    argv[0]);
            return 1;
        }
    }

    UrNetGraph g;
    UrNetBuild(&g, seed);

    if (json) {
        UrNetPrintJson(&g);
        return 0;
    }
    if (map) {
        UrNetPrintMap(&g);
        return 0;
    }
    (void)report;
    return UrNetReport(&g) == 0 ? 0 : 1;
}

#endif /* CC_UR_NETWORK_NO_MAIN */
