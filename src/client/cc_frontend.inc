/* Title and pause screens share the same world and journal as gameplay. */
typedef enum FrontendScreen {
    FRONTEND_TITLE,
    FRONTEND_PLAYING,
    FRONTEND_PAUSED,
    FRONTEND_DELETE
} FrontendScreen;

typedef enum FrontendAction {
    FRONTEND_ACTION_NONE,
    FRONTEND_ACTION_OFFLINE,
    FRONTEND_ACTION_ONLINE,
    FRONTEND_ACTION_PLAY,
    FRONTEND_ACTION_RESUME,
    FRONTEND_ACTION_SAVE,
    FRONTEND_ACTION_SOUND,
    FRONTEND_ACTION_MOTION,
    FRONTEND_ACTION_TITLE,
    FRONTEND_ACTION_DELETE,
    FRONTEND_ACTION_CANCEL,
    FRONTEND_ACTION_CONFIRM_DELETE,
    FRONTEND_ACTION_QUIT
} FrontendAction;

typedef struct FrontendState {
    FrontendScreen screen;
    bool online;
    bool has_world;
    bool needs_initial_save;
    bool quit;
    int focus;
    char feedback[256];
} FrontendState;

typedef struct FrontendItem {
    FrontendAction action;
    Rectangle bounds;
    const char *label;
    bool enabled;
} FrontendItem;

static Rectangle FrontendPanel(void)
{
    return (Rectangle){((float)GetScreenWidth() - 440.0f) * 0.5f,
                       ((float)GetScreenHeight() - 556.0f) * 0.5f,
                       440.0f, 556.0f};
}

static int FrontendItems(const FrontendState *state, FrontendItem *items)
{
    Rectangle panel = FrontendPanel();
    float x = panel.x + 36.0f;
    float y = panel.y + 148.0f;
    if (state->screen == FRONTEND_TITLE) {
        items[0] = (FrontendItem){FRONTEND_ACTION_OFFLINE,
            {x, y + 20.0f, 179.0f, 44.0f}, "Offline", true};
        items[1] = (FrontendItem){FRONTEND_ACTION_ONLINE,
            {x + 189.0f, y + 20.0f, 179.0f, 44.0f}, "Online", true};
        items[2] = (FrontendItem){FRONTEND_ACTION_PLAY,
            {x, y + 138.0f, 368.0f, 52.0f}, "Play", true};
        items[3] = (FrontendItem){FRONTEND_ACTION_QUIT,
            {x, y + 246.0f, 368.0f, 40.0f}, "Quit game", true};
#if defined(PLATFORM_WEB)
        return 3;
#else
        return 4;
#endif
    }
    if (state->screen == FRONTEND_DELETE) {
        items[0] = (FrontendItem){FRONTEND_ACTION_CANCEL,
            {x, y + 120.0f, 368.0f, 44.0f}, "Keep world", true};
        items[1] = (FrontendItem){FRONTEND_ACTION_CONFIRM_DELETE,
            {x, y + 180.0f, 368.0f, 44.0f}, "Delete world", true};
        return 2;
    }
    const FrontendAction actions[] = {
        FRONTEND_ACTION_RESUME, FRONTEND_ACTION_SAVE,
        FRONTEND_ACTION_SOUND, FRONTEND_ACTION_MOTION,
        FRONTEND_ACTION_TITLE, FRONTEND_ACTION_DELETE, FRONTEND_ACTION_QUIT
    };
    const char *labels[] = {
        "Resume", "Save world", "Sound", "Reduced motion",
        "Save and return to title", "Delete world", "Save and quit"
    };
    int count = 7;
#if defined(PLATFORM_WEB)
    count = 6;
#endif
    for (int i = 0; i < count; ++i) {
        items[i] = (FrontendItem){actions[i],
            {x, y + (float)i * 45.0f, 368.0f, 36.0f}, labels[i], true};
    }
    if (CcCoopClientActive()) {
        items[4].label = "Leave to title";
        items[5].enabled = CcCoopClientOwner();
        if (!items[5].enabled) items[5].label = "Host can delete world";
    }
    return count;
}

static void FrontendOpen(FrontendState *state, FrontendScreen screen)
{
    state->screen = screen;
    state->focus = screen == FRONTEND_TITLE ? 2 : 0;
    state->feedback[0] = '\0';
}

static FrontendAction FrontendInput(FrontendState *state)
{
    if (ClientKeyPressed(KEY_ESCAPE)) {
        if (state->screen == FRONTEND_PLAYING) {
            FrontendOpen(state, FRONTEND_PAUSED);
        } else if (state->screen == FRONTEND_PAUSED) {
            return FRONTEND_ACTION_RESUME;
        } else if (state->screen == FRONTEND_DELETE) {
            return FRONTEND_ACTION_CANCEL;
        }
        return FRONTEND_ACTION_NONE;
    }
    if (state->screen == FRONTEND_PLAYING) return FRONTEND_ACTION_NONE;
    FrontendItem items[7];
    int count = FrontendItems(state, items);
    if (state->focus < 0 || state->focus >= count) state->focus = 0;
    int direction = ClientKeyPressed(KEY_UP) ? -1 :
        (ClientKeyPressed(KEY_DOWN) || ClientKeyPressed(KEY_TAB)) ? 1 : 0;
    if (direction != 0) {
        do {
            state->focus = (state->focus + direction + count) % count;
        } while (!items[state->focus].enabled);
    }
    if (state->screen == FRONTEND_TITLE &&
        (ClientKeyPressed(KEY_LEFT) || ClientKeyPressed(KEY_RIGHT))) {
        return state->online ? FRONTEND_ACTION_OFFLINE : FRONTEND_ACTION_ONLINE;
    }
    if (ClientMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        for (int i = 0; i < count; ++i) {
            if (CheckCollisionPointRec(GetMousePosition(), items[i].bounds)) {
                state->focus = i;
                return items[i].enabled ? items[i].action : FRONTEND_ACTION_NONE;
            }
        }
    }
    if ((ClientKeyPressed(KEY_ENTER) || ClientKeyPressed(KEY_SPACE)) &&
        items[state->focus].enabled) return items[state->focus].action;
    return FRONTEND_ACTION_NONE;
}

static bool RemoveWorldFile(const char *path)
{
    return remove(path) == 0 || errno == ENOENT;
}

static bool DeleteClientWorld(CcJournal **journal, CcSim *sim,
                              const char *save_path, const char *session_path,
                              char *error, size_t capacity)
{
#if defined(PLATFORM_WEB)
    if (ClientBrowserCampaignAccess() != 0) {
        (void)snprintf(error, capacity, "%s",
            ClientBrowserCampaignAccessMessage(ClientBrowserCampaignAccess()));
        return false;
    }
#endif
    if (!CcJournalClose(journal, sim, error, capacity)) return false;
#if defined(PLATFORM_WEB)
    if (ClientDeleteBrowserCampaign() == 0) {
        *journal = CcJournalResume(save_path, sim, error, capacity);
        (void)snprintf(error, capacity, "World deletion failed. Try again.");
        return false;
    }
#endif
    if (!RemoveWorldFile(save_path)) {
        *journal = CcJournalResume(save_path, sim, error, capacity);
        (void)snprintf(error, capacity, "World deletion failed. Check save folder access.");
        return false;
    }
    /* Play also clears a leftover session before creating the next world. */
    if (!RemoveWorldFile(session_path)) {
        (void)snprintf(error, capacity, "World deleted. Check save folder access before Play.");
    } else {
        (void)snprintf(error, capacity, "World deleted. Play begins a new journey.");
    }
    return true;
}

static void HandleFrontendAction(FrontendState *state, FrontendAction action,
    CcJournal **journal, CcSim *sim, LocalState *local,
    ClientView *view, ClientView *return_view, int32_t *selected,
    int32_t *selected_situation, const char *save_path, const char *session_path)
{
    if (action == FRONTEND_ACTION_OFFLINE || action == FRONTEND_ACTION_ONLINE) {
        state->online = action == FRONTEND_ACTION_ONLINE;
        state->focus = state->online ? 1 : 2;
        state->feedback[0] = '\0';
    } else if (action == FRONTEND_ACTION_PLAY && state->online) {
        CcCoopClientOpenLobby();
        (void)snprintf(state->feedback, sizeof(state->feedback), "The online lobby opens in your browser.");
    } else if (action == FRONTEND_ACTION_PLAY) {
#if defined(PLATFORM_WEB)
        if (ClientBrowserCampaignAccess() != 0) {
            (void)snprintf(state->feedback, sizeof(state->feedback), "%s",
                ClientBrowserCampaignAccessMessage(ClientBrowserCampaignAccess()));
            return;
        }
#endif
        if (!state->has_world) {
            if (!RemoveWorldFile(session_path)) {
                (void)snprintf(state->feedback, sizeof(state->feedback),
                               "Check save folder access, then try Play again.");
                return;
            }
            *journal = CcJournalStart(save_path, sim, state->feedback,
                                      sizeof(state->feedback));
            state->has_world = *journal != NULL;
            state->needs_initial_save = state->has_world;
            if (*journal != NULL && OpeningRequired(sim) && !local->open_world) {
                BeginOpening(local);
            }
        }
        if (*journal == NULL) {
            if (state->feedback[0] == '\0') {
                (void)snprintf(state->feedback, sizeof(state->feedback),
                               "Check the saved world, then restart the game.");
            }
            return;
        }
        if (state->needs_initial_save) {
            if (!SaveClientWorld(*journal, sim, local, save_path, session_path,
                                  state->feedback, sizeof(state->feedback))) return;
            state->needs_initial_save = false;
        }
        FrontendOpen(state, FRONTEND_PLAYING);
    } else if (action == FRONTEND_ACTION_RESUME) {
        FrontendOpen(state, FRONTEND_PLAYING);
    } else if (action == FRONTEND_ACTION_DELETE) {
        FrontendOpen(state, FRONTEND_DELETE);
    } else if (action == FRONTEND_ACTION_CANCEL) {
        FrontendOpen(state, FRONTEND_PAUSED);
    } else if (action == FRONTEND_ACTION_CONFIRM_DELETE) {
        if (CcCoopClientActive()) {
            if (CcCoopClientDelete(state->feedback, sizeof(state->feedback))) {
                CcCoopClientReturnToTitle();
            }
            return;
        }
        if (DeleteClientWorld(journal, sim, save_path, session_path,
                               state->feedback, sizeof(state->feedback))) {
            state->has_world = false;
            state->needs_initial_save = false;
            state->screen = FRONTEND_TITLE;
            state->focus = 2;
            CcSimInit(sim, sim->world_seed + UINT32_C(0x9e3779b9));
            CcLocalTerrainSetSeed(sim->world_seed);
            CcLocalBindPlace(sim);
            LeaveOpenWorld(local);
            ResetLocalState(local);
            (void)InitializeOpenWorld(sim, local, false);
            *selected = FirstOutgoingRouteIndex(sim);
            *selected_situation = FirstActiveSituationIndex(sim);
            *view = VIEW_LOCAL;
            *return_view = VIEW_LOCAL;
        }
    } else if (action == FRONTEND_ACTION_SAVE ||
               action == FRONTEND_ACTION_TITLE || action == FRONTEND_ACTION_QUIT) {
        if (action == FRONTEND_ACTION_TITLE && CcCoopClientActive()) {
            CcCoopClientReturnToTitle();
            return;
        }
        /* The title already holds a saved or freshly loaded world. */
        bool saved = state->screen == FRONTEND_TITLE ||
            (*journal != NULL && SaveClientWorld(*journal, sim, local,
                save_path, session_path, state->feedback, sizeof(state->feedback)));
        if (saved && action == FRONTEND_ACTION_TITLE) {
            FrontendOpen(state, FRONTEND_TITLE);
        } else if (saved && action == FRONTEND_ACTION_QUIT) {
            state->quit = true;
        }
    }
}

static void FrontendCenteredText(const char *text, int y, int size, Color color)
{
    DrawText(text, (GetScreenWidth() - MeasureText(text, size)) / 2, y, size, color);
}

static void DrawFrontend(const FrontendState *state,
                         const CcClientPreferences *preferences, const CcSim *sim)
{
    Rectangle panel = FrontendPanel();
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BACKGROUND, 0.86f));
    DrawRectangleRounded(panel, 0.025f, 6, PANEL_DEEP);
    DrawRectangleRoundedLinesEx(panel, 0.025f, 6, 1.0f, Fade(CC_GOLD, 0.46f));
    int top = (int)panel.y;
    FrontendCenteredText("CROWNLESS CARRIAGE", top + 38, 14, CC_GOLD);
    const char *title = state->screen == FRONTEND_TITLE ? "The road awaits" :
        state->screen == FRONTEND_DELETE ? "Delete this world?" :
        CcCoopClientActive() ? "Company menu" : "Journey paused";
    FrontendCenteredText(title, top + 76, 28, INK);
    if (state->screen == FRONTEND_TITLE) {
        FrontendCenteredText("Choose how to play", top + 126, 14, MUTED);
        FrontendCenteredText(state->online ? "Share a carriage with your company." :
            "Your world is saved on this device.", top + 232, 14, MUTED);
        FrontendCenteredText(state->online ? "Play opens the online lobby." :
            state->has_world ? "Continue your saved journey." : "Begin a new world.",
            top + 354, 14, MUTED);
    } else if (state->screen == FRONTEND_DELETE) {
        FrontendCenteredText(CcCoopClientActive() ? "This removes the shared world and progress" :
            "This removes your world and its progress", top + 165, 16, INK);
        FrontendCenteredText(CcCoopClientActive() ? "for everyone in the company." :
            "from this device.", top + 189, 16, INK);
        FrontendCenteredText("Your next Play begins a new world.", top + 224, 14, MUTED);
    } else {
        FrontendCenteredText(CcCoopClientActive() ? "ONLINE  /  THE COMPANY KEEPS PLAYING" :
            TextFormat("OFFLINE  /  DAY %d", sim->current_day),
                             top + 120, 12, MUTED);
    }
    FrontendItem items[7];
    int count = FrontendItems(state, items);
    for (int i = 0; i < count; ++i) {
        FrontendItem item = items[i];
        bool selected = (item.action == FRONTEND_ACTION_ONLINE && state->online) ||
                        (item.action == FRONTEND_ACTION_OFFLINE && !state->online);
        bool danger = item.action == FRONTEND_ACTION_DELETE ||
                      item.action == FRONTEND_ACTION_CONFIRM_DELETE;
        bool primary = item.action == FRONTEND_ACTION_PLAY ||
                       item.action == FRONTEND_ACTION_RESUME;
        bool hover = CheckCollisionPointRec(GetMousePosition(), item.bounds);
        Color accent = danger ? DANGER : selected || primary ? TEAL : CC_GOLD;
        DrawRectangleRounded(item.bounds, 0.12f, 4,
            Fade(hover || selected || primary ? PANEL_HOVER : PANEL,
                 item.enabled ? 1.0f : 0.45f));
        DrawRectangleRoundedLinesEx(item.bounds, 0.12f, 4,
            i == state->focus ? 2.0f : 1.0f,
            Fade(accent, item.enabled ? (i == state->focus || selected ? 1.0f : 0.45f) : 0.18f));
        const char *label = item.label;
        if (item.action == FRONTEND_ACTION_SOUND) {
            label = preferences->audio_mode == 0 ? "Sound: full" :
                    preferences->audio_mode == 1 ? "Sound: effects" : "Sound: muted";
        } else if (item.action == FRONTEND_ACTION_MOTION) {
            label = preferences->reduced_motion ? "Reduced motion: on" : "Reduced motion: off";
        }
        int size = primary ? 20 : 16;
        DrawText(label, (int)(item.bounds.x + (item.bounds.width -
            (float)MeasureText(label, size)) * 0.5f),
            (int)(item.bounds.y + (item.bounds.height - (float)size) * 0.5f),
            size, Fade(danger ? DANGER : INK, item.enabled ? 1.0f : 0.35f));
    }
    if (state->feedback[0] != '\0') {
        /* Wrap long save errors inside the panel. */
        char line[64];
        size_t length = strlen(state->feedback);
        for (size_t start = 0; start < length && start < 150U; start += 50U) {
            (void)snprintf(line, sizeof(line), "%.50s", state->feedback + start);
            FrontendCenteredText(line, top + 478 + (int)(start / 50U) * 15, 12, CC_GOLD);
        }
    }
    FrontendCenteredText(state->screen == FRONTEND_TITLE ?
        "ARROWS / TAB  Choose     ENTER  Select" :
        "ESC  Back     ARROWS / TAB  Choose     ENTER  Select", top + 531, 10, MUTED);
}

#if defined(CC_CLIENT_SELF_TESTS)
static int RunFrontendRegression(void)
{
    const char *save_path = "frontend-test.ccsave";
    const char *session_path = "frontend-test.ccsave.session";
    const char *preferences_path = "frontend-test.ccsave.preferences";
    (void)remove(save_path);
    (void)remove(session_path);
    static CcSim sim;
    CcSimInit(&sim, UINT32_C(0xc0a71a9e));
    LocalState local = {0};
    ResetLocalState(&local);
    CcLocalBindPlace(&sim);
    CcJournal *journal = NULL;
    ClientView view = VIEW_LOCAL;
    ClientView return_view = VIEW_LOCAL;
    int32_t selected = FirstOutgoingRouteIndex(&sim);
    int32_t selected_situation = FirstActiveSituationIndex(&sim);
    FrontendState state = {.screen = FRONTEND_TITLE, .focus = 2};
#define FRONTEND_CHECK(condition) do { \
    if (!(condition)) { \
        (void)fprintf(stderr, "Frontend check failed at line %d: %s\n", __LINE__, #condition); \
        CcJournalAbandon(&journal); return 1; \
    } \
} while (0)
#define FRONTEND_ACT(action) HandleFrontendAction(&state, action, &journal, &sim, \
    &local, &view, &return_view, &selected, &selected_situation, save_path, session_path)
    FRONTEND_CHECK(!CampaignSaveExists(save_path));
    FRONTEND_ACT(FRONTEND_ACTION_ONLINE);
    FrontendItem items[7];
    (void)FrontendItems(&state, items);
    FRONTEND_CHECK(items[2].enabled && items[2].action == FRONTEND_ACTION_PLAY);
    FRONTEND_CHECK(state.screen == FRONTEND_TITLE && journal == NULL);
    FRONTEND_CHECK(!CampaignSaveExists(save_path));
    FRONTEND_ACT(FRONTEND_ACTION_OFFLINE);
    FRONTEND_ACT(FRONTEND_ACTION_PLAY);
    FRONTEND_CHECK(state.screen == FRONTEND_PLAYING && state.has_world && journal != NULL);
    FRONTEND_CHECK(CampaignSaveExists(save_path));
    uint32_t first_seed = sim.world_seed;
    uint64_t first_tick = sim.clock.tick;
    view = VIEW_MAP;
    queued_key_press[KEY_ESCAPE] = true;
    FRONTEND_CHECK(FrontendInput(&state) == FRONTEND_ACTION_NONE);
    ClientInputClearPressed();
    FRONTEND_CHECK(state.screen == FRONTEND_PAUSED && view == VIEW_MAP);
    FRONTEND_ACT(FRONTEND_ACTION_DELETE);
    FRONTEND_CHECK(state.screen == FRONTEND_DELETE && state.focus == 0);
    FRONTEND_ACT(FRONTEND_ACTION_CANCEL);
    FRONTEND_CHECK(state.screen == FRONTEND_PAUSED && sim.world_seed == first_seed);
    FRONTEND_CHECK(CampaignSaveExists(save_path) && sim.clock.tick == first_tick);
    FRONTEND_ACT(FRONTEND_ACTION_RESUME);
    FRONTEND_CHECK(state.screen == FRONTEND_PLAYING && view == VIEW_MAP);
    FrontendOpen(&state, FRONTEND_PAUSED);
    local.site_travel_active = true;
    FRONTEND_ACT(FRONTEND_ACTION_TITLE);
    FRONTEND_CHECK(state.screen == FRONTEND_PAUSED && state.feedback[0] != '\0');
    local.site_travel_active = false;
    FRONTEND_ACT(FRONTEND_ACTION_TITLE);
    FRONTEND_CHECK(state.screen == FRONTEND_TITLE && CampaignSaveExists(session_path));
    FRONTEND_ACT(FRONTEND_ACTION_PLAY);
    FRONTEND_CHECK(state.screen == FRONTEND_PLAYING && sim.world_seed == first_seed);
    CcClientPreferences preferences = {.reduced_motion = true, .audio_mode = 2};
    char error[256];
    FRONTEND_CHECK(CcClientPreferencesSave(preferences_path, &preferences, error, sizeof(error)));
    FrontendOpen(&state, FRONTEND_PAUSED);
    FRONTEND_ACT(FRONTEND_ACTION_DELETE);
    FRONTEND_ACT(FRONTEND_ACTION_CONFIRM_DELETE);
    FRONTEND_CHECK(state.screen == FRONTEND_TITLE && !state.has_world && journal == NULL);
    FRONTEND_CHECK(!CampaignSaveExists(save_path) && !CampaignSaveExists(session_path));
    FRONTEND_CHECK(CampaignSaveExists(preferences_path) && sim.world_seed != first_seed);
    uint32_t second_seed = sim.world_seed;
    FRONTEND_ACT(FRONTEND_ACTION_PLAY);
    FRONTEND_CHECK(state.screen == FRONTEND_PLAYING && state.has_world && journal != NULL);
    FRONTEND_CHECK(sim.world_seed == second_seed);
    FrontendOpen(&state, FRONTEND_PAUSED);
    FRONTEND_ACT(FRONTEND_ACTION_TITLE);
    FRONTEND_CHECK(state.screen == FRONTEND_TITLE);
    FRONTEND_ACT(FRONTEND_ACTION_QUIT);
    FRONTEND_CHECK(state.quit);
    FRONTEND_CHECK(DeleteClientWorld(&journal, &sim, save_path, session_path, error, sizeof(error)));
    (void)remove(preferences_path);
    LeaveOpenWorld(&local);
#undef FRONTEND_ACT
#undef FRONTEND_CHECK
    (void)puts("Title, pause, save, and delete-world regression passed");
    return 0;
}
#endif
