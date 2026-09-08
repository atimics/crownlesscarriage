#ifndef CROWNLESS_JOURNEY_INTERNAL_H
#define CROWNLESS_JOURNEY_INTERNAL_H

#include "sim/cc_sim.h"

/* Shared by journey execution, validation, and road-house generation. */
int32_t CcJourneyPaceRate(CcJourneyPace pace);
int32_t CcJourneyCarriageSpeedForPace(int32_t total_subticks, CcJourneyPace pace);
uint32_t CcJourneyRoadHouseSeed(const CcSim *sim, CcId route_id);
const char *CcJourneyGeneratedRoadHouseName(const CcSim *sim, CcId route_id);

#endif
