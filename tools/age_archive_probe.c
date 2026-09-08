/* Read-only weekly archive observations for the first 32 sweep seeds. */
#include "sim/cc_sim.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    static CcSim sim;
    char error[192];
    puts("seed_number,year,samples,ready,scribes,grain,paper,tools,binding,ledger_below_50,mean_ledger,lore_stored,lore_lost_total,archive_scribes,state_hash");
    for (int32_t seed = 1; seed <= 32; ++seed) {
        CcSimInit(&sim, (uint32_t)seed * UINT32_C(0x9e3779b9));
        for (int32_t year = 1; year <= 100; ++year) {
            int32_t counts[6] = {0};
            int32_t samples = 0, low_ledger = 0;
            int64_t ledger_sum = 0;
            for (int32_t day = 0; day < 365; ++day) {
                CcSimAdvanceDays(&sim, 1);
                if (sim.current_day % 7 != 0) continue;
                CcMaterialChainSnapshot snapshot = CcSimMaterialChainSnapshot(&sim);
                if (snapshot.blocker < 0 || snapshot.blocker >= 6) return EXIT_FAILURE;
                counts[snapshot.blocker] += 1;
                samples += 1;
                if (sim.iron_ledger_reserve < 50) low_ledger += 1;
                ledger_sum += sim.iron_ledger_reserve;
            }
            if (!CcSimValidate(&sim, error, sizeof(error))) {
                fprintf(stderr, "Seed %d year %d: %s\n", seed, year, error);
                return EXIT_FAILURE;
            }
            printf("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%.6f,%d,%d,%d,%" PRIu64 "\n",
                   seed, year, samples, counts[CC_MATERIAL_CHAIN_READY],
                   counts[CC_MATERIAL_CHAIN_NO_SCRIBES], counts[CC_MATERIAL_CHAIN_GRAIN],
                   counts[CC_MATERIAL_CHAIN_PAPER], counts[CC_MATERIAL_CHAIN_TOOLS],
                   counts[CC_MATERIAL_CHAIN_BINDING], low_ledger,
                   (double)ledger_sum / samples, sim.archives.lore_stored,
                   sim.archives.lore_lost_total, sim.archives.scribes, CcSimHash(&sim));
        }
    }
    return EXIT_SUCCESS;
}
