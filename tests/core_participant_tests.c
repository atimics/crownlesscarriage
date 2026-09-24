#include "story/cc_core_participant.h"
#include "story/cc_core_model.h"
#include "test_support.h"
#include <stdio.h>
#include <string.h>

static CcSim sim;
int main(void)
{
    CcSimInit(&sim, 1202U);
    CcSimAdvanceDays(&sim, 367);
    const CcCharacter *a = NULL, *b = NULL;
    for (int32_t i = 0; i < sim.character_count; ++i) {
        if (strcmp(sim.characters[i].name, "Harthild Underwick") == 0) a = &sim.characters[i];
        if (strcmp(sim.characters[i].name, "Jory Fen") == 0) b = &sim.characters[i];
    }
    CC_CHECK(a != NULL && b != NULL);
    uint64_t before = CcSimHash(&sim);
    CcCoreParticipant first, second;
    CC_CHECK(CcCoreParticipantBuild(&sim, a->id, b->id, &first));
    CC_CHECK(CcCoreParticipantBuild(&sim, b->id, a->id, &second));
    CC_CHECK(CcSimHash(&sim) == before);
    CC_CHECK(first.id == a->id && first.listener_id == b->id && second.id == b->id);
    CC_CHECK(first.stress == a->stress && first.hungry_days == a->hungry_days);
    CC_CHECK(first.coins == a->travel_coins && first.occupation == a->occupation);
    CC_CHECK(strcmp(first.name, a->name) == 0 && strcmp(first.listener_name, b->name) == 0);
    CC_CHECK(first.account_count > 0U);
    bool local_work = false;
    for (size_t i = 0; i < first.account_count; ++i) {
        const CcGossipVersion *held = NULL;
        const CcGossip *story = CcSimPersonalGossip(&sim, a->id, (int32_t)i, &held);
        char text[CC_EVENT_TEXT_CAPACITY];
        CC_CHECK(story != NULL && held != NULL);
        CcGossipText(&sim, story, held, text, sizeof(text));
        CC_CHECK(strcmp(first.accounts[i].telling, text) == 0);
        CC_CHECK(first.accounts[i].source_id == held->source_character_id);
        if (story->kind == CC_EVENT_QUARRY_OUTPUT) local_work = true;
    }
    CC_CHECK(local_work);
    /* Distinct private knowledge remains with its owner. */
    CcCharacter *owner = &sim.characters[0], *other = &sim.characters[1];
    owner->knowledge_count = 1;
    owner->knowledge[0] = (CcCharacterKnowledge){.kind = CC_KNOWLEDGE_OFFER,
        .subject_id = 777U, .private_knowledge = true, .day = sim.current_day};
    other->knowledge_count = 0;
    CC_CHECK(CcCoreParticipantBuild(&sim, owner->id, other->id, &first));
    CC_CHECK(CcCoreParticipantBuild(&sim, other->id, owner->id, &second));
    CC_CHECK(first.knowledge_count == 1U && first.knowledge[0].private_knowledge);
    CC_CHECK(second.knowledge_count == 0U);
    /* A person's direct knowledge reference resolves with its original source
       and certainty. Other events and the listener's private records stay separate. */
    CC_CHECK(sim.event_count > 1);
    CcEvent *known_event = &sim.events[0];
    known_event->day = sim.current_day - 2;
    known_event->parent_id = sim.events[1].id;
    (void)snprintf(known_event->text, sizeof(known_event->text), "Bren left a lamp at the west gallery.");
    (void)snprintf(sim.events[1].text, sizeof(sim.events[1].text), "Unobserved parent event.");
    owner->knowledge[0].event_id = known_event->id;
    owner->knowledge[0].source_character_id = other->id;
    owner->knowledge[0].certainty = CC_KNOWLEDGE_TOLD;
    before = CcSimHash(&sim);
    CC_CHECK(CcCoreParticipantBuild(&sim, owner->id, other->id, &first));
    CC_CHECK(CcSimHash(&sim) == before);
    CC_CHECK(first.knowledge_events[0].available);
    CC_CHECK(first.knowledge_events[0].day == known_event->day);
    CC_CHECK(strcmp(first.knowledge_events[0].text, known_event->text) == 0);
    CC_CHECK(first.knowledge[0].certainty == CC_KNOWLEDGE_TOLD);
    CC_CHECK(first.knowledge[0].source_character_id == other->id);
    CC_CHECK(CcCoreParticipantBuild(&sim, other->id, owner->id, &second));
    CC_CHECK(second.knowledge_count == 0U && !second.knowledge_events[0].available);
    /* An event recorded after the person's knowledge date cannot explain it. */
    owner->knowledge[0].day = known_event->day - 1;
    CC_CHECK(CcCoreParticipantBuild(&sim, owner->id, other->id, &first));
    CC_CHECK(!first.knowledge_events[0].available && first.knowledge_events[0].text[0] == '\0');
    owner->knowledge[0].day = sim.current_day + 1;
    CC_CHECK(CcCoreParticipantBuild(&sim, owner->id, other->id, &first));
    CC_CHECK(!first.knowledge_events[0].available);
    owner->knowledge[0].day = sim.current_day;
    owner->knowledge[0].event_id = UINT64_MAX;
    CC_CHECK(CcCoreParticipantBuild(&sim, owner->id, other->id, &first));
    CC_CHECK(!first.knowledge_events[0].available);
    owner->knowledge[0].event_id = 0U;
    CC_CHECK(CcCoreParticipantBuild(&sim, owner->id, other->id, &first));
    CC_CHECK(!first.knowledge_events[0].available);
    /* The band name stays in identity, while the trained voice remains a trade. */
    first.occupation = CC_OCCUPATION_QUARRYMAN;
    (void)snprintf(first.band, sizeof(first.band), "The Unpaid Company");
    first.has_relationship = true;
    first.relationship.trust = -2; first.relationship.obligation = 2;
    first.account_count = 2;
    first.accounts[0].event_id = 42U; first.accounts[0].supported = true;
    (void)snprintf(first.accounts[0].language.account, CC_EVENT_TEXT_CAPACITY, "Current event.");
    first.accounts[1].event_id = 41U; first.accounts[1].supported = true;
    (void)snprintf(first.accounts[1].language.account, CC_EVENT_TEXT_CAPACITY, "Older held event.");
    CcCoreMind mind;
    CcGossipLanguage memories[CC_CORE_MIND_LINES];
    CcCoreParticipantMind(&first, 42U, false, &mind, memories);
    CC_CHECK(strcmp(mind.voice, "quarryman") == 0 && CcCoreModelVoiceId(mind.voice) > 0);
    CC_CHECK(!mind.trusts_listener && mind.owes_listener);
    CC_CHECK(mind.memory_count == 1U && strcmp(mind.memories[0], "Older held event.") == 0);
    CC_CHECK(!CcCoreParticipantBuild(&sim, 0U, other->id, &first));
    CC_CHECK(first.id == 0U && first.account_count == 0U);
    return 0;
}
