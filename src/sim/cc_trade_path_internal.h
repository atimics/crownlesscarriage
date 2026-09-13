#ifndef CROWNLESS_TRADE_PATH_INTERNAL_H
#define CROWNLESS_TRADE_PATH_INTERNAL_H

#include "sim/cc_sim.h"

/* Shared freight capacity and path selection. Simulation state is required.
   Failed searches preserve caller output values. */
int32_t CcTradeRouteCapacity(const CcSim *sim, const CcRoute *route);
bool CcTradeFindPath(const CcSim *sim, CcId from_id, CcId to_id, CcGood good,
    int32_t *first_route_slot, CcId *first_hop_id, int32_t *total_cost,
    int32_t *path_capacity, const int32_t route_used[CC_MAX_ROUTES],
    bool allow_kingdom_borders, CcId carriage_kingdom_id,
    bool ignore_royal_relations, int32_t required_slots);

#endif
