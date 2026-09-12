#include "sim/cc_sim.h"
#include "persistence/cc_save.h"
#include <stdio.h>
static CcSim sim;
int main(void) {
 char error[256];
 CcSimInit(&sim, 42U);
 sim.player.location_id = sim.settlements[1].id;
 sim.carriage.location_id = sim.player.location_id;
 sim.player.coins = 400;
 CcCommand fund = {.kind=CC_COMMAND_FUND_GRAIN_SUPPLY,.target_id=sim.player.location_id};
 if (!CcSimApply(&sim,&fund,error,sizeof(error))) return 1;
 CcSimAdvanceDays(&sim,240);
 CcGrainDeliveryPlan plan=CcSimGrainDeliveryPlan(&sim,fund.target_id);
 printf("%d\n%s\n", (int)plan.status,plan.reason);
 return CcSaveWrite("/private/tmp/grain-report-funded.ccsave",&sim,error,sizeof(error)) ? 0 : 2;
}
