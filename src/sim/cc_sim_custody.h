#ifndef CC_SIM_CUSTODY_H
#define CC_SIM_CUSTODY_H
#include "sim/cc_sim.h"

bool CcSimStoredCustodyValid(const CcSim *sim);
/* The town is the authority for its bulk store and its owned containers.
   Allocation and container revisions bind packing to the observed state. */
CcCustodyResult CcSimPackStoreGoods(CcSim *sim, CcId town_id,
    CcGood good, int32_t quantity, uint64_t container_id,
    uint64_t container_revision, uint64_t next_id, CcId event_id,
    uint64_t *entry_id);
CcCustodyResult CcSimUnpackStoreGoods(CcSim *sim, CcId town_id,
    uint64_t entry_id, uint64_t revision, int32_t quantity, CcId event_id);
#endif
