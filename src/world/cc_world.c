#include "world/cc_world.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define CC_WORLD_MAP_SCALE 0.75f
#define CC_WORLD_MARGIN 64.0f
#define CC_WORLD_SETTLEMENT_RADIUS 38.0f
#define CC_WORLD_ROAD_HALF_WIDTH 3.2f
#define CC_WORLD_JUNCTION_CLEARANCE 18.0f
#define CC_WORLD_ROUTE_FANOUT_LENGTH 4.0f

static float ClampUnit(float value)
{
    return fmaxf(0.0f, fminf(1.0f, value));
}

static float SmoothStep(float amount)
{
    amount = ClampUnit(amount);
    return amount * amount * (3.0f - 2.0f * amount);
}

static uint32_t MixBits(uint32_t value)
{
    value ^= value >> 16U;
    value *= UINT32_C(0x7feb352d);
    value ^= value >> 15U;
    value *= UINT32_C(0x846ca68b);
    value ^= value >> 16U;
    return value;
}

static uint32_t WorldHash(uint32_t seed, int32_t x, int32_t z, uint32_t salt)
{
    uint32_t value = seed ^ salt;
    value ^= (uint32_t)x * UINT32_C(0x9e3779b9);
    value ^= (uint32_t)z * UINT32_C(0x85ebca6b);
    return MixBits(value);
}

static float HashSigned(uint32_t value)
{
    return (float)(value & UINT32_C(0x00ffffff)) /
               (float)UINT32_C(0x00ffffff) * 2.0f - 1.0f;
}

static float ValueNoise(uint32_t seed, float x, float z, float scale,
                        uint32_t salt)
{
    float sample_x = x / scale;
    float sample_z = z / scale;
    int32_t left = (int32_t)floorf(sample_x);
    int32_t near_row = (int32_t)floorf(sample_z);
    float tx = SmoothStep(sample_x - (float)left);
    float tz = SmoothStep(sample_z - (float)near_row);
    float near_left = HashSigned(WorldHash(seed, left, near_row, salt));
    float near_right = HashSigned(WorldHash(seed, left + 1, near_row, salt));
    float far_left = HashSigned(WorldHash(seed, left, near_row + 1, salt));
    float far_right = HashSigned(WorldHash(seed, left + 1, near_row + 1, salt));
    float near_value = near_left + (near_right - near_left) * tx;
    float far_value = far_left + (far_right - far_left) * tx;
    return near_value + (far_value - near_value) * tz;
}

static float BaseTerrainHeight(const CcWorldManifest *manifest,
                               float x, float z)
{
    uint32_t seed = manifest != NULL ? manifest->world_seed : 0U;
    float broad = ValueNoise(seed, x, z, 132.0f, UINT32_C(0x574f524c));
    float hills = ValueNoise(seed, x, z, 52.0f, UINT32_C(0x48494c4c));
    float detail = ValueNoise(seed, x, z, 19.0f, UINT32_C(0x44455441));
    return broad * 4.8f + hills * 2.3f + detail * 0.55f;
}

static float PointDistance(CcWorldPoint first, CcWorldPoint second)
{
    float dx = first.x - second.x;
    float dz = first.z - second.z;
    return sqrtf(dx * dx + dz * dz);
}

static float DistanceToSegment(CcWorldPoint point, CcWorldPoint first,
                               CcWorldPoint second, float *amount)
{
    float dx = second.x - first.x;
    float dz = second.z - first.z;
    float length_squared = dx * dx + dz * dz;
    float projection = length_squared > 0.0001f ?
        ((point.x - first.x) * dx + (point.z - first.z) * dz) /
            length_squared : 0.0f;
    projection = ClampUnit(projection);
    if (amount != NULL) *amount = projection;
    float nearest_x = first.x + dx * projection;
    float nearest_z = first.z + dz * projection;
    float offset_x = point.x - nearest_x;
    float offset_z = point.z - nearest_z;
    return sqrtf(offset_x * offset_x + offset_z * offset_z);
}

static int32_t ChunkCoordinate(float coordinate)
{
    return (int32_t)floorf(coordinate / CC_WORLD_CHUNK_SIZE);
}

static bool ChunkInsideManifest(const CcWorldManifest *manifest,
                                int32_t chunk_x, int32_t chunk_z)
{
    if (manifest == NULL) return false;
    float minimum_x = (float)chunk_x * CC_WORLD_CHUNK_SIZE;
    float minimum_z = (float)chunk_z * CC_WORLD_CHUNK_SIZE;
    float maximum_x = minimum_x + CC_WORLD_CHUNK_SIZE;
    float maximum_z = minimum_z + CC_WORLD_CHUNK_SIZE;
    return maximum_x >= manifest->minimum_x &&
           maximum_z >= manifest->minimum_z &&
           minimum_x <= manifest->maximum_x &&
           minimum_z <= manifest->maximum_z;
}

static float AlignDown(float value)
{
    return floorf(value / CC_WORLD_CHUNK_SIZE) * CC_WORLD_CHUNK_SIZE;
}

static float AlignUp(float value)
{
    return ceilf(value / CC_WORLD_CHUNK_SIZE) * CC_WORLD_CHUNK_SIZE;
}

float CcWorldRouteLength(const CcWorldRoutePlacement *route)
{
    if (route == NULL) return 0.0f;
    float length = 0.0f;
    for (int32_t sample = 0;
         sample < CC_WORLD_ROUTE_SAMPLE_COUNT - 1; ++sample) {
        length += PointDistance(route->samples[sample],
                                route->samples[sample + 1]);
    }
    return length;
}

float CcWorldRouteSampleAmount(const CcWorldRoutePlacement *route,
                               int32_t sample_index)
{
    if (route == NULL || sample_index <= 0) return 0.0f;
    if (sample_index >= CC_WORLD_ROUTE_SAMPLE_COUNT - 1) return 1.0f;
    float total = CcWorldRouteLength(route);
    if (total <= 0.0001f) return 0.0f;
    float length = 0.0f;
    for (int32_t sample = 0; sample < sample_index; ++sample) {
        length += PointDistance(route->samples[sample],
                                route->samples[sample + 1]);
    }
    return ClampUnit(length / total);
}

static int32_t RouteSegmentAtAmount(const CcWorldRoutePlacement *route,
                                    float amount, float *segment_amount)
{
    if (segment_amount != NULL) *segment_amount = 0.0f;
    if (route == NULL) return 0;
    float total = CcWorldRouteLength(route);
    if (total <= 0.0001f) return 0;
    float target = ClampUnit(amount) * total;
    float travelled = 0.0f;
    for (int32_t sample = 0;
         sample < CC_WORLD_ROUTE_SAMPLE_COUNT - 1; ++sample) {
        float segment = PointDistance(route->samples[sample],
                                      route->samples[sample + 1]);
        if (target <= travelled + segment ||
            sample == CC_WORLD_ROUTE_SAMPLE_COUNT - 2) {
            if (segment_amount != NULL && segment > 0.0001f) {
                *segment_amount = ClampUnit((target - travelled) / segment);
            }
            return sample;
        }
        travelled += segment;
    }
    return CC_WORLD_ROUTE_SAMPLE_COUNT - 2;
}

CcWorldPoint CcWorldRoutePoint(const CcWorldRoutePlacement *route, float amount)
{
    if (route == NULL) return (CcWorldPoint){0};
    float segment_amount = 0.0f;
    int32_t segment = RouteSegmentAtAmount(
        route, amount, &segment_amount);
    CcWorldPoint first = route->samples[segment];
    CcWorldPoint second = route->samples[segment + 1];
    return (CcWorldPoint){
        first.x + (second.x - first.x) * segment_amount,
        first.z + (second.z - first.z) * segment_amount,
    };
}

float CcWorldRouteJourneyAmount(const CcWorldRoutePlacement *route,
                                CcId origin_id, float progress)
{
    if (route == NULL ||
        (origin_id != route->from_id && origin_id != route->to_id)) {
        return 0.0f;
    }
    float from_junction_amount = CcWorldRouteSampleAmount(
        route, CC_WORLD_ROUTE_FROM_JUNCTION_SAMPLE);
    float to_junction_amount = CcWorldRouteSampleAmount(
        route, CC_WORLD_ROUTE_TO_JUNCTION_SAMPLE);
    bool forward = origin_id == route->from_id;
    float start_amount = forward ? from_junction_amount :
                                   1.0f - to_junction_amount;
    float end_amount = forward ? 1.0f - to_junction_amount :
                                 from_junction_amount;
    return start_amount + ClampUnit(progress) *
        fmaxf(0.0f, 1.0f - start_amount - end_amount);
}

bool CcWorldRoutePose(const CcWorldRoutePlacement *route, CcId origin_id,
                      float journey_amount, CcWorldPoint *position,
                      float *heading_yaw)
{
    if (route == NULL || position == NULL || heading_yaw == NULL ||
        (origin_id != route->from_id && origin_id != route->to_id)) {
        return false;
    }
    journey_amount = ClampUnit(journey_amount);
    bool reverse = origin_id == route->to_id;
    float route_amount = reverse ? 1.0f - journey_amount : journey_amount;
    *position = CcWorldRoutePoint(route, route_amount);

    float length = CcWorldRouteLength(route);
    float heading_window = length > 0.001f ?
        fminf(0.02f, 1.5f / length) : 0.0f;
    float before = fmaxf(0.0f, route_amount - heading_window);
    float after = fminf(1.0f, route_amount + heading_window);
    CcWorldPoint first = CcWorldRoutePoint(route, before);
    CcWorldPoint last = CcWorldRoutePoint(route, after);
    float dx = last.x - first.x;
    float dz = last.z - first.z;
    if (reverse) {
        dx = -dx;
        dz = -dz;
    }
    if (dx * dx + dz * dz < 0.000001f) {
        CcWorldPoint route_first = route->samples[0];
        CcWorldPoint route_last =
            route->samples[CC_WORLD_ROUTE_SAMPLE_COUNT - 1];
        dx = reverse ? route_first.x - route_last.x :
                       route_last.x - route_first.x;
        dz = reverse ? route_first.z - route_last.z :
                       route_last.z - route_first.z;
    }
    *heading_yaw = atan2f(dx, dz);
    return true;
}

const CcWorldSettlementPlacement *CcWorldSettlementPlacementForId(
    const CcWorldManifest *manifest, CcId settlement_id)
{
    if (manifest == NULL || settlement_id == 0U) return NULL;
    for (int32_t i = 0; i < manifest->settlement_count; ++i) {
        if (manifest->settlements[i].settlement_id == settlement_id) {
            return &manifest->settlements[i];
        }
    }
    return NULL;
}

const CcWorldRoutePlacement *CcWorldRoutePlacementForId(
    const CcWorldManifest *manifest, CcId route_id)
{
    if (manifest == NULL || route_id == 0U) return NULL;
    for (int32_t i = 0; i < manifest->route_count; ++i) {
        if (manifest->routes[i].route_id == route_id) {
            return &manifest->routes[i];
        }
    }
    return NULL;
}

const CcWorldRoadSitePlacement *CcWorldRoadSitePlacementForId(
    const CcWorldManifest *manifest, CcId road_site_id)
{
    if (manifest == NULL || road_site_id == 0U) return NULL;
    for (int32_t i = 0; i < manifest->road_site_count; ++i) {
        if (manifest->road_sites[i].road_site_id == road_site_id) {
            return &manifest->road_sites[i];
        }
    }
    return NULL;
}

const CcWorldSitePlacement *CcWorldSitePlacementForKind(
    const CcWorldManifest *manifest, CcWorldSiteKind kind)
{
    if (manifest == NULL) return NULL;
    for (int32_t i = 0; i < manifest->site_count; ++i) {
        if (manifest->sites[i].kind == kind) return &manifest->sites[i];
    }
    return NULL;
}

const CcWorldSettlementPlacement *CcWorldNearestSettlement(
    const CcWorldManifest *manifest, float x, float z, float *distance)
{
    if (distance != NULL) *distance = FLT_MAX;
    if (manifest == NULL) return NULL;
    CcWorldPoint point = {x, z};
    const CcWorldSettlementPlacement *nearest = NULL;
    float nearest_distance = FLT_MAX;
    for (int32_t i = 0; i < manifest->settlement_count; ++i) {
        float candidate = PointDistance(point, manifest->settlements[i].center);
        if (candidate >= nearest_distance) continue;
        nearest = &manifest->settlements[i];
        nearest_distance = candidate;
    }
    if (distance != NULL) *distance = nearest_distance;
    return nearest;
}

bool CcWorldManifestContains(const CcWorldManifest *manifest, float x, float z)
{
    return manifest != NULL && isfinite(x) && isfinite(z) &&
           x >= manifest->minimum_x && x <= manifest->maximum_x &&
           z >= manifest->minimum_z && z <= manifest->maximum_z;
}

CcWorldPoint CcWorldSettlementLocalPoint(
    const CcWorldManifest *manifest, CcId settlement_id,
    float local_x, float local_z)
{
    const CcWorldSettlementPlacement *placement =
        CcWorldSettlementPlacementForId(manifest, settlement_id);
    if (placement == NULL) return (CcWorldPoint){0};
    float local_offset_x = (local_x - 48.0f) * placement->profile_scale;
    float local_offset_z = (local_z - 36.0f) * placement->profile_scale;
    float rotation = placement->entrance_heading_yaw - 0.5f * 3.14159265359f;
    float cosine = cosf(rotation);
    float sine = sinf(rotation);
    return (CcWorldPoint){
        placement->center.x + cosine * local_offset_x +
            sine * local_offset_z,
        placement->center.z - sine * local_offset_x +
            cosine * local_offset_z,
    };
}

CcWorldPoint CcWorldSettlementFeaturePoint(
    const CcWorldManifest *manifest, CcId settlement_id,
    float offset_x, float offset_z)
{
    const CcWorldSettlementPlacement *placement =
        CcWorldSettlementPlacementForId(manifest, settlement_id);
    if (placement == NULL) return (CcWorldPoint){0};
    float rotation = placement->entrance_heading_yaw - 0.5f * 3.14159265359f;
    float cosine = cosf(rotation);
    float sine = sinf(rotation);
    return (CcWorldPoint){placement->center.x + cosine * offset_x +
                              sine * offset_z,
                          placement->center.z - sine * offset_x +
                              cosine * offset_z};
}

static float SettlementEntranceHeading(const CcWorldManifest *manifest,
                                        const CcSim *sim,
                                        CcId settlement_id)
{
    const CcWorldSettlementPlacement *settlement =
        CcWorldSettlementPlacementForId(manifest, settlement_id);
    if (settlement == NULL || sim == NULL) return 0.5f * 3.14159265359f;
    float direction_x = 0.0f;
    float direction_z = 0.0f;
    float fallback_x = 1.0f;
    float fallback_z = 0.0f;
    bool found = false;
    for (int32_t route_index = 0;
         route_index < sim->route_count; ++route_index) {
        const CcRoute *route = &sim->routes[route_index];
        CcId other_id = route->from_id == settlement_id ? route->to_id :
                        route->to_id == settlement_id ? route->from_id : 0U;
        if (other_id == 0U) continue;
        const CcWorldSettlementPlacement *other =
            CcWorldSettlementPlacementForId(manifest, other_id);
        if (other == NULL) continue;
        float dx = other->center.x - settlement->center.x;
        float dz = other->center.z - settlement->center.z;
        float length = sqrtf(dx * dx + dz * dz);
        if (length <= 0.001f) continue;
        dx /= length;
        dz /= length;
        if (!found) {
            fallback_x = dx;
            fallback_z = dz;
            found = true;
        }
        direction_x += dx;
        direction_z += dz;
    }
    if (direction_x * direction_x + direction_z * direction_z < 0.01f) {
        direction_x = fallback_x;
        direction_z = fallback_z;
    }
    return atan2f(direction_x, direction_z);
}

static CcWorldPoint QuadraticPoint(CcWorldPoint first, CcWorldPoint control,
                                   CcWorldPoint last, float amount)
{
    float inverse = 1.0f - amount;
    return (CcWorldPoint){
        inverse * inverse * first.x +
            2.0f * inverse * amount * control.x + amount * amount * last.x,
        inverse * inverse * first.z +
            2.0f * inverse * amount * control.z + amount * amount * last.z,
    };
}

static float WrapAngle(float angle)
{
    while (angle > 3.14159265359f) angle -= 6.28318530718f;
    while (angle < -3.14159265359f) angle += 6.28318530718f;
    return angle;
}

static CcWorldPoint SettlementRingPoint(
    const CcWorldSettlementPlacement *settlement, float heading_yaw,
    float extra_distance)
{
    if (settlement == NULL) return (CcWorldPoint){0};
    float distance = settlement->radius + CC_WORLD_JUNCTION_CLEARANCE +
                     fmaxf(0.0f, extra_distance);
    return (CcWorldPoint){
        settlement->center.x + sinf(heading_yaw) * distance,
        settlement->center.z + cosf(heading_yaw) * distance,
    };
}

static void BuildRouteSamples(CcWorldRoutePlacement *route,
                              const CcWorldSettlementPlacement *from,
                              const CcWorldSettlementPlacement *to)
{
    if (route == NULL || from == NULL || to == NULL) {
        return;
    }
    route->samples[0] = from->gate;
    route->samples[CC_WORLD_ROUTE_SAMPLE_COUNT - 1] = to->gate;

    float center_dx = to->center.x - from->center.x;
    float center_dz = to->center.z - from->center.z;
    float route_heading = atan2f(center_dx, center_dz);
    float reverse_heading = WrapAngle(route_heading + 3.14159265359f);
    float from_turn = WrapAngle(
        route_heading - from->entrance_heading_yaw);
    for (int32_t sample = CC_WORLD_ROUTE_FROM_JUNCTION_SAMPLE;
         sample <= CC_WORLD_ROUTE_FROM_CORRIDOR_SAMPLE; ++sample) {
        float amount = (float)(sample - CC_WORLD_ROUTE_FROM_JUNCTION_SAMPLE) /
            (float)(CC_WORLD_ROUTE_FROM_CORRIDOR_SAMPLE -
                    CC_WORLD_ROUTE_FROM_JUNCTION_SAMPLE);
        route->samples[sample] = SettlementRingPoint(
            from, from->entrance_heading_yaw + from_turn * amount,
            CC_WORLD_ROUTE_FANOUT_LENGTH * amount);
    }
    float to_turn = WrapAngle(
        to->entrance_heading_yaw - reverse_heading);
    for (int32_t sample = CC_WORLD_ROUTE_TO_CORRIDOR_SAMPLE;
         sample <= CC_WORLD_ROUTE_TO_JUNCTION_SAMPLE; ++sample) {
        float amount = (float)(sample - CC_WORLD_ROUTE_TO_CORRIDOR_SAMPLE) /
            (float)(CC_WORLD_ROUTE_TO_JUNCTION_SAMPLE -
                    CC_WORLD_ROUTE_TO_CORRIDOR_SAMPLE);
        route->samples[sample] = SettlementRingPoint(
            to, reverse_heading + to_turn * amount,
            CC_WORLD_ROUTE_FANOUT_LENGTH * (1.0f - amount));
    }

    CcWorldPoint first = route->samples[CC_WORLD_ROUTE_FROM_CORRIDOR_SAMPLE];
    CcWorldPoint last = route->samples[CC_WORLD_ROUTE_TO_CORRIDOR_SAMPLE];
    float dx = last.x - first.x;
    float dz = last.z - first.z;
    float length = sqrtf(dx * dx + dz * dz);
    float inverse_length = length > 0.001f ? 1.0f / length : 0.0f;
    float bend = HashSigned(route->seed) * fminf(18.0f, length * 0.10f);
    route->control = (CcWorldPoint){
        (first.x + last.x) * 0.5f - dz * inverse_length * bend,
        (first.z + last.z) * 0.5f + dx * inverse_length * bend,
    };
    int32_t middle_segments = CC_WORLD_ROUTE_TO_CORRIDOR_SAMPLE -
                              CC_WORLD_ROUTE_FROM_CORRIDOR_SAMPLE;
    for (int32_t sample = CC_WORLD_ROUTE_FROM_CORRIDOR_SAMPLE + 1;
         sample < CC_WORLD_ROUTE_TO_CORRIDOR_SAMPLE; ++sample) {
        float amount = (float)(sample - CC_WORLD_ROUTE_FROM_CORRIDOR_SAMPLE) /
            (float)middle_segments;
        route->samples[sample] = QuadraticPoint(
            first, route->control, last, amount);
    }
}

bool CcWorldManifestBuild(CcWorldManifest *manifest, const CcSim *sim)
{
    if (manifest == NULL || sim == NULL || sim->settlement_count <= 0 ||
        sim->settlement_count > CC_MAX_SETTLEMENTS ||
        sim->route_count < 0 || sim->route_count > CC_MAX_ROUTES ||
        sim->road_site_count < 0 ||
        sim->road_site_count > CC_MAX_ROAD_SITES) {
        return false;
    }
    *manifest = (CcWorldManifest){0};
    manifest->world_seed = sim->world_seed;
    manifest->generator_version = sim->generator_version;

    int32_t minimum_map_x = sim->settlements[0].map_x;
    int32_t minimum_map_z = sim->settlements[0].map_y;
    for (int32_t i = 1; i < sim->settlement_count; ++i) {
        minimum_map_x = sim->settlements[i].map_x < minimum_map_x ?
            sim->settlements[i].map_x : minimum_map_x;
        minimum_map_z = sim->settlements[i].map_y < minimum_map_z ?
            sim->settlements[i].map_y : minimum_map_z;
    }

    manifest->settlement_count = sim->settlement_count;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        const CcSettlement *settlement = &sim->settlements[i];
        CcWorldSettlementPlacement *placement = &manifest->settlements[i];
        placement->settlement_id = settlement->id;
        placement->function = settlement->function;
        placement->center = (CcWorldPoint){
            CC_WORLD_MARGIN +
                (float)(settlement->map_x - minimum_map_x) * CC_WORLD_MAP_SCALE,
            CC_WORLD_MARGIN +
                (float)(settlement->map_y - minimum_map_z) * CC_WORLD_MAP_SCALE,
        };
        placement->radius = CC_WORLD_SETTLEMENT_RADIUS +
            (settlement->size == CC_SETTLEMENT_CAPITAL_SIZE ? 8.0f :
             settlement->size == CC_SETTLEMENT_TOWN ? 4.0f : 0.0f);
        placement->profile_scale = (placement->radius - 3.0f) / 60.0f;
        placement->seed = MixBits(
            sim->world_seed ^ (uint32_t)settlement->id ^
            (uint32_t)(settlement->id >> 32U));
        placement->plateau_height = BaseTerrainHeight(
            manifest, placement->center.x, placement->center.z);
    }

    for (int32_t i = 0; i < manifest->settlement_count; ++i) {
        CcWorldSettlementPlacement *placement = &manifest->settlements[i];
        placement->entrance_heading_yaw = SettlementEntranceHeading(
            manifest, sim, placement->settlement_id);
        float forward_x = sinf(placement->entrance_heading_yaw);
        float forward_z = cosf(placement->entrance_heading_yaw);
        float gate_distance = 48.0f * placement->profile_scale;
        float junction_distance = placement->radius +
                                  CC_WORLD_JUNCTION_CLEARANCE;
        placement->gate = (CcWorldPoint){
            placement->center.x + forward_x * gate_distance,
            placement->center.z + forward_z * gate_distance,
        };
        placement->junction = (CcWorldPoint){
            placement->center.x + forward_x * junction_distance,
            placement->center.z + forward_z * junction_distance,
        };
    }

    manifest->route_count = sim->route_count;
    for (int32_t i = 0; i < sim->route_count; ++i) {
        const CcRoute *route = &sim->routes[i];
        const CcWorldSettlementPlacement *from =
            CcWorldSettlementPlacementForId(manifest, route->from_id);
        const CcWorldSettlementPlacement *to =
            CcWorldSettlementPlacementForId(manifest, route->to_id);
        if (from == NULL || to == NULL) return false;
        CcWorldRoutePlacement *placement = &manifest->routes[i];
        placement->route_id = route->id;
        placement->from_id = route->from_id;
        placement->to_id = route->to_id;
        placement->seed = MixBits(
            sim->world_seed ^ (uint32_t)route->id ^
            (uint32_t)(route->id >> 32U));
        BuildRouteSamples(placement, from, to);
    }

    manifest->road_site_count = sim->road_site_count;
    for (int32_t i = 0; i < sim->road_site_count; ++i) {
        const CcRoadSite *site = &sim->road_sites[i];
        const CcWorldRoutePlacement *route =
            CcWorldRoutePlacementForId(manifest, site->route_id);
        if (route == NULL) return false;
        CcWorldRoadSitePlacement *placement = &manifest->road_sites[i];
        placement->road_site_id = site->id;
        placement->route_id = site->route_id;
        placement->kind = site->kind;
        placement->blocker = site->blocker;
        placement->seed = MixBits(
            sim->world_seed ^ (uint32_t)site->id ^
            (uint32_t)(site->id >> 32U));
        float amount = CcWorldRouteJourneyAmount(
            route, route->from_id, (float)site->progress_milli / 1000.0f);
        float route_heading = 0.0f;
        if (!CcWorldRoutePose(route, route->from_id, amount,
                              &placement->junction, &route_heading)) {
            return false;
        }
        float forward_x = sinf(route_heading);
        float forward_z = cosf(route_heading);
        float right_x = cosf(route_heading) * (float)site->side;
        float right_z = -sinf(route_heading) * (float)site->side;
        float length = (float)site->spur_length;
        placement->bend = (CcWorldPoint){
            placement->junction.x + forward_x * 2.5f +
                right_x * length * 0.46f,
            placement->junction.z + forward_z * 2.5f +
                right_z * length * 0.46f,
        };
        placement->destination = (CcWorldPoint){
            placement->junction.x + forward_x * 5.0f + right_x * length,
            placement->junction.z + forward_z * 5.0f + right_z * length,
        };
        const float blocker_amount = 0.58f;
        float tangent_x = placement->destination.x - placement->bend.x;
        float tangent_z = placement->destination.z - placement->bend.z;
        placement->blocker_position = (CcWorldPoint){
            placement->bend.x + tangent_x * blocker_amount,
            placement->bend.z + tangent_z * blocker_amount,
        };
        placement->blocker_heading_yaw = atan2f(tangent_x, tangent_z);
    }

    const CcWorldSettlementPlacement *underroad =
        sim->dungeon_count > 0 ? CcWorldSettlementPlacementForId(
            manifest, sim->dungeons[0].settlement_id) : NULL;
    const CcWorldSettlementPlacement *goblin =
        CcWorldSettlementPlacementForId(
            manifest, sim->goblins.lair_settlement_id);
    const CcWorldSettlementPlacement *dragon =
        CcWorldSettlementPlacementForId(
            manifest, sim->dragon.lair_settlement_id);
    const CcWorldSettlementPlacement *site_anchors[CC_WORLD_SITE_COUNT] = {
        underroad, goblin, dragon
    };
    manifest->site_count = 0;
    for (int32_t i = 0; i < CC_WORLD_SITE_COUNT; ++i) {
        const CcWorldSettlementPlacement *anchor = site_anchors[i];
        if (anchor == NULL) continue;
        CcWorldSitePlacement *site = &manifest->sites[manifest->site_count++];
        site->kind = (CcWorldSiteKind)i;
        site->settlement_id = anchor->settlement_id;
        site->seed = MixBits(anchor->seed ^ (uint32_t)i * UINT32_C(0x9e3779b9));
        float angle = ((float)(site->seed & UINT32_C(0xffff)) /
                       65535.0f) * 6.28318530718f;
        float radius = anchor->radius + 18.0f + (float)i * 3.0f;
        site->position = (CcWorldPoint){
            anchor->center.x + cosf(angle) * radius,
            anchor->center.z + sinf(angle) * radius,
        };
    }

    float minimum_world_x = manifest->settlements[0].center.x;
    float minimum_world_z = manifest->settlements[0].center.z;
    float maximum_world_x = minimum_world_x;
    float maximum_world_z = minimum_world_z;
    for (int32_t i = 0; i < manifest->settlement_count; ++i) {
        const CcWorldPoint point = manifest->settlements[i].center;
        minimum_world_x = fminf(minimum_world_x, point.x);
        minimum_world_z = fminf(minimum_world_z, point.z);
        maximum_world_x = fmaxf(maximum_world_x, point.x);
        maximum_world_z = fmaxf(maximum_world_z, point.z);
    }
    for (int32_t i = 0; i < manifest->road_site_count; ++i) {
        const CcWorldPoint point = manifest->road_sites[i].destination;
        minimum_world_x = fminf(minimum_world_x, point.x);
        minimum_world_z = fminf(minimum_world_z, point.z);
        maximum_world_x = fmaxf(maximum_world_x, point.x);
        maximum_world_z = fmaxf(maximum_world_z, point.z);
    }
    manifest->minimum_x = AlignDown(minimum_world_x - CC_WORLD_MARGIN);
    manifest->minimum_z = AlignDown(minimum_world_z - CC_WORLD_MARGIN);
    manifest->maximum_x = AlignUp(maximum_world_x + CC_WORLD_MARGIN);
    manifest->maximum_z = AlignUp(maximum_world_z + CC_WORLD_MARGIN);
    return true;
}

static float TerrainWithoutRoads(const CcWorldManifest *manifest,
                                 float x, float z)
{
    float height = BaseTerrainHeight(manifest, x, z);
    if (manifest == NULL) return height;
    CcWorldPoint point = {x, z};
    for (int32_t i = 0; i < manifest->settlement_count; ++i) {
        const CcWorldSettlementPlacement *settlement =
            &manifest->settlements[i];
        float distance = PointDistance(point, settlement->center);
        if (distance >= settlement->radius + 12.0f) continue;
        float weight = 1.0f - SmoothStep(
            (distance - settlement->radius + 4.0f) / 16.0f);
        height += (settlement->plateau_height - height) * weight;
    }
    return height;
}

static float NearestRoadDistance(const CcWorldManifest *manifest,
                                 CcWorldPoint point,
                                 float maximum_distance,
                                 const CcWorldRoutePlacement **nearest_route,
                                 CcWorldPoint *nearest_point)
{
    if (nearest_route != NULL) *nearest_route = NULL;
    if (nearest_point != NULL) *nearest_point = (CcWorldPoint){0};
    if (manifest == NULL) return FLT_MAX;
    float nearest = FLT_MAX;
    float minimum_x = point.x - maximum_distance;
    float maximum_x = point.x + maximum_distance;
    float minimum_z = point.z - maximum_distance;
    float maximum_z = point.z + maximum_distance;
    for (int32_t route_index = 0;
         route_index < manifest->route_count; ++route_index) {
        const CcWorldRoutePlacement *route = &manifest->routes[route_index];
        for (int32_t sample = 0;
         sample < CC_WORLD_ROUTE_SAMPLE_COUNT - 1; ++sample) {
            CcWorldPoint first = route->samples[sample];
            CcWorldPoint second = route->samples[sample + 1];
            /* Terrain and surface queries only need roads within their reach. */
            if ((first.x < minimum_x && second.x < minimum_x) ||
                (first.x > maximum_x && second.x > maximum_x) ||
                (first.z < minimum_z && second.z < minimum_z) ||
                (first.z > maximum_z && second.z > maximum_z)) continue;
            float segment_amount = 0.0f;
            float distance = DistanceToSegment(
                point, first, second, &segment_amount);
            if (distance >= nearest) continue;
            nearest = distance;
            if (nearest_route != NULL) *nearest_route = route;
            if (nearest_point != NULL) {
                *nearest_point = (CcWorldPoint){
                    first.x + (second.x - first.x) * segment_amount,
                    first.z + (second.z - first.z) * segment_amount,
                };
            }
        }
    }
    return nearest;
}

float CcWorldTerrainHeight(const CcWorldManifest *manifest, float x, float z)
{
    if (manifest == NULL || !isfinite(x) || !isfinite(z)) return 0.0f;
    float height = TerrainWithoutRoads(manifest, x, z);
    const CcWorldRoutePlacement *route = NULL;
    CcWorldPoint center = {0};
    float distance = NearestRoadDistance(
        manifest, (CcWorldPoint){x, z}, CC_WORLD_ROAD_HALF_WIDTH + 3.5f,
        &route, &center);
    if (route != NULL && distance < CC_WORLD_ROAD_HALF_WIDTH + 3.5f) {
        float road_height = TerrainWithoutRoads(manifest, center.x, center.z);
        float weight = 1.0f - SmoothStep(
            (distance - CC_WORLD_ROAD_HALF_WIDTH) / 3.5f);
        height += (road_height - height) * weight;
    }
    return height;
}

CcWorldSurfaceKind CcWorldSurfaceAt(
    const CcWorldManifest *manifest, float x, float z)
{
    if (manifest == NULL) return CC_WORLD_SURFACE_WILDERNESS;
    float settlement_distance = FLT_MAX;
    const CcWorldSettlementPlacement *settlement = CcWorldNearestSettlement(
        manifest, x, z, &settlement_distance);
    if (settlement != NULL && settlement_distance <= settlement->radius) {
        return CC_WORLD_SURFACE_SETTLEMENT;
    }
    float road_distance = NearestRoadDistance(
        manifest, (CcWorldPoint){x, z}, CC_WORLD_ROAD_HALF_WIDTH, NULL, NULL);
    return road_distance <= CC_WORLD_ROAD_HALF_WIDTH ?
        CC_WORLD_SURFACE_ROAD : CC_WORLD_SURFACE_WILDERNESS;
}

static CcWorldChunk *FindChunk(CcWorldStream *stream,
                               int32_t chunk_x, int32_t chunk_z)
{
    if (stream == NULL) return NULL;
    for (int32_t i = 0; i < CC_WORLD_STREAM_CAPACITY; ++i) {
        CcWorldChunk *chunk = &stream->chunks[i];
        if (chunk->state != CC_WORLD_CHUNK_EMPTY &&
            chunk->x == chunk_x && chunk->z == chunk_z) return chunk;
    }
    return NULL;
}

const CcWorldChunk *CcWorldStreamChunkAt(
    const CcWorldStream *stream, int32_t chunk_x, int32_t chunk_z)
{
    return FindChunk((CcWorldStream *)stream, chunk_x, chunk_z);
}

static bool ChunkDesired(const CcWorldStream *stream,
                         int32_t chunk_x, int32_t chunk_z)
{
    return stream != NULL &&
           abs(chunk_x - stream->focus_chunk_x) <= CC_WORLD_STREAM_RADIUS &&
           abs(chunk_z - stream->focus_chunk_z) <= CC_WORLD_STREAM_RADIUS &&
           ChunkInsideManifest(&stream->manifest, chunk_x, chunk_z);
}

static CcWorldChunk *AllocateChunk(CcWorldStream *stream,
                                   int32_t chunk_x, int32_t chunk_z)
{
    CcWorldChunk *empty = NULL;
    CcWorldChunk *oldest = NULL;
    for (int32_t i = 0; i < CC_WORLD_STREAM_CAPACITY; ++i) {
        CcWorldChunk *chunk = &stream->chunks[i];
        if (chunk->state == CC_WORLD_CHUNK_EMPTY) {
            empty = chunk;
            break;
        }
        if (ChunkDesired(stream, chunk->x, chunk->z)) continue;
        if (oldest == NULL || chunk->last_touched < oldest->last_touched) {
            oldest = chunk;
        }
    }
    CcWorldChunk *chunk = empty != NULL ? empty : oldest;
    if (chunk == NULL) return NULL;
    if (chunk->state != CC_WORLD_CHUNK_EMPTY) stream->evicted_chunks += 1U;
    *chunk = (CcWorldChunk){
        .x = chunk_x,
        .z = chunk_z,
        .state = CC_WORLD_CHUNK_PENDING,
        .last_touched = stream->tick,
    };
    return chunk;
}

static void GenerateChunk(CcWorldStream *stream, CcWorldChunk *chunk)
{
    float sample_step = CC_WORLD_CHUNK_SIZE / (float)CC_WORLD_CHUNK_CELLS;
    float origin_x = (float)chunk->x * CC_WORLD_CHUNK_SIZE;
    float origin_z = (float)chunk->z * CC_WORLD_CHUNK_SIZE;
    for (int32_t row = 0; row < CC_WORLD_CHUNK_SAMPLES; ++row) {
        for (int32_t column = 0; column < CC_WORLD_CHUNK_SAMPLES; ++column) {
            float x = origin_x + (float)column * sample_step;
            float z = origin_z + (float)row * sample_step;
            chunk->heights[row * CC_WORLD_CHUNK_SAMPLES + column] =
                CcWorldTerrainHeight(&stream->manifest, x, z);
        }
    }
    chunk->state = CC_WORLD_CHUNK_READY;
    chunk->generation = stream->next_generation++;
    stream->generated_chunks += 1U;
}

bool CcWorldStreamInit(CcWorldStream *stream, const CcSim *sim)
{
    if (stream == NULL) return false;
    *stream = (CcWorldStream){0};
    stream->next_generation = 1U;
    if (!CcWorldManifestBuild(&stream->manifest, sim)) return false;
    const CcWorldSettlementPlacement *start =
        CcWorldSettlementPlacementForId(
            &stream->manifest, sim->player.location_id);
    float focus_x = start != NULL ? start->center.x : stream->manifest.minimum_x;
    float focus_z = start != NULL ? start->center.z : stream->manifest.minimum_z;
    CcWorldStreamUpdate(stream, focus_x, focus_z, CC_WORLD_STREAM_CAPACITY);
    return true;
}

void CcWorldStreamUpdate(CcWorldStream *stream, float focus_x, float focus_z,
                         int32_t generation_budget)
{
    if (stream == NULL || !isfinite(focus_x) || !isfinite(focus_z)) return;
    stream->tick += 1U;
    stream->focus_chunk_x = ChunkCoordinate(focus_x);
    stream->focus_chunk_z = ChunkCoordinate(focus_z);
    for (int32_t offset_z = -CC_WORLD_STREAM_RADIUS;
         offset_z <= CC_WORLD_STREAM_RADIUS; ++offset_z) {
        for (int32_t offset_x = -CC_WORLD_STREAM_RADIUS;
             offset_x <= CC_WORLD_STREAM_RADIUS; ++offset_x) {
            int32_t chunk_x = stream->focus_chunk_x + offset_x;
            int32_t chunk_z = stream->focus_chunk_z + offset_z;
            if (!ChunkInsideManifest(&stream->manifest, chunk_x, chunk_z)) {
                continue;
            }
            CcWorldChunk *chunk = FindChunk(stream, chunk_x, chunk_z);
            if (chunk == NULL) chunk = AllocateChunk(stream, chunk_x, chunk_z);
            if (chunk != NULL) chunk->last_touched = stream->tick;
        }
    }
    if (generation_budget < 0) generation_budget = 0;
    for (int32_t generated = 0; generated < generation_budget; ++generated) {
        CcWorldChunk *nearest = NULL;
        int32_t nearest_distance = INT32_MAX;
        for (int32_t i = 0; i < CC_WORLD_STREAM_CAPACITY; ++i) {
            CcWorldChunk *chunk = &stream->chunks[i];
            if (chunk->state != CC_WORLD_CHUNK_PENDING ||
                !ChunkDesired(stream, chunk->x, chunk->z)) continue;
            int32_t distance = abs(chunk->x - stream->focus_chunk_x) +
                               abs(chunk->z - stream->focus_chunk_z);
            if (distance >= nearest_distance) continue;
            nearest = chunk;
            nearest_distance = distance;
        }
        if (nearest == NULL) break;
        GenerateChunk(stream, nearest);
    }
}

void CcWorldStreamFollowRoute(CcWorldStream *stream,
                              const CcWorldRoutePlacement *route,
                              CcId origin_id, float amount,
                              int32_t generation_budget)
{
    CcWorldPoint point;
    float heading;
    if (stream == NULL || !isfinite(amount) || !CcWorldRoutePose(route, origin_id, amount,
                                           &point, &heading)) return;
    CcWorldStreamUpdate(stream, point.x, point.z, 0);
    float length = CcWorldRouteLength(route);
    float ahead_amount = fminf(1.0f, amount +
        (length > 0.001f ? CC_WORLD_CHUNK_SIZE / length : 0.0f));
    CcWorldPoint ahead = point;
    (void)CcWorldRoutePose(route, origin_id, ahead_amount, &ahead, &heading);
    CcWorldPoint priority[] = {point, ahead};
    for (int32_t i = 0; i < 2 && generation_budget > 0; ++i) {
        CcWorldChunk *chunk = FindChunk(
            stream, ChunkCoordinate(priority[i].x), ChunkCoordinate(priority[i].z));
        if (chunk != NULL && chunk->state == CC_WORLD_CHUNK_PENDING &&
            ChunkDesired(stream, chunk->x, chunk->z)) {
            GenerateChunk(stream, chunk);
            generation_budget -= 1;
        }
    }
    if (generation_budget > 0) {
        CcWorldStreamUpdate(stream, point.x, point.z, generation_budget);
    }
}

static float ChunkHeightAt(const CcWorldChunk *chunk, float x, float z)
{
    float origin_x = (float)chunk->x * CC_WORLD_CHUNK_SIZE;
    float origin_z = (float)chunk->z * CC_WORLD_CHUNK_SIZE;
    float sample_scale = (float)CC_WORLD_CHUNK_CELLS / CC_WORLD_CHUNK_SIZE;
    float local_x = fmaxf(0.0f, fminf((x - origin_x) * sample_scale,
                                      (float)CC_WORLD_CHUNK_CELLS));
    float local_z = fmaxf(0.0f, fminf((z - origin_z) * sample_scale,
                                      (float)CC_WORLD_CHUNK_CELLS));
    int32_t column = (int32_t)floorf(local_x);
    int32_t row = (int32_t)floorf(local_z);
    if (column >= CC_WORLD_CHUNK_CELLS) column = CC_WORLD_CHUNK_CELLS - 1;
    if (row >= CC_WORLD_CHUNK_CELLS) row = CC_WORLD_CHUNK_CELLS - 1;
    float tx = local_x - (float)column;
    float tz = local_z - (float)row;
    int32_t near_left = row * CC_WORLD_CHUNK_SAMPLES + column;
    int32_t near_right = near_left + 1;
    int32_t far_left = near_left + CC_WORLD_CHUNK_SAMPLES;
    int32_t far_right = far_left + 1;
    float near_height = chunk->heights[near_left] +
        (chunk->heights[near_right] - chunk->heights[near_left]) * tx;
    float far_height = chunk->heights[far_left] +
        (chunk->heights[far_right] - chunk->heights[far_left]) * tx;
    return near_height + (far_height - near_height) * tz;
}

float CcWorldStreamHeightAt(const CcWorldStream *stream, float x, float z)
{
    if (stream == NULL) return 0.0f;
    const CcWorldChunk *chunk = CcWorldStreamChunkAt(
        stream, ChunkCoordinate(x), ChunkCoordinate(z));
    if (chunk != NULL && chunk->state == CC_WORLD_CHUNK_READY) {
        return ChunkHeightAt(chunk, x, z);
    }
    return CcWorldTerrainHeight(&stream->manifest, x, z);
}

int32_t CcWorldStreamResidentCount(const CcWorldStream *stream)
{
    int32_t count = 0;
    if (stream == NULL) return count;
    for (int32_t i = 0; i < CC_WORLD_STREAM_CAPACITY; ++i) {
        if (stream->chunks[i].state == CC_WORLD_CHUNK_READY) count += 1;
    }
    return count;
}

int32_t CcWorldStreamPendingCount(const CcWorldStream *stream)
{
    int32_t count = 0;
    if (stream == NULL) return count;
    for (int32_t i = 0; i < CC_WORLD_STREAM_CAPACITY; ++i) {
        if (stream->chunks[i].state == CC_WORLD_CHUNK_PENDING) count += 1;
    }
    return count;
}

size_t CcWorldStreamResidentBytes(const CcWorldStream *stream)
{
    return (size_t)CcWorldStreamResidentCount(stream) * sizeof(CcWorldChunk);
}
