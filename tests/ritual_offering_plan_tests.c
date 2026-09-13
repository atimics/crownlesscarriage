#include "sim/cc_sim.h"
#include "test_support.h"
#include <string.h>
static CcSim sim, ready, control;
static void Fixture(void)
{
    CcSimInit(&sim, 123U);
    sim.dragon_cult.ranks[CC_CULT_HUMAN][0] = 36;
    sim.goblins.members = 48; sim.dragon_cult.devotion = 75; sim.goblins.cohesion = 75;
    sim.dragon_cult.offering_coins = 120;
    memset(sim.dragon_cult.offering_stock, 0, sizeof(sim.dragon_cult.offering_stock));
    sim.dragon_cult.offering_stock[CC_GOOD_BREAD] = 12;
    sim.dragon_cult.offering_stock[CC_GOOD_TOOLS] = 2;
    sim.dragon_cult.offering_stock[CC_GOOD_WEAPONS] = 3;
    sim.dragon_cult.offering_stock[CC_GOOD_GOLD] = 1;
    sim.dragon_cult.offering_stock[CC_GOOD_GEMS] = 1;
    ready = sim;
}
static void Expect(uint32_t mask)
{
    control = sim;
    CC_CHECK(CcSimRitualOfferingPlan(&sim).blocked == mask);
    CC_CHECK(memcmp(&sim, &control, sizeof(sim)) == 0);
    sim = ready;
}
int main(void)
{
    Fixture(); Expect(0U);
    CC_CHECK(CcSimRitualOfferingPlan(&sim).eggs == 1);
    sim.dragon_cult.ranks[CC_CULT_HUMAN][0] = 35; Expect(CC_RITUAL_MEMBERS);
    sim.dragon_cult.devotion = 74; Expect(CC_RITUAL_DEVOTION);
    sim.goblins.cohesion = 74; Expect(CC_RITUAL_COHESION);
    sim.dragon_cult.offering_coins = 119; Expect(CC_RITUAL_COINS);
    sim.dragon_cult.offering_stock[CC_GOOD_GEMS] = 0; Expect(CC_RITUAL_RELICS);
    sim.dragon_cult.offering_stock[CC_GOOD_BREAD] = 11; Expect(CC_RITUAL_FOOD);
    sim.dragon_cult.offering_stock[CC_GOOD_TOOLS] = 1; Expect(CC_RITUAL_TOOLS);
    sim.dragon_cult.offering_stock[CC_GOOD_WEAPONS] = 2; Expect(CC_RITUAL_WEAPONS);
    sim.dragon_cult.offering_stock[CC_GOOD_BREAD] = 0;
    sim.dragon_cult.offering_stock[CC_GOOD_WHEAT] = 24; Expect(0U);
    sim.dragon_cult.ranks[CC_CULT_HUMAN][0] = 60; sim.dragon_cult.devotion = 90; sim.goblins.cohesion = 90;
    CC_CHECK(CcSimRitualOfferingPlan(&sim).eggs == 2);
    CC_CHECK(CcSimRitualOfferingPlan(NULL).blocked == CC_RITUAL_INVALID);
    CC_CHECK(CcSimRitualOfferingPlan(NULL).food_rations == -1);

    Fixture();
    sim.current_day = 121 * 365;
    sim.dragon.slain = true; sim.dragon.egg_count = 0;
    sim.dragon.afterdeath_days = 121 * 365 - 1;
    sim.dragon_cult.dragon_seed_phase = CC_GOBLIN_DRAGON_SEED_PREPARING;
    sim.dragon_cult.dragon_seed_days_remaining = 0;
    sim.goblins.tribute_phase = CC_GOBLIN_TRIBUTE_IDLE;
    sim.goblins.members = 84; sim.dragon_cult.devotion = 90;
    sim.dragon_cult.offering_stock[CC_GOOD_BREAD] = 64;
    control = sim; control.goblins.cohesion = 74;
    CcSimAdvanceDays(&sim, 1); CcSimAdvanceDays(&control, 1);
    CC_CHECK(sim.dragon.egg_count == 1);
    CC_CHECK(control.dragon.egg_count == 0);
    CC_CHECK(control.dragon_cult.dragon_seed_phase == CC_GOBLIN_DRAGON_SEED_PREPARING);
    return 0;
}
