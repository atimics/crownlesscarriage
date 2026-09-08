#include "sim/cc_sim.h"
#include "test_support.h"
#include <string.h>

static CcSim sim, before, blocked;

static void Fixture(void)
{
    CcSimInit(&sim, 123U);
    sim.current_day = 112;
    CcRoute *route = &sim.routes[0];
    route->closed = true; route->condition = 40; route->smuggler_route = false;
    for (int i = 0; i < sim.kingdom_count; ++i) {
        sim.kingdoms[i].treasury = 0;
        for (int j = 0; j < sim.kingdom_count; ++j)
            sim.diplomacy[i][j] = CC_DIPLOMACY_ALLIANCE;
    }
    CcSettlement *from = CcSimSettlementMutable(&sim, route->from_id);
    CcSettlement *to = CcSimSettlementMutable(&sim, route->to_id);
    from->population = 300; to->population = 100;
    from->hunger = 0; to->hunger = 0;
    memset(from->stock, 0, sizeof(from->stock));
    memset(to->stock, 0, sizeof(to->stock));
    from->stock[CC_GOOD_BREAD] = 20;
    from->stock[CC_GOOD_WOOD] = 2;
    from->stock[CC_GOOD_STONE] = 2;
    from->stock[CC_GOOD_TOOLS] = 1;
}

int main(void)
{
    Fixture();
    before = sim;
    CcRoadRecoveryPlan plan = CcSimRoadRecoveryPlan(&sim, sim.routes[0].id);
    CC_CHECK(plan.blocked == 0U);
    CC_CHECK(plan.labor_base_id == sim.routes[0].from_id);
    CC_CHECK(plan.supplier_id == sim.routes[0].from_id);
    CC_CHECK(plan.population == 300 && plan.food_rations == 20);
    CC_CHECK(plan.effort == 6 && plan.people_used == 1 && plan.next_work_day == 112);
    CC_CHECK(memcmp(&before, &sim, sizeof(sim)) == 0);
    CcSettlement *from = CcSimSettlementMutable(&sim, sim.routes[0].from_id);
    CcSettlement *to = CcSimSettlementMutable(&sim, sim.routes[0].to_id);
    const CcGood goods[] = {CC_GOOD_BREAD, CC_GOOD_WOOD, CC_GOOD_STONE, CC_GOOD_TOOLS};
    const uint32_t masks[] = {CC_ROAD_RECOVERY_FOOD, CC_ROAD_RECOVERY_WOOD,
        CC_ROAD_RECOVERY_STONE, CC_ROAD_RECOVERY_TOOLS};
    for (unsigned i = 0; i < sizeof(goods) / sizeof(goods[0]); ++i) {
        int32_t saved = from->stock[goods[i]];
        from->stock[goods[i]] = 0;
        plan = CcSimRoadRecoveryPlan(&sim, sim.routes[0].id);
        CC_CHECK((plan.blocked & masks[i]) != 0U);
        from->stock[goods[i]] = saved;
        CC_CHECK(CcSimRoadRecoveryPlan(&sim, sim.routes[0].id).blocked == 0U);
    }
    /* Supplier and labor selection remain separate, with the from endpoint winning ties. */
    memcpy(to->stock, from->stock, sizeof(to->stock));
    to->stock[CC_GOOD_BREAD] += 1;
    plan = CcSimRoadRecoveryPlan(&sim, sim.routes[0].id);
    CC_CHECK(plan.supplier_id == to->id && plan.labor_base_id == from->id);
    to->stock[CC_GOOD_BREAD] -= 1;
    CC_CHECK(CcSimRoadRecoveryPlan(&sim, sim.routes[0].id).supplier_id == from->id);
    memset(to->stock, 0, sizeof(to->stock));
    from->population = 219;
    CC_CHECK(CcSimRoadRecoveryPlan(&sim, sim.routes[0].id).blocked == CC_ROAD_RECOVERY_PEOPLE);
    from->population = 300; to->population = 0;
    CC_CHECK(CcSimRoadRecoveryPlan(&sim, sim.routes[0].id).blocked == CC_ROAD_RECOVERY_ABANDONED);
    to->population = 100; sim.current_day = 111;
    plan = CcSimRoadRecoveryPlan(&sim, sim.routes[0].id);
    CC_CHECK(plan.blocked == CC_ROAD_RECOVERY_CALENDAR && plan.next_work_day == 112);
    sim.current_day = 112; sim.routes[0].closed = false;
    CC_CHECK(CcSimRoadRecoveryPlan(&sim, sim.routes[0].id).blocked == CC_ROAD_RECOVERY_OPEN);
    CC_CHECK(CcSimRoadRecoveryPlan(NULL, 0U).blocked == CC_ROAD_RECOVERY_INVALID);
    CC_CHECK(CcSimRoadRecoveryPlan(NULL, 0U).population == -1);

    /* The real daily loop reopens a supplied road when the war border is lifted. */
    Fixture(); sim.current_day = 111;
    CcSimSettlementMutable(&sim, sim.routes[0].from_id)->stock[CC_GOOD_STONE] = 4;
    CcSimSettlementMutable(&sim, sim.routes[0].from_id)->kingdom_id = sim.kingdoms[0].id;
    CcSimSettlementMutable(&sim, sim.routes[0].to_id)->kingdom_id = sim.kingdoms[1].id;
    blocked = sim;
    blocked.diplomacy[0][1] = CC_DIPLOMACY_WAR;
    blocked.diplomacy[1][0] = CC_DIPLOMACY_WAR;
    CC_CHECK((CcSimRoadRecoveryPlan(&blocked, blocked.routes[0].id).blocked &
        CC_ROAD_RECOVERY_WAR) != 0U);
    CcSimAdvanceDays(&sim, 1);
    CcSimAdvanceDays(&blocked, 1);
    CC_CHECK(!sim.routes[0].closed);
    CC_CHECK(blocked.routes[0].closed);
    CC_CHECK(sim.routes[0].condition >= 45);
    CC_CHECK(CcSimSettlement(&sim, sim.routes[0].from_id)->stock[CC_GOOD_TOOLS] == 0);
    /* A complete repair kit wins over a larger but incomplete stockpile. */
    Fixture();
    from = CcSimSettlementMutable(&sim, sim.routes[0].from_id);
    to = CcSimSettlementMutable(&sim, sim.routes[0].to_id);
    memset(from->stock, 0, sizeof(from->stock));
    from->stock[CC_GOOD_WOOD] = 1000;
    to->stock[CC_GOOD_BREAD] = 40;
    to->stock[CC_GOOD_WOOD] = 8;
    to->stock[CC_GOOD_STONE] = 8;
    to->stock[CC_GOOD_TOOLS] = 4;
    plan = CcSimRoadRecoveryPlan(&sim, sim.routes[0].id);
    CC_CHECK(plan.blocked == 0U && plan.supplier_id == to->id);
    CC_CHECK(plan.labor_base_id == from->id);
    sim.schema_version = 59U;
    plan = CcSimRoadRecoveryPlan(&sim, sim.routes[0].id);
    CC_CHECK(plan.supplier_id == from->id && plan.blocked != 0U);
    sim.schema_version = CC_SIM_SCHEMA_VERSION;
    sim.current_day = 111;
    blocked = sim; blocked.schema_version = 59U;
    CcSimAdvanceDays(&sim, 1); CcSimAdvanceDays(&blocked, 1);
    CC_CHECK(!sim.routes[0].closed && blocked.routes[0].closed);
    CC_CHECK(CcSimSettlement(&sim, sim.routes[0].to_id)->stock[CC_GOOD_TOOLS] <
        CcSimSettlement(&blocked, blocked.routes[0].to_id)->stock[CC_GOOD_TOOLS]);
    /* Paid crews also use the owned endpoint that has their Wood and Stone. */
    Fixture();
    for (int32_t i = 1; i < sim.route_count; ++i) {
        sim.routes[i].closed = false; sim.routes[i].condition = 100;
    }
    from = CcSimSettlementMutable(&sim, sim.routes[0].from_id);
    to = CcSimSettlementMutable(&sim, sim.routes[0].to_id);
    from->kingdom_id = sim.kingdoms[0].id; to->kingdom_id = sim.kingdoms[0].id;
    memset(from->stock, 0, sizeof(from->stock));
    memset(to->stock, 0, sizeof(to->stock));
    from->stock[CC_GOOD_WOOD] = 1000;
    to->stock[CC_GOOD_WOOD] = 8; to->stock[CC_GOOD_STONE] = 8;
    sim.kingdoms[0].treasury = 1000;
    sim.current_day = 27;
    blocked = sim; blocked.schema_version = 59U;
    CcSimAdvanceDays(&sim, 1); CcSimAdvanceDays(&blocked, 1);
    CC_CHECK(!sim.routes[0].closed && blocked.routes[0].closed);
    CC_CHECK(CcSimSettlement(&sim, sim.routes[0].to_id)->stock[CC_GOOD_STONE] ==
        CcSimSettlement(&blocked, blocked.routes[0].to_id)->stock[CC_GOOD_STONE] - 2);
    CC_CHECK(CcSimTrackedGold(&sim) == CcSimTrackedGold(&blocked));
    return 0;
}
