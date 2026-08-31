#ifndef CROWNLESS_CLIENT_POLICY_H
#define CROWNLESS_CLIENT_POLICY_H

#include <stdbool.h>
#include <stdint.h>

typedef enum CcClientConvoyGait {
    CC_CLIENT_CONVOY_GAIT_HALT,
    CC_CLIENT_CONVOY_GAIT_WALK,
    CC_CLIENT_CONVOY_GAIT_TROT,
    CC_CLIENT_CONVOY_GAIT_CANTER,
    CC_CLIENT_CONVOY_GAIT_COUNT
} CcClientConvoyGait;

typedef enum CcClientCampaignAccess {
    CC_CLIENT_CAMPAIGN_EPHEMERAL,
    CC_CLIENT_CAMPAIGN_PERSISTENT,
    CC_CLIENT_CAMPAIGN_BLOCKED
} CcClientCampaignAccess;

float CcClientConvoyPaceStep(float pace, bool road_phase,
                             bool urge, bool rein_in, bool stopped,
                             float delta_time);
float CcClientRoadApproachStep(float progress, float pace, float delta_time);
float CcClientConvoyPosturePace(int32_t posture);
int32_t CcClientStepConvoyPosture(int32_t posture, int32_t direction);
CcClientConvoyGait CcClientConvoyGaitForPace(float pace);
float CcClientConvoyGaitCadence(CcClientConvoyGait gait);
const char *CcClientConvoyGaitName(CcClientConvoyGait gait);
bool CcClientRoadHasNextBranch(int32_t branch_ordinal,
                               int32_t branch_count);
bool CcClientMapCommandEnabled(bool viewing_map, bool road_local,
                               bool market_interior,
                               float carriage_distance);
bool CcClientPromiseCanBeAccepted(bool market_interior,
                                  float notice_distance);
bool CcClientInteractionActivated(bool requested, float distance,
                                  float maximum_distance);
CcClientCampaignAccess CcClientCampaignAccessFor(bool normal_play,
                                                 bool journal_available);

#endif
