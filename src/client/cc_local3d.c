#include "client/cc_local3d.h"

#include "rlgl.h"

#include <float.h>
#include <math.h>
#include <stdint.h>

static const Color WORLD_VOID = {7, 14, 21, 255};
static const Color WORLD_INK = {231, 232, 211, 255};
static const Color WORLD_MUTED = {132, 154, 148, 255};
static const Color WORLD_TEAL = {104, 234, 207, 255};
static const Color WORLD_GOLD = {249, 197, 75, 255};
static const Color WORLD_DANGER = {218, 75, 86, 255};
static const Color WORLD_VIOLET = {195, 105, 221, 255};
static const float PLAYER_COLLISION_RADIUS = 0.30f;
static const float PERSON_COLLISION_RADIUS = 0.27f;

typedef struct WorldLabel {
    Vector3 point;
    const char *text;
    Color color;
} WorldLabel;

static const Rectangle STREET_BUILDINGS[] = {
    {0.65f, 1.10f, 2.05f, 1.75f},
    {3.85f, 0.50f, 2.15f, 1.55f},
    {6.60f, 1.00f, 2.35f, 2.00f},
    {7.05f, 5.15f, 2.10f, 1.85f}
};
static const float STREET_BUILDING_HEIGHTS[] = {3.20f, 4.20f, 3.40f, 3.60f};
static const Rectangle CARRIAGE_FOOTPRINT = {0.575f, 6.05f, 1.55f, 1.00f};
static const Rectangle MARKET_COUNTER_FOOTPRINT = {6.05f, 1.84f, 2.10f, 0.72f};
static const Rectangle MARKET_SHELF_FOOTPRINT = {1.10f, 1.175f, 0.72f, 3.45f};
static const Vector2 STREET_PEOPLE[] = {
    {6.15f, 4.10f}, {4.10f, 3.65f}, {5.55f, 6.15f}, {2.15f, 4.35f},
    {6.00f, 4.80f}, {8.55f, 4.65f}
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

static Camera3D LocalCamera(bool interior);

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
    float maximum_x = market_interior ? 8.72f : 15.72f;
    float maximum_z = market_interior ? 6.72f : 10.72f;
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
    for (int32_t i = 0; i < (int32_t)(sizeof(STREET_BUILDINGS) /
                                      sizeof(STREET_BUILDINGS[0])); ++i) {
        if (CircleTouchesFootprint(x, z, radius, STREET_BUILDINGS[i])) return true;
    }
    if (CircleTouchesFootprint(x, z, radius, CARRIAGE_FOOTPRINT)) return true;
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
        case CC_TRAVERSAL_CLIMB: return "CLIMB";
        case CC_TRAVERSAL_DESCEND: return "DOWN-CLIMB";
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
    } else {
        agent->humanoid = (CcHumanoidGait){0};
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

void CcLocalCourseInit(CcLocalCourse *course)
{
    static const int32_t starts[CC_LOCAL_COURSE_RUNNER_COUNT] = {0, 6, 12};
    static const Color colors[CC_LOCAL_COURSE_RUNNER_COUNT] = {
        {50, 151, 160, 255}, {166, 91, 132, 255}, {176, 122, 54, 255}
    };
    const int32_t waypoint_count = (int32_t)(sizeof(COURSE_WAYPOINTS) /
                                              sizeof(COURSE_WAYPOINTS[0]));
    *course = (CcLocalCourse){0};
    for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
        const Vector3 start = COURSE_WAYPOINTS[starts[i]];
        CcLocalCourseRunner *runner = &course->runners[i];
        CcLocalAgentInit(&runner->agent, (Vector2){start.x, start.z}, false);
        runner->agent.crowned = false;
        runner->agent.tunic_color = colors[i];
        runner->marker_color = colors[i];
        runner->duty = CC_GUARD_TRAINING;
        runner->next_waypoint = (starts[i] + 1) % waypoint_count;
        runner->pause_seconds = 0.20f + (float)i * 0.12f;
        if (CcLocalAgentSetExactTarget(
                &runner->agent, COURSE_WAYPOINTS[runner->next_waypoint], false)) {
            runner->next_waypoint = (runner->next_waypoint + 1) % waypoint_count;
        }
    }
    for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
        CcLocalAgentInit(&course->raiders[i],
                         (Vector2){15.05f, 4.55f + (float)i * 0.48f}, false);
        course->raiders[i].crowned = false;
        course->raiders[i].tunic_color = (Color){126, 55, 61, 255};
    }
    course->alarm_countdown = 8.0f;
}

void CcLocalCourseRaiseAlarm(CcLocalCourse *course)
{
    if (course->alarm_active) return;
    course->alarm_active = true;
    course->raiders_retreating = false;
    course->engagement_time = 0.0f;
    course->strike_timer = 0.55f;
    course->raider_resolve = 100;
    for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
        course->runners[i].duty = CC_GUARD_RESPONDING;
        course->runners[i].response_stage = 0;
        course->runners[i].response_waypoint_active = false;
        course->runners[i].agent.exact_target_valid = false;
    }
    for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
        CcLocalAgent *raider = &course->raiders[i];
        CcLocalAgentInit(raider,
                         (Vector2){15.05f, 4.55f + (float)i * 0.48f}, false);
        raider->crowned = false;
        raider->tunic_color = (Color){126, 55, 61, 255};
        (void)CcLocalAgentSetExactTarget(
            raider, (Vector3){9.35f, 0.0f, 4.60f + (float)i * 0.40f}, false);
    }
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

static bool CourseResponseWaypoint(int32_t guard, int32_t stage,
                                   Vector3 *waypoint)
{
    static const Vector3 west_route[] = {
        {9.25f, 0.0f, 4.72f}
    };
    static const Vector3 south_route[] = {
        {13.10f, 0.0f, 6.95f}, {14.35f, 0.0f, 6.95f},
        {14.45f, 0.0f, 5.18f}
    };
    const Vector3 *route = NULL;
    int32_t count = 0;
    if (guard == 0) {
        route = west_route;
        count = (int32_t)(sizeof(west_route) / sizeof(west_route[0]));
    } else if (guard == 2) {
        route = south_route;
        count = (int32_t)(sizeof(south_route) / sizeof(south_route[0]));
    }
    if (stage < 0 || stage >= count) return false;
    *waypoint = route[stage];
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

void CcLocalCourseUpdate(CcLocalCourse *course, const CcSim *sim,
                         float delta_time)
{
    delta_time = fminf(delta_time, 1.0f / 30.0f);
    if (!course->alarm_active) {
        course->alarm_countdown -= delta_time;
        if (course->alarm_countdown <= 0.0f) {
            CcLocalCourseRaiseAlarm(course);
        } else {
            UpdateCourseTraining(course, delta_time);
            return;
        }
    }

    Vector3 threat = CourseThreatCenter(course);
    static const float guard_z[CC_LOCAL_COURSE_RUNNER_COUNT] = {
        4.42f, 4.78f, 5.14f
    };
    for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
        CcLocalCourseRunner *runner = &course->runners[i];
        if (!CourseAgentBusy(&runner->agent)) {
            Vector3 response_waypoint;
            bool has_response_waypoint = CourseResponseWaypoint(
                i, runner->response_stage, &response_waypoint);
            if (has_response_waypoint &&
                !runner->response_waypoint_active) {
                if (CcLocalAgentSetExactTarget(
                        &runner->agent, response_waypoint, false)) {
                    runner->response_waypoint_active = true;
                }
            } else if (has_response_waypoint &&
                       runner->response_waypoint_active &&
                       !runner->agent.exact_target_valid) {
                runner->response_stage += 1;
                runner->response_waypoint_active = false;
            } else if (!has_response_waypoint) {
                Vector3 guard_target = course->raiders_retreating ?
                (Vector3){12.05f, 0.0f, guard_z[i]} :
                (Vector3){threat.x - 0.72f, 0.0f,
                          threat.z + ((float)i - 1.0f) * 0.52f};
                (void)CcLocalAgentSetExactTarget(&runner->agent,
                                                  guard_target, false);
            }
        }
        CcLocalAgentUpdate(&runner->agent, delta_time, false);
    }

    for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
        CcLocalAgent *raider = &course->raiders[i];
        if (!CourseAgentBusy(raider)) {
            Vector3 raider_target = course->raiders_retreating ?
                (Vector3){15.10f, 0.0f, 4.55f + (float)i * 0.48f} :
                course->engagement_time > 0.0f ? raider->position :
                (Vector3){9.35f, 0.0f, 4.60f + (float)i * 0.40f};
            (void)CcLocalAgentSetExactTarget(raider, raider_target, false);
        }
        CcLocalAgentUpdate(raider, delta_time, false);
    }

    threat = CourseThreatCenter(course);
    int32_t engaged_guards = 0;
    if (!course->raiders_retreating) {
        for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
            CcLocalCourseRunner *runner = &course->runners[i];
            float dx = runner->agent.position.x - threat.x;
            float dz = runner->agent.position.z - threat.z;
            if (dx * dx + dz * dz < 0.92f * 0.92f) {
                runner->duty = CC_GUARD_ENGAGED;
                engaged_guards += 1;
            } else {
                runner->duty = CC_GUARD_RESPONDING;
            }
        }
        if (engaged_guards > 0) {
            course->engagement_time += delta_time;
        }
        if (engaged_guards == CC_LOCAL_COURSE_RUNNER_COUNT) {
            course->strike_timer -= delta_time;
            if (course->strike_timer <= 0.0f) {
                course->raider_resolve -= engaged_guards * 13;
                course->strike_timer = 0.62f;
            }
        }
        if (course->raider_resolve <= 0 || course->engagement_time > 25.0f) {
            course->raiders_retreating = true;
            for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
                course->runners[i].duty = CC_GUARD_RETURNING;
            }
        }
        return;
    }

    bool raiders_clear = true;
    for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
        if (course->raiders[i].position.x < 14.75f) raiders_clear = false;
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
    Camera3D camera = LocalCamera(market_interior);
    Ray ray = GetScreenToWorldRayEx(local, camera, target.texture.width,
                                    target.texture.height);
    float nearest = FLT_MAX;
    Vector3 picked_point = {0};
    BoundingBox ground = {
        .min = {0.0f, -0.08f, 0.0f},
        .max = {market_interior ? 9.0f : 16.0f, 0.01f,
                market_interior ? 7.0f : 11.0f}
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
        for (int32_t i = 0; i < (int32_t)(sizeof(STREET_BUILDINGS) /
                                          sizeof(STREET_BUILDINGS[0])); ++i) {
            occluder = fminf(occluder,
                             RayFootprintDistance(ray, STREET_BUILDINGS[i],
                                                  STREET_BUILDING_HEIGHTS[i]));
        }
        occluder = fminf(occluder,
                         RayFootprintDistance(ray, CARRIAGE_FOOTPRINT, 1.92f));
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

void CcLocalAgentUpdate(CcLocalAgent *agent, float delta_time, bool market_interior)
{
    const float gravity = 9.81f;
    delta_time = fminf(delta_time, 1.0f / 30.0f);
    if (agent->climbing) {
        UpdateClimb(agent, delta_time, market_interior);
        return;
    }
    bool biped = agent->morphology == CC_MORPHOLOGY_BIPED;
    LocalProbeContext context = {.market_interior = market_interior};
    if (biped && agent->humanoid_needs_reset) {
        CcHumanoidGaitInit(&agent->humanoid, ToLimbVector(agent->position),
                            agent->facing_yaw, ProbeLocalSurface, &context);
        agent->humanoid_needs_reset = false;
    }
    float traction = agent->limb_rig.initialized ? agent->limb_rig.traction : 1.0f;
    float base_speed = biped ? 1.45f : 2.35f;
    float maximum_speed = biped ? base_speed :
                          base_speed * (0.68f + traction * 0.32f);
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
        if (!(biped && agent->humanoid.ragdoll.active)) {
            float target_yaw = atan2f(direction.x, direction.z);
            float difference = WrapAngle(target_yaw - agent->facing_yaw);
            agent->facing_yaw = WrapAngle(
                agent->facing_yaw +
                difference * fminf(1.0f, delta_time * 10.0f));
        }
    }
    if (biped) {
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
            return;
        }
    }
    agent->velocity.x = (agent->position.x - previous_position.x) / delta_time;
    agent->velocity.z = (agent->position.z - previous_position.z) / delta_time;

    agent->velocity.y -= gravity * delta_time;
    agent->position.y += agent->velocity.y * delta_time;
    float surface = BodySurfaceHeightAt(market_interior, agent->position.x,
                                        agent->position.z);
    bool landed = false;
    if (agent->position.y <= surface && agent->velocity.y <= 0.0f) {
        landed = !agent->grounded;
        agent->position.y = surface;
        agent->velocity.y = 0.0f;
        agent->grounded = true;
    } else {
        agent->grounded = false;
    }
    if (biped) {
        CcHumanoidGaitConstrainMotion(&agent->humanoid,
                                      ToLimbVector(agent->position),
                                      ToLimbVector(agent->velocity),
                                      agent->grounded);
    }

    float horizontal_speed = sqrtf(agent->velocity.x * agent->velocity.x +
                                   agent->velocity.z * agent->velocity.z);
    if (biped && agent->humanoid.ragdoll.active) {
        agent->traversal = agent->humanoid.recovering ?
                           CC_TRAVERSAL_GET_UP : CC_TRAVERSAL_RAGDOLL;
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
    } else {
        CcLimbRigUpdate(&agent->limb_rig, ToLimbVector(ShellPodBaseCenter(agent)),
                        agent->facing_yaw, ToLimbVector(agent->velocity),
                        agent->grounded, delta_time, ProbeLocalSurface, &context);
    }
}

static Camera3D LocalCamera(bool interior)
{
    Camera3D camera = {0};
    camera.target = interior ? (Vector3){4.55f, 0.72f, 3.40f} :
                               (Vector3){7.65f, 0.68f, 5.35f};
    float camera_distance = interior ? 10.0f : 14.0f;
    camera.position = (Vector3){camera.target.x + camera_distance,
                                camera.target.y + camera_distance,
                                camera.target.z + camera_distance};
    camera.up = (Vector3){0.0f, 1.0f, 0.0f};
    camera.fovy = interior ? 10.5f : 17.4f;
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
        DrawBox((Vector3){center.x, 0.92f, z + depth + 0.025f},
                (Vector3){0.72f, 1.84f, 0.05f}, (Color){43, 34, 37, 255});
        DrawSphere((Vector3){center.x + 0.25f, 0.92f, z + depth + 0.06f},
                   0.035f, WORLD_GOLD);
    }
    DrawBox((Vector3){x + 0.42f, height * 0.60f, z + depth + 0.03f},
            (Vector3){0.42f, 0.58f, 0.04f}, Fade(WORLD_TEAL, 0.78f));
    DrawBox((Vector3){x + width - 0.42f, height * 0.60f, z + depth + 0.03f},
            (Vector3){0.42f, 0.58f, 0.04f}, Fade(WORLD_TEAL, 0.78f));
}

static void DrawGroundTile(float x, float z, Color color)
{
    DrawCube((Vector3){x + 0.5f, -0.035f, z + 0.5f}, 0.98f, 0.06f, 0.98f, color);
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

static void DrawBone(Vector3 a, Vector3 b, Color color)
{
    DrawCylinderEx(a, b, 0.028f, 0.028f, 7, color);
    DrawSphere(a, 0.045f, color);
    DrawSphere(b, 0.045f, color);
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
    DrawSphere(head,
               0.19f * scale, (Color){222, 174, 139, 255});
    DrawSphere(LocalPoint(position, 0.0f, 1.75f * scale + bob, -0.02f, yaw),
               0.145f * scale, (Color){58, 43, 45, 255});

    DrawSphere(LocalPoint(position, -0.065f * scale, 1.64f * scale + bob,
                          0.175f * scale, yaw), 0.024f * scale, WORLD_VOID);
    DrawSphere(LocalPoint(position, 0.065f * scale, 1.64f * scale + bob,
                          0.175f * scale, yaw), 0.024f * scale, WORLD_VOID);

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
    DrawBone(hip, chest, rig);
    DrawBone(chest, neck, rig);
    DrawBone(shoulder_l, shoulder_r, rig);
    DrawBone(shoulder_l, elbow_l, rig);
    DrawBone(elbow_l, hand_l, rig);
    DrawBone(shoulder_r, elbow_r, rig);
    DrawBone(elbow_r, hand_r, rig);
    DrawBone(hip, knee_l, rig);
    DrawBone(knee_l, foot_l, rig);
    DrawBone(hip, knee_r, rig);
    DrawBone(knee_r, foot_r, rig);
    if (player) {
        DrawSphere(LocalPoint(position, 0.0f, 2.08f * scale + bob, 0.0f, yaw),
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

static void DrawBiomechanicalBiped(const CcLocalAgent *agent)
{
    const CcHumanoidGait *gait = &agent->humanoid;
    const CcHumanoidPose *pose = &gait->pose;
    Vector3 pelvis = FromLimbVector(pose->pelvis);
    Vector3 chest = FromLimbVector(pose->chest);
    Vector3 neck = FromLimbVector(pose->neck);
    Vector3 head = FromLimbVector(pose->head);
    float fallen_weight = fmaxf(0.0f, fminf(agent->ragdoll_visual_blend, 1.0f));
    float upright_weight = 1.0f - fallen_weight;
    Vector3 shoulder_left = FromLimbVector(pose->shoulder[0]);
    Vector3 shoulder_right = FromLimbVector(pose->shoulder[1]);
    Vector3 hip_left = FromLimbVector(pose->hip[0]);
    Vector3 hip_right = FromLimbVector(pose->hip[1]);
    float upper_yaw = PoseAxisYaw(shoulder_left, shoulder_right,
                                  agent->facing_yaw + pose->chest_yaw);
    float pelvis_yaw = PoseAxisYaw(hip_left, hip_right,
                                   agent->facing_yaw + pose->pelvis_yaw);
    Vector3 torso_center = PhysicsScale(PhysicsAdd(pelvis, chest), 0.5f);
    Vector3 cape_center = PhysicsAdd(torso_center,
        PhysicsScale((Vector3){sinf(upper_yaw), 0.0f, cosf(upper_yaw)}, -0.17f));
    Color tunic = agent->tunic_color;

    if (fallen_weight > 0.01f) {
        Vector3 fallen_back = PhysicsScale(
            (Vector3){sinf(upper_yaw), 0.0f, cosf(upper_yaw)}, -0.12f);
        DrawCylinderEx(PhysicsAdd(chest, fallen_back),
                       PhysicsAdd(pelvis, fallen_back),
                       0.17f, 0.25f, 5,
                       Fade((Color){73, 55, 91, 255}, fallen_weight));
        DrawSphere(pelvis, 0.18f,
                   Fade((Color){44, 61, 65, 255}, fallen_weight));
        DrawCylinderEx(pelvis, FromLimbVector(pose->spine),
                       0.18f, 0.22f, 10,
                       Fade((Color){38, 105, 112, 255}, fallen_weight));
        DrawCylinderEx(FromLimbVector(pose->spine), chest,
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
        DrawCylinderEx(FromLimbVector(pose->spine), chest,
                       0.25f, 0.29f, 10,
                       Fade(tunic, upright_weight));
        Vector3 plate = PhysicsAdd(chest,
            PhysicsScale((Vector3){sinf(upper_yaw), 0.0f, cosf(upper_yaw)},
                         0.17f));
        DrawOrientedBox(plate, (Vector3){0.0f, -0.04f, 0.0f},
                        (Vector3){0.34f, 0.28f, 0.045f}, upper_yaw,
                        Fade((Color){223, 173, 67, 255}, upright_weight));
    }

    for (int32_t leg = 0; leg < CC_HUMANOID_LEG_COUNT; ++leg) {
        Vector3 hip = FromLimbVector(pose->hip[leg]);
        Vector3 knee = FromLimbVector(pose->knee[leg]);
        Vector3 ankle = FromLimbVector(pose->ankle[leg]);
        Vector3 heel = FromLimbVector(pose->heel[leg]);
        Vector3 toe = FromLimbVector(pose->toe[leg]);
        DrawCylinderEx(hip, knee, 0.086f, 0.070f, 9,
                       (Color){38, 63, 68, 255});
        DrawCylinderEx(knee, ankle, 0.069f, 0.052f, 9,
                       (Color){48, 71, 75, 255});
        DrawPitchedFoot(heel, toe, agent->facing_yaw,
                        (Color){35, 54, 59, 255});
    }

    for (int32_t arm = 0; arm < CC_HUMANOID_ARM_COUNT; ++arm) {
        Vector3 shoulder = FromLimbVector(pose->shoulder[arm]);
        Vector3 elbow = FromLimbVector(pose->elbow[arm]);
        Vector3 hand = FromLimbVector(pose->hand[arm]);
        DrawCylinderEx(shoulder, elbow, 0.066f, 0.055f, 8,
                       tunic);
        DrawCylinderEx(elbow, hand, 0.055f, 0.044f, 8,
                       (Color){54, 66, 71, 255});
        DrawSphere(shoulder, 0.092f, (Color){223, 173, 67, 255});
        DrawSphere(hand, 0.055f, (Color){221, 174, 118, 255});
    }

    DrawSphere(head, 0.18f, (Color){221, 174, 118, 255});
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
    DrawSphere(hair, 0.145f, (Color){52, 46, 51, 255});
    Vector3 eye_center = PhysicsAdd(
        PhysicsAdd(head, PhysicsScale(head_up, 0.01f)),
        PhysicsScale(head_forward, 0.165f));
    DrawSphere(PhysicsAdd(eye_center, PhysicsScale(head_right, -0.058f)),
               0.022f, WORLD_VOID);
    DrawSphere(PhysicsAdd(eye_center, PhysicsScale(head_right, 0.058f)),
               0.022f, WORLD_VOID);
    if (agent->crowned) {
        DrawSphere(PhysicsAdd(head, PhysicsScale(head_up, 0.30f)),
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
        DrawSphere(Add3(physical_root, rig_offset), 0.052f,
                   Fade(WORLD_TEAL, upright_weight));
        DrawCylinderEx(Add3(physical_root, rig_offset),
                       Add3(reaction_end, rig_offset), 0.014f, 0.008f, 6,
                       Fade(WORLD_TEAL, 0.82f * upright_weight));
    }
    DrawBone(Add3(pelvis, rig_offset), Add3(FromLimbVector(pose->spine),
                                            rig_offset), WORLD_GOLD);
    DrawBone(Add3(FromLimbVector(pose->spine), rig_offset),
             Add3(chest, rig_offset), WORLD_GOLD);
    DrawBone(Add3(chest, rig_offset), Add3(neck, rig_offset), WORLD_GOLD);
    for (int32_t leg = 0; leg < CC_HUMANOID_LEG_COUNT; ++leg) {
        Color bone = HumanoidContactColor(gait->feet[leg].contact);
        Vector3 hip = Add3(FromLimbVector(pose->hip[leg]), rig_offset);
        Vector3 knee = Add3(FromLimbVector(pose->knee[leg]), rig_offset);
        Vector3 ankle = Add3(FromLimbVector(pose->ankle[leg]), rig_offset);
        Vector3 heel = Add3(FromLimbVector(pose->heel[leg]), rig_offset);
        Vector3 ball = Add3(FromLimbVector(pose->ball[leg]), rig_offset);
        Vector3 toe = Add3(FromLimbVector(pose->toe[leg]), rig_offset);
        DrawBone(hip, knee, bone);
        DrawBone(knee, ankle, bone);
        DrawBone(ankle, heel, bone);
        DrawBone(heel, ball, bone);
        DrawBone(ball, toe, bone);
    }
    for (int32_t arm = 0; arm < CC_HUMANOID_ARM_COUNT; ++arm) {
        DrawBone(Add3(FromLimbVector(pose->shoulder[arm]), rig_offset),
                 Add3(FromLimbVector(pose->elbow[arm]), rig_offset), WORLD_GOLD);
        DrawBone(Add3(FromLimbVector(pose->elbow[arm]), rig_offset),
                 Add3(FromLimbVector(pose->hand[arm]), rig_offset), WORLD_GOLD);
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
        DrawSphere(LocalPoint(body, -0.085f, 0.06f, 0.34f, agent->facing_yaw),
                   0.040f, WORLD_VOID);
        DrawSphere(LocalPoint(body, 0.085f, 0.06f, 0.34f, agent->facing_yaw),
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
            DrawSphere(rig_a, 0.046f, bone);
        }
        Vector3 foot = FromLimbVector(limb->joints[spec->segment_count]);
        DrawSphere(foot, 0.076f, bone);
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
        DrawSphere(shoulder_l, 0.095f, (Color){223, 173, 67, 255});
        DrawSphere(shoulder_r, 0.095f, (Color){223, 173, 67, 255});
        DrawSphere(hand_l, 0.055f, (Color){221, 174, 118, 255});
        DrawSphere(hand_r, 0.055f, (Color){221, 174, 118, 255});

        Vector3 visual_spine_base = Add3(spine_base, rig_offset);
        Vector3 visual_chest = Add3(chest, rig_offset);
        Vector3 visual_neck = Add3(neck, rig_offset);
        DrawBone(visual_spine_base, visual_chest, WORLD_GOLD);
        DrawBone(visual_chest, visual_neck, WORLD_GOLD);
        DrawBone(Add3(shoulder_l, rig_offset), Add3(shoulder_r, rig_offset),
                 WORLD_GOLD);
        DrawBone(Add3(shoulder_l, rig_offset), Add3(elbow_l, rig_offset),
                 WORLD_GOLD);
        DrawBone(Add3(elbow_l, rig_offset), Add3(hand_l, rig_offset), WORLD_GOLD);
        DrawBone(Add3(shoulder_r, rig_offset), Add3(elbow_r, rig_offset),
                 WORLD_GOLD);
        DrawBone(Add3(elbow_r, rig_offset), Add3(hand_r, rig_offset), WORLD_GOLD);

        Vector3 head = LocalPoint(body, 0.0f, 0.81f, hero_lean * 1.25f,
                                  upper_yaw);
        DrawSphere(head, 0.18f, (Color){221, 174, 118, 255});
        DrawSphere(LocalPoint(body, 0.0f, 0.91f,
                              -0.025f + hero_lean * 1.25f, upper_yaw),
                   0.145f, (Color){52, 46, 51, 255});
        DrawSphere(LocalPoint(body, -0.058f, 0.82f,
                              0.165f + hero_lean * 1.25f, upper_yaw),
                   0.022f, WORLD_VOID);
        DrawSphere(LocalPoint(body, 0.058f, 0.82f,
                              0.165f + hero_lean * 1.25f, upper_yaw),
                   0.022f, WORLD_VOID);
        DrawSphere(LocalPoint(body, 0.0f, 1.11f, hero_lean * 1.20f,
                              upper_yaw),
                   0.060f, WORLD_GOLD);
    } else {
        DrawSphere(LocalPoint(body, 0.0f, 0.52f, 0.0f, agent->facing_yaw),
                   0.065f, WORLD_GOLD);
    }
}

static void DrawCarriage3D(void)
{
    float x = CARRIAGE_FOOTPRINT.x + CARRIAGE_FOOTPRINT.width * 0.5f;
    float z = CARRIAGE_FOOTPRINT.y + CARRIAGE_FOOTPRINT.height * 0.5f;
    DrawBox((Vector3){x, 1.06f, z},
            (Vector3){CARRIAGE_FOOTPRINT.width, 1.36f, CARRIAGE_FOOTPRINT.height},
            (Color){125, 66, 50, 255});
    DrawBox((Vector3){x, 1.82f, z}, (Vector3){1.72f, 0.16f, 1.12f},
            (Color){228, 174, 77, 255});
    DrawBox((Vector3){x, 1.20f, 7.07f}, (Vector3){0.72f, 0.44f, 0.04f},
            (Color){35, 102, 108, 255});
    Vector3 wheels[] = {{0.79f, 0.42f, 6.05f}, {1.91f, 0.42f, 6.05f},
                        {0.79f, 0.42f, 7.05f}, {1.91f, 0.42f, 7.05f}};
    for (int32_t i = 0; i < 4; ++i) {
        DrawSphere(wheels[i], 0.34f, (Color){38, 31, 31, 255});
        DrawSphereWires(wheels[i], 0.35f, 7, 7, WORLD_GOLD);
    }
}

static void DrawNotice3D(const CcSim *sim)
{
    DrawBox((Vector3){2.79f, 0.56f, 2.75f}, (Vector3){0.08f, 1.12f, 0.08f},
            (Color){89, 58, 42, 255});
    DrawBox((Vector3){3.31f, 0.56f, 2.75f}, (Vector3){0.08f, 1.12f, 0.08f},
            (Color){89, 58, 42, 255});
    DrawBox((Vector3){3.05f, 1.05f, 2.75f}, (Vector3){0.72f, 0.58f, 0.10f},
            (Color){148, 94, 52, 255});
    int32_t count = CcSimActiveSituationCount(sim);
    for (int32_t i = 0; i < count && i < 4; ++i) {
        DrawBox((Vector3){2.82f + (float)i * 0.15f, 1.06f + (float)(i & 1) * 0.08f,
                          2.69f},
                (Vector3){0.11f, 0.22f, 0.025f},
                i == 3 ? WORLD_DANGER : WORLD_INK);
    }
}

static void DrawDungeon3D(const CcDungeon *dungeon)
{
    DrawBox((Vector3){7.82f, 1.12f, 7.25f}, (Vector3){0.42f, 2.24f, 0.58f},
            (Color){64, 56, 72, 255});
    DrawBox((Vector3){8.88f, 1.12f, 7.25f}, (Vector3){0.42f, 2.24f, 0.58f},
            (Color){64, 56, 72, 255});
    DrawBox((Vector3){8.35f, 2.27f, 7.25f}, (Vector3){1.48f, 0.34f, 0.58f},
            (Color){74, 62, 84, 255});
    DrawBox((Vector3){8.35f, 1.02f, 7.55f}, (Vector3){0.72f, 2.04f, 0.035f},
            (Color){8, 5, 14, 255});
    float pulse = 0.06f + (float)dungeon->regional_pressure / 500.0f;
    DrawSphere((Vector3){8.35f, 1.12f, 7.59f}, pulse, Fade(WORLD_VIOLET, 0.82f));
}

static void DrawTree(float x, float z, Color leaves)
{
    DrawCylinder((Vector3){x, 0.0f, z}, 0.13f, 0.10f, 1.30f, 7,
                 (Color){80, 57, 43, 255});
    DrawSphere((Vector3){x, 1.68f, z}, 0.58f, leaves);
    DrawSphereWires((Vector3){x, 1.68f, z}, 0.59f, 7, 7, Fade(WORLD_INK, 0.12f));
}

static void DrawLabels(const WorldLabel *labels, int32_t count, Camera3D camera,
                       int32_t width, int32_t height)
{
    for (int32_t i = 0; i < count; ++i) {
        Vector2 screen = GetWorldToScreenEx(labels[i].point, camera, width, height);
        int text_width = MeasureText(labels[i].text, 10);
        DrawRectangleRounded((Rectangle){screen.x - (float)text_width * 0.5f - 5.0f,
                                         screen.y - 5.0f,
                                         (float)text_width + 10.0f, 16.0f},
                             0.30f, 4, (Color){4, 10, 14, 210});
        DrawText(labels[i].text, (int)screen.x - text_width / 2,
                 (int)screen.y - 2, 10, labels[i].color);
    }
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
    Vector3 threat = CourseThreatCenter(course);
    if (course->alarm_active) {
        for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
            const CcLocalAgent *raider = &course->raiders[i];
            DrawRobotShell(raider);
            Vector3 hand = FromLimbVector(raider->humanoid.pose.hand[1]);
            Vector3 club_end = LocalPoint(hand, 0.0f, 0.08f, 0.46f,
                                          raider->facing_yaw);
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
        if (!runner->agent.climbing &&
            !runner->agent.humanoid.ragdoll.active) {
            Vector3 hand = FromLimbVector(runner->agent.humanoid.pose.hand[1]);
            Vector3 aim = {sinf(runner->agent.facing_yaw), 0.0f,
                           cosf(runner->agent.facing_yaw)};
            if (course->alarm_active) {
                aim = PhysicsNormalizeOr(
                    PhysicsSubtract(threat, hand), aim);
                aim.y = runner->duty == CC_GUARD_ENGAGED ? 0.06f : 0.16f;
                aim = PhysicsNormalizeOr(aim,
                    (Vector3){0.0f, 0.0f, 1.0f});
            }
            float thrust = runner->duty == CC_GUARD_ENGAGED ?
                0.76f + sinf(course->engagement_time * 12.0f +
                              (float)i * 2.1f) * 0.16f : 0.58f;
            Vector3 spear_tip = PhysicsAdd(hand, PhysicsScale(aim, thrust));
            DrawCylinderEx(hand, spear_tip, 0.022f, 0.013f, 7,
                           (Color){128, 92, 55, 255});
            DrawSphere(spear_tip, 0.038f, WORLD_GOLD);
            Vector3 shield_hand = FromLimbVector(
                runner->agent.humanoid.pose.hand[0]);
            DrawSphere(shield_hand, 0.105f,
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
    Camera3D camera = LocalCamera(false);
    Color kingdom = KingdomColor3D(sim, place->kingdom_id);
    BeginTextureMode(target);
    ClearBackground((Color){10, 24, 30, 255});
    BeginMode3D(camera);

    DrawPlane((Vector3){8.0f, -0.09f, 5.5f}, (Vector2){22.0f, 17.0f},
              (Color){24, 50, 45, 255});

    for (int32_t z = 0; z < 11; ++z) {
        for (int32_t x = 0; x < 16; ++x) {
            Color tile = ((x + z) & 1) ? (Color){44, 75, 64, 255} :
                                            (Color){38, 67, 59, 255};
            if (x >= 4 && x <= 5) tile = (Color){93, 91, 75, 255};
            if (z >= 3 && z <= 4 && x >= 4) tile = (Color){88, 86, 73, 255};
            if (x >= 9) {
                tile = ((x + z) & 1) ? (Color){48, 67, 65, 255} :
                                          (Color){42, 61, 60, 255};
            }
            DrawGroundTile((float)x, (float)z, tile);
        }
    }
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
    DrawBuilding(STREET_BUILDINGS[0].x, STREET_BUILDINGS[0].y,
                 STREET_BUILDINGS[0].width, STREET_BUILDINGS[0].height,
                 STREET_BUILDING_HEIGHTS[0],
                 (Color){84, 94, 91, 255}, kingdom, true);
    DrawBuilding(STREET_BUILDINGS[1].x, STREET_BUILDINGS[1].y,
                 STREET_BUILDINGS[1].width, STREET_BUILDINGS[1].height,
                 STREET_BUILDING_HEIGHTS[1],
                 (Color){99, 92, 83, 255}, kingdom, true);
    DrawBuilding(STREET_BUILDINGS[2].x, STREET_BUILDINGS[2].y,
                 STREET_BUILDINGS[2].width, STREET_BUILDINGS[2].height,
                 STREET_BUILDING_HEIGHTS[2],
                 (Color){117, 76, 62, 255}, (Color){216, 158, 68, 255}, true);
    DrawBuilding(STREET_BUILDINGS[3].x, STREET_BUILDINGS[3].y,
                 STREET_BUILDINGS[3].width, STREET_BUILDINGS[3].height,
                 STREET_BUILDING_HEIGHTS[3],
                 (Color){70, 86, 94, 255}, (Color){123, 79, 126, 255}, true);
    DrawCarriage3D();
    DrawNotice3D(sim);
    DrawTree(0.45f, 4.30f, (Color){58, 119, 91, 255});
    DrawTree(2.20f, 8.20f, (Color){54, 105, 85, 255});
    DrawTree(9.55f, 3.90f, (Color){63, 116, 93, 255});

    int32_t crates = place->stock[CC_GOOD_FOOD] / 12;
    if (crates > 4) crates = 4;
    for (int32_t i = 0; i < crates; ++i) {
        DrawBox((Vector3){6.30f + (float)(i % 2) * 0.38f, 0.20f,
                          3.45f + (float)(i / 2) * 0.42f},
                (Vector3){0.34f, 0.40f, 0.34f},
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
    EndMode3D();

    WorldLabel labels[5];
    int32_t count = 0;
    labels[count++] = (WorldLabel){{7.78f, 3.75f, 3.03f}, "MARKET HOUSE", WORLD_GOLD};
    labels[count++] = (WorldLabel){{1.35f, 2.08f, 6.55f}, "CROWNLESS CARRIAGE", WORLD_GOLD};
    labels[count++] = (WorldLabel){{3.05f, 1.42f, 2.75f}, "SITUATIONS", WORLD_INK};
    labels[count++] = (WorldLabel){{11.80f, 2.05f, 0.82f},
                                   "WAYFARER TRIALS", WORLD_GOLD};
    if (dungeon != NULL) {
        labels[count++] = (WorldLabel){{8.35f, 2.70f, 7.30f},
                                       CcDungeonStateName(dungeon->state), WORLD_VIOLET};
    }
    DrawLabels(labels, count, camera, target.texture.width, target.texture.height);
    if (agent->morphology == CC_MORPHOLOGY_BIPED) {
        DrawText(TextFormat("BIOMECHANICAL BIPED / %d JOINTS / %s / MUSCLES + LIGAMENTS",
                            agent->humanoid.body.morphology.joint_count,
                            CcLocalTraversalName(agent->traversal)),
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
        DrawText(TextFormat("VILLAGE ALARM / GUARDS %s / RAIDER RESOLVE %d%%",
                            course->raiders_retreating ? "DRIVING THEM OUT" :
                            line_engaged ? "ENGAGED" : "FORMING LINE",
                            course->raider_resolve > 0 ?
                            course->raider_resolve : 0),
                 18, 33, 10, WORLD_DANGER);
    }
    EndTextureMode();
    PresentTarget(target, destination);
}

void CcLocalDrawMarket3D(const CcSim *sim, const CcLocalAgent *agent, float clock,
                         RenderTexture2D target, Rectangle destination)
{
    const CcSettlement *place = CcSimSettlement(sim, sim->player.location_id);
    if (place == NULL) return;
    Camera3D camera = LocalCamera(true);
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
    if (point.x < 0.28f + radius || point.x > 9.72f - radius ||
        point.y < 0.28f + radius || point.y > 8.72f - radius) return false;
    for (int32_t i = 0; i < (int32_t)(sizeof(STREET_BUILDINGS) /
                                      sizeof(STREET_BUILDINGS[0])); ++i) {
        if (InsideExpanded(point, STREET_BUILDINGS[i], radius)) return false;
    }
    if (InsideExpanded(point, CARRIAGE_FOOTPRINT, radius)) return false;
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
