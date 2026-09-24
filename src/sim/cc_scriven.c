#include "sim/cc_scriven.h"
#include "sim/cc_archive_volumes_internal.h"
#include "sim/cc_archive_staff.h"
#include "sim/cc_archive_relocation.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

static int TownSlot(const CcSim *sim, CcId id)
{
    for (int i = 0; i < sim->settlement_count; ++i)
        if (sim->settlements[i].id == id) return i;
    return -1;
}
static CcScrivenBook *Book(CcSim *sim, CcId id)
{
    for (int i = 0; i < CC_SCRIVEN_BOOKS; ++i)
        if (id != 0 && sim->scriven.books[i].id == id) return &sim->scriven.books[i];
    return NULL;
}
const CcScrivenBook *CcScrivenBookById(const CcSim *sim, CcId id)
{
    if (sim == NULL || sim->schema_version < 113U) return NULL;
    for (int i = 0; i < CC_SCRIVEN_BOOKS; ++i)
        if (id != 0 && sim->scriven.books[i].id == id) return &sim->scriven.books[i];
    return NULL;
}
static const CcKingdom *School(const CcSim *sim, CcId id)
{
    for (int i = 0; i < sim->kingdom_count; ++i)
        if (sim->kingdoms[i].id == id) return &sim->kingdoms[i];
    return NULL;
}
static bool Alive(const CcSim *sim, const CcCharacter *person)
{
    return person != NULL && person->death_day > sim->current_day;
}
bool CcScrivenTravelling(const CcSim *sim, CcId person_id)
{
    if (sim == NULL || sim->schema_version < 113U || person_id == 0) return false;
    for (int i = 0; i < CC_SCRIVEN_DELEGATES; ++i) {
        const CcScrivenDelegate *d = &sim->scriven.delegates[i];
        if (d->person_id == person_id && d->phase >= CC_SCRIVEN_EXPEDITION &&
            d->phase <= CC_SCRIVEN_RETURNING) return true;
    }
    return false;
}
bool CcScrivenCarries(const CcSim *sim, CcId book_id, CcId holder_id)
{
    const CcScrivenBook *book = CcScrivenBookById(sim, book_id);
    if (book == NULL || holder_id == 0) return false;
    if (book->borrower_id == holder_id) return true;
    for (int i = 0; i < CC_SCRIVEN_DELEGATES; ++i) {
        const CcScrivenDelegate *d = &sim->scriven.delegates[i];
        if (d->book_id == book_id && d->person_id == holder_id &&
            d->phase >= CC_SCRIVEN_EXPEDITION && d->phase <= CC_SCRIVEN_RETURNING) return true;
    }
    return false;
}
bool CcScrivenReserved(const CcSim *sim, CcId id)
{
    const CcScrivenBook *book = CcScrivenBookById(sim, id);
    if (book == NULL) return false;
    if (book->borrower_id != 0) return true;
    for (int i = 0; i < CC_SCRIVEN_DELEGATES; ++i) {
        const CcScrivenDelegate *d = &sim->scriven.delegates[i];
        if (d->book_id == id && d->phase >= CC_SCRIVEN_EXPEDITION &&
            d->phase <= CC_SCRIVEN_RETURNING) return true;
    }
    return false;
}
bool CcScrivenBookAccessible(const CcSim *sim, CcId id, CcId place_id)
{
    const CcTreasure *t = CcSimTreasure(sim, id);
    const CcScrivenBook *b = CcScrivenBookById(sim, id);
    if (t == NULL || b == NULL || t->destroyed) return false;
    if (b->borrower_id == sim->player.id)
        return !sim->journey.active && place_id == sim->player.location_id;
    if (t->location_id == place_id) return true;
    for (int i = 0; i < CC_SCRIVEN_DELEGATES; ++i) {
        const CcScrivenDelegate *d = &sim->scriven.delegates[i];
        if (d->book_id == id && d->place_id == place_id && d->route_id == 0 &&
            (d->phase == CC_SCRIVEN_ATTENDING || d->phase == CC_SCRIVEN_FALLEN)) return true;
    }
    return false;
}
void CcScrivenFreeze(CcSim *sim, CcId id)
{
    if (sim == NULL || sim->schema_version < 113U || Book(sim, id) != NULL) return;
    for (int i = 0; i < sim->treasure_count; ++i) {
        const CcTreasure *t = &sim->treasures[i];
        if (t->id != id || !CcArchiveVolumeIsLive(t)) continue;
        CcScrivenBook fresh = {0};
        for (int page = 0; page < 3; ++page)
            (void)CcSimTomePassage(sim, id, page, fresh.passages[page], CC_SCRIVEN_TEXT);
        fresh.id = id; fresh.source_id = id; fresh.edition_day = sim->current_day;
        fresh.condition = 100;
        const CcSettlement *town = CcSimSettlement(sim, t->maker_settlement_id);
        fresh.school_id = town != NULL ? town->kingdom_id : 0;
        sim->scriven.books[i] = fresh;
        if (sim->schema_version >= 115U) sim->crown_calendar.book_editions[i] = 0;
        return;
    }
}
void CcScrivenRebind(CcSim *sim, const int32_t slots[4], CcId new_id)
{
    if (sim->schema_version < 113U) return;
    CcScrivenBook combined = sim->scriven.books[slots[0]];
    combined.id = new_id; combined.edition_day = sim->current_day;
    combined.borrower_id = 0; combined.return_place_id = 0; combined.loan_due_day = 0;
    combined.condition = 100;
    memset(combined.notes, 0, sizeof(combined.notes));
    CcId dragon = 0; int latest = 0;
    uint64_t crown_edition = 0;
    for (int i = 0; i < 4; ++i) {
        const CcScrivenBook *old = &sim->scriven.books[slots[i]];
        for (int j = 0; j < 2; ++j)
            if (old->notes[j].day > latest) { latest = old->notes[j].day; dragon = old->notes[j].dragon_id; }
        if (old->almanac_id > combined.almanac_id) combined.almanac_id = old->almanac_id;
        if (sim->schema_version >= 115U && sim->crown_calendar.book_editions[slots[i]] > crown_edition)
            crown_edition = sim->crown_calendar.book_editions[slots[i]];
    }
    for (int i = 0; i < 4; ++i) {
        const CcScrivenBook *old = &sim->scriven.books[slots[i]];
        if (i < 3) (void)snprintf(combined.passages[i], CC_SCRIVEN_TEXT, "%s", old->passages[0]);
        for (int j = 0; j < CC_SCRIVEN_NOTES; ++j) {
            const CcScrivenNote *n = &old->notes[j];
            CcScrivenNote *kept = &combined.notes[j];
            if (n->kind == 0 || (j < 2 && n->dragon_id != dragon)) continue;
            if (kept->kind == 0 || (j == 1 || (j == 0 && sim->schema_version >= 115U) ? n->day < kept->day : n->day > kept->day)) *kept = *n;
        }
    }
    for (int i = 0; i < 4; ++i) sim->scriven.books[slots[i]] = (CcScrivenBook){0};
    sim->scriven.books[slots[0]] = combined;
    if (sim->schema_version >= 115U) {
        for (int i = 0; i < 4; ++i) sim->crown_calendar.book_editions[slots[i]] = 0;
        sim->crown_calendar.book_editions[slots[0]] = crown_edition;
    }
    /* Empty legacy margins receive an honest snapshot at the time of rebinding. */
    if (combined.source_id == 0) {
        sim->scriven.books[slots[0]] = (CcScrivenBook){0};
        CcScrivenFreeze(sim, new_id);
    }
}
/* A road leg is a real route. Closed roads and war borders can delay a visit. */
static bool Path(const CcSim *sim, CcId from, CcId to, int *first, CcId *hop, int *days)
{
    int start = TownSlot(sim, from), end = TownSlot(sim, to);
    if (start < 0 || end < 0) return false;
    int distance[CC_MAX_SETTLEMENTS], edge[CC_MAX_SETTLEMENTS];
    CcId next[CC_MAX_SETTLEMENTS] = {0};
    bool done[CC_MAX_SETTLEMENTS] = {false};
    for (int i = 0; i < sim->settlement_count; ++i) { distance[i] = INT_MAX; edge[i] = -1; }
    distance[start] = 0;
    for (int step = 0; step < sim->settlement_count; ++step) {
        int at = -1;
        for (int i = 0; i < sim->settlement_count; ++i)
            if (!done[i] && distance[i] < INT_MAX && (at < 0 || distance[i] < distance[at])) at = i;
        if (at < 0) break;
        done[at] = true;
        for (int r = 0; r < sim->route_count; ++r) {
            const CcRoute *road = &sim->routes[r];
            if (road->closed || road->condition <= 0 || CcSimRouteCrossesWarBorder(sim, road->id)) continue;
            CcId here = sim->settlements[at].id;
            CcId other = road->from_id == here ? road->to_id : road->to_id == here ? road->from_id : 0;
            int slot = TownSlot(sim, other);
            if (slot < 0 || done[slot] || CcSettlementIsAbandoned(&sim->settlements[slot])) continue;
            int cost = distance[at] + road->travel_days;
            if (cost < distance[slot]) {
                distance[slot] = cost;
                edge[slot] = at == start ? r : edge[at];
                next[slot] = at == start ? other : next[at];
            }
        }
    }
    if (distance[end] == INT_MAX) return false;
    if (first != NULL) *first = edge[end];
    if (hop != NULL) *hop = next[end];
    if (days != NULL) *days = distance[end];
    return true;
}
static CcCharacter *Scribe(CcSim *sim, CcId town_id)
{
    for (int i = 0; i < sim->character_count; ++i) {
        CcCharacter *p = &sim->characters[i];
        if (!Alive(sim, p) || p->occupation != CC_OCCUPATION_SCRIBE ||
            p->current_settlement_id != town_id || p->travel_destination_id != 0 ||
            CcScrivenTravelling(sim, p->id) || p->bandit_group_id != 0 ||
            CcCharacterAgeYears(sim, p) < 16 || p->activity != CC_CHARACTER_ACTIVITY_WORKING) continue;
        if (sim->archive_recruitment.status > 0 &&
            (p->id == sim->archive_recruitment.person_id || p->id == sim->archive_recruitment.trainer_id)) continue;
        if (CcSimArchiveStaffMember(sim, p->id) && CcSimArchiveStaffCount(sim) <= 1) continue;
        return p;
    }
    return NULL;
}
static CcTreasure *NewBook(CcSim *sim, CcSettlement *town)
{
    if (town == NULL || CcSettlementIsAbandoned(town) || town->stock[CC_GOOD_PAPER] < 1 ||
        town->stock[CC_GOOD_WHEAT] < 1 || town->stock[CC_GOOD_TOOLS] < 1) return NULL;
    int slot = -1;
    for (int i = 0; i < sim->treasure_count; ++i)
        if (sim->treasures[i].destroyed && !CcScrivenReserved(sim, sim->treasures[i].id)) { slot = i; break; }
    if (slot < 0 && sim->treasure_count < CC_MAX_TREASURES) slot = sim->treasure_count++;
    if (slot < 0) return NULL;
    CcTreasure *t = &sim->treasures[slot];
    *t = (CcTreasure){.id = CcMakeId(CC_ENTITY_TREASURE, sim->next_entity_serial++),
        .maker_settlement_id = town->id, .owner_id = town->id, .location_id = town->id,
        .craft_work = 1, .appraised_value = 2, .created_day = sim->current_day};
    (void)snprintf(t->name, sizeof(t->name), "Annal of %.24s %d", town->name, CcCalendar(sim->current_day).year);
    --town->stock[CC_GOOD_PAPER]; --town->stock[CC_GOOD_WHEAT];
    CcScrivenFreeze(sim, t->id);
    sim->archives.lore_stored = CcSimArchivePhysicalLore(sim);
    return t;
}
static CcTreasure *LocalBook(CcSim *sim, CcId town_id)
{
    for (int i = 0; i < sim->treasure_count; ++i) {
        CcTreasure *t = &sim->treasures[i];
        if (!CcArchiveVolumeIsLive(t) || t->owner_id != town_id || t->location_id != town_id ||
            CcScrivenReserved(sim, t->id) || CcSimArchiveConvoyHoldsBook(sim, t->id)) continue;
        CcScrivenFreeze(sim, t->id);
        const CcScrivenBook *book = Book(sim, t->id);
        if (sim->schema_version >= 115U && book != NULL &&
            ((book->notes[0].kind != 0 && book->notes[0].dragon_id != sim->dragon.id) ||
             (book->notes[1].kind != 0 && book->notes[1].dragon_id != sim->dragon.id))) continue;
        return t;
    }
    return NewBook(sim, CcSimSettlementMutable(sim, town_id));
}
#include "sim/cc_crown_calendar.inc"

static void Observe(CcSim *sim, CcScrivenBook *b, CcId author, CcId place)
{
    if (b == NULL || place != sim->dragon.lair_settlement_id || sim->dragon.slain) return;
    int kind = sim->dragon.life_stage == CC_DRAGON_STAGE_DEEP_WYRM ?
        CC_SCRIVEN_NOTE_DEEP : sim->dragon.life_stage == CC_DRAGON_STAGE_CROWNED ? CC_SCRIVEN_NOTE_CROWNED : 0;
    if (kind > 0 && b->notes[kind - 1].kind == 0) {
        CcScrivenNote *n = &b->notes[kind - 1];
        *n = (CcScrivenNote){.dragon_id = sim->dragon.id, .author_id = author,
            .place_id = place, .source_book_id = b->id, .day = sim->current_day, .kind = kind};
        (void)snprintf(n->text, sizeof(n->text), "Day %d: %.31s was seen as %s at the lair.",
            n->day, sim->dragon.name, kind == CC_SCRIVEN_NOTE_DEEP ? "a Deep Wyrm" : "a Crowned Dragon");
        CrownRecord(sim, n);
        if (author == sim->player.id) CrownRead(sim, b);
    }
    /* A visible porter tally supports context, rather than supplying a secret date. */
    if (b->notes[2].kind == 0) {
        CcScrivenNote *n = &b->notes[2];
        int carriers = 0;
        for (int i = 0; i < CC_GOBLIN_FACTION_COUNT; ++i)
            if (sim->goblin_politics.factions[i].members > 0 &&
                sim->goblin_politics.factions[i].porter_room == 19) ++carriers;
        *n = (CcScrivenNote){.dragon_id = sim->dragon.id, .author_id = author,
            .place_id = place, .source_book_id = b->id, .day = sim->current_day,
            .kind = CC_SCRIVEN_NOTE_GOBLINS, .value = carriers};
        (void)snprintf(n->text, sizeof(n->text), "Day %d: porters from %d goblin factions carried tribute near the lair.", n->day, carriers);
    }
}
void CcScrivenAnchor(CcSim *sim)
{
    if (sim == NULL || sim->schema_version < 113U) return;
    for (int i = 0; i < sim->scriven.age_count; ++i)
        if (sim->scriven.ages[i].dragon_id == sim->dragon.id) return;
    if (sim->scriven.age_count == CC_SCRIVEN_AGES) return;
    sim->scriven.ages[sim->scriven.age_count++] = (CcScrivenAge){sim->dragon.id, sim->current_day};
}
void CcScrivenInit(CcSim *sim)
{
    memset(&sim->scriven, 0, sizeof(sim->scriven));
    sim->scriven.meeting_year = CcCalendar(sim->current_day).year - 1;
    for (int i = 0; i < CC_MAX_SETTLEMENTS; ++i) sim->scriven.last_hosted[i] = -1;
    for (int i = 0; i < sim->treasure_count; ++i) CcScrivenFreeze(sim, sim->treasures[i].id);
    CcCrownCalendarInit(sim);
    (void)snprintf(sim->scriven.report, sizeof(sim->scriven.report),
        "Scrivendays fall on Quill 1-7. Scribes carry their tomes to compare dates and plan expeditions.");
}
static bool Viable(const CcSettlement *t)
{
    return t != NULL && !CcSettlementIsAbandoned(t) && t->hunger < 40 &&
        t->security >= 20 && t->stock[CC_GOOD_WHEAT] >= 6 && t->stock[CC_GOOD_PAPER] >= 2 &&
        t->stock[CC_GOOD_TOOLS] > 0;
}
static void Convene(CcSim *sim)
{
    CcScrivenState *s = &sim->scriven;
    int host = -1, best_cost = INT_MAX;
    for (int i = 0; i < sim->settlement_count; ++i) {
        if (!Viable(&sim->settlements[i])) continue;
        int cost = 0, reachable = 0;
        for (int j = 0; j < sim->settlement_count; ++j) {
            int days = 0;
            if (Scribe(sim, sim->settlements[j].id) != NULL &&
                Path(sim, sim->settlements[j].id, sim->settlements[i].id, NULL, NULL, &days)) {
                cost += days; ++reachable;
            }
        }
        if (reachable < 2) continue;
        if (host < 0 || s->last_hosted[i] < s->last_hosted[host] ||
            (s->last_hosted[i] == s->last_hosted[host] && cost < best_cost)) { host = i; best_cost = cost; }
    }
    s->meeting_year = CcCalendar(sim->current_day).year;
    int64_t opens = (int64_t)s->meeting_year * CC_SOLAR_DAYS + CC_SCRIVENDAYS_PHASE;
    if (opens + 6 > CC_SIM_MAX_DAY) return;
    s->opens_day = (int32_t)opens; s->closes_day = s->opens_day + 6;
    s->status = host < 0 ? 4 : 1;
    s->host_id = host < 0 ? 0 : sim->settlements[host].id;
    memset(s->notice_arrives, 0, sizeof(s->notice_arrives));
    if (host < 0) {
        (void)snprintf(s->report, sizeof(s->report), "This year's gathering awaits a host with food, paper, and open roads.");
        return;
    }
    for (int i = 0; i < sim->settlement_count; ++i) {
        int days = 0;
        if (Path(sim, s->host_id, sim->settlements[i].id, NULL, NULL, &days) && days <= 28)
            s->notice_arrives[i] = sim->current_day + days;
    }
    (void)snprintf(s->report, sizeof(s->report), "%s will host Scrivendays on Quill 1-7. Bring an owned or borrowed tome.", sim->settlements[host].name);
}
static void Invite(CcSim *sim, int town_slot)
{
    CcScrivenState *s = &sim->scriven;
    CcSettlement *town = &sim->settlements[town_slot];
    for (int i = 0; i < CC_SCRIVEN_DELEGATES; ++i)
        if (s->delegates[i].home_id == town->id && s->delegates[i].notice_day >= s->opens_day - 56) return;
    CcCharacter *person = Scribe(sim, town->id);
    int slot = -1;
    for (int i = 0; i < CC_SCRIVEN_DELEGATES; ++i)
        if (s->delegates[i].phase == CC_SCRIVEN_IDLE || s->delegates[i].phase == CC_SCRIVEN_FINISHED || s->delegates[i].phase == CC_SCRIVEN_FALLEN) { slot = i; break; }
    int days = 0;
    if (person == NULL || slot < 0 || town->stock[CC_GOOD_WHEAT] < 5 ||
        !Path(sim, town->id, s->host_id, NULL, NULL, &days) || sim->current_day + days > s->closes_day) return;
    CcTreasure *tome = LocalBook(sim, town->id);
    if (tome == NULL) return;
    CcScrivenDelegate *d = &s->delegates[slot];
    *d = (CcScrivenDelegate){.person_id = person->id, .book_id = tome->id, .home_id = town->id,
        .place_id = town->id, .phase = CC_SCRIVEN_OUTWARD, .wheat = 4, .notice_day = sim->current_day};
    int expedition = 0, onward = 0;
    if (Path(sim, town->id, sim->dragon.lair_settlement_id, NULL, NULL, &expedition) &&
        Path(sim, sim->dragon.lair_settlement_id, s->host_id, NULL, NULL, &onward) &&
        sim->current_day + expedition + onward + 2 <= s->opens_day)
        d->phase = CC_SCRIVEN_EXPEDITION;
    town->stock[CC_GOOD_WHEAT] -= 4;
    tome->location_id = person->id;
    (void)CcSimActivateCharacter(sim, person->id);
    person->activity = CC_CHARACTER_ACTIVITY_PREPARING;
}
static void Travel(CcSim *sim, CcScrivenDelegate *d)
{
    if (d->phase < CC_SCRIVEN_EXPEDITION || d->phase > CC_SCRIVEN_RETURNING) return;
    CcCharacter *p = (CcCharacter *)CcSimCharacter(sim, d->person_id);
    CcTreasure *t = (CcTreasure *)CcSimTreasure(sim, d->book_id);
    if (!Alive(sim, p) || t == NULL || t->destroyed) {
        if (t != NULL && t->location_id == d->person_id) t->location_id = d->place_id;
        if (p != NULL) { p->travel_destination_id = 0; p->travel_arrival_day = 0; }
        d->phase = CC_SCRIVEN_FALLEN; d->route_id = 0; d->hop_id = 0;
        ++sim->scriven.failed_trips;
        return;
    }
    if (d->route_id != 0) {
        if (sim->current_day < d->arrival_day) return;
        d->place_id = d->hop_id; p->current_settlement_id = d->hop_id;
        d->route_id = 0; d->hop_id = 0; d->arrival_day = 0;
        p->travel_destination_id = 0; p->travel_arrival_day = 0;
    }
    if ((sim->current_day - d->notice_day) % 7 == 0) {
        if (d->wheat > 0) { --d->wheat; ++d->spent; p->hungry_days = 0; }
        else {
            CcSettlement *town = CcSimSettlementMutable(sim, d->place_id);
            CcMoney price = town != NULL && town->price[CC_GOOD_WHEAT] > 0 ? town->price[CC_GOOD_WHEAT] : 1;
            if (town != NULL && town->stock[CC_GOOD_WHEAT] > 0 && p->travel_coins >= price &&
                town->market_coins <= CC_SIM_MAX_MONEY - price) {
                --town->stock[CC_GOOD_WHEAT]; town->market_coins += price;
                p->travel_coins -= price; ++d->spent; p->hungry_days = 0;
            } else p->hungry_days = 7;
        }
    }
    if (d->phase != CC_SCRIVEN_RETURNING && sim->current_day > sim->scriven.closes_day)
        d->phase = CC_SCRIVEN_RETURNING;
    if (d->phase == CC_SCRIVEN_EXPEDITION && d->place_id == sim->dragon.lair_settlement_id) {
        Observe(sim, Book(sim, d->book_id), d->person_id, d->place_id);
        d->phase = CC_SCRIVEN_OUTWARD;
    }
    CcId target = d->phase == CC_SCRIVEN_EXPEDITION ? sim->dragon.lair_settlement_id :
        d->phase == CC_SCRIVEN_RETURNING ? d->home_id : sim->scriven.host_id;
    if (d->place_id == target) {
        if (d->phase == CC_SCRIVEN_RETURNING) {
            CcSettlement *home = CcSimSettlementMutable(sim, d->home_id);
            if (home != NULL && home->stock[CC_GOOD_WHEAT] <= CC_SIM_MAX_UNITS - d->wheat) {
                home->stock[CC_GOOD_WHEAT] += d->wheat; d->wheat = 0;
            }
            t->location_id = d->home_id;
            p->activity = CC_CHARACTER_ACTIVITY_WORKING;
            int home_slot = TownSlot(sim, d->home_id);
            if (home_slot >= 0 && d->edition_id != 0 &&
                d->edition_id <= (uint64_t)sim->scriven.editions)
                sim->scriven.local[home_slot] = sim->scriven.almanacs[d->edition_id - 1];
            uint64_t crown_edition = CrownEdition(sim, Book(sim, d->book_id));
            if (home_slot >= 0 && crown_edition > 0 && crown_edition <= (uint64_t)sim->crown_calendar.editions)
                sim->crown_calendar.local[home_slot] = sim->crown_calendar.almanacs[crown_edition - 1];
            d->phase = CC_SCRIVEN_FINISHED; ++sim->scriven.returns;
        } else {
            d->phase = CC_SCRIVEN_ATTENDING; p->activity = CC_CHARACTER_ACTIVITY_PREPARING;
        }
        return;
    }
    int route = -1, days = 0; CcId hop = 0;
    if (!Path(sim, d->place_id, target, &route, &hop, &days) || route < 0) return;
    int leg_days = sim->routes[route].travel_days;
    int food = (leg_days + 6) / 7;
    if (food > 11 || sim->current_day > CC_SIM_MAX_DAY - leg_days) return;
    if (d->wheat < food) {
        CcSettlement *town = CcSimSettlementMutable(sim, d->place_id);
        int need = food - d->wheat;
        int price = town != NULL && town->price[CC_GOOD_WHEAT] > 0 ? town->price[CC_GOOD_WHEAT] : 1;
        CcMoney cost = (CcMoney)need * price;
        if (town == NULL || town->stock[CC_GOOD_WHEAT] < need || p->travel_coins < cost ||
            town->market_coins > CC_SIM_MAX_MONEY - cost) return;
        town->stock[CC_GOOD_WHEAT] -= need; town->market_coins += cost;
        p->travel_coins -= cost; d->wheat += need;
    }
    d->wheat -= food; d->spent += food;
    d->route_id = sim->routes[route].id; d->hop_id = hop;
    d->arrival_day = sim->current_day + leg_days;
    p->travel_destination_id = hop; p->travel_arrival_day = d->arrival_day;
    p->activity = CC_CHARACTER_ACTIVITY_TRAVELLING;
    CcScrivenBook *b = Book(sim, d->book_id);
    if (b != NULL && b->condition > 1) --b->condition;
}
/* Only passages physically at the hearing enter this calculation. */
static bool DeepHearing(CcSim *sim)
{
    CcScrivenState *s = &sim->scriven;
    CcScrivenFinding f = {0};
    int newest = 0;
    for (int i = 0; i < CC_SCRIVEN_BOOKS; ++i)
        if (CcScrivenBookAccessible(sim, s->books[i].id, s->host_id)) ++s->comparisons;
    for (int b = 0; b < CC_SCRIVEN_BOOKS; ++b) {
        const CcScrivenBook *book = &s->books[b];
        if (!CcScrivenBookAccessible(sim, book->id, s->host_id)) continue;
        for (int n = 0; n < CC_SCRIVEN_NOTES; ++n) {
            const CcScrivenNote *note = &book->notes[n];
            if (note->kind == CC_SCRIVEN_NOTE_DEEP && note->day > newest) {
                newest = note->day; f.dragon_id = note->dragon_id;
            }
        }
    }
    if (f.dragon_id == 0) {
        (void)snprintf(s->report, sizeof(s->report), "The scribes compare their tomes. An expedition must bring a dated sighting of a Deep Wyrm.");
        return false;
    }
    f.latest_day = INT_MAX;
    int latest_crowned = 0, first_deep = INT_MAX;
    int best_span = INT_MAX;
    for (int b = 0; b < CC_SCRIVEN_BOOKS; ++b) {
        const CcScrivenBook *book = &s->books[b];
        if (!CcScrivenBookAccessible(sim, book->id, s->host_id)) continue;
        for (int n = 0; n < CC_SCRIVEN_NOTES; ++n) {
            const CcScrivenNote *note = &book->notes[n];
            if (note->dragon_id != f.dragon_id) continue;
            if (note->kind == CC_SCRIVEN_NOTE_DEEP && note->day < first_deep) first_deep = note->day;
            if (note->kind != CC_SCRIVEN_NOTE_CROWNED) continue;
            if (note->day > latest_crowned) latest_crowned = note->day;
            for (int other = 0; other < CC_SCRIVEN_BOOKS; ++other) {
                const CcScrivenBook *after = &s->books[other];
                const CcScrivenNote *deep = &after->notes[CC_SCRIVEN_NOTE_DEEP - 1];
                if (!CcScrivenBookAccessible(sim, after->id, s->host_id) ||
                    deep->kind != CC_SCRIVEN_NOTE_DEEP || deep->dragon_id != f.dragon_id ||
                    deep->source_book_id == note->source_book_id || deep->day <= note->day) continue;
                int span = deep->day - note->day - 1;
                if (span >= best_span) continue;
                best_span = span;
                f.earliest_day = note->day + 1; f.latest_day = deep->day;
                f.book_ids[0] = book->id; f.school_ids[0] = book->school_id;
                f.book_ids[1] = after->id; f.school_ids[1] = after->school_id;
                (void)snprintf(f.citations[0], CC_SCRIVEN_TEXT, "%s", note->text);
                (void)snprintf(f.citations[1], CC_SCRIVEN_TEXT, "%s", deep->text);
            }
        }
    }
    CcId school_ids[CC_SCRIVEN_DELEGATES] = {0};
    for (int i = 0; i < CC_SCRIVEN_DELEGATES; ++i) {
        const CcScrivenDelegate *d = &s->delegates[i];
        const CcScrivenBook *b = CcScrivenBookById(sim, d->book_id);
        const CcCharacter *person = CcSimCharacter(sim, d->person_id);
        if (!Alive(sim, person) || person->current_settlement_id != s->host_id || person->travel_destination_id != 0 ||
            d->phase != CC_SCRIVEN_ATTENDING || d->place_id != s->host_id || b == NULL) continue;
        bool duplicate = false;
        for (int j = 0; j < f.schools; ++j) duplicate |= school_ids[j] == b->school_id;
        if (!duplicate) school_ids[f.schools++] = b->school_id;
        f.signers[i] = d->person_id;
    }
    if (f.book_ids[0] == 0 || f.book_ids[1] == 0 || latest_crowned >= first_deep ||
        f.earliest_day > f.latest_day || f.schools < 2) {
        (void)snprintf(s->report, sizeof(s->report), "The date remains disputed. Bring independent dated sightings from before and after the change, with scribes from two schools.");
        return false;
    }
    f.votes = f.schools;
    f.proposed_day = f.earliest_day + (f.latest_day - f.earliest_day) / 2;
    f.agreed_day = sim->current_day;
    if (s->finding.dragon_id == f.dragon_id && s->finding.earliest_day == f.earliest_day &&
        s->finding.latest_day == f.latest_day) f = s->finding;
    else {
        if (s->editions >= 32) {
            (void)snprintf(s->report, sizeof(s->report), "The almanac shelf is full. Earlier editions remain available for comparison.");
            return false;
        }
        s->almanacs[s->editions++] = f;
        s->finding = f;
    }
    int host = TownSlot(sim, s->host_id);
    if (host >= 0) s->local[host] = f;
    CcSettlement *host_town = CcSimSettlementMutable(sim, s->host_id);
    for (int i = 0; i < CC_SCRIVEN_DELEGATES; ++i) {
        CcScrivenDelegate *d = &s->delegates[i];
        CcScrivenBook *b = Book(sim, d->book_id);
        if (d->phase == CC_SCRIVEN_ATTENDING && b != NULL && host_town != NULL &&
            (sim->schema_version < 115U || CrownAttends(sim, d)) &&
            b->almanac_id != (uint64_t)s->editions && host_town->stock[CC_GOOD_PAPER] > 0) {
            --host_town->stock[CC_GOOD_PAPER];
            b->almanac_id = (uint64_t)s->editions;
            d->edition_id = b->almanac_id;
        }
    }
    (void)snprintf(s->report, sizeof(s->report), "The scribes adopt day %d as an estimate. Their cited range is days %d-%d. %d schools sign the almanac.",
        f.proposed_day, f.earliest_day, f.latest_day, f.schools);
    return true;
}
static bool Hearing(CcSim *sim)
{
    bool deep = DeepHearing(sim);
    if (sim->schema_version < 115U) return deep;
    bool crown = CrownHearing(sim);
    if (!crown && !deep)
        (void)snprintf(sim->scriven.report, sizeof(sim->scriven.report),
            "Bring two independent Crowned Dragon sightings and scribes from two schools to date the Crown Age. Deep Wyrm epochs need evidence from before and after the change.");
    else if (deep && !crown)
        (void)snprintf(sim->scriven.report, sizeof(sim->scriven.report),
            "The Deep Wyrm Epoch is dated to day %d, within days %d-%d. The Crown Age keeps its first sighting date.",
            sim->scriven.finding.proposed_day, sim->scriven.finding.earliest_day, sim->scriven.finding.latest_day);
    return deep || crown;
}
void CcScrivenAdvance(CcSim *sim)
{
    if (sim == NULL || sim->schema_version < 113U) return;
    CcScrivenState *s = &sim->scriven;
    CcCalendarDate date = CcCalendar(sim->current_day);
    int phase = date.day_of_year - 1;
    if (phase >= CC_SCRIVENDAYS_PHASE - 56 && phase <= CC_SCRIVENDAYS_PHASE && s->meeting_year < date.year)
        Convene(sim);
    if (s->status == 1 || s->status == 2) {
        for (int i = 0; i < sim->settlement_count; ++i)
            if (s->notice_arrives[i] > 0 && s->notice_arrives[i] <= sim->current_day && sim->current_day <= s->opens_day)
                Invite(sim, i);
        if (s->status == 1 && sim->current_day >= s->opens_day) {
            const CcSettlement *host = CcSimSettlement(sim, s->host_id);
            if (Viable(host)) {
                s->status = 2; ++s->meetings;
                int slot = TownSlot(sim, s->host_id);
                if (slot >= 0) s->last_hosted[slot] = date.year;
            } else {
                s->status = 4;
                (void)snprintf(s->report, sizeof(s->report), "The host's stores failed. The scribes are taking their tomes home.");
                for (int i = 0; i < CC_SCRIVEN_DELEGATES; ++i)
                    if (s->delegates[i].phase >= CC_SCRIVEN_EXPEDITION && s->delegates[i].phase <= CC_SCRIVEN_RETURNING)
                        s->delegates[i].phase = CC_SCRIVEN_RETURNING;
            }
        }
    }
    for (int i = 0; i < CC_SCRIVEN_DELEGATES; ++i) Travel(sim, &s->delegates[i]);
    if (s->status == 2 && sim->current_day >= s->closes_day) {
        (void)Hearing(sim); s->status = 3;
    }
}
static bool Fail(char *error, size_t capacity, const char *message)
{
    if (error != NULL && capacity > 0) (void)snprintf(error, capacity, "%s", message);
    return false;
}
bool CcScrivenApply(CcSim *sim, const CcCommand *command, char *error, size_t capacity)
{
    if (sim->schema_version < 113U || sim->journey.active || sim->dungeon_expedition.active || sim->mine.phase != CC_MINE_NONE)
        return Fail(error, capacity, "Visit a settlement to consult the scribes.");
    CcScrivenState *s = &sim->scriven;
    CcId here = sim->player.location_id;
    CcTreasure *t = (CcTreasure *)CcSimTreasure(sim, command->target_id);
    CcScrivenBook *b = Book(sim, command->target_id);
    bool accessible = b != NULL && CcScrivenBookAccessible(sim, b->id, here);
    if ((command->amount == CC_SCRIVEN_COMMISSION || command->amount == CC_SCRIVEN_COPY ||
         command->amount == CC_SCRIVEN_OBSERVE) && sim->current_day == CC_SIM_MAX_DAY)
        return Fail(error, capacity, "This work needs one more day on the calendar.");
    switch ((CcScrivenAction)command->amount) {
    case CC_SCRIVEN_READ:
        if (!accessible) return Fail(error, capacity, "Bring the tome here to read its saved passages.");
        s->player_read_book = b->id; s->player_read_day = sim->current_day;
        (void)snprintf(s->report, sizeof(s->report), "%s", b->passages[0]);
        break;
    case CC_SCRIVEN_BORROW:
        if (!accessible || t == NULL || CcSimSettlement(sim, t->owner_id) == NULL || b->borrower_id != 0 ||
            CcScrivenReserved(sim, b->id) || CcSimArchiveConvoyHoldsBook(sim, b->id))
            return Fail(error, capacity, "Choose a town tome whose owner can lend it today.");
        if (CcPlayerCargoUsed(&sim->player) >= sim->player.cargo_capacity || sim->current_day > CC_SIM_MAX_DAY - 364)
            return Fail(error, capacity, "Make one cargo space for the borrowed tome.");
        b->borrower_id = sim->player.id; b->return_place_id = t->owner_id;
        b->loan_due_day = sim->current_day + 364; t->location_id = sim->player.id;
        ++sim->player.treasure_cargo_slots;
        (void)snprintf(s->report, sizeof(s->report), "The owner lends %.47s for one solar year. Return this same tome. Reading and copying are permitted.", t->name);
        break;
    case CC_SCRIVEN_RETURN:
        if (b == NULL || t == NULL || b->borrower_id != sim->player.id || here != b->return_place_id)
            return Fail(error, capacity, "Take the borrowed tome back to the town named in its loan.");
        t->location_id = here; b->borrower_id = 0; b->return_place_id = 0; b->loan_due_day = 0;
        --sim->player.treasure_cargo_slots; ++s->returns;
        (void)snprintf(s->report, sizeof(s->report), "The owner receives the same tome and closes the loan.");
        break;
    case CC_SCRIVEN_OBSERVE:
        if (!accessible || t == NULL || (t->owner_id != sim->player.id && b->borrower_id != sim->player.id) ||
            here != sim->dragon.lair_settlement_id || sim->dragon.slain)
            return Fail(error, capacity, "Bring your own or a borrowed tome to the dragon's lair to record a sighting and the goblin porters.");
        if ((sim->dragon.life_stage == CC_DRAGON_STAGE_CROWNED && b->notes[0].kind != 0) ||
            (sim->dragon.life_stage == CC_DRAGON_STAGE_DEEP_WYRM && b->notes[1].kind != 0) ||
            (b->notes[2].kind != 0 && sim->dragon.life_stage != CC_DRAGON_STAGE_CROWNED &&
             sim->dragon.life_stage != CC_DRAGON_STAGE_DEEP_WYRM))
            return Fail(error, capacity, "This tome already holds that kind of sighting. Bring a fresh field tome.");
        Observe(sim, b, sim->player.id, here); s->player_observed_day = sim->current_day;
        CcSimAdvanceDays(sim, 1);
        (void)snprintf(s->report, sizeof(s->report), "The company writes what it saw today in the tome's margins.");
        break;
    case CC_SCRIVEN_HEARING:
        if (s->status != 2 || here != s->host_id || sim->current_day < s->opens_day || sim->current_day > s->closes_day)
            return Fail(error, capacity, "Visit the host during Quill 1-7 to attend the hearing.");
        (void)Hearing(sim);
        CrownCarryFromHearing(sim);
        /* The company carries a written edition in an actual tome. */
        CcSettlement *host_town = CcSimSettlementMutable(sim, here);
        if (s->finding.agreed_day > 0 && host_town != NULL) {
            for (int i = 0; i < CC_SCRIVEN_BOOKS; ++i) {
                CcScrivenBook *held = &s->books[i];
                const CcTreasure *owned = CcSimTreasure(sim, held->id);
                if (owned != NULL && !owned->destroyed && (owned->owner_id == sim->player.id || held->borrower_id == sim->player.id) &&
                    (sim->schema_version < 115U || CcScrivenBookAccessible(sim, held->id, here)) &&
                    held->almanac_id != (uint64_t)s->editions && host_town->stock[CC_GOOD_PAPER] > 0) {
                    --host_town->stock[CC_GOOD_PAPER]; held->almanac_id = (uint64_t)s->editions;
                    s->company = s->finding; break;
                }
            }
        }
        break;
    case CC_SCRIVEN_COPY: {
        if (!accessible) return Fail(error, capacity, "Bring the source tome to a town with paper, wheat, and tools.");
        CcSettlement *town = CcSimSettlementMutable(sim, here);
        CcScrivenBook source = *b;
        uint64_t crown_edition = CrownEdition(sim, b);
        if (CcPlayerCargoUsed(&sim->player) >= sim->player.cargo_capacity || town == NULL || sim->player.coins < 2 || town->market_coins > CC_SIM_MAX_MONEY - 2)
            return Fail(error, capacity, "Copying needs two crowns and one free cargo space.");
        CcTreasure *copy = NewBook(sim, town);
        if (copy == NULL) return Fail(error, capacity, "The scribe needs paper, wheat, tools, and room for another tome.");
        CcId copy_id = copy->id;
        CcScrivenBook *copy_book = Book(sim, copy_id);
        *copy_book = source; copy_book->id = copy_id; copy_book->edition_day = sim->current_day;
        copy_book->borrower_id = 0; copy_book->return_place_id = 0; copy_book->loan_due_day = 0; copy_book->condition = 100;
        if (sim->schema_version >= 115U) sim->crown_calendar.book_editions[copy_book - s->books] = crown_edition;
        copy->owner_id = sim->player.id; ++sim->player.treasure_cargo_slots;
        sim->player.coins -= 2; town->market_coins += 2;
        CcSimAdvanceDays(sim, 1);
        (void)snprintf(s->report, sizeof(s->report), "The copy keeps the source passages and their origin. The loan still names the original tome.");
        break;
    }
    case CC_SCRIVEN_DELIVER: {
        int town = TownSlot(sim, here);
        bool carrying = false;
        for (int i = 0; i < CC_SCRIVEN_BOOKS; ++i) {
            const CcScrivenBook *held = &s->books[i];
            const CcTreasure *owned = CcSimTreasure(sim, held->id);
            if (owned != NULL && !owned->destroyed && held->almanac_id > 0 &&
                (owned->owner_id == sim->player.id || held->borrower_id == sim->player.id) &&
                (sim->schema_version < 115U || CcScrivenBookAccessible(sim, held->id, here)) &&
                s->almanacs[held->almanac_id - 1].agreed_day == s->company.agreed_day) carrying = true;
        }
        bool crown_delivered = CrownDeliver(sim, town);
        if (!crown_delivered && (town < 0 || s->company.agreed_day == 0 || !carrying))
            return Fail(error, capacity, "Attend a hearing or read a local almanac before carrying its findings onward.");
        if (carrying && s->company.agreed_day > 0) s->local[town] = s->company;
        (void)snprintf(s->report, sizeof(s->report), "The local scribes receive the company's cited almanac and adopt its proposed date.");
        break;
    }
    case CC_SCRIVEN_SKY: {
        if ((sim->clock.minute_subticks / CC_WORLD_MINUTE_SUBTICKS) < CC_CALENDAR_NIGHT_MINUTE)
            return Fail(error, capacity, "Watch after sunset to read the Wanderer among the signs.");
        if (!sim->dragon.slain && sim->dragon.omen_days_remaining > 0 && sim->dragon.retaliation_target_id == here)
            return Fail(error, capacity, "Read the Wanderer when the dragon's haze clears.");
        int sign = (int)(CcCalendarWanderer(sim->current_day, (sim->clock.minute_subticks / CC_WORLD_MINUTE_SUBTICKS)) * 13.0) % 13;
        s->player_observed_day = sim->current_day;
        (void)snprintf(s->report, sizeof(s->report), "The Wanderer stands in the %s. The daily sign is the %s.", CcZodiacName(sign), CcZodiacName(CcCalendar(sim->current_day).sign));
        for (int i = 0; i < CC_SCRIVEN_BOOKS; ++i) {
            CcScrivenBook *held = &s->books[i];
            const CcTreasure *owned = CcSimTreasure(sim, held->id);
            if (owned == NULL || owned->destroyed || held->notes[3].kind != 0 ||
                (owned->owner_id != sim->player.id && held->borrower_id != sim->player.id)) continue;
            held->notes[3] = (CcScrivenNote){.author_id = sim->player.id, .place_id = here,
                .source_book_id = held->id, .day = sim->current_day, .kind = CC_SCRIVEN_NOTE_SKY, .value = sign};
            (void)snprintf(held->notes[3].text, CC_SCRIVEN_TEXT,
                "Day %d: the Wanderer stood in the %s; the day's sign was the %s.",
                sim->current_day, CcZodiacName(sign), CcZodiacName(CcCalendar(sim->current_day).sign));
            break;
        }
        break;
    }
    case CC_SCRIVEN_COMMISSION: {
        CcSettlement *town = CcSimSettlementMutable(sim, here);
        if (town == NULL || sim->player.coins < 2 || town->market_coins > CC_SIM_MAX_MONEY - 2 ||
            CcPlayerCargoUsed(&sim->player) >= sim->player.cargo_capacity)
            return Fail(error, capacity, "A field tome needs two crowns and one cargo space.");
        CcTreasure *fresh = NewBook(sim, town);
        if (fresh == NULL) return Fail(error, capacity, "The town needs paper, wheat, tools, and shelf room to bind a tome.");
        fresh->owner_id = sim->player.id; ++sim->player.treasure_cargo_slots;
        sim->player.coins -= 2; town->market_coins += 2;
        CcSimAdvanceDays(sim, 1);
        (void)snprintf(s->report, sizeof(s->report), "The company receives a field tome with room for dated sightings. It takes one cargo space.");
        break;
    }
    case CC_SCRIVEN_WAIT: {
        int32_t time = sim->clock.minute_subticks + CC_WORLD_WATCH_SUBTICKS;
        if (time >= CC_WORLD_DAY_SUBTICKS && sim->current_day == CC_SIM_MAX_DAY)
            return Fail(error, capacity, "The calendar has reached its final supported day.");
        sim->clock.minute_subticks = time % CC_WORLD_DAY_SUBTICKS;
        if (time >= CC_WORLD_DAY_SUBTICKS) CcSimAdvanceDays(sim, 1);
        (void)snprintf(s->report, sizeof(s->report), "The company spends a watch with the calendar and town records.");
        break;
    }
    default: return Fail(error, capacity, "Choose read, borrow, return, observe, hearing, copy, deliver, sky, or wait.");
    }
    if (command->amount == CC_SCRIVEN_READ && b != NULL && b->almanac_id > 0)
        s->company = s->almanacs[b->almanac_id - 1];
    if (command->amount == CC_SCRIVEN_READ) CrownRead(sim, b);
    (void)snprintf(s->player_report, sizeof(s->player_report), "%s", s->report);
    if (error != NULL && capacity > 0) error[0] = '\0';
    return true;
}
void CcScrivenDescribe(const CcSim *sim, char *text, size_t capacity)
{
    if (sim == NULL || text == NULL || capacity == 0) return;
    char date[128]; CcCalendarFormat(sim->current_day, date, sizeof(date));
    const CcScrivenFinding *f = &sim->scriven.company;
    const CcSettlement *host = CcSimSettlement(sim, sim->scriven.host_id);
    int here = TownSlot(sim, sim->player.location_id);
    bool heard = here >= 0 && sim->scriven.notice_arrives[here] > 0 && sim->scriven.notice_arrives[here] <= sim->current_day;
    if (sim->schema_version >= 115U) {
        const CcCrownCalendar *c = &sim->crown_calendar;
        const CcScrivenFinding *crown = &c->company;
        const CcScrivenNote *seen = &c->company_sighting;
        bool provisional = seen->kind != 0 && (crown->agreed_day == 0 ||
            (seen->dragon_id == crown->dragon_id ? seen->day < crown->proposed_day : seen->day > crown->proposed_day));
        int start = provisional ? seen->day : crown->agreed_day > 0 ? crown->proposed_day : 0;
        CcCalendarDate sky = CcCalendar(sim->current_day);
        (void)snprintf(date, sizeof(date), "%d %s | Year of the %s", sky.day_of_sign,
            CcZodiacName(sky.sign), CcZodiacName(sky.year_sign));
        char age[192], epoch[192] = "";
        if (start > 0)
            (void)snprintf(age, sizeof(age), "Crown Age year %d. First sighting: day %d. %s",
                (sim->current_day - start) / CC_SOLAR_DAYS + 1, start,
                provisional ? "Scribes will compare the date at Scrivendays." : "Date agreed in the carried almanac.");
        else (void)snprintf(age, sizeof(age), "The Crown Age awaits a dated sighting in a received tome.");
        if (f->agreed_day > 0)
            (void)snprintf(epoch, sizeof(epoch), "\nDeep Wyrm Epoch year %d. Estimated start: day %d; range %d-%d.",
                (sim->current_day - f->proposed_day) / CC_SOLAR_DAYS + 1, f->proposed_day, f->earliest_day, f->latest_day);
        (void)snprintf(text, capacity, "%s\n%s%s\nScrivendays: Quill 1-7. Host: %s.", date, age, epoch,
            heard && host != NULL ? host->name : "awaiting a local notice");
        return;
    }
    if (f->agreed_day > 0)
        (void)snprintf(text, capacity, "%s\nAge year %d by the carried almanac. Proposed start: day %d; supported range %d-%d.\nScrivendays: Quill 1-7. Host: %s.",
            date, CcCalendar(sim->current_day - f->proposed_day).year + 1, f->proposed_day, f->earliest_day, f->latest_day, heard && host != NULL ? host->name : "awaiting a local notice");
    else
        (void)snprintf(text, capacity, "%s\nThe age awaits a dated almanac.\nScrivendays: Quill 1-7. Host: %s.", date, heard && host != NULL ? host->name : "awaiting a local notice");
}
static bool TextValid(const char *text, size_t capacity)
{
    return memchr(text, '\0', capacity) != NULL;
}
static bool FindingValid(const CcSim *sim, const CcScrivenFinding *f)
{
    if (!TextValid(f->citations[0], CC_SCRIVEN_TEXT) || !TextValid(f->citations[1], CC_SCRIVEN_TEXT)) return false;
    if (f->agreed_day == 0) return f->dragon_id == 0 && f->votes == 0 && f->schools == 0;
    if (CcIdKind(f->dragon_id) != CC_ENTITY_DRAGON || f->agreed_day < 1 || f->agreed_day > sim->current_day ||
        f->earliest_day < 1 || f->earliest_day > f->proposed_day || f->proposed_day > f->latest_day ||
        f->latest_day > f->agreed_day || f->votes < 2 || f->votes > f->schools || f->schools > CC_MAX_KINGDOMS ||
        f->votes * 3 < f->schools * 2) return false;
    for (int i = 0; i < 2; ++i)
        if (CcIdKind(f->book_ids[i]) != CC_ENTITY_TREASURE || School(sim, f->school_ids[i]) == NULL) return false;
    for (int i = 0; i < 6; ++i)
        if (f->signers[i] != 0 && CcIdKind(f->signers[i]) != CC_ENTITY_CHARACTER) return false;
    return true;
}
static bool CrownNoteValid(const CcSim *sim, const CcScrivenNote *n)
{
    if (!TextValid(n->text, sizeof(n->text))) return false;
    if (n->kind == 0) return n->dragon_id == 0 && n->day == 0;
    return n->kind == CC_SCRIVEN_NOTE_CROWNED && n->day > 0 && n->day <= sim->current_day &&
        CcIdKind(n->dragon_id) == CC_ENTITY_DRAGON && CcIdKind(n->source_book_id) == CC_ENTITY_TREASURE &&
        (n->author_id == sim->player.id || CcIdKind(n->author_id) == CC_ENTITY_CHARACTER) &&
        CcSimSettlement(sim, n->place_id) != NULL && n->value == 0;
}
static bool CrownFindingValid(const CcSim *sim, const CcScrivenFinding *f)
{
    return FindingValid(sim, f) && (f->agreed_day == 0 ||
        (f->earliest_day == f->proposed_day && f->latest_day == f->proposed_day && f->book_ids[0] != f->book_ids[1]));
}
static bool CrownValid(const CcSim *sim)
{
    if (sim->schema_version < 115U) return true;
    const CcCrownCalendar *c = &sim->crown_calendar;
    if (c->sighting_count < 0 || c->sighting_count > CC_SCRIVEN_AGES || c->editions < 0 || c->editions > 32 ||
        !CrownNoteValid(sim, &c->company_sighting) || !CrownFindingValid(sim, &c->company)) return false;
    for (int i = 0; i < CC_SCRIVEN_AGES; ++i) {
        if (!CrownNoteValid(sim, &c->sightings[i]) || (i < c->sighting_count && c->sightings[i].kind == 0)) return false;
        for (int j = 0; j < i && i < c->sighting_count; ++j)
            if (c->sightings[i].dragon_id == c->sightings[j].dragon_id) return false;
    }
    for (int i = 0; i < 32; ++i)
        if (!CrownFindingValid(sim, &c->almanacs[i]) || (i < c->editions && c->almanacs[i].agreed_day == 0)) return false;
    for (int i = 0; i < 6; ++i) if (!CrownFindingValid(sim, &c->local[i])) return false;
    for (int i = 0; i < CC_SCRIVEN_BOOKS; ++i)
        if (c->book_editions[i] > (uint64_t)c->editions || (c->book_editions[i] > 0 && sim->scriven.books[i].id == 0)) return false;
    return true;
}
bool CcScrivenValidate(const CcSim *sim)
{
    if (sim == NULL) return false;
    if (sim->schema_version < 113U) return true;
    if (sim->settlement_count < 0 || sim->settlement_count > CC_MAX_SETTLEMENTS ||
        sim->treasure_count < 0 || sim->treasure_count > CC_MAX_TREASURES ||
        sim->character_count < 0 || sim->character_count > CC_MAX_CHARACTER_RECORDS ||
        sim->kingdom_count < 0 || sim->kingdom_count > CC_MAX_KINGDOMS ||
        sim->route_count < 0 || sim->route_count > CC_MAX_ROUTES) return false;
    if (!CrownValid(sim)) return false;
    const CcScrivenState *s = &sim->scriven;
    if (s->status < 0 || s->status > 4 || s->age_count < 0 || s->age_count > CC_SCRIVEN_AGES ||
        s->meeting_year < -1 || s->meeting_year > CcCalendar(sim->current_day).year ||
        s->opens_day < 0 || s->closes_day < s->opens_day || s->closes_day > CC_SIM_MAX_DAY ||
        (s->host_id != 0 && CcSimSettlement(sim, s->host_id) == NULL) ||
        ((s->status == 1 || s->status == 2 || s->status == 3) && s->host_id == 0) ||
        s->meetings < 0 || s->comparisons < 0 || s->returns < 0 || s->failed_trips < 0 || s->editions < 0 || s->editions > 32 ||
        s->player_observed_day < 0 || s->player_observed_day > sim->current_day ||
        s->player_read_day < 0 || s->player_read_day > sim->current_day ||
        (s->player_read_book != 0 && CcIdKind(s->player_read_book) != CC_ENTITY_TREASURE) ||
        !TextValid(s->report, sizeof(s->report)) || !TextValid(s->player_report, sizeof(s->player_report)) || !FindingValid(sim, &s->finding) || !FindingValid(sim, &s->company)) return false;
    for (int i = 0; i < s->editions; ++i)
        if (!FindingValid(sim, &s->almanacs[i]) || s->almanacs[i].agreed_day == 0) return false;
    for (int i = 0; i < 6; ++i) {
        if (s->last_hosted[i] < -1 || s->last_hosted[i] > CcCalendar(sim->current_day).year ||
            s->notice_arrives[i] < 0 || s->notice_arrives[i] > CC_SIM_MAX_DAY || !FindingValid(sim, &s->local[i])) return false;
    }
    for (int i = 0; i < s->age_count; ++i) {
        if (CcIdKind(s->ages[i].dragon_id) != CC_ENTITY_DRAGON || s->ages[i].first_deep_day < 1 ||
            s->ages[i].first_deep_day > sim->current_day) return false;
        for (int j = 0; j < i; ++j) if (s->ages[i].dragon_id == s->ages[j].dragon_id) return false;
    }
    for (int i = 0; i < CC_SCRIVEN_BOOKS; ++i) {
        const CcScrivenBook *b = &s->books[i];
        if (b->id == 0) continue;
        if (CcIdKind(b->id) != CC_ENTITY_TREASURE || CcIdKind(b->source_id) != CC_ENTITY_TREASURE ||
            School(sim, b->school_id) == NULL || b->condition < 0 || b->condition > 100 || b->almanac_id > (uint64_t)s->editions ||
            b->edition_day < 1 || b->edition_day > sim->current_day || b->loan_due_day < 0 || b->loan_due_day > CC_SIM_MAX_DAY) return false;
        if (b->borrower_id != 0) {
            const CcTreasure *t = CcSimTreasure(sim, b->id);
            if (b->borrower_id != sim->player.id || CcSimSettlement(sim, b->return_place_id) == NULL ||
                b->loan_due_day < b->edition_day || t == NULL || t->destroyed || t->location_id != b->borrower_id ||
                t->owner_id != b->return_place_id) return false;
        } else if (b->return_place_id != 0 || b->loan_due_day != 0) return false;
        for (int j = 0; j < 3; ++j) if (!TextValid(b->passages[j], CC_SCRIVEN_TEXT)) return false;
        for (int j = 0; j < CC_SCRIVEN_NOTES; ++j) {
            const CcScrivenNote *n = &b->notes[j];
            if (!TextValid(n->text, sizeof(n->text)) || n->kind < 0 || n->kind > CC_SCRIVEN_NOTE_SKY || n->value < 0) return false;
            if (n->kind != 0 && (n->day < 1 || n->day > sim->current_day ||
                CcIdKind(n->source_book_id) != CC_ENTITY_TREASURE ||
                (n->author_id != sim->player.id && CcIdKind(n->author_id) != CC_ENTITY_CHARACTER) ||
                CcSimSettlement(sim, n->place_id) == NULL ||
                (n->kind != CC_SCRIVEN_NOTE_SKY && CcIdKind(n->dragon_id) != CC_ENTITY_DRAGON))) return false;
        }
        for (int j = 0; j < i; ++j) if (s->books[j].id == b->id) return false;
    }
    for (int i = 0; i < CC_SCRIVEN_DELEGATES; ++i) {
        const CcScrivenDelegate *d = &s->delegates[i];
        if (d->phase < CC_SCRIVEN_IDLE || d->phase > CC_SCRIVEN_FALLEN || d->edition_id > (uint64_t)s->editions || d->wheat < 0 || d->wheat > 11 || d->spent < 0 ||
            d->notice_day < 0 || d->notice_day > sim->current_day || d->arrival_day < 0 || d->arrival_day > CC_SIM_MAX_DAY) return false;
        if (d->phase == CC_SCRIVEN_IDLE) continue;
        if (CcIdKind(d->person_id) != CC_ENTITY_CHARACTER || CcIdKind(d->book_id) != CC_ENTITY_TREASURE ||
            CcSimSettlement(sim, d->home_id) == NULL || CcSimSettlement(sim, d->place_id) == NULL) return false;
        if (d->route_id != 0) {
            const CcRoute *r = CcSimRoute(sim, d->route_id);
            if (r == NULL || d->arrival_day <= sim->current_day ||
                !((r->from_id == d->place_id && r->to_id == d->hop_id) ||
                  (r->to_id == d->place_id && r->from_id == d->hop_id))) return false;
        }
        if (d->phase >= CC_SCRIVEN_EXPEDITION && d->phase <= CC_SCRIVEN_RETURNING) {
            const CcTreasure *t = CcSimTreasure(sim, d->book_id);
            if (t == NULL || t->destroyed || t->location_id != d->person_id || t->owner_id != d->home_id ||
                CcScrivenBookById(sim, d->book_id) == NULL || CcSimCharacter(sim, d->person_id) == NULL) return false;
            for (int j = 0; j < i; ++j)
                if (s->delegates[j].phase >= CC_SCRIVEN_EXPEDITION && s->delegates[j].phase <= CC_SCRIVEN_RETURNING &&
                    (s->delegates[j].book_id == d->book_id || s->delegates[j].person_id == d->person_id)) return false;
        }
    }
    return true;
}
