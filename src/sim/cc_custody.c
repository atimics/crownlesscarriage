#include "sim/cc_custody.h"
#include <limits.h>
#include <stddef.h>

void CcCustodyInit(CcCustodyState *state)
{
    *state = (CcCustodyState){.next_id = 1};
}

const CcCustodyEntry *CcCustodyFind(const CcCustodyState *state, uint64_t id)
{
    if (state == NULL || id == 0) return NULL;
    for (int i = 0; i < CC_CUSTODY_CAPACITY; ++i)
        if (state->entries[i].active && state->entries[i].id == id)
            return &state->entries[i];
    return NULL;
}

static bool SameHolder(CcCustodyHolder a, CcCustodyHolder b)
{
    return a.kind == b.kind && a.id == b.id;
}

static bool RootHolder(const CcCustodyState *state, CcCustodyHolder holder,
                       CcCustodyHolder *root)
{
    if (holder.id == 0 || holder.kind < CC_CUSTODY_STORE ||
        holder.kind > CC_CUSTODY_CONTAINER_HOLDER) return false;
    if (holder.kind == CC_CUSTODY_CONTAINER_HOLDER) {
        const CcCustodyEntry *box = CcCustodyFind(state, holder.id);
        if (box == NULL || box->kind != CC_CUSTODY_CONTAINER ||
            box->holder.kind == CC_CUSTODY_CONTAINER_HOLDER) return false;
        holder = box->holder;
    }
    if (holder.id == 0 || holder.kind < CC_CUSTODY_STORE ||
        holder.kind >= CC_CUSTODY_CONTAINER_HOLDER) return false;
    *root = holder;
    return true;
}

static bool ValidLocation(CcCustodyLocation location)
{
    return (location.place_id != 0) != (location.route_id != 0) &&
        location.progress_milli >= 0 && location.progress_milli <= 1000 &&
        (location.place_id == 0 || location.progress_milli == 0);
}

static bool AddLoad(int64_t *total, int64_t amount)
{
    if (amount < 0 || *total > INT64_MAX - amount) return false;
    *total += amount;
    return true;
}

static int64_t EntryLoad(const CcCustodyRules *rules, const CcCustodyEntry *entry, int64_t quantity)
{
    if (quantity == 0) return 0;
    if (rules->load != NULL) return rules->load(rules->context, entry, quantity);
    return entry->kind == CC_CUSTODY_GOODS ? quantity : 1;
}

static bool Load(const CcCustodyState *state, const CcCustodyRules *rules, CcCustodyHolder holder,
                 bool root_load, int64_t *load, int *count)
{
    *load = 0;
    *count = 0;
    for (int i = 0; i < CC_CUSTODY_CAPACITY; ++i) {
        const CcCustodyEntry *entry = &state->entries[i];
        if (!entry->active) continue;
        CcCustodyHolder actual = entry->holder;
        if (entry->quantity <= 0 || (root_load && !RootHolder(state, actual, &actual)))
            return false;
        if (!SameHolder(actual, holder)) continue;
        int64_t weight = EntryLoad(rules, entry, entry->quantity);
        if (weight <= 0 || !AddLoad(load, weight)) return false;
        ++*count;
    }
    return true;
}

static bool EmptyEntry(const CcCustodyEntry *entry)
{
    return entry->revision == 0 && entry->owner_id == 0 && entry->source_id == 0 &&
        entry->last_event_id == 0 && entry->holder.kind == CC_CUSTODY_STORE &&
        entry->holder.id == 0 && entry->kind == CC_CUSTODY_GOODS &&
        entry->reference_id == 0 && entry->quantity == 0 && entry->good == 0 &&
        entry->condition == 0 && entry->capacity == 0 && !entry->active;
}

bool CcCustodyValidate(const CcCustodyState *state, const CcCustodyRules *rules)
{
    if (state == NULL || state->next_id == 0 || rules == NULL ||
        rules->resolve == NULL || rules->good_count <= 0) return false;
    for (int i = 0; i < CC_CUSTODY_CAPACITY; ++i) {
        const CcCustodyEntry *entry = &state->entries[i];
        if (entry->id == 0) {
            if (!EmptyEntry(entry)) return false;
            continue;
        }
        if (entry->id >= state->next_id || entry->revision == 0 || entry->owner_id == 0 ||
            entry->source_id >= entry->id || entry->holder.id == 0 ||
            entry->holder.kind < CC_CUSTODY_STORE || entry->holder.kind > CC_CUSTODY_CONTAINER_HOLDER ||
            entry->kind < CC_CUSTODY_GOODS ||
            entry->kind > CC_CUSTODY_CONTAINER || entry->condition < 0 ||
            entry->condition > 100 || entry->capacity < 0 ||
            (entry->kind != CC_CUSTODY_CONTAINER && entry->capacity != 0)) return false;
        for (int j = 0; j < i; ++j)
            if (state->entries[j].id == entry->id) return false;
        if (entry->kind == CC_CUSTODY_GOODS) {
            if (entry->good < 0 || entry->good >= rules->good_count || entry->reference_id != 0)
                return false;
        } else if (entry->good != 0) return false;
        bool named = entry->kind == CC_CUSTODY_TREASURE || entry->kind == CC_CUSTODY_DOCUMENT;
        if (named != (entry->reference_id != 0)) return false;
        if (!entry->active) {
            if (entry->quantity != 0) return false;
            continue;
        }
        if (entry->quantity <= 0 ||
            (entry->kind >= CC_CUSTODY_TREASURE && entry->quantity != 1)) return false;
        if (named && (rules->reference_valid == NULL ||
            !rules->reference_valid(rules->context, entry->kind, entry->reference_id))) return false;
        if (entry->kind == CC_CUSTODY_TREASURE) {
            for (int j = 0; j < i; ++j) {
                const CcCustodyEntry *other = &state->entries[j];
                if (other->active && other->kind == CC_CUSTODY_TREASURE &&
                    other->reference_id == entry->reference_id) return false;
            }
        }
        CcCustodyHolder root;
        if (!RootHolder(state, entry->holder, &root) ||
            (entry->kind == CC_CUSTODY_CONTAINER &&
             entry->holder.kind == CC_CUSTODY_CONTAINER_HOLDER)) return false;
        CcCustodyLocation location = {0};
        int64_t capacity = 0, used = 0;
        int count = 0;
        if (!rules->resolve(rules->context, root, &location, &capacity) ||
            !ValidLocation(location) || capacity < 0 ||
            !Load(state, rules, root, true, &used, &count) || used > capacity) return false;
        if (entry->kind == CC_CUSTODY_CONTAINER) {
            CcCustodyHolder contents = {CC_CUSTODY_CONTAINER_HOLDER, entry->id};
            if (!Load(state, rules, contents, false, &used, &count) ||
                used > entry->capacity || count > CC_CUSTODY_MANIFEST_CAPACITY) return false;
        }
    }
    return true;
}

static uint64_t HashWord(uint64_t hash, uint64_t word)
{
    for (int byte = 0; byte < 8; ++byte) {
        hash ^= word & UINT64_C(255);
        hash *= UINT64_C(1099511628211);
        word >>= 8;
    }
    return hash;
}

uint64_t CcCustodyHash(const CcCustodyState *state)
{
    uint64_t hash = HashWord(UINT64_C(14695981039346656037), state->next_id);
    for (int i = 0; i < CC_CUSTODY_CAPACITY; ++i) {
        const CcCustodyEntry *entry = &state->entries[i];
        hash = HashWord(hash, entry->id);
        hash = HashWord(hash, entry->revision);
        hash = HashWord(hash, entry->owner_id);
        hash = HashWord(hash, entry->source_id);
        hash = HashWord(hash, entry->last_event_id);
        hash = HashWord(hash, (uint32_t)entry->holder.kind);
        hash = HashWord(hash, entry->holder.id);
        hash = HashWord(hash, (uint32_t)entry->kind);
        hash = HashWord(hash, entry->reference_id);
        hash = HashWord(hash, (uint64_t)entry->quantity);
        hash = HashWord(hash, (uint32_t)entry->good);
        hash = HashWord(hash, (uint32_t)entry->condition);
        hash = HashWord(hash, (uint32_t)entry->capacity);
        hash = HashWord(hash, entry->active ? 1U : 0U);
    }
    return hash;
}

static int FreeSlot(const CcCustodyState *state)
{
    for (int i = 0; i < CC_CUSTODY_CAPACITY; ++i)
        if (!state->entries[i].active && state->entries[i].quantity == 0) return i;
    return -1;
}

CcCustodyResult CcCustodyPlanTransfer(const CcCustodyState *state,
    const CcCustodyRules *rules, const CcCustodyTransfer *transfer)
{
    if (state == NULL || rules == NULL || rules->resolve == NULL ||
        rules->permit == NULL || transfer == NULL || transfer->event_id == 0 ||
        transfer->actor_id == 0 || transfer->quantity <= 0) return CC_CUSTODY_INVALID;
    const CcCustodyEntry *entry = CcCustodyFind(state, transfer->entry_id);
    if (entry == NULL || entry->revision != transfer->revision ||
        entry->revision == UINT64_MAX) return CC_CUSTODY_STALE;
    if (entry->kind < CC_CUSTODY_GOODS || entry->kind > CC_CUSTODY_CONTAINER ||
        entry->revision == 0 || entry->owner_id == 0 ||
        entry->condition < 0 || entry->condition > 100 ||
        (entry->kind >= CC_CUSTODY_TREASURE && entry->quantity != 1) ||
        ((entry->kind == CC_CUSTODY_TREASURE || entry->kind == CC_CUSTODY_DOCUMENT) &&
         entry->reference_id == 0) ||
        transfer->quantity > entry->quantity ||
        SameHolder(entry->holder, transfer->destination)) return CC_CUSTODY_INVALID;
    bool split = transfer->quantity < entry->quantity;
    if (split && entry->kind != CC_CUSTODY_GOODS && entry->kind != CC_CUSTODY_PURSE)
        return CC_CUSTODY_INVALID;
    CcCustodyHolder source, destination;
    if (!RootHolder(state, entry->holder, &source) ||
        !RootHolder(state, transfer->destination, &destination)) return CC_CUSTODY_INVALID;
    if (entry->kind == CC_CUSTODY_CONTAINER &&
        transfer->destination.kind == CC_CUSTODY_CONTAINER_HOLDER) return CC_CUSTODY_INVALID;
    CcCustodyHolder manifests[2] = {entry->holder, transfer->destination};
    for (int i = 0; i < 2; ++i) {
        if (manifests[i].kind != CC_CUSTODY_CONTAINER_HOLDER) continue;
        const CcCustodyEntry *box = CcCustodyFind(state, manifests[i].id);
        if (box == NULL || box->revision == 0 || box->revision == UINT64_MAX)
            return CC_CUSTODY_STALE;
    }
    if (!rules->permit(rules->context, transfer->actor_id, entry, transfer->destination))
        return CC_CUSTODY_FORBIDDEN;
    CcCustodyLocation from = {0}, to = {0};
    int64_t source_capacity = 0, destination_capacity = 0;
    if (!rules->resolve(rules->context, source, &from, &source_capacity) ||
        !rules->resolve(rules->context, destination, &to, &destination_capacity) ||
        !ValidLocation(from) || !ValidLocation(to) ||
        source_capacity < 0 || destination_capacity < 0) return CC_CUSTODY_INVALID;
    if (from.place_id != to.place_id || from.route_id != to.route_id ||
        from.progress_milli != to.progress_milli) return CC_CUSTODY_REMOTE;
    int64_t moved = EntryLoad(rules, entry, transfer->quantity), used = 0;
    int count = 0;
    if (moved <= 0) return CC_CUSTODY_INVALID;
    if (entry->kind == CC_CUSTODY_CONTAINER) {
        CcCustodyHolder contents = {CC_CUSTODY_CONTAINER_HOLDER, entry->id};
        if (!Load(state, rules, contents, false, &used, &count) || !AddLoad(&moved, used))
            return CC_CUSTODY_INVALID;
    }
    if (transfer->destination.kind == CC_CUSTODY_CONTAINER_HOLDER) {
        const CcCustodyEntry *box = CcCustodyFind(state, transfer->destination.id);
        if (!Load(state, rules, transfer->destination, false, &used, &count) ||
            box == NULL || box->capacity < 0) return CC_CUSTODY_INVALID;
        if (count >= CC_CUSTODY_MANIFEST_CAPACITY || used > box->capacity ||
            moved > box->capacity - used) return CC_CUSTODY_FULL;
    }
    if (!Load(state, rules, destination, true, &used, &count)) return CC_CUSTODY_INVALID;
    if (moved <= 0) return CC_CUSTODY_INVALID;
    int64_t extra = moved;
    if (SameHolder(source, destination)) {
        extra = 0;
        if (split) {
            int64_t original = EntryLoad(rules, entry, entry->quantity);
            int64_t remaining = EntryLoad(rules, entry, entry->quantity - transfer->quantity);
            int64_t combined = moved;
            if (original <= 0 || remaining <= 0 || !AddLoad(&combined, remaining) ||
                combined < original) return CC_CUSTODY_INVALID;
            extra = combined - original;
        }
    }
    if (used > destination_capacity || extra > destination_capacity - used)
        return CC_CUSTODY_FULL;
    if (split && (FreeSlot(state) < 0 || state->next_id == 0 || state->next_id == UINT64_MAX))
        return CC_CUSTODY_FULL;
    if (split) {
        for (int i = 0; i < CC_CUSTODY_CAPACITY; ++i)
            if (state->entries[i].id >= state->next_id) return CC_CUSTODY_INVALID;
    }
    return CC_CUSTODY_READY;
}

CcCustodyResult CcCustodyApplyTransfer(CcCustodyState *state,
    const CcCustodyRules *rules, const CcCustodyTransfer *transfer, uint64_t *result_id)
{
    CcCustodyResult result = CcCustodyPlanTransfer(state, rules, transfer);
    if (result != CC_CUSTODY_READY) return result;
    CcCustodyEntry *entry = &state->entries[CcCustodyFind(state, transfer->entry_id) - state->entries];
    CcCustodyHolder manifests[2] = {entry->holder, transfer->destination};
    for (int i = 0; i < 2; ++i) {
        if (manifests[i].kind != CC_CUSTODY_CONTAINER_HOLDER) continue;
        CcCustodyEntry *box = &state->entries[
            CcCustodyFind(state, manifests[i].id) - state->entries];
        box->revision++;
        box->last_event_id = transfer->event_id;
    }
    entry->revision++;
    entry->last_event_id = transfer->event_id;
    CcCustodyEntry *moved = entry;
    if (transfer->quantity < entry->quantity) {
        moved = &state->entries[FreeSlot(state)];
        *moved = *entry;
        moved->id = state->next_id++;
        moved->revision = 1;
        moved->source_id = entry->id;
        moved->quantity = transfer->quantity;
        entry->quantity -= transfer->quantity;
    }
    moved->holder = transfer->destination;
    if (result_id != NULL) *result_id = moved->id;
    return CC_CUSTODY_READY;
}
