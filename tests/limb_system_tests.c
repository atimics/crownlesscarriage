#include "locomotion/cc_limb.h"

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
