#include "sim/cc_sim.h"
#include "test_support.h"
#include <string.h>

int main(void)
{
    CcSim sim;
    CcSimInit(&sim, UINT32_C(0xba1a0ce1));
    sim.settlement_count = 3;
    sim.settlements[0].population = 100;
    sim.settlements[0].hunger = 10;
    sim.settlements[0].prosperity = 30;
    sim.settlements[0].security = 40;
    sim.settlements[1].population = 300;
    sim.settlements[1].hunger = 20;
    sim.settlements[1].prosperity = 80;
    sim.settlements[1].security = 90;
    sim.settlements[2].population = 0;
    sim.settlements[2].hunger = 100;
    sim.settlements[2].prosperity = 0;
    sim.settlements[2].security = 0;
    CcSim before = sim;
    uint64_t hash = CcSimHash(&sim);
    CcHungerSnapshot hunger = CcSimHungerSnapshot(&sim);
    CC_CHECK(hunger.inhabited_settlements == 2);
    CC_CHECK(hunger.abandoned_settlements == 1);
    CC_CHECK(hunger.population == 400);
    CC_CHECK(hunger.average == 15);
    CC_CHECK(hunger.maximum == 20);
    CC_CHECK(hunger.population_weighted == 17);
    CcWelfareSnapshot welfare = CcSimWelfareSnapshot(&sim);
    CC_CHECK(welfare.inhabited_settlements == 2 && welfare.abandoned_settlements == 1);
    CC_CHECK(welfare.population == 400);
    CC_CHECK(welfare.hunger == 15.0 && welfare.population_weighted_hunger == 17.5);
    CC_CHECK(welfare.prosperity == 55.0 && welfare.population_weighted_prosperity == 67.5);
    CC_CHECK(welfare.security == 65.0 && welfare.population_weighted_security == 77.5);
    CC_CHECK(CcSimHash(&sim) == hash);
    CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);

    /* An actual hungry population still contributes to both measures. */
    sim.settlements[0].hunger = 80;
    hunger = CcSimHungerSnapshot(&sim);
    CC_CHECK(hunger.average == 50);
    CC_CHECK(hunger.maximum == 80);
    CC_CHECK(hunger.population_weighted == 35);

    sim.settlements[0].population = 0;
    sim.settlements[1].population = 0;
    hunger = CcSimHungerSnapshot(&sim);
    CC_CHECK(hunger.inhabited_settlements == 0);
    CC_CHECK(hunger.abandoned_settlements == 3);
    CC_CHECK(hunger.population == 0);
    CC_CHECK(hunger.average == -1);
    CC_CHECK(hunger.maximum == -1);
    CC_CHECK(hunger.population_weighted == -1);
    welfare = CcSimWelfareSnapshot(&sim);
    CC_CHECK(welfare.population == 0 && welfare.abandoned_settlements == 3);
    CC_CHECK(welfare.hunger == -1 && welfare.prosperity == -1 && welfare.security == -1);
    CC_CHECK(welfare.population_weighted_hunger == -1 &&
             welfare.population_weighted_prosperity == -1 &&
             welfare.population_weighted_security == -1);
    CC_CHECK(sim.settlements[2].hunger == 100);

    sim.settlement_count = 0;
    hunger = CcSimHungerSnapshot(&sim);
    CC_CHECK(hunger.average == -1 && hunger.abandoned_settlements == 0);
    hunger = CcSimHungerSnapshot(NULL);
    CC_CHECK(hunger.average == -1 && hunger.population == 0);
    welfare = CcSimWelfareSnapshot(NULL);
    CC_CHECK(welfare.hunger == -1 && welfare.population == 0);
    sim.settlement_count = 2;
    for (int32_t i = 0; i < 2; ++i) {
        sim.settlements[i].population = INT32_MAX;
        sim.settlements[i].hunger = 100;
        sim.settlements[i].prosperity = 100;
        sim.settlements[i].security = 100;
    }
    welfare = CcSimWelfareSnapshot(&sim);
    CC_CHECK(welfare.population == (int64_t)INT32_MAX * 2);
    CC_CHECK(welfare.population_weighted_hunger == 100 &&
             welfare.population_weighted_prosperity == 100 &&
             welfare.population_weighted_security == 100);
    return 0;
}
