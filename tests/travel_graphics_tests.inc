static Image CaptureTravelLeaves(RenderTexture2D target, Model leaves, float shift)
{
    Camera3D camera = {.position = {shift, 40, 0}, .target = {shift, 0, 0},
        .up = {0, 0, -1}, .fovy = 96, .projection = CAMERA_ORTHOGRAPHIC};
    BeginTextureMode(target);
    ClearBackground(BLACK);
    BeginMode3D(camera);
    BeginWorldLighting(camera, &ROAD_ART_COMPOSITION);
    DrawModel(leaves, (Vector3){0}, 1, (Color){80, 130, 65, 255});
    EndWorldLighting();
    EndMode3D();
    EndTextureMode();
    return LoadImageFromTexture(target.texture);
}

static void TestTravelLeafShimmer(void)
{
    RenderTexture2D target = LoadRenderTexture(128, 128);
    Model leaves = LoadModelFromMesh(GenMeshPlane(200, 200, 1, 1));
    ApplyTreeFoliageShader(&leaves);
    Image first = CaptureTravelLeaves(target, leaves, 0);
    Image second = CaptureTravelLeaves(target, leaves, 0.13f);
    Color *a = LoadImageColors(first);
    Color *b = LoadImageColors(second);
    RequireRenderer(a != NULL && b != NULL, "travel leaf captures must load");
    double difference = 0;
    for (int y = 32; y < 96; ++y) {
        for (int x = 32; x < 96; ++x) {
            int i = y * 128 + x;
            difference += abs((int)a[i].r - (int)b[i].r) +
                          abs((int)a[i].g - (int)b[i].g) +
                          abs((int)a[i].b - (int)b[i].b);
        }
    }
    difference /= 64.0 * 64.0 * 3.0;
    (void)printf("Travel leaf motion: %.3f mean channel change\n", difference);
    RequireRenderer(difference < 0.1,
                    "distant leaf paint must stay steady during subpixel camera motion");
    UnloadImageColors(a);
    UnloadImageColors(b);
    UnloadImage(first);
    UnloadImage(second);
    UnloadModel(leaves);
    UnloadRenderTexture(target);
}

static void TestTravelForestCameraTurn(void)
{
    static CcSim sim;
    static CcWorldStream stream;
    CcSimInit(&sim, UINT32_C(0xc0a71a9e));
    RequireRenderer(CcWorldStreamInit(&stream, &sim), "travel world must load");
    CcLocalBindOpenWorld(&stream);
    Vector3 tree = {0};
    bool found = false;
    for (int gz = -20; gz <= 20 && !found; ++gz) {
        for (int gx = -20; gx <= 20 && !found; ++gx) {
            float scatter = TerrainScatter01(gx, gz, stream.manifest.world_seed);
            if (scatter <= 0.66f) continue;
            float x = ((float)gx + 0.2f + scatter * 0.6f) * 20.0f;
            float z = ((float)gz + TerrainScatter01(gx, gz, 813U)) * 20.0f;
            if (CcWorldSurfaceAt(&stream.manifest, x, z) != CC_WORLD_SURFACE_WILDERNESS ||
                !OpenWorldPointClearsSettlements(&stream.manifest, (CcWorldPoint){x, z}, 5)) continue;
            tree = (Vector3){x, CcWorldTerrainHeight(&stream.manifest, x, z), z};
            found = true;
        }
    }
    RequireRenderer(found, "camera turn fixture needs a wilderness tree");
    CcLocalWorldCarriageState carriage = {.storybook_travel = true,
        .position = {tree.x - 8, tree.y, tree.z - 4.5f},
        .camera_heading_yaw = atan2f(-20, 19) - 0.001f};
    CcLocalAgent agent = {.position = carriage.position};
    RenderTexture2D target = LoadRenderTexture(320, 180);
    BeginDrawing();
    CcLocalDrawOpenWorld3D(&sim, &stream, &agent, NULL, &carriage, 0,
        target, (Rectangle){0, 0, 320, 180});
    EndDrawing();
    uint64_t first_uploads = storybook_forest_uploads;
    RequireRenderer(first_uploads > 0, "travel forest must draw");
    unsigned int buffers[STORYBOOK_FOREST_CELL_COUNT] = {0};
    for (int i = 0; i < STORYBOOK_FOREST_CELL_COUNT; ++i) {
        if (storybook_forest_cells[i].batch.ready) {
            buffers[i] = storybook_forest_cells[i].batch.foliage.meshes[0].vboId[0];
        }
    }
    carriage.camera_heading_yaw += 0.002f;
    BeginDrawing();
    CcLocalDrawOpenWorld3D(&sim, &stream, &agent, NULL, &carriage, 0,
        target, (Rectangle){0, 0, 320, 180});
    EndDrawing();
    RequireRenderer(storybook_forest_uploads == first_uploads,
                    "small camera turns must reuse resident tree cells");
    for (int i = 0; i < STORYBOOK_FOREST_CELL_COUNT; ++i) {
        if (buffers[i] != 0) RequireRenderer(
            storybook_forest_cells[i].batch.foliage.meshes[0].vboId[0] == buffers[i],
            "resident cells must keep their GPU buffers");
    }
    (void)puts("PASS travel forest: camera turn preserves trees and GPU buffers");
    UnloadRenderTexture(target);
    CcLocalBindOpenWorld(NULL);
    OpenWorldRenderCacheClear();
}
