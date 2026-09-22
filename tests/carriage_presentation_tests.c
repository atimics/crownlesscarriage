#include "client/cc_local3d.h"
#include "locomotion/cc_creature.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void Require(bool ok, const char *message)
{
    if (!ok) { fprintf(stderr, "%s\n", message); exit(1); }
}

static void RunCadence(float rate, bool irregular)
{
    CcLocalWorldCarriageState c = {.storybook_travel = true};
    CcLocalCarriagePublishPose(&c, (Vector3){0}, 3.1f, 0, false, 1);
    const double fixed = 1.0 / 60.0;
    double local = 0, journey = 0;
    float distance = 0, last = 0;
    for (int frame = 0; frame < 720; ++frame) {
        const double cadence[] = {1.0/120.0, 1.0/24.0, 1.0/90.0, 1.0/40.0};
        local += irregular ? cadence[frame % 4] : 1.0/120.0;
        while (local + 1e-9 >= fixed) {
            journey += rate;
            int ticks = (int)floor(journey + 1e-9);
            journey -= ticks;
            distance += (float)ticks;
            CcLocalCarriagePublishPose(&c, (Vector3){distance, 0, 0},
                3.1f, distance, true, 1);
            local -= fixed;
        }
        CcLocalCarriageInterpolate(&c, (float)(local / fixed));
        Require(c.render_position.x + 0.0001f >= last, "forward travel rewound between local samples");
        Require(fabsf(c.render_position.x - c.render_travelled) < 0.0001f,
                "wheel distance and root came from different samples");
        Require(c.render_position.x <= distance + 0.0001f, "presentation extrapolated past authority");
        last = c.render_position.x;
    }
}

static void TestPosePublication(void)
{
    CcLocalWorldCarriageState c = {.storybook_travel = true};
    CcLocalCarriagePublishPose(&c, (Vector3){250, 4, -300}, 3.1f, 20, false, 1);
    CcLocalCarriagePublishPose(&c, (Vector3){251, 4, -300}, -3.1f, 21, true, 0.5f);
    Require(fabsf(c.render_position.x - 250.5f) < 0.0001f, "subframe position missing");
    Require(fabsf(c.render_heading_yaw - PI) < 0.0001f, "heading took the long turn");
    Require(c.render_travelled == 20.5f, "distance was not interpolated with pose");
    CcLocalCarriagePublishPose(&c, (Vector3){-120, 3, 140}, 1.2f, 80, false, 1);
    Require(c.render_position.x == -120 && c.render_heading_yaw == 1.2f &&
            c.render_travelled == 80, "direct arrival/departure left stale render fields");
    CcLocalCarriageInterpolate(&c, NAN);
    Require(c.render_position.x == -120, "invalid alpha corrupted the pose");
    c.presentation_valid = false;
    c.position.x = 42; c.heading_yaw = 0.7f; c.travelled = 123;
    Require(CcLocalCarriageRenderPosition(&c).x == 42 &&
            CcLocalCarriageRenderHeading(&c) == 0.7f &&
            CcLocalCarriageRenderDistance(&c) == 123,
            "uninitialized fixture mixed physical and render fields");
}

static void TestScaledRolling(void)
{
    for (int wheel = 0; wheel < 4; ++wheel) {
        float radius = CcLocalCarriageWheelWorldRadiusInternal(wheel);
        CcLocalWorldCarriageState c = {.storybook_travel = true, .route_id = 1};
        CcLocalCarriagePublishPose(&c, (Vector3){0}, 0, 0, false, 1);
        CcLocalCarriageRoll(&c, 0.5f);
        float circumference = 2 * PI * radius * 0.5f;
        CcLocalCarriagePublishPose(&c, (Vector3){0}, 0, circumference, false, 1);
        CcLocalCarriageRoll(&c, 0.5f);
        Require(fabs(c.rolling_distance / radius - 2 * PI) < 0.0001,
                "scaled wheel did not make one revolution");
        double stopped = c.rolling_distance;
        CcLocalCarriageRoll(&c, 0.35f);
        Require(c.rolling_distance == stopped, "resizing at rest spun the wheel");
        CcLocalCarriagePublishPose(&c, (Vector3){0}, 0, 0, false, 1);
        CcLocalCarriageRoll(&c, 0.35f);
        Require(c.rolling_distance < stopped, "reverse travel failed to reverse the wheel");
        stopped = c.rolling_distance;
        c.route_id = 2;
        c.render_travelled = 200;
        CcLocalCarriageRoll(&c, 0.35f);
        Require(c.rolling_distance == stopped, "route rebase changed wheel phase");
    }
    CcLocalWorldCarriageState c = {.storybook_travel = true};
    CcLocalCarriagePublishPose(&c, (Vector3){0}, 0, 0, false, 1);
    CcLocalCarriageRoll(&c, 1);
    for (int i = 1; i <= 1000; ++i) {
        float t = (float)i / 1000;
        CcLocalCarriagePublishPose(&c, (Vector3){0}, 0, t, false, 1);
        CcLocalCarriageRoll(&c, 1 - 0.5f * t);
    }
    Require(fabs(c.rolling_distance - 2 * log(2.0)) < 0.00001,
            "varying radius did not integrate signed distance");
}

static void TestResizePreservesContacts(void)
{
    CcCreatureRigController c;
    Require(CcCreatureRigControllerInit(&c, CC_CREATURE_RIG_HORSE, 0.37f, 0.96f), "rig init failed");
    (void)CcCreatureRigControllerSetGait(&c, CC_CREATURE_RIG_GAIT_TROT);
    c.movement = 0.8f;
    c.skeleton.limbs[0].state = CC_LIMB_SWING;
    c.skeleton.limbs[0].swing_progress = 0.43f;
    CcLimbRuntime before[CC_LIMB_MAX_COUNT];
    memcpy(before, c.skeleton.limbs, sizeof(before));
    for (int frame = 1; frame <= 120; ++frame) {
        float scale = 0.96f * (1 - 0.5f * (float)frame / 120);
        Require(CcCreatureRigControllerSetScale(&c, scale), "resize failed");
        Require(c.skeleton.gait_phase == 0.37f && c.movement == 0.8f &&
                c.gait == CC_CREATURE_RIG_GAIT_TROT,
                "resize reset locomotion state");
        Require(memcmp(before, c.skeleton.limbs, sizeof(before)) == 0,
                "resize discarded planted contacts or swing progress");
        Require(c.scale == scale, "new anatomical scale not applied");
    }
    Require(!CcCreatureRigControllerSetScale(&c, 0), "zero scale accepted");
}

int main(void)
{
    TestPosePublication();
    const float rates[] = {0, 0.25f, 0.5f, 0.8f, 1, 2, 8};
    for (int i = 0; i < 7; ++i) {
        RunCadence(rates[i], false);
        RunCadence(rates[i], true);
    }
    TestScaledRolling();
    TestResizePreservesContacts();
    puts("PASS carriage presentation: cadence, complete samples, scale, rolling and contacts");
    return 0;
}
