typedef struct RouteObservation {
    uint64_t sampled_days;
    uint64_t closed_days;
    uint64_t war_border_days;
    uint64_t unavailable_inhabited_days;
    uint64_t unavailable_ruin_days;
    uint64_t current_outage_days;
    uint64_t longest_outage_days;
} RouteObservation;

static void ObserveRoutes(const CcSim *sim, RouteObservation observations[CC_MAX_ROUTES])
{
    for (int32_t i = 0; i < sim->route_count; ++i) {
        const CcRoute *route = &sim->routes[i];
        RouteObservation *row = &observations[i];
        const CcSettlement *from = CcSimSettlement(sim, route->from_id);
        const CcSettlement *to = CcSimSettlement(sim, route->to_id);
        bool war = CcSimRouteCrossesWarBorder(sim, route->id);
        bool unavailable = route->closed || (war && !route->smuggler_route);
        bool inhabited = from != NULL && to != NULL &&
            !CcSettlementIsAbandoned(from) && !CcSettlementIsAbandoned(to);
        row->sampled_days += 1;
        row->closed_days += route->closed ? 1U : 0U;
        row->war_border_days += war ? 1U : 0U;
        if (unavailable) {
            if (inhabited) row->unavailable_inhabited_days += 1;
            else row->unavailable_ruin_days += 1;
            row->current_outage_days += 1;
            if (row->current_outage_days > row->longest_outage_days)
                row->longest_outage_days = row->current_outage_days;
        } else {
            row->current_outage_days = 0;
        }
    }
}

static void WriteRouteHeader(FILE *output)
{
    (void)fputs("seed_number,world_seed,schema_version,generator_version,year,day,"
        "route_id,from_id,to_id,from_kingdom_id,to_kingdom_id,from_population,to_population,"
        "condition,closed,war_border,smuggler_route,interval_days,closed_days,war_border_days,"
        "unavailable_inhabited_days,unavailable_ruin_days,cumulative_sampled_days,"
        "cumulative_closed_days,cumulative_war_border_days,cumulative_unavailable_inhabited_days,"
        "cumulative_unavailable_ruin_days,current_outage_days,longest_outage_days\n", output);
}

static void WriteRouteYear(FILE *output, const CcSim *sim, int32_t seed, int32_t year,
    const RouteObservation observations[CC_MAX_ROUTES],
    const RouteObservation previous[CC_MAX_ROUTES])
{
    for (int32_t i = 0; i < sim->route_count; ++i) {
        const CcRoute *route = &sim->routes[i];
        const CcSettlement *from = CcSimSettlement(sim, route->from_id);
        const CcSettlement *to = CcSimSettlement(sim, route->to_id);
        const RouteObservation *row = &observations[i], *old = &previous[i];
        (void)fprintf(output,
            "%d,%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%d,%d,%" PRIu64 ",%" PRIu64
            ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%d,%d,%d,%d,%d,%d",
            seed, sim->world_seed, sim->schema_version, sim->generator_version, year,
            sim->current_day, route->id, route->from_id, route->to_id,
            from != NULL ? from->kingdom_id : 0U, to != NULL ? to->kingdom_id : 0U,
            from != NULL ? from->population : -1, to != NULL ? to->population : -1,
            route->condition, route->closed ? 1 : 0,
            CcSimRouteCrossesWarBorder(sim, route->id) ? 1 : 0, route->smuggler_route ? 1 : 0);
        (void)fprintf(output,
            ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64
            ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64
            ",%" PRIu64 ",%" PRIu64 "\n",
            row->sampled_days - old->sampled_days, row->closed_days - old->closed_days,
            row->war_border_days - old->war_border_days,
            row->unavailable_inhabited_days - old->unavailable_inhabited_days,
            row->unavailable_ruin_days - old->unavailable_ruin_days,
            row->sampled_days, row->closed_days, row->war_border_days,
            row->unavailable_inhabited_days, row->unavailable_ruin_days,
            row->current_outage_days, row->longest_outage_days);
    }
}
