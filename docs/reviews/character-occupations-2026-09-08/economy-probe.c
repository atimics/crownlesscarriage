#include "sim/cc_occupations.h"
#include "sim/cc_production.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static CcSim a, b;
static CcNutritionAccounting na, nb;
static CcSmithyAccounting sa, sb;
static CcProductionAccounting pa, pb;
int main(int argc, char **argv) {
    if (argc != 2) return 2;
    CcSimInit(&a, (uint32_t)strtoul(argv[1], NULL, 0)); b = a;
    for (int i = 0; i < b.character_count; ++i)
        b.characters[i].occupation = CC_OCCUPATION_NONE;
    for (int day = 0; day < 40 * 365; ++day) {
        CcSimAdvanceDaysWithProductionAccounting(&a, 1, &na, &sa, &pa);
        CcSimAdvanceDaysWithProductionAccounting(&b, 1, &nb, &sb, &pb);
        if (memcmp(&na,&nb,sizeof(na)) || memcmp(&sa,&sb,sizeof(sa)) || memcmp(&pa,&pb,sizeof(pa))) {
            printf("production or nutrition diverged at day %d\n", day+1); return 1;
        }
        for (int i = 0; i < a.settlement_count; ++i)
            if (memcmp(a.settlements[i].stock,b.settlements[i].stock,sizeof(a.settlements[i].stock))) {
                printf("town stock diverged at day %d town %d\n", day+1,i); return 1;
            }
    }
    puts("14600 daily production, nutrition, smithy, freight receipts and town stock comparisons match with all occupations cleared.");
    return 0;
}
