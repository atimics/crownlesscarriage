/* Exercise the real input boundary with both card and world pointer positions. */
static int WorldCardIndex(const ContextActionSet *cards, ContextActionKind kind, int32_t amount)
{
    for (int32_t i = 0; i < cards->count; ++i)
        if (cards->items[i].kind == kind && cards->items[i].amount == amount) return i;
    return -1;
}

static bool WorldCardPoint(CcSim *sim, LocalState *local, ClientView view,
    int32_t selected, ContextActionKind kind, int32_t amount, bool world)
{
    ContextActionSet cards = BuildContextActions(sim, local, view, selected, 0);
    int32_t index = WorldCardIndex(&cards, kind, amount);
    if (index < 0) return false;
    client_test_pointer_active = true;
    if (!world) {
        local->card_page = index / ContextCardsPerPage();
        int32_t first = ContextCardFirst(local, cards.count);
        Rectangle card = ContextActionBounds(index - first, ContextCardCount(cards.count, first));
        client_test_pointer = (Vector2){card.x + card.width * 0.5f, card.y + card.height * 0.5f};
    } else {
        BuildWorldActionTargets(sim, local, view, selected, 0, (RenderTexture2D){0}, (Rectangle){0});
        local->interaction_view = view;
        CcInteractionTarget *chosen = NULL;
        for (int32_t i = 0; i < local->interactions.count; ++i) {
            CcInteractionTarget *target = &local->interactions.targets[i];
            if (target->action == (int32_t)kind && target->amount == amount) chosen = target;
        }
        if (chosen == NULL) return false;
        chosen->visible = true;
        chosen->left = 490; chosen->right = 510; chosen->top = 290; chosen->bottom = 310;
        chosen->camera_distance = 8;
        client_test_pointer = (Vector2){500, 300};
    }
    queued_mouse_button_press[MOUSE_BUTTON_LEFT] = true;
    return true;
}

static int RunWorldCardRegression(void)
{
    static CcSim sim;
    static LocalState local;
    uint64_t results[2] = {0};
    const char *path = "world-cards.ccsave";
    char error[192] = "", message[256] = "", feedback[256] = "";
    float feedback_age = 0;
    CcJournal *journal = NULL;
    ClientView view, return_view = VIEW_LOCAL;
    int32_t selected = 0, selected_situation = 0;
#define CARD_CHECK(test, reason) do { if (!(test)) { \
    client_test_pointer_active = false; ClientInputClearPressed(); \
    if (journal != NULL) (void)CcJournalClose(&journal, &sim, error, sizeof(error)); \
    (void)remove(path); return ClientRegressionFailure(reason); } } while (0)
#define CARD_INPUT() do { HandleInput(&journal, &sim, &selected, &selected_situation, \
    &view, &return_view, &local, (RenderTexture2D){0}, (Rectangle){0}, 1.0f / 60.0f, path, "", \
    message, sizeof(message), feedback, sizeof(feedback), &feedback_age); ClientInputClearPressed(); } while (0)
    for (int32_t mode = 0; mode < 2; ++mode) {
        (void)remove(path);
        CcSimInit(&sim, 42U);
        LeaveOpenWorld(&local);
        CcLocalBindPlace(&sim);
        ResetLocalState(&local);
        local.adventure_ui = true;
        local.course.automatic_alarm = false;
        local.course.alarm_active = false;
        journal = CcJournalStart(path, &sim, error, sizeof(error));
        CARD_CHECK(journal != NULL, error);
        view = VIEW_CARRIAGE;
        uint64_t carriage_hash = CcSimHash(&sim);
        for (int32_t tab = CARRIAGE_PONIES; tab >= CARRIAGE_OVERVIEW; --tab) {
            if (mode == 0) {
                Rectangle bounds = CarriageTabBounds(tab);
                client_test_pointer_active = true;
                client_test_pointer = (Vector2){bounds.x + bounds.width * 0.5f,
                    bounds.y + bounds.height * 0.5f};
                queued_mouse_button_press[MOUSE_BUTTON_LEFT] = true;
            } else {
                queued_key_press[KEY_ONE + tab] = true;
            }
            CARD_INPUT();
            CARD_CHECK(view == VIEW_CARRIAGE && local.carriage_tab == (CarriageTab)tab &&
                CcSimHash(&sim) == carriage_hash,
                "Pointer and keyboard tab selection must preserve the carriage and campaign.");
        }
        queued_key_press[KEY_F7] = true;
        CARD_INPUT();
        CARD_CHECK(view == VIEW_CARRIAGE && local.carriage_tab == CARRIAGE_PONIES,
            "F7 must select the carriage's Ponies tab.");
        queued_key_press[KEY_M] = true;
        CARD_INPUT();
        CARD_CHECK(view == VIEW_MAP && return_view == VIEW_CARRIAGE,
            "The map case must open from the Ponies tab.");
        queued_key_press[KEY_M] = true;
        CARD_INPUT();
        CARD_CHECK(view == VIEW_CARRIAGE && local.carriage_tab == CARRIAGE_PONIES,
            "Closing the map must restore the Ponies tab.");
        ContextActionSet carriage_cards = BuildContextActions(&sim, &local, view, selected, 0);
        CARD_CHECK(WorldCardIndex(&carriage_cards, CONTEXT_ACTION_CHOOSE_ROAD, 0) >= 0 &&
            WorldCardIndex(&carriage_cards, CONTEXT_ACTION_CLOSE_VIEW, 0) >= 0,
            "The Ponies tab must offer departure and step-away actions.");
        queued_key_press[mode == 0 ? KEY_ESCAPE : KEY_BACKSPACE] = true;
        CARD_INPUT();
        CARD_CHECK(view == VIEW_LOCAL && CcSimHash(&sim) == carriage_hash,
            "Escape and Backspace must leave the carriage and preserve the campaign.");
        local.carriage_tab = CARRIAGE_OVERVIEW;
        queued_key_press[KEY_F7] = true;
        CARD_INPUT();
        CARD_CHECK(view == VIEW_LOCAL && local.carriage_tab == CARRIAGE_OVERVIEW,
            "The Ponies shortcut belongs to the carriage interface.");
        queued_key_press[KEY_B] = true;
        CARD_INPUT();
        CARD_CHECK(view == VIEW_LEDGER && return_view == VIEW_LOCAL,
            "B must open the Company Book from the road.");
        for (int32_t page = 0; page < 4; ++page) {
            local.book_offset = 2;
            queued_key_press[KEY_ONE + page] = true;
            CARD_INPUT();
            CARD_CHECK(view == VIEW_LEDGER && local.book_page == page && local.book_offset == 0,
                "Keys 1 through 4 must open each book page at the start.");
        }
        queued_key_press[KEY_FIVE] = true;
        CARD_INPUT();
        CARD_CHECK(view == VIEW_LEDGER && local.book_page == 3 && local.book_offset == 0 &&
            local.carriage_tab == CARRIAGE_OVERVIEW,
            "The book must keep the Journal selected when 5 is pressed.");
        queued_key_press[KEY_B] = true;
        CARD_INPUT();
        CARD_CHECK(view == VIEW_LOCAL && CcSimHash(&sim) == carriage_hash,
            "Closing the Company Book must restore the road and preserve the campaign.");
        client_test_pointer_active = false;
        view = VIEW_LOCAL;
        BuildAdventureTargets(&sim, &local, (RenderTexture2D){0}, (Rectangle){0});
        ContextActionSet cards = BuildContextActions(&sim, &local, view, selected, 0);
        CARD_CHECK(local.interactions.count > 4 && cards.count == 4,
            "Town controls show the four closest objects.");
        CARD_CHECK(ContextCardsPerPage() == 4, "The tray must show four choices.");
        float last_distance = -1;
        for (int32_t i = 0; i < cards.count; ++i) {
            const CcInteractionTarget *target = CcInteractionFind(&local.interactions, cards.items[i].target);
            CARD_CHECK(target != NULL, "Each card must keep its world identity.");
            float distance = GridDistance(LocalPosition(&local), (Vector2){target->approach_x, target->approach_z});
            CARD_CHECK(distance >= last_distance, "The closest objects must come first.");
            last_distance = distance;
        }
        uint64_t before = CcSimHash(&sim);
        CcInteractionTarget *unavailable = NULL;
        for (int32_t i = 0; i < local.interactions.count; ++i)
            if (CcInteractionKeyEqual(local.interactions.targets[i].key, cards.items[0].target))
                unavailable = &local.interactions.targets[i];
        CARD_CHECK(unavailable != NULL, "The nearest card needs its scene target.");
        unavailable->available = false;
        (void)snprintf(unavailable->reason, sizeof(unavailable->reason), "Choose a clear approach.");
        cards = BuildContextActions(&sim, &local, view, selected, 0);
        Rectangle disabled_card = ContextActionBounds(0, ContextCardCount(cards.count, 0));
        client_test_pointer_active = true;
        client_test_pointer = (Vector2){disabled_card.x + 12, disabled_card.y + 12};
        queued_mouse_button_press[MOUSE_BUTTON_LEFT] = true;
        CARD_INPUT();
        CARD_CHECK(strstr(message, "Double-tap [N]") != NULL && CcSimHash(&sim) == before,
            "A denied card click must show the caravan recovery tip and preserve the campaign.");
        unavailable->available = true;
        message[0] = '\0';

        CARD_CHECK(cards.count == 4 && ContextCardCount(cards.count, 0) == 4,
            "Town controls must show the nearest four options together.");
        /* Card identity remains valid when the object is outside the camera. */
        CcInteractionTarget *door = NULL;
        for (int32_t i = 0; i < local.interactions.count; ++i)
            if (local.interactions.targets[i].key.kind == CC_INTERACTION_DOOR) door = &local.interactions.targets[i];
        CARD_CHECK(door != NULL, "Town needs a door.");
        RepositionHero(&local, (Vector2){door->approach_x, door->approach_z}, false);
        cards = BuildContextActions(&sim, &local, view, selected, 0);
        int32_t door_card = -1;
        for (int32_t i = 0; i < cards.count; ++i)
            if (CcInteractionKeyEqual(cards.items[i].target, door->key)) door_card = i;
        CARD_CHECK(door_card >= 0, "Door needs a card.");
        if (mode == 0) {
            local.card_page = door_card / ContextCardsPerPage();
            int32_t first = ContextCardFirst(&local, cards.count);
            Rectangle bounds = ContextActionBounds(door_card - first, ContextCardCount(cards.count, first));
            client_test_pointer = (Vector2){bounds.x + 20, bounds.y + 25};
        } else {
            door->visible = true; door->camera_distance = 8;
            door->left = 490; door->right = 510; door->top = 290; door->bottom = 310;
            client_test_pointer = (Vector2){500, 300};
        }
        queued_mouse_button_press[MOUSE_BUTTON_LEFT] = true;
        CARD_INPUT();
        CARD_CHECK(local.market_interior && !local.interaction.approaching,
            "The door card and world tap must both enter the shop.");
        for (int32_t site_kind = CC_LOCAL_SITE_DUNGEON;
             site_kind <= CC_LOCAL_SITE_GOBLIN_CAVE; ++site_kind) {
            EnterSiteFromGoblinTunnel(&local, (CcLocalSiteKind)site_kind,
                (Vector2){CC_LOCAL_SITE_ENTRANCE_X, CC_LOCAL_SITE_ENTRANCE_Z});
            before = CcSimHash(&sim);
            CARD_CHECK(WorldCardPoint(&sim, &local, view, selected,
                CONTEXT_ACTION_RETURN_FROM_SITE, 0, mode != 0),
                "Each site needs a return card and a carriage tap at its entrance.");
            CARD_INPUT();
            CARD_CHECK(view == VIEW_LOCAL && local.site_travel_active && local.site_returning,
                "The return card and carriage tap must start the drive back from the site entrance.");
            for (int32_t frame = 0; frame < 1200 && local.site_travel_active; ++frame) CARD_INPUT();
            CARD_CHECK(!local.site_travel_active && local.site_kind == CC_LOCAL_SITE_NONE &&
                local.agent.scene == CC_LOCAL_SCENE_STREET && AdventureScene(&local),
                "Returning from a site must restore the playable town.");
            CARD_CHECK(CcSimHash(&sim) == before,
                "A local site return must preserve the campaign and cargo.");
        }
        /* A denied accept click must explain itself instead of doing nothing. */
        view = VIEW_SITUATIONS;
        return_view = VIEW_LOCAL;
        selected_situation = FirstActiveSituationIndex(&sim);
        CARD_CHECK(selected_situation >= 0, "The campaign needs an opening promise.");
        ContextActionSet paper_cards = BuildContextActions(
            &sim, &local, view, selected, selected_situation);
        int32_t accept_card = WorldCardIndex(
            &paper_cards, CONTEXT_ACTION_ACCEPT_PROMISE, 0);
        CARD_CHECK(accept_card >= 0, "The papers must show the accept card.");
        RepositionHero(&local, (Vector2){CC_LOCAL_NOTICE_X + 9.0f, CC_LOCAL_NOTICE_Z}, false);
        paper_cards = BuildContextActions(
            &sim, &local, view, selected, selected_situation);
        accept_card = WorldCardIndex(&paper_cards, CONTEXT_ACTION_ACCEPT_PROMISE, 0);
        CARD_CHECK(accept_card >= 0 && !paper_cards.items[accept_card].enabled,
            "Accepting must wait at the notice board.");
        before = CcSimHash(&sim);
        CARD_CHECK(WorldCardPoint(&sim, &local, view, selected,
            CONTEXT_ACTION_ACCEPT_PROMISE, 0, false),
            "The papers need a clickable accept card.");
        message[0] = '\0';
        CARD_INPUT();
        CARD_CHECK(strstr(message, "Visit the local board") != NULL &&
            CcSimHash(&sim) == before,
            "A denied accept must point to the board and preserve the campaign.");
        message[0] = '\0';
        ResetLocalState(&local);
        local.adventure_ui = true;
        CARD_CHECK(InitializeOpenWorld(&sim, &local, false), "World must initialize.");
        selected = FirstOutgoingRouteIndex(&sim);
        const CcRoute *route = SelectedOutgoingRoute(&sim, selected);
        CARD_CHECK(route != NULL, "Town needs an outgoing road.");
        CARD_CHECK(EnterOpenWorldAtRoadGate(&sim, &local, route->id), "Carriage must reach the gate.");
        local.road_choice_active = true;
        local.departure = (CcClientDepartureTransition){.phase = CC_CLIENT_DEPARTURE_READY,
            .town_progress = 1, .road_book_progress = 1};
        view = VIEW_ROADS;
        int32_t chosen_route = StepOutgoingRouteIndex(&sim, selected, 1);
        CcId destination = RouteOtherEnd(SelectedOutgoingRoute(&sim, chosen_route), sim.player.location_id);
        CARD_CHECK(WorldCardPoint(&sim, &local, view, selected, CONTEXT_ACTION_TRAVEL, chosen_route, mode != 0),
            "Every outgoing road needs matching card and world actions.");
        CARD_INPUT();
        CARD_CHECK(sim.journey.active && sim.journey.destination_id == destination && view == VIEW_LOCAL,
            "A road tap and its card must travel to the same destination.");
        CARD_CHECK(WorldCardPoint(&sim, &local, view, selected, CONTEXT_ACTION_TOGGLE_DRIVE, 0, mode != 0),
            "Travel needs a carriage stop action.");
        CARD_INPUT();
        int32_t elapsed = sim.journey.elapsed_subticks;
        for (int32_t frame = 0; frame < 120; ++frame) CARD_INPUT();
        CARD_CHECK(local.carriage_stopped && local.convoy.pace == 0 && sim.journey.elapsed_subticks == elapsed,
            "A stop must hold the carriage across later frames.");
        CARD_CHECK(WorldCardPoint(&sim, &local, view, selected, CONTEXT_ACTION_TOGGLE_DRIVE, 0, mode != 0),
            "A stopped carriage needs a drive action.");
        CARD_INPUT();
        for (int32_t frame = 0; frame < 30; ++frame) CARD_INPUT();
        CARD_CHECK(!local.carriage_stopped && sim.journey.elapsed_subticks > elapsed,
            "Drive must resume journey progress.");
        client_test_pointer_active = false;
        queued_key_press[KEY_DOWN] = true;
        CARD_INPUT();
        CARD_CHECK(sim.journey.pace == CC_JOURNEY_PACE_CAREFUL, "The pace shortcut must set the journey posture.");
        /* Put this journalled journey at one of its road stops. The simulation
           road-stop suite covers natural arrival, both directions and replay. */
        const CcRoadSite *site = NULL;
        for (int32_t i = 0; i < sim.road_site_count; ++i)
            if (sim.road_sites[i].route_id == sim.journey.route_id) { site = &sim.road_sites[i]; break; }
        CARD_CHECK(site != NULL, "Fixture route needs a camp stop.");
        const CcRoute *travelling = CcSimRoute(&sim, sim.journey.route_id);
        sim.carriage.progress_milli = travelling->from_id == sim.journey.origin_id ?
            site->progress_milli : 1000 - site->progress_milli;
        sim.journey.elapsed_subticks = (int32_t)((int64_t)sim.journey.total_subticks * sim.carriage.progress_milli / 1000);
        for (int32_t i = 0; i < CC_PONY_COUNT; ++i) sim.pony_company.ponies[i].last_met_day = sim.current_day;
        CARD_CHECK(CcSimJourneyRoadSiteStop(&sim) == site, "Fixture must reach the site stop.");
        CARD_CHECK(CcJournalCheckpoint(journal, &sim, error, sizeof(error)), error);
        CARD_CHECK(WorldCardPoint(&sim, &local, view, selected, CONTEXT_ACTION_CAMP_ROAD_SITE, 0, mode != 0),
            "A camp needs matching card and world actions.");
        if (mode != 0) {
            CcInteractionTarget *camp = &local.interactions.targets[0];
            uint64_t subject = camp->subject_id;
            camp->subject_id = subject + 1;
            ContextActionSet current = BuildContextActions(&sim, &local, view, selected, 0);
            CARD_CHECK(WorldContextActionAt(&sim, &local, view, &current, client_test_pointer).kind == CONTEXT_ACTION_NONE,
                "A camp tap must keep the identity of the camp that was drawn.");
            camp->subject_id = subject;
        }
        int32_t progress = sim.carriage.progress_milli;
        CARD_INPUT();
        CARD_CHECK(CcSimJourneyRoadSiteStop(&sim) == NULL && sim.carriage.progress_milli == progress,
            "Camping through either control must rest at the same road position.");
        results[mode] = CcSimHash(&sim);
        CARD_CHECK(CcJournalCheckpoint(journal, &sim, error, sizeof(error)), error);
        CARD_CHECK(CcJournalClose(&journal, &sim, error, sizeof(error)), error);
        journal = CcJournalResume(path, &sim, error, sizeof(error));
        CARD_CHECK(journal != NULL && CcSimHash(&sim) == results[mode], "Card and tap results must survive reload.");
        CARD_CHECK(CcJournalClose(&journal, &sim, error, sizeof(error)), error);
    }
    CARD_CHECK(results[0] == results[1], "Cards and world taps must produce identical campaign state.");
    client_test_pointer_active = false;
    (void)remove(path);
#undef CARD_CHECK
#undef CARD_INPUT
    puts("World cards: nearest four, doors, site returns, roads, stop, drive, pace, camp and reload passed");
    return 0;
}
