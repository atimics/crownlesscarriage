#include "sim/cc_archive_relocation.h"
#include "sim/cc_identity_internal.h"
#include "sim/cc_food_economy_internal.h"

static CcKingdom *Funding(CcSim *sim, CcId id)
{
    for (int i = 0; i < sim->kingdom_count; ++i) if (sim->kingdoms[i].id == id) return &sim->kingdoms[i];
    return NULL;
}
static CcRoyalCarriage *Carriage(CcSim *sim, CcId id)
{
    for (int i = 0; i < sim->royal_carriage_count; ++i) if (sim->royal_carriages[i].id == id) return &sim->royal_carriages[i];
    return NULL;
}
static bool Issued(const CcSim *sim, CcId id, CcEntityKind kind)
{
    return CcIdKind(id) == kind && (id & CC_ID_SERIAL_MASK) > 0 &&
        (id & CC_ID_SERIAL_MASK) < sim->next_entity_serial;
}
bool CcSimArchiveConvoyValid(const CcSim *sim)
{
    if (sim == NULL) return false;
    if (sim->schema_version < 89U) return true;
    const CcArchiveConvoyOrder *o = &sim->archive_convoy;
    if (o->status == 0) {
        for (int i = 0; i < 4; ++i) if (o->book_ids[i] != 0) return false;
        return o->origin_id == 0 && o->destination_id == 0 && o->sponsor_id == 0 &&
            o->funding_kingdom_id == 0 && o->carriage_id == 0 && o->first_route_id == 0 &&
            o->first_hop_id == 0 && o->purse == 0 && o->wheat == 0 && o->book_count == 0 && o->reserved_day == 0;
    }
    if (o->status != 1 || o->book_count < 1 || o->book_count > 4 ||
        o->wheat < 1 || o->wheat > CC_SIM_MAX_UNITS || o->purse < 0 || o->purse > CC_SIM_MAX_MONEY ||
        o->reserved_day < 1 || o->reserved_day > sim->current_day ||
        o->origin_id == o->destination_id || CcSimSettlement(sim, o->origin_id) == NULL ||
        CcSimSettlement(sim, o->destination_id) == NULL ||
        !Issued(sim, o->sponsor_id, CC_ENTITY_CHARACTER) || sim->archive_recruitment.status != 0) return false;
    bool funding = false, carriage = false;
    for (int i = 0; i < sim->kingdom_count; ++i) funding |= sim->kingdoms[i].id == o->funding_kingdom_id;
    for (int i = 0; i < sim->royal_carriage_count; ++i) {
        const CcRoyalCarriage *item = &sim->royal_carriages[i];
        if (item->id == o->carriage_id) carriage = item->mode == CC_ROYAL_CARRIAGE_ARCHIVE_RESERVED &&
            item->kingdom_id == o->funding_kingdom_id && item->location_id == o->origin_id;
    }
    const CcRoute *route = CcSimRoute(sim, o->first_route_id);
    if (!funding || !carriage || route == NULL ||
        !((route->from_id == o->origin_id && route->to_id == o->first_hop_id) ||
          (route->to_id == o->origin_id && route->from_id == o->first_hop_id))) return false;
    for (int i = 0; i < 4; ++i) {
        if (i >= o->book_count) { if (o->book_ids[i] != 0) return false; continue; }
        if (!Issued(sim, o->book_ids[i], CC_ENTITY_TREASURE)) return false;
        for (int j = 0; j < i; ++j) if (o->book_ids[j] == o->book_ids[i]) return false;
    }
    return true;
}

bool CcSimReserveArchiveConvoy(CcSim *sim)
{
    if (sim == NULL || sim->schema_version < 89U || !CcSimArchiveConvoyValid(sim)) return false;
    CcArchiveRelocationPlan plan = CcSimArchiveRelocationPlan(sim);
    if (plan.gate != CC_ARCHIVE_MOVE_READY) return false;
    CcKingdom *kingdom = Funding(sim, plan.funding_kingdom_id);
    CcRoyalCarriage *carriage = Carriage(sim, plan.carriage_id);
    CcSettlement *origin = CcSimSettlementMutable(sim, plan.origin_id);
    if (kingdom == NULL || carriage == NULL || origin == NULL) return false;
    CcArchiveConvoyOrder order = {.origin_id = plan.origin_id, .destination_id = plan.destination_id,
        .sponsor_id = plan.sponsor_id, .funding_kingdom_id = plan.funding_kingdom_id,
        .carriage_id = plan.carriage_id, .first_route_id = plan.first_route_id, .first_hop_id = plan.first_hop_id,
        .purse = plan.first_leg_toll, .wheat = plan.first_leg_wheat, .book_count = plan.book_count,
        .reserved_day = sim->current_day, .status = 1};
    for (int i = 0; i < order.book_count; ++i) order.book_ids[i] = plan.book_ids[i];
    kingdom->treasury -= order.purse;
    origin->stock[CC_GOOD_WHEAT] -= order.wheat;
    CcEconomyRefreshSettlementGoodPrice(sim, origin, CC_GOOD_WHEAT);
    carriage->mode = CC_ROYAL_CARRIAGE_ARCHIVE_RESERVED;
    sim->archive_convoy = order;
    return true;
}

bool CcSimCancelArchiveConvoy(CcSim *sim)
{
    if (sim == NULL || sim->schema_version < 89U || sim->archive_convoy.status != 1 ||
        !CcSimArchiveConvoyValid(sim)) return false;
    const CcArchiveConvoyOrder *order = &sim->archive_convoy;
    CcKingdom *kingdom = Funding(sim, order->funding_kingdom_id);
    CcSettlement *origin = CcSimSettlementMutable(sim, order->origin_id);
    CcRoyalCarriage *carriage = Carriage(sim, order->carriage_id);
    if (kingdom == NULL || origin == NULL || carriage == NULL ||
        kingdom->treasury > CC_SIM_MAX_MONEY - order->purse ||
        origin->stock[CC_GOOD_WHEAT] > CC_SIM_MAX_UNITS - order->wheat) return false;
    kingdom->treasury += order->purse;
    origin->stock[CC_GOOD_WHEAT] += order->wheat;
    CcEconomyRefreshSettlementGoodPrice(sim, origin, CC_GOOD_WHEAT);
    carriage->mode = CC_ROYAL_CARRIAGE_IDLE;
    sim->archive_convoy = (CcArchiveConvoyOrder){0};
    return true;
}
