#ifndef CC_GATE_VOICE_H
#define CC_GATE_VOICE_H

#include "sim/cc_return.h"
#include "story/cc_core_account.h"
#include "story/cc_speech.h"

/* The Return, milestone 3: the gate voice (docs/design/the-return.md).

   When the company comes back to a town it has seen before, one resident at
   the gate says the most important change the company does not know yet, in
   their own words. Everything here is pure: it reads the simulation and
   writes only the voice. The one state change, marking the story as told to
   the company, is an ordinary journalled command (CcGateVoiceToldCommand).

   Every clause of the line records its evidence: the resident's own telling
   of a gossip story, who told them, the town as it is now, or the company's
   memory of a face. Nothing is said that the simulation does not hold. */

#define CC_GATE_VOICE_CLAUSES 4
#define CC_GATE_VOICE_FACTS CC_CORE_FIELDS
#define CC_GATE_VOICE_CLAUSE_CAPACITY 192

/* How the resident knows what they say. */
typedef enum CcGateVoiceTelling {
    CC_GATE_VOICE_HEARD = 0, /* their own telling of a gossip story */
    CC_GATE_VOICE_SEEN       /* plainly visible in the town, full confidence */
} CcGateVoiceTelling;

/* What a clause traces to. */
typedef enum CcGateVoiceEvidence {
    CC_GATE_EVIDENCE_NONE = 0,
    CC_GATE_EVIDENCE_STORY,  /* the speaker's gossip version of a story */
    CC_GATE_EVIDENCE_SOURCE, /* that version's source_character_id */
    CC_GATE_EVIDENCE_TOWN,   /* the town now against the company's last view */
    CC_GATE_EVIDENCE_FACE,   /* the speaker is a face the company remembers */
    CC_GATE_EVIDENCE_ROAD    /* a traveller's own road: the town they left */
} CcGateVoiceEvidence;

/* The spoken gate order: a greeting, what happened, why, and who said so.
   A line's clauses never go backwards in this order. */
typedef enum CcGateVoicePart {
    CC_GATE_PART_GREETING = 0,
    CC_GATE_PART_EVENT,
    CC_GATE_PART_CAUSE,
    CC_GATE_PART_SOURCE
} CcGateVoicePart;

typedef struct CcGateVoiceClause {
    char text[CC_GATE_VOICE_CLAUSE_CAPACITY];
    CcGateVoicePart part;
    CcGateVoiceEvidence evidence;
    /* The story's event, the source's id, the town, or the face. */
    CcId evidence_id;
    /* The clause also reads the town as it is now (how much burned). */
    bool town_state;
} CcGateVoiceClause;

/* A typed fact from the held account, as in tools/dialogue/fact_policy.py. */
typedef enum CcGateVoiceCertainty {
    CC_GATE_CERTAIN_WITNESSED = 0,
    CC_GATE_CERTAIN_TOLD,
    CC_GATE_CERTAIN_DOUBTFUL
} CcGateVoiceCertainty;

typedef struct CcGateVoiceFact {
    CcCoreRole role;
    CcGateVoiceCertainty certainty;
    int32_t field; /* index into the parsed account */
    char value[CC_EVENT_TEXT_CAPACITY];
} CcGateVoiceFact;

typedef struct CcGateVoiceKey {
    CcReturnChangeKind kind;
    int32_t detail;
    CcId subject_id;
} CcGateVoiceKey;

typedef struct CcGateVoice {
    CcId settlement_id;
    CcId speaker_id;
    /* Milestone 4: a traveller met on the road, not a resident at the gate.
       The town is named, not "here", and nobody says "You're back." */
    bool on_road;
    char speaker[CC_NAME_CAPACITY];
    /* "innkeeper", "resident", and so on, for the name plate. */
    char speaker_label[CC_NAME_CAPACITY];
    bool remembered_face;

    CcReturnChange change;
    CcGateVoiceTelling telling;
    /* The ledger slot of the evidence story the speaker holds, or -1. */
    int32_t story_slot;
    CcGossipVersion version;
    int32_t confidence;
    char source_name[CC_NAME_CAPACITY];

    /* Typed fact selection: the role the change asks about, the facts the
       held account offers, and the chosen one (-1 defers). */
    CcCoreRole asked_role;
    int32_t fact_count;
    CcGateVoiceFact facts[CC_GATE_VOICE_FACTS];
    int32_t chosen_fact;

    int32_t clause_count;
    CcGateVoiceClause clauses[CC_GATE_VOICE_CLAUSES];
    char line[CC_SPEECH_TEXT_CAPACITY];

    /* Changes already spoken during this visit, and whether another
       unknown change remains for "Tell me more". */
    int32_t spoken_count;
    CcGateVoiceKey spoken[CC_RETURN_MAX_CHANGES];
    bool more;
} CcGateVoice;

/* Pick who stands at the gate: a remembered face who is alive and in town,
   else a plausible local. Those who hold the evidence story come first. */
CcId CcGateVoicePickSpeaker(const CcSim *sim, CcId settlement_id,
                            CcId evidence_event_id);
/* Start the voice for a return. False for a first visit, when nothing new
   is left to say, or when nobody is at the gate. */
bool CcGateVoiceBegin(const CcSim *sim, CcId settlement_id, CcGateVoice *voice);
/* The same speaker's next untold change ("Tell me more"). */
bool CcGateVoiceNext(const CcSim *sim, CcGateVoice *voice);
/* Build the line for one change and one speaker. Exposed for tests and tools. */
bool CcGateVoiceSay(const CcSim *sim, CcId speaker_id,
                    const CcReturnChange *change, CcGateVoice *voice);
/* Milestone 4: a traveller on the road tells a change about `town` from their
   own telling of the evidence story. The event comes first and names the
   town, then the hedge or who said so, then, when the traveller has just
   left that town, a word about their own road. False when the traveller
   does not hold the story: a traveller only passes on what they carry. */
bool CcGateVoiceSayOnRoad(const CcSim *sim, CcId traveller_id, CcId town,
                          const CcReturnChange *change, CcGateVoice *voice);
/* Whether an unknown, unspoken change remains. Call it again after the told
   command: one story can explain several changes. */
bool CcGateVoiceHasMore(const CcSim *sim, const CcGateVoice *voice);
/* The command that marks the spoken story as told to the company. False
   when the line was seen, not heard: there is no story to mark. */
bool CcGateVoiceToldCommand(const CcGateVoice *voice, CcCommand *command);
/* The line as a speech turn for subtitles and the voice seam. */
bool CcGateVoiceSpeech(const CcSim *sim, const CcGateVoice *voice,
                       CcSpeech *speech);
const char *CcGateVoiceEvidenceName(CcGateVoiceEvidence evidence);
const char *CcGateVoicePartName(CcGateVoicePart part);

#endif
