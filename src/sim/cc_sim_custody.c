#include "sim/cc_sim_custody.h"
#include "sim/cc_mine.h"
#include <limits.h>
#include <stdio.h>
#include "sim/cc_goods_internal.h"

static void SetCustodyError(char *error, size_t capacity, const char *text)
{
    if (error != NULL && capacity > 0U) (void)snprintf(error, capacity, "%s", text);
}

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

static bool NearMinePoint(const CcMineVisit *mine, int32_t x, int32_t y)
{
    int32_t dx=mine->x-x, dy=mine->y-y;
    if (dx < 0) dx=-dx;
    if (dy < 0) dy=-dy;
    return dx+dy <= 1;
}

static int64_t LegacyMinePackUsed(const CcSim *sim)
{
    int64_t total=0;
    for (int32_t good=0;good<CC_GOOD_COUNT;++good) total+=sim->mine.pack[good];
    return total;
}

int32_t CcSimCustodyCarrierLoad(const CcSim *sim, CcId carrier_id)
{
    if (sim == NULL || sim->schema_version < 99U) return 0;
    int64_t total = 0;
    for (int i = 0; i < CcCustodyEffectiveCapacity(&sim->custody); ++i) {
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
    if (holder.kind == CC_CUSTODY_PLAYER && holder.id == sim->player.id) {
        *location=(CcCustodyLocation){.place_id=sim->mine.phase != CC_MINE_NONE ?
            sim->player.id : sim->player.location_id};
        *capacity=INT64_MAX;
        return true;
    }
    if (holder.kind == CC_CUSTODY_SITE &&
        (holder.id == sim->mine.source_id || holder.id == sim->mine.cache_id)) {
        *location=(CcCustodyLocation){.place_id=holder.id};
        *capacity=holder.id == sim->mine.cache_id ? CC_MINE_CACHE_CAPACITY : CC_SIM_MAX_UNITS;
        return true;
    }
    if (holder.kind == CC_CUSTODY_MINE_PACK &&
        holder.id == sim->player.id && sim->mine.phase != CC_MINE_NONE) {
        int64_t legacy=LegacyMinePackUsed(sim);
        if (legacy > CC_MINE_PACK_CAPACITY) return false;
        CcId place=sim->player.id;
        if (sim->mine.phase == CC_MINE_LEVEL &&
            NearMinePoint(&sim->mine,sim->mine.source_x,sim->mine.source_y))
            place=sim->mine.source_id;
        else if (sim->mine.phase == CC_MINE_LEVEL &&
            NearMinePoint(&sim->mine,sim->mine.cache_x,sim->mine.cache_y))
            place=sim->mine.cache_id;
        *location=(CcCustodyLocation){.place_id=place};
        *capacity=CC_MINE_PACK_CAPACITY-legacy;
        return true;
    }
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
    int32_t expected_capacity=sim->schema_version < 103U ?
        CC_CUSTODY_LEGACY_CAPACITY : CC_CUSTODY_CAPACITY;
    if (CcCustodyEffectiveCapacity(&sim->custody) != expected_capacity) return false;
    const CcCustodyRules rules = {.context = sim,
        .good_count = CC_GOOD_COUNT, .load = StoredCustodyLoad, .resolve = ResolveStoredCustody};
    if (!CcCustodyValidate(&sim->custody, &rules)) return false;
    for (int i = 0; i < CcCustodyEffectiveCapacity(&sim->custody); ++i) {
        const CcCustodyEntry *entry = &sim->custody.entries[i];
        if (!entry->active) continue;
        if (entry->quantity > (entry->kind == CC_CUSTODY_PURSE ?
            CC_SIM_MAX_MONEY : CC_SIM_MAX_UNITS)) return false;
        bool mine_load=entry->owner_id == sim->goblins.id && entry->kind == CC_CUSTODY_GOODS &&
            ((entry->holder.kind == CC_CUSTODY_SITE &&
              (entry->holder.id == sim->mine.source_id || entry->holder.id == sim->mine.cache_id)) ||
             (entry->holder.kind == CC_CUSTODY_MINE_PACK && entry->holder.id == sim->player.id) ||
             (sim->schema_version >= 106U && CcSimMineEntryTracked(sim,entry) &&
              entry->holder.kind == CC_CUSTODY_PLAYER && entry->holder.id == sim->player.id));
        if (!mine_load && entry->owner_id != sim->player.id &&
            CcSimSettlement(sim, entry->owner_id) == NULL &&
            /* Schema 102: a fallen person's purse is owned by the dead,
               recorded as a historic character (#288/#406). */
            !(sim->schema_version >= 102U &&
              CcSimHistoricCharacter(sim, entry->owner_id) != NULL)) return false;
    }
    return true;
}

static bool SameMineHolder(CcCustodyHolder holder, CcCustodyHolderKind kind, CcId id)
{
    return holder.kind == kind && holder.id == id;
}

static bool PermitMineTransfer(const void *context, uint64_t actor,
                               const CcCustodyEntry *entry, CcCustodyHolder destination)
{
    const CcSim *sim=context;
    if (actor != sim->player.id || entry->owner_id != sim->goblins.id ||
        entry->kind != CC_CUSTODY_GOODS) return false;
    CcCustodyHolder pack={CC_CUSTODY_MINE_PACK,sim->player.id};
    CcCustodyHolder player={CC_CUSTODY_PLAYER,sim->player.id};
    CcCustodyHolder source={CC_CUSTODY_SITE,sim->mine.source_id};
    CcCustodyHolder cache={CC_CUSTODY_SITE,sim->mine.cache_id};
    return (sim->mine.source_released &&
            SameMineHolder(entry->holder,source.kind,source.id) &&
            SameMineHolder(destination,pack.kind,pack.id)) ||
        (SameMineHolder(entry->holder,pack.kind,pack.id) &&
         SameMineHolder(destination,cache.kind,cache.id)) ||
        (SameMineHolder(entry->holder,cache.kind,cache.id) &&
         SameMineHolder(destination,pack.kind,pack.id)) ||
        (sim->schema_version >= 106U && CcSimMineEntryTracked(sim,entry) &&
         ((SameMineHolder(entry->holder,player.kind,player.id) &&
           SameMineHolder(destination,pack.kind,pack.id)) ||
          (SameMineHolder(entry->holder,pack.kind,pack.id) &&
           SameMineHolder(destination,player.kind,player.id))));
}

static int64_t MineCustodyLoad(const void *context, const CcCustodyEntry *entry,
                               int64_t quantity)
{
    (void)context;
    return entry->kind == CC_CUSTODY_GOODS && quantity > 0 ? quantity : -1;
}

CcCustodyResult CcSimTransferMineGoods(CcSim *sim, CcCustodyHolder source,
    CcCustodyHolder destination, CcGood good, int32_t quantity, uint64_t event_id)
{
    if (sim == NULL || sim->schema_version < 103U || good < 0 ||
        good >= CC_GOOD_COUNT || quantity <= 0 || event_id == 0 ||
        !CcSimStoredCustodyValid(sim)) return CC_CUSTODY_INVALID;
    if (destination.kind == CC_CUSTODY_MINE_PACK &&
        destination.id == sim->player.id &&
        CcMinePackUsed(sim) > CC_MINE_PACK_CAPACITY-quantity)
        return CC_CUSTODY_FULL;
    CcCustodyState original=sim->custody;
    CcCustodyState candidate=original;
    const CcCustodyRules rules={.context=sim,.good_count=CC_GOOD_COUNT,
        .load=MineCustodyLoad,.resolve=ResolveStoredCustody,.permit=PermitMineTransfer};
    int64_t available=0;
    for (int32_t i=0;i<CcCustodyEffectiveCapacity(&sim->custody);++i) {
        const CcCustodyEntry *entry=&candidate.entries[i];
        if (entry->active && entry->kind == CC_CUSTODY_GOODS &&
            entry->owner_id == sim->goblins.id && entry->good == (int32_t)good &&
            SameMineHolder(entry->holder,source.kind,source.id) &&
            (source.kind != CC_CUSTODY_PLAYER || CcSimMineEntryTracked(sim,entry)))
            available+=entry->quantity;
    }
    if (available < quantity) return CC_CUSTODY_INVALID;
    int32_t remaining=quantity;
    while (remaining > 0) {
        const CcCustodyEntry *entry=NULL;
        for (int32_t i=0;i<CcCustodyEffectiveCapacity(&sim->custody);++i) {
            const CcCustodyEntry *candidate_entry=&candidate.entries[i];
            if (candidate_entry->active && candidate_entry->kind == CC_CUSTODY_GOODS &&
                candidate_entry->owner_id == sim->goblins.id &&
                candidate_entry->good == (int32_t)good &&
                SameMineHolder(candidate_entry->holder,source.kind,source.id) &&
                (source.kind != CC_CUSTODY_PLAYER ||
                 CcSimMineEntryTracked(sim,candidate_entry))) {
                entry=candidate_entry;
                break;
            }
        }
        if (entry == NULL) return CC_CUSTODY_INVALID;
        int64_t move=entry->quantity < remaining ? entry->quantity : remaining;
        CcCustodyTransfer transfer={.entry_id=entry->id,.revision=entry->revision,
            .actor_id=sim->player.id,.event_id=event_id,.destination=destination,.quantity=move};
        CcCustodyResult result=CcCustodyApplyTransfer(&candidate,&rules,&transfer,NULL);
        if (result != CC_CUSTODY_READY) return result;
        remaining-=(int32_t)move;
    }
    sim->custody=candidate;
    if (CcSimStoredCustodyValid(sim)) return CC_CUSTODY_READY;
    sim->custody=original;
    return CC_CUSTODY_INVALID;
}

CcCustodyResult CcSimRepackMineGoods(CcSim *sim, CcGood good,
    int32_t quantity, uint64_t event_id, int32_t *tracked_quantity)
{
    if (tracked_quantity != NULL) *tracked_quantity=0;
    if (sim == NULL || sim->schema_version < 106U || good < 0 ||
        good >= CC_GOOD_COUNT || quantity <= 0 || event_id == 0U)
        return CC_CUSTODY_INVALID;
    int32_t tracked=0;
    for (int32_t i=0;i<CcCustodyEffectiveCapacity(&sim->custody);++i) {
        const CcCustodyEntry *entry=&sim->custody.entries[i];
        if (!entry->active || entry->kind != CC_CUSTODY_GOODS ||
            entry->good != (int32_t)good || entry->owner_id != sim->goblins.id ||
            entry->holder.kind != CC_CUSTODY_PLAYER ||
            entry->holder.id != sim->player.id || !CcSimMineEntryTracked(sim,entry))
            continue;
        int32_t available=entry->quantity > INT32_MAX ? INT32_MAX :
            (int32_t)entry->quantity;
        if (available >= quantity-tracked) { tracked=quantity; break; }
        tracked+=available;
    }
    if (tracked == 0) return CC_CUSTODY_READY;
    CcCustodyResult result=CcSimTransferMineGoods(sim,
        (CcCustodyHolder){CC_CUSTODY_PLAYER,sim->player.id},
        (CcCustodyHolder){CC_CUSTODY_MINE_PACK,sim->player.id},
        good,tracked,event_id);
    if (result == CC_CUSTODY_READY && tracked_quantity != NULL)
        *tracked_quantity=tracked;
    return result;
}

bool CcSimMineEntryTracked(const CcSim *sim, const CcCustodyEntry *entry)
{
    if (sim == NULL || entry == NULL || entry->kind != CC_CUSTODY_GOODS) return false;
    CcId expected=entry->good == CC_GOOD_BREAD ? sim->mine.bread_source_entry_id :
        entry->good == CC_GOOD_IRON ? sim->mine.iron_source_entry_id :
        entry->good == CC_GOOD_GOLD ? sim->mine.gold_source_entry_id :
        entry->good == CC_GOOD_GEMS ? sim->mine.gems_source_entry_id : 0U;
    if (expected == 0U) return false;
    const CcCustodyEntry *root=entry;
    int32_t remaining=CcCustodyEffectiveCapacity(&sim->custody);
    while (root->source_id != 0U && remaining-- > 0) {
        const CcCustodyEntry *parent=NULL;
        for (int32_t i=0;i<CcCustodyEffectiveCapacity(&sim->custody);++i)
            if (sim->custody.entries[i].id == root->source_id) {
                parent=&sim->custody.entries[i];
                break;
            }
        if (parent == NULL) return false;
        root=parent;
    }
    return root->source_id == 0U && root->id == expected;
}

static bool PermitMineSale(const void *context, uint64_t actor,
                           const CcCustodyEntry *entry, CcCustodyHolder destination)
{
    const CcSim *sim=context;
    return actor == sim->player.id && entry->owner_id == sim->goblins.id &&
        CcSimMineEntryTracked(sim,entry) &&
        entry->kind == CC_CUSTODY_GOODS &&
        entry->holder.kind == CC_CUSTODY_PLAYER &&
        entry->holder.id == sim->player.id &&
        destination.kind == CC_CUSTODY_STORE &&
        CcSimSettlement(sim,destination.id) != NULL;
}

static CcCustodyResult MineSaleCandidate(const CcSim *sim, CcId town_id,
    CcGood good, int32_t quantity, CcId event_id, CcCustodyState *result,
    int32_t *tracked_quantity)
{
    if (sim == NULL || sim->schema_version < 106U ||
        CcSimSettlement(sim,town_id) == NULL || sim->player.location_id != town_id ||
        good < 0 || good >= CC_GOOD_COUNT || quantity <= 0 || event_id == 0U)
        return CC_CUSTODY_INVALID;
    CcCustodyState candidate=sim->custody;
    const CcCustodyRules rules={.context=sim,.good_count=CC_GOOD_COUNT,
        .load=StoredCustodyLoad,.resolve=ResolveStoredCustody,.permit=PermitMineSale};
    int32_t remaining=quantity;
    int32_t tracked=0;
    for (int32_t i=0;i<CcCustodyEffectiveCapacity(&candidate) && remaining>0;++i) {
        CcCustodyEntry *entry=&candidate.entries[i];
        if (!entry->active || entry->kind != CC_CUSTODY_GOODS ||
            entry->good != (int32_t)good || entry->owner_id != sim->goblins.id ||
            !CcSimMineEntryTracked(sim,entry) ||
            entry->holder.kind != CC_CUSTODY_PLAYER ||
            entry->holder.id != sim->player.id) continue;
        int32_t moved=entry->quantity < remaining ? (int32_t)entry->quantity : remaining;
        CcCustodyTransfer transfer={.entry_id=entry->id,.revision=entry->revision,
            .actor_id=sim->player.id,.event_id=event_id,
            .destination={CC_CUSTODY_STORE,town_id},.quantity=moved};
        uint64_t moved_id=0U;
        CcCustodyResult applied=CcCustodyApplyTransfer(&candidate,&rules,&transfer,&moved_id);
        if (applied != CC_CUSTODY_READY) return applied;
        CcCustodyEntry *sold=(CcCustodyEntry *)CcCustodyFind(&candidate,moved_id);
        if (sold == NULL) return CC_CUSTODY_INVALID;
        sold->owner_id=town_id;
        sold->quantity=0;
        sold->active=false;
        sold->revision+=1;
        sold->last_event_id=event_id;
        tracked+=moved;
        remaining-=moved;
    }
    /* Ordinary cargo can make up the rest of a larger sale. */
    if (!CcCustodyValidate(&candidate,&(CcCustodyRules){.context=sim,
        .good_count=CC_GOOD_COUNT,.load=StoredCustodyLoad,.resolve=ResolveStoredCustody}))
        return CC_CUSTODY_INVALID;
    if (result != NULL) *result=candidate;
    if (tracked_quantity != NULL) *tracked_quantity=tracked;
    return CC_CUSTODY_READY;
}

CcCustodyResult CcSimPlanMineSale(const CcSim *sim, CcId town_id,
    CcGood good, int32_t quantity)
{
    return MineSaleCandidate(sim,town_id,good,quantity,1U,NULL,NULL);
}

CcCustodyResult CcSimApplyMineSale(CcSim *sim, CcId town_id,
    CcGood good, int32_t quantity, CcId event_id, int32_t *tracked_quantity)
{
    CcCustodyState candidate;
    int32_t tracked=0;
    CcCustodyResult result=MineSaleCandidate(
        sim,town_id,good,quantity,event_id,&candidate,&tracked);
    if (result == CC_CUSTODY_READY) sim->custody=candidate;
    if (tracked_quantity != NULL) *tracked_quantity=tracked;
    return result;
}

void CcSimReconcileMineCarriedGoods(CcSim *sim,
    const int32_t previous_cargo[CC_GOOD_COUNT], CcId event_id)
{
    if (sim == NULL || previous_cargo == NULL || sim->schema_version < 106U)
        return;
    for (int32_t good=0;good<CC_GOOD_COUNT;++good) {
        int32_t removed=previous_cargo[good]-sim->player.cargo[good];
        if (removed <= 0) continue;
        for (int32_t i=0;i<CcCustodyEffectiveCapacity(&sim->custody) && removed>0;++i) {
            CcCustodyEntry *entry=&sim->custody.entries[i];
            if (!entry->active || entry->kind != CC_CUSTODY_GOODS ||
                entry->good != good || entry->holder.kind != CC_CUSTODY_PLAYER ||
                entry->holder.id != sim->player.id || !CcSimMineEntryTracked(sim,entry))
                continue;
            int32_t spent=entry->quantity < removed ? (int32_t)entry->quantity : removed;
            entry->quantity-=spent;
            entry->revision+=1;
            if (event_id != 0U) entry->last_event_id=event_id;
            if (entry->quantity == 0) entry->active=false;
            removed-=spent;
        }
    }
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
    for (int i = 0; i < CcCustodyEffectiveCapacity(&sim->custody); ++i) {
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
    for (int i = 0; i < CcCustodyEffectiveCapacity(&sim->custody); ++i) {
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
    if (town == NULL || CcSettlementIsAbandoned(town) || town->fire_damage >= 100 ||
        work->storage_id != town->id || work->location_id != town->id ||
        work->stock != town->stock) return CC_CUSTODY_INVALID;
    if (next_id != sim->custody.next_id) return CC_CUSTODY_STALE;
    if (next_id == UINT64_MAX) return CC_CUSTODY_FULL;
    int slot = -1;
    for (int i = 0; i < CcCustodyEffectiveCapacity(&sim->custody); ++i) {
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
    if (town == NULL || CcSettlementIsAbandoned(town) || town->fire_damage >= 100 ||
        work->storage_id != town->id || work->location_id != town->id ||
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

bool CcSimIsBodyPurse(const CcSim *sim, const CcCustodyEntry *entry)
{
    if (sim == NULL || entry == NULL || !entry->active ||
        entry->kind != CC_CUSTODY_PURSE || entry->holder.kind != CC_CUSTODY_STORE)
        return false;
    if (entry->owner_id == sim->player.id ||
        CcSimSettlement(sim, entry->owner_id) != NULL ||
        CcSimCharacter(sim, entry->owner_id) != NULL) return false;
    return true;
}

bool CcSimLeaveBodyPurse(CcSim *sim, CcId person_id, CcId place_id,
    CcMoney coins, CcId death_event_id)
{
    if (sim == NULL || sim->schema_version < 102U || coins <= 0 ||
        CcSimSettlement(sim, place_id) == NULL ||
        CcSimCharacter(sim, person_id) == NULL ||
        sim->custody.next_id == UINT64_MAX) return false;
    int slot = -1;
    for (int i = 0; i < CcCustodyEffectiveCapacity(&sim->custody); ++i) {
        if (!sim->custody.entries[i].active && sim->custody.entries[i].quantity == 0) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return false;
    sim->custody.entries[slot] = (CcCustodyEntry){
        .id = sim->custody.next_id, .revision = 1,
        .owner_id = person_id, .holder = {CC_CUSTODY_STORE, place_id},
        .kind = CC_CUSTODY_PURSE, .quantity = coins, .good = 0,
        .condition = 100, .capacity = 0, .active = true,
        .last_event_id = death_event_id};
    sim->custody.next_id++;
    if (!CcSimStoredCustodyValid(sim)) return false;
    return true;
}

bool CcSimClaimBodyPurse(CcSim *sim, CcId entry_id, char *error, size_t error_capacity)
{
    if (sim == NULL || sim->schema_version < 102U) {
        SetCustodyError(error, error_capacity, "No purse lies here.");
        return false;
    }
    CcCustodyEntry *entry = NULL;
    for (int i = 0; i < CcCustodyEffectiveCapacity(&sim->custody); ++i) {
        if (sim->custody.entries[i].active && sim->custody.entries[i].id == entry_id) {
            entry = &sim->custody.entries[i];
            break;
        }
    }
    if (!CcSimIsBodyPurse(sim, entry)) {
        SetCustodyError(error, error_capacity, "That is not an unclaimed fallen purse.");
        return false;
    }
    if (entry->holder.id != sim->player.location_id) {
        SetCustodyError(error, error_capacity,
                 "Reach the place where that purse lies before lifting it.");
        return false;
    }
    CcMoney coins = entry->quantity;
    if (coins > CC_SIM_MAX_MONEY - sim->player.coins) {
        SetCustodyError(error, error_capacity, "The company cannot carry that much coin.");
        return false;
    }
    sim->player.coins += coins;
    entry->quantity = 0;
    entry->active = false;
    entry->revision++;
    if (!CcSimStoredCustodyValid(sim)) {
        /* Restore rather than leave an invalid half-claim. */
        sim->player.coins -= coins;
        entry->quantity = coins;
        entry->active = true;
        entry->revision--;
        SetCustodyError(error, error_capacity, "The claim did not hold.");
        return false;
    }
    SetCustodyError(error, error_capacity, "");
    return true;
}
