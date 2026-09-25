/* Cross-platform determinism trace.

   Prints a plain-text list of state hashes that must be byte-identical on
   every supported platform (macOS arm64, Linux x86_64, WebAssembly). CI runs
   it on each platform and diffs the outputs.

   Modes:
     seeds [days]        run a few fixed seeds, drive journeys, print hashes
     replay <save>...    load saves, print a hash trace for each journal step
     geometry <seed>     print every route's quantized road geometry

   For each journal step the replay trace prints the whole-state hash and one
   hash per SQLite snapshot table, so the first differing subsystem is easy
   to see when two platforms disagree. */
#include "persistence/cc_save.h"
#include "sim/cc_road_position.h"
#include "sim/cc_sim.h"

#include <inttypes.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t HashBytes(uint64_t hash, const void *data, size_t length)
{
    const unsigned char *bytes = data;
    for (size_t i = 0U; i < length; ++i) {
        hash ^= bytes[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t HashU64(uint64_t hash, uint64_t value)
{
    for (int32_t byte = 0; byte < 8; ++byte) {
        hash ^= value & UINT64_C(0xff);
        hash *= UINT64_C(1099511628211);
        value >>= 8U;
    }
    return hash;
}

static uint64_t GeometryHash(const CcRoadGeometry *geometry)
{
    uint64_t hash = UINT64_C(1469598103934665603);
    hash = HashU64(hash, geometry->route_id);
    hash = HashU64(hash, (uint64_t)(int64_t)geometry->control.x_units);
    hash = HashU64(hash, (uint64_t)(int64_t)geometry->control.z_units);
    for (int32_t i = 0; i < CC_ROAD_GEOMETRY_SAMPLE_COUNT; ++i) {
        hash = HashU64(hash,
                       (uint64_t)(int64_t)geometry->samples[i].x_units);
        hash = HashU64(hash,
                       (uint64_t)(int64_t)geometry->samples[i].z_units);
    }
    hash = HashU64(hash, (uint64_t)(int64_t)geometry->full_length_units);
    return HashU64(hash, (uint64_t)(int64_t)geometry->journey_length_units);
}

static uint64_t AllGeometryHash(const CcSim *sim)
{
    static CcRoadGeometry geometries[CC_MAX_ROUTES];
    int32_t count = CcRoadGeometryBuildAll(sim, geometries, CC_MAX_ROUTES);
    uint64_t hash = UINT64_C(1469598103934665603);
    hash = HashU64(hash, (uint64_t)(int64_t)count);
    for (int32_t i = 0; i < count; ++i) {
        hash = HashU64(hash, GeometryHash(&geometries[i]));
    }
    return hash;
}

/* Hash each table of the encoded snapshot. REAL columns hash their bit
   pattern, so any float drift shows up. */
static bool PrintTableHashes(const CcSim *sim, const char *prefix)
{
    unsigned char *bytes = NULL;
    size_t length = 0U;
    char error[256] = {0};
    if (!CcSaveEncode(sim, &bytes, &length, error, sizeof(error))) {
        printf("%s encode-failed %s\n", prefix, error);
        return false;
    }
    sqlite3 *database = NULL;
    if (sqlite3_open(":memory:", &database) != SQLITE_OK) {
        CcSaveFreeBuffer(bytes);
        return false;
    }
    unsigned char *copy = sqlite3_malloc64((sqlite3_uint64)length);
    if (copy == NULL) {
        CcSaveFreeBuffer(bytes);
        sqlite3_close(database);
        return false;
    }
    memcpy(copy, bytes, length);
    CcSaveFreeBuffer(bytes);
    if (sqlite3_deserialize(database, "main", copy, (sqlite3_int64)length,
                            (sqlite3_int64)length,
                            SQLITE_DESERIALIZE_FREEONCLOSE) != SQLITE_OK) {
        sqlite3_close(database);
        return false;
    }
    sqlite3_stmt *tables = NULL;
    if (sqlite3_prepare_v2(database,
            "SELECT name FROM sqlite_master WHERE type='table' "
            "ORDER BY name;", -1, &tables, NULL) != SQLITE_OK) {
        sqlite3_close(database);
        return false;
    }
    while (sqlite3_step(tables) == SQLITE_ROW) {
        const char *name = (const char *)sqlite3_column_text(tables, 0);
        char sql[256];
        (void)snprintf(sql, sizeof(sql), "SELECT * FROM \"%s\";", name);
        sqlite3_stmt *rows = NULL;
        if (sqlite3_prepare_v2(database, sql, -1, &rows, NULL) != SQLITE_OK)
            continue;
        uint64_t hash = UINT64_C(1469598103934665603);
        int64_t count = 0;
        int columns = sqlite3_column_count(rows);
        while (sqlite3_step(rows) == SQLITE_ROW) {
            count += 1;
            for (int column = 0; column < columns; ++column) {
                int type = sqlite3_column_type(rows, column);
                hash = HashU64(hash, (uint64_t)type);
                if (type == SQLITE_INTEGER) {
                    hash = HashU64(hash,
                        (uint64_t)sqlite3_column_int64(rows, column));
                } else if (type == SQLITE_FLOAT) {
                    double value = sqlite3_column_double(rows, column);
                    hash = HashBytes(hash, &value, sizeof(value));
                } else if (type != SQLITE_NULL) {
                    const void *blob = sqlite3_column_blob(rows, column);
                    int size = sqlite3_column_bytes(rows, column);
                    if (blob != NULL && size > 0)
                        hash = HashBytes(hash, blob, (size_t)size);
                }
            }
        }
        sqlite3_finalize(rows);
        printf("%s table %s rows=%" PRId64 " hash=%016" PRIx64 "\n",
               prefix, name, count, hash);
    }
    sqlite3_finalize(tables);
    sqlite3_close(database);
    return true;
}

static void PrintJourney(const CcSim *sim, const char *prefix)
{
    const CcJourneyEncounter *journey = &sim->journey;
    printf("%s journey active=%d route=%" PRIu64 " total_subticks=%d "
           "elapsed=%d road_active=%d geometry_length=%d leg_length=%d "
           "leg_total_subticks=%d compat=%d\n",
           prefix, journey->active ? 1 : 0, journey->route_id,
           journey->total_subticks, journey->elapsed_subticks,
           journey->road_position_active ? 1 : 0,
           journey->road_geometry_length_units,
           journey->road_leg_length_units, journey->road_leg_total_subticks,
           journey->road_compatibility_milli);
    printf("%s journey geometry", prefix);
    for (int32_t i = 0; i < 33; ++i) {
        printf(" %d,%d", journey->road_geometry_x_units[i],
               journey->road_geometry_z_units[i]);
    }
    printf("\n");
}

typedef struct ReplayContext {
    const char *label;
    bool verbose;
} ReplayContext;

static void ObserveReplay(void *context, const CcJournalReplayStep *step,
                          const CcSim *sim)
{
    const ReplayContext *replay = context;
    uint64_t hash = CcSimHash(sim);
    char prefix[160];
    (void)snprintf(prefix, sizeof(prefix), "replay %s step %" PRIu64,
                   replay->label, step->ordinal);
    printf("%s op=%d command=%d count=%d applied=%d hash=%016" PRIx64
           " committed=%016" PRIx64 " %s day=%d tick=%" PRIu64 "\n",
           prefix, step->operation_kind, step->command_kind,
           step->step_count, step->applied ? 1 : 0, hash,
           step->committed_post_state_hash,
           hash == step->committed_post_state_hash ? "match" : "DIVERGED",
           sim->current_day, sim->clock.tick);
    if (replay->verbose || hash != step->committed_post_state_hash) {
        PrintJourney(sim, prefix);
        (void)PrintTableHashes(sim, prefix);
    }
}

static const char *BaseName(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash != NULL ? slash + 1 : path;
}

static int RunReplay(int count, char **paths, bool verbose)
{
    static CcSim sim;
    int failures = 0;
    for (int i = 0; i < count; ++i) {
        ReplayContext context = {.label = BaseName(paths[i]),
                                 .verbose = verbose};
        CcJournalSetReplayObserver(ObserveReplay, &context);
        char error[256] = {0};
        bool loaded = CcSaveRead(paths[i], &sim, error, sizeof(error));
        CcJournalSetReplayObserver(NULL, NULL);
        if (!loaded) {
            printf("replay %s load-failed %s\n", context.label, error);
            failures += 1;
            continue;
        }
        printf("replay %s loaded hash=%016" PRIx64 " day=%d\n",
               context.label, CcSimHash(&sim), sim.current_day);
    }
    return failures;
}

/* Pick a route out of the player's town, in a fixed order. */
static bool NextTravel(const CcSim *sim, uint32_t turn, CcCommand *command)
{
    CcId here = sim->player.location_id;
    int32_t candidates = 0;
    for (int32_t i = 0; i < sim->route_count; ++i) {
        const CcRoute *route = &sim->routes[i];
        if (route->from_id == here || route->to_id == here) candidates += 1;
    }
    if (candidates == 0) return false;
    int32_t pick = (int32_t)(turn % (uint32_t)candidates);
    for (int32_t i = 0; i < sim->route_count; ++i) {
        const CcRoute *route = &sim->routes[i];
        if (route->from_id != here && route->to_id != here) continue;
        if (pick-- != 0) continue;
        *command = (CcCommand){
            .kind = CC_COMMAND_TRAVEL,
            .target_id = route->from_id == here ? route->to_id :
                                                  route->from_id
        };
        return true;
    }
    return false;
}

/* One journey decision, the way a player would take it. Returns the
   command kind used, or 0 when the step only let runtime pass. */
static int32_t DriveJourney(CcSim *sim, int32_t stuck_steps)
{
    char error[192];
    CcCommand command = {0};
    const CcRoadSite *site = CcSimJourneyRoadSiteStop(sim);
    if (site != NULL) {
        command = (CcCommand){.kind = CC_COMMAND_PASS_ROAD_SITE,
                              .target_id = site->id};
    } else if (sim->journey.phase == CC_JOURNEY_PHASE_ROAD_CHOICE) {
        CcRoadLegPreview previews[3];
        int32_t count = CcRoadNextLegPreviews(sim, previews, 3);
        int32_t pick = count > 0 ? 0 : -1;
        for (int32_t i = 0; i < count; ++i) {
            if (previews[i].direction == sim->journey.road_direction &&
                previews[i].segment_id != CC_PILOT_ROAD_MILL_SEGMENT_ID) {
                pick = i;
                break;
            }
        }
        if (pick >= 0) {
            command = (CcCommand){.kind = CC_COMMAND_CHOOSE_ROAD_LEG,
                                  .target_id = previews[pick].decision_token};
        }
    } else if (sim->journey.phase == CC_JOURNEY_PHASE_RESTING) {
        command.kind = CcSimJourneyStop(sim) == CC_JOURNEY_STOP_MIDDAY ?
            CC_COMMAND_TAKE_JOURNEY_BREAK : CC_COMMAND_MAKE_CAMP;
    } else if (sim->journey.phase == CC_JOURNEY_PHASE_BLOCKED) {
        command.kind = stuck_steps % 2 == 0 ?
            CC_COMMAND_RESOLVE_ENCOUNTER_NEGOTIATE :
            CC_COMMAND_WITHDRAW_ENCOUNTER;
    }
    if (command.kind != CC_COMMAND_NONE &&
        CcSimApply(sim, &command, error, sizeof(error))) {
        return (int32_t)command.kind;
    }
    CcSimAdvanceRuntimeTicks(sim, 10 * CC_WORLD_TICKS_PER_SECOND);
    return 0;
}

static void RunSeeds(int32_t days)
{
    static const uint32_t seeds[] = {
        UINT32_C(1), UINT32_C(0x205722), UINT32_C(0x3235a7ed),
        UINT32_C(0xc0a7118e), UINT32_C(0xffffffff)
    };
    static CcSim sim;
    for (size_t s = 0U; s < sizeof(seeds) / sizeof(seeds[0]); ++s) {
        CcSimInit(&sim, seeds[s]);
        printf("seed %08" PRIx32 " init hash=%016" PRIx64
               " geometry=%016" PRIx64 "\n", seeds[s], CcSimHash(&sim),
               AllGeometryHash(&sim));
        uint32_t turn = 0U;
        int32_t journeys = 0;
        while (sim.current_day < days) {
            if (!sim.journey.active) {
                CcCommand command;
                char error[192] = {0};
                bool applied = NextTravel(&sim, turn++, &command) &&
                    CcSimApply(&sim, &command, error, sizeof(error));
                printf("seed %08" PRIx32 " day %d travel %" PRIu64
                       " applied=%d hash=%016" PRIx64 "\n", seeds[s],
                       sim.current_day, applied ? command.target_id : 0U,
                       applied ? 1 : 0, CcSimHash(&sim));
                if (applied) journeys += 1;
                CcSimAdvanceDays(&sim, 2);
                printf("seed %08" PRIx32 " day %d hash=%016" PRIx64 "\n",
                       seeds[s], sim.current_day, CcSimHash(&sim));
                continue;
            }
            /* Print only a few journey steps to keep the list short;
               the next day line catches any drift in the rest. */
            int32_t step = 0;
            while (sim.journey.active && step < 4000) {
                int32_t kind = DriveJourney(&sim, step);
                if (kind != 0) {
                    printf("seed %08" PRIx32 " day %d tick %" PRIu64
                           " command %d hash=%016" PRIx64 "\n", seeds[s],
                           sim.current_day, sim.clock.tick, kind,
                           CcSimHash(&sim));
                }
                step += 1;
            }
            printf("seed %08" PRIx32 " day %d journey-end active=%d "
                   "location=%" PRIu64 " hash=%016" PRIx64 "\n", seeds[s],
                   sim.current_day, sim.journey.active ? 1 : 0,
                   sim.player.location_id, CcSimHash(&sim));
            if (sim.journey.active) break;
        }
        printf("seed %08" PRIx32 " end day=%d journeys=%d hash=%016" PRIx64
               "\n", seeds[s], sim.current_day, journeys, CcSimHash(&sim));
    }
}

static void RunGeometry(uint32_t seed)
{
    static CcSim sim;
    static CcRoadGeometry geometries[CC_MAX_ROUTES];
    CcSimInit(&sim, seed);
    int32_t count = CcRoadGeometryBuildAll(&sim, geometries, CC_MAX_ROUTES);
    for (int32_t i = 0; i < count; ++i) {
        const CcRoadGeometry *geometry = &geometries[i];
        printf("geometry %08" PRIx32 " route %" PRIu64 " full=%d journey=%d "
               "control=%d,%d samples", seed, geometry->route_id,
               geometry->full_length_units, geometry->journey_length_units,
               geometry->control.x_units, geometry->control.z_units);
        for (int32_t p = 0; p < CC_ROAD_GEOMETRY_SAMPLE_COUNT; ++p) {
            printf(" %d,%d", geometry->samples[p].x_units,
                   geometry->samples[p].z_units);
        }
        printf("\n");
    }
}

int main(int argc, char **argv)
{
    if (argc >= 2 && strcmp(argv[1], "seeds") == 0) {
        RunSeeds(argc >= 3 ? atoi(argv[2]) : 120);
        return 0;
    }
    if (argc >= 3 && strcmp(argv[1], "replay") == 0) {
        bool verbose = argc >= 4 && strcmp(argv[2], "--verbose") == 0;
        int first = verbose ? 3 : 2;
        return RunReplay(argc - first, argv + first, verbose) == 0 ? 0 : 1;
    }
    if (argc >= 3 && strcmp(argv[1], "geometry") == 0) {
        RunGeometry((uint32_t)strtoul(argv[2], NULL, 0));
        return 0;
    }
    fprintf(stderr,
            "usage: %s seeds [days] | replay [--verbose] <save>... | "
            "geometry <seed>\n", argv[0]);
    return 2;
}
