#include "sim/cc_journey_internal.h"

#include "sim/cc_route_rules_internal.h"

#include <limits.h>
#include <stdio.h>

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


static int32_t ClampI32(int32_t value, int32_t minimum, int32_t maximum)
{
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static void SetError(char *error, size_t capacity, const char *message)
{
    if (error == NULL || capacity == 0U) return;
    (void)snprintf(error, capacity, "%s", message);
}

static int32_t HorseFeedRequired(int32_t travel_days)
{
    return MaximumI32(1, (travel_days + 1) / 2);
}

static bool JourneyCrossesRain(const CcSim *sim, const CcRoute *route,
                               int32_t departure_day)
{
    if (sim == NULL || route == NULL) return false;
    uint32_t value = sim->world_seed ^ (uint32_t)route->id ^
        (uint32_t)(route->id >> 32U) ^
        ((uint32_t)departure_day * UINT32_C(0x9e3779b9));
    value ^= value >> 16U;
    value *= UINT32_C(0x7feb352d);
    value ^= value >> 15U;
    value *= UINT32_C(0x846ca68b);
    value ^= value >> 16U;
    return value % 100U < 35U;
}

bool CcSimTravelPreview(const CcSim *sim, CcId destination_id,
                        CcTravelPreview *preview, char *error,
                        size_t error_capacity)
{
    if (sim == NULL || preview == NULL) {
        SetError(error, error_capacity, "Travel preview state is missing.");
        return false;
    }
    const CcSettlement *destination = CcSimSettlement(sim, destination_id);
    if (destination == NULL) {
        SetError(error, error_capacity, "That destination does not exist.");
        return false;
    }
    const CcRoute *route = CcSimRouteBetween(
        sim, sim->player.location_id, destination->id);
    if (route == NULL) {
        SetError(error, error_capacity,
                 "No direct carriage route connects those places.");
        return false;
    }
    const CcMap *map = CcSimMapForRoute(sim, route->id, sim->player.id);
    const CcSituation *accepted = CcSimAcceptedSituation(sim);
    bool sponsored_night_passage = route->smuggler_route &&
        accepted != NULL &&
        accepted->kind == CC_SITUATION_BLACK_MARKET_DELIVERY &&
        route->to_id == accepted->target_id;
    bool uncharted = map == NULL && !sponsored_night_passage;
    int32_t readiness = sim->schema_version >= 14U ?
        CcSimHorseTeamReadiness(sim) : 100;
    int32_t days = route->travel_days + (uncharted ? 2 : 0) +
                   (readiness < 70 ? 1 : 0) +
                   (readiness < 45 ? 1 : 0);
    bool opening_half_day = sim->journey.total_subticks == 0;
    int32_t travel_watches = opening_half_day ? 1 :
        MaximumI32(3, days * 2);
    int32_t base_fare = days + (route->smuggler_route ? 3 : 0);
    int32_t shadow_danger = CcRouteDragonShadowDanger(sim, route);
    bool waits_for_morning = !opening_half_day &&
        sim->clock.minute_subticks > 0;
    int32_t departure_day = sim->current_day +
        (waits_for_morning ? 1 : 0);
    *preview = (CcTravelPreview){
        .route_id = route->id,
        .destination_id = destination->id,
        .provision_cost = sim->schema_version >= 41U ? 0 :
            base_fare + CcRouteToll(sim, route),
        .travel_days = days,
        .claimed_condition = map != NULL ? map->recorded_condition : -1,
        .claimed_danger = map != NULL ?
            ClampI32(map->recorded_danger + shadow_danger, 0, 95) : -1,
        .chart_accuracy = map != NULL ? map->accuracy : 0,
        .horse_feed_required = HorseFeedRequired(days),
        .horse_readiness = readiness,
        .travel_watches = travel_watches,
        .overnight_stops = (travel_watches - 1) / 2,
        .departure_wait_minutes = waits_for_morning ?
            (CC_WORLD_DAY_SUBTICKS - sim->clock.minute_subticks) /
                CC_WORLD_MINUTE_SUBTICKS : 0,
        .road_house_distance_miles = CcSimRoadHouseDistanceMiles(
            sim, route->id),
        .road_house_cost = CcSimRoadHouseCost(sim, route->id),
        .road_house_name = CcSimRoadHouseName(sim, route->id),
        .rain_expected = JourneyCrossesRain(
            sim, route, departure_day),
        .opening_half_day = opening_half_day,
        .charted = map != NULL,
        .destination_known = !route->smuggler_route || map != NULL ||
                             sponsored_night_passage,
        .sponsored_guide = sponsored_night_passage
    };
    return true;
}

