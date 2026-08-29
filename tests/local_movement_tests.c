#include "client/cc_local3d.h"
#include "client/cc_local3d_internal.h"
#include "client/cc_local_place.h"
#include "locomotion/cc_humanoid_skin.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int32_t StreetPortalIndex(const CcLocalAgent *agent,
                                 const char *name)
{
    int32_t count = CcLocalAgentStreetPortalCount(agent);
    for (int32_t portal = 0; portal < count; ++portal) {
        const char *candidate = CcLocalAgentStreetPortalName(agent, portal);
        if (candidate != NULL && strcmp(candidate, name) == 0) return portal;
    }
    return -1;
}

static void RequirePosition(const char *name, Vector2 actual, Vector2 expected)
{
    if (fabsf(actual.x - expected.x) < 0.001f &&
        fabsf(actual.y - expected.y) < 0.001f) return;
    (void)fprintf(stderr, "%s: expected %.2f,%.2f but got %.2f,%.2f\n",
                  name, expected.x, expected.y, actual.x, actual.y);
    exit(1);
}

static void RequireNearPosition(const char *name, Vector2 actual, Vector2 expected,
                                float tolerance)
{
    float x = actual.x - expected.x;
    float y = actual.y - expected.y;
    if (sqrtf(x * x + y * y) <= tolerance) return;
    (void)fprintf(stderr, "%s: expected near %.2f,%.2f but got %.2f,%.2f\n",
                  name, expected.x, expected.y, actual.x, actual.y);
    exit(1);
}

static float Distance3(CcLimbVec3 a, CcLimbVec3 b)
{
    float x = b.x - a.x;
    float y = b.y - a.y;
    float z = b.z - a.z;
    return sqrtf(x * x + y * y + z * z);
}

static float VectorDistance3(Vector3 a, Vector3 b)
{
    float x = b.x - a.x;
    float y = b.y - a.y;
    float z = b.z - a.z;
    return sqrtf(x * x + y * y + z * z);
}

static float VectorDistance2(Vector2 a, Vector2 b)
{
    float x = b.x - a.x;
    float y = b.y - a.y;
    return sqrtf(x * x + y * y);
}

static CcLimbVec3 ExpectedTopOutHand(const CcLocalAgent *agent, int32_t arm)
{
    CcMotionMantleSample motion;
    (void)CcMotionClipSampleMantle(agent->climb_progress, &motion);
    float side = arm == 0 ? -1.0f : 1.0f;
    Vector3 right = {cosf(agent->facing_yaw), 0.0f,
                     -sinf(agent->facing_yaw)};
    Vector3 edge = {
        agent->climb_face.x + right.x * side * 0.19f +
            agent->climb_normal.x * 0.025f,
        agent->climb_face.y + 0.040f,
        agent->climb_face.z + right.z * side * 0.19f +
            agent->climb_normal.z * 0.025f
    };
    Vector3 top = {
        agent->climb_face.x + right.x * side * 0.18f -
            agent->climb_normal.x * 0.19f,
        agent->climb_face.y + 0.045f,
        agent->climb_face.z + right.z * side * 0.18f -
            agent->climb_normal.z * 0.19f
    };
    float amount = motion.hand_press[arm];
    return (CcLimbVec3){
        edge.x + (top.x - edge.x) * amount,
        edge.y + (top.y - edge.y) * amount,
        edge.z + (top.z - edge.z) * amount
    };
}

static float MaximumPoseStep(const CcHumanoidPose *before,
                             const CcHumanoidPose *after)
{
    float maximum = Distance3(before->pelvis, after->pelvis);
#define INCLUDE_POSE_POINT(point) \
    maximum = fmaxf(maximum, Distance3(before->point, after->point))
    INCLUDE_POSE_POINT(spine);
    INCLUDE_POSE_POINT(chest);
    INCLUDE_POSE_POINT(neck);
    INCLUDE_POSE_POINT(head);
    for (int32_t leg = 0; leg < CC_HUMANOID_LEG_COUNT; ++leg) {
        maximum = fmaxf(maximum, Distance3(before->hip[leg], after->hip[leg]));
        maximum = fmaxf(maximum, Distance3(before->knee[leg], after->knee[leg]));
        maximum = fmaxf(maximum, Distance3(before->ankle[leg], after->ankle[leg]));
        maximum = fmaxf(maximum, Distance3(before->heel[leg], after->heel[leg]));
        maximum = fmaxf(maximum, Distance3(before->ball[leg], after->ball[leg]));
        maximum = fmaxf(maximum, Distance3(before->toe[leg], after->toe[leg]));
    }
    for (int32_t arm = 0; arm < CC_HUMANOID_ARM_COUNT; ++arm) {
        maximum = fmaxf(maximum,
                        Distance3(before->shoulder[arm], after->shoulder[arm]));
        maximum = fmaxf(maximum,
                        Distance3(before->elbow[arm], after->elbow[arm]));
        maximum = fmaxf(maximum,
                        Distance3(before->hand[arm], after->hand[arm]));
    }
#undef INCLUDE_POSE_POINT
    return maximum;
}

static float RelativePointStep(CcLimbVec3 before, CcLimbVec3 before_root,
                               CcLimbVec3 after, CcLimbVec3 after_root)
{
    CcLimbVec3 before_relative = {
        before.x - before_root.x,
        before.y - before_root.y,
        before.z - before_root.z,
    };
    CcLimbVec3 after_relative = {
        after.x - after_root.x,
        after.y - after_root.y,
        after.z - after_root.z,
    };
    return Distance3(before_relative, after_relative);
}

static float MaximumRelativeUpperPoseStep(const CcHumanoidPose *before,
                                          const CcHumanoidPose *after)
{
    float maximum = RelativePointStep(before->spine, before->pelvis,
                                      after->spine, after->pelvis);
#define INCLUDE_RELATIVE_UPPER_POINT(point) \
    maximum = fmaxf(maximum, RelativePointStep( \
        before->point, before->pelvis, after->point, after->pelvis))
    INCLUDE_RELATIVE_UPPER_POINT(chest);
    INCLUDE_RELATIVE_UPPER_POINT(neck);
    INCLUDE_RELATIVE_UPPER_POINT(head);
    for (int32_t arm = 0; arm < CC_HUMANOID_ARM_COUNT; ++arm) {
        maximum = fmaxf(maximum, RelativePointStep(
            before->shoulder[arm], before->pelvis,
            after->shoulder[arm], after->pelvis));
        maximum = fmaxf(maximum, RelativePointStep(
            before->elbow[arm], before->pelvis,
            after->elbow[arm], after->pelvis));
        maximum = fmaxf(maximum, RelativePointStep(
            before->hand[arm], before->pelvis,
            after->hand[arm], after->pelvis));
    }
#undef INCLUDE_RELATIVE_UPPER_POINT
    return maximum;
}

static float RagdollCenterVelocityY(const CcBiomechRagdoll *ragdoll,
                                    float delta_time)
{
    float momentum = 0.0f;
    float total_mass = 0.0f;
    for (int32_t particle = 0; particle < ragdoll->particle_count; ++particle) {
        if (ragdoll->particles[particle].inverse_mass <= 0.0f) continue;
        float mass = 1.0f / ragdoll->particles[particle].inverse_mass;
        CcBiomechVec3 velocity = CcBiomechRagdollParticleVelocity(
            ragdoll, particle, delta_time);
        momentum += velocity.y * mass;
        total_mass += mass;
    }
    return total_mass > 0.0f ? momentum / total_mass : 0.0f;
}

static Vector3 RagdollCenterVelocity(const CcBiomechRagdoll *ragdoll,
                                     float delta_time)
{
    Vector3 momentum = {0};
    float total_mass = 0.0f;
    for (int32_t particle = 0; particle < ragdoll->particle_count; ++particle) {
        if (ragdoll->particles[particle].inverse_mass <= 0.0f) continue;
        float mass = 1.0f / ragdoll->particles[particle].inverse_mass;
        CcBiomechVec3 velocity = CcBiomechRagdollParticleVelocity(
            ragdoll, particle, delta_time);
        momentum.x += velocity.x * mass;
        momentum.y += velocity.y * mass;
        momentum.z += velocity.z * mass;
        total_mass += mass;
    }
    return total_mass > 0.0f ?
        (Vector3){momentum.x / total_mass, momentum.y / total_mass,
                  momentum.z / total_mass} : (Vector3){0};
}

static bool RagdollTouchesStreet(const CcBiomechRagdoll *ragdoll)
{
    for (int32_t particle = 0; particle < ragdoll->particle_count; ++particle) {
        const CcBiomechRagdollParticle *body = &ragdoll->particles[particle];
        if (body->collided && body->position.y - body->radius < 0.12f) {
            return true;
        }
    }
    return false;
}

static void RequireSolidStreetHouse(const char *name, float wall_x,
                                    float center_x, float center_z)
{
    const float radius = 0.16f;
    float body_y = CcLocalTerrainHeightAt(center_x, center_z) + 1.0f;
    Vector3 previous = {wall_x - 0.80f, body_y, center_z};
    Vector3 proposed = {wall_x + 0.80f, body_y, center_z};
    Vector3 corrected = proposed;
    Vector3 normal = {0};
    if (!CcLocalProbePhysicsSphereInternal(
            CC_LOCAL_SCENE_STREET, previous, proposed, radius,
            &corrected, &normal) ||
        corrected.x > wall_x - radius + 0.006f || normal.x > -0.90f) {
        (void)fprintf(stderr,
                      "%s was not solid: %.3f %.3f %.3f normal %.3f %.3f %.3f\n",
                      name, corrected.x, corrected.y, corrected.z,
                      normal.x, normal.y, normal.z);
        exit(1);
    }
}

static void TestPlaceLandmarkCollision(void)
{
    CcSim sim;
    CcSimInit(&sim, UINT32_C(0x1a7d4a2b));
    sim.player.location_id = sim.settlements[0].id;
    CcLocalBindPlace(&sim);
    const CcLocalPlaceLandmark *barn = CcLocalPlaceLandmarkAt(
        CC_SETTLEMENT_FARMING, 0);
    if (barn == NULL) {
        (void)fprintf(stderr, "farming layout did not provide a landmark\n");
        exit(1);
    }
    float center_z = barn->z + barn->depth * 0.5f;
    float body_y = CcLocalTerrainHeightAt(barn->x, center_z) + 1.0f;
    Vector3 corrected = {0};
    Vector3 normal = {0};
    if (!CcLocalProbePhysicsSphereInternal(
            CC_LOCAL_SCENE_STREET,
            (Vector3){barn->x - 0.80f, body_y, center_z},
            (Vector3){barn->x + 0.80f, body_y, center_z}, 0.16f,
            &corrected, &normal) ||
        corrected.x > barn->x - 0.154f || normal.x > -0.90f) {
        (void)fprintf(stderr,
                      "authored landmark was not solid: %.3f normal %.3f\n",
                      corrected.x, normal.x);
        exit(1);
    }
    Vector2 legacy = CcLocalMove(
        (Vector2){barn->x - 0.70f, center_z},
        (Vector2){1.40f, 0.0f}, false);
    if (legacy.x > barn->x - 0.29f) {
        (void)fprintf(stderr,
                      "legacy movement crossed an authored landmark: %.3f\n",
                      legacy.x);
        exit(1);
    }

    CcLocalBindPlace(NULL);
}

static void TestTownPlanCollisionAndGate(void)
{
    CcSim sim;
    CcSimInit(&sim, UINT32_C(0x7a11c0de));
    for (int32_t settlement = 0;
         settlement < sim.settlement_count; ++settlement) {
        sim.player.location_id = sim.settlements[settlement].id;
        CcLocalBindPlace(&sim);

        Vector2 hall_approach = {50.0f, 27.0f};
        Vector2 hall_blocked = CcLocalMove(
            hall_approach, (Vector2){0.0f, -2.0f}, false);
        if (hall_blocked.y < 26.27f) {
            (void)fprintf(
                stderr,
                "town plan %d did not keep its civic hall solid: %.3f\n",
                sim.settlements[settlement].function, hall_blocked.y);
            exit(1);
        }

        Vector2 gate_walk = {78.50f, 35.00f};
        for (int32_t step = 0; step < 17; ++step) {
            gate_walk = CcLocalMove(
                gate_walk, (Vector2){0.0f, -0.50f}, false);
        }
        if (gate_walk.y > 26.65f) {
            (void)fprintf(
                stderr,
                "town plan %d blocked its promised compound gate: %.3f\n",
                sim.settlements[settlement].function, gate_walk.y);
            exit(1);
        }

        float minimum_height = 10000.0f;
        float maximum_height = -10000.0f;
        for (float z = 4.0f; z < CC_LOCAL_WORLD_DEPTH; z += 4.0f) {
            for (float x = 4.0f; x < CC_LOCAL_WORLD_WIDTH; x += 4.0f) {
                float height = CcLocalTerrainHeightAt(x, z);
                minimum_height = fminf(minimum_height, height);
                maximum_height = fmaxf(maximum_height, height);
            }
        }
        if (maximum_height - minimum_height < 8.0f) {
            (void)fprintf(
                stderr,
                "town plan %d lost its major landform: relief %.2f\n",
                sim.settlements[settlement].function,
                maximum_height - minimum_height);
            exit(1);
        }

        float maximum_gate_grade = 0.0f;
        float previous_gate_height = CcLocalTerrainHeightAt(78.50f, 54.0f);
        for (float z = 53.50f; z >= 31.0f; z -= 0.50f) {
            float gate_height = CcLocalTerrainHeightAt(78.50f, z);
            maximum_gate_grade = fmaxf(
                maximum_gate_grade,
                fabsf(gate_height - previous_gate_height) / 0.50f);
            previous_gate_height = gate_height;
        }
        if (maximum_gate_grade > 0.14f) {
            (void)fprintf(
                stderr,
                "town plan %d castle approach is too steep for carts: %.1f%%\n",
                sim.settlements[settlement].function,
                maximum_gate_grade * 100.0f);
            exit(1);
        }
    }
    CcLocalBindPlace(NULL);
}

static void TestSharedCharacterCollisionWorld(void)
{
    Vector3 corrected = {0};
    Vector3 normal = {0};
    if (!CcLocalProbePhysicsSphereInternal(
            CC_LOCAL_SCENE_STREET,
            (Vector3){3.50f, 0.82f, 6.45f},
            (Vector3){3.50f, 0.82f, 7.35f}, 0.16f,
            &corrected, &normal) ||
        corrected.z > 6.845f || normal.z > -0.90f) {
        (void)fprintf(stderr,
                      "shared collision world missed a platform wall: %.3f %.3f %.3f normal %.3f %.3f %.3f\n",
                      corrected.x, corrected.y, corrected.z,
                      normal.x, normal.y, normal.z);
        exit(1);
    }

    if (!CcLocalProbePhysicsSphereInternal(
            CC_LOCAL_SCENE_STREET,
            (Vector3){3.50f, 2.30f, 7.50f},
            (Vector3){3.50f, 1.55f, 7.50f}, 0.16f,
            &corrected, &normal) ||
        corrected.y < 1.809f || normal.y < 0.90f) {
        (void)fprintf(stderr,
                      "shared collision world missed a platform top: %.3f normal %.3f %.3f %.3f\n",
                      corrected.y, normal.x, normal.y, normal.z);
        exit(1);
    }

    if (!CcLocalProbePhysicsSphereInternal(
            CC_LOCAL_SCENE_STREET,
            (Vector3){2.60f, 0.82f, 6.60f},
            (Vector3){3.10f, 0.82f, 7.10f}, 0.16f,
            &corrected, &normal)) {
        (void)fprintf(stderr, "shared collision world missed a corner sweep\n");
        exit(1);
    }
    float closest_x = fmaxf(3.0f, fminf(corrected.x, 4.0f));
    float closest_z = fmaxf(7.0f, fminf(corrected.z, 8.0f));
    float separation_x = corrected.x - closest_x;
    float separation_z = corrected.z - closest_z;
    if (separation_x * separation_x + separation_z * separation_z <
        0.159f * 0.159f) {
        (void)fprintf(stderr,
                      "shared collision corner retained sphere penetration: %.3f %.3f\n",
                      corrected.x, corrected.z);
        exit(1);
    }

    /* These houses are faded by several low camera shots. The reveal must
       never remove their physical walls for the player, NPCs, or ragdoll. */
    RequireSolidStreetHouse("west crofts house", 20.0f, 25.0f, 37.0f);
    RequireSolidStreetHouse("artisan row house", 34.0f, 37.5f, 39.0f);
    RequireSolidStreetHouse("market road house", 55.0f, 58.25f, 33.25f);
    RequireSolidStreetHouse("coach yard house", 32.0f, 36.0f, 50.5f);

    /* The visible ore station base is 1.45 x 1.05 m centered here. It must use
       one footprint for physical sweeps, click paths, ray picking, and the
       legacy movement helper. */
    const Rectangle ore_station = {25.725f, 53.825f, 1.45f, 1.05f};
    float ore_y = CcLocalTerrainHeightAt(26.45f, 54.35f) + 0.58f;
    corrected = (Vector3){27.70f, ore_y, 54.35f};
    normal = (Vector3){0};
    if (!CcLocalProbePhysicsSphereInternal(
            CC_LOCAL_SCENE_STREET,
            (Vector3){24.90f, ore_y, 54.35f},
            corrected, 0.16f, &corrected, &normal) ||
        corrected.x > ore_station.x - 0.155f || normal.x > -0.90f) {
        (void)fprintf(stderr,
                      "ore station was ghost geometry for physics: %.3f %.3f normal %.3f %.3f %.3f\n",
                      corrected.x, corrected.z, normal.x, normal.y, normal.z);
        exit(1);
    }

    Vector2 legacy = CcLocalMove((Vector2){25.00f, 54.35f},
                                 (Vector2){1.0f, 0.0f}, false);
    if (legacy.x > ore_station.x - 0.21f) {
        (void)fprintf(stderr,
                      "legacy movement crossed the ore station: %.3f %.3f\n",
                      legacy.x, legacy.y);
        exit(1);
    }

    Ray ore_ray = {
        .position = {26.45f, 6.0f, 54.35f},
        .direction = {0.0f, -1.0f, 0.0f},
    };
    float ore_ray_distance = CcLocalRoomArtRayDistanceInternal(
        ore_ray, (Vector3){26.45f, 0.0f, 54.35f});
    if (!isfinite(ore_ray_distance) || ore_ray_distance > 1.11f) {
        (void)fprintf(stderr,
                      "ray picking ignored the ore station: %.3f\n",
                      ore_ray_distance);
        exit(1);
    }

    CcLocalAgent ore_path;
    CcLocalAgentInit(&ore_path, (Vector2){24.90f, 54.35f}, false);
    if (!CcLocalAgentSetStreetTarget(
            &ore_path, (Vector3){28.0f, 0.0f, 54.35f}) ||
        ore_path.navigation_point_count < 2) {
        (void)fprintf(stderr,
                      "pathfinding could not route around the ore station\n");
        exit(1);
    }
    bool path_detoured = false;
    for (int32_t point = 0; point < ore_path.navigation_point_count; ++point) {
        Vector3 waypoint = ore_path.navigation_point[point];
        if (waypoint.z < ore_station.y - ore_path.radius ||
            waypoint.z > ore_station.y + ore_station.height + ore_path.radius) {
            path_detoured = true;
        }
        if (waypoint.x > ore_station.x - ore_path.radius &&
            waypoint.x < ore_station.x + ore_station.width + ore_path.radius &&
            waypoint.z > ore_station.y - ore_path.radius &&
            waypoint.z < ore_station.y + ore_station.height + ore_path.radius) {
            (void)fprintf(stderr,
                          "pathfinding placed a waypoint inside the ore station\n");
            exit(1);
        }
    }
    if (!path_detoured) {
        (void)fprintf(stderr,
                      "pathfinding crossed instead of avoiding the ore station\n");
        exit(1);
    }

    CcLocalAgent articulated;
    CcLocalAgentInit(&articulated, (Vector2){2.10f, 7.50f}, false);
    articulated.facing_yaw = 0.0f;
    CcLocalAgentSetMorphology(
        &articulated, CC_MORPHOLOGY_QUADRUPED, false);
    Vector3 point_space_move = articulated.position;
    point_space_move.x = 2.50f;
    if (!CcLocalAgentPointSpaceBlockedInternal(
            &articulated, point_space_move)) {
        (void)fprintf(stderr,
                      "articulated limb point space missed the course block\n");
        exit(1);
    }
}

static void TestRagdollStepsInWater(void)
{
    CcLocalAgent agent;
    CcLocalAgentInit(&agent, (Vector2){11.0f, 9.70f}, false);
    if (!CcHumanoidGaitKnockDown(&agent.humanoid)) {
        (void)fprintf(stderr, "water ragdoll fixture could not fall\n");
        exit(1);
    }
    float ragdoll_time = agent.humanoid.ragdoll_time;
    CcBiomechVec3 center = CcBiomechRagdollCenterOfMass(
        &agent.humanoid.ragdoll);
    CcBiomechVec3 advanced = center;
    float minimum_center_y = center.y;
    float maximum_step = 0.0f;
    for (int32_t step = 0; step < 360; ++step) {
        CcBiomechVec3 previous = advanced;
        CcLocalAgentFixedStepInternal(&agent, 1.0f / 60.0f, false);
        advanced = CcBiomechRagdollCenterOfMass(&agent.humanoid.ragdoll);
        minimum_center_y = fminf(minimum_center_y, advanced.y);
        maximum_step = fmaxf(maximum_step,
                             VectorDistance3(
                                 (Vector3){previous.x, previous.y, previous.z},
                                 (Vector3){advanced.x, advanced.y,
                                           advanced.z}));
    }
    if (!agent.humanoid.ragdoll.active ||
        agent.humanoid.ragdoll_time < ragdoll_time + 5.90f ||
        fabsf(advanced.y - center.y) < 0.000001f ||
        minimum_center_y < 0.27f || advanced.y < 0.50f ||
        maximum_step > 0.10f) {
        (void)fprintf(stderr,
                      "water ragdoll was not bounded: active %d time %.4f -> %.4f y %.4f -> %.4f min %.4f step %.4f\n",
                      agent.humanoid.ragdoll.active ? 1 : 0,
                      ragdoll_time, agent.humanoid.ragdoll_time,
                      center.y, advanced.y, minimum_center_y, maximum_step);
        exit(1);
    }
}

static void AdvanceRoadWorld(CcLocalCourse *course, CcLocalAgent *player,
                             const CcSim *sim, float duration,
                             const float *frame_times, int32_t frame_count)
{
    double elapsed = 0.0;
    int32_t frame = 0;
    while (elapsed < (double)duration - 0.00000001) {
        float delta_time = frame_times[frame % frame_count];
        double remaining = (double)duration - elapsed;
        if ((double)delta_time > remaining) delta_time = (float)remaining;
        CcLocalWorldUpdate(course, player, sim, delta_time, false, true);
        elapsed += (double)delta_time;
        frame += 1;
    }
}

static bool RoadWorldMatches(const CcLocalCourse *expected_course,
                             const CcLocalAgent *expected_player,
                             const CcLocalCourse *actual_course,
                             const CcLocalAgent *actual_player,
                             float tolerance)
{
    if (VectorDistance3(expected_player->position,
                        actual_player->position) > tolerance ||
        expected_course->alarm_active != actual_course->alarm_active ||
        expected_course->raiders_retreating !=
            actual_course->raiders_retreating ||
        expected_course->defenses_completed !=
            actual_course->defenses_completed ||
        expected_course->raider_resolve != actual_course->raider_resolve) {
        return false;
    }
    for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
        const CcLocalAgent *expected =
            &expected_course->runners[i].agent;
        const CcLocalAgent *actual = &actual_course->runners[i].agent;
        if (VectorDistance3(expected->position, actual->position) > tolerance ||
            fabsf(expected->combat.health - actual->combat.health) > 0.001f ||
            fabsf(expected->combat.posture - actual->combat.posture) > 0.001f ||
            expected->combat.life_state != actual->combat.life_state) {
            return false;
        }
    }
    for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
        const CcLocalAgent *expected = &expected_course->raiders[i];
        const CcLocalAgent *actual = &actual_course->raiders[i];
        if (VectorDistance3(expected->position, actual->position) > tolerance ||
            fabsf(expected->combat.health - actual->combat.health) > 0.001f ||
            fabsf(expected->combat.posture - actual->combat.posture) > 0.001f ||
            expected->combat.life_state != actual->combat.life_state ||
            expected_course->raider_response_stage[i] !=
                actual_course->raider_response_stage[i]) {
            return false;
        }
    }
    return true;
}

static void RunTowerFallScenario(const char *name, Vector2 start,
                                 Vector3 tower_target, Vector3 street_target)
{
    CcLocalAgent agent;
    CcLocalAgentInit(&agent, start, false);
    if (!CcLocalAgentSetExactTarget(&agent, tower_target, false)) {
        (void)fprintf(stderr, "%s: tower target was rejected\n", name);
        exit(1);
    }
    bool climbed = false;
    for (int32_t frame = 0; frame < 1200; ++frame) {
        CcLocalAgentUpdate(&agent, 1.0f / 60.0f, false);
        climbed = climbed || agent.traversal == CC_TRAVERSAL_CLIMB;
    }
    float tower_error_x = agent.position.x - tower_target.x;
    float tower_error_z = agent.position.z - tower_target.z;
    if (!climbed || !agent.grounded || fabsf(agent.position.y - 1.65f) > 0.01f ||
        sqrtf(tower_error_x * tower_error_x + tower_error_z * tower_error_z) >
            0.14f) {
        (void)fprintf(stderr,
                      "%s: climb setup failed at %.3f %.3f %.3f climb %d\n",
                      name, agent.position.x, agent.position.y,
                      agent.position.z, climbed);
        exit(1);
    }
    agent.allow_downclimb = false;
    if (!CcLocalAgentSetExactTarget(&agent, street_target, false)) {
        (void)fprintf(stderr, "%s: street target was rejected\n", name);
        exit(1);
    }
    bool saw_ragdoll = false;
    bool saw_recovery = false;
    bool street_impact = false;
    int32_t ragdoll_start_frame = -1;
    int32_t street_impact_frame = -1;
    float maximum_rebound_speed = 0.0f;
    float maximum_particle_rebound = 0.0f;
    int32_t maximum_particle_frame = -1;
    int32_t maximum_particle_index = -1;
    CcBiomechVec3 maximum_particle_position = {0};
    int32_t contact_gap = 0;
    int32_t maximum_contact_gap = 0;
    float previous_visual_blend = agent.ragdoll_visual_blend;
    float maximum_visual_blend_step = 0.0f;
    float maximum_authoritative_position_error = 0.0f;
    float maximum_authoritative_velocity_error = 0.0f;
    bool saw_visual_transition = false;
    for (int32_t frame = 0; frame < 900; ++frame) {
        CcLocalAgentUpdate(&agent, 1.0f / 60.0f, false);
        float blend_step = fabsf(agent.ragdoll_visual_blend -
                                 previous_visual_blend);
        maximum_visual_blend_step = fmaxf(maximum_visual_blend_step,
                                          blend_step);
        saw_visual_transition = saw_visual_transition ||
            (agent.ragdoll_visual_blend > 0.01f &&
             agent.ragdoll_visual_blend < 0.99f);
        previous_visual_blend = agent.ragdoll_visual_blend;
        if (agent.humanoid.ragdoll.active &&
            agent.humanoid.ragdoll_body_offset_valid) {
            CcBiomechVec3 center = CcBiomechRagdollCenterOfMass(
                &agent.humanoid.ragdoll);
            CcBiomechVec3 center_velocity = CcBiomechRagdollCenterVelocity(
                &agent.humanoid.ragdoll, 1.0f / 60.0f);
            Vector3 authoritative_position = {
                center.x + agent.humanoid.ragdoll_body_offset.x,
                center.y + agent.humanoid.ragdoll_body_offset.y,
                center.z + agent.humanoid.ragdoll_body_offset.z,
            };
            Vector3 authoritative_velocity = {
                center_velocity.x, center_velocity.y, center_velocity.z,
            };
            maximum_authoritative_position_error = fmaxf(
                maximum_authoritative_position_error,
                VectorDistance3(agent.position, authoritative_position));
            maximum_authoritative_velocity_error = fmaxf(
                maximum_authoritative_velocity_error,
                VectorDistance3(agent.velocity, authoritative_velocity));
        }
        if (agent.humanoid.ragdoll.active && !agent.humanoid.recovering) {
            saw_ragdoll = true;
            if (ragdoll_start_frame < 0) ragdoll_start_frame = frame;
            bool street_contact = RagdollTouchesStreet(
                &agent.humanoid.ragdoll);
            if (street_contact && street_impact_frame < 0) {
                street_impact_frame = frame;
            }
            street_impact = street_impact || street_contact;
            if (street_impact) {
                maximum_rebound_speed = fmaxf(
                    maximum_rebound_speed,
                    RagdollCenterVelocityY(&agent.humanoid.ragdoll,
                                           1.0f / 60.0f));
                for (int32_t particle = 0;
                     particle < agent.humanoid.ragdoll.particle_count;
                     ++particle) {
                    CcBiomechVec3 velocity =
                        CcBiomechRagdollParticleVelocity(
                            &agent.humanoid.ragdoll, particle,
                            1.0f / 60.0f);
                    if (velocity.y > maximum_particle_rebound) {
                        maximum_particle_rebound = velocity.y;
                        maximum_particle_frame = frame;
                        maximum_particle_index = particle;
                        maximum_particle_position =
                            agent.humanoid.ragdoll.particles[particle].position;
                    }
                }
                contact_gap = street_contact ? 0 : contact_gap + 1;
                maximum_contact_gap = contact_gap > maximum_contact_gap ?
                                      contact_gap : maximum_contact_gap;
            }
        }
        saw_recovery = saw_recovery || agent.humanoid.recovering;
    }
    float street_error_x = agent.position.x - street_target.x;
    float street_error_z = agent.position.z - street_target.z;
    float street_error = sqrtf(street_error_x * street_error_x +
                               street_error_z * street_error_z);
    float fall_duration = ragdoll_start_frame >= 0 && street_impact_frame >= 0 ?
        (float)(street_impact_frame - ragdoll_start_frame) / 60.0f : 1000.0f;
    if (!saw_ragdoll || !saw_recovery || !street_impact ||
        maximum_rebound_speed >= 0.35f || maximum_particle_rebound >= 0.45f ||
        maximum_contact_gap > 4 || fall_duration < 0.35f ||
        fall_duration > 0.85f ||
        !saw_visual_transition || maximum_visual_blend_step > 0.076f ||
        maximum_authoritative_position_error > 0.002f ||
        maximum_authoritative_velocity_error > 0.002f ||
        agent.ragdoll_visual_blend > 0.001f ||
        agent.humanoid.ragdoll.active || !agent.grounded ||
        fabsf(agent.position.y) > 0.01f || street_error > 0.18f) {
        (void)fprintf(stderr,
                      "%s: fall failed ragdoll %d recovery %d impact %d fall %.3fs frames %d-%d rebound %.3f particle %.3f frame %d node %d at %.3f %.3f %.3f gap %d blend %d/%.3f/%.3f authority %.4f/%.4f active %d grounded %d support %s control %.2f target %d pos %.3f %.3f %.3f error %.3f\n",
                      name, saw_ragdoll, saw_recovery, street_impact,
                      fall_duration, ragdoll_start_frame, street_impact_frame,
                      maximum_rebound_speed, maximum_particle_rebound,
                      maximum_particle_frame, maximum_particle_index,
                      maximum_particle_position.x,
                      maximum_particle_position.y,
                      maximum_particle_position.z, maximum_contact_gap,
                      saw_visual_transition, maximum_visual_blend_step,
                      agent.ragdoll_visual_blend,
                      maximum_authoritative_position_error,
                      maximum_authoritative_velocity_error,
                      agent.humanoid.ragdoll.active, agent.grounded,
                      CcHumanoidSupportStateName(agent.support_state),
                      agent.humanoid.control_authority,
                      agent.exact_target_valid,
                      agent.position.x, agent.position.y, agent.position.z,
                      street_error);
        exit(1);
    }
}

static float FallenBodyConstraintError(const CcBiomechRagdoll *ragdoll)
{
    float maximum = 0.0f;
    for (int32_t index = 0; index < ragdoll->constraint_count; ++index) {
        const CcBiomechRagdollConstraint *constraint =
            &ragdoll->constraints[index];
        const CcBiomechVec3 a =
            ragdoll->particles[constraint->particle_a].position;
        const CcBiomechVec3 b =
            ragdoll->particles[constraint->particle_b].position;
        float x = b.x - a.x;
        float y = b.y - a.y;
        float z = b.z - a.z;
        maximum = fmaxf(maximum,
                        fabsf(sqrtf(x * x + y * y + z * z) -
                              constraint->rest_length));
    }
    return maximum;
}

static float FallenBodyAngleViolation(const CcBiomechRagdoll *ragdoll)
{
    float maximum = 0.0f;
    for (int32_t index = 0; index < ragdoll->angle_constraint_count; ++index) {
        const CcBiomechRagdollAngleConstraint *constraint =
            &ragdoll->angle_constraints[index];
        const CcBiomechVec3 a =
            ragdoll->particles[constraint->particle_a].position;
        const CcBiomechVec3 joint =
            ragdoll->particles[constraint->joint_particle].position;
        const CcBiomechVec3 b =
            ragdoll->particles[constraint->particle_b].position;
        float ax = a.x - joint.x;
        float ay = a.y - joint.y;
        float az = a.z - joint.z;
        float bx = b.x - joint.x;
        float by = b.y - joint.y;
        float bz = b.z - joint.z;
        float denominator = sqrtf((ax * ax + ay * ay + az * az) *
                                  (bx * bx + by * by + bz * bz));
        if (denominator <= 0.000001f) continue;
        float cosine = (ax * bx + ay * by + az * bz) / denominator;
        cosine = fmaxf(-1.0f, fminf(1.0f, cosine));
        float angle = acosf(cosine);
        maximum = fmaxf(maximum,
                        fmaxf(constraint->minimum_angle - angle,
                              angle - constraint->maximum_angle));
    }
    for (int32_t index = 0; index < ragdoll->hinge_constraint_count; ++index) {
        const CcBiomechRagdollHingeConstraint *constraint =
            &ragdoll->hinge_constraints[index];
        const CcBiomechVec3 a =
            ragdoll->particles[constraint->particle_a].position;
        const CcBiomechVec3 joint =
            ragdoll->particles[constraint->joint_particle].position;
        const CcBiomechVec3 b =
            ragdoll->particles[constraint->particle_b].position;
        const CcBiomechVec3 axis_a =
            ragdoll->particles[constraint->axis_particle_a].position;
        const CcBiomechVec3 axis_b =
            ragdoll->particles[constraint->axis_particle_b].position;
        float ax = a.x - joint.x;
        float ay = a.y - joint.y;
        float az = a.z - joint.z;
        float bx = b.x - joint.x;
        float by = b.y - joint.y;
        float bz = b.z - joint.z;
        float denominator = sqrtf((ax * ax + ay * ay + az * az) *
                                  (bx * bx + by * by + bz * bz));
        if (denominator > 0.000001f) {
            float cosine = (ax * bx + ay * by + az * bz) / denominator;
            cosine = fmaxf(-1.0f, fminf(1.0f, cosine));
            float angle = acosf(cosine);
            maximum = fmaxf(maximum,
                            fmaxf(constraint->minimum_angle - angle,
                                  angle - constraint->maximum_angle));
        }
        float axis_x = axis_b.x - axis_a.x;
        float axis_y = axis_b.y - axis_a.y;
        float axis_z = axis_b.z - axis_a.z;
        float axis_length = sqrtf(axis_x * axis_x + axis_y * axis_y +
                                  axis_z * axis_z);
        float child_length = sqrtf(bx * bx + by * by + bz * bz);
        if (axis_length > 0.000001f && child_length > 0.000001f) {
            float lateral = (bx * axis_x + by * axis_y + bz * axis_z) /
                            axis_length;
            float splay = asinf(fminf(
                1.0f,
                fabsf(lateral - constraint->rest_lateral_offset) /
                    child_length));
            maximum = fmaxf(
                maximum, splay - constraint->maximum_splay_angle);
        }
    }
    return fmaxf(0.0f, maximum);
}

static float FallenBodyBoxPenetration(const CcBiomechRagdoll *ragdoll,
                                      Vector3 minimum, Vector3 maximum)
{
    float deepest = 0.0f;
    for (int32_t index = 0; index < ragdoll->particle_count; ++index) {
        const CcBiomechRagdollParticle *particle = &ragdoll->particles[index];
        float closest_x = fmaxf(minimum.x,
                                fminf(particle->position.x, maximum.x));
        float closest_y = fmaxf(minimum.y,
                                fminf(particle->position.y, maximum.y));
        float closest_z = fmaxf(minimum.z,
                                fminf(particle->position.z, maximum.z));
        float x = particle->position.x - closest_x;
        float y = particle->position.y - closest_y;
        float z = particle->position.z - closest_z;
        float distance = sqrtf(x * x + y * y + z * z);
        deepest = fmaxf(deepest, particle->radius - distance);
    }
    return deepest;
}

static float FallenBodyLowestPoint(const CcBiomechRagdoll *ragdoll)
{
    float lowest = 1000.0f;
    for (int32_t particle = 0; particle < ragdoll->particle_count; ++particle) {
        lowest = fminf(lowest,
                       ragdoll->particles[particle].position.y -
                           ragdoll->particles[particle].radius);
    }
    static const float samples[] = {0.25f, 0.50f, 0.75f};
    for (int32_t segment_index = 0;
         segment_index < ragdoll->collision_segment_count; ++segment_index) {
        const CcBiomechRagdollCollisionSegment *segment =
            &ragdoll->collision_segments[segment_index];
        const CcBiomechVec3 a =
            ragdoll->particles[segment->particle_a].position;
        const CcBiomechVec3 b =
            ragdoll->particles[segment->particle_b].position;
        for (int32_t sample = 0;
             sample < (int32_t)(sizeof(samples) / sizeof(samples[0]));
             ++sample) {
            float y = a.y + (b.y - a.y) * samples[sample];
            lowest = fminf(lowest, y - segment->radius);
        }
    }
    return lowest;
}

static float FallenBodyShoulderFirstRoll(const CcBiomechRagdoll *ragdoll,
                                         int32_t shoulder_particle)
{
    CcBiomechVec3 center = CcBiomechRagdollCenterOfMass(ragdoll);
    float best_roll = 0.0f;
    float best_margin = -1000.0f;
    static const float samples[] = {0.25f, 0.50f, 0.75f};
    for (int32_t step = 0; step < 720; ++step) {
        float roll = (float)step / 720.0f * 6.28318530718f;
        float sine = sinf(roll);
        float cosine = cosf(roll);
        const CcBiomechRagdollParticle *shoulder =
            &ragdoll->particles[shoulder_particle];
        float shoulder_y = center.y +
            (shoulder->position.x - center.x) * sine +
            (shoulder->position.y - center.y) * cosine;
        float shoulder_bottom = shoulder_y - shoulder->radius;
        float lowest = 1000.0f;
        for (int32_t particle = 0;
             particle < ragdoll->particle_count; ++particle) {
            const CcBiomechRagdollParticle *body =
                &ragdoll->particles[particle];
            float y = center.y + (body->position.x - center.x) * sine +
                      (body->position.y - center.y) * cosine;
            lowest = fminf(lowest, y - body->radius);
        }
        for (int32_t segment_index = 0;
             segment_index < ragdoll->collision_segment_count;
             ++segment_index) {
            const CcBiomechRagdollCollisionSegment *segment =
                &ragdoll->collision_segments[segment_index];
            const CcBiomechVec3 a =
                ragdoll->particles[segment->particle_a].position;
            const CcBiomechVec3 b =
                ragdoll->particles[segment->particle_b].position;
            for (int32_t sample = 0;
                 sample < (int32_t)(sizeof(samples) / sizeof(samples[0]));
                 ++sample) {
                float amount = samples[sample];
                float x = a.x + (b.x - a.x) * amount;
                float y = a.y + (b.y - a.y) * amount;
                float rotated_y = center.y + (x - center.x) * sine +
                                  (y - center.y) * cosine;
                lowest = fminf(lowest, rotated_y - segment->radius);
            }
        }
        float margin = lowest - shoulder_bottom;
        if (margin > best_margin) {
            best_margin = margin;
            best_roll = roll;
        }
    }
    return best_roll;
}

static void PlaceFallenBody(CcLocalAgent *agent, float roll,
                            Vector3 translation, Vector3 velocity)
{
    if (!agent->humanoid.ragdoll.active &&
        !CcHumanoidGaitKnockDown(&agent->humanoid)) {
        (void)fprintf(stderr, "fallen-body fixture could not start physics\n");
        exit(1);
    }
    CcBiomechRagdoll *ragdoll = &agent->humanoid.ragdoll;
    CcBiomechVec3 center = CcBiomechRagdollCenterOfMass(ragdoll);
    float cosine = cosf(roll);
    float sine = sinf(roll);
    for (int32_t index = 0; index < ragdoll->particle_count; ++index) {
        CcBiomechRagdollParticle *particle = &ragdoll->particles[index];
        float x = particle->position.x - center.x;
        float y = particle->position.y - center.y;
        particle->position.x = center.x + x * cosine - y * sine +
                               translation.x;
        particle->position.y = center.y + x * sine + y * cosine +
                               translation.y;
        particle->position.z += translation.z;
        particle->previous_position = particle->position;
    }
    agent->position.x += translation.x;
    agent->position.y += translation.y;
    agent->position.z += translation.z;
    agent->grounded = false;
    agent->exact_target_valid = false;
    CcBiomechRagdollSetVelocity(
        ragdoll, (CcBiomechVec3){velocity.x, velocity.y, velocity.z},
        1.0f / 60.0f);
}

static void TestWallScrapeAndCornerImpact(void)
{
    const Vector3 tower_minimum = {3.0f, 0.0f, 7.0f};
    const Vector3 tower_maximum = {4.0f, 1.65f, 8.0f};
    const float half_pi = 1.57079632679f;

    CcLocalAgent scrape;
    CcLocalAgentInit(&scrape, (Vector2){3.50f, 6.46f}, false);
    PlaceFallenBody(&scrape, half_pi, (Vector3){0.0f, 0.48f, 0.0f},
                    (Vector3){0.0f, -0.55f, 1.45f});
    int32_t side_contact_frames = 0;
    float first_side_center_y = 0.0f;
    float lowest_side_center_y = 1000.0f;
    float maximum_constraint_error = 0.0f;
    float maximum_angle_violation = 0.0f;
    float maximum_penetration = 0.0f;
    float maximum_position_step = 0.0f;
    Vector3 previous_position = scrape.position;
    bool recovered = false;
    for (int32_t frame = 0; frame < 900; ++frame) {
        CcLocalAgentUpdate(&scrape, 1.0f / 60.0f, false);
        maximum_position_step = fmaxf(
            maximum_position_step,
            VectorDistance3(previous_position, scrape.position));
        previous_position = scrape.position;
        if (scrape.humanoid.ragdoll.active) {
            CcBiomechRagdoll *ragdoll = &scrape.humanoid.ragdoll;
            CcBiomechVec3 center = CcBiomechRagdollCenterOfMass(ragdoll);
            bool side_contact = false;
            for (int32_t particle = 0;
                 particle < ragdoll->particle_count; ++particle) {
                const CcBiomechRagdollParticle *body =
                    &ragdoll->particles[particle];
                side_contact = side_contact ||
                    (body->collided && body->contact_normal.z < -0.70f);
            }
            if (side_contact) {
                if (side_contact_frames == 0) first_side_center_y = center.y;
                side_contact_frames += 1;
                lowest_side_center_y = fminf(lowest_side_center_y, center.y);
            }
            maximum_constraint_error = fmaxf(
                maximum_constraint_error,
                FallenBodyConstraintError(ragdoll));
            maximum_angle_violation = fmaxf(
                maximum_angle_violation,
                FallenBodyAngleViolation(ragdoll));
            maximum_penetration = fmaxf(
                maximum_penetration,
                FallenBodyBoxPenetration(
                    ragdoll, tower_minimum, tower_maximum));
        }
        recovered = recovered || scrape.humanoid.recovering;
    }
    if (side_contact_frames < 4 ||
        first_side_center_y - lowest_side_center_y < 0.06f ||
        maximum_constraint_error > 0.012f ||
        maximum_angle_violation > 0.09f || maximum_penetration > 0.018f ||
        maximum_position_step > 0.12f ||
        !recovered || scrape.humanoid.ragdoll.active || !scrape.grounded) {
        (void)fprintf(stderr,
                      "wall scrape failed contacts %d drop %.3f bone %.3f angle %.3f penetration %.3f step %.3f recovery %d/%d time %.2f error %.3f speed %.3f support %d active %d grounded %d\n",
                      side_contact_frames,
                      first_side_center_y - lowest_side_center_y,
                      maximum_constraint_error, maximum_angle_violation,
                      maximum_penetration, maximum_position_step, recovered,
                      scrape.humanoid.recovering,
                      scrape.humanoid.recovery_time,
                      scrape.humanoid.recovery_error,
                      scrape.humanoid.recovery_speed,
                      CcHumanoidGaitRagdollSupportContactCount(
                          &scrape.humanoid),
                      scrape.humanoid.ragdoll.active, scrape.grounded);
        exit(1);
    }

    CcLocalAgent corner;
    CcLocalAgentInit(&corner, (Vector2){2.48f, 6.48f}, false);
    PlaceFallenBody(&corner, half_pi, (Vector3){0.0f, 0.48f, 0.0f},
                    (Vector3){1.35f, -0.45f, 1.35f});
    bool saw_x_contact = false;
    bool saw_z_contact = false;
    maximum_constraint_error = 0.0f;
    maximum_angle_violation = 0.0f;
    maximum_penetration = 0.0f;
    maximum_position_step = 0.0f;
    previous_position = corner.position;
    recovered = false;
    for (int32_t frame = 0; frame < 900; ++frame) {
        CcLocalAgentUpdate(&corner, 1.0f / 60.0f, false);
        maximum_position_step = fmaxf(
            maximum_position_step,
            VectorDistance3(previous_position, corner.position));
        previous_position = corner.position;
        if (corner.humanoid.ragdoll.active) {
            CcBiomechRagdoll *ragdoll = &corner.humanoid.ragdoll;
            for (int32_t particle = 0;
                 particle < ragdoll->particle_count; ++particle) {
                const CcBiomechRagdollParticle *body =
                    &ragdoll->particles[particle];
                saw_x_contact = saw_x_contact ||
                    (body->collided && body->contact_normal.x < -0.55f);
                saw_z_contact = saw_z_contact ||
                    (body->collided && body->contact_normal.z < -0.55f);
            }
            maximum_constraint_error = fmaxf(
                maximum_constraint_error,
                FallenBodyConstraintError(ragdoll));
            maximum_angle_violation = fmaxf(
                maximum_angle_violation,
                FallenBodyAngleViolation(ragdoll));
            maximum_penetration = fmaxf(
                maximum_penetration,
                FallenBodyBoxPenetration(
                    ragdoll, tower_minimum, tower_maximum));
        }
        recovered = recovered || corner.humanoid.recovering;
    }
    if (!saw_x_contact || !saw_z_contact ||
        maximum_constraint_error > 0.012f ||
        maximum_angle_violation > 0.09f || maximum_penetration > 0.018f ||
        maximum_position_step > 0.12f ||
        !recovered || corner.humanoid.ragdoll.active || !corner.grounded) {
        (void)fprintf(stderr,
                      "corner impact failed contacts %d/%d bone %.3f angle %.3f penetration %.3f step %.3f recovery %d/%d time %.2f error %.3f speed %.3f support %d active %d grounded %d\n",
                      saw_x_contact, saw_z_contact,
                      maximum_constraint_error, maximum_angle_violation,
                      maximum_penetration, maximum_position_step, recovered,
                      corner.humanoid.recovering,
                      corner.humanoid.recovery_time,
                      corner.humanoid.recovery_error,
                      corner.humanoid.recovery_speed,
                      CcHumanoidGaitRagdollSupportContactCount(
                          &corner.humanoid),
                      corner.humanoid.ragdoll.active, corner.grounded);
        exit(1);
    }
}

static void TestShoulderLanding(void)
{
    const int32_t left_shoulder_particle = 11;
    CcLocalAgent agent;
    CcLocalAgentInit(&agent, (Vector2){4.25f, 5.50f}, true);
    CcLimbVec3 shoulder_pose = agent.humanoid.pose.shoulder[0];
    agent.humanoid.pose.elbow[0] = (CcLimbVec3){
        shoulder_pose.x, shoulder_pose.y, shoulder_pose.z + 0.34f};
    agent.humanoid.pose.hand[0] = (CcLimbVec3){
        shoulder_pose.x, shoulder_pose.y + 0.08f,
        shoulder_pose.z + 0.34f + 0.34075f};
    agent.humanoid.previous_pose = agent.humanoid.pose;
    if (!CcHumanoidGaitKnockDown(&agent.humanoid)) {
        (void)fprintf(stderr,
                      "shoulder landing could not start fallen-body physics\n");
        exit(1);
    }
    float shoulder_first_roll = FallenBodyShoulderFirstRoll(
        &agent.humanoid.ragdoll, left_shoulder_particle);
    PlaceFallenBody(&agent, shoulder_first_roll,
                    (Vector3){0.0f, 2.0f, 0.0f},
                    (Vector3){0.0f, -1.40f, 0.0f});
    /* Tuck the lower arm across the body so this is a shoulder landing,
       instead of turning into a hand-first brace before impact. */
    CcBiomechRagdoll *placed = &agent.humanoid.ragdoll;
    float vertical_adjustment = 0.12f - FallenBodyLowestPoint(placed);
    for (int32_t particle = 0;
         particle < placed->particle_count; ++particle) {
        placed->particles[particle].position.y += vertical_adjustment;
        placed->particles[particle].previous_position =
            placed->particles[particle].position;
    }
    agent.position.y += vertical_adjustment;
    float initial_lowest_body = FallenBodyLowestPoint(placed);
    CcBiomechRagdollSetVelocity(
        placed, (CcBiomechVec3){0.0f, -1.40f, 0.0f}, 1.0f / 60.0f);
    int32_t first_impact_frame = -1;
    int32_t shoulder_impact_frame = -1;
    float maximum_constraint_error = 0.0f;
    float maximum_angle_violation = 0.0f;
    float deepest_ground_penetration = 0.0f;
    float maximum_position_step = 0.0f;
    Vector3 previous_position = agent.position;
    float minimum_shoulder_clearance = 1000.0f;
    uint32_t impact_mask = 0;
    bool recovered = false;
    for (int32_t frame = 0; frame < 900; ++frame) {
        CcLocalAgentUpdate(&agent, 1.0f / 60.0f, true);
        maximum_position_step = fmaxf(
            maximum_position_step,
            VectorDistance3(previous_position, agent.position));
        previous_position = agent.position;
        if (agent.humanoid.ragdoll.active) {
            CcBiomechRagdoll *ragdoll = &agent.humanoid.ragdoll;
            for (int32_t particle = 0;
                 particle < ragdoll->particle_count; ++particle) {
                const CcBiomechRagdollParticle *body =
                    &ragdoll->particles[particle];
                bool ground_contact = body->collided &&
                                      body->contact_normal.y > 0.70f;
                if (ground_contact && first_impact_frame < 0) {
                    first_impact_frame = frame;
                }
                if (ground_contact) impact_mask |= UINT32_C(1) << particle;
                deepest_ground_penetration = fmaxf(
                    deepest_ground_penetration,
                    body->radius - body->position.y);
            }
            const CcBiomechRagdollParticle *shoulder =
                &ragdoll->particles[left_shoulder_particle];
            minimum_shoulder_clearance = fminf(
                minimum_shoulder_clearance,
                shoulder->position.y - shoulder->radius);
            if (shoulder->collided && shoulder->contact_normal.y > 0.70f &&
                shoulder_impact_frame < 0) {
                shoulder_impact_frame = frame;
            }
            maximum_constraint_error = fmaxf(
                maximum_constraint_error,
                FallenBodyConstraintError(ragdoll));
            maximum_angle_violation = fmaxf(
                maximum_angle_violation,
                FallenBodyAngleViolation(ragdoll));
        }
        recovered = recovered || agent.humanoid.recovering;
    }
    if (first_impact_frame < 0 || shoulder_impact_frame < 0 ||
        shoulder_impact_frame - first_impact_frame > 30 ||
        maximum_constraint_error > 0.012f ||
        maximum_angle_violation > 0.09f ||
        deepest_ground_penetration > 0.018f ||
        maximum_position_step > 0.12f || !recovered ||
        agent.humanoid.ragdoll.active || !agent.grounded) {
        (void)fprintf(stderr,
                      "shoulder landing failed frames %d/%d mask %08x initial %.3f clearance %.3f bone %.3f angle %.3f penetration %.3f step %.3f recovery %d/%d time %.2f error %.3f speed %.3f support %d active %d grounded %d\n",
                      first_impact_frame, shoulder_impact_frame,
                      impact_mask, initial_lowest_body,
                      minimum_shoulder_clearance,
                      maximum_constraint_error, maximum_angle_violation,
                      deepest_ground_penetration, maximum_position_step,
                      recovered,
                      agent.humanoid.recovering,
                      agent.humanoid.recovery_time,
                      agent.humanoid.recovery_error,
                      agent.humanoid.recovery_speed,
                      CcHumanoidGaitRagdollSupportContactCount(
                          &agent.humanoid),
                      agent.humanoid.ragdoll.active, agent.grounded);
        exit(1);
    }
}

static void InitCombatant(CcLocalAgent *agent, Vector2 position,
                          float facing, CcCombatTeam team)
{
    CcLocalAgentInit(agent, position, true);
    agent->facing_yaw = facing;
    CcLocalAgentSetMorphology(agent, CC_MORPHOLOGY_BIPED, true);
    CcLocalCombatSetTeam(agent, team);
}

static CcCombatOutcome RunCombatStrike(CcLocalAgent *attacker,
                                       CcLocalAgent *defender)
{
    if (!CcLocalCombatBeginStrike(attacker, defender)) {
        (void)fprintf(stderr, "combat test rejected a valid strike\n");
        exit(1);
    }
    for (int32_t frame = 0; frame < 120; ++frame) {
        CcLocalAgentUpdate(attacker, 1.0f / 60.0f, true);
        CcLocalAgentUpdate(defender, 1.0f / 60.0f, true);
        if (CcHumanoidGaitConsumeStrikeImpact(&attacker->humanoid)) {
            return CcLocalCombatResolveStrike(attacker, defender);
        }
    }
    (void)fprintf(stderr, "combat test strike emitted no impact window\n");
    exit(1);
}

static void TestSharedCombat(void)
{
    CcLocalAgent attacker;
    CcLocalAgent defender;
    InitCombatant(&attacker, (Vector2){4.0f, 3.0f}, 0.0f,
                  CC_COMBAT_PLAYER);
    InitCombatant(&defender, (Vector2){4.0f, 3.82f}, PI,
                  CC_COMBAT_RAIDER);
    CcCombatOutcome outcome = RunCombatStrike(&attacker, &defender);
    if (outcome != CC_COMBAT_OUTCOME_HIT || defender.combat.health >=
        CC_LOCAL_COMBAT_MAX_HEALTH) {
        (void)fprintf(stderr,
                      "in-range frontal strike failed: outcome %d health %.1f\n",
                      outcome, defender.combat.health);
        exit(1);
    }
    float baseline_damage = CC_LOCAL_COMBAT_MAX_HEALTH -
                            defender.combat.health;
    if (!defender.combat.impact_valid || defender.combat.impact_speed < 2.0f ||
        defender.combat.impact_point.y < defender.position.y + 0.25f ||
        defender.combat.impact_point.y > defender.position.y + 1.70f ||
        defender.humanoid.impact_response <= 0.0f) {
        (void)fprintf(stderr,
                      "strike did not produce a localized physical contact\n");
        exit(1);
    }
    if (CcLocalCombatResolveStrike(&attacker, &defender) !=
        CC_COMBAT_OUTCOME_NONE) {
        (void)fprintf(stderr, "one swing damaged the same target twice\n");
        exit(1);
    }

    InitCombatant(&attacker, (Vector2){4.0f, 3.0f}, 0.0f,
                  CC_COMBAT_GUARD);
    InitCombatant(&defender, (Vector2){4.0f, 3.82f}, PI,
                  CC_COMBAT_PLAYER);
    outcome = RunCombatStrike(&attacker, &defender);
    if (outcome != CC_COMBAT_OUTCOME_MISS || defender.combat.health !=
        CC_LOCAL_COMBAT_MAX_HEALTH) {
        (void)fprintf(stderr,
                      "allied strike caused damage: outcome %d health %.1f\n",
                      outcome, defender.combat.health);
        exit(1);
    }

    InitCombatant(&attacker, (Vector2){4.0f, 2.0f}, 0.0f,
                  CC_COMBAT_PLAYER);
    InitCombatant(&defender, (Vector2){4.0f, 4.20f}, PI,
                  CC_COMBAT_RAIDER);
    outcome = RunCombatStrike(&attacker, &defender);
    if (outcome != CC_COMBAT_OUTCOME_MISS || defender.combat.health !=
        CC_LOCAL_COMBAT_MAX_HEALTH) {
        (void)fprintf(stderr,
                      "out-of-range strike did not whiff: outcome %d health %.1f\n",
                      outcome, defender.combat.health);
        exit(1);
    }

    InitCombatant(&attacker, (Vector2){0.75f, 3.00f}, 0.5f * PI,
                  CC_COMBAT_PLAYER);
    InitCombatant(&defender, (Vector2){2.17f, 3.00f}, -0.5f * PI,
                  CC_COMBAT_RAIDER);
    outcome = RunCombatStrike(&attacker, &defender);
    if (outcome != CC_COMBAT_OUTCOME_MISS || defender.combat.health !=
            CC_LOCAL_COMBAT_MAX_HEALTH ||
        !attacker.combat.impact_valid) {
        (void)fprintf(stderr,
                      "shared world collision let a strike pass through the market shelf: outcome %d health %.1f impact %d\n",
                      outcome, defender.combat.health,
                      attacker.combat.impact_valid);
        exit(1);
    }

    InitCombatant(&attacker, (Vector2){4.0f, 3.0f}, 0.0f,
                  CC_COMBAT_PLAYER);
    InitCombatant(&defender, (Vector2){4.0f, 3.82f}, PI,
                  CC_COMBAT_RAIDER);
    CcLocalCombatSetGuarded(&defender, &attacker, true);
    outcome = RunCombatStrike(&attacker, &defender);
    if (outcome != CC_COMBAT_OUTCOME_BLOCKED || defender.combat.health !=
        CC_LOCAL_COMBAT_MAX_HEALTH || defender.combat.posture >=
        CC_LOCAL_COMBAT_MAX_POSTURE) {
        (void)fprintf(stderr,
                      "frontal guard failed: outcome %d health %.1f posture %.1f\n",
                      outcome, defender.combat.health,
                      defender.combat.posture);
        exit(1);
    }

    InitCombatant(&attacker, (Vector2){4.0f, 3.0f}, 0.0f,
                  CC_COMBAT_PLAYER);
    InitCombatant(&defender, (Vector2){4.0f, 3.82f}, 0.0f,
                  CC_COMBAT_RAIDER);
    CcHumanoidGaitSetGuarded(&defender.humanoid, true);
    outcome = RunCombatStrike(&attacker, &defender);
    if (outcome != CC_COMBAT_OUTCOME_HIT || defender.combat.health >=
        CC_LOCAL_COMBAT_MAX_HEALTH) {
        (void)fprintf(stderr,
                      "rear strike was incorrectly blocked: outcome %d health %.1f\n",
                      outcome, defender.combat.health);
        exit(1);
    }

    InitCombatant(&attacker, (Vector2){4.0f, 3.0f}, 0.0f,
                  CC_COMBAT_PLAYER);
    InitCombatant(&defender, (Vector2){4.0f, 3.90f}, PI,
                  CC_COMBAT_RAIDER);
    if (!CcLocalAgentSetExactTarget(
            &attacker, (Vector3){7.0f, 0.0f, 3.0f}, true) ||
        !CcLocalCombatBeginStrike(&attacker, &defender)) {
        (void)fprintf(stderr, "combat movement setup failed\n");
        exit(1);
    }
    for (int32_t frame = 0; frame < 18; ++frame) {
        CcLocalAgentUpdate(&attacker, 1.0f / 60.0f, true);
    }
    float focus_x = defender.position.x - attacker.position.x;
    float focus_z = defender.position.z - attacker.position.z;
    float focus_length = sqrtf(focus_x * focus_x + focus_z * focus_z);
    float facing_dot = focus_length > 0.0001f ?
        (sinf(attacker.facing_yaw) * focus_x +
         cosf(attacker.facing_yaw) * focus_z) / focus_length : 1.0f;
    if (facing_dot < 0.92f || fabsf(attacker.position.x - 4.0f) > 0.12f) {
        (void)fprintf(stderr,
                      "strike did not own facing and movement: dot %.2f x %.2f\n",
                      facing_dot, attacker.position.x);
        exit(1);
    }

    InitCombatant(&attacker, (Vector2){4.0f, 3.0f}, 0.0f,
                  CC_COMBAT_PLAYER);
    InitCombatant(&defender, (Vector2){4.0f, 3.82f}, PI,
                  CC_COMBAT_RAIDER);
    CcLocalAgentSetAthleticLevel(&attacker, CC_ATHLETIC_POWER,
                                 CC_ATHLETIC_MAX_LEVEL);
    outcome = RunCombatStrike(&attacker, &defender);
    float heroic_damage = CC_LOCAL_COMBAT_MAX_HEALTH -
                          defender.combat.health;
    if (outcome != CC_COMBAT_OUTCOME_HIT ||
        heroic_damage <= baseline_damage + 3.0f) {
        (void)fprintf(stderr,
                      "power training did not increase contact impulse: %.1f vs %.1f\n",
                      heroic_damage, baseline_damage);
        exit(1);
    }
}

static void TestDeathLifecycle(void)
{
    CcLocalAgent attacker;
    CcLocalAgent corpse;
    InitCombatant(&attacker, (Vector2){4.0f, 3.0f}, 0.0f,
                  CC_COMBAT_PLAYER);
    InitCombatant(&corpse, (Vector2){4.0f, 3.82f}, PI,
                  CC_COMBAT_RAIDER);
    corpse.combat.health = 1.0f;
    if (RunCombatStrike(&attacker, &corpse) != CC_COMBAT_OUTCOME_DEFEATED ||
        corpse.combat.life_state != CC_LIFE_DEAD ||
        !corpse.humanoid.ragdoll.active || corpse.humanoid.recovering ||
        corpse.humanoid.ragdoll_recovery_allowed ||
        corpse.combat.weapon_mode != CC_WEAPON_RAGDOLL_ATTACHED) {
        (void)fprintf(stderr,
                      "lethal strike did not enter an authoritative dead ragdoll state\n");
        exit(1);
    }
    Vector3 impact_velocity = RagdollCenterVelocity(
        &corpse.humanoid.ragdoll, 1.0f / 60.0f);
    Vector3 impact_direction = corpse.combat.impact_direction;
    float impact_length = sqrtf(impact_direction.x * impact_direction.x +
                                impact_direction.y * impact_direction.y +
                                impact_direction.z * impact_direction.z);
    float inherited_impact = impact_length > 0.0001f ?
        (impact_velocity.x * impact_direction.x +
         impact_velocity.y * impact_direction.y +
         impact_velocity.z * impact_direction.z) / impact_length : 0.0f;
    if (inherited_impact < 0.20f) {
        (void)fprintf(stderr,
                      "lethal impulse did not transfer to ragdoll particles: %.3f\n",
                      inherited_impact);
        exit(1);
    }
    if (CcLocalAgentSetExactTarget(
            &corpse, (Vector3){5.0f, 0.0f, 5.0f}, true)) {
        (void)fprintf(stderr, "dead agent accepted a movement target\n");
        exit(1);
    }

    bool saw_physics_step = false;
    for (int32_t frame = 0; frame < 720; ++frame) {
        CcHumanoidPose prior_pose = corpse.humanoid.pose;
        float prior_time = corpse.humanoid.ragdoll_time;
        CcLocalAgentUpdate(&corpse, 1.0f / 60.0f, true);
        if (corpse.humanoid.ragdoll_time > prior_time) {
            saw_physics_step = true;
            if (MaximumPoseStep(&prior_pose,
                                &corpse.humanoid.previous_pose) > 0.0001f) {
                (void)fprintf(stderr,
                              "ragdoll previous pose did not follow physics history\n");
                exit(1);
            }
        }
    }
    if (!saw_physics_step || corpse.combat.life_state != CC_LIFE_DEAD ||
        corpse.combat.health != 0.0f || !corpse.humanoid.ragdoll.active ||
        corpse.humanoid.recovering) {
        (void)fprintf(stderr,
                      "defeated enemy recovered or left its corpse state\n");
        exit(1);
    }

    CcLocalAgent player;
    InitCombatant(&attacker, (Vector2){4.0f, 3.0f}, 0.0f,
                  CC_COMBAT_RAIDER);
    InitCombatant(&player, (Vector2){4.0f, 3.82f}, PI,
                  CC_COMBAT_PLAYER);
    player.combat.health = 1.0f;
    if (RunCombatStrike(&attacker, &player) != CC_COMBAT_OUTCOME_DEFEATED) {
        (void)fprintf(stderr, "player death fixture did not land lethally\n");
        exit(1);
    }
    bool saw_respawning = false;
    bool saw_get_up = false;
    bool resumed_inside_ragdoll = false;
    for (int32_t frame = 0; frame < 900; ++frame) {
        CcLocalAgentUpdate(&player, 1.0f / 60.0f, true);
        saw_respawning = saw_respawning ||
            player.combat.life_state == CC_LIFE_RESPAWNING;
        saw_get_up = saw_get_up || player.humanoid.recovering;
        resumed_inside_ragdoll = resumed_inside_ragdoll ||
            (player.combat.life_state == CC_LIFE_ALIVE &&
             player.humanoid.ragdoll.active);
        if (player.combat.life_state == CC_LIFE_ALIVE) break;
    }
    if (!saw_respawning || !saw_get_up || resumed_inside_ragdoll ||
        player.combat.life_state != CC_LIFE_ALIVE ||
        player.humanoid.ragdoll.active || player.humanoid.recovering ||
        player.combat.health != 45.0f ||
        player.combat.weapon_mode != CC_WEAPON_HELD) {
        (void)fprintf(stderr,
                      "player respawn was not synchronized with physical get-up\n");
        exit(1);
    }

    CcSim sim;
    CcSimInit(&sim, 917U);
    CcLocalCourse course;
    CcLocalCourseInit(&course);
    CcLocalAgent course_player;
    CcLocalAgentInit(&course_player,
                     (Vector2){CC_LOCAL_START_X, CC_LOCAL_START_Z}, false);
    CcLocalCombatSetTeam(&course_player, CC_COMBAT_PLAYER);
    CcLocalCourseRaiseAlarmNear(&course, &course_player);
    CcLocalAgent *dead_raider = &course.raiders[0];
    dead_raider->combat.health = 0.0f;
    dead_raider->combat.life_state = CC_LIFE_DEAD;
    dead_raider->combat.weapon_mode = CC_WEAPON_RAGDOLL_ATTACHED;
    (void)CcHumanoidGaitDie(
        &dead_raider->humanoid, (CcLimbVec3){1.0f, 0.0f, 0.0f},
        dead_raider->humanoid.pose.chest, 1.0f);
    course.raiders[1].combat.health = 50.0f;
    bool saw_retreat = false;
    for (int32_t frame = 0; frame < 180; ++frame) {
        CcLocalCourseUpdate(&course, &course_player, &sim, 1.0f / 60.0f);
        saw_retreat = saw_retreat || course.raiders_retreating;
    }
    if (!saw_retreat ||
        dead_raider->combat.life_state != CC_LIFE_DEAD ||
        dead_raider->combat.health != 0.0f ||
        dead_raider->humanoid.recovering) {
        (void)fprintf(stderr,
                      "morale retreat revived or mobilized a defeated raider: retreat %d state %d health %.1f recovering %d\n",
                      saw_retreat,
                      dead_raider->combat.life_state,
                      dead_raider->combat.health,
                      dead_raider->humanoid.recovering);
        exit(1);
    }
}

static void TestTargetDrivenCombat(void)
{
    CcSim sim;
    CcSimInit(&sim, 91U);
    CcLocalCourse course;
    CcLocalCourseInit(&course);
    CcLocalAgent player;
    CcLocalAgentInit(&player,
                     (Vector2){CC_LOCAL_START_X, CC_LOCAL_START_Z}, false);
    CcLocalCombatSetTeam(&player, CC_COMBAT_PLAYER);
    CcLocalCourseRaiseAlarmNear(&course, &player);
    for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
        course.runners[i].agent.combat.life_state = CC_LIFE_DEAD;
        course.runners[i].agent.combat.health = 0.0f;
    }
    CcLocalAgentInit(&course.raiders[0],
                     (Vector2){CC_LOCAL_START_X + 3.0f,
                               CC_LOCAL_START_Z}, false);
    CcLocalCombatSetTeam(&course.raiders[0], CC_COMBAT_RAIDER);
    CcLocalAgentInit(&course.raiders[1],
                     (Vector2){CC_LOCAL_START_X + 7.0f,
                               CC_LOCAL_START_Z + 3.0f}, false);
    CcLocalCombatSetTeam(&course.raiders[1], CC_COMBAT_RAIDER);
    for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
        course.raider_response_stage[i] = 3;
        course.raider_response_waypoint_active[i] = false;
    }

    player.combat.posture = 30.0f;
    if (CcLocalCourseBeginPlayerStrike(&course, &player) ||
        CcLocalCourseSetPlayerGuarded(&course, &player, true) ||
        CcLocalCourseUsePlayerSkill(&course, &player,
                                    CC_COMBAT_SKILL_SECOND_WIND) ||
        player.combat.target_index != -1 || player.combat.focus_valid ||
        player.humanoid.guard_requested || player.combat.posture != 30.0f) {
        (void)fprintf(stderr,
                      "combat activated before a hostile was targeted\n");
        exit(1);
    }
    if (CcLocalCourseSelectPlayerTarget(&course, &player, -1) ||
        !CcLocalCourseSelectPlayerTarget(&course, &player, 0) ||
        player.combat.target_index != 0 || !player.combat.focus_valid) {
        (void)fprintf(stderr, "explicit hostile targeting failed\n");
        exit(1);
    }
    if (!CcLocalCourseSetPlayerGuarded(&course, &player, true) ||
        !player.humanoid.guard_requested ||
        !CcLocalCourseSetPlayerGuarded(&course, &player, false) ||
        player.humanoid.guard_requested ||
        player.combat.target_index != 0 || !player.combat.focus_valid) {
        (void)fprintf(stderr,
                      "targeted guard did not preserve hostile focus\n");
        exit(1);
    }
    if (!CcLocalCourseUsePlayerSkill(&course, &player,
                                     CC_COMBAT_SKILL_SUNDER) ||
        player.combat.queued_skill != (int32_t)CC_COMBAT_SKILL_SUNDER) {
        (void)fprintf(stderr, "targeted combat skill did not queue\n");
        exit(1);
    }
    player.combat.health = 42.0f;
    player.combat.posture = 30.0f;
    if (!CcLocalCourseUsePlayerSkill(&course, &player,
                                     CC_COMBAT_SKILL_SECOND_WIND) ||
        player.combat.health != 42.0f || player.combat.posture <= 30.0f ||
        CcLocalCombatSkillCooldown(&player,
            CC_COMBAT_SKILL_SECOND_WIND) <= 0.0f) {
        (void)fprintf(stderr,
                      "Catch Breath healed a wound or failed to restore posture\n");
        exit(1);
    }

    float initial_distance = VectorDistance3(player.position,
                                              course.raiders[0].position);
    float minimum_distance = initial_distance;
    bool saw_sunder_style = false;
    for (int32_t frame = 0; frame < 300; ++frame) {
        CcLocalAgentUpdate(&player, 1.0f / 60.0f, false);
        CcLocalCourseUpdate(&course, &player, &sim, 1.0f / 60.0f);
        saw_sunder_style = saw_sunder_style ||
            (player.humanoid.action == CC_HUMANOID_ACTION_STRIKE &&
             player.combat.active_skill == CC_COMBAT_SKILL_SUNDER &&
             player.humanoid.strike_style == CC_HUMANOID_STRIKE_SWEEP);
        minimum_distance = fminf(
            minimum_distance,
            VectorDistance3(player.position, course.raiders[0].position));
    }
    if (minimum_distance < 1.08f || minimum_distance > 1.35f ||
        !saw_sunder_style ||
        (course.raiders[0].combat.health >= CC_LOCAL_COMBAT_MAX_HEALTH &&
         course.raiders[0].combat.posture >= CC_LOCAL_COMBAT_MAX_POSTURE) ||
        CcLocalCombatSkillCooldown(&player, CC_COMBAT_SKILL_SUNDER) <= 0.0f) {
        (void)fprintf(stderr,
                      "target combat did not hold weapon range and exchange styled attacks: distance %.2f style %d health %.1f posture %.1f cooldown %.1f\n",
                      minimum_distance, saw_sunder_style,
                      course.raiders[0].combat.health,
                      course.raiders[0].combat.posture,
                      CcLocalCombatSkillCooldown(
                          &player, CC_COMBAT_SKILL_SUNDER));
        exit(1);
    }
    bool damaging_outcome =
        course.last_outcome != CC_COMBAT_OUTCOME_NONE &&
        course.last_outcome != CC_COMBAT_OUTCOME_MISS;
    if (course.last_outcome == CC_COMBAT_OUTCOME_NONE ||
        course.last_attacker_team == CC_COMBAT_NEUTRAL ||
        course.last_defender_team == CC_COMBAT_NEUTRAL ||
        (damaging_outcome && course.last_health_damage < 0.5f &&
         course.last_posture_damage < 0.5f)) {
        (void)fprintf(stderr,
                      "combat feedback event was incomplete: outcome %d attacker %d defender %d health %.1f posture %.1f\n",
                      course.last_outcome, course.last_attacker_team,
                      course.last_defender_team,
                      course.last_health_damage,
                      course.last_posture_damage);
        exit(1);
    }
    CcLocalCourseClearPlayerTarget(&player);
    if (player.combat.target_index != -1 || player.combat.focus_valid ||
        player.combat.queued_skill != -1 || player.humanoid.guard_requested) {
        (void)fprintf(stderr, "combat target did not disengage cleanly\n");
        exit(1);
    }
    for (int32_t frame = 0; frame < 90; ++frame) {
        CcLocalAgentUpdate(&player, 1.0f / 60.0f, false);
    }
    if (player.humanoid.action == CC_HUMANOID_ACTION_GUARD) {
        (void)fprintf(stderr,
                      "disengaged combat target left a stale forward guard pose\n");
        exit(1);
    }
}

static void TestCombatStanceStability(void)
{
    CcLocalAgent guard;
    CcLocalAgent target;
    InitCombatant(&guard, (Vector2){4.0f, 3.0f}, 0.0f,
                  CC_COMBAT_PLAYER);
    InitCombatant(&target, (Vector2){4.0f, 4.3f}, PI,
                  CC_COMBAT_RAIDER);
    CcLocalCombatSetGuarded(&guard, &target, true);

    float minimum_x = guard.position.x;
    float maximum_x = guard.position.x;
    float minimum_z = guard.position.z;
    float maximum_z = guard.position.z;
    float maximum_pose_step = 0.0f;
    CcHumanoidPose previous_pose = guard.render_pose;
    for (int32_t frame = 0; frame < 240; ++frame) {
        CcLocalAgentUpdate(&guard, 1.0f / 60.0f, false);
        if (frame < 90) {
            previous_pose = guard.render_pose;
            continue;
        }
        minimum_x = fminf(minimum_x, guard.position.x);
        maximum_x = fmaxf(maximum_x, guard.position.x);
        minimum_z = fminf(minimum_z, guard.position.z);
        maximum_z = fmaxf(maximum_z, guard.position.z);
        maximum_pose_step = fmaxf(
            maximum_pose_step,
            MaximumPoseStep(&previous_pose, &guard.render_pose));
        previous_pose = guard.render_pose;
    }
    float horizontal_speed = sqrtf(
        guard.velocity.x * guard.velocity.x +
        guard.velocity.z * guard.velocity.z);
    if (maximum_x - minimum_x > 0.025f ||
        maximum_z - minimum_z > 0.025f ||
        horizontal_speed > 0.025f || maximum_pose_step > 0.035f) {
        (void)fprintf(stderr,
                      "combat guard did not settle: span %.4f,%.4f speed %.4f pose %.4f\n",
                      maximum_x - minimum_x, maximum_z - minimum_z,
                      horizontal_speed, maximum_pose_step);
        exit(1);
    }
}

static void TestCombatCrowdSpacing(void)
{
    CcSim sim;
    CcSimInit(&sim, 144U);
    CcLocalCourse course;
    CcLocalCourseInit(&course);
    CcLocalCourseRaiseAlarm(&course);
    for (int32_t i = 1; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
        course.runners[i].agent.combat.life_state = CC_LIFE_DEAD;
        course.runners[i].agent.combat.health = 0.0f;
    }
    course.raiders[1].combat.life_state = CC_LIFE_DEAD;
    course.raiders[1].combat.health = 0.0f;

    CcLocalAgentInit(&course.runners[0].agent,
                     (Vector2){45.0f, 30.0f}, false);
    CcLocalCombatSetTeam(&course.runners[0].agent, CC_COMBAT_GUARD);
    course.runners[0].agent.crowned = false;
    course.runners[0].response_stage = 4;
    course.runners[0].response_waypoint_active = false;
    course.runners[0].attack_cooldown = 0.0f;
    CcLocalAgentInit(&course.raiders[0],
                     (Vector2){45.0f, 30.0f}, false);
    CcLocalCombatSetTeam(&course.raiders[0], CC_COMBAT_RAIDER);
    course.raiders[0].crowned = false;
    course.raider_response_stage[0] = 3;
    course.raider_response_waypoint_active[0] = false;
    course.raider_attack_cooldown[0] = 0.0f;

    float maximum_distance = 0.0f;
    float first_strike_distance = -1.0f;
    for (int32_t frame = 0; frame < 240; ++frame) {
        CcLocalCourseUpdate(&course, NULL, &sim, 1.0f / 60.0f);
        CcLocalAgent *guard = &course.runners[0].agent;
        CcLocalAgent *raider = &course.raiders[0];
        float distance = VectorDistance3(guard->position, raider->position);
        maximum_distance = fmaxf(maximum_distance, distance);
        if (first_strike_distance < 0.0f &&
            (guard->humanoid.action == CC_HUMANOID_ACTION_STRIKE ||
             raider->humanoid.action == CC_HUMANOID_ACTION_STRIKE)) {
            first_strike_distance = distance;
        }
    }
    if (maximum_distance < 1.10f || first_strike_distance < 1.00f) {
        (void)fprintf(stderr,
                      "combatants attacked before restoring body spacing: max %.2f first strike %.2f\n",
                      maximum_distance, first_strike_distance);
        exit(1);
    }

    CcLocalCourse formation;
    CcLocalCourseInit(&formation);
    CcLocalCourseRaiseAlarm(&formation);
    for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
        CcLocalAgentInit(&formation.runners[i].agent,
                         (Vector2){45.0f, 30.0f}, false);
        CcLocalCombatSetTeam(&formation.runners[i].agent, CC_COMBAT_GUARD);
        formation.runners[i].response_stage = 4;
        formation.runners[i].attack_cooldown = 100.0f;
    }
    for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
        CcLocalAgentInit(&formation.raiders[i],
                         (Vector2){45.0f, 30.0f}, false);
        CcLocalCombatSetTeam(&formation.raiders[i], CC_COMBAT_RAIDER);
        formation.raider_response_stage[i] = 3;
        formation.raider_attack_cooldown[i] = 100.0f;
    }
    CcLocalAgentInit(&formation.travellers[0].agent,
                     (Vector2){45.0f, 30.0f}, false);
    formation.travellers[0].active = true;
    for (int32_t frame = 0; frame < 180; ++frame) {
        CcLocalCourseUpdate(&formation, NULL, &sim, 1.0f / 60.0f);
    }
    float closest_allies = 1000.0f;
    float closest_hostiles = 1000.0f;
    float closest_bystander = 1000.0f;
    CcLocalAgent *actors[] = {
        &formation.runners[0].agent, &formation.runners[1].agent,
        &formation.runners[2].agent, &formation.raiders[0],
        &formation.raiders[1]
    };
    for (int32_t first = 0; first < 5; ++first) {
        for (int32_t second = first + 1; second < 5; ++second) {
            float distance = VectorDistance3(actors[first]->position,
                                             actors[second]->position);
            bool hostile = actors[first]->combat.team !=
                           actors[second]->combat.team;
            if (hostile) closest_hostiles = fminf(closest_hostiles, distance);
            else closest_allies = fminf(closest_allies, distance);
        }
        closest_bystander = fminf(
            closest_bystander,
            VectorDistance3(actors[first]->position,
                            formation.travellers[0].agent.position));
    }
    if (closest_hostiles < 1.10f || closest_allies < 0.82f ||
        closest_bystander < 1.00f) {
        (void)fprintf(stderr,
                      "melee formation collapsed: hostile %.2f ally %.2f bystander %.2f\n",
                      closest_hostiles, closest_allies,
                      closest_bystander);
        exit(1);
    }
}

static void TestCapePhysics(void)
{
    static const float EXPECTED_LENGTHS[] = {0.270f, 0.275f, 0.285f, 0.285f};
    CcLocalAgent first;
    CcLocalAgent second;
    CcLocalAgentInit(&first, (Vector2){4.0f, 5.5f}, true);
    CcLocalAgentInit(&second, (Vector2){4.0f, 5.5f}, true);
    if (!first.cape.initialized || !second.cape.initialized) {
        (void)fprintf(stderr, "hero cape did not initialize with the body rig\n");
        exit(1);
    }
    Vector3 initial_tip = first.cape.point[CC_LOCAL_CAPE_POINT_COUNT - 1];
    if (!CcLocalAgentSetExactTarget(&first, (Vector3){8.0f, 0.0f, 5.5f}, true) ||
        !CcLocalAgentSetExactTarget(&second, (Vector3){8.0f, 0.0f, 5.5f}, true)) {
        (void)fprintf(stderr, "cape test walk target was rejected\n");
        exit(1);
    }
    float maximum_length_error = 0.0f;
    for (int32_t frame = 0; frame < 360; ++frame) {
        CcLocalAgentUpdate(&first, 1.0f / 60.0f, true);
        CcLocalAgentUpdate(&second, 1.0f / 60.0f, true);
        for (int32_t point = 1; point < CC_LOCAL_CAPE_POINT_COUNT; ++point) {
            float length = VectorDistance3(first.cape.point[point - 1],
                                           first.cape.point[point]);
            maximum_length_error = fmaxf(
                maximum_length_error,
                fabsf(length - EXPECTED_LENGTHS[point - 1]));
            if (!isfinite(first.cape.point[point].x) ||
                !isfinite(first.cape.point[point].y) ||
                !isfinite(first.cape.point[point].z)) {
                (void)fprintf(stderr, "cape solver emitted a non-finite point\n");
                exit(1);
            }
            if (VectorDistance3(first.cape.point[point],
                                second.cape.point[point]) > 0.000001f) {
                (void)fprintf(stderr, "cape solver was not deterministic\n");
                exit(1);
            }
        }
        if (VectorDistance3(first.cape.point[0], first.cape.anchor) >
            0.000001f) {
            (void)fprintf(stderr, "cape root detached from its back socket\n");
            exit(1);
        }
    }
    if (maximum_length_error > 0.004f) {
        (void)fprintf(stderr, "cape constraint stretched by %.5f metres\n",
                      maximum_length_error);
        exit(1);
    }
    if (VectorDistance3(initial_tip,
                        first.cape.point[CC_LOCAL_CAPE_POINT_COUNT - 1]) <
        0.12f) {
        (void)fprintf(stderr, "cape did not react to the moving body\n");
        exit(1);
    }
}

static void TestControlledJump(void)
{
    CcLocalAgent jumper;
    CcLocalAgentInit(&jumper, (Vector2){4.0f, 5.5f}, true);
    if (!CcLocalAgentJump(&jumper)) {
        (void)fprintf(stderr, "grounded biped rejected a controlled jump\n");
        exit(1);
    }
    if (CcLocalAgentJump(&jumper)) {
        (void)fprintf(stderr, "airborne biped accepted a second jump\n");
        exit(1);
    }
    if (CcLocalCombatBeginStrike(&jumper, NULL)) {
        (void)fprintf(stderr, "airborne biped interrupted jump with strike\n");
        exit(1);
    }
    float maximum_height = jumper.position.y;
    bool saw_ascent = false;
    bool saw_descent = false;
    bool saw_jump_pose = false;
    for (int32_t frame = 0; frame < 180; ++frame) {
        CcLocalAgentUpdate(&jumper, 1.0f / 60.0f, true);
        maximum_height = fmaxf(maximum_height, jumper.position.y);
        saw_ascent = saw_ascent || jumper.velocity.y > 0.20f;
        saw_descent = saw_descent || jumper.velocity.y < -0.20f;
        saw_jump_pose = saw_jump_pose ||
            jumper.humanoid.action == CC_HUMANOID_ACTION_JUMP;
        if (frame > 45 && jumper.grounded &&
            jumper.humanoid.action == CC_HUMANOID_ACTION_LOCOMOTION) break;
    }
    if (!saw_ascent || !saw_descent || !saw_jump_pose ||
        !jumper.grounded || jumper.humanoid.ragdoll.active ||
        maximum_height < 0.72f || maximum_height > 1.10f) {
        (void)fprintf(stderr,
                      "controlled jump failed: max %.2f up %d down %d pose %d grounded %d ragdoll %d\n",
                      maximum_height, saw_ascent, saw_descent, saw_jump_pose,
                      jumper.grounded, jumper.humanoid.ragdoll.active);
        exit(1);
    }
}

static void TestHeroicAthleticism(void)
{
    CcLocalAgent novice;
    CcLocalAgent hero;
    CcLocalAgentInit(&novice, (Vector2){6.0f, 4.0f}, true);
    CcLocalAgentInit(&hero, (Vector2){6.0f, 4.0f}, true);
    CcLocalAgentSetAthleticLevel(&hero, CC_ATHLETIC_MOBILITY,
                                 CC_ATHLETIC_MAX_LEVEL);
    if (!CcLocalAgentJump(&novice) || !CcLocalAgentJump(&hero) ||
        hero.velocity.y <= novice.velocity.y + 0.50f) {
        (void)fprintf(stderr,
                      "mobility training did not strengthen takeoff %.2f vs %.2f\n",
                      hero.velocity.y, novice.velocity.y);
        exit(1);
    }

    CcLocalAgent trainee;
    CcLocalAgentInit(&trainee, (Vector2){9.45f, 1.20f}, false);
    CcLocalAgentTrainAthleticism(&trainee, CC_ATHLETIC_GRIP, 55.0f);
    if (trainee.athletics.level[CC_ATHLETIC_GRIP] != 2 ||
        CcLocalAgentAthleticProgress(&trainee, CC_ATHLETIC_GRIP) <= 0.0f) {
        (void)fprintf(stderr, "grip experience did not level predictably\n");
        exit(1);
    }
    float grip_progress = CcLocalAgentAthleticProgress(
        &trainee, CC_ATHLETIC_GRIP);
    CcLocalAgentTrainAthleticism(&trainee, CC_ATHLETIC_GRIP, NAN);
    if (CcLocalAgentAthleticProgress(
            &trainee, (CcAthleticDiscipline)-1) != 0.0f ||
        CcLocalAgentAthleticProgress(NULL, CC_ATHLETIC_GRIP) != 0.0f ||
        CcLocalAgentAthleticProgress(&trainee, CC_ATHLETIC_GRIP) !=
            grip_progress) {
        (void)fprintf(stderr,
                      "athletic profile accepted an invalid public input\n");
        exit(1);
    }
    CcLocalAgentSetAthleticLevel(&trainee, CC_ATHLETIC_MOBILITY, 3);
    if (!CcLocalAgentSetExactTarget(
            &trainee, (Vector3){10.50f, 0.0f, 1.20f}, false)) {
        (void)fprintf(stderr, "heroic vault route was rejected\n");
        exit(1);
    }
    bool saw_vault = false;
    for (int32_t frame = 0; frame < 240; ++frame) {
        CcLocalAgentUpdate(&trainee, 1.0f / 60.0f, false);
        saw_vault = saw_vault || trainee.traversal == CC_TRAVERSAL_VAULT;
        if (saw_vault && !trainee.climbing) break;
    }
    if (!saw_vault) {
        (void)fprintf(stderr,
                      "trained mobility did not select the low-obstacle vault: pos %.2f %.2f %.2f traversal %d climbing %d target %d levels %d/%d\n",
                      trainee.position.x, trainee.position.y,
                      trainee.position.z, trainee.traversal,
                      trainee.climbing, trainee.exact_target_valid,
                      trainee.athletics.level[CC_ATHLETIC_MOBILITY],
                      trainee.athletics.level[CC_ATHLETIC_GRIP]);
        exit(1);
    }
}

static void TestIntentionalParkourEntry(void)
{
    CcLocalAgent knocked;
    CcLocalAgentInit(&knocked, (Vector2){3.50f, 6.35f}, false);
    knocked.combat.stagger_seconds = 0.75f;
    knocked.combat.knockback_velocity.z = 2.8f;
    for (int32_t frame = 0; frame < 60; ++frame) {
        CcLocalAgentUpdate(&knocked, 1.0f / 60.0f, false);
        if (knocked.climbing || knocked.vaulting) {
            (void)fprintf(stderr,
                          "passive knockback incorrectly triggered parkour\n");
            exit(1);
        }
    }
    if (knocked.exact_target_valid) {
        (void)fprintf(stderr,
                      "uncommanded parkour fixture acquired a destination\n");
        exit(1);
    }
}

static void TestTravellerIngress(void)
{
    CcLocalCourse opening;
    CcLocalCourseInit(&opening);
    if (opening.alarm_countdown < 20.0f) {
        (void)fprintf(stderr,
                      "opening raid did not leave enough discovery time: %.1f\n",
                      opening.alarm_countdown);
        exit(1);
    }
    for (int32_t frame = 0; frame < 600; ++frame) {
        CcLocalCourseUpdate(&opening, NULL, NULL, 1.0f / 60.0f);
    }
    if (opening.alarm_active) {
        (void)fprintf(stderr,
                      "opening raid interrupted the first ten seconds\n");
        exit(1);
    }

    CcLocalAgent plaza_player;
    CcLocalAgentInit(&plaza_player, (Vector2){45.0f, 30.0f}, false);
    CcLocalCombatSetTeam(&plaza_player, CC_COMBAT_PLAYER);
    CcLocalAgentInit(&opening.runners[0].agent,
                     (Vector2){45.0f, 30.0f}, false);
    CcLocalCombatSetTeam(&opening.runners[0].agent, CC_COMBAT_GUARD);
    opening.runners[0].pause_seconds = 100.0f;
    CcLocalAgentInit(&opening.travellers[0].agent,
                     (Vector2){45.0f, 30.0f}, false);
    opening.travellers[0].active = true;
    for (int32_t frame = 0; frame < 120; ++frame) {
        CcLocalAgentUpdate(&plaza_player, 1.0f / 60.0f, false);
        CcLocalCourseUpdate(&opening, &plaza_player, NULL, 1.0f / 60.0f);
    }
    float guard_space = VectorDistance3(
        plaza_player.position, opening.runners[0].agent.position);
    float traveller_space = VectorDistance3(
        plaza_player.position, opening.travellers[0].agent.position);
    float crowd_space = VectorDistance3(
        opening.runners[0].agent.position,
        opening.travellers[0].agent.position);
    if (guard_space < 0.82f || traveller_space < 1.0f ||
        crowd_space < 1.0f) {
        (void)fprintf(stderr,
                      "calm street crowd overlapped: guard %.2f traveller %.2f pair %.2f\n",
                      guard_space, traveller_space, crowd_space);
        exit(1);
    }

    CcLocalCourse course;
    CcLocalCourseInit(&course);
    Vector3 start[2] = {course.travellers[0].agent.position,
                        course.travellers[1].agent.position};
    for (int32_t frame = 0; frame < 180; ++frame) {
        CcLocalCourseUpdate(&course, NULL, NULL, 1.0f / 60.0f);
    }
    for (int32_t i = 0; i < 2; ++i) {
        if (!course.travellers[i].active ||
            VectorDistance3(start[i], course.travellers[i].agent.position) <
                0.55f) {
            (void)fprintf(stderr,
                          "traveller %d did not physically enter from the world edge\n",
                          i);
            exit(1);
        }
    }
    if (VectorDistance3(course.travellers[0].agent.position,
                        course.travellers[1].agent.position) < 4.0f) {
        (void)fprintf(stderr, "travellers entered in a visible bunch\n");
        exit(1);
    }
}

static void TestFaceAngleAndLodContract(void)
{
    if (CcLocalFaceViewForFrontAmountInternal(1.0f) !=
            CC_LOCAL_FACE_VIEW_FRONT ||
        CcLocalFaceViewForFrontAmountInternal(0.81f) !=
            CC_LOCAL_FACE_VIEW_THREE_QUARTER ||
        CcLocalFaceViewForFrontAmountInternal(0.27f) !=
            CC_LOCAL_FACE_VIEW_PROFILE) {
        (void)fprintf(stderr, "head-local face view thresholds changed\n");
        exit(1);
    }
    if (CcLocalFaceLodForProjectedHeightInternal(3.99f) !=
            CC_LOCAL_FACE_LOD_SILHOUETTE ||
        CcLocalFaceLodForProjectedHeightInternal(4.00f) !=
            CC_LOCAL_FACE_LOD_READABLE ||
        CcLocalFaceLodForProjectedHeightInternal(11.99f) !=
            CC_LOCAL_FACE_LOD_READABLE ||
        CcLocalFaceLodForProjectedHeightInternal(12.00f) !=
            CC_LOCAL_FACE_LOD_CLOSE) {
        (void)fprintf(stderr, "projected-size face LOD thresholds changed\n");
        exit(1);
    }
}

static void TestRoadBridgeSupport(void)
{
    if (fabsf(CcLocalRoadCheckpointSurfaceYInternal(52.00f, 40.00f) -
               0.56f) > 0.001f ||
        fabsf(CcLocalRoadCheckpointSurfaceYInternal(47.50f, 40.00f) -
               0.10f) > 0.001f ||
        fabsf(CcLocalRoadCheckpointSurfaceYInternal(46.00f, 40.00f)) >
               0.001f ||
        fabsf(CcLocalRoadHorseLateralSpacingInternal(true) - 0.78f) >
               0.001f ||
        fabsf(CcLocalRoadHorseLateralSpacingInternal(false) - 1.05f) >
               0.001f ||
        fabsf(CcLocalRoadHorseLongitudinalOffsetInternal() - 5.55f) >
               0.001f) {
        (void)fprintf(stderr,
                      "road horse team did not fit the authored bridge support\n");
        exit(1);
    }

    CcLocalAgent bridge_agent;
    CcLocalAgentInit(&bridge_agent, (Vector2){52.00f, 40.00f}, false);
    CcLocalAgentSetScene(&bridge_agent, CC_LOCAL_SCENE_ROAD);
    for (int32_t frame = 0; frame < 90; ++frame) {
        CcLocalAgentUpdate(&bridge_agent, 1.0f / 60.0f, false);
    }
    if (fabsf(bridge_agent.position.y - 0.56f) > 0.02f ||
        !bridge_agent.grounded) {
        (void)fprintf(stderr,
                      "bridge agent sank through the authored deck: y %.2f grounded %d\n",
                      bridge_agent.position.y, bridge_agent.grounded);
        exit(1);
    }

    Vector3 corrected = {0};
    Vector3 normal = {0};
    if (!CcLocalProbePhysicsSphereInternal(
            CC_LOCAL_SCENE_ROAD,
            (Vector3){52.00f, 1.20f, 40.00f},
            (Vector3){52.00f, 0.20f, 40.00f}, 0.30f,
            &corrected, &normal) ||
        fabsf(corrected.y - 0.86f) > 0.02f || normal.y < 0.99f) {
        (void)fprintf(stderr,
                      "bridge deck did not support a physical body: corrected %.2f normal %.2f\n",
                      corrected.y, normal.y);
        exit(1);
    }
}

static void TestBuildingCutawaySelection(void)
{
    Camera3D camera = {
        .position = {0.0f, 3.0f, 10.0f},
        .target = {0.0f, 1.0f, 0.0f},
        .up = {0.0f, 1.0f, 0.0f},
        .fovy = 45.0f,
        .projection = CAMERA_PERSPECTIVE,
    };
    Vector3 hero = {0.0f, 1.05f, 0.0f};
    Rectangle direct_blocker = {-1.2f, 3.0f, 2.4f, 2.0f};
    Rectangle shoulder_blocker = {0.18f, 3.0f, 0.72f, 2.0f};
    Rectangle unrelated_house = {4.0f, 3.0f, 2.0f, 2.0f};
    Rectangle house_behind_hero = {-1.2f, -4.0f, 2.4f, 2.0f};
    bool direct = CcLocalBuildingObscuresHeroInternal(
        direct_blocker, 4.0f, camera, hero, 457, 285);
    bool shoulder = CcLocalBuildingObscuresHeroInternal(
        shoulder_blocker, 4.0f, camera, hero, 457, 285);
    bool unrelated = CcLocalBuildingObscuresHeroInternal(
        unrelated_house, 4.0f, camera, hero, 457, 285);
    bool behind = CcLocalBuildingObscuresHeroInternal(
        house_behind_hero, 4.0f, camera, hero, 457, 285);

    if (!direct || !shoulder || unrelated || behind) {
        (void)fprintf(stderr,
                      "building cutaway selection failed: direct %d shoulder %d unrelated %d behind %d\n",
                      direct, shoulder, unrelated, behind);
        exit(1);
    }
}

int main(void)
{
    for (int32_t frame = 0; frame < 239; ++frame) {
        CcLocalRendererBeginFrame(0.010f);
    }
    CcLocalRendererBeginFrame(0.050f);
    CcLocalRendererRecordSkinUpdate(1);
    CcLocalRendererRecordHeroSkinUpdate(3);
    CcLocalRendererStats performance = CcLocalRendererGetStats();
    if (performance.p95_frame_milliseconds < 9.9f ||
        performance.p95_frame_milliseconds > 10.1f ||
        performance.maximum_frame_milliseconds < 49.9f ||
        performance.hitch_count != 1 ||
        performance.skin_updates != 2 || performance.skinned_meshes != 4 ||
        performance.hero_skin_updates != 1 ||
        performance.hero_skinned_meshes != 3) {
        (void)fprintf(stderr,
                      "renderer hitch or hero skin metrics were incorrect\n");
        return 1;
    }

    TestBuildingCutawaySelection();
    if (fabsf(CcLocalRoadCarriageX(0) - 20.15f) > 0.001f ||
        fabsf(CcLocalRoadCarriageX(350) - 38.35f) > 0.001f ||
        fabsf(CcLocalRoadCarriageX(1000) - 72.15f) > 0.001f) {
        (void)fprintf(stderr,
                      "road carriage did not preserve its encounter position\n");
        return 1;
    }
    Vector2 first_branch = CcLocalForkBranchEndInternal(0, 4);
    Vector2 second_branch = CcLocalForkBranchEndInternal(1, 4);
    Vector2 third_branch = CcLocalForkBranchEndInternal(2, 4);
    Vector2 fourth_branch = CcLocalForkBranchEndInternal(3, 4);
    if (first_branch.x <= 50.0f || second_branch.x <= 50.0f ||
        third_branch.x <= 50.0f || fourth_branch.x <= 50.0f ||
        !(first_branch.y < second_branch.y &&
          second_branch.y < third_branch.y &&
          third_branch.y < fourth_branch.y)) {
        (void)fprintf(stderr,
                      "road fork did not expose four distinct physical branches\n");
        return 1;
    }
    uint32_t wilderness_seed = CcLocalRoadWildernessSeedInternal(
        UINT32_C(0x1234abcd), UINT32_C(42), 3);
    if (wilderness_seed != CcLocalRoadWildernessSeedInternal(
            UINT32_C(0x1234abcd), UINT32_C(42), 3) ||
        wilderness_seed == CcLocalRoadWildernessSeedInternal(
            UINT32_C(0x1234abcd), UINT32_C(43), 3) ||
        wilderness_seed == CcLocalRoadWildernessSeedInternal(
            UINT32_C(0x1234abcd), UINT32_C(42), 4)) {
        (void)fprintf(stderr,
                      "procedural road wilderness seed was not stable and distinct\n");
        return 1;
    }
    TestRoadBridgeSupport();
    TestFaceAngleAndLodContract();
    TestPlaceLandmarkCollision();
    TestTownPlanCollisionAndGate();
    TestSharedCharacterCollisionWorld();
    TestRagdollStepsInWater();
    RenderTexture2D click_target = {0};
    click_target.texture.width = 457;
    click_target.texture.height = 285;
    Rectangle click_viewport = {0.0f, 0.0f, 914.0f, 570.0f};
    static const Vector3 crown_gate_road_targets[] = {
        {58.0f, 0.0f, 27.30f},
        {58.4f, 0.0f, 27.80f},
        {58.8f, 0.0f, 27.20f},
    };
    static const Vector2 camera_review_points[] = {
        {10.5f, 7.5f}, {11.0f, 28.5f}, {14.0f, 52.0f},
        {33.0f, 25.0f}, {44.0f, 29.0f}, {42.0f, 52.0f},
        {50.0f, 27.25f}, {60.0f, 27.5f}, {64.0f, 27.5f},
        {68.0f, 28.0f}, {72.0f, 30.0f}, {63.8f, 34.0f},
        {70.0f, 34.0f}, {78.5f, 34.0f}, {78.5f, 29.0f},
        {58.0f, 50.0f}, {78.0f, 50.0f}, {50.0f, 27.25f},
    };

    /* Every authored room and the complete Market Steps-to-Crown Gate road
       must retain the hero while the target, yaw, and lens interpolate. */
    CcLocalAgent framing_agent;
    CcLocalAgentInit(&framing_agent, camera_review_points[0], false);
    float camera_clock = 0.0f;
    for (int32_t point = 0;
         point < (int32_t)(sizeof(camera_review_points) /
                           sizeof(camera_review_points[0])); ++point) {
        framing_agent.position.x = camera_review_points[point].x;
        framing_agent.position.z = camera_review_points[point].y;
        framing_agent.position.y = CcLocalTerrainHeightAt(
            framing_agent.position.x, framing_agent.position.z);
        for (int32_t frame = 0; frame < 75; ++frame) {
            camera_clock += 1.0f / 60.0f;
            Camera3D review_camera = CcLocalStreetCameraInternal(
                &framing_agent, camera_clock, true,
                click_target.texture.height);
            Vector2 hero_screen = GetWorldToScreenEx(
                (Vector3){framing_agent.position.x,
                          framing_agent.position.y + 1.05f,
                          framing_agent.position.z},
                review_camera, click_target.texture.width,
                click_target.texture.height);
            if (hero_screen.x < 88.0f || hero_screen.x > 369.0f ||
                hero_screen.y < 54.0f || hero_screen.y > 231.0f) {
                (void)fprintf(
                    stderr,
                    "camera review point %d frame %d lost hero at %.2f %.2f\n",
                    point, frame, hero_screen.x, hero_screen.y);
                return 1;
            }
        }
    }

    /* Close facades remain part of the authored town-heart composition. They
       must not create a hidden fourth camera scene or lose the hero. */
    CcLocalAgent alley_camera_agent;
    CcLocalAgentInit(&alley_camera_agent, (Vector2){32.0f, 38.0f}, false);
    Camera3D alley_camera = {0};
    for (int32_t frame = 0; frame < 120; ++frame) {
        camera_clock += 1.0f / 60.0f;
        alley_camera = CcLocalStreetCameraInternal(
            &alley_camera_agent, camera_clock, true,
            click_target.texture.height);
    }
    Vector2 alley_hero_screen = GetWorldToScreenEx(
        (Vector3){alley_camera_agent.position.x,
                  alley_camera_agent.position.y + 1.05f,
                  alley_camera_agent.position.z},
        alley_camera, click_target.texture.width,
        click_target.texture.height);
    if (alley_camera.projection != CAMERA_ORTHOGRAPHIC ||
        alley_camera.fovy < 26.0f || alley_camera.fovy > 35.0f ||
        alley_hero_screen.x < 88.0f || alley_hero_screen.x > 369.0f ||
        alley_hero_screen.y < 54.0f || alley_hero_screen.y > 231.0f) {
        (void)fprintf(stderr,
                      "authored town-heart camera was invalid: fovy %.2f screen %.2f %.2f\n",
                      alley_camera.fovy, alley_hero_screen.x,
                      alley_hero_screen.y);
        return 1;
    }

    /* Once a shot has settled, ordinary movement inside its safe area must
       move the actor across the stage instead of dragging the camera along.
       This is the core King's Quest-style framing contract. */
    Vector3 held_alley_target = alley_camera.target;
    float held_alley_fovy = alley_camera.fovy;
    alley_camera_agent.position.x += 0.55f;
    for (int32_t frame = 0; frame < 90; ++frame) {
        camera_clock += 1.0f / 60.0f;
        alley_camera = CcLocalStreetCameraInternal(
            &alley_camera_agent, camera_clock, true,
            click_target.texture.height);
    }
    Vector2 moved_alley_hero_screen = GetWorldToScreenEx(
        (Vector3){alley_camera_agent.position.x,
                  alley_camera_agent.position.y + 1.05f,
                  alley_camera_agent.position.z},
        alley_camera, click_target.texture.width,
        click_target.texture.height);
    if (VectorDistance3(held_alley_target, alley_camera.target) > 0.03f ||
        fabsf(held_alley_fovy - alley_camera.fovy) > 0.01f ||
        fabsf(moved_alley_hero_screen.x - alley_hero_screen.x) < 2.0f) {
        (void)fprintf(
            stderr,
            "settled street shot followed hero: target drift %.3f fovy drift %.3f actor shift %.2f\n",
            VectorDistance3(held_alley_target, alley_camera.target),
            fabsf(held_alley_fovy - alley_camera.fovy),
            fabsf(moved_alley_hero_screen.x - alley_hero_screen.x));
        return 1;
    }

    /* Miller's Row is long enough to expose follow-camera creep. Walk the
       whole road at gameplay speed: the hero must stay in the safe frame,
       and the camera may only make a few authored page changes rather than
       moving continuously beside the actor. */
    CcLocalAgent miller_camera_agent;
    CcLocalAgentInit(&miller_camera_agent, (Vector2){54.6f, 51.4f}, false);
    Camera3D miller_camera = {0};
    for (int32_t frame = 0; frame < 120; ++frame) {
        camera_clock += 1.0f / 60.0f;
        miller_camera = CcLocalStreetCameraInternal(
            &miller_camera_agent, camera_clock, true,
            click_target.texture.height);
    }
    Vector3 previous_camera_target = miller_camera.target;
    bool camera_was_moving = false;
    int32_t camera_motion_runs = 0;
    int32_t camera_moving_frames = 0;
    int32_t current_motion_frames = 0;
    int32_t longest_motion_run = 0;
    for (int32_t frame = 0; frame < 720; ++frame) {
        float amount = (float)frame / 719.0f;
        miller_camera_agent.position.x = 54.6f + 17.0f * amount;
        miller_camera_agent.position.z = 51.4f;
        miller_camera_agent.position.y = CcLocalTerrainHeightAt(
            miller_camera_agent.position.x,
            miller_camera_agent.position.z);
        camera_clock += 1.0f / 60.0f;
        miller_camera = CcLocalStreetCameraInternal(
            &miller_camera_agent, camera_clock, true,
            click_target.texture.height);
        Vector2 hero_screen = GetWorldToScreenEx(
            (Vector3){miller_camera_agent.position.x,
                      miller_camera_agent.position.y + 1.05f,
                      miller_camera_agent.position.z},
            miller_camera, click_target.texture.width,
            click_target.texture.height);
        if (hero_screen.x < 88.0f || hero_screen.x > 369.0f ||
            hero_screen.y < 54.0f || hero_screen.y > 231.0f) {
            (void)fprintf(stderr,
                          "Miller's Row camera lost hero at frame %d: %.2f %.2f\n",
                          frame, hero_screen.x, hero_screen.y);
            return 1;
        }
        bool camera_moving =
            VectorDistance3(previous_camera_target, miller_camera.target) >
                0.002f;
        if (camera_moving && !camera_was_moving) camera_motion_runs += 1;
        if (camera_moving) camera_moving_frames += 1;
        current_motion_frames = camera_moving ? current_motion_frames + 1 : 0;
        if (current_motion_frames > longest_motion_run) {
            longest_motion_run = current_motion_frames;
        }
        camera_was_moving = camera_moving;
        previous_camera_target = miller_camera.target;
    }
    if (longest_motion_run > 75 || camera_moving_frames > 240) {
        (void)fprintf(stderr,
                      "Miller's Row camera followed continuously: %d runs, %d moving frames, longest %d frames\n",
                      camera_motion_runs, camera_moving_frames,
                      longest_motion_run);
        return 1;
    }

    /* MMO-style ground commands project an obstructed click to the nearest
       reachable edge. The windmill footprint on Miller's Row is a stable
       regression target for clicks that used to be rejected outright. */
    CcLocalAgent miller_click_agent;
    CcLocalAgentInit(&miller_click_agent, (Vector2){58.0f, 51.5f}, false);
    Camera3D miller_click_camera = CcLocalStreetCameraInternal(
        &miller_click_agent, camera_clock, false,
        click_target.texture.height);
    Vector3 blocked_windmill_point = {
        64.14f,
        CcLocalTerrainHeightAt(64.14f, 51.02f),
        51.02f,
    };
    Vector2 blocked_click_art = GetWorldToScreenEx(
        blocked_windmill_point, miller_click_camera,
        click_target.texture.width, click_target.texture.height);
    Vector2 blocked_click_screen = {
        blocked_click_art.x * click_viewport.width /
            (float)click_target.texture.width,
        blocked_click_art.y * click_viewport.height /
            (float)click_target.texture.height,
    };
    if (!CcLocalAgentPickTarget(&miller_click_agent,
                                blocked_click_screen,
                                click_target, click_viewport, false)) {
        (void)fprintf(stderr,
                      "Miller's Row blocked click was not projected\n");
        return 1;
    }
    Vector3 projected_command = miller_click_agent.command_point;
    bool command_inside_windmill =
        projected_command.x > 63.18f - miller_click_agent.radius &&
        projected_command.x < 65.10f + miller_click_agent.radius &&
        projected_command.z > 50.06f - miller_click_agent.radius &&
        projected_command.z < 51.98f + miller_click_agent.radius;
    float projection_distance = VectorDistance2(
        (Vector2){projected_command.x, projected_command.z},
        (Vector2){blocked_windmill_point.x, blocked_windmill_point.z});
    if (command_inside_windmill || projection_distance > 2.75f) {
        (void)fprintf(stderr,
                      "Miller's Row click projection was unsafe: %.2f %.2f distance %.2f\n",
                      projected_command.x, projected_command.z,
                      projection_distance);
        return 1;
    }
    for (int32_t frame = 0;
         frame < 2400 && miller_click_agent.navigation_active; ++frame) {
        CcLocalAgentUpdate(&miller_click_agent, 1.0f / 60.0f, false);
    }
    if (miller_click_agent.navigation_active ||
        VectorDistance2(
            (Vector2){miller_click_agent.position.x,
                      miller_click_agent.position.z},
            (Vector2){projected_command.x, projected_command.z}) > 0.38f) {
        (void)fprintf(stderr,
                      "Miller's Row projected path did not finish: %.2f %.2f toward %.2f %.2f\n",
                      miller_click_agent.position.x,
                      miller_click_agent.position.z,
                      projected_command.x, projected_command.z);
        return 1;
    }

    /* A nearby duel changes only the presentation: blend from the fixed
       room into a perspective lock-on view behind one shoulder. Keep the
       hero as the larger foreground anchor, retain both fighters, then
       return to the exact fixed-camera projection after combat. */
    CcLocalAgent shoulder_player;
    CcLocalAgentInit(&shoulder_player, (Vector2){15.40f, 9.65f}, false);
    CcLocalCourse shoulder_course;
    CcLocalCourseInit(&shoulder_course);
    shoulder_course.scene = CC_LOCAL_SCENE_STREET;
    shoulder_course.alarm_active = true;
    shoulder_course.raiders_retreating = false;
    shoulder_course.raiders[0].position = (Vector3){
        18.20f, CcLocalTerrainHeightAt(18.20f, 9.65f), 9.65f};
    Camera3D shoulder_base = {0};
    for (int32_t frame = 0; frame < 120; ++frame) {
        camera_clock += 1.0f / 60.0f;
        shoulder_base = CcLocalStreetCameraInternal(
            &shoulder_player, camera_clock, true,
            click_target.texture.height);
    }
    Camera3D shoulder_camera = shoulder_base;
    for (int32_t frame = 0; frame < 90; ++frame) {
        camera_clock += 1.0f / 60.0f;
        shoulder_camera = CcLocalCombatCameraInternal(
            shoulder_base, &shoulder_player, &shoulder_course,
            camera_clock, true, click_target.texture.height);
    }
    if (shoulder_camera.projection != CAMERA_ORTHOGRAPHIC) {
        (void)fprintf(
            stderr,
            "combat camera chose an unselected raider: projection %d\n",
            shoulder_camera.projection);
        return 1;
    }

    /* The portrait-sized physical box can become a thin target in a wide
       room shot. A click just outside that box still selects the visible
       body through the small screen-space target halo. */
    Vector2 raider_center_art = GetWorldToScreenEx(
        (Vector3){shoulder_course.raiders[0].position.x,
                  shoulder_course.raiders[0].position.y + 1.10f,
                  shoulder_course.raiders[0].position.z},
        shoulder_base, click_target.texture.width,
        click_target.texture.height);
    Vector2 raider_near_screen = {
        (raider_center_art.x + 17.0f) * click_viewport.width /
            (float)click_target.texture.width,
        raider_center_art.y * click_viewport.height /
            (float)click_target.texture.height,
    };
    int32_t picked_raider = CcLocalCoursePickPlayerTarget(
        &shoulder_course, &shoulder_player, raider_near_screen,
        click_target, click_viewport);
    if (picked_raider != 0 || shoulder_player.combat.target_index != 0) {
        (void)fprintf(stderr,
                      "near-body combat click missed raider: %d\n",
                      picked_raider);
        return 1;
    }
    for (int32_t frame = 0; frame < 180; ++frame) {
        camera_clock += 1.0f / 60.0f;
        shoulder_camera = CcLocalCombatCameraInternal(
            shoulder_base, &shoulder_player, &shoulder_course,
            camera_clock, true, click_target.texture.height);
    }
    Vector3 shoulder_fight = {
        shoulder_course.raiders[0].position.x - shoulder_player.position.x,
        0.0f,
        shoulder_course.raiders[0].position.z - shoulder_player.position.z,
    };
    float shoulder_fight_length = sqrtf(
        shoulder_fight.x * shoulder_fight.x +
        shoulder_fight.z * shoulder_fight.z);
    shoulder_fight.x /= shoulder_fight_length;
    shoulder_fight.z /= shoulder_fight_length;
    Vector3 shoulder_from_player = {
        shoulder_camera.position.x - shoulder_player.position.x,
        shoulder_camera.position.y - shoulder_player.position.y,
        shoulder_camera.position.z - shoulder_player.position.z,
    };
    float behind_amount = shoulder_from_player.x * shoulder_fight.x +
                          shoulder_from_player.z * shoulder_fight.z;
    float side_amount = shoulder_from_player.x * -shoulder_fight.z +
                        shoulder_from_player.z * shoulder_fight.x;
    Vector3 shoulder_player_center = {
        shoulder_player.position.x, shoulder_player.position.y + 1.02f,
        shoulder_player.position.z};
    Vector3 shoulder_raider_center = {
        shoulder_course.raiders[0].position.x,
        shoulder_course.raiders[0].position.y + 1.02f,
        shoulder_course.raiders[0].position.z};
    Vector2 shoulder_player_screen = GetWorldToScreenEx(
        shoulder_player_center, shoulder_camera,
        click_target.texture.width, click_target.texture.height);
    Vector2 shoulder_raider_screen = GetWorldToScreenEx(
        shoulder_raider_center, shoulder_camera,
        click_target.texture.width, click_target.texture.height);
    Vector2 shoulder_player_head = GetWorldToScreenEx(
        (Vector3){shoulder_player.position.x,
                  shoulder_player.position.y + 1.80f,
                  shoulder_player.position.z},
        shoulder_camera, click_target.texture.width,
        click_target.texture.height);
    Vector2 shoulder_player_foot = GetWorldToScreenEx(
        shoulder_player.position, shoulder_camera,
        click_target.texture.width, click_target.texture.height);
    Vector2 shoulder_raider_head = GetWorldToScreenEx(
        (Vector3){shoulder_course.raiders[0].position.x,
                  shoulder_course.raiders[0].position.y + 1.80f,
                  shoulder_course.raiders[0].position.z},
        shoulder_camera, click_target.texture.width,
        click_target.texture.height);
    Vector2 shoulder_raider_foot = GetWorldToScreenEx(
        shoulder_course.raiders[0].position, shoulder_camera,
        click_target.texture.width, click_target.texture.height);
    float shoulder_player_height = fabsf(
        shoulder_player_foot.y - shoulder_player_head.y);
    float shoulder_raider_height = fabsf(
        shoulder_raider_foot.y - shoulder_raider_head.y);
    bool shoulder_subjects_safe =
        shoulder_player_screen.x > 22.0f &&
        shoulder_player_screen.x < 435.0f &&
        shoulder_player_screen.y > 14.0f &&
        shoulder_player_screen.y < 271.0f &&
        shoulder_raider_screen.x > 22.0f &&
        shoulder_raider_screen.x < 435.0f &&
        shoulder_raider_screen.y > 14.0f &&
        shoulder_raider_screen.y < 271.0f;
    if (shoulder_camera.projection != CAMERA_PERSPECTIVE ||
        behind_amount > -3.50f || fabsf(side_amount) < 2.80f ||
        !shoulder_subjects_safe ||
        shoulder_player_height < shoulder_raider_height * 1.10f) {
        (void)fprintf(
            stderr,
            "combat shoulder framing failed: projection %d behind %.2f side %.2f hero %.2f %.2f/%.2f raider %.2f %.2f/%.2f fovy %.2f\n",
            shoulder_camera.projection, behind_amount, side_amount,
            shoulder_player_screen.x, shoulder_player_screen.y,
            shoulder_player_height, shoulder_raider_screen.x,
            shoulder_raider_screen.y, shoulder_raider_height,
            shoulder_camera.fovy);
        return 1;
    }
    shoulder_course.alarm_active = false;
    for (int32_t frame = 0; frame < 240; ++frame) {
        camera_clock += 1.0f / 60.0f;
        shoulder_camera = CcLocalCombatCameraInternal(
            shoulder_base, &shoulder_player, &shoulder_course,
            camera_clock, true, click_target.texture.height);
    }
    if (shoulder_camera.projection != shoulder_base.projection ||
        VectorDistance3(shoulder_camera.position,
                        shoulder_base.position) > 0.001f ||
        VectorDistance3(shoulder_camera.target,
                        shoulder_base.target) > 0.001f ||
        fabsf(shoulder_camera.fovy - shoulder_base.fovy) > 0.001f) {
        (void)fprintf(stderr,
                      "combat camera did not return to fixed shot\n");
        return 1;
    }

    /* The Wayfarer Yard tree used to sit across both fighters in the combat
       reel. The visibility pass must find a nearby angle that clears both
       bodies without moving or hiding the tree. */
    Vector3 sightline_target = {16.74f, 0.98f, 9.65f};
    Camera3D blocked_combat_camera = {
        .position = {18.91f, 7.10f, 21.84f},
        .target = sightline_target,
        .up = {0.0f, 1.0f, 0.0f},
        .fovy = 8.95f,
        .projection = CAMERA_ORTHOGRAPHIC,
    };
    Vector3 sightline_player = {15.40f, 1.05f, 9.65f};
    Vector3 sightline_raider = {18.20f, 1.05f, 9.65f};
    float blocked_tree_score = CcLocalCameraTreeOcclusionScoreInternal(
        blocked_combat_camera, sightline_player, sightline_raider);
    float clear_angle = 0.0f;
    Camera3D clear_combat_camera = CcLocalCameraClearSightlinesInternal(
        blocked_combat_camera, sightline_player, sightline_raider, 0.0f,
        &clear_angle);
    float clear_tree_score = CcLocalCameraTreeOcclusionScoreInternal(
        clear_combat_camera, sightline_player, sightline_raider);
    if (blocked_tree_score < 0.50f || clear_tree_score > 0.08f ||
        fabsf(clear_angle) < 0.10f) {
        (void)fprintf(stderr,
                      "tree sightline camera failed: blocked %.3f clear %.3f angle %.2f\n",
                      blocked_tree_score, clear_tree_score,
                      clear_angle * RAD2DEG);
        return 1;
    }

    /* Regression from an off-center road-edge position that reproduced the
       reported failure. Proximity starts the traversal here; the first
       authored portal waypoint must route around the gatehouse. */
    CcLocalAgent edge_walker;
    CcLocalAgentInit(&edge_walker, (Vector2){57.0f, 27.0f}, false);
    /* Each camera fixture owns a settled opening shot. This also keeps the
       test clock monotonic after the combat-camera transition above. */
    for (int32_t frame = 0; frame < 120; ++frame) {
        camera_clock += 1.0f / 60.0f;
        (void)CcLocalStreetCameraInternal(
            &edge_walker, camera_clock, true, 285);
    }
    if (!CcLocalAgentSetExactTarget(
            &edge_walker, (Vector3){58.2f, 0.0f, 28.0f}, false)) {
        (void)fprintf(stderr, "road-edge regression target was rejected\n");
        return 1;
    }
    CcLocalAgentUpdate(&edge_walker, 1.0f / 60.0f, false);
    const char *edge_destination = CcLocalAgentNavigationName(&edge_walker);
    if (edge_destination == NULL ||
        strcmp(edge_destination, "CROWN GATE") != 0) {
        (void)fprintf(stderr,
                      "road-edge proximity did not start Crown Gate traversal\n");
        return 1;
    }
    for (int32_t frame = 0;
         frame < 4800 && edge_walker.navigation_active; ++frame) {
        camera_clock += 1.0f / 60.0f;
        Camera3D travel_camera = CcLocalStreetCameraInternal(
            &edge_walker, camera_clock, true, 285);
        Vector2 hero_screen = GetWorldToScreenEx(
            (Vector3){edge_walker.position.x,
                      edge_walker.position.y + 1.0f,
                      edge_walker.position.z},
            travel_camera, 457, 285);
        if (hero_screen.x < 88.0f || hero_screen.x > 369.0f ||
            hero_screen.y < 54.0f || hero_screen.y > 231.0f) {
            (void)fprintf(stderr,
                          "road traversal camera lost hero at screen %.2f %.2f\n",
                          hero_screen.x, hero_screen.y);
            return 1;
        }
        CcLocalAgentUpdate(&edge_walker, 1.0f / 60.0f, false);
    }
    if (edge_walker.navigation_active ||
        fabsf(edge_walker.position.x - 78.5f) > 0.40f ||
        fabsf(edge_walker.position.z - 29.0f) > 0.40f) {
        (void)fprintf(stderr,
                      "road-edge traversal stopped at %.2f %.2f y %.2f waypoint %d/%d target %.2f %.2f %.2f exact %d traversal %d\n",
                      edge_walker.position.x, edge_walker.position.z,
                      edge_walker.position.y,
                      edge_walker.navigation_point_index,
                      edge_walker.navigation_point_count,
                      edge_walker.target_point.x,
                      edge_walker.target_point.y,
                      edge_walker.target_point.z,
                      edge_walker.exact_target_valid,
                      edge_walker.traversal);
        return 1;
    }

    /* Proximity must not hijack a click that turns back into the room. The
       old edge-only trigger sent this westward command east to Crown Gate. */
    CcLocalAgent edge_turnaround;
    CcLocalAgentInit(&edge_turnaround, (Vector2){57.0f, 27.0f}, false);
    if (!CcLocalAgentSetExactTarget(
            &edge_turnaround, (Vector3){52.0f, 0.0f, 27.0f}, false)) {
        (void)fprintf(stderr, "edge turnaround target was rejected\n");
        return 1;
    }
    for (int32_t frame = 0; frame < 30; ++frame) {
        CcLocalAgentUpdate(&edge_turnaround, 1.0f / 60.0f, false);
        if (CcLocalAgentNavigationName(&edge_turnaround) != NULL) {
            (void)fprintf(stderr,
                          "edge turnaround was hijacked by portal traversal\n");
            return 1;
        }
    }
    if (edge_turnaround.position.x >= 56.8f) {
        (void)fprintf(stderr,
                      "edge turnaround did not move back into the room: %.2f %.2f\n",
                      edge_turnaround.position.x,
                      edge_turnaround.position.z);
        return 1;
    }

    for (int32_t click = 0;
         click < (int32_t)(sizeof(crown_gate_road_targets) /
                           sizeof(crown_gate_road_targets[0])); ++click) {
        CcLocalAgent crown_gate_walker;
        CcLocalAgentInit(&crown_gate_walker,
                         (Vector2){50.0f, 27.25f}, false);
        Camera3D intended_camera = CcLocalStreetCameraInternal(
            &crown_gate_walker, 0.0f, false,
            click_target.texture.height);
        Vector3 intended_world = crown_gate_road_targets[click];
        intended_world.y = CcLocalTerrainHeightAt(intended_world.x,
                                                   intended_world.z);
        Vector2 intended_art = GetWorldToScreenEx(
            intended_world, intended_camera,
            click_target.texture.width, click_target.texture.height);
        Vector2 road_click = {
            intended_art.x * click_viewport.width /
                (float)click_target.texture.width,
            intended_art.y * click_viewport.height /
                (float)click_target.texture.height,
        };
        /* These are ordinary ground clicks across the right-hand road mouth,
           well below and inward from the label. Foreground architecture can
           occlude the ray, but no label may activate navigation directly. */
        if (!CcLocalAgentPickTarget(&crown_gate_walker,
                                    road_click,
                                    click_target, click_viewport, false) ||
            CcLocalAgentNavigationName(&crown_gate_walker) != NULL) {
            Camera3D rejected_camera = CcLocalStreetCameraInternal(
                &crown_gate_walker, 0.0f, false,
                click_target.texture.height);
            Vector2 rejected_local = {
                road_click.x *
                    (float)click_target.texture.width /
                    click_viewport.width,
                road_click.y *
                    (float)click_target.texture.height /
                    click_viewport.height,
            };
            Ray rejected_ray = GetScreenToWorldRayEx(
                rejected_local, rejected_camera,
                click_target.texture.width, click_target.texture.height);
            float plane_distance = -rejected_ray.position.y /
                                   rejected_ray.direction.y;
            Vector3 rejected_ground = {
                rejected_ray.position.x +
                    rejected_ray.direction.x * plane_distance,
                0.0f,
                rejected_ray.position.z +
                    rejected_ray.direction.z * plane_distance,
            };
            (void)fprintf(stderr,
                          "Crown Gate road click %d was not accepted; ground %.2f %.2f\n",
                          click, rejected_ground.x, rejected_ground.z);
            return 1;
        }
        Camera3D click_camera = CcLocalStreetCameraInternal(
            &crown_gate_walker, 0.0f, false,
            click_target.texture.height);
        Vector2 command_art = GetWorldToScreenEx(
            crown_gate_walker.command_point, click_camera,
            click_target.texture.width, click_target.texture.height);
        Vector2 command_screen = {
            command_art.x * click_viewport.width /
                (float)click_target.texture.width,
            command_art.y * click_viewport.height /
                (float)click_target.texture.height,
        };
        float command_error_x = command_screen.x - road_click.x;
        float command_error_y = command_screen.y - road_click.y;
        if (!crown_gate_walker.command_point_valid ||
            command_error_x * command_error_x +
                    command_error_y * command_error_y >
                3.0f * 3.0f) {
            (void)fprintf(
                stderr,
                "Crown Gate road click %d was redirected by %.2f pixels\n",
                click,
                sqrtf(command_error_x * command_error_x +
                      command_error_y * command_error_y));
            return 1;
        }
        bool proximity_started = false;
        Vector3 last_target = crown_gate_walker.target_point;
        int32_t last_waypoint = -1;
        int32_t last_waypoint_count = 0;
        for (int32_t frame = 0; frame < 3600; ++frame) {
            if (crown_gate_walker.navigation_active) {
                last_target = crown_gate_walker.target_point;
                last_waypoint = crown_gate_walker.navigation_point_index;
                last_waypoint_count =
                    crown_gate_walker.navigation_point_count;
            }
            CcLocalAgentUpdate(&crown_gate_walker, 1.0f / 60.0f, false);
            if (CcLocalAgentNavigationName(&crown_gate_walker) != NULL) {
                proximity_started = true;
            }
            if (proximity_started && !crown_gate_walker.navigation_active) {
                break;
            }
        }
        if (!proximity_started || crown_gate_walker.navigation_active ||
            fabsf(crown_gate_walker.position.x - 78.5f) > 0.40f ||
            fabsf(crown_gate_walker.position.z - 29.0f) > 0.40f) {
            (void)fprintf(stderr,
                          "Crown Gate road click %d stopped at %.2f %.2f targeting %.2f %.2f waypoint %d/%d\n",
                          click, crown_gate_walker.position.x,
                          crown_gate_walker.position.z, last_target.x,
                          last_target.z, last_waypoint,
                          last_waypoint_count);
            return 1;
        }
    }

    /* A rejected replacement click must cancel the old command. Otherwise
       the hero keeps walking toward a stale marker after the new click gives
       no movement feedback—the exact failure reported from the market road. */
    CcLocalAgent rejected_click_walker;
    CcLocalAgentInit(&rejected_click_walker,
                     (Vector2){50.0f, 27.25f}, false);
    if (!CcLocalAgentSetExactTarget(
            &rejected_click_walker,
            (Vector3){53.0f, 0.0f, 27.25f}, false) ||
        CcLocalAgentSetExactTarget(
            &rejected_click_walker,
            (Vector3){66.5f, 0.0f, 20.0f}, false) ||
        rejected_click_walker.exact_target_valid ||
        rejected_click_walker.navigation_active ||
        rejected_click_walker.command_point_valid) {
        (void)fprintf(stderr,
                      "rejected replacement target left stale movement active\n");
        return 1;
    }

    CcLocalAgent room_traveller;
    CcLocalAgentInit(&room_traveller, (Vector2){44.0f, 29.0f}, false);
    int32_t market_portal = StreetPortalIndex(&room_traveller,
                                               "MARKET STEPS");
    if (market_portal < 0 ||
        !CcLocalAgentFollowStreetPortal(&room_traveller, market_portal) ||
        CcLocalAgentNavigationName(&room_traveller) == NULL) {
        (void)fprintf(stderr,
                      "camera-room traversal portal was not exposed\n");
        return 1;
    }
    for (int32_t frame = 0;
         frame < 2400 && room_traveller.navigation_active; ++frame) {
        CcLocalAgentUpdate(&room_traveller, 1.0f / 60.0f, false);
    }
    if (room_traveller.navigation_active ||
        fabsf(room_traveller.position.x - 50.0f) > 0.35f ||
        fabsf(room_traveller.position.z - 27.25f) > 0.35f) {
        (void)fprintf(stderr,
                      "room traversal did not reach the adjoining camera: %.2f %.2f\n",
                      room_traveller.position.x,
                      room_traveller.position.z);
        return 1;
    }

    /* A new ground command must be able to cancel a named traversal without
       proximity immediately restarting the route from the same edge. */
    CcLocalAgent cancelled_traversal;
    CcLocalAgentInit(&cancelled_traversal,
                     (Vector2){50.0f, 27.25f}, false);
    int32_t cancelled_portal = StreetPortalIndex(
        &cancelled_traversal, "CROWN GATE");
    if (cancelled_portal < 0 ||
        !CcLocalAgentFollowStreetPortal(
            &cancelled_traversal, cancelled_portal)) {
        (void)fprintf(stderr,
                      "cancellable traversal could not start\n");
        return 1;
    }
    for (int32_t frame = 0; frame < 90; ++frame) {
        CcLocalAgentUpdate(&cancelled_traversal, 1.0f / 60.0f, false);
    }
    Vector3 cancellation_point = cancelled_traversal.position;
    if (!cancelled_traversal.navigation_active ||
        !CcLocalAgentSetExactTarget(
            &cancelled_traversal, cancellation_point, false)) {
        (void)fprintf(stderr,
                      "new ground command did not cancel traversal\n");
        return 1;
    }
    for (int32_t frame = 0; frame < 90; ++frame) {
        CcLocalAgentUpdate(&cancelled_traversal, 1.0f / 60.0f, false);
        if (CcLocalAgentNavigationName(&cancelled_traversal) != NULL) {
            (void)fprintf(stderr,
                          "cancelled traversal restarted at the edge\n");
            return 1;
        }
    }
    if (VectorDistance3(cancelled_traversal.position,
                        cancellation_point) > 0.18f) {
        (void)fprintf(stderr,
                      "cancelled traversal continued moving: %.2f %.2f\n",
                      cancelled_traversal.position.x,
                      cancelled_traversal.position.z);
        return 1;
    }

    CcLocalAgent boundary_traveller;
    CcLocalAgentInit(&boundary_traveller, (Vector2){78.5f, 29.0f}, false);
    int32_t boundary_portal = StreetPortalIndex(
        &boundary_traveller, "EASTERN KING'S ROAD");
    if (boundary_portal < 0 ||
        !CcLocalAgentFollowStreetPortal(&boundary_traveller,
                                        boundary_portal)) {
        (void)fprintf(stderr,
                      "settlement-boundary traversal portal was not exposed\n");
        return 1;
    }
    for (int32_t frame = 0;
         frame < 6000 && boundary_traveller.navigation_active; ++frame) {
        CcLocalAgentUpdate(&boundary_traveller, 1.0f / 60.0f, false);
    }
    if (boundary_traveller.navigation_active ||
        !CcLocalAgentConsumeWorldExit(&boundary_traveller) ||
        CcLocalAgentConsumeWorldExit(&boundary_traveller)) {
        (void)fprintf(stderr,
                      "boundary traversal did not emit exactly one travel handoff\n");
        return 1;
    }

    static const Vector2 street_rooms[] = {
        {10.5f, 7.5f}, {11.0f, 28.5f}, {14.0f, 52.0f},
        {33.0f, 25.0f}, {44.0f, 29.0f}, {42.0f, 52.0f},
        {50.0f, 27.25f}, {58.0f, 50.0f}, {78.5f, 29.0f},
        {78.0f, 50.0f},
    };
    for (int32_t room = 0;
         room < (int32_t)(sizeof(street_rooms) / sizeof(street_rooms[0]));
         ++room) {
        CcLocalAgent portal_probe;
        CcLocalAgentInit(&portal_probe, street_rooms[room], false);
        int32_t portal_count = CcLocalAgentStreetPortalCount(&portal_probe);
        if (portal_count <= 0) {
            (void)fprintf(stderr, "street room %d has no traversal portal\n",
                          room);
            return 1;
        }
        for (int32_t portal = 0; portal < portal_count; ++portal) {
            CcLocalAgentInit(&portal_probe, street_rooms[room], false);
            const char *portal_name = CcLocalAgentStreetPortalName(
                &portal_probe, portal);
            if (portal_name == NULL ||
                !CcLocalAgentFollowStreetPortal(&portal_probe, portal)) {
                (void)fprintf(stderr,
                              "street room %d portal %d could not start\n",
                              room, portal);
                return 1;
            }
            int32_t destination_room =
                portal_probe.navigation_destination_room;
            for (int32_t update = 0;
                 update < 1200 && portal_probe.navigation_active; ++update) {
                CcLocalAgentUpdate(&portal_probe, 0.10f, false);
            }
            if (portal_probe.navigation_active) {
                (void)fprintf(stderr,
                              "street room %d portal %s did not finish at %.2f %.2f waypoint %d/%d target %.2f %.2f y %.2f exact %d traversal %d\n",
                              room, portal_name, portal_probe.position.x,
                              portal_probe.position.z,
                              portal_probe.navigation_point_index,
                              portal_probe.navigation_point_count,
                              portal_probe.target_point.x,
                              portal_probe.target_point.z,
                              portal_probe.position.y,
                              portal_probe.exact_target_valid,
                              portal_probe.traversal);
                return 1;
            }
            if (destination_room >= 0) {
                float x = portal_probe.position.x -
                          street_rooms[destination_room].x;
                float z = portal_probe.position.z -
                          street_rooms[destination_room].y;
                if (x * x + z * z > 0.40f * 0.40f ||
                    portal_probe.world_exit_requested) {
                    (void)fprintf(stderr,
                                  "street room %d portal %s ended at %.2f %.2f target %.2f %.2f exact %d traversal %d\n",
                                  room, portal_name,
                                  portal_probe.position.x,
                                  portal_probe.position.z,
                                  portal_probe.target_point.x,
                                  portal_probe.target_point.z,
                                  portal_probe.exact_target_valid,
                                  portal_probe.traversal);
                    return 1;
                }
            } else if (!CcLocalAgentConsumeWorldExit(&portal_probe)) {
                (void)fprintf(stderr,
                              "street room %d boundary %s lacked handoff\n",
                              room, portal_name);
                return 1;
            }
        }
    }

    /* Authored centers are the easy case. Exercise every connection again
       from walkable offsets around each room so no route can assume the hero
       begins on the designer's centerline. */
    static const Vector2 room_start_offsets[] = {
        {1.5f, 0.0f}, {-1.5f, 0.0f}, {0.0f, 1.5f}, {0.0f, -1.5f},
        {1.0f, 1.0f}, {-1.0f, -1.0f},
    };
    for (int32_t room = 0;
         room < (int32_t)(sizeof(street_rooms) / sizeof(street_rooms[0]));
         ++room) {
        for (int32_t offset = 0;
             offset < (int32_t)(sizeof(room_start_offsets) /
                                sizeof(room_start_offsets[0])); ++offset) {
            Vector2 start = {
                street_rooms[room].x + room_start_offsets[offset].x,
                street_rooms[room].y + room_start_offsets[offset].y,
            };
            CcLocalAgent offset_probe;
            CcLocalAgentInit(&offset_probe, start, false);
            if (offset_probe.position.y > 0.24f ||
                !CcLocalAgentSetExactTarget(
                    &offset_probe,
                    (Vector3){start.x, 0.0f, start.y}, false)) {
                continue;
            }
            int32_t portal_count =
                CcLocalAgentStreetPortalCount(&offset_probe);
            for (int32_t portal = 0; portal < portal_count; ++portal) {
                CcLocalAgentInit(&offset_probe, start, false);
                const char *portal_name = CcLocalAgentStreetPortalName(
                    &offset_probe, portal);
                if (portal_name == NULL ||
                    !CcLocalAgentFollowStreetPortal(&offset_probe, portal)) {
                    (void)fprintf(stderr,
                                  "offset room %d start %.2f %.2f portal %d could not start\n",
                                  room, start.x, start.y, portal);
                    return 1;
                }
                int32_t destination_room =
                    offset_probe.navigation_destination_room;
                for (int32_t update = 0;
                     update < 1800 && offset_probe.navigation_active;
                     ++update) {
                    CcLocalAgentUpdate(&offset_probe, 0.10f, false);
                }
                if (offset_probe.navigation_active) {
                    (void)fprintf(stderr,
                                  "offset room %d start %.2f %.2f portal %s stalled at %.2f %.2f targeting %.2f %.2f waypoint %d/%d\n",
                                  room, start.x, start.y, portal_name,
                                  offset_probe.position.x,
                                  offset_probe.position.z,
                                  offset_probe.target_point.x,
                                  offset_probe.target_point.z,
                                  offset_probe.navigation_point_index,
                                  offset_probe.navigation_point_count);
                    return 1;
                }
                if (destination_room >= 0) {
                    float x = offset_probe.position.x -
                              street_rooms[destination_room].x;
                    float z = offset_probe.position.z -
                              street_rooms[destination_room].y;
                    if (x * x + z * z > 0.40f * 0.40f ||
                        offset_probe.world_exit_requested) {
                        (void)fprintf(stderr,
                                      "offset room %d portal %s ended at %.2f %.2f\n",
                                      room, portal_name,
                                      offset_probe.position.x,
                                      offset_probe.position.z);
                        return 1;
                    }
                } else if (!CcLocalAgentConsumeWorldExit(&offset_probe)) {
                    (void)fprintf(stderr,
                                  "offset room %d boundary %s lacked handoff\n",
                                  room, portal_name);
                    return 1;
                }
            }
        }
    }

    CcLocalSetStreetMarketCratesInternal(1);
    CcLocalAgent crate_climber;
    CcLocalAgentInit(&crate_climber, (Vector2){43.35f, 26.75f}, false);
    Camera3D crate_camera = CcLocalStreetCameraInternal(
        &crate_climber, 0.0f, false, click_target.texture.height);
    float crate_base = CcLocalTerrainHeightAt(44.40f, 26.75f);
    Vector2 crate_art_point = GetWorldToScreenEx(
        (Vector3){44.40f, crate_base + 0.61f, 26.75f}, crate_camera,
        click_target.texture.width, click_target.texture.height);
    Vector2 crate_screen_point = {
        crate_art_point.x * click_viewport.width /
            (float)click_target.texture.width,
        crate_art_point.y * click_viewport.height /
            (float)click_target.texture.height,
    };
    if (!CcLocalAgentPickTarget(
            &crate_climber, crate_screen_point, click_target,
            click_viewport, false)) {
        (void)fprintf(stderr, "market crate top could not be targeted\n");
        return 1;
    }
    bool climbed_market_crate = false;
    for (int32_t frame = 0; frame < 1200; ++frame) {
        CcLocalAgentUpdate(&crate_climber, 1.0f / 60.0f, false);
        bool inside_crate = crate_climber.position.x > 44.09f &&
                            crate_climber.position.x < 44.71f &&
                            crate_climber.position.z > 26.44f &&
                            crate_climber.position.z < 27.06f;
        if (inside_crate && crate_climber.position.y < crate_base + 0.45f &&
            !crate_climber.climbing) {
            (void)fprintf(stderr,
                          "hero phased into market crate at %.2f %.2f %.2f\n",
                          crate_climber.position.x,
                          crate_climber.position.y,
                          crate_climber.position.z);
            return 1;
        }
        if (crate_climber.position.y > crate_base + 0.50f && inside_crate) {
            climbed_market_crate = true;
        }
        if (climbed_market_crate && !crate_climber.exact_target_valid &&
            !crate_climber.climbing) break;
    }
    CcLocalSetStreetMarketCratesInternal(0);
    if (!climbed_market_crate) {
        (void)fprintf(stderr,
                      "hero did not climb onto the market crate: %.2f %.2f %.2f target %.2f %.2f %.2f command %.2f %.2f %.2f exact %d nav %d climb %d\n",
                      crate_climber.position.x, crate_climber.position.y,
                      crate_climber.position.z,
                      crate_climber.target_point.x,
                      crate_climber.target_point.y,
                      crate_climber.target_point.z,
                      crate_climber.command_point.x,
                      crate_climber.command_point.y,
                      crate_climber.command_point.z,
                      crate_climber.exact_target_valid,
                      crate_climber.navigation_active,
                      crate_climber.climbing);
        return 1;
    }

    TestSharedCombat();
    TestDeathLifecycle();
    TestTargetDrivenCombat();
    TestCombatStanceStability();
    TestCombatCrowdSpacing();
    TestCapePhysics();
    TestControlledJump();
    TestHeroicAthleticism();
    TestIntentionalParkourEntry();
    TestTravellerIngress();
    RequirePosition("market wall blocks entry",
                    CcLocalMove((Vector2){50.00f, 26.65f},
                                (Vector2){0.00f, -1.00f}, false),
                    (Vector2){50.00f, 26.65f});
    RequirePosition("collision slides along facade",
                    CcLocalMove((Vector2){42.50f, 26.65f},
                                (Vector2){2.00f, -1.00f}, false),
                    (Vector2){44.50f, 26.65f});
    RequirePosition("carriage blocks movement",
                    CcLocalMove((Vector2){39.10f, 31.70f},
                                (Vector2){-1.00f, 0.00f}, false),
                    (Vector2){39.10f, 31.70f});
    RequirePosition("castle wall blocks movement",
                    CcLocalMove((Vector2){65.30f, 20.00f},
                                (Vector2){1.00f, 0.00f}, false),
                    (Vector2){65.30f, 20.00f});
    RequirePosition("market counter blocks movement",
                    CcLocalMove((Vector2){7.10f, 3.00f},
                                (Vector2){0.00f, -1.00f}, true),
                    (Vector2){7.10f, 3.00f});
    RequirePosition("open street permits movement",
                    CcLocalMove((Vector2){CC_LOCAL_START_X, CC_LOCAL_START_Z},
                                (Vector2){0.10f, -0.10f}, false),
                    (Vector2){CC_LOCAL_START_X + 0.10f,
                              CC_LOCAL_START_Z - 0.10f});
    RequirePosition("far countryside is part of the same walkable world",
                    CcLocalMove((Vector2){80.00f, 60.00f},
                                (Vector2){0.50f, -0.50f}, false),
                    (Vector2){80.50f, 59.50f});
    RequirePosition("continuous world retains a physical outer boundary",
                    CcLocalMove((Vector2){95.40f, 60.00f},
                                (Vector2){1.00f, 0.00f}, false),
                    (Vector2){95.40f, 60.00f});

    CcLocalAgent agent;
    CcLocalAgentInit(&agent, (Vector2){4.75f, 5.85f}, false);
    if (agent.morphology != CC_MORPHOLOGY_BIPED) {
        (void)fprintf(stderr,
                      "the playable local agent should default to the tuned biped\n");
        return 1;
    }
    CcLocalAgent countryside_agent;
    CcLocalAgentInit(&countryside_agent,
                     (Vector2){CC_LOCAL_START_X, CC_LOCAL_START_Z}, false);
    if (!CcLocalAgentSetExactTarget(
            &countryside_agent, (Vector3){80.0f, 0.0f, 60.0f}, false)) {
        (void)fprintf(stderr,
                      "a distant target in the continuous world was rejected\n");
        return 1;
    }
    CcLocalAgent fallen_edge_agent;
    CcLocalAgentInit(&fallen_edge_agent, (Vector2){3.50f, 6.70f}, false);
    if (!CcHumanoidGaitKnockDown(&fallen_edge_agent.humanoid) ||
        !fallen_edge_agent.humanoid.ragdoll.active ||
        !CcLocalAgentSetExactTarget(
            &fallen_edge_agent, (Vector3){3.50f, 0.0f, 7.50f}, false)) {
        (void)fprintf(stderr,
                      "tower-edge ragdoll admission fixture did not initialize\n");
        return 1;
    }
    CcLocalAgentUpdate(&fallen_edge_agent, 1.0f / 60.0f, false);
    if (fallen_edge_agent.climbing ||
        fallen_edge_agent.traversal == CC_TRAVERSAL_CLIMB ||
        fallen_edge_agent.humanoid.climbing) {
        (void)fprintf(stderr,
                      "half-fallen biped snapped into climbing control\n");
        return 1;
    }
    CcLocalAgent paced_agent;
    CcLocalAgentInit(&paced_agent, (Vector2){4.00f, 5.50f}, true);
    if (!CcLocalAgentSetExactTarget(&paced_agent,
                                    (Vector3){8.00f, 0.0f, 5.50f}, true)) {
        (void)fprintf(stderr, "long biped gait target should be reachable\n");
        return 1;
    }
    float maximum_gait_speed = 0.0f;
    uint32_t pose_mask = 0;
    uint32_t stepped_pose_mask = 0;
    int32_t held_upper_pose_frames = 0;
    CcHumanoidPose previous_stepped_render = paced_agent.render_pose;
    int32_t previous_stepped_bin = -1;
    for (int32_t frame = 0; frame < 600; ++frame) {
        CcLocalAgentUpdate(&paced_agent, 1.0f / 60.0f, true);
        float speed = sqrtf(paced_agent.velocity.x * paced_agent.velocity.x +
                            paced_agent.velocity.z * paced_agent.velocity.z);
        maximum_gait_speed = fmaxf(maximum_gait_speed, speed);
        if (speed > 0.25f) {
            int32_t pose_bin = (int32_t)floorf(
                paced_agent.humanoid.phase * 8.0f) & 7;
            pose_mask |= UINT32_C(1) << pose_bin;
        }
        if (paced_agent.stepped_pose.initialized) {
            int32_t stepped_bin = paced_agent.stepped_pose.locomotion_bin;
            stepped_pose_mask |= UINT32_C(1) << stepped_bin;
            float within = paced_agent.humanoid.phase * 8.0f;
            within -= floorf(within);
            if (stepped_bin == previous_stepped_bin && within > 0.32f &&
                MaximumRelativeUpperPoseStep(
                    &previous_stepped_render,
                    &paced_agent.render_pose) < 0.00001f) {
                held_upper_pose_frames += 1;
            }
            previous_stepped_bin = stepped_bin;
        }
        previous_stepped_render = paced_agent.render_pose;

        const CcHumanoidPoseSnapshot *render_physical =
            CcHumanoidGaitPreviousSnapshot(&paced_agent.humanoid);
        if (render_physical != NULL && paced_agent.stepped_pose.initialized) {
            for (int32_t leg = 0; leg < CC_HUMANOID_LEG_COUNT; ++leg) {
                const CcHumanoidPose *physical = &render_physical->pose;
                const CcHumanoidPose *visual = &paced_agent.render_pose;
                if (Distance3(physical->ankle[leg], visual->ankle[leg]) >
                        0.00001f ||
                    Distance3(physical->heel[leg], visual->heel[leg]) >
                        0.00001f ||
                    Distance3(physical->ball[leg], visual->ball[leg]) >
                        0.00001f ||
                    Distance3(physical->toe[leg], visual->toe[leg]) >
                        0.00001f) {
                    (void)fprintf(stderr,
                                  "stepped render pose broke foot contact\n");
                    return 1;
                }
            }
        }
        for (int32_t leg = 0; leg < CC_HUMANOID_LEG_COUNT; ++leg) {
            const CcHumanoidFoot *foot = &paced_agent.humanoid.feet[leg];
            const CcHumanoidPose *pose = &paced_agent.humanoid.pose;
            if (foot->contact != CC_HUMANOID_CONTACT_SWING &&
                foot->contact != CC_HUMANOID_CONTACT_AIR &&
                Distance3(foot->current_point, foot->planted_point) > 0.001f) {
                (void)fprintf(stderr, "human stance foot slid under load\n");
                return 1;
            }
            if (fabsf(Distance3(pose->hip[leg], pose->knee[leg]) - 0.465f) >
                    0.003f ||
                fabsf(Distance3(pose->knee[leg], pose->ankle[leg]) - 0.475f) >
                    0.003f) {
                (void)fprintf(stderr, "biomechanical leg changed bone length\n");
                return 1;
            }
        }
    }
    if (maximum_gait_speed > 1.53f) {
        (void)fprintf(stderr, "biped gait exceeded its authored walk speed\n");
        return 1;
    }
    if (pose_mask != UINT32_C(0xff)) {
        (void)fprintf(stderr, "biped walk did not traverse every hero pose: 0x%02x\n",
                      pose_mask);
        return 1;
    }
    if (stepped_pose_mask != UINT32_C(0xff) ||
        held_upper_pose_frames < 4) {
        (void)fprintf(stderr,
                      "stepped gait vocabulary was incomplete: mask 0x%02x holds %d\n",
                      stepped_pose_mask, held_upper_pose_frames);
        return 1;
    }
    if (CcBiomechRigMeanActivation(&paced_agent.humanoid.body) <= 0.01f) {
        (void)fprintf(stderr, "human gait did not recruit its muscle pairs\n");
        return 1;
    }

    CcLocalAgent steady_walk;
    CcLocalAgent uneven_walk;
    CcLocalAgentInit(&steady_walk, (Vector2){2.00f, 4.40f}, true);
    CcLocalAgentInit(&uneven_walk, (Vector2){2.00f, 4.40f}, true);
    Vector3 pacing_target = {7.40f, 0.0f, 4.40f};
    if (!CcLocalAgentSetExactTarget(&steady_walk, pacing_target, true) ||
        !CcLocalAgentSetExactTarget(&uneven_walk, pacing_target, true)) {
        (void)fprintf(stderr, "frame-pacing gait target was rejected\n");
        return 1;
    }
    CcHumanoidPose previous_physical = uneven_walk.humanoid.pose;
    CcHumanoidPose previous_render = uneven_walk.render_pose;
    float maximum_physical_step = 0.0f;
    float maximum_render_step = 0.0f;
    for (int32_t pair = 0; pair < 180; ++pair) {
        CcLocalAgentUpdate(&steady_walk, 1.0f / 60.0f, true);
        CcLocalAgentUpdate(&steady_walk, 1.0f / 60.0f, true);
        const float uneven_frame_time[2] = {1.0f / 120.0f, 1.0f / 40.0f};
        for (int32_t sample = 0; sample < 2; ++sample) {
            CcLocalAgentUpdate(&uneven_walk, uneven_frame_time[sample], true);
            maximum_physical_step = fmaxf(
                maximum_physical_step,
                MaximumPoseStep(&previous_physical,
                                &uneven_walk.humanoid.pose));
            maximum_render_step = fmaxf(
                maximum_render_step,
                MaximumPoseStep(&previous_render, &uneven_walk.render_pose));
            previous_physical = uneven_walk.humanoid.pose;
            previous_render = uneven_walk.render_pose;
        }
    }
    if (!uneven_walk.render_pose_valid ||
        VectorDistance3(steady_walk.position, uneven_walk.position) > 0.002f ||
        fabsf(steady_walk.humanoid.phase - uneven_walk.humanoid.phase) >
            0.0002f) {
        (void)fprintf(stderr,
                      "uneven frame pacing changed the physical walk result\n");
        return 1;
    }
    if (maximum_render_step >= maximum_physical_step) {
        (void)fprintf(stderr,
                      "stepped render transition exceeded physical gait bound: visual %.4f physical %.4f\n",
                      maximum_render_step, maximum_physical_step);
        return 1;
    }
    CcHumanoidSkinPose render_skin;
    CcHumanoidSkinPoseResolve(&uneven_walk.render_pose, &render_skin);
    Vector3 expected_cape_anchor = {
        render_skin.sockets[CC_HUMANOID_SOCKET_BACK].position.x +
            render_skin.body_up.x * 0.14f,
        render_skin.sockets[CC_HUMANOID_SOCKET_BACK].position.y +
            render_skin.body_up.y * 0.14f,
        render_skin.sockets[CC_HUMANOID_SOCKET_BACK].position.z +
            render_skin.body_up.z * 0.14f,
    };
    if (!render_skin.valid || !uneven_walk.render_cape.initialized ||
        VectorDistance3(uneven_walk.render_cape.point[0],
                        expected_cape_anchor) > 0.00001f) {
        (void)fprintf(stderr,
                      "render cape did not share the interpolated skeleton clock\n");
        return 1;
    }

    CcLocalAgent retreat_walk;
    CcLocalAgentInit(&retreat_walk, (Vector2){4.00f, 5.50f}, true);
    if (!CcLocalAgentSetExactTarget(
            &retreat_walk, (Vector3){7.00f, 0.0f, 5.50f}, true)) {
        (void)fprintf(stderr, "momentum posture target was rejected\n");
        return 1;
    }
    retreat_walk.combat.focus_valid = true;
    retreat_walk.combat.focus_point = (Vector3){2.00f, 0.0f, 5.50f};
    bool saw_opposed_facing = false;
    float maximum_retreat_lead = 0.0f;
    for (int32_t frame = 0; frame < 75; ++frame) {
        CcLocalAgentUpdate(&retreat_walk, 1.0f / 60.0f, true);
        float speed = sqrtf(retreat_walk.velocity.x *
                            retreat_walk.velocity.x +
                            retreat_walk.velocity.z *
                            retreat_walk.velocity.z);
        if (speed <= 0.45f) continue;
        float momentum_x = retreat_walk.velocity.x / speed;
        float momentum_z = retreat_walk.velocity.z / speed;
        float facing_x = sinf(retreat_walk.facing_yaw);
        float facing_z = cosf(retreat_walk.facing_yaw);
        float facing_alignment = facing_x * momentum_x +
                                 facing_z * momentum_z;
        if (facing_alignment > -0.80f) continue;
        saw_opposed_facing = true;
        CcLimbVec3 torso = {
            retreat_walk.render_pose.neck.x -
                retreat_walk.render_pose.pelvis.x,
            0.0f,
            retreat_walk.render_pose.neck.z -
                retreat_walk.render_pose.pelvis.z,
        };
        maximum_retreat_lead = fmaxf(
            maximum_retreat_lead,
            torso.x * momentum_x + torso.z * momentum_z);
    }
    if (!saw_opposed_facing || maximum_retreat_lead <= 0.105f) {
        (void)fprintf(stderr,
                      "visible torso did not lead a retreating body's momentum: opposed %d lead %.3f\n",
                      saw_opposed_facing, maximum_retreat_lead);
        return 1;
    }

    CcLocalAgent scout_walk;
    CcLocalAgent refugee_walk;
    CcLocalAgentInit(&scout_walk, (Vector2){2.00f, 3.40f}, true);
    CcLocalAgentInit(&refugee_walk, (Vector2){2.00f, 4.40f}, true);
    scout_walk.crowned = false;
    refugee_walk.crowned = false;
    CcLocalAgentSetNpcAppearance(
        &scout_walk, UINT32_C(0x4d4f5645), CC_NPC_ROLE_SCOUT,
        (Color){96, 111, 117, 255});
    CcLocalAgentSetNpcAppearance(
        &refugee_walk, UINT32_C(0x4d4f5645), CC_NPC_ROLE_REFUGEE,
        (Color){96, 111, 117, 255});
    float expected_scout_cadence = 1.0f +
        (scout_walk.appearance.gait_cadence_scale - 1.0f) * 0.40f;
    float expected_refugee_cadence = 1.0f +
        (refugee_walk.appearance.gait_cadence_scale - 1.0f) * 0.40f;
    float expected_scout_stride = 1.0f +
        (scout_walk.appearance.stride_scale - 1.0f) * 0.40f;
    float expected_refugee_stride = 1.0f +
        (refugee_walk.appearance.stride_scale - 1.0f) * 0.40f;
    if (fabsf(scout_walk.humanoid.walk_cadence_scale -
              expected_scout_cadence) > 0.00001f ||
        fabsf(refugee_walk.humanoid.walk_cadence_scale -
              expected_refugee_cadence) > 0.00001f ||
        fabsf(scout_walk.humanoid.walk_stride_scale -
              expected_scout_stride) > 0.00001f ||
        fabsf(refugee_walk.humanoid.walk_stride_scale -
              expected_refugee_stride) > 0.00001f ||
        scout_walk.humanoid.walk_cadence_scale <=
            refugee_walk.humanoid.walk_cadence_scale ||
        scout_walk.humanoid.walk_stride_scale <=
            refugee_walk.humanoid.walk_stride_scale) {
        (void)fprintf(stderr,
                      "NPC movement signature did not reach the live gait\n");
        return 1;
    }

    CcLocalAgent merchant_idle;
    CcLocalAgent scout_idle;
    CcLocalAgentInit(&merchant_idle, (Vector2){3.00f, 3.40f}, true);
    CcLocalAgentInit(&scout_idle, (Vector2){4.00f, 3.40f}, true);
    merchant_idle.crowned = false;
    scout_idle.crowned = false;
    CcLocalAgentSetNpcAppearance(
        &merchant_idle, UINT32_C(0x1d1e600d), CC_NPC_ROLE_MERCHANT,
        (Color){133, 93, 58, 255});
    CcLocalAgentSetNpcAppearance(
        &scout_idle, UINT32_C(0x1d1e600d), CC_NPC_ROLE_SCOUT,
        (Color){80, 112, 102, 255});
    float maximum_merchant_gesture = 0.0f;
    float maximum_role_difference = 0.0f;
    for (int32_t frame = 0; frame < 240; ++frame) {
        CcLocalAgentUpdate(&merchant_idle, 1.0f / 60.0f, true);
        CcLocalAgentUpdate(&scout_idle, 1.0f / 60.0f, true);
        const CcHumanoidPoseSnapshot *merchant_physical =
            CcHumanoidGaitPreviousSnapshot(&merchant_idle.humanoid);
        if (merchant_physical != NULL) {
            maximum_merchant_gesture = fmaxf(
                maximum_merchant_gesture,
                MaximumRelativeUpperPoseStep(
                    &merchant_physical->pose, &merchant_idle.render_pose));
            for (int32_t leg = 0; leg < CC_HUMANOID_LEG_COUNT; ++leg) {
                if (Distance3(merchant_physical->pose.ankle[leg],
                              merchant_idle.render_pose.ankle[leg]) >
                        0.00001f ||
                    Distance3(merchant_physical->pose.toe[leg],
                              merchant_idle.render_pose.toe[leg]) >
                        0.00001f) {
                    (void)fprintf(stderr,
                                  "role idle gesture moved a planted foot\n");
                    return 1;
                }
            }
        }
        maximum_role_difference = fmaxf(
            maximum_role_difference,
            MaximumRelativeUpperPoseStep(&merchant_idle.render_pose,
                                         &scout_idle.render_pose));
    }
    if (!merchant_idle.humanoid.idle.stable ||
        !scout_idle.humanoid.idle.stable ||
        maximum_merchant_gesture < 0.08f ||
        maximum_role_difference < 0.12f) {
        (void)fprintf(stderr,
                      "role idle silhouettes were not distinct: gesture %.3f roles %.3f\n",
                      maximum_merchant_gesture, maximum_role_difference);
        return 1;
    }

    CcSim cadence_sim;
    CcSimInit(&cadence_sim, UINT32_C(0xcade60));
    CcLocalAgent cadence_player;
    CcLocalCourse cadence_course;
    CcLocalAgentInit(&cadence_player,
                     (Vector2){CC_LOCAL_ROAD_START_X,
                               CC_LOCAL_ROAD_START_Z}, false);
    CcLocalCombatSetTeam(&cadence_player, CC_COMBAT_PLAYER);
    CcLocalCourseInit(&cadence_course);
    CcLocalCourseStageRoadEncounter(&cadence_course, &cadence_player, true);
    CcLocalAgent player_60 = cadence_player;
    CcLocalAgent player_30 = cadence_player;
    CcLocalAgent player_144 = cadence_player;
    CcLocalAgent player_hitch = cadence_player;
    CcLocalCourse course_60 = cadence_course;
    CcLocalCourse course_30 = cadence_course;
    CcLocalCourse course_144 = cadence_course;
    CcLocalCourse course_hitch = cadence_course;
    const float cadence_60[] = {1.0f / 60.0f};
    const float cadence_30[] = {1.0f / 30.0f};
    const float cadence_144[] = {1.0f / 144.0f};
    const float cadence_hitch[] = {
        1.0f / 120.0f, 1.0f / 40.0f, 1.0f / 60.0f, 1.0f / 30.0f,
        1.0f / 90.0f, 0.075f
    };
    AdvanceRoadWorld(&course_60, &player_60, &cadence_sim, 6.0f,
                     cadence_60, 1);
    AdvanceRoadWorld(&course_30, &player_30, &cadence_sim, 6.0f,
                     cadence_30, 1);
    AdvanceRoadWorld(&course_144, &player_144, &cadence_sim, 6.0f,
                     cadence_144, 1);
    AdvanceRoadWorld(&course_hitch, &player_hitch, &cadence_sim, 6.0f,
                     cadence_hitch,
                     (int32_t)(sizeof(cadence_hitch) /
                               sizeof(cadence_hitch[0])));
    if (!RoadWorldMatches(&course_60, &player_60,
                          &course_30, &player_30, 0.002f) ||
        !RoadWorldMatches(&course_60, &player_60,
                          &course_144, &player_144, 0.002f) ||
        !RoadWorldMatches(&course_60, &player_60,
                          &course_hitch, &player_hitch, 0.002f)) {
        (void)fprintf(stderr,
                      "local world changed across 30/60/144 Hz or hitch pacing: player %.3f/%.3f/%.3f/%.3f raider %.3f/%.3f/%.3f/%.3f resolve %d/%d/%d/%d\n",
                      player_60.position.x, player_30.position.x,
                      player_144.position.x, player_hitch.position.x,
                      course_60.raiders[0].position.x,
                      course_30.raiders[0].position.x,
                      course_144.raiders[0].position.x,
                      course_hitch.raiders[0].position.x,
                      course_60.raider_resolve, course_30.raider_resolve,
                      course_144.raider_resolve,
                      course_hitch.raider_resolve);
        (void)fprintf(stderr,
                      "guard0 hp %.3f/%.3f/%.3f/%.3f posture %.3f/%.3f/%.3f/%.3f posz %.4f/%.4f/%.4f/%.4f; raider0 hp %.3f/%.3f/%.3f/%.3f posture %.3f/%.3f/%.3f/%.3f stage %d/%d/%d/%d accum %.6f/%.6f/%.6f/%.6f\n",
                      course_60.runners[0].agent.combat.health,
                      course_30.runners[0].agent.combat.health,
                      course_144.runners[0].agent.combat.health,
                      course_hitch.runners[0].agent.combat.health,
                      course_60.runners[0].agent.combat.posture,
                      course_30.runners[0].agent.combat.posture,
                      course_144.runners[0].agent.combat.posture,
                      course_hitch.runners[0].agent.combat.posture,
                      course_60.runners[0].agent.position.z,
                      course_30.runners[0].agent.position.z,
                      course_144.runners[0].agent.position.z,
                      course_hitch.runners[0].agent.position.z,
                      course_60.raiders[0].combat.health,
                      course_30.raiders[0].combat.health,
                      course_144.raiders[0].combat.health,
                      course_hitch.raiders[0].combat.health,
                      course_60.raiders[0].combat.posture,
                      course_30.raiders[0].combat.posture,
                      course_144.raiders[0].combat.posture,
                      course_hitch.raiders[0].combat.posture,
                      course_60.raider_response_stage[0],
                      course_30.raider_response_stage[0],
                      course_144.raider_response_stage[0],
                      course_hitch.raider_response_stage[0],
                      course_60.world_simulation_accumulator,
                      course_30.world_simulation_accumulator,
                      course_144.world_simulation_accumulator,
                      course_hitch.world_simulation_accumulator);
        return 1;
    }
    CcLocalAgent crowd_agent;
    CcLocalAgentInit(&crowd_agent, (Vector2){42.00f, 29.95f}, false);
    if (CcLocalAgentSetExactTarget(&crowd_agent,
                                   (Vector3){42.00f, 0.0f, 30.80f}, false)) {
        (void)fprintf(stderr, "a visible townsperson should occupy physical space\n");
        return 1;
    }
    if (!CcLocalAgentSetExactTarget(&crowd_agent,
                                    (Vector3){42.00f, 0.0f, 31.70f}, false)) {
        (void)fprintf(stderr, "a target beyond a townsperson should remain valid\n");
        return 1;
    }
    float closest_person_distance = 1000.0f;
    for (int32_t frame = 0; frame < 900; ++frame) {
        CcLocalAgentUpdate(&crowd_agent, 1.0f / 60.0f, false);
        float dx = crowd_agent.position.x - 42.00f;
        float dz = crowd_agent.position.z - 30.80f;
        closest_person_distance = fminf(closest_person_distance,
                                        sqrtf(dx * dx + dz * dz));
    }
    RequireNearPosition("agent sidesteps a townsperson",
                        CcLocalAgentPosition(&crowd_agent),
                        (Vector2){42.00f, 31.70f}, 0.12f);
    if (closest_person_distance < 0.565f) {
        (void)fprintf(stderr, "agent clipped through a townsperson\n");
        return 1;
    }

    float initial_facing = agent.facing_yaw;
    Vector3 exact_target = {4.25f, 0.0f, 5.20f};
    if (!CcLocalAgentSetExactTarget(&agent, exact_target, false)) {
        (void)fprintf(stderr, "arbitrary continuous target should be reachable\n");
        return 1;
    }
    for (int32_t frame = 0; frame < 600; ++frame) {
        CcLocalAgentUpdate(&agent, 1.0f / 60.0f, false);
    }
    Vector2 continuous_position = CcLocalAgentPosition(&agent);
    float exact_error_x = continuous_position.x - exact_target.x;
    float exact_error_z = continuous_position.y - exact_target.z;
    if (sqrtf(exact_error_x * exact_error_x + exact_error_z * exact_error_z) > 0.12f ||
        fabsf(agent.facing_yaw - initial_facing) < 0.10f) {
        (void)fprintf(stderr,
                      "continuous target or arbitrary-angle facing did not resolve\n");
        return 1;
    }

    if (!CcLocalAgentSetExactTarget(&agent, (Vector3){3.50f, 0.0f, 7.50f}, false)) {
        (void)fprintf(stderr, "raised physical target should be reachable\n");
        return 1;
    }
    float maximum_height = agent.position.y;
    bool saw_climb = false;
    bool saw_wall_foot = false;
    bool saw_top_foot = false;
    bool saw_left_isolated_scramble = false;
    bool saw_right_isolated_scramble = false;
    bool saw_airborne_swing[CC_HUMANOID_LEG_COUNT] = {false, false};
    bool saw_diagonal_swing[CC_HUMANOID_LEG_COUNT] = {false, false};
    bool saw_opposite_leg_support[CC_HUMANOID_LEG_COUNT] = {false, false};
    bool saw_sole_down_takeoff[CC_HUMANOID_LEG_COUNT] = {false, false};
    bool saw_wall_contact[CC_HUMANOID_LEG_COUNT] = {false, false};
    bool saw_preparation_crouch = false;
    bool saw_chest_lead = false;
    bool saw_trailing_tuck = false;
    uint32_t mantle_markers = 0;
    float maximum_airborne_knee_flexion[CC_HUMANOID_LEG_COUNT] = {0.0f, 0.0f};
    float first_top_foot_phase[CC_HUMANOID_LEG_COUNT] = {-1.0f, -1.0f};
    float deepest_top_foot_plant[CC_HUMANOID_LEG_COUNT] = {0.0f, 0.0f};
    float first_root_inside_phase = -1.0f;
    int32_t climb_frames = 0;
    bool saw_biomechanical_climb = false;
    float maximum_hand_reach = 0.0f;
    float maximum_hand_contact_error = 0.0f;
    float maximum_hand_error_phase = 0.0f;
    int32_t maximum_hand_error_limb = -1;
    float maximum_foot_contact_error = 0.0f;
    float maximum_foot_error_phase = 0.0f;
    int32_t maximum_foot_error_limb = -1;
    float minimum_wall_clearance = 1000.0f;
    float minimum_knee_wall_clearance = 1000.0f;
    float minimum_knee_clearance_phase = 0.0f;
    float maximum_climb_pose_step = 0.0f;
    float maximum_climb_pose_step_phase = 0.0f;
    CcHumanoidPose prior_climb_pose = agent.humanoid.pose;
    CcHumanoidPose maximum_step_before = prior_climb_pose;
    CcHumanoidPose maximum_step_after = prior_climb_pose;
    for (int32_t frame = 0; frame < 1200; ++frame) {
        CcHumanoidPose climb_pose_before = prior_climb_pose;
        bool climb_was_active = agent.traversal == CC_TRAVERSAL_CLIMB ||
                                agent.humanoid.climbing;
        CcLocalAgentUpdate(&agent, 1.0f / 60.0f, false);
        float climb_pose_step = MaximumPoseStep(
            &prior_climb_pose, &agent.humanoid.pose);
        if ((climb_was_active || agent.traversal == CC_TRAVERSAL_CLIMB ||
             agent.humanoid.climbing) &&
            climb_pose_step > maximum_climb_pose_step) {
            maximum_climb_pose_step = climb_pose_step;
            maximum_climb_pose_step_phase = agent.climb_progress;
            maximum_step_before = prior_climb_pose;
            maximum_step_after = agent.humanoid.pose;
        }
        prior_climb_pose = agent.humanoid.pose;
        if (agent.traversal == CC_TRAVERSAL_CLIMB) {
            mantle_markers |= CcHumanoidGaitConsumeMotionMarkers(
                &agent.humanoid);
            saw_climb = true;
            climb_frames += 1;
            saw_biomechanical_climb = saw_biomechanical_climb ||
                (agent.humanoid.climbing && !agent.humanoid_needs_reset);
            float left_ankle_step = Distance3(
                climb_pose_before.ankle[0], agent.humanoid.pose.ankle[0]);
            float right_ankle_step = Distance3(
                climb_pose_before.ankle[1], agent.humanoid.pose.ankle[1]);
            saw_left_isolated_scramble = saw_left_isolated_scramble ||
                (left_ankle_step > 0.012f &&
                 right_ankle_step < left_ankle_step * 0.35f);
            saw_right_isolated_scramble = saw_right_isolated_scramble ||
                (right_ankle_step > 0.012f &&
                 left_ankle_step < right_ankle_step * 0.35f);
            float root_outside =
                (agent.position.x - agent.climb_face.x) *
                    agent.climb_normal.x +
                (agent.position.z - agent.climb_face.z) *
                    agent.climb_normal.z;
            if (root_outside < -0.02f && first_root_inside_phase < 0.0f) {
                first_root_inside_phase = agent.climb_progress;
            }
            saw_preparation_crouch = saw_preparation_crouch ||
                (agent.climb_progress >= 0.07f &&
                 agent.climb_progress <= 0.15f &&
                 agent.humanoid.pose.pelvis.y < agent.climb_start.y + 0.86f);
            float chest_outside =
                (agent.humanoid.pose.chest.x - agent.climb_face.x) *
                    agent.climb_normal.x +
                (agent.humanoid.pose.chest.z - agent.climb_face.z) *
                    agent.climb_normal.z;
            saw_chest_lead = saw_chest_lead ||
                (agent.climb_progress >= 0.64f &&
                 agent.climb_progress <= 0.82f && root_outside > 0.15f &&
                 chest_outside < root_outside - 0.15f);
            for (int32_t limb = 0; limb < 2; ++limb) {
                const CcHumanoidPose *pose = &agent.humanoid.pose;
                const CcHumanoidFoot *foot = &agent.humanoid.feet[limb];
                if (foot->contact == CC_HUMANOID_CONTACT_AIR) {
                    saw_airborne_swing[limb] = true;
                    if (agent.climb_progress < 0.48f &&
                        foot->normal.y > 0.80f) {
                        saw_sole_down_takeoff[limb] = true;
                    }
                    saw_opposite_leg_support[limb] =
                        saw_opposite_leg_support[limb] ||
                        agent.humanoid.feet[1 - limb].contact ==
                            CC_HUMANOID_CONTACT_FLAT;
                    float ankle_dx = pose->ankle[limb].x -
                                     climb_pose_before.ankle[limb].x;
                    float ankle_dy = pose->ankle[limb].y -
                                     climb_pose_before.ankle[limb].y;
                    float ankle_dz = pose->ankle[limb].z -
                                     climb_pose_before.ankle[limb].z;
                    float planar_step = sqrtf(ankle_dx * ankle_dx +
                                              ankle_dz * ankle_dz);
                    saw_diagonal_swing[limb] = saw_diagonal_swing[limb] ||
                        (fabsf(ankle_dy) > 0.002f && planar_step > 0.002f);
                    maximum_airborne_knee_flexion[limb] = fmaxf(
                        maximum_airborne_knee_flexion[limb],
                        pose->knee_flexion[limb]);
                    if (limb == 1 && agent.climb_progress >= 0.45f &&
                        agent.climb_progress <= 0.82f &&
                        foot->normal.y > 0.90f &&
                        pose->ankle[limb].y > agent.climb_start.y + 0.28f &&
                        pose->knee_flexion[limb] > 0.80f) {
                        saw_trailing_tuck = true;
                    }
                }
                if (fabsf(foot->normal.y) < 0.5f) {
                    saw_wall_foot = true;
                    saw_wall_contact[limb] = true;
                    CcLimbVec3 knee = pose->knee[limb];
                    float knee_clearance =
                        (knee.x - agent.climb_face.x) * agent.climb_normal.x +
                        (knee.z - agent.climb_face.z) * agent.climb_normal.z;
                    if (knee_clearance < minimum_knee_wall_clearance) {
                        minimum_knee_wall_clearance = knee_clearance;
                        minimum_knee_clearance_phase = agent.climb_progress;
                    }
                }
                if (foot->normal.y > 0.9f &&
                    foot->planted_point.y > 1.60f) {
                    saw_top_foot = true;
                    if (first_top_foot_phase[limb] < 0.0f) {
                        first_top_foot_phase[limb] = agent.climb_progress;
                    }
                    float plant_depth = -(
                        (foot->planted_point.x - agent.climb_face.x) *
                            agent.climb_normal.x +
                        (foot->planted_point.z - agent.climb_face.z) *
                            agent.climb_normal.z);
                    deepest_top_foot_plant[limb] = fmaxf(
                        deepest_top_foot_plant[limb], plant_depth);
                }
                if (agent.climb_progress >= 0.18f &&
                    foot->contact == CC_HUMANOID_CONTACT_FLAT) {
                    CcLimbVec3 solved_contact = {
                        pose->ankle[limb].x - foot->normal.x * 0.085f,
                        pose->ankle[limb].y - foot->normal.y * 0.085f,
                        pose->ankle[limb].z - foot->normal.z * 0.085f
                    };
                    float foot_error = Distance3(
                        solved_contact, foot->planted_point);
                    if (foot_error > maximum_foot_contact_error) {
                        maximum_foot_contact_error = foot_error;
                        maximum_foot_error_phase = agent.climb_progress;
                        maximum_foot_error_limb = limb;
                    }
                }
                if (fabsf(Distance3(pose->hip[limb], pose->knee[limb]) -
                          0.465f) > 0.004f ||
                    fabsf(Distance3(pose->knee[limb], pose->ankle[limb]) -
                          0.475f) > 0.004f) {
                    (void)fprintf(stderr,
                                  "biomechanical climb changed a leg bone length\n");
                    return 1;
                }
            }
            if (agent.climb_progress >= 0.24f && agent.climb_progress < 0.78f) {
                const CcHumanoidPose *pose = &agent.humanoid.pose;
                for (int32_t arm = 0; arm < CC_HUMANOID_ARM_COUNT; ++arm) {
                    if (fabsf(Distance3(pose->shoulder[arm],
                                        pose->elbow[arm]) - 0.34f) > 0.004f ||
                        fabsf(Distance3(pose->elbow[arm],
                                        pose->hand[arm]) - 0.35f) > 0.004f) {
                        (void)fprintf(stderr,
                                      "biomechanical climb changed an arm bone length\n");
                        return 1;
                    }
                }
                maximum_hand_reach = fmaxf(
                    maximum_hand_reach,
                    fmaxf(Distance3(pose->shoulder[0],
                                    pose->hand[0]),
                          Distance3(pose->shoulder[1], pose->hand[1])));
                for (int32_t arm = 0; arm < CC_HUMANOID_ARM_COUNT; ++arm) {
                    float hand_error = Distance3(
                        pose->hand[arm], ExpectedTopOutHand(&agent, arm));
                    if (hand_error > maximum_hand_contact_error) {
                        maximum_hand_contact_error = hand_error;
                        maximum_hand_error_phase = agent.climb_progress;
                        maximum_hand_error_limb = arm;
                    }
                }
                float clearance =
                    (agent.position.x - agent.climb_face.x) *
                        agent.climb_normal.x +
                    (agent.position.z - agent.climb_face.z) *
                        agent.climb_normal.z;
                minimum_wall_clearance = fminf(minimum_wall_clearance, clearance);
            }
        }
        if (agent.position.y > maximum_height) maximum_height = agent.position.y;
    }
    if (sqrtf((agent.position.x - 3.50f) * (agent.position.x - 3.50f) +
              (agent.position.z - 7.50f) * (agent.position.z - 7.50f)) > 0.12f) {
        (void)fprintf(stderr,
                      "raised route debug: pos %.3f %.3f %.3f end %.3f %.3f %.3f progress %.3f climbing %d grounded %d traversal %d velocity %.3f %.3f %.3f\n",
                      agent.position.x, agent.position.y, agent.position.z,
                      agent.climb_end.x, agent.climb_end.y, agent.climb_end.z,
                      agent.climb_progress, agent.climbing, agent.grounded,
                      agent.traversal, agent.velocity.x, agent.velocity.y,
                      agent.velocity.z);
    }
    RequireNearPosition("agent reaches selected raised surface",
                        CcLocalAgentPosition(&agent), (Vector2){3.50f, 7.50f}, 0.12f);
    if (fabsf(agent.position.y - 1.65f) >= 0.01f || !agent.grounded ||
        maximum_height > 1.66f || !saw_climb || climb_frames < 30 ||
        !saw_wall_foot || !saw_top_foot || !saw_biomechanical_climb ||
        !saw_left_isolated_scramble || !saw_right_isolated_scramble ||
        !saw_airborne_swing[0] || !saw_airborne_swing[1] ||
        !saw_diagonal_swing[0] || !saw_diagonal_swing[1] ||
        !saw_opposite_leg_support[0] || !saw_opposite_leg_support[1] ||
        !saw_sole_down_takeoff[0] || !saw_sole_down_takeoff[1] ||
        !saw_wall_contact[0] || saw_wall_contact[1] ||
        !saw_preparation_crouch || !saw_chest_lead || !saw_trailing_tuck ||
        (mantle_markers & CC_MOTION_MARKER_LEFT_HAND_CONTACT) == 0U ||
        (mantle_markers & CC_MOTION_MARKER_RIGHT_HAND_CONTACT) == 0U ||
        (mantle_markers & CC_MOTION_MARKER_WEIGHT_TRANSFER) == 0U ||
        (mantle_markers & CC_MOTION_MARKER_LEFT_CONTACT) == 0U ||
        (mantle_markers & CC_MOTION_MARKER_RIGHT_CONTACT) == 0U ||
        (mantle_markers & CC_MOTION_MARKER_RECOVERY) == 0U ||
        maximum_airborne_knee_flexion[0] < 0.35f ||
        maximum_airborne_knee_flexion[1] < 0.35f ||
        first_root_inside_phase < 0.0f ||
        first_top_foot_phase[0] < 0.0f || first_top_foot_phase[1] < 0.0f ||
        first_top_foot_phase[0] + 0.05f >= first_top_foot_phase[1] ||
        first_root_inside_phase + 0.02f < first_top_foot_phase[0] ||
        deepest_top_foot_plant[0] < 0.27f ||
        deepest_top_foot_plant[1] < 0.23f) {
        (void)fprintf(stderr,
                      "climb traversal incomplete: y %.2f max %.2f climb %d frames %d wall %d top %d biotech %d scramble %d/%d air %d/%d diagonal %d/%d support %d/%d sole %d/%d wall-leg %d/%d prep/chest/tuck %d/%d/%d markers 0x%x knee %.3f/%.3f contacts %.3f/%.3f root %.3f depth %.3f/%.3f\n",
                      agent.position.y, maximum_height, saw_climb, climb_frames,
                      saw_wall_foot, saw_top_foot, saw_biomechanical_climb,
                      saw_left_isolated_scramble,
                      saw_right_isolated_scramble,
                      saw_airborne_swing[0], saw_airborne_swing[1],
                      saw_diagonal_swing[0], saw_diagonal_swing[1],
                      saw_opposite_leg_support[0],
                      saw_opposite_leg_support[1],
                      saw_sole_down_takeoff[0], saw_sole_down_takeoff[1],
                      saw_wall_contact[0], saw_wall_contact[1],
                      saw_preparation_crouch, saw_chest_lead,
                      saw_trailing_tuck, mantle_markers,
                      maximum_airborne_knee_flexion[0],
                      maximum_airborne_knee_flexion[1],
                      first_top_foot_phase[0], first_top_foot_phase[1],
                      first_root_inside_phase, deepest_top_foot_plant[0],
                      deepest_top_foot_plant[1]);
        return 1;
    }
    if (maximum_hand_reach > 0.69f || maximum_hand_contact_error > 0.025f ||
        maximum_foot_contact_error > 0.025f ||
        maximum_climb_pose_step > 0.085f || minimum_wall_clearance < 0.25f ||
        minimum_knee_wall_clearance < -0.001f) {
        (void)fprintf(stderr,
                      "climb contacts broke biotech constraints: duration %.3f reach %.3f hand %.3f at %.3f limb %d feet %.3f at %.3f limb %d step %.3f at %.3f wall %.3f knee %.3f at %.3f\n",
                      agent.climb_duration,
                      maximum_hand_reach, maximum_hand_contact_error,
                      maximum_hand_error_phase, maximum_hand_error_limb,
                      maximum_foot_contact_error,
                      maximum_foot_error_phase, maximum_foot_error_limb,
                      maximum_climb_pose_step, maximum_climb_pose_step_phase,
                      minimum_wall_clearance, minimum_knee_wall_clearance,
                      minimum_knee_clearance_phase);
        for (int32_t limb = 0; limb < 2; ++limb) {
            (void)fprintf(stderr,
                          "step limb %d hip %.3f knee %.3f ankle %.3f heel %.3f toe %.3f shoulder %.3f elbow %.3f hand %.3f\n",
                          limb,
                          Distance3(maximum_step_before.hip[limb],
                                    maximum_step_after.hip[limb]),
                          Distance3(maximum_step_before.knee[limb],
                                    maximum_step_after.knee[limb]),
                          Distance3(maximum_step_before.ankle[limb],
                                    maximum_step_after.ankle[limb]),
                          Distance3(maximum_step_before.heel[limb],
                                    maximum_step_after.heel[limb]),
                          Distance3(maximum_step_before.toe[limb],
                                    maximum_step_after.toe[limb]),
                          Distance3(maximum_step_before.shoulder[limb],
                                    maximum_step_after.shoulder[limb]),
                          Distance3(maximum_step_before.elbow[limb],
                                    maximum_step_after.elbow[limb]),
                          Distance3(maximum_step_before.hand[limb],
                                    maximum_step_after.hand[limb]));
        }
        return 1;
    }

    if (!CcLocalAgentSetExactTarget(&agent, (Vector3){3.50f, 0.0f, 6.50f}, false)) {
        (void)fprintf(stderr, "ground below raised target should be reachable\n");
        return 1;
    }
    bool saw_downclimb = false;
    bool saw_ragdoll = false;
    int32_t downclimb_frames = 0;
    float maximum_downclimb_pose_step = 0.0f;
    float maximum_downclimb_pose_step_phase = 0.0f;
    float maximum_downclimb_hand_error = 0.0f;
    float maximum_downclimb_hand_error_phase = 0.0f;
    CcHumanoidPose previous_downclimb_pose = agent.humanoid.pose;
    for (int32_t frame = 0; frame < 600; ++frame) {
        CcLocalAgentUpdate(&agent, 1.0f / 60.0f, false);
        float pose_step = MaximumPoseStep(&previous_downclimb_pose,
                                          &agent.humanoid.pose);
        if (agent.traversal == CC_TRAVERSAL_DESCEND && agent.climbing_down) {
            saw_downclimb = true;
            downclimb_frames += 1;
            if (pose_step > maximum_downclimb_pose_step) {
                maximum_downclimb_pose_step = pose_step;
                maximum_downclimb_pose_step_phase = agent.climb_progress;
            }
            if (agent.climb_progress >= 0.48f &&
                agent.climb_progress < 0.76f) {
                float hand_error = fmaxf(Distance3(
                              agent.humanoid.pose.hand[0],
                              (CcLimbVec3){agent.climb_hand_left.x,
                                           agent.climb_hand_left.y,
                                           agent.climb_hand_left.z}),
                          Distance3(
                              agent.humanoid.pose.hand[1],
                              (CcLimbVec3){agent.climb_hand_right.x,
                                           agent.climb_hand_right.y,
                                           agent.climb_hand_right.z}));
                if (hand_error > maximum_downclimb_hand_error) {
                    maximum_downclimb_hand_error = hand_error;
                    maximum_downclimb_hand_error_phase = agent.climb_progress;
                }
            }
            for (int32_t leg = 0; leg < CC_HUMANOID_LEG_COUNT; ++leg) {
                const CcHumanoidPose *pose = &agent.humanoid.pose;
                if (fabsf(Distance3(pose->hip[leg], pose->knee[leg]) -
                          0.465f) > 0.004f ||
                    fabsf(Distance3(pose->knee[leg], pose->ankle[leg]) -
                          0.475f) > 0.004f) {
                    (void)fprintf(stderr,
                                  "down-climb changed an anatomical bone length\n");
                    return 1;
                }
            }
        }
        previous_downclimb_pose = agent.humanoid.pose;
        if (agent.humanoid.ragdoll.active &&
            agent.traversal == CC_TRAVERSAL_RAGDOLL) saw_ragdoll = true;
    }
    float drop_error_x = agent.position.x - 3.50f;
    float drop_error_z = agent.position.z - 6.50f;
    if (sqrtf(drop_error_x * drop_error_x + drop_error_z * drop_error_z) > 0.12f) {
        (void)fprintf(stderr,
                      "down-climb target debug: active %d target %d climb %d/%d traversal %d ready-null %d settled %.3f time %.3f speed %.3f pos %.3f %.3f\n",
                      agent.humanoid.ragdoll.active,
                      agent.exact_target_valid,
                      agent.climbing, agent.humanoid.climbing,
                      agent.traversal,
                      CcHumanoidGaitClimbReady(
                          &agent.humanoid,
                          (CcLimbVec3){agent.position.x, agent.position.y,
                                       agent.position.z},
                          agent.facing_yaw, NULL, NULL, 0.025f),
                      agent.humanoid.ragdoll_settled_time,
                      agent.humanoid.ragdoll_time,
                      agent.humanoid.speed.value,
                      agent.position.x, agent.position.z);
        return 1;
    }
    if (!saw_downclimb || saw_ragdoll || downclimb_frames < 30 ||
        maximum_downclimb_pose_step > 0.085f ||
        maximum_downclimb_hand_error > 0.025f ||
        fabsf(agent.position.y) >= 0.01f || !agent.grounded) {
        (void)fprintf(stderr,
                      "controlled down-climb did not resolve: descent %d frames %d step %.3f@%.3f hand %.3f@%.3f ragdoll %d active %d target %d pos %.3f %.3f\n",
                      saw_downclimb, downclimb_frames,
                      maximum_downclimb_pose_step,
                      maximum_downclimb_pose_step_phase,
                      maximum_downclimb_hand_error,
                      maximum_downclimb_hand_error_phase, saw_ragdoll,
                      agent.humanoid.ragdoll.active, agent.exact_target_valid,
                      agent.position.x, agent.position.z);
        return 1;
    }

    for (int32_t limb = 0; limb < agent.limb_rig.morphology.limb_count; ++limb) {
        const CcLimbSpec *spec = &agent.limb_rig.morphology.limbs[limb];
        const CcLimbRuntime *runtime = &agent.limb_rig.limbs[limb];
        for (int32_t segment = 0; segment < spec->segment_count; ++segment) {
            CcLimbVec3 a = runtime->joints[segment];
            CcLimbVec3 b = runtime->joints[segment + 1];
            float x = b.x - a.x;
            float y = b.y - a.y;
            float z = b.z - a.z;
            float length = sqrtf(x * x + y * y + z * z);
            if (fabsf(length - spec->segment_length[segment]) > 0.015f) {
                (void)fprintf(stderr, "generalized limb constraint drifted\n");
                return 1;
            }
        }
    }

    RunTowerFallScenario("south tower fall", (Vector2){3.50f, 6.20f},
                         (Vector3){3.50f, 0.0f, 7.50f},
                         (Vector3){3.50f, 0.0f, 6.20f});
    RunTowerFallScenario("west tower fall", (Vector2){2.35f, 7.50f},
                         (Vector3){3.50f, 0.0f, 7.50f},
                         (Vector3){2.35f, 0.0f, 7.50f});
    RunTowerFallScenario("east tower fall", (Vector2){4.65f, 7.50f},
                         (Vector3){3.50f, 0.0f, 7.50f},
                         (Vector3){4.65f, 0.0f, 7.50f});
    RunTowerFallScenario("north tower fall", (Vector2){3.50f, 8.80f},
                         (Vector3){3.50f, 0.0f, 7.50f},
                         (Vector3){3.50f, 0.0f, 8.80f});
    TestWallScrapeAndCornerImpact();
    TestShoulderLanding();

    CcLocalCourse course;
    CcLocalCourseInit(&course);
    course.alarm_countdown = 1000.0f;
    float runner_travel[CC_LOCAL_COURSE_RUNNER_COUNT] = {0};
    bool runner_climbed[CC_LOCAL_COURSE_RUNNER_COUNT] = {false};
    bool runner_descended[CC_LOCAL_COURSE_RUNNER_COUNT] = {false};
    bool runner_ragdolled[CC_LOCAL_COURSE_RUNNER_COUNT] = {false};
    bool runner_swam[CC_LOCAL_COURSE_RUNNER_COUNT] = {false};
    int32_t runner_advances[CC_LOCAL_COURSE_RUNNER_COUNT] = {0};
    for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
        CcLocalAgent *runner = &course.runners[i].agent;
        if (runner->morphology != CC_MORPHOLOGY_BIPED || runner->crowned ||
            !runner->exact_target_valid) {
            (void)fprintf(stderr,
                          "course runner %d was not initialized as an autonomous biped\n",
                          i);
            return 1;
        }
    }
    for (int32_t frame = 0; frame < 3600; ++frame) {
        Vector2 before[CC_LOCAL_COURSE_RUNNER_COUNT];
        int32_t before_waypoint[CC_LOCAL_COURSE_RUNNER_COUNT];
        for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
            before[i] = CcLocalAgentPosition(&course.runners[i].agent);
            before_waypoint[i] = course.runners[i].next_waypoint;
        }
        CcLocalCourseUpdate(&course, NULL, NULL, 1.0f / 60.0f);
        for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
            const CcLocalAgent *runner = &course.runners[i].agent;
            Vector2 after = CcLocalAgentPosition(runner);
            float dx = after.x - before[i].x;
            float dz = after.y - before[i].y;
            runner_travel[i] += sqrtf(dx * dx + dz * dz);
            if (course.runners[i].next_waypoint != before_waypoint[i]) {
                runner_advances[i] += 1;
            }
            runner_climbed[i] = runner_climbed[i] ||
                                runner->traversal == CC_TRAVERSAL_CLIMB;
            runner_descended[i] = runner_descended[i] ||
                                  runner->traversal == CC_TRAVERSAL_DESCEND;
            runner_ragdolled[i] = runner_ragdolled[i] ||
                                  runner->traversal == CC_TRAVERSAL_RAGDOLL;
            runner_swam[i] = runner_swam[i] ||
                             runner->traversal == CC_TRAVERSAL_SWIM;
            if (runner->traversal == CC_TRAVERSAL_SWIM &&
                (runner->humanoid.action != CC_HUMANOID_ACTION_SWIM ||
                 runner->humanoid.planted_count != 0 || runner->grounded)) {
                (void)fprintf(stderr,
                              "swimmer retained terrestrial support contacts\n");
                return 1;
            }
            if (!isfinite(runner->position.x) ||
                !isfinite(runner->position.y) ||
                !isfinite(runner->position.z) || runner->position.x < 0.0f ||
                runner->position.x > 16.0f || runner->position.z < 0.0f ||
                runner->position.z > 11.0f) {
                (void)fprintf(stderr,
                              "course runner %d escaped the physical course\n", i);
                return 1;
            }
        }
    }
    int32_t climbers = 0;
    int32_t descenders = 0;
    int32_t ragdolls = 0;
    int32_t swimmers = 0;
    for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
        climbers += runner_climbed[i] ? 1 : 0;
        descenders += runner_descended[i] ? 1 : 0;
        ragdolls += runner_ragdolled[i] ? 1 : 0;
        swimmers += runner_swam[i] ? 1 : 0;
        if (runner_travel[i] < 3.0f || runner_advances[i] < 3) {
            Vector2 position = CcLocalAgentPosition(&course.runners[i].agent);
            const CcLocalAgent *stalled = &course.runners[i].agent;
            (void)fprintf(stderr,
                          "course runner %d stalled: travel %.2f at %.2f,%.2f y %.2f advances %d next %d traversal %d target %d ragdoll %d recover %d\n",
                          i, runner_travel[i], position.x, position.y,
                          stalled->position.y, runner_advances[i],
                          course.runners[i].next_waypoint, stalled->traversal,
                          stalled->exact_target_valid,
                          stalled->humanoid.ragdoll.active,
                          stalled->humanoid.recovering);
            return 1;
        }
    }
    if (climbers != CC_LOCAL_COURSE_RUNNER_COUNT ||
        descenders != CC_LOCAL_COURSE_RUNNER_COUNT || ragdolls != 0 ||
        swimmers < 1) {
        (void)fprintf(stderr,
                      "course traversal failed: climb %d descend %d swim %d ragdoll %d\n",
                      climbers, descenders, swimmers, ragdolls);
        return 1;
    }

    CcSim witness_sim;
    CcSimInit(&witness_sim, UINT32_C(0x717e55));
    const CcSituation *visible_situation = NULL;
    CcId witness_settlement = 0U;
    for (int32_t situation = 0;
         situation < witness_sim.situation_count && visible_situation == NULL;
         ++situation) {
        if (witness_sim.situations[situation].status !=
            CC_SITUATION_ACTIVE) continue;
        for (int32_t settlement = 0;
             settlement < witness_sim.settlement_count; ++settlement) {
            if (CcSimSituationTouchesSettlement(
                    &witness_sim, &witness_sim.situations[situation],
                    witness_sim.settlements[settlement].id)) {
                visible_situation = &witness_sim.situations[situation];
                witness_settlement = witness_sim.settlements[settlement].id;
                break;
            }
        }
    }
    if (visible_situation == NULL ||
        visible_situation->affected_name[0] == '\0') {
        (void)fprintf(stderr, "generated situation has no visible local cast\n");
        return 1;
    }
    witness_sim.player.location_id = witness_settlement;
    CcLocalCourse witness_course;
    CcLocalCourseInit(&witness_course);
    CcLocalCourseUpdate(&witness_course, NULL, &witness_sim,
                        1.0f / 60.0f);
    const CcSituation *staged_situation = CcSimSituation(
        &witness_sim, witness_course.situation_witness_id);
    Vector3 witness_board = {CC_LOCAL_NOTICE_X + 0.92f, 0.0f,
                             CC_LOCAL_NOTICE_Z + 0.72f};
    witness_board.y = CcLocalTerrainHeightAt(witness_board.x,
                                              witness_board.z);
    if (!witness_course.situation_witness_active ||
        staged_situation == NULL || staged_situation->affected_name[0] == '\0' ||
        VectorDistance3(witness_course.situation_witness.position,
                        witness_board) > 0.65f) {
        (void)fprintf(stderr,
                      "named situation witness was not staged at the local board: active %d id %llu/%llu pos %.2f %.2f %.2f\n",
                      witness_course.situation_witness_active,
                      (unsigned long long)witness_course.situation_witness_id,
                      (unsigned long long)(staged_situation != NULL ?
                                           staged_situation->id : 0U),
                      witness_course.situation_witness.position.x,
                      witness_course.situation_witness.position.y,
                      witness_course.situation_witness.position.z);
        return 1;
    }
    for (int32_t i = 0; i < witness_sim.situation_count; ++i) {
        witness_sim.situations[i].status = CC_SITUATION_RESOLVED;
    }
    CcLocalCourseUpdate(&witness_course, NULL, &witness_sim,
                        1.0f / 60.0f);
    if (witness_course.situation_witness_active) {
        (void)fprintf(stderr,
                      "situation witness remained after every local need closed\n");
        return 1;
    }

    CcLocalAgent road_player;
    CcLocalAgentInit(&road_player,
                     (Vector2){CC_LOCAL_ROAD_START_X,
                               CC_LOCAL_ROAD_START_Z}, false);
    CcLocalCombatSetTeam(&road_player, CC_COMBAT_PLAYER);
    CcLocalCourse road_course;
    CcLocalCourseInit(&road_course);
    CcLocalCourseStageRoadEncounter(&road_course, &road_player, true);
    if (!road_course.road_encounter || !road_course.alarm_active ||
        fabsf(road_player.position.x - CC_LOCAL_ROAD_START_X) > 0.01f ||
        road_course.raiders[0].position.x < road_player.position.x + 8.0f ||
        fabsf(road_course.raiders[0].position.y - 0.56f) > 0.02f) {
        (void)fprintf(stderr,
                      "hostile road encounter was not staged on the bridge deck: player %.2f %.2f raider %.2f %.2f\n",
                      road_player.position.x, road_player.position.y,
                      road_course.raiders[0].position.x,
                      road_course.raiders[0].position.y);
        return 1;
    }
    if (!CcLocalCourseSelectPlayerTarget(&road_course, &road_player, 0)) {
        (void)fprintf(stderr,
                      "hostile road encounter did not accept its first target\n");
        return 1;
    }
    Camera3D road_combat_base = {
        .position = {51.60f, 5.35f, 54.50f},
        .target = {48.60f, 0.95f, 40.00f},
        .up = {0.0f, 1.0f, 0.0f},
        .fovy = 10.8f,
        .projection = CAMERA_ORTHOGRAPHIC,
    };
    Camera3D road_combat_camera = road_combat_base;
    for (int32_t frame = 0; frame < 36; ++frame) {
        CcLocalWorldUpdate(&road_course, &road_player, &witness_sim,
                           1.0f / 60.0f, false, true);
        camera_clock += 1.0f / 60.0f;
        road_combat_camera = CcLocalCombatCameraInternal(
            road_combat_base, &road_player, &road_course, camera_clock,
            true, click_target.texture.height);
    }
    if (road_player.combat.target_index != 0) {
        (void)fprintf(stderr,
                      "hostile road encounter lost its target: player %.0f/%d raider %.0f/%d retreat %d\n",
                      road_player.combat.health,
                      road_player.combat.life_state,
                      road_course.raiders[0].combat.health,
                      road_course.raiders[0].combat.life_state,
                      road_course.raiders_retreating);
        return 1;
    }
    Vector3 road_fight = {
        road_course.raiders[0].position.x - road_player.position.x,
        0.0f,
        road_course.raiders[0].position.z - road_player.position.z,
    };
    road_fight.y = 0.0f;
    float road_fight_length = sqrtf(
        road_fight.x * road_fight.x + road_fight.z * road_fight.z);
    road_fight.x /= road_fight_length;
    road_fight.z /= road_fight_length;
    Vector3 road_camera_from_player = {
        road_combat_camera.position.x - road_player.position.x,
        road_combat_camera.position.y - road_player.position.y,
        road_combat_camera.position.z - road_player.position.z,
    };
    float road_behind = road_camera_from_player.x * road_fight.x +
                        road_camera_from_player.z * road_fight.z;
    float road_side = road_camera_from_player.x * -road_fight.z +
                      road_camera_from_player.z * road_fight.x;
    Vector2 road_player_screen = GetWorldToScreenEx(
        (Vector3){road_player.position.x, road_player.position.y + 1.02f,
                  road_player.position.z},
        road_combat_camera, click_target.texture.width,
        click_target.texture.height);
    Vector2 road_raider_screen = GetWorldToScreenEx(
        (Vector3){road_course.raiders[0].position.x,
                  road_course.raiders[0].position.y + 1.02f,
                  road_course.raiders[0].position.z},
        road_combat_camera, click_target.texture.width,
        click_target.texture.height);
    bool road_subjects_safe =
        road_player_screen.x > 20.0f && road_player_screen.x < 437.0f &&
        road_player_screen.y > 12.0f && road_player_screen.y < 273.0f &&
        road_raider_screen.x > 20.0f && road_raider_screen.x < 437.0f &&
        road_raider_screen.y > 12.0f && road_raider_screen.y < 273.0f;
    if (road_combat_camera.projection != CAMERA_PERSPECTIVE ||
        road_behind > -3.50f || fabsf(road_side) < 1.50f ||
        !road_subjects_safe) {
        (void)fprintf(stderr,
                      "bridge shoulder framing failed: projection %d behind %.2f side %.2f player %.1f,%.1f raider %.1f,%.1f\n",
                      road_combat_camera.projection, road_behind, road_side,
                      road_player_screen.x, road_player_screen.y,
                      road_raider_screen.x, road_raider_screen.y);
        return 1;
    }
    bool road_attackers_advanced = false;
    bool road_hands_advanced = false;
    for (int32_t frame = 0; frame < 900; ++frame) {
        CcLocalAgentUpdate(&road_player, 1.0f / 60.0f, false);
        CcLocalCourseUpdate(&road_course, &road_player, &witness_sim,
                            1.0f / 60.0f);
        road_attackers_advanced = road_attackers_advanced ||
            road_course.raider_response_stage[0] > 0;
        road_hands_advanced = road_hands_advanced ||
            road_course.runners[0].response_stage > 0;
    }
    if (!road_attackers_advanced || !road_hands_advanced) {
        (void)fprintf(stderr,
                      "road encounter actors did not enter along their route lanes: raider stage %d at %.2f %.2f guard stage %d at %.2f %.2f\n",
                      road_course.raider_response_stage[0],
                      road_course.raiders[0].position.x,
                      road_course.raiders[0].position.z,
                      road_course.runners[0].response_stage,
                      road_course.runners[0].agent.position.x,
                      road_course.runners[0].agent.position.z);
        return 1;
    }

    CcLocalCourse parley_course;
    CcLocalAgentInit(&road_player,
                     (Vector2){CC_LOCAL_ROAD_START_X,
                               CC_LOCAL_ROAD_START_Z}, false);
    CcLocalCombatSetTeam(&road_player, CC_COMBAT_PLAYER);
    CcLocalCourseInit(&parley_course);
    CcLocalCourseStageRoadEncounter(&parley_course, &road_player, false);
    if (CcLocalAgentSetExactTarget(
            &road_player, (Vector3){50.20f, 0.0f, 38.55f}, false) ||
        CcLocalAgentSetExactTarget(
            &road_player, (Vector3){50.20f, 0.0f, 37.80f}, false)) {
        (void)fprintf(stderr,
                      "authored bridge parapet or toll house accepted a walk target\n");
        return 1;
    }
    if (!parley_course.road_encounter || parley_course.alarm_active ||
        parley_course.raiders[0].combat.weapon_mode != CC_WEAPON_NONE ||
        VectorDistance3(
            parley_course.raiders[0].position,
            (Vector3){CC_LOCAL_ROAD_PARLEY_X, 0.0f,
                      CC_LOCAL_ROAD_PARLEY_Z}) > 1.55f ||
        !CcLocalAgentSetExactTarget(
            &road_player,
            (Vector3){CC_LOCAL_ROAD_PARLEY_X, 0.0f,
                      CC_LOCAL_ROAD_PARLEY_Z}, false)) {
        (void)fprintf(stderr,
                      "road parley did not expose a reachable unarmed collector\n");
        return 1;
    }
    for (int32_t frame = 0; frame < 900 &&
                            road_player.exact_target_valid; ++frame) {
        CcLocalAgentUpdate(&road_player, 1.0f / 60.0f, false);
    }
    if (fabsf(road_player.position.x - CC_LOCAL_ROAD_PARLEY_X) > 0.35f ||
        fabsf(road_player.position.z - CC_LOCAL_ROAD_PARLEY_Z) > 0.35f) {
        (void)fprintf(stderr,
                      "player could not physically reach the road collector\n");
        return 1;
    }

    CcSim defense_sim;
    CcSimInit(&defense_sim, 42U);
    CcLocalCourse defense;
    CcLocalCourseInit(&defense);
    defense.alarm_countdown = 1000.0f;
    for (int32_t frame = 0; frame < 720; ++frame) {
        CcLocalCourseUpdate(&defense, NULL, &defense_sim, 1.0f / 60.0f);
    }
    Vector3 guard_before_alarm[CC_LOCAL_COURSE_RUNNER_COUNT];
    Vector3 raider_before_alarm[CC_LOCAL_RAIDER_COUNT];
    for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
        guard_before_alarm[i] = defense.runners[i].agent.position;
    }
    for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
        raider_before_alarm[i] = defense.raiders[i].position;
    }
    CcLocalCourseRaiseAlarm(&defense);
    for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
        if (VectorDistance3(guard_before_alarm[i],
                            defense.runners[i].agent.position) > 0.0001f) {
            (void)fprintf(stderr, "guard %d warped when the alarm began\n", i);
            return 1;
        }
    }
    for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
        if (VectorDistance3(raider_before_alarm[i],
                            defense.raiders[i].position) > 0.0001f) {
            (void)fprintf(stderr, "raider %d warped when the alarm began\n", i);
            return 1;
        }
    }
    if (VectorDistance3(defense.raiders[0].position,
                        defense.raiders[1].position) < 1.50f ||
        VectorDistance3(defense.raiders[0].target_point,
                        defense.raiders[1].target_point) < 1.35f) {
        (void)fprintf(stderr, "raider arrivals were bunched together\n");
        return 1;
    }
    bool guard_engaged[CC_LOCAL_COURSE_RUNNER_COUNT] = {false};
    bool guard_returned[CC_LOCAL_COURSE_RUNNER_COUNT] = {false};
    float minimum_raider_distance = 1000.0f;
    for (int32_t frame = 0; frame < 12000 &&
                            defense.defenses_completed == 0; ++frame) {
        CcLocalCourseUpdate(&defense, NULL, &defense_sim, 1.0f / 60.0f);
        for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
            guard_engaged[i] = guard_engaged[i] ||
                               defense.runners[i].duty == CC_GUARD_ENGAGED;
            guard_returned[i] = guard_returned[i] ||
                                defense.runners[i].duty == CC_GUARD_RETURNING;
        }
        for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
            float distance_x = defense.raiders[i].position.x -
                               defense.combat_origin.x;
            float distance_z = defense.raiders[i].position.z -
                               defense.combat_origin.z;
            minimum_raider_distance = fminf(
                minimum_raider_distance,
                sqrtf(distance_x * distance_x + distance_z * distance_z));
        }
    }
    if (defense.defenses_completed != 1 || defense.alarm_active ||
        minimum_raider_distance > 3.00f || defense.raider_resolve > 0) {
        (void)fprintf(stderr,
                      "village defense did not intercept and repel the raid: wins %d alarm %d min distance %.2f resolve %d guards %.2f,%.2f/%.0f/%.0f/%d %.2f,%.2f/%.0f/%.0f/%d %.2f,%.2f/%.0f/%.0f/%d raiders %.2f,%.2f,%.2f/%.0f/%.0f/%d/l%d/s%d/a%d/%s/v%.2f,%.2f/t%d@%.2f,%.2f/nav%d:%d/%d %.2f,%.2f,%.2f/%.0f/%.0f/%d/l%d/s%d/a%d/%s/v%.2f,%.2f/t%d@%.2f,%.2f/nav%d:%d/%d seen %d%d%d\n",
                      defense.defenses_completed, defense.alarm_active,
                      minimum_raider_distance, defense.raider_resolve,
                      defense.runners[0].agent.position.x,
                      defense.runners[0].agent.position.z,
                      defense.runners[0].agent.combat.health,
                      defense.runners[0].agent.combat.posture,
                      defense.runners[0].agent.humanoid.action,
                      defense.runners[1].agent.position.x,
                      defense.runners[1].agent.position.z,
                      defense.runners[1].agent.combat.health,
                      defense.runners[1].agent.combat.posture,
                      defense.runners[1].agent.humanoid.action,
                      defense.runners[2].agent.position.x,
                      defense.runners[2].agent.position.z,
                      defense.runners[2].agent.combat.health,
                      defense.runners[2].agent.combat.posture,
                      defense.runners[2].agent.humanoid.action,
                      defense.raiders[0].position.x,
                      defense.raiders[0].position.z,
                      defense.raiders[0].position.y,
                      defense.raiders[0].combat.health,
                      defense.raiders[0].combat.posture,
                      defense.raiders[0].humanoid.action,
                      defense.raiders[0].combat.life_state,
                      defense.raider_response_stage[0],
                      defense.raider_response_waypoint_active[0],
                      CcHumanoidSupportStateName(
                          defense.raiders[0].support_state),
                      defense.raiders[0].velocity.x,
                      defense.raiders[0].velocity.z,
                      defense.raiders[0].exact_target_valid,
                      defense.raiders[0].target_point.x,
                      defense.raiders[0].target_point.z,
                      defense.raiders[0].navigation_active,
                      defense.raiders[0].navigation_point_index,
                      defense.raiders[0].navigation_point_count,
                      defense.raiders[1].position.x,
                      defense.raiders[1].position.z,
                      defense.raiders[1].position.y,
                      defense.raiders[1].combat.health,
                      defense.raiders[1].combat.posture,
                      defense.raiders[1].humanoid.action,
                      defense.raiders[1].combat.life_state,
                      defense.raider_response_stage[1],
                      defense.raider_response_waypoint_active[1],
                      CcHumanoidSupportStateName(
                          defense.raiders[1].support_state),
                      defense.raiders[1].velocity.x,
                      defense.raiders[1].velocity.z,
                      defense.raiders[1].exact_target_valid,
                      defense.raiders[1].target_point.x,
                      defense.raiders[1].target_point.z,
                      defense.raiders[1].navigation_active,
                      defense.raiders[1].navigation_point_index,
                      defense.raiders[1].navigation_point_count,
                      guard_engaged[0], guard_engaged[1], guard_engaged[2]);
        return 1;
    }
    for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
        if (!guard_engaged[i] || !guard_returned[i]) {
            (void)fprintf(stderr,
                          "guard %d defense lifecycle failed: engaged %d returned %d stage %d duty %d\n",
                          i, guard_engaged[i], guard_returned[i],
                          defense.runners[i].response_stage,
                          defense.runners[i].duty);
            return 1;
        }
    }

    static const int32_t expected_counts[] = {2, 4, 6, 8};
    for (int32_t preset = 0; preset < CC_MORPHOLOGY_PRESET_COUNT; ++preset) {
        CcLocalAgentSetMorphology(&agent, (CcMorphologyPreset)preset, false);
        if (agent.limb_rig.morphology.limb_count != expected_counts[preset]) {
            (void)fprintf(stderr, "local agent rejected a supported morphology\n");
            return 1;
        }
    }
    return 0;
}
