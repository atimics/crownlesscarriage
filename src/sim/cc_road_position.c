#include "sim/cc_road_position.h"
#include "sim/cc_journey_internal.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
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

static void RoadError(char *error, size_t capacity, const char *message)
{
    if (error != NULL && capacity > 0U)
        (void)snprintf(error, capacity, "%s", message);
}

static CcRoadAnchorKind PilotAnchorKind(const CcPilotRoadTopology *pilot,
                                        CcId anchor)
{
    if (anchor == pilot->origin_id || anchor == pilot->destination_id)
        return CC_ROAD_ANCHOR_SETTLEMENT;
    if (anchor == pilot->junction_id) return CC_ROAD_ANCHOR_JUNCTION;
    if (anchor == pilot->checkpoint_id) return CC_ROAD_ANCHOR_CHECKPOINT;
    return anchor != 0U ? CC_ROAD_ANCHOR_SITE : CC_ROAD_ANCHOR_NONE;
}

static CcId MainSegmentAt(const CcPilotRoadTopology *pilot,
                          int32_t coordinate, CcRoadDirection direction)
{
    if (coordinate < pilot->origin_to_junction_units)
        return pilot->origin_segment_id;
    if (coordinate > pilot->origin_to_junction_units)
        return pilot->destination_segment_id;
    return direction == CC_ROAD_DIRECTION_REVERSE ?
        pilot->origin_segment_id : pilot->destination_segment_id;
}

static bool IsPilotMill(const CcPilotRoadTopology *pilot,
                        const CcRoadSite *site)
{
    return site != NULL && site->id == pilot->mill_site_id;
}

static bool NextMainAnchor(const CcSim *sim,
                           const CcPilotRoadTopology *pilot,
                           int32_t coordinate, CcRoadDirection direction,
                           CcId *anchor, CcRoadAnchorKind *kind,
                           int32_t *destination)
{
    if (sim == NULL || pilot == NULL || anchor == NULL || kind == NULL ||
        destination == NULL || direction == CC_ROAD_DIRECTION_NONE) {
        return false;
    }
    int32_t best = direction == CC_ROAD_DIRECTION_FORWARD ?
        pilot->main_length_units : 0;
    CcId best_anchor = direction == CC_ROAD_DIRECTION_FORWARD ?
        pilot->destination_id : pilot->origin_id;
    CcRoadAnchorKind best_kind = CC_ROAD_ANCHOR_SETTLEMENT;
#define CONSIDER_MAIN_STOP(stop_coordinate, stop_anchor, stop_kind) do { \
    int32_t candidate_coordinate = (stop_coordinate); \
    bool beyond = direction == CC_ROAD_DIRECTION_FORWARD ? \
        candidate_coordinate > coordinate : candidate_coordinate < coordinate; \
    bool nearer = direction == CC_ROAD_DIRECTION_FORWARD ? \
        candidate_coordinate < best : candidate_coordinate > best; \
    if (beyond && nearer) { \
        best = candidate_coordinate; \
        best_anchor = (stop_anchor); \
        best_kind = (stop_kind); \
    } \
} while (0)
    CONSIDER_MAIN_STOP(pilot->checkpoint_distance_units,
                       pilot->checkpoint_id, CC_ROAD_ANCHOR_CHECKPOINT);
    CONSIDER_MAIN_STOP(pilot->origin_to_junction_units,
                       pilot->junction_id, CC_ROAD_ANCHOR_JUNCTION);
    for (int32_t i = 0; i < sim->road_site_count; ++i) {
        const CcRoadSite *site = &sim->road_sites[i];
        if (site->route_id != pilot->route_id || IsPilotMill(pilot, site) ||
            (sim->journey.road_site_stop_mask & (UINT32_C(1) << i)) != 0U)
            continue;
        CONSIDER_MAIN_STOP(
            CcRoadScaleDistance(pilot->main_length_units,
                                site->progress_milli),
            site->id, CC_ROAD_ANCHOR_SITE);
    }
#undef CONSIDER_MAIN_STOP
    if ((direction == CC_ROAD_DIRECTION_FORWARD && best <= coordinate) ||
        (direction == CC_ROAD_DIRECTION_REVERSE && best >= coordinate)) {
        return false;
    }
    *anchor = best_anchor;
    *kind = best_kind;
    *destination = best;
    return true;
}

static void RefreshCompatibility(CcSim *sim,
                                 const CcPilotRoadTopology *pilot)
{
    int32_t milli = CcRoadProgressMilli(
        sim->journey.road_coordinate_units, pilot->main_length_units);
    if (sim->journey.origin_id == pilot->destination_id) milli = 1000 - milli;
    sim->journey.road_compatibility_milli = milli;
    sim->carriage.progress_milli = milli;
}

static bool SetPilotLeg(CcSim *sim, const CcPilotRoadTopology *pilot,
                        CcId segment, CcRoadDirection direction,
                        int32_t start_coordinate, int32_t end_coordinate,
                        CcId start_anchor, CcId stop_anchor)
{
    int32_t length = end_coordinate >= start_coordinate ?
        end_coordinate - start_coordinate : start_coordinate - end_coordinate;
    if (length <= 0) return false;
    int32_t leg_time = CcRoadTravelSubticks(
        length, sim->journey.total_subticks, pilot->main_length_units);
    if (leg_time <= 0) leg_time = 1;
    sim->journey.road_segment_id = segment;
    sim->journey.road_direction = (int32_t)direction;
    sim->journey.road_anchor_id = start_anchor;
    sim->journey.road_stop_anchor_id = stop_anchor;
    sim->journey.road_return_anchor_id = start_anchor;
    sim->journey.road_leg_start_coordinate_units = start_coordinate;
    sim->journey.road_leg_end_coordinate_units = end_coordinate;
    sim->journey.road_coordinate_units = start_coordinate;
    sim->journey.road_leg_length_units = length;
    sim->journey.road_distance_travelled_units = 0;
    sim->journey.road_distance_remaining_units = length;
    sim->journey.road_leg_elapsed_subticks = 0;
    sim->journey.road_leg_total_subticks = leg_time;
    sim->journey.road_waiting_choice = false;
    sim->journey.road_revision++;
    sim->journey.phase = CC_JOURNEY_PHASE_TRAVELLING;
    sim->clock.game_minutes_per_second = CC_TRAVEL_GAME_MINUTES_PER_SECOND;
    sim->carriage.mode = CC_CARRIAGE_MOVING;
    sim->carriage.speed_milli_per_second = CcJourneyCarriageSpeedForPace(
        sim->journey.total_subticks, sim->journey.pace);
    if (segment != pilot->mill_segment_id) RefreshCompatibility(sim, pilot);
    return true;
}

static bool SetNextMainLeg(CcSim *sim, const CcPilotRoadTopology *pilot,
                           CcRoadDirection direction)
{
    CcId stop = 0U;
    CcRoadAnchorKind kind = CC_ROAD_ANCHOR_NONE;
    int32_t destination = 0;
    int32_t coordinate = sim->journey.road_coordinate_units;
    if (!NextMainAnchor(sim, pilot, coordinate, direction,
                        &stop, &kind, &destination)) return false;
    (void)kind;
    return SetPilotLeg(sim, pilot,
        MainSegmentAt(pilot, coordinate, direction), direction,
        coordinate, destination, sim->journey.road_anchor_id, stop);
}

bool CcRoadBeginPilotJourney(CcSim *sim, CcId journey_id)
{
    if (sim == NULL || !sim->journey.active || journey_id == 0U) return false;
    CcPilotRoadTopology pilot;
    CcRoadGeometry geometry;
    if (!CcPilotRoadTopologyBuild(sim, &pilot) ||
        sim->journey.route_id != pilot.route_id ||
        !CcRoadGeometryBuild(sim, pilot.route_id, &geometry)) return false;
    bool from_origin = sim->journey.origin_id == pilot.origin_id &&
        sim->journey.destination_id == pilot.destination_id;
    bool from_destination = sim->journey.origin_id == pilot.destination_id &&
        sim->journey.destination_id == pilot.origin_id;
    if (!from_origin && !from_destination) return false;
    sim->journey.road_position_active = true;
    sim->journey.road_journey_id = journey_id;
    sim->journey.road_goal_id = sim->journey.destination_id;
    sim->journey.road_geometry_length_units = geometry.journey_length_units;
    sim->journey.road_revision = 0U;
    for (int32_t i = 0; i < CC_ROAD_GEOMETRY_SAMPLE_COUNT; ++i) {
        sim->journey.road_geometry_x_units[i] = geometry.samples[i].x_units;
        sim->journey.road_geometry_z_units[i] = geometry.samples[i].z_units;
    }
    int32_t coordinate = from_origin ? 0 : pilot.main_length_units;
    sim->journey.road_coordinate_units = coordinate;
    sim->journey.road_anchor_id = sim->journey.origin_id;
    return SetNextMainLeg(sim, &pilot, from_origin ?
        CC_ROAD_DIRECTION_FORWARD : CC_ROAD_DIRECTION_REVERSE);
}

static bool AddPreview(const CcSim *sim,
                       const CcPilotRoadTopology *pilot,
                       CcRoadLegPreview *previews, int32_t capacity,
                       int32_t *count, CcId segment,
                       CcRoadDirection direction, CcId destination_anchor,
                       CcRoadAnchorKind kind, int32_t length)
{
    if (*count >= capacity || length <= 0) return false;
    CcRoadLegPreview *preview = &previews[(*count)++];
    *preview = (CcRoadLegPreview){
        .journey_goal_id = sim->journey.road_goal_id,
        .anchor_id = sim->journey.road_anchor_id,
        .segment_id = segment,
        .destination_anchor_id = destination_anchor,
        .next_named_place_id = destination_anchor,
        .destination_kind = kind,
        .direction = direction,
        .length_units = length,
        .travel_subticks = CcRoadTravelSubticks(
            length, sim->journey.total_subticks, pilot->main_length_units),
        .journey_revision = sim->journey.road_revision
    };
    preview->decision_token = CcRoadPreviewToken(
        sim->journey.road_journey_id, sim->journey.road_goal_id,
        sim->journey.road_anchor_id, sim->journey.road_coordinate_units,
        sim->journey.road_distance_travelled_units,
        sim->journey.road_distance_remaining_units,
        sim->journey.road_revision, segment, direction);
    return true;
}

int32_t CcRoadNextLegPreviews(const CcSim *sim,
                              CcRoadLegPreview *previews,
                              int32_t capacity)
{
    if (sim == NULL || previews == NULL || capacity <= 0 ||
        !sim->journey.active || !sim->journey.road_position_active) return 0;
    CcPilotRoadTopology pilot;
    if (!CcPilotRoadTopologyBuildWithLength(
            sim, sim->journey.road_geometry_length_units, &pilot)) return 0;
    int32_t count = 0;
    int32_t coordinate = sim->journey.road_coordinate_units;
    if (!sim->journey.road_waiting_choice) {
        AddPreview(sim, &pilot, previews, capacity, &count,
            sim->journey.road_segment_id,
            (CcRoadDirection)-sim->journey.road_direction,
            sim->journey.road_return_anchor_id,
            PilotAnchorKind(&pilot, sim->journey.road_return_anchor_id),
            sim->journey.road_distance_travelled_units);
        return count;
    }
    if (sim->journey.road_anchor_id == pilot.mill_site_id) {
        AddPreview(sim, &pilot, previews, capacity, &count,
            pilot.mill_segment_id, CC_ROAD_DIRECTION_REVERSE,
            pilot.junction_id, CC_ROAD_ANCHOR_JUNCTION,
            pilot.mill_spur_length_units);
        return count;
    }
    if (sim->journey.road_anchor_id == pilot.junction_id) {
        AddPreview(sim, &pilot, previews, capacity, &count,
            pilot.mill_segment_id, CC_ROAD_DIRECTION_FORWARD,
            pilot.mill_site_id, CC_ROAD_ANCHOR_SITE,
            pilot.mill_spur_length_units);
    }
    for (int32_t value = -1; value <= 1; value += 2) {
        CcRoadDirection direction = (CcRoadDirection)value;
        CcId anchor = 0U;
        CcRoadAnchorKind kind = CC_ROAD_ANCHOR_NONE;
        int32_t destination = 0;
        if (NextMainAnchor(sim, &pilot, coordinate, direction,
                           &anchor, &kind, &destination)) {
            AddPreview(sim, &pilot, previews, capacity, &count,
                MainSegmentAt(&pilot, coordinate, direction), direction,
                anchor, kind, destination >= coordinate ?
                    destination - coordinate : coordinate - destination);
        }
    }
    return count;
}

static void MarkCurrentRoadSitePassed(CcSim *sim)
{
    for (int32_t i = 0; i < sim->road_site_count; ++i) {
        if (sim->road_sites[i].id == sim->journey.road_anchor_id) {
            sim->journey.road_site_stop_mask |= UINT32_C(1) << i;
            return;
        }
    }
}

bool CcRoadChooseNextLeg(CcSim *sim, uint64_t decision_token,
                         char *error, size_t error_capacity)
{
    if (sim == NULL || decision_token == 0U) {
        RoadError(error, error_capacity, "Choose a current road leg.");
        return false;
    }
    CcRoadLegPreview previews[3];
    int32_t count = CcRoadNextLegPreviews(sim, previews, 3);
    const CcRoadLegPreview *chosen = NULL;
    for (int32_t i = 0; i < count; ++i) {
        if (previews[i].decision_token == decision_token) chosen = &previews[i];
    }
    if (chosen == NULL) {
        RoadError(error, error_capacity,
                  "That road choice is stale. Choose from the current position.");
        return false;
    }
    CcPilotRoadTopology pilot;
    if (!CcPilotRoadTopologyBuildWithLength(
            sim, sim->journey.road_geometry_length_units, &pilot)) {
        RoadError(error, error_capacity, "The saved road position is invalid.");
        return false;
    }
    CcId old_anchor = sim->journey.road_anchor_id;
    int32_t start = sim->journey.road_coordinate_units;
    int32_t end = start;
    if (chosen->segment_id == pilot.mill_segment_id) {
        start = chosen->direction == CC_ROAD_DIRECTION_FORWARD ? 0 :
            pilot.mill_spur_length_units;
        end = chosen->direction == CC_ROAD_DIRECTION_FORWARD ?
            pilot.mill_spur_length_units : 0;
    } else {
        end = chosen->direction == CC_ROAD_DIRECTION_FORWARD ?
            start + chosen->length_units : start - chosen->length_units;
    }
    MarkCurrentRoadSitePassed(sim);
    if (chosen->destination_kind == CC_ROAD_ANCHOR_SETTLEMENT) {
        sim->journey.road_goal_id = chosen->destination_anchor_id;
        sim->journey.destination_id = chosen->destination_anchor_id;
        sim->carriage.destination_id = chosen->destination_anchor_id;
    }
    if (!SetPilotLeg(sim, &pilot, chosen->segment_id, chosen->direction,
                     start, end, old_anchor,
                     chosen->destination_anchor_id)) {
        RoadError(error, error_capacity, "That road leg has no distance.");
        return false;
    }
    RoadError(error, error_capacity, "");
    return true;
}

bool CcRoadAdvanceLeg(CcSim *sim, int32_t journey_subticks)
{
    if (sim == NULL || journey_subticks <= 0 ||
        !sim->journey.road_position_active ||
        sim->journey.road_waiting_choice ||
        sim->journey.road_leg_total_subticks <= 0) return false;
    CcPilotRoadTopology pilot;
    if (!CcPilotRoadTopologyBuildWithLength(
            sim, sim->journey.road_geometry_length_units, &pilot)) return false;
    int32_t remaining_time = sim->journey.road_leg_total_subticks -
        sim->journey.road_leg_elapsed_subticks;
    int32_t elapsed = journey_subticks < remaining_time ?
        journey_subticks : remaining_time;
    sim->journey.road_leg_elapsed_subticks += elapsed;
    int32_t travelled = (int32_t)(
        ((int64_t)sim->journey.road_leg_length_units *
             sim->journey.road_leg_elapsed_subticks +
         sim->journey.road_leg_total_subticks / 2) /
        sim->journey.road_leg_total_subticks);
    if (travelled > sim->journey.road_leg_length_units)
        travelled = sim->journey.road_leg_length_units;
    sim->journey.road_distance_travelled_units = travelled;
    sim->journey.road_distance_remaining_units =
        sim->journey.road_leg_length_units - travelled;
    int32_t signed_travel = sim->journey.road_direction ==
        CC_ROAD_DIRECTION_FORWARD ? travelled : -travelled;
    sim->journey.road_coordinate_units =
        sim->journey.road_leg_start_coordinate_units + signed_travel;
    if (sim->journey.road_segment_id != pilot.mill_segment_id)
        RefreshCompatibility(sim, &pilot);
    if (sim->journey.road_distance_remaining_units > 0) return false;
    sim->journey.road_anchor_id = sim->journey.road_stop_anchor_id;
    sim->journey.road_return_anchor_id = sim->journey.road_anchor_id;
    sim->journey.road_waiting_choice = true;
    sim->journey.phase = CC_JOURNEY_PHASE_ROAD_CHOICE;
    sim->clock.game_minutes_per_second = CC_IDLE_GAME_MINUTES_PER_SECOND;
    sim->carriage.mode = CC_CARRIAGE_STOPPED;
    sim->carriage.speed_milli_per_second = 0;
    if (sim->journey.road_segment_id == pilot.mill_segment_id &&
        sim->journey.road_anchor_id == pilot.junction_id) {
        sim->journey.road_coordinate_units =
            pilot.origin_to_junction_units;
        RefreshCompatibility(sim, &pilot);
    }
    return true;
}

bool CcRoadSavedPositionValid(const CcSim *sim)
{
    if (sim == NULL || !sim->journey.road_position_active) return true;
    CcPilotRoadTopology pilot;
    if (!sim->journey.active || sim->journey.road_journey_id == 0U ||
        sim->journey.route_id == 0U ||
        !CcPilotRoadTopologyBuildWithLength(
            sim, sim->journey.road_geometry_length_units, &pilot) ||
        sim->journey.route_id != pilot.route_id ||
        (sim->journey.road_goal_id != pilot.origin_id &&
         sim->journey.road_goal_id != pilot.destination_id) ||
        (sim->journey.road_direction != CC_ROAD_DIRECTION_FORWARD &&
         sim->journey.road_direction != CC_ROAD_DIRECTION_REVERSE) ||
        sim->journey.road_leg_length_units <= 0 ||
        sim->journey.road_distance_travelled_units < 0 ||
        sim->journey.road_distance_remaining_units < 0 ||
        sim->journey.road_distance_travelled_units +
            sim->journey.road_distance_remaining_units !=
            sim->journey.road_leg_length_units ||
        sim->journey.road_leg_elapsed_subticks < 0 ||
        sim->journey.road_leg_total_subticks <= 0 ||
        sim->journey.road_leg_elapsed_subticks >
            sim->journey.road_leg_total_subticks ||
        sim->journey.road_compatibility_milli < 0 ||
        sim->journey.road_compatibility_milli > 1000 ||
        (sim->journey.road_waiting_choice &&
         (sim->journey.road_distance_remaining_units != 0 ||
          sim->journey.phase != CC_JOURNEY_PHASE_ROAD_CHOICE ||
          sim->journey.road_anchor_id !=
              sim->journey.road_stop_anchor_id)) ||
        (!sim->journey.road_waiting_choice &&
         sim->journey.phase != CC_JOURNEY_PHASE_TRAVELLING)) return false;
    bool spur = sim->journey.road_segment_id == pilot.mill_segment_id;
    bool spur_returned = spur && sim->journey.road_waiting_choice &&
        sim->journey.road_anchor_id == pilot.junction_id &&
        sim->journey.road_direction == CC_ROAD_DIRECTION_REVERSE;
    int32_t maximum = spur ? pilot.mill_spur_length_units :
        pilot.main_length_units;
    if ((!spur_returned && (sim->journey.road_coordinate_units < 0 ||
        sim->journey.road_coordinate_units > maximum)) ||
        sim->journey.road_leg_start_coordinate_units < 0 ||
        sim->journey.road_leg_start_coordinate_units > maximum ||
        sim->journey.road_leg_end_coordinate_units < 0 ||
        sim->journey.road_leg_end_coordinate_units > maximum) return false;
    int32_t expected_coordinate =
        sim->journey.road_leg_start_coordinate_units +
        (sim->journey.road_direction == CC_ROAD_DIRECTION_FORWARD ?
            sim->journey.road_distance_travelled_units :
           -sim->journey.road_distance_travelled_units);
    if (!spur_returned &&
        expected_coordinate != sim->journey.road_coordinate_units)
        return false;
    if (spur_returned && sim->journey.road_coordinate_units !=
            pilot.origin_to_junction_units) return false;
    int64_t saved_length = 0;
    for (int32_t i = CC_ROAD_GEOMETRY_FROM_JUNCTION_SAMPLE;
         i < CC_ROAD_GEOMETRY_TO_JUNCTION_SAMPLE; ++i) {
        CcRoadGeometryPoint first = {
            sim->journey.road_geometry_x_units[i],
            sim->journey.road_geometry_z_units[i]
        };
        CcRoadGeometryPoint second = {
            sim->journey.road_geometry_x_units[i + 1],
            sim->journey.road_geometry_z_units[i + 1]
        };
        saved_length += PointDistanceUnits(first, second);
    }
    return saved_length == sim->journey.road_geometry_length_units;
}
