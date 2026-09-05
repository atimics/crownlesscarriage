#include "locomotion/cc_collision.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static void Require(bool condition, const char *name)
{
    if (condition) return;
    (void)fprintf(stderr, "collision course: %s\n", name);
    exit(EXIT_FAILURE);
}

static void TestWallsAndCorners(void)
{
    CcCollisionBox wall = {{0.0f, -2.0f, -5.0f}, {0.01f, 4.0f, 5.0f}, 0};
    CcCollisionMove move = CcCollisionMoveCapsule(
        &wall, 1, (CcLimbVec3){-10, 0, 0}, (CcLimbVec3){100, 0, 2}, 0.3f, 1.8f);
    Require(move.collided && fabsf(move.position.x + 0.302f) < 0.0001f &&
            fabsf(move.position.z - 2.0f) < 0.0001f,
            "fast wall sweep preserves sliding distance");
    CcCollisionMove particle = CcCollisionMoveSphere(
        &wall, 1, (CcLimbVec3){-0.16f, 1.0f, 0},
        (CcLimbVec3){0.1f, -0.5f, 0.4f}, 0.16f);
    Require(particle.collided && fabsf(particle.position.x + 0.16f) < 0.0001f &&
            fabsf(particle.position.y - 0.5f) < 0.0001f &&
            fabsf(particle.position.z - 0.4f) < 0.0001f,
            "physical contact preserves sliding along a wall");
    CcCollisionBox corner[] = {
        wall, {{-5.0f, -2.0f, 1.0f}, {5.0f, 4.0f, 1.01f}, 0}
    };
    move = CcCollisionMoveCapsule(corner, 2, (CcLimbVec3){-2, 0, -2},
                                   (CcLimbVec3){5, 0, 5}, 0.3f, 1.8f);
    Require(move.position.x <= -0.3019f && move.position.z <= 0.6981f,
            "both wall contacts constrain a corner");
    move = CcCollisionMoveCapsule(&wall, 1, (CcLimbVec3){-0.1f, 0, 0},
                                   (CcLimbVec3){-0.5f, 0, 0}, 0.3f, 1.8f);
    Require(move.position.x < -0.80f, "overlap recovery permits movement out");

    CcCollisionBox box = {{0, 0, 0}, {2, 2, 2}, 0};
    CcCollisionHit hit;
    Require(CcCollisionSweepSphere(&box, (CcLimbVec3){-0.5f, 1, -1},
                                    (CcLimbVec3){-0.5f, 1, 1}, 0.6f, &hit),
            "rounded corner is swept");
    Require(fabsf(hit.fraction - (1.0f - sqrtf(0.11f)) * 0.5f) < 0.0001f,
            "rounded corner uses the sphere surface");
}

static void TestFullCapsuleAndDoors(void)
{
    CcCollisionBox thin = {{0, 0.605f, -2}, {0.005f, 0.615f, 2}, 0};
    CcCollisionMove move = CcCollisionMoveCapsule(
        &thin, 1, (CcLimbVec3){-2, 0, 0}, (CcLimbVec3){4, 0, 0}, 0.3f, 1.8f);
    Require(move.collided && move.position.x < -0.30f,
            "thin ledge between former sphere samples catches the body");
    CcCollisionBox doorway[] = {
        {{-3, 0, 0}, {-0.5f, 3, 1}, 0}, {{0.5f, 0, 0}, {3, 3, 1}, 0}
    };
    move = CcCollisionMoveCapsule(doorway, 2, (CcLimbVec3){0, 0, -2},
                                   (CcLimbVec3){0, 0, 4}, 0.3f, 1.8f);
    Require(fabsf(move.position.z - 2) < 0.0001f, "body fits an open doorway");
    doorway[0].maximum.x = -0.25f;
    doorway[1].minimum.x = 0.25f;
    move = CcCollisionMoveCapsule(doorway, 2, (CcLimbVec3){0, 0, -2},
                                   (CcLimbVec3){0, 0, 4}, 0.3f, 1.8f);
    Require(move.collided && move.position.z < 0.0f,
            "body stops before a doorway narrower than its width");
}

static void TestRotatedBuildings(void)
{
    for (int32_t index = 0; index < 16; ++index) {
        float yaw = (float)index * 0.37f;
        CcCollisionBox wall = {{-0.02f, 0, -3}, {0.02f, 4, 3}, yaw};
        float c = cosf(yaw);
        float s = sinf(yaw);
        CcCollisionMove move = CcCollisionMoveCapsule(
            &wall, 1, (CcLimbVec3){-2 * c, 0, 2 * s},
            (CcLimbVec3){4 * c, 0, -4 * s}, 0.3f, 1.8f);
        Require(move.collided &&
                fabsf(c * move.position.x - s * move.position.z + 0.322f) < 0.0001f,
                "town rotation preserves the collision surface");
    }
}

static void TestSupportAndCeilings(void)
{
    CcCollisionBox floor = {{-5, -0.2f, -5}, {5, 0, 5}, 0};
    CcCollisionMove move = CcCollisionMoveCapsule(
        &floor, 1, (CcLimbVec3){0, 2, 0}, (CcLimbVec3){1, -10, 0}, 0.3f, 1.8f);
    Require(move.collided && fabsf(move.position.y - CC_COLLISION_SKIN) < 0.0001f &&
            fabsf(move.position.x - 1) < 0.0001f,
            "fast fall lands with the requested horizontal motion");
    CcCollisionBox bridge = {{-2, 1.9f, -1}, {2, 2.2f, 1}, 0};
    move = CcCollisionMoveCapsule(&bridge, 1, (CcLimbVec3){0, 0, -3},
                                   (CcLimbVec3){0, 0, 6}, 0.3f, 1.8f);
    Require(fabsf(move.position.z - 3) < 0.0001f,
            "body passes beneath a bridge with enough clearance");
    move = CcCollisionMoveCapsule(&bridge, 1, (CcLimbVec3){0, 0, 0},
                                   (CcLimbVec3){0, 1, 0}, 0.3f, 1.8f);
    Require(move.collided && fabsf(move.position.y - 0.098f) < 0.0001f &&
            move.normal.y < -0.99f, "jump respects bridge underside");
}

int main(void)
{
    TestWallsAndCorners();
    TestFullCapsuleAndDoors();
    TestRotatedBuildings();
    TestSupportAndCeilings();
    (void)puts("continuous collision course passed");
    return EXIT_SUCCESS;
}
