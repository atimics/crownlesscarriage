#include "locomotion/cc_humanoid.h"
#include "locomotion/cc_humanoid_skin.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static float Dot(CcLimbVec3 a, CcLimbVec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static void RequirePoseValid(const CcHumanoidSkinPose *pose)
{
    Require(pose->valid, "skin adapter rejected a physical humanoid pose");
    for (int32_t bone = 0; bone < CC_HUMANOID_SKIN_BONE_COUNT; ++bone) {
        const CcHumanoidSkinBonePose *item = &pose->bones[bone];
        float quaternion_length = sqrtf(
            item->world_rotation.x * item->world_rotation.x +
            item->world_rotation.y * item->world_rotation.y +
            item->world_rotation.z * item->world_rotation.z +
            item->world_rotation.w * item->world_rotation.w);
        Require(isfinite(quaternion_length) &&
                fabsf(quaternion_length - 1.0f) < 0.001f,
                "skin bone emitted a non-finite or unnormalized rotation");
        Require(fabsf(Dot(item->right, item->up)) < 0.001f &&
                fabsf(Dot(item->right, item->forward)) < 0.001f &&
                fabsf(Dot(item->up, item->forward)) < 0.001f,
                "skin bone frame was not orthogonal");
        Require(Distance(item->head, item->tail) > 0.05f,
                "skin adapter collapsed a bone");
    }
}

static void TestContract(void)
{
    static const char *EXPECTED_NAMES[] = {
        "root", "pelvis", "spine", "chest", "neck", "head",
        "upper_arm.L", "forearm.L", "hand.L",
        "upper_arm.R", "forearm.R", "hand.R",
        "thigh.L", "shin.L", "foot.L", "thigh.R", "shin.R", "foot.R",
    };
    for (int32_t bone = 0; bone < CC_HUMANOID_SKIN_BONE_COUNT; ++bone) {
        Require(strcmp(CcHumanoidSkinBoneName((CcHumanoidSkinBone)bone),
                       EXPECTED_NAMES[bone]) == 0,
                "runtime bone name drifted from the Blender armature");
        Require(CcHumanoidSkinBoneFind(EXPECTED_NAMES[bone]) == bone,
                "runtime bone lookup did not round trip");
    }
    Require(CcHumanoidSkinBoneParent(CC_HUMANOID_SKIN_ROOT) == -1 &&
            CcHumanoidSkinBoneParent(CC_HUMANOID_SKIN_HAND_RIGHT) ==
                CC_HUMANOID_SKIN_FOREARM_RIGHT &&
            CcHumanoidSkinBoneParent(CC_HUMANOID_SKIN_FOOT_LEFT) ==
                CC_HUMANOID_SKIN_SHIN_LEFT,
            "runtime hierarchy drifted from the Blender armature");
    Require(CcHumanoidSkinBoneFind("not-a-bone") == -1,
            "unknown Blender bone unexpectedly resolved");
}

static void TestPhysicsDrivenPoses(void)
{
    const float delta_time = 1.0f / 60.0f;
    CcLimbVec3 body = {0.0f, 0.0f, 0.0f};
    CcHumanoidGait gait;
    CcHumanoidGaitInit(&gait, body, 0.0f, PlaneProbe, NULL);
    CcHumanoidSkinPose pose;
    CcHumanoidSkinPoseResolve(&gait.pose, &pose);
    RequirePoseValid(&pose);
    Require(Distance(pose.bones[CC_HUMANOID_SKIN_THIGH_LEFT].head,
                     gait.pose.hip[0]) < 0.00001f &&
            Distance(pose.bones[CC_HUMANOID_SKIN_THIGH_LEFT].tail,
                     gait.pose.knee[0]) < 0.00001f &&
            Distance(pose.sockets[CC_HUMANOID_SOCKET_HAND_RIGHT].position,
                     gait.pose.hand[1]) < 0.00001f,
            "physical landmarks did not reach their Blender bones and sockets");

    CcHumanoidSkinPose same_pose;
    CcHumanoidSkinPoseResolve(&gait.pose, &same_pose);
    Require(memcmp(&pose, &same_pose, sizeof(pose)) == 0,
            "skin resolution was not deterministic");

    for (int32_t frame = 0; frame < 180; ++frame) {
        CcLimbVec3 velocity = {0.35f, 0.0f, 1.1f};
        CcHumanoidGaitAdvance(&gait, body, 0.3f, velocity, true,
                              delta_time, PlaneProbe, NULL);
        body.x += gait.root_velocity.x * delta_time;
        body.z += gait.root_velocity.z * delta_time;
        CcHumanoidSkinPoseResolve(&gait.pose, &pose);
        RequirePoseValid(&pose);
    }

    CcHumanoidGaitSetGuarded(&gait, true);
    for (int32_t frame = 0; frame < 45; ++frame) {
        CcHumanoidGaitAdvance(&gait, body, 0.3f, (CcLimbVec3){0}, true,
                              delta_time, PlaneProbe, NULL);
    }
    Require(CcHumanoidGaitBeginStrike(&gait, 1),
            "test humanoid rejected its physical strike");
    for (int32_t frame = 0; frame < 90; ++frame) {
        CcHumanoidGaitAdvance(&gait, body, 0.3f, (CcLimbVec3){0}, true,
                              delta_time, PlaneProbe, NULL);
        CcHumanoidSkinPoseResolve(&gait.pose, &pose);
        RequirePoseValid(&pose);
    }

    for (int32_t frame = 0; frame < 180; ++frame) {
        CcHumanoidGaitAdvanceSwim(
            &gait, body, 0.3f, (CcLimbVec3){0.2f, 0.0f, 0.7f},
            0.82f, 1.0f, delta_time);
        CcHumanoidSkinPoseResolve(&gait.pose, &pose);
        RequirePoseValid(&pose);
    }
}

int main(void)
{
    TestContract();
    TestPhysicsDrivenPoses();
    return 0;
}
