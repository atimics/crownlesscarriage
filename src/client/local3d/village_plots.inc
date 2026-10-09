/* Household grounds share their saved house identity and their solid geometry
   with navigation. Coordinates use the authored settlement's ground frame. */
static Rectangle VillagePlotFootprint(CcLocalPlotRect f)
{
    return (Rectangle){f.x, f.z, f.width, f.depth};
}

static float VillagePlotGround(CcLocalPlotRect f, bool terrain_ground)
{
    return terrain_ground ? TerrainFootprintHeight(VillagePlotFootprint(f)) : 0.0f;
}

static Color VillagePlotWood(const CcBuildingState *house)
{
    float upkeep = house != NULL ? (float)house->upkeep / 100.0f : 0.7f;
    float burn = house != NULL ? (float)house->fire_damage / 100.0f : 0.0f;
    Color wood = BlendColor((Color){92,85,67,255}, (Color){143,107,64,255}, upkeep);
    return BlendColor(wood, (Color){43,34,29,255}, burn * 0.85f);
}

static void DrawVillagePlotFence(const CcLocalHouseholdObstacle *prop,
                                 bool terrain_ground, Color wood,
                                 float snow_cover)
{
    CcLocalPlotRect f = prop->footprint;
    float base = VillagePlotGround(f, terrain_ground);
    bool along_x = f.width > f.depth;
    float length = along_x ? f.width : f.depth;
    int32_t spans = (int32_t)ceilf(length / 1.8f);
    for (int32_t post = 0; post <= spans; ++post) {
        float along = 0.07f + (length - 0.14f) * (float)post / (float)spans;
        Vector3 center = {f.x + (along_x ? along : f.width * 0.5f),
                          base + prop->height * 0.5f,
                          f.z + (along_x ? f.depth * 0.5f : along)};
        DrawBox(center, (Vector3){0.14f, prop->height, 0.14f}, wood);
    }
    for (int32_t rail = 0; rail < 2; ++rail) {
        DrawBox((Vector3){f.x + f.width * 0.5f,
                          base + 0.35f + (float)rail * 0.40f,
                          f.z + f.depth * 0.5f},
                (Vector3){f.width, 0.11f, f.depth}, ShadeColor(wood, 0.88f));
    }
    if (snow_cover > 0.12f) {
        DrawBox((Vector3){f.x + f.width * 0.5f, base + 0.82f,
                          f.z + f.depth * 0.5f},
                (Vector3){f.width, 0.025f + snow_cover * 0.045f, f.depth},
                BlendColor(wood, (Color){226,232,224,255}, snow_cover));
    }
}

static void DrawVillagePlotShed(const CcLocalHouseholdObstacle *prop,
                                bool terrain_ground, Color wood,
                                float snow_cover)
{
    CcLocalPlotRect f = prop->footprint;
    float base = VillagePlotGround(f, terrain_ground);
    Color dark = ShadeColor(wood, 0.68f);
    DrawBox((Vector3){f.x + f.width * 0.5f, base + 0.10f, f.z + f.depth * 0.5f},
            (Vector3){f.width, 0.20f, f.depth}, dark);
    DrawBox((Vector3){f.x + f.width * 0.5f, base + 0.93f, f.z + 0.10f},
            (Vector3){f.width, 1.66f, 0.18f}, wood);
    for (int32_t side = 0; side < 2; ++side) {
        float x = f.x + 0.11f + (float)side * (f.width - 0.22f);
        DrawBox((Vector3){x, base + 1.05f, f.z + f.depth - 0.11f},
                (Vector3){0.18f, 2.10f, 0.18f}, dark);
        DrawBox((Vector3){x, base + 0.60f, f.z + f.depth * 0.5f},
                (Vector3){0.16f, 1.20f, f.depth}, wood);
    }
    Color roof = BlendColor(wood, (Color){187,163,106,255}, 0.36f);
    roof = BlendColor(roof, (Color){222,229,224,255}, snow_cover * 0.90f);
    DrawTiltedBox((Vector3){f.x + f.width * 0.5f, base + 2.15f,
                            f.z + f.depth * 0.5f},
                  (Vector3){f.width, 0.20f, f.depth},
                  (Vector3){1,0,0}, 7.0f, roof);
    for (int32_t stack = 0; stack < 3; ++stack) {
        DrawBox((Vector3){f.x + f.width * 0.5f, base + 0.28f + (float)stack * 0.16f,
                          f.z + f.depth * 0.65f},
                (Vector3){f.width * 0.62f, 0.13f, f.depth * 0.32f},
                ShadeColor(wood, 1.08f + (float)stack * 0.05f));
    }
}

static void DrawVillageHouseholdPlots(const CcSettlement *place,
                                      const CcLocalPlaceProfile *profile,
                                      bool terrain_ground, float snow_cover)
{
    if (profile == NULL) return;
    snow_cover = fmaxf(0.0f, fminf(1.0f, snow_cover));
    for (int32_t i = 0; i < profile->household_count; ++i) {
        const CcLocalHouseholdPlot *plot = &profile->household[i];
        const CcBuildingState *house = CcSettlementBuilding(place, plot->plot_id);
        float upkeep = house != NULL ? (float)house->upkeep / 100.0f : 0.7f;
        float burn = house != NULL ? (float)house->fire_damage / 100.0f : 0.0f;
        Color soil = BlendColor((Color){110,102,75,255},
                               (Color){151,133,94,255}, upkeep);
        soil = BlendColor(soil, (Color){71,60,44,255}, burn * 0.65f);
        CcLocalPlotRect yard = plot->yard;
        /* Small ground tiles follow the plot grade at the edge. The clear
           strip around each garden gives its household walking access. */
        for (float z = yard.z; z < yard.z + yard.depth; z += 1.0f) {
            for (float x = yard.x; x < yard.x + yard.width; x += 1.0f) {
                CcLocalPlotRect tile = {x,z,
                    fminf(1.0f,yard.x + yard.width - x),
                    fminf(1.0f,yard.z + yard.depth - z)};
                float base = VillagePlotGround(tile, terrain_ground);
                DrawBox((Vector3){x + tile.width * 0.5f, base + 0.025f,
                                  z + tile.depth * 0.5f},
                        (Vector3){tile.width, 0.05f, tile.depth}, soil);
            }
        }
        CcLocalPlotRect garden = plot->garden;
        if (garden.width <= 0.0f || garden.depth <= 0.0f) continue;
        bool workyard = plot->kind == CC_LOCAL_YARD_WORKSHOP;
        Color bed = BlendColor((Color){81,68,45,255}, (Color){215,225,218,255}, snow_cover);
        Color leaf = BlendColor((Color){87,102,59,255}, (Color){111,131,71,255}, upkeep);
        leaf = BlendColor(leaf, (Color){98,80,47,255}, burn);
        leaf = BlendColor(leaf, (Color){218,230,220,255}, snow_cover * 0.90f);
        Color wood = VillagePlotWood(house);
        for (float x = garden.x + 0.28f; x < garden.x + garden.width - 0.10f; x += 0.82f) {
            CcLocalPlotRect row = {x,garden.z,0.42f,garden.depth};
            float base = VillagePlotGround(row, terrain_ground);
            DrawBox((Vector3){x, base + 0.065f, garden.z + garden.depth * 0.5f},
                    (Vector3){0.42f, 0.12f, garden.depth}, workyard ? wood : bed);
            if (workyard) continue;
            for (float z = garden.z + 0.25f; z < garden.z + garden.depth - 0.15f; z += 0.72f) {
                float height = plot->kind == CC_LOCAL_YARD_THRESHING ? 0.42f : 0.20f;
                height *= 0.45f + upkeep * 0.55f;
                DrawBox((Vector3){x, base + 0.12f + height * 0.5f, z},
                        (Vector3){0.35f, height, 0.38f}, leaf);
            }
        }
        if (house != NULL && house->repair_progress > 0 && house->repair_progress < 100) {
            float base = VillagePlotGround(garden, terrain_ground);
            /* Low work timber sits inside the bed, within normal step height. */
            for (int32_t plank = 0; plank < 3; ++plank)
                DrawBox((Vector3){garden.x + garden.width * 0.5f,
                                  base + 0.10f + (float)plank * 0.06f,
                                  garden.z + 0.3f + (float)plank * 0.12f},
                        (Vector3){fminf(garden.width * 0.75f, 2.5f),0.05f,0.16f},
                        (Color){164,125,76,255});
        }
    }
    int32_t count = CcLocalPlaceHouseholdObstacleCount(profile);
    for (int32_t i = 0; i < count; ++i) {
        CcLocalHouseholdObstacle prop;
        if (!CcLocalPlaceHouseholdObstacleAt(profile, i, &prop)) continue;
        Color wood = VillagePlotWood(CcSettlementBuilding(place, prop.plot_id));
        if (prop.kind == CC_LOCAL_HOUSEHOLD_SHED)
            DrawVillagePlotShed(&prop, terrain_ground, wood, snow_cover);
        else
            DrawVillagePlotFence(&prop, terrain_ground, wood, snow_cover);
    }
}
