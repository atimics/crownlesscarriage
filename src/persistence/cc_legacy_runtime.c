#include "persistence/cc_legacy_runtime_internal.h"

#include <stdio.h>
#include <string.h>

enum { CC_SCHEMA17_EVENT_ENCOUNTER_LOOT = 90 };

static void SetError(char *error, size_t capacity, const char *message)
{
    if (error == NULL || capacity == 0U) return;
    (void)snprintf(error, capacity, "%s", message);
}

static void TunePhysicalReserveTargets(CcSim *sim)
{
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        CcSettlement *place = &sim->settlements[i];
        switch (place->function) {
            case CC_SETTLEMENT_FARMING:
                place->reserve_target[CC_GOOD_IRON] = 2;
                place->reserve_target[CC_GOOD_TOOLS] = 6;
                break;
            case CC_SETTLEMENT_MARKET:
                place->reserve_target[CC_GOOD_IRON] = 16;
                place->reserve_target[CC_GOOD_TOOLS] = 12;
                break;
            case CC_SETTLEMENT_FORTRESS:
                place->reserve_target[CC_GOOD_IRON] = 8;
                place->reserve_target[CC_GOOD_TOOLS] = 8;
                break;
            case CC_SETTLEMENT_MINING:
                place->reserve_target[CC_GOOD_IRON] = 12;
                place->reserve_target[CC_GOOD_TOOLS] = 8;
                break;
            case CC_SETTLEMENT_CAPITAL:
                place->reserve_target[CC_GOOD_IRON] = 16;
                place->reserve_target[CC_GOOD_TOOLS] = 14;
                break;
            case CC_SETTLEMENT_DUNGEON_TOWN:
                place->reserve_target[CC_GOOD_IRON] = 10;
                place->reserve_target[CC_GOOD_TOOLS] = 6;
                break;
        }
    }
}

static bool HasQuestArchitecture(const CcSim *sim)
{
    if (sim->front_count > 0 || sim->quest_outcome_count > 0 ||
        sim->pending_echo_count > 0) return true;
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        const CcSituation *situation = &sim->situations[i];
        if (situation->front_id != 0U ||
            situation->objective.progress.limit > 0) return true;
    }
    return false;
}

static void UpgradeLegacyJourneyRhythm(CcSim *sim)
{
    if (sim == NULL || !sim->journey.active ||
        sim->journey.total_subticks <= 0) return;
    int32_t old_total = sim->journey.total_subticks;
    int32_t old_days = old_total / CC_WORLD_DAY_SUBTICKS;
    if (old_days < 1) old_days = 1;
    int32_t watches = old_days * 2;
    if (watches < 3) watches = 3;
    int32_t new_total = watches * CC_WORLD_WATCH_SUBTICKS;
    sim->journey.elapsed_subticks = (int32_t)(
        (int64_t)sim->journey.elapsed_subticks * new_total / old_total);
    sim->journey.encounter_subticks = (int32_t)(
        (int64_t)sim->journey.encounter_subticks * new_total / old_total);
    sim->journey.total_subticks = new_total;
    sim->carriage.progress_milli = (int32_t)(
        (int64_t)sim->journey.elapsed_subticks * 1000 / new_total);
    if (sim->journey.phase == CC_JOURNEY_PHASE_TRAVELLING) {
        int32_t pace_rate = sim->journey.pace == CC_JOURNEY_PACE_CAREFUL ?
            24 : sim->journey.pace == CC_JOURNEY_PACE_PUSH ? 38 :
            CC_TRAVEL_GAME_MINUTES_PER_SECOND;
        int32_t base_speed = (int32_t)(
            (INT64_C(52000) * CC_TRAVEL_GAME_MINUTES_PER_SECOND *
             CC_WORLD_TICKS_PER_SECOND) / new_total);
        sim->carriage.speed_milli_per_second =
            base_speed * pace_rate / CC_TRAVEL_GAME_MINUTES_PER_SECOND;
    } else {
        sim->carriage.speed_milli_per_second = 0;
    }
}

static void ClearMissingLegacyEventReferences(CcSim *sim)
{
    for (int32_t i = 0; i < CC_MAX_EVENTS; ++i) {
        CcEvent *event = &sim->events[i];
        if (event->id != 0U && event->parent_id != 0U &&
            CcSimEvent(sim, event->parent_id) == NULL) {
            event->parent_id = 0U;
        }
    }
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        CcSituation *situation = &sim->situations[i];
        if (situation->cause_event_id != 0U &&
            CcSimEvent(sim, situation->cause_event_id) == NULL) {
            situation->cause_event_id = 0U;
        }
    }
}

static bool LegacyCharacterNameExists(const CcSim *sim, int32_t except,
                                      const char *name)
{
    for (int32_t i = 0; i < sim->character_count; ++i) {
        if (i != except && strcmp(sim->characters[i].name, name) == 0) {
            return true;
        }
    }
    return false;
}

static void MakeLegacyCharacterNamesUnique(CcSim *sim)
{
    for (int32_t i = 0; i < sim->character_count; ++i) {
        CcCharacter *character = &sim->characters[i];
        bool duplicate = false;
        for (int32_t earlier = 0; earlier < i; ++earlier) {
            if (strcmp(sim->characters[earlier].name, character->name) == 0) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) continue;

        char name[CC_NAME_CAPACITY];
        uint32_t ordinal = (uint32_t)i;
        do {
            CcGenerateCharacterName(
                sim->world_seed, character->home_settlement_id,
                character->generation, ordinal++, name);
        } while (LegacyCharacterNameExists(sim, i, name) && ordinal < 2048U);
        (void)snprintf(character->name, sizeof(character->name), "%s", name);
        /* Situations keep a name snapshot next to the character id; keep it
           in step so the migrated cast still validates. */
        for (int32_t s = 0; s < sim->situation_count; ++s) {
            CcSituation *situation = &sim->situations[s];
            if (situation->sponsor_character_id == character->id) {
                (void)snprintf(situation->sponsor_name,
                               sizeof(situation->sponsor_name), "%s",
                               character->name);
            }
            if (situation->affected_character_id == character->id) {
                (void)snprintf(situation->affected_name,
                               sizeof(situation->affected_name), "%s",
                               character->name);
            }
        }
    }
}

static void FinishMaterialChainUpgrade(CcSim *sim)
{
    CcSimInitializeMaterialChain(sim);
    CcSimUpgradeHistoryOffices(sim);
    CcSimUpgradeArchivePhysicalLore(sim);
    CcSimInitializeRoyalCarriages(sim);
    sim->schema_version = CC_SIM_SCHEMA_VERSION;
    sim->generator_version = CC_GENERATOR_VERSION;
}

static void FinishLegacyRuntimeUpgrade(CcSim *sim)
{
    if (!HasQuestArchitecture(sim)) CcSimUpgradeQuestArchitecture(sim);
    CcSimInitializePlayerRouteKnowledge(sim);
    UpgradeLegacyJourneyRhythm(sim);
    CcSimUpgradeCharacterLifecycles(sim);
    CcSimUpgradeGrainEconomy(sim);
    CcSimInitializeRoadSites(sim);
    CcSimUpgradeFlockEconomy(sim);
    ClearMissingLegacyEventReferences(sim);
    FinishMaterialChainUpgrade(sim);
}

static void InitializeExtendedGoods(CcSim *sim)
{
    for (int32_t settlement = 0;
         settlement < sim->settlement_count; ++settlement) {
        CcSettlement *place = &sim->settlements[settlement];
        for (int32_t good = CC_LEGACY_GOOD_COUNT;
             good < CC_GOOD_COUNT; ++good) {
            place->price[good] =
                CcGoodDefinitionFor((CcGood)good)->base_price;
        }
    }
    CcSimInitializeWoodEconomy(sim);
    CcSimInitializeStoneEconomy(sim);
    CcSimInitializePaperEconomy(sim);
}

static bool UpgradeLegacyRuntimeSchema(CcSim *sim,
                                       char *error, size_t error_capacity)
{
    uint32_t legacy_version = sim->schema_version;
    if ((legacy_version == 38U || legacy_version == 39U ||
         legacy_version == 40U || legacy_version == 41U ||
         legacy_version == 42U || legacy_version == 43U ||
         legacy_version == 44U || legacy_version == 45U ||
         legacy_version == 46U || legacy_version == 47U ||
         legacy_version == 48U || legacy_version == 49U ||
         legacy_version == 50U || legacy_version == 51U ||
         legacy_version == 52U || legacy_version == 53U ||
         legacy_version == 54U || legacy_version == 55U ||
         legacy_version == 56U || legacy_version == 57U ||
         legacy_version == 58U || legacy_version == 59U ||
         legacy_version == 60U || legacy_version == 61U ||
         legacy_version == 62U || legacy_version == 63U ||
         legacy_version == 64U || legacy_version == 65U ||
         legacy_version == 66U || legacy_version == 67U ||
         legacy_version == 68U || legacy_version == 69U ||
         legacy_version == 70U || legacy_version == 71U || legacy_version == 72U || legacy_version == 73U ||
         legacy_version == 74U || legacy_version == 75U || legacy_version == 76U ||
         legacy_version == 77U || legacy_version == 78U || legacy_version == 79U || legacy_version == 80U || legacy_version == 81U || legacy_version == 82U || legacy_version == 83U || legacy_version == 84U || legacy_version == 85U || legacy_version == 86U || legacy_version == 87U || legacy_version == 88U || legacy_version == 89U || legacy_version == 90U) &&
        sim->generator_version == 25U) {
        /* Schema 47 adds bandit war camps (camp_settlement_id, default
         * 0 = no camp). Schema 48 adds told-story bits (gossip_carrier.told_player,
         * default 0). Schema 49 makes notable famine accounts gossip and
         * schema 50 adds goblin raids, cult rallies, dragon omens and dragon
         * fires; both changes are derived from events, so older saves need
         * no data migration. Schema 51 seeds pony herds and schema 52
         * unharnesses the second animal in the caller below. Schema 53 changes
         * hoard-return food rules; schema 54 adds paper decay. Schema 55 adds
         * dragon succession gossip and reports gathered at ruins. Historical
         * journal replay uses the original rule gates before this upgrade.
         * Schema 56 adds the saved archive silence date, defaulting to zero.
         * Schema 58 uses local name roots for new residents and descendants;
         * saved names remain intact. Schema 59 adds mine visits with an empty
         * visit for older saves. Schema 61 seeds a Silverwick tool line in new
         * worlds; stored production capacities remain intact. */
        sim->schema_version = CC_SIM_SCHEMA_VERSION;
        return true;
    }
    if ((legacy_version == 36U || legacy_version == 37U) &&
        sim->generator_version == 25U) {
        CcSimInitializeRoyalCarriages(sim);
        sim->schema_version = CC_SIM_SCHEMA_VERSION;
        return true;
    }
    ClearMissingLegacyEventReferences(sim);
    MakeLegacyCharacterNamesUnique(sim);
    if (legacy_version == 35U && sim->generator_version == 25U) {
        CcSimUpgradeHistoryOffices(sim);
        CcSimUpgradeArchivePhysicalLore(sim);
        CcSimInitializeRoyalCarriages(sim);
        sim->schema_version = CC_SIM_SCHEMA_VERSION;
        sim->generator_version = CC_GENERATOR_VERSION;
        return true;
    }
    if (legacy_version == 34U && sim->generator_version == 25U) {
        CcSimUpgradeHistoryOffices(sim);
        CcSimUpgradeArchivePhysicalLore(sim);
        CcSimInitializeRoyalCarriages(sim);
        sim->schema_version = CC_SIM_SCHEMA_VERSION;
        sim->generator_version = CC_GENERATOR_VERSION;
        return true;
    }
    if (legacy_version == 33U && sim->generator_version == 25U) {
        FinishMaterialChainUpgrade(sim);
        return true;
    }
    if (legacy_version == 32U && sim->generator_version == 25U) {
        CcSimInitializePaperEconomy(sim);
        FinishMaterialChainUpgrade(sim);
        return true;
    }
    if (legacy_version == 31U && sim->generator_version == 24U) {
        CcSimUpgradeFlockEconomy(sim);
        CcSimInitializePaperEconomy(sim);
        FinishMaterialChainUpgrade(sim);
        return true;
    }
    if (legacy_version == 30U && sim->generator_version == 23U) {
        CcSimInitializeRoadSites(sim);
        CcSimUpgradeFlockEconomy(sim);
        CcSimInitializePaperEconomy(sim);
        FinishMaterialChainUpgrade(sim);
        return true;
    }
    if (legacy_version == 29U) {
        CcSimInitializeStoneEconomy(sim);
        CcSimInitializeRoadSites(sim);
        CcSimUpgradeFlockEconomy(sim);
        CcSimInitializePaperEconomy(sim);
        FinishMaterialChainUpgrade(sim);
        return true;
    }
    if (legacy_version == 28U) {
        CcSimUpgradeGrainEconomy(sim);
        CcSimInitializeStoneEconomy(sim);
        CcSimInitializeRoadSites(sim);
        CcSimUpgradeFlockEconomy(sim);
        CcSimInitializePaperEconomy(sim);
        FinishMaterialChainUpgrade(sim);
        return true;
    }
    if (legacy_version != 2U && legacy_version != 3U &&
        legacy_version != 4U &&
        legacy_version != 5U && legacy_version != 6U &&
        legacy_version != 7U && legacy_version != 8U &&
        legacy_version != 9U && legacy_version != 10U &&
        legacy_version != 11U && legacy_version != 12U &&
        legacy_version != 13U && legacy_version != 14U &&
        legacy_version != 15U && legacy_version != 16U &&
        legacy_version != 17U && legacy_version != 18U &&
        legacy_version != 19U && legacy_version != 20U &&
        legacy_version != 21U && legacy_version != 22U &&
        legacy_version != 23U && legacy_version != 24U &&
        legacy_version != 25U && legacy_version != 26U &&
        legacy_version != 27U && legacy_version != 28U &&
        legacy_version != 29U && legacy_version != 30U &&
        legacy_version != 31U && legacy_version != 32U &&
        legacy_version != 33U && legacy_version != 34U &&
        legacy_version != 35U) return true;
    if (legacy_version == 27U) {
        CcSimInitializeWoodEconomy(sim);
        if (sim->generator_version != 23U) {
            CcSimUpgradeGrainEconomy(sim);
        }
        CcSimInitializeStoneEconomy(sim);
        CcSimInitializeRoadSites(sim);
        CcSimUpgradeFlockEconomy(sim);
        CcSimInitializePaperEconomy(sim);
        FinishMaterialChainUpgrade(sim);
        return true;
    }
    InitializeExtendedGoods(sim);
    if (legacy_version == 26U) {
        CcSimUpgradeGrainEconomy(sim);
        CcSimInitializeRoadSites(sim);
        CcSimUpgradeFlockEconomy(sim);
        FinishMaterialChainUpgrade(sim);
        return true;
    }
    if (legacy_version == 17U) {
        for (int32_t i = 0; i < CC_MAX_EVENTS; ++i) {
            if ((int32_t)sim->events[i].kind ==
                CC_SCHEMA17_EVENT_ENCOUNTER_LOOT) {
                sim->events[i].kind = CC_EVENT_ENCOUNTER_LOOT;
            }
        }
    }
    bool social_schema_19 = legacy_version == 19U &&
        sim->relationship_count > 0;
    bool quest_schema_19 = legacy_version == 19U &&
        HasQuestArchitecture(sim);
    if (social_schema_19) {
        int32_t old_first = (int32_t)CC_EVENT_GOBLIN_TUNNEL_TRAVERSED + 1;
        int32_t old_last = old_first + 3;
        for (int32_t i = 0; i < CC_MAX_EVENTS; ++i) {
            int32_t kind = (int32_t)sim->events[i].kind;
            if (kind >= old_first && kind <= old_last) {
                sim->events[i].kind = (CcEventKind)(
                    (int32_t)CC_EVENT_RELATIONSHIP_HISTORY +
                    kind - old_first);
            }
        }
    }
    if (quest_schema_19) {
        int32_t old_first = (int32_t)CC_EVENT_GOBLIN_TUNNEL_TRAVERSED + 1;
        int32_t old_last = old_first + 3;
        for (int32_t i = 0; i < CC_MAX_EVENTS; ++i) {
            int32_t kind = (int32_t)sim->events[i].kind;
            if (kind >= old_first && kind <= old_last) {
                sim->events[i].kind = (CcEventKind)(
                    (int32_t)CC_EVENT_FRONT_CREATED + kind - old_first);
            }
        }
    }
    if (legacy_version == 25U) {
        CcSimUpgradeCharacterLifecycles(sim);
        CcSimUpgradeGrainEconomy(sim);
        CcSimInitializeRoadSites(sim);
        CcSimUpgradeFlockEconomy(sim);
        FinishMaterialChainUpgrade(sim);
        return true;
    }
    if (legacy_version == 24U) {
        FinishLegacyRuntimeUpgrade(sim);
        return true;
    }
    if (legacy_version == 23U) {
        FinishLegacyRuntimeUpgrade(sim);
        return true;
    }
    if (legacy_version == 22U) {
        FinishLegacyRuntimeUpgrade(sim);
        return true;
    }
    if (legacy_version == 21U) {
        sim->archives = (CcArchives){
            .lore_ceiling = 40
        };
        FinishLegacyRuntimeUpgrade(sim);
        return true;
    }
    CcSimInitializeUnderroad(sim);
    if (legacy_version == 20U) {
        FinishLegacyRuntimeUpgrade(sim);
        return true;
    }
    if (legacy_version == 19U) {
        CcSimInitializeCharacters(sim);
        FinishLegacyRuntimeUpgrade(sim);
        return true;
    }
    if (legacy_version == 18U) {
        if (sim->generator_version == 16U) {
            CcSimUpgradeMapCollection(sim);
        }
        CcSimInitializeCharacters(sim);
        FinishLegacyRuntimeUpgrade(sim);
        return true;
    }
    if (legacy_version == 17U) {
        CcSimUpgradeMapCollection(sim);
        sim->journey.pace = CC_JOURNEY_PACE_STEADY;
        sim->journey.ambush_warned = false;
        CcSimInitializeCharacters(sim);
        FinishLegacyRuntimeUpgrade(sim);
        return true;
    }
    sim->goblins.cohesion = 60;
    sim->goblins.target_warned = false;
    sim->goblins.expeditions_intercepted = 0;
    sim->dragon_cult.dragon_seed_phase = CC_GOBLIN_DRAGON_SEED_NONE;
    sim->dragon_cult.dragon_seed_days_remaining = 0;
    CcSimUpgradeMapCollection(sim);
    if (legacy_version == 16U) {
        CcSimInitializeCharacters(sim);
        FinishLegacyRuntimeUpgrade(sim);
        return true;
    }
    if (legacy_version == 15U) {
        CcSimInitializeCharacters(sim);
        FinishLegacyRuntimeUpgrade(sim);
        return true;
    }
    if (legacy_version == 14U) {
        CcSimInitializeHorseStableSystem(sim);
        CcSimInitializeCharacters(sim);
        FinishLegacyRuntimeUpgrade(sim);
        return true;
    }
    if (legacy_version == 13U) {
        CcSimInitializeAnimalEconomy(sim);
        CcSimInitializeCharacters(sim);
        FinishLegacyRuntimeUpgrade(sim);
        return true;
    }
    if (legacy_version <= 3U) {
        sim->clock = (CcWorldClock){
            .game_minutes_per_second = CC_IDLE_GAME_MINUTES_PER_SECOND
        };
        sim->carriage = (CcCarriageState){
            .mode = CC_CARRIAGE_PARKED,
            .location_id = sim->player.location_id,
            .condition = 100
        };
        if (sim->journey.active) {
            const CcRoute *route = CcSimRoute(sim, sim->journey.route_id);
            if (route == NULL) {
                SetError(error, error_capacity,
                         "Legacy journey route is no longer valid.");
                return false;
            }
            int32_t fare = route->travel_days +
                (route->smuggler_route ? 3 : 0);
            fare += route->closed ? 4 : 0;
            fare += route->smuggler_route ? 2 :
                CcSimRouteCrossesWarBorder(sim, route->id) ? 4 : 0;
            if (sim->player.coins >= fare) sim->player.coins -= fare;
            int32_t total_subticks = route->travel_days * CC_WORLD_DAY_SUBTICKS;
            sim->journey.phase = CC_JOURNEY_PHASE_BLOCKED;
            sim->journey.departure_day = sim->current_day;
            sim->journey.total_subticks = total_subticks;
            sim->journey.encounter_subticks = 0;
            sim->journey.elapsed_subticks = 0;
            sim->journey.fare_reserved = fare;
            sim->journey.encounter_triggered = true;
            const CcEvent *recent = CcSimRecentEvent(sim, 0);
            sim->journey.parent_event_id = recent != NULL ? recent->id : 0U;
            sim->clock.game_minutes_per_second = 0;
            sim->carriage = (CcCarriageState){
                .mode = CC_CARRIAGE_STOPPED,
                .route_id = sim->journey.route_id,
                .origin_id = sim->journey.origin_id,
                .destination_id = sim->journey.destination_id,
                .condition = 100
            };
        }
    }

    if (legacy_version <= 4U) {
#define LEGACY_SERVICE(service) (UINT32_C(1) << (uint32_t)(service))
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        CcSettlement *settlement = &sim->settlements[i];
        if (settlement->service_mask != 0U) continue;
        settlement->service_project = CC_SERVICE_NONE;
        settlement->service_project_days = 0;
        settlement->size = settlement->function == CC_SETTLEMENT_CAPITAL ?
            CC_SETTLEMENT_CAPITAL_SIZE :
            (settlement->function == CC_SETTLEMENT_MARKET ||
             settlement->function == CC_SETTLEMENT_FORTRESS ||
             settlement->function == CC_SETTLEMENT_MINING) ?
                CC_SETTLEMENT_TOWN : CC_SETTLEMENT_VILLAGE;
        settlement->service_mask = LEGACY_SERVICE(CC_SERVICE_INN);
        switch (settlement->function) {
            case CC_SETTLEMENT_FARMING:
                settlement->service_mask |= LEGACY_SERVICE(CC_SERVICE_FARM) |
                    LEGACY_SERVICE(CC_SERVICE_GRANARY) |
                    LEGACY_SERVICE(CC_SERVICE_STABLE);
                break;
            case CC_SETTLEMENT_MINING:
                settlement->service_mask |= LEGACY_SERVICE(CC_SERVICE_MINE) |
                    LEGACY_SERVICE(CC_SERVICE_SMITHY) |
                    LEGACY_SERVICE(CC_SERVICE_MARKET) |
                    LEGACY_SERVICE(CC_SERVICE_SHRINE);
                break;
            case CC_SETTLEMENT_MARKET:
                settlement->service_mask |= LEGACY_SERVICE(CC_SERVICE_MARKET) |
                    LEGACY_SERVICE(CC_SERVICE_SMITHY) |
                    LEGACY_SERVICE(CC_SERVICE_STABLE) |
                    LEGACY_SERVICE(CC_SERVICE_CARTOGRAPHER);
                break;
            case CC_SETTLEMENT_FORTRESS:
                settlement->service_mask |= LEGACY_SERVICE(CC_SERVICE_BARRACKS) |
                    LEGACY_SERVICE(CC_SERVICE_SMITHY) |
                    LEGACY_SERVICE(CC_SERVICE_HEALER) |
                    LEGACY_SERVICE(CC_SERVICE_GRANARY);
                break;
            case CC_SETTLEMENT_CAPITAL:
                settlement->service_mask |= LEGACY_SERVICE(CC_SERVICE_MARKET) |
                    LEGACY_SERVICE(CC_SERVICE_SMITHY) |
                    LEGACY_SERVICE(CC_SERVICE_HEALER) |
                    LEGACY_SERVICE(CC_SERVICE_STABLE) |
                    LEGACY_SERVICE(CC_SERVICE_SHRINE) |
                    LEGACY_SERVICE(CC_SERVICE_BARRACKS) |
                    LEGACY_SERVICE(CC_SERVICE_CARTOGRAPHER) |
                    LEGACY_SERVICE(CC_SERVICE_GUILDHALL);
                break;
            case CC_SETTLEMENT_DUNGEON_TOWN:
                settlement->service_mask |= LEGACY_SERVICE(CC_SERVICE_HEALER) |
                    LEGACY_SERVICE(CC_SERVICE_BLACK_MARKET) |
                    LEGACY_SERVICE(CC_SERVICE_DUNGEON_WARD);
                break;
        }
    }
    for (int32_t i = 0; i < sim->bandit_count; ++i) {
        CcBanditGroup *bandits = &sim->bandits[i];
        if (bandits->service_mask == 0U) {
            bandits->camp_size = bandits->influence >= 60 ?
                CC_BANDIT_WAR_CAMP : bandits->influence >= 35 ?
                CC_BANDIT_CAMP : CC_BANDIT_HIDEOUT;
            bandits->service_mask = LEGACY_SERVICE(CC_SERVICE_BLACK_MARKET) |
                                    LEGACY_SERVICE(CC_SERVICE_STABLE);
        }
        bandits->raid_phase = CC_BANDIT_RAID_IDLE;
        bandits->raid_target_id = 0U;
        bandits->raid_good = CC_GOOD_FOOD;
        bandits->raid_quantity = 0;
        bandits->raid_days_remaining = 0;
    }
#undef LEGACY_SERVICE
    }
    if (legacy_version >= 11U) {
        CcSimInitializeDragonEcology(sim);
        CcSimInitializeAnimalEconomy(sim);
        CcSimInitializeCharacters(sim);
        FinishLegacyRuntimeUpgrade(sim);
        return true;
    }
    if (legacy_version == 10U) {
        CcSimInitializeDragonEcology(sim);
        CcSimInitializeAnimalEconomy(sim);
        CcSimInitializeCharacters(sim);
        FinishLegacyRuntimeUpgrade(sim);
        return true;
    }
    if (legacy_version == 9U) {
        TunePhysicalReserveTargets(sim);
        if (sim->iron_ledger_reserve == 0) {
            for (int32_t i = 0; i < sim->kingdom_count; ++i) {
                CcMoney deposit = sim->kingdoms[i].treasury < 160 ?
                                  sim->kingdoms[i].treasury : 160;
                sim->kingdoms[i].treasury -= deposit;
                sim->iron_ledger_reserve += deposit;
                sim->kingdoms[i].iron_ledger_debt = 0;
            }
        }
        CcSimInitializeDragonEcology(sim);
        CcSimInitializeAnimalEconomy(sim);
        CcSimInitializeCharacters(sim);
        FinishLegacyRuntimeUpgrade(sim);
        return true;
    }
    CcSimInitializeDragonCycle(sim);
    CcSimInitializeHoardRaiders(sim);
    if (legacy_version == 6U && sim->dragon.stolen_outstanding > 0 &&
        sim->dragon.theft_actor_id == 0U) {
        const CcEvent *theft = CcSimEvent(sim, sim->dragon.hoard_event_id);
        sim->dragon.theft_actor_id = theft != NULL ? theft->subject_id :
                                      sim->player.id;
    }
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        CcSettlement *place = &sim->settlements[i];
        if (legacy_version < 8U) {
            place->market_coins = 60 + place->population / 20 +
                                  place->prosperity * 2;
            place->war_chest = 0;
        }
        place->price[CC_GOOD_WEAPONS] = 24;
        place->price[CC_GOOD_GOLD] = 40;
        place->price[CC_GOOD_GEMS] = 70;
        place->reserve_target[CC_GOOD_WEAPONS] =
            place->function == CC_SETTLEMENT_FORTRESS ? 14 : 4;
        place->reserve_target[CC_GOOD_GOLD] = 1;
        place->reserve_target[CC_GOOD_GEMS] = 1;
        bool needs_field = place->function == CC_SETTLEMENT_FARMING ||
                           place->function == CC_SETTLEMENT_MINING ||
                           place->function == CC_SETTLEMENT_FORTRESS ||
                           place->function == CC_SETTLEMENT_CAPITAL;
        if (needs_field &&
            !CcSettlementHasService(place, CC_SERVICE_FARM) &&
            CcSettlementServiceCount(place) <
                CcSettlementServiceCapacity(place->size)) {
            place->service_mask |=
                UINT32_C(1) << (uint32_t)CC_SERVICE_FARM;
        }
        if (CcSettlementHasService(place, CC_SERVICE_FARM)) {
            place->field_yield = place->function == CC_SETTLEMENT_FARMING ?
                                 100 : place->function == CC_SETTLEMENT_CAPITAL ?
                                 90 : place->function == CC_SETTLEMENT_FORTRESS ?
                                 85 : 70;
            if (place->production[CC_GOOD_FOOD] == 0) {
                place->production[CC_GOOD_FOOD] = 16;
            }
        }
        if (CcSettlementHasService(place, CC_SERVICE_MINE)) {
            place->iron_deposit = 8000 + i * 800;
            place->gold_seam = true;
            place->gem_seam = place->function == CC_SETTLEMENT_MINING;
        }
        if (CcSettlementHasService(place, CC_SERVICE_SMITHY)) {
            place->production[CC_GOOD_WEAPONS] =
                place->function == CC_SETTLEMENT_FORTRESS ? 2 : 1;
        }
        if (CcSettlementHasService(place, CC_SERVICE_BARRACKS)) {
            place->stock[CC_GOOD_WEAPONS] = 6;
        }
        place->consumption[CC_GOOD_IRON] = 0;
        place->consumption[CC_GOOD_TOOLS] = 0;
    }
    sim->goblins.lair_settlement_id = sim->settlements[
        sim->settlement_count > 3 ? 3 : 0].id;
    sim->goblins.raid_motive = sim->goblins.tribute_phase ==
        CC_GOBLIN_TRIBUTE_IDLE ? CC_GOBLIN_RAID_NONE :
        CC_GOBLIN_RAID_DRAGON_TRIBUTE;
    sim->goblins.lair_stock[CC_GOOD_FOOD] = 12;
    sim->goblins.lair_stock[CC_GOOD_TOOLS] = 2;
    sim->goblins.lair_stock[CC_GOOD_WEAPONS] = 3;
    if (sim->goblins.tribute_phase == CC_GOBLIN_TRIBUTE_RETURNING) {
        sim->goblins.tribute_phase = CC_GOBLIN_TRIBUTE_TO_DRAGON;
        sim->goblins.tribute_target_id = sim->dragon.lair_settlement_id;
    }
    if (sim->iron_ledger_reserve == 0) {
        for (int32_t i = 0; i < sim->kingdom_count; ++i) {
            CcMoney deposit = sim->kingdoms[i].treasury < 160 ?
                              sim->kingdoms[i].treasury : 160;
            sim->kingdoms[i].treasury -= deposit;
            sim->iron_ledger_reserve += deposit;
            sim->kingdoms[i].iron_ledger_debt = 0;
        }
    }
    TunePhysicalReserveTargets(sim);
    CcSimInitializeDragonEcology(sim);
    CcSimInitializeAnimalEconomy(sim);
    CcSimInitializeCharacters(sim);
    FinishLegacyRuntimeUpgrade(sim);
    return true;
}

bool CcSaveUpgradeLegacyRuntime(CcSim *sim,
                                 char *error, size_t error_capacity)
{
    uint32_t legacy_version = sim->schema_version;
    if (!UpgradeLegacyRuntimeSchema(sim, error, error_capacity)) return false;
    if (legacy_version < 57U) {
        /* Older saves identify hearts by their original generated name. */
        char name[CC_MAP_NAME_CAPACITY];
        (void)snprintf(name, sizeof(name), "Wyrmheart of %.20s", sim->dragon.name);
        const CcTreasure *first = NULL;
        for (int32_t i = 0; i < sim->treasure_count; ++i) {
            const CcTreasure *heart = &sim->treasures[i];
            if (strcmp(heart->name, name) == 0 &&
                heart->created_day >= sim->current_day - sim->dragon.age_days &&
                (first == NULL || heart->created_day < first->created_day)) {
                first = heart;
            }
        }
        sim->dragon.wyrmheart_id = first != NULL ? first->id : 0U;
        if (first == NULL && sim->dragon.life_stage == CC_DRAGON_STAGE_DEEP_WYRM) {
            /* Reserve the identity of an already formed, missing heart. */
            sim->dragon.wyrmheart_id = CcMakeId(CC_ENTITY_TREASURE, sim->next_entity_serial++);
        }
    }
    if (legacy_version < 75U) CcSimInitializeGoblinPolitics(sim);
    if (legacy_version < 62U) CcSimUpgradeKnowledgeSourceNames(sim);
    if (legacy_version < 51U) CcSimSeedCommonPonyHerds(sim);
    if (legacy_version < 52U) CcSimUnharnessSecondDraftAnimal(sim);
    /* Legacy upgrades can seed residents and situation casts through
       separate paths; make the final living cast unique before validation. */
    MakeLegacyCharacterNamesUnique(sim);
    return true;
}

