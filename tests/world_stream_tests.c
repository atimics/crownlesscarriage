#include "world/cc_world.h"

#include <math.h>
#include <stdio.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        (void)fprintf(stderr, "check failed at line %d: %s\n", \
                      __LINE__, #condition); \
        return 1; \
    } \
} while (0)

static int TestManifestIsStableAndFinite(void)
{
    CcSim first_sim;
    CcSim second_sim;
    CcSim other_sim;
    CcSimInit(&first_sim, UINT32_C(0x76543210));
    CcSimInit(&second_sim, UINT32_C(0x76543210));
    CcSimInit(&other_sim, UINT32_C(0x76543211));
    CcWorldManifest first;
    CcWorldManifest second;
    CcWorldManifest other;
    CHECK(CcWorldManifestBuild(&first, &first_sim));
    CHECK(CcWorldManifestBuild(&second, &second_sim));
    CHECK(CcWorldManifestBuild(&other, &other_sim));
    CHECK(first.settlement_count == first_sim.settlement_count);
    CHECK(first.route_count == first_sim.route_count);
    CHECK(first.maximum_x > first.minimum_x);
    CHECK(first.maximum_z > first.minimum_z);
    for (int32_t i = 0; i < first.settlement_count; ++i) {
        CHECK(first.settlements[i].settlement_id ==
              second.settlements[i].settlement_id);
        CHECK(first.settlements[i].center.x == second.settlements[i].center.x);
        CHECK(first.settlements[i].center.z == second.settlements[i].center.z);
        CHECK(first.settlements[i].profile_scale ==
              second.settlements[i].profile_scale);
        CHECK(first.settlements[i].entrance_heading_yaw ==
              second.settlements[i].entrance_heading_yaw);
        CHECK(CcWorldManifestContains(
            &first, first.settlements[i].center.x,
            first.settlements[i].center.z));
    }
    bool changed = first.settlements[0].center.x != other.settlements[0].center.x ||
                   first.settlements[0].center.z != other.settlements[0].center.z ||
                   first.settlements[0].plateau_height !=
                       other.settlements[0].plateau_height;
    CHECK(changed);
    return 0;
}

static int TestChunkSeamsMatch(void)
{
    CcSim sim;
    CcWorldStream stream;
    CcSimInit(&sim, UINT32_C(0xc0a71a9e));
    CHECK(CcWorldStreamInit(&stream, &sim));
    const CcWorldSettlementPlacement *start =
        CcWorldSettlementPlacementForId(
            &stream.manifest, sim.player.location_id);
    CHECK(start != NULL);
    int32_t chunk_x = (int32_t)floorf(start->center.x / CC_WORLD_CHUNK_SIZE);
    int32_t chunk_z = (int32_t)floorf(start->center.z / CC_WORLD_CHUNK_SIZE);
    const CcWorldChunk *left = CcWorldStreamChunkAt(
        &stream, chunk_x, chunk_z);
    const CcWorldChunk *right = CcWorldStreamChunkAt(
        &stream, chunk_x + 1, chunk_z);
    CHECK(left != NULL && left->state == CC_WORLD_CHUNK_READY);
    CHECK(right != NULL && right->state == CC_WORLD_CHUNK_READY);
    for (int32_t row = 0; row < CC_WORLD_CHUNK_SAMPLES; ++row) {
        float left_height = left->heights[
            row * CC_WORLD_CHUNK_SAMPLES + CC_WORLD_CHUNK_CELLS];
        float right_height = right->heights[row * CC_WORLD_CHUNK_SAMPLES];
        CHECK(fabsf(left_height - right_height) < 0.00001f);
    }
    return 0;
}

static int TestStreamingEvictsOldChunks(void)
{
    CcSim sim;
    CcWorldStream stream;
    CcSimInit(&sim, UINT32_C(42));
    CHECK(CcWorldStreamInit(&stream, &sim));
    CHECK(CcWorldStreamResidentCount(&stream) <= CC_WORLD_STREAM_CAPACITY);
    uint64_t generated_before = stream.generated_chunks;
    float far_x = stream.manifest.maximum_x - CC_WORLD_CHUNK_SIZE;
    float far_z = stream.manifest.maximum_z - CC_WORLD_CHUNK_SIZE;
    CcWorldStreamUpdate(&stream, far_x, far_z, CC_WORLD_STREAM_CAPACITY);
    CHECK(CcWorldStreamResidentCount(&stream) <= CC_WORLD_STREAM_CAPACITY);
    CHECK(CcWorldStreamPendingCount(&stream) == 0);
    CHECK(stream.generated_chunks > generated_before);
    CHECK(stream.evicted_chunks > 0U);
    CHECK(CcWorldStreamResidentBytes(&stream) <=
          (size_t)CC_WORLD_STREAM_CAPACITY * sizeof(CcWorldChunk));
    return 0;
}

static int TestRoadAndSettlementSurface(void)
{
    CcSim sim;
    CcWorldManifest manifest;
    CcSimInit(&sim, UINT32_C(87));
    CHECK(CcWorldManifestBuild(&manifest, &sim));
    CHECK(manifest.route_count > 0);
    CcWorldPoint road = CcWorldRoutePoint(&manifest.routes[0], 0.5f);
    CHECK(CcWorldSurfaceAt(&manifest, road.x, road.z) ==
          CC_WORLD_SURFACE_ROAD);
    CcWorldPoint town = manifest.settlements[0].center;
    CHECK(CcWorldSurfaceAt(&manifest, town.x, town.z) ==
          CC_WORLD_SURFACE_SETTLEMENT);
    float direct = CcWorldTerrainHeight(&manifest, road.x, road.z);
    CcWorldStream stream;
    CHECK(CcWorldStreamInit(&stream, &sim));
    CcWorldStreamUpdate(&stream, road.x, road.z, CC_WORLD_STREAM_CAPACITY);
    float streamed = CcWorldStreamHeightAt(&stream, road.x, road.z);
    CHECK(fabsf(direct - streamed) < 0.025f);
    return 0;
}

static int TestCarriagePoseFollowsRouteDirection(void)
{
    CcSim sim;
    CcWorldManifest manifest;
    CcSimInit(&sim, UINT32_C(0xca771a9e));
    CHECK(CcWorldManifestBuild(&manifest, &sim));
    CHECK(manifest.route_count > 0);
    const CcWorldRoutePlacement *route = &manifest.routes[0];
    CcWorldPoint forward_start;
    CcWorldPoint forward_end;
    CcWorldPoint reverse_start;
    float forward_yaw = 0.0f;
    float end_yaw = 0.0f;
    float reverse_yaw = 0.0f;
    CHECK(CcWorldRoutePose(route, route->from_id, 0.0f,
                           &forward_start, &forward_yaw));
    CHECK(CcWorldRoutePose(route, route->from_id, 1.0f,
                           &forward_end, &end_yaw));
    CHECK(CcWorldRoutePose(route, route->to_id, 0.0f,
                           &reverse_start, &reverse_yaw));
    CHECK(fabsf(forward_start.x - route->samples[0].x) < 0.0001f);
    CHECK(fabsf(forward_start.z - route->samples[0].z) < 0.0001f);
    CHECK(fabsf(forward_end.x -
                route->samples[CC_WORLD_ROUTE_SAMPLE_COUNT - 1].x) <
          0.0001f);
    CHECK(fabsf(forward_end.z -
                route->samples[CC_WORLD_ROUTE_SAMPLE_COUNT - 1].z) <
          0.0001f);
    CHECK(fabsf(reverse_start.x - forward_end.x) < 0.0001f);
    CHECK(fabsf(reverse_start.z - forward_end.z) < 0.0001f);
    float opposite_x = sinf(end_yaw) + sinf(reverse_yaw);
    float opposite_z = cosf(end_yaw) + cosf(reverse_yaw);
    CHECK(opposite_x * opposite_x + opposite_z * opposite_z < 0.08f);
    CHECK(CcWorldRouteLength(route) > 1.0f);

    const CcWorldSettlementPlacement *town =
        CcWorldSettlementPlacementForId(&manifest, route->from_id);
    CHECK(town != NULL);
    CcWorldPoint town_center = CcWorldSettlementLocalPoint(
        &manifest, town->settlement_id, 48.0f, 36.0f);
    CcWorldPoint town_gate = CcWorldSettlementLocalPoint(
        &manifest, town->settlement_id, 96.0f, 36.0f);
    CHECK(fabsf(town_center.x - town->center.x) < 0.0001f);
    CHECK(fabsf(town_center.z - town->center.z) < 0.0001f);
    float gate_x = town_gate.x - town_center.x;
    float gate_z = town_gate.z - town_center.z;
    CHECK(gate_x * sinf(forward_yaw) + gate_z * cosf(forward_yaw) > 0.0f);
    return 0;
}

int main(void)
{
    if (TestManifestIsStableAndFinite() != 0) return 1;
    if (TestChunkSeamsMatch() != 0) return 1;
    if (TestStreamingEvictsOldChunks() != 0) return 1;
    if (TestRoadAndSettlementSurface() != 0) return 1;
    if (TestCarriagePoseFollowsRouteDirection() != 0) return 1;
    puts("Finite world streaming tests passed");
    return 0;
}
