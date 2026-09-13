#define main CcRunnerFixtureMain
#include "../tools/sim_runner.c"
#undef main
#include "test_support.h"
static CcSim state, before;
static void Emit(void)
{
    before = state;
    JsonRoadNetwork(&state); putchar('\n');
    CC_CHECK(memcmp(&state, &before, sizeof(state)) == 0);
}
int main(void)
{
    CcSimInit(&state, 42U);
    state.settlement_count = 4;
    state.route_count = 4;
    for (int32_t i = 0; i < 4; ++i) {
        state.settlements[i].population = 100;
        state.routes[i].from_id = state.settlements[i].id;
        state.routes[i].to_id = state.settlements[(i + 1) % 4].id;
        state.routes[i].closed = false;
    }
    Emit(); /* A cycle. */
    state.routes[0].closed = true; Emit(); /* Alternate path remains. */
    state.routes[2].closed = true; Emit(); /* Two pairs. */
    state.routes[0].closed = false;
    state.routes[3].closed = true;
    state.settlements[1].population = 0; Emit(); /* A ruin connects two towns. */
    for (int32_t i = 0; i < 4; ++i) state.settlements[i].population = 0;
    Emit(); /* All ruins. */
    state.settlements[3].population = 100; Emit(); /* One inhabited town. */
    CcSettlement swap = state.settlements[0];
    state.settlements[0] = state.settlements[3]; state.settlements[3] = swap;
    Emit(); /* Component identity survives storage reordering. */
    state.settlement_count = 0; state.route_count = 0; Emit();
    return 0;
}
