#include "locomotion/cc_limb.h"
#include "locomotion/cc_multileg.h"
#include "locomotion/cc_robotics.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static bool PlaneProbe(void *context, CcLimbVec3 origin, float maximum_drop,
                       CcLimbVec3 *point, CcLimbVec3 *normal)
{
    (void)context;
    if (origin.y < 0.0f || origin.y > maximum_drop) return false;
    *point = (CcLimbVec3){origin.x, 0.0f, origin.z};
    *normal = (CcLimbVec3){0.0f, 1.0f, 0.0f};
    return true;
}

static bool SlopeProbe(void *context, CcLimbVec3 origin, float maximum_drop,
                       CcLimbVec3 *point, CcLimbVec3 *normal)
{
    (void)context;
    float height = origin.z * 0.30f;
    if (origin.y < height || origin.y - height > maximum_drop) return false;
    *point = (CcLimbVec3){origin.x, height, origin.z};
    float inverse_length = 1.0f / sqrtf(1.0f + 0.30f * 0.30f);
    *normal = (CcLimbVec3){0.0f, inverse_length, -0.30f * inverse_length};
    return true;
}

static bool PlaneCollision(void *context,
                           CcBiomechVec3 previous_position,
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

static float Distance(CcLimbVec3 a, CcLimbVec3 b)
{
    float x = b.x - a.x;
    float y = b.y - a.y;
    float z = b.z - a.z;
    return sqrtf(x * x + y * y + z * z);
}

static CcLimbVec3 Subtract(CcLimbVec3 a, CcLimbVec3 b)
{
    return (CcLimbVec3){a.x - b.x, a.y - b.y, a.z - b.z};
}

static float Dot(CcLimbVec3 a, CcLimbVec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static void Require(bool condition, const char *message)
{
    if (condition) return;
    (void)fprintf(stderr, "%s\n", message);
    exit(1);
}

static void VerifySegments(const CcLimbRig *rig)
{
    for (int32_t limb = 0; limb < rig->morphology.limb_count; ++limb) {
        const CcLimbSpec *spec = &rig->morphology.limbs[limb];
        const CcLimbRuntime *runtime = &rig->limbs[limb];
        for (int32_t segment = 0; segment < spec->segment_count; ++segment) {
            float actual = Distance(runtime->joints[segment],
                                    runtime->joints[segment + 1]);
            Require(fabsf(actual - spec->segment_length[segment]) < 0.012f,
                    "FABRIK segment length drifted");
        }
    }
}

int main(void)
{
    CcLimbMorphology invalid;
    (void)CcLimbMorphologyFromPreset(&invalid, CC_MORPHOLOGY_BIPED);
    CcLimbRig invalid_rig;
    invalid.limb_count = CC_LIMB_MAX_COUNT + 1;
    CcLimbRigInit(&invalid_rig, &invalid, (CcLimbVec3){0}, 0.0f,
                  PlaneProbe, NULL);
    Require(!invalid_rig.initialized,
            "limb rig accepted a morphology larger than its fixed storage");

    (void)CcLimbMorphologyFromPreset(&invalid, CC_MORPHOLOGY_BIPED);
    invalid.limb_count = 0;
    CcLimbRigInit(&invalid_rig, &invalid, (CcLimbVec3){0}, 0.0f,
                  PlaneProbe, NULL);
    Require(!invalid_rig.initialized,
            "limb rig accepted an empty morphology");

    (void)CcLimbMorphologyFromPreset(&invalid, CC_MORPHOLOGY_BIPED);
    invalid.limbs[0].segment_count = 0;
    CcLimbRigInit(&invalid_rig, &invalid, (CcLimbVec3){0}, 0.0f,
                  PlaneProbe, NULL);
    Require(!invalid_rig.initialized,
            "limb rig accepted a zero-segment chain");

    (void)CcLimbMorphologyFromPreset(&invalid, CC_MORPHOLOGY_BIPED);
    invalid.limbs[0].segment_count = CC_LIMB_MAX_SEGMENTS + 1;
    CcLimbRigInit(&invalid_rig, &invalid, (CcLimbVec3){0}, 0.0f,
                  PlaneProbe, NULL);
    Require(!invalid_rig.initialized,
            "limb rig accepted a chain larger than its joint storage");

    static const int32_t expected_counts[] = {2, 4, 6, 8};
    for (int32_t preset = 0; preset < CC_MORPHOLOGY_PRESET_COUNT; ++preset) {
        CcLimbMorphology morphology;
        Require(CcLimbMorphologyFromPreset(&morphology,
                                            (CcMorphologyPreset)preset),
                "morphology preset should exist");
        Require(morphology.limb_count == expected_counts[preset],
                "morphology preset has the wrong limb count");
        CcLimbRig rig;
        CcLimbVec3 body = {0.0f, morphology.body_height, 0.0f};
        CcLimbRigInit(&rig, &morphology, body, 0.0f, PlaneProbe, NULL);
        Require(rig.initialized, "limb rig did not initialize");
        CcLimbRigUpdate(&rig, body, 0.0f, (CcLimbVec3){0}, true,
                        1.0f / 60.0f, PlaneProbe, NULL);
        if (morphology.limb_count >= 4) {
            Require(rig.support_margin > 0.20f,
                    "centered multi-leg morphology should begin inside its support hull");
        }
        bool saw_swing = false;
        for (int32_t frame = 0; frame < 360; ++frame) {
            CcLimbVec3 velocity = {0.72f, 0.0f, 0.19f};
            body.x += velocity.x / 60.0f;
            body.z += velocity.z / 60.0f;
            CcLimbRigUpdate(&rig, body, 0.22f, velocity, true, 1.0f / 60.0f,
                            PlaneProbe, NULL);
            if (rig.swinging_count > 0) saw_swing = true;
            Require(rig.swinging_count <= morphology.maximum_swings,
                    "gait exceeded its simultaneous swing budget");
            Require(rig.planted_count >= morphology.minimum_supports,
                    "gait dropped below its required support count");
            VerifySegments(&rig);
        }
        Require(saw_swing, "moving morphology never replanted a foot");
        Require(isfinite(rig.support_margin), "support margin was not finite");
    }

    static const int32_t run_supports[] = {1, 2, 3, 6};
    static const int32_t run_swings[] = {1, 2, 3, 2};
    static const int32_t sprint_supports[] = {1, 1, 3, 4};
    static const int32_t sprint_swings[] = {1, 3, 3, 4};
    for (int32_t preset = 0; preset < CC_MORPHOLOGY_PRESET_COUNT; ++preset) {
        CcLimbMorphology morphology;
        (void)CcLimbMorphologyFromPreset(
            &morphology, (CcMorphologyPreset)preset);
        CcLimbRig rig;
        CcLimbVec3 body = {0.0f, morphology.body_height, 0.0f};
        CcLimbRigInit(&rig, &morphology, body, 0.0f, PlaneProbe, NULL);
        CcLimbRigUpdate(&rig, body, 0.0f, (CcLimbVec3){0}, true,
                        1.0f / 60.0f, PlaneProbe, NULL);
        Require(CcLimbRigRequestPace(&rig, CC_LIMB_PACE_RUN) &&
                    rig.pace == CC_LIMB_PACE_RUN &&
                    rig.morphology.minimum_supports == run_supports[preset] &&
                    rig.morphology.maximum_swings == run_swings[preset],
                "run pace did not configure the body-plan support policy");
        Require(CcLimbRigRequestPace(&rig, CC_LIMB_PACE_SPRINT) &&
                    rig.pace == CC_LIMB_PACE_SPRINT &&
                    rig.morphology.minimum_supports ==
                        sprint_supports[preset] &&
                    rig.morphology.maximum_swings == sprint_swings[preset],
                "sprint pace did not configure the body-plan support policy");
        Require(CcLimbPaceName(rig.pace)[0] == 'S',
                "limb pace has no stable player-facing name");
    }

    for (int32_t preset = CC_MORPHOLOGY_QUADRUPED;
         preset <= CC_MORPHOLOGY_OCTOPOD; ++preset) {
        CcLimbMorphology morphology;
        (void)CcLimbMorphologyFromPreset(
            &morphology, (CcMorphologyPreset)preset);
        CcLimbRig rig;
        CcLimbVec3 center = {0.0f, morphology.body_height, 0.0f};
        CcLimbRigInit(&rig, &morphology, center, 0.0f,
                      PlaneProbe, NULL);
        CcMultilegRagdoll ragdoll;
        Require(CcMultilegRagdollCollapse(
                    &ragdoll, &rig, center, 0.0f,
                    (CcLimbVec3){0.8f, 0.0f, 0.2f},
                    (CcLimbVec3){1.0f, 0.2f, 0.0f}, center,
                    5.0f, true),
                "multi-leg body rejected a physical collapse");
        Require(ragdoll.active && ragdoll.physics.active &&
                    ragdoll.control_authority == 0.0f,
                "multi-leg collapse did not transfer body authority");
        if (preset == CC_MORPHOLOGY_OCTOPOD) {
            Require(ragdoll.physics.particle_count > 32,
                    "octopod ragdoll did not include every articulated joint");
        }
        bool saw_contact = false;
        bool saw_recovery = false;
        for (int32_t frame = 0; frame < 720 && ragdoll.active; ++frame) {
            (void)CcMultilegRagdollStep(
                &ragdoll, &rig, 1.0f / 60.0f,
                PlaneProbe, PlaneCollision, NULL);
            saw_contact = saw_contact ||
                CcMultilegRagdollSupportContactCount(&ragdoll) > 0;
            saw_recovery = saw_recovery || ragdoll.recovering;
            VerifySegments(&rig);
        }
        Require(saw_contact,
                "multi-leg ragdoll never collided with the physical floor");
        Require(saw_recovery && !ragdoll.active && rig.initialized &&
                    rig.support_state == CC_LIMB_SUPPORT_STABLE,
                "multi-leg ragdoll did not recover into its gait rig");
        Require(CcMultilegRagdollStateName(&ragdoll)[0] == 'C',
                "recovered multi-leg body kept the wrong authority name");
    }

    CcLimbMorphology slope_quadruped;
    (void)CcLimbMorphologyFromPreset(
        &slope_quadruped, CC_MORPHOLOGY_QUADRUPED);
    CcLimbRig supported_creature;
    CcLimbVec3 support_body = {0.0f, slope_quadruped.body_height, 0.0f};
    CcLimbRigInit(&supported_creature, &slope_quadruped, support_body,
                  0.0f, SlopeProbe, NULL);
    CcLimbRigUpdate(&supported_creature, support_body, 0.0f,
                    (CcLimbVec3){0}, true, 1.0f / 60.0f,
                    SlopeProbe, NULL);
    Require(supported_creature.support_state == CC_LIMB_SUPPORT_STABLE &&
                supported_creature.control_authority > 0.99f,
            "grounded creature did not begin with stable support authority");
    Require(supported_creature.support_normal.y > 0.94f &&
                supported_creature.support_normal.z < -0.25f,
            "creature support frame did not retain the terrain normal");
    for (int32_t frame = 0; frame < 6; ++frame) {
        CcLimbRigUpdate(&supported_creature, support_body, 0.0f,
                        (CcLimbVec3){0}, false, 1.0f / 60.0f,
                        SlopeProbe, NULL);
    }
    Require(supported_creature.support_state ==
                CC_LIMB_SUPPORT_CONTROLLED_AIRBORNE &&
                supported_creature.control_authority > 0.0f,
            "brief support loss skipped controlled airborne authority");
    for (int32_t frame = 0; frame < 12; ++frame) {
        CcLimbRigUpdate(&supported_creature, support_body, 0.0f,
                        (CcLimbVec3){0}, false, 1.0f / 60.0f,
                        SlopeProbe, NULL);
    }
    Require(supported_creature.support_state ==
                CC_LIMB_SUPPORT_UNSUPPORTED,
            "sustained support loss did not become unsupported");
    bool saw_recovery = false;
    for (int32_t frame = 0; frame < 120; ++frame) {
        CcLimbRigUpdate(&supported_creature, support_body, 0.0f,
                        (CcLimbVec3){0}, true, 1.0f / 60.0f,
                        SlopeProbe, NULL);
        saw_recovery = saw_recovery || supported_creature.support_state ==
            CC_LIMB_SUPPORT_RECOVERING;
    }
    Require(saw_recovery && supported_creature.support_state ==
                CC_LIMB_SUPPORT_STABLE &&
                supported_creature.control_authority > 0.95f,
            "creature did not rebuild support authority after landing");
    Require(CcLimbSupportStateName(CC_LIMB_SUPPORT_RECOVERING)[0] == 'R',
            "support state has no stable player-facing name");

    CcLimbMorphology biped;
    (void)CcLimbMorphologyFromPreset(&biped, CC_MORPHOLOGY_BIPED);
    Require(biped.dynamic_balance, "biped should use dynamic lateral balance");
    Require(biped.limbs[0].bend_local.z > 0.0f &&
            biped.limbs[1].bend_local.z > 0.0f,
            "both biped knees should have an explicit forward pole");
    CcLimbRig walking_biped;
    CcLimbVec3 biped_body = {0.0f, biped.body_height, 0.0f};
    CcLimbRigInit(&walking_biped, &biped, biped_body, 0.0f, PlaneProbe, NULL);
    bool swung[2] = {false, false};
    CcLimbVec3 previous_knees[2] = {
        walking_biped.limbs[0].joints[1], walking_biped.limbs[1].joints[1]
    };
    for (int32_t frame = 0; frame < 420; ++frame) {
        CcLimbVec3 velocity = {0.0f, 0.0f, 0.78f};
        biped_body.z += velocity.z / 60.0f;
        CcLimbRigUpdate(&walking_biped, biped_body, 0.0f, velocity, true,
                        1.0f / 60.0f, PlaneProbe, NULL);
        Require(walking_biped.swinging_count <= 1,
                "biped tried to lift both feet at once");
        Require(fabsf(walking_biped.body_acceleration.z) < 0.0001f,
                "biped balance feedback should not tug along its travel axis");
        if (walking_biped.active_pose_limb >= 0) {
            int32_t active = walking_biped.active_pose_limb;
            float expected = active == 0 ?
                             walking_biped.limbs[active].swing_progress * 0.5f :
                             fmodf(0.5f +
                                   walking_biped.limbs[active].swing_progress * 0.5f,
                                   1.0f);
            Require(fabsf(walking_biped.pose_phase - expected) < 0.0001f,
                    "biped pose clock drifted away from its active swing foot");
        }
        for (int32_t limb = 0; limb < 2; ++limb) {
            const CcLimbRuntime *runtime = &walking_biped.limbs[limb];
            if (runtime->state == CC_LIMB_SWING) swung[limb] = true;
            Require(Distance(previous_knees[limb], runtime->joints[1]) < 0.16f,
                    "biped knee snapped between frames");
            CcLimbVec3 hip_to_foot = Subtract(runtime->joints[2],
                                              runtime->joints[0]);
            float distance_squared = Dot(hip_to_foot, hip_to_foot);
            if (distance_squared > 0.0001f) {
                CcLimbVec3 hip_to_knee = Subtract(runtime->joints[1],
                                                  runtime->joints[0]);
                float along = Dot(hip_to_knee, hip_to_foot) / distance_squared;
                CcLimbVec3 on_axis = {
                    runtime->joints[0].x + hip_to_foot.x * along,
                    runtime->joints[0].y + hip_to_foot.y * along,
                    runtime->joints[0].z + hip_to_foot.z * along
                };
                CcLimbVec3 bend = Subtract(runtime->joints[1], on_axis);
                Require(bend.z > -0.002f,
                        "biped knee flipped behind its pole plane");
            }
            previous_knees[limb] = runtime->joints[1];
        }
        VerifySegments(&walking_biped);
    }
    Require(swung[0] && swung[1], "biped did not alternate both feet");

    CcRobotCollisionPoint point_space[CC_ROBOT_POINT_CAPACITY];
    CcRobotCollisionPoint link_points[16];
    int32_t link_point_count = CcRobotSampleLink(
        (CcLimbVec3){0.0f, 0.0f, 0.0f},
        (CcLimbVec3){1.0f, 0.0f, 0.0f}, 0.10f,
        link_points, (int32_t)(sizeof(link_points) / sizeof(link_points[0])));
    Require(link_point_count >= 6,
            "a one-metre link did not receive enough sphere samples");
    for (int32_t point = 1; point < link_point_count; ++point) {
        Require(Distance(link_points[point - 1].center,
                         link_points[point].center) <= 0.20f,
                "point-space spheres left a gap along a link");
    }
    int32_t point_count = CcRobotLimbPointSpace(
        &walking_biped, 0.09f, point_space, CC_ROBOT_POINT_CAPACITY);
    Require(point_count >= 12,
            "articulated links did not produce an overlapping point space");
    for (int32_t point = 0; point < point_count; ++point) {
        Require(fabsf(point_space[point].radius - 0.09f) < 0.0001f,
                "point-space proxy lost its link radius");
    }

    CcLimbVec3 first_correction = {0};
    CcLimbVec3 second_correction = {0};
    Require(CcRobotPredictiveAvoidance(
                (CcLimbVec3){-0.70f, 0.0f, 0.0f},
                (CcLimbVec3){1.00f, 0.0f, 0.0f},
                (CcLimbVec3){0.70f, 0.0f, 0.0f},
                (CcLimbVec3){-1.00f, 0.0f, 0.0f},
                0.80f, 1.00f, 0,
                &first_correction, &second_correction),
            "head-on robots did not predict their closest approach");
    Require(fabsf(first_correction.z) > 0.01f &&
            fabsf(first_correction.x + second_correction.x) < 0.0001f &&
            fabsf(first_correction.z + second_correction.z) < 0.0001f,
            "predictive avoidance was not stable and reciprocal");
    Require(!CcRobotPredictiveAvoidance(
                (CcLimbVec3){-0.70f, 0.0f, 0.0f},
                (CcLimbVec3){-1.00f, 0.0f, 0.0f},
                (CcLimbVec3){0.70f, 0.0f, 0.0f},
                (CcLimbVec3){1.00f, 0.0f, 0.0f},
                0.80f, 1.00f, 0,
                &first_correction, &second_correction),
            "separating robots received an unnecessary avoidance command");
    Require(CcRobotTraversabilityCost(1.0f, 0.25f, 0.82f) >
                CcRobotTraversabilityCost(1.0f, 0.01f, 0.99f),
            "terrain cost did not prefer the more traversable edge");

    CcLimbVec3 wall_contact = {biped_body.x - 0.17f, biped_body.y - 0.71f,
                               biped_body.z + 0.30f};
    CcLimbRigPinContact(&walking_biped, 0, biped_body, 0.0f, wall_contact,
                        (CcLimbVec3){0.0f, 0.0f, -1.0f});
    Require(walking_biped.limbs[0].state == CC_LIMB_STANCE,
            "explicit wall contact should put the limb into stance");
    Require(Distance(walking_biped.limbs[0].joints[2], wall_contact) < 0.001f,
            "solved foot did not coincide with its explicit wall contact");
    Require(walking_biped.limbs[0].contact_normal.z < -0.99f,
            "wall contact normal was not retained");
    VerifySegments(&walking_biped);

    CcLimbRig paced_biped;
    biped_body = (CcLimbVec3){0.0f, biped.body_height, 0.0f};
    CcLimbRigInit(&paced_biped, &biped, biped_body, 0.0f, PlaneProbe, NULL);
    paced_biped.morphology.step_threshold = 10.0f;
    biped_body.z = 0.50f;
    CcLimbRigUpdate(&paced_biped, biped_body, 0.0f, (CcLimbVec3){0}, true,
                    1.0f / 60.0f, PlaneProbe, NULL);
    Require(paced_biped.drive_scale < 0.55f,
            "an extended planted biped leg should slow the body motor");
    Require(paced_biped.drive_scale >= 0.12f,
            "biped gait governor should remain bounded");

    CcLimbMorphology octopod;
    (void)CcLimbMorphologyFromPreset(&octopod, CC_MORPHOLOGY_OCTOPOD);
    Require(octopod.limbs[0].segment_count == 3,
            "octopod should prove arbitrary multi-segment chains");
    CcLimbRig damaged;
    CcLimbVec3 body = {0.0f, octopod.body_height, 0.0f};
    CcLimbRigInit(&damaged, &octopod, body, 0.0f, PlaneProbe, NULL);
    CcLimbRigSetHealth(&damaged, 2, 0.0f);
    CcLimbRigUpdate(&damaged, body, 0.0f, (CcLimbVec3){0}, true,
                    1.0f / 60.0f, PlaneProbe, NULL);
    Require(damaged.limbs[2].state == CC_LIMB_DISABLED,
            "destroyed limb should leave the support graph");
    Require(damaged.planted_count == 7,
            "damaged octopod should retain its seven healthy supports");
    Require(damaged.traction < 1.0f,
            "limb loss should feed back into available traction");
    CcLimbRig healthy_octopod;
    CcLimbRigInit(&healthy_octopod, &octopod, body, 0.0f, PlaneProbe, NULL);
    int32_t healthy_point_count = CcRobotLimbPointSpace(
        &healthy_octopod, 0.09f, point_space, CC_ROBOT_POINT_CAPACITY);
    int32_t damaged_point_count = CcRobotLimbPointSpace(
        &damaged, 0.09f, point_space, CC_ROBOT_POINT_CAPACITY);
    Require(damaged_point_count < healthy_point_count,
            "destroyed limb remained in the collision point space");

    CcLimbMorphology quadruped;
    (void)CcLimbMorphologyFromPreset(&quadruped, CC_MORPHOLOGY_QUADRUPED);
    CcLimbRig support_feedback;
    body = (CcLimbVec3){0.0f, quadruped.body_height, 0.0f};
    CcLimbRigInit(&support_feedback, &quadruped, body, 0.0f, PlaneProbe, NULL);
    support_feedback.morphology.step_threshold = 10.0f;
    body.x = 0.85f;
    CcLimbRigUpdate(&support_feedback, body, 0.0f, (CcLimbVec3){0}, true,
                    1.0f / 60.0f, PlaneProbe, NULL);
    Require(support_feedback.support_margin < 0.0f,
            "body outside planted contacts should have a negative support margin");
    Require(support_feedback.body_acceleration.x < 0.0f,
            "planted contacts should push the body back toward support");
    return 0;
}
