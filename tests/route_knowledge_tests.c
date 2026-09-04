#include "client/cc_road_book.h"
#include "persistence/cc_save.h"
#include "sim/cc_sim.h"
#include "world/cc_world.h"

#include "test_support.h"

#include <math.h>
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

static void PrepareJourneyView(CcSim *sim, const CcRoute *route,
                               CcId origin_id)
{
    sim->journey.active = true;
    sim->journey.route_id = route->id;
    sim->journey.origin_id = origin_id;
}

static void CheckForwardSight(const CcRoadBookRouteView *view,
                              bool from_origin, float carriage_amount,
                              float route_length, float expected_distance)
{
    CC_CHECK(CcRoadBookShowsRouteAmount(view, carriage_amount));
    float revealed_distance = from_origin ?
        (view->from_reveal - carriage_amount) * route_length :
        (view->to_reveal - (1.0f - carriage_amount)) * route_length;
    CC_CHECK(fabsf(revealed_distance - expected_distance) < 0.001f);
}

static void CheckCarriageSightForRouteLengths(void)
{
    const float short_route_length = 40.0f;
    const float long_route_length = 400.0f;
    const float departure_from_amount = 0.22f;
    const float departure_to_amount = 1.0f - departure_from_amount;
    const float progress_amount = 0.50f;

    CcSim forward;
    CcSimInit(&forward, UINT32_C(0x128f0));
    CcRoute *forward_route = &forward.routes[0];
    RemovePlayerChartForRoute(&forward, forward_route->id);
    PrepareJourneyView(&forward, forward_route, forward_route->from_id);

    CcRoadBookRouteView view = {0};
    CC_CHECK(CcRoadBookReadRouteAtCarriage(
        &forward, forward_route->id, departure_from_amount,
        long_route_length, &view));
    CC_CHECK(fabsf(view.from_reveal - 0.28f) < 0.0001f);
    CC_CHECK(CcRoadBookShowsRouteAmount(&view, departure_from_amount));

    CC_CHECK(CcRoadBookReadRouteAtCarriage(
        &forward, forward_route->id, departure_from_amount,
        short_route_length, &view));
    CheckForwardSight(&view, true, departure_from_amount,
                      short_route_length,
                      CC_ROAD_BOOK_FORWARD_SIGHT_DISTANCE);

    CC_CHECK(CcRoadBookReadRouteAtCarriage(
        &forward, forward_route->id, progress_amount,
        long_route_length, &view));
    CheckForwardSight(&view, true, progress_amount, long_route_length,
                      CC_ROAD_BOOK_FORWARD_SIGHT_DISTANCE);
    CC_CHECK(CcRoadBookReadRouteAtCarriage(
        &forward, forward_route->id, progress_amount,
        short_route_length, &view));
    CheckForwardSight(&view, true, progress_amount, short_route_length,
                      CC_ROAD_BOOK_FORWARD_SIGHT_DISTANCE);
    CC_CHECK(CcRoadBookReadRouteAtCarriage(
        &forward, forward_route->id, 0.95f, 100.0f, &view));
    CC_CHECK(fabsf(view.from_reveal - 1.0f) < 0.0001f);

    forward.player.route_knowledge[0].from_reveal_milli = 700;
    CC_CHECK(CcRoadBookReadRouteAtCarriage(
        &forward, forward_route->id, departure_from_amount,
        long_route_length, &view));
    CC_CHECK(fabsf(view.from_reveal - 0.70f) < 0.0001f);
    CC_CHECK(CcRoadBookReadRouteAtCarriage(
        &forward, forward_route->id, progress_amount,
        long_route_length, &view));
    CC_CHECK(fabsf(view.from_reveal - 0.70f) < 0.0001f);

    CcSim reverse;
    CcSimInit(&reverse, UINT32_C(0x128f0));
    CcRoute *reverse_route = &reverse.routes[0];
    RemovePlayerChartForRoute(&reverse, reverse_route->id);
    reverse.player.location_id = reverse_route->to_id;
    CcSimInitializePlayerRouteKnowledge(&reverse);
    PrepareJourneyView(&reverse, reverse_route, reverse_route->to_id);

    CC_CHECK(CcRoadBookReadRouteAtCarriage(
        &reverse, reverse_route->id, departure_to_amount,
        long_route_length, &view));
    CC_CHECK(fabsf(view.to_reveal - 0.28f) < 0.0001f);
    CC_CHECK(CcRoadBookShowsRouteAmount(&view, departure_to_amount));

    CC_CHECK(CcRoadBookReadRouteAtCarriage(
        &reverse, reverse_route->id, departure_to_amount,
        short_route_length, &view));
    CheckForwardSight(&view, false, departure_to_amount,
                      short_route_length,
                      CC_ROAD_BOOK_FORWARD_SIGHT_DISTANCE);

    CC_CHECK(CcRoadBookReadRouteAtCarriage(
        &reverse, reverse_route->id, progress_amount,
        long_route_length, &view));
    CheckForwardSight(&view, false, progress_amount, long_route_length,
                      CC_ROAD_BOOK_FORWARD_SIGHT_DISTANCE);
    CC_CHECK(CcRoadBookReadRouteAtCarriage(
        &reverse, reverse_route->id, progress_amount,
        short_route_length, &view));
    CheckForwardSight(&view, false, progress_amount, short_route_length,
                      CC_ROAD_BOOK_FORWARD_SIGHT_DISTANCE);
    CC_CHECK(CcRoadBookReadRouteAtCarriage(
        &reverse, reverse_route->id, 0.05f, 100.0f, &view));
    CC_CHECK(fabsf(view.to_reveal - 1.0f) < 0.0001f);

    reverse.player.route_knowledge[0].to_reveal_milli = 700;
    CC_CHECK(CcRoadBookReadRouteAtCarriage(
        &reverse, reverse_route->id, departure_to_amount,
        long_route_length, &view));
    CC_CHECK(fabsf(view.to_reveal - 0.70f) < 0.0001f);
}

static void BeginRealJourney(CcSim *sim, int32_t route_index, bool reverse,
                             char *error, size_t error_capacity)
{
    CcRoute *route = &sim->routes[route_index];
    CcId origin_id = reverse ? route->to_id : route->from_id;
    CcId destination_id = reverse ? route->from_id : route->to_id;
    sim->player.location_id = origin_id;
    sim->carriage.location_id = origin_id;
    RemovePlayerChartForRoute(sim, route->id);
    CcSimInitializePlayerRouteKnowledge(sim);
    route->closed = false;
    route->security = 100;
    route->condition = 100;
    CcCommand travel = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = destination_id
    };
    CC_CHECK(CcSimApply(sim, &travel, error, error_capacity));
    sim->journey.ambush_pending = false;
}

static float RenderedJourneyAmount(const CcWorldManifest *manifest,
                                   const CcWorldRoutePlacement *route,
                                   const CcSim *sim, float progress)
{
    const CcWorldSettlementPlacement *origin =
        CcWorldSettlementPlacementForId(manifest, sim->journey.origin_id);
    const CcWorldSettlementPlacement *destination =
        CcWorldSettlementPlacementForId(
            manifest, sim->journey.destination_id);
    float length = CcWorldRouteLength(route);
    CC_CHECK(origin != NULL);
    CC_CHECK(destination != NULL);
    CC_CHECK(length > 0.001f);
    float start_amount = fminf(0.22f, (origin->radius + 5.0f) / length);
    float end_amount = fminf(
        0.22f, (destination->radius + 5.0f) / length);
    return start_amount + progress *
        fmaxf(0.0f, 1.0f - start_amount - end_amount);
}

static void CheckRealJourneySight(bool reverse)
{
    CcSim sim;
    CcSimInit(&sim, UINT32_C(0xca771a9e));
    char error[256];
    BeginRealJourney(&sim, 4, reverse, error, sizeof(error));

    CcWorldManifest manifest;
    CC_CHECK(CcWorldManifestBuild(&manifest, &sim));
    const CcWorldRoutePlacement *placement =
        CcWorldRoutePlacementForId(&manifest, sim.journey.route_id);
    CC_CHECK(placement != NULL);
    float route_length = CcWorldRouteLength(placement);
    CC_CHECK(route_length > 300.0f);

    const CcRouteKnowledge *knowledge = CcSimPlayerRouteKnowledge(
        &sim, sim.journey.route_id);
    CC_CHECK(knowledge != NULL);
    int32_t departure_reveal = reverse ? knowledge->to_reveal_milli :
                                         knowledge->from_reveal_milli;
    CC_CHECK(departure_reveal == 280);
    float journey_amount = RenderedJourneyAmount(
        &manifest, placement, &sim, 0.0f);
    float junction_amount = reverse ?
        1.0f - CcWorldRouteSampleAmount(
            placement, CC_WORLD_ROUTE_TO_JUNCTION_SAMPLE) :
        CcWorldRouteSampleAmount(
            placement, CC_WORLD_ROUTE_FROM_JUNCTION_SAMPLE);
    CC_CHECK(journey_amount + 0.0001f >= junction_amount);
    float carriage_amount = reverse ? 1.0f - journey_amount :
                                      journey_amount;
    CcRoadBookRouteView view = {0};
    CC_CHECK(CcRoadBookReadRouteAtCarriage(
        &sim, sim.journey.route_id, carriage_amount, route_length, &view));
    CC_CHECK(CcRoadBookShowsRouteAmount(&view, carriage_amount));
    CC_CHECK(fabsf((reverse ? view.to_reveal : view.from_reveal) - 0.28f) <
             0.0001f);

    CcSimAdvanceRuntimeTicks(&sim, sim.journey.total_subticks / 60);
    CC_CHECK(sim.journey.active);
    CC_CHECK(sim.carriage.progress_milli == 500);
    knowledge = CcSimPlayerRouteKnowledge(&sim, sim.journey.route_id);
    CC_CHECK(knowledge != NULL);
    CC_CHECK((reverse ? knowledge->to_reveal_milli :
                        knowledge->from_reveal_milli) == 500);
    journey_amount = RenderedJourneyAmount(
        &manifest, placement, &sim, 0.50f);
    carriage_amount = reverse ? 1.0f - journey_amount : journey_amount;
    CC_CHECK(CcRoadBookReadRouteAtCarriage(
        &sim, sim.journey.route_id, carriage_amount, route_length, &view));
    CheckForwardSight(&view, !reverse, carriage_amount, route_length,
                      CC_ROAD_BOOK_FORWARD_SIGHT_DISTANCE);
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
    const CcRouteKnowledge *partial = CcSimPlayerRouteKnowledge(
        &sim, road->id);
    CC_CHECK(partial != NULL);
    CC_CHECK(partial->from_reveal_milli == 280);
    CC_CHECK(CcJournalAdvanceRuntimeTicks(
        journal, &sim, 2400, error, sizeof(error)));
    CC_CHECK(sim.journey.active);
    partial = CcSimPlayerRouteKnowledge(&sim, road->id);
    CC_CHECK(partial != NULL);
    int32_t expected_partial_reveal = sim.carriage.progress_milli > 280 ?
        sim.carriage.progress_milli : 280;
    CC_CHECK(partial->from_reveal_milli == expected_partial_reveal);
    CC_CHECK(partial->from_reveal_milli < 1000);
    CC_CHECK(partial->to_reveal_milli == 0);
    int32_t saved_partial_reveal = partial->from_reveal_milli;
    uint64_t saved_partial_hash = CcSimHash(&sim);
    CC_CHECK(CcJournalClose(&journal, &sim, error, sizeof(error)));

    CcSim restored;
    CcJournal *resumed_journal = CcJournalResume(
        path, &restored, error, sizeof(error));
    CC_CHECK(resumed_journal != NULL);
    CC_CHECK(CcSimHash(&restored) == saved_partial_hash);
    partial = CcSimPlayerRouteKnowledge(&restored, road->id);
    CC_CHECK(partial != NULL);
    CC_CHECK(partial->from_reveal_milli == saved_partial_reveal);
    CC_CHECK(partial->to_reveal_milli == 0);
    CC_CHECK(restored.journey.active);
    CC_CHECK(CcJournalClose(
        &resumed_journal, &restored, error, sizeof(error)));

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
    CcRoadBookRouteView next_road_view;
    CC_CHECK(CcRoadBookReadRoute(
        &restored, next_road->id, &next_road_view));
    CC_CHECK(!CcRoadBookShowsWholeRoute(&next_road_view));
    CC_CHECK(!CcRoadBookShowsRouteAmount(&next_road_view, 0.50f));
    if (next_road->from_id == destination_id) {
        CC_CHECK(CcRoadBookShowsRouteAmount(&next_road_view, 0.05f));
        CC_CHECK(!CcRoadBookShowsRouteAmount(&next_road_view, 0.95f));
    } else {
        CC_CHECK(!CcRoadBookShowsRouteAmount(&next_road_view, 0.05f));
        CC_CHECK(CcRoadBookShowsRouteAmount(&next_road_view, 0.95f));
    }

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

    CheckCarriageSightForRouteLengths();
    CheckRealJourneySight(false);
    CheckRealJourneySight(true);

    RemoveDatabase(path);
    puts("Persistent route knowledge tests passed");
    return 0;
}
