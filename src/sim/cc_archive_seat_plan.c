#include "sim/cc_archive_internal.h"
#include "sim/cc_route_rules_internal.h"
#include "sim/cc_trade_path_internal.h"

static int32_t Min(int32_t a, int32_t b) { return a < b ? a : b; }

CcArchiveSeatCandidate CcSimArchiveSeatCandidate(const CcSim *sim, CcId settlement_id)
{
    CcArchiveSeatCandidate candidate = {0};
    const CcSettlement *town = CcSimSettlement(sim, settlement_id);
    if (town == NULL) return candidate;
    candidate.settlement_id = town->id;
    candidate.inhabited = !CcSettlementIsAbandoned(town);
    candidate.mill = CcSettlementHasService(town, CC_SERVICE_MILL);
    candidate.paper = town->stock[CC_GOOD_PAPER];
    candidate.tools = town->stock[CC_GOOD_TOOLS];
    candidate.spare_wheat = CcArchiveSpareGrain(sim, town);
    candidate.security = town->security;
    bool connected[CC_MAX_SETTLEMENTS] = {false};
    for (int i = 0; i < sim->route_count; ++i) {
        const CcRoute *road = &sim->routes[i];
        if (!CcRouteRoyalCanReopen(sim, road) || CcTradeRouteCapacity(sim, road) <= 0) continue;
        CcId neighbor_id = road->from_id == town->id ? road->to_id : road->to_id == town->id ? road->from_id : 0U;
        const CcSettlement *neighbor = CcSimSettlement(sim, neighbor_id);
        if (neighbor == NULL || CcSettlementIsAbandoned(neighbor) || neighbor->id == town->id) continue;
        int slot = (int)(neighbor - sim->settlements);
        if (!connected[slot]) { connected[slot] = true; candidate.usable_connections++; }
    }
    for (int i = 0; i < sim->kingdom_count; ++i) {
        const CcKingdom *kingdom = &sim->kingdoms[i];
        if (kingdom->id != town->kingdom_id) continue;
        const CcCharacter *patron = CcSimCharacter(sim, kingdom->monastery_patron_id);
        const CcSettlement *home = patron != NULL ? CcSimSettlement(sim, patron->home_settlement_id) : NULL;
        if (patron != NULL && patron->death_day > sim->current_day && home != NULL &&
            home->kingdom_id == kingdom->id) candidate.patron_id = patron->id;
    }
    candidate.viable = candidate.inhabited && candidate.mill && candidate.paper > 0 &&
        candidate.tools > 0 && candidate.spare_wheat >= 2;
    candidate.score = Min(candidate.paper, 16) * 4 + Min(candidate.spare_wheat / 2, 26) * 2 +
        Min(candidate.tools, 4) * 8 + Min(candidate.usable_connections, 4) * 6 +
        candidate.security / 5 + (candidate.patron_id != 0 ? 20 : 0);
    return candidate;
}

CcArchiveSeatPlan CcSimArchiveSeatPlan(const CcSim *sim, CcId current_seat_id)
{
    CcArchiveSeatPlan plan = {.current_id = current_seat_id};
    if (sim == NULL) return plan;
    CcArchiveSeatCandidate current = CcSimArchiveSeatCandidate(sim, current_seat_id);
    if (current.viable) {
        plan.selected_id = current_seat_id;
        plan.score = current.score;
        plan.keep_current = true;
        return plan;
    }
    for (int i = 0; i < sim->settlement_count; ++i) {
        CcArchiveSeatCandidate candidate = CcSimArchiveSeatCandidate(sim, sim->settlements[i].id);
        if (!candidate.viable) continue;
        if (plan.selected_id != 0 && (candidate.score < plan.score ||
            (candidate.score == plan.score && candidate.settlement_id >= plan.selected_id))) continue;
        plan.selected_id = candidate.settlement_id;
        plan.score = candidate.score;
    }
    return plan;
}
