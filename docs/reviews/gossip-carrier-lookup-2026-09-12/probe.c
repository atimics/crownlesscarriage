#include "sim/cc_sim.h"
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
static CcSim sim;
int main(int argc,char **argv)
{
    if(argc!=4)return 2;
    unsigned ordinal=(unsigned)strtoul(argv[1],NULL,0);
    int years=atoi(argv[2]); unsigned schema=(unsigned)strtoul(argv[3],NULL,0);
    CcSimInit(&sim,ordinal*UINT32_C(0x9e3779b9));
    if(schema)sim.schema_version=schema;
    char error[256];
    printf("{\"ordinal\":%u,\"schema\":%u,\"years_requested\":%d,\"hashes\":[",ordinal,sim.schema_version,years);
    for(int y=1;y<=years;y++){
        CcSimAdvanceDays(&sim,365);
        printf("%s\"%016" PRIx64 "\"",y==1?"":",",CcSimHash(&sim));
        if(!CcSimValidate(&sim,error,sizeof(error))){
            printf("],\"failed_year\":%d,\"error\":\"%s\"}\n",y,error);return 1;
        }
    }
    puts("],\"failed_year\":0}");return 0;
}
