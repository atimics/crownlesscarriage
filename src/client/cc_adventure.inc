/* Scene actions and modal UI share the same input boundary. */
static CcClientPreferences *adventure_preferences;
static bool adventure_preferences_dirty;

static bool AdventureScene(const LocalState *local)
{
    return local->adventure_ui && !local->open_world &&
        local->site_kind == CC_LOCAL_SITE_NONE && !local->site_travel_active &&
        !local->journey_travel_active && !local->road_choice_active &&
        !local->journey_combat_active && !local->journey_parley_active;
}

static CcInteractionTarget *AdventureTarget(LocalState *local, const CcSim *sim,
    CcInteractionKind kind, uint64_t object, const char *name, const char *verb,
    Vector3 position, float width, float height, float depth,
    Vector2 approach, float radius)
{
    if (local->interactions.count >= CC_INTERACTION_CAPACITY) return NULL;
    CcInteractionTarget *target = &local->interactions.targets[local->interactions.count++];
    *target = (CcInteractionTarget){
        .key = {sim->player.location_id, object, kind},
        .x = position.x, .y = position.y, .z = position.z,
        .width = width, .height = height, .depth = depth,
        .approach_x = approach.x, .approach_z = approach.y, .radius = radius,
        .available = !LocalCombatActive(local)};
    (void)snprintf(target->name, sizeof(target->name), "%s", name);
    (void)snprintf(target->verb, sizeof(target->verb), "%s", verb);
    if (!target->available) (void)snprintf(target->reason, sizeof(target->reason),
        "Find safety before you %s.", verb);
    return target;
}

static const char *AdventureRoleName(CcNpcRole role)
{
    switch (role) {
        case CC_NPC_ROLE_GUARD: return "Town guard";
        case CC_NPC_ROLE_MERCHANT: return "Market trader";
        case CC_NPC_ROLE_LABORER: return "Town worker";
        case CC_NPC_ROLE_TRAVELLER: return "Traveller";
        case CC_NPC_ROLE_REFUGEE: return "Wayfarer";
        case CC_NPC_ROLE_SCOUT: return "Scout";
        case CC_NPC_ROLE_HEALER: return "Healer";
        default: return "Neighbour";
    }
}

static void BuildAdventureTargets(const CcSim *sim, LocalState *local,
    RenderTexture2D texture, Rectangle destination)
{
    local->interactions.count = 0;
    if (!AdventureScene(local)) return;
    const CcLocalPlaceProfile *place = CcLocalPlaceProfileForSettlement(
        CcSimSettlement(sim, sim->player.location_id));
    if (local->market_interior) {
        (void)AdventureTarget(local, sim, CC_INTERACTION_COUNTER, 1U,
            place->keeper_name, "Trade", (Vector3){6.55f, 0, 1.9f}, 2.1f, 2.0f, 1.0f,
            (Vector2){6.55f, 3.15f}, 1.0f);
        const CcSettlement *warehouse = CcSimSettlement(sim, sim->player.location_id);
        for (int32_t good = 0; warehouse != NULL && good < CC_GOOD_COUNT; ++good) {
            if (warehouse->stock[good] <= 0 && sim->player.cargo[good] <= 0) continue;
            Vector3 bay = CcLocalWarehouseGoodPosition((CcGood)good);
            CcInteractionTarget *stock = AdventureTarget(local, sim, CC_INTERACTION_STOCK,
                (uint64_t)good + 1U,
                TextFormat("%s: %d in warehouse", CcGoodName((CcGood)good), warehouse->stock[good]),
                "Trade", bay, 0.52f, 0.70f, 0.64f, (Vector2){bay.x, 2.85f}, 0.80f);
            if (stock != NULL) stock->amount = good;
        }
        (void)AdventureTarget(local, sim, CC_INTERACTION_EXIT, 1U,
            "Street", "Step outside", (Vector3){1.55f, 0, 6.54f}, 1.3f, 2.5f, 0.5f,
            INTERIOR_EXIT, 0.85f);
    } else {
        const CcLocalPlaceBuilding *hall = &place->building[place->primary_building];
        float door_x = hall->x + hall->width * 0.5f;
        float door_z = hall->z + hall->depth + 0.15f;
        (void)AdventureTarget(local, sim, CC_INTERACTION_DOOR, 1U,
            place->primary_hall, "Enter", (Vector3){door_x, 0, door_z}, 1.8f, 2.8f, 0.5f,
            (Vector2){door_x, door_z + 1.0f}, 1.1f);
        (void)AdventureTarget(local, sim, CC_INTERACTION_BOARD, 1U,
            place->notice_board, "Read promises", (Vector3){LOCAL_NOTICE.x, 0, LOCAL_NOTICE.y},
            1.6f, 2.5f, 0.7f, LOCAL_NOTICE, 1.05f);
        if (local->opening_step == CC_LOCAL_OPENING_COMPLETE) (void)AdventureTarget(local, sim, CC_INTERACTION_CARRIAGE, 1U,
            "Crownless carriage", "Board", (Vector3){LOCAL_CARRIAGE.x, 0, LOCAL_CARRIAGE.y},
            3.2f, 3.2f, 5.4f, LOCAL_CARRIAGE_BAY, 1.5f);
        if (local->course.situation_witness_active) {
            const CcCharacter *person = CcSimCharacter(sim, local->course.situation_witness_character_id);
            Vector3 position = local->course.situation_witness.position;
            CcInteractionTarget *target = AdventureTarget(local, sim, CC_INTERACTION_PERSON,
                local->course.situation_witness_character_id,
                person != NULL ? person->name : "Neighbour", "Talk",
                (Vector3){position.x, 0, position.z}, 0.9f, 2.0f, 0.7f,
                (Vector2){position.x, position.z}, 1.75f);
            if (target != NULL) {
                target->character_id = local->course.situation_witness_character_id;
                target->situation_id = local->course.situation_witness_id;
            }
        }
        for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
            Vector3 position = local->course.runners[i].agent.position;
            (void)AdventureTarget(local, sim, CC_INTERACTION_PERSON, UINT64_C(0x200000000) + (uint64_t)i,
                "Town guard", "Talk", (Vector3){position.x, 0, position.z}, 0.9f, 2.0f, 0.7f,
                (Vector2){position.x, position.z}, 1.8f);
        }
        for (int32_t i = 0; i < CC_LOCAL_TRAVELLER_COUNT; ++i) {
            if (!local->course.travellers[i].active) continue;
            Vector3 position = local->course.travellers[i].agent.position;
            (void)AdventureTarget(local, sim, CC_INTERACTION_PERSON, UINT64_C(0x300000000) + (uint64_t)i,
                "Traveller", "Talk", (Vector3){position.x, 0, position.z}, 0.9f, 2.0f, 0.7f,
                (Vector2){position.x, position.z}, 1.8f);
        }
        if (DungeonAtSettlement(sim, sim->player.location_id) != NULL) {
            (void)AdventureTarget(local, sim, CC_INTERACTION_SITE, CC_LOCAL_SITE_DUNGEON,
                "Mine road", "Drive to the mine", (Vector3){LOCAL_DUNGEON.x, 0, LOCAL_DUNGEON.y},
                2.2f, 3.1f, 1.6f, LOCAL_DUNGEON, 1.3f);
        }
        CcLocalPresentedPerson people[64];
        int32_t count = CcLocalPresentedPeople(people, 64);
        for (int32_t i = 0; i < count; ++i) {
            bool duplicate = false;
            for (int32_t j = 0; j < local->interactions.count; ++j) {
                const CcInteractionTarget *other = &local->interactions.targets[j];
                if (other->key.kind == CC_INTERACTION_PERSON &&
                    (other->key.object == (UINT64_C(0x100000000) | (uint64_t)people[i].seed) ||
                    GridDistance((Vector2){other->x, other->z},
                        (Vector2){people[i].position.x, people[i].position.z}) < 0.65f)) duplicate = true;
            }
            if (duplicate || people[i].role == CC_NPC_ROLE_RAIDER) continue;
            (void)AdventureTarget(local, sim, CC_INTERACTION_PERSON,
                UINT64_C(0x100000000) | (uint64_t)people[i].seed, AdventureRoleName(people[i].role), "Talk",
                (Vector3){people[i].position.x, 0, people[i].position.z},
                0.9f, 2.0f, 0.7f,
                (Vector2){people[i].position.x, people[i].position.z}, 1.8f);
        }
    }
    for (int32_t i = 0; i < local->interactions.count; ++i)
        CcLocalProjectInteraction(&local->agent, texture, destination, &local->interactions.targets[i]);
}

static const CcInteractionTarget *AdventureFocus(const LocalState *local)
{
    const CcInteractionTarget *target = CcInteractionFind(&local->interactions,
        local->interaction.approaching ? local->interaction.pending : local->interaction.focus);
    return target;
}

static void AdventureActivate(CcSim *sim, LocalState *local, const CcInteractionTarget *target,
    ClientView *view, ClientView *return_view, int32_t *selected_situation)
{
    CcLocalAgentStop(&local->agent);
    CcInteractionCancel(&local->interaction, "");
    local->card_page = 0;
    *return_view = VIEW_LOCAL;
    switch (target->key.kind) {
        case CC_INTERACTION_PERSON:
            local->conversation_object = target->key.object;
            local->conversation_character_id = target->character_id;
            local->conversation_situation_id = target->situation_id;
            local->conversation_position = (Vector3){target->x, target->y, target->z};
            (void)snprintf(local->conversation_name, sizeof(local->conversation_name), "%s", target->name);
            (void)snprintf(local->conversation_line, sizeof(local->conversation_line),
                "Welcome. The %s has work and supplies. The notice board carries news from our neighbours.",
                CcLocalPlaceProfileForSettlement(CcSimSettlement(sim, sim->player.location_id))->primary_hall);
            local->agent.facing_yaw = atan2f(target->x - local->agent.position.x,
                target->z - local->agent.position.z);
            if (target->character_id != 0U) local->course.situation_witness.facing_yaw =
                local->agent.facing_yaw + PI;
            *view = VIEW_CHARACTER;
            break;
        case CC_INTERACTION_DOOR:
            local->market_interior = true;
            RepositionHero(local, (Vector2){2.05f, 5.35f}, true);
            local->interactions.count = 0;
            break;
        case CC_INTERACTION_EXIT:
            local->market_interior = false;
            RepositionHero(local, (Vector2){CC_LOCAL_MARKET_X, CC_LOCAL_MARKET_Z + 1.1f}, false);
            local->interactions.count = 0;
            break;
        case CC_INTERACTION_STOCK:
        case CC_INTERACTION_COUNTER:
            local->trade_quantity = 1;
            local->trade_good = target->key.kind == CC_INTERACTION_STOCK ?
                (CcGood)target->amount : ContextCargoGood(sim);
            local->trade_mode = 0;
            local->trade_confirmed = false;
            local->trade_quote_presented = false;
            *view = VIEW_TRADE;
            break;
        case CC_INTERACTION_BOARD:
            if (SelectedActiveSituation(sim, *selected_situation) == NULL)
                *selected_situation = FirstActiveSituationIndex(sim);
            *view = VIEW_SITUATIONS;
            break;
        case CC_INTERACTION_SITE:
            BeginSiteTravelState(local, (CcLocalSiteKind)target->key.object, false);
            break;
        case CC_INTERACTION_CARRIAGE: *view = VIEW_CARRIAGE; break;
        default: break;
    }
}

/* Called before legacy movement and command handling. A handled click is consumed. */
static bool HandleAdventureScene(CcSim *sim, LocalState *local,
    ClientView *view, ClientView *return_view, int32_t *selected_situation,
    const CcInteractionTarget *card_target, float dt,
    char *message, size_t capacity)
{
    if (*view != VIEW_LOCAL || !AdventureScene(local)) return false;
    Vector2 position = LocalPosition(local);
    if (ClientKeyPressed(KEY_ESCAPE) && local->interaction.approaching) {
        CcInteractionCancel(&local->interaction, "Approach cancelled.");
        CcLocalAgentStop(&local->agent);
        return true;
    }
    if (ClientKeyPressed(KEY_E))
        CcInteractionCycle(&local->interaction, &local->interactions,
            IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT) ? -1 : 1);
    Vector2 mouse = ClientPointerPosition();
    const CcInteractionTarget *hover = CcInteractionPick(&local->interactions, mouse.x, mouse.y);
    bool click = ClientMouseButtonPressed(MOUSE_BUTTON_LEFT);
    const CcInteractionTarget *focus = AdventureFocus(local);
    bool activate = ClientKeyPressed(KEY_F);
    const CcInteractionTarget *chosen = card_target != NULL ? card_target :
        click && hover != NULL ? hover : activate ? focus : NULL;
    if (chosen == NULL && activate) {
        float nearest = 3.0f;
        for (int32_t i = 0; i < local->interactions.count; ++i) {
            const CcInteractionTarget *candidate = &local->interactions.targets[i];
            float distance = GridDistance(position, (Vector2){candidate->approach_x, candidate->approach_z});
            if (candidate->visible && distance < nearest) { nearest = distance; chosen = candidate; }
        }
    }
    if (chosen != NULL) {
        (void)CcInteractionStart(&local->interaction, chosen, position.x, position.y);
        if (!local->interaction.approaching) {
            (void)snprintf(message, capacity, "%s Double-tap [N] to return to the caravan.",
                           local->interaction.feedback);
            return true;
        }
    } else if (click || IsKeyDown(KEY_W) || IsKeyDown(KEY_A) || IsKeyDown(KEY_S) ||
               IsKeyDown(KEY_D) || IsKeyDown(KEY_UP) || IsKeyDown(KEY_DOWN) ||
               IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_RIGHT)) {
        if (local->interaction.approaching) CcLocalAgentStop(&local->agent);
        CcInteractionCancel(&local->interaction, "");
    }
    if (!local->interaction.approaching) return false;
    CcInteractionKey pending = local->interaction.pending;
    const CcInteractionTarget *target = CcInteractionFind(&local->interactions, pending);
    if (CcInteractionAdvance(&local->interaction, &local->interactions, position.x, position.y, dt)) {
        if (target != NULL) AdventureActivate(sim, local, target, view, return_view, selected_situation);
        return true;
    }
    if (!local->interaction.approaching) {
        CcLocalAgentStop(&local->agent);
        (void)snprintf(message, capacity, "%s Double-tap [N] to return to the caravan.",
                           local->interaction.feedback);
        return true;
    }
    if (target != NULL && local->interaction.repath_seconds <= 0.0f &&
        (target->key.kind == CC_INTERACTION_PERSON || !local->agent.target_valid)) {
        bool walking = CcLocalAgentApproachInteraction(&local->agent,
            (Vector2){target->approach_x, target->approach_z}, target->radius * 0.9f, local->market_interior);
        local->interaction.repath_seconds = target->key.kind == CC_INTERACTION_PERSON ? 0.5f : 1.5f;
        if (!walking) {
            CcInteractionCancel(&local->interaction, "Choose a clear path closer to this target.");
            (void)snprintf(message, capacity, "%s Double-tap [N] to return to the caravan.",
                           local->interaction.feedback);
        }
    }
    (void)CcLocalWorldUpdate(&local->course, &local->agent, sim, dt,
        local->market_interior, !local->market_interior);
    return true;
}

static int AdventureTextSize(int base)
{
    return base + (adventure_preferences != NULL ? adventure_preferences->text_size * 2 : 0);
}

static int AdventureText(const char *text, int x, int y, int width, int size, Color color, bool draw)
{
    char line[256] = "";
    size_t used = 0;
    int start = y;
    const char *cursor = text;
    while (cursor != NULL && *cursor != '\0') {
        const char *end = cursor;
        while (*end != '\0' && *end != ' ' && *end != '\n') ++end;
        size_t length = (size_t)(end - cursor);
        if (length > 180U) length = 180U;
        char next[256];
        (void)snprintf(next, sizeof(next), "%s%s%.*s", line, used > 0 ? " " : "", (int)length, cursor);
        if (used > 0 && CcOverlayMeasureText(next, size) > width) {
            if (draw) CcOverlayDrawText(line, x, y, size, color);
            y += size + 7;
            used = 0;
            line[0] = '\0';
        }
        if (used > 0 && used < sizeof(line) - 1U) line[used++] = ' ';
        size_t copy = length < sizeof(line) - used - 1U ? length : sizeof(line) - used - 1U;
        memcpy(line + used, cursor, copy); used += copy; line[used] = '\0';
        cursor = end;
        if (*cursor == '\n') {
            if (draw) CcOverlayDrawText(line, x, y, size, color);
            y += size + 7;
            used = 0; line[0] = '\0';
        }
        if (*cursor != '\0') ++cursor;
    }
    if (used > 0) {
        if (draw) CcOverlayDrawText(line, x, y, size, color);
        y += size + 7;
    }
    return y - start;
}

static int AdventureWrap(const char *text, int x, int y, int width, int size, Color color)
{
    return AdventureText(text, x, y, width, size, color, true);
}

static Rectangle AdventurePanel(void)
{
    float width = fminf(980.0f, (float)GetScreenWidth() - 48.0f);
    float height = fminf(560.0f, (float)GetScreenHeight() - 100.0f);
    return (Rectangle){((float)GetScreenWidth() - width) * 0.5f,
        ((float)GetScreenHeight() - height) * 0.5f, width, height};
}

static bool AdventureHit(Rectangle bounds)
{
    return ClientMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(ClientPointerPosition(), bounds);
}

static void AdventureButton(Rectangle bounds, const char *label, bool enabled, bool active)
{
    ClientTouchAdd(bounds, label, enabled, active);
    bool hover = CheckCollisionPointRec(ClientPointerPosition(), bounds);
    DrawRectangleRounded(bounds, 0.12f, 4, active || (hover && enabled) ? PANEL_HOVER : PANEL_DEEP);
    DrawRectangleRoundedLinesEx(bounds, 0.12f, 4, active ? 2.0f : 1.0f,
        enabled ? active ? CC_GOLD : TEAL : MUTED);
    int size = AdventureTextSize(14);
    while (size > 12 && CcOverlayMeasureText(label, size) > (int)bounds.width - 24) --size;
    CcOverlayDrawText(label, (int)bounds.x + 12, (int)bounds.y + 12, size, enabled ? INK : MUTED);
}

static Rectangle AdventureTab(Rectangle panel, int index, int count)
{
    float width = (panel.width - 48.0f) / (float)count;
    return (Rectangle){panel.x + 24.0f + (float)index * width, panel.y + 65.0f, width - 8.0f, 44.0f};
}

static Rectangle AdventureClose(Rectangle panel)
{
    return (Rectangle){panel.x + panel.width - 156.0f, panel.y + 14.0f, 132.0f, 38.0f};
}

static void AdventurePanelTitle(Rectangle panel, const char *title)
{
    ClientTouchBegin();
    ClientTouchHeading(title, "Choose an action below.");
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BACKGROUND, 0.65f));
    DrawPanel(panel, PANEL_DEEP);
    CcOverlayDrawText(title, (int)panel.x + 24, (int)panel.y + 22, AdventureTextSize(23), CC_GOLD);
    AdventureButton(AdventureClose(panel), "Back  Esc", true, false);
}

typedef struct AdventureQuote {
    CcCommand command;
    CcMoney total;
    CcMoney reward;
    CcId promise_id;
    bool available;
    char reason[160];
} AdventureQuote;

static AdventureQuote AdventureTradeQuote(const CcSim *sim, const LocalState *local)
{
    AdventureQuote quote = {.command = {.kind = CC_COMMAND_TRADE,
        .good = local->trade_good, .amount = local->trade_quantity}};
    const CcSettlement *place = CcSimSettlement(sim, sim->player.location_id);
    const CcSituation *promise = CcSimAcceptedSituation(sim);
    const char *reason = "";
    if (place == NULL || local->trade_good < 0 || local->trade_good >= CC_GOOD_COUNT ||
        local->trade_quantity < 1 || local->trade_quantity > 999) {
        reason = "Choose a good and quantity.";
    } else {
        bool promised_here = promise != NULL && promise->target_id == place->id &&
            (promise->kind == CC_SITUATION_RELIEF_DELIVERY || promise->kind == CC_SITUATION_BLACK_MARKET_DELIVERY);
        bool matching = promised_here && promise->good == local->trade_good;
        int32_t amount = local->trade_quantity;
        if (local->trade_mode == 2) {
            if (!promised_here) reason = "Choose Deliver at the town named in your accepted promise.";
            else {
                amount = promise->quantity - promise->progress;
                quote.command.good = promise->good;
                quote.reward = promise->reward;
                quote.promise_id = promise->id;
                if (sim->resolved_journey_situation_id != promise->id)
                    reason = "Carry the promised load here from another town first.";
            }
        } else if (local->trade_mode == 1 && matching) {
            reason = "This cargo belongs to your promise. Choose Deliver to settle it.";
        }
        CcGood good = quote.command.good;
        quote.command.amount = local->trade_mode == 0 ? amount : -amount;
        int32_t unit = local->trade_mode == 0 ? place->price[good] : place->price[good] * 3 / 4;
        if (unit < 1) unit = 1;
        quote.total = (CcMoney)amount * unit;
        if (reason[0] == '\0' && local->trade_mode == 0) {
            CcPlayerCompany loaded = sim->player;
            loaded.cargo[good] += amount;
            if (place->stock[good] < amount) reason = "Choose a smaller quantity to match the available stock.";
            else if (CcPlayerCargoUsed(&loaded) > loaded.cargo_capacity) reason = "Make room in the carriage or choose a smaller quantity.";
            else if (sim->player.coins < quote.total) reason = "Choose a quantity your purse can cover.";
        } else if (reason[0] == '\0') {
            if (amount < 1 || sim->player.cargo[good] < amount) reason = "Bring the full quantity to the counter.";
            else if (place->market_coins < quote.total) reason = "The keeper needs more crowns to buy this quantity.";
        }
    }
    quote.available = reason[0] == '\0' && !local->trade_confirmed;
    (void)snprintf(quote.reason, sizeof(quote.reason), "%s", local->trade_confirmed ?
        "Trade complete. Choose another good or quantity for your next trade." : reason);
    return quote;
}

static Rectangle AdventureGoodBounds(Rectangle panel, int32_t good)
{
    float width = (panel.width * 0.47f - 36.0f) * 0.5f;
    float pitch = fminf(44.0f, (panel.height - 240.0f) / 7.0f);
    return (Rectangle){panel.x + 24.0f + (float)(good % 2) * (width + 6.0f),
        panel.y + 140.0f + (float)(good / 2) * pitch, width, pitch - 3.0f};
}

static Rectangle AdventureQuantityBounds(Rectangle panel, int index)
{
    return (Rectangle){panel.x + panel.width * 0.52f + (float)index * 68.0f,
        panel.y + 230.0f, 60.0f, 44.0f};
}

static Rectangle AdventureConfirmBounds(Rectangle panel)
{
    return (Rectangle){panel.x + panel.width * 0.52f, panel.y + 302.0f,
        panel.width * 0.48f - 24.0f, 46.0f};
}

static void DrawAdventureTrade(const CcSim *sim, LocalState *local,
    Texture2D economic_goods)
{
    Rectangle panel = AdventurePanel();
    const CcSettlement *place = CcSimSettlement(sim, sim->player.location_id);
    if (place == NULL) return;
    AdventureQuote quote = AdventureTradeQuote(sim, local);
    local->trade_quote_presented = true;
    local->trade_presented_total = quote.total;
    local->trade_presented_reward = quote.reward;
    local->trade_presented_promise_id = quote.promise_id;
    local->trade_presented_command = quote.command;
    AdventurePanelTitle(panel, CcLocalPlaceProfileForSettlement(place)->interior_service);
    const char *modes[] = {"Buy", "Sell", "Deliver promise"};
    ClientTouchHeading(CcLocalPlaceProfileForSettlement(place)->interior_service,
        TextFormat("%s %d %s. %s: %" PRId64 " crowns. Purse: %" PRId64 ". %s",
            modes[local->trade_mode], abs(quote.command.amount), CcGoodName(quote.command.good),
            local->trade_mode == 0 ? "Total cost" : "Sale value", quote.total,
            sim->player.coins, quote.reason));
    for (int i = 0; i < 3; ++i) AdventureButton(AdventureTab(panel, i, 3), modes[i], true, i == local->trade_mode);
    CcOverlayDrawText(local->trade_mode == 0 ? "WAREHOUSE STOCK" : "CARRIAGE LOAD",
        (int)panel.x + 24, (int)panel.y + 119, AdventureTextSize(11), MUTED);
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        Rectangle bounds = AdventureGoodBounds(panel, good);
        bool active = good == (int32_t)quote.command.good;
        int32_t quantity = local->trade_mode == 0 ? place->stock[good] : sim->player.cargo[good];
        ClientTouchAdd(bounds, TextFormat("%s: %d", CcGoodName((CcGood)good), quantity),
            local->trade_mode != 2, active);
        DrawRectangleRec(bounds, active ? PANEL_HOVER : PANEL);
        if (active) DrawRectangleLinesEx(bounds, 1.0f, CC_GOLD);
        (void)DrawEconomicGoodIcon(economic_goods, (CcGood)good,
            (Rectangle){bounds.x + 4, bounds.y + 4, 32, 32}, WHITE);
        int size = AdventureTextSize(12);
        const char *name = CcGoodName((CcGood)good);
        while (size > 10 && CcOverlayMeasureText(name, size) > (int)bounds.width - 47) --size;
        CcOverlayDrawText(name, (int)bounds.x + 42, (int)bounds.y + 4,
            size, active ? CC_GOLD : INK);
        CcOverlayDrawText(TextFormat("%d units", quantity),
            (int)bounds.x + 42, (int)bounds.y + 23, 12, quantity > 0 ? MUTED : DANGER);
    }
    int x = (int)(panel.x + panel.width * 0.52f), y = (int)panel.y + 134;
    (void)DrawEconomicGoodIcon(economic_goods, quote.command.good,
        (Rectangle){(float)x, (float)y - 2, 40, 40}, WHITE);
    int title_size = AdventureTextSize(18);
    const char *title = TextFormat("%s: %d %s", modes[local->trade_mode],
        abs(quote.command.amount), CcGoodName(quote.command.good));
    while (title_size > 12 && CcOverlayMeasureText(title, title_size) >
        (int)(panel.width * 0.48f) - 74) --title_size;
    CcOverlayDrawText(title, x + 48, y + 8, title_size, INK);
    CcOverlayDrawText(TextFormat("%s %" PRId64 " crowns", local->trade_mode == 0 ? "Total cost" : "Sale value", quote.total),
        x, y + 34, AdventureTextSize(17), CC_GOLD);
    CcOverlayDrawText(TextFormat("Purse %" PRId64 "  |  Cargo %d/%d", sim->player.coins,
        CcPlayerCargoUsed(&sim->player), sim->player.cargo_capacity), x, y + 66, AdventureTextSize(14), MUTED);
    if (local->trade_mode != 2) {
        AdventureButton(AdventureQuantityBounds(panel, 0), "-", local->trade_quantity > 1, false);
        AdventureButton(AdventureQuantityBounds(panel, 1), "+", local->trade_quantity < 999, false);
        CcOverlayDrawText("Quantity: - / + keys", x + 144, (int)panel.y + 244, AdventureTextSize(12), MUTED);
    } else {
        CcOverlayDrawText(TextFormat("Promise reward +%" PRId64 " crowns", quote.reward), x,
            (int)panel.y + 244, AdventureTextSize(14), TEAL);
    }
    AdventureButton(AdventureConfirmBounds(panel), TextFormat("%s  Enter", modes[local->trade_mode]), quote.available, false);
    CcOverlayDrawText(TextFormat("Warehouse %d  |  Carriage %d",
        place->stock[quote.command.good], sim->player.cargo[quote.command.good]),
        x, (int)panel.y + 278, AdventureTextSize(12), TEAL);
    (void)AdventureWrap(quote.reason, x, (int)panel.y + 360, (int)(panel.width * 0.48f - 28.0f), AdventureTextSize(13), MUTED);
    DrawLine((int)panel.x + 24, (int)(panel.y + panel.height) - 89,
        (int)(panel.x + panel.width) - 24, (int)(panel.y + panel.height) - 89, Fade(TEAL, 0.5f));
    (void)AdventureWrap(local->receipt[0] != '\0' ? local->receipt :
        "Buy loads goods from this warehouse into your carriage. Sell unloads them here.",
        (int)panel.x + 24, (int)(panel.y + panel.height) - 72,
        (int)panel.width - 48, AdventureTextSize(14), INK);
}

static bool HandleAdventureTrade(CcJournal *journal, CcSim *sim, LocalState *local,
    ClientView *view, char *message, size_t capacity)
{
    if (*view != VIEW_TRADE || ClientKeyPressed(KEY_F5)) return false;
    Rectangle panel = AdventurePanel();
    if (ClientKeyPressed(KEY_ESCAPE) || AdventureHit(AdventureClose(panel))) { *view = VIEW_LOCAL; return true; }
    bool changed = false;
    for (int i = 0; i < 3; ++i) {
        if (AdventureHit(AdventureTab(panel, i, 3)) || ClientKeyPressed(KEY_ONE + i)) {
            local->trade_mode = i; changed = true;
        }
    }
    if (local->trade_mode != 2) {
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
            if (AdventureHit(AdventureGoodBounds(panel, good))) { local->trade_good = (CcGood)good; changed = true; }
        }
        if (ClientKeyPressed(KEY_UP) || ClientKeyPressed(KEY_DOWN)) {
            local->trade_good = (CcGood)(((int32_t)local->trade_good + CC_GOOD_COUNT +
                (ClientKeyPressed(KEY_UP) ? -1 : 1)) % CC_GOOD_COUNT); changed = true;
        }
        int step = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT) ? 10 : 1;
        if (AdventureHit(AdventureQuantityBounds(panel, 0)) || ClientKeyPressed(KEY_MINUS)) {
            local->trade_quantity -= step; changed = true;
        }
        if (AdventureHit(AdventureQuantityBounds(panel, 1)) || ClientKeyPressed(KEY_EQUAL)) {
            local->trade_quantity += step; changed = true;
        }
        if (local->trade_quantity < 1) local->trade_quantity = 1;
        if (local->trade_quantity > 999) local->trade_quantity = 999;
    }
    if (changed) local->trade_confirmed = false;
    if (!changed && (AdventureHit(AdventureConfirmBounds(panel)) || ClientKeyPressed(KEY_ENTER))) {
        AdventureQuote quote = AdventureTradeQuote(sim, local);
        bool quote_matches = local->trade_quote_presented &&
            local->trade_presented_command.good == quote.command.good &&
            local->trade_presented_command.amount == quote.command.amount &&
            local->trade_presented_total == quote.total && local->trade_presented_reward == quote.reward &&
            local->trade_presented_promise_id == quote.promise_id;
        if (quote.available && !quote_matches) {
            (void)snprintf(local->receipt, sizeof(local->receipt), "The quote changed. Review the price and confirm again.");
        } else if (quote.available) {
            if (ApplyCommand(journal, sim, quote.command, message, capacity)) {
                local->trade_confirmed = true;
                if (local->trade_mode == 2) {
                    (void)snprintf(local->receipt, sizeof(local->receipt), "Delivery complete. %s", message);
                } else (void)snprintf(local->receipt, sizeof(local->receipt), "%s", message);
                local->receipt_serial += 1U;
                const CcLocalPlaceProfile *place = CcLocalPlaceProfileForSettlement(
                    CcSimSettlement(sim, sim->player.location_id));
                int32_t quantity = quote.command.amount < 0 ? -quote.command.amount : quote.command.amount;
                char words[CC_SPEECH_TEXT_CAPACITY];
                (void)snprintf(words, sizeof(words), local->trade_mode == 0 ?
                    "The %d %s are yours. Thank you." : "I have the %d %s. Thank you.", quantity, CcGoodName(quote.command.good));
                CcSpeech speech;
                if (CcSpeechTrade(sim, place->keeper_name, quote.command.good, quantity,
                    quote.total, local->trade_mode, words, &speech)) ClientSaySpeech(&speech);
            } else (void)snprintf(local->receipt, sizeof(local->receipt), "%s", message);
        } else (void)snprintf(message, capacity, "%s", quote.reason);
    }
    return true;
}

static bool AdventureKnownPerson(const CcSim *sim, const CcCharacter *person)
{
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        const CcSituation *situation = &sim->situations[i];
        if (!SituationVisibleToPlayer(sim, i) && situation->id != sim->player.accepted_situation_id) continue;
        if (person->id == situation->affected_character_id || person->id == situation->sponsor_character_id ||
            person->id == situation->witness_character_id) return true;
    }
    return false;
}

static int32_t AdventureBookPageSize(const LocalState *local)
{
    if (local->book_page == 0) return 2;
    if (local->book_page == 1) return 5;
    if (local->book_page == 2) return 8;
    return adventure_preferences != NULL && adventure_preferences->text_size > 0 ? 3 : 4;
}

static int32_t AdventureBookLastPage(const CcSim *sim, const LocalState *local)
{
    int32_t count = 0;
    if (local->book_page == 0) count = sim->quest_outcome_count;
    else if (local->book_page == 1) {
        for (int32_t i = 0; i < sim->character_count; ++i)
            if (AdventureKnownPerson(sim, &sim->characters[i])) count += 1;
    } else if (local->book_page == 2) {
        for (int32_t i = 0; i < CC_GOOD_COUNT; ++i) if (sim->player.cargo[i] > 0) count += 1;
    } else count = sim->event_count;
    int32_t page = AdventureBookPageSize(local);
    return count > 0 ? (count - 1) / page * page : 0;
}

static void DrawAdventureBook(const CcSim *sim, const LocalState *local)
{
    Rectangle panel = AdventurePanel();
    AdventurePanelTitle(panel, "Company Book");
    const char *pages[] = {"Promises", "People", "Cargo", "Journal"};
    for (int i = 0; i < 4; ++i) AdventureButton(AdventureTab(panel, i, 4), pages[i], true, local->book_page == i);
    int x = (int)panel.x + 28, y = (int)panel.y + 132;
    int width = (int)panel.width - 56;
    int size = AdventureTextSize(15);
    if (local->book_page == 0) {
        const CcSituation *promise = CcSimAcceptedSituation(sim);
        if (promise != NULL) {
            CcOverlayDrawText(TextFormat("Accepted  /  Due day %d", promise->deadline_day), x, y, size, TEAL);
            y += 36;
            char next[192] = "";
            SituationNextAction(sim, promise, next, sizeof(next));
            y += AdventureWrap(next, x, y, width, size + 2, INK) + 18;
            CcOverlayDrawText(TextFormat("Progress %d/%d  /  reward %" PRId64 " crowns", promise->progress,
                promise->quantity, promise->reward), x, y, size, CC_GOLD);
            y += 50;
        } else {
            y += AdventureWrap("Visit a notice board or speak to a neighbour to find your next promise.", x, y, width, size + 2, INK) + 24;
        }
        (void)AdventureWrap("The board holds local offers. Your accepted promise appears here. The Journal records the outcome.", x, y, width, size, MUTED);
        y += 64;
        int32_t shown = 0;
        for (int32_t i = sim->quest_outcome_count - 1 - local->book_offset; i >= 0 && shown < 2; --i) {
            const CcQuestOutcomeRecord *outcome = &sim->quest_outcomes[i];
            if (y > (int)(panel.y + panel.height) - 120) break;
            CcOverlayDrawText(TextFormat("%s  /  Day %d", CcQuestEndReasonName(outcome->end_reason), outcome->resolved_day), x, y, size, TEAL);
            y += 28;
            CcOverlayDrawText(TextFormat("%s  /  Progress %d of %d", SituationTitle(outcome->situation_kind),
                outcome->progress_value, outcome->progress_limit), x, y, size, INK);
            y += 42; shown += 1;
        }
    } else if (local->book_page == 1) {
        int32_t shown = 0, skipped = 0;
        for (int32_t i = 0; i < sim->character_count && shown < 5; ++i) {
            const CcCharacter *person = &sim->characters[i];
            if (!AdventureKnownPerson(sim, person)) continue;
            if (skipped++ < local->book_offset) continue;
            CcOverlayDrawText(person->name, x, y, size + 2, CC_GOLD);
            const CcSettlement *home = CcSimSettlement(sim, person->home_settlement_id);
            CcOverlayDrawText(TextFormat("%s  /  From %s", CcCharacterRoleName(person->role),
                home != NULL ? home->name : "the road"), x, y + 28, size, INK);
            y += 68; shown += 1;
        }
        if (shown == 0) (void)AdventureWrap("People named in the stories you have found appear here.", x, y, width, size, INK);
    } else if (local->book_page == 2) {
        CcOverlayDrawText(TextFormat("Purse %" PRId64 " crowns  /  Cargo %d of %d spaces", sim->player.coins,
            CcPlayerCargoUsed(&sim->player), sim->player.cargo_capacity), x, y, size + 2, CC_GOLD);
        y += 46;
        int32_t skipped = 0;
        for (int32_t i = 0; i < CC_GOOD_COUNT && y < (int)(panel.y + panel.height) - 74; ++i) {
            if (sim->player.cargo[i] <= 0) continue;
            if (skipped++ < local->book_offset) continue;
            CcOverlayDrawText(TextFormat("%d  %s", sim->player.cargo[i], CcGoodName((CcGood)i)), x, y, size, INK);
            y += 34;
        }
    } else {
        int32_t limit = AdventureBookPageSize(local);
        for (int32_t i = local->book_offset; i < sim->event_count && i < local->book_offset + limit; ++i) {
            const CcEvent *event = CcSimRecentEvent(sim, i);
            if (event == NULL) continue;
            CcOverlayDrawText(TextFormat("Day %d  /  %s", event->day, CcEventKindName(event->kind)), x, y, size - 1, TEAL);
            y += size + 7;
            y += AdventureWrap(event->text, x, y, width, size, INK) + 17;
        }
    }
    AdventureButton((Rectangle){panel.x + 24, panel.y + panel.height - 52, 132, 38}, "Previous Up", local->book_offset > 0, false);
    AdventureButton((Rectangle){panel.x + 166, panel.y + panel.height - 52, 142, 38}, "Next Down", local->book_offset < AdventureBookLastPage(sim, local), false);
    CcOverlayDrawText("1-4 pages  /  B or Tab closes", (int)(panel.x + panel.width) - 350,
        (int)(panel.y + panel.height) - 38, AdventureTextSize(12), MUTED);
}

static bool HandleAdventureBook(const CcSim *sim, LocalState *local, ClientView *view, ClientView *return_view)
{
    if (*view != VIEW_LEDGER || ClientKeyPressed(KEY_F5)) return false;
    Rectangle panel = AdventurePanel();
    if (ClientKeyPressed(KEY_ESCAPE) || ClientKeyPressed(KEY_B) || ClientKeyPressed(KEY_TAB) ||
        AdventureHit(AdventureClose(panel))) { *view = SafeOverlayReturnView(*return_view); return true; }
    for (int i = 0; i < 4; ++i) {
        if (ClientKeyPressed(KEY_ONE + i) || AdventureHit(AdventureTab(panel, i, 4))) {
            local->book_page = i; local->book_offset = 0;
        }
    }
    if (ClientKeyPressed(KEY_UP) || AdventureHit((Rectangle){panel.x + 24, panel.y + panel.height - 52, 132, 38}))
        local->book_offset = local->book_offset >= AdventureBookPageSize(local) ? local->book_offset - AdventureBookPageSize(local) : 0;
    if (ClientKeyPressed(KEY_DOWN) || AdventureHit((Rectangle){panel.x + 166, panel.y + panel.height - 52, 142, 38}))
        local->book_offset += AdventureBookPageSize(local);
    if (local->book_offset > AdventureBookLastPage(sim, local)) local->book_offset = AdventureBookLastPage(sim, local);
    return true;
}

static Rectangle AdventureMenuItem(Rectangle panel, int index)
{
    return (Rectangle){panel.x + 24.0f, panel.y + 78.0f + (float)index * 56.0f, panel.width * 0.45f, 46.0f};
}

static void DrawAdventurePause(const LocalState *local)
{
    Rectangle panel = AdventurePanel();
    AdventurePanelTitle(panel, CcCoopClientActive() ? "Company menu" : "Paused");
    AdventureButton(AdventureMenuItem(panel, 0), "Resume  Esc", true, false);
    AdventureButton(AdventureMenuItem(panel, 1), "Save game  F5", true, false);
    AdventureButton(AdventureMenuItem(panel, 2), "Company Book  B", true, false);
    if (adventure_preferences != NULL) {
        AdventureButton(AdventureMenuItem(panel, 3), adventure_preferences->reduced_motion ?
            "Motion: gentle  F4" : "Motion: full  F4", true, false);
        AdventureButton(AdventureMenuItem(panel, 4), TextFormat("Text size: %s  T", adventure_preferences->text_size == 0 ?
            "standard" : adventure_preferences->text_size == 1 ? "large" : "largest"), true, false);
        AdventureButton(AdventureMenuItem(panel, 5), adventure_preferences->focus_hints ? "Target hints: shown  H" : "Target hints: focus only  H", true, false);
        AdventureButton(AdventureMenuItem(panel, 6), adventure_preferences->audio_mode == 0 ? "Sound: full  F6" :
            adventure_preferences->audio_mode == 1 ? "Sound: effects  F6" : "Sound: muted  F6", true, false);
    }
    int x = (int)(panel.x + panel.width * 0.53f), y = (int)panel.y + 84;
    CcOverlayDrawText(CcCoopClientActive() ? "The host keeps company time" : "At your own pace", x, y, AdventureTextSize(20), CC_GOLD);
    y += 46;
    (void)AdventureWrap("Choose an object card or tap the object to approach and act. Browse more cards with the arrows. Click the ground to walk. Tap a road to turn, the carriage to stop or drive, and a camp to rest.\n\nE selects the next visible target. Shift+E goes back. F uses the selected action.\n\nB opens the Company Book. Q opens promises. M opens the map case.\n\nEsc cancels an approach or returns to the scene. F5 saves.",
        x, y, (int)(panel.width * 0.47f - 30.0f), AdventureTextSize(14), INK);
    (void)local;
}

static bool HandleAdventurePause(LocalState *local, ClientView *view, ClientView *return_view)
{
    if (*view != VIEW_PAUSE) return false;
    Rectangle panel = AdventurePanel();
    if (ClientKeyPressed(KEY_ESCAPE) || AdventureHit(AdventureClose(panel)) || AdventureHit(AdventureMenuItem(panel, 0))) {
        *view = local->pause_return_view; return true;
    }
    if (AdventureHit(AdventureMenuItem(panel, 1))) { local->request_save = true; return false; }
    if (ClientKeyPressed(KEY_F5)) return false;
    if (ClientKeyPressed(KEY_B) || AdventureHit(AdventureMenuItem(panel, 2))) {
        *return_view = VIEW_PAUSE; *view = VIEW_LEDGER; return true;
    }
    if (adventure_preferences != NULL) {
        if (AdventureHit(AdventureMenuItem(panel, 3))) {
            adventure_preferences->reduced_motion = !adventure_preferences->reduced_motion;
            adventure_preferences_dirty = true;
        }
        if (AdventureHit(AdventureMenuItem(panel, 4)) || ClientKeyPressed(KEY_T)) {
            adventure_preferences->text_size = (adventure_preferences->text_size + 1) % 3;
            adventure_preferences_dirty = true;
        }
        if (AdventureHit(AdventureMenuItem(panel, 5)) || ClientKeyPressed(KEY_H)) {
            adventure_preferences->focus_hints = !adventure_preferences->focus_hints;
            adventure_preferences_dirty = true;
        }
        if (AdventureHit(AdventureMenuItem(panel, 6))) {
            adventure_preferences->audio_mode = (adventure_preferences->audio_mode + 1) % 3;
            adventure_preferences_dirty = true;
        }
    }
    return true;
}

static Rectangle WorldActionLabelBounds(const CcInteractionTarget *target)
{
    int width = CcOverlayMeasureText(target->name, AdventureTextSize(14)) + 20;
    float x = (target->left + target->right) * 0.5f;
    float left = fmaxf(12, fminf(x - (float)width * 0.5f, (float)GetScreenWidth() - (float)width - 12));
    return (Rectangle){left, target->top - 33, (float)width, 28};
}

static const CcInteractionTarget *WorldActionPointerTarget(const LocalState *local, Vector2 mouse)
{
    for (int32_t i = 0; i < local->interactions.count; ++i) {
        const CcInteractionTarget *target = &local->interactions.targets[i];
        if (target->key.kind == CC_INTERACTION_ACTION && target->visible &&
            CheckCollisionPointRec(mouse, WorldActionLabelBounds(target))) return target;
    }
    return CcInteractionPick(&local->interactions, mouse.x, mouse.y);
}

static void DrawAdventureFocus(const CcSim *sim, const LocalState *local, ClientView view,
    int32_t selected_route, int32_t selected_situation)
{
    const CcInteractionTarget *focus = AdventureFocus(local);
    Vector2 mouse = ClientPointerPosition();
    const CcInteractionTarget *hover = WorldActionPointerTarget(local, mouse);
    ContextActionSet cards = BuildContextActions(sim, local, view, selected_route, selected_situation);
    int32_t first = ContextCardFirst(local, cards.count);
    int32_t shown = ContextCardCount(cards.count, first);
    for (int32_t card = first; card < first + shown; ++card) {
        if (!CheckCollisionPointRec(mouse, ContextActionBounds(card - first, shown))) continue;
        for (int32_t i = 0; i < local->interactions.count; ++i) {
            const CcInteractionTarget *target = &local->interactions.targets[i];
            if (CcInteractionKeyEqual(target->key, cards.items[card].target) ||
                (target->key.kind == CC_INTERACTION_ACTION && target->action == (int32_t)cards.items[card].kind &&
                 target->amount == cards.items[card].amount)) hover = target;
        }
    }
    if (hover != NULL) SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
    else SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    bool hints = adventure_preferences == NULL || adventure_preferences->focus_hints;
    for (int32_t i = 0; i < local->interactions.count; ++i) {
        const CcInteractionTarget *target = &local->interactions.targets[i];
        if (!target->visible) continue;
        bool selected = target == focus || target == hover;
        float x = (target->left + target->right) * 0.5f;
        if (selected) {
            Rectangle outline = {target->left, target->top, target->right - target->left, target->bottom - target->top};
            DrawRectangleRoundedLinesEx(outline, 0.12f, 4, 2.0f, target->available ? CC_GOLD : MUTED);
        } else if (hints) DrawCircle((int)x, (int)target->top - 6, 3, Fade(CC_GOLD, 0.85f));
        if (selected || target->key.kind == CC_INTERACTION_ACTION) {
            Rectangle label = WorldActionLabelBounds(target);
            DrawRectangleRec(label, PANEL_DEEP);
            CcOverlayDrawText(target->name, (int)label.x + 10, (int)label.y + 6, AdventureTextSize(14), INK);
        }
    }

}

static void DrawAdventureConversation(const CcSim *sim, const LocalState *local)
{
    CcSpeech speech;
    bool spoken = ClientConversationSpeech(sim, local, &speech);
    const CcSpeech *playing = CcAudioCurrentSpeech();
    if (playing != NULL) { speech = *playing; spoken = true; }
    const char *name = spoken ? speech.speaker : local->conversation_name;
    const char *line = spoken ? speech.text : local->conversation_line;
    ClientTouchHeading(name, line);
    float width = fminf(720.0f, (float)GetScreenWidth() - 80.0f);
    float height = 68.0f + (float)AdventureText(line, 0, 0, (int)width - 44,
        AdventureTextSize(17), INK, false);
    Rectangle bubble = {((float)GetScreenWidth() - width) * 0.5f,
        (float)GetScreenHeight() - 108.0f - height, width, height};
    DrawPanel(bubble, Fade(PANEL_DEEP, 0.95f));
    CcOverlayDrawText(name, (int)bubble.x + 22, (int)bubble.y + 16, AdventureTextSize(20), CC_GOLD);
    (void)AdventureWrap(line, (int)bubble.x + 22, (int)bubble.y + 50,
        (int)bubble.width - 44, AdventureTextSize(17), INK);
}

static Rectangle AdventureNavBounds(int index)
{
    return (Rectangle){(float)GetScreenWidth() - 420.0f + (float)index * 102.0f, 12, 96, 40};
}

static Rectangle AdventurePaceBounds(int32_t pace)
{
    return (Rectangle){(float)GetScreenWidth() - 420.0f + (float)pace * 136.0f, 58, 130, 28};
}

static void DrawAdventureHeader(const CcSim *sim, const LocalState *local)
{
    const CcSettlement *place = CcSimSettlement(sim, sim->player.location_id);
    const CcLocalPlaceProfile *profile = CcLocalPlaceProfileForSettlement(place);
    const CcSettlement *destination = CcSimSettlement(sim, sim->journey.destination_id);
    const char *title = local->site_kind != CC_LOCAL_SITE_NONE ? CcLocalSiteName(sim, local->site_kind) :
        local->journey_travel_active && destination != NULL ?
            TextFormat("%s to %s", place != NULL ? place->name : "Road", destination->name) :
        place != NULL ? place->name : "Crownless";
    int title_size = AdventureTextSize(22);
    int toolbar_width = 450;
    while (title_size > 12 && CcOverlayMeasureText(title, title_size) > GetScreenWidth() - toolbar_width) --title_size;
    CcOverlayDrawText(title, 24, 14, title_size, INK);
    CcOverlayDrawText(TextFormat("Day %d  /  %" PRId64 " crowns  /  Cargo %d/%d", sim->current_day,
        sim->player.coins, CcPlayerCargoUsed(&sim->player), sim->player.cargo_capacity),
        24, 43, AdventureTextSize(13), CC_GOLD);
    const CcRoadSite *stop = CcSimJourneyRoadSiteStop(sim);
    const char *detail = stop != NULL ? TextFormat("Passing %s / Choose a stop below", stop->name) :
        local->journey_travel_active && sim->journey.active ?
            TextFormat("%s / %d%% / ETA %dh / %s", local->carriage_stopped ? "Stopped" :
                CcJourneyPaceName(sim->journey.pace), sim->carriage.progress_milli / 10,
                (CcSimJourneyEtaMinutes(sim) + 59) / 60,
                sim->journey.phase == CC_JOURNEY_PHASE_RESTING ? "Time to rest" : "On the road") :
        local->road_choice_active ? "Choose a road card or tap its sign" :
        local->site_travel_active ? "Tap the carriage to stop or drive" :
        local->market_interior ? profile->interior_service : profile->identity;
    ClientTouchHeading(title, detail);
    CcOverlayDrawText(detail, 24, 67, AdventureTextSize(12), MUTED);
}

static void DrawAdventurePromises(const CcSim *sim, const LocalState *local, int32_t selected)
{
    Rectangle panel = AdventurePanel();
    panel.height -= 60.0f;
    panel.y -= 14.0f;
    AdventurePanelTitle(panel, "Local promises");
    const CcSituation *promise = SelectedActiveSituation(sim, selected);
    int x = (int)panel.x + 28, y = (int)panel.y + 85;
    int width = (int)panel.width - 56;
    if (promise == NULL) {
        (void)AdventureWrap("Listen to the people in town for your next lead.", x, y, width, AdventureTextSize(18), INK);
        return;
    }
    bool accepted = promise->id == sim->player.accepted_situation_id;
    bool offer = CcSimSituationCanAccept(sim, promise);
    CcOverlayDrawText(accepted ? "Accepted promise" : offer ? "Offer" : "Lead",
        x, y, AdventureTextSize(15), accepted ? TEAL : CC_GOLD);
    y += 38;
    CcOverlayDrawText(SituationTitle(promise->kind), x, y, AdventureTextSize(24), INK);
    y += 46;
    char target[96] = "";
    SituationTargetLabel(sim, promise, target, sizeof(target));
    y += AdventureWrap(TextFormat("%s  /  %s", target, promise->affected_name), x, y, width, AdventureTextSize(18), CC_GOLD) + 18;
    if (offer || accepted) {
        if (promise->kind == CC_SITUATION_RELIEF_DELIVERY || promise->kind == CC_SITUATION_BLACK_MARKET_DELIVERY) {
            CcOverlayDrawText(TextFormat("%d %s  /  Progress %d of %d", promise->quantity,
                CcGoodName(promise->good), promise->progress, promise->quantity), x, y, AdventureTextSize(17), INK);
            y += 38;
        }
        CcOverlayDrawText(TextFormat("Due day %d  /  Reward %" PRId64 " crowns", promise->deadline_day, promise->reward),
            x, y, AdventureTextSize(17), TEAL);
        y += 42;
    }
    char next[192] = "";
    SituationNextAction(sim, promise, next, sizeof(next));
    if (offer && !accepted && !local->open_world &&
        !CcClientPromiseCanBeAccepted(local->market_interior, GridDistance(LocalPosition(local), LOCAL_NOTICE))) {
        (void)snprintf(next, sizeof(next), "Visit the %s to accept this promise.",
            CcLocalPlaceProfileForSettlement(CcSimSettlement(sim, sim->player.location_id))->notice_board);
    }
    (void)AdventureWrap(next, x, y, width, AdventureTextSize(17), MUTED);
}

static void DrawAdventureFeedback(const char *message)
{
    int size = AdventureTextSize(14);
    float width = fminf(800.0f, (float)GetScreenWidth() - 60.0f);
    float x = ((float)GetScreenWidth() - width) * 0.5f;
    float height = 26.0f + (float)AdventureText(message, 0, 0, (int)width - 32, size, INK, false);
    float y = (float)GetScreenHeight() - 104.0f - height;
    DrawPanel((Rectangle){x, y, width, height}, Fade(PANEL_DEEP, 0.96f));
    (void)AdventureWrap(message, (int)x + 16, (int)y + 13, (int)width - 32, size, INK);
}

static Rectangle AdventurePromisesPanel(void)
{
    Rectangle panel = AdventurePanel();
    panel.height -= 60.0f;
    panel.y -= 14.0f;
    return panel;
}
