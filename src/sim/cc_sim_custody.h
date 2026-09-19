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
/* Schema 102: a fallen person's carried purse (#288/#406). The death drops
   the coins the person actually carried at the place they fell; the purse is
   a custody entry owned by the dead person, lying at that place, claimable by
   whoever is there. Returns false when custody is full; the caller must then
   resolve the coins another way (never destroy them). */
bool CcSimLeaveBodyPurse(CcSim *sim, CcId person_id, CcId place_id,
    CcMoney coins, CcId death_event_id);
/* Claim a fallen person's purse for the player company at the player's
   current location. The purse dies with the claim; the coins are conserved. */
bool CcSimClaimBodyPurse(CcSim *sim, CcId entry_id, char *error, size_t error_capacity);
/* Is this entry an unclaimed fallen person's purse? */
bool CcSimIsBodyPurse(const CcSim *sim, const CcCustodyEntry *entry);
bool CcSimDispatchCustodyCarrier(CcSim *sim, CcId carrier_id, CcId destination_id);
CcCustodyResult CcSimTransferCustody(CcSim *sim, const CcCustodyTransfer *transfer,
                                    uint64_t *result_id);
/* Schema 103: move a finite mine load between its source, carried pack, and
   located cache. The mine command supplies revision/context; custody supplies
   entry revisions, capacity, splitting, and conservation. */
CcCustodyResult CcSimTransferMineGoods(CcSim *sim, CcCustodyHolder source,
    CcCustodyHolder destination, CcGood good, int32_t quantity, uint64_t event_id);
/* Keep the mine source attached when ordinary market trade moves part or all
   of a carried load into a settlement store. */
CcCustodyResult CcSimPlanMineSale(const CcSim *sim, CcId town_id,
    CcGood good, int32_t quantity);
CcCustodyResult CcSimApplyMineSale(CcSim *sim, CcId town_id,
    CcGood good, int32_t quantity, CcId event_id);
bool CcSimMineEntryTracked(const CcSim *sim, const CcCustodyEntry *entry);
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
