#include "sim/cc_goods_internal.h"
#include <stdio.h>
int main(void) {
    CcPlayerCompany player={0};
    player.cargo[CC_GOOD_BREAD]=INT32_MAX;
    player.cargo[CC_GOOD_MEAT]=INT32_MAX;
    printf("player_slots=%d freight_bread_slots=%d\n",
        CcPlayerCargoUsed(&player),CcGoodsFreightCargoSlots(CC_GOOD_BREAD,INT32_MAX));
    return 0;
}
