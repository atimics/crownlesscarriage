/* Open-world render caches, routes, sites, and settlements. */
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

static void OpenWorldRenderCacheClear(void)
{
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
                                    Vector3 focus, bool kingdom_view)
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
    Color kingdom = KingdomColor3D(sim, settlement->kingdom_id);
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
            CcHeraldryForKingdom(sim, settlement->kingdom_id), center,
            0.0f, 1.48f * scale, 0.0f);
        return;
    }
    float rotation = place->entrance_heading_yaw - 0.5f * PI;
    CcSettlementFunction previous_function = active_place_function;
    active_place_function = profile->function;

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
    DrawTriangle3D(points[0], points[2], points[1], color);
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
        Color road = route->closed ? WORLD_DANGER :
                     route->smuggler_route ? WORLD_VIOLET :
                                             WORLD_ROAD_LIGHT;
        for (int32_t sample = CC_WORLD_ROUTE_FROM_JUNCTION_SAMPLE;
             sample < CC_WORLD_ROUTE_TO_JUNCTION_SAMPLE; ++sample) {
            if (route->smuggler_route && (sample & 1) != 0) continue;
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
                &visibility, (float)site->progress_milli / 1000.0f)) {
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
        if (!OpenWorldSettlementRevealed(sim, site->settlement_id)) continue;
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
                (float)site->progress_milli / 1000.0f)) continue;
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
    bool kingdom_view = CcLocalOpenWorldUsesKingdomMarkers(
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
    camera = SnapCameraToArtPixels(camera, target.texture.height);
    RememberPresentedCamera(CC_LOCAL_SCENE_STREET, camera, agent,
                            target.texture.width, target.texture.height);
    ArtComposition art = ROAD_ART_COMPOSITION;
    art.focal_point = focus;
    art.foreground_anchor = focus;
    art.light_profile = ART_LIGHT_CLEAR_MARKET;
    SetFaceRenderContext(camera, target.texture.width, target.texture.height);
    BeginTextureMode(target);
    ClearBackground(ArtLightBackground(art.light_profile));
    BeginMode3D(camera);
    BeginWorldLighting(camera, &art);
    DrawOpenWorldRouteNetwork(
        sim, &stream->manifest, carriage, kingdom_view);
    DrawOpenWorldRoadSites(
        sim, &stream->manifest, carriage, focus, kingdom_view);
    for (int32_t i = 0; i < stream->manifest.settlement_count; ++i) {
        if (!OpenWorldSettlementRevealed(
                sim, stream->manifest.settlements[i].settlement_id)) {
            continue;
        }
        DrawOpenWorldSettlement(sim, &stream->manifest.settlements[i], focus,
                                kingdom_view);
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
        DrawRoadCarriage(carriage_base, &sim->player, clock,
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
    EndWorldLighting();
    EndMode3D();
    DrawTargetAtmosphere(target, clock);
    EndTextureMode();
    PresentTarget(target, destination);

    WorldLabel labels[64] = {0};
    int32_t label_count = 0;
    if (hero_embarked && carriage != NULL && !kingdom_view) {
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
                    (float)site->progress_milli / 1000.0f)) continue;
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
    DrawViewportText(
        TextFormat("%s  ·  ROAD DISTRICTS  ·  SIDE ROADS ARE CLOSED",
                   kingdom_view ? "THE CROWNLESS ROAD BOOK" :
                   town_arrival ? "ARRIVING AT THE TOWN GATE" :
                                  "LEAVING THE TOWN GATE"),
        destination, 18, 18, 10, WORLD_MUTED);
}
