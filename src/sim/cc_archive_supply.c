#include "sim/cc_archive_internal.h"
#include "sim/cc_food_economy_internal.h"
#include "sim/cc_goods_internal.h"
#include "sim/cc_route_rules_internal.h"
#include "sim/cc_trade_path_internal.h"
#include <limits.h>

static int32_t Min(int32_t a, int32_t b) { return a < b ? a : b; }
static int32_t Max(int32_t a, int32_t b) { return a > b ? a : b; }

/* Protect the town's food reserve while supplying one weekly archive task. */
static int32_t WheatNeed(const CcSim *sim, const CcSettlement *town)
{
    int32_t work = 2 * Max(1, sim->archives.scribes);
    int32_t rations = CcEconomyNutritionRations(town->stock, CC_NUTRITION_CIVILIAN);
    int32_t deficit = Max(0, 2 * CcEconomyWeeklyFoodUse(sim, town) - rations);
    return Max(work - CcArchiveSpareGrain(sim, town), deficit * 2 + (deficit > 0 ? work : 0));
}

CcArchiveSupplyPlan CcSimArchiveSupplyPlan(const CcSim *sim, CcId carriage_id)
{
    CcArchiveSupplyPlan plan = {.gate = CC_ARCHIVE_SUPPLY_UNAVAILABLE, .good = CC_GOOD_COUNT};
    if (sim == NULL || sim->schema_version < 58U) return plan;
    const CcSettlement *seat = CcArchiveSeat(sim);
    plan.gate = CC_ARCHIVE_SUPPLY_SEAT;
    if (seat == NULL) return plan;
    plan.seat_id = seat->id;
    CcSettlement projected = *seat;
    for (int i = 0; i < CC_GOOD_COUNT; ++i)
        projected.stock[i] += Min(CC_SIM_MAX_UNITS - projected.stock[i],
            CcSimIncomingGood(sim, seat->id, (CcGood)i));
    const CcGood goods[] = {CC_GOOD_WHEAT, CC_GOOD_TOOLS, CC_GOOD_PAPER};
    bool shortage = false;
    int32_t need = 0;
    for (int i = 0; i < 3; ++i) {
        CcGood good = goods[i];
        int32_t current = good == CC_GOOD_WHEAT ? WheatNeed(sim, seat) : Max(0, 1 - seat->stock[good]);
        if (current <= 0) continue;
        shortage = true;
        need = good == CC_GOOD_WHEAT ? WheatNeed(sim, &projected) : Max(0, 1 - projected.stock[good]);
        if (need > 0) { plan.good = good; break; }
    }
    plan.gate = shortage ? CC_ARCHIVE_SUPPLY_INCOMING : CC_ARCHIVE_SUPPLY_STOCKED;
    if (plan.good == CC_GOOD_COUNT) return plan;
    const CcRoyalCarriage *carriage = NULL;
    for (int i = 0; i < sim->royal_carriage_count; ++i)
        if (sim->royal_carriages[i].id == carriage_id) carriage = &sim->royal_carriages[i];
    plan.gate = CC_ARCHIVE_SUPPLY_CARRIAGE;
    if (carriage == NULL || carriage->kingdom_id != seat->kingdom_id ||
        carriage->mode != CC_ROYAL_CARRIAGE_IDLE || carriage->active_shipment_id != 0U ||
        carriage->condition < 20 || sim->current_day < carriage->next_dispatch_day) return plan;
    plan.carriage_id = carriage_id;
    const int32_t *used = sim->royal_trade_week == sim->current_day / 7 ? sim->royal_route_slots_used : NULL;
    bool source_found = false, path_found = false;
    int64_t best_score = INT64_MAX;
    for (int i = 0; i < sim->settlement_count; ++i) {
        const CcSettlement *source = &sim->settlements[i];
        if (source->id == seat->id || CcSettlementIsAbandoned(source)) continue;
        int32_t floor = source->reserve_target[plan.good];
        if (plan.good == CC_GOOD_WHEAT) floor = Max(floor, 6 * CcEconomyWeeklyFoodUse(sim, source));
        int32_t surplus = source->stock[plan.good] - floor;
        if (surplus <= 0) continue;
        source_found = true;
        int32_t route = -1, cost = 0, capacity = 0, reposition = 0;
        CcId hop = 0;
        if (!CcTradeFindPath(sim, source->id, seat->id, plan.good, &route, &hop,
            &cost, &capacity, used, true, carriage->kingdom_id, false, 1)) continue;
        if (carriage->location_id != source->id && !CcTradeFindPath(sim, carriage->location_id,
            source->id, plan.good, NULL, NULL, &reposition, NULL, NULL,
            true, carriage->kingdom_id, false, 1)) continue;
        path_found = true;
        CcMoney toll = CcRouteRoyalTradeToll(sim, &sim->routes[route], carriage->kingdom_id);
        CcMoney price = Max(1, source->price[plan.good]);
        if (sim->iron_ledger_reserve <= toll) continue;
        CcMoney affordable = (sim->iron_ledger_reserve - toll) / price;
        int32_t quantity = Min(need, Min(surplus, Min(capacity, CC_ROYAL_CARRIAGE_CARGO_SLOTS) *
            CcGoodsFreightUnitsPerCargoSlot(plan.good)));
        quantity = Min(quantity, affordable > INT32_MAX ? INT32_MAX : (int32_t)affordable);
        if (quantity <= 0) continue;
        CcMoney charge = price * quantity + toll;
        /* Compare the cost of one unit plus travel, then break ties by stable ID. */
        int64_t score = price + toll + cost + reposition;
        if (score > best_score || (score == best_score && source->id >= plan.source_id)) continue;
        best_score = score;
        plan.source_id = source->id; plan.first_route_id = sim->routes[route].id;
        plan.first_hop_id = hop; plan.path_cost = cost; plan.reposition_cost = reposition;
        plan.path_capacity = capacity; plan.quantity = quantity;
        plan.goods_cost = price * quantity; plan.first_leg_toll = toll; plan.total_charge = charge;
    }
    plan.gate = !source_found ? CC_ARCHIVE_SUPPLY_SOURCE : !path_found ? CC_ARCHIVE_SUPPLY_ROUTE :
        plan.source_id == 0U ? CC_ARCHIVE_SUPPLY_FUNDS : CC_ARCHIVE_SUPPLY_READY;
    return plan;
}

const char *CcArchiveSupplyGateName(CcArchiveSupplyGate gate)
{
    switch (gate) {
        case CC_ARCHIVE_SUPPLY_READY: return "ready";
        case CC_ARCHIVE_SUPPLY_UNAVAILABLE: return "unavailable";
        case CC_ARCHIVE_SUPPLY_SEAT: return "archive_seat";
        case CC_ARCHIVE_SUPPLY_STOCKED: return "stocked";
        case CC_ARCHIVE_SUPPLY_INCOMING: return "incoming_supply";
        case CC_ARCHIVE_SUPPLY_CARRIAGE: return "carriage";
        case CC_ARCHIVE_SUPPLY_SOURCE: return "source_stock";
        case CC_ARCHIVE_SUPPLY_ROUTE: return "route";
        case CC_ARCHIVE_SUPPLY_FUNDS: return "archive_funds";
    }
    return "unknown";
}
