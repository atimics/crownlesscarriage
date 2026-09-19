#ifndef CROWNLESS_MINE_H
#define CROWNLESS_MINE_H
#include "sim/cc_sim.h"
#define CC_MINE_WIDTH 31
#define CC_MINE_HEIGHT 21
#define CC_MINE_PACK_CAPACITY 8
#define CC_MINE_CACHE_CAPACITY 7
int32_t CcMineBranchSubtick(const CcSim *sim);
const CcRoadSite *CcMineSite(const CcSim *sim);
bool CcMineWalkable(const CcSim *sim, CcMinePhase phase, int32_t x, int32_t y);
bool CcMineWalkableState(CcMinePhase phase, int32_t x, int32_t y,
                         bool bar_open);
int32_t CcMineChamber(int32_t x, int32_t y);
const char *CcMineChamberName(int32_t chamber);
int32_t CcMinePackUsed(const CcSim *sim);
int32_t CcMinePackGood(const CcSim *sim, CcGood good);
void CcMineInitializeLoad(CcSim *sim);
bool CcMineSettleFallenPack(CcSim *sim);
int32_t CcMineSourceUsed(const CcSim *sim);
int32_t CcMineCacheUsed(const CcSim *sim);
int32_t CcMineSourceGood(const CcSim *sim, CcGood good);
int32_t CcMineCacheGood(const CcSim *sim, CcGood good);
const char *CcMineAction(const CcSim *sim);
bool CcMineApply(CcSim *sim, const CcCommand *command, char *error, size_t capacity);
bool CcMineValidate(const CcSim *sim);
#endif
