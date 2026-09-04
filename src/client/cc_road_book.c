#include "client/cc_road_book.h"

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
