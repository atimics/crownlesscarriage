#include "multiplayer/cc_coop.h"
#include "persistence/cc_save.h"
#include "sim/cc_mine.h"
#include "sim/cc_road_position.h"
#include "test_support.h"

#include <stdio.h>
#include <string.h>

static char error[256];

static const CcRoadLegPreview *FindLeg(
    const CcRoadLegPreview *previews, int32_t count,
    CcId destination, CcRoadDirection direction)
{
    for (int32_t i = 0; i < count; ++i) {
        if (previews[i].destination_anchor_id == destination &&
            previews[i].direction == direction) return &previews[i];
    }
    return NULL;
}

static void ChooseLeg(CcSim *sim, CcId destination,
                      CcRoadDirection direction)
{
    CcRoadLegPreview previews[3];
    int32_t count = CcRoadNextLegPreviews(sim, previews, 3);
    const CcRoadLegPreview *leg = FindLeg(
        previews, count, destination, direction);
    CC_CHECK(leg != NULL);
    CcCommand choose = {
        .kind = CC_COMMAND_CHOOSE_ROAD_LEG,
        .target_id = leg->decision_token
    };
    CC_CHECK(CcSimApply(sim, &choose, error, sizeof(error)));
}

static void ResumeWatch(CcSim *sim, int32_t *watch_stops,
                        int32_t *maximum_fatigue)
{
    CC_CHECK(sim->journey.phase == CC_JOURNEY_PHASE_RESTING);
    if (watch_stops != NULL) ++*watch_stops;
    for (int32_t i = 0; i < CcSimHorseTeamCount(sim); ++i) {
        if (maximum_fatigue != NULL &&
            sim->horse_team[i].fatigue > *maximum_fatigue)
            *maximum_fatigue = sim->horse_team[i].fatigue;
    }
    CcJourneyStopKind stop = CcSimJourneyStop(sim);
    CcCommand resume = {
        .kind = stop == CC_JOURNEY_STOP_MIDDAY ?
            CC_COMMAND_PRESS_ON : CC_COMMAND_MAKE_CAMP
    };
    CC_CHECK(CcSimApply(sim, &resume, error, sizeof(error)));
}

static bool AdvanceToAnchor(CcSim *sim, CcId target,
                            int32_t *watch_stops,
                            int32_t *maximum_fatigue)
{
    for (int32_t step = 0; step < 20000; ++step) {
        if (sim->journey.road_waiting_choice &&
            sim->journey.road_anchor_id == target) return true;
        if (sim->journey.phase == CC_JOURNEY_PHASE_RESTING) {
            ResumeWatch(sim, watch_stops, maximum_fatigue);
            continue;
        }
        if (sim->journey.phase == CC_JOURNEY_PHASE_BLOCKED) return false;
        if (!sim->journey.road_waiting_choice)
            CC_CHECK(CcSimJourneyRoadSiteStop(sim) == NULL);
        if (sim->journey.road_waiting_choice) {
            const CcRoadSite *site = CcSimJourneyRoadSiteStop(sim);
            if (site != NULL) {
                CcCommand pass = {
                    .kind = CC_COMMAND_PASS_ROAD_SITE,
                    .target_id = site->id
                };
                CC_CHECK(CcSimApply(sim, &pass, error, sizeof(error)));
            } else {
                CcRoadLegPreview previews[3];
                int32_t count = CcRoadNextLegPreviews(sim, previews, 3);
                const CcRoadLegPreview *forward = NULL;
                for (int32_t i = 0; i < count; ++i) {
                    if (previews[i].direction ==
                            sim->journey.road_direction &&
                        previews[i].segment_id !=
                            CC_PILOT_ROAD_MILL_SEGMENT_ID) {
                        forward = &previews[i];
                    }
                }
                CC_CHECK(forward != NULL);
                CcCommand choose = {
                    .kind = CC_COMMAND_CHOOSE_ROAD_LEG,
                    .target_id = forward->decision_token
                };
                CC_CHECK(CcSimApply(sim, &choose, error, sizeof(error)));
            }
            continue;
        }
        CcSimAdvanceRuntimeTicks(sim, 1);
    }
    return false;
}

static void StartPilot(CcSim *sim)
{
    CcSimInit(sim, UINT32_C(0x3235a7ed));
    CcPilotRoadTopology pilot;
    CC_CHECK(CcPilotRoadTopologyBuild(sim, &pilot));
    sim->player.coins = 0;
    CcSettlement *origin = CcSimSettlementMutable(sim, pilot.origin_id);
    CC_CHECK(origin != NULL);
    origin->stock[CC_GOOD_WHEAT] = 10000;
    CcCommand travel = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = pilot.destination_id
    };
    CC_CHECK(CcSimApply(sim, &travel, error, sizeof(error)));
    CC_CHECK(sim->journey.road_position_active);
    CC_CHECK(sim->player.coins == 0);
}

static void CheckFirstStopParity(void)
{
    CcSim one_tick;
    StartPilot(&one_tick);
    CcSim large = one_tick;
    CcSim shared = one_tick;
    for (int32_t tick = 0;
         tick < 1000 && !one_tick.journey.road_waiting_choice; ++tick) {
        CcSimAdvanceRuntimeTicks(&one_tick, 1);
    }
    CcSimAdvanceRuntimeTicks(&large, 1000);
    CC_CHECK(CcCoopAdvanceTravel(
        &shared, 1000, 8, error, sizeof(error)));
    CC_CHECK(one_tick.journey.road_waiting_choice);
    CC_CHECK(one_tick.journey.road_anchor_id ==
             large.journey.road_anchor_id);
    CC_CHECK(one_tick.journey.road_anchor_id ==
             shared.journey.road_anchor_id);
    CC_CHECK(one_tick.journey.road_coordinate_units ==
             large.journey.road_coordinate_units);
    CC_CHECK(one_tick.journey.road_coordinate_units ==
             shared.journey.road_coordinate_units);
    CC_CHECK(one_tick.journey.road_distance_remaining_units == 0);
}

static void CheckMineRoadAdapter(void)
{
    CcSim sim;
    CcSimInit(&sim, UINT32_C(0x3235a7ed));
    const CcRoadSite *mine = CcMineSite(&sim);
    CC_CHECK(mine != NULL);
    const CcRoute *route = CcSimRoute(&sim, mine->route_id);
    CC_CHECK(route != NULL);
    sim.player.location_id = route->from_id;
    sim.carriage.location_id = route->from_id;
    CcId route_id = 0U;
    CcId destination_id = 0U;
    CC_CHECK(CcRoadSiteJourneyTarget(
        &sim, mine->id, &route_id, &destination_id));
    CC_CHECK(route_id == route->id);
    CC_CHECK(destination_id == route->to_id);
    CcCommand travel = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = destination_id
    };
    CC_CHECK(CcSimApply(&sim, &travel, error, sizeof(error)));
    CC_CHECK(sim.journey.route_id == mine->route_id);
    CC_CHECK(!sim.journey.road_position_active);
}

static void CheckSaveReloadAndTokens(void)
{
    CcSim sim;
    StartPilot(&sim);
    CcSimAdvanceRuntimeTicks(&sim, 1000);
    CC_CHECK(sim.journey.road_waiting_choice);
    unsigned char *bytes = NULL;
    size_t length = 0U;
    CC_CHECK(CcSaveEncode(&sim, &bytes, &length, error, sizeof(error)));
    CcSim restored;
    CC_CHECK(CcSaveDecode(
        bytes, length, &restored, error, sizeof(error)));
    CcSaveFreeBuffer(bytes);
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));
    CC_CHECK(memcmp(sim.journey.road_geometry_x_units,
                    restored.journey.road_geometry_x_units,
                    sizeof(sim.journey.road_geometry_x_units)) == 0);

    CcRoadLegPreview previews[3];
    int32_t count = CcRoadNextLegPreviews(&restored, previews, 3);
    CC_CHECK(count == 2);
    const CcRoadLegPreview *forward = NULL;
    for (int32_t i = 0; i < count; ++i) {
        if (previews[i].direction == CC_ROAD_DIRECTION_FORWARD)
            forward = &previews[i];
    }
    CC_CHECK(forward != NULL);
    uint64_t token = forward->decision_token;
    CcCommand choose = {
        .kind = CC_COMMAND_CHOOSE_ROAD_LEG,
        .target_id = token
    };
    CC_CHECK(CcSimApply(&restored, &choose, error, sizeof(error)));
    uint64_t after_choice = CcSimHash(&restored);
    CC_CHECK(!CcSimApply(&restored, &choose, error, sizeof(error)));
    CC_CHECK(strstr(error, "stale") != NULL);
    CC_CHECK(CcSimHash(&restored) == after_choice);
    CcSimAdvanceRuntimeTicks(&restored, 1);
    CC_CHECK(restored.journey.road_distance_travelled_units > 0);

    int32_t frozen_x[33];
    memcpy(frozen_x, restored.journey.road_geometry_x_units,
           sizeof(frozen_x));
    restored.settlements[0].size = CC_SETTLEMENT_CAPITAL_SIZE;
    restored.settlements[0].population = 999999;
    bytes = NULL;
    length = 0U;
    CC_CHECK(CcSaveEncode(
        &restored, &bytes, &length, error, sizeof(error)));
    CcSim midpoint;
    CC_CHECK(CcSaveDecode(
        bytes, length, &midpoint, error, sizeof(error)));
    CcSaveFreeBuffer(bytes);
    CC_CHECK(memcmp(frozen_x, midpoint.journey.road_geometry_x_units,
                    sizeof(frozen_x)) == 0);
    CC_CHECK(midpoint.journey.road_coordinate_units ==
             restored.journey.road_coordinate_units);
    CC_CHECK(midpoint.journey.road_distance_remaining_units ==
             restored.journey.road_distance_remaining_units);
}

static void CheckMidSpurReversal(void)
{
    CcSim sim;
    StartPilot(&sim);
    CcPilotRoadTopology pilot;
    CC_CHECK(CcPilotRoadTopologyBuildWithLength(
        &sim, sim.journey.road_geometry_length_units, &pilot));
    CC_CHECK(AdvanceToAnchor(&sim, pilot.junction_id, NULL, NULL));
    ChooseLeg(&sim, pilot.mill_site_id, CC_ROAD_DIRECTION_FORWARD);
    CcRoadAdvanceLeg(&sim, sim.journey.road_leg_total_subticks / 2);
    int32_t first_coordinate = sim.journey.road_coordinate_units;
    CC_CHECK(first_coordinate > 0 &&
             first_coordinate < pilot.mill_spur_length_units);
    ChooseLeg(&sim, pilot.junction_id, CC_ROAD_DIRECTION_REVERSE);
    CC_CHECK(sim.journey.road_coordinate_units == first_coordinate);
    CC_CHECK(sim.journey.road_leg_start_coordinate_units == first_coordinate);
    CC_CHECK(sim.journey.road_leg_end_coordinate_units == 0);
    CcRoadAdvanceLeg(&sim, sim.journey.road_leg_total_subticks / 2);
    int32_t second_coordinate = sim.journey.road_coordinate_units;
    CC_CHECK(second_coordinate > 0 && second_coordinate < first_coordinate);
    ChooseLeg(&sim, pilot.mill_site_id, CC_ROAD_DIRECTION_FORWARD);
    CC_CHECK(sim.journey.road_coordinate_units == second_coordinate);
    CC_CHECK(sim.journey.road_leg_start_coordinate_units == second_coordinate);
    CC_CHECK(sim.journey.road_leg_end_coordinate_units ==
             pilot.mill_spur_length_units);

    unsigned char *bytes = NULL;
    size_t length = 0U;
    CC_CHECK(CcSaveEncode(&sim, &bytes, &length, error, sizeof(error)));
    CcSim restored;
    CC_CHECK(CcSaveDecode(bytes, length, &restored,
                          error, sizeof(error)));
    CcSaveFreeBuffer(bytes);
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));
    CC_CHECK(restored.journey.road_coordinate_units == second_coordinate);
    CcRoadAdvanceLeg(
        &restored, restored.journey.road_leg_total_subticks / 3);
    int32_t third_coordinate = restored.journey.road_coordinate_units;
    ChooseLeg(&restored, pilot.junction_id, CC_ROAD_DIRECTION_REVERSE);
    CC_CHECK(restored.journey.road_coordinate_units == third_coordinate);
    CcRoadAdvanceLeg(
        &restored, restored.journey.road_leg_total_subticks / 3);
    int32_t fourth_coordinate = restored.journey.road_coordinate_units;
    ChooseLeg(&restored, pilot.mill_site_id, CC_ROAD_DIRECTION_FORWARD);
    CC_CHECK(restored.journey.road_coordinate_units == fourth_coordinate);
}

static void CheckLongDetourAndHazardOrdering(void)
{
    CcSim detour;
    StartPilot(&detour);
    detour.journey.ambush_pending = false;
    CcPilotRoadTopology pilot;
    CC_CHECK(CcPilotRoadTopologyBuildWithLength(
        &detour, detour.journey.road_geometry_length_units, &pilot));
    int32_t watch_stops = 0;
    int32_t maximum_fatigue = 0;
    CC_CHECK(AdvanceToAnchor(
        &detour, pilot.junction_id, &watch_stops, &maximum_fatigue));
    bool crossed_plan = false;
    int32_t watches_at_crossing = 0;
    int32_t fatigue_at_crossing = 0;
    int64_t clock_at_crossing = 0;
    for (int32_t step = 0; step < 50000; ++step) {
        int64_t clock_now =
            (int64_t)detour.current_day * CC_WORLD_DAY_SUBTICKS +
            detour.clock.minute_subticks;
        if (!crossed_plan && detour.journey.elapsed_subticks >
                detour.journey.total_subticks) {
            crossed_plan = true;
            watches_at_crossing = watch_stops;
            fatigue_at_crossing = maximum_fatigue;
            clock_at_crossing = clock_now;
        }
        if (crossed_plan && watch_stops > watches_at_crossing &&
            maximum_fatigue > fatigue_at_crossing &&
            clock_now - clock_at_crossing >= CC_WORLD_WATCH_SUBTICKS) break;
        if (detour.journey.phase == CC_JOURNEY_PHASE_RESTING) {
            ResumeWatch(&detour, &watch_stops, &maximum_fatigue);
        } else if (detour.journey.road_waiting_choice) {
            bool at_junction = detour.journey.road_anchor_id ==
                pilot.junction_id;
            ChooseLeg(&detour,
                at_junction ? pilot.mill_site_id : pilot.junction_id,
                at_junction ? CC_ROAD_DIRECTION_FORWARD :
                              CC_ROAD_DIRECTION_REVERSE);
        } else {
            CcSimAdvanceRuntimeTicks(&detour, 1);
        }
    }
    int64_t clock_after_extra_watch =
        (int64_t)detour.current_day * CC_WORLD_DAY_SUBTICKS +
        detour.clock.minute_subticks;
    CC_CHECK(crossed_plan);
    CC_CHECK(watches_at_crossing > 0);
    CC_CHECK(fatigue_at_crossing > 0);
    CC_CHECK(watch_stops > watches_at_crossing);
    CC_CHECK(maximum_fatigue > fatigue_at_crossing);
    CC_CHECK(clock_after_extra_watch - clock_at_crossing >=
             CC_WORLD_WATCH_SUBTICKS);
    CC_CHECK(CcSimValidate(&detour, error, sizeof(error)));

    CcSim hazard;
    StartPilot(&hazard);
    CC_CHECK(CcPilotRoadTopologyBuildWithLength(
        &hazard, hazard.journey.road_geometry_length_units, &pilot));
    hazard.journey.ambush_pending = false;
    hazard.journey.encounter_triggered = false;
    hazard.journey.situation_id = hazard.situations[0].id;
    hazard.journey.encounter_subticks =
        hazard.journey.total_subticks * 35 / 100;
    CC_CHECK(!AdvanceToAnchor(&hazard, pilot.checkpoint_id, NULL, NULL));
    CC_CHECK(hazard.journey.phase == CC_JOURNEY_PHASE_BLOCKED);
    CC_CHECK(CcRoadRouteProgressSubticks(&hazard) >=
             hazard.journey.encounter_subticks);
    CC_CHECK(hazard.journey.road_coordinate_units <
             pilot.checkpoint_distance_units);
}

int main(void)
{
    CheckFirstStopParity();
    CheckMineRoadAdapter();
    CheckSaveReloadAndTokens();
    CheckMidSpurReversal();
    CheckLongDetourAndHazardOrdering();
    puts("Physical road runtime, shared stop, save, and token tests passed");
    return 0;
}
