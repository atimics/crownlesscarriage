#include "world/cc_world.h"

#include <math.h>
#include <float.h>
#include <stdio.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        (void)fprintf(stderr, "check failed at line %d: %s\n", \
                      __LINE__, #condition); \
        return 1; \
    } \
} while (0)

static float PointDistance(CcWorldPoint first, CcWorldPoint second)
{
    float dx = first.x - second.x;
    float dz = first.z - second.z;
    return sqrtf(dx * dx + dz * dz);
}

static float DistanceToSegment(CcWorldPoint point, CcWorldPoint first,
                               CcWorldPoint second)
{
    float dx = second.x - first.x;
    float dz = second.z - first.z;
    float length_squared = dx * dx + dz * dz;
    float amount = length_squared > 0.0001f ?
        ((point.x - first.x) * dx + (point.z - first.z) * dz) /
            length_squared : 0.0f;
    amount = fmaxf(0.0f, fminf(1.0f, amount));
    CcWorldPoint nearest = {
        first.x + dx * amount,
        first.z + dz * amount,
    };
    return PointDistance(point, nearest);
}

/* Exhaustive reference keeps query results stable as distant roads are culled. */
static int CheckRoadQuery(const CcWorldManifest *manifest,
                         const CcWorldManifest *without_roads,
                         CcWorldPoint point)
{
    float nearest = FLT_MAX;
    CcWorldPoint center = {0};
    for (int32_t r = 0; r < manifest->route_count; ++r) {
        for (int32_t s = 1; s < CC_WORLD_ROUTE_SAMPLE_COUNT; ++s) {
            CcWorldPoint a = manifest->routes[r].samples[s - 1];
            CcWorldPoint b = manifest->routes[r].samples[s];
            float distance = DistanceToSegment(point, a, b);
            if (distance >= nearest) continue;
            nearest = distance;
            float dx = b.x - a.x;
            float dz = b.z - a.z;
            float length_squared = dx * dx + dz * dz;
            float t = length_squared > 0.0001f ?
                ((point.x - a.x) * dx + (point.z - a.z) * dz) /
                    length_squared : 0.0f;
            t = fmaxf(0.0f, fminf(1.0f, t));
            center = (CcWorldPoint){a.x + dx * t, a.z + dz * t};
        }
    }
    float expected = CcWorldTerrainHeight(without_roads, point.x, point.z);
    if (nearest < 6.7f) {
        float road = CcWorldTerrainHeight(without_roads, center.x, center.z);
        float t = fmaxf(0.0f, fminf(1.0f, (nearest - 3.2f) / 3.5f));
        expected += (road - expected) * (1.0f - t * t * (3.0f - 2.0f * t));
    }
    CHECK(fabsf(CcWorldTerrainHeight(manifest, point.x, point.z) - expected) <
          0.000001f);
    CcWorldSurfaceKind surface = CcWorldSurfaceAt(without_roads, point.x, point.z);
    if (surface == CC_WORLD_SURFACE_WILDERNESS && nearest <= 3.2f) {
        surface = CC_WORLD_SURFACE_ROAD;
    }
    CHECK(CcWorldSurfaceAt(manifest, point.x, point.z) == surface);
    return 0;
}

static int TestRoadQueryParity(void)
{
    for (uint32_t seed = 1; seed <= 4; ++seed) {
        CcSim sim;
        CcWorldManifest manifest;
        CcSimInit(&sim, seed * UINT32_C(0x9e3779b9));
        CHECK(CcWorldManifestBuild(&manifest, &sim));
        CcWorldManifest without_roads = manifest;
        without_roads.route_count = 0;
        for (int32_t row = 0; row < 64; ++row) {
            for (int32_t col = 0; col < 64; ++col) {
                CcWorldPoint point = {
                    manifest.minimum_x + (manifest.maximum_x - manifest.minimum_x) *
                        ((float)col + 0.5f) / 64.0f,
                    manifest.minimum_z + (manifest.maximum_z - manifest.minimum_z) *
                        ((float)row + 0.5f) / 64.0f,
                };
                CHECK(CheckRoadQuery(&manifest, &without_roads, point) == 0);
            }
        }
        const float offsets[] = {-6.7001f, -6.7f, -3.2001f, -3.2f, 0.0f,
                                   3.2f, 3.2001f, 6.7f, 6.7001f};
        for (int32_t r = 0; r < manifest.route_count; ++r) {
            for (int32_t s = 0; s < CC_WORLD_ROUTE_SAMPLE_COUNT; ++s) {
                for (size_t offset = 0; offset < sizeof(offsets) / sizeof(offsets[0]);
                     ++offset) {
                    CcWorldPoint point = manifest.routes[r].samples[s];
                    point.x += offsets[offset];
                    CHECK(CheckRoadQuery(&manifest, &without_roads, point) == 0);
                    point = manifest.routes[r].samples[s];
                    point.z += offsets[offset];
                    CHECK(CheckRoadQuery(&manifest, &without_roads, point) == 0);
                }
            }
        }
    }
    CcWorldManifest straight = {.route_count = 1};
    for (int32_t s = 0; s < CC_WORLD_ROUTE_SAMPLE_COUNT; ++s) {
        straight.routes[0].samples[s] = (CcWorldPoint){(float)(s - 16) * 2.0f, 0};
    }
    CHECK(CcWorldSurfaceAt(&straight, 0, 3.2f) == CC_WORLD_SURFACE_ROAD);
    CHECK(CcWorldSurfaceAt(&straight, 0, nextafterf(3.2f, INFINITY)) ==
          CC_WORLD_SURFACE_WILDERNESS);
    CHECK(CcWorldSurfaceAt(&straight, 0, -3.2f) == CC_WORLD_SURFACE_ROAD);
    return 0;
}

static int TestCanonicalRoadManifest(void)
{
    static const int32_t expected_map[][2] = {
        {124, 489}, {353, 449}, {529, 337},
        {759, 463}, {785, 158}, {316, 141},
    };
    CcSim sim;
    CcWorldManifest manifest;
    CcSimInit(&sim, UINT32_C(0xc0a7118e));
    CHECK(sim.generator_version == CC_GENERATOR_VERSION);
    CHECK(sim.settlement_count == 6);
    for (int32_t index = 0; index < sim.settlement_count; ++index) {
        CHECK(sim.settlements[index].map_x == expected_map[index][0]);
        CHECK(sim.settlements[index].map_y == expected_map[index][1]);
    }
    CHECK(CcWorldManifestBuild(&manifest, &sim));
    CHECK(manifest.generator_version == CC_GENERATOR_VERSION);
    CHECK(fabsf(manifest.settlements[1].junction.x - 264.997284f) < 0.0001f);
    CHECK(fabsf(manifest.settlements[1].junction.z - 242.611099f) < 0.0001f);
    CHECK(fabsf(manifest.routes[0].control.x - 147.306854f) < 0.0001f);
    CHECK(fabsf(manifest.routes[0].control.z - 306.920654f) < 0.0001f);
    CHECK(fabsf(manifest.routes[0]
                    .samples[CC_WORLD_ROUTE_FROM_JUNCTION_SAMPLE].x -
                119.164772f) < 0.0001f);
    CHECK(fabsf(manifest.routes[0]
                    .samples[CC_WORLD_ROUTE_TO_JUNCTION_SAMPLE].x -
                264.997284f) < 0.0001f);
    return 0;
}

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
    CHECK(first.road_site_count == first_sim.road_site_count);
    CHECK(first.maximum_x > first.minimum_x);
    CHECK(first.maximum_z > first.minimum_z);
    for (int32_t i = 0; i < first.settlement_count; ++i) {
        CHECK(first.settlements[i].settlement_id ==
              second.settlements[i].settlement_id);
        CHECK(first.settlements[i].center.x == second.settlements[i].center.x);
        CHECK(first.settlements[i].center.z == second.settlements[i].center.z);
        CHECK(first.settlements[i].gate.x == second.settlements[i].gate.x);
        CHECK(first.settlements[i].gate.z == second.settlements[i].gate.z);
        CHECK(first.settlements[i].junction.x ==
              second.settlements[i].junction.x);
        CHECK(first.settlements[i].junction.z ==
              second.settlements[i].junction.z);
        CHECK(first.settlements[i].profile_scale ==
              second.settlements[i].profile_scale);
        CHECK(first.settlements[i].entrance_heading_yaw ==
              second.settlements[i].entrance_heading_yaw);
        CHECK(CcWorldManifestContains(
            &first, first.settlements[i].center.x,
            first.settlements[i].center.z));
        CHECK(CcWorldManifestContains(
            &first, first.settlements[i].gate.x,
            first.settlements[i].gate.z));
        CHECK(CcWorldManifestContains(
            &first, first.settlements[i].junction.x,
            first.settlements[i].junction.z));
    }
    for (int32_t i = 0; i < first.route_count; ++i) {
        CHECK(first.routes[i].route_id == second.routes[i].route_id);
        for (int32_t sample = 0;
             sample < CC_WORLD_ROUTE_SAMPLE_COUNT; ++sample) {
            CHECK(first.routes[i].samples[sample].x ==
                  second.routes[i].samples[sample].x);
            CHECK(first.routes[i].samples[sample].z ==
                  second.routes[i].samples[sample].z);
        }
    }
    for (int32_t i = 0; i < first.road_site_count; ++i) {
        CHECK(first.road_sites[i].road_site_id ==
              second.road_sites[i].road_site_id);
        CHECK(first.road_sites[i].junction.x ==
              second.road_sites[i].junction.x);
        CHECK(first.road_sites[i].junction.z ==
              second.road_sites[i].junction.z);
        CHECK(first.road_sites[i].blocker_position.x ==
              second.road_sites[i].blocker_position.x);
        CHECK(first.road_sites[i].blocker_position.z ==
              second.road_sites[i].blocker_position.z);
        CHECK(first.road_sites[i].destination.x ==
              second.road_sites[i].destination.x);
        CHECK(first.road_sites[i].destination.z ==
              second.road_sites[i].destination.z);
    }
    bool changed = first.settlements[0].center.x != other.settlements[0].center.x ||
                   first.settlements[0].center.z != other.settlements[0].center.z ||
                   first.settlements[0].plateau_height !=
                       other.settlements[0].plateau_height;
    CHECK(changed);
    return 0;
}

static int TestRoadDistrictSites(void)
{
    CcSim sim;
    CcWorldManifest manifest;
    CcSimInit(&sim, UINT32_C(0x51de40ad));
    CHECK(CcWorldManifestBuild(&manifest, &sim));
    CHECK(sim.road_site_count == CC_MAX_ROAD_SITES);
    CHECK(manifest.road_site_count == sim.road_site_count);
    for (int32_t route_slot = 0; route_slot < sim.route_count; ++route_slot) {
        int32_t route_sites = 0;
        int32_t road_houses = 0;
        for (int32_t site_slot = 0;
             site_slot < sim.road_site_count; ++site_slot) {
            const CcRoadSite *site = &sim.road_sites[site_slot];
            if (site->route_id != sim.routes[route_slot].id) continue;
            route_sites += 1;
            if (site->kind == CC_ROAD_SITE_ROAD_HOUSE) road_houses += 1;
        }
        CHECK(route_sites == 3);
        CHECK(road_houses == 1);
    }
    for (int32_t i = 0; i < sim.road_site_count; ++i) {
        const CcRoadSite *site = &sim.road_sites[i];
        const CcWorldRoadSitePlacement *placement =
            CcWorldRoadSitePlacementForId(&manifest, site->id);
        const CcWorldRoutePlacement *route =
            CcWorldRoutePlacementForId(&manifest, site->route_id);
        CHECK(placement != NULL);
        CHECK(route != NULL);
        CcWorldPoint expected_junction = CcWorldRoutePoint(
            route, CcWorldRouteJourneyAmount(
                route, route->from_id,
                (float)site->progress_milli / 1000.0f));
        CHECK(PointDistance(expected_junction, placement->junction) < 0.001f);
        float blocker_distance = PointDistance(
            placement->junction, placement->blocker_position);
        float destination_distance = PointDistance(
            placement->junction, placement->destination);
        CHECK(blocker_distance > 8.0f);
        CHECK(blocker_distance < destination_distance);
        CHECK(isfinite(placement->blocker_heading_yaw));
        CHECK(CcWorldManifestContains(
            &manifest, placement->destination.x,
            placement->destination.z));
        CHECK(site->blocker == CC_ROAD_SITE_BLOCKER_TREE ||
              site->blocker == CC_ROAD_SITE_BLOCKER_ROCKS);
        CHECK(!site->accessible);
    }
    return 0;
}

static int TestRoutesShareAuthoredGateConnectors(void)
{
    CcSim sim;
    CcWorldManifest manifest;
    CcSimInit(&sim, UINT32_C(42));
    CHECK(CcWorldManifestBuild(&manifest, &sim));
    bool found_multi_route_town = false;
    for (int32_t settlement_index = 0;
         settlement_index < manifest.settlement_count; ++settlement_index) {
        const CcWorldSettlementPlacement *settlement =
            &manifest.settlements[settlement_index];
        CcWorldPoint authored_gate = CcWorldSettlementLocalPoint(
            &manifest, settlement->settlement_id, 96.0f, 36.0f);
        CHECK(PointDistance(authored_gate, settlement->gate) < 0.0001f);
        CHECK(PointDistance(settlement->center, settlement->junction) >
              settlement->radius);
        float connector_dx = settlement->junction.x - settlement->gate.x;
        float connector_dz = settlement->junction.z - settlement->gate.z;
        float connector_length = sqrtf(
            connector_dx * connector_dx + connector_dz * connector_dz);
        CHECK(connector_length > 1.0f);
        connector_dx /= connector_length;
        connector_dz /= connector_length;
        CHECK(connector_dx * sinf(settlement->entrance_heading_yaw) +
              connector_dz * cosf(settlement->entrance_heading_yaw) >
              0.999f);

        int32_t incident_count = 0;
        CcWorldPoint branch_gate = {0};
        float branch_heading = 0.0f;
        for (int32_t route_index = 0;
             route_index < sim.route_count; ++route_index) {
            const CcRoute *route = &sim.routes[route_index];
            if (route->from_id != settlement->settlement_id &&
                route->to_id != settlement->settlement_id) continue;
            const CcWorldRoutePlacement *placement =
                CcWorldRoutePlacementForId(&manifest, route->id);
            CHECK(placement != NULL);
            bool from = placement->from_id == settlement->settlement_id;
            int32_t gate_sample = from ? 0 :
                CC_WORLD_ROUTE_SAMPLE_COUNT - 1;
            int32_t junction_sample = from ?
                CC_WORLD_ROUTE_FROM_JUNCTION_SAMPLE :
                CC_WORLD_ROUTE_TO_JUNCTION_SAMPLE;
            CHECK(PointDistance(placement->samples[gate_sample],
                                settlement->gate) < 0.0001f);
            CHECK(PointDistance(placement->samples[junction_sample],
                                settlement->junction) < 0.0001f);
            CcWorldPoint pose;
            float heading = 0.0f;
            CHECK(CcWorldRoutePose(placement, settlement->settlement_id,
                                   0.0f, &pose, &heading));
            CHECK(PointDistance(pose, settlement->gate) < 0.0001f);
            CHECK(fabsf(sinf(heading) -
                        sinf(settlement->entrance_heading_yaw)) < 0.0001f);
            CHECK(fabsf(cosf(heading) -
                        cosf(settlement->entrance_heading_yaw)) < 0.0001f);
            if (incident_count == 0) {
                branch_gate = pose;
                branch_heading = heading;
            } else {
                CHECK(PointDistance(branch_gate, pose) < 0.0001f);
                CHECK(fabsf(sinf(branch_heading) - sinf(heading)) < 0.0001f);
                CHECK(fabsf(cosf(branch_heading) - cosf(heading)) < 0.0001f);
            }
            incident_count += 1;
        }
        CHECK(incident_count > 0);
        if (incident_count > 1) found_multi_route_town = true;
    }
    CHECK(found_multi_route_town);

    for (int32_t route_index = 0;
         route_index < manifest.route_count; ++route_index) {
        const CcWorldRoutePlacement *route = &manifest.routes[route_index];
        float previous_amount = 0.0f;
        for (int32_t sample = 1;
             sample < CC_WORLD_ROUTE_SAMPLE_COUNT; ++sample) {
            float amount = CcWorldRouteSampleAmount(route, sample);
            CHECK(amount > previous_amount);
            CcWorldPoint point = CcWorldRoutePoint(route, amount);
            CHECK(PointDistance(point, route->samples[sample]) < 0.001f);
            previous_amount = amount;
        }
    }
    return 0;
}

static bool RouteCorridorsClearTownFootprints(
    const CcWorldManifest *manifest)
{
    if (manifest == NULL) return false;
    for (int32_t route_index = 0;
         route_index < manifest->route_count; ++route_index) {
        const CcWorldRoutePlacement *route = &manifest->routes[route_index];
        for (int32_t sample = 0;
             sample < CC_WORLD_ROUTE_SAMPLE_COUNT; ++sample) {
            if (!CcWorldManifestContains(
                    manifest, route->samples[sample].x,
                    route->samples[sample].z)) return false;
        }
        for (int32_t sample = CC_WORLD_ROUTE_FROM_JUNCTION_SAMPLE;
             sample < CC_WORLD_ROUTE_TO_JUNCTION_SAMPLE; ++sample) {
            for (int32_t settlement_index = 0;
                 settlement_index < manifest->settlement_count;
                 ++settlement_index) {
                const CcWorldSettlementPlacement *settlement =
                    &manifest->settlements[settlement_index];
                float distance = DistanceToSegment(
                    settlement->center, route->samples[sample],
                    route->samples[sample + 1]);
                bool corridor = sample >=
                                    CC_WORLD_ROUTE_FROM_CORRIDOR_SAMPLE &&
                                sample <
                                    CC_WORLD_ROUTE_TO_CORRIDOR_SAMPLE;
                float minimum = settlement->radius +
                    (corridor ? 15.0f : 0.0f);
                if (distance < minimum) {
                    (void)fprintf(
                        stderr,
                        "route %d segment %d crossed town %d: %.2f/%.2f\n",
                        route_index, sample, settlement_index, distance,
                        minimum);
                    (void)fprintf(
                        stderr,
                        "segment %.2f,%.2f to %.2f,%.2f; town %.2f,%.2f\n",
                        route->samples[sample].x,
                        route->samples[sample].z,
                        route->samples[sample + 1].x,
                        route->samples[sample + 1].z,
                        settlement->center.x, settlement->center.z);
                    return false;
                }
            }
        }
    }
    return true;
}

static int TestRouteTopologyAcrossSeeds(void)
{
    for (uint32_t seed = 0U; seed < 4096U; ++seed) {
        CcSim sim;
        CcWorldManifest manifest;
        CcSimInit(&sim, seed * UINT32_C(0x9e3779b9));
        CHECK(CcWorldManifestBuild(&manifest, &sim));
        for (int32_t settlement_index = 0;
             settlement_index < manifest.settlement_count;
             ++settlement_index) {
            const CcWorldSettlementPlacement *settlement =
                &manifest.settlements[settlement_index];
            CHECK(CcWorldManifestContains(
                &manifest, settlement->junction.x, settlement->junction.z));
        }
        if (!RouteCorridorsClearTownFootprints(&manifest)) {
            (void)fprintf(stderr, "seed %u crossed a town footprint\n",
                          seed);
            return 1;
        }
    }
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

static int TestStreamFollowsCarriage(void)
{
    CcSim sim;
    CcWorldStream stream;
    CcSimInit(&sim, 42U);
    CHECK(CcWorldStreamInit(&stream, &sim));
    const CcWorldRoutePlacement *route = &stream.manifest.routes[2];
    int32_t start_x = stream.focus_chunk_x;
    int32_t start_z = stream.focus_chunk_z;
    for (int32_t direction = 0; direction < 2; ++direction) {
        CcId origin = direction == 0 ? route->from_id : route->to_id;
        for (int32_t step = 0; step <= 100; ++step) {
            float amount = (float)step / 100.0f;
            CcWorldPoint point, ahead;
            float heading;
            CHECK(CcWorldRoutePose(route, origin, amount, &point, &heading));
            CHECK(CcWorldRoutePose(route, origin,
                fminf(1.0f, amount + CC_WORLD_CHUNK_SIZE / CcWorldRouteLength(route)),
                &ahead, &heading));
            uint64_t generated = stream.generated_chunks;
            CcWorldStreamFollowRoute(&stream, route, origin, amount, 4);
            CHECK(stream.generated_chunks - generated <= 4U);
            CHECK(stream.focus_chunk_x == (int32_t)floorf(point.x / CC_WORLD_CHUNK_SIZE));
            CHECK(stream.focus_chunk_z == (int32_t)floorf(point.z / CC_WORLD_CHUNK_SIZE));
            const CcWorldPoint points[] = {point, ahead};
            for (int32_t i = 0; i < 2; ++i) {
                const CcWorldChunk *chunk = CcWorldStreamChunkAt(&stream,
                    (int32_t)floorf(points[i].x / CC_WORLD_CHUNK_SIZE),
                    (int32_t)floorf(points[i].z / CC_WORLD_CHUNK_SIZE));
                CHECK(chunk != NULL && chunk->state == CC_WORLD_CHUNK_READY);
            }
            CHECK(CcWorldStreamResidentCount(&stream) + CcWorldStreamPendingCount(&stream) <=
                  CC_WORLD_STREAM_CAPACITY);
        }
    }
    CHECK(stream.focus_chunk_x != start_x || stream.focus_chunk_z != start_z);
    CHECK(stream.evicted_chunks > CC_WORLD_STREAM_CAPACITY);
    return 0;
}

int main(void)
{
    if (TestRoadQueryParity() != 0) return 1;
    if (TestStreamFollowsCarriage() != 0) return 1;
    if (TestCanonicalRoadManifest() != 0) return 1;
    if (TestManifestIsStableAndFinite() != 0) return 1;
    if (TestRoadDistrictSites() != 0) return 1;
    if (TestChunkSeamsMatch() != 0) return 1;
    if (TestStreamingEvictsOldChunks() != 0) return 1;
    if (TestRoadAndSettlementSurface() != 0) return 1;
    if (TestCarriagePoseFollowsRouteDirection() != 0) return 1;
    if (TestRoutesShareAuthoredGateConnectors() != 0) return 1;
    if (TestRouteTopologyAcrossSeeds() != 0) return 1;
    puts("Finite world streaming tests passed");
    return 0;
}
