#include "locomotion/cc_character_skin.h"
#include "locomotion/cc_humanoid_skin.h"
#include "locomotion/cc_quadruped.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

#define EXPECT(condition, message) do { \
    if (!(condition)) { \
        (void)fprintf(stderr, "FAIL: %s\n", message); \
        failures += 1; \
    } \
} while (0)

static float Distance(CcLimbVec3 a, CcLimbVec3 b)
{
    float x = b.x - a.x;
    float y = b.y - a.y;
    float z = b.z - a.z;
    return sqrtf(x * x + y * y + z * z);
}

static bool Resolve(CcBipedBodyKind kind, float phase, float movement,
                    CcLimbVec3 origin, float yaw, float scale,
                    CcHumanoidPose *pose)
{
    CcCreatureRigPose rig;
    if (!CcCreatureRigPoseResolve(CC_CREATURE_RIG_GOBLIN, phase, movement,
                                  origin, yaw, scale, &rig)) {
        return false;
    }
    return CcHumanoidPoseFromBipedRig(&rig, CcBipedBodyPlanFor(kind), origin,
                                      yaw, scale, pose);
}

static void PrintPoint(const char *name, CcLimbVec3 point)
{
    (void)printf("    \"%s\": (%.4f, %.4f, %.4f),\n", name,
                 (double)point.x, (double)point.y, (double)point.z);
}

/* The Blender builder authors the bind pose from these numbers. Print them
   in the builder's own form (model space, glTF axes: +y up, +z forward). */
static void PrintBindPose(void)
{
    for (int32_t kind = 0; kind < CC_BIPED_BODY_KIND_COUNT; ++kind) {
        CcHumanoidPose pose;
        if (!Resolve((CcBipedBodyKind)kind, 0.0f, 0.0f,
                     (CcLimbVec3){0}, 0.0f, 1.0f, &pose)) {
            (void)fprintf(stderr, "idle pose failed\n");
            exit(1);
        }
        (void)printf("%s = {\n", kind == CC_BIPED_BODY_GOBLIN ?
                     "GOBLIN_BIND" : "GOBLIN_CARRIER_BIND");
        PrintPoint("pelvis", pose.pelvis);
        PrintPoint("spine", pose.spine);
        PrintPoint("chest", pose.chest);
        PrintPoint("neck", pose.neck);
        PrintPoint("head", pose.head);
        const char *sides[2] = {"L", "R"};
        for (int32_t side = 0; side < 2; ++side) {
            char name[32];
            const char *labels[] = {"hip", "knee", "ankle", "toe",
                                    "shoulder", "elbow", "hand"};
            const CcLimbVec3 points[] = {
                pose.hip[side], pose.knee[side], pose.ankle[side],
                pose.toe[side], pose.shoulder[side], pose.elbow[side],
                pose.hand[side],
            };
            for (int32_t index = 0; index < 7; ++index) {
                (void)snprintf(name, sizeof(name), "%s.%s", labels[index],
                               sides[side]);
                PrintPoint(name, points[index]);
            }
        }
        (void)printf("}\n");
    }
}

static void TestFamilies(void)
{
    EXPECT(CcSkeletonFamilyBoneCount(CC_SKELETON_HUMANOID) == 18,
           "humanoid family carries the 18 hero bones");
    EXPECT(CcSkeletonFamilyBoneCount(CC_SKELETON_QUADRUPED) ==
               CC_QUADRUPED_BONE_COUNT,
           "quadruped family carries the quadruped bones");
    EXPECT(CcSkeletonFamilyBoneCount(CC_SKELETON_DRAGON) == 0,
           "dragon family is designed but not built yet");
    for (int32_t family = CC_SKELETON_HUMANOID;
         family < CC_SKELETON_FAMILY_COUNT; ++family) {
        EXPECT(CcSkeletonFamilyBoneCount((CcSkeletonFamily)family) <=
                   CC_SKINNED_CHARACTER_MAX_BONES,
               "every family fits the 32 shader bone matrices");
        EXPECT(CcSkeletonFamilyFind(
                   CcSkeletonFamilyName((CcSkeletonFamily)family)) ==
                   (CcSkeletonFamily)family,
               "family names round trip");
        for (int32_t bone = 0;
             bone < CcSkeletonFamilyBoneCount((CcSkeletonFamily)family);
             ++bone) {
            EXPECT(CcSkeletonFamilyBoneFind(
                       (CcSkeletonFamily)family,
                       CcSkeletonFamilyBoneName((CcSkeletonFamily)family,
                                                bone)) == bone,
                   "family bone names round trip");
        }
    }
    EXPECT(CcSkeletonFamilyBoneFind(CC_SKELETON_HUMANOID, "hoof.FL") < 0,
           "a humanoid does not accept quadruped bones");
    EXPECT(CcSkeletonFamilyFind("pig") == CC_SKELETON_NONE,
           "unknown families are rejected");
}

static void TestIdle(void)
{
    for (int32_t kind = 0; kind < CC_BIPED_BODY_KIND_COUNT; ++kind) {
        CcHumanoidPose pose;
        EXPECT(Resolve((CcBipedBodyKind)kind, 0.0f, 0.0f, (CcLimbVec3){0},
                       0.0f, 1.0f, &pose),
               "idle goblin pose resolves");
        EXPECT(fabsf(pose.pelvis.y - 0.78f) < 0.02f,
               "goblin pelvis stands at the rig body height");
        EXPECT(pose.hip[0].x < 0.0f && pose.hip[1].x > 0.0f,
               "limb 0 is the .L (-x) leg");
        EXPECT(pose.hand[0].x < 0.0f && pose.hand[1].x > 0.0f,
               "arm 0 is the .L (-x) arm");
        EXPECT(pose.toe[0].z > pose.ankle[0].z,
               "feet point along +z");
        EXPECT(fabsf(pose.ankle[0].y) < 0.03f,
               "idle feet stand on the ground");
        CcHumanoidSkinPose skin;
        CcHumanoidSkinPoseResolve(&pose, &skin);
        EXPECT(skin.valid, "skin solver accepts the goblin pose");
        EXPECT(fabsf(skin.body_forward.z - 1.0f) < 0.01f,
               "idle goblin body faces +z");
    }
}

static void TestModelSpace(void)
{
    CcHumanoidPose reference;
    CcHumanoidPose moved;
    EXPECT(Resolve(CC_BIPED_BODY_GOBLIN, 0.3f, 1.0f, (CcLimbVec3){0},
                   0.0f, 1.0f, &reference),
           "reference walk pose resolves");
    EXPECT(Resolve(CC_BIPED_BODY_GOBLIN, 0.3f, 1.0f,
                   (CcLimbVec3){12.0f, 0.4f, -7.0f}, 2.1f, 0.5f, &moved),
           "moved, turned and scaled pose resolves");
    EXPECT(Distance(reference.hand[1], moved.hand[1]) < 0.001f &&
               Distance(reference.ankle[0], moved.ankle[0]) < 0.001f &&
               Distance(reference.head, moved.head) < 0.001f,
           "model space removes position, yaw and scale");
}

static void TestWalk(void)
{
    int32_t opposed = 0;
    int32_t sampled = 0;
    float previous_hand = 0.0f;
    float hand_travel = 0.0f;
    for (int32_t step = 0; step < 32; ++step) {
        float phase = (float)step / 32.0f;
        CcHumanoidPose pose;
        EXPECT(Resolve(CC_BIPED_BODY_GOBLIN, phase, 1.0f, (CcLimbVec3){0},
                       0.0f, 1.0f, &pose),
               "walk pose resolves at every phase");
        CcHumanoidSkinPose skin;
        CcHumanoidSkinPoseResolve(&pose, &skin);
        EXPECT(skin.valid, "skin solver accepts every walk phase");
        float foot_lead = pose.ankle[0].z - pose.ankle[1].z;
        float hand_lead = pose.hand[0].z - pose.hand[1].z;
        if (fabsf(foot_lead) > 0.05f) {
            sampled += 1;
            if (foot_lead * hand_lead < 0.0f) opposed += 1;
        }
        if (step > 0) hand_travel += fabsf(pose.hand[0].z - previous_hand);
        previous_hand = pose.hand[0].z;
        EXPECT(fabsf(Distance(pose.shoulder[0], pose.elbow[0]) - 0.26f) <
                   0.001f,
               "upper arm keeps its bind length");
    }
    EXPECT(sampled > 8 && opposed == sampled,
           "each arm swings against the leg on its own side");
    EXPECT(hand_travel > 0.2f, "hands move through a walk cycle");

    CcHumanoidPose carry;
    EXPECT(Resolve(CC_BIPED_BODY_GOBLIN_CARRIER, 0.4f, 1.0f, (CcLimbVec3){0},
                   0.0f, 1.0f, &carry),
           "carrier walk resolves");
    EXPECT(carry.hand[0].z > carry.chest.z + 0.3f &&
               carry.hand[1].z > carry.chest.z + 0.3f,
           "carrier holds both hands forward");
}

static void TestRejects(void)
{
    CcCreatureRigPose rig = {0};
    CcHumanoidPose pose;
    EXPECT(!CcHumanoidPoseFromBipedRig(&rig,
               CcBipedBodyPlanFor(CC_BIPED_BODY_GOBLIN), (CcLimbVec3){0},
               0.0f, 1.0f, &pose),
           "an invalid rig is rejected");
    EXPECT(CcBipedBodyPlanFor(CC_BIPED_BODY_KIND_COUNT) == NULL,
           "an unknown body plan is rejected");
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "--print-bind") == 0) {
        PrintBindPose();
        return 0;
    }
    TestFamilies();
    TestIdle();
    TestModelSpace();
    TestWalk();
    TestRejects();
    if (failures != 0) return 1;
    (void)puts("character skin contract passed");
    return 0;
}
