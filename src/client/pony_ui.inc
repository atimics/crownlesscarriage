static Rectangle PonyBookButton(void)
{
    return (Rectangle){12, (float)GetScreenHeight() - 42, 142, 28};
}

static Rectangle PonyActionButton(int32_t action)
{
    return (Rectangle){(float)GetScreenWidth() * 0.5f - 318 + (float)action * 212,
                       (float)GetScreenHeight() - 98, 202, 30};
}

static bool PonyClicked(Rectangle bounds)
{
    return ClientMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
           CheckCollisionPointRec(GetMousePosition(), bounds);
}

static void DrawPonyButton(Rectangle bounds, const char *label)
{
    DrawRectangleRec(bounds, (Color){32, 37, 50, 255});
    DrawRectangleLinesEx(bounds, 1, TEAL);
    CcOverlayDrawText(label, (int)bounds.x + 10, (int)bounds.y + 10, 9, INK);
}

static bool HandlePonyInput(CcJournal *journal, CcSim *sim, LocalState *local,
                            ClientView view, char *message, size_t capacity)
{
    if (view != VIEW_LOCAL && !local->pony_book_open) return false;
    if (ClientKeyPressed(KEY_F7) || PonyClicked(PonyBookButton())) {
        local->pony_book_open = !local->pony_book_open;
        return true;
    }
    if (local->pony_book_open) {
        if (ClientKeyPressed(KEY_BACKSPACE) || ClientKeyPressed(KEY_ESCAPE)) local->pony_book_open = false;
        return true;
    }
    int32_t pony = CcPonyOnRoad(sim);
    if (pony < 0 || LocalCombatActive(local)) return false;
    if (sim->pony_company.encounter < 0) {
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
    if (ClientKeyPressed(KEY_BACKSPACE) || PonyClicked(PonyActionButton(2))) {
        command.kind = CC_COMMAND_LEAVE_PONY;
    } else if (state->ready) {
        if (ClientKeyPressed(KEY_ONE) || PonyClicked(PonyActionButton(0))) {
            command.kind = CC_COMMAND_SWAP_PONY;
            command.amount = 0;
        } else if (ClientKeyPressed(KEY_TWO) || PonyClicked(PonyActionButton(1))) {
            command.kind = CC_COMMAND_SWAP_PONY;
            command.amount = 1;
        }
    } else if (ClientKeyPressed(KEY_ENTER) || PonyClicked(PonyActionButton(0))) {
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

static void DrawPonyInterface(const CcSim *sim, const LocalState *local)
{
    static const Color colors[CC_PONY_COUNT] = {
        {244,104,115,255}, {255,169,84,255}, {255,229,110,255},
        {124,224,137,255}, {114,195,246,255}, {135,139,224,255}, {208,153,245,255}
    };
    DrawPonyButton(PonyBookButton(), "Pony book [F7]");
    int32_t pony = sim->pony_company.encounter;
    if (pony >= 0 && !local->pony_book_open) {
        float x = (float)GetScreenWidth() * 0.5f - 330;
        float y = (float)GetScreenHeight() - 258;
        DrawPanel((Rectangle){x, y, 660, 206}, (Color){13, 18, 28, 250});
        const CcPony *p = &sim->pony_company.ponies[pony];
        CcOverlayDrawText(TextFormat("%s / %s", CcPonyName(pony),
            p->releases > 0 ? "A familiar face" : "A rainbow traveller"),
            (int)x + 16, (int)y + 16, 15, colors[pony]);
        CcOverlayDrawText(CcPonyPersonality(pony), (int)x + 16, (int)y + 40, 10, MUTED);
        char quest[256] = {0};
        CcPonyQuestText(sim, pony, quest, sizeof(quest));
        DrawTwoLineText(quest, (int)x + 16, (int)y + 63, 76U, 10, INK);
        if (p->ready) {
            CcOverlayDrawText("Quest complete. Invite this pony and let one companion roam free.",
                (int)x + 16, (int)y + 106, 10, TEAL);
            for (int32_t slot = 0; slot < 2; ++slot) {
                DrawPonyButton(PonyActionButton(slot), TextFormat("[%d] Release %s", slot + 1,
                    CcPonyName(sim->pony_company.team[slot])));
            }
        } else {
            CcGood good = CcPonyQuestGood(p);
            CcOverlayDrawText(TextFormat("You carry %d / %d %s. Bond %d.",
                sim->player.cargo[good], p->quest_amount, CcGoodDefinitionFor(good)->name, p->bond),
                (int)x + 16, (int)y + 106, 10, TEAL);
            DrawPonyButton(PonyActionButton(0), "Help pony [Enter]");
        }
        DrawPonyButton(PonyActionButton(2), "Keep travelling [Backspace]");
    }
    if (!local->pony_book_open) return;
    int x = GetScreenWidth() / 2 - 330;
    int y = 106;
    DrawPanel((Rectangle){(float)x, (float)y, 660, 466}, (Color){13,18,28,252});
    CcOverlayDrawText("PONIES", x + 20, y + 18, 17, INK);
    CcOverlayDrawText("Meet them on the roads. Help them. Travel together. [F7 / Esc to close]",
                      x + 20, y + 47, 9, MUTED);
    for (int32_t i = 0; i < CC_PONY_COUNT; ++i) {
        const CcPony *p = &sim->pony_company.ponies[i];
        bool team = i == sim->pony_company.team[0] || i == sim->pony_company.team[1];
        int row = y + 83 + i * 51;
        CcLocalDrawPonyPortrait(i, p->seen,
            (Rectangle){(float)x + 18, (float)row - 5, 42, 42});
        if (!p->seen) {
            CcOverlayDrawText("??", x + 73, row, 13, MUTED);
            CcOverlayDrawText("Yet to meet", x + 532, row, 9, MUTED);
            continue;
        }
        CcOverlayDrawText(CcPonyName(i), x + 73, row, 13, colors[i]);
        CcOverlayDrawText(CcPonyPersonality(i), x + 210, row, 10, INK);
        CcOverlayDrawText(team ? "With you" : p->seen ? "Roaming" : "Yet to meet", x + 532, row, 9, colors[i]);
        const CcRoute *road = CcSimRoute(sim, p->last_seen_route);
        const CcSettlement *from = road ? CcSimSettlement(sim, road->from_id) : NULL;
        const CcSettlement *to = road ? CcSimSettlement(sim, road->to_id) : NULL;
        CcOverlayDrawText(TextFormat("Bond %d / Quests %d / Released %d / %s",
            p->bond, p->quests_completed, p->releases,
            from && to ? TextFormat("Seen: %s - %s", from->name, to->name) : "New adventures await"),
            x + 73, row + 23, 8, MUTED);
    }
}

/* Review captures exercise the same keyboard actions as normal play. */
static bool CheckPonyInterface(CcSim *sim, LocalState *local, int32_t pony)
{
    char message[256] = {0};
    ClientInputClearPressed();
    queued_key_press[KEY_ENTER] = true;
    if (!HandlePonyInput(NULL, sim, local, VIEW_LOCAL, message, sizeof(message)) ||
        !sim->pony_company.ponies[pony].ready) return false;
    CcPonyCompany ready = sim->pony_company;
    CcHorse horses[2] = {sim->horse_team[0], sim->horse_team[1]};
    for (int32_t slot = 0; slot < 2; ++slot) {
        ClientInputClearPressed();
        queued_key_press[KEY_ONE + slot] = true;
        if (!HandlePonyInput(NULL, sim, local, VIEW_LOCAL, message, sizeof(message)) ||
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
    if (!HandlePonyInput(NULL, sim, local, VIEW_LOCAL, message, sizeof(message)) ||
        !local->pony_book_open) return false;
    ClientInputClearPressed();
    queued_key_press[KEY_BACKSPACE] = true;
    if (!HandlePonyInput(NULL, sim, local, VIEW_LOCAL, message, sizeof(message)) ||
        local->pony_book_open) return false;
    ClientInputClearPressed();
    (void)puts("PASS pony interface: quest, both swap slots, book open and close");
    return true;
}
