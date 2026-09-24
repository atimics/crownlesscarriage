#include "persistence/cc_save.h"
#include "sim/cc_road_position.h"
#include "sim/cc_sim.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

int main(int argc, char **argv)
{
    if (argc != 3 || (strcmp(argv[1], "warning") != 0 &&
                       strcmp(argv[1], "blocked") != 0)) {
        (void)fprintf(stderr, "Usage: road_block_browser_fixture warning|blocked output\n");
        return 2;
    }
    CcSim *sim = malloc(sizeof(*sim));
    if (sim == NULL) return 1;
    CcSimInit(sim, UINT32_C(0x5ca17));
    CcRoute *route = &sim->routes[0];
    route->closed = false;
    sim->player.location_id = route->from_id;
    sim->carriage.location_id = route->from_id;
    sim->bandits[0].route_id = route->id;
    CcCommand travel = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = route->to_id
    };
    char error[192] = "";
    if (!CcSimApply(sim, &travel, error, sizeof(error))) goto fail;
    sim->journey.ambush_pending = true;
    sim->journey.ambush_warned = false;
    sim->journey.ambush_resolved = false;
    sim->journey.encounter_triggered = false;
    for (int32_t step = 0; step < 6000 && !sim->journey.ambush_warned;
         ++step) {
        if (!ContinueRoad(sim, error, sizeof(error))) goto fail;
    }
    if (!sim->journey.ambush_warned) {
        (void)snprintf(error, sizeof(error), "The scout warning is missing.");
        goto fail;
    }
    if (strcmp(argv[1], "blocked") == 0) {
        for (int32_t step = 0; step < 6000 &&
             sim->journey.phase != CC_JOURNEY_PHASE_BLOCKED; ++step) {
            if (!ContinueRoad(sim, error, sizeof(error))) goto fail;
        }
        if (sim->journey.phase != CC_JOURNEY_PHASE_BLOCKED) {
            (void)snprintf(error, sizeof(error), "The road block is missing.");
            goto fail;
        }
    }
    unsigned char *bytes = NULL;
    size_t length = 0;
    if (!CcSaveEncode(sim, &bytes, &length, error, sizeof(error))) goto fail;
    FILE *file = fopen(argv[2], "wb");
    if (file == NULL || fwrite(bytes, 1, length, file) != length) {
        (void)snprintf(error, sizeof(error), "The fixture could not be written.");
        if (file != NULL) fclose(file);
        free(bytes);
        goto fail;
    }
    fclose(file);
    free(bytes);
    free(sim);
    return 0;
fail:
    (void)fprintf(stderr, "%s\n", error);
    free(sim);
    return 1;
}
