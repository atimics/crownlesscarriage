#include "locomotion/cc_biomech.h"
#include "locomotion/cc_humanoid.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static void Require(bool condition, const char *message)
{
    if (condition) return;
    (void)fprintf(stderr, "%s\n", message);
    exit(1);
}

static bool PlaneProbe(void *context, CcLimbVec3 origin, float maximum_drop,
                       CcLimbVec3 *point, CcLimbVec3 *normal)
{
    (void)context;
    if (origin.y < 0.0f || origin.y > maximum_drop) return false;
    *point = (CcLimbVec3){origin.x, 0.0f, origin.z};
    *normal = (CcLimbVec3){0.0f, 1.0f, 0.0f};
    return true;
}

static float Distance(CcLimbVec3 a, CcLimbVec3 b)
{
    float x = b.x - a.x;
    float y = b.y - a.y;
    float z = b.z - a.z;
    return sqrtf(x * x + y * y + z * z);
}

static float MaximumPosePointStep(const CcHumanoidPose *before,
                                  const CcHumanoidPose *after)
{
    float maximum = Distance(before->pelvis, after->pelvis);
#define INCLUDE_POINT(point) maximum = fmaxf(maximum, Distance(before->point, \
                                                               after->point))
    INCLUDE_POINT(spine);
    INCLUDE_POINT(chest);
    INCLUDE_POINT(neck);
    INCLUDE_POINT(head);
    for (int32_t leg = 0; leg < CC_HUMANOID_LEG_COUNT; ++leg) {
        maximum = fmaxf(maximum, Distance(before->hip[leg], after->hip[leg]));
        maximum = fmaxf(maximum, Distance(before->knee[leg], after->knee[leg]));
        maximum = fmaxf(maximum, Distance(before->ankle[leg], after->ankle[leg]));
        maximum = fmaxf(maximum, Distance(before->heel[leg], after->heel[leg]));
        maximum = fmaxf(maximum, Distance(before->ball[leg], after->ball[leg]));
        maximum = fmaxf(maximum, Distance(before->toe[leg], after->toe[leg]));
    }
    for (int32_t arm = 0; arm < CC_HUMANOID_ARM_COUNT; ++arm) {
        maximum = fmaxf(maximum,
                        Distance(before->shoulder[arm], after->shoulder[arm]));
        maximum = fmaxf(maximum,
                        Distance(before->elbow[arm], after->elbow[arm]));
        maximum = fmaxf(maximum,
                        Distance(before->hand[arm], after->hand[arm]));
    }
#undef INCLUDE_POINT
    return maximum;
}

static const char *MaximumPosePointName(const CcHumanoidPose *before,
                                        const CcHumanoidPose *after)
{
    const char *name = "pelvis";
    float maximum = Distance(before->pelvis, after->pelvis);
#define CHECK_NAMED_POINT(point) do {                                      \
    float step = Distance(before->point, after->point);                    \
    if (step > maximum) { maximum = step; name = #point; }                 \
} while (0)
    CHECK_NAMED_POINT(spine);
    CHECK_NAMED_POINT(chest);
    CHECK_NAMED_POINT(neck);
    CHECK_NAMED_POINT(head);
    for (int32_t leg = 0; leg < CC_HUMANOID_LEG_COUNT; ++leg) {
        float steps[] = {
            Distance(before->hip[leg], after->hip[leg]),
            Distance(before->knee[leg], after->knee[leg]),
            Distance(before->ankle[leg], after->ankle[leg]),
            Distance(before->heel[leg], after->heel[leg]),
            Distance(before->ball[leg], after->ball[leg]),
            Distance(before->toe[leg], after->toe[leg])
        };
        const char *names[] = {"hip", "knee", "ankle", "heel", "ball", "toe"};
        for (int32_t point = 0; point < 6; ++point) {
            if (steps[point] > maximum) {
                maximum = steps[point];
                name = names[point];
            }
        }
    }
    for (int32_t arm = 0; arm < CC_HUMANOID_ARM_COUNT; ++arm) {
        float shoulder = Distance(before->shoulder[arm], after->shoulder[arm]);
        float elbow = Distance(before->elbow[arm], after->elbow[arm]);
        float hand = Distance(before->hand[arm], after->hand[arm]);
        if (shoulder > maximum) { maximum = shoulder; name = "shoulder"; }
        if (elbow > maximum) { maximum = elbow; name = "elbow"; }
        if (hand > maximum) { maximum = hand; name = "hand"; }
    }
#undef CHECK_NAMED_POINT
    return name;
}

static float BiomechDistance(CcBiomechVec3 a, CcBiomechVec3 b)
{
    float x = b.x - a.x;
    float y = b.y - a.y;
    float z = b.z - a.z;
    return sqrtf(x * x + y * y + z * z);
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

static bool RagdollPlane(void *context, CcBiomechVec3 previous_position,
                         CcBiomechVec3 position, float radius,
                         CcBiomechVec3 *corrected_position,
                         CcBiomechVec3 *surface_normal)
{
    (void)context;
    (void)previous_position;
    if (position.y - radius >= 0.0f) return false;
    *corrected_position = position;
    corrected_position->y = radius;
    *surface_normal = (CcBiomechVec3){0.0f, 1.0f, 0.0f};
    return true;
}

static void TestGenericTissues(void)
{
    CcBiomechMorphology morphology;
    CcBiomechMorphologyInit(&morphology);
    int32_t trunk = CcBiomechAddBone(&morphology, "trunk", -1,
                                     0.50f, 12.0f, 0.50f);
    int32_t limb = CcBiomechAddBone(&morphology, "limb", trunk,
                                    0.70f, 4.0f, 0.44f);
    Require(trunk == 0 && limb == 1, "generic bone graph rejected valid bones");
    int32_t joint = CcBiomechAddJoint(&morphology, "hinge", trunk, limb,
                                      0.0f, -0.50f, 0.90f, 0.10f,
                                      2.8f, 0.75f, 35.0f);
    Require(joint == 0, "generic joint graph rejected a valid hinge");
    Require(CcBiomechAddMuscle(&morphology, "flexor", joint, 0.04f,
                               900.0f, 0.20f, 0.80f, 18.0f, 8.0f) == 0,
            "generic morphology rejected its flexor");
    Require(CcBiomechAddMuscle(&morphology, "extensor", joint, -0.04f,
                               900.0f, 0.20f, 0.80f, 18.0f, 8.0f) == 1,
            "generic morphology rejected its extensor");

    CcBiomechRig rig;
    Require(CcBiomechRigInit(&rig, &morphology),
            "generic biomechanical rig did not initialize");
    Require(fabsf(rig.total_mass - 16.0f) < 0.001f,
            "bone masses did not aggregate");

    CcBiomechRigSetBodyState(&rig, (CcBiomechVec3){0.0f, 2.0f, 0.0f},
                             (CcBiomechVec3){0});
    for (int32_t frame = 0; frame < 60; ++frame) {
        CcBiomechRigStepBody(&rig, 1.0f / 60.0f);
    }
    Require(rig.root.position.y < -2.0f && rig.root.velocity.y < -8.0f,
            "unconstrained body did not fall under gravity");
    CcBiomechRigSetBodyState(&rig, (CcBiomechVec3){0.0f, 2.0f, 0.0f},
                             (CcBiomechVec3){0});
    for (int32_t frame = 0; frame < 120; ++frame) {
        CcBiomechRigApplyBodyForce(&rig,
            (CcBiomechVec3){0.0f, rig.total_mass * 9.81f, 0.0f});
        CcBiomechRigStepBody(&rig, 1.0f / 60.0f);
    }
    Require(fabsf(rig.root.position.y - 2.0f) < 0.001f,
            "balanced external force did not cancel gravity");
    CcBiomechRigConstrainBody(&rig, (CcBiomechVec3){0.0f, 1.0f, 0.0f},
                              (CcBiomechVec3){0.0f, 0.0f, 0.0f});
    Require(fabsf(rig.root.position.y - 1.0f) < 0.0001f,
            "whole-body contact did not constrain position");
    for (int32_t frame = 0; frame < 240; ++frame) {
        CcBiomechRigDriveJoint(&rig, joint, 0.72f, 0.85f);
        CcBiomechRigStep(&rig, 1.0f / 120.0f);
    }
    Require(rig.joints[joint].angle > 0.30f,
            "antagonistic muscles did not flex the joint");
    Require(rig.muscles[0].activation > rig.muscles[1].activation,
            "stretch reflex recruited the wrong muscle");

    for (int32_t frame = 0; frame < 180; ++frame) {
        CcBiomechRigApplyTorque(&rig, joint, 600.0f);
        CcBiomechRigStep(&rig, 1.0f / 120.0f);
    }
    Require(rig.joints[joint].angle <= morphology.joints[joint].upper_limit +
            0.0001f, "ligaments and hard anatomy allowed joint hyperextension");

    CcBiomechRigConstrainJoint(&rig, joint, 0.15f, 1.0f / 60.0f);
    Require(fabsf(rig.joints[joint].angle - 0.15f) < 0.0001f,
            "contact constraint did not correct the articulated joint");
    Require(fabsf(rig.joints[joint].contact_reaction_torque) > 0.01f,
            "contact correction did not produce a reaction torque");
}

static void TestGenericRagdoll(void)
{
    CcBiomechRagdoll ragdoll;
    CcBiomechRagdollInit(&ragdoll);
    int32_t hip = CcBiomechRagdollAddParticle(
        &ragdoll, (CcBiomechVec3){0.0f, 2.0f, 0.0f}, 0.10f, 0.12f);
    int32_t knee = CcBiomechRagdollAddParticle(
        &ragdoll, (CcBiomechVec3){0.0f, 1.5f, 0.0f}, 0.20f, 0.09f);
    int32_t ankle = CcBiomechRagdollAddParticle(
        &ragdoll, (CcBiomechVec3){0.0f, 1.0f, 0.0f}, 0.35f, 0.08f);
    Require(hip == 0 && knee == 1 && ankle == 2,
            "generic ragdoll rejected valid particles");
    Require(CcBiomechRagdollAddConstraint(&ragdoll, hip, knee, 0.0f) == 0 &&
            CcBiomechRagdollAddConstraint(&ragdoll, knee, ankle, 0.0f) == 1,
            "generic ragdoll rejected valid bone constraints");
    ragdoll.active = true;
    CcBiomechRagdollSetVelocity(&ragdoll,
                                (CcBiomechVec3){0.65f, 0.0f, 0.0f},
                                1.0f / 60.0f);
    float initial_height = ragdoll.particles[hip].position.y;
    bool collided = false;
    int32_t first_collision_frame = -1;
    bool impact_began = false;
    float maximum_rebound_speed = 0.0f;
    for (int32_t frame = 0; frame < 240; ++frame) {
        CcBiomechRagdollStep(&ragdoll, 1.0f / 60.0f, 16,
                             RagdollPlane, NULL);
        bool frame_collision = ragdoll.particles[ankle].collided ||
                               ragdoll.particles[knee].collided ||
                               ragdoll.particles[hip].collided;
        if (frame_collision && first_collision_frame < 0) {
            first_collision_frame = frame;
        }
        collided = collided || frame_collision;
        impact_began = impact_began || frame_collision;
        if (impact_began) {
            maximum_rebound_speed = fmaxf(
                maximum_rebound_speed,
                RagdollCenterVelocityY(&ragdoll, 1.0f / 60.0f));
        }
        float upper_length = BiomechDistance(ragdoll.particles[hip].position,
                                              ragdoll.particles[knee].position);
        Require(fabsf(upper_length - 0.50f) < 0.006f,
                "ragdoll upper bone stretched under gravity");
        float lower_length = BiomechDistance(
            ragdoll.particles[knee].position,
            ragdoll.particles[ankle].position);
        Require(fabsf(lower_length - 0.50f) < 0.006f,
                "ragdoll lower bone stretched on impact");
    }
    Require(ragdoll.particles[hip].position.y < initial_height - 0.50f,
            "ragdoll did not fall under gravity");
    Require(collided, "ragdoll never collided with terrain");
    Require(first_collision_frame >= 18 && first_collision_frame <= 38,
            "air drag made the ragdoll fall outside its gravity timing band");
    Require(maximum_rebound_speed < 0.35f,
            "contact correction launched the ragdoll back into the air");
    for (int32_t particle = 0; particle < ragdoll.particle_count; ++particle) {
        Require(ragdoll.particles[particle].position.y + 0.0001f >=
                ragdoll.particles[particle].radius,
                "ragdoll particle penetrated the ground");
    }
}

static void TestBiomechanicalClimb(void)
{
    const float delta_time = 1.0f / 60.0f;
    CcHumanoidGait gait;
    CcLimbVec3 start = {0.0f, 0.0f, 0.0f};
    CcLimbVec3 finish = {0.0f, 1.0f, 0.50f};
    CcHumanoidGaitInit(&gait, start, 0.0f, NULL, NULL);
    CcHumanoidGaitBeginClimb(&gait);
    Require(gait.climbing && !gait.ragdoll.active,
            "supported biped did not enter biomechanical climbing");

    CcHumanoidPose prior_pose = gait.pose;
    float maximum_pose_step = 0.0f;
    for (int32_t frame = 0; frame < 150; ++frame) {
        float progress = fminf(0.78f, (float)(frame + 1) / 120.0f * 0.78f);
        float motion = progress / 0.78f;
        motion = motion * motion * (3.0f - 2.0f * motion);
        CcLimbVec3 body = {
            start.x + (finish.x - start.x) * motion,
            start.y + (finish.y - start.y) * motion,
            start.z + (finish.z - start.z) * motion
        };
        float transfer = fmaxf(0.0f, fminf(1.0f,
            (progress - 0.30f) / 0.40f));
        transfer = transfer * transfer * (3.0f - 2.0f * transfer);
        CcLimbVec3 hands[2] = {
            {-0.18f, 1.10f, 0.34f}, {0.18f, 1.10f, 0.34f}
        };
        CcLimbVec3 feet[2] = {
            {-0.13f, body.y + 0.10f, 0.24f},
            {0.13f, body.y + 0.17f, 0.24f}
        };
        CcLimbVec3 top_feet[2] = {
            {-0.13f, 1.0f, 0.50f}, {0.13f, 1.0f, 0.50f}
        };
        CcLimbVec3 normals[2];
        const float support[2] = {1.0f, 1.0f};
        for (int32_t leg = 0; leg < 2; ++leg) {
            feet[leg] = (CcLimbVec3){
                feet[leg].x + (top_feet[leg].x - feet[leg].x) * transfer,
                feet[leg].y + (top_feet[leg].y - feet[leg].y) * transfer,
                feet[leg].z + (top_feet[leg].z - feet[leg].z) * transfer
            };
            CcLimbVec3 normal = {0.0f, transfer, -(1.0f - transfer)};
            float normal_length = Distance((CcLimbVec3){0}, normal);
            normals[leg] = (CcLimbVec3){normal.x / normal_length,
                                        normal.y / normal_length,
                                        normal.z / normal_length};
        }
        CcHumanoidGaitAdvanceClimb(
            &gait, body, 0.0f, hands, feet, normals, support,
            progress, delta_time, NULL, NULL);
        maximum_pose_step = fmaxf(
            maximum_pose_step,
            MaximumPosePointStep(&prior_pose, &gait.pose));
        prior_pose = gait.pose;
        for (int32_t leg = 0; leg < 2; ++leg) {
            Require(fabsf(Distance(gait.pose.hip[leg], gait.pose.knee[leg]) -
                          0.465f) < 0.004f &&
                    fabsf(Distance(gait.pose.knee[leg], gait.pose.ankle[leg]) -
                          0.475f) < 0.004f,
                    "biomechanical climb changed a leg bone length");
            Require(fabsf(Distance(gait.pose.shoulder[leg],
                                   gait.pose.elbow[leg]) - 0.34f) < 0.004f &&
                    fabsf(Distance(gait.pose.elbow[leg],
                                   gait.pose.hand[leg]) - 0.35f) < 0.004f,
                    "biomechanical climb changed an arm bone length");
        }
    }
    for (int32_t frame = 0; frame < 240 &&
         !CcHumanoidGaitClimbReady(&gait, finish, 0.0f,
                                   NULL, NULL, 0.025f); ++frame) {
        float settle = fminf(1.0f, (float)(frame + 1) / 60.0f);
        settle = settle * settle * (3.0f - 2.0f * settle);
        float progress = 0.78f + 0.22f * settle;
        CcLimbVec3 hands[2] = {
            {-0.18f, 1.10f, 0.34f}, {0.18f, 1.10f, 0.34f}
        };
        CcLimbVec3 feet[2] = {
            {-0.13f, 1.0f, 0.50f}, {0.13f, 1.0f, 0.50f}
        };
        CcLimbVec3 normals[2] = {
            {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}
        };
        const float support[2] = {1.0f, 1.0f};
        CcHumanoidGaitAdvanceClimb(
            &gait, finish, 0.0f, hands, feet, normals, support,
            progress, delta_time, NULL, NULL);
        maximum_pose_step = fmaxf(
            maximum_pose_step,
            MaximumPosePointStep(&prior_pose, &gait.pose));
        prior_pose = gait.pose;
    }
    Require(CcHumanoidGaitClimbReady(&gait, finish, 0.0f,
                                     NULL, NULL, 0.025f),
            "biomechanical climb did not converge to standing");
    if (maximum_pose_step >= 0.085f) {
        (void)fprintf(stderr, "biomechanical climb maximum step %.4f\n",
                      maximum_pose_step);
    }
    Require(maximum_pose_step < 0.085f,
            "biomechanical climb snapped a body landmark");
    CcHumanoidPose settled_pose = gait.pose;
    CcHumanoidGaitFinishClimb(&gait, finish, 0.0f, NULL, NULL);
    Require(MaximumPosePointStep(&settled_pose, &gait.pose) <= 0.0251f,
            "biomechanical climb snapped when standing control resumed");
    Require(!gait.climbing && !gait.ragdoll.active,
            "biomechanical climb did not return supported control");
}

static void TestHumanoidController(void)
{
    CcHumanoidGait gait;
    CcLimbVec3 body = {0.0f, 0.0f, 0.0f};
    CcHumanoidGaitInit(&gait, body, 0.0f, PlaneProbe, NULL);
    Require(gait.initialized && gait.body.initialized,
            "humanoid did not instantiate its biomechanical body");
    Require(gait.body.morphology.joint_count == CC_HUMANOID_JOINT_COUNT,
            "humanoid joint map is incomplete");
    Require(gait.body.morphology.bone_count >= 13 &&
            gait.body.morphology.muscle_count == CC_HUMANOID_JOINT_COUNT * 2,
            "humanoid anatomy lacks bones or antagonistic muscle pairs");

    uint32_t left_contacts = 0;
    uint32_t right_contacts = 0;
    float maximum_knee_flexion = 0.0f;
    float maximum_arm_angle = 0.0f;
    float minimum_pelvis_height = 1000.0f;
    float maximum_pelvis_height = -1000.0f;
    float first_frame_speed = 0.0f;
    float maximum_ground_reaction = 0.0f;
    for (int32_t frame = 0; frame < 600; ++frame) {
        CcHumanoidGaitAdvance(&gait, body, 0.0f,
                              (CcLimbVec3){0.0f, 0.0f, 1.20f}, true,
                              1.0f / 60.0f, PlaneProbe, NULL);
        body.x += gait.root_velocity.x / 60.0f;
        body.z += gait.root_velocity.z / 60.0f;
        float speed = sqrtf(gait.root_velocity.x * gait.root_velocity.x +
                            gait.root_velocity.z * gait.root_velocity.z);
        if (frame == 0) first_frame_speed = speed;
        minimum_pelvis_height = fminf(minimum_pelvis_height,
                                      gait.body.root.position.y);
        maximum_pelvis_height = fmaxf(maximum_pelvis_height,
                                      gait.body.root.position.y);
        maximum_ground_reaction = fmaxf(maximum_ground_reaction,
                                        gait.ground_reaction.y);
        for (int32_t leg = 0; leg < CC_HUMANOID_LEG_COUNT; ++leg) {
            const CcHumanoidFoot *foot = &gait.feet[leg];
            if (leg == 0) left_contacts |= UINT32_C(1) << foot->contact;
            else right_contacts |= UINT32_C(1) << foot->contact;
            if (foot->contact != CC_HUMANOID_CONTACT_SWING &&
                foot->contact != CC_HUMANOID_CONTACT_AIR) {
                Require(Distance(foot->current_point, foot->planted_point) <
                        0.0001f, "stance contact slid across the ground");
            }
            Require(fabsf(Distance(gait.pose.hip[leg], gait.pose.knee[leg]) -
                          0.465f) < 0.003f,
                    "humanoid thigh violated its bone length");
            float shin_length = Distance(gait.pose.knee[leg],
                                         gait.pose.ankle[leg]);
            if (fabsf(shin_length - 0.475f) >= 0.003f) {
                (void)fprintf(stderr,
                    "humanoid shin violated its bone length: frame %d leg %d contact %s length %.3f phase %.3f root %.3f foot %.3f\n",
                    frame, leg, CcHumanoidContactName(foot->contact), shin_length,
                    gait.phase, body.z, foot->current_point.z);
                exit(1);
            }
            maximum_knee_flexion = fmaxf(maximum_knee_flexion,
                                         gait.pose.knee_flexion[leg]);
        }
        maximum_arm_angle = fmaxf(maximum_arm_angle, fabsf(
            CcBiomechRigJointAngle(&gait.body, CC_HUMANOID_LEFT_SHOULDER)));
    }
    uint32_t grounded_contacts = (UINT32_C(1) << CC_HUMANOID_CONTACT_HEEL) |
                                 (UINT32_C(1) << CC_HUMANOID_CONTACT_FLAT) |
                                 (UINT32_C(1) << CC_HUMANOID_CONTACT_TOE) |
                                 (UINT32_C(1) << CC_HUMANOID_CONTACT_SWING);
    Require((left_contacts & grounded_contacts) == grounded_contacts &&
            (right_contacts & grounded_contacts) == grounded_contacts,
            "both legs did not traverse heel, flat, toe, and swing phases");
    Require(maximum_knee_flexion > 0.35f && maximum_knee_flexion < 2.56f,
            "swing knee did not flex within anatomical limits");
    Require(maximum_arm_angle > 0.05f,
            "muscle-driven arm counter-swing never emerged");
    Require(CcBiomechRigMeanActivation(&gait.body) > 0.01f,
            "humanoid gait did not recruit muscles");

    Require(first_frame_speed > 0.0f && first_frame_speed < 0.15f,
            "navigation intent assigned body velocity instead of applying force");
    Require(minimum_pelvis_height > 0.68f && maximum_pelvis_height < 1.08f,
            "force-supported center of mass left its anatomical height band");
    Require(maximum_ground_reaction > gait.body.total_mass * 9.81f * 0.80f,
            "planted feet did not generate a body-supporting reaction force");

    float stop_start = body.z;
    for (int32_t frame = 0; frame < 180; ++frame) {
        CcHumanoidGaitAdvance(&gait, body, 0.0f, (CcLimbVec3){0}, true,
                              1.0f / 60.0f, PlaneProbe, NULL);
        body.x += gait.root_velocity.x / 60.0f;
        body.z += gait.root_velocity.z / 60.0f;
    }
    float stopped_speed = sqrtf(gait.root_velocity.x * gait.root_velocity.x +
                                gait.root_velocity.z * gait.root_velocity.z);
    Require(stopped_speed < 0.035f && body.z - stop_start < 0.65f,
            "ground friction did not bring the body to a bounded stop");
    Require(fabsf(gait.body.joints[CC_HUMANOID_LEFT_SHOULDER].angular_velocity) <
                0.12f &&
            fabsf(gait.body.joints[CC_HUMANOID_RIGHT_SHOULDER].angular_velocity) <
                0.12f,
            "idle arms continued swinging without locomotion");

    float airborne_velocity = gait.body.root.velocity.y;
    float standing_head_height = gait.pose.head.y;
    float thigh_length = Distance(gait.pose.hip[0], gait.pose.knee[0]);
    float upper_arm_length = Distance(gait.pose.shoulder[0], gait.pose.elbow[0]);
    CcHumanoidPose supported_pose = gait.pose;
    CcHumanoidGaitAdvance(&gait, body, 0.0f, (CcLimbVec3){0}, false,
                          1.0f / 60.0f, PlaneProbe, NULL);
    Require(gait.ragdoll.active && gait.ragdoll.particle_count == 21,
            "unsupported humanoid did not hand control to its ragdoll");
    Require(gait.feet[0].contact == CC_HUMANOID_CONTACT_AIR &&
            gait.feet[1].contact == CC_HUMANOID_CONTACT_AIR,
            "airborne body retained fictional ground contacts");
    Require(gait.body.root.velocity.y < airborne_velocity,
            "airborne center of mass did not accelerate under gravity");
    Require(MaximumPosePointStep(&supported_pose, &gait.pose) < 0.035f,
            "ragdoll activation snapped a body landmark");
    float minimum_head_height = gait.pose.head.y;
    bool touched_ground = false;
    bool humanoid_impact_began = false;
    float maximum_humanoid_rebound = 0.0f;
    for (int32_t frame = 0; frame < 90; ++frame) {
        CcHumanoidGaitAdvance(&gait, body, 0.0f, (CcLimbVec3){0}, false,
                              1.0f / 60.0f, PlaneProbe, NULL);
        minimum_head_height = fminf(minimum_head_height, gait.pose.head.y);
        bool frame_collision = false;
        for (int32_t particle = 0;
             particle < gait.ragdoll.particle_count; ++particle) {
            frame_collision = frame_collision ||
                              gait.ragdoll.particles[particle].collided;
        }
        touched_ground = touched_ground || frame_collision;
        humanoid_impact_began = humanoid_impact_began || frame_collision;
        if (humanoid_impact_began) {
            maximum_humanoid_rebound = fmaxf(
                maximum_humanoid_rebound,
                RagdollCenterVelocityY(&gait.ragdoll, 1.0f / 60.0f));
        }
        Require(fabsf(Distance(gait.pose.hip[0], gait.pose.knee[0]) -
                      thigh_length) < 0.008f,
                "falling humanoid stretched its thigh");
        Require(fabsf(Distance(gait.pose.shoulder[0], gait.pose.elbow[0]) -
                      upper_arm_length) < 0.008f,
                "falling humanoid stretched its upper arm");
    }
    Require(minimum_head_height < standing_head_height - 0.35f && touched_ground,
            "humanoid did not physically collapse onto the terrain");
    Require(maximum_humanoid_rebound < 0.45f,
            "humanoid bounced after terrain impact");
    bool saw_recovery = false;
    int32_t recovery_frames = 0;
    float maximum_recovery_pose_step = 0.0f;
    float maximum_recovery_pose_step_time = 0.0f;
    CcHumanoidPose prior_pose = gait.pose;
    for (int32_t frame = 0; frame < 360 && gait.ragdoll.active; ++frame) {
        bool recovery_was_active = gait.recovering;
        CcHumanoidGaitAdvance(&gait, body, 0.0f, (CcLimbVec3){0}, true,
                              1.0f / 60.0f, PlaneProbe, NULL);
        if (gait.recovering) {
            saw_recovery = true;
            recovery_frames += 1;
        }
        if (recovery_was_active || gait.recovery_time > 0.0f) {
            float pose_step = MaximumPosePointStep(&prior_pose, &gait.pose);
            if (pose_step > maximum_recovery_pose_step) {
                maximum_recovery_pose_step = pose_step;
                maximum_recovery_pose_step_time = gait.recovery_time;
            }
        }
        prior_pose = gait.pose;
    }
    if (gait.ragdoll.active) {
        (void)fprintf(stderr,
                      "settled humanoid remained trapped: recovery %.3fs error %.4f speed %.4f\n",
                      gait.recovery_time, gait.recovery_error,
                      gait.recovery_speed);
        exit(1);
    }
    Require(saw_recovery && recovery_frames >= 150,
            "humanoid skipped its brace, kneel, or stand recovery stage");
    if (maximum_recovery_pose_step >= 0.060f) {
        (void)fprintf(stderr,
                      "maximum recovery landmark step: %.4f at %.3fs\n",
                      maximum_recovery_pose_step,
                      maximum_recovery_pose_step_time);
    }
    Require(maximum_recovery_pose_step < 0.060f,
            "humanoid snapped between ragdoll and standing poses");
    Require(gait.pose.head.y > gait.pose.pelvis.y + 0.75f,
            "recovered humanoid did not finish upright");
}

static void TestContinuousHumanActions(void)
{
    const float delta_time = 1.0f / 60.0f;
    CcLimbVec3 body = {0.0f, 0.0f, 0.0f};
    CcHumanoidGait gait;
    CcHumanoidGaitInit(&gait, body, 0.0f, PlaneProbe, NULL);
    CcHumanoidGaitSetGuarded(&gait, true);
    for (int32_t frame = 0; frame < 45; ++frame) {
        CcHumanoidGaitAdvance(&gait, body, 0.0f, (CcLimbVec3){0}, true,
                              delta_time, PlaneProbe, NULL);
    }
    Require(gait.action == CC_HUMANOID_ACTION_GUARD,
            "supported human did not settle into a guarded stance");
    Require(gait.guard_requested &&
            gait.pose.hand[0].x < gait.pose.chest.x - 0.07f &&
            gait.pose.hand[1].x > gait.pose.chest.x + 0.06f &&
            gait.pose.hand[0].z > gait.pose.chest.z + 0.16f &&
            gait.pose.hand[1].z > gait.pose.chest.z + 0.18f &&
            Distance(gait.pose.shoulder[0], gait.pose.hand[0]) < 0.52f &&
            Distance(gait.pose.shoulder[1], gait.pose.hand[1]) < 0.55f,
            "guard arms remained parallel, straight, or detached from the torso");

    Require(CcHumanoidGaitBeginStrike(&gait, 1),
            "guarded human rejected a valid right-arm strike");
    int32_t impact_count = 0;
    float maximum_strike_step = 0.0f;
    int32_t maximum_strike_step_frame = -1;
    const char *maximum_strike_step_point = "none";
    CcHumanoidPose previous = gait.pose;
    for (int32_t frame = 0; frame < 90; ++frame) {
        CcHumanoidGaitAdvance(&gait, body, 0.0f, (CcLimbVec3){0}, true,
                              delta_time, PlaneProbe, NULL);
        impact_count += CcHumanoidGaitConsumeStrikeImpact(&gait) ? 1 : 0;
        float pose_step = MaximumPosePointStep(&previous, &gait.pose);
        if (pose_step > maximum_strike_step) {
            maximum_strike_step = pose_step;
            maximum_strike_step_frame = frame;
            maximum_strike_step_point = MaximumPosePointName(
                &previous, &gait.pose);
        }
        previous = gait.pose;
        Require(fabsf(Distance(gait.pose.shoulder[1], gait.pose.elbow[1]) -
                      0.34f) < 0.002f,
                "strike stretched the upper arm");
        Require(fabsf(Distance(gait.pose.elbow[1], gait.pose.hand[1]) -
                      0.35f) < 0.002f,
                "strike stretched the forearm");
    }
    Require(impact_count == 1,
            "a strike did not emit exactly one physical impact window");
    Require(gait.action == CC_HUMANOID_ACTION_GUARD,
            "strike did not recover into guard");
    if (maximum_strike_step >= 0.055f) {
        (void)fprintf(stderr,
                      "maximum strike landmark step %.4f at frame %d (%s)\n",
                      maximum_strike_step, maximum_strike_step_frame,
                      maximum_strike_step_point);
    }
    Require(maximum_strike_step < 0.055f,
            "guard/strike/recovery transition snapped a body landmark");

    CcHumanoidGait released_gait;
    CcHumanoidGaitInit(&released_gait, body, 0.0f, PlaneProbe, NULL);
    CcHumanoidGaitSetGuarded(&released_gait, true);
    Require(CcHumanoidGaitBeginStrike(&released_gait, 1),
            "unguarded strike fixture was rejected");
    CcHumanoidGaitSetGuarded(&released_gait, false);
    for (int32_t frame = 0; frame < 90; ++frame) {
        CcHumanoidGaitAdvance(&released_gait, body, 0.0f,
                              (CcLimbVec3){0}, true,
                              delta_time, PlaneProbe, NULL);
    }
    Require(!released_gait.guard_requested &&
            released_gait.action == CC_HUMANOID_ACTION_LOCOMOTION,
            "completed strike resurrected a guard request that was released");

    float minimum_pelvis_height = 1000.0f;
    float maximum_swim_step = 0.0f;
    previous = gait.pose;
    for (int32_t frame = 0; frame < 180; ++frame) {
        CcHumanoidGaitAdvanceSwim(
            &gait, body, 0.0f, (CcLimbVec3){0.0f, 0.0f, 0.55f},
            0.82f, 1.0f, delta_time);
        body.x += gait.root_velocity.x * delta_time;
        body.z += gait.root_velocity.z * delta_time;
        minimum_pelvis_height = fminf(minimum_pelvis_height,
                                      gait.pose.pelvis.y);
        maximum_swim_step = fmaxf(
            maximum_swim_step,
            MaximumPosePointStep(&previous, &gait.pose));
        previous = gait.pose;
        Require(gait.planted_count == 0 &&
                gait.feet[0].contact == CC_HUMANOID_CONTACT_AIR &&
                gait.feet[1].contact == CC_HUMANOID_CONTACT_AIR,
                "swimmer invented ground contacts under water");
        Require(fabsf(Distance(gait.pose.hip[0], gait.pose.knee[0]) -
                      0.465f) < 0.006f,
                "swim kick stretched the thigh");
    }
    Require(gait.action == CC_HUMANOID_ACTION_SWIM &&
            minimum_pelvis_height > 0.62f,
            "buoyancy failed to support the swimming body");
    if (maximum_swim_step >= 0.075f) {
        (void)fprintf(stderr, "maximum swim landmark step %.4f\n",
                      maximum_swim_step);
    }
    Require(maximum_swim_step < 0.075f,
            "guard-to-swim transition snapped a body landmark");

    CcHumanoidGaitEndSwim(&gait, body, 0.0f, PlaneProbe, NULL);
    for (int32_t frame = 0; frame < 60; ++frame) {
        CcHumanoidGaitAdvance(&gait, body, 0.0f, (CcLimbVec3){0}, true,
                              delta_time, PlaneProbe, NULL);
    }
    Require(gait.action == CC_HUMANOID_ACTION_LOCOMOTION &&
            gait.planted_count > 0,
            "swimmer did not reacquire terrestrial support");
}

int main(void)
{
    TestGenericTissues();
    TestGenericRagdoll();
    TestBiomechanicalClimb();
    TestHumanoidController();
    TestContinuousHumanActions();
    return 0;
}
