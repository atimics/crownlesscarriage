#define main MetricsMain
#include "../tools/sim_metrics.c"
#undef main
#include "test_support.h"

int main(void)
{
    static CcSim sim, before;
    CcSimInit(&sim, 123U);
    CcMetricsHistory history = {0};
    CC_CHECK(sim.bandit_count > 0);
    sim.bandits[1] = sim.bandits[0];
    sim.bandits[1].id += 1U;
    sim.bandit_count = 2;
    for (int i = 0; i < sim.bandit_count; ++i) {
        sim.bandits[i].raid_phase = CC_BANDIT_RAID_IDLE;
        sim.bandits[i].influence = 0;
    }
    sim.bandits[0].raid_phase = CC_BANDIT_RAID_OUTBOUND;
    sim.bandits[1].raid_phase = CC_BANDIT_RAID_OUTBOUND;
    sim.bandits[0].influence = 70; sim.bandits[1].influence = 90;
    before = sim;
    UpdateDailyHistory(&sim, &history); UpdateHistory(&sim, &history);
    CC_CHECK(history.days_bandit_raid == 1 && history.years_bandit_raid == 1);
    CC_CHECK(history.days_bandit_influence_70_plus == 1);
    CC_CHECK(history.years_bandit_influence_70_plus == 1);
    CC_CHECK(history.bandit_raid_group_days == 2);
    CC_CHECK(history.bandit_influence_70_plus_group_days == 2);
    CC_CHECK(history.bandit_raid_group_year_samples == 2);
    CC_CHECK(history.bandit_influence_70_plus_group_year_samples == 2);
    CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
    sim.bandits[0].raid_phase = CC_BANDIT_RAID_IDLE; sim.bandits[0].influence = 69;
    UpdateDailyHistory(&sim, &history); UpdateHistory(&sim, &history);
    CC_CHECK(history.days_bandit_raid == 2 && history.years_bandit_raid == 2);
    CC_CHECK(history.bandit_raid_group_days == 3);
    CC_CHECK(history.bandit_influence_70_plus_group_days == 3);
    CC_CHECK(history.bandit_raid_group_year_samples == 3);
    sim.bandit_count = 0;
    UpdateDailyHistory(&sim, &history); UpdateHistory(&sim, &history);
    CC_CHECK(history.days_bandit_raid == 2 && history.years_bandit_raid == 2);
    CC_CHECK(history.bandit_raid_group_days == 3);
    return 0;
}
