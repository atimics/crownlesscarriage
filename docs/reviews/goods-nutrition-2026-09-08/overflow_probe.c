#include "sim/cc_sim.h"
#include <stdio.h>
int main(void) {
    int32_t goods[CC_GOOD_COUNT]={0}; goods[CC_GOOD_BREAD]=10;
    int32_t delivered=CcNutritionConsume(goods,CC_NUTRITION_CIVILIAN,INT32_MAX);
    printf("delivered=%d bread_remaining=%d\n",delivered,goods[CC_GOOD_BREAD]);
    return 0;
}
