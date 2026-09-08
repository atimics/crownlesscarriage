#include "sim/cc_sim.h"
#include "test_support.h"
#include <string.h>
#include "../tools/sim_runner_road_observation.inc"

static CcSim sim, before;

static void Sample(RoadObservation *observation, bool closed, int32_t population)
{
    sim.routes[0].closed = closed;
    CcSimSettlementMutable(&sim, sim.routes[0].to_id)->population = population;
    before = sim;
    ObserveRoadDay(&sim, &sim.routes[0], observation);
    CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
}

int main(void)
{
    CcSimInit(&sim, 42U);
    CcSimSettlementMutable(&sim, sim.routes[0].from_id)->population = 100;
    RoadObservation observation = {0};
    /* Two closed samples, one open sample, then a longer closure through abandonment. */
    Sample(&observation, true, 100);
    Sample(&observation, true, 100);
    CC_CHECK(observation.current_closed_days == 2 && observation.longest_closed_days == 2);
    Sample(&observation, false, 100);
    CC_CHECK(observation.current_closed_days == 0 && observation.longest_closed_days == 2);
    Sample(&observation, true, 100);
    Sample(&observation, true, 0);
    Sample(&observation, true, 0);
    CC_CHECK(observation.current_closed_days == 3 && observation.longest_closed_days == 3);
    CC_CHECK(observation.open_days == 1 && observation.closed_inhabited_days == 3);
    Sample(&observation, false, 0);
    CC_CHECK(observation.open_days == 2 && observation.current_closed_days == 0);
    CC_CHECK(observation.longest_closed_days == 3 && observation.closed_inhabited_days == 3);
    /* A fresh run starts its observed closure duration at its first sample. */
    observation = (RoadObservation){0};
    Sample(&observation, true, 100);
    CC_CHECK(observation.current_closed_days == 1 && observation.longest_closed_days == 1);
    CC_CHECK(observation.open_days == 0 && observation.closed_inhabited_days == 1);
    sim.routes[0].from_id = 0;
    Sample(&observation, true, 100);
    CC_CHECK(observation.current_closed_days == 2 && observation.closed_inhabited_days == 1);
    return 0;
}
