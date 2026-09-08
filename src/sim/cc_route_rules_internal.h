#ifndef CROWNLESS_ROUTE_RULES_INTERNAL_H
#define CROWNLESS_ROUTE_RULES_INTERNAL_H

#include "sim/cc_sim.h"

/* Shared route costs and threats for journey planning and trade. */
int32_t CcRouteDragonShadowDanger(const CcSim *sim, const CcRoute *route);
CcMoney CcRouteToll(const CcSim *sim, const CcRoute *route);

#endif
