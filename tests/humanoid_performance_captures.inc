/* This fixture uses the same target, fixed update and skin paths as play. */
static void ReviewAttention(CcLocalAgent *agent, Vector3 point, bool greeting)
{
    CcLocalAgentAttend(agent, point, greeting ? 4.0f : 1.4f, greeting);
}

static void CaptureHumanoidPerformance(const char *directory)
{
    RequireRenderer(MakeDirectory(directory) == 0 || DirectoryExists(directory),
                    "performance capture directory must exist");
    CcLocalAgent actor;
    CcLocalAgentInit(&actor, (Vector2){4.0f, 5.5f}, true);
    actor.facing_yaw = 0.0f;
    CcLocalAgentSetMorphology(&actor, CC_MORPHOLOGY_BIPED, true);
    CcLocalAgent partner;
    CcLocalAgentInit(&partner, (Vector2){8.2f, 5.0f}, true);
    partner.facing_yaw = -PI * 0.5f;
    CcLocalAgentSetNpcAppearance(&partner, 171U, CC_NPC_ROLE_GUARD,
                               (Color){126, 150, 116, 255});
    RenderTexture2D scene = LoadRenderTexture(384, 216);
    RequireRenderer(IsRenderTextureValid(scene), "performance target must load");
    Camera3D camera = {.position = {7.0f, 5.4f, 15.5f},
        .target = {6.2f, 0.8f, 5.0f}, .up = {0, 1, 0}, .fovy = 4.8f,
        .projection = CAMERA_ORTHOGRAPHIC};
    ArtComposition composition = INTERIOR_ART_COMPOSITION;
    composition.light_profile = ART_LIGHT_CLEAR_MARKET;
    const Vector3 look = {8.2f, 1.55f, 5.0f};
    for (int32_t tick = 0; tick < 1200; ++tick) {
        if (tick == 120) ReviewAttention(&actor, look, false);
        if (tick == 210) RequireRenderer(CcLocalAgentSetExactTarget(&actor,
            (Vector3){6.0f, 0.0f, 5.5f}, true), "first review target must work");
        if (tick == 390) RequireRenderer(CcLocalAgentSetExactTarget(&actor,
            (Vector3){7.4f, 0.0f, 4.4f}, true), "second review target must work");
        if (tick == 660) ReviewAttention(&actor, look, true);
        CcLocalAgentFixedStepInternal(&actor, 1.0f / 60.0f, true);
        CcLocalAgentFixedStepInternal(&partner, 1.0f / 60.0f, true);
        CcLocalAgentInterpolateInternal(&actor, 1.0f);
        CcLocalAgentInterpolateInternal(&partner, 1.0f);
        if (tick % 4 != 0) continue;
        BeginTextureMode(scene);
        ClearBackground((Color){35, 39, 43, 255});
        SetFaceRenderContext(camera, 384, 216);
        BeginMode3D(camera);
        for (int32_t line = 0; line <= 16; ++line) {
            float coordinate = (float)line;
            DrawLine3D((Vector3){coordinate, 0, 0},
                       (Vector3){coordinate, 0, 16}, (Color){57, 63, 66, 255});
            DrawLine3D((Vector3){0, 0, coordinate},
                       (Vector3){16, 0, coordinate}, (Color){57, 63, 66, 255});
        }
        BeginWorldLighting(camera, &composition);
        UseCharacterLighting();
        CcLocalAgent *cast[] = {&actor, &partner};
        for (int32_t i = 0; i < 2; ++i) {
            CcHumanoidSkinPose skin;
            CcHumanoidSkinPoseResolveGaze(AgentRenderPose(cast[i]),
                cast[i]->render_head_yaw, cast[i]->render_head_pitch, &skin);
            RequireRenderer(DrawDynamicNpcModules(cast[i], &skin,
                &cast[i]->appearance), "review character modules must render");
            if (i == 0) DrawWayfarerHeroDetails(&skin);
        }
        RestoreWorldLighting();
        EndWorldLighting();
        EndMode3D();
        EndTextureMode();
        BeginDrawing();
        ClearBackground((Color){22, 24, 28, 255});
        PresentTarget(scene, (Rectangle){0, 28, 768, 432});
        rlDrawRenderBatchActive();
        Image shot = LoadImageFromScreen();
        EndDrawing();
        ImageCrop(&shot, (Rectangle){0, 0, 768, 460});
        const char *label = tick < 120 ? "Rest" : tick < 210 ? "Notice" :
            tick < 390 ? "Approach" : tick < 570 ? "Turn" :
            tick < 660 ? "Arrive" : tick < 750 ? "Greet" : "Settle";
        ImageDrawText(&shot, label, 16, 6, 18, RAYWHITE);
        char path[1024];
        (void)snprintf(path, sizeof(path), "%s/frame-%03d.png", directory, tick / 4);
        RequireRenderer(ExportImage(shot, path), "performance frame must be written");
        UnloadImage(shot);
    }
    RequireRenderer(Vector3Distance(actor.position, (Vector3){7.4f, 0, 4.4f}) < 0.10f,
                    "review actor must arrive beside the partner");
    RequireRenderer(actor.humanoid.idle.pose_locked,
                    "review actor must finish at rest");
    UnloadRenderTexture(scene);
    (void)puts("PASS performance capture: notice, approach, turn, arrive, greet and settle");
}
