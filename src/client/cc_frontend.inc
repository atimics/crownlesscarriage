/* Title and pause screens share the same world and journal as gameplay. */
typedef enum FrontendScreen {
    FRONTEND_TITLE,
    FRONTEND_PLAYING,
    FRONTEND_PAUSED,
    FRONTEND_DELETE,
    FRONTEND_AVATAR,
    FRONTEND_SOUND,
    FRONTEND_WORLDS,
    FRONTEND_CREATE_WORLD,
    FRONTEND_JOIN_WORLD,
    FRONTEND_COMPANY,
    FRONTEND_REMOVE_MEMBER
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
    FRONTEND_ACTION_QUIT,
    FRONTEND_ACTION_BOOK,
    FRONTEND_ACTION_TEXT,
    FRONTEND_ACTION_HINTS,
    FRONTEND_ACTION_AVATAR,
    FRONTEND_ACTION_SKIN,
    FRONTEND_ACTION_HAIR,
    FRONTEND_ACTION_STYLE,
    FRONTEND_ACTION_FACE,
    FRONTEND_ACTION_COAT,
    FRONTEND_ACTION_SAVE_AVATAR,
    FRONTEND_ACTION_CANCEL_AVATAR,
    FRONTEND_ACTION_FULLSCREEN,
    FRONTEND_ACTION_COMPANY,
    FRONTEND_ACTION_HOST_PAUSE,
    FRONTEND_ACTION_CREDITS,
    FRONTEND_ACTION_TOUCH,
    FRONTEND_ACTION_SOUND_MENU,
    FRONTEND_ACTION_VOICE_VOLUME,
    FRONTEND_ACTION_PLAYER_VOICE,
    FRONTEND_ACTION_READ_ALOUD,
    FRONTEND_ACTION_AMBIENT,
    FRONTEND_ACTION_WORLD_LIST,
    FRONTEND_ACTION_CREATE_WORLD,
    FRONTEND_ACTION_JOIN_WORLD,
    FRONTEND_ACTION_CREATE_SUBMIT,
    FRONTEND_ACTION_JOIN_SUBMIT,
    FRONTEND_ACTION_WORLD_ENTER,
    FRONTEND_ACTION_INVITE_COPY,
    FRONTEND_ACTION_INVITE_ROTATE,
    FRONTEND_ACTION_WORLD_PREVIOUS,
    FRONTEND_ACTION_WORLD_NEXT,
    FRONTEND_ACTION_FIELD_NAME,
    FRONTEND_ACTION_FIELD_WORLD,
    FRONTEND_ACTION_FIELD_PASS,
    FRONTEND_ACTION_FIELD_INVITATION,
    FRONTEND_ACTION_CONFIRM_REMOVE,
    FRONTEND_ACTION_WORLD_FIRST = 100,
    FRONTEND_ACTION_MEMBER_FIRST = 200
} FrontendAction;

typedef struct FrontendState {
    FrontendScreen screen;
    bool online;
    bool has_world;
    bool needs_initial_save;
    bool quit;
    int focus;
    FrontendScreen avatar_return;
    uint32_t avatar;
    char feedback[256];
    CcCompany company;
    char player_name[32];
    char world_name[65];
    char world_pass[65];
    char invitation[256];
    int world_page;
    FrontendAction text_field;
    FrontendScreen company_return;
    bool deleting_company;
    char remove_member[33];
    char remove_name[65];
} FrontendState;

static CcLocalAgent frontend_avatar_preview;
static const int frontend_avatar_counts[] = {6, 6, 6, 4, 6};
static const char *frontend_avatar_labels[] = {"Skin", "Hair colour", "Hair style", "Face", "Coat"};
static const char *frontend_avatar_options[5][6] = {
    {"Warm", "Fair", "Sand", "Copper", "Umber", "Deep"},
    {"Ink", "Chestnut", "Auburn", "Gold", "Silver", "Chalk"},
    {"Cropped", "Swept", "Bob", "Crest", "Braided", "Long"},
    {"Square", "Long", "Broad", "Weathered", NULL, NULL},
    {"Plum", "Moss", "Rust", "Ocean", "Ochre", "Charcoal"}
};

static void FrontendChangeAvatar(FrontendState *state, int field, int direction)
{
    unsigned int shift = (unsigned int)field * 3U;
    int value = (int)((state->avatar >> shift) & 7U);
    value = (value + direction + frontend_avatar_counts[field]) % frontend_avatar_counts[field];
    state->avatar = (state->avatar & ~(7U << shift)) | ((uint32_t)value << shift);
    state->feedback[0] = '\0';
}

typedef struct FrontendItem {
    FrontendAction action;
    Rectangle bounds;
    const char *label;
    bool enabled;
} FrontendItem;

static Rectangle FrontendPanel(FrontendScreen screen)
{
    float width = screen == FRONTEND_PAUSED || screen == FRONTEND_AVATAR || screen == FRONTEND_COMPANY ||
        screen == FRONTEND_WORLDS || screen == FRONTEND_CREATE_WORLD || screen == FRONTEND_JOIN_WORLD ? 920.0f : 440.0f;
    return (Rectangle){((float)GetScreenWidth() - width) * 0.5f,
                       ((float)GetScreenHeight() - 556.0f) * 0.5f,
                       width, 556.0f};
}

static int FrontendItems(const FrontendState *state, FrontendItem *items)
{
    Rectangle panel = FrontendPanel(state->screen);
    float x = panel.x + 36.0f;
    float y = panel.y + 148.0f;
    if (state->screen == FRONTEND_REMOVE_MEMBER) {
        items[0] = (FrontendItem){FRONTEND_ACTION_CANCEL, {x, y + 120, 368, 44}, "Keep traveller", true};
        items[1] = (FrontendItem){FRONTEND_ACTION_CONFIRM_REMOVE, {x, y + 180, 368, 44}, "Remove from company", true};
        return 2;
    }
    if (state->screen == FRONTEND_WORLDS) {
        int count = 0;
        for (int i = state->world_page * 6; i < state->company.world_count && i < state->world_page * 6 + 6; ++i)
            items[count++] = (FrontendItem){(FrontendAction)(FRONTEND_ACTION_WORLD_FIRST + i),
                {x, y + (float)(i % 6) * 45, 488, 36}, state->company.worlds[i].name, true};
        items[count++] = (FrontendItem){FRONTEND_ACTION_CREATE_WORLD, {x + 512, y, 336, 40}, "Create world", true};
        items[count++] = (FrontendItem){FRONTEND_ACTION_JOIN_WORLD, {x + 512, y + 48, 336, 40}, "Join with invitation", true};
        items[count++] = (FrontendItem){FRONTEND_ACTION_WORLD_LIST, {x + 512, y + 96, 336, 40}, "Refresh worlds", true};
        items[count++] = (FrontendItem){FRONTEND_ACTION_WORLD_PREVIOUS, {x + 512, y + 144, 164, 40}, "Previous", state->world_page > 0};
        items[count++] = (FrontendItem){FRONTEND_ACTION_WORLD_NEXT, {x + 684, y + 144, 164, 40}, "Next", (state->world_page + 1) * 6 < state->company.world_count};
        items[count++] = (FrontendItem){FRONTEND_ACTION_CANCEL, {x + 512, y + 240, 336, 40}, "Back to title", true};
        return count;
    }
    if (state->screen == FRONTEND_CREATE_WORLD || state->screen == FRONTEND_JOIN_WORLD) {
        bool create = state->screen == FRONTEND_CREATE_WORLD;
        items[0] = (FrontendItem){FRONTEND_ACTION_FIELD_NAME, {x, y, 848, 46}, "Your name", true};
        items[1] = (FrontendItem){create ? FRONTEND_ACTION_FIELD_WORLD : FRONTEND_ACTION_FIELD_INVITATION,
            {x, y + 64, 848, 46}, create ? "World name" : "Invitation", true};
        int count = 2;
        if (create) items[count++] = (FrontendItem){FRONTEND_ACTION_FIELD_PASS, {x, y + 128, 848, 46}, "World pass", true};
        items[count++] = (FrontendItem){create ? FRONTEND_ACTION_CREATE_SUBMIT : FRONTEND_ACTION_JOIN_SUBMIT,
            {x, y + 240, 412, 46}, create ? "Create world" : "Join company", true};
        items[count++] = (FrontendItem){FRONTEND_ACTION_WORLD_LIST, {x + 436, y + 240, 412, 46}, "Back", true};
        return count;
    }
    if (state->screen == FRONTEND_COMPANY) {
        int count = 0;
        for (int i = 0; i < state->company.crew_count; ++i)
            items[count++] = (FrontendItem){(FrontendAction)(FRONTEND_ACTION_MEMBER_FIRST + i),
                {x, y + (float)i * 34, 440, 29}, state->company.crew[i].name,
                state->company.owner && !state->company.crew[i].self};
        items[count++] = (FrontendItem){FRONTEND_ACTION_WORLD_ENTER, {x + 464, y, 384, 44},
            CcCoopClientActive() ? "Return to the road" : "Enter world", true};
        items[count++] = (FrontendItem){FRONTEND_ACTION_INVITE_COPY, {x + 464, y + 54, 384, 40}, "Copy invitation", state->company.owner};
        items[count++] = (FrontendItem){FRONTEND_ACTION_INVITE_ROTATE, {x + 464, y + 102, 384, 40}, "New invitation", state->company.owner};
        items[count++] = (FrontendItem){FRONTEND_ACTION_HOST_PAUSE, {x + 464, y + 150, 384, 40},
            state->company.paused ? "Resume shared world" : "Pause shared world", state->company.owner};
        items[count++] = (FrontendItem){FRONTEND_ACTION_DELETE, {x + 464, y + 198, 384, 40}, "Delete world", state->company.owner};
        items[count++] = (FrontendItem){FRONTEND_ACTION_CANCEL, {x + 464, y + 246, 384, 40}, "Back", true};
        return count;
    }
    if (state->screen == FRONTEND_SOUND) {
        const FrontendAction actions[] = {FRONTEND_ACTION_SOUND, FRONTEND_ACTION_VOICE_VOLUME,
            FRONTEND_ACTION_PLAYER_VOICE, FRONTEND_ACTION_READ_ALOUD, FRONTEND_ACTION_AMBIENT,
            FRONTEND_ACTION_CANCEL};
        const char *labels[] = {"Sound", "Voice volume", "Your voice", "Read pages aloud", "Nearby voices", "Back"};
        for (int i = 0; i < 6; ++i)
            items[i] = (FrontendItem){actions[i], {x, y + (float)i * 48, 368, 40}, labels[i], true};
        return 6;
    }
    if (state->screen == FRONTEND_TITLE) {
        items[0] = (FrontendItem){FRONTEND_ACTION_OFFLINE,
            {x, y + 20.0f, 179.0f, 44.0f}, "Offline", true};
        items[1] = (FrontendItem){FRONTEND_ACTION_ONLINE,
            {x + 189.0f, y + 20.0f, 179.0f, 44.0f}, "Online", true};
        items[2] = (FrontendItem){FRONTEND_ACTION_PLAY,
            {x, y + 138.0f, 368.0f, 52.0f}, "Play", true};
        items[3] = (FrontendItem){FRONTEND_ACTION_AVATAR,
            {x, y + 234.0f, 368.0f, 40.0f}, "Your traveller", true};
        items[4] = (FrontendItem){FRONTEND_ACTION_QUIT,
            {x, y + 282.0f, 368.0f, 36.0f}, "Quit game", true};
#if defined(PLATFORM_WEB)
        return 4;
#else
        return 5;
#endif
    }
    if (state->screen == FRONTEND_AVATAR) {
        for (int i = 0; i < 5; ++i) {
            items[i] = (FrontendItem){(FrontendAction)(FRONTEND_ACTION_SKIN + i),
                {panel.x + 496.0f, y + (float)i * 52.0f, 388, 42}, frontend_avatar_labels[i], true};
        }
        items[5] = (FrontendItem){FRONTEND_ACTION_SAVE_AVATAR,
            {panel.x + 496.0f, y + 274, 236, 44}, "Save appearance", true};
        items[6] = (FrontendItem){FRONTEND_ACTION_CANCEL_AVATAR,
            {panel.x + 744.0f, y + 274, 140, 44}, "Cancel", true};
        return 7;
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
        FRONTEND_ACTION_SOUND_MENU, FRONTEND_ACTION_MOTION,
        FRONTEND_ACTION_TITLE, FRONTEND_ACTION_DELETE, FRONTEND_ACTION_QUIT
    };
    const char *labels[] = {
        "Resume", "Save world", "Sound & voices", "Reduced motion",
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
    float right = panel.x + 496.0f;
    items[count++] = (FrontendItem){FRONTEND_ACTION_BOOK, {right, y, 388, 44}, "Company Book  B", true};
    items[count++] = (FrontendItem){FRONTEND_ACTION_TEXT, {right, y + 48, 388, 40}, "Text size  T", true};
    items[count++] = (FrontendItem){FRONTEND_ACTION_HINTS, {right, y + 96, 388, 40}, "Target hints  H", true};
    items[count++] = (FrontendItem){FRONTEND_ACTION_AVATAR, {right, y + 144, 388, 40}, "Your traveller", true};
    items[count++] = (FrontendItem){FRONTEND_ACTION_FULLSCREEN, {right, y + 192, 388, 40}, "Full screen", true};
    float credits_y = y + 240;
    if (CcCoopClientActive()) {
        items[count++] = (FrontendItem){FRONTEND_ACTION_COMPANY, {right, y + 240, 388, 40}, "Company & invitations", true};
        if (CcCoopClientOwner()) {
            items[count++] = (FrontendItem){FRONTEND_ACTION_HOST_PAUSE, {right, y + 288, 388, 40},
                CcCoopClientPaused() ? "Resume shared world" : "Pause shared world", true};
        }
        credits_y = y + 336;
    }
    items[count++] = (FrontendItem){FRONTEND_ACTION_CREDITS, {right, credits_y, 388, 36}, "Licenses & credits", true};
    return count;
}

static void FrontendOpen(FrontendState *state, FrontendScreen screen)
{
    state->screen = screen;
    state->focus = screen == FRONTEND_TITLE ? 2 : 0;
    state->feedback[0] = '\0';
    state->text_field = FRONTEND_ACTION_NONE;
}

#include "cc_company_ui.inc"

/* A book opened from the menu returns to that menu and keeps its scene. */
static void FrontendReturnFromBook(FrontendState *state, LocalState *local, ClientView *view)
{
    if (*view == VIEW_PAUSE && state->screen == FRONTEND_PLAYING) {
        *view = local->pause_return_view;
        FrontendOpen(state, FRONTEND_PAUSED);
    }
}

static FrontendAction FrontendInput(FrontendState *state)
{
    if (FrontendCompanyTextInput(state)) return FRONTEND_ACTION_NONE;
    if (ClientKeyPressed(KEY_ESCAPE)) {
        if (state->screen == FRONTEND_PLAYING) {
            FrontendOpen(state, FRONTEND_PAUSED);
        } else if (state->screen == FRONTEND_PAUSED) {
            return FRONTEND_ACTION_RESUME;
        } else if (state->screen == FRONTEND_DELETE || state->screen == FRONTEND_SOUND ||
            state->screen >= FRONTEND_WORLDS) {
            return FRONTEND_ACTION_CANCEL;
        } else if (state->screen == FRONTEND_AVATAR) {
            return FRONTEND_ACTION_CANCEL_AVATAR;
        }
        return FRONTEND_ACTION_NONE;
    }
    if (state->screen == FRONTEND_PLAYING) return FRONTEND_ACTION_NONE;
    if (state->screen == FRONTEND_PAUSED) {
        if (ClientKeyPressed(KEY_B)) return FRONTEND_ACTION_BOOK;
        if (ClientKeyPressed(KEY_T)) return FRONTEND_ACTION_TEXT;
        if (ClientKeyPressed(KEY_H)) return FRONTEND_ACTION_HINTS;
        if (ClientKeyPressed(KEY_F5)) return FRONTEND_ACTION_SAVE;
    }
    FrontendItem items[16];
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
    if (state->screen == FRONTEND_AVATAR && state->focus < 5 &&
        (ClientKeyPressed(KEY_LEFT) || ClientKeyPressed(KEY_RIGHT))) {
        FrontendChangeAvatar(state, state->focus, ClientKeyPressed(KEY_LEFT) ? -1 : 1);
        return FRONTEND_ACTION_NONE;
    }
    if (ClientMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        for (int i = 0; i < count; ++i) {
            if (CheckCollisionPointRec(ClientPointerPosition(), items[i].bounds)) {
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
    int32_t *selected_situation, const char *save_path, const char *session_path,
    const char *preferences_path)
{
    if (FrontendCompanyAction(state, action)) return;
    if (action == FRONTEND_ACTION_AVATAR) {
        state->avatar_return = state->screen;
        state->avatar = CcCoopClientActive() ? CcCoopClientAppearance() :
            adventure_preferences != NULL ? adventure_preferences->avatar : 0U;
        CcLocalAgentInit(&frontend_avatar_preview, (Vector2){0, 0}, false);
        FrontendOpen(state, FRONTEND_AVATAR);
    } else if (action >= FRONTEND_ACTION_SKIN && action <= FRONTEND_ACTION_COAT) {
        FrontendChangeAvatar(state, (int)(action - FRONTEND_ACTION_SKIN), 1);
    } else if (action == FRONTEND_ACTION_CANCEL_AVATAR) {
        FrontendOpen(state, state->avatar_return);
    } else if (action == FRONTEND_ACTION_SAVE_AVATAR) {
        bool saved = false;
        if (CcCoopClientActive()) {
            saved = CcCoopClientSetAppearance(state->avatar, state->feedback, sizeof(state->feedback));
        } else if (adventure_preferences != NULL) {
            CcClientPreferences next = *adventure_preferences;
            next.avatar = state->avatar;
            saved = CcClientPreferencesSave(preferences_path, &next, state->feedback, sizeof(state->feedback));
#if defined(PLATFORM_WEB)
            if (saved && ClientFlushBrowserPreferences(preferences_path) == 0) {
                saved = false;
                (void)snprintf(state->feedback, sizeof(state->feedback), "Check browser storage, then save your appearance again.");
                (void)CcClientPreferencesSave(preferences_path, adventure_preferences, NULL, 0U);
            }
#endif
            if (saved) *adventure_preferences = next;
        }
        if (saved) {
            local->agent.appearance = CcNpcPlayerAppearance(state->avatar);
            FrontendOpen(state, state->avatar_return);
            (void)snprintf(state->feedback, sizeof(state->feedback), "Appearance saved.");
        }
    } else if (action == FRONTEND_ACTION_FULLSCREEN) {
#if defined(PLATFORM_WEB)
        ClientBrowserFullscreen();
#else
        ToggleFullscreen();
#endif
    } else if (action == FRONTEND_ACTION_SOUND_MENU) {
        FrontendOpen(state, FRONTEND_SOUND);
    } else if (adventure_preferences != NULL && (action == FRONTEND_ACTION_VOICE_VOLUME ||
        action == FRONTEND_ACTION_PLAYER_VOICE || action == FRONTEND_ACTION_READ_ALOUD ||
        action == FRONTEND_ACTION_AMBIENT)) {
        if (action == FRONTEND_ACTION_VOICE_VOLUME)
            adventure_preferences->voice_volume = (adventure_preferences->voice_volume + 25) % 125;
        else if (action == FRONTEND_ACTION_PLAYER_VOICE)
            adventure_preferences->player_voice = adventure_preferences->player_voice == 12 ? -1 :
                adventure_preferences->player_voice < 5 ? 5 : adventure_preferences->player_voice + 1;
        else if (action == FRONTEND_ACTION_READ_ALOUD)
            adventure_preferences->read_aloud = !adventure_preferences->read_aloud;
        else adventure_preferences->ambient_voices = !adventure_preferences->ambient_voices;
        adventure_preferences_dirty = true;
    } else if (action == FRONTEND_ACTION_TOUCH) {
#if defined(PLATFORM_WEB)
        ClientBrowserTouchControls();
#endif
    } else if (action == FRONTEND_ACTION_CREDITS) {
#if defined(PLATFORM_WEB)
        OpenURL("licenses.html");
#else
        OpenURL("https://atimics.github.io/crownlesscarriage/licenses.html");
#endif
    } else if (action == FRONTEND_ACTION_COMPANY) {
        CcCoopClientOpenCompany();
    } else if (action == FRONTEND_ACTION_HOST_PAUSE) {
        if (CcCoopClientTogglePause(state->feedback, sizeof(state->feedback)))
            (void)snprintf(state->feedback, sizeof(state->feedback), "%s",
                CcCoopClientPaused() ? "Shared world paused." : "Shared world resumed.");
    } else if (action == FRONTEND_ACTION_BOOK) {
        local->pause_return_view = *view;
        *return_view = VIEW_PAUSE;
        *view = VIEW_LEDGER;
        FrontendOpen(state, FRONTEND_PLAYING);
    } else if ((action == FRONTEND_ACTION_TEXT || action == FRONTEND_ACTION_HINTS) &&
               adventure_preferences != NULL) {
        if (action == FRONTEND_ACTION_TEXT)
            adventure_preferences->text_size = (adventure_preferences->text_size + 1) % 3;
        else adventure_preferences->focus_hints = !adventure_preferences->focus_hints;
        adventure_preferences_dirty = true;
    } else if (action == FRONTEND_ACTION_OFFLINE || action == FRONTEND_ACTION_ONLINE) {
        state->online = action == FRONTEND_ACTION_ONLINE;
        state->focus = state->online ? 1 : 2;
        state->feedback[0] = '\0';
    } else if (action == FRONTEND_ACTION_PLAY && state->online) {
        (void)FrontendCompanyAction(state, FRONTEND_ACTION_WORLD_LIST);
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
                         const CcClientPreferences *preferences, const CcSim *sim,
                         RenderTexture2D preview_target)
{
    ClientTouchBegin();
    Rectangle panel = FrontendPanel(state->screen);
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BACKGROUND, 0.86f));
    DrawRectangleRounded(panel, 0.025f, 6, PANEL_DEEP);
    DrawRectangleRoundedLinesEx(panel, 0.025f, 6, 1.0f, Fade(CC_GOLD, 0.46f));
    int top = (int)panel.y;
    FrontendCenteredText("CROWNLESS CARRIAGE", top + 38, 14, CC_GOLD);
    const char *title = state->screen == FRONTEND_TITLE ? "The road awaits" :
        state->screen == FRONTEND_AVATAR ? "Your traveller" :
        state->screen == FRONTEND_SOUND ? "Sound & voices" :
        state->screen == FRONTEND_WORLDS ? "Shared worlds" :
        state->screen == FRONTEND_CREATE_WORLD ? "Create a shared world" :
        state->screen == FRONTEND_JOIN_WORLD ? "Join a company" :
        state->screen == FRONTEND_COMPANY ? state->company.name :
        state->screen == FRONTEND_REMOVE_MEMBER ? "Remove this traveller?" :
        state->screen == FRONTEND_DELETE ? "Delete this world?" :
        CcCoopClientActive() ? "Company menu" : "Journey paused";
    ClientTouchHeading(title, state->feedback[0] != '\0' ? state->feedback :
        state->screen == FRONTEND_DELETE ? CcCoopClientActive() ?
            "This removes the shared world and progress for everyone in the company." :
            "This removes your world and its progress. Your next Play begins a new world." :
        state->screen == FRONTEND_TITLE ? "Choose Offline or Online, then Play. Your world is saved on this device." :
        "Resume your journey, save your world, or change your settings.");
    FrontendCenteredText(title, top + 76, 28, INK);
    if (state->screen == FRONTEND_TITLE) {
        FrontendCenteredText("Choose how to play", top + 126, 14, MUTED);
        FrontendCenteredText(state->online ? "Share a carriage with your company." :
            "Your world is saved on this device.", top + 232, 14, MUTED);
        FrontendCenteredText(state->online ? "Create a world or join your company." :
            state->has_world ? "Continue your saved journey." : "Begin a new world.",
            top + 354, 14, MUTED);
    } else if (state->screen == FRONTEND_DELETE) {
        FrontendCenteredText(CcCoopClientActive() || state->deleting_company ? "This removes the shared world and progress" :
            "This removes your world and its progress", top + 165, 16, INK);
        FrontendCenteredText(CcCoopClientActive() || state->deleting_company ? "for everyone in the company." :
            "from this device.", top + 189, 16, INK);
        FrontendCenteredText("Your next Play begins a new world.", top + 224, 14, MUTED);
    } else if (state->screen == FRONTEND_AVATAR) {
        FrontendCenteredText("Choose a look for the road", top + 120, 14, MUTED);
        frontend_avatar_preview.appearance = CcNpcPlayerAppearance(state->avatar);
        CcLocalDrawAvatarPreview3D(&frontend_avatar_preview, preview_target,
            (Rectangle){panel.x + 36, panel.y + 148, 424, 318});
    } else if (state->screen == FRONTEND_REMOVE_MEMBER) {
        FrontendCenteredText(state->remove_name, top + 165, 22, INK);
        FrontendCenteredText("This traveller will leave the company.", top + 209, 14, MUTED);
    } else if (state->screen >= FRONTEND_WORLDS) {
        FrontendCenteredText(state->screen == FRONTEND_CREATE_WORLD ? "Use the world pass from this host." :
            state->screen == FRONTEND_JOIN_WORLD ? "Paste an invitation and choose your traveller's name." :
            state->screen == FRONTEND_COMPANY ? "Your company and its shared road." :
            state->company.world_count == 0 ? "Create a world or join with an invitation." : "Choose a saved journey.", top + 120, 14, MUTED);
    } else {
        FrontendCenteredText(CcCoopClientActive() ? "ONLINE  /  THE COMPANY KEEPS PLAYING" :
            TextFormat("OFFLINE  /  DAY %d", sim->current_day),
                             top + 120, 12, MUTED);
    }
    FrontendItem items[16];
    int count = FrontendItems(state, items);
    for (int i = 0; i < count; ++i) {
        FrontendItem item = items[i];
        bool selected = (item.action == FRONTEND_ACTION_ONLINE && state->online) ||
                        (item.action == FRONTEND_ACTION_OFFLINE && !state->online);
        bool danger = item.action == FRONTEND_ACTION_DELETE || item.action == FRONTEND_ACTION_CONFIRM_REMOVE ||
                      item.action == FRONTEND_ACTION_CONFIRM_DELETE;
        bool primary = item.action == FRONTEND_ACTION_PLAY ||
                       item.action == FRONTEND_ACTION_SAVE_AVATAR ||
                       item.action == FRONTEND_ACTION_RESUME;
        bool hover = CheckCollisionPointRec(ClientPointerPosition(), item.bounds);
        Color accent = danger ? DANGER : selected || primary ? TEAL : CC_GOLD;
        DrawRectangleRounded(item.bounds, 0.12f, 4,
            Fade(hover || selected || primary ? PANEL_HOVER : PANEL,
                 item.enabled ? 1.0f : 0.45f));
        DrawRectangleRoundedLinesEx(item.bounds, 0.12f, 4,
            i == state->focus ? 2.0f : 1.0f,
            Fade(accent, item.enabled ? (i == state->focus || selected ? 1.0f : 0.45f) : 0.18f));
        const char *label = item.label;
        char field_label[192];
        if (FrontendIsCompanyField(item.action)) {
            size_t field_capacity;
            char *value = FrontendCompanyField((FrontendState *)state, item.action, &field_capacity);
            (void)field_capacity;
            (void)snprintf(field_label, sizeof(field_label), "%s: %s%s", item.label,
                item.action == FRONTEND_ACTION_FIELD_PASS && value[0] != '\0' ? "••••••••" : value,
                state->text_field == item.action ? "_" : "");
            label = field_label;
        }
        if (item.action >= FRONTEND_ACTION_MEMBER_FIRST) {
            const CcCompanyEntry *member = &state->company.crew[item.action - FRONTEND_ACTION_MEMBER_FIRST];
            label = TextFormat("%s%s%s / %s", item.enabled ? "Remove " : "", member->name,
                member->self ? " (you)" : "", member->online ? "Aboard" : "Away");
        }
        if (item.action >= FRONTEND_ACTION_SKIN && item.action <= FRONTEND_ACTION_COAT) {
            int field = (int)(item.action - FRONTEND_ACTION_SKIN);
            unsigned int choice = (state->avatar >> ((unsigned int)field * 3U)) & 7U;
            label = TextFormat("%s: %s  >", frontend_avatar_labels[field],
                frontend_avatar_options[field][choice < (unsigned int)frontend_avatar_counts[field] ? choice : 0]);
        }
        if (item.action == FRONTEND_ACTION_SOUND) {
            label = preferences->audio_mode == 0 ? "Sound: full" :
                    preferences->audio_mode == 1 ? "Sound: effects" : "Sound: muted";
        } else if (item.action == FRONTEND_ACTION_MOTION) {
            label = preferences->reduced_motion ? "Reduced motion: on" : "Reduced motion: off";
        }
        if (item.action == FRONTEND_ACTION_VOICE_VOLUME) label = TextFormat("Voice volume: %d%%", preferences->voice_volume);
        if (item.action == FRONTEND_ACTION_PLAYER_VOICE) label = preferences->player_voice < 5 ? "Your replies: text" :
            TextFormat("Your voice: %s", CcSpeechVoiceAt((size_t)preferences->player_voice)->name);
        if (item.action == FRONTEND_ACTION_READ_ALOUD) label = preferences->read_aloud ? "Read pages aloud: on" : "Read pages aloud: off";
        if (item.action == FRONTEND_ACTION_AMBIENT) label = preferences->ambient_voices ? "Nearby voices: on" : "Nearby voices: off";
        if (item.action == FRONTEND_ACTION_TEXT) label = TextFormat("Text size: %s  T",
            preferences->text_size == 0 ? "standard" : preferences->text_size == 1 ? "large" : "largest");
        if (item.action == FRONTEND_ACTION_HINTS) label = preferences->focus_hints ? "Target hints: shown  H" : "Target hints: focus only  H";
        ClientTouchAdd(item.bounds, label, item.enabled, selected);
        int size = (primary ? 20 : 16) + preferences->text_size * 2;
        while (size > 14 && MeasureText(label, size) > (int)item.bounds.width - 24) --size;
        DrawText(label, (int)(item.bounds.x + (item.bounds.width -
            (float)MeasureText(label, size)) * 0.5f),
            (int)(item.bounds.y + (item.bounds.height - (float)size) * 0.5f),
            size, Fade(danger ? DANGER : INK, item.enabled ? 1.0f : 0.35f));
    }
    FrontendCompanyPublishFields((FrontendState *)state, items, count);
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
        state->screen == FRONTEND_AVATAR ? "UP / DOWN  Choose     LEFT / RIGHT  Change     ESC  Cancel" :
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
    &local, &view, &return_view, &selected, &selected_situation, save_path, session_path, preferences_path)
    FRONTEND_CHECK(!CampaignSaveExists(save_path));
    FRONTEND_ACT(FRONTEND_ACTION_ONLINE);
    FrontendItem items[16];
    (void)FrontendItems(&state, items);
    FRONTEND_CHECK(items[2].enabled && items[2].action == FRONTEND_ACTION_PLAY);
    FRONTEND_CHECK(state.screen == FRONTEND_TITLE && journal == NULL);
    FRONTEND_CHECK(!CampaignSaveExists(save_path));
    CcClientPreferences avatar_preferences;
    CcClientPreferencesDefault(&avatar_preferences);
    adventure_preferences = &avatar_preferences;
    FRONTEND_ACT(FRONTEND_ACTION_AVATAR);
    FRONTEND_CHECK(state.screen == FRONTEND_AVATAR && state.avatar_return == FRONTEND_TITLE);
    FRONTEND_ACT(FRONTEND_ACTION_COAT);
    FRONTEND_ACT(FRONTEND_ACTION_CANCEL_AVATAR);
    FRONTEND_CHECK(state.screen == FRONTEND_TITLE && avatar_preferences.avatar == 0U);
    FRONTEND_ACT(FRONTEND_ACTION_AVATAR);
    FrontendChangeAvatar(&state, 3, -1);
    FRONTEND_CHECK(state.avatar == (3U << 9U));
    FRONTEND_ACT(FRONTEND_ACTION_FACE);
    FRONTEND_CHECK(state.avatar == 0U);
    FRONTEND_ACT(FRONTEND_ACTION_COAT);
    FRONTEND_ACT(FRONTEND_ACTION_SAVE_AVATAR);
    FRONTEND_CHECK(state.screen == FRONTEND_TITLE && avatar_preferences.avatar == (1U << 12U));
    FRONTEND_CHECK(!CampaignSaveExists(save_path));
    CcClientPreferences restored_avatar;
    FRONTEND_CHECK(CcClientPreferencesLoad(preferences_path, &restored_avatar, state.feedback, sizeof(state.feedback)));
    FRONTEND_CHECK(restored_avatar.avatar == avatar_preferences.avatar);
    FRONTEND_ACT(FRONTEND_ACTION_AVATAR);
    FRONTEND_ACT(FRONTEND_ACTION_SKIN);
    HandleFrontendAction(&state, FRONTEND_ACTION_SAVE_AVATAR, &journal, &sim,
        &local, &view, &return_view, &selected, &selected_situation,
        save_path, session_path, "missing-avatar-directory/settings");
    FRONTEND_CHECK(state.screen == FRONTEND_AVATAR && state.feedback[0] != '\0');
    FRONTEND_CHECK(avatar_preferences.avatar == restored_avatar.avatar);
    FRONTEND_ACT(FRONTEND_ACTION_CANCEL_AVATAR);
    adventure_preferences = NULL;
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
    FRONTEND_ACT(FRONTEND_ACTION_AVATAR);
    FRONTEND_CHECK(state.avatar_return == FRONTEND_PAUSED);
    FRONTEND_ACT(FRONTEND_ACTION_CANCEL_AVATAR);
    FRONTEND_CHECK(state.screen == FRONTEND_PAUSED && view == VIEW_MAP && sim.clock.tick == first_tick);
    FRONTEND_ACT(FRONTEND_ACTION_DELETE);
    FRONTEND_CHECK(state.screen == FRONTEND_DELETE && state.focus == 0);
    FRONTEND_ACT(FRONTEND_ACTION_CANCEL);
    FRONTEND_CHECK(state.screen == FRONTEND_PAUSED && sim.world_seed == first_seed);
    FRONTEND_CHECK(CampaignSaveExists(save_path) && sim.clock.tick == first_tick);
    FRONTEND_ACT(FRONTEND_ACTION_RESUME);
    FRONTEND_CHECK(state.screen == FRONTEND_PLAYING && view == VIEW_MAP);
    FrontendOpen(&state, FRONTEND_PAUSED);
    FRONTEND_ACT(FRONTEND_ACTION_BOOK);
    FRONTEND_CHECK(state.screen == FRONTEND_PLAYING && view == VIEW_LEDGER && return_view == VIEW_PAUSE);
    queued_key_press[KEY_ESCAPE] = true;
    FRONTEND_CHECK(HandleAdventureBook(&sim, &local, &view, &return_view));
    ClientInputClearPressed();
    FrontendReturnFromBook(&state, &local, &view);
    FRONTEND_CHECK(state.screen == FRONTEND_PAUSED && view == VIEW_MAP);
    CcClientPreferences interface_options;
    CcClientPreferencesDefault(&interface_options);
    adventure_preferences = &interface_options;
    FRONTEND_ACT(FRONTEND_ACTION_TEXT);
    FRONTEND_ACT(FRONTEND_ACTION_HINTS);
    FRONTEND_CHECK(interface_options.text_size == 1 && !interface_options.focus_hints && adventure_preferences_dirty);
    FRONTEND_ACT(FRONTEND_ACTION_SOUND_MENU);
    FRONTEND_CHECK(state.screen == FRONTEND_SOUND && FrontendItems(&state, items) == 6);
    FRONTEND_ACT(FRONTEND_ACTION_VOICE_VOLUME);
    FRONTEND_ACT(FRONTEND_ACTION_PLAYER_VOICE);
    FRONTEND_ACT(FRONTEND_ACTION_READ_ALOUD);
    FRONTEND_ACT(FRONTEND_ACTION_AMBIENT);
    FRONTEND_CHECK(interface_options.voice_volume == 0 && interface_options.player_voice == 6 &&
        interface_options.read_aloud && !interface_options.ambient_voices);
    FRONTEND_ACT(FRONTEND_ACTION_CANCEL);
    FRONTEND_CHECK(state.screen == FRONTEND_PAUSED);
    adventure_preferences = NULL;
    adventure_preferences_dirty = false;
    local.road_choice_active = true;
    FRONTEND_ACT(FRONTEND_ACTION_TITLE);
    FRONTEND_CHECK(state.screen == FRONTEND_PAUSED && state.feedback[0] != '\0');
    local.road_choice_active = false;
    local.site_travel_active = true;
    local.site_travel_progress = 0.5f;
    FRONTEND_ACT(FRONTEND_ACTION_TITLE);
    FRONTEND_CHECK(state.screen == FRONTEND_TITLE && CampaignSaveExists(session_path));
    CcClientSession travel_session;
    FRONTEND_CHECK(CcClientSessionRead(session_path, &travel_session,
        state.feedback, sizeof(state.feedback)));
    FRONTEND_CHECK(travel_session.site_travel_active && travel_session.site_travel_progress == 0.5f);
    local.site_travel_active = false;
    FRONTEND_ACT(FRONTEND_ACTION_PLAY);
    FRONTEND_CHECK(state.screen == FRONTEND_PLAYING && sim.world_seed == first_seed);
    CcClientPreferences preferences;
    CcClientPreferencesDefault(&preferences);
    preferences.reduced_motion = true;
    preferences.audio_mode = 2;
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
