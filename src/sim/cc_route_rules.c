#include "sim/cc_route_rules_internal.h"

static int32_t MaximumI32(int32_t a, int32_t b) { return a > b ? a : b; }

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

int32_t CcRouteSettlementMonsterPressure(const CcSim *sim, CcId settlement_id)
{
    int32_t pressure = 0;
    for (int32_t i = 0; i < sim->dungeon_count; ++i) {
        if (sim->dungeons[i].settlement_id != settlement_id) continue;
        for (int32_t monster = 0; monster < sim->monster_count; ++monster) {
            if (sim->monsters[monster].dungeon_id == sim->dungeons[i].id) {
                pressure = MaximumI32(pressure, sim->monsters[monster].pressure);
            }
        }
    }
    return pressure;
}


bool CcRouteRoyalIsOfficial(const CcSim *sim, const CcRoute *route)
{
    if (sim == NULL || route == NULL) return false;
    if (sim->schema_version >= 73U && route->condition == 0) return false;
    return !(route->smuggler_route &&
             CcSimRouteCrossesKingdomBorder(sim, route->id)) &&
           !CcSimRouteCrossesWarBorder(sim, route->id);
}

bool CcRouteRoyalCanReopen(const CcSim *sim, const CcRoute *route)
{
    if (sim == NULL || route == NULL) return false;
    return !(route->smuggler_route &&
             CcSimRouteCrossesKingdomBorder(sim, route->id));
}


CcMoney CcRouteRoyalTradeToll(const CcSim *sim, const CcRoute *route,
                                   CcId carriage_kingdom_id)
{
    CcMoney toll = CcRouteToll(sim, route);
    const CcSettlement *from = route != NULL ?
        CcSimSettlement(sim, route->from_id) : NULL;
    const CcSettlement *to = route != NULL ?
        CcSimSettlement(sim, route->to_id) : NULL;
    if (from == NULL || to == NULL ||
        from->kingdom_id == to->kingdom_id) return toll;
    CcId host = from->kingdom_id == carriage_kingdom_id ?
        to->kingdom_id : from->kingdom_id;
    toll += CcSimKingdomsAllied(sim, carriage_kingdom_id, host) ? 1 : 3;
    return toll;
}

