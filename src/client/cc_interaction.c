#include "client/cc_interaction.h"

#include <float.h>
#include <math.h>
#include <stdio.h>

bool CcInteractionKeyEqual(CcInteractionKey a, CcInteractionKey b)
{
    return a.place == b.place && a.object == b.object && a.kind == b.kind;
}

const CcInteractionTarget *CcInteractionFind(const CcInteractionPlan *plan,
                                           CcInteractionKey key)
{
    if (plan == NULL || key.kind == CC_INTERACTION_NONE) return NULL;
    for (int32_t i = 0; i < plan->count && i < CC_INTERACTION_CAPACITY; ++i) {
        if (CcInteractionKeyEqual(plan->targets[i].key, key)) return &plan->targets[i];
    }
    return NULL;
}

const CcInteractionTarget *CcInteractionPick(const CcInteractionPlan *plan,
                                           float x, float y)
{
    if (plan == NULL || !isfinite(x) || !isfinite(y)) return NULL;
    const CcInteractionTarget *picked = NULL;
    float nearest = FLT_MAX;
    for (int32_t i = 0; i < plan->count && i < CC_INTERACTION_CAPACITY; ++i) {
        const CcInteractionTarget *target = &plan->targets[i];
        if (!target->visible || !isfinite(target->camera_distance) ||
            x < target->left || x > target->right ||
            y < target->top || y > target->bottom) continue;
        if (target->camera_distance < nearest) {
            picked = target;
            nearest = target->camera_distance;
        }
    }
    return picked;
}

void CcInteractionCycle(CcInteractionState *state,
                        const CcInteractionPlan *plan, int direction)
{
    if (state == NULL || plan == NULL || plan->count <= 0 ||
        plan->count > CC_INTERACTION_CAPACITY) return;
    int32_t current = direction < 0 ? 0 : -1;
    for (int32_t i = 0; i < plan->count; ++i) {
        if (CcInteractionKeyEqual(state->focus, plan->targets[i].key)) current = i;
    }
    for (int32_t step = 1; step <= plan->count; ++step) {
        int32_t i = (current + (direction < 0 ? -step : step) +
                     plan->count * 2) % plan->count;
        if (plan->targets[i].visible) {
            state->focus = plan->targets[i].key;
            return;
        }
    }
}

bool CcInteractionInRange(const CcInteractionTarget *target, float x, float z)
{
    if (target == NULL || !isfinite(x) || !isfinite(z) ||
        !isfinite(target->approach_x) || !isfinite(target->approach_z) ||
        !isfinite(target->radius) || target->radius <= 0.0f) return false;
    float dx = x - target->approach_x;
    float dz = z - target->approach_z;
    return dx * dx + dz * dz <= target->radius * target->radius;
}

void CcInteractionCancel(CcInteractionState *state, const char *reason)
{
    if (state == NULL) return;
    state->approaching = false;
    state->pending = (CcInteractionKey){0};
    state->repath_seconds = 0.0f;
    state->elapsed_seconds = 0.0f;
    state->stalled_seconds = 0.0f;
    (void)snprintf(state->feedback, sizeof(state->feedback), "%s",
                   reason != NULL ? reason : "");
}

bool CcInteractionStart(CcInteractionState *state,
                        const CcInteractionTarget *target, float x, float z)
{
    if (state == NULL || target == NULL) return false;
    CcInteractionCancel(state, "");
    state->focus = target->key;
    if (!target->available) {
        (void)snprintf(state->feedback, sizeof(state->feedback), "%s", target->reason);
        return false;
    }
    state->pending = target->key;
    state->approaching = true;
    state->previous_x = x;
    state->previous_z = z;
    return true;
}

bool CcInteractionAdvance(CcInteractionState *state,
                          const CcInteractionPlan *plan,
                          float x, float z, float delta_time)
{
    if (state == NULL || !state->approaching) return false;
    const CcInteractionTarget *target = CcInteractionFind(plan, state->pending);
    if (target == NULL) {
        CcInteractionCancel(state, "The target moved out of this scene.");
        return false;
    }
    if (!target->available) {
        CcInteractionCancel(state, target->reason);
        return false;
    }
    if (CcInteractionInRange(target, x, z)) {
        state->focus = target->key;
        CcInteractionCancel(state, "");
        return true;
    }
    float dt = isfinite(delta_time) ? fmaxf(0.0f, fminf(delta_time, 0.1f)) : 0.0f;
    state->repath_seconds = fmaxf(0.0f, state->repath_seconds - dt);
    state->elapsed_seconds += dt;
    float dx = x - state->previous_x;
    float dz = z - state->previous_z;
    if (dx * dx + dz * dz >= 0.01f) {
        state->previous_x = x;
        state->previous_z = z;
        state->stalled_seconds = 0.0f;
    } else {
        state->stalled_seconds += dt;
    }
    if (state->stalled_seconds > 4.0f || state->elapsed_seconds > 45.0f) {
        CcInteractionCancel(state, "The path is blocked. Choose a clear approach.");
    }
    return false;
}
