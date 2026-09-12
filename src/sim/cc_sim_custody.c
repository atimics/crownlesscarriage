#include "sim/cc_sim_custody.h"
#include <limits.h>

static bool ResolveStoredCustody(const void *context, CcCustodyHolder holder,
                                CcCustodyLocation *location, int64_t *capacity)
{
    const CcSim *sim = context;
    if (holder.kind != CC_CUSTODY_STORE || CcSimSettlement(sim, holder.id) == NULL)
        return false;
    *location = (CcCustodyLocation){.place_id = holder.id};
    *capacity = INT64_MAX;
    return true;
}

bool CcSimStoredCustodyValid(const CcSim *sim)
{
    if (sim == NULL) return false;
    const CcCustodyRules rules = {.context = sim,
        .good_count = CC_GOOD_COUNT, .resolve = ResolveStoredCustody};
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
    if (to.kind == CC_CUSTODY_STORE) return to.id == actor;
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
    const CcCustodyRules rules = {.context = sim, .good_count = CC_GOOD_COUNT,
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
    const CcCustodyRules rules = {.context = sim, .good_count = CC_GOOD_COUNT,
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
