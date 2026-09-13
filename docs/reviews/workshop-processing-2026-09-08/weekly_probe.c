#include "sim/cc_sim.h"
#include <inttypes.h>
#include <stdio.h>
static CcSim sim;
int main(void) {
    const uint32_t versions[]={26,27,29,33,34,36,37,54,60};
    const uint32_t seeds[]={0x5eed0001,0xc0a71a9e};
    for(unsigned v=0;v<sizeof(versions)/sizeof(versions[0]);++v)
        for(unsigned seed=0;seed<2;++seed)
            for(int variant=0;variant<5;++variant) {
                CcSimInit(&sim,seeds[seed]);sim.schema_version=versions[v];
                sim.current_day=27;
                for(int i=0;i<sim.settlement_count;++i) {
                    CcSettlement *p=&sim.settlements[i];
                    if(variant==1) p->stock[CC_GOOD_IRON]=p->stock[CC_GOOD_WOOD]=0;
                    if(variant==2) p->stock[CC_GOOD_BREAD]=p->stock[CC_GOOD_WHEAT]=0;
                    if(variant==3) {
                        p->service_mask|=(UINT32_C(1)<<CC_SERVICE_SMITHY)|(UINT32_C(1)<<CC_SERVICE_MILL)|(UINT32_C(1)<<CC_SERVICE_BAKERY);
                        for(int g=0;g<CC_GOOD_COUNT;++g) p->stock[g]=100;
                        p->production[CC_GOOD_TOOLS]=5;p->production[CC_GOOD_WEAPONS]=2;
                        p->production[CC_GOOD_PAPER]=4;p->production[CC_GOOD_BREAD]=8;
                        p->treasure_work=2;p->treasure_gold_committed=1;p->treasure_gems_committed=1;
                        p->gold_progress=11;p->gem_progress=47;
                    }
                    if(variant==4) p->hunger=66;
                }
                for(int week=0;week<16;++week) {
                    CcSimAdvanceDays(&sim,week==0 ? 1 : 7);
                    printf("%u %u %d %d %016" PRIx64 "\n",versions[v],seed,variant,week,CcSimHash(&sim));
                }
            }
    return 0;
}
