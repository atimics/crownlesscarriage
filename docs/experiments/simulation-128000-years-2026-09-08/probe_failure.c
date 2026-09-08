/* Reproduce a long-run validation failure and report carriage and bandit counters. */
#include "sim/cc_sim.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    char *end = NULL;
    long seed = strtol(argv[1], &end, 10);
    if (*end != '\0' || seed < 1 || seed > 32) return 2;
    CcSim *sim = calloc(1, sizeof(*sim));
    if (sim == NULL) return 2;
    CcSimInit(sim, (uint32_t)seed * UINT32_C(0x9e3779b9));
    for (int32_t year = 1; year <= 128000; ++year) {
        CcSimAdvanceDays(sim, 365);
        char error[192];
        if (CcSimValidate(sim, error, sizeof(error))) continue;
        (void)printf("seed=%ld year=%d day=%d error=%s\n",
                     seed, year, sim->current_day, error);
        for (int32_t i = 0; i < sim->royal_carriage_count; ++i) {
            const CcRoyalCarriage *carriage = &sim->royal_carriages[i];
            (void)printf("carriage=%d trips=%d losses=%d condition=%d mode=%d next_dispatch=%d counter_limit=%d\n",
                         i, carriage->trips_completed, carriage->cargo_losses,
                         carriage->condition, (int32_t)carriage->mode,
                         carriage->next_dispatch_day, CC_SIM_MAX_UNITS);
        }
        for (int32_t i = 0; i < sim->bandit_count; ++i) {
            const CcBanditGroup *bandits = &sim->bandits[i];
            (void)printf("bandits=%d raids=%d phase=%d remaining=%d counter_limit=%d\n",
                         i, bandits->raids_completed, (int32_t)bandits->raid_phase,
                         bandits->raid_days_remaining, CC_SIM_MAX_UNITS);
        }
        free(sim);
        return 1;
    }
    free(sim);
    return 0;
}
