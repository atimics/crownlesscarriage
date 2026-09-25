#include "persistence/cc_save.h"
#include "sim/cc_census.h"
#include "sim/cc_census_layout.h"
#include "test_support.h"

#include <stdlib.h>

static CcSim sim, restored;

static bool SamePoint(CcCensusPoint a, CcCensusPoint b)
{
    return a.east_m == b.east_m && a.north_m == b.north_m;
}

int main(void)
{
    CcSimInit(&sim, UINT32_C(0x3235a7ed));
    CcId thornford = sim.settlements[0].id;
    CcId centre = sim.census.districts[0].id;
    CcCensusPoint town_centre;
    CC_CHECK(CcCensusDistrictCentre(&sim, centre, &town_centre));
    CC_CHECK(SamePoint(town_centre, (CcCensusPoint){0, 0}));
    CcCensusPoint entrances[368];
    int32_t homes = 0;
    for (int32_t district = 0; district < 6; ++district) {
        const CcCensusDistrict *place = &sim.census.districts[district];
        for (int32_t dwelling = 0; dwelling < place->dwelling_count;
             ++dwelling) {
            CcCensusPoint entrance;
            CC_CHECK(CcCensusDwellingEntrance(&sim, place->id, dwelling,
                                               &entrance));
            for (int32_t previous = 0; previous < homes; ++previous)
                CC_CHECK(!SamePoint(entrance, entrances[previous]));
            entrances[homes++] = entrance;
        }
    }
    CC_CHECK(homes == 368);
    for (int32_t road = 0; road < CC_CENSUS_ROADS_PER_TOWN; ++road) {
        CcCensusRoad link;
        CC_CHECK(CcCensusRoadAt(&sim, thornford, road, &link));
        CC_CHECK(link.length_m ==
            abs(link.corner.east_m - link.from.east_m) +
            abs(link.corner.north_m - link.from.north_m) +
            abs(link.to.east_m - link.corner.east_m) +
            abs(link.to.north_m - link.corner.north_m));
        CC_CHECK(CcCensusDistrictPathMeters(
            &sim, link.from_district_id, link.to_district_id) <=
            link.length_m);
    }
    CC_CHECK(CcCensusDistrictPathMeters(&sim, centre,
        sim.census.districts[5].id) == 1260);
    CC_CHECK(CcCensusDistrictPathMeters(&sim, centre,
        sim.census.districts[6].id) == -1);
    for (int32_t i = 0; i < sim.census.resident_count; ++i) {
        const CcCensusResident *person = &sim.census.residents[i];
        if (person->district_slot >= 6) continue;
        CcCensusPoint entrance, addressed;
        CC_CHECK(CcCensusHomeEntrance(&sim, person->id, &entrance));
        CC_CHECK(CcCensusDwellingEntrance(&sim,
            sim.census.districts[person->district_slot].id,
            person->dwelling_slot, &addressed));
        CC_CHECK(SamePoint(entrance, addressed));
    }
    CcId known = sim.census.residents[0].id;
    CcCensusPoint before;
    CC_CHECK(CcCensusHomeEntrance(&sim, known, &before));
    const char *path = "census-layout.ccsave";
    char error[256];
    CC_CHECK(CcSaveWrite(path, &sim, error, sizeof(error)));
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    CcCensusPoint after;
    CC_CHECK(CcCensusHomeEntrance(&restored, known, &after));
    CC_CHECK(SamePoint(before, after));
    restored.settlements[0].population += 1;
    CcCensusReconcile(&restored);
    CC_CHECK(CcCensusHomeEntrance(&restored, known, &after));
    CC_CHECK(SamePoint(before, after));
    (void)remove(path);
    (void)remove("census-layout.ccsave-wal");
    (void)remove("census-layout.ccsave-shm");
    return 0;
}
