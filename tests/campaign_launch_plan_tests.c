#include "sim/cc_sim.h"
#include "test_support.h"
#include <string.h>
static CcSim sim, ready, control;

static void Fixture(void)
{
    CcSimInit(&sim, 123U);
    sim.dragon.age_days = 500 * 365;
    sim.dragon.regional_influence = 100;
    sim.dragon.slain = false;
    sim.dragon_campaign.phase = CC_DRAGON_CAMPAIGN_IDLE;
    sim.dragon_campaign.cooldown_days = 0;
    sim.dragon_campaign.pledged_kingdom_mask = 3U;
    sim.dragon_campaign.supplies[CC_GOOD_BREAD] = 32;
    sim.dragon_campaign.supplies[CC_GOOD_TOOLS] = 8;
    sim.dragon_campaign.supplies[CC_GOOD_WEAPONS] = 12;
    sim.dragon_campaign.patron_character_id = sim.kingdoms[0].monastery_patron_id;
    sim.dragon_campaign.hero_character_id = sim.characters[0].id;
    ready = sim;
}

static void Expect(uint32_t mask)
{
    control = sim;
    CC_CHECK(CcSimCampaignLaunchPlan(&sim).blocked == mask);
    CC_CHECK(memcmp(&control, &sim, sizeof(sim)) == 0);
    sim = ready;
}

int main(void)
{
    Fixture();
    CcCampaignLaunchPlan plan = CcSimCampaignLaunchPlan(&sim);
    CC_CHECK(plan.blocked == 0U && plan.pledged_count == 2);
    CC_CHECK(plan.leader_slot == 0 && plan.origin_id != 0U);
    sim.dragon_campaign.phase = CC_DRAGON_CAMPAIGN_OUTBOUND; Expect(CC_CAMPAIGN_ACTIVE);
    sim.dragon_campaign.cooldown_days = 1; Expect(CC_CAMPAIGN_COOLDOWN);
    sim.dragon.slain = true; Expect(CC_CAMPAIGN_DRAGON_SLAIN);
    sim.dragon_campaign.pledged_kingdom_mask = 1U; Expect(CC_CAMPAIGN_PLEDGES);
    sim.dragon.age_days = 499 * 365; Expect(CC_CAMPAIGN_DRAGON_AGE);
    sim.dragon.age_days = 1; sim.dragon.life_stage = CC_DRAGON_STAGE_DEEP_WYRM;
    Expect(0U);
    sim.dragon_campaign.supplies[CC_GOOD_BREAD] = 31; Expect(CC_CAMPAIGN_FOOD);
    sim.dragon_campaign.supplies[CC_GOOD_TOOLS] = 7; Expect(CC_CAMPAIGN_TOOLS);
    sim.dragon_campaign.supplies[CC_GOOD_WEAPONS] = 11; Expect(CC_CAMPAIGN_WEAPONS);
    sim.dragon_campaign.patron_character_id = 0U; Expect(CC_CAMPAIGN_PATRON);
    sim.dragon_campaign.hero_character_id = 0U; Expect(CC_CAMPAIGN_HERO);
    for (int i = 0; i < sim.settlement_count; ++i) sim.settlements[i].population = 0;
    Expect(CC_CAMPAIGN_SEAT);
    CC_CHECK(CcSimCampaignLaunchPlan(NULL).blocked == CC_CAMPAIGN_INVALID);
    CC_CHECK(CcSimCampaignLaunchPlan(NULL).food_rations == -1);

    /* Actual preparation names leaders and launches after cooldown is lifted. */
    Fixture(); sim.current_day = 27;
    sim.dragon_campaign.patron_character_id = 0U;
    sim.dragon_campaign.hero_character_id = 0U;
    control = sim; control.dragon_campaign.cooldown_days = 10;
    int32_t attempts = sim.dragon_campaign.attempts;
    CcSimAdvanceDays(&sim, 1); CcSimAdvanceDays(&control, 1);
    CC_CHECK(sim.dragon_campaign.phase == CC_DRAGON_CAMPAIGN_OUTBOUND);
    CC_CHECK(sim.dragon_campaign.attempts == attempts + 1);
    CC_CHECK(sim.dragon_campaign.patron_character_id != 0U);
    CC_CHECK(sim.dragon_campaign.hero_character_id != 0U);
    CC_CHECK(control.dragon_campaign.phase == CC_DRAGON_CAMPAIGN_IDLE);
    CC_CHECK(control.dragon_campaign.attempts == attempts);
    return 0;
}
