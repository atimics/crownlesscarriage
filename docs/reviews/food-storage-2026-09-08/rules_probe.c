#include "sim/cc_food_economy_internal.h"
#include <inttypes.h>
#include <stdio.h>
static CcSim sim;
int main(void) {
    const uint32_t versions[]={26,29,32,33,60};
    const uint32_t seeds[]={0x5eed0001,0xc0a71a9e};
    for (unsigned v=0;v<sizeof(versions)/sizeof(versions[0]);++v)
        for (unsigned seed=0;seed<2;++seed) {
            CcSimInit(&sim,seeds[seed]); int count=sim.settlement_count;
            for (int slot=0;slot<count;++slot)
                for (int variant=0;variant<6;++variant) {
                    CcSimInit(&sim,seeds[seed]);sim.schema_version=versions[v];
                    CcSettlement *place=&sim.settlements[slot];
                    if (variant==1) place->service_mask=0;
                    if (variant==2) place->service_mask |= UINT32_C(1)<<CC_SERVICE_GRANARY;
                    if (variant==3) place->population=299;
                    if (variant==4) place->consumption[CC_GOOD_FOOD]=0;
                    if (variant==5) place->function=CC_SETTLEMENT_FORTRESS;
                    for (int good=0;good<CC_GOOD_COUNT;++good) {
                        if (variant==1) place->stock[good]=0;
                        if (variant==2) place->stock[good]=CC_SIM_MAX_UNITS;
                        CcEconomyRefreshSettlementGoodPrice(&sim,place,(CcGood)good);
                        printf("good %u %u %d %d %d %d %d %d %d\n",versions[v],seed,slot,variant,good,
                            CcEconomyWarExtraConsumption(&sim,place,(CcGood)good),
                            CcEconomyEffectiveReserveTarget(&sim,place,(CcGood)good),
                            CcEconomyNutritionStorageCapacity(&sim,place,(CcGood)good),place->price[good]);
                    }
                    CcTownNutritionAccounting accounting={0};
                    int spoiled=CcEconomySpoilStoredNutrition(&sim,place,&accounting);
                    printf("food %d %d %d %d %d %016" PRIx64,spoiled,
                        CcEconomyCivilianFoodUse(place),CcEconomyWeeklyFoodUse(&sim,place),
                        CcEconomyNutritionRations(place->stock,CC_NUTRITION_CIVILIAN),
                        CcEconomyIncomingNutrition(&sim,place->id,CC_NUTRITION_CIVILIAN),CcSimHash(&sim));
                    for (int good=0;good<CC_GOOD_COUNT;++good)
                        printf(" %" PRIu64 " %" PRIu64,accounting.aged_units[good],accounting.overflow_units[good]);
                    printf("\n");
                }
        }
    return 0;
}
