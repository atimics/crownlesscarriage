#include "sim/cc_scriven.h"
#include "persistence/cc_save.h"
#include <stdio.h>
#include <stdlib.h>
static CcSim sim;
int main(int argc,char **argv)
{
    int seeds=argc>1?atoi(argv[1]):8,years=argc>2?atoi(argv[2]):30;
    if(seeds<1 || seeds>128 || years<1 || years>3000) return 2;
    puts("seed,years,meetings,comparisons,returned,failed_trips,editions,true_ages,waiting,valid");
    int failed=0;
    for(int seed=1;seed<=seeds;++seed) {
        CcSimInit(&sim,(uint32_t)seed);char error[256]={0};bool valid=true;
        for(int day=0;day<years*364;++day) {
            CcSimAdvanceDays(&sim,1);
            if(day%7==0 && !CcSimValidate(&sim,error,sizeof(error))) {valid=false;break;}
        }
        int waiting=0;
        for(int i=0;i<CC_SCRIVEN_DELEGATES;++i)
            if(sim.scriven.delegates[i].phase>=CC_SCRIVEN_EXPEDITION && sim.scriven.delegates[i].phase<=CC_SCRIVEN_RETURNING) ++waiting;
        printf("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",seed,years,sim.scriven.meetings,sim.scriven.comparisons,sim.scriven.returns,sim.scriven.failed_trips,sim.scriven.editions,sim.scriven.age_count,waiting,valid?1:0);
        if(!valid) {fprintf(stderr,"Seed %d day %d: %s\n",seed,sim.current_day,error);++failed;}
    }
    return failed?1:0;
}
