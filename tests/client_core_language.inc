/* Exercise the real conversation view and its keyboard action. */
static int RunCoreLanguageRegression(const char *model_path)
{
    static CcSim sim;
    static LocalState local;
    CcSimInit(&sim, 73U); CcSimAdvanceDays(&sim, 20);
    ResetLocalState(&local); local.adventure_ui = true;
    core_conversation.model = CcCoreModelLoad(model_path);
    if (core_conversation.model == NULL) return ClientRegressionFailure("Load the packaged core model.");
    int result = 1;
    CcJournal *journal = NULL;
    char journal_error[192] = "";
    const char *journal_path = "core-language-test.ccsave";
#define MODEL_CHECK(condition, message) do { if (!(condition)) { (void)ClientRegressionFailure(message); goto finished; } } while (0)
    bool found = false;
    for (int32_t i = 0; i < sim.character_count && !found; ++i) {
        const CcGossipCarrier *carrier = CcSimGossipCarrier(&sim, sim.characters[i].id);
        if (carrier == NULL || sim.characters[i].activity == CC_CHARACTER_ACTIVITY_TRAVELLING ||
            sim.characters[i].current_settlement_id != sim.player.location_id ||
            CcCharacterAgeYears(&sim, &sim.characters[i]) < 16) continue;
        for (int32_t slot = 0; slot < CC_MAX_GOSSIP; ++slot) {
            const CcGossip *story = CcSimGossipStory(&sim, slot);
            if ((carrier->stories & (UINT32_C(1) << (uint32_t)slot)) == 0U ||
                story == NULL || story->kind != CC_EVENT_HARVEST_FAILED) continue;
            local.conversation_character_id = sim.characters[i].id;
            local.conversation_gossip_slot = slot; found = true; break;
        }
    }
    MODEL_CHECK(found, "Find a held harvest account in the simulated world.");
    CcSpeech speech;
    MODEL_CHECK(ClientConversationSpeech(&sim, &local, &speech), "Prepare a model turn in the conversation view.");
    MODEL_CHECK(core_conversation.pending, "The view starts bounded generation.");
    CcCoreConversationStep(&core_conversation, 1000U);
    MODEL_CHECK(ClientConversationSpeech(&sim, &local, &speech), "Show the completed model turn.");
    MODEL_CHECK(strcmp(speech.line_id, "gossip.core") == 0, "The displayed words come from the model.");
    uint64_t key = speech.audio_key;
    size_t history_count = core_conversation.count;
    for (int i = 0; i < 5; ++i) {
        MODEL_CHECK(ClientConversationSpeech(&sim, &local, &speech), "Redraw the same model reply.");
        MODEL_CHECK(speech.audio_key == key && core_conversation.count == history_count,
                    "Redraw preserves words and spoken history.");
    }
    ContextActionSet actions = BuildContextActions(&sim, &local, VIEW_CHARACTER, 0, 0);
    MODEL_CHECK(actions.count == 2 && actions.items[0].kind == CONTEXT_ACTION_GOSSIP_CHAT &&
                actions.items[1].kind == CONTEXT_ACTION_CLOSE_VIEW,
                "The conversation offers Chat and Farewell.");
    (void)remove(journal_path);
    journal = CcJournalStart(journal_path, &sim, journal_error, sizeof(journal_error));
    MODEL_CHECK(journal != NULL, "Start the test campaign journal.");
    int32_t selected = 0, selected_situation = 0;
    ClientView view = VIEW_CHARACTER, return_view = VIEW_LOCAL;
    char message[256] = "", feedback[256] = "";
    float feedback_age = 0.0f;
    queued_key_press[KEY_ONE] = true;
    HandleInput(&journal, &sim, &selected, &selected_situation, &view, &return_view, &local,
        (RenderTexture2D){0}, (Rectangle){0}, 1.0f / 60.0f, "", "", message, sizeof(message),
        feedback, sizeof(feedback), &feedback_age);
    ClientInputClearPressed();
    MODEL_CHECK(core_conversation.pending && core_conversation.round_phase == 1U,
                "Chat starts the player's model turn.");
    uint64_t after_exchange = CcSimHash(&sim);
    queued_key_press[KEY_ONE] = true;
    HandleInput(&journal, &sim, &selected, &selected_situation, &view, &return_view, &local,
        (RenderTexture2D){0}, (Rectangle){0}, 1.0f / 60.0f, "", "", message, sizeof(message),
        feedback, sizeof(feedback), &feedback_age);
    ClientInputClearPressed();
    MODEL_CHECK(CcSimHash(&sim) == after_exchange && core_conversation.round_phase == 1U,
                "A repeated Chat click preserves the running turn and its accounts.");
    CcCoreConversationStep(&core_conversation, 1000U);
    MODEL_CHECK(core_conversation.round_phase == 2U && core_conversation.pending,
                "The player line starts the NPC's model response.");
    MODEL_CHECK(ClientConversationSpeech(&sim, &local, &speech) && speech.speaker_id == sim.player.id &&
                strcmp(speech.line_id, "gossip.core") == 0, "Show the model's player line.");
    MODEL_CHECK(strcmp(core_conversation.history[core_conversation.count - 1U].text, speech.text) == 0,
                "The exact player line becomes the next spoken event.");
    (void)printf("You: %s\n", speech.text);
    actions = BuildContextActions(&sim, &local, VIEW_CHARACTER, 0, 0);
    MODEL_CHECK(actions.count == 2 && !actions.items[0].enabled && actions.items[1].enabled,
                "A running turn keeps Farewell available.");
    CcCoreConversationStep(&core_conversation, 1000U);
    MODEL_CHECK(core_conversation.round_phase == 3U, "The player line stays visible for reading.");
    CcCoreConversationAdvance(&core_conversation, 0U, 40.0f);
    MODEL_CHECK(ClientConversationSpeech(&sim, &local, &speech), "Show the NPC response.");
    MODEL_CHECK(core_conversation.round_phase == 0U && !core_conversation.pending &&
                strcmp(speech.line_id, "gossip.core") == 0 &&
                speech.speaker_id == local.conversation_character_id, "The NPC answers with the model.");
    MODEL_CHECK(CcSimHash(&sim) == after_exchange, "Generating both lines preserves the exchanged accounts.");
    (void)printf("%s: %s\n", speech.speaker, speech.text);
    key = speech.audio_key;
    for (int draw = 0; draw < 3; ++draw) {
        MODEL_CHECK(ClientConversationSpeech(&sim, &local, &speech) && speech.audio_key == key &&
            !core_conversation.pending, "Redraw keeps the completed Chat round.");
    }
    queued_key_press[KEY_ONE] = true;
    HandleInput(&journal, &sim, &selected, &selected_situation, &view, &return_view, &local,
        (RenderTexture2D){0}, (Rectangle){0}, 1.0f / 60.0f, "", "", message, sizeof(message),
        feedback, sizeof(feedback), &feedback_age);
    ClientInputClearPressed();
    MODEL_CHECK(core_conversation.round_phase == 1U, "A further Chat starts another pair.");
    queued_key_press[KEY_TWO] = true;
    HandleInput(&journal, &sim, &selected, &selected_situation, &view, &return_view, &local,
        (RenderTexture2D){0}, (Rectangle){0}, 1.0f / 60.0f, "", "", message, sizeof(message),
        feedback, sizeof(feedback), &feedback_age);
    ClientInputClearPressed();
    MODEL_CHECK(view == VIEW_LOCAL && !core_conversation.pending && core_conversation.count == 0U &&
                !core_conversation.round_shown, "Farewell clears an unfinished round and its history.");
    result = 0;
finished:
    if (journal != NULL) (void)CcJournalClose(&journal, &sim, journal_error, sizeof(journal_error));
    (void)remove(journal_path);
    CcCoreModelFree(core_conversation.model);
    core_conversation.model = NULL;
    CcCoreConversationReset(&core_conversation); core_conversation_speaker = 0U;
#undef MODEL_CHECK
    return result;
}
