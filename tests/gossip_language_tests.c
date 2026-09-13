#include "story/cc_speech.h"
#include "test_support.h"
#include <stdio.h>
#include <string.h>

static CcSim sim;

static void CheckClaim(CcEventKind kind, const char *account, const char *expected)
{
    CcGossip story = {.kind = kind, .event_id = 123U};
    (void)snprintf(story.text, sizeof(story.text), "%s", account);
    CcGossipVersion version = {.confidence = 80};
    uint64_t before = CcSimHash(&sim);
    CcGossipLanguage language;
    for (uint32_t variant = 0; variant < 2U; ++variant) {
        CC_CHECK(CcSpeechPrepareGossip(&sim, &story, &version, variant, &language));
        CC_CHECK(strstr(language.claim, expected) != NULL);
        CC_CHECK(strcmp(language.account, account) == 0);
        char text[CC_SPEECH_TEXT_CAPACITY];
        CC_CHECK(CcSpeechCoreGossip(&language, text, sizeof(text)));
        char clause[CC_SPEECH_TEXT_CAPACITY];
        (void)snprintf(clause, sizeof(clause), "%s", language.claim);
        clause[strlen(clause) - 1U] = '\0';
        CC_CHECK(strstr(text, clause) != NULL);
        CcCharacter speaker = sim.characters[0];
        CC_CHECK(CcSpeechRealizeGossip(&sim, &speaker, &story, &version, text, sizeof(text)));
        CC_CHECK(strstr(text, expected) != NULL);
        CC_CHECK(strstr(text, "only heard bits") == NULL);
    }
    language.confidence = 20;
    char text[CC_SPEECH_TEXT_CAPACITY];
    CC_CHECK(CcSpeechCoreGossip(&language, text, sizeof(text)));
    CC_CHECK(strstr(text, "truth in the rumour") != NULL);
    language.confidence = 80;
    language.retellings = 5;
    CC_CHECK(CcSpeechCoreGossip(&language, text, sizeof(text)));
    CC_CHECK(strstr(text, "so people say") != NULL);
    CC_CHECK(!CcSpeechCoreGossip(&language, text, 8));
    CC_CHECK(text[0] == '\0');
    CC_CHECK(CcSimHash(&sim) == before);
}

static void UncertainDetails(void)
{
    CcGossip story = {.kind = CC_EVENT_NOTICE_POSTED, .event_id = 234U};
    (void)snprintf(story.text, sizeof(story.text),
        "Forgen Miller posts a notice at Thornford: Relief charter.");
    CcGossipVersion version = {.confidence = 20, .retellings = 5};
    CcGossipLanguage language;
    char text[CC_SPEECH_TEXT_CAPACITY];
    CC_CHECK(CcSpeechPrepareGossip(&sim, &story, &version, 0, &language));
    CC_CHECK(language.detail == CC_GOSSIP_DETAIL_ACTOR);
    CC_CHECK(CcSpeechCoreGossip(&language, text, sizeof(text)));
    CC_CHECK(strcmp(text, "Someone put up a relief charter in Thornford.") == 0);
    CC_CHECK(strstr(text, "Forgen") == NULL);
    CC_CHECK(CcSpeechPrepareGossip(&sim, &story, &version, 1, &language));
    CC_CHECK(language.detail == CC_GOSSIP_DETAIL_SUBJECT);
    CC_CHECK(CcSpeechCoreGossip(&language, text, sizeof(text)));
    CC_CHECK(strcmp(text, "Forgen Miller posted something in Thornford.") == 0);
    CC_CHECK(strstr(text, "charter") == NULL);
    CC_CHECK(strcmp(story.text, "Forgen Miller posts a notice at Thornford: Relief charter.") == 0);
    CC_CHECK(CcSpeechRealizeGossip(&sim, &sim.characters[0], &story, &version, text, sizeof(text)));
    CC_CHECK(strstr(text, "Someone") != NULL || strstr(text, "something") != NULL);
    story.kind = CC_EVENT_PEACE_DECLARED;
    (void)snprintf(story.text, sizeof(story.text),
        "Willow Republic's courier reaches Ashen Throne: peace now binds the two courts.");
    version.confidence = 80;
    CC_CHECK(CcSpeechPrepareGossip(&sim, &story, &version, 0, &language));
    CC_CHECK(CcSpeechCoreGossip(&language, text, sizeof(text)));
    CC_CHECK(strstr(text, "Have you heard?") != NULL);
    CC_CHECK(strstr(text, "Willow Republic and Ashen Throne made peace.") != NULL);
    CC_CHECK(strstr(text, "word going round") != NULL);
    version.confidence = 20;
    CC_CHECK(CcSpeechPrepareGossip(&sim, &story, &version, 0, &language));
    CC_CHECK(CcSpeechCoreGossip(&language, text, sizeof(text)));
    CC_CHECK(strcmp(text, "Some courts made peace.") == 0);
    CC_CHECK(CcSpeechPrepareGossip(&sim, &story, &version, 1, &language));
    CC_CHECK(CcSpeechCoreGossip(&language, text, sizeof(text)));
    CC_CHECK(strcmp(text, "Something changed between Willow Republic and Ashen Throne.") == 0);
}

int main(void)
{
    CcSimInit(&sim, 17U);
    UncertainDetails();
    CheckClaim(CC_EVENT_HARVEST_FAILED,
        "Thornford's drought harvest cannot supply the eastern settlements.", "eastern settlements");
    CheckClaim(CC_EVENT_ROUTE_CLOSED,
        "Alderwatch closes the treaty bridge and delays the relief convoy.", "relief convoy");
    CheckClaim(CC_EVENT_BANDIT_PRESSURE,
        "Displaced workers reinforce The Ditch Parliament on the old road.", "Displaced workers");
    CheckClaim(CC_EVENT_NOTICE_POSTED,
        "Mara Venn posts a notice at Thornford: Relief charter.", "relief charter");
    CheckClaim(CC_EVENT_NOTICE_POSTED,
        "Tomas Rill posts a notice at Gloamgate: Quiet commission.", "quiet commission");
    CheckClaim(CC_EVENT_CHARACTER_DIED,
        "Mara died at age 72 after a life in Silverwick.", "Mara");
    CheckClaim(CC_EVENT_TREASURE_CRAFTED,
        "Silverwick finishes Sun Cup from 3 Raw Gold, 2 Gems, and 8 weeks of work.", "Sun Cup");
    CheckClaim(CC_EVENT_WAR_DECLARED,
        "Ash's courier reaches Oak: war now binds the two courts.", "war");
    CheckClaim(CC_EVENT_PEACE_DECLARED,
        "Ash's courier reaches Oak: peace now binds the two courts.", "peace");
    /* The held claim takes precedence over the original event's category. */
    CheckClaim(CC_EVENT_WAR_DECLARED,
        "Ash's courier reaches Oak: peace now binds the two courts.", "peace");
    CcGossip story = {.kind = CC_EVENT_SHORTAGE, .event_id = 9U};
    (void)snprintf(story.text, sizeof(story.text),
        "Silverwick has 3 weeks of food; hunger reaches pressure level 60.");
    CcGossipVersion version = {.confidence = 60, .retellings = 4};
    CcGossipLanguage original, changed;
    CC_CHECK(CcSpeechPrepareGossip(&sim, &story, &version, 0, &original));
    char held[CC_EVENT_TEXT_CAPACITY];
    CcGossipText(&sim, &story, &version, held, sizeof(held));
    CC_CHECK(strcmp(original.account, held) == 0);
    sim.dragon.slain = !sim.dragon.slain;
    sim.settlements[0].hunger = 0;
    (void)snprintf(sim.settlements[0].name, sizeof(sim.settlements[0].name), "New Name");
    CC_CHECK(CcSpeechPrepareGossip(&sim, &story, &version, 0, &changed));
    CC_CHECK(memcmp(&original, &changed, sizeof(original)) == 0);
    story.kind = CC_EVENT_HARVEST_FAILED;
    (void)snprintf(story.text, sizeof(story.text),
        "Thornford's drought harvest cannot supply the eastern settlements.");
    CC_CHECK(CcSpeechPrepareGossip(&sim, &story, &version, 0, &changed));
    CC_CHECK(strstr(changed.claim, "southern settlements") != NULL);
    CC_CHECK(strstr(changed.claim, "eastern") == NULL);
    story.kind = CC_EVENT_BANDIT_PRESSURE;
    version.retellings = 6;
    (void)snprintf(story.text, sizeof(story.text),
        "Displaced workers reinforce The Ditch Parliament on the old road.");
    CC_CHECK(CcSpeechPrepareGossip(&sim, &story, &version, 0, &changed));
    CC_CHECK(strstr(changed.claim, "merchants") != NULL);
    CC_CHECK(strstr(changed.claim, "workers") == NULL);
    story.kind = CC_EVENT_SHORTAGE;
    (void)snprintf(story.text, sizeof(story.text), "There are seventeen unknown things.");
    CC_CHECK(!CcSpeechPrepareGossip(&sim, &story, &version, 0, &changed));
    CC_CHECK(changed.claim[0] == '\0');
    CC_CHECK(!CcSpeechPrepareGossip(NULL, &story, &version, 0, &changed));
    CC_CHECK(!CcSpeechPrepareGossip(&sim, &story, &version, 2, &changed));
    return 0;
}
