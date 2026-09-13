#include "sim/cc_sim.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
static CcSim sim;
static void Preview(CcId destination)
{
    CcTravelPreview p = {0};
    char error[256] = {0};
    uint64_t before = CcSimHash(&sim);
    bool ok = CcSimTravelPreview(&sim, destination, &p, error, sizeof(error));
    if (before != CcSimHash(&sim)) exit(2);
    printf("%d %s | %" PRIu64 " %" PRIu64 " %" PRId64 " %d %d %d %d %d %d %d %d %d %d %" PRId64 " %s %d %d %d %d %d\n",
        ok, error, p.route_id, p.destination_id, p.provision_cost,
        p.travel_days, p.claimed_condition, p.claimed_danger, p.chart_accuracy,
        p.horse_feed_required, p.horse_readiness, p.travel_watches,
        p.overnight_stops, p.departure_wait_minutes, p.road_house_distance_miles,
        p.road_house_cost, p.road_house_name ? p.road_house_name : "(null)",
        p.rain_expected, p.opening_half_day, p.charted, p.destination_known,
        p.sponsored_guide);
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
                    for (int variant=0; variant<8; ++variant) {
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
                        printf("%u %u %d %d %d ",versions[v],seeds[seed],r,reverse,variant);
                        Preview(reverse ? route->from_id : route->to_id);
                    }
            Preview(0);
        }
    return 0;
}
