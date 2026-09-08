#include "sim/cc_trade_path_internal.h"
#include "sim/cc_route_rules_internal.h"

#include <limits.h>

static int32_t MaximumI32(int32_t a, int32_t b) { return a > b ? a : b; }
static int32_t MinimumI32(int32_t a, int32_t b) { return a < b ? a : b; }

static int32_t SettlementSlotById(const CcSim *sim, CcId id)
{
    for (int32_t i = 0; i < sim->settlement_count; ++i)
        if (sim->settlements[i].id == id) return i;
    return -1;
}

int32_t CcTradeRouteCapacity(const CcSim *sim, const CcRoute *route)
{
    if (sim == NULL || route == NULL) return 0;
    if (sim->schema_version >= 73U && route->condition == 0) return 0;
    int32_t capacity = MaximumI32(
        3, route->capacity * MaximumI32(25, route->condition) / 100);
    if (route->closed) capacity = MaximumI32(1, capacity / 2);
    if (CcSimRouteCrossesWarBorder(sim, route->id) &&
        !route->smuggler_route) {
        capacity = MaximumI32(1, capacity / 2);
    }
    return capacity;
}

bool CcTradeFindPath(const CcSim *sim, CcId from_id, CcId to_id,
                          CcGood good,
                          int32_t *first_route_slot, CcId *first_hop_id,
                          int32_t *total_cost, int32_t *path_capacity,
                          const int32_t route_used[CC_MAX_ROUTES],
                          bool allow_kingdom_borders,
                          CcId carriage_kingdom_id,
                          bool ignore_royal_relations,
                          int32_t required_slots)
{
    int32_t source = SettlementSlotById(sim, from_id);
    int32_t target = SettlementSlotById(sim, to_id);
    if (source < 0 || target < 0 || source == target ||
        CcSettlementIsAbandoned(&sim->settlements[source]) ||
        CcSettlementIsAbandoned(&sim->settlements[target])) return false;
    (void)good;
    int32_t distance[CC_MAX_SETTLEMENTS];
    int32_t bottleneck[CC_MAX_SETTLEMENTS];
    int32_t first_route[CC_MAX_SETTLEMENTS];
    CcId first_hop[CC_MAX_SETTLEMENTS];
    bool visited[CC_MAX_SETTLEMENTS];
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        distance[i] = INT_MAX;
        bottleneck[i] = 0;
        first_route[i] = -1;
        first_hop[i] = 0U;
        visited[i] = false;
    }
    distance[source] = 0;
    bottleneck[source] = INT_MAX;
    for (int32_t iteration = 0; iteration < sim->settlement_count; ++iteration) {
        int32_t current = -1;
        for (int32_t i = 0; i < sim->settlement_count; ++i) {
            if (!visited[i] && distance[i] < INT_MAX &&
                (current < 0 || distance[i] < distance[current])) current = i;
        }
        if (current < 0) break;
        if (current == target) break;
        visited[current] = true;
        CcId current_id = sim->settlements[current].id;
        for (int32_t route_slot = 0; route_slot < sim->route_count; ++route_slot) {
            const CcRoute *route = &sim->routes[route_slot];
            if (!allow_kingdom_borders &&
                CcSimRouteCrossesKingdomBorder(sim, route->id)) continue;
            if (carriage_kingdom_id != 0U) {
                if (!(ignore_royal_relations ?
                      CcRouteRoyalCanReopen(sim, route) :
                      CcRouteRoyalIsOfficial(sim, route)) ||
                    (!ignore_royal_relations &&
                     !CcSimRoyalCarriageCanUseRoute(
                         sim, carriage_kingdom_id, route->id))) continue;
            }
            bool war_border = CcSimRouteCrossesWarBorder(sim, route->id) &&
                              !route->smuggler_route;
            int32_t effective_capacity = CcTradeRouteCapacity(sim, route);
            int32_t available_capacity = effective_capacity -
                (route_used != NULL ? route_used[route_slot] : 0);
            if (available_capacity < MaximumI32(1, required_slots)) continue;
            CcId neighbor_id = route->from_id == current_id ? route->to_id :
                               route->to_id == current_id ? route->from_id : 0U;
            int32_t neighbor = SettlementSlotById(sim, neighbor_id);
            if (neighbor < 0 || visited[neighbor] ||
                CcSettlementIsAbandoned(&sim->settlements[neighbor])) continue;
            int32_t edge_cost = route->travel_days * 10 +
                                CcSimRouteDanger(sim, route->id) +
                                (war_border ? 25 : 0);
            if (distance[current] > INT_MAX - edge_cost) continue;
            int32_t candidate = distance[current] + edge_cost;
            int32_t candidate_bottleneck = MinimumI32(
                bottleneck[current], available_capacity);
            if (candidate > distance[neighbor] ||
                (candidate == distance[neighbor] &&
                 candidate_bottleneck <= bottleneck[neighbor])) continue;
            distance[neighbor] = candidate;
            bottleneck[neighbor] = candidate_bottleneck;
            first_route[neighbor] = current == source ? route_slot : first_route[current];
            first_hop[neighbor] = current == source ? neighbor_id : first_hop[current];
        }
    }
    if (distance[target] == INT_MAX || first_route[target] < 0 || first_hop[target] == 0U) {
        return false;
    }
    if (first_route_slot != NULL) *first_route_slot = first_route[target];
    if (first_hop_id != NULL) *first_hop_id = first_hop[target];
    if (total_cost != NULL) *total_cost = distance[target];
    if (path_capacity != NULL) *path_capacity = bottleneck[target];
    return true;
}

