#include "client/cc_local3d.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

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
        agent.ragdoll_visual_blend > 0.001f ||
        agent.humanoid.ragdoll.active || !agent.grounded ||
        fabsf(agent.position.y) > 0.01f || street_error > 0.18f) {
        (void)fprintf(stderr,
                      "%s: fall failed ragdoll %d recovery %d impact %d fall %.3fs frames %d-%d rebound %.3f particle %.3f frame %d node %d at %.3f %.3f %.3f gap %d blend %d/%.3f/%.3f active %d grounded %d pos %.3f %.3f %.3f error %.3f\n",
                      name, saw_ragdoll, saw_recovery, street_impact,
                      fall_duration, ragdoll_start_frame, street_impact_frame,
                      maximum_rebound_speed, maximum_particle_rebound,
                      maximum_particle_frame, maximum_particle_index,
                      maximum_particle_position.x,
                      maximum_particle_position.y,
                      maximum_particle_position.z, maximum_contact_gap,
                      saw_visual_transition, maximum_visual_blend_step,
                      agent.ragdoll_visual_blend,
                      agent.humanoid.ragdoll.active, agent.grounded,
                      agent.position.x, agent.position.y, agent.position.z,
                      street_error);
        exit(1);
    }
}

int main(void)
{
    RequirePosition("market wall blocks entry",
                    CcLocalMove((Vector2){7.20f, 3.50f},
                                (Vector2){0.00f, -0.60f}, false),
                    (Vector2){7.20f, 3.50f});
    RequirePosition("collision slides along facade",
                    CcLocalMove((Vector2){6.20f, 3.50f},
                                (Vector2){1.00f, -0.60f}, false),
                    (Vector2){7.20f, 3.50f});
    RequirePosition("carriage blocks movement",
                    CcLocalMove((Vector2){2.60f, 6.55f},
                                (Vector2){-0.60f, 0.00f}, false),
                    (Vector2){2.60f, 6.55f});
    RequirePosition("market counter blocks movement",
                    CcLocalMove((Vector2){7.10f, 3.00f},
                                (Vector2){0.00f, -1.00f}, true),
                    (Vector2){7.10f, 3.00f});
    RequirePosition("open street permits movement",
                    CcLocalMove((Vector2){4.75f, 5.85f},
                                (Vector2){0.10f, -0.10f}, false),
                    (Vector2){4.85f, 5.75f});

    CcLocalAgent agent;
    CcLocalAgentInit(&agent, (Vector2){4.75f, 5.85f}, false);
    if (agent.morphology != CC_MORPHOLOGY_BIPED) {
        (void)fprintf(stderr,
                      "the playable local agent should default to the tuned biped\n");
        return 1;
    }
    CcLocalAgent fallen_edge_agent;
    CcLocalAgentInit(&fallen_edge_agent, (Vector2){3.50f, 6.70f}, false);
    CcHumanoidGaitAdvance(
        &fallen_edge_agent.humanoid,
        (CcLimbVec3){fallen_edge_agent.position.x,
                     fallen_edge_agent.position.y,
                     fallen_edge_agent.position.z},
        fallen_edge_agent.facing_yaw, (CcLimbVec3){0}, false,
        1.0f / 60.0f, NULL, NULL);
    if (!fallen_edge_agent.humanoid.ragdoll.active ||
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
    if (CcBiomechRigMeanActivation(&paced_agent.humanoid.body) <= 0.01f) {
        (void)fprintf(stderr, "human gait did not recruit its muscle pairs\n");
        return 1;
    }

    CcLocalAgent crowd_agent;
    CcLocalAgentInit(&crowd_agent, (Vector2){5.55f, 5.30f}, false);
    if (CcLocalAgentSetExactTarget(&crowd_agent,
                                   (Vector3){5.55f, 0.0f, 6.15f}, false)) {
        (void)fprintf(stderr, "a visible townsperson should occupy physical space\n");
        return 1;
    }
    if (!CcLocalAgentSetExactTarget(&crowd_agent,
                                    (Vector3){5.55f, 0.0f, 6.90f}, false)) {
        (void)fprintf(stderr, "a target beyond a townsperson should remain valid\n");
        return 1;
    }
    float closest_person_distance = 1000.0f;
    for (int32_t frame = 0; frame < 900; ++frame) {
        CcLocalAgentUpdate(&crowd_agent, 1.0f / 60.0f, false);
        float dx = crowd_agent.position.x - 5.55f;
        float dz = crowd_agent.position.z - 6.15f;
        closest_person_distance = fminf(closest_person_distance,
                                        sqrtf(dx * dx + dz * dz));
    }
    RequireNearPosition("agent sidesteps a townsperson",
                        CcLocalAgentPosition(&crowd_agent),
                        (Vector2){5.55f, 6.90f}, 0.12f);
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
    int32_t climb_frames = 0;
    bool saw_biomechanical_climb = false;
    float maximum_hand_reach = 0.0f;
    float maximum_hand_contact_error = 0.0f;
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
            saw_climb = true;
            climb_frames += 1;
            saw_biomechanical_climb = saw_biomechanical_climb ||
                (agent.humanoid.climbing && !agent.humanoid_needs_reset);
            for (int32_t limb = 0; limb < 2; ++limb) {
                const CcHumanoidPose *pose = &agent.humanoid.pose;
                const CcHumanoidFoot *foot = &agent.humanoid.feet[limb];
                if (fabsf(foot->normal.y) < 0.5f) {
                    saw_wall_foot = true;
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
                }
                if (agent.climb_progress >= 0.18f) {
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
                maximum_hand_contact_error = fmaxf(
                    maximum_hand_contact_error,
                    fmaxf(Distance3(pose->hand[0],
                                   (CcLimbVec3){agent.climb_hand_left.x,
                                                agent.climb_hand_left.y,
                                                agent.climb_hand_left.z}),
                          Distance3(pose->hand[1],
                                   (CcLimbVec3){agent.climb_hand_right.x,
                                                agent.climb_hand_right.y,
                                                agent.climb_hand_right.z})));
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
        !saw_wall_foot || !saw_top_foot || !saw_biomechanical_climb) {
        (void)fprintf(stderr,
                      "climb traversal incomplete: y %.2f max %.2f climb %d frames %d wall %d top %d biotech %d\n",
                      agent.position.y, maximum_height, saw_climb, climb_frames,
                      saw_wall_foot, saw_top_foot, saw_biomechanical_climb);
        return 1;
    }
    if (maximum_hand_reach > 0.69f || maximum_hand_contact_error > 0.025f ||
        maximum_foot_contact_error > 0.025f ||
        maximum_climb_pose_step > 0.085f || minimum_wall_clearance < 0.25f ||
        minimum_knee_wall_clearance < -0.001f) {
        (void)fprintf(stderr,
                      "climb contacts broke biotech constraints: reach %.3f hand %.3f feet %.3f at %.3f limb %d step %.3f at %.3f wall %.3f knee %.3f at %.3f\n",
                      maximum_hand_reach, maximum_hand_contact_error,
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
                      "down-climb target debug: active %d target %d settled %.3f time %.3f speed %.3f pos %.3f %.3f\n",
                      agent.humanoid.ragdoll.active,
                      agent.exact_target_valid,
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
        CcLocalCourseUpdate(&course, NULL, 1.0f / 60.0f);
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

    CcSim defense_sim;
    CcSimInit(&defense_sim, 42U);
    CcLocalCourse defense;
    CcLocalCourseInit(&defense);
    defense.alarm_countdown = 1000.0f;
    for (int32_t frame = 0; frame < 720; ++frame) {
        CcLocalCourseUpdate(&defense, &defense_sim, 1.0f / 60.0f);
    }
    CcLocalCourseRaiseAlarm(&defense);
    bool guard_engaged[CC_LOCAL_COURSE_RUNNER_COUNT] = {false};
    bool guard_returned[CC_LOCAL_COURSE_RUNNER_COUNT] = {false};
    float minimum_raider_x = 1000.0f;
    for (int32_t frame = 0; frame < 2400 &&
                            defense.defenses_completed == 0; ++frame) {
        CcLocalCourseUpdate(&defense, &defense_sim, 1.0f / 60.0f);
        for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
            guard_engaged[i] = guard_engaged[i] ||
                               defense.runners[i].duty == CC_GUARD_ENGAGED;
            guard_returned[i] = guard_returned[i] ||
                                defense.runners[i].duty == CC_GUARD_RETURNING;
        }
        for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
            minimum_raider_x = fminf(minimum_raider_x,
                                     defense.raiders[i].position.x);
        }
    }
    if (defense.defenses_completed != 1 || defense.alarm_active ||
        minimum_raider_x > 15.00f || defense.raider_resolve > 0) {
        (void)fprintf(stderr,
                      "village defense did not intercept and repel the raid: wins %d alarm %d min x %.2f resolve %d guards %.2f,%.2f %.2f,%.2f %.2f,%.2f seen %d%d%d\n",
                      defense.defenses_completed, defense.alarm_active,
                      minimum_raider_x, defense.raider_resolve,
                      defense.runners[0].agent.position.x,
                      defense.runners[0].agent.position.z,
                      defense.runners[1].agent.position.x,
                      defense.runners[1].agent.position.z,
                      defense.runners[2].agent.position.x,
                      defense.runners[2].agent.position.z,
                      guard_engaged[0], guard_engaged[1], guard_engaged[2]);
        return 1;
    }
    for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
        if (!guard_engaged[i] || !guard_returned[i]) {
            (void)fprintf(stderr,
                          "guard %d failed to engage and return from defense\n", i);
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
