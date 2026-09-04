/* Capture the production modular actor path at small, fixed art sizes. */
static void CaptureCharacterMaterials(const char *directory)
{
    RequireRenderer(MakeDirectory(directory) == 0 || DirectoryExists(directory),
                    "character capture directory must exist");
    char metrics_path[1024];
    (void)snprintf(metrics_path, sizeof(metrics_path), "%s/measurements.csv", directory);
    FILE *metrics = fopen(metrics_path, "w");
    RequireRenderer(metrics != NULL, "character measurements must be writable");
    (void)fprintf(metrics, "actor,light,height,frame,measured_height,visible_pixels\n");
    const int heights[] = {35, 48, 60};
    RenderTexture2D scene = LoadRenderTexture(96, 96);
    RenderTexture2D graded = LoadRenderTexture(96, 96);
    RequireRenderer(IsRenderTextureValid(scene) && IsRenderTextureValid(graded),
                    "character capture targets must load");
    for (int actor = 0; actor < 2; ++actor) {
        for (int light = 0; light < 2; ++light) {
            ArtComposition composition = INTERIOR_ART_COMPOSITION;
            composition.light_profile = light == 0 ? ART_LIGHT_CLEAR_MARKET :
                                                    ART_LIGHT_INTERIOR_EMBER;
            for (int size = 0; size < 3; ++size) {
                for (int frame = 0; frame < 8; ++frame) {
                    CcLocalAgent agent = {0};
                    agent.crowned = actor == 0;
                    agent.appearance = actor == 0 ? CcNpcCrownlessAppearance() :
                        CcNpcAppearanceGenerate(271U, CC_NPC_ROLE_GUARD, GOLD);
                    agent.combat.health = 100.0f;
                    agent.facing_yaw = (float)frame * PI * 0.25f;
                    CcHumanoidGaitInit(&agent.humanoid, (CcLimbVec3){0},
                        agent.facing_yaw, FlatSkinProbe, NULL);
                    for (int tick = 0; tick < 24 + frame * 6; ++tick) {
                        CcHumanoidGaitAdvance(&agent.humanoid,
                            (CcLimbVec3){0}, agent.facing_yaw,
                            (CcLimbVec3){sinf(agent.facing_yaw), 0.0f,
                                         cosf(agent.facing_yaw)},
                            true, 1.0f / 60.0f, FlatSkinProbe, NULL);
                    }
                    CcHumanoidSkinPose skin;
                    CcHumanoidSkinPoseResolve(&agent.humanoid.pose, &skin);
                    Camera3D camera = {
                        .position = {3.0f, 2.5f, 7.0f},
                        .target = {0.0f, 1.05f, 0.0f},
                        .up = {0.0f, 1.0f, 0.0f},
                        .fovy = 2.25f * 96.0f / (float)heights[size],
                        .projection = CAMERA_ORTHOGRAPHIC,
                    };
                    int measured_height = 0;
                    int visible_pixels = 0;
                    for (int attempt = 0; attempt < 4; ++attempt) {
                        rlDrawRenderBatchActive();
                        BeginTextureMode(scene);
                        ClearBackground((Color){24, 28, 34, 255});
                        SetFaceRenderContext(camera, 96, 96);
                        BeginMode3D(camera);
                        BeginWorldLighting(camera, &composition);
                        UseCharacterLighting();
                        RequireRenderer(DrawDynamicNpcModules(
                            &agent, &skin, &agent.appearance),
                            "the production hero and guard modules must render");
                        if (agent.crowned) DrawWayfarerHeroDetails(&skin);
                        RestoreWorldLighting();
                        EndWorldLighting();
                        EndMode3D();
                        EndTextureMode();
                        Image raw = LoadImageFromTexture(scene.texture);
                        Color *pixels = LoadImageColors(raw);
                        RequireRenderer(pixels != NULL, "character pixels must be readable");
                        int top = 96;
                        int bottom = -1;
                        visible_pixels = 0;
                        for (int pixel = 0; pixel < 96 * 96; ++pixel) {
                            Color color = pixels[pixel];
                            if (color.r == 24 && color.g == 28 && color.b == 34) continue;
                            int y = pixel / 96;
                            if (y < top) top = y;
                            if (y > bottom) bottom = y;
                            visible_pixels++;
                        }
                        measured_height = bottom - top + 1;
                        UnloadImageColors(pixels);
                        UnloadImage(raw);
                        RequireRenderer(visible_pixels > 40 && top > 0 && bottom < 95,
                                        "the complete character must be visible");
                        if (abs(measured_height - heights[size]) <= 1) break;
                        camera.fovy *= (float)measured_height / (float)heights[size];
                    }
                    RequireRenderer(abs(measured_height - heights[size]) <= 2,
                                    "character captures must match the requested art height");
                    BeginTextureMode(graded);
                    ClearBackground(BLACK);
                    PresentTarget(scene, (Rectangle){0, 0, 96, 96});
                    EndTextureMode();
                    Image shot = LoadImageFromTexture(graded.texture);
                    ImageFlipVertical(&shot);
                    char path[1024];
                    (void)snprintf(path, sizeof(path), "%s/%s-%s-%d-%d.png",
                        directory, actor == 0 ? "hero" : "guard",
                        light == 0 ? "day" : "interior", heights[size], frame);
                    RequireRenderer(ExportImage(shot, path),
                                    "character capture must be written");
                    (void)fprintf(metrics, "%s,%s,%d,%d,%d,%d\n",
                        actor == 0 ? "hero" : "guard", light == 0 ? "day" : "interior",
                        heights[size], frame, measured_height, visible_pixels);
                    UnloadImage(shot);
                }
            }
        }
    }
    UnloadRenderTexture(scene);
    UnloadRenderTexture(graded);
    (void)fclose(metrics);
    (void)puts("PASS character captures: hero and guard, two lights, three sizes, eight walking turns");
}
