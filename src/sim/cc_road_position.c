#include "sim/cc_road_position.h"

#include <limits.h>
#include <string.h>

static const CcSettlement *SettlementNamed(const CcSim *sim,
                                            const char *name)
{
    if (sim == NULL || name == NULL) return NULL;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        if (strcmp(sim->settlements[i].name, name) == 0)
            return &sim->settlements[i];
    }
    return NULL;
}

static const CcRoadSite *RoadSiteNamed(const CcSim *sim,
                                       const char *name)
{
    if (sim == NULL || name == NULL) return NULL;
    for (int32_t i = 0; i < sim->road_site_count; ++i) {
        if (strcmp(sim->road_sites[i].name, name) == 0)
            return &sim->road_sites[i];
    }
    return NULL;
}

int32_t CcRoadScaleDistance(int32_t total_units, int32_t progress_milli)
{
    if (total_units <= 0 || progress_milli <= 0) return 0;
    if (progress_milli >= 1000) return total_units;
    return (int32_t)(((int64_t)total_units * progress_milli + 500) / 1000);
}

int32_t CcRoadProgressMilli(int32_t travelled_units, int32_t total_units)
{
    if (total_units <= 0 || travelled_units <= 0) return 0;
    if (travelled_units >= total_units) return 1000;
    return (int32_t)(((int64_t)travelled_units * 1000 +
                      total_units / 2) / total_units);
}

int32_t CcRoadTravelSubticks(int32_t leg_units,
                             int32_t route_total_subticks,
                             int32_t route_length_units)
{
    if (leg_units <= 0 || route_total_subticks <= 0 ||
        route_length_units <= 0) return 0;
    int64_t numerator = (int64_t)leg_units * route_total_subticks;
    int64_t rounded = (numerator + route_length_units / 2) /
        route_length_units;
    return rounded > INT32_MAX ? INT32_MAX : (int32_t)rounded;
}

static uint64_t TokenWord(uint64_t hash, uint64_t word)
{
    for (int32_t byte = 0; byte < 8; ++byte) {
        hash ^= word & UINT64_C(0xff);
        hash *= UINT64_C(1099511628211);
        word >>= 8U;
    }
    return hash;
}

uint64_t CcRoadPreviewToken(CcId journey_goal_id, CcId anchor_id,
                            uint32_t journey_revision, CcId segment_id,
                            CcRoadDirection direction)
{
    uint64_t token = UINT64_C(1469598103934665603);
    token = TokenWord(token, journey_goal_id);
    token = TokenWord(token, anchor_id);
    token = TokenWord(token, journey_revision);
    token = TokenWord(token, segment_id);
    token = TokenWord(token, (uint64_t)(int64_t)direction);
    return token == 0U ? UINT64_C(1) : token;
}

bool CcPilotRoadTopologyBuild(const CcSim *sim,
                              CcPilotRoadTopology *topology)
{
    if (sim == NULL || topology == NULL) return false;
    const CcRoadSite *mill = RoadSiteNamed(sim, "Stag's Mill");
    const CcSettlement *thornford = SettlementNamed(sim, "Thornford");
    const CcSettlement *gloamgate = SettlementNamed(sim, "Gloamgate");
    if (mill == NULL || mill->kind != CC_ROAD_SITE_MILL ||
        thornford == NULL || gloamgate == NULL) return false;
    const CcRoute *route = CcSimRoute(sim, mill->route_id);
    if (route == NULL || route->from_id != thornford->id ||
        route->to_id != gloamgate->id ||
        mill->progress_milli <= 0 || mill->progress_milli >= 1000 ||
        mill->spur_length <= 0) return false;

    int32_t origin_to_junction = CcRoadScaleDistance(
        CC_PILOT_ROAD_MAIN_LENGTH_UNITS, mill->progress_milli);
    *topology = (CcPilotRoadTopology){
        .route_id = route->id,
        .origin_id = thornford->id,
        .destination_id = gloamgate->id,
        .mill_site_id = mill->id,
        .junction_id = CC_PILOT_ROAD_JUNCTION_ID,
        .checkpoint_id = CC_PILOT_ROAD_CHECKPOINT_ID,
        .origin_segment_id = CC_PILOT_ROAD_ORIGIN_SEGMENT_ID,
        .destination_segment_id = CC_PILOT_ROAD_DESTINATION_SEGMENT_ID,
        .mill_segment_id = CC_PILOT_ROAD_MILL_SEGMENT_ID,
        .main_length_units = CC_PILOT_ROAD_MAIN_LENGTH_UNITS,
        .origin_to_junction_units = origin_to_junction,
        .junction_to_destination_units =
            CC_PILOT_ROAD_MAIN_LENGTH_UNITS - origin_to_junction,
        .mill_spur_length_units =
            mill->spur_length * CC_PILOT_ROAD_UNITS_PER_SPUR_UNIT,
        .checkpoint_distance_units = CcRoadScaleDistance(
            CC_PILOT_ROAD_MAIN_LENGTH_UNITS, 500),
        .junction_progress_milli = mill->progress_milli,
        .checkpoint_progress_milli = 500
    };
    return true;
}
