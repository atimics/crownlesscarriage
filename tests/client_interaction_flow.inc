static int RunAdventureInputRegression(void)
{
    static CcSim sim;
    static LocalState local;
    char message[256] = "", feedback[256] = "", error[192] = "";
    float feedback_age = 0;
    int32_t selected = 0, selected_situation = 0;
    ClientView view = VIEW_LOCAL, return_view = VIEW_LOCAL;
    const char *path = "interaction-input.ccsave";
    (void)remove(path);
    CcSimInit(&sim, 42U);
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
#undef TERMS_CHECK
    puts("adventure trade terms passed");
    return 0;
}
