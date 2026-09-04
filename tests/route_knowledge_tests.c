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

static void CheckRouteSpanClipping(void)
{
    CcRoadBookRouteSpan spans[2];
    CcRoadBookRouteView from_view = {.from_reveal = 0.425f};
    CC_CHECK(CcRoadBookVisibleRouteSpans(
        &from_view, 0.40f, 0.50f, spans) == 1);
    CC_CHECK(fabsf(spans[0].from_amount - 0.40f) < 0.0001f);
    CC_CHECK(fabsf(spans[0].to_amount - 0.425f) < 0.0001f);

    CcRoadBookRouteView to_view = {.to_reveal = 0.575f};
    CC_CHECK(CcRoadBookVisibleRouteSpans(
        &to_view, 0.40f, 0.50f, spans) == 1);
    CC_CHECK(fabsf(spans[0].from_amount - 0.425f) < 0.0001f);
    CC_CHECK(fabsf(spans[0].to_amount - 0.50f) < 0.0001f);

    CcRoadBookRouteView split_view = {
        .from_reveal = 0.42f,
        .to_reveal = 0.55f,
    };
    CC_CHECK(CcRoadBookVisibleRouteSpans(
        &split_view, 0.40f, 0.50f, spans) == 2);
    CC_CHECK(fabsf(spans[0].from_amount - 0.40f) < 0.0001f);
    CC_CHECK(fabsf(spans[0].to_amount - 0.42f) < 0.0001f);
    CC_CHECK(fabsf(spans[1].from_amount - 0.45f) < 0.0001f);
    CC_CHECK(fabsf(spans[1].to_amount - 0.50f) < 0.0001f);
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
    float journey_amount = CcWorldRouteJourneyAmount(
        placement, sim.journey.origin_id, 0.0f);
    float junction_amount = reverse ?
        1.0f - CcWorldRouteSampleAmount(
            placement, CC_WORLD_ROUTE_TO_JUNCTION_SAMPLE) :
        CcWorldRouteSampleAmount(
            placement, CC_WORLD_ROUTE_FROM_JUNCTION_SAMPLE);
    CC_CHECK(fabsf(journey_amount - junction_amount) < 0.0001f);
    float carriage_amount = reverse ? 1.0f - journey_amount :
                                      journey_amount;
    CcRoadBookRouteView view = {0};
    CC_CHECK(CcRoadBookReadRouteAtCarriage(
        &sim, sim.journey.route_id, carriage_amount, route_length, &view));
    CC_CHECK(CcRoadBookShowsRouteAmount(&view, carriage_amount));
    CC_CHECK(fabsf((reverse ? view.to_reveal : view.from_reveal) - 0.28f) <
             0.0001f);

    float from_junction = CcWorldRouteSampleAmount(
        placement, CC_WORLD_ROUTE_FROM_JUNCTION_SAMPLE);
    float to_junction = CcWorldRouteSampleAmount(
        placement, CC_WORLD_ROUTE_TO_JUNCTION_SAMPLE);
    float near_arrival_amount = reverse ?
        from_junction + 5.0f / route_length :
        to_junction - 5.0f / route_length;
    CcRoadBookRouteView near_arrival = {0};
    CC_CHECK(CcRoadBookReadRouteAtCarriage(
        &sim, sim.journey.route_id, near_arrival_amount,
        route_length, &near_arrival));
    CcRoadBookRouteSpan connector_spans[2];
    int32_t connector_count = reverse ?
        CcRoadBookVisibleRouteSpans(
            &near_arrival, 0.0f, from_junction, connector_spans) :
        CcRoadBookVisibleRouteSpans(
            &near_arrival, to_junction, 1.0f, connector_spans);
    CC_CHECK(connector_count == 1);
    if (reverse) {
        CC_CHECK(connector_spans[0].from_amount < from_junction);
        CC_CHECK(fabsf(connector_spans[0].to_amount - from_junction) <
                 0.0001f);
    } else {
        CC_CHECK(fabsf(connector_spans[0].from_amount - to_junction) <
                 0.0001f);
        CC_CHECK(connector_spans[0].to_amount > to_junction);
    }

    int32_t progress_ticks =
        (sim.journey.total_subticks * 35 / 100) / 30;
    CcSimAdvanceRuntimeTicks(&sim, progress_ticks);
    CC_CHECK(sim.journey.active);
    CC_CHECK(sim.carriage.progress_milli == 350);
    knowledge = CcSimPlayerRouteKnowledge(&sim, sim.journey.route_id);
    CC_CHECK(knowledge != NULL);
    CC_CHECK((reverse ? knowledge->to_reveal_milli :
                        knowledge->from_reveal_milli) == 350);
    journey_amount = CcWorldRouteJourneyAmount(
        placement, sim.journey.origin_id, 0.35f);
    carriage_amount = reverse ? 1.0f - journey_amount : journey_amount;
    CC_CHECK(CcRoadBookReadRouteAtCarriage(
        &sim, sim.journey.route_id, carriage_amount, route_length, &view));
    CheckForwardSight(&view, !reverse, carriage_amount, route_length,
                      CC_ROAD_BOOK_FORWARD_SIGHT_DISTANCE);

    const char *path = reverse ? "route-knowledge-reverse-test.ccsave" :
                                 "route-knowledge-forward-test.ccsave";
    RemoveDatabase(path);
    uint64_t saved_hash = CcSimHash(&sim);
    CC_CHECK(CcSaveWrite(path, &sim, error, sizeof(error)));
    CcSim restored;
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(&restored) == saved_hash);
    knowledge = CcSimPlayerRouteKnowledge(
        &restored, restored.journey.route_id);
    CC_CHECK(knowledge != NULL);
    CC_CHECK((reverse ? knowledge->to_reveal_milli :
                        knowledge->from_reveal_milli) == 350);
    CC_CHECK(CcRoadBookReadRouteAtCarriage(
        &restored, restored.journey.route_id,
        carriage_amount, route_length, &view));
    CheckForwardSight(&view, !reverse, carriage_amount, route_length,
                      CC_ROAD_BOOK_FORWARD_SIGHT_DISTANCE);
    RemoveDatabase(path);
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
    int32_t midpoint_ticks = sim.journey.total_subticks / 60;
    while (midpoint_ticks > 0) {
        int32_t batch = midpoint_ticks > 3600 ? 3600 : midpoint_ticks;
        CC_CHECK(CcJournalAdvanceRuntimeTicks(
            journal, &sim, batch, error, sizeof(error)));
        midpoint_ticks -= batch;
    }
    CC_CHECK(sim.journey.active);
    CC_CHECK(sim.carriage.progress_milli == 500);
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

    CheckRouteSpanClipping();
    CheckCarriageSightForRouteLengths();
    CheckRealJourneySight(false);
    CheckRealJourneySight(true);

    RemoveDatabase(path);
    puts("Persistent route knowledge tests passed");
    return 0;
}
