#ifndef CC_SIM_CUSTODY_H
#define CC_SIM_CUSTODY_H
#include "sim/cc_sim.h"
#include "sim/cc_production.h"

/* A producer supplies its remaining common production budget. Success spends
   one batch and returns the material/work receipt; failure preserves outputs. */
CcCustodyResult CcSimMakeCustodyContainer(CcSim *sim, CcProductionContext *work,
    uint64_t next_id, CcId event_id, uint64_t *entry_id, CcProductionReceipt *receipt);
CcCustodyResult CcSimRepairCustodyContainer(CcSim *sim, CcProductionContext *work,
    uint64_t container_id, uint64_t revision, CcId event_id, CcProductionReceipt *receipt);
int32_t CcSimCustodyCarrierLoad(const CcSim *sim, CcId carrier_id);
bool CcSimDispatchCustodyCarrier(CcSim *sim, CcId carrier_id, CcId destination_id);
CcCustodyResult CcSimTransferCustody(CcSim *sim, const CcCustodyTransfer *transfer,
                                    uint64_t *result_id);
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
