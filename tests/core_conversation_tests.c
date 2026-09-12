#include "story/cc_core_conversation.h"
#include "test_support.h"
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
    CcCoreConversationHear(&conversation, sim.player.id, "How sure are you?");
    CcSpeech answer = original[1];
    CC_CHECK(CcCoreConversationPrepare(&conversation, &accounts[1], &answer));
    CC_CHECK(conversation.pending);
    CcCoreConversationStep(&conversation, 1000U);
    CC_CHECK(!conversation.pending && strcmp(conversation.reply.line_id, "gossip.core") == 0);
    CC_CHECK(CcSimHash(&sim) == before);
    (void)printf("World hash retained: %016" PRIx64 "\n", before);
    CcCoreConversationReset(&conversation);
    CC_CHECK(conversation.count == 0U && !conversation.cached && !conversation.pending);
    CcCoreAccount bad = accounts[0]; bad.fields[0].start = SIZE_MAX;
    CC_CHECK(!CcCoreModelBegin(conversation.model, &bad, original[0].speaker_id, NULL, 0U));
    CC_CHECK(CcCoreModelText(conversation.model) == NULL);
    CcCoreModelFree(conversation.model);
    return 0;
}
