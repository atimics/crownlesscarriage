#include "sim/cc_sim.h"
#include <inttypes.h>
#include <stdio.h>

static CcSim sim;

int main(void)
{
    const uint32_t seeds[] = {UINT32_C(0x5eed0001), UINT32_C(0xc0a71a9e)};
    for (size_t seed = 0; seed < sizeof(seeds) / sizeof(seeds[0]); ++seed) {
        CcSimInit(&sim, seeds[seed]);
        for (int32_t route = 0; route < sim.route_count; ++route) {
            const CcRoute *road = &sim.routes[route];
            for (int direction = 0; direction < 2; ++direction) {
                sim.journey = (CcJourneyEncounter){.active = true, .route_id = road->id,
                    .origin_id = direction == 0 ? road->from_id : road->to_id};
                for (int32_t watches = 3; watches <= 12; ++watches) {
                    sim.journey.total_subticks = watches * CC_WORLD_WATCH_SUBTICKS;
                    for (int32_t completed = 0; completed <= watches; ++completed) {
                        sim.journey.elapsed_subticks = completed * CC_WORLD_WATCH_SUBTICKS;
                        for (int pace = CC_JOURNEY_PACE_CAREFUL; pace <= CC_JOURNEY_PACE_PUSH; ++pace) {
                            sim.journey.pace = (CcJourneyPace)pace;
                            for (int rest = 0; rest < 2; ++rest) {
                                sim.journey.phase = rest ? CC_JOURNEY_PHASE_RESTING : CC_JOURNEY_PHASE_TRAVELLING;
                                uint64_t before = CcSimHash(&sim);
                                printf("%u %" PRIu64 " %d %d %d %d %d %s %d %d %d %d %d %d %d %" PRId64 " %s\n",
                                    seeds[seed], road->id, direction, watches, completed, pace, rest,
                                    CcJourneyPaceName(sim.journey.pace), CcSimJourneyWatchCount(&sim),
                                    CcSimJourneyWatchNumber(&sim), (int)CcSimJourneyStop(&sim),
                                    CcSimJourneyRoadHouseAvailable(&sim) ? 1 : 0,
                                    CcSimJourneyEtaMinutes(&sim), CcSimRoadHouseProgressMilli(&sim, road->id, watches),
                                    CcSimRoadHouseDistanceMiles(&sim, road->id),
                                    CcSimRoadHouseCost(&sim, road->id), CcSimRoadHouseName(&sim, road->id));
                                if (CcSimHash(&sim) != before) return 1;
                            }
                        }
                    }
                }
            }
        }
    }
    return 0;
}
