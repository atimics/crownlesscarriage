static void TestBuildingRevealTiming(void)
{
    const float frame_times[] = {1.0f / 30.0f, 1.0f / 60.0f, 1.0f / 144.0f};
    for (int32_t policy = 0; policy < 2; ++policy) {
        float clear_delay = policy == 0 ? 0.14f : 0.16f;
        float close_response = policy == 0 ? 5.0f : 4.6f;
        for (int32_t rate = 0; rate < 3; ++rate) {
            float dt = frame_times[rate];
            WorldBuildingRevealState state = {0};
            UpdateBuildingReveal(&state, false, true, dt,
                                 clear_delay, close_response);
            for (int32_t frame = 0; frame < 60; ++frame) {
                UpdateBuildingReveal(&state, frame % 2 == 0, false, dt,
                                     clear_delay, close_response);
                RequireRenderer(state.amount == 0.0f,
                                "brief edge crossings must keep a closed building whole");
            }

            for (int32_t frame = 0; frame < 240; ++frame) {
                float previous = state.amount;
                UpdateBuildingReveal(&state, true, false, dt,
                                     clear_delay, close_response);
                RequireRenderer(state.amount >= previous,
                                "a blocked sightline must open the cutaway smoothly");
            }
            RequireRenderer(state.amount == 1.0f,
                            "a sustained blocked sightline must fully open the cutaway");

            for (int32_t frame = 0; frame < 120; ++frame) {
                UpdateBuildingReveal(&state, frame % 2 != 0, false, dt,
                                     clear_delay, close_response);
                RequireRenderer(state.amount == 1.0f,
                                "an open cutaway must hold through repeated edge crossings");
            }

            float clear_seconds = 0.0f;
            while (clear_seconds + dt < clear_delay) {
                UpdateBuildingReveal(&state, false, false, dt,
                                     clear_delay, close_response);
                clear_seconds += dt;
                RequireRenderer(state.amount == 1.0f,
                                "a brief clear sightline must hold the open cutaway");
            }
            for (int32_t frame = 0; frame < 300; ++frame) {
                float previous = state.amount;
                UpdateBuildingReveal(&state, false, false, dt,
                                     clear_delay, close_response);
                RequireRenderer(state.amount <= previous,
                                "a clear sightline must close the cutaway smoothly");
            }
            RequireRenderer(state.amount == 0.0f,
                            "a sustained clear sightline must fully restore the building");

            // Confirm brief clear frames also preserve a reveal while it opens.
            for (int32_t frame = 0; frame < 16; ++frame) {
                UpdateBuildingReveal(&state, true, false, dt,
                                     clear_delay, close_response);
            }
            RequireRenderer(state.amount > 0.0f && state.amount < 1.0f,
                            "the transition fixture must be partly open");
            for (int32_t frame = 0; frame < 60; ++frame) {
                float previous = state.amount;
                UpdateBuildingReveal(&state, frame % 2 == 0, false, dt,
                                     clear_delay, close_response);
                RequireRenderer(state.amount >= previous,
                                "edge crossings must preserve an opening transition");
            }
            float frozen_amount = state.amount;
            UpdateBuildingReveal(&state, false, false, 0.0f,
                                 clear_delay, close_response);
            RequireRenderer(state.amount == frozen_amount,
                            "a paused frame must keep the current cutaway height");

            UpdateBuildingReveal(&state, true, true, dt,
                                 clear_delay, close_response);
            RequireRenderer(state.amount == 1.0f,
                            "a scene reset must immediately reveal a blocked hero");
            UpdateBuildingReveal(&state, false, true, dt,
                                 clear_delay, close_response);
            RequireRenderer(state.amount == 0.0f,
                            "a scene reset must immediately restore a clear building");
        }
    }
    (void)puts("PASS building cutaways: edge crossings, transitions and resets at 30/60/144 Hz");
}

static Image RenderRaisedCutaway(RenderTexture2D target, bool reference)
{
    Camera3D camera = {
        .position = {4.0f, 5.0f, 7.0f}, .target = {0.0f, 1.0f, 0.0f},
        .up = {0.0f, 1.0f, 0.0f}, .fovy = 5.0f,
        .projection = CAMERA_ORTHOGRAPHIC,
    };
    BeginTextureMode(target);
    ClearBackground(BLACK);
    BeginMode3D(camera);
    BeginWorldLighting(camera, &INTERIOR_ART_COMPOSITION);
    SetWorldForegroundReveal(1.0f, 1.85f);
    rlPushMatrix();
    rlTranslatef(0.0f, 1.25f, 0.0f);
    DrawBox((Vector3){0.0f, 1.5f, 0.0f},
            (Vector3){2.0f, 3.0f, 2.0f}, WORLD_STONE);
    DrawBuildingRevealCap(-1.0f, -1.0f, 2.0f, 2.0f,
                          3.0f, 0.60f, WORLD_STONE, WORLD_WOOD);
    // The reference ends the placement transform before drawing queued walls.
    if (reference) rlPopMatrix();
    SetWorldForegroundReveal(0.0f, 1.85f);
    if (!reference) rlPopMatrix();
    EndWorldLighting();
    EndMode3D();
    EndTextureMode();
    return LoadImageFromTexture(target.texture);
}

static void TestRaisedBuildingCutaway(const char *capture_path)
{
    RenderTexture2D target = LoadRenderTexture(256, 256);
    RequireRenderer(IsRenderTextureValid(target), "cutaway target must load");
    Image reference = RenderRaisedCutaway(target, true);
    Image actual = RenderRaisedCutaway(target, false);
    Color *expected = LoadImageColors(reference);
    Color *pixels = LoadImageColors(actual);
    RequireRenderer(expected != NULL && pixels != NULL,
                    "cutaway images must be readable");
    int32_t differences = 0;
    int32_t visible = 0;
    for (int32_t i = 0; i < target.texture.width * target.texture.height; ++i) {
        if (expected[i].r > 0 || expected[i].g > 0 || expected[i].b > 0) visible++;
        if (expected[i].r != pixels[i].r || expected[i].g != pixels[i].g ||
            expected[i].b != pixels[i].b) differences++;
    }
    char path[1024];
    (void)snprintf(path, sizeof(path), "%s-cutaway.png", capture_path);
    ImageFlipVertical(&actual);
    RequireRenderer(ExportImage(actual, path), "cutaway capture must be written");
    UnloadImageColors(expected);
    UnloadImageColors(pixels);
    UnloadImage(reference);
    UnloadImage(actual);
    UnloadRenderTexture(target);
    (void)printf("Raised cutaway: %d visible pixels, %d changed pixels\n",
                 visible, differences);
    RequireRenderer(visible > 1000 && differences == 0,
                    "raised walls must keep the same cut height across draw flushes");
}
