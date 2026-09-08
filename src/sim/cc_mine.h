#ifndef CROWNLESS_MINE_H
#define CROWNLESS_MINE_H
#include "sim/cc_sim.h"
#define CC_MINE_WIDTH 31
#define CC_MINE_HEIGHT 21
#define CC_MINE_PACK_CAPACITY 8
int32_t CcMineBranchSubtick(const CcSim *sim);
const CcRoadSite *CcMineSite(const CcSim *sim);
bool CcMineWalkable(const CcSim *sim, CcMinePhase phase, int32_t x, int32_t y);
int32_t CcMineChamber(int32_t x, int32_t y);
const char *CcMineChamberName(int32_t chamber);
int32_t CcMinePackUsed(const CcSim *sim);
const char *CcMineAction(const CcSim *sim);
bool CcMineApply(CcSim *sim, const CcCommand *command, char *error, size_t capacity);
bool CcMineValidate(const CcSim *sim);
#endif
