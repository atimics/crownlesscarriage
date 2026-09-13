#include "sim/cc_goods_internal.h"
#include <stdio.h>
int main(void) {
    const int32_t quantities[]={-1,0,1,2,7,8,9,15,16,100,CC_SIM_MAX_UNITS};
    for (int g=-1;g<=CC_GOOD_COUNT;++g)
        for (unsigned q=0;q<sizeof(quantities)/sizeof(quantities[0]);++q)
            printf("%d %u %d %d %d\n",g,q,
                CcGoodsPlayerCargoBoxes((CcGood)g,quantities[q]),
                CcGoodsFreightCargoSlots((CcGood)g,quantities[q]),
                CcGoodsFreightUnitsPerCargoSlot((CcGood)g));
    for (int variant=0;variant<2000;++variant) {
        CcPlayerCompany player={0};
        player.treasure_cargo_slots=variant%17;
        for (int g=0;g<CC_GOOD_COUNT;++g)
            player.cargo[g]=quantities[(variant+g*7)%11];
        printf("cargo %d %d\n",variant,CcPlayerCargoUsed(&player));
    }
    return 0;
}
