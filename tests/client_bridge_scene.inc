/* Click the same rectangles that the bridge draws, through HandleInput. */
static int RunBridgeSceneRegression(void)
{
    static CcSim sim;
    static LocalState local;
    CcJournal *journal = NULL;
    ClientView view = VIEW_LOCAL, return_view = VIEW_LOCAL;
    int32_t selected = 0, selected_situation = 0;
    char message[256] = "", feedback[256] = "", error[192] = "";
    float feedback_age = 0.0f;
    const char *path = "bridge-input.ccsave";
    (void)remove(path);
    CcSimInit(&sim, 42U);
    CcRoute *route = &sim.routes[0];
    route->closed = false;
    sim.player.location_id = route->from_id;
    sim.carriage.location_id = route->from_id;
    CcCommand travel = {.kind = CC_COMMAND_TRAVEL, .target_id = route->to_id};
    if (!CcSimApply(&sim, &travel, error, sizeof(error))) return ClientRegressionFailure(error);
    sim.journey.phase = CC_JOURNEY_PHASE_BLOCKED;
    sim.journey.encounter_triggered = true;
    sim.clock.game_minutes_per_second = CC_IDLE_GAME_MINUTES_PER_SECOND;
    sim.carriage.mode = CC_CARRIAGE_STOPPED;
    sim.carriage.speed_milli_per_second = 0;
    CcLocalBindPlace(&sim);
    ResetLocalState(&local);
    BeginRoadLocalState(&sim, &local, true);
    local.adventure_ui = true;
    local.opening_step = CC_LOCAL_OPENING_COMPLETE;
#define BRIDGE_CHECK(test, reason) do { if (!(test)) { \
    client_test_pointer_active = false; ClientInputClearPressed(); \
    if (journal != NULL) (void)CcJournalClose(&journal, &sim, error, sizeof(error)); \
    (void)remove(path); return ClientRegressionFailure(reason); } } while (0)
#define BRIDGE_CLICK(kind, amount) do { \
    BRIDGE_CHECK(WorldCardPoint(&sim, &local, view, selected, kind, amount, false), "Bridge card must exist."); \
    HandleInput(&journal, &sim, &selected, &selected_situation, &view, &return_view, &local, \
        (RenderTexture2D){0}, (Rectangle){0}, 1.0f / 60.0f, path, "", message, sizeof(message), \
        feedback, sizeof(feedback), &feedback_age); ClientInputClearPressed(); } while (0)
    journal = CcJournalStart(path, &sim, error, sizeof(error));
    BRIDGE_CHECK(journal != NULL, error);
    BRIDGE_CHECK(CcLocalRoadEncounterCarriageXInternal() +
        CcLocalRoadHorseLongitudinalOffsetInternal() + 1.0f < local.agent.position.x,
        "The horse team must stop behind the hero.");
    ContextActionSet before = BuildContextActions(&sim, &local, view, selected, 0);
    BRIDGE_CHECK(before.combat && before.count == 8 && ContextCardCount(&before, 0) == 8,
        "A wide bridge view must show all eight controls.");
    for (int32_t i = 0; i < before.count; ++i) {
        Rectangle a = ContextActionBounds(i, before.count, before.combat);
        BRIDGE_CHECK(a.x >= 0 && a.y >= 0 && a.x + a.width <= ContextViewportWidth() &&
            a.y + a.height <= ContextViewportHeight(), "Bridge controls must fit the viewport.");
        for (int32_t j = 0; j < i; ++j) {
            Rectangle b = ContextActionBounds(j, before.count, before.combat);
            BRIDGE_CHECK(!CheckCollisionRecs(a, b), "Bridge controls need separate click areas.");
        }
    }
    BRIDGE_CLICK(CONTEXT_ACTION_SELECT_TARGET, 0);
    BRIDGE_CHECK(local.agent.combat.target_index == 0, "The first bandit card must select its bandit.");
    BRIDGE_CLICK(CONTEXT_ACTION_SELECT_TARGET, 1);
    BRIDGE_CHECK(local.agent.combat.target_index == 1, "The second bandit card must select its bandit.");
    BRIDGE_CLICK(CONTEXT_ACTION_BASIC_STRIKE, 0);
    BRIDGE_CHECK(local.agent.combat.focus_valid, "Attack must keep the chosen opponent while approaching.");
    BRIDGE_CLICK(CONTEXT_ACTION_TOGGLE_GUARD, 0);
    BRIDGE_CHECK(local.agent.humanoid.guard_requested, "Guard click must raise guard.");
    BRIDGE_CLICK(CONTEXT_ACTION_TOGGLE_GUARD, 0);
    BRIDGE_CHECK(!local.agent.humanoid.guard_requested, "A second guard click must lower guard.");
    BRIDGE_CLICK(CONTEXT_ACTION_SKILL_CRUSHING, 0);
    BRIDGE_CHECK(local.agent.combat.queued_skill == CC_COMBAT_SKILL_CRUSHING_BLOW,
        "Crushing blow must queue from its card.");
    BRIDGE_CLICK(CONTEXT_ACTION_SKILL_SUNDER, 0);
    BRIDGE_CHECK(local.agent.combat.queued_skill == CC_COMBAT_SKILL_SUNDER,
        "Sunder must queue from its card.");
    local.agent.combat.posture = 30.0f;
    BRIDGE_CLICK(CONTEXT_ACTION_SKILL_SECOND_WIND, 0);
    BRIDGE_CHECK(local.agent.combat.posture > 30.0f &&
        local.agent.combat.skill_cooldown[CC_COMBAT_SKILL_SECOND_WIND] > 0.0f,
        "Second wind must restore posture and start its cooldown.");
    float posture = local.agent.combat.posture;
    BRIDGE_CLICK(CONTEXT_ACTION_SKILL_SECOND_WIND, 0);
    BRIDGE_CHECK(local.agent.combat.posture == posture, "A cooldown click must preserve posture.");
    local.course.raiders[1].combat.life_state = CC_LIFE_DEAD;
    ContextActionSet after = BuildContextActions(&sim, &local, view, selected, 0);
    BRIDGE_CHECK(after.count == before.count, "Defeat must preserve the combat control layout.");
    for (int32_t i = 0; i < before.count; ++i) {
        BRIDGE_CHECK(before.items[i].kind == after.items[i].kind &&
            before.items[i].amount == after.items[i].amount, "Each combat slot must keep its action.");
    }
    BRIDGE_CHECK(!after.items[1].enabled, "The fallen bandit's card must show its state.");
    BeginRoadLocalState(&sim, &local, false);
    local.adventure_ui = true;
    BRIDGE_CLICK(CONTEXT_ACTION_APPROACH_COLLECTOR, 0);
    BRIDGE_CHECK(local.agent.exact_target_valid, "Approach captain must start a walk.");
    for (int32_t frame = 0; frame < 900 && local.agent.exact_target_valid; ++frame)
        CcLocalAgentUpdate(&local.agent, 1.0f / 60.0f, false);
    BRIDGE_CHECK(GridDistance(LocalPosition(&local),
        (Vector2){CC_LOCAL_ROAD_PARLEY_X, CC_LOCAL_ROAD_PARLEY_Z}) < 1.55f,
        "The captain's approach must reach speaking distance.");
    BRIDGE_CLICK(CONTEXT_ACTION_RETURN_TO_CHOICE, 0);
    BRIDGE_CHECK(view == VIEW_ENCOUNTER, "Return to carriage must restore the encounter choice.");
    BeginRoadLocalState(&sim, &local, true);
    view = VIEW_LOCAL;
    BRIDGE_CLICK(CONTEXT_ACTION_WITHDRAW, 0);
    BRIDGE_CHECK(!sim.journey.active && !local.journey_combat_active && view == VIEW_LOCAL,
        "Withdraw must return the company to town.");
    BRIDGE_CHECK(CcJournalClose(&journal, &sim, error, sizeof(error)), error);
    client_test_pointer_active = false;
    (void)remove(path);
#undef BRIDGE_CLICK
#undef BRIDGE_CHECK
    puts("Bridge placement, target clicks, attack, guard, skills, cooldown and stable controls passed");
    return 0;
}
