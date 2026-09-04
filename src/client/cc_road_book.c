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
    return visible;
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
