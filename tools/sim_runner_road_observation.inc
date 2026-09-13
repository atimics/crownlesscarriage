/* Run-local daily observations. A zeroed record begins an empty interval. */
typedef struct RoadObservation {
    int32_t open_days;
    int32_t current_closed_days;
    int32_t longest_closed_days;
    int32_t closed_inhabited_days;
} RoadObservation;

static void ObserveRoadDay(const CcSim *sim, const CcRoute *route,
                           RoadObservation *observation)
{
    if (!route->closed) {
        observation->open_days++;
        observation->current_closed_days = 0;
        return;
    }
    observation->current_closed_days++;
    if (observation->current_closed_days > observation->longest_closed_days)
        observation->longest_closed_days = observation->current_closed_days;
    const CcSettlement *from = CcSimSettlement(sim, route->from_id);
    const CcSettlement *to = CcSimSettlement(sim, route->to_id);
    if (from != NULL && to != NULL && from->population > 0 && to->population > 0)
        observation->closed_inhabited_days++;
}
