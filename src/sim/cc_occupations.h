#ifndef CC_OCCUPATIONS_H
#define CC_OCCUPATIONS_H
#include "sim/cc_gossip_topics.h"

const char *CcOccupationName(CcCharacterOccupation occupation);
CcGossipTopic CcOccupationTopic(CcCharacterOccupation occupation);
bool CcGossipCraftEvent(CcEventKind kind);
bool CcOccupationObserves(CcCharacterOccupation occupation, CcEventKind kind);
/* Allocation uses the town's trades and stable character IDs. */
CcCharacterOccupation CcSimInitialOccupation(const CcSim *sim, CcId home, CcId person);
void CcSimInitializeOccupations(CcSim *sim);
#endif
