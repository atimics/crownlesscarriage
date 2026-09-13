#define main CcRunnerFixtureMain
#include "../tools/sim_runner.c"
#undef main
#define main CcRoadPlanFixtureMain
#include "road_recovery_plan_tests.c"
#undef main
static void Emit(void)
{
    before = sim;
    JsonRoadContext(&sim, &sim.routes[0]); putchar('\n');
    CC_CHECK(memcmp(&before, &sim, sizeof(sim)) == 0);
}
int main(void)
{
    Fixture();
    CcSettlement *from = CcSimSettlementMutable(&sim, sim.routes[0].from_id);
    CcSettlement *to = CcSimSettlementMutable(&sim, sim.routes[0].to_id);
    from->kingdom_id = sim.kingdoms[0].id;
    to->kingdom_id = sim.kingdoms[1].id;
    Emit();
    to->population = 0; Emit();
    from->population = 0; Emit();
    from->population = 300; to->population = 100;
    sim.routes[0].closed = false; Emit();
    sim.diplomacy[0][1] = sim.diplomacy[1][0] = CC_DIPLOMACY_WAR; Emit();
    sim.diplomacy[0][1] = sim.diplomacy[1][0] = CC_DIPLOMACY_ALLIANCE;
    sim.routes[0].smuggler_route = true; Emit();
    sim.routes[0].smuggler_route = false;
    sim.routes[0].condition = 0; Emit();
    sim.routes[0].condition = 40;
    to->kingdom_id = from->kingdom_id; Emit();
    sim.diplomacy[0][1] = sim.diplomacy[1][0] = CC_DIPLOMACY_PEACE;
    for (int32_t i = 0; i < sim.faction_count; ++i)
        if (sim.factions[i].kingdom_id == from->kingdom_id)
            sim.factions[i].support = 0;
    Emit();
    sim.routes[0].to_id = 0; Emit();
    return 0;
}
