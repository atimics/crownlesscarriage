#ifndef CROWNLESS_CLIENT_POLICY_H
#define CROWNLESS_CLIENT_POLICY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct CcClientPreferences {
    bool reduced_motion;
    int32_t audio_mode; /* 0: full, 1: effects, 2: muted */
} CcClientPreferences;

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

typedef enum CcClientDeparturePhase {
    CC_CLIENT_DEPARTURE_TOWN = 0,
    CC_CLIENT_DEPARTURE_ROAD_BOOK,
    CC_CLIENT_DEPARTURE_READY
} CcClientDeparturePhase;

typedef struct CcClientDepartureTransition {
    CcClientDeparturePhase phase;
    float town_progress;
    float road_book_progress;
} CcClientDepartureTransition;

typedef enum CcClientArrivalPhase {
    CC_CLIENT_ARRIVAL_ROAD_BOOK = 0,
    CC_CLIENT_ARRIVAL_TOWN,
    CC_CLIENT_ARRIVAL_PARKED
} CcClientArrivalPhase;

typedef struct CcClientArrivalTransition {
    CcClientArrivalPhase phase;
    float road_book_progress;
    float town_progress;
} CcClientArrivalTransition;

float CcClientConvoyPaceStep(float pace, bool road_phase,
                             bool urge, bool rein_in, bool stopped,
                             float delta_time);
float CcClientTravelBlendStep(float blend, bool fast_forward, float delta_time);
float CcClientTravelTimeScale(float blend);
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
void CcClientPreferencesDefault(CcClientPreferences *preferences);
bool CcClientPreferencesLoad(const char *path,
                             CcClientPreferences *preferences,
                             char *error, size_t error_capacity);
bool CcClientPreferencesSave(const char *path,
                             const CcClientPreferences *preferences,
                             char *error, size_t error_capacity);
bool CcClientHitEffectVisible(bool reduced_motion,
                              float hit_flash_seconds);
void CcClientDepartureBegin(CcClientDepartureTransition *transition);
void CcClientDepartureAdvance(CcClientDepartureTransition *transition,
                              float pace, float delta_time);
void CcClientArrivalBegin(CcClientArrivalTransition *transition);
void CcClientArrivalAdvance(CcClientArrivalTransition *transition,
                            float pace, float delta_time);
void CcClientArrivalComplete(CcClientArrivalTransition *transition);
float CcClientArrivalCameraWeight(
    const CcClientArrivalTransition *transition);

#endif
