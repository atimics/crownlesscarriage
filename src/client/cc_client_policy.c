#include "client/cc_client_policy.h"

#include <math.h>

static float ClampPace(float pace)
{
    return fmaxf(0.0f, fminf(1.0f, pace));
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

CcClientClickIntent CcClientClickIntentForDistances(
    bool market_interior, float interior_exit_distance,
    float notice_distance, float carriage_distance,
    float dragon_cave_distance, float market_distance)
{
    if (market_interior) {
        return interior_exit_distance < 1.25f ?
            CC_CLIENT_CLICK_LEAVE_INTERIOR : CC_CLIENT_CLICK_NONE;
    }
    if (notice_distance < 1.15f) {
        return CC_CLIENT_CLICK_OPEN_PROMISES;
    }
    if (carriage_distance < 1.35f) {
        return CC_CLIENT_CLICK_DRIVE_OUT;
    }
    if (dragon_cave_distance < 1.35f) {
        return CC_CLIENT_CLICK_OPEN_DRAGON_CAVE;
    }
    if (market_distance < 1.30f) {
        return CC_CLIENT_CLICK_ENTER_INTERIOR;
    }
    return CC_CLIENT_CLICK_NONE;
}
