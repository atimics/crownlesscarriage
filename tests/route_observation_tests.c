#define main MetricsMain
#include "../tools/sim_metrics.c"
#undef main
#include "test_support.h"
static CcSim sim, before;
static RouteObservation rows[CC_MAX_ROUTES];
static void Observe(void)
{
    before = sim; ObserveRoutes(&sim, rows);
    CC_CHECK(memcmp(&before, &sim, sizeof(sim)) == 0);
}
int main(void)
{
    CcSimInit(&sim, 123U); sim.route_count = 1;
    CcRoute *route = &sim.routes[0];
    CcSettlement *from = CcSimSettlementMutable(&sim, route->from_id);
    CcSettlement *to = CcSimSettlementMutable(&sim, route->to_id);
    from->kingdom_id = sim.kingdoms[0].id; to->kingdom_id = sim.kingdoms[1].id;
    sim.diplomacy[0][1] = CC_DIPLOMACY_ALLIANCE;
    sim.diplomacy[1][0] = CC_DIPLOMACY_ALLIANCE;
    route->closed = false; route->smuggler_route = false; Observe();
    route->closed = true; Observe();
    route->closed = false;
    sim.diplomacy[0][1] = CC_DIPLOMACY_WAR;
    sim.diplomacy[1][0] = CC_DIPLOMACY_WAR; Observe();
    to->population = 0; Observe();
    CC_CHECK(rows[0].current_outage_days == 3);
    route->smuggler_route = true; Observe();
    CC_CHECK(rows[0].current_outage_days == 0);
    route->closed = true; Observe();
    CC_CHECK(rows[0].sampled_days == 6);
    CC_CHECK(rows[0].closed_days == 2 && rows[0].war_border_days == 4);
    CC_CHECK(rows[0].unavailable_inhabited_days == 2);
    CC_CHECK(rows[0].unavailable_ruin_days == 2);
    CC_CHECK(rows[0].current_outage_days == 1 && rows[0].longest_outage_days == 3);
    return 0;
}
