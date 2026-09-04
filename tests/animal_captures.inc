/* Capture the shipped skins through the game lighting and final screen pass. */
static void TestAnimalModels(const char *path)
{
    const CcCreatureVariant animals[] = {
        CC_CREATURE_HORSE, CC_CREATURE_HORSE, CC_CREATURE_HORSE,
        CC_CREATURE_HORSE, CC_CREATURE_COW, CC_CREATURE_SHEEP,
    };
    RenderTexture2D scene = LoadRenderTexture(96, 96);
    RequireRenderer(IsRenderTextureValid(scene), "animal target must load");
    Image sheet = GenImageColor(768, 576, BLACK);
    Camera3D camera = {
        .position = {3.0f, 2.4f, 4.0f}, .target = {0.0f, 0.8f, 0.0f},
        .up = {0.0f, 1.0f, 0.0f}, .fovy = 3.5f,
        .projection = CAMERA_ORTHOGRAPHIC,
    };
    for (int32_t animal = 0; animal < 6; ++animal) {
        CreatureModelCache *model = &creature_models[animals[animal]][CC_CREATURE_POSE_IDLE];
        RequireRenderer(model->ready && model->model.meshCount == 1,
                        "animal review must use the shipped skin");
        Color previous[96 * 96] = {0};
        for (int32_t frame = 0; frame < 8; ++frame) {
            BeginTextureMode(scene);
            ClearBackground((Color){24, 28, 34, 255});
            BeginMode3D(camera);
            BeginWorldLighting(camera, &ROAD_ART_COMPOSITION);
            RequireRenderer(DrawCreatureGait3D(
                animals[animal], CC_CREATURE_POSE_IDLE, (Vector3){0},
                (float)(frame / 2) * PI * 0.5f, 1.0f, animal < 4 ? PONY_COLORS[animal][0] : (Color){0},
                (float)(frame % 2) * PI * 0.5f, true, NULL),
                "animal walking draw must succeed");
            EndWorldLighting();
            EndMode3D();
            EndTextureMode();
            Image raw = LoadImageFromTexture(scene.texture);
            Color *raw_pixels = LoadImageColors(raw);
            int32_t edge = 0;
            for (int32_t p = 0; p < 96 * 96; ++p) {
                if (p / 96 > 1 && p / 96 < 94 && p % 96 > 1 && p % 96 < 94) continue;
                if (raw_pixels[p].r != 24 || raw_pixels[p].g != 28 || raw_pixels[p].b != 34) edge++;
            }
            RequireRenderer(edge == 0, "animal silhouette must fit inside the frame");
            UnloadImageColors(raw_pixels);
            UnloadImage(raw);
            BeginDrawing();
            ClearBackground(BLACK);
            PresentTarget(scene, (Rectangle){0, 0, 96, 96});
            rlDrawRenderBatchActive();
            Image shot = LoadImageFromScreen();
            EndDrawing();
            ImageCrop(&shot, (Rectangle){0, 0, 96, 96});
            Color *pixels = LoadImageColors(shot);
            int32_t visible = 0;
            int32_t changed = 0;
            for (int32_t p = 0; p < 96 * 96; ++p) {
                if (pixels[p].r > 65 || pixels[p].g > 65 || pixels[p].b > 65) visible++;
                if (pixels[p].r != previous[p].r || pixels[p].g != previous[p].g ||
                    pixels[p].b != previous[p].b) changed++;
                previous[p] = pixels[p];
            }
            RequireRenderer(visible > 60, "animal capture must contain visible lit geometry");
            if (frame % 2 == 1) {
                RequireRenderer(changed > 10, "walking poses must move the animal skin");
            }
            UnloadImageColors(pixels);
            ImageDraw(&sheet, shot, (Rectangle){0, 0, 96, 96},
                      (Rectangle){(float)frame * 96, (float)animal * 96, 96, 96}, WHITE);
            UnloadImage(shot);
        }
    }
    char output[1024];
    (void)snprintf(output, sizeof(output), "%s-animals.png", path);
    RequireRenderer(ExportImage(sheet, output), "animal sheet must be written");
    UnloadImage(sheet);
    UnloadRenderTexture(scene);
    (void)puts("PASS animal skins: four pony colors and two farm species, four turns, two walking poses");
}
