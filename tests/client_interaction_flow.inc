static int RunRoadJourneySaveRegression(void)
{
    static CcSim sim, loaded;
    static LocalState local, restored;
    const char *path = "road-journey-save.ccsave";
    const char *session_path = "road-journey-save.session";
    char error[256] = "", feedback[256] = "";
    (void)remove(path); (void)remove(session_path);
    CcSimInit(&sim, 42U);
    CcLocalBindPlace(&sim);
    ResetLocalState(&local);
    local.adventure_ui = true;
    CcJournal *journal = CcJournalStart(path, &sim, error, sizeof(error));
#define ROAD_SAVE_CHECK(condition, text) do { if (!(condition)) { \
    CcJournalAbandon(&journal); return ClientRegressionFailure(text); } } while (0)
    ROAD_SAVE_CHECK(journal != NULL, "Start the road save journal.");
    ROAD_SAVE_CHECK(SaveClientWorld(journal, &sim, &local, path, session_path,
        feedback, sizeof(feedback)), "Save the initial town checkpoint.");
    CcCommand travel = {.kind = CC_COMMAND_TRAVEL, .target_id = sim.settlements[1].id};
    ROAD_SAVE_CHECK(CcJournalApply(journal, &sim, &travel, error, sizeof(error)), "Start a real journey.");
    ROAD_SAVE_CHECK(CcJournalAdvanceRuntimeTicks(journal, &sim, 8, error, sizeof(error)), "Advance along the road.");
    ROAD_SAVE_CHECK(sim.journey.active && sim.journey.phase == CC_JOURNEY_PHASE_TRAVELLING,
        "The fixture must remain on the road.");
    ResetLocalState(&restored);
    ROAD_SAVE_CHECK(!RestoreLocalSession(session_path, &sim, &restored),
        "A previous town scene must yield to the active journey.");
    BeginRoadTravelState(&sim, &local);
    local.adventure_ui = true;
    local.agent.athletics.travel_training_distance = 5.5f;
    local.agent.combat.health = 77.0f;
    for (int scenario = 0; scenario < 5; ++scenario) {
        if (scenario == 2) {
            ROAD_SAVE_CHECK(InitializeOpenWorld(&sim, &local, false), "Build the world road scene.");
            BeginRoadTravelState(&sim, &local);
        }
        if (scenario == 4) {
            sim.journey.total_subticks = 2 * CC_WORLD_WATCH_SUBTICKS;
            sim.journey.elapsed_subticks = CC_WORLD_WATCH_SUBTICKS;
            sim.journey.phase = CC_JOURNEY_PHASE_RESTING;
            sim.clock.game_minutes_per_second = CC_IDLE_GAME_MINUTES_PER_SECOND;
            sim.carriage.mode = CC_CARRIAGE_STOPPED;
            sim.carriage.speed_milli_per_second = 0;
            sim.carriage.progress_milli = (int32_t)(
                (int64_t)sim.journey.elapsed_subticks * 1000 / sim.journey.total_subticks);
        }
        local.agent.combat.health = 77.0f;
        local.agent.athletics.travel_training_distance = 5.5f;
        local.convoy.pace = scenario % 2 ? 0.0f : 0.72f;
        uint64_t expected = CcSimHash(&sim);
        ROAD_SAVE_CHECK(SaveClientWorld(journal, &sim, &local, path, session_path,
            feedback, sizeof(feedback)), feedback);
        ROAD_SAVE_CHECK(CcSimHash(&sim) == expected, "Saving must preserve the world.");
        CcJournalAbandon(&journal);
        journal = CcJournalResume(path, &loaded, error, sizeof(error));
        ROAD_SAVE_CHECK(journal != NULL && CcSimHash(&loaded) == expected,
            "Resume the exact saved cargo, clock, and journey progress.");
        ResetLocalState(&restored);
        ClientView view = VIEW_LOCAL;
        int32_t selected = 0;
        ROAD_SAVE_CHECK(RestoreClientStartupSession(session_path, &loaded, &restored, &view, &selected),
            "Restore the road scene from its journey checkpoint.");
        ROAD_SAVE_CHECK(view == VIEW_LOCAL && restored.journey_travel_active &&
            restored.convoy.pace == 0.0f && restored.carriage_stopped && !restored.travel_fast_forward,
            "Resume with the carriage held until the player drives.");
        ROAD_SAVE_CHECK(SessionTestFloatMatches(restored.agent.athletics.travel_training_distance, 5.5f),
            "Keep the traveller's athletic progress.");
        ROAD_SAVE_CHECK(SessionTestFloatMatches(restored.agent.combat.health, 77.0f),
            "Keep the traveller's health on the road.");
        int32_t situation = 0;
        ClientView return_view = VIEW_LOCAL;
        float feedback_age = 0;
        restored.adventure_ui = true;
        HandleInput(&journal, &loaded, &selected, &situation, &view, &return_view,
            &restored, (RenderTexture2D){0}, (Rectangle){0}, 1.0f / 60.0f,
            path, session_path, error, sizeof(error), feedback, sizeof(feedback), &feedback_age);
        ROAD_SAVE_CHECK(CcSimHash(&loaded) == expected && restored.carriage_stopped,
            "An idle frame after loading must keep the saved world and stop state.");
        sim = loaded;
    }
    CcClientSession session;
    ROAD_SAVE_CHECK(CcClientSessionRead(session_path, &session, error, sizeof(error)), "Read the road checkpoint.");
    session.route_id = sim.routes[1].id;
    ROAD_SAVE_CHECK(CcClientSessionWrite(session_path, &session, error, sizeof(error)), "Write a different route fixture.");
    ROAD_SAVE_CHECK(!RestoreLocalSession(session_path, &sim, &restored), "Reject a checkpoint for another road.");
    ROAD_SAVE_CHECK(CcJournalClose(&journal, &sim, error, sizeof(error)), "Close the road journal.");
    (void)remove(path); (void)remove(session_path);
    LeaveOpenWorld(&local); LeaveOpenWorld(&restored);
#undef ROAD_SAVE_CHECK
    puts("Moving and stopped road saves resume exact progress with the team held.");
    return 0;
}

static int RunDeliveryHandoffRegression(void)
{
    static CcSim sim;
    static LocalState local;
    char error[192];
    CcSimInit(&sim, 42U);
    ResetLocalState(&local);
    local.adventure_ui = true;
    const CcSituation *promise = NULL;
    for (int i = 0; i < sim.situation_count; ++i)
        if (sim.situations[i].kind == CC_SITUATION_RELIEF_DELIVERY) promise = &sim.situations[i];
    if (promise == NULL || !CcSimApply(&sim, &((CcCommand){
        .kind = CC_COMMAND_ACCEPT_SITUATION, .target_id = promise->id}), error, sizeof(error)))
        return ClientRegressionFailure("Accept the delivery fixture.");
    if (AdventureDeliveryAtHand(&sim) != NULL)
        return ClientRegressionFailure("Departure still needs the road journey.");
    sim.player.location_id = promise->target_id;
    sim.carriage.location_id = promise->target_id;
    sim.resolved_journey_situation_id = promise->id;
    Vector2 position = LocalPosition(&local);
    CcInteractionTarget *door = AdventureTarget(&local, &sim, CC_INTERACTION_DOOR, 1,
        "Market hall", "Deliver promise", (Vector3){0}, 1, 2, 1,
        (Vector2){position.x + 30, position.y}, 1);
    CcInteractionTarget *person = AdventureTarget(&local, &sim, CC_INTERACTION_PERSON, 2,
        "Neighbour", "Talk", (Vector3){0}, 1, 2, 1, position, 1);
    if (door == NULL || person == NULL) return ClientRegressionFailure("Create the handoff targets.");
    ContextActionSet actions = BuildContextActions(&sim, &local, VIEW_LOCAL, 0, 0);
    if (actions.count != 2 || actions.items[0].target.kind != CC_INTERACTION_DOOR)
        return ClientRegressionFailure("The market handoff must lead the arrival tray.");
    int32_t cargo = sim.player.cargo[promise->good];
    sim.player.cargo[promise->good] = promise->quantity - 1;
    actions = BuildContextActions(&sim, &local, VIEW_LOCAL, 0, 0);
    if (AdventureDeliveryAtHand(&sim) != NULL || actions.items[0].target.kind != CC_INTERACTION_PERSON)
        return ClientRegressionFailure("A partial load keeps the ordinary nearby actions.");
    sim.player.cargo[promise->good] = cargo;
    sim.carriage.location_id = sim.settlements[0].id;
    if (AdventureDeliveryAtHand(&sim) != NULL)
        return ClientRegressionFailure("The handoff requires the loaded carriage here.");
    sim.carriage.location_id = promise->target_id;
    ClientView view = VIEW_LOCAL, back = VIEW_LOCAL;
    int32_t selected = 0;
    uint64_t before = CcSimHash(&sim);
    AdventureActivate(&sim, &local, door, &view, &back, &selected);
    if (!local.market_interior) return ClientRegressionFailure("The handoff door enters the hall.");
    CcInteractionTarget *counter = AdventureTarget(&local, &sim, CC_INTERACTION_COUNTER, 1,
        "Keeper", "Deliver promise", (Vector3){0}, 1, 2, 1, position, 1);
    AdventureActivate(&sim, &local, counter, &view, &back, &selected);
    if (view != VIEW_TRADE || local.trade_mode != 2 || local.trade_good != promise->good ||
        local.trade_quantity != promise->quantity - promise->progress || local.trade_quote_presented)
        return ClientRegressionFailure("The counter must open the delivery quote for the promised load.");
    AdventureQuote quote = AdventureTradeQuote(&sim, &local);
    if (quote.promise_id != promise->id || quote.command.amount != -promise->quantity ||
        CcSimHash(&sim) != before)
        return ClientRegressionFailure("Opening the handoff preserves cargo and coins until confirmation.");
    sim.player.accepted_situation_id = 0;
    AdventureActivate(&sim, &local, counter, &view, &back, &selected);
    if (local.trade_mode != 0)
        return ClientRegressionFailure("An ordinary counter visit opens the usual trade mode.");
    sim.player.accepted_situation_id = promise->id;
    CcLocalBindPlace(&sim);
    for (int interior = 0; interior < 2; ++interior) {
        local.market_interior = interior != 0;
        BuildAdventureTargets(&sim, &local, (RenderTexture2D){0}, (Rectangle){0});
        actions = BuildContextActions(&sim, &local, VIEW_LOCAL, 0, 0);
        CcInteractionKind kind = interior ? CC_INTERACTION_COUNTER : CC_INTERACTION_DOOR;
        if (actions.count == 0 || actions.items[0].target.kind != kind ||
            strcmp(actions.items[0].detail, "Deliver promise") != 0)
            return ClientRegressionFailure("The real town and hall trays must show the handoff first.");
    }
    return 0;
}

static int RunPresentedTargetRegression(void)
{
    static CcSim sim;
    static LocalState local;
    CcSimInit(&sim, 42U);
    ResetLocalState(&local);
    local.adventure_ui = true;
    Vector2 origin = LocalPosition(&local);
    for (int i = 0; i < 6; ++i) {
        CcInteractionTarget *target = AdventureTarget(&local, &sim,
            CC_INTERACTION_PERSON, (uint64_t)i + 1U, "Neighbour", "Talk",
            (Vector3){0}, 1, 2, 1, (Vector2){origin.x + (float)i + 1, origin.y}, 1);
        if (target == NULL) return ClientRegressionFailure("Create the card targets.");
        target->character_id = sim.characters[i].id;
    }
    ContextActionSet shown = BuildContextActions(&sim, &local, VIEW_LOCAL, 0, 0);
    RememberPresentedTargets(&local, VIEW_LOCAL, &shown);
    if (local.presented_target_count != 4)
        return ClientRegressionFailure("Show four nearby target cards.");
    CcInteractionKey chosen = local.presented_targets[0];
    Rectangle bounds = local.presented_target_bounds[0];
    client_test_pointer_active = true;
    client_test_pointer = (Vector2){bounds.x + bounds.width / 2, bounds.y + bounds.height / 2};
    /* Walking moves the shown target outside the newly nearest four. */
    local.interactions.targets[0].approach_x = origin.x + 100;
    ContextActionSet moved = BuildContextActions(&sim, &local, VIEW_LOCAL, 0, 0);
    for (int i = 0; i < 4; ++i)
        if (!CcInteractionKeyEqual(moved.items[i].target, shown.items[i].target))
            return ClientRegressionFailure("Resident movement keeps the visible card order.");
    local.presented_card_origin.x -= 4.0f;
    local.interaction.approaching = true;
    moved = BuildContextActions(&sim, &local, VIEW_LOCAL, 0, 0);
    if (!CcInteractionKeyEqual(moved.items[0].target, chosen))
        return ClientRegressionFailure("An approach keeps the row steady across distance changes.");
    local.interaction.approaching = false;
    moved = BuildContextActions(&sim, &local, VIEW_LOCAL, 0, 0);
    if (CcInteractionKeyEqual(moved.items[0].target, chosen))
        return ClientRegressionFailure("A new nearby area refreshes the distance ordering.");
    queued_mouse_button_press[MOUSE_BUTTON_LEFT] = true;
    ContextAction pressed = PressedContextAction(&sim, &local, VIEW_LOCAL, 0, 0);
    if (pressed.kind != CONTEXT_ACTION_WORLD_TARGET || !CcInteractionKeyEqual(pressed.target, chosen))
        return ClientRegressionFailure("Click the target shown before walking reordered the tray.");
    local.interactions.targets[0].available = false;
    (void)snprintf(local.interactions.targets[0].reason, sizeof(local.interactions.targets[0].reason), "The shop is closed.");
    pressed = PressedContextAction(&sim, &local, VIEW_LOCAL, 0, 0);
    if (pressed.enabled || strcmp(pressed.detail, "The shop is closed.") != 0)
        return ClientRegressionFailure("Use the shown target's current availability.");
    local.interactions.targets[0].character_id = sim.characters[6].id;
    pressed = PressedContextAction(&sim, &local, VIEW_LOCAL, 0, 0);
    if (pressed.kind != CONTEXT_ACTION_NONE)
        return ClientRegressionFailure("A new resident must receive a freshly drawn card.");
    local.interactions.targets[0].key.object = 99;
    pressed = PressedContextAction(&sim, &local, VIEW_LOCAL, 0, 0);
    if (pressed.kind != CONTEXT_ACTION_NONE || !PointerOverContextAction(
        &sim, &local, VIEW_LOCAL, 0, 0, client_test_pointer))
        return ClientRegressionFailure("Consume a removed card's click at its shown bounds.");
    RememberPresentedTargets(&local, VIEW_LOCAL, &moved);
    pressed = PressedContextAction(&sim, &local, VIEW_LOCAL, 0, 0);
    if (!CcInteractionKeyEqual(pressed.target, moved.items[0].target))
        return ClientRegressionFailure("A new drawing must replace the presented target.");
    RememberPresentedTargets(&local, VIEW_CHARACTER, &moved);
    if (local.presented_target_count != 0)
        return ClientRegressionFailure("Clear world cards when drawing another view.");
    ResetLocalState(&local);
    client_test_pointer_active = false;
    ClientInputClearPressed();
    return 0;
}

static int RunPresentedRoadRegression(void)
{
    static CcSim sim;
    static LocalState local;
    CcSimInit(&sim, 42U);
    ResetLocalState(&local);
    local.journey_travel_active = true;
    ContextActionSet shown = {0};
    AddDetailedContextAction(&shown, CONTEXT_ACTION_CLEAR_ROAD_SITE,
        "Clear rocks", "", "Tools needed", false, false);
    AddDetailedContextAction(&shown, CONTEXT_ACTION_CAMP_ROAD_SITE,
        "Camp at Low Silver Pit", "", "Rest team", true, false);
    AddDetailedContextAction(&shown, CONTEXT_ACTION_HOLD_TRAVEL,
        "Travel", "", "Hold to speed up time", true, false);
    RememberRoadActions(&sim, &local, VIEW_LOCAL, &shown);
    Rectangle bounds = local.presented_road_bounds[1];
    Vector2 mouse = {bounds.x + bounds.width / 2, bounds.y + bounds.height / 2};
    sim.journey.active = true;
    client_test_pointer_active = true;
    client_test_pointer = mouse;
    queued_mouse_button_press[MOUSE_BUTTON_LEFT] = true;
    ContextAction through_input = PressedContextAction(&sim, &local, VIEW_LOCAL, 0, 0);
    client_test_pointer_active = false;
    ClientInputClearPressed();
    if (through_input.kind != CONTEXT_ACTION_NONE)
        return ClientRegressionFailure("Road input consumes the old camp bounds when only Drive remains.");
    Rectangle removed_bounds = local.presented_road_bounds[0];
    Vector2 removed_pointer = {removed_bounds.x + 2, removed_bounds.y + 2};
    if (!PointerOverContextAction(&sim, &local, VIEW_LOCAL, 0, 0, removed_pointer))
        return ClientRegressionFailure("A removed road card keeps its drawn bounds inside the UI.");
    ContextActionSet branch = shown;
    branch.items[1].kind = CONTEXT_ACTION_VISIT_MINE;
    (void)snprintf(branch.items[1].label, sizeof(branch.items[1].label), "Turn to the mine");
    ContextAction pressed = {0};
    if (!PresentedRoadActionAt(&sim, &local, VIEW_LOCAL, &branch, mouse, &pressed) ||
        pressed.kind != CONTEXT_ACTION_NONE)
        return ClientRegressionFailure("A drawn camp card consumes its click when the mine branch replaces it.");
    branch.items[2] = shown.items[1];
    branch.items[2].enabled = false;
    if (!PresentedRoadActionAt(&sim, &local, VIEW_LOCAL, &branch, mouse, &pressed) ||
        pressed.kind != CONTEXT_ACTION_CAMP_ROAD_SITE || pressed.enabled)
        return ClientRegressionFailure("A moved road choice retains its identity and current availability.");
    RememberRoadActions(&sim, &local, VIEW_LOCAL, &branch);
    if (!PresentedRoadActionAt(&sim, &local, VIEW_LOCAL, &branch, mouse, &pressed) ||
        pressed.kind != CONTEXT_ACTION_VISIT_MINE)
        return ClientRegressionFailure("Drawing the branch makes its mine choice clickable.");
    sim.journey.route_id++;
    if (!PresentedRoadActionAt(&sim, &local, VIEW_LOCAL, &branch, mouse, &pressed) ||
        pressed.kind != CONTEXT_ACTION_NONE)
        return ClientRegressionFailure("A road card belongs to its drawn route.");
    RememberRoadActions(&sim, &local, VIEW_CHARACTER, &branch);
    if (local.presented_road_count != 0)
        return ClientRegressionFailure("Another view clears the road cards.");
    RememberRoadActions(&sim, &local, VIEW_LOCAL, &branch);
    ResetLocalState(&local);
    if (local.presented_road_count != 0)
        return ClientRegressionFailure("Reset clears the road cards.");
    return 0;
}

static int RunConversationFocusRegression(void)
{
    static CcSim sim;
    static LocalState local;
    CcSimInit(&sim, 42U);
    ResetLocalState(&local);
    local.adventure_ui = true;
    local.course.situation_witness_active = true;
    local.course.situation_witness.position = (Vector3){-12, 0, -8};
    CcInteractionTarget target = {.key = {sim.player.location_id, 1, CC_INTERACTION_PERSON},
        .character_id = sim.characters[0].id, .x = 9, .y = 0, .z = 13, .available = true};
    ClientView view = VIEW_LOCAL, back = VIEW_LOCAL;
    int32_t selected = 0;
    AdventureActivate(&sim, &local, &target, &view, &back, &selected);
    const Vector3 *focus = LocalConversationFocus(&local, view);
    if (focus == NULL || focus->x != target.x || focus->z != target.z ||
        local.conversation_character_id != target.character_id)
        return ClientRegressionFailure("Frame the named speaker selected in the scene.");
    target.character_id = 0;
    target.x = 4;
    AdventureActivate(&sim, &local, &target, &view, &back, &selected);
    focus = LocalConversationFocus(&local, view);
    if (focus == NULL || focus->x != target.x)
        return ClientRegressionFailure("Unnamed speakers keep their selected position.");
    if (LocalConversationFocus(&local, back) != NULL)
        return ClientRegressionFailure("Return to the street camera after conversation.");
    local.adventure_ui = false;
    if (LocalConversationFocus(&local, VIEW_CHARACTER) != NULL)
        return ClientRegressionFailure("The classic conversation keeps its witness camera.");
    ResetLocalState(&local);
    if (local.conversation_object != 0 || local.conversation_position.x != 0 ||
        local.conversation_position.y != 0 || local.conversation_position.z != 0)
        return ClientRegressionFailure("A reset clears the previous speaker position.");
    return 0;
}

static int RunAdventureInputRegression(void)
{
    int road_result = RunPresentedRoadRegression();
    if (road_result != 0) return road_result;
    int focus_result = RunConversationFocusRegression();
    if (focus_result != 0) return focus_result;
    int handoff_result = RunDeliveryHandoffRegression();
    if (handoff_result != 0) return handoff_result;
    int presented_result = RunPresentedTargetRegression();
    if (presented_result != 0) return presented_result;
    static CcSim sim;
    static LocalState local;
    char message[256] = "", feedback[256] = "", error[192] = "";
    float feedback_age = 0;
    int32_t selected = 0, selected_situation = 0;
    ClientView view = VIEW_LOCAL, return_view = VIEW_LOCAL;
    const char *path = "interaction-input.ccsave";
    (void)remove(path);
    CcSimInit(&sim, 42U);
    /* The chat flow needs several ordinary accounts to advance through. */
    for (int i = 0; i < 2; ++i) {
        CcEvent *account = &sim.events[sim.event_write_index];
        *account = (CcEvent){
            .id = CcMakeId(CC_ENTITY_EVENT, sim.next_entity_serial++),
            .day = sim.current_day, .kind = CC_EVENT_KINGDOM_ACTION,
            .subject_id = sim.kingdoms[0].id,
            .location_id = sim.player.location_id, .magnitude = 40
        };
        (void)snprintf(account->text, sizeof(account->text),
            "A traveller brings account %d from the town granary.", i + 1);
        sim.event_write_index = (sim.event_write_index + 1) % CC_MAX_EVENTS;
        if (sim.event_count < CC_MAX_EVENTS) sim.event_count += 1;
    }
    CcSimRefreshCharacterGossip(&sim);

    CcLocalBindPlace(&sim);
    ResetLocalState(&local);
    local.adventure_ui = true;
    CcJournal *journal = CcJournalStart(path, &sim, error, sizeof(error));
    if (journal == NULL) return ClientRegressionFailure(error);
#define UX_CHECK(condition, text) do { if (!(condition)) { (void)CcJournalClose(&journal, &sim, error, sizeof(error)); (void)remove(path); return ClientRegressionFailure(text); } } while (0)
#define UX_KEY(key) do { if (view == VIEW_TRADE) { \
    AdventureQuote shown = AdventureTradeQuote(&sim, &local); local.trade_quote_presented = true; \
    local.trade_presented_command = shown.command; local.trade_presented_total = shown.total; local.trade_presented_reward = shown.reward; } \
    queued_key_press[key] = true; \
    HandleInput(&journal, &sim, &selected, &selected_situation, &view, &return_view, &local, \
        (RenderTexture2D){0}, (Rectangle){0}, 1.0f / 60.0f, path, "", message, sizeof(message), \
        feedback, sizeof(feedback), &feedback_age); ClientInputClearPressed(); } while (0)
    CcInteractionTarget *target = AdventureTarget(&local, &sim, CC_INTERACTION_PERSON, 1,
        "Test neighbour", "Talk", local.agent.position, 1, 2, 1, LocalPosition(&local), 1.8f);
    target->visible = true;
    local.interaction.focus = target->key;
    local.course.alarm_active = true;
    for (int i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) local.course.raiders[i].position = (Vector3){1, 0, 1};
    UX_CHECK(!LocalCombatActive(&local), "Fixture needs a distant alarm.");
    UX_KEY(KEY_F);
    UX_CHECK(view == VIEW_CHARACTER, "A distant alarm must allow the offered conversation.");
    UX_KEY(KEY_B);
    UX_CHECK(view == VIEW_LEDGER, "Book must open from conversation.");
    UX_KEY(KEY_ESCAPE);
    UX_CHECK(view == VIEW_CHARACTER, "Book back must restore the conversation.");
    UX_KEY(KEY_ESCAPE);
    UX_CHECK(view == VIEW_LOCAL, "Conversation back must restore the scene.");
    Vector2 pointer = GetMousePosition();
    target->left = pointer.x - 10; target->right = pointer.x + 10;
    target->top = pointer.y - 10; target->bottom = pointer.y + 10;
    local.interaction.focus = (CcInteractionKey){0};
    queued_mouse_button_press[MOUSE_BUTTON_LEFT] = true;
    HandleInput(&journal, &sim, &selected, &selected_situation, &view, &return_view, &local,
        (RenderTexture2D){0}, (Rectangle){0}, 1.0f / 60.0f, path, "", message, sizeof(message),
        feedback, sizeof(feedback), &feedback_age);
    ClientInputClearPressed();
    UX_CHECK(view == VIEW_CHARACTER, "A scene click must open the same conversation as F.");
    UX_KEY(KEY_ESCAPE);
    CcCharacter *speaker = NULL;
    for (int32_t i = 0; i < sim.character_count; ++i) {
        if (sim.characters[i].current_settlement_id == sim.player.location_id &&
            sim.characters[i].activity != CC_CHARACTER_ACTIVITY_TRAVELLING &&
            CcCharacterAgeYears(&sim, &sim.characters[i]) >= 16) { speaker = &sim.characters[i]; break; }
    }
    UX_CHECK(speaker != NULL, "The starting town needs a resident to exchange news.");
    target->character_id = speaker->id;
    local.interaction.focus = target->key;
    UX_KEY(KEY_F);
    UX_CHECK(view == VIEW_CHARACTER, "A resident must open their own conversation.");
    CcSpeech report, source;
    UX_CHECK(ClientConversationSpeech(&sim, &local, &report) && report.source_event_id != 0U,
        "A resident must speak a local report with its event source.");
    ContextActionSet replies = BuildContextActions(&sim, &local, view, selected, selected_situation);
    UX_CHECK(replies.count == 2 && replies.items[0].kind == CONTEXT_ACTION_GOSSIP_CHAT &&
        replies.items[1].kind == CONTEXT_ACTION_CLOSE_VIEW,
        "The conversation offers Chat and Farewell.");
    int32_t first_slot = CcSimNextUntoldStory(&sim, local.conversation_character_id, NULL);
    UX_KEY(KEY_ONE);
    UX_CHECK(CcSimStoryTold(&sim, local.conversation_character_id, first_slot),
        "Chat marks the shared account heard.");
    UX_CHECK(CcSimGossipCarrier(&sim, sim.player.id) != NULL,
        "Chat exchanges news with the player's company.");
    UX_CHECK(ClientConversationSpeech(&sim, &local, &source), "Chat presents the next account.");
    replies = BuildContextActions(&sim, &local, view, selected, selected_situation);
    Rectangle source_card = ContextActionBounds(0, ContextCardCount(&replies, 0), replies.combat);
    client_test_pointer_active = true;
    client_test_pointer = (Vector2){source_card.x + source_card.width * 0.5f,
        source_card.y + source_card.height * 0.5f};
    int32_t next_slot = local.conversation_gossip_slot;
    queued_mouse_button_press[MOUSE_BUTTON_LEFT] = true;
    HandleInput(&journal, &sim, &selected, &selected_situation, &view, &return_view, &local,
        (RenderTexture2D){0}, (Rectangle){0}, 1.0f / 60.0f, path, "", message, sizeof(message),
        feedback, sizeof(feedback), &feedback_age);
    client_test_pointer_active = false;
    ClientInputClearPressed();
    UX_CHECK(next_slot < 0 || CcSimStoryTold(&sim, local.conversation_character_id, next_slot),
        "The Chat card shares news through the same action as its key.");
    UX_KEY(KEY_ESCAPE);
    local.course.alarm_active = false;
    local.course.automatic_alarm = false;
    target->key.kind = CC_INTERACTION_DOOR;
    target->approach_x = CC_LOCAL_MARKET_X;
    target->approach_z = CC_LOCAL_MARKET_Z + 0.6f;
    target->radius = 1.1f;
    local.interaction.focus = target->key;
    UX_KEY(KEY_F);
    for (int frame = 0; frame < 1800 && !local.market_interior && local.interaction.approaching; ++frame) {
        HandleInput(&journal, &sim, &selected, &selected_situation, &view, &return_view, &local,
            (RenderTexture2D){0}, (Rectangle){0}, 1.0f / 60.0f, path, "", message, sizeof(message),
            feedback, sizeof(feedback), &feedback_age);
    }
    if (!local.market_interior) {
        fprintf(stderr, "nav active=%d index=%d count=%d target=(%.2f,%.2f) radius=%.2f grounded=%d health=%.1f\n", local.agent.navigation_active, local.agent.navigation_point_index, local.agent.navigation_point_count, local.agent.target_point.x, local.agent.target_point.z, local.agent.radius, local.agent.grounded, local.agent.combat.health);
        for (int n = 0; n < local.agent.navigation_point_count; ++n) fprintf(stderr, "node %d %.2f %.2f\n", n, local.agent.navigation_point[n].x, local.agent.navigation_point[n].z);
        fprintf(stderr, "approach: x=%.2f z=%.2f pending=%d elapsed=%.2f message=%s feedback=%s\n", local.agent.position.x, local.agent.position.z, local.interaction.approaching, local.interaction.elapsed_seconds, message, local.interaction.feedback);
    }
    UX_CHECK(local.market_interior && view == VIEW_LOCAL, "Walking to the door must finish by entering its interior.");
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        Vector3 bay = CcLocalWarehouseGoodPosition((CcGood)good);
        CcLocalAgent shopper;
        CcLocalAgentInit(&shopper, (Vector2){4.5f, 4.2f}, true);
        UX_CHECK(CcLocalAgentSetExactTarget(&shopper, (Vector3){bay.x, 0, 2.85f}, true),
            "Every warehouse bay needs an open approach path.");
        CcInteractionTarget stock_target = {.key = {sim.player.location_id, (uint64_t)good + 1U,
            CC_INTERACTION_STOCK}, .amount = good};
        AdventureActivate(&sim, &local, &stock_target, &view, &return_view, &selected_situation);
        UX_CHECK(view == VIEW_TRADE && local.trade_good == (CcGood)good &&
            local.trade_quantity == 1 && !local.trade_confirmed && !local.trade_quote_presented,
            "A warehouse bay must open a fresh quote for its own good.");
        view = VIEW_LOCAL;
    }
    local.interactions.count = 0;
    target = AdventureTarget(&local, &sim, CC_INTERACTION_COUNTER, 1,
        "Keeper", "Trade", local.agent.position, 2, 2, 1, LocalPosition(&local), 1);
    target->visible = true;
    local.interaction.focus = target->key;
    UX_KEY(KEY_F);
    UX_CHECK(view == VIEW_TRADE, "Counter action must open explicit trade.");
    UX_KEY(KEY_B);
    UX_CHECK(view == VIEW_LEDGER, "Book must open from trade.");
    UX_KEY(KEY_ESCAPE);
    UX_CHECK(view == VIEW_TRADE, "Book back must restore trade.");
    local.trade_good = CC_GOOD_FOOD;
    local.trade_quantity = 1;
    AdventureQuote quote = AdventureTradeQuote(&sim, &local);
    UX_CHECK(quote.available, "Initial market must quote one food unit.");
    CcMoney before = sim.player.coins;
    int32_t cargo = sim.player.cargo[CC_GOOD_FOOD];
    int32_t warehouse_stock = CcSimSettlement(&sim, sim.player.location_id)->stock[CC_GOOD_FOOD];
    local.trade_quote_presented = true;
    local.trade_presented_command = quote.command;
    local.trade_presented_total = quote.total + 1;
    local.trade_presented_reward = quote.reward;
    queued_key_press[KEY_ENTER] = true;
    (void)HandleAdventureTrade(journal, &sim, &local, &view, message, sizeof(message));
    ClientInputClearPressed();
    UX_CHECK(sim.player.coins == before && strstr(local.receipt, "quote changed") != NULL,
        "A changed quote must wait for a fresh confirmation.");
    UX_KEY(KEY_ENTER);
    UX_CHECK(sim.player.coins == before - quote.total && sim.player.cargo[CC_GOOD_FOOD] == cargo + 1,
        "Confirmed purchase must match its quote.");
    UX_CHECK(CcSimSettlement(&sim, sim.player.location_id)->stock[CC_GOOD_FOOD] == warehouse_stock - 1,
        "A purchase must remove its load from this warehouse.");
    uint64_t purchased = CcSimHash(&sim);
    UX_KEY(KEY_ENTER);
    UX_CHECK(CcSimHash(&sim) == purchased, "Repeated confirmation must preserve the receipt and purse.");
    UX_KEY(KEY_TWO);
    UX_CHECK(local.trade_mode == 1 && CcSimHash(&sim) == purchased, "Switching to Sell must only switch mode.");
    quote = AdventureTradeQuote(&sim, &local);
    UX_CHECK(quote.available, "Owned cargo must have a sale quote.");
    before = sim.player.coins;
    UX_KEY(KEY_ENTER);
    UX_CHECK(sim.player.coins == before + quote.total && strstr(local.receipt, "Sold") != NULL,
        "Sale must show its actual proceeds.");
    UX_CHECK(CcSimSettlement(&sim, sim.player.location_id)->stock[CC_GOOD_FOOD] == warehouse_stock &&
        sim.player.cargo[CC_GOOD_FOOD] == cargo, "A sale must return the load to this warehouse.");
    UX_KEY(KEY_ESCAPE);
    local.interactions.count = 0;
    target = AdventureTarget(&local, &sim, CC_INTERACTION_EXIT, 1,
        "Street", "Step outside", local.agent.position, 1, 2, 1, LocalPosition(&local), 1);
    target->visible = true; local.interaction.focus = target->key;
    UX_KEY(KEY_F);
    UX_CHECK(!local.market_interior && view == VIEW_LOCAL, "Exit must return to the street.");
    uint64_t before_pause = CcSimHash(&sim);
    UX_KEY(KEY_ESCAPE);
    UX_CHECK(view == VIEW_PAUSE, "Escape in the scene must open the pause menu.");
    UX_KEY(KEY_N); UX_KEY(KEY_K); UX_KEY(KEY_PERIOD);
    UX_CHECK(CcSimHash(&sim) == before_pause, "Pause must preserve the campaign and its day.");
    UX_KEY(KEY_B);
    UX_CHECK(view == VIEW_LEDGER, "Book must open from pause.");
    UX_KEY(KEY_ESCAPE);
    UX_CHECK(view == VIEW_PAUSE, "Book back must return to pause.");
    UX_KEY(KEY_ESCAPE);
    UX_CHECK(view == VIEW_LOCAL, "Pause back must restore the scene.");
    RepositionHero(&local, (Vector2){28.9274921f, 0.340259194f}, false);
    local.agent.athletics = NonDefaultAthleticProfile();
    local.agent.combat.health = 37.0f;
    local.agent.appearance = CcNpcPlayerAppearance(4096);
    CcAthleticProfile recovery_athletics = local.agent.athletics;
    CcNpcAppearance recovery_appearance = local.agent.appearance;
    uint64_t before_recovery = CcSimHash(&sim);
    UX_KEY(KEY_N);
    UX_CHECK(SessionTestFloatMatches(local.agent.position.z, 0.340259194f) &&
        strstr(message, "again") != NULL, "First N tap must show the recovery prompt.");
    (void)HandleCaravanRecovery(&local, &view, &return_view, local.caravan_tap_deadline + 0.5, message, sizeof(message));
    UX_KEY(KEY_N);
    UX_CHECK(SessionTestFloatMatches(local.agent.position.z, 0.340259194f),
        "An expired tap must start a fresh recovery prompt.");
    local.interaction.approaching = true;
    local.agent.target_valid = true;
    local.course.raiders[0].combat.health = 19.0f;
    UX_KEY(KEY_N);
    UX_CHECK(GridDistance(LocalPosition(&local), LOCAL_CARRIAGE_BAY) < 0.001f &&
        !local.interaction.approaching && !local.agent.target_valid && local.request_save,
        "Double N must return to the caravan, clear the approach, and request a save.");
    UX_CHECK(local.agent.combat.health == 37.0f &&
        AthleticProfilesMatch(&local.agent.athletics, &recovery_athletics) &&
        memcmp(&local.agent.appearance, &recovery_appearance, sizeof(recovery_appearance)) == 0 &&
        local.course.raiders[0].combat.health == 19.0f && CcSimHash(&sim) == before_recovery,
        "Recovery must preserve health, skills, appearance, encounters, and campaign state.");
    local.request_save = false;
    RepositionHero(&local, (Vector2){4.0f, 3.0f}, true);
    local.market_interior = true;
    UX_KEY(KEY_N); UX_KEY(KEY_N);
    UX_CHECK(!local.market_interior && local.agent.scene == CC_LOCAL_SCENE_STREET &&
        GridDistance(LocalPosition(&local), LOCAL_CARRIAGE_BAY) < 0.001f,
        "Shop recovery must return to the town caravan.");
    local.request_save = false;
    local.site_kind = CC_LOCAL_SITE_DUNGEON;
    UX_KEY(KEY_N); UX_KEY(KEY_N);
    UX_CHECK(local.agent.scene == CC_LOCAL_SCENE_ROAD &&
        SessionTestFloatMatches(local.agent.position.x, CC_LOCAL_SITE_CARRIAGE_X + 3.0f),
        "Site recovery must use the site's caravan.");
    local.request_save = false;
    local.site_kind = CC_LOCAL_SITE_NONE;
    UX_CHECK(InitializeOpenWorld(&sim, &local, false), "Recovery needs an open-road stream.");
    const CcRoute *recovery_route = OpenWorldRouteFromSettlement(&sim, sim.player.location_id);
    UX_CHECK(recovery_route != NULL && EnterOpenWorldAtRoadGate(&sim, &local, recovery_route->id),
        "Recovery needs a road caravan.");
    view = VIEW_ROADS;
    Vector3 caravan_position = local.world_carriage.position;
    UX_KEY(KEY_N); UX_KEY(KEY_N);
    UX_CHECK(local.open_world && view == VIEW_LOCAL &&
        fabsf(GridDistance(LocalPosition(&local),
            (Vector2){caravan_position.x, caravan_position.z}) - 3.0f) < 0.01f &&
        CcSimHash(&sim) == before_recovery,
        "Road recovery must return beside the current caravan.");
    local.request_save = false;
    LeaveOpenWorld(&local);
    RepositionHero(&local, LOCAL_CARRIAGE_BAY, false);
    local.interactions.count = 0;
    target = AdventureTarget(&local, &sim, CC_INTERACTION_PERSON, 1,
        "Test neighbour", "Talk", local.agent.position, 1, 2, 1, LocalPosition(&local), 1.8f);
    target->visible = true; target->available = false;
    (void)snprintf(target->reason, sizeof(target->reason), "Choose a clear approach.");
    target->left = pointer.x - 10; target->right = pointer.x + 10;
    target->top = pointer.y - 10; target->bottom = pointer.y + 10;
    queued_mouse_button_press[MOUSE_BUTTON_LEFT] = true;
    HandleInput(&journal, &sim, &selected, &selected_situation, &view, &return_view, &local,
        (RenderTexture2D){0}, (Rectangle){0}, 1.0f / 60.0f, path, "", message, sizeof(message),
        feedback, sizeof(feedback), &feedback_age);
    ClientInputClearPressed();
    UX_CHECK(strstr(message, "Double-tap [N]") != NULL && strstr(message, target->reason) != NULL,
        "A denied scene click must show its reason and the caravan recovery tip.");
    UX_CHECK(CcJournalCheckpoint(journal, &sim, error, sizeof(error)), "Checkpoint must save the traded campaign.");
    uint64_t saved = CcSimHash(&sim);
    (void)CcJournalClose(&journal, &sim, error, sizeof(error));
    journal = CcJournalResume(path, &sim, error, sizeof(error));
    UX_CHECK(journal != NULL && CcSimHash(&sim) == saved, "Trade and pause must survive journal reload.");
    (void)CcJournalClose(&journal, &sim, error, sizeof(error)); (void)remove(path);
#undef UX_KEY
#undef UX_CHECK
    puts("adventure input flow passed");
    return 0;
}

static int RunAdventureTownRoutesRegression(void)
{
    static CcSim sim;
    static LocalState local;
    CcSimInit(&sim, 42U);
    for (int type = CC_SETTLEMENT_FARMING; type <= CC_SETTLEMENT_DUNGEON_TOWN; ++type) {
        CcSettlement *place = CcSimSettlementMutable(&sim, sim.player.location_id);
        place->function = (CcSettlementFunction)type;
        CcLocalBindPlace(&sim);
        ResetLocalState(&local);
        local.adventure_ui = true;
        local.course.automatic_alarm = false;
        BuildAdventureTargets(&sim, &local, (RenderTexture2D){0}, (Rectangle){0});
        CcInteractionTarget *door = NULL;
        for (int32_t i = 0; i < local.interactions.count; ++i) {
            if (local.interactions.targets[i].key.kind == CC_INTERACTION_DOOR) door = &local.interactions.targets[i];
        }
        if (door == NULL) return ClientRegressionFailure("Every town needs its service entrance.");
        door->visible = true;
        local.interaction.focus = door->key;
        ClientView view = VIEW_LOCAL, return_view = VIEW_LOCAL;
        int32_t selected_situation = 0;
        char message[256] = "";
        queued_key_press[KEY_F] = true;
        (void)HandleAdventureScene(&sim, &local, &view, &return_view, &selected_situation, NULL, 1.0f / 60.0f, message, sizeof(message));
        ClientInputClearPressed();
        for (int frame = 0; frame < 3000 && !local.market_interior && local.interaction.approaching; ++frame) {
            (void)HandleAdventureScene(&sim, &local, &view, &return_view, &selected_situation, NULL, 1.0f / 60.0f, message, sizeof(message));
        }
        if (!local.market_interior) {
            fprintf(stderr, "town type %d at %.2f %.2f: %s / %s\n", type, local.agent.position.x, local.agent.position.z, message, local.interaction.feedback);
            fprintf(stderr, "goal %.2f %.2f elapsed %.2f stalled %.2f active %d index %d of %d target %.2f %.2f\n", door->approach_x, door->approach_z, local.interaction.elapsed_seconds, local.interaction.stalled_seconds, local.agent.navigation_active, local.agent.navigation_point_index, local.agent.navigation_point_count, local.agent.target_point.x, local.agent.target_point.z);
            for (int n = 0; n < local.agent.navigation_point_count; ++n) fprintf(stderr, "node %d %.2f %.2f\n", n, local.agent.navigation_point[n].x, local.agent.navigation_point[n].z);
            return ClientRegressionFailure("Each town's service door must be reachable from the carriage yard.");
        }
        BuildAdventureTargets(&sim, &local, (RenderTexture2D){0}, (Rectangle){0});
        CcInteractionTarget *counter = &local.interactions.targets[0];
        counter->visible = true;
        local.interaction.focus = counter->key;
        queued_key_press[KEY_F] = true;
        (void)HandleAdventureScene(&sim, &local, &view, &return_view, &selected_situation, NULL, 1.0f / 60.0f, message, sizeof(message));
        ClientInputClearPressed();
        for (int frame = 0; frame < 1200 && view == VIEW_LOCAL && local.interaction.approaching; ++frame)
            (void)HandleAdventureScene(&sim, &local, &view, &return_view, &selected_situation, NULL, 1.0f / 60.0f, message, sizeof(message));
        if (view != VIEW_TRADE) return ClientRegressionFailure("Each keeper must be reachable from the shop entrance.");
    }
    puts("six town door and counter routes passed");
    return 0;
}

/* Counter fixtures cover money limits and a load whose journey has completed.
   The journey and journal suites cover how that arrival state is produced. */
static int RunAdventureTradeTermsRegression(void)
{
    static CcSim sim;
    static LocalState local;
    char error[192] = "", message[256] = "";
    CcSimInit(&sim, 42U);
    memset(&local, 0, sizeof(local));
    local.trade_good = CC_GOOD_FOOD;
    local.trade_quantity = 1;
#define TERMS_CHECK(condition, text) do { if (!(condition)) return ClientRegressionFailure(text); } while (0)
    CcSettlement *place = CcSimSettlementMutable(&sim, sim.player.location_id);
    CcMoney coins = sim.player.coins;
    sim.player.coins = 0;
    AdventureQuote quote = AdventureTradeQuote(&sim, &local);
    TERMS_CHECK(!quote.available && strstr(quote.reason, "purse") != NULL, "The quote must explain its purse limit.");
    sim.player.coins = coins;
    int32_t stock = place->stock[CC_GOOD_FOOD];
    place->stock[CC_GOOD_FOOD] = 0;
    TERMS_CHECK(!AdventureTradeQuote(&sim, &local).available, "Empty stock must disable Buy.");
    place->stock[CC_GOOD_FOOD] = stock;
    int32_t capacity = sim.player.cargo_capacity;
    sim.player.cargo_capacity = 0;
    TERMS_CHECK(!AdventureTradeQuote(&sim, &local).available, "Full cargo space must disable Buy.");
    sim.player.cargo_capacity = capacity;
    CcSituation *promise = NULL;
    for (int32_t i = 0; i < sim.situation_count; ++i)
        if (sim.situations[i].kind == CC_SITUATION_RELIEF_DELIVERY && sim.situations[i].status == CC_SITUATION_ACTIVE) {
            promise = &sim.situations[i]; break;
        }
    TERMS_CHECK(promise != NULL, "The fixture needs a delivery offer.");
    CcCommand accept = {.kind = CC_COMMAND_ACCEPT_SITUATION, .target_id = promise->id};
    TERMS_CHECK(CcSimApply(&sim, &accept, error, sizeof(error)), error);
    sim.player.location_id = promise->target_id;
    sim.carriage.location_id = promise->target_id;
    place = CcSimSettlementMutable(&sim, promise->target_id);
    local.trade_mode = 2;
    quote = AdventureTradeQuote(&sim, &local);
    TERMS_CHECK(!quote.available && strstr(quote.reason, "another town") != NULL,
        "A delivery quote must require the completed journey.");
    sim.resolved_journey_situation_id = promise->id;
    int32_t load = sim.player.cargo[promise->good];
    sim.player.cargo[promise->good] = promise->quantity - promise->progress - 1;
    TERMS_CHECK(!AdventureTradeQuote(&sim, &local).available, "Delivery needs the full remaining load.");
    sim.player.cargo[promise->good] = load;
    local.trade_mode = 1;
    local.trade_good = promise->good;
    TERMS_CHECK(!AdventureTradeQuote(&sim, &local).available, "The Sell tab must direct promised cargo to Deliver.");
    local.trade_mode = 2;
    CcMoney reserve = place->market_coins;
    place->market_coins = 0;
    TERMS_CHECK(!AdventureTradeQuote(&sim, &local).available, "The keeper must cover the sale value.");
    place->market_coins = reserve;
    quote = AdventureTradeQuote(&sim, &local);
    TERMS_CHECK(quote.available && quote.command.amount == -(promise->quantity - promise->progress),
        "Deliver must quote the full remaining load.");
    local.trade_quote_presented = true;
    local.trade_presented_command = quote.command;
    local.trade_presented_total = quote.total;
    local.trade_presented_reward = quote.reward;
    local.trade_presented_promise_id = quote.promise_id;
    ClientView view = VIEW_TRADE;
    coins = sim.player.coins;
    CcId id = promise->id;
    queued_key_press[KEY_ENTER] = true;
    (void)HandleAdventureTrade(NULL, &sim, &local, &view, message, sizeof(message));
    ClientInputClearPressed();
    TERMS_CHECK(local.trade_confirmed && sim.player.coins == coins + quote.total + quote.reward,
        "Delivery proceeds must match the displayed value and reward.");
    TERMS_CHECK(CcSimAcceptedSituation(&sim) == NULL && CcSimQuestOutcome(&sim, id) != NULL &&
        strstr(local.receipt, "Delivery complete") != NULL, "Delivery must show a receipt and record the outcome.");
    uint64_t delivered = CcSimHash(&sim);
    queued_key_press[KEY_ENTER] = true;
    (void)HandleAdventureTrade(NULL, &sim, &local, &view, message, sizeof(message));
    ClientInputClearPressed();
    TERMS_CHECK(CcSimHash(&sim) == delivered, "Repeated delivery confirmation must preserve its result.");
    CcSimInit(&sim, 42U);
    sim.player.location_id = sim.settlements[1].id;
    sim.carriage.location_id = sim.player.location_id;
    memset(sim.player.cargo, 0, sizeof(sim.player.cargo));
    sim.player.cargo[CC_GOOD_WHEAT] = 12;
    memset(&local, 0, sizeof(local));
    local.trade_mode = 3;
    CcBakerySupportPlan bakery = CcSimBakerySupportPlan(&sim, sim.player.location_id);
    TERMS_CHECK(bakery.ready, "The bakery fixture must have a ready offer.");
    coins = sim.player.coins;
    queued_key_press[KEY_ENTER] = true;
    (void)HandleAdventureTrade(NULL, &sim, &local, &view, message, sizeof(message));
    ClientInputClearPressed();
    TERMS_CHECK(sim.player.coins == coins, "Bakery support must require a displayed offer.");
    local.trade_quote_presented = true;
    local.trade_presented_total = bakery.coins;
    local.trade_presented_command = (CcCommand){.kind = CC_COMMAND_SUPPORT_BAKERY,
        .target_id = sim.player.location_id, .amount = bakery.building_days};
    queued_key_press[KEY_ENTER] = true;
    (void)HandleAdventureTrade(NULL, &sim, &local, &view, message, sizeof(message));
    ClientInputClearPressed();
    TERMS_CHECK(sim.player.coins == coins - bakery.coins && sim.player.cargo[CC_GOOD_WHEAT] == 0,
        "Bakery confirmation must transfer the displayed cargo and wages.");
    TERMS_CHECK(CcSimBakerySupportPlan(&sim, sim.player.location_id).remembered,
        "The receiving contact must remember the confirmed support.");
    local.trade_mode = 4;
    sim.player.coins = 400;
    local.trade_presented_command = (CcCommand){.kind = CC_COMMAND_FUND_GRAIN_SUPPLY,
        .target_id = sim.player.location_id};
    local.trade_presented_total = 200;
    queued_key_press[KEY_ENTER] = true;
    (void)HandleAdventureTrade(NULL, &sim, &local, &view, message, sizeof(message));
    ClientInputClearPressed();
    TERMS_CHECK(sim.grain_supplies[1].enabled && sim.grain_supplies[1].purse == 192,
        "The grain fund button must start the displayed agreement.");
    TERMS_CHECK(strstr(local.receipt, "grain fund") != NULL,
        "Funding must show its receipt.");
    uint64_t meeting_hash = CcSimHash(&sim);
    queued_key_press[KEY_SIX] = true;
    (void)HandleAdventureTrade(NULL, &sim, &local, &view, message, sizeof(message));
    ClientInputClearPressed();
    TERMS_CHECK(local.trade_mode == 5, "The sixth trade tab must open the road meeting.");
    queued_key_press[KEY_ENTER] = true;
    (void)HandleAdventureTrade(NULL, &sim, &local, &view, message, sizeof(message));
    ClientInputClearPressed();
    TERMS_CHECK(CcSimHash(&sim) == meeting_hash, "Reading the meeting must preserve funds and commitments.");

#undef TERMS_CHECK
    puts("adventure trade terms passed");
    return 0;
}
