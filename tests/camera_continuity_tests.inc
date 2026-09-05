static void TestCameraContinuity(void)
{
    const int rates[] = {30, 60, 144};
    const uint32_t seeds[] = {UINT32_C(0xc0a71a9e), UINT32_C(0x12345678)};
    float largest_speed = 0.0f;
    for (int seed = 0; seed < 2; ++seed) {
        static CcSim sim;
        CcSimInit(&sim, seeds[seed]);
        for (int town = 0; town < 6; ++town) {
            sim.player.location_id = sim.settlements[town].id;
            CcLocalBindPlace(&sim);
            const CcLocalPlaceProfile *profile = ActivePlaceProfile();
            for (int rate = 0; rate < 3; ++rate) {
                int frames = rates[rate] * 15;
                for (int route = 0; route < CC_LOCAL_CARRIAGE_ROUTE_COUNT; ++route) {
                    const CcLocalPlaceRoad *road = &profile->carriage_route[route];
                    street_camera_rig = (FixedCameraRig){0};
                    Camera3D previous = {0};
                    for (int frame = 0; frame <= frames; ++frame) {
                        float amount = (float)frame / (float)frames;
                        float x = road->x + road->width *
                            (road->runs_east_west ? amount : 0.5f);
                        float z = road->z + road->depth *
                            (road->runs_east_west ? 0.5f : amount);
                        CcLocalAgent hero = {.position = {
                            x, CcLocalTerrainHeightAt(x, z), z}};
                        float clock = (float)frame / (float)rates[rate];
                        Camera3D camera = CcLocalStreetCameraInternal(&hero, clock, true, 320);
                        Vector2 screen = GetWorldToScreenEx(Vector3Add(hero.position,
                            (Vector3){0, 1.05f, 0}), camera, 630, 320);
                        RequireRenderer(isfinite(camera.fovy) && camera.fovy > 0.0f,
                                        "town lens must remain finite");
                        RequireRenderer(screen.x >= 120 && screen.x <= 510 &&
                                        screen.y >= 60 && screen.y <= 260,
                                        "walking hero must stay inside the safe frame");
                        if (frame > 0) {
                            float speed = Vector3Distance(camera.target, previous.target) *
                                          (float)rates[rate];
                            largest_speed = fmaxf(largest_speed, speed);
                            if (speed > 50.0f) {
                                fprintf(stderr, "camera speed %.3f town %d route %d frame %d rate %d\n",
                                        speed, town, route, frame, rates[rate]);
                            }
                            RequireRenderer(speed <= 50.0f,
                                            "town camera must keep a bounded speed through shot changes");
                        }
                        Camera3D paused = CcLocalStreetCameraInternal(&hero, clock, false, 320);
                        RequireRenderer(Vector3Distance(paused.target, camera.target) < 0.0001f,
                                        "paused camera queries must preserve the displayed frame");
                        previous = camera;
                    }
                }
            }
        }
    }
    printf("PASS town camera: six towns, two seeds, 30/60/144 Hz, peak speed %.3f\n", largest_speed);

    FixedCameraRig rig = {0};
    FixedCameraRigAim(&rig, 0, (Vector3){0}, (Vector3){3, 8, 14}, 8, 0, true);
    for (int frame = 1; frame <= 20; ++frame) {
        FixedCameraRigAim(&rig, 1, (Vector3){10, 0, 0}, (Vector3){-3, 8, 14},
                          7, (float)frame / 60.0f, true);
    }
    Vector3 held = rig.displayed_target;
    Vector3 velocity = rig.target_velocity;
    FixedCameraRigAim(&rig, 2, (Vector3){-3, 0, 0}, (Vector3){3, 8, 14}, 8, 0.35f, true);
    RequireRenderer(Vector3Distance(held, rig.displayed_target) < 0.0001f &&
                    Vector3Distance(velocity, rig.target_velocity_from) < 0.0001f,
                    "interrupted town moves must carry position and velocity");

    CcLocalAgent hero = {.position = {0, 0, 0}};
    CcLocalAgent partner = {.position = {2, 0, 0}};
    Camera3D base = ExteriorCameraComposed((Vector3){0, 1, 0}, (Vector3){-9, 8, 11}, 7.2f);
    conversation_camera_rig = (ConversationCameraRig){0};
    Camera3D before = CcLocalConversationCameraInternal(base, &hero, &partner, false, 0, true, 320);
    Camera3D after = CcLocalConversationCameraInternal(base, &hero, &partner, true, 0, false, 320);
    for (int depth = -10; depth <= 5; depth += 5) {
        Vector3 point = {2, 1, (float)depth};
        RequireRenderer(Vector2Distance(GetWorldToScreenEx(point, before, 630, 320),
                                        GetWorldToScreenEx(point, after, 630, 320)) < 0.001f,
                        "conversation entry must preserve landmark screen positions");
    }
    for (int rate = 0; rate < 3; ++rate) {
        conversation_camera_rig = (ConversationCameraRig){0};
        Camera3D previous = base;
        float largest_step = 0;
        for (int frame = 0; frame < rates[rate] * 8; ++frame) {
            float clock = (float)frame / (float)rates[rate];
            Camera3D camera = CcLocalConversationCameraInternal(base, &hero, &partner,
                clock < 2.0f || (clock >= 2.2f && clock < 3.0f), clock, true, 320);
            largest_step = fmaxf(largest_step,
                Vector3Distance(camera.position, previous.position) * (float)rates[rate]);
            RequireRenderer(camera.projection == base.projection,
                            "conversation entry and return must share a lens family");
            previous = camera;
        }
        printf("camera return rate %d peak %.4f residual %.6f\n", rates[rate], largest_step, Vector3Distance(previous.position, base.position));
        RequireRenderer(largest_step < 60.0f && Vector3Distance(previous.position, base.position) < 0.001f,
                        "conversation interruptions and returns must settle smoothly");
    }
    puts("PASS camera handoffs: held landmarks, interrupted moves and smooth returns");
}

static void TestStableVisibility(void)
{
    Camera3D camera = {.position = {0, 2, 10}, .target = {0, 2, 0},
        .up = {0, 1, 0}, .fovy = 43, .projection = CAMERA_PERSPECTIVE};
    NavPlatform behind = {.x = -1, .z = -4, .width = 2, .depth = 1, .height = 2};
    RequireRenderer(CameraStreetPlatformSubjectOverlap(camera, &behind,
                    (Vector3){0, 2, 0}, (Vector3){0.5f, 2, 0}) == 0.0f,
                    "a platform behind both actors must preserve its shape");
    RequireRenderer(CameraBoundsVisible(camera, (Vector3){0, 2, 0}, (Vector3){1, 1, 1},
                    630.0f / 320.0f, 512), "visible bounds must stay in the frame");
    RequireRenderer(!CameraBoundsVisible(camera, (Vector3){0, 2, 20}, (Vector3){1, 1, 1},
                    630.0f / 320.0f, 512), "bounds behind the camera must cull");
    TreeRegionalStyle style = TreeStyleForKingdom(WHITE);
    uint32_t mask = WorldTreeVisibilityMask((Vector3){0}, style);
    street_camera_rig.shot = CC_LOCAL_TOWN_SCENE_LANDMARK;
    RequireRenderer(mask == WorldTreeVisibilityMask((Vector3){100, 0, 100}, style),
                    "authored tree residency must survive camera moves and shot changes");
    puts("PASS scenery visibility: depth ordering, full bounds and stable town tree residency");
}
