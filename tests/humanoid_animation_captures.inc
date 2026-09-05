/* Fixed-tick review of the production character skin and foot contacts. */
static void CaptureHumanoidAnimation(const char *directory)
{
    RequireRenderer(MakeDirectory(directory) == 0 || DirectoryExists(directory),
                    "animation capture directory must exist");
    const char *labels[] = {"Walk and stop", "Walk through a turn", "Turn in place"};
    CcLocalAgent agents[3] = {0};
    RenderTexture2D scenes[3];
    for (int32_t sample = 0; sample < 3; ++sample) {
        agents[sample].crowned = true;
        agents[sample].grounded = true;
        agents[sample].morphology = CC_MORPHOLOGY_BIPED;
        agents[sample].appearance = CcNpcCrownlessAppearance();
        agents[sample].combat.health = 100.0f;
        CcHumanoidGaitInit(&agents[sample].humanoid, (CcLimbVec3){0},
                          0.0f, FlatSkinProbe, NULL);
        scenes[sample] = LoadRenderTexture(128, 128);
        RequireRenderer(IsRenderTextureValid(scenes[sample]),
                        "animation render targets must load");
    }
    ArtComposition composition = INTERIOR_ART_COMPOSITION;
    composition.light_profile = ART_LIGHT_CLEAR_MARKET;
    for (int32_t tick = 0; tick < 480; ++tick) {
        for (int32_t sample = 0; sample < 3; ++sample) {
            CcLocalAgent *agent = &agents[sample];
            float turn = Clamp(((float)tick - 90.0f) / 60.0f, 0.0f, 1.0f);
            agent->facing_yaw = sample == 0 ? 0.0f : turn * PI * 0.5f;
            float speed = sample < 2 && tick < 270 ? 0.70f : 0.0f;
            CcHumanoidGaitAdvance(&agent->humanoid,
                (CcLimbVec3){agent->position.x, 0.0f, agent->position.z},
                agent->facing_yaw,
                (CcLimbVec3){sinf(agent->facing_yaw) * speed, 0.0f,
                             cosf(agent->facing_yaw) * speed},
                true, 1.0f / 60.0f, FlatSkinProbe, NULL);
            agent->position.x += agent->humanoid.root_velocity.x / 60.0f;
            agent->position.z += agent->humanoid.root_velocity.z / 60.0f;
            CcLocalAgentInterpolateInternal(agent, 1.0f);
        }
        if (tick % 4 != 0) continue;
        for (int32_t sample = 0; sample < 3; ++sample) {
            CcLocalAgent *agent = &agents[sample];
            CcHumanoidSkinPose skin;
            CcHumanoidSkinPoseResolve(AgentRenderPose(agent), &skin);
            Camera3D camera = {
                .position = {agent->position.x + 4.0f, 2.8f,
                             agent->position.z + 6.0f},
                .target = {agent->position.x, 0.95f, agent->position.z},
                .up = {0.0f, 1.0f, 0.0f}, .fovy = 3.2f,
                .projection = CAMERA_ORTHOGRAPHIC,
            };
            BeginTextureMode(scenes[sample]);
            ClearBackground((Color){32, 34, 38, 255});
            SetFaceRenderContext(camera, 128, 128);
            BeginMode3D(camera);
            for (int32_t line = -8; line <= 8; ++line) {
                float coordinate = (float)line;
                Color ground_line = {54, 58, 62, 255};
                DrawLine3D((Vector3){coordinate, 0.0f, -8.0f},
                           (Vector3){coordinate, 0.0f, 8.0f}, ground_line);
                DrawLine3D((Vector3){-8.0f, 0.0f, coordinate},
                           (Vector3){8.0f, 0.0f, coordinate}, ground_line);
            }
            BeginWorldLighting(camera, &composition);
            UseCharacterLighting();
            RequireRenderer(DrawDynamicNpcModules(agent, &skin, &agent->appearance),
                            "the production character modules must render");
            DrawWayfarerHeroDetails(&skin);
            RestoreWorldLighting();
            EndWorldLighting();
            EndMode3D();
            EndTextureMode();
        }
        BeginDrawing();
        ClearBackground((Color){22, 24, 28, 255});
        for (int32_t sample = 0; sample < 3; ++sample) {
            PresentTarget(scenes[sample], (Rectangle){(float)sample * 256.0f,
                                                     24.0f, 256.0f, 256.0f});
        }
        rlDrawRenderBatchActive();
        Image shot = LoadImageFromScreen();
        EndDrawing();
        ImageCrop(&shot, (Rectangle){0, 0, 768, 280});
        for (int32_t sample = 0; sample < 3; ++sample) {
            ImageDrawText(&shot, labels[sample], sample * 256 + 12, 6,
                          16, RAYWHITE);
        }
        char path[1024];
        (void)snprintf(path, sizeof(path), "%s/frame-%03d.png", directory, tick / 4);
        RequireRenderer(ExportImage(shot, path), "animation frame must be written");
        UnloadImage(shot);
    }
    for (int32_t sample = 0; sample < 3; ++sample) {
        RequireRenderer(agents[sample].humanoid.idle.pose_locked,
                        "each animation review scene should finish at rest");
        UnloadRenderTexture(scenes[sample]);
    }
    (void)puts("PASS animation captures: walk, walking turn, standing turn, and stops");
}
