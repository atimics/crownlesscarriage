#define main CcRunnerFixtureMain
#include "../tools/sim_runner.c"
#undef main
#define main CcEnginePlanFixtureMain
#ifdef CC_RITUAL_JSON_FIXTURE
#include "ritual_offering_plan_tests.c"
#else
#include "campaign_launch_plan_tests.c"
#endif
#undef main
static void Emit(void)
{
    control = sim;
#ifdef CC_RITUAL_JSON_FIXTURE
    JsonRitualPlan(&sim);
#else
    JsonCampaignPlan(&sim);
#endif
    putchar('\n');
    CC_CHECK(memcmp(&sim, &control, sizeof(sim)) == 0);
}
int main(void)
{
    Fixture(); Emit();
#ifdef CC_RITUAL_JSON_FIXTURE
    sim.dragon_cult.offering_coins = 119;
#else
    sim.dragon_campaign.supplies[CC_GOOD_BREAD] = 0;
#endif
    Emit(); Fixture(); Emit();
#ifdef CC_RITUAL_JSON_FIXTURE
    sim.dragon_cult.dragon_seed_days_remaining = 9;
#else
    sim.dragon_campaign.cooldown_days = 9;
#endif
    Emit();
    return 0;
}
