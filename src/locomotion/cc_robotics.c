#include "locomotion/cc_robotics.h"

#include <math.h>
#include <stddef.h>

#define CC_ROBOT_CLIMB_SEARCH_CAPACITY 512

static float Clamp(float value, float minimum, float maximum)
{
    return fmaxf(minimum, fminf(value, maximum));
}

static float PlanarLength(CcLimbVec3 value)
{
    return sqrtf(value.x * value.x + value.z * value.z);
}

static CcLimbVec3 Add(CcLimbVec3 first, CcLimbVec3 second)
{
    return (CcLimbVec3){first.x + second.x, first.y + second.y,
                        first.z + second.z};
}

static CcLimbVec3 Subtract(CcLimbVec3 first, CcLimbVec3 second)
{
    return (CcLimbVec3){first.x - second.x, first.y - second.y,
                        first.z - second.z};
}

static CcLimbVec3 Scale(CcLimbVec3 value, float scale)
{
    return (CcLimbVec3){value.x * scale, value.y * scale, value.z * scale};
}

static float Dot(CcLimbVec3 first, CcLimbVec3 second)
{
    return first.x * second.x + first.y * second.y + first.z * second.z;
}

static CcLimbVec3 Cross(CcLimbVec3 first, CcLimbVec3 second)
{
    return (CcLimbVec3){
        first.y * second.z - first.z * second.y,
        first.z * second.x - first.x * second.z,
        first.x * second.y - first.y * second.x,
    };
}

static float Length(CcLimbVec3 value)
{
    return sqrtf(Dot(value, value));
}

static CcLimbVec3 Normalize(CcLimbVec3 value, CcLimbVec3 fallback)
{
    float length = Length(value);
    return length > 0.0001f ? Scale(value, 1.0f / length) : fallback;
}

static CcRobotSurfaceKind SurfaceKind(CcLimbVec3 normal)
{
    if (normal.y > 0.55f) return CC_ROBOT_SURFACE_FLOOR;
    if (normal.y < -0.55f) return CC_ROBOT_SURFACE_CEILING;
    return CC_ROBOT_SURFACE_WALL;
}

typedef struct ClimbSearchNode {
    CcLimbVec3 point;
    CcLimbVec3 normal;
    float travelled;
    float heuristic;
    int32_t parent;
    bool open;
    bool closed;
} ClimbSearchNode;

static bool ClimbPointMatches(const ClimbSearchNode *node,
                              CcLimbVec3 point, CcLimbVec3 normal,
                              float tolerance)
{
    return Length(Subtract(node->point, point)) <= tolerance &&
           Dot(node->normal, normal) >= 0.72f;
}

static bool AppendClimbRoute(const ClimbSearchNode *search,
                             int32_t final_index,
                             CcLimbVec3 goal_point,
                             CcLimbVec3 goal_normal,
                             CcRobotClimbRoute *route)
{
    int32_t reverse[CC_ROBOT_CLIMB_ROUTE_CAPACITY];
    int32_t count = 0;
    for (int32_t index = final_index; index >= 0;
         index = search[index].parent) {
        if (count >= CC_ROBOT_CLIMB_ROUTE_CAPACITY - 1) return false;
        reverse[count++] = index;
    }
    route->count = 0;
    route->length = 0.0f;
    for (int32_t index = count - 1; index >= 0; --index) {
        const ClimbSearchNode *source = &search[reverse[index]];
        CcRobotClimbNode *destination = &route->nodes[route->count++];
        destination->point = source->point;
        destination->normal = source->normal;
        destination->surface = SurfaceKind(source->normal);
        destination->cost = source->travelled;
        if (route->count > 1) {
            route->length += Length(Subtract(
                destination->point,
                route->nodes[route->count - 2].point));
        }
    }
    CcRobotClimbNode *last = &route->nodes[route->count - 1];
    float goal_distance = Length(Subtract(last->point, goal_point));
    if (goal_distance > 0.01f) {
        if (route->count >= CC_ROBOT_CLIMB_ROUTE_CAPACITY) return false;
        CcRobotClimbNode *goal = &route->nodes[route->count++];
        goal->point = goal_point;
        goal->normal = goal_normal;
        goal->surface = SurfaceKind(goal_normal);
        goal->cost = last->cost + goal_distance;
        route->length += goal_distance;
    } else {
        last->point = goal_point;
        last->normal = goal_normal;
        last->surface = SurfaceKind(goal_normal);
    }
    return true;
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

bool CcRobotPlanFreeClimb(CcLimbVec3 start_point,
                          CcLimbVec3 start_normal,
                          CcLimbVec3 goal_point,
                          CcLimbVec3 goal_normal,
                          float step_length,
                          float maximum_reach,
                          CcRobotSurfaceProbe probe,
                          void *context,
                          CcRobotClimbRoute *route)
{
    if (route == NULL || probe == NULL || step_length <= 0.02f ||
        maximum_reach < step_length) {
        return false;
    }
    *route = (CcRobotClimbRoute){0};
    start_normal = Normalize(start_normal, (CcLimbVec3){0.0f, 1.0f, 0.0f});
    goal_normal = Normalize(goal_normal, (CcLimbVec3){0.0f, 1.0f, 0.0f});

    ClimbSearchNode search[CC_ROBOT_CLIMB_SEARCH_CAPACITY] = {0};
    int32_t node_count = 1;
    search[0] = (ClimbSearchNode){
        .point = start_point,
        .normal = start_normal,
        .travelled = 0.0f,
        .heuristic = Length(Subtract(goal_point, start_point)),
        .parent = -1,
        .open = true,
    };
    const float merge_distance = step_length * 0.32f;
    const float search_radius = fminf(maximum_reach * 0.82f,
                                      step_length * 0.78f);

    while (true) {
        int32_t current_index = -1;
        float best_score = INFINITY;
        float best_heuristic = INFINITY;
        for (int32_t index = 0; index < node_count; ++index) {
            if (!search[index].open || search[index].closed) continue;
            float score = search[index].travelled + search[index].heuristic;
            if (score < best_score - 0.0001f ||
                (fabsf(score - best_score) <= 0.0001f &&
                 search[index].heuristic < best_heuristic)) {
                current_index = index;
                best_score = score;
                best_heuristic = search[index].heuristic;
            }
        }
        if (current_index < 0) return false;
        ClimbSearchNode *current = &search[current_index];
        current->open = false;
        current->closed = true;

        float distance_to_goal = Length(Subtract(goal_point, current->point));
        if (distance_to_goal <= maximum_reach &&
            Dot(current->normal, goal_normal) >= -0.45f) {
            return AppendClimbRoute(search, current_index, goal_point,
                                    goal_normal, route);
        }

        CcLimbVec3 reference = fabsf(current->normal.y) < 0.82f ?
            (CcLimbVec3){0.0f, 1.0f, 0.0f} :
            (CcLimbVec3){0.0f, 0.0f, 1.0f};
        CcLimbVec3 tangent_right = Normalize(
            Cross(reference, current->normal),
            (CcLimbVec3){1.0f, 0.0f, 0.0f});
        CcLimbVec3 tangent_up = Normalize(
            Cross(current->normal, tangent_right),
            (CcLimbVec3){0.0f, 1.0f, 0.0f});
        CcLimbVec3 goal_delta = Subtract(goal_point, current->point);
        CcLimbVec3 goal_tangent = Subtract(
            goal_delta, Scale(current->normal, Dot(goal_delta,
                                                   current->normal)));
        goal_tangent = Normalize(goal_tangent, tangent_up);

        CcLimbVec3 directions[9] = {
            tangent_right, Scale(tangent_right, -1.0f),
            tangent_up, Scale(tangent_up, -1.0f),
            Normalize(Add(tangent_right, tangent_up), tangent_up),
            Normalize(Subtract(tangent_right, tangent_up), tangent_right),
            Normalize(Subtract(tangent_up, tangent_right), tangent_up),
            Normalize(Scale(Add(tangent_right, tangent_up), -1.0f),
                      tangent_up),
            goal_tangent,
        };
        for (int32_t direction = 0; direction < 9; ++direction) {
            CcLimbVec3 base_sample = Add(
                current->point, Scale(directions[direction], step_length));
            CcLimbVec3 offsets[3] = {
                {0.0f, 0.0f, 0.0f},
                Scale(current->normal, search_radius * 0.34f),
                Scale(current->normal, -search_radius * 0.34f),
            };
            bool found = false;
            CcLimbVec3 candidate_point = {0};
            CcLimbVec3 candidate_normal = {0};
            float candidate_score = INFINITY;
            for (int32_t offset = 0; offset < 3; ++offset) {
                CcLimbVec3 point = {0};
                CcLimbVec3 normal = {0};
                if (!probe(context, Add(base_sample, offsets[offset]),
                           search_radius, &point, &normal)) {
                    continue;
                }
                normal = Normalize(normal, current->normal);
                float edge_length = Length(Subtract(point, current->point));
                float normal_dot = Dot(current->normal, normal);
                if (edge_length < step_length * 0.16f ||
                    edge_length > maximum_reach || normal_dot < -0.45f) {
                    continue;
                }
                float score = Length(Subtract(goal_point, point)) +
                              (1.0f - Dot(normal, goal_normal)) *
                                  step_length * 0.18f;
                if (score < candidate_score) {
                    found = true;
                    candidate_score = score;
                    candidate_point = point;
                    candidate_normal = normal;
                }
            }
            if (!found) continue;

            float edge_length = Length(Subtract(candidate_point,
                                                current->point));
            float turn = 1.0f - Clamp(Dot(current->normal, candidate_normal),
                                      -1.0f, 1.0f);
            float travelled = current->travelled + edge_length +
                              turn * step_length * 0.38f;
            int32_t matching_index = -1;
            for (int32_t index = 0; index < node_count; ++index) {
                if (ClimbPointMatches(&search[index], candidate_point,
                                      candidate_normal, merge_distance)) {
                    matching_index = index;
                    break;
                }
            }
            if (matching_index >= 0) {
                ClimbSearchNode *matching = &search[matching_index];
                if (travelled + 0.0001f < matching->travelled) {
                    matching->travelled = travelled;
                    matching->parent = current_index;
                    matching->heuristic = Length(Subtract(
                        goal_point, candidate_point));
                    matching->point = candidate_point;
                    matching->normal = candidate_normal;
                    matching->open = true;
                    matching->closed = false;
                }
                continue;
            }
            if (node_count >= CC_ROBOT_CLIMB_SEARCH_CAPACITY) continue;
            search[node_count++] = (ClimbSearchNode){
                .point = candidate_point,
                .normal = candidate_normal,
                .travelled = travelled,
                .heuristic = Length(Subtract(goal_point, candidate_point)),
                .parent = current_index,
                .open = true,
            };
        }
    }
}
