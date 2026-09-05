typedef struct NavPlatform {
    float x;
    float z;
    float width;
    float depth;
    float height;
    int32_t style;
} NavPlatform;

static const NavPlatform STREET_PLATFORMS[] = {
    {3.00f, 7.00f, 1.00f, 1.00f, 1.65f, 0},
    {10.15f, 0.85f, 0.70f, 0.70f, 0.90f, 1},
    {11.75f, 1.45f, 1.05f, 1.05f, 1.05f, 2},
    {13.10f, 2.35f, 0.95f, 1.20f, 1.52f, 3},
    {10.20f, 3.55f, 2.70f, 0.62f, 1.65f, 2},
    {9.80f, 5.15f, 0.95f, 0.95f, 1.35f, 3},
    {11.25f, 5.70f, 0.82f, 0.82f, 0.92f, 1},
    {12.55f, 6.40f, 1.10f, 1.10f, 1.65f, 2},
    {9.85f, 7.80f, 2.60f, 1.00f, 0.90f, 1},
    {13.10f, 8.65f, 0.95f, 0.95f, 1.60f, 3}
};


static const NavPlatform STREET_CRATE_PLATFORMS[] = {
    {44.09f, 26.44f, 0.62f, 0.62f, 0.60f, 1},
    {44.81f, 26.44f, 0.62f, 0.62f, 0.60f, 1},
    {44.09f, 27.16f, 0.62f, 0.62f, 0.60f, 1},
    {44.81f, 27.16f, 0.62f, 0.62f, 0.60f, 1},
};
static int32_t street_crate_platform_count = 0;

void CcLocalSetStreetMarketCratesInternal(int32_t count)
{
    street_crate_platform_count = count < 0 ? 0 : count > 4 ? 4 : count;
}

static int32_t StreetPhysicsPlatformCount(void)
{
    return (int32_t)(sizeof(STREET_PLATFORMS) /
                     sizeof(STREET_PLATFORMS[0])) +
           street_crate_platform_count;
}

static const NavPlatform *StreetPhysicsPlatformAt(int32_t index)
{
    int32_t static_count = (int32_t)(sizeof(STREET_PLATFORMS) /
                                     sizeof(STREET_PLATFORMS[0]));
    if (index < 0) return NULL;
    if (index < static_count) return &STREET_PLATFORMS[index];
    index -= static_count;
    if (index >= street_crate_platform_count) return NULL;
    return &STREET_CRATE_PLATFORMS[index];
}


#define CC_TERRAIN_CELL_SIZE 0.50f
#define CC_TERRAIN_COLUMNS 193
#define CC_TERRAIN_ROWS 145
#define CC_TERRAIN_SAMPLE_COUNT (CC_TERRAIN_COLUMNS * CC_TERRAIN_ROWS)

typedef struct TerrainRoad {
    Rectangle footprint;
    bool runs_east_west;
} TerrainRoad;

static const TerrainRoad TERRAIN_ROADS[] = {
    {{14.70f, 26.80f, 77.60f, 5.20f}, true},
    {{39.40f, 7.70f, 5.20f, 50.60f}, false},
    {{7.20f, 10.00f, 34.80f, 3.80f}, true},
    {{76.00f, 27.00f, 5.00f, 27.00f}, false},
    {{14.00f, 54.20f, 28.00f, 2.70f}, true},
    {{12.30f, 26.30f, 4.80f, 5.80f}, false},
    {{29.00f, 23.30f, 13.00f, 4.50f}, true},
    {{39.50f, 53.80f, 4.90f, 4.20f}, false},
    {{47.00f, 25.25f, 6.00f, 1.38f}, true},
    {{54.20f, 50.10f, 18.70f, 2.90f}, true},
    {{75.80f, 27.00f, 5.20f, 5.20f}, false},
    {{62.80f, 27.00f, 2.60f, 11.50f}, false},
    {{63.80f, 32.40f, 17.50f, 3.20f}, true},
    {{39.50f, 53.50f, 16.20f, 3.00f}, true},

    {{51.00f, 37.00f, 45.00f, 5.60f}, true},
};

static uint32_t street_terrain_seed = UINT32_C(0xc0a71a9e);
static bool street_terrain_ready = false;
static CcSettlementFunction active_place_function = CC_SETTLEMENT_MARKET;
static float street_terrain_natural[CC_TERRAIN_SAMPLE_COUNT];
static float street_terrain_height[CC_TERRAIN_SAMPLE_COUNT];

static const CcLocalPlaceLandmark *ActivePlaceLandmarkAt(int32_t index)
{
    return CcLocalPlaceLandmarkAt(active_place_function, index);
}

static const CcLocalPlaceRoad *ActivePlaceRoadAt(int32_t index)
{
    return CcLocalPlaceRoadAt(active_place_function, index);
}

static CcLocalRoadSurface ActiveTownRoadSurface(void)
{
    const CcLocalPlaceRoad *road = ActivePlaceRoadAt(0);
    return road != NULL ? road->surface : CC_LOCAL_ROAD_TRADE;
}

static const CcLocalPlaceRoad *ActiveCarriageRouteAt(int32_t index)
{
    return CcLocalPlaceCarriageRouteAt(active_place_function, index);
}

static const CcLocalPlaceProfile *ActivePlaceProfile(void)
{
    return CcLocalPlaceProfileForFunction(active_place_function);
}

static int32_t ActiveWorldBuildingCount(void)
{
    return ActivePlaceProfile()->building_count;
}

static WorldBuilding ActiveWorldBuildingAt(int32_t index)
{
    const CcLocalPlaceBuilding *building =
        CcLocalPlaceBuildingAt(active_place_function, index);
    if (building == NULL) return (WorldBuilding){0};
    return (WorldBuilding){
        {building->x, building->z, building->width, building->depth},
        building->height,
        (int32_t)building->style,
        building->door,
    };
}

static int32_t ActiveCompoundStructureCount(void)
{
    return ActivePlaceProfile()->compound_structure_count;
}

static WorldStructure ActiveCompoundStructureAt(int32_t index)
{
    const CcLocalPlaceCompoundStructure *structure =
        CcLocalPlaceCompoundStructureAt(active_place_function, index);
    if (structure == NULL) return (WorldStructure){0};
    return (WorldStructure){
        {structure->x, structure->z, structure->width, structure->depth},
        structure->height,
        structure->kind,
    };
}

static Rectangle ActiveCompoundBounds(void)
{
    Rectangle bounds = {0};
    int32_t count = ActiveCompoundStructureCount();
    for (int32_t index = 0; index < count; ++index) {
        Rectangle footprint = ActiveCompoundStructureAt(index).footprint;
        if (index == 0) {
            bounds = footprint;
            continue;
        }
        float maximum_x = fmaxf(bounds.x + bounds.width,
                                footprint.x + footprint.width);
        float maximum_z = fmaxf(bounds.y + bounds.height,
                                footprint.y + footprint.height);
        bounds.x = fminf(bounds.x, footprint.x);
        bounds.y = fminf(bounds.y, footprint.y);
        bounds.width = maximum_x - bounds.x;
        bounds.height = maximum_z - bounds.y;
    }
    return bounds;
}

static float ActiveCompoundMaximumHeight(void)
{
    float height = 0.0f;
    for (int32_t index = 0;
         index < ActiveCompoundStructureCount(); ++index) {
        height = fmaxf(height, ActiveCompoundStructureAt(index).height);
    }
    return height;
}

static WorldBuilding ActivePrimaryBuilding(void)
{
    return ActiveWorldBuildingAt(ActivePlaceProfile()->primary_building);
}

static Rectangle PlaceLandmarkFootprint(
    const CcLocalPlaceLandmark *landmark)
{
    if (landmark == NULL) return (Rectangle){0};
    return (Rectangle){landmark->x, landmark->z,
                       landmark->width, landmark->depth};
}

static TerrainRoad PlaceTerrainRoad(const CcLocalPlaceRoad *road)
{
    if (road == NULL) return (TerrainRoad){0};
    return (TerrainRoad){
        {road->x, road->z, road->width, road->depth},
        road->runs_east_west,
    };
}

static int32_t TerrainCarriageRoadIndexOffset(void)
{
    return (int32_t)(sizeof(TERRAIN_ROADS) / sizeof(TERRAIN_ROADS[0])) +
           CC_LOCAL_PLACE_ROAD_COUNT;
}

static bool TerrainRoadIndexIsCarriageLane(int32_t road_index)
{
    int32_t carriage_index = road_index - TerrainCarriageRoadIndexOffset();
    return carriage_index >= 0 && carriage_index < 3;
}

typedef struct TerrainRenderCache {
    Model model;
    uint32_t seed;
    CcId place_id;
    int32_t hunger_band;
    int32_t prosperity_band;
    int32_t vertex_count;
    bool ready;
} TerrainRenderCache;

static TerrainRenderCache terrain_render_cache = {0};
static TerrainRenderCache terrain_detail_render_cache = {0};
static void TerrainRenderCacheClear(void);
static void TerrainDetailRenderCacheClear(void);

static float TerrainClamp(float value, float minimum, float maximum)
{
    return fmaxf(minimum, fminf(maximum, value));
}

static float TerrainSmooth01(float amount)
{
    amount = TerrainClamp(amount, 0.0f, 1.0f);
    return amount * amount * (3.0f - 2.0f * amount);
}


static float TerrainGentleRamp01(float amount)
{
    const float ease = 0.14f;
    amount = TerrainClamp(amount, 0.0f, 1.0f);
    if (amount < ease) {
        return amount * amount / (2.0f * ease * (1.0f - ease));
    }
    if (amount > 1.0f - ease) {
        float remaining = 1.0f - amount;
        return 1.0f - remaining * remaining /
                      (2.0f * ease * (1.0f - ease));
    }
    return (amount - ease * 0.5f) / (1.0f - ease);
}

static uint32_t TerrainMix(uint32_t value)
{
    value ^= value >> 16U;
    value *= UINT32_C(0x7feb352d);
    value ^= value >> 15U;
    value *= UINT32_C(0x846ca68b);
    return value ^ (value >> 16U);
}

static float TerrainLatticeValue(int32_t x, int32_t z, uint32_t stream)
{
    uint32_t sample = TerrainMix(
        street_terrain_seed ^ ((uint32_t)x * UINT32_C(0x9e3779b9)) ^
        ((uint32_t)z * UINT32_C(0x85ebca6b)) ^
        (stream * UINT32_C(0xc2b2ae35)));
    return (float)(sample & UINT32_C(0x00ffffff)) /
               (float)UINT32_C(0x00ffffff) *
               2.0f -
           1.0f;
}

static float TerrainValueNoise(float x, float z, float wavelength,
                               uint32_t stream)
{
    float sample_x = x / wavelength;
    float sample_z = z / wavelength;
    int32_t x0 = (int32_t)floorf(sample_x);
    int32_t z0 = (int32_t)floorf(sample_z);
    float tx = TerrainSmooth01(sample_x - (float)x0);
    float tz = TerrainSmooth01(sample_z - (float)z0);
    float bottom = TerrainLatticeValue(x0, z0, stream) +
        (TerrainLatticeValue(x0 + 1, z0, stream) -
         TerrainLatticeValue(x0, z0, stream)) *
            tx;
    float top = TerrainLatticeValue(x0, z0 + 1, stream) +
        (TerrainLatticeValue(x0 + 1, z0 + 1, stream) -
         TerrainLatticeValue(x0, z0 + 1, stream)) *
            tx;
    return bottom + (top - bottom) * tz;
}

static float TerrainLandform(float x, float z, float center_x,
                             float center_z, float radius_x,
                             float radius_z)
{
    float dx = (x - center_x) / radius_x;
    float dz = (z - center_z) / radius_z;
    return expf(-(dx * dx + dz * dz));
}

static float TerrainNaturalHeight(float x, float z)
{
    float broad = TerrainValueNoise(x, z, 34.0f, 1U) * 1.40f;
    float hills = TerrainValueNoise(x + 19.0f, z - 11.0f,
                                    18.0f, 2U) * 0.82f;
    float detail = TerrainValueNoise(x - 7.0f, z + 23.0f,
                                     7.0f, 3U) * 0.26f;
    float land = 1.20f + broad + hills + detail;
    switch (active_place_function) {
        case CC_SETTLEMENT_FARMING: {
            float river_center = 91.0f + sinf(z * 0.082f) * 1.7f;
            float river_distance = (x - river_center) / 4.8f;
            land -= 8.50f * expf(-river_distance * river_distance);
            land += TerrainLandform(x, z, 45.0f, 28.0f,
                                    25.0f, 17.0f) * 1.30f;
            land += TerrainLandform(x, z, 79.0f, 18.0f,
                                    21.0f, 15.0f) * 5.20f;
            land += TerrainLandform(x, z, 68.0f, 2.0f,
                                    34.0f, 13.0f) * 2.40f;
            break;
        }
        case CC_SETTLEMENT_MINING:
            land += x * 0.040f;
            land += TerrainSmooth01((54.0f - z) / 46.0f) * 5.20f;
            land += TerrainValueNoise(x + 8.0f, z - 4.0f,
                                      11.0f, 41U) * 1.25f;
            land -= TerrainLandform(x, z, 82.0f, 35.0f,
                                    21.0f, 15.0f) * 6.80f;
            land -= TerrainLandform(x, z, 48.0f, 30.0f,
                                    23.0f, 17.0f) * 3.80f;
            land += TerrainLandform(x, z, 78.0f, 14.0f,
                                    23.0f, 15.0f) * 13.20f;
            land += TerrainSmooth01((20.0f - z) / 13.0f) * 4.80f;
            break;
        case CC_SETTLEMENT_MARKET:
            land += TerrainLandform(x, z, 47.0f, 30.0f,
                                    39.0f, 29.0f) * 3.40f;
            land -= TerrainLandform(x, z, 47.0f, 30.0f,
                                    21.0f, 16.0f) * 3.10f;
            land += TerrainLandform(x, z, 79.0f, 16.0f,
                                    25.0f, 18.0f) * 11.40f;
            land += TerrainSmooth01((19.0f - z) / 15.0f) * 3.20f;
            break;
        case CC_SETTLEMENT_FORTRESS: {
            float ravine_center = 91.0f + sinf(z * 0.052f) * 1.2f;
            float ravine_distance = (x - ravine_center) / 4.5f;
            land -= 10.20f * expf(-ravine_distance * ravine_distance);
            land += TerrainLandform(x, z, 48.0f, 27.0f,
                                    32.0f, 16.0f) * 5.00f;
            land += TerrainLandform(x, z, 79.0f, 18.0f,
                                    21.0f, 15.0f) * 13.00f;
            land += TerrainSmooth01((19.0f - z) / 14.0f) * 4.80f;
            break;
        }
        case CC_SETTLEMENT_CAPITAL:
            land += TerrainSmooth01((x - 14.0f) / 68.0f) * 9.20f;
            land += TerrainSmooth01((50.0f - z) / 43.0f) * 3.80f;
            land += TerrainLandform(x, z, 79.0f, 18.0f,
                                    23.0f, 17.0f) * 7.40f;
            land += TerrainLandform(x, z, 46.0f, 29.0f,
                                    24.0f, 17.0f) * 2.80f;
            break;
        case CC_SETTLEMENT_DUNGEON_TOWN:
            land += TerrainLandform(x, z, 51.0f, 30.0f,
                                    39.0f, 27.0f) * 3.10f;
            land -= TerrainLandform(x, z, 47.0f, 30.0f,
                                    18.0f, 13.0f) * 6.20f;
            land += TerrainLandform(x, z, 79.0f, 17.0f,
                                    22.0f, 15.0f) * 13.80f;
            land += TerrainSmooth01((18.0f - z) / 14.0f) * 4.50f;
            break;
    }
    return land;
}

static int32_t TerrainIndex(int32_t column, int32_t row)
{
    return row * CC_TERRAIN_COLUMNS + column;
}

static float TerrainSampleGrid(const float *samples, float x, float z)
{
    x = TerrainClamp(x, 0.0f, CC_LOCAL_WORLD_WIDTH);
    z = TerrainClamp(z, 0.0f, CC_LOCAL_WORLD_DEPTH);
    float grid_x = x / CC_TERRAIN_CELL_SIZE;
    float grid_z = z / CC_TERRAIN_CELL_SIZE;
    int32_t column = (int32_t)floorf(grid_x);
    int32_t row = (int32_t)floorf(grid_z);
    if (column >= CC_TERRAIN_COLUMNS - 1) column = CC_TERRAIN_COLUMNS - 2;
    if (row >= CC_TERRAIN_ROWS - 1) row = CC_TERRAIN_ROWS - 2;
    float tx = grid_x - (float)column;
    float tz = grid_z - (float)row;
    float h00 = samples[TerrainIndex(column, row)];
    float h10 = samples[TerrainIndex(column + 1, row)];
    float h01 = samples[TerrainIndex(column, row + 1)];
    float h11 = samples[TerrainIndex(column + 1, row + 1)];
    float bottom = h00 + (h10 - h00) * tx;
    float top = h01 + (h11 - h01) * tx;
    return bottom + (top - bottom) * tz;
}

static float TerrainDistanceToRectangle(float x, float z,
                                        Rectangle footprint)
{
    float closest_x = TerrainClamp(x, footprint.x,
                                   footprint.x + footprint.width);
    float closest_z = TerrainClamp(z, footprint.y,
                                   footprint.y + footprint.height);
    float dx = x - closest_x;
    float dz = z - closest_z;
    return sqrtf(dx * dx + dz * dz);
}

static float TerrainRoadProfile(const TerrainRoad *road, float x, float z)
{
    float total = 0.0f;
    for (int32_t offset = -2; offset <= 2; ++offset) {
        float sample_x = road->runs_east_west ?
            TerrainClamp(x + (float)offset, road->footprint.x,
                         road->footprint.x + road->footprint.width) :
            road->footprint.x + road->footprint.width * 0.5f;
        float sample_z = road->runs_east_west ?
            road->footprint.y + road->footprint.height * 0.5f :
            TerrainClamp(z + (float)offset, road->footprint.y,
                         road->footprint.y + road->footprint.height);
        total += TerrainSampleGrid(street_terrain_natural,
                                   sample_x, sample_z);
    }
    return total / 5.0f;
}

static bool TerrainPointInsideMajorFoundation(float x, float z)
{
    for (int32_t i = 0; i < ActiveWorldBuildingCount(); ++i) {
        Rectangle footprint = ActiveWorldBuildingAt(i).footprint;
        if (x >= footprint.x && x <= footprint.x + footprint.width &&
            z >= footprint.y && z <= footprint.y + footprint.height) {
            return true;
        }
    }
    Rectangle keep = ActiveCompoundBounds();
    if (x >= keep.x && x <= keep.x + keep.width &&
        z >= keep.y && z <= keep.y + keep.height) return true;
    if (x >= CARRIAGE_FOOTPRINT.x &&
        x <= CARRIAGE_FOOTPRINT.x + CARRIAGE_FOOTPRINT.width &&
        z >= CARRIAGE_FOOTPRINT.y &&
        z <= CARRIAGE_FOOTPRINT.y + CARRIAGE_FOOTPRINT.height) {
        return true;
    }
    static const Rectangle local_pads[] = {
        {27.40f, 49.70f, 3.20f, 1.60f},
        {1.00f, 0.00f, 14.60f, 11.10f},
    };
    for (int32_t i = 0; i < (int32_t)(sizeof(local_pads) /
                                      sizeof(local_pads[0])); ++i) {
        Rectangle pad = local_pads[i];
        if (x >= pad.x && x <= pad.x + pad.width &&
            z >= pad.y && z <= pad.y + pad.height) return true;
    }
    for (int32_t i = 0; i < CC_LOCAL_PLACE_LANDMARK_COUNT; ++i) {
        Rectangle pad = PlaceLandmarkFootprint(ActivePlaceLandmarkAt(i));
        if (x >= pad.x && x <= pad.x + pad.width &&
            z >= pad.y && z <= pad.y + pad.height) return true;
    }
    return false;
}

static void TerrainGradeRoad(const TerrainRoad *road)
{
    const float shoulder = 1.85f;
    for (int32_t row = 0; row < CC_TERRAIN_ROWS; ++row) {
        float z = (float)row * CC_TERRAIN_CELL_SIZE;
        for (int32_t column = 0; column < CC_TERRAIN_COLUMNS; ++column) {
            float x = (float)column * CC_TERRAIN_CELL_SIZE;
            if (TerrainPointInsideMajorFoundation(x, z)) continue;
            float distance = TerrainDistanceToRectangle(
                x, z, road->footprint);
            if (distance >= shoulder) continue;
            bool inside = x >= road->footprint.x &&
                x <= road->footprint.x + road->footprint.width &&
                z >= road->footprint.y &&
                z <= road->footprint.y + road->footprint.height;
            float weight = inside ? 0.88f :
                0.88f * TerrainSmooth01(1.0f - distance / shoulder);
            int32_t index = TerrainIndex(column, row);
            float profile = TerrainRoadProfile(road, x, z);
            street_terrain_height[index] +=
                (profile - street_terrain_height[index]) * weight;
        }
    }
}

static void TerrainGradeActivePlaceRoads(void)
{
    for (int32_t road = 0; road < CC_LOCAL_PLACE_ROAD_COUNT; ++road) {
        TerrainRoad terrain_road = PlaceTerrainRoad(ActivePlaceRoadAt(road));
        TerrainGradeRoad(&terrain_road);
    }
}

static void TerrainGradeActiveCarriageLanes(void)
{
    for (int32_t route = 0; route < 3; ++route) {
        TerrainRoad terrain_road = PlaceTerrainRoad(
            ActiveCarriageRouteAt(route));
        TerrainGradeRoad(&terrain_road);
    }
}

static float TerrainRectangleAverage(Rectangle footprint)
{
    static const Vector2 samples[] = {
        {0.50f, 0.50f}, {0.12f, 0.12f}, {0.88f, 0.12f},
        {0.12f, 0.88f}, {0.88f, 0.88f},
    };
    float total = 0.0f;
    for (int32_t i = 0; i < (int32_t)(sizeof(samples) /
                                      sizeof(samples[0])); ++i) {
        total += TerrainSampleGrid(
            street_terrain_height,
            footprint.x + footprint.width * samples[i].x,
            footprint.y + footprint.height * samples[i].y);
    }
    return total / (float)(sizeof(samples) / sizeof(samples[0]));
}

static void TerrainGradePad(Rectangle footprint, float elevation,
                            float blend_distance)
{
    for (int32_t row = 0; row < CC_TERRAIN_ROWS; ++row) {
        float z = (float)row * CC_TERRAIN_CELL_SIZE;
        for (int32_t column = 0; column < CC_TERRAIN_COLUMNS; ++column) {
            float x = (float)column * CC_TERRAIN_CELL_SIZE;
            float distance = TerrainDistanceToRectangle(x, z, footprint);
            if (distance >= blend_distance) continue;
            bool inside = x >= footprint.x &&
                x <= footprint.x + footprint.width &&
                z >= footprint.y &&
                z <= footprint.y + footprint.height;
            float weight = inside ? 1.0f :
                TerrainSmooth01(1.0f - distance / blend_distance);
            int32_t index = TerrainIndex(column, row);
            street_terrain_height[index] +=
                (elevation - street_terrain_height[index]) * weight;
        }
    }
}

static void TerrainGradeActiveCarriageCourts(void)
{
    for (int32_t route = 3;
         route < CC_LOCAL_CARRIAGE_ROUTE_COUNT; ++route) {
        TerrainRoad court = PlaceTerrainRoad(ActiveCarriageRouteAt(route));
        float elevation = TerrainRectangleAverage(court.footprint);
        TerrainGradePad(court.footprint, elevation, 1.80f);
    }
}

static void TerrainGradeNorthSouthRamp(Rectangle footprint,
                                       float south_height,
                                       float north_height)
{
    const float shoulder = 0.85f;
    for (int32_t row = 0; row < CC_TERRAIN_ROWS; ++row) {
        float z = (float)row * CC_TERRAIN_CELL_SIZE;
        if (z < footprint.y || z > footprint.y + footprint.height) continue;
        float progress = TerrainSmooth01((z - footprint.y) /
                                         footprint.height);
        float elevation = south_height +
                          (north_height - south_height) * progress;
        for (int32_t column = 0; column < CC_TERRAIN_COLUMNS; ++column) {
            float x = (float)column * CC_TERRAIN_CELL_SIZE;
            float side_distance = x < footprint.x ? footprint.x - x :
                x > footprint.x + footprint.width ?
                    x - (footprint.x + footprint.width) : 0.0f;
            if (side_distance >= shoulder) continue;
            float weight = side_distance <= 0.0f ? 1.0f :
                TerrainSmooth01(1.0f - side_distance / shoulder);
            int32_t index = TerrainIndex(column, row);
            street_terrain_height[index] +=
                (elevation - street_terrain_height[index]) * weight;
        }
    }
}

static void TerrainGradeNorthSouthGentleRamp(Rectangle footprint,
                                             float south_height,
                                             float north_height)
{
    const float shoulder = 0.85f;
    for (int32_t row = 0; row < CC_TERRAIN_ROWS; ++row) {
        float z = (float)row * CC_TERRAIN_CELL_SIZE;
        if (z < footprint.y || z > footprint.y + footprint.height) continue;
        float progress = TerrainGentleRamp01((z - footprint.y) /
                                             footprint.height);
        float elevation = south_height +
                          (north_height - south_height) * progress;
        for (int32_t column = 0; column < CC_TERRAIN_COLUMNS; ++column) {
            float x = (float)column * CC_TERRAIN_CELL_SIZE;
            float side_distance = x < footprint.x ? footprint.x - x :
                x > footprint.x + footprint.width ?
                    x - (footprint.x + footprint.width) : 0.0f;
            if (side_distance >= shoulder) continue;
            float weight = side_distance <= 0.0f ? 1.0f :
                TerrainSmooth01(1.0f - side_distance / shoulder);
            int32_t index = TerrainIndex(column, row);
            street_terrain_height[index] +=
                (elevation - street_terrain_height[index]) * weight;
        }
    }
}

static void TerrainGradeEastWestRamp(Rectangle footprint,
                                     float west_height,
                                     float east_height)
{
    const float shoulder = 0.95f;
    for (int32_t column = 0; column < CC_TERRAIN_COLUMNS; ++column) {
        float x = (float)column * CC_TERRAIN_CELL_SIZE;
        if (x < footprint.x || x > footprint.x + footprint.width) continue;
        float progress = TerrainSmooth01((x - footprint.x) /
                                         footprint.width);
        float elevation = west_height +
                          (east_height - west_height) * progress;
        for (int32_t row = 0; row < CC_TERRAIN_ROWS; ++row) {
            float z = (float)row * CC_TERRAIN_CELL_SIZE;
            float side_distance = z < footprint.y ? footprint.y - z :
                z > footprint.y + footprint.height ?
                    z - (footprint.y + footprint.height) : 0.0f;
            if (side_distance >= shoulder) continue;
            float weight = side_distance <= 0.0f ? 1.0f :
                TerrainSmooth01(1.0f - side_distance / shoulder);
            int32_t index = TerrainIndex(column, row);
            street_terrain_height[index] +=
                (elevation - street_terrain_height[index]) * weight;
        }
    }
}

static float TerrainKeepRise(void)
{
    switch (active_place_function) {
        case CC_SETTLEMENT_FARMING: return 1.80f;
        case CC_SETTLEMENT_MINING: return 2.40f;
        case CC_SETTLEMENT_MARKET: return 2.20f;
        case CC_SETTLEMENT_FORTRESS: return 2.80f;
        case CC_SETTLEMENT_CAPITAL: return 2.20f;
        case CC_SETTLEMENT_DUNGEON_TOWN: return 2.60f;
    }
    return 2.20f;
}

static void TerrainGenerate(void)
{
    for (int32_t row = 0; row < CC_TERRAIN_ROWS; ++row) {
        float z = (float)row * CC_TERRAIN_CELL_SIZE;
        for (int32_t column = 0; column < CC_TERRAIN_COLUMNS; ++column) {
            float x = (float)column * CC_TERRAIN_CELL_SIZE;
            int32_t index = TerrainIndex(column, row);
            street_terrain_natural[index] = TerrainNaturalHeight(x, z);
            street_terrain_height[index] = street_terrain_natural[index];
        }
    }
    for (int32_t road = 0;
         road < (int32_t)(sizeof(TERRAIN_ROADS) /
                          sizeof(TERRAIN_ROADS[0])); ++road) {
        TerrainGradeRoad(&TERRAIN_ROADS[road]);
    }
    TerrainGradeActivePlaceRoads();
    TerrainGradeActiveCarriageLanes();

    float arrival_west_height = TerrainSampleGrid(
        street_terrain_height, 83.5f, 34.0f);
    float arrival_east_height = TerrainSampleGrid(
        street_terrain_height, 96.0f, 34.0f);
    float arrival_bridge_height = 0.0f;
    if (active_place_function == CC_SETTLEMENT_FARMING ||
        active_place_function == CC_SETTLEMENT_FORTRESS) {
        arrival_bridge_height = fmaxf(arrival_west_height,
                                      arrival_east_height) + 0.18f;
        TerrainGradeEastWestRamp((Rectangle){83.5f, 31.75f, 12.5f, 4.50f},
                                 arrival_bridge_height,
                                 arrival_bridge_height);
    } else {
        TerrainGradeEastWestRamp((Rectangle){63.8f, 31.75f, 32.2f, 4.50f},
                                 TerrainSampleGrid(street_terrain_height,
                                                   63.8f, 34.0f),
                                 arrival_east_height);
    }


    for (int32_t i = 0; i < ActiveWorldBuildingCount(); ++i) {
        Rectangle footprint = ActiveWorldBuildingAt(i).footprint;
        TerrainGradePad(footprint, TerrainRectangleAverage(footprint), 1.45f);
    }
    for (int32_t i = 0; i < CC_LOCAL_PLACE_LANDMARK_COUNT; ++i) {
        Rectangle footprint =
            PlaceLandmarkFootprint(ActivePlaceLandmarkAt(i));
        TerrainGradePad(footprint, TerrainRectangleAverage(footprint), 1.35f);
    }
    Rectangle keep_pad = ActiveCompoundBounds();
    float keep_approach_height = TerrainSampleGrid(
        street_terrain_height, 78.5f, 54.0f);
    float natural_keep_elevation = TerrainRectangleAverage(keep_pad) + 0.45f;

    float keep_elevation = fminf(
        natural_keep_elevation,
        keep_approach_height + TerrainKeepRise());
    keep_elevation = fmaxf(keep_elevation, keep_approach_height + 0.80f);
    TerrainGradePad(keep_pad, keep_elevation, 4.00f);

    Rectangle plaza_pad = {37.60f, 25.60f, 18.80f, 8.80f};
    float plaza_elevation = TerrainRectangleAverage(plaza_pad);
    TerrainGradePad(plaza_pad, plaza_elevation, 1.80f);
    TerrainGradePad(CARRIAGE_FOOTPRINT,
                    TerrainRectangleAverage(CARRIAGE_FOOTPRINT), 1.20f);
    TerrainGradeActiveCarriageCourts();
    TerrainGradePad(DUNGEON_FOOTPRINT,
                    TerrainRectangleAverage(DUNGEON_FOOTPRINT), 1.35f);


    TerrainGradePad((Rectangle){1.00f, 0.00f, 14.60f, 11.10f},
                    0.0f, 2.20f);


    float west_course_road_height = TerrainSampleGrid(
        street_terrain_height, 8.35f, 26.50f);
    TerrainGradeNorthSouthRamp((Rectangle){7.45f, 10.50f, 1.80f, 16.25f},
                               0.0f, west_course_road_height);
    float east_course_road_height = TerrainSampleGrid(
        street_terrain_height, 16.40f, 26.85f);
    TerrainGradeNorthSouthRamp((Rectangle){14.65f, 10.50f, 3.50f, 16.60f},
                               0.0f, east_course_road_height);


    for (int32_t road = 0;
         road < (int32_t)(sizeof(TERRAIN_ROADS) /
                          sizeof(TERRAIN_ROADS[0])); ++road) {
        TerrainGradeRoad(&TERRAIN_ROADS[road]);
    }
    TerrainGradeActivePlaceRoads();
    TerrainGradeActiveCarriageLanes();
    float artisan_south_height = TerrainSampleGrid(street_terrain_height,
                                                    33.0f, 25.0f);
    float artisan_north_height = TerrainSampleGrid(street_terrain_height,
                                                    33.0f, 30.0f);
    TerrainGradeNorthSouthRamp((Rectangle){29.00f, 25.00f, 13.00f, 5.00f},
                               artisan_south_height,
                               artisan_north_height);
    float commons_south_height = TerrainSampleGrid(street_terrain_height,
                                                    41.5f, 11.6f);
    float commons_north_height = TerrainSampleGrid(street_terrain_height,
                                                    41.5f, 25.0f);
    TerrainGradeNorthSouthRamp((Rectangle){39.40f, 11.60f, 4.20f, 13.40f},
                               commons_south_height,
                               commons_north_height);

    TerrainGradePad(keep_pad, keep_elevation, 4.00f);

    TerrainGradeRoad(&TERRAIN_ROADS[
        (int32_t)(sizeof(TERRAIN_ROADS) / sizeof(TERRAIN_ROADS[0])) - 1]);
    float west_gate_height = TerrainSampleGrid(street_terrain_height,
                                                57.0f, 29.4f);
    TerrainGradeEastWestRamp((Rectangle){54.0f, 26.80f, 11.4f, 5.20f},
                             west_gate_height, west_gate_height);
    float west_gate_north_height = TerrainSampleGrid(
        street_terrain_height, 64.1f, 38.0f);
    TerrainGradeNorthSouthRamp((Rectangle){62.80f, 27.00f, 2.60f, 11.50f},
                               west_gate_height,
                               west_gate_north_height);
    float gate_causeway_height = fmaxf(
        west_gate_north_height, keep_elevation - 0.90f);

    TerrainGradeEastWestRamp((Rectangle){63.80f, 36.20f, 15.20f, 6.00f},
                             west_gate_north_height,
                             gate_causeway_height);
    TerrainGradeEastWestRamp(
        (Rectangle){79.00f, 36.20f, 17.00f, 6.00f},
        gate_causeway_height,
        TerrainSampleGrid(street_terrain_height, 96.0f, 38.0f));
    float crown_road_north_height = TerrainSampleGrid(
        street_terrain_height, 78.5f, 54.0f);
    TerrainGradeNorthSouthGentleRamp(
        (Rectangle){75.80f, 30.60f, 5.20f, 23.40f},
        keep_elevation, crown_road_north_height);
    float eastern_gate_height = TerrainSampleGrid(
        street_terrain_height, 81.0f, 34.0f);
    if (active_place_function == CC_SETTLEMENT_FARMING ||
        active_place_function == CC_SETTLEMENT_FORTRESS) {
        TerrainGradeEastWestRamp((Rectangle){81.0f, 31.75f, 2.5f, 4.50f},
                                 eastern_gate_height,
                                 arrival_bridge_height);
        TerrainGradeEastWestRamp((Rectangle){83.5f, 31.75f, 12.5f, 4.50f},
                                 arrival_bridge_height,
                                 arrival_bridge_height);
    } else {
        TerrainGradeEastWestRamp((Rectangle){81.0f, 31.75f, 15.0f, 4.50f},
                                 eastern_gate_height,
                                 arrival_east_height);
    }
    TerrainGradePad((Rectangle){39.00f, 27.00f, 15.00f, 6.00f},
                    plaza_elevation, 2.50f);
    TerrainGradeActiveCarriageCourts();

    TerrainGradeNorthSouthRamp((Rectangle){7.45f, 10.50f, 1.80f, 16.25f},
                               0.0f, west_course_road_height);
    TerrainGradeNorthSouthRamp((Rectangle){14.65f, 10.50f, 3.50f, 16.60f},
                               0.0f, east_course_road_height);
    street_terrain_ready = true;
}

static void TerrainEnsureReady(void)
{
    if (!street_terrain_ready) TerrainGenerate();
}

void CcLocalTerrainSetSeed(uint32_t seed)
{
    seed = seed == 0U ? UINT32_C(0xc0a71a9e) : seed;
    if (street_terrain_ready && seed == street_terrain_seed) return;
    street_terrain_seed = seed;
    street_terrain_ready = false;
}

void CcLocalBindPlace(const CcSim *sim)
{
    const CcSettlement *place = sim != NULL ?
        CcSimSettlement(sim, sim->player.location_id) : NULL;
    CcSettlementFunction function = place != NULL ? place->function :
                                                    CC_SETTLEMENT_MARKET;
    if (function != active_place_function) {
        street_camera_rig = (FixedCameraRig){0};
    }
    active_place_function = function;
    CcLocalTerrainSetSeed(sim != NULL ?
        CcLocalPlaceTerrainSeed(sim->world_seed, place) :
        UINT32_C(0xc0a71a9e));
}

void CcLocalBindOpenWorld(const CcWorldStream *stream)
{
    active_world_stream = stream;
}

float CcLocalTerrainHeightAt(float x, float z)
{
    if (active_world_stream != NULL) {
        return CcWorldStreamHeightAt(active_world_stream, x, z);
    }
    TerrainEnsureReady();
    return TerrainSampleGrid(street_terrain_height, x, z);
}

Vector3 CcLocalTerrainNormalAt(float x, float z)
{
    const float offset = CC_TERRAIN_CELL_SIZE;
    float left = CcLocalTerrainHeightAt(x - offset, z);
    float right = CcLocalTerrainHeightAt(x + offset, z);
    float near_height = CcLocalTerrainHeightAt(x, z - offset);
    float far_height = CcLocalTerrainHeightAt(x, z + offset);
    Vector3 normal = {left - right, offset * 2.0f,
                      near_height - far_height};
    float length = sqrtf(normal.x * normal.x + normal.y * normal.y +
                         normal.z * normal.z);
    return length > 0.0001f ?
        (Vector3){normal.x / length, normal.y / length, normal.z / length} :
        (Vector3){0.0f, 1.0f, 0.0f};
}

static float PlatformBaseHeight(const NavPlatform *platform)
{
    return CcLocalTerrainHeightAt(platform->x + platform->width * 0.5f,
                                  platform->z + platform->depth * 0.5f);
}

static float PlatformTopHeight(const NavPlatform *platform)
{
    return PlatformBaseHeight(platform) + platform->height;
}

static float TerrainFootprintHeight(Rectangle footprint)
{
    return CcLocalTerrainHeightAt(footprint.x + footprint.width * 0.5f,
                                  footprint.y + footprint.height * 0.5f);
}

static Vector3 TerrainWorldPoint(float x, float z)
{
    return (Vector3){x, CcLocalTerrainHeightAt(x, z), z};
}

static const Vector3 COURSE_WAYPOINTS[] = {
    {9.45f, 0.0f, 1.20f}, {10.50f, 0.0f, 1.20f},
    {11.22f, 0.0f, 2.95f}, {12.27f, 0.0f, 1.97f},
    {13.00f, 0.0f, 1.45f}, {13.57f, 0.0f, 2.95f},
    {14.35f, 0.0f, 4.15f}, {11.55f, 0.0f, 3.86f},
    {9.10f, 0.0f, 3.86f}, {10.27f, 0.0f, 5.62f},
    {10.65f, 0.0f, 6.75f}, {11.66f, 0.0f, 6.11f},
    {12.05f, 0.0f, 7.45f}, {13.10f, 0.0f, 6.95f},
    {14.15f, 0.0f, 7.75f}, {11.15f, 0.0f, 8.30f},
    {12.55f, 0.0f, 9.65f}, {13.57f, 0.0f, 9.12f},
    {14.45f, 0.0f, 9.65f}, {9.40f, 0.0f, 9.65f}
};

static Camera3D LocalCamera(bool interior, Vector3 focus);
static Camera3D ExteriorCameraAt(Vector3 target, float fovy);
static Camera3D ExteriorCameraComposed(Vector3 target, Vector3 offset,
                                       float fovy);
static Camera3D PerspectiveCameraComposed(Vector3 target, Vector3 offset,
                                          float fovy);
static Camera3D SnapCameraToArtPixels(Camera3D camera, int32_t art_height);
Camera3D CcLocalStreetCameraInternal(const CcLocalAgent *agent, float clock,
                                     bool advance, int32_t art_height);
static Camera3D RoadCamera(Vector3 focus, bool travelling, float clock,
                           bool advance, int32_t art_height);
Camera3D CcLocalCombatCameraInternal(Camera3D base,
                                     const CcLocalAgent *player,
                                     const CcLocalCourse *course,
                                     float clock, bool advance,
                                     int32_t art_height);
static int32_t StreetNavigationRoomFor(Vector3 focus,
                                       int32_t current_room);
static float WrapAngle(float angle);
static float SmoothStep01(float amount);
static Color ShadeColor(Color color, float scale);
static void DrawCharacterEllipsoid(Vector3 center, Vector3 radius,
                                   Color color);
static void UpdateHeroCape(CcLocalAgent *agent, float delta_time);
static int32_t RoadObstacleCount(void);
static Rectangle RoadObstacleAt(int32_t index);
static float CameraPositionRectangleClutter(Vector3 position,
                                            Rectangle footprint,
                                            float clearance);
static bool RoomDetailPointVisible(float x, float z, Vector3 focus);

static bool CourseWaterContains(CcLocalSceneKind scene, float x, float z)
{
    if (active_world_stream != NULL) return false;
    if (scene != CC_LOCAL_SCENE_STREET) return false;
    return x >= COURSE_POOL.x && x <= COURSE_POOL.x + COURSE_POOL.width &&
           z >= COURSE_POOL.y && z <= COURSE_POOL.y + COURSE_POOL.height;
}

static float SurfaceHeightAt(CcLocalSceneKind scene, float x, float z)
{
    if (active_world_stream != NULL && scene != CC_LOCAL_SCENE_MARKET) {
        return CcLocalTerrainHeightAt(x, z);
    }
    if (scene == CC_LOCAL_SCENE_ROAD) {
        if (!RoadUsesAuthoredCheckpoint()) return 0.0f;
        return CcLocalRoadCheckpointSurfaceYInternal(x, z);
    }
    if (scene != CC_LOCAL_SCENE_STREET) return 0.0f;
    float height = CcLocalTerrainHeightAt(x, z);
    for (int32_t i = 0; i < StreetPhysicsPlatformCount(); ++i) {
        const NavPlatform *platform = StreetPhysicsPlatformAt(i);
        if (platform == NULL) continue;
        float top = PlatformTopHeight(platform);
        if (x >= platform->x && x <= platform->x + platform->width &&
            z >= platform->z && z <= platform->z + platform->depth &&
            top > height) {
            height = top;
        }
    }
    return height;
}

static float BodySurfaceHeightAt(CcLocalSceneKind scene, float x, float z)
{
    if (active_world_stream != NULL && scene != CC_LOCAL_SCENE_MARKET) {
        return CcLocalTerrainHeightAt(x, z);
    }
    if (scene == CC_LOCAL_SCENE_ROAD) {
        return SurfaceHeightAt(scene, x, z);
    }
    if (scene != CC_LOCAL_SCENE_STREET) return 0.0f;
    float height = CcLocalTerrainHeightAt(x, z);
    for (int32_t i = 0; i < StreetPhysicsPlatformCount(); ++i) {
        const NavPlatform *platform = StreetPhysicsPlatformAt(i);
        if (platform == NULL) continue;
        float top = PlatformTopHeight(platform);
        float edge_release = platform->style == 0 ? 0.0f : 0.10f;
        if (x >= platform->x + edge_release &&
            x <= platform->x + platform->width - edge_release &&
            z >= platform->z + edge_release &&
            z <= platform->z + platform->depth - edge_release &&
            top > height) {
            height = top;
        }
    }
    return height;
}

static Vector3 SurfaceNormalAt(CcLocalSceneKind scene, float x, float z)
{
    if (active_world_stream != NULL && scene != CC_LOCAL_SCENE_MARKET) {
        return CcLocalTerrainNormalAt(x, z);
    }
    if (scene != CC_LOCAL_SCENE_STREET) return (Vector3){0.0f, 1.0f, 0.0f};
    float terrain = CcLocalTerrainHeightAt(x, z);
    if (SurfaceHeightAt(scene, x, z) > terrain + 0.08f) {
        return (Vector3){0.0f, 1.0f, 0.0f};
    }
    return CcLocalTerrainNormalAt(x, z);
}

static float FootprintDistanceSquared(float x, float z, Rectangle footprint)
{
    float closest_x = fmaxf(footprint.x,
                            fminf(x, footprint.x + footprint.width));
    float closest_z = fmaxf(footprint.y,
                            fminf(z, footprint.y + footprint.height));
    float delta_x = x - closest_x;
    float delta_z = z - closest_z;
    return delta_x * delta_x + delta_z * delta_z;
}

static bool CircleTouchesFootprint(float x, float z, float radius,
                                   Rectangle footprint)
{
    return FootprintDistanceSquared(x, z, footprint) < radius * radius;
}

/* The town renderer uses this same profile origin, scale and Y rotation. */
#define CC_LOCAL_COLLISION_BOX_CAPACITY (72 + CC_MAX_SETTLEMENTS * \
    (CC_LOCAL_PLACE_BUILDING_CAPACITY + CC_LOCAL_PLACE_COMPOUND_CAPACITY + \
     CC_LOCAL_PLACE_LANDMARK_COUNT))
typedef CcCollisionBox LocalCollisionBox;

static void AddTownCollisionBox(const CcWorldManifest *manifest,
                                const CcWorldSettlementPlacement *place,
                                Rectangle footprint, float height,
                                LocalCollisionBox *boxes, int32_t *count)
{
    if (height <= 0.0f || footprint.width <= 0.0f || footprint.height <= 0.0f ||
        *count >= CC_LOCAL_COLLISION_BOX_CAPACITY) return;
    CcWorldPoint center = CcWorldSettlementLocalPoint(
        manifest, place->settlement_id, footprint.x + footprint.width * 0.5f,
        footprint.y + footprint.height * 0.5f);
    float half_x = footprint.width * place->profile_scale * 0.5f;
    float half_z = footprint.height * place->profile_scale * 0.5f;
    float base = place->plateau_height + 0.03f;
    boxes[(*count)++] = (LocalCollisionBox){
        .minimum = {center.x - half_x, base, center.z - half_z},
        .maximum = {center.x + half_x, base + height * place->profile_scale,
                    center.z + half_z},
        .yaw = place->entrance_heading_yaw - 0.5f * PI,
    };
}

static int32_t GatherTownCollisionBoxes(const CcWorldManifest *manifest,
                                        LocalCollisionBox *boxes)
{
    int32_t count = 0;
    if (manifest == NULL) return count;
    for (int32_t town = 0; town < manifest->settlement_count; ++town) {
        const CcWorldSettlementPlacement *place = &manifest->settlements[town];
        if (place->profile_scale <= 0.0001f) continue;
        const CcLocalPlaceProfile *profile =
            CcLocalPlaceProfileForFunction(place->function);
        if (profile == NULL) continue;
        for (int32_t i = 0; i < profile->building_count; ++i) {
            const CcLocalPlaceBuilding *building = &profile->building[i];
            AddTownCollisionBox(manifest, place,
                (Rectangle){building->x, building->z, building->width,
                             building->depth}, building->height, boxes, &count);
        }
        for (int32_t i = 0; i < profile->compound_structure_count; ++i) {
            const CcLocalPlaceCompoundStructure *structure =
                &profile->compound_structure[i];
            AddTownCollisionBox(manifest, place,
                (Rectangle){structure->x, structure->z, structure->width,
                             structure->depth}, structure->height, boxes, &count);
        }
        for (int32_t i = 0; i < CC_LOCAL_PLACE_LANDMARK_COUNT; ++i) {
            const CcLocalPlaceLandmark *landmark = &profile->landmark[i];
            AddTownCollisionBox(manifest, place,
                (Rectangle){landmark->x, landmark->z, landmark->width,
                             landmark->depth}, landmark->height, boxes, &count);
        }
    }
    return count;
}

static bool OpenWorldTownBodyBlocked(const CcWorldManifest *manifest,
                                     float x, float z, float radius)
{
    LocalCollisionBox boxes[CC_LOCAL_COLLISION_BOX_CAPACITY];
    int32_t count = GatherTownCollisionBoxes(manifest, boxes);
    for (int32_t i = 0; i < count; ++i) {
        CcLimbVec3 point = {x, (boxes[i].minimum.y + boxes[i].maximum.y) * 0.5f, z};
        CcLimbVec3 normal;
        if (CcCollisionPushSphere(&boxes[i], radius, &point, &normal)) return true;
    }
    return false;
}

static bool StaticBodyBlocked(CcLocalSceneKind scene, float x, float z,
                              float radius)
{
    if (active_world_stream != NULL && scene != CC_LOCAL_SCENE_MARKET) {
        const CcWorldManifest *manifest = &active_world_stream->manifest;
        return x < manifest->minimum_x + radius ||
               x > manifest->maximum_x - radius ||
               z < manifest->minimum_z + radius ||
               z > manifest->maximum_z - radius ||
               OpenWorldTownBodyBlocked(manifest, x, z, radius);
    }
    bool market_interior = scene == CC_LOCAL_SCENE_MARKET;
    float maximum_x = market_interior ? 8.72f : CC_LOCAL_WORLD_WIDTH - 0.28f;
    float maximum_z = market_interior ? 6.72f : CC_LOCAL_WORLD_DEPTH - 0.28f;
    if (x < 0.28f + radius || x > maximum_x - radius ||
        z < 0.28f + radius || z > maximum_z - radius) return true;
    if (market_interior) {
        if (CircleTouchesFootprint(x, z, radius, MARKET_COUNTER_FOOTPRINT) ||
            CircleTouchesFootprint(x, z, radius, MARKET_SHELF_FOOTPRINT)) {
            return true;
        }
        for (int32_t i = 0; i < (int32_t)(sizeof(MARKET_PEOPLE) /
                                          sizeof(MARKET_PEOPLE[0])); ++i) {
            float dx = x - MARKET_PEOPLE[i].x;
            float dz = z - MARKET_PEOPLE[i].y;
            float clearance = radius + PERSON_COLLISION_RADIUS;
            if (dx * dx + dz * dz < clearance * clearance) return true;
        }
        return false;
    }
    if (scene == CC_LOCAL_SCENE_ROAD) {
        for (int32_t i = 0; i < RoadObstacleCount(); ++i) {
            if (CircleTouchesFootprint(x, z, radius, RoadObstacleAt(i))) {
                return true;
            }
        }
        return false;
    }
    for (int32_t i = 0; i < ActiveWorldBuildingCount(); ++i) {
        WorldBuilding building = ActiveWorldBuildingAt(i);

        if (CircleTouchesFootprint(x, z, radius,
                                   building.footprint)) return true;
    }
    for (int32_t i = 0; i < ActiveCompoundStructureCount(); ++i) {
        WorldStructure structure = ActiveCompoundStructureAt(i);
        if (CircleTouchesFootprint(x, z, radius,
                                   structure.footprint)) return true;
    }
    for (int32_t i = 0; i < CC_LOCAL_PLACE_LANDMARK_COUNT; ++i) {
        if (CircleTouchesFootprint(
                x, z, radius,
                PlaceLandmarkFootprint(ActivePlaceLandmarkAt(i)))) {
            return true;
        }
    }
    if ((active_opening_step == CC_LOCAL_OPENING_COMPLETE &&
         CircleTouchesFootprint(x, z, radius, CARRIAGE_FOOTPRINT)) ||
        CircleTouchesFootprint(x, z, radius, DUNGEON_FOOTPRINT)) return true;
    for (int32_t i = 0; i < (int32_t)(sizeof(ROOM_ART_OBSTACLES) /
                                      sizeof(ROOM_ART_OBSTACLES[0])); ++i) {
        if (CircleTouchesFootprint(x, z, radius, ROOM_ART_OBSTACLES[i])) {
            return true;
        }
    }
    for (int32_t i = 0; i < (int32_t)(sizeof(STREET_PEOPLE) /
                                      sizeof(STREET_PEOPLE[0])); ++i) {
        float dx = x - STREET_PEOPLE[i].x;
        float dz = z - STREET_PEOPLE[i].y;
        float clearance = radius + PERSON_COLLISION_RADIUS;
        if (dx * dx + dz * dz < clearance * clearance) return true;
    }
    return false;
}

#define STREET_PATH_CELL_SIZE 0.50f
#define STREET_PATH_COLUMNS 192
#define STREET_PATH_ROWS 144
#define STREET_PATH_NODE_COUNT (STREET_PATH_COLUMNS * STREET_PATH_ROWS)
#define STREET_CLICK_MAX_PROJECTION 1.10f
#define STREET_SCRIPTED_MAX_PROJECTION 6.25f

typedef struct StreetPathSearch {
    float distance[STREET_PATH_NODE_COUNT];
    int32_t parent[STREET_PATH_NODE_COUNT];
    int32_t heap[STREET_PATH_NODE_COUNT];
    int32_t heap_position[STREET_PATH_NODE_COUNT];
    uint8_t closed[STREET_PATH_NODE_COUNT];
    int32_t heap_count;
} StreetPathSearch;

static StreetPathSearch street_path_search;

static bool StreetPathBodyBlocked(float x, float z, float radius)
{
    if (StaticBodyBlocked(CC_LOCAL_SCENE_STREET, x, z, radius)) return true;
    for (int32_t index = 0; index < StreetPhysicsPlatformCount(); ++index) {
        const NavPlatform *platform = StreetPhysicsPlatformAt(index);
        if (platform == NULL) continue;
        Rectangle footprint = {platform->x, platform->z,
                               platform->width, platform->depth};

        float platform_radius = fmaxf(0.0f, radius - 0.026f);
        if (CircleTouchesFootprint(x, z, platform_radius, footprint)) {
            return true;
        }
    }
    return false;
}

static Vector2 StreetPathNodePoint(int32_t node)
{
    int32_t column = node % STREET_PATH_COLUMNS;
    int32_t row = node / STREET_PATH_COLUMNS;
    return (Vector2){((float)column + 0.5f) * STREET_PATH_CELL_SIZE,
                     ((float)row + 0.5f) * STREET_PATH_CELL_SIZE};
}

static bool StreetSegmentClear(Vector2 from, Vector2 to, float radius, float minimum_normal)
{
    float x = to.x - from.x;
    float z = to.y - from.y;
    float length = sqrtf(x * x + z * z);
    int32_t steps = (int32_t)ceilf(length / 0.12f);
    if (steps < 1) steps = 1;
    Vector2 previous = from;
    float previous_height = CcLocalTerrainHeightAt(from.x, from.y);
    for (int32_t step = 1; step <= steps; ++step) {
        float amount = (float)step / (float)steps;
        float sample_x = from.x + x * amount;
        float sample_z = from.y + z * amount;
        if (StreetPathBodyBlocked(sample_x, sample_z,
                                  radius + 0.025f)) return false;
        float sample_height = CcLocalTerrainHeightAt(sample_x, sample_z);
        Vector3 sample_normal = CcLocalTerrainNormalAt(sample_x, sample_z);
        float step_x = sample_x - previous.x;
        float step_z = sample_z - previous.y;
        float step_length = sqrtf(step_x * step_x + step_z * step_z);
        float height_change = sample_height - previous_height;

        if (height_change > step_length * 1.25f + 0.025f ||
            height_change < -step_length * 1.65f - 0.060f ||
            sample_normal.y < minimum_normal) {
            return false;
        }
        previous = (Vector2){sample_x, sample_z};
        previous_height = sample_height;
    }
    return true;
}

static int32_t StreetPathNearestNode(Vector2 point, float radius,
                                     bool require_point_connection, float minimum_normal)
{
    int32_t center_column = (int32_t)floorf(point.x / STREET_PATH_CELL_SIZE);
    int32_t center_row = (int32_t)floorf(point.y / STREET_PATH_CELL_SIZE);
    for (int32_t ring = 0; ring <= 12; ++ring) {
        int32_t best = -1;
        float best_distance = FLT_MAX;
        for (int32_t row = center_row - ring; row <= center_row + ring;
             ++row) {
            if (row < 0 || row >= STREET_PATH_ROWS) continue;
            for (int32_t column = center_column - ring;
                 column <= center_column + ring; ++column) {
                if (column < 0 || column >= STREET_PATH_COLUMNS) continue;
                if (ring > 0 && abs(column - center_column) != ring &&
                    abs(row - center_row) != ring) continue;
                int32_t node = row * STREET_PATH_COLUMNS + column;
                Vector2 candidate = StreetPathNodePoint(node);
                if (StreetPathBodyBlocked(candidate.x, candidate.y,
                                          radius + 0.025f) ||
                    (require_point_connection &&
                     !StreetSegmentClear(point, candidate, radius, minimum_normal))) {
                    continue;
                }
                float x = candidate.x - point.x;
                float z = candidate.y - point.y;
                float distance = x * x + z * z;
                if (distance >= best_distance) continue;
                best = node;
                best_distance = distance;
            }
        }
        if (best >= 0) return best;
    }
    return -1;
}

static float StreetPathHeuristic(int32_t node, int32_t goal)
{
    Vector2 point = StreetPathNodePoint(node);
    Vector2 target = StreetPathNodePoint(goal);
    float x = target.x - point.x;
    float z = target.y - point.y;
    return sqrtf(x * x + z * z);
}

static bool StreetPathHeapLess(const StreetPathSearch *search,
                               int32_t left, int32_t right, int32_t goal)
{
    float left_score = search->distance[left] +
                       StreetPathHeuristic(left, goal);
    float right_score = search->distance[right] +
                        StreetPathHeuristic(right, goal);
    if (fabsf(left_score - right_score) > 0.0001f) {
        return left_score < right_score;
    }
    return search->distance[left] < search->distance[right];
}

static void StreetPathHeapSwap(StreetPathSearch *search,
                               int32_t left, int32_t right)
{
    int32_t node = search->heap[left];
    search->heap[left] = search->heap[right];
    search->heap[right] = node;
    search->heap_position[search->heap[left]] = left;
    search->heap_position[search->heap[right]] = right;
}

static void StreetPathHeapSiftUp(StreetPathSearch *search,
                                 int32_t position, int32_t goal)
{
    if (position <= 0 || position >= STREET_PATH_NODE_COUNT) return;
    while (position > 0) {
        int32_t parent = (position - 1) / 2;
        if (!StreetPathHeapLess(search, search->heap[position],
                                search->heap[parent], goal)) break;
        StreetPathHeapSwap(search, position, parent);
        position = parent;
    }
}

static void StreetPathHeapPushOrDecrease(StreetPathSearch *search,
                                         int32_t node, int32_t goal)
{
    int32_t position = search->heap_position[node];
    if (position >= 0) {
        StreetPathHeapSiftUp(search, position, goal);
        return;
    }
    if (search->heap_count >= STREET_PATH_NODE_COUNT) return;
    position = search->heap_count++;
    search->heap[position] = node;
    search->heap_position[node] = position;
    StreetPathHeapSiftUp(search, position, goal);
}

static int32_t StreetPathHeapPop(StreetPathSearch *search, int32_t goal)
{
    if (search->heap_count <= 0) return -1;
    int32_t result = search->heap[0];
    search->heap_position[result] = -1;
    search->heap_count -= 1;
    if (search->heap_count <= 0) return result;
    search->heap[0] = search->heap[search->heap_count];
    search->heap_position[search->heap[0]] = 0;
    int32_t position = 0;
    for (;;) {
        int32_t left = position * 2 + 1;
        if (left >= search->heap_count) break;
        int32_t right = left + 1;
        int32_t smallest = left;
        if (right < search->heap_count &&
            StreetPathHeapLess(search, search->heap[right],
                               search->heap[left], goal)) {
            smallest = right;
        }
        if (!StreetPathHeapLess(search, search->heap[smallest],
                                search->heap[position], goal)) break;
        StreetPathHeapSwap(search, position, smallest);
        position = smallest;
    }
    return result;
}

static int32_t FindStreetPath(Vector2 from, Vector2 to, float radius,
                              Vector2 *points, int32_t capacity,
                              float maximum_projection, float minimum_normal,
                              bool *target_projected_out)
{
    if (target_projected_out != NULL) *target_projected_out = false;
    if (points == NULL || capacity <= 0) return 0;
    bool target_blocked = StreetPathBodyBlocked(to.x, to.y, radius);
    bool target_projected = target_blocked;
    if (!target_blocked && StreetSegmentClear(from, to, radius, minimum_normal)) {
        points[0] = to;
        return 1;
    }

    int32_t start = StreetPathNearestNode(from, radius, true, minimum_normal);
    int32_t goal = StreetPathNearestNode(to, radius, !target_blocked, minimum_normal);
    if (start < 0 || goal < 0) return 0;
    if (target_blocked) {
        Vector2 projected = StreetPathNodePoint(goal);
        float projected_x = projected.x - to.x;
        float projected_z = projected.y - to.y;
        if (projected_x * projected_x + projected_z * projected_z >
            maximum_projection * maximum_projection) {
            return 0;
        }
    }
    StreetPathSearch *search = &street_path_search;
    search->heap_count = 0;
    for (int32_t node = 0; node < STREET_PATH_NODE_COUNT; ++node) {
        search->distance[node] = FLT_MAX;
        search->parent[node] = -1;
        search->heap_position[node] = -1;
        search->closed[node] = 0;
    }
    search->distance[start] = 0.0f;
    StreetPathHeapPushOrDecrease(search, start, goal);

    static const int32_t neighbor_column[] = {-1, 0, 1, -1, 1, -1, 0, 1};
    static const int32_t neighbor_row[] = {-1, -1, -1, 0, 0, 1, 1, 1};
    while (search->heap_count > 0) {
        int32_t current = StreetPathHeapPop(search, goal);
        if (current < 0) break;
        if (search->closed[current]) continue;
        search->closed[current] = 1;
        if (current == goal) break;
        int32_t current_column = current % STREET_PATH_COLUMNS;
        int32_t current_row = current / STREET_PATH_COLUMNS;
        for (int32_t neighbor = 0; neighbor < 8; ++neighbor) {
            int32_t column = current_column + neighbor_column[neighbor];
            int32_t row = current_row + neighbor_row[neighbor];
            if (column < 0 || column >= STREET_PATH_COLUMNS ||
                row < 0 || row >= STREET_PATH_ROWS) continue;
            int32_t next = row * STREET_PATH_COLUMNS + column;
            if (search->closed[next]) continue;
            Vector2 current_point = StreetPathNodePoint(current);
            Vector2 next_point = StreetPathNodePoint(next);
            if (StreetPathBodyBlocked(next_point.x, next_point.y,
                                      radius + 0.025f) ||
                !StreetSegmentClear(current_point, next_point, radius, minimum_normal)) {
                continue;
            }
            bool diagonal = neighbor_column[neighbor] != 0 &&
                            neighbor_row[neighbor] != 0;
            if (diagonal) {
                int32_t horizontal = current_row * STREET_PATH_COLUMNS + column;
                int32_t vertical = row * STREET_PATH_COLUMNS + current_column;
                Vector2 horizontal_point = StreetPathNodePoint(horizontal);
                Vector2 vertical_point = StreetPathNodePoint(vertical);
                if (StreetPathBodyBlocked(horizontal_point.x,
                                          horizontal_point.y,
                                          radius + 0.025f) ||
                    StreetPathBodyBlocked(vertical_point.x,
                                          vertical_point.y,
                                          radius + 0.025f)) continue;
            }
            float step = diagonal ? STREET_PATH_CELL_SIZE * 1.41421356f :
                                    STREET_PATH_CELL_SIZE;
            float current_height = CcLocalTerrainHeightAt(
                current_point.x, current_point.y);
            float next_height = CcLocalTerrainHeightAt(
                next_point.x, next_point.y);
            Vector3 next_normal = CcLocalTerrainNormalAt(
                next_point.x, next_point.y);
            float traversal_cost = CcRobotTraversabilityCost(
                step, next_height - current_height, next_normal.y);
            float distance = search->distance[current] + traversal_cost;
            if (distance + 0.0001f >= search->distance[next]) continue;
            search->distance[next] = distance;
            search->parent[next] = current;
            StreetPathHeapPushOrDecrease(search, next, goal);
        }
    }
    if (start != goal && search->parent[goal] < 0) {

        int32_t closest = -1;
        float closest_distance = maximum_projection * maximum_projection;
        for (int32_t node = 0; node < STREET_PATH_NODE_COUNT; ++node) {
            if (!search->closed[node]) continue;
            Vector2 candidate = StreetPathNodePoint(node);
            float x = candidate.x - to.x;
            float z = candidate.y - to.y;
            float distance = x * x + z * z;
            if (distance >= closest_distance) continue;
            closest = node;
            closest_distance = distance;
        }
        if (closest < 0) return 0;
        goal = closest;
        target_projected = true;
    }

    int32_t ordered_count = 0;
    for (int32_t node = goal; node >= 0 && ordered_count < STREET_PATH_NODE_COUNT;
         node = search->parent[node]) {
        search->heap[ordered_count++] = node;
        if (node == start) break;
    }
    if (ordered_count <= 0 || search->heap[ordered_count - 1] != start) {
        return 0;
    }
    for (int32_t left = 0, right = ordered_count - 1; left < right;
         ++left, --right) {
        int32_t node = search->heap[left];
        search->heap[left] = search->heap[right];
        search->heap[right] = node;
    }

    int32_t point_count = 0;
    Vector2 current_point = from;
    int32_t cursor = 0;
    Vector2 start_point = StreetPathNodePoint(search->heap[0]);
    float start_x = start_point.x - current_point.x;
    float start_z = start_point.y - current_point.y;
    if (start_x * start_x + start_z * start_z > 0.08f * 0.08f) {
        if (point_count >= capacity) return 0;
        points[point_count++] = start_point;
        current_point = start_point;
    }
    Vector2 resolved_target = target_projected ?
        StreetPathNodePoint(goal) : to;
    if (target_projected_out != NULL) {
        *target_projected_out = target_projected;
    }
    while (cursor < ordered_count - 1) {
        if (StreetSegmentClear(current_point, resolved_target, radius, minimum_normal)) {
            if (point_count >= capacity) return 0;
            points[point_count++] = resolved_target;
            return point_count;
        }
        int32_t chosen = cursor + 1;
        for (int32_t candidate = ordered_count - 1; candidate > cursor;
             --candidate) {
            Vector2 candidate_point = StreetPathNodePoint(
                search->heap[candidate]);
            if (!StreetSegmentClear(current_point, candidate_point, radius, minimum_normal)) {
                continue;
            }
            chosen = candidate;
            break;
        }
        Vector2 chosen_point = StreetPathNodePoint(search->heap[chosen]);
        if (point_count >= capacity) return 0;
        points[point_count++] = chosen_point;
        current_point = chosen_point;
        cursor = chosen;
    }
    if (!StreetSegmentClear(current_point, resolved_target, radius, minimum_normal) ||
        point_count >= capacity) return 0;
    points[point_count++] = resolved_target;
    return point_count;
}

typedef struct LocalProbeContext {
    CcLocalSceneKind scene;
} LocalProbeContext;

static void AddLocalCollisionBox(LocalCollisionBox *boxes, int32_t *count,
                                 Rectangle footprint, float base,
                                 float height)
{
    if (*count >= CC_LOCAL_COLLISION_BOX_CAPACITY || height <= 0.0f) return;
    boxes[*count] = (LocalCollisionBox){
        .minimum = {footprint.x, base, footprint.y},
        .maximum = {footprint.x + footprint.width, base + height,
                    footprint.y + footprint.height},
    };
    *count += 1;
}

static int32_t GatherLocalCollisionBoxes(const LocalProbeContext *context,
                                         LocalCollisionBox *boxes)
{
    CcLocalSceneKind scene = context != NULL ? context->scene :
                                               CC_LOCAL_SCENE_STREET;
    if (active_world_stream != NULL && scene != CC_LOCAL_SCENE_MARKET) {
        return GatherTownCollisionBoxes(&active_world_stream->manifest, boxes);
    }
    int32_t count = 0;
    if (scene == CC_LOCAL_SCENE_MARKET) {
        AddLocalCollisionBox(boxes, &count, MARKET_COUNTER_FOOTPRINT,
                             0.0f, 1.12f);
        AddLocalCollisionBox(boxes, &count, MARKET_SHELF_FOOTPRINT,
                             0.0f, 2.15f);
        return count;
    }
    if (scene == CC_LOCAL_SCENE_ROAD) {
        for (int32_t obstacle = 0; obstacle < RoadObstacleCount(); ++obstacle) {
            AddLocalCollisionBox(boxes, &count, RoadObstacleAt(obstacle),
                                 0.0f, 2.35f);
        }
        return count;
    }

    for (int32_t platform_index = 0;
         platform_index < StreetPhysicsPlatformCount(); ++platform_index) {
        const NavPlatform *platform = StreetPhysicsPlatformAt(platform_index);
        if (platform == NULL) continue;
        Rectangle footprint = {platform->x, platform->z,
                               platform->width, platform->depth};
        AddLocalCollisionBox(boxes, &count, footprint,
                             PlatformBaseHeight(platform), platform->height);
    }

    for (int32_t building = 0;
         building < ActiveWorldBuildingCount(); ++building) {
        WorldBuilding world = ActiveWorldBuildingAt(building);
        AddLocalCollisionBox(boxes, &count, world.footprint,
                             TerrainFootprintHeight(world.footprint),
                             world.height);
    }
    for (int32_t structure = 0;
         structure < ActiveCompoundStructureCount(); ++structure) {
        WorldStructure world = ActiveCompoundStructureAt(structure);
        AddLocalCollisionBox(boxes, &count, world.footprint,
                             TerrainFootprintHeight(world.footprint),
                             world.height);
    }
    for (int32_t landmark = 0;
         landmark < CC_LOCAL_PLACE_LANDMARK_COUNT; ++landmark) {
        const CcLocalPlaceLandmark *place_landmark =
            ActivePlaceLandmarkAt(landmark);
        Rectangle footprint = PlaceLandmarkFootprint(place_landmark);
        AddLocalCollisionBox(boxes, &count, footprint,
                             TerrainFootprintHeight(footprint),
                             place_landmark != NULL ?
                                 place_landmark->height : 0.0f);
    }
    if (active_opening_step == CC_LOCAL_OPENING_COMPLETE) {
        AddLocalCollisionBox(boxes, &count, CARRIAGE_FOOTPRINT,
                             TerrainFootprintHeight(CARRIAGE_FOOTPRINT),
                             2.65f);
    }
    AddLocalCollisionBox(boxes, &count, DUNGEON_FOOTPRINT,
                         TerrainFootprintHeight(DUNGEON_FOOTPRINT), 3.10f);
    for (int32_t obstacle = 0;
         obstacle < (int32_t)(sizeof(ROOM_ART_OBSTACLES) /
                              sizeof(ROOM_ART_OBSTACLES[0])); ++obstacle) {
        Rectangle footprint = ROOM_ART_OBSTACLES[obstacle];
        AddLocalCollisionBox(boxes, &count, footprint,
                             TerrainFootprintHeight(footprint), 1.35f);
    }
    return count;
}

static bool ProbeLocalCollision(void *raw_context,
                                CcBiomechVec3 previous_position,
                                CcBiomechVec3 proposed_position, float radius,
                                CcBiomechVec3 *corrected_position,
                                CcBiomechVec3 *surface_normal)
{
    const LocalProbeContext *context = raw_context;
    CcLocalSceneKind scene = context != NULL ? context->scene :
                                               CC_LOCAL_SCENE_STREET;
    Vector3 previous = {previous_position.x, previous_position.y,
                        previous_position.z};
    Vector3 proposed = {proposed_position.x, proposed_position.y,
                        proposed_position.z};
    LocalCollisionBox boxes[CC_LOCAL_COLLISION_BOX_CAPACITY];
    int32_t box_count = GatherLocalCollisionBoxes(context, boxes);
    CcCollisionMove movement = CcCollisionMoveSphere(boxes, box_count,
        (CcLimbVec3){previous.x, previous.y, previous.z},
        (CcLimbVec3){proposed.x - previous.x, proposed.y - previous.y,
                     proposed.z - previous.z}, radius);
    proposed = (Vector3){movement.position.x, movement.position.y, movement.position.z};
    bool collided = movement.collided;
    Vector3 normal = {movement.normal.x, movement.normal.y, movement.normal.z};

    float ground = BodySurfaceHeightAt(scene, proposed.x, proposed.z);
    if (proposed.y - radius < ground) {
        proposed.y = ground + radius;
        normal = SurfaceNormalAt(scene, proposed.x, proposed.z);
        collided = true;
    }
    if (!collided) return false;
    *corrected_position = (CcBiomechVec3){proposed.x, proposed.y, proposed.z};
    *surface_normal = (CcBiomechVec3){normal.x, normal.y, normal.z};
    return true;
}

bool CcLocalProbePhysicsSphereInternal(
    CcLocalSceneKind scene, Vector3 previous, Vector3 proposed, float radius,
    Vector3 *corrected, Vector3 *normal)
{
    if (corrected == NULL || normal == NULL || radius <= 0.0f) return false;
    LocalProbeContext context = {.scene = scene};
    CcBiomechVec3 resolved = {proposed.x, proposed.y, proposed.z};
    CcBiomechVec3 contact_normal = {0};
    bool collided = ProbeLocalCollision(
        &context,
        (CcBiomechVec3){previous.x, previous.y, previous.z},
        (CcBiomechVec3){proposed.x, proposed.y, proposed.z}, radius,
        &resolved, &contact_normal);
    *corrected = (Vector3){resolved.x, resolved.y, resolved.z};
    *normal = (Vector3){contact_normal.x, contact_normal.y,
                        contact_normal.z};
    return collided;
}

static int32_t LocalAgentPointSpace(const CcLocalAgent *agent,
                                    CcRobotCollisionPoint *points)
{
    if (agent == NULL || points == NULL) return 0;

    if (agent->morphology == CC_MORPHOLOGY_BIPED) return 0;
    return CcRobotLimbPointSpace(
        &agent->limb_rig, 0.085f, points, CC_ROBOT_POINT_CAPACITY);
}

bool CcLocalAgentPointSpaceBlockedInternal(const CcLocalAgent *agent,
                                            Vector3 proposed)
{
    if (agent == NULL) return false;
    CcRobotCollisionPoint points[CC_ROBOT_POINT_CAPACITY];
    int32_t point_count = LocalAgentPointSpace(agent, points);
    Vector3 movement = Vector3Subtract(proposed, agent->position);
    LocalProbeContext context = {.scene = agent->scene};
    for (int32_t point = 0; point < point_count; ++point) {
        Vector3 before = {points[point].center.x, points[point].center.y,
                          points[point].center.z};
        Vector3 after = Vector3Add(before, movement);
        CcBiomechVec3 corrected = {after.x, after.y, after.z};
        CcBiomechVec3 normal = {0};
        if (!ProbeLocalCollision(
                &context,
                (CcBiomechVec3){before.x, before.y, before.z},
                (CcBiomechVec3){after.x, after.y, after.z},
                points[point].radius, &corrected, &normal)) {
            continue;
        }
        float correction_x = corrected.x - after.x;
        float correction_z = corrected.z - after.z;
        float proposed_correction = correction_x * correction_x +
                                    correction_z * correction_z;
        if (proposed_correction <= 0.0001f * 0.0001f) continue;

        CcBiomechVec3 corrected_before = {before.x, before.y, before.z};
        CcBiomechVec3 before_normal = {0};
        (void)ProbeLocalCollision(
            &context,
            (CcBiomechVec3){before.x, before.y, before.z},
            (CcBiomechVec3){before.x, before.y, before.z},
            points[point].radius, &corrected_before, &before_normal);
        float before_x = corrected_before.x - before.x;
        float before_z = corrected_before.z - before.z;
        float before_correction = before_x * before_x + before_z * before_z;
        if (before_correction > proposed_correction + 0.000001f) continue;
        return true;
    }
    return false;
}

static bool ResolveLocalAgentCapsuleMove(CcLocalSceneKind scene,
                                         Vector3 previous, Vector3 proposed,
                                         float radius,
                                         float passable_support_height,
                                         Vector3 *resolved,
                                         Vector3 *contact_normal)
{
    if (resolved == NULL || contact_normal == NULL) return false;
    LocalProbeContext context = {.scene = scene};
    LocalCollisionBox boxes[CC_LOCAL_COLLISION_BOX_CAPACITY];
    int32_t count = GatherLocalCollisionBoxes(&context, boxes);
    int32_t retained = 0;
    float support = passable_support_height;
    for (int32_t i = 0; i < count; ++i) {
        /* A climb can pass the ledge it has already secured. Other surfaces
           remain solid along the entire body, including its upper half. */
        if (support >= boxes[i].maximum.y - 0.055f) continue;
        boxes[retained++] = boxes[i];
    }
    CcCollisionMove movement = CcCollisionMoveCapsule(
        boxes, retained, (CcLimbVec3){previous.x, previous.y, previous.z},
        (CcLimbVec3){proposed.x - previous.x, proposed.y - previous.y,
                     proposed.z - previous.z}, radius, 1.54f + radius);
    *resolved = (Vector3){movement.position.x, movement.position.y,
                          movement.position.z};
    *contact_normal = (Vector3){movement.normal.x, movement.normal.y,
                                movement.normal.z};
    return movement.collided;
}

bool CcLocalMoveCapsuleInternal(CcLocalSceneKind scene, Vector3 previous,
                                Vector3 proposed, float radius,
                                Vector3 *resolved, Vector3 *normal)
{
    return ResolveLocalAgentCapsuleMove(scene, previous, proposed, radius,
                                        -FLT_MAX, resolved, normal);
}

static bool LocalWorldSphereSweep(CcLocalSceneKind scene, Vector3 previous,
                                  Vector3 proposed, float radius,
                                  Vector3 *resolved, Vector3 *normal)
{
    LocalProbeContext context = {.scene = scene};
    CcBiomechVec3 corrected = {proposed.x, proposed.y, proposed.z};
    CcBiomechVec3 contact = {0};
    bool collided = ProbeLocalCollision(
        &context, (CcBiomechVec3){previous.x, previous.y, previous.z},
        (CcBiomechVec3){proposed.x, proposed.y, proposed.z}, radius,
        &corrected, &contact);
    if (resolved != NULL) {
        *resolved = (Vector3){corrected.x, corrected.y, corrected.z};
    }
    if (normal != NULL) {
        *normal = (Vector3){contact.x, contact.y, contact.z};
    }
    return collided;
}

static float LocalClimbContactSupport(CcLocalSceneKind scene, Vector3 point,
                                      Vector3 expected_normal,
                                      float requested_support)
{
    requested_support = fmaxf(0.0f, fminf(1.0f, requested_support));
    if (requested_support < 0.58f) return requested_support;
    float normal_length = sqrtf(expected_normal.x * expected_normal.x +
                                expected_normal.y * expected_normal.y +
                                expected_normal.z * expected_normal.z);
    if (normal_length <= 0.00001f) {
        expected_normal = (Vector3){0.0f, 1.0f, 0.0f};
    } else {
        expected_normal.x /= normal_length;
        expected_normal.y /= normal_length;
        expected_normal.z /= normal_length;
    }
    Vector3 outside = {
        point.x + expected_normal.x * 0.10f,
        point.y + expected_normal.y * 0.10f,
        point.z + expected_normal.z * 0.10f,
    };
    Vector3 inside = {
        point.x - expected_normal.x * 0.08f,
        point.y - expected_normal.y * 0.08f,
        point.z - expected_normal.z * 0.08f,
    };
    return LocalWorldSphereSweep(scene, outside, inside, 0.045f,
                                 NULL, NULL) ? requested_support : 0.0f;
}

static CcLimbVec3 ToLimbVector(Vector3 value)
{
    return (CcLimbVec3){value.x, value.y, value.z};
}

static Vector3 FromLimbVector(CcLimbVec3 value)
{
    return (Vector3){value.x, value.y, value.z};
}

static bool ProbeLocalSurface(void *raw_context, CcLimbVec3 origin,
                              float maximum_drop, CcLimbVec3 *point,
                              CcLimbVec3 *normal)
{
    const LocalProbeContext *context = raw_context;
    CcLocalSceneKind scene = context != NULL ? context->scene :
                                               CC_LOCAL_SCENE_STREET;
    if (StaticBodyBlocked(scene, origin.x, origin.z, 0.025f)) return false;
    float height = SurfaceHeightAt(scene, origin.x, origin.z) + 0.035f;
    if (height > origin.y + 0.05f || origin.y - height > maximum_drop) return false;
    *point = (CcLimbVec3){origin.x, height, origin.z};
    Vector3 surface_normal = SurfaceNormalAt(scene, origin.x, origin.z);
    *normal = (CcLimbVec3){surface_normal.x, surface_normal.y,
                           surface_normal.z};
    return true;
}

static bool ProbeLocalClimbSurface(void *raw_context, CcLimbVec3 sample,
                                   float search_radius, CcLimbVec3 *point,
                                   CcLimbVec3 *normal)
{
    const LocalProbeContext *context = raw_context;
    CcLocalSceneKind scene = context != NULL ? context->scene :
                                               CC_LOCAL_SCENE_STREET;
    if (point == NULL || normal == NULL || search_radius <= 0.0f) {
        return false;
    }
    static const Vector3 directions[] = {
        {1.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f}, {0.0f, -1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, -1.0f},
    };
    Vector3 origin = FromLimbVector(sample);
    bool found = false;
    float best_distance = FLT_MAX;
    for (int32_t direction = 0;
         direction < (int32_t)(sizeof(directions) / sizeof(directions[0]));
         ++direction) {
        Vector3 outside = PhysicsAdd(
            origin, PhysicsScale(directions[direction], search_radius));
        Vector3 inside = PhysicsAdd(
            origin, PhysicsScale(directions[direction], -search_radius));
        Vector3 resolved = inside;
        Vector3 contact_normal = {0};
        if (!LocalWorldSphereSweep(scene, outside, inside, 0.035f,
                                   &resolved, &contact_normal)) {
            continue;
        }
        float normal_length = PhysicsLength(contact_normal);
        if (normal_length <= 0.50f) continue;
        contact_normal = PhysicsScale(contact_normal, 1.0f / normal_length);
        Vector3 contact = PhysicsSubtract(
            resolved, PhysicsScale(contact_normal, 0.035f));
        float distance = PhysicsLength(PhysicsSubtract(contact, origin));
        if (distance > search_radius * 1.12f || distance >= best_distance) {
            continue;
        }
        found = true;
        best_distance = distance;
        *point = ToLimbVector(contact);
        *normal = ToLimbVector(contact_normal);
    }

    CcLimbVec3 terrain_point = {0};
    CcLimbVec3 terrain_normal = {0};
    CcLimbVec3 terrain_origin = sample;
    terrain_origin.y += search_radius * 0.50f;
    if (ProbeLocalSurface(raw_context, terrain_origin,
                          search_radius * 1.55f,
                          &terrain_point, &terrain_normal)) {
        float distance = PhysicsLength(PhysicsSubtract(
            FromLimbVector(terrain_point), origin));
        if (distance <= search_radius * 1.12f && distance < best_distance) {
            found = true;
            *point = terrain_point;
            *normal = terrain_normal;
        }
    }
    return found;
}

static Vector3 ShellPodBaseCenter(const CcLocalAgent *agent)
{
    return (Vector3){agent->position.x,
                     agent->position.y + agent->limb_rig.morphology.body_height,
                     agent->position.z};
}

static Vector3 ShellPodCenter(const CcLocalAgent *agent)
{
    Vector3 body = ShellPodBaseCenter(agent);
    body.y += agent->limb_rig.supported_height_offset;
    return body;
}

static float LocalClimbTriggerRadius(const CcLocalAgent *agent)
{
    if (agent == NULL ||
        agent->morphology == CC_MORPHOLOGY_BIPED) {
        return agent != NULL ? agent->radius : 0.0f;
    }
    float reach = agent->radius;
    for (int32_t limb = 0;
         limb < agent->limb_rig.morphology.limb_count; ++limb) {
        const CcLimbVec3 rest =
            agent->limb_rig.morphology.limbs[limb].rest_contact_local;
        reach = fmaxf(reach, sqrtf(rest.x * rest.x + rest.z * rest.z));
    }
    return fminf(reach, 0.96f);
}

static const NavPlatform *ClimbPlatformAt(CcLocalSceneKind scene,
                                          float x, float z, float radius,
                                          float feet_height)
{
    if (scene != CC_LOCAL_SCENE_STREET) return NULL;
    const NavPlatform *highest = NULL;
    for (int32_t i = 0; i < StreetPhysicsPlatformCount(); ++i) {
        const NavPlatform *platform = StreetPhysicsPlatformAt(i);
        if (platform == NULL) continue;
        float top = PlatformTopHeight(platform);
        Rectangle footprint = {platform->x, platform->z,
                               platform->width, platform->depth};
        if (top > feet_height + 0.24f &&
            CircleTouchesFootprint(x, z, radius, footprint) &&
            (highest == NULL || top > PlatformTopHeight(highest))) {
            highest = platform;
        }
    }
    return highest;
}

static const NavPlatform *SupportingPlatformAt(float x, float z,
                                               float feet_height)
{
    for (int32_t i = 0; i < StreetPhysicsPlatformCount(); ++i) {
        const NavPlatform *platform = StreetPhysicsPlatformAt(i);
        if (platform == NULL) continue;
        if (fabsf(PlatformTopHeight(platform) - feet_height) > 0.08f) continue;
        if (x >= platform->x && x <= platform->x + platform->width &&
            z >= platform->z && z <= platform->z + platform->depth) {
            return platform;
        }
    }
    return NULL;
}

const char *CcLocalTraversalName(CcTraversalMode mode)
{
    switch (mode) {
        case CC_TRAVERSAL_WALK: return "WALK";
        case CC_TRAVERSAL_JUMP: return "JUMP";
        case CC_TRAVERSAL_VAULT: return "VAULT";
        case CC_TRAVERSAL_CLIMB: return "CLIMB";
        case CC_TRAVERSAL_DESCEND: return "DOWN-CLIMB";
        case CC_TRAVERSAL_SWIM: return "SWIM";
        case CC_TRAVERSAL_DROP: return "DROP";
        case CC_TRAVERSAL_RAGDOLL: return "RAGDOLL";
        case CC_TRAVERSAL_GET_UP: return "GET UP";
        case CC_TRAVERSAL_IDLE:
        default: return "IDLE";
    }
}

static float AthleticThreshold(int32_t level)
{
    return 30.0f + (float)level * 20.0f;
}

static bool AthleticDisciplineValid(CcAthleticDiscipline discipline)
{
    return discipline >= 0 && discipline < CC_ATHLETIC_DISCIPLINE_COUNT;
}

static int32_t AthleticLevel(const CcLocalAgent *agent,
                             CcAthleticDiscipline discipline)
{
    if (agent == NULL || !AthleticDisciplineValid(discipline)) return 1;
    int32_t level = agent->athletics.level[discipline];
    if (level < 1) return 1;
    return level > CC_ATHLETIC_MAX_LEVEL ? CC_ATHLETIC_MAX_LEVEL : level;
}

static float AthleticBonus(const CcLocalAgent *agent,
                           CcAthleticDiscipline discipline, float per_level)
{
    return 1.0f + (float)(AthleticLevel(agent, discipline) - 1) * per_level;
}

void CcLocalAgentTrainAthleticism(CcLocalAgent *agent,
                                  CcAthleticDiscipline discipline,
                                  float experience)
{
    if (agent == NULL || !AthleticDisciplineValid(discipline) ||
        !isfinite(experience) || experience <= 0.0f) {
        return;
    }
    int32_t *level = &agent->athletics.level[discipline];
    if (*level < 1) *level = 1;
    if (*level >= CC_ATHLETIC_MAX_LEVEL) return;
    float *stored = &agent->athletics.experience[discipline];
    *stored += experience;
    while (*level < CC_ATHLETIC_MAX_LEVEL &&
           *stored >= AthleticThreshold(*level)) {
        *stored -= AthleticThreshold(*level);
        *level += 1;
    }
    if (*level >= CC_ATHLETIC_MAX_LEVEL) *stored = 0.0f;
}

void CcLocalAgentSetAthleticLevel(CcLocalAgent *agent,
                                  CcAthleticDiscipline discipline,
                                  int32_t level)
{
    if (agent == NULL || !AthleticDisciplineValid(discipline)) return;
    agent->athletics.level[discipline] =
        level < 1 ? 1 : level > CC_ATHLETIC_MAX_LEVEL ?
        CC_ATHLETIC_MAX_LEVEL : level;
    agent->athletics.experience[discipline] = 0.0f;
}

int32_t CcLocalAgentHeroicTier(const CcLocalAgent *agent)
{
    if (agent == NULL) return 1;
    int32_t total = 0;
    for (int32_t discipline = 0;
         discipline < CC_ATHLETIC_DISCIPLINE_COUNT; ++discipline) {
        total += AthleticLevel(agent, (CcAthleticDiscipline)discipline);
    }
    return total / CC_ATHLETIC_DISCIPLINE_COUNT;
}

float CcLocalAgentAthleticProgress(const CcLocalAgent *agent,
                                   CcAthleticDiscipline discipline)
{
    if (agent == NULL || !AthleticDisciplineValid(discipline)) return 0.0f;
    int32_t level = AthleticLevel(agent, discipline);
    if (level >= CC_ATHLETIC_MAX_LEVEL) return 1.0f;
    return fmaxf(0.0f, fminf(agent->athletics.experience[discipline] /
                             AthleticThreshold(level), 1.0f));
}

const char *CcAthleticDisciplineName(CcAthleticDiscipline discipline)
{
    switch (discipline) {
        case CC_ATHLETIC_MOBILITY: return "MOBILITY";
        case CC_ATHLETIC_GRIP: return "GRIP";
        case CC_ATHLETIC_POWER: return "POWER";
        default: return "ATHLETIC";
    }
}

Vector2 CcLocalAgentPosition(const CcLocalAgent *agent)
{
    return (Vector2){agent->position.x, agent->position.z};
}

static CcLocalSceneKind AgentSceneForCall(const CcLocalAgent *agent,
                                          bool market_interior)
{
    if (market_interior) return CC_LOCAL_SCENE_MARKET;
    return agent != NULL && agent->scene == CC_LOCAL_SCENE_ROAD ?
           CC_LOCAL_SCENE_ROAD : CC_LOCAL_SCENE_STREET;
}

static void ApplyAgentWalkingProfile(CcLocalAgent *agent)
{
    if (agent == NULL || agent->morphology != CC_MORPHOLOGY_BIPED ||
        !agent->humanoid.initialized) return;
    float signature_weight = agent->crowned ? 0.0f : 0.40f;
    float cadence_scale = 1.0f +
        (agent->appearance.gait_cadence_scale - 1.0f) * signature_weight;
    float stride_scale = 1.0f +
        (agent->appearance.stride_scale - 1.0f) * signature_weight;
    CcHumanoidGaitSetWalkingProfile(
        &agent->humanoid, cadence_scale, stride_scale);
}

void CcLocalAgentInit(CcLocalAgent *agent, Vector2 position, bool market_interior)
{
    *agent = (CcLocalAgent){0};
    agent->scene = market_interior ? CC_LOCAL_SCENE_MARKET :
                                     CC_LOCAL_SCENE_STREET;
    agent->position = (Vector3){position.x,
                                SurfaceHeightAt(agent->scene, position.x,
                                                position.y),
                                position.y};
    agent->facing_yaw = 0.75f * PI;
    agent->traversal = CC_TRAVERSAL_IDLE;
    agent->support_state = CC_HUMANOID_SUPPORT_STABLE;
    agent->radius = PLAYER_COLLISION_RADIUS;
    agent->grounded = true;
    agent->allow_downclimb = true;
    agent->crowned = true;
    agent->appearance = CcNpcCrownlessAppearance();
    agent->tunic_color = agent->appearance.outer;
    agent->target_point = agent->position;
    agent->combat.health = CC_LOCAL_COMBAT_MAX_HEALTH;
    agent->combat.posture = CC_LOCAL_COMBAT_MAX_POSTURE;
    agent->combat.life_state = CC_LIFE_ALIVE;
    agent->combat.target_index = -1;
    agent->combat.queued_skill = -1;
    agent->combat.active_skill = -1;
    for (int32_t discipline = 0;
         discipline < CC_ATHLETIC_DISCIPLINE_COUNT; ++discipline) {
        agent->athletics.level[discipline] = 1;
    }
    CcLocalAgentSetMorphology(agent, CC_MORPHOLOGY_BIPED, market_interior);
}

void CcLocalAgentSetNpcAppearance(CcLocalAgent *agent, uint32_t seed,
                                  CcNpcRole role, Color accent)
{
    if (agent == NULL) return;
    agent->appearance = CcNpcAppearanceGenerate(seed, role, accent);
    agent->tunic_color = agent->appearance.outer;
    ApplyAgentWalkingProfile(agent);
}

void CcLocalAgentSetMorphology(CcLocalAgent *agent, CcMorphologyPreset preset,
                               bool market_interior)
{
    CcLimbMorphology morphology;
    if (!CcLimbMorphologyFromPreset(&morphology, preset)) return;
    agent->morphology = preset;
    agent->ragdoll_visual_blend = 0.0f;
    agent->multileg_ragdoll = (CcMultilegRagdoll){0};
    agent->climb_route = (CcRobotClimbRoute){0};
    agent->climb_route_index = 0;
    agent->free_climbing = false;
    agent->scene = AgentSceneForCall(agent, market_interior);
    LocalProbeContext context = {.scene = agent->scene};
    Vector3 body = {agent->position.x,
                    agent->position.y + morphology.body_height,
                    agent->position.z};
    CcLimbRigInit(&agent->limb_rig, &morphology, ToLimbVector(body),
                  agent->facing_yaw, ProbeLocalSurface, &context);
    agent->humanoid_needs_reset = false;
    if (preset == CC_MORPHOLOGY_BIPED) {
        CcHumanoidGaitInit(&agent->humanoid, ToLimbVector(agent->position),
                            agent->facing_yaw, ProbeLocalSurface, &context);
        ApplyAgentWalkingProfile(agent);
        agent->support_state = agent->humanoid.support_state;
        agent->render_pose = agent->humanoid.pose;
        agent->stepped_pose = (CcSteppedPoseState){0};
        agent->render_pose_valid = true;
        agent->simulation_accumulator = 0.0f;
        agent->cape = (CcLocalCapeState){0};
        UpdateHeroCape(agent, 1.0f / 60.0f);
        agent->previous_cape = agent->cape;
        agent->render_cape = agent->cape;
    } else {
        agent->humanoid = (CcHumanoidGait){0};
        agent->render_pose = (CcHumanoidPose){0};
        agent->stepped_pose = (CcSteppedPoseState){0};
        agent->render_pose_valid = false;
        agent->simulation_accumulator = 0.0f;
        agent->cape = (CcLocalCapeState){0};
        agent->previous_cape = (CcLocalCapeState){0};
        agent->render_cape = (CcLocalCapeState){0};
    }
}

void CcLocalAgentSetScene(CcLocalAgent *agent, CcLocalSceneKind scene)
{
    if (agent == NULL || scene < CC_LOCAL_SCENE_STREET ||
        scene > CC_LOCAL_SCENE_ROAD) return;
    agent->scene = scene;
    agent->position.y = SurfaceHeightAt(
        scene, agent->position.x, agent->position.z);
    agent->velocity = (Vector3){0};
    agent->separation_velocity = (Vector3){0};
    agent->target_point = agent->position;
    agent->command_point = agent->position;
    agent->command_origin = agent->position;
    agent->exact_target_valid = false;
    agent->target_valid = false;
    agent->command_point_valid = false;
    agent->navigation_active = false;
    agent->navigation_world_exit = false;
    agent->navigation_point_count = 0;
    agent->navigation_point_index = 0;
    agent->navigation_destination_room = -1;
    agent->world_target = CC_LOCAL_WORLD_TARGET_NONE;
    agent->climbing = false;
    agent->climbing_down = false;
    agent->vaulting = false;
    agent->swimming = false;
    agent->grounded = true;
    agent->traversal = CC_TRAVERSAL_IDLE;
    CcLocalAgentSetMorphology(
        agent, agent->morphology, scene == CC_LOCAL_SCENE_MARKET);
}

void CcLocalAgentCycleMorphology(CcLocalAgent *agent, bool market_interior)
{
    CcMorphologyPreset next = (CcMorphologyPreset)(agent->morphology + 1);
    if (next >= CC_MORPHOLOGY_PRESET_COUNT) next = CC_MORPHOLOGY_BIPED;
    CcLocalAgentSetMorphology(agent, next, market_interior);
}

const char *CcLocalAgentMorphologyName(const CcLocalAgent *agent)
{
    return agent->limb_rig.morphology.name != NULL ?
           agent->limb_rig.morphology.name : "UNRIGGED";
}
