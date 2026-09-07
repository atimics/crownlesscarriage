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

/* A drawn pose is expressed around the ground position it was asked for, so
   every joint belongs within a body length or so of the origin. A team whose
   legs are still standing where it used to be reads as long thin triangles
   reaching back across the world. */
static float FarthestJoint(const CcCreatureRigPose *pose)
{
    float farthest = fmaxf(fabsf(pose->body.x),
                           fmaxf(fabsf(pose->body.y), fabsf(pose->body.z)));
    for (int32_t limb = 0; limb < pose->limb_count; ++limb) {
        for (int32_t joint = 0; joint <= pose->limbs[limb].segment_count;
             ++joint) {
            CcLimbVec3 point = pose->limbs[limb].joints[joint];
            farthest = fmaxf(farthest, fmaxf(fabsf(point.x),
                fmaxf(fabsf(point.y), fabsf(point.z))));
        }
    }
    return farthest;
}

static float WalkThenMove(Vector3 arrival, float *settled_out)
{
    const float scale = 0.96f;
    CcCreatureRigPose pose;
    Vector3 ground = {12.0f, 0.0f, 4.0f};
    float clock = 0.0f;
    for (int32_t frame = 0; frame < 180; ++frame) {
        clock = (float)frame / 60.0f;
        ground.z += 0.02f;
        Require(CcLocalCreatureGaitPoseInternal(
                    0, CC_CREATURE_RIG_HORSE, CC_CREATURE_RIG_GAIT_WALK, clock,
                    0.0f, ground, 0.0f, scale, CC_LOCAL_SCENE_ROAD, &pose),
                "a walking team should hold a pose");
        CcLocalCreatureGaitsFixedStepInternal(1.0f / 60.0f);
    }
    float settled = FarthestJoint(&pose);
    Require(settled < 8.0f, "a walking team keeps its legs under itself");
    if (settled_out != NULL) *settled_out = settled;

    /* Departure puts the team on the road in a single frame, with the clock
       running on and the scene unchanged. */
    clock += 1.0f / 60.0f;
    Require(CcLocalCreatureGaitPoseInternal(
                0, CC_CREATURE_RIG_HORSE, CC_CREATURE_RIG_GAIT_WALK, clock,
                0.0f, arrival, 0.0f, scale, CC_LOCAL_SCENE_ROAD, &pose),
            "a departing team should hold a pose");
    return FarthestJoint(&pose);
}

int main(void)
{
    float settled = 0.0f;
    float near_step = WalkThenMove((Vector3){12.4f, 0.0f, 8.2f}, &settled);
    printf("settled %.2f, ordinary step %.2f\n", (double)settled,
           (double)near_step);
    Require(near_step < 8.0f, "an ordinary step should not stretch the team");

    float departure = WalkThenMove((Vector3){52.0f, 0.0f, -21.0f}, NULL);
    printf("departure %.2f\n", (double)departure);
    Require(departure < 8.0f,
            "a team that moves to the road keeps its legs on its body");

    puts("Creature teleport poses passed");
    return 0;
}
