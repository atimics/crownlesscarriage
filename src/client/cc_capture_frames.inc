/* Frame state belongs to the capture harness. */
typedef struct CcCaptureState {
    CcLocalAgent walk_cycle_frames[8];
    ActionReelState action_reel;
    GameplayReelState gameplay_reel;
    int capture_frames;
    int walk_frame_count;
} CcCaptureState;

#if defined(CC_CLIENT_SELF_TESTS)
static void ClientTakeScreenshot(const char *path)
{
#if defined(__APPLE__)
    Image frame = LoadImageFromScreen();
    if (frame.data != NULL) {
        if (frame.width != GetScreenWidth() || frame.height != GetScreenHeight())
            ImageResize(&frame, GetScreenWidth(), GetScreenHeight());
        (void)ExportImage(frame, path);
        UnloadImage(frame);
    }
#else
    TakeScreenshot(path);
#endif
#if defined(PLATFORM_WEB)
    (void)remove(path);
#endif
}

static bool CcCaptureBeforeFrame(const CcCaptureRequest *request,
    CcCaptureState *state, CcSim *sim, LocalState *local,
    int32_t *selected, int32_t *selected_situation, ClientView *view,
    ClientView *return_view, char *message, size_t message_size)
{
    if (request->capture_walk_cycle) {
        local->agent = state->walk_cycle_frames[state->walk_frame_count];
    } else if (request->capture_gameplay_reel) {
        for (int32_t step = 0;
             step < 4 && !state->gameplay_reel.complete; ++step) {
            UpdateGameplayReel(sim, local, &state->gameplay_reel,
                               selected, selected_situation,
                               view, return_view, message,
                               message_size);
        }
    } else if (request->capture_action_reel) {
        for (int32_t step = 0; step < 4 && !state->action_reel.complete; ++step) {
            UpdateActionReel(local, &state->action_reel, message,
                             message_size);
        }
    }
    return request->capture_walk_cycle || request->capture_gameplay_reel ||
    request->capture_action_reel;
}

static bool CcCaptureAfterFrame(const CcCaptureRequest *request,
    CcCaptureState *state, void (*take_screenshot)(const char *))
{
    if (request->capture_gameplay_reel) {
        state->capture_frames += 1;
        if (state->capture_frames <= 2) return false;
        char frame_path[768];
        (void)snprintf(frame_path, sizeof(frame_path), "%s-%03d.png",
                       request->capture_path, state->gameplay_reel.captured_frames);
        take_screenshot(frame_path);
        state->gameplay_reel.captured_frames += 1;
        if (state->gameplay_reel.complete) return true;
    } else if (request->capture_action_reel) {
        state->capture_frames += 1;
        if (state->capture_frames <= 2) return false;
        char frame_path[768];
        (void)snprintf(frame_path, sizeof(frame_path), "%s-%03d.png",
                       request->capture_path, state->action_reel.captured_frames);
        take_screenshot(frame_path);
        state->action_reel.captured_frames += 1;
        if (state->action_reel.complete) return true;
    } else if (request->capture_walk_cycle) {
        state->capture_frames += 1;
        if (state->capture_frames <= 2) return false;
        char frame_path[768];
        (void)snprintf(frame_path, sizeof(frame_path), "%s-%02d.png",
                       request->capture_path, state->walk_frame_count);
        take_screenshot(frame_path);
        state->walk_frame_count += 1;
        if (state->walk_frame_count >= 8) return true;
    } else if (request->capture_creature_reel) {
        state->capture_frames += 1;
        if (state->capture_frames <= 2) return false;
        int32_t creature_frame = state->capture_frames - 3;
        char frame_path[768];
        (void)snprintf(frame_path, sizeof(frame_path), "%s-%03d.png",
                       request->capture_path, creature_frame);
        take_screenshot(frame_path);
        if (creature_frame >= 44) return true;
    } else if (request->capture) {
        state->capture_frames += 1;
        int32_t settled_frames = request->capture_ux ? 90 : request->capture_road || request->capture_pony_encounter || request->capture_pony_swap ? 45 :
                                 request->capture_character ? 120 : 3;
        if (state->capture_frames >= settled_frames) {
            take_screenshot(request->capture_path);
            return true;
        }
    }
    return false;
}

static float CcCaptureClock(const CcCaptureRequest *request,
                            const CcCaptureState *state, float clock)
{
    return request->capture_gameplay_reel ?
        (float)state->gameplay_reel.captured_frames / 15.0f :
        request->capture_creature_reel ? (float)state->capture_frames / 15.0f : clock;
}

static void CcCaptureDrawOverlay(const CcCaptureRequest *request,
                                  const CcCaptureState *state)
{
    if (request->capture_gameplay_reel) {
        CcOverlayFlush();
        DrawGameplayReelQuestComplete(&state->gameplay_reel);
        DrawGameplayReelTransition(&state->gameplay_reel);
    }
}

static void CcCaptureReport(const CcCaptureRequest *request,
                            const CcCaptureState *state)
{
    if (request->capture_walk_cycle) {
        (void)printf("captured %d walk-cycle frames with prefix %s\n",
                     state->walk_frame_count, request->capture_path);
    } else if (request->capture_gameplay_reel) {
        (void)printf("captured %d real-gameplay reel frames with prefix %s\n",
                     state->gameplay_reel.captured_frames, request->capture_path);
    } else if (request->capture_action_reel) {
        (void)printf("captured %d heroic action-reel frames with prefix %s\n",
                     state->action_reel.captured_frames, request->capture_path);
    } else if (request->capture) {
        (void)printf("captured %s\n", request->capture_path);
    }
}
#else
#define CcCaptureClock(request, state, clock) (clock)
#define CcCaptureDrawOverlay(...) ((void)0)
#define CcCaptureBeforeFrame(...) false
#define CcCaptureAfterFrame(...) false
#define CcCaptureReport(...) ((void)0)
#endif
