/* Parse capture requests before opening a window or loading a save. */
static int RunCaptureRequestRegression(void)
{
    CcCaptureRequest request = {0};
    char *normal[] = {"crownless"};
    if (!CcCaptureRequestParse(1, normal, &request) || request.capture)
        return ClientRegressionFailure("Normal startup must select play mode.");
    char *plain[] = {"crownless", "--capture", "proof.png"};
    if (!CcCaptureRequestParse(3, plain, &request) || !request.capture ||
        strcmp(request.capture_path, "proof.png") != 0)
        return ClientRegressionFailure("A plain capture must retain its output path.");
    char *ux[] = {"crownless", "--capture-ux", "14", "ux.png", "1040", "2"};
    if (!CcCaptureRequestParse(6, ux, &request) || !request.capture_ux ||
        !request.capture || request.capture_ux_view != 14 ||
        strcmp(request.capture_path, "ux.png") != 0)
        return ClientRegressionFailure("UX capture must retain its view and path.");
    ux[2] = "20";
    if (CcCaptureRequestParse(6, ux, &request))
        return ClientRegressionFailure("UX capture must enforce its view range.");
    char *storybook[] = {"crownless", "--capture-storybook", "0.25", "0.75", "story.png"};
    if (!CcCaptureRequestParse(5, storybook, &request) || !request.capture ||
        !request.capture_storybook || request.storybook_progress != 0.25f ||
        request.storybook_blend != 0.75f || strcmp(request.capture_path, "story.png") != 0)
        return ClientRegressionFailure("Storybook capture must retain both scene settings.");
    storybook[2] = "nan";
    if (CcCaptureRequestParse(5, storybook, &request))
        return ClientRegressionFailure("Storybook progress must be finite.");
    char *arrival[] = {"crownless", "--capture-road-arrival", "0.5", "arrival.png"};
    if (!CcCaptureRequestParse(4, arrival, &request) || !request.capture ||
        !request.capture_road_arrival || !request.capture_road_zoom ||
        request.capture_road_zoom_weight != 0.5f ||
        strcmp(request.capture_path, "arrival.png") != 0)
        return ClientRegressionFailure("Road arrival must retain the zoom capture alias.");
    if (!CcCaptureRequestParse(1, normal, &request) || request.capture ||
        request.capture_road_arrival || request.capture_storybook)
        return ClientRegressionFailure("Each parse must reset the previous request.");
    (void)puts("PASS capture requests: play, paths, views, scene settings, and ranges");
    return 0;
}

static int capture_test_images;
static char capture_test_last_path[768];

static void RecordCaptureFrame(const char *path)
{
    capture_test_images += 1;
    (void)snprintf(capture_test_last_path, sizeof(capture_test_last_path), "%s", path);
}

static int RunCaptureFrameRegression(void)
{
    struct {
        CcCaptureRequest request;
        int frames;
        int images;
        const char *last_path;
    } cases[] = {
        {{.capture = true}, 3, 1, "frame"},
        {{.capture = true, .capture_ux = true}, 90, 1, "frame"},
        {{.capture = true, .capture_road = true}, 45, 1, "frame"},
        {{.capture = true, .capture_pony_encounter = true}, 45, 1, "frame"},
        {{.capture = true, .capture_pony_swap = true}, 45, 1, "frame"},
        {{.capture = true, .capture_character = true}, 120, 1, "frame"},
        {{.capture = true, .capture_walk_cycle = true}, 10, 8, "frame-07.png"},
        {{.capture = true, .capture_creature_reel = true}, 47, 45, "frame-044.png"},
        {{.capture = true, .capture_gameplay_reel = true}, 6, 4, "frame-003.png"},
        {{.capture = true, .capture_action_reel = true}, 6, 4, "frame-003.png"},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        CcCaptureState state = {0};
        capture_test_images = 0;
        capture_test_last_path[0] = '\0';
        cases[i].request.capture_path = "frame";
        for (int frame = 1; frame <= cases[i].frames; ++frame) {
            state.gameplay_reel.complete = frame == cases[i].frames;
            state.action_reel.complete = frame == cases[i].frames;
            bool finished = CcCaptureAfterFrame(&cases[i].request, &state, RecordCaptureFrame);
            if (finished != (frame == cases[i].frames))
                return ClientRegressionFailure("Capture must finish on the expected frame.");
            if (frame <= 2 && capture_test_images != 0)
                return ClientRegressionFailure("Capture must wait for its warmup frames.");
        }
        if (capture_test_images != cases[i].images ||
            strcmp(capture_test_last_path, cases[i].last_path) != 0)
            return ClientRegressionFailure("Capture image counts and filenames must match.");
    }
    CcCaptureRequest normal = {0};
    CcCaptureState state = {0};
    capture_test_images = 0;
    for (int frame = 0; frame < 125; ++frame) {
        if (CcCaptureAfterFrame(&normal, &state, RecordCaptureFrame))
            return ClientRegressionFailure("Normal play must keep its frame loop running.");
    }
    if (capture_test_images != 0 || state.capture_frames != 0)
        return ClientRegressionFailure("Normal play must keep capture state idle.");
    (void)puts("PASS capture frames: warmup, timing, paths, reels, and normal play");
    return 0;
}

static int RunCaptureSceneRegression(void)
{
    static CcSim sim;
    CcSimInit(&sim, UINT32_C(0xc0a71a9e));
    CcCaptureRequest request = {0};
    uint64_t before = CcSimHash(&sim);
    if (!CcCapturePrepareWorld(&request, &sim, NULL) ||
        !CcCapturePrepareDungeon(&request, &sim) || CcSimHash(&sim) != before)
        return ClientRegressionFailure("Normal startup must preserve its simulation state.");
    request.capture_town = true;
    request.capture_town_index = 2;
    if (!CcCapturePrepareWorld(&request, &sim, NULL) ||
        sim.player.location_id != sim.settlements[2].id)
        return ClientRegressionFailure("Town capture must use the requested settlement.");
    CcSimInit(&sim, UINT32_C(0xc0a71a9e));
    request = (CcCaptureRequest){.capture_underroad = true};
    if (!CcCapturePrepareDungeon(&request, &sim) ||
        !sim.dungeon_expedition.active ||
        sim.dungeon_expedition.current_room != 9 ||
        sim.player.location_id != sim.dungeons[0].settlement_id)
        return ClientRegressionFailure("Underroad capture must enter its prepared expedition.");
    (void)puts("PASS capture scenes: normal state, town selection, and expedition setup");
    return 0;
}

static int RunCapturePresentationRegression(void)
{
    CcCaptureRequest request = {0};
    CcCaptureState state = {0};
    CcCapturePresentation play = CcCapturePresentationFor(&request, &state, true);
    if (!play.adventure_ui || !play.navigation || !play.accept_input || !play.update_convoy ||
        !play.local_panels || !play.combat_status || !play.feedback || !play.save_feedback ||
        !play.context_actions || !play.commands || play.message_step != 0.0f)
        return ClientRegressionFailure("Normal play must retain its input and presentation.");
    char *ux[] = {"crownless", "--capture-ux", "5", "menu.png", "1040"};
    CcCaptureWindow window = {.width = 1280, .height = 760,
        .minimum_width = 1280, .minimum_height = 760, .fps = 60};
    if (!CcCaptureRequestParse(5, ux, &request) ||
        !CcCaptureConfigureWindow(&request, 5, ux, &window) ||
        window.width != 1040 || window.height != 620 ||
        window.minimum_width != 1040 || window.minimum_height != 620 || window.fps != 60)
        return ClientRegressionFailure("UX capture must retain its window dimensions.");
    FrontendState frontend = {.screen = FRONTEND_PLAYING};
    CcCaptureConfigureFrontend(&request, &frontend);
    if (frontend.screen != FRONTEND_PAUSED)
        return ClientRegressionFailure("Menu capture must open the pause screen.");
    ux[4] = "1000";
    if (CcCaptureConfigureWindow(&request, 5, ux, &window))
        return ClientRegressionFailure("UX capture must enforce its window size choices.");
    request = (CcCaptureRequest){.capture_gameplay_reel = true};
    state.gameplay_reel.stage = GAMEPLAY_REEL_QUEST_COMPLETE;
    play = CcCapturePresentationFor(&request, &state, false);
    if (play.combat_status || play.feedback || play.save_feedback || play.context_actions ||
        play.message_step != 1.0f / 15.0f)
        return ClientRegressionFailure("The final reel frame must retain its presentation timing.");
    (void)puts("PASS capture presentation: play, window sizes, menus, and reel completion");
    return 0;
}
