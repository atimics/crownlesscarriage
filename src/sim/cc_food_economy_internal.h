#ifndef CROWNLESS_FOOD_ECONOMY_INTERNAL_H
#define CROWNLESS_FOOD_ECONOMY_INTERNAL_H

#include "sim/cc_sim.h"

/* Shared reserve, nutrition storage, and market-price rules. */
int32_t CcEconomyWarExtraConsumption(const CcSim *sim,
                                   const CcSettlement *place,
                                   CcGood good);
int32_t CcEconomyEffectiveReserveTarget(const CcSim *sim,
                                      const CcSettlement *place,
                                      CcGood good);
int32_t CcEconomyCivilianFoodUse(const CcSettlement *place);
int32_t CcEconomyWeeklyFoodUse(const CcSim *sim,
                             const CcSettlement *place);
int32_t CcEconomyNutritionRations(const int32_t goods[CC_GOOD_COUNT],
                                CcNutritionPurpose purpose);
int32_t CcEconomyIncomingNutrition(const CcSim *sim, CcId settlement_id,
                                 CcNutritionPurpose purpose);
void CcEconomyRefreshSettlementGoodPrice(const CcSim *sim,
                                       CcSettlement *settlement,
                                       CcGood good);
int32_t CcEconomyNutritionStorageCapacity(const CcSim *sim,
                                        const CcSettlement *place,
                                        CcGood good);
int32_t CcEconomySpoilStoredNutrition(const CcSim *sim, CcSettlement *place,
                                    CcTownNutritionAccounting *accounting);

#endif
