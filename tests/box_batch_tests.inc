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
        if (i % 17 == 0) {
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
    for (int32_t view = 0; view < 9; ++view) {
        bool crowded = view == 8;
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
