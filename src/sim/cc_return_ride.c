#include "sim/cc_return_ride.h"

#include "sim/cc_road_position.h"

#include <stdio.h>
#include <string.h>

CcId CcReturnRideTownByName(const CcSim *sim, const char *name)
{
    if (sim == NULL || name == NULL) return 0U;
    for (int32_t i = 0; i < sim->settlement_count; ++i)
        if (strcmp(sim->settlements[i].name, name) == 0) return sim->settlements[i].id;
    return 0U;
}

int32_t CcReturnRideRoadPath(const CcSim *sim, CcId from, CcId to, CcId *path)
{
    if (sim == NULL || path == NULL || from == 0U || to == 0U) return 0;
    CcId previous[CC_MAX_SETTLEMENTS] = {0};
    CcId queue[CC_MAX_SETTLEMENTS];
    bool seen[CC_MAX_SETTLEMENTS] = {false};
    int32_t head = 0, tail = 0;
    queue[tail++] = from;
    for (int32_t i = 0; i < sim->settlement_count; ++i)
        if (sim->settlements[i].id == from) seen[i] = true;
    while (head < tail) {
        CcId at = queue[head++];
        for (int32_t r = 0; r < sim->route_count; ++r) {
            const CcRoute *route = &sim->routes[r];
            CcId next = route->from_id == at ? route->to_id :
                route->to_id == at ? route->from_id : 0U;
            for (int32_t i = 0; next != 0U && i < sim->settlement_count; ++i) {
                if (sim->settlements[i].id != next || seen[i]) continue;
                seen[i] = true;
                previous[i] = at;
                queue[tail++] = next;
            }
        }
    }
    int32_t count = 0;
    CcId reversed[CC_MAX_SETTLEMENTS];
    for (CcId at = to; at != from && count < CC_MAX_SETTLEMENTS;) {
        reversed[count++] = at;
        CcId back = 0U;
        for (int32_t i = 0; i < sim->settlement_count; ++i)
            if (sim->settlements[i].id == at) back = previous[i];
        if (back == 0U) return 0;
        at = back;
    }
    for (int32_t i = 0; i < count; ++i) path[i] = reversed[count - 1 - i];
    return count;
}

static bool RideApply(CcSim *sim, CcCommandKind kind, CcId target,
                      char *error, size_t error_capacity)
{
    CcCommand command = {.kind = kind, .target_id = target};
    return CcSimApply(sim, &command, error, error_capacity);
}

static bool RideContinuePause(CcSim *sim, char *error, size_t error_capacity)
{
    if (sim->journey.phase == CC_JOURNEY_PHASE_RESTING)
        return RideApply(sim, CcSimJourneyStop(sim) == CC_JOURNEY_STOP_MIDDAY ?
                         CC_COMMAND_TAKE_JOURNEY_BREAK : CC_COMMAND_MAKE_CAMP,
                         0U, error, error_capacity);
    if (sim->journey.phase == CC_JOURNEY_PHASE_ROAD_CHOICE) {
        const CcRoadSite *site = CcSimJourneyRoadSiteStop(sim);
        if (site != NULL)
            return RideApply(sim, CC_COMMAND_PASS_ROAD_SITE, site->id,
                             error, error_capacity);
        CcRoadLegPreview previews[3];
        int32_t count = CcRoadNextLegPreviews(sim, previews, 3);
        for (int32_t i = 0; i < count; ++i) {
            if (previews[i].direction == sim->journey.road_direction &&
                previews[i].segment_id != CC_PILOT_ROAD_MILL_SEGMENT_ID)
                return RideApply(sim, CC_COMMAND_CHOOSE_ROAD_LEG,
                                 previews[i].decision_token, error, error_capacity);
        }
        (void)snprintf(error, error_capacity,
                       "no road-choice leg heads toward the destination.");
        return false;
    }
    if (sim->journey.phase == CC_JOURNEY_PHASE_BLOCKED)
        return RideApply(sim, CC_COMMAND_RESOLVE_ENCOUNTER_NEGOTIATE, 0U,
                         error, error_capacity) ||
            RideApply(sim, CC_COMMAND_RESOLVE_ENCOUNTER_COMBAT, 0U,
                     error, error_capacity);
    (void)snprintf(error, error_capacity, "journey is not paused.");
    return false;
}

/* One leg: apply the travel command, then step through the journey a
   second at a time, skipping ambush encounters, until it parks. */
static bool RideLeg(CcSim *sim, CcId destination, char *error,
                    size_t error_capacity)
{
    if (!RideApply(sim, CC_COMMAND_TRAVEL, destination, error, error_capacity))
        return false;
    /* The digest is the subject here, not the road: skip road fights. */
    sim->journey.ambush_pending = false;
    sim->pony_company.encounter = -1;
    for (int32_t step = 0; step < 200000 && sim->journey.active; ++step) {
        if (sim->journey.phase == CC_JOURNEY_PHASE_TRAVELLING) {
            CcSimAdvanceRuntimeTicks(sim, CC_WORLD_TICKS_PER_SECOND);
        } else if (!RideContinuePause(sim, error, error_capacity)) {
            return false;
        }
    }
    if (sim->journey.active || sim->player.location_id != destination) {
        (void)snprintf(error, error_capacity,
                       "the ride did not reach the destination.");
        return false;
    }
    return true;
}

bool CcReturnRideAlongPath(CcSim *sim, CcId destination, char *error,
                          size_t error_capacity)
{
    if (sim == NULL) return false;
    CcId path[CC_MAX_SETTLEMENTS];
    int32_t legs = CcReturnRideRoadPath(sim, sim->player.location_id,
                                        destination, path);
    if (legs == 0) {
        (void)snprintf(error, error_capacity, "no road path to the destination.");
        return false;
    }
    for (int32_t i = 0; i < legs; ++i) {
        if (!RideLeg(sim, path[i], error, error_capacity)) return false;
    }
    return true;
}
