#include "client/cc_local3d.h"
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
        {47.0f, 17.0f, 6.0f, 8.0f},
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
        CC_CHECK(CcLocalFootstepSurfaceAt(CC_LOCAL_SCENE_STREET, 24.0f, 29.4f) == CC_SOUND_STEP_STONE);
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

int main(void)
{
    if (TestSeededTerrain() != 0) return 1;
    if (TestCountryHasRelief() != 0) return 1;
    if (TestSocietyUsesLocalFoundations() != 0) return 1;
    if (TestWalkingFollowsTheLand() != 0) return 1;
    TestFootstepSurfaces();
    return 0;
}
