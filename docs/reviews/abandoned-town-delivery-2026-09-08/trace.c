#include "sim/cc_sim.h"
#include <stdio.h>
static CcSim sim;
int main(void){CcSimInit(&sim,353U*0x9e3779b9U);for(int y=0;y<80;y++)CcSimAdvanceDays(&sim,365);for(int d=0;d<365;d++){CcSimAdvanceDays(&sim,1);for(int i=0;i<sim.settlement_count;i++){CcSettlement*t=&sim.settlements[i];if(t->population==0&&t->prosperity!=0){printf("day%d %s prosperity%d\n",sim.current_day,t->name,t->prosperity);for(int e=0;e<sim.event_count;e++){const CcEvent*v=CcSimRecentEvent(&sim,e);if(v->day<sim.current_day)break;printf("event%d %s\n",v->kind,v->text);}return 1;}}}return 0;}
