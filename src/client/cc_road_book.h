#ifndef CROWNLESS_ROAD_BOOK_H
#define CROWNLESS_ROAD_BOOK_H

#include "sim/cc_sim.h"

#include <stdbool.h>

#define CC_ROAD_BOOK_FORWARD_SIGHT_DISTANCE 18.0f

typedef struct CcRoadBookRouteView {
    float from_reveal;
    float to_reveal;
    bool charted;
} CcRoadBookRouteView;

typedef struct CcRoadBookRouteSpan {
    float from_amount;
    float to_amount;
} CcRoadBookRouteSpan;

bool CcRoadBookReadRoute(const CcSim *sim, CcId route_id,
                         CcRoadBookRouteView *view);
/* The carriage amount runs from the route's from end at 0 to its to end at 1.
   Route length and forward sight use world units. */
bool CcRoadBookReadRouteAtCarriage(const CcSim *sim, CcId route_id,
                                   float carriage_route_amount,
                                   float route_length,
                                   CcRoadBookRouteView *view);
bool CcRoadBookShowsRouteAmount(const CcRoadBookRouteView *view,
                                float amount);
bool CcRoadBookShowsWholeRoute(const CcRoadBookRouteView *view);
int32_t CcRoadBookVisibleRouteSpans(
    const CcRoadBookRouteView *view, float segment_from, float segment_to,
    CcRoadBookRouteSpan spans[2]);

#endif
