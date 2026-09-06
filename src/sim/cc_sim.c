#include "sim/cc_sim.h"

#include "quest/cc_quest.h"

#include <inttypes.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#define CC_ID_SERIAL_MASK UINT64_C(0x00ffffffffffffff)

static void GenerateSituations(CcSim *sim);
static void DeliverDelayedEchoIfReady(CcSim *sim);
static void ResolveSituation(CcSim *sim, CcSituation *situation);
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
static void PlanTrade(CcSim *sim);
static int32_t SettlementSlotById(const CcSim *sim, CcId id);
static int32_t CalculateDragonCrownStrength(const CcSim *sim);
static bool DragonIsAliveAndUncrowned(const CcSim *sim);
static int32_t TakeAllianceGood(CcSim *sim, uint32_t mask, CcGood good,
                                int32_t quantity);
static CcId LatestLocalCause(const CcSim *sim, CcId location);
static uint32_t RoadHouseSeed(const CcSim *sim, CcId route_id);
static const char *GeneratedRoadHouseName(const CcSim *sim, CcId route_id);
static const CcSettlement *Scriptorium(const CcSim *sim);
static void AssignHistoryOffices(CcSim *sim, bool announce);

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

static uint32_t MixCharacterSeed(uint32_t value)
{
    value ^= value >> 16U;
    value *= UINT32_C(0x7feb352d);
    value ^= value >> 15U;
    value *= UINT32_C(0x846ca68b);
    value ^= value >> 16U;
    return value;
}

void CcGenerateCharacterName(uint32_t world_seed, CcId settlement_id,
                             int32_t generation, uint32_t ordinal,
                             char output[CC_NAME_CAPACITY])
{
    static const char *first_names[] = {
        "Alden", "Anja", "Brin", "Cera", "Dain", "Edda", "Elian",
        "Fara", "Garr", "Hesta", "Ilya", "Jory", "Kellan", "Lysa",
        "Mara", "Neryn", "Orin", "Petra", "Quin", "Rhea", "Sabin",
        "Tamsin", "Ulric", "Vela", "Wren", "Ysra", "Zorin", "Bren",
        "Kael", "Mina", "Oren", "Sela"
    };
    static const char *family_names[] = {
        "Alder", "Ash", "Barrow", "Drost", "Ember", "Fen", "Gale",
        "Holt", "Ives", "Kest", "Lark", "Mott", "North", "Reed",
        "Rill", "Senn", "Thorne", "Vale", "Varo", "Venn", "Wold",
        "Yarrow", "Flint", "Mere", "Pike", "Rook", "Stone", "Tern",
        "Wick", "Zeal"
    };
    if (output == NULL) return;
    uint32_t settlement = (uint32_t)(settlement_id ^
                                     (settlement_id >> 32U));
    uint32_t seed = MixCharacterSeed(
        world_seed ^ settlement ^
        ((uint32_t)generation + 1U) * UINT32_C(0x9e3779b9) ^
        (ordinal + 1U) * UINT32_C(0x85ebca6b));
    uint32_t first = seed %
        (uint32_t)(sizeof(first_names) / sizeof(first_names[0]));
    uint32_t family = MixCharacterSeed(seed ^ UINT32_C(0xa511e9b3)) %
        (uint32_t)(sizeof(family_names) / sizeof(family_names[0]));
    (void)snprintf(output, CC_NAME_CAPACITY, "%s %s",
                   first_names[first], family_names[family]);
}

int32_t CcCharacterAgeYears(const CcSim *sim,
                            const CcCharacter *character)
{
    if (sim == NULL || character == NULL ||
        character->birth_day > sim->current_day) return 0;
    int64_t age_days = (int64_t)sim->current_day - character->birth_day;
    if (age_days / 365 > INT32_MAX) return INT32_MAX;
    return (int32_t)(age_days / 365);
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
    sim->dragon.hair_color = CC_DRAGON_HAIR_PURPLE;
    CopyName(sim->dragon.name, "Varkesh the Unappeased");
    sim->dragon.lair_settlement_id =
        sim->settlements[sim->settlement_count - 1].id;

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
    CcSimUpgradeFlockEconomy(sim);
}

void CcSimUpgradeFlockEconomy(CcSim *sim)
{
    if (sim == NULL) return;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        CcSettlement *place = &sim->settlements[i];
        place->reserve_target[CC_GOOD_MEAT] = MaximumI32(
            place->reserve_target[CC_GOOD_MEAT],
            MaximumI32(4, place->population / 600));
        place->reserve_target[CC_GOOD_WOOL] = MaximumI32(
            place->reserve_target[CC_GOOD_WOOL],
            MaximumI32(4, place->population / 500));
        if (!CcSettlementHasService(place, CC_SERVICE_FARM)) continue;
        switch (place->function) {
            case CC_SETTLEMENT_FARMING:
                place->sheep_adults = 42;
                place->sheep_lambs = 10;
                break;
            case CC_SETTLEMENT_CAPITAL:
                place->sheep_adults = 30;
                place->sheep_lambs = 7;
                break;
            case CC_SETTLEMENT_FORTRESS:
                place->sheep_adults = 24;
                place->sheep_lambs = 6;
                break;
            case CC_SETTLEMENT_MINING:
                place->sheep_adults = 14;
                place->sheep_lambs = 3;
                break;
            case CC_SETTLEMENT_MARKET:
            case CC_SETTLEMENT_DUNGEON_TOWN:
                place->sheep_adults = 12;
                place->sheep_lambs = 3;
                break;
        }
        place->sheep_condition = 88;
        place->sheep_hunger = 0;
        place->stock[CC_GOOD_WOOL] = MaximumI32(
            place->stock[CC_GOOD_WOOL], place->sheep_adults / 8);
        place->stock[CC_GOOD_MEAT] = MaximumI32(
            place->stock[CC_GOOD_MEAT],
            MaximumI32(1, place->sheep_adults / 16));
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
    if (sim->schema_version >= 44U) {
        for (int32_t i = 0; i < CC_MAX_GOSSIP; ++i) {
            if (!sim->gossip[i].recorded &&
                (sim->gossip[i].event_id == event_id ||
                 sim->gossip[i].heard_event_id == event_id)) return true;
        }
    }
    if (event_id == incoming_parent ||
        event_id == sim->journey.parent_event_id ||
        event_id == sim->delayed_echo.parent_event_id ||
        event_id == sim->goblins.tribute_event_id ||
        event_id == sim->dragon.hoard_event_id ||
        event_id == sim->dragon.omen_event_id ||
        event_id == sim->dragon.lifecycle_event_id ||
        event_id == sim->hoard_raiders.cause_event_id ||
        event_id == sim->dragon_campaign.cause_event_id) return true;
    for (int32_t i = 0; i < sim->pending_echo_count; ++i) {
        if (sim->pending_echoes[i].parent_event_id == event_id) return true;
    }
    for (int32_t i = 0; i < sim->courier_count; ++i) {
        if (sim->couriers[i].cause_event_id == event_id) return true;
    }
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        const CcSituation *situation = &sim->situations[i];
        if (situation->cause_event_id == event_id ||
            situation->lead_event_id == event_id ||
            situation->objective.progress.created_by_event_id == event_id ||
            situation->objective.progress.resolved_by_event_id == event_id ||
            situation->objective.danger.created_by_event_id == event_id ||
            situation->objective.danger.resolved_by_event_id == event_id) {
            return true;
        }
        for (int32_t evidence = 0;
             evidence < situation->objective.evidence_count; ++evidence) {
            if (situation->objective.evidence_event_ids[evidence] == event_id) {
                return true;
            }
        }
    }
    for (int32_t i = 0; i < sim->front_count; ++i) {
        const CcFront *front = &sim->fronts[i];
        if (front->cause_event_id == event_id ||
            front->created_event_id == event_id ||
            front->resolved_event_id == event_id ||
            front->portent.created_by_event_id == event_id ||
            front->portent.resolved_by_event_id == event_id) return true;
    }
    for (int32_t i = 0; i < sim->quest_outcome_count; ++i) {
        const CcQuestOutcomeRecord *outcome = &sim->quest_outcomes[i];
        if (outcome->cause_event_id == event_id ||
            outcome->resolved_event_id == event_id) return true;
    }
    for (int32_t i = 0; i < sim->character_count; ++i) {
        const CcCharacter *character = &sim->characters[i];
        for (int32_t memory = 0; memory < character->memory_count; ++memory) {
            if (character->memories[memory].event_id == event_id) return true;
        }
        for (int32_t knowledge = 0;
             knowledge < character->knowledge_count; ++knowledge) {
            if (character->knowledge[knowledge].event_id == event_id) return true;
        }
    }
    for (int32_t i = 0; i < sim->relationship_count; ++i) {
        if (sim->relationships[i].cause_event_id == event_id) return true;
    }
    return false;
}

static void RedirectEventReference(CcSim *sim, CcId removed_id,
                                   CcId replacement_id)
{
    if (sim->schema_version >= 44U) {
        for (int32_t i = 0; i < CC_MAX_GOSSIP; ++i) {
            if (sim->gossip[i].heard_event_id == removed_id) {
                sim->gossip[i].heard_event_id = 0U;
            }
        }
    }
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        CcSituation *situation = &sim->situations[i];
        if (situation->cause_event_id == removed_id) {
            situation->cause_event_id = replacement_id;
        }
        if (situation->objective.progress.created_by_event_id == removed_id) {
            situation->objective.progress.created_by_event_id = replacement_id;
        }
        if (situation->objective.progress.resolved_by_event_id == removed_id) {
            situation->objective.progress.resolved_by_event_id = replacement_id;
        }
        if (situation->objective.danger.created_by_event_id == removed_id) {
            situation->objective.danger.created_by_event_id = replacement_id;
        }
        if (situation->objective.danger.resolved_by_event_id == removed_id) {
            situation->objective.danger.resolved_by_event_id = replacement_id;
        }
        for (int32_t evidence = 0;
             evidence < situation->objective.evidence_count; ++evidence) {
            if (situation->objective.evidence_event_ids[evidence] ==
                removed_id) {
                situation->objective.evidence_event_ids[evidence] =
                    replacement_id;
            }
        }
        if (sim->situations[i].lead_event_id == removed_id) {
            sim->situations[i].lead_event_id = replacement_id;
        }
    }
    if (sim->journey.parent_event_id == removed_id) {
        sim->journey.parent_event_id = replacement_id;
    }
    if (sim->delayed_echo.parent_event_id == removed_id) {
        sim->delayed_echo.parent_event_id = replacement_id;
    }
    for (int32_t i = 0; i < sim->pending_echo_count; ++i) {
        if (sim->pending_echoes[i].parent_event_id == removed_id) {
            sim->pending_echoes[i].parent_event_id = replacement_id;
        }
    }
    for (int32_t i = 0; i < sim->front_count; ++i) {
        CcFront *front = &sim->fronts[i];
        if (front->cause_event_id == removed_id) {
            front->cause_event_id = replacement_id;
        }
        if (front->created_event_id == removed_id) {
            front->created_event_id = replacement_id;
        }
        if (front->resolved_event_id == removed_id) {
            front->resolved_event_id = replacement_id;
        }
        if (front->portent.created_by_event_id == removed_id) {
            front->portent.created_by_event_id = replacement_id;
        }
        if (front->portent.resolved_by_event_id == removed_id) {
            front->portent.resolved_by_event_id = replacement_id;
        }
    }
    for (int32_t i = 0; i < sim->quest_outcome_count; ++i) {
        CcQuestOutcomeRecord *outcome = &sim->quest_outcomes[i];
        if (outcome->cause_event_id == removed_id) {
            outcome->cause_event_id = replacement_id;
        }
        if (outcome->resolved_event_id == removed_id) {
            outcome->resolved_event_id = replacement_id;
        }
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
        for (int32_t knowledge = 0;
             knowledge < character->knowledge_count; ++knowledge) {
            if (character->knowledge[knowledge].event_id == removed_id) {
                character->knowledge[knowledge].event_id = replacement_id;
            }
        }
    }
    for (int32_t i = 0; i < sim->relationship_count; ++i) {
        if (sim->relationships[i].cause_event_id == removed_id) {
            sim->relationships[i].cause_event_id = replacement_id;
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

static void LearnPlayerKnowledgeFromEvent(
    CcSim *sim, const CcEvent *event, CcPlayerKnowledgeSource source);

static CcEvent *PushEventRecord(CcSim *sim, CcEventKind kind, CcId subject,
                                CcId location, CcId parent,
                                int32_t magnitude, const char *text)
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

static CcEvent *PushEvent(CcSim *sim, CcEventKind kind, CcId subject,
                          CcId location, CcId parent, int32_t magnitude,
                          const char *text)
{
    CcEvent *event = PushEventRecord(
        sim, kind, subject, location, parent, magnitude, text);
    LearnPlayerKnowledgeFromEvent(
        sim, event, CC_PLAYER_KNOWLEDGE_EVENT);
    return event;
}

static CcEvent *PushSocialEvent(CcSim *sim, CcEventKind kind, CcId subject,
                                CcId location, CcId parent,
                                CcId actor, CcId target,
                                CcId beneficiary, CcId witness,
                                int32_t magnitude, const char *text)
{
    CcEvent *event = PushEventRecord(
        sim, kind, subject, location, parent, magnitude, text);
    event->actor_id = actor;
    event->target_id = target;
    event->beneficiary_id = beneficiary;
    event->witness_id = witness;
    LearnPlayerKnowledgeFromEvent(
        sim, event,
        kind == CC_EVENT_RUMOR_SHARED ? CC_PLAYER_KNOWLEDGE_RUMOR :
        witness == sim->player.id ? CC_PLAYER_KNOWLEDGE_WITNESS :
                                    CC_PLAYER_KNOWLEDGE_EVENT);
    return event;
}

static const CcGoodDefinition GOOD_DEFINITIONS[CC_GOOD_COUNT] = {
    [CC_GOOD_BREAD] = {"Bread", 4, 8, 1, 4, 16},
    [CC_GOOD_IRON] = {"Iron", 8, 4, 1, 4, 8},
    [CC_GOOD_TOOLS] = {"Tools", 14, 2, 1, 1, 4},
    [CC_GOOD_WEAPONS] = {"Weapons", 24, 2, 1, 1, 4},
    [CC_GOOD_GOLD] = {"Raw Gold", 40, 1, 1, 1, 2},
    [CC_GOOD_GEMS] = {"Gems", 70, 1, 1, 1, 1},
    [CC_GOOD_WOOD] = {"Wood", 6, 6, 1, 4, 12},
    [CC_GOOD_WHEAT] = {"Wheat", 3, 10, 1, 4, 20},
    [CC_GOOD_MEAT] = {"Meat", 7, 6, 1, 4, 12},
    [CC_GOOD_WOOL] = {"Wool", 9, 8, 1, 4, 16},
    [CC_GOOD_STONE] = {"Stone", 7, 4, 1, 4, 8},
    [CC_GOOD_PAPER] = {"Paper", 12, 8, 1, 4, 16},
    [CC_GOOD_ROTTEN_MEAT] = {"Rotten Meat", 1, 6, 1, 4, 12},
    [CC_GOOD_ROTTEN_GRAIN] = {"Rotten Grain", 1, 10, 1, 4, 20}
};

int32_t CcGoodCountForSchema(uint32_t schema_version)
{
    if (schema_version >= 33U) return CC_GOOD_COUNT;
    if (schema_version >= 27U) return 11;
    if (schema_version >= 9U) return CC_LEGACY_GOOD_COUNT;
    return 3;
}

bool CcGoodIsValid(CcGood good)
{
    return good >= CC_GOOD_BREAD && good < CC_GOOD_COUNT;
}

const CcGoodDefinition *CcGoodDefinitionFor(CcGood good)
{
    return CcGoodIsValid(good) ? &GOOD_DEFINITIONS[good] : NULL;
}

const char *CcGoodName(CcGood good)
{
    const CcGoodDefinition *definition = CcGoodDefinitionFor(good);
    return definition != NULL ? definition->name : "Unknown";
}

int32_t CcGoodNutritionValue(CcGood good, CcNutritionPurpose purpose)
{
    if (purpose == CC_NUTRITION_ANIMAL) {
        return good == CC_GOOD_WHEAT ? CC_NUTRITION_PER_RATION : 0;
    }
    if (good == CC_GOOD_BREAD || good == CC_GOOD_MEAT) {
        return CC_NUTRITION_PER_RATION;
    }
    if (purpose == CC_NUTRITION_CIVILIAN && good == CC_GOOD_WHEAT) {
        return 1;
    }
    return 0;
}

int32_t CcNutritionAvailable(const int32_t goods[CC_GOOD_COUNT],
                             CcNutritionPurpose purpose)
{
    if (goods == NULL) return 0;
    int64_t nutrition = 0;
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        int32_t quantity = MaximumI32(0, goods[good]);
        nutrition += (int64_t)quantity *
                     CcGoodNutritionValue((CcGood)good, purpose);
    }
    return nutrition > INT32_MAX ? INT32_MAX : (int32_t)nutrition;
}

int32_t CcNutritionConsume(int32_t goods[CC_GOOD_COUNT],
                           CcNutritionPurpose purpose,
                           int32_t requested_nutrition)
{
    if (goods == NULL || requested_nutrition <= 0) return 0;
    static const CcGood order[] = {
        CC_GOOD_BREAD, CC_GOOD_MEAT, CC_GOOD_WHEAT
    };
    int32_t delivered = 0;
    for (size_t i = 0;
         i < sizeof(order) / sizeof(order[0]) &&
         delivered < requested_nutrition; ++i) {
        CcGood good = order[i];
        int32_t value = CcGoodNutritionValue(good, purpose);
        if (value <= 0 || goods[good] <= 0) continue;
        int32_t remaining = requested_nutrition - delivered;
        int32_t needed_units = (remaining + value - 1) / value;
        int32_t used = MinimumI32(goods[good], needed_units);
        goods[good] -= used;
        delivered += used * value;
    }
    return MinimumI32(delivered, requested_nutrition);
}

static CcGood PreferredNutritionGood(
    const int32_t goods[CC_GOOD_COUNT], CcNutritionPurpose purpose)
{
    static const CcGood order[] = {
        CC_GOOD_BREAD, CC_GOOD_MEAT, CC_GOOD_WHEAT
    };
    CcGood best = CC_GOOD_BREAD;
    int64_t best_nutrition = -1;
    for (size_t i = 0; i < sizeof(order) / sizeof(order[0]); ++i) {
        CcGood good = order[i];
        int64_t nutrition = (int64_t)goods[good] *
            CcGoodNutritionValue(good, purpose);
        if (nutrition > best_nutrition) {
            best = good;
            best_nutrition = nutrition;
        }
    }
    return best;
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
        case CC_SERVICE_BAKERY: return "Bakery";
        case CC_SERVICE_MILL: return "Mill";
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

const char *CcRoyalCarriageModeName(CcRoyalCarriageMode mode)
{
    switch (mode) {
        case CC_ROYAL_CARRIAGE_IDLE: return "Idle";
        case CC_ROYAL_CARRIAGE_REPOSITIONING: return "Seeking cargo";
        case CC_ROYAL_CARRIAGE_DELIVERING: return "Delivering";
        case CC_ROYAL_CARRIAGE_BLOCKED: return "Blocked";
        case CC_ROYAL_CARRIAGE_WAITING_CAPACITY:
            return "Waiting for road space";
    }
    return "Unknown";
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
        case CC_SETTLEMENT_TOWN: return 7;
        case CC_SETTLEMENT_CITY: return 9;
        case CC_SETTLEMENT_CAPITAL_SIZE: return 13;
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
        case CC_EVENT_GOBLIN_TUNNEL_TRAVERSED: return "MOUNTAIN TUNNEL";
        case CC_EVENT_DUNGEON_EXPEDITION_BEGAN: return "UNDERROAD DELVE";
        case CC_EVENT_DUNGEON_ROOM_ENTERED: return "UNDERROAD MAPPED";
        case CC_EVENT_DUNGEON_ENCOUNTER: return "UNDERROAD ENCOUNTER";
        case CC_EVENT_DUNGEON_SHORTCUT_OPENED: return "SHORTCUT OPENED";
        case CC_EVENT_DUNGEON_LOOT: return "UNDERROAD RECOVERY";
        case CC_EVENT_DUNGEON_THRESHOLD_REACHED: return "HOARD THRESHOLD";
        case CC_EVENT_DUNGEON_EXPEDITION_ENDED: return "UNDERROAD RETURN";
        case CC_EVENT_RELATIONSHIP_HISTORY: return "SHARED HISTORY";
        case CC_EVENT_RUMOR_SHARED: return "RUMOR";
        case CC_EVENT_FACT_REVEALED: return "DISCOVERY";
        case CC_EVENT_RELATIONSHIP_CHANGED: return "RELATIONSHIP";
        case CC_EVENT_FRONT_CREATED: return "STORY FRONT";
        case CC_EVENT_FRONT_RESOLVED: return "FRONT RESOLVED";
        case CC_EVENT_FRONT_FAILED: return "FRONT FAILED";
        case CC_EVENT_QUEST_PROGRESS: return "QUEST PROGRESS";
        case CC_EVENT_LORE_RECORDED: return "LORE RECORDED";
        case CC_EVENT_LORE_LOST: return "LORE LOST";
        case CC_EVENT_JOURNEY_BREAK: return "ROAD BREAK";
        case CC_EVENT_JOURNEY_CAMP: return "CAMP";
        case CC_EVENT_ROAD_HOUSE_LODGING: return "ROAD HOUSE";
        case CC_EVENT_CHARACTER_BORN: return "BIRTH";
        case CC_EVENT_CHARACTER_DIED: return "DEATH";
        case CC_EVENT_PARTY_WIPED: return "TWENTY YEARS LATER";
        case CC_EVENT_WOODLOT_HARVEST: return "WOODLOT";
        case CC_EVENT_BAKERY_PRODUCTION: return "BAKERY";
        case CC_EVENT_QUARRY_OUTPUT: return "QUARRY";
        case CC_EVENT_MASONRY_REPAIR: return "MASONRY";
        case CC_EVENT_SHEEP_BRED: return "LAMBING";
        case CC_EVENT_SHEEP_SHEARED: return "SHEARING";
        case CC_EVENT_SHEEP_SLAUGHTERED: return "FLOCK CULL";
        case CC_EVENT_PAPER_MILLED: return "PAPER MILL";
        case CC_EVENT_KING_ANOINTED: return "CORONATION";
        case CC_EVENT_PRETENDER_CRISIS: return "PRETENDER";
        case CC_EVENT_MONASTIC_SUCCESSION: return "NEW ABBOT";
        case CC_EVENT_ROYAL_SUCCESSION: return "NEW RULER";
        case CC_EVENT_DRAGON_PATRON_NAMED: return "DRAGON VOW";
        case CC_EVENT_DRAGON_TERRITORY_LOST: return "CROWN BROKEN";
        case CC_EVENT_ROYAL_CARRIAGE_BLOCKED: return "BORDER BLOCK";
        case CC_EVENT_ROYAL_CARRIAGE_REROUTED: return "CARRIAGE ROUTE";
    }
    return "EVENT";
}

const char *CcRelationshipHistoryName(CcRelationshipHistory history)
{
    switch (history) {
        case CC_RELATIONSHIP_HISTORY_NONE: return "acquaintances";
        case CC_RELATIONSHIP_HISTORY_OLD_FRIENDS: return "old friends";
        case CC_RELATIONSHIP_HISTORY_FORMER_PARTNERS:
            return "former partners";
        case CC_RELATIONSHIP_HISTORY_PROFESSIONAL_RIVALS:
            return "professional rivals";
        case CC_RELATIONSHIP_HISTORY_COWORKERS: return "coworkers";
    }
    return "acquaintances";
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

const char *CcQuestObjectiveKindName(CcQuestObjectiveKind kind)
{
    switch (kind) {
        case CC_QUEST_OBJECTIVE_DELIVER_GOODS: return "deliver goods";
        case CC_QUEST_OBJECTIVE_RESTORE_ROUTE: return "restore route";
        case CC_QUEST_OBJECTIVE_SETTLE_DUNGEON: return "settle dungeon";
        case CC_QUEST_OBJECTIVE_ESCORT_COURIER: return "escort courier";
    }
    return "unknown objective";
}

const char *CcQuestEndReasonName(CcQuestEndReason reason)
{
    switch (reason) {
        case CC_QUEST_END_NONE: return "open";
        case CC_QUEST_END_COMPLETED: return "completed";
        case CC_QUEST_END_EXPIRED: return "expired";
        case CC_QUEST_END_REFUSED: return "refused";
        case CC_QUEST_END_INVALIDATED: return "invalidated";
        case CC_QUEST_END_COURIER_LOST: return "courier lost";
    }
    return "unknown ending";
}

const char *CcFrontKindName(CcFrontKind kind)
{
    switch (kind) {
        case CC_FRONT_SUPPLY_CRISIS: return "Supply crisis";
        case CC_FRONT_MONSTER_PRESSURE: return "Monster pressure";
        case CC_FRONT_COURIER_DISPATCH: return "Courier dispatch";
    }
    return "Unknown front";
}

const char *CcFrontOutcomeName(CcFrontOutcome outcome)
{
    switch (outcome) {
        case CC_FRONT_OUTCOME_NONE: return "Unanswered";
        case CC_FRONT_OUTCOME_RELIEF_DELIVERED: return "Relief delivered";
        case CC_FRONT_OUTCOME_ROUTE_RESTORED: return "Route restored";
        case CC_FRONT_OUTCOME_NIGHT_ROAD: return "Night road supplied";
        case CC_FRONT_OUTCOME_MONSTER_SETTLED: return "Depths settled";
        case CC_FRONT_OUTCOME_DISPATCH_DELIVERED: return "Dispatch delivered";
        case CC_FRONT_OUTCOME_PRESSURE_WON: return "Pressure won";
    }
    return "Unknown outcome";
}

const char *CcFrontStageName(CcFrontStage stage)
{
    switch (stage) {
        case CC_FRONT_STAGE_RUMBLING: return "rumbling";
        case CC_FRONT_STAGE_PRESSING: return "pressing";
        case CC_FRONT_STAGE_BREAKING: return "breaking";
        case CC_FRONT_STAGE_CLOSED: return "closed";
    }
    return "unknown";
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

const CcRoadSite *CcSimRoadSite(const CcSim *sim, CcId id)
{
    if (sim == NULL || CcIdKind(id) != CC_ENTITY_ROAD_SITE) return NULL;
    for (int32_t i = 0; i < sim->road_site_count; ++i) {
        if (sim->road_sites[i].id == id) return &sim->road_sites[i];
    }
    return NULL;
}

const CcRoadSite *CcSimRoadSiteAt(const CcSim *sim, int32_t index)
{
    if (sim == NULL || index < 0 || index >= sim->road_site_count) return NULL;
    return &sim->road_sites[index];
}

const CcRoadSite *CcSimRoadHouseSite(const CcSim *sim, CcId route_id)
{
    if (sim == NULL) return NULL;
    for (int32_t i = 0; i < sim->road_site_count; ++i) {
        const CcRoadSite *site = &sim->road_sites[i];
        if (site->route_id == route_id &&
            site->kind == CC_ROAD_SITE_ROAD_HOUSE) return site;
    }
    return NULL;
}

const char *CcRoadSiteKindName(CcRoadSiteKind kind)
{
    switch (kind) {
        case CC_ROAD_SITE_FARM: return "farm";
        case CC_ROAD_SITE_PASTURE: return "pasture";
        case CC_ROAD_SITE_WOODLOT: return "woodlot";
        case CC_ROAD_SITE_QUARRY: return "quarry";
        case CC_ROAD_SITE_MINE: return "mine";
        case CC_ROAD_SITE_MILL: return "mill";
        case CC_ROAD_SITE_BAKERY: return "bakery";
        case CC_ROAD_SITE_SMITHY: return "smithy";
        case CC_ROAD_SITE_ROAD_HOUSE: return "road house";
        case CC_ROAD_SITE_ROAD_CREW: return "road crew";
        case CC_ROAD_SITE_KIND_COUNT: break;
    }
    return "working site";
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

static uint32_t NextDungeonRandom(CcDungeon *dungeon)
{
    uint32_t value = dungeon->encounter_random_state;
    value ^= value << 13U;
    value ^= value >> 17U;
    value ^= value << 5U;
    dungeon->encounter_random_state = value == 0U ?
        UINT32_C(0x6d2b79f5) : value;
    return dungeon->encounter_random_state;
}

static void InitDungeonRoom(CcDungeon *dungeon, int32_t slot,
                            const char *name, CcDungeonRoomKind kind,
                            int32_t depth, int32_t map_x, int32_t map_y,
                            uint32_t flags, CcGood loot_good,
                            int32_t loot_quantity)
{
    if (dungeon == NULL || slot < 0 || slot >= CC_MAX_DUNGEON_ROOMS) return;
    CcDungeonRoom *room = &dungeon->rooms[slot];
    *room = (CcDungeonRoom){0};
    (void)snprintf(room->name, sizeof(room->name), "%s", name);
    room->kind = kind;
    room->depth = depth;
    room->map_x = map_x;
    room->map_y = map_y;
    room->flags = flags;
    room->loot_good = loot_good;
    room->loot_quantity = loot_quantity;
}

static void AddDungeonLink(CcDungeon *dungeon, int32_t from_room,
                           int32_t to_room, CcDungeonLinkKind kind)
{
    if (dungeon == NULL || dungeon->link_count >= CC_MAX_DUNGEON_LINKS) return;
    CcDungeonLink *link = &dungeon->links[dungeon->link_count++];
    *link = (CcDungeonLink){
        .from_room = from_room,
        .to_room = to_room,
        .kind = kind,
        .flags = kind == CC_DUNGEON_LINK_SHORTCUT ? 0U :
                 CC_DUNGEON_LINK_OPEN
    };
}

static void GenerateUnderroad(CcSim *sim, CcDungeon *dungeon)
{
    if (sim == NULL || dungeon == NULL) return;
    memset(dungeon->rooms, 0, sizeof(dungeon->rooms));
    memset(dungeon->links, 0, sizeof(dungeon->links));
    dungeon->layout_seed = sim->world_seed ^
        (uint32_t)(dungeon->id & UINT64_C(0xffffffff)) ^
        UINT32_C(0x71a7e5ed);
    if (dungeon->layout_seed == 0U) dungeon->layout_seed = UINT32_C(0x51ed270b);
    dungeon->encounter_random_state = dungeon->layout_seed;
    dungeon->room_count = CC_MAX_DUNGEON_ROOMS;
    dungeon->link_count = 0;

    InitDungeonRoom(dungeon, 0, "Mine Mouth", CC_DUNGEON_ROOM_MINE_MOUTH,
                    0, 0, 0, CC_DUNGEON_ROOM_SAFE, CC_GOOD_FOOD, 0);
    InitDungeonRoom(dungeon, 1, "Lamp Hall", CC_DUNGEON_ROOM_RAIL,
                    0, 1, 0, 0U, CC_GOOD_FOOD, 0);
    InitDungeonRoom(dungeon, 2, "Broken Weighhouse", CC_DUNGEON_ROOM_WORKSHOP,
                    0, 2, -1, CC_DUNGEON_ROOM_HAZARD,
                    CC_GOOD_TOOLS, 1);
    InitDungeonRoom(dungeon, 3, "Sump Gallery", CC_DUNGEON_ROOM_FLOODWAY,
                    0, 2, 1, CC_DUNGEON_ROOM_STONEBACK,
                    CC_GOOD_IRON, 0);
    InitDungeonRoom(dungeon, 4, "Chain Bridge", CC_DUNGEON_ROOM_BRIDGE,
                    1, 3, 0, CC_DUNGEON_ROOM_HAZARD,
                    CC_GOOD_IRON, 0);
    InitDungeonRoom(dungeon, 5, "Foreman's Lockroom",
                    CC_DUNGEON_ROOM_WORKSHOP, 1, 3, -2,
                    CC_DUNGEON_ROOM_SAFE, CC_GOOD_TOOLS, 2);
    InitDungeonRoom(dungeon, 6, "Flooded Turntable",
                    CC_DUNGEON_ROOM_FLOODWAY, 1, 4, 1,
                    CC_DUNGEON_ROOM_STONEBACK, CC_GOOD_IRON, 0);
    InitDungeonRoom(dungeon, 7, "Lost Cargo Spur", CC_DUNGEON_ROOM_RAIL,
                    1, 4, 3, 0U, CC_GOOD_IRON, 4);
    InitDungeonRoom(dungeon, 8, "Bell Shaft", CC_DUNGEON_ROOM_SHAFT,
                    1, 5, 0, CC_DUNGEON_ROOM_DRAGON_SIGN,
                    CC_GOOD_FOOD, 0);
    InitDungeonRoom(dungeon, 9, "Archive Gate", CC_DUNGEON_ROOM_ARCHIVE,
                    2, 6, 0, CC_DUNGEON_ROOM_GOBLIN,
                    CC_GOOD_FOOD, 0);
    InitDungeonRoom(dungeon, 10, "Hall of Ash Clerks",
                    CC_DUNGEON_ROOM_ARCHIVE, 2, 7, -1,
                    CC_DUNGEON_ROOM_GOBLIN, CC_GOOD_FOOD, 0);
    InitDungeonRoom(dungeon, 11, "Cinder Market",
                    CC_DUNGEON_ROOM_MARKET, 2, 7, 1,
                    CC_DUNGEON_ROOM_SAFE | CC_DUNGEON_ROOM_GOBLIN,
                    CC_GOOD_FOOD, 2);
    InitDungeonRoom(dungeon, 12, "Red Cap Barracks",
                    CC_DUNGEON_ROOM_BARRACKS, 2, 8, -2,
                    CC_DUNGEON_ROOM_GOBLIN | CC_DUNGEON_ROOM_HAZARD,
                    CC_GOOD_WEAPONS, 1);
    InitDungeonRoom(dungeon, 13, "Ledger Vault",
                    CC_DUNGEON_ROOM_VAULT, 2, 8, 0,
                    CC_DUNGEON_ROOM_GOBLIN, CC_GOOD_GOLD, 2);
    InitDungeonRoom(dungeon, 14, "Tribute Lift",
                    CC_DUNGEON_ROOM_SHAFT, 2, 8, 2,
                    CC_DUNGEON_ROOM_GOBLIN | CC_DUNGEON_ROOM_DRAGON_SIGN,
                    CC_GOOD_FOOD, 0);
    InitDungeonRoom(dungeon, 15, "Blind Aqueduct",
                    CC_DUNGEON_ROOM_FLOODWAY, 3, 9, 3,
                    CC_DUNGEON_ROOM_HAZARD, CC_GOOD_GEMS, 0);
    InitDungeonRoom(dungeon, 16, "The Tithe Road",
                    CC_DUNGEON_ROOM_RAIL, 3, 9, 1,
                    CC_DUNGEON_ROOM_GOBLIN | CC_DUNGEON_ROOM_DRAGON_SIGN,
                    CC_GOOD_FOOD, 0);
    InitDungeonRoom(dungeon, 17, "Furnace Shrine",
                    CC_DUNGEON_ROOM_SHRINE, 3, 10, -1,
                    CC_DUNGEON_ROOM_HAZARD | CC_DUNGEON_ROOM_DRAGON_SIGN,
                    CC_GOOD_GOLD, 0);
    InitDungeonRoom(dungeon, 18, "Porters' Grave",
                    CC_DUNGEON_ROOM_VAULT, 3, 10, 2,
                    CC_DUNGEON_ROOM_HAZARD, CC_GOOD_GEMS, 1);
    InitDungeonRoom(dungeon, 19, "Hoard Threshold",
                    CC_DUNGEON_ROOM_THRESHOLD, 4, 11, 0,
                    CC_DUNGEON_ROOM_OBJECTIVE | CC_DUNGEON_ROOM_DRAGON_SIGN,
                    CC_GOOD_FOOD, 0);
    InitDungeonRoom(dungeon, 20, "Old Smuggler Cut",
                    CC_DUNGEON_ROOM_RAIL, 2, 6, 3,
                    CC_DUNGEON_ROOM_SMUGGLER,
                    CC_GOOD_GOLD, 1);
    InitDungeonRoom(dungeon, 21, "Stoneback Nursery",
                    CC_DUNGEON_ROOM_FLOODWAY, 1, 5, 3,
                    CC_DUNGEON_ROOM_STONEBACK, CC_GOOD_IRON, 0);
    InitDungeonRoom(dungeon, 22, "Collapsed Freight Lift",
                    CC_DUNGEON_ROOM_SHAFT, 1, 2, 3,
                    CC_DUNGEON_ROOM_HAZARD, CC_GOOD_TOOLS, 0);
    InitDungeonRoom(dungeon, 23, "King's Survey Room",
                    CC_DUNGEON_ROOM_ARCHIVE, 1, 4, -2,
                    CC_DUNGEON_ROOM_SAFE, CC_GOOD_IRON, 3);

    static const int32_t passages[][2] = {
        {0, 1}, {1, 2}, {1, 3}, {2, 4}, {2, 5}, {3, 4}, {3, 6},
        {4, 8}, {5, 23}, {23, 8}, {6, 7}, {6, 21}, {21, 22},
        {8, 9}, {9, 10}, {9, 11}, {10, 12}, {10, 13}, {11, 13},
        {11, 14}, {12, 17}, {13, 16}, {14, 15}, {14, 20},
        {15, 16}, {16, 17}, {16, 18}, {17, 19}, {18, 19}
    };
    for (size_t i = 0; i < sizeof(passages) / sizeof(passages[0]); ++i) {
        AddDungeonLink(dungeon, passages[i][0], passages[i][1],
                       CC_DUNGEON_LINK_PASSAGE);
    }
    AddDungeonLink(dungeon, 7, 20, CC_DUNGEON_LINK_SECRET);
    AddDungeonLink(dungeon, 20, 11, CC_DUNGEON_LINK_SECRET);
    AddDungeonLink(dungeon, 22, 1, CC_DUNGEON_LINK_SHORTCUT);
    AddDungeonLink(dungeon, 15, 18, CC_DUNGEON_LINK_SECRET);

    static const int32_t optional_links[][2] = {
        {5, 6}, {7, 22}, {12, 13}, {10, 17}, {14, 18}, {20, 21}
    };
    uint32_t choices = NextDungeonRandom(dungeon);
    for (int32_t i = 0; i < 2; ++i) {
        int32_t option = (int32_t)((choices >> (uint32_t)(i * 5)) % 3U) +
                         i * 3;
        AddDungeonLink(dungeon, optional_links[option][0],
                       optional_links[option][1],
                       (choices & (UINT32_C(1) << (uint32_t)(20 + i))) != 0U ?
                           CC_DUNGEON_LINK_SECRET :
                           CC_DUNGEON_LINK_PASSAGE);
    }
    dungeon->rooms[0].state_flags |= CC_DUNGEON_ROOM_DISCOVERED;
}

void CcSimInitializeUnderroad(CcSim *sim)
{
    if (sim == NULL) return;
    for (int32_t i = 0; i < sim->dungeon_count; ++i) {
        CcDungeon *dungeon = &sim->dungeons[i];
        if (dungeon->room_count == 0) GenerateUnderroad(sim, dungeon);
    }
    if (!sim->dungeon_expedition.active) {
        sim->dungeon_expedition = (CcDungeonExpedition){0};
        sim->dungeon_expedition.current_room = -1;
        sim->dungeon_expedition.encounter_room = -1;
    }
}

const CcDungeon *CcSimDungeon(const CcSim *sim, CcId id)
{
    if (sim == NULL || CcIdKind(id) != CC_ENTITY_DUNGEON) return NULL;
    for (int32_t i = 0; i < sim->dungeon_count; ++i) {
        if (sim->dungeons[i].id == id) return &sim->dungeons[i];
    }
    return NULL;
}

static CcDungeon *DungeonMutable(CcSim *sim, CcId id)
{
    return (CcDungeon *)CcSimDungeon((const CcSim *)sim, id);
}

static bool DungeonLinkAllowsTravel(const CcDungeonLink *link,
                                    int32_t from_room)
{
    if (link == NULL || (link->flags & CC_DUNGEON_LINK_OPEN) == 0U) {
        return false;
    }
    if (link->kind == CC_DUNGEON_LINK_SECRET &&
        (link->flags & CC_DUNGEON_LINK_DISCOVERED) == 0U) return false;
    if (link->kind == CC_DUNGEON_LINK_DROP) return link->from_room == from_room;
    return link->from_room == from_room || link->to_room == from_room;
}

static int32_t DungeonLinkOtherRoom(const CcDungeonLink *link,
                                    int32_t from_room)
{
    if (link == NULL) return -1;
    if (link->from_room == from_room) return link->to_room;
    if (link->to_room == from_room && link->kind != CC_DUNGEON_LINK_DROP) {
        return link->from_room;
    }
    return -1;
}

const CcDungeonRoom *CcSimDungeonCurrentRoom(const CcSim *sim)
{
    if (sim == NULL || !sim->dungeon_expedition.active) return NULL;
    const CcDungeon *dungeon = CcSimDungeon(
        sim, sim->dungeon_expedition.dungeon_id);
    int32_t room = sim->dungeon_expedition.current_room;
    return dungeon != NULL && room >= 0 && room < dungeon->room_count ?
        &dungeon->rooms[room] : NULL;
}

int32_t CcSimDungeonVisibleExitCount(const CcSim *sim)
{
    if (sim == NULL || !sim->dungeon_expedition.active) return 0;
    const CcDungeon *dungeon = CcSimDungeon(
        sim, sim->dungeon_expedition.dungeon_id);
    if (dungeon == NULL) return 0;
    int32_t count = 0;
    for (int32_t i = 0; i < dungeon->link_count; ++i) {
        if (DungeonLinkAllowsTravel(&dungeon->links[i],
                                    sim->dungeon_expedition.current_room)) {
            count += 1;
        }
    }
    return count;
}

int32_t CcSimDungeonVisibleExitAt(const CcSim *sim, int32_t ordinal)
{
    if (sim == NULL || ordinal < 0 || !sim->dungeon_expedition.active) {
        return -1;
    }
    const CcDungeon *dungeon = CcSimDungeon(
        sim, sim->dungeon_expedition.dungeon_id);
    if (dungeon == NULL) return -1;
    int32_t found = 0;
    for (int32_t i = 0; i < dungeon->link_count; ++i) {
        const CcDungeonLink *link = &dungeon->links[i];
        if (!DungeonLinkAllowsTravel(
                link, sim->dungeon_expedition.current_room)) continue;
        if (found++ == ordinal) {
            return DungeonLinkOtherRoom(
                link, sim->dungeon_expedition.current_room);
        }
    }
    return -1;
}

int32_t CcSimDungeonOpenableShortcut(const CcSim *sim)
{
    if (sim == NULL || !sim->dungeon_expedition.active) return -1;
    const CcDungeon *dungeon = CcSimDungeon(
        sim, sim->dungeon_expedition.dungeon_id);
    if (dungeon == NULL) return -1;
    int32_t room = sim->dungeon_expedition.current_room;
    for (int32_t i = 0; i < dungeon->link_count; ++i) {
        const CcDungeonLink *link = &dungeon->links[i];
        if (link->kind == CC_DUNGEON_LINK_SHORTCUT &&
            (link->flags & CC_DUNGEON_LINK_DISCOVERED) != 0U &&
            (link->flags & CC_DUNGEON_LINK_OPEN) == 0U &&
            (link->from_room == room || link->to_room == room)) return i;
    }
    return -1;
}

const char *CcDungeonRoomKindName(CcDungeonRoomKind kind)
{
    switch (kind) {
        case CC_DUNGEON_ROOM_MINE_MOUTH: return "mine mouth";
        case CC_DUNGEON_ROOM_RAIL: return "old freight road";
        case CC_DUNGEON_ROOM_FLOODWAY: return "floodway";
        case CC_DUNGEON_ROOM_WORKSHOP: return "workshop";
        case CC_DUNGEON_ROOM_BRIDGE: return "bridge";
        case CC_DUNGEON_ROOM_SHAFT: return "shaft";
        case CC_DUNGEON_ROOM_ARCHIVE: return "archive";
        case CC_DUNGEON_ROOM_MARKET: return "goblin market";
        case CC_DUNGEON_ROOM_BARRACKS: return "barracks";
        case CC_DUNGEON_ROOM_SHRINE: return "furnace shrine";
        case CC_DUNGEON_ROOM_VAULT: return "vault";
        case CC_DUNGEON_ROOM_THRESHOLD: return "hoard threshold";
    }
    return "unknown chamber";
}

const char *CcDungeonEncounterName(CcDungeonEncounterKind kind)
{
    switch (kind) {
        case CC_DUNGEON_ENCOUNTER_STONEBACKS: return "stoneback procession";
        case CC_DUNGEON_ENCOUNTER_TITHE_KEEPERS: return "Tithe Keeper patrol";
        case CC_DUNGEON_ENCOUNTER_GOBLIN_DESERTERS: return "goblin deserters";
        case CC_DUNGEON_ENCOUNTER_MONSTERS: return "deep-dwelling monsters";
        case CC_DUNGEON_ENCOUNTER_SMUGGLERS: return "underroad smugglers";
        case CC_DUNGEON_ENCOUNTER_NONE: break;
    }
    return "no encounter";
}

const char *CcDungeonReactionName(int32_t reaction)
{
    if (reaction <= 3) return "immediately hostile";
    if (reaction <= 5) return "threatening";
    if (reaction <= 8) return "uncertain";
    if (reaction <= 10) return "willing to talk";
    return "helpful for now";
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

const CcRoyalCarriage *CcSimRoyalCarriage(const CcSim *sim,
                                          CcId kingdom_id)
{
    if (sim == NULL) return NULL;
    for (int32_t i = 0; i < sim->royal_carriage_count; ++i) {
        if (sim->royal_carriages[i].kingdom_id == kingdom_id) {
            return &sim->royal_carriages[i];
        }
    }
    return NULL;
}

static int32_t FactionSupportFor(const CcSim *sim, CcId kingdom_id,
                                 CcFactionKind kind)
{
    if (sim == NULL) return 0;
    for (int32_t i = 0; i < sim->faction_count; ++i) {
        const CcFaction *faction = &sim->factions[i];
        if (faction->kingdom_id == kingdom_id && faction->kind == kind) {
            return faction->support;
        }
    }
    return 0;
}

static void RaiseRoyalTradeSupport(CcSim *sim,
                                  const CcRoyalCarriage *carriage)
{
    if (sim == NULL || carriage == NULL) return;
    const CcRoute *route = CcSimRoute(sim, carriage->route_id);
    const CcSettlement *from = route != NULL ?
        CcSimSettlement(sim, route->from_id) : NULL;
    const CcSettlement *to = route != NULL ?
        CcSimSettlement(sim, route->to_id) : NULL;
    if (from == NULL || to == NULL) return;
    CcId hosts[2] = {from->kingdom_id, to->kingdom_id};
    for (int32_t host = 0; host < 2; ++host) {
        CcId host_id = hosts[host];
        if (host_id == carriage->kingdom_id ||
            (host == 1 && host_id == hosts[0]) ||
            CcSimKingdomsAtWar(sim, carriage->kingdom_id, host_id)) continue;
        for (int32_t faction = 0; faction < sim->faction_count; ++faction) {
            CcFaction *item = &sim->factions[faction];
            if (item->kingdom_id == host_id &&
                (item->kind == CC_FACTION_CROWN ||
                 item->kind == CC_FACTION_GUILD)) {
                item->support = ClampI32(item->support + 3, 0, 100);
            }
        }
    }
}

static bool RoyalCarriageMayEnter(const CcSim *sim,
                                  CcId carriage_kingdom_id,
                                  CcId host_kingdom_id)
{
    if (sim == NULL || KingdomSlotById(sim, carriage_kingdom_id) < 0 ||
        KingdomSlotById(sim, host_kingdom_id) < 0) return false;
    if (carriage_kingdom_id == host_kingdom_id ||
        CcSimKingdomsAllied(sim, carriage_kingdom_id, host_kingdom_id)) {
        return true;
    }
    if (CcSimKingdomsAtWar(sim, carriage_kingdom_id, host_kingdom_id)) {
        return false;
    }
    return FactionSupportFor(sim, host_kingdom_id, CC_FACTION_CROWN) >=
               CC_ROYAL_TRADE_SUPPORT_FLOOR &&
           FactionSupportFor(sim, host_kingdom_id, CC_FACTION_GUILD) >=
               CC_ROYAL_TRADE_SUPPORT_FLOOR;
}

static bool RoyalRouteIsOfficial(const CcSim *sim, const CcRoute *route)
{
    if (sim == NULL || route == NULL) return false;
    return !(route->smuggler_route &&
             CcSimRouteCrossesKingdomBorder(sim, route->id)) &&
           !CcSimRouteCrossesWarBorder(sim, route->id);
}

static bool RoyalRouteCanReopen(const CcSim *sim, const CcRoute *route)
{
    if (sim == NULL || route == NULL) return false;
    return !(route->smuggler_route &&
             CcSimRouteCrossesKingdomBorder(sim, route->id));
}

bool CcSimRoyalCarriageCanUseRoute(const CcSim *sim,
                                   CcId carriage_kingdom_id,
                                   CcId route_id)
{
    const CcRoute *route = CcSimRoute(sim, route_id);
    const CcSettlement *from = route != NULL ?
        CcSimSettlement(sim, route->from_id) : NULL;
    const CcSettlement *to = route != NULL ?
        CcSimSettlement(sim, route->to_id) : NULL;
    if (route == NULL || from == NULL || to == NULL) return false;
    if (!RoyalRouteIsOfficial(sim, route)) return false;
    return RoyalCarriageMayEnter(sim, carriage_kingdom_id,
                                 from->kingdom_id) &&
           RoyalCarriageMayEnter(sim, carriage_kingdom_id,
                                 to->kingdom_id);
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

bool CcSimRouteCrossesKingdomBorder(const CcSim *sim, CcId route_id)
{
    const CcRoute *route = CcSimRoute(sim, route_id);
    const CcSettlement *from = route != NULL ?
        CcSimSettlement(sim, route->from_id) : NULL;
    const CcSettlement *to = route != NULL ?
        CcSimSettlement(sim, route->to_id) : NULL;
    return from != NULL && to != NULL &&
        from->kingdom_id != to->kingdom_id;
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

static bool IsWinter(const CcSim *sim)
{
    return sim != NULL && (sim->current_day / 7) % 52 >= 39;
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
    if (good == CC_GOOD_WOOL && sim->schema_version >= 32U) {
        return 1 + burden / 50;
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
    if (good == CC_GOOD_WOOL && sim->schema_version >= 32U) {
        return 1 + burden / 50;
    }
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

static int32_t CivilianWoolUse(const CcSim *sim,
                               const CcSettlement *place)
{
    if (sim == NULL || sim->schema_version < 32U || !IsWinter(sim) ||
        place == NULL || place->population <= 0) return 0;
    return MaximumI32(1, (place->population + 1199) / 1200);
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

static int32_t NutritionRations(const int32_t goods[CC_GOOD_COUNT],
                                CcNutritionPurpose purpose)
{
    return CcNutritionAvailable(goods, purpose) / CC_NUTRITION_PER_RATION;
}

static int32_t IncomingNutrition(const CcSim *sim, CcId settlement_id,
                                 CcNutritionPurpose purpose)
{
    int64_t nutrition = 0;
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        nutrition += (int64_t)CcSimIncomingGood(
            sim, settlement_id, (CcGood)good) *
            CcGoodNutritionValue((CcGood)good, purpose);
    }
    return nutrition > INT32_MAX ? INT32_MAX : (int32_t)nutrition;
}

static void RefreshSettlementGoodPrice(const CcSim *sim,
                                       CcSettlement *settlement,
                                       CcGood good)
{
    if (sim == NULL || settlement == NULL ||
        good < 0 || good >= CC_GOOD_COUNT) return;
    int32_t target = EffectiveReserveTarget(sim, settlement, good);
    int32_t incoming = CcSimIncomingGood(sim, settlement->id, good);
    int32_t expected_stock = settlement->stock[good] + incoming / 2;
    int32_t shortage = target > 0 ?
        (target - expected_stock) * 100 / target : 0;
    int32_t pressure = ClampI32(shortage, -35, 220);
    const CcGoodDefinition *definition = CcGoodDefinitionFor(good);
    settlement->price[good] = MinimumI32(
        99, definition->base_price * (100 + pressure) / 100);
    if (settlement->price[good] < 1) settlement->price[good] = 1;
}

static int32_t NutritionStorageCapacity(const CcSim *sim,
                                        const CcSettlement *place,
                                        CcGood good)
{
    int32_t weekly_use = WeeklyFoodUse(sim, place);
    bool granary = CcSettlementHasService(place, CC_SERVICE_GRANARY);
    if (good == CC_GOOD_BREAD) return FoodStorageCapacity(sim, place);
    if (good == CC_GOOD_WHEAT) {
        return weekly_use * (granary ? 64 : 24);
    }
    if (good == CC_GOOD_MEAT) return weekly_use * (granary ? 8 : 2);
    return CC_SIM_MAX_UNITS;
}

static int32_t SpoilStoredNutrition(const CcSim *sim, CcSettlement *place)
{
    static const CcGood goods[] = {
        CC_GOOD_BREAD, CC_GOOD_WHEAT, CC_GOOD_MEAT
    };
    int32_t total_spoiled = 0;
    for (size_t i = 0; i < sizeof(goods) / sizeof(goods[0]); ++i) {
        CcGood good = goods[i];
        int32_t divisor = good == CC_GOOD_MEAT ? 20 :
                          good == CC_GOOD_WHEAT ? 400 : 100;
        int32_t stored = place->stock[good];
        int32_t spoiled = stored / divisor;
        stored -= spoiled;
        int32_t capacity = NutritionStorageCapacity(sim, place, good);
        if (stored > capacity) {
            spoiled += stored - capacity;
            stored = capacity;
        }
        place->stock[good] = stored;
        total_spoiled += spoiled;
    }
    return total_spoiled;
}

static int32_t BakeryCapacity(const CcSettlement *place)
{
    if (!CcSettlementHasService(place, CC_SERVICE_BAKERY)) return 0;
    return MaximumI32(0, place->production[CC_GOOD_BREAD]);
}

static int32_t RunBakery(CcSim *sim, CcSettlement *place,
                         CcId scriptorium_id)
{
    int32_t capacity = BakeryCapacity(place);
    if (capacity <= 0 || place->stock[CC_GOOD_WHEAT] <= 0) return 0;
    if (place->hunger > 65) capacity = capacity * 72 / 100;
    else if (place->hunger > 35) capacity = capacity * 86 / 100;
    int32_t grain_floor = 0;
    if (scriptorium_id != 0U && place->id == scriptorium_id &&
        place->hunger == 0 &&
        place->stock[CC_GOOD_TOOLS] > 0 &&
        CcSettlementHasService(place, CC_SERVICE_MILL)) {
        grain_floor = MaximumI32(
            place->reserve_target[CC_GOOD_WHEAT],
            WeeklyFoodUse(sim, place) * 2 + sim->archives.scribes * 2);
    }
    int32_t baked = MinimumI32(
        capacity, MaximumI32(
            0, place->stock[CC_GOOD_WHEAT] - grain_floor));
    baked = MinimumI32(baked,
                       CC_SIM_MAX_UNITS - place->stock[CC_GOOD_BREAD]);
    if (baked <= 0) return 0;
    place->stock[CC_GOOD_WHEAT] -= baked;
    place->stock[CC_GOOD_BREAD] += baked;
    if (sim->current_day % 28 == 0) {
        char text[CC_EVENT_TEXT_CAPACITY];
        (void)snprintf(text, sizeof(text),
                       "%s's bakery mills %d Wheat into %d Bread.",
                       place->name, baked, baked);
        (void)PushEvent(sim, CC_EVENT_BAKERY_PRODUCTION, place->id,
                        place->id, 0U, baked, text);
    }
    return baked;
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
    int32_t food_gap = MaximumI32(
        0, food_need * 3 -
           NutritionRations(place->stock, CC_NUTRITION_CIVILIAN));
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
        int32_t food_target = place->reserve_target[CC_GOOD_BREAD];
        int32_t food_stock = NutritionRations(
            place->stock, CC_NUTRITION_CIVILIAN);
        if (food_target > 0 && food_stock < food_target) {
            int32_t food_gap =
                (food_target - food_stock) * 100 /
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
        if ((sim->shipments[i].status == CC_SHIPMENT_TRAVELLING ||
             sim->shipments[i].status == CC_SHIPMENT_BLOCKED) &&
            sim->shipments[i].good == good) total += sim->shipments[i].quantity;
    }
    if (sim->schema_version >= 19U) {
        for (int32_t dungeon = 0; dungeon < sim->dungeon_count; ++dungeon) {
            for (int32_t room = 0;
                 room < sim->dungeons[dungeon].room_count; ++room) {
                const CcDungeonRoom *cache =
                    &sim->dungeons[dungeon].rooms[room];
                if (cache->loot_good == good) total += cache->loot_quantity;
            }
        }
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
    const int32_t wood_cost = 8;
    const int32_t stone_cost = 6;
    const int32_t material_cost = 6;
    const int32_t tool_cost = 5;
    const CcMoney money_cost = 80;
    if (kingdom == NULL || settlement->stock[CC_GOOD_WOOD] < wood_cost ||
        settlement->stock[CC_GOOD_STONE] < stone_cost ||
        settlement->stock[CC_GOOD_MATERIAL] < material_cost ||
        settlement->stock[CC_GOOD_TOOLS] < tool_cost ||
        kingdom->treasury < money_cost) {
        SetError(error, error_capacity,
                 "The town needs 8 Wood, 6 Stone, 6 Iron, 5 Tools, and 80 crowns.");
        return false;
    }
    settlement->stock[CC_GOOD_WOOD] -= wood_cost;
    settlement->stock[CC_GOOD_STONE] -= stone_cost;
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

enum {
    CC_ROUTE_ANCHOR_REVEAL_MILLI = 280,
    CC_LEGACY_ROUTE_SIGHT_REVEAL_MILLI = 140,
    CC_ROUTE_FULL_REVEAL_MILLI = 1000
};

static CcRouteKnowledge *PlayerRouteKnowledgeMutable(CcSim *sim,
                                                      CcId route_id)
{
    if (sim == NULL || route_id == 0U) return NULL;
    for (int32_t i = 0; i < sim->route_count; ++i) {
        if (sim->routes[i].id != route_id) continue;
        CcRouteKnowledge *knowledge = &sim->player.route_knowledge[i];
        if (knowledge->route_id == 0U) knowledge->route_id = route_id;
        return knowledge->route_id == route_id ? knowledge : NULL;
    }
    return NULL;
}

const CcRouteKnowledge *CcSimPlayerRouteKnowledge(const CcSim *sim,
                                                  CcId route_id)
{
    if (sim == NULL || route_id == 0U) return NULL;
    for (int32_t i = 0; i < sim->route_count; ++i) {
        const CcRouteKnowledge *knowledge =
            &sim->player.route_knowledge[i];
        if (sim->routes[i].id == route_id &&
            knowledge->route_id == route_id) return knowledge;
    }
    return NULL;
}

const CcSettlementKnowledge *CcSimPlayerSettlementKnowledge(
    const CcSim *sim, CcId settlement_id)
{
    if (sim == NULL || settlement_id == 0U) return NULL;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        const CcSettlementKnowledge *knowledge =
            &sim->player.settlement_knowledge[i];
        if (sim->settlements[i].id == settlement_id &&
            knowledge->settlement_id == settlement_id &&
            knowledge->source != CC_PLAYER_KNOWLEDGE_NONE) {
            return knowledge;
        }
    }
    return NULL;
}

static int32_t PlayerKnowledgeSourceRank(CcPlayerKnowledgeSource source)
{
    switch (source) {
        case CC_PLAYER_KNOWLEDGE_TRAVEL: return 6;
        case CC_PLAYER_KNOWLEDGE_WITNESS: return 5;
        case CC_PLAYER_KNOWLEDGE_EVENT: return 4;
        case CC_PLAYER_KNOWLEDGE_RUMOR: return 3;
        case CC_PLAYER_KNOWLEDGE_CHART: return 2;
        case CC_PLAYER_KNOWLEDGE_LEGACY: return 1;
        case CC_PLAYER_KNOWLEDGE_NONE: return 0;
    }
    return 0;
}

static bool PlayerKnowledgeIsNewer(
    int32_t day, CcPlayerKnowledgeSource source,
    int32_t known_day, CcPlayerKnowledgeSource known_source)
{
    return known_source == CC_PLAYER_KNOWLEDGE_NONE ||
        day > known_day ||
        (day == known_day &&
         PlayerKnowledgeSourceRank(source) >=
             PlayerKnowledgeSourceRank(known_source));
}

static void LearnSettlementKnowledge(CcSim *sim, CcId settlement_id,
                                     CcId kingdom_id, int32_t day,
                                     CcPlayerKnowledgeSource source)
{
    if (sim == NULL || source == CC_PLAYER_KNOWLEDGE_NONE ||
        CcIdKind(kingdom_id) != CC_ENTITY_KINGDOM) return;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        if (sim->settlements[i].id != settlement_id) continue;
        CcSettlementKnowledge *knowledge =
            &sim->player.settlement_knowledge[i];
        if (knowledge->settlement_id == 0U) {
            knowledge->settlement_id = settlement_id;
        }
        if (!PlayerKnowledgeIsNewer(
                day, source, knowledge->learned_day, knowledge->source)) {
            return;
        }
        knowledge->kingdom_id = kingdom_id;
        knowledge->learned_day = day;
        knowledge->source = source;
        return;
    }
}

static void LearnCurrentSettlementKnowledge(
    CcSim *sim, CcId settlement_id, CcPlayerKnowledgeSource source)
{
    const CcSettlement *settlement = CcSimSettlement(sim, settlement_id);
    if (settlement == NULL) return;
    LearnSettlementKnowledge(sim, settlement->id, settlement->kingdom_id,
                             sim->current_day, source);
}

static void LearnRouteKnowledgeFacts(CcSim *sim, CcId route_id,
                                     CcPlayerKnowledgeSource source)
{
    const CcRoute *route = CcSimRoute(sim, route_id);
    CcRouteKnowledge *knowledge = PlayerRouteKnowledgeMutable(sim, route_id);
    if (route == NULL || knowledge == NULL ||
        source == CC_PLAYER_KNOWLEDGE_NONE ||
        !PlayerKnowledgeIsNewer(
            sim->current_day, source,
            knowledge->learned_day, knowledge->source)) return;
    knowledge->learned_day = sim->current_day;
    knowledge->recorded_condition = route->condition;
    knowledge->recorded_danger = CcSimRouteDanger(sim, route->id);
    knowledge->source = source;
    knowledge->recorded_closed = route->closed;
    knowledge->recorded_smuggler_route = route->smuggler_route;
}

static void SnapshotMapWorldFacts(CcSim *sim, CcMap *map)
{
    const CcRoute *route = map != NULL ?
        CcSimRoute(sim, map->route_id) : NULL;
    const CcSettlement *from = route != NULL ?
        CcSimSettlement(sim, route->from_id) : NULL;
    const CcSettlement *to = route != NULL ?
        CcSimSettlement(sim, route->to_id) : NULL;
    if (route == NULL || from == NULL || to == NULL) return;
    map->recorded_from_kingdom_id = from->kingdom_id;
    map->recorded_to_kingdom_id = to->kingdom_id;
    map->recorded_closed = route->closed;
    map->recorded_smuggler_route = route->smuggler_route;
}

static void LearnMapKnowledge(CcSim *sim, const CcMap *map)
{
    const CcRoute *route = map != NULL ?
        CcSimRoute(sim, map->route_id) : NULL;
    if (route == NULL) return;
    LearnSettlementKnowledge(
        sim, route->from_id, map->recorded_from_kingdom_id,
        map->surveyed_day, CC_PLAYER_KNOWLEDGE_CHART);
    LearnSettlementKnowledge(
        sim, route->to_id, map->recorded_to_kingdom_id,
        map->surveyed_day, CC_PLAYER_KNOWLEDGE_CHART);
}

static void DiscoverRoadBookSitesAtSettlement(CcSim *sim,
                                               CcId settlement_id)
{
    if (sim == NULL || settlement_id == 0U) return;
    if (sim->dungeon_count > 0 &&
        sim->dungeons[0].settlement_id == settlement_id) {
        sim->player.road_book_site_discovery_mask |=
            UINT32_C(1) << CC_ROAD_BOOK_SITE_UNDERROAD;
    }
    if (sim->goblins.lair_settlement_id == settlement_id) {
        sim->player.road_book_site_discovery_mask |=
            UINT32_C(1) << CC_ROAD_BOOK_SITE_GOBLIN_TRAIL;
    }
    if (sim->dragon.lair_settlement_id == settlement_id) {
        sim->player.road_book_site_discovery_mask |=
            UINT32_C(1) << CC_ROAD_BOOK_SITE_DRAGON_ROOST;
    }
}

bool CcSimPlayerKnowsRoadBookSite(const CcSim *sim,
                                  CcRoadBookSiteKind kind)
{
    return sim != NULL && kind >= CC_ROAD_BOOK_SITE_UNDERROAD &&
        kind < CC_ROAD_BOOK_SITE_COUNT &&
        (sim->player.road_book_site_discovery_mask &
         (UINT32_C(1) << (uint32_t)kind)) != 0U;
}

static void NormalizeRouteKnowledge(CcRouteKnowledge *knowledge)
{
    if (knowledge == NULL) return;
    knowledge->from_reveal_milli = ClampI32(
        knowledge->from_reveal_milli, 0, CC_ROUTE_FULL_REVEAL_MILLI);
    knowledge->to_reveal_milli = ClampI32(
        knowledge->to_reveal_milli, 0, CC_ROUTE_FULL_REVEAL_MILLI);
    if (knowledge->from_reveal_milli >= CC_ROUTE_FULL_REVEAL_MILLI ||
        knowledge->to_reveal_milli >= CC_ROUTE_FULL_REVEAL_MILLI ||
        knowledge->from_reveal_milli + knowledge->to_reveal_milli >=
            CC_ROUTE_FULL_REVEAL_MILLI) {
        knowledge->from_reveal_milli = CC_ROUTE_FULL_REVEAL_MILLI;
        knowledge->to_reveal_milli = CC_ROUTE_FULL_REVEAL_MILLI;
    }
}

static void RevealRouteFromAnchor(CcSim *sim, const CcRoute *route,
                                  CcId anchor_id, int32_t amount_milli)
{
    CcRouteKnowledge *knowledge = route != NULL ?
        PlayerRouteKnowledgeMutable(sim, route->id) : NULL;
    if (knowledge == NULL) return;
    if (route->from_id == anchor_id) {
        knowledge->from_reveal_milli = MaximumI32(
            knowledge->from_reveal_milli, amount_milli);
    } else if (route->to_id == anchor_id) {
        knowledge->to_reveal_milli = MaximumI32(
            knowledge->to_reveal_milli, amount_milli);
    }
    NormalizeRouteKnowledge(knowledge);
    LearnRouteKnowledgeFacts(
        sim, route->id, CC_PLAYER_KNOWLEDGE_TRAVEL);
    LearnCurrentSettlementKnowledge(
        sim, anchor_id, CC_PLAYER_KNOWLEDGE_TRAVEL);
}

static void RevealSettlementRoadAnchors(CcSim *sim, CcId settlement_id)
{
    if (sim == NULL || settlement_id == 0U) return;
    LearnCurrentSettlementKnowledge(
        sim, settlement_id, CC_PLAYER_KNOWLEDGE_TRAVEL);
    DiscoverRoadBookSitesAtSettlement(sim, settlement_id);
    for (int32_t i = 0; i < sim->route_count; ++i) {
        CcRoute *route = &sim->routes[i];
        if (route->from_id == settlement_id ||
            route->to_id == settlement_id) {
            RevealRouteFromAnchor(
                sim, route, settlement_id, CC_ROUTE_ANCHOR_REVEAL_MILLI);
        }
    }
}

static void RevealJourneyRoad(CcSim *sim)
{
    if (sim == NULL || !sim->journey.active) return;
    const CcRoute *route = CcSimRoute(sim, sim->journey.route_id);
    int32_t legacy_sight_milli = sim->schema_version <= 23U ?
        CC_LEGACY_ROUTE_SIGHT_REVEAL_MILLI : 0;
    int32_t visible_milli = ClampI32(
        sim->carriage.progress_milli + legacy_sight_milli,
        CC_ROUTE_ANCHOR_REVEAL_MILLI, CC_ROUTE_FULL_REVEAL_MILLI);
    RevealRouteFromAnchor(
        sim, route, sim->journey.origin_id, visible_milli);
}

static void RevealCompleteRoute(CcSim *sim, CcId route_id)
{
    CcRouteKnowledge *knowledge = PlayerRouteKnowledgeMutable(sim, route_id);
    if (knowledge == NULL) return;
    knowledge->from_reveal_milli = CC_ROUTE_FULL_REVEAL_MILLI;
    knowledge->to_reveal_milli = CC_ROUTE_FULL_REVEAL_MILLI;
    LearnRouteKnowledgeFacts(
        sim, route_id, CC_PLAYER_KNOWLEDGE_TRAVEL);
}

static bool PlayerOwnsCataloguedRouteMap(const CcSim *sim, CcId route_id)
{
    if (sim == NULL || route_id == 0U) return false;
    for (int32_t i = 0; i < sim->map_count; ++i) {
        const CcMap *map = &sim->maps[i];
        if (map->route_id == route_id &&
            map->owner_id == sim->player.id &&
            CcSimMapIsCatalogued(sim, map)) return true;
    }
    return false;
}

bool CcSimPlayerRouteReveal(const CcSim *sim, CcId route_id,
                            int32_t *from_reveal_milli,
                            int32_t *to_reveal_milli,
                            bool *charted)
{
    if (from_reveal_milli != NULL) *from_reveal_milli = 0;
    if (to_reveal_milli != NULL) *to_reveal_milli = 0;
    if (charted != NULL) *charted = false;
    const CcRoute *route = CcSimRoute(sim, route_id);
    if (route == NULL) return false;
    bool has_chart = PlayerOwnsCataloguedRouteMap(sim, route_id);
    if (charted != NULL) *charted = has_chart;
    int32_t from = 0;
    int32_t to = 0;
    const CcRouteKnowledge *knowledge =
        CcSimPlayerRouteKnowledge(sim, route_id);
    if (knowledge != NULL) {
        from = knowledge->from_reveal_milli;
        to = knowledge->to_reveal_milli;
    }
    if (route->from_id == sim->player.location_id) {
        from = MaximumI32(from, CC_ROUTE_ANCHOR_REVEAL_MILLI);
    }
    if (route->to_id == sim->player.location_id) {
        to = MaximumI32(to, CC_ROUTE_ANCHOR_REVEAL_MILLI);
    }
    if (has_chart) from = to = CC_ROUTE_FULL_REVEAL_MILLI;
    if (from_reveal_milli != NULL) *from_reveal_milli = from;
    if (to_reveal_milli != NULL) *to_reveal_milli = to;
    return from > 0 || to > 0;
}

bool CcSimPlayerKnowsSettlement(const CcSim *sim, CcId settlement_id)
{
    if (CcSimSettlement(sim, settlement_id) == NULL) return false;
    return settlement_id == sim->player.location_id ||
        CcSimPlayerSettlementKnowledge(sim, settlement_id) != NULL;
}

void CcSimInitializePlayerRouteKnowledge(CcSim *sim)
{
    if (sim == NULL) return;
    (void)memset(sim->player.route_knowledge, 0,
                 sizeof(sim->player.route_knowledge));
    (void)memset(sim->player.settlement_knowledge, 0,
                 sizeof(sim->player.settlement_knowledge));
    sim->player.road_book_site_discovery_mask = 0U;
    for (int32_t i = 0; i < sim->route_count; ++i) {
        sim->player.route_knowledge[i].route_id = sim->routes[i].id;
    }
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        sim->player.settlement_knowledge[i].settlement_id =
            sim->settlements[i].id;
    }
    for (int32_t i = 0; i < sim->map_count; ++i) {
        CcMap *map = &sim->maps[i];
        if (map->recorded_from_kingdom_id == 0U ||
            map->recorded_to_kingdom_id == 0U) {
            SnapshotMapWorldFacts(sim, map);
        }
        if (map->owner_id == sim->player.id &&
            CcSimMapIsCatalogued(sim, map)) {
            LearnMapKnowledge(sim, map);
        }
    }
    RevealSettlementRoadAnchors(sim, sim->player.location_id);
    if (sim->journey.active) {
        RevealSettlementRoadAnchors(sim, sim->journey.origin_id);
        RevealJourneyRoad(sim);
    }
}

void CcSimUpgradePlayerKnowledge(CcSim *sim)
{
    if (sim == NULL) return;
    for (int32_t i = 0; i < sim->map_count; ++i) {
        CcMap *map = &sim->maps[i];
        if (map->recorded_from_kingdom_id == 0U ||
            map->recorded_to_kingdom_id == 0U) {
            SnapshotMapWorldFacts(sim, map);
        }
    }
    for (int32_t i = 0; i < sim->route_count; ++i) {
        CcRouteKnowledge *knowledge = &sim->player.route_knowledge[i];
        if (knowledge->route_id == 0U) {
            knowledge->route_id = sim->routes[i].id;
        }
        if (knowledge->source == CC_PLAYER_KNOWLEDGE_NONE &&
            (knowledge->from_reveal_milli > 0 ||
             knowledge->to_reveal_milli > 0)) {
            LearnRouteKnowledgeFacts(
                sim, knowledge->route_id, CC_PLAYER_KNOWLEDGE_LEGACY);
        }
    }
    (void)memset(sim->player.settlement_knowledge, 0,
                 sizeof(sim->player.settlement_knowledge));
    sim->player.road_book_site_discovery_mask = 0U;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        sim->player.settlement_knowledge[i].settlement_id =
            sim->settlements[i].id;
    }
    for (int32_t i = 0; i < sim->map_count; ++i) {
        const CcMap *map = &sim->maps[i];
        if (map->owner_id == sim->player.id &&
            CcSimMapIsCatalogued(sim, map)) {
            LearnMapKnowledge(sim, map);
        }
    }
    LearnCurrentSettlementKnowledge(
        sim, sim->player.location_id, CC_PLAYER_KNOWLEDGE_LEGACY);
    DiscoverRoadBookSitesAtSettlement(sim, sim->player.location_id);
    if (sim->journey.active) {
        LearnCurrentSettlementKnowledge(
            sim, sim->journey.origin_id, CC_PLAYER_KNOWLEDGE_LEGACY);
        DiscoverRoadBookSitesAtSettlement(sim, sim->journey.origin_id);
    }
}

static bool RouteTouchesSettlement(const CcSim *sim, CcId route_id,
                                   CcId settlement_id)
{
    const CcRoute *route = CcSimRoute(sim, route_id);
    return route != NULL && (route->from_id == settlement_id ||
                             route->to_id == settlement_id);
}

static void LearnPlayerKnowledgeFromEvent(
    CcSim *sim, const CcEvent *event, CcPlayerKnowledgeSource source)
{
    if (sim == NULL || event == NULL || sim->player.id == 0U) return;
    bool participant = event->actor_id == sim->player.id ||
        event->target_id == sim->player.id ||
        event->beneficiary_id == sim->player.id ||
        event->witness_id == sim->player.id;
    bool local = event->location_id == sim->player.location_id ||
        (CcIdKind(event->location_id) == CC_ENTITY_ROUTE &&
         RouteTouchesSettlement(
             sim, event->location_id, sim->player.location_id)) ||
        (CcIdKind(event->subject_id) == CC_ENTITY_ROUTE &&
         RouteTouchesSettlement(
             sim, event->subject_id, sim->player.location_id));
    if (!participant && !local) return;

    if (CcIdKind(event->subject_id) == CC_ENTITY_ROUTE) {
        LearnRouteKnowledgeFacts(sim, event->subject_id, source);
    }
    if (CcIdKind(event->location_id) == CC_ENTITY_ROUTE) {
        LearnRouteKnowledgeFacts(sim, event->location_id, source);
    }
    if (CcIdKind(event->subject_id) == CC_ENTITY_SETTLEMENT) {
        LearnCurrentSettlementKnowledge(sim, event->subject_id, source);
    }
    if (CcIdKind(event->location_id) == CC_ENTITY_SETTLEMENT) {
        LearnCurrentSettlementKnowledge(sim, event->location_id, source);
    }
    if (event->subject_id == sim->goblins.id) {
        sim->player.road_book_site_discovery_mask |=
            UINT32_C(1) << CC_ROAD_BOOK_SITE_GOBLIN_TRAIL;
    } else if (event->subject_id == sim->dragon.id) {
        sim->player.road_book_site_discovery_mask |=
            UINT32_C(1) << CC_ROAD_BOOK_SITE_DRAGON_ROOST;
    } else if (CcIdKind(event->subject_id) == CC_ENTITY_DUNGEON) {
        sim->player.road_book_site_discovery_mask |=
            UINT32_C(1) << CC_ROAD_BOOK_SITE_UNDERROAD;
    }
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

const CcFront *CcSimFront(const CcSim *sim, CcId id)
{
    if (sim == NULL || CcIdKind(id) != CC_ENTITY_FRONT) return NULL;
    for (int32_t i = 0; i < sim->front_count; ++i) {
        if (sim->fronts[i].id == id) return &sim->fronts[i];
    }
    return NULL;
}

const CcFront *CcSimSituationFront(const CcSim *sim,
                                   const CcSituation *situation)
{
    return situation != NULL ? CcSimFront(sim, situation->front_id) : NULL;
}

const CcQuestOutcomeRecord *CcSimQuestOutcome(const CcSim *sim,
                                              CcId situation_id)
{
    if (sim == NULL || CcIdKind(situation_id) != CC_ENTITY_SITUATION) {
        return NULL;
    }
    const CcQuestOutcomeRecord *latest = NULL;
    for (int32_t i = 0; i < sim->quest_outcome_count; ++i) {
        const CcQuestOutcomeRecord *outcome = &sim->quest_outcomes[i];
        if (outcome->situation_id == situation_id &&
            (latest == NULL || outcome->resolved_day > latest->resolved_day)) {
            latest = outcome;
        }
    }
    return latest;
}

const CcQuestOutcomeRecord *CcSimLatestQuestOutcomeForCharacter(
    const CcSim *sim, CcId character_id)
{
    if (sim == NULL || CcIdKind(character_id) != CC_ENTITY_CHARACTER) {
        return NULL;
    }
    const CcQuestOutcomeRecord *latest = NULL;
    for (int32_t i = 0; i < sim->quest_outcome_count; ++i) {
        const CcQuestOutcomeRecord *outcome = &sim->quest_outcomes[i];
        bool involved = outcome->sponsor_character_id == character_id ||
                        outcome->affected_character_id == character_id;
        if (involved &&
            (latest == NULL || outcome->resolved_day > latest->resolved_day)) {
            latest = outcome;
        }
    }
    return latest;
}

CcFrontStage CcSimFrontStage(const CcFront *front)
{
    if (front == NULL || front->status != CC_FRONT_ACTIVE) {
        return CC_FRONT_STAGE_CLOSED;
    }
    int32_t percent = CcQuestClockPercent(&front->portent);
    if (percent >= 67) return CC_FRONT_STAGE_BREAKING;
    if (percent >= 34) return CC_FRONT_STAGE_PRESSING;
    return CC_FRONT_STAGE_RUMBLING;
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

const CcCharacter *CcSimSituationWitnessCharacter(
    const CcSim *sim, const CcSituation *situation)
{
    return situation != NULL ?
        CcSimCharacter(sim, situation->witness_character_id) : NULL;
}

const CcRelationship *CcSimRelationship(const CcSim *sim,
                                        CcId from_character_id,
                                        CcId to_character_id)
{
    if (sim == NULL || from_character_id == 0U || to_character_id == 0U) {
        return NULL;
    }
    for (int32_t i = 0; i < sim->relationship_count; ++i) {
        const CcRelationship *relationship = &sim->relationships[i];
        if (relationship->from_character_id == from_character_id &&
            relationship->to_character_id == to_character_id) {
            return relationship;
        }
    }
    return NULL;
}

bool CcCharacterKnows(const CcCharacter *character,
                      CcKnowledgeKind kind, CcId subject_id)
{
    if (character == NULL || kind == CC_KNOWLEDGE_NONE) return false;
    for (int32_t i = 0; i < character->knowledge_count; ++i) {
        const CcCharacterKnowledge *knowledge = &character->knowledge[i];
        if (knowledge->kind == kind && knowledge->subject_id == subject_id) {
            return true;
        }
    }
    return false;
}

bool CcSimSituationCanAccept(const CcSim *sim,
                             const CcSituation *situation)
{
    (void)sim;
    if (situation == NULL || situation->status != CC_SITUATION_ACTIVE) {
        return false;
    }
    return situation->kind != CC_SITUATION_MONSTER_EXPEDITION ||
           situation->discovery_stage == CC_DISCOVERY_OFFER;
}

const CcCharacter *CcSimSituationConversationCharacter(
    const CcSim *sim, const CcSituation *situation, CcId settlement_id)
{
    if (sim == NULL || situation == NULL || settlement_id == 0U) return NULL;
    const CcCharacter *sponsor = CcSimSituationSponsorCharacter(
        sim, situation);
    const CcCharacter *affected = CcSimSituationAffectedCharacter(
        sim, situation);
    const CcCharacter *witness = CcSimSituationWitnessCharacter(
        sim, situation);
    if (situation->kind == CC_SITUATION_MONSTER_EXPEDITION) {
        const CcCharacter *lead = NULL;
        switch (situation->discovery_stage) {
            case CC_DISCOVERY_RUMOR:
            case CC_DISCOVERY_DECISION:
                lead = affected;
                break;
            case CC_DISCOVERY_WITNESS:
                lead = witness;
                break;
            case CC_DISCOVERY_AUTHORITY:
                lead = sponsor;
                break;
            case CC_DISCOVERY_OFFER:
                lead = situation->lead_path == CC_LEAD_PATH_CONFIDENCE ?
                    affected : sponsor;
                break;
        }
        return lead != NULL && lead->current_settlement_id == settlement_id ?
            lead : NULL;
    }
    bool sponsor_here = sponsor != NULL &&
        sponsor->current_settlement_id == settlement_id;
    bool affected_here = affected != NULL &&
        affected->current_settlement_id == settlement_id;
    CcId offer_settlement = CcSimSituationOfferSettlementId(sim, situation);

    if (affected_here &&
        (situation->kind == CC_SITUATION_COURIER_DELIVERY ||
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
        if (situation->discovery_stage == CC_DISCOVERY_AUTHORITY ||
            (situation->discovery_stage == CC_DISCOVERY_OFFER &&
             situation->lead_path == CC_LEAD_PATH_REPORT)) {
            const CcCharacter *sponsor = CcSimSituationSponsorCharacter(
                sim, situation);
            if (sponsor != NULL) return sponsor->current_settlement_id;
        }
        if (situation->discovery_stage == CC_DISCOVERY_OFFER &&
            situation->lead_path == CC_LEAD_PATH_CONFIDENCE) {
            const CcCharacter *affected = CcSimSituationAffectedCharacter(
                sim, situation);
            if (affected != NULL) return affected->current_settlement_id;
        }
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

int32_t CcSimActiveFrontCount(const CcSim *sim)
{
    if (sim == NULL) return 0;
    int32_t count = 0;
    for (int32_t i = 0; i < sim->front_count; ++i) {
        if (sim->fronts[i].status == CC_FRONT_ACTIVE) count += 1;
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
        const CcCharacter *lead = CcSimSituationConversationCharacter(
            sim, situation, settlement_id);
        if (lead != NULL) return true;
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
        if ((shipment->status == CC_SHIPMENT_TRAVELLING ||
             shipment->status == CC_SHIPMENT_BLOCKED) &&
            shipment->final_destination_id == settlement_id && shipment->good == good) {
            incoming += shipment->quantity;
        }
    }
    return incoming > CC_SIM_MAX_UNITS ? CC_SIM_MAX_UNITS :
           incoming < 0 ? 0 : (int32_t)incoming;
}

static const CcSettlement *Scriptorium(const CcSim *sim)
{
    if (sim == NULL) return NULL;
    const CcSettlement *fallback = NULL;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        const CcSettlement *place = &sim->settlements[i];
        if (CcSettlementIsAbandoned(place)) continue;
        if (strcmp(place->name, "Gloamgate") == 0) return place;
        if (fallback == NULL &&
            (place->function == CC_SETTLEMENT_MARKET ||
             place->function == CC_SETTLEMENT_CAPITAL)) {
            fallback = place;
        }
    }
    return fallback;
}

CcMaterialChainSnapshot CcSimMaterialChainSnapshot(const CcSim *sim)
{
    CcMaterialChainSnapshot snapshot = {0};
    const CcSettlement *place = Scriptorium(sim);
    if (sim == NULL || place == NULL) {
        snapshot.blocker = CC_MATERIAL_CHAIN_NO_SCRIBES;
        return snapshot;
    }
    snapshot.scriptorium_id = place->id;
    snapshot.scribes = sim->archives.scribes;
    snapshot.wheat = place->stock[CC_GOOD_WHEAT];
    snapshot.paper = place->stock[CC_GOOD_PAPER];
    snapshot.tools = place->stock[CC_GOOD_TOOLS];
    snapshot.iron = place->stock[CC_GOOD_IRON];
    snapshot.incoming_tools = CcSimIncomingGood(
        sim, place->id, CC_GOOD_TOOLS);
    snapshot.incoming_iron = CcSimIncomingGood(
        sim, place->id, CC_GOOD_IRON);
    bool binding_available = false;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        const CcSettlement *vault = &sim->settlements[i];
        if (CcSettlementIsAbandoned(vault)) continue;
        int64_t gold = (int64_t)snapshot.gold +
            vault->stock[CC_GOOD_GOLD];
        int64_t gems = (int64_t)snapshot.gems +
            vault->stock[CC_GOOD_GEMS];
        snapshot.gold = gold > CC_SIM_MAX_UNITS ?
            CC_SIM_MAX_UNITS : (int32_t)gold;
        snapshot.gems = gems > CC_SIM_MAX_UNITS ?
            CC_SIM_MAX_UNITS : (int32_t)gems;
        if (vault->stock[CC_GOOD_GOLD] > 0 &&
            vault->stock[CC_GOOD_GEMS] > 0) {
            binding_available = true;
        }
    }
    int32_t scribe_grain = MaximumI32(
        0, snapshot.wheat - WeeklyFoodUse(sim, place) * 2);
    snapshot.blocker = snapshot.scribes <= 0 ?
        CC_MATERIAL_CHAIN_NO_SCRIBES :
        !binding_available ? CC_MATERIAL_CHAIN_BINDING :
        snapshot.tools <= 0 ? CC_MATERIAL_CHAIN_TOOLS :
        scribe_grain < 2 ? CC_MATERIAL_CHAIN_GRAIN :
        snapshot.paper <= 0 ? CC_MATERIAL_CHAIN_PAPER :
        CC_MATERIAL_CHAIN_READY;
    return snapshot;
}

const char *CcMaterialChainBlockerName(CcMaterialChainBlocker blocker)
{
    switch (blocker) {
        case CC_MATERIAL_CHAIN_READY: return "ready";
        case CC_MATERIAL_CHAIN_NO_SCRIBES: return "scribes";
        case CC_MATERIAL_CHAIN_GRAIN: return "grain";
        case CC_MATERIAL_CHAIN_PAPER: return "paper";
        case CC_MATERIAL_CHAIN_TOOLS: return "tools";
        case CC_MATERIAL_CHAIN_BINDING: return "binding";
    }
    return "unknown";
}

int32_t CcPlayerCargoUsed(const CcPlayerCompany *player)
{
    if (player == NULL) return 0;
    int32_t used = player->treasure_cargo_slots;
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        if (player->cargo[good] > 0) {
            const CcGoodDefinition *definition = CcGoodDefinitionFor(
                (CcGood)good);
            used += (player->cargo[good] +
                     definition->player_units_per_slot - 1) /
                    definition->player_units_per_slot;
        }
    }
    return used;
}

static int32_t PlayerCargoBoxes(CcGood good, int32_t quantity)
{
    const CcGoodDefinition *definition = CcGoodDefinitionFor(good);
    if (definition == NULL || quantity <= 0) return 0;
    return (quantity + definition->player_units_per_slot - 1) /
           definition->player_units_per_slot;
}

static int32_t FreightCargoSlots(CcGood good, int32_t quantity)
{
    const CcGoodDefinition *definition = CcGoodDefinitionFor(good);
    if (definition == NULL || quantity <= 0) return 0;
    return (quantity + definition->freight_units_per_slot - 1) /
           definition->freight_units_per_slot;
}

static int32_t FreightUnitsPerCargoSlot(CcGood good)
{
    const CcGoodDefinition *definition = CcGoodDefinitionFor(good);
    return definition != NULL ? definition->freight_units_per_slot : 1;
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
    kingdom->sanction = legitimacy;
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
        settlement->price[good] =
            CcGoodDefinitionFor((CcGood)good)->base_price;
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
                    ServiceBit(CC_SERVICE_CARTOGRAPHER) |
                    ServiceBit(CC_SERVICE_BAKERY) |
                    ServiceBit(CC_SERVICE_MILL);
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
                    ServiceBit(CC_SERVICE_FARM) |
                    ServiceBit(CC_SERVICE_BAKERY) |
                    ServiceBit(CC_SERVICE_MILL);
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
        if (map->recorded_from_kingdom_id == 0U ||
            map->recorded_to_kingdom_id == 0U) {
            SnapshotMapWorldFacts(sim, map);
        }
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
    farm->stock[CC_GOOD_WHEAT] = 60;
    farm->stock[CC_GOOD_MATERIAL] = 12;
    farm->stock[CC_GOOD_TOOLS] = 8;
    farm->reserve_target[CC_GOOD_FOOD] = 105;
    farm->reserve_target[CC_GOOD_WHEAT] = 60;
    farm->reserve_target[CC_GOOD_MATERIAL] = 2;
    farm->reserve_target[CC_GOOD_TOOLS] = 6;
    farm->production[CC_GOOD_WHEAT] = 43;
    farm->consumption[CC_GOOD_FOOD] = 5;
    farm->field_yield = 100;

    CcSettlement *market = &sim->settlements[1];
    market->stock[CC_GOOD_FOOD] = 35;
    market->stock[CC_GOOD_WHEAT] = 24;
    market->stock[CC_GOOD_MATERIAL] = 32;
    market->stock[CC_GOOD_TOOLS] = 23;
    market->reserve_target[CC_GOOD_FOOD] = 75;
    market->reserve_target[CC_GOOD_WHEAT] = 36;
    market->reserve_target[CC_GOOD_MATERIAL] = 16;
    market->reserve_target[CC_GOOD_TOOLS] = 12;
    market->production[CC_GOOD_TOOLS] = 4;
    market->production[CC_GOOD_WEAPONS] = 2;
    market->production[CC_GOOD_BREAD] = 35;
    market->consumption[CC_GOOD_FOOD] = 7;
    market->consumption[CC_GOOD_MATERIAL] = 3;

    CcSettlement *fortress = &sim->settlements[2];
    fortress->stock[CC_GOOD_FOOD] = 42;
    fortress->stock[CC_GOOD_WHEAT] = 24;
    fortress->stock[CC_GOOD_MATERIAL] = 18;
    fortress->stock[CC_GOOD_TOOLS] = 15;
    fortress->reserve_target[CC_GOOD_FOOD] = 70;
    fortress->reserve_target[CC_GOOD_WHEAT] = 24;
    fortress->reserve_target[CC_GOOD_MATERIAL] = 8;
    fortress->reserve_target[CC_GOOD_TOOLS] = 8;
    fortress->production[CC_GOOD_WHEAT] = 30;
    fortress->field_yield = 85;
    fortress->stock[CC_GOOD_WEAPONS] = 8;
    fortress->reserve_target[CC_GOOD_WEAPONS] = 14;
    fortress->production[CC_GOOD_WEAPONS] = 2;
    fortress->consumption[CC_GOOD_FOOD] = 6;
    fortress->consumption[CC_GOOD_TOOLS] = 1;

    CcSettlement *mine = &sim->settlements[3];
    mine->stock[CC_GOOD_FOOD] = 21;
    mine->stock[CC_GOOD_WHEAT] = 18;
    mine->stock[CC_GOOD_MATERIAL] = 76;
    mine->stock[CC_GOOD_TOOLS] = 13;
    mine->reserve_target[CC_GOOD_FOOD] = 78;
    mine->reserve_target[CC_GOOD_WHEAT] = 24;
    mine->reserve_target[CC_GOOD_MATERIAL] = 12;
    mine->reserve_target[CC_GOOD_TOOLS] = 8;
    mine->production[CC_GOOD_MATERIAL] = 10;
    mine->production[CC_GOOD_WHEAT] = 20;
    mine->field_yield = 70;
    mine->iron_deposit = 12000;
    mine->gold_seam = true;
    mine->gem_seam = true;
    mine->consumption[CC_GOOD_FOOD] = 7;
    mine->consumption[CC_GOOD_TOOLS] = 2;

    CcSettlement *capital = &sim->settlements[4];
    capital->stock[CC_GOOD_FOOD] = 61;
    capital->stock[CC_GOOD_WHEAT] = 32;
    capital->stock[CC_GOOD_MATERIAL] = 36;
    capital->stock[CC_GOOD_TOOLS] = 31;
    capital->reserve_target[CC_GOOD_FOOD] = 92;
    capital->reserve_target[CC_GOOD_WHEAT] = 40;
    capital->reserve_target[CC_GOOD_MATERIAL] = 16;
    capital->reserve_target[CC_GOOD_TOOLS] = 14;
    capital->production[CC_GOOD_WHEAT] = 35;
    capital->production[CC_GOOD_BREAD] = 35;
    capital->field_yield = 90;
    capital->production[CC_GOOD_TOOLS] = 5;
    capital->production[CC_GOOD_WEAPONS] = 2;
    capital->stock[CC_GOOD_GOLD] = 1;
    capital->stock[CC_GOOD_GEMS] = 1;
    capital->consumption[CC_GOOD_FOOD] = 8;
    capital->consumption[CC_GOOD_MATERIAL] = 3;

    CcSettlement *frontier = &sim->settlements[5];
    frontier->stock[CC_GOOD_FOOD] = 39;
    frontier->stock[CC_GOOD_WHEAT] = 8;
    frontier->stock[CC_GOOD_MATERIAL] = 48;
    frontier->stock[CC_GOOD_TOOLS] = 17;
    frontier->reserve_target[CC_GOOD_FOOD] = 62;
    frontier->reserve_target[CC_GOOD_WHEAT] = 12;
    frontier->reserve_target[CC_GOOD_MATERIAL] = 10;
    frontier->reserve_target[CC_GOOD_TOOLS] = 6;
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
        int32_t settlement_good_count = CC_LEGACY_GOOD_COUNT;
        for (int32_t good = 0; good < settlement_good_count; ++good) {
            bool legacy_bulk_good = good == CC_GOOD_BREAD ||
                                    good == CC_GOOD_IRON ||
                                    good == CC_GOOD_TOOLS;
            bool precious_good = good == CC_GOOD_GOLD ||
                                 good == CC_GOOD_GEMS;
            int32_t floor = legacy_bulk_good ? 1 : 0;
            settlement->stock[good] = MaximumI32(floor, settlement->stock[good] +
                (int32_t)(NextRandom(sim) % (precious_good ? 3U : 13U)) -
                (precious_good ? 1 : 6));
            if (good == CC_GOOD_BREAD &&
                settlement->function != CC_SETTLEMENT_MARKET) {
                int32_t variation = (int32_t)(NextRandom(sim) % 3U) - 1;
                if (settlement->production[CC_GOOD_WHEAT] > 0) {
                    settlement->production[CC_GOOD_WHEAT] = MaximumI32(
                        0, settlement->production[CC_GOOD_WHEAT] + variation);
                }
            } else if (settlement->production[good] > 0) {
                settlement->production[good] = MaximumI32(
                    0, settlement->production[good] +
                        (int32_t)(NextRandom(sim) % 3U) - 1);
            }
        }
        RefreshSettlementGoodPrice(sim, settlement, CC_GOOD_FOOD);
    }
}

void CcSimInitializeWoodEconomy(CcSim *sim)
{
    if (sim == NULL) return;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        CcSettlement *place = &sim->settlements[i];
        int32_t stock = 0;
        int32_t reserve = 0;
        int32_t production = 0;
        switch (place->function) {
            case CC_SETTLEMENT_FARMING:
                stock = 28;
                reserve = 18;
                production = 12;
                break;
            case CC_SETTLEMENT_MARKET:
                stock = 18;
                reserve = 24;
                production = 3;
                break;
            case CC_SETTLEMENT_FORTRESS:
                stock = 20;
                reserve = 22;
                production = 4;
                break;
            case CC_SETTLEMENT_MINING:
                stock = 8;
                reserve = 18;
                production = 1;
                break;
            case CC_SETTLEMENT_CAPITAL:
                stock = 24;
                reserve = 28;
                production = 4;
                break;
            case CC_SETTLEMENT_DUNGEON_TOWN:
                stock = 36;
                reserve = 16;
                production = 14;
                break;
        }
        place->stock[CC_GOOD_WOOD] = stock;
        place->reserve_target[CC_GOOD_WOOD] = reserve;
        place->production[CC_GOOD_WOOD] = production;
        place->consumption[CC_GOOD_WOOD] = 0;
        place->price[CC_GOOD_WOOD] =
            CcGoodDefinitionFor(CC_GOOD_WOOD)->base_price;
    }
}

typedef struct CcRoadSiteSeed {
    int32_t route_slot;
    const char *name;
    CcRoadSiteKind kind;
    CcGood input_good;
    CcGood output_good;
    int32_t progress_milli;
    int32_t side;
    int32_t spur_length;
} CcRoadSiteSeed;

void CcSimInitializeRoadSites(CcSim *sim)
{
    if (sim == NULL || sim->road_site_count > 0 ||
        sim->route_count < CC_MAX_ROUTES) return;
    static const CcRoadSiteSeed seeds[CC_MAX_ROAD_SITES] = {
        {0, "Nine Furrows", CC_ROAD_SITE_FARM, CC_GOOD_TOOLS,
         CC_GOOD_WHEAT, 160, -1, 28},
        {0, NULL, CC_ROAD_SITE_ROAD_HOUSE, CC_GOOD_BREAD,
         CC_GOOD_COUNT, 0, 1, 22},
        {0, "Stag's Mill", CC_ROAD_SITE_MILL, CC_GOOD_WHEAT,
         CC_GOOD_BREAD, 820, 1, 24},
        {1, "Hook Meadow", CC_ROAD_SITE_FARM, CC_GOOD_TOOLS,
         CC_GOOD_WHEAT, 170, 1, 26},
        {1, NULL, CC_ROAD_SITE_ROAD_HOUSE, CC_GOOD_BREAD,
         CC_GOOD_COUNT, 0, -1, 20},
        {1, "Treaty Stone Yard", CC_ROAD_SITE_ROAD_CREW, CC_GOOD_STONE,
         CC_GOOD_COUNT, 830, -1, 25},
        {2, "Watch Woodlot", CC_ROAD_SITE_WOODLOT, CC_GOOD_TOOLS,
         CC_GOOD_WOOD, 150, -1, 30},
        {2, NULL, CC_ROAD_SITE_ROAD_HOUSE, CC_GOOD_BREAD,
         CC_GOOD_COUNT, 0, 1, 22},
        {2, "Low Silver Pit", CC_ROAD_SITE_MINE, CC_GOOD_TOOLS,
         CC_GOOD_IRON, 840, 1, 28},
        {3, "Fellside Quarry", CC_ROAD_SITE_QUARRY, CC_GOOD_TOOLS,
         CC_GOOD_STONE, 180, 1, 30},
        {3, NULL, CC_ROAD_SITE_ROAD_HOUSE, CC_GOOD_BREAD,
         CC_GOOD_COUNT, 0, -1, 21},
        {3, "Crown Forge", CC_ROAD_SITE_SMITHY, CC_GOOD_IRON,
         CC_GOOD_TOOLS, 820, -1, 24},
        {4, "Abbey Fold", CC_ROAD_SITE_PASTURE, CC_GOOD_WHEAT,
         CC_GOOD_MEAT, 160, -1, 28},
        {4, NULL, CC_ROAD_SITE_ROAD_HOUSE, CC_GOOD_BREAD,
         CC_GOOD_COUNT, 0, 1, 22},
        {4, "Hollow Woodlot", CC_ROAD_SITE_WOODLOT, CC_GOOD_TOOLS,
         CC_GOOD_WOOD, 830, 1, 30},
        {5, "Ashfield Farm", CC_ROAD_SITE_FARM, CC_GOOD_TOOLS,
         CC_GOOD_WHEAT, 180, 1, 28},
        {5, NULL, CC_ROAD_SITE_ROAD_HOUSE, CC_GOOD_BREAD,
         CC_GOOD_COUNT, 0, -1, 20},
        {5, "Underroad Stone Yard", CC_ROAD_SITE_QUARRY, CC_GOOD_TOOLS,
         CC_GOOD_STONE, 840, -1, 27},
        {6, "Black Coppice", CC_ROAD_SITE_WOODLOT, CC_GOOD_TOOLS,
         CC_GOOD_WOOD, 150, -1, 32},
        {6, NULL, CC_ROAD_SITE_ROAD_HOUSE, CC_GOOD_BREAD,
         CC_GOOD_COUNT, 0, 1, 20},
        {6, "Smugglers' Lime Pit", CC_ROAD_SITE_QUARRY, CC_GOOD_TOOLS,
         CC_GOOD_STONE, 850, 1, 29},
        {7, "Muster Farm", CC_ROAD_SITE_FARM, CC_GOOD_TOOLS,
         CC_GOOD_WHEAT, 170, 1, 27},
        {7, NULL, CC_ROAD_SITE_ROAD_HOUSE, CC_GOOD_BREAD,
         CC_GOOD_COUNT, 0, -1, 22},
        {7, "Highroad Bakehouse", CC_ROAD_SITE_BAKERY, CC_GOOD_WHEAT,
         CC_GOOD_BREAD, 830, -1, 24}
    };
    for (int32_t i = 0; i < CC_MAX_ROAD_SITES; ++i) {
        const CcRoadSiteSeed *seed = &seeds[i];
        CcRoute *route = &sim->routes[seed->route_slot];
        CcRoadSite *site = &sim->road_sites[sim->road_site_count++];
        site->id = CcMakeId(CC_ENTITY_ROAD_SITE, (uint64_t)i + 1U);
        site->route_id = route->id;
        site->kind = seed->kind;
        site->input_good = seed->input_good;
        site->output_good = seed->output_good;
        site->progress_milli = seed->kind == CC_ROAD_SITE_ROAD_HOUSE ?
            300 + (int32_t)(RoadHouseSeed(sim, route->id) % 401U) :
            seed->progress_milli;
        site->side = seed->side;
        site->spur_length = seed->spur_length;
        site->condition = 55 + (int32_t)(
            (sim->world_seed + (uint32_t)(i * 17)) % 41U);
        site->blocker = seed->kind == CC_ROAD_SITE_QUARRY ||
            seed->kind == CC_ROAD_SITE_MINE ||
            seed->kind == CC_ROAD_SITE_ROAD_CREW ?
            CC_ROAD_SITE_BLOCKER_ROCKS : CC_ROAD_SITE_BLOCKER_TREE;
        site->accessible = false;
        site->home_settlement_id = site->progress_milli < 500 ?
            route->from_id : route->to_id;
        CopyName(site->name, seed->name != NULL ? seed->name :
                 GeneratedRoadHouseName(sim, route->id));
    }
}

void CcSimUpgradeGrainEconomy(CcSim *sim)
{
    if (sim == NULL) return;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        CcSettlement *place = &sim->settlements[i];
        int32_t legacy_food_output = place->production[CC_GOOD_BREAD];
        if (CcSettlementHasService(place, CC_SERVICE_FARM)) {
            place->production[CC_GOOD_WHEAT] = MaximumI32(
                place->production[CC_GOOD_WHEAT],
                legacy_food_output * 5 / 4);
        }
        place->production[CC_GOOD_BREAD] = 0;

        bool bakery_town = place->function == CC_SETTLEMENT_MARKET ||
                           place->function == CC_SETTLEMENT_CAPITAL;
        if (bakery_town &&
            !CcSettlementHasService(place, CC_SERVICE_BAKERY) &&
            CcSettlementServiceCount(place) <
                CcSettlementServiceCapacity(place->size)) {
            place->service_mask |=
                UINT32_C(1) << (uint32_t)CC_SERVICE_BAKERY;
        }
        if (CcSettlementHasService(place, CC_SERVICE_BAKERY)) {
            place->production[CC_GOOD_BREAD] = 35;
        }

        int32_t wheat_reserve = place->function == CC_SETTLEMENT_FARMING ? 60 :
            place->function == CC_SETTLEMENT_MARKET ? 36 :
            place->function == CC_SETTLEMENT_CAPITAL ? 40 :
            (place->function == CC_SETTLEMENT_FORTRESS ||
             place->function == CC_SETTLEMENT_MINING) ? 24 : 12;
        place->reserve_target[CC_GOOD_WHEAT] = MaximumI32(
            place->reserve_target[CC_GOOD_WHEAT], wheat_reserve);
        if (place->stock[CC_GOOD_WHEAT] == 0) {
            place->stock[CC_GOOD_WHEAT] = MaximumI32(4, wheat_reserve / 2);
        }
        place->price[CC_GOOD_BREAD] = MaximumI32(
            1, place->price[CC_GOOD_BREAD]);
        place->price[CC_GOOD_WHEAT] =
            CcGoodDefinitionFor(CC_GOOD_WHEAT)->base_price;
    }
}

void CcSimInitializeStoneEconomy(CcSim *sim)
{
    if (sim == NULL) return;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        CcSettlement *place = &sim->settlements[i];
        int32_t stock = 0;
        int32_t reserve = 0;
        int32_t production = 0;
        switch (place->function) {
            case CC_SETTLEMENT_FARMING:
                stock = 8;
                reserve = 12;
                break;
            case CC_SETTLEMENT_MARKET:
                stock = 14;
                reserve = 18;
                break;
            case CC_SETTLEMENT_FORTRESS:
                stock = 20;
                reserve = 20;
                break;
            case CC_SETTLEMENT_MINING:
                stock = 36;
                reserve = 16;
                production = 12;
                break;
            case CC_SETTLEMENT_CAPITAL:
                stock = 24;
                reserve = 26;
                break;
            case CC_SETTLEMENT_DUNGEON_TOWN:
                stock = 10;
                reserve = 14;
                break;
        }
        place->stock[CC_GOOD_STONE] = stock;
        place->reserve_target[CC_GOOD_STONE] = reserve;
        place->production[CC_GOOD_STONE] = production;
        place->consumption[CC_GOOD_STONE] = 0;
        place->price[CC_GOOD_STONE] =
            CcGoodDefinitionFor(CC_GOOD_STONE)->base_price;
    }
}

void CcSimInitializePaperEconomy(CcSim *sim)
{
    if (sim == NULL) return;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        CcSettlement *place = &sim->settlements[i];
        int32_t stock = 0;
        int32_t reserve = 0;
        int32_t production = 0;
        int32_t consumption = 0;
        switch (place->function) {
            case CC_SETTLEMENT_FARMING:
                stock = 4;
                reserve = 6;
                production = 2;
                consumption = 1;
                break;
            case CC_SETTLEMENT_MARKET:
                stock = 14;
                reserve = 12;
                production = 6;
                consumption = 3;
                break;
            case CC_SETTLEMENT_FORTRESS:
                stock = 4;
                reserve = 6;
                consumption = 1;
                break;
            case CC_SETTLEMENT_MINING:
                stock = 2;
                reserve = 4;
                consumption = 1;
                break;
            case CC_SETTLEMENT_CAPITAL:
                stock = 18;
                reserve = 16;
                production = 8;
                consumption = 4;
                break;
            case CC_SETTLEMENT_DUNGEON_TOWN:
                stock = 1;
                reserve = 3;
                break;
        }
        place->stock[CC_GOOD_PAPER] = stock;
        place->reserve_target[CC_GOOD_PAPER] = reserve;
        place->production[CC_GOOD_PAPER] = production;
        place->consumption[CC_GOOD_PAPER] = consumption;
        place->price[CC_GOOD_PAPER] =
            CcGoodDefinitionFor(CC_GOOD_PAPER)->base_price;
        for (CcGood spoiled = CC_GOOD_ROTTEN_MEAT;
             spoiled <= CC_GOOD_ROTTEN_GRAIN;
             spoiled = (CcGood)(spoiled + 1)) {
            place->stock[spoiled] = 0;
            place->reserve_target[spoiled] = 0;
            place->production[spoiled] = 0;
            place->consumption[spoiled] = 0;
            place->price[spoiled] =
                CcGoodDefinitionFor(spoiled)->base_price;
        }
    }
}

void CcSimInitializeMaterialChain(CcSim *sim)
{
    if (sim == NULL) return;
    sim->archives.kit_tool_wear = 0;
    for (int32_t i = 0; i < sim->kingdom_count; ++i) {
        CcKingdom *kingdom = &sim->kingdoms[i];
        kingdom->sanction = ClampI32(kingdom->legitimacy, 0, 100);
        kingdom->unsanctioned_weeks = 0;
        kingdom->pretender_crises = 0;
        kingdom->anointed = false;
    }
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        CcSettlement *place = &sim->settlements[i];
        place->paper_tool_wear = 0;
        place->production[CC_GOOD_PAPER] = 0;
        if (CcSettlementIsAbandoned(place)) continue;
        if (place->function == CC_SETTLEMENT_FARMING) {
            place->production[CC_GOOD_WHEAT] = MaximumI32(
                place->production[CC_GOOD_WHEAT], 60);
        } else if (place->function == CC_SETTLEMENT_MARKET) {
            place->service_mask |= ServiceBit(CC_SERVICE_MILL);
            place->production[CC_GOOD_PAPER] = 3;
            place->consumption[CC_GOOD_PAPER] = 0;
        } else if (place->function == CC_SETTLEMENT_CAPITAL) {
            place->service_mask |= ServiceBit(CC_SERVICE_MILL);
            place->production[CC_GOOD_PAPER] = 4;
        }
    }
}

void CcSimInitializeRoyalCarriages(CcSim *sim)
{
    if (sim == NULL) return;
    sim->royal_trade_week = sim->current_day / 7;
    for (int32_t route = 0; route < CC_MAX_ROUTES; ++route) {
        sim->royal_route_slots_used[route] = 0;
    }
    sim->royal_carriage_count = sim->kingdom_count;
    for (int32_t kingdom = 0; kingdom < sim->kingdom_count; ++kingdom) {
        CcId home_id = 0U;
        for (int32_t place = 0; place < sim->settlement_count; ++place) {
            if (sim->settlements[place].kingdom_id == sim->kingdoms[kingdom].id) {
                home_id = sim->settlements[place].id;
                break;
            }
        }
        sim->royal_carriages[kingdom] = (CcRoyalCarriage){
            .id = NextId(sim, CC_ENTITY_ROYAL_CARRIAGE),
            .kingdom_id = sim->kingdoms[kingdom].id,
            .location_id = home_id,
            .mode = CC_ROYAL_CARRIAGE_IDLE,
            .condition = 100
        };
    }
    for (int32_t shipment_slot = 0;
         shipment_slot < sim->shipment_count; ++shipment_slot) {
        CcShipment *shipment = &sim->shipments[shipment_slot];
        if (shipment->status != CC_SHIPMENT_TRAVELLING) continue;
        const CcSettlement *origin = CcSimSettlement(sim, shipment->origin_id);
        const CcSettlement *final = CcSimSettlement(
            sim, shipment->final_destination_id);
        int32_t kingdom_slot = final != NULL ?
            KingdomSlotById(sim, final->kingdom_id) :
            origin != NULL ? KingdomSlotById(sim, origin->kingdom_id) : -1;
        if (kingdom_slot < 0 ||
            sim->royal_carriages[kingdom_slot].active_shipment_id != 0U ||
            FreightCargoSlots(shipment->good, shipment->quantity) >
                CC_ROYAL_CARRIAGE_CARGO_SLOTS) {
            CcSettlement *destination = CcSimSettlementMutable(
                sim, shipment->final_destination_id);
            if (destination != NULL && shipment->good >= 0 &&
                shipment->good < CC_GOOD_COUNT && shipment->quantity > 0) {
                destination->stock[shipment->good] = ClampI32(
                    destination->stock[shipment->good] +
                        shipment->quantity,
                    0, CC_SIM_MAX_UNITS);
            }
            shipment->status = CC_SHIPMENT_ARRIVED;
            continue;
        }
        CcRoyalCarriage *carriage = &sim->royal_carriages[kingdom_slot];
        carriage->location_id = shipment->origin_id;
        carriage->route_id = shipment->route_id;
        carriage->destination_id = shipment->destination_id;
        carriage->target_id = shipment->final_destination_id;
        carriage->active_shipment_id = shipment->id;
        carriage->mode = CC_ROYAL_CARRIAGE_DELIVERING;
        carriage->departure_day = shipment->departure_day;
        carriage->arrival_day = shipment->arrival_day;
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
    sim->archives.scribes = 2;
    sim->archives.lore_ceiling = 75;

    static const char *kingdom_sets[][3] = {
        {"Alder Concord", "Ember Crown", "Star Oath"},
        {"Willow Republic", "Ashen Throne", "Indigo March"},
        {"Moss Compact", "Rose Dominion", "Silver Covenant"}
    };
    uint32_t name_set = NextRandom(sim) % 3U;
    CcMoney treasury = 470 + (CcMoney)(NextRandom(sim) % 151U);
    int32_t legitimacy = 55 + (int32_t)(NextRandom(sim) % 17U);
    InitKingdom(sim, 0, kingdom_sets[name_set][0], 54U, 173U, 146U,
                treasury, legitimacy);
    treasury = 470 + (CcMoney)(NextRandom(sim) % 151U);
    legitimacy = 48 + (int32_t)(NextRandom(sim) % 17U);
    InitKingdom(sim, 1, kingdom_sets[name_set][1], 210U, 101U, 71U,
                treasury, legitimacy);
    treasury = 470 + (CcMoney)(NextRandom(sim) % 151U);
    legitimacy = 58 + (int32_t)(NextRandom(sim) % 17U);
    InitKingdom(sim, 2, kingdom_sets[name_set][2], 102U, 123U, 205U,
                treasury, legitimacy);
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
    int32_t map_x = Jitter(sim, 125, 22);
    int32_t map_y = Jitter(sim, 500, 20);
    InitSettlement(sim, 0, 0, "Thornford", CC_SETTLEMENT_FARMING,
                   CC_SETTLEMENT_VILLAGE,
                   map_x, map_y, 1460, 58, 54);
    map_x = Jitter(sim, 355, 22);
    map_y = Jitter(sim, 445, 20);
    InitSettlement(sim, 1, 0, "Gloamgate", CC_SETTLEMENT_MARKET,
                   CC_SETTLEMENT_TOWN,
                   map_x, map_y, 2180, 62, 67);
    map_x = Jitter(sim, 535, 18);
    map_y = Jitter(sim, 325, 18);
    InitSettlement(sim, 2, 1, "Alderwatch", CC_SETTLEMENT_FORTRESS,
                   CC_SETTLEMENT_TOWN,
                   map_x, map_y, 1720, 82, 49);
    map_x = Jitter(sim, 755, 22);
    map_y = Jitter(sim, 455, 20);
    InitSettlement(sim, 3, 1, "Silverwick", CC_SETTLEMENT_MINING,
                   CC_SETTLEMENT_TOWN,
                   map_x, map_y, 2350, 43, 61);
    map_x = Jitter(sim, 770, 18);
    map_y = Jitter(sim, 145, 18);
    InitSettlement(sim, 4, 2, "Rosespire", CC_SETTLEMENT_CAPITAL,
                   CC_SETTLEMENT_CAPITAL_SIZE,
                   map_x, map_y, 3180, 71, 72);
    map_x = Jitter(sim, 335, 22);
    map_y = Jitter(sim, 155, 20);
    InitSettlement(sim, 5, 2, "Hollowbarrow", CC_SETTLEMENT_DUNGEON_TOWN,
                   CC_SETTLEMENT_VILLAGE,
                   map_x, map_y, 1280, 46, 45);
    sim->settlement_count = CC_MAX_SETTLEMENTS;
    ConfigureSettlementEconomies(sim);
    CcSimInitializeWoodEconomy(sim);
    CcSimInitializeStoneEconomy(sim);
    CcSimInitializePaperEconomy(sim);
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        SeedSettlementServices(&sim->settlements[i]);
    }
    CcSimInitializeMaterialChain(sim);

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
            int32_t power = 54 + (int32_t)(NextRandom(sim) % 25U);
            int32_t support = 42 + (int32_t)(NextRandom(sim) % 31U);
            InitFaction(sim, slot, kingdom, (CcFactionKind)local,
                        faction_name, power, support);
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
    CcSimInitializeUnderroad(sim);

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
    CcSimInitializePlayerRouteKnowledge(sim);
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
    CcId harvest_event_id = harvest->id;
    (void)snprintf(text, sizeof(text),
                   "%s closes the treaty bridge and delays the relief convoy.",
                   sim->settlements[2].name);
    CcEvent *bridge = PushEvent(sim, CC_EVENT_ROUTE_CLOSED, sim->routes[1].id,
                                sim->settlements[2].id, harvest_event_id,
                                54, text);
    CcId bridge_event_id = bridge->id;
    (void)snprintf(text, sizeof(text),
                   "Displaced workers reinforce %s on the old road.",
                   sim->bandits[0].name);
    (void)PushEvent(sim, CC_EVENT_BANDIT_PRESSURE, sim->bandits[0].id,
                    sim->routes[6].id, bridge_event_id,
                    sim->bandits[0].influence, text);
    (void)snprintf(text, sizeof(text),
                   "%s opens a sealed tunnel and disturbs the %s.",
                   sim->settlements[3].name, sim->monsters[0].name);
    (void)PushEvent(sim, CC_EVENT_MONSTER_PRESSURE, sim->monsters[0].id,
                    sim->dungeons[0].id, bridge_event_id,
                    sim->monsters[0].pressure, text);
    GenerateSituations(sim);
    CcSimInitializeCharacters(sim);
    CcSimUpgradeHistoryOffices(sim);
    CcSimInitializeAnimalEconomy(sim);
    CcSimInitializeRoadSites(sim);
    CcSimInitializeRoyalCarriages(sim);
    CcPoniesInit(sim);
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

bool CcSettlementCanRepairFire(const CcSettlement *place)
{
    return place != NULL && place->population > 0 &&
           place->fire_damage > 0 && place->hunger < 40 &&
           place->security >= 30 &&
           place->stock[CC_GOOD_WOOD] >= 2 &&
           place->stock[CC_GOOD_STONE] >= 1 &&
           place->stock[CC_GOOD_TOOLS] >= 1;
}

uint32_t CcSimTownConditions(const CcSim *sim, CcId settlement_id)
{
    const CcSettlement *place = CcSimSettlement(sim, settlement_id);
    if (place == NULL) return 0U;
    uint32_t conditions = place->fire_damage > 0 ? CC_TOWN_BURNT : 0U;
    if (CcSettlementIsAbandoned(place)) return conditions | CC_TOWN_ABANDONED;
    if (CcSettlementCanRepairFire(place) ||
        place->service_project != CC_SERVICE_NONE) conditions |= CC_TOWN_REBUILDING;
    if (place->security <= 25) conditions |= CC_TOWN_LAWLESS;
    if (place->security >= 60 &&
        CcSimWarBurdenAtSettlement(sim, place->id) < 20 &&
        MonsterPressureAtSettlement(sim, place->id) < 30) conditions |= CC_TOWN_PEACEFUL;
    if (place->prosperity >= 70 && place->hunger < 20) conditions |= CC_TOWN_THRIVING;
    if (place->hunger >= 40) conditions |= CC_TOWN_HUNGRY;
    return conditions;
}

static void RepairSettlementFire(CcSim *sim, CcSettlement *place)
{
    if (sim->schema_version < 45U || !CcSettlementCanRepairFire(place) ||
        sim->current_day - place->last_fire_day < 7) return;
    place->stock[CC_GOOD_WOOD] -= 2;
    place->stock[CC_GOOD_STONE] -= 1;
    place->stock[CC_GOOD_TOOLS] -= 1;
    place->fire_damage = MaximumI32(0, place->fire_damage - 10);
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(text, sizeof(text),
        "%s's builders use 2 Wood, 1 Stone, and 1 Tools to repair fire damage; %d%% remains.",
        place->name, place->fire_damage);
    (void)PushEvent(sim, CC_EVENT_MASONRY_REPAIR, place->id, place->id,
                    LatestLocalCause(sim, place->id), 10, text);
}

static int32_t GrainSeasonFactor(const CcSim *sim)
{
    int32_t week = (sim->current_day / 7) % 52;
    if (week < 13) return 72;
    if (week < 26) return 112;
    if (week < 39) return 148;
    return 58;
}

static int32_t EffectiveProduction(const CcSim *sim,
                                   const CcSettlement *settlement,
                                   int32_t index, CcGood good)
{
    if (CcSettlementIsAbandoned(settlement)) return 0;
    bool legacy_food_economy = sim->schema_version < 29U;
    int32_t production = settlement->production[good];
    CcGood staple = legacy_food_economy ? CC_GOOD_BREAD : CC_GOOD_WHEAT;
    bool subsistence_muster = good == staple &&
        (settlement->hunger > 65 ||
         (settlement->population < 600 && settlement->hunger >= 20));
    int32_t subsistence_food = subsistence_muster ?
        MaximumI32(1, CivilianFoodUse(settlement) * 2 / 3) : 0;
    if (production <= 0 && subsistence_food <= 0) return 0;
    if (settlement->hunger > 65) production = production * 72 / 100;
    else if (settlement->hunger > 35) production = production * 86 / 100;

    if (!legacy_food_economy && good == CC_GOOD_BREAD) return 0;
    if (good == staple) {
        if (!CcSettlementHasService(settlement, CC_SERVICE_FARM) ||
            settlement->field_yield <= 0) return subsistence_food;
        production = production * GrainSeasonFactor(sim) / 100;
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
    if (legacy_food_economy && good == CC_GOOD_BREAD) {
        return MaximumI32(0, production);
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
    if (good == CC_GOOD_WOOD && settlement->stock[CC_GOOD_TOOLS] <= 0) {
        production = MaximumI32(1, production / 4);
    }
    if (good == CC_GOOD_STONE) {
        if (!CcSettlementHasService(settlement, CC_SERVICE_MINE)) return 0;
        if (settlement->stock[CC_GOOD_TOOLS] <= 0) {
            production = MaximumI32(1, production / 4);
        }
    }
    if (legacy_food_economy && good >= CC_GOOD_TOOLS) return 0;
    if (good != CC_GOOD_WHEAT && good != CC_GOOD_IRON &&
        good != CC_GOOD_WOOD && good != CC_GOOD_STONE) return 0;
    return MaximumI32(0, production);
}

bool CcSimFoodEconomyAtSettlement(const CcSim *sim, CcId settlement_id,
                                  CcFoodEconomy *economy)
{
    if (sim == NULL || economy == NULL) return false;
    const CcSettlement *settlement = CcSimSettlement(sim, settlement_id);
    if (settlement == NULL) return false;
    int32_t slot = SettlementSlotById(sim, settlement_id);
    if (slot < 0) return false;
    int32_t grain_production = EffectiveProduction(
        sim, settlement, slot, CC_GOOD_WHEAT);
    int32_t dairy_nutrition = sim->schema_version >= 14U &&
        CcSettlementHasService(settlement, CC_SERVICE_FARM) &&
        settlement->cow_adults > 0 ?
        MaximumI32(
            1, settlement->cow_adults * settlement->cow_condition / 1200) *
            CC_NUTRITION_PER_RATION :
        0;
    int32_t bakery_input = MinimumI32(
        BakeryCapacity(settlement),
        settlement->stock[CC_GOOD_WHEAT] + grain_production);
    int32_t production_nutrition =
        bakery_input * CC_NUTRITION_PER_RATION +
        MaximumI32(0, grain_production - bakery_input) +
        dairy_nutrition;
    int32_t storage_nutrition =
        NutritionStorageCapacity(sim, settlement, CC_GOOD_BREAD) *
            CC_NUTRITION_PER_RATION +
        NutritionStorageCapacity(sim, settlement, CC_GOOD_WHEAT) +
        NutritionStorageCapacity(sim, settlement, CC_GOOD_MEAT) *
            CC_NUTRITION_PER_RATION;
    *economy = (CcFoodEconomy){
        .stock = NutritionRations(
            settlement->stock, CC_NUTRITION_CIVILIAN),
        .incoming = IncomingNutrition(
            sim, settlement_id, CC_NUTRITION_CIVILIAN) /
            CC_NUTRITION_PER_RATION,
        .weekly_production = production_nutrition /
            CC_NUTRITION_PER_RATION,
        .weekly_consumption = WeeklyFoodUse(sim, settlement),
        .reserve_target = EffectiveReserveTarget(
            sim, settlement, CC_GOOD_BREAD) +
            settlement->reserve_target[CC_GOOD_WHEAT] /
                CC_NUTRITION_PER_RATION,
        .storage_capacity = storage_nutrition /
            CC_NUTRITION_PER_RATION,
        .unit_price = settlement->price[CC_GOOD_BREAD],
        .hunger = settlement->hunger
    };
    return true;
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
    for (int32_t i = 0; i < sim->treasure_count; ++i) {
        if (sim->treasures[i].destroyed) {
            CcTreasure *treasure = &sim->treasures[i];
            *treasure = (CcTreasure){0};
            treasure->id = NextId(sim, CC_ENTITY_TREASURE);
            return treasure;
        }
    }
    if (sim->treasure_count >= CC_MAX_TREASURES) return NULL;
    CcTreasure *treasure = &sim->treasures[sim->treasure_count++];
    *treasure = (CcTreasure){0};
    treasure->id = NextId(sim, CC_ENTITY_TREASURE);
    return treasure;
}

static bool TreasureIsArchiveVolume(const CcTreasure *treasure)
{
    if (treasure == NULL || treasure->destroyed) return false;
    return strncmp(treasure->name, "Chronicle ", 10) == 0 ||
           strncmp(treasure->name, "Ledger ", 7) == 0 ||
           strncmp(treasure->name, "Annal ", 6) == 0 ||
           strncmp(treasure->name, "Register ", 9) == 0 ||
           strncmp(treasure->name, "Codex of ", 9) == 0;
}

int32_t CcSimArchivePhysicalLore(const CcSim *sim)
{
    if (sim == NULL) return 0;
    int64_t lore = 0;
    for (int32_t i = 0; i < sim->treasure_count; ++i) {
        const CcTreasure *volume = &sim->treasures[i];
        if (TreasureIsArchiveVolume(volume)) lore += volume->craft_work;
    }
    return lore > INT32_MAX ? INT32_MAX : (int32_t)lore;
}

void CcSimUpgradeArchivePhysicalLore(CcSim *sim)
{
    if (sim == NULL) return;
    sim->archives.lore_stored = CcSimArchivePhysicalLore(sim);
}

static bool EarlierArchiveVolume(const CcSim *sim, int32_t first,
                                 int32_t second)
{
    if (second < 0) return true;
    const CcTreasure *a = &sim->treasures[first];
    const CcTreasure *b = &sim->treasures[second];
    return a->created_day < b->created_day ||
        (a->created_day == b->created_day && first < second);
}

static bool FindArchiveVolumesToBind(const CcSim *sim, int32_t slots[4])
{
    int32_t best[4] = {-1, -1, -1, -1};
    for (int32_t anchor = 0; anchor < sim->treasure_count; ++anchor) {
        const CcTreasure *volume = &sim->treasures[anchor];
        if (!TreasureIsArchiveVolume(volume) ||
            volume->owner_id == sim->player.id) continue;

        int32_t candidate[4] = {-1, -1, -1, -1};
        for (int32_t i = 0; i < sim->treasure_count; ++i) {
            const CcTreasure *other = &sim->treasures[i];
            if (!TreasureIsArchiveVolume(other) ||
                other->owner_id == sim->player.id ||
                other->owner_id != volume->owner_id ||
                other->location_id != volume->location_id) continue;
            for (int32_t position = 0; position < 4; ++position) {
                if (!EarlierArchiveVolume(sim, i, candidate[position])) {
                    continue;
                }
                for (int32_t move = 3; move > position; --move) {
                    candidate[move] = candidate[move - 1];
                }
                candidate[position] = i;
                break;
            }
        }
        if (candidate[3] < 0) continue;
        if (best[0] < 0 ||
            EarlierArchiveVolume(sim, candidate[0], best[0])) {
            for (int32_t i = 0; i < 4; ++i) best[i] = candidate[i];
        }
    }
    if (best[0] < 0) return false;
    for (int32_t i = 0; i < 4; ++i) slots[i] = best[i];
    return true;
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
    bool legacy_smithy = sim->schema_version < 27U;
    int32_t iron_before = settlement->stock[CC_GOOD_IRON];
    int32_t wood_before = settlement->stock[CC_GOOD_WOOD];
    int32_t tools_made = 0;
    int32_t tool_gap = MaximumI32(0, settlement->reserve_target[CC_GOOD_TOOLS] * 2 -
                                     settlement->stock[CC_GOOD_TOOLS]);
    int32_t tool_capacity = MaximumI32(0, settlement->production[CC_GOOD_TOOLS]);
    int32_t tool_material = settlement->stock[CC_GOOD_IRON] / 2;
    if (!legacy_smithy) {
        tool_material = MinimumI32(
            tool_material, settlement->stock[CC_GOOD_WOOD]);
    }
    tools_made = MinimumI32(
        tool_capacity, MinimumI32(tool_gap, tool_material));
    settlement->stock[CC_GOOD_IRON] -= tools_made * 2;
    if (!legacy_smithy) settlement->stock[CC_GOOD_WOOD] -= tools_made;
    settlement->stock[CC_GOOD_TOOLS] += tools_made;

    int32_t weapons_made = 0;
    int32_t weapon_gap = MaximumI32(0,
        EffectiveReserveTarget(sim, settlement, CC_GOOD_WEAPONS) * 2 -
        settlement->stock[CC_GOOD_WEAPONS]);
    int32_t weapon_capacity = MaximumI32(
        0, settlement->production[CC_GOOD_WEAPONS]);
    int32_t weapon_material = settlement->stock[CC_GOOD_IRON] / 3;
    if (!legacy_smithy) {
        weapon_material = MinimumI32(
            weapon_material, settlement->stock[CC_GOOD_WOOD] / 2);
    }
    weapons_made = MinimumI32(
        weapon_capacity, MinimumI32(weapon_gap, weapon_material));
    settlement->stock[CC_GOOD_IRON] -= weapons_made * 3;
    if (!legacy_smithy) settlement->stock[CC_GOOD_WOOD] -= weapons_made * 2;
    settlement->stock[CC_GOOD_WEAPONS] += weapons_made;

    int32_t smith_batches = tools_made + weapons_made;
    for (int32_t batch = 0; batch < smith_batches; ++batch) {
        WearOneTool(settlement, &settlement->smith_tool_wear, 4);
    }
    if (smith_batches > 0) {
        char text[CC_EVENT_TEXT_CAPACITY];
        if (legacy_smithy) {
            (void)snprintf(
                text, sizeof(text),
                "%s's smithy turns %d Iron into %d Tools and %d Weapons.",
                settlement->name,
                iron_before - settlement->stock[CC_GOOD_IRON],
                tools_made, weapons_made);
        } else {
            (void)snprintf(
                text, sizeof(text),
                "%s's smithy uses %d Iron and %d Wood to make %d Tools and %d Weapons.",
                settlement->name,
                iron_before - settlement->stock[CC_GOOD_IRON],
                wood_before - settlement->stock[CC_GOOD_WOOD],
                tools_made, weapons_made);
        }
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

static void RunPaperMill(CcSim *sim, CcSettlement *settlement)
{
    if (sim == NULL || sim->schema_version < 33U) return;
    if (sim->schema_version < 34U) {
        int32_t capacity = MaximumI32(
            0, settlement->production[CC_GOOD_PAPER]);
        int32_t gap = MaximumI32(
            0, settlement->reserve_target[CC_GOOD_PAPER] * 2 -
               settlement->stock[CC_GOOD_PAPER]);
        int32_t wood_available = MaximumI32(
            0, settlement->stock[CC_GOOD_WOOD] -
               settlement->reserve_target[CC_GOOD_WOOD]);
        int32_t paper_made = MinimumI32(
            capacity, MinimumI32(gap, wood_available * 4));
        if (paper_made <= 0) return;
        int32_t wood_used = (paper_made + 3) / 4;
        settlement->stock[CC_GOOD_WOOD] -= wood_used;
        settlement->stock[CC_GOOD_PAPER] += paper_made;
        RefreshSettlementGoodPrice(sim, settlement, CC_GOOD_WOOD);
        RefreshSettlementGoodPrice(sim, settlement, CC_GOOD_PAPER);
        return;
    }
    if (!CcSettlementHasService(settlement, CC_SERVICE_MILL) ||
        settlement->hunger > 0 ||
        settlement->stock[CC_GOOD_TOOLS] <= 0) return;
    /* Keep mill work behind the town's food buffer, across all edible goods. */
    if (sim->schema_version >= 37U &&
        NutritionRations(settlement->stock, CC_NUTRITION_CIVILIAN) <
            WeeklyFoodUse(sim, settlement) * 4) return;
    int32_t capacity = MaximumI32(
        0, settlement->production[CC_GOOD_PAPER]);
    int32_t gap = MaximumI32(
        0, settlement->reserve_target[CC_GOOD_PAPER] * 2 -
           settlement->stock[CC_GOOD_PAPER]);
    CcGood input = sim->schema_version < 37U ?
        CC_GOOD_WHEAT : CC_GOOD_WOOD;
    int32_t protected_input = settlement->reserve_target[input];
    if (input == CC_GOOD_WHEAT) {
        protected_input += WeeklyFoodUse(sim, settlement) * 4;
    }
    int32_t input_available = MaximumI32(
        0, settlement->stock[input] - protected_input);
    int32_t paper_made = MinimumI32(
        capacity, MinimumI32(gap, input_available * 4));
    if (paper_made <= 0) return;
    int32_t input_used = (paper_made + 3) / 4;
    settlement->stock[input] -= input_used;
    settlement->stock[CC_GOOD_PAPER] += paper_made;
    WearOneTool(settlement, &settlement->paper_tool_wear, 8);
    RefreshSettlementGoodPrice(sim, settlement, input);
    RefreshSettlementGoodPrice(sim, settlement, CC_GOOD_PAPER);
    RefreshSettlementGoodPrice(sim, settlement, CC_GOOD_TOOLS);
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(
        text, sizeof(text),
        "%s's mill uses %d %s to make %d Paper.",
        settlement->name, input_used, CcGoodName(input), paper_made);
    (void)PushEvent(
        sim, CC_EVENT_PAPER_MILLED, settlement->id, settlement->id,
        LatestLocalCause(sim, settlement->id), paper_made, text);
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
    int32_t feed_eaten;
    if (sim->schema_version < 29U) {
        feed_eaten = MinimumI32(
            feed_required, settlement->stock[CC_GOOD_BREAD]);
        settlement->stock[CC_GOOD_BREAD] -= feed_eaten;
    } else {
        feed_eaten = CcNutritionConsume(
            settlement->stock, CC_NUTRITION_ANIMAL,
            feed_required * CC_NUTRITION_PER_RATION) /
            CC_NUTRITION_PER_RATION;
    }
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
        settlement->cow_hunger = ClampI32(
            settlement->cow_hunger - 12, 0, 100);
        char text[CC_EVENT_TEXT_CAPACITY];
        if (sim->schema_version >= 32U) {
            int32_t beef = MinimumI32(
                4, CC_SIM_MAX_UNITS - settlement->stock[CC_GOOD_MEAT]);
            settlement->stock[CC_GOOD_MEAT] += beef;
            (void)snprintf(
                text, sizeof(text),
                "%s slaughters one cow after the herd's fodder runs short; %d Meat enters the local store as beef.",
                settlement->name, beef);
            (void)PushEvent(
                sim, CC_EVENT_COW_SLAUGHTERED, settlement->id,
                settlement->id, LatestLocalCause(sim, settlement->id),
                beef, text);
        } else {
            settlement->stock[CC_GOOD_FOOD] += 4;
            (void)snprintf(
                text, sizeof(text),
                "%s slaughters one cow after the herd's fodder runs short; 4 Food enters the local store.",
                settlement->name);
            (void)PushEvent(
                sim, CC_EVENT_COW_SLAUGHTERED, settlement->id,
                settlement->id, LatestLocalCause(sim, settlement->id),
                1, text);
        }
    }

    if (settlement->cow_adults <= 0) return 0;
    int32_t dairy_rations = MaximumI32(
        1, settlement->cow_adults * settlement->cow_condition / 1200);
    return sim->schema_version >= 32U ?
        dairy_rations * CC_NUTRITION_PER_RATION : dairy_rations;
}

static int32_t SheepCapacity(const CcSettlement *settlement)
{
    return MaximumI32(12, settlement->population / 20);
}

static int32_t StoreMutton(CcSettlement *settlement, int32_t sheep)
{
    int32_t wanted = sheep * 2;
    int32_t stored = MinimumI32(
        wanted, CC_SIM_MAX_UNITS - settlement->stock[CC_GOOD_MEAT]);
    settlement->stock[CC_GOOD_MEAT] += stored;
    return stored;
}

static void RecordSheepCull(CcSim *sim, CcSettlement *settlement,
                            int32_t sheep, int32_t meat,
                            const char *reason)
{
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(
        text, sizeof(text),
        "%s slaughters %d sheep %s; %d Meat enters the local store as mutton.",
        settlement->name, sheep, reason, meat);
    (void)PushEvent(
        sim, CC_EVENT_SHEEP_SLAUGHTERED, settlement->id, settlement->id,
        0U, meat, text);
}

static void AdvanceSheepFlock(CcSim *sim, CcSettlement *settlement)
{
    if (sim->schema_version < 32U ||
        !CcSettlementHasService(settlement, CC_SERVICE_FARM)) return;
    int32_t flock = settlement->sheep_adults + settlement->sheep_lambs;
    if (flock <= 0) return;

    if (IsWinter(sim)) {
        int32_t feed_required = MaximumI32(1, (flock + 23) / 24);
        int32_t feed_eaten = CcNutritionConsume(
            settlement->stock, CC_NUTRITION_ANIMAL,
            feed_required * CC_NUTRITION_PER_RATION) /
            CC_NUTRITION_PER_RATION;
        int32_t feed_shortfall = feed_required - feed_eaten;
        if (feed_shortfall > 0) {
            settlement->sheep_hunger = ClampI32(
                settlement->sheep_hunger + feed_shortfall * 14, 0, 100);
            settlement->sheep_condition = ClampI32(
                settlement->sheep_condition - feed_shortfall * 4, 1, 100);
        } else {
            settlement->sheep_hunger = ClampI32(
                settlement->sheep_hunger - 12, 0, 100);
            settlement->sheep_condition = ClampI32(
                settlement->sheep_condition + 2, 1, 100);
        }
    } else {
        settlement->sheep_hunger = ClampI32(
            settlement->sheep_hunger - 8, 0, 100);
        settlement->sheep_condition = ClampI32(
            settlement->sheep_condition + 2, 1, 100);
    }

    int32_t year_day = sim->current_day % 364;
    if (year_day == 91 && settlement->sheep_adults > 0) {
        int32_t wool = MaximumI32(1, settlement->sheep_adults / 4);
        wool = MinimumI32(
            wool, CC_SIM_MAX_UNITS - settlement->stock[CC_GOOD_WOOL]);
        settlement->stock[CC_GOOD_WOOL] += wool;
        if (wool > 0) {
            char text[CC_EVENT_TEXT_CAPACITY];
            (void)snprintf(
                text, sizeof(text),
                "%s shears the spring flock and stores %d Wool.",
                settlement->name, wool);
            (void)PushEvent(
                sim, CC_EVENT_SHEEP_SHEARED, settlement->id,
                settlement->id, 0U, wool, text);
        }

        if (settlement->sheep_condition >= 65 &&
            settlement->sheep_hunger <= 30) {
            int32_t matured = settlement->sheep_lambs;
            settlement->sheep_lambs = 0;
            settlement->sheep_adults += matured;
            int32_t room = MaximumI32(
                0, SheepCapacity(settlement) - settlement->sheep_adults);
            int32_t births = MinimumI32(
                room, MaximumI32(1, settlement->sheep_adults / 5));
            settlement->sheep_lambs += births;
            if (births > 0) {
                char text[CC_EVENT_TEXT_CAPACITY];
                (void)snprintf(
                    text, sizeof(text),
                    "%s's flock raises %d new lamb%s; %d yearling%s join the adult flock.",
                    settlement->name, births, births == 1 ? "" : "s",
                    matured, matured == 1 ? "" : "s");
                (void)PushEvent(
                    sim, CC_EVENT_SHEEP_BRED, settlement->id,
                    settlement->id, 0U, births, text);
            }
        }
    }

    flock = settlement->sheep_adults + settlement->sheep_lambs;
    int32_t capacity = SheepCapacity(settlement);
    if (year_day == 273 && settlement->sheep_adults > 8 &&
        flock >= capacity * 3 / 4) {
        int32_t planned = MaximumI32(1, settlement->sheep_adults / 12);
        planned = MinimumI32(planned, settlement->sheep_adults - 8);
        settlement->sheep_adults -= planned;
        int32_t meat = StoreMutton(settlement, planned);
        RecordSheepCull(
            sim, settlement, planned, meat, "in the autumn cull");
    }

    if (settlement->sheep_hunger >= 65 &&
        settlement->sheep_adults > 0) {
        settlement->sheep_adults -= 1;
        int32_t meat = StoreMutton(settlement, 1);
        settlement->sheep_hunger = ClampI32(
            settlement->sheep_hunger - 14, 0, 100);
        RecordSheepCull(
            sim, settlement, 1, meat, "after winter fodder runs short");
    }
}

static void MaintainSettlementStonework(CcSim *sim,
                                        CcSettlement *settlement)
{
    if (sim->current_day % 112 != 0) return;
    int32_t service_count = CcSettlementServiceCount(settlement);
    if (service_count <= 0) return;
    int32_t stone_cost = MaximumI32(1, (service_count + 3) / 4);
    bool repaired = settlement->stock[CC_GOOD_STONE] >= stone_cost;
    if (repaired) {
        settlement->stock[CC_GOOD_STONE] -= stone_cost;
        settlement->prosperity = ClampI32(
            settlement->prosperity + 1, 0, 100);
    } else {
        settlement->prosperity = ClampI32(
            settlement->prosperity - 1, 0, 100);
    }
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(
        text, sizeof(text),
        repaired ?
            "%s's masons use %d Stone to repair public buildings." :
            "%s defers public masonry repairs for want of %d Stone.",
        settlement->name, stone_cost);
    (void)PushEvent(
        sim, CC_EVENT_MASONRY_REPAIR, settlement->id, settlement->id,
        LatestLocalCause(sim, settlement->id), repaired ? stone_cost : 0,
        text);
}

/* The fall of a town is not the end of its people: the remaining
 * households walk to the strongest living host, preferring their own
 * kingdom and near neighbors, and the named cast flees with them. */
/* Office holders (rulers, monastery patrons) must keep their home inside
 * the kingdom they serve; they may only flee within their own country. */
static bool CharacterHoldsOffice(const CcSim *sim, CcId character_id)
{
    if (sim == NULL || character_id == 0U) return false;
    for (int32_t i = 0; i < sim->kingdom_count; ++i) {
        if (sim->kingdoms[i].ruler_character_id == character_id ||
            sim->kingdoms[i].monastery_patron_id == character_id) {
            return true;
        }
    }
    return false;
}

static CcSettlement *FallenTownHost(CcSim *sim, const CcSettlement *ruin,
                                    CcId kingdom_requirement)
{
    if (sim == NULL || ruin == NULL) return NULL;
    CcSettlement *host = NULL;
    int32_t host_score = INT32_MIN;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        CcSettlement *place = &sim->settlements[i];
        if (place == ruin || CcSettlementIsAbandoned(place)) continue;
        if (kingdom_requirement != 0U &&
            place->kingdom_id != kingdom_requirement) continue;
        int32_t score = place->prosperity - place->hunger;
        if (place->kingdom_id == ruin->kingdom_id) score += 200;
        for (int32_t route = 0; route < sim->route_count; ++route) {
            const CcRoute *road = &sim->routes[route];
            if ((road->from_id == ruin->id && road->to_id == place->id) ||
                (road->to_id == ruin->id && road->from_id == place->id)) {
                score += 100;
                break;
            }
        }
        if (host == NULL || score > host_score) {
            host = place;
            host_score = score;
        }
    }
    return host;
}

static void RelocateFallenTown(CcSim *sim, CcSettlement *ruin,
                               int32_t households, CcId cause_event_id)
{
    if (sim == NULL || ruin == NULL || households <= 0) return;
    CcSettlement *host = FallenTownHost(sim, ruin, 0U);
    if (host == NULL) return;
    host->population += households;
    host->hunger = ClampI32(
        host->hunger + MinimumI32(15, households / 5), 0, 100);
    int32_t relocated = 0;
    for (int32_t i = 0; i < sim->character_count; ++i) {
        CcCharacter *person = &sim->characters[i];
        if (person->home_settlement_id != ruin->id) continue;
        CcSettlement *seat = host;
        if (CharacterHoldsOffice(sim, person->id) &&
            host->kingdom_id != ruin->kingdom_id) {
            seat = FallenTownHost(sim, ruin, ruin->kingdom_id);
            if (seat == NULL) continue; /* the crown stays in its country */
        }
        person->home_settlement_id = seat->id;
        person->current_settlement_id = seat->id;
        person->role = CC_CHARACTER_REFUGEE;
        person->goal = CC_CHARACTER_GOAL_SURVIVE_CRISIS;
        person->activity = CC_CHARACTER_ACTIVITY_SEEKING_AID;
        relocated += 1;
    }
    if (relocated == 0) return;
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(
        text, sizeof(text),
        "The fall of %s drives %d households and %d named residents to %s.",
        ruin->name, households, relocated, host->name);
    (void)PushEvent(
        sim, CC_EVENT_KINGDOM_ACTION, host->id, host->id,
        cause_event_id != 0U ? cause_event_id :
            LatestLocalCause(sim, host->id), households, text);
}

static void UpdateSettlement(CcSim *sim, int32_t index,
                             CcId scriptorium_id)
{
    CcSettlement *settlement = &sim->settlements[index];
    if (CcSettlementIsAbandoned(settlement)) return;
    int32_t produced[CC_GOOD_COUNT] = {0};
    int32_t cow_output = AdvanceCowHerd(sim, settlement);
    AdvanceSheepFlock(sim, settlement);
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        int32_t production = EffectiveProduction(sim, settlement, index, (CcGood)good);
        produced[good] = production;
        if ((CcGood)good == CC_GOOD_IRON) {
            settlement->iron_deposit -= production;
        }
        int64_t replenished = (int64_t)settlement->stock[good] +
                              (int64_t)production;
        settlement->stock[good] = replenished > CC_SIM_MAX_UNITS ?
                                  CC_SIM_MAX_UNITS :
                                  replenished < 0 ? 0 :
                                  (int32_t)replenished;
    }
    if (sim->schema_version < 32U) {
        int32_t cow_bread = MinimumI32(
            cow_output,
            CC_SIM_MAX_UNITS - settlement->stock[CC_GOOD_BREAD]);
        settlement->stock[CC_GOOD_BREAD] += cow_bread;
        produced[CC_GOOD_BREAD] += cow_bread;
    }
    produced[CC_GOOD_BREAD] += RunBakery(
        sim, settlement, scriptorium_id);

    int32_t food_required = MaximumI32(
        1, WeeklyFoodUse(sim, settlement)) * CC_NUTRITION_PER_RATION;
    int32_t food_eaten = sim->schema_version >= 32U ?
        MinimumI32(food_required, cow_output) : 0;
    food_eaten += CcNutritionConsume(
        settlement->stock, CC_NUTRITION_CIVILIAN,
        food_required - food_eaten);
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        if (CcGoodNutritionValue(
                (CcGood)good, CC_NUTRITION_CIVILIAN) > 0) continue;
        int32_t consumption = settlement->consumption[good] +
            WarExtraConsumption(sim, settlement, (CcGood)good);
        if ((CcGood)good == CC_GOOD_WOOL) {
            consumption += CivilianWoolUse(sim, settlement);
        }
        int32_t consumed = MinimumI32(settlement->stock[good], consumption);
        settlement->stock[good] -= consumed;
    }
    (void)SpoilStoredNutrition(sim, settlement);
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        RefreshSettlementGoodPrice(sim, settlement, (CcGood)good);
    }
    CcGood farm_output = sim->schema_version < 29U ?
        CC_GOOD_BREAD : CC_GOOD_WHEAT;
    if (produced[farm_output] > 0 &&
        CcSettlementHasService(settlement, CC_SERVICE_FARM) &&
        settlement->field_yield > 0) {
        WearOneTool(settlement, &settlement->farm_tool_wear, 4);
    }
    if (produced[CC_GOOD_IRON] > 0) {
        AdvanceRareMineWork(sim, settlement, produced[CC_GOOD_IRON]);
        WearOneTool(settlement, &settlement->mine_tool_wear, 3);
    }
    if (produced[CC_GOOD_WOOD] > 0 && sim->current_day % 84 == 0) {
        bool used_tools = settlement->stock[CC_GOOD_TOOLS] > 0;
        if (used_tools) settlement->stock[CC_GOOD_TOOLS] -= 1;
        char text[CC_EVENT_TEXT_CAPACITY];
        (void)snprintf(
            text, sizeof(text),
            used_tools ?
                "%s's woodlot cuts %d Wood and wears one Tools bundle." :
                "%s's hand crews cut %d Wood without working tools.",
            settlement->name, produced[CC_GOOD_WOOD]);
        (void)PushEvent(
            sim, CC_EVENT_WOODLOT_HARVEST, settlement->id, settlement->id,
            LatestLocalCause(sim, settlement->id),
            produced[CC_GOOD_WOOD], text);
    }
    if (produced[CC_GOOD_STONE] > 0 && sim->current_day % 28 == 0) {
        char text[CC_EVENT_TEXT_CAPACITY];
        (void)snprintf(
            text, sizeof(text),
            "%s's quarry cuts %d Stone for roads and public works.",
            settlement->name, produced[CC_GOOD_STONE]);
        (void)PushEvent(
            sim, CC_EVENT_QUARRY_OUTPUT, settlement->id, settlement->id,
            LatestLocalCause(sim, settlement->id),
            produced[CC_GOOD_STONE], text);
    }
    MaintainSettlementStonework(sim, settlement);
    RunSmithy(sim, settlement);
    RunPaperMill(sim, settlement);
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
    int32_t coverage = NutritionRations(
        settlement->stock, CC_NUTRITION_CIVILIAN) / food_use;
    int32_t food_unmet = MaximumI32(0, food_required - food_eaten);
    bool legacy_food_economy = sim->schema_version < 29U;
    int32_t hunger_delta = food_unmet > 0 ?
        2 + food_unmet * 4 / food_required :
        legacy_food_economy ?
            (coverage >= 8 ? -4 : coverage >= 5 ? -2 : -1) :
            (coverage >= 8 ? -6 : coverage >= 5 ? -4 : -2);
    if (!legacy_food_economy && food_unmet == 0 &&
        settlement->hunger < 50) hunger_delta -= 1;
    settlement->hunger = ClampI32(settlement->hunger + hunger_delta, 0, 100);
    if (CcSettlementHasService(settlement, CC_SERVICE_HEALER)) {
        settlement->hunger = ClampI32(settlement->hunger - 1, 0, 100);
    }
    bool prosperity_recovering =
        (coverage >= 8 && settlement->prosperity < 78) ||
        (settlement->hunger < 20 && coverage >= 3 &&
         settlement->prosperity < 45);
    int32_t prosperity_change = settlement->hunger > 55 ? -2 :
                                settlement->hunger > 30 ? -1 :
                                prosperity_recovering ? 1 : 0;
    settlement->prosperity = ClampI32(
        settlement->prosperity + prosperity_change, 0, 100);
    if (CcSettlementHasService(settlement, CC_SERVICE_MARKET) &&
        settlement->hunger < 45 && settlement->prosperity < 78) {
        settlement->prosperity = ClampI32(settlement->prosperity + 1, 0, 100);
    }
    int32_t local_threat = MonsterPressureAtSettlement(sim, settlement->id);
    bool security_recovering =
        (settlement->security < 45 && settlement->hunger < 35) ||
        (settlement->prosperity > 70 && settlement->hunger < 15 &&
         settlement->security < 80);
    int32_t security_change = settlement->hunger > 50 || local_threat > 60 ? -1 :
                              security_recovering ? 1 : 0;
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
            int32_t refugee_households = settlement->population;
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
            CcEvent *abandoned = PushEvent(
                sim, CC_EVENT_KINGDOM_ACTION, settlement->id,
                settlement->id, LatestLocalCause(sim, settlement->id),
                -1, text);
            RelocateFallenTown(
                sim, settlement, refugee_households,
                abandoned != NULL ? abandoned->id : 0U);
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
    RepairSettlementFire(sim, settlement);
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

static bool EventWasArchived(const CcSim *sim, CcId event_id)
{
    for (int32_t i = 0; i < sim->event_count; ++i) {
        const CcEvent *event = CcSimRecentEvent(sim, i);
        if (event != NULL && event->kind == CC_EVENT_LORE_RECORDED &&
            event->parent_id == event_id) return true;
    }
    return false;
}

static bool IsNotableGossip(const CcSim *sim, const CcEvent *event)
{
    return event->magnitude >= 20 &&
        (event->kind == CC_EVENT_WAR_DECLARED ||
         event->kind == CC_EVENT_PEACE_DECLARED ||
         event->kind == CC_EVENT_DRAGON_SLAIN ||
         event->kind == CC_EVENT_DRAGON_CROWNED ||
         event->kind == CC_EVENT_DRAGON_BROOD ||
         event->kind == CC_EVENT_TREASURE_CRAFTED ||
         event->kind == CC_EVENT_KINGDOM_ACTION ||
         event->kind == CC_EVENT_SETTLEMENT_RAIDED ||
         event->kind == CC_EVENT_CHARACTER_DIED ||
         (sim->schema_version >= 46U &&
          (event->kind == CC_EVENT_HARVEST_FAILED || event->kind == CC_EVENT_ROUTE_CLOSED ||
           event->kind == CC_EVENT_BANDIT_PRESSURE || event->kind == CC_EVENT_MONSTER_PRESSURE)));
}

static void GatherGossip(CcSim *sim)
{
    if (sim->schema_version < 44U) return;
    const CcEvent *newest = CcSimRecentEvent(sim, 0);
    if (newest == NULL || newest->id <= sim->gossip_last_event_id) return;
    CcId latest = sim->gossip_last_event_id;
    for (int32_t offset = sim->event_count - 1; offset >= 0; --offset) {
        const CcEvent *event = CcSimRecentEvent(sim, offset);
        if (event == NULL || event->id <= sim->gossip_last_event_id ||
            event->day > sim->current_day) continue;
        if (event->id > latest) latest = event->id;
        if (!IsNotableGossip(sim, event) || EventWasArchived(sim, event->id)) continue;
        int32_t origin = SettlementSlotById(sim, event->location_id);
        if (origin < 0) {
            const CcRoute *route = CcSimRoute(sim, event->location_id);
            if (route != NULL) origin = SettlementSlotById(sim, route->from_id);
        }
        if (origin < 0 || CcSettlementIsAbandoned(&sim->settlements[origin])) {
            continue;
        }
        int32_t slot = 0;
        for (int32_t i = 0; i < CC_MAX_GOSSIP; ++i) {
            if (sim->gossip[i].event_id == 0U) { slot = i; break; }
            if ((sim->gossip[i].recorded && !sim->gossip[slot].recorded) ||
                (sim->gossip[i].recorded == sim->gossip[slot].recorded &&
                 sim->gossip[i].event_id < sim->gossip[slot].event_id)) slot = i;
        }
        uint32_t bit = UINT32_C(1) << (uint32_t)slot;
        for (int32_t i = 0; i < CcSimGossipCarrierCapacity(sim); ++i) {
            sim->gossip_carriers[i].stories &= ~bit;
            sim->gossip_carriers[i].versions[slot] = (CcGossipVersion){0};
        }
        sim->gossip[slot] = (CcGossip){
            .event_id = event->id, .origin_id = sim->settlements[origin].id,
            .day = event->day,
            .kind = event->kind,
            .settlement_mask = UINT32_C(1) << (uint32_t)origin
        };
        sim->gossip[slot].local[origin].confidence = 100;
        (void)snprintf(sim->gossip[slot].text,
                       sizeof(sim->gossip[slot].text), "%s", event->text);
    }
    sim->gossip_last_event_id = latest;
}

int32_t CcSimGossipCarrierCapacity(const CcSim *sim)
{
    return sim != NULL && sim->schema_version >= 46U ?
        CC_MAX_GOSSIP_CARRIERS : CC_LEGACY_GOSSIP_CARRIERS;
}

const CcGossipCarrier *CcSimGossipCarrier(const CcSim *sim, CcId id)
{
    if (sim == NULL || id == 0U) return NULL;
    for (int32_t i = 0; i < CcSimGossipCarrierCapacity(sim); ++i) {
        if (sim->gossip_carriers[i].id == id) return &sim->gossip_carriers[i];
    }
    return NULL;
}

const CcGossip *CcSimPersonalGossip(const CcSim *sim, CcId id, int32_t offset,
                                  const CcGossipVersion **version)
{
    if (version != NULL) *version = NULL;
    const CcGossipCarrier *carrier = CcSimGossipCarrier(sim, id);
    if (carrier == NULL || offset < 0) return NULL;
    /* Newest reports first; slot reuse leaves the ordering unchanged. */
    CcId before = UINT64_MAX;
    for (int32_t n = 0; n <= offset && n < CC_MAX_GOSSIP; ++n) {
        int32_t best = -1;
        for (int32_t i = 0; i < CC_MAX_GOSSIP; ++i) {
            if ((carrier->stories & (UINT32_C(1) << (uint32_t)i)) == 0U ||
                sim->gossip[i].event_id >= before) continue;
            if (best < 0 || sim->gossip[i].event_id > sim->gossip[best].event_id) best = i;
        }
        if (best < 0) return NULL;
        if (n == offset) {
            if (version != NULL) *version = &carrier->versions[best];
            return &sim->gossip[best];
        }
        before = sim->gossip[best].event_id;
    }
    return NULL;
}

static bool GossipCarrierExists(const CcSim *sim, CcId id)
{
    if (id == 0U) return false;
    if (id == sim->player.id) return true;
    if (sim->schema_version >= 46U && CcSimCharacter(sim, id) != NULL) return true;
    for (int32_t i = 0; i < sim->royal_carriage_count; ++i) {
        if (id == sim->royal_carriages[i].id) return true;
    }
    for (int32_t i = 0; i < sim->shipment_count; ++i) {
        if (id == sim->shipments[i].id &&
            (sim->shipments[i].status == CC_SHIPMENT_TRAVELLING ||
             sim->shipments[i].status == CC_SHIPMENT_BLOCKED)) return true;
    }
    for (int32_t i = 0; i < sim->courier_count; ++i) {
        const CcCourier *courier = &sim->couriers[i];
        if (id == courier->id &&
            (courier->status == CC_COURIER_WAITING ||
             courier->status == CC_COURIER_TRAVELLING ||
             courier->status == CC_COURIER_WITH_PLAYER)) return true;
    }
    return false;
}

static const CcCharacter *GossipTellerAt(const CcSim *sim, CcId place_id,
                                        CcId story_id)
{
    if (sim->character_count == 0) return NULL;
    int32_t first = (int32_t)(story_id % (uint64_t)sim->character_count);
    for (int32_t offset = 0; offset < sim->character_count; ++offset) {
        const CcCharacter *person = &sim->characters[(first + offset) % sim->character_count];
        if (person->current_settlement_id == place_id &&
            CcCharacterAgeYears(sim, person) >= 16 &&
            person->activity != CC_CHARACTER_ACTIVITY_TRAVELLING) return person;
    }
    return NULL;
}

static CcGossipVersion RetellGossip(const CcSim *sim, const CcGossip *story,
                                    CcGossipVersion version,
                                    const CcCharacter *person, CcId carrier_id)
{
    int32_t loyalty = 0;
    int32_t stress = 0;
    int32_t courage = 50;
    if (person != NULL) {
        version.source_character_id = person->id;
        stress = person->stress;
        courage = person->courage;
        loyalty = person->role == CC_CHARACTER_OFFICIAL ||
                  person->role == CC_CHARACTER_COURIER ? 20 :
                  person->role == CC_CHARACTER_REFUGEE ? -20 : 0;
        for (int32_t i = 0; i < sim->faction_count; ++i) {
            if (person->faction_id != sim->factions[i].id) continue;
            if (sim->factions[i].kind == CC_FACTION_CROWN) loyalty = 20;
            if (sim->factions[i].kind == CC_FACTION_COMMONS) loyalty = -20;
        }
    } else if (CcIdKind(carrier_id) == CC_ENTITY_COURIER ||
               CcIdKind(carrier_id) == CC_ENTITY_ROYAL_CARRIAGE) {
        loyalty = 12;
    }
    version.retellings = MinimumI32(255, version.retellings + 1);
    version.court_bias = ClampI32(version.court_bias + loyalty, -100, 100);
    version.alarm = ClampI32(version.alarm + 4 + stress / 4 - courage / 20, 0, 100);
    int32_t age = MinimumI32(12, (sim->current_day - story->day) / 7);
    version.confidence = MaximumI32(10, version.confidence - 7 - stress / 10 - age);
    return version;
}

/* Retellings change a few concrete details. The saved version always gives
 * the same words, including after the scribes bind their account. */
static void MutatedGossipText(const char *original, int32_t retellings,
                              char *text, size_t capacity)
{
    static const struct { const char *before; const char *after; int32_t hop; } swaps[] = {
        {"one", "two", 2}, {"two", "three", 2}, {"three", "five", 2},
        {"four", "six", 2}, {"five", "seven", 2},
        {"western", "northern", 4}, {"eastern", "southern", 4},
        {"west", "north", 4}, {"east", "south", 4},
        {"workers", "merchants", 6}, {"Raiders", "Deserters", 6},
        {"raiders", "deserters", 6}, {"guards", "soldiers", 6}
    };
    size_t used = 0U;
    bool changed_count = false, changed_place = false, changed_people = false;
    for (size_t at = 0U; original[at] != '\0' && used + 1U < capacity;) {
        bool replaced = false;
        for (size_t i = 0U; i < sizeof(swaps) / sizeof(swaps[0]); ++i) {
            size_t length = strlen(swaps[i].before);
            if (retellings < swaps[i].hop ||
                (swaps[i].hop == 2 && changed_count) ||
                (swaps[i].hop == 4 && changed_place) ||
                (swaps[i].hop == 6 && changed_people) ||
                (at > 0U && isalpha((unsigned char)original[at - 1U])) ||
                strncmp(original + at, swaps[i].before, length) != 0 ||
                isalpha((unsigned char)original[at + length])) continue;
            size_t replacement = strlen(swaps[i].after);
            if (used + replacement >= capacity) break;
            memcpy(text + used, swaps[i].after, replacement);
            used += replacement;
            at += length;
            changed_count |= swaps[i].hop == 2;
            changed_place |= swaps[i].hop == 4;
            changed_people |= swaps[i].hop == 6;
            replaced = true;
            break;
        }
        if (!replaced) text[used++] = original[at++];
    }
    text[used] = '\0';
}

void CcGossipText(const CcSim *sim, const CcGossip *story,
                  const CcGossipVersion *version, char *text, size_t capacity)
{
    if (text == NULL || capacity == 0U) return;
    if (sim == NULL || story == NULL || version == NULL) { text[0] = '\0'; return; }
    if (version->retellings == 0) {
        (void)snprintf(text, capacity, "%s", story->text);
        return;
    }
    char account[CC_EVENT_TEXT_CAPACITY];
    if (sim->schema_version >= 46U) {
        MutatedGossipText(story->text, version->retellings, account, sizeof(account));
    } else if (version->retellings == 1) {
        (void)snprintf(account, sizeof(account), "%.85s", story->text);
    } else {
        const char *claim = "a ruler has changed things";
        switch (story->kind) {
            case CC_EVENT_WAR_DECLARED: claim = "war is spreading"; break;
            case CC_EVENT_PEACE_DECLARED: claim = "the courts have made peace"; break;
            case CC_EVENT_DRAGON_SLAIN: claim = "a great dragon has fallen"; break;
            case CC_EVENT_DRAGON_CROWNED: claim = "a dragon commands the realm"; break;
            case CC_EVENT_DRAGON_BROOD: claim = "dragons are multiplying"; break;
            case CC_EVENT_TREASURE_CRAFTED: claim = "a remarkable treasure has appeared"; break;
            case CC_EVENT_SETTLEMENT_RAIDED:
                claim = version->alarm >= 30 ? "raids are spreading" : "raiders have struck";
                break;
            case CC_EVENT_CHARACTER_DIED: claim = "a familiar face has died"; break;
            default: break;
        }
        const CcSettlement *origin = CcSimSettlement(sim, story->origin_id);
        (void)snprintf(account, sizeof(account), "Word from %.24s: %s.",
                       origin != NULL ? origin->name : "the road", claim);
    }
    const char *bias = version->court_bias >= 15 ? " Loyal voices credit the crown." :
                       version->court_bias <= -15 ? " Some blame the court." : "";
    const char *alarm = version->alarm >= 30 ? " They fear worse is coming." : "";
    (void)snprintf(text, capacity, "%s%s%s", account, bias, alarm);
}

static void HearGossip(CcSim *sim, CcGossip *story, CcId place_id,
                        CcId parent_id, const char *speaker, CcGossipVersion version)
{
    const CcSettlement *scriptorium = Scriptorium(sim);
    if (story->heard_day > 0 || story->recorded ||
        sim->archives.scribes <= 0 || scriptorium == NULL ||
        scriptorium->id != place_id) return;
    story->heard_day = sim->current_day;
    story->heard = version;
    (void)snprintf(story->heard_from, sizeof(story->heard_from), "%s", speaker);
    char account[CC_EVENT_TEXT_CAPACITY];
    CcGossipText(sim, story, &version, account, sizeof(account));
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(text, sizeof(text), "%.31s tell the scribes: %.85s",
                   speaker, account);
    story->heard_event_id = PushEvent(
        sim, CC_EVENT_RUMOR_SHARED, story->event_id, place_id,
        parent_id, 1, text)->id;
}

static void ExchangeGossip(CcSim *sim, CcId carrier_id, CcId place_id,
                            const char *speaker)
{
    if (sim->schema_version < 44U) return;
    int32_t place = SettlementSlotById(sim, place_id);
    if (place < 0 || CcSettlementIsAbandoned(&sim->settlements[place])) return;
    GatherGossip(sim);
    CcGossipCarrier *carrier = NULL;
    for (int32_t i = 0; i < CcSimGossipCarrierCapacity(sim); ++i) {
        if (sim->gossip_carriers[i].id == carrier_id) {
            carrier = &sim->gossip_carriers[i];
            break;
        }
    }
    if (carrier == NULL) {
        for (int32_t i = 0; i < CcSimGossipCarrierCapacity(sim); ++i) {
            if (!GossipCarrierExists(sim, sim->gossip_carriers[i].id)) {
                carrier = &sim->gossip_carriers[i];
                *carrier = (CcGossipCarrier){.id = carrier_id};
                break;
            }
        }
    }
    if (carrier == NULL) return;
    uint32_t town = UINT32_C(1) << (uint32_t)place;
    for (int32_t i = 0; i < CC_MAX_GOSSIP; ++i) {
        CcGossip *story = &sim->gossip[i];
        if (story->event_id == 0U) continue;
        uint32_t bit = UINT32_C(1) << (uint32_t)i;
        if ((carrier->stories & bit) != 0U &&
            (story->settlement_mask & town) == 0U) {
            story->settlement_mask |= town;
            story->local[place] = RetellGossip(sim, story, carrier->versions[i],
                sim->schema_version >= 46U ? CcSimCharacter(sim, carrier_id) : NULL, carrier_id);
            if (sim->schema_version < 46U || CcIdKind(carrier_id) != CC_ENTITY_CHARACTER)
                carrier->versions[i] = story->local[place];
            char account[CC_EVENT_TEXT_CAPACITY];
            CcGossipText(sim, story, &story->local[place], account, sizeof(account));
            char text[CC_EVENT_TEXT_CAPACITY];
            (void)snprintf(text, sizeof(text), "%.31s share gossip: %.90s",
                           speaker, account);
            CcId shared = PushEvent(sim, CC_EVENT_RUMOR_SHARED, carrier_id,
                                     place_id, story->event_id, 1, text)->id;
            HearGossip(sim, story, place_id, shared, speaker, story->local[place]);
        }
        if ((story->settlement_mask & town) != 0U &&
            (carrier->stories & bit) == 0U) {
            carrier->stories |= bit;
            const CcCharacter *teller = GossipTellerAt(sim, place_id, story->event_id);
            if (sim->schema_version >= 46U && CcIdKind(carrier_id) == CC_ENTITY_CHARACTER) {
                teller = NULL;
            }
            carrier->versions[i] = RetellGossip(sim, story, story->local[place], teller, 0U);
        }
    }
}

void CcSimRefreshCharacterGossip(CcSim *sim)
{
    if (sim == NULL || sim->schema_version < 46U) return;
    for (int32_t i = 0; i < sim->character_count; ++i) {
        const CcCharacter *person = &sim->characters[i];
        if (person->activity == CC_CHARACTER_ACTIVITY_TRAVELLING ||
            CcCharacterAgeYears(sim, person) < 16) continue;
        ExchangeGossip(sim, person->id, person->current_settlement_id, person->name);
    }
}

static bool ApplyExchangeGossip(CcSim *sim, const CcCommand *command,
                                char *error, size_t error_capacity)
{
    const CcCharacter *person = CcSimCharacter(sim, command->target_id);
    if (sim->schema_version < 46U || person == NULL ||
        person->current_settlement_id != sim->player.location_id ||
        person->activity == CC_CHARACTER_ACTIVITY_TRAVELLING ||
        CcCharacterAgeYears(sim, person) < 16) {
        SetError(error, error_capacity, "Find that person in town to exchange news.");
        return false;
    }
    const CcGossipCarrier *previous = CcSimGossipCarrier(sim, sim->player.id);
    uint32_t known = previous != NULL ? previous->stories : 0U;
    ExchangeGossip(sim, person->id, person->current_settlement_id, person->name);
    ExchangeGossip(sim, sim->player.id, sim->player.location_id, "Your company");
    const CcGossipCarrier *teller = CcSimGossipCarrier(sim, person->id);
    if (teller != NULL) {
        for (int32_t i = 0; i < CcSimGossipCarrierCapacity(sim); ++i) {
            CcGossipCarrier *listener = &sim->gossip_carriers[i];
            if (listener->id != sim->player.id) continue;
            for (int32_t j = 0; j < CC_MAX_GOSSIP; ++j) {
                uint32_t bit = UINT32_C(1) << (uint32_t)j;
                if ((known & bit) == 0U && (teller->stories & bit) != 0U)
                    listener->versions[j] = RetellGossip(sim, &sim->gossip[j], teller->versions[j], person, 0U);
            }
        }
    }
    ExchangeGossip(sim, person->id, person->current_settlement_id, person->name);
    SetError(error, error_capacity, "");
    return true;
}

static void HearLocalGossip(CcSim *sim)
{
    if (sim->schema_version < 44U) return;
    GatherGossip(sim);
    const CcSettlement *place = Scriptorium(sim);
    int32_t slot = place != NULL ? SettlementSlotById(sim, place->id) : -1;
    if (slot < 0) return;
    uint32_t town = UINT32_C(1) << (uint32_t)slot;
    for (int32_t i = 0; i < CC_MAX_GOSSIP; ++i) {
        if ((sim->gossip[i].settlement_mask & town) != 0U) {
            CcGossip *story = &sim->gossip[i];
            CcGossipVersion version = RetellGossip(sim, story, story->local[slot],
                GossipTellerAt(sim, place->id, story->event_id), 0U);
            HearGossip(sim, &sim->gossip[i], place->id,
                        sim->gossip[i].event_id, "Town residents", version);
        }
    }
}

static CcSettlement *ArchiveVaultWithBindingMaterials(CcSim *sim,
                                                       int32_t first_kingdom)
{
    if (sim == NULL || sim->kingdom_count <= 0) return NULL;
    for (int32_t offset = 0; offset < sim->kingdom_count; ++offset) {
        int32_t kingdom_slot =
            (first_kingdom + offset) % sim->kingdom_count;
        CcId kingdom_id = sim->kingdoms[kingdom_slot].id;
        for (int32_t i = 0; i < sim->settlement_count; ++i) {
            CcSettlement *vault = &sim->settlements[i];
            if (vault->kingdom_id == kingdom_id &&
                !CcSettlementIsAbandoned(vault) &&
                vault->stock[CC_GOOD_GOLD] > 0 &&
                vault->stock[CC_GOOD_GEMS] > 0) {
                return vault;
            }
        }
    }
    return NULL;
}

static bool BindArchiveTome(CcSim *sim)
{
    if (sim == NULL || sim->kingdom_count <= 0) return false;
    static const char *forms[] = {
        "Chronicle", "Ledger", "Annal", "Register"
    };
    int32_t holder = (int32_t)(sim->treasure_count % sim->kingdom_count);
    CcSettlement *vault = ArchiveVaultWithBindingMaterials(sim, holder);
    CcKingdom *kingdom = vault != NULL ?
        KingdomMutable(sim, vault->kingdom_id) : NULL;
    CcTreasure *tome = kingdom != NULL ? AllocateTreasure(sim) : NULL;
    if (tome == NULL || vault == NULL) return false;
    vault->stock[CC_GOOD_GOLD] -= 1;
    vault->stock[CC_GOOD_GEMS] -= 1;
    (void)snprintf(tome->name, sizeof(tome->name),
                   "%.12s %.18s of %d",
                   forms[sim->treasure_count % 4],
                   kingdom->name, sim->current_day / 364);
    tome->maker_settlement_id = vault->id;
    tome->owner_id = vault->id;
    tome->location_id = vault->id;
    tome->gold_content = 1;
    tome->gem_content = 1;
    tome->craft_work = 1;
    tome->appraised_value = 6;
    tome->created_day = sim->current_day;
    return true;
}

static void AdvanceCoronationLaw(CcSim *sim, bool scriptorium_ready,
                                 CcId scriptorium_id)
{
    if (sim == NULL || sim->schema_version < 34U) return;
    if (sim->schema_version >= 35U) {
        AssignHistoryOffices(sim, true);
        CcArchives *archives = &sim->archives;
        int32_t rank_target = archives->scribes * 18 +
            MinimumI32(20, archives->lore_stored / 100) +
            (scriptorium_ready ? 8 : 0);
        rank_target = ClampI32(rank_target, 12, 100);
        if (archives->stewardship_rank < rank_target) {
            archives->stewardship_rank += 1;
        } else if (archives->stewardship_rank > rank_target) {
            archives->stewardship_rank -= 1;
        }
    }
    for (int32_t i = 0; i < sim->kingdom_count; ++i) {
        CcKingdom *kingdom = &sim->kingdoms[i];
        if (scriptorium_ready) {
            kingdom->sanction = ClampI32(kingdom->sanction + 2, 0, 100);
            kingdom->unsanctioned_weeks = 0;
            if (kingdom->anointed || kingdom->sanction < 60) {
                if (sim->schema_version >= 35U && kingdom->anointed) {
                    kingdom->anointed_by_character_id =
                        sim->archives.abbot_character_id;
                }
                continue;
            }
            kingdom->anointed = true;
            if (sim->schema_version >= 35U) {
                kingdom->anointed_by_character_id =
                    sim->archives.abbot_character_id;
            }
            const CcCharacter *abbot = CcSimCharacter(
                sim, sim->archives.abbot_character_id);
            const CcCharacter *ruler = CcSimCharacter(
                sim, kingdom->ruler_character_id);
            char text[CC_EVENT_TEXT_CAPACITY];
            if (sim->schema_version >= 35U) {
                (void)snprintf(
                    text, sizeof(text),
                    "%.20s anoints %.20s of %.20s; the crown becomes lawful.",
                    abbot != NULL ? abbot->name : "The abbot",
                    ruler != NULL ? ruler->name : "the ruler",
                    kingdom->name);
            } else {
                (void)snprintf(
                    text, sizeof(text),
                    "Fed and papered scribes anoint the true ruler of %.72s.",
                    kingdom->name);
            }
            (void)PushEvent(
                sim, CC_EVENT_KING_ANOINTED, kingdom->id,
                scriptorium_id, LatestLocalCause(sim, scriptorium_id),
                kingdom->sanction, text);
            continue;
        }
        kingdom->sanction = MaximumI32(40, kingdom->sanction - 1);
        kingdom->unsanctioned_weeks += 1;
        if (kingdom->unsanctioned_weeks < 52) continue;
        if (!kingdom->anointed) {
            kingdom->unsanctioned_weeks = 51;
            continue;
        }
        kingdom->unsanctioned_weeks = 0;
        kingdom->anointed = false;
        if (sim->schema_version >= 35U) {
            kingdom->anointed_by_character_id = 0U;
        }
        bool live_uncrowned_dragon = sim->schema_version >= 42U &&
                                     DragonIsAliveAndUncrowned(sim);
        int32_t legitimacy_loss = live_uncrowned_dragon ? 8 : 15;
        kingdom->legitimacy = MaximumI32(0, kingdom->legitimacy - legitimacy_loss);
        kingdom->pretender_crises += 1;
        char text[CC_EVENT_TEXT_CAPACITY];
        if (live_uncrowned_dragon) {
            (void)snprintf(
                text, sizeof(text),
                "A pretender rises in %.60s, vowing to slay %.30s.",
                kingdom->name, sim->dragon.name);
            if (sim->dragon_campaign.phase == CC_DRAGON_CAMPAIGN_OUTBOUND &&
                ((sim->dragon_campaign.alliance_kingdom_mask >> i) & 1U) != 0) {
                int32_t room = CC_SIM_MAX_UNITS -
                    sim->dragon_campaign.supplies[CC_GOOD_TOOLS];
                sim->dragon_campaign.supplies[CC_GOOD_TOOLS] +=
                    TakeAllianceGood(sim, UINT32_C(1) << (uint32_t)i,
                                     CC_GOOD_TOOLS, MinimumI32(3, room));
            }
            if (sim->dragon_campaign.phase == CC_DRAGON_CAMPAIGN_IDLE &&
                sim->dragon_campaign.cooldown_days > 0) {
                sim->dragon_campaign.cooldown_days =
                    MaximumI32(0, sim->dragon_campaign.cooldown_days - 60);
            }
        } else {
            (void)snprintf(
                text, sizeof(text),
                "A year of abbey silence breeds a pretender in %.72s.",
                kingdom->name);
        }
        (void)PushEvent(
            sim, CC_EVENT_PRETENDER_CRISIS, kingdom->id,
            scriptorium_id, LatestLocalCause(sim, scriptorium_id),
            -legitimacy_loss, text);
    }
}

static bool DragonIsAliveAndUncrowned(const CcSim *sim)
{
    if (sim == NULL || sim->dragon.id == 0U || sim->dragon.slain) return false;
    return sim->dragon.life_stage == CC_DRAGON_STAGE_EGG ||
           sim->dragon.life_stage == CC_DRAGON_STAGE_WHELP ||
           sim->dragon.life_stage == CC_DRAGON_STAGE_WANDERER ||
           sim->dragon.life_stage == CC_DRAGON_STAGE_UNCROWNED ||
           sim->dragon.life_stage == CC_DRAGON_STAGE_DEEP_WYRM;
}

static int32_t CharacterKingdomSlot(const CcSim *sim,
                                    const CcCharacter *character)
{
    if (sim == NULL || character == NULL) return -1;
    const CcSettlement *home = CcSimSettlement(
        sim, character->home_settlement_id);
    return home == NULL ? -1 : KingdomSlotById(sim, home->kingdom_id);
}

static CcCharacter *BestOfficeHolder(CcSim *sim, int32_t kingdom_slot,
                                     CcId except_id, bool abbot)
{
    CcCharacter *best = NULL;
    int32_t best_score = INT32_MIN;
    for (int32_t i = 0; i < sim->character_count; ++i) {
        CcCharacter *candidate = &sim->characters[i];
        int32_t candidate_kingdom = CharacterKingdomSlot(sim, candidate);
        if (candidate->id == except_id ||
            (!abbot && candidate_kingdom != kingdom_slot)) continue;
        const CcSettlement *home = CcSimSettlement(
            sim, candidate->home_settlement_id);
        int32_t score = candidate->courage;
        if (candidate->role == CC_CHARACTER_OFFICIAL) score += 100;
        if (home != NULL && home->function == CC_SETTLEMENT_CAPITAL) {
            score += abbot ? 80 : 30;
        }
        if (candidate->current_settlement_id ==
            candidate->home_settlement_id) score += 5;
        if (best == NULL || score > best_score ||
            (score == best_score && candidate->id < best->id)) {
            best = candidate;
            best_score = score;
        }
    }
    return best;
}

static void AssignHistoryOffices(CcSim *sim, bool announce)
{
    if (sim == NULL || sim->character_count == 0) return;
    CcArchives *archives = &sim->archives;
    if (CcSimCharacter(sim, archives->abbot_character_id) == NULL) {
        CcCharacter *abbot = BestOfficeHolder(sim, -1, 0U, true);
        archives->abbot_character_id = abbot != NULL ? abbot->id : 0U;
        if (announce && abbot != NULL) {
            char text[CC_EVENT_TEXT_CAPACITY];
            (void)snprintf(text, sizeof(text),
                           "%.24s becomes Abbot of the Scriptorium and keeps the anointing roll.",
                           abbot->name);
            (void)PushEvent(sim, CC_EVENT_MONASTIC_SUCCESSION, abbot->id,
                            abbot->home_settlement_id, 0U, 30, text);
        }
    }
    for (int32_t slot = 0; slot < sim->kingdom_count; ++slot) {
        CcKingdom *kingdom = &sim->kingdoms[slot];
        const CcCharacter *ruler = CcSimCharacter(
            sim, kingdom->ruler_character_id);
        if (ruler == NULL || CharacterKingdomSlot(sim, ruler) != slot) {
            CcCharacter *chosen = BestOfficeHolder(sim, slot, 0U, false);
            kingdom->ruler_character_id = chosen != NULL ? chosen->id : 0U;
            if (announce && chosen != NULL) {
                char text[CC_EVENT_TEXT_CAPACITY];
                (void)snprintf(text, sizeof(text),
                               "%.24s takes the rule of %.24s and seeks the abbot's blessing.",
                               chosen->name, kingdom->name);
                (void)PushEvent(sim, CC_EVENT_ROYAL_SUCCESSION, chosen->id,
                                chosen->home_settlement_id, 0U, 25, text);
            }
        }
        if (CcSimCharacter(sim, kingdom->monastery_patron_id) == NULL ||
            CharacterKingdomSlot(
                sim, CcSimCharacter(sim, kingdom->monastery_patron_id)) !=
                slot) {
            kingdom->monastery_patron_id = kingdom->ruler_character_id;
        }
    }
}

void CcSimUpgradeHistoryOffices(CcSim *sim)
{
    if (sim == NULL) return;
    if (sim->archives.stewardship_rank == 0) {
        sim->archives.stewardship_rank = 60;
    }
    AssignHistoryOffices(sim, true);
    for (int32_t i = 0; i < sim->kingdom_count; ++i) {
        CcKingdom *kingdom = &sim->kingdoms[i];
        kingdom->anointed_by_character_id = kingdom->anointed ?
            sim->archives.abbot_character_id : 0U;
    }
}

static void AdvanceArchives(CcSim *sim)
{
    if (sim == NULL || sim->current_day % 7 != 0) return;
    CcArchives *archives = &sim->archives;

    int32_t target_scribes = sim->iron_ledger_reserve >= 300 ? CC_MAX_SCRIBES :
        sim->iron_ledger_reserve >= 150 ? 2 :
        sim->iron_ledger_reserve >= 50 ? 1 : 0;
    if (target_scribes > archives->scribes) {
        archives->scribes = target_scribes;
    } else if (target_scribes < archives->scribes) {
        archives->scribes -= 1;
    }

    CcSettlement *scriptorium = NULL;
    int32_t active_scribes = archives->scribes;
    bool scriptorium_ready = archives->scribes > 0;
    if (sim->schema_version >= 34U) {
        const CcSettlement *place = Scriptorium(sim);
        if (place != NULL) {
            scriptorium = CcSimSettlementMutable(sim, place->id);
        }
        int32_t scribe_grain = scriptorium == NULL ? 0 : MaximumI32(
            0, scriptorium->stock[CC_GOOD_WHEAT] -
               WeeklyFoodUse(sim, scriptorium) * 2);
        active_scribes = MinimumI32(archives->scribes, scribe_grain / 2);
        if (active_scribes > 0) {
            scriptorium->stock[CC_GOOD_WHEAT] -= active_scribes * 2;
            RefreshSettlementGoodPrice(sim, scriptorium, CC_GOOD_WHEAT);
        }
        scriptorium_ready = active_scribes > 0 &&
            scriptorium->stock[CC_GOOD_PAPER] > 0 &&
            scriptorium->stock[CC_GOOD_TOOLS] > 0;
    }

    CcId noted[CC_MAX_SCRIBES];
    CcId noted_location[CC_MAX_SCRIBES];
    int32_t noted_gossip[CC_MAX_SCRIBES];
    char noted_text[CC_MAX_SCRIBES][CC_EVENT_TEXT_CAPACITY];
    int32_t noted_count = 0;
    HearLocalGossip(sim);
    if (sim->schema_version >= 44U) {
        while (noted_count < active_scribes) {
            int32_t oldest = -1;
            for (int32_t i = 0; i < CC_MAX_GOSSIP; ++i) {
                const CcGossip *candidate = &sim->gossip[i];
                if (candidate->heard_day == 0 || candidate->recorded) continue;
                bool selected = false;
                for (int32_t j = 0; j < noted_count; ++j) {
                    if (noted_gossip[j] == i) selected = true;
                }
                if (selected) continue;
                if (oldest < 0 || candidate->heard_day < sim->gossip[oldest].heard_day ||
                    (candidate->heard_day == sim->gossip[oldest].heard_day &&
                     candidate->event_id < sim->gossip[oldest].event_id)) oldest = i;
            }
            if (oldest < 0) break;
            const CcGossip *story = &sim->gossip[oldest];
            noted[noted_count] = story->heard_event_id != 0U ?
                story->heard_event_id : story->event_id;
            noted_location[noted_count] = scriptorium != NULL ? scriptorium->id : 0U;
            noted_gossip[noted_count] = oldest;
            char account[CC_EVENT_TEXT_CAPACITY];
            CcGossipText(sim, story, &story->heard, account, sizeof(account));
            (void)snprintf(noted_text[noted_count],
                           sizeof(noted_text[noted_count]),
                           "%s", account);
            noted_count += 1;
        }
    }
    int32_t first_day = MaximumI32(1, sim->current_day - 7);
    for (int32_t offset = 0;
         sim->schema_version < 44U && offset < sim->event_count &&
         noted_count < active_scribes;
         ++offset) {
        const CcEvent *event = CcSimRecentEvent(sim, offset);
        if (event == NULL || event->day < first_day ||
            event->day > sim->current_day || event->magnitude < 20 ||
            EventWasArchived(sim, event->id)) continue;
        if (!IsNotableGossip(sim, event)) continue;
        noted[noted_count] = event->id;
        noted_location[noted_count] = event->location_id;
        (void)snprintf(noted_text[noted_count],
                       sizeof(noted_text[noted_count]),
                       "The scriptorium records the lore of: %.90s",
                       event->text);
        noted_count += 1;
    }
    int32_t tomes_written = 0;
    for (int32_t i = 0; i < noted_count; ++i) {
        if (sim->schema_version >= 34U &&
            (scriptorium == NULL ||
             scriptorium->stock[CC_GOOD_PAPER] <= 0 ||
             scriptorium->stock[CC_GOOD_TOOLS] <= 0)) break;
        if (!BindArchiveTome(sim)) break;
        archives->lore_stored += 1;
        archives->last_recorded_day = sim->current_day;
        if (sim->schema_version >= 44U) {
            sim->gossip[noted_gossip[i]].recorded = true;
        }
        (void)PushEvent(sim, CC_EVENT_LORE_RECORDED, noted[i],
                        noted_location[i], noted[i], 1, noted_text[i]);
        if (sim->schema_version >= 34U) {
            scriptorium->stock[CC_GOOD_PAPER] -= 1;
            RefreshSettlementGoodPrice(sim, scriptorium, CC_GOOD_PAPER);
        }
        tomes_written += 1;
    }
    if (sim->schema_version >= 34U && tomes_written > 0) {
        WearOneTool(scriptorium, &archives->kit_tool_wear, 8);
        RefreshSettlementGoodPrice(sim, scriptorium, CC_GOOD_TOOLS);
    }

    if (sim->treasure_count >= CC_MAX_TREASURES - 4) {
        int32_t tome_slots[4];
        if (FindArchiveVolumesToBind(sim, tome_slots)) {
            CcId oldest_owner = sim->treasures[tome_slots[0]].owner_id;
            CcId oldest_location =
                sim->treasures[tome_slots[0]].location_id;
            CcId oldest_maker =
                sim->treasures[tome_slots[0]].maker_settlement_id;
            int32_t combined_lore = 0;
            int32_t combined_gold = 0;
            int32_t combined_gems = 0;
            int32_t combined_value = 0;
            CcId eldest_parent = LatestLocalCause(sim, oldest_location);
            for (int32_t i = 0; i < 4; ++i) {
                CcTreasure *t = &sim->treasures[tome_slots[i]];
                combined_lore += t->craft_work;
                combined_gold += t->gold_content;
                combined_gems += t->gem_content;
                combined_value += t->appraised_value;
                t->destroyed = true;
            }
            CcTreasure *codex = &sim->treasures[tome_slots[0]];
            *codex = (CcTreasure){0};
            (void)snprintf(codex->name, sizeof(codex->name),
                           "Codex of the Scriptorium");
            codex->id = NextId(sim, CC_ENTITY_TREASURE);
            codex->maker_settlement_id = oldest_maker;
            codex->owner_id = oldest_owner;
            codex->location_id = oldest_location;
            codex->gold_content = combined_gold;
            codex->gem_content = combined_gems;
            codex->craft_work = combined_lore;
            codex->appraised_value = combined_value;
            codex->created_day = sim->current_day;
            char text[CC_EVENT_TEXT_CAPACITY];
            (void)snprintf(text, sizeof(text),
                           "The scriptorium binds four chronicles into "
                           "the Codex of the Scriptorium; nothing is "
                           "lost that was written.");
            (void)PushEvent(sim, CC_EVENT_LORE_RECORDED, codex->id,
                            oldest_location, eldest_parent,
                            combined_lore, text);
        }
    }

    int32_t lore_ceiling = archives->lore_lost_total == 0 ? 75 :
        40 + MinimumI32(35, archives->lore_stored / 200);
    int32_t lore_floor = 25;
    if (tomes_written > 0 && archives->lore_ceiling < lore_ceiling) {
        archives->lore_ceiling = lore_ceiling;
    }
    if (archives->lore_ceiling > lore_ceiling) {
        archives->lore_ceiling -= 1;
    }
    for (int32_t i = 0; i < sim->kingdom_count; ++i) {
        CcKingdom *kingdom = &sim->kingdoms[i];
        int32_t target = archives->scribes > 0 ?
            archives->lore_ceiling : lore_floor;
        if (kingdom->legitimacy < target) {
            kingdom->legitimacy += 1;
        } else if (kingdom->legitimacy > target) {
            kingdom->legitimacy -= 1;
        }
    }
    AdvanceCoronationLaw(
        sim, scriptorium_ready,
        scriptorium != NULL ? scriptorium->id : 0U);

    if (archives->scribes == 0 && archives->lore_stored > 0 &&
        sim->schema_version < 36U) {
        archives->lore_stored -= 1;
        archives->lore_lost_total += 1;
        char text[CC_EVENT_TEXT_CAPACITY];
        (void)snprintf(text, sizeof(text),
                       "Unfunded and unwatched, part of the archive crumbles; "
                       "a name is forgotten.");
        (void)PushEvent(sim, CC_EVENT_LORE_LOST, 0U, 0U, 0U, 1, text);
    } else if (archives->scribes == 0 && archives->lore_stored > 0) {
        int32_t oldest = -1;
        for (int32_t i = 0; i < sim->treasure_count; ++i) {
            if (TreasureIsArchiveVolume(&sim->treasures[i]) &&
                EarlierArchiveVolume(sim, i, oldest)) {
                oldest = i;
            }
        }
        if (oldest >= 0) {
            CcTreasure *volume = &sim->treasures[oldest];
            char former_name[CC_MAP_NAME_CAPACITY];
            (void)snprintf(former_name, sizeof(former_name), "%s",
                           volume->name);
            bool ruined = volume->craft_work == 1;
            if (ruined) {
                (void)snprintf(volume->name, sizeof(volume->name),
                               "Ruined %.40s", former_name);
            } else {
                volume->craft_work -= 1;
            }
            archives->lore_stored -= 1;
            archives->lore_lost_total += 1;
            char text[CC_EVENT_TEXT_CAPACITY];
            (void)snprintf(
                text, sizeof(text),
                ruined ?
                    "Unfunded and unwatched, %.48s falls into ruin; its lore is lost." :
                    "Unfunded and unwatched, a page of %.48s crumbles; part of its lore is lost.",
                former_name);
            CcId parent = LatestLocalCause(sim, volume->location_id);
            (void)PushEvent(sim, CC_EVENT_LORE_LOST, volume->id,
                            volume->location_id, parent, 1, text);
        }
    }
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
                NutritionRations(
                    candidate->stock, CC_NUTRITION_CIVILIAN) < 18 ||
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
        (void)CcNutritionConsume(
            donor->stock, CC_NUTRITION_CIVILIAN,
            12 * CC_NUTRITION_PER_RATION);
        donor->stock[CC_GOOD_TOOLS] -= 2;
        donor->market_coins -= 12;
        ruin->kingdom_id = donor->kingdom_id;
        if (sim->schema_version >= 36U) AssignHistoryOffices(sim, true);
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

static CcId StartDragonTheft(CcSim *sim, CcId thief_id,
                             CcSettlement *target, int32_t amount,
                             CcId parent_event_id,
                             const char *theft_text)
{
    if (sim == NULL || target == NULL || amount <= 0 || sim->dragon.slain ||
        sim->dragon.stolen_outstanding > 0 ||
        (CcMoney)amount > sim->dragon.hoard) return 0U;
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
    sim->dragon.crown_continuity_days = MaximumI32(
        0, sim->dragon.crown_continuity_days - 182);
    sim->dragon.activity = CC_DRAGON_ACTIVITY_RETALIATING;
    CcEvent *theft = PushEvent(
        sim, CC_EVENT_DRAGON_HOARD_STOLEN, thief_id,
        sim->dragon.lair_settlement_id, parent_event_id,
        amount, theft_text);
    CcId theft_event_id = theft->id;
    sim->dragon.hoard_event_id = theft_event_id;
    char omen_text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(omen_text, sizeof(omen_text),
                   "Smoke falls into %s's chimneys; old readers count 14 nights until %s comes.",
                   target->name, sim->dragon.name);
    CcEvent *omen = PushEvent(
        sim, CC_EVENT_DRAGON_OMEN, sim->dragon.id, target->id,
        theft_event_id, 14, omen_text);
    sim->dragon.omen_event_id = omen->id;
    return theft_event_id;
}

static void PlanGoblinTribute(CcSim *sim)
{
    CcGoblinCult *goblins = &sim->goblins;
    if (goblins->tribute_phase != CC_GOBLIN_TRIBUTE_IDLE ||
        goblins->tribute_cooldown_days > 0 ||
        sim->dragon.stolen_outstanding > 0) return;
    if (NutritionRations(
            goblins->lair_stock, CC_NUTRITION_CIVILIAN) < 8) {
        goblins->raid_motive = CC_GOBLIN_RAID_HUNGER;
    } else if (goblins->lair_stock[CC_GOOD_TOOLS] < 2 ||
               goblins->lair_stock[CC_GOOD_WEAPONS] < 3) {
        goblins->raid_motive = CC_GOBLIN_RAID_EQUIPMENT;
    } else if (sim->dragon.slain) {

        goblins->tribute_cooldown_days = 14;
        return;
    } else {
        goblins->raid_motive = CC_GOBLIN_RAID_DRAGON_TRIBUTE;
    }
    /* A quarrelling cult cannot organize anything beyond a food raid;
     * it reconvenes in two weeks rather than muster while divided. */
    if (goblins->raid_motive != CC_GOBLIN_RAID_HUNGER &&
        goblins->cohesion < 25) {
        goblins->raid_motive = CC_GOBLIN_RAID_NONE;
        goblins->tribute_cooldown_days = 14;
        return;
    }
    CcSettlement *target = NULL;
    int64_t best_score = INT64_MIN;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        CcSettlement *place = &sim->settlements[i];
        if (CcSettlementIsAbandoned(place) ||
            place->id == sim->dragon.lair_settlement_id ||
            place->id == goblins->lair_settlement_id) continue;
        int64_t score = -(int64_t)place->security * 2;
        if (goblins->raid_motive == CC_GOBLIN_RAID_HUNGER) {
            score += NutritionRations(
                place->stock, CC_NUTRITION_CIVILIAN) * 4;
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
    goblins->tribute_event_id = event->id;
}

static void AdvanceGoblinTribute(CcSim *sim)
{
    CcGoblinCult *goblins = &sim->goblins;
    if (goblins->tribute_phase == CC_GOBLIN_TRIBUTE_IDLE) {
        if (sim->current_day % 7 == 0) {
            int32_t food_needed = sim->dragon.slain ?
                1 + (goblins->members - 1) / 24 : 1;
            int32_t food_eaten = CcNutritionConsume(
                goblins->lair_stock, CC_NUTRITION_CIVILIAN,
                food_needed * CC_NUTRITION_PER_RATION) /
                CC_NUTRITION_PER_RATION;
            int32_t hunger_loss = food_needed - food_eaten;
            goblins->members = MaximumI32(
                12, goblins->members - hunger_loss);
            if (hunger_loss > 0) {
                goblins->cohesion = ClampI32(
                    goblins->cohesion - hunger_loss * 3, 0, 100);
            }
            /* A cult too divided to muster also cannot hold its members:
             * the quarrel itself drives ash-sworn away every week. */
            if (goblins->cohesion < 25 && goblins->members > 12) {
                goblins->members -= 1;
            }
        }
        if (sim->schema_version >= 36U && sim->current_day % 28 == 0 &&
            NutritionRations(goblins->lair_stock, CC_NUTRITION_CIVILIAN) >= 4 &&
            goblins->lair_stock[CC_GOOD_TOOLS] >= 1) {
            goblins->cohesion = MinimumI32(100, goblins->cohesion + 1);
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
        goblins->tribute_event_id = event->id;
    }

    goblins->tribute_days_remaining = MaximumI32(
        0, goblins->tribute_days_remaining - 1);
    if (goblins->tribute_days_remaining > 0) return;

    if (goblins->tribute_phase == CC_GOBLIN_TRIBUTE_OUTBOUND) {
        CcGood chosen = PreferredNutritionGood(
            target->stock, CC_NUTRITION_CIVILIAN);
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
                     PreferredNutritionGood(
                         target->stock, CC_NUTRITION_CIVILIAN);
        }
        int32_t capacity = CcGoodDefinitionFor(chosen)->raid_capacity;
        if (goblins->target_warned) capacity = MaximumI32(1, capacity / 2);
        if (goblins->cohesion < 50) capacity = MaximumI32(1, capacity / 2);
        int32_t taken_goods = MinimumI32(target->stock[chosen], capacity);
        target->stock[chosen] -= taken_goods;
        goblins->carried_goods[chosen] = taken_goods;
        CcMoney taken_coins = 0;
        if (goblins->raid_motive == CC_GOBLIN_RAID_DRAGON_TRIBUTE) {
            CcMoney wanted = 8 + goblins->members / 6;
            if (goblins->target_warned) wanted = wanted > 1 ? wanted / 2 : 1;
            if (goblins->cohesion < 50) wanted = wanted > 1 ? wanted / 2 : 1;
            taken_coins = MinimumI32(
                target->market_coins > INT32_MAX ? INT32_MAX :
                    (int32_t)target->market_coins,
                wanted > INT32_MAX ? INT32_MAX : (int32_t)wanted);
            target->market_coins -= taken_coins;
            goblins->carried_tribute = taken_coins;
        }
        target->prosperity = ClampI32(target->prosperity - 2, 0, 100);
        target->security = ClampI32(target->security - 2, 0, 100);
        if (CcGoodNutritionValue(
                chosen, CC_NUTRITION_CIVILIAN) > 0) {
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
        goblins->tribute_event_id = event->id;
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
        goblins->tribute_event_id = event->id;

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
            goblins->tribute_event_id = event->id;
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
        sim->dragon.hoard_event_id = event->id;
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
        pressure->id,
        GoblinTravelDays(sim, origin->id), text);
    raiders->phase = CC_HOARD_RAIDERS_OUTBOUND;
    raiders->motive = motive;
    raiders->origin_settlement_id = origin->id;
    raiders->cause_event_id = departure->id;
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
            theft_parent_id = defense->id;
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
        CcId theft_event_id = StartDragonTheft(
            sim, raiders->id, origin, stolen, theft_parent_id, text);
        if (theft_event_id == 0U) return;
        raiders->phase = CC_HOARD_RAIDERS_RETURNING;
        raiders->cause_event_id = theft_event_id;
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
    raiders->cooldown_days =
        3 * 364 + (int32_t)(NextRandom(sim) % (3U * 364U));
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
            (NutritionRations(
                 place->stock, CC_NUTRITION_CIVILIAN) <= 0 &&
             place->cow_adults <= 0 &&
             place->sheep_adults <= 0)) continue;
        int32_t score = NutritionRations(
                            place->stock, CC_NUTRITION_CIVILIAN) * 3 +
                        place->cow_adults * 8 +
                        place->sheep_adults * 3 - place->security;
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
    int32_t sheep_wanted = sim->schema_version >= 32U ?
        MaximumI32(0, appetite - cows_taken * 3) : 0;
    int32_t sheep_taken = MinimumI32(
        target->sheep_adults, sheep_wanted);
    target->sheep_adults -= sheep_taken;
    int32_t food_wanted = sim->schema_version >= 32U ?
        MaximumI32(0, appetite - cows_taken * 3 - sheep_taken) :
        MaximumI32(1, appetite - cows_taken * 3);
    int32_t food_taken = CcNutritionConsume(
        target->stock, CC_NUTRITION_CIVILIAN,
        food_wanted * CC_NUTRITION_PER_RATION) /
        CC_NUTRITION_PER_RATION;
    int32_t taken = cows_taken * 3 + sheep_taken + food_taken;
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
    if (sim->schema_version >= 32U) {
        (void)snprintf(
            text, sizeof(text),
            "%s takes %d cow%s, %d sheep, and %d ration%s from %s.",
            dragon->name, cows_taken, cows_taken == 1 ? "" : "s",
            sheep_taken, food_taken, food_taken == 1 ? "" : "s",
            target->name);
    } else if (sim->schema_version >= 14U) {
        (void)snprintf(
            text, sizeof(text),
            "%s takes %d cow%s and %d Food from %s, sparing the roofs.",
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
    dragon->lifecycle_event_id = event->id;
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
    dragon->lifecycle_event_id = event->id;
}

static void HatchDragonSuccessor(CcSim *sim)
{
    CcDragon *dragon = &sim->dragon;
    if (!dragon->slain || dragon->egg_count <= 0) return;
    int32_t clutch = dragon->egg_count;
    CcId former_dragon_id = dragon->id;
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
    dragon->territoryless_days = 0;
    dragon->hunt_cooldown_days = 7;
    dragon->egg_count = 0;
    dragon->brood_days_remaining = 0;
    dragon->brood_cooldown_days =
        (220 + (int32_t)(NextRandom(sim) % 121U)) * 365;
    dragon->afterdeath_days = 0;
    for (int32_t i = 0; i < sim->treasure_count; ++i) {
        CcTreasure *treasure = &sim->treasures[i];
        if (treasure->owner_id == former_dragon_id) {
            treasure->owner_id = dragon->id;
            treasure->location_id = dragon->lair_settlement_id;
        }
    }
    sim->goblins.devotion = MaximumI32(sim->goblins.devotion, 28);
    dragon->crown_strength = CalculateDragonCrownStrength(sim);
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
    dragon->lifecycle_event_id = event->id;
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
    dragon->lifecycle_event_id = event->id;
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
    dragon->lifecycle_event_id = event->id;
}

static void GatherDragonSeedOfferings(CcSim *sim)
{
    CcGoblinCult *goblins = &sim->goblins;
    CcDragon *dragon = &sim->dragon;
    if (dragon->afterdeath_days < 100 * 365 ||
        dragon->afterdeath_days % 365 != 0) return;

    CcSettlement *donor = NULL;
    CcMoney best_wealth = -1;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        CcSettlement *place = &sim->settlements[i];
        if (CcSettlementIsAbandoned(place)) continue;
        CcKingdom *kingdom = KingdomMutable(sim, place->kingdom_id);
        CcMoney wealth = place->market_coins +
            (kingdom != NULL ? kingdom->treasury : 0);
        if (donor == NULL || wealth > best_wealth) {
            donor = place;
            best_wealth = wealth;
        }
    }
    if (donor == NULL) return;

    int32_t saved_coins = goblins->lair_coins >= 120 ?
        120 : (int32_t)goblins->lair_coins;
    CcMoney coin_need = MaximumI32(0, 120 - saved_coins);
    CcMoney coin_taken = coin_need < 30 ? coin_need : 30;
    CcMoney market_take = donor->market_coins < coin_taken ?
        donor->market_coins : coin_taken;
    donor->market_coins -= market_take;
    coin_taken -= market_take;
    CcMoney crown_take = 0;
    for (int32_t i = 0; i < sim->kingdom_count && coin_taken > 0; ++i) {
        CcMoney taken = sim->kingdoms[i].treasury < coin_taken ?
            sim->kingdoms[i].treasury : coin_taken;
        sim->kingdoms[i].treasury -= taken;
        coin_taken -= taken;
        crown_take += taken;
    }
    CcMoney gathered_coins = market_take + crown_take;
    goblins->lair_coins += gathered_coins;

    int32_t weekly_rations = 1 + (goblins->members - 1) / 24;
    int32_t nutrition_need = MaximumI32(
        0, 60 * weekly_rations * CC_NUTRITION_PER_RATION -
           CcNutritionAvailable(
               goblins->lair_stock, CC_NUTRITION_CIVILIAN));
    for (int32_t i = 0;
         i < sim->settlement_count && nutrition_need > 0; ++i) {
        CcSettlement *place = &sim->settlements[i];
        if (CcSettlementIsAbandoned(place)) continue;
        int32_t before[CC_GOOD_COUNT];
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
            before[good] = place->stock[good];
        }
        int32_t gathered = CcNutritionConsume(
            place->stock, CC_NUTRITION_CIVILIAN, nutrition_need);
        nutrition_need -= gathered;
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
            goblins->lair_stock[good] +=
                before[good] - place->stock[good];
        }
    }

    int32_t relics = goblins->lair_stock[CC_GOOD_GOLD] +
                     goblins->lair_stock[CC_GOOD_GEMS];
    if (relics < 2) {
        CcSettlement *relic_donor = NULL;
        CcGood rare_good = CC_GOOD_GEMS;
        for (int32_t pass = 0; pass < 2 && relic_donor == NULL; ++pass) {
            CcGood candidate_good = pass == 0 ?
                CC_GOOD_GEMS : CC_GOOD_GOLD;
            for (int32_t i = 0; i < sim->settlement_count; ++i) {
                CcSettlement *place = &sim->settlements[i];
                if (!CcSettlementIsAbandoned(place) &&
                    place->stock[candidate_good] > 0) {
                    relic_donor = place;
                    rare_good = candidate_good;
                    break;
                }
            }
        }
        if (relic_donor != NULL) {
            relic_donor->stock[rare_good] -= 1;
            goblins->lair_stock[rare_good] += 1;
        }
    }

    for (int32_t good = CC_GOOD_TOOLS;
         good <= CC_GOOD_WEAPONS; ++good) {
        int32_t target = good == CC_GOOD_TOOLS ? 2 : 3;
        int32_t need = MaximumI32(0, target - goblins->lair_stock[good]);
        for (int32_t i = 0;
             i < sim->settlement_count && need > 0; ++i) {
            CcSettlement *place = &sim->settlements[i];
            int32_t taken = MinimumI32(need, place->stock[good]);
            place->stock[good] -= taken;
            goblins->lair_stock[good] += taken;
            need -= taken;
        }
        if (need > 0) {
            int32_t iron_per_item = good == CC_GOOD_TOOLS ? 2 : 3;
            int32_t crafted = MinimumI32(
                need, goblins->lair_stock[CC_GOOD_IRON] / iron_per_item);
            goblins->lair_stock[CC_GOOD_IRON] -=
                crafted * iron_per_item;
            goblins->lair_stock[good] += crafted;
            need -= crafted;
        }
        if (need > 0) {
            int32_t wood_per_item = good == CC_GOOD_TOOLS ? 4 : 5;
            for (int32_t i = 0;
                 i < sim->settlement_count && need > 0; ++i) {
                CcSettlement *place = &sim->settlements[i];
                if (CcSettlementIsAbandoned(place)) continue;
                int32_t crafted = MinimumI32(
                    need, place->stock[CC_GOOD_WOOD] / wood_per_item);
                place->stock[CC_GOOD_WOOD] -= crafted * wood_per_item;
                goblins->lair_stock[good] += crafted;
                need -= crafted;
            }
        }
    }

    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(
        text, sizeof(text),
        "The Ashkeepers gather %" PRId64 " crowns and grave gifts at %.24s for the hidden dragon seed.",
        gathered_coins, donor->name);
    (void)PushEvent(
        sim, CC_EVENT_GOBLIN_DRAGON_SEED_PREPARED, goblins->id,
        donor->id, dragon->lifecycle_event_id,
        gathered_coins > INT32_MAX ? INT32_MAX : (int32_t)gathered_coins,
        text);
}

static void AdvanceAfterdragonCult(CcSim *sim)
{
    CcDragon *dragon = &sim->dragon;
    CcGoblinCult *goblins = &sim->goblins;
    if (dragon->afterdeath_days == 0 ||
        dragon->afterdeath_days % 365 != 0 ||
        goblins->tribute_phase != CC_GOBLIN_TRIBUTE_IDLE) return;

    GatherDragonSeedOfferings(sim);

    bool provisioned = NutritionRations(
        goblins->lair_stock, CC_NUTRITION_CIVILIAN) >= 8;
    bool armed = goblins->lair_stock[CC_GOOD_TOOLS] >= 2 &&
                 goblins->lair_stock[CC_GOOD_WEAPONS] >= 3;
    bool preserve_clutch_provisions =
        goblins->dragon_seed_phase == CC_GOBLIN_DRAGON_SEED_PREPARING &&
        goblins->dragon_seed_days_remaining <= 0 &&
        goblins->members >= 48 && goblins->devotion >= 75 &&
        goblins->cohesion >= 75;
    int32_t cult_limit = goblins->cohesion < 35 ?
        (armed && provisioned ? 36 : 24) : armed ? 84 :
        goblins->devotion >= 60 ? 48 : 24;
    if (!preserve_clutch_provisions && provisioned &&
        goblins->members < cult_limit) {
        int32_t recruits = armed ? 1 + goblins->devotion / 40 : 1;
        recruits = MinimumI32(
            recruits, cult_limit - goblins->members);
        int32_t food_cost = 2 + recruits;
        if (NutritionRations(
                goblins->lair_stock, CC_NUTRITION_CIVILIAN) >= food_cost) {
            (void)CcNutritionConsume(
                goblins->lair_stock, CC_NUTRITION_CIVILIAN,
                food_cost * CC_NUTRITION_PER_RATION);
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
    } else if (!preserve_clutch_provisions && !provisioned) {
        goblins->devotion = MaximumI32(20, goblins->devotion - 1);
        goblins->cohesion = MaximumI32(0, goblins->cohesion - 2);
    } else if (!preserve_clutch_provisions) {
        goblins->devotion = ClampI32(goblins->devotion + 1, 0, 100);
        if (provisioned && goblins->cohesion < 50) {
            goblins->cohesion += 1;
        }
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
        dragon->lifecycle_event_id = event->id;
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
        dragon->lifecycle_event_id = event->id;
    }
    if (goblins->dragon_seed_days_remaining > 0) return;

    int32_t relics = goblins->lair_stock[CC_GOOD_GOLD] +
                     goblins->lair_stock[CC_GOOD_GEMS];
    bool can_reveal_clutch = goblins->members >= 48 &&
        goblins->devotion >= 75 && goblins->cohesion >= 75 &&
        goblins->lair_coins >= 120 && relics >= 2 &&
        NutritionRations(
            goblins->lair_stock, CC_NUTRITION_CIVILIAN) >= 12 &&
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
    (void)CcNutritionConsume(
        goblins->lair_stock, CC_NUTRITION_CIVILIAN,
        12 * CC_NUTRITION_PER_RATION);
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
    dragon->lifecycle_event_id = event->id;
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
        NutritionRations(
            goblins->lair_stock, CC_NUTRITION_CIVILIAN) < 6 ||
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
    (void)CcNutritionConsume(
        goblins->lair_stock, CC_NUTRITION_CIVILIAN,
        food_cost * CC_NUTRITION_PER_RATION);
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
            if (CcNutritionConsume(
                    sim->goblins.lair_stock, CC_NUTRITION_CIVILIAN,
                    CC_NUTRITION_PER_RATION) <
                CC_NUTRITION_PER_RATION) {
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
        dragon->lifecycle_event_id = event->id;
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
    if (dragon->stolen_outstanding == 0 &&
        dragon->territory_stability > 0) {
        dragon->crown_continuity_days += 1;
    }
    if (sim->current_day % 28 == 0) {
        if (sim->current_day % (10 * 364) == 0 &&
            dragon->stolen_outstanding == 0 && dragon->hoard > 0) {
            dragon->memory_integrity = ClampI32(
                dragon->memory_integrity + 1, 0, 100);
        }
        int32_t stability_change = 0;
        if (NutritionRations(
                sim->goblins.lair_stock, CC_NUTRITION_CIVILIAN) >= 4 &&
            sim->goblins.lair_stock[CC_GOOD_TOOLS] >= 1 &&
            sim->goblins.devotion >= 50 &&
            sim->goblins.cohesion >= 50) stability_change += 1;
        if (NutritionRations(
                sim->goblins.lair_stock, CC_NUTRITION_CIVILIAN) == 0) {
            stability_change -= 2;
        }
        if (DragonTerritoryAtWar(sim)) stability_change -= 1;
        dragon->territory_stability = ClampI32(
            dragon->territory_stability + stability_change, 0, 100);
    }
    if (dragon->territory_stability == 0) {
        dragon->territoryless_days += 1;
        if (dragon->territoryless_days == 1) {
            char text[CC_EVENT_TEXT_CAPACITY];
            (void)snprintf(
                text, sizeof(text),
                "%.24s loses the last ground that answers to the dragon crown.",
                dragon->name);
            CcEvent *event = PushEvent(
                sim, CC_EVENT_DRAGON_TERRITORY_LOST, dragon->id,
                dragon->lair_settlement_id, dragon->lifecycle_event_id,
                -100, text);
            dragon->lifecycle_event_id = event->id;
        }
        if (sim->current_day % 28 == 0) {
            dragon->body_condition = MaximumI32(
                0, dragon->body_condition - 1);
            dragon->regional_influence = MaximumI32(
                0, dragon->regional_influence - 2);
        }
    } else {
        dragon->territoryless_days = 0;
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
            if (CcNutritionConsume(
                    sim->goblins.lair_stock, CC_NUTRITION_CIVILIAN,
                    CC_NUTRITION_PER_RATION) <
                CC_NUTRITION_PER_RATION) {
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
        dragon->territoryless_days >= 5 * 364) {
        ChangeDragonStage(sim, CC_DRAGON_STAGE_UNCROWNED,
                          CC_EVENT_DRAGON_UNCROWNED,
                          "five landless years break the dragon crown");
    } else if ((dragon->life_stage == CC_DRAGON_STAGE_CROWNED ||
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
        CcSettlement *lair = CcSimSettlementMutable(
            sim, dragon->lair_settlement_id);
        CcTreasure *gem = lair != NULL ? AllocateTreasure(sim) : NULL;
        if (gem != NULL) {
            (void)snprintf(gem->name, sizeof(gem->name),
                           "Wyrmheart of %.20s", dragon->name);
            gem->maker_settlement_id = lair->id;
            gem->owner_id = dragon->id;
            gem->location_id = lair->id;
            gem->gold_content = 2;
            gem->gem_content = 12;
            gem->craft_work = 50;
            gem->appraised_value = gem->gold_content * 40 +
                gem->gem_content * 70 + gem->craft_work * 10;
            gem->created_day = sim->current_day;
        }
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
    if (sim->schema_version >= 45U) {
        target->fire_damage = ClampI32(target->fire_damage + 60, 0, 100);
        target->last_fire_day = sim->current_day;
    }
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
    CcId fire_event_id = fire->id;
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
            fire_event_id, (int32_t)payment, text);
        dragon->hoard_event_id = restitution->id;
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
            fire_event_id, 28, text);
        dragon->omen_event_id = omen->id;
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

static void PrepareRoyalRouteUsage(CcSim *sim)
{
    if (sim == NULL || sim->schema_version < 38U) return;
    int32_t trade_week = sim->current_day / 7;
    if (sim->royal_trade_week == trade_week) return;
    sim->royal_trade_week = trade_week;
    for (int32_t route = 0; route < CC_MAX_ROUTES; ++route) {
        sim->royal_route_slots_used[route] = 0;
    }
}

static void UseRoyalRoute(CcSim *sim, int32_t route_slot,
                          int32_t cargo_slots)
{
    if (sim == NULL || route_slot < 0 || route_slot >= sim->route_count ||
        cargo_slots < 1) return;
    PrepareRoyalRouteUsage(sim);
    sim->royal_route_slots_used[route_slot] = ClampI32(
        sim->royal_route_slots_used[route_slot] + cargo_slots,
        0, CC_SIM_MAX_UNITS);
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
                          int32_t *total_cost, int32_t *path_capacity,
                          const int32_t route_used[CC_MAX_ROUTES],
                          bool allow_kingdom_borders,
                          CcId carriage_kingdom_id,
                          bool ignore_royal_relations,
                          int32_t required_slots)
{
    int32_t source = SettlementSlotById(sim, from_id);
    int32_t target = SettlementSlotById(sim, to_id);
    if (source < 0 || target < 0 || source == target ||
        CcSettlementIsAbandoned(&sim->settlements[source]) ||
        CcSettlementIsAbandoned(&sim->settlements[target])) return false;
    (void)good;
    int32_t distance[CC_MAX_SETTLEMENTS];
    int32_t bottleneck[CC_MAX_SETTLEMENTS];
    int32_t first_route[CC_MAX_SETTLEMENTS];
    CcId first_hop[CC_MAX_SETTLEMENTS];
    bool visited[CC_MAX_SETTLEMENTS];
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        distance[i] = INT_MAX;
        bottleneck[i] = 0;
        first_route[i] = -1;
        first_hop[i] = 0U;
        visited[i] = false;
    }
    distance[source] = 0;
    bottleneck[source] = INT_MAX;
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
            if (!allow_kingdom_borders &&
                CcSimRouteCrossesKingdomBorder(sim, route->id)) continue;
            if (carriage_kingdom_id != 0U) {
                if (!(ignore_royal_relations ?
                      RoyalRouteCanReopen(sim, route) :
                      RoyalRouteIsOfficial(sim, route)) ||
                    (!ignore_royal_relations &&
                     !CcSimRoyalCarriageCanUseRoute(
                         sim, carriage_kingdom_id, route->id))) continue;
            }
            bool war_border = CcSimRouteCrossesWarBorder(sim, route->id) &&
                              !route->smuggler_route;
            int32_t effective_capacity = TradeRouteCapacity(sim, route);
            int32_t available_capacity = effective_capacity -
                (route_used != NULL ? route_used[route_slot] : 0);
            if (available_capacity < MaximumI32(1, required_slots)) continue;
            CcId neighbor_id = route->from_id == current_id ? route->to_id :
                               route->to_id == current_id ? route->from_id : 0U;
            int32_t neighbor = SettlementSlotById(sim, neighbor_id);
            if (neighbor < 0 || visited[neighbor] ||
                CcSettlementIsAbandoned(&sim->settlements[neighbor])) continue;
            int32_t edge_cost = route->travel_days * 10 +
                                CcSimRouteDanger(sim, route->id) +
                                (war_border ? 25 : 0);
            if (distance[current] > INT_MAX - edge_cost) continue;
            int32_t candidate = distance[current] + edge_cost;
            int32_t candidate_bottleneck = MinimumI32(
                bottleneck[current], available_capacity);
            if (candidate > distance[neighbor] ||
                (candidate == distance[neighbor] &&
                 candidate_bottleneck <= bottleneck[neighbor])) continue;
            distance[neighbor] = candidate;
            bottleneck[neighbor] = candidate_bottleneck;
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
    if (path_capacity != NULL) *path_capacity = bottleneck[target];
    return true;
}

static bool RoyalPathExistsIgnoringRelations(const CcSim *sim,
                                              CcId owner_id,
                                              CcId from_id, CcId to_id,
                                              int32_t required_slots)
{
    if (from_id == to_id) return true;
    return FindTradePath(sim, from_id, to_id, CC_GOOD_FOOD,
                         NULL, NULL, NULL, NULL, NULL, true,
                         owner_id, true, required_slots);
}

static CcRoyalCarriage *RoyalCarriageForShipment(CcSim *sim,
                                                 CcId shipment_id)
{
    if (sim == NULL || shipment_id == 0U) return NULL;
    for (int32_t i = 0; i < sim->royal_carriage_count; ++i) {
        if (sim->royal_carriages[i].active_shipment_id == shipment_id) {
            return &sim->royal_carriages[i];
        }
    }
    return NULL;
}

static const CcRoyalCarriage *RoyalCarriageForShipmentConst(
    const CcSim *sim, CcId shipment_id)
{
    if (sim == NULL || shipment_id == 0U) return NULL;
    for (int32_t i = 0; i < sim->royal_carriage_count; ++i) {
        if (sim->royal_carriages[i].active_shipment_id == shipment_id) {
            return &sim->royal_carriages[i];
        }
    }
    return NULL;
}

static void ParkRoyalCarriage(CcRoyalCarriage *carriage, CcId location_id)
{
    if (carriage == NULL) return;
    carriage->location_id = location_id;
    carriage->route_id = 0U;
    carriage->destination_id = 0U;
    carriage->target_id = 0U;
    carriage->active_shipment_id = 0U;
    carriage->mode = CC_ROYAL_CARRIAGE_IDLE;
    carriage->arrival_day = 0;
    carriage->blocked_since_day = 0;
}

static void BlockRoyalCarriage(CcSim *sim, CcRoyalCarriage *carriage,
                               CcId target_id, CcId route_id,
                               CcId destination_id)
{
    if (sim == NULL || carriage == NULL) return;
    bool first_block = carriage->mode != CC_ROYAL_CARRIAGE_BLOCKED;
    carriage->mode = CC_ROYAL_CARRIAGE_BLOCKED;
    carriage->target_id = target_id;
    carriage->route_id = route_id;
    carriage->destination_id = destination_id;
    carriage->departure_day = 0;
    carriage->arrival_day = 0;
    if (carriage->blocked_since_day == 0) {
        carriage->blocked_since_day = sim->current_day;
    }
    if (!first_block) return;
    int32_t kingdom_slot = KingdomSlotById(sim, carriage->kingdom_id);
    const char *kingdom_name = kingdom_slot >= 0 ?
        sim->kingdoms[kingdom_slot].name : "A royal";
    const CcSettlement *place = CcSimSettlement(sim, carriage->location_id);
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(text, sizeof(text),
                   "%.24s carriage waits at %.24s; court relations close every legal road.",
                   kingdom_name, place != NULL ? place->name : "the border");
    (void)PushEvent(sim, CC_EVENT_ROYAL_CARRIAGE_BLOCKED,
                    carriage->id, carriage->location_id,
                    0U, 1, text);
}

static void WaitRoyalCarriageForCapacity(CcRoyalCarriage *carriage,
                                         CcId target_id, CcId route_id,
                                         CcId destination_id)
{
    if (carriage == NULL) return;
    carriage->mode = CC_ROYAL_CARRIAGE_WAITING_CAPACITY;
    carriage->target_id = target_id;
    carriage->route_id = route_id;
    carriage->destination_id = destination_id;
    carriage->departure_day = 0;
    carriage->arrival_day = 0;
    carriage->blocked_since_day = 0;
}

static bool HasRoyalCapacityWait(const CcSim *sim)
{
    if (sim == NULL) return false;
    for (int32_t i = 0; i < sim->royal_carriage_count; ++i) {
        if (sim->royal_carriages[i].mode ==
            CC_ROYAL_CARRIAGE_WAITING_CAPACITY) return true;
    }
    return false;
}

static CcSettlement *RoyalFallbackSettlement(CcSim *sim,
                                              const CcRoyalCarriage *carriage)
{
    if (sim == NULL || carriage == NULL) return NULL;
    const CcSettlement *location = CcSimSettlement(
        sim, carriage->location_id);
    CcSettlement *best = NULL;
    int64_t best_distance = INT64_MAX;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        CcSettlement *candidate = &sim->settlements[i];
        if (candidate->kingdom_id != carriage->kingdom_id ||
            CcSettlementIsAbandoned(candidate)) continue;
        int64_t dx = location != NULL ?
            (int64_t)candidate->map_x - location->map_x : 0;
        int64_t dy = location != NULL ?
            (int64_t)candidate->map_y - location->map_y : 0;
        int64_t distance = dx * dx + dy * dy;
        if (best == NULL || distance < best_distance) {
            best = candidate;
            best_distance = distance;
        }
    }
    return best;
}

static void ReleaseBlockedRoyalCarriage(CcSim *sim,
                                         CcRoyalCarriage *carriage,
                                         CcShipment *shipment,
                                         const char *reason)
{
    if (sim == NULL || carriage == NULL) return;
    CcSettlement *place = CcSimSettlementMutable(
        sim, carriage->location_id);
    if (place == NULL || CcSettlementIsAbandoned(place)) {
        place = RoyalFallbackSettlement(sim, carriage);
    }
    if (shipment != NULL && place != NULL &&
        shipment->good >= 0 && shipment->good < CC_GOOD_COUNT &&
        shipment->quantity > 0) {
        place->stock[shipment->good] = ClampI32(
            place->stock[shipment->good] + shipment->quantity,
            0, CC_SIM_MAX_UNITS);
        shipment->status = CC_SHIPMENT_ARRIVED;
    }
    CcId location_id = place != NULL ? place->id : carriage->location_id;
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(
        text, sizeof(text),
        "A royal carriage releases its load at %.32s after %.48s.",
        place != NULL ? place->name : "the nearest safe road house",
        reason != NULL ? reason : "a border delay");
    (void)PushEvent(sim, CC_EVENT_SHIPMENT_ARRIVED,
                    shipment != NULL ? shipment->id : carriage->id,
                    location_id, 0U,
                    shipment != NULL ? shipment->quantity : 0, text);
    ParkRoyalCarriage(carriage, location_id);
    carriage->departure_day = sim->current_day;
}

static void SettleChangedRoyalDestinations(CcSim *sim)
{
    if (sim->schema_version < 46U) return;
    for (int32_t i = 0; i < sim->royal_carriage_count; ++i) {
        CcRoyalCarriage *carriage = &sim->royal_carriages[i];
        if (carriage->active_shipment_id == 0U) continue;
        const CcSettlement *target = CcSimSettlement(sim, carriage->target_id);
        if (target != NULL && target->kingdom_id == carriage->kingdom_id) continue;
        for (int32_t j = 0; j < sim->shipment_count; ++j) {
            CcShipment *shipment = &sim->shipments[j];
            if (shipment->id == carriage->active_shipment_id) {
                ReleaseBlockedRoyalCarriage(sim, carriage, shipment, "the destination changed crowns");
                break;
            }
        }
    }
}

static bool StartRoyalRepositioningLeg(CcSim *sim,
                                       CcRoyalCarriage *carriage,
                                       CcId target_id)
{
    if (sim == NULL || carriage == NULL ||
        CcSimSettlement(sim, carriage->location_id) == NULL ||
        CcSimSettlement(sim, target_id) == NULL) return false;
    if (carriage->location_id == target_id) {
        ParkRoyalCarriage(carriage, target_id);
        return true;
    }
    int32_t route_slot = -1;
    CcId next_hop_id = 0U;
    if (!FindTradePath(sim, carriage->location_id, target_id,
                       CC_GOOD_FOOD, &route_slot, &next_hop_id,
                       NULL, NULL, NULL, true, carriage->kingdom_id,
                       false, 1)) {
        if (!FindTradePath(sim, carriage->location_id, target_id,
                           CC_GOOD_FOOD, &route_slot, &next_hop_id,
                           NULL, NULL, NULL, true,
                           carriage->kingdom_id, true, 1)) {
            return false;
        }
        if (!CcSimRoyalCarriageCanUseRoute(
                sim, carriage->kingdom_id,
                sim->routes[route_slot].id)) {
            BlockRoyalCarriage(sim, carriage, target_id,
                               sim->routes[route_slot].id,
                               next_hop_id);
            return false;
        }
    }
    CcRoute *route = &sim->routes[route_slot];
    carriage->route_id = route->id;
    carriage->destination_id = next_hop_id;
    carriage->target_id = target_id;
    carriage->mode = CC_ROYAL_CARRIAGE_REPOSITIONING;
    carriage->departure_day = sim->current_day;
    carriage->arrival_day = sim->current_day + route->travel_days;
    carriage->blocked_since_day = 0;
    ExchangeGossip(sim, carriage->id, carriage->location_id, "Carriage travelers");
    const CcSettlement *destination = CcSimSettlement(sim, next_hop_id);
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(text, sizeof(text),
                   "A royal carriage rides empty toward %.32s to collect a needed load.",
                   destination != NULL ? destination->name : "the next market");
    (void)PushEvent(sim, CC_EVENT_ROYAL_CARRIAGE_REROUTED,
                    carriage->id, route->id,
                    0U, route->travel_days, text);
    return true;
}

static void AdvanceRoyalCarriages(CcSim *sim)
{
    if (sim == NULL || sim->schema_version < 38U) return;
    bool reached_market = false;
    for (int32_t i = 0; i < sim->royal_carriage_count; ++i) {
        CcRoyalCarriage *carriage = &sim->royal_carriages[i];
        const CcSettlement *location = CcSimSettlement(
            sim, carriage->location_id);
        if ((location == NULL || CcSettlementIsAbandoned(location)) &&
            carriage->active_shipment_id == 0U) {
            CcSettlement *home = RoyalFallbackSettlement(sim, carriage);
            if (home != NULL) {
                ParkRoyalCarriage(carriage, home->id);
                carriage->departure_day = sim->current_day;
                reached_market = true;
            }
            continue;
        }
        if (carriage->mode == CC_ROYAL_CARRIAGE_BLOCKED &&
            carriage->active_shipment_id == 0U) {
            if (sim->current_day % 7 == 0 &&
                !CcSimRoyalCarriageCanUseRoute(
                    sim, carriage->kingdom_id, carriage->route_id)) {
                RaiseRoyalTradeSupport(sim, carriage);
            }
            if (sim->current_day - carriage->blocked_since_day >= 28) {
                ReleaseBlockedRoyalCarriage(
                    sim, carriage, NULL, "four weeks of closed talks");
                reached_market = true;
                continue;
            }
            if (CcSimRoyalCarriageCanUseRoute(
                    sim, carriage->kingdom_id, carriage->route_id)) {
                if (carriage->location_id == carriage->target_id) {
                    ParkRoyalCarriage(carriage, carriage->location_id);
                    carriage->departure_day = sim->current_day;
                    reached_market = true;
                } else {
                    (void)StartRoyalRepositioningLeg(
                        sim, carriage, carriage->target_id);
                }
            }
            continue;
        }
        if (carriage->mode == CC_ROYAL_CARRIAGE_BLOCKED &&
            sim->current_day % 7 == 0 &&
            !CcSimRoyalCarriageCanUseRoute(
                sim, carriage->kingdom_id, carriage->route_id)) {
            RaiseRoyalTradeSupport(sim, carriage);
        }
        if (carriage->mode != CC_ROYAL_CARRIAGE_REPOSITIONING ||
            carriage->arrival_day > sim->current_day) continue;
        if (!CcSimRoyalCarriageCanUseRoute(
                sim, carriage->kingdom_id, carriage->route_id)) {
            BlockRoyalCarriage(sim, carriage, carriage->target_id,
                               carriage->route_id,
                               carriage->destination_id);
            continue;
        }
        carriage->location_id = carriage->destination_id;
        ExchangeGossip(sim, carriage->id, carriage->location_id, "Carriage travelers");
        carriage->condition = ClampI32(carriage->condition - 1, 0, 100);
        if (carriage->location_id == carriage->target_id) {
            ParkRoyalCarriage(carriage, carriage->location_id);
            carriage->departure_day = sim->current_day;
            reached_market = true;
        } else {
            (void)StartRoyalRepositioningLeg(
                sim, carriage, carriage->target_id);
        }
    }
    if (reached_market && !HasRoyalCapacityWait(sim)) PlanTrade(sim);
}

static void UpdateShipments(CcSim *sim)
{
    PrepareRoyalRouteUsage(sim);
    bool reached_market = false;
    for (int32_t i = 0; i < sim->shipment_count; ++i) {
        CcShipment *shipment = &sim->shipments[i];
        CcRoyalCarriage *carriage = RoyalCarriageForShipment(
            sim, shipment->id);
        if (shipment->status == CC_SHIPMENT_BLOCKED) {
            if (carriage == NULL) continue;
            bool waited_for_capacity = carriage->mode ==
                CC_ROYAL_CARRIAGE_WAITING_CAPACITY;
            CcId final_id = shipment->final_destination_id;
            const CcSettlement *final = CcSimSettlement(sim, final_id);
            if (final == NULL || CcSettlementIsAbandoned(final)) {
                ReleaseBlockedRoyalCarriage(
                    sim, carriage, shipment,
                    "the destination fell");
                reached_market = true;
                continue;
            }
            int32_t route_slot = -1;
            CcId next_hop_id = 0U;
            int32_t cargo_slots = FreightCargoSlots(
                shipment->good, shipment->quantity);
            if (!FindTradePath(sim, carriage->location_id, final_id,
                               shipment->good, &route_slot, &next_hop_id,
                               NULL, NULL, sim->royal_route_slots_used, true,
                               carriage->kingdom_id, false,
                               cargo_slots)) {
                int32_t waiting_route_slot = -1;
                CcId waiting_hop_id = 0U;
                if (FindTradePath(
                        sim, carriage->location_id, final_id,
                        shipment->good, &waiting_route_slot,
                        &waiting_hop_id, NULL, NULL, NULL, true,
                        carriage->kingdom_id, false, cargo_slots)) {
                    WaitRoyalCarriageForCapacity(
                        carriage, final_id,
                        sim->routes[waiting_route_slot].id,
                        waiting_hop_id);
                    continue;
                }
                if (carriage->mode ==
                    CC_ROYAL_CARRIAGE_WAITING_CAPACITY) {
                    int32_t blocked_route_slot = -1;
                    CcId blocked_hop_id = 0U;
                    if (FindTradePath(
                            sim, carriage->location_id, final_id,
                            shipment->good, &blocked_route_slot,
                            &blocked_hop_id, NULL, NULL, NULL, true,
                            carriage->kingdom_id, true, cargo_slots)) {
                        BlockRoyalCarriage(
                            sim, carriage, final_id,
                            sim->routes[blocked_route_slot].id,
                            blocked_hop_id);
                    } else {
                        ReleaseBlockedRoyalCarriage(
                            sim, carriage, shipment,
                            "every safe road failed");
                        reached_market = true;
                    }
                    continue;
                }
                if (sim->current_day - carriage->blocked_since_day >= 28) {
                    ReleaseBlockedRoyalCarriage(
                        sim, carriage, shipment,
                        "four weeks of closed talks");
                    reached_market = true;
                }
                continue;
            }
            CcRoute *route = &sim->routes[route_slot];
            shipment->origin_id = carriage->location_id;
            shipment->destination_id = next_hop_id;
            shipment->route_id = route->id;
            shipment->departure_day = sim->current_day;
            shipment->arrival_day = sim->current_day + route->travel_days;
            shipment->status = CC_SHIPMENT_TRAVELLING;
            carriage->route_id = route->id;
            carriage->destination_id = next_hop_id;
            carriage->target_id = final_id;
            carriage->mode = CC_ROYAL_CARRIAGE_DELIVERING;
            carriage->departure_day = shipment->departure_day;
            carriage->arrival_day = shipment->arrival_day;
            carriage->blocked_since_day = 0;
            UseRoyalRoute(sim, route_slot, cargo_slots);
            const CcSettlement *destination = CcSimSettlement(
                sim, next_hop_id);
            char text[CC_EVENT_TEXT_CAPACITY];
            if (waited_for_capacity) {
                (void)snprintf(
                    text, sizeof(text),
                    "Weekly road space clears; the carriage resumes toward %.32s.",
                    destination != NULL ? destination->name :
                    "the next market");
            } else {
                (void)snprintf(
                    text, sizeof(text),
                    "Court relations reopen the road; the carriage resumes toward %.32s.",
                    destination != NULL ? destination->name :
                    "the next market");
            }
            (void)PushEvent(sim, CC_EVENT_ROYAL_CARRIAGE_REROUTED,
                            carriage->id, route->id,
                            0U, shipment->quantity, text);
            continue;
        }
        if (shipment->status != CC_SHIPMENT_TRAVELLING ||
            shipment->arrival_day > sim->current_day) continue;
        if (shipment->good < CC_GOOD_FOOD ||
            shipment->good >= CC_GOOD_COUNT) {
            shipment->status = CC_SHIPMENT_LOST;
            if (carriage != NULL) {
                carriage->cargo_losses += 1;
                carriage->condition = ClampI32(
                    carriage->condition - 12, 0, 100);
                ParkRoyalCarriage(carriage, shipment->origin_id);
                carriage->departure_day = sim->current_day;
            }
            continue;
        }
        if (carriage != NULL &&
            !CcSimRoyalCarriageCanUseRoute(
                sim, carriage->kingdom_id, shipment->route_id)) {
            shipment->status = CC_SHIPMENT_BLOCKED;
            BlockRoyalCarriage(sim, carriage,
                               shipment->final_destination_id,
                               shipment->route_id,
                               shipment->destination_id);
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
        if (!lost) {
            ExchangeGossip(sim, carriage != NULL ? carriage->id : shipment->id,
                            shipment->destination_id, "Carriage travelers");
        }
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
            if (carriage != NULL) {
                carriage->cargo_losses += 1;
                carriage->condition = ClampI32(
                    carriage->condition - 12, 0, 100);
                ParkRoyalCarriage(carriage, shipment->origin_id);
                carriage->departure_day = sim->current_day;
            }
            continue;
        }
        if (sim->schema_version >= 36U) {
            CcRoute *used_route = RouteMutable(sim, shipment->route_id);
            if (used_route != NULL) {
                used_route->condition = MinimumI32(100, used_route->condition + 1);
            }
        }
        CcSettlement *hop = CcSimSettlementMutable(sim, shipment->destination_id);
        if (shipment->destination_id != final_id && hop != NULL) {
            int32_t unload = 0;
            if (carriage == NULL) {
                int32_t local_need = EffectiveReserveTarget(
                                         sim, hop, shipment_good) -
                                     hop->stock[shipment_good] -
                                     CcSimIncomingGood(
                                         sim, hop->id, shipment_good);
                unload = MinimumI32(local_need, shipment->quantity / 3);
                unload = MinimumI32(unload, shipment->quantity - 4);
                unload = MaximumI32(0, unload);
                if (unload > 0) {
                    hop->stock[shipment_good] += unload;
                    hop->prosperity = ClampI32(
                        hop->prosperity + 1, 0, 100);
                    shipment->quantity -= unload;
                }
            }
            int32_t next_route_slot = -1;
            CcId next_hop_id = 0U;
            int32_t cargo_slots = FreightCargoSlots(
                shipment_good, shipment->quantity);
            if (FindTradePath(sim, hop->id, final_id, shipment_good,
                              &next_route_slot,
                              &next_hop_id, NULL, NULL,
                              carriage != NULL ?
                                  sim->royal_route_slots_used : NULL,
                              carriage != NULL,
                              carriage != NULL ? carriage->kingdom_id : 0U,
                              false, cargo_slots)) {
                CcRoute *next_route = &sim->routes[next_route_slot];
                shipment->origin_id = hop->id;
                shipment->destination_id = next_hop_id;
                shipment->route_id = next_route->id;
                shipment->departure_day = sim->current_day;
                shipment->arrival_day = sim->current_day + next_route->travel_days;
                if (carriage != NULL) {
                    carriage->location_id = hop->id;
                    carriage->route_id = next_route->id;
                    carriage->destination_id = next_hop_id;
                    carriage->target_id = final_id;
                    carriage->departure_day = shipment->departure_day;
                    carriage->arrival_day = shipment->arrival_day;
                    carriage->condition = ClampI32(
                        carriage->condition - 1, 0, 100);
                    UseRoyalRoute(sim, next_route_slot, cargo_slots);
                }
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
            if (carriage != NULL) {
                carriage->location_id = hop->id;
                shipment->status = CC_SHIPMENT_BLOCKED;
                int32_t waiting_route_slot = -1;
                CcId waiting_hop = 0U;
                if (FindTradePath(
                        sim, hop->id, final_id, shipment_good,
                        &waiting_route_slot, &waiting_hop,
                        NULL, NULL, NULL, true,
                        carriage->kingdom_id, false, cargo_slots)) {
                    WaitRoyalCarriageForCapacity(
                        carriage, final_id,
                        sim->routes[waiting_route_slot].id,
                        waiting_hop);
                    continue;
                }
                int32_t blocked_route_slot = -1;
                CcId blocked_hop = 0U;
                if (FindTradePath(
                        sim, hop->id, final_id, shipment_good,
                        &blocked_route_slot, &blocked_hop,
                        NULL, NULL, NULL, true,
                        carriage->kingdom_id, true, cargo_slots)) {
                    BlockRoyalCarriage(
                        sim, carriage, final_id,
                        sim->routes[blocked_route_slot].id,
                        blocked_hop);
                } else {
                    ReleaseBlockedRoyalCarriage(
                        sim, carriage, shipment,
                        "every safe road failed");
                    reached_market = true;
                }
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
        if (carriage != NULL) {
            carriage->condition = ClampI32(
                carriage->condition - 1, 0, 100);
            carriage->trips_completed += 1;
            ParkRoyalCarriage(carriage, final_id);
            carriage->departure_day = sim->current_day;
            reached_market = true;
        }
    }
    if (reached_market) PlanTrade(sim);
}

static CcShipment *AllocateShipment(CcSim *sim)
{
    for (int32_t i = 0; i < sim->shipment_count; ++i) {
        if (sim->shipments[i].status != CC_SHIPMENT_TRAVELLING &&
            sim->shipments[i].status != CC_SHIPMENT_BLOCKED) {
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
    if ((good == CC_GOOD_GOLD || good == CC_GOOD_GEMS) &&
        destination->stock[good == CC_GOOD_GOLD ?
                           CC_GOOD_GEMS : CC_GOOD_GOLD] > 0) {
        protected_stock = 0;
    }
    if (good == CC_GOOD_FOOD && destination->hunger >= 65 &&
        origin->hunger < 35) {
        int32_t survival_stock = MaximumI32(
            WeeklyFoodUse(sim, origin) * 6,
            origin->reserve_target[CC_GOOD_FOOD] / 2);
        protected_stock = MinimumI32(protected_stock, survival_stock);
    }
    return origin->stock[good] - protected_stock;
}

static CcMoney RoyalTradeRouteToll(const CcSim *sim, const CcRoute *route,
                                   CcId carriage_kingdom_id)
{
    CcMoney toll = TradeRouteToll(sim, route);
    const CcSettlement *from = route != NULL ?
        CcSimSettlement(sim, route->from_id) : NULL;
    const CcSettlement *to = route != NULL ?
        CcSimSettlement(sim, route->to_id) : NULL;
    if (from == NULL || to == NULL ||
        from->kingdom_id == to->kingdom_id) return toll;
    CcId host = from->kingdom_id == carriage_kingdom_id ?
        to->kingdom_id : from->kingdom_id;
    toll += CcSimKingdomsAllied(sim, carriage_kingdom_id, host) ? 1 : 3;
    return toll;
}

static bool CreateTradeShipment(CcSim *sim, CcRoyalCarriage *carriage,
                                int32_t route_slot, CcId next_hop_id,
                                CcGood good, CcSettlement *origin,
                                CcSettlement *final_destination,
                                int32_t path_capacity,
                                int32_t minimum_cargo_slots,
                                int32_t route_used[CC_MAX_ROUTES])
{
    if (sim == NULL || route_slot < 0 ||
        route_slot >= sim->route_count || origin == NULL ||
        final_destination == NULL) return false;
    bool royal = carriage != NULL;
    if (royal &&
        (carriage->mode != CC_ROYAL_CARRIAGE_IDLE ||
         carriage->active_shipment_id != 0U ||
         carriage->location_id != origin->id ||
         final_destination->kingdom_id != carriage->kingdom_id)) return false;
    CcRoute *route = &sim->routes[route_slot];
    if (royal) {
        if (!CcSimRoyalCarriageCanUseRoute(
                sim, carriage->kingdom_id, route->id)) return false;
    } else if (CcSimRouteCrossesKingdomBorder(sim, route->id) ||
               origin->kingdom_id != final_destination->kingdom_id) {
        return false;
    }
    int32_t surplus = TradeSurplus(
        sim, origin, final_destination, good);
    int32_t incoming = CcSimIncomingGood(sim, final_destination->id, good);
    int32_t need = EffectiveReserveTarget(sim, final_destination, good) -
                   final_destination->stock[good] - incoming;
    int32_t effective_capacity = TradeRouteCapacity(sim, route);
    int32_t available_capacity = effective_capacity - route_used[route_slot];
    int32_t cargo_capacity = MinimumI32(
        available_capacity, path_capacity) *
        FreightUnitsPerCargoSlot(good);
    int32_t quantity = MinimumI32(
        (royal ? CC_ROYAL_CARRIAGE_CARGO_SLOTS : 5) *
            FreightUnitsPerCargoSlot(good),
        MinimumI32(cargo_capacity, MinimumI32(surplus, need)));
    bool military_supply = WarWeeklyNeed(
        sim, final_destination, good) > 0;
    CcMoney *buyer_coins = military_supply ?
                           &final_destination->war_chest :
                           &final_destination->market_coins;
    CcKingdom *buyer_kingdom = KingdomMutable(
        sim, final_destination->kingdom_id);
    bool essential_credit = IronLedgerWillFund(final_destination, good);
    int32_t unit_price = MaximumI32(1, origin->price[good]);
    CcMoney toll = royal ? RoyalTradeRouteToll(
        sim, route, carriage->kingdom_id) : TradeRouteToll(sim, route);
    CcMoney credit_available = essential_credit ?
        IronLedgerCreditAvailable(sim, buyer_kingdom) : 0;
    CcMoney purchasing_power = *buyer_coins + credit_available;
    if (purchasing_power <= toll) return false;
    int32_t affordable = (purchasing_power - toll) / unit_price > INT32_MAX ?
                         INT32_MAX :
                         (int32_t)((purchasing_power - toll) / unit_price);
    quantity = MinimumI32(quantity, affordable);
    int32_t minimum_load =
        CcGoodDefinitionFor(good)->minimum_trade_units;
    if (quantity < minimum_load ||
        FreightCargoSlots(good, quantity) < minimum_cargo_slots) return false;
    CcShipment *shipment = AllocateShipment(sim);
    if (shipment == NULL) return false;
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
    if (royal) {
        carriage->route_id = route->id;
        carriage->destination_id = next_hop_id;
        carriage->target_id = final_destination->id;
        carriage->active_shipment_id = shipment->id;
        carriage->mode = CC_ROYAL_CARRIAGE_DELIVERING;
        carriage->departure_day = shipment->departure_day;
        carriage->arrival_day = shipment->arrival_day;
        carriage->blocked_since_day = 0;
        carriage->next_dispatch_day = sim->current_day + 7;
    }
    route_used[route_slot] += FreightCargoSlots(good, quantity);
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
    ExchangeGossip(sim, royal ? carriage->id : shipment->id,
                    origin->id, "Carriage travelers");
    return true;
}

static void PlanLegacyTrade(CcSim *sim)
{
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        int32_t minimum_load = CcGoodDefinitionFor(
            (CcGood)good)->minimum_trade_units;
        int32_t route_used[CC_MAX_ROUTES] = {0};
        for (int32_t transfer = 0;
             transfer < sim->settlement_count * 2; ++transfer) {
            int32_t best_score = 0;
            int32_t best_source = -1;
            int32_t best_destination = -1;
            int32_t best_route = -1;
            int32_t best_path_capacity = 0;
            CcId best_hop = 0U;
            for (int32_t destination = 0;
                 destination < sim->settlement_count; ++destination) {
                CcSettlement *to = &sim->settlements[destination];
                if (CcSettlementIsAbandoned(to)) continue;
                int32_t need = EffectiveReserveTarget(
                                   sim, to, (CcGood)good) - to->stock[good] -
                               CcSimIncomingGood(
                                   sim, to->id, (CcGood)good);
                if (need < minimum_load) continue;
                for (int32_t source = 0;
                     source < sim->settlement_count; ++source) {
                    if (source == destination) continue;
                    CcSettlement *from = &sim->settlements[source];
                    if (CcSettlementIsAbandoned(from)) continue;
                    int32_t surplus = TradeSurplus(
                        sim, from, to, (CcGood)good);
                    if (surplus < minimum_load) continue;
                    int32_t route_slot = -1;
                    CcId next_hop = 0U;
                    int32_t path_cost = 0;
                    int32_t path_capacity = 0;
                    if (!FindTradePath(
                            sim, from->id, to->id, (CcGood)good,
                            &route_slot, &next_hop, &path_cost,
                            &path_capacity, route_used, false, 0U,
                            false, FreightCargoSlots(
                                (CcGood)good, minimum_load))) continue;
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
                        (CcMoney)minimum_load *
                            MaximumI32(1, from->price[good]) +
                        TradeRouteToll(sim, &sim->routes[route_slot]);
                    if (buyer_coins + credit_available < minimum_cost) {
                        continue;
                    }
                    int32_t score = need * 3 + surplus +
                        ((CcGood)good == CC_GOOD_FOOD ? to->hunger * 2 : 0) -
                        path_cost / 3;
                    if (score > best_score) {
                        best_score = score;
                        best_source = source;
                        best_destination = destination;
                        best_route = route_slot;
                        best_path_capacity = path_capacity;
                        best_hop = next_hop;
                    }
                }
            }
            if (best_source < 0 || best_destination < 0 || best_route < 0) {
                break;
            }
            (void)CreateTradeShipment(
                sim, NULL, best_route, best_hop, (CcGood)good,
                &sim->settlements[best_source],
                &sim->settlements[best_destination],
                best_path_capacity, 1, route_used);
        }
    }
}

static int32_t RoyalTradeScore(const CcSim *sim,
                               const CcMaterialChainSnapshot *archive_chain,
                               const CcSettlement *destination,
                               CcGood good, int32_t need, int32_t surplus,
                               int32_t path_cost, int32_t reposition_cost)
{
    int32_t freight_need = FreightCargoSlots(good, need);
    int32_t freight_surplus = FreightCargoSlots(good, surplus);
    bool nutrition = good == CC_GOOD_BREAD ||
                     good == CC_GOOD_WHEAT || good == CC_GOOD_MEAT;
    bool military_supply = WarWeeklyNeed(sim, destination, good) > 0;
    int32_t load_slots = MinimumI32(
        CC_ROYAL_CARRIAGE_CARGO_SLOTS,
        MinimumI32(freight_need, freight_surplus));
    int32_t score = freight_need * 120 +
        MinimumI32(freight_surplus, freight_need * 2) * 20 +
        load_slots * 60 +
        (nutrition ? destination->hunger * 30 +
         (destination->hunger >= 25 ? 500 : 0) : 0) +
        (military_supply ? 180 : 0);
    if (good == CC_GOOD_TOOLS &&
        CcSettlementHasService(destination, CC_SERVICE_MINE)) score += 500;
    if (good == CC_GOOD_IRON &&
        CcSettlementHasService(destination, CC_SERVICE_SMITHY)) score += 600;
    if (good == CC_GOOD_WOOL && military_supply) score += 240;
    if (destination->id == archive_chain->scriptorium_id) {
        if (archive_chain->blocker == CC_MATERIAL_CHAIN_GRAIN &&
            good == CC_GOOD_WHEAT) score += 2400;
        if (archive_chain->blocker == CC_MATERIAL_CHAIN_PAPER &&
            (good == CC_GOOD_PAPER || good == CC_GOOD_WOOD)) score += 2000;
        if (archive_chain->blocker == CC_MATERIAL_CHAIN_TOOLS &&
            (good == CC_GOOD_TOOLS || good == CC_GOOD_IRON)) score += 2400;
        if (archive_chain->blocker == CC_MATERIAL_CHAIN_BINDING &&
            (good == CC_GOOD_GOLD || good == CC_GOOD_GEMS) &&
            destination->stock[good == CC_GOOD_GOLD ?
                               CC_GOOD_GEMS : CC_GOOD_GOLD] > 0) {
            score += 2400;
        }
    }
    return score - path_cost / 3 - reposition_cost / 4;
}

static bool RoyalTradeIsUrgent(
    const CcSim *sim, const CcMaterialChainSnapshot *archive_chain,
    const CcSettlement *destination, CcGood good)
{
    bool nutrition = good == CC_GOOD_BREAD ||
                     good == CC_GOOD_WHEAT || good == CC_GOOD_MEAT;
    if ((nutrition && destination->hunger >= 25) ||
        WarWeeklyNeed(sim, destination, good) > 0 ||
        (good == CC_GOOD_TOOLS &&
         CcSettlementHasService(destination, CC_SERVICE_MINE)) ||
        (good == CC_GOOD_IRON &&
         CcSettlementHasService(destination, CC_SERVICE_SMITHY))) return true;
    if (destination->id != archive_chain->scriptorium_id) return false;
    return (archive_chain->blocker == CC_MATERIAL_CHAIN_GRAIN &&
            good == CC_GOOD_WHEAT) ||
           (archive_chain->blocker == CC_MATERIAL_CHAIN_PAPER &&
            (good == CC_GOOD_PAPER || good == CC_GOOD_WOOD)) ||
           (archive_chain->blocker == CC_MATERIAL_CHAIN_TOOLS &&
            (good == CC_GOOD_TOOLS || good == CC_GOOD_IRON)) ||
           (archive_chain->blocker == CC_MATERIAL_CHAIN_BINDING &&
            (good == CC_GOOD_GOLD || good == CC_GOOD_GEMS));
}

static void BlockRoyalTradeDemand(
    CcSim *sim, CcRoyalCarriage *carriage,
    const CcMaterialChainSnapshot *archive_chain)
{
    if (sim->current_day % 7 != 0) return;
    bool has_permission_barrier = false;
    for (int32_t route = 0; route < sim->route_count; ++route) {
        if (RoyalRouteCanReopen(sim, &sim->routes[route]) &&
            !CcSimRoyalCarriageCanUseRoute(
                sim, carriage->kingdom_id, sim->routes[route].id)) {
            has_permission_barrier = true;
            break;
        }
    }
    if (!has_permission_barrier) return;
    int32_t best_score = 0;
    int32_t best_source = -1;
    int32_t best_destination = -1;
    int32_t best_required_slots = 0;
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        CcGood cargo_good = (CcGood)good;
        int32_t minimum_load = CcGoodDefinitionFor(
            cargo_good)->minimum_trade_units;
        int32_t required_slots = FreightCargoSlots(
            cargo_good, minimum_load);
        for (int32_t destination = 0;
             destination < sim->settlement_count; ++destination) {
            CcSettlement *to = &sim->settlements[destination];
            if (CcSettlementIsAbandoned(to) ||
                to->kingdom_id != carriage->kingdom_id) continue;
            int32_t need = EffectiveReserveTarget(sim, to, cargo_good) -
                to->stock[good] - CcSimIncomingGood(sim, to->id, cargo_good);
            if (need < minimum_load) continue;
            for (int32_t source = 0;
                 source < sim->settlement_count; ++source) {
                CcSettlement *from = &sim->settlements[source];
                if (source == destination || CcSettlementIsAbandoned(from)) {
                    continue;
                }
                int32_t surplus = TradeSurplus(sim, from, to, cargo_good);
                if (surplus < minimum_load) continue;
                int32_t load_slots = MinimumI32(
                    CC_ROYAL_CARRIAGE_CARGO_SLOTS,
                    MinimumI32(FreightCargoSlots(cargo_good, need),
                               FreightCargoSlots(cargo_good, surplus)));
                    if (load_slots < 2 &&
                        !RoyalTradeIsUrgent(
                        sim, archive_chain, to, cargo_good)) continue;
                if (!RoyalPathExistsIgnoringRelations(
                        sim, carriage->kingdom_id, from->id, to->id,
                        required_slots) ||
                    (carriage->location_id != from->id &&
                     !RoyalPathExistsIgnoringRelations(
                         sim, carriage->kingdom_id,
                         carriage->location_id, from->id, 1))) continue;
                bool source_path_open = carriage->location_id == from->id ||
                    FindTradePath(
                        sim, carriage->location_id, from->id,
                        CC_GOOD_FOOD, NULL, NULL, NULL, NULL, NULL,
                        true, carriage->kingdom_id, false, 1);
                bool delivery_path_open = FindTradePath(
                    sim, from->id, to->id, cargo_good,
                    NULL, NULL, NULL, NULL, NULL, true,
                    carriage->kingdom_id, false, required_slots);
                if (source_path_open && delivery_path_open) continue;
                bool military_supply = WarWeeklyNeed(
                    sim, to, cargo_good) > 0;
                CcMoney buyer_coins = military_supply ?
                    to->war_chest : to->market_coins;
                CcKingdom *buyer_kingdom = KingdomMutable(
                    sim, to->kingdom_id);
                CcMoney credit_available = IronLedgerWillFund(
                    to, cargo_good) ?
                    IronLedgerCreditAvailable(sim, buyer_kingdom) : 0;
                CcMoney minimum_cost =
                    (CcMoney)minimum_load *
                        MaximumI32(1, from->price[good]) + 3;
                if (buyer_coins + credit_available < minimum_cost) continue;
                int32_t score = RoyalTradeScore(
                    sim, archive_chain, to, cargo_good,
                    need, surplus, 0, 0);
                if (score > best_score) {
                    best_score = score;
                    best_source = source;
                    best_destination = destination;
                    best_required_slots = required_slots;
                }
            }
        }
    }
    if (best_source < 0 || best_destination < 0) return;
    CcSettlement *source = &sim->settlements[best_source];
    if (carriage->location_id != source->id) {
        (void)StartRoyalRepositioningLeg(sim, carriage, source->id);
        return;
    }
    int32_t blocked_route = -1;
    CcId blocked_destination = 0U;
    if (!FindTradePath(
            sim, source->id, sim->settlements[best_destination].id,
            CC_GOOD_FOOD, &blocked_route, &blocked_destination,
            NULL, NULL, NULL, true, carriage->kingdom_id, true,
            best_required_slots) ||
        CcSimRoyalCarriageCanUseRoute(
            sim, carriage->kingdom_id,
            sim->routes[blocked_route].id)) return;
    BlockRoyalCarriage(sim, carriage, carriage->location_id,
                       sim->routes[blocked_route].id,
                       blocked_destination);
}

static void PlanTrade(CcSim *sim)
{
    if (sim->schema_version < 38U) {
        PlanLegacyTrade(sim);
        return;
    }
    PrepareRoyalRouteUsage(sim);
    int32_t *route_used = sim->royal_route_slots_used;
    CcMaterialChainSnapshot archive_chain =
        CcSimMaterialChainSnapshot(sim);
    for (int32_t carriage_slot = 0;
         carriage_slot < sim->royal_carriage_count; ++carriage_slot) {
        CcRoyalCarriage *carriage = &sim->royal_carriages[carriage_slot];
        if (carriage->mode != CC_ROYAL_CARRIAGE_IDLE ||
            carriage->active_shipment_id != 0U ||
            CcSimSettlement(sim, carriage->location_id) == NULL ||
            sim->current_day < carriage->next_dispatch_day) continue;
        if (carriage->condition < 20) {
            carriage->condition = ClampI32(
                carriage->condition + 4, 0, 100);
            continue;
        }
        int32_t best_score = 0;
        int32_t best_source = -1;
        int32_t best_destination = -1;
        int32_t best_route = -1;
        int32_t best_path_capacity = 0;
        int32_t best_minimum_cargo_slots = 1;
        CcId best_hop = 0U;
        CcGood best_good = CC_GOOD_FOOD;
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
            int32_t minimum_load = CcGoodDefinitionFor(
                (CcGood)good)->minimum_trade_units;
            for (int32_t destination = 0;
                 destination < sim->settlement_count; ++destination) {
                CcSettlement *to = &sim->settlements[destination];
                if (CcSettlementIsAbandoned(to) ||
                    to->kingdom_id != carriage->kingdom_id) continue;
                int32_t need = EffectiveReserveTarget(
                                   sim, to, (CcGood)good) - to->stock[good] -
                               CcSimIncomingGood(
                                   sim, to->id, (CcGood)good);
                if (need < minimum_load) continue;
                for (int32_t source = 0;
                     source < sim->settlement_count; ++source) {
                    if (source == destination) continue;
                    CcSettlement *from = &sim->settlements[source];
                    if (CcSettlementIsAbandoned(from)) continue;
                    int32_t surplus = TradeSurplus(
                        sim, from, to, (CcGood)good);
                    if (surplus < minimum_load) continue;
                    int32_t route_slot = -1;
                    CcId next_hop = 0U;
                    int32_t path_cost = 0;
                    int32_t path_capacity = 0;
                    bool delivery_path = FindTradePath(
                            sim, from->id, to->id, (CcGood)good,
                            &route_slot, &next_hop, &path_cost,
                            &path_capacity, route_used, true,
                            carriage->kingdom_id, false,
                            FreightCargoSlots(
                                (CcGood)good, minimum_load));
                    if (!delivery_path) {
                        delivery_path = FindTradePath(
                            sim, from->id, to->id, (CcGood)good,
                            &route_slot, &next_hop, &path_cost,
                            &path_capacity, route_used, true,
                            carriage->kingdom_id, true,
                            FreightCargoSlots(
                                (CcGood)good, minimum_load));
                        if (!delivery_path ||
                            !CcSimRoyalCarriageCanUseRoute(
                                sim, carriage->kingdom_id,
                                sim->routes[route_slot].id)) continue;
                    }
                    int32_t load_slots = MinimumI32(
                        CC_ROYAL_CARRIAGE_CARGO_SLOTS,
                        MinimumI32(
                            path_capacity,
                            MinimumI32(
                                FreightCargoSlots((CcGood)good, need),
                                FreightCargoSlots((CcGood)good, surplus))));
                    bool urgent = RoyalTradeIsUrgent(
                        sim, &archive_chain, to, (CcGood)good);
                    int32_t minimum_cargo_slots = MinimumI32(
                        2, path_capacity);
                    if (load_slots < minimum_cargo_slots && !urgent) continue;
                    int32_t reposition_cost = 0;
                    if (carriage->location_id != from->id &&
                        !FindTradePath(
                            sim, carriage->location_id, from->id,
                            CC_GOOD_FOOD, NULL, NULL,
                            &reposition_cost, NULL, NULL, true,
                            carriage->kingdom_id, false, 1)) continue;
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
                    int32_t required_units = urgent ? minimum_load :
                        MaximumI32(minimum_load,
                            (minimum_cargo_slots - 1) *
                                FreightUnitsPerCargoSlot((CcGood)good) + 1);
                    CcMoney minimum_cost =
                        (CcMoney)required_units *
                            MaximumI32(1, from->price[good]) +
                        RoyalTradeRouteToll(
                            sim, &sim->routes[route_slot],
                            carriage->kingdom_id);
                    if (buyer_coins + credit_available < minimum_cost) {
                        continue;
                    }
                    int32_t score = RoyalTradeScore(
                        sim, &archive_chain, to, (CcGood)good,
                        need, surplus, path_cost, reposition_cost);
                    if (score > best_score) {
                        best_score = score;
                        best_source = source;
                        best_destination = destination;
                        best_route = route_slot;
                        best_path_capacity = path_capacity;
                        best_minimum_cargo_slots = urgent ?
                            1 : minimum_cargo_slots;
                        best_hop = next_hop;
                        best_good = (CcGood)good;
                    }
                }
            }
        }
        if (best_source < 0 || best_destination < 0 || best_route < 0) {
            BlockRoyalTradeDemand(sim, carriage, &archive_chain);
            continue;
        }
        CcSettlement *source = &sim->settlements[best_source];
        if (carriage->location_id != source->id) {
            (void)StartRoyalRepositioningLeg(sim, carriage, source->id);
            continue;
        }
        (void)CreateTradeShipment(
            sim, carriage, best_route, best_hop, best_good, source,
            &sim->settlements[best_destination],
            best_path_capacity, best_minimum_cargo_slots, route_used);
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
    for (int32_t i = 0; i < sim->front_count; ++i) {
        const CcFront *front = &sim->fronts[i];
        if (front->kind == CC_FRONT_SUPPLY_CRISIS &&
            front->status == CC_FRONT_RESOLVED) return true;
    }
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

static CcCharacter *CharacterForName(CcSim *sim, const char *name)
{
    if (sim == NULL || name == NULL || name[0] == '\0') return NULL;
    for (int32_t i = 0; i < sim->character_count; ++i) {
        if (strcmp(sim->characters[i].name, name) == 0) {
            return &sim->characters[i];
        }
    }
    return NULL;
}

static int32_t CharacterNaturalLifeDays(uint32_t seed, int32_t generation)
{
    uint32_t value = MixCharacterSeed(
        seed ^ ((uint32_t)generation + 1U) * UINT32_C(0x27d4eb2d));
    int32_t years = 64 + (int32_t)(value % 29U);
    return years * 365 + (int32_t)(MixCharacterSeed(value) % 365U);
}

static void InitializeCharacterLife(CcSim *sim, CcCharacter *character,
                                    CcId ancestor_id, int32_t generation,
                                    int32_t age_years)
{
    if (sim == NULL || character == NULL) return;
    uint32_t age_seed = MixCharacterSeed(
        character->appearance_seed ^ UINT32_C(0x6d2b79f5));
    character->ancestor_id = ancestor_id;
    character->generation = generation;
    character->birth_day = sim->current_day - age_years * 365 -
        (age_years > 0 ? (int32_t)(age_seed % 365U) : 0);
    int64_t natural_death = (int64_t)character->birth_day +
        CharacterNaturalLifeDays(character->appearance_seed, generation);
    character->death_day = natural_death > CC_SIM_MAX_DAY ?
        CC_SIM_MAX_DAY : (int32_t)natural_death;
    if (character->death_day <= sim->current_day) {
        int64_t later_death = (int64_t)sim->current_day + 365 +
            (int32_t)(age_seed % (9U * 365U));
        character->death_day = later_death > CC_SIM_MAX_DAY ?
            CC_SIM_MAX_DAY : (int32_t)later_death;
    }
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
    InitializeCharacterLife(
        sim, character, 0U, 0,
        16 + (int32_t)(MixCharacterSeed(character->appearance_seed) % 58U));
    character->player_disposition = 0;
    character->stress = activity == CC_CHARACTER_ACTIVITY_SEEKING_AID ?
        68 : 28;
    character->courage = 38 + (int32_t)(character->appearance_seed % 41U);
    return character;
}

static void RememberKnowledge(CcCharacter *character, CcKnowledgeKind kind,
                              CcId subject_id, CcId source_character_id,
                              CcId event_id, CcKnowledgeCertainty certainty,
                              bool private_knowledge, int32_t day)
{
    if (character == NULL || kind == CC_KNOWLEDGE_NONE) return;
    for (int32_t i = 0; i < character->knowledge_count; ++i) {
        CcCharacterKnowledge *existing = &character->knowledge[i];
        if (existing->kind != kind || existing->subject_id != subject_id) {
            continue;
        }
        if (certainty >= existing->certainty) {
            existing->source_character_id = source_character_id;
            existing->event_id = event_id;
            existing->certainty = certainty;
            existing->private_knowledge = private_knowledge;
            existing->day = day;
        }
        return;
    }
    int32_t slot = character->knowledge_write_index;
    character->knowledge[slot] = (CcCharacterKnowledge){
        .kind = kind,
        .subject_id = subject_id,
        .source_character_id = source_character_id,
        .event_id = event_id,
        .certainty = certainty,
        .private_knowledge = private_knowledge,
        .day = day
    };
    character->knowledge_write_index =
        (slot + 1) % CC_CHARACTER_KNOWLEDGE_CAPACITY;
    if (character->knowledge_count < CC_CHARACTER_KNOWLEDGE_CAPACITY) {
        character->knowledge_count += 1;
    }
}

static CcRelationship *RelationshipMutable(CcSim *sim,
                                           CcId from_character_id,
                                           CcId to_character_id)
{
    return (CcRelationship *)CcSimRelationship(
        sim, from_character_id, to_character_id);
}

static CcRelationship *EnsureRelationship(
    CcSim *sim, CcId from_character_id, CcId to_character_id,
    CcRelationshipHistory history, int32_t affinity, int32_t trust,
    int32_t obligation, CcId cause_event_id)
{
    CcRelationship *relationship = RelationshipMutable(
        sim, from_character_id, to_character_id);
    if (relationship != NULL) return relationship;
    if (sim == NULL || sim->relationship_count >= CC_MAX_RELATIONSHIPS ||
        CcSimCharacter(sim, from_character_id) == NULL ||
        CcSimCharacter(sim, to_character_id) == NULL ||
        from_character_id == to_character_id) return NULL;
    relationship = &sim->relationships[sim->relationship_count++];
    *relationship = (CcRelationship){
        .from_character_id = from_character_id,
        .to_character_id = to_character_id,
        .affinity = ClampI32(affinity, -3, 3),
        .trust = ClampI32(trust, -3, 3),
        .obligation = ClampI32(obligation, -3, 3),
        .history = history,
        .cause_event_id = cause_event_id
    };
    return relationship;
}

static void InitializeMineSocialThread(CcSim *sim, CcSituation *situation,
                                       CcCharacter *participant,
                                       CcCharacter *sponsor,
                                       CcCharacter *witness)
{
    if (sim == NULL || situation == NULL || participant == NULL ||
        sponsor == NULL || witness == NULL ||
        situation->kind != CC_SITUATION_MONSTER_EXPEDITION) {
        return;
    }
    situation->witness_character_id = witness->id;
    if (situation->discovery_stage == CC_DISCOVERY_OFFER &&
        situation->lead_event_id == 0U) {
        situation->discovery_stage = CC_DISCOVERY_RUMOR;
        situation->lead_path = CC_LEAD_PATH_UNDECIDED;
    }

    const CcRelationship *existing = CcSimRelationship(
        sim, participant->id, sponsor->id);
    CcId relationship_cause_id = situation->cause_event_id;
    if (existing == NULL) {
        uint32_t choice = (sim->world_seed ^
                           (uint32_t)(situation->id >> 8)) % 3U;
        CcRelationshipHistory history = choice == 0U ?
            CC_RELATIONSHIP_HISTORY_OLD_FRIENDS : choice == 1U ?
            CC_RELATIONSHIP_HISTORY_FORMER_PARTNERS :
            CC_RELATIONSHIP_HISTORY_PROFESSIONAL_RIVALS;
        int32_t affinity = history == CC_RELATIONSHIP_HISTORY_OLD_FRIENDS ?
            2 : history == CC_RELATIONSHIP_HISTORY_FORMER_PARTNERS ? 1 : -1;
        int32_t trust = history == CC_RELATIONSHIP_HISTORY_OLD_FRIENDS ?
            2 : history == CC_RELATIONSHIP_HISTORY_FORMER_PARTNERS ? -1 : 0;
        int32_t obligation = history ==
            CC_RELATIONSHIP_HISTORY_FORMER_PARTNERS ? 1 : 0;
        char history_text[CC_EVENT_TEXT_CAPACITY];
        if (history == CC_RELATIONSHIP_HISTORY_OLD_FRIENDS) {
            (void)snprintf(history_text, sizeof(history_text),
                           "%s and %s worked together for years and still trust each other.",
                           participant->name, sponsor->name);
        } else if (history == CC_RELATIONSHIP_HISTORY_FORMER_PARTNERS) {
            (void)snprintf(history_text, sizeof(history_text),
                           "%s and %s once lived and worked together. They separated during a shortage.",
                           participant->name, sponsor->name);
        } else {
            (void)snprintf(history_text, sizeof(history_text),
                           "%s and %s have argued about mine safety for years.",
                           participant->name, sponsor->name);
        }
        CcEvent *history_event = PushSocialEvent(
            sim, CC_EVENT_RELATIONSHIP_HISTORY, situation->id,
            participant->home_settlement_id, situation->cause_event_id,
            participant->id, sponsor->id, 0U, 0U, 0, history_text);
        CcId history_event_id = history_event->id;
        relationship_cause_id = history_event_id;
        (void)EnsureRelationship(
            sim, participant->id, sponsor->id, history, affinity, trust,
            obligation, history_event_id);
        (void)EnsureRelationship(
            sim, sponsor->id, participant->id, history,
            affinity > 0 ? affinity - 1 : affinity,
            trust == 2 ? 1 : trust, -obligation, history_event_id);
    } else if (relationship_cause_id == 0U) {
        relationship_cause_id = existing->cause_event_id;
    }
    (void)EnsureRelationship(
        sim, participant->id, witness->id,
        CC_RELATIONSHIP_HISTORY_COWORKERS,
        1, 2, 0, relationship_cause_id);
    (void)EnsureRelationship(
        sim, witness->id, participant->id,
        CC_RELATIONSHIP_HISTORY_COWORKERS,
        1, 2, 0, relationship_cause_id);

    if (situation->lead_event_id == 0U) {
        char text[CC_EVENT_TEXT_CAPACITY];
        (void)snprintf(text, sizeof(text),
                       "%s ran out of the west gallery and left a lamp. %s heard the warning.",
                       witness->name, participant->name);
        CcEvent *rumor = PushSocialEvent(
            sim, CC_EVENT_RUMOR_SHARED, situation->id,
            participant->home_settlement_id, situation->cause_event_id,
            witness->id, participant->id, participant->id, witness->id,
            1, text);
        situation->lead_event_id = rumor->id;
    }
    CcId witness_event_id = situation->cause_event_id != 0U ?
        situation->cause_event_id : situation->lead_event_id;
    RememberKnowledge(witness, CC_KNOWLEDGE_WITNESS_ACCOUNT, situation->id,
                      witness->id, witness_event_id,
                      CC_KNOWLEDGE_WITNESSED, true, sim->current_day);
    RememberKnowledge(participant, CC_KNOWLEDGE_PROBLEM_RUMOR, situation->id,
                      witness->id, situation->lead_event_id,
                      CC_KNOWLEDGE_TOLD, true, sim->current_day);
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

static CcCharacter *AdultCharacterAt(CcSim *sim, CcId settlement_id,
                                     CcCharacterRole preferred_role,
                                     CcId excluded_first,
                                     CcId excluded_second)
{
    CcCharacter *best = NULL;
    for (int32_t pass = 0; pass < 3 && best == NULL; ++pass) {
        int32_t best_score = INT32_MIN;
        for (int32_t i = 0; i < sim->character_count; ++i) {
            CcCharacter *character = &sim->characters[i];
            if ((pass == 0 &&
                 character->home_settlement_id != settlement_id) ||
                character->id == excluded_first ||
                character->id == excluded_second ||
                (pass < 2 &&
                 CcCharacterAgeYears(sim, character) < 16)) continue;
            int32_t score = character->role == preferred_role ? 100 : 0;
            score += character->activity == CC_CHARACTER_ACTIVITY_WORKING ?
                10 : 0;
            score += CcCharacterAgeYears(sim, character) / 5;
            if (best == NULL || score > best_score ||
                (score == best_score && character->id < best->id)) {
                best = character;
                best_score = score;
            }
        }
    }
    return best;
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
    CcId affected_home = sim->settlements[slot].id;
    CcId sponsor_home = situation->kind ==
            CC_SITUATION_MONSTER_EXPEDITION && sim->settlement_count > 0 ?
        sim->settlements[0].id :
        CcSimSituationOfferSettlementId(sim, situation);
    if (sponsor_home == 0U) sponsor_home = affected_home;
    CcCharacter *sponsor = CharacterMutable(
        sim, situation->sponsor_character_id);
    if (sponsor == NULL && sim->current_day == 1) {
        int32_t sponsor_slot = situation->kind ==
                CC_SITUATION_RELIEF_DELIVERY ? 0 :
            situation->kind == CC_SITUATION_BLACK_MARKET_DELIVERY ? 1 : slot;
        const char *founder = situation->kind ==
            CC_SITUATION_MONSTER_EXPEDITION ? "Mara Venn" :
            sponsors[sponsor_slot];
        sponsor = PromoteCharacter(
            sim, founder, sponsor_home, situation->issuer_faction_id,
            CC_CHARACTER_OFFICIAL, CC_CHARACTER_GOAL_KEEP_ORDER,
            CC_CHARACTER_ACTIVITY_WORKING);
    }
    if (sponsor == NULL) {
        sponsor = AdultCharacterAt(
            sim, sponsor_home, CC_CHARACTER_OFFICIAL, 0U, 0U);
    }
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
    CcCharacter *participant = CharacterMutable(
        sim, situation->affected_character_id);
    if (participant == NULL && sim->current_day == 1) {
        participant = PromoteCharacter(
            sim, affected[slot], affected_home, 0U, affected_role,
            affected_goal, CC_CHARACTER_ACTIVITY_SEEKING_AID);
    }
    if (participant == NULL) {
        participant = AdultCharacterAt(
            sim, affected_home, affected_role,
            sponsor != NULL ? sponsor->id : 0U, 0U);
    }
    if (sponsor != NULL) {
        sponsor->current_settlement_id = sponsor_home;
        sponsor->faction_id = situation->issuer_faction_id;
        sponsor->role = CC_CHARACTER_OFFICIAL;
        sponsor->goal = CC_CHARACTER_GOAL_KEEP_ORDER;
        CopyName(situation->sponsor_name, sponsor->name);
    }
    if (participant != NULL) {
        participant->current_settlement_id = affected_home;
        participant->role = affected_role;
        participant->goal = affected_goal;
        CopyName(situation->affected_name, participant->name);
    }
    situation->sponsor_character_id = sponsor != NULL ? sponsor->id : 0U;
    situation->affected_character_id = participant != NULL ?
        participant->id : 0U;
    if (situation->kind == CC_SITUATION_RELIEF_DELIVERY &&
        situation->cause_event_id != 0U) {
        RememberKnowledge(
            sponsor, CC_KNOWLEDGE_OFFER, situation->id,
            sponsor != NULL ? sponsor->id : 0U,
            situation->cause_event_id, CC_KNOWLEDGE_WITNESSED,
            false, sim->current_day);
        RememberKnowledge(
            participant, CC_KNOWLEDGE_IMMEDIATE_STAKE, situation->id,
            participant != NULL ? participant->id : 0U,
            situation->cause_event_id, CC_KNOWLEDGE_WITNESSED,
            false, sim->current_day);
    }
    if (situation->kind == CC_SITUATION_MONSTER_EXPEDITION) {
        CcCharacter *witness = CharacterMutable(
            sim, situation->witness_character_id);
        if (witness == NULL && sim->current_day == 1) {
            witness = PromoteCharacter(
                sim, "Bren Alder", affected_home, 0U,
                CC_CHARACTER_LABORER,
                CC_CHARACTER_GOAL_SECURE_LIVELIHOOD,
                CC_CHARACTER_ACTIVITY_WORKING);
        }
        if (witness == NULL) {
            witness = AdultCharacterAt(
                sim, affected_home, CC_CHARACTER_LABORER,
                participant != NULL ? participant->id : 0U,
                sponsor != NULL ? sponsor->id : 0U);
        }
        if (witness != NULL) {
            witness->current_settlement_id = affected_home;
        }
        InitializeMineSocialThread(sim, situation, participant, sponsor,
                                   witness);
        if (situation->status != CC_SITUATION_ACTIVE ||
            sim->player.accepted_situation_id == situation->id) {
            situation->discovery_stage = CC_DISCOVERY_OFFER;
        }
    }
    RefreshSituationCharacterActivities(sim, situation);
}

static void FillSettlementResidents(CcSim *sim)
{
    if (sim == NULL) return;
    for (int32_t settlement = 0;
         settlement < sim->settlement_count; ++settlement) {
        CcId settlement_id = sim->settlements[settlement].id;
        int32_t residents = 0;
        for (int32_t i = 0; i < sim->character_count; ++i) {
            if (sim->characters[i].home_settlement_id == settlement_id) {
                residents += 1;
            }
        }
        uint32_t ordinal = 0U;
        while (residents < 4 && sim->character_count < CC_MAX_CHARACTERS) {
            char name[CC_NAME_CAPACITY];
            do {
                CcGenerateCharacterName(
                    sim->world_seed, settlement_id, 0, ordinal++, name);
            } while (CharacterForName(sim, name) != NULL && ordinal < 2048U);
            CcCharacterRole role = residents == 0 ? CC_CHARACTER_OFFICIAL :
                residents == 1 ? CC_CHARACTER_LABORER :
                residents == 2 ? CC_CHARACTER_SCOUT :
                                 CC_CHARACTER_TRAVELLER;
            CcCharacterGoal goal = role == CC_CHARACTER_OFFICIAL ?
                CC_CHARACTER_GOAL_KEEP_ORDER :
                CC_CHARACTER_GOAL_SECURE_LIVELIHOOD;
            if (PromoteCharacter(sim, name, settlement_id, 0U, role, goal,
                                 CC_CHARACTER_ACTIVITY_WORKING) == NULL) {
                break;
            }
            residents += 1;
        }
    }
}

void CcSimUpgradeCharacterLifecycles(CcSim *sim)
{
    if (sim == NULL) return;
    for (int32_t i = 0; i < sim->character_count; ++i) {
        CcCharacter *character = &sim->characters[i];
        if (character->death_day > sim->current_day &&
            character->birth_day <= sim->current_day) continue;
        int32_t age = 16 + (int32_t)(
            MixCharacterSeed(character->appearance_seed) % 58U);
        InitializeCharacterLife(sim, character, 0U, 0, age);
    }
    FillSettlementResidents(sim);
}

void CcSimInitializeCharacters(CcSim *sim)
{
    if (sim == NULL) return;
    CcSimUpgradeCharacterLifecycles(sim);
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        AssignSituationCast(sim, &sim->situations[i]);
    }
}

static void RemoveCharacterRelationships(CcSim *sim, CcId character_id)
{
    int32_t write = 0;
    for (int32_t i = 0; i < sim->relationship_count; ++i) {
        const CcRelationship *relationship = &sim->relationships[i];
        if (relationship->from_character_id == character_id ||
            relationship->to_character_id == character_id) continue;
        if (write != i) sim->relationships[write] = *relationship;
        write += 1;
    }
    for (int32_t i = write; i < sim->relationship_count; ++i) {
        sim->relationships[i] = (CcRelationship){0};
    }
    sim->relationship_count = write;
}

static void RemoveCharacterKnowledgeSources(CcSim *sim, CcId character_id)
{
    for (int32_t i = 0; i < sim->character_count; ++i) {
        CcCharacter *character = &sim->characters[i];
        CcCharacterKnowledge kept[CC_CHARACTER_KNOWLEDGE_CAPACITY] = {0};
        int32_t count = 0;
        for (int32_t knowledge = 0;
             knowledge < character->knowledge_count; ++knowledge) {
            const CcCharacterKnowledge *item =
                &character->knowledge[knowledge];
            if (item->source_character_id == character_id) continue;
            kept[count++] = *item;
        }
        for (int32_t knowledge = 0;
             knowledge < CC_CHARACTER_KNOWLEDGE_CAPACITY; ++knowledge) {
            character->knowledge[knowledge] = kept[knowledge];
        }
        character->knowledge_count = count;
        character->knowledge_write_index =
            count % CC_CHARACTER_KNOWLEDGE_CAPACITY;
    }
}

static void SuccessorName(const CcSim *sim, const CcCharacter *ancestor,
                          int32_t generation, uint32_t ordinal,
                          char output[CC_NAME_CAPACITY])
{
    const char *family = strrchr(ancestor->name, ' ');
    for (uint32_t attempt = 0U; attempt < 2048U; ++attempt) {
        char generated[CC_NAME_CAPACITY];
        CcGenerateCharacterName(
            sim->world_seed, ancestor->home_settlement_id, generation,
            ordinal + attempt, generated);
        char *space = strchr(generated, ' ');
        if (family != NULL && space != NULL) {
            *space = '\0';
            (void)snprintf(output, CC_NAME_CAPACITY, "%s%s",
                           generated, family);
        } else {
            CopyName(output, generated);
        }
        if (CharacterForName((CcSim *)sim, output) == NULL) return;
    }
    (void)snprintf(output, CC_NAME_CAPACITY, "Heir %u",
                   ordinal % 100000U);
}

static void RecastSituationsAfterDeath(CcSim *sim, CcId character_id)
{
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        CcSituation *situation = &sim->situations[i];
        if (situation->status != CC_SITUATION_ACTIVE) continue;
        bool changed = false;
        if (situation->sponsor_character_id == character_id) {
            situation->sponsor_character_id = 0U;
            situation->sponsor_name[0] = '\0';
            changed = true;
        }
        if (situation->affected_character_id == character_id) {
            situation->affected_character_id = 0U;
            situation->affected_name[0] = '\0';
            changed = true;
        }
        if (situation->witness_character_id == character_id) {
            situation->witness_character_id = 0U;
            changed = true;
        }
        if (!changed) continue;
        if (situation->kind == CC_SITUATION_MONSTER_EXPEDITION) {
            situation->lead_event_id = 0U;
            situation->discovery_stage = CC_DISCOVERY_RUMOR;
            situation->lead_path = CC_LEAD_PATH_UNDECIDED;
        }
        AssignSituationCast(sim, situation);
    }
}

static void TransferHistoricalTitles(CcSim *sim,
                                     const CcCharacter *dead,
                                     const CcCharacter *successor)
{
    if (sim->archives.abbot_character_id == dead->id) {
        sim->archives.abbot_character_id = successor->id;
        char text[CC_EVENT_TEXT_CAPACITY];
        (void)snprintf(text, sizeof(text),
                       "%.24s succeeds %.24s as Abbot of the Scriptorium.",
                       successor->name, dead->name);
        (void)PushEvent(sim, CC_EVENT_MONASTIC_SUCCESSION,
                        successor->id, successor->home_settlement_id,
                        0U, 30, text);
    }
    for (int32_t i = 0; i < sim->kingdom_count; ++i) {
        CcKingdom *kingdom = &sim->kingdoms[i];
        if (kingdom->ruler_character_id == dead->id) {
            kingdom->ruler_character_id = successor->id;
            kingdom->anointed_by_character_id = 0U;
            /* A new ruler is not anointed; the abbey must sanction the
             * succession before the crown becomes lawful again. */
            kingdom->anointed = false;
            char text[CC_EVENT_TEXT_CAPACITY];
            (void)snprintf(text, sizeof(text),
                           "%.20s succeeds %.20s as ruler of %.20s.",
                           successor->name, dead->name, kingdom->name);
            (void)PushEvent(sim, CC_EVENT_ROYAL_SUCCESSION,
                            successor->id, successor->home_settlement_id,
                            0U, 25, text);
        }
        if (kingdom->monastery_patron_id == dead->id) {
            kingdom->monastery_patron_id = successor->id;
        }
        if (kingdom->anointed_by_character_id == dead->id) {
            kingdom->anointed_by_character_id = successor->id;
        }
    }
    if (sim->dragon_campaign.patron_character_id == dead->id) {
        sim->dragon_campaign.patron_character_id = successor->id;
    }
    if (sim->dragon_campaign.hero_character_id == dead->id) {
        sim->dragon_campaign.hero_character_id = successor->id;
    }
    for (int32_t i = 0; i < sim->treasure_count; ++i) {
        CcTreasure *treasure = &sim->treasures[i];
        if (!treasure->destroyed && treasure->owner_id == dead->id) {
            treasure->owner_id = successor->id;
            treasure->location_id = successor->home_settlement_id;
        }
    }
}

static void ReplaceDeadCharacter(CcSim *sim, int32_t slot)
{
    CcCharacter dead = sim->characters[slot];
    int32_t age = CcCharacterAgeYears(sim, &dead);
    const CcSettlement *home = CcSimSettlement(sim, dead.home_settlement_id);
    char death_text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(death_text, sizeof(death_text),
                   "%s died at age %d after a life in %s.",
                   dead.name, age,
                   home != NULL ? home->name : "a forgotten place");
    (void)PushEvent(sim, CC_EVENT_CHARACTER_DIED, dead.id,
                    dead.home_settlement_id, 0U, 30, death_text);
    sim->character_deaths += 1;

    RemoveCharacterRelationships(sim, dead.id);
    RemoveCharacterKnowledgeSources(sim, dead.id);

    CcCharacter successor = {0};
    successor.id = NextId(sim, CC_ENTITY_CHARACTER);
    successor.home_settlement_id = dead.home_settlement_id;
    successor.current_settlement_id = dead.home_settlement_id;
    /* A heir is not born into a ghost town: when the family seat is a
     * ruin, the successor is born wherever the family fled. */
    {
        const CcSettlement *seat = CcSimSettlement(sim, dead.home_settlement_id);
        if (seat != NULL && CcSettlementIsAbandoned(seat)) {
            CcSettlement *fled_to = FallenTownHost(
                sim, seat,
                CharacterHoldsOffice(sim, dead.id) ?
                    seat->kingdom_id : 0U);
            if (fled_to != NULL) {
                successor.home_settlement_id = fled_to->id;
                successor.current_settlement_id = fled_to->id;
            }
        }
    }
    successor.role = dead.role;
    successor.goal = dead.goal;
    successor.activity = CC_CHARACTER_ACTIVITY_WORKING;
    successor.appearance_seed = (uint32_t)(
        successor.id ^ (successor.id >> 32U) ^ sim->world_seed ^
        UINT32_C(0x9e3779b9));
    successor.stress = 0;
    successor.courage = 38 +
        (int32_t)(successor.appearance_seed % 41U);
    SuccessorName(sim, &dead, dead.generation + 1,
                  (uint32_t)sim->character_births, successor.name);
    InitializeCharacterLife(
        sim, &successor, dead.id, dead.generation + 1, 0);
    sim->characters[slot] = successor;
    sim->character_births += 1;
    TransferHistoricalTitles(sim, &dead, &sim->characters[slot]);

    char birth_text[CC_EVENT_TEXT_CAPACITY];
    {
        const CcSettlement *birthplace = CcSimSettlement(
            sim, successor.home_settlement_id);
        (void)snprintf(birth_text, sizeof(birth_text),
                       "%s was born into the family of %s in %s.",
                       successor.name, dead.name,
                       birthplace != NULL ? birthplace->name : "a forgotten place");
    }
    (void)PushEvent(sim, CC_EVENT_CHARACTER_BORN, successor.id,
                    successor.home_settlement_id, 0U, 12, birth_text);
    RecastSituationsAfterDeath(sim, dead.id);
}

static void AdvanceCharacterLifecycles(CcSim *sim)
{
    for (int32_t i = 0; i < sim->character_count; ++i) {
        if (sim->characters[i].death_day <= sim->current_day) {
            ReplaceDeadCharacter(sim, i);
        }
    }
}

static CcFront *FrontMutable(CcSim *sim, CcId id)
{
    return (CcFront *)CcSimFront((const CcSim *)sim, id);
}

static CcFront *AllocateFront(CcSim *sim)
{
    if (sim->front_count < CC_MAX_FRONTS) {
        CcFront *front = &sim->fronts[sim->front_count++];
        *front = (CcFront){0};
        return front;
    }
    int32_t oldest = -1;
    for (int32_t i = 0; i < sim->front_count; ++i) {
        if (sim->fronts[i].status == CC_FRONT_ACTIVE) continue;
        if (oldest < 0 || sim->fronts[i].created_day <
                          sim->fronts[oldest].created_day) {
            oldest = i;
        }
    }
    if (oldest < 0) return NULL;
    sim->fronts[oldest] = (CcFront){0};
    return &sim->fronts[oldest];
}

static CcQuestOutcomeRecord *AllocateQuestOutcome(CcSim *sim)
{
    if (sim->quest_outcome_count < CC_MAX_QUEST_OUTCOMES) {
        CcQuestOutcomeRecord *outcome =
            &sim->quest_outcomes[sim->quest_outcome_count++];
        *outcome = (CcQuestOutcomeRecord){0};
        return outcome;
    }
    int32_t oldest = 0;
    for (int32_t i = 1; i < sim->quest_outcome_count; ++i) {
        if (sim->quest_outcomes[i].resolved_day <
            sim->quest_outcomes[oldest].resolved_day) oldest = i;
    }
    sim->quest_outcomes[oldest] = (CcQuestOutcomeRecord){0};
    return &sim->quest_outcomes[oldest];
}

static CcId FrontAnchorForSituation(const CcSim *sim,
                                    const CcSituation *situation)
{
    if (situation == NULL) return 0U;
    if (CcQuestFrontForSituationKind(situation->kind) ==
        CC_FRONT_SUPPLY_CRISIS) {
        CcId offer = CcSimSituationOfferSettlementId(sim, situation);
        return offer != 0U ? offer : situation->target_id;
    }
    return situation->target_id;
}

static void WriteFrontPremise(CcFront *front)
{
    if (front == NULL) return;
    const char *premise = "The region is choosing what happens next.";
    switch (front->kind) {
        case CC_FRONT_SUPPLY_CRISIS:
            premise = "Food and passage are failing together; every remedy will choose who holds the road.";
            break;
        case CC_FRONT_MONSTER_PRESSURE:
            premise = "Pressure below the workings is making the road, mine, and settlement answer one another.";
            break;
        case CC_FRONT_COURIER_DISPATCH:
            premise = "A sealed message is crossing a road that can still change its meaning.";
            break;
    }
    (void)snprintf(front->premise, sizeof(front->premise), "%s", premise);
}

static CcFront *FindOpenFrontForSituation(CcSim *sim,
                                         const CcSituation *situation)
{
    CcFrontKind kind = CcQuestFrontForSituationKind(situation->kind);
    CcId anchor = FrontAnchorForSituation(sim, situation);
    for (int32_t i = 0; i < sim->front_count; ++i) {
        CcFront *front = &sim->fronts[i];
        bool same_crisis = kind == CC_FRONT_SUPPLY_CRISIS ||
                           front->anchor_id == anchor;
        if (front->status == CC_FRONT_ACTIVE && front->kind == kind &&
            same_crisis &&
            front->situation_count < CC_MAX_FRONT_SITUATIONS) return front;
    }
    return NULL;
}

static CcFront *CreateFrontForSituation(CcSim *sim,
                                       const CcSituation *situation,
                                       bool emit_event)
{
    CcFront *front = AllocateFront(sim);
    if (front == NULL) return NULL;
    front->id = NextId(sim, CC_ENTITY_FRONT);
    front->kind = CcQuestFrontForSituationKind(situation->kind);
    front->status = CC_FRONT_ACTIVE;
    front->anchor_id = FrontAnchorForSituation(sim, situation);
    front->cause_event_id = situation->cause_event_id;
    front->created_day = situation->created_day;
    CcQuestClockBegin(&front->portent,
                      situation->objective.danger.limit,
                      situation->objective.progress.created_by_event_id);
    WriteFrontPremise(front);
    if (emit_event) {
        char text[CC_EVENT_TEXT_CAPACITY];
        (void)snprintf(text, sizeof(text), "%s begins: %.96s",
                       CcFrontKindName(front->kind), front->premise);
        CcEvent *event = PushEvent(
            sim, CC_EVENT_FRONT_CREATED, front->id,
            CcSimSituationOfferSettlementId(sim, situation),
            situation->cause_event_id, front->portent.limit, text);
        front->created_event_id = event->id;
        front->portent.created_by_event_id = front->created_event_id;
    }
    return front;
}

static void AttachSituationToFront(CcSim *sim, CcSituation *situation,
                                   bool emit_event)
{
    if (sim == NULL || situation == NULL || situation->front_id != 0U) {
        return;
    }
    CcFront *front = FindOpenFrontForSituation(sim, situation);
    if (front == NULL) {
        front = CreateFrontForSituation(sim, situation, emit_event);
    }
    if (front == NULL || front->situation_count >=
                         CC_MAX_FRONT_SITUATIONS) return;
    front->situation_ids[front->situation_count++] = situation->id;
    front->portent.limit = MaximumI32(
        front->portent.limit, situation->objective.danger.limit);
    situation->front_id = front->id;
}

static void InitializeSituationObjective(CcSituation *situation,
                                         CcId created_event_id)
{
    if (situation == NULL) return;
    int32_t required = situation->quantity > 0 ? situation->quantity : 1;
    int32_t danger_limit = MaximumI32(
        1, situation->deadline_day - situation->created_day + 1);
    situation->objective = (CcQuestObjective){
        .kind = CcQuestObjectiveForSituationKind(situation->kind),
        .target_id = situation->target_id,
        .good = situation->good,
        .required = required
    };
    CcQuestClockBegin(&situation->objective.progress, required,
                      created_event_id);
    CcQuestClockBegin(&situation->objective.danger, danger_limit,
                      created_event_id);
    situation->objective.progress.value = MinimumI32(
        required, situation->progress);
    situation->end_reason = CC_QUEST_END_NONE;
}

static void SyncSituationObjectiveDefinition(CcSituation *situation)
{
    if (situation == NULL) return;
    int32_t required = situation->quantity > 0 ? situation->quantity : 1;
    int32_t danger_limit = MaximumI32(
        1, situation->deadline_day - situation->created_day + 1);
    situation->objective.kind =
        CcQuestObjectiveForSituationKind(situation->kind);
    situation->objective.target_id = situation->target_id;
    situation->objective.good = situation->good;
    situation->objective.required = required;
    situation->objective.progress.limit = required;
    situation->objective.progress.value = MinimumI32(
        required, MaximumI32(0, situation->progress));
    situation->objective.danger.limit = danger_limit;
    situation->objective.danger.value = MinimumI32(
        danger_limit, MaximumI32(0, situation->objective.danger.value));
}

static bool RecordSituationEvidence(CcSituation *situation,
                                    CcId event_id, int32_t amount)
{
    if (situation == NULL || situation->status != CC_SITUATION_ACTIVE) {
        return false;
    }
    SyncSituationObjectiveDefinition(situation);
    bool recorded = CcQuestRecordEvidence(
        &situation->objective, event_id, amount);
    if (recorded && situation->quantity > 0) {
        situation->progress = situation->objective.progress.value;
    }
    return recorded;
}

static void ArchiveSituationOutcome(CcSim *sim,
                                    const CcSituation *situation,
                                    CcId resolved_event_id)
{
    if (sim == NULL || situation == NULL ||
        CcSimQuestOutcome(sim, situation->id) != NULL) return;
    CcQuestOutcomeRecord *record = AllocateQuestOutcome(sim);
    const CcFront *front = CcSimSituationFront(sim, situation);
    *record = (CcQuestOutcomeRecord){
        .id = NextId(sim, CC_ENTITY_QUEST_OUTCOME),
        .situation_id = situation->id,
        .front_id = situation->front_id,
        .situation_kind = situation->kind,
        .front_kind = front != NULL ? front->kind :
            CcQuestFrontForSituationKind(situation->kind),
        .situation_status = situation->status,
        .end_reason = situation->end_reason,
        .front_outcome = front != NULL ? front->outcome :
            CC_FRONT_OUTCOME_NONE,
        .target_id = situation->target_id,
        .sponsor_character_id = situation->sponsor_character_id,
        .affected_character_id = situation->affected_character_id,
        .cause_event_id = situation->cause_event_id,
        .resolved_event_id = resolved_event_id,
        .resolved_day = sim->current_day,
        .progress_value = situation->objective.progress.value,
        .progress_limit = situation->objective.progress.limit,
        .danger_value = situation->objective.danger.value,
        .danger_limit = situation->objective.danger.limit
    };
}

static bool FrontHasActiveSituation(const CcSim *sim,
                                    const CcFront *front)
{
    if (sim == NULL || front == NULL) return false;
    for (int32_t i = 0; i < front->situation_count; ++i) {
        const CcSituation *situation = CcSimSituation(
            sim, front->situation_ids[i]);
        if (situation != NULL && situation->status == CC_SITUATION_ACTIVE) {
            return true;
        }
    }
    return false;
}

static void FinishFrontAfterSituation(CcSim *sim,
                                      const CcSituation *situation,
                                      CcId situation_event_id)
{
    CcFront *front = FrontMutable(sim, situation->front_id);
    if (front == NULL || front->status != CC_FRONT_ACTIVE) return;
    CcEventKind event_kind;
    const char *verb;
    if (situation->status == CC_SITUATION_RESOLVED) {
        front->status = CC_FRONT_RESOLVED;
        front->outcome = CcQuestOutcomeForSituationKind(situation->kind);
        event_kind = CC_EVENT_FRONT_RESOLVED;
        verb = "resolves";
    } else if (!FrontHasActiveSituation(sim, front)) {
        front->status = CC_FRONT_FAILED;
        front->outcome = CC_FRONT_OUTCOME_PRESSURE_WON;
        event_kind = CC_EVENT_FRONT_FAILED;
        verb = "closes without an answer";
    } else {
        return;
    }
    front->resolved_day = sim->current_day;
    front->portent.resolved_by_event_id = situation_event_id;
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(text, sizeof(text), "%s %s: %s.",
                   CcFrontKindName(front->kind), verb,
                   CcFrontOutcomeName(front->outcome));
    CcEvent *event = PushEvent(sim, event_kind, front->id, front->anchor_id,
                               situation_event_id,
                               (int32_t)front->outcome, text);
    front->resolved_event_id = event->id;
    front->portent.resolved_by_event_id = front->resolved_event_id;
}

static void AdvanceQuestDangerClocks(CcSim *sim)
{
    if (sim == NULL) return;
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        CcSituation *situation = &sim->situations[i];
        if (situation->status != CC_SITUATION_ACTIVE) continue;
        int32_t elapsed = sim->current_day - situation->created_day;
        situation->objective.danger.value = MinimumI32(
            situation->objective.danger.limit, MaximumI32(0, elapsed));
    }
    for (int32_t i = 0; i < sim->front_count; ++i) {
        CcFront *front = &sim->fronts[i];
        if (front->status != CC_FRONT_ACTIVE) continue;
        int32_t elapsed = sim->current_day - front->created_day;
        front->portent.value = MinimumI32(
            front->portent.limit, MaximumI32(0, elapsed));
    }
}

void CcSimUpgradeQuestArchitecture(CcSim *sim)
{
    if (sim == NULL) return;
    sim->front_count = 0;
    sim->quest_outcome_count = 0;
    sim->pending_echo_count = 0;
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        CcSituation *situation = &sim->situations[i];
        const CcEvent *created = LatestEvent(
            sim, CC_EVENT_SITUATION_CREATED, situation->id, 0U);
        InitializeSituationObjective(
            situation, created != NULL ? created->id :
                       situation->cause_event_id);
        AttachSituationToFront(sim, situation, false);
        if (situation->status == CC_SITUATION_RESOLVED) {
            situation->end_reason = CC_QUEST_END_COMPLETED;
            situation->objective.progress.value =
                situation->objective.progress.limit;
            const CcEvent *resolved = LatestEvent(
                sim, CC_EVENT_SITUATION_RESOLVED, situation->id, 0U);
            situation->objective.progress.resolved_by_event_id =
                resolved != NULL ? resolved->id : situation->cause_event_id;
            FinishFrontAfterSituation(
                sim, situation,
                situation->objective.progress.resolved_by_event_id);
            ArchiveSituationOutcome(
                sim, situation,
                situation->objective.progress.resolved_by_event_id);
        } else if (situation->status == CC_SITUATION_FAILED) {
            situation->end_reason = CC_QUEST_END_INVALIDATED;
            const CcEvent *failed = LatestEvent(
                sim, CC_EVENT_SITUATION_FAILED, situation->id, 0U);
            CcId failed_id = failed != NULL ? failed->id :
                             situation->cause_event_id;
            FinishFrontAfterSituation(sim, situation, failed_id);
            ArchiveSituationOutcome(sim, situation, failed_id);
        }
    }
    AdvanceQuestDangerClocks(sim);
}

static void ForgetRetiredSituation(CcSim *sim, CcId situation_id)
{
    if (sim == NULL || situation_id == 0U) return;
    /* Quest history lives in the outcome archive. Journey references belong
       to the situation slot and expire when that slot is reused. */
    if (sim->journey.situation_id == situation_id) {
        sim->journey.situation_id = 0U;
    }
    if (sim->resolved_journey_situation_id == situation_id) {
        sim->resolved_journey_situation_id = 0U;
        sim->resolved_journey_outcome = CC_JOURNEY_OUTCOME_NONE;
    }
    for (int32_t i = 0; i < sim->character_count; ++i) {
        CcCharacter *character = &sim->characters[i];
        CcCharacterMemory kept_memories[CC_CHARACTER_MEMORY_CAPACITY] = {0};
        int32_t memory_count = 0;
        for (int32_t slot = 0; slot < CC_CHARACTER_MEMORY_CAPACITY; ++slot) {
            if (character->memories[slot].kind != CC_CHARACTER_MEMORY_NONE &&
                character->memories[slot].subject_id != situation_id) {
                kept_memories[memory_count++] = character->memories[slot];
            }
        }
        memcpy(character->memories, kept_memories,
               sizeof(character->memories));
        character->memory_count = memory_count;
        character->memory_write_index =
            memory_count % CC_CHARACTER_MEMORY_CAPACITY;

        CcCharacterKnowledge kept_knowledge[
            CC_CHARACTER_KNOWLEDGE_CAPACITY] = {0};
        int32_t knowledge_count = 0;
        for (int32_t slot = 0;
             slot < CC_CHARACTER_KNOWLEDGE_CAPACITY; ++slot) {
            if (character->knowledge[slot].kind != CC_KNOWLEDGE_NONE &&
                character->knowledge[slot].subject_id != situation_id) {
                kept_knowledge[knowledge_count++] =
                    character->knowledge[slot];
            }
        }
        memcpy(character->knowledge, kept_knowledge,
               sizeof(character->knowledge));
        character->knowledge_count = knowledge_count;
        character->knowledge_write_index =
            knowledge_count % CC_CHARACTER_KNOWLEDGE_CAPACITY;
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
    ForgetRetiredSituation(sim, sim->situations[oldest].id);
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
    CcEvent *created = PushEvent(
        sim, CC_EVENT_SITUATION_CREATED, situation->id, target,
        cause, quantity, text);
    InitializeSituationObjective(situation, created->id);
    AttachSituationToFront(sim, situation, true);
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
        int32_t projected = NutritionRations(
            settlement->stock, CC_NUTRITION_CIVILIAN) +
            IncomingNutrition(sim, settlement->id,
                              CC_NUTRITION_CIVILIAN) /
                CC_NUTRITION_PER_RATION;
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
        if (shortage == NULL && !HasRecentSituation(
                sim, CC_SITUATION_RELIEF_DELIVERY, relief_target->id)) {
            char text[CC_EVENT_TEXT_CAPACITY];
            int32_t food = NutritionRations(
                relief_target->stock, CC_NUTRITION_CIVILIAN) +
                IncomingNutrition(sim, relief_target->id,
                                  CC_NUTRITION_CIVILIAN) /
                    CC_NUTRITION_PER_RATION;
            (void)snprintf(
                text, sizeof(text),
                "%s has %d food in store. Its reserve target is %d.",
                relief_target->name, food,
                relief_target->reserve_target[CC_GOOD_FOOD]);
            const CcEvent *harvest = LatestEvent(
                sim, CC_EVENT_HARVEST_FAILED, 0U, 0U);
            shortage = PushEvent(
                sim, CC_EVENT_SHORTAGE, relief_target->id,
                relief_target->id,
                harvest != NULL ? harvest->id :
                    LatestLocalCause(sim, relief_target->id),
                relief_need, text);
        }
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

static bool ScheduleDelayedEcho(CcSim *sim, const CcDelayedEcho *echo)
{
    if (sim == NULL || echo == NULL || !echo->active) return false;
    if (!sim->delayed_echo.active) {
        sim->delayed_echo = *echo;
        return true;
    }
    if (sim->pending_echo_count >= CC_MAX_PENDING_ECHOES) return false;
    sim->pending_echoes[sim->pending_echo_count++] = *echo;
    return true;
}

static void PromotePendingEcho(CcSim *sim)
{
    if (sim == NULL || sim->delayed_echo.active ||
        sim->pending_echo_count <= 0) return;
    sim->delayed_echo = sim->pending_echoes[0];
    for (int32_t i = 1; i < sim->pending_echo_count; ++i) {
        sim->pending_echoes[i - 1] = sim->pending_echoes[i];
    }
    sim->pending_echo_count -= 1;
    sim->pending_echoes[sim->pending_echo_count] = (CcDelayedEcho){0};
}

static void ResolveSituation(CcSim *sim, CcSituation *situation)
{
    if (situation == NULL || situation->status != CC_SITUATION_ACTIVE) return;
    if (situation->objective.progress.value <
        situation->objective.progress.limit) {
        const CcEvent *evidence = CcSimRecentEvent(sim, 0);
        CcId evidence_id = evidence != NULL ? evidence->id :
                           situation->cause_event_id;
        int32_t remaining = situation->objective.progress.limit -
                            situation->objective.progress.value;
        if (!RecordSituationEvidence(situation, evidence_id, remaining)) {
            return;
        }
    }
    if (situation->objective.progress.value <
        situation->objective.progress.limit) return;
    bool accepted = sim->player.accepted_situation_id == situation->id;
    CcFaction *issuer = FactionByIdMutable(
        sim, situation->issuer_faction_id);
    situation->status = CC_SITUATION_RESOLVED;
    situation->end_reason = CC_QUEST_END_COMPLETED;
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
    CcId resolution_id = resolution->id;
    CcCharacter *sponsor = CharacterMutable(
        sim, situation->sponsor_character_id);
    CcCharacter *affected = CharacterMutable(
        sim, situation->affected_character_id);
    if (accepted) {
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
    FinishFrontAfterSituation(sim, situation, resolution_id);
    ArchiveSituationOutcome(sim, situation, resolution_id);
    if (accepted) {
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
            CcDelayedEcho echo = (CcDelayedEcho){
                .active = true,
                .situation_id = situation->id,
                .settlement_id = echo_settlement,
                .parent_event_id = resolution->id,
                .outcome = sim->resolved_journey_outcome !=
                        CC_JOURNEY_OUTCOME_NONE ?
                    sim->resolved_journey_outcome :
                    CC_JOURNEY_OUTCOME_NEGOTIATED,
                .due_day = sim->current_day + 30
            };
            (void)snprintf(echo.character_name,
                           sizeof(echo.character_name), "%s",
                           witness);
            (void)ScheduleDelayedEcho(sim, &echo);
        }
    }
    SupersedeCompetingCrisisSituations(sim, situation);
}

static void ProgressDeliverySituations(CcSim *sim, CcId settlement_id,
                                       CcGood good, int32_t quantity,
                                       CcId evidence_event_id)
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
        if (!RecordSituationEvidence(
                situation, evidence_event_id, quantity)) continue;
        if (situation->objective.progress.value >=
            situation->objective.progress.limit) ResolveSituation(sim, situation);
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
        situation->end_reason = CC_QUEST_END_INVALIDATED;
        if (sim->player.accepted_situation_id == situation->id) {
            sim->player.accepted_situation_id = 0U;
        }
        char text[CC_EVENT_TEXT_CAPACITY];
        (void)snprintf(text, sizeof(text), "%s withdrawn after another power intervenes.",
                       CcSituationKindName(situation->kind));
        CcEvent *failure = PushEvent(
            sim, CC_EVENT_SITUATION_FAILED, situation->id,
            situation->target_id, situation->cause_event_id, 0, text);
        CcId failure_id = failure->id;
        FinishFrontAfterSituation(sim, situation, failure_id);
        ArchiveSituationOutcome(sim, situation, failure_id);
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
            other->front_id != chosen->front_id ||
            other->status != CC_SITUATION_ACTIVE) continue;
        other->status = CC_SITUATION_FAILED;
        other->end_reason = CC_QUEST_END_INVALIDATED;
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
        CcEvent *failure = PushEvent(
            sim, CC_EVENT_SITUATION_FAILED, other->id,
            other->target_id, other->cause_event_id, -3, text);
        CcId failure_id = failure->id;
        ArchiveSituationOutcome(sim, other, failure_id);
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
        situation->end_reason = CC_QUEST_END_EXPIRED;
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
        CcId failure_id = failure->id;
        situation->objective.danger.resolved_by_event_id = failure_id;
        if (accepted) {
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
        FinishFrontAfterSituation(sim, situation, failure_id);
        ArchiveSituationOutcome(sim, situation, failure_id);
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
            total += NutritionRations(
                sim->settlements[i].stock, CC_NUTRITION_CIVILIAN);
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
    if (sim->schema_version >= 36U) AssignHistoryOffices(sim, true);
    ceded->security = MaximumI32(10, ceded->security - 12);
    ceded->prosperity = MaximumI32(0, ceded->prosperity - 8);
    ceded->hunger = ClampI32(ceded->hunger + 8, 0, 100);
    sim->kingdoms[winner].legitimacy = ClampI32(
        sim->kingdoms[winner].legitimacy + 5, 0, 100);
    sim->kingdoms[loser].legitimacy = ClampI32(
        sim->kingdoms[loser].legitimacy - 12, 0, 100);
    int32_t captured = 0;
    for (int32_t i = 0; i < sim->treasure_count; ++i) {
        CcTreasure *t = &sim->treasures[i];
        if (!TreasureIsArchiveVolume(t) ||
            t->location_id != ceded->id) continue;
        captured += 1;
    }
    if (captured > 0) {
        char sack[CC_EVENT_TEXT_CAPACITY];
        (void)snprintf(sack, sizeof(sack),
                       "%d chronicles of %.16s are taken from the vaults "
                       "of %.16s into the keeping of %.16s.",
                       captured, sim->kingdoms[loser].name,
                       ceded->name, sim->kingdoms[winner].name);
        (void)PushEvent(sim, CC_EVENT_KINGDOM_ACTION,
                        sim->kingdoms[loser].id, ceded->id,
                        cause_event_id, captured, sack);
    }
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
    situation->end_reason = courier->status == CC_COURIER_LOST ?
        CC_QUEST_END_COURIER_LOST : CC_QUEST_END_INVALIDATED;
    const CcEvent *failure = CcSimRecentEvent(sim, 0);
    CcId failure_id = failure != NULL ? failure->id :
                      situation->cause_event_id;
    FinishFrontAfterSituation(sim, situation, failure_id);
    ArchiveSituationOutcome(sim, situation, failure_id);
    RefreshSituationCharacterActivities(sim, situation);
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
    ApplyCourierMessage(sim, courier, arrival->id);
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
    ExchangeGossip(sim, courier->id, courier->current_settlement_id, "Royal couriers");
    if (courier->current_settlement_id ==
        courier->destination_settlement_id) {
        DeliverCourier(sim, courier, false);
        return;
    }
    int32_t route_slot = -1;
    CcId next_hop = 0U;
    if (!FindTradePath(sim, courier->current_settlement_id,
                       courier->destination_settlement_id, CC_GOOD_FOOD,
                       &route_slot, &next_hop, NULL, NULL, NULL,
                       true, 0U, false, 1)) {
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
    courier->cause_event_id = departure->id;
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
        ExchangeGossip(sim, courier->id, next_hop, "Royal couriers");
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

static int32_t TakeAllianceGood(CcSim *sim, uint32_t mask, CcGood good,
                                int32_t quantity)
{
    int32_t taken_total = 0;
    for (int32_t i = 0; i < sim->settlement_count && quantity > 0; ++i) {
        CcSettlement *place = &sim->settlements[i];
        int32_t slot = KingdomSlotById(sim, place->kingdom_id);
        if (slot < 0 ||
            (mask & (UINT32_C(1) << (uint32_t)slot)) == 0U) continue;
        int32_t taken = MinimumI32(quantity, place->stock[good]);
        place->stock[good] -= taken;
        quantity -= taken;
        taken_total += taken;
    }
    return taken_total;
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

static bool AllianceHasSmithy(const CcSim *sim, uint32_t mask)
{
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        const CcSettlement *place = &sim->settlements[i];
        int32_t slot = KingdomSlotById(sim, place->kingdom_id);
        if (slot >= 0 &&
            (mask & (UINT32_C(1) << (uint32_t)slot)) != 0U &&
            !CcSettlementIsAbandoned(place) &&
            CcSettlementHasService(place, CC_SERVICE_SMITHY)) return true;
    }
    return false;
}

static void TakeAllianceNutrition(
    CcSim *sim, uint32_t mask, int32_t requested_nutrition,
    int32_t supplies[CC_GOOD_COUNT])
{
    for (int32_t i = 0;
         i < sim->settlement_count && requested_nutrition > 0; ++i) {
        CcSettlement *place = &sim->settlements[i];
        int32_t slot = KingdomSlotById(sim, place->kingdom_id);
        if (slot < 0 ||
            (mask & (UINT32_C(1) << (uint32_t)slot)) == 0U) continue;
        int32_t before[CC_GOOD_COUNT];
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
            before[good] = place->stock[good];
        }
        int32_t taken = CcNutritionConsume(
            place->stock, CC_NUTRITION_CIVILIAN, requested_nutrition);
        requested_nutrition -= taken;
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
            supplies[good] += before[good] - place->stock[good];
        }
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

static CcCharacter *BestCampaignHero(CcSim *sim, uint32_t mask,
                                     CcId patron_id)
{
    CcCharacter *best = NULL;
    int32_t best_score = INT32_MIN;
    for (int32_t i = 0; i < sim->character_count; ++i) {
        CcCharacter *candidate = &sim->characters[i];
        int32_t slot = CharacterKingdomSlot(sim, candidate);
        if (candidate->id == patron_id || slot < 0 ||
            (mask & (UINT32_C(1) << (uint32_t)slot)) == 0U) continue;
        int32_t score = candidate->courage +
            (candidate->role == CC_CHARACTER_SCOUT ? 25 : 0) +
            (candidate->role == CC_CHARACTER_OFFICIAL ? 8 : 0);
        if (best == NULL || score > best_score ||
            (score == best_score && candidate->id < best->id)) {
            best = candidate;
            best_score = score;
        }
    }
    return best;
}

static void NameDragonCampaignLeaders(CcSim *sim, uint32_t mask)
{
    CcDragonCampaign *campaign = &sim->dragon_campaign;
    CcId old_patron = campaign->patron_character_id;
    CcId old_hero = campaign->hero_character_id;
    int32_t patron_slot = -1;
    int32_t best_sanction = INT32_MIN;
    for (int32_t i = 0; i < sim->kingdom_count; ++i) {
        if ((mask & (UINT32_C(1) << (uint32_t)i)) == 0U) continue;
        if (patron_slot < 0 ||
            sim->kingdoms[i].sanction > best_sanction) {
            patron_slot = i;
            best_sanction = sim->kingdoms[i].sanction;
        }
    }
    campaign->patron_character_id = patron_slot >= 0 ?
        sim->kingdoms[patron_slot].monastery_patron_id : 0U;
    CcCharacter *hero = BestCampaignHero(
        sim, mask, campaign->patron_character_id);
    campaign->hero_character_id = hero != NULL ? hero->id : 0U;
    if (campaign->patron_character_id == old_patron &&
        campaign->hero_character_id == old_hero) return;
    const CcCharacter *patron = CcSimCharacter(
        sim, campaign->patron_character_id);
    if (patron == NULL || hero == NULL) return;
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(
        text, sizeof(text),
        "%.24s funds the dragon host and names %.24s as its champion.",
        patron->name, hero->name);
    CcEvent *event = PushEvent(
        sim, CC_EVENT_DRAGON_PATRON_NAMED, patron->id,
        patron->home_settlement_id, campaign->cause_event_id,
        hero->courage, text);
    campaign->cause_event_id = event->id;
}

static void StockDragonCampaign(CcSim *sim, uint32_t mask)
{
    CcDragonCampaign *campaign = &sim->dragon_campaign;
    int32_t nutrition_needed = MaximumI32(
        0, 32 * CC_NUTRITION_PER_RATION -
           CcNutritionAvailable(
               campaign->supplies, CC_NUTRITION_CIVILIAN));
    TakeAllianceNutrition(sim, mask, nutrition_needed,
                          campaign->supplies);

    int32_t tool_need = MaximumI32(
        0, 8 - campaign->supplies[CC_GOOD_TOOLS]);
    int32_t tools_taken = TakeAllianceGood(
        sim, mask, CC_GOOD_TOOLS, tool_need);
    campaign->supplies[CC_GOOD_TOOLS] += tools_taken;
    tool_need -= tools_taken;
    if (tool_need > 0 && AllianceHasSmithy(sim, mask)) {
        int32_t commissioned = MinimumI32(
            MinimumI32(2, tool_need),
            MinimumI32(
                AllianceGoodAvailable(sim, mask, CC_GOOD_IRON) / 2,
                AllianceGoodAvailable(sim, mask, CC_GOOD_WOOD)));
        if (commissioned > 0) {
            (void)TakeAllianceGood(
                sim, mask, CC_GOOD_IRON, commissioned * 2);
            (void)TakeAllianceGood(
                sim, mask, CC_GOOD_WOOD, commissioned);
            campaign->supplies[CC_GOOD_TOOLS] += commissioned;
            tool_need -= commissioned;
        }
    }
    if (tool_need > 0 && campaign->patron_character_id != 0U &&
        sim->archives.stewardship_rank >= 60) {
        int32_t commissioned = MinimumI32(
            MinimumI32(2, tool_need),
            MinimumI32(
                AllianceGoodAvailable(sim, mask, CC_GOOD_WOOD) / 2,
                AllianceGoodAvailable(sim, mask, CC_GOOD_STONE) / 2));
        if (commissioned > 0) {
            (void)TakeAllianceGood(
                sim, mask, CC_GOOD_WOOD, commissioned * 2);
            (void)TakeAllianceGood(
                sim, mask, CC_GOOD_STONE, commissioned * 2);
            campaign->supplies[CC_GOOD_TOOLS] += commissioned;
            tool_need -= commissioned;
        }
    }
    if (tool_need > 0 && campaign->patron_character_id != 0U &&
        sim->archives.stewardship_rank >= 60) {
        int32_t commissioned = MinimumI32(
            MinimumI32(2, tool_need),
            AllianceGoodAvailable(sim, mask, CC_GOOD_WOOD) / 4);
        if (commissioned > 0) {
            (void)TakeAllianceGood(
                sim, mask, CC_GOOD_WOOD, commissioned * 4);
            campaign->supplies[CC_GOOD_TOOLS] += commissioned;
        }
    }

    int32_t weapon_need = MaximumI32(
        0, 12 - campaign->supplies[CC_GOOD_WEAPONS]);
    int32_t weapons_taken = TakeAllianceGood(
        sim, mask, CC_GOOD_WEAPONS, weapon_need);
    campaign->supplies[CC_GOOD_WEAPONS] += weapons_taken;
    weapon_need -= weapons_taken;
    if (weapon_need > 0 && AllianceHasSmithy(sim, mask)) {
        int32_t commissioned = MinimumI32(
            MinimumI32(2, weapon_need),
            MinimumI32(
                AllianceGoodAvailable(sim, mask, CC_GOOD_IRON) / 3,
                AllianceGoodAvailable(sim, mask, CC_GOOD_WOOD) / 2));
        if (commissioned > 0) {
            (void)TakeAllianceGood(
                sim, mask, CC_GOOD_IRON, commissioned * 3);
            (void)TakeAllianceGood(
                sim, mask, CC_GOOD_WOOD, commissioned * 2);
            campaign->supplies[CC_GOOD_WEAPONS] += commissioned;
            weapon_need -= commissioned;
        }
    }
    if (weapon_need > 0 && campaign->patron_character_id != 0U &&
        sim->archives.stewardship_rank >= 60) {
        int32_t commissioned = MinimumI32(
            MinimumI32(2, weapon_need),
            MinimumI32(
                AllianceGoodAvailable(sim, mask, CC_GOOD_WOOD) / 3,
                AllianceGoodAvailable(sim, mask, CC_GOOD_STONE)));
        if (commissioned > 0) {
            (void)TakeAllianceGood(
                sim, mask, CC_GOOD_WOOD, commissioned * 3);
            (void)TakeAllianceGood(
                sim, mask, CC_GOOD_STONE, commissioned);
            campaign->supplies[CC_GOOD_WEAPONS] += commissioned;
            weapon_need -= commissioned;
        }
    }
    if (weapon_need > 0 && campaign->patron_character_id != 0U &&
        sim->archives.stewardship_rank >= 60) {
        int32_t commissioned = MinimumI32(
            MinimumI32(2, weapon_need),
            AllianceGoodAvailable(sim, mask, CC_GOOD_WOOD) / 5);
        if (commissioned > 0) {
            (void)TakeAllianceGood(
                sim, mask, CC_GOOD_WOOD, commissioned * 5);
            campaign->supplies[CC_GOOD_WEAPONS] += commissioned;
        }
    }
}

static void TryLaunchDragonCampaign(CcSim *sim)
{
    CcDragonCampaign *campaign = &sim->dragon_campaign;
    uint32_t mask = campaign->pledged_kingdom_mask;
    if (campaign->phase != CC_DRAGON_CAMPAIGN_IDLE ||
        campaign->cooldown_days > 0 || sim->dragon.slain ||
        MaskCount(mask) < 2) return;
    if (sim->dragon.age_days < 500 * 365 &&
        sim->dragon.life_stage != CC_DRAGON_STAGE_DEEP_WYRM) return;
    NameDragonCampaignLeaders(sim, mask);
    StockDragonCampaign(sim, mask);
    if (NutritionRations(
            campaign->supplies, CC_NUTRITION_CIVILIAN) < 32 ||
        campaign->supplies[CC_GOOD_TOOLS] < 8 ||
        campaign->supplies[CC_GOOD_WEAPONS] < 12 ||
        campaign->patron_character_id == 0U ||
        campaign->hero_character_id == 0U) return;
    int32_t leader_slot = -1;
    CcSettlement *origin = NULL;
    for (int32_t i = 0; i < sim->kingdom_count; ++i) {
        if ((mask & (UINT32_C(1) << (uint32_t)i)) == 0U) continue;
        CcSettlement *candidate = KingdomSeat(sim, i);
        if (candidate != NULL) {
            leader_slot = i;
            origin = candidate;
            break;
        }
    }
    if (leader_slot < 0 || origin == NULL) return;
    campaign->phase = CC_DRAGON_CAMPAIGN_OUTBOUND;
    campaign->alliance_kingdom_mask = mask;
    campaign->origin_settlement_id = origin->id;
    campaign->days_remaining = DragonCampaignTravelDays(sim, origin->id);
    campaign->attempts += 1;
    const CcCharacter *patron = CcSimCharacter(
        sim, campaign->patron_character_id);
    const CcCharacter *hero = CcSimCharacter(
        sim, campaign->hero_character_id);
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(
        text, sizeof(text),
        "%.20s sends the host from %.20s under %.20s: 32 food, 8 tools, 12 weapons.",
        patron != NULL ? patron->name : "The patron",
        origin->name, hero != NULL ? hero->name : "its champion");
    CcEvent *event = PushEvent(
        sim, CC_EVENT_DRAGON_MUSTERED, sim->kingdoms[leader_slot].id,
        origin->id, campaign->cause_event_id, MaskCount(mask), text);
    campaign->cause_event_id = event->id;
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
    const CcCharacter *hero = CcSimCharacter(
        sim, campaign->hero_character_id);
    const CcCharacter *patron = CcSimCharacter(
        sim, campaign->patron_character_id);
    int32_t patron_slot = patron != NULL ?
        CharacterKingdomSlot(sim, patron) : -1;
    int32_t named_leadership = (hero != NULL ? hero->courage / 2 : 0) +
        (patron_slot >= 0 ?
             sim->kingdoms[patron_slot].sanction / 4 : 0);
    int32_t attack = campaign->supplies[CC_GOOD_WEAPONS] * 4 +
                     campaign->supplies[CC_GOOD_TOOLS] * 2 +
                     NutritionRations(
                         campaign->supplies,
                         CC_NUTRITION_CIVILIAN) / 4 +
                     allies * 12 + campaign_experience + named_leadership +
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
            "%.24s's host breaks: %d against %d; survivors bring knowledge home.",
            hero != NULL ? hero->name : "The champion", attack, defense);
        CcEvent *battle = PushEvent(
            sim, CC_EVENT_DRAGON_BATTLE, sim->dragon.id,
            campaign->origin_settlement_id, campaign->cause_event_id,
            attack - defense, text);
        campaign->cause_event_id = battle->id;
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
    /* Relic law: capture the victim's age and stage before the stage flip,
       so the bane can be priced by the dragon it killed. */
    int32_t slain_age_years = sim->dragon.age_days / 365;
    CcDragonLifeStage slain_stage = sim->dragon.life_stage;
    sim->dragon.slain = true;
    sim->dragon.slain_day = sim->current_day;
    sim->dragon.life_stage = CC_DRAGON_STAGE_AFTERDRAGON;
    sim->dragon.activity = CC_DRAGON_ACTIVITY_AFTERMATH;
    sim->dragon.body_condition = 0;
    sim->dragon.crown_strength = 0;
    sim->dragon.afterdeath_days = 0;
    sim->dragon.territoryless_days = 0;
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
        "%.20s slays %.20s for %.20s: %d against %d; the hoard goes home.",
        hero != NULL ? hero->name : "The champion", sim->dragon.name,
        patron != NULL ? patron->name : "the allied realms",
        attack, defense);
    CcEvent *battle = PushEvent(
        sim, CC_EVENT_DRAGON_SLAIN, sim->dragon.id,
        sim->dragon.lair_settlement_id, campaign->cause_event_id,
        attack - defense, text);
    CcId battle_event_id = battle->id;
    {
        CcSettlement *origin = CcSimSettlementMutable(
            sim, campaign->origin_settlement_id);
        CcTreasure *relic = origin != NULL ? AllocateTreasure(sim) : NULL;
        if (relic != NULL) {
            const char *epithet = sim->schema_version < 42U ? "" :
                slain_stage == CC_DRAGON_STAGE_DEEP_WYRM ? ", Wyrmsbane" :
                slain_stage == CC_DRAGON_STAGE_CROWNED ?
                    ", the Crown's End" : "";
            int name_limit = (int)sizeof(relic->name) - 9 - (int)strlen(epithet);
            if (name_limit > 24) name_limit = 24;
            (void)snprintf(relic->name, sizeof(relic->name),
                           "Bane of %.*s%s", name_limit, sim->dragon.name, epithet);
            relic->maker_settlement_id = origin->id;
            relic->owner_id = hero != NULL ? hero->id : origin->id;
            relic->location_id = origin->id;
            relic->gold_content = 3;
            relic->gem_content = 2;
            relic->craft_work = MaximumI32(1, attack - defense);
            /* The older the victim, the greater the trophy: one age step
               per decade of the slain dragon's life, capped at a
               millennium. A whelp's bane is trinket; a wyrm's bane
               outranks the Wyrmheart it was taken from. */
            relic->appraised_value = relic->gold_content * 40 +
                relic->gem_content * 70 + relic->craft_work * 10 +
                (sim->schema_version >= 42U ?
                    MinimumI32(slain_age_years / 10, 100) * 15 : 0);
            relic->created_day = sim->current_day;
            char relic_text[CC_EVENT_TEXT_CAPACITY];
            if (sim->schema_version >= 42U) {
                (void)snprintf(relic_text, sizeof(relic_text),
                    "From the hoard-fire %.20s raises %.20s, bane of %.20s, slain at %d years, day %d.",
                    hero != NULL ? hero->name : "the host",
                    relic->name, sim->dragon.name, slain_age_years, sim->current_day);
            } else {
                (void)snprintf(relic_text, sizeof(relic_text),
                    "From the hoard-fire %.20s raises %.20s, bane of %.20s, day %d.",
                    hero != NULL ? hero->name : "the host",
                    relic->name, sim->dragon.name, sim->current_day);
            }
            (void)PushEvent(sim, CC_EVENT_TREASURE_CRAFTED,
                            relic->id, origin->id, battle_event_id,
                            relic->appraised_value, relic_text);
        }
    }
    campaign->cause_event_id = battle_event_id;
    sim->dragon.lifecycle_event_id = battle_event_id;
    campaign->phase = CC_DRAGON_CAMPAIGN_RETURNING;
    campaign->days_remaining = DragonCampaignTravelDays(
        sim, campaign->origin_settlement_id);
}

static void UpdateRoyalDiplomacy(CcSim *sim)
{
    if (sim == NULL || sim->current_day % 28 != 0) return;
    CcMoney tracked_gold = CcSimTrackedGold(sim);
    bool campaign_age = sim->dragon.age_days >= 500 * 365 ||
        sim->dragon.life_stage == CC_DRAGON_STAGE_DEEP_WYRM;
    bool dragon_crisis = !sim->dragon.slain && campaign_age &&
        ((tracked_gold > 0 && sim->dragon.hoard * 3 >= tracked_gold) ||
         sim->dragon.regional_influence >= 60 ||
         (sim->dragon.age_days >= 500 * 365 &&
          sim->dragon.territory_stability < 50));
    CcDragonCampaign *campaign = &sim->dragon_campaign;
    bool abbot_calls_host = dragon_crisis &&
        campaign->phase == CC_DRAGON_CAMPAIGN_IDLE &&
        campaign->cooldown_days == 0 &&
        (sim->dragon.life_stage == CC_DRAGON_STAGE_DEEP_WYRM ||
         (sim->dragon.age_days >= 500 * 365 &&
          sim->current_day % (5 * 364) == 0));
    if (abbot_calls_host &&
        MaskCount(campaign->pledged_kingdom_mask) < 2) {
        uint32_t mask = 0U;
        for (int32_t i = 0; i < sim->kingdom_count; ++i) {
            if (sim->kingdoms[i].sanction >= 40) {
                mask |= UINT32_C(1) << (uint32_t)i;
            }
        }
        if (MaskCount(mask) < 2) {
            mask = (UINT32_C(1) << (uint32_t)sim->kingdom_count) - 1U;
        }
        campaign->pledged_kingdom_mask = mask;
        NameDragonCampaignLeaders(sim, mask);
    }
    if (dragon_crisis && MaskCount(campaign->pledged_kingdom_mask) >= 2) {
        TryLaunchDragonCampaign(sim);
        if (campaign->phase != CC_DRAGON_CAMPAIGN_IDLE) return;
    }
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
                int32_t issuer_legitimacy = issuer == first ?
                    sim->kingdoms[first].legitimacy :
                    sim->kingdoms[second].legitimacy;
                if (pressure >= 110 && issuer_hunger >= 25 &&
                    issuer_hunger <= 55 &&
                    issuer_legitimacy >= 30 &&
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
    const CcRoyalCarriage *carriage = CcSimRoyalCarriage(
        sim, kingdom_id);
    if (carriage != NULL &&
        (carriage->mode == CC_ROYAL_CARRIAGE_DELIVERING ||
         (carriage->mode == CC_ROYAL_CARRIAGE_IDLE &&
          carriage->trips_completed + carriage->cargo_losses > 0 &&
          carriage->departure_day > 0 &&
          sim->current_day - carriage->departure_day <= 7))) {
        count += 1;
    }
    for (int32_t i = 0; i < sim->shipment_count; ++i) {
        const CcShipment *shipment = &sim->shipments[i];
        if (RoyalCarriageForShipmentConst(sim, shipment->id) != NULL) {
            continue;
        }
        const CcSettlement *origin = CcSimSettlement(sim, shipment->origin_id);
        if ((shipment->status == CC_SHIPMENT_TRAVELLING ||
             shipment->status == CC_SHIPMENT_BLOCKED) && origin != NULL &&
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
         to->stock[CC_GOOD_TOOLS] +
             to->stock[CC_GOOD_WOOD] +
             to->stock[CC_GOOD_STONE] +
             NutritionRations(to->stock, CC_NUTRITION_CIVILIAN) >
         best->stock[CC_GOOD_TOOLS] +
             best->stock[CC_GOOD_WOOD] +
             best->stock[CC_GOOD_STONE] +
             NutritionRations(best->stock, CC_NUTRITION_CIVILIAN))) {
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
            NutritionRations(from->stock, CC_NUTRITION_CIVILIAN) -
                from->reserve_target[CC_GOOD_BREAD]);
    }
    if (to != NULL) {
        food_surplus = MaximumI32(
            food_surplus,
            NutritionRations(to->stock, CC_NUTRITION_CIVILIAN) -
                to->reserve_target[CC_GOOD_BREAD]);
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
        NutritionRations(from->stock, CC_NUTRITION_CIVILIAN) +
            from->stock[CC_GOOD_TOOLS] + from->stock[CC_GOOD_WOOD] +
            from->stock[CC_GOOD_STONE] >=
        NutritionRations(to->stock, CC_NUTRITION_CIVILIAN) +
            to->stock[CC_GOOD_TOOLS] + to->stock[CC_GOOD_WOOD] +
            to->stock[CC_GOOD_STONE] ? from : to;
    if (labor_base->population < 220 ||
        NutritionRations(supplier->stock, CC_NUTRITION_CIVILIAN) < 4 ||
        supplier->stock[CC_GOOD_WOOD] < 2 ||
        supplier->stock[CC_GOOD_STONE] < 2 ||
        supplier->stock[CC_GOOD_TOOLS] < 1) return;
    (void)CcNutritionConsume(
        supplier->stock, CC_NUTRITION_CIVILIAN,
        4 * CC_NUTRITION_PER_RATION);
    supplier->stock[CC_GOOD_WOOD] -= 2;
    supplier->stock[CC_GOOD_STONE] -= 2;
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
            CcSettlement *route_from = CcSimSettlementMutable(
                sim, route->from_id);
            CcSettlement *route_to = CcSimSettlementMutable(
                sim, route->to_id);
            CcSettlement *wood_source = route_from;
            CcSettlement *stone_source = route_from;
            if (wood_source == NULL ||
                (route_to != NULL &&
                 route_to->stock[CC_GOOD_WOOD] >
                     wood_source->stock[CC_GOOD_WOOD])) {
                wood_source = route_to;
            }
            if (stone_source == NULL ||
                (route_to != NULL &&
                 route_to->stock[CC_GOOD_STONE] >
                     stone_source->stock[CC_GOOD_STONE])) {
                stone_source = route_to;
            }
            bool locally_maintained = !route->smuggler_route &&
                route_from != NULL && route_to != NULL &&
                !CcSettlementIsAbandoned(route_from) &&
                !CcSettlementIsAbandoned(route_to) &&
                MaximumI32(route_from->hunger, route_to->hunger) < 30 &&
                (route_from->prosperity + route_to->prosperity) / 2 >= 45 &&
                wood_source != NULL &&
                wood_source->stock[CC_GOOD_WOOD] > 0 &&
                stone_source != NULL &&
                stone_source->stock[CC_GOOD_STONE] > 0;
            if (locally_maintained) {
                wood_source->stock[CC_GOOD_WOOD] -= 1;
                stone_source->stock[CC_GOOD_STONE] -= 1;
            }
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
                int32_t burned = 0;
                int32_t held = 0;
                int32_t lore_burned = 0;
                for (int32_t i = 0; i < sim->treasure_count; ++i) {
                    CcTreasure *t = &sim->treasures[i];
                    if (!TreasureIsArchiveVolume(t)) continue;
                    CcSettlement *vault = CcSimSettlementMutable(
                        sim, t->owner_id);
                    if (vault == NULL ||
                        vault->kingdom_id != kingdom->id) continue;
                    held += 1;
                }
                int32_t burn_target = (held + 1) / 2;
                for (int32_t i = 0;
                     i < sim->treasure_count && burned < burn_target; ++i) {
                    CcTreasure *t = &sim->treasures[i];
                    if (!TreasureIsArchiveVolume(t)) continue;
                    CcSettlement *vault = CcSimSettlementMutable(
                        sim, t->owner_id);
                    if (vault == NULL ||
                        vault->kingdom_id != kingdom->id) continue;
                    lore_burned += MaximumI32(1, t->craft_work);
                    t->destroyed = true;
                    burned += 1;
                }
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
                CcEvent *repudiation = PushEvent(
                    sim, CC_EVENT_KINGDOM_ACTION, kingdom->id,
                    worst != NULL ? worst->id : kingdom->id,
                    LatestLocalCause(sim, kingdom->id),
                    repudiated > INT32_MAX ? INT32_MAX :
                        (int32_t)repudiated, text);
                if (lore_burned > 0) {
                    int32_t newly_lost = MinimumI32(
                        lore_burned, sim->archives.lore_stored);
                    sim->archives.lore_stored -= newly_lost;
                    sim->archives.lore_lost_total += newly_lost;
                    char lore_text[CC_EVENT_TEXT_CAPACITY];
                    (void)snprintf(
                        lore_text, sizeof(lore_text),
                        "%s burns %d archive volume%s; %d pieces of lore are lost.",
                        kingdom->name, burned, burned == 1 ? "" : "s",
                        newly_lost);
                    (void)PushEvent(
                        sim, CC_EVENT_LORE_LOST, kingdom->id,
                        worst != NULL ? worst->id : kingdom->id,
                        repudiation->id,
                        newly_lost, lore_text);
                }
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
            int32_t projected = NutritionRations(
                place->stock, CC_NUTRITION_CIVILIAN) +
                IncomingNutrition(
                    sim, place->id, CC_NUTRITION_CIVILIAN) /
                    CC_NUTRITION_PER_RATION;
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
            int32_t wool_need = WarWeeklyNeed(
                sim, front, CC_GOOD_WOOL);
            CcMoney desired_chest = wage * 4 +
                (CcMoney)food_need * front->price[CC_GOOD_FOOD] * 2 +
                (CcMoney)tool_need * front->price[CC_GOOD_TOOLS] * 2 +
                (CcMoney)weapon_need * front->price[CC_GOOD_WEAPONS] * 2 +
                (CcMoney)wool_need * front->price[CC_GOOD_WOOL] * 2;
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
                                repair_base->stock[CC_GOOD_WOOD] >= 2 &&
                                repair_base->stock[CC_GOOD_STONE] >= 2 &&
                                kingdom->treasury >= 24;
            bool locally_funded = best_route != NULL && !crown_funded &&
                                  repair_base->stock[CC_GOOD_WOOD] >= 2 &&
                                  repair_base->stock[CC_GOOD_STONE] >= 2 &&
                                  repair_base->stock[CC_GOOD_TOOLS] >= 1 &&
                                  NutritionRations(
                                      repair_base->stock,
                                      CC_NUTRITION_CIVILIAN) >= 4;
            if (crown_funded || locally_funded) {
                repair_base->stock[CC_GOOD_WOOD] -= 2;
                repair_base->stock[CC_GOOD_STONE] -= 2;
                if (crown_funded) {
                    kingdom->treasury -= 24;
                    repair_base->market_coins += 24;
                } else {
                    repair_base->stock[CC_GOOD_TOOLS] -= 1;
                    (void)CcNutritionConsume(
                        repair_base->stock, CC_NUTRITION_CIVILIAN,
                        4 * CC_NUTRITION_PER_RATION);
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
                               crown_funded ?
                                   "paid crews with local Wood and Stone" :
                                   "local Food, Wood, Stone, and Tools",
                               best_route->condition);
                (void)PushEvent(
                    sim, CC_EVENT_KINGDOM_ACTION, kingdom->id, best_route->id,
                    LatestLocalCause(sim, repair_base->id),
                    best_route->condition, text);
            }
        }

        if (sim->current_day % 28 == 0 && kingdom->treasury >= 12) {
            CcRoute *upkeep_route = NULL;
            CcSettlement *upkeep_base = NULL;
            for (int32_t route_index = 0; route_index < sim->route_count; ++route_index) {
                CcRoute *route = &sim->routes[route_index];
                if (route->closed || route->condition >= 58) continue;
                CcSettlement *base = CcSimSettlementMutable(sim, route->to_id);
                bool supplied = base != NULL && !CcSettlementIsAbandoned(base) &&
                    base->kingdom_id == kingdom->id &&
                    base->stock[CC_GOOD_WOOD] >= 1 && base->stock[CC_GOOD_STONE] >= 1;
                if (!supplied && sim->schema_version >= 36U) {
                    base = CcSimSettlementMutable(sim, route->from_id);
                    supplied = base != NULL && !CcSettlementIsAbandoned(base) &&
                        base->kingdom_id == kingdom->id &&
                        base->stock[CC_GOOD_WOOD] >= 1 && base->stock[CC_GOOD_STONE] >= 1;
                }
                if (!supplied) continue;
                if (upkeep_route == NULL || route->condition < upkeep_route->condition) {
                    upkeep_route = route;
                    upkeep_base = base;
                }
                if (sim->schema_version < 36U) break;
            }
            if (upkeep_route != NULL) {
                kingdom->treasury -= 12;
                upkeep_base->market_coins += 12;
                upkeep_base->stock[CC_GOOD_WOOD] -= 1;
                upkeep_base->stock[CC_GOOD_STONE] -= 1;
                upkeep_route->condition = ClampI32(upkeep_route->condition + 16, 0, 100);
                upkeep_route->security = ClampI32(upkeep_route->security + 2, 0, 100);
                if (upkeep_route->condition < 45) {
                    char text[CC_EVENT_TEXT_CAPACITY];
                    (void)snprintf(text, sizeof(text),
                                   "%s maintains a strategic road before it fails.",
                                   kingdom->name);
                    (void)PushEvent(sim, CC_EVENT_KINGDOM_ACTION, kingdom->id, upkeep_route->id,
                                    0U, upkeep_route->condition, text);
                }
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

static bool HorseFeedAvailable(const CcSim *sim,
                               const CcSettlement *settlement)
{
    if (sim->schema_version < 29U) {
        return settlement->stock[CC_GOOD_BREAD] > 0;
    }
    return CcNutritionAvailable(
        settlement->stock, CC_NUTRITION_ANIMAL) >= CC_NUTRITION_PER_RATION;
}

static void ConsumeHorseFeed(const CcSim *sim, CcSettlement *settlement)
{
    if (sim->schema_version < 29U) {
        settlement->stock[CC_GOOD_BREAD] -= 1;
        return;
    }
    (void)CcNutritionConsume(
        settlement->stock, CC_NUTRITION_ANIMAL,
        CC_NUTRITION_PER_RATION);
}

static void AdvanceLegacyHorseTeam(CcSim *sim)
{
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
        HorseFeedAvailable(sim, place);
    if (feed_available) {
        CcSettlement *mutable_place = CcSimSettlementMutable(
            sim, place->id);
        ConsumeHorseFeed(sim, mutable_place);
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
                             pace_strain - horse->hardiness / 25;
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
                   HorseFeedAvailable(sim, stable);
        if (fed) ConsumeHorseFeed(sim, stable);
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

static void AdvanceHorseTeam(CcSim *sim)
{
    if (sim->schema_version < 14U) return;
    if (sim->schema_version < 24U) {
        AdvanceLegacyHorseTeam(sim);
        return;
    }
    bool on_journey = sim->journey.active;
    const CcSettlement *place = !on_journey ?
        CcSimSettlement(sim, sim->player.location_id) : NULL;
    bool stable_care = CcSettlementHasService(place, CC_SERVICE_STABLE);
    bool weekly_feed = !on_journey && sim->current_day % 7 == 0;
    bool feed_available = weekly_feed && stable_care && place != NULL &&
        HorseFeedAvailable(sim, place);
    if (feed_available) {
        CcSettlement *mutable_place = CcSimSettlementMutable(
            sim, place->id);
        ConsumeHorseFeed(sim, mutable_place);
    }

    int32_t boarded_at_start = sim->stable_horse_count;
    for (int32_t i = 0; i < CC_CARRIAGE_HORSE_COUNT; ++i) {
        CcHorse *horse = &sim->horse_team[i];
        AdvanceHorseLifecycle(sim, horse);
        if (!on_journey) {
            horse->fatigue = ClampI32(
                horse->fatigue - (stable_care ? 10 : 4), 0, 100);
            if (weekly_feed) {
                horse->hunger = ClampI32(
                    horse->hunger + (feed_available ? -24 : 12), 0, 100);
            }
        }
        if (horse->fatigue >= 88 || horse->hunger >= 80) {
            horse->health = ClampI32(horse->health - 2, 1, 100);
        } else if (!on_journey && stable_care && horse->hunger < 45) {
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
                   HorseFeedAvailable(sim, stable);
        if (fed) {
            ConsumeHorseFeed(sim, stable);
        }
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
        if (sim->schema_version >= 26U) AdvanceCharacterLifecycles(sim);
        HearLocalGossip(sim);
        CcSimRefreshCharacterGossip(sim);
        if (!sim->journey.active) {
            ExchangeGossip(sim, sim->player.id, sim->player.location_id,
                            "Your fellow travelers");
        }
        AdvanceHorseTeam(sim);
        AdvanceQuestDangerClocks(sim);
        if (sim->current_day > next_situation_expiry) {
            ExpireSituations(sim);
            next_situation_expiry = NextSituationExpiryDay(sim);
        }
        AdvanceRoyalCarriages(sim);
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
            const CcSettlement *scriptorium = Scriptorium(sim);
            CcId scriptorium_id = scriptorium != NULL ?
                scriptorium->id : 0U;
            for (int32_t settlement = 0; settlement < sim->settlement_count; ++settlement) {
                UpdateSettlement(sim, settlement, scriptorium_id);
            }
            AdvanceRuins(sim);
            if (sim->schema_version >= 22U) AdvanceArchives(sim);
            UpdateThreats(sim);
            UpdateRoutesAndGovernments(sim);
            UpdateRoyalDiplomacy(sim);
            PlanTrade(sim);
            GenerateSituations(sim);
            PlanGoblinTribute(sim);
            PlanHoardRaid(sim);
            next_situation_expiry = NextSituationExpiryDay(sim);
        }
        SettleChangedRoyalDestinations(sim);
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
        int32_t old_slots = PlayerCargoBoxes(
            command->good, sim->player.cargo[command->good]);
        int32_t new_slots = PlayerCargoBoxes(
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
        int32_t nutrition = CcGoodNutritionValue(
            command->good, CC_NUTRITION_CIVILIAN);
        if (nutrition > 0 && settlement->hunger > 0) {
            settlement->hunger = ClampI32(
                settlement->hunger - selling * nutrition, 0, 100);
            sim->player.reputation = ClampI32(
                sim->player.reputation +
                    selling * nutrition / CC_NUTRITION_PER_RATION,
                -100, 100);
        }
    }
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(text, sizeof(text), "The Crownless company %s %d %s at %s.",
                   amount > 0 ? "loads" : "delivers", amount > 0 ? amount : -amount,
                   CcGoodName(command->good), settlement->name);
    CcEvent *trade = PushEvent(
        sim, CcGoodNutritionValue(
                 command->good, CC_NUTRITION_CIVILIAN) > 0 && amount < 0 ?
            CC_EVENT_RELIEF : CC_EVENT_PLAYER_TRADE,
        sim->player.id, settlement->id, 0,
        amount > 0 ? amount : -amount, text);
    if (amount < 0) {
        ProgressDeliverySituations(
            sim, settlement->id, command->good, -amount,
            trade->id);
    }
    RefreshSettlementGoodPrice(sim, settlement, command->good);
    SetError(error, error_capacity, "");
    return true;
}

static bool ApplyGoblinTrade(CcSim *sim, const CcCommand *command,
                             char *error, size_t error_capacity)
{
    bool useful_good = CcGoodNutritionValue(
            command->good, CC_NUTRITION_CIVILIAN) > 0 ||
        command->good == CC_GOOD_TOOLS ||
        command->good == CC_GOOD_WEAPONS;
    if (!useful_good || command->amount <= 0) {
        SetError(error, error_capacity,
                 "The Cinder Tithe trades only for provisions, Tools, or Weapons.");
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
    goblins->tribute_event_id = event->id;
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
    LearnMapKnowledge(sim, atlas);
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
    LearnMapKnowledge(sim, map);
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
    sim->dragon.hoard_event_id = theft->id;
    (void)snprintf(
        text, sizeof(text),
        "Smoke falls over %.16s; old readers count 14 nights until %.16s comes for %.20s.",
        target->name, sim->dragon.name, treasure->name);
    CcEvent *omen = PushEvent(
        sim, CC_EVENT_DRAGON_OMEN, sim->dragon.id, target->id,
        theft->id, 14, text);
    sim->dragon.omen_event_id = omen->id;
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
    sim->dragon.hoard_event_id = returned->id;
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
    SyncSituationObjectiveDefinition((CcSituation *)situation);
    if (!CcSimSituationCanAccept(sim, situation)) {
        SetError(error, error_capacity,
                 "You do not have a job to accept yet. Ask what happened first.");
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
    CcSettlement *relief_origin = NULL;
    int32_t relief_load = 0;
    if (situation->kind == CC_SITUATION_RELIEF_DELIVERY) {
        if (sim->player.location_id != offer_settlement) {
            SetError(error, error_capacity,
                     "Return to Mara so she can load the food boxes.");
            return false;
        }
        relief_load = MaximumI32(
            0, situation->quantity - situation->progress);
        relief_origin = CcSimSettlementMutable(sim, offer_settlement);
        if (relief_origin == NULL ||
            relief_origin->stock[CC_GOOD_FOOD] < relief_load) {
            SetError(error, error_capacity,
                     "Mara's granary cannot cover the promised food load.");
            return false;
        }
        int32_t old_slots = PlayerCargoBoxes(
            CC_GOOD_FOOD, sim->player.cargo[CC_GOOD_FOOD]);
        int32_t new_slots = PlayerCargoBoxes(
            CC_GOOD_FOOD,
            sim->player.cargo[CC_GOOD_FOOD] + relief_load);
        if (CcPlayerCargoUsed(&sim->player) - old_slots + new_slots >
            sim->player.cargo_capacity) {
            SetError(error, error_capacity,
                     "Clear cargo space so Mara can load the food boxes.");
            return false;
        }
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
    CcEvent *accepted = PushEvent(
        sim, CC_EVENT_CHARTER_ACCEPTED, situation->id,
        situation->target_id, situation->cause_event_id,
        situation->deadline_day - sim->current_day, text);
    if (relief_load > 0 && relief_origin != NULL) {
        relief_origin->stock[CC_GOOD_FOOD] -= relief_load;
        sim->player.cargo[CC_GOOD_FOOD] += relief_load;
        RefreshSettlementGoodPrice(sim, relief_origin, CC_GOOD_FOOD);
        (void)snprintf(
            text, sizeof(text),
            "%s loads %d food boxes from %s's granary into the Crownless carriage.",
            situation->sponsor_name, relief_load, relief_origin->name);
        (void)PushEvent(
            sim, CC_EVENT_PLAYER_TRADE, sim->player.id,
            relief_origin->id, accepted->id,
            relief_load, text);
    }
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
    situation->end_reason = CC_QUEST_END_REFUSED;
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
    CcEvent *failure = PushEvent(
        sim, CC_EVENT_SITUATION_FAILED, situation->id,
        situation->target_id, situation->cause_event_id, -2, text);
    CcId failure_id = failure->id;
    CcCharacter *sponsor = CharacterMutable(
        sim, situation->sponsor_character_id);
    if (sponsor != NULL) {
        sponsor->player_disposition = ClampI32(
            sponsor->player_disposition - 8, -100, 100);
        sponsor->stress = ClampI32(sponsor->stress + 4, 0, 100);
    }
    FinishFrontAfterSituation(sim, situation, failure_id);
    ArchiveSituationOutcome(sim, situation, failure_id);
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
                      situation->id, withdrawal->id,
                      sim->current_day);
    if (sponsor != affected) {
        RememberCharacter(sponsor, CC_CHARACTER_MEMORY_PLAYER_WITHDREW,
                          situation->id, withdrawal->id,
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

static void ShiftRelationshipTrust(CcSim *sim, CcRelationship *relationship,
                                   int32_t amount, CcId parent_event_id,
                                   CcId situation_id, const char *text)
{
    if (sim == NULL || relationship == NULL || amount == 0) return;
    relationship->trust = ClampI32(relationship->trust + amount, -3, 3);
    const CcCharacter *from = CcSimCharacter(
        sim, relationship->from_character_id);
    CcEvent *event = PushSocialEvent(
        sim, CC_EVENT_RELATIONSHIP_CHANGED, situation_id,
        from != NULL ? from->current_settlement_id : 0U,
        parent_event_id, relationship->from_character_id,
        relationship->to_character_id, 0U, sim->player.id, amount, text);
    relationship->cause_event_id = event->id;
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
    bool mine_thread = situation->kind == CC_SITUATION_MONSTER_EXPEDITION;
    bool relationship_choice =
        response == CC_CHARACTER_RESPONSE_REPORT_EVIDENCE ||
        response == CC_CHARACTER_RESPONSE_KEEP_CONFIDENCE;
    if (relationship_choice &&
        (!mine_thread ||
         situation->discovery_stage != CC_DISCOVERY_DECISION ||
         character->id != situation->affected_character_id)) {
        SetError(error, error_capacity,
                 "You cannot choose that here.");
        return false;
    }
    if (mine_thread && situation->discovery_stage == CC_DISCOVERY_DECISION &&
        response == CC_CHARACTER_RESPONSE_LISTEN) {
        SetError(error, error_capacity,
                 "Choose whether to tell Mara or keep this between you and Jory.");
        return false;
    }
    CcCharacterMemoryKind memory_kind = response ==
        CC_CHARACTER_RESPONSE_LISTEN ? CC_CHARACTER_MEMORY_MET_PLAYER :
        response == CC_CHARACTER_RESPONSE_PLEDGE_HELP ?
            CC_CHARACTER_MEMORY_PLAYER_PROMISED : CC_CHARACTER_MEMORY_NONE;
    if (memory_kind == CC_CHARACTER_MEMORY_NONE && !relationship_choice) {
        SetError(error, error_capacity, "Choose a clear response.");
        return false;
    }
    if (memory_kind != CC_CHARACTER_MEMORY_NONE &&
        CcCharacterRemembers(character, memory_kind, situation->id)) {
        SetError(error, error_capacity,
                 "This conversation has already changed their view of you.");
        return false;
    }
    CcId parent = situation->lead_event_id != 0U ?
        situation->lead_event_id : situation->cause_event_id;
    if (response == CC_CHARACTER_RESPONSE_PLEDGE_HELP) {
        if (!CcSimSituationCanAccept(sim, situation)) {
            SetError(error, error_capacity,
                     "There is no job to accept yet. Ask what happened first.");
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
    CcEventKind event_kind = CC_EVENT_CHARACTER_INTERACTION;
    if (relationship_choice) {
        event_kind = CC_EVENT_RELATIONSHIP_CHANGED;
        (void)snprintf(
            text, sizeof(text), response == CC_CHARACTER_RESPONSE_REPORT_EVIDENCE ?
                "The player decides to tell Mara what Bren heard." :
                "The player agrees not to tell Mara.");
    } else if (mine_thread && response == CC_CHARACTER_RESPONSE_LISTEN &&
               situation->discovery_stage == CC_DISCOVERY_RUMOR) {
        event_kind = CC_EVENT_RUMOR_SHARED;
        (void)snprintf(text, sizeof(text),
                       "Jory tells the player that Bren ran from the west gallery and left his lamp.");
    } else if (mine_thread && response == CC_CHARACTER_RESPONSE_LISTEN &&
               situation->discovery_stage == CC_DISCOVERY_WITNESS) {
        event_kind = CC_EVENT_FACT_REVEALED;
        (void)snprintf(text, sizeof(text),
                       "Bren tells the player that he heard someone using a pick behind the old wall while Cera was below.");
    } else if (mine_thread && response == CC_CHARACTER_RESPONSE_LISTEN &&
               situation->discovery_stage == CC_DISCOVERY_AUTHORITY) {
        event_kind = CC_EVENT_FACT_REVEALED;
        (void)snprintf(text, sizeof(text),
                       "Mara believes Bren and agrees to help find Cera.");
    } else {
        (void)snprintf(
            text, sizeof(text), response == CC_CHARACTER_RESPONSE_LISTEN ?
                "The Crownless company hears %.24s's account of %s." :
                "The Crownless company promises %.24s that it will answer %s.",
            character->name, CcSituationKindName(situation->kind));
    }
    CcEvent *event = PushSocialEvent(
        sim, event_kind, situation->id,
        character->current_settlement_id, parent,
        character->id, sim->player.id, situation->affected_character_id,
        character->id,
        relationship_choice ?
            (response == CC_CHARACTER_RESPONSE_REPORT_EVIDENCE ? 1 : -1) :
        response == CC_CHARACTER_RESPONSE_LISTEN ? 2 : 8,
        text);
    CcId event_id = event->id;
    if (memory_kind != CC_CHARACTER_MEMORY_NONE) {
        RememberCharacter(character, memory_kind, situation->id,
                          event_id, sim->current_day);
    }

    int32_t disposition_change = response ==
        CC_CHARACTER_RESPONSE_LISTEN ? 2 :
        response == CC_CHARACTER_RESPONSE_PLEDGE_HELP ? 8 : 0;
    if (relationship_choice) {
        const CcRelationship *jory_to_mara = CcSimRelationship(
            sim, situation->affected_character_id,
            situation->sponsor_character_id);
        bool jory_wanted_report = jory_to_mara == NULL ||
            jory_to_mara->history !=
                CC_RELATIONSHIP_HISTORY_PROFESSIONAL_RIVALS;
        bool reported = response == CC_CHARACTER_RESPONSE_REPORT_EVIDENCE;
        disposition_change = reported == jory_wanted_report ? 3 : -5;
    }
    character->player_disposition = ClampI32(
        character->player_disposition + disposition_change,
        -100, 100);
    character->stress = ClampI32(
        character->stress -
            (response == CC_CHARACTER_RESPONSE_LISTEN ? 5 :
             response == CC_CHARACTER_RESPONSE_PLEDGE_HELP ? 12 : 2),
        0, 100);

    if (mine_thread && response == CC_CHARACTER_RESPONSE_LISTEN) {
        if (situation->discovery_stage == CC_DISCOVERY_RUMOR) {
            situation->discovery_stage = CC_DISCOVERY_WITNESS;
        } else if (situation->discovery_stage == CC_DISCOVERY_WITNESS) {
            situation->discovery_stage = CC_DISCOVERY_DECISION;
            CcCharacter *jory = CharacterMutable(
                sim, situation->affected_character_id);
            RememberKnowledge(jory, CC_KNOWLEDGE_IMMEDIATE_STAKE,
                              situation->id, character->id, event_id,
                              CC_KNOWLEDGE_TOLD, true, sim->current_day);
        } else if (situation->discovery_stage == CC_DISCOVERY_AUTHORITY) {
            situation->discovery_stage = CC_DISCOVERY_OFFER;
            CcCharacter *mara = CharacterMutable(
                sim, situation->sponsor_character_id);
            RememberKnowledge(mara, CC_KNOWLEDGE_WITNESS_ACCOUNT,
                              situation->id, sim->player.id, event_id,
                              CC_KNOWLEDGE_TOLD, false, sim->current_day);
            CcRelationship *mara_to_jory = RelationshipMutable(
                sim, situation->sponsor_character_id,
                situation->affected_character_id);
            CcRelationship *jory_to_mara = RelationshipMutable(
                sim, situation->affected_character_id,
                situation->sponsor_character_id);
            ShiftRelationshipTrust(
                sim, mara_to_jory, 1, event_id, situation->id,
                "Bren confirms Jory's warning, so Mara trusts Jory more.");
            ShiftRelationshipTrust(
                sim, jory_to_mara, 1,
                event_id, situation->id,
                "Mara acts on the warning, so Jory trusts Mara more.");
            RememberKnowledge(mara, CC_KNOWLEDGE_OFFER,
                              situation->id, mara != NULL ? mara->id : 0U,
                              event_id, CC_KNOWLEDGE_WITNESSED, false,
                              sim->current_day);
        }
    } else if (mine_thread && relationship_choice) {
        if (response == CC_CHARACTER_RESPONSE_REPORT_EVIDENCE) {
            situation->lead_path = CC_LEAD_PATH_REPORT;
            situation->discovery_stage = CC_DISCOVERY_AUTHORITY;
        } else {
            situation->lead_path = CC_LEAD_PATH_CONFIDENCE;
            situation->discovery_stage = CC_DISCOVERY_OFFER;
            RememberKnowledge(character, CC_KNOWLEDGE_OFFER,
                              situation->id, character->id, event_id,
                              CC_KNOWLEDGE_WITNESSED, true,
                              sim->current_day);
        }
    }
    if (mine_thread && event_id != 0U) situation->lead_event_id = event_id;
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
        sim->delayed_echo.parent_event_id = echo->id;
        sim->delayed_echo.due_day = sim->current_day + 30;
    } else {
        sim->delayed_echo.active = false;
        PromotePendingEcho(sim);
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
        CcSimRouteCrossesKingdomBorder(sim, route->id)) return;
    CcRoyalCarriage *carriage = NULL;
    if (sim->schema_version >= 41U) {
        for (int32_t i = 0; i < sim->royal_carriage_count; ++i) {
            CcRoyalCarriage *candidate = &sim->royal_carriages[i];
            if (candidate->kingdom_id == destination->kingdom_id &&
                candidate->location_id == origin->id &&
                candidate->mode == CC_ROYAL_CARRIAGE_IDLE &&
                candidate->active_shipment_id == 0U) {
                carriage = candidate;
                break;
            }
        }
        /* The daily trade planner can dispatch when a carriage returns. */
        if (carriage == NULL) return;
    }
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
    if (carriage != NULL) {
        carriage->route_id = route->id;
        carriage->destination_id = destination->id;
        carriage->target_id = destination->id;
        carriage->active_shipment_id = shipment->id;
        carriage->mode = CC_ROYAL_CARRIAGE_DELIVERING;
        carriage->departure_day = shipment->departure_day;
        carriage->arrival_day = shipment->arrival_day;
        carriage->blocked_since_day = 0;
        carriage->next_dispatch_day = sim->current_day + 7;
    }
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
    sim->journey.parent_event_id = event->id;
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
        sim->journey.parent_event_id = event->id;
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
    ExchangeGossip(sim, sim->player.id, destination->id, "Your fellow travelers");
    RevealCompleteRoute(sim, route->id);
    RevealSettlementRoadAnchors(sim, destination->id);
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
        ExchangeGossip(sim, courier->id, destination->id, "Royal couriers");
        courier->reliability = ClampI32(
            courier->reliability - sim->journey.danger / 12, 0, 100);
        if (courier->destination_settlement_id == destination->id) {
            DeliverCourier(sim, courier, true);
        }
    }
    char text[CC_EVENT_TEXT_CAPACITY];
    int32_t journey_watches =
        sim->journey.total_subticks / CC_WORLD_WATCH_SUBTICKS;
    (void)snprintf(text, sizeof(text),
                   "The carriage reaches %.24s from %.24s after %d road watches at %s pace (%d%% danger).",
                   destination->name,
                   origin != NULL ? origin->name : "the road",
                   journey_watches,
                   CcJourneyPaceName(sim->journey.pace),
                   sim->journey.danger);
    (void)PushEvent(sim, CC_EVENT_PLAYER_TRAVEL, sim->player.id,
                    destination->id, sim->journey.parent_event_id,
                    (journey_watches + 1) / 2,
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
    sim->journey.parent_event_id = event->id;
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

static uint32_t RoadHouseSeed(const CcSim *sim, CcId route_id)
{
    uint32_t seed = sim != NULL ? sim->world_seed : 0U;
    seed ^= (uint32_t)route_id;
    seed ^= (uint32_t)(route_id >> 32U);
    seed ^= seed >> 16U;
    seed *= UINT32_C(0x7feb352d);
    seed ^= seed >> 15U;
    return seed;
}

static const char *GeneratedRoadHouseName(const CcSim *sim, CcId route_id)
{
    static const char *const names[] = {
        "The Lantern and Pike",
        "The Three Wheels",
        "Ash Tree House",
        "The Red Mile",
        "Pilgrim's Rest",
        "The Barrow Lantern",
        "The Fox and Fir",
        "The Broken Crown"
    };
    uint32_t seed = RoadHouseSeed(sim, route_id);
    return names[seed % (sizeof(names) / sizeof(names[0]))];
}

const char *CcSimRoadHouseName(const CcSim *sim, CcId route_id)
{
    const CcRoadSite *site = CcSimRoadHouseSite(sim, route_id);
    if (site != NULL) return site->name;
    return GeneratedRoadHouseName(sim, route_id);
}

static int32_t RoadHouseTargetProgress(const CcSim *sim, CcId route_id)
{
    const CcRoadSite *site = CcSimRoadHouseSite(sim, route_id);
    int32_t progress = site != NULL ? site->progress_milli :
        300 + (int32_t)(RoadHouseSeed(sim, route_id) % 401U);
    const CcRoute *route = CcSimRoute(sim, route_id);
    CcId origin_id = sim != NULL && sim->journey.active ?
        sim->journey.origin_id : sim != NULL ? sim->player.location_id : 0U;
    if (route != NULL && origin_id == route->to_id) progress = 1000 - progress;
    return progress;
}

int32_t CcSimRoadHouseProgressMilli(const CcSim *sim, CcId route_id,
                                    int32_t journey_watch_count)
{
    if (journey_watch_count < 3) return 0;
    int32_t target = RoadHouseTargetProgress(sim, route_id);
    int32_t best_watch = 2;
    int32_t best_distance = INT32_MAX;
    for (int32_t watch = 2; watch < journey_watch_count; watch += 2) {
        int32_t progress = watch * 1000 / journey_watch_count;
        int32_t distance = progress > target ? progress - target :
                                                target - progress;
        if (distance < best_distance) {
            best_watch = watch;
            best_distance = distance;
        }
    }
    return best_watch * 1000 / journey_watch_count;
}

int32_t CcSimRoadHouseDistanceMiles(const CcSim *sim, CcId route_id)
{
    const CcRoute *route = CcSimRoute(sim, route_id);
    if (route == NULL) return 0;
    int32_t route_miles = route->travel_days * 18 +
        4 + (int32_t)(RoadHouseSeed(sim, route_id) % 9U);
    int32_t progress = RoadHouseTargetProgress(sim, route_id);
    return MaximumI32(1, (route_miles * progress + 500) / 1000);
}

CcMoney CcSimRoadHouseCost(const CcSim *sim, CcId route_id)
{
    return 4 + (CcMoney)(RoadHouseSeed(sim, route_id) % 5U);
}

int32_t CcSimJourneyWatchCount(const CcSim *sim)
{
    if (sim == NULL || !sim->journey.active ||
        sim->journey.total_subticks <= 0) return 0;
    return sim->journey.total_subticks / CC_WORLD_WATCH_SUBTICKS;
}

int32_t CcSimJourneyWatchNumber(const CcSim *sim)
{
    int32_t watch_count = CcSimJourneyWatchCount(sim);
    if (watch_count <= 0) return 0;
    if (sim->journey.phase == CC_JOURNEY_PHASE_RESTING) {
        return MinimumI32(
            watch_count,
            sim->journey.elapsed_subticks / CC_WORLD_WATCH_SUBTICKS);
    }
    return MinimumI32(
        watch_count,
        sim->journey.elapsed_subticks / CC_WORLD_WATCH_SUBTICKS + 1);
}

CcJourneyStopKind CcSimJourneyStop(const CcSim *sim)
{
    if (sim == NULL || !sim->journey.active ||
        sim->journey.phase != CC_JOURNEY_PHASE_RESTING ||
        sim->journey.elapsed_subticks <= 0 ||
        sim->journey.elapsed_subticks >= sim->journey.total_subticks ||
        sim->journey.elapsed_subticks % CC_WORLD_WATCH_SUBTICKS != 0) {
        return CC_JOURNEY_STOP_NONE;
    }
    int32_t completed_watch =
        sim->journey.elapsed_subticks / CC_WORLD_WATCH_SUBTICKS;
    return completed_watch % 2 == 0 ? CC_JOURNEY_STOP_OVERNIGHT :
                                      CC_JOURNEY_STOP_MIDDAY;
}

bool CcSimJourneyRoadHouseAvailable(const CcSim *sim)
{
    if (CcSimJourneyStop(sim) != CC_JOURNEY_STOP_OVERNIGHT) return false;
    int32_t watch_count = CcSimJourneyWatchCount(sim);
    int32_t house_progress = CcSimRoadHouseProgressMilli(
        sim, sim->journey.route_id, watch_count);
    int32_t completed_watch =
        sim->journey.elapsed_subticks / CC_WORLD_WATCH_SUBTICKS;
    return completed_watch * 1000 / watch_count == house_progress;
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
    int32_t first_boundary = sim->journey.elapsed_subticks /
        CC_WORLD_WATCH_SUBTICKS + 1;
    if (sim->journey.phase == CC_JOURNEY_PHASE_RESTING &&
        CcSimJourneyStop(sim) == CC_JOURNEY_STOP_OVERNIGHT) {
        world_subticks += CC_WORLD_WATCH_SUBTICKS;
    }
    int32_t watch_count = CcSimJourneyWatchCount(sim);
    for (int32_t watch = first_boundary; watch < watch_count; ++watch) {
        if (watch % 2 == 0) world_subticks += CC_WORLD_WATCH_SUBTICKS;
    }
    return (int32_t)((world_subticks + CC_WORLD_MINUTE_SUBTICKS - 1) /
                     CC_WORLD_MINUTE_SUBTICKS);
}

static int32_t HorseFeedRequired(int32_t travel_days)
{
    return MaximumI32(1, (travel_days + 1) / 2);
}

static bool JourneyCrossesRain(const CcSim *sim, const CcRoute *route,
                               int32_t departure_day)
{
    if (sim == NULL || route == NULL) return false;
    uint32_t value = sim->world_seed ^ (uint32_t)route->id ^
        (uint32_t)(route->id >> 32U) ^
        ((uint32_t)departure_day * UINT32_C(0x9e3779b9));
    value ^= value >> 16U;
    value *= UINT32_C(0x7feb352d);
    value ^= value >> 15U;
    value *= UINT32_C(0x846ca68b);
    value ^= value >> 16U;
    return value % 100U < 35U;
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
    bool opening_half_day = sim->journey.total_subticks == 0;
    int32_t travel_watches = opening_half_day ? 1 :
        MaximumI32(3, days * 2);
    int32_t base_fare = days + (route->smuggler_route ? 3 : 0);
    int32_t shadow_danger = DragonRouteShadowDanger(sim, route);
    bool waits_for_morning = !opening_half_day &&
        sim->clock.minute_subticks > 0;
    int32_t departure_day = sim->current_day +
        (waits_for_morning ? 1 : 0);
    *preview = (CcTravelPreview){
        .route_id = route->id,
        .destination_id = destination->id,
        .provision_cost = sim->schema_version >= 41U ? 0 :
            base_fare + TradeRouteToll(sim, route),
        .travel_days = days,
        .claimed_condition = map != NULL ? map->recorded_condition : -1,
        .claimed_danger = map != NULL ?
            ClampI32(map->recorded_danger + shadow_danger, 0, 95) : -1,
        .chart_accuracy = map != NULL ? map->accuracy : 0,
        .horse_feed_required = HorseFeedRequired(days),
        .horse_readiness = readiness,
        .travel_watches = travel_watches,
        .overnight_stops = (travel_watches - 1) / 2,
        .departure_wait_minutes = waits_for_morning ?
            (CC_WORLD_DAY_SUBTICKS - sim->clock.minute_subticks) /
                CC_WORLD_MINUTE_SUBTICKS : 0,
        .road_house_distance_miles = CcSimRoadHouseDistanceMiles(
            sim, route->id),
        .road_house_cost = CcSimRoadHouseCost(sim, route->id),
        .road_house_name = CcSimRoadHouseName(sim, route->id),
        .rain_expected = JourneyCrossesRain(
            sim, route, departure_day),
        .opening_half_day = opening_half_day,
        .charted = map != NULL,
        .destination_known = !route->smuggler_route || map != NULL ||
                             sponsored_night_passage,
        .sponsored_guide = sponsored_night_passage
    };
    return true;
}

static void SpoilPlayerJourneyCargo(CcSim *sim, bool rain_expected,
                                    int32_t *meat_spoiled,
                                    int32_t *grain_spoiled)
{
    if (sim == NULL || sim->schema_version < 33U) {
        *meat_spoiled = 0;
        *grain_spoiled = 0;
        return;
    }
    *meat_spoiled = sim->player.cargo[CC_GOOD_MEAT];
    *grain_spoiled = rain_expected ?
        sim->player.cargo[CC_GOOD_WHEAT] : 0;
    sim->player.cargo[CC_GOOD_MEAT] = 0;
    int64_t rotten_meat =
        (int64_t)sim->player.cargo[CC_GOOD_ROTTEN_MEAT] + *meat_spoiled;
    sim->player.cargo[CC_GOOD_ROTTEN_MEAT] =
        rotten_meat > CC_SIM_MAX_UNITS ? CC_SIM_MAX_UNITS :
                                         (int32_t)rotten_meat;
    if (*grain_spoiled > 0) {
        sim->player.cargo[CC_GOOD_WHEAT] = 0;
        int64_t rotten_grain =
            (int64_t)sim->player.cargo[CC_GOOD_ROTTEN_GRAIN] +
            *grain_spoiled;
        sim->player.cargo[CC_GOOD_ROTTEN_GRAIN] =
            rotten_grain > CC_SIM_MAX_UNITS ? CC_SIM_MAX_UNITS :
                                              (int32_t)rotten_grain;
    }
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
    /* Replay older journals with their original departure transfers. */
    int32_t base_fare = sim->schema_version >= 41U ? 0 :
        days + (route->smuggler_route ? 3 : 0);
    CcMoney toll = sim->schema_version >= 41U ? 0 :
        TradeRouteToll(sim, route);
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
        CcNutritionAvailable(origin->stock, CC_NUTRITION_ANIMAL) <
            preview.horse_feed_required * CC_NUTRITION_PER_RATION)) {
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
    int32_t total_subticks = preview.travel_watches *
        CC_WORLD_WATCH_SUBTICKS;
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
            (void)CcNutritionConsume(
                origin_market->stock, CC_NUTRITION_ANIMAL,
                preview.horse_feed_required * CC_NUTRITION_PER_RATION);
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
    bool waited_for_morning = preview.departure_wait_minutes > 0;
    if (waited_for_morning) {
        sim->clock.minute_subticks = 0;
        CcSimAdvanceDays(sim, 1);
    }
    int32_t meat_spoiled = 0;
    int32_t grain_spoiled = 0;
    SpoilPlayerJourneyCargo(
        sim, preview.rain_expected, &meat_spoiled, &grain_spoiled);
    ExchangeGossip(sim, sim->player.id, sim->player.location_id, "Your fellow travelers");
    CcId parent_event_id = LatestLocalCause(sim, destination->id);
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
    RevealSettlementRoadAnchors(sim, sim->journey.origin_id);
    RevealJourneyRoad(sim);
    char text[CC_EVENT_TEXT_CAPACITY];
    if (sim->schema_version >= 14U) {
        (void)snprintf(
            text, sizeof(text),
            "%.16s and %.16s pull from %.16s toward %.16s %sfor %d watches with %d fodder.",
            sim->schema_version >= 40U ? CcPonyName(sim->pony_company.team[0]) : sim->horse_team[0].name,
            sim->schema_version >= 40U ? CcPonyName(sim->pony_company.team[1]) : sim->horse_team[1].name,
            origin != NULL ? origin->name : "the waystation",
            destination->name,
            waited_for_morning ? "at first light " : "",
            preview.travel_watches,
            preview.horse_feed_required);
    } else {
        (void)snprintf(
            text, sizeof(text),
            "The Crownless carriage leaves %s for %s with %d travel watches reserved.",
            origin != NULL ? origin->name : "the waystation",
            destination->name, preview.travel_watches);
    }
    CcEvent *departure = PushEvent(
        sim, CC_EVENT_JOURNEY_DEPARTED, sim->player.id, route->id,
        parent_event_id, days, text);
    sim->journey.parent_event_id = departure->id;
    if (meat_spoiled > 0 || grain_spoiled > 0) {
        if (meat_spoiled > 0 && grain_spoiled > 0) {
            (void)snprintf(
                text, sizeof(text),
                "Before the first mile, %d Meat becomes Rotten Meat; rain changes %d Wheat into Rotten Grain.",
                meat_spoiled, grain_spoiled);
        } else if (meat_spoiled > 0) {
            (void)snprintf(
                text, sizeof(text),
                "Before the first mile, %d Meat becomes Rotten Meat.",
                meat_spoiled);
        } else {
            (void)snprintf(
                text, sizeof(text),
                "Rain changes %d Wheat into Rotten Grain.", grain_spoiled);
        }
        CcEvent *spoilage = PushEvent(
            sim, CC_EVENT_PLAYER_TRAVEL, sim->player.id, route->id,
            departure->id, meat_spoiled + grain_spoiled, text);
        sim->journey.parent_event_id = spoilage->id;
    }
    SetError(error, error_capacity, "");
    return true;
}

static int32_t RollD6(CcSim *sim)
{
    return 1 + (int32_t)(NextRandom(sim) % 6U);
}


static void RollEncounterLoot(CcSim *sim, CcBanditGroup *bandits,
                              const CcJourneyEncounter *journey,
                              CcId combat_event_id)
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
    const int32_t cap = free_slots;
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
        (void)snprintf(text + length, sizeof(text) - length, ".");
        (void)PushEvent(sim, CC_EVENT_ENCOUNTER_LOOT, journey->situation_id,
                        journey->route_id, combat_event_id,
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
    CcId outcome_event_id = outcome_event->id;
    sim->journey.phase = CC_JOURNEY_PHASE_TRAVELLING;
    sim->journey.parent_event_id = outcome_event_id;
    if (outcome == CC_JOURNEY_OUTCOME_COMBAT) {
        RollEncounterLoot(sim, bandits, &journey, outcome_event_id);
    }
    sim->resolved_journey_situation_id = journey.situation_id;
    sim->resolved_journey_outcome = outcome;
    sim->clock.game_minutes_per_second =
        CC_TRAVEL_GAME_MINUTES_PER_SECOND;
    sim->carriage.mode = CC_CARRIAGE_MOVING;
    sim->carriage.speed_milli_per_second =
        JourneyCarriageSpeedForPace(
            sim->journey.total_subticks, sim->journey.pace);
    CreateJourneyTraffic(sim, &journey, outcome_event_id);
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

static void ApplyTravelWatchStrain(CcSim *sim)
{
    const CcRoute *route = CcSimRoute(sim, sim->journey.route_id);
    if (route == NULL || sim->schema_version < 14U) return;
    int32_t cargo_strain = sim->player.cargo_capacity > 0 ?
        CcPlayerCargoUsed(&sim->player) * 3 /
            sim->player.cargo_capacity : 0;
    int32_t road_strain = MaximumI32(0, 60 - route->condition) / 18;
    int32_t pace_strain =
        sim->journey.pace == CC_JOURNEY_PACE_PUSH ? 4 :
        sim->journey.pace == CC_JOURNEY_PACE_CAREFUL ? -2 : 0;
    int32_t road_wear = MaximumI32(0, 70 - route->condition) / 35;
    int32_t pace_wear =
        sim->journey.pace == CC_JOURNEY_PACE_PUSH ? 2 :
        sim->journey.pace == CC_JOURNEY_PACE_STEADY ? 1 : 0;
    sim->carriage.condition = ClampI32(
        sim->carriage.condition - road_wear - pace_wear, 0, 100);
    for (int32_t i = 0; i < CC_CARRIAGE_HORSE_COUNT; ++i) {
        CcHorse *horse = &sim->horse_team[i];
        int32_t strength_strain = sim->schema_version >= 15U ?
            cargo_strain * MaximumI32(50, 150 - horse->strength) / 100 :
            cargo_strain;
        int32_t strain = 4 + strength_strain + road_strain + pace_strain -
            horse->hardiness / 35;
        horse->fatigue = ClampI32(
            horse->fatigue + MaximumI32(1, strain), 0, 100);
        horse->hunger = ClampI32(horse->hunger + 1, 0, 100);
        if (horse->fatigue >= 88 || horse->hunger >= 80) {
            horse->health = ClampI32(horse->health - 1, 1, 100);
        }
    }
}

static void RecoverJourneyTeam(CcSim *sim, int32_t fatigue_recovery,
                               int32_t hunger_recovery)
{
    if (sim->schema_version < 14U) return;
    for (int32_t i = 0; i < CC_CARRIAGE_HORSE_COUNT; ++i) {
        CcHorse *horse = &sim->horse_team[i];
        horse->fatigue = ClampI32(
            horse->fatigue - fatigue_recovery, 0, 100);
        horse->hunger = ClampI32(
            horse->hunger - hunger_recovery, 0, 100);
    }
}

static void ResumeJourney(CcSim *sim)
{
    sim->journey.phase = CC_JOURNEY_PHASE_TRAVELLING;
    sim->clock.game_minutes_per_second =
        CC_TRAVEL_GAME_MINUTES_PER_SECOND;
    sim->carriage.mode = CC_CARRIAGE_MOVING;
    sim->carriage.speed_milli_per_second = JourneyCarriageSpeedForPace(
        sim->journey.total_subticks, sim->journey.pace);
}

static void AdvanceJourneyRestWatch(CcSim *sim)
{
    sim->clock.minute_subticks += CC_WORLD_WATCH_SUBTICKS;
    while (sim->clock.minute_subticks >= CC_WORLD_DAY_SUBTICKS) {
        sim->clock.minute_subticks -= CC_WORLD_DAY_SUBTICKS;
        CcSimAdvanceDays(sim, 1);
    }
}

const CcRoadSite *CcSimJourneyRoadSiteStop(const CcSim *sim)
{
    if (sim == NULL || sim->schema_version < 39U ||
        !sim->journey.active ||
        sim->journey.phase != CC_JOURNEY_PHASE_TRAVELLING) return NULL;
    const CcRoute *route = CcSimRoute(sim, sim->journey.route_id);
    if (route == NULL) return NULL;
    for (int32_t i = 0; i < sim->road_site_count; ++i) {
        const CcRoadSite *site = &sim->road_sites[i];
        if (site->route_id != route->id ||
            (sim->journey.road_site_stop_mask & (UINT32_C(1) << i)) != 0U)
            continue;
        int32_t progress = sim->journey.origin_id == route->from_id ?
            site->progress_milli : 1000 - site->progress_milli;
        int32_t distance = sim->carriage.progress_milli - progress;
        if (distance >= -20 && distance <= 30) return site;
    }
    return NULL;
}

static bool ApplyRoadSiteStop(CcSim *sim, const CcCommand *command,
                              char *error, size_t error_capacity)
{
    const CcRoadSite *site = CcSimJourneyRoadSiteStop(sim);
    if (site == NULL || site->id != command->target_id) {
        SetError(error, error_capacity, "Reach this roadside stop first.");
        return false;
    }
    int32_t slot = (int32_t)(site - sim->road_sites);
    sim->journey.road_site_stop_mask |= UINT32_C(1) << slot;
    bool camping = command->kind == CC_COMMAND_CAMP_ROAD_SITE;
    char text[CC_EVENT_TEXT_CAPACITY];
    if (camping) {
        RecoverJourneyTeam(sim, 8, 5);
        sim->journey.danger = ClampI32(sim->journey.danger + 3, 0, 95);
        AdvanceJourneyRestWatch(sim);
        (void)snprintf(text, sizeof(text),
            "The company camps beside %.40s for one watch. The team rests while a guard keeps watch by the carriage.",
            site->name);
    } else {
        (void)snprintf(text, sizeof(text),
            "The company passes the turn to %.40s and follows the main road.",
            site->name);
    }
    CcEvent *event = PushEvent(sim,
        camping ? CC_EVENT_JOURNEY_CAMP : CC_EVENT_JOURNEY_BREAK,
        sim->player.id, sim->journey.route_id,
        sim->journey.parent_event_id, camping ? 3 : 0, text);
    sim->journey.parent_event_id = event->id;
    SetError(error, error_capacity, "");
    return true;
}

static bool ApplyJourneyStopAction(CcSim *sim, const CcCommand *command,
                                   char *error, size_t error_capacity)
{
    CcJourneyStopKind stop = CcSimJourneyStop(sim);
    if (sim == NULL || command == NULL || stop == CC_JOURNEY_STOP_NONE) {
        SetError(error, error_capacity,
                 "The carriage is not waiting at a travel stop.");
        return false;
    }
    bool midday = stop == CC_JOURNEY_STOP_MIDDAY;
    bool overnight = stop == CC_JOURNEY_STOP_OVERNIGHT;
    bool road_house = CcSimJourneyRoadHouseAvailable(sim);
    char text[CC_EVENT_TEXT_CAPACITY];
    CcEventKind event_kind = CC_EVENT_JOURNEY_BREAK;
    int32_t magnitude = 0;

    if (command->kind == CC_COMMAND_TAKE_JOURNEY_BREAK && midday) {
        RecoverJourneyTeam(sim, 2, 0);
        sim->journey.danger = ClampI32(sim->journey.danger - 2, 0, 95);
        (void)snprintf(
            text, sizeof(text),
            "The company waters the team, checks the wheels, and reads the road before the afternoon watch.");
    } else if (command->kind == CC_COMMAND_PRESS_ON && midday) {
        for (int32_t i = 0; i < CC_CARRIAGE_HORSE_COUNT; ++i) {
            sim->horse_team[i].fatigue = ClampI32(
                sim->horse_team[i].fatigue + 4, 0, 100);
        }
        sim->journey.danger = ClampI32(sim->journey.danger + 5, 0, 95);
        magnitude = 5;
        (void)snprintf(
            text, sizeof(text),
            "The company presses through the midday stop. The team tires and the road grows harder to read.");
    } else if (command->kind == CC_COMMAND_MAKE_CAMP && overnight) {
        RecoverJourneyTeam(sim, 8, 5);
        sim->journey.danger = ClampI32(sim->journey.danger + 3, 0, 95);
        AdvanceJourneyRestWatch(sim);
        event_kind = CC_EVENT_JOURNEY_CAMP;
        magnitude = 3;
        (void)snprintf(
            text, sizeof(text),
            "The company makes camp, feeds the team from its reserved fodder, and keeps a lantern watch until morning.");
    } else if (command->kind == CC_COMMAND_LODGE_ROAD_HOUSE &&
               overnight && road_house) {
        CcMoney cost = CcSimRoadHouseCost(sim, sim->journey.route_id);
        if (sim->player.coins < cost) {
            SetError(error, error_capacity,
                     "The company cannot afford beds and stable feed here.");
            return false;
        }
        sim->player.coins -= cost;
        CcSettlement *origin = CcSimSettlementMutable(
            sim, sim->journey.origin_id);
        if (origin != NULL) origin->market_coins += cost;
        RecoverJourneyTeam(sim, 15, 10);
        sim->carriage.condition = ClampI32(
            sim->carriage.condition + 2, 0, 100);
        sim->journey.danger = ClampI32(sim->journey.danger - 5, 0, 95);
        sim->journey.ambush_pending = false;
        sim->journey.ambush_resolved = true;
        AdvanceJourneyRestWatch(sim);
        event_kind = CC_EVENT_ROAD_HOUSE_LODGING;
        magnitude = (int32_t)cost;
        (void)snprintf(
            text, sizeof(text),
            "The company pays %d crowns at %.32s. Warm beds, stable feed, and a wheelwright make the morning safer.",
            (int32_t)cost,
            CcSimRoadHouseName(sim, sim->journey.route_id));
    } else {
        SetError(error, error_capacity, midday ?
                 "Choose a midday break or press on." :
                 road_house ? "Choose camp or the road house." :
                              "Choose camp before the next watch.");
        return false;
    }

    CcEvent *event = PushEvent(
        sim, event_kind, sim->player.id, sim->journey.route_id,
        sim->journey.parent_event_id, magnitude, text);
    sim->journey.parent_event_id = event->id;
    ResumeJourney(sim);
    SetError(error, error_capacity, "");
    return true;
}

static void PauseJourneyForWatchStop(CcSim *sim)
{
    if (sim->journey.ambush_pending && !sim->journey.ambush_warned) {
        WarnJourneyAmbush(sim);
    }
    sim->journey.phase = CC_JOURNEY_PHASE_RESTING;
    sim->clock.game_minutes_per_second = CC_IDLE_GAME_MINUTES_PER_SECOND;
    sim->carriage.mode = CC_CARRIAGE_STOPPED;
    sim->carriage.speed_milli_per_second = 0;
}

void CcSimAdvanceRuntimeTicks(CcSim *sim, int32_t ticks)
{
    if (sim == NULL || ticks <= 0 || !sim->journey.active ||
        sim->journey.phase != CC_JOURNEY_PHASE_TRAVELLING ||
        sim->clock.tick > UINT64_MAX - (uint64_t)ticks) return;
    for (int32_t tick = 0; tick < ticks; ++tick) {
        if (sim->schema_version >= 40U && sim->pony_company.encounter >= 0) break;
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
        int32_t next_watch =
            (sim->journey.elapsed_subticks / CC_WORLD_WATCH_SUBTICKS + 1) *
            CC_WORLD_WATCH_SUBTICKS;
        int32_t next_limit = MinimumI32(
            sim->journey.total_subticks, next_watch);
        sim->journey.elapsed_subticks = MinimumI32(
            next_limit, sim->journey.elapsed_subticks + journey_rate);
        sim->carriage.progress_milli = sim->journey.total_subticks > 0 ?
            (int32_t)(((int64_t)sim->journey.elapsed_subticks * 1000) /
                      sim->journey.total_subticks) : 0;
        RevealJourneyRoad(sim);
        if (sim->journey.elapsed_subticks >= sim->journey.total_subticks) {
            ApplyTravelWatchStrain(sim);
            FinishJourneyArrival(sim);
            continue;
        }
        if (sim->journey.elapsed_subticks == next_watch) {
            ApplyTravelWatchStrain(sim);
            PauseJourneyForWatchStop(sim);
            continue;
        }
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
        (command->amount == 0 &&
         sim->player.cargo[CC_GOOD_TOOLS] >= 2 &&
         sim->player.cargo[CC_GOOD_WOOD] >= 2 &&
         sim->player.cargo[CC_GOOD_STONE] >= 2);
    bool use_cash = command->amount == 2 ||
        (command->amount == 0 && !use_tools);
    if (command->amount < 0 || command->amount > 2) {
        SetError(error, error_capacity,
                 "Choose a tools repair or a cash repair.");
        return false;
    }
    if (use_tools && sim->player.cargo[CC_GOOD_TOOLS] >= 2 &&
        sim->player.cargo[CC_GOOD_WOOD] >= 2 &&
        sim->player.cargo[CC_GOOD_STONE] >= 2) {
        sim->player.cargo[CC_GOOD_TOOLS] -= 2;
        sim->player.cargo[CC_GOOD_WOOD] -= 2;
        sim->player.cargo[CC_GOOD_STONE] -= 2;
    } else if (use_cash && sim->player.coins >= 18) {
        sim->player.coins -= 18;
    } else {
        SetError(error, error_capacity, use_tools ?
                 "The material repair requires 2 Tools, 2 Wood, and 2 Stone." :
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
                   "Local crews use the company's Tools, Wood, and Stone to reopen the treaty bridge." :
                   "Paid officials reopen the treaty bridge after three days of fees and delay.");
    (void)PushEvent(sim, CC_EVENT_ROUTE_REPAIRED, route->id,
                    sim->player.location_id, 0, route->condition, text);
    sim->player.reputation += 2;
    ResolveTargetSituations(sim, CC_SITUATION_ROUTE_REPAIR, route->id);
    SetError(error, error_capacity, "");
    return true;
}

static bool DungeonObjectiveReached(const CcDungeon *dungeon)
{
    if (dungeon == NULL) return false;
    for (int32_t i = 0; i < dungeon->room_count; ++i) {
        if ((dungeon->rooms[i].state_flags &
             CC_DUNGEON_ROOM_OBJECTIVE_REACHED) != 0U) return true;
    }
    return false;
}

bool CcSimDungeonOutcomeAvailable(const CcDungeon *dungeon,
                                  CcDungeonState outcome)
{
    if (dungeon == NULL || !DungeonObjectiveReached(dungeon)) return false;
    if (outcome == CC_DUNGEON_EXPLORED) return true;
    if (outcome == CC_DUNGEON_PUBLIC_ROUTE) {
        for (int32_t i = 0; i < dungeon->link_count; ++i) {
            const CcDungeonLink *link = &dungeon->links[i];
            if (link->kind == CC_DUNGEON_LINK_SHORTCUT &&
                (link->flags & CC_DUNGEON_LINK_OPEN) != 0U) return true;
        }
        return false;
    }
    if (outcome == CC_DUNGEON_SMUGGLER_ROUTE) {
        for (int32_t i = 0; i < dungeon->room_count; ++i) {
            const CcDungeonRoom *room = &dungeon->rooms[i];
            if ((room->flags & CC_DUNGEON_ROOM_SMUGGLER) != 0U &&
                (room->state_flags & CC_DUNGEON_ROOM_SEARCHED) != 0U) {
                return true;
            }
        }
        return false;
    }
    if (outcome == CC_DUNGEON_RESEALED) {
        for (int32_t i = 0; i < dungeon->room_count; ++i) {
            const CcDungeonRoom *room = &dungeon->rooms[i];
            if (room->kind == CC_DUNGEON_ROOM_BRIDGE &&
                (room->state_flags & CC_DUNGEON_ROOM_SEARCHED) != 0U) {
                return true;
            }
        }
    }
    return false;
}

static void ClearDungeonEncounter(CcDungeonExpedition *expedition)
{
    if (expedition == NULL) return;
    expedition->encounter_kind = CC_DUNGEON_ENCOUNTER_NONE;
    expedition->encounter_reaction = 0;
    expedition->encounter_room = -1;
}

static void RollDungeonEncounter(CcSim *sim, CcDungeon *dungeon)
{
    if (sim == NULL || dungeon == NULL ||
        !sim->dungeon_expedition.active ||
        sim->dungeon_expedition.encounter_kind !=
            CC_DUNGEON_ENCOUNTER_NONE) return;
    CcDungeonExpedition *expedition = &sim->dungeon_expedition;
    CcDungeonRoom *room = &dungeon->rooms[expedition->current_room];
    if ((room->flags & CC_DUNGEON_ROOM_SAFE) != 0U) return;
    int32_t threshold = 1 + expedition->noise / 4 +
                        (expedition->light_remaining <= 0 ? 1 : 0);
    if ((room->state_flags & CC_DUNGEON_ROOM_CLEARED) != 0U) threshold -= 1;
    threshold = ClampI32(threshold, 1, 4);
    if ((int32_t)(NextDungeonRandom(dungeon) % 6U) >= threshold) return;

    uint32_t roll = NextDungeonRandom(dungeon);
    if ((room->flags & CC_DUNGEON_ROOM_STONEBACK) != 0U) {
        expedition->encounter_kind = CC_DUNGEON_ENCOUNTER_STONEBACKS;
    } else if ((room->flags & CC_DUNGEON_ROOM_GOBLIN) != 0U) {
        expedition->encounter_kind = sim->goblins.cohesion >= 50 ?
            CC_DUNGEON_ENCOUNTER_TITHE_KEEPERS :
            CC_DUNGEON_ENCOUNTER_GOBLIN_DESERTERS;
    } else if (dungeon->state == CC_DUNGEON_SMUGGLER_ROUTE &&
               (roll & 1U) != 0U) {
        expedition->encounter_kind = CC_DUNGEON_ENCOUNTER_SMUGGLERS;
    } else {
        expedition->encounter_kind = CC_DUNGEON_ENCOUNTER_MONSTERS;
    }
    int32_t reaction = 2 + (int32_t)(NextDungeonRandom(dungeon) % 6U) +
                       (int32_t)(NextDungeonRandom(dungeon) % 6U);
    if (expedition->encounter_kind == CC_DUNGEON_ENCOUNTER_TITHE_KEEPERS) {
        reaction += (sim->goblins.cohesion - 50) / 20;
    }
    if (expedition->noise >= 6) reaction -= 1;
    expedition->encounter_reaction = ClampI32(reaction, 2, 12);
    expedition->encounter_room = expedition->current_room;
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(text, sizeof(text),
                   "A %s finds the company in %s; its reaction is %s.",
                   CcDungeonEncounterName(expedition->encounter_kind),
                   room->name,
                   CcDungeonReactionName(expedition->encounter_reaction));
    (void)PushEvent(sim, CC_EVENT_DUNGEON_ENCOUNTER, dungeon->id,
                    dungeon->id, 0U, expedition->encounter_reaction, text);
}

static void SpendDungeonTurn(CcSim *sim, CcDungeon *dungeon,
                             int32_t noise, bool check_encounter)
{
    CcDungeonExpedition *expedition = &sim->dungeon_expedition;
    expedition->turns_elapsed += 1;
    expedition->light_remaining = MaximumI32(
        0, expedition->light_remaining - 1);
    expedition->noise = ClampI32(expedition->noise + noise - 1, 0, 10);
    if (expedition->light_remaining == 0) {
        expedition->strain = ClampI32(expedition->strain + 4, 0, 100);
    }
    if ((expedition->turns_elapsed % 6) == 0) {
        int32_t eaten = CcNutritionConsume(
            sim->player.cargo, CC_NUTRITION_TRAVEL,
            CC_NUTRITION_PER_RATION);
        if (eaten < CC_NUTRITION_PER_RATION) {
            expedition->strain = ClampI32(
                expedition->strain + 15, 0, 100);
        }
        CcSimAdvanceDays(sim, 1);
        expedition->days_elapsed += 1;
    }
    if (check_encounter) RollDungeonEncounter(sim, dungeon);
}

static bool ApplyBeginDungeonExpedition(CcSim *sim,
                                        const CcCommand *command,
                                        char *error, size_t error_capacity)
{
    CcDungeon *dungeon = DungeonMutable(sim, command->target_id);
    if (dungeon == NULL || sim->player.location_id != dungeon->settlement_id) {
        SetError(error, error_capacity,
                 "The carriage must be parked at the dungeon entrance.");
        return false;
    }
    if (sim->journey.active || sim->dungeon_expedition.active) {
        SetError(error, error_capacity,
                 "The company is already committed to a journey.");
        return false;
    }
    if (dungeon->state == CC_DUNGEON_SEALED ||
        dungeon->state == CC_DUNGEON_RESEALED) {
        SetError(error, error_capacity, "The Underroad is sealed.");
        return false;
    }
    if (CcNutritionAvailable(
            sim->player.cargo, CC_NUTRITION_TRAVEL) <
        CC_NUTRITION_PER_RATION) {
        SetError(error, error_capacity,
                 "Carry at least 1 Bread or Meat before entering the Underroad.");
        return false;
    }
    sim->dungeon_expedition = (CcDungeonExpedition){
        .active = true,
        .dungeon_id = dungeon->id,
        .current_room = 0,
        .light_remaining = 18,
        .maximum_depth = 0,
        .encounter_room = -1
    };
    dungeon->rooms[0].state_flags |= CC_DUNGEON_ROOM_DISCOVERED;
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(text, sizeof(text),
                   "The company leaves the carriage at %s and enters %s.",
                   dungeon->rooms[0].name, dungeon->name);
    (void)PushEvent(sim, CC_EVENT_DUNGEON_EXPEDITION_BEGAN,
                    sim->player.id, dungeon->id, 0U, 0, text);
    SetError(error, error_capacity, "");
    return true;
}

static CcDungeonLink *DungeonTravelLink(CcDungeon *dungeon,
                                        int32_t from_room,
                                        int32_t to_room)
{
    if (dungeon == NULL) return NULL;
    for (int32_t i = 0; i < dungeon->link_count; ++i) {
        CcDungeonLink *link = &dungeon->links[i];
        if (DungeonLinkAllowsTravel(link, from_room) &&
            DungeonLinkOtherRoom(link, from_room) == to_room) return link;
    }
    return NULL;
}

static void MarkDungeonThresholdReached(CcSim *sim, CcDungeon *dungeon,
                                        CcDungeonRoom *room)
{
    if (sim == NULL || dungeon == NULL || room == NULL ||
        (room->flags & CC_DUNGEON_ROOM_OBJECTIVE) == 0U ||
        (room->state_flags & CC_DUNGEON_ROOM_OBJECTIVE_REACHED) != 0U) {
        return;
    }
    room->state_flags |= CC_DUNGEON_ROOM_OBJECTIVE_REACHED;
    if (dungeon->state == CC_DUNGEON_DISTURBED) {
        CcDungeonEffects explored = DungeonEffects(CC_DUNGEON_EXPLORED);
        dungeon->state = CC_DUNGEON_EXPLORED;
        dungeon->regional_pressure = 40;
        for (int32_t i = 0; i < sim->monster_count; ++i) {
            CcMonsterPopulation *monster = &sim->monsters[i];
            if (monster->dungeon_id != dungeon->id) continue;
            monster->hunting_pressure = ClampI32(
                monster->hunting_pressure + explored.hunting_pressure,
                0, 100);
            monster->population = MaximumI32(
                0, monster->population - explored.population_loss);
            monster->pressure = MaximumI32(
                0, monster->pressure - explored.monster_pressure_loss);
        }
    }
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(text, sizeof(text),
                   "The company reaches %s. Heat and tribute tracks continue toward Hollowbarrow.",
                   room->name);
    (void)PushEvent(sim, CC_EVENT_DUNGEON_THRESHOLD_REACHED,
                    sim->player.id, dungeon->id, 0U, room->depth, text);
}

static bool ApplyMoveDungeon(CcSim *sim, const CcCommand *command,
                             char *error, size_t error_capacity)
{
    CcDungeonExpedition *expedition = &sim->dungeon_expedition;
    CcDungeon *dungeon = DungeonMutable(sim, expedition->dungeon_id);
    if (!expedition->active || dungeon == NULL) {
        SetError(error, error_capacity, "No dungeon expedition is active.");
        return false;
    }
    if (expedition->encounter_kind != CC_DUNGEON_ENCOUNTER_NONE) {
        SetError(error, error_capacity,
                 "The encounter blocks the passage. Parley, evade, force it, or retreat.");
        return false;
    }
    if (expedition->strain >= 100) {
        SetError(error, error_capacity,
                 "The company is spent and must retreat.");
        return false;
    }
    int32_t target = command->amount;
    if (target < 0 || target >= dungeon->room_count ||
        DungeonTravelLink(dungeon, expedition->current_room, target) == NULL) {
        SetError(error, error_capacity,
                 "That passage is not open from this chamber.");
        return false;
    }
    expedition->current_room = target;
    CcDungeonRoom *room = &dungeon->rooms[target];
    room->state_flags |= CC_DUNGEON_ROOM_DISCOVERED;
    expedition->maximum_depth = MaximumI32(
        expedition->maximum_depth, room->depth);
    if ((room->flags & CC_DUNGEON_ROOM_HAZARD) != 0U) {
        expedition->strain = ClampI32(expedition->strain + 4, 0, 100);
    }
    SpendDungeonTurn(sim, dungeon, 0, true);
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(text, sizeof(text), "The company enters %s.", room->name);
    (void)PushEvent(sim, CC_EVENT_DUNGEON_ROOM_ENTERED,
                    sim->player.id, dungeon->id, 0U, target, text);
    MarkDungeonThresholdReached(sim, dungeon, room);
    SetError(error, error_capacity, "");
    return true;
}

static bool ApplySearchDungeon(CcSim *sim, char *error,
                               size_t error_capacity)
{
    CcDungeonExpedition *expedition = &sim->dungeon_expedition;
    CcDungeon *dungeon = DungeonMutable(sim, expedition->dungeon_id);
    if (!expedition->active || dungeon == NULL) {
        SetError(error, error_capacity, "No dungeon expedition is active.");
        return false;
    }
    if (expedition->encounter_kind != CC_DUNGEON_ENCOUNTER_NONE) {
        SetError(error, error_capacity,
                 "The encounter must be dealt with before searching.");
        return false;
    }
    CcDungeonRoom *room = &dungeon->rooms[expedition->current_room];
    if ((room->state_flags & CC_DUNGEON_ROOM_SEARCHED) != 0U) {
        SetError(error, error_capacity, "This chamber has already been searched.");
        return false;
    }
    room->state_flags |= CC_DUNGEON_ROOM_SEARCHED;
    for (int32_t i = 0; i < dungeon->link_count; ++i) {
        CcDungeonLink *link = &dungeon->links[i];
        if ((link->kind == CC_DUNGEON_LINK_SECRET ||
             link->kind == CC_DUNGEON_LINK_SHORTCUT) &&
            (link->from_room == expedition->current_room ||
             link->to_room == expedition->current_room)) {
            link->flags |= CC_DUNGEON_LINK_DISCOVERED;
        }
    }
    int32_t cargo_space = sim->player.cargo_capacity -
                          CcPlayerCargoUsed(&sim->player);
    int32_t taken = MinimumI32(MaximumI32(cargo_space, 0),
                               room->loot_quantity);
    if (taken > 0) {
        sim->player.cargo[room->loot_good] += taken;
        room->loot_quantity -= taken;
        char text[CC_EVENT_TEXT_CAPACITY];
        (void)snprintf(text, sizeof(text),
                       "The company recovers %d %s from %s.", taken,
                       CcGoodName(room->loot_good), room->name);
        (void)PushEvent(sim, CC_EVENT_DUNGEON_LOOT,
                        sim->player.id, dungeon->id, 0U, taken, text);
    }
    SpendDungeonTurn(sim, dungeon, 2, true);
    SetError(error, error_capacity, "");
    return true;
}

static bool ApplyOpenDungeonShortcut(CcSim *sim,
                                     const CcCommand *command,
                                     char *error, size_t error_capacity)
{
    CcDungeonExpedition *expedition = &sim->dungeon_expedition;
    CcDungeon *dungeon = DungeonMutable(sim, expedition->dungeon_id);
    if (!expedition->active || dungeon == NULL) {
        SetError(error, error_capacity, "No dungeon expedition is active.");
        return false;
    }
    if (expedition->encounter_kind != CC_DUNGEON_ENCOUNTER_NONE) {
        SetError(error, error_capacity,
                 "The encounter must be dealt with before working.");
        return false;
    }
    int32_t slot = command->amount;
    if (slot < 0 || slot >= dungeon->link_count) {
        SetError(error, error_capacity, "That shortcut does not exist.");
        return false;
    }
    CcDungeonLink *link = &dungeon->links[slot];
    if (link->kind != CC_DUNGEON_LINK_SHORTCUT ||
        (link->flags & CC_DUNGEON_LINK_DISCOVERED) == 0U ||
        (link->flags & CC_DUNGEON_LINK_OPEN) != 0U ||
        (link->from_room != expedition->current_room &&
         link->to_room != expedition->current_room)) {
        SetError(error, error_capacity,
                 "There is no closed, discovered shortcut here.");
        return false;
    }
    if (sim->player.cargo[CC_GOOD_TOOLS] < 1) {
        SetError(error, error_capacity,
                 "Opening the freight lift requires 1 Tools.");
        return false;
    }
    sim->player.cargo[CC_GOOD_TOOLS] -= 1;
    link->flags |= CC_DUNGEON_LINK_OPEN;
    SpendDungeonTurn(sim, dungeon, 4, false);
    SpendDungeonTurn(sim, dungeon, 3, true);
    (void)PushEvent(sim, CC_EVENT_DUNGEON_SHORTCUT_OPENED,
                    sim->player.id, dungeon->id, 0U, slot,
                    "The company braces the old freight lift and opens a lasting shortcut.");
    SetError(error, error_capacity, "");
    return true;
}

static bool ApplyResolveDungeonEncounter(CcSim *sim,
                                         const CcCommand *command,
                                         char *error,
                                         size_t error_capacity)
{
    CcDungeonExpedition *expedition = &sim->dungeon_expedition;
    CcDungeon *dungeon = DungeonMutable(sim, expedition->dungeon_id);
    if (!expedition->active || dungeon == NULL ||
        expedition->encounter_kind == CC_DUNGEON_ENCOUNTER_NONE) {
        SetError(error, error_capacity, "There is no dungeon encounter to resolve.");
        return false;
    }
    bool resolved = false;
    if (command->amount == CC_DUNGEON_APPROACH_PARLEY) {
        int32_t score = expedition->encounter_reaction;
        if (score < 7 && CcNutritionAvailable(
                sim->player.cargo, CC_NUTRITION_TRAVEL) >=
                CC_NUTRITION_PER_RATION) {
            (void)CcNutritionConsume(
                sim->player.cargo, CC_NUTRITION_TRAVEL,
                CC_NUTRITION_PER_RATION);
            score += 3;
        }
        if (expedition->encounter_kind == CC_DUNGEON_ENCOUNTER_STONEBACKS &&
            sim->player.cargo[CC_GOOD_TOOLS] > 0) score += 1;
        resolved = score >= 7;
        if (!resolved) {
            expedition->strain = ClampI32(expedition->strain + 5, 0, 100);
            expedition->encounter_reaction = ClampI32(
                expedition->encounter_reaction + 1, 2, 12);
        }
        SpendDungeonTurn(sim, dungeon, resolved ? 0 : 2, false);
    } else if (command->amount == CC_DUNGEON_APPROACH_EVADE) {
        int32_t score = 1 + (int32_t)(NextDungeonRandom(dungeon) % 6U);
        if (expedition->light_remaining > 0) score += 1;
        if (expedition->noise <= 2) score += 1;
        if (CcPlayerCargoUsed(&sim->player) >=
            sim->player.cargo_capacity - 2) score -= 1;
        resolved = score >= 4;
        if (!resolved) {
            expedition->strain = ClampI32(expedition->strain + 12, 0, 100);
        }
        SpendDungeonTurn(sim, dungeon, resolved ? 0 : 3, false);
    } else if (command->amount == CC_DUNGEON_APPROACH_FORCE) {
        int32_t score = 1 + (int32_t)(NextDungeonRandom(dungeon) % 6U) +
                        (sim->player.cargo[CC_GOOD_WEAPONS] > 0 ? 2 : 0);
        resolved = score >= 4;
        expedition->strain = ClampI32(
            expedition->strain + (resolved ? 8 : 22), 0, 100);
        if (resolved) {
            dungeon->rooms[expedition->current_room].state_flags |=
                CC_DUNGEON_ROOM_CLEARED;
        }
        SpendDungeonTurn(sim, dungeon, 4, false);
    } else {
        SetError(error, error_capacity,
                 "Choose parley, evade, or force passage.");
        return false;
    }
    if (resolved) ClearDungeonEncounter(expedition);
    SetError(error, error_capacity, "");
    return true;
}

static bool ApplyRetreatDungeon(CcSim *sim, char *error,
                                size_t error_capacity)
{
    CcDungeonExpedition *expedition = &sim->dungeon_expedition;
    CcDungeon *dungeon = DungeonMutable(sim, expedition->dungeon_id);
    if (!expedition->active || dungeon == NULL) {
        SetError(error, error_capacity, "No dungeon expedition is active.");
        return false;
    }
    int32_t depth = dungeon->rooms[expedition->current_room].depth;
    bool flight = expedition->current_room != 0;
    if (flight) {
        expedition->strain = ClampI32(
            expedition->strain + 8 + depth * 6, 0, 100);
        int32_t cargo_used = CcPlayerCargoUsed(&sim->player);
        if ((expedition->encounter_kind != CC_DUNGEON_ENCOUNTER_NONE ||
             expedition->strain >= 75) && cargo_used > 0) {
            CcDungeonRoom *room =
                &dungeon->rooms[expedition->current_room];
            for (int32_t good = CC_GOOD_COUNT - 1; good >= 0; --good) {
                if (sim->player.cargo[good] <= 0) continue;
                if (room->loot_quantity > 0 &&
                    room->loot_good != (CcGood)good) {
                    continue;
                }
                sim->player.cargo[good] -= 1;
                room->loot_good = (CcGood)good;
                room->loot_quantity += 1;
                break;
            }
        }
    }
    if (expedition->days_elapsed == 0) CcSimAdvanceDays(sim, 1);
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(text, sizeof(text),
                   flight ?
                   "The company abandons its position and fights back to the carriage after %d turns." :
                   "The company returns to the carriage after %d turns.",
                   expedition->turns_elapsed);
    (void)PushEvent(sim, CC_EVENT_DUNGEON_EXPEDITION_ENDED,
                    sim->player.id, dungeon->id, 0U,
                    expedition->maximum_depth, text);
    *expedition = (CcDungeonExpedition){0};
    expedition->current_room = -1;
    expedition->encounter_room = -1;
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
    if (sim->dungeon_expedition.active) {
        SetError(error, error_capacity,
                 "Return to the carriage before changing the Underroad.");
        return false;
    }
    if (command->dungeon_state != CC_DUNGEON_EXPLORED &&
        command->dungeon_state != CC_DUNGEON_PUBLIC_ROUTE &&
        command->dungeon_state != CC_DUNGEON_SMUGGLER_ROUTE &&
        command->dungeon_state != CC_DUNGEON_RESEALED) {
        SetError(error, error_capacity, "That dungeon outcome is invalid.");
        return false;
    }
    if (command->dungeon_state != CC_DUNGEON_EXPLORED &&
        !CcSimDungeonOutcomeAvailable(dungeon, command->dungeon_state)) {
        SetError(error, error_capacity,
                 command->dungeon_state == CC_DUNGEON_PUBLIC_ROUTE ?
                 "Reach the Hoard Threshold and open the freight shortcut first." :
                 command->dungeon_state == CC_DUNGEON_SMUGGLER_ROUTE ?
                 "Reach the Hoard Threshold and search the Old Smuggler Cut first." :
                 "Reach the Hoard Threshold and study the Chain Bridge supports first.");
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
        CcNutritionAvailable(place->stock, CC_NUTRITION_ANIMAL) <
            breeding_fodder * CC_NUTRITION_PER_RATION) {
        SetError(error, error_capacity,
                 "Breeding costs 20 crowns and 2 Wheat at the stable.");
        return false;
    }
    sim->player.coins -= breeding_cost;
    place->market_coins += breeding_cost;
    (void)CcNutritionConsume(
        place->stock, CC_NUTRITION_ANIMAL,
        breeding_fodder * CC_NUTRITION_PER_RATION);
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

static bool ApplyGoblinTunnelTraversal(CcSim *sim,
                                       const CcCommand *command,
                                       char *error,
                                       size_t error_capacity)
{
    CcId origin_id = sim->player.location_id;
    bool climbing_to_dragon =
        origin_id == sim->goblins.lair_settlement_id &&
        command->target_id == sim->dragon.lair_settlement_id;
    bool returning_to_goblins =
        origin_id == sim->dragon.lair_settlement_id &&
        command->target_id == sim->goblins.lair_settlement_id;
    if (!climbing_to_dragon && !returning_to_goblins) {
        SetError(error, error_capacity,
                 "The dragon mountain is reachable only through the goblin dungeon.");
        return false;
    }
    if (sim->carriage.location_id != sim->goblins.lair_settlement_id) {
        SetError(error, error_capacity,
                 "The carriage must remain at the hidden goblin trailhead.");
        return false;
    }
    const CcSettlement *destination = CcSimSettlement(
        sim, command->target_id);
    if (destination == NULL) {
        SetError(error, error_capacity, "The tunnel destination is missing.");
        return false;
    }
    CcSimAdvanceDays(sim, 1);
    sim->player.location_id = command->target_id;
    RevealSettlementRoadAnchors(sim, sim->player.location_id);
    sim->carriage.location_id = sim->goblins.lair_settlement_id;
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(
        text, sizeof(text),
        climbing_to_dragon ?
            "The company leaves the carriage behind and spends a day crossing the goblin network to a chimney inside the dragon roost." :
            "The company descends through the goblin dungeon and returns to the hidden trailhead.");
    (void)PushEvent(sim, CC_EVENT_GOBLIN_TUNNEL_TRAVERSED,
                    sim->player.id, command->target_id, 0U, 1, text);
    SetError(error, error_capacity, "");
    return true;
}

static bool ApplyPartyWipe(CcSim *sim, const CcCommand *command,
                           char *error, size_t error_capacity)
{
    if (sim->current_day < 1 || command->target_id != (CcId)sim->current_day ||
        sim->current_day > CC_SIM_MAX_DAY - CC_PARTY_WIPE_DAYS) {
        SetError(error, error_capacity, "Refresh the world before the next generation.");
        return false;
    }
    sim->journey = (CcJourneyEncounter){0};
    sim->dungeon_expedition = (CcDungeonExpedition){
        .current_room = -1,
        .encounter_room = -1
    };
    sim->resolved_journey_situation_id = 0U;
    sim->resolved_journey_outcome = CC_JOURNEY_OUTCOME_NONE;
    sim->pony_company.encounter = -1;
    sim->carriage = (CcCarriageState){
        .mode = CC_CARRIAGE_PARKED,
        .location_id = sim->player.location_id,
        .condition = sim->carriage.condition
    };
    sim->clock.game_minutes_per_second = CC_IDLE_GAME_MINUTES_PER_SECOND;
    CcSimAdvanceDays(sim, CC_PARTY_WIPE_DAYS);
    (void)PushEvent(sim, CC_EVENT_PARTY_WIPED, sim->player.id,
                    sim->player.location_id, 0U, 20,
                    "The whole company fell. Twenty years pass, and a new company takes up the carriage.");
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
    bool party_wipe = command->kind == CC_COMMAND_PARTY_WIPE;
    if (!party_wipe && sim->schema_version >= 40U && sim->pony_company.encounter >= 0 &&
        (command->kind < CC_COMMAND_MEET_PONY || command->kind > CC_COMMAND_LEAVE_PONY)) {
        SetError(error, error_capacity, "Finish your pony visit before continuing.");
        return false;
    }
    bool dungeon_action =
        command->kind == CC_COMMAND_MOVE_DUNGEON ||
        command->kind == CC_COMMAND_SEARCH_DUNGEON ||
        command->kind == CC_COMMAND_OPEN_DUNGEON_SHORTCUT ||
        command->kind == CC_COMMAND_RESOLVE_DUNGEON_ENCOUNTER ||
        command->kind == CC_COMMAND_RETREAT_DUNGEON;
    if (sim->dungeon_expedition.active && !dungeon_action && !party_wipe) {
        SetError(error, error_capacity,
                 "Return to the carriage before taking an outside action.");
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
        command->kind == CC_COMMAND_CHARACTER_RESPONSE ||
        command->kind == CC_COMMAND_EXCHANGE_GOSSIP ||
        command->kind == CC_COMMAND_TRAVERSE_GOBLIN_TUNNEL ||
        command->kind == CC_COMMAND_BEGIN_DUNGEON_EXPEDITION;
    if (sim->journey.active && settlement_action) {
        SetError(error, error_capacity,
                 "Settlement business must wait until the carriage arrives.");
        return false;
    }
    switch (command->kind) {
        case CC_COMMAND_EXCHANGE_GOSSIP:
            return ApplyExchangeGossip(sim, command, error, error_capacity);
        case CC_COMMAND_PARTY_WIPE:
            return ApplyPartyWipe(sim, command, error, error_capacity);
        case CC_COMMAND_MEET_PONY:
        case CC_COMMAND_HELP_PONY:
        case CC_COMMAND_SWAP_PONY:
        case CC_COMMAND_LEAVE_PONY:
            return CcPoniesApply(sim, command, error, error_capacity);
        case CC_COMMAND_TRADE:
            return ApplyTrade(sim, command, error, error_capacity);
        case CC_COMMAND_TRAVEL:
            return ApplyTravel(sim, command, error, error_capacity);
        case CC_COMMAND_REPAIR_ROUTE:
            return ApplyRepair(sim, command, error, error_capacity);
        case CC_COMMAND_CHANGE_DUNGEON:
            return ApplyDungeonChange(sim, command, error, error_capacity);
        case CC_COMMAND_BEGIN_DUNGEON_EXPEDITION:
            return ApplyBeginDungeonExpedition(
                sim, command, error, error_capacity);
        case CC_COMMAND_MOVE_DUNGEON:
            return ApplyMoveDungeon(sim, command, error, error_capacity);
        case CC_COMMAND_SEARCH_DUNGEON:
            return ApplySearchDungeon(sim, error, error_capacity);
        case CC_COMMAND_OPEN_DUNGEON_SHORTCUT:
            return ApplyOpenDungeonShortcut(
                sim, command, error, error_capacity);
        case CC_COMMAND_RESOLVE_DUNGEON_ENCOUNTER:
            return ApplyResolveDungeonEncounter(
                sim, command, error, error_capacity);
        case CC_COMMAND_RETREAT_DUNGEON:
            return ApplyRetreatDungeon(sim, error, error_capacity);
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
        case CC_COMMAND_TAKE_JOURNEY_BREAK:
        case CC_COMMAND_PRESS_ON:
        case CC_COMMAND_MAKE_CAMP:
        case CC_COMMAND_LODGE_ROAD_HOUSE:
            return ApplyJourneyStopAction(
                sim, command, error, error_capacity);
        case CC_COMMAND_CAMP_ROAD_SITE:
        case CC_COMMAND_PASS_ROAD_SITE:
            return ApplyRoadSiteStop(sim, command, error, error_capacity);
        case CC_COMMAND_TRAVERSE_GOBLIN_TUNNEL:
            return ApplyGoblinTunnelTraversal(
                sim, command, error, error_capacity);
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
                    sim->dragon.hoard_event_id, text) == 0U) {
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
            sim->dragon.hoard_event_id = returned->id;
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
     CC_MAX_ROAD_SITES + CC_MAX_TREASURES + CC_MAX_FACTIONS + CC_MAX_SHIPMENTS + \
     CC_MAX_COURIERS + CC_MAX_BANDITS + CC_MAX_MONSTERS + CC_MAX_DUNGEONS + \
     CC_MAX_SITUATIONS + CC_MAX_FRONTS + CC_MAX_QUEST_OUTCOMES + \
     CC_MAX_CHARACTERS + CC_MAX_EVENTS + \
     CC_CARRIAGE_HORSE_COUNT + \
     CC_MAX_STABLE_HORSES + CC_MAX_KINGDOMS + 4)

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

static bool IsIssuedCharacterId(const CcSim *sim, CcId id)
{
    uint64_t serial = id & CC_ID_SERIAL_MASK;
    return sim != NULL && CcIdKind(id) == CC_ENTITY_CHARACTER &&
           serial > 0U && serial < sim->next_entity_serial;
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
    if (sim->schema_version >= 31U) {
        for (int32_t i = 0; i < sim->road_site_count; ++i)
            TRACK_ID(sim->road_sites[i].id, CC_ENTITY_ROAD_SITE);
    }
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
    if (sim->schema_version >= 38U) {
        if (sim->royal_trade_week != sim->current_day / 7) {
            SetError(error, error_capacity,
                     "Royal route usage week is invalid.");
            return false;
        }
        for (int32_t route = 0; route < sim->route_count; ++route) {
            if (sim->royal_route_slots_used[route] < 0 ||
                sim->royal_route_slots_used[route] > CC_SIM_MAX_UNITS) {
                SetError(error, error_capacity,
                         "Royal route usage is invalid.");
                return false;
            }
        }
        for (int32_t i = 0; i < sim->royal_carriage_count; ++i) {
            TRACK_ID(sim->royal_carriages[i].id,
                     CC_ENTITY_ROYAL_CARRIAGE);
        }
    }
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
    if (sim->schema_version >= 19U) {
        for (int32_t i = 0; i < sim->front_count; ++i)
            TRACK_ID(sim->fronts[i].id, CC_ENTITY_FRONT);
        for (int32_t i = 0; i < sim->quest_outcome_count; ++i)
            TRACK_ID(sim->quest_outcomes[i].id, CC_ENTITY_QUEST_OUTCOME);
    }
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

static bool ValidGossipVersion(const CcSim *sim, const CcGossipVersion *version,
                                bool known)
{
    if (!known) {
        return version->source_character_id == 0U && version->retellings == 0 &&
            version->court_bias == 0 && version->alarm == 0 && version->confidence == 0;
    }
    return version->retellings >= 0 && version->retellings <= 255 &&
        version->court_bias >= -100 && version->court_bias <= 100 &&
        version->alarm >= 0 && version->alarm <= 100 &&
        version->confidence >= 10 && version->confidence <= 100 &&
        (version->source_character_id == 0U ||
         (CcIdKind(version->source_character_id) == CC_ENTITY_CHARACTER &&
          (version->source_character_id & CC_ID_SERIAL_MASK) < sim->next_entity_serial));
}

bool CcSimValidate(const CcSim *sim, char *error, size_t error_capacity)
{
    if (sim == NULL) {
        SetError(error, error_capacity, "Simulation is missing.");
        return false;
    }
    bool legacy_schema = sim->schema_version == 2U ||
                         sim->schema_version == 3U ||
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
                         sim->schema_version == 17U ||
                         sim->schema_version == 18U ||
                         sim->schema_version == 19U ||
                         sim->schema_version == 20U ||
                         sim->schema_version == 21U ||
                         sim->schema_version == 22U ||
                         sim->schema_version == 23U ||
                         sim->schema_version == 24U ||
                         sim->schema_version == 25U ||
                         sim->schema_version == 26U ||
                         sim->schema_version == 27U ||
                         sim->schema_version == 28U ||
                         sim->schema_version == 29U ||
                         sim->schema_version == 30U ||
                         sim->schema_version == 31U ||
                         sim->schema_version == 32U ||
                         sim->schema_version == 33U ||
                         sim->schema_version == 34U ||
                         sim->schema_version == 35U ||
                         sim->schema_version == 36U ||
                         sim->schema_version == 37U ||
                         sim->schema_version == 38U ||
                         sim->schema_version == 39U ||
                         sim->schema_version == 40U ||
                         sim->schema_version == 41U ||
                         sim->schema_version == 42U ||
                         sim->schema_version == 43U ||
                         sim->schema_version == 44U ||
                         sim->schema_version == 45U;
    bool supported_generator =
        (sim->schema_version == CC_SIM_SCHEMA_VERSION &&
         sim->generator_version == CC_GENERATOR_VERSION) ||
        (legacy_schema && sim->schema_version <= 27U &&
         sim->generator_version == CC_GENERATOR_VERSION) ||
        (sim->schema_version == 45U && sim->generator_version == 25U) ||
        (sim->schema_version == 44U && sim->generator_version == 25U) ||
        (sim->schema_version == 43U && sim->generator_version == 25U) ||
        (sim->schema_version == 42U && sim->generator_version == 25U) ||
        (sim->schema_version == 41U && sim->generator_version == 25U) ||
        (sim->schema_version == 40U && sim->generator_version == 25U) ||
        (sim->schema_version == 39U && sim->generator_version == 25U) ||
        (sim->schema_version == 38U && sim->generator_version == 25U) ||
        (sim->schema_version == 37U && sim->generator_version == 25U) ||
        (sim->schema_version == 36U && sim->generator_version == 25U) ||
        (sim->schema_version == 35U && sim->generator_version == 25U) ||
        (sim->schema_version == 34U && sim->generator_version == 25U) ||
        (sim->schema_version == 33U && sim->generator_version == 25U) ||
        (sim->schema_version == 32U && sim->generator_version == 25U) ||
        (sim->schema_version == 31U && sim->generator_version == 24U) ||
        (sim->schema_version == 27U && sim->generator_version == 21U) ||
        (sim->schema_version == 27U && sim->generator_version == 22U) ||
        (sim->schema_version == 27U && sim->generator_version == 23U) ||
        (sim->schema_version == 28U && sim->generator_version == 22U) ||
        (sim->schema_version == 29U && sim->generator_version == 23U) ||
        (sim->schema_version == 30U && sim->generator_version == 23U) ||
        (legacy_schema && (sim->generator_version == 2U ||
                           sim->generator_version == 3U ||
                           sim->generator_version == 4U ||
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
                           sim->generator_version == 16U ||
                           sim->generator_version == 17U ||
                           sim->generator_version == 18U ||
                           sim->generator_version == 19U ||
                           sim->generator_version == 20U ||
                           sim->generator_version == 21U));
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
    if (sim->schema_version == CC_SIM_SCHEMA_VERSION &&
        (sim->archives.scribes < 0 ||
         sim->archives.scribes > CC_MAX_SCRIBES ||
         sim->archives.lore_stored < 0 ||
         sim->archives.lore_lost_total < 0 ||
         sim->archives.lore_ceiling < 0 ||
         sim->archives.lore_ceiling > 100 ||
         sim->archives.kit_tool_wear < 0 ||
         sim->archives.kit_tool_wear >= 8 ||
         CcSimCharacter(sim, sim->archives.abbot_character_id) == NULL ||
         sim->archives.stewardship_rank < 0 ||
         sim->archives.stewardship_rank > 100 ||
         sim->archives.last_recorded_day < 0 ||
         sim->archives.last_recorded_day > sim->current_day)) {
        SetError(error, error_capacity,
                 "The archive state is invalid.");
        return false;
    }
    if (sim->kingdom_count < 1 || sim->kingdom_count > CC_MAX_KINGDOMS ||
        sim->settlement_count < 1 || sim->settlement_count > CC_MAX_SETTLEMENTS ||
        sim->route_count < 0 || sim->route_count > CC_MAX_ROUTES ||
        sim->road_site_count < 0 ||
        sim->road_site_count > CC_MAX_ROAD_SITES ||
        (sim->schema_version == CC_SIM_SCHEMA_VERSION &&
         sim->road_site_count != CC_MAX_ROAD_SITES) ||
        sim->map_count < 0 || sim->map_count > CC_MAX_MAPS ||
        (sim->schema_version == CC_SIM_SCHEMA_VERSION &&
         sim->map_count != CC_MAP_COLLECTION_COUNT) ||
        sim->treasure_count < 0 || sim->treasure_count > CC_MAX_TREASURES ||
        sim->faction_count < 0 || sim->faction_count > CC_MAX_FACTIONS ||
        sim->shipment_count < 0 || sim->shipment_count > CC_MAX_SHIPMENTS ||
        sim->royal_carriage_count < 0 ||
        sim->royal_carriage_count > CC_MAX_KINGDOMS ||
        (sim->schema_version == CC_SIM_SCHEMA_VERSION &&
         sim->royal_carriage_count != sim->kingdom_count) ||
        sim->courier_count < 0 || sim->courier_count > CC_MAX_COURIERS ||
        sim->bandit_count < 0 || sim->bandit_count > CC_MAX_BANDITS ||
        sim->monster_count < 0 || sim->monster_count > CC_MAX_MONSTERS ||
        sim->dungeon_count < 0 || sim->dungeon_count > CC_MAX_DUNGEONS ||
        sim->situation_count < 0 || sim->situation_count > CC_MAX_SITUATIONS ||
        sim->front_count < 0 || sim->front_count > CC_MAX_FRONTS ||
        sim->quest_outcome_count < 0 ||
        sim->quest_outcome_count > CC_MAX_QUEST_OUTCOMES ||
        sim->pending_echo_count < 0 ||
        sim->pending_echo_count > CC_MAX_PENDING_ECHOES ||
        sim->character_count < 0 ||
        sim->character_count > CC_MAX_CHARACTERS ||
        (sim->schema_version == CC_SIM_SCHEMA_VERSION &&
         (sim->character_count != CC_MAX_CHARACTERS ||
          sim->character_births < 0 ||
          sim->character_births > CC_SIM_MAX_DAY ||
          sim->character_deaths < 0 ||
          sim->character_deaths > CC_SIM_MAX_DAY ||
          sim->character_births != sim->character_deaths)) ||
        sim->relationship_count < 0 ||
        sim->relationship_count > CC_MAX_RELATIONSHIPS ||
        sim->stable_horse_count < 0 ||
        sim->stable_horse_count > CC_MAX_STABLE_HORSES ||
        sim->event_count < 0 || sim->event_count > CC_MAX_EVENTS ||
        sim->event_write_index < 0 ||
        sim->event_write_index >= CC_MAX_EVENTS) {
        SetError(error, error_capacity, "Simulation counts are invalid.");
        return false;
    }
    if (!ValidateIdentityState(sim, error, error_capacity)) return false;
    if (sim->schema_version >= 44U) {
        uint32_t stories = 0U;
        uint32_t towns = (UINT32_C(1) << (uint32_t)sim->settlement_count) - 1U;
        if (sim->gossip_last_event_id != 0U &&
            (CcIdKind(sim->gossip_last_event_id) != CC_ENTITY_EVENT ||
             (sim->gossip_last_event_id & CC_ID_SERIAL_MASK) >= sim->next_entity_serial)) {
            SetError(error, error_capacity, "The gossip event cursor is invalid.");
            return false;
        }
        for (int32_t i = 0; i < CC_MAX_GOSSIP; ++i) {
            const CcGossip *story = &sim->gossip[i];
            for (int32_t town = 0; town < CC_MAX_SETTLEMENTS; ++town) {
                bool known = (story->settlement_mask & (UINT32_C(1) << (uint32_t)town)) != 0U;
                if (!ValidGossipVersion(sim, &story->local[town], known)) {
                    SetError(error, error_capacity, "A town's gossip version is invalid.");
                    return false;
                }
            }
            if (!ValidGossipVersion(sim, &story->heard, story->heard_day > 0)) {
                SetError(error, error_capacity, "The scribes' gossip version is invalid.");
                return false;
            }
            if (story->event_id == 0U) {
                if (story->settlement_mask != 0U || story->heard_day != 0 ||
                    story->recorded || story->heard_event_id != 0U ||
                    story->origin_id != 0U || story->day != 0 ||
                    story->text[0] != '\0' || story->heard_from[0] != '\0') {
                    SetError(error, error_capacity, "An empty gossip slot has account data.");
                    return false;
                }
                continue;
            }
            stories |= UINT32_C(1) << (uint32_t)i;
            CcEvent source = {.kind = story->kind, .magnitude = 20};
            if (!IsNotableGossip(sim, &source) || CcIdKind(story->event_id) != CC_ENTITY_EVENT ||
                story->event_id > sim->gossip_last_event_id ||
                CcSimSettlement(sim, story->origin_id) == NULL ||
                story->day < 1 || story->day > sim->current_day ||
                story->heard_day < 0 || story->heard_day > sim->current_day ||
                (story->heard_day > 0 && story->heard_day < story->day) ||
                (story->settlement_mask & ~towns) != 0U || story->settlement_mask == 0U ||
                !ValidBoundedText(story->text, sizeof(story->text)) ||
                (story->heard_day > 0 &&
                 !ValidBoundedText(story->heard_from, sizeof(story->heard_from))) ||
                (story->heard_day == 0 &&
                 (story->recorded || story->heard_event_id != 0U ||
                  story->heard_from[0] != '\0')) ||
                (story->heard_event_id != 0U && CcSimEvent(sim, story->heard_event_id) == NULL)) {
                SetError(error, error_capacity, "A gossip account is invalid.");
                return false;
            }
            for (int32_t j = 0; j < i; ++j) {
                if (sim->gossip[j].event_id == story->event_id) {
                    SetError(error, error_capacity, "A gossip account is repeated.");
                    return false;
                }
            }
        }
        for (int32_t i = 0; i < CcSimGossipCarrierCapacity(sim); ++i) {
            const CcGossipCarrier *carrier = &sim->gossip_carriers[i];
            for (int32_t slot = 0; slot < CC_MAX_GOSSIP; ++slot) {
                bool known = (carrier->stories & (UINT32_C(1) << (uint32_t)slot)) != 0U;
                if (!ValidGossipVersion(sim, &carrier->versions[slot], known)) {
                    SetError(error, error_capacity, "A traveler's gossip version is invalid.");
                    return false;
                }
            }
            CcEntityKind kind = CcIdKind(carrier->id);
            if ((carrier->stories & ~stories) != 0U ||
                (carrier->id == 0U && carrier->stories != 0U) ||
                (carrier->id != 0U &&
                 ((kind != CC_ENTITY_PLAYER_COMPANY && kind != CC_ENTITY_ROYAL_CARRIAGE &&
                   kind != CC_ENTITY_SHIPMENT && kind != CC_ENTITY_COURIER &&
                   !(sim->schema_version >= 46U && kind == CC_ENTITY_CHARACTER)) ||
                  (carrier->id & CC_ID_SERIAL_MASK) >= sim->next_entity_serial))) {
                SetError(error, error_capacity, "A gossip carrier is invalid.");
                return false;
            }
            for (int32_t j = 0; carrier->id != 0U && j < i; ++j) {
                if (sim->gossip_carriers[j].id == carrier->id) {
                    SetError(error, error_capacity, "A gossip carrier is repeated.");
                    return false;
                }
            }
        }
    }
    for (int32_t i = 0; i < sim->kingdom_count; ++i) {
        const CcKingdom *kingdom = &sim->kingdoms[i];
        const CcCharacter *ruler = CcSimCharacter(
            sim, kingdom->ruler_character_id);
        const CcCharacter *patron = CcSimCharacter(
            sim, kingdom->monastery_patron_id);
        if (sim->schema_version == CC_SIM_SCHEMA_VERSION &&
            (ruler == NULL || CharacterKingdomSlot(sim, ruler) != i ||
             patron == NULL || CharacterKingdomSlot(sim, patron) != i)) {
            if (error != NULL && error_capacity > 0U) {
                (void)snprintf(error, error_capacity,
                               "Kingdom %.48s has an invalid %s home allegiance.",
                               kingdom->name,
                               ruler == NULL || CharacterKingdomSlot(sim, ruler) != i ?
                                   "ruler" : "monastery patron");
            }
            return false;
        }
        if (CcIdKind(kingdom->id) != CC_ENTITY_KINGDOM ||
            !ValidBoundedText(kingdom->name, sizeof(kingdom->name)) ||
            kingdom->treasury < 0 ||
            kingdom->treasury > CC_SIM_MAX_MONEY ||
            kingdom->legitimacy < 0 ||
            kingdom->legitimacy > 100 ||
            (sim->schema_version == CC_SIM_SCHEMA_VERSION &&
             (kingdom->iron_ledger_debt < 0 ||
              kingdom->iron_ledger_debt > CC_SIM_MAX_MONEY ||
              kingdom->sanction < 0 || kingdom->sanction > 100 ||
              kingdom->unsanctioned_weeks < 0 ||
              kingdom->unsanctioned_weeks >= 52 ||
              kingdom->pretender_crises < 0 ||
              kingdom->pretender_crises > CC_SIM_MAX_UNITS ||
              (kingdom->anointed &&
               kingdom->anointed_by_character_id !=
                   sim->archives.abbot_character_id) ||
              (!kingdom->anointed &&
               kingdom->anointed_by_character_id != 0U)))) {
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
        for (int32_t i = 0; i < sim->event_count; ++i) {
            const CcEvent *event = CcSimRecentEvent(sim, i);
            if (event == NULL || CcIdKind(event->id) != CC_ENTITY_EVENT ||
                !ValidBoundedText(event->text, sizeof(event->text)) ||
                event->day < 1 || event->day > sim->current_day ||
                event->kind < CC_EVENT_HARVEST_FAILED ||
                event->kind > CC_EVENT_PARTY_WIPED ||
                event->parent_id == event->id ||
                (event->parent_id != 0U &&
                 CcSimEvent(sim, event->parent_id) == NULL) ||
                (event->actor_id != 0U &&
                 event->actor_id != sim->player.id &&
                 !IsIssuedCharacterId(sim, event->actor_id)) ||
                (event->target_id != 0U &&
                 event->target_id != sim->player.id &&
                 !IsIssuedCharacterId(sim, event->target_id)) ||
                (event->beneficiary_id != 0U &&
                 event->beneficiary_id != sim->player.id &&
                 !IsIssuedCharacterId(sim, event->beneficiary_id)) ||
                (event->witness_id != 0U &&
                 event->witness_id != sim->player.id &&
                 !IsIssuedCharacterId(sim, event->witness_id))) {
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
        if (sim->schema_version >= 45U &&
            (settlement->fire_damage < 0 || settlement->fire_damage > 100 ||
             settlement->last_fire_day < 0 ||
             settlement->last_fire_day > sim->current_day ||
             (settlement->fire_damage > 0 && settlement->last_fire_day == 0))) {
            SetError(error, error_capacity, "Town fire history is invalid.");
            return false;
        }
        int32_t saved_good_count = CcGoodCountForSchema(sim->schema_version);
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
                settlement->paper_tool_wear < 0 ||
                settlement->paper_tool_wear >= 8 ||
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
                settlement->sheep_adults < 0 ||
                settlement->sheep_adults > CC_SIM_MAX_UNITS ||
                settlement->sheep_lambs < 0 ||
                settlement->sheep_lambs > CC_SIM_MAX_UNITS ||
                settlement->sheep_condition < 0 ||
                settlement->sheep_condition > 100 ||
                settlement->sheep_hunger < 0 ||
                settlement->sheep_hunger > 100 ||
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
        if (sim->schema_version == CC_SIM_SCHEMA_VERSION) {
            const CcRouteKnowledge *knowledge =
                &sim->player.route_knowledge[i];
            bool overlapping_fragments =
                knowledge->from_reveal_milli +
                    knowledge->to_reveal_milli >=
                    CC_ROUTE_FULL_REVEAL_MILLI &&
                (knowledge->from_reveal_milli !=
                     CC_ROUTE_FULL_REVEAL_MILLI ||
                 knowledge->to_reveal_milli !=
                     CC_ROUTE_FULL_REVEAL_MILLI);
            if (knowledge->route_id != route->id ||
                knowledge->from_reveal_milli < 0 ||
                knowledge->from_reveal_milli >
                    CC_ROUTE_FULL_REVEAL_MILLI ||
                knowledge->to_reveal_milli < 0 ||
                knowledge->to_reveal_milli >
                    CC_ROUTE_FULL_REVEAL_MILLI ||
                overlapping_fragments ||
                knowledge->source < CC_PLAYER_KNOWLEDGE_NONE ||
                knowledge->source > CC_PLAYER_KNOWLEDGE_LEGACY ||
                (knowledge->source == CC_PLAYER_KNOWLEDGE_NONE &&
                 (knowledge->learned_day != 0 ||
                  knowledge->recorded_condition != 0 ||
                  knowledge->recorded_danger != 0)) ||
                (knowledge->source != CC_PLAYER_KNOWLEDGE_NONE &&
                 (knowledge->learned_day < 1 ||
                  knowledge->learned_day > sim->current_day ||
                  knowledge->recorded_condition < 0 ||
                  knowledge->recorded_condition > 100 ||
                  knowledge->recorded_danger < 0 ||
                  knowledge->recorded_danger > 100))) {
                SetError(error, error_capacity,
                         "Player route knowledge is invalid.");
                return false;
            }
        }
    }
    if (sim->schema_version == CC_SIM_SCHEMA_VERSION) {
        for (int32_t i = 0; i < sim->road_site_count; ++i) {
            const CcRoadSite *site = &sim->road_sites[i];
            const CcRoute *route = CcSimRoute(sim, site->route_id);
            bool valid_home = route != NULL &&
                (site->home_settlement_id == route->from_id ||
                 site->home_settlement_id == route->to_id);
            bool valid_input = CcGoodIsValid(site->input_good) ||
                site->input_good == CC_GOOD_COUNT;
            bool valid_output = CcGoodIsValid(site->output_good) ||
                site->output_good == CC_GOOD_COUNT;
            if (!ValidBoundedText(site->name, sizeof(site->name)) ||
                site->kind < CC_ROAD_SITE_FARM ||
                site->kind >= CC_ROAD_SITE_KIND_COUNT ||
                site->blocker < CC_ROAD_SITE_BLOCKER_TREE ||
                site->blocker >= CC_ROAD_SITE_BLOCKER_COUNT ||
                route == NULL || !valid_home || !valid_input || !valid_output ||
                site->progress_milli < 100 || site->progress_milli > 900 ||
                (site->side != -1 && site->side != 1) ||
                site->spur_length < 12 || site->spur_length > 48 ||
                site->condition < 0 || site->condition > 100 ||
                site->accessible) {
                SetError(error, error_capacity,
                         "Road district site data is invalid.");
                return false;
            }
        }
        for (int32_t route_slot = 0;
             route_slot < sim->route_count; ++route_slot) {
            int32_t site_count = 0;
            int32_t road_house_count = 0;
            for (int32_t site_slot = 0;
                 site_slot < sim->road_site_count; ++site_slot) {
                const CcRoadSite *site = &sim->road_sites[site_slot];
                if (site->route_id != sim->routes[route_slot].id) continue;
                site_count += 1;
                if (site->kind == CC_ROAD_SITE_ROAD_HOUSE)
                    road_house_count += 1;
            }
            if (site_count != 3 || road_house_count != 1) {
                SetError(error, error_capacity,
                         "Road district coverage is invalid.");
                return false;
            }
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
        if (sim->schema_version == CC_SIM_SCHEMA_VERSION &&
            (KingdomSlotById(sim, map->recorded_from_kingdom_id) < 0 ||
             KingdomSlotById(sim, map->recorded_to_kingdom_id) < 0)) {
            SetError(error, error_capacity,
                     "Physical map knowledge is invalid.");
            return false;
        }
    }
    if (sim->schema_version == CC_SIM_SCHEMA_VERSION) {
        uint32_t known_sites =
            (UINT32_C(1) << CC_ROAD_BOOK_SITE_COUNT) - 1U;
        if ((sim->player.road_book_site_discovery_mask & ~known_sites) != 0U) {
            SetError(error, error_capacity,
                     "Road-book site knowledge is invalid.");
            return false;
        }
        for (int32_t i = 0; i < sim->settlement_count; ++i) {
            const CcSettlementKnowledge *knowledge =
                &sim->player.settlement_knowledge[i];
            bool unknown = knowledge->source == CC_PLAYER_KNOWLEDGE_NONE;
            if (knowledge->settlement_id != sim->settlements[i].id ||
                knowledge->source < CC_PLAYER_KNOWLEDGE_NONE ||
                knowledge->source > CC_PLAYER_KNOWLEDGE_LEGACY ||
                (unknown && (knowledge->kingdom_id != 0U ||
                             knowledge->learned_day != 0)) ||
                (!unknown &&
                 (KingdomSlotById(sim, knowledge->kingdom_id) < 0 ||
                  knowledge->learned_day < -CC_SIM_MAX_DAY ||
                  knowledge->learned_day > sim->current_day))) {
                SetError(error, error_capacity,
                         "Settlement knowledge is invalid.");
                return false;
            }
        }
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
            CcSimCharacter(sim, treasure->owner_id) != NULL ||
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
    if (sim->schema_version == CC_SIM_SCHEMA_VERSION &&
        sim->archives.lore_stored != CcSimArchivePhysicalLore(sim)) {
        SetError(error, error_capacity,
                 "Archive lore and physical volumes disagree.");
        return false;
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
            FreightCargoSlots(shipment->good, shipment->quantity) <=
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
            shipment->status >
                (sim->schema_version >= 38U ?
                 CC_SHIPMENT_BLOCKED : CC_SHIPMENT_LOST) ||
            !capacity_valid) {
            SetError(error, error_capacity, "Shipment data is invalid.");
            return false;
        }
    }
    if (sim->schema_version >= 38U) {
        for (int32_t i = 0; i < sim->royal_carriage_count; ++i) {
            const CcRoyalCarriage *carriage = &sim->royal_carriages[i];
            const CcSettlement *location = CcSimSettlement(
                sim, carriage->location_id);
            const CcSettlement *destination = CcSimSettlement(
                sim, carriage->destination_id);
            const CcSettlement *target = CcSimSettlement(
                sim, carriage->target_id);
            const CcRoute *route = CcSimRoute(sim, carriage->route_id);
            const CcShipment *shipment = NULL;
            for (int32_t slot = 0; slot < sim->shipment_count; ++slot) {
                if (sim->shipments[slot].id ==
                    carriage->active_shipment_id) {
                    shipment = &sim->shipments[slot];
                    break;
                }
            }
            bool idle = carriage->mode == CC_ROYAL_CARRIAGE_IDLE;
            bool repositioning =
                carriage->mode == CC_ROYAL_CARRIAGE_REPOSITIONING;
            bool delivering =
                carriage->mode == CC_ROYAL_CARRIAGE_DELIVERING;
            bool blocked = carriage->mode == CC_ROYAL_CARRIAGE_BLOCKED;
            bool waiting_capacity = carriage->mode ==
                CC_ROYAL_CARRIAGE_WAITING_CAPACITY;
            bool route_connects = route != NULL && destination != NULL &&
                ((route->from_id == carriage->location_id &&
                  route->to_id == carriage->destination_id) ||
                 (route->to_id == carriage->location_id &&
                  route->from_id == carriage->destination_id));
            bool trip_timing = carriage->departure_day >= 1 &&
                carriage->departure_day <= sim->current_day &&
                carriage->arrival_day > sim->current_day && route != NULL &&
                (int64_t)carriage->arrival_day ==
                    (int64_t)carriage->departure_day + route->travel_days;
            bool mode_valid =
                (idle && carriage->active_shipment_id == 0U &&
                 carriage->route_id == 0U &&
                 carriage->destination_id == 0U &&
                 carriage->target_id == 0U &&
                 carriage->arrival_day == 0 &&
                 carriage->blocked_since_day == 0) ||
                (repositioning && carriage->active_shipment_id == 0U &&
                 route_connects && target != NULL && trip_timing &&
                 carriage->blocked_since_day == 0) ||
                (delivering && shipment != NULL &&
                 shipment->status == CC_SHIPMENT_TRAVELLING &&
                 shipment->origin_id == carriage->location_id &&
                 shipment->route_id == carriage->route_id &&
                 shipment->destination_id == carriage->destination_id &&
                 shipment->final_destination_id == carriage->target_id &&
                 route_connects && trip_timing &&
                 carriage->blocked_since_day == 0) ||
                (blocked && target != NULL && route_connects &&
                 carriage->arrival_day == 0 &&
                 carriage->blocked_since_day >= 1 &&
                 carriage->blocked_since_day <= sim->current_day &&
                 ((carriage->active_shipment_id == 0U &&
                   shipment == NULL) ||
                  (shipment != NULL &&
                   shipment->status == CC_SHIPMENT_BLOCKED &&
                   shipment->final_destination_id ==
                       carriage->target_id))) ||
                (waiting_capacity && target != NULL && route_connects &&
                 carriage->arrival_day == 0 &&
                 carriage->blocked_since_day == 0 && shipment != NULL &&
                 shipment->status == CC_SHIPMENT_BLOCKED &&
                 shipment->final_destination_id == carriage->target_id);
            if (CcIdKind(carriage->id) != CC_ENTITY_ROYAL_CARRIAGE ||
                carriage->kingdom_id != sim->kingdoms[i].id ||
                location == NULL ||
                carriage->mode < CC_ROYAL_CARRIAGE_IDLE ||
                carriage->mode > CC_ROYAL_CARRIAGE_WAITING_CAPACITY ||
                carriage->condition < 0 || carriage->condition > 100 ||
                carriage->trips_completed < 0 ||
                carriage->trips_completed > CC_SIM_MAX_UNITS ||
                carriage->cargo_losses < 0 ||
                carriage->cargo_losses > CC_SIM_MAX_UNITS ||
                carriage->departure_day < 0 ||
                carriage->departure_day > sim->current_day ||
                carriage->next_dispatch_day < 0 ||
                carriage->next_dispatch_day > sim->current_day + 7 ||
                (shipment != NULL &&
                 (CcSimSettlement(sim,
                     shipment->final_destination_id)->kingdom_id !=
                      carriage->kingdom_id ||
                  FreightCargoSlots(shipment->good,
                                    shipment->quantity) >
                      CC_ROYAL_CARRIAGE_CARGO_SLOTS)) ||
                !mode_valid) {
                if (error != NULL && error_capacity > 0U) {
                    (void)snprintf(
                        error, error_capacity,
                        "Royal carriage %d state is invalid"
                        " (mode=%d route=%d timing=%d link=%d).",
                        i, (int32_t)carriage->mode,
                        route_connects ? 1 : 0, trip_timing ? 1 : 0,
                        shipment != NULL ? 1 : 0);
                }
                return false;
            }
        }
        for (int32_t shipment_slot = 0;
             shipment_slot < sim->shipment_count; ++shipment_slot) {
            const CcShipment *shipment = &sim->shipments[shipment_slot];
            if (shipment->status != CC_SHIPMENT_TRAVELLING &&
                shipment->status != CC_SHIPMENT_BLOCKED) continue;
            int32_t links = 0;
            for (int32_t carriage_slot = 0;
                 carriage_slot < sim->royal_carriage_count;
                 ++carriage_slot) {
                if (sim->royal_carriages[carriage_slot].active_shipment_id ==
                    shipment->id) links += 1;
            }
            if (links != 1) {
                SetError(error, error_capacity,
                         "Active shipment carriage link is invalid.");
                return false;
            }
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
        if (sim->schema_version == CC_SIM_SCHEMA_VERSION) {
            uint32_t known_room_flags =
                CC_DUNGEON_ROOM_SAFE | CC_DUNGEON_ROOM_HAZARD |
                CC_DUNGEON_ROOM_STONEBACK | CC_DUNGEON_ROOM_GOBLIN |
                CC_DUNGEON_ROOM_DRAGON_SIGN | CC_DUNGEON_ROOM_OBJECTIVE |
                CC_DUNGEON_ROOM_SMUGGLER;
            uint32_t known_room_state =
                CC_DUNGEON_ROOM_DISCOVERED | CC_DUNGEON_ROOM_SEARCHED |
                CC_DUNGEON_ROOM_CLEARED |
                CC_DUNGEON_ROOM_OBJECTIVE_REACHED;
            uint32_t known_link_flags =
                CC_DUNGEON_LINK_DISCOVERED | CC_DUNGEON_LINK_OPEN;
            if (dungeon->layout_seed == 0U ||
                dungeon->encounter_random_state == 0U ||
                dungeon->room_count < 1 ||
                dungeon->room_count > CC_MAX_DUNGEON_ROOMS ||
                dungeon->link_count < 0 ||
                dungeon->link_count > CC_MAX_DUNGEON_LINKS) {
                SetError(error, error_capacity,
                         "Dungeon layout state is invalid.");
                return false;
            }
            for (int32_t room = 0; room < dungeon->room_count; ++room) {
                const CcDungeonRoom *item = &dungeon->rooms[room];
                if (!ValidBoundedText(item->name, sizeof(item->name)) ||
                    item->kind < CC_DUNGEON_ROOM_MINE_MOUTH ||
                    item->kind > CC_DUNGEON_ROOM_THRESHOLD ||
                    item->depth < 0 || item->depth > dungeon->depth ||
                    item->map_x < -100 || item->map_x > 100 ||
                    item->map_y < -100 || item->map_y > 100 ||
                    (item->flags & ~known_room_flags) != 0U ||
                    (item->state_flags & ~known_room_state) != 0U ||
                    item->loot_good < 0 || item->loot_good >= CC_GOOD_COUNT ||
                    item->loot_quantity < 0 ||
                    item->loot_quantity > CC_SIM_MAX_UNITS) {
                    SetError(error, error_capacity,
                             "Dungeon room state is invalid.");
                    return false;
                }
            }
            for (int32_t link = 0; link < dungeon->link_count; ++link) {
                const CcDungeonLink *item = &dungeon->links[link];
                if (item->from_room < 0 ||
                    item->from_room >= dungeon->room_count ||
                    item->to_room < 0 ||
                    item->to_room >= dungeon->room_count ||
                    item->from_room == item->to_room ||
                    item->kind < CC_DUNGEON_LINK_PASSAGE ||
                    item->kind > CC_DUNGEON_LINK_DROP ||
                    (item->flags & ~known_link_flags) != 0U) {
                    SetError(error, error_capacity,
                             "Dungeon passage state is invalid.");
                    return false;
                }
            }
        }
        for (int32_t earlier = 0; earlier < i; ++earlier) {
            if (sim->dungeons[earlier].id == dungeon->id) {
                SetError(error, error_capacity,
                         "Dungeon identities are not unique.");
                return false;
            }
        }
    }
    if (sim->schema_version == CC_SIM_SCHEMA_VERSION) {
        const CcDungeonExpedition *expedition = &sim->dungeon_expedition;
        const CcDungeon *dungeon = CcSimDungeon(sim, expedition->dungeon_id);
        bool idle_valid = !expedition->active &&
            expedition->dungeon_id == 0U &&
            expedition->current_room == -1 &&
            expedition->turns_elapsed == 0 &&
            expedition->days_elapsed == 0 &&
            expedition->light_remaining == 0 &&
            expedition->noise == 0 &&
            expedition->strain == 0 &&
            expedition->maximum_depth == 0 &&
            expedition->encounter_kind == CC_DUNGEON_ENCOUNTER_NONE &&
            expedition->encounter_reaction == 0 &&
            expedition->encounter_room == -1;
        bool encounter_valid = expedition->encounter_kind ==
                CC_DUNGEON_ENCOUNTER_NONE ?
            expedition->encounter_reaction == 0 &&
                expedition->encounter_room == -1 :
            expedition->encounter_kind >= CC_DUNGEON_ENCOUNTER_STONEBACKS &&
                expedition->encounter_kind <=
                    CC_DUNGEON_ENCOUNTER_SMUGGLERS &&
                expedition->encounter_reaction >= 2 &&
                expedition->encounter_reaction <= 12 &&
                expedition->encounter_room == expedition->current_room;
        bool active_valid = expedition->active && dungeon != NULL &&
            expedition->current_room >= 0 &&
            expedition->current_room < dungeon->room_count &&
            expedition->turns_elapsed >= 0 &&
            expedition->days_elapsed >= 0 &&
            expedition->light_remaining >= 0 &&
            expedition->light_remaining <= 18 &&
            expedition->noise >= 0 && expedition->noise <= 10 &&
            expedition->strain >= 0 && expedition->strain <= 100 &&
            expedition->maximum_depth >= 0 &&
            expedition->maximum_depth <= dungeon->depth &&
            encounter_valid;
        if (!idle_valid && !active_valid) {
            SetError(error, error_capacity,
                     "Dungeon expedition state is invalid.");
            return false;
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
            dragon->hair_color < CC_DRAGON_HAIR_PURPLE ||
            dragon->hair_color >= CC_DRAGON_HAIR_COLOR_COUNT ||
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
            dragon->territoryless_days < 0 ||
            dragon->territoryless_days > CC_SIM_MAX_DAY ||
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
            (campaign->patron_character_id != 0U &&
             CcSimCharacter(sim, campaign->patron_character_id) == NULL) ||
            (campaign->hero_character_id != 0U &&
             CcSimCharacter(sim, campaign->hero_character_id) == NULL) ||
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
                (sim->schema_version == CC_SIM_SCHEMA_VERSION &&
                 ((character->ancestor_id != 0U &&
                   (!IsIssuedCharacterId(sim, character->ancestor_id) ||
                    character->ancestor_id == character->id)) ||
                  character->birth_day < -CC_SIM_MAX_DAY ||
                  character->birth_day > sim->current_day ||
                  character->death_day <= character->birth_day ||
                  character->death_day > CC_SIM_MAX_DAY ||
                  character->generation < 0 ||
                  character->generation > CC_SIM_MAX_UNITS ||
                  (character->generation == 0 &&
                   character->ancestor_id != 0U) ||
                  (character->generation > 0 &&
                   character->ancestor_id == 0U))) ||
                character->memory_count < 0 ||
                character->memory_count > CC_CHARACTER_MEMORY_CAPACITY ||
                character->memory_write_index < 0 ||
                character->memory_write_index >=
                    CC_CHARACTER_MEMORY_CAPACITY ||
                (sim->schema_version == CC_SIM_SCHEMA_VERSION &&
                 (character->knowledge_count < 0 ||
                  character->knowledge_count >
                      CC_CHARACTER_KNOWLEDGE_CAPACITY ||
                  character->knowledge_write_index < 0 ||
                  character->knowledge_write_index >=
                      CC_CHARACTER_KNOWLEDGE_CAPACITY))) {
                SetError(error, error_capacity,
                         "Character state is invalid.");
                return false;
            }
            if (sim->schema_version == CC_SIM_SCHEMA_VERSION) {
                for (int32_t earlier = 0; earlier < i; ++earlier) {
                    if (strcmp(sim->characters[earlier].name,
                               character->name) == 0) {
                        SetError(error, error_capacity,
                                 "Living character names are duplicated.");
                        return false;
                    }
                }
            }
            for (int32_t memory = 0;
                 memory < character->memory_count; ++memory) {
                const CcCharacterMemory *item =
                    &character->memories[memory];
                bool subject_exists =
                    CcSimSituation(sim, item->subject_id) != NULL ||
                    (sim->schema_version >= 19U &&
                     CcSimQuestOutcome(sim, item->subject_id) != NULL);
                if (item->kind <= CC_CHARACTER_MEMORY_NONE ||
                    item->kind > CC_CHARACTER_MEMORY_PLAYER_WITHDREW ||
                    !subject_exists ||
                    CcSimEvent(sim, item->event_id) == NULL ||
                    item->day < 1 || item->day > sim->current_day) {
                    SetError(error, error_capacity,
                             "Character memory is invalid.");
                    return false;
                }
            }
            if (sim->schema_version == CC_SIM_SCHEMA_VERSION) {
                for (int32_t knowledge = 0;
                     knowledge < character->knowledge_count; ++knowledge) {
                    const CcCharacterKnowledge *item =
                        &character->knowledge[knowledge];
                    bool source_exists = item->source_character_id ==
                            sim->player.id ||
                        CcSimCharacter(sim,
                                       item->source_character_id) != NULL;
                    if (item->kind <= CC_KNOWLEDGE_NONE ||
                        item->kind > CC_KNOWLEDGE_OFFER ||
                        CcSimSituation(sim, item->subject_id) == NULL ||
                        !source_exists ||
                        CcSimEvent(sim, item->event_id) == NULL ||
                        item->certainty < CC_KNOWLEDGE_DOUBTFUL ||
                        item->certainty > CC_KNOWLEDGE_WITNESSED ||
                        item->day < 1 || item->day > sim->current_day) {
                        SetError(error, error_capacity,
                                 "Character knowledge is invalid.");
                        return false;
                    }
                }
            }
        }
    }
    if (sim->schema_version == CC_SIM_SCHEMA_VERSION) {
        for (int32_t i = 0; i < sim->relationship_count; ++i) {
            const CcRelationship *relationship = &sim->relationships[i];
            if (relationship->from_character_id ==
                    relationship->to_character_id ||
                CcSimCharacter(sim,
                               relationship->from_character_id) == NULL ||
                CcSimCharacter(sim,
                               relationship->to_character_id) == NULL ||
                relationship->affinity < -3 || relationship->affinity > 3 ||
                relationship->trust < -3 || relationship->trust > 3 ||
                relationship->obligation < -3 ||
                relationship->obligation > 3 ||
                relationship->history <= CC_RELATIONSHIP_HISTORY_NONE ||
                relationship->history > CC_RELATIONSHIP_HISTORY_COWORKERS ||
                CcSimEvent(sim, relationship->cause_event_id) == NULL) {
                SetError(error, error_capacity,
                         "Character relationship is invalid.");
                return false;
            }
            for (int32_t earlier = 0; earlier < i; ++earlier) {
                if (sim->relationships[earlier].from_character_id ==
                        relationship->from_character_id &&
                    sim->relationships[earlier].to_character_id ==
                        relationship->to_character_id) {
                    SetError(error, error_capacity,
                             "Character relationships are duplicated.");
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
        const CcCharacter *sponsor_character = CcSimCharacter(
            sim, situation->sponsor_character_id);
        const CcCharacter *affected_character = CcSimCharacter(
            sim, situation->affected_character_id);
        bool cast_valid = true;
        if (sim->schema_version >= 17U) {
            bool historical_cast =
                sim->schema_version == CC_SIM_SCHEMA_VERSION &&
                situation->status != CC_SITUATION_ACTIVE;
            cast_valid = historical_cast ?
                IsIssuedCharacterId(
                    sim, situation->sponsor_character_id) &&
                IsIssuedCharacterId(
                    sim, situation->affected_character_id) :
                sponsor_character != NULL && affected_character != NULL &&
                strcmp(sponsor_character->name,
                       situation->sponsor_name) == 0 &&
                strcmp(affected_character->name,
                       situation->affected_name) == 0;
        }
        bool quest_valid = true;
        bool quest_schema = sim->schema_version == CC_SIM_SCHEMA_VERSION ||
            (sim->schema_version == 19U &&
             (sim->front_count > 0 || sim->quest_outcome_count > 0));
        if (quest_schema) {
            const CcQuestObjective *objective = &situation->objective;
            const CcFront *front = CcSimSituationFront(sim, situation);
            const CcQuestOutcomeRecord *archived_outcome =
                CcSimQuestOutcome(sim, situation->id);
            int32_t required = situation->quantity > 0 ?
                               situation->quantity : 1;
            int32_t danger_limit = MaximumI32(
                1, situation->deadline_day - situation->created_day + 1);
            bool front_contains_situation = false;
            if (front != NULL) {
                for (int32_t member = 0;
                     member < front->situation_count; ++member) {
                    if (front->situation_ids[member] == situation->id) {
                        front_contains_situation = true;
                    }
                }
            }
            bool front_history_exists = front_contains_situation ||
                (situation->status != CC_SITUATION_ACTIVE &&
                 archived_outcome != NULL &&
                 archived_outcome->front_id == situation->front_id);
            bool ending_matches =
                (situation->status == CC_SITUATION_ACTIVE &&
                 situation->end_reason == CC_QUEST_END_NONE) ||
                (situation->status == CC_SITUATION_RESOLVED &&
                 situation->end_reason == CC_QUEST_END_COMPLETED) ||
                (situation->status == CC_SITUATION_FAILED &&
                 situation->end_reason > CC_QUEST_END_COMPLETED &&
                 situation->end_reason <= CC_QUEST_END_COURIER_LOST);
            quest_valid = front_history_exists &&
                objective->kind ==
                    CcQuestObjectiveForSituationKind(situation->kind) &&
                objective->target_id == situation->target_id &&
                objective->good == situation->good &&
                objective->required == required &&
                objective->progress.limit == required &&
                objective->progress.value >= 0 &&
                objective->progress.value <= required &&
                objective->danger.limit == danger_limit &&
                objective->danger.value >= 0 &&
                objective->danger.value <= danger_limit &&
                objective->evidence_count >= 0 &&
                objective->evidence_count <= CC_MAX_QUEST_EVIDENCE &&
                ending_matches;
            for (int32_t evidence = 0;
                 quest_valid && evidence < objective->evidence_count;
                 ++evidence) {
                if (CcSimEvent(
                        sim, objective->evidence_event_ids[evidence]) == NULL) {
                    quest_valid = false;
                }
                for (int32_t earlier = 0; earlier < evidence; ++earlier) {
                    if (objective->evidence_event_ids[earlier] ==
                        objective->evidence_event_ids[evidence]) {
                        quest_valid = false;
                    }
                }
            }
            if (objective->progress.created_by_event_id != 0U &&
                CcSimEvent(sim,
                    objective->progress.created_by_event_id) == NULL) {
                quest_valid = false;
            }
            if (objective->progress.resolved_by_event_id != 0U &&
                CcSimEvent(sim,
                    objective->progress.resolved_by_event_id) == NULL) {
                quest_valid = false;
            }
            if (objective->danger.created_by_event_id != 0U &&
                CcSimEvent(sim,
                    objective->danger.created_by_event_id) == NULL) {
                quest_valid = false;
            }
            if (objective->danger.resolved_by_event_id != 0U &&
                CcSimEvent(sim,
                    objective->danger.resolved_by_event_id) == NULL) {
                quest_valid = false;
            }
        }
        bool social_thread_valid = true;
        if (sim->schema_version == CC_SIM_SCHEMA_VERSION) {
            if (situation->kind == CC_SITUATION_MONSTER_EXPEDITION &&
                situation->status == CC_SITUATION_ACTIVE) {
                bool undecided_stage =
                    situation->discovery_stage == CC_DISCOVERY_RUMOR ||
                    situation->discovery_stage == CC_DISCOVERY_WITNESS ||
                    situation->discovery_stage == CC_DISCOVERY_DECISION;
                social_thread_valid =
                    CcSimCharacter(sim,
                                   situation->witness_character_id) != NULL &&
                    situation->discovery_stage >= CC_DISCOVERY_OFFER &&
                    situation->discovery_stage <= CC_DISCOVERY_AUTHORITY &&
                    situation->lead_path >= CC_LEAD_PATH_UNDECIDED &&
                    situation->lead_path <= CC_LEAD_PATH_CONFIDENCE &&
                    CcSimEvent(sim, situation->lead_event_id) != NULL &&
                    (!undecided_stage ||
                     situation->lead_path == CC_LEAD_PATH_UNDECIDED) &&
                    (situation->discovery_stage != CC_DISCOVERY_AUTHORITY ||
                     situation->lead_path == CC_LEAD_PATH_REPORT) &&
                    (situation->lead_path != CC_LEAD_PATH_CONFIDENCE ||
                     situation->discovery_stage == CC_DISCOVERY_OFFER);
            } else if (situation->kind ==
                       CC_SITUATION_MONSTER_EXPEDITION) {
                social_thread_valid =
                    IsIssuedCharacterId(
                        sim, situation->witness_character_id) &&
                    situation->discovery_stage >= CC_DISCOVERY_OFFER &&
                    situation->discovery_stage <= CC_DISCOVERY_AUTHORITY &&
                    situation->lead_path >= CC_LEAD_PATH_UNDECIDED &&
                    situation->lead_path <= CC_LEAD_PATH_CONFIDENCE &&
                    CcSimEvent(sim, situation->lead_event_id) != NULL;
            } else {
                social_thread_valid =
                    situation->witness_character_id == 0U &&
                    situation->discovery_stage == CC_DISCOVERY_OFFER &&
                    situation->lead_path == CC_LEAD_PATH_UNDECIDED &&
                    situation->lead_event_id == 0U;
            }
        }
        if (CcIdKind(situation->id) != CC_ENTITY_SITUATION ||
            !ValidOptionalBoundedText(situation->sponsor_name,
                                      sizeof(situation->sponsor_name)) ||
            !ValidOptionalBoundedText(situation->affected_name,
                                      sizeof(situation->affected_name)) ||
            !cast_valid ||
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
            !quest_valid ||
            !social_thread_valid ||
            (sim->schema_version == CC_SIM_SCHEMA_VERSION &&
             situation->cause_event_id != 0U &&
             CcSimEvent(sim, situation->cause_event_id) == NULL)) {
            SetError(error, error_capacity, "Situation data is invalid.");
            return false;
        }
    }
    bool quest_schema = sim->schema_version == CC_SIM_SCHEMA_VERSION ||
        (sim->schema_version == 19U &&
         (sim->front_count > 0 || sim->quest_outcome_count > 0));
    if (quest_schema) {
        for (int32_t i = 0; i < sim->front_count; ++i) {
            const CcFront *front = &sim->fronts[i];
            bool outcome_matches =
                (front->status == CC_FRONT_ACTIVE &&
                 front->outcome == CC_FRONT_OUTCOME_NONE &&
                 front->resolved_day == 0) ||
                (front->status == CC_FRONT_RESOLVED &&
                 front->outcome > CC_FRONT_OUTCOME_NONE &&
                 front->outcome < CC_FRONT_OUTCOME_PRESSURE_WON &&
                 front->resolved_day >= front->created_day) ||
                ((front->status == CC_FRONT_FAILED ||
                  front->status == CC_FRONT_INVALIDATED) &&
                 front->outcome == CC_FRONT_OUTCOME_PRESSURE_WON &&
                 front->resolved_day >= front->created_day);
            bool members_valid = front->situation_count > 0 &&
                front->situation_count <= CC_MAX_FRONT_SITUATIONS;
            for (int32_t member = 0;
                 members_valid && member < front->situation_count;
                 ++member) {
                CcId situation_id = front->situation_ids[member];
                const CcSituation *situation = CcSimSituation(
                    sim, situation_id);
                if (CcIdKind(situation_id) != CC_ENTITY_SITUATION ||
                    (situation != NULL && situation->front_id != front->id)) {
                    members_valid = false;
                }
                for (int32_t earlier = 0; earlier < member; ++earlier) {
                    if (front->situation_ids[earlier] == situation_id) {
                        members_valid = false;
                    }
                }
            }
            bool event_refs_valid =
                (front->cause_event_id == 0U ||
                 CcSimEvent(sim, front->cause_event_id) != NULL) &&
                (front->created_event_id == 0U ||
                 CcSimEvent(sim, front->created_event_id) != NULL) &&
                (front->resolved_event_id == 0U ||
                 CcSimEvent(sim, front->resolved_event_id) != NULL) &&
                (front->portent.created_by_event_id == 0U ||
                 CcSimEvent(sim,
                    front->portent.created_by_event_id) != NULL) &&
                (front->portent.resolved_by_event_id == 0U ||
                 CcSimEvent(sim,
                    front->portent.resolved_by_event_id) != NULL);
            if (CcIdKind(front->id) != CC_ENTITY_FRONT ||
                front->kind < CC_FRONT_SUPPLY_CRISIS ||
                front->kind > CC_FRONT_COURIER_DISPATCH ||
                front->status < CC_FRONT_ACTIVE ||
                front->status > CC_FRONT_INVALIDATED ||
                !outcome_matches || front->anchor_id == 0U ||
                front->created_day < 1 ||
                front->created_day > sim->current_day ||
                front->portent.limit < 1 ||
                front->portent.value < 0 ||
                front->portent.value > front->portent.limit ||
                !ValidBoundedText(front->premise,
                                  sizeof(front->premise)) ||
                !members_valid || !event_refs_valid ||
                (front->status == CC_FRONT_ACTIVE &&
                 !FrontHasActiveSituation(sim, front))) {
                SetError(error, error_capacity, "Story front data is invalid.");
                return false;
            }
        }
        for (int32_t i = 0; i < sim->quest_outcome_count; ++i) {
            const CcQuestOutcomeRecord *outcome = &sim->quest_outcomes[i];
            bool events_valid =
                (outcome->cause_event_id == 0U ||
                 CcSimEvent(sim, outcome->cause_event_id) != NULL) &&
                (outcome->resolved_event_id == 0U ||
                 CcSimEvent(sim, outcome->resolved_event_id) != NULL);
            if (CcIdKind(outcome->id) != CC_ENTITY_QUEST_OUTCOME ||
                CcIdKind(outcome->situation_id) != CC_ENTITY_SITUATION ||
                CcIdKind(outcome->front_id) != CC_ENTITY_FRONT ||
                outcome->situation_kind < CC_SITUATION_RELIEF_DELIVERY ||
                outcome->situation_kind > CC_SITUATION_COURIER_DELIVERY ||
                outcome->front_kind < CC_FRONT_SUPPLY_CRISIS ||
                outcome->front_kind > CC_FRONT_COURIER_DISPATCH ||
                outcome->situation_status <= CC_SITUATION_ACTIVE ||
                outcome->situation_status > CC_SITUATION_FAILED ||
                outcome->end_reason <= CC_QUEST_END_NONE ||
                outcome->end_reason > CC_QUEST_END_COURIER_LOST ||
                CcIdKind(outcome->target_id) == CC_ENTITY_NONE ||
                !IsIssuedCharacterId(
                    sim, outcome->sponsor_character_id) ||
                !IsIssuedCharacterId(
                    sim, outcome->affected_character_id) ||
                outcome->resolved_day < 1 ||
                outcome->resolved_day > sim->current_day ||
                outcome->progress_limit < 1 ||
                outcome->progress_value < 0 ||
                outcome->progress_value > outcome->progress_limit ||
                outcome->danger_limit < 1 ||
                outcome->danger_value < 0 ||
                outcome->danger_value > outcome->danger_limit ||
                !events_valid) {
                SetError(error, error_capacity,
                         "Permanent quest outcome is invalid.");
                return false;
            }
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
    if (sim->schema_version <= 3U) {
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
        if (sim->schema_version >= 39U &&
            (sim->journey.road_site_stop_mask >> CC_MAX_ROAD_SITES) != 0U) {
            SetError(error, error_capacity, "Roadside stop history is invalid.");
            return false;
        }
        if (sim->journey.active) {
            bool situation_valid = sim->journey.situation_id == 0U ||
                CcSimSituation(sim, sim->journey.situation_id) != NULL;
            bool phase_valid =
                sim->journey.phase == CC_JOURNEY_PHASE_TRAVELLING ||
                sim->journey.phase == CC_JOURNEY_PHASE_BLOCKED ||
                sim->journey.phase == CC_JOURNEY_PHASE_RESTING;
            bool encounter_valid =
                sim->journey.phase != CC_JOURNEY_PHASE_BLOCKED ||
                sim->journey.encounter_triggered;
            bool stop_valid =
                sim->journey.phase != CC_JOURNEY_PHASE_RESTING ||
                CcSimJourneyStop(sim) != CC_JOURNEY_STOP_NONE;
            int32_t expected_rate =
                sim->journey.phase == CC_JOURNEY_PHASE_TRAVELLING ?
                    CC_TRAVEL_GAME_MINUTES_PER_SECOND :
                    CC_IDLE_GAME_MINUTES_PER_SECOND;
            if (!situation_valid || !phase_valid || !encounter_valid ||
                !stop_valid ||
                CcSimSettlement(sim, sim->journey.origin_id) == NULL ||
                CcSimSettlement(sim, sim->journey.destination_id) == NULL ||
                CcSimRoute(sim, sim->journey.route_id) == NULL ||
                sim->journey.origin_id != sim->player.location_id ||
                sim->journey.danger < 0 || sim->journey.danger > 100 ||
                sim->journey.bargain_cost < 1 ||
                sim->journey.departure_day < 1 ||
                sim->journey.departure_day > sim->current_day ||
                sim->journey.elapsed_subticks < 0 ||
                sim->journey.total_subticks < CC_WORLD_WATCH_SUBTICKS ||
                sim->journey.total_subticks >
                    CC_SIM_MAX_ROUTE_DAYS * 2 * CC_WORLD_WATCH_SUBTICKS ||
                sim->journey.elapsed_subticks >
                    sim->journey.total_subticks ||
                sim->journey.encounter_subticks < 0 ||
                sim->journey.encounter_subticks >
                    sim->journey.total_subticks ||
                sim->journey.fare_reserved < 0 ||
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
                sim->journey.phase == CC_JOURNEY_PHASE_TRAVELLING ?
                    CC_CARRIAGE_MOVING : CC_CARRIAGE_STOPPED;
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
        } else {
            bool carriage_left_below_dragon =
                sim->player.location_id == sim->dragon.lair_settlement_id &&
                sim->carriage.location_id ==
                    sim->goblins.lair_settlement_id;
            if (sim->journey.phase != CC_JOURNEY_PHASE_NONE ||
                sim->carriage.mode != CC_CARRIAGE_PARKED ||
                (sim->carriage.location_id != sim->player.location_id &&
                 !carriage_left_below_dragon) ||
                   sim->clock.game_minutes_per_second !=
                       CC_IDLE_GAME_MINUTES_PER_SECOND ||
                   sim->carriage.condition < 0 ||
                sim->carriage.condition > 100) {
                SetError(error, error_capacity,
                         "Parked carriage state is invalid.");
                return false;
            }
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
    bool echo_subject_exists =
        CcSimSituation(sim, sim->delayed_echo.situation_id) != NULL ||
        CcSimQuestOutcome(sim, sim->delayed_echo.situation_id) != NULL;
    bool active_echo_valid = !sim->delayed_echo.active ||
        (echo_subject_exists &&
         CcSimSettlement(sim, sim->delayed_echo.settlement_id) != NULL &&
         sim->delayed_echo.outcome > CC_JOURNEY_OUTCOME_NONE &&
         sim->delayed_echo.outcome <= CC_JOURNEY_OUTCOME_NEGOTIATED &&
         sim->delayed_echo.due_day >= 0 &&
         (sim->delayed_echo.parent_event_id == 0U ||
          CcSimEvent(sim, sim->delayed_echo.parent_event_id) != NULL) &&
         sim->delayed_echo.character_name[0] != '\0');
    if (!ValidOptionalBoundedText(sim->delayed_echo.character_name,
                                  sizeof(sim->delayed_echo.character_name)) ||
        !active_echo_valid) {
        SetError(error, error_capacity, "Delayed journey echo is invalid.");
        return false;
    }
    for (int32_t i = 0; i < sim->pending_echo_count; ++i) {
        const CcDelayedEcho *echo = &sim->pending_echoes[i];
        if (!echo->active ||
            !ValidBoundedText(echo->character_name,
                              sizeof(echo->character_name)) ||
            (CcSimSituation(sim, echo->situation_id) == NULL &&
             CcSimQuestOutcome(sim, echo->situation_id) == NULL) ||
            CcSimSettlement(sim, echo->settlement_id) == NULL ||
            echo->outcome <= CC_JOURNEY_OUTCOME_NONE ||
            echo->outcome > CC_JOURNEY_OUTCOME_NEGOTIATED ||
            echo->due_day < 0 ||
            (echo->parent_event_id != 0U &&
             CcSimEvent(sim, echo->parent_event_id) == NULL)) {
            SetError(error, error_capacity,
                     "Queued journey echo is invalid.");
            return false;
        }
    }
    SetError(error, error_capacity, "");
    if (sim->schema_version >= 40U && !CcPoniesValidate(sim)) {
        SetError(error, error_capacity, "Pony company state is invalid.");
        return false;
    }
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

static uint64_t HashGossipVersion(uint64_t hash, const CcGossipVersion *version)
{
    hash = HashU64(hash, version->source_character_id);
    hash = HashU64(hash, (uint64_t)version->retellings);
    hash = HashU64(hash, (uint64_t)version->court_bias);
    hash = HashU64(hash, (uint64_t)version->alarm);
    return HashU64(hash, (uint64_t)version->confidence);
}

uint64_t CcSimHash(const CcSim *sim)
{
    if (sim == NULL) return 0U;
    bool hash_underroad = sim->schema_version >= 20U ||
        (sim->schema_version == 19U && sim->dungeon_count > 0 &&
         sim->dungeons[0].room_count > 0);
    bool hash_social = sim->schema_version >= 20U ||
        (sim->schema_version == 19U && sim->relationship_count > 0);
    bool hash_quest = sim->schema_version >= 21U ||
        (sim->schema_version == 19U &&
         (sim->front_count > 0 || sim->quest_outcome_count > 0));
    bool hash_archives = sim->schema_version >= 22U;
    bool hash_lifecycles = sim->schema_version >= 26U;
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
    if (sim->schema_version >= 31U) HASH_VALUE(sim->road_site_count);
    if (sim->schema_version >= 3U) HASH_VALUE(sim->map_count);
    if (sim->schema_version >= 9U) HASH_VALUE(sim->treasure_count);
    HASH_VALUE(sim->faction_count);
    HASH_VALUE(sim->shipment_count);
    if (sim->schema_version >= 38U) {
        HASH_VALUE(sim->royal_carriage_count);
    }
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
        if (sim->schema_version >= 34U) {
            HASH_VALUE(item->sanction);
            HASH_VALUE(item->unsanctioned_weeks);
            HASH_VALUE(item->pretender_crises);
            HASH_VALUE(item->anointed);
        }
        if (sim->schema_version >= 35U) {
            HASH_VALUE(item->ruler_character_id);
            HASH_VALUE(item->monastery_patron_id);
            HASH_VALUE(item->anointed_by_character_id);
        }
    }
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        const CcSettlement *item = &sim->settlements[i];
        HASH_VALUE(item->id); HASH_VALUE(item->kingdom_id);
        hash = HashString(hash, item->name); HASH_VALUE(item->function);
        HASH_VALUE(item->map_x); HASH_VALUE(item->map_y); HASH_VALUE(item->population);
        HASH_VALUE(item->security); HASH_VALUE(item->prosperity); HASH_VALUE(item->hunger);
        if (sim->schema_version >= 45U) {
            HASH_VALUE(item->fire_damage); HASH_VALUE(item->last_fire_day);
        }
        if (sim->schema_version >= 5U) {
            HASH_VALUE(item->size); HASH_VALUE(item->service_mask);
            HASH_VALUE(item->service_project);
            HASH_VALUE(item->service_project_days);
        }
        int32_t good_count = CcGoodCountForSchema(sim->schema_version);
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
        if (sim->schema_version >= 32U) {
            HASH_VALUE(item->sheep_adults);
            HASH_VALUE(item->sheep_lambs);
            HASH_VALUE(item->sheep_condition);
            HASH_VALUE(item->sheep_hunger);
        }
        if (sim->schema_version >= 34U) {
            HASH_VALUE(item->paper_tool_wear);
        }
        HASH_VALUE(sim->last_shortage_level[i]);
    }
    for (int32_t i = 0; i < sim->route_count; ++i) {
        const CcRoute *item = &sim->routes[i];
        HASH_VALUE(item->id); HASH_VALUE(item->from_id); HASH_VALUE(item->to_id);
        HASH_VALUE(item->travel_days); HASH_VALUE(item->capacity); HASH_VALUE(item->security);
        HASH_VALUE(item->condition); HASH_VALUE(item->closed); HASH_VALUE(item->smuggler_route);
    }
    if (sim->schema_version >= 31U) {
        for (int32_t i = 0; i < sim->road_site_count; ++i) {
            const CcRoadSite *item = &sim->road_sites[i];
            HASH_VALUE(item->id); HASH_VALUE(item->route_id);
            HASH_VALUE(item->home_settlement_id);
            hash = HashString(hash, item->name);
            HASH_VALUE(item->kind); HASH_VALUE(item->input_good);
            HASH_VALUE(item->output_good); HASH_VALUE(item->progress_milli);
            HASH_VALUE(item->side); HASH_VALUE(item->spur_length);
            HASH_VALUE(item->condition); HASH_VALUE(item->blocker);
            HASH_VALUE(item->accessible);
        }
    }
    for (int32_t i = 0; i < sim->map_count; ++i) {
        const CcMap *item = &sim->maps[i];
        HASH_VALUE(item->id); HASH_VALUE(item->route_id);
        HASH_VALUE(item->maker_settlement_id); HASH_VALUE(item->owner_id);
        hash = HashString(hash, item->name);
        HASH_VALUE(item->surveyed_day); HASH_VALUE(item->accuracy);
        HASH_VALUE(item->recorded_condition); HASH_VALUE(item->recorded_danger);
        if (sim->schema_version >= 34U) {
            HASH_VALUE(item->recorded_from_kingdom_id);
            HASH_VALUE(item->recorded_to_kingdom_id);
            HASH_VALUE(item->recorded_closed);
            HASH_VALUE(item->recorded_smuggler_route);
        }
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
    if (sim->schema_version >= 38U) {
        HASH_VALUE(sim->royal_trade_week);
        for (int32_t route = 0; route < sim->route_count; ++route) {
            HASH_VALUE(sim->royal_route_slots_used[route]);
        }
        for (int32_t i = 0; i < sim->royal_carriage_count; ++i) {
            const CcRoyalCarriage *item = &sim->royal_carriages[i];
            HASH_VALUE(item->id); HASH_VALUE(item->kingdom_id);
            HASH_VALUE(item->location_id); HASH_VALUE(item->route_id);
            HASH_VALUE(item->destination_id); HASH_VALUE(item->target_id);
            HASH_VALUE(item->active_shipment_id); HASH_VALUE(item->mode);
            HASH_VALUE(item->departure_day); HASH_VALUE(item->arrival_day);
            HASH_VALUE(item->blocked_since_day);
            HASH_VALUE(item->next_dispatch_day);
            HASH_VALUE(item->condition);
            HASH_VALUE(item->trips_completed); HASH_VALUE(item->cargo_losses);
        }
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
            int32_t good_count = CcGoodCountForSchema(sim->schema_version);
            for (int32_t good = 0; good < good_count; ++good) {
                HASH_VALUE(goblins->carried_goods[good]);
                HASH_VALUE(goblins->lair_stock[good]);
            }
        }
        const CcDragon *dragon = &sim->dragon;
        HASH_VALUE(dragon->id); hash = HashString(hash, dragon->name);
        if (sim->schema_version >= 43U) HASH_VALUE(dragon->hair_color);
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
            if (sim->schema_version >= 35U) {
                HASH_VALUE(dragon->territoryless_days);
            }
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
            int32_t good_count = CcGoodCountForSchema(sim->schema_version);
            for (int32_t good = 0; good < good_count; ++good) {
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
        if (sim->schema_version >= 35U) {
            HASH_VALUE(campaign->patron_character_id);
            HASH_VALUE(campaign->hero_character_id);
        }
        HASH_VALUE(campaign->days_remaining);
        HASH_VALUE(campaign->cooldown_days);
        int32_t good_count = CcGoodCountForSchema(sim->schema_version);
        for (int32_t good = 0; good < good_count; ++good) {
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
        if (hash_underroad) {
            HASH_VALUE(item->layout_seed);
            HASH_VALUE(item->encounter_random_state);
            HASH_VALUE(item->room_count);
            HASH_VALUE(item->link_count);
            for (int32_t room = 0; room < item->room_count; ++room) {
                const CcDungeonRoom *room_item = &item->rooms[room];
                hash = HashString(hash, room_item->name);
                HASH_VALUE(room_item->kind);
                HASH_VALUE(room_item->depth);
                HASH_VALUE(room_item->map_x);
                HASH_VALUE(room_item->map_y);
                HASH_VALUE(room_item->flags);
                HASH_VALUE(room_item->state_flags);
                HASH_VALUE(room_item->loot_good);
                HASH_VALUE(room_item->loot_quantity);
            }
            for (int32_t link = 0; link < item->link_count; ++link) {
                const CcDungeonLink *link_item = &item->links[link];
                HASH_VALUE(link_item->from_room);
                HASH_VALUE(link_item->to_room);
                HASH_VALUE(link_item->kind);
                HASH_VALUE(link_item->flags);
            }
        }
    }
    if (hash_underroad) {
        const CcDungeonExpedition *expedition = &sim->dungeon_expedition;
        HASH_VALUE(expedition->active);
        HASH_VALUE(expedition->dungeon_id);
        HASH_VALUE(expedition->current_room);
        HASH_VALUE(expedition->turns_elapsed);
        HASH_VALUE(expedition->days_elapsed);
        HASH_VALUE(expedition->light_remaining);
        HASH_VALUE(expedition->noise);
        HASH_VALUE(expedition->strain);
        HASH_VALUE(expedition->maximum_depth);
        HASH_VALUE(expedition->encounter_kind);
        HASH_VALUE(expedition->encounter_reaction);
        HASH_VALUE(expedition->encounter_room);
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
        if (hash_quest) {
            HASH_VALUE(item->front_id);
            HASH_VALUE(item->end_reason);
            HASH_VALUE(item->objective.kind);
            HASH_VALUE(item->objective.target_id);
            HASH_VALUE(item->objective.good);
            HASH_VALUE(item->objective.required);
            HASH_VALUE(item->objective.progress.value);
            HASH_VALUE(item->objective.progress.limit);
            HASH_VALUE(item->objective.progress.created_by_event_id);
            HASH_VALUE(item->objective.progress.resolved_by_event_id);
            HASH_VALUE(item->objective.danger.value);
            HASH_VALUE(item->objective.danger.limit);
            HASH_VALUE(item->objective.danger.created_by_event_id);
            HASH_VALUE(item->objective.danger.resolved_by_event_id);
            HASH_VALUE(item->objective.evidence_count);
            for (int32_t evidence = 0;
                 evidence < CC_MAX_QUEST_EVIDENCE; ++evidence) {
                HASH_VALUE(item->objective.evidence_event_ids[evidence]);
            }
        }
        if (hash_social) {
            HASH_VALUE(item->witness_character_id);
            HASH_VALUE(item->discovery_stage);
            HASH_VALUE(item->lead_path);
            HASH_VALUE(item->lead_event_id);
        }
        if (item->sponsor_name[0] != '\0' || item->affected_name[0] != '\0') {
            hash = HashString(hash, item->sponsor_name);
            hash = HashString(hash, item->affected_name);
        }
    }
    if (hash_quest) {
        HASH_VALUE(sim->front_count);
        for (int32_t i = 0; i < sim->front_count; ++i) {
            const CcFront *front = &sim->fronts[i];
            HASH_VALUE(front->id);
            HASH_VALUE(front->kind);
            HASH_VALUE(front->status);
            HASH_VALUE(front->outcome);
            HASH_VALUE(front->anchor_id);
            HASH_VALUE(front->cause_event_id);
            HASH_VALUE(front->created_event_id);
            HASH_VALUE(front->resolved_event_id);
            HASH_VALUE(front->created_day);
            HASH_VALUE(front->resolved_day);
            HASH_VALUE(front->portent.value);
            HASH_VALUE(front->portent.limit);
            HASH_VALUE(front->portent.created_by_event_id);
            HASH_VALUE(front->portent.resolved_by_event_id);
            HASH_VALUE(front->situation_count);
            for (int32_t member = 0;
                 member < CC_MAX_FRONT_SITUATIONS; ++member) {
                HASH_VALUE(front->situation_ids[member]);
            }
            hash = HashString(hash, front->premise);
        }
        HASH_VALUE(sim->quest_outcome_count);
        for (int32_t i = 0; i < sim->quest_outcome_count; ++i) {
            const CcQuestOutcomeRecord *outcome = &sim->quest_outcomes[i];
            HASH_VALUE(outcome->id);
            HASH_VALUE(outcome->situation_id);
            HASH_VALUE(outcome->front_id);
            HASH_VALUE(outcome->situation_kind);
            HASH_VALUE(outcome->front_kind);
            HASH_VALUE(outcome->situation_status);
            HASH_VALUE(outcome->end_reason);
            HASH_VALUE(outcome->front_outcome);
            HASH_VALUE(outcome->target_id);
            HASH_VALUE(outcome->sponsor_character_id);
            HASH_VALUE(outcome->affected_character_id);
            HASH_VALUE(outcome->cause_event_id);
            HASH_VALUE(outcome->resolved_event_id);
            HASH_VALUE(outcome->resolved_day);
            HASH_VALUE(outcome->progress_value);
            HASH_VALUE(outcome->progress_limit);
            HASH_VALUE(outcome->danger_value);
            HASH_VALUE(outcome->danger_limit);
        }
    }
    if (sim->schema_version >= 17U) {
        HASH_VALUE(sim->character_count);
        if (hash_lifecycles) {
            HASH_VALUE(sim->character_births);
            HASH_VALUE(sim->character_deaths);
        }
        for (int32_t i = 0; i < sim->character_count; ++i) {
            const CcCharacter *character = &sim->characters[i];
            HASH_VALUE(character->id);
            hash = HashString(hash, character->name);
            if (hash_lifecycles) {
                HASH_VALUE(character->ancestor_id);
                HASH_VALUE(character->birth_day);
                HASH_VALUE(character->death_day);
                HASH_VALUE(character->generation);
            }
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
            if (hash_social) {
                HASH_VALUE(character->knowledge_count);
                HASH_VALUE(character->knowledge_write_index);
                for (int32_t knowledge = 0;
                     knowledge < CC_CHARACTER_KNOWLEDGE_CAPACITY;
                     ++knowledge) {
                    const CcCharacterKnowledge *item =
                        &character->knowledge[knowledge];
                    HASH_VALUE(item->kind);
                    HASH_VALUE(item->subject_id);
                    HASH_VALUE(item->source_character_id);
                    HASH_VALUE(item->event_id);
                    HASH_VALUE(item->certainty);
                    HASH_VALUE(item->private_knowledge);
                    HASH_VALUE(item->day);
                }
            }
        }
    }
    if (hash_social) {
        HASH_VALUE(sim->relationship_count);
        for (int32_t i = 0; i < sim->relationship_count; ++i) {
            const CcRelationship *relationship = &sim->relationships[i];
            HASH_VALUE(relationship->from_character_id);
            HASH_VALUE(relationship->to_character_id);
            HASH_VALUE(relationship->affinity);
            HASH_VALUE(relationship->trust);
            HASH_VALUE(relationship->obligation);
            HASH_VALUE(relationship->history);
            HASH_VALUE(relationship->cause_event_id);
        }
    }
    HASH_VALUE(sim->player.id); HASH_VALUE(sim->player.location_id);
    HASH_VALUE(sim->player.coins); HASH_VALUE(sim->player.cargo_capacity);
    HASH_VALUE(sim->player.passenger_capacity);
    if (sim->schema_version >= 3U) HASH_VALUE(sim->player.map_capacity);
    HASH_VALUE(sim->player.reputation);
    if (sim->schema_version >= 9U) HASH_VALUE(sim->player.treasure_cargo_slots);
    if (sim->schema_version >= 13U) {
        HASH_VALUE(sim->player.map_catalogue_mask);
        HASH_VALUE(sim->player.map_archive_mask);
    }
    if (sim->schema_version >= 23U) {
        for (int32_t i = 0; i < sim->route_count; ++i) {
            const CcRouteKnowledge *knowledge =
                &sim->player.route_knowledge[i];
            HASH_VALUE(knowledge->route_id);
            HASH_VALUE(knowledge->from_reveal_milli);
            HASH_VALUE(knowledge->to_reveal_milli);
            if (sim->schema_version >= 34U) {
                HASH_VALUE(knowledge->learned_day);
                HASH_VALUE(knowledge->recorded_condition);
                HASH_VALUE(knowledge->recorded_danger);
                HASH_VALUE(knowledge->source);
                HASH_VALUE(knowledge->recorded_closed);
                HASH_VALUE(knowledge->recorded_smuggler_route);
            }
        }
    }
    if (sim->schema_version >= 34U) {
        HASH_VALUE(sim->player.road_book_site_discovery_mask);
        for (int32_t i = 0; i < sim->settlement_count; ++i) {
            const CcSettlementKnowledge *knowledge =
                &sim->player.settlement_knowledge[i];
            HASH_VALUE(knowledge->settlement_id);
            HASH_VALUE(knowledge->kingdom_id);
            HASH_VALUE(knowledge->learned_day);
            HASH_VALUE(knowledge->source);
        }
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

    if (sim->player.accepted_situation_id != 0U) {
        HASH_VALUE(sim->player.accepted_situation_id);
    }
    if (sim->schema_version <= 3U) {

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
        if (sim->schema_version >= 39U) {
            HASH_VALUE(sim->journey.road_site_stop_mask);
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
        if (hash_quest) {
            HASH_VALUE(sim->pending_echo_count);
            for (int32_t i = 0; i < CC_MAX_PENDING_ECHOES; ++i) {
                const CcDelayedEcho *echo = &sim->pending_echoes[i];
                HASH_VALUE(echo->active);
                HASH_VALUE(echo->situation_id);
                HASH_VALUE(echo->settlement_id);
                HASH_VALUE(echo->parent_event_id);
                HASH_VALUE(echo->outcome);
                HASH_VALUE(echo->due_day);
                hash = HashString(hash, echo->character_name);
            }
        }
    }
    int32_t player_good_count = CcGoodCountForSchema(sim->schema_version);
    for (int32_t good = 0; good < player_good_count; ++good) {
        HASH_VALUE(sim->player.cargo[good]);
    }
    for (int32_t i = 0; i < CC_MAX_EVENTS; ++i) {
        const CcEvent *item = &sim->events[i];
        HASH_VALUE(item->id); HASH_VALUE(item->day); HASH_VALUE(item->kind);
        HASH_VALUE(item->subject_id); HASH_VALUE(item->location_id); HASH_VALUE(item->parent_id);
        if (hash_social) {
            HASH_VALUE(item->actor_id);
            HASH_VALUE(item->target_id);
            HASH_VALUE(item->beneficiary_id);
            HASH_VALUE(item->witness_id);
        }
        HASH_VALUE(item->magnitude); hash = HashString(hash, item->text);
    }
    if (hash_archives) {
        HASH_VALUE(sim->archives.scribes);
        HASH_VALUE(sim->archives.lore_stored);
        HASH_VALUE(sim->archives.lore_lost_total);
        HASH_VALUE(sim->archives.last_recorded_day);
        HASH_VALUE(sim->archives.lore_ceiling);
        if (sim->schema_version >= 34U) {
            HASH_VALUE(sim->archives.kit_tool_wear);
        }
        if (sim->schema_version >= 35U) {
            HASH_VALUE(sim->archives.abbot_character_id);
            HASH_VALUE(sim->archives.stewardship_rank);
        }
    }
    if (sim->schema_version >= 44U) {
        HASH_VALUE(sim->gossip_last_event_id);
        for (int32_t i = 0; i < CC_MAX_GOSSIP; ++i) {
            const CcGossip *story = &sim->gossip[i];
            HASH_VALUE(story->event_id); HASH_VALUE(story->origin_id);
            HASH_VALUE(story->heard_event_id); HASH_VALUE(story->day);
            HASH_VALUE(story->heard_day); HASH_VALUE(story->settlement_mask);
            HASH_VALUE(story->recorded);
            HASH_VALUE(story->kind);
            hash = HashString(hash, story->text);
            hash = HashString(hash, story->heard_from);
            for (int32_t town = 0; town < CC_MAX_SETTLEMENTS; ++town) {
                if ((story->settlement_mask & (UINT32_C(1) << (uint32_t)town)) != 0U) {
                    hash = HashGossipVersion(hash, &story->local[town]);
                }
            }
            if (story->heard_day > 0) hash = HashGossipVersion(hash, &story->heard);
        }
        for (int32_t i = 0; i < CcSimGossipCarrierCapacity(sim); ++i) {
            HASH_VALUE(sim->gossip_carriers[i].id);
            HASH_VALUE(sim->gossip_carriers[i].stories);
            for (int32_t slot = 0; slot < CC_MAX_GOSSIP; ++slot) {
                if ((sim->gossip_carriers[i].stories & (UINT32_C(1) << (uint32_t)slot)) != 0U) {
                    hash = HashGossipVersion(hash, &sim->gossip_carriers[i].versions[slot]);
                }
            }
        }
    }
    if (sim->schema_version >= 40U) {
        HASH_VALUE(sim->pony_company.team[0]);
        HASH_VALUE(sim->pony_company.team[1]);
        HASH_VALUE(sim->pony_company.encounter);
        for (int32_t i = 0; i < CC_PONY_COUNT; ++i) {
            const CcPony *pony = &sim->pony_company.ponies[i];
            HASH_VALUE(pony->route_id);
            HASH_VALUE(pony->last_seen_route);
            HASH_VALUE(pony->bond);
            HASH_VALUE(pony->quests_completed);
            HASH_VALUE(pony->releases);
            HASH_VALUE(pony->last_met_day);
            HASH_VALUE(pony->quest_kind);
            HASH_VALUE(pony->quest_amount);
            HASH_VALUE(pony->health);
            HASH_VALUE(pony->fatigue);
            HASH_VALUE(pony->hunger);
            HASH_VALUE(pony->seen);
            HASH_VALUE(pony->ready);
        }
    }
#undef HASH_VALUE
    return hash;
}
