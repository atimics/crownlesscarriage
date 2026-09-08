#include "sim/cc_sim.h"
#include "sim/cc_food_economy_internal.h"
#include "test_support.h"
#include <string.h>

static CcSim sim, before;
static void Fixture(void)
{
    CcSimInit(&sim, 42U);
    sim.settlement_count = 3; sim.route_count = 2; sim.shipment_count = 0;
    sim.royal_carriage_count = 1; sim.royal_trade_week = sim.current_day / 7;
    memset(sim.royal_route_slots_used, 0, sizeof(sim.royal_route_slots_used));
    sim.bandit_count = 0; sim.monster_count = 0;
    sim.iron_ledger_reserve = 100; sim.archives.scribes = 1;
    for (int i = 0; i < 3; ++i) {
        CcSettlement *town = &sim.settlements[i];
        town->kingdom_id = sim.kingdoms[0].id; town->population = 100;
        memset(town->stock, 0, sizeof(town->stock));
        memset(town->reserve_target, 0, sizeof(town->reserve_target));
        town->stock[CC_GOOD_FOOD] = 10000; town->stock[CC_GOOD_WHEAT] = 10000;
        town->stock[CC_GOOD_PAPER] = 10; town->stock[CC_GOOD_TOOLS] = 10;
        town->price[CC_GOOD_PAPER] = 3; town->price[CC_GOOD_TOOLS] = 5;
        town->price[CC_GOOD_WHEAT] = 2;
    }
    (void)snprintf(sim.settlements[0].name, sizeof(sim.settlements[0].name), "Gloamgate");
    sim.settlements[0].stock[CC_GOOD_PAPER] = 0;
    for (int i = 0; i < 2; ++i) {
        CcId id = sim.routes[i].id;
        sim.routes[i] = (CcRoute){.id = id, .from_id = sim.settlements[i + 1].id,
            .to_id = sim.settlements[0].id, .capacity = 12, .condition = 100,
            .security = 100, .travel_days = 2};
    }
    CcId id = sim.royal_carriages[0].id;
    sim.royal_carriages[0] = (CcRoyalCarriage){.id = id, .kingdom_id = sim.kingdoms[0].id,
        .location_id = sim.settlements[1].id, .condition = 100, .mode = CC_ROYAL_CARRIAGE_IDLE};
    for (int i = 0; i < sim.kingdom_count; ++i)
        for (int j = 0; j < sim.kingdom_count; ++j) sim.diplomacy[i][j] = CC_DIPLOMACY_ALLIANCE;
}
static CcArchiveSupplyPlan Query(CcArchiveSupplyGate gate)
{
    before = sim;
    CcArchiveSupplyPlan plan = CcSimArchiveSupplyPlan(&sim, sim.royal_carriages[0].id);
    CC_CHECK(memcmp(&before, &sim, sizeof(sim)) == 0);
    CC_CHECK(plan.gate == gate);
    return plan;
}
int main(void)
{
    Fixture();
    CcArchiveSupplyPlan plan = Query(CC_ARCHIVE_SUPPLY_READY);
    CC_CHECK(plan.source_id == sim.settlements[1].id && plan.seat_id == sim.settlements[0].id);
    CC_CHECK(plan.good == CC_GOOD_PAPER && plan.quantity == 1 && plan.goods_cost == 3);
    CC_CHECK(plan.first_leg_toll == 0 && plan.total_charge == 3 && plan.reposition_cost == 0);
    CC_CHECK(plan.first_route_id == sim.routes[0].id && plan.first_hop_id == plan.seat_id);
    sim.iron_ledger_reserve = 2; (void)Query(CC_ARCHIVE_SUPPLY_FUNDS);
    sim.iron_ledger_reserve = 3; (void)Query(CC_ARCHIVE_SUPPLY_READY);
    sim.settlements[0].stock[CC_GOOD_PAPER] = 1; (void)Query(CC_ARCHIVE_SUPPLY_STOCKED);
    sim.settlements[0].stock[CC_GOOD_PAPER] = 0;
    sim.shipment_count = 1;
    sim.shipments[0] = (CcShipment){.id = 999, .final_destination_id = sim.settlements[0].id,
        .good = CC_GOOD_PAPER, .quantity = 1, .status = CC_SHIPMENT_TRAVELLING};
    (void)Query(CC_ARCHIVE_SUPPLY_INCOMING);
    sim.shipments[0].status = CC_SHIPMENT_ARRIVED;
    (void)Query(CC_ARCHIVE_SUPPLY_READY);
    Fixture(); sim.royal_carriages[0].condition = 19; (void)Query(CC_ARCHIVE_SUPPLY_CARRIAGE);
    Fixture(); sim.royal_carriages[0].next_dispatch_day = sim.current_day + 1; (void)Query(CC_ARCHIVE_SUPPLY_CARRIAGE);
    Fixture(); sim.royal_carriages[0].active_shipment_id = 999; (void)Query(CC_ARCHIVE_SUPPLY_CARRIAGE);
    Fixture(); sim.royal_carriages[0].kingdom_id = sim.kingdoms[1].id; (void)Query(CC_ARCHIVE_SUPPLY_CARRIAGE);
    Fixture();
    for (int i = 1; i < 3; ++i) sim.settlements[i].reserve_target[CC_GOOD_PAPER] = 10;
    (void)Query(CC_ARCHIVE_SUPPLY_SOURCE);
    Fixture();
    for (int i = 0; i < 2; ++i) sim.routes[i].condition = 0;
    (void)Query(CC_ARCHIVE_SUPPLY_ROUTE);
    Fixture(); sim.royal_route_slots_used[0] = 12;
    plan = Query(CC_ARCHIVE_SUPPLY_READY); CC_CHECK(plan.source_id == sim.settlements[2].id);
    sim.royal_route_slots_used[1] = 12; (void)Query(CC_ARCHIVE_SUPPLY_ROUTE);
    sim.royal_trade_week -= 1;
    plan = Query(CC_ARCHIVE_SUPPLY_READY); CC_CHECK(plan.source_id == sim.settlements[1].id);
    Fixture(); sim.routes[0].closed = true;
    plan = Query(CC_ARCHIVE_SUPPLY_READY);
    CC_CHECK(plan.source_id == sim.settlements[1].id && plan.path_capacity == 6 && plan.first_leg_toll == 4);
    Fixture(); sim.settlements[1].kingdom_id = sim.kingdoms[1].id;
    plan = Query(CC_ARCHIVE_SUPPLY_READY); CC_CHECK(plan.first_leg_toll == 1);
    sim.diplomacy[0][1] = sim.diplomacy[1][0] = CC_DIPLOMACY_WAR;
    (void)Query(CC_ARCHIVE_SUPPLY_ROUTE); /* The carriage must also reach the alternate supplier. */
    Fixture(); sim.settlements[0].stock[CC_GOOD_WHEAT] = 0;
    plan = Query(CC_ARCHIVE_SUPPLY_READY);
    CC_CHECK(plan.good == CC_GOOD_WHEAT && plan.quantity == 2 && plan.goods_cost == 4);
    sim.iron_ledger_reserve = 2;
    plan = Query(CC_ARCHIVE_SUPPLY_READY); CC_CHECK(plan.quantity == 1);
    Fixture(); sim.settlements[0].stock[CC_GOOD_TOOLS] = 0;
    plan = Query(CC_ARCHIVE_SUPPLY_READY); CC_CHECK(plan.good == CC_GOOD_TOOLS);
    Fixture();
    sim.settlements[0].stock[CC_GOOD_FOOD] = 0;
    sim.settlements[0].stock[CC_GOOD_WHEAT] = 0;
    sim.iron_ledger_reserve = 10000;
    plan = Query(CC_ARCHIVE_SUPPLY_READY);
    CC_CHECK(plan.good == CC_GOOD_WHEAT && plan.quantity > 2);
    /* Food already on the road protects the archive's wheat for its own work. */
    sim.shipment_count = 2;
    sim.shipments[0] = (CcShipment){.id = 999, .final_destination_id = sim.settlements[0].id,
        .good = CC_GOOD_FOOD, .quantity = 10000, .status = CC_SHIPMENT_TRAVELLING};
    sim.shipments[1] = (CcShipment){.id = 998, .final_destination_id = sim.settlements[0].id,
        .good = CC_GOOD_WHEAT, .quantity = 2, .status = CC_SHIPMENT_TRAVELLING};
    plan = Query(CC_ARCHIVE_SUPPLY_READY); CC_CHECK(plan.good == CC_GOOD_PAPER);
    Fixture(); sim.settlements[0].stock[CC_GOOD_WHEAT] = 0;
    for (int i = 1; i < 3; ++i)
        sim.settlements[i].stock[CC_GOOD_WHEAT] = 6 * CcEconomyWeeklyFoodUse(&sim, &sim.settlements[i]);
    (void)Query(CC_ARCHIVE_SUPPLY_SOURCE);
    Fixture(); sim.royal_carriages[0].location_id = sim.settlements[0].id;
    plan = Query(CC_ARCHIVE_SUPPLY_READY);
    CcId chosen = plan.source_id;
    CcSettlement swap = sim.settlements[1];
    sim.settlements[1] = sim.settlements[2]; sim.settlements[2] = swap;
    plan = Query(CC_ARCHIVE_SUPPLY_READY); CC_CHECK(plan.source_id == chosen);
    CC_CHECK(plan.reposition_cost > 0 && plan.first_dispatch_day == 28);
    Fixture(); sim.schema_version = 57U; (void)Query(CC_ARCHIVE_SUPPLY_UNAVAILABLE);
    Fixture();
    for (int i = 0; i < 3; ++i) sim.settlements[i].population = 0;
    (void)Query(CC_ARCHIVE_SUPPLY_SEAT);
    CC_CHECK(CcSimArchiveSupplyPlan(NULL, 0).gate == CC_ARCHIVE_SUPPLY_UNAVAILABLE);
    CC_CHECK(strcmp(CcArchiveSupplyGateName(CC_ARCHIVE_SUPPLY_READY), "ready") == 0);
    CC_CHECK(strcmp(CcArchiveSupplyGateName((CcArchiveSupplyGate)99), "unknown") == 0);
    puts("Archive supply booking fixtures passed.");
    return 0;
}
