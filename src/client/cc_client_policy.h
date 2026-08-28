#ifndef CROWNLESS_CLIENT_POLICY_H
#define CROWNLESS_CLIENT_POLICY_H

#include <stdbool.h>
#include <stdint.h>

typedef enum CcClientClickIntent {
    CC_CLIENT_CLICK_NONE = 0,
    CC_CLIENT_CLICK_LEAVE_INTERIOR,
    CC_CLIENT_CLICK_OPEN_PROMISES,
    CC_CLIENT_CLICK_DRIVE_OUT,
    CC_CLIENT_CLICK_OPEN_DRAGON_CAVE,
    CC_CLIENT_CLICK_ENTER_INTERIOR
} CcClientClickIntent;

float CcClientConvoyPaceStep(float pace, bool road_phase,
                             bool urge, bool rein_in, bool stopped,
                             float delta_time);
float CcClientRoadApproachStep(float progress, float pace, float delta_time);
bool CcClientRoadHasNextBranch(int32_t branch_ordinal,
                               int32_t branch_count);
bool CcClientMapCommandEnabled(bool viewing_map, bool road_local,
                               bool market_interior,
                               float carriage_distance);
bool CcClientPromiseCanBeAccepted(bool market_interior,
                                  float notice_distance);
CcClientClickIntent CcClientClickIntentForDistances(
    bool market_interior, float interior_exit_distance,
    float notice_distance, float carriage_distance,
    float dragon_cave_distance, float market_distance);

#endif
