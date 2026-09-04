#include "client/cc_client_policy.h"

#include <math.h>
#include <stddef.h>

static float ClampPace(float pace)
{
    return fmaxf(0.0f, fminf(1.0f, pace));
}

static float ClampUnit(float value)
{
    return fmaxf(0.0f, fminf(1.0f, value));
}

void CcClientDepartureBegin(CcClientDepartureTransition *transition)
{
    if (transition == NULL) return;
    *transition = (CcClientDepartureTransition){
        .phase = CC_CLIENT_DEPARTURE_TOWN,
    };
}

void CcClientDepartureAdvance(CcClientDepartureTransition *transition,
                              float pace, float delta_time)
{
    if (transition == NULL || transition->phase == CC_CLIENT_DEPARTURE_READY) {
        return;
    }
    float step = isfinite(delta_time) ? fmaxf(0.0f, delta_time) : 0.0f;
    if (transition->phase == CC_CLIENT_DEPARTURE_TOWN) {
        transition->town_progress = ClampUnit(
            transition->town_progress + ClampPace(pace) * step * 0.11f);
        if (transition->town_progress >= 1.0f) {
            transition->phase = CC_CLIENT_DEPARTURE_ROAD_BOOK;
            transition->road_book_progress = 0.0f;
        }
        return;
    }
    transition->road_book_progress = ClampUnit(
        transition->road_book_progress + step * 1.35f);
    if (transition->road_book_progress >= 1.0f) {
        transition->phase = CC_CLIENT_DEPARTURE_READY;
    }
}

float CcClientConvoyPaceStep(float pace, bool road_phase,
                             bool urge, bool rein_in, bool stopped,
                             float delta_time)
{
    float step = fmaxf(0.0f, delta_time);
    if (urge) pace += step * 0.72f;
    if (rein_in) pace -= step * 1.08f;
    if (!urge && !rein_in && !stopped) {
        float cruise = road_phase ? 0.72f : 1.0f;
        float correction = cruise - pace;
        float maximum_change = step * 0.55f;
        correction = fmaxf(-maximum_change,
                           fminf(maximum_change, correction));
        pace += correction;
    }
    if (stopped) pace = 0.0f;
    return ClampPace(pace);
}

float CcClientRoadApproachStep(float progress, float pace, float delta_time)
{
    float step = fmaxf(0.0f, delta_time);
    float next = progress + ClampPace(pace) * step * 0.24f;
    return fmaxf(0.0f, fminf(1.0f, next));
}

float CcClientConvoyPosturePace(int32_t posture)
{
    if (posture <= 0) return 0.52f;
    if (posture >= 2) return 1.0f;
    return 0.72f;
}

int32_t CcClientStepConvoyPosture(int32_t posture, int32_t direction)
{
    int32_t next = posture + (direction > 0 ? 1 : direction < 0 ? -1 : 0);
    if (next < 0) next = 0;
    if (next > 2) next = 2;
    return next;
}

CcClientConvoyGait CcClientConvoyGaitForPace(float pace)
{
    if (!isfinite(pace)) return CC_CLIENT_CONVOY_GAIT_HALT;
    pace = ClampPace(pace);
    if (pace <= 0.02f) return CC_CLIENT_CONVOY_GAIT_HALT;
    if (pace < 0.62f) return CC_CLIENT_CONVOY_GAIT_WALK;
    if (pace < 0.86f) return CC_CLIENT_CONVOY_GAIT_TROT;
    return CC_CLIENT_CONVOY_GAIT_CANTER;
}

float CcClientConvoyGaitCadence(CcClientConvoyGait gait)
{
    switch (gait) {
        case CC_CLIENT_CONVOY_GAIT_HALT: return 0.0f;
        case CC_CLIENT_CONVOY_GAIT_WALK: return 3.6f;
        case CC_CLIENT_CONVOY_GAIT_TROT: return 4.8f;
        case CC_CLIENT_CONVOY_GAIT_CANTER: return 6.0f;
        case CC_CLIENT_CONVOY_GAIT_COUNT:
        default:
            return 0.0f;
    }
}

const char *CcClientConvoyGaitName(CcClientConvoyGait gait)
{
    switch (gait) {
        case CC_CLIENT_CONVOY_GAIT_HALT: return "HALT";
        case CC_CLIENT_CONVOY_GAIT_WALK: return "WALK";
        case CC_CLIENT_CONVOY_GAIT_TROT: return "TROT";
        case CC_CLIENT_CONVOY_GAIT_CANTER: return "CANTER";
        case CC_CLIENT_CONVOY_GAIT_COUNT:
        default:
            return "UNKNOWN";
    }
}

bool CcClientRoadHasNextBranch(int32_t branch_ordinal,
                               int32_t branch_count)
{
    return branch_ordinal >= 0 && branch_ordinal + 1 < branch_count;
}

bool CcClientMapCommandEnabled(bool viewing_map, bool road_local,
                               bool market_interior,
                               float carriage_distance)
{
    return viewing_map ||
           (!road_local && !market_interior && carriage_distance < 1.35f);
}

bool CcClientPromiseCanBeAccepted(bool market_interior,
                                  float notice_distance)
{
    return !market_interior && notice_distance < 1.15f;
}

bool CcClientInteractionActivated(bool requested, float distance,
                                  float maximum_distance)
{
    return requested && isfinite(distance) && isfinite(maximum_distance) &&
           maximum_distance >= 0.0f && distance < maximum_distance;
}

CcClientCampaignAccess CcClientCampaignAccessFor(bool normal_play,
                                                 bool journal_available)
{
    if (!normal_play) return CC_CLIENT_CAMPAIGN_EPHEMERAL;
    return journal_available ? CC_CLIENT_CAMPAIGN_PERSISTENT :
                               CC_CLIENT_CAMPAIGN_BLOCKED;
}
