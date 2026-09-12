#include "sim/cc_sim_custody.h"
#include <limits.h>
#include "sim/cc_goods_internal.h"

static int64_t StoredCustodyLoad(const void *context, const CcCustodyEntry *entry,
                                 int64_t quantity)
{
    (void)context;
    if (quantity <= 0) return -1;
    if (entry->kind != CC_CUSTODY_GOODS) return 1;
    if (quantity > INT32_MAX || entry->good < 0 || entry->good >= CC_GOOD_COUNT) return -1;
    return CcGoodsFreightCargoSlots((CcGood)entry->good, (int32_t)quantity);
}

static const CcRoyalCarriage *CustodyCarrier(const CcSim *sim, CcId id)
{
    for (int i = 0; i < sim->royal_carriage_count; ++i)
        if (sim->royal_carriages[i].id == id) return &sim->royal_carriages[i];
    return NULL;
}

int32_t CcSimCustodyCarrierLoad(const CcSim *sim, CcId carrier_id)
{
    if (sim == NULL || sim->schema_version < 99U) return 0;
    int64_t total = 0;
    for (int i = 0; i < CC_CUSTODY_CAPACITY; ++i) {
        const CcCustodyEntry *entry = &sim->custody.entries[i];
        if (!entry->active) continue;
        CcCustodyHolder root = entry->holder;
        if (root.kind == CC_CUSTODY_CONTAINER_HOLDER) {
            const CcCustodyEntry *box = CcCustodyFind(&sim->custody, root.id);
            if (box == NULL) continue;
            root = box->holder;
        }
        if (root.kind != CC_CUSTODY_CARRIER || root.id != carrier_id) continue;
        int64_t weight = StoredCustodyLoad(sim, entry, entry->quantity);
        if (weight <= 0 || weight > CC_ROYAL_CARRIAGE_CARGO_SLOTS - total)
            return CC_ROYAL_CARRIAGE_CARGO_SLOTS + 1;
        total += weight;
    }
    return (int32_t)total;
}

static bool ResolveStoredCustody(const void *context, CcCustodyHolder holder,
                                CcCustodyLocation *location, int64_t *capacity)
{
    const CcSim *sim = context;
    if (holder.kind == CC_CUSTODY_STORE) {
        if (CcSimSettlement(sim, holder.id) == NULL) return false;
        *location = (CcCustodyLocation){.place_id = holder.id};
        *capacity = INT64_MAX;
        return true;
    }
    const CcRoyalCarriage *carrier = CustodyCarrier(sim, holder.id);
    if (holder.kind != CC_CUSTODY_CARRIER || carrier == NULL ||
        carrier->active_shipment_id != 0 || carrier->archive_contract ||
        (carrier->mode != CC_ROYAL_CARRIAGE_IDLE &&
         carrier->mode != CC_ROYAL_CARRIAGE_REPOSITIONING &&
         carrier->mode != CC_ROYAL_CARRIAGE_BLOCKED)) return false;
    *capacity = CC_ROYAL_CARRIAGE_CARGO_SLOTS;
    *location = (CcCustodyLocation){.place_id = carrier->location_id};
    if (carrier->mode == CC_ROYAL_CARRIAGE_REPOSITIONING ||
        carrier->mode == CC_ROYAL_CARRIAGE_BLOCKED) {
        CcFreightLeg leg;
        if (!CcSimFreightLeg(sim, carrier->route_id, carrier->location_id,
                            carrier->destination_id, &leg)) return false;
        if (carrier->mode == CC_ROYAL_CARRIAGE_BLOCKED) {
            *location = (CcCustodyLocation){.route_id = leg.route_id,
                .progress_milli = leg.origin_milli};
            return true;
        }
        if (carrier->arrival_day <= carrier->departure_day) return false;
        int64_t elapsed = sim->current_day - carrier->departure_day;
        int64_t duration = carrier->arrival_day - carrier->departure_day;
        if (elapsed < 0) elapsed = 0;
        if (elapsed > duration) elapsed = duration;
        *location = (CcCustodyLocation){.route_id = leg.route_id,
            .progress_milli = leg.origin_milli + (int32_t)(
                (leg.destination_milli - leg.origin_milli) * elapsed / duration)};
    }
    return true;
}

bool CcSimStoredCustodyValid(const CcSim *sim)
{
    if (sim == NULL) return false;
    const CcCustodyRules rules = {.context = sim,
        .good_count = CC_GOOD_COUNT, .load = StoredCustodyLoad, .resolve = ResolveStoredCustody};
    if (!CcCustodyValidate(&sim->custody, &rules)) return false;
    for (int i = 0; i < CC_CUSTODY_CAPACITY; ++i) {
        const CcCustodyEntry *entry = &sim->custody.entries[i];
        if (!entry->active) continue;
        if (entry->quantity > (entry->kind == CC_CUSTODY_PURSE ?
            CC_SIM_MAX_MONEY : CC_SIM_MAX_UNITS)) return false;
        if (entry->owner_id != sim->player.id &&
            CcSimSettlement(sim, entry->owner_id) == NULL) return false;
    }
    return true;
}

static bool PermitStoreTransfer(const void *context, uint64_t actor,
                                const CcCustodyEntry *entry, CcCustodyHolder to)
{
    const CcSim *sim = context;
    if (actor != entry->owner_id || CcSimSettlement(sim, actor) == NULL) return false;
    CcCustodyHolder root = to;
    if (root.kind == CC_CUSTODY_CONTAINER_HOLDER) {
        const CcCustodyEntry *container = CcCustodyFind(&sim->custody, root.id);
        if (container == NULL) return false;
        root = container->holder;
    }
    if (root.kind == CC_CUSTODY_CARRIER) {
        const CcRoyalCarriage *carrier = CustodyCarrier(sim, root.id);
        if (carrier == NULL || carrier->mode != CC_ROYAL_CARRIAGE_IDLE) return false;
    }
    if (to.kind == CC_CUSTODY_STORE) return CcSimSettlement(sim, to.id) != NULL;
    if (to.kind == CC_CUSTODY_CARRIER) {
        const CcRoyalCarriage *carrier = CustodyCarrier(sim, to.id);
        return carrier != NULL && carrier->kingdom_id == CcSimSettlement(sim, actor)->kingdom_id;
    }
    const CcCustodyEntry *box = CcCustodyFind(&sim->custody, to.id);
    return to.kind == CC_CUSTODY_CONTAINER_HOLDER && box != NULL && box->owner_id == actor;
}

CcCustodyResult CcSimPackStoreGoods(CcSim *sim, CcId town_id,
    CcGood good, int32_t quantity, uint64_t container_id,
    uint64_t container_revision, uint64_t next_id, CcId event_id,
    uint64_t *entry_id)
{
    if (sim == NULL || sim->schema_version < 99U || good < 0 || good >= CC_GOOD_COUNT ||
        quantity <= 0 || event_id == 0 || !CcSimStoredCustodyValid(sim)) return CC_CUSTODY_INVALID;
    CcSettlement *town = CcSimSettlementMutable(sim, town_id);
    if (town == NULL || quantity > town->stock[good]) return CC_CUSTODY_INVALID;
    const CcCustodyEntry *box = CcCustodyFind(&sim->custody, container_id);
    if (box == NULL || box->revision != container_revision || sim->custody.next_id != next_id)
        return CC_CUSTODY_STALE;
    if (box->owner_id != town_id) return CC_CUSTODY_FORBIDDEN;
    if (next_id == UINT64_MAX) return CC_CUSTODY_FULL;
    int slot = -1;
    for (int i = 0; i < CC_CUSTODY_CAPACITY; ++i) {
        if (!sim->custody.entries[i].active && sim->custody.entries[i].quantity == 0) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return CC_CUSTODY_FULL;
    CcCustodyState candidate = sim->custody;
    candidate.entries[slot] = (CcCustodyEntry){.id = next_id, .revision = 1,
        .owner_id = town_id, .holder = {CC_CUSTODY_STORE, town_id},
        .kind = CC_CUSTODY_GOODS, .quantity = quantity, .good = (int32_t)good,
        .condition = 100, .active = true};
    candidate.next_id++;
    const CcCustodyRules rules = {.context = sim, .good_count = CC_GOOD_COUNT, .load = StoredCustodyLoad,
        .resolve = ResolveStoredCustody, .permit = PermitStoreTransfer};
    const CcCustodyTransfer move = {.entry_id = next_id, .revision = 1,
        .actor_id = town_id, .event_id = event_id,
        .destination = {CC_CUSTODY_CONTAINER_HOLDER, container_id}, .quantity = quantity};
    CcCustodyResult result = CcCustodyApplyTransfer(&candidate, &rules, &move, NULL);
    if (result != CC_CUSTODY_READY) return result;
    if (!CcCustodyValidate(&candidate, &rules)) return CC_CUSTODY_INVALID;
    town->stock[good] -= quantity;
    sim->custody = candidate;
    if (entry_id != NULL) *entry_id = next_id;
    return CC_CUSTODY_READY;
}

CcCustodyResult CcSimUnpackStoreGoods(CcSim *sim, CcId town_id,
    uint64_t entry_id, uint64_t revision, int32_t quantity, CcId event_id)
{
    if (sim == NULL || sim->schema_version < 99U || quantity <= 0 || event_id == 0 ||
        !CcSimStoredCustodyValid(sim)) return CC_CUSTODY_INVALID;
    CcSettlement *town = CcSimSettlementMutable(sim, town_id);
    const CcCustodyEntry *entry = CcCustodyFind(&sim->custody, entry_id);
    if (entry == NULL || entry->revision != revision) return CC_CUSTODY_STALE;
    if (town == NULL || entry->kind != CC_CUSTODY_GOODS || quantity > entry->quantity)
        return CC_CUSTODY_INVALID;
    if (entry->owner_id != town_id) return CC_CUSTODY_FORBIDDEN;
    int32_t good = entry->good;
    if (town->stock[good] > CC_SIM_MAX_UNITS - quantity) return CC_CUSTODY_FULL;
    CcCustodyState candidate = sim->custody;
    const CcCustodyRules rules = {.context = sim, .good_count = CC_GOOD_COUNT, .load = StoredCustodyLoad,
        .resolve = ResolveStoredCustody, .permit = PermitStoreTransfer};
    const CcCustodyTransfer move = {.entry_id = entry_id, .revision = revision,
        .actor_id = town_id, .event_id = event_id,
        .destination = {CC_CUSTODY_STORE, town_id}, .quantity = quantity};
    uint64_t moved_id = 0;
    CcCustodyResult result = CcCustodyApplyTransfer(&candidate, &rules, &move, &moved_id);
    if (result != CC_CUSTODY_READY) return result;
    for (int i = 0; i < CC_CUSTODY_CAPACITY; ++i) {
        CcCustodyEntry *moved = &candidate.entries[i];
        if (moved->id == moved_id) {
            moved->quantity = 0;
            moved->active = false;
            break;
        }
    }
    if (!CcCustodyValidate(&candidate, &rules)) return CC_CUSTODY_INVALID;
    town->stock[good] += quantity;
    sim->custody = candidate;
    return CC_CUSTODY_READY;
}

CcCustodyResult CcSimTransferCustody(CcSim *sim, const CcCustodyTransfer *transfer,
                                    uint64_t *result_id)
{
    if (sim == NULL || sim->schema_version < 99U || !CcSimStoredCustodyValid(sim))
        return CC_CUSTODY_INVALID;
    const CcCustodyRules rules = {.context = sim, .good_count = CC_GOOD_COUNT,
        .load = StoredCustodyLoad, .resolve = ResolveStoredCustody, .permit = PermitStoreTransfer};
    return CcCustodyApplyTransfer(&sim->custody, &rules, transfer, result_id);
}

CcCustodyResult CcSimMakeCustodyContainer(CcSim *sim, CcProductionContext *work,
    uint64_t next_id, CcId event_id, uint64_t *entry_id, CcProductionReceipt *receipt)
{
    if (sim == NULL || work == NULL || sim->schema_version < 99U || event_id == 0 ||
        !CcSimStoredCustodyValid(sim)) return CC_CUSTODY_INVALID;
    const CcSettlement *town = CcSimSettlement(sim, work->producer_id);
    if (town == NULL || work->storage_id != town->id || work->location_id != town->id ||
        work->stock != town->stock) return CC_CUSTODY_INVALID;
    if (next_id != sim->custody.next_id) return CC_CUSTODY_STALE;
    if (next_id == UINT64_MAX) return CC_CUSTODY_FULL;
    int slot = -1;
    for (int i = 0; i < CC_CUSTODY_CAPACITY; ++i) {
        if (!sim->custody.entries[i].active && sim->custody.entries[i].quantity == 0) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return CC_CUSTODY_FULL;
    const CcProductionRecipe recipe = {.output = CC_GOOD_COUNT, .output_units = 1,
        .work_only = true, .input_count = 2,
        .inputs = {{CC_GOOD_WOOD, 4, 0}, {CC_GOOD_IRON, 1, 0}},
        .work_per_batch = 2, .tools_required = 1, .minimum_condition = 20,
        .hunger_soft_limit = 100, .hunger_hard_limit = 100};
    CcProductionContext one = *work;
    if (one.capacity > 1) one.capacity = 1;
    if (one.output_limit > 1) one.output_limit = 1;
    CcProductionReceipt plan = CcProductionPlan(&recipe, &one);
    if (plan.gate != CC_PRODUCTION_READY) return CC_CUSTODY_INVALID;
    /* Allocation and recipe checks finish before either stock or work changes. */
    CcProductionReceipt done = CcProductionRun(&recipe, &one);
    if (done.gate != CC_PRODUCTION_READY) return CC_CUSTODY_INVALID;
    sim->custody.entries[slot] = (CcCustodyEntry){.id = next_id, .revision = 1,
        .owner_id = town->id, .last_event_id = event_id,
        .holder = {CC_CUSTODY_STORE, town->id}, .kind = CC_CUSTODY_CONTAINER,
        .quantity = 1, .condition = 100, .capacity = 10, .active = true};
    sim->custody.next_id++;
    work->work_available -= done.work;
    work->capacity -= done.batches;
    work->output_limit -= done.batches;
    if (entry_id != NULL) *entry_id = next_id;
    if (receipt != NULL) *receipt = done;
    return CC_CUSTODY_READY;
}

CcCustodyResult CcSimRepairCustodyContainer(CcSim *sim, CcProductionContext *work,
    uint64_t container_id, uint64_t revision, CcId event_id, CcProductionReceipt *receipt)
{
    if (sim == NULL || work == NULL || sim->schema_version < 99U || event_id == 0 ||
        !CcSimStoredCustodyValid(sim)) return CC_CUSTODY_INVALID;
    const CcSettlement *town = CcSimSettlement(sim, work->producer_id);
    if (town == NULL || work->storage_id != town->id || work->location_id != town->id ||
        work->stock != town->stock) return CC_CUSTODY_INVALID;
    const CcCustodyEntry *found = CcCustodyFind(&sim->custody, container_id);
    if (found == NULL || found->revision != revision || revision == UINT64_MAX) return CC_CUSTODY_STALE;
    if (found->kind != CC_CUSTODY_CONTAINER || found->condition >= 100) return CC_CUSTODY_INVALID;
    if (found->owner_id != town->id) return CC_CUSTODY_FORBIDDEN;
    if (found->holder.kind != CC_CUSTODY_STORE || found->holder.id != town->id) return CC_CUSTODY_REMOTE;
    const CcProductionRecipe recipe = {.output = CC_GOOD_COUNT, .output_units = 1,
        .work_only = true, .input_count = 1, .inputs = {{CC_GOOD_WOOD, 1, 0}},
        .work_per_batch = 1, .tools_required = 1, .minimum_condition = 20,
        .hunger_soft_limit = 100, .hunger_hard_limit = 100};
    CcProductionContext one = *work;
    if (one.capacity > 1) one.capacity = 1;
    if (one.output_limit > 1) one.output_limit = 1;
    CcProductionReceipt done = CcProductionRun(&recipe, &one);
    if (done.gate != CC_PRODUCTION_READY) return CC_CUSTODY_INVALID;
    CcCustodyEntry *box = &sim->custody.entries[found - sim->custody.entries];
    box->condition = box->condition > 75 ? 100 : box->condition + 25;
    box->revision++;
    box->last_event_id = event_id;
    work->work_available -= done.work;
    work->capacity -= done.batches;
    work->output_limit -= done.batches;
    if (receipt != NULL) *receipt = done;
    return CC_CUSTODY_READY;
}
