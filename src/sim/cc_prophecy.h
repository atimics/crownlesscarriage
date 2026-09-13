#ifndef CC_PROPHECY_H
#define CC_PROPHECY_H
#include "sim/cc_sim.h"
#define CC_DEEP_WYRM_SEED UINT32_C(2536334854)
#define CC_DEEP_WYRM_DAY 73366
#define CC_PROPHECY_TITLE "Prophecy of the Deep Wyrm"
#define CC_PROPHECY_WORDS "Three courts shall bind their banners. A champion shall leave Gloamgate. The Deep Wyrm shall fall by the narrowest measure."
#define CC_PROPHECY_CHARGE "Carry this book to the town council in Gloamgate. Let them hear the warning while there is time."
const CcTreasure *CcSimDeepWyrmProphecy(const CcSim *sim);
const CcSettlement *CcSimProphecyDestination(const CcSim *sim);
bool CcSimGiveDeepWyrmProphecy(CcSim *sim);
bool CcSimCanDeliverProphecy(const CcSim *sim);
#endif
