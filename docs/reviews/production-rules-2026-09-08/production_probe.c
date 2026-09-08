#include "sim/cc_production_internal.h"
#include "sim/cc_route_rules_internal.h"
#include "test_support.h"
#include <inttypes.h>
#include <stdio.h>
static CcSim sim;
int main(void) {
    const uint32_t versions[]={26,28,29,32,40,60};
    const uint32_t seeds[]={0x5eed0001,0xc0a71a9e};
    const int hunger[]={0,36,66};
    for(unsigned v=0;v<sizeof(versions)/sizeof(versions[0]);++v)
        for(unsigned seed=0;seed<2;++seed) {
            CcSimInit(&sim,seeds[seed]);int count=sim.settlement_count;
            for(int slot=0;slot<count;++slot)
                for(int variant=0;variant<12;++variant) {
                    CcSimInit(&sim,seeds[seed]);sim.schema_version=versions[v];
                    sim.current_day=variant/3*13*7;
                    CcSettlement *place=&sim.settlements[slot];
                    place->hunger=hunger[variant%3];
                    if(variant%2) place->stock[CC_GOOD_TOOLS]=0;
                    if(variant==7) place->population=299;
                    if(variant==8) place->iron_deposit=0;
                    if(variant==9) place->field_yield=0;
                    if(variant==10) place->service_mask=0;
                    if(variant==11) for(int d=0;d<sim.dungeon_count;++d)
                        sim.dungeons[d].state=CC_DUNGEON_PUBLIC_ROUTE;
                    uint64_t before=CcSimHash(&sim);
                    for(int good=0;good<CC_GOOD_COUNT;++good)
                        printf("%u %u %d %d %d %d %d %d\n",versions[v],seed,slot,variant,good,
                            CcEconomyEffectiveProduction(&sim,place,slot,(CcGood)good),
                            CcEconomyBakeryCapacity(place),CcRouteSettlementMonsterPressure(&sim,place->id));
                    CC_CHECK(before==CcSimHash(&sim));
                }
        }
    return 0;
}
