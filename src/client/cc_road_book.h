#ifndef CROWNLESS_ROAD_BOOK_H
#define CROWNLESS_ROAD_BOOK_H

#include "sim/cc_sim.h"

#include <stdbool.h>

typedef struct CcRoadBookRouteView {
    float from_reveal;
    float to_reveal;
    bool charted;
} CcRoadBookRouteView;

bool CcRoadBookReadRoute(const CcSim *sim, CcId route_id,
                         CcRoadBookRouteView *view);
bool CcRoadBookShowsRouteAmount(const CcRoadBookRouteView *view,
                                float amount);
bool CcRoadBookShowsWholeRoute(const CcRoadBookRouteView *view);

#endif
