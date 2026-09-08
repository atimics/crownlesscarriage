#include "sim/cc_production.h"
#include "test_support.h"
#include <string.h>
static CcSim sim;
static CcRoadSite *site;
static CcSettlement *town;
static CcRoyalCarriage *carriage;
static CcRoute *route;

static void Prepare(int32_t index)
{
    CcSimInit(&sim, UINT32_C(0x5eed0001));
    site = &sim.road_sites[index];
    town = &sim.settlements[CcSimSettlement(&sim, site->home_settlement_id) - sim.settlements];
    route = &sim.routes[CcSimRoute(&sim, site->route_id) - sim.routes];
    site->accessible = true; site->condition = 100;
    site->blocker = CC_ROAD_SITE_BLOCKER_NONE;
    route->closed = false; route->condition = 100; route->capacity = 12;
    carriage = &sim.royal_carriages[0];
    carriage->location_id = town->id; carriage->kingdom_id = town->kingdom_id;
    carriage->mode = CC_ROYAL_CARRIAGE_IDLE; carriage->active_shipment_id = 0;
    carriage->condition = 100; carriage->next_dispatch_day = sim.current_day;
    for (int32_t i = 0; i < CC_GOOD_COUNT; ++i) town->stock[i] = 10000;
}

static CcSiteFreightPlan Plan(void)
{
    uint64_t hash = CcSimHash(&sim);
    CcSiteFreightPlan plan = CcSimPlanSiteFreight(&sim, carriage->id, site->id);
    CC_CHECK(hash == CcSimHash(&sim));
    return plan;
}

int main(void)
{
    Prepare(2);
    CcSiteFreightPlan plan = Plan();
    CC_CHECK(plan.gate == CC_SITE_FREIGHT_READY && plan.kind == CC_SITE_FREIGHT_SUPPLY);
    CC_CHECK(plan.good == CC_GOOD_TOOLS && plan.quantity == 1);
    CC_CHECK(plan.travel_days == CcSimFreightLegDays(&sim, route->id, town->id, site->id));
    CC_CHECK(plan.return_days == plan.travel_days && plan.town_id == town->id);
    site->stock[CC_GOOD_TOOLS] = 1;
    plan = Plan();
    CC_CHECK(plan.good == CC_GOOD_WHEAT && plan.quantity == 4);
    town->stock[CC_GOOD_WHEAT] = town->reserve_target[CC_GOOD_WHEAT];
    CC_CHECK(Plan().gate == CC_SITE_FREIGHT_GOODS_REQUIRED);
    town->stock[CC_GOOD_WHEAT] = 1; town->reserve_target[CC_GOOD_WHEAT] = 0;
    CC_CHECK(Plan().gate == CC_SITE_FREIGHT_GOODS_REQUIRED);
    site->stock[CC_GOOD_BREAD] = 4;
    plan = Plan();
    CC_CHECK(plan.kind == CC_SITE_FREIGHT_PICKUP && plan.good == CC_GOOD_BREAD && plan.quantity == 4);
    town->stock[CC_GOOD_BREAD] = CC_SIM_MAX_UNITS - 1;
    CC_CHECK(Plan().quantity == 1);
    town->stock[CC_GOOD_BREAD] = CC_SIM_MAX_UNITS;
    CC_CHECK(Plan().gate == CC_SITE_FREIGHT_GOODS_REQUIRED);
    Prepare(2);
    site->stock[CC_GOOD_STONE] = CC_ROAD_SITE_CAPACITY;
    CC_CHECK(Plan().gate == CC_SITE_FREIGHT_GOODS_REQUIRED);
    Prepare(11);
    site->stock[CC_GOOD_TOOLS] = 5;
    plan = Plan();
    CC_CHECK(plan.kind == CC_SITE_FREIGHT_PICKUP && plan.quantity == 4 && plan.good == CC_GOOD_TOOLS);
    sim.royal_trade_week = sim.current_day / 7;
    sim.royal_route_slots_used[route - sim.routes] = 11;
    CC_CHECK(Plan().quantity == 2);
    sim.royal_route_slots_used[route - sim.routes] = 12;
    CC_CHECK(Plan().gate == CC_SITE_FREIGHT_CAPACITY_REQUIRED);
    sim.royal_trade_week -= 1;
    CC_CHECK(Plan().quantity == 4);
    site->stock[CC_GOOD_TOOLS] = 1;
    plan = Plan();
    CC_CHECK(plan.kind == CC_SITE_FREIGHT_SUPPLY && plan.good == CC_GOOD_IRON && plan.quantity == 5);
    site->stock[CC_GOOD_IRON] = 5;
    plan = Plan();
    CC_CHECK(plan.good == CC_GOOD_WOOD && plan.quantity == 3);
    route->closed = true;
    CC_CHECK(Plan().gate == CC_SITE_FREIGHT_ROUTE_REQUIRED);
    route->closed = false; carriage->kingdom_id += 1;
    CC_CHECK(Plan().gate == CC_SITE_FREIGHT_LOCAL_TOWN_REQUIRED);
    carriage->kingdom_id = town->kingdom_id; carriage->location_id = site->id;
    CC_CHECK(Plan().gate == CC_SITE_FREIGHT_LOCAL_TOWN_REQUIRED);
    carriage->location_id = town->id; site->accessible = false;
    CC_CHECK(Plan().gate == CC_SITE_FREIGHT_SITE_REQUIRED);
    site->accessible = true; site->condition = 49;
    CC_CHECK(Plan().gate == CC_SITE_FREIGHT_SITE_REQUIRED);
    site->condition = 100; carriage->active_shipment_id = 1;
    CC_CHECK(Plan().gate == CC_SITE_FREIGHT_CARRIAGE_REQUIRED);
    carriage->active_shipment_id = 0; carriage->next_dispatch_day += 1;
    CC_CHECK(Plan().gate == CC_SITE_FREIGHT_CARRIAGE_REQUIRED);
    CC_CHECK(CcSimPlanSiteFreight(NULL, 0, 0).gate == CC_SITE_FREIGHT_CARRIAGE_REQUIRED);
    puts("Site freight: reserves, working tools, store and road capacity, travel times and readonly plans passed");
    return 0;
}
