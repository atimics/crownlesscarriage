#include "sim/cc_sim.h"
#include <stdio.h>
#include <string.h>
int main(void)
{
    const int quantities[]={-1,0,1,2,3,7,100,CC_SIM_MAX_UNITS};
    const int requests[]={-1,0,1,2,3,5,11,199,CC_SIM_MAX_UNITS};
    for (int p=0; p<3; ++p)
        for (unsigned b=0; b<sizeof(quantities)/sizeof(quantities[0]); ++b)
            for (unsigned m=0; m<sizeof(quantities)/sizeof(quantities[0]); ++m)
                for (unsigned w=0; w<sizeof(quantities)/sizeof(quantities[0]); ++w)
                    for (unsigned r=0; r<sizeof(requests)/sizeof(requests[0]); ++r) {
                        int32_t goods[CC_GOOD_COUNT]={0};
                        goods[CC_GOOD_BREAD]=quantities[b];
                        goods[CC_GOOD_MEAT]=quantities[m];
                        goods[CC_GOOD_WHEAT]=quantities[w];
                        int32_t available=CcNutritionAvailable(goods,(CcNutritionPurpose)p);
                        int32_t consumed=CcNutritionConsume(goods,(CcNutritionPurpose)p,requests[r]);
                        printf("%d %u %u %u %u %d %d %d %d %d\n",p,b,m,w,r,available,consumed,goods[CC_GOOD_BREAD],goods[CC_GOOD_MEAT],goods[CC_GOOD_WHEAT]);
                    }
    return 0;
}
