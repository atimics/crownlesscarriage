#include "sim/cc_sim_custody.h"
#include "sim/cc_archive_relocation.h"
#include "sim/cc_archive_internal.h"
#include "sim/cc_archive_volumes_internal.h"
#include "sim/cc_route_rules_internal.h"
#include "sim/cc_trade_path_internal.h"

static void SelectBooks(const CcSim *sim, CcArchiveRelocationPlan *plan)
{
    for (int i = 0; i < sim->treasure_count; ++i) {
        const CcTreasure *book = &sim->treasures[i];
        if (!CcArchiveVolumeIsLive(book) || book->owner_id != plan->origin_id ||
            book->location_id != plan->origin_id) continue;
        plan->eligible_books++;
        for (int j = 0; j < CC_ARCHIVE_MOVE_BOOK_CAPACITY; ++j) {
            if (plan->book_ids[j] != 0 && plan->book_ids[j] < book->id) continue;
            for (int k = CC_ARCHIVE_MOVE_BOOK_CAPACITY - 1; k > j; --k) plan->book_ids[k] = plan->book_ids[k - 1];
            plan->book_ids[j] = book->id; break;
        }
    }
    plan->book_count = plan->eligible_books < CC_ARCHIVE_MOVE_BOOK_CAPACITY ? plan->eligible_books : CC_ARCHIVE_MOVE_BOOK_CAPACITY;
}

static CcArchiveRelocationPlan QuoteDestination(const CcSim *sim,
    CcArchiveRelocationPlan plan, CcArchiveSeatCandidate candidate)
{
    const CcSettlement *origin = CcSimSettlement(sim, plan.origin_id);
    const CcSettlement *destination = CcSimSettlement(sim, candidate.settlement_id);
    plan.destination_id = destination->id; plan.destination_score = candidate.score;
    plan.rival_foundation = origin->kingdom_id != destination->kingdom_id;
    plan.funding_kingdom_id = destination->kingdom_id;
    plan.gate = CC_ARCHIVE_MOVE_SPONSOR;
    const CcKingdom *kingdom = NULL;
    for (int i = 0; i < sim->kingdom_count; ++i)
        if (sim->kingdoms[i].id == destination->kingdom_id) kingdom = &sim->kingdoms[i];
    if (kingdom == NULL) return plan;
    const CcCharacter *sponsor = CcSimCharacter(sim, candidate.patron_id != 0 ? candidate.patron_id : kingdom->ruler_character_id);
    if (sponsor == NULL || sponsor->death_day <= sim->current_day) return plan;
    plan.sponsor_id = sponsor->id;
    plan.gate = CC_ARCHIVE_MOVE_CARRIAGE;
    const CcRoyalCarriage *carriage = NULL;
    for (int i = 0; i < sim->royal_carriage_count; ++i) {
        const CcRoyalCarriage *item = &sim->royal_carriages[i];
        if (CcSimCustodyCarrierLoad(sim, item->id) > 0 || item->kingdom_id != kingdom->id || item->location_id != origin->id ||
            item->mode != CC_ROYAL_CARRIAGE_IDLE || item->active_shipment_id != 0 ||
            item->condition < 20 || item->next_dispatch_day > sim->current_day) continue;
        if (carriage == NULL || item->id < carriage->id) carriage = item;
    }
    if (carriage == NULL) return plan;
    plan.carriage_id = carriage->id;
    plan.gate = CC_ARCHIVE_MOVE_ROUTE;
    int slot = -1, capacity = 0;
    const int32_t *used = sim->royal_trade_week == sim->current_day / 7 ? sim->royal_route_slots_used : NULL;
    if (!CcTradeFindPath(sim, origin->id, destination->id, CC_GOOD_PAPER, &slot,
        &plan.first_hop_id, NULL, &capacity, used, true, carriage->kingdom_id, true, 1)) return plan;
    if (plan.book_count > capacity) plan.book_count = capacity;
    const CcRoute *route = &sim->routes[slot];
    plan.first_route_id = route->id;
    plan.first_leg_days = CcSimFreightLegDays(sim, route->id, origin->id, plan.first_hop_id);
    plan.first_leg_wheat = 2 * ((plan.first_leg_days + 6) / 7);
    plan.first_leg_toll = CcRouteRoyalTradeToll(sim, route, carriage->kingdom_id);
    plan.route_danger = CcSimRouteDanger(sim, route->id);
    plan.gate = CC_ARCHIVE_MOVE_FOOD;
    if (CcArchiveSpareGrain(sim, origin) < plan.first_leg_wheat) return plan;
    plan.gate = CC_ARCHIVE_MOVE_FUNDS;
    if (kingdom->treasury < plan.first_leg_toll) return plan;
    plan.gate = CC_ARCHIVE_MOVE_READY;
    return plan;
}

CcArchiveRelocationPlan CcSimArchiveRelocationPlan(const CcSim *sim)
{
    CcArchiveRelocationPlan plan = {.gate = CC_ARCHIVE_MOVE_SEAT};
    if (sim == NULL || sim->schema_version < 91U || sim->archives.seat_id == 0) return plan;
    if (sim->schema_version >= 92U && sim->archive_convoy.status != 0) {
        plan.gate = CC_ARCHIVE_MOVE_BUSY; return plan;
    }
    plan.origin_id = sim->archives.seat_id;
    if (CcSimSettlement(sim, plan.origin_id) == NULL) return plan;
    plan.gate = CC_ARCHIVE_MOVE_HEALTHY;
    if (CcSimArchiveSeatCandidate(sim, plan.origin_id).viable) return plan;
    plan.gate = CC_ARCHIVE_MOVE_WAIT;
    if (sim->archives.seat_failed_since_day <= 0 ||
        (int64_t)sim->current_day - sim->archives.seat_failed_since_day < CC_ARCHIVE_SEAT_FAILURE_DAYS) return plan;
    plan.gate = CC_ARCHIVE_MOVE_RECRUITMENT;
    if (sim->archive_recruitment.status != 0) return plan;
    plan.gate = CC_ARCHIVE_MOVE_BOOKS;
    SelectBooks(sim, &plan);
    if (plan.book_count == 0) return plan;
    plan.gate = CC_ARCHIVE_MOVE_DESTINATION;
    CcArchiveRelocationPlan best = plan;
    for (int i = 0; i < sim->settlement_count; ++i) {
        CcArchiveSeatCandidate candidate = CcSimArchiveSeatCandidate(sim, sim->settlements[i].id);
        if (!candidate.viable || candidate.settlement_id == plan.origin_id) continue;
        CcArchiveRelocationPlan next = QuoteDestination(sim, plan, candidate);
        bool ready = next.gate == CC_ARCHIVE_MOVE_READY, best_ready = best.gate == CC_ARCHIVE_MOVE_READY;
        if (best.destination_id == 0 || (ready && !best_ready) || (ready == best_ready &&
            (next.destination_score > best.destination_score ||
             (next.destination_score == best.destination_score && next.destination_id < best.destination_id)))) best = next;
    }
    return best;
}

const char *CcArchiveRelocationGateName(CcArchiveRelocationGate gate)
{
    switch (gate) {
        case CC_ARCHIVE_MOVE_BUSY: return "convoy_order";
        case CC_ARCHIVE_MOVE_READY: return "ready";
        case CC_ARCHIVE_MOVE_SEAT: return "saved_seat";
        case CC_ARCHIVE_MOVE_HEALTHY: return "healthy_seat";
        case CC_ARCHIVE_MOVE_WAIT: return "failure_period";
        case CC_ARCHIVE_MOVE_RECRUITMENT: return "recruitment_order";
        case CC_ARCHIVE_MOVE_DESTINATION: return "viable_destination";
        case CC_ARCHIVE_MOVE_BOOKS: return "local_books";
        case CC_ARCHIVE_MOVE_SPONSOR: return "living_sponsor";
        case CC_ARCHIVE_MOVE_CARRIAGE: return "carriage_at_origin";
        case CC_ARCHIVE_MOVE_ROUTE: return "route_capacity";
        case CC_ARCHIVE_MOVE_FOOD: return "crew_wheat";
        case CC_ARCHIVE_MOVE_FUNDS: return "sponsor_funds";
    }
    return "unknown";
}
