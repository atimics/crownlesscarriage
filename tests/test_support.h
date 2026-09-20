#ifndef CROWNLESS_TEST_SUPPORT_H
#define CROWNLESS_TEST_SUPPORT_H

#include <stdio.h>
#include <stdlib.h>

#include "sim/cc_road_position.h"


#define CC_CHECK(expression)                                                   \
    do {                                                                       \
        if (!(expression)) {                                                   \
            (void)fprintf(stderr, "%s:%d: check failed: %s\n",               \
                          __FILE__, __LINE__, #expression);                    \
            exit(EXIT_FAILURE);                                                \
        }                                                                      \
    } while (0)

static inline bool CcTestContinueJourneyPause(
    CcSim *sim, char *error, size_t error_capacity)
{
    if (sim == NULL) return false;
    if (sim->journey.phase == CC_JOURNEY_PHASE_RESTING) {
        CcCommand resume = {
            .kind = CcSimJourneyStop(sim) == CC_JOURNEY_STOP_MIDDAY ?
                CC_COMMAND_TAKE_JOURNEY_BREAK : CC_COMMAND_MAKE_CAMP
        };
        return CcSimApply(sim, &resume, error, error_capacity);
    }
    if (sim->journey.phase != CC_JOURNEY_PHASE_ROAD_CHOICE) return false;
    const CcRoadSite *site = CcSimJourneyRoadSiteStop(sim);
    if (site != NULL) {
        CcCommand pass = {
            .kind = CC_COMMAND_PASS_ROAD_SITE,
            .target_id = site->id
        };
        return CcSimApply(sim, &pass, error, error_capacity);
    }
    CcRoadLegPreview previews[3];
    int32_t count = CcRoadNextLegPreviews(sim, previews, 3);
    for (int32_t i = 0; i < count; ++i) {
        if (previews[i].direction == sim->journey.road_direction &&
            previews[i].segment_id != CC_PILOT_ROAD_MILL_SEGMENT_ID) {
            CcCommand choose = {
                .kind = CC_COMMAND_CHOOSE_ROAD_LEG,
                .target_id = previews[i].decision_token
            };
            return CcSimApply(sim, &choose, error, error_capacity);
        }
    }
    return false;
}

static inline bool CcTestLoadReliefCrates(
    CcSim *sim, CcSituation *situation, char *error, size_t error_capacity)
{
    if (sim == NULL || situation == NULL) return false;
    while (CcSimReliefCratesToLoad(situation) > 0) {
        CcCommand pickup = {
            .kind = CC_COMMAND_PICKUP_RELIEF_CRATE,
            .target_id = situation->id
        };
        if (!CcSimApply(sim, &pickup, error, error_capacity)) return false;
        CcCommand stow = {
            .kind = CC_COMMAND_STOW_RELIEF_CRATE,
            .target_id = situation->id
        };
        if (!CcSimApply(sim, &stow, error, error_capacity)) return false;
    }
    return true;
}

#endif
