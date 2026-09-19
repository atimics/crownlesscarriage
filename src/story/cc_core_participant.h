#ifndef CC_CORE_PARTICIPANT_H
#define CC_CORE_PARTICIPANT_H
#include "story/cc_core_account.h"
#include "story/cc_speech.h"

/* An owned view of one person's present state and held evidence. */
typedef struct CcCoreHeldAccount {
    CcId event_id, source_id;
    int32_t day;
    char telling[CC_EVENT_TEXT_CAPACITY];
    bool supported;
    CcGossipLanguage language;
} CcCoreHeldAccount;

typedef struct CcCoreKnownEvent {
    bool available;
    int32_t day;
    char text[CC_EVENT_TEXT_CAPACITY];
} CcCoreKnownEvent;

typedef struct CcCoreParticipant {
    CcId id, listener_id, home_id, place_id, faction_id, bandit_id;
    char name[CC_NAME_CAPACITY], listener_name[CC_NAME_CAPACITY];
    char home[CC_NAME_CAPACITY], place[CC_NAME_CAPACITY], band[CC_NAME_CAPACITY];
    int32_t day, age, stress, courage, hungry_days, unsheltered_nights;
    CcMoney coins;
    CcCharacterRole role;
    CcCharacterOccupation occupation;
    CcCharacterGoal goal;
    CcCharacterActivity activity;
    bool in_transit, has_relationship;
    CcCoreFaction faction;
    CcRelationship relationship;
    CcCharacterMemory memories[CC_CHARACTER_MEMORY_CAPACITY];
    size_t memory_count;
    CcCharacterKnowledge knowledge[CC_CHARACTER_KNOWLEDGE_CAPACITY];
    CcCoreKnownEvent knowledge_events[CC_CHARACTER_KNOWLEDGE_CAPACITY];
    size_t knowledge_count;
    CcCoreHeldAccount accounts[CC_MAX_GOSSIP];
    size_t account_count;
} CcCoreParticipant;

const char *CcCoreOccupationName(CcCharacterOccupation occupation);
bool CcCoreParticipantBuild(const CcSim *sim, CcId speaker, CcId listener,
                            CcCoreParticipant *participant);
/* The shipped model receives its supported traits and older held accounts.
   Caller owns memory_language for the lifetime of the returned mind. */
void CcCoreParticipantMind(const CcCoreParticipant *participant, CcId current_event,
                            bool witnessed, CcCoreMind *mind,
                            CcGossipLanguage memory_language[CC_CORE_MIND_LINES]);
#endif
