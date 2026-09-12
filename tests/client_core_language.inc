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
        if (carrier == NULL) continue;
        for (int32_t slot = 0; slot < CC_MAX_GOSSIP; ++slot) {
            const CcGossip *story = CcSimGossipStory(&sim, slot);
            if ((carrier->stories & (UINT32_C(1) << (uint32_t)slot)) == 0U ||
                story == NULL || story->kind != CC_EVENT_HARVEST_FAILED) continue;
            local.conversation_character_id = sim.characters[i].id;
            local.conversation_gossip_slot = slot; found = true; break;
        }
    }
    MODEL_CHECK(found, "Find a held harvest account in the simulated world.");
    uint64_t before = CcSimHash(&sim);
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
    MODEL_CHECK(actions.count == 4 && actions.items[1].kind == CONTEXT_ACTION_GOSSIP_CERTAINTY,
                "The conversation offers its confidence question.");
    (void)remove(journal_path);
    journal = CcJournalStart(journal_path, &sim, journal_error, sizeof(journal_error));
    MODEL_CHECK(journal != NULL, "Start the test campaign journal.");
    int32_t selected = 0, selected_situation = 0;
    ClientView view = VIEW_CHARACTER, return_view = VIEW_LOCAL;
    char message[256] = "", feedback[256] = "";
    float feedback_age = 0.0f;
    queued_key_press[KEY_TWO] = true;
    HandleInput(&journal, &sim, &selected, &selected_situation, &view, &return_view, &local,
        (RenderTexture2D){0}, (Rectangle){0}, 1.0f / 60.0f, "", "", message, sizeof(message),
        feedback, sizeof(feedback), &feedback_age);
    ClientInputClearPressed();
    MODEL_CHECK(core_conversation.pending, "The real question key starts a new model reply.");
    MODEL_CHECK(strcmp(core_conversation.history[core_conversation.count - 1U].text, "How sure are you?") == 0,
                "The question becomes the next spoken event.");
    CcCoreConversationStep(&core_conversation, 1000U);
    MODEL_CHECK(ClientConversationSpeech(&sim, &local, &speech), "Show the response to the question.");
    MODEL_CHECK(strcmp(speech.line_id, "gossip.core") == 0 && speech.audio_key != key,
                "The model answers the question with a new line.");
    MODEL_CHECK(CcSimHash(&sim) == before, "The conversation preserves the simulation.");
    (void)printf("PASS core game conversation: %s\n", speech.text);
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
