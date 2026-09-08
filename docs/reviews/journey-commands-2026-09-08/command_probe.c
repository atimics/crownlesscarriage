#include "sim/cc_sim.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

static CcSim sim;
static char error[256];

static void Prepare(uint32_t seed, uint32_t schema, int32_t route_slot, int direction)
{
    CcSimInit(&sim, seed);
    sim.schema_version = schema;
    const CcRoute *road = &sim.routes[route_slot];
    CcId origin = direction ? road->to_id : road->from_id;
    CcId destination = direction ? road->from_id : road->to_id;
    sim.player.location_id = origin;
    sim.player.coins = 10000;
    sim.journey = (CcJourneyEncounter){.active = true, .phase = CC_JOURNEY_PHASE_TRAVELLING,
        .origin_id = origin, .destination_id = destination, .route_id = road->id,
        .total_subticks = 4 * CC_WORLD_WATCH_SUBTICKS, .pace = CC_JOURNEY_PACE_STEADY,
        .encounter_triggered = true, .ambush_resolved = true, .danger = 20,
        .bargain_cost = 1, .fare_reserved = 1, .departure_day = sim.current_day};
    sim.carriage.location_id = origin;
    sim.carriage.route_id = road->id;
    sim.carriage.origin_id = origin;
    sim.carriage.destination_id = destination;
    sim.carriage.mode = CC_CARRIAGE_MOVING;
    sim.clock.game_minutes_per_second = CC_TRAVEL_GAME_MINUTES_PER_SECOND;
    sim.clock.minute_subticks = CC_WORLD_DAY_SUBTICKS - CC_WORLD_WATCH_SUBTICKS;
    for (int32_t horse = 0; horse < CcSimHorseTeamCount(&sim); ++horse) {
        sim.horse_team[horse].fatigue = 45;
        sim.horse_team[horse].hunger = 30;
    }
    CcCommand pace = {.kind = CC_COMMAND_SET_JOURNEY_PACE, .amount = CC_JOURNEY_PACE_STEADY};
    if (!CcSimApply(&sim, &pace, error, sizeof(error))) {
        (void)fprintf(stderr, "Journey setup failed: %s\n", error);
        exit(1);
    }
}

static void Stop(int watches)
{
    sim.journey.elapsed_subticks = watches * CC_WORLD_WATCH_SUBTICKS;
    sim.carriage.progress_milli = watches * 250;
    sim.journey.phase = CC_JOURNEY_PHASE_RESTING;
    sim.carriage.mode = CC_CARRIAGE_STOPPED;
    sim.carriage.speed_milli_per_second = 0;
    sim.clock.game_minutes_per_second = CC_IDLE_GAME_MINUTES_PER_SECOND;
}

static void Record(const char *label, CcCommand command)
{
    bool success = CcSimApply(&sim, &command, error, sizeof(error));
    printf("%u %u %s %d %d %016" PRIx64 " %s\n", sim.world_seed,
        sim.schema_version, label, (int)command.kind, success ? 1 : 0, CcSimHash(&sim), error);
}

int main(void)
{
    const uint32_t schemas[] = {26,33,38,39,40,41,59,60};
    const uint32_t seeds[] = {UINT32_C(0x5eed0001),UINT32_C(0xc0a71a9e)};
    for (size_t schema = 0; schema < sizeof(schemas)/sizeof(schemas[0]); ++schema) {
        for (size_t seed = 0; seed < sizeof(seeds)/sizeof(seeds[0]); ++seed) {
            for (int direction = 0; direction < 2; ++direction) {
                for (int pace = -1; pace <= 3; ++pace) {
                    Prepare(seeds[seed], schemas[schema], 0, direction);
                    Record("pace", (CcCommand){.kind = CC_COMMAND_SET_JOURNEY_PACE, .amount = pace});
                    if (pace >= 0 && pace <= 2) {
                        CcSimAdvanceRuntimeTicks(&sim, CC_WORLD_WATCH_SUBTICKS);
                        printf("%u %u watch %d %016" PRIx64 "\n", sim.world_seed,
                            sim.schema_version, pace, CcSimHash(&sim));
                    }
                }
                const CcCommandKind choices[] = {CC_COMMAND_TAKE_JOURNEY_BREAK,
                    CC_COMMAND_PRESS_ON, CC_COMMAND_MAKE_CAMP, CC_COMMAND_LODGE_ROAD_HOUSE};
                for (int watch = 1; watch <= 2; ++watch) {
                    for (size_t choice = 0; choice < sizeof(choices)/sizeof(choices[0]); ++choice) {
                        for (int funded = 0; funded < 2; ++funded) {
                            Prepare(seeds[seed], schemas[schema], 0, direction);
                            Stop(watch);
                            if (!funded) sim.player.coins = 0;
                            Record("stop", (CcCommand){.kind = choices[choice]});
                        }
                    }
                }
                for (int camp = 0; camp < 2; ++camp) {
                    Prepare(seeds[seed], schemas[schema], 0, direction);
                    const CcRoadSite *site = &sim.road_sites[0];
                    for (int32_t route = 0; route < sim.route_count; ++route) {
                        if (sim.routes[route].id == site->route_id) {
                            Prepare(seeds[seed], schemas[schema], route, direction);
                            break;
                        }
                    }
                    site = &sim.road_sites[0];
                    sim.carriage.progress_milli = direction ? 1000 - site->progress_milli : site->progress_milli;
                    sim.journey.elapsed_subticks = (int32_t)((int64_t)sim.journey.total_subticks * sim.carriage.progress_milli / 1000);
                    Record("site", (CcCommand){.kind = camp ? CC_COMMAND_CAMP_ROAD_SITE : CC_COMMAND_PASS_ROAD_SITE,
                        .target_id = site->id});
                }
            }
        }
    }
    return 0;
}
