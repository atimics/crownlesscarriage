#define main CcRunnerFixtureMain
#include "../tools/sim_runner.c"
#undef main
#include "test_support.h"
static CcSim sim, before;
static void Emit(void)
{
    before = sim;
    JsonRetainedHistory(&sim); putchar('\n');
    CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
}
int main(void)
{
    CcSimInit(&sim, 42);
    sim.event_count = 0; sim.event_write_index = 0; Emit();
    memset(sim.events, 0, sizeof(sim.events));
    sim.event_count = 3; sim.event_write_index = 1;
    sim.events[CC_MAX_EVENTS - 2].day = 10;
    sim.events[CC_MAX_EVENTS - 1].day = 3;
    sim.events[0].day = 7;
    Emit();
    sim.event_count = CC_MAX_EVENTS; sim.event_write_index = 127;
    for (int32_t i = 0; i < CC_MAX_EVENTS; ++i) sim.events[i].day = i + 1;
    Emit();
    return 0;
}
