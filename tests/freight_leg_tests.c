#include "sim/cc_sim.h"
#include "test_support.h"
#include <limits.h>
static CcSim sim;

int main(void)
{
    CcSimInit(&sim, UINT32_C(0x5eed0001));
    uint64_t hash = CcSimHash(&sim);
    CcFreightLeg leg;
    for (int32_t i = 0; i < sim.route_count; ++i) {
        const CcRoute *route = &sim.routes[i];
        CC_CHECK(CcSimFreightLeg(&sim, route->id, route->from_id, route->to_id, &leg));
        CC_CHECK(leg.travel_days == route->travel_days && leg.origin_milli == 0 && leg.destination_milli == 1000);
        CC_CHECK(CcSimFreightLegDays(&sim, route->id, route->to_id, route->from_id) == route->travel_days);
        CC_CHECK(!CcSimFreightLeg(&sim, route->id, route->from_id, route->from_id, &leg));
        CC_CHECK(leg.travel_days == 0 && leg.route_id == 0);
    }
    for (int32_t i = 0; i < sim.road_site_count; ++i) {
        const CcRoadSite *site = &sim.road_sites[i];
        const CcRoute *route = CcSimRoute(&sim, site->route_id);
        CC_CHECK(route != NULL);
        for (int32_t reverse = 0; reverse < 2; ++reverse) {
            CcId town = reverse ? route->to_id : route->from_id;
            int32_t distance = reverse ? 1000 - site->progress_milli : site->progress_milli;
            int32_t expected = (route->travel_days * distance + 999) / 1000;
            CC_CHECK(CcSimFreightLeg(&sim, route->id, town, site->id, &leg));
            CC_CHECK(leg.route_id == route->id && leg.origin_id == town && leg.destination_id == site->id);
            CC_CHECK(leg.travel_days == expected && leg.destination_milli == site->progress_milli);
            CC_CHECK(CcSimFreightLegDays(&sim, route->id, site->id, town) == expected);
        }
        for (int32_t j = 0; j < sim.road_site_count; ++j) {
            const CcRoadSite *other = &sim.road_sites[j];
            bool expected = i != j && site->route_id == other->route_id;
            CC_CHECK(CcSimFreightLeg(&sim, route->id, site->id, other->id, &leg) == expected);
            if (expected) CC_CHECK(leg.travel_days >= 1 && leg.travel_days <= route->travel_days);
        }
    }
    CC_CHECK(CcSimHash(&sim) == hash);
    CC_CHECK(!CcSimFreightLeg(NULL, 0, 1, 2, &leg));
    CC_CHECK(!CcSimFreightLeg(&sim, sim.routes[0].id, sim.player.id, sim.routes[0].to_id, NULL));
    CcRoadSite *site = &sim.road_sites[0];
    int32_t days = CcSimFreightLegDays(&sim, site->route_id, sim.routes[0].from_id, site->id);
    site->home_settlement_id = sim.settlements[5].id;
    CC_CHECK(CcSimFreightLegDays(&sim, site->route_id, sim.routes[0].from_id, site->id) == days);
    site->progress_milli = 1001;
    CC_CHECK(CcSimFreightLegDays(&sim, site->route_id, sim.routes[0].from_id, site->id) == 0);
    sim.routes[0].travel_days = INT32_MAX;
    CC_CHECK(CcSimFreightLegDays(&sim, sim.routes[0].id, sim.routes[0].from_id, sim.routes[0].to_id) == 0);
    puts("Freight legs: towns, site positions, both directions, invalid endpoints and readonly geometry passed");
    return 0;
}
