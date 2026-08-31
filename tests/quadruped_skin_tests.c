#include "locomotion/cc_quadruped.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define EXPECT(condition, message) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL: %s\n", message); \
        failures += 1; \
    } \
} while (0)

static float Distance(CcLimbVec3 a, CcLimbVec3 b)
{
    float x = a.x - b.x;
    float y = a.y - b.y;
    float z = a.z - b.z;
    return sqrtf(x * x + y * y + z * z);
}

int main(void)
{
    int failures = 0;
    for (int32_t bone = 0; bone < CC_QUADRUPED_BONE_COUNT; ++bone) {
        const char *name = CcQuadrupedBoneName((CcQuadrupedBone)bone);
        EXPECT(strcmp(name, "invalid") != 0, "every quadruped bone is named");
        EXPECT(CcQuadrupedBoneFind(name) == bone,
               "quadruped bone names round trip");
        EXPECT(CcQuadrupedBoneParent((CcQuadrupedBone)bone) < bone,
               "quadruped parents precede their children");
    }

    CcQuadrupedPose horse_idle = {0};
    CcQuadrupedPose horse_step = {0};
    CcQuadrupedPose cow_idle = {0};
    CcQuadrupedPose sheep_idle = {0};
    CcQuadrupedPose repeated = {0};
    CcQuadrupedPose horse_swing = {0};
    CcQuadrupedPoseResolve(CC_QUADRUPED_HORSE, 0.0f, false, &horse_idle);
    CcQuadrupedPoseResolve(CC_QUADRUPED_HORSE, 0.5f, true, &horse_step);
    CcQuadrupedPoseResolve(CC_QUADRUPED_COW, 0.0f, false, &cow_idle);
    CcQuadrupedPoseResolve(CC_QUADRUPED_SHEEP, 0.0f, false, &sheep_idle);
    CcQuadrupedPoseResolve(CC_QUADRUPED_HORSE, 0.5f, true, &repeated);
    CcQuadrupedPoseResolve(CC_QUADRUPED_HORSE, 5.15f, true, &horse_swing);
    EXPECT(horse_idle.valid && horse_step.valid && cow_idle.valid &&
               sheep_idle.valid,
           "supported animal poses resolve");
    EXPECT(memcmp(&horse_step, &repeated, sizeof(horse_step)) == 0,
           "quadruped poses are deterministic");
    EXPECT(horse_idle.bones[CC_QUADRUPED_HEAD].head.y >
               cow_idle.bones[CC_QUADRUPED_HEAD].head.y,
           "pony and cow keep distinct head lines");
    EXPECT(cow_idle.bones[CC_QUADRUPED_HEAD].head.y >
               sheep_idle.bones[CC_QUADRUPED_HEAD].head.y,
           "sheep keeps its small low head line");
    EXPECT(sheep_idle.bones[CC_QUADRUPED_TAIL].tail.z >
               cow_idle.bones[CC_QUADRUPED_TAIL].tail.z,
           "sheep keeps a short readable tail");
    EXPECT(horse_step.bones[CC_QUADRUPED_HOOF_FL].head.z >
               horse_idle.bones[CC_QUADRUPED_HOOF_FL].head.z,
           "front-left hoof advances during its swing");
    EXPECT(horse_step.bones[CC_QUADRUPED_HOOF_FR].head.z <
               horse_idle.bones[CC_QUADRUPED_HOOF_FR].head.z,
           "paired hooves use the opposite phase");
    float highest_lift = 0.0f;
    const CcQuadrupedBone hooves[] = {
        CC_QUADRUPED_HOOF_FL, CC_QUADRUPED_HOOF_FR,
        CC_QUADRUPED_HOOF_HL, CC_QUADRUPED_HOOF_HR,
    };
    for (int32_t hoof = 0; hoof < 4; ++hoof) {
        float lift = horse_swing.bones[hooves[hoof]].head.y -
                     horse_idle.bones[hooves[hoof]].head.y;
        highest_lift = fmaxf(highest_lift, lift);
    }
    EXPECT(highest_lift > 0.10f,
           "a prancing pony lifts a swinging hoof high off the ground");

    CcQuadrupedPose pony_hold_a = {0};
    CcQuadrupedPose pony_hold_b = {0};
    CcQuadrupedPose pony_next_hold = {0};
    CcQuadrupedPoseResolve(CC_QUADRUPED_HORSE, 0.10f * 2.0f * 3.14159265f,
                           false, &pony_hold_a);
    CcQuadrupedPoseResolve(CC_QUADRUPED_HORSE, 0.11f * 2.0f * 3.14159265f,
                           false, &pony_hold_b);
    CcQuadrupedPoseResolve(CC_QUADRUPED_HORSE, 0.18f * 2.0f * 3.14159265f,
                           false, &pony_next_hold);
    EXPECT(Distance(pony_hold_a.bones[CC_QUADRUPED_TAIL].tail,
                    pony_hold_b.bones[CC_QUADRUPED_TAIL].tail) < 0.00001f,
           "secondary motion holds between anime-style pose steps");
    EXPECT(Distance(pony_hold_a.bones[CC_QUADRUPED_TAIL].tail,
                    pony_next_hold.bones[CC_QUADRUPED_TAIL].tail) > 0.01f,
           "pony tail follows through on the next held pose");
    for (int32_t bone = 0; bone < CC_QUADRUPED_BONE_COUNT; ++bone) {
        EXPECT(Distance(horse_step.bones[bone].head,
                        horse_step.bones[bone].tail) > 0.01f,
               "every resolved bone keeps a useful length");
    }
    CcQuadrupedPose invalid = {0};
    CcQuadrupedPoseResolve(CC_QUADRUPED_MORPHOLOGY_COUNT, 0.0f, true,
                           &invalid);
    EXPECT(!invalid.valid, "invalid animal morphologies are rejected");

    if (failures != 0) return 1;
    puts("quadruped skin contract passed");
    return 0;
}
