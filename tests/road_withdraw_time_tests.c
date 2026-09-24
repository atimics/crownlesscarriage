#include "persistence/cc_save.h"
#include "sim/cc_road_position.h"
#include "sim/cc_sim.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        (void)fprintf(stderr, "check failed at line %d: %s\n", \
                      __LINE__, #condition); \
        return EXIT_FAILURE; \
    } \
} while (0)

static bool ContinueRoad(CcSim *sim, char *error, size_t capacity)
{
    if (sim->journey.phase == CC_JOURNEY_PHASE_ROAD_CHOICE) {
        CcRoadLegPreview previews[3];
        int32_t count = CcRoadNextLegPreviews(sim, previews, 3);
        for (int32_t i = 0; i < count; ++i) {
            if (previews[i].direction != sim->journey.road_direction ||
                previews[i].segment_id == CC_PILOT_ROAD_MILL_SEGMENT_ID)
                continue;
            CcCommand leg = {
                .kind = CC_COMMAND_CHOOSE_ROAD_LEG,
                .target_id = previews[i].decision_token
            };
            return CcSimApply(sim, &leg, error, capacity);
        }
        (void)snprintf(error, capacity, "A forward road leg is missing.");
        return false;
    }
    if (sim->journey.phase == CC_JOURNEY_PHASE_RESTING) {
        CcCommand rest = {
            .kind = CcSimJourneyStop(sim) == CC_JOURNEY_STOP_MIDDAY ?
                CC_COMMAND_TAKE_JOURNEY_BREAK : CC_COMMAND_MAKE_CAMP
        };
        return CcSimApply(sim, &rest, error, capacity);
    }
    CcSimAdvanceRuntimeTicks(sim, CC_WORLD_TICKS_PER_SECOND);
    return true;
}

static bool ReachBanditBlock(CcSim *sim, char *error, size_t capacity)
{
    CcSimInit(sim, UINT32_C(0x5ca17));
    CcRoute *route = &sim->routes[0];
    route->closed = false;
    route->travel_days = 3;
    /* The opening carriage ride is one watch; this is a later journey. */
    sim->journey.total_subticks = CC_WORLD_WATCH_SUBTICKS;
    sim->player.location_id = route->from_id;
    sim->carriage.location_id = route->from_id;
    sim->bandits[0].route_id = route->id;
    CcCommand travel = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = route->to_id
    };
    if (!CcSimApply(sim, &travel, error, capacity)) return false;
    sim->journey.ambush_pending = true;
    sim->journey.ambush_warned = false;
    sim->journey.ambush_resolved = false;
    sim->journey.encounter_triggered = false;
    for (int32_t step = 0; step < 6000 &&
         sim->journey.phase != CC_JOURNEY_PHASE_BLOCKED; ++step) {
        if (!ContinueRoad(sim, error, capacity)) return false;
    }
    return sim->journey.phase == CC_JOURNEY_PHASE_BLOCKED &&
        sim->journey.ambush_warned;
}

int main(void)
{
    static CcSim blocked, saved, loaded, legacy, careful, push;
    char error[192] = "";
    CHECK(ReachBanditBlock(&blocked, error, sizeof(error)));
    CHECK(blocked.schema_version == 112U);
    int32_t physical = CcRoadRouteProgressSubticks(&blocked);
    CHECK(physical > CC_WORLD_WATCH_SUBTICKS);
    CHECK(physical < blocked.journey.total_subticks);
    CHECK(blocked.journey.pace == CC_JOURNEY_PACE_STEADY);
    int32_t expected_minutes =
        (physical + CC_WORLD_MINUTE_SUBTICKS - 1) /
            CC_WORLD_MINUTE_SUBTICKS;
    CHECK(CcSimJourneyWithdrawalMinutes(&blocked) == expected_minutes);
    careful = blocked;
    careful.journey.pace = CC_JOURNEY_PACE_CAREFUL;
    push = blocked;
    push.journey.pace = CC_JOURNEY_PACE_PUSH;
    CHECK(CcSimJourneyWithdrawalMinutes(&careful) > expected_minutes);
    CHECK(CcSimJourneyWithdrawalMinutes(&push) < expected_minutes);

    /* The return crosses one midnight while the horses remain on the road. */
    blocked.clock.minute_subticks = CC_WORLD_DAY_SUBTICKS - 60;
    saved = blocked;
    int32_t before_day = blocked.current_day;
    int32_t before_fatigue = blocked.horse_team[0].fatigue;
    int32_t before_hunger = blocked.horse_team[0].hunger;
    int32_t before_condition = blocked.carriage.condition;
    int32_t before_security = blocked.routes[0].security;
    CcMoney before_coins = blocked.player.coins;
    int32_t before_cargo[CC_GOOD_COUNT];
    memcpy(before_cargo, blocked.player.cargo, sizeof(before_cargo));
    CcCommand withdraw = {.kind = CC_COMMAND_WITHDRAW_ENCOUNTER};
    CHECK(CcSimApply(&blocked, &withdraw, error, sizeof(error)));
    CHECK(blocked.current_day == before_day +
          (CC_WORLD_DAY_SUBTICKS - 60 + physical) /
              CC_WORLD_DAY_SUBTICKS);
    CHECK(blocked.clock.minute_subticks ==
        (CC_WORLD_DAY_SUBTICKS - 60 + physical) % CC_WORLD_DAY_SUBTICKS);
    CHECK(!blocked.journey.active &&
          blocked.carriage.mode == CC_CARRIAGE_PARKED);
    CHECK(blocked.player.location_id == saved.journey.origin_id &&
          blocked.carriage.location_id == saved.journey.origin_id);
    CHECK(blocked.horse_team[0].fatigue > before_fatigue);
    CHECK(blocked.horse_team[0].hunger >= before_hunger);
    CHECK(blocked.carriage.condition <= before_condition);
    CHECK(blocked.routes[0].security < before_security);
    CHECK(blocked.player.coins == before_coins);
    CHECK(memcmp(before_cargo, blocked.player.cargo,
                 sizeof(before_cargo)) == 0);
    const CcEvent *receipt = CcSimRecentEvent(&blocked, 0);
    CHECK(receipt != NULL && receipt->kind == CC_EVENT_ENCOUNTER_WITHDRAWN);
    char minutes[32];
    (void)snprintf(minutes, sizeof(minutes), "%d minutes", expected_minutes);
    CHECK(strstr(receipt->text, minutes) != NULL);
    CHECK(CcSimValidate(&blocked, error, sizeof(error)));

    const char *save_path = "road-withdraw-time-test.ccsave";
    (void)remove(save_path);
    CHECK(CcSaveWrite(save_path, &blocked, error, sizeof(error)));
    CHECK(CcSaveRead(save_path, &loaded, error, sizeof(error)));
    CHECK(CcSimHash(&loaded) == CcSimHash(&blocked));
    CHECK(CcSimRecentEvent(&loaded, 0)->kind ==
          CC_EVENT_ENCOUNTER_WITHDRAWN);
    (void)remove(save_path);
    CcCommand depart_again = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = saved.journey.destination_id
    };
    CHECK(CcSimApply(&loaded, &depart_again, error, sizeof(error)));
    CHECK(loaded.journey.active &&
          loaded.routes[0].security == blocked.routes[0].security);

    /* Schema 111 journals keep their original instant-return result. */
    legacy = saved;
    legacy.schema_version = 111U;
    CHECK(CcSimApply(&legacy, &withdraw, error, sizeof(error)));
    CHECK(legacy.current_day == saved.current_day);
    CHECK(legacy.clock.minute_subticks == saved.clock.minute_subticks);
    CHECK(strstr(CcSimRecentEvent(&legacy, 0)->text,
                 "before blood is drawn") != NULL);
    return EXIT_SUCCESS;
}
