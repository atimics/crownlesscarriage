#include "sim/cc_custody.h"
#include "test_support.h"
#include <string.h>

static CcCustodyState state, before;
static int carrier_place = 1;
static int64_t carrier_capacity = 12;
static int64_t store_capacity = 10000;

static bool Resolve(const void *context, CcCustodyHolder holder,
                    CcCustodyLocation *location, int64_t *capacity)
{
    (void)context;
    if (holder.id == 0 || holder.id > 40) return false;
    *location = (CcCustodyLocation){.place_id = holder.id == 20 ?
        (uint64_t)carrier_place : holder.id >= 30 ? 2U : 1U};
    *capacity = holder.id == 20 ? carrier_capacity : store_capacity;
    return true;
}

static bool Permit(const void *context, uint64_t actor, const CcCustodyEntry *entry,
                   CcCustodyHolder destination)
{
    (void)context; (void)entry; (void)destination;
    return actor == 7;
}

static bool ReferenceValid(const void *context, CcCustodyKind kind, uint64_t id)
{
    (void)context;
    return (kind == CC_CUSTODY_DOCUMENT && id == 77) ||
        (kind == CC_CUSTODY_TREASURE && id == 88);
}

static const CcCustodyRules rules = {.good_count = 14,
    .reference_valid = ReferenceValid, .resolve = Resolve, .permit = Permit};

static void Prepare(void)
{
    CcCustodyInit(&state);
    carrier_place = 1;
    carrier_capacity = 12;
    store_capacity = 10000;
    state.entries[0] = (CcCustodyEntry){.id = 1, .revision = 1, .owner_id = 7,
        .holder = {CC_CUSTODY_STORE, 10}, .kind = CC_CUSTODY_GOODS,
        .quantity = 14, .good = 3, .condition = 90, .active = true};
    state.entries[1] = (CcCustodyEntry){.id = 2, .revision = 1, .owner_id = 7,
        .holder = {CC_CUSTODY_STORE, 10}, .kind = CC_CUSTODY_CONTAINER,
        .quantity = 1, .capacity = 10, .condition = 100, .active = true};
    state.next_id = 3;
}

static CcCustodyTransfer Request(uint64_t id, int64_t quantity, CcCustodyHolder holder)
{
    const CcCustodyEntry *entry = CcCustodyFind(&state, id);
    CC_CHECK(entry != NULL);
    return (CcCustodyTransfer){.entry_id = id, .revision = entry->revision,
        .actor_id = 7, .event_id = 100 + entry->revision,
        .destination = holder, .quantity = quantity};
}

static void Reject(CcCustodyTransfer request, CcCustodyResult expected)
{
    before = state;
    uint64_t result_id = 999;
    CC_CHECK(CcCustodyApplyTransfer(&state, &rules, &request, &result_id) == expected);
    CC_CHECK(memcmp(&before, &state, sizeof(state)) == 0);
    CC_CHECK(result_id == 999);
}

static void Journey(void)
{
    Prepare();
    CcCustodyTransfer pack = Request(1, 5, (CcCustodyHolder){CC_CUSTODY_CONTAINER_HOLDER, 2});
    CcCustodyTransfer old_load = Request(2, 1, (CcCustodyHolder){CC_CUSTODY_CARRIER, 20});
    before = state;
    CC_CHECK(CcCustodyPlanTransfer(&state, &rules, &pack) == CC_CUSTODY_READY);
    CC_CHECK(memcmp(&before, &state, sizeof(state)) == 0);
    uint64_t split = 0;
    CC_CHECK(CcCustodyApplyTransfer(&state, &rules, &pack, &split) == CC_CUSTODY_READY);
    CC_CHECK(split == 3 && state.next_id == 4);
    CC_CHECK(CcCustodyFind(&state, 1)->quantity == 9);
    CC_CHECK(CcCustodyFind(&state, split)->quantity == 5);
    CC_CHECK(CcCustodyFind(&state, split)->source_id == 1);
    CC_CHECK(CcCustodyFind(&state, split)->condition == 90);
    Reject(pack, CC_CUSTODY_STALE);
    Reject(old_load, CC_CUSTODY_STALE);
    CcCustodyTransfer load = Request(2, 1, (CcCustodyHolder){CC_CUSTODY_CARRIER, 20});
    carrier_capacity = 5;
    Reject(load, CC_CUSTODY_FULL);
    carrier_capacity = 6;
    CC_CHECK(CcCustodyApplyTransfer(&state, &rules, &load, NULL) == CC_CUSTODY_READY);
    CcCustodyTransfer unload = Request(3, 5, (CcCustodyHolder){CC_CUSTODY_STORE, 30});
    Reject(unload, CC_CUSTODY_REMOTE);
    carrier_place = 2;
    CcCustodyTransfer intercept = Request(2, 1, (CcCustodyHolder){CC_CUSTODY_CAPTOR, 40});
    CC_CHECK(CcCustodyApplyTransfer(&state, &rules, &intercept, NULL) == CC_CUSTODY_READY);
    CC_CHECK(CcCustodyFind(&state, 3)->holder.id == 2);
    CC_CHECK(CcCustodyFind(&state, 2)->owner_id == 7);
    load = Request(2, 1, (CcCustodyHolder){CC_CUSTODY_CARRIER, 20});
    CC_CHECK(CcCustodyApplyTransfer(&state, &rules, &load, NULL) == CC_CUSTODY_READY);
    CC_CHECK(CcCustodyApplyTransfer(&state, &rules, &unload, NULL) == CC_CUSTODY_READY);
    CC_CHECK(CcCustodyFind(&state, 3)->holder.id == 30);
    CC_CHECK(CcCustodyFind(&state, 1)->quantity + CcCustodyFind(&state, 3)->quantity == 14);
    CC_CHECK(CcCustodyFind(&state, 3)->last_event_id == unload.event_id);
    CC_CHECK(CcCustodyValidate(&state, &rules));
}

static void Gates(void)
{
    Prepare();
    CcCustodyTransfer move = Request(1, 5, (CcCustodyHolder){CC_CUSTODY_CARRIER, 20});
    move.actor_id = 8;
    Reject(move, CC_CUSTODY_FORBIDDEN);
    move.actor_id = 7;
    move.quantity = 0;
    Reject(move, CC_CUSTODY_INVALID);
    move.quantity = 15;
    Reject(move, CC_CUSTODY_INVALID);
    move = Request(2, 1, (CcCustodyHolder){CC_CUSTODY_CONTAINER_HOLDER, 2});
    Reject(move, CC_CUSTODY_INVALID);
    move = Request(1, 11, (CcCustodyHolder){CC_CUSTODY_CONTAINER_HOLDER, 2});
    Reject(move, CC_CUSTODY_FULL);
    move = Request(1, 5, (CcCustodyHolder){CC_CUSTODY_CHARACTER, 999});
    Reject(move, CC_CUSTODY_INVALID);
    move.destination = (CcCustodyHolder){CC_CUSTODY_CARRIER, 20};
    for (int i = 2; i < CC_CUSTODY_CAPACITY; ++i) {
        state.entries[i] = state.entries[0];
        state.entries[i].id = (uint64_t)i + 1;
    }
    state.next_id = CC_CUSTODY_CAPACITY + 1;
    Reject(move, CC_CUSTODY_FULL);
    state.entries[2].active = false; /* A retired entry with contents remains reserved. */
    Reject(move, CC_CUSTODY_FULL);
    state.entries[2].quantity = 0;
    uint64_t fresh = 0;
    CC_CHECK(CcCustodyApplyTransfer(&state, &rules, &move, &fresh) == CC_CUSTODY_READY);
    CC_CHECK(fresh == CC_CUSTODY_CAPACITY + 1);
    Prepare();
    state.entries[0].revision = UINT64_MAX;
    move = Request(1, 5, (CcCustodyHolder){CC_CUSTODY_CARRIER, 20});
    Reject(move, CC_CUSTODY_STALE);
}

static void PursesAndCopies(void)
{
    Prepare();
    state.entries[0].kind = CC_CUSTODY_PURSE;
    state.entries[0].good = 0;
    state.entries[0].quantity = INT64_MAX;
    CcCustodyTransfer split = Request(1, 8, (CcCustodyHolder){CC_CUSTODY_CONTAINER_HOLDER, 2});
    uint64_t purse = 0;
    CC_CHECK(CcCustodyApplyTransfer(&state, &rules, &split, &purse) == CC_CUSTODY_READY);
    CC_CHECK(CcCustodyFind(&state, 1)->quantity == INT64_MAX - 8);
    CC_CHECK(CcCustodyFind(&state, purse)->quantity == 8);
    Reject(split, CC_CUSTODY_STALE);
    Prepare();
    state.entries[0].kind = CC_CUSTODY_DOCUMENT;
    state.entries[0].good = 0;
    state.entries[0].reference_id = 77;
    state.entries[0].quantity = 1;
    state.entries[2] = state.entries[0];
    state.entries[2].id = 3;
    state.next_id = 4;
    for (uint64_t id = 1; id <= 3; id += 2) {
        CcCustodyTransfer copy = Request(id, 1, (CcCustodyHolder){CC_CUSTODY_CONTAINER_HOLDER, 2});
        CC_CHECK(CcCustodyApplyTransfer(&state, &rules, &copy, NULL) == CC_CUSTODY_READY);
        CC_CHECK(CcCustodyFind(&state, id)->reference_id == 77);
        CC_CHECK(CcCustodyFind(&state, id)->id == id);
    }
    Prepare();
    CcCustodyTransfer plan = Request(1, 5, (CcCustodyHolder){CC_CUSTODY_CARRIER, 20});
    CC_CHECK(CcCustodyPlanTransfer(&state, &rules, &plan) == CC_CUSTODY_READY);
    carrier_place = 2;
    Reject(plan, CC_CUSTODY_REMOTE);
}

static void ValidationAndHash(void)
{
    Prepare();
    CC_CHECK(CcCustodyValidate(&state, &rules));
    uint64_t original = CcCustodyHash(&state);
#define HASH_FIELD(field, value) do { \
    before = state; before.entries[0].field = (value); \
    CC_CHECK(CcCustodyHash(&before) != original); \
} while (0)
    HASH_FIELD(id, 9);
    HASH_FIELD(revision, 2);
    HASH_FIELD(owner_id, 9);
    HASH_FIELD(source_id, 9);
    HASH_FIELD(last_event_id, 9);
    HASH_FIELD(holder.kind, CC_CUSTODY_SITE);
    HASH_FIELD(holder.id, 20);
    HASH_FIELD(kind, CC_CUSTODY_PURSE);
    HASH_FIELD(reference_id, 88);
    HASH_FIELD(quantity, 13);
    HASH_FIELD(good, 4);
    HASH_FIELD(condition, 89);
    HASH_FIELD(capacity, 1);
    HASH_FIELD(active, false);
#undef HASH_FIELD
    before = state; before.next_id++;
    CC_CHECK(CcCustodyHash(&before) != original);
    before = state; before.entries[CC_CUSTODY_CAPACITY - 1].last_event_id = 1;
    CC_CHECK(CcCustodyHash(&before) != original);
    CC_CHECK(!CcCustodyValidate(&before, &rules));
#define INVALID_FIELD(field, value) do { \
    before = state; before.entries[0].field = (value); \
    CC_CHECK(!CcCustodyValidate(&before, &rules)); \
} while (0)
    INVALID_FIELD(id, 2);
    INVALID_FIELD(revision, 0);
    INVALID_FIELD(owner_id, 0);
    INVALID_FIELD(source_id, 1);
    INVALID_FIELD(holder.kind, (CcCustodyHolderKind)99);
    INVALID_FIELD(holder.id, 999);
    INVALID_FIELD(kind, (CcCustodyKind)99);
    INVALID_FIELD(reference_id, 88);
    INVALID_FIELD(quantity, 0);
    INVALID_FIELD(good, 14);
    INVALID_FIELD(condition, 101);
    INVALID_FIELD(capacity, 1);
    INVALID_FIELD(active, false);
#undef INVALID_FIELD
    before = state; before.next_id = 2;
    CC_CHECK(!CcCustodyValidate(&before, &rules));
    state.entries[0].holder = (CcCustodyHolder){CC_CUSTODY_CONTAINER_HOLDER, 2};
    CC_CHECK(!CcCustodyValidate(&state, &rules)); /* Fourteen units exceed ten. */
    state.entries[0].quantity = 10;
    CC_CHECK(CcCustodyValidate(&state, &rules));
    state.entries[1].holder = (CcCustodyHolder){CC_CUSTODY_CONTAINER_HOLDER, 2};
    CC_CHECK(!CcCustodyValidate(&state, &rules));
    Prepare();
    state.entries[1].capacity = 100;
    for (int i = 2; i <= 18; ++i) {
        state.entries[i] = state.entries[0];
        state.entries[i].id = (uint64_t)i + 1;
        state.entries[i].quantity = 1;
        state.entries[i].holder = (CcCustodyHolder){CC_CUSTODY_CONTAINER_HOLDER, 2};
    }
    state.next_id = 20;
    CC_CHECK(!CcCustodyValidate(&state, &rules));
    state.entries[18].active = false;
    state.entries[18].quantity = 0;
    CC_CHECK(CcCustodyValidate(&state, &rules));
    Prepare();
    state.entries[0].kind = CC_CUSTODY_TREASURE;
    state.entries[0].good = 0;
    state.entries[0].quantity = 1;
    state.entries[0].reference_id = 88;
    CC_CHECK(CcCustodyValidate(&state, &rules));
    state.entries[2] = state.entries[0];
    state.entries[2].id = 3;
    state.next_id = 4;
    CC_CHECK(!CcCustodyValidate(&state, &rules));
    state.entries[0].kind = state.entries[2].kind = CC_CUSTODY_DOCUMENT;
    state.entries[0].reference_id = state.entries[2].reference_id = 77;
    CC_CHECK(CcCustodyValidate(&state, &rules));
    state.entries[2].reference_id = 78;
    CC_CHECK(!CcCustodyValidate(&state, &rules));
    state.entries[2].active = false;
    state.entries[2].quantity = 0;
    state.entries[2].holder.id = 999; /* Historical lifetime may have ended. */
    CC_CHECK(CcCustodyValidate(&state, &rules));
}

static int64_t RoundedLoad(const void *context, const CcCustodyEntry *entry, int64_t quantity)
{
    (void)context;
    return entry->kind == CC_CUSTODY_GOODS ? quantity / 10 + (quantity % 10 != 0) : 1;
}

static void FreightRounding(void)
{
    Prepare();
    CcCustodyRules freight = rules;
    freight.load = RoundedLoad;
    store_capacity = 3; /* Two slots of bulk goods and one empty container. */
    CC_CHECK(CcCustodyValidate(&state, &freight));
    CcCustodyTransfer pack = Request(1, 1, (CcCustodyHolder){CC_CUSTODY_CONTAINER_HOLDER, 2});
    before = state;
    CC_CHECK(CcCustodyApplyTransfer(&state, &freight, &pack, NULL) == CC_CUSTODY_FULL);
    CC_CHECK(memcmp(&state, &before, sizeof(state)) == 0);
    store_capacity = 4;
    CC_CHECK(CcCustodyApplyTransfer(&state, &freight, &pack, NULL) == CC_CUSTODY_READY);
    CC_CHECK(CcCustodyValidate(&state, &freight));
    CC_CHECK(CcCustodyFind(&state, 1)->quantity == 13);
    CC_CHECK(CcCustodyFind(&state, 3)->quantity == 1);
}

int main(void)
{
    Journey();
    Gates();
    PursesAndCopies();
    ValidationAndHash();
    FreightRounding();
    return 0;
}
