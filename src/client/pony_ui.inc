static bool HandleCarriageTabs(LocalState *local, ClientView view)
{
    if (view != VIEW_CARRIAGE) return false;
    for (int32_t tab = 0; tab < CARRIAGE_TAB_COUNT; ++tab) {
        if (AdventureHit(CarriageTabBounds(tab)) || ClientKeyPressed(KEY_ONE + tab) ||
            (tab == CARRIAGE_PONIES && ClientKeyPressed(KEY_F7))) {
            local->carriage_tab = (CarriageTab)tab;
            return true;
        }
    }
    return false;
}

static bool HandlePonyInput(CcJournal *journal, CcSim *sim, LocalState *local,
                            ClientView view, ContextAction action, char *message, size_t capacity)
{
    if (view != VIEW_LOCAL) return false;
    int32_t pony = CcPonyOnRoad(sim);
    if (pony < 0 || LocalCombatActive(local)) return false;
    if (sim->pony_company.encounter < 0) {
        if (action.kind != CONTEXT_ACTION_PONY_MEET) return false;
        CcCommand meet = {.kind = CC_COMMAND_MEET_PONY, .target_id = (CcId)pony + 1U};
        if (!ApplyCommand(journal, sim, meet, message, capacity)) return true;
        local->travel_fast_forward = false;
        local->convoy.runtime_tick_accumulator = 0.0f;
        local->world_carriage.pace = 0.0f;
        local->convoy.pace = 0.0f;
        return true;
    }
    CcCommand command = {.target_id = (CcId)pony + 1U};
    const CcPony *state = &sim->pony_company.ponies[pony];
    if (ClientKeyPressed(KEY_BACKSPACE) || action.kind == CONTEXT_ACTION_PONY_LEAVE) {
        command.kind = CC_COMMAND_LEAVE_PONY;
    } else if (state->ready) {
        if (ClientKeyPressed(KEY_ONE) || (action.kind == CONTEXT_ACTION_PONY_SWAP && action.amount == 0)) {
            command.kind = CC_COMMAND_SWAP_PONY;
            command.amount = 0;
        } else if (ClientKeyPressed(KEY_TWO) || (action.kind == CONTEXT_ACTION_PONY_SWAP && action.amount == 1)) {
            command.kind = CC_COMMAND_SWAP_PONY;
            command.amount = 1;
        }
    } else if (ClientKeyPressed(KEY_ENTER) || action.kind == CONTEXT_ACTION_PONY_HELP) {
        command.kind = CC_COMMAND_HELP_PONY;
    }
    if (command.kind != CC_COMMAND_NONE) {
        int32_t released = sim->pony_company.team[command.amount];
        if (ApplyCommand(journal, sim, command, message, capacity)) {
            if (command.kind == CC_COMMAND_SWAP_PONY) {
                (void)snprintf(message, capacity, "%s joins you. %s heads out on a new adventure.",
                               CcPonyName(pony), CcPonyName(released));
            } else if (command.kind == CC_COMMAND_HELP_PONY) {
                (void)snprintf(message, capacity, "%s trusts you. Choose a companion to release.", CcPonyName(pony));
            }
            if (sim->pony_company.encounter < 0) local->convoy.pace = 0.72f;
        }
    }
    return true;
}

static const Color PONY_COLORS[CC_PONY_COUNT] = {
    {244,104,115,255}, {255,169,84,255}, {255,229,110,255},
    {124,224,137,255}, {114,195,246,255}, {135,139,224,255}, {208,153,245,255}
};

static void DrawPonyEncounter(const CcSim *sim)
{
    int32_t pony = sim->pony_company.encounter;
    if (pony < 0) return;
    const CcPony *p = &sim->pony_company.ponies[pony];
    char quest[256] = {0};
    CcPonyQuestText(sim, pony, quest, sizeof(quest));
    const char *line = p->ready ? "Thank you. I would love to travel with you. Who will head out on a new adventure?" : quest;
    ClientTouchHeading(CcPonyName(pony), line);
    int size = AdventureTextSize(17);
    float width = fminf(760.0f, (float)GetScreenWidth() - 80.0f);
    int line_height = AdventureText(line, 0, 0, (int)width - 44, size, INK, false);
    float height = 120.0f + (float)line_height;
    float x = ((float)GetScreenWidth() - width) * 0.5f;
    float y = (float)GetScreenHeight() - 108.0f - height;
    DrawPanel((Rectangle){x, y, width, height}, Fade(PANEL_DEEP, 0.96f));
    CcOverlayDrawText(CcPonyName(pony), (int)x + 22, (int)y + 16,
        AdventureTextSize(23), PONY_COLORS[pony]);
    CcOverlayDrawText(CcPonyPersonality(pony), (int)x + 22, (int)y + 49,
        AdventureTextSize(13), MUTED);
    (void)AdventureWrap(line, (int)x + 22, (int)y + 77, (int)width - 44, size, INK);
    CcGood good = CcPonyQuestGood(p);
    const char *detail = p->ready ? "Choose a companion to release, or say farewell." :
        TextFormat("You carry %d of %d %s. Bond %d.", sim->player.cargo[good],
            p->quest_amount, CcGoodDefinitionFor(good)->name, p->bond);
    CcOverlayDrawText(detail, (int)x + 22, (int)(y + height) - 29,
        AdventureTextSize(13), TEAL);
}

static void DrawCarriagePonies(const CcSim *sim)
{
    int x = 52;
    int y = 174;
    int width = GetScreenWidth() - 104;
    int height = (int)fminf(454.0f, (float)GetScreenHeight() - 284.0f);
    int row_height = (height - 72) / CC_PONY_COUNT;
    DrawPanel((Rectangle){(float)x, (float)y, (float)width, (float)height},
        Fade(BACKGROUND, 0.76f));
    CcOverlayDrawText("PONIES", x + 20, y + 18, 17, INK);
    CcOverlayDrawText("Meet them on the roads. Help them. Travel together.",
                      x + 20, y + 47, 9, MUTED);
    for (int32_t i = 0; i < CC_PONY_COUNT; ++i) {
        const CcPony *p = &sim->pony_company.ponies[i];
        bool team = i == sim->pony_company.team[0] || i == sim->pony_company.team[1];
        int row = y + 77 + i * row_height;
        float portrait_size = fminf(42.0f, (float)row_height - 4.0f);
        CcLocalDrawPonyPortrait(i, p->seen,
            (Rectangle){(float)x + 18, (float)row - 5, portrait_size, portrait_size});
        if (!p->seen) {
            CcOverlayDrawText("??", x + 73, row, 13, MUTED);
            CcOverlayDrawText("Yet to meet", x + width - 120, row, 9, MUTED);
            continue;
        }
        CcOverlayDrawText(CcPonyName(i), x + 73, row, 13, PONY_COLORS[i]);
        CcOverlayDrawText(CcPonyPersonality(i), x + 210, row, 10, INK);
        CcOverlayDrawText(team ? "With you" : "Roaming", x + width - 120, row, 9, PONY_COLORS[i]);
        const CcRoute *road = CcSimRoute(sim, p->last_seen_route);
        const CcSettlement *from = road ? CcSimSettlement(sim, road->from_id) : NULL;
        const CcSettlement *to = road ? CcSimSettlement(sim, road->to_id) : NULL;
        CcOverlayDrawText(TextFormat("Bond %d / Quests %d / Released %d / %s",
            p->bond, p->quests_completed, p->releases,
            from && to ? TextFormat("Seen: %s - %s", from->name, to->name) : "New adventures await"),
            x + 73, row + 20, 8, MUTED);
    }
}

/* Review captures exercise the same keyboard actions as normal play. */
static bool CheckPonyInterface(CcSim *sim, LocalState *local, int32_t pony)
{
    char message[256] = {0};
    ClientInputClearPressed();
    queued_key_press[KEY_ENTER] = true;
    if (!HandlePonyInput(NULL, sim, local, VIEW_LOCAL, (ContextAction){0}, message, sizeof(message)) ||
        !sim->pony_company.ponies[pony].ready) return false;
    CcPonyCompany ready = sim->pony_company;
    CcHorse horses[2] = {sim->horse_team[0], sim->horse_team[1]};
    for (int32_t slot = 0; slot < 2; ++slot) {
        ClientInputClearPressed();
        queued_key_press[KEY_ONE + slot] = true;
        if (!HandlePonyInput(NULL, sim, local, VIEW_LOCAL, (ContextAction){0}, message, sizeof(message)) ||
            sim->pony_company.team[slot] != pony ||
            sim->pony_company.team[1 - slot] != ready.team[1 - slot] ||
            sim->pony_company.ponies[ready.team[slot]].route_id == 0U ||
            sim->pony_company.ponies[ready.team[slot]].releases !=
                ready.ponies[ready.team[slot]].releases + 1) return false;
        sim->pony_company = ready;
        sim->horse_team[0] = horses[0];
        sim->horse_team[1] = horses[1];
    }
    ClientInputClearPressed();
    queued_key_press[KEY_F7] = true;
    if (!HandleCarriageTabs(local, VIEW_CARRIAGE) ||
        local->carriage_tab != CARRIAGE_PONIES) return false;
    ClientInputClearPressed();
    queued_key_press[KEY_ONE] = true;
    if (!HandleCarriageTabs(local, VIEW_CARRIAGE) ||
        local->carriage_tab != CARRIAGE_OVERVIEW) return false;
    ClientInputClearPressed();
    (void)puts("PASS pony interface: quest, both swap slots, carriage tabs");
    return true;
}
