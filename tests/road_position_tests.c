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
    CHECK(CcRoadGeometryKnownFixtures());
    CcSim sim;
    CcSimInit(&sim, UINT32_C(0x3235a7ed));
    CcPilotRoadTopology pilot;
    CHECK(CcPilotRoadTopologyBuild(&sim, &pilot));
    CcRoadGeometry geometry;
    CHECK(CcRoadGeometryBuild(&sim, pilot.route_id, &geometry));
    CHECK(geometry.full_length_units == 261356);
    CHECK(geometry.journey_length_units == 204556);
    CHECK(geometry.control.x_units == 147659);
    CHECK(geometry.control.z_units == 307287);
    CHECK(pilot.route_id != 0U && pilot.mill_site_id != 0U);
    CHECK(pilot.origin_id == sim.settlements[0].id);
    CHECK(pilot.destination_id == sim.settlements[1].id);
    CHECK(pilot.junction_progress_milli == 820);
    CHECK(pilot.main_length_units == geometry.journey_length_units);
    CHECK(pilot.origin_to_junction_units == 167736);
    CHECK(pilot.junction_to_destination_units == 36820);
    CHECK(pilot.origin_to_junction_units ==
          CcRoadScaleDistance(pilot.main_length_units, 820));
    CHECK(pilot.origin_to_junction_units +
          pilot.junction_to_destination_units == pilot.main_length_units);
    CHECK(pilot.mill_spur_length_units == 24000);
    CHECK(pilot.checkpoint_distance_units ==
          CcRoadScaleDistance(pilot.main_length_units, 500));
    CHECK(pilot.checkpoint_distance_units == 102278);

    CcPilotRoadTopology resized = pilot;
    sim.settlements[0].size = CC_SETTLEMENT_CAPITAL_SIZE;
    sim.settlements[0].population = 999999;
    sim.settlements[1].size = CC_SETTLEMENT_HAMLET;
    sim.settlements[1].population = 1;
    CHECK(CcPilotRoadTopologyBuild(&sim, &resized));
    CHECK(resized.main_length_units != pilot.main_length_units);
    CcPilotRoadTopology frozen;
    CHECK(CcPilotRoadTopologyBuildWithLength(
        &sim, geometry.journey_length_units, &frozen));
    CHECK(frozen.main_length_units == pilot.main_length_units);
    CHECK(frozen.origin_to_junction_units ==
          pilot.origin_to_junction_units);
    CHECK(frozen.junction_to_destination_units ==
          pilot.junction_to_destination_units);
    CHECK(resized.mill_spur_length_units == pilot.mill_spur_length_units);

    CHECK(CcRoadScaleDistance(52000, 1) == 52);
    CHECK(CcRoadScaleDistance(52000, 999) == 51948);
    CHECK(CcRoadProgressMilli(42640, 52000) == 820);
    CHECK(CcRoadTravelSubticks(26000, 1001, 52000) == 501);

    uint64_t token = CcRoadPreviewToken(
        UINT64_C(323001), pilot.destination_id, pilot.junction_id,
        pilot.origin_to_junction_units, pilot.origin_to_junction_units, 0,
        9U,
        pilot.destination_segment_id, CC_ROAD_DIRECTION_FORWARD);
    CHECK(token != 0U);
    CHECK(token == CcRoadPreviewToken(
        UINT64_C(323001), pilot.destination_id, pilot.junction_id,
        pilot.origin_to_junction_units, pilot.origin_to_junction_units, 0,
        9U,
        pilot.destination_segment_id, CC_ROAD_DIRECTION_FORWARD));
    CHECK(token != CcRoadPreviewToken(
        UINT64_C(323001), pilot.destination_id, pilot.junction_id,
        pilot.origin_to_junction_units, pilot.origin_to_junction_units, 0,
        10U,
        pilot.destination_segment_id, CC_ROAD_DIRECTION_FORWARD));
    CHECK(token != CcRoadPreviewToken(
        UINT64_C(323001), pilot.destination_id, pilot.junction_id,
        pilot.origin_to_junction_units, pilot.origin_to_junction_units, 0,
        9U,
        pilot.mill_segment_id, CC_ROAD_DIRECTION_FORWARD));
    CHECK(token != CcRoadPreviewToken(
        UINT64_C(323001), pilot.destination_id, pilot.junction_id,
        pilot.origin_to_junction_units, pilot.origin_to_junction_units, 0,
        9U,
        pilot.destination_segment_id, CC_ROAD_DIRECTION_REVERSE));
    CHECK(token != CcRoadPreviewToken(
        UINT64_C(323002), pilot.destination_id, pilot.junction_id,
        pilot.origin_to_junction_units, pilot.origin_to_junction_units, 0,
        9U, pilot.destination_segment_id, CC_ROAD_DIRECTION_FORWARD));
    CHECK(token != CcRoadPreviewToken(
        UINT64_C(323001), pilot.destination_id, pilot.junction_id,
        pilot.origin_to_junction_units + 1,
        pilot.origin_to_junction_units, 0, 9U,
        pilot.destination_segment_id, CC_ROAD_DIRECTION_FORWARD));
    CHECK(token != CcRoadPreviewToken(
        UINT64_C(323001), pilot.destination_id, pilot.junction_id,
        pilot.origin_to_junction_units,
        pilot.origin_to_junction_units - 1, 1, 9U,
        pilot.destination_segment_id, CC_ROAD_DIRECTION_FORWARD));

    CcRoute first_route = sim.routes[0];
    sim.routes[0] = sim.routes[7];
    sim.routes[7] = first_route;
    CHECK(CcPilotRoadTopologyBuild(&sim, &resized));
    CHECK(resized.route_id == pilot.route_id);

    CHECK(token == UINT64_C(16498057034038110418));

    CcSim journey;
    CcSimInit(&journey, UINT32_C(0x3235a7ed));
    CcPilotRoadTopology journey_pilot;
    CHECK(CcPilotRoadTopologyBuild(&journey, &journey_pilot));
    journey.journey.active = true;
    journey.journey.origin_id = journey_pilot.origin_id;
    journey.journey.destination_id = journey_pilot.destination_id;
    journey.journey.route_id = journey_pilot.route_id;
    journey.journey.total_subticks = 10000;
    for (int32_t i = 0; i < journey.road_site_count; ++i) {
        if (journey.road_sites[i].id != journey_pilot.mill_site_id)
            journey.journey.road_site_stop_mask |= UINT32_C(1) << i;
    }
    CHECK(CcRoadBeginPilotJourney(&journey, UINT64_C(3239001)));
    CHECK(CcRoadSavedPositionValid(&journey));
    CHECK(journey.journey.road_geometry_length_units == 204556);
    CHECK(journey.journey.road_geometry_x_units[1] ==
          geometry.samples[1].x_units);
    CHECK(CcRoadAdvanceLeg(&journey, INT32_MAX));
    CHECK(CcRoadSavedPositionValid(&journey));
    CHECK(journey.journey.road_coordinate_units == 102278);
    CHECK(journey.journey.road_anchor_id == journey_pilot.checkpoint_id);

    CcRoadLegPreview previews[3];
    int32_t preview_count = CcRoadNextLegPreviews(&journey, previews, 3);
    CHECK(preview_count == 2);
    const CcRoadLegPreview *toward_junction = NULL;
    for (int32_t i = 0; i < preview_count; ++i) {
        if (previews[i].direction == CC_ROAD_DIRECTION_FORWARD)
            toward_junction = &previews[i];
    }
    CHECK(toward_junction != NULL);
    uint64_t checkpoint_token = toward_junction->decision_token;
    CHECK(CcRoadChooseNextLeg(&journey, checkpoint_token, NULL, 0));
    CHECK(CcRoadSavedPositionValid(&journey));
    CHECK(!CcRoadChooseNextLeg(&journey, checkpoint_token, NULL, 0));

    int32_t outward_time = journey.journey.road_leg_total_subticks;
    int32_t outward_length = journey.journey.road_leg_length_units;
    CHECK(!CcRoadAdvanceLeg(&journey, outward_time / 2));
    int32_t half_distance = journey.journey.road_distance_travelled_units;
    preview_count = CcRoadNextLegPreviews(&journey, previews, 3);
    CHECK(preview_count == 1);
    CHECK(previews[0].direction == CC_ROAD_DIRECTION_REVERSE);
    CHECK(previews[0].length_units == half_distance);
    CHECK(CcRoadChooseNextLeg(
        &journey, previews[0].decision_token, NULL, 0));
    CHECK(journey.journey.road_leg_length_units == half_distance);
    CHECK(journey.journey.road_leg_total_subticks ==
          CcRoadTravelSubticks(half_distance, 10000, 204556));
    CHECK(CcRoadAdvanceLeg(&journey, INT32_MAX));
    CHECK(CcRoadSavedPositionValid(&journey));
    CHECK(journey.journey.road_coordinate_units == 102278);

    preview_count = CcRoadNextLegPreviews(&journey, previews, 3);
    for (int32_t i = 0; i < preview_count; ++i) {
        if (previews[i].direction == CC_ROAD_DIRECTION_FORWARD)
            toward_junction = &previews[i];
    }
    CHECK(CcRoadChooseNextLeg(
        &journey, toward_junction->decision_token, NULL, 0));
    CHECK(CcRoadAdvanceLeg(&journey, INT32_MAX));
    CHECK(CcRoadSavedPositionValid(&journey));
    CHECK(journey.journey.road_anchor_id == journey_pilot.junction_id);
    CHECK(journey.journey.road_coordinate_units == 167736);
    preview_count = CcRoadNextLegPreviews(&journey, previews, 3);
    CHECK(preview_count == 3);
    const CcRoadLegPreview *mill = NULL;
    for (int32_t i = 0; i < preview_count; ++i) {
        if (previews[i].segment_id == journey_pilot.mill_segment_id)
            mill = &previews[i];
    }
    CHECK(mill != NULL && mill->length_units == 24000);
    CHECK(CcRoadChooseNextLeg(&journey, mill->decision_token, NULL, 0));
    CHECK(CcRoadAdvanceLeg(&journey, INT32_MAX));
    CHECK(CcRoadSavedPositionValid(&journey));
    CHECK(journey.journey.road_anchor_id == journey_pilot.mill_site_id);
    CHECK(journey.journey.road_compatibility_milli == 820);
    preview_count = CcRoadNextLegPreviews(&journey, previews, 3);
    CHECK(preview_count == 1);
    CHECK(CcRoadChooseNextLeg(
        &journey, previews[0].decision_token, NULL, 0));
    CHECK(CcRoadAdvanceLeg(&journey, INT32_MAX));
    CHECK(CcRoadSavedPositionValid(&journey));
    CHECK(journey.journey.road_anchor_id == journey_pilot.junction_id);
    CHECK(journey.journey.road_coordinate_units == 167736);
    preview_count = CcRoadNextLegPreviews(&journey, previews, 3);
    const CcRoadLegPreview *destination_leg = NULL;
    for (int32_t i = 0; i < preview_count; ++i) {
        if (previews[i].destination_anchor_id == journey_pilot.destination_id)
            destination_leg = &previews[i];
    }
    CHECK(destination_leg != NULL);
    CHECK(CcRoadChooseNextLeg(
        &journey, destination_leg->decision_token, NULL, 0));
    CHECK(CcRoadAdvanceLeg(&journey, INT32_MAX));
    CHECK(CcRoadSavedPositionValid(&journey));
    CHECK(journey.journey.road_anchor_id == journey_pilot.destination_id);
    CHECK(journey.journey.road_coordinate_units == 204556);
    CHECK(outward_length > half_distance);

    CcSim single_steps;
    CcSim large_step;
    CcSimInit(&single_steps, UINT32_C(0x3235a7ed));
    single_steps.journey.active = true;
    single_steps.journey.origin_id = journey_pilot.origin_id;
    single_steps.journey.destination_id = journey_pilot.destination_id;
    single_steps.journey.route_id = journey_pilot.route_id;
    single_steps.journey.total_subticks = 10000;
    for (int32_t i = 0; i < single_steps.road_site_count; ++i) {
        if (single_steps.road_sites[i].id != journey_pilot.mill_site_id)
            single_steps.journey.road_site_stop_mask |= UINT32_C(1) << i;
    }
    CHECK(CcRoadBeginPilotJourney(&single_steps, UINT64_C(3239002)));
    large_step = single_steps;
    int32_t first_leg_time = single_steps.journey.road_leg_total_subticks;
    for (int32_t i = 0; i < first_leg_time; ++i)
        (void)CcRoadAdvanceLeg(&single_steps, 1);
    CHECK(CcRoadAdvanceLeg(&large_step, INT32_MAX));
    CHECK(single_steps.journey.road_coordinate_units ==
          large_step.journey.road_coordinate_units);
    CHECK(single_steps.journey.road_anchor_id ==
          large_step.journey.road_anchor_id);
    CHECK(single_steps.journey.road_revision ==
          large_step.journey.road_revision);

    puts("Pilot road identity, physical frozen basis, and preview tokens passed.");
    return EXIT_SUCCESS;
}
