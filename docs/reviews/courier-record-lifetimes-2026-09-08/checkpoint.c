#include "persistence/cc_save.h"
#include <stdio.h>
static CcSim sim,before;
int main(int argc,char**argv){
 if(argc!=2)return 2;
 CcSimInit(&sim,148U*UINT32_C(0x9e3779b9));
 for(int y=0;y<844;y++)CcSimAdvanceDays(&sim,365);
 for(int d=0;d<365;d++){
  before=sim;CcSimAdvanceDays(&sim,1);
  for(int i=0;i<sim.situation_count;i++){
   CcSituation*q=&sim.situations[i];if(q->kind!=CC_SITUATION_COURIER_DELIVERY)continue;
   bool found=false;for(int j=0;j<sim.courier_count;j++)found|=sim.couriers[j].id==q->target_id;
   if(found)continue;
   char error[256];unsigned char*bytes=NULL;size_t length=0;
   if(!CcSaveEncode(&before,&bytes,&length,error,sizeof error)){fprintf(stderr,"%s\n",error);return 1;}
   FILE*f=fopen(argv[1],"wb");if(!f)return 1;
   bool ok=fwrite(bytes,1,length,f)==length;ok=fclose(f)==0&&ok;CcSaveFreeBuffer(bytes);
   printf("checkpoint day %d; failure day %d; quest status %d\n",before.current_day,sim.current_day,q->status);return ok?0:1;
  }
 }
 return 1;
}
