typedef struct BuildingAppearance {
    float burn;
    float wear;
    float roof_wear;
    float fresh;
    float snow;
    float wetness;
    float repair;
    uint32_t seed;
    bool gutted;
} BuildingAppearance;

static BuildingAppearance BuildingAppearanceForPlot(
    const CcSettlement *place, const CcLocalPlaceProfile *profile,
    int32_t index, int32_t day)
{
    BuildingAppearance result = {0};
    if (place == NULL || profile == NULL || index < 0 ||
        index >= profile->building_count) return result;
    const CcBuildingState *house = CcSettlementBuilding(
        place, profile->building[index].plot_id);
    result.seed = house != NULL ? house->style_seed :
        TownFireHash((uint32_t)place->id * UINT32_C(31) + (uint32_t)index);
    result.burn = house != NULL ? CcSettlementBuildingBurn(place,
        profile->building[index].plot_id) : TownBuildingBurn(place, index);
    result.gutted = result.burn >= 0.75f;
    if (house != NULL) {
        result.wear = (float)(100 - house->upkeep) / 100.0f;
        result.roof_wear = (float)(100 - house->roof_health) / 100.0f;
        int64_t age = (int64_t)day - house->built_day;
        int64_t repair_age = (int64_t)day - house->last_repair_day;
        if (age >= 0 && age < 120) result.fresh = 1.0f - (float)age / 120.0f;
        if (house->last_repair_day > 0 && repair_age >= 0 && repair_age < 90) {
            result.fresh = fmaxf(result.fresh, 1.0f - (float)repair_age / 90.0f);
        }
        if (house->repair_progress > 0 && house->repair_progress < 100) {
            result.repair = (float)house->repair_progress / 100.0f;
        }
    }
    CcBuildingWeather weather = CcBuildingWeatherForDay(day);
    result.wetness = weather.wetness;
    result.snow = CcBuildingRoofSnow(weather, 0.55f, 0.85f,
        result.gutted || place->population <= 0 ? 0.0f : 0.5f);
    return result;
}

static void DrawBuildingCare(const WorldBuilding *building,
                              BuildingAppearance appearance, Color wall)
{
    Rectangle p = building->footprint;
    float h = building->height;
    float front = p.y + p.height + 0.026f;
    float eaves = h * 0.68f;
    if (appearance.wear > 0.35f) {
        /* Damp and old plaster gather at the wall foot. The doorway keeps its
         * existing clear width and its exact interaction position. */
        Color damp = BlendColor(wall, WORLD_FOLIAGE, appearance.wear * 0.40f);
        for (int32_t side = -1; side <= 1; side += 2) {
            float x = p.x + p.width * (side < 0 ? 0.17f : 0.83f);
            DrawBox((Vector3){x, 0.22f, front},
                    (Vector3){p.width * 0.26f, 0.44f, 0.045f}, damp);
        }
        if (appearance.wear > 0.65f) {
            DrawTiltedBox((Vector3){p.x + 0.18f, eaves * 0.43f,
                                    p.y + p.height * 0.74f},
                          (Vector3){0.23f, eaves * 0.91f, 0.28f},
                          (Vector3){0,0,1}, -9.0f, WORLD_WOOD_SHADOW);
        }
    }
    if (appearance.fresh > 0.05f && !appearance.gutted) {
        Color plaster = BlendColor(wall, CC_VISUAL_PALETTE.parchment.light,
                                   0.22f + appearance.fresh * 0.20f);
        DrawBox((Vector3){p.x + p.width * 0.19f, eaves * 0.41f, front},
                (Vector3){p.width * 0.26f, eaves * 0.51f, 0.055f}, plaster);
        DrawBox((Vector3){p.x + p.width * 0.06f, eaves * 0.45f, front + 0.025f},
                (Vector3){0.18f, eaves * 0.85f, 0.16f}, WORLD_WOOD_LIGHT);
    }
    if (appearance.repair > 0.0f) {
        /* Work stands beside the wall within the roof overhang. Its front
         * keeps the same clear approach used by the shop and house doors. */
        float x = p.x + p.width + 0.22f;
        for (int32_t end = 0; end < 2; ++end) {
            float z = p.y + p.height * (end == 0 ? 0.18f : 0.82f);
            DrawBox((Vector3){x, (h + 0.4f) * 0.5f, z},
                    (Vector3){0.18f, h + 0.4f, 0.18f}, WORLD_WOOD_LIGHT);
        }
        for (int32_t level = 1; level <= 2; ++level) {
            DrawBox((Vector3){x, h * (float)level / 2.7f,
                              p.y + p.height * 0.5f},
                    (Vector3){0.38f, 0.16f, p.height * 0.73f}, WORLD_WOOD);
        }
        for (int32_t timber = 0; appearance.gutted && timber < 4; ++timber) {
            DrawBox((Vector3){p.x + p.width * 0.36f,
                              0.20f + (float)timber * 0.16f, p.y + 0.60f},
                    (Vector3){p.width * 0.48f, 0.14f, 0.22f}, WORLD_WOOD_LIGHT);
        }
    }
}

/* Called in the building's local ground/yaw frame from both scene paths. */
static BuildingAppearance DrawConditionedSettlementBuilding(
    const CcSettlement *place, const CcLocalPlaceProfile *profile,
    const WorldBuilding *building, int32_t index, int32_t day,
    Color wall, Color roof)
{
    BuildingAppearance appearance = BuildingAppearanceForPlot(place, profile, index, day);
    BuildingRoofSurface previous_surface = building_roof_surface;
    building_roof_surface = (BuildingRoofSurface){
        .snow = appearance.snow,
        .wear = fmaxf(appearance.wear, appearance.roof_wear),
        .fresh = appearance.fresh,
        .seed = appearance.seed,
    };
    wall = BlendColor(wall, WORLD_STONE, appearance.wear * 0.20f);
    wall = ShadeColor(wall, 1.0f - appearance.wetness * 0.10f);
    roof = ShadeColor(roof, 1.0f - appearance.wetness * 0.18f);
    if (appearance.burn > 0.0f) {
        wall = BlendColor(wall, TOWN_CHAR, 0.30f + 0.40f * appearance.burn);
        roof = BlendColor(roof, TOWN_CHAR, 0.65f + 0.35f * appearance.burn);
        DrawFireScorch(building->footprint, appearance.burn, appearance.seed);
    }
    if (appearance.gutted) {
        DrawGuttedBuildingShell(building->footprint, building->height,
                                wall, appearance.seed);
        if (appearance.snow > 0.02f) {
            Rectangle p = building->footprint;
            DrawBox((Vector3){p.x + p.width * 0.50f,
                              0.12f + appearance.snow * 0.07f,
                              p.y + p.height * 0.48f},
                    (Vector3){p.width * 0.73f, 0.08f + appearance.snow * 0.08f,
                              p.height * 0.70f}, BUILDING_SNOW);
        }
    } else {
        DrawSettlementBuilding(building, wall, roof, profile, index,
                                appearance.burn > 0.0f);
        if (appearance.burn > 0.0f) {
            DrawScorchedBuildingMarks(building->footprint, building->height,
                                      appearance.burn, appearance.seed);
        }
    }
    DrawBuildingCare(building, appearance, wall);
    building_roof_surface = previous_surface;
    return appearance;
}
