#include "sim/cc_return.h"

#include <stdio.h>
#include <string.h>

#define CC_RETURN_CANDIDATE_CAP 96
#define CC_RETURN_MARKET_LINES 3
#define CC_RETURN_ENCODING_VERSION 1U
#define CC_RETURN_TOWN_BYTES \
    (7U * 8U + (1U + CC_RETURN_FACES) * CC_NAME_CAPACITY + 9U * 4U + \
     2U * CC_GOOD_COUNT * 4U)

static int32_t ClampI32(int32_t value, int32_t low, int32_t high)
{
    return value < low ? low : value > high ? high : value;
}

static int32_t AbsI32(int32_t value)
{
    return value < 0 ? -value : value;
}

static int32_t SettlementSlot(const CcSim *sim, CcId id)
{
    if (sim == NULL || id == 0U) return -1;
    for (int32_t i = 0; i < sim->settlement_count && i < CC_MAX_SETTLEMENTS; ++i)
        if (sim->settlements[i].id == id) return i;
    return -1;
}

static const CcKingdom *KingdomById(const CcSim *sim, CcId id)
{
    for (int32_t i = 0; i < sim->kingdom_count && i < CC_MAX_KINGDOMS; ++i)
        if (sim->kingdoms[i].id == id) return &sim->kingdoms[i];
    return NULL;
}

static void CopyName(char *out, const char *name)
{
    (void)snprintf(out, CC_NAME_CAPACITY, "%s", name != NULL ? name : "");
}

static uint32_t TownThreats(const CcSim *sim, CcId id)
{
    uint32_t threats = 0U;
    for (int32_t i = 0; i < sim->bandit_count && i < CC_MAX_BANDITS; ++i) {
        const CcBanditGroup *band = &sim->bandits[i];
        if (band->members <= 0) continue;
        if (band->camp_settlement_id == id) threats |= CC_RETURN_THREAT_BANDIT_CAMP;
        if (band->raid_target_id == id && band->raid_phase != CC_BANDIT_RAID_IDLE)
            threats |= CC_RETURN_THREAT_BANDIT_RAID;
    }
    if (!sim->dragon.slain && sim->dragon.stolen_outstanding > 0 &&
        sim->dragon.retaliation_target_id == id)
        threats |= CC_RETURN_THREAT_DRAGON_OMEN;
    return threats;
}

bool CcReturnCapture(const CcSim *sim, CcId settlement_id, CcTownSeen *seen)
{
    if (seen == NULL) return false;
    *seen = (CcTownSeen){0};
    const CcSettlement *place = CcSimSettlement(sim, settlement_id);
    if (place == NULL) return false;
    seen->settlement_id = place->id;
    seen->kingdom_id = place->kingdom_id;
    const CcKingdom *kingdom = KingdomById(sim, place->kingdom_id);
    if (kingdom != NULL) {
        seen->ruler_id = kingdom->ruler_character_id;
        const CcCharacter *ruler = CcSimCharacter(sim, seen->ruler_id);
        if (ruler != NULL) CopyName(seen->ruler_name, ruler->name);
    }
    seen->seen_day = sim->current_day;
    seen->conditions = CcSimTownConditions(sim, place->id);
    seen->service_mask = place->service_mask;
    seen->threats = TownThreats(sim, place->id);
    seen->population = place->population;
    seen->security = place->security;
    seen->prosperity = place->prosperity;
    seen->hunger = place->hunger;
    seen->fire_damage = place->fire_damage;
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        seen->stock[good] = place->stock[good];
        seen->price[good] = place->price[good];
    }
    /* The faces are residents the company has been introduced to, newest
       introductions first, then by id. The ruler is kept separately. */
    for (int32_t f = 0; f < CC_RETURN_FACES; ++f) {
        const CcCharacter *best = NULL;
        for (int32_t i = 0; i < sim->character_count; ++i) {
            const CcCharacter *person = &sim->characters[i];
            if (person->introduced_day <= 0 ||
                person->home_settlement_id != place->id ||
                person->id == seen->ruler_id) continue;
            bool taken = false;
            for (int32_t g = 0; g < f; ++g)
                if (seen->face_ids[g] == person->id) taken = true;
            if (taken) continue;
            if (best == NULL || person->introduced_day > best->introduced_day ||
                (person->introduced_day == best->introduced_day &&
                 person->id < best->id)) best = person;
        }
        if (best == NULL) break;
        seen->face_ids[f] = best->id;
        CopyName(seen->face_names[f], best->name);
    }
    return true;
}

void CcReturnRecordSeen(CcSim *sim, CcId settlement_id)
{
    if (sim == NULL || sim->schema_version < CC_RETURN_SCHEMA_VERSION) return;
    int32_t slot = SettlementSlot(sim, settlement_id);
    if (slot < 0) return;
    (void)CcReturnCapture(sim, settlement_id, &sim->return_memory.towns[slot]);
}

const CcTownSeen *CcReturnLastSeen(const CcSim *sim, CcId settlement_id)
{
    int32_t slot = SettlementSlot(sim, settlement_id);
    if (slot < 0 || sim->return_memory.towns[slot].settlement_id != settlement_id)
        return NULL;
    return &sim->return_memory.towns[slot];
}

/* How much each kind matters to a returning traveller, before its size and
   surprise are applied. A town-wide loss comes first; prices come last. */
static const int32_t RETURN_WEIGHTS[CC_RETURN_CHANGE_KIND_COUNT] = {
    [CC_RETURN_CHANGE_ABANDONED] = 1000,
    [CC_RETURN_CHANGE_FIRE] = 900,
    [CC_RETURN_CHANGE_REBUILT] = 450,
    [CC_RETURN_CHANGE_NEW_RULER] = 850,
    [CC_RETURN_CHANGE_NEW_KINGDOM] = 850,
    [CC_RETURN_CHANGE_FACE_DIED] = 800,
    [CC_RETURN_CHANGE_FACE_GONE] = 350,
    [CC_RETURN_CHANGE_HUNGER] = 650,
    [CC_RETURN_CHANGE_FED] = 400,
    [CC_RETURN_CHANGE_LAWLESS] = 550,
    [CC_RETURN_CHANGE_SAFER] = 300,
    [CC_RETURN_CHANGE_THRIVING] = 350,
    [CC_RETURN_CHANGE_POORER] = 450,
    [CC_RETURN_CHANGE_POPULATION] = 400,
    [CC_RETURN_CHANGE_BANDIT_CAMP] = 600,
    [CC_RETURN_CHANGE_BANDITS_GONE] = 350,
    [CC_RETURN_CHANGE_DRAGON_OMEN] = 700,
    [CC_RETURN_CHANGE_SERVICE_LOST] = 450,
    [CC_RETURN_CHANGE_SERVICE_OPENED] = 300,
    [CC_RETURN_CHANGE_STALL_EMPTY] = 380,
    [CC_RETURN_CHANGE_STALL_RESTOCKED] = 200,
    [CC_RETURN_CHANGE_PRICE] = 250
};

typedef struct CcReturnCandidates {
    int32_t count;
    int32_t away_days;
    CcReturnChange items[CC_RETURN_CANDIDATE_CAP];
} CcReturnCandidates;

static CcReturnChange *AddChange(CcReturnCandidates *list,
                                 CcReturnChangeKind kind, int32_t detail,
                                 int32_t before, int32_t after,
                                 int32_t magnitude, int32_t surprise)
{
    if (list->count >= CC_RETURN_CANDIDATE_CAP) return NULL;
    CcReturnChange *change = &list->items[list->count++];
    *change = (CcReturnChange){0};
    change->kind = kind;
    change->detail = detail;
    change->before = before;
    change->after = after;
    change->magnitude = ClampI32(magnitude, 0, 100);
    /* A long absence makes change expected: a year away takes 40 points off. */
    int32_t damp = ClampI32(list->away_days / 9, 0, 40);
    change->surprise = ClampI32(surprise - damp, 5, 100);
    return change;
}

static bool KindIn(CcEventKind kind, const CcEventKind *kinds, int32_t count)
{
    for (int32_t i = 0; i < count; ++i)
        if (kinds[i] == kind) return true;
    return false;
}

/* The world events that can explain each change. */
static int32_t EvidenceKinds(CcReturnChangeKind kind, CcEventKind *out)
{
    int32_t n = 0;
    switch (kind) {
    case CC_RETURN_CHANGE_ABANDONED:
        out[n++] = CC_EVENT_DRAGON_RETALIATION;
        out[n++] = CC_EVENT_SETTLEMENT_RAIDED;
        out[n++] = CC_EVENT_SHORTAGE;
        break;
    case CC_RETURN_CHANGE_FIRE:
    case CC_RETURN_CHANGE_SERVICE_LOST:
        out[n++] = CC_EVENT_DRAGON_RETALIATION;
        out[n++] = CC_EVENT_GOBLIN_RAIDED;
        out[n++] = CC_EVENT_SETTLEMENT_RAIDED;
        break;
    case CC_RETURN_CHANGE_REBUILT:
        out[n++] = CC_EVENT_MASONRY_REPAIR;
        break;
    case CC_RETURN_CHANGE_NEW_RULER:
        out[n++] = CC_EVENT_ROYAL_SUCCESSION;
        break;
    case CC_RETURN_CHANGE_NEW_KINGDOM:
        out[n++] = CC_EVENT_KINGDOM_ACTION;
        break;
    case CC_RETURN_CHANGE_FACE_DIED:
        out[n++] = CC_EVENT_CHARACTER_DIED;
        break;
    case CC_RETURN_CHANGE_HUNGER:
        out[n++] = CC_EVENT_SHORTAGE;
        out[n++] = CC_EVENT_HARVEST_FAILED;
        break;
    case CC_RETURN_CHANGE_FED:
        out[n++] = CC_EVENT_RELIEF;
        out[n++] = CC_EVENT_SHIPMENT_ARRIVED;
        break;
    case CC_RETURN_CHANGE_LAWLESS:
    case CC_RETURN_CHANGE_BANDIT_CAMP:
        out[n++] = CC_EVENT_SETTLEMENT_RAIDED;
        out[n++] = CC_EVENT_BANDIT_RAID_DEPARTED;
        out[n++] = CC_EVENT_BANDIT_PRESSURE;
        break;
    case CC_RETURN_CHANGE_DRAGON_OMEN:
        out[n++] = CC_EVENT_DRAGON_OMEN;
        break;
    case CC_RETURN_CHANGE_SERVICE_OPENED:
        out[n++] = CC_EVENT_SERVICE_OPENED;
        break;
    case CC_RETURN_CHANGE_STALL_EMPTY:
        out[n++] = CC_EVENT_SETTLEMENT_RAIDED;
        out[n++] = CC_EVENT_GOBLIN_RAIDED;
        break;
    default:
        break;
    }
    return n;
}

static bool SubjectKind(CcReturnChangeKind kind)
{
    return kind == CC_RETURN_CHANGE_NEW_RULER ||
        kind == CC_RETURN_CHANGE_FACE_DIED;
}

/* The newest event since the last view that explains the change. The event
   ring is short, so the gossip ledger, which keeps notable stories longer, is
   the fallback. */
static CcId FindEvidence(const CcSim *sim, const CcTownSeen *before,
                         const CcReturnChange *change)
{
    CcEventKind kinds[4];
    int32_t count = EvidenceKinds(change->kind, kinds);
    if (count == 0) return 0U;
    bool by_subject = SubjectKind(change->kind);
    for (int32_t offset = 0; offset < sim->event_count; ++offset) {
        const CcEvent *event = CcSimRecentEvent(sim, offset);
        if (event == NULL || event->day < before->seen_day) continue;
        if (!KindIn(event->kind, kinds, count)) continue;
        if (by_subject ? event->subject_id == change->subject_id :
            (event->location_id == before->settlement_id ||
             event->target_id == before->settlement_id))
            return event->id;
    }
    CcId best = 0U;
    for (int32_t slot = 0; slot < CC_MAX_GOSSIP; ++slot) {
        const CcGossip *story = &sim->gossip[slot];
        if (story->event_id == 0U || story->day < before->seen_day ||
            !KindIn(story->kind, kinds, count)) continue;
        bool matches = by_subject ?
            change->subject_name[0] != '\0' &&
                strstr(story->text, change->subject_name) != NULL :
            story->origin_id == before->settlement_id;
        if (matches && story->event_id > best) best = story->event_id;
    }
    return best;
}

static void FindKnowledge(const CcSim *sim, CcReturnChange *change)
{
    CcId id = change->evidence_event_id;
    if (id == 0U) return;
    for (int32_t offset = 0; offset < sim->event_count; ++offset) {
        const CcEvent *event = CcSimRecentEvent(sim, offset);
        if (event != NULL && event->id == id && event->witness_id != 0U &&
            event->witness_id == sim->player.id) {
            change->knowledge = CC_RETURN_WITNESSED;
            return;
        }
    }
    int32_t carriers = CcSimGossipCarrierCapacity(sim);
    for (int32_t slot = 0; slot < CC_MAX_GOSSIP; ++slot) {
        if (sim->gossip[slot].event_id != id) continue;
        uint32_t bit = UINT32_C(1) << (uint32_t)slot;
        for (int32_t c = 0; c < carriers; ++c) {
            const CcGossipCarrier *carrier = &sim->gossip_carriers[c];
            if (carrier->id == 0U || (carrier->told_player & bit) == 0U) continue;
            int32_t confidence = carrier->versions[slot].confidence;
            if (change->knowledge != CC_RETURN_TOLD ||
                confidence > change->source_confidence) {
                change->knowledge = CC_RETURN_TOLD;
                change->source_id = carrier->id;
                change->source_confidence = confidence;
            }
        }
    }
}

static int32_t Score(const CcReturnChange *change)
{
    int64_t score = (int64_t)RETURN_WEIGHTS[change->kind] *
        (100 + change->magnitude) * (100 + change->surprise) / 40000;
    /* News the company already heard ranks lower: the gate voice should
       lead with what the player does not know. */
    if (change->knowledge == CC_RETURN_TOLD) score /= 3;
    else if (change->knowledge == CC_RETURN_WITNESSED) score /= 4;
    return (int32_t)score;
}

static bool RanksBefore(const CcReturnChange *a, const CcReturnChange *b)
{
    if (a->score != b->score) return a->score > b->score;
    if (a->kind != b->kind) return a->kind < b->kind;
    if (a->detail != b->detail) return a->detail < b->detail;
    return a->subject_id < b->subject_id;
}

static const char *PersonName(const CcSim *sim, CcId id)
{
    const CcCharacter *person = CcSimCharacter(sim, id);
    if (person != NULL) return person->name;
    const CcHistoricCharacter *past = CcSimHistoricCharacter(sim, id);
    return past != NULL ? past->name : "";
}

static void CompareFaces(const CcSim *sim, const CcTownSeen *before,
                         CcReturnCandidates *list)
{
    for (int32_t i = 0; i < CC_RETURN_FACES; ++i) {
        CcId id = before->face_ids[i];
        if (id == 0U) continue;
        const CcCharacter *person = CcSimCharacter(sim, id);
        if (person != NULL && person->home_settlement_id == before->settlement_id)
            continue;
        const CcHistoricCharacter *past = CcSimHistoricCharacter(sim, id);
        bool died = person == NULL && past != NULL && past->death_day > 0;
        CcReturnChange *change = died ?
            AddChange(list, CC_RETURN_CHANGE_FACE_DIED, -1, 1, 0, 100, 85) :
            AddChange(list, CC_RETURN_CHANGE_FACE_GONE, -1, 1, 0, 50, 50);
        if (change == NULL) return;
        change->subject_id = id;
        CopyName(change->subject_name, before->face_names[i]);
    }
}

static void CompareTown(const CcSim *sim, const CcTownSeen *before,
                        const CcTownSeen *now, CcReturnCandidates *list)
{
    CcReturnChange *change = NULL;
    uint32_t gained = now->conditions & ~before->conditions;
    uint32_t lost = before->conditions & ~now->conditions;
    if ((gained & CC_TOWN_ABANDONED) != 0U)
        (void)AddChange(list, CC_RETURN_CHANGE_ABANDONED, -1,
                        before->population, now->population, 100, 100);
    if (now->fire_damage >= before->fire_damage + 5)
        (void)AddChange(list, CC_RETURN_CHANGE_FIRE, -1, before->fire_damage,
                        now->fire_damage, now->fire_damage,
                        before->fire_damage == 0 ? 95 : 60);
    else if (before->fire_damage > 0 && now->fire_damage <= before->fire_damage - 10)
        (void)AddChange(list, CC_RETURN_CHANGE_REBUILT, -1, before->fire_damage,
                        now->fire_damage, before->fire_damage - now->fire_damage, 40);
    if (before->kingdom_id != 0U && now->kingdom_id != before->kingdom_id) {
        change = AddChange(list, CC_RETURN_CHANGE_NEW_KINGDOM, -1, 0, 0, 100, 100);
        if (change != NULL) {
            const CcKingdom *old_crown = KingdomById(sim, before->kingdom_id);
            const CcKingdom *new_crown = KingdomById(sim, now->kingdom_id);
            change->subject_id = now->kingdom_id;
            change->previous_id = before->kingdom_id;
            CopyName(change->subject_name, new_crown != NULL ? new_crown->name : "");
            CopyName(change->previous_name, old_crown != NULL ? old_crown->name : "");
        }
    } else if (before->ruler_id != 0U && now->ruler_id != before->ruler_id) {
        change = AddChange(list, CC_RETURN_CHANGE_NEW_RULER, -1, 0, 0, 100, 90);
        if (change != NULL) {
            change->subject_id = now->ruler_id;
            change->previous_id = before->ruler_id;
            CopyName(change->subject_name, now->ruler_name);
            CopyName(change->previous_name, before->ruler_name);
        }
    }
    CompareFaces(sim, before, list);

    int32_t delta = now->hunger - before->hunger;
    int32_t size = AbsI32(delta) * 2;
    if ((gained & CC_TOWN_HUNGRY) != 0U)
        (void)AddChange(list, CC_RETURN_CHANGE_HUNGER, -1, before->hunger, now->hunger, size, 85);
    else if (delta >= 15)
        (void)AddChange(list, CC_RETURN_CHANGE_HUNGER, -1, before->hunger, now->hunger, size, 50);
    else if ((lost & CC_TOWN_HUNGRY) != 0U)
        (void)AddChange(list, CC_RETURN_CHANGE_FED, -1, before->hunger, now->hunger, size, 50);
    else if (delta <= -20)
        (void)AddChange(list, CC_RETURN_CHANGE_FED, -1, before->hunger, now->hunger, size, 30);

    delta = now->security - before->security;
    size = AbsI32(delta) * 2;
    if ((gained & CC_TOWN_LAWLESS) != 0U)
        (void)AddChange(list, CC_RETURN_CHANGE_LAWLESS, -1, before->security, now->security, size, 80);
    else if (delta <= -20)
        (void)AddChange(list, CC_RETURN_CHANGE_LAWLESS, -1, before->security, now->security, size, 45);
    else if ((lost & CC_TOWN_LAWLESS) != 0U || delta >= 20)
        (void)AddChange(list, CC_RETURN_CHANGE_SAFER, -1, before->security, now->security, size, 35);

    delta = now->prosperity - before->prosperity;
    size = AbsI32(delta) * 2;
    if ((gained & CC_TOWN_THRIVING) != 0U || delta >= 20)
        (void)AddChange(list, CC_RETURN_CHANGE_THRIVING, -1, before->prosperity,
                        now->prosperity, size, (gained & CC_TOWN_THRIVING) != 0U ? 40 : 30);
    else if ((lost & CC_TOWN_THRIVING) != 0U || delta <= -20)
        (void)AddChange(list, CC_RETURN_CHANGE_POORER, -1, before->prosperity,
                        now->prosperity, size, (lost & CC_TOWN_THRIVING) != 0U ? 60 : 40);

    if (before->population > 0) {
        int64_t pct = ((int64_t)now->population - before->population) * 100 /
            before->population;
        int32_t percent = (int32_t)(pct > 1000 ? 1000 : pct < -1000 ? -1000 : pct);
        if (AbsI32(percent) >= 5)
            (void)AddChange(list, CC_RETURN_CHANGE_POPULATION, -1, before->population,
                            now->population, AbsI32(percent) * 4,
                            percent < 0 ? 40 + AbsI32(percent) * 3 : 25);
    }

    uint32_t new_threats = now->threats & ~before->threats;
    uint32_t old_threats = before->threats & ~now->threats;
    if ((new_threats & CC_RETURN_THREAT_DRAGON_OMEN) != 0U)
        (void)AddChange(list, CC_RETURN_CHANGE_DRAGON_OMEN, -1, 0, 1, 90, 90);
    if ((new_threats & CC_RETURN_THREAT_BANDIT_CAMP) != 0U)
        (void)AddChange(list, CC_RETURN_CHANGE_BANDIT_CAMP, 0, 0, 1, 70, 75);
    else if ((new_threats & CC_RETURN_THREAT_BANDIT_RAID) != 0U)
        (void)AddChange(list, CC_RETURN_CHANGE_BANDIT_CAMP, 1, 0, 1, 60, 70);
    if ((old_threats & CC_RETURN_THREAT_BANDIT_CAMP) != 0U)
        (void)AddChange(list, CC_RETURN_CHANGE_BANDITS_GONE, -1, 1, 0, 50, 40);

    for (int32_t service = 0; service < CC_SERVICE_COUNT; ++service) {
        uint32_t bit = UINT32_C(1) << (uint32_t)service;
        if ((before->service_mask & bit) != 0U && (now->service_mask & bit) == 0U)
            (void)AddChange(list, CC_RETURN_CHANGE_SERVICE_LOST, service, 1, 0, 60, 70);
        else if ((before->service_mask & bit) == 0U && (now->service_mask & bit) != 0U)
            (void)AddChange(list, CC_RETURN_CHANGE_SERVICE_OPENED, service, 0, 1, 40, 30);
    }

    /* Emptied stalls are one change: a bare market. Its detail is the most
       important empty good, food first, and goods_mask lists them all. */
    uint32_t emptied = 0U;
    int32_t first_empty = -1, emptied_count = 0, emptied_before = 0;
    static const int32_t stall_order[] = {
        CC_GOOD_BREAD, CC_GOOD_WHEAT, CC_GOOD_MEAT, CC_GOOD_TOOLS,
        CC_GOOD_IRON, CC_GOOD_WOOD, CC_GOOD_STONE, CC_GOOD_WOOL,
        CC_GOOD_PAPER, CC_GOOD_WEAPONS, CC_GOOD_GOLD, CC_GOOD_GEMS,
        CC_GOOD_RAW_STONE
    };
    for (size_t i = 0; i < sizeof(stall_order) / sizeof(stall_order[0]); ++i) {
        int32_t good = stall_order[i];
        if (before->stock[good] < 3 || now->stock[good] != 0) continue;
        emptied |= UINT32_C(1) << (uint32_t)good;
        if (first_empty < 0) {
            first_empty = good;
            emptied_before = before->stock[good];
        }
        ++emptied_count;
    }
    if (emptied != 0U) {
        bool staple = first_empty == CC_GOOD_BREAD ||
            first_empty == CC_GOOD_WHEAT || first_empty == CC_GOOD_MEAT;
        change = AddChange(list, CC_RETURN_CHANGE_STALL_EMPTY, first_empty,
                           emptied_before, 0,
                           (staple ? 80 : 50) + 5 * emptied_count,
                           emptied_count >= 3 ? 75 : 60);
        if (change != NULL) change->goods_mask = emptied;
    }
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        if (good == CC_GOOD_ROTTEN_MEAT || good == CC_GOOD_ROTTEN_GRAIN) continue;
        if (before->stock[good] == 0 && now->stock[good] >= 5)
            (void)AddChange(list, CC_RETURN_CHANGE_STALL_RESTOCKED, good, 0,
                            now->stock[good], 40, 30);
        int32_t was = before->price[good];
        int32_t is = now->price[good];
        if (was <= 0 || AbsI32(is - was) < 2) continue;
        int64_t pct = ((int64_t)is - was) * 100 / was;
        int32_t percent = (int32_t)(pct > 1000 ? 1000 : pct < -1000 ? -1000 : pct);
        /* Market prices swing often; only a large move is news. */
        if (AbsI32(percent) >= 35)
            (void)AddChange(list, CC_RETURN_CHANGE_PRICE, good, was, is,
                            10 + AbsI32(percent) / 2, 10 + AbsI32(percent) / 4);
    }
}

void CcReturnCompare(const CcSim *sim, const CcTownSeen *before,
                     const CcTownSeen *now, CcReturnDigest *digest)
{
    if (digest == NULL) return;
    *digest = (CcReturnDigest){0};
    if (sim == NULL || before == NULL || now == NULL) return;
    digest->settlement_id = now->settlement_id;
    digest->seen_day = before->seen_day;
    digest->return_day = now->seen_day;
    CcReturnCandidates list;
    list.count = 0;
    list.away_days = now->seen_day > before->seen_day ?
        now->seen_day - before->seen_day : 0;
    CompareTown(sim, before, now, &list);
    for (int32_t i = 0; i < list.count; ++i) {
        CcReturnChange *change = &list.items[i];
        change->evidence_event_id = FindEvidence(sim, before, change);
        FindKnowledge(sim, change);
        change->score = Score(change);
    }
    /* Insertion sort with a total order keeps the ranking deterministic. */
    for (int32_t i = 1; i < list.count; ++i) {
        CcReturnChange held = list.items[i];
        int32_t at = i;
        while (at > 0 && RanksBefore(&held, &list.items[at - 1])) {
            list.items[at] = list.items[at - 1];
            --at;
        }
        list.items[at] = held;
    }
    /* Keep the list about the town: at most a few market lines. */
    digest->found_count = list.count;
    int32_t market_lines = 0;
    for (int32_t i = 0; i < list.count &&
         digest->change_count < CC_RETURN_MAX_CHANGES; ++i) {
        CcReturnChangeKind kind = list.items[i].kind;
        if (kind == CC_RETURN_CHANGE_PRICE ||
            kind == CC_RETURN_CHANGE_STALL_RESTOCKED) {
            if (market_lines >= CC_RETURN_MARKET_LINES) continue;
            ++market_lines;
        }
        digest->changes[digest->change_count++] = list.items[i];
    }
}

bool CcReturnDigestBuild(const CcSim *sim, CcId settlement_id,
                         CcReturnDigest *digest)
{
    if (digest == NULL) return false;
    *digest = (CcReturnDigest){0};
    CcTownSeen now;
    if (!CcReturnCapture(sim, settlement_id, &now)) return false;
    const CcTownSeen *before = CcReturnLastSeen(sim, settlement_id);
    if (before == NULL) {
        digest->settlement_id = settlement_id;
        digest->return_day = now.seen_day;
        digest->first_visit = true;
        return true;
    }
    CcReturnCompare(sim, before, &now, digest);
    return true;
}

const char *CcReturnChangeKindName(CcReturnChangeKind kind)
{
    static const char *const names[CC_RETURN_CHANGE_KIND_COUNT] = {
        "abandoned", "fire", "rebuilt", "new ruler", "new kingdom",
        "death", "gone", "hunger", "fed", "lawless", "safer", "thriving",
        "poorer", "population", "bandit camp", "bandits gone",
        "dragon omen", "service lost", "service opened", "empty stall",
        "restocked", "price"
    };
    return kind >= 0 && kind < CC_RETURN_CHANGE_KIND_COUNT ? names[kind] : "unknown";
}

const char *CcReturnKnowledgeName(CcReturnKnowledge knowledge)
{
    return knowledge == CC_RETURN_TOLD ? "told" :
        knowledge == CC_RETURN_WITNESSED ? "witnessed" : "new";
}

static const char *GoodName(int32_t good)
{
    return good >= 0 && good < CC_GOOD_COUNT ? CcGoodName((CcGood)good) : "goods";
}

static const char *NameOr(const char *name, const char *fallback)
{
    return name != NULL && name[0] != '\0' ? name : fallback;
}

void CcReturnDescribe(const CcSim *sim, const CcReturnChange *change,
                      char *text, size_t capacity)
{
    if (text == NULL || capacity == 0U) return;
    text[0] = '\0';
    if (change == NULL) return;
    int32_t b = change->before, a = change->after;
    switch (change->kind) {
    case CC_RETURN_CHANGE_ABANDONED:
        (void)snprintf(text, capacity, "The town stands empty.");
        break;
    case CC_RETURN_CHANGE_FIRE:
        (void)snprintf(text, capacity, "Fire has burned the town: %d%% damage, was %d%%.", a, b);
        break;
    case CC_RETURN_CHANGE_REBUILT:
        (void)snprintf(text, capacity, "Builders have repaired fire damage: %d%% remains, was %d%%.", a, b);
        break;
    case CC_RETURN_CHANGE_NEW_RULER:
        (void)snprintf(text, capacity, "%s now rules here, not %s.",
                       NameOr(change->subject_name, "A new ruler"),
                       NameOr(change->previous_name, "the old ruler"));
        break;
    case CC_RETURN_CHANGE_NEW_KINGDOM:
        (void)snprintf(text, capacity, "The town now answers to %s, not %s.",
                       NameOr(change->subject_name, "another crown"),
                       NameOr(change->previous_name, "its old crown"));
        break;
    case CC_RETURN_CHANGE_FACE_DIED:
        (void)snprintf(text, capacity, "%s has died.",
                       NameOr(change->subject_name, "Someone you knew"));
        break;
    case CC_RETURN_CHANGE_FACE_GONE:
        (void)snprintf(text, capacity, "%s no longer lives here.",
                       NameOr(change->subject_name, "Someone you knew"));
        break;
    case CC_RETURN_CHANGE_HUNGER:
        (void)snprintf(text, capacity, "The town is hungrier: hunger %d, was %d.", a, b);
        break;
    case CC_RETURN_CHANGE_FED:
        (void)snprintf(text, capacity, "Hunger has eased: %d, was %d.", a, b);
        break;
    case CC_RETURN_CHANGE_LAWLESS:
        (void)snprintf(text, capacity, "The streets are less safe: security %d, was %d.", a, b);
        break;
    case CC_RETURN_CHANGE_SAFER:
        (void)snprintf(text, capacity, "The streets are safer: security %d, was %d.", a, b);
        break;
    case CC_RETURN_CHANGE_THRIVING:
        (void)snprintf(text, capacity, "Trade is good: prosperity %d, was %d.", a, b);
        break;
    case CC_RETURN_CHANGE_POORER:
        (void)snprintf(text, capacity, "The town is poorer: prosperity %d, was %d.", a, b);
        break;
    case CC_RETURN_CHANGE_POPULATION:
        (void)snprintf(text, capacity, "%d people live here now, was %d.", a, b);
        break;
    case CC_RETURN_CHANGE_BANDIT_CAMP:
        (void)snprintf(text, capacity, change->detail == 1 ?
                       "Bandits are planning a raid on the town." :
                       "Bandits have made camp by the town.");
        break;
    case CC_RETURN_CHANGE_BANDITS_GONE:
        (void)snprintf(text, capacity, "The bandit camp is gone.");
        break;
    case CC_RETURN_CHANGE_DRAGON_OMEN:
        (void)snprintf(text, capacity, "%s's anger is turned toward the town.",
                       NameOr(sim != NULL ? sim->dragon.name : NULL, "The dragon"));
        break;
    case CC_RETURN_CHANGE_SERVICE_LOST:
        (void)snprintf(text, capacity, "The %s is gone.",
                       CcServiceName((CcServiceKind)change->detail));
        break;
    case CC_RETURN_CHANGE_SERVICE_OPENED:
        (void)snprintf(text, capacity, "A %s has opened.",
                       CcServiceName((CcServiceKind)change->detail));
        break;
    case CC_RETURN_CHANGE_STALL_EMPTY: {
        char goods[160] = "";
        size_t used = 0U;
        int32_t listed = 0, total = 0;
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good)
            if ((change->goods_mask & (UINT32_C(1) << (uint32_t)good)) != 0U) ++total;
        /* The detail good leads; the rest follow in good order. */
        for (int32_t pass = 0; pass < 2; ++pass) {
            for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
                bool lead = good == change->detail;
                if ((pass == 0) != lead ||
                    (change->goods_mask & (UINT32_C(1) << (uint32_t)good)) == 0U)
                    continue;
                ++listed;
                const char *join = listed == 1 ? "" :
                    listed == total ? (total == 2 ? " or " : ", or ") : ", ";
                int wrote = snprintf(goods + used, sizeof(goods) - used, "%s%s",
                                     join, GoodName(good));
                if (wrote > 0 && (size_t)wrote < sizeof(goods) - used)
                    used += (size_t)wrote;
            }
        }
        (void)snprintf(text, capacity, "The market is bare: no %s for sale.", goods);
        break;
    }
    case CC_RETURN_CHANGE_STALL_RESTOCKED:
        (void)snprintf(text, capacity, "%s is for sale again: %d.",
                       GoodName(change->detail), a);
        break;
    case CC_RETURN_CHANGE_PRICE:
        (void)snprintf(text, capacity, "%s costs %d, was %d.",
                       GoodName(change->detail), a, b);
        break;
    default:
        (void)snprintf(text, capacity, "Something has changed.");
        break;
    }
}

static bool Append(char *text, size_t capacity, size_t *used, const char *line)
{
    int written = snprintf(text + *used, capacity - *used, "%s", line);
    if (written < 0 || (size_t)written >= capacity - *used) {
        *used = capacity - 1U;
        return false;
    }
    *used += (size_t)written;
    return true;
}

bool CcReturnDigestText(const CcSim *sim, const CcReturnDigest *digest,
                        char *text, size_t capacity)
{
    if (text == NULL || capacity == 0U) return false;
    text[0] = '\0';
    if (sim == NULL || digest == NULL) return false;
    size_t used = 0U;
    char line[512];
    const CcSettlement *place = CcSimSettlement(sim, digest->settlement_id);
    const char *name = place != NULL ? place->name : "?";
    if (digest->first_visit) {
        (void)snprintf(line, sizeof(line),
                       "%s: first visit, nothing remembered (day %d).\n",
                       name, digest->return_day);
        return Append(text, capacity, &used, line);
    }
    (void)snprintf(line, sizeof(line),
                   "%s: last seen day %d, back on day %d (%d days away); %d changes, %d shown.\n",
                   name, digest->seen_day, digest->return_day,
                   digest->return_day - digest->seen_day, digest->found_count,
                   digest->change_count);
    bool ok = Append(text, capacity, &used, line);
    for (int32_t i = 0; ok && i < digest->change_count; ++i) {
        const CcReturnChange *change = &digest->changes[i];
        char said[256];
        CcReturnDescribe(sim, change, said, sizeof(said));
        char known[96] = "";
        if (change->knowledge == CC_RETURN_TOLD)
            (void)snprintf(known, sizeof(known), "told by %s, %d%% sure",
                           NameOr(PersonName(sim, change->source_id),
                                  change->source_id == sim->player.id ?
                                      "your company" : "a traveller"),
                           change->source_confidence);
        else
            (void)snprintf(known, sizeof(known), "%s",
                           CcReturnKnowledgeName(change->knowledge));
        char evidence[64] = "no event";
        const CcEvent *event = NULL;
        for (int32_t offset = 0; offset < sim->event_count && event == NULL; ++offset) {
            const CcEvent *candidate = CcSimRecentEvent(sim, offset);
            if (candidate != NULL && candidate->id == change->evidence_event_id)
                event = candidate;
        }
        if (event != NULL)
            (void)snprintf(evidence, sizeof(evidence), "%s day %d",
                           CcEventKindName(event->kind), event->day);
        else if (change->evidence_event_id != 0U)
            (void)snprintf(evidence, sizeof(evidence), "a story");
        (void)snprintf(line, sizeof(line),
                       "%2d. [%s] %s (score %d, size %d, surprise %d, %s; %s)\n",
                       i + 1, CcReturnChangeKindName(change->kind), said,
                       change->score, change->magnitude, change->surprise, known,
                       evidence);
        ok = Append(text, capacity, &used, line);
    }
    return ok;
}

static uint64_t HashU64(uint64_t hash, uint64_t value)
{
    for (int32_t byte = 0; byte < 8; ++byte) {
        hash ^= value & UINT64_C(0xff);
        hash *= UINT64_C(1099511628211);
        value >>= 8U;
    }
    return hash;
}

static uint64_t HashName(uint64_t hash, const char *name)
{
    for (size_t i = 0; i < CC_NAME_CAPACITY; ++i)
        hash = HashU64(hash, (uint8_t)name[i]);
    return hash;
}

uint64_t CcReturnMemoryHash(const CcReturnMemory *memory)
{
    if (memory == NULL) return 0U;
    uint64_t hash = UINT64_C(1469598103934665603);
    for (int32_t i = 0; i < CC_MAX_SETTLEMENTS; ++i) {
        const CcTownSeen *seen = &memory->towns[i];
        /* An empty slot adds nothing, so a company that never travels hashes
           the same as before this record existed, apart from the schema. */
        if (seen->settlement_id == 0U) continue;
        hash = HashU64(hash, (uint64_t)i);
        hash = HashU64(hash, seen->settlement_id);
        hash = HashU64(hash, seen->kingdom_id);
        hash = HashU64(hash, seen->ruler_id);
        for (int32_t f = 0; f < CC_RETURN_FACES; ++f) {
            hash = HashU64(hash, seen->face_ids[f]);
            hash = HashName(hash, seen->face_names[f]);
        }
        hash = HashName(hash, seen->ruler_name);
        hash = HashU64(hash, (uint32_t)seen->seen_day);
        hash = HashU64(hash, seen->conditions);
        hash = HashU64(hash, seen->service_mask);
        hash = HashU64(hash, seen->threats);
        hash = HashU64(hash, (uint32_t)seen->population);
        hash = HashU64(hash, (uint32_t)seen->security);
        hash = HashU64(hash, (uint32_t)seen->prosperity);
        hash = HashU64(hash, (uint32_t)seen->hunger);
        hash = HashU64(hash, (uint32_t)seen->fire_damage);
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
            hash = HashU64(hash, (uint32_t)seen->stock[good]);
            hash = HashU64(hash, (uint32_t)seen->price[good]);
        }
    }
    return hash;
}

static bool NameTerminated(const char *name)
{
    return memchr(name, '\0', CC_NAME_CAPACITY) != NULL;
}

bool CcReturnMemoryValidate(const CcSim *sim)
{
    if (sim == NULL) return false;
    for (int32_t i = 0; i < CC_MAX_SETTLEMENTS; ++i) {
        const CcTownSeen *seen = &sim->return_memory.towns[i];
        if (seen->settlement_id == 0U) {
            if (seen->seen_day != 0) return false;
            continue;
        }
        if (i >= sim->settlement_count ||
            sim->settlements[i].id != seen->settlement_id ||
            seen->seen_day < 0 || seen->seen_day > sim->current_day ||
            seen->population < 0 || seen->hunger < 0 || seen->security < 0 ||
            seen->prosperity < 0 || seen->fire_damage < 0 ||
            !NameTerminated(seen->ruler_name)) return false;
        for (int32_t f = 0; f < CC_RETURN_FACES; ++f)
            if (!NameTerminated(seen->face_names[f])) return false;
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good)
            if (seen->stock[good] < 0 || seen->price[good] < 0) return false;
    }
    return true;
}

size_t CcReturnMemoryEncodedSize(const CcReturnMemory *memory)
{
    if (memory == NULL) return 0U;
    return 4U + (size_t)CC_MAX_SETTLEMENTS * CC_RETURN_TOWN_BYTES;
}

static void Write32(uint8_t **at, uint32_t value)
{
    for (int i = 0; i < 4; ++i) *(*at)++ = (uint8_t)(value >> (8 * i));
}

static void Write64(uint8_t **at, uint64_t value)
{
    for (int i = 0; i < 8; ++i) *(*at)++ = (uint8_t)(value >> (8 * i));
}

static uint32_t Read32(const uint8_t **at)
{
    uint32_t value = 0U;
    for (int i = 0; i < 4; ++i) value |= (uint32_t)*(*at)++ << (8 * i);
    return value;
}

static uint64_t Read64(const uint8_t **at)
{
    uint64_t value = 0U;
    for (int i = 0; i < 8; ++i) value |= (uint64_t)*(*at)++ << (8 * i);
    return value;
}

size_t CcReturnMemoryEncode(const CcReturnMemory *memory, uint8_t *bytes,
                            size_t capacity)
{
    size_t needed = CcReturnMemoryEncodedSize(memory);
    if (needed == 0U || bytes == NULL || capacity < needed) return 0U;
    uint8_t *at = bytes;
    Write32(&at, CC_RETURN_ENCODING_VERSION);
    for (int32_t i = 0; i < CC_MAX_SETTLEMENTS; ++i) {
        const CcTownSeen *seen = &memory->towns[i];
        Write64(&at, seen->settlement_id);
        Write64(&at, seen->kingdom_id);
        Write64(&at, seen->ruler_id);
        for (int32_t f = 0; f < CC_RETURN_FACES; ++f) Write64(&at, seen->face_ids[f]);
        memcpy(at, seen->ruler_name, CC_NAME_CAPACITY); at += CC_NAME_CAPACITY;
        for (int32_t f = 0; f < CC_RETURN_FACES; ++f) {
            memcpy(at, seen->face_names[f], CC_NAME_CAPACITY); at += CC_NAME_CAPACITY;
        }
        Write32(&at, (uint32_t)seen->seen_day);
        Write32(&at, seen->conditions);
        Write32(&at, seen->service_mask);
        Write32(&at, seen->threats);
        Write32(&at, (uint32_t)seen->population);
        Write32(&at, (uint32_t)seen->security);
        Write32(&at, (uint32_t)seen->prosperity);
        Write32(&at, (uint32_t)seen->hunger);
        Write32(&at, (uint32_t)seen->fire_damage);
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
            Write32(&at, (uint32_t)seen->stock[good]);
            Write32(&at, (uint32_t)seen->price[good]);
        }
    }
    return (size_t)(at - bytes);
}

bool CcReturnMemoryDecode(CcReturnMemory *memory, const uint8_t *bytes,
                          size_t length)
{
    if (memory == NULL || bytes == NULL ||
        length != 4U + (size_t)CC_MAX_SETTLEMENTS * CC_RETURN_TOWN_BYTES)
        return false;
    const uint8_t *at = bytes;
    if (Read32(&at) != CC_RETURN_ENCODING_VERSION) return false;
    *memory = (CcReturnMemory){0};
    for (int32_t i = 0; i < CC_MAX_SETTLEMENTS; ++i) {
        CcTownSeen *seen = &memory->towns[i];
        seen->settlement_id = Read64(&at);
        seen->kingdom_id = Read64(&at);
        seen->ruler_id = Read64(&at);
        for (int32_t f = 0; f < CC_RETURN_FACES; ++f) seen->face_ids[f] = Read64(&at);
        memcpy(seen->ruler_name, at, CC_NAME_CAPACITY); at += CC_NAME_CAPACITY;
        for (int32_t f = 0; f < CC_RETURN_FACES; ++f) {
            memcpy(seen->face_names[f], at, CC_NAME_CAPACITY); at += CC_NAME_CAPACITY;
        }
        seen->seen_day = (int32_t)Read32(&at);
        seen->conditions = Read32(&at);
        seen->service_mask = Read32(&at);
        seen->threats = Read32(&at);
        seen->population = (int32_t)Read32(&at);
        seen->security = (int32_t)Read32(&at);
        seen->prosperity = (int32_t)Read32(&at);
        seen->hunger = (int32_t)Read32(&at);
        seen->fire_damage = (int32_t)Read32(&at);
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
            seen->stock[good] = (int32_t)Read32(&at);
            seen->price[good] = (int32_t)Read32(&at);
        }
    }
    return true;
}
