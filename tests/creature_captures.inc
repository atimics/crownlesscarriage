typedef struct CreatureReview {
    CcCreatureVariant variant;
    const char *name;
    Vector3 center;
    float span;
    Vector3 face;
    float face_span;
} CreatureReview;

static const CreatureReview CREATURE_REVIEWS[] = {
    {CC_CREATURE_GOBLIN_SCAVENGER, "Scavenger", {0, 1.25f, 0}, 3.0f,
     {0, 1.84f, 0.05f}, 1.75f},
    {CC_CREATURE_GOBLIN_RAIDER, "Raider", {0, 1.25f, 0}, 3.0f,
     {0, 1.84f, 0.05f}, 1.75f},
    {CC_CREATURE_GOBLIN_TRIBUTE_BEARER, "Tribute bearer", {0, 1.25f, 0}, 3.0f,
     {0, 1.84f, 0.05f}, 1.75f},
    {CC_CREATURE_DRAGON_WHELP, "Whelp", {0, 1.1f, -0.1f}, 4.5f,
     {0, 1.38f, 0.95f}, 1.9f},
    {CC_CREATURE_DRAGON_WANDERER, "Wanderer", {0, 1.2f, -0.6f}, 11.0f,
     {0.08f, 1.70f, 3.65f}, 2.6f},
    {CC_CREATURE_DRAGON, "Crowned dragon", {0, 1.3f, -1.8f}, 18.0f,
     {0, 1.80f, 5.30f}, 3.2f},
    {CC_CREATURE_DRAGON_DEEP_WYRM, "Deep wyrm", {0, 1.4f, -3.4f}, 26.0f,
     {0, 2.50f, 6.80f}, 5.1f},
};

static Image CaptureCreatureView(const CreatureReview *review, int32_t size,
                                 float yaw, CcCreaturePose pose, bool face,
                                 CcDragonHairColor hair)
{
    RenderTexture2D scene = LoadRenderTexture(size, size);
    RequireRenderer(IsRenderTextureValid(scene), "creature review target must load");
    Vector3 focus = face ? review->face : review->center;
    focus = LocalPoint((Vector3){0}, focus.x, focus.y, focus.z, yaw);
    float span = face ? review->face_span : review->span;
    Camera3D camera = {
        .position = Vector3Add(focus, Vector3Scale(
            Vector3Normalize((Vector3){1.0f, 0.80f, 1.6f}), 18.0f)),
        .target = focus, .up = {0, 1, 0}, .fovy = span,
        .projection = CAMERA_ORTHOGRAPHIC,
    };
    BeginTextureMode(scene);
    ClearBackground((Color){24, 28, 34, 255});
    BeginMode3D(camera);
    ArtComposition composition = ROAD_ART_COMPOSITION;
    composition.light_profile = ART_LIGHT_CLEAR_MARKET;
    composition.focal_point = focus;
    BeginWorldLighting(camera, &composition);
    float scale = IsDragonCreatureVariant(review->variant) ?
        1.0f / DragonCreatureGrowthScale(review->variant) : 1.0f;
    CcDragon dragon = {.hair_color = hair};
    RequireRenderer(DrawCreature3D(review->variant, pose, (Vector3){0},
                                   yaw, scale, DragonCourtHairColor(&dragon)),
                    "creature review must use the game renderer");
    EndWorldLighting();
    EndMode3D();
    EndTextureMode();
    Image raw = LoadImageFromTexture(scene.texture);
    ImageFlipVertical(&raw);
    Color *pixels = LoadImageColors(raw);
    RequireRenderer(pixels != NULL, "creature pixels must be readable");
    int32_t visible = 0;
    int32_t edge = 0;
    int32_t hair_pixels = 0;
    for (int32_t y = 0; y < size; ++y) {
        for (int32_t x = 0; x < size; ++x) {
            Color color = pixels[y * size + x];
            if (color.r == 24 && color.g == 28 && color.b == 34) continue;
            visible++;
            bool hair_match = hair == CC_DRAGON_HAIR_RED ?
                color.r > color.g + 30 && color.r > color.b + 25 :
                hair == CC_DRAGON_HAIR_BLUE ?
                color.b > color.r + 25 && color.b > color.g + 15 :
                color.b > color.g + 20 && color.r > color.g + 15;
            if (hair_match && y < size / 2) hair_pixels++;
            if (x < 2 || y < 2 || x >= size - 2 || y >= size - 2) edge++;
        }
    }
    RequireRenderer(visible > size, "creature capture must contain visible geometry");
    if (!face) RequireRenderer(edge == 0, "whole creature must fit inside its review frame");
    if (face || review->variant <= CC_CREATURE_GOBLIN_TRIBUTE_BEARER) {
        RequireRenderer(hair_pixels > size / 2, "court hair colour must stay visible");
    }
    UnloadImageColors(pixels);
    UnloadImage(raw);
    BeginDrawing();
    ClearBackground(BLACK);
    PresentTarget(scene, (Rectangle){0, 0, (float)size, (float)size});
    rlDrawRenderBatchActive();
    Image shot = LoadImageFromScreen();
    EndDrawing();
    ImageCrop(&shot, (Rectangle){0, 0, (float)size, (float)size});
    UnloadRenderTexture(scene);
    return shot;
}

static void TestCreatureTurns(void)
{
    /* Keep full silhouettes in frame at game size through every quarter turn. */
    for (int32_t index = 0; index < 7; ++index) {
        const CreatureReview *review = &CREATURE_REVIEWS[index];
        CcCreaturePose moving = IsDragonCreatureVariant(review->variant) ?
            CC_CREATURE_POSE_STALK_A : CC_CREATURE_POSE_PASSING_A;
        for (int32_t turn = 0; turn < 4; ++turn) {
            Image shot = CaptureCreatureView(review, 96, (float)turn * PI * 0.5f,
                                             moving, false, CC_DRAGON_HAIR_PURPLE);
            UnloadImage(shot);
        }
    }
    (void)puts("PASS creature turns: seven variants, four moving turns, visible purple hair");
}

static void TestDragonCourtColors(void)
{
    for (int32_t court = 0; court < CC_DRAGON_HAIR_COLOR_COUNT; ++court) {
        for (int32_t index = 0; index < 7; ++index) {
            Image shot = CaptureCreatureView(&CREATURE_REVIEWS[index], 96, 0,
                CC_CREATURE_POSE_IDLE, true, (CcDragonHairColor)court);
            UnloadImage(shot);
        }
    }
    (void)puts("PASS dragon courts: matching purple, red and blue hair across seven variants");
}

static void TestCreatureEllipsoidTurn(void)
{
    RenderTexture2D scene = LoadRenderTexture(96, 96);
    RequireRenderer(IsRenderTextureValid(scene), "creature turn target must load");
    Camera3D camera = {
        .position = {0, 0, 5}, .target = {0}, .up = {0, 1, 0},
        .fovy = 3, .projection = CAMERA_ORTHOGRAPHIC,
    };
    int32_t widths[2] = {0};
    for (int32_t turn = 0; turn < 2; ++turn) {
        BeginTextureMode(scene);
        ClearBackground(BLACK);
        BeginMode3D(camera);
        DrawCreatureEllipsoid((Vector3){0}, (Vector3){1, 0.5f, 0.2f},
                               (float)turn * PI * 0.5f, WHITE);
        EndMode3D();
        EndTextureMode();
        Image shot = LoadImageFromTexture(scene.texture);
        Color *pixels = LoadImageColors(shot);
        RequireRenderer(pixels != NULL, "creature turn pixels must be readable");
        int32_t left = 96;
        int32_t right = -1;
        for (int32_t y = 0; y < 96; ++y) {
            for (int32_t x = 0; x < 96; ++x) {
                Color color = pixels[y * 96 + x];
                if (color.r + color.g + color.b == 0) continue;
                if (x < left) left = x;
                if (x > right) right = x;
            }
        }
        widths[turn] = right - left + 1;
        UnloadImageColors(pixels);
        UnloadImage(shot);
    }
    RequireRenderer(widths[1] > 0 && widths[0] > widths[1] * 3,
                    "creature ellipsoid silhouette must follow its yaw");
    UnloadRenderTexture(scene);
    (void)puts("PASS creature body rotation: rendered silhouette follows yaw");
}

static void CaptureCreatures(const char *directory)
{
    RequireRenderer(MakeDirectory(directory) == 0 || DirectoryExists(directory),
                    "creature capture directory must exist");
    const int32_t size = 256;
    const float turns[] = {0.0f, PI * 0.5f, 0.0f};
    for (int32_t family = 0; family < 2; ++family) {
        int32_t count = family == 0 ? 3 : 4;
        int32_t first = family == 0 ? 0 : 3;
        Image sheet = GenImageColor(size * count, size * 3 + 28,
                                    (Color){17, 19, 28, 255});
        for (int32_t column = 0; column < count; ++column) {
            const CreatureReview *review = &CREATURE_REVIEWS[first + column];
            ImageDrawText(&sheet, review->name, column * size + 12, 8, 16,
                          (Color){224, 219, 202, 255});
            for (int32_t row = 0; row < 3; ++row) {
                Image shot = CaptureCreatureView(review, size, turns[row],
                    CC_CREATURE_POSE_IDLE, row == 2, CC_DRAGON_HAIR_PURPLE);
                ImageDraw(&sheet, shot, (Rectangle){0, 0, (float)size, (float)size},
                          (Rectangle){(float)(column * size), (float)(row * size + 28),
                                      (float)size, (float)size}, WHITE);
                UnloadImage(shot);
            }
        }
        char path[1024];
        (void)snprintf(path, sizeof(path), "%s/%s.png", directory,
                        family == 0 ? "goblins" : "dragons");
        RequireRenderer(ExportImage(sheet, path), "creature review sheet must be written");
        UnloadImage(sheet);
    }
    Image courts = GenImageColor(size * 3, size * 3 + 28, (Color){17, 19, 28, 255});
    const char *names[] = {"Purple court", "Red court", "Blue court"};
    for (int32_t court = 0; court < CC_DRAGON_HAIR_COLOR_COUNT; ++court) {
        ImageDrawText(&courts, names[court], court * size + 12, 8, 16,
                      (Color){224, 219, 202, 255});
        for (int32_t row = 0; row < 3; ++row) {
            int32_t index = row == 0 ? 4 : row == 1 ? 5 : 0;
            Image shot = CaptureCreatureView(&CREATURE_REVIEWS[index],
                size, row == 0 ? PI * 0.5f : 0, CC_CREATURE_POSE_IDLE,
                row > 0, (CcDragonHairColor)court);
            ImageDraw(&courts, shot, (Rectangle){0, 0, (float)size, (float)size},
                (Rectangle){(float)(court * size), (float)(row * size + 28),
                            (float)size, (float)size}, WHITE);
            UnloadImage(shot);
        }
    }
    char courts_path[1024];
    (void)snprintf(courts_path, sizeof(courts_path), "%s/dragon-courts.png", directory);
    RequireRenderer(ExportImage(courts, courts_path), "dragon court sheet must be written");
    UnloadImage(courts);
    TestCreatureEllipsoidTurn();
    TestCreatureTurns();
    TestDragonCourtColors();
    (void)puts("PASS creature review: three goblin roles, four dragon stages, faces and moving turns");
}
