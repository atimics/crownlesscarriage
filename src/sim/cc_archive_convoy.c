#include "sim/cc_archive_relocation.h"
#include "sim/cc_archive_internal.h"
#include "sim/cc_identity_internal.h"
#include "sim/cc_food_economy_internal.h"
#include "sim/cc_archive_volumes_internal.h"
#include "sim/cc_route_rules_internal.h"
#include "sim/cc_trade_path_internal.h"
#include <string.h>

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
static CcId Home(const CcArchiveConvoyOrder *o) { return o->home_id != 0 ? o->home_id : o->origin_id; }
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
    if (sim->schema_version >= 90U && o->status <= 1 && (o->departure_day != 0 || o->arrival_day != 0)) return false;
    if (sim->schema_version >= 91U && ((o->status == 0 && o->home_id != 0) ||
        (o->status != 0 && CcSimSettlement(sim, Home(o)) == NULL))) return false;
    if (o->status == 0) {
        for (int i = 0; i < 4; ++i) if (o->book_ids[i] != 0) return false;
        return o->origin_id == 0 && o->destination_id == 0 && o->sponsor_id == 0 &&
            o->funding_kingdom_id == 0 && o->carriage_id == 0 && o->first_route_id == 0 &&
            o->first_hop_id == 0 && o->purse == 0 && o->wheat == 0 && o->book_count == 0 && o->reserved_day == 0;
    }
    bool journey = sim->schema_version >= 90U && o->status >= 2 && o->status <= (sim->schema_version >= 91U ? 6 : 5);
    if ((!journey && o->status != 1) || o->book_count < 1 || o->book_count > 4 ||
        o->wheat < (journey ? 0 : 1) || o->wheat > CC_SIM_MAX_UNITS || o->purse < 0 || o->purse > CC_SIM_MAX_MONEY ||
        o->reserved_day < 1 || o->reserved_day > sim->current_day ||
        o->origin_id == o->destination_id || CcSimSettlement(sim, o->origin_id) == NULL ||
        CcSimSettlement(sim, o->destination_id) == NULL ||
        !Issued(sim, o->sponsor_id, CC_ENTITY_CHARACTER) || sim->archive_recruitment.status != 0) return false;
    bool funding = false, carriage = false;
    for (int i = 0; i < sim->kingdom_count; ++i) funding |= sim->kingdoms[i].id == o->funding_kingdom_id;
    for (int i = 0; i < sim->royal_carriage_count; ++i) {
        const CcRoyalCarriage *item = &sim->royal_carriages[i];
        if (item->id != o->carriage_id) continue;
        if ((o->status == 3 && (sim->schema_version < 91U || o->home_id == 0)) || o->status == 4 || o->status == 6) { carriage = true; continue; }
        if (o->status == 3) {
            carriage = item->mode == CC_ROYAL_CARRIAGE_ARCHIVE_RESERVED && item->kingdom_id == o->funding_kingdom_id &&
                item->blocked_since_day == 0 && item->location_id == o->first_hop_id &&
                item->active_shipment_id == 0 && item->route_id == 0 && item->target_id == 0 &&
                item->destination_id == 0 && item->arrival_day == 0 && !item->archive_contract;
            continue;
        }
        if (o->status == 2 || o->status == 5) {
            carriage = item->mode == (o->status == 2 ? CC_ROYAL_CARRIAGE_ARCHIVE_TRAVELLING : CC_ROYAL_CARRIAGE_ARCHIVE_WAITING) &&
                item->kingdom_id == o->funding_kingdom_id && item->location_id == o->origin_id &&
                item->active_shipment_id == 0 && item->route_id == o->first_route_id &&
                item->destination_id == o->first_hop_id && item->target_id == o->destination_id &&
                item->departure_day == o->departure_day && item->arrival_day == o->arrival_day &&
                item->blocked_since_day == 0 && item->archive_contract;
        } else carriage = item->mode == CC_ROYAL_CARRIAGE_ARCHIVE_RESERVED &&
            item->kingdom_id == o->funding_kingdom_id && item->location_id == o->origin_id &&
            item->active_shipment_id == 0 && item->route_id == 0 && item->destination_id == 0 &&
            item->target_id == 0 && item->arrival_day == 0 && item->blocked_since_day == 0 && !item->archive_contract;
    }
    const CcRoute *route = CcSimRoute(sim, o->first_route_id);
    if (!funding || !carriage || route == NULL ||
        !((route->from_id == o->origin_id && route->to_id == o->first_hop_id) ||
          (route->to_id == o->origin_id && route->from_id == o->first_hop_id))) return false;
    if (journey && (o->departure_day < o->reserved_day || o->departure_day > sim->current_day ||
        o->arrival_day <= o->departure_day || o->arrival_day > CC_SIM_MAX_DAY ||
        (o->status == 2 ? o->arrival_day <= sim->current_day : o->arrival_day > sim->current_day))) return false;
    for (int i = 0; i < 4; ++i) {
        if (i >= o->book_count) { if (o->book_ids[i] != 0) return false; continue; }
        if (!Issued(sim, o->book_ids[i], CC_ENTITY_TREASURE)) return false;
        if (o->status == 2 || o->status == 5) {
            const CcTreasure *book = CcSimTreasure(sim, o->book_ids[i]);
            if (!CcArchiveVolumeIsLive(book) || book->owner_id != Home(o) || book->location_id != o->carriage_id) return false;
        }
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
        .reserved_day = sim->current_day, .status = 1,
        .home_id = sim->schema_version >= 91U ? plan.origin_id : 0};
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
    if (sim == NULL || sim->schema_version < 89U ||
        (sim->archive_convoy.status != 1 && !(sim->schema_version >= 90U &&
         (sim->archive_convoy.status == 3 || sim->archive_convoy.status == 4 ||
          (sim->schema_version >= 91U && sim->archive_convoy.status == 6)))) ||
        !CcSimArchiveConvoyValid(sim)) return false;
    const CcArchiveConvoyOrder *order = &sim->archive_convoy;
    CcKingdom *kingdom = Funding(sim, order->funding_kingdom_id);
    CcSettlement *origin = CcSimSettlementMutable(sim, order->status == 1 ? order->origin_id : order->first_hop_id);
    CcRoyalCarriage *carriage = Carriage(sim, order->carriage_id);
    if (kingdom == NULL || origin == NULL || carriage == NULL ||
        kingdom->treasury > CC_SIM_MAX_MONEY - order->purse ||
        origin->stock[CC_GOOD_WHEAT] > CC_SIM_MAX_UNITS - order->wheat) return false;
    kingdom->treasury += order->purse;
    origin->stock[CC_GOOD_WHEAT] += order->wheat;
    CcEconomyRefreshSettlementGoodPrice(sim, origin, CC_GOOD_WHEAT);
    if (order->status == 1 || (sim->schema_version >= 91U && order->status == 3 &&
        carriage->mode == CC_ROYAL_CARRIAGE_ARCHIVE_RESERVED && carriage->location_id == order->first_hop_id))
        carriage->mode = CC_ROYAL_CARRIAGE_IDLE;
    sim->archive_convoy = (CcArchiveConvoyOrder){0};
    return true;
}

bool CcSimArchiveConvoyCarriesBook(const CcSim *sim, CcId book_id)
{
    if (sim == NULL || sim->schema_version < 90U || book_id == 0 ||
        (sim->archive_convoy.status != 2 && sim->archive_convoy.status != 5)) return false;
    for (int i = 0; i < sim->archive_convoy.book_count && i < 4; ++i)
        if (sim->archive_convoy.book_ids[i] == book_id) return true;
    return false;
}
static CcTreasure *Book(CcSim *sim, CcId id)
{
    for (int i = 0; i < sim->treasure_count; ++i) if (sim->treasures[i].id == id) return &sim->treasures[i];
    return NULL;
}
bool CcSimArchiveConvoyHoldsBook(const CcSim *sim, CcId book_id)
{
    if (CcSimArchiveConvoyCarriesBook(sim, book_id)) return true;
    if (sim == NULL || sim->schema_version < 91U || sim->archive_convoy.status != 3) return false;
    for (int i = 0; i < sim->archive_convoy.book_count && i < 4; ++i)
        if (sim->archive_convoy.book_ids[i] == book_id) return true;
    return false;
}
static bool CompleteSeat(CcSim *sim, CcRoyalCarriage *carriage)
{
    CcArchiveConvoyOrder *o = &sim->archive_convoy;
    const CcSettlement *destination = CcSimSettlement(sim, o->destination_id);
    if (o->first_hop_id != o->destination_id || destination == NULL ||
        destination->kingdom_id != o->funding_kingdom_id ||
        !CcSimArchiveSeatCandidate(sim, destination->id).viable) return false;
    for (int i = 0; i < o->book_count; ++i) {
        const CcTreasure *book = Book(sim, o->book_ids[i]);
        if (!CcArchiveVolumeIsLive(book) || book->owner_id != Home(o) || book->location_id != destination->id) return false;
    }
    for (int i = 0; i < o->book_count; ++i) Book(sim, o->book_ids[i])->owner_id = destination->id;
    sim->archives.seat_id = destination->id;
    sim->archives.seat_failed_since_day = 0;
    sim->archives.scribes = 0; sim->archives.dead_since_day = sim->current_day;
    sim->archive_staff = (CcArchiveStaff){0};
    o->status = 6;
    if (carriage->mode == CC_ROYAL_CARRIAGE_ARCHIVE_RESERVED && carriage->location_id == destination->id) {
        carriage->mode = CC_ROYAL_CARRIAGE_IDLE; carriage->next_dispatch_day = sim->current_day + 7;
    }
    return true;
}
static bool PrepareNextLeg(CcSim *sim, CcRoyalCarriage *carriage)
{
    CcArchiveConvoyOrder *o = &sim->archive_convoy;
    CcSettlement *stop = CcSimSettlementMutable(sim, o->first_hop_id);
    if (sim->current_day <= o->arrival_day || stop == NULL || stop->id == o->destination_id ||
        carriage->location_id != stop->id || carriage->active_shipment_id != 0 ||
        (carriage->mode != CC_ROYAL_CARRIAGE_ARCHIVE_RESERVED && !(o->home_id == 0 && carriage->mode == CC_ROYAL_CARRIAGE_IDLE))) return false;
    for (int i = 0; i < o->book_count; ++i) {
        const CcTreasure *book = Book(sim, o->book_ids[i]);
        if (!CcArchiveVolumeIsLive(book) || book->owner_id != Home(o) || book->location_id != stop->id) return false;
    }
    const CcCharacter *sponsor = CcSimCharacter(sim, o->sponsor_id);
    if (sponsor == NULL || sponsor->death_day <= sim->current_day) return false;
    int slot = -1; CcId hop = 0;
    const int32_t *used = sim->royal_trade_week == sim->current_day / 7 ? sim->royal_route_slots_used : NULL;
    if (!CcTradeFindPath(sim, stop->id, o->destination_id, CC_GOOD_PAPER, &slot, &hop, NULL, NULL,
        used, true, carriage->kingdom_id, true, o->book_count)) return false;
    int days = CcSimFreightLegDays(sim, sim->routes[slot].id, stop->id, hop);
    if (days < 1 || sim->current_day > CC_SIM_MAX_DAY - days) return false;
    int wheat = 2 * ((days + 6) / 7) - o->wheat; if (wheat < 0) wheat = 0;
    CcMoney toll = CcRouteRoyalTradeToll(sim, &sim->routes[slot], carriage->kingdom_id);
    CcMoney topup = toll > o->purse ? toll - o->purse : 0;
    CcKingdom *funder = Funding(sim, o->funding_kingdom_id);
    if (funder == NULL || funder->treasury < topup || CcArchiveSpareGrain(sim, stop) < wheat) return false;
    o->home_id = Home(o);
    funder->treasury -= topup; o->purse += topup;
    stop->stock[CC_GOOD_WHEAT] -= wheat; o->wheat += wheat;
    CcEconomyRefreshSettlementGoodPrice(sim, stop, CC_GOOD_WHEAT);
    o->origin_id = stop->id; o->first_route_id = sim->routes[slot].id; o->first_hop_id = hop;
    o->reserved_day = sim->current_day; o->departure_day = 0; o->arrival_day = 0; o->status = 1;
    carriage->mode = CC_ROYAL_CARRIAGE_ARCHIVE_RESERVED;
    return true;
}

CcArchiveConvoyStep CcSimAdvanceArchiveConvoy(CcSim *sim, uint32_t road_roll)
{
    if (sim == NULL || sim->schema_version < 90U) return CC_ARCHIVE_CONVOY_WAIT;
    CcArchiveConvoyOrder *o = &sim->archive_convoy;
    CcRoyalCarriage *carriage = Carriage(sim, o->carriage_id);
    if (carriage == NULL) return CC_ARCHIVE_CONVOY_WAIT;
    if (sim->schema_version >= 91U && o->status == 6) {
        (void)CcSimCancelArchiveConvoy(sim); return CC_ARCHIVE_CONVOY_WAIT;
    }
    if (sim->schema_version >= 91U && o->status == 3) {
        if (CompleteSeat(sim, carriage)) return CC_ARCHIVE_CONVOY_COMPLETED;
        if (!PrepareNextLeg(sim, carriage)) return CC_ARCHIVE_CONVOY_WAIT;
    }
    if (o->status != 1 && o->status != 2 && o->status != 5) return CC_ARCHIVE_CONVOY_WAIT;
    if (sim->schema_version >= 91U && o->home_id == 0) o->home_id = o->origin_id;
    if (o->status == 1) {
        if (!CcSimArchiveConvoyValid(sim)) return CC_ARCHIVE_CONVOY_WAIT;
        const CcCharacter *sponsor = CcSimCharacter(sim, o->sponsor_id);
        if (sponsor == NULL || sponsor->death_day <= sim->current_day) return CC_ARCHIVE_CONVOY_WAIT;
        for (int i = 0; i < o->book_count; ++i) {
            CcTreasure *book = Book(sim, o->book_ids[i]);
            if (!CcArchiveVolumeIsLive(book) || book->owner_id != Home(o) || book->location_id != o->origin_id)
                return CC_ARCHIVE_CONVOY_WAIT;
        }
        int slot = -1; CcId hop = 0;
        const int32_t *used = sim->royal_trade_week == sim->current_day / 7 ? sim->royal_route_slots_used : NULL;
        if (!CcTradeFindPath(sim, o->origin_id, o->destination_id, CC_GOOD_PAPER, &slot, &hop,
            NULL, NULL, used, true, carriage->kingdom_id, true, o->book_count) ||
            sim->routes[slot].id != o->first_route_id || hop != o->first_hop_id) return CC_ARCHIVE_CONVOY_WAIT;
        int days = CcSimFreightLegDays(sim, o->first_route_id, o->origin_id, o->first_hop_id);
        int wheat = 2 * ((days + 6) / 7);
        CcMoney toll = CcRouteRoyalTradeToll(sim, &sim->routes[slot], carriage->kingdom_id);
        const CcSettlement *stop = CcSimSettlement(sim, hop);
        CcKingdom *receiver = stop != NULL ? Funding(sim, stop->kingdom_id) : NULL;
        if (days < 1 || sim->current_day > CC_SIM_MAX_DAY - days || o->wheat < wheat || o->purse < toll ||
            receiver == NULL || receiver->treasury > CC_SIM_MAX_MONEY - toll) return CC_ARCHIVE_CONVOY_WAIT;
        o->purse -= toll; receiver->treasury += toll; o->wheat -= wheat;
        o->departure_day = sim->current_day; o->arrival_day = sim->current_day + days; o->status = 2;
        for (int i = 0; i < o->book_count; ++i) Book(sim, o->book_ids[i])->location_id = carriage->id;
        carriage->mode = CC_ROYAL_CARRIAGE_ARCHIVE_TRAVELLING; carriage->archive_contract = true;
        carriage->route_id = o->first_route_id; carriage->destination_id = o->first_hop_id;
        carriage->target_id = o->destination_id; carriage->departure_day = o->departure_day;
        carriage->arrival_day = o->arrival_day;
        if (sim->royal_trade_week != sim->current_day / 7) {
            sim->royal_trade_week = sim->current_day / 7;
            memset(sim->royal_route_slots_used, 0, sizeof(sim->royal_route_slots_used));
        }
        sim->royal_route_slots_used[slot] += o->book_count;
        return CC_ARCHIVE_CONVOY_DEPARTED;
    }
    if (sim->current_day < o->arrival_day) return CC_ARCHIVE_CONVOY_WAIT;
    const CcRoute *route = CcSimRoute(sim, o->first_route_id);
    if (route == NULL || route->condition == 0 || !CcRouteCarriageCanUse(sim, carriage, route->id)) {
        if (o->status == 5) return CC_ARCHIVE_CONVOY_WAIT;
        o->status = 5; carriage->mode = CC_ROYAL_CARRIAGE_ARCHIVE_WAITING;
        return CC_ARCHIVE_CONVOY_BLOCKED;
    }
    int danger = CcSimRouteDanger(sim, route->id);
    if (CcSimRouteCrossesWarBorder(sim, route->id) && !route->smuggler_route) {
        danger += 25; if (danger > 85) danger = 85;
    }
    bool lost = (int)(road_roll % 100U) < danger;
    int64_t lore_lost = 0;
    for (int i = 0; i < o->book_count; ++i) {
        CcTreasure *book = Book(sim, o->book_ids[i]);
        if (book == NULL) continue;
        book->location_id = o->first_hop_id;
        if (lost && !book->destroyed) { lore_lost += book->craft_work; book->destroyed = true; }
    }
    if (lost) {
        int64_t total = (int64_t)sim->archives.lore_lost_total + lore_lost;
        sim->archives.lore_lost_total = total > CC_SIM_MAX_UNITS ? CC_SIM_MAX_UNITS : (int32_t)total;
        CcSimUpgradeArchivePhysicalLore(sim);
    }
    o->status = lost ? 4 : 3;
    carriage->mode = CC_ROYAL_CARRIAGE_IDLE; carriage->archive_contract = false;
    carriage->location_id = o->first_hop_id; carriage->route_id = 0; carriage->destination_id = 0;
    carriage->target_id = 0; carriage->departure_day = sim->current_day; carriage->arrival_day = 0;
    carriage->next_dispatch_day = sim->current_day + 7;
    if (lost) {
        if (carriage->cargo_losses < CC_SIM_MAX_UNITS) carriage->cargo_losses++;
        carriage->condition = carriage->condition > 12 ? carriage->condition - 12 : 0;
    } else {
        if (carriage->trips_completed < CC_SIM_MAX_UNITS) carriage->trips_completed++;
        if (carriage->condition > 0) carriage->condition--;
    }
    if (!lost && sim->schema_version >= 91U) {
        carriage->mode = CC_ROYAL_CARRIAGE_ARCHIVE_RESERVED;
        if (CompleteSeat(sim, carriage)) return CC_ARCHIVE_CONVOY_COMPLETED;
    }
    return lost ? CC_ARCHIVE_CONVOY_LOST : CC_ARCHIVE_CONVOY_ARRIVED;
}
