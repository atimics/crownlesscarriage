#ifndef CROWNLESS_RETURN_H
#define CROWNLESS_RETURN_H

#include "sim/cc_sim.h"

#include <stddef.h>

/* The Return (docs/design/the-return.md).

   When the company leaves a town, the simulation keeps a small record of how
   the town looked (CcTownSeen in cc_sim.h). When the company comes back, the
   digest compares that record with the town as it is now and ranks what
   changed. Every function here except CcReturnRecordSeen is pure: it reads the
   simulation and writes only its output. */

#define CC_RETURN_MAX_CHANGES 16

typedef enum CcReturnThreat {
    CC_RETURN_THREAT_BANDIT_CAMP = UINT32_C(1) << 0,
    CC_RETURN_THREAT_BANDIT_RAID = UINT32_C(1) << 1,
    CC_RETURN_THREAT_DRAGON_OMEN = UINT32_C(1) << 2
} CcReturnThreat;

typedef enum CcReturnChangeKind {
    CC_RETURN_CHANGE_ABANDONED = 0,
    CC_RETURN_CHANGE_FIRE,
    CC_RETURN_CHANGE_REBUILT,
    CC_RETURN_CHANGE_NEW_RULER,
    CC_RETURN_CHANGE_NEW_KINGDOM,
    CC_RETURN_CHANGE_FACE_DIED,
    CC_RETURN_CHANGE_FACE_GONE,
    CC_RETURN_CHANGE_HUNGER,
    CC_RETURN_CHANGE_FED,
    CC_RETURN_CHANGE_LAWLESS,
    CC_RETURN_CHANGE_SAFER,
    CC_RETURN_CHANGE_THRIVING,
    CC_RETURN_CHANGE_POORER,
    CC_RETURN_CHANGE_POPULATION,
    CC_RETURN_CHANGE_BANDIT_CAMP,
    CC_RETURN_CHANGE_BANDITS_GONE,
    CC_RETURN_CHANGE_DRAGON_OMEN,
    CC_RETURN_CHANGE_SERVICE_LOST,
    CC_RETURN_CHANGE_SERVICE_OPENED,
    CC_RETURN_CHANGE_STALL_EMPTY,
    CC_RETURN_CHANGE_STALL_RESTOCKED,
    CC_RETURN_CHANGE_PRICE,
    CC_RETURN_CHANGE_KIND_COUNT
} CcReturnChangeKind;

/* How the player could already know a change before seeing it. */
typedef enum CcReturnKnowledge {
    CC_RETURN_UNKNOWN = 0,
    CC_RETURN_TOLD,      /* someone told the company the story */
    CC_RETURN_WITNESSED  /* the company was a witness to the event */
} CcReturnKnowledge;

typedef struct CcReturnChange {
    CcReturnChangeKind kind;
    /* A good (stall, price), a service, or -1. */
    int32_t detail;
    /* The person or kingdom the change is about, and who it replaced. */
    CcId subject_id;
    CcId previous_id;
    char subject_name[CC_NAME_CAPACITY];
    char previous_name[CC_NAME_CAPACITY];
    int32_t before;
    int32_t after;
    uint32_t goods_mask; /* every good in an emptied market */
    int32_t magnitude;  /* 0..100: how large */
    int32_t surprise;   /* 0..100: how unexpected from the last view */
    CcReturnKnowledge knowledge;
    CcId evidence_event_id; /* the world event behind it, when one is known */
    CcId source_id;         /* who told the company, for CC_RETURN_TOLD */
    int32_t source_confidence;
    int32_t score;          /* ranking key; higher comes first */
} CcReturnChange;

typedef struct CcReturnDigest {
    CcId settlement_id;
    int32_t seen_day;
    int32_t return_day;
    bool first_visit;
    /* All changes found, including those beyond the kept list. */
    int32_t found_count;
    int32_t change_count;
    CcReturnChange changes[CC_RETURN_MAX_CHANGES];
} CcReturnDigest;

/* Capture a town as a traveller would see it now. */
bool CcReturnCapture(const CcSim *sim, CcId settlement_id, CcTownSeen *seen);
/* Remember the town as the company leaves it. */
void CcReturnRecordSeen(CcSim *sim, CcId settlement_id);
const CcTownSeen *CcReturnLastSeen(const CcSim *sim, CcId settlement_id);

/* Compare an earlier view with a later one and rank the changes. The sim
   supplies the event ring and the gossip the company heard. */
void CcReturnCompare(const CcSim *sim, const CcTownSeen *before,
                     const CcTownSeen *now, CcReturnDigest *digest);
/* The digest for a town the company is returning to. A town the company
   has never left gives an empty digest with first_visit set. */
bool CcReturnDigestBuild(const CcSim *sim, CcId settlement_id,
                         CcReturnDigest *digest);

const char *CcReturnChangeKindName(CcReturnChangeKind kind);
const char *CcReturnKnowledgeName(CcReturnKnowledge knowledge);
/* One plain line for debug views, such as "Fire: 60% of the town burned." */
void CcReturnDescribe(const CcSim *sim, const CcReturnChange *change,
                      char *text, size_t capacity);
/* Write the digest as text lines; returns false if the text was cut. */
bool CcReturnDigestText(const CcSim *sim, const CcReturnDigest *digest,
                        char *text, size_t capacity);

uint64_t CcReturnMemoryHash(const CcReturnMemory *memory);
bool CcReturnMemoryValidate(const CcSim *sim);
size_t CcReturnMemoryEncodedSize(const CcReturnMemory *memory);
size_t CcReturnMemoryEncode(const CcReturnMemory *memory, uint8_t *bytes,
                            size_t capacity);
bool CcReturnMemoryDecode(CcReturnMemory *memory, const uint8_t *bytes,
                          size_t length);

#endif
