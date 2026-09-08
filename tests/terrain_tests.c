#include "client/cc_local3d.h"
#include "client/cc_local3d_internal.h"
#include "client/cc_local_place.h"
#include "test_support.h"

#include <float.h>
#include <math.h>
#include <stdio.h>

static int TestSeededTerrain(void)
{
    static const Vector2 points[] = {
        {3.0f, 18.0f}, {18.0f, 45.0f}, {31.0f, 62.0f},
        {58.0f, 12.0f}, {74.0f, 48.0f}, {92.0f, 66.0f},
    };
    float first[sizeof(points) / sizeof(points[0])];
    CcLocalTerrainSetSeed(UINT32_C(0xc0a71a9e));
    for (int32_t i = 0; i < (int32_t)(sizeof(points) /
                                      sizeof(points[0])); ++i) {
        first[i] = CcLocalTerrainHeightAt(points[i].x, points[i].y);
    }

    CcLocalTerrainSetSeed(UINT32_C(0x12345678));
    bool changed = false;
    float largest_change = 0.0f;
    for (int32_t i = 0; i < (int32_t)(sizeof(points) /
                                      sizeof(points[0])); ++i) {
        float change = fabsf(CcLocalTerrainHeightAt(
            points[i].x, points[i].y) - first[i]);
        largest_change = fmaxf(largest_change, change);
        changed = changed || change > 0.05f;
    }
    CcLocalTerrainSetSeed(UINT32_C(0xc0a71a9e));
    for (int32_t i = 0; i < (int32_t)(sizeof(points) /
                                      sizeof(points[0])); ++i) {
        float repeated = CcLocalTerrainHeightAt(points[i].x, points[i].y);
        if (fabsf(repeated - first[i]) > 0.00001f) {
            (void)fprintf(stderr, "terrain seed did not reproduce sample %d\n",
                          i);
            return 1;
        }
    }
    if (!changed) {
        (void)fprintf(stderr,
                      "different terrain seeds made the same land (delta %.4f)\n",
                      largest_change);
        return 1;
    }
    return 0;
}

static int TestCountryHasRelief(void)
{
    float minimum = FLT_MAX;
    float maximum = -FLT_MAX;
    float road_minimum = FLT_MAX;
    float road_maximum = -FLT_MAX;
    int32_t sloped_samples = 0;
    for (float z = 1.0f; z < CC_LOCAL_WORLD_DEPTH; z += 2.0f) {
        for (float x = 1.0f; x < CC_LOCAL_WORLD_WIDTH; x += 2.0f) {
            float height = CcLocalTerrainHeightAt(x, z);
            minimum = fminf(minimum, height);
            maximum = fmaxf(maximum, height);
            Vector3 normal = CcLocalTerrainNormalAt(x, z);
            if (normal.y < 0.985f) sloped_samples += 1;

            if (normal.y < 0.18f) {
                (void)fprintf(stderr,
                              "terrain is too steep at %.1f %.1f (normal %.4f; L %.2f R %.2f N %.2f F %.2f)\n",
                              x, z, normal.y,
                              CcLocalTerrainHeightAt(x - 1.6f, z),
                              CcLocalTerrainHeightAt(x + 1.6f, z),
                              CcLocalTerrainHeightAt(x, z - 1.6f),
                              CcLocalTerrainHeightAt(x, z + 1.6f));
                return 1;
            }
        }
    }
    for (float x = 15.0f; x <= 91.0f; x += 2.0f) {
        float height = CcLocalTerrainHeightAt(x, 29.4f);
        road_minimum = fminf(road_minimum, height);
        road_maximum = fmaxf(road_maximum, height);
    }

    if (maximum - minimum < 5.0f || sloped_samples < 180 ||
        road_maximum - road_minimum < 0.60f) {
        (void)fprintf(stderr,
                      "terrain is still too flat: country %.2fm, road %.2fm, slopes %d\n",
                      maximum - minimum, road_maximum - road_minimum,
                      sloped_samples);
        return 1;
    }
    return 0;
}

static int TestSocietyUsesLocalFoundations(void)
{
    static const Rectangle pads[] = {
        {47.0f, 13.0f, 6.0f, 8.0f},
        {65.20f, 8.20f, 26.50f, 24.40f},
        {27.40f, 49.70f, 3.20f, 1.60f},
    };
    for (int32_t i = 0; i < (int32_t)(sizeof(pads) /
                                      sizeof(pads[0])); ++i) {
        Rectangle pad = pads[i];
        float center = CcLocalTerrainHeightAt(
            pad.x + pad.width * 0.5f, pad.y + pad.height * 0.5f);
        static const Vector2 inset[] = {
            {0.18f, 0.18f}, {0.82f, 0.18f},
            {0.18f, 0.82f}, {0.82f, 0.82f},
        };
        for (int32_t corner = 0; corner < 4; ++corner) {
            float height = CcLocalTerrainHeightAt(
                pad.x + pad.width * inset[corner].x,
                pad.y + pad.height * inset[corner].y);
            if (fabsf(height - center) > 0.035f) {
                (void)fprintf(stderr,
                              "foundation %d is not level: %.3f versus %.3f\n",
                              i, height, center);
                return 1;
            }
        }
    }
    return 0;
}

static int TestWalkingFollowsTheLand(void)
{
    const Vector2 start = {16.0f, 29.4f};
    const Vector2 end = {34.0f, 29.4f};
    CcLocalAgent walker;
    CcLocalAgentInit(&walker, start, false);
    walker.crowned = false;
    if (!CcLocalAgentSetExactTarget(
            &walker,
            (Vector3){end.x, CcLocalTerrainHeightAt(end.x, end.y), end.y},
            false)) {
        (void)fprintf(stderr, "hilly road target was rejected\n");
        return 1;
    }
    float minimum = walker.position.y;
    float maximum = walker.position.y;
    bool ragdolled = false;
    for (int32_t frame = 0; frame < 3600; ++frame) {
        CcLocalAgentUpdate(&walker, 1.0f / 60.0f, false);
        minimum = fminf(minimum, walker.position.y);
        maximum = fmaxf(maximum, walker.position.y);
        ragdolled = ragdolled || walker.traversal == CC_TRAVERSAL_RAGDOLL ||
                    walker.humanoid.ragdoll.active;
        if (!walker.exact_target_valid && !walker.climbing &&
            walker.humanoid.speed.value < 0.03f) break;
    }
    float dx = walker.position.x - end.x;
    float dz = walker.position.z - end.y;
    float expected = CcLocalTerrainHeightAt(walker.position.x,
                                             walker.position.z);
    if (sqrtf(dx * dx + dz * dz) > 0.16f || ragdolled || walker.climbing ||
        fabsf(walker.position.y - expected) > 0.035f ||
        maximum - minimum < 0.18f) {
        (void)fprintf(stderr,
                      "hill walk failed: pos %.2f %.2f y %.2f expected %.2f relief %.2f ragdoll %d\n",
                      walker.position.x, walker.position.z,
                      walker.position.y, expected, maximum - minimum,
                      ragdolled);
        return 1;
    }
    return 0;
}

static void TestFootstepSurfaces(void)
{
    CcLocalBindOpenWorld(NULL);
    CcLocalBindPlace(NULL);
    const uint32_t seeds[] = {UINT32_C(0xc0a71a9e), UINT32_C(0x12345678)};
    for (size_t i = 0; i < sizeof(seeds) / sizeof(seeds[0]); ++i) {
        CcLocalTerrainSetSeed(seeds[i]);
        CC_CHECK(CcLocalFootstepSurfaceAt(CC_LOCAL_SCENE_STREET, 24.0f, 28.0f) == CC_SOUND_STEP_STONE);
        CC_CHECK(CcLocalFootstepSurfaceAt(CC_LOCAL_SCENE_STREET, 24.0f, 32.8f) == CC_SOUND_STEP_GRASS);
        CC_CHECK(CcLocalFootstepSurfaceAt(CC_LOCAL_SCENE_STREET, 11.0f, 9.8f) == CC_SOUND_SPLASH);
        CC_CHECK(CcLocalFootstepSurfaceAt(CC_LOCAL_SCENE_STREET, 10.4f, 1.0f) == CC_SOUND_STEP_WOOD);
        CC_CHECK(CcLocalFootstepSurfaceAt(CC_LOCAL_SCENE_ROAD, 24.0f, 40.0f) == CC_SOUND_STEP_DIRT);
        CC_CHECK(CcLocalFootstepSurfaceAt(CC_LOCAL_SCENE_ROAD, 24.0f, 44.0f) == CC_SOUND_STEP_GRASS);
        CC_CHECK(CcLocalFootstepSurfaceAt(CC_LOCAL_SCENE_MARKET, 4.0f, 4.0f) == CC_SOUND_STEP_WOOD);
    }
    static CcSim sim;
    CcSimInit(&sim, UINT32_C(0xc0a71a9e));
    for (int32_t i = 0; i < sim.settlement_count; ++i) {
        if (sim.settlements[i].function != CC_SETTLEMENT_FARMING) continue;
        sim.player.location_id = sim.settlements[i].id;
        CcLocalBindPlace(&sim);
        CC_CHECK(CcLocalFootstepSurfaceAt(CC_LOCAL_SCENE_STREET, 24.0f, 29.4f) == CC_SOUND_STEP_DIRT);
        CC_CHECK(CcLocalFootstepSurfaceAt(CC_LOCAL_SCENE_STREET, 24.0f, 32.8f) == CC_SOUND_STEP_GRASS);
        break;
    }
    CcLocalBindPlace(NULL);
}

static void TestCurvedVillageRoads(void)
{
    static CcSim sim;
    CcSimInit(&sim, UINT32_C(0xc0a71a9e));
    CcLocalBindOpenWorld(NULL);
    sim.player.location_id = sim.settlements[0].id;
    CcLocalBindPlace(&sim);
    const CcLocalPlaceProfile *profile = CcLocalPlaceProfileForSettlement(&sim.settlements[0]);
    CC_CHECK(profile->lane_count > 0);
    for (int32_t lane = 0; lane < 3; ++lane) {
        const CcLocalLane *path = &profile->lane[lane];
        CcLocalLanePoint previous = CcLocalLaneSample(path, 0.0f);
        for (int32_t i = 1; i <= 180; ++i) {
            CcLocalLanePoint point = CcLocalLaneSample(path, (float)i / 180.0f);
            CC_CHECK(CcLocalFootstepSurfaceAt(CC_LOCAL_SCENE_STREET, point.x, point.z) == CC_SOUND_STEP_DIRT);
            Vector3 start = {previous.x, CcLocalTerrainHeightAt(previous.x, previous.z), previous.z};
            Vector3 end = {point.x, CcLocalTerrainHeightAt(point.x, point.z), point.z};
            Vector3 corrected, normal;
            CC_CHECK(!CcLocalMoveCapsuleInternal(CC_LOCAL_SCENE_STREET,
                start, end, 0.24f, &corrected, &normal));
            CC_CHECK(CcLocalTerrainNormalAt(point.x, point.z).y > 0.82f);
            previous = point;
        }
    }
    CC_CHECK(CcLocalFootstepSurfaceAt(CC_LOCAL_SCENE_STREET, 78.5f, 50.0f) == CC_SOUND_STEP_GRASS);
    CC_CHECK(CcLocalFootstepSurfaceAt(CC_LOCAL_SCENE_STREET, 41.5f, 15.0f) == CC_SOUND_STEP_GRASS);
    Vector2 arrival[CC_LOCAL_CARRIAGE_PATH_POINT_CAPACITY];
    Vector2 departure[CC_LOCAL_CARRIAGE_PATH_POINT_CAPACITY];
    int32_t count = CcLocalTownCarriagePath(true, arrival, CC_LOCAL_CARRIAGE_PATH_POINT_CAPACITY);
    CC_CHECK(count > 2);
    CC_CHECK(CcLocalTownCarriagePath(false, departure, CC_LOCAL_CARRIAGE_PATH_POINT_CAPACITY) == count);
    CC_CHECK(fabsf(arrival[0].x - CC_LOCAL_TOWN_GATE_X) < 0.001f);
    CC_CHECK(fabsf(arrival[count - 1].x - CC_LOCAL_CARRIAGE_X) < 0.001f);
    CC_CHECK(fabsf(arrival[count - 1].y - CC_LOCAL_CARRIAGE_Z) < 0.001f);
    for (int32_t i = 0; i < count; ++i) {
        CC_CHECK(hypotf(arrival[i].x - departure[count - 1 - i].x,
                         arrival[i].y - departure[count - 1 - i].y) < 0.001f);
        CC_CHECK(CcLocalFootstepSurfaceAt(CC_LOCAL_SCENE_STREET,
            arrival[i].x, arrival[i].y) == CC_SOUND_STEP_DIRT);
    }
    CcLocalBindPlace(NULL);
}

static void TestGloamgateMarketRoutes(void)
{
    static CcSim sim;
    const uint32_t seeds[] = {UINT32_C(0xc0a71a9e), UINT32_C(0x12345678)};
    for (int32_t seed = 0; seed < 2; ++seed) {
        CcSimInit(&sim, seeds[seed]);
        sim.player.location_id = sim.settlements[1].id;
        CcLocalBindPlace(&sim);
        const CcLocalPlaceProfile *profile = CcLocalPlaceProfileForSettlement(&sim.settlements[1]);
        /* All service lanes and the ring remain walkable, including their bends. */
        for (int32_t lane = 0; lane < 7; ++lane) {
            const CcLocalLane *path = &profile->lane[lane];
            CcLocalLanePoint previous = CcLocalLaneSample(path, 0.0f);
            for (int32_t sample = 1; sample <= 160; ++sample) {
                CcLocalLanePoint point = CcLocalLaneSample(path, (float)sample / 160.0f);
                Vector3 start = {previous.x, CcLocalTerrainHeightAt(previous.x, previous.z), previous.z};
                Vector3 end = {point.x, CcLocalTerrainHeightAt(point.x, point.z), point.z};
                Vector3 corrected, normal;
                if (CcLocalMoveCapsuleInternal(CC_LOCAL_SCENE_STREET,
                    start, end, 0.24f, &corrected, &normal)) {
                    fprintf(stderr, "Gloamgate lane %d blocked at %.2f %.2f\n", lane, point.x, point.z);
                    CC_CHECK(false);
                }
                CC_CHECK(CcLocalFootstepSurfaceAt(CC_LOCAL_SCENE_STREET,
                    point.x, point.z) == CC_SOUND_STEP_STONE);
                previous = point;
            }
        }
        Vector2 arrival[CC_LOCAL_CARRIAGE_PATH_POINT_CAPACITY];
        Vector2 departure[CC_LOCAL_CARRIAGE_PATH_POINT_CAPACITY];
        int32_t count = CcLocalTownCarriagePath(true, arrival, CC_LOCAL_CARRIAGE_PATH_POINT_CAPACITY);
        CC_CHECK(count > 2);
        CC_CHECK(CcLocalTownCarriagePath(false, departure, CC_LOCAL_CARRIAGE_PATH_POINT_CAPACITY) == count);
        CC_CHECK(fabsf(arrival[0].x - CC_LOCAL_TOWN_GATE_X) < 0.001f);
        CC_CHECK(hypotf(arrival[count - 1].x - CC_LOCAL_CARRIAGE_X,
                        arrival[count - 1].y - CC_LOCAL_CARRIAGE_Z) < 0.001f);
        for (int32_t i = 0; i < count; ++i) {
            CC_CHECK(hypotf(arrival[i].x - departure[count - 1 - i].x,
                            arrival[i].y - departure[count - 1 - i].y) < 0.001f);
            CC_CHECK(hypotf(arrival[i].x - 46.0f, arrival[i].y - 31.5f) > 4.0f);
            if (i > 0) CC_CHECK(hypotf(arrival[i].x - arrival[i-1].x,
                                       arrival[i].y - arrival[i-1].y) < 2.0f);
        }
        /* Both ordinary movement and the body collision probe see the basin. */
        Vector2 blocked = CcLocalMove((Vector2){46,34.5f}, (Vector2){0,-2.0f}, false);
        CC_CHECK(blocked.y > 32.85f);
        float ground = CcLocalTerrainHeightAt(46,31.5f);
        Vector3 corrected, normal;
        CC_CHECK(CcLocalProbePhysicsSphereInternal(CC_LOCAL_SCENE_STREET,
            (Vector3){46,ground+0.4f,34}, (Vector3){46,ground+0.4f,32.7f},
            0.16f, &corrected, &normal));
        CC_CHECK(corrected.z >= 33.0f);
    }
    CcLocalBindPlace(NULL);
}

int main(void)
{
    if (TestSeededTerrain() != 0) return 1;
    if (TestCountryHasRelief() != 0) return 1;
    if (TestSocietyUsesLocalFoundations() != 0) return 1;
    if (TestWalkingFollowsTheLand() != 0) return 1;
    TestFootstepSurfaces();
    TestCurvedVillageRoads();
    TestGloamgateMarketRoutes();
    return 0;
}
