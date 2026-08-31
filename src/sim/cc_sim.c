#include "sim/cc_sim.h"

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#define CC_ID_SERIAL_MASK UINT64_C(0x00ffffffffffffff)

static void GenerateSituations(CcSim *sim);
static void DeliverDelayedEchoIfReady(CcSim *sim);
static void PlanGoblinTribute(CcSim *sim);
static void AdvanceGoblinTribute(CcSim *sim);
static void PlanHoardRaid(CcSim *sim);
static void AdvanceHoardRaid(CcSim *sim);
static void AdvanceDragonRetaliation(CcSim *sim);
static void AdvanceDragonEcology(CcSim *sim);
static void AdvanceCouriers(CcSim *sim);
static void UpdateRoyalDiplomacy(CcSim *sim);
static void AdvanceDragonCampaign(CcSim *sim);
static void AdvanceHorseTeam(CcSim *sim);
static void AdvanceRuins(CcSim *sim);
static int32_t BasePrice(CcGood good);
static int32_t CalculateDragonCrownStrength(const CcSim *sim);

static int32_t ClampI32(int32_t value, int32_t minimum, int32_t maximum)
{
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static int32_t MinimumI32(int32_t a, int32_t b)
{
    return a < b ? a : b;
}

static int32_t MaximumI32(int32_t a, int32_t b)
{
    return a > b ? a : b;
}

static int32_t AbsoluteI32(int32_t value)
{
    return value < 0 ? -value : value;
}

static bool ValidBoundedText(const char *text, size_t capacity)
{
    return text != NULL && text[0] != '\0' &&
           memchr(text, '\0', capacity) != NULL;
}

static bool ValidOptionalBoundedText(const char *text, size_t capacity)
{
    return text != NULL && memchr(text, '\0', capacity) != NULL;
}

static void SetError(char *error, size_t capacity, const char *message)
{
    if (error == NULL || capacity == 0U) return;
    (void)snprintf(error, capacity, "%s", message);
}

static uint32_t NextRandom(CcSim *sim)
{
    uint32_t value = sim->random_state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    sim->random_state = value == 0U ? UINT32_C(0xa341316c) : value;
    return sim->random_state;
}

bool CcSettlementIsAbandoned(const CcSettlement *settlement)
{
    return settlement != NULL && settlement->population <= 0;
}

int32_t CcSimClimateFactor(const CcSim *sim)
{
    if (sim == NULL) return 100;
    uint32_t year = sim->current_day > 1 ?
        (uint32_t)((sim->current_day - 1) / 364) : 0U;
    uint32_t era = year / 40U;
    uint32_t value = sim->world_seed ^
        ((era + 1U) * UINT32_C(0x9e3779b9));
    value ^= value >> 16U;
    value *= UINT32_C(0x7feb352d);
    value ^= value >> 15U;
    value *= UINT32_C(0x846ca68b);
    value ^= value >> 16U;
    uint32_t roll = value % 100U;
    if (roll < 10U) return 58;
    if (roll < 28U) return 78;
    if (roll < 68U) return 100;
    if (roll < 90U) return 116;
    return 132;
}

int32_t CcDragonCampaignExperience(const CcSim *sim)
{
    if (sim == NULL || sim->dragon_campaign.defeats <= 0) return 0;
    return sim->dragon_campaign.defeats >= 6 ?
        72 : sim->dragon_campaign.defeats * 12;
}

static CcId NextId(CcSim *sim, CcEntityKind kind)
{
    CcId id = CcMakeId(kind, sim->next_entity_serial);
    sim->next_entity_serial += 1U;
    return id;
}

CcId CcMakeId(CcEntityKind kind, uint64_t serial)
{
    return ((uint64_t)kind << 56U) | (serial & CC_ID_SERIAL_MASK);
}

CcEntityKind CcIdKind(CcId id)
{
    return (CcEntityKind)((id >> 56U) & UINT64_C(0xff));
}

static void CopyName(char destination[CC_NAME_CAPACITY], const char *name)
{
    (void)snprintf(destination, CC_NAME_CAPACITY, "%s", name);
}

void CcSimInitializeDragonCycle(CcSim *sim)
{
    if (sim == NULL || sim->settlement_count < 1 ||
        sim->goblins.id != 0U || sim->dragon.id != 0U) return;
    sim->goblins.id = NextId(sim, CC_ENTITY_GOBLIN_CULT);
    CopyName(sim->goblins.name, "The Cinder Tithe");
    sim->goblins.members = 48;
    sim->goblins.devotion = 74;
    sim->goblins.cohesion = 68;
    sim->goblins.tribute_phase = CC_GOBLIN_TRIBUTE_IDLE;
    sim->goblins.raid_motive = CC_GOBLIN_RAID_NONE;
    sim->goblins.lair_settlement_id = sim->settlements[
        sim->settlement_count > 3 ? 3 : 0].id;
    sim->goblins.lair_stock[CC_GOOD_FOOD] = 12;
    sim->goblins.lair_stock[CC_GOOD_TOOLS] = 2;
    sim->goblins.lair_stock[CC_GOOD_WEAPONS] = 3;

    sim->dragon.id = NextId(sim, CC_ENTITY_DRAGON);
    CopyName(sim->dragon.name, "Varkesh the Unappeased");
    sim->dragon.lair_settlement_id =
        sim->settlements[sim->settlement_count - 1].id;
    /* Old offerings make the cave worth robbing before the first live tithe. */
    sim->dragon.hoard = 30;
    CcSimInitializeDragonEcology(sim);
}

void CcSimInitializeDragonEcology(CcSim *sim)
{
    if (sim == NULL || sim->dragon.id == 0U) return;
    CcDragon *dragon = &sim->dragon;
    if (dragon->age_days > 0 || dragon->memory_integrity > 0 ||
        dragon->crown_strength > 0 || dragon->regional_influence > 0) return;
    dragon->life_stage = dragon->slain ? CC_DRAGON_STAGE_AFTERDRAGON :
                                         CC_DRAGON_STAGE_CROWNED;
    dragon->activity = dragon->slain ? CC_DRAGON_ACTIVITY_AFTERMATH :
                                       CC_DRAGON_ACTIVITY_DORMANT;
    dragon->age_days = dragon->slain ? 0 :
        (220 + (int32_t)(sim->world_seed % 81U)) * 365;
    dragon->body_condition = dragon->slain ? 0 : 82;
    dragon->memory_integrity = dragon->slain ? 0 : 100;
    dragon->territory_stability = dragon->slain ? 0 : 72;
    dragon->regional_influence = dragon->slain ? 45 : 58;
    dragon->crown_continuity_days = dragon->slain ? 0 : 80 * 365;
    dragon->hunt_cooldown_days = dragon->slain ? 0 : 42;
    dragon->brood_cooldown_days = dragon->slain ? 0 :
        (90 + (int32_t)(sim->world_seed % 121U)) * 365;
    dragon->crown_strength = dragon->slain ? 0 :
        CalculateDragonCrownStrength(sim);
}

void CcSimInitializeHoardRaiders(CcSim *sim)
{
    if (sim == NULL || sim->hoard_raiders.id != 0U) return;
    sim->hoard_raiders.id = NextId(sim, CC_ENTITY_HOARD_RAIDERS);
    CopyName(sim->hoard_raiders.name, "The Ash-Poor Company");
    sim->hoard_raiders.phase = CC_HOARD_RAIDERS_IDLE;
    sim->hoard_raiders.motive = CC_HOARD_RAID_NO_MOTIVE;
    sim->hoard_raiders.cooldown_days = 42;
}

void CcSimInitializeAnimalEconomy(CcSim *sim)
{
    if (sim == NULL || sim->player.id == 0U ||
        sim->settlement_count < 1 || sim->horse_team[0].id != 0U) return;

    static const char *horse_names[CC_CARRIAGE_HORSE_COUNT] = {
        "Bracken", "Morrow"
    };
    static const int32_t horse_ages[CC_CARRIAGE_HORSE_COUNT] = {
        8 * 365, 9 * 365
    };
    for (int32_t i = 0; i < CC_CARRIAGE_HORSE_COUNT; ++i) {
        CcHorse *horse = &sim->horse_team[i];
        horse->id = NextId(sim, CC_ENTITY_HORSE);
        CopyName(horse->name, horse_names[i]);
        horse->age_days = horse_ages[i];
        horse->health = 100;
    }
    CcSimInitializeHorseStableSystem(sim);

    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        CcSettlement *place = &sim->settlements[i];
        if (!CcSettlementHasService(place, CC_SERVICE_FARM)) continue;
        switch (place->function) {
            case CC_SETTLEMENT_FARMING:
                place->cow_adults = 28;
                place->cow_calves = 7;
                break;
            case CC_SETTLEMENT_CAPITAL:
                place->cow_adults = 18;
                place->cow_calves = 4;
                break;
            case CC_SETTLEMENT_FORTRESS:
                place->cow_adults = 12;
                place->cow_calves = 3;
                break;
            case CC_SETTLEMENT_MINING:
                place->cow_adults = 8;
                place->cow_calves = 2;
                break;
            case CC_SETTLEMENT_MARKET:
            case CC_SETTLEMENT_DUNGEON_TOWN:
                place->cow_adults = 6;
                place->cow_calves = 1;
                break;
        }
        place->cow_condition = 88;
    }
}

void CcSimInitializeHorseStableSystem(CcSim *sim)
{
    if (sim == NULL ||
        CcIdKind(sim->horse_team[0].id) != CC_ENTITY_HORSE ||
        CcIdKind(sim->horse_team[1].id) != CC_ENTITY_HORSE) return;
    static const CcHorseSex sexes[CC_CARRIAGE_HORSE_COUNT] = {
        CC_HORSE_STALLION, CC_HORSE_MARE
    };
    static const int32_t strengths[CC_CARRIAGE_HORSE_COUNT] = {82, 72};
    static const int32_t temperaments[CC_CARRIAGE_HORSE_COUNT] = {58, 84};
    static const int32_t hardiness[CC_CARRIAGE_HORSE_COUNT] = {76, 70};
    for (int32_t i = 0; i < CC_CARRIAGE_HORSE_COUNT; ++i) {
        CcHorse *horse = &sim->horse_team[i];
        if (horse->training > 0 && horse->strength > 0 &&
            horse->temperament > 0 && horse->hardiness > 0) continue;
        horse->sex = sexes[i];
        horse->training = 100;
        horse->strength = strengths[i];
        horse->temperament = temperaments[i];
        horse->hardiness = hardiness[i];
    }
}

int32_t CcSimHorseCount(const CcSim *sim)
{
    if (sim == NULL) return 0;
    return CC_CARRIAGE_HORSE_COUNT + sim->stable_horse_count;
}

const CcHorse *CcSimHorseAt(const CcSim *sim, int32_t index)
{
    if (sim == NULL || index < 0 || index >= CcSimHorseCount(sim)) {
        return NULL;
    }
    if (index < CC_CARRIAGE_HORSE_COUNT) return &sim->horse_team[index];
    return &sim->stable_horses[index - CC_CARRIAGE_HORSE_COUNT];
}

const CcHorse *CcSimHorse(const CcSim *sim, CcId horse_id)
{
    if (sim == NULL || horse_id == 0U) return NULL;
    for (int32_t i = 0; i < CcSimHorseCount(sim); ++i) {
        const CcHorse *horse = CcSimHorseAt(sim, i);
        if (horse != NULL && horse->id == horse_id) return horse;
    }
    return NULL;
}

static CcHorse *HorseMutable(CcSim *sim, CcId horse_id)
{
    if (sim == NULL || horse_id == 0U) return NULL;
    for (int32_t i = 0; i < CC_CARRIAGE_HORSE_COUNT; ++i) {
        if (sim->horse_team[i].id == horse_id) return &sim->horse_team[i];
    }
    for (int32_t i = 0; i < sim->stable_horse_count; ++i) {
        if (sim->stable_horses[i].id == horse_id) {
            return &sim->stable_horses[i];
        }
    }
    return NULL;
}

const char *CcHorseSexName(CcHorseSex sex)
{
    return sex == CC_HORSE_STALLION ? "stallion" : "mare";
}

const char *CcHorseLifeStageName(const CcHorse *horse)
{
    if (horse == NULL) return "unknown";
    if (horse->age_days < 365) return "foal";
    if (horse->age_days < 3 * 365) return "young horse";
    if (horse->age_days < 18 * 365) return "working horse";
    return "senior horse";
}

bool CcHorseWorkingReady(const CcHorse *horse)
{
    return horse != NULL && horse->age_days >= 3 * 365 &&
           horse->training >= 60 && horse->health >= 45;
}

int32_t CcSimHorseTeamReadiness(const CcSim *sim)
{
    if (sim == NULL) return 0;
    int32_t readiness = 100;
    for (int32_t i = 0; i < CC_CARRIAGE_HORSE_COUNT; ++i) {
        const CcHorse *horse = &sim->horse_team[i];
        if (CcIdKind(horse->id) != CC_ENTITY_HORSE) return 0;
        int32_t value = horse->health - horse->fatigue / 2 -
                        horse->hunger / 2;
        if (sim->schema_version >= 15U &&
            !CcHorseWorkingReady(horse)) value -= 40;
        if (sim->schema_version >= 15U &&
            horse->pregnancy_days_remaining > 0 &&
            horse->pregnancy_days_remaining <= 90) value -= 15;
        readiness = MinimumI32(readiness, value);
    }
    return ClampI32(readiness, 0, 100);
}

static uint32_t ServiceBit(CcServiceKind service)
{
    if (service < 0 || service >= CC_SERVICE_COUNT) return 0U;
    return UINT32_C(1) << (uint32_t)service;
}

static void GeneratePlaceName(CcSim *sim, char destination[CC_NAME_CAPACITY],
                              CcSettlementFunction function)
{
    static const char *prefixes[] = {
        "Rose", "Mere", "Iron", "Hollow", "Star", "Alder", "Ash",
        "Glass", "Willow", "Thorn", "Silver", "Moss", "Ember", "Gloam"
    };
    static const char *suffixes[][4] = {
        {"fall", "field", "mead", "acre"},
        {"cross", "market", "gate", "exchange"},
        {"watch", "bridge", "ward", "keep"},
        {"wick", "delve", "forge", "pit"},
        {"haven", "court", "spire", "crown"},
        {"ford", "deep", "barrow", "hollow"}
    };
    uint32_t prefix_count = (uint32_t)(sizeof(prefixes) / sizeof(prefixes[0]));
    uint32_t function_index = (uint32_t)function;
    if (function_index >= 6U) function_index = 0U;
    const char *prefix = prefixes[NextRandom(sim) % prefix_count];
    const char *suffix = suffixes[function_index][NextRandom(sim) % 4U];
    (void)snprintf(destination, CC_NAME_CAPACITY, "%s%s", prefix, suffix);
}

static int32_t Jitter(CcSim *sim, int32_t center, int32_t radius)
{
    uint32_t span = (uint32_t)(radius * 2 + 1);
    return center + (int32_t)(NextRandom(sim) % span) - radius;
}

static bool EventIsPinned(const CcSim *sim, CcId event_id,
                          CcId incoming_parent)
{
    if (event_id == 0U) return false;
    if (event_id == incoming_parent ||
        event_id == sim->journey.parent_event_id ||
        event_id == sim->delayed_echo.parent_event_id ||
        event_id == sim->goblins.tribute_event_id ||
        event_id == sim->dragon.hoard_event_id ||
        event_id == sim->dragon.omen_event_id ||
        event_id == sim->dragon.lifecycle_event_id ||
        event_id == sim->hoard_raiders.cause_event_id ||
        event_id == sim->dragon_campaign.cause_event_id) return true;
    for (int32_t i = 0; i < sim->courier_count; ++i) {
        if (sim->couriers[i].cause_event_id == event_id) return true;
    }
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        if (sim->situations[i].cause_event_id == event_id) return true;
    }
    for (int32_t i = 0; i < sim->character_count; ++i) {
        const CcCharacter *character = &sim->characters[i];
        for (int32_t memory = 0; memory < character->memory_count; ++memory) {
            if (character->memories[memory].event_id == event_id) return true;
        }
    }
    return false;
}

static void RedirectEventReference(CcSim *sim, CcId removed_id,
                                   CcId replacement_id)
{
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        if (sim->situations[i].cause_event_id == removed_id) {
            sim->situations[i].cause_event_id = replacement_id;
        }
    }
    if (sim->journey.parent_event_id == removed_id) {
        sim->journey.parent_event_id = replacement_id;
    }
    if (sim->delayed_echo.parent_event_id == removed_id) {
        sim->delayed_echo.parent_event_id = replacement_id;
    }
    if (sim->goblins.tribute_event_id == removed_id) {
        sim->goblins.tribute_event_id = replacement_id;
    }
    if (sim->dragon.hoard_event_id == removed_id) {
        sim->dragon.hoard_event_id = replacement_id;
    }
    if (sim->dragon.omen_event_id == removed_id) {
        sim->dragon.omen_event_id = replacement_id;
    }
    if (sim->dragon.lifecycle_event_id == removed_id) {
        sim->dragon.lifecycle_event_id = replacement_id;
    }
    if (sim->hoard_raiders.cause_event_id == removed_id) {
        sim->hoard_raiders.cause_event_id = replacement_id;
    }
    if (sim->dragon_campaign.cause_event_id == removed_id) {
        sim->dragon_campaign.cause_event_id = replacement_id;
    }
    for (int32_t i = 0; i < sim->courier_count; ++i) {
        if (sim->couriers[i].cause_event_id == removed_id) {
            sim->couriers[i].cause_event_id = replacement_id;
        }
    }
    for (int32_t i = 0; i < sim->character_count; ++i) {
        CcCharacter *character = &sim->characters[i];
        for (int32_t memory = 0; memory < character->memory_count; ++memory) {
            if (character->memories[memory].event_id == removed_id) {
                character->memories[memory].event_id = replacement_id;
            }
        }
    }
}

static void CompactEventLedger(CcSim *sim, CcId incoming_parent)
{
    if (sim->event_count < CC_MAX_EVENTS) return;

    CcEvent ordered[CC_MAX_EVENTS];
    for (int32_t i = 0; i < sim->event_count; ++i) {
        const CcEvent *event = CcSimRecentEvent(
            sim, sim->event_count - 1 - i);
        ordered[i] = event != NULL ? *event : (CcEvent){0};
    }

    int32_t removed = -1;
    for (int32_t i = 0; i < sim->event_count; ++i) {
        if (!EventIsPinned(sim, ordered[i].id, incoming_parent)) {
            removed = i;
            break;
        }
    }
    if (removed < 0) removed = 0;

    CcId removed_id = ordered[removed].id;
    CcId replacement_id = ordered[removed].parent_id;
    if (replacement_id != 0U &&
        CcSimEvent(sim, replacement_id) == NULL) {
        replacement_id = 0U;
    }
    for (int32_t i = 0; i < sim->event_count; ++i) {
        if (i != removed && ordered[i].parent_id == removed_id) {
            ordered[i].parent_id = replacement_id;
        }
    }
    RedirectEventReference(sim, removed_id, replacement_id);

    for (int32_t i = removed; i + 1 < sim->event_count; ++i) {
        ordered[i] = ordered[i + 1];
    }
    sim->event_count -= 1;
    for (int32_t i = 0; i < sim->event_count; ++i) {
        sim->events[i] = ordered[i];
    }
    sim->events[sim->event_count] = (CcEvent){0};
    sim->event_write_index = sim->event_count;
}

static CcEvent *PushEvent(CcSim *sim, CcEventKind kind, CcId subject,
                          CcId location, CcId parent, int32_t magnitude,
                          const char *text)
{
    CompactEventLedger(sim, parent);
    if (parent != 0U && CcSimEvent(sim, parent) == NULL) parent = 0U;
    int32_t slot = sim->event_write_index;
    CcEvent *event = &sim->events[slot];
    *event = (CcEvent){0};
    event->id = NextId(sim, CC_ENTITY_EVENT);
    event->day = sim->current_day;
    event->kind = kind;
    event->subject_id = subject;
    event->location_id = location;
    event->parent_id = parent;
    event->magnitude = magnitude;
    (void)snprintf(event->text, sizeof(event->text), "%s", text);
    sim->event_write_index = (slot + 1) % CC_MAX_EVENTS;
    if (sim->event_count < CC_MAX_EVENTS) sim->event_count += 1;
    return event;
}

const char *CcGoodName(CcGood good)
{
    switch (good) {
        case CC_GOOD_FOOD: return "Food";
        case CC_GOOD_IRON: return "Iron";
        case CC_GOOD_TOOLS: return "Tools";
        case CC_GOOD_WEAPONS: return "Weapons";
        case CC_GOOD_GOLD: return "Raw Gold";
        case CC_GOOD_GEMS: return "Gems";
        case CC_GOOD_COUNT: break;
    }
    return "Unknown";
}

const char *CcSettlementFunctionName(CcSettlementFunction function)
{
    switch (function) {
        case CC_SETTLEMENT_FARMING: return "Agricultural basin";
        case CC_SETTLEMENT_MINING: return "Mining town";
        case CC_SETTLEMENT_MARKET: return "Crossroads market";
        case CC_SETTLEMENT_FORTRESS: return "Bridge fortress";
        case CC_SETTLEMENT_CAPITAL: return "Administrative capital";
        case CC_SETTLEMENT_DUNGEON_TOWN: return "Dungeon frontier";
    }
    return "Settlement";
}

const char *CcSettlementSizeName(CcSettlementSize size)
{
    switch (size) {
        case CC_SETTLEMENT_HAMLET: return "Hamlet";
        case CC_SETTLEMENT_VILLAGE: return "Village";
        case CC_SETTLEMENT_TOWN: return "Town";
        case CC_SETTLEMENT_CITY: return "City";
        case CC_SETTLEMENT_CAPITAL_SIZE: return "Capital";
    }
    return "Settlement";
}

const char *CcServiceName(CcServiceKind service)
{
    switch (service) {
        case CC_SERVICE_MARKET: return "Market";
        case CC_SERVICE_INN: return "Inn";
        case CC_SERVICE_GRANARY: return "Granary";
        case CC_SERVICE_SMITHY: return "Smithy";
        case CC_SERVICE_HEALER: return "Healer";
        case CC_SERVICE_STABLE: return "Stable";
        case CC_SERVICE_SHRINE: return "Shrine";
        case CC_SERVICE_BARRACKS: return "Barracks";
        case CC_SERVICE_CARTOGRAPHER: return "Cartographer";
        case CC_SERVICE_GUILDHALL: return "Guildhall";
        case CC_SERVICE_MINE: return "Mine";
        case CC_SERVICE_FARM: return "Farm";
        case CC_SERVICE_BLACK_MARKET: return "Black market";
        case CC_SERVICE_DUNGEON_WARD: return "Dungeon ward";
        case CC_SERVICE_NONE:
        case CC_SERVICE_COUNT: break;
    }
    return "No service";
}

const char *CcFactionKindName(CcFactionKind kind)
{
    switch (kind) {
        case CC_FACTION_CROWN: return "Court";
        case CC_FACTION_GUILD: return "Factors";
        case CC_FACTION_COMMONS: return "Commons";
    }
    return "Faction";
}

const char *CcKingdomCallingName(CcKingdomCalling calling)
{
    switch (calling) {
        case CC_KINGDOM_CALLING_ROAD: return "Road and Granary";
        case CC_KINGDOM_CALLING_IRON: return "Iron and Wall";
        case CC_KINGDOM_CALLING_DEEP: return "Capital and Deep";
        case CC_KINGDOM_CALLING_COUNT: break;
    }
    return "Unknown realm";
}

const char *CcBanditCampSizeName(CcBanditCampSize size)
{
    switch (size) {
        case CC_BANDIT_HIDEOUT: return "Hideout";
        case CC_BANDIT_CAMP: return "Camp";
        case CC_BANDIT_WAR_CAMP: return "War camp";
        case CC_BANDIT_OUTLAW_TOWN: return "Outlaw town";
    }
    return "Hideout";
}

const char *CcBanditRaidPhaseName(CcBanditRaidPhase phase)
{
    switch (phase) {
        case CC_BANDIT_RAID_IDLE: return "At camp";
        case CC_BANDIT_RAID_SCOUTING: return "Scouting";
        case CC_BANDIT_RAID_MUSTERING: return "Mustering";
        case CC_BANDIT_RAID_OUTBOUND: return "Raiding";
        case CC_BANDIT_RAID_RETURNING: return "Returning";
    }
    return "At camp";
}

int32_t CcSettlementServiceCapacity(CcSettlementSize size)
{
    switch (size) {
        case CC_SETTLEMENT_HAMLET: return 2;
        case CC_SETTLEMENT_VILLAGE: return 4;
        case CC_SETTLEMENT_TOWN: return 6;
        case CC_SETTLEMENT_CITY: return 9;
        case CC_SETTLEMENT_CAPITAL_SIZE: return 12;
    }
    return 0;
}

int32_t CcSettlementServiceCount(const CcSettlement *settlement)
{
    if (settlement == NULL) return 0;
    int32_t count = 0;
    for (int32_t service = 0; service < CC_SERVICE_COUNT; ++service) {
        if ((settlement->service_mask & (UINT32_C(1) << (uint32_t)service)) != 0U) {
            count += 1;
        }
    }
    return count;
}

bool CcSettlementHasService(const CcSettlement *settlement,
                            CcServiceKind service)
{
    return settlement != NULL && service >= 0 && service < CC_SERVICE_COUNT &&
        (settlement->service_mask & (UINT32_C(1) << (uint32_t)service)) != 0U;
}

int32_t CcBanditCampServiceCapacity(CcBanditCampSize size)
{
    switch (size) {
        case CC_BANDIT_HIDEOUT: return 2;
        case CC_BANDIT_CAMP: return 4;
        case CC_BANDIT_WAR_CAMP: return 6;
        case CC_BANDIT_OUTLAW_TOWN: return 8;
    }
    return 0;
}

const char *CcDungeonStateName(CcDungeonState state)
{
    switch (state) {
        case CC_DUNGEON_SEALED: return "Sealed";
        case CC_DUNGEON_DISTURBED: return "Disturbed";
        case CC_DUNGEON_EXPLORED: return "Explored";
        case CC_DUNGEON_PUBLIC_ROUTE: return "Public route";
        case CC_DUNGEON_SMUGGLER_ROUTE: return "Smuggler route";
        case CC_DUNGEON_RESEALED: return "Resealed";
    }
    return "Unknown";
}

const char *CcEventKindName(CcEventKind kind)
{
    switch (kind) {
        case CC_EVENT_HARVEST_FAILED: return "HARVEST";
        case CC_EVENT_ROUTE_CLOSED: return "ROUTE";
        case CC_EVENT_SHORTAGE: return "SHORTAGE";
        case CC_EVENT_SHIPMENT_DEPARTED: return "SHIPMENT";
        case CC_EVENT_SHIPMENT_ARRIVED: return "ARRIVAL";
        case CC_EVENT_BANDIT_PRESSURE: return "BANDITS";
        case CC_EVENT_MONSTER_PRESSURE: return "MONSTERS";
        case CC_EVENT_PLAYER_TRADE: return "TRADE";
        case CC_EVENT_PLAYER_TRAVEL: return "TRAVEL";
        case CC_EVENT_ROUTE_REPAIRED: return "REPAIRED";
        case CC_EVENT_DUNGEON_CHANGED: return "DUNGEON";
        case CC_EVENT_RELIEF: return "RELIEF";
        case CC_EVENT_SHIPMENT_LOST: return "AMBUSH";
        case CC_EVENT_KINGDOM_ACTION: return "DECREE";
        case CC_EVENT_ROUTE_DECAY: return "ROAD";
        case CC_EVENT_FACTION_SHIFT: return "POLITICS";
        case CC_EVENT_SITUATION_CREATED: return "CONTRACT";
        case CC_EVENT_SITUATION_RESOLVED: return "FULFILLED";
        case CC_EVENT_SITUATION_FAILED: return "FAILED";
        case CC_EVENT_PLAYER_AMBUSH: return "AMBUSH";
        case CC_EVENT_MAP_BOUGHT: return "CHART";
        case CC_EVENT_MAP_SOLD: return "CHART";
        case CC_EVENT_CHARTER_ACCEPTED: return "PROMISE";
        case CC_EVENT_CHARTER_ABANDONED: return "WITHDRAWN";
        case CC_EVENT_JOURNEY_ENCOUNTER: return "ROAD CRISIS";
        case CC_EVENT_ENCOUNTER_COMBAT: return "ROAD DEFENDED";
        case CC_EVENT_ENCOUNTER_NEGOTIATED: return "ROAD BARGAIN";
        case CC_EVENT_DELAYED_ECHO: return "RETURN ECHO";
        case CC_EVENT_JOURNEY_DEPARTED: return "DEPARTURE";
        case CC_EVENT_SERVICE_OPENED: return "NEW SERVICE";
        case CC_EVENT_BANDIT_RAID_DEPARTED: return "RAIDERS";
        case CC_EVENT_SETTLEMENT_RAIDED: return "RAID";
        case CC_EVENT_BANDIT_RAID_RETURNED: return "RAIDERS RETURN";
        case CC_EVENT_GOBLIN_TRIBUTE_DEPARTED: return "GOBLIN TITHE";
        case CC_EVENT_GOBLIN_TRIBUTE_TAKEN: return "GOBLIN THEFT";
        case CC_EVENT_GOBLIN_TRIBUTE_DELIVERED: return "DRAGON TRIBUTE";
        case CC_EVENT_DRAGON_HOARD_STOLEN: return "HOARD THEFT";
        case CC_EVENT_DRAGON_OMEN: return "DRAGON OMEN";
        case CC_EVENT_DRAGON_TREASURE_RETURNED: return "TREASURE RETURNED";
        case CC_EVENT_DRAGON_RETALIATION: return "DRAGON FIRE";
        case CC_EVENT_INEQUALITY_PRESSURE: return "INEQUALITY";
        case CC_EVENT_HOARD_HEIST_DEPARTED: return "HOARD RAIDERS";
        case CC_EVENT_HOARD_HEIST_RETURNED: return "STOLEN RELIEF";
        case CC_EVENT_WAR_PRESSURE: return "WAR LEVY";
        case CC_EVENT_WAR_CHEST_FUNDED: return "WAR CHEST";
        case CC_EVENT_WAR_SUPPLY_BOUGHT: return "WAR SUPPLY";
        case CC_EVENT_WAR_SUPPLY_SHORTAGE: return "ARMY SHORTAGE";
        case CC_EVENT_RESOURCE_EXTRACTED: return "MINE YIELD";
        case CC_EVENT_SMITH_PRODUCTION: return "SMITHY";
        case CC_EVENT_TREASURE_CRAFTED: return "TREASURE";
        case CC_EVENT_GOBLIN_RAID_DEPARTED: return "GOBLIN RAIDERS";
        case CC_EVENT_GOBLIN_RAIDED: return "GOBLIN RAID";
        case CC_EVENT_GOBLIN_RAID_RETURNED: return "GOBLINS RETURN";
        case CC_EVENT_WAR_MATERIEL_LOST: return "WAR LOSS";
        case CC_EVENT_IRON_LEDGER_LOAN: return "MONASTERY CREDIT";
        case CC_EVENT_IRON_LEDGER_REPAID: return "DEBT REPAID";
        case CC_EVENT_GOBLIN_HOARD_DEFENDED: return "GOBLIN GUARD";
        case CC_EVENT_COURIER_DEPARTED: return "COURIER";
        case CC_EVENT_COURIER_ARRIVED: return "NEWS ARRIVES";
        case CC_EVENT_COURIER_LOST: return "COURIER LOST";
        case CC_EVENT_COURIER_DISTORTED: return "RUMOR TWISTED";
        case CC_EVENT_WAR_DECLARED: return "WAR DECLARED";
        case CC_EVENT_PEACE_DECLARED: return "PEACE";
        case CC_EVENT_ALLIANCE_DECLARED: return "ALLIANCE";
        case CC_EVENT_DRAGON_MUSTERED: return "DRAGON HOST";
        case CC_EVENT_DRAGON_BATTLE: return "DRAGON BATTLE";
        case CC_EVENT_DRAGON_SLAIN: return "DRAGON SLAIN";
        case CC_EVENT_DRAGON_HOARD_RECOVERED: return "HOARD RECOVERED";
        case CC_EVENT_DRAGON_HUNT: return "DRAGON HUNT";
        case CC_EVENT_DRAGON_CROWNED: return "DRAGON CROWNED";
        case CC_EVENT_DRAGON_UNCROWNED: return "DRAGON UNCROWNED";
        case CC_EVENT_DRAGON_BROOD: return "DRAGON BROOD";
        case CC_EVENT_DRAGON_WHELP_DISPERSED: return "WHELPS DISPERSE";
        case CC_EVENT_DRAGON_AFTERSHOCK: return "AFTERDRAGON";
        case CC_EVENT_DRAGON_SUCCESSOR: return "DRAGON SUCCESSOR";
        case CC_EVENT_GOBLIN_CULT_RALLIED: return "CULT RALLIES";
        case CC_EVENT_GOBLIN_DRAGON_SEED: return "DRAGON SEED";
        case CC_EVENT_ENCOUNTER_WITHDRAWN: return "ROAD WITHDRAWAL";
        case CC_EVENT_COW_CALVING: return "CALVING";
        case CC_EVENT_COW_SLAUGHTERED: return "CATTLE LOSS";
        case CC_EVENT_HORSE_BRED: return "HORSE BRED";
        case CC_EVENT_FOAL_BORN: return "FOAL BORN";
        case CC_EVENT_HORSE_TEAM_CHANGED: return "HORSE TEAM";
        case CC_EVENT_GOBLIN_RAID_PREPARED: return "GOBLIN MUSTER";
        case CC_EVENT_GOBLIN_TARGET_WARNED: return "RAID WARNING";
        case CC_EVENT_GOBLIN_EXPEDITION_INTERCEPTED: return "RAID INTERCEPTED";
        case CC_EVENT_GOBLIN_TRADE: return "GOBLIN TRADE";
        case CC_EVENT_GOBLIN_DRAGON_SEED_RUMORED: return "ASH-VAULT RUMOR";
        case CC_EVENT_GOBLIN_DRAGON_SEED_PREPARED: return "ASH-VAULT WORK";
        case CC_EVENT_CHARACTER_INTERACTION: return "PERSONAL WORD";
        case CC_EVENT_JOURNEY_WARNING: return "ROAD WARNING";
        case CC_EVENT_AMBUSH_EVADED: return "AMBUSH EVADED";
        case CC_EVENT_ENCOUNTER_LOOT: return "ROAD LOOT";
    }
    return "EVENT";
}

const char *CcCharacterRoleName(CcCharacterRole role)
{
    switch (role) {
        case CC_CHARACTER_OFFICIAL: return "official";
        case CC_CHARACTER_LABORER: return "laborer";
        case CC_CHARACTER_SCOUT: return "scout";
        case CC_CHARACTER_TRAVELLER: return "traveller";
        case CC_CHARACTER_REFUGEE: return "refugee";
        case CC_CHARACTER_COURIER: return "courier";
    }
    return "resident";
}

const char *CcCharacterActivityName(CcCharacterActivity activity)
{
    switch (activity) {
        case CC_CHARACTER_ACTIVITY_WORKING: return "working";
        case CC_CHARACTER_ACTIVITY_SEEKING_AID: return "seeking aid";
        case CC_CHARACTER_ACTIVITY_PREPARING: return "preparing";
        case CC_CHARACTER_ACTIVITY_RECOVERING: return "recovering";
        case CC_CHARACTER_ACTIVITY_HIDING: return "keeping out of sight";
        case CC_CHARACTER_ACTIVITY_TRAVELLING: return "travelling";
    }
    return "waiting";
}

const char *CcDragonLifeStageName(CcDragonLifeStage stage)
{
    switch (stage) {
        case CC_DRAGON_STAGE_EGG: return "egg";
        case CC_DRAGON_STAGE_WHELP: return "whelp";
        case CC_DRAGON_STAGE_WANDERER: return "wanderer";
        case CC_DRAGON_STAGE_CROWNED: return "Crowned dragon";
        case CC_DRAGON_STAGE_DEEP_WYRM: return "Deep Wyrm";
        case CC_DRAGON_STAGE_UNCROWNED: return "Uncrowned dragon";
        case CC_DRAGON_STAGE_AFTERDRAGON: return "Afterdragon";
    }
    return "dragon";
}

const char *CcDragonActivityName(CcDragonActivity activity)
{
    switch (activity) {
        case CC_DRAGON_ACTIVITY_DORMANT: return "dormant";
        case CC_DRAGON_ACTIVITY_HUNTING: return "hunting";
        case CC_DRAGON_ACTIVITY_RETALIATING: return "retaliating";
        case CC_DRAGON_ACTIVITY_BROODING: return "brooding";
        case CC_DRAGON_ACTIVITY_AFTERMATH: return "an aftermath";
    }
    return "unknown";
}

const char *CcSituationKindName(CcSituationKind kind)
{
    switch (kind) {
        case CC_SITUATION_RELIEF_DELIVERY: return "Relief charter";
        case CC_SITUATION_ROUTE_REPAIR: return "Road compact";
        case CC_SITUATION_MONSTER_EXPEDITION: return "Depth warrant";
        case CC_SITUATION_BLACK_MARKET_DELIVERY: return "Quiet commission";
        case CC_SITUATION_COURIER_DELIVERY: return "Sealed dispatch";
    }
    return "Unknown situation";
}

const CcSettlement *CcSimSettlement(const CcSim *sim, CcId id)
{
    if (sim == NULL || CcIdKind(id) != CC_ENTITY_SETTLEMENT) return NULL;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        if (sim->settlements[i].id == id) return &sim->settlements[i];
    }
    return NULL;
}

CcSettlement *CcSimSettlementMutable(CcSim *sim, CcId id)
{
    return (CcSettlement *)CcSimSettlement((const CcSim *)sim, id);
}

const CcRoute *CcSimRoute(const CcSim *sim, CcId id)
{
    if (sim == NULL || CcIdKind(id) != CC_ENTITY_ROUTE) return NULL;
    for (int32_t i = 0; i < sim->route_count; ++i) {
        if (sim->routes[i].id == id) return &sim->routes[i];
    }
    return NULL;
}

static CcRoute *RouteMutable(CcSim *sim, CcId id)
{
    return (CcRoute *)CcSimRoute((const CcSim *)sim, id);
}

static CcRoute *DungeonRouteMutable(CcSim *sim, const CcDungeon *dungeon)
{
    if (sim == NULL || dungeon == NULL) return NULL;
    CcRoute *dungeon_route = NULL;
    for (int32_t i = 0; i < sim->route_count; ++i) {
        CcRoute *route = &sim->routes[i];
        if (route->from_id != dungeon->settlement_id &&
            route->to_id != dungeon->settlement_id) continue;
        if (dungeon_route == NULL ||
            route->capacity < dungeon_route->capacity) {
            dungeon_route = route;
        }
    }
    return dungeon_route;
}

typedef struct CcDungeonEffects {
    int32_t mine_production;
    int32_t prosperity;
    int32_t route_capacity;
    int32_t route_security;
    int32_t route_condition;
    int32_t bandit_supplies;
    int32_t bandit_influence;
    int32_t hunting_pressure;
    int32_t population_loss;
    int32_t monster_pressure_loss;
} CcDungeonEffects;

static CcDungeonEffects DungeonEffects(CcDungeonState state)
{
    switch (state) {
        case CC_DUNGEON_EXPLORED:
            return (CcDungeonEffects){
                .hunting_pressure = 8,
                .population_loss = 2,
                .monster_pressure_loss = 4
            };
        case CC_DUNGEON_PUBLIC_ROUTE:
            return (CcDungeonEffects){
                .mine_production = 3,
                .prosperity = 4,
                .route_capacity = 6,
                .route_security = 10,
                .route_condition = 15,
                .hunting_pressure = 24,
                .population_loss = 8,
                .monster_pressure_loss = 14
            };
        case CC_DUNGEON_SMUGGLER_ROUTE:
            return (CcDungeonEffects){
                .prosperity = 2,
                .route_capacity = 4,
                .route_condition = 8,
                .bandit_supplies = 8,
                .bandit_influence = 10,
                .hunting_pressure = 12,
                .population_loss = 4,
                .monster_pressure_loss = 8
            };
        case CC_DUNGEON_RESEALED:
            return (CcDungeonEffects){
                .mine_production = -4,
                .prosperity = -3,
                .hunting_pressure = 40,
                .population_loss = 16,
                .monster_pressure_loss = 25
            };
        default:
            return (CcDungeonEffects){0};
    }
}

const CcRoute *CcSimRouteBetween(const CcSim *sim, CcId a, CcId b)
{
    if (sim == NULL) return NULL;
    for (int32_t i = 0; i < sim->route_count; ++i) {
        const CcRoute *route = &sim->routes[i];
        if ((route->from_id == a && route->to_id == b) ||
            (route->from_id == b && route->to_id == a)) return route;
    }
    return NULL;
}

static int32_t KingdomSlotById(const CcSim *sim, CcId id)
{
    if (sim == NULL) return -1;
    for (int32_t i = 0; i < sim->kingdom_count; ++i) {
        if (sim->kingdoms[i].id == id) return i;
    }
    return -1;
}

const char *CcDiplomaticStateName(CcDiplomaticState state)
{
    switch (state) {
        case CC_DIPLOMACY_PEACE: return "peace";
        case CC_DIPLOMACY_WAR: return "war";
        case CC_DIPLOMACY_ALLIANCE: return "alliance";
    }
    return "unknown";
}

bool CcSimKingdomsAtWar(const CcSim *sim, CcId first, CcId second)
{
    int32_t first_slot = KingdomSlotById(sim, first);
    int32_t second_slot = KingdomSlotById(sim, second);
    return first_slot >= 0 && second_slot >= 0 && first_slot != second_slot &&
        sim->diplomacy[first_slot][second_slot] == CC_DIPLOMACY_WAR;
}

bool CcSimKingdomsAllied(const CcSim *sim, CcId first, CcId second)
{
    int32_t first_slot = KingdomSlotById(sim, first);
    int32_t second_slot = KingdomSlotById(sim, second);
    return first_slot >= 0 && second_slot >= 0 && first_slot != second_slot &&
        sim->diplomacy[first_slot][second_slot] == CC_DIPLOMACY_ALLIANCE;
}

CcKingdomCalling CcSimKingdomCalling(const CcSim *sim, CcId kingdom_id)
{
    if (sim == NULL || KingdomSlotById(sim, kingdom_id) < 0) {
        return CC_KINGDOM_CALLING_COUNT;
    }
    bool holds_fortress = false;
    bool holds_mine = false;
    bool holds_capital = false;
    bool holds_dungeon_frontier = false;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        const CcSettlement *place = &sim->settlements[i];
        if (place->kingdom_id != kingdom_id) continue;
        holds_fortress = holds_fortress ||
            place->function == CC_SETTLEMENT_FORTRESS;
        holds_mine = holds_mine ||
            place->function == CC_SETTLEMENT_MINING;
        holds_capital = holds_capital ||
            place->function == CC_SETTLEMENT_CAPITAL;
        holds_dungeon_frontier = holds_dungeon_frontier ||
            place->function == CC_SETTLEMENT_DUNGEON_TOWN;
    }
    if (holds_capital || holds_dungeon_frontier) {
        return CC_KINGDOM_CALLING_DEEP;
    }
    if (holds_fortress || holds_mine) return CC_KINGDOM_CALLING_IRON;
    return CC_KINGDOM_CALLING_ROAD;
}

bool CcSimRouteCrossesWarBorder(const CcSim *sim, CcId route_id)
{
    const CcRoute *route = CcSimRoute(sim, route_id);
    const CcSettlement *from = route != NULL ?
        CcSimSettlement(sim, route->from_id) : NULL;
    const CcSettlement *to = route != NULL ?
        CcSimSettlement(sim, route->to_id) : NULL;
    return from != NULL && to != NULL &&
        CcSimKingdomsAtWar(sim, from->kingdom_id, to->kingdom_id);
}

int32_t CcSimWarBurdenAtSettlement(const CcSim *sim, CcId settlement_id)
{
    const CcSettlement *place = CcSimSettlement(sim, settlement_id);
    if (sim == NULL || place == NULL) return 0;
    int32_t burden = place->function == CC_SETTLEMENT_FORTRESS ? 10 :
                     place->function == CC_SETTLEMENT_CAPITAL ? 5 : 0;
    for (int32_t i = 0; i < sim->route_count; ++i) {
        const CcRoute *route = &sim->routes[i];
        if (route->from_id != settlement_id &&
            route->to_id != settlement_id) continue;
        if (!CcSimRouteCrossesWarBorder(sim, route->id)) continue;
        burden += 10 + (100 - route->security) / 12;
        if (route->closed) burden += 12;
    }
    return ClampI32(burden, 0, 100);
}

static bool IsWarSeat(const CcSettlement *place)
{
    return place != NULL &&
        (place->function == CC_SETTLEMENT_FORTRESS ||
         place->function == CC_SETTLEMENT_CAPITAL);
}

static int32_t WarWeeklyNeed(const CcSim *sim, const CcSettlement *place,
                             CcGood good)
{
    if (!IsWarSeat(place)) return 0;
    int32_t burden = CcSimWarBurdenAtSettlement(sim, place->id);
    if (burden < 20) return 0;
    if (good == CC_GOOD_FOOD) {
        return MinimumI32(place->consumption[CC_GOOD_FOOD],
                          MaximumI32(1, burden / 10)) +
               MaximumI32(1, burden / 25);
    }
    if (good == CC_GOOD_TOOLS) {
        return place->consumption[CC_GOOD_TOOLS] +
               (burden >= 50 ? 1 : 0);
    }
    if (good == CC_GOOD_WEAPONS) {
        return burden >= 20 ? 1 + burden / 35 : 0;
    }
    return 0;
}

static int32_t WarExtraConsumption(const CcSim *sim,
                                   const CcSettlement *place,
                                   CcGood good)
{
    if (!IsWarSeat(place)) return 0;
    int32_t burden = CcSimWarBurdenAtSettlement(sim, place->id);
    if (burden < 20) return 0;
    if (good == CC_GOOD_FOOD) return MaximumI32(1, burden / 25);
    if (good == CC_GOOD_TOOLS) return burden >= 50 ? 1 : 0;
    return 0;
}

static int32_t EffectiveReserveTarget(const CcSim *sim,
                                      const CcSettlement *place,
                                      CcGood good)
{
    if (place == NULL || good < 0 || good >= CC_GOOD_COUNT) return 0;
    return place->reserve_target[good] +
           WarExtraConsumption(sim, place, good) * 3;
}

static int32_t CivilianFoodUse(const CcSettlement *place)
{
    if (place == NULL || place->consumption[CC_GOOD_FOOD] <= 0) return 0;
    if (place->population >= 600) {
        return place->consumption[CC_GOOD_FOOD];
    }
    int32_t population_use = MaximumI32(
        1, (place->population + 299) / 300);
    return MinimumI32(
        place->consumption[CC_GOOD_FOOD], population_use);
}

static int32_t SettlementPopulationCapacity(const CcSettlement *place)
{
    if (place == NULL) return 0;
    static const int32_t capacity[] = {900, 2200, 4200, 7000, 9500};
    int32_t result = capacity[place->size];
    if (place->function == CC_SETTLEMENT_FARMING) result += 600;
    if (place->function == CC_SETTLEMENT_MARKET) result += 400;
    return result;
}

static int32_t WeeklyFoodUse(const CcSim *sim,
                             const CcSettlement *place)
{
    if (place == NULL) return 1;
    return MaximumI32(
        1, CivilianFoodUse(place) +
           WarExtraConsumption(sim, place, CC_GOOD_FOOD));
}

static int32_t FoodStorageCapacity(const CcSim *sim,
                                   const CcSettlement *place)
{
    int32_t storage_weeks = CcSettlementHasService(
        place, CC_SERVICE_GRANARY) ? 32 : 12;
    return WeeklyFoodUse(sim, place) * storage_weeks;
}

static int32_t SpoilStoredFood(const CcSim *sim, CcSettlement *place)
{
    int32_t stored = place->stock[CC_GOOD_FOOD];
    int32_t spoiled = stored / 100;
    stored -= spoiled;
    int32_t capacity = FoodStorageCapacity(sim, place);
    if (stored > capacity) {
        spoiled += stored - capacity;
        stored = capacity;
    }
    place->stock[CC_GOOD_FOOD] = stored;
    return spoiled;
}

static int32_t WarWeeklyWage(const CcSim *sim, const CcSettlement *place)
{
    if (!IsWarSeat(place)) return 0;
    int32_t burden = CcSimWarBurdenAtSettlement(sim, place->id);
    return burden >= 20 ? 1 + burden / 18 : 0;
}

int32_t CcSimWarSupplyCrisisAtSettlement(const CcSim *sim,
                                         CcId settlement_id)
{
    const CcSettlement *place = CcSimSettlement(sim, settlement_id);
    if (sim == NULL || !IsWarSeat(place)) return 0;
    int32_t burden = CcSimWarBurdenAtSettlement(sim, settlement_id);
    int32_t food_need = WarWeeklyNeed(sim, place, CC_GOOD_FOOD);
    int32_t tool_need = WarWeeklyNeed(sim, place, CC_GOOD_TOOLS);
    int32_t weapon_need = WarWeeklyNeed(sim, place, CC_GOOD_WEAPONS);
    int32_t wage = WarWeeklyWage(sim, place);
    if (burden < 20 || food_need < 1 || wage < 1) return 0;
    int32_t food_gap = MaximumI32(0, food_need * 3 -
                                  place->stock[CC_GOOD_FOOD]);
    int32_t tool_gap = MaximumI32(0, tool_need * 3 -
                                  place->stock[CC_GOOD_TOOLS]);
    int32_t weapon_gap = MaximumI32(0, weapon_need * 3 -
                                    place->stock[CC_GOOD_WEAPONS]);
    int32_t wage_gap = place->war_chest < wage * 2 ?
                       (int32_t)(wage * 2 - place->war_chest) : 0;
    int32_t score = burden / 3 + food_gap * 4 + tool_gap * 8 +
                    weapon_gap * 10 + wage_gap * 6;
    return ClampI32(score, 0, 100);
}

int32_t CcSimKingdomPressure(const CcSim *sim, CcId kingdom_id)
{
    if (sim == NULL || KingdomSlotById(sim, kingdom_id) < 0) return 0;
    CcKingdomCalling calling = CcSimKingdomCalling(sim, kingdom_id);
    int32_t pressure = 0;
    const CcKingdom *kingdom = NULL;
    for (int32_t i = 0; i < sim->kingdom_count; ++i) {
        if (sim->kingdoms[i].id == kingdom_id) {
            kingdom = &sim->kingdoms[i];
            break;
        }
    }
    if (kingdom != NULL) {
        pressure = MaximumI32(pressure, 100 - kingdom->legitimacy);
        CcMoney credit_limit = 480 + (CcMoney)kingdom->legitimacy * 4;
        if (credit_limit > 0) {
            CcMoney debt_pressure = kingdom->iron_ledger_debt * 100 /
                                    credit_limit;
            pressure = MaximumI32(
                pressure, debt_pressure > INT32_MAX ? INT32_MAX :
                (int32_t)debt_pressure);
        }
    }
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        const CcSettlement *place = &sim->settlements[i];
        if (place->kingdom_id != kingdom_id) continue;
        pressure = MaximumI32(pressure, place->hunger);
        int32_t food_target = place->reserve_target[CC_GOOD_FOOD];
        if (food_target > 0 && place->stock[CC_GOOD_FOOD] < food_target) {
            int32_t food_gap =
                (food_target - place->stock[CC_GOOD_FOOD]) * 100 /
                food_target;
            pressure = MaximumI32(pressure, food_gap);
        }
        if (calling == CC_KINGDOM_CALLING_IRON &&
            (place->function == CC_SETTLEMENT_FORTRESS ||
             place->function == CC_SETTLEMENT_CAPITAL)) {
            pressure = MaximumI32(
                pressure,
                CcSimWarSupplyCrisisAtSettlement(sim, place->id));
            pressure = MaximumI32(
                pressure, CcSimWarBurdenAtSettlement(sim, place->id));
        }
    }
    if (calling == CC_KINGDOM_CALLING_ROAD) {
        for (int32_t i = 0; i < sim->route_count; ++i) {
            const CcRoute *route = &sim->routes[i];
            const CcSettlement *from = CcSimSettlement(sim, route->from_id);
            const CcSettlement *to = CcSimSettlement(sim, route->to_id);
            if ((from == NULL || from->kingdom_id != kingdom_id) &&
                (to == NULL || to->kingdom_id != kingdom_id)) continue;
            int32_t route_pressure = (100 - route->condition) / 2 +
                                     (100 - route->security) / 4;
            if (route->closed) route_pressure += 20;
            if (CcSimRouteCrossesWarBorder(sim, route->id)) {
                route_pressure += 10;
            }
            pressure = MaximumI32(pressure, route_pressure);
        }
    } else if (calling == CC_KINGDOM_CALLING_DEEP) {
        for (int32_t i = 0; i < sim->dungeon_count; ++i) {
            const CcDungeon *dungeon = &sim->dungeons[i];
            const CcSettlement *place = CcSimSettlement(
                sim, dungeon->settlement_id);
            if (place == NULL || place->kingdom_id != kingdom_id) continue;
            pressure = MaximumI32(pressure, dungeon->regional_pressure);
            for (int32_t monster = 0; monster < sim->monster_count;
                 ++monster) {
                if (sim->monsters[monster].dungeon_id == dungeon->id) {
                    pressure = MaximumI32(
                        pressure, sim->monsters[monster].pressure);
                }
            }
        }
        const CcSettlement *lair = CcSimSettlement(
            sim, sim->dragon.lair_settlement_id);
        if (lair != NULL && lair->kingdom_id == kingdom_id) {
            pressure = MaximumI32(
                pressure, sim->dragon.regional_influence);
        }
    }
    return ClampI32(pressure, 0, 100);
}

CcMoney CcSimTrackedGold(const CcSim *sim)
{
    if (sim == NULL) return 0;
    CcMoney total = sim->player.coins + sim->dragon.hoard +
                    sim->iron_ledger_reserve +
                    sim->goblins.carried_tribute +
                    sim->goblins.lair_coins +
                    sim->hoard_raiders.carried_treasure +
                    sim->dragon_campaign.recovered_coins;
    for (int32_t i = 0; i < sim->kingdom_count; ++i) {
        total += sim->kingdoms[i].treasury;
    }
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        total += sim->settlements[i].market_coins;
        total += sim->settlements[i].war_chest;
    }
    return total;
}

int32_t CcSimTrackedGood(const CcSim *sim, CcGood good)
{
    if (sim == NULL || good < 0 || good >= CC_GOOD_COUNT) return 0;
    int64_t total = sim->player.cargo[good] +
                    sim->goblins.carried_goods[good] +
                    sim->goblins.lair_stock[good] +
                    sim->dragon.hoard_goods[good] +
                    sim->dragon_campaign.supplies[good];
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        total += sim->settlements[i].stock[good];
        if (good == CC_GOOD_IRON) total += sim->settlements[i].iron_deposit;
    }
    for (int32_t i = 0; i < sim->shipment_count; ++i) {
        if (sim->shipments[i].status == CC_SHIPMENT_TRAVELLING &&
            sim->shipments[i].good == good) total += sim->shipments[i].quantity;
    }
    if (good == CC_GOOD_GOLD || good == CC_GOOD_GEMS) {
        for (int32_t i = 0; i < sim->treasure_count; ++i) {
            const CcTreasure *treasure = &sim->treasures[i];
            if (treasure->destroyed) continue;
            total += good == CC_GOOD_GOLD ? treasure->gold_content :
                                           treasure->gem_content;
        }
        for (int32_t i = 0; i < sim->settlement_count; ++i) {
            total += good == CC_GOOD_GOLD ?
                sim->settlements[i].treasure_gold_committed :
                sim->settlements[i].treasure_gems_committed;
        }
    }
    return total > INT32_MAX ? INT32_MAX : (int32_t)total;
}

static CcKingdom *KingdomMutable(CcSim *sim, CcId id)
{
    if (sim == NULL) return NULL;
    for (int32_t i = 0; i < sim->kingdom_count; ++i) {
        if (sim->kingdoms[i].id == id) return &sim->kingdoms[i];
    }
    return NULL;
}

static CcMoney IronLedgerCreditLimit(const CcKingdom *kingdom)
{
    if (kingdom == NULL) return 0;
    return 480 + (CcMoney)kingdom->legitimacy * 4;
}

static CcMoney IronLedgerCreditAvailable(const CcSim *sim,
                                         const CcKingdom *kingdom)
{
    if (sim == NULL || kingdom == NULL || sim->iron_ledger_reserve <= 0) {
        return 0;
    }
    CcMoney headroom = IronLedgerCreditLimit(kingdom) -
                       kingdom->iron_ledger_debt;
    if (headroom <= 0) return 0;
    return MinimumI32(
        headroom > INT32_MAX ? INT32_MAX : (int32_t)headroom,
        sim->iron_ledger_reserve > INT32_MAX ? INT32_MAX :
            (int32_t)sim->iron_ledger_reserve);
}

static int32_t IronLedgerDebtPressure(const CcSim *sim, CcId kingdom_id)
{
    if (sim == NULL) return 0;
    for (int32_t i = 0; i < sim->kingdom_count; ++i) {
        const CcKingdom *kingdom = &sim->kingdoms[i];
        if (kingdom->id != kingdom_id) continue;
        CcMoney limit = IronLedgerCreditLimit(kingdom);
        if (limit <= 0) return 0;
        CcMoney pressure = kingdom->iron_ledger_debt * 100 / limit;
        return ClampI32(
            pressure > INT32_MAX ? INT32_MAX : (int32_t)pressure, 0, 100);
    }
    return 0;
}

static bool IronLedgerWillFund(const CcSettlement *place, CcGood good)
{
    if (place == NULL) return false;
    if (good == CC_GOOD_FOOD) return place->hunger >= 65;
    if (good != CC_GOOD_TOOLS || place->stock[CC_GOOD_TOOLS] > 0) {
        return false;
    }
    return place->hunger >= 40 ||
           CcSettlementHasService(place, CC_SERVICE_FARM) ||
           CcSettlementHasService(place, CC_SERVICE_MINE) ||
           CcSettlementHasService(place, CC_SERVICE_SMITHY);
}

int32_t CcSimInequalityAtSettlement(const CcSim *sim, CcId settlement_id)
{
    const CcSettlement *place = CcSimSettlement(sim, settlement_id);
    if (sim == NULL || place == NULL) return 0;
    const CcKingdom *kingdom = NULL;
    for (int32_t i = 0; i < sim->kingdom_count; ++i) {
        if (sim->kingdoms[i].id == place->kingdom_id) {
            kingdom = &sim->kingdoms[i];
            break;
        }
    }
    int32_t commons_support = 50;
    for (int32_t i = 0; i < sim->faction_count; ++i) {
        const CcFaction *faction = &sim->factions[i];
        if (faction->kingdom_id == place->kingdom_id &&
            faction->kind == CC_FACTION_COMMONS) {
            commons_support = faction->support;
            break;
        }
    }
    int32_t treasury_pressure = 0;
    if (kingdom != NULL && kingdom->treasury > 0) {
        CcMoney scaled = kingdom->treasury / 80;
        treasury_pressure = scaled > 12 ? 12 : (int32_t)scaled;
    }
    CcMoney visible_scaled = place->market_coins / 30;
    int32_t visible_wealth = visible_scaled > 18 ? 18 :
                             (int32_t)visible_scaled;
    int32_t price_pressure = MaximumI32(
        0, (place->price[CC_GOOD_FOOD] - 4) * 2);
    price_pressure = MinimumI32(price_pressure, 15);
    int32_t score = place->prosperity / 4 + place->hunger / 2 +
                    treasury_pressure + (100 - commons_support) / 4 +
                    price_pressure + visible_wealth;
    return ClampI32(score, 0, 100);
}

bool CcSimStartServiceProject(CcSim *sim, CcId settlement_id,
                              CcServiceKind service,
                              char *error, size_t error_capacity)
{
    CcSettlement *settlement = CcSimSettlementMutable(sim, settlement_id);
    if (settlement == NULL || service < 0 || service >= CC_SERVICE_COUNT) {
        SetError(error, error_capacity, "Settlement service project is invalid.");
        return false;
    }
    if (settlement->service_project != CC_SERVICE_NONE) {
        SetError(error, error_capacity, "This settlement is already building a service.");
        return false;
    }
    if (CcSettlementHasService(settlement, service)) {
        SetError(error, error_capacity, "This service is already open.");
        return false;
    }
    if (CcSettlementServiceCount(settlement) >=
        CcSettlementServiceCapacity(settlement->size)) {
        SetError(error, error_capacity, "This settlement has no free service slots.");
        return false;
    }
    CcKingdom *kingdom = KingdomMutable(sim, settlement->kingdom_id);
    const int32_t material_cost = 12;
    const int32_t tool_cost = 5;
    const CcMoney money_cost = 80;
    if (kingdom == NULL || settlement->stock[CC_GOOD_MATERIAL] < material_cost ||
        settlement->stock[CC_GOOD_TOOLS] < tool_cost ||
        kingdom->treasury < money_cost) {
        SetError(error, error_capacity,
                 "The town needs 12 Iron, 5 Tools, and 80 crowns.");
        return false;
    }
    settlement->stock[CC_GOOD_MATERIAL] -= material_cost;
    settlement->stock[CC_GOOD_TOOLS] -= tool_cost;
    kingdom->treasury -= money_cost;
    settlement->market_coins += money_cost;
    settlement->service_project = service;
    settlement->service_project_days = 7;
    SetError(error, error_capacity, "");
    return true;
}

const CcMap *CcSimMap(const CcSim *sim, CcId id)
{
    if (sim == NULL || CcIdKind(id) != CC_ENTITY_MAP) return NULL;
    for (int32_t i = 0; i < sim->map_count; ++i) {
        if (sim->maps[i].id == id) return &sim->maps[i];
    }
    return NULL;
}

static int32_t MapSlot(const CcSim *sim, const CcMap *map)
{
    if (sim == NULL || map == NULL) return -1;
    for (int32_t i = 0; i < sim->map_count; ++i) {
        if (&sim->maps[i] == map || sim->maps[i].id == map->id) return i;
    }
    return -1;
}

static uint32_t MapSlotBit(int32_t slot)
{
    return slot >= 0 && slot < CC_MAP_COLLECTION_COUNT ?
        UINT32_C(1) << (uint32_t)slot : 0U;
}

bool CcSimMapIsCatalogued(const CcSim *sim, const CcMap *map)
{
    int32_t slot = MapSlot(sim, map);
    return slot >= 0 &&
           (sim->player.map_catalogue_mask & MapSlotBit(slot)) != 0U;
}

bool CcSimMapIsArchived(const CcSim *sim, const CcMap *map)
{
    int32_t slot = MapSlot(sim, map);
    return slot >= 0 &&
           (sim->player.map_archive_mask & MapSlotBit(slot)) != 0U;
}

static CcMap *MapMutable(CcSim *sim, CcId id)
{
    return (CcMap *)CcSimMap((const CcSim *)sim, id);
}

static CcCourier *CourierMutable(CcSim *sim, CcId id)
{
    if (sim == NULL || CcIdKind(id) != CC_ENTITY_COURIER) return NULL;
    for (int32_t i = 0; i < sim->courier_count; ++i) {
        if (sim->couriers[i].id == id) return &sim->couriers[i];
    }
    return NULL;
}

const CcMap *CcSimMapForRoute(const CcSim *sim, CcId route_id, CcId owner_id)
{
    if (sim == NULL) return NULL;
    for (int32_t i = 0; i < sim->map_count; ++i) {
        const CcMap *map = &sim->maps[i];
        if (map->route_id != route_id || map->owner_id != owner_id) continue;
        if (owner_id == sim->player.id &&
            (!CcSimMapIsCatalogued(sim, map) ||
             CcSimMapIsArchived(sim, map))) continue;
        return map;
    }
    return NULL;
}

const CcEvent *CcSimRecentEvent(const CcSim *sim, int32_t offset)
{
    if (sim == NULL || offset < 0 || offset >= sim->event_count) return NULL;
    int32_t slot = sim->event_write_index - 1 - offset;
    while (slot < 0) slot += CC_MAX_EVENTS;
    return &sim->events[slot];
}

const CcEvent *CcSimEvent(const CcSim *sim, CcId id)
{
    if (sim == NULL || CcIdKind(id) != CC_ENTITY_EVENT) return NULL;
    for (int32_t offset = 0; offset < sim->event_count; ++offset) {
        const CcEvent *event = CcSimRecentEvent(sim, offset);
        if (event != NULL && event->id == id) return event;
    }
    return NULL;
}

const CcSituation *CcSimSituation(const CcSim *sim, CcId id)
{
    if (sim == NULL || CcIdKind(id) != CC_ENTITY_SITUATION) return NULL;
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        if (sim->situations[i].id == id) return &sim->situations[i];
    }
    return NULL;
}

const CcCharacter *CcSimCharacter(const CcSim *sim, CcId id)
{
    if (sim == NULL || CcIdKind(id) != CC_ENTITY_CHARACTER) return NULL;
    for (int32_t i = 0; i < sim->character_count; ++i) {
        if (sim->characters[i].id == id) return &sim->characters[i];
    }
    return NULL;
}

static CcCharacter *CharacterMutable(CcSim *sim, CcId id)
{
    return (CcCharacter *)CcSimCharacter(sim, id);
}

const CcCharacter *CcSimSituationSponsorCharacter(
    const CcSim *sim, const CcSituation *situation)
{
    return situation != NULL ?
        CcSimCharacter(sim, situation->sponsor_character_id) : NULL;
}

const CcCharacter *CcSimSituationAffectedCharacter(
    const CcSim *sim, const CcSituation *situation)
{
    return situation != NULL ?
        CcSimCharacter(sim, situation->affected_character_id) : NULL;
}

const CcCharacter *CcSimSituationConversationCharacter(
    const CcSim *sim, const CcSituation *situation, CcId settlement_id)
{
    if (sim == NULL || situation == NULL || settlement_id == 0U) return NULL;
    const CcCharacter *sponsor = CcSimSituationSponsorCharacter(
        sim, situation);
    const CcCharacter *affected = CcSimSituationAffectedCharacter(
        sim, situation);
    bool sponsor_here = sponsor != NULL &&
        sponsor->current_settlement_id == settlement_id;
    bool affected_here = affected != NULL &&
        affected->current_settlement_id == settlement_id;
    CcId offer_settlement = CcSimSituationOfferSettlementId(sim, situation);

    if (affected_here &&
        (situation->kind == CC_SITUATION_MONSTER_EXPEDITION ||
         situation->kind == CC_SITUATION_COURIER_DELIVERY ||
         !sponsor_here || settlement_id != offer_settlement)) {
        return affected;
    }
    if (sponsor_here && settlement_id == offer_settlement) return sponsor;
    if (affected_here && CcSimSituationTouchesSettlement(
            sim, situation, settlement_id)) return affected;
    return sponsor_here ? sponsor : NULL;
}

bool CcCharacterRemembers(const CcCharacter *character,
                          CcCharacterMemoryKind kind, CcId subject_id)
{
    if (character == NULL || kind == CC_CHARACTER_MEMORY_NONE) return false;
    for (int32_t i = 0; i < character->memory_count; ++i) {
        const CcCharacterMemory *memory = &character->memories[i];
        if (memory->kind == kind && memory->subject_id == subject_id) {
            return true;
        }
    }
    return false;
}

const CcSituation *CcSimAcceptedSituation(const CcSim *sim)
{
    if (sim == NULL) return NULL;
    const CcSituation *situation = CcSimSituation(
        sim, sim->player.accepted_situation_id);
    return situation != NULL && situation->status == CC_SITUATION_ACTIVE ?
           situation : NULL;
}

CcId CcSimSituationOfferSettlementId(const CcSim *sim,
                                     const CcSituation *situation)
{
    if (sim == NULL || situation == NULL) return 0U;
    if (situation->kind == CC_SITUATION_RELIEF_DELIVERY) {
        return sim->settlement_count > 0 ? sim->settlements[0].id : 0U;
    }
    if (situation->kind == CC_SITUATION_ROUTE_REPAIR) {
        const CcRoute *route = CcSimRoute(sim, situation->target_id);
        return route != NULL ? route->from_id : 0U;
    }
    if (situation->kind == CC_SITUATION_MONSTER_EXPEDITION) {
        for (int32_t i = 0; i < sim->dungeon_count; ++i) {
            if (sim->dungeons[i].id == situation->target_id) {
                return sim->dungeons[i].settlement_id;
            }
        }
        return 0U;
    }
    if (situation->kind == CC_SITUATION_BLACK_MARKET_DELIVERY) {
        for (int32_t i = 0; i < sim->route_count; ++i) {
            const CcRoute *route = &sim->routes[i];
            if (route->smuggler_route && route->to_id == situation->target_id) {
                return route->from_id;
            }
        }
    }
    if (situation->kind == CC_SITUATION_COURIER_DELIVERY) {
        const CcCourier *courier = CourierMutable(
            (CcSim *)sim, situation->target_id);
        if (courier != NULL &&
            (courier->status == CC_COURIER_WAITING ||
             courier->status == CC_COURIER_WITH_PLAYER)) {
            return courier->current_settlement_id;
        }
    }
    return 0U;
}

static const CcEvent *LatestEvent(const CcSim *sim, CcEventKind kind,
                                  CcId subject, CcId location)
{
    if (sim == NULL) return NULL;
    for (int32_t offset = 0; offset < sim->event_count; ++offset) {
        const CcEvent *event = CcSimRecentEvent(sim, offset);
        if (event == NULL || event->kind != kind) continue;
        if (subject != 0U && event->subject_id != subject) continue;
        if (location != 0U && event->location_id != location) continue;
        return event;
    }
    return NULL;
}

static CcId LatestLocalCause(const CcSim *sim, CcId location)
{
    if (sim == NULL) return 0U;
    for (int32_t offset = 0; offset < sim->event_count; ++offset) {
        const CcEvent *event = CcSimRecentEvent(sim, offset);
        if (event == NULL || event->location_id != location) continue;
        if (event->kind == CC_EVENT_PLAYER_TRADE ||
            event->kind == CC_EVENT_PLAYER_TRAVEL ||
            event->kind == CC_EVENT_SITUATION_CREATED) continue;
        return event->id;
    }
    return 0U;
}

int32_t CcSimActiveSituationCount(const CcSim *sim)
{
    if (sim == NULL) return 0;
    int32_t count = 0;
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        if (sim->situations[i].status == CC_SITUATION_ACTIVE) count += 1;
    }
    return count;
}

bool CcSimSituationTouchesSettlement(const CcSim *sim,
                                     const CcSituation *situation,
                                     CcId settlement_id)
{
    if (sim == NULL || situation == NULL || settlement_id == 0U) return false;
    if (situation->kind == CC_SITUATION_RELIEF_DELIVERY ||
        situation->kind == CC_SITUATION_BLACK_MARKET_DELIVERY) {
        return situation->target_id == settlement_id;
    }
    if (situation->kind == CC_SITUATION_ROUTE_REPAIR) {
        const CcRoute *route = CcSimRoute(sim, situation->target_id);
        return route != NULL &&
               (route->from_id == settlement_id || route->to_id == settlement_id);
    }
    if (situation->kind == CC_SITUATION_COURIER_DELIVERY) {
        const CcCourier *courier = CourierMutable(
            (CcSim *)sim, situation->target_id);
        return courier != NULL &&
            (courier->current_settlement_id == settlement_id ||
             courier->destination_settlement_id == settlement_id);
    }
    if (situation->kind == CC_SITUATION_MONSTER_EXPEDITION) {
        for (int32_t i = 0; i < sim->dungeon_count; ++i) {
            if (sim->dungeons[i].id == situation->target_id) {
                return sim->dungeons[i].settlement_id == settlement_id;
            }
        }
    }
    return false;
}

const CcSituation *CcSimSituationForSettlement(const CcSim *sim,
                                                CcId settlement_id)
{
    if (sim == NULL) return NULL;
    const CcSituation *best = NULL;
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        const CcSituation *situation = &sim->situations[i];
        if (situation->status != CC_SITUATION_ACTIVE ||
            !CcSimSituationTouchesSettlement(sim, situation,
                                             settlement_id)) continue;
        if (best == NULL || situation->deadline_day < best->deadline_day) best = situation;
    }
    return best;
}

int32_t CcSimIncomingGood(const CcSim *sim, CcId settlement_id, CcGood good)
{
    if (sim == NULL || good < 0 || good >= CC_GOOD_COUNT) return 0;
    int64_t incoming = 0;
    for (int32_t i = 0; i < sim->shipment_count; ++i) {
        const CcShipment *shipment = &sim->shipments[i];
        if (shipment->status == CC_SHIPMENT_TRAVELLING &&
            shipment->final_destination_id == settlement_id && shipment->good == good) {
            incoming += shipment->quantity;
        }
    }
    return incoming > CC_SIM_MAX_UNITS ? CC_SIM_MAX_UNITS :
           incoming < 0 ? 0 : (int32_t)incoming;
}

int32_t CcPlayerCargoUsed(const CcPlayerCompany *player)
{
    if (player == NULL) return 0;
    static const int32_t per_slot[CC_GOOD_COUNT] = {8, 4, 2, 2, 1, 1};
    int32_t used = player->treasure_cargo_slots;
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        int32_t quantity = player->cargo[good];
        used += quantity > 0 ?
            (quantity + per_slot[good] - 1) / per_slot[good] : 0;
    }
    return used;
}

static int32_t GoodCargoSlots(CcGood good, int32_t quantity)
{
    static const int32_t per_slot[CC_GOOD_COUNT] = {8, 4, 2, 2, 1, 1};
    if (good < 0 || good >= CC_GOOD_COUNT || quantity <= 0) return 0;
    return (quantity + per_slot[good] - 1) / per_slot[good];
}

static int32_t GoodUnitsPerCargoSlot(CcGood good)
{
    static const int32_t per_slot[CC_GOOD_COUNT] = {8, 4, 2, 2, 1, 1};
    return good >= 0 && good < CC_GOOD_COUNT ? per_slot[good] : 1;
}

const CcTreasure *CcSimTreasure(const CcSim *sim, CcId id)
{
    if (sim == NULL || CcIdKind(id) != CC_ENTITY_TREASURE) return NULL;
    for (int32_t i = 0; i < sim->treasure_count; ++i) {
        if (sim->treasures[i].id == id && !sim->treasures[i].destroyed) {
            return &sim->treasures[i];
        }
    }
    return NULL;
}

int32_t CcSimTreasureCountForOwner(const CcSim *sim, CcId owner_id)
{
    if (sim == NULL || owner_id == 0U) return 0;
    int32_t count = 0;
    for (int32_t i = 0; i < sim->treasure_count; ++i) {
        if (!sim->treasures[i].destroyed &&
            sim->treasures[i].owner_id == owner_id) count += 1;
    }
    return count;
}

int32_t CcPlayerMapCount(const CcSim *sim)
{
    if (sim == NULL) return 0;
    int32_t count = 0;
    for (int32_t i = 0; i < sim->map_count; ++i) {
        const CcMap *map = &sim->maps[i];
        if (map->owner_id == sim->player.id &&
            CcSimMapIsCatalogued(sim, map) &&
            !CcSimMapIsArchived(sim, map)) count += 1;
    }
    return count;
}

int32_t CcPlayerMapCollectionCount(const CcSim *sim)
{
    if (sim == NULL) return 0;
    uint32_t mask = sim->player.map_catalogue_mask;
    int32_t count = 0;
    for (int32_t i = 0; i < CC_MAP_COLLECTION_COUNT; ++i) {
        if ((mask & MapSlotBit(i)) != 0U) count += 1;
    }
    return count;
}

static void InitKingdom(CcSim *sim, int32_t slot, const char *name,
                        uint8_t red, uint8_t green, uint8_t blue,
                        CcMoney treasury, int32_t legitimacy)
{
    CcKingdom *kingdom = &sim->kingdoms[slot];
    kingdom->id = NextId(sim, CC_ENTITY_KINGDOM);
    CopyName(kingdom->name, name);
    kingdom->color_r = red;
    kingdom->color_g = green;
    kingdom->color_b = blue;
    kingdom->treasury = treasury;
    kingdom->legitimacy = legitimacy;
}

static void InitSettlement(CcSim *sim, int32_t slot, int32_t kingdom_slot,
                           const char *name, CcSettlementFunction function,
                           CcSettlementSize size,
                           int32_t x, int32_t y, int32_t population,
                           int32_t security, int32_t prosperity)
{
    CcSettlement *settlement = &sim->settlements[slot];
    settlement->id = NextId(sim, CC_ENTITY_SETTLEMENT);
    settlement->kingdom_id = sim->kingdoms[kingdom_slot].id;
    CopyName(settlement->name, name);
    settlement->function = function;
    settlement->size = size;
    settlement->service_project = CC_SERVICE_NONE;
    settlement->map_x = x;
    settlement->map_y = y;
    settlement->population = population;
    settlement->security = security;
    settlement->prosperity = prosperity;
    settlement->hunger = 0;
    settlement->market_coins = 60 + population / 20 + prosperity * 2;
    settlement->war_chest = 0;
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        settlement->price[good] = BasePrice((CcGood)good);
    }
}

static void SeedSettlementServices(CcSettlement *settlement)
{
    if (settlement == NULL) return;
    uint32_t mask = ServiceBit(CC_SERVICE_INN);
    switch (settlement->function) {
        case CC_SETTLEMENT_FARMING:
            mask |= ServiceBit(CC_SERVICE_FARM) |
                    ServiceBit(CC_SERVICE_GRANARY) |
                    ServiceBit(CC_SERVICE_STABLE);
            break;
        case CC_SETTLEMENT_MARKET:
            mask |= ServiceBit(CC_SERVICE_MARKET) |
                    ServiceBit(CC_SERVICE_SMITHY) |
                    ServiceBit(CC_SERVICE_STABLE) |
                    ServiceBit(CC_SERVICE_CARTOGRAPHER);
            break;
        case CC_SETTLEMENT_FORTRESS:
            mask |= ServiceBit(CC_SERVICE_BARRACKS) |
                    ServiceBit(CC_SERVICE_SMITHY) |
                    ServiceBit(CC_SERVICE_HEALER) |
                    ServiceBit(CC_SERVICE_GRANARY) |
                    ServiceBit(CC_SERVICE_FARM);
            break;
        case CC_SETTLEMENT_MINING:
            mask |= ServiceBit(CC_SERVICE_MINE) |
                    ServiceBit(CC_SERVICE_SMITHY) |
                    ServiceBit(CC_SERVICE_MARKET) |
                    ServiceBit(CC_SERVICE_SHRINE) |
                    ServiceBit(CC_SERVICE_FARM);
            break;
        case CC_SETTLEMENT_CAPITAL:
            mask |= ServiceBit(CC_SERVICE_MARKET) |
                    ServiceBit(CC_SERVICE_SMITHY) |
                    ServiceBit(CC_SERVICE_HEALER) |
                    ServiceBit(CC_SERVICE_STABLE) |
                    ServiceBit(CC_SERVICE_SHRINE) |
                    ServiceBit(CC_SERVICE_BARRACKS) |
                    ServiceBit(CC_SERVICE_CARTOGRAPHER) |
                    ServiceBit(CC_SERVICE_GUILDHALL) |
                    ServiceBit(CC_SERVICE_FARM);
            break;
        case CC_SETTLEMENT_DUNGEON_TOWN:
            mask |= ServiceBit(CC_SERVICE_HEALER) |
                    ServiceBit(CC_SERVICE_BLACK_MARKET) |
                    ServiceBit(CC_SERVICE_DUNGEON_WARD);
            break;
    }
    settlement->service_mask = mask;
}

static void InitRoute(CcSim *sim, int32_t slot, int32_t from, int32_t to,
                      int32_t days, int32_t capacity, int32_t security,
                      bool closed, bool smuggler)
{
    CcRoute *route = &sim->routes[slot];
    route->id = NextId(sim, CC_ENTITY_ROUTE);
    route->from_id = sim->settlements[from].id;
    route->to_id = sim->settlements[to].id;
    route->travel_days = days;
    route->capacity = capacity;
    route->security = security;
    route->condition = closed ? 38 : 76;
    route->closed = closed;
    route->smuggler_route = smuggler;
}

typedef struct CcCollectibleMapSeed {
    const char *name;
    int32_t route_slot;
    int32_t maker_slot;
    int32_t owner_slot;
    int32_t age;
    int32_t accuracy;
    int32_t condition_offset;
    int32_t danger_offset;
    int32_t ask_price;
    bool contraband;
} CcCollectibleMapSeed;

static const CcCollectibleMapSeed COLLECTIBLE_MAPS[CC_MAP_COLLECTION_COUNT] = {
    {"Thornford Fordings", 0, 0, -1, 8, 78, -3, -8, 18, false},
    {CC_GLOAMGATE_ALDERWATCH_MAP_NAME, 1, 1, 1, 0, 72, 0, 0, 24, false},
    {"Silverwick Mine Roads", 2, 3, 2, 17, 83, -7, 5, 28, false},
    {"The Rosespire Pilgrim Way", 3, 4, 4, 36, 68, -2, -12, 26, false},
    {"Tribute Roads of Varkesh", 5, 5, 5, 60, 47, -15, 24, 44, true},
    {"The Broken March", 4, 4, 4, 95, 41, -21, 17, 36, false},
    {"The Gloamgate Night Road", 6, 1, 1, 4, 56, -9, 19, 31, true},
    {"Alderwatch Muster Map", 7, 2, 2, 12, 90, 4, -3, 42, true},
    {"Treaty Bridge Survey", 1, 2, 2, 1, 94, 8, -4, 38, false},
    {"The Lower Silverworks", 2, 3, 3, 120, 63, -18, 28, 34, true},
    {"The Ash-Poor's Skin Map", 6, 1, 1, 6, 38, -12, 31, 48, true},
    {CC_CROWNLESS_ATLAS_MAP_NAME, 5, 1, -1, 0, 88, 0, 0, 90, false},
    {CC_DRAGON_HOARD_MAP_NAME, 5, 5, 1, 146, 36, -18, 39, 72, true}
};

void CcSimUpgradeMapCollection(CcSim *sim)
{
    if (sim == NULL || sim->route_count < CC_MAX_ROUTES ||
        sim->settlement_count < CC_MAX_SETTLEMENTS) return;
    int32_t old_count = sim->map_count;
    for (int32_t i = 0; i < CC_MAP_COLLECTION_COUNT; ++i) {
        const CcCollectibleMapSeed *seed = &COLLECTIBLE_MAPS[i];
        const CcRoute *route = &sim->routes[seed->route_slot];
        CcMap *map = &sim->maps[i];
        bool existing = i < old_count &&
                        CcIdKind(map->id) == CC_ENTITY_MAP;
        if (!existing) {
            *map = (CcMap){0};
            map->id = NextId(sim, CC_ENTITY_MAP);
            map->owner_id = seed->owner_slot < 0 ? sim->player.id :
                sim->settlements[seed->owner_slot].id;
            map->surveyed_day = sim->current_day - seed->age;
            map->accuracy = seed->accuracy;
            map->recorded_condition = ClampI32(
                route->condition + seed->condition_offset, 0, 100);
            map->recorded_danger = ClampI32(
                CcSimRouteDanger(sim, route->id) + seed->danger_offset,
                0, 100);
            map->ask_price = seed->ask_price;
            map->contraband = seed->contraband;
        }
        map->route_id = route->id;
        map->maker_settlement_id = sim->settlements[seed->maker_slot].id;
        (void)snprintf(map->name, sizeof(map->name), "%s", seed->name);
    }
    sim->map_count = CC_MAP_COLLECTION_COUNT;
    if (sim->player.map_catalogue_mask == 0U) {
        for (int32_t i = 0; i < sim->map_count; ++i) {
            if (i != CC_MAP_CROWNLESS_ATLAS &&
                sim->maps[i].owner_id == sim->player.id) {
                sim->player.map_catalogue_mask |= MapSlotBit(i);
            }
        }
    }
    sim->player.map_archive_mask &= sim->player.map_catalogue_mask;
}

static void InitMaps(CcSim *sim)
{
    /* Keep the world generator's later random sequence stable. The original
       eight route sheets consumed these draws before collectible maps gained
       authored properties. Save upgrades deliberately do not consume them. */
    for (int32_t i = 0; i < CC_MAX_ROUTES; ++i) {
        (void)NextRandom(sim);
        if (i != 0) (void)NextRandom(sim);
        (void)NextRandom(sim);
        (void)NextRandom(sim);
    }
    sim->map_count = 0;
    CcSimUpgradeMapCollection(sim);
}

static void InitFaction(CcSim *sim, int32_t slot, int32_t kingdom_slot,
                        CcFactionKind kind, const char *name,
                        int32_t power, int32_t support)
{
    CcFaction *faction = &sim->factions[slot];
    faction->id = NextId(sim, CC_ENTITY_FACTION);
    faction->kingdom_id = sim->kingdoms[kingdom_slot].id;
    faction->kind = kind;
    CopyName(faction->name, name);
    faction->power = power;
    faction->support = support;
}

static void ConfigureSettlementEconomies(CcSim *sim)
{
    CcSettlement *farm = &sim->settlements[0];
    farm->stock[CC_GOOD_FOOD] = 74;
    farm->stock[CC_GOOD_MATERIAL] = 12;
    farm->stock[CC_GOOD_TOOLS] = 8;
    farm->reserve_target[CC_GOOD_FOOD] = 105;
    farm->reserve_target[CC_GOOD_MATERIAL] = 2;
    farm->reserve_target[CC_GOOD_TOOLS] = 6;
    farm->production[CC_GOOD_FOOD] = 35;
    farm->consumption[CC_GOOD_FOOD] = 5;
    farm->field_yield = 100;

    CcSettlement *market = &sim->settlements[1];
    market->stock[CC_GOOD_FOOD] = 35;
    market->stock[CC_GOOD_MATERIAL] = 32;
    market->stock[CC_GOOD_TOOLS] = 23;
    market->reserve_target[CC_GOOD_FOOD] = 75;
    market->reserve_target[CC_GOOD_MATERIAL] = 16;
    market->reserve_target[CC_GOOD_TOOLS] = 12;
    market->production[CC_GOOD_TOOLS] = 4;
    market->production[CC_GOOD_WEAPONS] = 2;
    market->consumption[CC_GOOD_FOOD] = 7;
    market->consumption[CC_GOOD_MATERIAL] = 3;

    CcSettlement *fortress = &sim->settlements[2];
    fortress->stock[CC_GOOD_FOOD] = 42;
    fortress->stock[CC_GOOD_MATERIAL] = 18;
    fortress->stock[CC_GOOD_TOOLS] = 15;
    fortress->reserve_target[CC_GOOD_FOOD] = 70;
    fortress->reserve_target[CC_GOOD_MATERIAL] = 8;
    fortress->reserve_target[CC_GOOD_TOOLS] = 8;
    fortress->production[CC_GOOD_FOOD] = 24;
    fortress->field_yield = 85;
    fortress->stock[CC_GOOD_WEAPONS] = 8;
    fortress->reserve_target[CC_GOOD_WEAPONS] = 14;
    fortress->production[CC_GOOD_WEAPONS] = 2;
    fortress->consumption[CC_GOOD_FOOD] = 6;
    fortress->consumption[CC_GOOD_TOOLS] = 1;

    CcSettlement *mine = &sim->settlements[3];
    mine->stock[CC_GOOD_FOOD] = 21;
    mine->stock[CC_GOOD_MATERIAL] = 76;
    mine->stock[CC_GOOD_TOOLS] = 13;
    mine->reserve_target[CC_GOOD_FOOD] = 78;
    mine->reserve_target[CC_GOOD_MATERIAL] = 12;
    mine->reserve_target[CC_GOOD_TOOLS] = 8;
    mine->production[CC_GOOD_MATERIAL] = 10;
    mine->production[CC_GOOD_FOOD] = 16;
    mine->field_yield = 70;
    mine->iron_deposit = 12000;
    mine->gold_seam = true;
    mine->gem_seam = true;
    mine->consumption[CC_GOOD_FOOD] = 7;
    mine->consumption[CC_GOOD_TOOLS] = 2;

    CcSettlement *capital = &sim->settlements[4];
    capital->stock[CC_GOOD_FOOD] = 61;
    capital->stock[CC_GOOD_MATERIAL] = 36;
    capital->stock[CC_GOOD_TOOLS] = 31;
    capital->reserve_target[CC_GOOD_FOOD] = 92;
    capital->reserve_target[CC_GOOD_MATERIAL] = 16;
    capital->reserve_target[CC_GOOD_TOOLS] = 14;
    capital->production[CC_GOOD_FOOD] = 28;
    capital->field_yield = 90;
    capital->production[CC_GOOD_TOOLS] = 5;
    capital->production[CC_GOOD_WEAPONS] = 2;
    capital->stock[CC_GOOD_GOLD] = 1;
    capital->stock[CC_GOOD_GEMS] = 1;
    capital->consumption[CC_GOOD_FOOD] = 8;
    capital->consumption[CC_GOOD_MATERIAL] = 3;

    CcSettlement *frontier = &sim->settlements[5];
    frontier->stock[CC_GOOD_FOOD] = 39;
    frontier->stock[CC_GOOD_MATERIAL] = 48;
    frontier->stock[CC_GOOD_TOOLS] = 17;
    frontier->reserve_target[CC_GOOD_FOOD] = 62;
    frontier->reserve_target[CC_GOOD_MATERIAL] = 10;
    frontier->reserve_target[CC_GOOD_TOOLS] = 6;
    frontier->production[CC_GOOD_FOOD] = 3;
    frontier->production[CC_GOOD_MATERIAL] = 4;
    frontier->iron_deposit = 5200;
    frontier->gold_seam = true;
    frontier->consumption[CC_GOOD_FOOD] = 5;
    frontier->consumption[CC_GOOD_TOOLS] = 1;

    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        CcSettlement *settlement = &sim->settlements[i];
        if (settlement->field_yield == 0 &&
            (settlement->function == CC_SETTLEMENT_FARMING ||
             settlement->function == CC_SETTLEMENT_FORTRESS ||
             settlement->function == CC_SETTLEMENT_CAPITAL)) {
            settlement->field_yield = settlement->function ==
                CC_SETTLEMENT_FARMING ? 100 : 72;
        }
        settlement->reserve_target[CC_GOOD_WEAPONS] = MaximumI32(
            settlement->reserve_target[CC_GOOD_WEAPONS],
            settlement->function == CC_SETTLEMENT_FORTRESS ? 14 : 4);
        settlement->reserve_target[CC_GOOD_GOLD] = 1;
        settlement->reserve_target[CC_GOOD_GEMS] = 1;
        settlement->consumption[CC_GOOD_IRON] = 0;
        settlement->consumption[CC_GOOD_TOOLS] = 0;
        settlement->consumption[CC_GOOD_WEAPONS] = 0;
        settlement->consumption[CC_GOOD_GOLD] = 0;
        settlement->consumption[CC_GOOD_GEMS] = 0;
        settlement->population += (int32_t)(NextRandom(sim) % 401U) - 200;
        settlement->security = ClampI32(settlement->security +
            (int32_t)(NextRandom(sim) % 15U) - 7, 20, 92);
        settlement->prosperity = ClampI32(settlement->prosperity +
            (int32_t)(NextRandom(sim) % 15U) - 7, 20, 88);
        int32_t settlement_good_count =
            sim->schema_version >= 9U ? CC_GOOD_COUNT : 3;
        for (int32_t good = 0; good < settlement_good_count; ++good) {
            int32_t floor = good <= CC_GOOD_TOOLS ? 1 : 0;
            settlement->stock[good] = MaximumI32(floor, settlement->stock[good] +
                (int32_t)(NextRandom(sim) % (good >= CC_GOOD_GOLD ? 3U : 13U)) -
                (good >= CC_GOOD_GOLD ? 1 : 6));
            settlement->production[good] = MaximumI32(0, settlement->production[good] +
                (settlement->production[good] > 0 ? (int32_t)(NextRandom(sim) % 3U) - 1 : 0));
        }
    }
}

void CcSimInit(CcSim *sim, uint32_t seed)
{
    if (sim == NULL) return;
    *sim = (CcSim){0};
    sim->schema_version = CC_SIM_SCHEMA_VERSION;
    sim->generator_version = CC_GENERATOR_VERSION;
    sim->world_seed = seed == 0U ? UINT32_C(0xc0a71a9e) : seed;
    sim->random_state = sim->world_seed;
    sim->current_day = 1;
    sim->next_entity_serial = 1U;

    static const char *kingdom_sets[][3] = {
        {"Alder Concord", "Ember Crown", "Star Oath"},
        {"Willow Republic", "Ashen Throne", "Indigo March"},
        {"Moss Compact", "Rose Dominion", "Silver Covenant"}
    };
    uint32_t name_set = NextRandom(sim) % 3U;
    InitKingdom(sim, 0, kingdom_sets[name_set][0], 54U, 173U, 146U,
                470 + (CcMoney)(NextRandom(sim) % 151U),
                55 + (int32_t)(NextRandom(sim) % 17U));
    InitKingdom(sim, 1, kingdom_sets[name_set][1], 210U, 101U, 71U,
                470 + (CcMoney)(NextRandom(sim) % 151U),
                48 + (int32_t)(NextRandom(sim) % 17U));
    InitKingdom(sim, 2, kingdom_sets[name_set][2], 102U, 123U, 205U,
                470 + (CcMoney)(NextRandom(sim) % 151U),
                58 + (int32_t)(NextRandom(sim) % 17U));
    sim->kingdom_count = CC_MAX_KINGDOMS;
    for (int32_t first = 0; first < sim->kingdom_count; ++first) {
        for (int32_t second = 0; second < sim->kingdom_count; ++second) {
            sim->diplomacy[first][second] = CC_DIPLOMACY_PEACE;
            sim->diplomacy_changed_day[first][second] = sim->current_day;
        }
    }
    sim->diplomacy[0][1] = CC_DIPLOMACY_WAR;
    sim->diplomacy[1][0] = CC_DIPLOMACY_WAR;
    for (int32_t i = 0; i < sim->kingdom_count; ++i) {
        CcMoney monastery_deposit = MinimumI32(
            160, sim->kingdoms[i].treasury > INT32_MAX ? INT32_MAX :
                 (int32_t)sim->kingdoms[i].treasury);
        sim->kingdoms[i].treasury -= monastery_deposit;
        sim->iron_ledger_reserve += monastery_deposit;
    }

    char place_names[CC_MAX_SETTLEMENTS][CC_NAME_CAPACITY];
    for (int32_t i = 0; i < CC_MAX_SETTLEMENTS; ++i) {
        GeneratePlaceName(sim, place_names[i], (CcSettlementFunction)i);
    }
    InitSettlement(sim, 0, 0, "Thornford", CC_SETTLEMENT_FARMING,
                   CC_SETTLEMENT_VILLAGE,
                   Jitter(sim, 125, 22), Jitter(sim, 500, 20), 1460, 58, 54);
    InitSettlement(sim, 1, 0, "Gloamgate", CC_SETTLEMENT_MARKET,
                   CC_SETTLEMENT_TOWN,
                   Jitter(sim, 355, 22), Jitter(sim, 445, 20), 2180, 62, 67);
    InitSettlement(sim, 2, 1, "Alderwatch", CC_SETTLEMENT_FORTRESS,
                   CC_SETTLEMENT_TOWN,
                   Jitter(sim, 535, 18), Jitter(sim, 325, 18), 1720, 82, 49);
    InitSettlement(sim, 3, 1, "Silverwick", CC_SETTLEMENT_MINING,
                   CC_SETTLEMENT_TOWN,
                   Jitter(sim, 755, 22), Jitter(sim, 455, 20), 2350, 43, 61);
    InitSettlement(sim, 4, 2, "Rosespire", CC_SETTLEMENT_CAPITAL,
                   CC_SETTLEMENT_CAPITAL_SIZE,
                   Jitter(sim, 770, 18), Jitter(sim, 145, 18), 3180, 71, 72);
    InitSettlement(sim, 5, 2, "Hollowbarrow", CC_SETTLEMENT_DUNGEON_TOWN,
                   CC_SETTLEMENT_VILLAGE,
                   Jitter(sim, 335, 22), Jitter(sim, 155, 20), 1280, 46, 45);
    sim->settlement_count = CC_MAX_SETTLEMENTS;
    ConfigureSettlementEconomies(sim);
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        SeedSettlementServices(&sim->settlements[i]);
    }

    InitRoute(sim, 0, 0, 1, 2, 40, 64, false, false);
    InitRoute(sim, 1, 1, 2, 2, 34, 78, true, false);
    InitRoute(sim, 2, 2, 3, 2, 30, 34, false, false);
    InitRoute(sim, 3, 3, 4, 3, 28, 62, false, false);
    InitRoute(sim, 4, 4, 5, 3, 24, 45, false, false);
    InitRoute(sim, 5, 5, 1, 3, 24, 49, false, false);
    InitRoute(sim, 6, 1, 3, 4, 7, 20, false, true);
    InitRoute(sim, 7, 2, 4, 3, 26, 57, false, false);
    sim->route_count = CC_MAX_ROUTES;

    for (int32_t kingdom = 0; kingdom < CC_MAX_KINGDOMS; ++kingdom) {
        for (int32_t local = 0; local < 3; ++local) {
            int32_t slot = kingdom * 3 + local;
            char faction_name[CC_NAME_CAPACITY];
            const char *suffix = local == 0 ? " Court" :
                                 local == 1 ? " Factors" : " Commons";
            (void)snprintf(faction_name, sizeof(faction_name), "%.21s%s",
                           sim->kingdoms[kingdom].name, suffix);
            InitFaction(sim, slot, kingdom, (CcFactionKind)local,
                        faction_name,
                        54 + (int32_t)(NextRandom(sim) % 25U),
                        42 + (int32_t)(NextRandom(sim) % 31U));
        }
    }
    sim->faction_count = CC_MAX_FACTIONS;

    sim->bandits[0].id = NextId(sim, CC_ENTITY_BANDIT_GROUP);
    sim->bandits[0].route_id = sim->routes[6].id;
    static const char *bandit_names[] = {
        "The Unpaid Company", "The Tallow Knives", "The Broken Pennants",
        "The Ditch Parliament"
    };
    CopyName(sim->bandits[0].name, bandit_names[NextRandom(sim) % 4U]);
    sim->bandits[0].members = 28 + (int32_t)(NextRandom(sim) % 15U);
    sim->bandits[0].supplies = 20 + (int32_t)(NextRandom(sim) % 15U);
    sim->bandits[0].influence = 48 + (int32_t)(NextRandom(sim) % 16U);
    sim->bandits[0].camp_size = CC_BANDIT_CAMP;
    sim->bandits[0].service_mask = ServiceBit(CC_SERVICE_BLACK_MARKET) |
                                   ServiceBit(CC_SERVICE_STABLE) |
                                   ServiceBit(CC_SERVICE_SMITHY);
    sim->bandits[0].raid_phase = CC_BANDIT_RAID_IDLE;
    sim->bandits[0].raid_good = CC_GOOD_FOOD;
    sim->bandit_count = 1;

    sim->dungeons[0].id = NextId(sim, CC_ENTITY_DUNGEON);
    sim->dungeons[0].settlement_id = sim->settlements[3].id;
    static const char *dungeon_names[] = {
        "The Lower Silverworks", "The Drowned Mint", "The Blackglass Galleries",
        "The Saintless Vault"
    };
    CopyName(sim->dungeons[0].name, dungeon_names[NextRandom(sim) % 4U]);
    sim->dungeons[0].state = CC_DUNGEON_DISTURBED;
    sim->dungeons[0].depth = 4;
    sim->dungeons[0].regional_pressure = 48;
    sim->dungeon_count = 1;

    sim->monsters[0].id = NextId(sim, CC_ENTITY_MONSTER_POPULATION);
    sim->monsters[0].dungeon_id = sim->dungeons[0].id;
    static const char *monster_names[] = {
        "Glassback Burrowers", "Ash-Maw Choir", "Pale Ore-Eaters",
        "Crownless Centipedes"
    };
    CopyName(sim->monsters[0].name, monster_names[NextRandom(sim) % 4U]);
    sim->monsters[0].population = 38 + (int32_t)(NextRandom(sim) % 17U);
    sim->monsters[0].pressure = 38 + (int32_t)(NextRandom(sim) % 15U);
    sim->monsters[0].hunting_pressure = 0;
    sim->monster_count = 1;

    sim->player.id = NextId(sim, CC_ENTITY_PLAYER_COMPANY);
    sim->player.location_id = sim->settlements[0].id;
    sim->player.coins = 42;
    sim->player.cargo_capacity = CC_CARGO_CAPACITY;
    sim->player.passenger_capacity = 4;
    sim->player.map_capacity = CC_MAP_CAPACITY;
    sim->player.reputation = 0;
    sim->clock.game_minutes_per_second =
        CC_IDLE_GAME_MINUTES_PER_SECOND;
    sim->carriage = (CcCarriageState){
        .mode = CC_CARRIAGE_PARKED,
        .location_id = sim->player.location_id,
        .condition = 100
    };
    InitMaps(sim);
    CcSimInitializeDragonCycle(sim);
    CcSimInitializeHoardRaiders(sim);

    char text[CC_EVENT_TEXT_CAPACITY];
    int32_t drought = 55 + (int32_t)(NextRandom(sim) % 26U);
    (void)snprintf(text, sizeof(text),
                   "%s's drought harvest cannot supply the eastern settlements.",
                   sim->settlements[0].name);
    CcEvent *harvest = PushEvent(sim, CC_EVENT_HARVEST_FAILED,
                                 sim->settlements[0].id,
                                 sim->settlements[0].id, 0, drought, text);
    (void)snprintf(text, sizeof(text),
                   "%s closes the treaty bridge and delays the relief convoy.",
                   sim->settlements[2].name);
    CcEvent *bridge = PushEvent(sim, CC_EVENT_ROUTE_CLOSED, sim->routes[1].id,
                                sim->settlements[2].id, harvest->id, 54, text);
    (void)snprintf(text, sizeof(text),
                   "Displaced workers reinforce %s on the old road.",
                   sim->bandits[0].name);
    (void)PushEvent(sim, CC_EVENT_BANDIT_PRESSURE, sim->bandits[0].id,
                    sim->routes[6].id, bridge->id,
                    sim->bandits[0].influence, text);
    (void)snprintf(text, sizeof(text),
                   "%s opens a sealed tunnel and disturbs the %s.",
                   sim->settlements[3].name, sim->monsters[0].name);
    (void)PushEvent(sim, CC_EVENT_MONSTER_PRESSURE, sim->monsters[0].id,
                    sim->dungeons[0].id, bridge->id,
                    sim->monsters[0].pressure, text);
    GenerateSituations(sim);
    CcSimInitializeAnimalEconomy(sim);
}

static int32_t BasePrice(CcGood good)
{
    switch (good) {
        case CC_GOOD_FOOD: return 4;
        case CC_GOOD_IRON: return 8;
        case CC_GOOD_TOOLS: return 14;
        case CC_GOOD_WEAPONS: return 24;
        case CC_GOOD_GOLD: return 40;
        case CC_GOOD_GEMS: return 70;
        case CC_GOOD_COUNT: break;
    }
    return 1;
}

static CcDungeon *DungeonByIdMutable(CcSim *sim, CcId id)
{
    for (int32_t i = 0; i < sim->dungeon_count; ++i) {
        if (sim->dungeons[i].id == id) return &sim->dungeons[i];
    }
    return NULL;
}

static int32_t MonsterPressureAtSettlement(const CcSim *sim, CcId settlement_id)
{
    int32_t pressure = 0;
    for (int32_t i = 0; i < sim->dungeon_count; ++i) {
        if (sim->dungeons[i].settlement_id != settlement_id) continue;
        for (int32_t monster = 0; monster < sim->monster_count; ++monster) {
            if (sim->monsters[monster].dungeon_id == sim->dungeons[i].id) {
                pressure = MaximumI32(pressure, sim->monsters[monster].pressure);
            }
        }
    }
    return pressure;
}

static int32_t FoodSeasonFactor(const CcSim *sim)
{
    int32_t week = (sim->current_day / 7) % 52;
    if (week < 13) return 72;
    if (week < 26) return 112;
    if (week < 39) return 148;
    return 58;
}

static int32_t EffectiveProduction(CcSim *sim, CcSettlement *settlement,
                                   int32_t index, CcGood good)
{
    if (CcSettlementIsAbandoned(settlement)) return 0;
    int32_t production = settlement->production[good];
    bool subsistence_muster = good == CC_GOOD_FOOD &&
        (settlement->hunger > 65 ||
         (settlement->population < 600 && settlement->hunger >= 20));
    int32_t subsistence_food = subsistence_muster ?
        MaximumI32(1, CivilianFoodUse(settlement) * 2 / 3) : 0;
    if (production <= 0 && subsistence_food <= 0) return 0;
    if (settlement->hunger > 65) production = production * 72 / 100;
    else if (settlement->hunger > 35) production = production * 86 / 100;

    if (good == CC_GOOD_FOOD) {
        if (!CcSettlementHasService(settlement, CC_SERVICE_FARM) ||
            settlement->field_yield <= 0) return subsistence_food;
        production = production * FoodSeasonFactor(sim) / 100;
        production = production * settlement->field_yield / 100;
        production = production * CcSimClimateFactor(sim) / 100;
        int32_t labor_factor = ClampI32(
            35 + settlement->population / 20, 35, 100);
        production = production * labor_factor / 100;
        if (index == 0 && sim->current_day < 112) production = production * 64 / 100;
        if (settlement->stock[CC_GOOD_TOOLS] <= 0) {
            production = production * 50 / 100;
        }
        production = MaximumI32(production, subsistence_food);
    }
    if (good == CC_GOOD_IRON) {
        if (!CcSettlementHasService(settlement, CC_SERVICE_MINE) ||
            settlement->iron_deposit <= 0) return 0;
        int32_t monster_pressure = MonsterPressureAtSettlement(sim, settlement->id);
        production = production * (100 - monster_pressure / 2) / 100;
        if (settlement->stock[CC_GOOD_TOOLS] <= 0) production = MaximumI32(1, production / 4);
        for (int32_t dungeon = 0; dungeon < sim->dungeon_count; ++dungeon) {
            if (sim->dungeons[dungeon].settlement_id == settlement->id &&
                sim->dungeons[dungeon].state == CC_DUNGEON_PUBLIC_ROUTE) {
                production = production * 125 / 100;
            }
        }
        production = MinimumI32(production, settlement->iron_deposit);
    }
    if (good >= CC_GOOD_TOOLS) return 0;
    return MaximumI32(0, production);
}

static void WearOneTool(CcSettlement *settlement, int32_t *wear,
                        int32_t batches_per_tool)
{
    if (settlement->stock[CC_GOOD_TOOLS] <= 0) return;
    *wear += 1;
    if (*wear < batches_per_tool) return;
    settlement->stock[CC_GOOD_TOOLS] -= 1;
    *wear = 0;
}

static CcTreasure *AllocateTreasure(CcSim *sim)
{
    if (sim->treasure_count >= CC_MAX_TREASURES) return NULL;
    CcTreasure *treasure = &sim->treasures[sim->treasure_count++];
    *treasure = (CcTreasure){0};
    treasure->id = NextId(sim, CC_ENTITY_TREASURE);
    return treasure;
}

static void CompleteTreasure(CcSim *sim, CcSettlement *settlement)
{
    CcTreasure *treasure = AllocateTreasure(sim);
    if (treasure == NULL) return;
    static const char *forms[] = {
        "Sun Reliquary", "Ash Crown", "Moon Cup", "Saintless Torc",
        "Blackglass Icon", "Ember Casket"
    };
    const char *form = forms[(sim->treasure_count - 1) % 6];
    char treasure_name[CC_MAP_NAME_CAPACITY];
    (void)snprintf(treasure_name, sizeof(treasure_name),
                   "%.20s %s", settlement->name, form);
    (void)snprintf(treasure->name, sizeof(treasure->name), "%s",
                   treasure_name);
    treasure->maker_settlement_id = settlement->id;
    treasure->owner_id = settlement->id;
    treasure->location_id = settlement->id;
    treasure->gold_content = settlement->treasure_gold_committed;
    treasure->gem_content = settlement->treasure_gems_committed;
    treasure->craft_work = settlement->treasure_work;
    treasure->appraised_value = treasure->gold_content * 40 +
                                treasure->gem_content * 70 +
                                treasure->craft_work * 10;
    treasure->created_day = sim->current_day;
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(text, sizeof(text),
                   "%s finishes %s from %d Raw Gold, %d Gems, and %d weeks of work.",
                   settlement->name, treasure->name, treasure->gold_content,
                   treasure->gem_content, treasure->craft_work);
    (void)PushEvent(sim, CC_EVENT_TREASURE_CRAFTED, treasure->id,
                    settlement->id, LatestLocalCause(sim, settlement->id),
                    treasure->appraised_value, text);
    settlement->treasure_gold_committed = 0;
    settlement->treasure_gems_committed = 0;
    settlement->treasure_work = 0;
}

static void RunSmithy(CcSim *sim, CcSettlement *settlement)
{
    if (!CcSettlementHasService(settlement, CC_SERVICE_SMITHY)) return;
    int32_t iron_before = settlement->stock[CC_GOOD_IRON];
    int32_t tools_made = 0;
    int32_t tool_gap = MaximumI32(0, settlement->reserve_target[CC_GOOD_TOOLS] * 2 -
                                     settlement->stock[CC_GOOD_TOOLS]);
    int32_t tool_capacity = MaximumI32(0, settlement->production[CC_GOOD_TOOLS]);
    tools_made = MinimumI32(tool_capacity,
        MinimumI32(tool_gap, settlement->stock[CC_GOOD_IRON] / 2));
    settlement->stock[CC_GOOD_IRON] -= tools_made * 2;
    settlement->stock[CC_GOOD_TOOLS] += tools_made;

    int32_t weapons_made = 0;
    int32_t weapon_gap = MaximumI32(0,
        EffectiveReserveTarget(sim, settlement, CC_GOOD_WEAPONS) * 2 -
        settlement->stock[CC_GOOD_WEAPONS]);
    int32_t weapon_capacity = MaximumI32(
        0, settlement->production[CC_GOOD_WEAPONS]);
    weapons_made = MinimumI32(weapon_capacity,
        MinimumI32(weapon_gap, settlement->stock[CC_GOOD_IRON] / 3));
    settlement->stock[CC_GOOD_IRON] -= weapons_made * 3;
    settlement->stock[CC_GOOD_WEAPONS] += weapons_made;

    int32_t smith_batches = tools_made + weapons_made;
    for (int32_t batch = 0; batch < smith_batches; ++batch) {
        WearOneTool(settlement, &settlement->smith_tool_wear, 4);
    }
    if (smith_batches > 0) {
        char text[CC_EVENT_TEXT_CAPACITY];
        (void)snprintf(text, sizeof(text),
                       "%s's smithy turns %d Iron into %d Tools and %d Weapons.",
                       settlement->name,
                       iron_before - settlement->stock[CC_GOOD_IRON],
                       tools_made, weapons_made);
        (void)PushEvent(sim, CC_EVENT_SMITH_PRODUCTION, settlement->id,
                        settlement->id, LatestLocalCause(sim, settlement->id),
                        tools_made + weapons_made, text);
    }

    bool treasure_town = settlement->function == CC_SETTLEMENT_MARKET ||
                         settlement->function == CC_SETTLEMENT_CAPITAL;
    if (!treasure_town) return;
    if (settlement->treasure_work == 0 &&
        settlement->stock[CC_GOOD_GOLD] >= 1 &&
        settlement->stock[CC_GOOD_GEMS] >= 1) {
        settlement->stock[CC_GOOD_GOLD] -= 1;
        settlement->stock[CC_GOOD_GEMS] -= 1;
        settlement->treasure_gold_committed = 1;
        settlement->treasure_gems_committed = 1;
        settlement->treasure_work = 1;
    } else if (settlement->treasure_work > 0 &&
               settlement->treasure_work < 3) {
        settlement->treasure_work += 1;
    }
    if (settlement->treasure_work >= 3 &&
        sim->treasure_count < CC_MAX_TREASURES) {
        CompleteTreasure(sim, settlement);
    }
}

static void AdvanceRareMineWork(CcSim *sim, CcSettlement *settlement,
                                int32_t iron_mined)
{
    if (iron_mined <= 0) return;
    if (settlement->gold_seam && settlement->stock[CC_GOOD_TOOLS] > 0) {
        settlement->gold_progress += 1;
        if (settlement->gold_progress >= 12) {
            settlement->gold_progress -= 12;
            settlement->stock[CC_GOOD_GOLD] += 1;
        }
    }
    if (settlement->gem_seam && settlement->stock[CC_GOOD_TOOLS] > 0) {
        settlement->gem_progress += 1;
        if (settlement->gem_progress >= 48) {
            settlement->gem_progress -= 48;
            settlement->stock[CC_GOOD_GEMS] += 1;
        }
    }
    if (sim->current_day % 28 == 0) {
        char text[CC_EVENT_TEXT_CAPACITY];
        (void)snprintf(text, sizeof(text),
                       "%s extracts %d Iron; seams stand at gold %d/12 and gems %d/48.",
                       settlement->name, iron_mined,
                       settlement->gold_progress, settlement->gem_progress);
        (void)PushEvent(sim, CC_EVENT_RESOURCE_EXTRACTED, settlement->id,
                        settlement->id, LatestLocalCause(sim, settlement->id),
                        iron_mined, text);
    }
}

static void AdvanceServiceProjects(CcSim *sim)
{
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        CcSettlement *settlement = &sim->settlements[i];
        if (settlement->service_project == CC_SERVICE_NONE) continue;
        settlement->service_project_days =
            MaximumI32(0, settlement->service_project_days - 1);
        if (settlement->service_project_days > 0) continue;
        CcServiceKind service = settlement->service_project;
        settlement->service_mask |= ServiceBit(service);
        settlement->service_project = CC_SERVICE_NONE;
        settlement->prosperity = ClampI32(settlement->prosperity + 2, 0, 100);
        char text[CC_EVENT_TEXT_CAPACITY];
        (void)snprintf(text, sizeof(text), "%s opens a new %s.",
                       settlement->name, CcServiceName(service));
        (void)PushEvent(sim, CC_EVENT_SERVICE_OPENED, settlement->id,
                        settlement->id, LatestLocalCause(sim, settlement->id),
                        (int32_t)service, text);
    }
}

static int32_t AdvanceCowHerd(CcSim *sim, CcSettlement *settlement)
{
    if (sim->schema_version < 14U) return 0;
    if (!CcSettlementHasService(settlement, CC_SERVICE_FARM)) return 0;
    int32_t herd = settlement->cow_adults + settlement->cow_calves;
    if (herd <= 0) return 0;

    int32_t feed_required = MaximumI32(1, (herd + 11) / 12);
    int32_t feed_eaten = MinimumI32(
        feed_required, settlement->stock[CC_GOOD_FOOD]);
    settlement->stock[CC_GOOD_FOOD] -= feed_eaten;
    int32_t feed_shortfall = feed_required - feed_eaten;
    if (feed_shortfall > 0) {
        settlement->cow_hunger = ClampI32(
            settlement->cow_hunger + feed_shortfall * 12, 0, 100);
        settlement->cow_condition = ClampI32(
            settlement->cow_condition - feed_shortfall * 3, 1, 100);
    } else {
        settlement->cow_hunger = ClampI32(
            settlement->cow_hunger - 10, 0, 100);
        settlement->cow_condition = ClampI32(
            settlement->cow_condition + 2, 1, 100);
    }

    if (sim->current_day % 112 == 0 &&
        settlement->cow_condition >= 65 &&
        settlement->cow_hunger <= 30) {
        int32_t matured = settlement->cow_calves > 0 ?
            MaximumI32(1, settlement->cow_calves / 4) : 0;
        matured = MinimumI32(matured, settlement->cow_calves);
        settlement->cow_calves -= matured;
        settlement->cow_adults += matured;
        int32_t capacity = MaximumI32(6, settlement->population / 50);
        int32_t room = MaximumI32(
            0, capacity - settlement->cow_adults - settlement->cow_calves);
        int32_t births = MinimumI32(
            room, MaximumI32(1, settlement->cow_adults / 14));
        settlement->cow_calves += births;
        if (births > 0) {
            char text[CC_EVENT_TEXT_CAPACITY];
            (void)snprintf(
                text, sizeof(text),
                "%s's cattle herd raises %d new %s; %d older %s join the working herd.",
                settlement->name, births, births == 1 ? "calf" : "calves",
                matured, matured == 1 ? "calf" : "calves");
            (void)PushEvent(
                sim, CC_EVENT_COW_CALVING, settlement->id, settlement->id,
                LatestLocalCause(sim, settlement->id), births, text);
        }
    }

    if (settlement->cow_hunger >= 65 && settlement->cow_adults > 0) {
        settlement->cow_adults -= 1;
        settlement->stock[CC_GOOD_FOOD] += 4;
        settlement->cow_hunger = ClampI32(
            settlement->cow_hunger - 12, 0, 100);
        char text[CC_EVENT_TEXT_CAPACITY];
        (void)snprintf(
            text, sizeof(text),
            "%s slaughters one cow after the herd's fodder runs short; 4 Food enters the local store.",
            settlement->name);
        (void)PushEvent(
            sim, CC_EVENT_COW_SLAUGHTERED, settlement->id, settlement->id,
            LatestLocalCause(sim, settlement->id), 1, text);
    }

    return MaximumI32(
        1, settlement->cow_adults * settlement->cow_condition / 1200);
}

static void UpdateSettlement(CcSim *sim, int32_t index)
{
    CcSettlement *settlement = &sim->settlements[index];
    if (CcSettlementIsAbandoned(settlement)) return;
    int32_t produced[CC_GOOD_COUNT] = {0};
    int32_t cow_food = AdvanceCowHerd(sim, settlement);
    int32_t food_required = 1;
    int32_t food_eaten = 0;
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        int32_t production = EffectiveProduction(sim, settlement, index, (CcGood)good);
        if ((CcGood)good == CC_GOOD_FOOD) production += cow_food;
        produced[good] = production;
        if ((CcGood)good == CC_GOOD_IRON) {
            settlement->iron_deposit -= production;
        }
        int32_t civilian_consumption = (CcGood)good == CC_GOOD_FOOD ?
            CivilianFoodUse(settlement) : settlement->consumption[good];
        int32_t consumption = civilian_consumption +
            WarExtraConsumption(sim, settlement, (CcGood)good);
        int64_t replenished = (int64_t)settlement->stock[good] +
                              (int64_t)production;
        settlement->stock[good] = replenished > CC_SIM_MAX_UNITS ?
                                  CC_SIM_MAX_UNITS :
                                  replenished < 0 ? 0 :
                                  (int32_t)replenished;
        int32_t consumed = MinimumI32(settlement->stock[good], consumption);
        settlement->stock[good] -= consumed;
        if ((CcGood)good == CC_GOOD_FOOD) {
            food_required = MaximumI32(1, consumption);
            food_eaten = consumed;
            (void)SpoilStoredFood(sim, settlement);
        }
        int32_t target = EffectiveReserveTarget(
            sim, settlement, (CcGood)good);
        int32_t incoming = CcSimIncomingGood(sim, settlement->id, (CcGood)good);
        int32_t expected_stock = settlement->stock[good] + incoming / 2;
        int32_t shortage = target > 0 ? (target - expected_stock) * 100 / target : 0;
        int32_t pressure = ClampI32(shortage, -35, 220);
        settlement->price[good] = MinimumI32(99,
            BasePrice((CcGood)good) * (100 + pressure) / 100);
        if (settlement->price[good] < 1) settlement->price[good] = 1;
    }
    if (produced[CC_GOOD_FOOD] > 0 &&
        CcSettlementHasService(settlement, CC_SERVICE_FARM) &&
        settlement->field_yield > 0) {
        WearOneTool(settlement, &settlement->farm_tool_wear, 4);
    }
    if (produced[CC_GOOD_IRON] > 0) {
        AdvanceRareMineWork(sim, settlement, produced[CC_GOOD_IRON]);
        WearOneTool(settlement, &settlement->mine_tool_wear, 3);
    }
    RunSmithy(sim, settlement);
    int32_t war_burden = CcSimWarBurdenAtSettlement(sim, settlement->id);
    if (IsWarSeat(settlement) && war_burden >= 50 &&
        sim->current_day % 14 == 0 &&
        settlement->stock[CC_GOOD_WEAPONS] > 0) {
        settlement->stock[CC_GOOD_WEAPONS] -= 1;
        char text[CC_EVENT_TEXT_CAPACITY];
        (void)snprintf(text, sizeof(text),
                       "%s loses one Weapons bundle in border fighting.",
                       settlement->name);
        (void)PushEvent(sim, CC_EVENT_WAR_MATERIEL_LOST, settlement->id,
                        settlement->id, LatestLocalCause(sim, settlement->id),
                        1, text);
    }

    int32_t food_use = WeeklyFoodUse(sim, settlement);
    int32_t coverage = settlement->stock[CC_GOOD_FOOD] / food_use;
    int32_t food_unmet = MaximumI32(0, food_required - food_eaten);
    int32_t hunger_delta = food_unmet > 0 ?
        2 + food_unmet * 4 / food_required :
        coverage >= 8 ? -4 : coverage >= 5 ? -2 : -1;
    settlement->hunger = ClampI32(settlement->hunger + hunger_delta, 0, 100);
    if (CcSettlementHasService(settlement, CC_SERVICE_HEALER)) {
        settlement->hunger = ClampI32(settlement->hunger - 1, 0, 100);
    }
    int32_t prosperity_change = settlement->hunger > 55 ? -2 :
                                settlement->hunger > 30 ? -1 :
                                coverage >= 8 &&
                                settlement->prosperity < 78 ? 1 :
                                settlement->hunger < 20 && coverage >= 3 &&
                                settlement->prosperity < 45 ? 1 : 0;
    settlement->prosperity = ClampI32(
        settlement->prosperity + prosperity_change, 0, 100);
    if (CcSettlementHasService(settlement, CC_SERVICE_MARKET) &&
        settlement->hunger < 45 && settlement->prosperity < 78) {
        settlement->prosperity = ClampI32(settlement->prosperity + 1, 0, 100);
    }
    int32_t local_threat = MonsterPressureAtSettlement(sim, settlement->id);
    int32_t security_change = settlement->hunger > 50 || local_threat > 60 ? -1 :
                              settlement->security < 45 &&
                              settlement->hunger < 35 ? 1 :
                              settlement->prosperity > 70 &&
                              settlement->hunger < 15 &&
                              settlement->security < 80 ? 1 : 0;
    settlement->security = ClampI32(
        settlement->security + security_change, 0, 100);
    if (CcSettlementHasService(settlement, CC_SERVICE_BARRACKS)) {
        settlement->security = ClampI32(settlement->security + 1, 0, 100);
    }
    if (sim->current_day % 28 == 0) {
        int32_t population_delta = settlement->hunger > 65 ? -MaximumI32(1, settlement->population / 250) :
                                   settlement->prosperity > 70 &&
                                   settlement->hunger < 15 &&
                                   settlement->population <
                                       SettlementPopulationCapacity(settlement) ?
                                   MaximumI32(1, settlement->population / 500) : 0;
        settlement->population = MaximumI32(
            0, settlement->population + population_delta);
        if (settlement->population < 80 && settlement->hunger >= 65) {
            CcKingdom *kingdom = KingdomMutable(sim, settlement->kingdom_id);
            if (kingdom != NULL) {
                kingdom->treasury += settlement->market_coins +
                                     settlement->war_chest;
            }
            settlement->market_coins = 0;
            settlement->war_chest = 0;
            settlement->population = 0;
            settlement->security = 0;
            settlement->prosperity = 0;
            settlement->hunger = 100;
            settlement->field_yield = settlement->field_yield * 85 / 100;
            settlement->service_mask = 0U;
            settlement->service_project = CC_SERVICE_NONE;
            settlement->service_project_days = 0;
            for (int32_t route = 0; route < sim->route_count; ++route) {
                CcRoute *road = &sim->routes[route];
                if (road->from_id != settlement->id &&
                    road->to_id != settlement->id) continue;
                road->closed = true;
                road->condition = MinimumI32(road->condition, 12);
            }
            char text[CC_EVENT_TEXT_CAPACITY];
            (void)snprintf(
                text, sizeof(text),
                "%s is abandoned after famine reduces its last households to refugees.",
                settlement->name);
            (void)PushEvent(
                sim, CC_EVENT_KINGDOM_ACTION, settlement->id,
                settlement->id, LatestLocalCause(sim, settlement->id),
                -1, text);
            return;
        }
    }

    int32_t level = settlement->hunger >= 65 ? 3 :
                    settlement->hunger >= 40 ? 2 :
                    settlement->hunger >= 20 ? 1 : 0;
    if (level > sim->last_shortage_level[index]) {
        char text[CC_EVENT_TEXT_CAPACITY];
        (void)snprintf(text, sizeof(text),
                       "%s has %d weeks of food; hunger reaches pressure level %d.",
                       settlement->name, coverage, level);
        (void)PushEvent(sim, CC_EVENT_SHORTAGE, settlement->id,
                        settlement->id, LatestLocalCause(sim, settlement->id),
                        settlement->hunger, text);
    }
    sim->last_shortage_level[index] = level;
}

static uint32_t RecolonizedServiceMask(const CcSettlement *settlement)
{
    uint32_t mask = ServiceBit(CC_SERVICE_INN);
    if (settlement->field_yield > 0) {
        return mask | ServiceBit(CC_SERVICE_FARM);
    }
    if (settlement->iron_deposit > 0) {
        return mask | ServiceBit(CC_SERVICE_MINE);
    }
    return mask | ServiceBit(CC_SERVICE_MARKET);
}

static void AdvanceRuins(CcSim *sim)
{
    if (sim == NULL || sim->current_day % 7 != 0) return;
    int32_t week = sim->current_day / 7;
    for (int32_t slot = 0; slot < sim->settlement_count; ++slot) {
        CcSettlement *ruin = &sim->settlements[slot];
        if (!CcSettlementIsAbandoned(ruin) ||
            (week + slot * 37) % (8 * 52) != 0) continue;

        CcSettlement *donor = NULL;
        int32_t donor_score = INT32_MIN;
        for (int32_t route_slot = 0;
             route_slot < sim->route_count; ++route_slot) {
            const CcRoute *route = &sim->routes[route_slot];
            CcId neighbor_id = route->from_id == ruin->id ? route->to_id :
                route->to_id == ruin->id ? route->from_id : 0U;
            CcSettlement *candidate = CcSimSettlementMutable(
                sim, neighbor_id);
            if (candidate == NULL || CcSettlementIsAbandoned(candidate) ||
                candidate->population < 900 || candidate->hunger > 25 ||
                candidate->prosperity < 55 ||
                candidate->stock[CC_GOOD_FOOD] < 18 ||
                candidate->stock[CC_GOOD_TOOLS] < 2 ||
                candidate->market_coins < 20) continue;
            int32_t score = candidate->population +
                candidate->prosperity * 12 - candidate->hunger * 20;
            if (donor == NULL || score > donor_score) {
                donor = candidate;
                donor_score = score;
            }
        }
        if (donor == NULL) continue;

        donor->population -= 180;
        donor->stock[CC_GOOD_FOOD] -= 12;
        donor->stock[CC_GOOD_TOOLS] -= 2;
        donor->market_coins -= 12;
        ruin->kingdom_id = donor->kingdom_id;
        ruin->population = 180;
        ruin->security = 15;
        ruin->prosperity = 20;
        ruin->hunger = 25;
        ruin->service_mask = RecolonizedServiceMask(ruin);
        ruin->stock[CC_GOOD_FOOD] += 10;
        ruin->stock[CC_GOOD_TOOLS] += 1;
        ruin->market_coins += 12;
        char text[CC_EVENT_TEXT_CAPACITY];
        (void)snprintf(
            text, sizeof(text),
            "%s sends 180 settlers to reclaim %s; the ruin changes allegiance.",
            donor->name, ruin->name);
        (void)PushEvent(
            sim, CC_EVENT_KINGDOM_ACTION, ruin->id, ruin->id,
            LatestLocalCause(sim, donor->id), 180, text);
    }

    if (sim->current_day % (40 * 364) == 0) {
        int32_t climate = CcSimClimateFactor(sim);
        char text[CC_EVENT_TEXT_CAPACITY];
        (void)snprintf(
            text, sizeof(text),
            climate < 90 ?
                "A long lean era begins; regional field yield falls to %d%%." :
                climate > 110 ?
                "A long generous era begins; regional field yield rises to %d%%." :
                "A temperate era begins; regional field yield settles at %d%%.",
            climate);
        (void)PushEvent(
            sim, climate < 90 ? CC_EVENT_HARVEST_FAILED :
                CC_EVENT_KINGDOM_ACTION,
            0U, 0U, 0U, climate, text);
    }
}

static CcBanditGroup *BanditsOnRoute(CcSim *sim, CcId route_id)
{
    for (int32_t i = 0; i < sim->bandit_count; ++i) {
        if (sim->bandits[i].route_id == route_id) return &sim->bandits[i];
    }
    return NULL;
}

const CcBanditGroup *CcSimBanditGroupOnRoute(const CcSim *sim,
                                             CcId route_id)
{
    if (sim == NULL || route_id == 0U) return NULL;
    for (int32_t i = 0; i < sim->bandit_count; ++i) {
        if (sim->bandits[i].route_id == route_id) return &sim->bandits[i];
    }
    return NULL;
}

int32_t CcSimBanditReactionRoll(const CcSim *sim, CcId route_id)
{
    const CcBanditGroup *bandits = CcSimBanditGroupOnRoute(sim, route_id);
    if (sim == NULL || bandits == NULL) return 7;
    uint32_t value = sim->world_seed ^ (uint32_t)route_id ^
                     (uint32_t)(route_id >> 32U) ^
                     (uint32_t)bandits->id ^ (uint32_t)sim->current_day;
    value ^= value >> 16U;
    value *= UINT32_C(0x7feb352d);
    value ^= value >> 15U;
    int32_t first = 1 + (int32_t)(value % 6U);
    int32_t second = 1 + (int32_t)((value / 7U) % 6U);
    int32_t modifier = ClampI32(sim->player.reputation / 20, -2, 2);
    if (bandits->supplies < 24) modifier -= 1;
    if (bandits->influence >= 75) modifier -= 1;
    return ClampI32(first + second + modifier, 2, 12);
}

const char *CcBanditReactionName(int32_t roll)
{
    if (roll <= 5) return "HOSTILE";
    if (roll <= 8) return "WARY";
    if (roll <= 10) return "READY TO BARGAIN";
    return "OPEN TO TERMS";
}

bool CcSimBanditProvisionDemand(const CcSim *sim, CcId route_id,
                                CcGood *good, int32_t *quantity)
{
    const CcBanditGroup *bandits = CcSimBanditGroupOnRoute(sim, route_id);
    if (bandits == NULL) return false;
    CcGood wanted = bandits->raid_good;
    if (wanted < CC_GOOD_FOOD || wanted > CC_GOOD_WEAPONS) {
        wanted = CC_GOOD_FOOD;
    }
    int32_t wanted_quantity = 1 + (int32_t)bandits->camp_size;
    if (wanted == CC_GOOD_TOOLS || wanted == CC_GOOD_WEAPONS) {
        wanted_quantity = MinimumI32(wanted_quantity, 2);
    }
    int32_t reaction = CcSimBanditReactionRoll(sim, route_id);
    if (reaction <= 5) wanted_quantity += 1;
    if (reaction >= 10) wanted_quantity = MaximumI32(1,
                                                      wanted_quantity - 1);
    if (good != NULL) *good = wanted;
    if (quantity != NULL) *quantity = wanted_quantity;
    return true;
}

static CcBanditGroup *BanditMutable(CcSim *sim, CcId id)
{
    if (sim == NULL || CcIdKind(id) != CC_ENTITY_BANDIT_GROUP) return NULL;
    for (int32_t i = 0; i < sim->bandit_count; ++i) {
        if (sim->bandits[i].id == id) return &sim->bandits[i];
    }
    return NULL;
}

static int32_t ServiceMaskCount(uint32_t mask)
{
    int32_t count = 0;
    for (int32_t service = 0; service < CC_SERVICE_COUNT; ++service) {
        if ((mask & ServiceBit((CcServiceKind)service)) != 0U) count += 1;
    }
    return count;
}

static void GrowBanditCamp(CcBanditGroup *bandits)
{
    CcBanditCampSize desired = bandits->influence >= 80 ||
            bandits->raids_completed >= 6 ? CC_BANDIT_OUTLAW_TOWN :
        bandits->influence >= 60 || bandits->raids_completed >= 3 ?
            CC_BANDIT_WAR_CAMP :
        bandits->influence >= 35 ? CC_BANDIT_CAMP : CC_BANDIT_HIDEOUT;
    if (desired > bandits->camp_size) bandits->camp_size = desired;
    static const CcServiceKind order[] = {
        CC_SERVICE_BLACK_MARKET, CC_SERVICE_STABLE, CC_SERVICE_SMITHY,
        CC_SERVICE_HEALER, CC_SERVICE_BARRACKS, CC_SERVICE_MARKET,
        CC_SERVICE_GRANARY, CC_SERVICE_GUILDHALL
    };
    int32_t capacity = CcBanditCampServiceCapacity(bandits->camp_size);
    for (size_t i = 0; i < sizeof(order) / sizeof(order[0]) &&
                       ServiceMaskCount(bandits->service_mask) < capacity; ++i) {
        bandits->service_mask |= ServiceBit(order[i]);
    }
}

bool CcSimLaunchBanditRaid(CcSim *sim, CcId bandit_id,
                           char *error, size_t error_capacity)
{
    CcBanditGroup *bandits = BanditMutable(sim, bandit_id);
    const CcRoute *route = bandits != NULL ? CcSimRoute(sim, bandits->route_id) : NULL;
    if (bandits == NULL || route == NULL) {
        SetError(error, error_capacity, "Bandit camp is invalid.");
        return false;
    }
    if (bandits->raid_phase != CC_BANDIT_RAID_IDLE) {
        SetError(error, error_capacity, "This bandit expedition is already away.");
        return false;
    }
    if (bandits->members < 12) {
        SetError(error, error_capacity, "The camp cannot muster enough raiders.");
        return false;
    }
    CcSettlement *candidates[2] = {
        CcSimSettlementMutable(sim, route->from_id),
        CcSimSettlementMutable(sim, route->to_id)
    };
    CcSettlement *target = NULL;
    int32_t best_score = INT_MIN;
    for (int32_t i = 0; i < 2; ++i) {
        CcSettlement *candidate = candidates[i];
        if (candidate == NULL || CcSettlementIsAbandoned(candidate)) continue;
        int32_t score = -candidate->security * 2;
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
            score += candidate->stock[good];
        }
        if (target == NULL || score > best_score) {
            target = candidate;
            best_score = score;
        }
    }
    if (target == NULL) {
        SetError(error, error_capacity, "The camp has no reachable raid target.");
        return false;
    }
    CcGood good = CC_GOOD_FOOD;
    for (int32_t candidate = 1; candidate < CC_GOOD_COUNT; ++candidate) {
        if (target->stock[candidate] > target->stock[good]) {
            good = (CcGood)candidate;
        }
    }
    if (target->stock[good] < 4) {
        SetError(error, error_capacity,
                 "The nearby settlements are too poor to raid.");
        return false;
    }
    bandits->raid_phase = CC_BANDIT_RAID_SCOUTING;
    bandits->raid_target_id = target->id;
    bandits->raid_good = good;
    bandits->raid_quantity = 0;
    bandits->raid_days_remaining = 2;
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(text, sizeof(text),
                   "%s scouts %s for a %s raid from the no-man's-land camp.",
                   bandits->name, target->name, CcGoodName(good));
    (void)PushEvent(sim, CC_EVENT_BANDIT_RAID_DEPARTED, bandits->id,
                    bandits->route_id, LatestLocalCause(sim, target->id),
                    bandits->members, text);
    SetError(error, error_capacity, "");
    return true;
}

static void AdvanceBanditRaids(CcSim *sim)
{
    for (int32_t i = 0; i < sim->bandit_count; ++i) {
        CcBanditGroup *bandits = &sim->bandits[i];
        if (bandits->raid_phase == CC_BANDIT_RAID_IDLE) continue;
        const CcRoute *route = CcSimRoute(sim, bandits->route_id);
        CcSettlement *target = CcSimSettlementMutable(sim, bandits->raid_target_id);
        if (route == NULL || target == NULL) {
            bandits->raid_phase = CC_BANDIT_RAID_IDLE;
            bandits->raid_target_id = 0U;
            bandits->raid_days_remaining = 0;
            continue;
        }
        bandits->raid_days_remaining =
            MaximumI32(0, bandits->raid_days_remaining - 1);
        if (bandits->raid_days_remaining > 0) continue;
        switch (bandits->raid_phase) {
            case CC_BANDIT_RAID_SCOUTING:
                bandits->raid_phase = CC_BANDIT_RAID_MUSTERING;
                bandits->raid_days_remaining = 2;
                break;
            case CC_BANDIT_RAID_MUSTERING:
                bandits->raid_phase = CC_BANDIT_RAID_OUTBOUND;
                bandits->raid_days_remaining = route->travel_days;
                break;
            case CC_BANDIT_RAID_OUTBOUND: {
                int32_t wanted = ClampI32(bandits->members / 3, 4, 28);
                int32_t stolen = MinimumI32(target->stock[bandits->raid_good], wanted);
                target->stock[bandits->raid_good] -= stolen;
                if (stolen > 0) {
                    target->security = ClampI32(target->security - 4, 0, 100);
                    target->prosperity = ClampI32(
                        target->prosperity - 3, 0, 100);
                }
                if (stolen > 0 && bandits->raid_good == CC_GOOD_FOOD) {
                    target->hunger = ClampI32(target->hunger +
                        MaximumI32(1, stolen / 4), 0, 100);
                }
                bandits->raid_quantity = stolen;
                bandits->raid_phase = CC_BANDIT_RAID_RETURNING;
                bandits->raid_days_remaining = route->travel_days;
                char text[CC_EVENT_TEXT_CAPACITY];
                (void)snprintf(text, sizeof(text),
                               "%s raids %s and takes %d %s.",
                               bandits->name, target->name, stolen,
                               CcGoodName(bandits->raid_good));
                const CcEvent *departure = LatestEvent(
                    sim, CC_EVENT_BANDIT_RAID_DEPARTED, bandits->id,
                    bandits->route_id);
                (void)PushEvent(sim, CC_EVENT_SETTLEMENT_RAIDED, bandits->id,
                                target->id,
                                departure != NULL ? departure->id : 0U,
                                stolen, text);
                break;
            }
            case CC_BANDIT_RAID_RETURNING: {
                int32_t loot = bandits->raid_quantity;
                bandits->supplies = ClampI32(bandits->supplies + loot, 0, 100);
                bandits->members = ClampI32(bandits->members +
                    (loot > 0 ? MaximumI32(1, loot / 12) : -3), 4, 120);
                if (loot > 0) bandits->raids_completed += 1;
                bandits->influence = ClampI32(
                    (bandits->members + bandits->supplies) / 2 +
                    MinimumI32(20, bandits->raids_completed / 3), 0, 100);
                GrowBanditCamp(bandits);
                char text[CC_EVENT_TEXT_CAPACITY];
                (void)snprintf(text, sizeof(text),
                               "%s returns with %d %s; the %s now supports %d services.",
                               bandits->name, loot, CcGoodName(bandits->raid_good),
                               CcBanditCampSizeName(bandits->camp_size),
                               ServiceMaskCount(bandits->service_mask));
                const CcEvent *raid = LatestEvent(
                    sim, CC_EVENT_SETTLEMENT_RAIDED, bandits->id,
                    bandits->raid_target_id);
                (void)PushEvent(sim, CC_EVENT_BANDIT_RAID_RETURNED, bandits->id,
                                bandits->route_id,
                                raid != NULL ? raid->id : 0U, loot, text);
                bandits->raid_phase = CC_BANDIT_RAID_IDLE;
                bandits->raid_target_id = 0U;
                bandits->raid_quantity = 0;
                bandits->raid_days_remaining = 0;
                break;
            }
            case CC_BANDIT_RAID_IDLE:
                break;
        }
    }
}

static int32_t GoblinTravelDays(const CcSim *sim, CcId target_id)
{
    const CcSettlement *lair = CcSimSettlement(
        sim, sim->goblins.lair_settlement_id);
    const CcSettlement *target = CcSimSettlement(sim, target_id);
    if (lair == NULL || target == NULL) return 3;
    int32_t map_distance = AbsoluteI32(lair->map_x - target->map_x) +
                           AbsoluteI32(lair->map_y - target->map_y);
    return ClampI32(2 + map_distance / 260, 2, 6);
}

static CcTreasure *TreasureAtSettlementMutable(CcSim *sim,
                                                CcId settlement_id)
{
    for (int32_t i = 0; i < sim->treasure_count; ++i) {
        CcTreasure *treasure = &sim->treasures[i];
        if (!treasure->destroyed && treasure->owner_id == settlement_id &&
            treasure->location_id == settlement_id) return treasure;
    }
    return NULL;
}

static CcSettlement *RichestDragonTarget(CcSim *sim)
{
    CcSettlement *best = NULL;
    int64_t best_score = INT64_MIN;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        CcSettlement *place = &sim->settlements[i];
        if (CcSettlementIsAbandoned(place) ||
            place->id == sim->dragon.lair_settlement_id) continue;
        CcKingdom *kingdom = KingdomMutable(sim, place->kingdom_id);
        int64_t score = (int64_t)place->prosperity * 4 +
                        (int64_t)CcSettlementServiceCount(place) * 8 +
                        (kingdom != NULL ? kingdom->treasury / 20 : 0);
        if (best == NULL || score > best_score) {
            best = place;
            best_score = score;
        }
    }
    return best;
}

static CcEvent *StartDragonTheft(CcSim *sim, CcId thief_id,
                                 CcSettlement *target, int32_t amount,
                                 CcId parent_event_id,
                                 const char *theft_text)
{
    if (sim == NULL || target == NULL || amount <= 0 || sim->dragon.slain ||
        sim->dragon.stolen_outstanding > 0 ||
        (CcMoney)amount > sim->dragon.hoard) return NULL;
    CcMoney hoard_before = sim->dragon.hoard;
    sim->dragon.hoard -= amount;
    sim->dragon.stolen_outstanding = amount;
    sim->dragon.theft_actor_id = thief_id;
    sim->dragon.retaliation_target_id = target->id;
    sim->dragon.omen_days_remaining = 14;
    int32_t wound = 5 + (int32_t)MinimumI32(
        55, hoard_before > 0 ?
            (int32_t)(((int64_t)amount * 100) / hoard_before) : 55);
    sim->dragon.memory_integrity = ClampI32(
        sim->dragon.memory_integrity - wound, 0, 100);
    sim->dragon.crown_continuity_days = 0;
    sim->dragon.activity = CC_DRAGON_ACTIVITY_RETALIATING;
    CcEvent *theft = PushEvent(
        sim, CC_EVENT_DRAGON_HOARD_STOLEN, thief_id,
        sim->dragon.lair_settlement_id, parent_event_id,
        amount, theft_text);
    sim->dragon.hoard_event_id = theft != NULL ? theft->id :
                                 sim->dragon.hoard_event_id;
    char omen_text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(omen_text, sizeof(omen_text),
                   "Smoke falls into %s's chimneys; old readers count 14 nights until %s comes.",
                   target->name, sim->dragon.name);
    CcEvent *omen = PushEvent(
        sim, CC_EVENT_DRAGON_OMEN, sim->dragon.id, target->id,
        theft != NULL ? theft->id : 0U, 14, omen_text);
    sim->dragon.omen_event_id = omen != NULL ? omen->id : 0U;
    return theft;
}

static void PlanGoblinTribute(CcSim *sim)
{
    CcGoblinCult *goblins = &sim->goblins;
    if (goblins->tribute_phase != CC_GOBLIN_TRIBUTE_IDLE ||
        goblins->tribute_cooldown_days > 0 ||
        sim->dragon.stolen_outstanding > 0) return;
    if (goblins->lair_stock[CC_GOOD_FOOD] < 8) {
        goblins->raid_motive = CC_GOBLIN_RAID_HUNGER;
    } else if (goblins->lair_stock[CC_GOOD_TOOLS] < 2 ||
               goblins->lair_stock[CC_GOOD_WEAPONS] < 3) {
        goblins->raid_motive = CC_GOBLIN_RAID_EQUIPMENT;
    } else if (sim->dragon.slain) {
        /* A dead dragon has no hidden replacement clutch to provision.
           The cult may still raid when hungry or under-equipped, but it
           cannot manufacture a successor from stolen relics. */
        goblins->tribute_cooldown_days = 14;
        return;
    } else {
        goblins->raid_motive = CC_GOBLIN_RAID_DRAGON_TRIBUTE;
    }
    CcSettlement *target = NULL;
    int64_t best_score = INT64_MIN;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        CcSettlement *place = &sim->settlements[i];
        if (CcSettlementIsAbandoned(place) ||
            place->id == sim->dragon.lair_settlement_id ||
            place->id == goblins->lair_settlement_id) continue;
        int64_t score = -place->security * 2;
        if (goblins->raid_motive == CC_GOBLIN_RAID_HUNGER) {
            score += place->stock[CC_GOOD_FOOD] * 4;
        } else if (goblins->raid_motive == CC_GOBLIN_RAID_EQUIPMENT) {
            score += place->stock[CC_GOOD_IRON] +
                     place->stock[CC_GOOD_TOOLS] * 10 +
                     place->stock[CC_GOOD_WEAPONS] * 15;
        } else {
            CcTreasure *treasure = TreasureAtSettlementMutable(sim, place->id);
            score += place->market_coins / 3 +
                     place->stock[CC_GOOD_GOLD] * 40 +
                     place->stock[CC_GOOD_GEMS] * 70 +
                     (treasure != NULL ? treasure->appraised_value : 0);
        }
        if (target == NULL || score > best_score) {
            target = place;
            best_score = score;
        }
    }
    if (target == NULL) return;
    goblins->tribute_phase = CC_GOBLIN_TRIBUTE_PREPARING;
    goblins->tribute_target_id = target->id;
    goblins->target_warned = false;
    goblins->carried_tribute = 0;
    goblins->carried_treasure_id = 0U;
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        goblins->carried_goods[good] = 0;
    }
    goblins->tribute_days_remaining = GoblinTravelDays(sim, target->id);
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(text, sizeof(text),
                   "Nara names %.24s for %.32s. %.20s musters until tomorrow.",
                   target->name,
                   goblins->raid_motive == CC_GOBLIN_RAID_HUNGER ? "Food" :
                   goblins->raid_motive == CC_GOBLIN_RAID_EQUIPMENT ?
                       "Tools and Weapons" : sim->dragon.slain ?
                       "coin and relics for the dead dragon" :
                       "dragon tribute", goblins->name);
    CcEvent *event = PushEvent(
        sim, CC_EVENT_GOBLIN_RAID_PREPARED, goblins->id,
        target->id, LatestLocalCause(sim, target->id),
        goblins->tribute_days_remaining, text);
    goblins->tribute_event_id = event != NULL ? event->id : 0U;
}

static void AdvanceGoblinTribute(CcSim *sim)
{
    CcGoblinCult *goblins = &sim->goblins;
    if (goblins->tribute_phase == CC_GOBLIN_TRIBUTE_IDLE) {
        if (sim->current_day % 7 == 0) {
            int32_t food_needed = sim->dragon.slain ?
                1 + (goblins->members - 1) / 24 : 1;
            int32_t food_eaten = MinimumI32(
                goblins->lair_stock[CC_GOOD_FOOD], food_needed);
            goblins->lair_stock[CC_GOOD_FOOD] -= food_eaten;
            int32_t hunger_loss = food_needed - food_eaten;
            goblins->members = MaximumI32(
                12, goblins->members - hunger_loss);
            if (hunger_loss > 0) {
                goblins->cohesion = ClampI32(
                    goblins->cohesion - hunger_loss * 3, 0, 100);
            }
        }
        goblins->tribute_cooldown_days = MaximumI32(
            0, goblins->tribute_cooldown_days - 1);
        return;
    }
    CcSettlement *target = CcSimSettlementMutable(sim,
        goblins->tribute_target_id);
    if (target == NULL) {
        goblins->tribute_phase = CC_GOBLIN_TRIBUTE_IDLE;
        goblins->tribute_target_id = 0U;
        goblins->tribute_event_id = 0U;
        goblins->carried_tribute = 0;
        goblins->carried_treasure_id = 0U;
        goblins->raid_motive = CC_GOBLIN_RAID_NONE;
        goblins->target_warned = false;
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
            goblins->carried_goods[good] = 0;
        }
        goblins->tribute_days_remaining = 0;
        goblins->tribute_cooldown_days = 14;
        return;
    }
    if (goblins->tribute_phase == CC_GOBLIN_TRIBUTE_PREPARING) {
        goblins->tribute_phase = CC_GOBLIN_TRIBUTE_OUTBOUND;
        char text[CC_EVENT_TEXT_CAPACITY];
        (void)snprintf(text, sizeof(text),
                       "%s leaves its lair to raid %s%s.",
                       goblins->name, target->name,
                       goblins->target_warned ?
                           ", where warned defenders wait" : "");
        CcEvent *event = PushEvent(
            sim, CC_EVENT_GOBLIN_RAID_DEPARTED, goblins->id,
            target->id, goblins->tribute_event_id,
            goblins->tribute_days_remaining, text);
        goblins->tribute_event_id = event != NULL ? event->id :
                                      goblins->tribute_event_id;
    }

    goblins->tribute_days_remaining = MaximumI32(
        0, goblins->tribute_days_remaining - 1);
    if (goblins->tribute_days_remaining > 0) return;

    if (goblins->tribute_phase == CC_GOBLIN_TRIBUTE_OUTBOUND) {
        static const int32_t raid_capacity[CC_GOOD_COUNT] = {16, 8, 4, 4, 2, 1};
        CcGood chosen = CC_GOOD_FOOD;
        if (goblins->raid_motive == CC_GOBLIN_RAID_EQUIPMENT) {
            chosen = target->stock[CC_GOOD_WEAPONS] > 0 ?
                CC_GOOD_WEAPONS : target->stock[CC_GOOD_TOOLS] > 0 ?
                CC_GOOD_TOOLS : CC_GOOD_IRON;
        } else if (goblins->raid_motive == CC_GOBLIN_RAID_DRAGON_TRIBUTE) {
            CcTreasure *treasure = TreasureAtSettlementMutable(sim, target->id);
            if (treasure != NULL && !goblins->target_warned) {
                treasure->owner_id = goblins->id;
                treasure->location_id = target->id;
                goblins->carried_treasure_id = treasure->id;
            }
            chosen = target->stock[CC_GOOD_GEMS] > 0 ? CC_GOOD_GEMS :
                     target->stock[CC_GOOD_GOLD] > 0 ? CC_GOOD_GOLD :
                     CC_GOOD_FOOD;
        }
        int32_t capacity = raid_capacity[chosen];
        if (goblins->target_warned) capacity = MaximumI32(1, capacity / 2);
        int32_t taken_goods = MinimumI32(target->stock[chosen], capacity);
        target->stock[chosen] -= taken_goods;
        goblins->carried_goods[chosen] = taken_goods;
        CcMoney taken_coins = 0;
        if (goblins->raid_motive == CC_GOBLIN_RAID_DRAGON_TRIBUTE) {
            CcMoney wanted = 8 + goblins->members / 6;
            if (goblins->target_warned) wanted = wanted > 1 ? wanted / 2 : 1;
            taken_coins = MinimumI32(
                target->market_coins > INT32_MAX ? INT32_MAX :
                    (int32_t)target->market_coins,
                wanted > INT32_MAX ? INT32_MAX : (int32_t)wanted);
            target->market_coins -= taken_coins;
            goblins->carried_tribute = taken_coins;
        }
        target->prosperity = ClampI32(target->prosperity - 2, 0, 100);
        target->security = ClampI32(target->security - 2, 0, 100);
        if (chosen == CC_GOOD_FOOD) {
            target->hunger = ClampI32(target->hunger +
                MaximumI32(1, taken_goods / 4), 0, 100);
        }
        goblins->tribute_phase = CC_GOBLIN_TRIBUTE_RETURNING;
        goblins->tribute_days_remaining = GoblinTravelDays(sim, target->id);
        char text[CC_EVENT_TEXT_CAPACITY];
        (void)snprintf(text, sizeof(text),
                       "%.16s raids %.16s: %d %.8s, %" PRId64 " crowns%s.",
                       goblins->name, target->name, taken_goods,
                       CcGoodName(chosen), taken_coins,
                       goblins->carried_treasure_id != 0U ?
                           ", and a named treasure" : "");
        CcEvent *event = PushEvent(
            sim, CC_EVENT_GOBLIN_RAIDED, goblins->id, target->id,
            goblins->tribute_event_id, taken_goods + (int32_t)taken_coins, text);
        goblins->tribute_event_id = event != NULL ? event->id :
                                      goblins->tribute_event_id;
        goblins->target_warned = false;
        return;
    }

    if (goblins->tribute_phase == CC_GOBLIN_TRIBUTE_RETURNING) {
        CcId raid_origin = target->id;
        int32_t returned_value = goblins->carried_tribute > INT32_MAX ?
            INT32_MAX : (int32_t)goblins->carried_tribute;
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
            int32_t carried = goblins->carried_goods[good];
            returned_value = carried > INT32_MAX - returned_value ?
                INT32_MAX : returned_value + carried;
            goblins->lair_stock[good] += goblins->carried_goods[good];
            goblins->carried_goods[good] = 0;
        }
        goblins->cohesion = ClampI32(
            goblins->cohesion + (returned_value > 0 ? 1 : -3), 0, 100);
        goblins->lair_coins += goblins->carried_tribute;
        goblins->carried_tribute = 0;
        if (goblins->carried_treasure_id != 0U) {
            CcTreasure *treasure = (CcTreasure *)CcSimTreasure(
                sim, goblins->carried_treasure_id);
            if (treasure != NULL) treasure->location_id = goblins->lair_settlement_id;
        }
        goblins->last_tribute_origin_id = raid_origin;
        char text[CC_EVENT_TEXT_CAPACITY];
        (void)snprintf(text, sizeof(text),
                       "%s returns to its lair from %s; Food and gear enter the lair stores.",
                       goblins->name, target->name);
        CcEvent *event = PushEvent(
            sim, CC_EVENT_GOBLIN_RAID_RETURNED, goblins->id,
            goblins->lair_settlement_id, goblins->tribute_event_id,
            goblins->members, text);
        goblins->tribute_event_id = event != NULL ? event->id :
                                      goblins->tribute_event_id;

        if (sim->dragon.slain) {
            bool relic_raid = goblins->raid_motive ==
                              CC_GOBLIN_RAID_DRAGON_TRIBUTE;
            if (goblins->carried_treasure_id != 0U) {
                CcTreasure *treasure = (CcTreasure *)CcSimTreasure(
                    sim, goblins->carried_treasure_id);
                if (treasure != NULL) {
                    treasure->owner_id = goblins->id;
                    treasure->location_id = goblins->lair_settlement_id;
                }
            }
            goblins->tribute_phase = CC_GOBLIN_TRIBUTE_IDLE;
            goblins->raid_motive = CC_GOBLIN_RAID_NONE;
            goblins->tribute_target_id = 0U;
            goblins->tribute_event_id = 0U;
            goblins->carried_treasure_id = 0U;
            goblins->target_warned = false;
            goblins->tribute_days_remaining = 0;
            if (relic_raid) {
                goblins->devotion = ClampI32(
                    goblins->devotion + 2, 0, 100);
                goblins->tribute_cooldown_days = 120 +
                    (int32_t)(NextRandom(sim) % 121U);
            } else {
                goblins->tribute_cooldown_days = 21 +
                    (int32_t)(NextRandom(sim) % 15U);
            }
            return;
        }

        for (int32_t good = CC_GOOD_GOLD; good <= CC_GOOD_GEMS; ++good) {
            goblins->carried_goods[good] = goblins->lair_stock[good];
            goblins->lair_stock[good] = 0;
        }
        goblins->carried_tribute = goblins->lair_coins;
        goblins->lair_coins = 0;
        bool has_offering = goblins->carried_tribute > 0 ||
            goblins->carried_goods[CC_GOOD_GOLD] > 0 ||
            goblins->carried_goods[CC_GOOD_GEMS] > 0 ||
            goblins->carried_treasure_id != 0U;
        if (has_offering) {
            goblins->tribute_phase = CC_GOBLIN_TRIBUTE_TO_DRAGON;
            goblins->tribute_target_id = sim->dragon.lair_settlement_id;
            goblins->tribute_days_remaining = GoblinTravelDays(
                sim, sim->dragon.lair_settlement_id);
            (void)snprintf(text, sizeof(text),
                           "%s carries its portable spoils from the lair toward %s's cave.",
                           goblins->name, sim->dragon.name);
            event = PushEvent(sim, CC_EVENT_GOBLIN_TRIBUTE_DEPARTED,
                              goblins->id, sim->dragon.lair_settlement_id,
                              goblins->tribute_event_id,
                              goblins->tribute_days_remaining, text);
            goblins->tribute_event_id = event != NULL ? event->id :
                                          goblins->tribute_event_id;
            return;
        }
        goblins->carried_treasure_id = 0U;
    } else {
        CcMoney delivered = goblins->carried_tribute;
        sim->dragon.hoard += delivered;
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
            sim->dragon.hoard_goods[good] += goblins->carried_goods[good];
            goblins->carried_goods[good] = 0;
        }
        if (goblins->carried_treasure_id != 0U) {
            CcTreasure *treasure = (CcTreasure *)CcSimTreasure(
                sim, goblins->carried_treasure_id);
            if (treasure != NULL) {
                treasure->owner_id = sim->dragon.id;
                treasure->location_id = sim->dragon.lair_settlement_id;
            }
        }
        goblins->tributes_delivered += 1;
        goblins->devotion = ClampI32(goblins->devotion + 1, 0, 100);
        goblins->cohesion = ClampI32(goblins->cohesion + 1, 0, 100);
        char text[CC_EVENT_TEXT_CAPACITY];
        (void)snprintf(text, sizeof(text),
                       "%s lays %" PRId64 " crowns and its portable spoils in %s's hoard.",
                       goblins->name, delivered, sim->dragon.name);
        CcEvent *event = PushEvent(
            sim, CC_EVENT_GOBLIN_TRIBUTE_DELIVERED, goblins->id,
            sim->dragon.lair_settlement_id, goblins->tribute_event_id,
            (int32_t)delivered, text);
        sim->dragon.hoard_event_id = event != NULL ? event->id :
                                     sim->dragon.hoard_event_id;
    }
    goblins->tribute_phase = CC_GOBLIN_TRIBUTE_IDLE;
    goblins->raid_motive = CC_GOBLIN_RAID_NONE;
    goblins->tribute_target_id = 0U;
    goblins->tribute_event_id = 0U;
    goblins->carried_tribute = 0;
    goblins->carried_treasure_id = 0U;
    goblins->target_warned = false;
    goblins->tribute_days_remaining = 0;
    goblins->tribute_cooldown_days = 21 +
        (int32_t)(NextRandom(sim) % 15U);
}

static void PlanHoardRaid(CcSim *sim)
{
    CcHoardRaiders *raiders = &sim->hoard_raiders;
    if (raiders->phase != CC_HOARD_RAIDERS_IDLE ||
        raiders->cooldown_days > 0 ||
        sim->dragon.stolen_outstanding > 0 || sim->dragon.hoard < 25 ||
        sim->dragon.slain) return;
    CcSettlement *origin = NULL;
    int32_t worst_social_pressure = 0;
    int32_t social_pressure = 0;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        CcSettlement *place = &sim->settlements[i];
        if (CcSettlementIsAbandoned(place)) continue;
        int32_t inequality = CcSimInequalityAtSettlement(sim, place->id);
        int32_t debt_pressure = IronLedgerDebtPressure(
            sim, place->kingdom_id);
        int32_t pressure = MaximumI32(inequality, debt_pressure);
        if (place->id != sim->dragon.lair_settlement_id &&
            (place->hunger >= 25 || debt_pressure >= 80) &&
            pressure > social_pressure) {
            social_pressure = pressure;
        }
    }
    if (social_pressure < 55) raiders->social_raid_latched = false;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        CcSettlement *place = &sim->settlements[i];
        if (CcSettlementIsAbandoned(place)) continue;
        int32_t inequality = CcSimInequalityAtSettlement(sim, place->id);
        CcKingdom *kingdom = KingdomMutable(sim, place->kingdom_id);
        int32_t debt_pressure = IronLedgerDebtPressure(
            sim, place->kingdom_id);
        bool bankrupt = debt_pressure >= 80 && kingdom != NULL &&
                        kingdom->treasury < 60;
        int32_t pressure = MaximumI32(inequality, debt_pressure);
        if (raiders->social_raid_latched ||
            place->id == sim->dragon.lair_settlement_id ||
            (!bankrupt && (place->hunger < 25 || inequality < 75))) continue;
        if (origin == NULL || pressure > worst_social_pressure) {
            origin = place;
            worst_social_pressure = pressure;
        }
    }
    CcHoardRaidMotive motive = CC_HOARD_RAID_SOCIAL_RELIEF;
    int32_t pressure_value = worst_social_pressure;
    if (origin == NULL) {
        motive = CC_HOARD_RAID_WAR_FINANCE;
        int32_t war_pressure = 0;
        for (int32_t i = 0; i < sim->settlement_count; ++i) {
            CcSettlement *place = &sim->settlements[i];
            if (CcSettlementIsAbandoned(place)) continue;
            bool war_seat = place->function == CC_SETTLEMENT_FORTRESS ||
                            place->function == CC_SETTLEMENT_CAPITAL;
            int32_t burden = CcSimWarBurdenAtSettlement(sim, place->id);
            int32_t crisis = CcSimWarSupplyCrisisAtSettlement(
                sim, place->id);
            if (war_seat && burden >= 20 && crisis > war_pressure) {
                war_pressure = crisis;
            }
        }
        if (war_pressure < 35) raiders->war_raid_latched = false;
        for (int32_t i = 0; i < sim->settlement_count; ++i) {
            CcSettlement *place = &sim->settlements[i];
            if (CcSettlementIsAbandoned(place)) continue;
            CcKingdom *kingdom = KingdomMutable(sim, place->kingdom_id);
            int32_t burden = CcSimWarBurdenAtSettlement(sim, place->id);
            int32_t crisis = CcSimWarSupplyCrisisAtSettlement(
                sim, place->id);
            CcMoney available_war_gold = kingdom != NULL ?
                kingdom->treasury + place->war_chest : 0;
            bool war_seat = place->function == CC_SETTLEMENT_FORTRESS ||
                            place->function == CC_SETTLEMENT_CAPITAL;
            if (raiders->war_raid_latched ||
                place->id == sim->dragon.lair_settlement_id || !war_seat ||
                burden < 35 || crisis < 55 || kingdom == NULL ||
                available_war_gold >= 200 ||
                kingdom->legitimacy >= 65) continue;
            if (origin == NULL || crisis > pressure_value) {
                origin = place;
                pressure_value = crisis;
            }
        }
    }
    if (origin == NULL) return;

    char text[CC_EVENT_TEXT_CAPACITY];
    CcEventKind pressure_kind;
    if (motive == CC_HOARD_RAID_SOCIAL_RELIEF) {
        CopyName(raiders->name, "The Ash-Poor Company");
        pressure_kind = CC_EVENT_INEQUALITY_PRESSURE;
        if (IronLedgerDebtPressure(sim, origin->kingdom_id) >= 80) {
            (void)snprintf(text, sizeof(text),
                           "In %s, monastery defaults and empty purses drive the Ash-Poor toward the dragon's gold.",
                           origin->name);
        } else {
            (void)snprintf(text, sizeof(text),
                           "In %s, hunger beside guarded wealth drives people toward the dragon's gold.",
                           origin->name);
        }
    } else {
        CopyName(raiders->name, "The Crown Levy");
        pressure_kind = CC_EVENT_WAR_PRESSURE;
        (void)snprintf(text, sizeof(text),
                       "%s cannot move enough pay, wheat, and tools to its garrison; the court turns toward the dragon's gold.",
                       origin->name);
    }
    CcEvent *pressure = PushEvent(
        sim, pressure_kind, raiders->id, origin->id,
        LatestLocalCause(sim, origin->id), pressure_value, text);
    if (motive == CC_HOARD_RAID_SOCIAL_RELIEF) {
        (void)snprintf(text, sizeof(text),
                       "%s leaves %s for the dragon cave with empty purses and bread debts.",
                       raiders->name, origin->name);
    } else {
        (void)snprintf(text, sizeof(text),
                       "%s leaves %s for the dragon cave under sealed military orders.",
                       raiders->name, origin->name);
    }
    CcEvent *departure = PushEvent(
        sim, CC_EVENT_HOARD_HEIST_DEPARTED, raiders->id, origin->id,
        pressure != NULL ? pressure->id : 0U,
        GoblinTravelDays(sim, origin->id), text);
    raiders->phase = CC_HOARD_RAIDERS_OUTBOUND;
    raiders->motive = motive;
    raiders->origin_settlement_id = origin->id;
    raiders->cause_event_id = departure != NULL ? departure->id : 0U;
    raiders->carried_treasure = 0;
    raiders->days_remaining = GoblinTravelDays(sim, origin->id);
}

static void AdvanceHoardRaid(CcSim *sim)
{
    CcHoardRaiders *raiders = &sim->hoard_raiders;
    if (raiders->phase == CC_HOARD_RAIDERS_IDLE) {
        raiders->cooldown_days = MaximumI32(0, raiders->cooldown_days - 1);
        return;
    }
    CcSettlement *origin = CcSimSettlementMutable(
        sim, raiders->origin_settlement_id);
    if (origin == NULL) {
        CcId raider_id = raiders->id;
        int32_t completed = raiders->raids_completed;
        int32_t war_completed = raiders->war_raids_completed;
        bool social_latched = raiders->social_raid_latched;
        bool war_latched = raiders->war_raid_latched;
        *raiders = (CcHoardRaiders){
            .id = raider_id,
            .phase = CC_HOARD_RAIDERS_IDLE,
            .cooldown_days = 28,
            .raids_completed = completed,
            .war_raids_completed = war_completed,
            .social_raid_latched = social_latched,
            .war_raid_latched = war_latched
        };
        CopyName(raiders->name, "The Ash-Poor Company");
        return;
    }
    raiders->days_remaining = MaximumI32(0, raiders->days_remaining - 1);
    if (raiders->days_remaining > 0) return;

    if (raiders->phase == CC_HOARD_RAIDERS_OUTBOUND) {
        if (sim->dragon.stolen_outstanding > 0 || sim->dragon.hoard < 1) {
            raiders->phase = CC_HOARD_RAIDERS_IDLE;
            raiders->origin_settlement_id = 0U;
            raiders->cause_event_id = 0U;
            raiders->motive = CC_HOARD_RAID_NO_MOTIVE;
            raiders->days_remaining = 0;
            raiders->cooldown_days = 28;
            return;
        }
        int32_t pressure = raiders->motive == CC_HOARD_RAID_WAR_FINANCE ?
            CcSimWarSupplyCrisisAtSettlement(sim, origin->id) :
            CcSimInequalityAtSettlement(sim, origin->id);
        int32_t wanted = raiders->motive == CC_HOARD_RAID_WAR_FINANCE ?
            ClampI32(18 + pressure / 4, 18, 36) :
            ClampI32(10 + pressure / 5, 12, 30);
        int32_t stolen = sim->dragon.hoard < wanted ?
                         (int32_t)sim->dragon.hoard : wanted;
        char text[CC_EVENT_TEXT_CAPACITY];
        CcId theft_parent_id = raiders->cause_event_id;
        if (stolen > 1 && sim->goblins.members >= 12 &&
            sim->goblins.devotion >= 40) {
            int32_t weapons_used =
                sim->goblins.lair_stock[CC_GOOD_WEAPONS] > 0 ? 1 : 0;
            int32_t defended = sim->goblins.members / 24 +
                               sim->goblins.devotion / 25 +
                               weapons_used * 3;
            defended = ClampI32(defended, 1, stolen - 1);
            sim->goblins.lair_stock[CC_GOOD_WEAPONS] -= weapons_used;
            sim->goblins.members = MaximumI32(
                12, sim->goblins.members - MaximumI32(1, defended / 4));
            sim->goblins.devotion = ClampI32(
                sim->goblins.devotion + 1, 0, 100);
            sim->goblins.cohesion = ClampI32(
                sim->goblins.cohesion + 2, 0, 100);
            sim->goblins.hoard_defenses += 1;
            (void)snprintf(
                text, sizeof(text),
                "%.16s fights for %.16s and keeps %d crowns beyond the raiders' reach.",
                sim->goblins.name, sim->dragon.name, defended);
            CcEvent *defense = PushEvent(
                sim, CC_EVENT_GOBLIN_HOARD_DEFENDED, sim->goblins.id,
                sim->dragon.lair_settlement_id, raiders->cause_event_id,
                defended, text);
            if (defense != NULL) theft_parent_id = defense->id;
            stolen -= defended;
        }
        if (raiders->motive == CC_HOARD_RAID_WAR_FINANCE) {
            (void)snprintf(text, sizeof(text),
                           "%.16s steals %d crowns from %.16s for soldiers at %.16s.",
                           raiders->name, stolen, sim->dragon.name, origin->name);
        } else {
            (void)snprintf(text, sizeof(text),
                           "%.16s steals %d crowns from %.16s for relief in %.16s.",
                           raiders->name, stolen, sim->dragon.name, origin->name);
        }
        CcEvent *theft = StartDragonTheft(
            sim, raiders->id, origin, stolen, theft_parent_id, text);
        if (theft == NULL) return;
        raiders->phase = CC_HOARD_RAIDERS_RETURNING;
        raiders->cause_event_id = theft->id;
        raiders->carried_treasure = stolen;
        raiders->days_remaining = GoblinTravelDays(sim, origin->id);
        return;
    }

    int32_t relief = (int32_t)raiders->carried_treasure;
    char text[CC_EVENT_TEXT_CAPACITY];
    if (raiders->motive == CC_HOARD_RAID_WAR_FINANCE) {
        CcKingdom *kingdom = KingdomMutable(sim, origin->kingdom_id);
        if (kingdom != NULL) {
            origin->war_chest += relief;
            kingdom->legitimacy = ClampI32(kingdom->legitimacy - 6, 0, 100);
        }
        (void)snprintf(text, sizeof(text),
                       "%s returns to %s and puts %d stolen crowns into its war chest.",
                       raiders->name, origin->name, relief);
        raiders->war_raids_completed += 1;
    } else {
        origin->market_coins += relief;
        origin->hunger = ClampI32(origin->hunger - MaximumI32(3, relief / 2),
                                  0, 100);
        origin->prosperity = ClampI32(origin->prosperity + 2, 0, 100);
        for (int32_t i = 0; i < sim->faction_count; ++i) {
            CcFaction *faction = &sim->factions[i];
            if (faction->kingdom_id == origin->kingdom_id &&
                faction->kind == CC_FACTION_COMMONS) {
                faction->support = ClampI32(faction->support + 10, 0, 100);
            }
        }
        (void)snprintf(text, sizeof(text),
                       "%s returns to %s and spends %d stolen crowns on bread and old debts.",
                       raiders->name, origin->name, relief);
    }
    (void)PushEvent(sim, CC_EVENT_HOARD_HEIST_RETURNED, raiders->id,
                    origin->id, raiders->cause_event_id, relief, text);
    if (raiders->motive == CC_HOARD_RAID_WAR_FINANCE) {
        raiders->war_raid_latched = true;
    } else {
        raiders->social_raid_latched = true;
    }
    raiders->raids_completed += 1;
    raiders->phase = CC_HOARD_RAIDERS_IDLE;
    raiders->motive = CC_HOARD_RAID_NO_MOTIVE;
    raiders->origin_settlement_id = 0U;
    raiders->cause_event_id = 0U;
    raiders->carried_treasure = 0;
    raiders->days_remaining = 0;
    raiders->cooldown_days = 280 + (int32_t)(NextRandom(sim) % 86U);
    CopyName(raiders->name, "The Ash-Poor Company");
}

static int32_t DragonCoinCrownScore(CcMoney coins)
{
    static const CcMoney thresholds[] = {
        1, 4, 9, 16, 25, 50, 100, 250, 500, 1000, 2000, 5000
    };
    int32_t score = 0;
    for (size_t i = 0; i < sizeof(thresholds) / sizeof(thresholds[0]); ++i) {
        if (coins < thresholds[i]) break;
        score += i < 5U ? 2 : 3;
    }
    return MinimumI32(30, score);
}

static int32_t DragonNamedTreasureScore(const CcSim *sim)
{
    int32_t score = 0;
    for (int32_t i = 0; i < sim->treasure_count; ++i) {
        const CcTreasure *treasure = &sim->treasures[i];
        if (treasure->destroyed || treasure->owner_id != sim->dragon.id) {
            continue;
        }
        score += 4 + MinimumI32(6, treasure->craft_work * 2) +
                 MinimumI32(5, treasure->gem_content) +
                 MinimumI32(5, treasure->appraised_value / 100);
    }
    return MinimumI32(25, score);
}

static int32_t CalculateDragonCrownStrength(const CcSim *sim)
{
    const CcDragon *dragon = &sim->dragon;
    int64_t mineral_value =
        (int64_t)dragon->hoard_goods[CC_GOOD_GOLD] * 2 +
        (int64_t)dragon->hoard_goods[CC_GOOD_GEMS] * 4;
    int32_t mineral_score = (int32_t)(mineral_value > 20 ? 20 :
                                      mineral_value);
    int32_t continuity_score = MinimumI32(
        15, dragon->crown_continuity_days / (20 * 365));
    int32_t devotion_score = sim->goblins.devotion * 15 / 100;
    int32_t physical_score = DragonCoinCrownScore(dragon->hoard) +
        mineral_score + DragonNamedTreasureScore(sim) + continuity_score +
        devotion_score;
    physical_score = MinimumI32(100, physical_score);
    return physical_score * dragon->memory_integrity / 100;
}

static bool DragonTerritoryAtWar(const CcSim *sim)
{
    const CcSettlement *lair = CcSimSettlement(
        sim, sim->dragon.lair_settlement_id);
    if (lair == NULL) return false;
    for (int32_t i = 0; i < sim->kingdom_count; ++i) {
        if (sim->kingdoms[i].id == lair->kingdom_id) continue;
        if (CcSimKingdomsAtWar(sim, lair->kingdom_id,
                               sim->kingdoms[i].id)) return true;
    }
    return false;
}

static CcSettlement *DragonHuntTarget(CcSim *sim)
{
    CcSettlement *best = NULL;
    int32_t best_score = INT32_MIN;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        CcSettlement *place = &sim->settlements[i];
        if (CcSettlementIsAbandoned(place) ||
            place->id == sim->dragon.lair_settlement_id ||
            (place->stock[CC_GOOD_FOOD] <= 0 &&
             place->cow_adults <= 0)) continue;
        int32_t score = place->stock[CC_GOOD_FOOD] * 3 +
                        place->cow_adults * 8 - place->security;
        if (place->function == CC_SETTLEMENT_FARMING ||
            CcSettlementHasService(place, CC_SERVICE_FARM)) score += 40;
        if (best == NULL || score > best_score) {
            best = place;
            best_score = score;
        }
    }
    return best;
}

static void DragonHunt(CcSim *sim)
{
    CcDragon *dragon = &sim->dragon;
    CcSettlement *target = DragonHuntTarget(sim);
    if (target == NULL) {
        dragon->body_condition = ClampI32(
            dragon->body_condition - 2, 0, 100);
        dragon->territory_stability = ClampI32(
            dragon->territory_stability - 1, 0, 100);
        dragon->hunt_cooldown_days = 14;
        return;
    }
    int32_t appetite = dragon->life_stage == CC_DRAGON_STAGE_WHELP ? 4 :
        dragon->life_stage == CC_DRAGON_STAGE_WANDERER ? 6 :
        dragon->life_stage == CC_DRAGON_STAGE_DEEP_WYRM ? 12 : 8;
    int32_t cows_taken = MinimumI32(
        target->cow_adults, MaximumI32(1, appetite / 4));
    target->cow_adults -= cows_taken;
    int32_t food_wanted = MaximumI32(1, appetite - cows_taken * 3);
    int32_t food_taken = MinimumI32(
        food_wanted, target->stock[CC_GOOD_FOOD]);
    target->stock[CC_GOOD_FOOD] -= food_taken;
    int32_t taken = cows_taken * 3 + food_taken;
    target->hunger = ClampI32(target->hunger + MaximumI32(1, taken / 4),
                              0, 100);
    target->prosperity = ClampI32(target->prosperity - 1, 0, 100);
    dragon->body_condition = ClampI32(
        dragon->body_condition + taken * 5, 0, 100);
    dragon->hunt_cooldown_days =
        dragon->life_stage == CC_DRAGON_STAGE_UNCROWNED ?
            14 + (int32_t)(NextRandom(sim) % 29U) :
            28 + (int32_t)(NextRandom(sim) % 57U);
    dragon->hunts += 1;
    dragon->activity = CC_DRAGON_ACTIVITY_HUNTING;
    char text[CC_EVENT_TEXT_CAPACITY];
    if (sim->schema_version >= 14U) {
        (void)snprintf(
            text, sizeof(text),
            "%s takes %d cow%s and %d Food from %s and leaves the roofs untouched.",
            dragon->name, cows_taken, cows_taken == 1 ? "" : "s",
            food_taken, target->name);
    } else {
        (void)snprintf(
            text, sizeof(text),
            "%s takes %d Food worth of livestock from %s and leaves the roofs untouched.",
            dragon->name, taken, target->name);
    }
    CcEvent *event = PushEvent(
        sim, CC_EVENT_DRAGON_HUNT, dragon->id, target->id,
        LatestLocalCause(sim, target->id), taken, text);
    dragon->lifecycle_event_id = event != NULL ? event->id :
                                 dragon->lifecycle_event_id;
}

static void HatchDragonBrood(CcSim *sim)
{
    CcDragon *dragon = &sim->dragon;
    if (dragon->egg_count <= 0) return;
    int32_t whelps = dragon->egg_count;
    dragon->egg_count = 0;
    dragon->brood_days_remaining = 0;
    dragon->whelps_dispersed += whelps;
    dragon->territory_stability = ClampI32(
        dragon->territory_stability - whelps * 3, 0, 100);
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(
        text, sizeof(text),
        "%d whelp%s leave%s %s's brood hoard to seek distant caves.",
        whelps, whelps == 1 ? "" : "s", whelps == 1 ? "s" : "",
        dragon->name);
    CcEvent *event = PushEvent(
        sim, CC_EVENT_DRAGON_WHELP_DISPERSED, dragon->id,
        dragon->lair_settlement_id, dragon->lifecycle_event_id,
        whelps, text);
    dragon->lifecycle_event_id = event != NULL ? event->id :
                                 dragon->lifecycle_event_id;
}

static void HatchDragonSuccessor(CcSim *sim)
{
    CcDragon *dragon = &sim->dragon;
    if (!dragon->slain || dragon->egg_count <= 0) return;
    int32_t clutch = dragon->egg_count;
    char former_name[CC_NAME_CAPACITY];
    CopyName(former_name, dragon->name);
    dragon->whelps_dispersed += MaximumI32(0, clutch - 1);
    dragon->id = NextId(sim, CC_ENTITY_DRAGON);
    static const char *successor_names[] = {
        "Ashwing the Inheritor", "Cinder-Child", "The Remembered Scale",
        "Ember Beneath Stone"
    };
    CopyName(dragon->name,
             successor_names[NextRandom(sim) % 4U]);
    dragon->slain = false;
    dragon->slain_day = 0;
    dragon->life_stage = CC_DRAGON_STAGE_WHELP;
    dragon->activity = CC_DRAGON_ACTIVITY_HUNTING;
    dragon->age_days = 1;
    dragon->body_condition = 64;
    dragon->memory_integrity = 45;
    dragon->territory_stability = 28;
    dragon->regional_influence = MaximumI32(24,
        dragon->regional_influence / 2);
    dragon->crown_continuity_days = 0;
    dragon->hunt_cooldown_days = 7;
    dragon->egg_count = 0;
    dragon->brood_days_remaining = 0;
    dragon->brood_cooldown_days =
        (220 + (int32_t)(NextRandom(sim) % 121U)) * 365;
    dragon->afterdeath_days = 0;
    dragon->crown_strength = CalculateDragonCrownStrength(sim);
    sim->goblins.devotion = MaximumI32(sim->goblins.devotion, 28);
    sim->goblins.dragon_seed_phase = CC_GOBLIN_DRAGON_SEED_NONE;
    sim->goblins.dragon_seed_days_remaining = 0;
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(
        text, sizeof(text),
        "%.20s hatches from %.20s's visible clutch; %d sibling%s leave with goblins.",
        dragon->name, former_name, MaximumI32(0, clutch - 1),
        clutch == 2 ? "" : "s");
    CcEvent *event = PushEvent(
        sim, CC_EVENT_DRAGON_SUCCESSOR, dragon->id,
        dragon->lair_settlement_id, dragon->lifecycle_event_id,
        clutch, text);
    dragon->lifecycle_event_id = event != NULL ? event->id : 0U;
    dragon->hoard_event_id = dragon->lifecycle_event_id;
}

static void BeginDragonBrood(CcSim *sim)
{
    CcDragon *dragon = &sim->dragon;
    dragon->egg_count = 1 + (int32_t)(NextRandom(sim) % 3U);
    dragon->brood_days_remaining =
        (7 + (int32_t)(NextRandom(sim) % 9U)) * 365;
    dragon->brood_cooldown_days =
        (180 + (int32_t)(NextRandom(sim) % 121U)) * 365;
    dragon->broods_laid += 1;
    dragon->body_condition = ClampI32(
        dragon->body_condition - 12, 0, 100);
    dragon->memory_integrity = ClampI32(
        dragon->memory_integrity - 8, 0, 100);
    dragon->territory_stability = ClampI32(
        dragon->territory_stability - 5, 0, 100);
    dragon->activity = CC_DRAGON_ACTIVITY_BROODING;
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(
        text, sizeof(text),
        "%s accepts a distant heart-scale and lays %d egg%s; goblin Ashkeepers seal the brood hoard.",
        dragon->name, dragon->egg_count,
        dragon->egg_count == 1 ? "" : "s");
    CcEvent *event = PushEvent(
        sim, CC_EVENT_DRAGON_BROOD, dragon->id,
        dragon->lair_settlement_id, dragon->hoard_event_id,
        dragon->egg_count, text);
    dragon->lifecycle_event_id = event != NULL ? event->id :
                                 dragon->lifecycle_event_id;
}

static void ChangeDragonStage(CcSim *sim, CcDragonLifeStage stage,
                              CcEventKind event_kind, const char *reason)
{
    CcDragon *dragon = &sim->dragon;
    if (dragon->life_stage == stage) return;
    dragon->life_stage = stage;
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(text, sizeof(text), "%s becomes %s: %s.",
                   dragon->name, CcDragonLifeStageName(stage), reason);
    CcEvent *event = PushEvent(
        sim, event_kind, dragon->id, dragon->lair_settlement_id,
        dragon->lifecycle_event_id, dragon->crown_strength, text);
    dragon->lifecycle_event_id = event != NULL ? event->id :
                                 dragon->lifecycle_event_id;
}

static void AdvanceAfterdragonCult(CcSim *sim)
{
    CcDragon *dragon = &sim->dragon;
    CcGoblinCult *goblins = &sim->goblins;
    if (dragon->afterdeath_days == 0 ||
        dragon->afterdeath_days % 365 != 0 ||
        goblins->tribute_phase != CC_GOBLIN_TRIBUTE_IDLE) return;

    bool provisioned = goblins->lair_stock[CC_GOOD_FOOD] >= 8;
    bool armed = goblins->lair_stock[CC_GOOD_TOOLS] >= 2 &&
                 goblins->lair_stock[CC_GOOD_WEAPONS] >= 3;
    int32_t cult_limit = goblins->cohesion < 35 ? 24 : armed ? 84 :
        goblins->devotion >= 60 ? 48 : 24;
    if (provisioned && goblins->members < cult_limit) {
        int32_t recruits = armed ? 1 + goblins->devotion / 40 : 1;
        recruits = MinimumI32(
            recruits, cult_limit - goblins->members);
        int32_t food_cost = 2 + recruits;
        if (goblins->lair_stock[CC_GOOD_FOOD] >= food_cost) {
            goblins->lair_stock[CC_GOOD_FOOD] -= food_cost;
            goblins->members += recruits;
            goblins->devotion = ClampI32(
                goblins->devotion + 2, 0, 100);
            goblins->cohesion = ClampI32(
                goblins->cohesion + 2, 0, 100);
            char text[CC_EVENT_TEXT_CAPACITY];
            (void)snprintf(
                text, sizeof(text),
                "%s feeds and binds %d new ash-sworn; the dead dragon's court reaches %d.",
                goblins->name, recruits, goblins->members);
            (void)PushEvent(
                sim, CC_EVENT_GOBLIN_CULT_RALLIED, goblins->id,
                goblins->lair_settlement_id, dragon->lifecycle_event_id,
                recruits, text);
        }
    } else if (!provisioned) {
        goblins->devotion = MaximumI32(20, goblins->devotion - 1);
        goblins->cohesion = MaximumI32(0, goblins->cohesion - 2);
    } else {
        goblins->devotion = ClampI32(goblins->devotion + 1, 0, 100);
    }

    if (dragon->egg_count != 0) return;
    if (goblins->dragon_seed_phase == CC_GOBLIN_DRAGON_SEED_NONE) {
        bool can_begin = dragon->afterdeath_days >= 100 * 365 &&
            goblins->members >= 36 && goblins->devotion >= 60 &&
            goblins->cohesion >= 50;
        if (!can_begin) return;
        goblins->dragon_seed_phase = CC_GOBLIN_DRAGON_SEED_RUMORED;
        goblins->dragon_seed_days_remaining = 20 * 365;
        char text[CC_EVENT_TEXT_CAPACITY];
        (void)snprintf(
            text, sizeof(text),
            "Nara Soot-Tongue admits that %s has opened an ash-vault; any dragon seed is at least twenty years away.",
            goblins->name);
        CcEvent *event = PushEvent(
            sim, CC_EVENT_GOBLIN_DRAGON_SEED_RUMORED, goblins->id,
            dragon->lair_settlement_id, dragon->lifecycle_event_id,
            goblins->dragon_seed_days_remaining, text);
        dragon->lifecycle_event_id = event != NULL ? event->id :
                                      dragon->lifecycle_event_id;
        return;
    }

    goblins->dragon_seed_days_remaining = MaximumI32(
        0, goblins->dragon_seed_days_remaining - 365);
    if (goblins->dragon_seed_phase == CC_GOBLIN_DRAGON_SEED_RUMORED &&
        goblins->dragon_seed_days_remaining <= 15 * 365) {
        goblins->dragon_seed_phase = CC_GOBLIN_DRAGON_SEED_PREPARING;
        char text[CC_EVENT_TEXT_CAPACITY];
        (void)snprintf(
            text, sizeof(text),
            "%s's Ashkeepers begin public work on the ash-vault; Food, Tools, Weapons, coin, and relics are still required.",
            goblins->name);
        CcEvent *event = PushEvent(
            sim, CC_EVENT_GOBLIN_DRAGON_SEED_PREPARED, goblins->id,
            dragon->lair_settlement_id, dragon->lifecycle_event_id,
            goblins->dragon_seed_days_remaining, text);
        dragon->lifecycle_event_id = event != NULL ? event->id :
                                      dragon->lifecycle_event_id;
    }
    if (goblins->dragon_seed_days_remaining > 0) return;

    int32_t relics = goblins->lair_stock[CC_GOOD_GOLD] +
                     goblins->lair_stock[CC_GOOD_GEMS];
    bool can_reveal_clutch = goblins->members >= 48 &&
        goblins->devotion >= 75 && goblins->cohesion >= 75 &&
        goblins->lair_coins >= 120 && relics >= 2 &&
        goblins->lair_stock[CC_GOOD_FOOD] >= 12 &&
        goblins->lair_stock[CC_GOOD_TOOLS] >= 2 &&
        goblins->lair_stock[CC_GOOD_WEAPONS] >= 3;
    if (!can_reveal_clutch) return;

    CcMoney ritual_coins = 120;
    goblins->lair_coins -= ritual_coins;
    dragon->hoard += ritual_coins;
    for (int32_t relic = 0; relic < 2; ++relic) {
        CcGood good = goblins->lair_stock[CC_GOOD_GEMS] > 0 ?
                      CC_GOOD_GEMS : CC_GOOD_GOLD;
        goblins->lair_stock[good] -= 1;
        dragon->hoard_goods[good] += 1;
    }
    goblins->lair_stock[CC_GOOD_FOOD] -= 12;
    goblins->lair_stock[CC_GOOD_TOOLS] -= 1;
    goblins->lair_stock[CC_GOOD_WEAPONS] -= 1;
    dragon->egg_count = goblins->members >= 72 &&
                        goblins->devotion >= 90 &&
                        goblins->cohesion >= 90 ? 2 : 1;
    dragon->brood_days_remaining =
        (10 + (int32_t)(NextRandom(sim) % 6U)) * 365;
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(
        text, sizeof(text),
        "%s opens a sealed ash-vault and reveals %d dragon egg%s, bought with stolen coin and years of sacrifice.",
        goblins->name, dragon->egg_count,
        dragon->egg_count == 1 ? "" : "s");
    CcEvent *event = PushEvent(
        sim, CC_EVENT_GOBLIN_DRAGON_SEED, goblins->id,
        dragon->lair_settlement_id, dragon->lifecycle_event_id,
        dragon->egg_count, text);
    dragon->lifecycle_event_id = event != NULL ? event->id :
                                  dragon->lifecycle_event_id;
    goblins->dragon_seed_phase = CC_GOBLIN_DRAGON_SEED_NONE;
    goblins->dragon_seed_days_remaining = 0;
}

static void AdvanceLivingDragonCult(CcSim *sim)
{
    CcDragon *dragon = &sim->dragon;
    CcGoblinCult *goblins = &sim->goblins;
    if (sim->current_day % (2 * 365) != 0 ||
        goblins->tribute_phase != CC_GOBLIN_TRIBUTE_IDLE ||
        goblins->members >= 48 ||
        goblins->lair_stock[CC_GOOD_FOOD] < 6 ||
        goblins->cohesion < 35) return;

    bool armed = goblins->lair_stock[CC_GOOD_TOOLS] >= 2 &&
                 goblins->lair_stock[CC_GOOD_WEAPONS] >= 3;
    int32_t cult_limit = armed ? 48 : 36;
    bool ash_poor_muster = !armed && goblins->devotion >= 75 &&
                           sim->current_day % (4 * 365) == 0;
    if (goblins->members >= cult_limit || (!armed && !ash_poor_muster)) {
        return;
    }
    int32_t recruits = 1;
    int32_t food_cost = 3;
    if (goblins->lair_stock[CC_GOOD_FOOD] < food_cost) return;
    goblins->lair_stock[CC_GOOD_FOOD] -= food_cost;
    goblins->members += recruits;
    goblins->cohesion = ClampI32(goblins->cohesion + 1, 0, 100);
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(
        text, sizeof(text),
        "%s gathers %d new tithe-bearers; %s's court returns to %d.",
        goblins->name, recruits, dragon->name, goblins->members);
    (void)PushEvent(
        sim, CC_EVENT_GOBLIN_CULT_RALLIED, goblins->id,
        goblins->lair_settlement_id, dragon->lifecycle_event_id,
        recruits, text);
}

static void AdvanceAfterdragon(CcSim *sim)
{
    CcDragon *dragon = &sim->dragon;
    dragon->life_stage = CC_DRAGON_STAGE_AFTERDRAGON;
    dragon->activity = CC_DRAGON_ACTIVITY_AFTERMATH;
    dragon->body_condition = 0;
    dragon->crown_strength = 0;
    dragon->afterdeath_days += 1;
    if (dragon->afterdeath_days % 90 == 0) {
        dragon->regional_influence = MaximumI32(
            0, dragon->regional_influence - 1);
    }
    if (dragon->afterdeath_days % 28 == 0) {
        dragon->memory_integrity = MaximumI32(
            0, dragon->memory_integrity - 1);
        dragon->territory_stability = MaximumI32(
            0, dragon->territory_stability - 1);
    }
    AdvanceAfterdragonCult(sim);
    if (dragon->egg_count > 0) {
        if (sim->current_day % 14 == 0) {
            if (sim->goblins.lair_stock[CC_GOOD_FOOD] > 0) {
                sim->goblins.lair_stock[CC_GOOD_FOOD] -= 1;
            } else {
                dragon->brood_days_remaining += 7;
            }
        }
        dragon->brood_days_remaining = MaximumI32(
            0, dragon->brood_days_remaining - 1);
        if (dragon->brood_days_remaining == 0) HatchDragonSuccessor(sim);
    }
    if (dragon->slain && dragon->afterdeath_days > 0 &&
        (dragon->afterdeath_days == 365 ||
         dragon->afterdeath_days % (10 * 365) == 0) &&
        dragon->regional_influence > 0) {
        char text[CC_EVENT_TEXT_CAPACITY];
        (void)snprintf(
            text, sizeof(text),
            "%s's dragon shadow cools to %d; abandoned vents, ash fields, and goblin shrines still shape the country.",
            dragon->name, dragon->regional_influence);
        CcEvent *event = PushEvent(
            sim, CC_EVENT_DRAGON_AFTERSHOCK, dragon->id,
            dragon->lair_settlement_id, dragon->lifecycle_event_id,
            dragon->regional_influence, text);
        dragon->lifecycle_event_id = event != NULL ? event->id :
                                     dragon->lifecycle_event_id;
    }
}

static void AdvanceDragonEcology(CcSim *sim)
{
    CcDragon *dragon = &sim->dragon;
    if (dragon->slain) {
        AdvanceAfterdragon(sim);
        return;
    }
    AdvanceLivingDragonCult(sim);
    dragon->age_days += 1;
    dragon->hunt_cooldown_days = MaximumI32(
        0, dragon->hunt_cooldown_days - 1);
    dragon->brood_cooldown_days = MaximumI32(
        0, dragon->brood_cooldown_days - 1);
    if (dragon->stolen_outstanding == 0) {
        dragon->crown_continuity_days += 1;
    }
    if (sim->current_day % 28 == 0) {
        if (sim->current_day % (10 * 364) == 0 &&
            dragon->stolen_outstanding == 0 && dragon->hoard > 0) {
            dragon->memory_integrity = ClampI32(
                dragon->memory_integrity + 1, 0, 100);
        }
        int32_t stability_change = 0;
        if (sim->goblins.lair_stock[CC_GOOD_FOOD] >= 4 &&
            sim->goblins.lair_stock[CC_GOOD_TOOLS] >= 1 &&
            sim->goblins.devotion >= 50 &&
            sim->goblins.cohesion >= 50) stability_change += 1;
        if (sim->goblins.lair_stock[CC_GOOD_FOOD] == 0) {
            stability_change -= 2;
        }
        if (DragonTerritoryAtWar(sim)) stability_change -= 1;
        dragon->territory_stability = ClampI32(
            dragon->territory_stability + stability_change, 0, 100);
    }
    dragon->crown_strength = CalculateDragonCrownStrength(sim);
    int32_t influence_target = dragon->life_stage ==
        CC_DRAGON_STAGE_DEEP_WYRM ? 70 + dragon->crown_strength / 3 :
        dragon->life_stage == CC_DRAGON_STAGE_CROWNED ?
            35 + dragon->crown_strength / 2 :
        dragon->life_stage == CC_DRAGON_STAGE_UNCROWNED ?
            18 + dragon->crown_strength / 3 : 15 + dragon->crown_strength / 4;
    influence_target = ClampI32(influence_target, 0, 100);
    if (sim->current_day % 28 == 0) {
        if (dragon->regional_influence < influence_target) {
            dragon->regional_influence += 1;
        } else if (dragon->regional_influence > influence_target) {
            dragon->regional_influence -= 1;
        }
    }

    if (dragon->egg_count > 0) {
        if (sim->current_day % 14 == 0) {
            if (sim->goblins.lair_stock[CC_GOOD_FOOD] > 0) {
                sim->goblins.lair_stock[CC_GOOD_FOOD] -= 1;
            } else {
                dragon->brood_days_remaining += 7;
                dragon->territory_stability = MaximumI32(
                    0, dragon->territory_stability - 1);
            }
        }
        dragon->brood_days_remaining = MaximumI32(
            0, dragon->brood_days_remaining - 1);
        if (dragon->brood_days_remaining == 0) HatchDragonBrood(sim);
    }

    bool fast_metabolism = dragon->life_stage == CC_DRAGON_STAGE_WHELP ||
                           dragon->life_stage == CC_DRAGON_STAGE_WANDERER;
    int32_t condition_interval = fast_metabolism ? 7 :
        dragon->egg_count > 0 ? 14 :
        dragon->life_stage == CC_DRAGON_STAGE_DEEP_WYRM ? 84 : 56;
    if (sim->current_day % condition_interval == 0) {
        dragon->body_condition = MaximumI32(
            0, dragon->body_condition - 1);
    }
    if (dragon->age_days >= 2000 * 365 &&
        sim->current_day % (50 * 364) == 0) {
        dragon->memory_integrity = MaximumI32(
            0, dragon->memory_integrity - 1);
    }

    if ((dragon->life_stage == CC_DRAGON_STAGE_CROWNED ||
         dragon->life_stage == CC_DRAGON_STAGE_DEEP_WYRM) &&
        (dragon->crown_strength < 12 || dragon->memory_integrity < 25)) {
        ChangeDragonStage(sim, CC_DRAGON_STAGE_UNCROWNED,
                          CC_EVENT_DRAGON_UNCROWNED,
                          "its external memory can no longer hold a crown");
    } else if ((dragon->life_stage == CC_DRAGON_STAGE_WANDERER ||
                dragon->life_stage == CC_DRAGON_STAGE_UNCROWNED) &&
               dragon->age_days >= 60 * 365 &&
               dragon->crown_strength >= 25 &&
               dragon->memory_integrity >= 60 &&
               dragon->territory_stability >= 50 &&
               sim->goblins.devotion >= 50) {
        ChangeDragonStage(sim, CC_DRAGON_STAGE_CROWNED,
                          CC_EVENT_DRAGON_CROWNED,
                          "hoard, goblin court, and territory hold together");
    } else if (dragon->life_stage == CC_DRAGON_STAGE_WHELP &&
               dragon->age_days >= 15 * 365) {
        ChangeDragonStage(sim, CC_DRAGON_STAGE_WANDERER,
                          CC_EVENT_DRAGON_CROWNED,
                          "it leaves the brood cave to seek a crown of its own");
    } else if (dragon->life_stage == CC_DRAGON_STAGE_CROWNED &&
               dragon->age_days >= 500 * 365 &&
               dragon->crown_strength >= 60 &&
               dragon->crown_continuity_days >= 200 * 365 &&
               dragon->territory_stability >= 75) {
        ChangeDragonStage(sim, CC_DRAGON_STAGE_DEEP_WYRM,
                          CC_EVENT_DRAGON_CROWNED,
                          "centuries of possession bind wyrm, hoard, and mountain");
    }

    if (dragon->brood_cooldown_days == 0 && dragon->egg_count == 0 &&
        (dragon->life_stage == CC_DRAGON_STAGE_CROWNED ||
         dragon->life_stage == CC_DRAGON_STAGE_DEEP_WYRM) &&
        dragon->body_condition >= 68 && dragon->crown_strength >= 58 &&
        dragon->territory_stability >= 60 &&
        dragon->crown_continuity_days >= 30 * 365 &&
        sim->goblins.devotion >= 65 &&
        dragon->stolen_outstanding == 0) {
        BeginDragonBrood(sim);
    }

    int32_t hunt_threshold = fast_metabolism ? 65 :
        dragon->life_stage == CC_DRAGON_STAGE_UNCROWNED ? 58 :
        dragon->life_stage == CC_DRAGON_STAGE_DEEP_WYRM ? 35 : 45;
    if (dragon->stolen_outstanding > 0) {
        dragon->activity = CC_DRAGON_ACTIVITY_RETALIATING;
    } else if (dragon->egg_count > 0) {
        dragon->activity = CC_DRAGON_ACTIVITY_BROODING;
    } else if (dragon->body_condition <= hunt_threshold &&
               dragon->hunt_cooldown_days == 0) {
        DragonHunt(sim);
    } else {
        dragon->activity = CC_DRAGON_ACTIVITY_DORMANT;
    }
}

static void RemoveBurnedService(CcSettlement *target)
{
    static const CcServiceKind order[] = {
        CC_SERVICE_MARKET, CC_SERVICE_GUILDHALL, CC_SERVICE_GRANARY,
        CC_SERVICE_CARTOGRAPHER, CC_SERVICE_STABLE, CC_SERVICE_SMITHY,
        CC_SERVICE_INN
    };
    if (CcSettlementServiceCount(target) <= 1) return;
    for (size_t i = 0; i < sizeof(order) / sizeof(order[0]); ++i) {
        if (CcSettlementHasService(target, order[i])) {
            target->service_mask &= ~ServiceBit(order[i]);
            return;
        }
    }
}

static CcHoardRaidMotive DragonAutomaticTheftMotive(const CcSim *sim)
{
    const CcEvent *event = CcSimEvent(sim, sim->dragon.hoard_event_id);
    for (int32_t depth = 0; event != NULL && depth < 5; ++depth) {
        if (event->kind == CC_EVENT_INEQUALITY_PRESSURE) {
            return CC_HOARD_RAID_SOCIAL_RELIEF;
        }
        if (event->kind == CC_EVENT_WAR_PRESSURE) {
            return CC_HOARD_RAID_WAR_FINANCE;
        }
        event = CcSimEvent(sim, event->parent_id);
    }
    return CC_HOARD_RAID_NO_MOTIVE;
}

static void AdvanceDragonRetaliation(CcSim *sim)
{
    CcDragon *dragon = &sim->dragon;
    if (dragon->stolen_outstanding <= 0 || dragon->slain) return;
    dragon->omen_days_remaining = MaximumI32(
        0, dragon->omen_days_remaining - 1);
    if (dragon->omen_days_remaining > 0) return;
    CcSettlement *target = CcSimSettlementMutable(
        sim, dragon->retaliation_target_id);
    if (target == NULL) target = RichestDragonTarget(sim);
    if (target == NULL) return;

    int32_t population_loss = MaximumI32(100, target->population / 10);
    target->population = MaximumI32(100, target->population - population_loss);
    target->prosperity = ClampI32(target->prosperity - 28, 0, 100);
    target->security = ClampI32(target->security - 20, 0, 100);
    target->hunger = ClampI32(target->hunger + 10, 0, 100);
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        target->stock[good] -= target->stock[good] / 3;
    }
    target->service_project = CC_SERVICE_NONE;
    target->service_project_days = 0;
    RemoveBurnedService(target);
    for (int32_t i = 0; i < sim->route_count; ++i) {
        CcRoute *route = &sim->routes[i];
        if (route->from_id != target->id && route->to_id != target->id) continue;
        route->condition = ClampI32(route->condition - 16, 0, 100);
        if (route->condition < 18) route->closed = true;
    }
    CcKingdom *kingdom = KingdomMutable(sim, target->kingdom_id);
    if (kingdom != NULL) {
        kingdom->legitimacy = ClampI32(kingdom->legitimacy - 8, 0, 100);
    }
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(text, sizeof(text),
                   "%s burns %s because %" PRId64 " stolen crowns remain missing.",
                   dragon->name, target->name, dragon->stolen_outstanding);
    CcEvent *fire = PushEvent(
        sim, CC_EVENT_DRAGON_RETALIATION, dragon->id, target->id,
        dragon->omen_event_id, 28, text);
    dragon->retaliations += 1;

    if (dragon->theft_actor_id == sim->hoard_raiders.id &&
        kingdom != NULL) {
        CcMoney debt = dragon->stolen_outstanding;
        CcMoney remaining = debt;
        CcHoardRaidMotive motive = DragonAutomaticTheftMotive(sim);
        CcMoney chest_payment = 0;
        if (motive == CC_HOARD_RAID_WAR_FINANCE) {
            chest_payment = target->war_chest < remaining ?
                            target->war_chest : remaining;
            target->war_chest -= chest_payment;
            remaining -= chest_payment;
        }
        CcMoney treasury_payment = kingdom->treasury < remaining ?
                                   kingdom->treasury : remaining;
        kingdom->treasury -= treasury_payment;
        remaining -= treasury_payment;
        CcMoney household_levy = 0;
        for (int32_t pass = 0; pass < 2 && remaining > 0; ++pass) {
            for (int32_t i = 0; i < sim->settlement_count && remaining > 0; ++i) {
                CcSettlement *place = &sim->settlements[i];
                if (place->kingdom_id != kingdom->id) continue;
                bool is_target = place->id == target->id;
                if ((pass == 0 && is_target) || (pass == 1 && !is_target)) continue;
                CcMoney taken = place->market_coins < remaining ?
                                place->market_coins : remaining;
                place->market_coins -= taken;
                household_levy += taken;
                remaining -= taken;
            }
        }
        CcMoney payment = debt - remaining;
        dragon->hoard += payment;
        dragon->stolen_outstanding -= payment;
        int32_t healing = payment > 0 ?
            1 + (int32_t)((payment * 40) / MaximumI32(
                1, debt > INT32_MAX ? INT32_MAX : (int32_t)debt)) : 0;
        dragon->memory_integrity = ClampI32(
            dragon->memory_integrity + healing, 0, 100);
        if (household_levy > 0) {
            (void)snprintf(text, sizeof(text),
                           "After %.12s burns, %.12s pays %d to %.12s "
                           "(%d chest, %d crown, %d levy).",
                           target->name, kingdom->name, (int32_t)payment,
                           dragon->name, (int32_t)chest_payment,
                           (int32_t)treasury_payment,
                           (int32_t)household_levy);
        } else {
            (void)snprintf(text, sizeof(text),
                           "After %.16s burns, %.16s pays %d crowns to %.16s from war funds.",
                           target->name, kingdom->name, (int32_t)payment,
                           dragon->name);
        }
        CcEvent *restitution = PushEvent(
            sim, CC_EVENT_DRAGON_TREASURE_RETURNED, kingdom->id,
            dragon->lair_settlement_id,
            fire != NULL ? fire->id : 0U, (int32_t)payment, text);
        dragon->hoard_event_id = restitution != NULL ? restitution->id :
                                 dragon->hoard_event_id;
        if (dragon->stolen_outstanding == 0) {
            dragon->memory_integrity = MaximumI32(
                dragon->memory_integrity, 75);
            dragon->theft_actor_id = 0U;
            dragon->retaliation_target_id = 0U;
            dragon->omen_event_id = 0U;
            dragon->omen_days_remaining = 0;
            return;
        }
    }

    target = RichestDragonTarget(sim);
    dragon->retaliation_target_id = target != NULL ? target->id : 0U;
    dragon->omen_days_remaining = 28;
    if (target != NULL) {
        (void)snprintf(text, sizeof(text),
                       "The wells of %s steam at dawn; old readers count 28 nights until %s comes.",
                       target->name, dragon->name);
        CcEvent *omen = PushEvent(
            sim, CC_EVENT_DRAGON_OMEN, dragon->id, target->id,
            fire != NULL ? fire->id : 0U, 28, text);
        dragon->omen_event_id = omen != NULL ? omen->id :
                                dragon->omen_event_id;
    }
}

static int32_t DragonRouteShadowDanger(const CcSim *sim,
                                       const CcRoute *route)
{
    if (sim == NULL || route == NULL ||
        (route->from_id != sim->dragon.lair_settlement_id &&
         route->to_id != sim->dragon.lair_settlement_id)) return 0;
    int32_t influence = sim->dragon.regional_influence;
    return sim->dragon.slain ? (influence >= 60 ? 1 : 0) :
           influence >= 80 ? 2 : influence >= 50 ? 1 : 0;
}

int32_t CcSimRouteDanger(const CcSim *sim, CcId route_id)
{
    const CcRoute *route = CcSimRoute(sim, route_id);
    if (route == NULL) return 100;
    int32_t danger = (100 - route->security) / 4 +
                     (100 - route->condition) / 8 +
                     (route->smuggler_route ? 5 : 0) +
                     (route->closed ? 8 : 0);
    for (int32_t i = 0; i < sim->bandit_count; ++i) {
        if (sim->bandits[i].route_id == route_id) danger += sim->bandits[i].influence / 3;
    }
    danger += MonsterPressureAtSettlement(sim, route->from_id) / 10;
    danger += MonsterPressureAtSettlement(sim, route->to_id) / 10;
    return ClampI32(danger, 0, 95);
}

int32_t CcSimDragonBattleStrength(const CcSim *sim)
{
    if (sim == NULL || sim->dragon.slain) return 0;
    const CcDragon *dragon = &sim->dragon;
    int32_t stage_strength = 0;
    switch (dragon->life_stage) {
        case CC_DRAGON_STAGE_EGG: stage_strength = 8; break;
        case CC_DRAGON_STAGE_WHELP: stage_strength = 28; break;
        case CC_DRAGON_STAGE_WANDERER: stage_strength = 40; break;
        case CC_DRAGON_STAGE_CROWNED: stage_strength = 55; break;
        case CC_DRAGON_STAGE_DEEP_WYRM: stage_strength = 70; break;
        case CC_DRAGON_STAGE_UNCROWNED: stage_strength = 36; break;
        case CC_DRAGON_STAGE_AFTERDRAGON: return 0;
    }
    return stage_strength + dragon->body_condition / 10 +
           dragon->crown_strength / 12 +
           dragon->territory_stability / 20 +
           dragon->memory_integrity / 25;
}

static CcMoney TradeRouteToll(const CcSim *sim, const CcRoute *route)
{
    if (sim == NULL || route == NULL) return 0;
    CcMoney toll = route->closed ? 4 : 0;
    if (route->smuggler_route) {
        toll += 2;
    } else if (CcSimRouteCrossesWarBorder(sim, route->id)) {
        toll += 4;
    }
    return toll;
}

static int32_t TradeRouteCapacity(const CcSim *sim, const CcRoute *route)
{
    if (sim == NULL || route == NULL) return 0;
    int32_t capacity = MaximumI32(
        3, route->capacity * MaximumI32(25, route->condition) / 100);
    if (route->closed) capacity = MaximumI32(1, capacity / 2);
    if (CcSimRouteCrossesWarBorder(sim, route->id) &&
        !route->smuggler_route) {
        capacity = MaximumI32(1, capacity / 2);
    }
    return capacity;
}

static int32_t SettlementSlotById(const CcSim *sim, CcId id)
{
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        if (sim->settlements[i].id == id) return i;
    }
    return -1;
}

static bool FindTradePath(const CcSim *sim, CcId from_id, CcId to_id,
                          CcGood good,
                          int32_t *first_route_slot, CcId *first_hop_id,
                          int32_t *total_cost,
                          const int32_t route_used[CC_MAX_ROUTES])
{
    int32_t source = SettlementSlotById(sim, from_id);
    int32_t target = SettlementSlotById(sim, to_id);
    if (source < 0 || target < 0 || source == target ||
        CcSettlementIsAbandoned(&sim->settlements[source]) ||
        CcSettlementIsAbandoned(&sim->settlements[target])) return false;
    (void)good;
    int32_t distance[CC_MAX_SETTLEMENTS];
    int32_t first_route[CC_MAX_SETTLEMENTS];
    CcId first_hop[CC_MAX_SETTLEMENTS];
    bool visited[CC_MAX_SETTLEMENTS];
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        distance[i] = INT_MAX;
        first_route[i] = -1;
        first_hop[i] = 0U;
        visited[i] = false;
    }
    distance[source] = 0;
    for (int32_t iteration = 0; iteration < sim->settlement_count; ++iteration) {
        int32_t current = -1;
        for (int32_t i = 0; i < sim->settlement_count; ++i) {
            if (!visited[i] && distance[i] < INT_MAX &&
                (current < 0 || distance[i] < distance[current])) current = i;
        }
        if (current < 0) break;
        if (current == target) break;
        visited[current] = true;
        CcId current_id = sim->settlements[current].id;
        for (int32_t route_slot = 0; route_slot < sim->route_count; ++route_slot) {
            const CcRoute *route = &sim->routes[route_slot];
            bool war_blockade = CcSimRouteCrossesWarBorder(sim, route->id) &&
                                !route->smuggler_route;
            int32_t effective_capacity = TradeRouteCapacity(sim, route);
            if (route_used != NULL && route_used[route_slot] >= effective_capacity) continue;
            CcId neighbor_id = route->from_id == current_id ? route->to_id :
                               route->to_id == current_id ? route->from_id : 0U;
            int32_t neighbor = SettlementSlotById(sim, neighbor_id);
            if (neighbor < 0 || visited[neighbor] ||
                CcSettlementIsAbandoned(&sim->settlements[neighbor])) continue;
            int32_t edge_cost = route->travel_days * 10 +
                                CcSimRouteDanger(sim, route->id) +
                                (war_blockade ? 25 : 0);
            if (distance[current] > INT_MAX - edge_cost) continue;
            int32_t candidate = distance[current] + edge_cost;
            if (candidate >= distance[neighbor]) continue;
            distance[neighbor] = candidate;
            first_route[neighbor] = current == source ? route_slot : first_route[current];
            first_hop[neighbor] = current == source ? neighbor_id : first_hop[current];
        }
    }
    if (distance[target] == INT_MAX || first_route[target] < 0 || first_hop[target] == 0U) {
        return false;
    }
    if (first_route_slot != NULL) *first_route_slot = first_route[target];
    if (first_hop_id != NULL) *first_hop_id = first_hop[target];
    if (total_cost != NULL) *total_cost = distance[target];
    return true;
}

static void UpdateShipments(CcSim *sim)
{
    for (int32_t i = 0; i < sim->shipment_count; ++i) {
        CcShipment *shipment = &sim->shipments[i];
        if (shipment->status != CC_SHIPMENT_TRAVELLING ||
            shipment->arrival_day > sim->current_day) continue;
        if (shipment->good < CC_GOOD_FOOD ||
            shipment->good >= CC_GOOD_COUNT) {
            shipment->status = CC_SHIPMENT_LOST;
            continue;
        }
        CcGood shipment_good = shipment->good;
        CcId final_id = shipment->final_destination_id != 0U ?
                        shipment->final_destination_id : shipment->destination_id;
        CcBanditGroup *bandits = BanditsOnRoute(sim, shipment->route_id);
        int32_t danger = CcSimRouteDanger(sim, shipment->route_id);
        if (CcSimRouteCrossesWarBorder(sim, shipment->route_id) &&
            (CcSimRoute(sim, shipment->route_id) == NULL ||
             !CcSimRoute(sim, shipment->route_id)->smuggler_route)) {
            danger = MinimumI32(85, danger + 25);
        }
        const CcSettlement *final_destination = CcSimSettlement(
            sim, final_id);
        if (shipment_good == CC_GOOD_FOOD &&
            final_destination != NULL && final_destination->hunger >= 40) {
            int32_t relief_escort = MinimumI32(
                24, 8 + (final_destination->hunger - 40) / 3);
            danger = MaximumI32(5, danger - relief_escort);
        }
        bool lost = (int32_t)(NextRandom(sim) % 100U) < danger;
        const CcEvent *departure = LatestEvent(sim, CC_EVENT_SHIPMENT_DEPARTED,
                                               shipment->id, 0U);
        if (lost) {
            shipment->status = CC_SHIPMENT_LOST;
            if (bandits != NULL) {
                bandits->supplies = ClampI32(bandits->supplies + shipment->quantity, 0, 100);
            }
            char text[CC_EVENT_TEXT_CAPACITY];
            (void)snprintf(text, sizeof(text),
                           "%d %s bound for %s vanish on a road with %d%% danger.",
                           shipment->quantity, CcGoodName(shipment_good),
                           final_destination != NULL ? final_destination->name :
                           "the frontier", danger);
            (void)PushEvent(sim, CC_EVENT_SHIPMENT_LOST, shipment->id,
                            final_id,
                            departure != NULL ? departure->id : 0U,
                            shipment->quantity, text);
            continue;
        }
        CcSettlement *hop = CcSimSettlementMutable(sim, shipment->destination_id);
        if (shipment->destination_id != final_id && hop != NULL) {
            int32_t local_need = EffectiveReserveTarget(
                                     sim, hop, shipment_good) -
                                 hop->stock[shipment_good] -
                                 CcSimIncomingGood(sim, hop->id, shipment_good);
            int32_t unload = MinimumI32(local_need, shipment->quantity / 3);
            unload = MinimumI32(unload, shipment->quantity - 4);
            unload = MaximumI32(0, unload);
            if (unload > 0) {
                hop->stock[shipment_good] += unload;
                hop->prosperity = ClampI32(hop->prosperity + 1, 0, 100);
                shipment->quantity -= unload;
            }
            int32_t next_route_slot = -1;
            CcId next_hop_id = 0U;
            if (FindTradePath(sim, hop->id, final_id, shipment_good,
                              &next_route_slot,
                              &next_hop_id, NULL, NULL)) {
                CcRoute *next_route = &sim->routes[next_route_slot];
                shipment->origin_id = hop->id;
                shipment->destination_id = next_hop_id;
                shipment->route_id = next_route->id;
                shipment->departure_day = sim->current_day;
                shipment->arrival_day = sim->current_day + next_route->travel_days;
                if (next_route->smuggler_route || sim->current_day % 21 == 0) {
                    next_route->condition = ClampI32(next_route->condition - 1, 0, 100);
                }
                const CcSettlement *final = CcSimSettlement(sim, final_id);
                char text[CC_EVENT_TEXT_CAPACITY];
                (void)snprintf(text, sizeof(text),
                               "%s unloads %d and transfers %d %s onward toward %s.",
                               hop->name, unload,
                               shipment->quantity, CcGoodName(shipment_good),
                               final != NULL ? final->name : "the frontier");
                (void)PushEvent(sim, CC_EVENT_SHIPMENT_DEPARTED, shipment->id,
                                hop->id, departure != NULL ? departure->id : 0U,
                                shipment->quantity, text);
                continue;
            }
            hop->stock[shipment_good] += shipment->quantity;
            shipment->status = CC_SHIPMENT_ARRIVED;
            char text[CC_EVENT_TEXT_CAPACITY];
            (void)snprintf(text, sizeof(text),
                           "%d %s become stranded at %s after their route changes.",
                           shipment->quantity, CcGoodName(shipment_good), hop->name);
            (void)PushEvent(sim, CC_EVENT_SHIPMENT_ARRIVED, shipment->id, hop->id,
                            departure != NULL ? departure->id : 0U,
                            shipment->quantity, text);
            continue;
        }
        CcSettlement *destination = CcSimSettlementMutable(sim, final_id);
        if (destination != NULL) {
            destination->stock[shipment_good] += shipment->quantity;
            destination->prosperity = ClampI32(destination->prosperity + 1, 0, 100);
        }
        shipment->status = CC_SHIPMENT_ARRIVED;
        char text[CC_EVENT_TEXT_CAPACITY];
        (void)snprintf(text, sizeof(text), "%d units of %s reach %s.",
                       shipment->quantity, CcGoodName(shipment_good),
                       destination != NULL ? destination->name : "their destination");
        (void)PushEvent(sim, CC_EVENT_SHIPMENT_ARRIVED, shipment->id,
                        final_id,
                        departure != NULL ? departure->id : 0U,
                        shipment->quantity, text);
    }
}

static CcShipment *AllocateShipment(CcSim *sim)
{
    for (int32_t i = 0; i < sim->shipment_count; ++i) {
        if (sim->shipments[i].status != CC_SHIPMENT_TRAVELLING) {
            sim->shipments[i] = (CcShipment){0};
            return &sim->shipments[i];
        }
    }
    if (sim->shipment_count >= CC_MAX_SHIPMENTS) return NULL;
    CcShipment *shipment = &sim->shipments[sim->shipment_count];
    sim->shipment_count += 1;
    return shipment;
}

static int32_t TradeSurplus(const CcSim *sim,
                            const CcSettlement *origin,
                            const CcSettlement *destination,
                            CcGood good)
{
    if (sim == NULL || origin == NULL || destination == NULL ||
        CcSettlementIsAbandoned(origin) ||
        CcSettlementIsAbandoned(destination) ||
        good < 0 || good >= CC_GOOD_COUNT) return 0;
    int32_t protected_stock = origin->reserve_target[good];
    if (good == CC_GOOD_FOOD && destination->hunger >= 65 &&
        origin->hunger < 35) {
        int32_t survival_stock = MaximumI32(
            WeeklyFoodUse(sim, origin) * 6,
            origin->reserve_target[CC_GOOD_FOOD] / 2);
        protected_stock = MinimumI32(protected_stock, survival_stock);
    }
    return origin->stock[good] - protected_stock;
}

static void CreateTradeShipment(CcSim *sim, int32_t route_slot, CcId next_hop_id,
                                CcGood good, CcSettlement *origin,
                                CcSettlement *final_destination,
                                int32_t route_used[CC_MAX_ROUTES])
{
    CcRoute *route = &sim->routes[route_slot];
    int32_t surplus = TradeSurplus(
        sim, origin, final_destination, good);
    int32_t incoming = CcSimIncomingGood(sim, final_destination->id, good);
    int32_t need = EffectiveReserveTarget(sim, final_destination, good) -
                   final_destination->stock[good] - incoming;
    int32_t effective_capacity = TradeRouteCapacity(sim, route);
    int32_t available_capacity = effective_capacity - route_used[route_slot];
    int32_t cargo_capacity = available_capacity * GoodUnitsPerCargoSlot(good);
    int32_t quantity = MinimumI32(
        5 * GoodUnitsPerCargoSlot(good), MinimumI32(cargo_capacity,
                                                 MinimumI32(surplus, need)));
    bool military_supply = WarWeeklyNeed(
        sim, final_destination, good) > 0;
    CcMoney *buyer_coins = military_supply ?
                           &final_destination->war_chest :
                           &final_destination->market_coins;
    CcKingdom *buyer_kingdom = KingdomMutable(
        sim, final_destination->kingdom_id);
    bool essential_credit = IronLedgerWillFund(final_destination, good);
    int32_t unit_price = MaximumI32(1, origin->price[good]);
    CcMoney toll = TradeRouteToll(sim, route);
    CcMoney credit_available = essential_credit ?
        IronLedgerCreditAvailable(sim, buyer_kingdom) : 0;
    CcMoney purchasing_power = *buyer_coins + credit_available;
    if (purchasing_power <= toll) return;
    int32_t affordable = (purchasing_power - toll) / unit_price > INT32_MAX ?
                         INT32_MAX :
                         (int32_t)((purchasing_power - toll) / unit_price);
    quantity = MinimumI32(quantity, affordable);
    int32_t minimum_load = good >= CC_GOOD_GOLD ? 1 :
                           good >= CC_GOOD_TOOLS ? 1 : 4;
    if (quantity < minimum_load) return;
    CcShipment *shipment = AllocateShipment(sim);
    if (shipment == NULL) return;
    origin->stock[good] -= quantity;
    CcMoney payment = (CcMoney)quantity * unit_price;
    CcMoney total_charge = payment + toll;
    CcMoney cash_payment = MinimumI32(
        total_charge > INT32_MAX ? INT32_MAX : (int32_t)total_charge,
        *buyer_coins > INT32_MAX ? INT32_MAX : (int32_t)*buyer_coins);
    CcMoney credit_payment = total_charge - cash_payment;
    *buyer_coins -= cash_payment;
    if (credit_payment > 0 && buyer_kingdom != NULL) {
        sim->iron_ledger_reserve -= credit_payment;
        buyer_kingdom->iron_ledger_debt += credit_payment;
        char loan_text[CC_EVENT_TEXT_CAPACITY];
        (void)snprintf(
            loan_text, sizeof(loan_text),
            "Copied monastery books lend %.16s %d crowns for %s at %.16s.",
            buyer_kingdom->name, (int32_t)credit_payment,
            CcGoodName(good), final_destination->name);
        (void)PushEvent(
            sim, CC_EVENT_IRON_LEDGER_LOAN, buyer_kingdom->id,
            final_destination->id,
            LatestLocalCause(sim, final_destination->id),
            (int32_t)credit_payment, loan_text);
    }
    origin->market_coins += payment;
    const CcSettlement *toll_gate = CcSimSettlement(sim, next_hop_id);
    CcKingdom *toll_kingdom = toll_gate != NULL ?
        KingdomMutable(sim, toll_gate->kingdom_id) : NULL;
    if (toll_kingdom != NULL) toll_kingdom->treasury += toll;
    shipment->id = NextId(sim, CC_ENTITY_SHIPMENT);
    shipment->origin_id = origin->id;
    shipment->destination_id = next_hop_id;
    shipment->final_destination_id = final_destination->id;
    shipment->route_id = route->id;
    shipment->good = good;
    shipment->quantity = quantity;
    shipment->departure_day = sim->current_day;
    shipment->arrival_day = sim->current_day + route->travel_days;
    shipment->status = CC_SHIPMENT_TRAVELLING;
    route_used[route_slot] += GoodCargoSlots(good, quantity);
    if (route->smuggler_route || sim->current_day % 21 == 0) {
        route->condition = ClampI32(route->condition - 1, 0, 100);
    }
    char text[CC_EVENT_TEXT_CAPACITY];
    const CcSettlement *next_hop = CcSimSettlement(sim, next_hop_id);
    CcEvent *purchase = NULL;
    if (military_supply) {
        (void)snprintf(text, sizeof(text),
                       "%.16s war chest pays %" PRId64
                       " crowns plus %d in road dues to %.16s for %d %.8s.",
                       final_destination->name, payment, (int32_t)toll,
                       origin->name,
                       quantity, CcGoodName(good));
        purchase = PushEvent(
            sim, CC_EVENT_WAR_SUPPLY_BOUGHT, final_destination->id,
            origin->id, LatestLocalCause(sim, final_destination->id),
            quantity, text);
    }
    (void)snprintf(text, sizeof(text),
                   "%.16s sends %d %.8s to %.16s via %.16s; paid %" PRId64
                   " plus %d in tolls and sanctions.",
                   origin->name, quantity, CcGoodName(good),
                   final_destination->name,
                   next_hop != NULL ? next_hop->name : "the road", payment,
                   (int32_t)toll);
    const CcEvent *need_event = LatestEvent(sim, CC_EVENT_SHORTAGE,
                                           final_destination->id,
                                           final_destination->id);
    (void)PushEvent(sim, CC_EVENT_SHIPMENT_DEPARTED, shipment->id,
                    origin->id, purchase != NULL ? purchase->id :
                    need_event != NULL ? need_event->id :
                    LatestLocalCause(sim, final_destination->id),
                    quantity, text);
}

static void PlanTrade(CcSim *sim)
{
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        int32_t minimum_load = good >= CC_GOOD_GOLD ? 1 :
                               good >= CC_GOOD_TOOLS ? 1 : 4;
        int32_t route_used[CC_MAX_ROUTES] = {0};
        for (int32_t transfer = 0; transfer < sim->settlement_count * 2; ++transfer) {
            int32_t best_score = 0;
            int32_t best_source = -1;
            int32_t best_destination = -1;
            int32_t best_route = -1;
            CcId best_hop = 0U;
            for (int32_t destination = 0; destination < sim->settlement_count; ++destination) {
                CcSettlement *to = &sim->settlements[destination];
                if (CcSettlementIsAbandoned(to)) continue;
                int32_t need = EffectiveReserveTarget(
                                   sim, to, (CcGood)good) - to->stock[good] -
                               CcSimIncomingGood(sim, to->id, (CcGood)good);
                if (need < minimum_load) continue;
                for (int32_t source = 0; source < sim->settlement_count; ++source) {
                    if (source == destination) continue;
                    CcSettlement *from = &sim->settlements[source];
                    if (CcSettlementIsAbandoned(from)) continue;
                    int32_t surplus = TradeSurplus(
                        sim, from, to, (CcGood)good);
                    if (surplus < minimum_load) continue;
                    int32_t route_slot = -1;
                    CcId next_hop = 0U;
                    int32_t path_cost = 0;
                    if (!FindTradePath(sim, from->id, to->id,
                                       (CcGood)good, &route_slot,
                                       &next_hop, &path_cost, route_used)) continue;
                    bool military_supply = WarWeeklyNeed(
                        sim, to, (CcGood)good) > 0;
                    CcMoney buyer_coins = military_supply ?
                        to->war_chest : to->market_coins;
                    CcKingdom *buyer_kingdom = KingdomMutable(
                        sim, to->kingdom_id);
                    bool essential_credit = IronLedgerWillFund(
                        to, (CcGood)good);
                    CcMoney credit_available = essential_credit ?
                        IronLedgerCreditAvailable(sim, buyer_kingdom) : 0;
                    CcMoney minimum_cost =
                        (CcMoney)minimum_load * MaximumI32(1, from->price[good]) +
                        TradeRouteToll(sim, &sim->routes[route_slot]);
                    if (buyer_coins + credit_available < minimum_cost) continue;
                    int32_t score = need * 3 + surplus +
                                    ((CcGood)good == CC_GOOD_FOOD ? to->hunger * 2 : 0) -
                                    path_cost / 3;
                    if (score > best_score) {
                        best_score = score;
                        best_source = source;
                        best_destination = destination;
                        best_route = route_slot;
                        best_hop = next_hop;
                    }
                }
            }
            if (best_source < 0 || best_destination < 0 || best_route < 0) break;
            CreateTradeShipment(sim, best_route, best_hop, (CcGood)good,
                                &sim->settlements[best_source],
                                &sim->settlements[best_destination], route_used);
        }
    }
}

static CcFaction *FactionFor(CcSim *sim, CcId kingdom_id, CcFactionKind kind)
{
    for (int32_t i = 0; i < sim->faction_count; ++i) {
        if (sim->factions[i].kingdom_id == kingdom_id && sim->factions[i].kind == kind) {
            return &sim->factions[i];
        }
    }
    return NULL;
}

static CcFaction *FactionByIdMutable(CcSim *sim, CcId faction_id)
{
    if (sim == NULL || faction_id == 0U) return NULL;
    for (int32_t i = 0; i < sim->faction_count; ++i) {
        if (sim->factions[i].id == faction_id) return &sim->factions[i];
    }
    return NULL;
}

static bool PrimaryCrisisSettled(const CcSim *sim)
{
    if (sim == NULL) return false;
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        const CcSituation *situation = &sim->situations[i];
        bool primary = situation->kind == CC_SITUATION_RELIEF_DELIVERY ||
                       situation->kind == CC_SITUATION_ROUTE_REPAIR ||
                       situation->kind == CC_SITUATION_BLACK_MARKET_DELIVERY;
        if (primary && situation->status == CC_SITUATION_RESOLVED) return true;
    }
    return false;
}

static int32_t SituationSettlementSlot(const CcSim *sim,
                                       CcSituationKind kind, CcId target)
{
    CcId settlement_id = 0U;
    if (kind == CC_SITUATION_RELIEF_DELIVERY ||
        kind == CC_SITUATION_BLACK_MARKET_DELIVERY) {
        settlement_id = target;
    } else if (kind == CC_SITUATION_ROUTE_REPAIR) {
        const CcRoute *route = CcSimRoute(sim, target);
        if (route != NULL) settlement_id = route->to_id;
    } else if (kind == CC_SITUATION_MONSTER_EXPEDITION) {
        for (int32_t i = 0; i < sim->dungeon_count; ++i) {
            if (sim->dungeons[i].id == target) {
                settlement_id = sim->dungeons[i].settlement_id;
                break;
            }
        }
    } else if (kind == CC_SITUATION_COURIER_DELIVERY) {
        const CcCourier *courier = CourierMutable((CcSim *)sim, target);
        if (courier != NULL) settlement_id = courier->current_settlement_id;
    }
    return SettlementSlotById(sim, settlement_id);
}

static CcCharacter *CharacterForNameAt(CcSim *sim, const char *name,
                                       CcId settlement_id)
{
    if (sim == NULL || name == NULL || name[0] == '\0') return NULL;
    for (int32_t i = 0; i < sim->character_count; ++i) {
        CcCharacter *character = &sim->characters[i];
        if (character->home_settlement_id == settlement_id &&
            strcmp(character->name, name) == 0) return character;
    }
    return NULL;
}

static CcCharacter *PromoteCharacter(CcSim *sim, const char *name,
                                     CcId settlement_id, CcId faction_id,
                                     CcCharacterRole role,
                                     CcCharacterGoal goal,
                                     CcCharacterActivity activity)
{
    CcCharacter *character = CharacterForNameAt(sim, name, settlement_id);
    if (character != NULL) return character;
    if (sim == NULL || sim->character_count >= CC_MAX_CHARACTERS ||
        CcSimSettlement(sim, settlement_id) == NULL) return NULL;
    character = &sim->characters[sim->character_count++];
    *character = (CcCharacter){0};
    character->id = NextId(sim, CC_ENTITY_CHARACTER);
    CopyName(character->name, name);
    character->home_settlement_id = settlement_id;
    character->current_settlement_id = settlement_id;
    character->faction_id = faction_id;
    character->role = role;
    character->goal = goal;
    character->activity = activity;
    character->appearance_seed = (uint32_t)(
        character->id ^ (character->id >> 32) ^ sim->world_seed ^
        UINT32_C(0x9e3779b9));
    character->player_disposition = 0;
    character->stress = activity == CC_CHARACTER_ACTIVITY_SEEKING_AID ?
        68 : 28;
    character->courage = 38 + (int32_t)(character->appearance_seed % 41U);
    return character;
}

static void RefreshSituationCharacterActivities(CcSim *sim,
                                                CcSituation *situation)
{
    if (sim == NULL || situation == NULL) return;
    CcCharacter *sponsor = CharacterMutable(
        sim, situation->sponsor_character_id);
    CcCharacter *affected = CharacterMutable(
        sim, situation->affected_character_id);
    if (sponsor != NULL) sponsor->activity = CC_CHARACTER_ACTIVITY_WORKING;
    if (affected == NULL) return;
    if (situation->status == CC_SITUATION_RESOLVED) {
        affected->activity = CC_CHARACTER_ACTIVITY_RECOVERING;
        affected->stress = ClampI32(affected->stress - 30, 0, 100);
    } else if (situation->status == CC_SITUATION_FAILED) {
        affected->activity = CC_CHARACTER_ACTIVITY_HIDING;
        affected->stress = ClampI32(affected->stress + 12, 0, 100);
    } else if (sim->player.accepted_situation_id == situation->id) {
        affected->activity = CC_CHARACTER_ACTIVITY_PREPARING;
    } else {
        affected->activity = CC_CHARACTER_ACTIVITY_SEEKING_AID;
    }
}

static void AssignSituationCast(CcSim *sim, CcSituation *situation)
{
    static const char *sponsors[CC_MAX_SETTLEMENTS] = {
        "Mara Venn", "Tomas Rill", "Ilyra Senn",
        "Orren Vale", "Sabine Holt", "Pavel Drost"
    };
    static const char *affected[CC_MAX_SETTLEMENTS] = {
        "Nell Varo", "Brin Alder", "Cera Mott",
        "Jory Fen", "Anja Reed", "Mikel Thorne"
    };
    int32_t slot = SituationSettlementSlot(
        sim, situation->kind, situation->target_id);
    if (slot < 0 || slot >= CC_MAX_SETTLEMENTS) slot = 0;
    if (situation->sponsor_name[0] == '\0') {
        int32_t sponsor_slot = situation->kind ==
                CC_SITUATION_RELIEF_DELIVERY ? 0 :
            situation->kind == CC_SITUATION_BLACK_MARKET_DELIVERY ? 1 : slot;
        CopyName(situation->sponsor_name, sponsors[sponsor_slot]);
    }
    if (situation->affected_name[0] == '\0') {
        CopyName(situation->affected_name, affected[slot]);
    }
    CcId affected_home = sim->settlements[slot].id;
    CcId sponsor_home = CcSimSituationOfferSettlementId(sim, situation);
    if (sponsor_home == 0U) sponsor_home = affected_home;
    CcCharacter *sponsor = PromoteCharacter(
        sim, situation->sponsor_name, sponsor_home,
        situation->issuer_faction_id, CC_CHARACTER_OFFICIAL,
        CC_CHARACTER_GOAL_KEEP_ORDER, CC_CHARACTER_ACTIVITY_WORKING);
    CcCharacterRole affected_role =
        situation->kind == CC_SITUATION_MONSTER_EXPEDITION ?
            CC_CHARACTER_SCOUT :
        situation->kind == CC_SITUATION_BLACK_MARKET_DELIVERY ?
            CC_CHARACTER_TRAVELLER :
        situation->kind == CC_SITUATION_COURIER_DELIVERY ?
            CC_CHARACTER_COURIER :
        situation->kind == CC_SITUATION_RELIEF_DELIVERY ?
            CC_CHARACTER_REFUGEE : CC_CHARACTER_LABORER;
    CcCharacterGoal affected_goal =
        situation->kind == CC_SITUATION_COURIER_DELIVERY ?
            CC_CHARACTER_GOAL_CARRY_NEWS :
        situation->kind == CC_SITUATION_RELIEF_DELIVERY ?
            CC_CHARACTER_GOAL_SURVIVE_CRISIS :
            CC_CHARACTER_GOAL_SECURE_LIVELIHOOD;
    CcCharacter *participant = PromoteCharacter(
        sim, situation->affected_name, affected_home, 0U, affected_role,
        affected_goal, CC_CHARACTER_ACTIVITY_SEEKING_AID);
    situation->sponsor_character_id = sponsor != NULL ? sponsor->id : 0U;
    situation->affected_character_id = participant != NULL ?
        participant->id : 0U;
    RefreshSituationCharacterActivities(sim, situation);
}

void CcSimInitializeCharacters(CcSim *sim)
{
    if (sim == NULL) return;
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        AssignSituationCast(sim, &sim->situations[i]);
    }
}

static CcSituation *AllocateSituation(CcSim *sim)
{
    if (sim->situation_count < CC_MAX_SITUATIONS) {
        CcSituation *situation = &sim->situations[sim->situation_count];
        sim->situation_count += 1;
        *situation = (CcSituation){0};
        return situation;
    }
    int32_t oldest = -1;
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        if (sim->situations[i].status == CC_SITUATION_ACTIVE) continue;
        if (oldest < 0 || sim->situations[i].created_day < sim->situations[oldest].created_day) {
            oldest = i;
        }
    }
    if (oldest < 0) return NULL;
    sim->situations[oldest] = (CcSituation){0};
    return &sim->situations[oldest];
}

static bool HasRecentSituation(const CcSim *sim, CcSituationKind kind, CcId target)
{
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        const CcSituation *situation = &sim->situations[i];
        if (situation->kind == kind && situation->target_id == target &&
            (situation->status == CC_SITUATION_ACTIVE ||
             situation->created_day >= sim->current_day - 28)) return true;
    }
    return false;
}

static CcSituation *CreateSituation(
    CcSim *sim, CcSituationKind kind, CcId target, CcId issuer, CcId cause,
    CcGood good, int32_t quantity, CcMoney reward, int32_t duration)
{
    if (HasRecentSituation(sim, kind, target)) return NULL;
    CcSituation *situation = AllocateSituation(sim);
    if (situation == NULL) return NULL;
    situation->id = NextId(sim, CC_ENTITY_SITUATION);
    situation->kind = kind;
    situation->status = CC_SITUATION_ACTIVE;
    situation->issuer_faction_id = issuer;
    situation->target_id = target;
    situation->cause_event_id = cause;
    situation->good = good;
    situation->quantity = quantity;
    situation->reward = reward;
    situation->created_day = sim->current_day;
    situation->deadline_day = sim->current_day + duration;
    AssignSituationCast(sim, situation);
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(text, sizeof(text), "%s issued; reward %" PRId64
                   " crowns before day %d.", CcSituationKindName(kind), reward,
                   situation->deadline_day);
    (void)PushEvent(sim, CC_EVENT_SITUATION_CREATED, situation->id, target,
                    cause, quantity, text);
    return situation;
}

static void GenerateSituations(CcSim *sim)
{
    bool primary_settled = PrimaryCrisisSettled(sim);
    CcSettlement *relief_target = NULL;
    int32_t relief_need = 0;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        CcSettlement *settlement = &sim->settlements[i];
        if (CcSettlementIsAbandoned(settlement)) continue;
        int32_t projected = settlement->stock[CC_GOOD_FOOD] +
                            CcSimIncomingGood(sim, settlement->id, CC_GOOD_FOOD);
        int32_t need = settlement->reserve_target[CC_GOOD_FOOD] - projected +
                       settlement->hunger;
        if (need > relief_need && (projected < settlement->reserve_target[CC_GOOD_FOOD] / 2 ||
                                   settlement->hunger >= 25)) {
            relief_need = need;
            relief_target = settlement;
        }
    }
    if (!primary_settled && relief_target != NULL) {
        CcFaction *issuer = FactionFor(sim, relief_target->kingdom_id, CC_FACTION_COMMONS);
        const CcEvent *shortage = LatestEvent(sim, CC_EVENT_SHORTAGE,
                                              relief_target->id, relief_target->id);
        CreateSituation(sim, CC_SITUATION_RELIEF_DELIVERY, relief_target->id,
                        issuer != NULL ? issuer->id : 0U,
                        shortage != NULL ? shortage->id : LatestLocalCause(sim, relief_target->id),
                        CC_GOOD_FOOD, 8, 18, 35);
    }

    for (int32_t i = 0; i < sim->route_count; ++i) {
        CcRoute *route = &sim->routes[i];
        const CcSettlement *from = CcSimSettlement(sim, route->from_id);
        const CcSettlement *to = CcSimSettlement(sim, route->to_id);
        if (primary_settled || !route->closed || from == NULL || to == NULL ||
            CcSettlementIsAbandoned(from) ||
            CcSettlementIsAbandoned(to)) continue;
        CcSettlement *place = CcSimSettlementMutable(sim, route->to_id);
        CcFaction *issuer = place != NULL ?
            FactionFor(sim, place->kingdom_id, CC_FACTION_CROWN) : NULL;
        const CcEvent *closure = LatestEvent(sim, CC_EVENT_ROUTE_CLOSED, route->id, 0U);
        CreateSituation(sim, CC_SITUATION_ROUTE_REPAIR, route->id,
                        issuer != NULL ? issuer->id : 0U,
                        closure != NULL ? closure->id : 0U,
                        CC_GOOD_TOOLS, 2, 24, 49);
    }

    for (int32_t i = 0; i < sim->monster_count; ++i) {
        CcMonsterPopulation *monster = &sim->monsters[i];
        if (monster->pressure < 35) continue;
        CcDungeon *dungeon = DungeonByIdMutable(sim, monster->dungeon_id);
        CcSettlement *place = dungeon != NULL ?
            CcSimSettlementMutable(sim, dungeon->settlement_id) : NULL;
        if (place != NULL && CcSettlementIsAbandoned(place)) continue;
        CcFaction *issuer = place != NULL ?
            FactionFor(sim, place->kingdom_id, CC_FACTION_GUILD) : NULL;
        const CcEvent *pressure = LatestEvent(sim, CC_EVENT_MONSTER_PRESSURE,
                                             monster->id, monster->dungeon_id);
        if (dungeon != NULL) {
            CreateSituation(sim, CC_SITUATION_MONSTER_EXPEDITION, dungeon->id,
                            issuer != NULL ? issuer->id : 0U,
                            pressure != NULL ? pressure->id : 0U,
                            CC_GOOD_TOOLS, 0, 18, 42);
        }
    }

    for (int32_t i = 0; i < sim->bandit_count; ++i) {
        CcBanditGroup *bandits = &sim->bandits[i];
        const CcRoute *route = CcSimRoute(sim, bandits->route_id);
        if (primary_settled || route == NULL || !route->smuggler_route ||
            bandits->influence < 42) continue;
        CcSettlement *target = CcSimSettlementMutable(sim, route->to_id);
        if (target != NULL && CcSettlementIsAbandoned(target)) continue;
        CcFaction *issuer = target != NULL ?
            FactionFor(sim, target->kingdom_id, CC_FACTION_GUILD) : NULL;
        const CcEvent *pressure = LatestEvent(sim, CC_EVENT_BANDIT_PRESSURE,
                                             bandits->id, route->id);
        if (target != NULL) {
            CreateSituation(sim, CC_SITUATION_BLACK_MARKET_DELIVERY, target->id,
                            issuer != NULL ? issuer->id : 0U,
                            pressure != NULL ? pressure->id : 0U,
                            CC_GOOD_FOOD, 8, 24, 28);
        }
    }
}

static void SupersedeCompetingCrisisSituations(CcSim *sim,
                                                const CcSituation *chosen);
static void RememberCharacter(CcCharacter *character,
                              CcCharacterMemoryKind kind,
                              CcId subject_id, CcId event_id, int32_t day);

static void ResolveSituation(CcSim *sim, CcSituation *situation)
{
    if (situation == NULL || situation->status != CC_SITUATION_ACTIVE) return;
    bool accepted = sim->player.accepted_situation_id == situation->id;
    CcFaction *issuer = FactionByIdMutable(
        sim, situation->issuer_faction_id);
    situation->status = CC_SITUATION_RESOLVED;
    CcMoney reward_paid = 0;
    if (accepted) {
        CcKingdom *kingdom = issuer != NULL ?
            KingdomMutable(sim, issuer->kingdom_id) : NULL;
        if (kingdom != NULL) {
            reward_paid = kingdom->treasury < situation->reward ?
                          kingdom->treasury : situation->reward;
            kingdom->treasury -= reward_paid;
        }
        sim->player.coins += reward_paid;
        sim->player.reputation += 3 + situation->quantity / 3;
        sim->player.accepted_situation_id = 0U;
    }
    if (issuer != NULL) {
        issuer->support = ClampI32(issuer->support + (accepted ? 6 : 2),
                                   0, 100);
    }
    char text[CC_EVENT_TEXT_CAPACITY];
    if (accepted) {
        (void)snprintf(text, sizeof(text),
                       "%s fulfilled; the company receives %" PRId64 " crowns.",
                       CcSituationKindName(situation->kind), reward_paid);
    } else {
        (void)snprintf(text, sizeof(text),
                       "%s closes after the company acts without taking its charter.",
                       CcSituationKindName(situation->kind));
    }
    CcEvent *resolution = PushEvent(
        sim, CC_EVENT_SITUATION_RESOLVED, situation->id,
        situation->target_id, situation->cause_event_id,
        accepted ? (int32_t)reward_paid : 0, text);
    CcCharacter *sponsor = CharacterMutable(
        sim, situation->sponsor_character_id);
    CcCharacter *affected = CharacterMutable(
        sim, situation->affected_character_id);
    if (accepted) {
        CcId resolution_id = resolution != NULL ? resolution->id : 0U;
        if (sponsor != NULL) {
            RememberCharacter(sponsor, CC_CHARACTER_MEMORY_PLAYER_HELPED,
                              situation->id, resolution_id,
                              sim->current_day);
            sponsor->player_disposition = ClampI32(
                sponsor->player_disposition + 8, -100, 100);
        }
        if (affected != NULL && affected != sponsor) {
            RememberCharacter(affected, CC_CHARACTER_MEMORY_PLAYER_HELPED,
                              situation->id, resolution_id,
                              sim->current_day);
            affected->player_disposition = ClampI32(
                affected->player_disposition + 15, -100, 100);
        }
    }
    RefreshSituationCharacterActivities(sim, situation);
    if (accepted && !sim->delayed_echo.active) {
        CcId echo_settlement = 0U;
        if (situation->kind == CC_SITUATION_RELIEF_DELIVERY ||
            situation->kind == CC_SITUATION_BLACK_MARKET_DELIVERY) {
            echo_settlement = situation->target_id;
        } else if (situation->kind == CC_SITUATION_ROUTE_REPAIR) {
            const CcRoute *route = CcSimRoute(sim, situation->target_id);
            if (route != NULL) {
                echo_settlement = sim->player.location_id == route->to_id ?
                    route->to_id : route->from_id;
            }
        } else {
            for (int32_t i = 0; i < sim->dungeon_count; ++i) {
                if (sim->dungeons[i].id == situation->target_id) {
                    echo_settlement = sim->dungeons[i].settlement_id;
                    break;
                }
            }
        }
        if (echo_settlement != 0U) {
            char witness[CC_NAME_CAPACITY];
            const char *witness_name =
                (situation->kind == CC_SITUATION_ROUTE_REPAIR ||
                 situation->kind == CC_SITUATION_BLACK_MARKET_DELIVERY) &&
                        situation->sponsor_name[0] != '\0' ?
                    situation->sponsor_name : situation->affected_name;
            (void)snprintf(witness, sizeof(witness), "%.31s",
                           witness_name[0] != '\0' ? witness_name :
                               "A local witness");
            sim->delayed_echo = (CcDelayedEcho){
                .active = true,
                .situation_id = situation->id,
                .settlement_id = echo_settlement,
                .parent_event_id = resolution != NULL ? resolution->id : 0U,
                .outcome = sim->resolved_journey_outcome !=
                        CC_JOURNEY_OUTCOME_NONE ?
                    sim->resolved_journey_outcome :
                    CC_JOURNEY_OUTCOME_NEGOTIATED,
                .due_day = sim->current_day + 30
            };
            (void)snprintf(sim->delayed_echo.character_name,
                           sizeof(sim->delayed_echo.character_name), "%s",
                           witness);
        }
    }
    SupersedeCompetingCrisisSituations(sim, situation);
}

static void ProgressDeliverySituations(CcSim *sim, CcId settlement_id,
                                       CcGood good, int32_t quantity)
{
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        CcSituation *situation = &sim->situations[i];
        if (situation->status != CC_SITUATION_ACTIVE ||
            situation->id != sim->player.accepted_situation_id ||
            situation->target_id != settlement_id || situation->good != good) continue;
        if (situation->kind != CC_SITUATION_RELIEF_DELIVERY &&
            situation->kind != CC_SITUATION_BLACK_MARKET_DELIVERY) continue;
        if (sim->resolved_journey_situation_id != situation->id ||
            quantity < situation->quantity - situation->progress) continue;
        situation->progress = MinimumI32(situation->quantity,
                                         situation->progress + quantity);
        if (situation->progress >= situation->quantity) ResolveSituation(sim, situation);
    }
}

static void ResolveTargetSituations(CcSim *sim, CcSituationKind kind, CcId target)
{
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        CcSituation *situation = &sim->situations[i];
        if (situation->status == CC_SITUATION_ACTIVE && situation->kind == kind &&
            situation->target_id == target) ResolveSituation(sim, situation);
    }
}

static void SupersedeTargetSituations(CcSim *sim, CcSituationKind kind, CcId target)
{
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        CcSituation *situation = &sim->situations[i];
        if (situation->status != CC_SITUATION_ACTIVE || situation->kind != kind ||
            situation->target_id != target) continue;
        situation->status = CC_SITUATION_FAILED;
        if (sim->player.accepted_situation_id == situation->id) {
            sim->player.accepted_situation_id = 0U;
        }
        char text[CC_EVENT_TEXT_CAPACITY];
        (void)snprintf(text, sizeof(text), "%s withdrawn after another power intervenes.",
                       CcSituationKindName(situation->kind));
        (void)PushEvent(sim, CC_EVENT_SITUATION_FAILED, situation->id,
                        situation->target_id, situation->cause_event_id, 0, text);
        RefreshSituationCharacterActivities(sim, situation);
    }
}

static void SupersedeCompetingCrisisSituations(CcSim *sim,
                                                const CcSituation *chosen)
{
    if (sim == NULL || chosen == NULL) return;
    bool chosen_is_primary =
        chosen->kind == CC_SITUATION_RELIEF_DELIVERY ||
        chosen->kind == CC_SITUATION_ROUTE_REPAIR ||
        chosen->kind == CC_SITUATION_BLACK_MARKET_DELIVERY;
    if (!chosen_is_primary) return;
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        CcSituation *other = &sim->situations[i];
        bool other_is_primary =
            other->kind == CC_SITUATION_RELIEF_DELIVERY ||
            other->kind == CC_SITUATION_ROUTE_REPAIR ||
            other->kind == CC_SITUATION_BLACK_MARKET_DELIVERY;
        if (!other_is_primary || other->id == chosen->id ||
            other->status != CC_SITUATION_ACTIVE) continue;
        other->status = CC_SITUATION_FAILED;
        if (sim->player.accepted_situation_id == other->id) {
            sim->player.accepted_situation_id = 0U;
        }
        CcFaction *issuer = FactionByIdMutable(
            sim, other->issuer_faction_id);
        if (issuer != NULL) {
            issuer->support = ClampI32(issuer->support - 3, 0, 100);
        }
        char text[CC_EVENT_TEXT_CAPACITY];
        (void)snprintf(text, sizeof(text),
                       "%s is withdrawn after the company backs a rival answer.",
                       CcSituationKindName(other->kind));
        (void)PushEvent(sim, CC_EVENT_SITUATION_FAILED, other->id,
                        other->target_id, other->cause_event_id, -3, text);
        RefreshSituationCharacterActivities(sim, other);
    }
}

static void ExpireSituations(CcSim *sim)
{
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        CcSituation *situation = &sim->situations[i];
        if (situation->status != CC_SITUATION_ACTIVE ||
            situation->deadline_day >= sim->current_day) continue;
        situation->status = CC_SITUATION_FAILED;
        bool accepted = sim->player.accepted_situation_id == situation->id;
        if (situation->kind == CC_SITUATION_COURIER_DELIVERY) {
            CcCourier *courier = CourierMutable(sim, situation->target_id);
            if (courier != NULL &&
                (courier->status == CC_COURIER_WAITING ||
                 courier->status == CC_COURIER_WITH_PLAYER)) {
                courier->status = CC_COURIER_WAITING;
                courier->current_settlement_id = accepted ?
                    sim->player.location_id : courier->current_settlement_id;
                courier->departure_day = sim->current_day;
            }
        }
        if (accepted) sim->player.accepted_situation_id = 0U;
        char text[CC_EVENT_TEXT_CAPACITY];
        (void)snprintf(text, sizeof(text), accepted ?
                       "%s expires after the company's promise; its sponsors remember." :
                       "%s expires unanswered; other powers continue without the company.",
                       CcSituationKindName(situation->kind));
        CcEvent *failure = PushEvent(
            sim, CC_EVENT_SITUATION_FAILED, situation->id,
            situation->target_id, situation->cause_event_id, 0, text);
        if (accepted) {
            CcId failure_id = failure != NULL ? failure->id : 0U;
            CcCharacter *sponsor = CharacterMutable(
                sim, situation->sponsor_character_id);
            CcCharacter *affected = CharacterMutable(
                sim, situation->affected_character_id);
            RememberCharacter(sponsor, CC_CHARACTER_MEMORY_PLAYER_WITHDREW,
                              situation->id, failure_id,
                              sim->current_day);
            if (affected != sponsor) {
                RememberCharacter(
                    affected, CC_CHARACTER_MEMORY_PLAYER_WITHDREW,
                    situation->id, failure_id, sim->current_day);
            }
            if (sponsor != NULL) {
                sponsor->player_disposition = ClampI32(
                    sponsor->player_disposition - 6, -100, 100);
            }
            if (affected != NULL && affected != sponsor) {
                affected->player_disposition = ClampI32(
                    affected->player_disposition - 12, -100, 100);
            }
            sim->player.reputation -= 1;
        }
        RefreshSituationCharacterActivities(sim, situation);
    }
}

static int32_t NextSituationExpiryDay(const CcSim *sim)
{
    int32_t next_day = INT_MAX;
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        const CcSituation *situation = &sim->situations[i];
        if (situation->status == CC_SITUATION_ACTIVE &&
            situation->deadline_day < next_day) {
            next_day = situation->deadline_day;
        }
    }
    return next_day;
}

static CcSettlement *KingdomSeat(CcSim *sim, int32_t kingdom_slot)
{
    if (sim == NULL || kingdom_slot < 0 ||
        kingdom_slot >= sim->kingdom_count) return NULL;
    CcSettlement *best = NULL;
    CcId kingdom_id = sim->kingdoms[kingdom_slot].id;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        CcSettlement *place = &sim->settlements[i];
        if (place->kingdom_id != kingdom_id ||
            CcSettlementIsAbandoned(place)) continue;
        if (best == NULL || place->function == CC_SETTLEMENT_CAPITAL ||
            (best->function != CC_SETTLEMENT_CAPITAL &&
             place->population > best->population)) {
            best = place;
        }
    }
    return best;
}

static int32_t KingdomAverageHunger(const CcSim *sim, int32_t kingdom_slot)
{
    if (sim == NULL || kingdom_slot < 0 ||
        kingdom_slot >= sim->kingdom_count) return 0;
    int32_t total = 0;
    int32_t count = 0;
    CcId kingdom_id = sim->kingdoms[kingdom_slot].id;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        if (sim->settlements[i].kingdom_id != kingdom_id ||
            CcSettlementIsAbandoned(&sim->settlements[i])) continue;
        total += sim->settlements[i].hunger;
        count += 1;
    }
    return count > 0 ? total / count : 0;
}

static int32_t KingdomAverageProsperity(const CcSim *sim,
                                        int32_t kingdom_slot)
{
    if (sim == NULL || kingdom_slot < 0 ||
        kingdom_slot >= sim->kingdom_count) return 0;
    int32_t total = 0;
    int32_t count = 0;
    CcId kingdom_id = sim->kingdoms[kingdom_slot].id;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        if (sim->settlements[i].kingdom_id != kingdom_id ||
            CcSettlementIsAbandoned(&sim->settlements[i])) continue;
        total += sim->settlements[i].prosperity;
        count += 1;
    }
    return count > 0 ? total / count : 0;
}

static int32_t KingdomFood(const CcSim *sim, int32_t kingdom_slot)
{
    if (sim == NULL || kingdom_slot < 0 ||
        kingdom_slot >= sim->kingdom_count) return 0;
    int32_t total = 0;
    CcId kingdom_id = sim->kingdoms[kingdom_slot].id;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        if (sim->settlements[i].kingdom_id == kingdom_id &&
            !CcSettlementIsAbandoned(&sim->settlements[i])) {
            total += sim->settlements[i].stock[CC_GOOD_FOOD];
        }
    }
    return total;
}

static int64_t KingdomWarScore(const CcSim *sim, int32_t kingdom_slot)
{
    const CcKingdom *kingdom = &sim->kingdoms[kingdom_slot];
    int64_t score = (int64_t)kingdom->legitimacy * 3 +
                    MinimumI32(100, kingdom->treasury > INT32_MAX ?
                        INT32_MAX : (int32_t)kingdom->treasury / 10);
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        const CcSettlement *place = &sim->settlements[i];
        if (place->kingdom_id != kingdom->id ||
            CcSettlementIsAbandoned(place)) continue;
        score += place->security + place->prosperity - place->hunger;
    }
    return score;
}

static void ResolveWarSettlement(CcSim *sim, int32_t first,
                                 int32_t second, CcId cause_event_id)
{
    int32_t winner = KingdomWarScore(sim, first) >=
                     KingdomWarScore(sim, second) ? first : second;
    int32_t loser = winner == first ? second : first;
    CcId loser_id = sim->kingdoms[loser].id;
    int32_t active_loser_settlements = 0;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        const CcSettlement *place = &sim->settlements[i];
        if (place->kingdom_id == loser_id &&
            !CcSettlementIsAbandoned(place)) {
            active_loser_settlements += 1;
        }
    }
    if (active_loser_settlements <= 1) return;

    CcSettlement *ceded = NULL;
    int32_t weakest_score = INT32_MAX;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        CcSettlement *place = &sim->settlements[i];
        if (place->kingdom_id != loser_id ||
            CcSettlementIsAbandoned(place) ||
            place->function == CC_SETTLEMENT_CAPITAL) continue;
        int32_t score = place->security + place->prosperity -
                        place->hunger;
        if (ceded == NULL || score < weakest_score) {
            ceded = place;
            weakest_score = score;
        }
    }
    if (ceded == NULL) return;

    ceded->kingdom_id = sim->kingdoms[winner].id;
    ceded->security = MaximumI32(10, ceded->security - 12);
    ceded->prosperity = MaximumI32(0, ceded->prosperity - 8);
    ceded->hunger = ClampI32(ceded->hunger + 8, 0, 100);
    sim->kingdoms[winner].legitimacy = ClampI32(
        sim->kingdoms[winner].legitimacy + 5, 0, 100);
    sim->kingdoms[loser].legitimacy = ClampI32(
        sim->kingdoms[loser].legitimacy - 12, 0, 100);
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(
        text, sizeof(text),
        "%.16s cedes %.16s to %.16s; peace redraws the border.",
        sim->kingdoms[loser].name, ceded->name,
        sim->kingdoms[winner].name);
    (void)PushEvent(
        sim, CC_EVENT_KINGDOM_ACTION, ceded->id, ceded->id,
        cause_event_id, winner + 1, text);
}

static void SetDiplomacy(CcSim *sim, int32_t first, int32_t second,
                         CcDiplomaticState state)
{
    if (sim == NULL || first < 0 || second < 0 || first == second ||
        first >= sim->kingdom_count || second >= sim->kingdom_count) return;
    sim->diplomacy[first][second] = state;
    sim->diplomacy[second][first] = state;
    sim->diplomacy_changed_day[first][second] = sim->current_day;
    sim->diplomacy_changed_day[second][first] = sim->current_day;
}

static bool CourierActiveBetween(const CcSim *sim, CcCourierKind kind,
                                 CcId first, CcId second)
{
    if (sim == NULL) return false;
    for (int32_t i = 0; i < sim->courier_count; ++i) {
        const CcCourier *courier = &sim->couriers[i];
        bool active = courier->status == CC_COURIER_WAITING ||
                      courier->status == CC_COURIER_TRAVELLING ||
                      courier->status == CC_COURIER_WITH_PLAYER;
        if (active && courier->kind == kind &&
            courier->issuer_kingdom_id == first &&
            courier->recipient_kingdom_id == second) return true;
    }
    return false;
}

static CcCourier *AllocateCourier(CcSim *sim)
{
    if (sim->courier_count < CC_MAX_COURIERS) {
        CcCourier *courier = &sim->couriers[sim->courier_count++];
        *courier = (CcCourier){0};
        return courier;
    }
    int32_t oldest = -1;
    for (int32_t i = 0; i < sim->courier_count; ++i) {
        CcCourierStatus status = sim->couriers[i].status;
        if (status == CC_COURIER_WAITING ||
            status == CC_COURIER_TRAVELLING ||
            status == CC_COURIER_WITH_PLAYER) continue;
        if (oldest < 0 || sim->couriers[i].departure_day <
                          sim->couriers[oldest].departure_day) oldest = i;
    }
    if (oldest < 0) return NULL;
    sim->couriers[oldest] = (CcCourier){0};
    return &sim->couriers[oldest];
}

static const char *CourierPurpose(CcCourierKind kind)
{
    switch (kind) {
        case CC_COURIER_WAR_DECLARATION: return "a declaration of war";
        case CC_COURIER_PEACE_OFFER: return "a peace offer";
        case CC_COURIER_DRAGON_ALLIANCE: return "an anti-dragon alliance call";
        case CC_COURIER_DRAGON_MUSTER: return "a dragon-host muster";
    }
    return "a sealed message";
}

static void FailCourierSituation(CcSim *sim, CcCourier *courier)
{
    if (sim == NULL || courier == NULL || courier->situation_id == 0U) return;
    CcSituation *situation = (CcSituation *)CcSimSituation(
        sim, courier->situation_id);
    if (situation == NULL || situation->status != CC_SITUATION_ACTIVE) return;
    if (sim->player.accepted_situation_id == situation->id) {
        sim->player.accepted_situation_id = 0U;
        sim->player.reputation -= 1;
    }
    situation->status = CC_SITUATION_FAILED;
}

static void ApplyCourierMessage(CcSim *sim, CcCourier *courier,
                                CcId arrival_event_id)
{
    int32_t issuer = KingdomSlotById(sim, courier->issuer_kingdom_id);
    int32_t recipient = KingdomSlotById(sim, courier->recipient_kingdom_id);
    if (issuer < 0 || recipient < 0) return;
    CcEventKind event_kind = CC_EVENT_COURIER_ARRIVED;
    CcDiplomaticState state = sim->diplomacy[issuer][recipient];
    bool ending_war = false;
    bool changed = false;
    if (courier->kind == CC_COURIER_WAR_DECLARATION) {
        state = CC_DIPLOMACY_WAR;
        event_kind = CC_EVENT_WAR_DECLARED;
        changed = true;
    } else if (courier->kind == CC_COURIER_PEACE_OFFER &&
               (state == CC_DIPLOMACY_WAR ||
                state == CC_DIPLOMACY_ALLIANCE)) {
        ending_war = state == CC_DIPLOMACY_WAR;
        state = CC_DIPLOMACY_PEACE;
        event_kind = CC_EVENT_PEACE_DECLARED;
        changed = true;
    } else if (courier->kind == CC_COURIER_DRAGON_ALLIANCE &&
               !sim->dragon.slain &&
               sim->dragon.hoard * 4 >= CcSimTrackedGold(sim)) {
        state = CC_DIPLOMACY_ALLIANCE;
        event_kind = CC_EVENT_ALLIANCE_DECLARED;
        changed = true;
    } else if (courier->kind == CC_COURIER_DRAGON_MUSTER &&
               state == CC_DIPLOMACY_ALLIANCE) {
        sim->dragon_campaign.pledged_kingdom_mask |=
            UINT32_C(1) << (uint32_t)issuer;
        sim->dragon_campaign.pledged_kingdom_mask |=
            UINT32_C(1) << (uint32_t)recipient;
        event_kind = CC_EVENT_DRAGON_MUSTERED;
        changed = true;
    }
    if (courier->kind != CC_COURIER_DRAGON_MUSTER && changed) {
        if (ending_war) {
            ResolveWarSettlement(sim, issuer, recipient, arrival_event_id);
        }
        SetDiplomacy(sim, issuer, recipient, state);
    }
    char text[CC_EVENT_TEXT_CAPACITY];
    if (changed) {
        (void)snprintf(
            text, sizeof(text),
            "%s's courier reaches %s: %s now binds the two courts.",
            sim->kingdoms[issuer].name, sim->kingdoms[recipient].name,
            courier->kind == CC_COURIER_DRAGON_MUSTER ?
                "a sworn dragon muster" : CcDiplomaticStateName(state));
    } else {
        (void)snprintf(
            text, sizeof(text),
            "%s receives %s, but its court does not act on it.",
            sim->kingdoms[recipient].name, CourierPurpose(courier->kind));
    }
    CcEvent *effect = PushEvent(
        sim, event_kind, sim->kingdoms[issuer].id,
        courier->destination_settlement_id, arrival_event_id,
        courier->reliability, text);
    (void)effect;
    /* Keep the delivered seal pinned. The political effect remains its child. */
    courier->cause_event_id = arrival_event_id;
}

static void DeliverCourier(CcSim *sim, CcCourier *courier,
                           bool carried_by_player)
{
    CcSettlement *destination = CcSimSettlementMutable(
        sim, courier->destination_settlement_id);
    CcKingdom *recipient = KingdomMutable(sim, courier->recipient_kingdom_id);
    int32_t corruption = destination != NULL ?
        MaximumI32(0, 40 - destination->security) : 20;
    if (recipient != NULL) {
        corruption += MaximumI32(0, 35 - recipient->legitimacy);
    }
    bool distorted = courier->reliability < 35 ||
        (corruption > 0 &&
         (int32_t)(NextRandom(sim) % 100U) < corruption / 2);
    char text[CC_EVENT_TEXT_CAPACITY];
    if (distorted) {
        courier->status = CC_COURIER_DISTORTED;
        bool forged_reading = (NextRandom(sim) & 1U) == 0U;
        if (forged_reading) {
            int32_t issuer = KingdomSlotById(
                sim, courier->issuer_kingdom_id);
            int32_t recipient_slot = KingdomSlotById(
                sim, courier->recipient_kingdom_id);
            if (issuer >= 0 && recipient_slot >= 0) {
                CcDiplomaticState false_state =
                    courier->kind == CC_COURIER_WAR_DECLARATION ?
                        CC_DIPLOMACY_PEACE : CC_DIPLOMACY_WAR;
                SetDiplomacy(sim, issuer, recipient_slot, false_state);
                (void)snprintf(
                    text, sizeof(text),
                    "A corrupt mayor in %s breaks the seal and reads it backward; the courts record %s.",
                    destination != NULL ? destination->name :
                        "the receiving court",
                    CcDiplomaticStateName(false_state));
            } else {
                forged_reading = false;
            }
        }
        if (!forged_reading) {
            (void)snprintf(
                text, sizeof(text),
                "A damaged seal reaches %s; a local mayor suppresses its disputed meaning.",
                destination != NULL ? destination->name :
                    "the receiving court");
        }
        (void)PushEvent(
            sim, CC_EVENT_COURIER_DISTORTED, courier->id,
            courier->destination_settlement_id, courier->cause_event_id,
            courier->reliability, text);
        FailCourierSituation(sim, courier);
        return;
    }
    courier->status = CC_COURIER_DELIVERED;
    (void)snprintf(
        text, sizeof(text),
        "%s reaches %s with %s intact.",
        carried_by_player ? "The Crownless carriage" : "A royal courier",
        destination != NULL ? destination->name : "the court",
        CourierPurpose(courier->kind));
    CcEvent *arrival = PushEvent(
        sim, CC_EVENT_COURIER_ARRIVED, courier->id,
        courier->destination_settlement_id, courier->cause_event_id,
        courier->reliability, text);
    ApplyCourierMessage(sim, courier,
                        arrival != NULL ? arrival->id : courier->cause_event_id);
    CcSituation *situation = (CcSituation *)CcSimSituation(
        sim, courier->situation_id);
    if (carried_by_player && situation != NULL &&
        situation->status == CC_SITUATION_ACTIVE) {
        ResolveSituation(sim, situation);
    } else {
        FailCourierSituation(sim, courier);
    }
}

static void StartCourierLeg(CcSim *sim, CcCourier *courier)
{
    if (courier->current_settlement_id ==
        courier->destination_settlement_id) {
        DeliverCourier(sim, courier, false);
        return;
    }
    int32_t route_slot = -1;
    CcId next_hop = 0U;
    if (!FindTradePath(sim, courier->current_settlement_id,
                       courier->destination_settlement_id, CC_GOOD_FOOD,
                       &route_slot, &next_hop, NULL, NULL)) {
        courier->status = CC_COURIER_LOST;
        char text[CC_EVENT_TEXT_CAPACITY];
        (void)snprintf(text, sizeof(text),
                       "No rider can find a passable road for %s.",
                       CourierPurpose(courier->kind));
        (void)PushEvent(sim, CC_EVENT_COURIER_LOST, courier->id,
                        courier->current_settlement_id,
                        courier->cause_event_id, 0, text);
        FailCourierSituation(sim, courier);
        return;
    }
    CcRoute *route = &sim->routes[route_slot];
    courier->status = CC_COURIER_TRAVELLING;
    courier->route_id = route->id;
    courier->arrival_day = sim->current_day + route->travel_days;
    char text[CC_EVENT_TEXT_CAPACITY];
    const CcSettlement *from = CcSimSettlement(
        sim, courier->current_settlement_id);
    const CcSettlement *to = CcSimSettlement(sim, next_hop);
    (void)snprintf(text, sizeof(text),
                   "A royal courier carries %s from %s toward %s.",
                   CourierPurpose(courier->kind),
                   from != NULL ? from->name : "one court",
                   to != NULL ? to->name : "the next post");
    CcEvent *departure = PushEvent(
        sim, CC_EVENT_COURIER_DEPARTED, courier->id,
        courier->current_settlement_id, courier->cause_event_id,
        route->travel_days, text);
    courier->cause_event_id = departure != NULL ? departure->id :
                               courier->cause_event_id;
}

static CcCourier *LaunchCourier(CcSim *sim, CcCourierKind kind,
                                int32_t issuer_slot, int32_t recipient_slot,
                                CcId cause_event_id)
{
    if (issuer_slot < 0 || recipient_slot < 0 ||
        issuer_slot >= sim->kingdom_count ||
        recipient_slot >= sim->kingdom_count ||
        CourierActiveBetween(sim, kind, sim->kingdoms[issuer_slot].id,
                             sim->kingdoms[recipient_slot].id)) return NULL;
    CcSettlement *origin = KingdomSeat(sim, issuer_slot);
    CcSettlement *destination = KingdomSeat(sim, recipient_slot);
    if (origin == NULL || destination == NULL) return NULL;
    CcCourier *courier = AllocateCourier(sim);
    if (courier == NULL) return NULL;
    *courier = (CcCourier){
        .id = NextId(sim, CC_ENTITY_COURIER),
        .kind = kind,
        .status = CC_COURIER_WAITING,
        .issuer_kingdom_id = sim->kingdoms[issuer_slot].id,
        .recipient_kingdom_id = sim->kingdoms[recipient_slot].id,
        .origin_settlement_id = origin->id,
        .destination_settlement_id = destination->id,
        .current_settlement_id = origin->id,
        .cause_event_id = cause_event_id,
        .departure_day = sim->current_day + 7,
        .reliability = 85 + (int32_t)(NextRandom(sim) % 16U)
    };
    CcFaction *issuer = FactionFor(
        sim, courier->issuer_kingdom_id, CC_FACTION_CROWN);
    CcSituation *situation = CreateSituation(
        sim, CC_SITUATION_COURIER_DELIVERY, courier->id,
        issuer != NULL ? issuer->id : 0U, cause_event_id,
        CC_GOOD_FOOD, 0, 16, 7);
    if (situation == NULL) {
        courier->status = CC_COURIER_LOST;
        return NULL;
    }
    courier->situation_id = situation->id;
    return courier;
}

static void AdvanceCouriers(CcSim *sim)
{
    for (int32_t i = 0; i < sim->courier_count; ++i) {
        CcCourier *courier = &sim->couriers[i];
        if (courier->status == CC_COURIER_WAITING &&
            sim->current_day >= courier->departure_day) {
            StartCourierLeg(sim, courier);
            continue;
        }
        if (courier->status != CC_COURIER_TRAVELLING ||
            sim->current_day < courier->arrival_day) continue;
        const CcRoute *route = CcSimRoute(sim, courier->route_id);
        if (route == NULL) {
            courier->status = CC_COURIER_LOST;
            FailCourierSituation(sim, courier);
            continue;
        }
        int32_t danger = CcSimRouteDanger(sim, route->id);
        CcId next_hop = route->from_id == courier->current_settlement_id ?
                        route->to_id : route->from_id;
        if ((int32_t)(NextRandom(sim) % 100U) < danger / 5) {
            courier->status = CC_COURIER_LOST;
            char text[CC_EVENT_TEXT_CAPACITY];
            (void)snprintf(text, sizeof(text),
                           "A courier carrying %s vanishes on the road.",
                           CourierPurpose(courier->kind));
            (void)PushEvent(sim, CC_EVENT_COURIER_LOST, courier->id,
                            route->id, courier->cause_event_id, danger, text);
            FailCourierSituation(sim, courier);
            continue;
        }
        courier->reliability = ClampI32(
            courier->reliability - danger / 6 -
            (route->smuggler_route ? 5 : 0), 0, 100);
        courier->current_settlement_id = next_hop;
        courier->route_id = 0U;
        if (next_hop == courier->destination_settlement_id) {
            DeliverCourier(sim, courier, false);
        } else {
            StartCourierLeg(sim, courier);
        }
    }
}

static int32_t MaskCount(uint32_t mask)
{
    int32_t count = 0;
    while (mask != 0U) {
        count += (int32_t)(mask & 1U);
        mask >>= 1U;
    }
    return count;
}

static int32_t AllianceGoodAvailable(const CcSim *sim, uint32_t mask,
                                     CcGood good)
{
    int32_t total = 0;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        int32_t slot = KingdomSlotById(sim, sim->settlements[i].kingdom_id);
        if (slot >= 0 &&
            (mask & (UINT32_C(1) << (uint32_t)slot)) != 0U) {
            total += sim->settlements[i].stock[good];
        }
    }
    return total;
}

static void TakeAllianceGood(CcSim *sim, uint32_t mask, CcGood good,
                             int32_t quantity)
{
    for (int32_t i = 0; i < sim->settlement_count && quantity > 0; ++i) {
        CcSettlement *place = &sim->settlements[i];
        int32_t slot = KingdomSlotById(sim, place->kingdom_id);
        if (slot < 0 ||
            (mask & (UINT32_C(1) << (uint32_t)slot)) == 0U) continue;
        int32_t taken = MinimumI32(quantity, place->stock[good]);
        place->stock[good] -= taken;
        quantity -= taken;
    }
}

static int32_t DragonCampaignTravelDays(const CcSim *sim,
                                        CcId origin_id)
{
    const CcSettlement *origin = CcSimSettlement(sim, origin_id);
    const CcSettlement *lair = CcSimSettlement(
        sim, sim->dragon.lair_settlement_id);
    if (origin == NULL || lair == NULL) return 7;
    return ClampI32(
        4 + (AbsoluteI32(origin->map_x - lair->map_x) +
             AbsoluteI32(origin->map_y - lair->map_y)) / 180,
        5, 12);
}

static void TryLaunchDragonCampaign(CcSim *sim)
{
    CcDragonCampaign *campaign = &sim->dragon_campaign;
    uint32_t mask = campaign->pledged_kingdom_mask;
    if (campaign->phase != CC_DRAGON_CAMPAIGN_IDLE ||
        campaign->cooldown_days > 0 || sim->dragon.slain ||
        MaskCount(mask) < 2 ||
        AllianceGoodAvailable(sim, mask, CC_GOOD_FOOD) < 32 ||
        AllianceGoodAvailable(sim, mask, CC_GOOD_TOOLS) < 8 ||
        AllianceGoodAvailable(sim, mask, CC_GOOD_WEAPONS) < 12) return;
    int32_t leader_slot = 0;
    while (leader_slot < sim->kingdom_count &&
           (mask & (UINT32_C(1) << (uint32_t)leader_slot)) == 0U) {
        leader_slot += 1;
    }
    CcSettlement *origin = KingdomSeat(sim, leader_slot);
    if (origin == NULL) return;
    TakeAllianceGood(sim, mask, CC_GOOD_FOOD, 32);
    TakeAllianceGood(sim, mask, CC_GOOD_TOOLS, 8);
    TakeAllianceGood(sim, mask, CC_GOOD_WEAPONS, 12);
    campaign->phase = CC_DRAGON_CAMPAIGN_OUTBOUND;
    campaign->alliance_kingdom_mask = mask;
    campaign->origin_settlement_id = origin->id;
    campaign->days_remaining = DragonCampaignTravelDays(sim, origin->id);
    campaign->supplies[CC_GOOD_FOOD] = 32;
    campaign->supplies[CC_GOOD_TOOLS] = 8;
    campaign->supplies[CC_GOOD_WEAPONS] = 12;
    campaign->attempts += 1;
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(
        text, sizeof(text),
        "An allied host leaves %s with 32 Food, 8 Tools, and 12 Weapons for the dragon cave.",
        origin->name);
    CcEvent *event = PushEvent(
        sim, CC_EVENT_DRAGON_MUSTERED, sim->kingdoms[leader_slot].id,
        origin->id, campaign->cause_event_id, MaskCount(mask), text);
    campaign->cause_event_id = event != NULL ? event->id : 0U;
}

static void RecoverDragonHoard(CcSim *sim)
{
    CcDragonCampaign *campaign = &sim->dragon_campaign;
    CcSettlement *origin = CcSimSettlementMutable(
        sim, campaign->origin_settlement_id);
    int32_t allies = MaskCount(campaign->alliance_kingdom_mask);
    CcMoney share = allies > 0 ? campaign->recovered_coins / allies : 0;
    CcMoney distributed = 0;
    for (int32_t i = 0; i < sim->kingdom_count; ++i) {
        if ((campaign->alliance_kingdom_mask &
             (UINT32_C(1) << (uint32_t)i)) == 0U) continue;
        sim->kingdoms[i].treasury += share;
        distributed += share;
    }
    if (origin != NULL) {
        origin->market_coins += campaign->recovered_coins - distributed;
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
            origin->stock[good] += campaign->supplies[good];
            campaign->supplies[good] = 0;
        }
        for (int32_t i = 0; i < sim->treasure_count; ++i) {
            CcTreasure *treasure = &sim->treasures[i];
            if (!treasure->destroyed &&
                treasure->owner_id == sim->dragon.id) {
                treasure->owner_id = origin->id;
                treasure->location_id = origin->id;
            }
        }
    }
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(
        text, sizeof(text),
        "The dragon host returns %" PRId64
        " recovered crowns to %d allied realms and unloads the cave's trinkets at %s.",
        campaign->recovered_coins, allies,
        origin != NULL ? origin->name : "its mustering ground");
    (void)PushEvent(
        sim, CC_EVENT_DRAGON_HOARD_RECOVERED, sim->dragon.id,
        campaign->origin_settlement_id, campaign->cause_event_id,
        campaign->recovered_coins > INT32_MAX ? INT32_MAX :
            (int32_t)campaign->recovered_coins, text);
    campaign->phase = CC_DRAGON_CAMPAIGN_IDLE;
    campaign->pledged_kingdom_mask = 0U;
    campaign->alliance_kingdom_mask = 0U;
    campaign->origin_settlement_id = 0U;
    campaign->cause_event_id = 0U;
    campaign->days_remaining = 0;
    campaign->cooldown_days = 365;
    campaign->recovered_coins = 0;
}

static void AdvanceDragonCampaign(CcSim *sim)
{
    CcDragonCampaign *campaign = &sim->dragon_campaign;
    if (campaign->phase == CC_DRAGON_CAMPAIGN_IDLE) {
        campaign->cooldown_days = MaximumI32(
            0, campaign->cooldown_days - 1);
        return;
    }
    campaign->days_remaining = MaximumI32(0, campaign->days_remaining - 1);
    if (campaign->days_remaining > 0) return;
    if (campaign->phase == CC_DRAGON_CAMPAIGN_RETURNING) {
        RecoverDragonHoard(sim);
        return;
    }
    int32_t allies = MaskCount(campaign->alliance_kingdom_mask);
    int32_t campaign_experience = CcDragonCampaignExperience(sim);
    int32_t attack = campaign->supplies[CC_GOOD_WEAPONS] * 4 +
                     campaign->supplies[CC_GOOD_TOOLS] * 2 +
                     campaign->supplies[CC_GOOD_FOOD] / 4 +
                     allies * 12 + campaign_experience +
                     (int32_t)(NextRandom(sim) % 21U);
    int32_t defense = CcSimDragonBattleStrength(sim) +
                      sim->goblins.members / 2 +
                      (sim->goblins.devotion + sim->goblins.cohesion) / 8 +
                      MinimumI32(18, sim->goblins.hoard_defenses * 3) +
                      (int32_t)(NextRandom(sim) % 31U);
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        campaign->supplies[good] = 0;
    }
    char text[CC_EVENT_TEXT_CAPACITY];
    CcEvent *battle = NULL;
    if (attack < defense) {
        campaign->defeats += 1;
        campaign->phase = CC_DRAGON_CAMPAIGN_IDLE;
        campaign->pledged_kingdom_mask = 0U;
        campaign->alliance_kingdom_mask = 0U;
        campaign->days_remaining = 0;
        campaign->cooldown_days = 365;
        CcSettlement *origin = CcSimSettlementMutable(
            sim, campaign->origin_settlement_id);
        if (origin != NULL) {
            origin->security = ClampI32(origin->security - 12, 0, 100);
            origin->population = MaximumI32(100, origin->population - 120);
        }
        sim->goblins.members = MaximumI32(12, sim->goblins.members - 6);
        sim->goblins.devotion = ClampI32(
            sim->goblins.devotion + 4, 0, 100);
        sim->goblins.cohesion = ClampI32(
            sim->goblins.cohesion + 2, 0, 100);
        sim->dragon.body_condition = MaximumI32(
            0, sim->dragon.body_condition - 12 - allies * 2);
        sim->dragon.memory_integrity = MaximumI32(
            0, sim->dragon.memory_integrity - 4);
        sim->dragon.territory_stability = MaximumI32(
            0, sim->dragon.territory_stability - 8);
        (void)snprintf(
            text, sizeof(text),
            "The allied dragon host breaks at the cave: strength %d against %d, but survivors carry hard-won knowledge home.",
            attack, defense);
        battle = PushEvent(
            sim, CC_EVENT_DRAGON_BATTLE, sim->dragon.id,
            campaign->origin_settlement_id, campaign->cause_event_id,
            attack - defense, text);
        campaign->cause_event_id = battle != NULL ? battle->id : 0U;
        campaign->origin_settlement_id = 0U;
        return;
    }
    campaign->victories += 1;
    campaign->recovered_coins = sim->dragon.hoard;
    sim->dragon.hoard = 0;
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        campaign->supplies[good] = sim->dragon.hoard_goods[good];
        sim->dragon.hoard_goods[good] = 0;
    }
    sim->dragon.slain = true;
    sim->dragon.slain_day = sim->current_day;
    sim->dragon.life_stage = CC_DRAGON_STAGE_AFTERDRAGON;
    sim->dragon.activity = CC_DRAGON_ACTIVITY_AFTERMATH;
    sim->dragon.body_condition = 0;
    sim->dragon.crown_strength = 0;
    sim->dragon.afterdeath_days = 0;
    sim->dragon.stolen_outstanding = 0;
    sim->dragon.stolen_treasure_id = 0U;
    sim->dragon.theft_actor_id = 0U;
    sim->dragon.retaliation_target_id = 0U;
    sim->dragon.omen_event_id = 0U;
    sim->dragon.omen_days_remaining = 0;
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        sim->goblins.lair_stock[good] += sim->goblins.carried_goods[good];
        sim->goblins.carried_goods[good] = 0;
    }
    sim->goblins.lair_coins += sim->goblins.carried_tribute;
    sim->goblins.carried_tribute = 0;
    if (sim->goblins.carried_treasure_id != 0U) {
        CcTreasure *treasure = (CcTreasure *)CcSimTreasure(
            sim, sim->goblins.carried_treasure_id);
        if (treasure != NULL) {
            treasure->owner_id = sim->goblins.id;
            treasure->location_id = sim->goblins.lair_settlement_id;
        }
        sim->goblins.carried_treasure_id = 0U;
    }
    sim->goblins.tribute_phase = CC_GOBLIN_TRIBUTE_IDLE;
    sim->goblins.raid_motive = CC_GOBLIN_RAID_NONE;
    sim->goblins.tribute_target_id = 0U;
    sim->goblins.tribute_event_id = 0U;
    sim->goblins.tribute_days_remaining = 0;
    sim->goblins.devotion = ClampI32(MaximumI32(
        25, sim->goblins.devotion * 2 / 3 +
            sim->goblins.hoard_defenses * 2), 0, 100);
    sim->goblins.cohesion = ClampI32(
        sim->goblins.cohesion - 15, 0, 100);
    sim->goblins.members = MaximumI32(
        12, sim->goblins.members * 3 / 4);
    (void)snprintf(
        text, sizeof(text),
        "The allied host slays %s: strength %d against %d; the real hoard begins its journey home.",
        sim->dragon.name, attack, defense);
    battle = PushEvent(
        sim, CC_EVENT_DRAGON_SLAIN, sim->dragon.id,
        sim->dragon.lair_settlement_id, campaign->cause_event_id,
        attack - defense, text);
    campaign->cause_event_id = battle != NULL ? battle->id : 0U;
    sim->dragon.lifecycle_event_id = battle != NULL ? battle->id :
                                      sim->dragon.lifecycle_event_id;
    campaign->phase = CC_DRAGON_CAMPAIGN_RETURNING;
    campaign->days_remaining = DragonCampaignTravelDays(
        sim, campaign->origin_settlement_id);
}

static void UpdateRoyalDiplomacy(CcSim *sim)
{
    if (sim == NULL || sim->current_day % 28 != 0) return;
    CcMoney tracked_gold = CcSimTrackedGold(sim);
    bool dragon_crisis = !sim->dragon.slain &&
        ((tracked_gold > 0 && sim->dragon.hoard * 3 >= tracked_gold) ||
         sim->dragon.regional_influence >= 60);
    if (sim->current_day % 112 == 0 &&
        sim->dragon_campaign.cooldown_days == 0) {
        for (int32_t first = 0; first < sim->kingdom_count; ++first) {
            for (int32_t second = first + 1;
                 second < sim->kingdom_count; ++second) {
                if (sim->diplomacy[first][second] !=
                    CC_DIPLOMACY_ALLIANCE) continue;
                int32_t age = sim->current_day -
                    sim->diplomacy_changed_day[first][second];
                int32_t limit = dragon_crisis ? 8 * 364 :
                                sim->dragon.slain ? 364 : 4 * 364;
                if (age < limit) continue;
                CcCourierKind message = dragon_crisis &&
                    sim->dragon_campaign.defeats >
                        sim->dragon_campaign.victories ?
                    CC_COURIER_WAR_DECLARATION : CC_COURIER_PEACE_OFFER;
                (void)LaunchCourier(sim, message,
                                    first, second,
                                    sim->dragon.hoard_event_id);
                if (dragon_crisis) {
                    sim->dragon_campaign.cooldown_days = 5 * 364;
                }
                return;
            }
        }
    }
    if (dragon_crisis) {
        if (sim->dragon_campaign.cooldown_days > 365) return;
        for (int32_t first = 0; first < sim->kingdom_count; ++first) {
            for (int32_t second = first + 1;
                 second < sim->kingdom_count; ++second) {
                CcDiplomaticState state = sim->diplomacy[first][second];
                if (state == CC_DIPLOMACY_ALLIANCE ||
                    CourierActiveBetween(
                        sim, CC_COURIER_DRAGON_ALLIANCE,
                        sim->kingdoms[first].id,
                        sim->kingdoms[second].id)) continue;
                (void)LaunchCourier(sim, CC_COURIER_DRAGON_ALLIANCE,
                                    first, second,
                                    sim->dragon.hoard_event_id);
                return;
            }
        }
        for (int32_t first = 0; first < sim->kingdom_count; ++first) {
            for (int32_t second = first + 1;
                 second < sim->kingdom_count; ++second) {
                if (sim->diplomacy[first][second] !=
                        CC_DIPLOMACY_ALLIANCE ||
                    CourierActiveBetween(
                        sim, CC_COURIER_DRAGON_MUSTER,
                        sim->kingdoms[first].id,
                        sim->kingdoms[second].id)) continue;
                uint32_t pair_mask =
                    (UINT32_C(1) << (uint32_t)first) |
                    (UINT32_C(1) << (uint32_t)second);
                if ((sim->dragon_campaign.pledged_kingdom_mask & pair_mask) ==
                    pair_mask) continue;
                (void)LaunchCourier(sim, CC_COURIER_DRAGON_MUSTER,
                                    first, second,
                                    sim->dragon.hoard_event_id);
                return;
            }
        }
        TryLaunchDragonCampaign(sim);
        return;
    }
    if (sim->current_day % 112 != 0) return;
    for (int32_t first = 0; first < sim->kingdom_count; ++first) {
        for (int32_t second = first + 1;
             second < sim->kingdom_count; ++second) {
            CcDiplomaticState state = sim->diplomacy[first][second];
            int32_t age = sim->current_day -
                          sim->diplomacy_changed_day[first][second];
            if (state == CC_DIPLOMACY_WAR && age >= 364 &&
                (age >= 12 * 364 ||
                 KingdomAverageHunger(sim, first) > 35 ||
                 KingdomAverageHunger(sim, second) > 35 ||
                 (sim->kingdoms[first].legitimacy < 35 &&
                  sim->kingdoms[second].legitimacy < 35))) {
                int32_t issuer = sim->kingdoms[first].legitimacy >=
                                 sim->kingdoms[second].legitimacy ?
                                 first : second;
                int32_t recipient = issuer == first ? second : first;
                (void)LaunchCourier(sim, CC_COURIER_PEACE_OFFER,
                                    issuer, recipient, 0U);
                return;
            }
            if (state == CC_DIPLOMACY_PEACE && age >= 1092) {
                int32_t first_hunger = KingdomAverageHunger(sim, first);
                int32_t second_hunger = KingdomAverageHunger(sim, second);
                int32_t first_pressure = first_hunger * 2 +
                    MaximumI32(0, KingdomFood(sim, second) -
                                   KingdomFood(sim, first));
                int32_t second_pressure = second_hunger * 2 +
                    MaximumI32(0, KingdomFood(sim, first) -
                                   KingdomFood(sim, second));
                int32_t issuer = first_pressure >= second_pressure ?
                                 first : second;
                int32_t recipient = issuer == first ? second : first;
                int32_t pressure = issuer == first ?
                                   first_pressure : second_pressure;
                int32_t issuer_hunger = issuer == first ?
                                         first_hunger : second_hunger;
                if (pressure >= 110 && issuer_hunger >= 25 &&
                    issuer_hunger <= 55 &&
                    sim->kingdoms[issuer].legitimacy >= 30 &&
                    KingdomAverageProsperity(sim, recipient) >=
                        KingdomAverageProsperity(sim, issuer)) {
                    (void)LaunchCourier(
                        sim, CC_COURIER_WAR_DECLARATION,
                        issuer, recipient, 0U);
                    return;
                }
            }
        }
    }
}

static void UpdateThreats(CcSim *sim)
{
    for (int32_t i = 0; i < sim->bandit_count; ++i) {
        CcBanditGroup *bandits = &sim->bandits[i];
        if (sim->current_day % 28 == 0 &&
            bandits->raid_phase == CC_BANDIT_RAID_IDLE &&
            bandits->influence >= 80) {
            CcRoute *best_route = RouteMutable(sim, bandits->route_id);
            int64_t best_score = INT64_MIN;
            for (int32_t route_index = 0;
                 route_index < sim->route_count; ++route_index) {
                CcRoute *candidate = &sim->routes[route_index];
                if (!candidate->smuggler_route) continue;
                const CcSettlement *from = CcSimSettlement(
                    sim, candidate->from_id);
                const CcSettlement *to = CcSimSettlement(
                    sim, candidate->to_id);
                int64_t score = -(int64_t)candidate->security * 4;
                if (from != NULL) {
                    score += from->market_coins / 8 + from->hunger * 2;
                    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
                        score += from->stock[good];
                    }
                }
                if (to != NULL) {
                    score += to->market_coins / 8 + to->hunger * 2;
                    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
                        score += to->stock[good];
                    }
                }
                if (score > best_score) {
                    best_score = score;
                    best_route = candidate;
                }
            }
            if (best_route != NULL && best_route->id != bandits->route_id) {
                bandits->route_id = best_route->id;
                char text[CC_EVENT_TEXT_CAPACITY];
                (void)snprintf(
                    text, sizeof(text),
                    "%s follows the richest night road and takes its tolls with it.",
                    bandits->name);
                (void)PushEvent(
                    sim, CC_EVENT_BANDIT_PRESSURE, bandits->id,
                    best_route->id, LatestLocalCause(sim, best_route->to_id),
                    bandits->influence, text);
            }
        }
        const CcRoute *route = CcSimRoute(sim, bandits->route_id);
        const CcSettlement *a = route != NULL ? CcSimSettlement(sim, route->from_id) : NULL;
        const CcSettlement *b = route != NULL ? CcSimSettlement(sim, route->to_id) : NULL;
        int32_t hunger = (a != NULL ? a->hunger : 0) + (b != NULL ? b->hunger : 0);
        int32_t debt_pressure =
            (a != NULL ? IronLedgerDebtPressure(sim, a->kingdom_id) : 0) +
            (b != NULL ? IronLedgerDebtPressure(sim, b->kingdom_id) : 0);
        int32_t recruits = hunger / 65 + debt_pressure / 100;
        int32_t attrition = bandits->supplies == 0 ? 6 :
                            bandits->supplies < 20 ? 3 : 1;
        bandits->members = ClampI32(
            bandits->members + recruits - attrition, 4, 120);
        bandits->supplies = ClampI32(
            bandits->supplies - MaximumI32(1, bandits->members / 40),
            0, 100);
        bandits->influence = ClampI32(
            (bandits->members + bandits->supplies) / 2 +
            MinimumI32(20, bandits->raids_completed / 3), 0, 100);
        GrowBanditCamp(bandits);
        CcRoute *mutable_route = RouteMutable(sim, bandits->route_id);
        if (mutable_route != NULL && sim->current_day % 28 == 0) {
            mutable_route->security = ClampI32(mutable_route->security -
                (bandits->influence >= 65 ? 2 : bandits->influence >= 45 ? 1 : 0),
                5, 100);
        }
        int32_t level = bandits->influence >= 70 ? 3 :
                        bandits->influence >= 50 ? 2 :
                        bandits->influence >= 30 ? 1 : 0;
        if (level > sim->last_bandit_level[i]) {
            char text[CC_EVENT_TEXT_CAPACITY];
            (void)snprintf(text, sizeof(text),
                           "%s recruits from hungry debtors and unpaid households; road control reaches %d%%.",
                           bandits->name, bandits->influence);
            const CcEvent *loss = LatestEvent(sim, CC_EVENT_SHIPMENT_LOST, 0U,
                                              b != NULL ? b->id : 0U);
            (void)PushEvent(sim, CC_EVENT_BANDIT_PRESSURE, bandits->id,
                            bandits->route_id, loss != NULL ? loss->id : 0U,
                            bandits->influence, text);
        }
        sim->last_bandit_level[i] = level;
        if (sim->current_day % 28 == 0 &&
            bandits->raid_phase == CC_BANDIT_RAID_IDLE &&
            bandits->members >= 12 &&
            (bandits->supplies <= 45 || bandits->influence >= 40)) {
            (void)CcSimLaunchBanditRaid(sim, bandits->id, NULL, 0U);
        }
    }

    for (int32_t i = 0; i < sim->monster_count; ++i) {
        CcMonsterPopulation *monster = &sim->monsters[i];
        CcDungeon *dungeon = NULL;
        for (int32_t d = 0; d < sim->dungeon_count; ++d) {
            if (sim->dungeons[d].id == monster->dungeon_id) dungeon = &sim->dungeons[d];
        }
        int32_t growth = dungeon != NULL && dungeon->state == CC_DUNGEON_DISTURBED ? 3 :
                         dungeon != NULL && dungeon->state == CC_DUNGEON_RESEALED ? -3 : 1;
        monster->population = ClampI32(monster->population + growth -
                                       monster->hunting_pressure / 20, 0, 160);
        monster->hunting_pressure = ClampI32(monster->hunting_pressure - 4, 0, 100);
        if (dungeon != NULL) {
            int32_t pressure_delta = dungeon->state == CC_DUNGEON_DISTURBED ? 2 :
                                     dungeon->state == CC_DUNGEON_SMUGGLER_ROUTE ? 1 :
                                     dungeon->state == CC_DUNGEON_RESEALED ? -4 : -1;
            dungeon->regional_pressure = ClampI32(dungeon->regional_pressure +
                                                  pressure_delta, 0, 100);
        }
        monster->pressure = ClampI32(monster->population / 2 +
                                     (dungeon != NULL ? dungeon->regional_pressure / 2 : 0),
                                     0, 100);
        int32_t level = monster->pressure >= 70 ? 3 :
                        monster->pressure >= 50 ? 2 :
                        monster->pressure >= 30 ? 1 : 0;
        if (level > sim->last_monster_level[i]) {
            char text[CC_EVENT_TEXT_CAPACITY];
            const CcSettlement *place = dungeon != NULL ?
                CcSimSettlement(sim, dungeon->settlement_id) : NULL;
            (void)snprintf(text, sizeof(text), "%s breach another shaft beneath %s.",
                           monster->name, place != NULL ? place->name : "the frontier");
            const CcEvent *change = dungeon != NULL ?
                LatestEvent(sim, CC_EVENT_DUNGEON_CHANGED, dungeon->id,
                            dungeon->settlement_id) : NULL;
            (void)PushEvent(sim, CC_EVENT_MONSTER_PRESSURE, monster->id,
                            monster->dungeon_id, change != NULL ? change->id : 0U,
                            monster->pressure, text);
        }
        sim->last_monster_level[i] = level;
    }
}

static int32_t ActiveShipmentsForKingdom(const CcSim *sim, CcId kingdom_id)
{
    int32_t count = 0;
    for (int32_t i = 0; i < sim->shipment_count; ++i) {
        const CcShipment *shipment = &sim->shipments[i];
        const CcSettlement *origin = CcSimSettlement(sim, shipment->origin_id);
        if (shipment->status == CC_SHIPMENT_TRAVELLING && origin != NULL &&
            origin->kingdom_id == kingdom_id) count += 1;
    }
    return count;
}

static CcSettlement *RepairBaseForKingdom(CcSim *sim,
                                          const CcRoute *route,
                                          CcId kingdom_id)
{
    CcSettlement *from = CcSimSettlementMutable(sim, route->from_id);
    CcSettlement *to = CcSimSettlementMutable(sim, route->to_id);
    CcSettlement *best = NULL;
    if (from != NULL && !CcSettlementIsAbandoned(from) &&
        from->kingdom_id == kingdom_id) best = from;
    if (to != NULL && !CcSettlementIsAbandoned(to) &&
        to->kingdom_id == kingdom_id &&
        (best == NULL ||
         to->stock[CC_GOOD_TOOLS] + to->stock[CC_GOOD_FOOD] >
         best->stock[CC_GOOD_TOOLS] + best->stock[CC_GOOD_FOOD])) {
        best = to;
    }
    return best;
}

static int32_t RouteRecoveryScore(const CcSim *sim, const CcRoute *route,
                                  CcId kingdom_id)
{
    if (!route->closed ||
        (CcSimRouteCrossesWarBorder(sim, route->id) &&
         !route->smuggler_route)) return -1;
    const CcSettlement *from = CcSimSettlement(sim, route->from_id);
    const CcSettlement *to = CcSimSettlement(sim, route->to_id);
    if (CcSettlementIsAbandoned(from) || CcSettlementIsAbandoned(to)) {
        return -1;
    }
    bool touches_kingdom =
        (from != NULL && from->kingdom_id == kingdom_id) ||
        (to != NULL && to->kingdom_id == kingdom_id);
    if (!touches_kingdom) return -1;
    int32_t hunger = MaximumI32(from != NULL ? from->hunger : 0,
                                to != NULL ? to->hunger : 0);
    int32_t food_surplus = 0;
    if (from != NULL) {
        food_surplus = MaximumI32(
            food_surplus,
            from->stock[CC_GOOD_FOOD] - from->reserve_target[CC_GOOD_FOOD]);
    }
    if (to != NULL) {
        food_surplus = MaximumI32(
            food_surplus,
            to->stock[CC_GOOD_FOOD] - to->reserve_target[CC_GOOD_FOOD]);
    }
    return hunger * 4 + MinimumI32(100, food_surplus) * 2 +
           (100 - route->condition);
}

static void AdvanceRoadsideRecovery(CcSim *sim, CcRoute *route)
{
    if (sim == NULL || route == NULL || !route->closed ||
        (CcSimRouteCrossesWarBorder(sim, route->id) &&
         !route->smuggler_route) || sim->current_day % 112 != 0) return;
    CcSettlement *from = CcSimSettlementMutable(sim, route->from_id);
    CcSettlement *to = CcSimSettlementMutable(sim, route->to_id);
    if (from == NULL || to == NULL || CcSettlementIsAbandoned(from) ||
        CcSettlementIsAbandoned(to)) return;

    int32_t distress = MaximumI32(from->hunger, to->hunger);
    CcSettlement *labor_base = from->population >= to->population ?
                               from : to;
    CcSettlement *supplier =
        from->stock[CC_GOOD_FOOD] + from->stock[CC_GOOD_TOOLS] >=
        to->stock[CC_GOOD_FOOD] + to->stock[CC_GOOD_TOOLS] ? from : to;
    if (labor_base->population < 220 ||
        supplier->stock[CC_GOOD_FOOD] < 4 ||
        supplier->stock[CC_GOOD_TOOLS] < 1) return;
    supplier->stock[CC_GOOD_FOOD] -= 4;
    supplier->stock[CC_GOOD_TOOLS] -= 1;
    int32_t effort = 6 + distress / 20 +
                     (route->smuggler_route ? 1 : 0);
    route->condition = ClampI32(route->condition + effort, 0, 100);
    labor_base->population = MaximumI32(
        0, labor_base->population - MaximumI32(1,
        labor_base->population / 500));
    if (route->condition < 45) return;

    route->closed = false;
    route->security = ClampI32(route->security + 4, 0, 100);
    SupersedeTargetSituations(sim, CC_SITUATION_ROUTE_REPAIR, route->id);
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(
        text, sizeof(text),
        "%s and %s reopen their road after a hard season of unpaid labor.",
        from->name, to->name);
    (void)PushEvent(
        sim, CC_EVENT_KINGDOM_ACTION, route->id, route->to_id,
        LatestLocalCause(sim, route->to_id), route->condition, text);
}

static void UpdateRoutesAndGovernments(CcSim *sim)
{
    if (sim->current_day % 28 == 0) {
        CcRoute *new_night_road = NULL;
        int32_t worst_distress = 0;
        for (int32_t route_index = 0;
             route_index < sim->route_count; ++route_index) {
            CcRoute *route = &sim->routes[route_index];
            if (route->smuggler_route ||
                !CcSimRouteCrossesWarBorder(sim, route->id)) continue;
            const CcSettlement *from = CcSimSettlement(sim, route->from_id);
            const CcSettlement *to = CcSimSettlement(sim, route->to_id);
            if (CcSettlementIsAbandoned(from) ||
                CcSettlementIsAbandoned(to)) continue;
            int32_t distress = MaximumI32(
                from != NULL ? from->hunger : 0,
                to != NULL ? to->hunger : 0);
            int32_t debt_pressure = MaximumI32(
                from != NULL ? IronLedgerDebtPressure(
                    sim, from->kingdom_id) : 0,
                to != NULL ? IronLedgerDebtPressure(
                    sim, to->kingdom_id) : 0);
            if (debt_pressure >= 80) {
                distress = MaximumI32(distress, 65 + (debt_pressure - 80));
            }
            if (distress < 65 || distress <= worst_distress) continue;
            worst_distress = distress;
            new_night_road = route;
        }
        if (new_night_road != NULL) {
            new_night_road->smuggler_route = true;
            new_night_road->closed = false;
            new_night_road->condition = MaximumI32(
                34, new_night_road->condition);
            for (int32_t map_index = 0;
                 map_index < sim->map_count; ++map_index) {
                if (sim->maps[map_index].route_id == new_night_road->id) {
                    sim->maps[map_index].contraband = true;
                }
            }
            const CcSettlement *from = CcSimSettlement(
                sim, new_night_road->from_id);
            const CcSettlement *to = CcSimSettlement(
                sim, new_night_road->to_id);
            char text[CC_EVENT_TEXT_CAPACITY];
            (void)snprintf(
                text, sizeof(text),
                "Hunger, debt, and sanctions open a night road between %s and %s.",
                from != NULL ? from->name : "one border town",
                to != NULL ? to->name : "the other");
            (void)PushEvent(
                sim, CC_EVENT_KINGDOM_ACTION, new_night_road->id,
                new_night_road->to_id,
                LatestLocalCause(sim, new_night_road->to_id),
                worst_distress, text);
        }
    }
    for (int32_t route_index = 0; route_index < sim->route_count; ++route_index) {
        CcRoute *route = &sim->routes[route_index];
        if (sim->current_day % 28 == 0) {
            AdvanceRoadsideRecovery(sim, route);
        }
        if (!route->closed && sim->current_day % 28 == 0) {
            const CcSettlement *route_from = CcSimSettlement(
                sim, route->from_id);
            const CcSettlement *route_to = CcSimSettlement(
                sim, route->to_id);
            bool locally_maintained = !route->smuggler_route &&
                route_from != NULL && route_to != NULL &&
                !CcSettlementIsAbandoned(route_from) &&
                !CcSettlementIsAbandoned(route_to) &&
                MaximumI32(route_from->hunger, route_to->hunger) < 30 &&
                (route_from->prosperity + route_to->prosperity) / 2 >= 45;
            int32_t decay = locally_maintained ? 0 :
                            route->smuggler_route ? 2 : 1;
            route->condition = ClampI32(route->condition - decay, 0, 100);
            CcBanditGroup *bandits = BanditsOnRoute(sim, route->id);
            if (locally_maintained &&
                (bandits == NULL || bandits->influence < 30)) {
                route->security = ClampI32(route->security + 1, 0, 100);
            }
            if (route->condition < 22) {
                route->closed = true;
                char text[CC_EVENT_TEXT_CAPACITY];
                const CcSettlement *from = CcSimSettlement(sim, route->from_id);
                const CcSettlement *to = CcSimSettlement(sim, route->to_id);
                (void)snprintf(text, sizeof(text),
                               "The %s-%s road fails after years of deferred maintenance.",
                               from != NULL ? from->name : "western",
                               to != NULL ? to->name : "eastern");
                (void)PushEvent(sim, CC_EVENT_ROUTE_DECAY, route->id, route->to_id,
                                LatestLocalCause(sim, route->to_id), route->condition, text);
            }
        }
    }

    for (int32_t kingdom_index = 0; kingdom_index < sim->kingdom_count; ++kingdom_index) {
        CcKingdom *kingdom = &sim->kingdoms[kingdom_index];
        int32_t settlements = 0;
        int32_t hunger_sum = 0;
        int32_t prosperity_sum = 0;
        int32_t war_crisis_max = 0;
        CcSettlement *worst = NULL;
        for (int32_t i = 0; i < sim->settlement_count; ++i) {
            CcSettlement *settlement = &sim->settlements[i];
            if (settlement->kingdom_id != kingdom->id ||
                CcSettlementIsAbandoned(settlement)) continue;
            settlements += 1;
            hunger_sum += settlement->hunger;
            prosperity_sum += settlement->prosperity;
            if (worst == NULL || settlement->hunger > worst->hunger) worst = settlement;
            CcMoney taxable = settlement->market_coins > 140 ?
                              settlement->market_coins - 140 : 0;
            CcMoney tax_due = taxable > 0 ? 1 + taxable / 8 : 0;
            if (tax_due > 32) tax_due = 32;
            CcMoney tax_paid = settlement->market_coins < tax_due ?
                               settlement->market_coins : tax_due;
            settlement->market_coins -= tax_paid;
            kingdom->treasury += tax_paid;
        }
        if (sim->current_day % 28 == 0 &&
            kingdom->iron_ledger_debt > 0) {
            CcMoney due = MaximumI32(
                4, kingdom->iron_ledger_debt / 24 > INT32_MAX ? INT32_MAX :
                   (int32_t)(kingdom->iron_ledger_debt / 24));
            if (due > 24) due = 24;
            if (due > kingdom->iron_ledger_debt) {
                due = kingdom->iron_ledger_debt;
            }
            CcMoney treasury_payment = kingdom->treasury > 60 ?
                MinimumI32((int32_t)due,
                           (int32_t)(kingdom->treasury - 60)) : 0;
            kingdom->treasury -= treasury_payment;
            CcMoney payment = treasury_payment;
            for (int32_t i = 0;
                 i < sim->settlement_count && payment < due; ++i) {
                CcSettlement *place = &sim->settlements[i];
                if (place->kingdom_id != kingdom->id ||
                    CcSettlementIsAbandoned(place) ||
                    place->market_coins <= 80) continue;
                CcMoney available = place->market_coins - 80;
                CcMoney taken = MinimumI32(
                    (int32_t)(due - payment), (int32_t)available);
                place->market_coins -= taken;
                payment += taken;
            }
            kingdom->iron_ledger_debt -= payment;
            sim->iron_ledger_reserve += payment;
            if (payment > 0) {
            char text[CC_EVENT_TEXT_CAPACITY];
            (void)snprintf(
                text, sizeof(text),
                "%s repays %d crowns into the monasteries' copied ledger.",
                kingdom->name, (int32_t)payment);
            const CcEvent *loan = LatestEvent(
                sim, CC_EVENT_IRON_LEDGER_LOAN, kingdom->id, 0U);
            (void)PushEvent(
                sim, CC_EVENT_IRON_LEDGER_REPAID, kingdom->id, kingdom->id,
                loan != NULL ? loan->id : 0U, (int32_t)payment, text);
            }
        }
        if (sim->current_day % (10 * 364) == 0 &&
            kingdom->iron_ledger_debt > 0) {
            bool can_pay = kingdom->treasury > 60;
            for (int32_t i = 0;
                 i < sim->settlement_count && !can_pay; ++i) {
                const CcSettlement *place = &sim->settlements[i];
                can_pay = place->kingdom_id == kingdom->id &&
                    !CcSettlementIsAbandoned(place) &&
                    place->market_coins > 80;
            }
            if (!can_pay) {
                CcMoney repudiated = kingdom->iron_ledger_debt;
                kingdom->iron_ledger_debt = 0;
                kingdom->legitimacy = ClampI32(
                    kingdom->legitimacy - 25, 0, 100);
                if (worst != NULL) RemoveBurnedService(worst);
                CcFaction *commons = FactionFor(
                    sim, kingdom->id, CC_FACTION_COMMONS);
                CcFaction *crown = FactionFor(
                    sim, kingdom->id, CC_FACTION_CROWN);
                if (commons != NULL) commons->support = ClampI32(
                    commons->support + 18, 0, 100);
                if (crown != NULL) crown->support = ClampI32(
                    crown->support - 18, 0, 100);
                char text[CC_EVENT_TEXT_CAPACITY];
                (void)snprintf(
                    text, sizeof(text),
                    "%s repudiates %d crowns of monastery debt; a royal service closes and the commons rally.",
                    kingdom->name,
                    repudiated > INT32_MAX ? INT32_MAX :
                        (int32_t)repudiated);
                (void)PushEvent(
                    sim, CC_EVENT_KINGDOM_ACTION, kingdom->id,
                    worst != NULL ? worst->id : kingdom->id,
                    LatestLocalCause(sim, kingdom->id),
                    repudiated > INT32_MAX ? INT32_MAX :
                        (int32_t)repudiated, text);
            }
        }
        if (sim->current_day % 28 == 0 &&
            kingdom->iron_ledger_debt == 0 &&
            kingdom->treasury > 320 && sim->iron_ledger_reserve < 480) {
            CcMoney deposit = MinimumI32(
                12, (int32_t)(kingdom->treasury - 320));
            deposit = MinimumI32(
                (int32_t)deposit, (int32_t)(480 - sim->iron_ledger_reserve));
            kingdom->treasury -= deposit;
            sim->iron_ledger_reserve += deposit;
            char text[CC_EVENT_TEXT_CAPACITY];
            (void)snprintf(
                text, sizeof(text),
                "%s deposits %d surplus crowns in the monasteries' copied vaults.",
                kingdom->name, (int32_t)deposit);
            (void)PushEvent(
                sim, CC_EVENT_KINGDOM_ACTION, kingdom->id, kingdom->id,
                LatestLocalCause(sim, kingdom->id), (int32_t)deposit, text);
        }
        for (int32_t i = 0; i < sim->settlement_count; ++i) {
            CcSettlement *place = &sim->settlements[i];
            if (place->kingdom_id != kingdom->id ||
                CcSettlementIsAbandoned(place)) continue;
            int32_t target = EffectiveReserveTarget(
                sim, place, CC_GOOD_FOOD);
            int32_t projected = place->stock[CC_GOOD_FOOD] +
                CcSimIncomingGood(sim, place->id, CC_GOOD_FOOD);
            CcMoney buying_floor = place->price[CC_GOOD_FOOD] * 16;
            if (projected >= target / 2 ||
                place->market_coins >= buying_floor) continue;
            CcMoney grant = buying_floor - place->market_coins;
            if (grant > 40) grant = 40;
            if (grant > kingdom->treasury) grant = kingdom->treasury;
            kingdom->treasury -= grant;
            place->market_coins += grant;
        }
        for (int32_t i = 0; i < sim->settlement_count; ++i) {
            CcSettlement *front = &sim->settlements[i];
            if (front->kingdom_id != kingdom->id ||
                CcSettlementIsAbandoned(front) || !IsWarSeat(front)) continue;
            int32_t burden = CcSimWarBurdenAtSettlement(sim, front->id);
            int32_t wage = WarWeeklyWage(sim, front);
            if (burden < 20 || wage < 1) continue;
            int32_t food_need = WarWeeklyNeed(sim, front, CC_GOOD_FOOD);
            int32_t tool_need = WarWeeklyNeed(sim, front, CC_GOOD_TOOLS);
            int32_t weapon_need = WarWeeklyNeed(
                sim, front, CC_GOOD_WEAPONS);
            CcMoney desired_chest = wage * 4 +
                (CcMoney)food_need * front->price[CC_GOOD_FOOD] * 2 +
                (CcMoney)tool_need * front->price[CC_GOOD_TOOLS] * 2 +
                (CcMoney)weapon_need * front->price[CC_GOOD_WEAPONS] * 2;
            desired_chest = desired_chest > 120 ? 120 : desired_chest;
            CcMoney transfer = front->war_chest < desired_chest ?
                               desired_chest - front->war_chest : 0;
            if (transfer > kingdom->treasury) transfer = kingdom->treasury;
            if (transfer > 0) {
                kingdom->treasury -= transfer;
                front->war_chest += transfer;
                char text[CC_EVENT_TEXT_CAPACITY];
                (void)snprintf(text, sizeof(text),
                               "%s moves %" PRId64
                               " crowns from its treasury into %s's war chest.",
                               kingdom->name, transfer, front->name);
                (void)PushEvent(
                    sim, CC_EVENT_WAR_CHEST_FUNDED, kingdom->id, front->id,
                    LatestLocalCause(sim, front->id), (int32_t)transfer, text);
            }
            CcMoney wage_paid = front->war_chest < wage ?
                                front->war_chest : wage;
            front->war_chest -= wage_paid;
            front->market_coins += wage_paid;
            int32_t crisis = CcSimWarSupplyCrisisAtSettlement(sim, front->id);
            if (wage_paid < wage) crisis = MaximumI32(crisis, 70);
            if (crisis > war_crisis_max) war_crisis_max = crisis;
            if (crisis >= 50) {
                front->hunger = ClampI32(front->hunger + 2, 0, 100);
                front->security = ClampI32(front->security - 2, 0, 100);
                kingdom->legitimacy = ClampI32(
                    kingdom->legitimacy - 2, 0, 100);
                if (sim->current_day % 28 == 0) {
                    char text[CC_EVENT_TEXT_CAPACITY];
                    (void)snprintf(text, sizeof(text),
                                   "%s lacks pay, wheat, tools, or weapons for its garrison; supply crisis reaches %d.",
                                   front->name, crisis);
                    (void)PushEvent(
                        sim, CC_EVENT_WAR_SUPPLY_SHORTAGE, front->id,
                        front->id, LatestLocalCause(sim, front->id),
                        crisis, text);
                }
            }
        }
        int32_t average_hunger = settlements > 0 ? hunger_sum / settlements : 0;
        int32_t average_prosperity = settlements > 0 ? prosperity_sum / settlements : 0;
        int32_t legitimacy_change = average_hunger > 55 ? -2 :
                                    average_hunger > 35 ? -1 :
                                    average_hunger < 25 &&
                                    average_prosperity > 25 &&
                                    war_crisis_max < 50 &&
                                    kingdom->legitimacy < 75 ? 1 : 0;
        kingdom->legitimacy = ClampI32(
            kingdom->legitimacy + legitimacy_change, 0, 100);

        if (sim->current_day % (5 * 364) == 0) {
            for (int32_t i = 0; i < sim->settlement_count; ++i) {
                CcSettlement *place = &sim->settlements[i];
                if (place->kingdom_id != kingdom->id ||
                    CcSettlementIsAbandoned(place) ||
                    (place->hunger < 60 && place->prosperity > 10)) continue;
                int32_t services_before = CcSettlementServiceCount(place);
                RemoveBurnedService(place);
                if (CcSettlementServiceCount(place) == services_before) continue;
                char text[CC_EVENT_TEXT_CAPACITY];
                (void)snprintf(
                    text, sizeof(text),
                    "%s loses a public service after five years of hunger and neglect.",
                    place->name);
                (void)PushEvent(
                    sim, CC_EVENT_KINGDOM_ACTION, place->id, place->id,
                    LatestLocalCause(sim, place->id), -1, text);
            }
        }

        CcFaction *crown = FactionFor(sim, kingdom->id, CC_FACTION_CROWN);
        CcFaction *guild = FactionFor(sim, kingdom->id, CC_FACTION_GUILD);
        CcFaction *commons = FactionFor(sim, kingdom->id, CC_FACTION_COMMONS);
        int32_t shipments = ActiveShipmentsForKingdom(sim, kingdom->id);
        if (crown != NULL) crown->support = ClampI32(crown->support +
            (average_hunger < 20 ? 1 : -2), 0, 100);
        if (guild != NULL) guild->support = ClampI32(guild->support +
            (shipments > 0 ? 1 : -1), 0, 100);
        if (commons != NULL) commons->support = ClampI32(commons->support +
            (average_hunger > 25 ? 2 : -1), 0, 100);
        CcFaction *factions[3] = {crown, guild, commons};
        for (int32_t i = 0; i < 3; ++i) {
            if (factions[i] == NULL) continue;
            int32_t difference = factions[i]->support - factions[i]->power;
            factions[i]->power = ClampI32(factions[i]->power +
                (AbsoluteI32(difference) > 3 ? (difference > 0 ? 1 : -1) : 0), 0, 100);
        }
        if (commons != NULL && crown != NULL &&
            commons->support > 72 && crown->support < 38) {
            int32_t faction_change = kingdom->legitimacy > 20 ? -1 :
                                     average_hunger < 55 &&
                                     (shipments > 0 ||
                                      average_prosperity > 20) ? 1 : 0;
            kingdom->legitimacy = ClampI32(
                kingdom->legitimacy + faction_change, 0, 100);
        }

        if (sim->current_day % 28 == 0 && worst != NULL && worst->hunger >= 38 &&
            kingdom->treasury >= 28) {
            kingdom->treasury -= 28;
            worst->market_coins += 28;
            kingdom->legitimacy = ClampI32(kingdom->legitimacy + 2, 0, 100);
            char text[CC_EVENT_TEXT_CAPACITY];
            (void)snprintf(text, sizeof(text),
                           "%s moves 28 crowns into %s's market to buy real grain shipments.",
                           kingdom->name, worst->name);
            const CcEvent *shortage = LatestEvent(sim, CC_EVENT_SHORTAGE,
                            worst->id, worst->id);
            (void)PushEvent(sim, CC_EVENT_KINGDOM_ACTION, kingdom->id, worst->id,
                            shortage != NULL ? shortage->id : 0U, 28, text);
        }

        if (sim->current_day % 28 == 0) {
            CcRoute *best_route = NULL;
            CcSettlement *repair_base = NULL;
            int32_t best_score = -1;
            for (int32_t route_index = 0; route_index < sim->route_count; ++route_index) {
                CcRoute *route = &sim->routes[route_index];
                int32_t score = RouteRecoveryScore(sim, route, kingdom->id);
                if (score <= best_score) continue;
                CcSettlement *base = RepairBaseForKingdom(
                    sim, route, kingdom->id);
                if (base == NULL) continue;
                best_score = score;
                best_route = route;
                repair_base = base;
            }
            bool crown_funded = best_route != NULL &&
                                kingdom->treasury >= 24;
            bool locally_funded = best_route != NULL && !crown_funded &&
                                  repair_base->stock[CC_GOOD_TOOLS] >= 1 &&
                                  repair_base->stock[CC_GOOD_FOOD] >= 4;
            if (crown_funded || locally_funded) {
                if (crown_funded) {
                    kingdom->treasury -= 24;
                    repair_base->market_coins += 24;
                } else {
                    repair_base->stock[CC_GOOD_TOOLS] -= 1;
                    repair_base->stock[CC_GOOD_FOOD] -= 4;
                }
                best_route->condition = ClampI32(
                    best_route->condition + 25, 0, 100);
                if (best_route->condition >= 45) {
                    best_route->closed = false;
                    best_route->security = ClampI32(
                        best_route->security + 6, 0, 100);
                    SupersedeTargetSituations(
                        sim, CC_SITUATION_ROUTE_REPAIR, best_route->id);
                }
                char text[CC_EVENT_TEXT_CAPACITY];
                (void)snprintf(text, sizeof(text),
                               "%s uses %s; road condition rises to %d.",
                               crown_funded ? kingdom->name : repair_base->name,
                               crown_funded ? "paid crews" : "local Food and Tools",
                               best_route->condition);
                (void)PushEvent(
                    sim, CC_EVENT_KINGDOM_ACTION, kingdom->id, best_route->id,
                    LatestLocalCause(sim, repair_base->id),
                    best_route->condition, text);
            }
        }

        if (sim->current_day % 28 == 0 && kingdom->treasury >= 12) {
            for (int32_t route_index = 0; route_index < sim->route_count; ++route_index) {
                CcRoute *route = &sim->routes[route_index];
                CcSettlement *to = CcSimSettlementMutable(sim, route->to_id);
                if (route->closed || route->condition >= 58 || to == NULL ||
                    CcSettlementIsAbandoned(to) ||
                    to->kingdom_id != kingdom->id) continue;
                kingdom->treasury -= 12;
                to->market_coins += 12;
                route->condition = ClampI32(route->condition + 16, 0, 100);
                route->security = ClampI32(route->security + 2, 0, 100);
                if (route->condition < 45) {
                    char text[CC_EVENT_TEXT_CAPACITY];
                    (void)snprintf(text, sizeof(text),
                                   "%s maintains a strategic road before it fails.",
                                   kingdom->name);
                    (void)PushEvent(sim, CC_EVENT_KINGDOM_ACTION, kingdom->id, route->id,
                                    0U, route->condition, text);
                }
                break;
            }
        }

        if (sim->current_day % 28 == 0) {
            CcMonsterPopulation *worst_monster = NULL;
            CcDungeon *worst_dungeon = NULL;
            for (int32_t monster_index = 0; monster_index < sim->monster_count; ++monster_index) {
                CcMonsterPopulation *monster = &sim->monsters[monster_index];
                CcDungeon *dungeon = DungeonByIdMutable(sim, monster->dungeon_id);
                CcSettlement *place = dungeon != NULL ?
                    CcSimSettlementMutable(sim, dungeon->settlement_id) : NULL;
                if (place == NULL || CcSettlementIsAbandoned(place) ||
                    place->kingdom_id != kingdom->id ||
                    monster->pressure < 72) continue;
                if (worst_monster == NULL || monster->pressure > worst_monster->pressure) {
                    worst_monster = monster;
                    worst_dungeon = dungeon;
                }
            }
            if (worst_monster != NULL && worst_dungeon != NULL) {
                CcMoney hunt_cost = kingdom->treasury >= 18 ? 18 : kingdom->treasury;
                kingdom->treasury -= hunt_cost;
                CcSettlement *hunt_market = CcSimSettlementMutable(
                    sim, worst_dungeon->settlement_id);
                if (hunt_market != NULL) {
                    hunt_market->market_coins += hunt_cost;
                }
                worst_monster->hunting_pressure = ClampI32(
                    worst_monster->hunting_pressure + 36, 0, 100);
                worst_monster->population = ClampI32(worst_monster->population - 12, 0, 160);
                worst_monster->pressure = ClampI32(worst_monster->pressure - 18, 0, 100);
                worst_dungeon->regional_pressure = ClampI32(
                    worst_dungeon->regional_pressure - 9, 0, 100);
                char text[CC_EVENT_TEXT_CAPACITY];
                (void)snprintf(text, sizeof(text),
                               "%s musters a deep hunt; %s pressure falls to %d.",
                               guild != NULL ? guild->name : kingdom->name,
                               worst_monster->name, worst_monster->pressure);
                const CcEvent *pressure = LatestEvent(sim, CC_EVENT_MONSTER_PRESSURE,
                                                     worst_monster->id,
                                                     worst_monster->dungeon_id);
                (void)PushEvent(sim, CC_EVENT_KINGDOM_ACTION, kingdom->id,
                                worst_dungeon->id,
                                pressure != NULL ? pressure->id : 0U,
                                worst_monster->pressure, text);
            }
        }

        if (sim->current_day % 28 == 0) {
            CcFaction *leader = crown;
            if (guild != NULL && (leader == NULL || guild->support > leader->support)) leader = guild;
            if (commons != NULL && (leader == NULL || commons->support > leader->support)) leader = commons;
            if (leader != NULL) {
                char text[CC_EVENT_TEXT_CAPACITY];
                (void)snprintf(text, sizeof(text),
                               "%s leads public support in %s at %d%%.",
                               leader->name, kingdom->name, leader->support);
                (void)PushEvent(sim, CC_EVENT_FACTION_SHIFT, leader->id, kingdom->id,
                                0U, leader->support, text);
            }
        }
    }
}

static bool HorseNameUsed(const CcSim *sim, const char *name)
{
    for (int32_t i = 0; i < CcSimHorseCount(sim); ++i) {
        const CcHorse *horse = CcSimHorseAt(sim, i);
        if (horse != NULL && strcmp(horse->name, name) == 0) return true;
    }
    return false;
}

static void NameFoal(const CcSim *sim, CcHorse *foal)
{
    static const char *names[] = {
        "Sorrel", "Rowan", "Tansy", "Flint", "Clover", "Rook",
        "Juniper", "Sable", "Thistle", "Lark", "Briar", "Wren"
    };
    const int32_t count = (int32_t)(sizeof(names) / sizeof(names[0]));
    int32_t start = (int32_t)((foal->id & CC_ID_SERIAL_MASK) %
                              (uint64_t)count);
    for (int32_t offset = 0; offset < count; ++offset) {
        const char *candidate = names[(start + offset) % count];
        if (!HorseNameUsed(sim, candidate)) {
            CopyName(foal->name, candidate);
            return;
        }
    }
    (void)snprintf(foal->name, sizeof(foal->name), "Foal-%" PRIu64,
                   foal->id & UINT64_C(0xffff));
}

static CcId HorseFoalingLocation(const CcSim *sim, const CcHorse *mare)
{
    if (mare->stable_settlement_id != 0U) {
        return mare->stable_settlement_id;
    }
    for (int32_t i = 0; i < CC_CARRIAGE_HORSE_COUNT; ++i) {
        if (sim->horse_team[i].id == mare->id && !sim->journey.active) {
            return sim->player.location_id;
        }
    }
    return 0U;
}

static void TryBirthFoal(CcSim *sim, CcHorse *mare)
{
    if (mare->pregnant_by_id == 0U ||
        mare->pregnancy_days_remaining > 0) return;
    CcId location_id = HorseFoalingLocation(sim, mare);
    const CcSettlement *place = CcSimSettlement(sim, location_id);
    if (!CcSettlementHasService(place, CC_SERVICE_STABLE)) return;
    if (sim->stable_horse_count >= CC_MAX_STABLE_HORSES) {
        mare->pregnancy_days_remaining = 1;
        return;
    }
    const CcHorse *sire = CcSimHorse(sim, mare->pregnant_by_id);
    if (sire == NULL) {
        mare->pregnant_by_id = 0U;
        return;
    }
    CcHorse *foal = &sim->stable_horses[sim->stable_horse_count];
    sim->stable_horse_count += 1;
    *foal = (CcHorse){
        .id = NextId(sim, CC_ENTITY_HORSE),
        .health = ClampI32((mare->health + sire->health) / 2, 55, 100),
        .sex = ((sim->next_entity_serial - 1U) & UINT64_C(1)) != 0U ?
            CC_HORSE_MARE : CC_HORSE_STALLION,
        .sire_id = sire->id,
        .dam_id = mare->id,
        .stable_settlement_id = location_id,
        .strength = ClampI32(
            (mare->strength + sire->strength) / 2 +
                (int32_t)((foal->id >> 1U) % 11U) - 5,
            1, 100),
        .temperament = ClampI32(
            (mare->temperament + sire->temperament) / 2 +
                (int32_t)((foal->id >> 5U) % 11U) - 5,
            1, 100),
        .hardiness = ClampI32(
            (mare->hardiness + sire->hardiness) / 2 +
                (int32_t)((foal->id >> 9U) % 11U) - 5,
            1, 100)
    };
    NameFoal(sim, foal);
    mare->pregnant_by_id = 0U;
    mare->pregnancy_days_remaining = 0;
    mare->breeding_cooldown_days = 180;
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(
        text, sizeof(text),
        "%.18s gives birth to %.18s, a %.8s by %.18s, at %.18s.",
        mare->name, foal->name, CcHorseSexName(foal->sex),
        sire->name, place->name);
    (void)PushEvent(sim, CC_EVENT_FOAL_BORN, foal->id, location_id,
                    0U, foal->training, text);
}

static void AdvanceHorseLifecycle(CcSim *sim, CcHorse *horse)
{
    horse->age_days += 1;
    horse->breeding_cooldown_days = MaximumI32(
        0, horse->breeding_cooldown_days - 1);
    if (horse->pregnancy_days_remaining > 0) {
        horse->pregnancy_days_remaining -= 1;
    }
    TryBirthFoal(sim, horse);
}

static void AdvanceHorseTeam(CcSim *sim)
{
    if (sim->schema_version < 14U) return;
    bool travelling = sim->journey.active;
    const CcRoute *route = travelling ?
        CcSimRoute(sim, sim->journey.route_id) : NULL;
    const CcSettlement *place = !travelling ?
        CcSimSettlement(sim, sim->player.location_id) : NULL;
    bool stable_care = CcSettlementHasService(place, CC_SERVICE_STABLE);
    int32_t cargo_strain = sim->player.cargo_capacity > 0 ?
        CcPlayerCargoUsed(&sim->player) * 4 /
            sim->player.cargo_capacity : 0;
    int32_t road_strain = route != NULL ?
        MaximumI32(0, 60 - route->condition) / 12 : 0;
    int32_t pace_strain = travelling ?
        sim->journey.pace == CC_JOURNEY_PACE_PUSH ? 5 :
        sim->journey.pace == CC_JOURNEY_PACE_CAREFUL ? -3 : 0 : 0;

    if (travelling && route != NULL) {
        int32_t road_wear = MaximumI32(0, 70 - route->condition) / 28;
        int32_t pace_wear =
            sim->journey.pace == CC_JOURNEY_PACE_PUSH ? 3 :
            sim->journey.pace == CC_JOURNEY_PACE_STEADY ? 1 : -1;
        sim->carriage.condition = ClampI32(
            sim->carriage.condition -
                MaximumI32(0, pace_wear + road_wear),
            0, 100);
    }

    bool weekly_feed = !travelling && sim->current_day % 7 == 0;
    bool feed_available = weekly_feed && stable_care && place != NULL &&
        place->stock[CC_GOOD_FOOD] > 0;
    if (feed_available) {
        CcSettlement *mutable_place = CcSimSettlementMutable(
            sim, place->id);
        mutable_place->stock[CC_GOOD_FOOD] -= 1;
    }

    int32_t boarded_at_start = sim->stable_horse_count;
    for (int32_t i = 0; i < CC_CARRIAGE_HORSE_COUNT; ++i) {
        CcHorse *horse = &sim->horse_team[i];
        AdvanceHorseLifecycle(sim, horse);
        if (travelling) {
            int32_t horse_cargo_strain = sim->schema_version >= 15U ?
                cargo_strain * MaximumI32(50, 150 - horse->strength) / 100 :
                cargo_strain;
            int32_t strain = 7 + horse_cargo_strain + road_strain +
                             pace_strain -
                             horse->hardiness / 25;
            horse->fatigue = ClampI32(
                horse->fatigue + MaximumI32(3, strain), 0, 100);
            horse->hunger = ClampI32(horse->hunger + 2, 0, 100);
        } else {
            horse->fatigue = ClampI32(
                horse->fatigue - (stable_care ? 10 : 4), 0, 100);
            if (weekly_feed) {
                horse->hunger = ClampI32(
                    horse->hunger + (feed_available ? -24 : 12), 0, 100);
            }
        }
        if (horse->fatigue >= 88 || horse->hunger >= 80) {
            horse->health = ClampI32(horse->health - 2, 1, 100);
        } else if (!travelling && stable_care && horse->hunger < 45) {
            horse->health = ClampI32(horse->health + 1, 1, 100);
        }
    }

    for (int32_t i = 0; i < boarded_at_start; ++i) {
        CcHorse *horse = &sim->stable_horses[i];
        AdvanceHorseLifecycle(sim, horse);
        CcSettlement *stable = CcSimSettlementMutable(
            sim, horse->stable_settlement_id);
        bool cared_for = CcSettlementHasService(stable, CC_SERVICE_STABLE);
        bool fed = cared_for && sim->current_day % 7 == 0 &&
                   stable->stock[CC_GOOD_FOOD] > 0;
        if (fed) stable->stock[CC_GOOD_FOOD] -= 1;
        if (cared_for) {
            horse->fatigue = ClampI32(horse->fatigue - 12, 0, 100);
            horse->health = ClampI32(horse->health + 1, 1, 100);
        }
        if (sim->current_day % 7 == 0) {
            horse->hunger = ClampI32(
                horse->hunger + (fed ? -20 : 12), 0, 100);
            if (fed && horse->age_days >= 365) {
                horse->training = ClampI32(
                    horse->training + 1 + horse->temperament / 50,
                    0, 100);
            }
        }
        if (horse->hunger >= 80) {
            horse->health = ClampI32(horse->health - 2, 1, 100);
        }
    }
}

void CcSimAdvanceDays(CcSim *sim, int32_t days)
{
    if (sim == NULL || days <= 0 || sim->current_day < 1 ||
        sim->current_day > CC_SIM_MAX_DAY ||
        days > CC_SIM_MAX_DAY - sim->current_day) return;
    int32_t next_situation_expiry = NextSituationExpiryDay(sim);
    for (int32_t day = 0; day < days; ++day) {
        sim->current_day += 1;
        AdvanceHorseTeam(sim);
        if (sim->current_day > next_situation_expiry) {
            ExpireSituations(sim);
            next_situation_expiry = NextSituationExpiryDay(sim);
        }
        UpdateShipments(sim);
        AdvanceCouriers(sim);
        AdvanceServiceProjects(sim);
        AdvanceBanditRaids(sim);
        AdvanceGoblinTribute(sim);
        AdvanceDragonEcology(sim);
        AdvanceDragonRetaliation(sim);
        AdvanceHoardRaid(sim);
        AdvanceDragonCampaign(sim);
        if (sim->current_day % 7 == 0) {
            for (int32_t settlement = 0; settlement < sim->settlement_count; ++settlement) {
                UpdateSettlement(sim, settlement);
            }
            AdvanceRuins(sim);
            UpdateThreats(sim);
            UpdateRoutesAndGovernments(sim);
            UpdateRoyalDiplomacy(sim);
            PlanTrade(sim);
            GenerateSituations(sim);
            PlanGoblinTribute(sim);
            PlanHoardRaid(sim);
            next_situation_expiry = NextSituationExpiryDay(sim);
        }
        DeliverDelayedEchoIfReady(sim);
    }
}

static bool ApplyTrade(CcSim *sim, const CcCommand *command,
                       char *error, size_t error_capacity)
{
    if (command->good < 0 || command->good >= CC_GOOD_COUNT ||
        command->amount == 0 || command->amount == INT32_MIN) {
        SetError(error, error_capacity, "Trade command is invalid.");
        return false;
    }
    CcSettlement *settlement = CcSimSettlementMutable(sim, sim->player.location_id);
    if (settlement == NULL) {
        SetError(error, error_capacity, "The carriage is not at a settlement.");
        return false;
    }
    int32_t amount = command->amount;
    int32_t price = settlement->price[command->good];
    const CcSituation *accepted = CcSimAcceptedSituation(sim);
    if (amount < 0 && accepted != NULL &&
        (accepted->kind == CC_SITUATION_RELIEF_DELIVERY ||
         accepted->kind == CC_SITUATION_BLACK_MARKET_DELIVERY) &&
        accepted->target_id == settlement->id &&
        accepted->good == command->good) {
        int32_t selling = -amount;
        int32_t remaining = accepted->quantity - accepted->progress;
        if (sim->resolved_journey_situation_id != accepted->id) {
            SetError(error, error_capacity,
                     "The charter only counts a full load carried here from another town.");
            return false;
        }
        if (selling < remaining) {
            SetError(error, error_capacity,
                     "The sponsor will only receive the full promised consignment.");
            return false;
        }
    }
    if (amount > 0) {
        if (settlement->stock[command->good] < amount) {
            SetError(error, error_capacity, "The local market lacks that stock.");
            return false;
        }
        int32_t old_slots = GoodCargoSlots(
            command->good, sim->player.cargo[command->good]);
        int32_t new_slots = GoodCargoSlots(
            command->good, sim->player.cargo[command->good] + amount);
        if (CcPlayerCargoUsed(&sim->player) - old_slots + new_slots >
            sim->player.cargo_capacity) {
            SetError(error, error_capacity, "The carriage has no cargo space.");
            return false;
        }
        CcMoney cost = (CcMoney)amount * (CcMoney)price;
        if (sim->player.coins < cost) {
            SetError(error, error_capacity, "The company cannot afford that cargo.");
            return false;
        }
        sim->player.coins -= cost;
        settlement->market_coins += cost;
        settlement->stock[command->good] -= amount;
        sim->player.cargo[command->good] += amount;
    } else {
        int32_t selling = -amount;
        if (sim->player.cargo[command->good] < selling) {
            SetError(error, error_capacity, "The carriage does not carry that cargo.");
            return false;
        }
        int32_t sale_price = MaximumI32(1, price * 3 / 4);
        CcMoney proceeds = (CcMoney)selling * (CcMoney)sale_price;
        if (settlement->market_coins < proceeds) {
            SetError(error, error_capacity,
                     "The local market lacks enough coin for that purchase.");
            return false;
        }
        sim->player.cargo[command->good] -= selling;
        settlement->stock[command->good] += selling;
        settlement->market_coins -= proceeds;
        sim->player.coins += proceeds;
        if (command->good == CC_GOOD_FOOD && settlement->hunger > 0) {
            settlement->hunger = ClampI32(settlement->hunger - selling * 2, 0, 100);
            sim->player.reputation = ClampI32(
                sim->player.reputation + selling, -100, 100);
        }
    }
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(text, sizeof(text), "The Crownless company %s %d %s at %s.",
                   amount > 0 ? "loads" : "delivers", amount > 0 ? amount : -amount,
                   CcGoodName(command->good), settlement->name);
    (void)PushEvent(sim, command->good == CC_GOOD_FOOD && amount < 0 ?
                    CC_EVENT_RELIEF : CC_EVENT_PLAYER_TRADE,
                    sim->player.id, settlement->id, 0,
                    amount > 0 ? amount : -amount, text);
    if (amount < 0) {
        ProgressDeliverySituations(sim, settlement->id, command->good, -amount);
    }
    SetError(error, error_capacity, "");
    return true;
}

static bool ApplyGoblinTrade(CcSim *sim, const CcCommand *command,
                             char *error, size_t error_capacity)
{
    bool useful_good = command->good == CC_GOOD_FOOD ||
        command->good == CC_GOOD_TOOLS ||
        command->good == CC_GOOD_WEAPONS;
    if (!useful_good || command->amount <= 0) {
        SetError(error, error_capacity,
                 "The Cinder Tithe trades only for Food, Tools, or Weapons.");
        return false;
    }
    if (sim->player.location_id != sim->goblins.lair_settlement_id) {
        SetError(error, error_capacity,
                 "Goblin trade must be made at the Cinder Tithe's lair.");
        return false;
    }
    if (sim->player.cargo[command->good] < command->amount) {
        SetError(error, error_capacity,
                 "The carriage does not carry that much cargo.");
        return false;
    }
    CcSettlement *lair = CcSimSettlementMutable(
        sim, sim->goblins.lair_settlement_id);
    if (lair == NULL) {
        SetError(error, error_capacity, "The goblin lair cannot be reached.");
        return false;
    }
    int32_t unit_price = MaximumI32(1, lair->price[command->good] * 3 / 4);
    CcMoney proceeds = (CcMoney)unit_price * command->amount;
    if (lair->market_coins < proceeds) {
        SetError(error, error_capacity,
                 "The lair market cannot pay for that whole load.");
        return false;
    }
    sim->player.cargo[command->good] -= command->amount;
    sim->goblins.lair_stock[command->good] += command->amount;
    lair->market_coins -= proceeds;
    sim->player.coins += proceeds;
    int32_t social_change = MinimumI32(8, 1 + command->amount / 2);
    sim->goblins.cohesion = ClampI32(
        sim->goblins.cohesion + social_change, 0, 100);
    sim->goblins.devotion = ClampI32(
        sim->goblins.devotion - MinimumI32(4, 1 + command->amount / 6),
        0, 100);
    sim->player.reputation = ClampI32(
        sim->player.reputation + MinimumI32(5, command->amount), -100, 100);
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(
        text, sizeof(text),
        "Nara Soot-Tongue buys %d %s from the Crownless company for %" PRId64
        " crowns; the lair relies less on the dragon.",
        command->amount, CcGoodName(command->good), proceeds);
    (void)PushEvent(sim, CC_EVENT_GOBLIN_TRADE, sim->goblins.id,
                    sim->goblins.lair_settlement_id, 0U,
                    command->amount, text);
    SetError(error, error_capacity, "");
    return true;
}

static bool ApplyGoblinWarning(CcSim *sim,
                               char *error, size_t error_capacity)
{
    CcGoblinCult *goblins = &sim->goblins;
    bool warning_window =
        goblins->tribute_phase == CC_GOBLIN_TRIBUTE_PREPARING ||
        goblins->tribute_phase == CC_GOBLIN_TRIBUTE_OUTBOUND;
    if (!warning_window || goblins->tribute_target_id == 0U) {
        SetError(error, error_capacity,
                 "No goblin expedition is still close enough to warn about.");
        return false;
    }
    if (sim->player.location_id != goblins->tribute_target_id) {
        SetError(error, error_capacity,
                 "The warning must be delivered at the threatened settlement.");
        return false;
    }
    if (goblins->target_warned) {
        SetError(error, error_capacity, "This settlement has already been warned.");
        return false;
    }
    CcSettlement *target = CcSimSettlementMutable(
        sim, goblins->tribute_target_id);
    if (target == NULL) {
        SetError(error, error_capacity, "The threatened settlement is missing.");
        return false;
    }
    goblins->target_warned = true;
    target->security = ClampI32(target->security + 6, 0, 100);
    goblins->cohesion = ClampI32(goblins->cohesion - 2, 0, 100);
    sim->player.reputation = ClampI32(sim->player.reputation + 3, -100, 100);
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(text, sizeof(text),
                   "The Crownless company warns %s about %s; stores are hidden and defenders gather.",
                   target->name, goblins->name);
    CcEvent *event = PushEvent(
        sim, CC_EVENT_GOBLIN_TARGET_WARNED, sim->player.id,
        target->id, goblins->tribute_event_id, 6, text);
    if (event != NULL) goblins->tribute_event_id = event->id;
    SetError(error, error_capacity, "");
    return true;
}

static bool ApplyGoblinIntercept(CcSim *sim,
                                 char *error, size_t error_capacity)
{
    CcGoblinCult *goblins = &sim->goblins;
    bool intercept_window =
        goblins->tribute_phase == CC_GOBLIN_TRIBUTE_PREPARING ||
        goblins->tribute_phase == CC_GOBLIN_TRIBUTE_OUTBOUND;
    if (!intercept_window || goblins->tribute_target_id == 0U) {
        SetError(error, error_capacity,
                 "No outbound goblin expedition can be intercepted now.");
        return false;
    }
    if (sim->player.location_id != goblins->tribute_target_id) {
        SetError(error, error_capacity,
                 "Meet the expedition at its threatened settlement to intercept it.");
        return false;
    }
    CcId target_id = goblins->tribute_target_id;
    const CcSettlement *target = CcSimSettlement(sim, target_id);
    int32_t losses = MaximumI32(2, goblins->members / 24);
    goblins->members = MaximumI32(12, goblins->members - losses);
    goblins->cohesion = ClampI32(goblins->cohesion - 8, 0, 100);
    goblins->devotion = ClampI32(goblins->devotion + 3, 0, 100);
    goblins->expeditions_intercepted += 1;
    sim->carriage.condition = ClampI32(sim->carriage.condition - 6, 0, 100);
    sim->player.reputation = ClampI32(sim->player.reputation + 5, -100, 100);
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(text, sizeof(text),
                   "Crownless turns %.24s back outside %.24s; %d goblins fall. "
                   "Survivors close ranks.",
                   goblins->name, target != NULL ? target->name : "the town",
                   losses);
    (void)PushEvent(
        sim, CC_EVENT_GOBLIN_EXPEDITION_INTERCEPTED, sim->player.id,
        target_id, goblins->tribute_event_id, losses, text);
    goblins->tribute_phase = CC_GOBLIN_TRIBUTE_IDLE;
    goblins->raid_motive = CC_GOBLIN_RAID_NONE;
    goblins->tribute_target_id = 0U;
    goblins->tribute_event_id = 0U;
    goblins->tribute_days_remaining = 0;
    goblins->target_warned = false;
    goblins->tribute_cooldown_days = 28;
    SetError(error, error_capacity, "");
    return true;
}

static void UnlockCrownlessAtlas(CcSim *sim)
{
    uint32_t atlas_bit = MapSlotBit(CC_MAP_CROWNLESS_ATLAS);
    uint32_t required =
        ((UINT32_C(1) << CC_MAP_COLLECTION_COUNT) - 1U) & ~atlas_bit;
    if ((sim->player.map_catalogue_mask & required) != required ||
        (sim->player.map_catalogue_mask & atlas_bit) != 0U) return;
    CcMap *atlas = &sim->maps[CC_MAP_CROWNLESS_ATLAS];
    atlas->owner_id = sim->player.id;
    sim->player.map_catalogue_mask |= atlas_bit;
    sim->player.map_archive_mask |= atlas_bit;
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(text, sizeof(text),
                   "The Gloamgate archive binds twelve collected charts into %s.",
                   atlas->name);
    (void)PushEvent(sim, CC_EVENT_MAP_BOUGHT, atlas->id,
                    sim->settlements[1].id, 0U,
                    CC_MAP_COLLECTION_COUNT, text);
}

static bool ApplyBuyMap(CcSim *sim, const CcCommand *command,
                        char *error, size_t error_capacity)
{
    CcMap *map = MapMutable(sim, command->target_id);
    CcSettlement *seller = CcSimSettlementMutable(
        sim, sim->player.location_id);
    if (map == NULL || seller == NULL || map->owner_id != seller->id) {
        SetError(error, error_capacity, "That chart is not for sale here.");
        return false;
    }
    if (CcPlayerMapCount(sim) >= sim->player.map_capacity) {
        SetError(error, error_capacity, "The carriage map case is full.");
        return false;
    }
    if (sim->player.coins < map->ask_price) {
        SetError(error, error_capacity, "The company cannot afford that chart.");
        return false;
    }
    sim->player.coins -= map->ask_price;
    seller->market_coins += map->ask_price;
    map->owner_id = sim->player.id;
    int32_t slot = MapSlot(sim, map);
    sim->player.map_catalogue_mask |= MapSlotBit(slot);
    sim->player.map_archive_mask &= ~MapSlotBit(slot);
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(text, sizeof(text),
                   "The Crownless company buys %s at %s for %d crowns.",
                   map->name, seller->name, map->ask_price);
    (void)PushEvent(sim, CC_EVENT_MAP_BOUGHT, map->id, seller->id, 0U,
                    map->ask_price, text);
    UnlockCrownlessAtlas(sim);
    SetError(error, error_capacity, "");
    return true;
}

static bool ApplySellMap(CcSim *sim, const CcCommand *command,
                         char *error, size_t error_capacity)
{
    CcMap *map = MapMutable(sim, command->target_id);
    CcSettlement *buyer = CcSimSettlementMutable(
        sim, sim->player.location_id);
    if (map == NULL || buyer == NULL || map->owner_id != sim->player.id) {
        SetError(error, error_capacity, "The carriage does not carry that chart.");
        return false;
    }
    if (CcSimMapIsArchived(sim, map) &&
        sim->player.location_id != sim->settlements[1].id) {
        SetError(error, error_capacity,
                 "That chart is stored in the Gloamgate archive.");
        return false;
    }
    int32_t payment = MaximumI32(1, map->ask_price * 2 / 3);
    if (buyer->market_coins < payment) {
        SetError(error, error_capacity,
                 "The local market lacks enough coin to buy that chart.");
        return false;
    }
    map->owner_id = buyer->id;
    sim->player.map_archive_mask &= ~MapSlotBit(MapSlot(sim, map));
    buyer->market_coins -= payment;
    sim->player.coins += payment;
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(text, sizeof(text),
                   "The Crownless company sells %s at %s for %d crowns.",
                   map->name, buyer->name, payment);
    (void)PushEvent(sim, CC_EVENT_MAP_SOLD, map->id, buyer->id, 0U,
                    payment, text);
    SetError(error, error_capacity, "");
    return true;
}

static bool ApplyArchiveMap(CcSim *sim, const CcCommand *command,
                            char *error, size_t error_capacity)
{
    CcMap *map = MapMutable(sim, command->target_id);
    if (map == NULL || map->owner_id != sim->player.id ||
        !CcSimMapIsCatalogued(sim, map)) {
        SetError(error, error_capacity,
                 "The carriage does not own that chart.");
        return false;
    }
    if (sim->journey.active ||
        sim->player.location_id != sim->settlements[1].id) {
        SetError(error, error_capacity,
                 "Maps can only be stored at the Gloamgate archive.");
        return false;
    }
    if (CcSimMapIsArchived(sim, map)) {
        SetError(error, error_capacity, "That chart is already archived.");
        return false;
    }
    sim->player.map_archive_mask |= MapSlotBit(MapSlot(sim, map));
    SetError(error, error_capacity, "");
    return true;
}

static bool ApplyRetrieveMap(CcSim *sim, const CcCommand *command,
                             char *error, size_t error_capacity)
{
    CcMap *map = MapMutable(sim, command->target_id);
    if (map == NULL || map->owner_id != sim->player.id ||
        !CcSimMapIsArchived(sim, map)) {
        SetError(error, error_capacity,
                 "That chart is not in the archive.");
        return false;
    }
    if (sim->journey.active ||
        sim->player.location_id != sim->settlements[1].id) {
        SetError(error, error_capacity,
                 "The Gloamgate archive is not within reach.");
        return false;
    }
    if (CcPlayerMapCount(sim) >= sim->player.map_capacity) {
        SetError(error, error_capacity, "The carriage map case is full.");
        return false;
    }
    sim->player.map_archive_mask &= ~MapSlotBit(MapSlot(sim, map));
    SetError(error, error_capacity, "");
    return true;
}

static bool ApplyBuyTreasure(CcSim *sim, const CcCommand *command,
                             char *error, size_t error_capacity)
{
    CcTreasure *treasure = (CcTreasure *)CcSimTreasure(
        sim, command->target_id);
    CcSettlement *seller = CcSimSettlementMutable(
        sim, sim->player.location_id);
    if (treasure == NULL || seller == NULL ||
        treasure->owner_id != seller->id ||
        treasure->location_id != seller->id) {
        SetError(error, error_capacity,
                 "That named treasure is not for sale here.");
        return false;
    }
    if (CcPlayerCargoUsed(&sim->player) + 1 >
        sim->player.cargo_capacity) {
        SetError(error, error_capacity,
                 "The carriage needs one empty cargo slot for that treasure.");
        return false;
    }
    if (sim->player.coins < treasure->appraised_value) {
        SetError(error, error_capacity,
                 "The company cannot afford that named treasure.");
        return false;
    }
    sim->player.coins -= treasure->appraised_value;
    seller->market_coins += treasure->appraised_value;
    treasure->owner_id = sim->player.id;
    treasure->location_id = sim->player.location_id;
    sim->player.treasure_cargo_slots += 1;
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(text, sizeof(text),
                   "Loads %.24s at %.16s for %d crowns; one carriage slot.",
                   treasure->name, seller->name,
                   treasure->appraised_value);
    (void)PushEvent(sim, CC_EVENT_PLAYER_TRADE, treasure->id,
                    seller->id, 0U, treasure->appraised_value, text);
    SetError(error, error_capacity, "");
    return true;
}

static bool ApplySellTreasure(CcSim *sim, const CcCommand *command,
                              char *error, size_t error_capacity)
{
    CcTreasure *treasure = (CcTreasure *)CcSimTreasure(
        sim, command->target_id);
    CcSettlement *buyer = CcSimSettlementMutable(
        sim, sim->player.location_id);
    if (treasure == NULL || buyer == NULL ||
        treasure->owner_id != sim->player.id ||
        treasure->location_id != sim->player.location_id) {
        SetError(error, error_capacity,
                 "The carriage does not carry that named treasure.");
        return false;
    }
    int32_t payment = MaximumI32(1, treasure->appraised_value * 3 / 4);
    if (buyer->market_coins < payment) {
        SetError(error, error_capacity,
                 "The local market lacks enough coin to buy that treasure.");
        return false;
    }
    buyer->market_coins -= payment;
    sim->player.coins += payment;
    treasure->owner_id = buyer->id;
    treasure->location_id = buyer->id;
    sim->player.treasure_cargo_slots -= 1;
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(text, sizeof(text),
                   "The Crownless company unloads %s at %s for %d crowns.",
                   treasure->name, buyer->name, payment);
    (void)PushEvent(sim, CC_EVENT_PLAYER_TRADE, treasure->id,
                    buyer->id, 0U, payment, text);
    SetError(error, error_capacity, "");
    return true;
}

static bool ApplyStealDragonNamedTreasure(
    CcSim *sim, const CcCommand *command,
    char *error, size_t error_capacity)
{
    CcTreasure *treasure = (CcTreasure *)CcSimTreasure(
        sim, command->target_id);
    if (sim->dragon.slain) {
        SetError(error, error_capacity,
                 "The slain dragon no longer guards a hoard.");
        return false;
    }
    if (sim->journey.active ||
        sim->player.location_id != sim->dragon.lair_settlement_id) {
        SetError(error, error_capacity,
                 "A named hoard treasure can only be taken from the cave.");
        return false;
    }
    if (treasure == NULL || treasure->destroyed ||
        treasure->owner_id != sim->dragon.id ||
        treasure->location_id != sim->dragon.lair_settlement_id) {
        SetError(error, error_capacity,
                 "That named treasure is not in the dragon hoard.");
        return false;
    }
    if (sim->dragon.stolen_outstanding > 0) {
        SetError(error, error_capacity,
                 "The dragon is already hunting for stolen treasure.");
        return false;
    }
    if (CcPlayerCargoUsed(&sim->player) + 1 >
        sim->player.cargo_capacity) {
        SetError(error, error_capacity,
                 "The carriage needs one empty slot for that treasure.");
        return false;
    }
    CcSettlement *target = CcSimSettlementMutable(
        sim, sim->goblins.last_tribute_origin_id);
    if (target == NULL || target->id == sim->dragon.lair_settlement_id) {
        target = RichestDragonTarget(sim);
    }
    if (target == NULL) {
        SetError(error, error_capacity,
                 "The dragon has no realm to threaten.");
        return false;
    }
    treasure->owner_id = sim->player.id;
    treasure->location_id = sim->player.location_id;
    sim->player.treasure_cargo_slots += 1;
    sim->dragon.stolen_treasure_id = treasure->id;
    sim->dragon.stolen_outstanding = treasure->appraised_value;
    sim->dragon.theft_actor_id = sim->player.id;
    sim->dragon.retaliation_target_id = target->id;
    sim->dragon.omen_days_remaining = 14;
    sim->dragon.memory_integrity = ClampI32(
        sim->dragon.memory_integrity - MinimumI32(
            70, 25 + treasure->craft_work * 5 + treasure->gem_content * 2),
        0, 100);
    sim->dragon.crown_continuity_days = 0;
    sim->dragon.activity = CC_DRAGON_ACTIVITY_RETALIATING;
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(text, sizeof(text),
                   "The carriage takes %.28s from %s's remembered hoard.",
                   treasure->name, sim->dragon.name);
    CcEvent *theft = PushEvent(
        sim, CC_EVENT_DRAGON_HOARD_STOLEN, sim->player.id,
        sim->dragon.lair_settlement_id, sim->dragon.hoard_event_id,
        treasure->appraised_value, text);
    sim->dragon.hoard_event_id = theft != NULL ? theft->id :
                                 sim->dragon.hoard_event_id;
    (void)snprintf(
        text, sizeof(text),
        "Smoke falls over %.16s; old readers count 14 nights until %.16s comes for %.20s.",
        target->name, sim->dragon.name, treasure->name);
    CcEvent *omen = PushEvent(
        sim, CC_EVENT_DRAGON_OMEN, sim->dragon.id, target->id,
        theft != NULL ? theft->id : 0U, 14, text);
    sim->dragon.omen_event_id = omen != NULL ? omen->id : 0U;
    sim->player.reputation = ClampI32(
        sim->player.reputation - 8, -100, 100);
    SetError(error, error_capacity, "");
    return true;
}

static bool ApplyReturnDragonNamedTreasure(
    CcSim *sim, const CcCommand *command,
    char *error, size_t error_capacity)
{
    CcTreasure *treasure = (CcTreasure *)CcSimTreasure(
        sim, command->target_id);
    if (sim->dragon.slain) {
        SetError(error, error_capacity,
                 "No dragon remains to receive the named treasure.");
        return false;
    }
    if (sim->journey.active ||
        sim->player.location_id != sim->dragon.lair_settlement_id) {
        SetError(error, error_capacity,
                 "The exact treasure must be carried back to the cave.");
        return false;
    }
    if (treasure == NULL || treasure->id != sim->dragon.stolen_treasure_id ||
        treasure->owner_id != sim->player.id ||
        treasure->location_id != sim->player.location_id) {
        SetError(error, error_capacity,
                 "The carriage does not carry the exact object the dragon remembers.");
        return false;
    }
    treasure->owner_id = sim->dragon.id;
    treasure->location_id = sim->dragon.lair_settlement_id;
    sim->player.treasure_cargo_slots -= 1;
    sim->dragon.stolen_outstanding = 0;
    sim->dragon.stolen_treasure_id = 0U;
    sim->dragon.memory_integrity = ClampI32(
        MaximumI32(90, sim->dragon.memory_integrity + 50), 0, 100);
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(
        text, sizeof(text),
        "The carriage restores %.28s to %s's remembered place in the hoard.",
        treasure->name, sim->dragon.name);
    CcEvent *returned = PushEvent(
        sim, CC_EVENT_DRAGON_TREASURE_RETURNED, sim->player.id,
        sim->dragon.lair_settlement_id, sim->dragon.omen_event_id,
        treasure->appraised_value, text);
    sim->dragon.hoard_event_id = returned != NULL ? returned->id :
                                 sim->dragon.hoard_event_id;
    sim->dragon.theft_actor_id = 0U;
    sim->dragon.retaliation_target_id = 0U;
    sim->dragon.omen_event_id = 0U;
    sim->dragon.omen_days_remaining = 0;
    sim->player.reputation = ClampI32(
        sim->player.reputation + 3, -100, 100);
    SetError(error, error_capacity, "");
    return true;
}

static bool ApplyInterceptDragonTribute(
    CcSim *sim, char *error, size_t error_capacity)
{
    CcGoblinCult *goblins = &sim->goblins;
    if (sim->journey.active ||
        sim->player.location_id != sim->dragon.lair_settlement_id) {
        SetError(error, error_capacity,
                 "The tribute can only be intercepted at the dragon cave approach.");
        return false;
    }
    if (sim->dragon.slain ||
        goblins->tribute_phase != CC_GOBLIN_TRIBUTE_TO_DRAGON) {
        SetError(error, error_capacity,
                 "No dragon tribute is approaching the cave.");
        return false;
    }

    CcTreasure *treasure = NULL;
    if (goblins->carried_treasure_id != 0U) {
        treasure = (CcTreasure *)CcSimTreasure(
            sim, goblins->carried_treasure_id);
        if (treasure == NULL || treasure->destroyed) {
            SetError(error, error_capacity,
                     "The tribute's named treasure cannot be accounted for.");
            return false;
        }
    }
    CcPlayerCompany loaded = sim->player;
    int32_t goods_taken = 0;
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        loaded.cargo[good] += goblins->carried_goods[good];
        goods_taken += goblins->carried_goods[good];
    }
    if (treasure != NULL) loaded.treasure_cargo_slots += 1;
    if (CcPlayerCargoUsed(&loaded) > loaded.cargo_capacity) {
        SetError(error, error_capacity,
                 "The carriage lacks room for the whole tribute load.");
        return false;
    }

    CcId parent_id = goblins->tribute_event_id;
    CcMoney coins_taken = goblins->carried_tribute;
    sim->player.coins += coins_taken;
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        sim->player.cargo[good] += goblins->carried_goods[good];
        goblins->carried_goods[good] = 0;
    }
    if (treasure != NULL) {
        treasure->owner_id = sim->player.id;
        treasure->location_id = sim->player.location_id;
        sim->player.treasure_cargo_slots += 1;
    }

    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(
        text, sizeof(text),
        "The Crownless carriage intercepts %" PRId64
        " tribute crowns, %d goods%s before they enter %s's hoard.",
        coins_taken, goods_taken,
        treasure != NULL ? ", and a named treasure" : "",
        sim->dragon.name);
    int64_t value = coins_taken + goods_taken +
                    (treasure != NULL ? treasure->appraised_value : 0);
    (void)PushEvent(
        sim, CC_EVENT_GOBLIN_TRIBUTE_TAKEN, sim->player.id,
        sim->dragon.lair_settlement_id, parent_id,
        value > INT32_MAX ? INT32_MAX : (int32_t)value, text);

    goblins->tribute_phase = CC_GOBLIN_TRIBUTE_IDLE;
    goblins->raid_motive = CC_GOBLIN_RAID_NONE;
    goblins->tribute_target_id = 0U;
    goblins->tribute_event_id = 0U;
    goblins->carried_tribute = 0;
    goblins->carried_treasure_id = 0U;
    goblins->tribute_days_remaining = 0;
    goblins->tribute_cooldown_days = 35;
    goblins->devotion = ClampI32(goblins->devotion - 8, 0, 100);
    sim->player.reputation = ClampI32(
        sim->player.reputation - 3, -100, 100);
    SetError(error, error_capacity, "");
    return true;
}

static bool AcceptSituation(CcSim *sim, const CcSituation *situation,
                            bool from_affected_character,
                            char *error, size_t error_capacity)
{
    if (situation == NULL || situation->status != CC_SITUATION_ACTIVE) {
        SetError(error, error_capacity, "That charter is no longer available.");
        return false;
    }
    CcId offer_settlement = CcSimSituationOfferSettlementId(sim, situation);
    bool affected_here = from_affected_character &&
        CcSimSituationTouchesSettlement(sim, situation,
                                        sim->player.location_id);
    if ((offer_settlement == 0U ||
         sim->player.location_id != offer_settlement) && !affected_here) {
        SetError(error, error_capacity,
                 "That sponsor is not here. Find the offer in its own settlement.");
        return false;
    }
    if (sim->player.accepted_situation_id == situation->id) {
        SetError(error, error_capacity, "The company has already promised this work.");
        return false;
    }
    if (CcSimAcceptedSituation(sim) != NULL) {
        SetError(error, error_capacity,
                 "The company must fulfill or withdraw from its current charter first.");
        return false;
    }
    if (situation->kind == CC_SITUATION_COURIER_DELIVERY) {
        CcCourier *courier = CourierMutable(sim, situation->target_id);
        if (courier == NULL || courier->status != CC_COURIER_WAITING ||
            courier->current_settlement_id != sim->player.location_id) {
            SetError(error, error_capacity,
                     "That sealed dispatch has already left.");
            return false;
        }
        courier->status = CC_COURIER_WITH_PLAYER;
        courier->reliability = ClampI32(
            courier->reliability + 5, 0, 100);
    }
    sim->player.accepted_situation_id = situation->id;
    sim->resolved_journey_situation_id = 0U;
    sim->resolved_journey_outcome = CC_JOURNEY_OUTCOME_NONE;
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(text, sizeof(text),
                   "The Crownless company accepts %s before day %d.",
                   CcSituationKindName(situation->kind),
                   situation->deadline_day);
    (void)PushEvent(sim, CC_EVENT_CHARTER_ACCEPTED, situation->id,
                    situation->target_id, situation->cause_event_id,
                    situation->deadline_day - sim->current_day, text);
    RefreshSituationCharacterActivities(sim, (CcSituation *)situation);
    SetError(error, error_capacity, "");
    return true;
}

static bool ApplyAcceptSituation(CcSim *sim, const CcCommand *command,
                                 char *error, size_t error_capacity)
{
    return AcceptSituation(sim, CcSimSituation(sim, command->target_id),
                           false, error, error_capacity);
}

static bool ApplyRefuseSituation(CcSim *sim, const CcCommand *command,
                                 char *error, size_t error_capacity)
{
    CcSituation *situation = NULL;
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        if (sim->situations[i].id == command->target_id) {
            situation = &sim->situations[i];
            break;
        }
    }
    if (situation == NULL || situation->status != CC_SITUATION_ACTIVE) {
        SetError(error, error_capacity, "That offer is no longer open.");
        return false;
    }
    if (situation->id == sim->player.accepted_situation_id) {
        SetError(error, error_capacity,
                 "This is already a promise. Withdraw from it instead.");
        return false;
    }
    if (CcSimSituationOfferSettlementId(sim, situation) !=
        sim->player.location_id) {
        SetError(error, error_capacity,
                 "The sponsor is not here to hear the refusal.");
        return false;
    }
    situation->status = CC_SITUATION_FAILED;
    if (situation->kind == CC_SITUATION_COURIER_DELIVERY) {
        CcCourier *courier = CourierMutable(sim, situation->target_id);
        if (courier != NULL && courier->status == CC_COURIER_WAITING) {
            courier->departure_day = sim->current_day + 1;
        }
    }
    CcFaction *issuer = FactionByIdMutable(sim, situation->issuer_faction_id);
    if (issuer != NULL) issuer->support = ClampI32(issuer->support - 2, 0, 100);
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(text, sizeof(text),
                   "The Crownless company refuses %s to %s's face.",
                   CcSituationKindName(situation->kind),
                   situation->sponsor_name);
    (void)PushEvent(sim, CC_EVENT_SITUATION_FAILED, situation->id,
                    situation->target_id, situation->cause_event_id, -2, text);
    CcCharacter *sponsor = CharacterMutable(
        sim, situation->sponsor_character_id);
    if (sponsor != NULL) {
        sponsor->player_disposition = ClampI32(
            sponsor->player_disposition - 8, -100, 100);
        sponsor->stress = ClampI32(sponsor->stress + 4, 0, 100);
    }
    RefreshSituationCharacterActivities(sim, situation);
    SetError(error, error_capacity, "");
    return true;
}

static bool ApplyAbandonSituation(CcSim *sim, const CcCommand *command,
                                  char *error, size_t error_capacity)
{
    const CcSituation *situation = CcSimAcceptedSituation(sim);
    if (situation == NULL || (command->target_id != 0U &&
                              command->target_id != situation->id)) {
        SetError(error, error_capacity, "The company has no such accepted charter.");
        return false;
    }
    if (situation->kind == CC_SITUATION_COURIER_DELIVERY) {
        CcCourier *courier = CourierMutable(sim, situation->target_id);
        if (courier != NULL &&
            courier->status == CC_COURIER_WITH_PLAYER) {
            courier->status = CC_COURIER_WAITING;
            courier->current_settlement_id = sim->player.location_id;
            courier->departure_day = sim->current_day + 1;
        }
    }
    sim->player.accepted_situation_id = 0U;
    sim->resolved_journey_situation_id = 0U;
    sim->resolved_journey_outcome = CC_JOURNEY_OUTCOME_NONE;
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(text, sizeof(text),
                   "The Crownless company withdraws from %s; the need remains.",
                   CcSituationKindName(situation->kind));
    CcEvent *withdrawal = PushEvent(
        sim, CC_EVENT_CHARTER_ABANDONED, situation->id,
        situation->target_id, situation->cause_event_id,
        situation->progress, text);
    CcCharacter *affected = CharacterMutable(
        sim, situation->affected_character_id);
    CcCharacter *sponsor = CharacterMutable(
        sim, situation->sponsor_character_id);
    RememberCharacter(affected, CC_CHARACTER_MEMORY_PLAYER_WITHDREW,
                      situation->id,
                      withdrawal != NULL ? withdrawal->id : 0U,
                      sim->current_day);
    if (sponsor != affected) {
        RememberCharacter(sponsor, CC_CHARACTER_MEMORY_PLAYER_WITHDREW,
                          situation->id,
                          withdrawal != NULL ? withdrawal->id : 0U,
                          sim->current_day);
    }
    if (affected != NULL) {
        affected->player_disposition = ClampI32(
            affected->player_disposition - 12, -100, 100);
        affected->stress = ClampI32(affected->stress + 10, 0, 100);
    }
    if (sponsor != NULL && sponsor != affected) {
        sponsor->player_disposition = ClampI32(
            sponsor->player_disposition - 6, -100, 100);
        sponsor->stress = ClampI32(sponsor->stress + 4, 0, 100);
    }
    RefreshSituationCharacterActivities(sim, (CcSituation *)situation);
    SetError(error, error_capacity, "");
    return true;
}

static void RememberCharacter(CcCharacter *character,
                              CcCharacterMemoryKind kind,
                              CcId subject_id, CcId event_id, int32_t day)
{
    if (character == NULL || kind == CC_CHARACTER_MEMORY_NONE ||
        CcCharacterRemembers(character, kind, subject_id)) return;
    int32_t slot = character->memory_write_index;
    if (slot < 0 || slot >= CC_CHARACTER_MEMORY_CAPACITY) slot = 0;
    character->memories[slot] = (CcCharacterMemory){
        .kind = kind,
        .subject_id = subject_id,
        .event_id = event_id,
        .day = day
    };
    character->memory_write_index =
        (slot + 1) % CC_CHARACTER_MEMORY_CAPACITY;
    if (character->memory_count < CC_CHARACTER_MEMORY_CAPACITY) {
        character->memory_count += 1;
    }
}

static bool ApplyCharacterResponse(CcSim *sim, const CcCommand *command,
                                   char *error, size_t error_capacity)
{
    CcSituation *situation = (CcSituation *)CcSimSituation(
        sim, command->target_id);
    CcCharacter *character = (CcCharacter *)
        CcSimSituationConversationCharacter(
            sim, situation, sim->player.location_id);
    if (situation == NULL || character == NULL) {
        SetError(error, error_capacity,
                 "That person is not here to answer.");
        return false;
    }
    CcCharacterResponse response = (CcCharacterResponse)command->amount;
    CcCharacterMemoryKind memory_kind = response ==
        CC_CHARACTER_RESPONSE_LISTEN ? CC_CHARACTER_MEMORY_MET_PLAYER :
        response == CC_CHARACTER_RESPONSE_PLEDGE_HELP ?
            CC_CHARACTER_MEMORY_PLAYER_PROMISED : CC_CHARACTER_MEMORY_NONE;
    if (memory_kind == CC_CHARACTER_MEMORY_NONE) {
        SetError(error, error_capacity, "Choose a clear response.");
        return false;
    }
    if (CcCharacterRemembers(character, memory_kind, situation->id)) {
        SetError(error, error_capacity,
                 "This conversation has already changed their view of you.");
        return false;
    }
    CcId parent = situation->cause_event_id;
    if (response == CC_CHARACTER_RESPONSE_PLEDGE_HELP) {
        if (situation->status != CC_SITUATION_ACTIVE) {
            SetError(error, error_capacity,
                     "That crisis is no longer open to a promise.");
            return false;
        }
        const CcSituation *accepted = CcSimAcceptedSituation(sim);
        if (accepted != NULL && accepted->id != situation->id) {
            SetError(error, error_capacity,
                     "Keep the promise already carried by the company first.");
            return false;
        }
        if (accepted == NULL &&
            !AcceptSituation(sim, situation, true,
                             error, error_capacity)) return false;
        const CcEvent *promise = CcSimRecentEvent(sim, 0);
        if (promise != NULL) parent = promise->id;
    }
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(
        text, sizeof(text), response == CC_CHARACTER_RESPONSE_LISTEN ?
            "The Crownless company hears %.24s's account of %s." :
            "The Crownless company promises %.24s that it will answer %s.",
        character->name, CcSituationKindName(situation->kind));
    CcEvent *event = PushEvent(
        sim, CC_EVENT_CHARACTER_INTERACTION, character->id,
        character->current_settlement_id, parent,
        response == CC_CHARACTER_RESPONSE_LISTEN ? 2 : 8, text);
    RememberCharacter(character, memory_kind, situation->id,
                      event != NULL ? event->id : 0U, sim->current_day);
    character->player_disposition = ClampI32(
        character->player_disposition +
            (response == CC_CHARACTER_RESPONSE_LISTEN ? 2 : 8),
        -100, 100);
    character->stress = ClampI32(
        character->stress -
            (response == CC_CHARACTER_RESPONSE_LISTEN ? 5 : 12),
        0, 100);
    RefreshSituationCharacterActivities(sim, situation);
    SetError(error, error_capacity, "");
    return true;
}

static void DeliverDelayedEchoIfReady(CcSim *sim)
{
    if (sim == NULL || !sim->delayed_echo.active ||
        sim->journey.active ||
        sim->current_day < sim->delayed_echo.due_day ||
        sim->player.location_id != sim->delayed_echo.settlement_id) return;
    const CcSettlement *place = CcSimSettlement(
        sim, sim->delayed_echo.settlement_id);
    const CcSituation *situation = CcSimSituation(
        sim, sim->delayed_echo.situation_id);
    int32_t prior_echoes = 0;
    for (int32_t offset = 0; offset < sim->event_count; ++offset) {
        const CcEvent *event = CcSimRecentEvent(sim, offset);
        if (event != NULL && event->kind == CC_EVENT_DELAYED_ECHO &&
            event->subject_id == sim->delayed_echo.situation_id) {
            prior_echoes += 1;
        }
    }
    char text[CC_EVENT_TEXT_CAPACITY];
    if (situation != NULL &&
        situation->kind == CC_SITUATION_ROUTE_REPAIR && prior_echoes == 0) {
        (void)snprintf(text, sizeof(text),
                       "A letter from %.16s holds a broken chain link. Carts cross again; the hungry boy now eats before the officers do.",
                       sim->delayed_echo.character_name);
    } else if (situation != NULL &&
               situation->kind == CC_SITUATION_ROUTE_REPAIR) {
        (void)snprintf(text, sizeof(text),
                       "A second letter from %.16s says the bridge feeds %.16s, but its toll keepers are buying very large hats.",
                       sim->delayed_echo.character_name,
                       place != NULL ? place->name : "the settlement");
    } else if (situation != NULL &&
               situation->kind == CC_SITUATION_BLACK_MARKET_DELIVERY &&
               prior_echoes == 0) {
        (void)snprintf(text, sizeof(text),
                       "A letter from %.16s smells of onion soup. A fox sits beside %.16s's new toll sign. Food reached the hungry first.",
                       sim->delayed_echo.character_name,
                       place != NULL ? place->name : "the settlement");
    } else if (situation != NULL &&
               situation->kind == CC_SITUATION_BLACK_MARKET_DELIVERY) {
        (void)snprintf(text, sizeof(text),
                       "A second letter from %.16s has no drawing. Night Road collectors now demand a share of every load entering %.16s.",
                       sim->delayed_echo.character_name,
                       place != NULL ? place->name : "the settlement");
    } else if (situation != NULL &&
               situation->kind == CC_SITUATION_MONSTER_EXPEDITION) {
        (void)snprintf(text, sizeof(text),
                       prior_echoes == 0 ?
                       "A letter from %.16s leaves silver dust. Three rings came back from %.16s's mine. The guild still calls the keepers thieves." :
                       "A second letter from %.16s says the mine below %.16s has made safer crews, new debts, and enemies with names.",
                       sim->delayed_echo.character_name,
                       place != NULL ? place->name : "the settlement");
    } else if (prior_echoes == 0) {
        (void)snprintf(text, sizeof(text),
                       "A letter from %.16s is tied with red thread. The ovens of %.16s are warm, though the bread line still reaches the well.",
                       sim->delayed_echo.character_name,
                       place != NULL ? place->name : "the settlement");
    } else {
        (void)snprintf(text, sizeof(text),
                       "A second letter from %.16s says families followed the smell of bread to %.16s, and the old stores found new locks.",
                       sim->delayed_echo.character_name,
                       place != NULL ? place->name : "the settlement");
    }
    CcEvent *echo = PushEvent(sim, CC_EVENT_DELAYED_ECHO,
                              sim->delayed_echo.situation_id,
                              sim->delayed_echo.settlement_id,
                              sim->delayed_echo.parent_event_id,
                              sim->current_day - sim->delayed_echo.due_day,
                              text);
    if (prior_echoes == 0) {
        sim->delayed_echo.parent_event_id = echo != NULL ? echo->id :
            sim->delayed_echo.parent_event_id;
        sim->delayed_echo.due_day = sim->current_day + 30;
    } else {
        sim->delayed_echo.active = false;
    }
}

static void CreateJourneyTraffic(CcSim *sim,
                                 const CcJourneyEncounter *journey,
                                 CcId parent_event_id)
{
    if (sim == NULL || journey == NULL) return;
    CcSettlement *origin = CcSimSettlementMutable(sim, journey->origin_id);
    const CcSettlement *destination = CcSimSettlement(
        sim, journey->destination_id);
    const CcRoute *route = CcSimRoute(sim, journey->route_id);
    if (origin == NULL || destination == NULL || route == NULL ||
        CcSimRouteCrossesWarBorder(sim, route->id)) return;
    CcGood good = CC_GOOD_FOOD;
    for (int32_t candidate = 1; candidate < CC_GOOD_COUNT; ++candidate) {
        if (origin->stock[candidate] > origin->stock[good]) {
            good = (CcGood)candidate;
        }
    }
    int32_t quantity = MinimumI32(4, origin->stock[good]);
    if (quantity < 1) return;
    CcShipment *shipment = AllocateShipment(sim);
    if (shipment == NULL) return;
    origin->stock[good] -= quantity;
    *shipment = (CcShipment){
        .id = NextId(sim, CC_ENTITY_SHIPMENT),
        .origin_id = origin->id,
        .destination_id = destination->id,
        .final_destination_id = destination->id,
        .route_id = route->id,
        .good = good,
        .quantity = quantity,
        .departure_day = sim->current_day,
        .arrival_day = sim->current_day + route->travel_days,
        .status = CC_SHIPMENT_TRAVELLING
    };
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(text, sizeof(text),
                   "%.24s sends %d %.16s toward %.24s after the Crownless intervention.",
                   origin->name, quantity, CcGoodName(good), destination->name);
    (void)PushEvent(sim, CC_EVENT_SHIPMENT_DEPARTED, shipment->id,
                    origin->id, parent_event_id, quantity, text);
}

static void InterruptJourney(CcSim *sim);

static void WarnJourneyAmbush(CcSim *sim)
{
    if (sim == NULL || !sim->journey.active ||
        !sim->journey.ambush_pending || sim->journey.ambush_warned) return;
    const CcBanditGroup *bandits = BanditsOnRoute(
        sim, sim->journey.route_id);
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(
        text, sizeof(text),
        "Scouts spot riders from %.24s shadowing the carriage. Careful pace may lose them before the road narrows.",
        bandits != NULL ? bandits->name : "a roadside company");
    CcEvent *event = PushEvent(
        sim, CC_EVENT_JOURNEY_WARNING, sim->player.id,
        sim->journey.route_id, sim->journey.parent_event_id,
        sim->journey.danger, text);
    if (event != NULL) sim->journey.parent_event_id = event->id;
    sim->journey.ambush_warned = true;
}

static void ResolveWarnedJourneyAmbush(CcSim *sim)
{
    if (sim == NULL || !sim->journey.active ||
        !sim->journey.ambush_pending || !sim->journey.ambush_warned) return;
    if (sim->journey.pace == CC_JOURNEY_PACE_CAREFUL) {
        const CcBanditGroup *bandits = BanditsOnRoute(
            sim, sim->journey.route_id);
        char text[CC_EVENT_TEXT_CAPACITY];
        (void)snprintf(
            text, sizeof(text),
            "The careful team leaves %.24s behind on a watched side track; no cargo or crowns are lost.",
            bandits != NULL ? bandits->name : "the roadside riders");
        CcEvent *event = PushEvent(
            sim, CC_EVENT_AMBUSH_EVADED, sim->player.id,
            sim->journey.route_id, sim->journey.parent_event_id, 0, text);
        if (event != NULL) sim->journey.parent_event_id = event->id;
        sim->journey.ambush_pending = false;
        sim->journey.ambush_resolved = true;
        return;
    }
    sim->journey.ambush_pending = false;
    sim->journey.ambush_resolved = true;
    sim->journey.encounter_triggered = true;
    InterruptJourney(sim);
}

static void FinishJourneyArrival(CcSim *sim)
{
    if (sim == NULL || !sim->journey.active) return;
    const CcSettlement *destination = CcSimSettlement(
        sim, sim->journey.destination_id);
    const CcSettlement *origin = CcSimSettlement(
        sim, sim->journey.origin_id);
    const CcRoute *route = CcSimRoute(sim, sim->journey.route_id);
    if (destination == NULL || route == NULL) return;
    if (sim->journey.situation_id != 0U &&
        sim->player.accepted_situation_id == sim->journey.situation_id) {
        const CcSituation *situation = CcSimSituation(
            sim, sim->journey.situation_id);
        bool delivery = situation != NULL &&
            (situation->kind == CC_SITUATION_RELIEF_DELIVERY ||
             situation->kind == CC_SITUATION_BLACK_MARKET_DELIVERY);
        int32_t remaining = situation != NULL ?
            situation->quantity - situation->progress : 0;
        bool delivered_load_arrived = !delivery ||
            (destination->id == situation->target_id &&
             situation->good >= 0 && situation->good < CC_GOOD_COUNT &&
             sim->player.cargo[situation->good] >= remaining);
        if (delivered_load_arrived) {
            if (sim->resolved_journey_situation_id !=
                sim->journey.situation_id) {
                sim->resolved_journey_outcome = CC_JOURNEY_OUTCOME_NONE;
            }
            sim->resolved_journey_situation_id = sim->journey.situation_id;
        } else {
            sim->resolved_journey_situation_id = 0U;
            sim->resolved_journey_outcome = CC_JOURNEY_OUTCOME_NONE;
        }
    }
    sim->player.location_id = destination->id;
    for (int32_t i = 0; i < sim->treasure_count; ++i) {
        CcTreasure *treasure = &sim->treasures[i];
        if (!treasure->destroyed &&
            treasure->owner_id == sim->player.id) {
            treasure->location_id = destination->id;
        }
    }
    sim->journey.elapsed_subticks = sim->journey.total_subticks;
    sim->journey.active = false;
    sim->journey.phase = CC_JOURNEY_PHASE_NONE;
    sim->clock.game_minutes_per_second =
        CC_IDLE_GAME_MINUTES_PER_SECOND;
    sim->carriage = (CcCarriageState){
        .mode = CC_CARRIAGE_PARKED,
        .location_id = destination->id,
        .condition = sim->carriage.condition
    };
    for (int32_t i = 0; i < sim->courier_count; ++i) {
        CcCourier *courier = &sim->couriers[i];
        if (courier->status != CC_COURIER_WITH_PLAYER) continue;
        courier->current_settlement_id = destination->id;
        courier->reliability = ClampI32(
            courier->reliability - sim->journey.danger / 12, 0, 100);
        if (courier->destination_settlement_id == destination->id) {
            DeliverCourier(sim, courier, true);
        }
    }
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(text, sizeof(text),
                   "The carriage reaches %.24s from %.24s along a %d-day road at %s pace (%d%% danger).",
                   destination->name,
                   origin != NULL ? origin->name : "the road",
                   sim->journey.total_subticks / CC_WORLD_DAY_SUBTICKS,
                   CcJourneyPaceName(sim->journey.pace),
                   sim->journey.danger);
    (void)PushEvent(sim, CC_EVENT_PLAYER_TRAVEL, sim->player.id,
                    destination->id, sim->journey.parent_event_id,
                    sim->journey.total_subticks / CC_WORLD_DAY_SUBTICKS,
                    text);
    DeliverDelayedEchoIfReady(sim);
}

static void InterruptJourney(CcSim *sim)
{
    const CcSettlement *destination = CcSimSettlement(
        sim, sim->journey.destination_id);
    const CcSituation *situation = CcSimSituation(
        sim, sim->journey.situation_id);
    const CcRoute *route = CcSimRoute(sim, sim->journey.route_id);
    const CcBanditGroup *bandits = BanditsOnRoute(
        sim, sim->journey.route_id);
    char text[CC_EVENT_TEXT_CAPACITY];
    if (route != NULL && route->closed && bandits == NULL) {
        (void)snprintf(
            text, sizeof(text),
            "Captain Ilyra Senn lowers Alderwatch's bright chain across the road. 'Orders,' she says, while a hungry boy eats too quickly on the wall.");
    } else if (situation == NULL) {
        (void)snprintf(
            text, sizeof(text),
            "The warned riders from %.24s close the road to %.24s. The carriage stops before anything is taken.",
            bandits != NULL ? bandits->name : "an armed roadside company",
            destination != NULL ? destination->name : "the far gate");
    } else {
        (void)snprintf(text, sizeof(text),
                       "Members of %.24s block the road to %.24s; %.24s awaits a Crownless choice.",
                       bandits != NULL ? bandits->name : "Armed road collectors",
                       destination != NULL ? destination->name : "the far gate",
                       situation->affected_name[0] != '\0' ?
                           situation->affected_name : "a local household");
    }
    CcEvent *event = PushEvent(
        sim, CC_EVENT_JOURNEY_ENCOUNTER, sim->journey.situation_id,
        sim->journey.route_id, sim->journey.parent_event_id,
        sim->journey.danger, text);
    if (event != NULL) sim->journey.parent_event_id = event->id;
    sim->journey.encounter_triggered = true;
    sim->journey.phase = CC_JOURNEY_PHASE_BLOCKED;
    sim->clock.game_minutes_per_second = 0;
    sim->carriage.mode = CC_CARRIAGE_STOPPED;
    sim->carriage.speed_milli_per_second = 0;
}

static int32_t JourneyCarriageSpeed(int32_t total_subticks)
{
    if (total_subticks <= 0) return 0;
    const int32_t represented_route_millimetres = 52000;
    return (int32_t)(((int64_t)represented_route_millimetres *
                      CC_TRAVEL_GAME_MINUTES_PER_SECOND *
                      CC_WORLD_TICKS_PER_SECOND) / total_subticks);
}

static int32_t JourneyPaceRate(CcJourneyPace pace)
{
    switch (pace) {
        case CC_JOURNEY_PACE_CAREFUL: return 24;
        case CC_JOURNEY_PACE_STEADY: return CC_TRAVEL_GAME_MINUTES_PER_SECOND;
        case CC_JOURNEY_PACE_PUSH: return 38;
    }
    return CC_TRAVEL_GAME_MINUTES_PER_SECOND;
}

const char *CcJourneyPaceName(CcJourneyPace pace)
{
    switch (pace) {
        case CC_JOURNEY_PACE_CAREFUL: return "CAREFUL";
        case CC_JOURNEY_PACE_STEADY: return "STEADY";
        case CC_JOURNEY_PACE_PUSH: return "PUSH";
    }
    return "STEADY";
}

static int32_t JourneyCarriageSpeedForPace(int32_t total_subticks,
                                            CcJourneyPace pace)
{
    return JourneyCarriageSpeed(total_subticks) * JourneyPaceRate(pace) /
        CC_TRAVEL_GAME_MINUTES_PER_SECOND;
}

int32_t CcSimJourneyEtaMinutes(const CcSim *sim)
{
    if (sim == NULL || !sim->journey.active ||
        sim->journey.elapsed_subticks >= sim->journey.total_subticks) return 0;
    int32_t remaining = sim->journey.total_subticks -
                        sim->journey.elapsed_subticks;
    int32_t pace_rate = JourneyPaceRate(sim->journey.pace);
    int64_t world_subticks =
        ((int64_t)remaining * CC_TRAVEL_GAME_MINUTES_PER_SECOND +
         pace_rate - 1) / pace_rate;
    return (int32_t)((world_subticks + CC_WORLD_MINUTE_SUBTICKS - 1) /
                     CC_WORLD_MINUTE_SUBTICKS);
}

static int32_t HorseFeedRequired(int32_t travel_days)
{
    return MaximumI32(1, (travel_days + 1) / 2);
}

bool CcSimTravelPreview(const CcSim *sim, CcId destination_id,
                        CcTravelPreview *preview, char *error,
                        size_t error_capacity)
{
    if (sim == NULL || preview == NULL) {
        SetError(error, error_capacity, "Travel preview state is missing.");
        return false;
    }
    const CcSettlement *destination = CcSimSettlement(sim, destination_id);
    if (destination == NULL) {
        SetError(error, error_capacity, "That destination does not exist.");
        return false;
    }
    const CcRoute *route = CcSimRouteBetween(
        sim, sim->player.location_id, destination->id);
    if (route == NULL) {
        SetError(error, error_capacity,
                 "No direct carriage route connects those places.");
        return false;
    }
    const CcMap *map = CcSimMapForRoute(sim, route->id, sim->player.id);
    const CcSituation *accepted = CcSimAcceptedSituation(sim);
    bool sponsored_night_passage = route->smuggler_route &&
        accepted != NULL &&
        accepted->kind == CC_SITUATION_BLACK_MARKET_DELIVERY &&
        route->to_id == accepted->target_id;
    bool uncharted = map == NULL && !sponsored_night_passage;
    int32_t readiness = sim->schema_version >= 14U ?
        CcSimHorseTeamReadiness(sim) : 100;
    int32_t days = route->travel_days + (uncharted ? 2 : 0) +
                   (readiness < 70 ? 1 : 0) +
                   (readiness < 45 ? 1 : 0);
    int32_t base_fare = days + (route->smuggler_route ? 3 : 0);
    int32_t shadow_danger = DragonRouteShadowDanger(sim, route);
    *preview = (CcTravelPreview){
        .route_id = route->id,
        .destination_id = destination->id,
        .provision_cost = base_fare + TradeRouteToll(sim, route),
        .travel_days = days,
        .claimed_condition = map != NULL ? map->recorded_condition : -1,
        .claimed_danger = map != NULL ?
            ClampI32(map->recorded_danger + shadow_danger, 0, 95) : -1,
        .chart_accuracy = map != NULL ? map->accuracy : 0,
        .horse_feed_required = HorseFeedRequired(days),
        .horse_readiness = readiness,
        .charted = map != NULL,
        .destination_known = !route->smuggler_route || map != NULL ||
                             sponsored_night_passage,
        .sponsored_guide = sponsored_night_passage
    };
    return true;
}

static bool ApplyTravel(CcSim *sim, const CcCommand *command,
                        char *error, size_t error_capacity)
{
    if (sim->journey.active) {
        SetError(error, error_capacity,
                 "Resolve the encounter already blocking the carriage.");
        return false;
    }
    CcTravelPreview preview = {0};
    if (!CcSimTravelPreview(sim, command->target_id, &preview,
                            error, error_capacity)) return false;
    const CcSettlement *destination = CcSimSettlement(
        sim, preview.destination_id);
    const CcRoute *route = CcSimRoute(sim, preview.route_id);
    const CcSituation *accepted = CcSimAcceptedSituation(sim);
    bool sponsored_night_passage = preview.sponsored_guide;
    bool delivery = accepted != NULL &&
        (accepted->kind == CC_SITUATION_RELIEF_DELIVERY ||
         accepted->kind == CC_SITUATION_BLACK_MARKET_DELIVERY);
    bool full_contract_load = delivery &&
        sim->player.cargo[accepted->good] >=
            accepted->quantity - accepted->progress;
    bool sanctioned_closed_crossing = route->closed &&
        accepted != NULL &&
        accepted->kind == CC_SITUATION_RELIEF_DELIVERY &&
        full_contract_load;
    bool uncharted = !preview.charted && !sponsored_night_passage;
    int32_t days = preview.travel_days;
    int32_t base_fare = days + (route->smuggler_route ? 3 : 0);
    CcMoney toll = TradeRouteToll(sim, route);
    CcMoney fare = preview.provision_cost;
    const CcSettlement *origin = CcSimSettlement(
        sim, sim->player.location_id);
    if (sim->schema_version >= 14U && preview.horse_readiness < 30) {
        SetError(error, error_capacity,
                 "The horse team needs food and rest before another journey.");
        return false;
    }
    if (sim->schema_version >= 15U) {
        for (int32_t i = 0; i < CC_CARRIAGE_HORSE_COUNT; ++i) {
            int32_t due = sim->horse_team[i].pregnancy_days_remaining;
            if (due > 0 && due <= 30) {
                SetError(error, error_capacity,
                         "A mare near foaling must remain at the stable.");
                return false;
            }
        }
    }
    if (sim->schema_version >= 14U && (origin == NULL ||
        origin->stock[CC_GOOD_FOOD] < preview.horse_feed_required)) {
        SetError(error, error_capacity,
                 "The departure market lacks enough fodder for the horse team.");
        return false;
    }
    if (sim->player.coins < fare) {
        SetError(error, error_capacity, "The company cannot provision that journey.");
        return false;
    }
    bool contract_journey = false;
    if (accepted != NULL && full_contract_load) {
        contract_journey = accepted->kind == CC_SITUATION_RELIEF_DELIVERY ||
            (accepted->kind == CC_SITUATION_BLACK_MARKET_DELIVERY &&
             route->smuggler_route);
    }
    if (accepted != NULL &&
        accepted->kind == CC_SITUATION_MONSTER_EXPEDITION &&
        destination->id == CcSimSituationOfferSettlementId(sim, accepted)) {
        contract_journey = true;
    }
    if (accepted != NULL &&
        accepted->kind == CC_SITUATION_COURIER_DELIVERY) {
        CcCourier *courier = CourierMutable(sim, accepted->target_id);
        if (courier != NULL &&
            courier->status == CC_COURIER_WITH_PLAYER) {
            contract_journey = true;
        }
    }
    bool encounter_planned = contract_journey &&
        (sanctioned_closed_crossing ||
         (accepted != NULL &&
          accepted->kind == CC_SITUATION_BLACK_MARKET_DELIVERY &&
          route->smuggler_route));
    int32_t danger = ClampI32(
        CcSimRouteDanger(sim, route->id) +
        DragonRouteShadowDanger(sim, route), 0, 95);
    if (uncharted) danger = ClampI32(danger + 20, 0, 95);
    int32_t reaction = CcSimBanditReactionRoll(sim, route->id);
    int32_t bargain_cost = ClampI32(
        4 + danger / 7 + (reaction <= 5 ? 3 : reaction >= 10 ? -2 : 0),
        3, 21);
    int32_t total_subticks = days * CC_WORLD_DAY_SUBTICKS;
    CcId parent_event_id = LatestLocalCause(sim, destination->id);
    bool ambush_pending = !encounter_planned &&
        (int32_t)(NextRandom(sim) % 100U) < danger / 2;
    sim->resolved_journey_situation_id = 0U;
    sim->resolved_journey_outcome = CC_JOURNEY_OUTCOME_NONE;
    sim->player.coins -= fare;
    CcSettlement *origin_market = CcSimSettlementMutable(
        sim, sim->player.location_id);
    if (origin_market != NULL) {
        origin_market->market_coins += base_fare;
        if (sim->schema_version >= 14U) {
            origin_market->stock[CC_GOOD_FOOD] -=
                preview.horse_feed_required;
        }
    }
    if (sim->schema_version >= 14U) {
        for (int32_t i = 0; i < CC_CARRIAGE_HORSE_COUNT; ++i) {
            sim->horse_team[i].hunger = ClampI32(
                sim->horse_team[i].hunger -
                    preview.horse_feed_required * 12, 0, 100);
        }
    }
    CcKingdom *toll_kingdom = KingdomMutable(sim, destination->kingdom_id);
    if (toll_kingdom != NULL) toll_kingdom->treasury += toll;
    sim->journey = (CcJourneyEncounter){
        .active = true,
        .phase = CC_JOURNEY_PHASE_TRAVELLING,
        .situation_id = contract_journey ? accepted->id : 0U,
        .origin_id = sim->player.location_id,
        .destination_id = destination->id,
        .route_id = route->id,
        .danger = danger,
        .bargain_cost = bargain_cost,
        .departure_day = sim->current_day,
        .total_subticks = total_subticks,
        .encounter_subticks = encounter_planned ?
            total_subticks * 35 / 100 : 0,
        .fare_reserved = (int32_t)fare,
        .pace = CC_JOURNEY_PACE_STEADY,
        .ambush_pending = ambush_pending,
        .encounter_triggered = contract_journey && !encounter_planned,
        .parent_event_id = parent_event_id
    };
    sim->clock.game_minutes_per_second =
        CC_TRAVEL_GAME_MINUTES_PER_SECOND;
    sim->carriage = (CcCarriageState){
        .mode = CC_CARRIAGE_MOVING,
        .route_id = route->id,
        .origin_id = sim->player.location_id,
        .destination_id = destination->id,
        .speed_milli_per_second =
            JourneyCarriageSpeedForPace(
                total_subticks, CC_JOURNEY_PACE_STEADY),
        .condition = sim->carriage.condition
    };
    char text[CC_EVENT_TEXT_CAPACITY];
    if (sim->schema_version >= 14U) {
        (void)snprintf(
            text, sizeof(text),
            "%.16s and %.16s pull from %.16s toward %.16s for %d days with %d fodder.",
            sim->horse_team[0].name, sim->horse_team[1].name,
            origin != NULL ? origin->name : "the waystation",
            destination->name, days, preview.horse_feed_required);
    } else {
        (void)snprintf(
            text, sizeof(text),
            "The Crownless carriage leaves %s for %s with %d days of provisions reserved.",
            origin != NULL ? origin->name : "the waystation",
            destination->name, days);
    }
    CcEvent *departure = PushEvent(
        sim, CC_EVENT_JOURNEY_DEPARTED, sim->player.id, route->id,
        parent_event_id, days, text);
    if (departure != NULL) sim->journey.parent_event_id = departure->id;
    SetError(error, error_capacity, "");
    return true;
}

static int32_t RollD6(CcSim *sim)
{
    return 1 + (int32_t)(NextRandom(sim) % 6U);
}

/* OSR encounter loot: the ledger decides what exists (the group's real
 * supplies stock), the 2d6 table decides what the company recovers. Loot is
 * carried, never minted: every recovered good is deducted from the defeated
 * group's supplies at a fixed ten-supplies-per-good rate, bounded by free
 * cargo slots. A natural 12 takes a trophy: a named curio with provenance
 * that occupies one carriage slot and sells at markets like any treasure. */
static void RollEncounterLoot(CcSim *sim, CcBanditGroup *bandits,
                              const CcJourneyEncounter *journey,
                              const CcEvent *combat_event)
{
    const int32_t roll = RollD6(sim) + RollD6(sim);
    if (bandits == NULL || roll < 4) return;

    CcGood good = CC_GOOD_FOOD;
    if (bandits->raid_phase == CC_BANDIT_RAID_RETURNING &&
        bandits->raid_good >= 0 && bandits->raid_good < CC_GOOD_COUNT) {
        good = bandits->raid_good;
    }
    const int32_t recoverable = bandits->supplies / 10;
    int32_t take = 0;
    if (roll <= 5) {
        take = recoverable * RollD6(sim) / 10;
    } else if (roll <= 8) {
        take = recoverable / 2;
    } else {
        take = recoverable;
    }

    int32_t free_slots = sim->player.cargo_capacity -
                         CcPlayerCargoUsed(&sim->player);
    if (free_slots < 0) free_slots = 0;
    const int32_t cap = free_slots * GoodUnitsPerCargoSlot(good);
    if (take > cap) take = cap;

    char text[CC_EVENT_TEXT_CAPACITY];
    size_t length = 0;
    int32_t magnitude = 0;
    CcTreasure *trophy = NULL;

    if (take > 0) {
        sim->player.cargo[good] += take;
        bandits->supplies = ClampI32(bandits->supplies - take * 10, 0, 100);
        magnitude += take;
        length += (size_t)snprintf(
            text + length, sizeof(text) - length,
            "The company strips the broken cordon: %d %s recovered",
            take, CcGoodName(good));
    }

    if (roll >= 12) {
        if (take == 0) {
            length += (size_t)snprintf(
                text + length, sizeof(text) - length,
                "The broken cordon yields little");
        }
        if (CcPlayerCargoUsed(&sim->player) <
                sim->player.cargo_capacity &&
            sim->treasure_count < CC_MAX_TREASURES) {
            trophy = AllocateTreasure(sim);
        }
        if (trophy != NULL) {
            const int32_t value = RollD6(sim) + RollD6(sim);
            (void)snprintf(trophy->name, sizeof(trophy->name),
                           "Outlaw Trophy of %.16s", bandits->name);
            trophy->owner_id = sim->player.id;
            trophy->location_id = sim->player.location_id;
            trophy->appraised_value = value;
            trophy->created_day = sim->current_day;
            sim->player.treasure_cargo_slots += 1;
            magnitude += value;
            length += (size_t)snprintf(
                text + length, sizeof(text) - length,
                "%sthe outlaw trophy %s (%d crowns)",
                length > 0 && text[length - 1] != ' ' ? ", " : "",
                trophy->name, value);
        } else if (length > 0 && length < sizeof(text) - 1) {
            length += (size_t)snprintf(
                text + length, sizeof(text) - length,
                "; the trophy is left on the road");
        }
    }

    if (length > 0) {
        length += (size_t)snprintf(
            text + length, sizeof(text) - length, ".");
        (void)PushEvent(sim, CC_EVENT_ENCOUNTER_LOOT, journey->situation_id,
                        journey->route_id,
                        combat_event != NULL ? combat_event->id : 0U,
                        magnitude, text);
    }
}

static bool ApplyResolveEncounter(CcSim *sim, CcJourneyOutcome outcome,
                                  bool provisions,
                                  char *error, size_t error_capacity)
{
    if (!sim->journey.active ||
        sim->journey.phase != CC_JOURNEY_PHASE_BLOCKED ||
        (outcome != CC_JOURNEY_OUTCOME_COMBAT &&
         outcome != CC_JOURNEY_OUTCOME_NEGOTIATED)) {
        SetError(error, error_capacity, "There is no unresolved road encounter.");
        return false;
    }
    CcJourneyEncounter journey = sim->journey;
    CcRoute *route = RouteMutable(sim, journey.route_id);
    CcSettlement *origin = CcSimSettlementMutable(sim, journey.origin_id);
    CcSettlement *destination = CcSimSettlementMutable(
        sim, journey.destination_id);
    CcBanditGroup *bandits = BanditsOnRoute(sim, journey.route_id);
    if (route == NULL || origin == NULL || destination == NULL ||
        sim->player.location_id != journey.origin_id) {
        SetError(error, error_capacity,
                 "The road encounter no longer matches the prepared journey.");
        return false;
    }
    if (!provisions && outcome == CC_JOURNEY_OUTCOME_NEGOTIATED &&
        sim->player.coins < journey.bargain_cost) {
        SetError(error, error_capacity,
                 "The company cannot cover the negotiated passage.");
        return false;
    }
    CcGood demanded_good = CC_GOOD_FOOD;
    int32_t demanded_quantity = 0;
    if (provisions &&
        (!CcSimBanditProvisionDemand(sim, journey.route_id,
                                     &demanded_good, &demanded_quantity) ||
         sim->player.cargo[demanded_good] < demanded_quantity)) {
        SetError(error, error_capacity,
                 "The carriage does not carry the provisions they asked for.");
        return false;
    }
    char text[CC_EVENT_TEXT_CAPACITY];
    CcEventKind event_kind;
    int32_t magnitude;
    if (outcome == CC_JOURNEY_OUTCOME_COMBAT) {
        int32_t damage = ClampI32(7 + journey.danger / 8, 8, 20);
        int32_t medical_cost = MinimumI32(
            (int32_t)sim->player.coins, 3 + journey.danger / 12);
        sim->carriage.condition = ClampI32(
            sim->carriage.condition - damage, 0, 100);
        sim->player.coins -= medical_cost;
        origin->market_coins += medical_cost;
        route->security = ClampI32(route->security + 6, 0, 100);
        destination->security = ClampI32(destination->security + 2, 0, 100);
        if (bandits != NULL) {
            bandits->members = ClampI32(bandits->members - 3, 0, 200);
            bandits->supplies = ClampI32(bandits->supplies - 4, 0, 100);
            bandits->influence = ClampI32(bandits->influence - 3, 0, 100);
        }
        (void)snprintf(text, sizeof(text),
                       "The company breaks the cordon, but the carriage takes %d damage and wounds cost %d crowns.",
                       damage, medical_cost);
        event_kind = CC_EVENT_ENCOUNTER_COMBAT;
        magnitude = damage;
    } else if (!provisions) {
        sim->player.coins -= journey.bargain_cost;
        destination->market_coins += journey.bargain_cost;
        route->security = ClampI32(route->security - 1, 0, 100);
        destination->prosperity = ClampI32(destination->prosperity + 1, 0, 100);
        if (bandits != NULL) {
            bandits->supplies = ClampI32(bandits->supplies + 4, 0, 100);
            bandits->influence = ClampI32(bandits->influence + 3, 0, 100);
        }
        (void)snprintf(text, sizeof(text),
                       "The Crownless company buys passage for %d crowns; commerce moves immediately, but the collectors grow stronger.",
                       journey.bargain_cost);
        event_kind = CC_EVENT_ENCOUNTER_NEGOTIATED;
        magnitude = journey.bargain_cost;
    } else {
        int32_t supply_value = demanded_good == CC_GOOD_WEAPONS ? 4 :
                               demanded_good == CC_GOOD_TOOLS ? 3 :
                               demanded_good == CC_GOOD_IRON ? 2 : 1;
        sim->player.cargo[demanded_good] -= demanded_quantity;
        route->security = ClampI32(route->security - 1, 0, 100);
        if (bandits != NULL) {
            bandits->supplies = ClampI32(
                bandits->supplies + demanded_quantity * supply_value,
                0, 100);
            bandits->influence = ClampI32(bandits->influence + 1, 0, 100);
            if ((bandits->raid_phase == CC_BANDIT_RAID_SCOUTING ||
                 bandits->raid_phase == CC_BANDIT_RAID_MUSTERING) &&
                bandits->raid_good == demanded_good) {
                bandits->raid_phase = CC_BANDIT_RAID_IDLE;
                bandits->raid_target_id = 0U;
                bandits->raid_quantity = 0;
                bandits->raid_days_remaining = 0;
            }
        }
        (void)snprintf(
            text, sizeof(text),
            "The Crownless company gives %d %s to %s; their immediate shortage eases and the cordon opens.",
            demanded_quantity, CcGoodName(demanded_good),
            bandits != NULL ? bandits->name : "the road company");
        event_kind = CC_EVENT_ENCOUNTER_NEGOTIATED;
        magnitude = demanded_quantity;
    }
    CcEvent *outcome_event = PushEvent(
        sim, event_kind, journey.situation_id, journey.route_id,
        journey.parent_event_id, magnitude, text);
    sim->journey.phase = CC_JOURNEY_PHASE_TRAVELLING;
    sim->journey.parent_event_id = outcome_event != NULL ?
        outcome_event->id : journey.parent_event_id;
    if (outcome == CC_JOURNEY_OUTCOME_COMBAT) {
        RollEncounterLoot(sim, bandits, &journey, outcome_event);
    }
    sim->resolved_journey_situation_id = journey.situation_id;
    sim->resolved_journey_outcome = outcome;
    sim->clock.game_minutes_per_second =
        CC_TRAVEL_GAME_MINUTES_PER_SECOND;
    sim->carriage.mode = CC_CARRIAGE_MOVING;
    sim->carriage.speed_milli_per_second =
        JourneyCarriageSpeedForPace(
            sim->journey.total_subticks, sim->journey.pace);
    CreateJourneyTraffic(sim, &journey,
                         outcome_event != NULL ? outcome_event->id : 0U);
    SetError(error, error_capacity, "");
    return true;
}

static bool ApplyWithdrawEncounter(CcSim *sim, const CcCommand *command,
                                   char *error, size_t error_capacity)
{
    if (!sim->journey.active ||
        sim->journey.phase != CC_JOURNEY_PHASE_BLOCKED ||
        (command->amount != 0 && command->amount != 1)) {
        SetError(error, error_capacity,
                 "There is no road fight to withdraw from.");
        return false;
    }
    CcJourneyEncounter journey = sim->journey;
    CcRoute *route = RouteMutable(sim, journey.route_id);
    CcSettlement *origin = CcSimSettlementMutable(sim, journey.origin_id);
    CcBanditGroup *bandits = BanditsOnRoute(sim, journey.route_id);
    if (route == NULL || origin == NULL ||
        sim->player.location_id != journey.origin_id) {
        SetError(error, error_capacity,
                 "The road withdrawal no longer matches this journey.");
        return false;
    }
    bool under_fire = command->amount == 1;
    int32_t damage = under_fire ? ClampI32(4 + journey.danger / 15,
                                           4, 10) : 0;
    int32_t medical_cost = under_fire ? MinimumI32(
        (int32_t)sim->player.coins, 2 + journey.danger / 20) : 0;
    sim->carriage.condition = ClampI32(
        sim->carriage.condition - damage, 0, 100);
    sim->player.coins -= medical_cost;
    origin->market_coins += medical_cost;
    route->security = ClampI32(route->security - (under_fire ? 2 : 1),
                               0, 100);
    if (bandits != NULL) {
        bandits->influence = ClampI32(
            bandits->influence + (under_fire ? 2 : 1), 0, 100);
    }
    char text[CC_EVENT_TEXT_CAPACITY];
    if (under_fire) {
        (void)snprintf(
            text, sizeof(text),
            "The Crownless carriage withdraws under fire to %s; it takes %d damage and treatment costs %d crowns.",
            origin->name, damage, medical_cost);
    } else {
        (void)snprintf(
            text, sizeof(text),
            "The Crownless carriage refuses the fight and returns to %s before blood is drawn.",
            origin->name);
    }
    (void)PushEvent(sim, CC_EVENT_ENCOUNTER_WITHDRAWN, sim->player.id,
                    journey.route_id, journey.parent_event_id,
                    under_fire ? damage : 0, text);
    sim->resolved_journey_situation_id = 0U;
    sim->resolved_journey_outcome = CC_JOURNEY_OUTCOME_NONE;
    sim->journey.active = false;
    sim->journey.phase = CC_JOURNEY_PHASE_NONE;
    sim->clock.game_minutes_per_second = CC_IDLE_GAME_MINUTES_PER_SECOND;
    sim->carriage = (CcCarriageState){
        .mode = CC_CARRIAGE_PARKED,
        .location_id = origin->id,
        .condition = sim->carriage.condition
    };
    SetError(error, error_capacity, "");
    return true;
}

static bool ApplyJourneyPace(CcSim *sim, const CcCommand *command,
                             char *error, size_t error_capacity)
{
    if (sim == NULL || command == NULL || !sim->journey.active ||
        sim->journey.phase != CC_JOURNEY_PHASE_TRAVELLING) {
        SetError(error, error_capacity,
                 "Pace can only change while the carriage is moving.");
        return false;
    }
    if (command->amount < CC_JOURNEY_PACE_CAREFUL ||
        command->amount > CC_JOURNEY_PACE_PUSH) {
        SetError(error, error_capacity, "Journey pace is invalid.");
        return false;
    }
    sim->journey.pace = (CcJourneyPace)command->amount;
    sim->carriage.speed_milli_per_second = JourneyCarriageSpeedForPace(
        sim->journey.total_subticks, sim->journey.pace);
    SetError(error, error_capacity, "");
    return true;
}

void CcSimAdvanceRuntimeTicks(CcSim *sim, int32_t ticks)
{
    if (sim == NULL || ticks <= 0 || !sim->journey.active ||
        sim->journey.phase != CC_JOURNEY_PHASE_TRAVELLING ||
        sim->clock.tick > UINT64_MAX - (uint64_t)ticks) return;
    for (int32_t tick = 0; tick < ticks; ++tick) {
        if (!sim->journey.active ||
            sim->journey.phase != CC_JOURNEY_PHASE_TRAVELLING) break;
        sim->clock.tick += 1U;
        int32_t clock_rate = CC_TRAVEL_GAME_MINUTES_PER_SECOND;
        int32_t journey_rate = JourneyPaceRate(sim->journey.pace);
        sim->clock.game_minutes_per_second = clock_rate;
        sim->clock.minute_subticks += clock_rate;
        while (sim->clock.minute_subticks >= CC_WORLD_DAY_SUBTICKS) {
            sim->clock.minute_subticks -= CC_WORLD_DAY_SUBTICKS;
            CcSimAdvanceDays(sim, 1);
        }
        sim->journey.elapsed_subticks = MinimumI32(
            sim->journey.total_subticks,
            sim->journey.elapsed_subticks + journey_rate);
        sim->carriage.progress_milli = sim->journey.total_subticks > 0 ?
            (int32_t)(((int64_t)sim->journey.elapsed_subticks * 1000) /
                      sim->journey.total_subticks) : 0;
        if (sim->journey.ambush_pending &&
            !sim->journey.ambush_warned &&
            sim->journey.elapsed_subticks >=
                sim->journey.total_subticks * 45 / 100) {
            WarnJourneyAmbush(sim);
        }
        if (sim->journey.ambush_pending &&
            sim->journey.elapsed_subticks >=
                sim->journey.total_subticks * 60 / 100) {
            ResolveWarnedJourneyAmbush(sim);
            if (sim->journey.phase == CC_JOURNEY_PHASE_BLOCKED) continue;
        }
        if (!sim->journey.encounter_triggered &&
            sim->journey.situation_id != 0U &&
            sim->journey.elapsed_subticks >=
                sim->journey.encounter_subticks) {
            InterruptJourney(sim);
            continue;
        }
        if (sim->journey.elapsed_subticks >= sim->journey.total_subticks) {
            FinishJourneyArrival(sim);
        }
    }
}

static bool ApplyRepair(CcSim *sim, const CcCommand *command,
                        char *error, size_t error_capacity)
{
    CcRoute *route = RouteMutable(sim, command->target_id);
    if (route == NULL || !route->closed) {
        SetError(error, error_capacity, "That route does not require repair.");
        return false;
    }
    if (sim->player.location_id != route->from_id &&
        sim->player.location_id != route->to_id) {
        SetError(error, error_capacity, "The carriage must reach an end of the route first.");
        return false;
    }
    bool use_tools = command->amount == 1 ||
        (command->amount == 0 && sim->player.cargo[CC_GOOD_TOOLS] >= 2);
    bool use_cash = command->amount == 2 ||
        (command->amount == 0 && !use_tools);
    if (command->amount < 0 || command->amount > 2) {
        SetError(error, error_capacity,
                 "Choose a tools repair or a cash repair.");
        return false;
    }
    if (use_tools && sim->player.cargo[CC_GOOD_TOOLS] >= 2) {
        sim->player.cargo[CC_GOOD_TOOLS] -= 2;
    } else if (use_cash && sim->player.coins >= 18) {
        sim->player.coins -= 18;
    } else {
        SetError(error, error_capacity, use_tools ?
                 "The tools repair requires 2 tools." :
                 "The cash repair requires 18 crowns.");
        return false;
    }
    CcSimAdvanceDays(sim, use_tools ? 1 : 3);
    route = RouteMutable(sim, command->target_id);
    if (route == NULL) return false;
    route->closed = false;
    route->condition = use_tools ? 92 : 76;
    route->security = ClampI32(route->security + (use_tools ? 10 : 4),
                               0, 100);
    CcSettlement *nearby = CcSimSettlementMutable(sim, route->from_id);
    if (use_cash && nearby != NULL) nearby->market_coins += 18;
    CcFaction *beneficiary = nearby != NULL ? FactionFor(
        sim, nearby->kingdom_id,
        use_tools ? CC_FACTION_GUILD : CC_FACTION_CROWN) : NULL;
    if (beneficiary != NULL) {
        beneficiary->support = ClampI32(beneficiary->support + 5, 0, 100);
    }
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(text, sizeof(text),
                   use_tools ?
                   "Local crews use the company's tools to reopen the treaty bridge." :
                   "Paid officials reopen the treaty bridge after three days of fees and delay.");
    (void)PushEvent(sim, CC_EVENT_ROUTE_REPAIRED, route->id,
                    sim->player.location_id, 0, route->condition, text);
    sim->player.reputation += 2;
    ResolveTargetSituations(sim, CC_SITUATION_ROUTE_REPAIR, route->id);
    SetError(error, error_capacity, "");
    return true;
}

static bool ApplyDungeonChange(CcSim *sim, const CcCommand *command,
                               char *error, size_t error_capacity)
{
    CcDungeon *dungeon = NULL;
    for (int32_t i = 0; i < sim->dungeon_count; ++i) {
        if (sim->dungeons[i].id == command->target_id) dungeon = &sim->dungeons[i];
    }
    if (dungeon == NULL || sim->player.location_id != dungeon->settlement_id) {
        SetError(error, error_capacity, "The company must prepare at the dungeon settlement.");
        return false;
    }
    if (command->dungeon_state != CC_DUNGEON_EXPLORED &&
        command->dungeon_state != CC_DUNGEON_PUBLIC_ROUTE &&
        command->dungeon_state != CC_DUNGEON_SMUGGLER_ROUTE &&
        command->dungeon_state != CC_DUNGEON_RESEALED) {
        SetError(error, error_capacity, "That dungeon outcome is invalid.");
        return false;
    }
    if (dungeon->state == command->dungeon_state) {
        SetError(error, error_capacity,
                 "The dungeon already has that lasting outcome.");
        return false;
    }
    CcRoute *mine_road = DungeonRouteMutable(sim, dungeon);
    if (command->dungeon_state != CC_DUNGEON_EXPLORED &&
        mine_road == NULL) {
        SetError(error, error_capacity,
                 "No mine road is linked to that dungeon.");
        return false;
    }
    CcDungeonState previous_state = dungeon->state;
    CcDungeonEffects previous_effects = DungeonEffects(previous_state);
    CcDungeonEffects next_effects = DungeonEffects(command->dungeon_state);
    int32_t tools = command->dungeon_state == CC_DUNGEON_PUBLIC_ROUTE ? 2 :
                    command->dungeon_state == CC_DUNGEON_SMUGGLER_ROUTE ? 1 :
                    command->dungeon_state == CC_DUNGEON_RESEALED ? 3 : 0;
    int32_t crowns = command->dungeon_state == CC_DUNGEON_PUBLIC_ROUTE ? 12 :
                     command->dungeon_state == CC_DUNGEON_SMUGGLER_ROUTE ? 6 : 0;
    if (sim->player.cargo[CC_GOOD_TOOLS] < tools ||
        sim->player.coins < crowns) {
        SetError(error, error_capacity,
                 command->dungeon_state == CC_DUNGEON_PUBLIC_ROUTE ?
                 "A public route needs 2 tools and 12 crowns." :
                 command->dungeon_state == CC_DUNGEON_SMUGGLER_ROUTE ?
                 "A smuggler route needs 1 tool and 6 crowns." :
                 "A lasting seal needs 3 tools.");
        return false;
    }
    sim->player.cargo[CC_GOOD_TOOLS] -= tools;
    sim->player.coins -= crowns;
    CcSimAdvanceDays(sim, 3);
    dungeon->state = command->dungeon_state;
    dungeon->regional_pressure = command->dungeon_state == CC_DUNGEON_RESEALED ? 12 :
                                 command->dungeon_state == CC_DUNGEON_PUBLIC_ROUTE ? 28 :
                                 command->dungeon_state == CC_DUNGEON_SMUGGLER_ROUTE ? 38 : 40;
    CcSettlement *mine = CcSimSettlementMutable(sim, dungeon->settlement_id);
    mine_road = DungeonRouteMutable(sim, dungeon);
    if (mine != NULL) mine->market_coins += crowns;
    const char *event_text = NULL;
    if (mine != NULL) {
        mine->production[CC_GOOD_MATERIAL] = MaximumI32(
            0, mine->production[CC_GOOD_MATERIAL] +
               next_effects.mine_production -
               previous_effects.mine_production);
        mine->prosperity = ClampI32(
            mine->prosperity + next_effects.prosperity -
                previous_effects.prosperity,
            0, 100);
    }
    if (mine_road != NULL) {
        mine_road->capacity = ClampI32(
            mine_road->capacity + next_effects.route_capacity -
                previous_effects.route_capacity,
            1, 40);
        mine_road->security = ClampI32(
            mine_road->security + next_effects.route_security -
                previous_effects.route_security,
            0, 100);
        mine_road->condition = ClampI32(
            mine_road->condition + next_effects.route_condition -
                previous_effects.route_condition,
            0, 100);
        if (command->dungeon_state == CC_DUNGEON_PUBLIC_ROUTE) {
            mine_road->smuggler_route = false;
            mine_road->closed = false;
        } else if (command->dungeon_state == CC_DUNGEON_SMUGGLER_ROUTE) {
            mine_road->smuggler_route = true;
            mine_road->closed = false;
        } else if (command->dungeon_state == CC_DUNGEON_RESEALED) {
            mine_road->smuggler_route = false;
            mine_road->closed = true;
        }
        CcBanditGroup *bandits = BanditsOnRoute(sim, mine_road->id);
        if (bandits != NULL) {
            bandits->supplies = ClampI32(
                bandits->supplies + next_effects.bandit_supplies -
                    previous_effects.bandit_supplies,
                0, 100);
            bandits->influence = ClampI32(
                bandits->influence + next_effects.bandit_influence -
                    previous_effects.bandit_influence,
                0, 100);
        }
    }
    if (command->dungeon_state == CC_DUNGEON_EXPLORED) {
        event_text = "Scouts return from the disturbed tunnel with routes, losses, and three rival proposals.";
    } else if (command->dungeon_state == CC_DUNGEON_PUBLIC_ROUTE) {
        event_text = "The old tunnel becomes a taxed public road; trade grows and officials take control.";
    } else if (command->dungeon_state == CC_DUNGEON_SMUGGLER_ROUTE) {
        event_text = "The old tunnel becomes a hidden toll road; goods move and the outlaws gain power.";
    } else {
        event_text = "The old tunnel is sealed; the monsters retreat and part of the mine economy dies with it.";
    }
    for (int32_t i = 0; i < sim->monster_count; ++i) {
        if (sim->monsters[i].dungeon_id != dungeon->id) continue;
        sim->monsters[i].hunting_pressure = ClampI32(
            sim->monsters[i].hunting_pressure +
                next_effects.hunting_pressure -
                previous_effects.hunting_pressure,
            0, 100);
        sim->monsters[i].population = ClampI32(
            sim->monsters[i].population - next_effects.population_loss +
                previous_effects.population_loss,
            0, 160);
        sim->monsters[i].pressure = ClampI32(
            sim->monsters[i].pressure -
                next_effects.monster_pressure_loss +
                previous_effects.monster_pressure_loss,
            0, 100);
    }
    (void)PushEvent(sim, CC_EVENT_DUNGEON_CHANGED, dungeon->id,
                    dungeon->settlement_id, 0, dungeon->regional_pressure,
                    event_text);
    if (command->dungeon_state != CC_DUNGEON_EXPLORED) {
        ResolveTargetSituations(
            sim, CC_SITUATION_MONSTER_EXPEDITION, dungeon->id);
    }
    SetError(error, error_capacity, "");
    return true;
}

static bool HorsePresentAtStable(const CcSim *sim, const CcHorse *horse,
                                 CcId settlement_id)
{
    if (sim == NULL || horse == NULL || sim->journey.active) return false;
    for (int32_t i = 0; i < CC_CARRIAGE_HORSE_COUNT; ++i) {
        if (sim->horse_team[i].id == horse->id) {
            return sim->player.location_id == settlement_id;
        }
    }
    return horse->stable_settlement_id == settlement_id;
}

static int32_t ReservedStableHorseSlots(const CcSim *sim)
{
    int32_t reserved = sim->stable_horse_count;
    for (int32_t i = 0; i < CcSimHorseCount(sim); ++i) {
        const CcHorse *horse = CcSimHorseAt(sim, i);
        if (horse != NULL && horse->pregnant_by_id != 0U) reserved += 1;
    }
    return reserved;
}

static bool ApplyBreedHorses(CcSim *sim, const CcCommand *command,
                             char *error, size_t error_capacity)
{
    CcSettlement *place = CcSimSettlementMutable(
        sim, sim->player.location_id);
    if (place == NULL ||
        !CcSettlementHasService(place, CC_SERVICE_STABLE)) {
        SetError(error, error_capacity,
                 "Horse breeding requires a settlement stable.");
        return false;
    }
    CcHorse *mare = HorseMutable(sim, command->target_id);
    const CcHorse *chosen_stallion = CcSimHorseAt(
        sim, command->amount - 1);
    CcHorse *stallion = chosen_stallion != NULL ?
        HorseMutable(sim, chosen_stallion->id) : NULL;
    if (mare == NULL || stallion == NULL || mare->id == stallion->id ||
        mare->sex != CC_HORSE_MARE ||
        stallion->sex != CC_HORSE_STALLION) {
        SetError(error, error_capacity,
                 "Choose one mare and one stallion from the horse list.");
        return false;
    }
    if (!HorsePresentAtStable(sim, mare, place->id) ||
        !HorsePresentAtStable(sim, stallion, place->id)) {
        SetError(error, error_capacity,
                 "Both horses must be present at this stable.");
        return false;
    }
    if (!CcHorseWorkingReady(mare) || !CcHorseWorkingReady(stallion) ||
        mare->health < 70 || stallion->health < 70 ||
        mare->fatigue > 45 || stallion->fatigue > 45 ||
        mare->hunger > 45 || stallion->hunger > 45) {
        SetError(error, error_capacity,
                 "Both horses must be mature, trained, healthy, fed, and rested.");
        return false;
    }
    if (mare->pregnant_by_id != 0U ||
        mare->breeding_cooldown_days > 0 ||
        stallion->breeding_cooldown_days > 0) {
        SetError(error, error_capacity,
                 "One of those horses is not ready to breed again.");
        return false;
    }
    if (ReservedStableHorseSlots(sim) >= CC_MAX_STABLE_HORSES) {
        SetError(error, error_capacity,
                 "The company has no stable space reserved for another foal.");
        return false;
    }
    const int32_t breeding_cost = 20;
    const int32_t breeding_fodder = 2;
    if (sim->player.coins < breeding_cost ||
        place->stock[CC_GOOD_FOOD] < breeding_fodder) {
        SetError(error, error_capacity,
                 "Breeding costs 20 crowns and 2 Food at the stable.");
        return false;
    }
    sim->player.coins -= breeding_cost;
    place->market_coins += breeding_cost;
    place->stock[CC_GOOD_FOOD] -= breeding_fodder;
    mare->pregnant_by_id = stallion->id;
    mare->pregnancy_days_remaining = 330;
    stallion->breeding_cooldown_days = 30;
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(
        text, sizeof(text),
        "%.18s stable breeds %.18s with %.18s; foal due in 330 days.",
        place->name, mare->name, stallion->name);
    (void)PushEvent(sim, CC_EVENT_HORSE_BRED, mare->id, place->id,
                    0U, 330, text);
    SetError(error, error_capacity, "");
    return true;
}

static bool ApplyAssignHorse(CcSim *sim, const CcCommand *command,
                             char *error, size_t error_capacity)
{
    CcSettlement *place = CcSimSettlementMutable(
        sim, sim->player.location_id);
    if (place == NULL ||
        !CcSettlementHasService(place, CC_SERVICE_STABLE)) {
        SetError(error, error_capacity,
                 "The carriage team can only change at a stable.");
        return false;
    }
    int32_t team_slot = command->amount - 1;
    if (team_slot < 0 || team_slot >= CC_CARRIAGE_HORSE_COUNT) {
        SetError(error, error_capacity, "Choose carriage team slot 1 or 2.");
        return false;
    }
    int32_t stable_slot = -1;
    for (int32_t i = 0; i < sim->stable_horse_count; ++i) {
        if (sim->stable_horses[i].id == command->target_id) {
            stable_slot = i;
            break;
        }
    }
    if (stable_slot < 0 ||
        sim->stable_horses[stable_slot].stable_settlement_id != place->id) {
        SetError(error, error_capacity,
                 "That horse is not boarded at this stable.");
        return false;
    }
    CcHorse incoming = sim->stable_horses[stable_slot];
    if (!CcHorseWorkingReady(&incoming) || incoming.fatigue > 70 ||
        incoming.hunger > 60 ||
        (incoming.pregnancy_days_remaining > 0 &&
         incoming.pregnancy_days_remaining <= 90)) {
        SetError(error, error_capacity,
                 "That horse is not ready for carriage work.");
        return false;
    }
    CcHorse outgoing = sim->horse_team[team_slot];
    incoming.stable_settlement_id = 0U;
    outgoing.stable_settlement_id = place->id;
    sim->horse_team[team_slot] = incoming;
    sim->stable_horses[stable_slot] = outgoing;
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(
        text, sizeof(text),
        "%.20s joins the carriage team; %.20s is boarded at %.20s.",
        incoming.name, outgoing.name, place->name);
    (void)PushEvent(sim, CC_EVENT_HORSE_TEAM_CHANGED, incoming.id,
                    place->id, 0U, team_slot + 1, text);
    SetError(error, error_capacity, "");
    return true;
}

bool CcSimApply(CcSim *sim, const CcCommand *command,
                char *error, size_t error_capacity)
{
    if (sim == NULL || command == NULL) {
        SetError(error, error_capacity, "Command target is missing.");
        return false;
    }
    bool settlement_action = command->kind == CC_COMMAND_TRADE ||
        command->kind == CC_COMMAND_REPAIR_ROUTE ||
        command->kind == CC_COMMAND_CHANGE_DUNGEON ||
        command->kind == CC_COMMAND_BUY_MAP ||
        command->kind == CC_COMMAND_SELL_MAP ||
        command->kind == CC_COMMAND_BUY_TREASURE ||
        command->kind == CC_COMMAND_SELL_TREASURE ||
        command->kind == CC_COMMAND_ACCEPT_SITUATION ||
        command->kind == CC_COMMAND_ABANDON_SITUATION ||
        command->kind == CC_COMMAND_REFUSE_SITUATION ||
        command->kind == CC_COMMAND_BREED_HORSES ||
        command->kind == CC_COMMAND_ASSIGN_HORSE ||
        command->kind == CC_COMMAND_GOBLIN_TRADE ||
        command->kind == CC_COMMAND_GOBLIN_WARN ||
        command->kind == CC_COMMAND_GOBLIN_INTERCEPT ||
        command->kind == CC_COMMAND_CHARACTER_RESPONSE;
    if (sim->journey.active && settlement_action) {
        SetError(error, error_capacity,
                 "Settlement business must wait until the carriage arrives.");
        return false;
    }
    switch (command->kind) {
        case CC_COMMAND_TRADE:
            return ApplyTrade(sim, command, error, error_capacity);
        case CC_COMMAND_TRAVEL:
            return ApplyTravel(sim, command, error, error_capacity);
        case CC_COMMAND_REPAIR_ROUTE:
            return ApplyRepair(sim, command, error, error_capacity);
        case CC_COMMAND_CHANGE_DUNGEON:
            return ApplyDungeonChange(sim, command, error, error_capacity);
        case CC_COMMAND_BUY_MAP:
            return ApplyBuyMap(sim, command, error, error_capacity);
        case CC_COMMAND_SELL_MAP:
            return ApplySellMap(sim, command, error, error_capacity);
        case CC_COMMAND_BUY_TREASURE:
            return ApplyBuyTreasure(sim, command, error, error_capacity);
        case CC_COMMAND_SELL_TREASURE:
            return ApplySellTreasure(sim, command, error, error_capacity);
        case CC_COMMAND_ARCHIVE_MAP:
            return ApplyArchiveMap(sim, command, error, error_capacity);
        case CC_COMMAND_RETRIEVE_MAP:
            return ApplyRetrieveMap(sim, command, error, error_capacity);
        case CC_COMMAND_STEAL_DRAGON_NAMED_TREASURE:
            return ApplyStealDragonNamedTreasure(
                sim, command, error, error_capacity);
        case CC_COMMAND_RETURN_DRAGON_NAMED_TREASURE:
            return ApplyReturnDragonNamedTreasure(
                sim, command, error, error_capacity);
        case CC_COMMAND_GOBLIN_TRADE:
            return ApplyGoblinTrade(sim, command, error, error_capacity);
        case CC_COMMAND_GOBLIN_WARN:
            return ApplyGoblinWarning(sim, error, error_capacity);
        case CC_COMMAND_GOBLIN_INTERCEPT:
            return ApplyGoblinIntercept(sim, error, error_capacity);
        case CC_COMMAND_CHARACTER_RESPONSE:
            return ApplyCharacterResponse(
                sim, command, error, error_capacity);
        case CC_COMMAND_SET_JOURNEY_PACE:
            return ApplyJourneyPace(sim, command, error, error_capacity);
        case CC_COMMAND_ACCEPT_SITUATION:
            return ApplyAcceptSituation(sim, command, error, error_capacity);
        case CC_COMMAND_ABANDON_SITUATION:
            return ApplyAbandonSituation(sim, command, error, error_capacity);
        case CC_COMMAND_REFUSE_SITUATION:
            return ApplyRefuseSituation(sim, command, error, error_capacity);
        case CC_COMMAND_RESOLVE_ENCOUNTER_COMBAT:
            return ApplyResolveEncounter(sim, CC_JOURNEY_OUTCOME_COMBAT,
                                         false,
                                         error, error_capacity);
        case CC_COMMAND_RESOLVE_ENCOUNTER_NEGOTIATE:
            return ApplyResolveEncounter(sim, CC_JOURNEY_OUTCOME_NEGOTIATED,
                                         false,
                                         error, error_capacity);
        case CC_COMMAND_RESOLVE_ENCOUNTER_PROVISIONS:
            return ApplyResolveEncounter(sim, CC_JOURNEY_OUTCOME_NEGOTIATED,
                                         true,
                                         error, error_capacity);
        case CC_COMMAND_WITHDRAW_ENCOUNTER:
            return ApplyWithdrawEncounter(sim, command,
                                          error, error_capacity);
        case CC_COMMAND_BREED_HORSES:
            return ApplyBreedHorses(sim, command, error, error_capacity);
        case CC_COMMAND_ASSIGN_HORSE:
            return ApplyAssignHorse(sim, command, error, error_capacity);
        case CC_COMMAND_INTERCEPT_DRAGON_TRIBUTE:
            return ApplyInterceptDragonTribute(
                sim, error, error_capacity);
        case CC_COMMAND_STEAL_DRAGON_HOARD: {
            if (sim->dragon.slain) {
                SetError(error, error_capacity,
                         "The slain dragon no longer guards a hoard.");
                return false;
            }
            if (sim->journey.active ||
                sim->player.location_id != sim->dragon.lair_settlement_id) {
                SetError(error, error_capacity,
                         "The dragon hoard can only be reached from its cave.");
                return false;
            }
            if (command->amount <= 0 ||
                (CcMoney)command->amount > sim->dragon.hoard) {
                SetError(error, error_capacity,
                         "Choose a positive amount that is present in the hoard.");
                return false;
            }
            if (sim->dragon.stolen_outstanding > 0) {
                SetError(error, error_capacity,
                         "The dragon is already hunting for stolen treasure.");
                return false;
            }
            CcSettlement *target = CcSimSettlementMutable(
                sim, sim->goblins.last_tribute_origin_id);
            if (target == NULL || target->id == sim->dragon.lair_settlement_id) {
                target = RichestDragonTarget(sim);
            }
            if (target == NULL) {
                SetError(error, error_capacity,
                         "The dragon has no realm to threaten.");
                return false;
            }
            char text[CC_EVENT_TEXT_CAPACITY];
            (void)snprintf(text, sizeof(text),
                           "The carriage steals %d crowns from %s's hoard.",
                           command->amount, sim->dragon.name);
            if (StartDragonTheft(
                    sim, sim->player.id, target, command->amount,
                    sim->dragon.hoard_event_id, text) == NULL) {
                SetError(error, error_capacity,
                         "The hoard theft could not be recorded.");
                return false;
            }
            sim->player.coins += command->amount;
            sim->player.reputation = ClampI32(
                sim->player.reputation - 5, -100, 100);
            SetError(error, error_capacity, "");
            return true;
        }
        case CC_COMMAND_RETURN_DRAGON_TREASURE: {
            if (sim->dragon.slain) {
                SetError(error, error_capacity,
                         "No dragon remains to receive restitution.");
                return false;
            }
            if (sim->journey.active ||
                sim->player.location_id != sim->dragon.lair_settlement_id) {
                SetError(error, error_capacity,
                         "Stolen treasure must be carried back to the dragon cave.");
                return false;
            }
            if (sim->dragon.stolen_treasure_id != 0U) {
                SetError(error, error_capacity,
                         "The dragon remembers the exact named object; replacement coins do not settle this wound.");
                return false;
            }
            if (command->amount <= 0 ||
                (CcMoney)command->amount > sim->dragon.stolen_outstanding ||
                (CcMoney)command->amount > sim->player.coins) {
                SetError(error, error_capacity,
                         "Return a positive amount you carry and still owe.");
                return false;
            }
            CcMoney debt_before = sim->dragon.stolen_outstanding;
            sim->player.coins -= command->amount;
            sim->dragon.hoard += command->amount;
            sim->dragon.stolen_outstanding -= command->amount;
            int32_t healing = 1 + (int32_t)(
                ((CcMoney)command->amount * 40) / debt_before);
            sim->dragon.memory_integrity = ClampI32(
                sim->dragon.memory_integrity + healing, 0, 100);
            char text[CC_EVENT_TEXT_CAPACITY];
            (void)snprintf(text, sizeof(text),
                           "The carriage returns %d crowns to %s; %" PRId64 " remain missing.",
                           command->amount, sim->dragon.name,
                           sim->dragon.stolen_outstanding);
            CcEvent *returned = PushEvent(
                sim, CC_EVENT_DRAGON_TREASURE_RETURNED, sim->player.id,
                sim->dragon.lair_settlement_id,
                sim->dragon.omen_event_id, command->amount, text);
            sim->dragon.hoard_event_id = returned != NULL ? returned->id :
                                         sim->dragon.hoard_event_id;
            if (sim->dragon.stolen_outstanding == 0) {
                sim->dragon.memory_integrity = MaximumI32(
                    sim->dragon.memory_integrity, 75);
                sim->player.reputation = ClampI32(
                    sim->player.reputation + 2, -100, 100);
                sim->dragon.theft_actor_id = 0U;
                sim->dragon.retaliation_target_id = 0U;
                sim->dragon.omen_event_id = 0U;
                sim->dragon.omen_days_remaining = 0;
            }
            SetError(error, error_capacity, "");
            return true;
        }
        case CC_COMMAND_NONE:
            break;
    }
    SetError(error, error_capacity, "Command kind is invalid.");
    return false;
}

#define CC_MAX_TRACKED_IDENTITIES \
    (CC_MAX_KINGDOMS + CC_MAX_SETTLEMENTS + CC_MAX_ROUTES + CC_MAX_MAPS + \
     CC_MAX_TREASURES + CC_MAX_FACTIONS + CC_MAX_SHIPMENTS + \
     CC_MAX_COURIERS + CC_MAX_BANDITS + CC_MAX_MONSTERS + CC_MAX_DUNGEONS + \
     CC_MAX_SITUATIONS + CC_MAX_CHARACTERS + CC_MAX_EVENTS + \
     CC_CARRIAGE_HORSE_COUNT + \
     CC_MAX_STABLE_HORSES + 4)

typedef struct CcIdentityLedger {
    CcId ids[CC_MAX_TRACKED_IDENTITIES];
    int32_t count;
    uint64_t greatest_serial;
} CcIdentityLedger;

static bool TrackIdentity(CcIdentityLedger *ledger, CcId id,
                          CcEntityKind expected_kind,
                          char *error, size_t error_capacity)
{
    uint64_t serial = id & CC_ID_SERIAL_MASK;
    if (CcIdKind(id) != expected_kind || serial == 0U ||
        ledger->count >= CC_MAX_TRACKED_IDENTITIES) {
        SetError(error, error_capacity, "Simulation identity is invalid.");
        return false;
    }
    for (int32_t i = 0; i < ledger->count; ++i) {
        if (ledger->ids[i] == id) {
            SetError(error, error_capacity,
                     "Simulation identities are not unique.");
            return false;
        }
    }
    ledger->ids[ledger->count++] = id;
    if (serial > ledger->greatest_serial) ledger->greatest_serial = serial;
    return true;
}

static bool ValidateIdentityState(const CcSim *sim,
                                  char *error, size_t error_capacity)
{
    CcIdentityLedger ledger = {0};
#define TRACK_ID(value, kind) \
    do { \
        if (!TrackIdentity(&ledger, (value), (kind), \
                           error, error_capacity)) return false; \
    } while (0)
    for (int32_t i = 0; i < sim->kingdom_count; ++i)
        TRACK_ID(sim->kingdoms[i].id, CC_ENTITY_KINGDOM);
    for (int32_t i = 0; i < sim->settlement_count; ++i)
        TRACK_ID(sim->settlements[i].id, CC_ENTITY_SETTLEMENT);
    for (int32_t i = 0; i < sim->route_count; ++i)
        TRACK_ID(sim->routes[i].id, CC_ENTITY_ROUTE);
    for (int32_t i = 0; i < sim->map_count; ++i)
        TRACK_ID(sim->maps[i].id, CC_ENTITY_MAP);
    if (sim->schema_version >= 9U) {
        for (int32_t i = 0; i < sim->treasure_count; ++i)
            TRACK_ID(sim->treasures[i].id, CC_ENTITY_TREASURE);
    }
    for (int32_t i = 0; i < sim->faction_count; ++i)
        TRACK_ID(sim->factions[i].id, CC_ENTITY_FACTION);
    for (int32_t i = 0; i < sim->shipment_count; ++i)
        TRACK_ID(sim->shipments[i].id, CC_ENTITY_SHIPMENT);
    if (sim->schema_version >= 11U) {
        for (int32_t i = 0; i < sim->courier_count; ++i)
            TRACK_ID(sim->couriers[i].id, CC_ENTITY_COURIER);
    }
    for (int32_t i = 0; i < sim->bandit_count; ++i)
        TRACK_ID(sim->bandits[i].id, CC_ENTITY_BANDIT_GROUP);
    for (int32_t i = 0; i < sim->monster_count; ++i)
        TRACK_ID(sim->monsters[i].id, CC_ENTITY_MONSTER_POPULATION);
    for (int32_t i = 0; i < sim->dungeon_count; ++i)
        TRACK_ID(sim->dungeons[i].id, CC_ENTITY_DUNGEON);
    for (int32_t i = 0; i < sim->situation_count; ++i)
        TRACK_ID(sim->situations[i].id, CC_ENTITY_SITUATION);
    if (sim->schema_version >= 17U) {
        for (int32_t i = 0; i < sim->character_count; ++i)
            TRACK_ID(sim->characters[i].id, CC_ENTITY_CHARACTER);
    }
    for (int32_t i = 0; i < sim->event_count; ++i) {
        const CcEvent *event = CcSimRecentEvent(sim, i);
        if (event == NULL) {
            SetError(error, error_capacity, "Simulation event identity is invalid.");
            return false;
        }
        TRACK_ID(event->id, CC_ENTITY_EVENT);
    }
    TRACK_ID(sim->player.id, CC_ENTITY_PLAYER_COMPANY);
    if (sim->schema_version >= 6U) {
        TRACK_ID(sim->goblins.id, CC_ENTITY_GOBLIN_CULT);
        TRACK_ID(sim->dragon.id, CC_ENTITY_DRAGON);
    }
    if (sim->schema_version >= 7U)
        TRACK_ID(sim->hoard_raiders.id, CC_ENTITY_HOARD_RAIDERS);
    if (sim->schema_version >= 14U) {
        for (int32_t i = 0; i < CC_CARRIAGE_HORSE_COUNT; ++i)
            TRACK_ID(sim->horse_team[i].id, CC_ENTITY_HORSE);
    }
    if (sim->schema_version >= 15U) {
        for (int32_t i = 0; i < sim->stable_horse_count; ++i)
            TRACK_ID(sim->stable_horses[i].id, CC_ENTITY_HORSE);
    }
#undef TRACK_ID
    if (sim->next_entity_serial <= ledger.greatest_serial ||
        sim->next_entity_serial > CC_ID_SERIAL_MASK) {
        SetError(error, error_capacity,
                 "Simulation identity counter is behind saved entities.");
        return false;
    }
    return true;
}

bool CcSimValidate(const CcSim *sim, char *error, size_t error_capacity)
{
    if (sim == NULL) {
        SetError(error, error_capacity, "Simulation is missing.");
        return false;
    }
    bool legacy_schema = sim->schema_version == 3U ||
                         sim->schema_version == 4U ||
                         sim->schema_version == 5U ||
                         sim->schema_version == 6U ||
                         sim->schema_version == 7U ||
                         sim->schema_version == 8U ||
                         sim->schema_version == 9U ||
                         sim->schema_version == 10U ||
                         sim->schema_version == 11U ||
                         sim->schema_version == 12U ||
                         sim->schema_version == 13U ||
                         sim->schema_version == 14U ||
                         sim->schema_version == 15U ||
                         sim->schema_version == 16U ||
                         sim->schema_version == 17U;
    bool supported_generator = sim->generator_version == CC_GENERATOR_VERSION ||
        (legacy_schema && (sim->generator_version == 3U ||
                           sim->generator_version == 5U ||
                           sim->generator_version == 6U ||
                           sim->generator_version == 7U ||
                           sim->generator_version == 8U ||
                           sim->generator_version == 9U ||
                           sim->generator_version == 10U ||
                           sim->generator_version == 11U ||
                           sim->generator_version == 12U ||
                           sim->generator_version == 13U ||
                           sim->generator_version == 14U ||
                           sim->generator_version == 15U ||
                           sim->generator_version == 16U));
    if ((!legacy_schema && sim->schema_version != CC_SIM_SCHEMA_VERSION) ||
        !supported_generator) {
        SetError(error, error_capacity, "Simulation version is unsupported.");
        return false;
    }
    if (sim->current_day < 1 || sim->current_day > CC_SIM_MAX_DAY ||
        sim->next_entity_serial == 0U ||
        sim->next_entity_serial > CC_ID_SERIAL_MASK ||
        sim->random_state == 0U || sim->clock.tick == UINT64_MAX) {
        SetError(error, error_capacity,
                 "Simulation clock or identity state is invalid.");
        return false;
    }
    if (sim->schema_version == CC_SIM_SCHEMA_VERSION &&
        (sim->iron_ledger_reserve < 0 ||
         sim->iron_ledger_reserve > CC_SIM_MAX_MONEY)) {
        SetError(error, error_capacity,
                 "The monastery reserve is invalid.");
        return false;
    }
    if (sim->kingdom_count < 1 || sim->kingdom_count > CC_MAX_KINGDOMS ||
        sim->settlement_count < 1 || sim->settlement_count > CC_MAX_SETTLEMENTS ||
        sim->route_count < 0 || sim->route_count > CC_MAX_ROUTES ||
        sim->map_count < 0 || sim->map_count > CC_MAX_MAPS ||
        (sim->schema_version == CC_SIM_SCHEMA_VERSION &&
         sim->map_count != CC_MAP_COLLECTION_COUNT) ||
        sim->treasure_count < 0 || sim->treasure_count > CC_MAX_TREASURES ||
        sim->faction_count < 0 || sim->faction_count > CC_MAX_FACTIONS ||
        sim->shipment_count < 0 || sim->shipment_count > CC_MAX_SHIPMENTS ||
        sim->courier_count < 0 || sim->courier_count > CC_MAX_COURIERS ||
        sim->bandit_count < 0 || sim->bandit_count > CC_MAX_BANDITS ||
        sim->monster_count < 0 || sim->monster_count > CC_MAX_MONSTERS ||
        sim->dungeon_count < 0 || sim->dungeon_count > CC_MAX_DUNGEONS ||
        sim->situation_count < 0 || sim->situation_count > CC_MAX_SITUATIONS ||
        sim->character_count < 0 ||
        sim->character_count > CC_MAX_CHARACTERS ||
        sim->stable_horse_count < 0 ||
        sim->stable_horse_count > CC_MAX_STABLE_HORSES ||
        sim->event_count < 0 || sim->event_count > CC_MAX_EVENTS ||
        sim->event_write_index < 0 ||
        sim->event_write_index >= CC_MAX_EVENTS) {
        SetError(error, error_capacity, "Simulation counts are invalid.");
        return false;
    }
    if (!ValidateIdentityState(sim, error, error_capacity)) return false;
    for (int32_t i = 0; i < sim->kingdom_count; ++i) {
        const CcKingdom *kingdom = &sim->kingdoms[i];
        if (CcIdKind(kingdom->id) != CC_ENTITY_KINGDOM ||
            !ValidBoundedText(kingdom->name, sizeof(kingdom->name)) ||
            kingdom->treasury < 0 ||
            kingdom->treasury > CC_SIM_MAX_MONEY ||
            kingdom->legitimacy < 0 ||
            kingdom->legitimacy > 100 ||
            (sim->schema_version == CC_SIM_SCHEMA_VERSION &&
             (kingdom->iron_ledger_debt < 0 ||
              kingdom->iron_ledger_debt > CC_SIM_MAX_MONEY))) {
            SetError(error, error_capacity, "Kingdom accounts are invalid.");
            return false;
        }
    }
    if (sim->schema_version == CC_SIM_SCHEMA_VERSION) {
        for (int32_t first = 0; first < sim->kingdom_count; ++first) {
            for (int32_t second = 0;
                 second < sim->kingdom_count; ++second) {
                CcDiplomaticState state = sim->diplomacy[first][second];
                bool diagonal = first == second;
                if (state < CC_DIPLOMACY_PEACE ||
                    state > CC_DIPLOMACY_ALLIANCE ||
                    (diagonal && state != CC_DIPLOMACY_PEACE) ||
                    state != sim->diplomacy[second][first] ||
                    sim->diplomacy_changed_day[first][second] < 1 ||
                    sim->diplomacy_changed_day[first][second] >
                        sim->current_day ||
                    sim->diplomacy_changed_day[first][second] !=
                        sim->diplomacy_changed_day[second][first]) {
                    SetError(error, error_capacity,
                             "Diplomatic state is invalid.");
                    return false;
                }
            }
        }
    }
    if (sim->schema_version == CC_SIM_SCHEMA_VERSION) {
        for (int32_t i = 0; i < sim->event_count; ++i) {
            const CcEvent *event = CcSimRecentEvent(sim, i);
            if (event == NULL || CcIdKind(event->id) != CC_ENTITY_EVENT ||
                !ValidBoundedText(event->text, sizeof(event->text)) ||
                event->day < 1 || event->day > sim->current_day ||
                event->kind < CC_EVENT_HARVEST_FAILED ||
                event->kind > CC_EVENT_ENCOUNTER_LOOT ||
                event->parent_id == event->id ||
                (event->parent_id != 0U &&
                 CcSimEvent(sim, event->parent_id) == NULL)) {
                if (error != NULL && error_capacity > 0U) {
                    (void)snprintf(
                        error, error_capacity,
                        "Causal event %" PRIu64 " kind %d lost parent %" PRIu64 ".",
                        event->id, (int32_t)event->kind, event->parent_id);
                }
                return false;
            }
        }
    }
    for (int32_t i = 0; i < sim->faction_count; ++i) {
        const CcFaction *faction = &sim->factions[i];
        if (CcIdKind(faction->id) != CC_ENTITY_FACTION ||
            KingdomSlotById(sim, faction->kingdom_id) < 0 ||
            !ValidBoundedText(faction->name, sizeof(faction->name)) ||
            faction->kind < CC_FACTION_CROWN ||
            faction->kind > CC_FACTION_COMMONS ||
            faction->power < 0 || faction->power > 100 ||
            faction->support < 0 || faction->support > 100) {
            SetError(error, error_capacity, "Faction state is invalid.");
            return false;
        }
        for (int32_t earlier = 0; earlier < i; ++earlier) {
            if (sim->factions[earlier].id == faction->id) {
                SetError(error, error_capacity,
                         "Faction identities are not unique.");
                return false;
            }
        }
    }
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        const CcSettlement *settlement = &sim->settlements[i];
        if (CcIdKind(settlement->id) != CC_ENTITY_SETTLEMENT ||
            KingdomSlotById(sim, settlement->kingdom_id) < 0 ||
            !ValidBoundedText(settlement->name,
                              sizeof(settlement->name)) ||
            settlement->function < CC_SETTLEMENT_FARMING ||
            settlement->function > CC_SETTLEMENT_DUNGEON_TOWN ||
            settlement->map_x < -CC_SIM_MAX_UNITS ||
            settlement->map_x > CC_SIM_MAX_UNITS ||
            settlement->map_y < -CC_SIM_MAX_UNITS ||
            settlement->map_y > CC_SIM_MAX_UNITS ||
            settlement->population < 0 ||
            settlement->population > CC_SIM_MAX_UNITS ||
            settlement->security < 0 || settlement->security > 100 ||
            settlement->prosperity < 0 || settlement->prosperity > 100 ||
            settlement->hunger < 0 || settlement->hunger > 100) {
            SetError(error, error_capacity, "Settlement state is invalid.");
            return false;
        }
        int32_t saved_good_count = sim->schema_version >= 9U ?
                                   CC_GOOD_COUNT : 3;
        for (int32_t good = 0; good < saved_good_count; ++good) {
            if (settlement->stock[good] < 0 ||
                settlement->stock[good] > CC_SIM_MAX_UNITS ||
                settlement->reserve_target[good] < 0 ||
                settlement->reserve_target[good] > CC_SIM_MAX_UNITS ||
                settlement->production[good] < 0 ||
                settlement->production[good] > CC_SIM_MAX_UNITS ||
                settlement->consumption[good] < 0 ||
                settlement->consumption[good] > CC_SIM_MAX_UNITS ||
                settlement->price[good] < 1 ||
                settlement->price[good] > 99) {
                SetError(error, error_capacity, "Market accounting is invalid.");
                return false;
            }
        }
        if (sim->schema_version == CC_SIM_SCHEMA_VERSION) {
            uint32_t known_services = (UINT32_C(1) << CC_SERVICE_COUNT) - 1U;
            bool valid_project = settlement->service_project == CC_SERVICE_NONE ?
                settlement->service_project_days == 0 :
                settlement->service_project >= 0 &&
                settlement->service_project < CC_SERVICE_COUNT &&
                settlement->service_project_days > 0 &&
                settlement->service_project_days <= 7 &&
                !CcSettlementHasService(settlement,
                                        settlement->service_project);
            bool valid_ruin = !CcSettlementIsAbandoned(settlement) ||
                (settlement->service_mask == 0U &&
                 settlement->service_project == CC_SERVICE_NONE &&
                 settlement->service_project_days == 0 &&
                 settlement->security == 0 &&
                 settlement->prosperity == 0);
            if (settlement->population < 0 || !valid_ruin ||
                settlement->size < CC_SETTLEMENT_HAMLET ||
                settlement->size > CC_SETTLEMENT_CAPITAL_SIZE ||
                settlement->market_coins < 0 ||
                settlement->market_coins > CC_SIM_MAX_MONEY ||
                settlement->war_chest < 0 ||
                settlement->war_chest > CC_SIM_MAX_MONEY ||
                settlement->field_yield < 0 ||
                settlement->field_yield > 100 ||
                settlement->iron_deposit < 0 ||
                settlement->iron_deposit > CC_SIM_MAX_UNITS ||
                settlement->gold_progress < 0 ||
                settlement->gold_progress >= 12 ||
                settlement->gem_progress < 0 ||
                settlement->gem_progress >= 48 ||
                settlement->farm_tool_wear < 0 ||
                settlement->farm_tool_wear >= 4 ||
                settlement->mine_tool_wear < 0 ||
                settlement->mine_tool_wear >= 3 ||
                settlement->smith_tool_wear < 0 ||
                settlement->smith_tool_wear >= 4 ||
                settlement->treasure_gold_committed < 0 ||
                settlement->treasure_gold_committed > CC_SIM_MAX_UNITS ||
                settlement->treasure_gems_committed < 0 ||
                settlement->treasure_gems_committed > CC_SIM_MAX_UNITS ||
                settlement->treasure_work < 0 ||
                settlement->treasure_work > 3 ||
                settlement->cow_adults < 0 ||
                settlement->cow_adults > CC_SIM_MAX_UNITS ||
                settlement->cow_calves < 0 ||
                settlement->cow_calves > CC_SIM_MAX_UNITS ||
                settlement->cow_condition < 0 ||
                settlement->cow_condition > 100 ||
                settlement->cow_hunger < 0 ||
                settlement->cow_hunger > 100 ||
                (settlement->service_mask & ~known_services) != 0U ||
                CcSettlementServiceCount(settlement) >
                    CcSettlementServiceCapacity(settlement->size) ||
                !valid_project) {
                SetError(error, error_capacity,
                         "Settlement service state is invalid.");
                return false;
            }
        }
    }
    for (int32_t i = 0; i < sim->route_count; ++i) {
        const CcRoute *route = &sim->routes[i];
        if (CcIdKind(route->id) != CC_ENTITY_ROUTE ||
            CcSimSettlement(sim, route->from_id) == NULL ||
            CcSimSettlement(sim, route->to_id) == NULL ||
            route->from_id == route->to_id ||
            route->travel_days < 1 ||
            route->travel_days > CC_SIM_MAX_ROUTE_DAYS ||
            route->capacity < 1 || route->capacity > CC_SIM_MAX_UNITS ||
            route->security < 0 || route->security > 100 ||
            route->condition < 0 || route->condition > 100) {
            SetError(error, error_capacity, "Route data is invalid.");
            return false;
        }
    }
    for (int32_t i = 0; i < sim->map_count; ++i) {
        const CcMap *map = &sim->maps[i];
        bool valid_owner = map->owner_id == sim->player.id ||
                           CcSimSettlement(sim, map->owner_id) != NULL;
        if (CcIdKind(map->id) != CC_ENTITY_MAP ||
            CcSimRoute(sim, map->route_id) == NULL ||
            CcSimSettlement(sim, map->maker_settlement_id) == NULL ||
            !valid_owner || map->accuracy < 0 || map->accuracy > 100 ||
            map->recorded_condition < 0 || map->recorded_condition > 100 ||
            map->recorded_danger < 0 || map->recorded_danger > 100 ||
            !ValidBoundedText(map->name, sizeof(map->name)) ||
            map->surveyed_day < -CC_SIM_MAX_DAY ||
            map->surveyed_day > sim->current_day ||
            map->ask_price < 1 || map->ask_price > CC_SIM_MAX_UNITS) {
            SetError(error, error_capacity, "Physical map data is invalid.");
            return false;
        }
    }
    if (sim->schema_version == CC_SIM_SCHEMA_VERSION) {
        uint32_t known_maps =
            (UINT32_C(1) << CC_MAP_COLLECTION_COUNT) - 1U;
        if ((sim->player.map_catalogue_mask & ~known_maps) != 0U ||
            (sim->player.map_archive_mask &
             ~sim->player.map_catalogue_mask) != 0U) {
            SetError(error, error_capacity,
                     "Map collection state is invalid.");
            return false;
        }
        for (int32_t i = 0; i < sim->map_count; ++i) {
            if ((sim->player.map_archive_mask & MapSlotBit(i)) != 0U &&
                sim->maps[i].owner_id != sim->player.id) {
                SetError(error, error_capacity,
                         "An archived map has the wrong owner.");
                return false;
            }
        }
    }
    int32_t player_treasure_count = 0;
    for (int32_t i = 0; i < sim->treasure_count; ++i) {
        const CcTreasure *treasure = &sim->treasures[i];
        bool valid_owner = CcSimSettlement(sim, treasure->owner_id) != NULL ||
            treasure->owner_id == sim->goblins.id ||
            treasure->owner_id == sim->dragon.id ||
            treasure->owner_id == sim->player.id ||
            treasure->owner_id == sim->hoard_raiders.id;
        if (CcIdKind(treasure->id) != CC_ENTITY_TREASURE ||
            !ValidBoundedText(treasure->name, sizeof(treasure->name)) ||
            CcSimSettlement(sim, treasure->maker_settlement_id) == NULL ||
            CcSimSettlement(sim, treasure->location_id) == NULL ||
            !valid_owner || treasure->gold_content < 1 ||
            treasure->gold_content > CC_SIM_MAX_UNITS ||
            treasure->gem_content < 1 ||
            treasure->gem_content > CC_SIM_MAX_UNITS ||
            treasure->craft_work < 1 ||
            treasure->craft_work > CC_SIM_MAX_UNITS ||
            treasure->appraised_value < 1 ||
            treasure->appraised_value > CC_SIM_MAX_UNITS ||
            treasure->created_day < 1 ||
            treasure->created_day > sim->current_day) {
            SetError(error, error_capacity, "Treasure state is invalid.");
            return false;
        }
        if (!treasure->destroyed &&
            treasure->owner_id == sim->player.id) {
            if (treasure->location_id != sim->player.location_id) {
                SetError(error, error_capacity,
                         "Carried treasure location is invalid.");
                return false;
            }
            player_treasure_count += 1;
        }
    }
    for (int32_t i = 0; i < sim->shipment_count; ++i) {
        const CcShipment *shipment = &sim->shipments[i];
        const CcRoute *shipment_route = CcSimRoute(sim, shipment->route_id);
        bool route_connects = shipment_route != NULL &&
            ((shipment_route->from_id == shipment->origin_id &&
              shipment_route->to_id == shipment->destination_id) ||
             (shipment_route->to_id == shipment->origin_id &&
              shipment_route->from_id == shipment->destination_id));
        bool timing_valid = shipment->status != CC_SHIPMENT_TRAVELLING ||
            (shipment->departure_day >= 1 &&
             shipment->departure_day <= sim->current_day &&
             shipment->arrival_day > sim->current_day &&
             shipment_route != NULL &&
             (int64_t)shipment->arrival_day ==
                 (int64_t)shipment->departure_day +
                     (int64_t)shipment_route->travel_days);
        bool capacity_valid = shipment_route != NULL &&
            shipment->good >= 0 && shipment->good < CC_GOOD_COUNT &&
            shipment->quantity >= 1 &&
            shipment->quantity <= CC_SIM_MAX_UNITS &&
            GoodCargoSlots(shipment->good, shipment->quantity) <=
                shipment_route->capacity;
        if (CcIdKind(shipment->id) != CC_ENTITY_SHIPMENT ||
            CcSimSettlement(sim, shipment->origin_id) == NULL ||
            CcSimSettlement(sim, shipment->destination_id) == NULL ||
            CcSimSettlement(sim, shipment->final_destination_id) == NULL ||
            shipment_route == NULL || !route_connects || !timing_valid ||
            shipment->good < 0 || shipment->good >= CC_GOOD_COUNT ||
            shipment->quantity < 1 ||
            shipment->quantity > CC_SIM_MAX_UNITS ||
            shipment->status < CC_SHIPMENT_UNUSED ||
            shipment->status > CC_SHIPMENT_LOST || !capacity_valid) {
            SetError(error, error_capacity, "Shipment data is invalid.");
            return false;
        }
    }
    if (sim->schema_version == CC_SIM_SCHEMA_VERSION) {
        for (int32_t i = 0; i < sim->courier_count; ++i) {
            const CcCourier *courier = &sim->couriers[i];
            bool active = courier->status == CC_COURIER_WAITING ||
                          courier->status == CC_COURIER_TRAVELLING ||
                          courier->status == CC_COURIER_WITH_PLAYER;
            bool route_valid =
                courier->status != CC_COURIER_TRAVELLING ||
                (CcSimRoute(sim, courier->route_id) != NULL &&
                 courier->arrival_day > sim->current_day);
            bool situation_valid = !active ||
                (CcSimSituation(sim, courier->situation_id) != NULL &&
                 CcSimSituation(sim, courier->situation_id)->target_id ==
                    courier->id);
            if (CcIdKind(courier->id) != CC_ENTITY_COURIER ||
                courier->kind < CC_COURIER_WAR_DECLARATION ||
                courier->kind > CC_COURIER_DRAGON_MUSTER ||
                courier->status < CC_COURIER_WAITING ||
                courier->status > CC_COURIER_DISTORTED) {
                SetError(error, error_capacity,
                         "Courier identity or enum is invalid.");
                return false;
            }
            if (KingdomSlotById(sim, courier->issuer_kingdom_id) < 0 ||
                KingdomSlotById(sim, courier->recipient_kingdom_id) < 0 ||
                courier->issuer_kingdom_id ==
                    courier->recipient_kingdom_id) {
                SetError(error, error_capacity,
                         "Courier kingdom endpoints are invalid.");
                return false;
            }
            if (CcSimSettlement(sim, courier->origin_settlement_id) == NULL ||
                CcSimSettlement(
                    sim, courier->destination_settlement_id) == NULL ||
                CcSimSettlement(sim, courier->current_settlement_id) == NULL) {
                SetError(error, error_capacity,
                         "Courier settlement endpoint is invalid.");
                return false;
            }
            if (courier->reliability < 0 || courier->reliability > 100 ||
                courier->departure_day < 1) {
                SetError(error, error_capacity,
                         "Courier timing or reliability is invalid.");
                return false;
            }
            if (!route_valid) {
                SetError(error, error_capacity,
                         "Courier route timing is invalid.");
                return false;
            }
            if (!situation_valid) {
                SetError(error, error_capacity,
                         "Active courier has no matching situation.");
                return false;
            }
            if (courier->cause_event_id != 0U &&
                CcSimEvent(sim, courier->cause_event_id) == NULL) {
                SetError(error, error_capacity,
                         "Courier cause is missing from history.");
                return false;
            }
        }
    }
    for (int32_t i = 0; i < sim->bandit_count; ++i) {
        const CcBanditGroup *bandits = &sim->bandits[i];
        if (CcIdKind(bandits->id) != CC_ENTITY_BANDIT_GROUP ||
            CcSimRoute(sim, bandits->route_id) == NULL ||
            !ValidBoundedText(bandits->name, sizeof(bandits->name)) ||
            bandits->members < 0 || bandits->members > 120 ||
            bandits->supplies < 0 || bandits->supplies > 100 ||
            bandits->influence < 0 || bandits->influence > 100) {
            SetError(error, error_capacity, "Bandit camp state is invalid.");
            return false;
        }
        if (sim->schema_version == CC_SIM_SCHEMA_VERSION) {
            uint32_t known_services = (UINT32_C(1) << CC_SERVICE_COUNT) - 1U;
            bool idle = bandits->raid_phase == CC_BANDIT_RAID_IDLE;
            bool valid_raid = idle ?
                bandits->raid_target_id == 0U &&
                bandits->raid_quantity == 0 &&
                bandits->raid_days_remaining == 0 :
                CcSimSettlement(sim, bandits->raid_target_id) != NULL &&
                bandits->raid_good >= 0 && bandits->raid_good < CC_GOOD_COUNT &&
                bandits->raid_quantity >= 0 &&
                bandits->raid_quantity <= CC_SIM_MAX_UNITS &&
                bandits->raid_days_remaining > 0 &&
                bandits->raid_days_remaining <= CC_SIM_MAX_DAY;
            if (bandits->camp_size < CC_BANDIT_HIDEOUT ||
                bandits->camp_size > CC_BANDIT_OUTLAW_TOWN ||
                (bandits->service_mask & ~known_services) != 0U ||
                ServiceMaskCount(bandits->service_mask) >
                    CcBanditCampServiceCapacity(bandits->camp_size) ||
                bandits->raid_phase < CC_BANDIT_RAID_IDLE ||
                bandits->raid_phase > CC_BANDIT_RAID_RETURNING ||
                bandits->raids_completed < 0 ||
                bandits->raids_completed > CC_SIM_MAX_UNITS || !valid_raid) {
                SetError(error, error_capacity, "Bandit expedition state is invalid.");
                return false;
            }
        }
    }
    for (int32_t i = 0; i < sim->dungeon_count; ++i) {
        const CcDungeon *dungeon = &sim->dungeons[i];
        if (CcIdKind(dungeon->id) != CC_ENTITY_DUNGEON ||
            CcSimSettlement(sim, dungeon->settlement_id) == NULL ||
            !ValidBoundedText(dungeon->name, sizeof(dungeon->name)) ||
            dungeon->state < CC_DUNGEON_SEALED ||
            dungeon->state > CC_DUNGEON_RESEALED ||
            dungeon->depth < 1 || dungeon->depth > 1000 ||
            dungeon->regional_pressure < 0 ||
            dungeon->regional_pressure > 100) {
            SetError(error, error_capacity, "Dungeon state is invalid.");
            return false;
        }
        for (int32_t earlier = 0; earlier < i; ++earlier) {
            if (sim->dungeons[earlier].id == dungeon->id) {
                SetError(error, error_capacity,
                         "Dungeon identities are not unique.");
                return false;
            }
        }
    }
    for (int32_t i = 0; i < sim->monster_count; ++i) {
        const CcMonsterPopulation *monster = &sim->monsters[i];
        bool dungeon_exists = false;
        for (int32_t dungeon = 0; dungeon < sim->dungeon_count; ++dungeon) {
            if (sim->dungeons[dungeon].id == monster->dungeon_id) {
                dungeon_exists = true;
                break;
            }
        }
        if (CcIdKind(monster->id) != CC_ENTITY_MONSTER_POPULATION ||
            !dungeon_exists ||
            !ValidBoundedText(monster->name, sizeof(monster->name)) ||
            monster->population < 0 || monster->population > 160 ||
            monster->pressure < 0 || monster->pressure > 100 ||
            monster->hunting_pressure < 0 ||
            monster->hunting_pressure > 100) {
            SetError(error, error_capacity,
                     "Monster population state is invalid.");
            return false;
        }
        for (int32_t earlier = 0; earlier < i; ++earlier) {
            if (sim->monsters[earlier].id == monster->id) {
                SetError(error, error_capacity,
                         "Monster identities are not unique.");
                return false;
            }
        }
    }
    if (sim->schema_version == CC_SIM_SCHEMA_VERSION) {
        const CcGoblinCult *goblins = &sim->goblins;
        const CcDragon *dragon = &sim->dragon;
        bool goblins_idle = goblins->tribute_phase ==
                            CC_GOBLIN_TRIBUTE_IDLE;
        int32_t goblin_carried_goods = 0;
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
            if (goblins->carried_goods[good] < 0 ||
                goblins->carried_goods[good] > CC_SIM_MAX_UNITS ||
                goblins->lair_stock[good] < 0 ||
                goblins->lair_stock[good] > CC_SIM_MAX_UNITS ||
                dragon->hoard_goods[good] < 0 ||
                dragon->hoard_goods[good] > CC_SIM_MAX_UNITS) {
                SetError(error, error_capacity,
                         "Goblin inventory is invalid.");
                return false;
            }
            goblin_carried_goods += goblins->carried_goods[good];
        }
        bool goblin_trip_valid = goblins_idle ?
            goblins->tribute_target_id == 0U &&
            goblins->tribute_event_id == 0U &&
            goblins->carried_tribute == 0 &&
            goblin_carried_goods == 0 &&
            goblins->carried_treasure_id == 0U &&
            goblins->tribute_days_remaining == 0 :
            CcSimSettlement(sim, goblins->tribute_target_id) != NULL &&
            CcSimEvent(sim, goblins->tribute_event_id) != NULL &&
            goblins->tribute_days_remaining > 0 &&
            goblins->carried_tribute >= 0;
        if (CcIdKind(goblins->id) != CC_ENTITY_GOBLIN_CULT ||
            !ValidBoundedText(goblins->name, sizeof(goblins->name)) ||
            goblins->members < 1 ||
            goblins->members > 120 || goblins->devotion < 0 ||
            goblins->devotion > 100 || goblins->cohesion < 0 ||
            goblins->cohesion > 100 ||
            goblins->tribute_phase < CC_GOBLIN_TRIBUTE_IDLE ||
            goblins->tribute_phase > CC_GOBLIN_TRIBUTE_PREPARING ||
            goblins->raid_motive < CC_GOBLIN_RAID_NONE ||
            goblins->raid_motive > CC_GOBLIN_RAID_DRAGON_TRIBUTE ||
            CcSimSettlement(sim, goblins->lair_settlement_id) == NULL ||
            goblins->lair_coins < 0 ||
            goblins->lair_coins > CC_SIM_MAX_MONEY ||
            goblins->carried_tribute < 0 ||
            goblins->carried_tribute > CC_SIM_MAX_MONEY ||
            goblins->tribute_cooldown_days < 0 ||
            goblins->tributes_delivered < 0 ||
            goblins->tributes_delivered > CC_SIM_MAX_UNITS ||
            goblins->hoard_defenses < 0 ||
            goblins->hoard_defenses > CC_SIM_MAX_UNITS ||
            goblins->expeditions_intercepted < 0 ||
            goblins->expeditions_intercepted > CC_SIM_MAX_UNITS ||
            (goblins->target_warned &&
             goblins->tribute_phase != CC_GOBLIN_TRIBUTE_PREPARING &&
             goblins->tribute_phase != CC_GOBLIN_TRIBUTE_OUTBOUND) ||
            goblins->dragon_seed_phase < CC_GOBLIN_DRAGON_SEED_NONE ||
            goblins->dragon_seed_phase > CC_GOBLIN_DRAGON_SEED_PREPARING ||
            goblins->dragon_seed_days_remaining < 0 ||
            (goblins->dragon_seed_phase == CC_GOBLIN_DRAGON_SEED_NONE &&
             goblins->dragon_seed_days_remaining != 0) ||
            (goblins->dragon_seed_phase != CC_GOBLIN_DRAGON_SEED_NONE &&
             (!dragon->slain || dragon->egg_count != 0)) ||
            !goblin_trip_valid ||
            (goblins->last_tribute_origin_id != 0U &&
             CcSimSettlement(sim, goblins->last_tribute_origin_id) == NULL)) {
            SetError(error, error_capacity,
                     "Goblin tribute state is invalid.");
            return false;
        }
        bool dragon_calm = dragon->stolen_outstanding == 0;
        bool dragon_anger_valid = dragon_calm ?
            dragon->theft_actor_id == 0U &&
            dragon->retaliation_target_id == 0U &&
            dragon->omen_event_id == 0U &&
            dragon->omen_days_remaining == 0 &&
            dragon->stolen_treasure_id == 0U :
            CcSimSettlement(sim, dragon->retaliation_target_id) != NULL &&
            dragon->retaliation_target_id != dragon->lair_settlement_id &&
            (dragon->theft_actor_id == sim->player.id ||
             dragon->theft_actor_id == sim->hoard_raiders.id) &&
            CcSimEvent(sim, dragon->omen_event_id) != NULL &&
            dragon->omen_days_remaining > 0 &&
            dragon->omen_days_remaining <= 28;
        const CcTreasure *stolen_named = dragon->stolen_treasure_id != 0U ?
            CcSimTreasure(sim, dragon->stolen_treasure_id) : NULL;
        bool named_theft_valid = dragon->stolen_treasure_id == 0U ||
            (stolen_named != NULL && !stolen_named->destroyed &&
             stolen_named->owner_id == sim->player.id &&
             stolen_named->location_id == sim->player.location_id &&
             dragon->theft_actor_id == sim->player.id &&
             dragon->stolen_outstanding == stolen_named->appraised_value);
        if (CcIdKind(dragon->id) != CC_ENTITY_DRAGON ||
            !ValidBoundedText(dragon->name, sizeof(dragon->name)) ||
            CcSimSettlement(sim, dragon->lair_settlement_id) == NULL ||
            dragon->hoard < 0 || dragon->hoard > CC_SIM_MAX_MONEY ||
            dragon->stolen_outstanding < 0 ||
            dragon->stolen_outstanding > CC_SIM_MAX_MONEY ||
            dragon->retaliations < 0 ||
            dragon->retaliations > CC_SIM_MAX_UNITS ||
            !dragon_anger_valid ||
            !named_theft_valid ||
            (dragon->slain &&
             (dragon->slain_day < 1 ||
              dragon->slain_day > sim->current_day ||
              !dragon_calm)) ||
            (dragon->hoard_event_id != 0U &&
             CcSimEvent(sim, dragon->hoard_event_id) == NULL)) {
            SetError(error, error_capacity, "Dragon state is invalid.");
            return false;
        }
        bool brood_valid = dragon->egg_count == 0 ?
            dragon->brood_days_remaining == 0 :
            dragon->brood_days_remaining > 0;
        bool life_valid = dragon->slain ?
            dragon->life_stage == CC_DRAGON_STAGE_AFTERDRAGON &&
                dragon->activity == CC_DRAGON_ACTIVITY_AFTERMATH &&
                dragon->body_condition == 0 &&
                dragon->crown_strength == 0 :
            dragon->life_stage != CC_DRAGON_STAGE_AFTERDRAGON;
        if (dragon->life_stage < CC_DRAGON_STAGE_EGG ||
            dragon->life_stage > CC_DRAGON_STAGE_AFTERDRAGON ||
            dragon->activity < CC_DRAGON_ACTIVITY_DORMANT ||
            dragon->activity > CC_DRAGON_ACTIVITY_AFTERMATH ||
            dragon->age_days < 0 || dragon->age_days > CC_SIM_MAX_DAY ||
            dragon->body_condition < 0 ||
            dragon->body_condition > 100 || dragon->crown_strength < 0 ||
            dragon->crown_strength > 100 || dragon->memory_integrity < 0 ||
            dragon->memory_integrity > 100 ||
            dragon->territory_stability < 0 ||
            dragon->territory_stability > 100 ||
            dragon->regional_influence < 0 ||
            dragon->regional_influence > 100 ||
            dragon->crown_continuity_days < 0 ||
            dragon->hunt_cooldown_days < 0 || dragon->hunts < 0 ||
            dragon->egg_count < 0 || dragon->egg_count > 3 ||
            dragon->brood_days_remaining < 0 ||
            dragon->brood_cooldown_days < 0 ||
            dragon->broods_laid < 0 || dragon->whelps_dispersed < 0 ||
            dragon->afterdeath_days < 0 || !brood_valid || !life_valid ||
            (dragon->lifecycle_event_id != 0U &&
             CcSimEvent(sim, dragon->lifecycle_event_id) == NULL)) {
            SetError(error, error_capacity,
                     "Dragon ecology state is invalid.");
            return false;
        }
        const CcDragonCampaign *campaign = &sim->dragon_campaign;
        bool campaign_idle =
            campaign->phase == CC_DRAGON_CAMPAIGN_IDLE;
        bool campaign_trip_valid = campaign_idle ?
            campaign->origin_settlement_id == 0U &&
            campaign->days_remaining == 0 &&
            campaign->recovered_coins == 0 :
            CcSimSettlement(sim, campaign->origin_settlement_id) != NULL &&
            campaign->days_remaining > 0 &&
            MaskCount(campaign->alliance_kingdom_mask) >= 2;
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
            if (campaign->supplies[good] < 0 ||
                campaign->supplies[good] > CC_SIM_MAX_UNITS) {
                SetError(error, error_capacity,
                         "Dragon-host supplies are invalid.");
                return false;
            }
        }
        if (campaign->phase < CC_DRAGON_CAMPAIGN_IDLE ||
            campaign->phase > CC_DRAGON_CAMPAIGN_RETURNING ||
            campaign->cooldown_days < 0 ||
            campaign->recovered_coins < 0 ||
            campaign->recovered_coins > CC_SIM_MAX_MONEY ||
            campaign->attempts < 0 ||
            campaign->attempts > CC_SIM_MAX_UNITS ||
            campaign->victories < 0 ||
            campaign->victories > CC_SIM_MAX_UNITS ||
            campaign->defeats < 0 ||
            campaign->defeats > CC_SIM_MAX_UNITS ||
            (int64_t)campaign->victories + campaign->defeats >
                campaign->attempts ||
            !campaign_trip_valid ||
            (campaign->cause_event_id != 0U &&
             CcSimEvent(sim, campaign->cause_event_id) == NULL)) {
            SetError(error, error_capacity,
                     "Dragon campaign state is invalid.");
            return false;
        }
        const CcHoardRaiders *raiders = &sim->hoard_raiders;
        bool raiders_idle = raiders->phase == CC_HOARD_RAIDERS_IDLE;
        bool raider_trip_valid = raiders_idle ?
            raiders->motive == CC_HOARD_RAID_NO_MOTIVE &&
            raiders->origin_settlement_id == 0U &&
            raiders->cause_event_id == 0U &&
            raiders->carried_treasure == 0 &&
            raiders->days_remaining == 0 :
            (raiders->motive == CC_HOARD_RAID_SOCIAL_RELIEF ||
             raiders->motive == CC_HOARD_RAID_WAR_FINANCE) &&
            CcSimSettlement(sim, raiders->origin_settlement_id) != NULL &&
            CcSimEvent(sim, raiders->cause_event_id) != NULL &&
            raiders->days_remaining > 0 &&
            raiders->carried_treasure >= 0 &&
            (raiders->phase == CC_HOARD_RAIDERS_RETURNING ||
             raiders->carried_treasure == 0);
        if (CcIdKind(raiders->id) != CC_ENTITY_HOARD_RAIDERS ||
            !ValidBoundedText(raiders->name, sizeof(raiders->name)) ||
            raiders->phase < CC_HOARD_RAIDERS_IDLE ||
            raiders->phase > CC_HOARD_RAIDERS_RETURNING ||
            raiders->carried_treasure < 0 ||
            raiders->carried_treasure > CC_SIM_MAX_MONEY ||
            raiders->cooldown_days < 0 || raiders->raids_completed < 0 ||
            raiders->raids_completed > CC_SIM_MAX_UNITS ||
            raiders->war_raids_completed < 0 ||
            raiders->war_raids_completed > raiders->raids_completed ||
            !raider_trip_valid) {
            SetError(error, error_capacity,
                     "Hoard-raider state is invalid.");
            return false;
        }
    }
    if (sim->schema_version >= 17U) {
        for (int32_t i = 0; i < sim->character_count; ++i) {
            const CcCharacter *character = &sim->characters[i];
            bool faction_exists = character->faction_id == 0U;
            for (int32_t faction = 0;
                 !faction_exists && faction < sim->faction_count; ++faction) {
                faction_exists = sim->factions[faction].id ==
                                 character->faction_id;
            }
            if (CcIdKind(character->id) != CC_ENTITY_CHARACTER ||
                !ValidBoundedText(character->name,
                                  sizeof(character->name)) ||
                CcSimSettlement(sim, character->home_settlement_id) == NULL ||
                CcSimSettlement(sim,
                                character->current_settlement_id) == NULL ||
                !faction_exists ||
                character->role < CC_CHARACTER_OFFICIAL ||
                character->role > CC_CHARACTER_COURIER ||
                character->goal < CC_CHARACTER_GOAL_KEEP_ORDER ||
                character->goal > CC_CHARACTER_GOAL_CARRY_NEWS ||
                character->activity < CC_CHARACTER_ACTIVITY_WORKING ||
                character->activity > CC_CHARACTER_ACTIVITY_TRAVELLING ||
                character->player_disposition < -100 ||
                character->player_disposition > 100 ||
                character->stress < 0 || character->stress > 100 ||
                character->courage < 0 || character->courage > 100 ||
                character->memory_count < 0 ||
                character->memory_count > CC_CHARACTER_MEMORY_CAPACITY ||
                character->memory_write_index < 0 ||
                character->memory_write_index >=
                    CC_CHARACTER_MEMORY_CAPACITY) {
                SetError(error, error_capacity,
                         "Character state is invalid.");
                return false;
            }
            for (int32_t memory = 0;
                 memory < character->memory_count; ++memory) {
                const CcCharacterMemory *item =
                    &character->memories[memory];
                if (item->kind <= CC_CHARACTER_MEMORY_NONE ||
                    item->kind > CC_CHARACTER_MEMORY_PLAYER_WITHDREW ||
                    CcSimSituation(sim, item->subject_id) == NULL ||
                    CcSimEvent(sim, item->event_id) == NULL ||
                    item->day < 1 || item->day > sim->current_day) {
                    SetError(error, error_capacity,
                             "Character memory is invalid.");
                    return false;
                }
            }
        }
    }
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        const CcSituation *situation = &sim->situations[i];
        CcEntityKind target_kind = CcIdKind(situation->target_id);
        CcEntityKind expected_kind = situation->kind == CC_SITUATION_ROUTE_REPAIR ?
                                     CC_ENTITY_ROUTE :
                                     situation->kind == CC_SITUATION_MONSTER_EXPEDITION ?
                                     CC_ENTITY_DUNGEON :
                                     situation->kind == CC_SITUATION_COURIER_DELIVERY ?
                                     CC_ENTITY_COURIER : CC_ENTITY_SETTLEMENT;
        bool target_exists = false;
        if (expected_kind == CC_ENTITY_ROUTE) {
            target_exists = CcSimRoute(sim, situation->target_id) != NULL;
        } else if (expected_kind == CC_ENTITY_DUNGEON) {
            for (int32_t dungeon = 0; dungeon < sim->dungeon_count;
                 ++dungeon) {
                if (sim->dungeons[dungeon].id == situation->target_id) {
                    target_exists = true;
                    break;
                }
            }
        } else if (expected_kind == CC_ENTITY_COURIER) {
            for (int32_t courier = 0; courier < sim->courier_count;
                 ++courier) {
                if (sim->couriers[courier].id == situation->target_id) {
                    target_exists = true;
                    break;
                }
            }
        } else {
            target_exists = CcSimSettlement(sim, situation->target_id) != NULL;
        }
        bool issuer_exists = situation->issuer_faction_id == 0U;
        for (int32_t faction = 0;
             !issuer_exists && faction < sim->faction_count; ++faction) {
            issuer_exists = sim->factions[faction].id ==
                            situation->issuer_faction_id;
        }
        if (CcIdKind(situation->id) != CC_ENTITY_SITUATION ||
            !ValidOptionalBoundedText(situation->sponsor_name,
                                      sizeof(situation->sponsor_name)) ||
            !ValidOptionalBoundedText(situation->affected_name,
                                      sizeof(situation->affected_name)) ||
            (sim->schema_version >= 17U &&
             (CcSimCharacter(sim, situation->sponsor_character_id) == NULL ||
              CcSimCharacter(sim, situation->affected_character_id) == NULL ||
              strcmp(CcSimCharacter(sim, situation->sponsor_character_id)->name,
                     situation->sponsor_name) != 0 ||
              strcmp(CcSimCharacter(sim, situation->affected_character_id)->name,
                     situation->affected_name) != 0)) ||
            situation->kind < CC_SITUATION_RELIEF_DELIVERY ||
            situation->kind > CC_SITUATION_COURIER_DELIVERY ||
            target_kind != expected_kind || !target_exists ||
            !issuer_exists ||
            situation->status < CC_SITUATION_ACTIVE ||
            situation->status > CC_SITUATION_FAILED ||
            situation->good < 0 || situation->good >= CC_GOOD_COUNT ||
            situation->quantity < 0 ||
            situation->quantity > CC_SIM_MAX_UNITS ||
            situation->progress < 0 ||
            situation->progress > situation->quantity ||
            situation->reward < 0 ||
            situation->reward > CC_SIM_MAX_MONEY ||
            situation->created_day < 1 ||
            situation->created_day > sim->current_day ||
            situation->deadline_day < situation->created_day ||
            (sim->schema_version == CC_SIM_SCHEMA_VERSION &&
             situation->cause_event_id != 0U &&
             CcSimEvent(sim, situation->cause_event_id) == NULL)) {
            SetError(error, error_capacity, "Situation data is invalid.");
            return false;
        }
    }
    const CcSituation *accepted = CcSimAcceptedSituation(sim);
    bool nonnegative_cargo = true;
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        if (sim->player.cargo[good] < 0 ||
            sim->player.cargo[good] > CC_SIM_MAX_UNITS) {
            nonnegative_cargo = false;
        }
    }
    if (CcIdKind(sim->player.id) != CC_ENTITY_PLAYER_COMPANY ||
        CcSimSettlement(sim, sim->player.location_id) == NULL ||
        !nonnegative_cargo ||
        CcPlayerCargoUsed(&sim->player) > sim->player.cargo_capacity ||
        sim->player.treasure_cargo_slots != player_treasure_count ||
        sim->player.cargo_capacity < 1 ||
        sim->player.cargo_capacity > CC_SIM_MAX_UNITS ||
        sim->player.passenger_capacity < 0 ||
        sim->player.passenger_capacity > CC_SIM_MAX_UNITS ||
        sim->player.map_capacity < 1 ||
        sim->player.map_capacity > CC_MAX_MAPS ||
        CcPlayerMapCount(sim) > sim->player.map_capacity ||
        sim->player.coins < 0 || sim->player.coins > CC_SIM_MAX_MONEY ||
        sim->player.reputation < -100 || sim->player.reputation > 100 ||
        (sim->player.accepted_situation_id != 0U && accepted == NULL)) {
        SetError(error, error_capacity, "Player company state is invalid.");
        return false;
    }
    if (sim->schema_version == CC_SIM_SCHEMA_VERSION) {
        int32_t horse_count = CcSimHorseCount(sim);
        for (int32_t i = 0; i < horse_count; ++i) {
            const CcHorse *horse = CcSimHorseAt(sim, i);
            const CcHorse *sire = CcSimHorse(sim, horse->sire_id);
            const CcHorse *dam = CcSimHorse(sim, horse->dam_id);
            const CcHorse *pregnancy_sire = CcSimHorse(
                sim, horse->pregnant_by_id);
            bool on_team = i < CC_CARRIAGE_HORSE_COUNT;
            if (CcIdKind(horse->id) != CC_ENTITY_HORSE ||
                !ValidBoundedText(horse->name, sizeof(horse->name)) ||
                horse->age_days < 0 || horse->age_days > CC_SIM_MAX_DAY ||
                horse->health < 1 || horse->health > 100 ||
                horse->fatigue < 0 || horse->fatigue > 100 ||
                horse->hunger < 0 || horse->hunger > 100 ||
                horse->sex < CC_HORSE_MARE ||
                horse->sex > CC_HORSE_STALLION ||
                horse->training < 0 || horse->training > 100 ||
                horse->strength < 1 || horse->strength > 100 ||
                horse->temperament < 1 || horse->temperament > 100 ||
                horse->hardiness < 1 || horse->hardiness > 100 ||
                horse->pregnancy_days_remaining < 0 ||
                horse->pregnancy_days_remaining > 330 ||
                horse->breeding_cooldown_days < 0 ||
                horse->breeding_cooldown_days > 365 ||
                (on_team && horse->stable_settlement_id != 0U) ||
                (!on_team &&
                 (!CcSettlementHasService(
                      CcSimSettlement(sim, horse->stable_settlement_id),
                      CC_SERVICE_STABLE))) ||
                (horse->sire_id != 0U &&
                 (sire == NULL || sire->sex != CC_HORSE_STALLION)) ||
                (horse->dam_id != 0U &&
                 (dam == NULL || dam->sex != CC_HORSE_MARE)) ||
                (horse->pregnant_by_id == 0U &&
                 horse->pregnancy_days_remaining != 0) ||
                (horse->pregnant_by_id != 0U &&
                 (horse->sex != CC_HORSE_MARE ||
                  pregnancy_sire == NULL ||
                  pregnancy_sire->sex != CC_HORSE_STALLION)) ||
                (horse->sire_id == horse->id || horse->dam_id == horse->id)) {
                SetError(error, error_capacity,
                         "Carriage horse state is invalid.");
                return false;
            }
            for (int32_t earlier = 0; earlier < i; ++earlier) {
                const CcHorse *other = CcSimHorseAt(sim, earlier);
                if (other != NULL && other->id == horse->id) {
                    SetError(error, error_capacity,
                             "Horse identities are not unique.");
                    return false;
                }
            }
        }
    }
    if (sim->schema_version == 3U) {
        if (sim->journey.active &&
            (CcSimSituation(sim, sim->journey.situation_id) == NULL ||
             CcSimSettlement(sim, sim->journey.origin_id) == NULL ||
             CcSimSettlement(sim, sim->journey.destination_id) == NULL ||
             CcSimRoute(sim, sim->journey.route_id) == NULL ||
             sim->journey.origin_id != sim->player.location_id ||
             sim->journey.danger < 0 || sim->journey.danger > 100 ||
             sim->journey.bargain_cost < 1)) {
            SetError(error, error_capacity,
                     "Prepared journey encounter is invalid.");
            return false;
        }
    } else {
        bool valid_rate = sim->clock.game_minutes_per_second ==
                CC_IDLE_GAME_MINUTES_PER_SECOND ||
            sim->clock.game_minutes_per_second ==
                CC_TRAVEL_GAME_MINUTES_PER_SECOND;
        if (sim->clock.minute_subticks < 0 ||
            sim->clock.minute_subticks >= CC_WORLD_DAY_SUBTICKS ||
            !valid_rate) {
            SetError(error, error_capacity, "World clock state is invalid.");
            return false;
        }
        if (sim->journey.active) {
            bool situation_valid = sim->journey.situation_id == 0U ||
                CcSimSituation(sim, sim->journey.situation_id) != NULL;
            bool phase_valid =
                sim->journey.phase == CC_JOURNEY_PHASE_TRAVELLING ||
                sim->journey.phase == CC_JOURNEY_PHASE_BLOCKED;
            bool encounter_valid =
                sim->journey.phase != CC_JOURNEY_PHASE_BLOCKED ||
                sim->journey.encounter_triggered;
            int32_t expected_rate =
                sim->journey.phase == CC_JOURNEY_PHASE_TRAVELLING ?
                    CC_TRAVEL_GAME_MINUTES_PER_SECOND :
                    CC_IDLE_GAME_MINUTES_PER_SECOND;
            if (!situation_valid || !phase_valid || !encounter_valid ||
                CcSimSettlement(sim, sim->journey.origin_id) == NULL ||
                CcSimSettlement(sim, sim->journey.destination_id) == NULL ||
                CcSimRoute(sim, sim->journey.route_id) == NULL ||
                sim->journey.origin_id != sim->player.location_id ||
                sim->journey.danger < 0 || sim->journey.danger > 100 ||
                sim->journey.bargain_cost < 1 ||
                sim->journey.departure_day < 1 ||
                sim->journey.departure_day > sim->current_day ||
                sim->journey.elapsed_subticks < 0 ||
                sim->journey.total_subticks < CC_WORLD_DAY_SUBTICKS ||
                sim->journey.total_subticks >
                    CC_SIM_MAX_ROUTE_DAYS * CC_WORLD_DAY_SUBTICKS ||
                sim->journey.elapsed_subticks >
                    sim->journey.total_subticks ||
                sim->journey.encounter_subticks < 0 ||
                sim->journey.encounter_subticks >
                    sim->journey.total_subticks ||
                sim->journey.fare_reserved < 1 ||
                sim->journey.pace < CC_JOURNEY_PACE_CAREFUL ||
                sim->journey.pace > CC_JOURNEY_PACE_PUSH ||
                (sim->journey.ambush_warned &&
                 !sim->journey.ambush_pending &&
                 !sim->journey.ambush_resolved) ||
                (sim->journey.parent_event_id != 0U &&
                 CcSimEvent(sim, sim->journey.parent_event_id) == NULL) ||
                sim->clock.game_minutes_per_second != expected_rate) {
                SetError(error, error_capacity,
                         "Persistent journey state is invalid.");
                return false;
            }
            CcCarriageMode expected_mode =
                sim->journey.phase == CC_JOURNEY_PHASE_BLOCKED ?
                    CC_CARRIAGE_STOPPED : CC_CARRIAGE_MOVING;
            int32_t expected_progress = (int32_t)(
                ((int64_t)sim->journey.elapsed_subticks * 1000) /
                sim->journey.total_subticks);
            if (sim->carriage.mode != expected_mode ||
                sim->carriage.route_id != sim->journey.route_id ||
                sim->carriage.origin_id != sim->journey.origin_id ||
                sim->carriage.destination_id !=
                    sim->journey.destination_id ||
                sim->carriage.progress_milli < 0 ||
                sim->carriage.progress_milli > 1000 ||
                sim->carriage.progress_milli != expected_progress ||
                (sim->carriage.mode == CC_CARRIAGE_MOVING &&
                 sim->carriage.speed_milli_per_second !=
                    JourneyCarriageSpeedForPace(
                        sim->journey.total_subticks,
                        sim->journey.pace)) ||
                (sim->carriage.mode == CC_CARRIAGE_STOPPED &&
                 sim->carriage.speed_milli_per_second != 0) ||
                sim->carriage.condition < 0 ||
                sim->carriage.condition > 100) {
                SetError(error, error_capacity,
                         "Carriage journey state is invalid.");
                return false;
            }
        } else if (sim->journey.phase != CC_JOURNEY_PHASE_NONE ||
                   sim->carriage.mode != CC_CARRIAGE_PARKED ||
                   sim->carriage.location_id != sim->player.location_id ||
                   sim->clock.game_minutes_per_second !=
                       CC_IDLE_GAME_MINUTES_PER_SECOND ||
                   sim->carriage.condition < 0 ||
                   sim->carriage.condition > 100) {
            SetError(error, error_capacity, "Parked carriage state is invalid.");
            return false;
        }
    }
    if (sim->resolved_journey_outcome < CC_JOURNEY_OUTCOME_NONE ||
        sim->resolved_journey_outcome > CC_JOURNEY_OUTCOME_NEGOTIATED ||
        (sim->resolved_journey_situation_id != 0U &&
         (CcSimSituation(sim, sim->resolved_journey_situation_id) == NULL ||
          CcSimSettlement(sim, sim->journey.destination_id) == NULL ||
          CcSimRoute(sim, sim->journey.route_id) == NULL))) {
        SetError(error, error_capacity, "Journey outcome state is invalid.");
        return false;
    }
    if (!ValidOptionalBoundedText(sim->delayed_echo.character_name,
                                  sizeof(sim->delayed_echo.character_name)) ||
        (sim->delayed_echo.active &&
        (CcSimSituation(sim, sim->delayed_echo.situation_id) == NULL ||
         CcSimSettlement(sim, sim->delayed_echo.settlement_id) == NULL ||
         sim->delayed_echo.outcome <= CC_JOURNEY_OUTCOME_NONE ||
         sim->delayed_echo.outcome > CC_JOURNEY_OUTCOME_NEGOTIATED ||
         sim->delayed_echo.due_day < 0 ||
         (sim->delayed_echo.parent_event_id != 0U &&
          CcSimEvent(sim, sim->delayed_echo.parent_event_id) == NULL) ||
         sim->delayed_echo.character_name[0] == '\0'))) {
        SetError(error, error_capacity, "Delayed journey echo is invalid.");
        return false;
    }
    SetError(error, error_capacity, "");
    return true;
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

static uint64_t HashString(uint64_t hash, const char *text)
{
    while (*text != '\0') {
        hash ^= (uint8_t)*text;
        hash *= UINT64_C(1099511628211);
        text += 1;
    }
    return HashU64(hash, 0U);
}

uint64_t CcSimHash(const CcSim *sim)
{
    if (sim == NULL) return 0U;
    uint64_t hash = UINT64_C(1469598103934665603);
#define HASH_VALUE(value) hash = HashU64(hash, (uint64_t)(value))
    HASH_VALUE(sim->schema_version);
    HASH_VALUE(sim->generator_version);
    HASH_VALUE(sim->world_seed);
    HASH_VALUE(sim->random_state);
    HASH_VALUE(sim->current_day);
    HASH_VALUE(sim->next_entity_serial);
    if (sim->schema_version >= 10U) {
        HASH_VALUE(sim->iron_ledger_reserve);
    }
    HASH_VALUE(sim->kingdom_count);
    HASH_VALUE(sim->settlement_count);
    HASH_VALUE(sim->route_count);
    HASH_VALUE(sim->map_count);
    if (sim->schema_version >= 9U) HASH_VALUE(sim->treasure_count);
    HASH_VALUE(sim->faction_count);
    HASH_VALUE(sim->shipment_count);
    HASH_VALUE(sim->bandit_count);
    HASH_VALUE(sim->monster_count);
    HASH_VALUE(sim->dungeon_count);
    HASH_VALUE(sim->situation_count);
    HASH_VALUE(sim->event_count);
    HASH_VALUE(sim->event_write_index);
    if (sim->schema_version >= 11U) {
        HASH_VALUE(sim->courier_count);
        for (int32_t first = 0; first < sim->kingdom_count; ++first) {
            for (int32_t second = 0;
                 second < sim->kingdom_count; ++second) {
                HASH_VALUE(sim->diplomacy[first][second]);
                HASH_VALUE(sim->diplomacy_changed_day[first][second]);
            }
        }
    }
    for (int32_t i = 0; i < sim->kingdom_count; ++i) {
        const CcKingdom *item = &sim->kingdoms[i];
        HASH_VALUE(item->id); hash = HashString(hash, item->name);
        HASH_VALUE(item->color_r); HASH_VALUE(item->color_g); HASH_VALUE(item->color_b);
        HASH_VALUE(item->treasury); HASH_VALUE(item->legitimacy);
        if (sim->schema_version >= 10U) {
            HASH_VALUE(item->iron_ledger_debt);
        }
    }
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        const CcSettlement *item = &sim->settlements[i];
        HASH_VALUE(item->id); HASH_VALUE(item->kingdom_id);
        hash = HashString(hash, item->name); HASH_VALUE(item->function);
        HASH_VALUE(item->map_x); HASH_VALUE(item->map_y); HASH_VALUE(item->population);
        HASH_VALUE(item->security); HASH_VALUE(item->prosperity); HASH_VALUE(item->hunger);
        if (sim->schema_version >= 5U) {
            HASH_VALUE(item->size); HASH_VALUE(item->service_mask);
            HASH_VALUE(item->service_project);
            HASH_VALUE(item->service_project_days);
        }
        int32_t good_count = sim->schema_version >= 9U ? CC_GOOD_COUNT : 3;
        for (int32_t good = 0; good < good_count; ++good) {
            HASH_VALUE(item->stock[good]); HASH_VALUE(item->reserve_target[good]);
            HASH_VALUE(item->production[good]); HASH_VALUE(item->consumption[good]);
            HASH_VALUE(item->price[good]);
        }
        if (sim->schema_version >= 8U) {
            HASH_VALUE(item->market_coins);
            HASH_VALUE(item->war_chest);
        }
        if (sim->schema_version >= 9U) {
            HASH_VALUE(item->field_yield);
            HASH_VALUE(item->iron_deposit);
            HASH_VALUE(item->gold_seam);
            HASH_VALUE(item->gem_seam);
            HASH_VALUE(item->gold_progress);
            HASH_VALUE(item->gem_progress);
            HASH_VALUE(item->farm_tool_wear);
            HASH_VALUE(item->mine_tool_wear);
            HASH_VALUE(item->smith_tool_wear);
            HASH_VALUE(item->treasure_gold_committed);
            HASH_VALUE(item->treasure_gems_committed);
            HASH_VALUE(item->treasure_work);
        }
        if (sim->schema_version >= 14U) {
            HASH_VALUE(item->cow_adults);
            HASH_VALUE(item->cow_calves);
            HASH_VALUE(item->cow_condition);
            HASH_VALUE(item->cow_hunger);
        }
        HASH_VALUE(sim->last_shortage_level[i]);
    }
    for (int32_t i = 0; i < sim->route_count; ++i) {
        const CcRoute *item = &sim->routes[i];
        HASH_VALUE(item->id); HASH_VALUE(item->from_id); HASH_VALUE(item->to_id);
        HASH_VALUE(item->travel_days); HASH_VALUE(item->capacity); HASH_VALUE(item->security);
        HASH_VALUE(item->condition); HASH_VALUE(item->closed); HASH_VALUE(item->smuggler_route);
    }
    for (int32_t i = 0; i < sim->map_count; ++i) {
        const CcMap *item = &sim->maps[i];
        HASH_VALUE(item->id); HASH_VALUE(item->route_id);
        HASH_VALUE(item->maker_settlement_id); HASH_VALUE(item->owner_id);
        hash = HashString(hash, item->name);
        HASH_VALUE(item->surveyed_day); HASH_VALUE(item->accuracy);
        HASH_VALUE(item->recorded_condition); HASH_VALUE(item->recorded_danger);
        HASH_VALUE(item->ask_price); HASH_VALUE(item->contraband);
    }
    if (sim->schema_version >= 9U) {
        for (int32_t i = 0; i < sim->treasure_count; ++i) {
            const CcTreasure *item = &sim->treasures[i];
            HASH_VALUE(item->id); hash = HashString(hash, item->name);
            HASH_VALUE(item->maker_settlement_id); HASH_VALUE(item->owner_id);
            HASH_VALUE(item->location_id); HASH_VALUE(item->gold_content);
            HASH_VALUE(item->gem_content); HASH_VALUE(item->craft_work);
            HASH_VALUE(item->appraised_value); HASH_VALUE(item->created_day);
            HASH_VALUE(item->destroyed);
        }
    }
    for (int32_t i = 0; i < sim->faction_count; ++i) {
        const CcFaction *item = &sim->factions[i];
        HASH_VALUE(item->id); HASH_VALUE(item->kingdom_id); hash = HashString(hash, item->name);
        HASH_VALUE(item->kind); HASH_VALUE(item->power); HASH_VALUE(item->support);
    }
    for (int32_t i = 0; i < sim->shipment_count; ++i) {
        const CcShipment *item = &sim->shipments[i];
        HASH_VALUE(item->id); HASH_VALUE(item->origin_id); HASH_VALUE(item->destination_id);
        HASH_VALUE(item->final_destination_id); HASH_VALUE(item->route_id);
        HASH_VALUE(item->good); HASH_VALUE(item->quantity);
        HASH_VALUE(item->departure_day); HASH_VALUE(item->arrival_day); HASH_VALUE(item->status);
    }
    if (sim->schema_version >= 11U) {
        for (int32_t i = 0; i < sim->courier_count; ++i) {
            const CcCourier *item = &sim->couriers[i];
            HASH_VALUE(item->id); HASH_VALUE(item->kind);
            HASH_VALUE(item->status); HASH_VALUE(item->issuer_kingdom_id);
            HASH_VALUE(item->recipient_kingdom_id);
            HASH_VALUE(item->origin_settlement_id);
            HASH_VALUE(item->destination_settlement_id);
            HASH_VALUE(item->current_settlement_id);
            HASH_VALUE(item->route_id); HASH_VALUE(item->cause_event_id);
            HASH_VALUE(item->situation_id); HASH_VALUE(item->departure_day);
            HASH_VALUE(item->arrival_day); HASH_VALUE(item->reliability);
        }
    }
    for (int32_t i = 0; i < sim->bandit_count; ++i) {
        const CcBanditGroup *item = &sim->bandits[i];
        HASH_VALUE(item->id); HASH_VALUE(item->route_id); hash = HashString(hash, item->name);
        HASH_VALUE(item->members); HASH_VALUE(item->supplies); HASH_VALUE(item->influence);
        if (sim->schema_version >= 5U) {
            HASH_VALUE(item->camp_size); HASH_VALUE(item->service_mask);
            HASH_VALUE(item->raid_phase); HASH_VALUE(item->raid_target_id);
            HASH_VALUE(item->raid_good); HASH_VALUE(item->raid_quantity);
            HASH_VALUE(item->raid_days_remaining);
            HASH_VALUE(item->raids_completed);
        }
        HASH_VALUE(sim->last_bandit_level[i]);
    }
    if (sim->schema_version >= 6U) {
        const CcGoblinCult *goblins = &sim->goblins;
        HASH_VALUE(goblins->id); hash = HashString(hash, goblins->name);
        HASH_VALUE(goblins->members); HASH_VALUE(goblins->devotion);
        HASH_VALUE(goblins->tribute_phase);
        HASH_VALUE(goblins->tribute_target_id);
        HASH_VALUE(goblins->last_tribute_origin_id);
        HASH_VALUE(goblins->tribute_event_id);
        HASH_VALUE(goblins->carried_tribute);
        HASH_VALUE(goblins->tribute_days_remaining);
        HASH_VALUE(goblins->tribute_cooldown_days);
        HASH_VALUE(goblins->tributes_delivered);
        if (sim->schema_version >= 10U) {
            HASH_VALUE(goblins->hoard_defenses);
        }
        if (sim->schema_version >= 16U) {
            HASH_VALUE(goblins->cohesion);
            HASH_VALUE(goblins->target_warned);
            HASH_VALUE(goblins->expeditions_intercepted);
            HASH_VALUE(goblins->dragon_seed_phase);
            HASH_VALUE(goblins->dragon_seed_days_remaining);
        }
        if (sim->schema_version >= 9U) {
            HASH_VALUE(goblins->lair_settlement_id);
            HASH_VALUE(goblins->raid_motive);
            HASH_VALUE(goblins->lair_coins);
            HASH_VALUE(goblins->carried_treasure_id);
            for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
                HASH_VALUE(goblins->carried_goods[good]);
                HASH_VALUE(goblins->lair_stock[good]);
            }
        }
        const CcDragon *dragon = &sim->dragon;
        HASH_VALUE(dragon->id); hash = HashString(hash, dragon->name);
        HASH_VALUE(dragon->lair_settlement_id);
        HASH_VALUE(dragon->hoard);
        HASH_VALUE(dragon->stolen_outstanding);
        if (sim->schema_version >= 7U) {
            HASH_VALUE(dragon->theft_actor_id);
        }
        HASH_VALUE(dragon->retaliation_target_id);
        HASH_VALUE(dragon->hoard_event_id);
        HASH_VALUE(dragon->omen_event_id);
        HASH_VALUE(dragon->omen_days_remaining);
        HASH_VALUE(dragon->retaliations);
        if (sim->schema_version >= 11U) {
            HASH_VALUE(dragon->slain);
            HASH_VALUE(dragon->slain_day);
        }
        if (sim->schema_version >= 12U) {
            HASH_VALUE(dragon->life_stage);
            HASH_VALUE(dragon->activity);
            HASH_VALUE(dragon->age_days);
            HASH_VALUE(dragon->body_condition);
            HASH_VALUE(dragon->crown_strength);
            HASH_VALUE(dragon->memory_integrity);
            HASH_VALUE(dragon->territory_stability);
            HASH_VALUE(dragon->regional_influence);
            HASH_VALUE(dragon->crown_continuity_days);
            HASH_VALUE(dragon->hunt_cooldown_days);
            HASH_VALUE(dragon->hunts);
            HASH_VALUE(dragon->egg_count);
            HASH_VALUE(dragon->brood_days_remaining);
            HASH_VALUE(dragon->brood_cooldown_days);
            HASH_VALUE(dragon->broods_laid);
            HASH_VALUE(dragon->whelps_dispersed);
            HASH_VALUE(dragon->afterdeath_days);
            HASH_VALUE(dragon->lifecycle_event_id);
        }
        if (sim->schema_version >= 9U) {
            HASH_VALUE(dragon->stolen_treasure_id);
            for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
                HASH_VALUE(dragon->hoard_goods[good]);
            }
        }
    }
    if (sim->schema_version >= 7U) {
        const CcHoardRaiders *raiders = &sim->hoard_raiders;
        HASH_VALUE(raiders->id); hash = HashString(hash, raiders->name);
        HASH_VALUE(raiders->phase);
        HASH_VALUE(raiders->motive);
        HASH_VALUE(raiders->origin_settlement_id);
        HASH_VALUE(raiders->cause_event_id);
        HASH_VALUE(raiders->carried_treasure);
        HASH_VALUE(raiders->days_remaining);
        HASH_VALUE(raiders->cooldown_days);
        HASH_VALUE(raiders->raids_completed);
        HASH_VALUE(raiders->war_raids_completed);
        if (sim->schema_version >= 10U) {
            HASH_VALUE(raiders->social_raid_latched);
            HASH_VALUE(raiders->war_raid_latched);
        }
    }
    if (sim->schema_version >= 11U) {
        const CcDragonCampaign *campaign = &sim->dragon_campaign;
        HASH_VALUE(campaign->phase);
        HASH_VALUE(campaign->pledged_kingdom_mask);
        HASH_VALUE(campaign->alliance_kingdom_mask);
        HASH_VALUE(campaign->origin_settlement_id);
        HASH_VALUE(campaign->cause_event_id);
        HASH_VALUE(campaign->days_remaining);
        HASH_VALUE(campaign->cooldown_days);
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
            HASH_VALUE(campaign->supplies[good]);
        }
        HASH_VALUE(campaign->recovered_coins);
        HASH_VALUE(campaign->attempts);
        HASH_VALUE(campaign->victories);
        HASH_VALUE(campaign->defeats);
    }
    for (int32_t i = 0; i < sim->monster_count; ++i) {
        const CcMonsterPopulation *item = &sim->monsters[i];
        HASH_VALUE(item->id); HASH_VALUE(item->dungeon_id); hash = HashString(hash, item->name);
        HASH_VALUE(item->population); HASH_VALUE(item->pressure);
        HASH_VALUE(item->hunting_pressure); HASH_VALUE(sim->last_monster_level[i]);
    }
    for (int32_t i = 0; i < sim->dungeon_count; ++i) {
        const CcDungeon *item = &sim->dungeons[i];
        HASH_VALUE(item->id); HASH_VALUE(item->settlement_id); hash = HashString(hash, item->name);
        HASH_VALUE(item->state); HASH_VALUE(item->depth); HASH_VALUE(item->regional_pressure);
    }
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        const CcSituation *item = &sim->situations[i];
        HASH_VALUE(item->id); HASH_VALUE(item->kind); HASH_VALUE(item->status);
        HASH_VALUE(item->issuer_faction_id); HASH_VALUE(item->target_id);
        HASH_VALUE(item->cause_event_id); HASH_VALUE(item->good);
        HASH_VALUE(item->quantity); HASH_VALUE(item->progress); HASH_VALUE(item->reward);
        HASH_VALUE(item->created_day); HASH_VALUE(item->deadline_day);
        if (sim->schema_version >= 17U) {
            HASH_VALUE(item->sponsor_character_id);
            HASH_VALUE(item->affected_character_id);
        }
        if (item->sponsor_name[0] != '\0' || item->affected_name[0] != '\0') {
            hash = HashString(hash, item->sponsor_name);
            hash = HashString(hash, item->affected_name);
        }
    }
    if (sim->schema_version >= 17U) {
        HASH_VALUE(sim->character_count);
        for (int32_t i = 0; i < sim->character_count; ++i) {
            const CcCharacter *character = &sim->characters[i];
            HASH_VALUE(character->id);
            hash = HashString(hash, character->name);
            HASH_VALUE(character->home_settlement_id);
            HASH_VALUE(character->current_settlement_id);
            HASH_VALUE(character->faction_id);
            HASH_VALUE(character->role);
            HASH_VALUE(character->goal);
            HASH_VALUE(character->activity);
            HASH_VALUE(character->appearance_seed);
            HASH_VALUE(character->player_disposition);
            HASH_VALUE(character->stress);
            HASH_VALUE(character->courage);
            HASH_VALUE(character->memory_count);
            HASH_VALUE(character->memory_write_index);
            for (int32_t memory = 0;
                 memory < CC_CHARACTER_MEMORY_CAPACITY; ++memory) {
                const CcCharacterMemory *item =
                    &character->memories[memory];
                HASH_VALUE(item->kind);
                HASH_VALUE(item->subject_id);
                HASH_VALUE(item->event_id);
                HASH_VALUE(item->day);
            }
        }
    }
    HASH_VALUE(sim->player.id); HASH_VALUE(sim->player.location_id);
    HASH_VALUE(sim->player.coins); HASH_VALUE(sim->player.cargo_capacity);
    HASH_VALUE(sim->player.passenger_capacity); HASH_VALUE(sim->player.map_capacity);
    HASH_VALUE(sim->player.reputation);
    if (sim->schema_version >= 9U) HASH_VALUE(sim->player.treasure_cargo_slots);
    if (sim->schema_version >= 13U) {
        HASH_VALUE(sim->player.map_catalogue_mask);
        HASH_VALUE(sim->player.map_archive_mask);
    }
    if (sim->schema_version >= 14U) {
        for (int32_t i = 0; i < CC_CARRIAGE_HORSE_COUNT; ++i) {
            const CcHorse *horse = &sim->horse_team[i];
            HASH_VALUE(horse->id);
            hash = HashString(hash, horse->name);
            HASH_VALUE(horse->age_days);
            HASH_VALUE(horse->health);
            HASH_VALUE(horse->fatigue);
            HASH_VALUE(horse->hunger);
            if (sim->schema_version >= 15U) {
                HASH_VALUE(horse->sex);
                HASH_VALUE(horse->sire_id);
                HASH_VALUE(horse->dam_id);
                HASH_VALUE(horse->stable_settlement_id);
                HASH_VALUE(horse->pregnant_by_id);
                HASH_VALUE(horse->pregnancy_days_remaining);
                HASH_VALUE(horse->breeding_cooldown_days);
                HASH_VALUE(horse->training);
                HASH_VALUE(horse->strength);
                HASH_VALUE(horse->temperament);
                HASH_VALUE(horse->hardiness);
            }
        }
    }
    if (sim->schema_version >= 15U) {
        HASH_VALUE(sim->stable_horse_count);
        for (int32_t i = 0; i < sim->stable_horse_count; ++i) {
            const CcHorse *horse = &sim->stable_horses[i];
            HASH_VALUE(horse->id);
            hash = HashString(hash, horse->name);
            HASH_VALUE(horse->age_days);
            HASH_VALUE(horse->health);
            HASH_VALUE(horse->fatigue);
            HASH_VALUE(horse->hunger);
            HASH_VALUE(horse->sex);
            HASH_VALUE(horse->sire_id);
            HASH_VALUE(horse->dam_id);
            HASH_VALUE(horse->stable_settlement_id);
            HASH_VALUE(horse->pregnant_by_id);
            HASH_VALUE(horse->pregnancy_days_remaining);
            HASH_VALUE(horse->breeding_cooldown_days);
            HASH_VALUE(horse->training);
            HASH_VALUE(horse->strength);
            HASH_VALUE(horse->temperament);
            HASH_VALUE(horse->hardiness);
        }
    }
    /* Optional schema-v3 extension: omitting zero preserves hashes written by
       saves created before explicit charter commitments existed. */
    if (sim->player.accepted_situation_id != 0U) {
        HASH_VALUE(sim->player.accepted_situation_id);
    }
    if (sim->schema_version == 3U) {
        /* Sparse schema-v3 fields preserve the hashes written before journey
           encounters and explicit charter commitments existed. */
        if (sim->journey.active) {
            HASH_VALUE(sim->journey.active);
            HASH_VALUE(sim->journey.situation_id);
            HASH_VALUE(sim->journey.origin_id);
            HASH_VALUE(sim->journey.destination_id);
            HASH_VALUE(sim->journey.route_id);
            HASH_VALUE(sim->journey.danger);
            HASH_VALUE(sim->journey.bargain_cost);
        }
        if (sim->resolved_journey_situation_id != 0U) {
            HASH_VALUE(sim->resolved_journey_situation_id);
            HASH_VALUE(sim->resolved_journey_outcome);
            HASH_VALUE(sim->journey.destination_id);
            HASH_VALUE(sim->journey.route_id);
        }
        if (sim->delayed_echo.active) {
            HASH_VALUE(sim->delayed_echo.active);
            HASH_VALUE(sim->delayed_echo.situation_id);
            HASH_VALUE(sim->delayed_echo.settlement_id);
            HASH_VALUE(sim->delayed_echo.parent_event_id);
            HASH_VALUE(sim->delayed_echo.outcome);
            HASH_VALUE(sim->delayed_echo.due_day);
            hash = HashString(hash, sim->delayed_echo.character_name);
        }
    } else {
        HASH_VALUE(sim->clock.tick);
        HASH_VALUE(sim->clock.minute_subticks);
        HASH_VALUE(sim->clock.game_minutes_per_second);
        HASH_VALUE(sim->journey.active);
        HASH_VALUE(sim->journey.phase);
        HASH_VALUE(sim->journey.situation_id);
        HASH_VALUE(sim->journey.origin_id);
        HASH_VALUE(sim->journey.destination_id);
        HASH_VALUE(sim->journey.route_id);
        HASH_VALUE(sim->journey.danger);
        HASH_VALUE(sim->journey.bargain_cost);
        HASH_VALUE(sim->journey.departure_day);
        HASH_VALUE(sim->journey.elapsed_subticks);
        HASH_VALUE(sim->journey.total_subticks);
        HASH_VALUE(sim->journey.encounter_subticks);
        HASH_VALUE(sim->journey.fare_reserved);
        HASH_VALUE(sim->journey.encounter_triggered);
        HASH_VALUE(sim->journey.ambush_pending);
        HASH_VALUE(sim->journey.ambush_resolved);
        if (sim->schema_version >= 18U) {
            HASH_VALUE(sim->journey.pace);
            HASH_VALUE(sim->journey.ambush_warned);
        }
        HASH_VALUE(sim->journey.parent_event_id);
        HASH_VALUE(sim->carriage.mode);
        HASH_VALUE(sim->carriage.location_id);
        HASH_VALUE(sim->carriage.route_id);
        HASH_VALUE(sim->carriage.origin_id);
        HASH_VALUE(sim->carriage.destination_id);
        HASH_VALUE(sim->carriage.progress_milli);
        HASH_VALUE(sim->carriage.speed_milli_per_second);
        HASH_VALUE(sim->carriage.condition);
        HASH_VALUE(sim->resolved_journey_situation_id);
        HASH_VALUE(sim->resolved_journey_outcome);
        HASH_VALUE(sim->delayed_echo.active);
        HASH_VALUE(sim->delayed_echo.situation_id);
        HASH_VALUE(sim->delayed_echo.settlement_id);
        HASH_VALUE(sim->delayed_echo.parent_event_id);
        HASH_VALUE(sim->delayed_echo.outcome);
        HASH_VALUE(sim->delayed_echo.due_day);
        hash = HashString(hash, sim->delayed_echo.character_name);
    }
    int32_t player_good_count = sim->schema_version >= 9U ? CC_GOOD_COUNT : 3;
    for (int32_t good = 0; good < player_good_count; ++good) {
        HASH_VALUE(sim->player.cargo[good]);
    }
    for (int32_t i = 0; i < CC_MAX_EVENTS; ++i) {
        const CcEvent *item = &sim->events[i];
        HASH_VALUE(item->id); HASH_VALUE(item->day); HASH_VALUE(item->kind);
        HASH_VALUE(item->subject_id); HASH_VALUE(item->location_id); HASH_VALUE(item->parent_id);
        HASH_VALUE(item->magnitude); hash = HashString(hash, item->text);
    }
#undef HASH_VALUE
    return hash;
}
