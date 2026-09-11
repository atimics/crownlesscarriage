#include "persistence/cc_save.h"
#include <stdio.h>
#include <inttypes.h>
static CcSim sim;
int main(int argc,char**argv){char error[256];if(argc!=2)return 2;if(!CcSaveRead(argv[1],&sim,error,sizeof error)){fprintf(stderr,"%s\n",error);return 1;}printf("day=%d schema=%u hash=%016" PRIx64 "\n",sim.current_day,sim.schema_version,CcSimHash(&sim));return 0;}
