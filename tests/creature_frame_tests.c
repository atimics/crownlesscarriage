#include "client/cc_local3d_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static void Require(bool condition, const char *message)
{
    if (condition) return;
    fprintf(stderr, "%s\n", message);
    exit(1);
}

static CcCreatureRigPose Run(int32_t rate, bool pause)
{
    CcCreatureRigPose pose;
    const float scale = 0.84f;
    Vector3 ground = {7.0f, 0.0f, 5.0f};
    double accumulator = 0.0;
    float pause_time = 0.0f;
    for (int32_t frame = 0; frame <= rate * 5; ++frame) {
        float clock = (float)frame / (float)rate;
        ground.z = 5.0f + clock * 0.3f;
        clock += pause_time;
        Require(CcLocalCreatureGaitPoseInternal(0, CC_CREATURE_RIG_HORSE,
            CC_CREATURE_RIG_GAIT_WALK, clock, 0.0f, ground, 0.0f, scale,
            CC_LOCAL_SCENE_ROAD, &pose), "animal pose request failed");
        if (frame > 0) accumulator += 1.0 / (double)rate;
        while (accumulator + 0.000000001 >= 1.0 / 60.0) {
            CcLocalCreatureGaitsFixedStepInternal(1.0f / 60.0f);
            accumulator -= 1.0 / 60.0;
        }
        Require(CcLocalCreatureGaitPoseInternal(0, CC_CREATURE_RIG_HORSE,
            CC_CREATURE_RIG_GAIT_WALK, clock, 0.0f, ground, 0.0f, scale,
            CC_LOCAL_SCENE_ROAD, &pose), "animal draw pose failed");
        CcCreatureRigPose repeated;
        Require(CcLocalCreatureGaitPoseInternal(0, CC_CREATURE_RIG_HORSE,
            CC_CREATURE_RIG_GAIT_WALK, clock, 0.0f, ground, 0.0f, scale,
            CC_LOCAL_SCENE_ROAD, &repeated), "repeated draw failed");
        Require(pose.phase == repeated.phase && pose.planted_count == repeated.planted_count,
                "drawing advanced the gait");
        if (pause && frame == rate * 2) {
            for (int32_t i = 1; i <= rate; ++i) {
                Require(CcLocalCreatureGaitPoseInternal(0, CC_CREATURE_RIG_HORSE,
                    CC_CREATURE_RIG_GAIT_WALK, clock + (float)i / (float)rate,
                    0.0f, ground, 0.0f, scale, CC_LOCAL_SCENE_ROAD, &repeated),
                    "paused pose failed");
                Require(repeated.phase == pose.phase, "paused drawing advanced a footstep");
            }
            pause_time = 1.0f;
        }
    }
    Require(pose.planted_count >= 3 && pose.phase > 0.1f, "road pony did not walk with support");
    return pose;
}

int main(void)
{
    CcCreatureRigPose slow = Run(30, false);
    CcCreatureRigPose fast = Run(120, false);
    CcCreatureRigPose paused = Run(60, true);
    Require(fabsf(paused.phase - fast.phase) < 0.0001f,
            "paused drawing delayed movement after resuming");
    Require(fabsf(slow.phase - fast.phase) < 0.0001f,
            "rendering rate changed gait timing");
    for (int32_t leg = 0; leg < 4; ++leg) {
        for (int32_t joint = 0; joint <= 2; ++joint) {
            CcLimbVec3 a = slow.limbs[leg].joints[joint];
            CcLimbVec3 b = fast.limbs[leg].joints[joint];
            float d = sqrtf((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z));
            Require(d < 0.001f, "rendering rate changed the animal pose");
        }
    }
    puts("fixed creature gait timing passed at 30 and 120 frames per second");
    return 0;
}
