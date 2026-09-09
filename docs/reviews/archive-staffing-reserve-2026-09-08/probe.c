#include "sim/cc_sim.h"
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
static CcSim sim;
int main(int argc,char **argv){
 if(argc!=3)return 2;
 unsigned seed=(unsigned)strtoul(argv[1],NULL,0),schema=(unsigned)strtoul(argv[2],NULL,0);
 if(schema==0)schema=CC_SIM_SCHEMA_VERSION;
 CcSimInit(&sim,seed);sim.schema_version=schema;
 int zero=0,low=0,ready=0,booked=0,arrived=0,lost=0;long long staff=0,lore=0;
 char error[256];
 printf("{\"seed\":%u,\"schema\":%u,\"annual_hashes\":[",seed,schema);
 for(int day=1;day<=36500;day++){
  CcSimAdvanceDays(&sim,1);
  if(sim.current_day%7==0){zero+=sim.archives.scribes==0;low+=sim.iron_ledger_reserve<50;staff+=sim.archives.scribes;lore+=sim.archives.lore_stored;ready+=CcSimArchiveWorkPlan(&sim).recording_ready;}
  for(int i=0;i<sim.event_count;i++){const CcEvent*e=CcSimRecentEvent(&sim,i);if(e->day!=sim.current_day)break;booked+=e->kind==CC_EVENT_SHIPMENT_DEPARTED;arrived+=e->kind==CC_EVENT_SHIPMENT_ARRIVED;lost+=e->kind==CC_EVENT_SHIPMENT_LOST;}
  if(day%365==0){if(!CcSimValidate(&sim,error,sizeof error)){fprintf(stderr,"seed%u schema%u day%d: %s\n",seed,schema,sim.current_day,error);return 1;}printf("%s\"%016" PRIx64 "\"",day==365?"":",",CcSimHash(&sim));}
 }
 printf("],\"weekly_zero_staff\":%d,\"weekly_low_reserve\":%d,\"weekly_ready\":%d,\"staff_sum\":%lld,\"lore_sum\":%lld,\"all_shipments_departed\":%d,\"all_shipments_arrived\":%d,\"all_shipments_lost\":%d,\"end_staff\":%d,\"end_lore\":%d}\n",zero,low,ready,staff,lore,booked,arrived,lost,sim.archives.scribes,sim.archives.lore_stored);
 return 0;
}
