#include "sim/cc_production.h"
#include "test_support.h"
#include <string.h>
static CcSim sim, split, plain;
static CcProductionAccounting totals, daily;

static uint64_t GateCount(const CcRecipeAccounting *recipe)
{
    uint64_t total = 0;
    for (int32_t gate = 0; gate < CC_PRODUCTION_GATE_COUNT; ++gate) total += recipe->gates[gate];
    return total;
}

static void CheckYear(uint32_t seed, uint32_t schema)
{
    CcSimInit(&sim, seed);
    sim.schema_version = schema;
    memset(&totals, 0, sizeof(totals)); memset(&daily, 0, sizeof(daily));
    split = sim; plain = sim;
    CcSimAdvanceDaysWithProductionAccounting(&sim, 365, NULL, NULL, &totals);
    for (int32_t day = 0; day < 365; ++day)
        CcSimAdvanceDaysWithProductionAccounting(&split, 1, NULL, NULL, &daily);
    CcSimAdvanceDays(&plain, 365);
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&split) && CcSimHash(&sim) == CcSimHash(&plain));
    CC_CHECK(memcmp(&totals, &daily, sizeof(totals)) == 0);
    uint64_t bread = 0;
    for (int32_t i = 0; i < sim.settlement_count; ++i) {
        const CcTownProductionAccounting *row = &totals.towns[i];
        const CcSettlement *town = &sim.settlements[i];
        CC_CHECK(row->settlement_id == town->id && row->active_weeks + row->inactive_weeks == 52);
        CC_CHECK(GateCount(&row->bakery) == row->active_weeks);
        CC_CHECK(GateCount(&row->paper) == row->active_weeks);
        CC_CHECK(GateCount(&row->treasure) == row->active_weeks);
        CC_CHECK(row->bakery.input[CC_GOOD_WHEAT] == row->bakery.output[CC_GOOD_BREAD]);
        CC_CHECK(row->bakery.work == row->bakery.output[CC_GOOD_BREAD]);
        CcGood paper_input = schema >= 34 && schema < 37 ? CC_GOOD_WHEAT : CC_GOOD_WOOD;
        CC_CHECK(row->paper.work == row->paper.input[paper_input]);
        CC_CHECK(row->paper.output[CC_GOOD_PAPER] <= row->paper.work * 4);
        CC_CHECK(row->paper.output[CC_GOOD_PAPER] >= row->paper.work);
        CC_CHECK(row->paper.tools_worn <= row->paper.gates[CC_PRODUCTION_READY] / 8);
        CC_CHECK(row->treasure.input[CC_GOOD_GOLD] == row->treasures_completed + (uint64_t)town->treasure_gold_committed);
        CC_CHECK(row->treasure.input[CC_GOOD_GEMS] == row->treasures_completed + (uint64_t)town->treasure_gems_committed);
        CC_CHECK(row->treasure.work == row->treasures_completed * 3 + (uint64_t)town->treasure_work);
        bread += row->bakery.output[CC_GOOD_BREAD];
    }
    CC_CHECK(bread > 0);
}

static void CheckPrimary(void)
{
    CcSimInit(&sim, 42);
    memset(&totals, 0, sizeof(totals));
    sim.current_day = 6;
    for (int32_t i = 0; i < sim.settlement_count; ++i) {
        sim.settlements[i].stock[CC_GOOD_WOOD] = CC_SIM_MAX_UNITS;
        sim.settlements[i].production[CC_GOOD_WOOD] = 10;
    }
    CcSettlement *mine = NULL;
    for (int32_t i = 0; i < sim.settlement_count; ++i)
        if (sim.settlements[i].function == CC_SETTLEMENT_MINING) mine = &sim.settlements[i];
    CC_CHECK(mine != NULL);
    mine->gold_seam = true; mine->gem_seam = true;
    mine->gold_progress = 11; mine->gem_progress = 47; mine->stock[CC_GOOD_TOOLS] = 10;
    CcSimAdvanceDaysWithProductionAccounting(&sim, 1, NULL, NULL, &totals);
    uint64_t cap_loss = 0;
    for (int32_t i = 0; i < sim.settlement_count; ++i) {
        CC_CHECK(totals.towns[i].primary_output[CC_GOOD_WOOD] == 0);
        cap_loss += totals.towns[i].primary_cap_loss[CC_GOOD_WOOD];
    }
    CC_CHECK(cap_loss > 0);
    const CcTownProductionAccounting *row = &totals.towns[mine - sim.settlements];
    CC_CHECK(row->rare_mine_output[CC_GOOD_GOLD] == 1 && row->rare_mine_output[CC_GOOD_GEMS] == 1);
}

static void CheckFundedRecipes(void)
{
    CcSimInit(&sim, UINT32_C(0x5eed0001));
    memset(&totals, 0, sizeof(totals));
    sim.current_day = 6;
    CcSettlement *town = &sim.settlements[1];
    CC_CHECK(town->function == CC_SETTLEMENT_MARKET);
    town->service_mask |= (UINT32_C(1) << CC_SERVICE_MILL) | (UINT32_C(1) << CC_SERVICE_SMITHY);
    town->hunger = 0;
    town->stock[CC_GOOD_BREAD] = 300; town->stock[CC_GOOD_WHEAT] = 100;
    town->stock[CC_GOOD_WOOD] = 20; town->stock[CC_GOOD_TOOLS] = 5;
    town->stock[CC_GOOD_GOLD] = 1; town->stock[CC_GOOD_GEMS] = 1;
    town->production[CC_GOOD_PAPER] = 3;
    town->reserve_target[CC_GOOD_PAPER] = 100; town->reserve_target[CC_GOOD_WOOD] = 1;
    town->paper_tool_wear = 7;
    CcSimAdvanceDaysWithProductionAccounting(&sim, 1, NULL, NULL, &totals);
    const CcTownProductionAccounting *row = &totals.towns[1];
    CC_CHECK(row->paper.work == 1 && row->paper.input[CC_GOOD_WOOD] == 1);
    CC_CHECK(row->paper.output[CC_GOOD_PAPER] == 3 && row->paper.tools_worn == 1);
    CC_CHECK(row->treasure.input[CC_GOOD_GOLD] == 1 && row->treasure.input[CC_GOOD_GEMS] == 1);
    CC_CHECK(row->treasure.work == 1 && row->treasures_completed == 0);
    CcSimAdvanceDaysWithProductionAccounting(&sim, 14, NULL, NULL, &totals);
    CC_CHECK(row->treasure.input[CC_GOOD_GOLD] == 1 && row->treasure.input[CC_GOOD_GEMS] == 1);
    CC_CHECK(row->treasure.work == 3 && row->treasures_completed == 1);
}

int main(void)
{
    CheckYear(UINT32_C(0x5eed0001), CC_SIM_SCHEMA_VERSION);
    CheckYear(UINT32_C(0xc0a71a9e), CC_SIM_SCHEMA_VERSION);
    CheckYear(42, 33); CheckYear(42, 34); CheckYear(42, 36); CheckYear(42, 37);
    CheckPrimary();
    CheckFundedRecipes();
    puts("Town accounting: recipe costs, work orders, tool wear, cap loss, rare seams and observer parity passed");
    return 0;
}
