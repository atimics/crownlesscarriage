#include "sim/cc_road_position.h"
#include <limits.h>
#include <math.h>
#include <string.h>

#define ROAD_MAP_SCALE 0.75f
#define ROAD_WORLD_MARGIN 64.0f
#define ROAD_SETTLEMENT_RADIUS 38.0f
#define ROAD_JUNCTION_CLEARANCE 18.0f
#define ROAD_FANOUT_LENGTH 4.0f
#define ROAD_FROM_CORRIDOR_SAMPLE 8
#define ROAD_TO_CORRIDOR_SAMPLE 24

typedef struct GeometrySettlement {
    CcId id;
    float x;
    float z;
    float radius;
    float entrance_heading;
    float gate_x;
    float gate_z;
} GeometrySettlement;

static uint32_t MixBits(uint32_t value)
{
    value ^= value >> 16U;
    value *= UINT32_C(0x7feb352d);
    value ^= value >> 15U;
    value *= UINT32_C(0x846ca68b);
    value ^= value >> 16U;
    return value;
}

static float HashSigned(uint32_t value)
{
    return (float)(value & UINT32_C(0x00ffffff)) /
               (float)UINT32_C(0x00ffffff) * 2.0f - 1.0f;
}

static float WrapAngle(float angle)
{
    while (angle > 3.14159265359f) angle -= 6.28318530718f;
    while (angle < -3.14159265359f) angle += 6.28318530718f;
    return angle;
}

static int32_t QuantizeWorld(float value)
{
    double scaled = (double)value *
        (double)CC_ROAD_GEOMETRY_UNITS_PER_WORLD_UNIT;
    double rounded = scaled >= 0.0 ? floor(scaled + 0.5) :
                                     ceil(scaled - 0.5);
    if (rounded > (double)INT32_MAX) return INT32_MAX;
    if (rounded < (double)INT32_MIN) return INT32_MIN;
    return (int32_t)rounded;
}

static uint64_t IntegerSquareRoot(uint64_t value)
{
    uint64_t result = 0;
    uint64_t bit = UINT64_C(1) << 62U;
    while (bit > value) bit >>= 2U;
    while (bit != 0U) {
        if (value >= result + bit) {
            value -= result + bit;
            result = (result >> 1U) + bit;
        } else {
            result >>= 1U;
        }
        bit >>= 2U;
    }
    return result;
}

static int32_t PointDistanceUnits(CcRoadGeometryPoint first,
                                  CcRoadGeometryPoint second)
{
    int64_t dx = (int64_t)second.x_units - first.x_units;
    int64_t dz = (int64_t)second.z_units - first.z_units;
    uint64_t squared = (uint64_t)(dx * dx + dz * dz);
    uint64_t lower = IntegerSquareRoot(squared);
    uint64_t upper = lower + 1U;
    if (squared - lower * lower >= upper * upper - squared) lower = upper;
    return lower > INT32_MAX ? INT32_MAX : (int32_t)lower;
}

static const GeometrySettlement *GeometrySettlementForId(
    const GeometrySettlement *settlements, int32_t count, CcId id)
{
    for (int32_t i = 0; i < count; ++i) {
        if (settlements[i].id == id) return &settlements[i];
    }
    return NULL;
}

static bool BuildGeometrySettlements(const CcSim *sim,
                                     GeometrySettlement *settlements)
{
    if (sim == NULL || settlements == NULL || sim->settlement_count <= 0 ||
        sim->settlement_count > CC_MAX_SETTLEMENTS) return false;
    int32_t minimum_x = sim->settlements[0].map_x;
    int32_t minimum_z = sim->settlements[0].map_y;
    for (int32_t i = 1; i < sim->settlement_count; ++i) {
        if (sim->settlements[i].map_x < minimum_x)
            minimum_x = sim->settlements[i].map_x;
        if (sim->settlements[i].map_y < minimum_z)
            minimum_z = sim->settlements[i].map_y;
    }
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        const CcSettlement *source = &sim->settlements[i];
        settlements[i] = (GeometrySettlement){
            .id = source->id,
            .x = ROAD_WORLD_MARGIN +
                (float)(source->map_x - minimum_x) * ROAD_MAP_SCALE,
            .z = ROAD_WORLD_MARGIN +
                (float)(source->map_y - minimum_z) * ROAD_MAP_SCALE,
            .radius = ROAD_SETTLEMENT_RADIUS +
                (source->size == CC_SETTLEMENT_CAPITAL_SIZE ? 8.0f :
                 source->size == CC_SETTLEMENT_TOWN ? 4.0f : 0.0f)
        };
    }
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        GeometrySettlement *place = &settlements[i];
        float direction_x = 0.0f;
        float direction_z = 0.0f;
        float fallback_x = 1.0f;
        float fallback_z = 0.0f;
        bool found = false;
        for (int32_t r = 0; r < sim->route_count; ++r) {
            const CcRoute *route = &sim->routes[r];
            CcId other_id = route->from_id == place->id ? route->to_id :
                route->to_id == place->id ? route->from_id : 0U;
            const GeometrySettlement *other = GeometrySettlementForId(
                settlements, sim->settlement_count, other_id);
            if (other == NULL) continue;
            float dx = other->x - place->x;
            float dz = other->z - place->z;
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
        place->entrance_heading = atan2f(direction_x, direction_z);
        float gate_distance = 48.0f * (place->radius - 3.0f) / 60.0f;
        place->gate_x = place->x + sinf(place->entrance_heading) *
            gate_distance;
        place->gate_z = place->z + cosf(place->entrance_heading) *
            gate_distance;
    }
    return true;
}

static void SetGeometryPoint(CcRoadGeometryPoint *point, float x, float z)
{
    point->x_units = QuantizeWorld(x);
    point->z_units = QuantizeWorld(z);
}

static void RingPoint(const GeometrySettlement *place, float heading,
                      float extra, float *x, float *z)
{
    float distance = place->radius + ROAD_JUNCTION_CLEARANCE +
        fmaxf(0.0f, extra);
    *x = place->x + sinf(heading) * distance;
    *z = place->z + cosf(heading) * distance;
}

static bool BuildRoadGeometry(const CcSim *sim, CcId route_id,
                              const GeometrySettlement *settlements,
                              CcRoadGeometry *geometry)
{
    if (sim == NULL || geometry == NULL || settlements == NULL) return false;
    const CcRoute *route = CcSimRoute(sim, route_id);
    if (route == NULL) return false;
    const GeometrySettlement *from = GeometrySettlementForId(
        settlements, sim->settlement_count, route->from_id);
    const GeometrySettlement *to = GeometrySettlementForId(
        settlements, sim->settlement_count, route->to_id);
    if (from == NULL || to == NULL) return false;

    *geometry = (CcRoadGeometry){
        .route_id = route->id,
        .from_id = route->from_id,
        .to_id = route->to_id
    };
    float sample_x[CC_ROAD_GEOMETRY_SAMPLE_COUNT] = {0};
    float sample_z[CC_ROAD_GEOMETRY_SAMPLE_COUNT] = {0};
    sample_x[0] = from->gate_x;
    sample_z[0] = from->gate_z;
    sample_x[CC_ROAD_GEOMETRY_SAMPLE_COUNT - 1] = to->gate_x;
    sample_z[CC_ROAD_GEOMETRY_SAMPLE_COUNT - 1] = to->gate_z;
    float center_dx = to->x - from->x;
    float center_dz = to->z - from->z;
    float route_heading = atan2f(center_dx, center_dz);
    float reverse_heading = WrapAngle(route_heading + 3.14159265359f);
    float from_turn = WrapAngle(route_heading - from->entrance_heading);
    for (int32_t sample = CC_ROAD_GEOMETRY_FROM_JUNCTION_SAMPLE;
         sample <= ROAD_FROM_CORRIDOR_SAMPLE; ++sample) {
        float amount = (float)(sample -
            CC_ROAD_GEOMETRY_FROM_JUNCTION_SAMPLE) /
            (float)(ROAD_FROM_CORRIDOR_SAMPLE -
                CC_ROAD_GEOMETRY_FROM_JUNCTION_SAMPLE);
        RingPoint(from, from->entrance_heading + from_turn * amount,
                  ROAD_FANOUT_LENGTH * amount,
                  &sample_x[sample], &sample_z[sample]);
    }
    float to_turn = WrapAngle(to->entrance_heading - reverse_heading);
    for (int32_t sample = ROAD_TO_CORRIDOR_SAMPLE;
         sample <= CC_ROAD_GEOMETRY_TO_JUNCTION_SAMPLE; ++sample) {
        float amount = (float)(sample - ROAD_TO_CORRIDOR_SAMPLE) /
            (float)(CC_ROAD_GEOMETRY_TO_JUNCTION_SAMPLE -
                ROAD_TO_CORRIDOR_SAMPLE);
        RingPoint(to, reverse_heading + to_turn * amount,
                  ROAD_FANOUT_LENGTH * (1.0f - amount),
                  &sample_x[sample], &sample_z[sample]);
    }
    float first_x = sample_x[ROAD_FROM_CORRIDOR_SAMPLE];
    float first_z = sample_z[ROAD_FROM_CORRIDOR_SAMPLE];
    float last_x = sample_x[ROAD_TO_CORRIDOR_SAMPLE];
    float last_z = sample_z[ROAD_TO_CORRIDOR_SAMPLE];
    float dx = last_x - first_x;
    float dz = last_z - first_z;
    float length = sqrtf(dx * dx + dz * dz);
    float inverse_length = length > 0.001f ? 1.0f / length : 0.0f;
    uint32_t seed = MixBits(sim->world_seed ^ (uint32_t)route->id ^
                            (uint32_t)(route->id >> 32U));
    float bend = HashSigned(seed) * fminf(18.0f, length * 0.10f);
    float control_x = (first_x + last_x) * 0.5f -
        dz * inverse_length * bend;
    float control_z = (first_z + last_z) * 0.5f +
        dx * inverse_length * bend;
    SetGeometryPoint(&geometry->control, control_x, control_z);
    int32_t middle_segments = ROAD_TO_CORRIDOR_SAMPLE -
        ROAD_FROM_CORRIDOR_SAMPLE;
    for (int32_t sample = ROAD_FROM_CORRIDOR_SAMPLE + 1;
         sample < ROAD_TO_CORRIDOR_SAMPLE; ++sample) {
        float amount = (float)(sample - ROAD_FROM_CORRIDOR_SAMPLE) /
            (float)middle_segments;
        float inverse = 1.0f - amount;
        sample_x[sample] = inverse * inverse * first_x +
            2.0f * inverse * amount * control_x + amount * amount * last_x;
        sample_z[sample] = inverse * inverse * first_z +
            2.0f * inverse * amount * control_z + amount * amount * last_z;
    }
    for (int32_t sample = 0;
         sample < CC_ROAD_GEOMETRY_SAMPLE_COUNT; ++sample) {
        SetGeometryPoint(&geometry->samples[sample],
                         sample_x[sample], sample_z[sample]);
    }
    int64_t full = 0;
    int64_t journey = 0;
    for (int32_t sample = 0;
         sample < CC_ROAD_GEOMETRY_SAMPLE_COUNT - 1; ++sample) {
        int32_t segment = PointDistanceUnits(
            geometry->samples[sample], geometry->samples[sample + 1]);
        full += segment;
        if (sample >= CC_ROAD_GEOMETRY_FROM_JUNCTION_SAMPLE &&
            sample < CC_ROAD_GEOMETRY_TO_JUNCTION_SAMPLE) journey += segment;
    }
    if (full <= 0 || full > INT32_MAX || journey <= 0 ||
        journey > INT32_MAX) return false;
    geometry->full_length_units = (int32_t)full;
    geometry->journey_length_units = (int32_t)journey;
    return true;
}

bool CcRoadGeometryBuild(const CcSim *sim, CcId route_id,
                         CcRoadGeometry *geometry)
{
    GeometrySettlement settlements[CC_MAX_SETTLEMENTS];
    return BuildGeometrySettlements(sim, settlements) &&
        BuildRoadGeometry(sim, route_id, settlements, geometry);
}

int32_t CcRoadGeometryBuildAll(const CcSim *sim,
                               CcRoadGeometry *geometries,
                               int32_t capacity)
{
    if (sim == NULL || geometries == NULL || capacity < sim->route_count)
        return 0;
    GeometrySettlement settlements[CC_MAX_SETTLEMENTS];
    if (!BuildGeometrySettlements(sim, settlements)) return 0;
    for (int32_t i = 0; i < sim->route_count; ++i) {
        if (!BuildRoadGeometry(sim, sim->routes[i].id, settlements,
                               &geometries[i])) return 0;
    }
    return sim->route_count;
}

static const CcSettlement *SettlementNamed(const CcSim *sim,
                                            const char *name)
{
    if (sim == NULL || name == NULL) return NULL;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        if (strcmp(sim->settlements[i].name, name) == 0)
            return &sim->settlements[i];
    }
    return NULL;
}

static const CcRoadSite *RoadSiteNamed(const CcSim *sim,
                                       const char *name)
{
    if (sim == NULL || name == NULL) return NULL;
    for (int32_t i = 0; i < sim->road_site_count; ++i) {
        if (strcmp(sim->road_sites[i].name, name) == 0)
            return &sim->road_sites[i];
    }
    return NULL;
}

int32_t CcRoadScaleDistance(int32_t total_units, int32_t progress_milli)
{
    if (total_units <= 0 || progress_milli <= 0) return 0;
    if (progress_milli >= 1000) return total_units;
    return (int32_t)(((int64_t)total_units * progress_milli + 500) / 1000);
}

int32_t CcRoadProgressMilli(int32_t travelled_units, int32_t total_units)
{
    if (total_units <= 0 || travelled_units <= 0) return 0;
    if (travelled_units >= total_units) return 1000;
    return (int32_t)(((int64_t)travelled_units * 1000 +
                      total_units / 2) / total_units);
}

int32_t CcRoadTravelSubticks(int32_t leg_units,
                             int32_t route_total_subticks,
                             int32_t route_length_units)
{
    if (leg_units <= 0 || route_total_subticks <= 0 ||
        route_length_units <= 0) return 0;
    int64_t numerator = (int64_t)leg_units * route_total_subticks;
    int64_t rounded = (numerator + route_length_units / 2) /
        route_length_units;
    return rounded > INT32_MAX ? INT32_MAX : (int32_t)rounded;
}

static uint64_t TokenWord(uint64_t hash, uint64_t word)
{
    for (int32_t byte = 0; byte < 8; ++byte) {
        hash ^= word & UINT64_C(0xff);
        hash *= UINT64_C(1099511628211);
        word >>= 8U;
    }
    return hash;
}

uint64_t CcRoadPreviewToken(CcId journey_id, CcId journey_goal_id,
                            CcId anchor_id, int32_t coordinate_units,
                            int32_t distance_travelled,
                            int32_t distance_remaining,
                            uint32_t journey_revision, CcId segment_id,
                            CcRoadDirection direction)
{
    uint64_t token = UINT64_C(1469598103934665603);
    token = TokenWord(token, journey_id);
    token = TokenWord(token, journey_goal_id);
    token = TokenWord(token, anchor_id);
    token = TokenWord(token, (uint64_t)(int64_t)coordinate_units);
    token = TokenWord(token, (uint64_t)(int64_t)distance_travelled);
    token = TokenWord(token, (uint64_t)(int64_t)distance_remaining);
    token = TokenWord(token, journey_revision);
    token = TokenWord(token, segment_id);
    token = TokenWord(token, (uint64_t)(int64_t)direction);
    return token == 0U ? UINT64_C(1) : token;
}

bool CcPilotRoadTopologyBuildWithLength(const CcSim *sim,
                                        int32_t main_length_units,
                                        CcPilotRoadTopology *topology)
{
    if (sim == NULL || topology == NULL || main_length_units <= 0)
        return false;
    const CcRoadSite *mill = RoadSiteNamed(sim, "Stag's Mill");
    const CcSettlement *thornford = SettlementNamed(sim, "Thornford");
    const CcSettlement *gloamgate = SettlementNamed(sim, "Gloamgate");
    if (mill == NULL || mill->kind != CC_ROAD_SITE_MILL ||
        thornford == NULL || gloamgate == NULL) return false;
    const CcRoute *route = CcSimRoute(sim, mill->route_id);
    if (route == NULL || route->from_id != thornford->id ||
        route->to_id != gloamgate->id ||
        mill->progress_milli <= 0 || mill->progress_milli >= 1000 ||
        mill->spur_length <= 0) return false;

    int32_t origin_to_junction = CcRoadScaleDistance(
        main_length_units, mill->progress_milli);
    *topology = (CcPilotRoadTopology){
        .route_id = route->id,
        .origin_id = thornford->id,
        .destination_id = gloamgate->id,
        .mill_site_id = mill->id,
        .junction_id = CC_PILOT_ROAD_JUNCTION_ID,
        .checkpoint_id = CC_PILOT_ROAD_CHECKPOINT_ID,
        .origin_segment_id = CC_PILOT_ROAD_ORIGIN_SEGMENT_ID,
        .destination_segment_id = CC_PILOT_ROAD_DESTINATION_SEGMENT_ID,
        .mill_segment_id = CC_PILOT_ROAD_MILL_SEGMENT_ID,
        .main_length_units = main_length_units,
        .origin_to_junction_units = origin_to_junction,
        .junction_to_destination_units =
            main_length_units - origin_to_junction,
        .mill_spur_length_units =
            mill->spur_length * CC_PILOT_ROAD_UNITS_PER_SPUR_UNIT,
        .checkpoint_distance_units = CcRoadScaleDistance(
            main_length_units, 500),
        .junction_progress_milli = mill->progress_milli,
        .checkpoint_progress_milli = 500
    };
    return true;
}

bool CcPilotRoadTopologyBuild(const CcSim *sim,
                              CcPilotRoadTopology *topology)
{
    if (sim == NULL || topology == NULL) return false;
    const CcRoadSite *mill = RoadSiteNamed(sim, "Stag's Mill");
    CcRoadGeometry geometry;
    return mill != NULL && CcRoadGeometryBuild(sim, mill->route_id, &geometry) &&
        CcPilotRoadTopologyBuildWithLength(
            sim, geometry.journey_length_units, topology);
}

bool CcRoadGeometryKnownFixtures(void)
{
    typedef struct GeometryFixture {
        uint32_t seed;
        int32_t full_length;
        int32_t journey_length;
        int32_t control_x;
        int32_t control_z;
        int32_t junction;
        int32_t checkpoint;
    } GeometryFixture;
    static const GeometryFixture fixtures[] = {
        {UINT32_C(0x3235a7ed), 261356, 204556, 147659, 307287,
         167736, 102278},
        {UINT32_C(0xc0a7118e), 250440, 193639, 147307, 306921,
         158784, 96820},
        {UINT32_C(1), 259110, 202310, 142059, 301501,
         165894, 101155},
        {UINT32_C(0xffffffff), 261120, 204319, 147115, 317300,
         167542, 102160}
    };
    for (size_t i = 0; i < sizeof(fixtures) / sizeof(fixtures[0]); ++i) {
        CcSim sim;
        CcPilotRoadTopology pilot;
        CcRoadGeometry geometry;
        CcSimInit(&sim, fixtures[i].seed);
        if (!CcPilotRoadTopologyBuild(&sim, &pilot) ||
            !CcRoadGeometryBuild(&sim, pilot.route_id, &geometry) ||
            geometry.full_length_units != fixtures[i].full_length ||
            geometry.journey_length_units != fixtures[i].journey_length ||
            geometry.control.x_units != fixtures[i].control_x ||
            geometry.control.z_units != fixtures[i].control_z ||
            pilot.origin_to_junction_units != fixtures[i].junction ||
            pilot.checkpoint_distance_units != fixtures[i].checkpoint) {
            return false;
        }
    }
    CcSim token_sim;
    CcPilotRoadTopology token_pilot;
    CcSimInit(&token_sim, fixtures[0].seed);
    if (!CcPilotRoadTopologyBuild(&token_sim, &token_pilot)) return false;
    return CcRoadPreviewToken(
        UINT64_C(323001), token_pilot.destination_id,
        token_pilot.junction_id, token_pilot.origin_to_junction_units,
        token_pilot.origin_to_junction_units, 0, 9U,
        token_pilot.destination_segment_id,
        CC_ROAD_DIRECTION_FORWARD) == UINT64_C(16498057034038110418);
}
