/* Capture the shipped skins through the game lighting and final screen pass. */
static void TestPonyHarnessAttachment(void)
{
    CreatureModelCache *pony =
        &creature_models[CC_CREATURE_HORSE][CC_CREATURE_POSE_IDLE];
    CreatureModelCache saved = *pony;
    Transform bind[2] = {
        {.translation = {0, 1, 0}, .rotation = {0, 0, 0, 1}, .scale = {1, 1, 1}},
        {.translation = {0, 2, 1}, .rotation = {0, 0, 0, 1}, .scale = {1, 1, 1}},
    };
    *pony = (CreatureModelCache){0};
    pony->ready = true;
    pony->model.skeleton.boneCount = 2;
    pony->model.skeleton.bindPose = bind;
    pony->quadruped_bone[0] = CC_QUADRUPED_CHEST;
    pony->quadruped_bone[1] = CC_QUADRUPED_HEAD;
    pony->pose[0] = bind[0];
    pony->pose[1] = bind[1];
    /* A lifted head turns the muzzle from forward to right in model space. */
    pony->pose[1].translation = (Vector3){0, 3, 1};
    pony->pose[1].rotation = QuaternionFromAxisAngle((Vector3){0, 1, 0}, PI * 0.5f);
    Vector3 nose = RoadPonySkinPoint((Vector3){4, 5, 6}, PI * 0.5f,
                                    (Vector3){0, 2, 2}, CC_QUADRUPED_HEAD);
    RequireRenderer(Vector3Distance(nose, (Vector3){4.96f, 7.88f, 5.04f}) < 0.001f,
                    "bridle must follow head lift, head turn, body turn, and pony scale");
    Vector3 chest = RoadPonySkinPoint((Vector3){4, 5, 6}, PI * 0.5f,
                                     (Vector3){0, 1, 0}, CC_QUADRUPED_CHEST);
    RequireRenderer(Vector3Distance(chest, (Vector3){4, 5.96f, 6}) < 0.001f,
                    "collar must follow the chest while the head turns");
    *pony = saved;
    (void)puts("PASS pony harness: head and chest attachments follow the skin");
}

static void TestAnimalModels(const char *path)
{
    const CcCreatureVariant animals[] = {
        CC_CREATURE_HORSE, CC_CREATURE_HORSE, CC_CREATURE_HORSE,
        CC_CREATURE_HORSE, CC_CREATURE_HORSE, CC_CREATURE_HORSE,
        CC_CREATURE_HORSE, CC_CREATURE_COW, CC_CREATURE_SHEEP,
    };
    RenderTexture2D scene = LoadRenderTexture(96, 96);
    RequireRenderer(IsRenderTextureValid(scene), "animal target must load");
    Image sheet = GenImageColor(768, 864, BLACK);
    Camera3D camera = {
        .position = {3.0f, 2.4f, 4.0f}, .target = {0.0f, 0.8f, 0.0f},
        .up = {0.0f, 1.0f, 0.0f}, .fovy = 3.5f,
        .projection = CAMERA_ORTHOGRAPHIC,
    };
    for (int32_t animal = 0; animal < 9; ++animal) {
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
                (float)(frame / 2) * PI * 0.5f, 1.0f, animal < CC_PONY_COUNT ? PONY_COLORS[animal][0] : (Color){0},
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
    (void)puts("PASS animal skins: seven pony colors and two farm species, four turns, two walking poses");
}

static void TestPonyPortraits(const char *path)
{
    Image sheet = GenImageColor(64 * (CC_PONY_COUNT + 1), 64, BLACK);
    for (int32_t known = 0; known < 2; ++known) {
        for (int32_t pony = 0; pony < CC_PONY_COUNT; ++pony) {
            BeginDrawing();
            CcLocalDrawPonyPortrait(pony, known != 0, (Rectangle){0, 0, 64, 64});
            EndDrawing();
            int32_t index = known ? pony : CC_PONY_COUNT;
            RequireRenderer(IsRenderTextureValid(pony_portraits[index]), "pony face must render");
            Image face = LoadImageFromTexture(pony_portraits[index].texture);
            Color *pixels = LoadImageColors(face);
            int32_t colorful = 0;
            int32_t visible = 0;
            for (int32_t p = 0; p < face.width * face.height; ++p) {
                int32_t high = pixels[p].r > pixels[p].g ? pixels[p].r : pixels[p].g;
                high = high > pixels[p].b ? high : pixels[p].b;
                int32_t low = pixels[p].r < pixels[p].g ? pixels[p].r : pixels[p].g;
                low = low < pixels[p].b ? low : pixels[p].b;
                if (high - low > 18) colorful++;
                if (high > 65) visible++;
                if (!known) RequireRenderer(high == low, "unmet pony portrait must be gray");
            }
            RequireRenderer(visible > 100, "pony portrait must contain visible face geometry");
            if (known) RequireRenderer(colorful > 100, "met pony face must show its color");
            UnloadImageColors(pixels);
            ImageFlipVertical(&face);
            ImageDraw(&sheet, face, (Rectangle){0, 0, 64, 64},
                      (Rectangle){(float)index * 64, 0, 64, 64}, WHITE);
            UnloadImage(face);
        }
    }
    char output[1024];
    (void)snprintf(output, sizeof(output), "%s-portraits.png", path);
    RequireRenderer(ExportImage(sheet, output), "pony portraits must be written");
    UnloadImage(sheet);
    (void)puts("PASS pony portraits: seven colored faces and a shared gray mystery face");
}

static void CapturePonies(const char *path)
{
    const int size = 192;
    RenderTexture2D scene = LoadRenderTexture(size, size);
    RequireRenderer(IsRenderTextureValid(scene), "pony review target must load");
    Image sheet = GenImageColor(size * CC_PONY_COUNT, size * 3, BLACK);
    Camera3D camera = {
        .position = {3.0f, 2.4f, 4.0f}, .target = {0.0f, 0.85f, 0.0f},
        .up = {0.0f, 1.0f, 0.0f}, .fovy = 3.2f,
        .projection = CAMERA_ORTHOGRAPHIC,
    };
    const float turns[] = {0.0f, PI * 0.5f, PI};
    for (int32_t turn = 0; turn < 3; ++turn) {
        for (int32_t pony = 0; pony < CC_PONY_COUNT; ++pony) {
            BeginTextureMode(scene);
            ClearBackground((Color){24, 28, 34, 255});
            BeginMode3D(camera);
            BeginWorldLighting(camera, &ROAD_ART_COMPOSITION);
            RequireRenderer(DrawCreatureGait3D(
                CC_CREATURE_HORSE, CC_CREATURE_POSE_IDLE, (Vector3){0},
                turns[turn], 0.96f, PONY_COLORS[pony][0], 0.0f, false, NULL),
                "pony review must draw the shipped skin");
            if (turn == 2) DrawRoadHorseHarness((Vector3){0}, turns[turn]);
            EndWorldLighting();
            EndMode3D();
            EndTextureMode();
            BeginDrawing();
            ClearBackground(BLACK);
            PresentTarget(scene, (Rectangle){0, 0, (float)size, (float)size});
            rlDrawRenderBatchActive();
            Image shot = LoadImageFromScreen();
            EndDrawing();
            ImageCrop(&shot, (Rectangle){0, 0, (float)size, (float)size});
            ImageDraw(&sheet, shot, (Rectangle){0, 0, (float)size, (float)size},
                      (Rectangle){(float)(pony * size), (float)(turn * size),
                                  (float)size, (float)size}, WHITE);
            UnloadImage(shot);
        }
    }
    RequireRenderer(ExportImage(sheet, path), "pony review sheet must be written");
    UnloadImage(sheet);
    UnloadRenderTexture(scene);
    TestPonyPortraits(path);
}
