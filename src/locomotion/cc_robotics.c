#include "locomotion/cc_robotics.h"

#include <math.h>
#include <stddef.h>

static float Clamp(float value, float minimum, float maximum)
{
    return fmaxf(minimum, fminf(value, maximum));
}

static float PlanarLength(CcLimbVec3 value)
{
    return sqrtf(value.x * value.x + value.z * value.z);
}

int32_t CcRobotSampleLink(CcLimbVec3 start, CcLimbVec3 end, float radius,
                          CcRobotCollisionPoint *points, int32_t capacity)
{
    if (points == NULL || capacity <= 0 || radius <= 0.0f) return 0;
    float x = end.x - start.x;
    float y = end.y - start.y;
    float z = end.z - start.z;
    float length = sqrtf(x * x + y * y + z * z);
    int32_t interval_count = (int32_t)ceilf(length / (radius * 1.80f));
    if (interval_count < 1) interval_count = 1;
    int32_t point_count = interval_count + 1;
    if (point_count > capacity) return 0;
    for (int32_t sample = 0; sample < point_count; ++sample) {
        float amount = point_count > 1 ?
            (float)sample / (float)(point_count - 1) : 0.0f;
        points[sample] = (CcRobotCollisionPoint){
            .center = {
                start.x + x * amount,
                start.y + y * amount,
                start.z + z * amount,
            },
            .radius = radius,
        };
    }
    return point_count;
}

int32_t CcRobotLimbPointSpace(const CcLimbRig *rig, float radius,
                              CcRobotCollisionPoint *points,
                              int32_t capacity)
{
    if (rig == NULL || !rig->initialized || points == NULL ||
        capacity <= 0 || radius <= 0.0f) {
        return 0;
    }
    int32_t count = 0;
    for (int32_t limb = 0; limb < rig->morphology.limb_count; ++limb) {
        const CcLimbRuntime *runtime = &rig->limbs[limb];
        const CcLimbSpec *spec = &rig->morphology.limbs[limb];
        if (runtime->health <= 0.0f ||
            runtime->state == CC_LIMB_DISABLED) {
            continue;
        }
        for (int32_t segment = 0; segment < spec->segment_count; ++segment) {
            if (count >= capacity) return count;
            int32_t segment_count = CcRobotSampleLink(
                runtime->joints[segment], runtime->joints[segment + 1],
                radius, &points[count], capacity - count);
            if (segment_count <= 0) return count;
            count += segment_count;
        }
    }
    return count;
}

bool CcRobotPredictiveAvoidance(
    CcLimbVec3 first_position, CcLimbVec3 first_velocity,
    CcLimbVec3 second_position, CcLimbVec3 second_velocity,
    float minimum_clearance, float horizon, int32_t stable_pair_index,
    CcLimbVec3 *first_correction, CcLimbVec3 *second_correction)
{
    if (first_correction == NULL || second_correction == NULL ||
        minimum_clearance <= 0.0f || horizon <= 0.0f) {
        return false;
    }
    *first_correction = (CcLimbVec3){0};
    *second_correction = (CcLimbVec3){0};

    CcLimbVec3 position = {
        first_position.x - second_position.x,
        0.0f,
        first_position.z - second_position.z,
    };
    CcLimbVec3 velocity = {
        first_velocity.x - second_velocity.x,
        0.0f,
        first_velocity.z - second_velocity.z,
    };
    float current_distance = PlanarLength(position);
    float velocity_squared = velocity.x * velocity.x +
                             velocity.z * velocity.z;
    float closest_time = 0.0f;
    if (current_distance >= minimum_clearance) {
        if (velocity_squared <= 0.000001f) return false;
        float closing = position.x * velocity.x + position.z * velocity.z;
        if (closing >= 0.0f) return false;
        closest_time = Clamp(-closing / velocity_squared, 0.0f, horizon);
    }
    CcLimbVec3 closest = {
        position.x + velocity.x * closest_time,
        0.0f,
        position.z + velocity.z * closest_time,
    };
    float closest_distance = PlanarLength(closest);
    if (closest_distance >= minimum_clearance) return false;

    CcLimbVec3 normal = closest;
    float normal_length = closest_distance;
    if (current_distance < minimum_clearance && current_distance > 0.0001f) {
        normal = position;
        normal_length = current_distance;
    }
    if (normal_length > 0.0001f) {
        normal.x /= normal_length;
        normal.z /= normal_length;
    } else if (velocity_squared > 0.000001f) {
        float velocity_length = sqrtf(velocity_squared);
        normal.x = velocity.z / velocity_length;
        normal.z = -velocity.x / velocity_length;
        if ((stable_pair_index & 1) != 0) {
            normal.x = -normal.x;
            normal.z = -normal.z;
        }
    } else {
        static const CcLimbVec3 directions[4] = {
            {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f},
            {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f},
        };
        normal = directions[stable_pair_index & 3];
    }

    float depth = minimum_clearance - closest_distance;
    float warning = 1.0f - closest_time / horizon;
    float relative_speed = fminf(
        0.92f, 0.12f + depth * 1.9f + warning * 0.22f);
    float shared_speed = relative_speed * 0.5f;
    *first_correction = (CcLimbVec3){
        normal.x * shared_speed, 0.0f, normal.z * shared_speed};
    *second_correction = (CcLimbVec3){
        -normal.x * shared_speed, 0.0f, -normal.z * shared_speed};
    return true;
}

float CcRobotTraversabilityCost(float distance, float height_change,
                                float support_normal_y)
{
    if (distance <= 0.0f) return 0.0f;
    float grade = fabsf(height_change) / fmaxf(distance, 0.0001f);
    float surface_tilt = 1.0f - Clamp(support_normal_y, 0.0f, 1.0f);
    float multiplier = 1.0f + fminf(grade, 2.0f) * 0.48f +
                       surface_tilt * 2.40f;
    return distance * multiplier;
}
