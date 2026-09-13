#include "sim/cc_sim.h"
#include <inttypes.h>
#include <stdio.h>
static CcSim sim;
int main(void) {
    const uint32_t versions[] = {26,27,33,34,36,37,58,59,60};
    const uint32_t seeds[] = {0x5eed0001,0xc0a71a9e};
    for (unsigned v=0;v<sizeof(versions)/sizeof(versions[0]);++v)
        for (unsigned s=0;s<2;++s) {
            CcSimInit(&sim,seeds[s]); sim.schema_version=versions[v];
            for (int year=0;year<40;++year) {
                CcSimAdvanceDays(&sim,365);
                printf("%u %u %d %016" PRIx64 "\n",versions[v],seeds[s],sim.current_day,CcSimHash(&sim));
            }
        }
    return 0;
}
