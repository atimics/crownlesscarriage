#ifndef CROWNLESS_PRODUCTION_INTERNAL_H
#define CROWNLESS_PRODUCTION_INTERNAL_H

#include "sim/cc_sim.h"

/* Shared by weekly production and the food-economy report. */
int32_t CcEconomyBakeryCapacity(const CcSettlement *place);
int32_t CcEconomyEffectiveProduction(const CcSim *sim,
    const CcSettlement *settlement, int32_t index, CcGood good);

#endif
