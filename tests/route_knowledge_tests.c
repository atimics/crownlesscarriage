#include "persistence/cc_save.h"
#include "sim/cc_sim.h"

#include "test_support.h"

#include <stdio.h>

static void RemoveDatabase(const char *path)
{
    char sidecar[384];
    (void)remove(path);
    (void)snprintf(sidecar, sizeof(sidecar), "%s-wal", path);
    (void)remove(sidecar);
    (void)snprintf(sidecar, sizeof(sidecar), "%s-shm", path);
    (void)remove(sidecar);
}

static void RemovePlayerChartForRoute(CcSim *sim, CcId route_id)
{
    for (int32_t i = 0; i < sim->map_count; ++i) {
        CcMap *map = &sim->maps[i];
        if (map->route_id != route_id ||
            map->owner_id != sim->player.id) continue;
        map->owner_id = sim->player.location_id;
        sim->player.map_catalogue_mask &=
            ~(UINT32_C(1) << (uint32_t)i);
        sim->player.map_archive_mask &=
            ~(UINT32_C(1) << (uint32_t)i);
    }
}

static void AdvanceJourneyToArrival(CcSim *sim)
{
    while (sim->journey.active &&
           sim->journey.phase == CC_JOURNEY_PHASE_TRAVELLING) {
        CcSimAdvanceRuntimeTicks(sim, CC_WORLD_TICKS_PER_SECOND);
    }
}

int main(void)
{
    const char *path = "route-knowledge-test.ccsave";
    RemoveDatabase(path);

    CcSim sim;
    CcSimInit(&sim, UINT32_C(0x125cafe));
    CcRoute *road = &sim.routes[0];
    CcId origin_id = road->from_id;
    CcId destination_id = road->to_id;
    CC_CHECK(sim.player.location_id == origin_id);
    RemovePlayerChartForRoute(&sim, road->id);
    road->security = 100;
    road->condition = 100;

    int32_t from_reveal = 0;
    int32_t to_reveal = 0;
    bool charted = true;
    CC_CHECK(CcSimPlayerRouteReveal(
        &sim, road->id, &from_reveal, &to_reveal, &charted));
    CC_CHECK(!charted);
    CC_CHECK(from_reveal == 280);
    CC_CHECK(to_reveal == 0);
    CC_CHECK(!CcSimPlayerRouteReveal(
        &sim, sim.routes[2].id, NULL, NULL, NULL));
    CC_CHECK(!CcSimPlayerKnowsSettlement(
        &sim, sim.routes[2].to_id));

    char error[256];
    CcJournal *journal = CcJournalStart(
        path, &sim, error, sizeof(error));
    CC_CHECK(journal != NULL);
    CcCommand depart = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = destination_id
    };
    CC_CHECK(CcJournalApply(journal, &sim, &depart,
                            error, sizeof(error)));
    CC_CHECK(sim.journey.active);
    CC_CHECK(!sim.journey.ambush_pending);
    CC_CHECK(CcJournalAdvanceRuntimeTicks(
        journal, &sim, 2400, error, sizeof(error)));
    CC_CHECK(sim.journey.active);
    const CcRouteKnowledge *partial = CcSimPlayerRouteKnowledge(
        &sim, road->id);
    CC_CHECK(partial != NULL);
    CC_CHECK(partial->from_reveal_milli > 280);
    CC_CHECK(partial->from_reveal_milli < 1000);
    CC_CHECK(partial->to_reveal_milli == 0);
    int32_t saved_partial_reveal = partial->from_reveal_milli;
    uint64_t saved_partial_hash = CcSimHash(&sim);
    CC_CHECK(CcJournalClose(&journal, &sim, error, sizeof(error)));

    CcSim restored;
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(&restored) == saved_partial_hash);
    partial = CcSimPlayerRouteKnowledge(&restored, road->id);
    CC_CHECK(partial != NULL);
    CC_CHECK(partial->from_reveal_milli == saved_partial_reveal);
    CC_CHECK(partial->to_reveal_milli == 0);
    CC_CHECK(restored.journey.active);

    restored.journey.ambush_pending = false;
    AdvanceJourneyToArrival(&restored);
    CC_CHECK(!restored.journey.active);
    CC_CHECK(restored.player.location_id == destination_id);
    const CcRouteKnowledge *complete = CcSimPlayerRouteKnowledge(
        &restored, road->id);
    CC_CHECK(complete != NULL);
    CC_CHECK(complete->from_reveal_milli == 1000);
    CC_CHECK(complete->to_reveal_milli == 1000);
    CC_CHECK(CcSimPlayerKnowsSettlement(&restored, origin_id));
    CC_CHECK(CcSimPlayerKnowsSettlement(&restored, destination_id));

    const CcRoute *next_road = CcSimRouteBetween(
        &restored, destination_id, restored.settlements[2].id);
    CC_CHECK(next_road != NULL);
    CC_CHECK(CcSimPlayerRouteReveal(
        &restored, next_road->id,
        &from_reveal, &to_reveal, &charted));
    CC_CHECK(!charted);
    CC_CHECK(from_reveal == 280 || to_reveal == 280);
    CC_CHECK(from_reveal == 0 || to_reveal == 0);
    CC_CHECK(!CcSimPlayerKnowsSettlement(
        &restored, restored.settlements[2].id));

    CcCommand return_home = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = origin_id
    };
    CC_CHECK(CcSimApply(&restored, &return_home,
                        error, sizeof(error)));
    restored.journey.ambush_pending = false;
    CC_CHECK(CcSimPlayerRouteReveal(
        &restored, road->id, &from_reveal, &to_reveal, &charted));
    CC_CHECK(from_reveal == 1000);
    CC_CHECK(to_reveal == 1000);
    AdvanceJourneyToArrival(&restored);
    CC_CHECK(!restored.journey.active);
    CC_CHECK(restored.player.location_id == origin_id);

    uint64_t completed_hash = CcSimHash(&restored);
    CC_CHECK(CcSaveWrite(path, &restored, error, sizeof(error)));
    CcSim completed_restore;
    CC_CHECK(CcSaveRead(path, &completed_restore,
                        error, sizeof(error)));
    CC_CHECK(CcSimHash(&completed_restore) == completed_hash);
    complete = CcSimPlayerRouteKnowledge(&completed_restore, road->id);
    CC_CHECK(complete != NULL);
    CC_CHECK(complete->from_reveal_milli == 1000);
    CC_CHECK(complete->to_reveal_milli == 1000);
    CC_CHECK(CcSimValidate(&completed_restore, error, sizeof(error)));

    RemoveDatabase(path);
    puts("Persistent route knowledge tests passed");
    return 0;
}
