#include "sim/cc_production.h"
#include "persistence/cc_save.h"
#include <stdio.h>
#include <stdlib.h>
static CcSim sim;
int main(int argc,char **argv) {
    if(argc!=4) return 1;
    CcSimInit(&sim,(uint32_t)strtoul(argv[1],NULL,0));
    const int slots[]={2,11,15};
    for(int i=0;i<3;i++) {
        CcRoadSite *site=&sim.road_sites[slots[i]];
        site->accessible=atoi(argv[2])!=0;
        if(site->accessible)site->blocker=CC_ROAD_SITE_BLOCKER_NONE;
        CcProductionRecipe recipe; if(!CcRoadSiteRecipe(site,&recipe))return 2;
        site->stock[CC_GOOD_TOOLS]=1;
        for(int j=0;j<recipe.input_count;j++)
            site->stock[recipe.inputs[j].good]=recipe.inputs[j].reserve+4*recipe.inputs[j].units;
    }
    char error[256];
    if(!CcSaveWrite(argv[3],&sim,error,sizeof(error))) {puts(error);return 3;}
    return 0;
}
