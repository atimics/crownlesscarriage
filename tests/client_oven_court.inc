/* Synthetic ordinary-control path from a staged Silverwick arrival.
 * No production, stock transfer or evidence flag is set by the input driver. */
static int RunOvenCourtInputRegression(void)
{
    static CcSim sim;
    static LocalState local;
    char error[256] = "", message[256] = "", feedback[256] = "";
    float feedback_age = 0.0f;
    int32_t selected = 0, selected_situation = 0;
    ClientView view = VIEW_LOCAL, return_view = VIEW_LOCAL;
    const char *path = "oven-court-input.ccsave";
    (void)remove(path);
    CcSimInit(&sim, 42U);
    CcSettlement *town = CcSimSettlementMutable(&sim, CcOvenCourtPlace(&sim)->id);
    sim.player.location_id = town->id;
    sim.carriage.location_id = town->id;
    sim.player.accepted_situation_id = 0U;
    town->service_mask |= UINT32_C(1) << CC_SERVICE_BAKERY;
    town->production[CC_GOOD_BREAD] = 12;
    town->stock[CC_GOOD_BREAD] = 0;
    town->stock[CC_GOOD_WHEAT] = 0;
    town->market_coins = 400;
    sim.current_day = 6;
    for (int32_t g = 0; g < CC_GOOD_COUNT; ++g) sim.player.cargo[g] = 0;
    sim.player.cargo[CC_GOOD_WHEAT] = 4;
    CcLocalBindPlace(&sim);
    ResetLocalState(&local);
    local.adventure_ui = true;
    local.opening_step = CC_LOCAL_OPENING_COMPLETE;
    local.course.automatic_alarm = false;
    CcLocalLanePoint anchor = CcLocalOvenCourtAnchor(CcLocalPlaceProfileForSettlement(town));
    RepositionHero(&local, (Vector2){anchor.x, anchor.z + 1.1f}, false);
    CcJournal *journal = CcJournalStart(path, &sim, error, sizeof(error));
    if (journal == NULL) return ClientRegressionFailure(error);
#define OVEN_CHECK(test, text) do { if (!(test)) { CcJournalAbandon(&journal); (void)remove(path); return ClientRegressionFailure(text); } } while (0)
#define OVEN_FRAME() do { HandleInput(&journal, &sim, &selected, &selected_situation, &view, &return_view, &local, (RenderTexture2D){0}, (Rectangle){0}, 1.0f/60.0f, path, "", message, sizeof(message), feedback, sizeof(feedback), &feedback_age); ClientInputClearPressed(); } while (0)
#define OVEN_KEY(key) do { queued_key_press[key] = true; OVEN_FRAME(); } while (0)
    BuildAdventureTargets(&sim, &local, (RenderTexture2D){0}, (Rectangle){0});
    CcInteractionTarget *target = NULL;
    for (int32_t i = 0; i < local.interactions.count; ++i)
        if (local.interactions.targets[i].key.kind == CC_INTERACTION_OVEN_COURT) target = &local.interactions.targets[i];
    OVEN_CHECK(target != NULL && target->available && fabsf(target->x - anchor.x) < 0.01f,
        "Court target must share its rendered wall anchor.");
    target->visible = true; /* No graphics context in this input-only regression. */
    local.interaction.focus = target->key;
    OVEN_KEY(KEY_F);
    OVEN_CHECK(view == VIEW_OVEN_COURT && CcOvenCourtNote(&sim, 0) != NULL,
        "F at the tally must acquire a note through the journal and open inspection.");
    OVEN_CHECK(town->stock[CC_GOOD_BREAD] == 0 && sim.player.cargo[CC_GOOD_WHEAT] == 4 && sim.current_day == 6,
        "Opening the tally must not bake, transfer cargo or advance time.");
    CcId first = CcOvenCourtNote(&sim, 0)->id;
    OVEN_KEY(KEY_TWO);
    OVEN_CHECK(local.oven_page == 1 && CcOvenCourtNote(&sim, 0)->id == first,
        "Reading advice cannot create more evidence.");
    OVEN_KEY(KEY_B);
    OVEN_CHECK(view == VIEW_LEDGER && local.book_page == 3, "B must open dated court notes.");
    OVEN_KEY(KEY_ESCAPE);
    OVEN_CHECK(view == VIEW_OVEN_COURT, "Back must restore the court, not enter trade.");
    OVEN_KEY(KEY_THREE);
    OVEN_CHECK(view == VIEW_LOCAL && local.interaction.approaching && !local.market_interior,
        "Store action must start a real approach, not teleport to a trade menu.");
    for (int32_t frame = 0; frame < 1800 && !local.market_interior; ++frame) {
        BuildAdventureTargets(&sim, &local, (RenderTexture2D){0}, (Rectangle){0});
        OVEN_FRAME();
    }
    OVEN_CHECK(local.market_interior, "Walking from the tally must enter the actual service door.");
    BuildAdventureTargets(&sim, &local, (RenderTexture2D){0}, (Rectangle){0});
    target = NULL;
    for (int32_t i = 0; i < local.interactions.count; ++i)
        if (local.interactions.targets[i].key.kind == CC_INTERACTION_COUNTER) target = &local.interactions.targets[i];
    OVEN_CHECK(target != NULL, "Interior keeps its real counter.");
    target->visible = true; local.interaction.focus = target->key;
    OVEN_KEY(KEY_F);
    for (int32_t frame = 0; frame < 1800 && view == VIEW_LOCAL; ++frame) {
        BuildAdventureTargets(&sim, &local, (RenderTexture2D){0}, (Rectangle){0});
        OVEN_FRAME();
    }
    OVEN_CHECK(view == VIEW_TRADE, "Approaching the counter must open ordinary trade.");
    OVEN_KEY(KEY_TWO); /* Existing Sell tab. */
    OVEN_CHECK(local.trade_mode == 1, "Use the existing Sell tab.");
    local.trade_good = CC_GOOD_WHEAT; local.trade_quantity = 4;
    AdventureQuote quote = AdventureTradeQuote(&sim, &local);
    OVEN_CHECK(quote.available, "The ordinary grain sale must be funded and available.");
    local.trade_quote_presented = true;
    local.trade_presented_command = quote.command;
    local.trade_presented_total = quote.total;
    local.trade_presented_reward = quote.reward;
    local.trade_presented_promise_id = quote.promise_id;
    local.trade_presented_shop_building_index = local.shop_building_index;
    CcMoney coins = CcSimTrackedGold(&sim);
    int32_t wheat = CcSimTrackedGood(&sim, CC_GOOD_WHEAT);
    OVEN_KEY(KEY_ENTER);
    OVEN_CHECK(sim.player.cargo[CC_GOOD_WHEAT] == 0 && town->stock[CC_GOOD_WHEAT] == 4 &&
        CcSimTrackedGold(&sim) == coins && CcSimTrackedGood(&sim, CC_GOOD_WHEAT) == wheat,
        "Store confirmation must transfer real supplies once.");
    OVEN_KEY(KEY_ENTER);
    OVEN_CHECK(town->stock[CC_GOOD_WHEAT] == 4, "Repeated confirmation cannot duplicate a delivery.");
    OVEN_KEY(KEY_B);
    OVEN_KEY(KEY_FOUR);
    OVEN_CHECK(view == VIEW_LEDGER && local.book_page == 3 && CcOvenCourtNote(&sim, 0)->id == first &&
        strstr(CcOvenCourtNote(&sim, 0)->text, "0 Wheat") != NULL,
        "Selling cannot silently refresh a dated field note.");
    /* Exercise an actual eligible contact through the normal character reply path. */
    local.conversation_character_id = CcSimBakerySupportPlan(&sim, town->id).contact_id;
    local.conversation_situation_id = 0U;
    view = VIEW_CHARACTER;
    OVEN_CHECK(CcOvenCourtCanDiscuss(&sim, local.conversation_character_id), "Question fixture needs the present bakery receiver.");
    OVEN_KEY(KEY_ONE);
    OVEN_CHECK(local.conversation_oven_response && CcOvenCourtNote(&sim, 0)->actor_id == local.conversation_character_id &&
        strstr(local.conversation_line, "not happened yet") != NULL,
        "A present worker can explain available grain without claiming completed baking.");
    OVEN_CHECK(CcJournalClose(&journal, &sim, error, sizeof(error)), "Save the completed input path.");
    (void)remove(path);
#undef OVEN_KEY
#undef OVEN_FRAME
#undef OVEN_CHECK
    puts("Oven Court input: inspect, Book, walk to door/counter, funded sale, remembered note and grounded question.");
    return 0;
}
