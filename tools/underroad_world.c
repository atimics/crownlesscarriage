/*
 * underroad_world.c - the Underroad, dug by an actual simulated world.
 *
 * tools/underroad_construction.c grows a mountain from its own invented
 * hauling demand. This runs the real Crownless simulation instead and lets
 * that world supply the demand: the goblin society's raids decide what has
 * to be carried underground, faction membership decides how many hands are
 * at the face, and the dragon's life decides whether the colonies are
 * paying tribute or fighting each other over the same loot.
 *
 * It is also a check on the simulation. The sim moves tribute through an
 * abstract room graph (`AdvanceGoblinPorter` in cc_goblin_politics.inc);
 * this builds the passages that movement would need at player scale and
 * reports where the abstract deliveries had no physical road to use.
 *
 *   crownless_underroad_world --years 60 --seed 7 --report
 *   crownless_underroad_world --json > world.json
 */
#include "sim/cc_sim.h"

#define UR_EMBED
#include "underroad_construction.c"

#include <inttypes.h>

/* The sim's three colours are the generator's three colonies, in order. */
/* A faction's goblins are the hands: the ones who dig are the ones who
 * haul. Three is the fewest that can work a face at all. */
static int32_t WorldHands(const CcGoblinFaction *faction)
{
    return faction->members > 3 ? faction->members : 3;
}

static CcMoney WorldFactionWealth(const CcGoblinFaction *faction)
{
    return faction->coins + (CcMoney)faction->gold * 40 +
           (CcMoney)faction->gems * 70;
}

static bool WorldDragonAlive(const CcSim *sim)
{
    return !sim->dragon.slain &&
           sim->dragon.life_stage != CC_DRAGON_STAGE_EGG;
}

/* Raids that actually came back this year, as crowns that now have to get
 * from the mouth of the mountain to wherever they are going. */
static int32_t WorldRaidTake(const CcSim *sim, CcId *last_event, char *headline,
                             size_t headline_capacity)
{
    int32_t take = 0;
    CcId newest = *last_event;
    for (int32_t i = 0; i < sim->event_count; ++i) {
        const CcEvent *event = &sim->events[i];
        if (event->id <= *last_event) continue;
        if (event->id > newest) newest = event->id;
        if (event->kind != CC_EVENT_GOBLIN_RAIDED) continue;
        take += event->magnitude > 0 ? event->magnitude : 0;
        if (headline != NULL && headline[0] == '\0') {
            (void)snprintf(headline, headline_capacity, "%s", event->text);
        }
    }
    *last_event = newest;
    return take;
}

static void WorldCarryEvents(UrWorld *w, const CcSim *sim, CcId *last_seen)
{
    CcId newest = *last_seen;
    for (int32_t i = 0; i < sim->event_count; ++i) {
        const CcEvent *event = &sim->events[i];
        if (event->id <= *last_seen) continue;
        if (event->id > newest) newest = event->id;
        switch (event->kind) {
            case CC_EVENT_DRAGON_HUNT:
            case CC_EVENT_DRAGON_CROWNED:
            case CC_EVENT_GOBLIN_RAIDED:
            case CC_EVENT_GOBLIN_TRIBUTE_DELIVERED:
                UrLog(w, false, "[world] %s", event->text);
                break;
            default: break;
        }
    }
    *last_seen = newest;
}

int main(int argc, char **argv)
{
    uint32_t seed = UINT32_C(0xc0a71a9e);
    int32_t years = 60;
    int32_t layer = 1;
    bool report = true;
    bool emit_json = false;
    bool emit_game = false;
    bool dates = false;
    bool map_only = false;
    bool slay_dragon_early = false;
    int32_t slay_dragon_year = 0;
    bool crawl = false;

    for (int32_t i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (strcmp(arg, "--seed") == 0 && i + 1 < argc) {
            seed = (uint32_t)strtoul(argv[++i], NULL, 10);
        } else if (strcmp(arg, "--years") == 0 && i + 1 < argc) {
            years = (int32_t)strtol(argv[++i], NULL, 10);
        } else if (strcmp(arg, "--layer") == 0 && i + 1 < argc) {
            layer = (int32_t)strtol(argv[++i], NULL, 10);
        } else if (strcmp(arg, "--json") == 0) {
            emit_json = true;
            report = false;
        } else if (strcmp(arg, "--game") == 0) {
            emit_game = true;
            report = false;
        } else if (strcmp(arg, "--dates") == 0) {
            dates = true;
            report = false;
        } else if (strcmp(arg, "--map") == 0) {
            map_only = true;
            report = false;
        } else if (strcmp(arg, "--slay-dragon-year") == 0 && i + 1 < argc) {
            slay_dragon_year = (int32_t)strtol(argv[++i], NULL, 10);
        } else if (strcmp(arg, "--slay-dragon") == 0) {
            slay_dragon_early = true;
        } else if (strcmp(arg, "--crawl") == 0) {
            crawl = true;
            report = false;
        } else if (strcmp(arg, "--report") == 0) {
            report = true;
        } else {
            (void)fprintf(stderr,
                          "usage: %s [--seed N] [--years N] [--layer N] "
                          "[--slay-dragon] [--slay-dragon-year N]\n       "
                          "[--report|--json|--game|--dates|--map|--crawl]\n",
                          argv[0]);
            return 2;
        }
    }
    if (years < 1) years = 1;
    if (years > 400) years = 400;
    if (layer < 0) layer = 0;
    if (layer >= UR_LAYERS) layer = UR_LAYERS - 1;

    static CcSim sim;
    CcSimInit(&sim, seed);
    if (slay_dragon_early) {
        sim.dragon.slain = true;
        sim.dragon.slain_day = 1;
        sim.dragon.life_stage = CC_DRAGON_STAGE_AFTERDRAGON;
    }

    UrWorld *w = &g_world;
    UrInit(w, seed ^ UINT32_C(0x9e3779b9), false, 0, -1);
    w->hosted = true;
    w->dragon_alive = WorldDragonAlive(&sim);
    UrLog(w, false,
          "[world] %s, %d goblins in three colours, dragon %s.",
          sim.goblins.name, sim.goblins.members,
          w->dragon_alive ? "alive" : "already dead");

    (void)snprintf(w->host_note, sizeof(w->host_note),
                   "%s, %d goblins in three colours", sim.goblins.name,
                   sim.goblins.members);

    CcId last_event = 0;
    CcId last_carried = 0;
    int32_t hauled_by_world = 0;
    int32_t raid_years = 0;
    int64_t raid_total = 0;
    bool announced_death = !w->dragon_alive;

    for (w->year = 1; w->year <= years; ++w->year) {
        CcSimAdvanceDays(&sim, 365);
        if (slay_dragon_year > 0 && w->year == slay_dragon_year &&
            !sim.dragon.slain) {
            /* Somebody in the world killed it. The sim owns the aftermath;
             * the mountain only finds out that tribute has stopped. */
            sim.dragon.slain = true;
            sim.dragon.slain_day = sim.current_day;
            sim.dragon.life_stage = CC_DRAGON_STAGE_AFTERDRAGON;
        }

        char headline[CC_EVENT_TEXT_CAPACITY];
        headline[0] = '\0';
        int32_t take = WorldRaidTake(&sim, &last_event, headline,
                                     sizeof(headline));
        if (take > 0) {
            /* A raid's take is staged at the mouth; a big one arrives with
             * something nobody can split. */
            UrAddLoot(w, 0, take, take >= 300 ? 1 : 0);
            raid_total += take;
            raid_years += 1;
        }

        for (int32_t f = 0; f < UR_FACTIONS; ++f) {
            UrSetHands(w, f, WorldHands(&sim.goblin_politics.factions[f]));
        }
        w->dragon_alive = WorldDragonAlive(&sim);
        w->crown = sim.goblin_politics.crown_faction;
        if (w->crown >= 0 && w->crown_year == 0) w->crown_year = w->year;
        if (!w->dragon_alive && !announced_death) {
            announced_death = true;
            UrLog(w, false,
                  "[world] %s is dead on day %d. Nobody is owed tribute, and "
                  "every colony starts carrying its take home.",
                  sim.dragon.name, sim.dragon.slain_day);
        }

        WorldCarryEvents(w, &sim, &last_carried);
        UrRunYear(w);

        if (w->year == 1) UrSnapshot(w, "First simulated year");
        if (w->year == years / 4) UrSnapshot(w, "Early workings");
        if (w->year == years / 2) UrSnapshot(w, "Mid history");
        if (w->year == years) UrSnapshot(w, "As the world left it");
    }
    w->year = years;
    (void)snprintf(w->host_note, sizeof(w->host_note),
                   "%s, %d goblins; dragon %s; hoard %" PRId64 " crowns",
                   sim.goblins.name, sim.goblins.members,
                   WorldDragonAlive(&sim) ? "alive" :
                       (sim.dragon.slain_day > 0 ? "slain" : "gone"),
                   sim.dragon.hoard);
    for (int32_t f = 0; f < UR_FACTIONS; ++f) {
        hauled_by_world += sim.goblin_politics.factions[f].deliveries;
    }

    if (emit_game) {
        UrPrintGameJson(w);
        return 0;
    }
    if (emit_json) {
        UrRunChecks(w);
        UrPrintJson(w);
        return 0;
    }
    if (dates) {
        UrPrintSnapshots(w, layer);
        return 0;
    }
    if (map_only) {
        for (int32_t i = 0; i < UR_LAYERS; ++i) UrPrintMap(w, i, -1, true);
        UrPrintLegend();
        return 0;
    }
    if (crawl) {
        (void)printf("\n  This mountain was dug by %s over %d simulated "
                     "years.\n", sim.goblins.name, years);
        UrCrawl(w);
        return 0;
    }
    if (report) {
        UrReport(w);
        (void)printf("\nTHE WORLD THAT DUG IT\n");
        (void)printf("  %s: %d goblins, cohesion %d, %d tributes delivered, "
                     "%d raids that brought something back.\n",
                     sim.goblins.name, sim.goblins.members,
                     sim.goblins.cohesion, sim.goblins.tributes_delivered,
                     raid_years);
        (void)printf("  Raids carried %" PRId64 " crowns of loot to the "
                     "mountain over %d years.\n", raid_total, years);
        (void)printf("  Dragon %s; hoard %" PRId64 " crowns; crown faction "
                     "%s.\n",
                     WorldDragonAlive(&sim) ? "alive" : "dead",
                     sim.dragon.hoard,
                     CcGoblinColorName(sim.goblin_politics.crown_faction));
        for (int32_t f = 0; f < UR_FACTIONS; ++f) {
            const CcGoblinFaction *faction =
                &sim.goblin_politics.factions[f];
            (void)printf("  %-7s %4d members, %5" PRId64 " crowns of wealth, "
                         "%4" PRId64 " tribute, %d graph deliveries; the "
                         "mountain gave them %d tiles of road.\n",
                         CcGoblinColorName(f), faction->members,
                         WorldFactionWealth(faction), faction->tribute,
                         faction->deliveries, w->crews[f].tiles_cut);
        }
        int32_t tiles = 0;
        int32_t goblin_years = 0;
        for (int32_t f = 0; f < UR_FACTIONS; ++f) {
            tiles += w->crews[f].tiles_cut;
            goblin_years += sim.goblin_politics.factions[f].members * years;
        }
        (void)printf("\n  GRAPH AGAINST GROUND\n");
        (void)printf("  The simulation moved %d tribute deliveries through "
                     "its room graph.\n  The physical Underroad carried %d "
                     "arrivals over the same years.\n", hauled_by_world,
                     w->deliveries);
        (void)printf("  %d goblin-years of labour cut %d tiles of passage "
                     "and %d spans.\n", goblin_years, tiles, w->bridge_count);
        if (hauled_by_world > w->deliveries * 2 && w->deliveries >= 0) {
            (void)printf("  FINDING: the graph delivered %.1fx more tribute "
                         "than a dug road could carry with these goblins.\n",
                         w->deliveries > 0 ?
                             (double)hauled_by_world / (double)w->deliveries :
                             (double)hauled_by_world);
        }
        return 0;
    }
    return 0;
}
