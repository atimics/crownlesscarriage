static bool ClientLoadReliefFixture(CcSim *sim, const CcSituation *situation,
                                    char *error, size_t error_capacity)
{
    while (CcSimReliefCratesToLoad(situation) > 0) {
        CcCommand pickup = {
            .kind = CC_COMMAND_PICKUP_RELIEF_CRATE,
            .target_id = situation->id
        };
        if (!CcSimApply(sim, &pickup, error, error_capacity)) return false;
        CcCommand stow = {
            .kind = CC_COMMAND_STOW_RELIEF_CRATE,
            .target_id = situation->id
        };
        if (!CcSimApply(sim, &stow, error, error_capacity)) return false;
    }
    return true;
}

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
            for (int step = 0; step < 20000 &&
                 sim.journey.phase == CC_JOURNEY_PHASE_TRAVELLING; ++step) {
                ROAD_SAVE_CHECK(CcJournalAdvanceRuntimeTicks(
                    journal, &sim, 1, error, sizeof(error)),
                    "Advance to a saved road stop.");
            }
            ROAD_SAVE_CHECK(sim.journey.active &&
                (sim.journey.phase == CC_JOURNEY_PHASE_RESTING ||
                 sim.journey.phase == CC_JOURNEY_PHASE_ROAD_CHOICE),
                "Reach a real saved road stop.");
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
    if (!ClientLoadReliefFixture(&sim, promise, error, sizeof(error)))
        return ClientRegressionFailure("Load the delivery fixture crate by crate.");
    if (AdventureDeliveryAtHand(&sim) != NULL)
        return ClientRegressionFailure("Departure still needs the road journey.");
    sim.player.location_id = promise->target_id;
    sim.carriage.location_id = promise->target_id;
    sim.resolved_journey_situation_id = promise->id;
    Vector2 position = LocalPosition(&local);
    const CcLocalPlaceProfile *delivery_profile = CcLocalPlaceProfileForSettlement(
        CcSimSettlement(&sim, promise->target_id));
    const CcLocalShop *delivery_shop = CcLocalShopAt(delivery_profile,
        CC_LOCAL_SHOP_GRAIN_MERCHANT);
    if (delivery_shop == NULL) return ClientRegressionFailure("The destination needs a grain merchant.");
    CcInteractionTarget *door = AdventureTarget(&local, &sim, CC_INTERACTION_DOOR,
        (uint64_t)delivery_shop->building_index + 1U,
        "Grain merchant", "Deliver promise", (Vector3){0}, 1, 2, 1,
        (Vector2){position.x + 30, position.y}, 1);
    CcInteractionTarget *person = AdventureTarget(&local, &sim, CC_INTERACTION_PERSON, 2,
        "Neighbour", "Talk", (Vector3){0}, 1, 2, 1, position, 1);
    if (door == NULL || person == NULL) return ClientRegressionFailure("Create the handoff targets.");
    ContextActionSet actions = BuildContextActions(&sim, &local, VIEW_LOCAL, 0, 0);
    if (actions.count != 3 ||
        actions.items[0].target.kind != CC_INTERACTION_DOOR ||
        actions.items[1].target.kind != CC_INTERACTION_PERSON ||
        actions.items[2].kind != CONTEXT_ACTION_CARE_HORSES)
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
    RepositionHero(&local, (Vector2){origin.x + 20.0f, origin.y + 20.0f}, false);
    origin = LocalPosition(&local);
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

static int RunSituationWitnessCardPriorityRegression(void)
{
    static CcSim sim;
    static LocalState local;
    CcSimInit(&sim, 42U);
    ResetLocalState(&local);
    local.adventure_ui = true;
    int32_t opening = OpeningSituationIndex(&sim);
    const CcSituation *situation = opening >= 0 ? &sim.situations[opening] : NULL;
    const CcCharacter *mara = situation != NULL ?
        CcSimSituationConversationCharacter(&sim, situation, sim.player.location_id) : NULL;
    if (mara == NULL || strcmp(mara->name, "Mara Venn") != 0)
        return ClientRegressionFailure("The opening fixture needs Mara Venn.");
    Vector2 origin = LocalPosition(&local);
    RepositionHero(&local, (Vector2){origin.x + 20.0f, origin.y + 20.0f}, false);
    origin = LocalPosition(&local);
    int32_t generic = 0;
    for (int32_t i = 0; i < sim.character_count && generic < 4; ++i) {
        if (sim.characters[i].id == mara->id) continue;
        CcInteractionTarget *target = AdventureTarget(&local, &sim,
            CC_INTERACTION_PERSON, UINT64_C(0x100000000) + (uint64_t)generic,
            "Scout", "Talk", (Vector3){0}, 1, 2, 1,
            (Vector2){origin.x + (float)generic + 1.0f, origin.y}, 1);
        if (target == NULL) return ClientRegressionFailure("Create retained generic cards.");
        target->character_id = sim.characters[i].id;
        local.presented_person_binding_place[generic] = sim.player.location_id;
        local.presented_person_binding_object[generic] = target->key.object;
        local.presented_person_binding_character[generic] = target->character_id;
        ++generic;
    }
    if (generic != 4) return ClientRegressionFailure("Seed four generic residents.");
    ContextActionSet shown = BuildContextActions(&sim, &local, VIEW_LOCAL, 0, opening);
    RememberPresentedTargets(&local, VIEW_LOCAL, &shown);
    if (local.presented_target_count != 4)
        return ClientRegressionFailure("Publish the retained generic cards.");
    local.course.situation_witness_active = true;
    local.course.situation_witness_id = situation->id;
    local.course.situation_witness_character_id = mara->id;
    CcInteractionTarget *witness = AdventureTarget(&local, &sim,
        CC_INTERACTION_PERSON, mara->id, mara->name, "Talk",
        (Vector3){0}, 1, 2, 1, (Vector2){origin.x + 30.0f, origin.y}, 1);
    if (witness == NULL) return ClientRegressionFailure("Create Mara's named witness target.");
    witness->character_id = mara->id;
    witness->situation_id = situation->id;
    ContextActionSet priority = BuildContextActions(&sim, &local, VIEW_LOCAL, 0, opening);
    bool mara_shown = false;
    int32_t retained = 0;
    for (int32_t i = 0; i < priority.count; ++i) {
        const CcInteractionTarget *target = CcInteractionFind(&local.interactions,
            priority.items[i].target);
        if (target != NULL && target->character_id == mara->id) mara_shown = true;
        if (AdventureGenericPersonKey(priority.items[i].target)) ++retained;
    }
    if (priority.count != 5 || !mara_shown || retained != 4 ||
        priority.items[0].target.object != mara->id)
        return ClientRegressionFailure("Mara's named witness card leads retained generic cards.");
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
    local.conversation_report_response = true;
    AdventureActivate(&sim, &local, &target, &view, &back, &selected);
    if (local.conversation_report_response)
        return ClientRegressionFailure("A new conversation must clear Jory's report reply.");
    local.adventure_ui = false;
    if (LocalConversationFocus(&local, VIEW_CHARACTER) != NULL)
        return ClientRegressionFailure("The classic conversation keeps its witness camera.");
    ResetLocalState(&local);
    if (local.conversation_object != 0 || local.conversation_position.x != 0 ||
        local.conversation_position.y != 0 || local.conversation_position.z != 0)
        return ClientRegressionFailure("A reset clears the previous speaker position.");
    return 0;
}

static int RunAdventureMineRoadRegression(void)
{
    static CcSim sim;
    static LocalState local;
    const char *path = "adventure-mine-road.ccsave";
    char error[256] = "";
    char message[256] = "";
    (void)remove(path);
    CcSimInit(&sim, UINT32_C(0x3235a7ed));
    const CcRoadSite *mine = CcMineSite(&sim);
    if (mine == NULL)
        return ClientRegressionFailure("Low Silver Pit must have an authored road site.");
    const CcRoute *route = CcSimRoute(&sim, mine->route_id);
    if (route == NULL)
        return ClientRegressionFailure("Low Silver Pit must have an authored route.");
    CcId silverwick = sim.dungeons[0].settlement_id;
    if (route->from_id != silverwick && route->to_id != silverwick)
        return ClientRegressionFailure("Silverwick must be an endpoint of the mine road.");
    sim.player.location_id = silverwick;
    sim.carriage.location_id = silverwick;
    (void)snprintf(sim.dungeons[0].name,
                   sizeof(sim.dungeons[0].name),
                   "%s", "The Saintless Vault");
    CcLocalBindPlace(&sim);
    ResetLocalState(&local);
    local.adventure_ui = true;
    local.course.automatic_alarm = false;
    BuildAdventureTargets(
        &sim, &local, (RenderTexture2D){0}, (Rectangle){0});
    CcInteractionTarget *mine_card = NULL;
    for (int32_t i = 0; i < local.interactions.count; ++i) {
        CcInteractionTarget *candidate = &local.interactions.targets[i];
        if (candidate->key.kind == CC_INTERACTION_SITE &&
            candidate->key.object == CC_LOCAL_SITE_DUNGEON) {
            mine_card = candidate;
            break;
        }
    }
    if (mine_card == NULL ||
        strcmp(mine_card->name, "Low Silver Pit road") != 0)
        return ClientRegressionFailure("The shipped mine card must name the Low Silver Pit road.");
    CcSim other = sim;
    other.dungeons[0].settlement_id = other.settlements[4].id;
    other.player.location_id = other.settlements[4].id;
    other.carriage.location_id = other.player.location_id;
    CcLocalBindPlace(&other);
    LocalState other_local;
    ResetLocalState(&other_local);
    other_local.adventure_ui = true;
    BuildAdventureTargets(
        &other, &other_local, (RenderTexture2D){0}, (Rectangle){0});
    const CcInteractionTarget *other_card = NULL;
    for (int32_t i = 0; i < other_local.interactions.count; ++i) {
        const CcInteractionTarget *candidate = &other_local.interactions.targets[i];
        if (candidate->key.kind == CC_INTERACTION_SITE &&
            candidate->key.object == CC_LOCAL_SITE_DUNGEON) {
            other_card = candidate;
            break;
        }
    }
    if (other_card == NULL ||
        strcmp(other_card->name, "The Saintless Vault") != 0)
        return ClientRegressionFailure("A dungeon away from the mine road must keep its authored name.");
    CcLocalBindPlace(&sim);
    RepositionHero(
        &local, (Vector2){mine_card->approach_x,
                          mine_card->approach_z}, false);
    CcJournal *journal = CcJournalStart(
        path, &sim, error, sizeof(error));
    if (journal == NULL) return ClientRegressionFailure(error);
    ClientView view = VIEW_LOCAL;
    ClientView return_view = VIEW_LOCAL;
    int32_t selected_situation = 0;
    for (int32_t frame = 0; frame < 8 && !sim.journey.active; ++frame) {
        (void)HandleAdventureScene(
            journal, &sim, &local, &view, &return_view,
            &selected_situation, frame == 0 ? mine_card : NULL,
            1.0f / 60.0f, message, sizeof(message));
    }
    bool passed = sim.journey.active &&
        sim.journey.route_id == mine->route_id &&
        local.site_kind == CC_LOCAL_SITE_NONE &&
        !sim.dungeon_expedition.active &&
        local.journey_travel_active;
    CcJournalAbandon(&journal);
    (void)remove(path);
    if (!passed)
        return ClientRegressionFailure("The adventure mine card must begin the real Silverwick-Alderwatch road journey.");
    return 0;
}

static bool DispatchDisplayedRoadChoice(
    CcJournal **journal, CcSim *sim, LocalState *local,
    ClientView *view, ClientView *return_view, int32_t *selected,
    int32_t *selected_situation, CcId destination_anchor,
    bool keyboard, const char *path, char *message, size_t message_capacity)
{
    CcRoadLegPreview previews[3];
    int32_t preview_count = CcRoadNextLegPreviews(sim, previews, 3);
    const CcRoadLegPreview *preview = NULL;
    for (int32_t i = 0; i < preview_count; ++i) {
        if (previews[i].destination_anchor_id == destination_anchor)
            preview = &previews[i];
    }
    if (preview == NULL) return false;
    /* Mid-leg reversal lives behind Stop -> Road options. Exercise the
       player's inputs, not a direct flag write, and preserve the exact road
       coordinate, clock and resources before selecting the displayed token. */
    if (!sim->journey.road_waiting_choice) {
        uint64_t before_options = CcSimHash(sim);
        char feedback[128] = "";
        float feedback_age = 0.0f;
        if (!local->carriage_stopped) {
            ContextActionSet moving = BuildContextActions(
                sim, local, *view, *selected, *selected_situation);
            for (int32_t i = 0; i < moving.count; ++i)
                if (moving.items[i].kind == CONTEXT_ACTION_CHOOSE_ROAD_LEG)
                    return false;
            queued_key_press[KEY_SPACE] = true;
            HandleInput(journal, sim, selected, selected_situation, view,
                return_view, local, (RenderTexture2D){0}, (Rectangle){0},
                1.0f / 60.0f, path, "", message, message_capacity,
                feedback, sizeof(feedback), &feedback_age);
            ClientInputClearPressed();
        }
        if (!local->carriage_stopped || CcSimHash(sim) != before_options)
            return false;
        ContextActionSet stopped = BuildContextActions(
            sim, local, *view, *selected, *selected_situation);
        int32_t options_index = -1;
        for (int32_t i = 0; i < stopped.count; ++i) {
            if (stopped.items[i].kind == CONTEXT_ACTION_ROAD_OPTIONS &&
                strcmp(stopped.items[i].label, "Road options") == 0)
                options_index = i;
        }
        if (options_index < 0) return false;
        local->card_page = options_index / ContextActionPageSize(&stopped);
        int32_t first = ContextCardFirst(local, &stopped);
        Rectangle bounds = ContextActionBounds(options_index - first,
            ContextCardCount(&stopped, first), stopped.combat);
        client_test_pointer_active = true;
        client_test_pointer = (Vector2){
            bounds.x + bounds.width * 0.5f,
            bounds.y + bounds.height * 0.5f
        };
        queued_mouse_button_press[MOUSE_BUTTON_LEFT] = true;
        HandleInput(journal, sim, selected, selected_situation, view,
            return_view, local, (RenderTexture2D){0}, (Rectangle){0},
            1.0f / 60.0f, path, "", message, message_capacity,
            feedback, sizeof(feedback), &feedback_age);
        client_test_pointer_active = false;
        ClientInputClearPressed();
        if (!local->carriage_stopped || !local->road_actions_expanded ||
            CcSimHash(sim) != before_options) return false;
    }
    ContextActionSet actions = BuildContextActions(
        sim, local, *view, *selected, *selected_situation);
    int32_t action_index = -1;
    for (int32_t i = 0; i < actions.count; ++i) {
        if (actions.items[i].kind == CONTEXT_ACTION_CHOOSE_ROAD_LEG &&
            actions.items[i].target.place == sim->journey.road_anchor_id &&
            actions.items[i].target.object == preview->decision_token) {
            action_index = i;
            break;
        }
    }
    if (action_index < 0) return false;
    char expected_hint[8];
    (void)snprintf(expected_hint, sizeof(expected_hint), "%d", action_index + 1);
    if (strcmp(actions.items[action_index].key_hint, expected_hint) != 0)
        return false;
    int32_t expected_coordinate = sim->journey.road_coordinate_units;
    CcPilotRoadTopology pilot;
    if (!CcPilotRoadTopologyBuildWithLength(
            sim, sim->journey.road_geometry_length_units, &pilot)) return false;
    if (sim->journey.road_waiting_choice &&
        preview->segment_id == pilot.mill_segment_id) {
        expected_coordinate = preview->direction == CC_ROAD_DIRECTION_FORWARD ?
            0 : pilot.mill_spur_length_units;
    }
    if (keyboard) {
        queued_key_press[KEY_ONE + action_index] = true;
    } else {
        local->card_page = action_index / ContextActionPageSize(&actions);
        int32_t first = ContextCardFirst(local, &actions);
        int32_t shown = ContextCardCount(&actions, first);
        Rectangle bounds = ContextActionBounds(
            action_index - first, shown, actions.combat);
        client_test_pointer_active = true;
        client_test_pointer = (Vector2){
            bounds.x + bounds.width * 0.5f,
            bounds.y + bounds.height * 0.5f
        };
        queued_mouse_button_press[MOUSE_BUTTON_LEFT] = true;
    }
    char feedback[128] = "";
    float feedback_age = 0.0f;
    HandleInput(journal, sim, selected, selected_situation, view,
        return_view, local, (RenderTexture2D){0}, (Rectangle){0},
        1.0f / 60.0f, path, "", message, message_capacity,
        feedback, sizeof(feedback), &feedback_age);
    client_test_pointer_active = false;
    ClientInputClearPressed();
    return sim->journey.road_revision == preview->journey_revision + 1U &&
        sim->journey.road_segment_id == preview->segment_id &&
        sim->journey.road_direction == (int32_t)preview->direction &&
        sim->journey.road_stop_anchor_id == destination_anchor &&
        sim->journey.road_coordinate_units == expected_coordinate &&
        sim->journey.road_leg_start_coordinate_units == expected_coordinate;
}

static int RunDisplayedRoadChoiceRegression(void)
{
    static CcSim sim;
    static LocalState local;
    const char *path = "displayed-road-choice.ccsave";
    char error[256] = "", message[256] = "";
    (void)remove(path);
    CcSimInit(&sim, UINT32_C(0x3235a7ed));
    CcPilotRoadTopology pilot;
    if (!CcPilotRoadTopologyBuild(&sim, &pilot))
        return ClientRegressionFailure("Build the displayed pilot road fixture.");
    sim.player.location_id = pilot.origin_id;
    sim.carriage.location_id = pilot.origin_id;
    CcCommand travel = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = pilot.destination_id
    };
    if (!CcSimApply(&sim, &travel, error, sizeof(error)) ||
        !sim.journey.road_position_active)
        return ClientRegressionFailure("Start the displayed pilot road journey.");
    sim.journey.encounter_triggered = true;
    for (int32_t step = 0;
         step < 16 && sim.journey.road_anchor_id != pilot.junction_id;
         ++step) {
        (void)CcRoadAdvanceLeg(&sim, INT32_MAX);
        if (sim.journey.road_anchor_id == pilot.junction_id) break;
        CcRoadLegPreview previews[3];
        int32_t count = CcRoadNextLegPreviews(&sim, previews, 3);
        const CcRoadLegPreview *forward = NULL;
        for (int32_t i = 0; i < count; ++i) {
            if (previews[i].direction == CC_ROAD_DIRECTION_FORWARD &&
                previews[i].segment_id != pilot.mill_segment_id)
                forward = &previews[i];
        }
        if (forward == NULL || !CcRoadChooseNextLeg(
                &sim, forward->decision_token, error, sizeof(error)))
            return ClientRegressionFailure("Reach the displayed Stag's Mill junction.");
    }
    if (sim.journey.road_anchor_id != pilot.junction_id ||
        !sim.journey.road_waiting_choice)
        return ClientRegressionFailure("Stop at the displayed Stag's Mill junction.");
    ResetLocalState(&local);
    local.adventure_ui = true;
    local.journey_travel_active = true;
    local.carriage_stopped = true;
    ClientView view = VIEW_LOCAL;
    ClientView return_view = VIEW_LOCAL;
    int32_t selected = 0;
    int32_t selected_situation = 0;
    CcJournal *journal = CcJournalStart(path, &sim, error, sizeof(error));
#define DISPLAYED_ROAD_CHECK(condition, reason) do { if (!(condition)) { \
    CcJournalAbandon(&journal); (void)remove(path); \
    return ClientRegressionFailure(reason); } } while (0)
    DISPLAYED_ROAD_CHECK(journal != NULL, "Start the displayed road choice journal.");
    DISPLAYED_ROAD_CHECK(DispatchDisplayedRoadChoice(
        &journal, &sim, &local, &view, &return_view, &selected,
        &selected_situation, pilot.mill_site_id, false, path,
        message, sizeof(message)),
        "A pointer choice must dispatch the exact Stag's Mill leg.");
    DISPLAYED_ROAD_CHECK(sim.journey.road_coordinate_units == 0,
        "The mill spur must begin at its saved junction coordinate.");
    int32_t half_time = sim.journey.road_leg_total_subticks / 2;
    DISPLAYED_ROAD_CHECK(!CcRoadAdvanceLeg(&sim, half_time),
        "The half-spur fixture must remain between its anchors.");
    int32_t half_coordinate = sim.journey.road_coordinate_units;
    DISPLAYED_ROAD_CHECK(half_coordinate > 0 &&
        half_coordinate < pilot.mill_spur_length_units &&
        CcJournalCheckpoint(journal, &sim, error, sizeof(error)),
        "Save the half-spur coordinate before reversing.");
    DISPLAYED_ROAD_CHECK(DispatchDisplayedRoadChoice(
        &journal, &sim, &local, &view, &return_view, &selected,
        &selected_situation, pilot.junction_id, true, path,
        message, sizeof(message)),
        "Keyboard choice one must reverse from the exact half-spur coordinate.");
    DISPLAYED_ROAD_CHECK(sim.journey.road_coordinate_units == half_coordinate &&
        sim.journey.road_leg_length_units == half_coordinate,
        "Half-spur reversal must keep its saved coordinate and return cost.");
    DISPLAYED_ROAD_CHECK(CcRoadAdvanceLeg(&sim, INT32_MAX) &&
        sim.journey.road_anchor_id == pilot.junction_id &&
        sim.journey.road_coordinate_units == pilot.origin_to_junction_units &&
        CcJournalCheckpoint(journal, &sim, error, sizeof(error)),
        "The reversed half-spur must return to its main-road anchor.");
    DISPLAYED_ROAD_CHECK(DispatchDisplayedRoadChoice(
        &journal, &sim, &local, &view, &return_view, &selected,
        &selected_situation, pilot.mill_site_id, true, path,
        message, sizeof(message)),
        "A keyboard choice must dispatch the full Stag's Mill leg.");
    DISPLAYED_ROAD_CHECK(CcRoadAdvanceLeg(&sim, INT32_MAX) &&
        sim.journey.road_anchor_id == pilot.mill_site_id &&
        CcJournalCheckpoint(journal, &sim, error, sizeof(error)),
        "The keyboard mill leg must arrive at Stag's Mill.");
    DISPLAYED_ROAD_CHECK(DispatchDisplayedRoadChoice(
        &journal, &sim, &local, &view, &return_view, &selected,
        &selected_situation, pilot.junction_id, false, path,
        message, sizeof(message)),
        "A pointer choice must dispatch the mill return leg.");
    DISPLAYED_ROAD_CHECK(CcRoadAdvanceLeg(&sim, INT32_MAX) &&
        sim.journey.road_anchor_id == pilot.junction_id &&
        sim.journey.road_coordinate_units == pilot.origin_to_junction_units &&
        CcJournalCheckpoint(journal, &sim, error, sizeof(error)),
        "The mill return must keep the authored main-road anchor.");
    DISPLAYED_ROAD_CHECK(DispatchDisplayedRoadChoice(
        &journal, &sim, &local, &view, &return_view, &selected,
        &selected_situation, pilot.destination_id, true, path,
        message, sizeof(message)),
        "A keyboard choice must dispatch the goal leg.");
    DISPLAYED_ROAD_CHECK(CcRoadAdvanceLeg(&sim, INT32_MAX) &&
        sim.journey.road_anchor_id == pilot.destination_id &&
        sim.journey.road_coordinate_units == pilot.main_length_units,
        "The displayed goal leg must reach its saved physical coordinate.");
    DISPLAYED_ROAD_CHECK(CcJournalClose(
        &journal, &sim, error, sizeof(error)), error);
    (void)remove(path);
#undef DISPLAYED_ROAD_CHECK
    return 0;
}

static int RunMovingPresentedPersonRegression(void)
{
    static CcSim sim;
    static LocalState local;
    char message[256] = "";
    CcSimInit(&sim, UINT32_C(0x3235a7ed));
    CcId silverwick = sim.dungeons[0].settlement_id;
    sim.player.location_id = silverwick;
    sim.carriage.location_id = silverwick;
    CcCharacter *scout = NULL;
    for (int32_t i = 0; i < sim.character_count; ++i) {
        CcCharacter *candidate = &sim.characters[i];
        if (CcCharacterAgeYears(&sim, candidate) < 16) continue;
        scout = candidate;
        break;
    }
    if (scout == NULL)
        return ClientRegressionFailure("The Silverwick fixture needs an adult scout.");
    scout->current_settlement_id = silverwick;
    scout->activity = CC_CHARACTER_ACTIVITY_WORKING;
    scout->role = CC_CHARACTER_SCOUT;
    CcCharacter *second_scout = NULL;
    CcCharacter *third_scout = NULL;
    for (int32_t i = 0; i < sim.character_count; ++i) {
        CcCharacter *candidate = &sim.characters[i];
        if (candidate == scout || CcCharacterAgeYears(&sim, candidate) < 16)
            continue;
        candidate->current_settlement_id = silverwick;
        candidate->activity = CC_CHARACTER_ACTIVITY_WORKING;
        candidate->role = CC_CHARACTER_SCOUT;
        if (second_scout == NULL) second_scout = candidate;
        else { third_scout = candidate; break; }
    }
    if (second_scout == NULL || third_scout == NULL)
        return ClientRegressionFailure("The Scout binding fixture needs three adults.");
    {
        CcInteractionState approach = {0};
        CcInteractionPlan plan = {.count = 1};
        plan.targets[0] = (CcInteractionTarget){
            .key = {silverwick, UINT64_C(0x10000b1ce), CC_INTERACTION_PERSON},
            .character_id = scout->id, .approach_x = 4.0f, .approach_z = 0.0f,
            .radius = 1.0f, .available = true};
        if (!CcInteractionStart(&approach, &plan.targets[0], 0.0f, 0.0f) ||
            CcInteractionAdvance(&approach, &plan, 0.0f, 0.0f, 1.0f / 60.0f) ||
            !approach.approaching)
            return ClientRegressionFailure("A moving Scout keeps its pending approach.");
        plan.targets[0].character_id = second_scout->id;
        plan.targets[0].approach_x = 0.0f;
        plan.targets[0].radius = 2.0f;
        if (CcInteractionAdvance(&approach, &plan, 0.0f, 0.0f, 1.0f / 60.0f) ||
            approach.approaching || strstr(approach.feedback, "person changed") == NULL)
            return ClientRegressionFailure("A rebuilt Scout seed with another resident cancels its approach.");
    }
    CcLocalBindPlace(&sim);
    ResetLocalState(&local);
    local.adventure_ui = true;
    {
        LocalState bindings = {0};
        ResetLocalState(&bindings);
        bindings.adventure_ui = true;
        CcInteractionTarget *first = AdventureTarget(
            &bindings, &sim, CC_INTERACTION_PERSON, UINT64_C(0x10000b100),
            "Scout", "Talk", (Vector3){17.2f, 0, 7.2f},
            0.9f, 2.0f, 0.7f, (Vector2){17.2f, 7.2f}, 1.8f);
        CcInteractionTarget *second = AdventureTarget(
            &bindings, &sim, CC_INTERACTION_PERSON, UINT64_C(0x10000b101),
            "Scout", "Talk", (Vector3){18.2f, 0, 7.2f},
            0.9f, 2.0f, 0.7f, (Vector2){18.2f, 7.2f}, 1.8f);
        if (first == NULL || second == NULL ||
            !AdventureNamePresented(&sim, &bindings, first, CC_NPC_ROLE_SCOUT,
                                    NULL, 0, 0) ||
            !AdventureNamePresented(&sim, &bindings, second, CC_NPC_ROLE_SCOUT,
                                    NULL, 0, 1))
            return ClientRegressionFailure("Two visible Scouts need distinct resident bindings.");
        CcId first_id = first->character_id;
        CcId second_id = second->character_id;
        bindings.interactions.count = 0;
        second = AdventureTarget(&bindings, &sim, CC_INTERACTION_PERSON,
            UINT64_C(0x10000b101), "Scout", "Talk", (Vector3){18.5f, 0, 7.2f},
            0.9f, 2.0f, 0.7f, (Vector2){18.5f, 7.2f}, 1.8f);
        first = AdventureTarget(&bindings, &sim, CC_INTERACTION_PERSON,
            UINT64_C(0x10000b100), "Scout", "Talk", (Vector3){17.5f, 0, 7.2f},
            0.9f, 2.0f, 0.7f, (Vector2){17.5f, 7.2f}, 1.8f);
        if (second == NULL || first == NULL ||
            !AdventureNamePresented(&sim, &bindings, second, CC_NPC_ROLE_SCOUT,
                                    NULL, 0, 0) ||
            !AdventureNamePresented(&sim, &bindings, first, CC_NPC_ROLE_SCOUT,
                                    NULL, 0, 1) ||
            first->character_id != first_id || second->character_id != second_id)
            return ClientRegressionFailure("Rebuilding Scouts in reverse must keep each seed's resident.");
    }
    {
        LocalState rendered = {0}; ResetLocalState(&rendered); rendered.adventure_ui = true;
        Vector2 anchor = {LOCAL_CARRIAGE_BAY.x + 4.0f, LOCAL_CARRIAGE_BAY.y};
        CcLocalAgentInit(&rendered.agent, (Vector2){anchor.x - 4.0f, anchor.y}, false);
        ClientView view = VIEW_LOCAL, back = VIEW_LOCAL; int32_t situation = 0; CcId selected_id = 0U; bool saw_competing_frame = false;
        for (int frame = 0; frame < 600 && view == VIEW_LOCAL; ++frame) {
            CcLocalPresentedPerson people[2] = {
                {{anchor.x - 1.8f, 0, anchor.y + 1.4f}, UINT32_C(0xb1cf), CC_NPC_ROLE_SCOUT, 1},
                {{anchor.x, 0, anchor.y}, UINT32_C(0xb1ce), CC_NPC_ROLE_SCOUT, 1}};
            if (frame == 0) { CcLocalPresentedPerson swap = people[0]; people[0] = people[1]; people[1] = swap; }
            CcLocalRendererTestSetPresentedPeople(people, frame == 0 ? 1 : 2);
            BuildAdventureTargets(&sim, &rendered, (RenderTexture2D){.texture={.width=630,.height=320}}, (Rectangle){0,0,630,320});
            CcInteractionTarget *card = NULL, *new_card = NULL;
            for (int i = 0; i < rendered.interactions.count; ++i) {
                CcInteractionTarget *candidate = &rendered.interactions.targets[i];
                if (candidate->key.object == UINT64_C(0x10000b1ce)) card = candidate;
                if (candidate->key.object == UINT64_C(0x10000b1cf)) new_card = candidate;
            }
            if (card == NULL) return ClientRegressionFailure("The selected rendered Scout must publish a card.");
            if (selected_id == 0U) selected_id = card->character_id;
            if (card->character_id != selected_id || (frame > 0 && (new_card == NULL || new_card->character_id == selected_id)))
                return ClientRegressionFailure("A new earlier Scout must not replace the selected resident.");
            if (frame > 0 && new_card != NULL) saw_competing_frame = true;
            card->visible = true;
            if (frame == 0) { rendered.interaction.focus = card->key; queued_key_press[KEY_F] = true; }
            (void)HandleAdventureScene(NULL, &sim, &rendered, &view, &back, &situation, NULL, 1.0f/60.0f, message, sizeof(message));
            ClientInputClearPressed();
        }
        if (!saw_competing_frame || view != VIEW_CHARACTER || rendered.conversation_character_id != selected_id)
            return ClientRegressionFailure("The selected rendered Scout must keep its conversation identity.");
        CcLocalRendererTestSetPresentedPeople(NULL, 0);
    }
    /* This is the raised, fenced obstacle-course platform beside the crates. */
    CcLocalAgentInit(&local.agent, (Vector2){11.55f, 3.86f}, false);
    if (local.agent.position.y <= CcLocalTerrainHeightAt(
            local.agent.position.x, local.agent.position.z) + 0.24f)
        return ClientRegressionFailure("The moving-person fixture must start on the raised platform.");
    ClientView view = VIEW_LOCAL;
    ClientView return_view = VIEW_LOCAL;
    int32_t selected_situation = 0;
    CcId bound_character = 0U;
    for (int32_t frame = 0; frame < 2700 && view == VIEW_LOCAL; ++frame) {
        float phase = (float)frame / 60.0f;
        Vector3 position = {17.20f + sinf(phase * 0.85f) * 1.10f,
                            0.0f,
                            7.20f + cosf(phase * 0.55f) * 0.55f};
        local.interactions.count = 0;
        CcInteractionTarget *presented = AdventureTarget(
            &local, &sim, CC_INTERACTION_PERSON,
            UINT64_C(0x10000b1ce), "Scout", "Talk", position,
            0.9f, 2.0f, 0.7f, (Vector2){position.x, position.z}, 1.8f);
        if (presented == NULL || !AdventureNamePresented(
                &sim, &local, presented, CC_NPC_ROLE_SCOUT, NULL, 0, 0))
            return ClientRegressionFailure("The visible Scout action needs a resident binding.");
        presented->visible = true;
        if (bound_character == 0U) bound_character = presented->character_id;
        if (presented->character_id != bound_character)
            return ClientRegressionFailure("A moving Scout action must keep its bound resident.");
        (void)HandleAdventureScene(
            NULL, &sim, &local, &view, &return_view, &selected_situation,
            frame == 0 ? presented : NULL, 1.0f / 60.0f,
            message, sizeof(message));
    }
    if (view != VIEW_CHARACTER || local.conversation_character_id != bound_character)
        return ClientRegressionFailure("The raised-platform Scout must open the bound conversation within 45 seconds.");
    if (local.agent.interaction_navigation || local.agent.target_valid)
        return ClientRegressionFailure("Conversation must finish the moving-person approach.");
    view = VIEW_LOCAL;
    Vector3 after_conversation = local.agent.position;
    if (!CcLocalAgentSetStreetTarget(&local.agent,
            (Vector3){LOCAL_CARRIAGE_BAY.x, 0.0f,
                      LOCAL_CARRIAGE_BAY.y}))
        return ClientRegressionFailure("The actor must accept movement after conversation.");
    for (int32_t frame = 0; frame < 360; ++frame)
        (void)CcLocalWorldUpdate(&local.course, &local.agent, &sim,
                                 1.0f / 60.0f, false, true);
    float moved_x = after_conversation.x - local.agent.position.x;
    float moved_z = after_conversation.z - local.agent.position.z;
    if (moved_x * moved_x + moved_z * moved_z < 0.25f)
        return ClientRegressionFailure("The actor must move after a moving-person conversation.");
    local.interactions.count = 0;
    CcInteractionTarget *cancel_target = AdventureTarget(
        &local, &sim, CC_INTERACTION_PERSON, UINT64_C(0x10000b1cf),
        "Scout", "Talk", (Vector3){17.20f, 0.0f, 7.20f},
        0.9f, 2.0f, 0.7f,
        (Vector2){17.20f, 7.20f}, 1.8f);
    if (cancel_target == NULL || !AdventureNamePresented(
            &sim, &local, cancel_target, CC_NPC_ROLE_SCOUT, NULL, 0, 1))
        return ClientRegressionFailure("The second visible Scout action needs a resident binding.");
    cancel_target->visible = true;
    (void)HandleAdventureScene(NULL, &sim, &local, &view, &return_view,
                               &selected_situation, cancel_target,
                               1.0f / 60.0f, message, sizeof(message));
    if (!local.interaction.approaching || !local.agent.interaction_navigation)
        return ClientRegressionFailure("The second Scout action must begin an approach.");
    queued_key_press[KEY_ESCAPE] = true;
    (void)HandleAdventureScene(NULL, &sim, &local, &view, &return_view,
                               &selected_situation, NULL,
                               1.0f / 60.0f, message, sizeof(message));
    ClientInputClearPressed();
    if (local.interaction.approaching || local.agent.interaction_navigation ||
        local.agent.target_valid)
        return ClientRegressionFailure("Escape must cancel a moving-person approach and its route.");
    Vector3 after_cancel = local.agent.position;
    if (!CcLocalAgentSetStreetTarget(&local.agent,
            (Vector3){LOCAL_CARRIAGE_BAY.x, 0.0f,
                      LOCAL_CARRIAGE_BAY.y}))
        return ClientRegressionFailure("The actor must accept movement after cancelling a person approach.");
    for (int32_t frame = 0; frame < 240; ++frame)
        (void)CcLocalWorldUpdate(&local.course, &local.agent, &sim,
                                 1.0f / 60.0f, false, true);
    moved_x = after_cancel.x - local.agent.position.x;
    moved_z = after_cancel.z - local.agent.position.z;
    if (moved_x * moved_x + moved_z * moved_z < 0.25f)
        return ClientRegressionFailure("The actor must move after cancelling a person approach.");
    scout->activity = CC_CHARACTER_ACTIVITY_TRAVELLING;
    AdventurePrunePresentedPersonBindings(&sim, &local);
    for (int32_t i = 0; i < (int32_t)(sizeof(local.presented_person_binding_character) /
                                      sizeof(local.presented_person_binding_character[0]));
         ++i)
        if (local.presented_person_binding_character[i] == bound_character)
            return ClientRegressionFailure("An ineligible resident must clear its presented-person binding.");
    return 0;
}

static int RunReliefCrateInputRegression(void)
{
    static CcSim sim;
    static LocalState local;
    char error[192] = "", message[256] = "", feedback[128] = "";
    float feedback_age = 0.0f;
    int32_t selected = 0, selected_situation = 0;
    ClientView view = VIEW_LOCAL, return_view = VIEW_LOCAL;
    const char *path = "relief-crate-input.ccsave";
    (void)remove(path);
    CcSimInit(&sim, 42U);
    CcSituation *relief = NULL;
    for (int32_t i = 0; i < sim.situation_count; ++i) {
        if (sim.situations[i].kind == CC_SITUATION_RELIEF_DELIVERY) {
            relief = &sim.situations[i];
            break;
        }
    }
    if (relief == NULL) return ClientRegressionFailure("The relief input fixture needs Mara's charter.");
    CcCommand accept = {.kind = CC_COMMAND_ACCEPT_SITUATION, .target_id = relief->id};
    if (!CcSimApply(&sim, &accept, error, sizeof(error)))
        return ClientRegressionFailure(error);
    CcLocalBindPlace(&sim);
    ResetLocalState(&local);
    local.adventure_ui = true;
    local.opening_step = CC_LOCAL_OPENING_COMPLETE;
    CcJournal *journal = CcJournalStart(path, &sim, error, sizeof(error));
    if (journal == NULL) return ClientRegressionFailure(error);
#define CRATE_INPUT_CHECK(condition, text) do { if (!(condition)) { \
    CcJournalAbandon(&journal); (void)remove(path); \
    return ClientRegressionFailure(text); } } while (0)
    CcSettlement *origin = CcSimSettlementMutable(
        &sim, CcSimSituationOfferSettlementId(&sim, relief));
    CRATE_INPUT_CHECK(origin != NULL, "The relief crate needs its granary.");
    int32_t stock = origin->stock[CC_GOOD_FOOD];
    int32_t cargo = sim.player.cargo[CC_GOOD_FOOD];
    RepositionHero(&local, LOCAL_RELIEF_CRATES, false);
    ContextActionSet actions = BuildContextActions(
        &sim, &local, view, selected, selected_situation);
    bool lift_visible = false;
    for (int32_t i = 0; i < actions.count; ++i)
        lift_visible |= actions.items[i].kind == CONTEXT_ACTION_PICKUP_RELIEF_CRATE &&
            actions.items[i].enabled;
    CRATE_INPUT_CHECK(lift_visible, "The granary must offer a lift action.");
    queued_key_press[KEY_F] = true;
    HandleInput(&journal, &sim, &selected, &selected_situation, &view,
        &return_view, &local, (RenderTexture2D){0}, (Rectangle){0},
        1.0f / 60.0f, path, "", message, sizeof(message), feedback,
        sizeof(feedback), &feedback_age);
    ClientInputClearPressed();
    CRATE_INPUT_CHECK(relief->loading_crate_carried &&
        relief->loading_progress == 0 &&
        origin->stock[CC_GOOD_FOOD] == stock - 1 &&
        sim.player.cargo[CC_GOOD_FOOD] == cargo,
        "The granary interaction must put one crate in the player's hands.");
    RepositionHero(&local, LOCAL_CARRIAGE_BAY, false);
    actions = BuildContextActions(&sim, &local, view, selected,
        selected_situation);
    int32_t stow_index = -1;
    for (int32_t i = 0; i < actions.count; ++i)
        if (actions.items[i].kind == CONTEXT_ACTION_STOW_RELIEF_CRATE &&
            actions.items[i].enabled) stow_index = i;
    CRATE_INPUT_CHECK(stow_index >= 0, "The parked carriage must offer a stow action.");
    local.card_page = stow_index / ContextActionPageSize(&actions);
    int32_t first = ContextCardFirst(&local, &actions);
    Rectangle bounds = ContextActionBounds(stow_index - first,
        ContextCardCount(&actions, first), actions.combat);
    client_test_pointer_active = true;
    client_test_pointer = (Vector2){bounds.x + bounds.width * 0.5f,
        bounds.y + bounds.height * 0.5f};
    queued_mouse_button_press[MOUSE_BUTTON_LEFT] = true;
    HandleInput(&journal, &sim, &selected, &selected_situation, &view,
        &return_view, &local, (RenderTexture2D){0}, (Rectangle){0},
        1.0f / 60.0f, path, "", message, sizeof(message), feedback,
        sizeof(feedback), &feedback_age);
    client_test_pointer_active = false;
    ClientInputClearPressed();
    CRATE_INPUT_CHECK(!relief->loading_crate_carried &&
        relief->loading_progress == 1 &&
        origin->stock[CC_GOOD_FOOD] == stock - 1 &&
        sim.player.cargo[CC_GOOD_FOOD] == cargo + 1,
        "The carriage card must stow the carried crate.");
    CcJournalAbandon(&journal);
    (void)remove(path);
#undef CRATE_INPUT_CHECK
    return 0;
}

static int RunLearnedNameRegression(void)
{
    static CcSim sim, loaded;
    static LocalState local, restored;
    const char *path = "learned-name.ccsave";
    char error[256] = "", message[256] = "", feedback[256] = "";
    float feedback_age = 0.0f;
    int32_t selected = 0, selected_situation = 0;
    ClientView view = VIEW_LOCAL, return_view = VIEW_LOCAL;
    (void)remove(path);
    CcSimInit(&sim, 42U);
    CcLocalBindPlace(&sim);
    ResetLocalState(&local);
    local.adventure_ui = true;
    uint64_t before_scene = CcSimHash(&sim);
    BuildAdventureTargets(&sim, &local, (RenderTexture2D){0}, (Rectangle){0});
    if (CcSimHash(&sim) != before_scene)
        return ClientRegressionFailure("Reading town labels must preserve the world.");
    CcInteractionTarget *target = NULL;
    for (int32_t i = 0; i < local.interactions.count; ++i) {
        CcInteractionTarget *candidate = &local.interactions.targets[i];
        const CcCharacter *person = CcSimCharacter(&sim, candidate->character_id);
        if (candidate->key.kind == CC_INTERACTION_PERSON && person != NULL &&
            !AdventureKnownPerson(&sim, person)) { target = candidate; break; }
    }
    if (target == NULL) return ClientRegressionFailure("The town needs an unknown named resident.");
    CcId person_id = target->character_id;
    char role[64], name[CC_NAME_CAPACITY];
    (void)snprintf(role, sizeof(role), "%s", target->name);
    (void)snprintf(name, sizeof(name), "%s", CcSimCharacter(&sim, person_id)->name);
    if (strcmp(role, name) == 0)
        return ClientRegressionFailure("A stranger's card must show the role first.");
    target->visible = true;
    target->x = local.agent.position.x;
    target->z = local.agent.position.z;
    target->approach_x = target->x;
    target->approach_z = target->z;
    target->radius = 2.0f;
    local.interaction.focus = target->key;
    CcJournal *journal = CcJournalStart(path, &sim, error, sizeof(error));
    if (journal == NULL) return ClientRegressionFailure(error);
#define NAME_CHECK(condition, text) do { if (!(condition)) { \
    CcJournalAbandon(&journal); \
    (void)remove(path); return ClientRegressionFailure(text); } } while (0)
    queued_key_press[KEY_F] = true;
    HandleInput(&journal, &sim, &selected, &selected_situation, &view, &return_view,
        &local, (RenderTexture2D){0}, (Rectangle){0}, 1.0f / 60.0f, path, "",
        message, sizeof(message), feedback, sizeof(feedback), &feedback_age);
    ClientInputClearPressed();
    NAME_CHECK(view == VIEW_CHARACTER && local.conversation_character_id == person_id &&
        strcmp(local.conversation_name, name) == 0 &&
        CcSimCharacter(&sim, person_id)->introduced_day > 0,
        "Talk must save the resident's introduction and show the name immediately.");
    NAME_CHECK(AdventureKnownPerson(&sim, CcSimCharacter(&sim, person_id)),
        "The saved company must know the person after Talk.");
    NAME_CHECK(CcJournalCheckpoint(journal, &sim, error, sizeof(error)),
        "Save the introduced person.");
    uint64_t saved = CcSimHash(&sim);
    (void)CcJournalClose(&journal, &sim, error, sizeof(error));
    journal = CcJournalResume(path, &loaded, error, sizeof(error));
    NAME_CHECK(journal != NULL && CcSimHash(&loaded) == saved,
        "Reload the same world and introduction.");
    ResetLocalState(&restored);
    restored.adventure_ui = true;
    BuildAdventureTargets(&loaded, &restored, (RenderTexture2D){0}, (Rectangle){0});
    NAME_CHECK(CcSimHash(&loaded) == saved,
        "Rebuilding labels after reload must preserve the world.");
    CcInteractionTarget *returned = NULL;
    for (int32_t i = 0; i < restored.interactions.count; ++i)
        if (restored.interactions.targets[i].character_id == person_id)
            returned = &restored.interactions.targets[i];
    NAME_CHECK(returned != NULL && strcmp(returned->name, name) == 0,
        "The learned name must return with the same lifetime identity.");
    CcJournalAbandon(&journal);
    CcCharacter *departed = (CcCharacter *)CcSimCharacter(&loaded, person_id);
    NAME_CHECK(departed != NULL, "The learned person must remain in the loaded world.");
    departed->activity = CC_CHARACTER_ACTIVITY_TRAVELLING;
    BuildAdventureTargets(&loaded, &restored, (RenderTexture2D){0}, (Rectangle){0});
    for (int32_t i = 0; i < restored.interactions.count; ++i)
        NAME_CHECK(restored.interactions.targets[i].character_id != person_id,
            "A departed person must leave the town cast.");
    (void)remove(path);
#undef NAME_CHECK
    return 0;
}

static int RunAdventureInputRegression(void)
{
    int relief_result = RunReliefCrateInputRegression();
    if (relief_result != 0) return relief_result;
    int name_result = RunLearnedNameRegression();
    if (name_result != 0) return name_result;
    int moving_person_result = RunMovingPresentedPersonRegression();
    if (moving_person_result != 0) return moving_person_result;
    int mine_road_result = RunAdventureMineRoadRegression();
    if (mine_road_result != 0) return mine_road_result;
    int road_result = RunPresentedRoadRegression();
    if (road_result != 0) return road_result;
    int displayed_road_result = RunDisplayedRoadChoiceRegression();
    if (displayed_road_result != 0) return displayed_road_result;
    int focus_result = RunConversationFocusRegression();
    if (focus_result != 0) return focus_result;
    int handoff_result = RunDeliveryHandoffRegression();
    if (handoff_result != 0) return handoff_result;
    int presented_result = RunPresentedTargetRegression();
    if (presented_result != 0) return presented_result;
    int witness_card_result = RunSituationWitnessCardPriorityRegression();
    if (witness_card_result != 0) return witness_card_result;
    {
        CcSim choice_sim;
        LocalState choice_local = {0};
        char error[192] = "", label[128] = "";
        CcSimInit(&choice_sim, 42U);
        ResetLocalState(&choice_local);
        choice_local.adventure_ui = true;
        int32_t opening = OpeningSituationIndex(&choice_sim);
        CcSituation *offer = opening >= 0 ?
            &choice_sim.situations[opening] : NULL;
        const CcCharacter *giver = offer != NULL ?
            CcSimSituationConversationCharacter(
                &choice_sim, offer, choice_sim.player.location_id) : NULL;
        if (offer == NULL || giver == NULL)
            return ClientRegressionFailure("The opening promise needs its giver.");
        choice_local.conversation_situation_id = offer->id;
        choice_local.conversation_character_id = giver->id;
        ContextActionSet choices = BuildContextActions(
            &choice_sim, &choice_local, VIEW_CHARACTER, opening, opening);
        if (choices.count != 2 ||
            choices.items[0].kind != CONTEXT_ACTION_LISTEN_CHARACTER ||
            strcmp(ContextTouchActionLabel(&choice_local, VIEW_CHARACTER, 0,
                &choices.items[0], label, sizeof(label)),
                "1 What do they need?") != 0 ||
            choices.items[1].kind != CONTEXT_ACTION_CLOSE_VIEW ||
            strncmp(ContextTouchActionLabel(&choice_local, VIEW_CHARACTER, 1,
                &choices.items[1], label, sizeof(label)), "2 ", 2) != 0)
            return ClientRegressionFailure(
                "Opening semantic choices must publish their real number keys.");
        CcCommand listen = {.kind = CC_COMMAND_CHARACTER_RESPONSE,
            .target_id = offer->id,
            .amount = CC_CHARACTER_RESPONSE_LISTEN};
        if (!CcSimApply(&choice_sim, &listen, error, sizeof(error)))
            return ClientRegressionFailure(error);
        giver = CcSimCharacter(&choice_sim, choice_local.conversation_character_id);
        if (giver == NULL || !CcCharacterRemembers(
                giver, CC_CHARACTER_MEMORY_MET_PLAYER, offer->id)) {
            (void)fprintf(stderr, "conversation=%" PRIu64 " sponsor=%" PRIu64
                " affected=%" PRIu64 " memory=%d\n",
                choice_local.conversation_character_id,
                offer->sponsor_character_id, offer->affected_character_id,
                giver != NULL ? giver->memory_count : -1);
            return ClientRegressionFailure(
                "The opening answer must belong to its visible speaker.");
        }
        choices = BuildContextActions(
            &choice_sim, &choice_local, VIEW_CHARACTER, opening, opening);
        if (choices.count != 2 ||
            choices.items[0].kind != CONTEXT_ACTION_PLEDGE_CHARACTER ||
            strcmp(ContextTouchActionLabel(&choice_local, VIEW_CHARACTER, 0,
                &choices.items[0], label, sizeof(label)),
                "1 I'll take the job.") != 0 ||
            choices.items[1].kind != CONTEXT_ACTION_CLOSE_VIEW ||
            strncmp(ContextTouchActionLabel(&choice_local, VIEW_CHARACTER, 1,
                &choices.items[1], label, sizeof(label)), "2 ", 2) != 0)
            return ClientRegressionFailure(
                "Promise choices must announce the same numbered actions as the canvas.");
        CcCommand pledge = listen;
        pledge.amount = CC_CHARACTER_RESPONSE_PLEDGE_HELP;
        if (!CcSimApply(&choice_sim, &pledge, error, sizeof(error)) ||
            choice_sim.player.accepted_situation_id != offer->id ||
            choice_sim.player.cargo[CC_GOOD_FOOD] != 0 ||
            offer->loading_progress != 0 || offer->loading_crate_carried)
            return ClientRegressionFailure(
                "The announced first promise choice must leave Mara's eight crates ready to carry.");
    }
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
    local.trade_presented_command = shown.command; local.trade_presented_total = shown.total; local.trade_presented_reward = shown.reward; \
    local.trade_presented_shop_building_index = local.shop_building_index; } \
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
    const CcLocalPlaceProfile *shop_profile = CcLocalPlaceProfileForSettlement(
        CcSimSettlement(&sim, sim.player.location_id));
    const CcLocalShop *bakery_shop = CcLocalShopAt(shop_profile,
        CC_LOCAL_SHOP_BAKERY);
    UX_CHECK(bakery_shop != NULL, "The town needs a bakery door.");
    target->key.kind = CC_INTERACTION_DOOR;
    target->key.object = (uint64_t)bakery_shop->building_index + 1U;
    CcLocalLanePoint bakery_approach = CcLocalShopApproach(shop_profile, bakery_shop);
    target->approach_x = bakery_approach.x;
    target->approach_z = bakery_approach.z;
    target->radius = 1.1f;
    RepositionHero(&local, (Vector2){bakery_approach.x,
        bakery_approach.z + 1.6f}, false);
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
        if (!CcLocalShopSells(bakery_shop, (CcGood)good)) continue;
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
    local.trade_presented_shop_building_index = local.shop_building_index;
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
        for (int32_t i = 0; i < local.interactions.count; ++i) {
            if (local.interactions.targets[i].key.kind == CC_INTERACTION_PERSON &&
                local.interactions.targets[i].character_id == 0U) {
                return ClientRegressionFailure("Talkable town figures are always named residents.");
            }
        }
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
        (void)HandleAdventureScene(NULL, &sim, &local, &view, &return_view, &selected_situation, NULL, 1.0f / 60.0f, message, sizeof(message));
        ClientInputClearPressed();
        for (int frame = 0; frame < 3000 && !local.market_interior && local.interaction.approaching; ++frame) {
            (void)HandleAdventureScene(NULL, &sim, &local, &view, &return_view, &selected_situation, NULL, 1.0f / 60.0f, message, sizeof(message));
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
        (void)HandleAdventureScene(NULL, &sim, &local, &view, &return_view, &selected_situation, NULL, 1.0f / 60.0f, message, sizeof(message));
        ClientInputClearPressed();
        for (int frame = 0; frame < 1200 && view == VIEW_LOCAL && local.interaction.approaching; ++frame)
            (void)HandleAdventureScene(NULL, &sim, &local, &view, &return_view, &selected_situation, NULL, 1.0f / 60.0f, message, sizeof(message));
        if (view != VIEW_TRADE) return ClientRegressionFailure("Each keeper must be reachable from the shop entrance.");
    }
    puts("six town door and counter routes passed");
    return 0;
}

/* Counter fixtures cover money limits and a load whose journey has completed.
   The journey and journal suites cover how that arrival state is produced. */
static int RunAdventureShopFilterRegression(void)
{
    static CcSim sim;
    static LocalState local, restored;
    char error[192] = "", message[256] = "";
    const char *path = "shop-session-test.state";
    (void)remove(path);
    CcSimInit(&sim, 42U);
    CcLocalBindPlace(&sim);
    ResetLocalState(&local);
    local.adventure_ui = true;
    const CcSettlement *town = CcSimSettlement(&sim, sim.player.location_id);
    const CcLocalPlaceProfile *profile = CcLocalPlaceProfileForSettlement(town);
    const CcLocalShop *bakery = CcLocalShopAt(profile, CC_LOCAL_SHOP_BAKERY);
    const CcLocalShop *stonecutter = CcLocalShopAt(profile, CC_LOCAL_SHOP_STONECUTTER);
#define SHOP_CHECK(condition, text) do { if (!(condition)) { (void)remove(path); return ClientRegressionFailure(text); } } while (0)
    SHOP_CHECK(bakery != NULL && stonecutter != NULL,
        "The starting town needs bakery and stonecutter shop doors.");
    local.market_interior = true;
    local.shop_building_index = bakery->building_index;
    RepositionHero(&local, (Vector2){2.05f, 5.35f}, true);
    local.trade_good = CC_GOOD_BREAD;
    local.trade_quantity = 1;
    sim.player.coins = 1000;
    uint64_t before = CcSimHash(&sim);
    SHOP_CHECK(CcLocalShopSells(LocalActiveShop(&sim, &local), CC_GOOD_BREAD) &&
        !CcLocalShopSells(LocalActiveShop(&sim, &local), CC_GOOD_STONE),
        "The bakery lists bread and excludes stone.");
    local.trade_good = CC_GOOD_STONE;
    SHOP_CHECK(!AdventureTradeQuote(&sim, &local).available,
        "A stale stone choice cannot trade at the bakery.");
    local.trade_good = CC_GOOD_BREAD;
    AdventureQuote bread = AdventureTradeQuote(&sim, &local);
    SHOP_CHECK(bread.available, "The bakery quotes its own bread stock.");
    RenderTexture2D texture = {.texture = {.width = 960, .height = 540}};
    BuildAdventureTargets(&sim, &local, texture, (Rectangle){0, 0, 960, 540});
    for (int32_t i = 0; i < local.interactions.count; ++i)
        if (local.interactions.targets[i].key.kind == CC_INTERACTION_STOCK)
            SHOP_CHECK(local.interactions.targets[i].amount == CC_GOOD_BREAD,
                "Bakery shelf hotspots expose only bread.");
    local.trade_quote_presented = true;
    local.trade_presented_command = bread.command;
    local.trade_presented_total = bread.total;
    local.trade_presented_shop_building_index = bakery->building_index;
    local.shop_building_index = stonecutter->building_index;
    local.trade_good = CC_GOOD_STONE;
    SHOP_CHECK(CcLocalShopSells(LocalActiveShop(&sim, &local), CC_GOOD_STONE) &&
        !CcLocalShopSells(LocalActiveShop(&sim, &local), CC_GOOD_BREAD),
        "The stonecutter lists stone and excludes bread.");
    SHOP_CHECK(AdventureTradeQuote(&sim, &local).available,
        "The stonecutter quotes its own stone stock.");
    ClientView view = VIEW_TRADE;
    queued_key_press[KEY_ENTER] = true;
    (void)HandleAdventureTrade(NULL, &sim, &local, &view, message,
        sizeof(message));
    ClientInputClearPressed();
    SHOP_CHECK(CcSimHash(&sim) == before &&
        strstr(local.receipt, "quote changed") != NULL,
        "A bakery quote cannot be confirmed at the stonecutter.");
    local.trade_quote_presented = false;
    local.trade_good = CC_GOOD_BREAD;
    SHOP_CHECK(!AdventureTradeQuote(&sim, &local).available,
        "A stale bread choice cannot trade at the stonecutter.");
    local.trade_good = CC_GOOD_STONE;
    BuildAdventureTargets(&sim, &local, texture, (Rectangle){0, 0, 960, 540});
    for (int32_t i = 0; i < local.interactions.count; ++i)
        if (local.interactions.targets[i].key.kind == CC_INTERACTION_STOCK)
            SHOP_CHECK(local.interactions.targets[i].amount == CC_GOOD_STONE,
                "Stonecutter shelf hotspots expose only stone.");
    SHOP_CHECK(SaveLocalSession(path, &sim, &local, error, sizeof(error)),
        "The stonecutter interior should save its shop context.");
    ResetLocalState(&restored);
    SHOP_CHECK(RestoreLocalSession(path, &sim, &restored) &&
        restored.market_interior &&
        restored.shop_building_index == stonecutter->building_index,
        "Reload restores the stonecutter instead of the old hall.");
    restored.trade_good = CC_GOOD_BREAD;
    restored.trade_quantity = 1;
    SHOP_CHECK(!AdventureTradeQuote(&sim, &restored).available,
        "Reload keeps bread outside the stonecutter's offers.");
    CcInteractionTarget exit = {.key = {sim.player.location_id, 1U,
        CC_INTERACTION_EXIT}};
    ClientView back = VIEW_LOCAL;
    int32_t selected = 0;
    AdventureActivate(&sim, &restored, &exit, &view, &back, &selected);
    CcLocalLanePoint approach = CcLocalShopApproach(profile, stonecutter);
    SHOP_CHECK(!restored.market_interior && restored.shop_building_index == -1 &&
        GridDistance(LocalPosition(&restored),
            (Vector2){approach.x, approach.z}) < 0.25f,
        "Leaving the stonecutter returns to its own door.");
    local.shop_building_index = bakery->building_index;
    AdventureActivate(&sim, &local, &exit, &view, &back, &selected);
    approach = CcLocalShopApproach(profile, bakery);
    SHOP_CHECK(GridDistance(LocalPosition(&local),
        (Vector2){approach.x, approach.z}) < 0.25f,
        "Leaving the bakery returns to the bakery door.");
    SHOP_CHECK(CcSimHash(&sim) == before,
        "Browsing shops, saving and leaving preserve simulation time and stock.");
    (void)remove(path);
#undef SHOP_CHECK
    return 0;
}

static int RunAdventureSupplyQuoteRegression(void)
{
    static CcSim sim;
    static LocalState local;
    char message[256] = "";
#define SUPPLY_CHECK(condition, text) do { if (!(condition)) return ClientRegressionFailure(text); } while (0)
    CcSimInit(&sim, 42U);
    CcLocalBindPlace(&sim);
    ResetLocalState(&local);
    local.adventure_ui = true;
    local.market_interior = true;
    const CcSettlement *town = CcSimSettlement(&sim, sim.player.location_id);
    const CcLocalPlaceProfile *profile = CcLocalPlaceProfileForSettlement(town);
    const CcLocalShop *grain = CcLocalShopAt(profile, CC_LOCAL_SHOP_GRAIN_MERCHANT);
    const CcLocalShop *bakery = CcLocalShopAt(profile, CC_LOCAL_SHOP_BAKERY);
    SUPPLY_CHECK(grain != NULL && bakery != NULL, "The grain source and bakery buyer need local doors.");
    local.shop_building_index = grain->building_index;
    local.trade_good = CC_GOOD_WHEAT;
    local.trade_quantity = 2;
    local.trade_mode = 0;
    CcSupplyOffer wheat_offer = CcSimSupplyOffer(&sim, CC_GOOD_WHEAT);
    AdventureQuote source = AdventureTradeQuote(&sim, &local);
    SUPPLY_CHECK(source.available && source.command.kind == CC_COMMAND_TRADE_SUPPLY &&
        source.command.amount == 2 && source.command.target_id == town->id &&
        source.total == 2 * wheat_offer.source_unit_price,
        "The grain merchant quotes the paid Wheat source offer.");
    CcMoney purse = sim.player.coins;
    int32_t bread = town->stock[CC_GOOD_BREAD];
    local.trade_quote_presented = true;
    local.trade_presented_command = source.command;
    local.trade_presented_total = source.total;
    local.trade_presented_shop_building_index = grain->building_index;
    ClientView view = VIEW_TRADE;
    queued_key_press[KEY_ENTER] = true;
    (void)HandleAdventureTrade(NULL, &sim, &local, &view, message, sizeof(message));
    ClientInputClearPressed();
    SUPPLY_CHECK(sim.player.cargo[CC_GOOD_WHEAT] == 2 &&
        sim.player.coins == purse - source.total &&
        strstr(local.receipt, "Bought 2 Wheat") != NULL,
        "The quoted grain purchase uses the authoritative paid command.");
    local.shop_building_index = bakery->building_index;
    local.trade_mode = 1;
    local.trade_confirmed = false;
    AdventureQuote buyer = AdventureTradeQuote(&sim, &local);
    SUPPLY_CHECK(buyer.available && buyer.command.kind == CC_COMMAND_TRADE_SUPPLY &&
        buyer.command.amount == -2 && buyer.total == 2 * wheat_offer.delivery_unit_price,
        "The bakery quotes payment for Wheat in the carriage.");
    uint64_t bought_hash = CcSimHash(&sim);
    queued_key_press[KEY_ENTER] = true;
    (void)HandleAdventureTrade(NULL, &sim, &local, &view, message, sizeof(message));
    ClientInputClearPressed();
    SUPPLY_CHECK(CcSimHash(&sim) == bought_hash &&
        strstr(local.receipt, "quote changed") != NULL,
        "A source quote cannot authorize a bakery delivery.");
    local.trade_presented_command = buyer.command;
    local.trade_presented_total = buyer.total;
    local.trade_presented_shop_building_index = bakery->building_index;
    queued_key_press[KEY_ENTER] = true;
    (void)HandleAdventureTrade(NULL, &sim, &local, &view, message, sizeof(message));
    ClientInputClearPressed();
    SUPPLY_CHECK(sim.player.cargo[CC_GOOD_WHEAT] == 0 &&
        sim.player.coins == purse - source.total + buyer.total &&
        CcSimSettlement(&sim, town->id)->stock[CC_GOOD_BREAD] == bread + 2 &&
        CcSimSupplyOffer(&sim, CC_GOOD_WHEAT).remaining == wheat_offer.remaining - 2 &&
        strstr(local.receipt, "Delivered 2 Wheat") != NULL,
        "Bakery input delivery pays the quoted reward and fills its finite order.");

    const CcLocalShop *mine = NULL;
    const CcLocalShop *stonecutter = NULL;
    for (int32_t i = 0; i < sim.settlement_count; ++i) {
        profile = CcLocalPlaceProfileForSettlement(&sim.settlements[i]);
        mine = CcLocalShopAt(profile, CC_LOCAL_SHOP_MINE_SUPPLIER);
        if (mine != NULL) {
            stonecutter = CcLocalShopAt(profile, CC_LOCAL_SHOP_STONECUTTER);
            sim.player.location_id = sim.settlements[i].id;
            sim.carriage.location_id = sim.player.location_id;
            break;
        }
    }
    SUPPLY_CHECK(mine != NULL && stonecutter != NULL,
        "Silverwick needs a mine supplier and stonecutter.");
    local.shop_building_index = mine->building_index;
    local.trade_mode = 0;
    local.trade_good = CC_GOOD_RAW_STONE;
    local.trade_quantity = 1;
    local.trade_confirmed = false;
    source = AdventureTradeQuote(&sim, &local);
    SUPPLY_CHECK(source.available && source.command.kind == CC_COMMAND_TRADE_SUPPLY &&
        source.total == 3, "The mine supplier sells raw stone at the haul price.");
    local.shop_building_index = stonecutter->building_index;
    local.trade_mode = 1;
    buyer = AdventureTradeQuote(&sim, &local);
    SUPPLY_CHECK(buyer.command.kind == CC_COMMAND_TRADE_SUPPLY && buyer.total == 6 &&
        !buyer.available, "The stonecutter waits for raw stone cargo.");
#undef SUPPLY_CHECK
    return 0;
}

static int RunAdventureTradeTermsRegression(void)
{
    int shop_result = RunAdventureShopFilterRegression();
    if (shop_result != 0) return shop_result;
    shop_result = RunAdventureSupplyQuoteRegression();
    if (shop_result != 0) return shop_result;
    static CcSim sim;
    static LocalState local;
    char error[192] = "", message[256] = "";
    CcSimInit(&sim, 42U);
    memset(&local, 0, sizeof(local));
    local.market_interior = true;
    local.shop_building_index = CcLocalShopAt(CcLocalPlaceProfileForSettlement(
        CcSimSettlement(&sim, sim.player.location_id)),
        CC_LOCAL_SHOP_BAKERY)->building_index;
    local.trade_good = CC_GOOD_FOOD;
    local.trade_quantity = 1;
#define TERMS_CHECK(condition, text) do { if (!(condition)) return ClientRegressionFailure(text); } while (0)
    CcSettlement *place = CcSimSettlementMutable(&sim, sim.player.location_id);
    char touch_label[384];
    AdventureQuote quote = AdventureTradeQuote(&sim, &local);
    AdventureTradeConfirmTouchLabel(&quote, 0, touch_label, sizeof(touch_label));
    TERMS_CHECK(quote.available && strstr(touch_label, "Buy 1 Bread. Total cost:") == touch_label &&
        strstr(touch_label, " crowns.") != NULL,
        "The focused Buy action must state its quantity and full cost.");
    local.trade_mode = 1;
    quote = AdventureTradeQuote(&sim, &local);
    AdventureTradeConfirmTouchLabel(&quote, 1, touch_label, sizeof(touch_label));
    TERMS_CHECK(!quote.available && strstr(touch_label, "Sell 1 Bread. Sale value:") == touch_label &&
        strstr(touch_label, quote.reason) != NULL,
        "The focused Sell action must state its value and disabled reason.");
    sim.player.cargo[CC_GOOD_FOOD] = 1;
    quote = AdventureTradeQuote(&sim, &local);
    AdventureTradeConfirmTouchLabel(&quote, 1, touch_label, sizeof(touch_label));
    TERMS_CHECK(quote.available && strstr(touch_label, "Sell 1 Bread. Sale value:") == touch_label,
        "The enabled Sell action must state the full sale value.");
    sim.player.cargo[CC_GOOD_FOOD] = 0;
    local.trade_mode = 0;
    CcMoney coins = sim.player.coins;
    sim.player.coins = 0;
    quote = AdventureTradeQuote(&sim, &local);
    AdventureTradeConfirmTouchLabel(&quote, 0, touch_label, sizeof(touch_label));
    TERMS_CHECK(strstr(touch_label, quote.reason) != NULL,
        "The focused Buy action must explain its purse limit.");
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
    TERMS_CHECK(ClientLoadReliefFixture(&sim, promise, error, sizeof(error)), error);
    sim.player.location_id = promise->target_id;
    sim.carriage.location_id = promise->target_id;
    place = CcSimSettlementMutable(&sim, promise->target_id);
    local.shop_building_index = CcLocalShopAt(CcLocalPlaceProfileForSettlement(place),
        CC_LOCAL_SHOP_GRAIN_MERCHANT)->building_index;
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
    AdventureTradeConfirmTouchLabel(&quote, 2, touch_label, sizeof(touch_label));
    TERMS_CHECK(strstr(touch_label, "Deliver ") == touch_label &&
        strstr(touch_label, "Sale value:") != NULL &&
        strstr(touch_label, "Promise reward:") != NULL,
        "The focused delivery action must state both the sale and reward.");
    local.trade_quote_presented = true;
    local.trade_presented_command = quote.command;
    local.trade_presented_total = quote.total;
    local.trade_presented_reward = quote.reward;
    local.trade_presented_promise_id = quote.promise_id;
    local.trade_presented_shop_building_index = local.shop_building_index;
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
    local.market_interior = true;
    local.shop_building_index = CcLocalShopAt(CcLocalPlaceProfileForSettlement(
        CcSimSettlement(&sim, sim.player.location_id)),
        CC_LOCAL_SHOP_BAKERY)->building_index;
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
    local.shop_building_index = CcLocalShopAt(CcLocalPlaceProfileForSettlement(
        CcSimSettlement(&sim, sim.player.location_id)),
        CC_LOCAL_SHOP_GRAIN_MERCHANT)->building_index;
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
    queued_key_press[KEY_FIVE] = true;
    (void)HandleAdventureTrade(NULL, &sim, &local, &view, message, sizeof(message));
    ClientInputClearPressed();
    TERMS_CHECK(local.trade_mode == 5, "The fifth grain merchant tab must open the road meeting.");
    queued_key_press[KEY_ENTER] = true;
    (void)HandleAdventureTrade(NULL, &sim, &local, &view, message, sizeof(message));
    ClientInputClearPressed();
    TERMS_CHECK(CcSimHash(&sim) == meeting_hash, "Reading the meeting must preserve funds and commitments.");

#undef TERMS_CHECK
    puts("adventure trade terms passed");
    return 0;
}

static int RunAbandonedTownRegression(void)
{
    static CcSim sim;
    static LocalState local;
    CcSimInit(&sim, 42U);
    ResetLocalState(&local);
    local.adventure_ui = true;
    local.opening_step = CC_LOCAL_OPENING_COMPLETE;
    CcSettlement *town = &sim.settlements[5];
    sim.player.location_id = town->id;
    town->population = 0;
    town->service_mask = 0;
    town->prosperity = 0;
    town->hunger = 100;
    for (int32_t i = 0; i < sim.character_count; ++i)
        sim.characters[i].current_settlement_id = sim.settlements[0].id;
    CcCharacter *visitor = &sim.characters[0];
    visitor->current_settlement_id = town->id;
    visitor->activity = CC_CHARACTER_ACTIVITY_RECOVERING;
    visitor->death_day = 0;
    visitor->birth_day = sim.current_day - 30 * 365;
    local.course.travellers[0].active = true;
    local.course.situation_witness_active = true;
    uint64_t hash = CcSimHash(&sim);
    RenderTexture2D texture = {.texture = {.width = 960, .height = 540}};
    Rectangle destination = {0, 0, 960, 540};
    BuildAdventureTargets(&sim, &local, texture, destination);
    int32_t people = 0;
    bool closed_hall = false;
    for (int32_t i = 0; i < local.interactions.count; ++i) {
        const CcInteractionTarget *target = &local.interactions.targets[i];
        if (target->key.kind == CC_INTERACTION_PERSON) {
            ++people;
            if (target->character_id != visitor->id)
                return ClientRegressionFailure("Ruin people must use a present living character.");
        }
        if (target->key.kind == CC_INTERACTION_BOARD)
            return ClientRegressionFailure("An abandoned town has a retired notice board.");
        if (target->key.kind == CC_INTERACTION_DOOR) closed_hall = !target->available;
    }
    if (people != 1 || !closed_hall)
        return ClientRegressionFailure("Show the visitor and the closed hall in the ruin.");
    local.market_interior = true;
    BuildAdventureTargets(&sim, &local, texture, destination);
    if (local.interactions.count != 1 || local.interactions.targets[0].key.kind != CC_INTERACTION_EXIT)
        return ClientRegressionFailure("A saved interior in a ruin offers the street exit.");
    local.market_interior = false;
    visitor->current_settlement_id = sim.settlements[0].id;
    BuildAdventureTargets(&sim, &local, texture, destination);
    for (int32_t i = 0; i < local.interactions.count; ++i)
        if (local.interactions.targets[i].key.kind == CC_INTERACTION_PERSON)
            return ClientRegressionFailure("A departed visitor leaves the scene.");
    visitor->current_settlement_id = town->id;
    if (CcSimHash(&sim) != hash)
        return ClientRegressionFailure("Scene inspection preserves the simulation.");
    town->population = 180;
    BuildAdventureTargets(&sim, &local, texture, destination);
    bool open_hall = false;
    for (int32_t i = 0; i < local.interactions.count; ++i)
        if (local.interactions.targets[i].key.kind == CC_INTERACTION_DOOR)
            open_hall = local.interactions.targets[i].available;
    if (!open_hall) return ClientRegressionFailure("Resettlement refreshes the scene immediately.");
    puts("Abandoned town targets follow living presence and resettlement.");
    return 0;
}
