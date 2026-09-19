#include "story/cc_core_participant.h"
#include <stdio.h>
#include <string.h>

const char *CcCoreOccupationName(CcCharacterOccupation occupation)
{
    static const char *const names[CC_OCCUPATION_COUNT] = {
        "resident", "woodcutter", "shepherd", "miller", "smith", "quarryman",
        "farmer", "baker", "innkeeper", "cartwright", "scribe"
    };
    return occupation >= CC_OCCUPATION_NONE && occupation < CC_OCCUPATION_COUNT ?
        names[occupation] : "resident";
}

bool CcCoreParticipantBuild(const CcSim *sim, CcId speaker, CcId listener,
                            CcCoreParticipant *p)
{
    if (p == NULL) return false;
    *p = (CcCoreParticipant){0};
    if (sim == NULL) return false;
    const CcCharacter *self = CcSimCharacter(sim, speaker);
    if (self == NULL) return false;
    p->id = self->id; p->listener_id = listener;
    p->home_id = self->home_settlement_id; p->place_id = self->current_settlement_id;
    p->faction_id = self->faction_id; p->bandit_id = self->bandit_group_id;
    p->day = sim->current_day; p->age = CcCharacterAgeYears(sim, self);
    p->stress = self->stress; p->courage = self->courage;
    p->hungry_days = self->hungry_days; p->unsheltered_nights = self->unsheltered_nights;
    p->coins = self->travel_coins; p->role = self->role; p->occupation = self->occupation;
    p->goal = self->goal; p->activity = self->activity;
    p->in_transit = self->travel_destination_id != 0U;
    (void)snprintf(p->name, sizeof(p->name), "%s", self->name);
    const CcCharacter *other = CcSimCharacter(sim, listener);
    if (other != NULL) (void)snprintf(p->listener_name, sizeof(p->listener_name), "%s", other->name);
    const CcSettlement *home = CcSimSettlement(sim, p->home_id);
    const CcSettlement *place = CcSimSettlement(sim, p->place_id);
    if (home != NULL) (void)snprintf(p->home, sizeof(p->home), "%s", home->name);
    if (place != NULL) (void)snprintf(p->place, sizeof(p->place), "%s", place->name);
    for (int32_t i = 0; i < sim->bandit_count; ++i)
        if (sim->bandits[i].id == p->bandit_id)
            (void)snprintf(p->band, sizeof(p->band), "%s", sim->bandits[i].name);
    for (int32_t i = 0; i < sim->faction_count; ++i) {
        if (sim->factions[i].id != p->faction_id) continue;
        switch (sim->factions[i].kind) {
            case CC_FACTION_CROWN: p->faction = CC_CORE_FACTION_CROWN; break;
            case CC_FACTION_GUILD: p->faction = CC_CORE_FACTION_GUILD; break;
            case CC_FACTION_COMMONS: p->faction = CC_CORE_FACTION_COMMONS; break;
            default: break;
        }
    }
    const CcRelationship *bond = listener == speaker ? NULL : CcSimRelationship(sim, speaker, listener);
    if (bond != NULL) { p->relationship = *bond; p->has_relationship = true; }
    for (int32_t i = 0; i < self->memory_count && i < CC_CHARACTER_MEMORY_CAPACITY; ++i)
        p->memories[p->memory_count++] = self->memories[i];
    for (int32_t i = 0; i < self->knowledge_count && i < CC_CHARACTER_KNOWLEDGE_CAPACITY; ++i) {
        const CcCharacterKnowledge *known = &self->knowledge[i];
        size_t slot = p->knowledge_count++;
        p->knowledge[slot] = *known;
        /* Resolve only the event directly recorded in this person's knowledge.
           Its source and certainty remain attached to the remembered account. */
        const CcEvent *event = known->event_id != 0U ? CcSimEvent(sim, known->event_id) : NULL;
        if (event != NULL && known->day <= sim->current_day &&
            event->day <= known->day && event->text[0] != '\0') {
            CcCoreKnownEvent *detail = &p->knowledge_events[slot];
            detail->available = true;
            detail->day = event->day;
            (void)snprintf(detail->text, sizeof(detail->text), "%s", event->text);
        }
    }
    for (int32_t i = 0; i < CC_MAX_GOSSIP; ++i) {
        const CcGossipVersion *version = NULL;
        const CcGossip *story = CcSimPersonalGossip(sim, speaker, i, &version);
        if (story == NULL || version == NULL) break;
        CcCoreHeldAccount *held = &p->accounts[p->account_count++];
        held->event_id = story->event_id; held->day = story->day;
        held->source_id = version->source_character_id;
        CcGossipText(sim, story, version, held->telling, sizeof(held->telling));
        held->supported = CcSpeechPrepareGossip(sim, story, version, 0U, &held->language);
    }
    return true;
}

static CcCoreLevel Level(int32_t value)
{
    return value < 34 ? CC_CORE_LEVEL_LOW : value > 66 ? CC_CORE_LEVEL_HIGH : CC_CORE_LEVEL_MEDIUM;
}

void CcCoreParticipantMind(const CcCoreParticipant *p, CcId current_event,
                            bool witnessed, CcCoreMind *mind,
                            CcGossipLanguage memory_language[CC_CORE_MIND_LINES])
{
    if (mind == NULL) return;
    *mind = (CcCoreMind){0};
    if (p == NULL) return;
    mind->goal = (CcCoreGoal)p->goal; mind->stress = Level(p->stress); mind->courage = Level(p->courage);
    mind->voice = CcCoreOccupationName(p->occupation);
    mind->witnessed = witnessed; mind->hungry = p->hungry_days > 0;
    mind->sheltered = p->unsheltered_nights == 0; mind->in_transit = p->in_transit;
    mind->faction = p->faction; mind->far_from_home = p->home_id != p->place_id;
    mind->owes_listener = p->has_relationship && p->relationship.obligation >= 2;
    mind->trusts_listener = p->has_relationship && p->relationship.trust >= 2;
    if (memory_language == NULL) return;
    for (size_t i = 0; i < p->account_count && mind->memory_count < CC_CORE_MIND_LINES; ++i) {
        const CcCoreHeldAccount *held = &p->accounts[i];
        if (held->event_id == current_event || !held->supported || held->language.account[0] == '\0') continue;
        size_t m = mind->memory_count++;
        memory_language[m] = held->language;
        mind->memories[m] = memory_language[m].account;
    }
}
