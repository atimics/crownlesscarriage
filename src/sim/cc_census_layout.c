#include "sim/cc_census_layout.h"
#include "sim/cc_census.h"

#include <limits.h>
#include <stdlib.h>

/* The first patch is the authored centre. Other patches are reached by local
   lanes and can be streamed around these fixed anchors. */
static const CcCensusPoint DISTRICT_CENTRES[CC_CENSUS_DISTRICTS_PER_TOWN] = {
    {0, 0}, {420, 0}, {0, 460}, {-520, 0}, {420, -520}, {-920, 340}
};

static const int32_t ROAD_ENDPOINTS[CC_CENSUS_ROADS_PER_TOWN][2] = {
    {0, 1}, {0, 2}, {0, 3}, {1, 4}, {3, 5}, {2, 5}
};

static int32_t DistrictSlot(const CcSim *sim, CcId district_id)
{
    if (sim == NULL || sim->schema_version < 114U) return -1;
    for (int32_t i = 0; i < sim->census.district_count; ++i)
        if (sim->census.districts[i].id == district_id) return i;
    return -1;
}

bool CcCensusDistrictCentre(const CcSim *sim, CcId district_id,
                            CcCensusPoint *point)
{
    int32_t slot = DistrictSlot(sim, district_id);
    if (slot < 0 || point == NULL) return false;
    *point = DISTRICT_CENTRES[slot % CC_CENSUS_DISTRICTS_PER_TOWN];
    return true;
}

bool CcCensusDwellingEntrance(const CcSim *sim, CcId district_id,
                              int32_t dwelling_slot, CcCensusPoint *point)
{
    int32_t district_slot = DistrictSlot(sim, district_id);
    if (point == NULL || district_slot < 0 || dwelling_slot < 0 ||
        dwelling_slot >= sim->census.districts[district_slot].dwelling_count)
        return false;
    CcCensusPoint centre = DISTRICT_CENTRES[
        district_slot % CC_CENSUS_DISTRICTS_PER_TOWN];
    if (district_slot % CC_CENSUS_DISTRICTS_PER_TOWN == 0) {
        point->east_m = centre.east_m + (dwelling_slot % 4) * 18 - 27;
        point->north_m = centre.north_m + (dwelling_slot / 4) * 18 - 27;
    } else {
        point->east_m = centre.east_m + (dwelling_slot % 12) * 18 - 99;
        point->north_m = centre.north_m + 48 + (dwelling_slot / 12) * 18;
    }
    return true;
}

bool CcCensusHomeEntrance(const CcSim *sim, CcId resident_id,
                          CcCensusPoint *point)
{
    if (sim == NULL) return false;
    const CcCensusResident *person = CcCensusResidentById(sim, resident_id);
    if (person == NULL || person->left_day != 0 ||
        person->district_slot < 0 ||
        person->district_slot >= sim->census.district_count) return false;
    return CcCensusDwellingEntrance(sim,
        sim->census.districts[person->district_slot].id,
        person->dwelling_slot, point);
}

bool CcCensusRoadAt(const CcSim *sim, CcId settlement_id,
                    int32_t road_index, CcCensusRoad *road)
{
    if (sim == NULL || sim->schema_version < 114U || road == NULL ||
        road_index < 0 || road_index >= CC_CENSUS_ROADS_PER_TOWN) return false;
    int32_t first = -1;
    for (int32_t i = 0; i < sim->census.district_count;
         i += CC_CENSUS_DISTRICTS_PER_TOWN)
        if (sim->census.districts[i].settlement_id == settlement_id) {
            first = i;
            break;
        }
    if (first < 0 || first + CC_CENSUS_DISTRICTS_PER_TOWN >
        sim->census.district_count) return false;
    int32_t a = ROAD_ENDPOINTS[road_index][0];
    int32_t b = ROAD_ENDPOINTS[road_index][1];
    CcCensusPoint from = DISTRICT_CENTRES[a];
    CcCensusPoint to = DISTRICT_CENTRES[b];
    *road = (CcCensusRoad){
        .from_district_id = sim->census.districts[first + a].id,
        .to_district_id = sim->census.districts[first + b].id,
        .from = from,
        .corner = {to.east_m, from.north_m},
        .to = to,
        .length_m = abs(to.east_m - from.east_m) +
                    abs(to.north_m - from.north_m)
    };
    return true;
}

int32_t CcCensusDistrictPathMeters(const CcSim *sim,
                                   CcId from_district_id,
                                   CcId to_district_id)
{
    int32_t start = DistrictSlot(sim, from_district_id);
    int32_t goal = DistrictSlot(sim, to_district_id);
    if (start < 0 || goal < 0 ||
        start / CC_CENSUS_DISTRICTS_PER_TOWN !=
            goal / CC_CENSUS_DISTRICTS_PER_TOWN) return -1;
    int32_t distances[CC_CENSUS_DISTRICTS_PER_TOWN];
    bool visited[CC_CENSUS_DISTRICTS_PER_TOWN] = {false};
    for (int32_t i = 0; i < CC_CENSUS_DISTRICTS_PER_TOWN; ++i)
        distances[i] = INT_MAX;
    distances[start % CC_CENSUS_DISTRICTS_PER_TOWN] = 0;
    CcId settlement_id = sim->census.districts[start].settlement_id;
    for (int32_t step = 0; step < CC_CENSUS_DISTRICTS_PER_TOWN; ++step) {
        int32_t next = -1;
        for (int32_t i = 0; i < CC_CENSUS_DISTRICTS_PER_TOWN; ++i)
            if (!visited[i] && (next < 0 || distances[i] < distances[next]))
                next = i;
        if (next < 0 || distances[next] == INT_MAX) break;
        visited[next] = true;
        for (int32_t i = 0; i < CC_CENSUS_ROADS_PER_TOWN; ++i) {
            CcCensusRoad road;
            if (!CcCensusRoadAt(sim, settlement_id, i, &road)) return -1;
            int32_t a = ROAD_ENDPOINTS[i][0], b = ROAD_ENDPOINTS[i][1];
            int32_t other = a == next ? b : b == next ? a : -1;
            if (other >= 0 && !visited[other] &&
                distances[next] + road.length_m < distances[other])
                distances[other] = distances[next] + road.length_m;
        }
    }
    return distances[goal % CC_CENSUS_DISTRICTS_PER_TOWN] == INT_MAX ?
        -1 : distances[goal % CC_CENSUS_DISTRICTS_PER_TOWN];
}
