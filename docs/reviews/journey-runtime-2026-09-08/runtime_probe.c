#include "sim/cc_sim.h"
#include "test_support.h"
#include <inttypes.h>
#include <stdio.h>
static CcSim base;
static CcSim sim;
int main(void)
{
    const uint32_t versions[]={26,27,33,34,38,39,40,41,60};
    const uint32_t seeds[]={0x5eed0001,0xc0a71a9e};
    char error[256];
    for (unsigned v=0; v<sizeof(versions)/sizeof(versions[0]); ++v)
        for (unsigned seed=0; seed<2; ++seed) {
            CcSimInit(&base,seeds[seed]);
            int count=base.route_count;
            for (int r=0; r<count; ++r)
                for (int reverse=0; reverse<2; ++reverse) {
                    CcSimInit(&base,seeds[seed]);
                    base.schema_version=versions[v];
                    const CcRoute *route=&base.routes[r];
                    base.player.location_id=reverse ? route->to_id : route->from_id;
                    base.carriage.location_id=base.player.location_id;
                    CcSettlement *origin=CcSimSettlementMutable(&base,base.player.location_id);
                    origin->stock[CC_GOOD_WHEAT]=10000;
                    base.player.coins=10000;
                    CcCommand travel={.kind=CC_COMMAND_TRAVEL,.target_id=reverse ? route->from_id : route->to_id};
                    CC_CHECK(CcSimApply(&base,&travel,error,sizeof(error)));
                    for (int pace=0; pace<3; ++pace)
                        for (int variant=0; variant<10; ++variant) {
                            sim=base;
                            sim.journey.total_subticks=3*CC_WORLD_WATCH_SUBTICKS;
                            sim.journey.pace=(CcJourneyPace)pace;
                            sim.journey.encounter_triggered=true;
                            sim.journey.ambush_pending=true;
                            sim.journey.ambush_warned=false;
                            if (variant==1) sim.journey.elapsed_subticks=CC_WORLD_WATCH_SUBTICKS-1;
                            if (variant==2) sim.journey.elapsed_subticks=sim.journey.total_subticks*45/100-1;
                            if (variant==3) {
                                sim.journey.elapsed_subticks=sim.journey.total_subticks*60/100-1;
                                sim.journey.ambush_warned=true;
                            }
                            if (variant==4 || variant==5) {
                                sim.journey.elapsed_subticks=sim.journey.total_subticks-1;
                                if (sim.courier_count>0) {
                                    sim.couriers[0].status=CC_COURIER_WITH_PLAYER;
                                    sim.couriers[0].destination_settlement_id=sim.journey.destination_id;
                                }
                                if (sim.treasure_count>0) sim.treasures[0].owner_id=sim.player.id;
                            }
                            if (variant==5) sim.clock.minute_subticks=CC_WORLD_DAY_SUBTICKS-1;
                            if (variant==6) sim.journey.phase=CC_JOURNEY_PHASE_BLOCKED;
                            if (variant==7) sim.clock.tick=UINT64_MAX-1;
                            if (variant==8) sim.journey.active=false;
                            if (variant==9) sim.pony_company.encounter=0;
                            CcSimAdvanceRuntimeTicks(&sim,120);
                            printf("%u %u %d %d %d %d %d %d %d %d %016" PRIx64 "\n",versions[v],seeds[seed],r,reverse,pace,variant,
                                sim.journey.active,sim.journey.phase,sim.journey.ambush_warned,sim.journey.ambush_resolved,CcSimHash(&sim));
                        }
                }
        }
    return 0;
}
