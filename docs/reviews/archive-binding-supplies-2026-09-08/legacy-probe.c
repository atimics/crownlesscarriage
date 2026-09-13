#include "sim/cc_sim.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
static CcSim sim;
int main(int argc, char **argv) {
    if (argc != 2) return 2;
    CcSimInit(&sim, (uint32_t)strtoul(argv[1], NULL, 0));
    sim.schema_version = 82;
    for (int year = 0; year <= 40; ++year) {
        printf("%d %016" PRIx64 "\n", year, CcSimHash(&sim));
        if (year < 40) CcSimAdvanceDays(&sim, 365);
    }
    return 0;
}
