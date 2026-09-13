#include "sim/cc_sim.h"
#include "sim/cc_identity_internal.h"
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
static CcSim sim;
int main(void)
{
    const unsigned seeds[] = {42, 0x5eed0001};
    for (int seed = 0; seed < 2; ++seed) for (int age = 0; age < 2; ++age) {
        CcSimInit(&sim, seeds[seed]);
        if (age) CcSimAdvanceDays(&sim, 365 * 20);
        char error[256];
        for (int mode = 0; mode < 2; ++mode) {
            clock_t start = clock();
            for (int i = 0; i < 3000; ++i) {
                bool valid = mode == 0 ? CcIdentityValidate(&sim, error, sizeof(error)) :
                    CcSimValidate(&sim, error, sizeof(error));
                if (!valid) { fprintf(stderr, "%s\n", error); return 1; }
            }
            printf("%u %d %d %.9f\n", seeds[seed], age, mode,
                (double)(clock() - start) / CLOCKS_PER_SEC);
        }
    }
    return 0;
}
