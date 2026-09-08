#ifndef CROWNLESS_ROUTE_RULES_INTERNAL_H
#define CROWNLESS_ROUTE_RULES_INTERNAL_H

#include "sim/cc_sim.h"

/* Shared route costs and threats for journey planning and trade. */
int32_t CcRouteDragonShadowDanger(const CcSim *sim, const CcRoute *route);
CcMoney CcRouteToll(const CcSim *sim, const CcRoute *route);

int32_t CcRouteSettlementMonsterPressure(const CcSim *sim, CcId settlement_id);

bool CcRouteRoyalIsOfficial(const CcSim *sim, const CcRoute *route);
bool CcRouteRoyalCanReopen(const CcSim *sim, const CcRoute *route);

CcMoney CcRouteRoyalTradeToll(const CcSim *sim, const CcRoute *route, CcId carriage_kingdom_id);

bool CcRouteCarriageCanUse(const CcSim *sim, const CcRoyalCarriage *carriage, CcId route_id);

#endif
