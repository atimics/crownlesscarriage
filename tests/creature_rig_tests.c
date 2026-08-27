#include "locomotion/cc_creature.h"

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
        Require(pose->biomech_joint_count == pose->limb_count * 2,
                "each creature limb has two driven joints");
        Require(pose->biomech_muscle_count == pose->limb_count * 4,
                "each creature joint has flexor and extensor muscles");
        Require(pose->mean_activation > 0.01f,
                "creature muscles activate under gait load");
        for (int32_t limb = 0; limb < pose->limb_count; ++limb) {
            float upper = Distance(pose->limbs[limb].joints[0],
                                   pose->limbs[limb].joints[1]);
            float lower = Distance(pose->limbs[limb].joints[1],
                                   pose->limbs[limb].joints[2]);
            Require(upper > 0.30f && lower > 0.30f,
                    "creature skeleton keeps useful limb lengths");
            Require(pose->limbs[limb].upper_activation >= 0.0f &&
                    pose->limbs[limb].lower_activation >= 0.0f,
                    "creature muscle envelopes have valid activation");
        }
    }

    Require(poses[CC_CREATURE_RIG_GOBLIN].limb_count == 2,
            "goblins use the biped skeleton");
    Require(poses[CC_CREATURE_RIG_HORSE].limb_count == 4 &&
            poses[CC_CREATURE_RIG_COW].limb_count == 4 &&
            poses[CC_CREATURE_RIG_DRAGON].limb_count == 4,
            "horse, cow, and dragon use the quadruped skeleton");
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

    puts("creature skeletal-muscular rig contract passed");
    return 0;
}
