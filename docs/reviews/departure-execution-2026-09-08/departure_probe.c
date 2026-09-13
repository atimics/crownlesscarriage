#include "sim/cc_sim.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
static CcSim sim;
static void Preview(CcId destination)
{
    char error[256] = {0};
    CcCommand depart = {.kind = CC_COMMAND_TRAVEL, .target_id = destination};
    bool ok = CcSimApply(&sim, &depart, error, sizeof(error));
    printf("%d %s %016" PRIx64 "\n", ok, error, CcSimHash(&sim));
    if (ok) {
        CcSimAdvanceRuntimeTicks(&sim, 120);
        printf("tick %016" PRIx64 "\n", CcSimHash(&sim));
    }
}
int main(void)
{
    const uint32_t versions[] = {13,14,15,26,33,38,39,40,41,59,60};
    const uint32_t seeds[] = {0x5eed0001,0xc0a71a9e};
    for (unsigned v=0; v<sizeof(versions)/sizeof(versions[0]); ++v)
        for (unsigned seed=0; seed<2; ++seed) {
            CcSimInit(&sim,seeds[seed]);
            int routes=sim.route_count;
            for (int r=0; r<routes; ++r)
                for (int reverse=0; reverse<2; ++reverse)
                    for (int variant=0; variant<16; ++variant) {
                        CcSimInit(&sim,seeds[seed]);
                        sim.schema_version=versions[v];
                        CcRoute *route=&sim.routes[r];
                        sim.player.location_id=reverse ? route->to_id : route->from_id;
                        sim.journey.total_subticks=variant & 1 ? CC_WORLD_WATCH_SUBTICKS : 0;
                        sim.clock.minute_subticks=variant & 2 ? CC_WORLD_MINUTE_SUBTICKS * 720 : 0;
                        if (variant & 4) {
                            sim.map_count=0;
                            for (int i=0; i<CcSimHorseTeamCount(&sim); ++i) sim.horse_team[i].hunger=90;
                            sim.dragon.lair_settlement_id=route->from_id;
                            sim.dragon.regional_influence=90;
                            route->closed=true;
                        }
                        sim.player.cargo[CC_GOOD_MEAT] = 7;
                        sim.player.cargo[CC_GOOD_WHEAT] = 9;
                        if (variant & 8) {
                            sim.player.coins = 0;
                            sim.horse_team[0].pregnancy_days_remaining = 12;
                        }
                        printf("%u %u %d %d %d ",versions[v],seeds[seed],r,reverse,variant);
                        Preview(reverse ? route->from_id : route->to_id);
                    }
            Preview(0);
        }
    return 0;
}
