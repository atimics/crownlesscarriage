#ifndef CROWNLESS_CENSUS_LAYOUT_H
#define CROWNLESS_CENSUS_LAYOUT_H

#include "sim/cc_sim.h"

/* Local positions and route lengths use metres from the settlement centre. */
typedef struct CcCensusPoint {
    int32_t east_m;
    int32_t north_m;
} CcCensusPoint;

typedef struct CcCensusRoad {
    CcId from_district_id;
    CcId to_district_id;
    CcCensusPoint from;
    CcCensusPoint corner;
    CcCensusPoint to;
    int32_t length_m;
} CcCensusRoad;

#define CC_CENSUS_ROADS_PER_TOWN 6

bool CcCensusDistrictCentre(const CcSim *sim, CcId district_id,
                            CcCensusPoint *point);
bool CcCensusHomeEntrance(const CcSim *sim, CcId resident_id,
                          CcCensusPoint *point);
bool CcCensusDwellingEntrance(const CcSim *sim, CcId district_id,
                              int32_t dwelling_slot, CcCensusPoint *point);
bool CcCensusRoadAt(const CcSim *sim, CcId settlement_id,
                    int32_t road_index, CcCensusRoad *road);
int32_t CcCensusDistrictPathMeters(const CcSim *sim,
                                   CcId from_district_id,
                                   CcId to_district_id);

#endif
