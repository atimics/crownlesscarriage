#include "sim/cc_wants.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static CcCharacter *Person(CcSim *sim, CcId id)
{
    for (int i = 0; i < sim->character_count; ++i)
        if (sim->characters[i].id == id) return &sim->characters[i];
    return NULL;
}
static bool Alive(const CcSim *sim, const CcCharacter *p)
{
    return p != NULL && p->death_day > sim->current_day;
}
static bool Present(const CcSim *sim, const CcCharacter *p)
{
    return Alive(sim, p) && p->current_settlement_id == sim->player.location_id &&
        p->activity != CC_CHARACTER_ACTIVITY_TRAVELLING && p->travel_destination_id == 0;
}
static CcId NewId(CcSim *sim, CcEntityKind kind)
{
    return ((uint64_t)kind << 56U) | sim->next_entity_serial++;
}
static bool Issued(const CcSim *sim, CcId id, CcEntityKind kind)
{
    return CcIdKind(id) == kind && (id & UINT64_C(0x00ffffffffffffff)) > 0 &&
        (id & UINT64_C(0x00ffffffffffffff)) < sim->next_entity_serial;
}
static CcCustodyEntry *Entry(CcSim *sim, CcId id)
{
    for (int i = 0; i < CC_CUSTODY_CAPACITY; ++i)
        if (sim->custody.entries[i].active && sim->custody.entries[i].id == id)
            return &sim->custody.entries[i];
    return NULL;
}
const CcPersonalWant *CcWantsFind(const CcSim *sim, CcId id)
{
    if (sim == NULL || sim->schema_version < 117U || id == 0) return NULL;
    for (int i = 0; i < CC_PERSONAL_WANTS; ++i)
        if (sim->wants.wants[i].id == id) return &sim->wants.wants[i];
    return NULL;
}
const CcBelonging *CcWantsItem(const CcSim *sim, CcId id)
{
    if (sim == NULL || sim->schema_version < 117U || id == 0) return NULL;
    for (int i = 0; i < CC_BELONGINGS; ++i)
        if (sim->wants.items[i].id == id) return &sim->wants.items[i];
    return NULL;
}
static CcPersonalWant *Want(CcSim *sim, CcId id)
{
    for (int i = 0; i < CC_PERSONAL_WANTS; ++i)
        if (id != 0 && sim->wants.wants[i].id == id) return &sim->wants.wants[i];
    return NULL;
}
static const CcCustodyEntry *ItemEntry(const CcSim *sim, const CcBelonging *item)
{
    return item != NULL ? CcCustodyFind(&sim->custody, item->custody_id) : NULL;
}
bool CcWantsCustodyReference(const CcSim *sim, const CcCustodyEntry *entry)
{
    const CcBelonging *item = entry != NULL ? CcWantsItem(sim, entry->reference_id) : NULL;
    return item != NULL && entry->kind == CC_CUSTODY_BELONGING &&
        item->custody_id == entry->id && Issued(sim, entry->owner_id, CC_ENTITY_CHARACTER);
}
static int TownSlot(const CcSim *sim, CcId id)
{
    for (int i = 0; i < sim->settlement_count; ++i)
        if (sim->settlements[i].id == id) return i;
    return -1;
}
static bool Reachable(const CcSim *sim, CcId from, CcId to)
{
    int start = TownSlot(sim, from), end = TownSlot(sim, to);
    if (start < 0 || end < 0) return false;
    bool seen[CC_MAX_SETTLEMENTS] = {false}; seen[start] = true;
    for (int pass = 0; pass < sim->settlement_count; ++pass) {
        for (int i = 0; i < sim->route_count; ++i) {
            const CcRoute *r = &sim->routes[i];
            if (r->closed || r->condition <= 0) continue;
            int a = TownSlot(sim, r->from_id), b = TownSlot(sim, r->to_id);
            if (a >= 0 && b >= 0 && (seen[a] || seen[b])) seen[a] = seen[b] = true;
        }
    }
    return seen[end];
}
static CcId Supply(const CcSim *sim, CcId from, CcGood good, int quantity)
{
    for (int i = 0; i < sim->settlement_count; ++i) {
        const CcSettlement *town = &sim->settlements[i];
        if (town->population > 0 && town->stock[good] >= quantity && Reachable(sim, from, town->id))
            return town->id;
    }
    return 0;
}
static const CcCharacter *Smith(const CcSim *sim, CcId from)
{
    for (int pass = 0; pass < 2; ++pass) {
    for (int i = 0; i < sim->character_count; ++i) {
        const CcCharacter *p = &sim->characters[i];
        if ((p->current_settlement_id == from) != (pass == 0)) continue;
        const CcSettlement *town = CcSimSettlement(sim, p->current_settlement_id);
        if (Alive(sim, p) && p->occupation == CC_OCCUPATION_SMITH &&
            p->activity != CC_CHARACTER_ACTIVITY_TRAVELLING && p->travel_destination_id == 0 &&
            town != NULL && town->population > 0 && town->stock[CC_GOOD_TOOLS] > 0 &&
            CcSettlementHasService(town, CC_SERVICE_SMITHY) && Reachable(sim, from, town->id) &&
            Supply(sim, town->id, CC_GOOD_IRON, 1) != 0) return p;
    }
    }
    return NULL;
}
static bool HasChild(const CcSim *sim, CcId id)
{
    for (int i = 0; i < CC_PERSONAL_WANTS; ++i)
        if (id != 0 && sim->wants.wants[i].parent_id == id) return true;
    return false;
}
static CcPersonalWant *FreeWant(CcSim *sim)
{
    CcPersonalWant *oldest = NULL;
    for (int i = 0; i < CC_PERSONAL_WANTS; ++i) {
        CcPersonalWant *w = &sim->wants.wants[i];
        if (w->id == 0) return w;
        if (w->status != CC_WANT_ACTIVE && w->settled_day + 28 < sim->current_day &&
            (w->parent_id == 0 || (CcWantsFind(sim, w->parent_id) != NULL &&
             CcWantsFind(sim, w->parent_id)->status != CC_WANT_ACTIVE)) &&
            !HasChild(sim, w->id) && (oldest == NULL || w->settled_day < oldest->settled_day)) oldest = w;
    }
    return oldest;
}
static bool HasRoot(const CcSim *sim, CcId person)
{
    for (int i = 0; i < CC_PERSONAL_WANTS; ++i) {
        const CcPersonalWant *w = &sim->wants.wants[i];
        if (w->person_id == person && w->parent_id == 0 &&
            (w->status == CC_WANT_ACTIVE || w->settled_day + 14 > sim->current_day)) return true;
    }
    return false;
}
static void WantName(const CcSim *sim, const CcPersonalWant *w, char *text, size_t capacity)
{
    const CcBelonging *item = CcWantsItem(sim, w->item_id);
    if (w->kind == CC_WANT_REPAIR_IRON) (void)snprintf(text, capacity, "1 iron to repair %.47s", item != NULL ? item->name : "the tool");
    else if (item != NULL) (void)snprintf(text, capacity, "%s", item->name);
    else (void)snprintf(text, capacity, "%d %s", w->quantity, CcGoodName((CcGood)w->good));
}
static CcPersonalWant *Create(CcSim *sim, CcCharacter *p, CcWantKind kind,
    CcGood good, int quantity, const CcBelonging *item, CcId source, CcId parent)
{
    CcPersonalWant *w = FreeWant(sim);
    if (w == NULL || source == 0) return NULL;
    *w = (CcPersonalWant){.id = NewId(sim, CC_ENTITY_PERSONAL_WANT), .person_id = p->id,
        .item_id = item != NULL ? item->id : 0, .parent_id = parent, .source_place_id = source,
        .home_id = p->current_settlement_id, .kind = (int32_t)kind, .status = CC_WANT_ACTIVE,
        .good = (int32_t)good, .quantity = quantity, .created_day = sim->current_day, .revision = 1};
    int reward = parent != 0 ? 0 : item != NULL ? 6 : 2;
    w->escrow = p->travel_coins < reward ? p->travel_coins : reward;
    p->travel_coins -= w->escrow;
    char object[96], text[CC_EVENT_TEXT_CAPACITY]; WantName(sim, w, object, sizeof(object));
    (void)snprintf(text, sizeof(text), "%.31s asks for %.95s.", p->name, object);
    const CcEvent *event = CcSimPushEvent(sim, CC_EVENT_WANT_CREATED, p->id,
        p->current_settlement_id, item != NULL ? item->cause_event_id : 0, quantity, text);
    w->cause_event_id = event->id;
    return w;
}
static void Finish(CcSim *sim, CcPersonalWant *w, CcWantStatus status)
{
    CcCharacter *p = Person(sim, w->person_id);
    CcSettlement *town = CcSimSettlementMutable(sim, w->home_id);
    if (status == CC_WANT_FULFILLED) sim->player.coins += w->escrow;
    else if (Alive(sim, p) && p->travel_coins <= CC_SIM_MAX_MONEY - w->escrow) p->travel_coins += w->escrow;
    else if (town != NULL && town->market_coins <= CC_SIM_MAX_MONEY - w->escrow) town->market_coins += w->escrow;
    else return;
    w->escrow = 0; w->status = (int32_t)status; w->settled_day = sim->current_day; ++w->revision;
    char object[96], text[CC_EVENT_TEXT_CAPACITY]; WantName(sim, w, object, sizeof(object));
    if (status == CC_WANT_FULFILLED)
        (void)snprintf(text, sizeof(text), "%.31s receives %.79s from the company.", p != NULL ? p->name : "The recipient", object);
    else (void)snprintf(text, sizeof(text), "The request for %.95s ends.", object);
    const CcEvent *event = CcSimPushEvent(sim,
        status == CC_WANT_FULFILLED ? CC_EVENT_WANT_FULFILLED : CC_EVENT_WANT_CLOSED,
        w->person_id, w->home_id, w->cause_event_id, w->quantity, text);
    w->outcome_event_id = event->id;
    if (status == CC_WANT_FULFILLED && p != NULL) {
        p->player_disposition = p->player_disposition > 94 ? 100 : p->player_disposition + 6;
        p->stress = p->stress > 8 ? p->stress - 8 : 0;
        CcCharacterMemory *m = &p->memories[p->memory_write_index];
        *m = (CcCharacterMemory){.kind = CC_CHARACTER_MEMORY_PLAYER_HELPED,
            .subject_id = p->current_settlement_id, .event_id = event->id, .day = sim->current_day};
        p->memory_write_index = (p->memory_write_index + 1) % CC_CHARACTER_MEMORY_CAPACITY;
        if (p->memory_count < CC_CHARACTER_MEMORY_CAPACITY) ++p->memory_count;
    }
}
static CcGood WorkInput(CcCharacterOccupation occupation)
{
    switch (occupation) {
        case CC_OCCUPATION_BAKER: case CC_OCCUPATION_MILLER: return CC_GOOD_WHEAT;
        case CC_OCCUPATION_SMITH: return CC_GOOD_IRON;
        case CC_OCCUPATION_CARTWRIGHT: return CC_GOOD_WOOD;
        default: return CC_GOOD_COUNT;
    }
}
static const char *ToolName(CcCharacterOccupation occupation)
{
    switch (occupation) {
        case CC_OCCUPATION_WOODCUTTER: return "axe";
        case CC_OCCUPATION_QUARRYMAN: return "pick";
        case CC_OCCUPATION_SMITH: case CC_OCCUPATION_CARTWRIGHT: return "hammer";
        case CC_OCCUPATION_FARMER: case CC_OCCUPATION_SHEPHERD: return "knife";
        default: return "work lamp";
    }
}
static void SeedBelongings(CcSim *sim)
{
    int item_slot = 0;
    for (int i = 0; i < sim->character_count && item_slot < CC_BELONGINGS; ++i) {
        CcCharacter *p = &sim->characters[i];
        CcSettlement *town = CcSimSettlementMutable(sim, p->current_settlement_id);
        if (!Alive(sim, p) || p->occupation == CC_OCCUPATION_NONE || town == NULL ||
            town->stock[CC_GOOD_TOOLS] <= 2 || p->activity == CC_CHARACTER_ACTIVITY_TRAVELLING) continue;
        CcCustodyEntry *entry = NULL;
        for (int j = 0; j < CC_CUSTODY_CAPACITY; ++j)
            if (sim->custody.entries[j].id == 0) { entry = &sim->custody.entries[j]; break; }
        if (entry == NULL) break;
        CcBelonging *item = &sim->wants.items[item_slot++];
        uint32_t choice = sim->world_seed + p->appearance_seed * UINT32_C(2654435761) +
            (uint32_t)p->id * UINT32_C(2246822519);
        choice ^= choice >> 16U;
        choice *= UINT32_C(2246822519);
        choice ^= choice >> 13U;
        CcId place = town->id;
        if (choice % 3U == 0) {
            for (int j = 0; j < sim->settlement_count; ++j)
                if (sim->settlements[j].id != town->id && sim->settlements[j].population > 0 &&
                    Reachable(sim, town->id, sim->settlements[j].id)) { place = sim->settlements[j].id; break; }
        }
        *item = (CcBelonging){.id = NewId(sim, CC_ENTITY_BELONGING),
            .custody_id = sim->custody.next_id++, .home_id = town->id,
            .last_place_id = place, .last_wear_day = sim->current_day};
        (void)snprintf(item->name, sizeof(item->name), "%.26s's %s", p->name, ToolName(p->occupation));
        *entry = (CcCustodyEntry){.id = item->custody_id, .revision = 1, .owner_id = p->id,
            .holder = {place == town->id ? CC_CUSTODY_CHARACTER : CC_CUSTODY_STORE,
                       place == town->id ? p->id : place},
            .kind = CC_CUSTODY_BELONGING, .reference_id = item->id, .quantity = 1,
            .condition = choice % 3U == 1 ? 45 : 100, .active = true};
        --town->stock[CC_GOOD_TOOLS];
        char text[CC_EVENT_TEXT_CAPACITY];
        const CcSettlement *at = CcSimSettlement(sim, place);
        (void)snprintf(text, sizeof(text), "%.47s is kept at %.31s.", item->name, at->name);
        item->cause_event_id = CcSimPushEvent(sim, CC_EVENT_BELONGING_MOVED,
            p->id, place, 0, entry->condition, text)->id;
        entry->last_event_id = item->cause_event_id;
    }
}
void CcWantsInit(CcSim *sim)
{
    if (sim == NULL || sim->schema_version < 117U || sim->wants.initialized) return;
    /* The first direct question starts personal requests. Further changes
       follow the live world, with this state stored in the campaign. */
    sim->wants = (CcWantsState){.initialized = 1, .last_day = sim->current_day};
}
void CcWantsAdvance(CcSim *sim)
{
    if (sim == NULL || sim->schema_version < 117U || !sim->wants.initialized) return;
    if (sim->wants.initialized == 1) return;
    bool new_day = sim->wants.last_day != sim->current_day;
    sim->wants.last_day = sim->current_day;
    for (int i = 0; i < CC_BELONGINGS; ++i) {
        CcBelonging *item = &sim->wants.items[i];
        CcCustodyEntry *e = Entry(sim, item->custody_id);
        if (e == NULL) continue;
        const CcCharacter *p = CcSimCharacter(sim, e->owner_id);
        if (e->holder.kind == CC_CUSTODY_CHARACTER) {
            if (!Alive(sim, p)) {
                e->holder = (CcCustodyHolder){CC_CUSTODY_STORE, item->last_place_id}; ++e->revision;
            } else {
                item->last_place_id = p->current_settlement_id;
                if (new_day && p->activity == CC_CHARACTER_ACTIVITY_WORKING &&
                    sim->current_day - item->last_wear_day >= 7 && e->condition > 40) {
                    --e->condition; ++e->revision; item->last_wear_day = sim->current_day;
                }
            }
        }
    }
    for (int i = 0; i < CC_PERSONAL_WANTS; ++i) {
        CcPersonalWant *w = &sim->wants.wants[i];
        if (w->id == 0 || w->status != CC_WANT_ACTIVE) continue;
        const CcCharacter *p = CcSimCharacter(sim, w->person_id);
        const CcPersonalWant *parent = CcWantsFind(sim, w->parent_id);
        if (!Alive(sim, p) || (w->parent_id != 0 && (parent == NULL || parent->status != CC_WANT_ACTIVE))) {
            Finish(sim, w, CC_WANT_CLOSED); continue;
        }
        if (w->kind == CC_WANT_REPAIR) {
            bool can_repair = false;
            const CcCustodyEntry *item_entry = ItemEntry(sim, CcWantsItem(sim, w->item_id));
            if (item_entry != NULL && item_entry->condition >= 70) can_repair = true;
            for (int j = 0; j < CC_PERSONAL_WANTS; ++j) {
                const CcPersonalWant *child = &sim->wants.wants[j];
                if (child->parent_id == w->id && child->status == CC_WANT_ACTIVE &&
                    Alive(sim, CcSimCharacter(sim, child->person_id))) can_repair = true;
            }
            if (!can_repair) { Finish(sim, w, CC_WANT_CLOSED); continue; }
        }
        const CcSettlement *town = CcSimSettlement(sim, w->home_id);
        if ((w->kind == CC_WANT_MEAL && p->hungry_days == 0) ||
            (w->kind == CC_WANT_WORK_SUPPLIES && town != NULL && town->stock[w->good] >= w->quantity))
            Finish(sim, w, CC_WANT_SETTLED);
        if (w->item_id != 0 && ItemEntry(sim, CcWantsItem(sim, w->item_id)) == NULL)
            Finish(sim, w, CC_WANT_CLOSED);
    }
    for (int pass = 0; pass < 2; ++pass)
        for (int i = 0; i < CC_PERSONAL_WANTS; ++i) {
            CcPersonalWant *w = &sim->wants.wants[i];
            const CcPersonalWant *parent = CcWantsFind(sim, w->parent_id);
            if (w->id != 0 && w->status != CC_WANT_ACTIVE &&
                sim->current_day - w->settled_day > 28 && !HasChild(sim, w->id) &&
                (parent == NULL || parent->status != CC_WANT_ACTIVE)) *w = (CcPersonalWant){0};
        }
    for (int i = 0; i < sim->character_count; ++i) {
        CcCharacter *p = &sim->characters[i];
        CcSettlement *town = CcSimSettlementMutable(sim, p->current_settlement_id);
        if (!Alive(sim, p) || town == NULL || town->population == 0 || HasRoot(sim, p->id) ||
            p->bandit_group_id != 0 || p->activity == CC_CHARACTER_ACTIVITY_TRAVELLING ||
            CcCharacterAgeYears(sim, p) < 16) continue;
        if (p->hungry_days > 0) {
            CcId source = Supply(sim, town->id, CC_GOOD_BREAD, 1);
            if (source != 0) { (void)Create(sim, p, CC_WANT_MEAL, CC_GOOD_BREAD, 1, NULL, source, 0); continue; }
        }
        for (int j = 0; j < CC_BELONGINGS && !HasRoot(sim, p->id); ++j) {
            const CcBelonging *item = &sim->wants.items[j];
            const CcCustodyEntry *e = ItemEntry(sim, item);
            if (e == NULL || e->owner_id != p->id) continue;
            bool away = e->holder.kind != CC_CUSTODY_CHARACTER;
            CcId source = e->holder.kind == CC_CUSTODY_STORE ? e->holder.id :
                e->holder.kind == CC_CUSTODY_PLAYER ? sim->player.location_id : town->id;
            if (!Reachable(sim, town->id, source)) continue;
            if (e->condition < 70) {
                const CcCharacter *smith = Smith(sim, source);
                int free_slots = 0;
                for (int k = 0; k < CC_PERSONAL_WANTS; ++k) if (sim->wants.wants[k].id == 0) ++free_slots;
                if (smith != NULL && free_slots >= 2) {
                    CcPersonalWant *root = Create(sim, p, CC_WANT_REPAIR, CC_GOOD_TOOLS, 1, item, source, 0);
                    if (root != NULL) (void)Create(sim, Person(sim, smith->id), CC_WANT_REPAIR_IRON,
                        CC_GOOD_IRON, 1, item, Supply(sim, smith->current_settlement_id, CC_GOOD_IRON, 1), root->id);
                }
            } else if (away) (void)Create(sim, p, CC_WANT_RECOVER, CC_GOOD_TOOLS, 1, item, source, 0);
        }
        if (HasRoot(sim, p->id)) continue;
        CcGood input = WorkInput(p->occupation);
        if (input < CC_GOOD_COUNT && town->stock[input] < 2) {
            CcId source = Supply(sim, town->id, input, 2);
            if (source != 0) (void)Create(sim, p, CC_WANT_WORK_SUPPLIES, input, 2, NULL, source, 0);
        }
    }
}

static bool Fail(char *text, size_t capacity, const char *reason)
{
    if (text != NULL && capacity > 0) (void)snprintf(text, capacity, "%s", reason);
    return false;
}
static bool Resolve(const void *context, CcCustodyHolder holder, CcCustodyLocation *at, int64_t *capacity)
{
    const CcSim *sim = context;
    *capacity = INT64_MAX;
    if (holder.kind == CC_CUSTODY_PLAYER && holder.id == sim->player.id) {
        *at = (CcCustodyLocation){.place_id = sim->player.location_id};
        *capacity = INT64_MAX; return true;
    }
    if (holder.kind == CC_CUSTODY_STORE && CcSimSettlement(sim, holder.id) != NULL) {
        *at = (CcCustodyLocation){.place_id = holder.id}; return true;
    }
    const CcCharacter *p = CcSimCharacter(sim, holder.id);
    if (holder.kind == CC_CUSTODY_CHARACTER && Present(sim, p)) {
        *at = (CcCustodyLocation){.place_id = p->current_settlement_id}; return true;
    }
    return false;
}
static bool Permit(const void *context, uint64_t actor, const CcCustodyEntry *entry, CcCustodyHolder destination)
{
    const CcSim *sim = context;
    return actor == sim->player.id && entry->kind == CC_CUSTODY_BELONGING &&
        ((destination.kind == CC_CUSTODY_PLAYER && destination.id == actor) ||
         (destination.kind == CC_CUSTODY_CHARACTER && destination.id == entry->owner_id) ||
         (destination.kind == CC_CUSTODY_STORE && destination.id == sim->player.location_id));
}
static CcCustodyResult Transfer(CcSim *sim, const CcBelonging *item, CcCustodyHolder to, bool apply)
{
    const CcCustodyEntry *e = ItemEntry(sim, item);
    if (e == NULL) return CC_CUSTODY_INVALID;
    CcCustodyRules rules = {.context = sim, .good_count = CC_GOOD_COUNT, .resolve = Resolve, .permit = Permit};
    CcCustodyTransfer move = {.entry_id = e->id, .revision = e->revision,
        .actor_id = sim->player.id, .event_id = e->last_event_id, .destination = to, .quantity = 1};
    return apply ? CcCustodyApplyTransfer(&sim->custody, &rules, &move, NULL) :
        CcCustodyPlanTransfer(&sim->custody, &rules, &move);
}
bool CcWantsPlan(const CcSim *sim, const CcCommand *cmd, char *reason, size_t capacity)
{
    if (sim == NULL || cmd == NULL || sim->schema_version < 117U || cmd->kind != CC_COMMAND_PERSONAL_WANT ||
        (cmd->actor_id != 0 && cmd->actor_id != sim->player.id)) return Fail(reason, capacity, "Choose a current personal request.");
    if (sim->journey.active || sim->mine.phase != CC_MINE_NONE || sim->dungeon_expedition.active)
        return Fail(reason, capacity, "Meet at a town to exchange items.");
    if (cmd->amount == CC_WANT_DISCOVER) {
        if (sim->wants.initialized != 1 || !Present(sim, CcSimCharacter(sim, cmd->target_id)))
            return Fail(reason, capacity, "Choose a person's current item request.");
        return true;
    }
    if (cmd->amount == CC_WANT_TAKE || cmd->amount == CC_WANT_LEAVE) {
        const CcBelonging *item = CcWantsItem(sim, cmd->target_id);
        const CcCustodyEntry *e = ItemEntry(sim, item);
        if (e == NULL || cmd->secondary_id != e->revision) return Fail(reason, capacity, "Choose the item's current location again.");
        if (cmd->amount == CC_WANT_LEAVE) {
            if (e->holder.kind != CC_CUSTODY_PLAYER || e->holder.id != sim->player.id ||
                Transfer((CcSim *)sim, item, (CcCustodyHolder){CC_CUSTODY_STORE, sim->player.location_id}, false) != CC_CUSTODY_READY)
                return Fail(reason, capacity, "Bring the item to the town stores.");
            return true;
        }
        int carried = 0;
        for (int i = 0; i < CC_BELONGINGS; ++i) {
            const CcCustodyEntry *held = ItemEntry(sim, &sim->wants.items[i]);
            if (held != NULL && held->holder.kind == CC_CUSTODY_PLAYER) ++carried;
        }
        if (carried >= CC_BELONGINGS_CARRIED)
            return Fail(reason, capacity, "The satchel holds four belongings. Leave one in the town stores.");
        const CcCharacter *owner = CcSimCharacter(sim, e->owner_id);
        bool permission = e->holder.kind == CC_CUSTODY_STORE && e->holder.id == sim->player.location_id && Alive(sim, owner);
        for (int i = 0; i < CC_PERSONAL_WANTS; ++i) {
            const CcPersonalWant *w = &sim->wants.wants[i];
            if (w->item_id == item->id && w->parent_id == 0 && w->status == CC_WANT_ACTIVE &&
                w->known_day > 0 && Present(sim, owner) && e->holder.kind == CC_CUSTODY_CHARACTER)
                permission = true;
        }
        if (!permission) return Fail(reason, capacity, "Ask the owner to lend the item, or collect it at its recorded store.");
        CcSim *mutable_sim = (CcSim *)sim;
        if (Transfer(mutable_sim, item, (CcCustodyHolder){CC_CUSTODY_PLAYER, sim->player.id}, false) != CC_CUSTODY_READY)
            return Fail(reason, capacity, "Meet at the item's current location.");
        return true;
    }
    const CcPersonalWant *w = CcWantsFind(sim, cmd->target_id);
    const CcCharacter *p = w != NULL ? CcSimCharacter(sim, w->person_id) : NULL;
    if (w == NULL || w->status != CC_WANT_ACTIVE || cmd->secondary_id != (uint64_t)w->revision)
        return Fail(reason, capacity, "This request has changed. Read the current request.");
    if (!Present(sim, p)) return Fail(reason, capacity, "Meet the person at their current town.");
    if (cmd->amount == CC_WANT_LEARN) {
        if (w->known_day > 0) return Fail(reason, capacity, "The request is already in the company book.");
        if (w->parent_id != 0) {
            const CcPersonalWant *parent = CcWantsFind(sim, w->parent_id);
            if (parent == NULL || parent->known_day == 0) return Fail(reason, capacity, "Ask the owner about the repair first.");
        }
        return true;
    }
    if (cmd->amount != CC_WANT_GIVE || w->known_day == 0) return Fail(reason, capacity, "Hear the request before agreeing to the exchange.");
    if (sim->player.coins > CC_SIM_MAX_MONEY - w->escrow) return Fail(reason, capacity, "Make room in the company purse.");
    const CcBelonging *item = CcWantsItem(sim, w->item_id);
    const CcCustodyEntry *e = ItemEntry(sim, item);
    if (w->item_id != 0 && (e == NULL || e->holder.kind != CC_CUSTODY_PLAYER || e->holder.id != sim->player.id))
        return Fail(reason, capacity, "Bring the named item in the satchel.");
    if (w->kind == CC_WANT_MEAL && p->hungry_days == 0)
        return Fail(reason, capacity, "This person has eaten. Check back another day.");
    if (w->kind == CC_WANT_REPAIR_IRON) {
        const CcPersonalWant *parent = CcWantsFind(sim, w->parent_id);
        const CcSettlement *town = CcSimSettlement(sim, sim->player.location_id);
        if (parent == NULL || parent->status != CC_WANT_ACTIVE || p->occupation != CC_OCCUPATION_SMITH || town == NULL ||
            town->population == 0 || !CcSettlementHasService(town, CC_SERVICE_SMITHY) ||
            town->stock[CC_GOOD_TOOLS] == 0 || item->repair_iron >= CC_SIM_MAX_UNITS)
            return Fail(reason, capacity, "Find the smith at a working smithy with tools.");
    } else if (item != NULL) {
        if (w->kind == CC_WANT_REPAIR && e->condition < 70) return Fail(reason, capacity, "Take the item and one iron to the named smith first.");
        if (Transfer((CcSim *)sim, item, (CcCustodyHolder){CC_CUSTODY_CHARACTER, p->id}, false) != CC_CUSTODY_READY)
            return Fail(reason, capacity, "Meet the owner with the named item.");
        return true;
    }
    if (sim->player.cargo[w->good] < w->quantity) return Fail(reason, capacity, "Bring the requested goods in the carriage.");
    if (w->kind == CC_WANT_WORK_SUPPLIES) {
        const CcSettlement *town = CcSimSettlement(sim, w->home_id);
        if (town != NULL && town->stock[w->good] >= w->quantity)
            return Fail(reason, capacity, "The workplace has its supplies. Check back another day.");
        if (town == NULL || p->current_settlement_id != w->home_id)
            return Fail(reason, capacity, "Deliver these supplies at the person's workplace.");
    }
    return true;
}
bool CcWantsApply(CcSim *sim, const CcCommand *cmd, char *message, size_t capacity)
{
    if (!CcWantsPlan(sim, cmd, message, capacity)) return false;
    if (cmd->amount == CC_WANT_DISCOVER) {
        sim->wants.initialized = 2;
        SeedBelongings(sim);
        CcWantsAdvance(sim);
        for (int i = 0; i < CC_PERSONAL_WANTS; ++i) {
            const CcPersonalWant *w = &sim->wants.wants[i];
            if (w->person_id == cmd->target_id && w->parent_id == 0 && w->status == CC_WANT_ACTIVE) {
                CcCommand learn = {.kind = CC_COMMAND_PERSONAL_WANT, .target_id = w->id,
                    .secondary_id = (uint64_t)w->revision, .amount = CC_WANT_LEARN};
                return CcWantsApply(sim, &learn, message, capacity);
            }
        }
        (void)snprintf(message, capacity, "I have what I need today. Ask the other workers about their tools and supplies.");
        return true;
    }
    if (cmd->amount == CC_WANT_TAKE || cmd->amount == CC_WANT_LEAVE) {
        const CcBelonging *item = CcWantsItem(sim, cmd->target_id);
        CcCustodyHolder to = cmd->amount == CC_WANT_TAKE ?
            (CcCustodyHolder){CC_CUSTODY_PLAYER, sim->player.id} :
            (CcCustodyHolder){CC_CUSTODY_STORE, sim->player.location_id};
        if (Transfer(sim, item, to, true) != CC_CUSTODY_READY)
            return Fail(message, capacity, "Choose the item again.");
        char text[CC_EVENT_TEXT_CAPACITY];
        if (cmd->amount == CC_WANT_TAKE)
            (void)snprintf(text, sizeof(text), "The company carries %.47s.", item->name);
        else
            (void)snprintf(text, sizeof(text), "%.47s is kept at %.31s.", item->name,
                CcSimSettlement(sim, sim->player.location_id)->name);
        for (int i = 0; i < CC_BELONGINGS; ++i)
            if (sim->wants.items[i].id == item->id) sim->wants.items[i].last_place_id = sim->player.location_id;
        if (cmd->amount == CC_WANT_LEAVE)
            for (int i = 0; i < CC_PERSONAL_WANTS; ++i)
                if (sim->wants.wants[i].item_id == item->id && sim->wants.wants[i].parent_id == 0) {
                    sim->wants.wants[i].source_place_id = sim->player.location_id;
                    ++sim->wants.wants[i].revision;
                }
        CcId event = CcSimPushEvent(sim, CC_EVENT_BELONGING_MOVED, sim->player.id,
            sim->player.location_id, item->cause_event_id, 1, text)->id;
        Entry(sim, item->custody_id)->last_event_id = event;
        (void)snprintf(message, capacity, "%s", text); return true;
    }
    CcPersonalWant *w = Want(sim, cmd->target_id);
    if (cmd->amount == CC_WANT_LEARN) {
        w->known_day = sim->current_day; ++w->revision;
        for (int i = 0; i < CC_PERSONAL_WANTS; ++i)
            if (sim->wants.wants[i].parent_id == w->id && sim->wants.wants[i].known_day == 0) {
                sim->wants.wants[i].known_day = sim->current_day; ++sim->wants.wants[i].revision;
            }
        (void)CcWantsRequestText(sim, w->id, message, capacity); return true;
    }
    const CcBelonging *item = CcWantsItem(sim, w->item_id);
    if (w->kind == CC_WANT_REPAIR_IRON) {
        CcCustodyEntry *e = Entry(sim, item->custody_id);
        sim->player.cargo[CC_GOOD_IRON] -= 1;
        for (int i = 0; i < CC_BELONGINGS; ++i)
            if (sim->wants.items[i].id == item->id) {
                ++sim->wants.items[i].repair_iron; sim->wants.items[i].last_wear_day = sim->current_day;
            }
        e->condition = 100; ++e->revision;
        char text[CC_EVENT_TEXT_CAPACITY];
        const CcCharacter *p = CcSimCharacter(sim, w->person_id);
        (void)snprintf(text, sizeof(text), "%.31s repairs %.47s with iron.", p->name, item->name);
        e->last_event_id = CcSimPushEvent(sim, CC_EVENT_BELONGING_REPAIRED, p->id,
            sim->player.location_id, w->cause_event_id, 1, text)->id;
    } else if (item != NULL) {
        if (Transfer(sim, item, (CcCustodyHolder){CC_CUSTODY_CHARACTER, w->person_id}, true) != CC_CUSTODY_READY)
            return Fail(message, capacity, "Choose the current handover again.");
    } else {
        sim->player.cargo[w->good] -= w->quantity;
        if (w->kind == CC_WANT_MEAL) Person(sim, w->person_id)->hungry_days = 0;
        else CcSimSettlementMutable(sim, w->home_id)->stock[w->good] += w->quantity;
    }
    Finish(sim, w, CC_WANT_FULFILLED);
    if (item != NULL) Entry(sim, item->custody_id)->last_event_id = w->outcome_event_id;
    const CcEvent *event = CcSimRecentEvent(sim, 0);
    (void)snprintf(message, capacity, "%s", event != NULL ? event->text : "The handover is complete.");
    return true;
}

static void Offer(const CcSim *sim, CcWantOffer *o, CcCommand cmd, const char *label, const char *detail)
{
    *o = (CcWantOffer){.command = cmd};
    (void)snprintf(o->label, sizeof(o->label), "%s", label);
    o->ready = CcWantsPlan(sim, &cmd, o->detail, sizeof(o->detail));
    if (o->ready) (void)snprintf(o->detail, sizeof(o->detail), "%s", detail);
}
int32_t CcWantsOffers(const CcSim *sim, CcId person, CcWantOffer *offers, int32_t capacity)
{
    if (sim == NULL || offers == NULL || capacity <= 0 || sim->schema_version < 117U) return 0;
    int count = 0;
    if (person != 0 && sim->wants.initialized == 1) {
        Offer(sim, &offers[count++], (CcCommand){.kind = CC_COMMAND_PERSONAL_WANT,
            .target_id = person, .amount = CC_WANT_DISCOVER}, "What do you need?",
            "Ask about tools, food, and work supplies.");
        return count;
    }
    for (int i = 0; i < CC_PERSONAL_WANTS && count < capacity && person != 0; ++i) {
        const CcPersonalWant *w = &sim->wants.wants[i];
        if (w->status != CC_WANT_ACTIVE || w->person_id != person) continue;
        const CcPersonalWant *parent = CcWantsFind(sim, w->parent_id);
        if (w->parent_id != 0 && (parent == NULL || parent->known_day == 0)) continue;
        char object[96], label[128], detail[192]; WantName(sim, w, object, sizeof(object));
        const CcSettlement *source = CcSimSettlement(sim, w->source_place_id);
        (void)snprintf(label, sizeof(label), "%s %.95s", w->known_day == 0 ? "Ask about" : "Give", object);
        (void)snprintf(detail, sizeof(detail), "%s. Reward: %" PRId64 " crowns and trust.",
            source != NULL ? source->name : "Ask locally", w->escrow);
        Offer(sim, &offers[count++], (CcCommand){.kind = CC_COMMAND_PERSONAL_WANT,
            .target_id = w->id, .secondary_id = (uint64_t)w->revision,
            .amount = w->known_day == 0 ? CC_WANT_LEARN : CC_WANT_GIVE}, label, detail);
    }
    for (int i = 0; i < CC_BELONGINGS && count < capacity; ++i) {
        const CcBelonging *item = &sim->wants.items[i];
        const CcCustodyEntry *e = ItemEntry(sim, item);
        bool carried = e != NULL && e->holder.kind == CC_CUSTODY_PLAYER && e->holder.id == sim->player.id;
        if (e == NULL || (person == 0 ? !carried && (e->holder.kind != CC_CUSTODY_STORE || e->holder.id != sim->player.location_id) :
            e->holder.kind != CC_CUSTODY_CHARACTER || e->holder.id != person)) continue;
        CcCommand cmd = {.kind = CC_COMMAND_PERSONAL_WANT, .target_id = item->id,
            .secondary_id = e->revision, .amount = carried ? CC_WANT_LEAVE : CC_WANT_TAKE};
        char label[96]; (void)snprintf(label, sizeof(label), "%s %s", carried ? "Store" : person == 0 ? "Recover" : "Borrow", item->name);
        CcWantOffer proposal; Offer(sim, &proposal, cmd, label,
            carried ? "Leave this item in the town stores." : "Carry this named item in the satchel.");
        if (proposal.ready || person == 0) offers[count++] = proposal;
    }
    return count;
}
bool CcWantsRequestText(const CcSim *sim, CcId request, char *text, size_t capacity)
{
    const CcPersonalWant *w = CcWantsFind(sim, request);
    if (w == NULL || text == NULL || capacity == 0) return false;
    char object[96]; WantName(sim, w, object, sizeof(object));
    const CcSettlement *source = CcSimSettlement(sim, w->source_place_id);
    const char *reason = w->kind == CC_WANT_MEAL ? "I need a meal." :
        w->kind == CC_WANT_WORK_SUPPLIES ? "These supplies will keep our work going." :
        w->kind == CC_WANT_RECOVER ? "I left it in the town stores. I need it for work." :
        w->kind == CC_WANT_REPAIR ? "It is worn. Bring it back in working order." :
        "Bring the item and the iron to my smithy.";
    const CcCharacter *smith = NULL;
    const CcSettlement *smith_town = NULL;
    for (int j = 0; j < CC_PERSONAL_WANTS; ++j)
        if (sim->wants.wants[j].parent_id == w->id) {
            smith = CcSimCharacter(sim, sim->wants.wants[j].person_id);
            smith_town = CcSimSettlement(sim, sim->wants.wants[j].home_id);
        }
    (void)snprintf(text, capacity, "I need %s. %s %s%s.%s%s%s%s%s",
        object, reason, w->item_id != 0 && w->kind != CC_WANT_REPAIR_IRON ? "Last kept at " : "Try ",
        source != NULL ? source->name : "the local market",
        smith != NULL ? " Ask " : "", smith != NULL ? smith->name : "",
        smith_town != NULL ? " at " : "", smith_town != NULL ? smith_town->name : "",
        smith != NULL ? " for the repair." : "");
    return true;
}
bool CcWantsPersonText(const CcSim *sim, CcId person, char *text, size_t capacity)
{
    if (sim == NULL || sim->schema_version < 117U) return false;
    for (int i = 0; i < CC_PERSONAL_WANTS; ++i) {
        const CcPersonalWant *w = &sim->wants.wants[i];
        if (w->person_id != person || w->status != CC_WANT_ACTIVE) continue;
        const CcPersonalWant *parent = CcWantsFind(sim, w->parent_id);
        if (w->parent_id != 0 && (parent == NULL || parent->known_day == 0)) continue;
        return CcWantsRequestText(sim, w->id, text, capacity);
    }
    return false;
}
void CcWantsDescribe(const CcSim *sim, char *text, size_t capacity)
{
    if (text == NULL || capacity == 0) return;
    text[0] = '\0';
    if (sim == NULL || sim->schema_version < 117U) return;
    size_t used = 0;
    for (int i = 0; i < CC_PERSONAL_WANTS && used + 1 < capacity; ++i) {
        const CcPersonalWant *w = &sim->wants.wants[i];
        if (w->known_day == 0) continue;
        char object[96]; WantName(sim, w, object, sizeof(object));
        const CcCharacter *p = CcSimCharacter(sim, w->person_id);
        const CcSettlement *town = CcSimSettlement(sim, w->home_id);
        int n = snprintf(text + used, capacity - used, "%s: %s. %s%s%s\n",
            p != NULL ? p->name : "A former acquaintance", object,
            w->status == CC_WANT_FULFILLED ? "Helped." : w->status == CC_WANT_SETTLED ? "Supplied by the town." :
            w->status == CC_WANT_CLOSED ? "Request ended." : "Meet at ",
            w->status == CC_WANT_ACTIVE && town != NULL ? town->name : "",
            w->status == CC_WANT_ACTIVE ? "." : "");
        if (n < 0 || (size_t)n >= capacity - used) return;
        used += (size_t)n;
    }
    for (int i = 0; i < CC_BELONGINGS && used + 1 < capacity; ++i) {
        const CcBelonging *item = &sim->wants.items[i]; const CcCustodyEntry *e = ItemEntry(sim, item);
        if (e == NULL || e->holder.kind != CC_CUSTODY_PLAYER) continue;
        int n = snprintf(text + used, capacity - used, "Carrying %s; condition %d.\n", item->name, e->condition);
        if (n < 0 || (size_t)n >= capacity - used) return;
        used += (size_t)n;
    }
    if (used == 0) (void)snprintf(text, capacity, "Ask people what they need. Their requests and your named items will appear here.");
}
bool CcWantsValidate(const CcSim *sim)
{
    if (sim->schema_version < 117U) return true;
    if ((sim->wants.initialized != 1 && sim->wants.initialized != 2) || sim->wants.last_day < 1 || sim->wants.last_day > sim->current_day) return false;
    int carried = 0;
    for (int i = 0; i < CC_BELONGINGS; ++i) {
        const CcBelonging *item = &sim->wants.items[i];
        if (item->id == 0) {
            CcBelonging empty = {0}; if (memcmp(item, &empty, sizeof(empty)) != 0) return false;
            continue;
        }
        const CcCustodyEntry *e = ItemEntry(sim, item);
        if (!Issued(sim, item->id, CC_ENTITY_BELONGING) || e == NULL || !CcWantsCustodyReference(sim, e) ||
            CcSimSettlement(sim, item->home_id) == NULL || CcSimSettlement(sim, item->last_place_id) == NULL ||
            (item->cause_event_id != 0 && !Issued(sim, item->cause_event_id, CC_ENTITY_EVENT)) || item->repair_iron < 0 || item->repair_iron > CC_SIM_MAX_UNITS ||
            item->last_wear_day < 1 || item->last_wear_day > sim->current_day || item->name[0] == '\0' ||
            memchr(item->name, '\0', sizeof(item->name)) == NULL) return false;
        if (e->holder.kind != CC_CUSTODY_CHARACTER && e->holder.kind != CC_CUSTODY_STORE && e->holder.kind != CC_CUSTODY_PLAYER) return false;
        if (e->holder.kind == CC_CUSTODY_CHARACTER && e->holder.id != e->owner_id) return false;
        if (e->holder.kind == CC_CUSTODY_PLAYER && ++carried > CC_BELONGINGS_CARRIED) return false;
        for (int j = 0; j < i; ++j) if (sim->wants.items[j].id == item->id || sim->wants.items[j].custody_id == item->custody_id) return false;
    }
    for (int i = 0; i < CC_PERSONAL_WANTS; ++i) {
        const CcPersonalWant *w = &sim->wants.wants[i];
        if (w->id == 0) {
            CcPersonalWant empty = {0}; if (memcmp(w, &empty, sizeof(empty)) != 0) return false;
            continue;
        }
        if (!Issued(sim, w->id, CC_ENTITY_PERSONAL_WANT) || !Issued(sim, w->person_id, CC_ENTITY_CHARACTER) ||
            w->kind < CC_WANT_MEAL || w->kind > CC_WANT_REPAIR_IRON || w->status < CC_WANT_ACTIVE || w->status > CC_WANT_CLOSED ||
            w->good < 0 || w->good >= CC_GOOD_COUNT || w->quantity < 1 || w->quantity > 2 || w->escrow < 0 || w->escrow > 6 ||
            w->created_day < 1 || w->created_day > sim->current_day || w->known_day < 0 || w->known_day > sim->current_day ||
            w->settled_day < 0 || w->settled_day > sim->current_day || w->revision < 1 ||
            CcSimSettlement(sim, w->home_id) == NULL || CcSimSettlement(sim, w->source_place_id) == NULL ||
            (w->cause_event_id != 0 && !Issued(sim, w->cause_event_id, CC_ENTITY_EVENT))) return false;
        if ((w->status == CC_WANT_ACTIVE) != (w->settled_day == 0 && w->outcome_event_id == 0)) return false;
        if (w->status != CC_WANT_ACTIVE && (w->escrow != 0 || (w->outcome_event_id != 0 && !Issued(sim, w->outcome_event_id, CC_ENTITY_EVENT)))) return false;
        bool named = w->kind >= CC_WANT_RECOVER;
        if (named != (w->item_id != 0) || (named && CcWantsItem(sim, w->item_id) == NULL)) return false;
        if (w->kind == CC_WANT_MEAL && (w->good != CC_GOOD_BREAD || w->quantity != 1)) return false;
        if ((w->kind == CC_WANT_RECOVER || w->kind == CC_WANT_REPAIR) &&
            (w->good != CC_GOOD_TOOLS || w->quantity != 1 || ItemEntry(sim, CcWantsItem(sim, w->item_id))->owner_id != w->person_id)) return false;
        if (w->kind == CC_WANT_REPAIR_IRON && (w->good != CC_GOOD_IRON || w->quantity != 1)) return false;
        if (w->kind == CC_WANT_WORK_SUPPLIES && (w->quantity != 2 ||
            (w->good != CC_GOOD_WHEAT && w->good != CC_GOOD_WOOD && w->good != CC_GOOD_IRON))) return false;
        if ((w->kind == CC_WANT_REPAIR_IRON) != (w->parent_id != 0)) return false;
        if (w->parent_id != 0) {
            const CcPersonalWant *parent = CcWantsFind(sim, w->parent_id);
            if (parent == NULL || parent->parent_id != 0 || parent->kind != CC_WANT_REPAIR || parent->item_id != w->item_id || w->escrow != 0) return false;
        }
        for (int j = 0; j < i; ++j) {
            const CcPersonalWant *other = &sim->wants.wants[j];
            if (other->id == w->id) return false;
            if (w->status == CC_WANT_ACTIVE && other->status == CC_WANT_ACTIVE && w->parent_id == 0 &&
                other->parent_id == 0 && other->person_id == w->person_id) return false;
        }
    }
    return true;
}
