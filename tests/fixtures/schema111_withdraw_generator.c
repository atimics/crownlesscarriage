#include "persistence/cc_save.h"
#include "sim/cc_road_position.h"
#include "sim/cc_sim.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

static bool ContinueRoad(CcSim *sim, char *error, size_t capacity)
{
    if (sim->journey.phase == CC_JOURNEY_PHASE_ROAD_CHOICE) {
        CcRoadLegPreview choices[3];
        int32_t count = CcRoadNextLegPreviews(sim, choices, 3);
        for (int32_t i = 0; i < count; ++i) {
            if (choices[i].direction != sim->journey.road_direction ||
                choices[i].segment_id == CC_PILOT_ROAD_MILL_SEGMENT_ID)
                continue;
            CcCommand leg = {
                .kind = CC_COMMAND_CHOOSE_ROAD_LEG,
                .target_id = choices[i].decision_token
            };
            return CcSimApply(sim, &leg, error, capacity);
        }
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

int main(int argc, char **argv)
{
    if (argc != 2) {
        (void)fprintf(stderr, "Usage: schema111_withdraw_generator output.ccsave\n");
        return 2;
    }
    CcSim *sim = malloc(sizeof(*sim));
    if (sim == NULL) return 1;
    CcSimInit(sim, UINT32_C(0x5ca17));
    CcRoute *route = &sim->routes[0];
    route->closed = false;
    route->travel_days = 3;
    sim->journey.total_subticks = CC_WORLD_WATCH_SUBTICKS;
    sim->player.location_id = route->from_id;
    sim->carriage.location_id = route->from_id;
    sim->bandits[0].route_id = route->id;
    CcCommand travel = {.kind = CC_COMMAND_TRAVEL, .target_id = route->to_id};
    char error[192] = "";
    if (!CcSimApply(sim, &travel, error, sizeof(error))) goto fail;
    sim->journey.ambush_pending = true;
    sim->journey.ambush_warned = false;
    sim->journey.ambush_resolved = false;
    sim->journey.encounter_triggered = false;
    for (int32_t step = 0; step < 6000 &&
         sim->journey.phase != CC_JOURNEY_PHASE_BLOCKED; ++step) {
        if (!ContinueRoad(sim, error, sizeof(error))) goto fail;
    }
    if (sim->journey.phase != CC_JOURNEY_PHASE_BLOCKED) goto fail;
    sim->clock.minute_subticks = CC_WORLD_DAY_SUBTICKS - 60;
    uint64_t base_hash = CcSimHash(sim);
    CcJournal *journal = CcJournalStart(argv[1], sim, error, sizeof(error));
    if (journal == NULL) goto fail;
    CcCommand withdraw = {.kind = CC_COMMAND_WITHDRAW_ENCOUNTER};
    if (!CcJournalApply(journal, sim, &withdraw, error, sizeof(error))) {
        CcJournalAbandon(&journal);
        goto fail;
    }
    uint64_t post_hash = CcSimHash(sim);
    if (!CcJournalClose(&journal, sim, error, sizeof(error))) goto fail;
    (void)printf("schema=%u base=%016" PRIx64 " post=%016" PRIx64
                 " day=%d subticks=%d\n", sim->schema_version,
                 base_hash, post_hash, sim->current_day,
                 sim->clock.minute_subticks);
    free(sim);
    return 0;
fail:
    (void)fprintf(stderr, "%s\n", error);
    free(sim);
    return 1;
}
