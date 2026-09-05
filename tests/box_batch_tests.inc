/* Compare the box batch with raylib's reference primitive on the GPU. */
static Image RenderBoxBatchFixture(RenderTexture2D target, Texture2D marker,
                                    bool reference, int32_t view, bool crowded)
{
    float turn = (float)view * PI * 0.5f;
    Camera3D camera = {
        .position = {cosf(turn) * 5.0f, view < 4 ? 3.0f : -3.0f,
                     sinf(turn) * 5.0f},
        .target = {0}, .up = {0, 1, 0},
        .fovy = crowded ? 38.0f : (view % 2 == 0 ? 4.0f : 45.0f),
        .projection = crowded || view % 2 == 0 ?
            CAMERA_ORTHOGRAPHIC : CAMERA_PERSPECTIVE,
    };
    BeginTextureMode(target);
    ClearBackground(BLACK);
    BeginMode3D(camera);
    BeginWorldLighting(camera, &INTERIOR_ART_COMPOSITION);
    // Texture draws also use quads; the following box must use solid color.
    DrawTexturePro(marker, (Rectangle){0, 0, 2, 2},
                   (Rectangle){-0.7f, -0.7f, 0.25f, 0.25f},
                   (Vector2){0}, 0.0f, WHITE);
    int32_t count = crowded ? 1600 : 4;
    for (int32_t i = 0; i < count; ++i) {
        rlPushMatrix();
        if (crowded) {
            rlTranslatef((float)(i % 40) * 0.6f - 11.7f, 0.0f,
                         (float)(i / 40) * 0.6f - 11.7f);
        } else if (i != 0) {
            rlTranslatef(0.32f, 0.17f, -0.14f);
            rlRotatef((float)i * 23.0f, 0.3f, 1.0f, 0.2f);
            rlScalef(0.7f, 1.1f, 0.8f);
        }
        Vector3 center = crowded ? (Vector3){0} :
            (Vector3){(float)i * 0.45f - 0.7f, -0.05f, 0.0f};
        Vector3 size = crowded ? (Vector3){0.46f, 0.64f, 0.34f} :
            (Vector3){0.65f, 0.95f, 0.55f};
        Color color = i % 2 == 0 ? (Color){176, 116, 78, 255} :
                                   (Color){73, 151, 140, 180};
        if (reference) DrawCubeV(center, size, color);
        else DrawBox(center, size, color);
        rlPopMatrix();
        // Reference primitives flush after the parent transform is restored.
        // This gives correct lighting independent of automatic batch limits.
        if (reference && crowded) rlDrawRenderBatchActive();
        if (view == 9 || i % 17 == 0) {
            // Exercise index alignment when triangles and lines share the batch.
            DrawTriangle3D((Vector3){-0.3f, -0.7f, 0.3f},
                           (Vector3){0.3f, -0.7f, 0.3f},
                           (Vector3){0.0f, -0.2f, 0.3f}, BLUE);
            DrawLine3D((Vector3){-0.8f, -0.8f, 0.0f},
                       (Vector3){0.8f, -0.8f, 0.0f}, RED);
        }
    }
    EndWorldLighting();
    EndMode3D();
    EndTextureMode();
    return LoadImageFromTexture(target.texture);
}

static void TestBoxBatchParity(void)
{
    RenderTexture2D target = LoadRenderTexture(256, 256);
    RequireRenderer(IsRenderTextureValid(target), "box target must load");
    Image marker_image = GenImageColor(2, 2, MAGENTA);
    Texture2D marker = LoadTextureFromImage(marker_image);
    UnloadImage(marker_image);
    RequireRenderer(IsTextureValid(marker), "box marker texture must load");
    for (int32_t view = 0; view < 10; ++view) {
        bool crowded = view >= 8;
        Image expected = RenderBoxBatchFixture(target, marker, true, view, crowded);
        Image actual = RenderBoxBatchFixture(target, marker, false, view, crowded);
        Color *before = LoadImageColors(expected);
        Color *after = LoadImageColors(actual);
        RequireRenderer(before != NULL && after != NULL,
                        "box images must be readable");
        int32_t changed = 0;
        int32_t visible = 0;
        int32_t count = expected.width * expected.height;
        for (int32_t pixel = 0; pixel < count; ++pixel) {
            Color a = before[pixel];
            Color b = after[pixel];
            if (a.r > 16 || a.g > 16 || a.b > 16) visible++;
            if (abs((int)a.r - (int)b.r) > 3 ||
                abs((int)a.g - (int)b.g) > 3 ||
                abs((int)a.b - (int)b.b) > 3) changed++;
        }
        (void)printf("Box parity view %d: %d changed of %d visible pixels\n",
                     view, changed, visible);
        RequireRenderer(visible > 1000, "box fixture must cover the target");
        RequireRenderer(changed < visible / 100,
                        "batched boxes must match reference faces and shading");
        UnloadImageColors(before);
        UnloadImageColors(after);
        UnloadImage(expected);
        UnloadImage(actual);
    }
    UnloadTexture(marker);
    UnloadRenderTexture(target);
    (void)puts("PASS box batch: all faces, transforms, alpha, and buffer rollover");
}

static void ApplyBoxFixtureState(bool reference, float reveal, float height,
                                  bool terrain)
{
    if (!reference) {
        SetWorldForegroundReveal(reveal, height);
        SetWorldTerrainSurface(terrain);
        return;
    }
    // The reference always uploads state after flushing the preceding shapes.
    rlDrawRenderBatchActive();
    float surface = terrain ? 1.0f : 0.0f;
    SetShaderValue(visual_style.world, visual_style.foreground_reveal_location,
                   &reveal, SHADER_UNIFORM_FLOAT);
    SetShaderValue(visual_style.world, visual_style.reveal_cut_height_location,
                   &height, SHADER_UNIFORM_FLOAT);
    SetShaderValue(visual_style.world, visual_style.terrain_surface_location,
                   &surface, SHADER_UNIFORM_FLOAT);
}

static Image RenderBoxStateFixture(RenderTexture2D target, bool reference)
{
    Camera3D camera = {.position = {4, 5, 7}, .target = {0}, .up = {0, 1, 0},
                      .fovy = 6.0f, .projection = CAMERA_ORTHOGRAPHIC};
    BeginTextureMode(target);
    ClearBackground(BLACK);
    BeginMode3D(camera);
    for (int32_t pass = 0; pass < 2; ++pass) {
        BeginWorldLighting(camera, &INTERIOR_ART_COMPOSITION);
        const float reveals[] = {1.0f, 1.0f, 1.0f, 0.0f, 1.0f};
        const float heights[] = {0.2f, 0.2f, 0.7f, 0.7f, 0.2f};
        const bool terrain[] = {true, true, false, false, true};
        for (int32_t i = 0; i < 5; ++i) {
            ApplyBoxFixtureState(reference, reveals[i], heights[i], terrain[i]);
            DrawBox((Vector3){(float)i * 0.8f - 1.6f, 0.0f,
                              (float)pass * 1.5f - 0.75f},
                    (Vector3){0.6f, 1.8f, 0.6f},
                    i % 2 == 0 ? WORLD_ROAD : WORLD_TEAL);
        }
        EndWorldLighting();
    }
    EndMode3D();
    EndTextureMode();
    return LoadImageFromTexture(target.texture);
}

static void TestBoxStateParity(void)
{
    RenderTexture2D target = LoadRenderTexture(256, 256);
    RequireRenderer(IsRenderTextureValid(target), "state target must load");
    Image expected = RenderBoxStateFixture(target, true);
    Image actual = RenderBoxStateFixture(target, false);
    Color *before = LoadImageColors(expected);
    Color *after = LoadImageColors(actual);
    RequireRenderer(before != NULL && after != NULL,
                    "state images must be readable");
    int32_t changed = 0;
    int32_t visible = 0;
    for (int32_t i = 0; i < expected.width * expected.height; ++i) {
        if (before[i].r > 16 || before[i].g > 16 || before[i].b > 16) visible++;
        if (abs((int)before[i].r - (int)after[i].r) > 1 ||
            abs((int)before[i].g - (int)after[i].g) > 1 ||
            abs((int)before[i].b - (int)after[i].b) > 1) changed++;
    }
    RequireRenderer(visible > 1000, "state fixture must cover the target");
    RequireRenderer(changed == 0,
                    "cached state must preserve cuts, terrain, and new passes");
    UnloadImageColors(before);
    UnloadImageColors(after);
    UnloadImage(expected);
    UnloadImage(actual);
    UnloadRenderTexture(target);
    (void)puts("PASS render state: repeated values, cuts, terrain, and new passes");
}

static double MeasureBoxDraws(RenderTexture2D target, bool reference)
{
    Camera3D camera = {.position = {25, 30, 25}, .target = {0},
                      .up = {0, 1, 0}, .fovy = 38.0f,
                      .projection = CAMERA_ORTHOGRAPHIC};
    double start = GetTime();
    for (int32_t frame = 0; frame < 32; ++frame) {
        BeginTextureMode(target);
        ClearBackground(BLACK);
        BeginMode3D(camera);
        for (int32_t box = 0; box < 1600; ++box) {
            Vector3 center = {(float)(box % 40) * 0.6f - 11.7f, 0.0f,
                              (float)(box / 40) * 0.6f - 11.7f};
            Vector3 size = {0.46f, 0.64f, 0.34f};
            if (reference) DrawCubeV(center, size, WHITE);
            else DrawBox(center, size, WHITE);
        }
        EndMode3D();
        EndTextureMode();
    }
    // Readback includes completion of the queued GPU work in both timings.
    Image result = LoadImageFromTexture(target.texture);
    double elapsed = GetTime() - start;
    UnloadImage(result);
    return elapsed;
}

static void ReportBoxDrawPerformance(void)
{
    RenderTexture2D target = LoadRenderTexture(256, 256);
    RequireRenderer(IsRenderTextureValid(target), "benchmark target must load");
    (void)MeasureBoxDraws(target, true);
    (void)MeasureBoxDraws(target, false);
    double reference = 0.0;
    double batched = 0.0;
    for (int32_t round = 0; round < 4; ++round) {
        // Alternate order to share warmup and scheduling effects.
        if (round % 2 == 0) reference += MeasureBoxDraws(target, true);
        batched += MeasureBoxDraws(target, false);
        if (round % 2 != 0) reference += MeasureBoxDraws(target, true);
    }
    (void)printf("Box draw benchmark (1600 boxes, 128 frames): "
                 "reference %.3f ms/frame, batched %.3f ms/frame\n",
                 reference * 1000.0 / 128.0, batched * 1000.0 / 128.0);
    UnloadRenderTexture(target);
}
