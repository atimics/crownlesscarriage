#include "client/cc_road_book.h"

#include <float.h>

static float ClampUnit(float value)
{
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

static float Maximum(float first, float second)
{
    return first > second ? first : second;
}

static float Minimum(float first, float second)
{
    return first < second ? first : second;
}

static void ReadRecordedRouteFacts(const CcSim *sim, CcId route_id,
                                   CcRoadBookRouteView *view)
{
    const CcRouteKnowledge *knowledge =
        CcSimPlayerRouteKnowledge(sim, route_id);
    if (knowledge != NULL &&
        knowledge->source != CC_PLAYER_KNOWLEDGE_NONE) {
        view->learned_day = knowledge->learned_day;
        view->recorded_condition = knowledge->recorded_condition;
        view->recorded_danger = knowledge->recorded_danger;
        view->source = knowledge->source;
        view->recorded_closed = knowledge->recorded_closed;
        view->recorded_smuggler_route =
            knowledge->recorded_smuggler_route;
    }
    for (int32_t i = 0; sim != NULL && i < sim->map_count; ++i) {
        const CcMap *map = &sim->maps[i];
        if (map->route_id != route_id ||
            map->owner_id != sim->player.id ||
            !CcSimMapIsCatalogued(sim, map) ||
            (view->source != CC_PLAYER_KNOWLEDGE_NONE &&
             map->surveyed_day <= view->learned_day)) continue;
        view->learned_day = map->surveyed_day;
        view->recorded_condition = map->recorded_condition;
        view->recorded_danger = map->recorded_danger;
        view->source = CC_PLAYER_KNOWLEDGE_CHART;
        view->recorded_closed = map->recorded_closed;
        view->recorded_smuggler_route = map->recorded_smuggler_route;
    }
}

bool CcRoadBookReadRoute(const CcSim *sim, CcId route_id,
                         CcRoadBookRouteView *view)
{
    if (view == NULL) return false;
    *view = (CcRoadBookRouteView){0};
    int32_t from_reveal_milli = 0;
    int32_t to_reveal_milli = 0;
    bool visible = CcSimPlayerRouteReveal(
        sim, route_id, &from_reveal_milli, &to_reveal_milli,
        &view->charted);
    view->from_reveal = (float)from_reveal_milli / 1000.0f;
    view->to_reveal = (float)to_reveal_milli / 1000.0f;
    ReadRecordedRouteFacts(sim, route_id, view);
    return visible;
}

CcId CcRoadBookSettlementKingdom(const CcSim *sim, CcId settlement_id)
{
    const CcSettlementKnowledge *knowledge =
        CcSimPlayerSettlementKnowledge(sim, settlement_id);
    return knowledge != NULL ? knowledge->kingdom_id : 0U;
}

bool CcRoadBookReadRouteAtCarriage(const CcSim *sim, CcId route_id,
                                   float carriage_route_amount,
                                   float route_length,
                                   CcRoadBookRouteView *view)
{
    bool visible = CcRoadBookReadRoute(sim, route_id, view);
    const CcRoute *route = CcSimRoute(sim, route_id);
    if (route == NULL || view == NULL || sim == NULL ||
        !sim->journey.active || sim->journey.route_id != route_id) {
        return visible;
    }
    if (!(carriage_route_amount >= -FLT_MAX &&
          carriage_route_amount <= FLT_MAX) ||
        !(route_length > 0.001f && route_length <= FLT_MAX)) {
        return visible;
    }
    float carriage_amount = ClampUnit(carriage_route_amount);
    float sight_amount = ClampUnit(
        CC_ROAD_BOOK_FORWARD_SIGHT_DISTANCE / route_length);
    if (sim->journey.origin_id == route->from_id) {
        view->from_reveal = Maximum(
            view->from_reveal, ClampUnit(carriage_amount + sight_amount));
    } else if (sim->journey.origin_id == route->to_id) {
        view->to_reveal = Maximum(
            view->to_reveal,
            ClampUnit(1.0f - carriage_amount + sight_amount));
    }
    return visible || view->from_reveal > 0.0f || view->to_reveal > 0.0f;
}

bool CcRoadBookShowsRouteAmount(const CcRoadBookRouteView *view,
                                float amount)
{
    if (view == NULL) return false;
    return (view->from_reveal > 0.0f &&
            amount <= view->from_reveal) ||
           (view->to_reveal > 0.0f &&
            amount >= 1.0f - view->to_reveal);
}

bool CcRoadBookShowsWholeRoute(const CcRoadBookRouteView *view)
{
    if (view == NULL) return false;
    return view->from_reveal >= 1.0f || view->to_reveal >= 1.0f ||
           view->from_reveal + view->to_reveal >= 1.0f;
}

int32_t CcRoadBookVisibleRouteSpans(
    const CcRoadBookRouteView *view, float segment_from, float segment_to,
    CcRoadBookRouteSpan spans[2])
{
    if (view == NULL || spans == NULL ||
        !(segment_from >= -FLT_MAX && segment_from <= FLT_MAX) ||
        !(segment_to >= -FLT_MAX && segment_to <= FLT_MAX)) {
        return 0;
    }
    float start = ClampUnit(Minimum(segment_from, segment_to));
    float end = ClampUnit(Maximum(segment_from, segment_to));
    if (end <= start) return 0;

    int32_t count = 0;
    float from_end = ClampUnit(view->from_reveal);
    float first_end = Minimum(end, from_end);
    if (first_end > start) {
        spans[count++] = (CcRoadBookRouteSpan){start, first_end};
    }

    float to_start = ClampUnit(1.0f - view->to_reveal);
    float second_start = Maximum(start, to_start);
    if (end > second_start) {
        if (count > 0 && second_start <= spans[0].to_amount) {
            spans[0].to_amount = end;
        } else {
            spans[count++] = (CcRoadBookRouteSpan){second_start, end};
        }
    }
    return count;
}
