#include "story/cc_core_conversation.h"
#include "test_support.h"
#include "story/cc_hrakhor.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    CC_CHECK(argc == 2);
    CcCoreConversation conversation = {0};
    conversation.model = CcCoreModelLoad(argv[1]);
    CC_CHECK(conversation.model != NULL);
    static CcSim sim;
    CcSimInit(&sim, 73U); CcSimAdvanceDays(&sim, 20);
    uint64_t before = CcSimHash(&sim);
    CcCoreAccount accounts[2]; CcSpeech original[2];
    int found = 0;
    for (int32_t i = 0; i < sim.character_count && found < 2; ++i) {
        const CcGossipCarrier *carrier = CcSimGossipCarrier(&sim, sim.characters[i].id);
        if (carrier == NULL) continue;
        for (int32_t slot = 0; slot < CC_MAX_GOSSIP && found < 2; ++slot) {
            if ((carrier->stories & (UINT32_C(1) << (uint32_t)slot)) == 0U) continue;
            const CcGossip *story = CcSimGossipStory(&sim, slot);
            if (story == NULL || story->kind != CC_EVENT_HARVEST_FAILED) continue;
            CcGossipLanguage language;
            if (!CcSpeechPrepareGossip(&sim, story, &carrier->versions[slot], 0U, &language) ||
                !CcCoreAccountPrepare(story->kind, language.account, language.confidence,
                    language.retellings, &accounts[found]) ||
                !CcSpeechStory(&sim, sim.characters[i].id, story, &carrier->versions[slot], false, &original[found])) continue;
            ++found; break;
        }
    }
    CC_CHECK(found == 2);
    for (int turn = 0; turn < 6; ++turn) {
        int person = turn % 2;
        CcSpeech speech = original[person];
        CC_CHECK(CcCoreConversationPrepare(&conversation, &accounts[person], &speech));
        CC_CHECK(conversation.pending);
        CC_CHECK(strcmp(speech.line_id, "gossip.thinking") == 0);
        int steps = 0;
        while (conversation.pending && steps++ < 400) CcCoreConversationStep(&conversation, 2U);
        CC_CHECK(!conversation.pending);
        speech = original[person];
        CC_CHECK(CcCoreConversationPrepare(&conversation, &accounts[person], &speech));
        CC_CHECK(strcmp(speech.line_id, "gossip.core") == 0);
        CC_CHECK(speech.speaker_id == original[person].speaker_id);
        CC_CHECK(speech.source_event_id == original[person].source_event_id);
        CC_CHECK(strcmp(conversation.history[conversation.count - 1U].text, speech.text) == 0);
        CC_CHECK(conversation.count == (size_t)(turn < 4 ? turn + 1 : 4));
        size_t count = conversation.count;
        for (int draw = 0; draw < 3; ++draw) {
            CcSpeech again = original[person];
            CC_CHECK(CcCoreConversationPrepare(&conversation, &accounts[person], &again));
            CC_CHECK(again.audio_key == speech.audio_key);
            CC_CHECK(conversation.count == count && !conversation.pending);
        }
        (void)printf("%s: %s\n", speech.speaker, speech.text);
    }
    CcCoreConversationReset(&conversation);
    CC_CHECK(CcCoreConversationStartRound(&conversation, &accounts[0], &original[0],
        &accounts[1], &original[1]));
    CC_CHECK(!CcCoreConversationStartRound(&conversation, &accounts[0], &original[0],
        &accounts[1], &original[1]));
    CcCoreConversationStep(&conversation, 1000U);
    CC_CHECK(conversation.round_phase == 2U && conversation.count == 1U);
    CC_CHECK(strcmp(conversation.history[0].text, conversation.player_line.text) == 0);
    CcSpeech shown;
    CC_CHECK(CcCoreConversationShown(&conversation, &shown));
    CC_CHECK(shown.speaker_id == original[0].speaker_id);
    CcCoreConversationStep(&conversation, 1000U);
    CC_CHECK(conversation.round_phase == 3U && conversation.count == 2U);
    CC_CHECK(CcCoreConversationShown(&conversation, &shown));
    CC_CHECK(shown.speaker_id == original[0].speaker_id);
    CcCoreConversationAdvance(&conversation, 0U, 40.0f);
    CC_CHECK(conversation.round_phase == 0U);
    CC_CHECK(CcCoreConversationShown(&conversation, &shown));
    CC_CHECK(shown.speaker_id == original[1].speaker_id);
    /* Independent generation proves the listener receives the generated player text.
       It is cued with the move the conversation chose for that turn, since the
       policy is part of what the model was asked to say. */
    char expected[CC_CORE_UTTERANCE];
    CC_CHECK(CcCoreModelGenerateMind(conversation.model, &accounts[1], original[1].speaker_id,
        conversation.history, 1U, NULL, conversation.last_move, expected, sizeof(expected)));
    CC_CHECK(strcmp(expected, shown.text) == 0);
    CC_CHECK(CcSimHash(&sim) == before);
    (void)printf("World hash retained: %016" PRIx64 "\n", before);
    CcCoreConversationReset(&conversation);
    CC_CHECK(conversation.count == 0U && !conversation.cached && !conversation.pending);
    CcSpeech goblin = original[0];
    CC_CHECK(CcSpeechCompose(&goblin, original[0].line_id, original[0].speaker_id,
        original[0].speaker, CC_SPEECH_GOBLIN_VOICE, original[0].text,
        original[0].delivery, original[0].priority, original[0].source_event_id));
    CC_CHECK(CcCoreConversationPrepare(&conversation, &accounts[0], &goblin));
    for (int step = 0; conversation.pending && step < 400; ++step)
        CcCoreConversationStep(&conversation, 2U);
    CC_CHECK(!conversation.pending);
    CC_CHECK(strcmp(conversation.reply.line_id, "gossip.core") == 0);
    char dialect[CC_SPEECH_TEXT_CAPACITY];
    CC_CHECK(CcHrakhorCorrupt(&accounts[0], CcCoreModelText(conversation.model),
        100U, dialect, sizeof(dialect)));
    CC_CHECK(strcmp(conversation.reply.text, dialect) == 0);
    CC_CHECK(conversation.reply.voice_index == CC_SPEECH_GOBLIN_VOICE);
    CC_CHECK(strcmp(conversation.history[0].text, CcCoreModelText(conversation.model)) == 0);
    CcCoreConversationReset(&conversation);
    CcCoreAccount bad = accounts[0]; bad.fields[0].start = SIZE_MAX;
    CC_CHECK(!CcCoreModelBegin(conversation.model, &bad, original[0].speaker_id, NULL, 0U));
    CC_CHECK(CcCoreModelText(conversation.model) == NULL);
    /* Memory sharing: the held memory that names what the conversation named,
       and nothing when the speaker is unwilling or nothing overlaps. */
    CcCoreMind mind = {0};
    mind.memories[0] = "Rosespire's mill uses 1 Wood to make 4 Paper.";
    mind.memories[1] = "A goblin rises to Keeper in the dragon cult through service.";
    mind.memory_count = 2;
    CcCoreSpoken talk[1] = {0};
    (void)snprintf(talk[0].text, sizeof(talk[0].text), "What of the mill at Rosespire?");
    CC_CHECK(CcCoreMemoryShare(&mind, talk, 1U, true) == mind.memories[0]);
    CC_CHECK(CcCoreMemoryShare(&mind, talk, 1U, false) == NULL);
    (void)snprintf(talk[0].text, sizeof(talk[0].text), "Tell me about the mill at Rosespire.");
    CC_CHECK(CcCoreMemoryShare(&mind, talk, 1U, true) == mind.memories[0]);
    (void)snprintf(talk[0].text, sizeof(talk[0].text), "Nothing here concerns any of it.");
    CC_CHECK(CcCoreMemoryShare(&mind, talk, 1U, true) == NULL);
    CcCoreModelFree(conversation.model);
    return 0;
}
