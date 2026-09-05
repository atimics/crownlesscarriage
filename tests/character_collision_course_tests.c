#include "client/cc_local3d_internal.h"
#include "client/cc_local_place.h"
#include "world/cc_world.h"
#include "raymath.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static void Require(bool condition, const char *name)
{
    if (condition) return;
    (void)fprintf(stderr, "character collision course: %s\n", name);
    exit(EXIT_FAILURE);
}

static void TestStreamedTownContacts(void)
{
    static CcSim sim;
    static CcWorldStream stream;
    CcSimInit(&sim, UINT32_C(0x5e771e));
    Require(CcWorldStreamInit(&stream, &sim), "initialize streamed towns");
    CcLocalBindOpenWorld(&stream);
    for (int32_t i = 0; i < stream.manifest.settlement_count; ++i) {
        const CcWorldSettlementPlacement *town = &stream.manifest.settlements[i];
        const CcLocalPlaceProfile *profile = CcLocalPlaceProfileForFunction(town->function);
        const CcLocalPlaceBuilding *building = &profile->building[0];
        float z = building->z + building->depth * 0.5f;
        CcWorldPoint a = CcWorldSettlementLocalPoint(&stream.manifest, town->settlement_id,
            building->x - 1.0f / town->profile_scale, z);
        CcWorldPoint b = CcWorldSettlementLocalPoint(&stream.manifest, town->settlement_id,
            building->x + 0.5f / town->profile_scale, z);
        CcWorldPoint wall = CcWorldSettlementLocalPoint(&stream.manifest, town->settlement_id,
            building->x, z);
        float yaw = town->entrance_heading_yaw - PI * 0.5f;
        Vector3 inward = {cosf(yaw), 0, -sinf(yaw)};
        Vector3 start = {a.x, town->plateau_height + 0.8f, a.z};
        Vector3 end = {b.x, start.y, b.z};
        Vector3 corrected, normal;
        Require(CcLocalProbePhysicsSphereInternal(CC_LOCAL_SCENE_STREET, start, end,
            0.16f, &corrected, &normal), "ragdoll sphere hits a streamed building");
        Require(Vector3DotProduct(normal, inward) < -0.90f &&
            (corrected.x - wall.x) * inward.x + (corrected.z - wall.z) * inward.z < -0.154f,
            "sphere uses the visible town rotation and scale");
        start.y = end.y = town->plateau_height;
        Require(CcLocalMoveCapsuleInternal(CC_LOCAL_SCENE_STREET, start, end,
            0.22f, &corrected, &normal), "whole character hits a streamed building");
        Require((corrected.x - wall.x) * inward.x + (corrected.z - wall.z) * inward.z < -0.219f,
            "character remains outside the town wall");
        CcWorldPoint center = CcWorldSettlementLocalPoint(&stream.manifest, town->settlement_id,
            building->x + building->width * 0.5f, z);
        float roof = town->plateau_height + 0.03f + building->height * town->profile_scale;
        start = (Vector3){center.x, roof + 2, center.z};
        end = (Vector3){center.x, town->plateau_height - 1, center.z};
        Require(CcLocalMoveCapsuleInternal(CC_LOCAL_SCENE_STREET, start, end,
            0.22f, &corrected, &normal) && corrected.y >= roof && normal.y > 0.99f,
            "fast fall lands on a streamed building");
        start.y = end.y = roof + 0.2f;
        end.x += 0.1f;
        Require(!CcLocalMoveCapsuleInternal(CC_LOCAL_SCENE_STREET, start, end,
            0.22f, &corrected, &normal), "clear air above the roof stays passable");
    }
    CcLocalBindOpenWorld(NULL);
}

static void TestStandingCrowdContacts(void)
{
    static CcLocalCourse course;
    CcLocalCourseInit(&course);
    CcLocalAgent player;
    CcLocalAgentInit(&player, (Vector2){45, 30}, false);
    CcLocalAgentInit(&course.runners[0].agent, (Vector2){45, 30}, false);
    CcLocalAgentInit(&course.runners[1].agent, (Vector2){45, 30}, false);
    CcLocalCourseResolveContactsInternal(&course, &player);
    const CcLocalAgent *actors[] = {&player, &course.runners[0].agent, &course.runners[1].agent};
    for (int32_t i = 0; i < 3; ++i) {
        for (int32_t j = i + 1; j < 3; ++j) {
            Require(Vector3Distance(actors[i]->position, actors[j]->position) >=
                actors[i]->radius + actors[j]->radius - 0.001f,
                "three overlapping actors separate in one fixed step");
        }
    }
    CcLocalAgentInit(&player, (Vector2){2.778f, 7.5f}, false);
    CcLocalAgentInit(&course.runners[0].agent, (Vector2){2.5f, 7.5f}, false);
    CcLocalCourseResolveContactsInternal(&course, &player);
    Require(player.position.x <= 3.0f - player.radius + 0.001f,
        "crowd correction respects the tower wall");
    Require(Vector3Distance(player.position, course.runners[0].agent.position) >=
        player.radius + course.runners[0].agent.radius - 0.001f,
        "free actor takes the remaining correction beside a wall");
    course.runners[0].agent.position = player.position;
    course.runners[0].agent.scene = CC_LOCAL_SCENE_MARKET;
    Vector3 before = player.position;
    CcLocalCourseResolveContactsInternal(&course, &player);
    Require(Vector3Distance(before, player.position) < 0.0001f,
        "contacts respect separate scenes");
    course.runners[0].agent.scene = CC_LOCAL_SCENE_STREET;
    course.runners[0].agent.position.y += 3;
    CcLocalCourseResolveContactsInternal(&course, &player);
    Require(Vector3Distance(before, player.position) < 0.0001f,
        "vertically separated actors pass above each other");
}

static void TestWaterDepthAndExit(void)
{
    CcLocalAgent agent;
    CcLocalAgentInit(&agent, (Vector2){11, 10.75f}, false);
    CcLocalAgentUpdate(&agent, 1.0f / 60.0f, false);
    for (int32_t leg = 0; leg < CC_HUMANOID_LEG_COUNT; ++leg) {
        Require(agent.humanoid.feet[leg].normal.y > 0.99f,
            "flat footing beside the bank has level contact forces");
    }
    CcLocalAgentInit(&agent, (Vector2){11, 9.70f}, false);
    agent.position.y = 2;
    agent.grounded = false;
    agent.humanoid_needs_reset = true;
    CcLocalAgentUpdate(&agent, 1.0f / 60.0f, false);
    Require(!agent.swimming && agent.position.y > 1.8f,
        "character above the pool stays airborne");
    CcLocalAgentInit(&agent, (Vector2){11, 9.70f}, false);
    CcLocalAgentUpdate(&agent, 1.0f / 60.0f, false);
    Require(agent.swimming, "body in water enters swimming");
    Require(CcLocalAgentSetExactTarget(&agent, (Vector3){11, 0, 11.5f}, false),
        "set a target across the water edge");
    for (int32_t frame = 0; frame < 600 && agent.exact_target_valid; ++frame) {
        CcLocalAgentUpdate(&agent, 1.0f / 60.0f, false);
        Require(isfinite(agent.position.y) && agent.position.y >= -0.01f,
            "water exit keeps the body above the terrain");
    }
    Require(!agent.swimming && agent.grounded && agent.position.z > 10.65f,
        "character reaches dry ground after swimming");
}

int main(void)
{
    TestStreamedTownContacts();
    TestStandingCrowdContacts();
    TestWaterDepthAndExit();
    (void)puts("character collision course passed");
    return EXIT_SUCCESS;
}
