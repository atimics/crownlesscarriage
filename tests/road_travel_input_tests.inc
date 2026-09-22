#include "rlgl.h"
/* Feed the same queued keyboard/pointer input consumed by normal play. */
static int RunRoadTravelInputRegression(const char *capture_directory)
{
    static CcSim sim;
    static LocalState local;
    CcSimInit(&sim, UINT32_C(0xc0a71a9e));
    ResetLocalState(&local);
    char error[256] = "", message[256] = "", save_feedback[256] = "";
    const char *journal_path = "road-input.ccsave";
    (void)remove(journal_path);
    CcJournal *journal = CcJournalStart(journal_path, &sim, error, sizeof(error));
    if (journal == NULL) return ClientRegressionFailure(error);
    int32_t selected = FirstOutgoingRouteIndex(&sim), situation = 0;
    ClientView view = VIEW_ROADS, back = VIEW_LOCAL;
    float save_age = 0;
    if (!InitializeOpenWorld(&sim, &local, false) ||
        !StartOnlyOutgoingRoad(journal, &sim, &local, &view, &selected, error, sizeof(error)))
        return ClientRegressionFailure("Begin the production road journey.");
    local.adventure_ui = true;
    RenderTexture2D target = {0};
    bool capture = capture_directory != NULL;
    if (capture) {
        SetConfigFlags(FLAG_WINDOW_HIDDEN);
        InitWindow(1040, 700, "Continuous road input review");
        SetTraceLogLevel(LOG_WARNING);
        CcLocalRendererInit();
        target = LoadRenderTexture(640, 360);
    }
    uint64_t stopped_hash = 0;
    int32_t at_stop = 0, choices = 0;
    uint64_t waiting_hash = 0;
    bool saw_wait = false;
    for (int f = 0; f < 480; ++f) {
        if (f == 30) queued_key_press[KEY_SPACE] = true;
        if (f == 120) {
            ContextActionSet actions = BuildContextActions(&sim, &local, view, selected, situation);
            int at = -1;
            for (int i = 0; i < actions.count; ++i)
                if (actions.items[i].kind == CONTEXT_ACTION_HOLD_TRAVEL &&
                    strcmp(actions.items[i].label, "Travel") == 0) at = i;
            if (at < 0) return ClientRegressionFailure("One Travel action resumes the stopped journey.");
            Rectangle b = ContextActionBounds(at, actions.count, false);
            client_test_pointer = (Vector2){b.x + b.width/2, b.y + b.height/2};
            client_test_pointer_active = true;
            queued_mouse_button_press[MOUSE_BUTTON_LEFT] = true;
        }
        /* A real fork remains a deliberate choice. Pause on its actual cards,
           then click the onward leg rather than issuing a hidden command. */
        if (f > 120 && sim.journey.road_waiting_choice) {
            uint64_t hash = CcSimHash(&sim);
            if (saw_wait && hash != waiting_hash)
                return ClientRegressionFailure("Waiting at a junction must not spend time.");
            waiting_hash = hash;
            saw_wait = true;
            if (f % 90 == 0) {
                CcRoadLegPreview previews[3];
                int count = CcRoadNextLegPreviews(&sim, previews, 3);
                uint64_t token = 0;
                for (int i=0;i<count;++i)
                    if (previews[i].direction == sim.journey.road_direction &&
                        previews[i].segment_id != CC_PILOT_ROAD_MILL_SEGMENT_ID)
                        token = previews[i].decision_token;
                ContextActionSet actions = BuildContextActions(&sim, &local, view, selected, situation);
                int at = -1;
                for (int i=0;i<actions.count;++i)
                    if (actions.items[i].kind == CONTEXT_ACTION_CHOOSE_ROAD_LEG &&
                        actions.items[i].target.object == token) at = i;
                if (at < 0) return ClientRegressionFailure("Junction has a visible onward choice.");
                local.card_page = at / ContextActionPageSize(&actions);
                int first = ContextCardFirst(&local, &actions);
                Rectangle b = ContextActionBounds(at-first, ContextCardCount(&actions, first), false);
                client_test_pointer = (Vector2){b.x+b.width/2,b.y+b.height/2};
                client_test_pointer_active = true;
                queued_mouse_button_press[MOUSE_BUTTON_LEFT] = true;
                choices++;
                saw_wait = false;
            }
        }
        UpdateTravelHold(&sim, &local, view, selected, situation, (Vector2){0},
                         f >= 30 && f < 120, 1.0f/60);
        HandleInput(&journal, &sim, &selected, &situation, &view, &back, &local,
            target, (Rectangle){0,0,1040,580}, 1.0f/60,
            "road-input-unused.ccsave", "road-input-unused.session", message, sizeof(message),
            save_feedback, sizeof(save_feedback), &save_age);
        ClientInputClearPressed();
        client_test_pointer_active = false;
        if (f == 30) {
            if (!local.carriage_stopped) return ClientRegressionFailure("Space must stop on key press, not release.");
            stopped_hash = CcSimHash(&sim);
            at_stop = sim.journey.elapsed_subticks;
            if (at_stop <= 0) return ClientRegressionFailure("Hands-off travel must make actual progress.");
        }
        if (f > 30 && f < 120 && (CcSimHash(&sim) != stopped_hash || !local.carriage_stopped))
            return ClientRegressionFailure("Holding input must not resume or spend stopped time.");
        if (f == 120 && local.carriage_stopped)
            return ClientRegressionFailure("One pointer click resumes travel.");
        if (capture && f % 4 == 0) {
            CcLocalRendererBeginFrame(1.0f/60);
            BeginDrawing();
            ClearBackground(BACKGROUND);
            CcLocalDrawOpenWorld3D(&sim, &local.world_stream, &local.agent, &local.course,
                &local.world_carriage, (float)f/60, target, (Rectangle){0,100,1040,494});
            CcOverlayBegin(1);
            DrawLocalHeader(&sim, &local, false);
            DrawContextActionTray(&sim, &local, view, selected, situation);
            CcOverlayEnd();
            rlDrawRenderBatchActive();
            Image image = LoadImageFromScreen();
            char path[1024];
            (void)snprintf(path, sizeof(path), "%s/frame-%03d.png", capture_directory, f/4);
            bool saved = ExportImage(image, path);
            UnloadImage(image);
            EndDrawing();
            if (!saved) return ClientRegressionFailure("Save the road input capture.");
        }
    }
    if (sim.journey.elapsed_subticks <= at_stop && sim.journey.active)
        return ClientRegressionFailure("Resumed road must advance, not merely animate.");
    if (choices < 1) return ClientRegressionFailure("Exercise a real named junction choice.");
    /* One explicit stop is harmless to campaign resources and time. */
    uint64_t before = CcSimHash(&sim);
    if (sim.journey.active && !SetRoadTravelStopped(&sim, &local, true, message, sizeof(message))) return 1;
    if (CcSimHash(&sim) != before) return ClientRegressionFailure("Stop consumed campaign resources.");
    if (capture) { UnloadRenderTexture(target); CcLocalRendererShutdown(); CloseWindow(); }
    if (!CcJournalClose(&journal, &sim, error, sizeof(error)))
        return ClientRegressionFailure(error);
    (void)remove(journal_path);
    LeaveOpenWorld(&local);
    puts("PASS real road input: automatic progress, Space stop, held-input stability, pointer resume, resource safety.");
    return 0;
}
