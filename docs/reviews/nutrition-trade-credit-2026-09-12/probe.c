#include "sim/cc_sim.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
static CcSim sim;
static CcNutritionAccounting nutrition;
int main(int argc, char **argv)
{
    if (argc != 4) return 2;
    unsigned ordinal = (unsigned)strtoul(argv[1], NULL, 0);
    unsigned schema = (unsigned)strtoul(argv[2], NULL, 0);
    int slain = atoi(argv[3]);
    CcSimInit(&sim, ordinal * UINT32_C(0x9e3779b9));
    sim.schema_version = schema;
    if (slain) {
        sim.dragon.slain = true; sim.dragon.slain_day = 1;
        sim.dragon.life_stage = CC_DRAGON_STAGE_AFTERDRAGON;
        sim.dragon.activity = CC_DRAGON_ACTIVITY_AFTERMATH;
        sim.dragon.body_condition = 0; sim.dragon.crown_strength = 0;
    }
    char error[256];
    for (int year = 1; year <= 40; ++year) {
        CcSimAdvanceDaysWithNutritionAccounting(&sim, 365, &nutrition);
        if (!CcSimValidate(&sim, error, sizeof(error))) { fprintf(stderr, "%s\n", error); return 1; }
        CcHungerSnapshot hunger = CcSimHungerSnapshot(&sim);
        uint64_t waste = 0; CcMoney debt = 0;
        for (int i = 0; i < sim.settlement_count; ++i) waste += nutrition.towns[i].overflow_units[CC_GOOD_WHEAT];
        for (int i = 0; i < sim.kingdom_count; ++i) debt += sim.kingdoms[i].iron_ledger_debt;
        printf("{\"year\":%d,\"hash\":\"%016" PRIx64 "\",\"population\":%" PRId64
               ",\"hunger\":%d,\"abandoned\":%d,\"wheat_overflow\":%" PRIu64
               ",\"debt\":%" PRId64 ",\"ledger_reserve\":%" PRId64 "}\n",
               year, CcSimHash(&sim), hunger.population, hunger.population_weighted,
               hunger.abandoned_settlements, waste, debt, sim.iron_ledger_reserve);
    }
    return 0;
}
