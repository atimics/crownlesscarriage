#include "client/cc_local3d.h"

#include "locomotion/cc_humanoid_skin.h"

#include "raymath.h"
#include "rlgl.h"

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const Color WORLD_VOID = {7, 14, 21, 255};
static const Color WORLD_INK = {231, 232, 211, 255};
static const Color WORLD_MUTED = {132, 154, 148, 255};
static const Color WORLD_TEAL = {104, 234, 207, 255};
static const Color WORLD_GOLD = {249, 197, 75, 255};
static const Color WORLD_DANGER = {218, 75, 86, 255};
static const Color WORLD_VIOLET = {195, 105, 221, 255};
static const float PLAYER_COLLISION_RADIUS = 0.30f;
static const float PERSON_COLLISION_RADIUS = 0.27f;
static const float COURSE_GUARD_SPACING = 1.18f;
static const float COURSE_RAIDER_SPACING = 1.48f;

typedef struct WorldLabel {
    Vector3 point;
    const char *text;
    Color color;
} WorldLabel;

typedef struct WorldBuilding {
    Rectangle footprint;
    float height;
    int32_t style;
    bool door;
} WorldBuilding;

/* One world unit is one metre. Even the smallest cottage has room for an
   actual household around the 1.9 m hero; the market is a 12 x 10 m hall. */
static const WorldBuilding WORLD_BUILDINGS[] = {
    {{20.0f, 15.0f, 8.0f, 10.0f}, 7.20f, 0, true},
    {{32.0f, 14.0f, 7.0f, 9.0f}, 6.40f, 1, true},
    {{44.0f, 16.0f, 12.0f, 10.0f}, 8.80f, 2, true},
    {{20.0f, 33.0f, 10.0f, 8.0f}, 6.60f, 3, true},
    {{34.0f, 35.0f, 7.0f, 8.0f}, 5.80f, 0, true},
    {{55.0f, 42.0f, 8.0f, 7.0f}, 6.20f, 1, true},
    {{55.0f, 29.0f, 6.5f, 8.5f}, 5.90f, 0, true},
    {{55.0f, 14.0f, 7.0f, 9.0f}, 6.10f, 3, true},
    {{32.0f, 47.0f, 8.0f, 7.0f}, 5.80f, 1, true},
    {{44.0f, 46.0f, 10.0f, 7.0f}, 6.90f, 2, true}
};

typedef struct WorldStructure {
    Rectangle footprint;
    float height;
} WorldStructure;

/* A 25 x 23 m fortified bailey, with a person-width gate left physically open. */
static const WorldStructure CASTLE_STRUCTURES[] = {
    {{66.0f, 9.0f, 1.2f, 23.0f}, 6.5f},
    {{90.0f, 9.0f, 1.2f, 23.0f}, 6.5f},
    {{66.0f, 9.0f, 25.2f, 1.2f}, 6.5f},
    {{66.0f, 30.8f, 9.5f, 1.2f}, 6.5f},
    {{81.5f, 30.8f, 9.7f, 1.2f}, 6.5f},
    {{72.0f, 13.0f, 13.0f, 9.0f}, 11.5f},
    {{75.3f, 27.8f, 2.0f, 3.0f}, 9.0f},
    {{79.7f, 27.8f, 2.0f, 3.0f}, 9.0f},
    {{64.8f, 7.8f, 4.0f, 4.0f}, 12.5f},
    {{88.4f, 7.8f, 4.0f, 4.0f}, 12.5f},
    {{64.8f, 29.2f, 4.0f, 4.0f}, 12.5f},
    {{88.4f, 29.2f, 4.0f, 4.0f}, 12.5f}
};
static const Rectangle CARRIAGE_FOOTPRINT = {35.20f, 29.00f, 3.20f, 5.40f};
static const Rectangle DUNGEON_FOOTPRINT = {27.40f, 49.70f, 3.20f, 1.60f};
static const Rectangle COURSE_POOL = {10.00f, 9.05f, 2.55f, 1.38f};
static const float COURSE_WATER_SURFACE = 0.82f;
static const Rectangle MARKET_COUNTER_FOOTPRINT = {6.05f, 1.84f, 2.10f, 0.72f};
static const Rectangle MARKET_SHELF_FOOTPRINT = {1.10f, 1.175f, 0.72f, 3.45f};
static const Vector2 STREET_PEOPLE[] = {
    {50.50f, 28.80f}, {42.00f, 30.80f}, {37.00f, 44.00f},
    {29.00f, 28.00f}, {52.00f, 31.00f}, {58.00f, 26.50f}
};
static const Vector2 MARKET_PEOPLE[] = {{6.55f, 1.60f}};

typedef struct NavPlatform {
    float x;
    float z;
    float width;
    float depth;
    float height;
    int32_t style;
} NavPlatform;

static const NavPlatform STREET_PLATFORMS[] = {
    {3.00f, 7.00f, 1.00f, 1.00f, 1.65f, 0},
    {10.15f, 0.85f, 0.70f, 0.70f, 0.90f, 1},
    {11.75f, 1.45f, 1.05f, 1.05f, 1.05f, 2},
    {13.10f, 2.35f, 0.95f, 1.20f, 1.52f, 3},
    {10.20f, 3.55f, 2.70f, 0.62f, 1.65f, 2},
    {9.80f, 5.15f, 0.95f, 0.95f, 1.35f, 3},
    {11.25f, 5.70f, 0.82f, 0.82f, 0.92f, 1},
    {12.55f, 6.40f, 1.10f, 1.10f, 1.65f, 2},
    {9.85f, 7.80f, 2.60f, 1.00f, 0.90f, 1},
    {13.10f, 8.65f, 0.95f, 0.95f, 1.60f, 3}
};

static const Vector3 COURSE_WAYPOINTS[] = {
    {9.45f, 0.0f, 1.20f}, {10.50f, 0.0f, 1.20f},
    {11.22f, 0.0f, 2.95f}, {12.27f, 0.0f, 1.97f},
    {13.00f, 0.0f, 1.45f}, {13.57f, 0.0f, 2.95f},
    {14.35f, 0.0f, 4.15f}, {11.55f, 0.0f, 3.86f},
    {9.10f, 0.0f, 3.86f}, {10.27f, 0.0f, 5.62f},
    {10.65f, 0.0f, 6.75f}, {11.66f, 0.0f, 6.11f},
    {12.05f, 0.0f, 7.45f}, {13.10f, 0.0f, 6.95f},
    {14.15f, 0.0f, 7.75f}, {11.15f, 0.0f, 8.30f},
    {12.55f, 0.0f, 9.65f}, {13.57f, 0.0f, 9.12f},
    {14.45f, 0.0f, 9.65f}, {9.40f, 0.0f, 9.65f}
};

static Camera3D LocalCamera(bool interior, Vector3 focus);
static float WrapAngle(float angle);
static void UpdateHeroCape(CcLocalAgent *agent, float delta_time);

static bool CourseWaterContains(float x, float z)
{
    return x >= COURSE_POOL.x && x <= COURSE_POOL.x + COURSE_POOL.width &&
           z >= COURSE_POOL.y && z <= COURSE_POOL.y + COURSE_POOL.height;
}

static float SurfaceHeightAt(bool market_interior, float x, float z)
{
    float height = 0.0f;
    if (market_interior) return height;
    for (int32_t i = 0; i < (int32_t)(sizeof(STREET_PLATFORMS) /
                                      sizeof(STREET_PLATFORMS[0])); ++i) {
        const NavPlatform *platform = &STREET_PLATFORMS[i];
        if (x >= platform->x && x <= platform->x + platform->width &&
            z >= platform->z && z <= platform->z + platform->depth &&
            platform->height > height) {
            height = platform->height;
        }
    }
    return height;
}

static float BodySurfaceHeightAt(bool market_interior, float x, float z)
{
    float height = 0.0f;
    if (market_interior) return height;
    for (int32_t i = 0; i < (int32_t)(sizeof(STREET_PLATFORMS) /
                                      sizeof(STREET_PLATFORMS[0])); ++i) {
        const NavPlatform *platform = &STREET_PLATFORMS[i];
        float edge_release = platform->style == 0 ? 0.0f : 0.10f;
        if (x >= platform->x + edge_release &&
            x <= platform->x + platform->width - edge_release &&
            z >= platform->z + edge_release &&
            z <= platform->z + platform->depth - edge_release &&
            platform->height > height) {
            height = platform->height;
        }
    }
    return height;
}

static float FootprintDistanceSquared(float x, float z, Rectangle footprint)
{
    float closest_x = fmaxf(footprint.x,
                            fminf(x, footprint.x + footprint.width));
    float closest_z = fmaxf(footprint.y,
                            fminf(z, footprint.y + footprint.height));
    float delta_x = x - closest_x;
    float delta_z = z - closest_z;
    return delta_x * delta_x + delta_z * delta_z;
}

static bool CircleTouchesFootprint(float x, float z, float radius,
                                   Rectangle footprint)
{
    return FootprintDistanceSquared(x, z, footprint) < radius * radius;
}

static bool StaticBodyBlocked(bool market_interior, float x, float z, float radius)
{
    float maximum_x = market_interior ? 8.72f : CC_LOCAL_WORLD_WIDTH - 0.28f;
    float maximum_z = market_interior ? 6.72f : CC_LOCAL_WORLD_DEPTH - 0.28f;
    if (x < 0.28f + radius || x > maximum_x - radius ||
        z < 0.28f + radius || z > maximum_z - radius) return true;
    if (market_interior) {
        if (CircleTouchesFootprint(x, z, radius, MARKET_COUNTER_FOOTPRINT) ||
            CircleTouchesFootprint(x, z, radius, MARKET_SHELF_FOOTPRINT)) {
            return true;
        }
        for (int32_t i = 0; i < (int32_t)(sizeof(MARKET_PEOPLE) /
                                          sizeof(MARKET_PEOPLE[0])); ++i) {
            float dx = x - MARKET_PEOPLE[i].x;
            float dz = z - MARKET_PEOPLE[i].y;
            float clearance = radius + PERSON_COLLISION_RADIUS;
            if (dx * dx + dz * dz < clearance * clearance) return true;
        }
        return false;
    }
    for (int32_t i = 0; i < (int32_t)(sizeof(WORLD_BUILDINGS) /
                                      sizeof(WORLD_BUILDINGS[0])); ++i) {
        if (CircleTouchesFootprint(x, z, radius,
                                   WORLD_BUILDINGS[i].footprint)) return true;
    }
    for (int32_t i = 0; i < (int32_t)(sizeof(CASTLE_STRUCTURES) /
                                      sizeof(CASTLE_STRUCTURES[0])); ++i) {
        if (CircleTouchesFootprint(x, z, radius,
                                   CASTLE_STRUCTURES[i].footprint)) return true;
    }
    if (CircleTouchesFootprint(x, z, radius, CARRIAGE_FOOTPRINT) ||
        CircleTouchesFootprint(x, z, radius, DUNGEON_FOOTPRINT)) return true;
    for (int32_t i = 0; i < (int32_t)(sizeof(STREET_PEOPLE) /
                                      sizeof(STREET_PEOPLE[0])); ++i) {
        float dx = x - STREET_PEOPLE[i].x;
        float dz = z - STREET_PEOPLE[i].y;
        float clearance = radius + PERSON_COLLISION_RADIUS;
        if (dx * dx + dz * dz < clearance * clearance) return true;
    }
    return false;
}

typedef struct LocalProbeContext {
    bool market_interior;
} LocalProbeContext;

static CcLimbVec3 ToLimbVector(Vector3 value)
{
    return (CcLimbVec3){value.x, value.y, value.z};
}

static Vector3 FromLimbVector(CcLimbVec3 value)
{
    return (Vector3){value.x, value.y, value.z};
}

static bool ProbeLocalSurface(void *raw_context, CcLimbVec3 origin,
                              float maximum_drop, CcLimbVec3 *point,
                              CcLimbVec3 *normal)
{
    const LocalProbeContext *context = raw_context;
    bool interior = context != NULL && context->market_interior;
    if (StaticBodyBlocked(interior, origin.x, origin.z, 0.025f)) return false;
    float height = SurfaceHeightAt(interior, origin.x, origin.z) + 0.035f;
    if (height > origin.y + 0.05f || origin.y - height > maximum_drop) return false;
    *point = (CcLimbVec3){origin.x, height, origin.z};
    *normal = (CcLimbVec3){0.0f, 1.0f, 0.0f};
    return true;
}

static Vector3 ShellPodBaseCenter(const CcLocalAgent *agent)
{
    return (Vector3){agent->position.x,
                     agent->position.y + agent->limb_rig.morphology.body_height,
                     agent->position.z};
}

static Vector3 ShellPodCenter(const CcLocalAgent *agent)
{
    Vector3 body = ShellPodBaseCenter(agent);
    body.y += agent->limb_rig.supported_height_offset;
    return body;
}

static const NavPlatform *ClimbPlatformAt(float x, float z, float radius,
                                          float feet_height)
{
    const NavPlatform *highest = NULL;
    for (int32_t i = 0; i < (int32_t)(sizeof(STREET_PLATFORMS) /
                                      sizeof(STREET_PLATFORMS[0])); ++i) {
        const NavPlatform *platform = &STREET_PLATFORMS[i];
        Rectangle footprint = {platform->x, platform->z,
                               platform->width, platform->depth};
        if (platform->height > feet_height + 0.24f &&
            CircleTouchesFootprint(x, z, radius, footprint) &&
            (highest == NULL || platform->height > highest->height)) {
            highest = platform;
        }
    }
    return highest;
}

static const NavPlatform *SupportingPlatformAt(float x, float z,
                                               float feet_height)
{
    for (int32_t i = 0; i < (int32_t)(sizeof(STREET_PLATFORMS) /
                                      sizeof(STREET_PLATFORMS[0])); ++i) {
        const NavPlatform *platform = &STREET_PLATFORMS[i];
        if (fabsf(platform->height - feet_height) > 0.08f) continue;
        if (x >= platform->x && x <= platform->x + platform->width &&
            z >= platform->z && z <= platform->z + platform->depth) {
            return platform;
        }
    }
    return NULL;
}

const char *CcLocalTraversalName(CcTraversalMode mode)
{
    switch (mode) {
        case CC_TRAVERSAL_WALK: return "WALK";
        case CC_TRAVERSAL_JUMP: return "JUMP";
        case CC_TRAVERSAL_CLIMB: return "CLIMB";
        case CC_TRAVERSAL_DESCEND: return "DOWN-CLIMB";
        case CC_TRAVERSAL_SWIM: return "SWIM";
        case CC_TRAVERSAL_DROP: return "DROP";
        case CC_TRAVERSAL_RAGDOLL: return "RAGDOLL";
        case CC_TRAVERSAL_GET_UP: return "GET UP";
        case CC_TRAVERSAL_IDLE:
        default: return "IDLE";
    }
}

Vector2 CcLocalAgentPosition(const CcLocalAgent *agent)
{
    return (Vector2){agent->position.x, agent->position.z};
}

void CcLocalAgentInit(CcLocalAgent *agent, Vector2 position, bool market_interior)
{
    *agent = (CcLocalAgent){0};
    agent->position = (Vector3){position.x,
                                SurfaceHeightAt(market_interior, position.x,
                                                position.y),
                                position.y};
    agent->facing_yaw = 0.75f * PI;
    agent->traversal = CC_TRAVERSAL_IDLE;
    agent->radius = PLAYER_COLLISION_RADIUS;
    agent->grounded = true;
    agent->allow_downclimb = true;
    agent->crowned = true;
    agent->tunic_color = (Color){42, 128, 136, 255};
    agent->target_point = agent->position;
    agent->combat.health = CC_LOCAL_COMBAT_MAX_HEALTH;
    agent->combat.posture = CC_LOCAL_COMBAT_MAX_POSTURE;
    agent->combat.target_index = -1;
    CcLocalAgentSetMorphology(agent, CC_MORPHOLOGY_BIPED, market_interior);
}

void CcLocalAgentSetMorphology(CcLocalAgent *agent, CcMorphologyPreset preset,
                               bool market_interior)
{
    CcLimbMorphology morphology;
    if (!CcLimbMorphologyFromPreset(&morphology, preset)) return;
    agent->morphology = preset;
    agent->ragdoll_visual_blend = 0.0f;
    LocalProbeContext context = {.market_interior = market_interior};
    Vector3 body = {agent->position.x,
                    agent->position.y + morphology.body_height,
                    agent->position.z};
    CcLimbRigInit(&agent->limb_rig, &morphology, ToLimbVector(body),
                  agent->facing_yaw, ProbeLocalSurface, &context);
    agent->humanoid_needs_reset = false;
    if (preset == CC_MORPHOLOGY_BIPED) {
        CcHumanoidGaitInit(&agent->humanoid, ToLimbVector(agent->position),
                            agent->facing_yaw, ProbeLocalSurface, &context);
        agent->render_pose = agent->humanoid.pose;
        agent->render_pose_valid = true;
        agent->simulation_accumulator = 0.0f;
        agent->cape = (CcLocalCapeState){0};
        UpdateHeroCape(agent, 1.0f / 60.0f);
    } else {
        agent->humanoid = (CcHumanoidGait){0};
        agent->render_pose = (CcHumanoidPose){0};
        agent->render_pose_valid = false;
        agent->simulation_accumulator = 0.0f;
        agent->cape = (CcLocalCapeState){0};
    }
}

void CcLocalAgentCycleMorphology(CcLocalAgent *agent, bool market_interior)
{
    CcMorphologyPreset next = (CcMorphologyPreset)(agent->morphology + 1);
    if (next >= CC_MORPHOLOGY_PRESET_COUNT) next = CC_MORPHOLOGY_BIPED;
    CcLocalAgentSetMorphology(agent, next, market_interior);
}

const char *CcLocalAgentMorphologyName(const CcLocalAgent *agent)
{
    return agent->limb_rig.morphology.name != NULL ?
           agent->limb_rig.morphology.name : "UNRIGGED";
}

static float CombatClamp(float value, float minimum, float maximum)
{
    return fmaxf(minimum, fminf(value, maximum));
}

static Vector3 CombatSubtract(Vector3 a, Vector3 b)
{
    return (Vector3){a.x - b.x, a.y - b.y, a.z - b.z};
}

static Vector3 CombatAdd(Vector3 a, Vector3 b)
{
    return (Vector3){a.x + b.x, a.y + b.y, a.z + b.z};
}

static Vector3 CombatScale(Vector3 value, float scale)
{
    return (Vector3){value.x * scale, value.y * scale, value.z * scale};
}

static float CombatDot(Vector3 a, Vector3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static float CombatLengthSquared(Vector3 value)
{
    return CombatDot(value, value);
}

static float CombatHorizontalDistanceSquared(const CcLocalAgent *a,
                                             const CcLocalAgent *b)
{
    float x = b->position.x - a->position.x;
    float z = b->position.z - a->position.z;
    return x * x + z * z;
}

static float CombatFacingDot(const CcLocalAgent *agent, Vector3 point)
{
    float x = point.x - agent->position.x;
    float z = point.z - agent->position.z;
    float length = sqrtf(x * x + z * z);
    if (length <= 0.0001f) return 1.0f;
    return (sinf(agent->facing_yaw) * x + cosf(agent->facing_yaw) * z) /
           length;
}

static float CombatSegmentDistanceSquared(Vector3 first_start,
                                          Vector3 first_end,
                                          Vector3 second_start,
                                          Vector3 second_end)
{
    const float epsilon = 0.000001f;
    Vector3 first = CombatSubtract(first_end, first_start);
    Vector3 second = CombatSubtract(second_end, second_start);
    Vector3 offset = CombatSubtract(first_start, second_start);
    float first_length = CombatDot(first, first);
    float second_length = CombatDot(second, second);
    float second_offset = CombatDot(second, offset);
    float first_amount = 0.0f;
    float second_amount = 0.0f;
    if (first_length <= epsilon && second_length <= epsilon) {
        return CombatLengthSquared(offset);
    }
    if (first_length <= epsilon) {
        second_amount = CombatClamp(second_offset / second_length, 0.0f, 1.0f);
    } else {
        float first_offset = CombatDot(first, offset);
        if (second_length <= epsilon) {
            first_amount = CombatClamp(-first_offset / first_length, 0.0f,
                                       1.0f);
        } else {
            float directions = CombatDot(first, second);
            float denominator = first_length * second_length -
                                directions * directions;
            if (fabsf(denominator) > epsilon) {
                first_amount = CombatClamp(
                    (directions * second_offset -
                     first_offset * second_length) / denominator,
                    0.0f, 1.0f);
            }
            float second_numerator = directions * first_amount + second_offset;
            if (second_numerator < 0.0f) {
                second_amount = 0.0f;
                first_amount = CombatClamp(-first_offset / first_length,
                                           0.0f, 1.0f);
            } else if (second_numerator > second_length) {
                second_amount = 1.0f;
                first_amount = CombatClamp(
                    (directions - first_offset) / first_length, 0.0f, 1.0f);
            } else {
                second_amount = second_numerator / second_length;
            }
        }
    }
    Vector3 first_closest = CombatAdd(first_start,
                                      CombatScale(first, first_amount));
    Vector3 second_closest = CombatAdd(second_start,
                                       CombatScale(second, second_amount));
    return CombatLengthSquared(CombatSubtract(first_closest, second_closest));
}

static float CombatWeaponExtension(CcCombatTeam team)
{
    if (team == CC_COMBAT_GUARD) return 0.66f;
    if (team == CC_COMBAT_RAIDER) return 0.42f;
    return 0.10f;
}

static float CombatStrikeReach(CcCombatTeam team)
{
    if (team == CC_COMBAT_GUARD) return 1.38f;
    if (team == CC_COMBAT_RAIDER) return 1.18f;
    return 1.04f;
}

static float CombatStrikeDamage(CcCombatTeam team)
{
    if (team == CC_COMBAT_GUARD) return 21.0f;
    if (team == CC_COMBAT_RAIDER) return 19.0f;
    return 24.0f;
}

static bool CombatTeamsHostile(CcCombatTeam first, CcCombatTeam second)
{
    if (first == CC_COMBAT_NEUTRAL || second == CC_COMBAT_NEUTRAL) {
        return false;
    }
    bool first_raider = first == CC_COMBAT_RAIDER;
    bool second_raider = second == CC_COMBAT_RAIDER;
    return first_raider != second_raider;
}

static void CombatApplyKnockback(CcLocalAgent *attacker,
                                CcLocalAgent *defender, float speed)
{
    float x = defender->position.x - attacker->position.x;
    float z = defender->position.z - attacker->position.z;
    float length = sqrtf(x * x + z * z);
    if (length <= 0.0001f) {
        x = sinf(attacker->facing_yaw);
        z = cosf(attacker->facing_yaw);
        length = 1.0f;
    }
    defender->combat.knockback_velocity.x = x / length * speed;
    defender->combat.knockback_velocity.z = z / length * speed;
    if (defender->humanoid.initialized &&
        !defender->humanoid.ragdoll.active) {
        defender->humanoid.body.root.velocity.x += x / length * speed * 0.46f;
        defender->humanoid.body.root.velocity.z += z / length * speed * 0.46f;
        defender->humanoid.root_velocity.x =
            defender->humanoid.body.root.velocity.x;
        defender->humanoid.root_velocity.z =
            defender->humanoid.body.root.velocity.z;
    }
}

static void CombatDefeat(CcLocalAgent *agent)
{
    agent->combat.health = 0.0f;
    agent->combat.defeated = true;
    agent->combat.focus_valid = false;
    agent->combat.target_index = -1;
    agent->exact_target_valid = false;
    agent->combat.stagger_seconds = 1.10f;
    agent->combat.recovery_seconds = agent->combat.team == CC_COMBAT_PLAYER ?
                                     2.75f : 0.0f;
    CcHumanoidGaitSetGuarded(&agent->humanoid, false);
}

void CcLocalCombatSetTeam(CcLocalAgent *agent, CcCombatTeam team)
{
    if (agent == NULL) return;
    agent->combat.team = team;
}

void CcLocalCombatSetFocus(CcLocalAgent *agent,
                           const CcLocalAgent *target)
{
    if (agent == NULL) return;
    if (target == NULL || target->combat.defeated) {
        CcLocalCombatClearFocus(agent);
        return;
    }
    agent->combat.focus_point = target->position;
    agent->combat.focus_valid = true;
}

void CcLocalCombatClearFocus(CcLocalAgent *agent)
{
    if (agent == NULL) return;
    agent->combat.focus_valid = false;
    agent->combat.target_index = -1;
}

void CcLocalCombatSetGuarded(CcLocalAgent *agent,
                             const CcLocalAgent *target, bool guarded)
{
    if (agent == NULL) return;
    if (guarded && target != NULL) CcLocalCombatSetFocus(agent, target);
    if (agent->combat.defeated || agent->combat.stagger_seconds > 0.0f) {
        guarded = false;
    }
    CcHumanoidGaitSetGuarded(&agent->humanoid, guarded);
    if (!guarded && agent->humanoid.action != CC_HUMANOID_ACTION_STRIKE) {
        CcLocalCombatClearFocus(agent);
    }
}

bool CcLocalCombatBeginStrike(CcLocalAgent *agent,
                              const CcLocalAgent *target)
{
    if (agent == NULL || agent->combat.defeated ||
        agent->combat.stagger_seconds > 0.0f || !agent->grounded) return false;
    if (target != NULL) CcLocalCombatSetFocus(agent, target);
    if (!CcHumanoidGaitBeginStrike(&agent->humanoid, 1)) return false;
    agent->combat.strike_resolved = false;
    return true;
}

bool CcLocalAgentJump(CcLocalAgent *agent)
{
    if (agent == NULL || agent->morphology != CC_MORPHOLOGY_BIPED ||
        !agent->grounded || agent->climbing || agent->swimming ||
        agent->combat.defeated || agent->combat.stagger_seconds > 0.0f ||
        !CcHumanoidGaitBeginJump(&agent->humanoid)) {
        return false;
    }
    const float takeoff_speed = 4.35f;
    agent->velocity.y = takeoff_speed;
    agent->grounded = false;
    agent->humanoid.body.root.velocity.y = takeoff_speed;
    agent->humanoid.root_velocity.y = takeoff_speed;
    return true;
}

CcCombatOutcome CcLocalCombatResolveStrike(CcLocalAgent *attacker,
                                           CcLocalAgent *defender)
{
    if (attacker == NULL || attacker->combat.strike_resolved) {
        return CC_COMBAT_OUTCOME_NONE;
    }
    attacker->combat.strike_resolved = true;
    if (defender == NULL || attacker == defender || attacker->combat.defeated ||
        defender->combat.defeated ||
        !CombatTeamsHostile(attacker->combat.team, defender->combat.team) ||
        attacker->humanoid.ragdoll.active || attacker->humanoid.recovering ||
        defender->humanoid.ragdoll.active || defender->humanoid.recovering ||
        defender->climbing || defender->swimming) {
        return CC_COMBAT_OUTCOME_MISS;
    }
    float reach = CombatStrikeReach(attacker->combat.team);
    if (CombatHorizontalDistanceSquared(attacker, defender) > reach * reach ||
        fabsf(attacker->position.y - defender->position.y) > 0.62f ||
        CombatFacingDot(attacker, defender->position) < 0.24f) {
        return CC_COMBAT_OUTCOME_MISS;
    }

    int32_t arm = attacker->humanoid.strike_side == 0 ? 0 : 1;
    Vector3 previous_hand = FromLimbVector(
        attacker->humanoid.previous_pose.hand[arm]);
    Vector3 current_hand = FromLimbVector(attacker->humanoid.pose.hand[arm]);
    Vector3 forward = {sinf(attacker->facing_yaw), 0.0f,
                       cosf(attacker->facing_yaw)};
    float extension = CombatWeaponExtension(attacker->combat.team);
    Vector3 previous_tip = CombatAdd(previous_hand,
                                     CombatScale(forward, extension));
    Vector3 current_tip = CombatAdd(current_hand,
                                    CombatScale(forward, extension));
    Vector3 body_bottom = {defender->position.x,
                           defender->position.y + 0.38f,
                           defender->position.z};
    Vector3 body_top = {defender->position.x,
                        defender->position.y + 1.52f,
                        defender->position.z};
    float collision_distance = CombatSegmentDistanceSquared(
        current_hand, current_tip, body_bottom, body_top);
    collision_distance = fminf(collision_distance,
        CombatSegmentDistanceSquared(previous_hand, previous_tip,
                                     body_bottom, body_top));
    collision_distance = fminf(collision_distance,
        CombatSegmentDistanceSquared(previous_tip, current_tip,
                                     body_bottom, body_top));
    const float combined_radius = 0.41f;
    if (collision_distance > combined_radius * combined_radius) {
        return CC_COMBAT_OUTCOME_MISS;
    }

    bool guarded = defender->humanoid.action == CC_HUMANOID_ACTION_GUARD &&
                   defender->combat.posture > 0.0f &&
                   CombatFacingDot(defender, attacker->position) >= 0.28f;
    attacker->combat.hitstop_seconds = fmaxf(
        attacker->combat.hitstop_seconds, guarded ? 0.040f : 0.055f);
    defender->combat.hitstop_seconds = fmaxf(
        defender->combat.hitstop_seconds, guarded ? 0.050f : 0.075f);
    defender->combat.hit_flash_seconds = fmaxf(
        defender->combat.hit_flash_seconds, guarded ? 0.075f : 0.14f);
    if (guarded) {
        float posture_damage = attacker->combat.team == CC_COMBAT_GUARD ?
                               42.0f :
                               attacker->combat.team == CC_COMBAT_RAIDER ?
                               24.0f : 32.0f;
        defender->combat.posture = fmaxf(0.0f,
                                         defender->combat.posture -
                                         posture_damage);
        CombatApplyKnockback(attacker, defender, 0.34f);
        if (defender->combat.posture > 0.0f) {
            return CC_COMBAT_OUTCOME_BLOCKED;
        }
        CcHumanoidGaitSetGuarded(&defender->humanoid, false);
        defender->combat.health = fmaxf(
            0.0f, defender->combat.health - CombatStrikeDamage(
                attacker->combat.team) * 0.65f);
        defender->combat.stagger_seconds = 0.78f;
        CombatApplyKnockback(attacker, defender, 1.22f);
        if (defender->combat.health <= 0.0f) {
            CombatDefeat(defender);
            return CC_COMBAT_OUTCOME_DEFEATED;
        }
        return CC_COMBAT_OUTCOME_GUARD_BROKEN;
    }

    defender->combat.health = fmaxf(
        0.0f, defender->combat.health - CombatStrikeDamage(attacker->combat.team));
    defender->combat.posture = fmaxf(0.0f, defender->combat.posture - 12.0f);
    defender->combat.stagger_seconds = 0.34f;
    CombatApplyKnockback(attacker, defender, 1.06f);
    if (defender->combat.health <= 0.0f) {
        CombatDefeat(defender);
        return CC_COMBAT_OUTCOME_DEFEATED;
    }
    return CC_COMBAT_OUTCOME_HIT;
}

const char *CcLocalCombatOutcomeName(CcCombatOutcome outcome)
{
    switch (outcome) {
        case CC_COMBAT_OUTCOME_MISS: return "WHIFF";
        case CC_COMBAT_OUTCOME_HIT: return "HIT";
        case CC_COMBAT_OUTCOME_BLOCKED: return "BLOCKED";
        case CC_COMBAT_OUTCOME_GUARD_BROKEN: return "GUARD BROKEN";
        case CC_COMBAT_OUTCOME_DEFEATED: return "DOWNED";
        case CC_COMBAT_OUTCOME_NONE:
        default: return "";
    }
}

const char *CcLocalCombatTeamName(CcCombatTeam team)
{
    switch (team) {
        case CC_COMBAT_PLAYER: return "YOU";
        case CC_COMBAT_GUARD: return "GUARD";
        case CC_COMBAT_RAIDER: return "RAIDER";
        case CC_COMBAT_NEUTRAL:
        default: return "";
    }
}

void CcLocalCourseInit(CcLocalCourse *course)
{
    static const int32_t starts[CC_LOCAL_COURSE_RUNNER_COUNT] = {0, 6, 12};
    static const Color colors[CC_LOCAL_COURSE_RUNNER_COUNT] = {
        {50, 151, 160, 255}, {166, 91, 132, 255}, {176, 122, 54, 255}
    };
    static const Vector2 traveller_entries[CC_LOCAL_TRAVELLER_COUNT] = {
        {18.00f, 0.72f}, {31.00f, 71.28f},
        {43.00f, 0.72f}, {64.00f, 71.28f}
    };
    static const Vector2 traveller_exits[CC_LOCAL_TRAVELLER_COUNT] = {
        {18.00f, 71.28f}, {31.00f, 0.72f},
        {43.00f, 71.28f}, {64.00f, 0.72f}
    };
    static const Color traveller_colors[CC_LOCAL_TRAVELLER_COUNT] = {
        {118, 134, 145, 255}, {150, 105, 91, 255},
        {73, 137, 121, 255}, {139, 102, 153, 255}
    };
    const int32_t waypoint_count = (int32_t)(sizeof(COURSE_WAYPOINTS) /
                                              sizeof(COURSE_WAYPOINTS[0]));
    *course = (CcLocalCourse){0};
    for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
        const Vector3 start = COURSE_WAYPOINTS[starts[i]];
        CcLocalCourseRunner *runner = &course->runners[i];
        CcLocalAgentInit(&runner->agent, (Vector2){start.x, start.z}, false);
        CcLocalCombatSetTeam(&runner->agent, CC_COMBAT_GUARD);
        runner->agent.crowned = false;
        runner->agent.tunic_color = colors[i];
        runner->marker_color = colors[i];
        runner->duty = CC_GUARD_TRAINING;
        runner->next_waypoint = (starts[i] + 1) % waypoint_count;
        runner->pause_seconds = 0.20f + (float)i * 0.12f;
        runner->attack_cooldown = 0.28f + (float)i * 0.18f;
        if (CcLocalAgentSetExactTarget(
                &runner->agent, COURSE_WAYPOINTS[runner->next_waypoint], false)) {
            runner->next_waypoint = (runner->next_waypoint + 1) % waypoint_count;
        }
    }
    for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
        CcLocalAgentInit(&course->raiders[i],
                         (Vector2){CC_LOCAL_WORLD_WIDTH - 0.80f,
                                   38.60f + (float)i * 2.80f}, false);
        CcLocalCombatSetTeam(&course->raiders[i], CC_COMBAT_RAIDER);
        course->raiders[i].crowned = false;
        course->raiders[i].tunic_color = (Color){126, 55, 61, 255};
        course->raider_entry[i] = course->raiders[i].position;
    }
    for (int32_t i = 0; i < CC_LOCAL_TRAVELLER_COUNT; ++i) {
        CcLocalTraveller *traveller = &course->travellers[i];
        traveller->entry = (Vector3){traveller_entries[i].x, 0.0f,
                                     traveller_entries[i].y};
        traveller->exit = (Vector3){traveller_exits[i].x, 0.0f,
                                    traveller_exits[i].y};
        CcLocalAgentInit(&traveller->agent, traveller_entries[i], false);
        traveller->agent.crowned = false;
        traveller->agent.tunic_color = traveller_colors[i];
        traveller->active = i < 2;
        traveller->respawn_delay = 1.8f + (float)i * 1.4f;
        if (traveller->active) {
            (void)CcLocalAgentSetExactTarget(&traveller->agent,
                                             traveller->exit, false);
        }
    }
    course->alarm_countdown = 8.0f;
    course->raider_attack_cooldown[0] = 0.52f;
    course->raider_attack_cooldown[1] = 0.78f;
}

static bool CourseCombatOriginOpen(Vector3 origin)
{
    for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
        float z = origin.z + ((float)i - 1.0f) * COURSE_GUARD_SPACING;
        if (StaticBodyBlocked(false, origin.x - 2.05f, z,
                              PLAYER_COLLISION_RADIUS)) return false;
    }
    for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
        float z = origin.z + ((float)i - 0.5f) * COURSE_RAIDER_SPACING;
        if (StaticBodyBlocked(false, origin.x + 1.85f, z,
                              PLAYER_COLLISION_RADIUS)) return false;
    }
    return !StaticBodyBlocked(false, origin.x, origin.z,
                              PLAYER_COLLISION_RADIUS);
}

static void CoursePrepareCombatant(CcLocalAgent *agent, CcCombatTeam team)
{
    agent->combat = (CcCombatState){
        .health = CC_LOCAL_COMBAT_MAX_HEALTH,
        .posture = CC_LOCAL_COMBAT_MAX_POSTURE,
        .target_index = -1,
        .team = team,
    };
    agent->exact_target_valid = false;
    agent->target_valid = false;
    CcHumanoidGaitSetGuarded(&agent->humanoid, false);
}

void CcLocalCourseRaiseAlarmNear(CcLocalCourse *course,
                                 const CcLocalAgent *player)
{
    if (course->alarm_active) return;
    Vector3 origin = player != NULL ?
        (Vector3){player->position.x + 4.65f, 0.0f,
                  player->position.z + 0.60f} :
        (Vector3){CC_LOCAL_START_X + 4.65f, 0.0f,
                  CC_LOCAL_START_Z + 0.60f};
    if (!CourseCombatOriginOpen(origin)) {
        origin = (Vector3){CC_LOCAL_START_X + 4.65f, 0.0f,
                           CC_LOCAL_START_Z + 0.60f};
    }
    if (!CourseCombatOriginOpen(origin)) {
        origin = (Vector3){44.80f, 0.0f, 40.20f};
    }
    course->combat_origin = origin;
    course->combat_origin_valid = true;
    course->alarm_active = true;
    course->raiders_retreating = false;
    course->engagement_time = 0.0f;
    course->raider_resolve = 100;
    course->last_outcome = CC_COMBAT_OUTCOME_NONE;
    course->last_attacker_team = CC_COMBAT_NEUTRAL;
    course->combat_event_seconds = 0.0f;
    for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
        CcLocalCourseRunner *runner = &course->runners[i];
        course->guard_entry[i] = runner->agent.position;
        CoursePrepareCombatant(&runner->agent, CC_COMBAT_GUARD);
        runner->agent.crowned = false;
        runner->agent.tunic_color = runner->marker_color;
        runner->duty = CC_GUARD_RESPONDING;
        runner->response_stage = 0;
        runner->response_waypoint_active = false;
        runner->attack_cooldown = 0.22f + (float)i * 0.20f;
    }
    for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
        CcLocalAgent *raider = &course->raiders[i];
        course->raider_entry[i] = raider->position;
        CoursePrepareCombatant(raider, CC_COMBAT_RAIDER);
        raider->crowned = false;
        raider->tunic_color = (Color){126, 55, 61, 255};
        course->raider_response_stage[i] = 0;
        course->raider_response_waypoint_active[i] = false;
        course->raider_attack_cooldown[i] = 0.46f + (float)i * 0.24f;
    }
}

void CcLocalCourseRaiseAlarm(CcLocalCourse *course)
{
    CcLocalCourseRaiseAlarmNear(course, NULL);
}

static bool CourseAgentBusy(const CcLocalAgent *agent)
{
    return agent->climbing || agent->humanoid.ragdoll.active ||
           agent->humanoid.recovering;
}

static Vector3 CourseThreatCenter(const CcLocalCourse *course)
{
    Vector3 center = {0};
    for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
        center.x += course->raiders[i].position.x;
        center.y += course->raiders[i].position.y;
        center.z += course->raiders[i].position.z;
    }
    center.x /= (float)CC_LOCAL_RAIDER_COUNT;
    center.y /= (float)CC_LOCAL_RAIDER_COUNT;
    center.z /= (float)CC_LOCAL_RAIDER_COUNT;
    return center;
}

static float CourseAlarmInterval(const CcSim *sim)
{
    int32_t influence = 0;
    if (sim != NULL) {
        for (int32_t i = 0; i < sim->bandit_count; ++i) {
            if (sim->bandits[i].influence > influence) {
                influence = sim->bandits[i].influence;
            }
        }
    }
    return fmaxf(9.0f, 22.0f - (float)influence * 0.16f);
}

static bool CourseRaiderResponseWaypoint(int32_t raider, int32_t stage,
                                         Vector3 *waypoint)
{
    static const Vector3 routes[CC_LOCAL_RAIDER_COUNT][3] = {
        {{72.00f, 0.0f, 38.60f}, {64.20f, 0.0f, 38.60f},
         {53.20f, 0.0f, 38.60f}},
        {{72.00f, 0.0f, 40.40f}, {64.20f, 0.0f, 40.40f},
         {53.20f, 0.0f, 40.40f}},
    };
    if (raider < 0 || raider >= CC_LOCAL_RAIDER_COUNT ||
        stage < 0 || stage >= 3) return false;
    *waypoint = routes[raider][stage];
    return true;
}

static bool CourseGuardIngressWaypoint(const CcLocalCourse *course,
                                       int32_t guard, int32_t stage,
                                       Vector3 *waypoint)
{
    if (course == NULL || waypoint == NULL || guard < 0 ||
        guard >= CC_LOCAL_COURSE_RUNNER_COUNT || stage < 0 || stage >= 4) {
        return false;
    }
    Vector3 entry = course->guard_entry[guard];
    float exit_x = entry.x < 11.80f ? 8.35f : 15.90f;
    float lane_z = 26.50f + (float)guard * 0.35f;
    if (stage == 0) {
        *waypoint = (Vector3){exit_x, 0.0f, entry.z};
    } else if (stage == 1) {
        *waypoint = (Vector3){exit_x, 0.0f, 12.80f};
    } else if (stage == 2) {
        *waypoint = (Vector3){16.20f + (float)guard * 0.50f, 0.0f,
                              lane_z};
    } else {
        *waypoint = (Vector3){42.80f, 0.0f, lane_z};
    }
    return true;
}

static void UpdateCourseTraining(CcLocalCourse *course, float delta_time)
{
    const int32_t waypoint_count = (int32_t)(sizeof(COURSE_WAYPOINTS) /
                                              sizeof(COURSE_WAYPOINTS[0]));
    for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
        CcLocalCourseRunner *runner = &course->runners[i];
        runner->duty = CC_GUARD_TRAINING;
        CcLocalAgentUpdate(&runner->agent, delta_time, false);
        if (!runner->agent.exact_target_valid &&
            !CourseAgentBusy(&runner->agent)) {
            runner->pause_seconds -= delta_time;
            if (runner->pause_seconds <= 0.0f &&
                CcLocalAgentSetExactTarget(
                    &runner->agent,
                    COURSE_WAYPOINTS[runner->next_waypoint], false)) {
                runner->next_waypoint =
                    (runner->next_waypoint + 1) % waypoint_count;
                runner->pause_seconds = 0.28f;
            }
        }
    }
}

static int32_t CourseTravellerDemand(const CcSim *sim)
{
    int32_t count = 2;
    if (sim == NULL) return count;
    for (int32_t i = 0; i < sim->shipment_count && count <
         CC_LOCAL_TRAVELLER_COUNT; ++i) {
        if (sim->shipments[i].status == CC_SHIPMENT_TRAVELLING) count += 1;
    }
    return count;
}

static void UpdateCourseTravellers(CcLocalCourse *course, const CcSim *sim,
                                   float delta_time)
{
    int32_t demand = CourseTravellerDemand(sim);
    for (int32_t i = 0; i < CC_LOCAL_TRAVELLER_COUNT; ++i) {
        CcLocalTraveller *traveller = &course->travellers[i];
        if (!traveller->active) {
            if (i >= demand) continue;
            traveller->respawn_delay -= delta_time;
            if (traveller->respawn_delay > 0.0f) continue;
            CcLocalAgentInit(&traveller->agent,
                             (Vector2){traveller->entry.x,
                                       traveller->entry.z}, false);
            traveller->agent.crowned = false;
            static const Color colors[CC_LOCAL_TRAVELLER_COUNT] = {
                {118, 134, 145, 255}, {150, 105, 91, 255},
                {73, 137, 121, 255}, {139, 102, 153, 255}
            };
            traveller->agent.tunic_color = colors[i];
            traveller->active = CcLocalAgentSetExactTarget(
                &traveller->agent, traveller->exit, false);
            continue;
        }
        CcLocalAgentUpdate(&traveller->agent, delta_time, false);
        if (traveller->agent.exact_target_valid) continue;
        float exit_x = traveller->agent.position.x - traveller->exit.x;
        float exit_z = traveller->agent.position.z - traveller->exit.z;
        if (exit_x * exit_x + exit_z * exit_z > 0.20f * 0.20f) continue;
        traveller->active = false;
        traveller->respawn_delay = 5.5f + (float)i * 1.7f;
    }
}

static int32_t CourseClosestRaider(const CcLocalCourse *course,
                                   const CcLocalAgent *agent,
                                   float maximum_distance,
                                   bool require_forward)
{
    int32_t closest = -1;
    float closest_distance = maximum_distance * maximum_distance;
    for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
        const CcLocalAgent *raider = &course->raiders[i];
        if (raider->combat.defeated) continue;
        float distance = CombatHorizontalDistanceSquared(agent, raider);
        if (distance >= closest_distance) continue;
        if (require_forward && distance > 1.15f * 1.15f &&
            CombatFacingDot(agent, raider->position) < -0.05f) continue;
        closest = i;
        closest_distance = distance;
    }
    return closest;
}

static CcLocalAgent *CourseClosestDefender(CcLocalCourse *course,
                                           CcLocalAgent *player,
                                           const CcLocalAgent *raider)
{
    CcLocalAgent *closest = NULL;
    float closest_distance = 2.35f * 2.35f;
    for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
        CcLocalAgent *guard = &course->runners[i].agent;
        if (guard->combat.defeated || CourseAgentBusy(guard)) continue;
        float distance = CombatHorizontalDistanceSquared(raider, guard);
        if (distance < closest_distance) {
            closest = guard;
            closest_distance = distance;
        }
    }
    if (player != NULL && !player->combat.defeated) {
        float distance = CombatHorizontalDistanceSquared(raider, player);
        if (distance < closest_distance) closest = player;
    }
    return closest;
}

static void CourseRecordOutcome(CcLocalCourse *course,
                                const CcLocalAgent *attacker,
                                CcCombatOutcome outcome)
{
    if (outcome == CC_COMBAT_OUTCOME_NONE) return;
    course->last_outcome = outcome;
    course->last_attacker_team = attacker->combat.team;
    course->combat_event_seconds = 0.72f;
}

static void CourseResolveImpact(CcLocalCourse *course,
                                CcLocalAgent *attacker,
                                CcLocalAgent *defender)
{
    if (!CcHumanoidGaitConsumeStrikeImpact(&attacker->humanoid)) return;
    CourseRecordOutcome(course, attacker,
                        CcLocalCombatResolveStrike(attacker, defender));
}

static void CourseRefreshRaiderResolve(CcLocalCourse *course)
{
    float total = 0.0f;
    for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
        total += course->raiders[i].combat.health;
    }
    float average = total / (float)CC_LOCAL_RAIDER_COUNT;
    course->raider_resolve = (int32_t)lroundf(CombatClamp(
        average, 0.0f, CC_LOCAL_COMBAT_MAX_HEALTH));
}

static bool CourseRaidersBroken(const CcLocalCourse *course)
{
    float total_health = 0.0f;
    int32_t standing = 0;
    for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
        total_health += course->raiders[i].combat.health;
        standing += course->raiders[i].combat.defeated ? 0 : 1;
    }
    return standing == 0 || total_health <= 60.0f;
}

static void CourseBeginRetreat(CcLocalCourse *course, CcLocalAgent *player)
{
    course->raiders_retreating = true;
    course->raider_resolve = 0;
    for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
        CcLocalCourseRunner *runner = &course->runners[i];
        runner->duty = CC_GUARD_RETURNING;
        CcLocalCombatSetGuarded(&runner->agent, NULL, false);
    }
    for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
        CcLocalAgent *raider = &course->raiders[i];
        raider->combat.defeated = false;
        raider->combat.stagger_seconds = 0.0f;
        raider->combat.hitstop_seconds = 0.0f;
        raider->combat.health = fmaxf(1.0f, raider->combat.health);
        CcLocalCombatSetGuarded(raider, NULL, false);
        CcLocalCombatClearFocus(raider);
        course->raider_response_stage[i] = 2;
        course->raider_response_waypoint_active[i] = false;
    }
    if (player != NULL) CcLocalCombatClearFocus(player);
}

bool CcLocalCourseBeginPlayerStrike(CcLocalCourse *course,
                                    CcLocalAgent *player)
{
    if (course == NULL || player == NULL) return false;
    CcLocalCombatSetTeam(player, CC_COMBAT_PLAYER);
    int32_t target = course->alarm_active && !course->raiders_retreating ?
        CourseClosestRaider(course, player, 2.75f, true) : -1;
    player->combat.target_index = target;
    if (target < 0) CcLocalCombatClearFocus(player);
    return CcLocalCombatBeginStrike(
        player, target >= 0 ? &course->raiders[target] : NULL);
}

void CcLocalCourseSetPlayerGuarded(CcLocalCourse *course,
                                   CcLocalAgent *player, bool guarded)
{
    if (course == NULL || player == NULL) return;
    CcLocalCombatSetTeam(player, CC_COMBAT_PLAYER);
    int32_t target = guarded && course->alarm_active &&
                     !course->raiders_retreating ?
        CourseClosestRaider(course, player, 3.10f, false) : -1;
    player->combat.target_index = target;
    CcLocalCombatSetGuarded(
        player, target >= 0 ? &course->raiders[target] : NULL, guarded);
}

void CcLocalCourseUpdate(CcLocalCourse *course, CcLocalAgent *player,
                         const CcSim *sim, float delta_time)
{
    delta_time = fminf(delta_time, 1.0f / 30.0f);
    UpdateCourseTravellers(course, sim, delta_time);
    course->combat_event_seconds = fmaxf(
        0.0f, course->combat_event_seconds - delta_time);
    if (player != NULL && player->combat.team == CC_COMBAT_NEUTRAL) {
        CcLocalCombatSetTeam(player, CC_COMBAT_PLAYER);
    }
    if (!course->alarm_active) {
        if (player != NULL) CourseResolveImpact(course, player, NULL);
        course->alarm_countdown -= delta_time;
        if (course->alarm_countdown <= 0.0f) {
            CcLocalCourseRaiseAlarmNear(course, player);
        } else {
            UpdateCourseTraining(course, delta_time);
            return;
        }
    }

    if (player != NULL && player->combat.target_index >= 0 &&
        player->combat.target_index < CC_LOCAL_RAIDER_COUNT &&
        !course->raiders[player->combat.target_index].combat.defeated) {
        CcLocalCombatSetFocus(
            player, &course->raiders[player->combat.target_index]);
    } else if (player != NULL &&
               player->humanoid.action != CC_HUMANOID_ACTION_STRIKE &&
               player->humanoid.action != CC_HUMANOID_ACTION_GUARD) {
        CcLocalCombatClearFocus(player);
    }

    if (course->raiders_retreating) {
        for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
            CcLocalCourseRunner *runner = &course->runners[i];
            runner->duty = CC_GUARD_RETURNING;
            CcLocalCombatSetGuarded(&runner->agent, NULL, false);
            (void)CcLocalAgentSetExactTarget(
                &runner->agent,
                (Vector3){course->combat_origin.x - 1.30f, 0.0f,
                          course->combat_origin.z +
                          ((float)i - 1.0f) * COURSE_GUARD_SPACING}, false);
            CcLocalAgentUpdate(&runner->agent, delta_time, false);
        }
        bool raiders_clear = true;
        for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
            CcLocalAgent *raider = &course->raiders[i];
            Vector3 retreat_waypoint;
            bool has_retreat_waypoint = CourseRaiderResponseWaypoint(
                i, course->raider_response_stage[i], &retreat_waypoint);
            if (has_retreat_waypoint &&
                !course->raider_response_waypoint_active[i]) {
                if (CcLocalAgentSetExactTarget(raider, retreat_waypoint,
                                               false)) {
                    course->raider_response_waypoint_active[i] = true;
                }
            } else if (has_retreat_waypoint &&
                       course->raider_response_waypoint_active[i] &&
                       !raider->exact_target_valid) {
                course->raider_response_stage[i] -= 1;
                course->raider_response_waypoint_active[i] = false;
            } else if (!has_retreat_waypoint) {
                (void)CcLocalAgentSetExactTarget(
                    raider, course->raider_entry[i], false);
            }
            CcLocalAgentUpdate(raider, delta_time, false);
            float entry_x = raider->position.x - course->raider_entry[i].x;
            float entry_z = raider->position.z - course->raider_entry[i].z;
            if (entry_x * entry_x + entry_z * entry_z > 0.18f * 0.18f) {
                raiders_clear = false;
            }
        }
        if (raiders_clear) {
            course->alarm_active = false;
            course->raiders_retreating = false;
            course->defenses_completed += 1;
            course->alarm_countdown = CourseAlarmInterval(sim);
            for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
                course->runners[i].duty = CC_GUARD_TRAINING;
                course->runners[i].pause_seconds = 0.18f + (float)i * 0.08f;
                course->runners[i].agent.exact_target_valid = false;
            }
        }
        return;
    }

    int32_t engaged_guards = 0;
    for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
        CcLocalCourseRunner *runner = &course->runners[i];
        CcLocalAgent *guard = &runner->agent;
        int32_t target_index = CourseClosestRaider(course, guard, 2.40f, false);
        CcLocalAgent *target = target_index >= 0 ?
                               &course->raiders[target_index] : NULL;
        bool engaged = target != NULL &&
            CombatHorizontalDistanceSquared(guard, target) < 1.42f * 1.42f;

        if (!guard->combat.defeated && !CourseAgentBusy(guard)) {
            Vector3 response_waypoint;
            bool has_response_waypoint = CourseGuardIngressWaypoint(
                course, i, runner->response_stage, &response_waypoint);
            if (has_response_waypoint &&
                !runner->response_waypoint_active) {
                if (CcLocalAgentSetExactTarget(guard, response_waypoint,
                                               false)) {
                    runner->response_waypoint_active = true;
                }
            } else if (has_response_waypoint &&
                       runner->response_waypoint_active &&
                       !guard->exact_target_valid) {
                runner->response_stage += 1;
                runner->response_waypoint_active = false;
            } else if (!has_response_waypoint && !engaged) {
                Vector3 anchor = target != NULL ? target->position :
                                 course->combat_origin;
                Vector3 guard_target = {
                    anchor.x - 0.68f, 0.0f,
                    anchor.z +
                    ((float)i - 1.0f) * COURSE_GUARD_SPACING
                };
                (void)CcLocalAgentSetExactTarget(guard, guard_target,
                                                 false);
            }
        }
        if (engaged && !guard->combat.defeated) {
            runner->duty = CC_GUARD_ENGAGED;
            guard->exact_target_valid = false;
            guard->combat.target_index = target_index;
            CcLocalCombatSetFocus(guard, target);
            CcLocalCombatSetGuarded(guard, target, true);
            runner->attack_cooldown -= delta_time;
            if (runner->attack_cooldown <= 0.0f &&
                CcLocalCombatBeginStrike(guard, target)) {
                runner->attack_cooldown = 0.88f + (float)i * 0.07f;
            }
            engaged_guards += 1;
        } else if (!guard->combat.defeated) {
            runner->duty = CC_GUARD_RESPONDING;
            CcLocalCombatSetGuarded(guard, target, false);
        }
        CcLocalAgentUpdate(guard, delta_time, false);
        target_index = guard->combat.target_index;
        target = target_index >= 0 && target_index < CC_LOCAL_RAIDER_COUNT ?
                 &course->raiders[target_index] : NULL;
        CourseResolveImpact(course, guard, target);
    }

    if (engaged_guards > 0) course->engagement_time += delta_time;
    for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
        CcLocalAgent *raider = &course->raiders[i];
        if (raider->combat.defeated) {
            CcLocalAgentUpdate(raider, delta_time, false);
            continue;
        }
        CcLocalAgent *target = CourseClosestDefender(course, player, raider);
        bool engaged = target != NULL &&
            CombatHorizontalDistanceSquared(raider, target) < 1.30f * 1.30f;
        if (!CourseAgentBusy(raider)) {
            Vector3 response_waypoint;
            bool has_response_waypoint = CourseRaiderResponseWaypoint(
                i, course->raider_response_stage[i], &response_waypoint);
            if (has_response_waypoint &&
                !course->raider_response_waypoint_active[i]) {
                if (CcLocalAgentSetExactTarget(raider, response_waypoint,
                                               false)) {
                    course->raider_response_waypoint_active[i] = true;
                }
            } else if (has_response_waypoint &&
                       course->raider_response_waypoint_active[i] &&
                       !raider->exact_target_valid) {
                course->raider_response_stage[i] += 1;
                course->raider_response_waypoint_active[i] = false;
            } else if (!has_response_waypoint) {
                Vector3 anchor = target != NULL ? target->position :
                                 course->combat_origin;
                Vector3 target_point = engaged ? raider->position :
                    (Vector3){anchor.x + 0.68f, 0.0f,
                              anchor.z + ((float)i - 0.5f) *
                              COURSE_RAIDER_SPACING};
                (void)CcLocalAgentSetExactTarget(raider, target_point, false);
            }
        }
        if (engaged) {
            raider->exact_target_valid = false;
            CcLocalCombatSetFocus(raider, target);
            CcLocalCombatSetGuarded(raider, target, true);
            course->raider_attack_cooldown[i] -= delta_time;
            if (course->raider_attack_cooldown[i] <= 0.0f &&
                CcLocalCombatBeginStrike(raider, target)) {
                course->raider_attack_cooldown[i] = 1.02f +
                                                    (float)i * 0.11f;
            }
        } else {
            CcLocalCombatSetGuarded(raider, target, false);
        }
        CcLocalAgentUpdate(raider, delta_time, false);
        CourseResolveImpact(course, raider, target);
    }

    if (player != NULL) {
        CcLocalAgent *target = NULL;
        if (player->combat.target_index >= 0 &&
            player->combat.target_index < CC_LOCAL_RAIDER_COUNT) {
            target = &course->raiders[player->combat.target_index];
            if (target->combat.defeated) target = NULL;
        }
        CourseResolveImpact(course, player, target);
    }
    CourseRefreshRaiderResolve(course);
    if (CourseRaidersBroken(course)) {
        CourseBeginRetreat(course, player);
    }
}

bool CcLocalAgentSetExactTarget(CcLocalAgent *agent, Vector3 target,
                                bool market_interior)
{
    if (StaticBodyBlocked(market_interior, target.x, target.z, agent->radius)) {
        return false;
    }
    target.y = SurfaceHeightAt(market_interior, target.x, target.z);
    agent->target_point = target;
    agent->target_valid = true;
    agent->exact_target_valid = true;
    return true;
}

static float RayFootprintDistance(Ray ray, Rectangle footprint, float height)
{
    BoundingBox box = {
        .min = {footprint.x, 0.0f, footprint.y},
        .max = {footprint.x + footprint.width, height,
                footprint.y + footprint.height}
    };
    RayCollision collision = GetRayCollisionBox(ray, box);
    return collision.hit ? collision.distance : FLT_MAX;
}

bool CcLocalAgentPickTarget(CcLocalAgent *agent, Vector2 screen_point,
                            RenderTexture2D target, Rectangle destination,
                            bool market_interior)
{
    if (!CheckCollisionPointRec(screen_point, destination)) return false;
    Vector2 local = {
        (screen_point.x - destination.x) / destination.width * (float)target.texture.width,
        (screen_point.y - destination.y) / destination.height * (float)target.texture.height
    };
    Camera3D camera = LocalCamera(market_interior, agent->position);
    Ray ray = GetScreenToWorldRayEx(local, camera, target.texture.width,
                                    target.texture.height);
    float nearest = FLT_MAX;
    Vector3 picked_point = {0};
    BoundingBox ground = {
        .min = {0.0f, -0.08f, 0.0f},
        .max = {market_interior ? 9.0f : CC_LOCAL_WORLD_WIDTH, 0.01f,
                market_interior ? 7.0f : CC_LOCAL_WORLD_DEPTH}
    };
    RayCollision collision = GetRayCollisionBox(ray, ground);
    if (collision.hit) {
        nearest = collision.distance;
        picked_point = collision.point;
    }
    if (!market_interior) {
        for (int32_t i = 0; i < (int32_t)(sizeof(STREET_PLATFORMS) /
                                          sizeof(STREET_PLATFORMS[0])); ++i) {
            const NavPlatform *platform = &STREET_PLATFORMS[i];
            BoundingBox box = {
                .min = {platform->x, -0.02f, platform->z},
                .max = {platform->x + platform->width,
                        platform->height + 0.02f,
                        platform->z + platform->depth}
            };
            collision = GetRayCollisionBox(ray, box);
            if (!collision.hit || collision.distance >= nearest) continue;
            nearest = collision.distance;
            picked_point = collision.point;
        }
    }
    if (nearest == FLT_MAX) return false;
    float occluder = FLT_MAX;
    if (market_interior) {
        occluder = fminf(occluder,
                         RayFootprintDistance(ray, MARKET_COUNTER_FOOTPRINT, 0.92f));
        occluder = fminf(occluder,
                         RayFootprintDistance(ray, MARKET_SHELF_FOOTPRINT, 1.90f));
        occluder = fminf(occluder, RayFootprintDistance(
            ray, (Rectangle){0.0f, 0.0f, 9.0f, 0.50f}, 2.60f));
        occluder = fminf(occluder, RayFootprintDistance(
            ray, (Rectangle){0.0f, 0.0f, 0.50f, 7.0f}, 2.60f));
    } else {
        for (int32_t i = 0; i < (int32_t)(sizeof(WORLD_BUILDINGS) /
                                          sizeof(WORLD_BUILDINGS[0])); ++i) {
            occluder = fminf(occluder,
                             RayFootprintDistance(
                                 ray, WORLD_BUILDINGS[i].footprint,
                                 WORLD_BUILDINGS[i].height));
        }
        for (int32_t i = 0; i < (int32_t)(sizeof(CASTLE_STRUCTURES) /
                                          sizeof(CASTLE_STRUCTURES[0])); ++i) {
            occluder = fminf(occluder,
                             RayFootprintDistance(
                                 ray, CASTLE_STRUCTURES[i].footprint,
                                 CASTLE_STRUCTURES[i].height));
        }
        occluder = fminf(occluder,
                         RayFootprintDistance(ray, CARRIAGE_FOOTPRINT, 1.92f));
        occluder = fminf(occluder,
                         RayFootprintDistance(ray, DUNGEON_FOOTPRINT, 2.45f));
    }
    if (occluder < nearest) return false;
    return CcLocalAgentSetExactTarget(agent, picked_point, market_interior);
}

static float WrapAngle(float angle)
{
    while (angle > PI) angle -= 2.0f * PI;
    while (angle < -PI) angle += 2.0f * PI;
    return angle;
}

static float Approach(float current, float target, float maximum_change)
{
    if (current < target) return fminf(target, current + maximum_change);
    return fmaxf(target, current - maximum_change);
}

static float SmoothStep01(float amount)
{
    amount = fmaxf(0.0f, fminf(amount, 1.0f));
    return amount * amount * (3.0f - 2.0f * amount);
}

static Vector3 PhysicsAdd(Vector3 a, Vector3 b)
{
    return (Vector3){a.x + b.x, a.y + b.y, a.z + b.z};
}

static Vector3 PhysicsSubtract(Vector3 a, Vector3 b)
{
    return (Vector3){a.x - b.x, a.y - b.y, a.z - b.z};
}

static Vector3 PhysicsScale(Vector3 value, float scale)
{
    return (Vector3){value.x * scale, value.y * scale, value.z * scale};
}

static float PhysicsDot(Vector3 a, Vector3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static float PhysicsLength(Vector3 value)
{
    return sqrtf(PhysicsDot(value, value));
}

static Vector3 PhysicsCross(Vector3 a, Vector3 b)
{
    return (Vector3){a.y * b.z - a.z * b.y,
                     a.z * b.x - a.x * b.z,
                     a.x * b.y - a.y * b.x};
}

static Vector3 PhysicsNormalizeOr(Vector3 value, Vector3 fallback)
{
    float length = PhysicsLength(value);
    return length > 0.0001f ? PhysicsScale(value, 1.0f / length) : fallback;
}

static Vector3 PhysicsLerp(Vector3 a, Vector3 b, float amount)
{
    return PhysicsAdd(a, PhysicsScale(PhysicsSubtract(b, a), amount));
}

static void ResetHeroCape(CcLocalAgent *agent,
                          const CcHumanoidSkinPose *skin,
                          Vector3 anchor)
{
    static const float SEGMENT_LENGTHS[] = {0.270f, 0.275f, 0.285f, 0.285f};
    CcLocalCapeState *cape = &agent->cape;
    Vector3 up = FromLimbVector(skin->body_up);
    Vector3 backward = PhysicsScale(FromLimbVector(skin->body_forward), -1.0f);
    Vector3 rest_direction = PhysicsNormalizeOr(
        PhysicsAdd(PhysicsScale(up, -0.975f),
                   PhysicsScale(backward, 0.22f)),
        (Vector3){0.0f, -1.0f, 0.0f});
    cape->point[0] = anchor;
    cape->previous[0] = anchor;
    for (int32_t point = 1; point < CC_LOCAL_CAPE_POINT_COUNT; ++point) {
        cape->point[point] = PhysicsAdd(
            cape->point[point - 1],
            PhysicsScale(rest_direction, SEGMENT_LENGTHS[point - 1]));
        cape->previous[point] = cape->point[point];
    }
    cape->anchor = anchor;
    cape->initialized = true;
}

static void UpdateHeroCape(CcLocalAgent *agent, float delta_time)
{
    if (agent == NULL || agent->morphology != CC_MORPHOLOGY_BIPED ||
        !agent->humanoid.initialized) return;
    static const float SEGMENT_LENGTHS[] = {0.270f, 0.275f, 0.285f, 0.285f};
    CcHumanoidSkinPose skin;
    CcHumanoidSkinPoseResolve(&agent->humanoid.pose, &skin);
    if (!skin.valid) return;
    Vector3 up = FromLimbVector(skin.body_up);
    Vector3 forward = FromLimbVector(skin.body_forward);
    Vector3 anchor = PhysicsAdd(
        FromLimbVector(skin.sockets[CC_HUMANOID_SOCKET_BACK].position),
        PhysicsScale(up, 0.14f));
    CcLocalCapeState *cape = &agent->cape;
    if (!cape->initialized ||
        PhysicsLength(PhysicsSubtract(anchor, cape->anchor)) > 0.72f) {
        ResetHeroCape(agent, &skin, anchor);
        return;
    }

    float step = fmaxf(1.0f / 240.0f, fminf(delta_time, 1.0f / 30.0f));
    float damping = expf(-3.2f * step);
    Vector3 acceleration = {
        -agent->velocity.x * 2.15f,
        agent->swimming ? 0.45f : -7.25f,
        -agent->velocity.z * 2.15f,
    };
    acceleration = PhysicsAdd(
        acceleration,
        PhysicsScale(forward, -0.30f * PhysicsLength(agent->velocity)));
    for (int32_t point = 1; point < CC_LOCAL_CAPE_POINT_COUNT; ++point) {
        Vector3 current = cape->point[point];
        Vector3 velocity = PhysicsScale(
            PhysicsSubtract(current, cape->previous[point]), damping);
        cape->previous[point] = current;
        cape->point[point] = PhysicsAdd(
            PhysicsAdd(current, velocity),
            PhysicsScale(acceleration, step * step));
    }
    cape->point[0] = anchor;
    cape->previous[0] = anchor;

    Vector3 chest = FromLimbVector(skin.bones[CC_HUMANOID_SKIN_CHEST].head);
    Vector3 pelvis = FromLimbVector(skin.bones[CC_HUMANOID_SKIN_PELVIS].head);
    for (int32_t iteration = 0; iteration < 9; ++iteration) {
        cape->point[0] = anchor;
        for (int32_t point = 1; point < CC_LOCAL_CAPE_POINT_COUNT; ++point) {
            Vector3 delta = PhysicsSubtract(cape->point[point],
                                            cape->point[point - 1]);
            float distance = PhysicsLength(delta);
            if (distance <= 0.0001f) continue;
            Vector3 correction = PhysicsScale(
                delta, (distance - SEGMENT_LENGTHS[point - 1]) / distance);
            if (point == 1) {
                cape->point[point] = PhysicsSubtract(cape->point[point],
                                                     correction);
            } else {
                cape->point[point - 1] = PhysicsAdd(
                    cape->point[point - 1], PhysicsScale(correction, 0.5f));
                cape->point[point] = PhysicsSubtract(
                    cape->point[point], PhysicsScale(correction, 0.5f));
            }
        }
        for (int32_t point = 2; point < CC_LOCAL_CAPE_POINT_COUNT; ++point) {
            float rest_span = (SEGMENT_LENGTHS[point - 2] +
                               SEGMENT_LENGTHS[point - 1]) * 0.985f;
            Vector3 delta = PhysicsSubtract(cape->point[point],
                                            cape->point[point - 2]);
            float distance = PhysicsLength(delta);
            if (distance <= rest_span || distance <= 0.0001f) continue;
            Vector3 correction = PhysicsScale(
                delta, (distance - rest_span) / distance * 0.16f);
            if (point == 2) {
                cape->point[point] = PhysicsSubtract(cape->point[point],
                                                     correction);
            } else {
                cape->point[point - 2] = PhysicsAdd(
                    cape->point[point - 2], PhysicsScale(correction, 0.5f));
                cape->point[point] = PhysicsSubtract(
                    cape->point[point], PhysicsScale(correction, 0.5f));
            }
        }
        for (int32_t point = 1; point < CC_LOCAL_CAPE_POINT_COUNT; ++point) {
            float amount = (float)point /
                           (float)(CC_LOCAL_CAPE_POINT_COUNT - 1);
            Vector3 torso = PhysicsLerp(chest, pelvis, amount * 0.86f);
            float front_distance = PhysicsDot(
                PhysicsSubtract(cape->point[point], torso), forward);
            float rear_limit = -0.115f - amount * 0.035f;
            if (front_distance > rear_limit) {
                cape->point[point] = PhysicsAdd(
                    cape->point[point],
                    PhysicsScale(forward, rear_limit - front_distance));
            }
        }
    }
    cape->point[0] = anchor;
    cape->anchor = anchor;
}

static Vector3 FramePoint(Vector3 base, float x, float y, float z, float yaw)
{
    float cosine = cosf(yaw);
    float sine = sinf(yaw);
    return (Vector3){base.x + x * cosine + z * sine,
                     base.y + y,
                     base.z - x * sine + z * cosine};
}

static bool BeginClimb(CcLocalAgent *agent, const NavPlatform *platform)
{
    if (agent->morphology == CC_MORPHOLOGY_BIPED &&
        (!agent->humanoid.initialized || agent->humanoid.ragdoll.active ||
         agent->humanoid.recovering || agent->humanoid.climbing)) {
        return false;
    }
    float left = fabsf(agent->position.x - platform->x);
    float right_edge = fabsf(agent->position.x -
                             (platform->x + platform->width));
    float near_edge = fabsf(agent->position.z - platform->z);
    float far_edge = fabsf(agent->position.z -
                           (platform->z + platform->depth));
    float nearest = left;
    Vector3 normal = {-1.0f, 0.0f, 0.0f};
    Vector3 face = {platform->x, platform->height, agent->position.z};
    if (right_edge < nearest) {
        nearest = right_edge;
        normal = (Vector3){1.0f, 0.0f, 0.0f};
        face = (Vector3){platform->x + platform->width, platform->height,
                         agent->position.z};
    }
    if (near_edge < nearest) {
        nearest = near_edge;
        normal = (Vector3){0.0f, 0.0f, -1.0f};
        face = (Vector3){agent->position.x, platform->height,
                         platform->z};
    }
    if (far_edge < nearest) {
        normal = (Vector3){0.0f, 0.0f, 1.0f};
        face = (Vector3){agent->position.x, platform->height,
                         platform->z + platform->depth};
    }
    if (normal.x != 0.0f) {
        face.z = fmaxf(platform->z + 0.24f,
                       fminf(face.z,
                             platform->z + platform->depth - 0.24f));
    } else {
        face.x = fmaxf(platform->x + 0.24f,
                       fminf(face.x,
                             platform->x + platform->width - 0.24f));
    }
    Vector3 forward = PhysicsScale(normal, -1.0f);
    float facing = atan2f(forward.x, forward.z);
    Vector3 frame_right = {cosf(facing), 0.0f, -sinf(facing)};
    Vector3 body = {agent->position.x,
                    agent->position.y + agent->limb_rig.morphology.body_height,
                    agent->position.z};
    Vector3 shoulder_left = FramePoint(body, -0.25f, 0.48f, 0.0f, facing);
    Vector3 shoulder_right = FramePoint(body, 0.25f, 0.48f, 0.0f, facing);
    Vector3 hand_left = PhysicsAdd(PhysicsAdd(face, PhysicsScale(normal, 0.025f)),
                                  PhysicsScale(frame_right, -0.16f));
    Vector3 hand_right = PhysicsAdd(PhysicsAdd(face, PhysicsScale(normal, 0.025f)),
                                   PhysicsScale(frame_right, 0.16f));
    hand_left.y += 0.035f;
    hand_right.y += 0.035f;
    const float arm_reach = 0.69f;
    if (PhysicsLength(PhysicsSubtract(hand_left, shoulder_left)) > arm_reach ||
        PhysicsLength(PhysicsSubtract(hand_right, shoulder_right)) > arm_reach) {
        return false;
    }

    agent->climb_start = agent->position;
    agent->climb_end = PhysicsAdd(face,
                                  PhysicsScale(normal, -(agent->radius + 0.18f)));
    agent->climb_end.y = platform->height;
    if (StaticBodyBlocked(false, agent->climb_end.x, agent->climb_end.z,
                          agent->radius)) return false;
    agent->climb_face = face;
    agent->climb_normal = normal;
    agent->climb_hand_left = hand_left;
    agent->climb_hand_right = hand_right;
    agent->facing_yaw = facing;
    float rise = agent->climb_end.y - agent->climb_start.y;
    agent->climb_duration = 1.28f + rise * 0.16f;
    agent->climb_progress = 0.0f;
    agent->climb_settle = 0.0f;
    agent->climbing = true;
    agent->climbing_down = false;
    agent->grounded = true;
    agent->velocity = (Vector3){0};
    agent->traversal = CC_TRAVERSAL_CLIMB;
    if (agent->morphology == CC_MORPHOLOGY_BIPED) {
        CcHumanoidGaitBeginClimb(&agent->humanoid);
        agent->humanoid_needs_reset = false;
    }
    return true;
}

static bool BeginDownClimb(CcLocalAgent *agent,
                           const NavPlatform *platform,
                           bool move_x, float amount)
{
    if (!agent->allow_downclimb ||
        agent->morphology != CC_MORPHOLOGY_BIPED ||
        !agent->humanoid.initialized || agent->humanoid.ragdoll.active ||
        agent->humanoid.recovering || agent->humanoid.climbing) {
        return false;
    }
    Vector3 normal = {0.0f, 0.0f, 0.0f};
    Vector3 face = {agent->position.x, platform->height, agent->position.z};
    if (move_x) {
        normal.x = amount < 0.0f ? -1.0f : 1.0f;
        face.x = amount < 0.0f ? platform->x :
                                 platform->x + platform->width;
        face.z = fmaxf(platform->z + 0.24f,
                       fminf(face.z,
                             platform->z + platform->depth - 0.24f));
    } else {
        normal.z = amount < 0.0f ? -1.0f : 1.0f;
        face.z = amount < 0.0f ? platform->z :
                                 platform->z + platform->depth;
        face.x = fmaxf(platform->x + 0.24f,
                       fminf(face.x,
                             platform->x + platform->width - 0.24f));
    }
    Vector3 descent_end = PhysicsAdd(
        face, PhysicsScale(normal, agent->radius + 0.42f));
    descent_end.y = BodySurfaceHeightAt(false, descent_end.x, descent_end.z);
    if (descent_end.y >= platform->height - 0.24f ||
        StaticBodyBlocked(false, descent_end.x, descent_end.z,
                          agent->radius)) {
        return false;
    }
    float facing = atan2f(-normal.x, -normal.z);
    Vector3 frame_right = {cosf(facing), 0.0f, -sinf(facing)};
    Vector3 hand_center = PhysicsAdd(face, PhysicsScale(normal, 0.018f));
    hand_center.y = platform->height + 0.035f;

    agent->climb_start = agent->position;
    agent->climb_end = descent_end;
    agent->climb_face = face;
    agent->climb_normal = normal;
    agent->climb_hand_left = PhysicsAdd(
        hand_center, PhysicsScale(frame_right, -0.16f));
    agent->climb_hand_right = PhysicsAdd(
        hand_center, PhysicsScale(frame_right, 0.16f));
    agent->climb_start_yaw = agent->facing_yaw;
    agent->climb_end_yaw = facing;
    agent->climb_duration = 1.42f +
        (platform->height - descent_end.y) * 0.18f;
    agent->climb_progress = 0.0f;
    agent->climb_settle = 0.0f;
    agent->climbing = true;
    agent->climbing_down = true;
    agent->grounded = true;
    agent->velocity = (Vector3){0};
    agent->traversal = CC_TRAVERSAL_DESCEND;
    CcHumanoidGaitBeginClimb(&agent->humanoid);
    agent->humanoid_needs_reset = false;
    return true;
}

static void UpdateDownClimb(CcLocalAgent *agent, float delta_time,
                            bool market_interior)
{
    agent->climb_progress = fminf(1.0f, agent->climb_progress +
                                  delta_time / agent->climb_duration);
    float amount = agent->climb_progress;
    float turn = SmoothStep01(amount / 0.20f);
    float yaw_delta = WrapAngle(agent->climb_end_yaw -
                                agent->climb_start_yaw);
    agent->facing_yaw = WrapAngle(agent->climb_start_yaw + yaw_delta * turn);

    Vector3 outside_top = PhysicsAdd(
        agent->climb_face,
        PhysicsScale(agent->climb_normal, agent->radius + 0.09f));
    outside_top.y = agent->climb_face.y - 0.10f;
    Vector3 hang = outside_top;
    hang.y = fmaxf(agent->climb_end.y + 0.34f,
                   agent->climb_face.y - 1.08f);
    float edge_transfer = SmoothStep01((amount - 0.02f) / 0.22f);
    float lower_transfer = SmoothStep01((amount - 0.08f) / 0.30f);
    float land_transfer = SmoothStep01((amount - 0.64f) / 0.28f);
    Vector3 target = PhysicsLerp(agent->climb_start, outside_top,
                                 edge_transfer);
    target = PhysicsLerp(target, hang, lower_transfer);
    target = PhysicsLerp(target, agent->climb_end, land_transfer);

    Vector3 acceleration = PhysicsSubtract(
        PhysicsScale(PhysicsSubtract(target, agent->position), 31.0f),
        PhysicsScale(agent->velocity, 9.5f));
    agent->velocity = PhysicsAdd(agent->velocity,
                                 PhysicsScale(acceleration, delta_time));
    float speed = PhysicsLength(agent->velocity);
    if (speed > 2.7f) {
        agent->velocity = PhysicsScale(agent->velocity, 2.7f / speed);
    }
    agent->position = PhysicsAdd(agent->position,
                                 PhysicsScale(agent->velocity, delta_time));
    agent->grounded = amount < 0.10f;

    float outside_distance = PhysicsDot(
        PhysicsSubtract(agent->position, agent->climb_face),
        agent->climb_normal);
    float clearance_weight = SmoothStep01((amount - 0.18f) / 0.24f);
    float required_outside = (agent->radius + 0.035f) * clearance_weight;
    if (amount < 0.86f && outside_distance < required_outside) {
        agent->position = PhysicsAdd(
            agent->position,
            PhysicsScale(agent->climb_normal,
                         required_outside - outside_distance));
        float into_face = PhysicsDot(agent->velocity, agent->climb_normal);
        if (into_face < 0.0f) {
            agent->velocity = PhysicsSubtract(
                agent->velocity,
                PhysicsScale(agent->climb_normal, into_face));
        }
    }
    if (agent->position.y < agent->climb_end.y) {
        agent->position.y = agent->climb_end.y;
        if (agent->velocity.y < 0.0f) agent->velocity.y = 0.0f;
    }

    float end_distance = PhysicsLength(
        PhysicsSubtract(agent->position, agent->climb_end));
    if (amount >= 1.0f && end_distance < 0.020f) {
        agent->position = agent->climb_end;
        agent->velocity = (Vector3){0};
        agent->grounded = true;
        agent->climb_settle = fminf(1.0f, agent->climb_settle +
                                    delta_time / 0.64f);
    }

    LocalProbeContext context = {.market_interior = market_interior};
    Vector3 frame_right = {cosf(agent->facing_yaw), 0.0f,
                           -sinf(agent->facing_yaw)};
    Vector3 up = {0.0f, 1.0f, 0.0f};
    Vector3 top_left = PhysicsAdd(
        PhysicsAdd(agent->climb_face,
                   PhysicsScale(agent->climb_normal, -0.18f)),
        PhysicsScale(frame_right, -0.13f));
    Vector3 top_right = PhysicsAdd(
        PhysicsAdd(agent->climb_face,
                   PhysicsScale(agent->climb_normal, -0.18f)),
        PhysicsScale(frame_right, 0.13f));
    top_left.y = agent->climb_face.y + 0.035f;
    top_right.y = agent->climb_face.y + 0.035f;
    float wall_height = agent->climb_face.y - 0.32f -
                        SmoothStep01(amount / 0.72f) * 0.48f;
    Vector3 wall_left = PhysicsAdd(
        PhysicsAdd(agent->climb_face,
                   PhysicsScale(agent->climb_normal, 0.018f)),
        PhysicsScale(frame_right, -0.13f));
    Vector3 wall_right = PhysicsAdd(
        PhysicsAdd(agent->climb_face,
                   PhysicsScale(agent->climb_normal, 0.018f)),
        PhysicsScale(frame_right, 0.13f));
    wall_left.y = fmaxf(agent->climb_end.y + 0.14f, wall_height);
    wall_right.y = fmaxf(agent->climb_end.y + 0.21f, wall_height + 0.07f);
    Vector3 ground_left = PhysicsAdd(
        agent->climb_end, PhysicsScale(frame_right, -0.13f));
    Vector3 ground_right = PhysicsAdd(
        agent->climb_end, PhysicsScale(frame_right, 0.13f));
    ground_left.y = agent->climb_end.y + 0.035f;
    ground_right.y = agent->climb_end.y + 0.035f;
    float wall_transfer = SmoothStep01((amount - 0.12f) / 0.30f);
    float ground_transfer = SmoothStep01((amount - 0.58f) / 0.28f);
    Vector3 foot_targets[CC_HUMANOID_LEG_COUNT] = {
        PhysicsLerp(PhysicsLerp(top_left, wall_left, wall_transfer),
                    ground_left, ground_transfer),
        PhysicsLerp(PhysicsLerp(top_right, wall_right, wall_transfer),
                    ground_right, ground_transfer)
    };
    Vector3 wall_normal_left = PhysicsNormalizeOr(
        PhysicsLerp(up, agent->climb_normal, wall_transfer), up);
    Vector3 wall_normal_right = wall_normal_left;
    Vector3 foot_normals[CC_HUMANOID_LEG_COUNT] = {
        PhysicsNormalizeOr(
            PhysicsLerp(wall_normal_left, up, ground_transfer), up),
        PhysicsNormalizeOr(
            PhysicsLerp(wall_normal_right, up, ground_transfer), up)
    };
    CcLimbVec3 hands[CC_HUMANOID_ARM_COUNT] = {
        ToLimbVector(agent->climb_hand_left),
        ToLimbVector(agent->climb_hand_right)
    };
    CcLimbVec3 feet[CC_HUMANOID_LEG_COUNT] = {
        ToLimbVector(foot_targets[0]), ToLimbVector(foot_targets[1])
    };
    CcLimbVec3 normals[CC_HUMANOID_LEG_COUNT] = {
        ToLimbVector(foot_normals[0]), ToLimbVector(foot_normals[1])
    };
    float standing_convergence = SmoothStep01(agent->climb_settle);
    float biomech_progress = amount < 0.78f ? amount :
        0.78f + 0.22f * standing_convergence;
    CcHumanoidGaitAdvanceClimb(
        &agent->humanoid, ToLimbVector(agent->position),
        agent->facing_yaw, hands, feet, normals, biomech_progress,
        delta_time, ProbeLocalSurface, &context);

    agent->traversal = CC_TRAVERSAL_DESCEND;
    if (agent->climb_settle >= 1.0f &&
        CcHumanoidGaitClimbReady(
            &agent->humanoid, ToLimbVector(agent->position),
            agent->facing_yaw, ProbeLocalSurface, &context, 0.025f)) {
        agent->climbing = false;
        agent->climbing_down = false;
        agent->grounded = true;
        agent->traversal = CC_TRAVERSAL_IDLE;
        CcHumanoidGaitFinishClimb(
            &agent->humanoid, ToLimbVector(agent->position),
            agent->facing_yaw, ProbeLocalSurface, &context);
    }
}

static void UpdateClimb(CcLocalAgent *agent, float delta_time,
                        bool market_interior)
{
    if (agent->climbing_down) {
        UpdateDownClimb(agent, delta_time, market_interior);
        return;
    }
    agent->climb_progress = fminf(1.0f, agent->climb_progress +
                                  delta_time / agent->climb_duration);
    float amount = agent->climb_progress;
    float pull = SmoothStep01((amount - 0.18f) / 0.42f);
    float high_step = SmoothStep01((amount - 0.58f) / 0.18f);
    float stand = SmoothStep01((amount - 0.74f) / 0.26f);
    Vector3 outside = PhysicsAdd(agent->climb_face,
                                 PhysicsScale(agent->climb_normal,
                                              agent->radius + 0.08f));
    Vector3 hang = outside;
    hang.y = agent->climb_face.y - 1.08f;
    Vector3 high = PhysicsLerp(hang, agent->climb_end, 0.34f);
    high.y = agent->climb_face.y - 0.62f;
    Vector3 target = agent->climb_start;
    if (amount < 0.18f) {
        agent->position = agent->climb_start;
        agent->velocity = (Vector3){0};
        agent->grounded = true;
    } else {
        target = PhysicsLerp(agent->climb_start, hang, pull);
        target = PhysicsLerp(target, high, high_step);
        target = PhysicsLerp(target, agent->climb_end, stand);
        Vector3 acceleration = PhysicsSubtract(
            PhysicsScale(PhysicsSubtract(target, agent->position), 34.0f),
            PhysicsScale(agent->velocity, 9.0f));
        agent->velocity = PhysicsAdd(agent->velocity,
                                     PhysicsScale(acceleration, delta_time));
        float speed = PhysicsLength(agent->velocity);
        if (speed > 3.0f) agent->velocity = PhysicsScale(agent->velocity, 3.0f / speed);
        agent->position = PhysicsAdd(agent->position,
                                     PhysicsScale(agent->velocity, delta_time));
        agent->grounded = false;
    }

    if (amount >= 0.18f && amount < 0.78f) {
        Vector3 body = {agent->position.x,
                        agent->position.y + agent->limb_rig.morphology.body_height,
                        agent->position.z};
        Vector3 shoulders[2] = {
            FramePoint(body, -0.25f, 0.48f, 0.0f, agent->facing_yaw),
            FramePoint(body, 0.25f, 0.48f, 0.0f, agent->facing_yaw)
        };
        Vector3 hands[2] = {agent->climb_hand_left, agent->climb_hand_right};
        float desired_reach = 0.67f - pull * 0.15f;
        Vector3 correction = {0};
        for (int32_t hand = 0; hand < 2; ++hand) {
            Vector3 toward = PhysicsSubtract(hands[hand], shoulders[hand]);
            float distance = PhysicsLength(toward);
            if (distance <= desired_reach || distance <= 0.0001f) continue;
            correction = PhysicsAdd(correction,
                                    PhysicsScale(toward,
                                                 (distance - desired_reach) /
                                                 distance * 0.32f));
        }
        agent->position = PhysicsAdd(agent->position, PhysicsScale(correction, 0.5f));
    }

    float outside_distance = PhysicsDot(
        PhysicsSubtract(agent->position, agent->climb_face), agent->climb_normal);
    float acquisition_clearance =
        (1.0f - SmoothStep01(amount / 0.28f)) * 0.110f;
    float required_outside = agent->radius + 0.035f + acquisition_clearance;
    if ((amount < 0.72f || agent->position.y < agent->climb_face.y - 0.12f) &&
        outside_distance < required_outside) {
        agent->position = PhysicsAdd(
            agent->position,
            PhysicsScale(agent->climb_normal, required_outside - outside_distance));
        float into_face = PhysicsDot(agent->velocity, agent->climb_normal);
        if (into_face < 0.0f) {
            agent->velocity = PhysicsSubtract(
                agent->velocity, PhysicsScale(agent->climb_normal, into_face));
        }
    }
    if (agent->position.y > agent->climb_end.y) {
        agent->position.y = agent->climb_end.y;
        if (agent->velocity.y > 0.0f) agent->velocity.y = 0.0f;
    }
    agent->traversal = CC_TRAVERSAL_CLIMB;
    float end_distance = PhysicsLength(
        PhysicsSubtract(agent->position, agent->climb_end));
    if (amount >= 1.0f && end_distance < 0.020f) {
        agent->position = agent->climb_end;
        agent->velocity = (Vector3){0};
        agent->grounded = true;
        agent->climb_settle = fminf(1.0f, agent->climb_settle +
                                    delta_time / 0.70f);
    }
    LocalProbeContext context = {.market_interior = market_interior};
    if (agent->morphology == CC_MORPHOLOGY_BIPED) {
        Vector3 frame_right = {cosf(agent->facing_yaw), 0.0f,
                               -sinf(agent->facing_yaw)};
        float wall_height = agent->position.y + 0.10f;
        Vector3 wall_left = PhysicsAdd(
            PhysicsAdd(agent->climb_face,
                       PhysicsScale(agent->climb_normal, 0.018f)),
            PhysicsScale(frame_right, -0.13f));
        Vector3 wall_right = PhysicsAdd(
            PhysicsAdd(agent->climb_face,
                       PhysicsScale(agent->climb_normal, 0.018f)),
            PhysicsScale(frame_right, 0.13f));
        wall_left.y = wall_height;
        wall_right.y = wall_height + 0.07f;
        Vector3 top_left = PhysicsAdd(
            PhysicsAdd(agent->climb_face,
                       PhysicsScale(agent->climb_normal, -0.20f)),
            PhysicsScale(frame_right, -0.13f));
        Vector3 top_right = PhysicsAdd(
            PhysicsAdd(agent->climb_face,
                       PhysicsScale(agent->climb_normal, -0.20f)),
            PhysicsScale(frame_right, 0.13f));
        top_left.y = agent->climb_face.y + 0.035f;
        top_right.y = agent->climb_face.y + 0.035f;
        float left_transfer = SmoothStep01((amount - 0.28f) / 0.40f);
        float right_transfer = SmoothStep01((amount - 0.34f) / 0.40f);
        Vector3 foot_targets[CC_HUMANOID_LEG_COUNT] = {
            PhysicsLerp(wall_left, top_left, left_transfer),
            PhysicsLerp(wall_right, top_right, right_transfer)
        };
        Vector3 up = {0.0f, 1.0f, 0.0f};
        Vector3 foot_normals[CC_HUMANOID_LEG_COUNT] = {
            PhysicsNormalizeOr(
                PhysicsLerp(agent->climb_normal, up, left_transfer), up),
            PhysicsNormalizeOr(
                PhysicsLerp(agent->climb_normal, up, right_transfer), up)
        };
        CcLimbVec3 hand_targets[CC_HUMANOID_ARM_COUNT] = {
            ToLimbVector(agent->climb_hand_left),
            ToLimbVector(agent->climb_hand_right)
        };
        CcLimbVec3 feet[CC_HUMANOID_LEG_COUNT] = {
            ToLimbVector(foot_targets[0]), ToLimbVector(foot_targets[1])
        };
        CcLimbVec3 normals[CC_HUMANOID_LEG_COUNT] = {
            ToLimbVector(foot_normals[0]), ToLimbVector(foot_normals[1])
        };
        float standing_convergence = SmoothStep01(agent->climb_settle);
        float biomech_progress = amount < 0.78f ? amount :
            0.78f + 0.22f * standing_convergence;
        CcHumanoidGaitAdvanceClimb(
            &agent->humanoid, ToLimbVector(agent->position),
            agent->facing_yaw, hand_targets, feet, normals,
            biomech_progress, delta_time, ProbeLocalSurface, &context);
        if (agent->climb_settle >= 1.0f &&
            CcHumanoidGaitClimbReady(
                &agent->humanoid, ToLimbVector(agent->position),
                agent->facing_yaw, ProbeLocalSurface, &context, 0.025f)) {
            agent->climbing = false;
            agent->grounded = true;
            agent->traversal = CC_TRAVERSAL_IDLE;
        }
        if (!agent->climbing) {
            CcHumanoidGaitFinishClimb(
                &agent->humanoid, ToLimbVector(agent->position),
                agent->facing_yaw, ProbeLocalSurface, &context);
        }
        return;
    }
    if (agent->climb_settle >= 1.0f) {
        agent->climbing = false;
        agent->grounded = true;
        agent->traversal = CC_TRAVERSAL_IDLE;
    }
    CcLimbRigUpdate(&agent->limb_rig, ToLimbVector(ShellPodBaseCenter(agent)),
                    agent->facing_yaw, ToLimbVector(agent->velocity),
                    agent->grounded, delta_time, ProbeLocalSurface, &context);
    if (agent->climbing && amount >= 0.18f) {
        Vector3 frame_right = {cosf(agent->facing_yaw), 0.0f,
                               -sinf(agent->facing_yaw)};
        float wall_height = agent->climb_start.y + 0.16f + pull * 0.40f;
        Vector3 wall_left = PhysicsAdd(
            PhysicsAdd(agent->climb_face, PhysicsScale(agent->climb_normal, 0.018f)),
            PhysicsScale(frame_right, -0.13f));
        Vector3 wall_right = PhysicsAdd(
            PhysicsAdd(agent->climb_face, PhysicsScale(agent->climb_normal, 0.018f)),
            PhysicsScale(frame_right, 0.13f));
        wall_left.y = wall_height;
        wall_right.y = wall_height + 0.07f;
        Vector3 top_left = PhysicsAdd(
            PhysicsAdd(agent->climb_face, PhysicsScale(agent->climb_normal, -0.20f)),
            PhysicsScale(frame_right, -0.13f));
        Vector3 top_right = PhysicsAdd(
            PhysicsAdd(agent->climb_face, PhysicsScale(agent->climb_normal, -0.20f)),
            PhysicsScale(frame_right, 0.13f));
        top_left.y = agent->climb_face.y + 0.035f;
        top_right.y = agent->climb_face.y + 0.035f;
        float plant = SmoothStep01((amount - 0.18f) / 0.16f);
        float left_transfer = SmoothStep01((amount - 0.56f) / 0.18f);
        float right_transfer = SmoothStep01((amount - 0.70f) / 0.18f);
        Vector3 left_contact = PhysicsLerp(wall_left, top_left, left_transfer);
        Vector3 right_contact = PhysicsLerp(wall_right, top_right,
                                            right_transfer);
        Vector3 up = {0.0f, 1.0f, 0.0f};
        Vector3 left_normal = PhysicsNormalizeOr(
            PhysicsLerp(agent->climb_normal, up, left_transfer), up);
        Vector3 right_normal = PhysicsNormalizeOr(
            PhysicsLerp(agent->climb_normal, up, right_transfer), up);
        const CcLimbSpec *left_spec = &agent->limb_rig.morphology.limbs[0];
        const CcLimbSpec *right_spec = &agent->limb_rig.morphology.limbs[1];
        Vector3 current_left = FromLimbVector(
            agent->limb_rig.limbs[0].joints[left_spec->segment_count]);
        Vector3 current_right = FromLimbVector(
            agent->limb_rig.limbs[1].joints[right_spec->segment_count]);
        left_contact = PhysicsLerp(current_left, left_contact, plant);
        right_contact = PhysicsLerp(current_right, right_contact, plant);
        left_normal = PhysicsNormalizeOr(
            PhysicsLerp(FromLimbVector(agent->limb_rig.limbs[0].contact_normal),
                        left_normal, plant), up);
        right_normal = PhysicsNormalizeOr(
            PhysicsLerp(FromLimbVector(agent->limb_rig.limbs[1].contact_normal),
                        right_normal, plant), up);
        CcLimbVec3 root = ToLimbVector(ShellPodBaseCenter(agent));
        CcLimbRigPinContact(&agent->limb_rig, 0, root, agent->facing_yaw,
                            ToLimbVector(left_contact), ToLimbVector(left_normal));
        CcLimbRigPinContact(&agent->limb_rig, 1, root, agent->facing_yaw,
                            ToLimbVector(right_contact), ToLimbVector(right_normal));
    }
}

static bool TryHorizontalAxis(CcLocalAgent *agent, bool market_interior,
                              bool move_x, float amount)
{
    if (agent->climbing) return false;
    if (fabsf(amount) <= 0.000001f) return true;
    float candidate_x = agent->position.x + (move_x ? amount : 0.0f);
    float candidate_z = agent->position.z + (move_x ? 0.0f : amount);
    if (StaticBodyBlocked(market_interior, candidate_x, candidate_z, agent->radius)) {
        return false;
    }
    if (!market_interior) {
        const NavPlatform *support = SupportingPlatformAt(
            agent->position.x, agent->position.z, agent->position.y);
        float candidate_surface = BodySurfaceHeightAt(
            false, candidate_x, candidate_z);
        bool moving_toward_target = agent->exact_target_valid &&
            amount * ((move_x ? agent->target_point.x : agent->target_point.z) -
                      (move_x ? agent->position.x : agent->position.z)) > 0.0f;
        bool target_requests_descent = agent->exact_target_valid &&
            agent->target_point.y < agent->position.y - 0.24f;
        if (support != NULL && agent->grounded && moving_toward_target &&
            target_requests_descent &&
            candidate_surface < agent->position.y - 0.24f &&
            BeginDownClimb(agent, support, move_x, amount)) {
            return false;
        }
        const NavPlatform *platform = ClimbPlatformAt(candidate_x, candidate_z,
                                                      agent->radius,
                                                      agent->position.y);
        if (platform != NULL) {
            bool target_requests_climb = !agent->exact_target_valid ||
                agent->target_point.y >= platform->height - 0.10f;
            if (agent->grounded && target_requests_climb) {
                if (BeginClimb(agent, platform)) return false;
            }
            if (agent->grounded) {
                Rectangle footprint = {platform->x, platform->z,
                                       platform->width, platform->depth};
                float current_distance = FootprintDistanceSquared(
                    agent->position.x, agent->position.z, footprint);
                float candidate_distance = FootprintDistanceSquared(
                    candidate_x, candidate_z, footprint);
                if (candidate_distance <= current_distance + 0.000001f) {
                    return false;
                }
            }
        }
    }
    if (move_x) agent->position.x = candidate_x;
    else agent->position.z = candidate_z;
    return true;
}

static bool UpdateCombatClock(CcLocalAgent *agent, float delta_time)
{
    CcCombatState *combat = &agent->combat;
    combat->hit_flash_seconds = fmaxf(0.0f,
                                      combat->hit_flash_seconds - delta_time);
    if (combat->hitstop_seconds > 0.0f && agent->grounded &&
        !agent->climbing && !agent->swimming) {
        combat->hitstop_seconds = fmaxf(0.0f,
                                        combat->hitstop_seconds - delta_time);
        return true;
    }
    combat->hitstop_seconds = fmaxf(0.0f,
                                    combat->hitstop_seconds - delta_time);
    combat->stagger_seconds = fmaxf(0.0f,
                                    combat->stagger_seconds - delta_time);
    if (combat->defeated && combat->team == CC_COMBAT_PLAYER) {
        combat->recovery_seconds = fmaxf(0.0f,
                                         combat->recovery_seconds - delta_time);
        if (combat->recovery_seconds <= 0.0f) {
            combat->defeated = false;
            combat->health = 45.0f;
            combat->posture = 72.0f;
            combat->stagger_seconds = 0.42f;
            combat->hit_flash_seconds = 0.22f;
        }
    }
    if (!combat->defeated && combat->stagger_seconds <= 0.0f) {
        float regeneration = agent->humanoid.action ==
                             CC_HUMANOID_ACTION_GUARD ? 4.0f : 15.0f;
        combat->posture = fminf(CC_LOCAL_COMBAT_MAX_POSTURE,
                                combat->posture + regeneration * delta_time);
    }
    return false;
}

static void CcLocalAgentPhysicsStep(CcLocalAgent *agent, float delta_time,
                                    bool market_interior)
{
    const float gravity = 9.81f;
    delta_time = fminf(delta_time, 1.0f / 30.0f);
    if (UpdateCombatClock(agent, delta_time)) {
        UpdateHeroCape(agent, delta_time);
        return;
    }
    if (agent->climbing) {
        UpdateClimb(agent, delta_time, market_interior);
        UpdateHeroCape(agent, delta_time);
        return;
    }
    bool biped = agent->morphology == CC_MORPHOLOGY_BIPED;
    LocalProbeContext context = {.market_interior = market_interior};
    if (biped && agent->humanoid_needs_reset) {
        CcHumanoidGaitInit(&agent->humanoid, ToLimbVector(agent->position),
                            agent->facing_yaw, ProbeLocalSurface, &context);
        agent->humanoid_needs_reset = false;
    }
    bool was_swimming = agent->swimming;
    bool in_water = biped && !market_interior &&
                    CourseWaterContains(agent->position.x, agent->position.z);
    if (was_swimming && !in_water) {
        float surface = BodySurfaceHeightAt(market_interior, agent->position.x,
                                            agent->position.z);
        agent->position.y = surface;
        agent->velocity.y = 0.0f;
        agent->grounded = true;
        CcHumanoidGaitEndSwim(&agent->humanoid,
                              ToLimbVector(agent->position),
                              agent->facing_yaw, ProbeLocalSurface, &context);
    }
    agent->swimming = in_water;
    agent->immersion = Approach(agent->immersion, in_water ? 1.0f : 0.0f,
                                delta_time * 2.8f);
    float traction = agent->limb_rig.initialized ? agent->limb_rig.traction : 1.0f;
    float base_speed = in_water ? 0.76f : biped ? 1.45f : 2.35f;
    float maximum_speed = biped ? base_speed :
                          base_speed * (0.68f + traction * 0.32f);
    if (biped && agent->humanoid.action == CC_HUMANOID_ACTION_GUARD) {
        maximum_speed *= 0.54f;
    } else if (biped &&
               agent->humanoid.action == CC_HUMANOID_ACTION_STRIKE) {
        maximum_speed *= 0.16f;
    }
    if (agent->combat.defeated || agent->combat.stagger_seconds > 0.0f) {
        maximum_speed = 0.0f;
    }
    float acceleration = 10.5f * traction;
    Vector3 direction = {0};
    float target_distance = 0.0f;
    if (agent->exact_target_valid) {
        direction.x = agent->target_point.x - agent->position.x;
        direction.z = agent->target_point.z - agent->position.z;
        target_distance = sqrtf(direction.x * direction.x + direction.z * direction.z);
        float physical_speed = sqrtf(agent->velocity.x * agent->velocity.x +
                                     agent->velocity.z * agent->velocity.z);
        bool gait_settled = !biped ||
                            (!agent->humanoid.ragdoll.active &&
                             agent->humanoid.speed.value < 0.03f &&
                             physical_speed < 0.02f);
        if (target_distance < 0.025f && gait_settled &&
            fabsf(agent->position.y - agent->target_point.y) < 0.12f) {
            agent->exact_target_valid = false;
            target_distance = 0.0f;
        }
    }
    float desired_x = 0.0f;
    float desired_z = 0.0f;
    if (target_distance > 0.001f) {
        float desired_speed = fminf(maximum_speed, target_distance * 3.2f);
        desired_x = direction.x / target_distance * desired_speed;
        desired_z = direction.z / target_distance * desired_speed;
    }
    if (biped && agent->humanoid.action == CC_HUMANOID_ACTION_STRIKE) {
        desired_x = 0.0f;
        desired_z = 0.0f;
        if (agent->combat.focus_valid && agent->humanoid.action_time >= 0.14f &&
            agent->humanoid.action_time <= 0.57f) {
            float focus_x = agent->combat.focus_point.x - agent->position.x;
            float focus_z = agent->combat.focus_point.z - agent->position.z;
            float focus_distance = sqrtf(focus_x * focus_x + focus_z * focus_z);
            if (focus_distance > 0.62f) {
                desired_x = focus_x / focus_distance * 0.68f;
                desired_z = focus_z / focus_distance * 0.68f;
            }
        }
    }
    if (agent->combat.defeated) {
        desired_x = 0.0f;
        desired_z = 0.0f;
    } else if (agent->combat.stagger_seconds > 0.0f) {
        desired_x = agent->combat.knockback_velocity.x;
        desired_z = agent->combat.knockback_velocity.z;
    } else {
        desired_x += agent->combat.knockback_velocity.x;
        desired_z += agent->combat.knockback_velocity.z;
    }
    if (!(biped && agent->humanoid.ragdoll.active)) {
        float face_x = direction.x;
        float face_z = direction.z;
        bool should_face = target_distance > 0.001f;
        if (agent->combat.focus_valid) {
            face_x = agent->combat.focus_point.x - agent->position.x;
            face_z = agent->combat.focus_point.z - agent->position.z;
            should_face = face_x * face_x + face_z * face_z > 0.0001f;
        }
        if (should_face) {
            float target_yaw = atan2f(face_x, face_z);
            float difference = WrapAngle(target_yaw - agent->facing_yaw);
            agent->facing_yaw = WrapAngle(
                agent->facing_yaw +
                difference * fminf(1.0f, delta_time * 12.0f));
        }
    }
    if (biped && in_water) {
        CcHumanoidGaitAdvanceSwim(
            &agent->humanoid, ToLimbVector(agent->position),
            agent->facing_yaw,
            (CcLimbVec3){desired_x, 0.0f, desired_z},
            COURSE_WATER_SURFACE, agent->immersion, delta_time);
        agent->velocity.x = agent->humanoid.root_velocity.x;
        agent->velocity.z = agent->humanoid.root_velocity.z;
    } else if (biped) {
        CcHumanoidGaitAdvance(&agent->humanoid, ToLimbVector(agent->position),
                              agent->facing_yaw,
                              (CcLimbVec3){desired_x, 0.0f, desired_z},
                              agent->grounded, delta_time, ProbeLocalSurface,
                              &context);
        agent->velocity.x = agent->humanoid.root_velocity.x;
        agent->velocity.z = agent->humanoid.root_velocity.z;
    } else {
        agent->velocity.x = Approach(agent->velocity.x, desired_x,
                                     acceleration * delta_time);
        agent->velocity.z = Approach(agent->velocity.z, desired_z,
                                     acceleration * delta_time);
        float support_weight = fminf(1.0f, target_distance / 0.45f);
        agent->velocity.x += agent->limb_rig.body_acceleration.x * delta_time *
                             support_weight;
        agent->velocity.z += agent->limb_rig.body_acceleration.z * delta_time *
                             support_weight;
    }

    Vector3 previous_position = agent->position;
    bool moved_x = TryHorizontalAxis(agent, market_interior, true,
                                     agent->velocity.x * delta_time);
    bool moved_z = TryHorizontalAxis(agent, market_interior, false,
                                     agent->velocity.z * delta_time);
    if (!moved_x) agent->velocity.x = 0.0f;
    if (!moved_z) agent->velocity.z = 0.0f;
    if (agent->climbing) {
        UpdateClimb(agent, delta_time, market_interior);
        UpdateHeroCape(agent, delta_time);
        return;
    }
    if (target_distance > 0.001f && (!moved_x || !moved_z)) {
        Vector3 forward = {direction.x / target_distance, 0.0f,
                           direction.z / target_distance};
        Vector3 side = {-forward.z, 0.0f, forward.x};
        float sidestep = maximum_speed * 0.72f * delta_time;
        bool side_x = TryHorizontalAxis(agent, market_interior, true,
                                        side.x * sidestep);
        bool side_z = TryHorizontalAxis(agent, market_interior, false,
                                        side.z * sidestep);
        if ((!side_x || !side_z) && !agent->climbing) {
            if (side_x) agent->position.x -= side.x * sidestep;
            if (side_z) agent->position.z -= side.z * sidestep;
            (void)TryHorizontalAxis(agent, market_interior, true,
                                    -side.x * sidestep);
            (void)TryHorizontalAxis(agent, market_interior, false,
                                    -side.z * sidestep);
        }
        if (agent->climbing) {
            UpdateClimb(agent, delta_time, market_interior);
            UpdateHeroCape(agent, delta_time);
            return;
        }
    }
    agent->velocity.x = (agent->position.x - previous_position.x) / delta_time;
    agent->velocity.z = (agent->position.z - previous_position.z) / delta_time;
    float knockback_decay = expf(-7.5f * delta_time);
    agent->combat.knockback_velocity.x *= knockback_decay;
    agent->combat.knockback_velocity.z *= knockback_decay;

    bool landed = false;
    bool occupies_water = biped && !market_interior &&
                          CourseWaterContains(agent->position.x,
                                              agent->position.z);
    if (occupies_water) {
        agent->swimming = true;
        agent->immersion = Approach(agent->immersion, 1.0f,
                                    delta_time * 2.8f);
        if (!in_water) {
            CcHumanoidGaitAdvanceSwim(
                &agent->humanoid, ToLimbVector(agent->position),
                agent->facing_yaw,
                (CcLimbVec3){agent->velocity.x, 0.0f, agent->velocity.z},
                COURSE_WATER_SURFACE, agent->immersion, delta_time);
            agent->velocity.x = agent->humanoid.root_velocity.x;
            agent->velocity.z = agent->humanoid.root_velocity.z;
        }
        float float_base = COURSE_WATER_SURFACE - 0.82f;
        agent->position.y = Approach(agent->position.y, float_base,
                                     delta_time * 1.8f);
        agent->velocity.y = Approach(agent->velocity.y, 0.0f,
                                     delta_time * 4.0f);
        agent->grounded = false;
    } else {
        if (agent->swimming) {
            agent->swimming = false;
            CcHumanoidGaitEndSwim(&agent->humanoid,
                                  ToLimbVector(agent->position),
                                  agent->facing_yaw, ProbeLocalSurface,
                                  &context);
        }
        agent->velocity.y -= gravity * delta_time;
        agent->position.y += agent->velocity.y * delta_time;
        float surface = BodySurfaceHeightAt(market_interior,
                                            agent->position.x,
                                            agent->position.z);
        if (agent->position.y <= surface && agent->velocity.y <= 0.0f) {
            landed = !agent->grounded;
            agent->position.y = surface;
            agent->velocity.y = 0.0f;
            agent->grounded = true;
        } else {
            agent->grounded = false;
        }
    }
    if (biped && !agent->swimming) {
        CcHumanoidGaitConstrainMotion(&agent->humanoid,
                                      ToLimbVector(agent->position),
                                      ToLimbVector(agent->velocity),
                                      agent->grounded);
    }

    float horizontal_speed = sqrtf(agent->velocity.x * agent->velocity.x +
                                   agent->velocity.z * agent->velocity.z);
    if (agent->swimming) {
        agent->traversal = CC_TRAVERSAL_SWIM;
    } else if (biped && agent->humanoid.ragdoll.active) {
        agent->traversal = agent->humanoid.recovering ?
                           CC_TRAVERSAL_GET_UP : CC_TRAVERSAL_RAGDOLL;
    } else if (biped && agent->humanoid.action == CC_HUMANOID_ACTION_JUMP) {
        agent->traversal = CC_TRAVERSAL_JUMP;
    } else if (agent->grounded) {
        agent->traversal = horizontal_speed > 0.08f ? CC_TRAVERSAL_WALK :
                                                     CC_TRAVERSAL_IDLE;
    } else {
        agent->traversal = CC_TRAVERSAL_DROP;
    }
    if (landed && horizontal_speed < 0.08f &&
        !(biped && agent->humanoid.ragdoll.active)) {
        agent->traversal = CC_TRAVERSAL_IDLE;
    }
    if (biped) {
        CcHumanoidGaitResolvePose(&agent->humanoid,
                                  ToLimbVector(agent->position),
                                  agent->facing_yaw);
        float ragdoll_target = agent->humanoid.ragdoll.active ? 1.0f : 0.0f;
        agent->ragdoll_visual_blend = Approach(
            agent->ragdoll_visual_blend, ragdoll_target,
            delta_time * 4.5f);
        UpdateHeroCape(agent, delta_time);
    } else {
        CcLimbRigUpdate(&agent->limb_rig, ToLimbVector(ShellPodBaseCenter(agent)),
                        agent->facing_yaw, ToLimbVector(agent->velocity),
                        agent->grounded, delta_time, ProbeLocalSurface, &context);
        agent->cape = (CcLocalCapeState){0};
    }
}

static CcLimbVec3 BlendLimbPoint(CcLimbVec3 before, CcLimbVec3 after,
                                 float amount)
{
    return (CcLimbVec3){
        before.x + (after.x - before.x) * amount,
        before.y + (after.y - before.y) * amount,
        before.z + (after.z - before.z) * amount,
    };
}

static float BlendPoseAngle(float before, float after, float amount)
{
    return WrapAngle(before + WrapAngle(after - before) * amount);
}

static void BlendHumanoidPose(CcHumanoidPose *result,
                              const CcHumanoidPose *before,
                              const CcHumanoidPose *after, float amount)
{
#define BLEND_POSE_POINT(point) \
    result->point = BlendLimbPoint(before->point, after->point, amount)
    BLEND_POSE_POINT(pelvis);
    BLEND_POSE_POINT(spine);
    BLEND_POSE_POINT(chest);
    BLEND_POSE_POINT(neck);
    BLEND_POSE_POINT(head);
    for (int32_t leg = 0; leg < CC_HUMANOID_LEG_COUNT; ++leg) {
        result->hip[leg] = BlendLimbPoint(before->hip[leg], after->hip[leg],
                                          amount);
        result->knee[leg] = BlendLimbPoint(before->knee[leg],
                                           after->knee[leg], amount);
        result->ankle[leg] = BlendLimbPoint(before->ankle[leg],
                                            after->ankle[leg], amount);
        result->heel[leg] = BlendLimbPoint(before->heel[leg],
                                           after->heel[leg], amount);
        result->ball[leg] = BlendLimbPoint(before->ball[leg],
                                           after->ball[leg], amount);
        result->toe[leg] = BlendLimbPoint(before->toe[leg], after->toe[leg],
                                          amount);
        result->foot_pitch[leg] = BlendPoseAngle(before->foot_pitch[leg],
                                                 after->foot_pitch[leg],
                                                 amount);
        result->knee_flexion[leg] =
            before->knee_flexion[leg] +
            (after->knee_flexion[leg] - before->knee_flexion[leg]) * amount;
    }
    for (int32_t arm = 0; arm < CC_HUMANOID_ARM_COUNT; ++arm) {
        result->shoulder[arm] = BlendLimbPoint(before->shoulder[arm],
                                               after->shoulder[arm], amount);
        result->elbow[arm] = BlendLimbPoint(before->elbow[arm],
                                            after->elbow[arm], amount);
        result->hand[arm] = BlendLimbPoint(before->hand[arm], after->hand[arm],
                                           amount);
    }
#undef BLEND_POSE_POINT
    result->pelvis_yaw = BlendPoseAngle(before->pelvis_yaw,
                                        after->pelvis_yaw, amount);
    result->pelvis_roll = BlendPoseAngle(before->pelvis_roll,
                                         after->pelvis_roll, amount);
    result->pelvis_pitch = BlendPoseAngle(before->pelvis_pitch,
                                          after->pelvis_pitch, amount);
    result->chest_yaw = BlendPoseAngle(before->chest_yaw,
                                       after->chest_yaw, amount);
    result->chest_roll = BlendPoseAngle(before->chest_roll,
                                        after->chest_roll, amount);
    result->chest_pitch = BlendPoseAngle(before->chest_pitch,
                                         after->chest_pitch, amount);
}

static float LocalAgentPhysicsInterval(const CcLocalAgent *agent)
{
    (void)agent;
    return 1.0f / 60.0f;
}

void CcLocalAgentUpdate(CcLocalAgent *agent, float delta_time,
                        bool market_interior)
{
    /* Locomotion and contact solve at a stable rate even when rendering does
       not. The displayed skeleton interpolates between those physical states,
       retaining planted contacts without exposing fixed-step stair-stepping. */
    const float minimum_step = 1.0f / 60.0f;
    const int32_t maximum_steps = 6;
    float frame_time = fmaxf(0.0f, fminf(delta_time, 0.10f));
    agent->simulation_accumulator = fminf(
        agent->simulation_accumulator + frame_time,
        minimum_step * (float)maximum_steps);
    int32_t steps = 0;
    float physics_interval = LocalAgentPhysicsInterval(agent);
    while (agent->simulation_accumulator + 0.0000001f >= physics_interval &&
           steps < maximum_steps) {
        CcLocalAgentPhysicsStep(agent, physics_interval, market_interior);
        agent->simulation_accumulator -= physics_interval;
        if (agent->simulation_accumulator < 0.0f) {
            agent->simulation_accumulator = 0.0f;
        }
        steps += 1;
        physics_interval = LocalAgentPhysicsInterval(agent);
    }

    if (agent->morphology == CC_MORPHOLOGY_BIPED &&
        agent->humanoid.initialized) {
        float amount = fmaxf(0.0f, fminf(
            agent->simulation_accumulator / physics_interval, 1.0f));
        CcHumanoidPose sampled_pose;
        BlendHumanoidPose(&sampled_pose, &agent->humanoid.previous_pose,
                          &agent->humanoid.pose, amount);
        if (agent->render_pose_valid &&
            (agent->traversal == CC_TRAVERSAL_WALK ||
             agent->traversal == CC_TRAVERSAL_IDLE)) {
            CcHumanoidPose prior_render_pose = agent->render_pose;
            /* Let the torso and limbs retain a little inertial continuity
               without filtering the contact points copied below. */
            float response = 1.0f - expf(-38.0f * frame_time);
            BlendHumanoidPose(&agent->render_pose, &prior_render_pose,
                              &sampled_pose, response);
            for (int32_t leg = 0; leg < CC_HUMANOID_LEG_COUNT; ++leg) {
                CcHumanoidContact contact = agent->humanoid.feet[leg].contact;
                if (contact == CC_HUMANOID_CONTACT_SWING ||
                    contact == CC_HUMANOID_CONTACT_AIR) continue;
                agent->render_pose.ankle[leg] = sampled_pose.ankle[leg];
                agent->render_pose.heel[leg] = sampled_pose.heel[leg];
                agent->render_pose.ball[leg] = sampled_pose.ball[leg];
                agent->render_pose.toe[leg] = sampled_pose.toe[leg];
                agent->render_pose.foot_pitch[leg] =
                    sampled_pose.foot_pitch[leg];
            }
        } else {
            agent->render_pose = sampled_pose;
        }
        agent->render_pose_valid = true;
    } else {
        agent->render_pose_valid = false;
    }
}

static const CcHumanoidPose *AgentRenderPose(const CcLocalAgent *agent)
{
    return agent->render_pose_valid ? &agent->render_pose :
                                      &agent->humanoid.pose;
}

static Camera3D LocalCamera(bool interior, Vector3 focus)
{
    Camera3D camera = {0};
    if (interior) {
        camera.target = (Vector3){4.55f, 0.72f, 3.40f};
    } else {
        camera.target = (Vector3){
            fmaxf(8.0f, fminf(focus.x,
                               CC_LOCAL_WORLD_WIDTH - 8.0f)),
            focus.y + 0.95f,
            fmaxf(7.0f, fminf(focus.z,
                               CC_LOCAL_WORLD_DEPTH - 7.0f))
        };
    }
    float camera_distance = interior ? 10.0f : 18.0f;
    camera.position = (Vector3){camera.target.x + camera_distance,
                                camera.target.y +
                                    (interior ? camera_distance : 30.0f),
                                camera.target.z + camera_distance};
    camera.up = (Vector3){0.0f, 1.0f, 0.0f};
    camera.fovy = interior ? 10.5f : 17.0f;
    camera.projection = CAMERA_ORTHOGRAPHIC;
    return camera;
}

static Color KingdomColor3D(const CcSim *sim, CcId id)
{
    for (int32_t i = 0; i < sim->kingdom_count; ++i) {
        if (sim->kingdoms[i].id != id) continue;
        return (Color){sim->kingdoms[i].color_r, sim->kingdoms[i].color_g,
                       sim->kingdoms[i].color_b, 255};
    }
    return WORLD_MUTED;
}

static const CcDungeon *DungeonAt(const CcSim *sim, CcId settlement_id)
{
    for (int32_t i = 0; i < sim->dungeon_count; ++i) {
        if (sim->dungeons[i].settlement_id == settlement_id) return &sim->dungeons[i];
    }
    return NULL;
}

static bool HasSmugglerRoad(const CcSim *sim, CcId settlement_id)
{
    for (int32_t i = 0; i < sim->route_count; ++i) {
        const CcRoute *route = &sim->routes[i];
        if (!route->smuggler_route) continue;
        if (route->from_id == settlement_id || route->to_id == settlement_id) return true;
    }
    return false;
}

typedef struct SphereModelCache {
    Model small;
    Model character;
    Model scenery;
    bool ready;
} SphereModelCache;

static SphereModelCache sphere_models = {0};

#define CC_HERO_SKIN_MAX_BONES 64
#define CC_HERO_CAPE_BONE_COUNT 4
#define CC_HERO_SKIN_ASSET \
    "assets/exports/hero/crownless_hero_engine_rig_v01.glb"

typedef struct HeroSkinCache {
    Model model;
    ModelAnimation animation;
    Transform pose[CC_HERO_SKIN_MAX_BONES];
    Transform *frames[1];
    int32_t skin_bone[CC_HERO_SKIN_MAX_BONES];
    int32_t cape_bone[CC_HERO_SKIN_MAX_BONES];
    bool ready;
} HeroSkinCache;

static HeroSkinCache hero_skin = {0};

static const Vector3 HERO_REST_DIRECTIONS[CC_HUMANOID_SKIN_BONE_COUNT] = {
    {0.0f, 1.0f, 0.0f},
    {0.0f, 1.0f, 0.0f},
    {0.0f, 1.0f, 0.0f},
    {0.0f, 1.0f, 0.0f},
    {0.0f, 1.0f, 0.0f},
    {0.0f, 1.0f, 0.0f},
    {-0.469f, -0.883f, 0.0f},
    {-0.325f, -0.945f, 0.044f},
    {-0.061f, -0.979f, 0.184f},
    {0.469f, -0.883f, 0.0f},
    {0.325f, -0.945f, 0.044f},
    {0.061f, -0.979f, 0.184f},
    {-0.023f, -1.0f, 0.0f},
    {0.0f, -1.0f, 0.0f},
    {0.0f, -0.182f, 0.983f},
    {0.023f, -1.0f, 0.0f},
    {0.0f, -1.0f, 0.0f},
    {0.0f, -0.182f, 0.983f},
};

static const char *HERO_CAPE_BONE_NAMES[CC_HERO_CAPE_BONE_COUNT] = {
    "cape.0", "cape.1", "cape.2", "cape.3",
};

static const Vector3 HERO_CAPE_REST_DIRECTIONS[CC_HERO_CAPE_BONE_COUNT] = {
    {0.0f, -0.989f, -0.146f},
    {0.0f, -0.986f, -0.164f},
    {0.0f, -0.984f, -0.176f},
    {0.0f, -0.984f, -0.176f},
};

static int32_t HeroCapeBoneFind(const char *name)
{
    for (int32_t bone = 0; bone < CC_HERO_CAPE_BONE_COUNT; ++bone) {
        if (strcmp(name, HERO_CAPE_BONE_NAMES[bone]) == 0) return bone;
    }
    return -1;
}

static Quaternion HeroRotationBetween(Vector3 from, Vector3 to)
{
    from = Vector3Normalize(from);
    to = Vector3Normalize(to);
    float cosine = Vector3DotProduct(from, to);
    if (cosine > -0.9995f) return QuaternionFromVector3ToVector3(from, to);
    Vector3 axis = Vector3CrossProduct(from, (Vector3){1.0f, 0.0f, 0.0f});
    if (Vector3LengthSqr(axis) < 0.0001f) {
        axis = Vector3CrossProduct(from, (Vector3){0.0f, 0.0f, 1.0f});
    }
    return QuaternionFromAxisAngle(Vector3Normalize(axis), PI);
}

static const char *HeroSkinAssetPath(void)
{
    if (FileExists(CC_HERO_SKIN_ASSET)) return CC_HERO_SKIN_ASSET;
    static char bundled_path[1024];
    (void)snprintf(bundled_path, sizeof(bundled_path),
                   "%s/../Resources/%s", GetApplicationDirectory(),
                   CC_HERO_SKIN_ASSET);
    return FileExists(bundled_path) ? bundled_path : NULL;
}

static void LoadHeroSkin(void)
{
    const char *asset_path = HeroSkinAssetPath();
    if (asset_path == NULL) {
        TraceLog(LOG_WARNING, "HERO: engine skin was not found");
        return;
    }
    hero_skin.model = LoadModel(asset_path);
    int32_t bone_count = hero_skin.model.skeleton.boneCount;
    if (hero_skin.model.meshCount <= 0 || bone_count <= 0 ||
        bone_count > CC_HERO_SKIN_MAX_BONES) {
        TraceLog(LOG_WARNING, "HERO: invalid engine skin (%d meshes, %d bones)",
                 hero_skin.model.meshCount, bone_count);
        UnloadModel(hero_skin.model);
        hero_skin = (HeroSkinCache){0};
        return;
    }
    bool found[CC_HUMANOID_SKIN_BONE_COUNT] = {false};
    bool found_cape[CC_HERO_CAPE_BONE_COUNT] = {false};
    for (int32_t bone = 0; bone < bone_count; ++bone) {
        int32_t skin_bone = CcHumanoidSkinBoneFind(
            hero_skin.model.skeleton.bones[bone].name);
        int32_t cape_bone = HeroCapeBoneFind(
            hero_skin.model.skeleton.bones[bone].name);
        hero_skin.skin_bone[bone] = skin_bone;
        hero_skin.cape_bone[bone] = cape_bone;
        if (skin_bone >= 0) found[skin_bone] = true;
        if (cape_bone >= 0) found_cape[cape_bone] = true;
    }
    for (int32_t bone = 0; bone < CC_HUMANOID_SKIN_BONE_COUNT; ++bone) {
        if (found[bone]) continue;
        TraceLog(LOG_WARNING, "HERO: engine skin is missing bone %s",
                 CcHumanoidSkinBoneName((CcHumanoidSkinBone)bone));
        UnloadModel(hero_skin.model);
        hero_skin = (HeroSkinCache){0};
        return;
    }
    for (int32_t bone = 0; bone < CC_HERO_CAPE_BONE_COUNT; ++bone) {
        if (found_cape[bone]) continue;
        TraceLog(LOG_WARNING, "HERO: engine skin is missing cloth bone %s",
                 HERO_CAPE_BONE_NAMES[bone]);
        UnloadModel(hero_skin.model);
        hero_skin = (HeroSkinCache){0};
        return;
    }
    hero_skin.frames[0] = hero_skin.pose;
    hero_skin.animation.boneCount = bone_count;
    hero_skin.animation.keyframeCount = 1;
    hero_skin.animation.keyframePoses = hero_skin.frames;
    (void)snprintf(hero_skin.animation.name, sizeof(hero_skin.animation.name),
                   "engine-physics");
    hero_skin.ready = true;
    TraceLog(LOG_INFO, "HERO: loaded %d modular meshes on %d physics bones",
             hero_skin.model.meshCount, bone_count);
}

static bool DrawHeroSkin(const CcHumanoidSkinPose *skin,
                         const CcLocalCapeState *cape)
{
    if (!hero_skin.ready || skin == NULL || !skin->valid || cape == NULL ||
        !cape->initialized) return false;
    for (int32_t bone = 0; bone < hero_skin.model.skeleton.boneCount; ++bone) {
        int32_t skin_bone = hero_skin.skin_bone[bone];
        int32_t cape_bone = hero_skin.cape_bone[bone];
        Vector3 target_head = {0};
        Vector3 target_direction = {0};
        Vector3 rest_direction = {0};
        if (skin_bone >= 0 && skin_bone < CC_HUMANOID_SKIN_BONE_COUNT) {
            const CcHumanoidSkinBonePose *target = &skin->bones[skin_bone];
            target_head = FromLimbVector(target->head);
            target_direction = FromLimbVector(target->up);
            rest_direction = HERO_REST_DIRECTIONS[skin_bone];
        } else if (cape_bone >= 0 && cape_bone < CC_HERO_CAPE_BONE_COUNT) {
            target_head = cape->point[cape_bone];
            target_direction = PhysicsNormalizeOr(
                PhysicsSubtract(cape->point[cape_bone + 1],
                                cape->point[cape_bone]),
                (Vector3){0.0f, -1.0f, 0.0f});
            rest_direction = HERO_CAPE_REST_DIRECTIONS[cape_bone];
        } else {
            hero_skin.pose[bone] = hero_skin.model.skeleton.bindPose[bone];
            continue;
        }
        Quaternion delta = HeroRotationBetween(
            rest_direction, target_direction);
        hero_skin.pose[bone].translation = target_head;
        hero_skin.pose[bone].rotation = QuaternionMultiply(
            delta, hero_skin.model.skeleton.bindPose[bone].rotation);
        hero_skin.pose[bone].scale =
            hero_skin.model.skeleton.bindPose[bone].scale;
    }
    UpdateModelAnimation(hero_skin.model, hero_skin.animation, 0.0f);
    DrawModel(hero_skin.model, (Vector3){0.0f, 0.0f, 0.0f}, 1.0f, WHITE);
    return true;
}

void CcLocalRendererInit(void)
{
    if (sphere_models.ready) return;
    sphere_models.small = LoadModelFromMesh(GenMeshSphere(1.0f, 6, 8));
    sphere_models.character = LoadModelFromMesh(GenMeshSphere(1.0f, 8, 8));
    sphere_models.scenery = LoadModelFromMesh(GenMeshSphere(1.0f, 10, 12));
    sphere_models.ready = true;
    LoadHeroSkin();
}

void CcLocalRendererShutdown(void)
{
    if (!sphere_models.ready) return;
    UnloadModel(sphere_models.small);
    UnloadModel(sphere_models.character);
    UnloadModel(sphere_models.scenery);
    if (hero_skin.ready) UnloadModel(hero_skin.model);
    sphere_models = (SphereModelCache){0};
    hero_skin = (HeroSkinCache){0};
}

static void DrawSphereModel(Model model, Vector3 center, float radius, Color color)
{
    DrawModelEx(model, center, (Vector3){0.0f, 1.0f, 0.0f}, 0.0f,
                (Vector3){radius, radius, radius}, color);
}

/* Raylib's default 16x16 sphere is excessive for this fixed isometric camera. */
static void DrawSmallSphere(Vector3 center, float radius, Color color)
{
    DrawSphereModel(sphere_models.small, center, radius, color);
}

static void DrawCharacterSphere(Vector3 center, float radius, Color color)
{
    DrawSphereModel(sphere_models.character, center, radius, color);
}

static void DrawScenerySphere(Vector3 center, float radius, Color color)
{
    DrawSphereModel(sphere_models.scenery, center, radius, color);
}

static void DrawBox(Vector3 center, Vector3 size, Color color)
{
    DrawCubeV(center, size, color);
    DrawCubeWiresV(center, size, Fade(WORLD_INK, 0.18f));
}

static void DrawBuilding(float x, float z, float width, float depth, float height,
                         Color wall, Color roof, bool door)
{
    Vector3 center = {x + width * 0.5f, height * 0.5f, z + depth * 0.5f};
    DrawBox(center, (Vector3){width, height, depth}, wall);
    DrawBox((Vector3){center.x, height + 0.07f, center.z},
            (Vector3){width + 0.20f, 0.14f, depth + 0.20f}, roof);
    if (door) {
        DrawBox((Vector3){center.x, 1.075f, z + depth + 0.025f},
                (Vector3){1.05f, 2.15f, 0.05f}, (Color){43, 34, 37, 255});
        DrawSmallSphere((Vector3){center.x + 0.39f, 1.075f,
                                  z + depth + 0.06f},
                        0.035f, WORLD_GOLD);
    }
    DrawBox((Vector3){x + 1.35f, height * 0.56f, z + depth + 0.03f},
            (Vector3){0.90f, 1.10f, 0.04f}, Fade(WORLD_TEAL, 0.78f));
    DrawBox((Vector3){x + width - 1.35f, height * 0.56f,
                      z + depth + 0.03f},
            (Vector3){0.90f, 1.10f, 0.04f}, Fade(WORLD_TEAL, 0.78f));
}

static void DrawGroundTile(float x, float z, Color color)
{
    DrawCube((Vector3){x + 0.5f, -0.035f, z + 0.5f}, 0.98f, 0.06f, 0.98f, color);
}

static void DrawTerrainPatch(float x, float z, float width, float depth,
                             Color color)
{
    DrawCubeV((Vector3){x + width * 0.5f, -0.045f,
                        z + depth * 0.5f},
              (Vector3){width, 0.07f, depth}, color);
}

static void DrawExteriorTerrain(void)
{
    DrawPlane((Vector3){CC_LOCAL_WORLD_WIDTH * 0.5f, -0.09f,
                        CC_LOCAL_WORLD_DEPTH * 0.5f},
              (Vector2){CC_LOCAL_WORLD_WIDTH + 8.0f,
                        CC_LOCAL_WORLD_DEPTH + 8.0f},
              (Color){35, 67, 53, 255});

    DrawTerrainPatch(1.0f, 1.0f, 16.0f, 11.0f,
                     (Color){43, 67, 61, 255});
    DrawTerrainPatch(15.0f, 27.1f, 77.0f, 4.6f,
                     (Color){101, 94, 76, 255});
    DrawTerrainPatch(39.7f, 8.0f, 4.6f, 50.0f,
                     (Color){96, 91, 75, 255});
    DrawTerrainPatch(7.2f, 10.0f, 34.8f, 3.8f,
                     (Color){88, 86, 73, 255});
    DrawTerrainPatch(76.0f, 27.0f, 5.0f, 7.0f,
                     (Color){112, 102, 81, 255});

    DrawTerrainPatch(3.0f, 17.0f, 13.0f, 9.0f,
                     (Color){86, 91, 56, 255});
    DrawTerrainPatch(3.0f, 29.0f, 13.0f, 9.0f,
                     (Color){93, 86, 52, 255});
    DrawTerrainPatch(3.0f, 41.0f, 13.0f, 9.0f,
                     (Color){79, 87, 51, 255});
    DrawTerrainPatch(59.0f, 43.0f, 14.0f, 10.0f,
                     (Color){89, 91, 54, 255});
    DrawTerrainPatch(76.0f, 43.0f, 15.0f, 10.0f,
                     (Color){82, 89, 52, 255});

    for (int32_t row = 0; row < 4; ++row) {
        float z = 18.3f + (float)row * 2.0f;
        DrawTerrainPatch(4.0f, z, 11.0f, 0.24f,
                         Fade(WORLD_GOLD, 0.34f));
        DrawTerrainPatch(60.0f, 44.3f + (float)row * 2.0f, 12.0f, 0.24f,
                         Fade(WORLD_GOLD, 0.28f));
    }
}

static Color BuildingWallColor(int32_t style)
{
    switch (style) {
        case 1: return (Color){105, 96, 84, 255};
        case 2: return (Color){122, 79, 61, 255};
        case 3: return (Color){72, 89, 95, 255};
        default: return (Color){88, 98, 91, 255};
    }
}

static Color BuildingRoofColor(int32_t style, Color kingdom)
{
    switch (style) {
        case 1: return Fade(kingdom, 0.92f);
        case 2: return (Color){216, 158, 68, 255};
        case 3: return (Color){123, 79, 126, 255};
        default: return kingdom;
    }
}

static void DrawWorldBuildings(Color kingdom)
{
    for (int32_t i = 0; i < (int32_t)(sizeof(WORLD_BUILDINGS) /
                                      sizeof(WORLD_BUILDINGS[0])); ++i) {
        const WorldBuilding *building = &WORLD_BUILDINGS[i];
        DrawBuilding(building->footprint.x, building->footprint.y,
                     building->footprint.width, building->footprint.height,
                     building->height, BuildingWallColor(building->style),
                     BuildingRoofColor(building->style, kingdom),
                     building->door);
    }
}

static void DrawCastle(Color kingdom)
{
    for (int32_t i = 0; i < (int32_t)(sizeof(CASTLE_STRUCTURES) /
                                      sizeof(CASTLE_STRUCTURES[0])); ++i) {
        const WorldStructure *structure = &CASTLE_STRUCTURES[i];
        Rectangle footprint = structure->footprint;
        Color stone = i == 5 ? (Color){82, 80, 78, 255} :
                               (Color){100, 103, 98, 255};
        DrawBox((Vector3){footprint.x + footprint.width * 0.5f,
                          structure->height * 0.5f,
                          footprint.y + footprint.height * 0.5f},
                (Vector3){footprint.width, structure->height,
                          footprint.height}, stone);
        DrawBox((Vector3){footprint.x + footprint.width * 0.5f,
                          structure->height + 0.11f,
                          footprint.y + footprint.height * 0.5f},
                (Vector3){footprint.width + 0.18f, 0.22f,
                          footprint.height + 0.18f},
                i >= 8 ? kingdom : (Color){74, 77, 75, 255});
    }
    DrawBox((Vector3){78.5f, 1.20f, 22.03f},
            (Vector3){1.35f, 2.40f, 0.06f}, (Color){43, 34, 37, 255});
    DrawBox((Vector3){76.30f, 7.65f, 30.84f},
            (Vector3){0.78f, 2.30f, 0.06f}, kingdom);
    DrawBox((Vector3){80.70f, 7.65f, 30.84f},
            (Vector3){0.78f, 2.30f, 0.06f}, kingdom);
}

static Vector3 Add3(Vector3 a, Vector3 b)
{
    return (Vector3){a.x + b.x, a.y + b.y, a.z + b.z};
}

static Vector3 LocalPoint(Vector3 base, float x, float y, float z, float yaw)
{
    float cosine = cosf(yaw);
    float sine = sinf(yaw);
    return (Vector3){base.x + x * cosine + z * sine,
                     base.y + y,
                     base.z - x * sine + z * cosine};
}

static void DrawOrientedBox(Vector3 base, Vector3 local_center, Vector3 size,
                            float yaw, Color color)
{
    rlPushMatrix();
    rlTranslatef(base.x, base.y, base.z);
    rlRotatef(yaw * RAD2DEG, 0.0f, 1.0f, 0.0f);
    DrawBox(local_center, size, color);
    rlPopMatrix();
}

static void DrawBoneSegment(Vector3 a, Vector3 b, Color color)
{
    DrawCylinderEx(a, b, 0.028f, 0.028f, 7, color);
}

static void DrawBoneJoint(Vector3 point, Color color)
{
    DrawSmallSphere(point, 0.045f, color);
}

static void DrawPuppet3D(Vector3 position, float scale, float yaw, Color tunic,
                         Color rig, float phase, CcTraversalMode mode, bool player)
{
    float active = mode == CC_TRAVERSAL_IDLE ? 0.0f : 1.0f;
    float stride = sinf(phase) * 0.13f * scale * active;
    float bob = mode == CC_TRAVERSAL_WALK ? fabsf(sinf(phase)) * 0.035f * scale : 0.0f;
    Vector3 hip = LocalPoint(position, 0.0f, 0.73f * scale + bob, 0.0f, yaw);
    Vector3 chest = LocalPoint(position, 0.0f, 1.15f * scale + bob, 0.0f, yaw);
    Vector3 neck = LocalPoint(position, 0.0f, 1.43f * scale + bob, 0.0f, yaw);
    Vector3 shoulder_l = LocalPoint(position, -0.20f * scale,
                                    1.29f * scale + bob, 0.0f, yaw);
    Vector3 shoulder_r = LocalPoint(position, 0.20f * scale,
                                    1.29f * scale + bob, 0.0f, yaw);
    Vector3 elbow_l = LocalPoint(position, -0.29f * scale,
                                 1.01f * scale + bob, stride, yaw);
    Vector3 elbow_r = LocalPoint(position, 0.29f * scale,
                                 1.01f * scale + bob, -stride, yaw);
    Vector3 hand_l = LocalPoint(position, -0.25f * scale,
                                0.78f * scale + bob, stride, yaw);
    Vector3 hand_r = LocalPoint(position, 0.25f * scale,
                                0.78f * scale + bob, -stride, yaw);
    Vector3 knee_l = LocalPoint(position, -0.12f * scale, 0.40f * scale,
                                stride, yaw);
    Vector3 knee_r = LocalPoint(position, 0.12f * scale, 0.40f * scale,
                                -stride, yaw);
    Vector3 foot_l = LocalPoint(position, -0.13f * scale, 0.08f,
                                0.06f + stride, yaw);
    Vector3 foot_r = LocalPoint(position, 0.13f * scale, 0.08f,
                                0.06f - stride, yaw);
    if (mode == CC_TRAVERSAL_CLIMB) {
        elbow_l = LocalPoint(position, -0.25f * scale, 1.50f * scale,
                             0.10f, yaw);
        hand_l = LocalPoint(position, -0.18f * scale, 1.70f * scale,
                            0.15f, yaw);
        knee_r = LocalPoint(position, 0.13f * scale, 0.58f * scale,
                            0.18f, yaw);
    } else if (mode == CC_TRAVERSAL_DROP) {
        elbow_l = LocalPoint(position, -0.28f * scale, 1.48f * scale,
                             0.02f, yaw);
        elbow_r = LocalPoint(position, 0.28f * scale, 1.48f * scale,
                             0.02f, yaw);
        hand_l = LocalPoint(position, -0.20f * scale, 1.68f * scale,
                            0.04f, yaw);
        hand_r = LocalPoint(position, 0.20f * scale, 1.68f * scale,
                            0.04f, yaw);
        knee_l = LocalPoint(position, -0.13f * scale, 0.56f * scale,
                            0.17f, yaw);
        knee_r = LocalPoint(position, 0.13f * scale, 0.56f * scale,
                            0.17f, yaw);
        foot_l = LocalPoint(position, -0.14f * scale, 0.30f * scale,
                            0.03f, yaw);
        foot_r = LocalPoint(position, 0.14f * scale, 0.30f * scale,
                            0.03f, yaw);
    }

    DrawCylinder((Vector3){position.x, position.y + 0.005f, position.z},
                 0.27f * scale, 0.27f * scale, 0.012f, 18,
                 (Color){2, 7, 10, 115});
    DrawCylinderEx(hip, knee_l, 0.085f * scale, 0.075f * scale, 8,
                   (Color){43, 49, 53, 255});
    DrawCylinderEx(hip, knee_r, 0.085f * scale, 0.075f * scale, 8,
                   (Color){43, 49, 53, 255});
    DrawCylinderEx(knee_l, foot_l, 0.075f * scale, 0.065f * scale, 8,
                   (Color){43, 49, 53, 255});
    DrawCylinderEx(knee_r, foot_r, 0.075f * scale, 0.065f * scale, 8,
                   (Color){43, 49, 53, 255});
    DrawOrientedBox(position, (Vector3){0.0f, 1.02f * scale + bob, 0.0f},
                    (Vector3){0.38f * scale, 0.54f * scale, 0.25f * scale},
                    yaw, tunic);
    DrawCylinderEx(shoulder_l, hand_l, 0.065f * scale, 0.055f * scale, 8, tunic);
    DrawCylinderEx(shoulder_r, hand_r, 0.065f * scale, 0.055f * scale, 8, tunic);
    Vector3 head = LocalPoint(position, 0.0f, 1.62f * scale + bob, 0.0f, yaw);
    DrawCharacterSphere(head, 0.19f * scale, (Color){222, 174, 139, 255});
    DrawCharacterSphere(
        LocalPoint(position, 0.0f, 1.75f * scale + bob, -0.02f, yaw),
        0.145f * scale, (Color){58, 43, 45, 255});

    DrawSmallSphere(LocalPoint(position, -0.065f * scale,
                               1.64f * scale + bob, 0.175f * scale, yaw),
                    0.024f * scale, WORLD_VOID);
    DrawSmallSphere(LocalPoint(position, 0.065f * scale,
                               1.64f * scale + bob, 0.175f * scale, yaw),
                    0.024f * scale, WORLD_VOID);

    Vector3 offset = LocalPoint((Vector3){0.0f, 0.0f, 0.0f},
                                0.025f, 0.012f, 0.050f, yaw);
    hip = Add3(hip, offset);
    chest = Add3(chest, offset);
    neck = Add3(neck, offset);
    shoulder_l = Add3(shoulder_l, offset);
    shoulder_r = Add3(shoulder_r, offset);
    elbow_l = Add3(elbow_l, offset);
    elbow_r = Add3(elbow_r, offset);
    hand_l = Add3(hand_l, offset);
    hand_r = Add3(hand_r, offset);
    knee_l = Add3(knee_l, offset);
    knee_r = Add3(knee_r, offset);
    foot_l = Add3(foot_l, offset);
    foot_r = Add3(foot_r, offset);
    DrawBoneSegment(hip, chest, rig);
    DrawBoneSegment(chest, neck, rig);
    DrawBoneSegment(shoulder_l, shoulder_r, rig);
    DrawBoneSegment(shoulder_l, elbow_l, rig);
    DrawBoneSegment(elbow_l, hand_l, rig);
    DrawBoneSegment(shoulder_r, elbow_r, rig);
    DrawBoneSegment(elbow_r, hand_r, rig);
    DrawBoneSegment(hip, knee_l, rig);
    DrawBoneSegment(knee_l, foot_l, rig);
    DrawBoneSegment(hip, knee_r, rig);
    DrawBoneSegment(knee_r, foot_r, rig);
    const Vector3 joints[] = {
        hip, chest, neck, shoulder_l, shoulder_r, elbow_l, elbow_r,
        hand_l, hand_r, knee_l, knee_r, foot_l, foot_r
    };
    for (int32_t joint = 0;
         joint < (int32_t)(sizeof(joints) / sizeof(joints[0])); ++joint) {
        DrawBoneJoint(joints[joint], rig);
    }
    if (player) {
        DrawSmallSphere(
            LocalPoint(position, 0.0f, 2.08f * scale + bob, 0.0f, yaw),
            0.065f, WORLD_GOLD);
    }
}

static void DrawPitchedFoot(Vector3 heel, Vector3 toe, float yaw, Color color)
{
    Vector3 center = PhysicsScale(PhysicsAdd(heel, toe), 0.5f);
    Vector3 difference = PhysicsSubtract(toe, heel);
    float horizontal = sqrtf(difference.x * difference.x +
                             difference.z * difference.z);
    Vector3 fallback_forward = {sinf(yaw), 0.0f, cosf(yaw)};
    Vector3 horizontal_forward = horizontal > 0.0001f ?
        (Vector3){difference.x / horizontal, 0.0f,
                  difference.z / horizontal} : fallback_forward;
    horizontal_forward = PhysicsNormalizeOr(
        PhysicsLerp(fallback_forward, horizontal_forward,
                    SmoothStep01(horizontal / 0.08f)),
        fallback_forward);
    float solved_yaw = atan2f(horizontal_forward.x, horizontal_forward.z);
    float pitch = atan2f(difference.y, fmaxf(0.0001f, horizontal));
    center.y += 0.045f;
    rlPushMatrix();
    rlTranslatef(center.x, center.y, center.z);
    rlRotatef(solved_yaw * RAD2DEG, 0.0f, 1.0f, 0.0f);
    rlRotatef(-pitch * RAD2DEG, 1.0f, 0.0f, 0.0f);
    DrawBox((Vector3){0.0f, 0.0f, 0.0f}, (Vector3){0.18f, 0.10f, 0.32f},
            color);
    rlPopMatrix();
}

static Color HumanoidContactColor(CcHumanoidContact contact)
{
    switch (contact) {
        case CC_HUMANOID_CONTACT_HEEL: return (Color){116, 224, 197, 255};
        case CC_HUMANOID_CONTACT_FLAT: return WORLD_GOLD;
        case CC_HUMANOID_CONTACT_TOE: return (Color){245, 151, 66, 255};
        case CC_HUMANOID_CONTACT_SWING: return WORLD_VIOLET;
        case CC_HUMANOID_CONTACT_AIR:
        default: return WORLD_MUTED;
    }
}

static float PoseAxisYaw(Vector3 left, Vector3 right, float fallback)
{
    Vector3 axis = PhysicsSubtract(right, left);
    float horizontal = sqrtf(axis.x * axis.x + axis.z * axis.z);
    return horizontal > 0.0001f ? atan2f(-axis.z, axis.x) : fallback;
}

static void DrawHeroSkinRigOverlay(const CcHumanoidGait *gait,
                                   const CcHumanoidSkinPose *skin,
                                   const CcLocalCapeState *cape)
{
    Vector3 offset = {0.018f, 0.010f, 0.018f};
    for (int32_t bone = 0; bone < CC_HUMANOID_SKIN_BONE_COUNT; ++bone) {
        Color color = Fade(WORLD_GOLD, 0.78f);
        if (bone == CC_HUMANOID_SKIN_ROOT) color = Fade(WORLD_TEAL, 0.85f);
        else if (bone >= CC_HUMANOID_SKIN_THIGH_LEFT &&
                 bone <= CC_HUMANOID_SKIN_FOOT_LEFT) {
            color = Fade(HumanoidContactColor(gait->feet[0].contact), 0.82f);
        } else if (bone >= CC_HUMANOID_SKIN_THIGH_RIGHT &&
                   bone <= CC_HUMANOID_SKIN_FOOT_RIGHT) {
            color = Fade(HumanoidContactColor(gait->feet[1].contact), 0.82f);
        }
        Vector3 head = Add3(FromLimbVector(skin->bones[bone].head), offset);
        Vector3 tail = Add3(FromLimbVector(skin->bones[bone].tail), offset);
        DrawBoneSegment(head, tail, color);
        DrawBoneJoint(head, color);
    }
    if (cape != NULL && cape->initialized) {
        for (int32_t point = 0; point < CC_LOCAL_CAPE_POINT_COUNT - 1;
             ++point) {
            Vector3 head = Add3(cape->point[point], offset);
            Vector3 tail = Add3(cape->point[point + 1], offset);
            DrawBoneSegment(head, tail, Fade(WORLD_VIOLET, 0.86f));
            DrawBoneJoint(head, Fade(WORLD_VIOLET, 0.92f));
        }
    }
}

static void DrawBiomechanicalBiped(const CcLocalAgent *agent)
{
    const CcHumanoidGait *gait = &agent->humanoid;
    const CcHumanoidPose *pose = AgentRenderPose(agent);
    CcHumanoidSkinPose skin;
    CcHumanoidSkinPoseResolve(pose, &skin);
    if (!skin.valid) return;
    bool modular_hero = agent->crowned ||
                        agent->combat.team == CC_COMBAT_GUARD;
    if (modular_hero && DrawHeroSkin(&skin, &agent->cape)) {
        DrawHeroSkinRigOverlay(gait, &skin, &agent->cape);
        if (agent->crowned) {
            Vector3 crown = FromLimbVector(
                skin.bones[CC_HUMANOID_SKIN_HEAD].tail);
            crown = PhysicsAdd(crown, PhysicsScale(
                FromLimbVector(skin.body_up), 0.10f));
            DrawSmallSphere(crown, 0.060f, WORLD_GOLD);
        }
        return;
    }
    Vector3 pelvis = FromLimbVector(
        skin.bones[CC_HUMANOID_SKIN_PELVIS].head);
    Vector3 spine = FromLimbVector(
        skin.bones[CC_HUMANOID_SKIN_SPINE].head);
    Vector3 chest = FromLimbVector(
        skin.bones[CC_HUMANOID_SKIN_CHEST].head);
    Vector3 neck = FromLimbVector(
        skin.bones[CC_HUMANOID_SKIN_NECK].head);
    Vector3 head = FromLimbVector(
        skin.sockets[CC_HUMANOID_SOCKET_HEAD].position);
    float fallen_weight = fmaxf(0.0f, fminf(agent->ragdoll_visual_blend, 1.0f));
    float upright_weight = 1.0f - fallen_weight;
    Vector3 shoulder_left = FromLimbVector(
        skin.sockets[CC_HUMANOID_SOCKET_SHOULDER_LEFT].position);
    Vector3 shoulder_right = FromLimbVector(
        skin.sockets[CC_HUMANOID_SOCKET_SHOULDER_RIGHT].position);
    Vector3 hip_left = FromLimbVector(
        skin.sockets[CC_HUMANOID_SOCKET_HIP_LEFT].position);
    Vector3 hip_right = FromLimbVector(
        skin.sockets[CC_HUMANOID_SOCKET_HIP_RIGHT].position);
    float upper_yaw = PoseAxisYaw(shoulder_left, shoulder_right,
                                  agent->facing_yaw + pose->chest_yaw);
    float pelvis_yaw = PoseAxisYaw(hip_left, hip_right,
                                   agent->facing_yaw + pose->pelvis_yaw);
    Vector3 body_up = FromLimbVector(skin.body_up);
    Vector3 cape_center = PhysicsAdd(
        FromLimbVector(skin.sockets[CC_HUMANOID_SOCKET_BACK].position),
        PhysicsScale(body_up, -0.24f));
    Color tunic = agent->tunic_color;
    if (agent->combat.defeated) tunic = (Color){61, 57, 62, 255};
    else if (agent->combat.hit_flash_seconds > 0.0f) tunic = WORLD_INK;

    if (fallen_weight > 0.01f) {
        Vector3 fallen_back = PhysicsScale(
            (Vector3){sinf(upper_yaw), 0.0f, cosf(upper_yaw)}, -0.12f);
        DrawCylinderEx(PhysicsAdd(chest, fallen_back),
                       PhysicsAdd(pelvis, fallen_back),
                       0.17f, 0.25f, 5,
                       Fade((Color){73, 55, 91, 255}, fallen_weight));
        DrawCharacterSphere(pelvis, 0.18f,
                            Fade((Color){44, 61, 65, 255}, fallen_weight));
        DrawCylinderEx(pelvis, spine,
                       0.18f, 0.22f, 10,
                       Fade((Color){38, 105, 112, 255}, fallen_weight));
        DrawCylinderEx(spine, chest,
                       0.23f, 0.27f, 10,
                       Fade(tunic, fallen_weight));
        DrawCylinderEx(chest, neck, 0.18f, 0.09f, 9,
                       Fade(tunic, fallen_weight));
    }
    if (upright_weight > 0.01f) {
        DrawOrientedBox(cape_center, (Vector3){0.0f, 0.04f, 0.0f},
                        (Vector3){0.46f, 0.62f, 0.045f}, upper_yaw,
                        Fade((Color){73, 55, 91, 255}, upright_weight));
        DrawOrientedBox(pelvis, (Vector3){0.0f, 0.02f, 0.0f},
                        (Vector3){0.40f, 0.18f, 0.27f},
                        pelvis_yaw,
                        Fade((Color){44, 61, 65, 255}, upright_weight));
        DrawCylinderEx(spine, chest,
                       0.25f, 0.29f, 10,
                       Fade(tunic, upright_weight));
        Vector3 plate = FromLimbVector(
            skin.sockets[CC_HUMANOID_SOCKET_CHEST_FRONT].position);
        DrawOrientedBox(plate, (Vector3){0.0f, -0.04f, 0.0f},
                        (Vector3){0.34f, 0.28f, 0.045f}, upper_yaw,
                        Fade((Color){223, 173, 67, 255}, upright_weight));
    }

    const CcHumanoidSkinBone thigh_bones[] = {
        CC_HUMANOID_SKIN_THIGH_LEFT,
        CC_HUMANOID_SKIN_THIGH_RIGHT,
    };
    const CcHumanoidSkinBone shin_bones[] = {
        CC_HUMANOID_SKIN_SHIN_LEFT,
        CC_HUMANOID_SKIN_SHIN_RIGHT,
    };
    const CcHumanoidSkinBone foot_bones[] = {
        CC_HUMANOID_SKIN_FOOT_LEFT,
        CC_HUMANOID_SKIN_FOOT_RIGHT,
    };
    for (int32_t leg = 0; leg < CC_HUMANOID_LEG_COUNT; ++leg) {
        Vector3 hip = FromLimbVector(skin.bones[thigh_bones[leg]].head);
        Vector3 knee = FromLimbVector(skin.bones[thigh_bones[leg]].tail);
        Vector3 ankle = FromLimbVector(skin.bones[shin_bones[leg]].tail);
        Vector3 heel = FromLimbVector(pose->heel[leg]);
        Vector3 toe = FromLimbVector(skin.bones[foot_bones[leg]].tail);
        DrawCylinderEx(hip, knee, 0.086f, 0.070f, 9,
                       (Color){38, 63, 68, 255});
        DrawCylinderEx(knee, ankle, 0.069f, 0.052f, 9,
                       (Color){48, 71, 75, 255});
        DrawPitchedFoot(heel, toe, agent->facing_yaw,
                        (Color){35, 54, 59, 255});
    }

    const CcHumanoidSkinBone upper_arm_bones[] = {
        CC_HUMANOID_SKIN_UPPER_ARM_LEFT,
        CC_HUMANOID_SKIN_UPPER_ARM_RIGHT,
    };
    const CcHumanoidSkinBone forearm_bones[] = {
        CC_HUMANOID_SKIN_FOREARM_LEFT,
        CC_HUMANOID_SKIN_FOREARM_RIGHT,
    };
    for (int32_t arm = 0; arm < CC_HUMANOID_ARM_COUNT; ++arm) {
        Vector3 shoulder = FromLimbVector(
            skin.bones[upper_arm_bones[arm]].head);
        Vector3 elbow = FromLimbVector(
            skin.bones[upper_arm_bones[arm]].tail);
        Vector3 hand = FromLimbVector(
            skin.bones[forearm_bones[arm]].tail);
        DrawCylinderEx(shoulder, elbow, 0.066f, 0.055f, 8,
                       tunic);
        DrawCylinderEx(elbow, hand, 0.055f, 0.044f, 8,
                       (Color){54, 66, 71, 255});
        DrawCharacterSphere(shoulder, 0.092f,
                            (Color){223, 173, 67, 255});
        DrawSmallSphere(hand, 0.055f, (Color){221, 174, 118, 255});
    }

    DrawCharacterSphere(head, 0.18f, (Color){221, 174, 118, 255});
    Vector3 head_up = PhysicsNormalizeOr(PhysicsSubtract(head, neck),
                                         (Vector3){0.0f, 1.0f, 0.0f});
    Vector3 fallback_right = {cosf(upper_yaw), 0.0f, -sinf(upper_yaw)};
    Vector3 shoulder_axis = PhysicsSubtract(shoulder_right, shoulder_left);
    Vector3 head_right = PhysicsSubtract(
        shoulder_axis, PhysicsScale(head_up,
                                    PhysicsDot(shoulder_axis, head_up)));
    head_right = PhysicsNormalizeOr(head_right, fallback_right);
    Vector3 head_forward = PhysicsNormalizeOr(PhysicsCross(head_right, head_up),
        (Vector3){sinf(upper_yaw), 0.0f, cosf(upper_yaw)});
    Vector3 hair = PhysicsAdd(
        PhysicsAdd(head, PhysicsScale(head_up, 0.10f)),
        PhysicsScale(head_forward, -0.025f));
    DrawCharacterSphere(hair, 0.145f, (Color){52, 46, 51, 255});
    Vector3 eye_center = PhysicsAdd(
        PhysicsAdd(head, PhysicsScale(head_up, 0.01f)),
        PhysicsScale(head_forward, 0.165f));
    DrawSmallSphere(PhysicsAdd(eye_center,
                               PhysicsScale(head_right, -0.058f)),
                    0.022f, WORLD_VOID);
    DrawSmallSphere(PhysicsAdd(eye_center,
                               PhysicsScale(head_right, 0.058f)),
                    0.022f, WORLD_VOID);
    if (agent->crowned) {
        DrawSmallSphere(PhysicsAdd(head, PhysicsScale(head_up, 0.30f)),
                        0.060f, WORLD_GOLD);
    }

    Vector3 rig_offset = LocalPoint((Vector3){0}, 0.022f, 0.010f, 0.040f,
                                    upper_yaw);
    Vector3 physical_root = {gait->body.root.position.x,
                             gait->body.root.position.y,
                             gait->body.root.position.z};
    Vector3 reaction_end = physical_root;
    if (gait->body.total_mass > 0.0f) {
        reaction_end.x += gait->ground_reaction.x / gait->body.total_mass * 0.012f;
        reaction_end.y += gait->ground_reaction.y / gait->body.total_mass * 0.012f;
        reaction_end.z += gait->ground_reaction.z / gait->body.total_mass * 0.012f;
    }
    if (upright_weight > 0.01f) {
        DrawSmallSphere(Add3(physical_root, rig_offset), 0.052f,
                        Fade(WORLD_TEAL, upright_weight));
        DrawCylinderEx(Add3(physical_root, rig_offset),
                       Add3(reaction_end, rig_offset), 0.014f, 0.008f, 6,
                       Fade(WORLD_TEAL, 0.82f * upright_weight));
    }
    Vector3 rig_pelvis = Add3(pelvis, rig_offset);
    Vector3 rig_spine = Add3(spine, rig_offset);
    Vector3 rig_chest = Add3(chest, rig_offset);
    Vector3 rig_neck = Add3(neck, rig_offset);
    DrawBoneSegment(rig_pelvis, rig_spine, WORLD_GOLD);
    DrawBoneSegment(rig_spine, rig_chest, WORLD_GOLD);
    DrawBoneSegment(rig_chest, rig_neck, WORLD_GOLD);
    DrawBoneJoint(rig_pelvis, WORLD_GOLD);
    DrawBoneJoint(rig_spine, WORLD_GOLD);
    DrawBoneJoint(rig_chest, WORLD_GOLD);
    DrawBoneJoint(rig_neck, WORLD_GOLD);
    for (int32_t leg = 0; leg < CC_HUMANOID_LEG_COUNT; ++leg) {
        Color bone = HumanoidContactColor(gait->feet[leg].contact);
        Vector3 hip = Add3(FromLimbVector(
            skin.bones[thigh_bones[leg]].head), rig_offset);
        Vector3 knee = Add3(FromLimbVector(
            skin.bones[thigh_bones[leg]].tail), rig_offset);
        Vector3 ankle = Add3(FromLimbVector(
            skin.bones[shin_bones[leg]].tail), rig_offset);
        Vector3 heel = Add3(FromLimbVector(pose->heel[leg]), rig_offset);
        Vector3 ball = Add3(FromLimbVector(pose->ball[leg]), rig_offset);
        Vector3 toe = Add3(FromLimbVector(pose->toe[leg]), rig_offset);
        DrawBoneSegment(hip, knee, bone);
        DrawBoneSegment(knee, ankle, bone);
        DrawBoneSegment(ankle, heel, bone);
        DrawBoneSegment(heel, ball, bone);
        DrawBoneSegment(ball, toe, bone);
        DrawBoneJoint(hip, bone);
        DrawBoneJoint(knee, bone);
        DrawBoneJoint(ankle, bone);
        DrawBoneJoint(heel, bone);
        DrawBoneJoint(ball, bone);
        DrawBoneJoint(toe, bone);
    }
    for (int32_t arm = 0; arm < CC_HUMANOID_ARM_COUNT; ++arm) {
        Vector3 shoulder = Add3(FromLimbVector(
            skin.bones[upper_arm_bones[arm]].head), rig_offset);
        Vector3 elbow = Add3(FromLimbVector(
            skin.bones[upper_arm_bones[arm]].tail), rig_offset);
        Vector3 hand = Add3(FromLimbVector(
            skin.bones[forearm_bones[arm]].tail), rig_offset);
        DrawBoneSegment(shoulder, elbow, WORLD_GOLD);
        DrawBoneSegment(elbow, hand, WORLD_GOLD);
        DrawBoneJoint(shoulder, WORLD_GOLD);
        DrawBoneJoint(elbow, WORLD_GOLD);
        DrawBoneJoint(hand, WORLD_GOLD);
    }
}

static Vector3 SolveVisualTwoBone(Vector3 root, Vector3 target,
                                  Vector3 bend_direction,
                                  float upper_length, float lower_length)
{
    Vector3 delta = PhysicsSubtract(target, root);
    float raw_distance = PhysicsLength(delta);
    Vector3 direction = raw_distance > 0.0001f ?
                        PhysicsScale(delta, 1.0f / raw_distance) :
                        (Vector3){0.0f, -1.0f, 0.0f};
    float distance = fmaxf(fabsf(upper_length - lower_length) + 0.0001f,
                           fminf(raw_distance,
                                 upper_length + lower_length - 0.0001f));
    Vector3 pole = PhysicsSubtract(
        bend_direction, PhysicsScale(direction,
                                     PhysicsDot(bend_direction, direction)));
    float pole_length = PhysicsLength(pole);
    if (pole_length <= 0.0001f) pole = (Vector3){1.0f, 0.0f, 0.0f};
    else pole = PhysicsScale(pole, 1.0f / pole_length);
    float along = (upper_length * upper_length - lower_length * lower_length +
                   distance * distance) / (2.0f * distance);
    float height = sqrtf(fmaxf(0.0f,
                               upper_length * upper_length - along * along));
    return PhysicsAdd(PhysicsAdd(root, PhysicsScale(direction, along)),
                      PhysicsScale(pole, height));
}

static void DrawRobotShell(const CcLocalAgent *agent)
{
    Vector3 body = ShellPodCenter(agent);
    const CcLimbRig *rig = &agent->limb_rig;
    bool biped = rig->morphology.preset == CC_MORPHOLOGY_BIPED;
    if (biped && agent->humanoid.initialized) {
        DrawBiomechanicalBiped(agent);
        return;
    }
    if (!biped) {
        float body_length = rig->morphology.limb_count >= 6 ? 0.72f : 0.54f;
        DrawOrientedBox(body, (Vector3){0.0f, 0.0f, 0.0f},
                        (Vector3){0.54f, 0.38f, body_length}, agent->facing_yaw,
                        (Color){42, 128, 136, 255});
        DrawSphereWires(body, 0.32f, 10, 10, WORLD_GOLD);
        DrawOrientedBox(body, (Vector3){0.0f, 0.02f, 0.22f},
                        (Vector3){0.24f, 0.18f, 0.28f}, agent->facing_yaw,
                        (Color){225, 177, 68, 255});
        DrawSmallSphere(
            LocalPoint(body, -0.085f, 0.06f, 0.34f, agent->facing_yaw),
            0.040f, WORLD_VOID);
        DrawSmallSphere(
            LocalPoint(body, 0.085f, 0.06f, 0.34f, agent->facing_yaw),
            0.040f, WORLD_VOID);
    }
    Vector3 rig_offset = LocalPoint((Vector3){0}, 0.022f, 0.010f, 0.040f,
                                    agent->facing_yaw);
    for (int32_t limb_index = 0; limb_index < rig->morphology.limb_count;
         ++limb_index) {
        const CcLimbRuntime *limb = &rig->limbs[limb_index];
        const CcLimbSpec *spec = &rig->morphology.limbs[limb_index];
        Color bone = limb->state == CC_LIMB_SWING ? WORLD_VIOLET :
                     limb->state == CC_LIMB_DISABLED ? WORLD_DANGER : WORLD_GOLD;
        for (int32_t segment = 0; segment < spec->segment_count; ++segment) {
            Vector3 a = FromLimbVector(limb->joints[segment]);
            Vector3 b = FromLimbVector(limb->joints[segment + 1]);
            float skin = segment == 0 ? 0.072f : 0.058f;
            DrawCylinderEx(a, b, skin, skin * 0.82f, 8,
                           limb->state == CC_LIMB_DISABLED ?
                           (Color){67, 48, 51, 255} : (Color){54, 66, 71, 255});
            Vector3 rig_a = Add3(a, rig_offset);
            Vector3 rig_b = Add3(b, rig_offset);
            DrawCylinderEx(rig_a, rig_b, 0.022f, 0.022f, 7, bone);
            DrawSmallSphere(rig_a, 0.046f, bone);
        }
        Vector3 foot = FromLimbVector(limb->joints[spec->segment_count]);
        DrawSmallSphere(foot, 0.076f, bone);
        Color foot_color = limb->state == CC_LIMB_SWING ?
                           Fade(WORLD_VIOLET, 0.72f) :
                           limb->state == CC_LIMB_DISABLED ?
                           Fade(WORLD_DANGER, 0.72f) : (Color){37, 62, 67, 255};
        if (biped) {
            if (agent->traversal == CC_TRAVERSAL_CLIMB &&
                fabsf(limb->contact_normal.y) < 0.5f) {
                Vector3 size = fabsf(limb->contact_normal.x) > 0.5f ?
                               (Vector3){0.09f, 0.28f, 0.17f} :
                               (Vector3){0.17f, 0.28f, 0.09f};
                Vector3 center = PhysicsAdd(foot,
                                            PhysicsScale(FromLimbVector(
                                                limb->contact_normal), 0.035f));
                DrawBox(center, size, foot_color);
            } else {
                DrawOrientedBox(foot, (Vector3){0.0f, 0.025f, 0.075f},
                                (Vector3){0.17f, 0.09f, 0.30f}, agent->facing_yaw,
                                foot_color);
            }
        } else {
            DrawCylinder((Vector3){foot.x, foot.y - 0.02f, foot.z},
                         0.11f, 0.09f, 0.04f, 8, foot_color);
        }
    }
    if (biped) {
        float arm_swing = 0.0f;
        float upper_yaw = agent->facing_yaw;
        float climb_lean = agent->traversal == CC_TRAVERSAL_CLIMB ?
                           sinf(agent->climb_progress * PI) * 0.11f : 0.0f;
        float hero_lean = climb_lean;
        Vector3 spine_base = LocalPoint(body, 0.0f, 0.03f, 0.010f,
                                        upper_yaw);
        Vector3 chest = LocalPoint(body, 0.0f, 0.41f, 0.020f + hero_lean,
                                   upper_yaw);
        Vector3 neck = LocalPoint(body, 0.0f, 0.66f,
                                  0.020f + hero_lean * 1.20f, upper_yaw);
        Vector3 shoulder_l = LocalPoint(body, -0.285f, 0.50f,
                                        hero_lean * 0.92f, upper_yaw);
        Vector3 shoulder_r = LocalPoint(body, 0.285f, 0.50f,
                                        hero_lean * 0.92f, upper_yaw);
        Vector3 elbow_l = LocalPoint(body, -0.34f, 0.27f,
                                     -arm_swing * 0.68f, upper_yaw);
        Vector3 elbow_r = LocalPoint(body, 0.34f, 0.27f,
                                     arm_swing * 0.68f, upper_yaw);
        Vector3 hand_l = LocalPoint(body, -0.30f, 0.04f,
                                    -arm_swing * 1.35f, upper_yaw);
        Vector3 hand_r = LocalPoint(body, 0.30f, 0.04f,
                                    arm_swing * 1.35f, upper_yaw);
        if (agent->traversal == CC_TRAVERSAL_CLIMB) {
            float reach = SmoothStep01(agent->climb_progress / 0.18f);
            float release = SmoothStep01((agent->climb_progress - 0.76f) / 0.24f);
            Vector3 resting_left = hand_l;
            Vector3 resting_right = hand_r;
            Vector3 held_left = PhysicsLerp(resting_left,
                                            agent->climb_hand_left, reach);
            Vector3 held_right = PhysicsLerp(resting_right,
                                             agent->climb_hand_right, reach);
            hand_l = PhysicsLerp(held_left, resting_left, release);
            hand_r = PhysicsLerp(held_right, resting_right, release);
            Vector3 bend_left = FramePoint((Vector3){0}, -1.0f, 0.15f, -0.18f,
                                           upper_yaw);
            Vector3 bend_right = FramePoint((Vector3){0}, 1.0f, 0.15f, -0.18f,
                                            upper_yaw);
            elbow_l = SolveVisualTwoBone(shoulder_l, hand_l, bend_left,
                                         0.36f, 0.38f);
            elbow_r = SolveVisualTwoBone(shoulder_r, hand_r, bend_right,
                                         0.36f, 0.38f);
        }

        DrawOrientedBox(body, (Vector3){0.0f, 0.36f, -0.155f},
                        (Vector3){0.46f, 0.56f, 0.045f}, upper_yaw,
                        (Color){73, 55, 91, 255});
        DrawOrientedBox(body, (Vector3){0.0f, 0.03f, 0.0f},
                        (Vector3){0.40f, 0.18f, 0.27f}, upper_yaw,
                        (Color){44, 61, 65, 255});
        DrawOrientedBox(body, (Vector3){0.0f, 0.36f, hero_lean * 0.55f},
                        (Vector3){0.50f, 0.50f, 0.29f}, upper_yaw,
                        (Color){42, 128, 136, 255});
        DrawOrientedBox(body, (Vector3){0.0f, 0.39f, 0.165f + hero_lean * 0.55f},
                        (Vector3){0.34f, 0.29f, 0.045f}, upper_yaw,
                        (Color){223, 173, 67, 255});
        DrawCylinderEx(shoulder_l, elbow_l, 0.065f, 0.055f, 8,
                       (Color){42, 128, 136, 255});
        DrawCylinderEx(elbow_l, hand_l, 0.055f, 0.045f, 8,
                       (Color){54, 66, 71, 255});
        DrawCylinderEx(shoulder_r, elbow_r, 0.065f, 0.055f, 8,
                       (Color){42, 128, 136, 255});
        DrawCylinderEx(elbow_r, hand_r, 0.055f, 0.045f, 8,
                       (Color){54, 66, 71, 255});
        DrawCharacterSphere(shoulder_l, 0.095f,
                            (Color){223, 173, 67, 255});
        DrawCharacterSphere(shoulder_r, 0.095f,
                            (Color){223, 173, 67, 255});
        DrawSmallSphere(hand_l, 0.055f, (Color){221, 174, 118, 255});
        DrawSmallSphere(hand_r, 0.055f, (Color){221, 174, 118, 255});

        Vector3 visual_spine_base = Add3(spine_base, rig_offset);
        Vector3 visual_chest = Add3(chest, rig_offset);
        Vector3 visual_neck = Add3(neck, rig_offset);
        Vector3 visual_shoulder_l = Add3(shoulder_l, rig_offset);
        Vector3 visual_shoulder_r = Add3(shoulder_r, rig_offset);
        Vector3 visual_elbow_l = Add3(elbow_l, rig_offset);
        Vector3 visual_elbow_r = Add3(elbow_r, rig_offset);
        Vector3 visual_hand_l = Add3(hand_l, rig_offset);
        Vector3 visual_hand_r = Add3(hand_r, rig_offset);
        DrawBoneSegment(visual_spine_base, visual_chest, WORLD_GOLD);
        DrawBoneSegment(visual_chest, visual_neck, WORLD_GOLD);
        DrawBoneSegment(visual_shoulder_l, visual_shoulder_r, WORLD_GOLD);
        DrawBoneSegment(visual_shoulder_l, visual_elbow_l, WORLD_GOLD);
        DrawBoneSegment(visual_elbow_l, visual_hand_l, WORLD_GOLD);
        DrawBoneSegment(visual_shoulder_r, visual_elbow_r, WORLD_GOLD);
        DrawBoneSegment(visual_elbow_r, visual_hand_r, WORLD_GOLD);
        Vector3 visual_joints[] = {
            visual_spine_base, visual_chest,      visual_neck,
            visual_shoulder_l, visual_shoulder_r, visual_elbow_l,
            visual_elbow_r,    visual_hand_l,     visual_hand_r,
        };
        for (size_t joint_index = 0;
             joint_index < sizeof(visual_joints) / sizeof(visual_joints[0]);
             joint_index++) {
            DrawBoneJoint(visual_joints[joint_index], WORLD_GOLD);
        }

        Vector3 head = LocalPoint(body, 0.0f, 0.81f, hero_lean * 1.25f,
                                  upper_yaw);
        DrawCharacterSphere(head, 0.18f, (Color){221, 174, 118, 255});
        DrawCharacterSphere(
            LocalPoint(body, 0.0f, 0.91f,
                       -0.025f + hero_lean * 1.25f, upper_yaw),
            0.145f, (Color){52, 46, 51, 255});
        DrawSmallSphere(
            LocalPoint(body, -0.058f, 0.82f,
                       0.165f + hero_lean * 1.25f, upper_yaw),
            0.022f, WORLD_VOID);
        DrawSmallSphere(
            LocalPoint(body, 0.058f, 0.82f,
                       0.165f + hero_lean * 1.25f, upper_yaw),
            0.022f, WORLD_VOID);
        DrawSmallSphere(
            LocalPoint(body, 0.0f, 1.11f, hero_lean * 1.20f, upper_yaw),
            0.060f, WORLD_GOLD);
    } else {
        DrawSmallSphere(
            LocalPoint(body, 0.0f, 0.52f, 0.0f, agent->facing_yaw),
            0.065f, WORLD_GOLD);
    }
}

static void DrawCarriage3D(void)
{
    float x = CARRIAGE_FOOTPRINT.x + CARRIAGE_FOOTPRINT.width * 0.5f;
    float z = CARRIAGE_FOOTPRINT.y + CARRIAGE_FOOTPRINT.height * 0.5f;
    DrawBox((Vector3){x, 1.06f, z},
            (Vector3){CARRIAGE_FOOTPRINT.width, 1.62f,
                      CARRIAGE_FOOTPRINT.height},
            (Color){125, 66, 50, 255});
    DrawBox((Vector3){x, 1.94f, z},
            (Vector3){CARRIAGE_FOOTPRINT.width + 0.22f, 0.18f,
                      CARRIAGE_FOOTPRINT.height + 0.18f},
            (Color){228, 174, 77, 255});
    DrawBox((Vector3){CARRIAGE_FOOTPRINT.x + CARRIAGE_FOOTPRINT.width + 0.03f,
                       1.20f, z}, (Vector3){0.04f, 0.70f, 1.20f},
            (Color){35, 102, 108, 255});
    Vector3 wheels[] = {
        {CARRIAGE_FOOTPRINT.x, 0.58f, CARRIAGE_FOOTPRINT.y + 1.15f},
        {CARRIAGE_FOOTPRINT.x + CARRIAGE_FOOTPRINT.width, 0.58f,
         CARRIAGE_FOOTPRINT.y + 1.15f},
        {CARRIAGE_FOOTPRINT.x, 0.58f,
         CARRIAGE_FOOTPRINT.y + CARRIAGE_FOOTPRINT.height - 1.15f},
        {CARRIAGE_FOOTPRINT.x + CARRIAGE_FOOTPRINT.width, 0.58f,
         CARRIAGE_FOOTPRINT.y + CARRIAGE_FOOTPRINT.height - 1.15f}
    };
    for (int32_t i = 0; i < 4; ++i) {
        DrawScenerySphere(wheels[i], 0.54f, (Color){38, 31, 31, 255});
        DrawSphereWires(wheels[i], 0.55f, 7, 7, WORLD_GOLD);
    }
}

static void DrawNotice3D(const CcSim *sim)
{
    float x = CC_LOCAL_NOTICE_X;
    float z = CC_LOCAL_NOTICE_Z;
    DrawBox((Vector3){x - 0.38f, 0.72f, z}, (Vector3){0.10f, 1.44f, 0.10f},
            (Color){89, 58, 42, 255});
    DrawBox((Vector3){x + 0.38f, 0.72f, z}, (Vector3){0.10f, 1.44f, 0.10f},
            (Color){89, 58, 42, 255});
    DrawBox((Vector3){x, 1.32f, z}, (Vector3){1.08f, 0.82f, 0.12f},
            (Color){148, 94, 52, 255});
    int32_t count = CcSimActiveSituationCount(sim);
    for (int32_t i = 0; i < count && i < 4; ++i) {
        DrawBox((Vector3){x - 0.34f + (float)i * 0.22f,
                          1.34f + (float)(i & 1) * 0.12f, z - 0.07f},
                (Vector3){0.16f, 0.30f, 0.025f},
                i == 3 ? WORLD_DANGER : WORLD_INK);
    }
}

static void DrawDungeon3D(const CcDungeon *dungeon)
{
    float x = CC_LOCAL_DUNGEON_X;
    float z = DUNGEON_FOOTPRINT.y + DUNGEON_FOOTPRINT.height * 0.5f;
    DrawBox((Vector3){x - 1.15f, 1.40f, z}, (Vector3){0.62f, 2.80f, 0.82f},
            (Color){64, 56, 72, 255});
    DrawBox((Vector3){x + 1.15f, 1.40f, z}, (Vector3){0.62f, 2.80f, 0.82f},
            (Color){64, 56, 72, 255});
    DrawBox((Vector3){x, 2.84f, z}, (Vector3){2.92f, 0.42f, 0.82f},
            (Color){74, 62, 84, 255});
    DrawBox((Vector3){x, 1.22f, DUNGEON_FOOTPRINT.y +
                                     DUNGEON_FOOTPRINT.height + 0.03f},
            (Vector3){1.28f, 2.44f, 0.05f},
            (Color){8, 5, 14, 255});
    float pulse = 0.06f + (float)dungeon->regional_pressure / 500.0f;
    DrawScenerySphere((Vector3){x, 1.32f, DUNGEON_FOOTPRINT.y +
                                            DUNGEON_FOOTPRINT.height + 0.08f}, pulse,
                      Fade(WORLD_VIOLET, 0.82f));
}

static void DrawTree(float x, float z, Color leaves)
{
    DrawCylinder((Vector3){x, 0.0f, z}, 0.13f, 0.10f, 1.30f, 7,
                 (Color){80, 57, 43, 255});
    DrawScenerySphere((Vector3){x, 1.68f, z}, 0.58f, leaves);
    DrawSphereWires((Vector3){x, 1.68f, z}, 0.59f, 7, 7, Fade(WORLD_INK, 0.12f));
}

static void DrawWorldTrees(void)
{
    static const Vector2 trees[] = {
        {2.5f, 58.0f}, {6.0f, 62.0f}, {10.0f, 56.0f}, {14.0f, 65.0f},
        {18.0f, 58.0f}, {22.0f, 64.0f}, {27.0f, 60.0f}, {33.0f, 66.0f},
        {48.0f, 63.0f}, {54.0f, 59.0f}, {59.0f, 65.0f}, {66.0f, 59.0f},
        {73.0f, 64.0f}, {81.0f, 59.0f}, {88.0f, 64.0f}, {93.0f, 56.0f},
        {18.0f, 5.0f}, {24.0f, 8.0f}, {31.0f, 5.5f}, {49.0f, 6.0f},
        {57.0f, 5.0f}, {62.0f, 3.5f}, {4.0f, 14.0f}, {16.0f, 14.5f},
        {17.0f, 45.0f}, {20.0f, 52.0f}, {57.5f, 41.0f}, {74.0f, 39.0f},
        {86.0f, 40.0f}, {93.0f, 36.0f}
    };
    for (int32_t i = 0; i < (int32_t)(sizeof(trees) / sizeof(trees[0])); ++i) {
        Color leaves = (i & 1) != 0 ? (Color){54, 105, 85, 255} :
                                      (Color){58, 119, 91, 255};
        DrawTree(trees[i].x, trees[i].y, leaves);
    }
}

static void DrawLabels(const WorldLabel *labels, int32_t count, Camera3D camera,
                       int32_t width, int32_t height)
{
    for (int32_t i = 0; i < count; ++i) {
        Vector2 screen = GetWorldToScreenEx(labels[i].point, camera, width, height);
        if (screen.x < -120.0f || screen.x > (float)width + 120.0f ||
            screen.y < -40.0f || screen.y > (float)height + 40.0f) continue;
        int text_width = MeasureText(labels[i].text, 10);
        DrawRectangleRounded((Rectangle){screen.x - (float)text_width * 0.5f - 5.0f,
                                         screen.y - 5.0f,
                                         (float)text_width + 10.0f, 16.0f},
                             0.30f, 4, (Color){4, 10, 14, 210});
        DrawText(labels[i].text, (int)screen.x - text_width / 2,
                 (int)screen.y - 2, 10, labels[i].color);
    }
}

static void DrawCombatBar(const CcLocalAgent *agent, Camera3D camera,
                          int32_t width, int32_t height, Color accent)
{
    Vector3 anchor = {agent->position.x, agent->position.y + 2.18f,
                      agent->position.z};
    Vector2 screen = GetWorldToScreenEx(anchor, camera, width, height);
    const float bar_width = 44.0f;
    float health = CombatClamp(agent->combat.health /
                               CC_LOCAL_COMBAT_MAX_HEALTH, 0.0f, 1.0f);
    float posture = CombatClamp(agent->combat.posture /
                                CC_LOCAL_COMBAT_MAX_POSTURE, 0.0f, 1.0f);
    DrawRectangle((int)(screen.x - bar_width * 0.5f), (int)screen.y,
                  (int)bar_width, 5, (Color){5, 11, 15, 220});
    DrawRectangle((int)(screen.x - bar_width * 0.5f), (int)screen.y,
                  (int)(bar_width * health), 3,
                  agent->combat.defeated ? WORLD_MUTED : accent);
    DrawRectangle((int)(screen.x - bar_width * 0.5f), (int)screen.y + 4,
                  (int)(bar_width * posture), 2, WORLD_GOLD);
    if (agent->combat.defeated) {
        int label_width = MeasureText("DOWN", 8);
        DrawText("DOWN", (int)screen.x - label_width / 2,
                 (int)screen.y - 9, 8, WORLD_DANGER);
    }
}

static void DrawCombatImpact(const CcLocalAgent *agent)
{
    if (agent->combat.hit_flash_seconds <= 0.0f) return;
    float pulse = 0.42f + agent->combat.hit_flash_seconds * 0.85f;
    DrawSphereWires((Vector3){agent->position.x, agent->position.y + 1.02f,
                              agent->position.z},
                    pulse, 8, 8, WORLD_INK);
}

static void PresentTarget(RenderTexture2D target, Rectangle destination)
{
    Rectangle source = {0.0f, 0.0f, (float)target.texture.width,
                        -(float)target.texture.height};
    DrawTexturePro(target.texture, source, destination, (Vector2){0.0f, 0.0f},
                   0.0f, WHITE);
}

static void DrawAgentPath(const CcLocalAgent *agent, bool market_interior)
{
    (void)market_interior;
    if (!agent->exact_target_valid && !agent->target_valid) return;
    Vector3 target = agent->target_point;
    DrawCylinder((Vector3){target.x, target.y + 0.018f, target.z},
                 0.24f, 0.24f, 0.036f, 24, Fade(WORLD_GOLD, 0.42f));
    DrawCylinderWires((Vector3){target.x, target.y + 0.020f, target.z},
                      0.26f, 0.26f, 0.040f, 24, WORLD_GOLD);
    DrawLine3D((Vector3){agent->position.x, agent->position.y + 0.04f,
                         agent->position.z},
               (Vector3){target.x, target.y + 0.04f, target.z},
               Fade(WORLD_GOLD, 0.30f));
}

static Color CoursePlatformColor(int32_t style)
{
    switch (style) {
        case 1: return (Color){87, 113, 105, 255};
        case 2: return (Color){98, 79, 118, 255};
        case 3: return (Color){126, 82, 74, 255};
        default: return (Color){96, 121, 111, 255};
    }
}

static void DrawObstacleCourse(void)
{
    float pool_center_x = COURSE_POOL.x + COURSE_POOL.width * 0.5f;
    float pool_center_z = COURSE_POOL.y + COURSE_POOL.height * 0.5f;
    DrawBox((Vector3){pool_center_x, 0.36f, pool_center_z},
            (Vector3){COURSE_POOL.width, 0.72f, COURSE_POOL.height},
            (Color){19, 57, 73, 255});
    DrawBox((Vector3){pool_center_x, COURSE_WATER_SURFACE, pool_center_z},
            (Vector3){COURSE_POOL.width - 0.10f, 0.035f,
                      COURSE_POOL.height - 0.10f},
            (Color){48, 151, 167, 205});
    DrawBox((Vector3){pool_center_x, 0.43f, COURSE_POOL.y - 0.06f},
            (Vector3){COURSE_POOL.width + 0.18f, 0.86f, 0.12f},
            (Color){86, 104, 102, 255});
    DrawBox((Vector3){pool_center_x, 0.43f,
                      COURSE_POOL.y + COURSE_POOL.height + 0.06f},
            (Vector3){COURSE_POOL.width + 0.18f, 0.86f, 0.12f},
            (Color){86, 104, 102, 255});
    for (int32_t i = 0; i < (int32_t)(sizeof(COURSE_WAYPOINTS) /
                                      sizeof(COURSE_WAYPOINTS[0])); ++i) {
        Vector3 point = COURSE_WAYPOINTS[i];
        point.y = SurfaceHeightAt(false, point.x, point.z) + 0.035f;
        DrawCylinder(point, 0.075f, 0.075f, 0.025f, 10,
                     Fade(WORLD_GOLD, (i & 1) != 0 ? 0.52f : 0.30f));
    }
    DrawBox((Vector3){9.32f, 0.76f, 1.08f}, (Vector3){0.08f, 1.52f, 0.08f},
            (Color){72, 58, 55, 255});
    DrawBox((Vector3){9.32f, 0.76f, 1.82f}, (Vector3){0.08f, 1.52f, 0.08f},
            (Color){72, 58, 55, 255});
    DrawBox((Vector3){9.32f, 1.48f, 1.45f}, (Vector3){0.08f, 0.08f, 0.82f},
            WORLD_GOLD);
    DrawBox((Vector3){9.28f, 1.20f, 1.45f}, (Vector3){0.035f, 0.42f, 0.52f},
            (Color){73, 55, 91, 255});
}

static void DrawCourseRunners(const CcLocalCourse *course)
{
    if (course == NULL) return;
    for (int32_t i = 0; i < CC_LOCAL_TRAVELLER_COUNT; ++i) {
        const CcLocalTraveller *traveller = &course->travellers[i];
        if (traveller->active) DrawRobotShell(&traveller->agent);
    }
    Vector3 threat = CourseThreatCenter(course);
    if (course->alarm_active) {
        for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
            const CcLocalAgent *raider = &course->raiders[i];
            DrawRobotShell(raider);
            DrawCombatImpact(raider);
            const CcHumanoidPose *pose = AgentRenderPose(raider);
            Vector3 hand = FromLimbVector(pose->hand[1]);
            Vector3 elbow = FromLimbVector(pose->elbow[1]);
            Vector3 club_direction = PhysicsNormalizeOr(
                PhysicsSubtract(hand, elbow),
                (Vector3){sinf(raider->facing_yaw), 0.0f,
                          cosf(raider->facing_yaw)});
            Vector3 club_end = PhysicsAdd(
                hand, PhysicsScale(club_direction, 0.48f));
            DrawCylinderEx(hand, club_end, 0.035f, 0.052f, 7,
                           (Color){82, 58, 45, 255});
            DrawSphereWires((Vector3){raider->position.x,
                                      raider->position.y + 2.05f,
                                      raider->position.z},
                            0.075f, 6, 6, WORLD_DANGER);
        }
    }
    for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
        const CcLocalCourseRunner *runner = &course->runners[i];
        if (runner->agent.exact_target_valid) {
            Vector3 target = runner->agent.target_point;
            DrawCylinderWires((Vector3){target.x, target.y + 0.025f, target.z},
                              0.13f, 0.13f, 0.035f, 14,
                              Fade(runner->marker_color, 0.58f));
        }
        DrawRobotShell(&runner->agent);
        DrawCombatImpact(&runner->agent);
        if (!runner->agent.climbing &&
            !runner->agent.humanoid.ragdoll.active) {
            const CcHumanoidPose *pose = AgentRenderPose(&runner->agent);
            Vector3 hand = FromLimbVector(pose->hand[1]);
            Vector3 elbow = FromLimbVector(pose->elbow[1]);
            Vector3 aim = {sinf(runner->agent.facing_yaw), 0.0f,
                           cosf(runner->agent.facing_yaw)};
            if (course->alarm_active) {
                aim = PhysicsNormalizeOr(
                    PhysicsSubtract(threat, hand), aim);
                aim.y = runner->duty == CC_GUARD_ENGAGED ? 0.06f : 0.16f;
                aim = PhysicsNormalizeOr(aim,
                    (Vector3){0.0f, 0.0f, 1.0f});
            }
            Vector3 forearm = PhysicsNormalizeOr(
                PhysicsSubtract(hand, elbow), aim);
            float arm_weight = runner->duty == CC_GUARD_ENGAGED ? 0.78f : 0.42f;
            Vector3 spear_direction = PhysicsNormalizeOr(
                PhysicsLerp(aim, forearm, arm_weight), aim);
            float spear_length = runner->duty == CC_GUARD_ENGAGED ? 0.78f :
                                                                         0.58f;
            Vector3 spear_tip = PhysicsAdd(
                hand, PhysicsScale(spear_direction, spear_length));
            DrawCylinderEx(hand, spear_tip, 0.022f, 0.013f, 7,
                           (Color){128, 92, 55, 255});
            DrawSmallSphere(spear_tip, 0.038f, WORLD_GOLD);
            Vector3 shield_hand = FromLimbVector(pose->hand[0]);
            DrawCharacterSphere(shield_hand, 0.105f,
                                (Color){55, 74, 78, 255});
            DrawSphereWires(shield_hand, 0.108f, 6, 6,
                            runner->marker_color);
        }
        DrawSphereWires((Vector3){runner->agent.position.x,
                                  runner->agent.position.y + 2.05f,
                                  runner->agent.position.z},
                        0.075f, 6, 6, runner->marker_color);
    }
}

void CcLocalDrawStreet3D(const CcSim *sim, const CcLocalAgent *agent,
                         const CcLocalCourse *course, float clock,
                         RenderTexture2D target, Rectangle destination)
{
    const CcSettlement *place = CcSimSettlement(sim, sim->player.location_id);
    if (place == NULL) return;
    Camera3D camera = LocalCamera(false, agent->position);
    Color kingdom = KingdomColor3D(sim, place->kingdom_id);
    BeginTextureMode(target);
    ClearBackground((Color){10, 24, 30, 255});
    BeginMode3D(camera);

    DrawExteriorTerrain();
    for (int32_t i = 0; i < (int32_t)(sizeof(STREET_PLATFORMS) /
                                      sizeof(STREET_PLATFORMS[0])); ++i) {
        const NavPlatform *platform = &STREET_PLATFORMS[i];
        Color color = CoursePlatformColor(platform->style);
        DrawBox((Vector3){platform->x + platform->width * 0.5f,
                          platform->height * 0.5f,
                          platform->z + platform->depth * 0.5f},
                (Vector3){fmaxf(0.10f, platform->width - 0.10f),
                          platform->height,
                          fmaxf(0.10f, platform->depth - 0.10f)}, color);
        DrawBox((Vector3){platform->x + platform->width * 0.5f,
                          platform->height + 0.025f,
                          platform->z + platform->depth * 0.5f},
                (Vector3){fmaxf(0.10f, platform->width - 0.04f), 0.05f,
                          fmaxf(0.10f, platform->depth - 0.04f)},
                i == 0 ? (Color){124, 145, 131, 255} :
                         Fade(WORLD_GOLD, 0.70f));
    }
    DrawObstacleCourse();
    DrawAgentPath(agent, false);
    DrawWorldBuildings(kingdom);
    DrawCastle(kingdom);
    DrawCarriage3D();
    DrawNotice3D(sim);
    DrawWorldTrees();

    int32_t crates = place->stock[CC_GOOD_FOOD] / 12;
    if (crates > 4) crates = 4;
    for (int32_t i = 0; i < crates; ++i) {
        DrawBox((Vector3){44.40f + (float)(i % 2) * 0.72f, 0.30f,
                          26.75f + (float)(i / 2) * 0.72f},
                (Vector3){0.62f, 0.60f, 0.62f},
                (Color){177, 116, 55, 255});
    }
    const CcDungeon *dungeon = DungeonAt(sim, place->id);
    if (dungeon != NULL) DrawDungeon3D(dungeon);

    DrawPuppet3D((Vector3){STREET_PEOPLE[0].x, 0.0f, STREET_PEOPLE[0].y},
                 0.88f, -0.55f,
                 (Color){223, 151, 68, 255}, WORLD_TEAL, clock * 1.2f,
                 CC_TRAVERSAL_WALK, false);
    DrawPuppet3D((Vector3){STREET_PEOPLE[1].x, 0.0f, STREET_PEOPLE[1].y},
                 0.92f, 1.70f, kingdom,
                 WORLD_GOLD, clock + 1.0f, CC_TRAVERSAL_WALK, false);
    DrawPuppet3D((Vector3){STREET_PEOPLE[2].x, 0.0f, STREET_PEOPLE[2].y},
                 0.86f, 0.35f,
                 (Color){97, 154, 137, 255}, WORLD_TEAL, clock + 2.0f,
                 CC_TRAVERSAL_WALK, false);
    DrawPuppet3D((Vector3){STREET_PEOPLE[3].x, 0.0f, STREET_PEOPLE[3].y},
                 0.82f, 2.40f,
                 (Color){168, 112, 128, 255}, WORLD_TEAL, clock * 0.8f + 3.0f,
                 CC_TRAVERSAL_WALK, false);
    bool hungry_crowd = place->hunger >= 30;
    DrawPuppet3D((Vector3){STREET_PEOPLE[4].x, 0.0f, STREET_PEOPLE[4].y},
                 0.76f, -0.40f,
                 hungry_crowd ? (Color){91, 102, 104, 255} : kingdom,
                 hungry_crowd ? WORLD_DANGER : WORLD_TEAL, clock * 0.6f,
                 CC_TRAVERSAL_WALK, false);
    bool underworld_present = HasSmugglerRoad(sim, place->id) ||
                              place->security < 50;
    DrawPuppet3D((Vector3){STREET_PEOPLE[5].x, 0.0f, STREET_PEOPLE[5].y},
                 0.82f, 2.75f,
                 underworld_present ? (Color){74, 60, 91, 255} : kingdom,
                 underworld_present ? WORLD_VIOLET : WORLD_GOLD, clock * 0.7f,
                 CC_TRAVERSAL_WALK, false);
    DrawCourseRunners(course);
    DrawRobotShell(agent);
    DrawCombatImpact(agent);
    EndMode3D();

    WorldLabel labels[8];
    int32_t count = 0;
    labels[count++] = (WorldLabel){{agent->position.x,
                                    agent->position.y + 2.30f,
                                    agent->position.z}, "YOU", WORLD_TEAL};
    labels[count++] = (WorldLabel){{50.0f, 9.15f, 21.0f},
                                   "MARKET HALL", WORLD_GOLD};
    labels[count++] = (WorldLabel){{36.80f, 2.28f, 31.70f},
                                   "CROWNLESS CARRIAGE", WORLD_GOLD};
    labels[count++] = (WorldLabel){{CC_LOCAL_NOTICE_X, 1.82f,
                                    CC_LOCAL_NOTICE_Z},
                                   "SITUATIONS", WORLD_INK};
    labels[count++] = (WorldLabel){{11.80f, 2.05f, 0.82f},
                                   "WAYFARER TRIALS", WORLD_GOLD};
    labels[count++] = (WorldLabel){{11.28f, 1.12f, 9.72f},
                                   "BUOYANCY TRENCH", WORLD_TEAL};
    labels[count++] = (WorldLabel){{78.50f, 12.10f, 17.50f},
                                   "GREYWARD KEEP", kingdom};
    if (dungeon != NULL) {
        labels[count++] = (WorldLabel){{CC_LOCAL_DUNGEON_X, 3.38f,
                                        CC_LOCAL_DUNGEON_Z - 0.70f},
                                       CcDungeonStateName(dungeon->state), WORLD_VIOLET};
    }
    DrawLabels(labels, count, camera, target.texture.width, target.texture.height);
    if (course != NULL && course->alarm_active) {
        DrawCombatBar(agent, camera, target.texture.width,
                      target.texture.height, WORLD_TEAL);
        for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
            DrawCombatBar(&course->runners[i].agent, camera,
                          target.texture.width, target.texture.height,
                          course->runners[i].marker_color);
        }
        for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
            DrawCombatBar(&course->raiders[i], camera, target.texture.width,
                          target.texture.height, WORLD_DANGER);
        }
    }
    if (agent->morphology == CC_MORPHOLOGY_BIPED) {
        DrawText(TextFormat("BIOMECHANICAL BIPED / %d JOINTS / %s / %s / MUSCLES + LIGAMENTS",
                            agent->humanoid.body.morphology.joint_count,
                            CcLocalTraversalName(agent->traversal),
                            CcHumanoidActionName(agent->humanoid.action)),
                 18, 18, 10, WORLD_TEAL);
    } else {
        DrawText(TextFormat("ROBOTIC %s / %d LEGS / %s / CONTACT IK",
                            CcLocalAgentMorphologyName(agent),
                            agent->limb_rig.morphology.limb_count,
                            CcLocalTraversalName(agent->traversal)),
                 18, 18, 10, WORLD_TEAL);
    }
    if (course != NULL && course->alarm_active) {
        bool line_engaged = false;
        for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
            line_engaged = line_engaged ||
                           course->runners[i].duty == CC_GUARD_ENGAGED;
        }
        DrawText(TextFormat("VILLAGE ALARM / YOU %d HP / %d POSTURE / GUARDS %s / RAIDERS %d%%",
                            (int32_t)lroundf(agent->combat.health),
                            (int32_t)lroundf(agent->combat.posture),
                            course->raiders_retreating ? "DRIVING THEM OUT" :
                            line_engaged ? "ENGAGED" : "FORMING LINE",
                            course->raider_resolve > 0 ?
                            course->raider_resolve : 0),
                 18, 33, 10, WORLD_DANGER);
        if (course->combat_event_seconds > 0.0f) {
            DrawText(TextFormat("%s / %s",
                                CcLocalCombatTeamName(
                                    course->last_attacker_team),
                                CcLocalCombatOutcomeName(course->last_outcome)),
                     18, 48, 12,
                     course->last_outcome == CC_COMBAT_OUTCOME_BLOCKED ?
                     WORLD_GOLD : WORLD_INK);
        }
    }
    EndTextureMode();
    PresentTarget(target, destination);
}

void CcLocalDrawMarket3D(const CcSim *sim, const CcLocalAgent *agent, float clock,
                         RenderTexture2D target, Rectangle destination)
{
    const CcSettlement *place = CcSimSettlement(sim, sim->player.location_id);
    if (place == NULL) return;
    Camera3D camera = LocalCamera(true, agent->position);
    BeginTextureMode(target);
    ClearBackground((Color){31, 23, 25, 255});
    BeginMode3D(camera);
    for (int32_t z = 0; z < 7; ++z) {
        for (int32_t x = 0; x < 9; ++x) {
            Color tile = ((x + z) & 1) ? (Color){116, 87, 64, 255} :
                                            (Color){100, 76, 61, 255};
            DrawGroundTile((float)x, (float)z, tile);
        }
    }
    DrawAgentPath(agent, true);
    DrawBox((Vector3){4.50f, 1.30f, 0.25f}, (Vector3){9.0f, 2.60f, 0.50f},
            (Color){80, 53, 48, 255});
    DrawBox((Vector3){0.25f, 1.30f, 3.50f}, (Vector3){0.50f, 2.60f, 7.0f},
            (Color){67, 48, 47, 255});
    DrawBox((Vector3){MARKET_COUNTER_FOOTPRINT.x +
                      MARKET_COUNTER_FOOTPRINT.width * 0.5f,
                      0.46f,
                      MARKET_COUNTER_FOOTPRINT.y +
                      MARKET_COUNTER_FOOTPRINT.height * 0.5f},
            (Vector3){MARKET_COUNTER_FOOTPRINT.width, 0.92f,
                      MARKET_COUNTER_FOOTPRINT.height},
            (Color){139, 85, 49, 255});
    DrawBox((Vector3){MARKET_SHELF_FOOTPRINT.x + MARKET_SHELF_FOOTPRINT.width * 0.5f,
                      0.95f,
                      MARKET_SHELF_FOOTPRINT.y + MARKET_SHELF_FOOTPRINT.height * 0.5f},
            (Vector3){MARKET_SHELF_FOOTPRINT.width, 1.90f,
                      MARKET_SHELF_FOOTPRINT.height},
            (Color){103, 68, 49, 255});
    DrawBox((Vector3){1.55f, 1.05f, 6.54f}, (Vector3){0.82f, 2.10f, 0.08f},
            (Color){37, 28, 30, 255});
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        int32_t count = place->stock[good] / 18;
        if (count > 3) count = 3;
        for (int32_t i = 0; i < count; ++i) {
            Color color = good == CC_GOOD_FOOD ? WORLD_GOLD :
                          good == CC_GOOD_MATERIAL ? (Color){170, 139, 112, 255} :
                          WORLD_TEAL;
            DrawBox((Vector3){2.55f + (float)good * 1.02f, 0.25f,
                              1.18f + (float)i * 0.55f},
                    (Vector3){0.48f, 0.50f, 0.48f}, color);
        }
    }
    DrawPuppet3D((Vector3){MARKET_PEOPLE[0].x, 0.0f, MARKET_PEOPLE[0].y},
                 0.96f, 2.75f,
                 (Color){218, 148, 61, 255}, WORLD_TEAL, clock,
                 CC_TRAVERSAL_WALK, false);
    DrawRobotShell(agent);
    EndMode3D();
    WorldLabel labels[] = {
        {{6.55f, 2.05f, 1.60f}, "MARA / FACTOR", WORLD_GOLD},
        {{1.55f, 2.25f, 6.54f}, "STREET", WORLD_MUTED}
    };
    DrawLabels(labels, 2, camera, target.texture.width, target.texture.height);
    if (agent->morphology == CC_MORPHOLOGY_BIPED) {
        DrawText(TextFormat("BIO BIPED / %s + %s / %.0f%% MUSCLE ACTIVATION",
                            CcHumanoidContactName(agent->humanoid.feet[0].contact),
                            CcHumanoidContactName(agent->humanoid.feet[1].contact),
                            CcBiomechRigMeanActivation(&agent->humanoid.body) *
                                100.0f),
                 18, 18, 10, WORLD_TEAL);
    } else {
        DrawText(TextFormat("ROBOTIC %s / %d LEGS / PLANTED CONTACTS",
                            CcLocalAgentMorphologyName(agent),
                            agent->limb_rig.morphology.limb_count),
                 18, 18, 10, WORLD_TEAL);
    }
    EndTextureMode();
    PresentTarget(target, destination);
}

static bool InsideExpanded(Vector2 point, Rectangle footprint, float radius)
{
    return point.x > footprint.x - radius &&
           point.x < footprint.x + footprint.width + radius &&
           point.y > footprint.y - radius &&
           point.y < footprint.y + footprint.height + radius;
}

static bool StreetCanOccupy(Vector2 point)
{
    const float radius = PLAYER_COLLISION_RADIUS;
    if (point.x < 0.28f + radius ||
        point.x > CC_LOCAL_WORLD_WIDTH - 0.28f - radius ||
        point.y < 0.28f + radius ||
        point.y > CC_LOCAL_WORLD_DEPTH - 0.28f - radius) return false;
    for (int32_t i = 0; i < (int32_t)(sizeof(WORLD_BUILDINGS) /
                                      sizeof(WORLD_BUILDINGS[0])); ++i) {
        if (InsideExpanded(point, WORLD_BUILDINGS[i].footprint, radius)) {
            return false;
        }
    }
    for (int32_t i = 0; i < (int32_t)(sizeof(CASTLE_STRUCTURES) /
                                      sizeof(CASTLE_STRUCTURES[0])); ++i) {
        if (InsideExpanded(point, CASTLE_STRUCTURES[i].footprint, radius)) {
            return false;
        }
    }
    if (InsideExpanded(point, CARRIAGE_FOOTPRINT, radius) ||
        InsideExpanded(point, DUNGEON_FOOTPRINT, radius)) return false;
    return true;
}

static bool MarketCanOccupy(Vector2 point)
{
    const float radius = PLAYER_COLLISION_RADIUS;
    if (point.x < 0.52f + radius || point.x > 8.72f - radius ||
        point.y < 0.52f + radius || point.y > 6.72f - radius) return false;
    if (InsideExpanded(point, MARKET_COUNTER_FOOTPRINT, radius)) return false;
    if (InsideExpanded(point, MARKET_SHELF_FOOTPRINT, radius)) return false;
    return true;
}

Vector2 CcLocalMove(Vector2 current, Vector2 delta, bool market_interior)
{
    bool (*can_occupy)(Vector2) = market_interior ? MarketCanOccupy : StreetCanOccupy;
    Vector2 candidate = {current.x + delta.x, current.y};
    if (can_occupy(candidate)) current.x = candidate.x;
    candidate = (Vector2){current.x, current.y + delta.y};
    if (can_occupy(candidate)) current.y = candidate.y;
    return current;
}
