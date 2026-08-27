#include "locomotion/cc_creature.h"
#include "locomotion/cc_robotics.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static void Require(bool condition, const char *message)
{
    if (condition) return;
    (void)fprintf(stderr, "%s\n", message);
    exit(1);
}

static float Distance(CcLimbVec3 a, CcLimbVec3 b)
{
    float x = b.x - a.x;
    float y = b.y - a.y;
    float z = b.z - a.z;
    return sqrtf(x * x + y * y + z * z);
}

int main(void)
{
    static const int32_t expected_limbs[CC_CREATURE_RIG_PROFILE_COUNT] = {
        2, 4, 4, 4, 6, 8,
    };
    CcCreatureRigPose poses[CC_CREATURE_RIG_PROFILE_COUNT];
    for (int32_t profile = 0; profile < CC_CREATURE_RIG_PROFILE_COUNT;
         ++profile) {
        Require(CcCreatureRigPoseResolve(
                    (CcCreatureRigProfile)profile, 0.82f, 1.0f,
                    (CcLimbVec3){2.0f, 0.0f, 4.0f}, 0.35f, 1.0f,
                    &poses[profile]),
                "every creature profile resolves a rig pose");
        const CcCreatureRigPose *pose = &poses[profile];
        Require(pose->valid, "resolved creature rig pose is valid");
        Require(pose->limb_count == expected_limbs[profile],
                "creature profile has the expected limb count");
        int32_t expected_joints = 0;
        for (int32_t limb = 0; limb < pose->limb_count; ++limb) {
            expected_joints += pose->limbs[limb].segment_count;
        }
        Require(pose->biomech_joint_count == expected_joints,
                "each creature segment has a driven joint");
        Require(pose->biomech_bone_count == expected_joints + 1,
                "each creature segment has a biomechanical bone");
        Require(pose->biomech_muscle_count == expected_joints * 2,
                "each creature joint has flexor and extensor muscles");
        Require(pose->mean_activation > 0.01f,
                "creature muscles activate under gait load");
        for (int32_t limb = 0; limb < pose->limb_count; ++limb) {
            const CcCreatureRigLimbPose *leg = &pose->limbs[limb];
            int32_t wanted_segments = profile == CC_CREATURE_RIG_OCTOPOD ?
                                      3 : 2;
            Require(leg->segment_count == wanted_segments,
                    "creature limb keeps its authored segment count");
            for (int32_t segment = 0; segment < leg->segment_count; ++segment) {
                float length = Distance(leg->joints[segment],
                                        leg->joints[segment + 1]);
                Require(length > 0.20f,
                        "creature skeleton keeps useful segment lengths");
                Require(leg->segment_activation[segment] >= 0.0f &&
                        leg->segment_activation[segment] <= 1.0f,
                        "creature muscle envelopes have valid activation");
            }
        }
    }

    Require(poses[CC_CREATURE_RIG_GOBLIN].limb_count == 2,
            "goblins use the biped skeleton");
    Require(poses[CC_CREATURE_RIG_HORSE].limb_count == 4 &&
            poses[CC_CREATURE_RIG_COW].limb_count == 4 &&
            poses[CC_CREATURE_RIG_DRAGON].limb_count == 4,
            "horse, cow, and dragon use the quadruped skeleton");
    Require(poses[CC_CREATURE_RIG_HEXAPOD].limb_count == 6,
            "hexapod rig exposes six independently driven legs");
    Require(poses[CC_CREATURE_RIG_OCTOPOD].limb_count == 8,
            "octopod rig exposes eight independently driven legs");
    Require(poses[CC_CREATURE_RIG_COW].body_width >
            poses[CC_CREATURE_RIG_HORSE].body_width,
            "cow skeleton keeps a broader barrel than the horse");
    Require(poses[CC_CREATURE_RIG_DRAGON].body_length >
            poses[CC_CREATURE_RIG_COW].body_length,
            "dragon skeleton keeps the longest body plan");

    CcCreatureRigPose idle;
    CcCreatureRigPose stride;
    Require(CcCreatureRigPoseResolve(
                CC_CREATURE_RIG_HORSE, 0.10f, 0.0f,
                (CcLimbVec3){0}, 0.0f, 1.0f, &idle),
            "idle horse pose resolves");
    Require(CcCreatureRigPoseResolve(
                CC_CREATURE_RIG_HORSE, 0.82f, 1.0f,
                (CcLimbVec3){0}, 0.0f, 1.0f, &stride),
            "moving horse pose resolves");
    float largest_change = 0.0f;
    for (int32_t limb = 0; limb < idle.limb_count; ++limb) {
        largest_change = fmaxf(
            largest_change,
            Distance(idle.limbs[limb].joints[2],
                     stride.limbs[limb].joints[2]));
    }
    Require(largest_change > 0.08f,
            "movement phase changes skeletal contact positions");

    static const CcCreatureRigProfile controller_profiles[] = {
        CC_CREATURE_RIG_GOBLIN,
        CC_CREATURE_RIG_HORSE,
        CC_CREATURE_RIG_COW,
        CC_CREATURE_RIG_DRAGON,
        CC_CREATURE_RIG_HEXAPOD,
        CC_CREATURE_RIG_OCTOPOD,
    };
    for (size_t profile_index = 0;
         profile_index < sizeof(controller_profiles) /
                             sizeof(controller_profiles[0]);
         ++profile_index) {
        CcCreatureRigController controller;
        CcCreatureRigProfile profile = controller_profiles[profile_index];
        Require(CcCreatureRigControllerInit(&controller, profile, 0.13f, 1.0f),
                "persistent creature controller initializes");
        CcRobotCollisionPoint collision_points[CC_ROBOT_POINT_CAPACITY];
        int32_t collision_point_count = CcRobotLimbPointSpace(
            &controller.skeleton, 0.14f, collision_points,
            CC_ROBOT_POINT_CAPACITY);
        Require(collision_point_count >=
                    controller.skeleton.morphology.limb_count * 4,
                "creature bones produce a useful collision point space");
        for (int32_t point = 0; point < collision_point_count; ++point) {
            Require(isfinite(collision_points[point].center.x) &&
                    isfinite(collision_points[point].center.y) &&
                    isfinite(collision_points[point].center.z) &&
                    fabsf(collision_points[point].radius - 0.14f) < 0.0001f,
                    "creature collision point space remains finite");
        }
        bool saw_swing = false;
        int32_t locked_stance_frames = 0;
        for (int32_t frame = 0; frame < 360; ++frame) {
            CcLimbState previous_state[CC_CREATURE_RIG_MAX_LIMBS];
            CcLimbVec3 previous_contact[CC_CREATURE_RIG_MAX_LIMBS];
            for (int32_t limb = 0;
                 limb < controller.skeleton.morphology.limb_count; ++limb) {
                previous_state[limb] = controller.skeleton.limbs[limb].state;
                previous_contact[limb] =
                    controller.skeleton.limbs[limb].planted_contact;
            }
            CcCreatureRigPose controlled;
            Require(CcCreatureRigControllerStep(
                        &controller, 0.82f, 1.0f, 1.0f / 60.0f, &controlled),
                    "persistent creature controller advances");
            Require(controlled.valid,
                    "persistent creature controller returns a valid pose");
            Require(controlled.planted_count >=
                        controller.skeleton.morphology.minimum_supports,
                    "persistent gait keeps its support budget");
            Require(controlled.swinging_count <=
                        controller.skeleton.morphology.maximum_swings,
                    "persistent gait keeps its swing budget");
            for (int32_t limb = 0; limb < controlled.limb_count; ++limb) {
                CcLimbState state = controller.skeleton.limbs[limb].state;
                if (state == CC_LIMB_SWING) saw_swing = true;
                if (previous_state[limb] == CC_LIMB_STANCE &&
                    state == CC_LIMB_STANCE) {
                    Require(Distance(
                                previous_contact[limb],
                                controller.skeleton.limbs[limb].planted_contact) <
                                0.00001f,
                            "stance contact remains locked in world space");
                    locked_stance_frames += 1;
                }
            }
        }
        Require(saw_swing, "persistent gait replants at least one foot");
        Require(locked_stance_frames > 60,
                "persistent gait spends useful time on locked contacts");
    }

    puts("creature skeletal-muscular rig contract passed");
    return 0;
}
