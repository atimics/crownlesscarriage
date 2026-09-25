#ifndef CC_WANTS_H
#define CC_WANTS_H
#include "sim/cc_sim.h"

typedef struct CcWantOffer {
    CcCommand command;
    bool ready;
    char label[96], detail[192];
} CcWantOffer;

void CcWantsInit(CcSim *sim);
void CcWantsAdvance(CcSim *sim);
const CcPersonalWant *CcWantsFind(const CcSim *sim, CcId id);
const CcBelonging *CcWantsItem(const CcSim *sim, CcId id);
bool CcWantsApply(CcSim *sim, const CcCommand *command, char *message, size_t capacity);
bool CcWantsPlan(const CcSim *sim, const CcCommand *command, char *reason, size_t capacity);
int32_t CcWantsOffers(const CcSim *sim, CcId person_id, CcWantOffer *offers, int32_t capacity);
bool CcWantsRequestText(const CcSim *sim, CcId request, char *text, size_t capacity);
bool CcWantsPersonText(const CcSim *sim, CcId person, char *text, size_t capacity);
void CcWantsDescribe(const CcSim *sim, char *text, size_t capacity);
bool CcWantsValidate(const CcSim *sim);
bool CcWantsCustodyReference(const CcSim *sim, const CcCustodyEntry *entry);
uint64_t CcWantsHash(const CcWantsState *state);
size_t CcWantsEncode(const CcWantsState *state, uint8_t *bytes, size_t capacity);
bool CcWantsDecode(CcWantsState *state, const uint8_t *bytes, size_t length);
#endif
