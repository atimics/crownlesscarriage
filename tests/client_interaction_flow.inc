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
    ResetLocalState(&local);
    local.adventure_ui = true;
    CcLocalBindPlace(&sim);
    CcJournal *journal = CcJournalStart(path, &sim, error, sizeof(error));
    if (journal == NULL) return ClientRegressionFailure(error);
#define UX_CHECK(condition, text) do { if (!(condition)) { (void)CcJournalClose(&journal, &sim, error, sizeof(error)); (void)remove(path); return ClientRegressionFailure(text); } } while (0)
#define UX_KEY(key) do { queued_key_press[key] = true; \
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
    UX_KEY(KEY_ESCAPE);
    UX_CHECK(view == VIEW_LOCAL, "Conversation back must restore the scene.");
    local.course.alarm_active = false;
    target->key.kind = CC_INTERACTION_DOOR;
    local.interaction.focus = target->key;
    UX_KEY(KEY_F);
    UX_CHECK(local.market_interior && view == VIEW_LOCAL, "Door action must enter its interior.");
    local.interactions.count = 0;
    target = AdventureTarget(&local, &sim, CC_INTERACTION_COUNTER, 1,
        "Keeper", "Trade", local.agent.position, 2, 2, 1, LocalPosition(&local), 1);
    target->visible = true;
    local.interaction.focus = target->key;
    UX_KEY(KEY_F);
    UX_CHECK(view == VIEW_TRADE, "Counter action must open explicit trade.");
    local.trade_good = CC_GOOD_FOOD;
    local.trade_quantity = 1;
    AdventureQuote quote = AdventureTradeQuote(&sim, &local);
    UX_CHECK(quote.available, "Initial market must quote one food unit.");
    CcMoney before = sim.player.coins;
    int32_t cargo = sim.player.cargo[CC_GOOD_FOOD];
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
