#include "sim/cc_trade_path_internal.h"
#include "test_support.h"
#include <string.h>
static CcSim sim, before;
static void Fixture(void)
{
    CcSimInit(&sim, 42U);
    sim.settlement_count = 4;
    sim.route_count = 4;
    sim.bandit_count = 0;
    sim.monster_count = 0;
    for (int i = 0; i < 4; ++i) {
        sim.settlements[i].population = 100;
        sim.settlements[i].kingdom_id = sim.kingdoms[0].id;
    }
    const int from[] = {0, 1, 0, 2}, to[] = {1, 3, 2, 3};
    const int days[] = {2, 2, 1, 3}, capacity[] = {12, 12, 8, 8};
    for (int i = 0; i < 4; ++i) {
        CcId id = sim.routes[i].id;
        sim.routes[i] = (CcRoute){.id = id, .from_id = sim.settlements[from[i]].id,
            .to_id = sim.settlements[to[i]].id, .travel_days = days[i],
            .capacity = capacity[i], .condition = 100, .security = 100};
    }
    for (int i = 0; i < sim.kingdom_count; ++i)
        for (int j = 0; j < sim.kingdom_count; ++j)
            sim.diplomacy[i][j] = CC_DIPLOMACY_ALLIANCE;
}
static void Path(int slots, bool borders, CcId owner, bool ignore,
                 const int32_t *used, int expected_route, int expected_cost, int expected_capacity)
{
    int32_t route = -7, cost = -7, capacity = -7;
    CcId hop = 7;
    before = sim;
    bool found = CcTradeFindPath(&sim, sim.settlements[0].id, sim.settlements[3].id,
        CC_GOOD_TOOLS, &route, &hop, &cost, &capacity, used, borders, owner, ignore, slots);
    CC_CHECK(memcmp(&before, &sim, sizeof(sim)) == 0);
    CC_CHECK(found == (expected_route >= 0));
    if (found) {
        CC_CHECK(route == expected_route && cost == expected_cost && capacity == expected_capacity);
        CC_CHECK(hop == sim.routes[route].to_id);
    } else {
        CC_CHECK(route == -7 && cost == -7 && capacity == -7 && hop == 7);
    }
}
int main(void)
{
    Fixture();
    /* Equal costs prefer the larger available bottleneck, even when found later. */
    Path(1, true, 0, false, NULL, 0, 40, 12);
    Path(13, true, 0, false, NULL, -1, 0, 0);
    int32_t used[CC_MAX_ROUTES] = {0};
    used[0] = 6;
    Path(1, true, 0, false, used, 2, 40, 8);
    used[2] = 8;
    Path(1, true, 0, false, used, 0, 40, 6);
    Path(7, true, 0, false, used, -1, 0, 0);
    Fixture();
    sim.routes[0].closed = true;
    Path(1, true, 0, false, NULL, 2, 40, 8);
    memset(used, 0, sizeof(used)); used[2] = 8;
    Path(1, true, 0, false, used, 0, 48, 6);
    sim.routes[0].closed = false; sim.routes[0].condition = 0;
    Path(1, true, 0, false, used, -1, 0, 0);
    sim.schema_version = 72;
    Path(1, true, 0, false, used, 0, 52, 3);
    Fixture();
    sim.settlements[1].population = 0;
    Path(1, true, 0, false, NULL, 2, 40, 8);
    sim.settlements[2].population = 0;
    Path(1, true, 0, false, NULL, -1, 0, 0);
    Fixture();
    sim.settlements[3].kingdom_id = sim.kingdoms[1].id;
    Path(1, false, 0, false, NULL, -1, 0, 0);
    Path(1, true, sim.kingdoms[0].id, false, NULL, 0, 40, 12);
    sim.diplomacy[0][1] = sim.diplomacy[1][0] = CC_DIPLOMACY_WAR;
    Path(1, true, sim.kingdoms[0].id, false, NULL, -1, 0, 0);
    Path(1, true, 0, false, NULL, 0, 65, 6);
    Path(1, true, sim.kingdoms[0].id, true, NULL, 0, 65, 6);
    sim.routes[1].smuggler_route = true; sim.routes[3].smuggler_route = true;
    Path(1, true, sim.kingdoms[0].id, true, NULL, -1, 0, 0);
    puts("Verified alternate freight paths, bottlenecks, closures, ruins, borders, and royal permissions");
    return 0;
}
