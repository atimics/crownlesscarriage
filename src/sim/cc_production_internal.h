#ifndef CROWNLESS_PRODUCTION_INTERNAL_H
#define CROWNLESS_PRODUCTION_INTERNAL_H

#include "sim/cc_sim.h"

/* Shared by weekly production and the food-economy report. */
int32_t CcEconomyBakeryCapacity(const CcSettlement *place);
int32_t CcEconomyEffectiveProduction(const CcSim *sim,
    const CcSettlement *settlement, int32_t index, CcGood good);

/* Core services retain event history and treasure allocation ownership. */
typedef struct {
    CcEvent *(*record_event)(CcSim *sim, CcEventKind kind, CcId subject,
        CcId location, CcId parent, int32_t magnitude, const char *text);
    CcId (*latest_local_cause)(const CcSim *sim, CcId location);
    void (*complete_treasure)(CcSim *sim, CcSettlement *settlement);
} CcProductionServices;

void CcEconomyWearOneTool(CcSettlement *settlement, int32_t *wear,
                        int32_t batches_per_tool);
int32_t CcEconomyRunBakery(CcSim *sim, CcSettlement *place,
                         CcId scriptorium_id,
    const CcProductionServices *services);
void CcEconomyRunSmithy(CcSim *sim, CcSettlement *settlement,
    const CcProductionServices *services);
void CcEconomyRunPaperMill(CcSim *sim, CcSettlement *settlement,
    const CcProductionServices *services);
void CcEconomyAdvanceRareMineWork(CcSim *sim, CcSettlement *settlement,
                                int32_t iron_mined,
    const CcProductionServices *services);

#endif
