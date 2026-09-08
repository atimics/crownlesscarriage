#include "sim/cc_journey_internal.h"

#include <limits.h>

static int32_t MinimumI32(int32_t a, int32_t b) { return a < b ? a : b; }
static int32_t MaximumI32(int32_t a, int32_t b) { return a > b ? a : b; }

static int32_t JourneyCarriageSpeed(int32_t total_subticks)
{
    if (total_subticks <= 0) return 0;
    const int32_t represented_route_millimetres = 52000;
    return (int32_t)(((int64_t)represented_route_millimetres *
                      CC_TRAVEL_GAME_MINUTES_PER_SECOND *
                      CC_WORLD_TICKS_PER_SECOND) / total_subticks);
}

int32_t CcJourneyPaceRate(CcJourneyPace pace)
{
    switch (pace) {
        case CC_JOURNEY_PACE_CAREFUL: return 24;
        case CC_JOURNEY_PACE_STEADY: return CC_TRAVEL_GAME_MINUTES_PER_SECOND;
        case CC_JOURNEY_PACE_PUSH: return 38;
    }
    return CC_TRAVEL_GAME_MINUTES_PER_SECOND;
}

const char *CcJourneyPaceName(CcJourneyPace pace)
{
    switch (pace) {
        case CC_JOURNEY_PACE_CAREFUL: return "CAREFUL";
        case CC_JOURNEY_PACE_STEADY: return "STEADY";
        case CC_JOURNEY_PACE_PUSH: return "PUSH";
    }
    return "STEADY";
}

int32_t CcJourneyCarriageSpeedForPace(int32_t total_subticks,
                                            CcJourneyPace pace)
{
    return JourneyCarriageSpeed(total_subticks) * CcJourneyPaceRate(pace) /
        CC_TRAVEL_GAME_MINUTES_PER_SECOND;
}

uint32_t CcJourneyRoadHouseSeed(const CcSim *sim, CcId route_id)
{
    uint32_t seed = sim != NULL ? sim->world_seed : 0U;
    seed ^= (uint32_t)route_id;
    seed ^= (uint32_t)(route_id >> 32U);
    seed ^= seed >> 16U;
    seed *= UINT32_C(0x7feb352d);
    seed ^= seed >> 15U;
    return seed;
}

const char *CcJourneyGeneratedRoadHouseName(const CcSim *sim, CcId route_id)
{
    static const char *const names[] = {
        "The Lantern and Pike",
        "The Three Wheels",
        "Ash Tree House",
        "The Red Mile",
        "Pilgrim's Rest",
        "The Barrow Lantern",
        "The Fox and Fir",
        "The Broken Crown"
    };
    uint32_t seed = CcJourneyRoadHouseSeed(sim, route_id);
    return names[seed % (sizeof(names) / sizeof(names[0]))];
}

const char *CcSimRoadHouseName(const CcSim *sim, CcId route_id)
{
    const CcRoadSite *site = CcSimRoadHouseSite(sim, route_id);
    if (site != NULL) return site->name;
    return CcJourneyGeneratedRoadHouseName(sim, route_id);
}

static int32_t RoadHouseTargetProgress(const CcSim *sim, CcId route_id)
{
    const CcRoadSite *site = CcSimRoadHouseSite(sim, route_id);
    int32_t progress = site != NULL ? site->progress_milli :
        300 + (int32_t)(CcJourneyRoadHouseSeed(sim, route_id) % 401U);
    const CcRoute *route = CcSimRoute(sim, route_id);
    CcId origin_id = sim != NULL && sim->journey.active ?
        sim->journey.origin_id : sim != NULL ? sim->player.location_id : 0U;
    if (route != NULL && origin_id == route->to_id) progress = 1000 - progress;
    return progress;
}

int32_t CcSimRoadHouseProgressMilli(const CcSim *sim, CcId route_id,
                                    int32_t journey_watch_count)
{
    if (journey_watch_count < 3) return 0;
    int32_t target = RoadHouseTargetProgress(sim, route_id);
    int32_t best_watch = 2;
    int32_t best_distance = INT32_MAX;
    for (int32_t watch = 2; watch < journey_watch_count; watch += 2) {
        int32_t progress = watch * 1000 / journey_watch_count;
        int32_t distance = progress > target ? progress - target :
                                                target - progress;
        if (distance < best_distance) {
            best_watch = watch;
            best_distance = distance;
        }
    }
    return best_watch * 1000 / journey_watch_count;
}

int32_t CcSimRoadHouseDistanceMiles(const CcSim *sim, CcId route_id)
{
    const CcRoute *route = CcSimRoute(sim, route_id);
    if (route == NULL) return 0;
    int32_t route_miles = route->travel_days * 18 +
        4 + (int32_t)(CcJourneyRoadHouseSeed(sim, route_id) % 9U);
    int32_t progress = RoadHouseTargetProgress(sim, route_id);
    return MaximumI32(1, (route_miles * progress + 500) / 1000);
}

CcMoney CcSimRoadHouseCost(const CcSim *sim, CcId route_id)
{
    return 4 + (CcMoney)(CcJourneyRoadHouseSeed(sim, route_id) % 5U);
}

int32_t CcSimJourneyWatchCount(const CcSim *sim)
{
    if (sim == NULL || !sim->journey.active ||
        sim->journey.total_subticks <= 0) return 0;
    return sim->journey.total_subticks / CC_WORLD_WATCH_SUBTICKS;
}

int32_t CcSimJourneyWatchNumber(const CcSim *sim)
{
    int32_t watch_count = CcSimJourneyWatchCount(sim);
    if (watch_count <= 0) return 0;
    if (sim->journey.phase == CC_JOURNEY_PHASE_RESTING) {
        return MinimumI32(
            watch_count,
            sim->journey.elapsed_subticks / CC_WORLD_WATCH_SUBTICKS);
    }
    return MinimumI32(
        watch_count,
        sim->journey.elapsed_subticks / CC_WORLD_WATCH_SUBTICKS + 1);
}

CcJourneyStopKind CcSimJourneyStop(const CcSim *sim)
{
    if (sim == NULL || !sim->journey.active ||
        sim->journey.phase != CC_JOURNEY_PHASE_RESTING ||
        sim->journey.elapsed_subticks <= 0 ||
        sim->journey.elapsed_subticks >= sim->journey.total_subticks ||
        sim->journey.elapsed_subticks % CC_WORLD_WATCH_SUBTICKS != 0) {
        return CC_JOURNEY_STOP_NONE;
    }
    int32_t completed_watch =
        sim->journey.elapsed_subticks / CC_WORLD_WATCH_SUBTICKS;
    return completed_watch % 2 == 0 ? CC_JOURNEY_STOP_OVERNIGHT :
                                      CC_JOURNEY_STOP_MIDDAY;
}

bool CcSimJourneyRoadHouseAvailable(const CcSim *sim)
{
    if (CcSimJourneyStop(sim) != CC_JOURNEY_STOP_OVERNIGHT) return false;
    int32_t watch_count = CcSimJourneyWatchCount(sim);
    int32_t house_progress = CcSimRoadHouseProgressMilli(
        sim, sim->journey.route_id, watch_count);
    int32_t completed_watch =
        sim->journey.elapsed_subticks / CC_WORLD_WATCH_SUBTICKS;
    return completed_watch * 1000 / watch_count == house_progress;
}

int32_t CcSimJourneyEtaMinutes(const CcSim *sim)
{
    if (sim == NULL || !sim->journey.active ||
        sim->journey.elapsed_subticks >= sim->journey.total_subticks) return 0;
    int32_t remaining = sim->journey.total_subticks -
                        sim->journey.elapsed_subticks;
    int32_t pace_rate = CcJourneyPaceRate(sim->journey.pace);
    int64_t world_subticks =
        ((int64_t)remaining * CC_TRAVEL_GAME_MINUTES_PER_SECOND +
         pace_rate - 1) / pace_rate;
    int32_t first_boundary = sim->journey.elapsed_subticks /
        CC_WORLD_WATCH_SUBTICKS + 1;
    if (sim->journey.phase == CC_JOURNEY_PHASE_RESTING &&
        CcSimJourneyStop(sim) == CC_JOURNEY_STOP_OVERNIGHT) {
        world_subticks += CC_WORLD_WATCH_SUBTICKS;
    }
    int32_t watch_count = CcSimJourneyWatchCount(sim);
    for (int32_t watch = first_boundary; watch < watch_count; ++watch) {
        if (watch % 2 == 0) world_subticks += CC_WORLD_WATCH_SUBTICKS;
    }
    return (int32_t)((world_subticks + CC_WORLD_MINUTE_SUBTICKS - 1) /
                     CC_WORLD_MINUTE_SUBTICKS);
}

const CcRoadSite *CcSimJourneyRoadSiteStop(const CcSim *sim)
{
    if (sim == NULL || sim->schema_version < 39U ||
        !sim->journey.active ||
        sim->journey.phase != CC_JOURNEY_PHASE_TRAVELLING) return NULL;
    const CcRoute *route = CcSimRoute(sim, sim->journey.route_id);
    if (route == NULL) return NULL;
    for (int32_t i = 0; i < sim->road_site_count; ++i) {
        const CcRoadSite *site = &sim->road_sites[i];
        if (site->route_id != route->id ||
            (sim->journey.road_site_stop_mask & (UINT32_C(1) << i)) != 0U)
            continue;
        int32_t progress = sim->journey.origin_id == route->from_id ?
            site->progress_milli : 1000 - site->progress_milli;
        int32_t distance = sim->carriage.progress_milli - progress;
        if (distance >= -20 && distance <= 30) return site;
    }
    return NULL;
}

