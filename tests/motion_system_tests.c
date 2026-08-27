#include "locomotion/cc_humanoid.h"
#include "locomotion/cc_motion.h"
#include <math.h>
#include <stdint.h>
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
    if (origin.y < -maximum_drop) return false;
    *point = (CcLimbVec3){origin.x, 0.0f, origin.z};
    *normal = (CcLimbVec3){0.0f, 1.0f, 0.0f};
    return true;
}

static float Distance(CcLimbVec3 a, CcLimbVec3 b)
{
    float x = a.x - b.x;
    float y = a.y - b.y;
    float z = a.z - b.z;
    return sqrtf(x * x + y * y + z * z);
}

static float MaximumPosePointStep(const CcHumanoidPose *a,
                                  const CcHumanoidPose *b)
{
    float result = 0.0f;
#define INCLUDE_POINT(name) result = fmaxf(result, Distance(a->name, b->name))
    INCLUDE_POINT(pelvis);
    INCLUDE_POINT(spine);
    INCLUDE_POINT(chest);
    INCLUDE_POINT(neck);
    INCLUDE_POINT(head);
    for (int32_t leg = 0; leg < CC_HUMANOID_LEG_COUNT; ++leg) {
        result = fmaxf(result, Distance(a->hip[leg], b->hip[leg]));
        result = fmaxf(result, Distance(a->knee[leg], b->knee[leg]));
        result = fmaxf(result, Distance(a->ankle[leg], b->ankle[leg]));
        result = fmaxf(result, Distance(a->heel[leg], b->heel[leg]));
        result = fmaxf(result, Distance(a->ball[leg], b->ball[leg]));
        result = fmaxf(result, Distance(a->toe[leg], b->toe[leg]));
    }
    for (int32_t arm = 0; arm < CC_HUMANOID_ARM_COUNT; ++arm) {
        result = fmaxf(result, Distance(a->shoulder[arm], b->shoulder[arm]));
        result = fmaxf(result, Distance(a->elbow[arm], b->elbow[arm]));
        result = fmaxf(result, Distance(a->hand[arm], b->hand[arm]));
    }
#undef INCLUDE_POINT
    return result;
}

static void TestMotionTimeline(void)
{
    CcMotionPlayer player;
    CcMotionPlayerInit(&player, CC_MOTION_CLIP_STRIKE_CUT);
    CcMotionPlayerAdvance(&player, 0.50f);
    Require(CcMotionPlayerConsumeMarkers(&player) == 0,
            "strike emitted a gameplay marker during anticipation");
    CcMotionPlayerAdvance(&player, 0.12f);
    uint32_t impact = CcMotionPlayerConsumeMarkers(&player);
    Require((impact & CC_MOTION_MARKER_WEAPON_ACTIVE) != 0 &&
            (impact & CC_MOTION_MARKER_WEAPON_IMPACT) != 0,
            "strike timeline did not emit its active and impact markers");
    Require(CcMotionPlayerConsumeMarkers(&player) == 0,
            "motion markers were not consume-once events");
    CcMotionPlayerAdvance(&player, 1.0f);
    Require(player.finished &&
            fabsf(CcMotionPlayerNormalizedTime(&player) - 1.0f) < 0.0001f,
            "non-looping motion did not finish at the last sample");

    CcMotionPlayerPlay(&player, CC_MOTION_CLIP_WALK, true);
    CcMotionPlayerAdvance(&player, 2.10f);
    uint32_t contacts = CcMotionPlayerConsumeMarkers(&player);
    Require(player.loop_count == 2 &&
            (contacts & CC_MOTION_MARKER_LEFT_CONTACT) != 0 &&
            (contacts & CC_MOTION_MARKER_RIGHT_CONTACT) != 0,
            "looping motion lost synchronized contact markers");

    CcMotionPlayer paused = player;
    CcMotionPlayerAdvance(&player, INFINITY);
    Require(memcmp(&player, &paused, sizeof(player)) == 0,
            "infinite elapsed time advanced or hung a looping motion");
    CcMotionPlayerAdvance(&player, NAN);
    Require(memcmp(&player, &paused, sizeof(player)) == 0,
            "non-finite elapsed time mutated a motion player");
}

static void TestLocalClipSampling(void)
{
    const CcMotionTransform samples[] = {
        {
            .translation = {0.0f, 0.0f, 0.0f},
            .rotation = {0.0f, 0.0f, 0.0f, 1.0f},
            .scale = {1.0f, 1.0f, 1.0f}
        },
        {
            .translation = {2.0f, 1.0f, -1.0f},
            .rotation = {0.0f, 1.0f, 0.0f, 0.0f},
            .scale = {1.0f, 1.0f, 1.0f}
        }
    };
    const CcMotionClip clip = {
        .id = CC_MOTION_CLIP_NONE,
        .name = "test.authored",
        .duration = 1.0f,
        .sample_rate = 1.0f,
        .bone_count = 1,
        .sample_count = 2,
        .samples = samples
    };
    CcMotionTransform sampled;
    Require(CcMotionClipSampleLocalPose(&clip, 0.5f, &sampled, 1),
            "authored local-space motion could not be sampled");
    float rotation_length = sqrtf(
        sampled.rotation.x * sampled.rotation.x +
        sampled.rotation.y * sampled.rotation.y +
        sampled.rotation.z * sampled.rotation.z +
        sampled.rotation.w * sampled.rotation.w);
    Require(fabsf(sampled.translation.x - 1.0f) < 0.0001f &&
            fabsf(sampled.translation.y - 0.5f) < 0.0001f &&
            fabsf(sampled.translation.z + 0.5f) < 0.0001f &&
            fabsf(rotation_length - 1.0f) < 0.0001f,
            "local-space motion interpolation changed pose or quaternion scale");
}

static void TestHighMantlePerformance(void)
{
    const CcMotionClip *clip = CcMotionClipGet(CC_MOTION_CLIP_CLIMB);
    Require(clip != NULL && clip->name != NULL &&
            clip->marker_count == 7 && !clip->loop,
            "high mantle has no authored contact timeline");

    CcMotionMantleSample preparation;
    CcMotionMantleSample wall_plant;
    CcMotionMantleSample transfer;
    CcMotionMantleSample lead_top;
    CcMotionMantleSample trail_top;
    Require(CcMotionClipSampleMantle(0.10f, &preparation) &&
            CcMotionClipSampleMantle(0.40f, &wall_plant) &&
            CcMotionClipSampleMantle(0.56f, &transfer) &&
            CcMotionClipSampleMantle(0.78f, &lead_top) &&
            CcMotionClipSampleMantle(0.90f, &trail_top),
            "high mantle performance could not be sampled");
    Require(preparation.pelvis_height < 0.85f &&
            preparation.root_depth_progress < 0.001f &&
            preparation.hand_grip[0] > preparation.hand_grip[1],
            "high mantle lost its crouch or staggered hand preparation");
    Require(wall_plant.lead_wall_step > 0.99f &&
            wall_plant.trail_tuck < 0.01f &&
            wall_plant.root_depth_progress < wall_plant.root_progress,
            "high mantle no longer loads a lead-foot wall plant");
    Require(transfer.trail_tuck > 0.99f &&
            transfer.foot_support[0] > 0.99f &&
            transfer.foot_support[1] < 0.01f &&
            transfer.chest_inward > 0.17f,
            "high mantle lost its supported trailing-leg tuck");
    Require(lead_top.lead_top_step > 0.99f &&
            lead_top.trail_top_step < 0.01f &&
            lead_top.hand_press[0] > lead_top.hand_press[1] &&
            lead_top.root_depth_progress < 0.25f,
            "high mantle lost lead-foot top contact before the ledge crossing");
    Require(trail_top.trail_top_step > 0.99f &&
            trail_top.foot_support[0] > 0.99f &&
            trail_top.foot_support[1] > 0.99f,
            "high mantle does not finish with two-foot support");

    CcMotionPlayer player;
    CcMotionPlayerInit(&player, CC_MOTION_CLIP_CLIMB);
    CcMotionPlayerSetNormalizedTime(&player, 0.14f);
    uint32_t markers = CcMotionPlayerConsumeMarkers(&player);
    Require(markers == CC_MOTION_MARKER_LEFT_HAND_CONTACT,
            "mantle did not emit the lead-hand contact at authored time");
    CcMotionPlayerSetNormalizedTime(&player, 0.58f);
    markers = CcMotionPlayerConsumeMarkers(&player);
    Require((markers & CC_MOTION_MARKER_RIGHT_HAND_CONTACT) != 0U &&
            (markers & CC_MOTION_MARKER_LEFT_CONTACT) != 0U &&
            (markers & CC_MOTION_MARKER_WEIGHT_TRANSFER) != 0U,
            "mantle timeline lost catch, wall-plant, or transfer markers");
    Require(CcMotionPlayerConsumeMarkers(&player) == 0U,
            "mantle contact markers were not consume-once events");
    CcMotionPlayerSetNormalizedTime(&player, 1.0f);
    markers = CcMotionPlayerConsumeMarkers(&player);
    Require((markers & CC_MOTION_MARKER_LEFT_CONTACT) != 0U &&
            (markers & CC_MOTION_MARKER_RIGHT_CONTACT) != 0U &&
            (markers & CC_MOTION_MARKER_RECOVERY) != 0U && player.finished,
            "mantle timeline did not emit both top plants and recovery");
}

static void TestStableIdleAndSnapshots(void)
{
    CcLimbVec3 body = {1.0f, 0.0f, 2.0f};
    CcHumanoidGait gait;
    CcHumanoidGaitInit(&gait, body, 0.0f, PlaneProbe, NULL);
    Require(gait.initialized && gait.idle.stable && gait.idle.pose_locked,
            "humanoid did not initialize into its stable idle contract");
    const CcHumanoidPoseSnapshot *initial =
        CcHumanoidGaitCurrentSnapshot(&gait);
    Require(initial != NULL && initial->owner ==
            CC_HUMANOID_POSE_OWNER_PROCEDURAL,
            "initial pose snapshot has no authoritative owner");

    CcHumanoidPose settled = gait.pose;
    float maximum_settled_step = 0.0f;
    for (int32_t frame = 0; frame < 600; ++frame) {
        CcHumanoidPose previous = gait.pose;
        CcHumanoidGaitAdvance(&gait, body, 0.0f, (CcLimbVec3){0}, true,
                              1.0f / 60.0f, PlaneProbe, NULL);
        if (frame >= 60) {
            maximum_settled_step = fmaxf(
                maximum_settled_step,
                MaximumPosePointStep(&previous, &gait.pose));
            settled = gait.pose;
        }
    }
    const CcHumanoidPoseSnapshot *current =
        CcHumanoidGaitCurrentSnapshot(&gait);
    const CcHumanoidPoseSnapshot *previous =
        CcHumanoidGaitPreviousSnapshot(&gait);
    Require(maximum_settled_step < 0.000001f && gait.idle.stable &&
            gait.idle.pose_locked && gait.motion.clip->id ==
            CC_MOTION_CLIP_IDLE,
            "grounded idle continued producing simulation pose motion");
    Require(current != NULL && previous != NULL &&
            current->sequence == previous->sequence + 1 &&
            MaximumPosePointStep(&current->pose, &settled) < 0.000001f,
            "finalized pose history is not a consecutive fixed-tick pair");
    const CcHumanoidAnimationTraceRecord *trace =
        CcHumanoidGaitTraceLatest(&gait);
    Require(trace != NULL && gait.trace.count == CC_HUMANOID_TRACE_CAPACITY &&
            trace->sequence == current->sequence && trace->idle_locked &&
            trace->clip == CC_MOTION_CLIP_IDLE,
            "rewindable animation trace did not capture the finalized state");
    for (int32_t leg = 0; leg < CC_HUMANOID_LEG_COUNT; ++leg) {
        Require(gait.feet[leg].contact == CC_HUMANOID_CONTACT_FLAT &&
                Distance(gait.feet[leg].current_point,
                         gait.idle.foot_anchor[leg]) < 0.000001f,
                "stable idle released a planted foot anchor");
    }

    CcHumanoidGaitAdvance(&gait, body, 0.0f,
                          (CcLimbVec3){0.0f, 0.0f, 1.0f}, true,
                          1.0f / 60.0f, PlaneProbe, NULL);
    Require(!gait.idle.stable && gait.motion.clip->id == CC_MOTION_CLIP_WALK,
            "movement intent did not release stable idle hysteresis");
}

static void TestPoseOwnership(void)
{
    CcHumanoidGait gait;
    CcLimbVec3 body = {0.0f, 0.0f, 0.0f};
    CcHumanoidGaitInit(&gait, body, 0.0f, PlaneProbe, NULL);
    Require(CcHumanoidGaitKnockDown(&gait),
            "pose ownership fixture could not enter ragdoll");
    CcHumanoidGaitAdvance(&gait, body, 0.0f, (CcLimbVec3){0}, false,
                          1.0f / 60.0f, PlaneProbe, NULL);
    const CcHumanoidPoseSnapshot *snapshot =
        CcHumanoidGaitCurrentSnapshot(&gait);
    Require(snapshot != NULL && snapshot->owner ==
            CC_HUMANOID_POSE_OWNER_RAGDOLL,
            "ragdoll pose was finalized under a competing owner");
    Require(CcMotionClipGet(CC_MOTION_CLIP_GET_UP_SUPINE)->duration > 0.0f &&
            CcMotionClipGet(CC_MOTION_CLIP_GET_UP_PRONE)->duration > 0.0f,
            "orientation-specific recovery timelines are unavailable");
}

int main(void)
{
    TestMotionTimeline();
    TestLocalClipSampling();
    TestHighMantlePerformance();
    TestStableIdleAndSnapshots();
    TestPoseOwnership();
    (void)printf("motion system tests passed\n");
    return 0;
}
