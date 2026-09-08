#include "sim/cc_route_rules_internal.h"

int32_t CcRouteDragonShadowDanger(const CcSim *sim,
                                       const CcRoute *route)
{
    if (sim == NULL || route == NULL ||
        (route->from_id != sim->dragon.lair_settlement_id &&
         route->to_id != sim->dragon.lair_settlement_id)) return 0;
    int32_t influence = sim->dragon.regional_influence;
    return sim->dragon.slain ? (influence >= 60 ? 1 : 0) :
           influence >= 80 ? 2 : influence >= 50 ? 1 : 0;
}

CcMoney CcRouteToll(const CcSim *sim, const CcRoute *route)
{
    if (sim == NULL || route == NULL) return 0;
    CcMoney toll = route->closed ? 4 : 0;
    if (route->smuggler_route) {
        toll += 2;
    } else if (CcSimRouteCrossesWarBorder(sim, route->id)) {
        toll += 4;
    }
    return toll;
}

