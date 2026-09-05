#if 0
typedef struct OpenWorldRenderChunk {
    int32_t x;
    int32_t z;
    uint64_t generation;
    Model model;
    bool ready;
} OpenWorldRenderChunk;

#define CC_WORLD_OVERVIEW_CELLS 48

typedef struct OpenWorldOverviewCache {
    Model model;
    uint32_t world_seed;
    uint32_t generator_version;
    float minimum_x;
    float minimum_z;
    float maximum_x;
    float maximum_z;
    bool ready;
} OpenWorldOverviewCache;

static OpenWorldRenderChunk
    open_world_render_chunks[CC_WORLD_STREAM_CAPACITY] = {0};
static OpenWorldOverviewCache open_world_overview = {0};

static void OpenWorldRenderCacheClear(void)
{
    for (int32_t i = 0; i < CC_WORLD_STREAM_CAPACITY; ++i) {
        if (open_world_render_chunks[i].ready) {
            UnloadModel(open_world_render_chunks[i].model);
        }
        open_world_render_chunks[i] = (OpenWorldRenderChunk){0};
    }
    if (open_world_overview.ready) {
        UnloadModel(open_world_overview.model);
    }
    open_world_overview = (OpenWorldOverviewCache){0};
}

static Color OpenWorldTerrainColor(const CcWorldManifest *manifest,
                                   float x, float z, Vector3 normal)
{
    CcWorldSurfaceKind surface = CcWorldSurfaceAt(manifest, x, z);
    Color color = surface == CC_WORLD_SURFACE_ROAD ? WORLD_ROAD_LIGHT :
                  surface == CC_WORLD_SURFACE_SETTLEMENT ? WORLD_EARTH_LIGHT :
                  WORLD_GRASS;
    if (normal.y < 0.78f) {
        color = BlendColor(color, WORLD_STONE, (0.78f - normal.y) * 2.8f);
    }
    float variation = sinf(x * 0.071f + z * 0.113f) * 0.035f;
    return ShadeColor(color, 1.0f + variation);
}

static bool OpenWorldOverviewMatches(const CcWorldManifest *manifest)
{
    return manifest != NULL && open_world_overview.ready &&
           open_world_overview.world_seed == manifest->world_seed &&
           open_world_overview.generator_version ==
               manifest->generator_version &&
           open_world_overview.minimum_x == manifest->minimum_x &&
           open_world_overview.minimum_z == manifest->minimum_z &&
           open_world_overview.maximum_x == manifest->maximum_x &&
           open_world_overview.maximum_z == manifest->maximum_z;
}

static Model OpenWorldBuildOverviewModel(const CcWorldManifest *manifest)
{
    const int32_t vertex_count = CC_WORLD_OVERVIEW_CELLS *
        CC_WORLD_OVERVIEW_CELLS * 6;
    TerrainMeshWriter writer;
    if (manifest == NULL ||
        !TerrainMeshWriterAllocate(&writer, vertex_count)) return (Model){0};
    float step_x = (manifest->maximum_x - manifest->minimum_x) /
                   (float)CC_WORLD_OVERVIEW_CELLS;
    float step_z = (manifest->maximum_z - manifest->minimum_z) /
                   (float)CC_WORLD_OVERVIEW_CELLS;
    for (int32_t row = 0; row < CC_WORLD_OVERVIEW_CELLS; ++row) {
        for (int32_t column = 0;
             column < CC_WORLD_OVERVIEW_CELLS; ++column) {
            float x = manifest->minimum_x + (float)column * step_x;
            float z = manifest->minimum_z + (float)row * step_z;
            Vector3 points[4] = {
                {x, CcWorldTerrainHeight(manifest, x, z) - 0.10f, z},
                {x + step_x,
                 CcWorldTerrainHeight(manifest, x + step_x, z) - 0.10f, z},
                {x, CcWorldTerrainHeight(manifest, x, z + step_z) - 0.10f,
                 z + step_z},
                {x + step_x,
                 CcWorldTerrainHeight(manifest, x + step_x, z + step_z) -
                     0.10f,
                 z + step_z},
            };
            Vector3 normal = {
                ((points[0].y + points[2].y) -
                 (points[1].y + points[3].y)) * 0.5f * step_z,
                step_x * step_z,
                ((points[0].y + points[1].y) -
                 (points[2].y + points[3].y)) * 0.5f * step_x,
            };
            normal = Vector3Normalize(normal);
            Color color = OpenWorldTerrainColor(
                manifest, x + step_x * 0.5f, z + step_z * 0.5f, normal);
            static const int32_t order[6] = {0, 3, 1, 0, 2, 3};
            for (int32_t vertex = 0; vertex < 6; ++vertex) {
                TerrainMeshWriteVertexWithNormal(
                    &writer, points[order[vertex]], normal, color);
            }
        }
    }
    if (writer.cursor != vertex_count) {
        UnloadMesh(writer.mesh);
        return (Model){0};
    }
    UploadMesh(&writer.mesh, false);
    Model model = LoadModelFromMesh(writer.mesh);
    ReleaseUploadedModelCpuData(&model);
    ApplyWorldShader(&model);
    return model;
}

static void OpenWorldSyncOverview(const CcWorldManifest *manifest)
{
    if (manifest == NULL || OpenWorldOverviewMatches(manifest)) return;
    if (open_world_overview.ready) UnloadModel(open_world_overview.model);
    open_world_overview = (OpenWorldOverviewCache){0};
    Model model = OpenWorldBuildOverviewModel(manifest);
    if (!IsModelValid(model)) return;
    open_world_overview = (OpenWorldOverviewCache){
        .model = model,
        .world_seed = manifest->world_seed,
        .generator_version = manifest->generator_version,
        .minimum_x = manifest->minimum_x,
        .minimum_z = manifest->minimum_z,
        .maximum_x = manifest->maximum_x,
        .maximum_z = manifest->maximum_z,
        .ready = true,
    };
}

static bool OpenWorldRenderChunkMatches(const OpenWorldRenderChunk *render,
                                        const CcWorldChunk *chunk)
{
    return render != NULL && chunk != NULL && render->ready &&
           render->x == chunk->x && render->z == chunk->z &&
           render->generation == chunk->generation;
}

static Model OpenWorldBuildChunkModel(const CcWorldStream *stream,
                                      const CcWorldChunk *chunk)
{
    const int32_t vertex_count = CC_WORLD_CHUNK_CELLS *
        CC_WORLD_CHUNK_CELLS * 6;
    TerrainMeshWriter writer;
    if (!TerrainMeshWriterAllocate(&writer, vertex_count)) return (Model){0};
    float step = CC_WORLD_CHUNK_SIZE / (float)CC_WORLD_CHUNK_CELLS;
    float origin_x = (float)chunk->x * CC_WORLD_CHUNK_SIZE;
    float origin_z = (float)chunk->z * CC_WORLD_CHUNK_SIZE;
    for (int32_t row = 0; row < CC_WORLD_CHUNK_CELLS; ++row) {
        for (int32_t column = 0; column < CC_WORLD_CHUNK_CELLS; ++column) {
            int32_t near_left = row * CC_WORLD_CHUNK_SAMPLES + column;
            int32_t near_right = near_left + 1;
            int32_t far_left = near_left + CC_WORLD_CHUNK_SAMPLES;
            int32_t far_right = far_left + 1;
            float x = origin_x + (float)column * step;
            float z = origin_z + (float)row * step;
            Vector3 points[4] = {
                {x, chunk->heights[near_left], z},
                {x + step, chunk->heights[near_right], z},
                {x, chunk->heights[far_left], z + step},
                {x + step, chunk->heights[far_right], z + step},
            };
            Vector3 normal = {
                ((points[0].y + points[2].y) -
                 (points[1].y + points[3].y)) * 0.5f,
                step,
                ((points[0].y + points[1].y) -
                 (points[2].y + points[3].y)) * 0.5f,
            };
            normal = Vector3Normalize(normal);
            Color color = OpenWorldTerrainColor(
                &stream->manifest, x + step * 0.5f, z + step * 0.5f,
                normal);
            int32_t order[6] = {0, 3, 1, 0, 2, 3};
            for (int32_t vertex = 0; vertex < 6; ++vertex) {
                Vector3 point = points[order[vertex]];
                TerrainMeshWriteVertexWithNormal(
                    &writer, point, normal, color);
            }
        }
    }
    if (writer.cursor != vertex_count) {
        UnloadMesh(writer.mesh);
        return (Model){0};
    }
    UploadMesh(&writer.mesh, false);
    Model model = LoadModelFromMesh(writer.mesh);
    ReleaseUploadedModelCpuData(&model);
    ApplyWorldShader(&model);
    return model;
}

static void OpenWorldSyncRenderChunks(const CcWorldStream *stream)
{
    if (stream == NULL) return;
    for (int32_t i = 0; i < CC_WORLD_STREAM_CAPACITY; ++i) {
        OpenWorldRenderChunk *render = &open_world_render_chunks[i];
        if (!render->ready) continue;
        const CcWorldChunk *chunk = CcWorldStreamChunkAt(
            stream, render->x, render->z);
        if (OpenWorldRenderChunkMatches(render, chunk)) continue;
        UnloadModel(render->model);
        *render = (OpenWorldRenderChunk){0};
    }

    for (int32_t built = 0; built < CC_WORLD_STREAM_CAPACITY; ++built) {
        const CcWorldChunk *candidate = NULL;
        int32_t nearest_distance = INT32_MAX;
        for (int32_t i = 0; i < CC_WORLD_STREAM_CAPACITY; ++i) {
            const CcWorldChunk *chunk = &stream->chunks[i];
            if (chunk->state != CC_WORLD_CHUNK_READY) continue;
            bool found = false;
            for (int32_t render_index = 0;
                 render_index < CC_WORLD_STREAM_CAPACITY; ++render_index) {
                if (OpenWorldRenderChunkMatches(
                        &open_world_render_chunks[render_index], chunk)) {
                    found = true;
                    break;
                }
            }
            if (found) continue;
            int32_t distance = abs(chunk->x - stream->focus_chunk_x) +
                               abs(chunk->z - stream->focus_chunk_z);
            if (distance >= nearest_distance) continue;
            candidate = chunk;
            nearest_distance = distance;
        }
        if (candidate == NULL) break;
        OpenWorldRenderChunk *slot = NULL;
        for (int32_t i = 0; i < CC_WORLD_STREAM_CAPACITY; ++i) {
            if (!open_world_render_chunks[i].ready) {
                slot = &open_world_render_chunks[i];
                break;
            }
        }
        if (slot == NULL) break;
        Model model = OpenWorldBuildChunkModel(stream, candidate);
        if (!IsModelValid(model)) break;
        *slot = (OpenWorldRenderChunk){
            .x = candidate->x,
            .z = candidate->z,
            .generation = candidate->generation,
            .model = model,
            .ready = true,
        };
    }
}
#endif

/* One local landscape, rebuilt on a world-aligned grid as the caravan moves. */
static Model storybook_ground = {0};
static uint32_t storybook_seed = 0;
static uint32_t storybook_generator = 0;
static Vector2 storybook_center = {0};
static bool storybook_ground_ready = false;
enum { STORYBOOK_FOREST_CELL_COUNT = 121 };
typedef struct StorybookForestCell {
    TreeSceneryBatchCache batch;
    Vector3 rocks[16];
    int rock_count;
    int32_t x;
    int32_t z;
    bool occupied;
} StorybookForestCell;
static StorybookForestCell storybook_forest_cells[STORYBOOK_FOREST_CELL_COUNT];
static uint32_t storybook_forest_seed;
static uint32_t storybook_forest_generator;
static uint64_t storybook_forest_uploads;

static void ClearStorybookForestCell(StorybookForestCell *cell)
{
    if (IsModelValid(cell->batch.structure)) UnloadModel(cell->batch.structure);
    if (IsModelValid(cell->batch.foliage)) UnloadModel(cell->batch.foliage);
    *cell = (StorybookForestCell){0};
}

static void ClearStorybookForest(void)
{
    for (int32_t i = 0; i < STORYBOOK_FOREST_CELL_COUNT; ++i) {
        ClearStorybookForestCell(&storybook_forest_cells[i]);
    }
}

static void OpenWorldRenderCacheClear(void)
{
    ClearStorybookForest();
    if (storybook_ground_ready) UnloadModel(storybook_ground);
    storybook_ground = (Model){0};
    storybook_ground_ready = false;
}

static Color StorybookMeadowColor(float x, float z, Vector3 normal)
{
    float meadow = 0.5f + 0.5f * sinf(x * 0.047f +
        sinf(z * 0.064f) * 2.0f);
    Color color = BlendColor((Color){47, 83, 59, 255},
                             (Color){115, 130, 75, 255}, meadow);
    return BlendColor(color, WORLD_STONE,
                       fmaxf(0.0f, 0.93f - normal.y) * 3.0f);
}

static void StorybookMeadowTuft(TerrainMeshWriter *writer, Vector3 base,
                                 float scale, float angle, Color tip)
{
    Color root = BlendColor(tip, WORLD_GRASS_SHADOW, 0.72f);
    for (int blade = 0; blade < 3; ++blade) {
        float direction = angle + (float)blade * 2.1f;
        float width = scale * 0.22f;
        float height = scale * (blade == 1 ? 0.95f : 0.64f);
        Vector3 points[3] = {
            {base.x - cosf(direction) * width, base.y,
             base.z - sinf(direction) * width},
            {base.x + cosf(direction) * width, base.y,
             base.z + sinf(direction) * width},
            {base.x + sinf(direction) * scale * 0.26f, base.y + height,
             base.z - cosf(direction) * scale * 0.26f},
        };
        static const int order[6] = {0, 1, 2, 2, 1, 0};
        for (int vertex = 0; vertex < 6; ++vertex) {
            int point = order[vertex];
            TerrainMeshWriteVertexWithNormal(writer, points[point],
                (Vector3){0.0f, 1.0f, 0.0f}, point == 2 ? tip : root);
        }
    }
}

static void DrawStorybookGround(const CcWorldManifest *manifest, Vector3 focus)
{
    Vector2 center = {floorf(focus.x / 32.0f) * 32.0f,
                      floorf(focus.z / 32.0f) * 32.0f};
    if (!storybook_ground_ready || storybook_seed != manifest->world_seed ||
        storybook_generator != manifest->generator_version ||
        center.x != storybook_center.x || center.y != storybook_center.y) {
        OpenWorldRenderCacheClear();
        enum { CELLS = 80, COVER_CELLS = CELLS / 2, TUFT_VERTICES = 18 };
        const float step = 4.0f;
        TerrainMeshWriter writer;
        if (!TerrainMeshWriterAllocate(&writer, CELLS * CELLS * 6 +
            COVER_CELLS * COVER_CELLS * TUFT_VERTICES)) return;
        /* A one-cell border gives shared vertices the same slope at cache edges. */
        float heights[CELLS + 3][CELLS + 3];
        Vector3 normals[CELLS + 1][CELLS + 1];
        Color colors[CELLS + 1][CELLS + 1];
        for (int row = 0; row < CELLS + 3; ++row) {
            for (int col = 0; col < CELLS + 3; ++col) {
                float x = center.x + (float)(col - 1 - CELLS / 2) * step;
                float z = center.y + (float)(row - 1 - CELLS / 2) * step;
                heights[row][col] = CcWorldTerrainHeight(manifest, x, z);
            }
        }
        for (int row = 0; row <= CELLS; ++row) {
            for (int col = 0; col <= CELLS; ++col) {
                float x = center.x + (float)(col - CELLS / 2) * step;
                float z = center.y + (float)(row - CELLS / 2) * step;
                normals[row][col] = Vector3Normalize((Vector3){
                    heights[row + 1][col] - heights[row + 1][col + 2],
                    2.0f * step,
                    heights[row][col + 1] - heights[row + 2][col + 1]});
                colors[row][col] = StorybookMeadowColor(x, z, normals[row][col]);
            }
        }
        for (int row = 0; row < CELLS; ++row) {
            for (int col = 0; col < CELLS; ++col) {
                float x = center.x + (float)(col - CELLS / 2) * step;
                float z = center.y + (float)(row - CELLS / 2) * step;
                Vector3 points[4] = {
                    {x, heights[row + 1][col + 1] - 0.10f, z},
                    {x + step, heights[row + 1][col + 2] - 0.10f, z},
                    {x, heights[row + 2][col + 1] - 0.10f, z + step},
                    {x + step, heights[row + 2][col + 2] - 0.10f, z + step},
                };
                static const int order[6] = {0, 3, 1, 0, 2, 3};
                for (int vertex = 0; vertex < 6; ++vertex) {
                    int corner = order[vertex];
                    int vr = row + corner / 2;
                    int vc = col + corner % 2;
                    TerrainMeshWriteVertexWithNormal(
                        &writer, points[corner], normals[vr][vc], colors[vr][vc]);
                }
            }
        }
        /* Stable world cells keep grass in place as the terrain cache moves. */
        for (int row = 0; row < COVER_CELLS; ++row) {
            for (int col = 0; col < COVER_CELLS; ++col) {
                int32_t gx = (int32_t)(center.x / 8.0f) + col - COVER_CELLS / 2;
                int32_t gz = (int32_t)(center.y / 8.0f) + row - COVER_CELLS / 2;
                uint32_t scatter = TerrainMix(manifest->world_seed ^
                    (uint32_t)gx * UINT32_C(0x9e3779b9) ^
                    (uint32_t)gz * UINT32_C(0x85ebca6b));
                if ((scatter & 3U) == 0U) continue;
                float x = ((float)gx + 0.2f +
                    (float)((scatter >> 2U) & 255U) / 425.0f) * 8.0f;
                float z = ((float)gz + 0.2f +
                    (float)((scatter >> 10U) & 255U) / 425.0f) * 8.0f;
                if (CcWorldSurfaceAt(manifest, x, z) !=
                    CC_WORLD_SURFACE_WILDERNESS) continue;
                /* Seat each root on the same triangle that draws the meadow. */
                float grid_x = (x - center.x) / step + (float)(CELLS / 2);
                float grid_z = (z - center.y) / step + (float)(CELLS / 2);
                int cell_x = (int)floorf(grid_x);
                int cell_z = (int)floorf(grid_z);
                float u = grid_x - (float)cell_x;
                float v = grid_z - (float)cell_z;
                float h00 = heights[cell_z + 1][cell_x + 1];
                float h10 = heights[cell_z + 1][cell_x + 2];
                float h01 = heights[cell_z + 2][cell_x + 1];
                float h11 = heights[cell_z + 2][cell_x + 2];
                float ground = u >= v ?
                    h00 + u * (h10 - h00) + v * (h11 - h10) :
                    h00 + u * (h11 - h01) + v * (h01 - h00);
                float scale = 0.65f + (float)((scatter >> 18U) & 63U) / 90.0f;
                float angle = (float)((scatter >> 24U) & 255U) * (2.0f * PI / 255.0f);
                Color tip = BlendColor(StorybookMeadowColor(x, z,
                    (Vector3){0.0f, 1.0f, 0.0f}), WORLD_CROP_LIGHT, 0.28f);
                StorybookMeadowTuft(&writer,
                    (Vector3){x, ground - 0.10f, z},
                    scale, angle, tip);
            }
        }
        writer.mesh.vertexCount = writer.cursor;
        writer.mesh.triangleCount = writer.cursor / 3;
        UploadMesh(&writer.mesh, false);
        storybook_ground = LoadModelFromMesh(writer.mesh);
        ReleaseUploadedModelCpuData(&storybook_ground);
        ApplyWorldShader(&storybook_ground);
        storybook_ground_ready = IsModelValid(storybook_ground);
        storybook_seed = manifest->world_seed;
        storybook_generator = manifest->generator_version;
        storybook_center = center;
    }
    if (storybook_ground_ready) {
        SetWorldTerrainSurface(true);
        DrawModel(storybook_ground, (Vector3){0}, 1.0f, WHITE);
        SetWorldTerrainSurface(false);
    }
}

bool CcLocalOpenWorldUsesKingdomMarkers(float camera_weight)
{
    camera_weight = fmaxf(0.0f, fminf(1.0f, camera_weight));
    camera_weight = camera_weight * camera_weight *
                    (3.0f - 2.0f * camera_weight);
    return camera_weight > CC_LOCAL_OPEN_WORLD_KINGDOM_VIEW_START;
}

static void DrawOpenWorldSettlement(const CcSim *sim,
                                    const CcWorldSettlementPlacement *place,
                                    Vector3 focus, bool kingdom_view,
                                    bool distant_view)
{
    float dx = place->center.x - focus.x;
    float dz = place->center.z - focus.z;
    if (!kingdom_view && dx * dx + dz * dz > 92.0f * 92.0f) return;
    const CcSettlement *settlement = CcSimSettlement(
        sim, place->settlement_id);
    if (settlement == NULL) return;
    const CcLocalPlaceProfile *profile =
        CcLocalPlaceProfileForSettlement(settlement);
    if (profile == NULL) return;
    CcId visible_kingdom_id = settlement->kingdom_id;
    if (kingdom_view) {
        CcId recorded_kingdom_id =
            CcRoadBookSettlementKingdom(sim, settlement->id);
        if (recorded_kingdom_id != 0U) {
            visible_kingdom_id = recorded_kingdom_id;
        }
    }
    Color kingdom = KingdomColor3D(sim, visible_kingdom_id);
    if (kingdom_view) {
        float ground = CcWorldTerrainHeight(
            active_world_stream != NULL ? &active_world_stream->manifest : NULL,
            place->center.x, place->center.z);
        float scale = settlement->size == CC_SETTLEMENT_CAPITAL_SIZE ? 1.25f :
                      settlement->size == CC_SETTLEMENT_TOWN ? 1.05f : 0.88f;
        Vector3 center = {place->center.x, ground, place->center.z};
        DrawCylinder((Vector3){center.x, center.y + 0.06f, center.z},
                     7.8f * scale, 7.8f * scale, 0.12f, 24,
                     Fade(WORLD_EARTH_LIGHT, 0.92f));
        DrawCylinderWires((Vector3){center.x, center.y + 0.14f, center.z},
                          8.2f * scale, 8.2f * scale, 0.16f, 24,
                          Fade(kingdom, 0.82f));
        DrawBox((Vector3){center.x - 2.25f * scale,
                          center.y + 1.25f * scale, center.z},
                (Vector3){1.25f * scale, 2.5f * scale, 1.45f * scale},
                BlendColor(WORLD_STONE, kingdom, 0.16f));
        DrawBox((Vector3){center.x + 2.25f * scale,
                          center.y + 1.25f * scale, center.z},
                (Vector3){1.25f * scale, 2.5f * scale, 1.45f * scale},
                BlendColor(WORLD_STONE, kingdom, 0.16f));
        DrawBox((Vector3){center.x, center.y + 2.32f * scale, center.z},
                (Vector3){5.7f * scale, 0.42f * scale, 1.55f * scale},
                kingdom);
        DrawBox((Vector3){center.x, center.y + 1.12f * scale, center.z},
                (Vector3){1.95f * scale, 2.25f * scale, 0.24f * scale},
                WORLD_ROAD_SHADOW);
        DrawHeraldryBanner3D(
            CcHeraldryForKingdom(sim, visible_kingdom_id), center,
            0.0f, 1.48f * scale, 0.0f);
        return;
    }
    float rotation = place->entrance_heading_yaw - 0.5f * PI;
    CcSettlementFunction previous_function = active_place_function;
    active_place_function = profile->function;
    bool previous_distant_view = distant_building_view;
    distant_building_view = distant_view;

    rlPushMatrix();
    rlTranslatef(place->center.x, place->plateau_height + 0.03f,
                 place->center.z);
    rlRotatef(rotation * RAD2DEG, 0.0f, 1.0f, 0.0f);
    rlScalef(place->profile_scale, place->profile_scale,
             place->profile_scale);
    rlTranslatef(-48.0f, 0.0f, -36.0f);

    for (int32_t road_index = 0;
         road_index < CC_LOCAL_PLACE_ROAD_COUNT; ++road_index) {
        const CcLocalPlaceRoad *road = &profile->road[road_index];
        DrawBox((Vector3){road->x + road->width * 0.5f, 0.045f,
                          road->z + road->depth * 0.5f},
                (Vector3){road->width, 0.09f, road->depth},
                TerrainBrightRoadColor(
                    road->surface, (float)settlement->prosperity / 100.0f));
    }
    for (int32_t road_index = 0;
         road_index < CC_LOCAL_CARRIAGE_ROUTE_COUNT; ++road_index) {
        const CcLocalPlaceRoad *road = &profile->carriage_route[road_index];
        DrawBox((Vector3){road->x + road->width * 0.5f, 0.055f,
                          road->z + road->depth * 0.5f},
                (Vector3){road->width, 0.11f, road->depth},
                TerrainPlaceRoadColor(road->surface));
    }

    for (int32_t building_index = 0;
         building_index < profile->building_count; ++building_index) {
        const CcLocalPlaceBuilding *source =
            &profile->building[building_index];
        WorldBuilding building = {
            .footprint = {source->x, source->z,
                          source->width, source->depth},
            .height = source->height,
            .style = (int32_t)source->style,
            .door = source->door,
        };
        DrawSettlementBuilding(
            &building, BuildingWallColor(building.style, profile),
            BuildingRoofColor(building.style, kingdom, profile),
            profile, building_index);
    }

    for (int32_t landmark_index = 0;
         landmark_index < CC_LOCAL_PLACE_LANDMARK_COUNT; ++landmark_index) {
        DrawPlaceLandmark(&profile->landmark[landmark_index], kingdom);
    }

    for (int32_t structure_index = 0;
         structure_index < profile->compound_structure_count;
         ++structure_index) {
        const CcLocalPlaceCompoundStructure *structure =
            &profile->compound_structure[structure_index];
        float center_x = structure->x + structure->width * 0.5f;
        float center_z = structure->z + structure->depth * 0.5f;
        Color stone = BlendColor(
            WORLD_STONE, kingdom,
            structure->kind == CC_LOCAL_COMPOUND_WALL ? 0.08f : 0.18f);
        if (structure->kind == CC_LOCAL_COMPOUND_TOWER ||
            structure->kind == CC_LOCAL_COMPOUND_SILO) {
            float radius = fminf(structure->width, structure->depth) * 0.5f;
            DrawCylinder((Vector3){center_x, 0.0f, center_z},
                         radius * 0.92f, radius,
                         structure->height, 12, stone);
            DrawCylinder((Vector3){center_x, structure->height, center_z},
                         0.08f, radius * 1.12f,
                         fmaxf(0.35f, structure->height * 0.12f),
                         12, BuildingRoofColor(1, kingdom, profile));
        } else {
            DrawBox((Vector3){center_x, structure->height * 0.5f, center_z},
                    (Vector3){structure->width, structure->height,
                              structure->depth}, stone);
            if (structure->kind != CC_LOCAL_COMPOUND_WALL) {
                DrawBox((Vector3){center_x, structure->height + 0.16f,
                                  center_z},
                        (Vector3){structure->width + 0.22f, 0.32f,
                                  structure->depth + 0.22f},
                        BuildingRoofColor(1, kingdom, profile));
            }
        }
    }

    DrawBox((Vector3){CC_LOCAL_NOTICE_X, 0.90f, CC_LOCAL_NOTICE_Z},
            (Vector3){0.18f, 1.80f, 0.18f}, WORLD_WOOD_SHADOW);
    DrawBox((Vector3){CC_LOCAL_NOTICE_X, 1.45f,
                      CC_LOCAL_NOTICE_Z + 0.05f},
            (Vector3){1.80f, 0.82f, 0.12f},
            PlaceIdentityAccent(profile, kingdom));
    rlPopMatrix();
    active_place_function = previous_function;
    distant_building_view = previous_distant_view;
}

static bool OpenWorldSettlementRevealed(const CcSim *sim, CcId settlement_id)
{
    return CcSimPlayerKnowsSettlement(sim, settlement_id);
}

static void DrawOpenWorldFogEdge(const CcWorldManifest *manifest,
                                 const CcWorldRoutePlacement *route,
                                 float amount, uint32_t seed)
{
    CcWorldPoint point = CcWorldRoutePoint(route, amount);
    float ground = CcWorldTerrainHeight(manifest, point.x, point.z);
    for (int32_t i = 0; i < 7; ++i) {
        float angle = (float)i * 2.39996323f + (float)(seed & 255U) * 0.01f;
        float radius = 2.2f + (float)(i % 3) * 1.45f;
        Vector3 center = {
            point.x + cosf(angle) * radius,
            ground + 0.65f + (float)(i & 1) * 0.42f,
            point.z + sinf(angle) * radius,
        };
        DrawSmallSphere(center, 2.5f + (float)(i % 2) * 0.75f,
                        Fade(WORLD_VOID, 0.82f));
    }
    DrawCylinderWires((Vector3){point.x, ground + 0.12f, point.z},
                      5.5f, 5.5f, 0.18f, 24, Fade(WORLD_GOLD, 0.55f));
}

static void DrawOpenWorldRibbonSegment(const CcWorldManifest *manifest,
                                       CcWorldPoint first,
                                       CcWorldPoint second, float width,
                                       float lift, Color color,
                                       bool extend_first, bool extend_second)
{
    float dx = second.x - first.x;
    float dz = second.z - first.z;
    float length = sqrtf(dx * dx + dz * dz);
    if (manifest == NULL || length < 0.01f) return;
    float extension = fminf(3.6f, width * 0.16f);
    if (extend_first) {
        first.x -= dx / length * extension;
        first.z -= dz / length * extension;
    }
    if (extend_second) {
        second.x += dx / length * extension;
        second.z += dz / length * extension;
    }
    float side_x = -dz / length * width * 0.5f;
    float side_z = dx / length * width * 0.5f;
    Vector3 points[4] = {
        {first.x + side_x,
         CcWorldTerrainHeight(manifest, first.x + side_x,
                              first.z + side_z) + lift,
         first.z + side_z},
        {first.x - side_x,
         CcWorldTerrainHeight(manifest, first.x - side_x,
                              first.z - side_z) + lift,
         first.z - side_z},
        {second.x - side_x,
         CcWorldTerrainHeight(manifest, second.x - side_x,
                              second.z - side_z) + lift,
         second.z - side_z},
        {second.x + side_x,
         CcWorldTerrainHeight(manifest, second.x + side_x,
                              second.z + side_z) + lift,
         second.z + side_z},
    };
    Vector3 normal = Vector3Normalize(Vector3CrossProduct(
        Vector3Subtract(points[2], points[0]), Vector3Subtract(points[1], points[0])));
    rlNormal3f(normal.x, normal.y, normal.z);
    DrawTriangle3D(points[0], points[2], points[1], color);
    normal = Vector3Normalize(Vector3CrossProduct(
        Vector3Subtract(points[3], points[0]), Vector3Subtract(points[2], points[0])));
    rlNormal3f(normal.x, normal.y, normal.z);
    DrawTriangle3D(points[0], points[3], points[2], color);
}

static bool OpenWorldPointClearsSettlements(
    const CcWorldManifest *manifest, CcWorldPoint point, float clearance)
{
    if (manifest == NULL) return false;
    for (int32_t i = 0; i < manifest->settlement_count; ++i) {
        const CcWorldSettlementPlacement *settlement =
            &manifest->settlements[i];
        float dx = point.x - settlement->center.x;
        float dz = point.z - settlement->center.z;
        float minimum = settlement->radius + clearance;
        if (dx * dx + dz * dz < minimum * minimum) return false;
    }
    return true;
}

static void DrawOpenWorldGateConnectors(const CcSim *sim,
                                        const CcWorldManifest *manifest,
                                        bool kingdom_view)
{
    if (sim == NULL || manifest == NULL) return;
    for (int32_t i = 0; i < manifest->settlement_count; ++i) {
        const CcWorldSettlementPlacement *settlement =
            &manifest->settlements[i];
        if (!OpenWorldSettlementRevealed(
                sim, settlement->settlement_id)) continue;
        CcWorldPoint gate = kingdom_view ? settlement->center :
                                           settlement->gate;
        DrawOpenWorldRibbonSegment(
            manifest, gate, settlement->junction,
            11.0f, 0.04f, Fade(WORLD_EARTH_LIGHT, 0.96f), true, true);
        DrawOpenWorldRibbonSegment(
            manifest, gate, settlement->junction,
            7.5f, 0.08f, Fade(WORLD_GRASS_LIGHT, 0.98f), true, true);
        DrawOpenWorldRibbonSegment(
            manifest, gate, settlement->junction,
            5.6f, 0.13f, WORLD_ROAD_LIGHT, true, true);
    }
}

static bool OpenWorldReadRouteView(
    const CcSim *sim, const CcWorldRoutePlacement *placement,
    const CcLocalWorldCarriageState *carriage,
    CcRoadBookRouteView *view)
{
    if (placement == NULL) return false;
    if (carriage != NULL && carriage->visible &&
        carriage->route_id == placement->route_id) {
        return CcRoadBookReadRouteAtCarriage(
            sim, placement->route_id, carriage->route_amount,
            CcWorldRouteLength(placement), view);
    }
    return CcRoadBookReadRoute(sim, placement->route_id, view);
}

static bool OpenWorldRouteVisibleBefore(
    const CcRoadBookRouteView *view, float amount)
{
    return amount <= 0.00001f ||
           CcRoadBookShowsRouteAmount(view, amount - 0.00001f);
}

static bool OpenWorldRouteVisibleAfter(
    const CcRoadBookRouteView *view, float amount)
{
    return amount >= 0.99999f ||
           CcRoadBookShowsRouteAmount(view, amount + 0.00001f);
}

static void DrawOpenWorldActiveConnector(
    const CcWorldManifest *manifest,
    const CcWorldRoutePlacement *placement,
    const CcRoadBookRouteView *visibility, int32_t sample, Color road)
{
    float segment_from = CcWorldRouteSampleAmount(placement, sample);
    float segment_to = CcWorldRouteSampleAmount(placement, sample + 1);
    CcRoadBookRouteSpan spans[2];
    int32_t span_count = CcRoadBookVisibleRouteSpans(
        visibility, segment_from, segment_to, spans);
    for (int32_t span_index = 0;
         span_index < span_count; ++span_index) {
        CcWorldPoint first = CcWorldRoutePoint(
            placement, spans[span_index].from_amount);
        CcWorldPoint second = CcWorldRoutePoint(
            placement, spans[span_index].to_amount);
        bool extend_first = OpenWorldRouteVisibleBefore(
            visibility, spans[span_index].from_amount);
        bool extend_second = OpenWorldRouteVisibleAfter(
            visibility, spans[span_index].to_amount);
        DrawOpenWorldRibbonSegment(
            manifest, first, second, 11.0f, 0.04f,
            Fade(WORLD_EARTH_LIGHT, 0.96f),
            extend_first, extend_second);
        DrawOpenWorldRibbonSegment(
            manifest, first, second, 7.5f, 0.08f,
            Fade(WORLD_GRASS_LIGHT, 0.98f),
            extend_first, extend_second);
        DrawOpenWorldRibbonSegment(
            manifest, first, second, 5.6f, 0.13f, road,
            extend_first, extend_second);
    }
}

static void DrawOpenWorldRouteNetwork(const CcSim *sim,
                                      const CcWorldManifest *manifest,
                                      const CcLocalWorldCarriageState *carriage,
                                      bool kingdom_view)
{
    if (sim == NULL || manifest == NULL) return;
    DrawOpenWorldGateConnectors(sim, manifest, kingdom_view);
    for (int32_t route_index = 0;
         route_index < manifest->route_count; ++route_index) {
        const CcWorldRoutePlacement *placement =
            &manifest->routes[route_index];
        const CcRoute *route = CcSimRoute(sim, placement->route_id);
        CcRoadBookRouteView visibility = {0};
        if (route == NULL ||
            !OpenWorldReadRouteView(
                sim, placement, carriage, &visibility)) continue;
        Color road = visibility.recorded_closed ? WORLD_DANGER :
                     visibility.recorded_smuggler_route ? WORLD_VIOLET :
                                                               WORLD_ROAD_LIGHT;
        for (int32_t sample = CC_WORLD_ROUTE_FROM_JUNCTION_SAMPLE;
             sample < CC_WORLD_ROUTE_TO_JUNCTION_SAMPLE; ++sample) {
            if (visibility.recorded_smuggler_route &&
                (sample & 1) != 0) continue;
            float segment_from =
                CcWorldRouteSampleAmount(placement, sample);
            float segment_to =
                CcWorldRouteSampleAmount(placement, sample + 1);
            CcRoadBookRouteSpan spans[2];
            int32_t span_count = CcRoadBookVisibleRouteSpans(
                &visibility, segment_from, segment_to, spans);
            if (span_count <= 0) continue;
            float variation = 1.0f +
                0.08f * sinf((float)sample * 1.7f +
                              (float)(placement->seed & 31U));
            bool corridor = sample >=
                                CC_WORLD_ROUTE_FROM_CORRIDOR_SAMPLE &&
                            sample < CC_WORLD_ROUTE_TO_CORRIDOR_SAMPLE;
            for (int32_t span_index = 0;
                 span_index < span_count; ++span_index) {
                CcWorldPoint first = CcWorldRoutePoint(
                    placement, spans[span_index].from_amount);
                CcWorldPoint second = CcWorldRoutePoint(
                    placement, spans[span_index].to_amount);
                bool extend_first = OpenWorldRouteVisibleBefore(
                    &visibility, spans[span_index].from_amount);
                bool extend_second = OpenWorldRouteVisibleAfter(
                    &visibility, spans[span_index].to_amount);
                if (corridor) {
                    DrawOpenWorldRibbonSegment(
                        manifest, first, second, 30.0f * variation, 0.04f,
                        Fade(WORLD_EARTH_LIGHT, 0.96f),
                        extend_first, extend_second);
                    DrawOpenWorldRibbonSegment(
                        manifest, first, second, 22.0f * variation, 0.08f,
                        Fade(WORLD_GRASS_LIGHT, 0.98f),
                        extend_first, extend_second);
                } else {
                    DrawOpenWorldRibbonSegment(
                        manifest, first, second, 9.0f, 0.08f,
                        Fade(WORLD_GRASS_LIGHT, 0.98f),
                        extend_first, extend_second);
                }
                DrawOpenWorldRibbonSegment(
                    manifest, first, second,
                    (visibility.charted ? 6.2f : 5.2f) * variation,
                    0.13f, road, extend_first, extend_second);
            }

            float midpoint = 0.5f * (segment_from + segment_to);
            CcWorldPoint first = placement->samples[sample];
            CcWorldPoint second = placement->samples[sample + 1];
            Vector3 along = {second.x - first.x, 0.0f,
                             second.z - first.z};
            float length = sqrtf(along.x * along.x + along.z * along.z);
            if (corridor && (sample & 1) == 0 && length > 0.01f &&
                CcRoadBookShowsRouteAmount(&visibility, midpoint)) {
                float inverse = 1.0f / length;
                Vector3 side = {-along.z * inverse, 0.0f,
                                 along.x * inverse};
                float side_amount = (sample & 2) == 0 ? 12.5f : -12.5f;
                Vector3 center = {
                    (first.x + second.x) * 0.5f,
                    (CcWorldTerrainHeight(manifest, first.x, first.z) +
                     CcWorldTerrainHeight(manifest, second.x, second.z)) *
                        0.5f + 0.08f,
                    (first.z + second.z) * 0.5f,
                };
                Vector3 tree = Vector3Add(
                    center, Vector3Scale(side, side_amount));
                if (!OpenWorldPointClearsSettlements(
                        manifest, (CcWorldPoint){tree.x, tree.z}, 2.5f)) {
                    continue;
                }
                tree.y = CcWorldTerrainHeight(manifest, tree.x, tree.z);
                DrawBox((Vector3){tree.x, tree.y + 1.05f, tree.z},
                        (Vector3){0.34f, 2.1f, 0.34f}, WORLD_WOOD_SHADOW);
                DrawSmallSphere((Vector3){tree.x, tree.y + 2.85f, tree.z},
                                2.25f, WORLD_FOLIAGE_LIGHT);
            }
        }
        bool active_route = carriage != NULL && carriage->visible &&
            carriage->route_id == placement->route_id;
        if (active_route && !OpenWorldSettlementRevealed(
                sim, placement->from_id)) {
            DrawOpenWorldActiveConnector(
                manifest, placement, &visibility, 0, road);
        }
        if (active_route && !OpenWorldSettlementRevealed(
                sim, placement->to_id)) {
            DrawOpenWorldActiveConnector(
                manifest, placement, &visibility,
                CC_WORLD_ROUTE_TO_JUNCTION_SAMPLE, road);
        }
        if (!visibility.charted &&
            !CcRoadBookShowsWholeRoute(&visibility)) {
            if (visibility.from_reveal > 0.0f) {
                DrawOpenWorldFogEdge(manifest, placement,
                                     visibility.from_reveal,
                                     placement->seed);
            }
            if (visibility.to_reveal > 0.0f) {
                DrawOpenWorldFogEdge(manifest, placement,
                                     1.0f - visibility.to_reveal,
                                     placement->seed ^ UINT32_C(0x5bd1e995));
            }
        }
    }
}

static void DrawOpenWorldRoadSiteBlocker(
    const CcWorldManifest *manifest,
    const CcWorldRoadSitePlacement *placement)
{
    if (manifest == NULL || placement == NULL) return;
    float heading = placement->blocker_heading_yaw;
    Vector3 across = {cosf(heading), 0.0f, -sinf(heading)};
    float ground = CcWorldTerrainHeight(
        manifest, placement->blocker_position.x,
        placement->blocker_position.z);
    Vector3 center = {placement->blocker_position.x, ground,
                      placement->blocker_position.z};
    if (placement->blocker == CC_ROAD_SITE_BLOCKER_TREE) {
        Vector3 first = Vector3Add(
            center, Vector3Scale(across, -3.2f));
        Vector3 second = Vector3Add(
            center, Vector3Scale(across, 3.2f));
        first.y += 0.42f;
        second.y += 0.42f;
        DrawCylinderEx(first, second, 0.46f, 0.34f, 8,
                       WORLD_WOOD_SHADOW);
        Vector3 crown = Vector3Add(
            second, Vector3Scale(across, 0.40f));
        DrawSmallSphere(Vector3Add(crown, (Vector3){0.0f, 0.35f, 0.0f}),
                        1.45f, WORLD_FOLIAGE_LIGHT);
        DrawSmallSphere(Vector3Add(crown, (Vector3){0.6f, 0.08f, 0.4f}),
                        1.05f, WORLD_FOLIAGE_SHADOW);
    } else {
        for (int32_t stone = -2; stone <= 2; ++stone) {
            Vector3 rock = Vector3Add(
                center, Vector3Scale(across, (float)stone * 1.15f));
            float radius = 0.72f + 0.12f * (float)((stone + 2) & 1);
            rock.y += radius * 0.62f;
            DrawSmallSphere(rock, radius,
                            stone == 0 ? WORLD_STONE_LIGHT : WORLD_STONE);
        }
    }
}

static void DrawOpenWorldRoadSites(
    const CcSim *sim, const CcWorldManifest *manifest,
    const CcLocalWorldCarriageState *carriage, Vector3 focus,
    bool kingdom_view)
{
    if (sim == NULL || manifest == NULL) return;
    for (int32_t i = 0; i < manifest->road_site_count; ++i) {
        const CcWorldRoadSitePlacement *placement =
            &manifest->road_sites[i];
        const CcRoadSite *site = CcSimRoadSite(
            sim, placement->road_site_id);
        const CcWorldRoutePlacement *route =
            CcWorldRoutePlacementForId(manifest, placement->route_id);
        CcRoadBookRouteView visibility = {0};
        if (site == NULL || route == NULL ||
            !OpenWorldReadRouteView(
                sim, route, carriage, &visibility) ||
            !CcRoadBookShowsRouteAmount(
                &visibility, CcWorldRouteJourneyAmount(route, route->from_id,
                    (float)site->progress_milli / 1000.0f))) {
            continue;
        }
        float dx = placement->junction.x - focus.x;
        float dz = placement->junction.z - focus.z;
        if (!kingdom_view && dx * dx + dz * dz > 112.0f * 112.0f)
            continue;
        float heading = placement->blocker_heading_yaw;
        CcWorldPoint end = {
            placement->blocker_position.x + sinf(heading) * 1.4f,
            placement->blocker_position.z + cosf(heading) * 1.4f,
        };
        Color verge = Fade(WORLD_GRASS_LIGHT, kingdom_view ? 0.82f : 0.96f);
        Color track = site->kind == CC_ROAD_SITE_ROAD_HOUSE ?
            Fade(WORLD_GOLD, kingdom_view ? 0.58f : 0.72f) :
            Fade(WORLD_ROAD_LIGHT, kingdom_view ? 0.74f : 0.90f);
        DrawOpenWorldRibbonSegment(
            manifest, placement->junction, placement->bend,
            7.2f, 0.07f, verge, true, true);
        DrawOpenWorldRibbonSegment(
            manifest, placement->bend, end,
            7.2f, 0.07f, verge, true, true);
        DrawOpenWorldRibbonSegment(
            manifest, placement->junction, placement->bend,
            4.2f, 0.12f, track, true, true);
        DrawOpenWorldRibbonSegment(
            manifest, placement->bend, end,
            4.2f, 0.12f, track, true, true);
        DrawOpenWorldRoadSiteBlocker(manifest, placement);
    }
}

static void DrawOpenWorldSites(const CcSim *sim,
                               const CcWorldManifest *manifest,
                               Vector3 focus, float clock, bool kingdom_view)
{
    for (int32_t i = 0; i < manifest->site_count; ++i) {
        const CcWorldSitePlacement *site = &manifest->sites[i];
        if (!CcSimPlayerKnowsRoadBookSite(
                sim, (CcRoadBookSiteKind)site->kind)) continue;
        float dx = site->position.x - focus.x;
        float dz = site->position.z - focus.z;
        if (!kingdom_view && dx * dx + dz * dz > 74.0f * 74.0f) continue;
        float ground = CcLocalTerrainHeightAt(site->position.x,
                                               site->position.z);
        float pulse = 0.06f + sinf(clock * 1.7f + (float)i) * 0.025f;
        Color color = i == CC_WORLD_SITE_UNDERROAD ? WORLD_GOLD :
                      i == CC_WORLD_SITE_GOBLIN_TRAIL ? WORLD_TEAL :
                                                            WORLD_VIOLET;
        DrawBox((Vector3){site->position.x - 1.25f, ground + 1.0f,
                          site->position.z},
                (Vector3){0.50f, 2.0f, 0.70f}, WORLD_STONE);
        DrawBox((Vector3){site->position.x + 1.25f, ground + 1.0f,
                          site->position.z},
                (Vector3){0.50f, 2.0f, 0.70f}, WORLD_STONE);
        DrawBox((Vector3){site->position.x, ground + 2.0f,
                          site->position.z},
                (Vector3){3.0f, 0.45f, 0.75f}, color);
        DrawSmallSphere((Vector3){site->position.x, ground + 2.65f,
                                  site->position.z},
                        0.16f + pulse, color);
    }
}

static void OpenWorldRevealedBounds(const CcSim *sim,
                                    const CcWorldManifest *manifest,
                                    const CcLocalWorldCarriageState *carriage,
                                    float *minimum_x, float *minimum_z,
                                    float *maximum_x, float *maximum_z)
{
    float left = FLT_MAX;
    float near = FLT_MAX;
    float right = -FLT_MAX;
    float far = -FLT_MAX;
    for (int32_t route_index = 0;
         sim != NULL && manifest != NULL &&
         route_index < manifest->route_count; ++route_index) {
        const CcWorldRoutePlacement *placement =
            &manifest->routes[route_index];
        const CcRoute *route = CcSimRoute(sim, placement->route_id);
        CcRoadBookRouteView visibility = {0};
        if (route == NULL ||
            !OpenWorldReadRouteView(
                sim, placement, carriage, &visibility)) continue;
        for (int32_t sample = 0;
             sample < CC_WORLD_ROUTE_SAMPLE_COUNT; ++sample) {
            float amount = CcWorldRouteSampleAmount(placement, sample);
            if (!CcRoadBookShowsRouteAmount(
                    &visibility, amount)) continue;
            CcWorldPoint point = placement->samples[sample];
            left = fminf(left, point.x);
            near = fminf(near, point.z);
            right = fmaxf(right, point.x);
            far = fmaxf(far, point.z);
        }
        if (visibility.from_reveal > 0.0f) {
            CcWorldPoint edge = CcWorldRoutePoint(
                placement, visibility.from_reveal);
            left = fminf(left, edge.x);
            near = fminf(near, edge.z);
            right = fmaxf(right, edge.x);
            far = fmaxf(far, edge.z);
        }
        if (visibility.to_reveal > 0.0f) {
            CcWorldPoint edge = CcWorldRoutePoint(
                placement, 1.0f - visibility.to_reveal);
            left = fminf(left, edge.x);
            near = fminf(near, edge.z);
            right = fmaxf(right, edge.x);
            far = fmaxf(far, edge.z);
        }
    }
    for (int32_t site_index = 0;
         sim != NULL && manifest != NULL &&
         site_index < manifest->road_site_count; ++site_index) {
        const CcWorldRoadSitePlacement *site_placement =
            &manifest->road_sites[site_index];
        const CcRoadSite *site = CcSimRoadSite(
            sim, site_placement->road_site_id);
        const CcWorldRoutePlacement *route =
            CcWorldRoutePlacementForId(
                manifest, site_placement->route_id);
        CcRoadBookRouteView visibility = {0};
        if (site == NULL || route == NULL ||
            !OpenWorldReadRouteView(
                sim, route, carriage, &visibility) ||
            !CcRoadBookShowsRouteAmount(
                &visibility,
                CcWorldRouteJourneyAmount(route, route->from_id,
                    (float)site->progress_milli / 1000.0f))) continue;
        CcWorldPoint edge = site_placement->destination;
        left = fminf(left, edge.x);
        near = fminf(near, edge.z);
        right = fmaxf(right, edge.x);
        far = fmaxf(far, edge.z);
    }
    const CcWorldSettlementPlacement *current = sim != NULL &&
        manifest != NULL ? CcWorldSettlementPlacementForId(
            manifest, sim->player.location_id) : NULL;
    if (left == FLT_MAX && current != NULL) {
        left = right = current->center.x;
        near = far = current->center.z;
    } else if (left == FLT_MAX) {
        left = near = -24.0f;
        right = far = 24.0f;
    }
    float margin = 24.0f;
    *minimum_x = left - margin;
    *minimum_z = near - margin;
    *maximum_x = right + margin;
    *maximum_z = far + margin;
}

static Camera3D StorybookCamera(const CcWorldManifest *manifest,
                                 const CcLocalWorldCarriageState *carriage)
{
    float wide = SmoothStep01(carriage->camera_weight);
    float heading = carriage->camera_heading_yaw;
    Vector3 forward = {sinf(heading), 0.0f, cosf(heading)};
    Vector3 right = {cosf(heading), 0.0f, -sinf(heading)};
    Vector3 focus = Vector3Add(carriage->position,
        Vector3Add(Vector3Scale(forward, 5.0f + wide * 12.0f),
                   (Vector3){0.0f, 1.8f, 0.0f}));
    Vector3 position = Vector3Add(carriage->position,
        Vector3Add(Vector3Scale(right, 19.0f + wide * 36.0f),
            Vector3Add(Vector3Scale(forward, -20.0f - wide * 39.0f),
                       (Vector3){0.0f, 10.0f + wide * 54.0f, 0.0f})));
    position.y = fmaxf(position.y,
        CcWorldTerrainHeight(manifest, position.x, position.z) + 5.5f);
    return (Camera3D){.position = position, .target = focus,
        .up = {0.0f, 1.0f, 0.0f}, .fovy = 43.0f,
        .projection = CAMERA_PERSPECTIVE};
}

static Vector2 StorybookRoadSide(const CcWorldRoutePlacement *route,
                                  float amount, float span)
{
    CcWorldPoint before = CcWorldRoutePoint(route, fmaxf(0.0f, amount - span));
    CcWorldPoint after = CcWorldRoutePoint(route, fminf(1.0f, amount + span));
    float length = fmaxf(0.001f, hypotf(after.x - before.x, after.z - before.z));
    return (Vector2){-(after.z - before.z) / length, (after.x - before.x) / length};
}

static void DrawStorybookStrip(CcWorldPoint first, CcWorldPoint second,
                                Vector2 first_side, Vector2 second_side,
                                float first_height, float second_height,
                                float width, float offset, float lift, Color color,
                                bool feather_edge)
{
    Vector3 points[4];
    Color colors[4];
    for (int corner = 0; corner < 4; ++corner) {
        CcWorldPoint end = corner < 2 ? first : second;
        Vector2 side = corner < 2 ? first_side : second_side;
        float across = offset + ((corner == 0 || corner == 3) ? width : -width) * 0.5f;
        float wear = 1.0f + sinf(end.x * 0.71f + end.z * 0.43f) * 0.035f +
                     sinf(end.z * 1.13f - end.x * 0.37f) * 0.018f;
        across *= wear;
        float x = end.x + side.x * across;
        float z = end.z + side.y * across;
        points[corner] = (Vector3){x,
            (corner < 2 ? first_height : second_height) + lift, z};
        bool outer = feather_edge && fabsf(across) > fabsf(offset * wear);
        colors[corner] = outer ? StorybookMeadowColor(x, z,
            (Vector3){0.0f, 1.0f, 0.0f}) :
            ShadeColor(color, 0.98f + 0.04f * sinf(end.x * 0.9f + end.z * 0.6f));
    }
    static const int order[6] = {0, 2, 1, 0, 3, 2};
    rlBegin(RL_TRIANGLES);
    for (int triangle = 0; triangle < 2; ++triangle) {
        Vector3 normal = Vector3Normalize(Vector3CrossProduct(
            Vector3Subtract(points[order[triangle * 3 + 1]], points[0]),
            Vector3Subtract(points[order[triangle * 3 + 2]], points[0])));
        rlNormal3f(normal.x, normal.y, normal.z);
        for (int vertex = 0; vertex < 3; ++vertex) {
            int corner = order[triangle * 3 + vertex];
            Color tint = colors[corner];
            rlColor4ub(tint.r, tint.g, tint.b, tint.a);
            rlVertex3f(points[corner].x, points[corner].y, points[corner].z);
        }
    }
    rlEnd();
}

static void DrawStorybookRoads(const CcSim *sim,
                                const CcWorldManifest *manifest,
                                const CcLocalWorldCarriageState *carriage)
{
    SetWorldTerrainSurface(true);
    for (int32_t index = 0; index < manifest->route_count; ++index) {
        const CcWorldRoutePlacement *route = &manifest->routes[index];
        CcRoadBookRouteView view = {0};
        if (!OpenWorldReadRouteView(sim, route, carriage, &view)) continue;
        float tangent_span = 0.6f / fmaxf(1.0f, CcWorldRouteLength(route));
        for (int32_t sample = 0; sample < CC_WORLD_ROUTE_SAMPLE_COUNT - 1;
             ++sample) {
            CcRoadBookRouteSpan spans[2];
            int32_t count = CcRoadBookVisibleRouteSpans(&view,
                CcWorldRouteSampleAmount(route, sample),
                CcWorldRouteSampleAmount(route, sample + 1), spans);
            for (int32_t span = 0; span < count; ++span) {
                CcWorldPoint start = CcWorldRoutePoint(route, spans[span].from_amount);
                CcWorldPoint end = CcWorldRoutePoint(route, spans[span].to_amount);
                float length = hypotf(end.x - start.x, end.z - start.z);
                int32_t steps = (int32_t)ceilf(length / 2.0f);
                for (int32_t step = 0; step < steps; ++step) {
                    float a = (float)step / (float)steps;
                    float b = (float)(step + 1) / (float)steps;
                    CcWorldPoint first = {start.x + (end.x - start.x) * a,
                                          start.z + (end.z - start.z) * a};
                    CcWorldPoint second = {start.x + (end.x - start.x) * b,
                                           start.z + (end.z - start.z) * b};
                    if (hypotf(first.x - carriage->position.x,
                               first.z - carriage->position.z) > 135.0f) continue;
                    float amount_span = spans[span].to_amount - spans[span].from_amount;
                    Vector2 first_side = StorybookRoadSide(route,
                        spans[span].from_amount + amount_span * a, tangent_span);
                    Vector2 second_side = StorybookRoadSide(route,
                        spans[span].from_amount + amount_span * b, tangent_span);
                    float first_height = CcWorldTerrainHeight(manifest, first.x, first.z);
                    float second_height = CcWorldTerrainHeight(manifest, second.x, second.z);
                    DrawStorybookStrip(first, second, first_side, second_side,
                        first_height, second_height,
                        5.8f, 0.0f, 0.13f, (Color){150, 127, 88, 255}, false);
                    for (int side = -1; side <= 1; side += 2) {
                        DrawStorybookStrip(first, second, first_side, second_side,
                            first_height, second_height,
                            1.3f, (float)side * 3.55f, 0.13f,
                            (Color){150, 127, 88, 255}, true);
                        DrawStorybookStrip(first, second, first_side, second_side,
                            first_height, second_height,
                            0.24f, (float)side * 1.35f, 0.15f,
                            (Color){112, 94, 68, 255}, false);
                    }
                }
            }
        }
    }
    SetWorldTerrainSurface(false);
}

typedef struct StorybookTree {
    float x;
    float z;
    float shade;
} StorybookTree;

static void EmitStorybookTrees(const StorybookTree *trees, int count,
                                TreeRegionalStyle style)
{
    for (int index = 0; index < count; ++index) {
        const StorybookTree *tree = &trees[index];
        DrawTree(tree->x, tree->z,
            tree->shade > 0.84f ? TREE_FAMILY_OAK : TREE_FAMILY_ALDER,
            BlendColor(WORLD_FOLIAGE, WORLD_FOLIAGE_LIGHT, tree->shade), style);
    }
}

static bool BuildStorybookForest(TreeSceneryBatchCache *batch,
                                  const StorybookTree *trees, int count,
                                  TreeRegionalStyle style)
{
    rlDrawRenderBatchActive();
    TreeBatchMeshWriter structure = {.indexed = true, .count_only = true};
    TreeBatchMeshWriter foliage = {.count_only = true};
    tree_render_pass = TREE_RENDER_BATCH_BUILD;
    tree_structure_writer = &structure;
    tree_foliage_writer = &foliage;
    EmitStorybookTrees(trees, count, style);
    TreeBatchFinishBuildPass();
    int source_draws = structure.source_draw_count + foliage.source_draw_count;
    int vertices = structure.vertex_cursor + foliage.vertex_cursor;
    if (!TreeBatchMeshWriterAllocate(&structure, structure.vertex_cursor,
                                      structure.index_cursor)) return false;
    if (!TreeBatchMeshWriterAllocate(&foliage, foliage.vertex_cursor, 0)) {
        UnloadMesh(structure.mesh);
        return false;
    }
    tree_render_pass = TREE_RENDER_BATCH_BUILD;
    tree_structure_writer = &structure;
    tree_foliage_writer = &foliage;
    EmitStorybookTrees(trees, count, style);
    TreeBatchFinishBuildPass();
    bool trunk_ready = TreeBatchMeshUpload(&structure, false, &batch->structure);
    bool leaves_ready = TreeBatchMeshUpload(&foliage, true, &batch->foliage);
    if (!trunk_ready || !leaves_ready) {
            return false;
    }
    batch->ready = true;
    batch->source_draw_count = source_draws;
    batch->vertex_count = vertices;
    storybook_forest_uploads += 2;
    return true;
}

/* A whole cell stays resident until it is beyond the camera's far plane.
   The extra ring lets a reversed move reuse the same GPU buffers. */
static bool StorybookCellVisible(Camera3D camera, int32_t x, int32_t z,
                                 float ground)
{
    Vector3 center = {(float)x * 80.0f + 40.0f, ground + 8.0f,
                       (float)z * 80.0f + 40.0f};
    return CameraBoundsVisible(camera, center, (Vector3){49.0f, 60.0f, 49.0f},
        (float)CC_LOCAL_ART_WIDTH / (float)CC_LOCAL_ART_HEIGHT, 240.0f);
}

static int StorybookCellTrees(const CcWorldManifest *manifest,
                              int32_t cell_x, int32_t cell_z,
                              StorybookTree trees[16], Vector3 rocks[16], int *rock_count)
{
    int count = 0;
    *rock_count = 0;
    for (int32_t row = 0; row < 4; ++row) {
        for (int32_t col = 0; col < 4; ++col) {
            int32_t gx = cell_x * 4 + col;
            int32_t gz = cell_z * 4 + row;
            float scatter = TerrainScatter01(gx, gz, manifest->world_seed);
            if (scatter < 0.54f) continue;
            float x = ((float)gx + 0.2f + scatter * 0.6f) * 20.0f;
            float z = ((float)gz + TerrainScatter01(gx, gz, 813U)) * 20.0f;
            if (CcWorldSurfaceAt(manifest, x, z) != CC_WORLD_SURFACE_WILDERNESS ||
                !OpenWorldPointClearsSettlements(manifest,
                    (CcWorldPoint){x, z}, 5.0f)) continue;
            if (scatter > 0.66f) {
                trees[count++] = (StorybookTree){x, z, scatter};
            } else {
                float y = CcWorldTerrainHeight(manifest, x, z);
                rocks[(*rock_count)++] = (Vector3){x, y + 0.4f, z};
            }
        }
    }
    return count;
}

static void DrawStorybookScenery(const CcWorldManifest *manifest, Camera3D camera)
{
    int32_t origin_x = (int32_t)floorf(camera.position.x / 80.0f);
    int32_t origin_z = (int32_t)floorf(camera.position.z / 80.0f);
    if (storybook_forest_seed != manifest->world_seed ||
        storybook_forest_generator != manifest->generator_version) {
        ClearStorybookForest();
        storybook_forest_seed = manifest->world_seed;
        storybook_forest_generator = manifest->generator_version;
    }
    for (int32_t i = 0; i < STORYBOOK_FOREST_CELL_COUNT; ++i) {
        StorybookForestCell *cell = &storybook_forest_cells[i];
        if (cell->occupied && (abs(cell->x - origin_x) > 5 ||
                               abs(cell->z - origin_z) > 5)) {
            ClearStorybookForestCell(cell);
        }
    }
    TreeRegionalStyle style = TreeStyleForKingdom(WORLD_GRASS);
    style.proportions = (Vector3){1.5f, 1.6f, 1.5f};
    for (int32_t row = -4; row <= 4; ++row) {
        for (int32_t col = -4; col <= 4; ++col) {
            int32_t x = origin_x + col;
            int32_t z = origin_z + row;
            float ground = CcWorldTerrainHeight(manifest,
                (float)x * 80.0f + 40.0f, (float)z * 80.0f + 40.0f);
            if (!StorybookCellVisible(camera, x, z, ground)) continue;
            StorybookForestCell *cell = NULL;
            StorybookForestCell *free_cell = NULL;
            for (int32_t i = 0; i < STORYBOOK_FOREST_CELL_COUNT; ++i) {
                StorybookForestCell *candidate = &storybook_forest_cells[i];
                if (candidate->occupied && candidate->x == x && candidate->z == z) {
                    cell = candidate;
                    break;
                }
                if (!candidate->occupied) free_cell = candidate;
            }
            if (cell == NULL) cell = free_cell;
            if (cell == NULL) continue;
            if (!cell->occupied) {
                StorybookTree trees[16];
                int count = StorybookCellTrees(manifest, x, z, trees,
                    cell->rocks, &cell->rock_count);
                if (count > 0 && !BuildStorybookForest(&cell->batch, trees, count, style)) {
                    EmitStorybookTrees(trees, count, style);
                    continue;
                }
                cell->occupied = true;
                cell->x = x;
                cell->z = z;
            }
            for (int rock = 0; rock < cell->rock_count; ++rock) {
                DrawScenerySphere(cell->rocks[rock], 0.9f,
                    BlendColor(WORLD_STONE, WORLD_GRASS, 0.25f));
            }
            if (!cell->batch.ready) continue;
            DrawModel(cell->batch.structure, (Vector3){0}, 1.0f, WHITE);
            DrawModel(cell->batch.foliage, (Vector3){0}, 1.0f, WHITE);
            CcLocalRendererRecordStaticBatch(2, cell->batch.source_draw_count,
                                            cell->batch.vertex_count);
        }
    }
}

void CcLocalDrawOpenWorld3D(const CcSim *sim,
                            const CcWorldStream *stream,
                            const CcLocalAgent *agent,
                            const CcLocalCourse *course,
                            const CcLocalWorldCarriageState *carriage,
                            float clock, RenderTexture2D target,
                            Rectangle destination)
{
    if (sim == NULL || stream == NULL || agent == NULL) return;
    CcLocalBindOpenWorld(stream);
    float state_camera_weight = carriage != NULL ?
        fmaxf(0.0f, fminf(1.0f, carriage->camera_weight)) : 0.0f;
    bool town_arrival = carriage != NULL && carriage->town_arrival;
    bool storybook = carriage != NULL && carriage->storybook_travel;
    bool kingdom_view = !storybook && CcLocalOpenWorldUsesKingdomMarkers(
        state_camera_weight);
    float camera_weight = SmoothStep01(state_camera_weight);
    Vector3 foot_focus = agent->position;
    foot_focus.y += 0.85f;
    float revealed_minimum_x = 0.0f;
    float revealed_minimum_z = 0.0f;
    float revealed_maximum_x = 0.0f;
    float revealed_maximum_z = 0.0f;
    OpenWorldRevealedBounds(
        sim, &stream->manifest, carriage,
        &revealed_minimum_x, &revealed_minimum_z,
        &revealed_maximum_x, &revealed_maximum_z);
    float world_width = revealed_maximum_x - revealed_minimum_x;
    float world_depth = revealed_maximum_z - revealed_minimum_z;
    float kingdom_span = sqrtf(world_width * world_width +
                               world_depth * world_depth);
    Vector3 kingdom_focus = {
        (revealed_minimum_x + revealed_maximum_x) * 0.5f,
        1.25f,
        (revealed_minimum_z + revealed_maximum_z) * 0.5f,
    };
    kingdom_focus.y += CcWorldTerrainHeight(
        &stream->manifest, kingdom_focus.x, kingdom_focus.z);
    Vector3 close_offset = {18.0f, 22.0f, 25.0f};
    Vector3 road_offset = {kingdom_span * 0.22f,
                           kingdom_span * 0.29f,
                           kingdom_span * 0.34f};
    Vector3 close_focus = foot_focus;
    Vector3 close_position = Vector3Add(close_focus, close_offset);
    const CcWorldSettlementPlacement *arrival_place =
        town_arrival && carriage != NULL ?
        CcWorldSettlementPlacementForId(
            &stream->manifest, sim->journey.destination_id) : NULL;
    if (arrival_place != NULL) {
        float scale = fmaxf(0.1f, arrival_place->profile_scale);
        float heading = carriage->heading_yaw;
        Vector3 forward = {sinf(heading), 0.0f, cosf(heading)};
        Vector3 right = {cosf(heading), 0.0f, -sinf(heading)};
        close_focus = Vector3Add(
            carriage->position,
            Vector3Add(Vector3Scale(forward, 5.2f * scale),
                       (Vector3){0.0f, 1.55f * scale, 0.0f}));
        close_position = Vector3Add(
            carriage->position,
            Vector3Add(Vector3Scale(forward, -10.5f * scale),
                       Vector3Add(Vector3Scale(right, 3.8f * scale),
                                  (Vector3){0.0f, 5.2f * scale, 0.0f})));
    }
    Vector3 focus = Vector3Lerp(
        close_focus, kingdom_focus, camera_weight);
    Vector3 road_position = Vector3Add(kingdom_focus, road_offset);
    float kingdom_fovy = fmaxf(
        54.0f, fmaxf(world_width * 0.82f, world_depth * 1.10f));
    float camera_fovy = 31.0f +
        (kingdom_fovy - 31.0f) * camera_weight;
    int32_t camera_projection = CAMERA_ORTHOGRAPHIC;
    if (arrival_place != NULL) {
        float road_distance = Vector3Distance(
            road_position, kingdom_focus);
        float road_fovy = 2.0f * atanf(
            kingdom_fovy * 0.5f / fmaxf(1.0f, road_distance)) * RAD2DEG;
        camera_fovy = 43.0f + (road_fovy - 43.0f) * camera_weight;
        camera_projection = CAMERA_PERSPECTIVE;
    }
    Camera3D camera = {
        .position = Vector3Lerp(
            close_position, road_position, camera_weight),
        .target = focus,
        .up = {0.0f, 1.0f, 0.0f},
        .fovy = camera_fovy,
        .projection = camera_projection,
    };
    if (storybook) {
        CcLocalWorldCarriageState travel_camera = *carriage;
        if (town_arrival) travel_camera.camera_weight = carriage->arrival_travel_weight;
        camera = StorybookCamera(&stream->manifest, &travel_camera);
        if (town_arrival) {
            camera.position = Vector3Lerp(close_position, camera.position, camera_weight);
            camera.target = Vector3Lerp(close_focus, camera.target, camera_weight);
        }
        focus = camera.target;
    } else {
        camera = SnapCameraToArtPixels(camera, target.texture.height);
    }
    if (carriage != NULL && carriage->visible) {
        camera = PonyConversationCamera(camera, carriage->position, carriage->heading_yaw, sim);
        focus = camera.target;
    }
    RememberPresentedCamera(CC_LOCAL_SCENE_STREET, camera, agent,
                            target.texture.width, target.texture.height);
    ArtComposition art = ROAD_ART_COMPOSITION;
    art.focal_point = focus;
    art.foreground_anchor = focus;
    art.light_profile = storybook ? ART_LIGHT_STORYBOOK : ART_LIGHT_CLEAR_MARKET;
    if (storybook) art.depth_splits = (Vector3){35.0f, 75.0f, 130.0f};
    SetFaceRenderContext(camera, target.texture.width, target.texture.height);
    BeginTextureMode(target);
    ClearBackground(ArtLightBackground(art.light_profile));
    if (storybook) {
        Color horizon = ArtLightBackground(art.light_profile);
        DrawRectangleGradientV(0, 0, target.texture.width, target.texture.height,
            BlendColor(horizon, (Color){81, 116, 143, 255}, 0.28f), horizon);
        for (int cloud = 0; cloud < 4; ++cloud) {
            float span = (float)target.texture.width + 140.0f;
            float x = fmodf((float)cloud * 197.0f + clock * 0.8f, span) - 70.0f;
            float y = 17.0f + (float)(cloud % 3) * 11.0f;
            Color mist = Fade(BlendColor(horizon, CC_STYLE_PARCHMENT, 0.45f), 0.16f);
            DrawEllipse((int)x, (int)y, 42.0f, 7.0f, mist);
            DrawEllipse((int)x + 25, (int)y + 3, 34.0f, 5.0f, mist);
        }
    }
    rlSetClipPlanes(0.10, storybook ? 240.0 : 4000.0);
    BeginMode3D(camera);
    BeginWorldLighting(camera, &art);
    if (storybook) {
        DrawStorybookGround(&stream->manifest, carriage->position);
        DrawStorybookRoads(sim, &stream->manifest, carriage);
        /* Finish world-space ribbons before tree models change the transform. */
        rlDrawRenderBatchActive();
        float scenery_fog = 1.0f;
        SetShaderValue(visual_style.world, visual_style.horizon_fog_location,
                       &scenery_fog, SHADER_UNIFORM_FLOAT);
        DrawStorybookScenery(&stream->manifest, camera);
    } else {
        DrawOpenWorldRouteNetwork(
            sim, &stream->manifest, carriage, kingdom_view);
    }
    DrawOpenWorldRoadSites(
        sim, &stream->manifest, carriage, focus, kingdom_view);
    for (int32_t i = 0; i < stream->manifest.settlement_count; ++i) {
        if (!OpenWorldSettlementRevealed(
                sim, stream->manifest.settlements[i].settlement_id)) {
            continue;
        }
        const CcWorldSettlementPlacement *place = &stream->manifest.settlements[i];
        Vector3 town_center = {place->center.x, place->plateau_height,
                                place->center.z};
        bool distant_view = storybook &&
            Vector3Distance(camera.position, town_center) > 55.0f;
        DrawOpenWorldSettlement(sim, place, focus, kingdom_view, distant_view);
    }
    DrawOpenWorldSites(sim, &stream->manifest, focus, clock, kingdom_view);
    bool hero_embarked = carriage != NULL && carriage->hero_embarked;
    if (!hero_embarked) DrawAgentPath(agent, false);
    if (course != NULL &&
        (course->alarm_active || course->road_encounter)) {
        DrawCourseRunners(course, focus);
    }
    if (carriage != NULL && carriage->visible) {
        Vector3 carriage_base = carriage->position;
        if (storybook) carriage_base.y += 0.13f;
        if (carriage->pace > 0.02f) {
            carriage_base.y += 0.025f + sinf(clock * 5.0f) * 0.018f;
        }
        float carriage_scale = 1.0f;
        if (arrival_place != NULL) {
            carriage_scale = arrival_place->profile_scale +
                (1.0f - arrival_place->profile_scale) * camera_weight;
        }
        rlPushMatrix();
        rlTranslatef(carriage_base.x, carriage_base.y, carriage_base.z);
        rlScalef(carriage_scale, carriage_scale, carriage_scale);
        rlTranslatef(-carriage_base.x, -carriage_base.y, -carriage_base.z);
        DrawRoadCarriage(carriage_base, sim, clock,
                         carriage->pace, carriage->heading_yaw,
                         true, false);
        rlPopMatrix();
        if (kingdom_view) {
            float marker_radius = fmaxf(2.8f, kingdom_span * 0.0055f);
            DrawCylinder((Vector3){carriage_base.x,
                                   carriage_base.y + 0.04f,
                                   carriage_base.z},
                         marker_radius, marker_radius, 0.10f, 24,
                         Fade(WORLD_TEAL, 0.78f));
            DrawCylinderWires((Vector3){carriage_base.x,
                                        carriage_base.y + 0.10f,
                                        carriage_base.z},
                              marker_radius * 1.35f,
                              marker_radius * 1.35f, 0.12f, 24,
                              Fade(WORLD_GOLD, 0.90f));
        }
    }
    if (!hero_embarked) {
        DrawRobotShell(agent);
        DrawCombatSword(agent);
    }
    CcLocalAgent passenger;
    if (hero_embarked && crew_exchange != NULL) {
        PrepareCrewPassenger(agent, carriage->position, carriage->heading_yaw, &passenger);
        DrawRobotShell(&passenger);
    }
    DrawCrew(hero_embarked && crew_exchange != NULL ? &passenger : agent, 6);
    EndWorldLighting();
    EndMode3D();
    rlSetClipPlanes(0.05, 4000.0);
    DrawTargetAtmosphere(target, clock);
    EndTextureMode();
    PresentTarget(target, destination);
    DrawCrewLabels(camera, destination);

    WorldLabel labels[64] = {0};
    int32_t label_count = 0;
    if (hero_embarked && carriage != NULL && !kingdom_view && !storybook) {
        labels[label_count++] = (WorldLabel){
            {carriage->position.x, carriage->position.y + 3.20f,
             carriage->position.z}, "YOUR CARRIAGE", WORLD_TEAL};
    } else if (!hero_embarked) {
        labels[label_count++] = (WorldLabel){
            {agent->position.x, agent->position.y + 2.50f,
             agent->position.z}, "YOU", WORLD_TEAL};
    }
    for (int32_t i = 0;
         i < stream->manifest.settlement_count && label_count < 64; ++i) {
        const CcWorldSettlementPlacement *place =
            &stream->manifest.settlements[i];
        if (!OpenWorldSettlementRevealed(sim, place->settlement_id)) continue;
        float dx = place->center.x - focus.x;
        float dz = place->center.z - focus.z;
        if (!kingdom_view && dx * dx + dz * dz > 78.0f * 78.0f) continue;
        const CcSettlement *settlement = CcSimSettlement(
            sim, place->settlement_id);
        if (settlement == NULL) continue;
        labels[label_count++] = (WorldLabel){
            {place->center.x, place->plateau_height + 4.2f,
             place->center.z}, settlement->name, WORLD_GOLD};
    }
    if (!kingdom_view) {
        for (int32_t i = 0;
             i < stream->manifest.road_site_count && label_count < 64; ++i) {
            const CcWorldRoadSitePlacement *placement =
                &stream->manifest.road_sites[i];
            const CcRoadSite *site = CcSimRoadSite(
                sim, placement->road_site_id);
            const CcWorldRoutePlacement *route =
                CcWorldRoutePlacementForId(
                    &stream->manifest, placement->route_id);
            CcRoadBookRouteView visibility = {0};
            float dx = placement->blocker_position.x - focus.x;
            float dz = placement->blocker_position.z - focus.z;
            if (site == NULL || route == NULL ||
                dx * dx + dz * dz > 88.0f * 88.0f ||
                !OpenWorldReadRouteView(
                    sim, route, carriage, &visibility) ||
                !CcRoadBookShowsRouteAmount(
                    &visibility,
                    CcWorldRouteJourneyAmount(route, route->from_id,
                    (float)site->progress_milli / 1000.0f))) continue;
            float ground = CcWorldTerrainHeight(
                &stream->manifest, placement->blocker_position.x,
                placement->blocker_position.z);
            labels[label_count++] = (WorldLabel){
                {placement->blocker_position.x, ground + 2.8f,
                 placement->blocker_position.z},
                site->name, WORLD_MUTED};
        }
    }
    DrawLabels(labels, label_count, camera, destination);
    DrawViewportText(storybook ? RoadJourneyBeatName(sim) :
        TextFormat("%s  ·  ROAD DISTRICTS",
                   kingdom_view ? "THE CROWNLESS ROAD BOOK" :
                   town_arrival ? "ARRIVING AT THE TOWN GATE" :
                                  "LEAVING THE TOWN GATE"),
        destination, 18, 18, 10, WORLD_MUTED);
}
