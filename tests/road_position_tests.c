#include "sim/cc_road_position.h"

#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        (void)fprintf(stderr, "check failed at line %d: %s\n", \
                      __LINE__, #condition); \
        return EXIT_FAILURE; \
    } \
} while (0)

int main(void)
{
    CcSim sim;
    CcSimInit(&sim, UINT32_C(0x3235a7ed));
    CcPilotRoadTopology pilot;
    CHECK(CcPilotRoadTopologyBuild(&sim, &pilot));
    CHECK(pilot.route_id != 0U && pilot.mill_site_id != 0U);
    CHECK(pilot.origin_id == sim.settlements[0].id);
    CHECK(pilot.destination_id == sim.settlements[1].id);
    CHECK(pilot.junction_progress_milli == 820);
    CHECK(pilot.origin_to_junction_units == 42640);
    CHECK(pilot.junction_to_destination_units == 9360);
    CHECK(pilot.mill_spur_length_units == 24000);
    CHECK(pilot.checkpoint_distance_units == 26000);

    CcPilotRoadTopology resized = pilot;
    sim.settlements[0].size = CC_SETTLEMENT_CAPITAL_SIZE;
    sim.settlements[0].population = 999999;
    sim.settlements[1].size = CC_SETTLEMENT_HAMLET;
    sim.settlements[1].population = 1;
    CHECK(CcPilotRoadTopologyBuild(&sim, &resized));
    CHECK(resized.origin_to_junction_units ==
          pilot.origin_to_junction_units);
    CHECK(resized.junction_to_destination_units ==
          pilot.junction_to_destination_units);
    CHECK(resized.mill_spur_length_units == pilot.mill_spur_length_units);

    CHECK(CcRoadScaleDistance(52000, 1) == 52);
    CHECK(CcRoadScaleDistance(52000, 999) == 51948);
    CHECK(CcRoadProgressMilli(42640, 52000) == 820);
    CHECK(CcRoadTravelSubticks(26000, 1001, 52000) == 501);

    uint64_t token = CcRoadPreviewToken(
        pilot.destination_id, pilot.junction_id, 9U,
        pilot.destination_segment_id, CC_ROAD_DIRECTION_FORWARD);
    CHECK(token != 0U);
    CHECK(token == CcRoadPreviewToken(
        pilot.destination_id, pilot.junction_id, 9U,
        pilot.destination_segment_id, CC_ROAD_DIRECTION_FORWARD));
    CHECK(token != CcRoadPreviewToken(
        pilot.destination_id, pilot.junction_id, 10U,
        pilot.destination_segment_id, CC_ROAD_DIRECTION_FORWARD));
    CHECK(token != CcRoadPreviewToken(
        pilot.destination_id, pilot.junction_id, 9U,
        pilot.mill_segment_id, CC_ROAD_DIRECTION_FORWARD));
    CHECK(token != CcRoadPreviewToken(
        pilot.destination_id, pilot.junction_id, 9U,
        pilot.destination_segment_id, CC_ROAD_DIRECTION_REVERSE));

    CcRoute first_route = sim.routes[0];
    sim.routes[0] = sim.routes[7];
    sim.routes[7] = first_route;
    CHECK(CcPilotRoadTopologyBuild(&sim, &resized));
    CHECK(resized.route_id == pilot.route_id);

    puts("Pilot road identity, fixed distance, and preview tokens passed.");
    return EXIT_SUCCESS;
}
