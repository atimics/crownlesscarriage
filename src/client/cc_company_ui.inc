static FrontendState *company_frontend;

static bool FrontendIsCompanyField(FrontendAction action)
{
    return action >= FRONTEND_ACTION_FIELD_NAME && action <= FRONTEND_ACTION_FIELD_INVITATION;
}

static char *FrontendCompanyField(FrontendState *state, FrontendAction action, size_t *capacity)
{
    if (action == FRONTEND_ACTION_FIELD_NAME) { *capacity = sizeof(state->player_name); return state->player_name; }
    if (action == FRONTEND_ACTION_FIELD_WORLD) { *capacity = sizeof(state->world_name); return state->world_name; }
    if (action == FRONTEND_ACTION_FIELD_PASS) { *capacity = sizeof(state->world_pass); return state->world_pass; }
    *capacity = sizeof(state->invitation); return state->invitation;
}

#if defined(PLATFORM_WEB)
EMSCRIPTEN_KEEPALIVE void CrownlessCompanyText(int action, const char *value)
{
    if (company_frontend != NULL && action == 0) { company_frontend->text_field = FRONTEND_ACTION_NONE; return; }
    if (company_frontend == NULL || !FrontendIsCompanyField((FrontendAction)action)) return;
    size_t capacity;
    char *field = FrontendCompanyField(company_frontend, (FrontendAction)action, &capacity);
    (void)snprintf(field, capacity, "%s", value);
    company_frontend->text_field = (FrontendAction)action;
}
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wstrict-prototypes"
#pragma clang diagnostic ignored "-Wextra-semi"
EM_JS(void, CompanyFieldsBegin, (), { Module.crownlessFields = []; });
EM_JS(void, CompanyField, (int action, const char *label, const char *value, int capacity, int secret, float x, float y, float width, float height), {
    Module.crownlessFields.push({action, label:UTF8ToString(label), value:UTF8ToString(value),
        capacity, secret:!!secret, x, y, width, height});
});
EM_JS(void, CompanyFieldsEnd, (), { Module.renderCrownlessFields?.(Module.crownlessFields); });
EM_JS(void, CompanyInvitation, (char *invitation, int capacity), {
    stringToUTF8(new URLSearchParams(location.hash.slice(1)).get('join') || "", invitation, capacity);
});
EM_JS(int, CompanySharedHost, (), {
    if (location.hostname.endsWith('.github.io')) {
        location.assign('https://crownless.ratimics.com/game/index.html?lobby=1');
        return 0;
    }
    return 1;
});
/* 2 shared through the browser, 1 copied, 3 share dismissed, 0 nothing worked. */
EM_ASYNC_JS(int, CompanyShareInvitation, (const char *invitation), {
    const url = UTF8ToString(invitation);
    if (navigator.share) {
        try { await navigator.share({title: 'Crownless Carriage', url}); return 2; }
        catch (error) { if (error && error.name === 'AbortError') return 3; }
    }
    try { await navigator.clipboard.writeText(url); return 1; }
    catch { return 0; }
});
EM_JS(void, CompanyInvitationLink, (const char *code, char *link, int capacity), {
    stringToUTF8(location.origin + '/#join=' + UTF8ToString(code), link, capacity);
});
#pragma clang diagnostic pop
#else
static void CompanyInvitation(char *invitation, int capacity) { (void)capacity; invitation[0] = '\0'; }
static int CompanyShareInvitation(const char *invitation)
{
    SetClipboardText(invitation);
    const char *copied = GetClipboardText();
    return copied != NULL && strcmp(copied, invitation) == 0 ? 1 : 0;
}
static void CompanyInvitationLink(const char *code, char *link, int capacity)
{
    const char *origin = getenv("CC_COOP_ORIGIN");
    (void)snprintf(link, (size_t)capacity, "%s/#join=%.97s",
        origin != NULL ? origin : "https://crownless.ratimics.com", code);
}
#endif

static void FrontendCompanyPublishFields(FrontendState *state, const FrontendItem *items, int count)
{
    company_frontend = state;
#if defined(PLATFORM_WEB)
    CompanyFieldsBegin();
    for (int i = 0; i < count; ++i) {
        if (!FrontendIsCompanyField(items[i].action)) continue;
        size_t capacity;
        char *value = FrontendCompanyField(state, items[i].action, &capacity);
        Rectangle bounds = items[i].bounds;
        CompanyField((int)items[i].action, items[i].label, value, (int)capacity - 1,
            items[i].action == FRONTEND_ACTION_FIELD_PASS, bounds.x, bounds.y, bounds.width, bounds.height);
    }
    CompanyFieldsEnd();
#else
    (void)items; (void)count;
#endif
}

static bool FrontendCompanyTextInput(FrontendState *state)
{
    if (!FrontendIsCompanyField(state->text_field)) return false;
    if (ClientKeyPressed(KEY_ESCAPE) || ClientKeyPressed(KEY_ENTER) || ClientKeyPressed(KEY_TAB)) {
        state->text_field = FRONTEND_ACTION_NONE;
        if (ClientKeyPressed(KEY_TAB)) state->focus += 1;
        return true;
    }
    if (ClientMouseButtonPressed(MOUSE_BUTTON_LEFT)) { state->text_field = FRONTEND_ACTION_NONE; return false; }
#if !defined(PLATFORM_WEB)
    size_t capacity;
    char *value = FrontendCompanyField(state, state->text_field, &capacity);
    size_t length = strlen(value);
    bool paste = (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_LEFT_SUPER) ||
        IsKeyDown(KEY_RIGHT_CONTROL) || IsKeyDown(KEY_RIGHT_SUPER)) && ClientKeyPressed(KEY_V);
    if (paste) {
        const char *text = GetClipboardText();
        if (text != NULL) (void)snprintf(value, capacity, "%s", text);
    } else {
        if (ClientKeyPressed(KEY_BACKSPACE) && length > 0) value[--length] = '\0';
        int character;
        while ((character = GetCharPressed()) != 0) {
            if (character >= 32 && character <= 126 && length + 1 < capacity) {
                value[length++] = (char)character;
                value[length] = '\0';
            }
        }
    }
#endif
    return true;
}

/* Hand the invitation link to the traveller's share sheet or clipboard. When neither
   is available the link is shown in full so it can be selected by hand. */
static void FrontendCompanyShare(FrontendState *state)
{
    int result = CompanyShareInvitation(state->invitation);
    if (result == 0 || result == 3) FrontendOpen(state, FRONTEND_INVITATION);
    (void)snprintf(state->feedback, sizeof(state->feedback), "%s",
        result == 2 ? "Invitation shared." :
        result == 1 ? "Invitation link copied." :
        result == 3 ? "Sharing was dismissed. The link is below." :
        "Select the invitation link below and copy it.");
}

static bool FrontendCompanyAction(FrontendState *state, FrontendAction action)
{
    if (state->screen == FRONTEND_INVITATION) {
        if (action == FRONTEND_ACTION_INVITE_COPY) {
            FrontendCompanyShare(state);
            return true;
        }
        if (action == FRONTEND_ACTION_CANCEL) { FrontendOpen(state, FRONTEND_COMPANY); return true; }
    }
    if (state->screen == FRONTEND_DELETE && state->deleting_company) {
        if (action == FRONTEND_ACTION_CONFIRM_DELETE) {
            if (CcCompanyHost(&state->company, "delete", NULL, state->feedback, sizeof(state->feedback))) {
                state->deleting_company = false;
                if (CcCoopClientActive()) CcCoopClientReturnToTitle();
                else return FrontendCompanyAction(state, FRONTEND_ACTION_WORLD_LIST);
            }
            return true;
        }
        if (action == FRONTEND_ACTION_CANCEL) {
            state->deleting_company = false;
            FrontendOpen(state, FRONTEND_COMPANY);
            return true;
        }
    }
    if (state->screen == FRONTEND_REMOVE_MEMBER) {
        if (action == FRONTEND_ACTION_CONFIRM_REMOVE) {
            if (CcCompanyHost(&state->company, "remove", state->remove_member, state->feedback, sizeof(state->feedback)))
                FrontendOpen(state, FRONTEND_COMPANY);
            return true;
        }
        if (action == FRONTEND_ACTION_CANCEL) { FrontendOpen(state, FRONTEND_COMPANY); return true; }
    }
    if (FrontendIsCompanyField(action)) { state->text_field = action; return true; }
    if (action == FRONTEND_ACTION_WORLD_LIST) {
#if defined(PLATFORM_WEB)
        if (!CompanySharedHost()) return true;
#endif
        FrontendOpen(state, FRONTEND_WORLDS);
        state->world_page = 0;
        (void)CcCompanyList(&state->company, state->feedback, sizeof(state->feedback));
        return true;
    }
    if (action == FRONTEND_ACTION_CREATE_WORLD || action == FRONTEND_ACTION_JOIN_WORLD) {
        FrontendOpen(state, action == FRONTEND_ACTION_CREATE_WORLD ? FRONTEND_CREATE_WORLD : FRONTEND_JOIN_WORLD);
        if (action == FRONTEND_ACTION_JOIN_WORLD && state->invitation[0] == '\0')
            CompanyInvitation(state->invitation, sizeof(state->invitation));
        return true;
    }
    if (action == FRONTEND_ACTION_WORLD_PREVIOUS || action == FRONTEND_ACTION_WORLD_NEXT) {
        state->world_page += action == FRONTEND_ACTION_WORLD_NEXT ? 1 : -1;
        if (state->world_page < 0) state->world_page = 0;
        state->focus = 0;
        return true;
    }
    if (action == FRONTEND_ACTION_CREATE_SUBMIT || action == FRONTEND_ACTION_JOIN_SUBMIT) {
        bool ok = action == FRONTEND_ACTION_CREATE_SUBMIT ?
            CcCompanyCreate(&state->company, state->player_name, state->world_name, state->world_pass,
                state->feedback, sizeof(state->feedback)) :
            CcCompanyJoin(&state->company, state->player_name, state->invitation, state->feedback, sizeof(state->feedback));
        if (ok) {
            state->world_pass[0] = '\0';
            state->company_return = FRONTEND_WORLDS;
            FrontendOpen(state, FRONTEND_COMPANY);
        }
        return true;
    }
    if (action >= FRONTEND_ACTION_WORLD_FIRST && action < FRONTEND_ACTION_WORLD_FIRST + 16) {
        int index = (int)action - FRONTEND_ACTION_WORLD_FIRST;
        if (index >= state->company.world_count) return true;
        char id[33];
        (void)snprintf(id, sizeof(id), "%s", state->company.worlds[index].id);
        if (CcCompanyLoad(&state->company, id, state->feedback, sizeof(state->feedback))) {
            state->company_return = FRONTEND_WORLDS;
            FrontendOpen(state, FRONTEND_COMPANY);
        }
        return true;
    }
    if (action == FRONTEND_ACTION_COMPANY) {
        char id[33];
        bool refreshing = state->screen == FRONTEND_COMPANY;
        if (refreshing) (void)snprintf(id, sizeof(id), "%s", state->company.id);
        else CcCoopClientWorldId(id, sizeof(id));
        if (CcCompanyLoad(&state->company, id, state->feedback, sizeof(state->feedback))) {
            if (!refreshing) state->company_return = FRONTEND_PAUSED;
            FrontendOpen(state, FRONTEND_COMPANY);
        }
        return true;
    }
    if (state->screen == FRONTEND_COMPANY) {
        if (action == FRONTEND_ACTION_DELETE) {
            state->deleting_company = true;
            FrontendOpen(state, FRONTEND_DELETE);
            return true;
        }
        if (action == FRONTEND_ACTION_WORLD_ENTER) {
            if (CcCoopClientActive() && state->company_return == FRONTEND_PAUSED) FrontendOpen(state, FRONTEND_PLAYING);
            else CcCoopClientEnterWorld(state->company.id);
            return true;
        }
        if (action == FRONTEND_ACTION_INVITE_COPY || action == FRONTEND_ACTION_INVITE_ROTATE) {
            if (CcCompanyInvite(&state->company, action == FRONTEND_ACTION_INVITE_ROTATE,
                state->feedback, sizeof(state->feedback))) {
                CompanyInvitationLink(state->company.invitation, state->invitation, sizeof(state->invitation));
                FrontendCompanyShare(state);
            }
            return true;
        }
        if (action == FRONTEND_ACTION_HOST_PAUSE) {
            (void)CcCompanyHost(&state->company, state->company.paused ? "resume" : "pause", NULL,
                state->feedback, sizeof(state->feedback));
            return true;
        }
        if (action >= FRONTEND_ACTION_MEMBER_FIRST && action < FRONTEND_ACTION_MEMBER_FIRST + 8) {
            int index = (int)action - FRONTEND_ACTION_MEMBER_FIRST;
            if (index >= state->company.crew_count || state->company.crew[index].self) return true;
            (void)snprintf(state->remove_member, sizeof(state->remove_member), "%s", state->company.crew[index].id);
            (void)snprintf(state->remove_name, sizeof(state->remove_name), "%s", state->company.crew[index].name);
            FrontendOpen(state, FRONTEND_REMOVE_MEMBER);
            return true;
        }
    }
    if (action == FRONTEND_ACTION_CANCEL && state->screen >= FRONTEND_WORLDS) {
        FrontendScreen back = state->screen == FRONTEND_COMPANY ? state->company_return : FRONTEND_TITLE;
        if (back == FRONTEND_WORLDS) return FrontendCompanyAction(state, FRONTEND_ACTION_WORLD_LIST);
        FrontendOpen(state, back);
        return true;
    }
    return false;
}
