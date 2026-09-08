#define main CcRunnerFixtureMain
#include "../tools/sim_runner.c"
#undef main
#define main CcRoadPlanFixtureMain
#include "road_recovery_plan_tests.c"
#undef main
static void Emit(CcId route)
{
    before = sim;
    JsonRoadRecovery(&sim, route); putchar('\n');
    CC_CHECK(memcmp(&before, &sim, sizeof(sim)) == 0);
}
int main(void)
{
    Fixture(); Emit(sim.routes[0].id);
    const CcGood goods[] = {CC_GOOD_BREAD, CC_GOOD_WOOD, CC_GOOD_STONE, CC_GOOD_TOOLS};
    for (unsigned i = 0; i < sizeof(goods) / sizeof(goods[0]); ++i) {
        Fixture();
        CcSimSettlementMutable(&sim, sim.routes[0].from_id)->stock[goods[i]] = 0;
        Emit(sim.routes[0].id);
    }
    Fixture(); Emit(sim.routes[0].id);
    sim.current_day = 111; Emit(sim.routes[0].id);
    sim.current_day = 112; sim.routes[0].closed = false; Emit(sim.routes[0].id);
    Emit(0);
    return 0;
}
