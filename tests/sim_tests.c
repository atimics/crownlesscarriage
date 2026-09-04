#include "sim/cc_sim.h"

#include "test_support.h"
#include <stdio.h>
#include <string.h>

static void AdvanceTravellingJourney(CcSim *sim)
{
    char error[160];
    while (sim->journey.active) {
        if (sim->journey.phase == CC_JOURNEY_PHASE_TRAVELLING) {
            CcSimAdvanceRuntimeTicks(sim, CC_WORLD_TICKS_PER_SECOND);
        } else if (sim->journey.phase == CC_JOURNEY_PHASE_RESTING) {
            CcCommand rest = {
                .kind = CcSimJourneyStop(sim) == CC_JOURNEY_STOP_MIDDAY ?
                    CC_COMMAND_TAKE_JOURNEY_BREAK : CC_COMMAND_MAKE_CAMP
            };
            CC_CHECK(CcSimApply(sim, &rest, error, sizeof(error)));
        } else {
            break;
        }
    }
}

static void ApplySequence(CcSim *sim)
{
    char error[160];
    CcCommand buy = {
        .kind = CC_COMMAND_TRADE,
        .good = CC_GOOD_FOOD,
        .amount = 3
    };
    CC_CHECK(CcSimApply(sim, &buy, error, sizeof(error)));
    CcCommand travel = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = sim->settlements[1].id
    };
    CC_CHECK(CcSimApply(sim, &travel, error, sizeof(error)));
    AdvanceTravellingJourney(sim);
    CC_CHECK(!sim->journey.active);
    CcSimAdvanceDays(sim, 17);
}

static CcSituation *FirstActiveSituation(CcSim *sim, int32_t excluded)
{
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        if (i != excluded &&
            sim->situations[i].status == CC_SITUATION_ACTIVE) {
            return &sim->situations[i];
        }
    }
    return NULL;
}

static CcSituation *PreparePromisedJourney(CcSim *sim, char *error,
                                           size_t error_capacity)
{
    CcSituation *situation = FirstActiveSituation(sim, -1);
    CC_CHECK(situation != NULL);
    situation->kind = CC_SITUATION_RELIEF_DELIVERY;
    situation->target_id = sim->settlements[1].id;
    situation->good = CC_GOOD_FOOD;
    situation->quantity = 1;
    situation->progress = 0;
    situation->reward = 20;
    situation->deadline_day = sim->current_day + 40;
    sim->player.cargo[CC_GOOD_FOOD] = 0;
    CcCommand accept = {
        .kind = CC_COMMAND_ACCEPT_SITUATION,
        .target_id = situation->id
    };
    CC_CHECK(CcSimApply(sim, &accept, error, error_capacity));
    sim->routes[0].closed = true;
    const CcMap *map = CcSimMapForRoute(sim, sim->routes[0].id,
                                        sim->player.id);
    CC_CHECK(map != NULL);
    CcCommand travel = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = sim->settlements[1].id
    };
    CC_CHECK(CcSimApply(sim, &travel, error, error_capacity));
    CC_CHECK(sim->journey.active);
    CC_CHECK(sim->journey.phase == CC_JOURNEY_PHASE_TRAVELLING);
    AdvanceTravellingJourney(sim);
    CC_CHECK(sim->journey.phase == CC_JOURNEY_PHASE_BLOCKED);
    return situation;
}

static int32_t CountLoreRecordsForParent(const CcSim *sim, CcId parent)
{
    int32_t count = 0;
    for (int32_t i = 0; i < sim->event_count; ++i) {
        const CcEvent *event = CcSimRecentEvent(sim, i);
        if (event != NULL && event->kind == CC_EVENT_LORE_RECORDED &&
            event->parent_id == parent) count += 1;
    }
    return count;
}

static bool IsArchiveVolumeName(const char *name)
{
    return strncmp(name, "Chronicle ", 10) == 0 ||
           strncmp(name, "Ledger ", 7) == 0 ||
           strncmp(name, "Annal ", 6) == 0 ||
           strncmp(name, "Register ", 9) == 0 ||
           strncmp(name, "Codex of ", 9) == 0;
}

static void PrepareArchiveWeek(CcSim *sim, CcMoney reserve,
                               int32_t scribes)
{
    CcSimInit(sim, UINT32_C(0xa4c417e));
    sim->current_day = 6;
    sim->iron_ledger_reserve = reserve;
    sim->archives.scribes = scribes;
    sim->archives.lore_stored = 0;
    sim->archives.lore_lost_total = 0;
    sim->archives.last_recorded_day = 0;
    CcSettlement *scriptorium = &sim->settlements[1];
    scriptorium->stock[CC_GOOD_WHEAT] = 100;
    scriptorium->stock[CC_GOOD_PAPER] = CC_MAX_SCRIBES;
    scriptorium->stock[CC_GOOD_TOOLS] = CC_MAX_SCRIBES;
    scriptorium->production[CC_GOOD_PAPER] = 0;
    scriptorium->consumption[CC_GOOD_PAPER] = 0;
    for (int32_t i = 0; i < sim->event_count; ++i) {
        sim->events[i].magnitude = 0;
    }
    int32_t notable = sim->event_count < 3 ? sim->event_count : 3;
    for (int32_t i = 0; i < notable; ++i) {
        sim->events[i].day = 6;
        sim->events[i].kind = CC_EVENT_KINGDOM_ACTION;
        sim->events[i].magnitude = 40 + i;
    }
}

static void CheckArchiveRecording(void)
{
    CcSim unfunded;
    PrepareArchiveWeek(&unfunded, 0, 0);
    CcSimAdvanceDays(&unfunded, 1);
    CC_CHECK(unfunded.archives.scribes == 0);
    CC_CHECK(unfunded.archives.lore_stored == 0);

    CcSim funded;
    PrepareArchiveWeek(&funded, 50, 1);
    int32_t funded_gold = CcSimTrackedGood(&funded, CC_GOOD_GOLD);
    int32_t funded_gems = CcSimTrackedGood(&funded, CC_GOOD_GEMS);
    CcSimAdvanceDays(&funded, 1);
    CC_CHECK(funded.archives.scribes == 1);
    CC_CHECK(funded.archives.lore_stored == 1);
    CC_CHECK(funded.archives.last_recorded_day == 7);
    CC_CHECK(CcSimTrackedGood(&funded, CC_GOOD_GOLD) == funded_gold);
    CC_CHECK(CcSimTrackedGood(&funded, CC_GOOD_GEMS) == funded_gems);

    CcId recorded_parent = 0U;
    for (int32_t i = 0; i < funded.event_count; ++i) {
        const CcEvent *event = CcSimRecentEvent(&funded, i);
        if (event != NULL && event->kind == CC_EVENT_LORE_RECORDED &&
            event->day == 7) {
            recorded_parent = event->parent_id;
            break;
        }
    }
    CC_CHECK(recorded_parent != 0U);
    CC_CHECK(CountLoreRecordsForParent(&funded, recorded_parent) == 1);

    bool physical_record = false;
    for (int32_t i = 0; i < funded.treasure_count; ++i) {
        if (!funded.treasures[i].destroyed &&
            IsArchiveVolumeName(funded.treasures[i].name)) {
            physical_record = true;
        }
    }
    CC_CHECK(physical_record);

    CcSimAdvanceDays(&funded, 7);
    CC_CHECK(CountLoreRecordsForParent(&funded, recorded_parent) == 1);

    CcSim missing_materials;
    PrepareArchiveWeek(&missing_materials, 50, 1);
    for (int32_t i = 0; i < missing_materials.settlement_count; ++i) {
        CcSettlement *settlement = &missing_materials.settlements[i];
        settlement->stock[CC_GOOD_GOLD] = 0;
        settlement->stock[CC_GOOD_GEMS] = 0;
        settlement->gold_seam = false;
        settlement->gem_seam = false;
        settlement->gold_progress = 0;
        settlement->gem_progress = 0;
        settlement->treasure_gold_committed = 0;
        settlement->treasure_gems_committed = 0;
        settlement->treasure_work = 0;
    }
    int32_t treasure_count = missing_materials.treasure_count;
    int32_t missing_gold = CcSimTrackedGood(
        &missing_materials, CC_GOOD_GOLD);
    int32_t missing_gems = CcSimTrackedGood(
        &missing_materials, CC_GOOD_GEMS);
    CcSimAdvanceDays(&missing_materials, 1);
    CC_CHECK(missing_materials.archives.lore_stored == 0);
    CC_CHECK(missing_materials.treasure_count == treasure_count);
    CC_CHECK(CcSimTrackedGood(&missing_materials, CC_GOOD_GOLD) ==
             missing_gold);
    CC_CHECK(CcSimTrackedGood(&missing_materials, CC_GOOD_GEMS) ==
             missing_gems);
}

static void CheckArchiveBindingConservation(void)
{
    CcSim sim;
    PrepareArchiveWeek(&sim, 0, 0);
    sim.treasure_count = CC_MAX_TREASURES - 4;
    CcSettlement *vault = &sim.settlements[0];
    CcSettlement *rival_vault = &sim.settlements[1];
    int32_t archive_gold = 0;
    int32_t archive_gems = 0;
    int32_t archive_lore = 0;
    int32_t archive_value = 0;
    for (int32_t i = 0; i < sim.treasure_count; ++i) {
        CcTreasure *treasure = &sim.treasures[i];
        bool bound_volume = i >= 1 && i <= 4;
        *treasure = (CcTreasure){
            .id = CcMakeId(CC_ENTITY_TREASURE,
                           sim.next_entity_serial++),
            .maker_settlement_id = vault->id,
            .owner_id = i == 0 ? rival_vault->id : vault->id,
            .location_id = vault->id,
            .gold_content = bound_volume ? i : 1,
            .gem_content = bound_volume ? i + 1 : 1,
            .craft_work = bound_volume ? i : 1,
            .appraised_value = bound_volume ? 20 + i : 1,
            .created_day = 1
        };
        (void)snprintf(treasure->name, sizeof(treasure->name),
                       i <= 4 ? "Chronicle test %d" : "Vault piece %d", i);
        if (bound_volume) {
            archive_gold += treasure->gold_content;
            archive_gems += treasure->gem_content;
            archive_lore += treasure->craft_work;
            archive_value += treasure->appraised_value;
        }
    }
    for (int32_t i = 0; i < sim.settlement_count; ++i) {
        sim.settlements[i].stock[CC_GOOD_GOLD] = 0;
        sim.settlements[i].stock[CC_GOOD_GEMS] = 0;
        sim.settlements[i].gold_seam = false;
        sim.settlements[i].gem_seam = false;
        sim.settlements[i].treasure_gold_committed = 0;
        sim.settlements[i].treasure_gems_committed = 0;
        sim.settlements[i].treasure_work = 0;
    }
    int32_t gold_before = CcSimTrackedGood(&sim, CC_GOOD_GOLD);
    int32_t gems_before = CcSimTrackedGood(&sim, CC_GOOD_GEMS);

    CcSimAdvanceDays(&sim, 1);

    CC_CHECK(CcSimTrackedGood(&sim, CC_GOOD_GOLD) == gold_before);
    CC_CHECK(CcSimTrackedGood(&sim, CC_GOOD_GEMS) == gems_before);
    const CcTreasure *codex = NULL;
    for (int32_t i = 0; i < sim.treasure_count; ++i) {
        if (!sim.treasures[i].destroyed &&
            strncmp(sim.treasures[i].name, "Codex of ", 9) == 0) {
            codex = &sim.treasures[i];
            break;
        }
    }
    CC_CHECK(codex != NULL);
    CC_CHECK(codex->owner_id == vault->id);
    CC_CHECK(codex->location_id == vault->id);
    CC_CHECK(codex->gold_content == archive_gold);
    CC_CHECK(codex->gem_content == archive_gems);
    CC_CHECK(codex->craft_work == archive_lore);
    CC_CHECK(codex->appraised_value == archive_value);
    CC_CHECK(!sim.treasures[0].destroyed);
    CC_CHECK(sim.treasures[0].owner_id == rival_vault->id);
    char error[160];
    CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
}

static void CheckLongArchiveConservation(void)
{
    CcSim sim;
    CcSimInit(&sim, UINT32_C(42));
    sim.iron_ledger_reserve = 300;
    sim.dragon_campaign.cooldown_days = 10000;
    sim.hoard_raiders.cooldown_days = 10000;
    for (int32_t i = 0; i < sim.settlement_count; ++i) {
        CcSettlement *settlement = &sim.settlements[i];
        settlement->stock[CC_GOOD_GOLD] += 64;
        settlement->stock[CC_GOOD_GEMS] += 64;
        settlement->gold_seam = false;
        settlement->gem_seam = false;
        settlement->gold_progress = 0;
        settlement->gem_progress = 0;
    }
    int32_t gold_before = CcSimTrackedGood(&sim, CC_GOOD_GOLD);
    int32_t gems_before = CcSimTrackedGood(&sim, CC_GOOD_GEMS);

    CcSimAdvanceDays(&sim, 364);

    CC_CHECK(sim.archives.lore_stored > 0);
    CC_CHECK(CcSimTrackedGood(&sim, CC_GOOD_GOLD) == gold_before);
    CC_CHECK(CcSimTrackedGood(&sim, CC_GOOD_GEMS) == gems_before);
    char error[160];
    CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
}

static int32_t CountCharacterLifeEvents(const CcSim *sim,
                                        CcEventKind kind)
{
    int32_t count = 0;
    for (int32_t i = 0; i < sim->event_count; ++i) {
        const CcEvent *event = CcSimRecentEvent(sim, i);
        if (event != NULL && event->kind == kind) count += 1;
    }
    return count;
}

static void CheckCharacterLifecycles(void)
{
    char first_name[CC_NAME_CAPACITY];
    char repeated_name[CC_NAME_CAPACITY];
    char second_name[CC_NAME_CAPACITY];
    CcGenerateCharacterName(UINT32_C(0x1a2b3c4d),
                            CcMakeId(CC_ENTITY_SETTLEMENT, 7U),
                            3, 11U, first_name);
    CcGenerateCharacterName(UINT32_C(0x1a2b3c4d),
                            CcMakeId(CC_ENTITY_SETTLEMENT, 7U),
                            3, 11U, repeated_name);
    CcGenerateCharacterName(UINT32_C(0x1a2b3c4d),
                            CcMakeId(CC_ENTITY_SETTLEMENT, 7U),
                            3, 12U, second_name);
    CC_CHECK(strcmp(first_name, repeated_name) == 0);
    CC_CHECK(strchr(first_name, ' ') != NULL);
    CC_CHECK(strcmp(first_name, second_name) != 0);

    CcSim first;
    CcSim second;
    CcSimInit(&first, UINT32_C(0x11fe71fe));
    CcSimInit(&second, UINT32_C(0x11fe71fe));
    CC_CHECK(first.character_count == CC_MAX_CHARACTERS);
    for (int32_t i = 0; i < first.character_count; ++i) {
        CC_CHECK(first.characters[i].birth_day <= first.current_day);
        CC_CHECK(first.characters[i].death_day > first.current_day);
        CC_CHECK(CcCharacterAgeYears(&first, &first.characters[i]) >= 22);
    }

    int32_t slot = first.character_count - 1;
    CcId ancestor_id = first.characters[slot].id;
    int32_t ancestor_generation = first.characters[slot].generation;
    char ancestor_name[CC_NAME_CAPACITY];
    (void)snprintf(ancestor_name, sizeof(ancestor_name), "%s",
                   first.characters[slot].name);
    first.characters[slot].death_day = first.current_day + 1;
    second.characters[slot].death_day = second.current_day + 1;
    CcSimAdvanceDays(&first, 1);
    CcSimAdvanceDays(&second, 1);
    const CcCharacter *successor = &first.characters[slot];
    const char *ancestor_family = strrchr(ancestor_name, ' ');
    const char *successor_family = strrchr(successor->name, ' ');
    CC_CHECK(CcSimHash(&first) == CcSimHash(&second));
    CC_CHECK(successor->id != ancestor_id);
    CC_CHECK(successor->ancestor_id == ancestor_id);
    CC_CHECK(successor->generation == ancestor_generation + 1);
    CC_CHECK(successor->birth_day == first.current_day);
    CC_CHECK(CcCharacterAgeYears(&first, successor) == 0);
    CC_CHECK(ancestor_family != NULL && successor_family != NULL);
    CC_CHECK(strcmp(ancestor_family, successor_family) == 0);
    CC_CHECK(first.character_births == 1);
    CC_CHECK(first.character_deaths == 1);
    CC_CHECK(CountCharacterLifeEvents(
                 &first, CC_EVENT_CHARACTER_BORN) == 1);
    CC_CHECK(CountCharacterLifeEvents(
                 &first, CC_EVENT_CHARACTER_DIED) == 1);
    char error[160];
    CC_CHECK(CcSimValidate(&first, error, sizeof(error)));

    CcSim forged = first;
    bool found_death = false;
    for (int32_t i = 0; i < CC_MAX_EVENTS; ++i) {
        if (forged.events[i].id != 0U &&
            forged.events[i].kind == CC_EVENT_CHARACTER_DIED) {
            forged.events[i].actor_id = CcMakeId(
                CC_ENTITY_CHARACTER, forged.next_entity_serial + 4U);
            found_death = true;
            break;
        }
    }
    CC_CHECK(found_death);
    CC_CHECK(!CcSimValidate(&forged, error, sizeof(error)));

    forged = first;
    forged.characters[slot].ancestor_id = CcMakeId(
        CC_ENTITY_CHARACTER, forged.next_entity_serial + 5U);
    CC_CHECK(!CcSimValidate(&forged, error, sizeof(error)));
}

static void CheckRoadDistrictSites(void)
{
    CcSim sim;
    CcSimInit(&sim, UINT32_C(0xd15771c7));
    CC_CHECK(sim.road_site_count == CC_MAX_ROAD_SITES);
    bool found_farm = false;
    bool found_quarry = false;
    bool found_mill = false;
    for (int32_t route_slot = 0; route_slot < sim.route_count; ++route_slot) {
        const CcRoute *route = &sim.routes[route_slot];
        const CcRoadSite *road_house =
            CcSimRoadHouseSite(&sim, route->id);
        CC_CHECK(road_house != NULL);
        CC_CHECK(strcmp(CcSimRoadHouseName(&sim, route->id),
                        road_house->name) == 0);
        int32_t route_sites = 0;
        for (int32_t site_slot = 0;
             site_slot < sim.road_site_count; ++site_slot) {
            const CcRoadSite *site = CcSimRoadSiteAt(&sim, site_slot);
            CC_CHECK(site != NULL);
            CC_CHECK(CcSimRoadSite(&sim, site->id) == site);
            if (site->route_id == route->id) route_sites += 1;
            found_farm = found_farm || site->kind == CC_ROAD_SITE_FARM;
            found_quarry = found_quarry || site->kind == CC_ROAD_SITE_QUARRY;
            found_mill = found_mill || site->kind == CC_ROAD_SITE_MILL;
            CC_CHECK(!site->accessible);
        }
        CC_CHECK(route_sites == 3);
    }
    CC_CHECK(found_farm && found_quarry && found_mill);
    uint64_t hash = CcSimHash(&sim);
    sim.road_sites[0].condition -= 1;
    CC_CHECK(CcSimHash(&sim) != hash);
    char error[160];
    sim.road_sites[0].accessible = true;
    CC_CHECK(!CcSimValidate(&sim, error, sizeof(error)));
}

int main(void)
{
    CcSim first;
    CcSim second;
    char error[160];
    CcSimInit(&first, UINT32_C(0x12345678));
    CheckRoadDistrictSites();
    CcSimInit(&second, UINT32_C(0x12345678));
    CC_CHECK(CcSimHash(&first) == CcSimHash(&second));
    CheckArchiveRecording();
    CheckArchiveBindingConservation();
    CheckLongArchiveConservation();
    CheckCharacterLifecycles();
    CC_CHECK(first.character_count > 0);
    CC_CHECK(first.character_count == second.character_count);
    CC_CHECK(first.characters[0].id == second.characters[0].id);
    CC_CHECK(first.characters[0].appearance_seed ==
             second.characters[0].appearance_seed);

    CcSim character_sim;
    CcSimInit(&character_sim, UINT32_C(0xc4a4ac7e));
    CcSituation *personal_situation = NULL;
    for (int32_t i = 0; i < character_sim.situation_count; ++i) {
        if (character_sim.situations[i].status == CC_SITUATION_ACTIVE &&
            character_sim.situations[i].kind !=
                CC_SITUATION_RELIEF_DELIVERY) {
            personal_situation = &character_sim.situations[i];
            break;
        }
    }
    CC_CHECK(personal_situation != NULL);
    const CcCharacter *personal_character =
        CcSimSituationAffectedCharacter(&character_sim, personal_situation);
    CC_CHECK(personal_character != NULL);
    CcId personal_character_id = personal_character->id;
    uint32_t personal_appearance = personal_character->appearance_seed;
    character_sim.player.location_id =
        personal_character->current_settlement_id;
    character_sim.carriage.location_id = character_sim.player.location_id;
    CcCommand listen = {
        .kind = CC_COMMAND_CHARACTER_RESPONSE,
        .target_id = personal_situation->id,
        .amount = CC_CHARACTER_RESPONSE_LISTEN
    };
    CC_CHECK(CcSimApply(&character_sim, &listen, error, sizeof(error)));
    personal_character = CcSimCharacter(&character_sim, personal_character_id);
    CC_CHECK(personal_character != NULL);
    CC_CHECK(CcCharacterRemembers(
        personal_character, CC_CHARACTER_MEMORY_MET_PLAYER,
        personal_situation->id));
    CC_CHECK(personal_character->player_disposition == 2);
    CC_CHECK(CcSimRecentEvent(&character_sim, 0)->kind ==
             CC_EVENT_CHARACTER_INTERACTION);
    CcCommand pledge = listen;
    pledge.amount = CC_CHARACTER_RESPONSE_PLEDGE_HELP;
    CC_CHECK(CcSimApply(&character_sim, &pledge, error, sizeof(error)));
    personal_character = CcSimCharacter(&character_sim, personal_character_id);
    CC_CHECK(character_sim.player.accepted_situation_id ==
             personal_situation->id);
    CC_CHECK(personal_character->activity ==
             CC_CHARACTER_ACTIVITY_PREPARING);
    CC_CHECK(personal_character->appearance_seed == personal_appearance);
    CC_CHECK(CcCharacterRemembers(
        personal_character, CC_CHARACTER_MEMORY_PLAYER_PROMISED,
        personal_situation->id));
    uint64_t remembered_hash = CcSimHash(&character_sim);
    CC_CHECK(!CcSimApply(&character_sim, &pledge, error, sizeof(error)));
    CC_CHECK(CcSimHash(&character_sim) == remembered_hash);
    CcCommand abandon_person = {
        .kind = CC_COMMAND_ABANDON_SITUATION,
        .target_id = personal_situation->id
    };
    CC_CHECK(CcSimApply(&character_sim, &abandon_person,
                        error, sizeof(error)));
    personal_character = CcSimCharacter(
        &character_sim, personal_character_id);
    CC_CHECK(CcCharacterRemembers(
        personal_character, CC_CHARACTER_MEMORY_PLAYER_WITHDREW,
        personal_situation->id));
    CC_CHECK(CcSimValidate(&character_sim, error, sizeof(error)));

    CcSim realtime;
    CcSimInit(&realtime, UINT32_C(0x71ae71e));
    CcId realtime_origin = realtime.player.location_id;
    CcId realtime_destination = realtime.settlements[1].id;
    int32_t realtime_departure_day = realtime.current_day;
    CcMoney realtime_coins = realtime.player.coins;
    CcTravelPreview opening_preview = {0};
    CC_CHECK(CcSimTravelPreview(
        &realtime, realtime_destination, &opening_preview,
        error, sizeof(error)));
    CC_CHECK(opening_preview.opening_half_day);
    CC_CHECK(opening_preview.travel_watches == 1);
    CC_CHECK(opening_preview.overnight_stops == 0);
    CcCommand realtime_travel = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = realtime_destination
    };
    CC_CHECK(CcSimApply(&realtime, &realtime_travel,
                        error, sizeof(error)));
    CC_CHECK(realtime.player.location_id == realtime_origin);
    CC_CHECK(realtime.current_day == realtime_departure_day);
    CC_CHECK(realtime.player.coins ==
             realtime_coins - realtime.journey.fare_reserved);
    CC_CHECK(realtime.carriage.mode == CC_CARRIAGE_MOVING);
    int32_t travelling_food = realtime.player.cargo[CC_GOOD_FOOD];
    int32_t origin_food = realtime.settlements[0].stock[CC_GOOD_FOOD];
    CcMoney travelling_coins = realtime.player.coins;
    CcCommand roadside_trade = {
        .kind = CC_COMMAND_TRADE,
        .good = CC_GOOD_FOOD,
        .amount = 1
    };
    CC_CHECK(!CcSimApply(&realtime, &roadside_trade,
                         error, sizeof(error)));
    CC_CHECK(realtime.player.cargo[CC_GOOD_FOOD] == travelling_food);
    CC_CHECK(realtime.settlements[0].stock[CC_GOOD_FOOD] == origin_food);
    CC_CHECK(realtime.player.coins == travelling_coins);
    CcSimAdvanceRuntimeTicks(&realtime, 12);
    CC_CHECK(realtime.clock.tick == 12U);
    CC_CHECK(realtime.clock.minute_subticks ==
             CC_TRAVEL_GAME_MINUTES_PER_SECOND * 12);
    CC_CHECK(realtime.carriage.progress_milli > 0);
    AdvanceTravellingJourney(&realtime);
    CC_CHECK(!realtime.journey.active);
    CC_CHECK(realtime.player.location_id == realtime_destination);
    CC_CHECK(realtime.current_day == realtime_departure_day);
    CC_CHECK(realtime.clock.minute_subticks == CC_WORLD_WATCH_SUBTICKS);
    CC_CHECK(realtime.carriage.mode == CC_CARRIAGE_PARKED);

    CcTravelPreview return_preview = {0};
    CC_CHECK(CcSimTravelPreview(
        &realtime, realtime_origin, &return_preview,
        error, sizeof(error)));
    CC_CHECK(!return_preview.opening_half_day);
    CC_CHECK(return_preview.travel_watches >= 3);
    CC_CHECK(return_preview.overnight_stops >= 1);
    CC_CHECK(return_preview.departure_wait_minutes == 16 * 60);
    CC_CHECK(return_preview.road_house_name != NULL);
    CC_CHECK(return_preview.road_house_distance_miles > 0);
    CC_CHECK(return_preview.road_house_cost > 0);

    int32_t first_house_distance = CcSimRoadHouseDistanceMiles(
        &realtime, realtime.routes[0].id);
    bool varied_house_distance = false;
    for (int32_t route_index = 1;
         route_index < realtime.route_count; ++route_index) {
        if (CcSimRoadHouseDistanceMiles(
                &realtime, realtime.routes[route_index].id) !=
            first_house_distance) {
            varied_house_distance = true;
        }
    }
    CC_CHECK(varied_house_distance);

    CcCommand return_travel = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = realtime_origin
    };
    int32_t return_preparation_day = realtime.current_day;
    CC_CHECK(CcSimApply(&realtime, &return_travel,
                        error, sizeof(error)));
    CC_CHECK(realtime.journey.departure_day == return_preparation_day + 1);
    CC_CHECK(realtime.clock.minute_subticks == 0);
    realtime.journey.ambush_pending = false;
    while (realtime.journey.phase == CC_JOURNEY_PHASE_TRAVELLING) {
        CcSimAdvanceRuntimeTicks(&realtime, CC_WORLD_TICKS_PER_SECOND);
    }
    CC_CHECK(CcSimJourneyStop(&realtime) == CC_JOURNEY_STOP_MIDDAY);
    int32_t break_danger = realtime.journey.danger;
    int32_t break_fatigue = realtime.horse_team[0].fatigue;
    CcSim pressed_on = realtime;
    CcCommand take_break = {.kind = CC_COMMAND_TAKE_JOURNEY_BREAK};
    CcCommand press_on = {.kind = CC_COMMAND_PRESS_ON};
    CC_CHECK(CcSimApply(&realtime, &take_break, error, sizeof(error)));
    CC_CHECK(CcSimApply(&pressed_on, &press_on, error, sizeof(error)));
    CC_CHECK(realtime.journey.danger < break_danger);
    CC_CHECK(realtime.horse_team[0].fatigue < break_fatigue);
    CC_CHECK(pressed_on.journey.danger > break_danger);
    CC_CHECK(pressed_on.horse_team[0].fatigue > break_fatigue);
    while (realtime.journey.phase == CC_JOURNEY_PHASE_TRAVELLING) {
        CcSimAdvanceRuntimeTicks(&realtime, CC_WORLD_TICKS_PER_SECOND);
    }
    CC_CHECK(CcSimJourneyStop(&realtime) == CC_JOURNEY_STOP_OVERNIGHT);
    CC_CHECK(CcSimJourneyRoadHouseAvailable(&realtime));
    realtime.horse_team[0].fatigue = 30;
    CcSim camped = realtime;
    CcSim lodged = realtime;
    int32_t camp_day = camped.current_day;
    int32_t camp_clock = camped.clock.minute_subticks;
    int32_t stop_fatigue = camped.horse_team[0].fatigue;
    CcMoney lodging_coins = lodged.player.coins;
    CcMoney lodging_cost = CcSimRoadHouseCost(
        &lodged, lodged.journey.route_id);
    CcCommand make_camp = {.kind = CC_COMMAND_MAKE_CAMP};
    CcCommand lodge = {.kind = CC_COMMAND_LODGE_ROAD_HOUSE};
    CC_CHECK(CcSimApply(&camped, &make_camp, error, sizeof(error)));
    CC_CHECK(CcSimApply(&lodged, &lodge, error, sizeof(error)));
    CC_CHECK(camped.current_day > camp_day ||
             camped.clock.minute_subticks > camp_clock);
    CC_CHECK(camped.horse_team[0].fatigue < stop_fatigue);
    CC_CHECK(lodged.horse_team[0].fatigue <
             camped.horse_team[0].fatigue);
    CC_CHECK(lodged.player.coins == lodging_coins - lodging_cost);
    CC_CHECK(CcSimRecentEvent(&camped, 0)->kind == CC_EVENT_JOURNEY_CAMP);
    CC_CHECK(CcSimRecentEvent(&lodged, 0)->kind ==
             CC_EVENT_ROAD_HOUSE_LODGING);
    CC_CHECK(CcSimValidate(&camped, error, sizeof(error)));
    CC_CHECK(CcSimValidate(&lodged, error, sizeof(error)));

    CcSim careful_pace;
    CcSim steady_pace;
    CcSim push_pace;
    CcSimInit(&careful_pace, UINT32_C(0xca9e));
    CcSimInit(&steady_pace, UINT32_C(0xca9e));
    CcSimInit(&push_pace, UINT32_C(0xca9e));
    CcCommand pace_travel = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = careful_pace.settlements[1].id
    };
    CC_CHECK(CcSimApply(&careful_pace, &pace_travel,
                        error, sizeof(error)));
    CC_CHECK(CcSimJourneyWatchNumber(&careful_pace) == 1);
    pace_travel.target_id = steady_pace.settlements[1].id;
    CC_CHECK(CcSimApply(&steady_pace, &pace_travel,
                        error, sizeof(error)));
    pace_travel.target_id = push_pace.settlements[1].id;
    CC_CHECK(CcSimApply(&push_pace, &pace_travel,
                        error, sizeof(error)));
    careful_pace.journey.ambush_pending = false;
    steady_pace.journey.ambush_pending = false;
    push_pace.journey.ambush_pending = false;
    CcCommand set_careful = {
        .kind = CC_COMMAND_SET_JOURNEY_PACE,
        .amount = CC_JOURNEY_PACE_CAREFUL
    };
    CcCommand set_push = {
        .kind = CC_COMMAND_SET_JOURNEY_PACE,
        .amount = CC_JOURNEY_PACE_PUSH
    };
    CC_CHECK(CcSimApply(&careful_pace, &set_careful,
                        error, sizeof(error)));
    CC_CHECK(CcSimApply(&push_pace, &set_push,
                        error, sizeof(error)));
    CC_CHECK(CcSimJourneyEtaMinutes(&careful_pace) >
             CcSimJourneyEtaMinutes(&steady_pace));
    CC_CHECK(CcSimJourneyEtaMinutes(&steady_pace) >
             CcSimJourneyEtaMinutes(&push_pace));
    int32_t careful_condition = careful_pace.carriage.condition;
    int32_t push_condition = push_pace.carriage.condition;
    int32_t careful_fatigue = careful_pace.horse_team[0].fatigue;
    int32_t push_fatigue = push_pace.horse_team[0].fatigue;
    CcSimAdvanceRuntimeTicks(&careful_pace, 480);
    CcSimAdvanceRuntimeTicks(&steady_pace, 480);
    CcSimAdvanceRuntimeTicks(&push_pace, 480);
    CC_CHECK(careful_pace.carriage.progress_milli <
             steady_pace.carriage.progress_milli);
    CC_CHECK(steady_pace.carriage.progress_milli <
             push_pace.carriage.progress_milli);
    CC_CHECK(careful_pace.carriage.condition == careful_condition);
    AdvanceTravellingJourney(&careful_pace);
    AdvanceTravellingJourney(&steady_pace);
    AdvanceTravellingJourney(&push_pace);
    CC_CHECK(careful_pace.carriage.condition >= careful_condition - 1);
    CC_CHECK(push_pace.carriage.condition < push_condition);
    CC_CHECK(careful_pace.horse_team[0].fatigue - careful_fatigue <
             push_pace.horse_team[0].fatigue - push_fatigue);

    CcSim warned_road;
    CcSimInit(&warned_road, UINT32_C(0x5ca17));
    CcCommand warned_travel = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = warned_road.settlements[1].id
    };
    CC_CHECK(CcSimApply(&warned_road, &warned_travel,
                        error, sizeof(error)));
    warned_road.journey.ambush_pending = true;
    warned_road.journey.ambush_warned = false;
    warned_road.journey.ambush_resolved = false;
    warned_road.journey.encounter_triggered = false;
    while (!warned_road.journey.ambush_warned) {
        CcSimAdvanceRuntimeTicks(&warned_road,
                                 CC_WORLD_TICKS_PER_SECOND);
    }
    CC_CHECK(CcSimRecentEvent(&warned_road, 0)->kind ==
             CC_EVENT_JOURNEY_WARNING);
    CcSim careful_escape = warned_road;
    CcSim warned_block = warned_road;
    CcMoney warning_coins = warned_road.player.coins;
    int32_t warning_cargo = CcPlayerCargoUsed(&warned_road.player);
    CC_CHECK(CcSimApply(&careful_escape, &set_careful,
                        error, sizeof(error)));
    while (!careful_escape.journey.ambush_resolved) {
        CcSimAdvanceRuntimeTicks(&careful_escape,
                                 CC_WORLD_TICKS_PER_SECOND);
    }
    CC_CHECK(careful_escape.journey.phase == CC_JOURNEY_PHASE_TRAVELLING);
    CC_CHECK(CcSimRecentEvent(&careful_escape, 0)->kind ==
             CC_EVENT_AMBUSH_EVADED);
    CC_CHECK(careful_escape.player.coins == warning_coins);
    CC_CHECK(CcPlayerCargoUsed(&careful_escape.player) == warning_cargo);
    CC_CHECK(CcSimApply(&warned_block, &set_push,
                        error, sizeof(error)));
    while (warned_block.journey.phase == CC_JOURNEY_PHASE_TRAVELLING) {
        CcSimAdvanceRuntimeTicks(&warned_block,
                                 CC_WORLD_TICKS_PER_SECOND);
    }
    CC_CHECK(warned_block.journey.phase == CC_JOURNEY_PHASE_BLOCKED);
    CC_CHECK(CcSimRecentEvent(&warned_block, 0)->kind ==
             CC_EVENT_JOURNEY_ENCOUNTER);
    CC_CHECK(warned_block.player.coins == warning_coins);
    CC_CHECK(CcPlayerCargoUsed(&warned_block.player) == warning_cargo);
    CC_CHECK(CcSimValidate(&careful_escape, error, sizeof(error)));
    CC_CHECK(CcSimValidate(&warned_block, error, sizeof(error)));

    CcSim fine_ticks;
    CcSim batched_ticks;
    CcSimInit(&fine_ticks, UINT32_C(0xf17ed));
    CcSimInit(&batched_ticks, UINT32_C(0xf17ed));
    CcCommand batching_travel = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = fine_ticks.settlements[1].id
    };
    CC_CHECK(CcSimApply(&fine_ticks, &batching_travel,
                        error, sizeof(error)));
    batching_travel.target_id = batched_ticks.settlements[1].id;
    CC_CHECK(CcSimApply(&batched_ticks, &batching_travel,
                        error, sizeof(error)));
    for (int32_t tick = 0; tick < 1200; ++tick) {
        CcSimAdvanceRuntimeTicks(&fine_ticks, 1);
    }
    for (int32_t batch = 0; batch < 20; ++batch) {
        CcSimAdvanceRuntimeTicks(&batched_ticks,
                                 CC_WORLD_TICKS_PER_SECOND);
    }
    CC_CHECK(CcSimHash(&fine_ticks) == CcSimHash(&batched_ticks));
    ApplySequence(&first);
    ApplySequence(&second);
    CC_CHECK(CcSimHash(&first) == CcSimHash(&second));
    CC_CHECK(first.map_count == CC_MAP_COLLECTION_COUNT);
    CC_CHECK(CcPlayerMapCount(&first) == 1);
    CC_CHECK(CcPlayerMapCollectionCount(&first) == 1);
    CC_CHECK(CcSimMapForRoute(&first, first.routes[0].id,
                             first.player.id) != NULL);
    const CcMap *offered = CcSimMapForRoute(
        &first, first.routes[1].id, first.player.location_id);
    CC_CHECK(offered != NULL);
    CcId offered_id = offered->id;
    first.player.coins = 100;
    CcCommand buy_map = {.kind = CC_COMMAND_BUY_MAP, .target_id = offered_id};
    CC_CHECK(CcSimApply(&first, &buy_map, error, sizeof(error)));
    CC_CHECK(CcPlayerMapCount(&first) == 2);
    CC_CHECK(CcPlayerMapCollectionCount(&first) == 2);
    CC_CHECK(CcSimMap(&first, offered_id)->owner_id == first.player.id);
    CcCommand sell_map = {.kind = CC_COMMAND_SELL_MAP, .target_id = offered_id};
    CC_CHECK(CcSimApply(&first, &sell_map, error, sizeof(error)));
    CC_CHECK(CcPlayerMapCount(&first) == 1);
    CC_CHECK(CcPlayerMapCollectionCount(&first) == 2);
    CC_CHECK(CcSimMap(&first, offered_id)->owner_id ==
             first.player.location_id);

    const CcMap *illustrated = NULL;
    for (int32_t i = 0; i < first.map_count; ++i) {
        if (strcmp(first.maps[i].name,
                   CC_GLOAMGATE_ALDERWATCH_MAP_NAME) == 0) {
            illustrated = &first.maps[i];
            break;
        }
    }
    CC_CHECK(illustrated != NULL);
    CC_CHECK(strcmp(first.settlements[1].name, "Gloamgate") == 0);
    CC_CHECK(strcmp(first.settlements[2].name, "Alderwatch") == 0);
    CC_CHECK(strcmp(first.settlements[5].name, "Hollowbarrow") == 0);
    CC_CHECK(illustrated->route_id == first.routes[1].id);
    CC_CHECK(illustrated->owner_id == first.settlements[1].id);
    CC_CHECK(illustrated->ask_price == 24);
    const CcMap *hoard_map = &first.maps[CC_MAP_DRAGON_HOARD];
    CC_CHECK(strcmp(hoard_map->name, CC_DRAGON_HOARD_MAP_NAME) == 0);
    CC_CHECK(hoard_map->route_id == first.routes[5].id);
    CC_CHECK(hoard_map->maker_settlement_id == first.settlements[5].id);
    CC_CHECK(hoard_map->owner_id == first.settlements[1].id);
    CC_CHECK(hoard_map->contraband);
    CC_CHECK(hoard_map->recorded_danger >
             CcSimRouteDanger(&first, first.routes[5].id));
    first.player.location_id = first.settlements[1].id;
    CcCommand buy_illustrated = {
        .kind = CC_COMMAND_BUY_MAP,
        .target_id = illustrated->id
    };
    CC_CHECK(CcSimApply(&first, &buy_illustrated, error, sizeof(error)));
    CC_CHECK(CcSimMap(&first, illustrated->id)->owner_id == first.player.id);
    CC_CHECK(CcPlayerMapCount(&first) == 2);
    CcCommand archive_illustrated = {
        .kind = CC_COMMAND_ARCHIVE_MAP,
        .target_id = illustrated->id
    };
    CC_CHECK(CcSimApply(&first, &archive_illustrated,
                        error, sizeof(error)));
    CC_CHECK(CcSimMapIsArchived(&first, illustrated));
    CC_CHECK(CcPlayerMapCount(&first) == 1);
    CC_CHECK(CcSimMapForRoute(&first, first.routes[1].id,
                             first.player.id) == NULL);
    CcCommand retrieve_illustrated = {
        .kind = CC_COMMAND_RETRIEVE_MAP,
        .target_id = illustrated->id
    };
    CC_CHECK(CcSimApply(&first, &retrieve_illustrated,
                        error, sizeof(error)));
    CC_CHECK(!CcSimMapIsArchived(&first, illustrated));
    CC_CHECK(CcPlayerMapCount(&first) == 2);

    CcSim collector;
    CcSimInit(&collector, UINT32_C(0xc011ec7));
    collector.player.coins = 10000;
    collector.player.location_id = collector.settlements[1].id;
    CcCommand store_starting = {
        .kind = CC_COMMAND_ARCHIVE_MAP,
        .target_id = collector.maps[CC_MAP_THORNFORD_FORDINGS].id
    };
    CC_CHECK(CcSimApply(&collector, &store_starting,
                        error, sizeof(error)));
    for (int32_t i = 0; i < CC_MAP_COLLECTION_COUNT; ++i) {
        if (i == CC_MAP_CROWNLESS_ATLAS) continue;
        CcMap *map = &collector.maps[i];
        if (map->owner_id == collector.player.id) continue;
        collector.player.location_id = map->owner_id;
        CcCommand collect = {
            .kind = CC_COMMAND_BUY_MAP,
            .target_id = map->id
        };
        CC_CHECK(CcSimApply(&collector, &collect, error, sizeof(error)));
        collector.player.location_id = collector.settlements[1].id;
        CcCommand store = {
            .kind = CC_COMMAND_ARCHIVE_MAP,
            .target_id = map->id
        };
        CC_CHECK(CcSimApply(&collector, &store, error, sizeof(error)));
    }
    const CcMap *atlas = &collector.maps[CC_MAP_CROWNLESS_ATLAS];
    CC_CHECK(CcPlayerMapCollectionCount(&collector) ==
             CC_MAP_COLLECTION_COUNT);
    CC_CHECK(CcSimMapIsCatalogued(&collector, atlas));
    CC_CHECK(CcSimMapIsArchived(&collector, atlas));
    CcCommand retrieve_atlas = {
        .kind = CC_COMMAND_RETRIEVE_MAP,
        .target_id = atlas->id
    };
    CC_CHECK(CcSimApply(&collector, &retrieve_atlas,
                        error, sizeof(error)));
    CC_CHECK(!CcSimMapIsArchived(&collector, atlas));
    collector.carriage.location_id = collector.player.location_id;
    CC_CHECK(CcSimValidate(&collector, error, sizeof(error)));

    CcSim uncharted;
    CcSimInit(&uncharted, UINT32_C(0x12345678));
    uncharted.player.location_id = uncharted.settlements[1].id;
    uncharted.carriage.location_id = uncharted.player.location_id;
    CcCommand uncharted_travel = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = uncharted.settlements[5].id
    };
    CC_CHECK(CcSimMapForRoute(&uncharted, uncharted.routes[5].id,
                              uncharted.player.id) == NULL);
    CcTravelPreview uncharted_preview = {0};
    CC_CHECK(CcSimTravelPreview(&uncharted,
                                uncharted_travel.target_id,
                                &uncharted_preview,
                                error, sizeof(error)));
    CC_CHECK(!uncharted_preview.charted);
    CC_CHECK(uncharted_preview.destination_known);
    CC_CHECK(uncharted_preview.travel_days ==
             uncharted.routes[5].travel_days + 2);
    CC_CHECK(CcSimApply(&uncharted, &uncharted_travel,
                        error, sizeof(error)));
    CC_CHECK(uncharted.journey.total_subticks ==
             uncharted_preview.travel_watches *
                 CC_WORLD_WATCH_SUBTICKS);

    CcSim hidden_fork;
    CcSimInit(&hidden_fork, UINT32_C(0xf04c));
    hidden_fork.player.location_id = hidden_fork.settlements[1].id;
    hidden_fork.carriage.location_id = hidden_fork.player.location_id;
    const CcRoute *night_road = &hidden_fork.routes[6];
    CC_CHECK(night_road->smuggler_route);
    CC_CHECK(CcSimMapForRoute(&hidden_fork, night_road->id,
                              hidden_fork.player.id) == NULL);
    CcId hidden_destination = night_road->from_id ==
            hidden_fork.player.location_id ?
        night_road->to_id : night_road->from_id;
    CcTravelPreview hidden_preview = {0};
    CC_CHECK(CcSimTravelPreview(&hidden_fork, hidden_destination,
                                &hidden_preview, error, sizeof(error)));
    CC_CHECK(!hidden_preview.charted);
    CC_CHECK(!hidden_preview.destination_known);
    CC_CHECK(hidden_preview.travel_days == night_road->travel_days + 2);
    CcCommand take_hidden_fork = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = hidden_destination
    };
    CC_CHECK(CcSimApply(&hidden_fork, &take_hidden_fork,
                        error, sizeof(error)));
    CC_CHECK(hidden_fork.journey.route_id == night_road->id);

    CcSim commitment;
    CcSimInit(&commitment, UINT32_C(0xc011ab1e));
    CcSituation *first_charter = FirstActiveSituation(&commitment, -1);
    CC_CHECK(first_charter != NULL);
    int32_t first_slot = (int32_t)(first_charter - commitment.situations);
    CcSituation *second_charter = FirstActiveSituation(&commitment, first_slot);
    CC_CHECK(second_charter != NULL);
    CcCommand accept = {
        .kind = CC_COMMAND_ACCEPT_SITUATION,
        .target_id = first_charter->id
    };
    CC_CHECK(CcSimApply(&commitment, &accept, error, sizeof(error)));
    CC_CHECK(CcSimAcceptedSituation(&commitment) == first_charter);
    CcCommand second_accept = {
        .kind = CC_COMMAND_ACCEPT_SITUATION,
        .target_id = second_charter->id
    };
    CC_CHECK(!CcSimApply(&commitment, &second_accept, error, sizeof(error)));
    CcCommand abandon = {
        .kind = CC_COMMAND_ABANDON_SITUATION,
        .target_id = first_charter->id
    };
    CC_CHECK(CcSimApply(&commitment, &abandon, error, sizeof(error)));
    CC_CHECK(CcSimAcceptedSituation(&commitment) == NULL &&
             first_charter->status == CC_SITUATION_ACTIVE);

    CcSim blocked_loading;
    CcSimInit(&blocked_loading, UINT32_C(0xb0ced));
    CcSituation *blocked_relief = NULL;
    for (int32_t i = 0; i < blocked_loading.situation_count; ++i) {
        if (blocked_loading.situations[i].kind ==
            CC_SITUATION_RELIEF_DELIVERY) {
            blocked_relief = &blocked_loading.situations[i];
            break;
        }
    }
    CC_CHECK(blocked_relief != NULL);
    blocked_loading.player.cargo[CC_GOOD_TOOLS] =
        blocked_loading.player.cargo_capacity;
    CC_CHECK(CcPlayerCargoUsed(&blocked_loading.player) ==
             blocked_loading.player.cargo_capacity);
    CcSettlement *blocked_origin = CcSimSettlementMutable(
        &blocked_loading,
        CcSimSituationOfferSettlementId(&blocked_loading, blocked_relief));
    CC_CHECK(blocked_origin != NULL);
    int32_t blocked_origin_food = blocked_origin->stock[CC_GOOD_FOOD];
    CcCommand blocked_accept = {
        .kind = CC_COMMAND_ACCEPT_SITUATION,
        .target_id = blocked_relief->id
    };
    CC_CHECK(!CcSimApply(&blocked_loading, &blocked_accept,
                         error, sizeof(error)));
    CC_CHECK(strstr(error, "Clear cargo space") != NULL);
    CC_CHECK(blocked_loading.player.accepted_situation_id == 0U);
    CC_CHECK(blocked_loading.player.cargo[CC_GOOD_FOOD] == 0);
    CC_CHECK(blocked_origin->stock[CC_GOOD_FOOD] == blocked_origin_food);

    CcSim empty_granary;
    CcSimInit(&empty_granary, UINT32_C(0x6a6a));
    CcSituation *unfunded_relief = NULL;
    for (int32_t i = 0; i < empty_granary.situation_count; ++i) {
        if (empty_granary.situations[i].kind ==
            CC_SITUATION_RELIEF_DELIVERY) {
            unfunded_relief = &empty_granary.situations[i];
            break;
        }
    }
    CC_CHECK(unfunded_relief != NULL);
    CcSettlement *unfunded_origin = CcSimSettlementMutable(
        &empty_granary,
        CcSimSituationOfferSettlementId(&empty_granary, unfunded_relief));
    CC_CHECK(unfunded_origin != NULL);
    unfunded_origin->stock[CC_GOOD_FOOD] = unfunded_relief->quantity - 1;
    CcCommand unfunded_accept = {
        .kind = CC_COMMAND_ACCEPT_SITUATION,
        .target_id = unfunded_relief->id
    };
    CC_CHECK(!CcSimApply(&empty_granary, &unfunded_accept,
                         error, sizeof(error)));
    CC_CHECK(strstr(error, "granary") != NULL);
    CC_CHECK(empty_granary.player.accepted_situation_id == 0U);
    CC_CHECK(empty_granary.player.cargo[CC_GOOD_FOOD] == 0);

    CcSim remote_relief;
    CcSimInit(&remote_relief, UINT32_C(0x4e6f7465));
    CcSituation *remote_offer = NULL;
    for (int32_t i = 0; i < remote_relief.situation_count; ++i) {
        if (remote_relief.situations[i].kind ==
            CC_SITUATION_RELIEF_DELIVERY) {
            remote_offer = &remote_relief.situations[i];
            break;
        }
    }
    CC_CHECK(remote_offer != NULL);
    const CcCharacter *remote_affected =
        CcSimSituationAffectedCharacter(&remote_relief, remote_offer);
    CC_CHECK(remote_affected != NULL);
    remote_relief.player.location_id =
        remote_affected->current_settlement_id;
    remote_relief.carriage.location_id =
        remote_relief.player.location_id;
    int32_t remote_origin_slot = -1;
    CcId remote_origin_id = CcSimSituationOfferSettlementId(
        &remote_relief, remote_offer);
    for (int32_t i = 0; i < remote_relief.settlement_count; ++i) {
        if (remote_relief.settlements[i].id == remote_origin_id) {
            remote_origin_slot = i;
            break;
        }
    }
    CC_CHECK(remote_origin_slot >= 0);
    int32_t remote_origin_food =
        remote_relief.settlements[remote_origin_slot].stock[CC_GOOD_FOOD];
    CcCommand remote_listen = {
        .kind = CC_COMMAND_CHARACTER_RESPONSE,
        .target_id = remote_offer->id,
        .amount = CC_CHARACTER_RESPONSE_LISTEN
    };
    CC_CHECK(CcSimApply(&remote_relief, &remote_listen,
                        error, sizeof(error)));
    CcCommand remote_pledge = remote_listen;
    remote_pledge.amount = CC_CHARACTER_RESPONSE_PLEDGE_HELP;
    CC_CHECK(!CcSimApply(&remote_relief, &remote_pledge,
                         error, sizeof(error)));
    CC_CHECK(strstr(error, "Return to Mara") != NULL);
    CC_CHECK(remote_relief.player.accepted_situation_id == 0U);
    CC_CHECK(remote_relief.player.cargo[CC_GOOD_FOOD] == 0);
    CC_CHECK(remote_relief.settlements[remote_origin_slot]
                 .stock[CC_GOOD_FOOD] == remote_origin_food);

    CcSim laundering;
    CcSimInit(&laundering, UINT32_C(0x1a0d3e));
    CcSituation *delivery = NULL;
    for (int32_t i = 0; i < laundering.situation_count; ++i) {
        if (laundering.situations[i].kind == CC_SITUATION_RELIEF_DELIVERY) {
            delivery = &laundering.situations[i];
            break;
        }
    }
    CC_CHECK(delivery != NULL);
    CcCommand accept_delivery = {
        .kind = CC_COMMAND_ACCEPT_SITUATION,
        .target_id = delivery->id
    };
    CC_CHECK(CcSimApply(&laundering, &accept_delivery,
                        error, sizeof(error)));
    laundering.player.location_id = delivery->target_id;
    laundering.carriage.location_id = delivery->target_id;
    laundering.player.coins = 500;
    laundering.player.cargo[CC_GOOD_FOOD] = 0;
    CcCommand local_food = {
        .kind = CC_COMMAND_TRADE,
        .good = CC_GOOD_FOOD,
        .amount = delivery->quantity
    };
    CC_CHECK(CcSimApply(&laundering, &local_food, error, sizeof(error)));
    int32_t local_cargo = laundering.player.cargo[CC_GOOD_FOOD];
    CcCommand fake_delivery = {
        .kind = CC_COMMAND_TRADE,
        .good = CC_GOOD_FOOD,
        .amount = -delivery->quantity
    };
    CC_CHECK(!CcSimApply(&laundering, &fake_delivery,
                         error, sizeof(error)));
    CC_CHECK(laundering.player.cargo[CC_GOOD_FOOD] == local_cargo);
    CC_CHECK(delivery->progress == 0 &&
             delivery->status == CC_SITUATION_ACTIVE);

    CcSim washed_load;
    CcSimInit(&washed_load, UINT32_C(0x1a0d3e));
    CcSituation *washed_delivery = NULL;
    for (int32_t i = 0; i < washed_load.situation_count; ++i) {
        if (washed_load.situations[i].kind == CC_SITUATION_RELIEF_DELIVERY) {
            washed_delivery = &washed_load.situations[i];
            break;
        }
    }
    CC_CHECK(washed_delivery != NULL);
    washed_delivery->target_id = washed_load.settlements[3].id;
    washed_delivery->quantity = 1;
    washed_delivery->progress = 0;
    washed_delivery->deadline_day = washed_load.current_day + 60;
    washed_load.player.coins = 500;
    washed_load.player.cargo[CC_GOOD_FOOD] = 0;
    washed_load.routes[0].security = 100;
    washed_load.routes[0].condition = 100;
    washed_load.routes[6].security = 100;
    washed_load.routes[6].condition = 100;
    washed_load.routes[6].smuggler_route = false;
    washed_load.bandit_count = 0;
    washed_load.monster_count = 0;
    CcCommand accept_washed = {
        .kind = CC_COMMAND_ACCEPT_SITUATION,
        .target_id = washed_delivery->id
    };
    CC_CHECK(CcSimApply(&washed_load, &accept_washed,
                        error, sizeof(error)));
    CcCommand wrong_way = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = washed_load.settlements[1].id
    };
    CC_CHECK(CcSimApply(&washed_load, &wrong_way, error, sizeof(error)));
    CcCommand roadside_abandon = {
        .kind = CC_COMMAND_ABANDON_SITUATION,
        .target_id = washed_delivery->id
    };
    CC_CHECK(!CcSimApply(&washed_load, &roadside_abandon,
                         error, sizeof(error)));
    CC_CHECK(washed_load.player.accepted_situation_id ==
             washed_delivery->id);
    AdvanceTravellingJourney(&washed_load);
    CC_CHECK(washed_load.resolved_journey_situation_id == 0U);
    CcCommand sell_load = {
        .kind = CC_COMMAND_TRADE,
        .good = CC_GOOD_FOOD,
        .amount = -1
    };
    CC_CHECK(CcSimApply(&washed_load, &sell_load, error, sizeof(error)));
    CcCommand empty_arrival = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = washed_delivery->target_id
    };
    CC_CHECK(CcSimApply(&washed_load, &empty_arrival,
                        error, sizeof(error)));
    AdvanceTravellingJourney(&washed_load);
    CC_CHECK(washed_load.resolved_journey_situation_id == 0U);
    CcCommand buy_local_replacement = {
        .kind = CC_COMMAND_TRADE,
        .good = CC_GOOD_FOOD,
        .amount = 1
    };
    CC_CHECK(CcSimApply(&washed_load, &buy_local_replacement,
                        error, sizeof(error)));
    CcCommand deliver_replacement = {
        .kind = CC_COMMAND_TRADE,
        .good = CC_GOOD_FOOD,
        .amount = -1
    };
    CC_CHECK(!CcSimApply(&washed_load, &deliver_replacement,
                         error, sizeof(error)));
    CC_CHECK(washed_delivery->status == CC_SITUATION_ACTIVE &&
             washed_delivery->progress == 0);

    CcSim unanswered;
    CcSimInit(&unanswered, UINT32_C(0xc011ab1e));
    CcSituation *unanswered_charter = FirstActiveSituation(&unanswered, -1);
    CC_CHECK(unanswered_charter != NULL);
    unanswered.current_day = 36;
    unanswered_charter->deadline_day = 36;
    int32_t reputation_before = unanswered.player.reputation;
    CcSimAdvanceDays(&unanswered, 1);
    CC_CHECK(unanswered.player.reputation == reputation_before);

    CcSim promised;
    CcSimInit(&promised, UINT32_C(0xc011ab1e));
    CcSituation *promised_charter = FirstActiveSituation(&promised, -1);
    CC_CHECK(promised_charter != NULL);
    CcCommand promise = {
        .kind = CC_COMMAND_ACCEPT_SITUATION,
        .target_id = promised_charter->id
    };
    CC_CHECK(CcSimApply(&promised, &promise, error, sizeof(error)));
    promised.current_day = 36;
    promised_charter->deadline_day = 36;
    reputation_before = promised.player.reputation;
    CcSimAdvanceDays(&promised, 1);
    CC_CHECK(promised.player.reputation == reputation_before - 1 &&
             CcSimAcceptedSituation(&promised) == NULL);

    CcSim defended_road;
    CcSimInit(&defended_road, UINT32_C(0x50adca11));
    defended_road.bandits[0].route_id = defended_road.routes[0].id;
    CcSituation *journey_charter = PreparePromisedJourney(
        &defended_road, error, sizeof(error));
    CcId journey_origin = defended_road.player.location_id;
    CcId journey_destination = defended_road.journey.destination_id;
    int32_t route_security = defended_road.routes[0].security;
    int32_t destination_population = defended_road.settlements[1].population;
    int32_t bandit_members = defended_road.bandits[0].members;
    int32_t shipment_count = defended_road.shipment_count;
    int32_t carriage_condition = defended_road.carriage.condition;
    CcMoney combat_coins = defended_road.player.coins;
    CC_CHECK(journey_origin != journey_destination);
    CC_CHECK(CcSimRecentEvent(&defended_road, 0)->kind ==
             CC_EVENT_JOURNEY_ENCOUNTER);
    int32_t blocked_day = defended_road.current_day;
    int32_t blocked_time = defended_road.clock.minute_subticks;
    int32_t blocked_progress = defended_road.carriage.progress_milli;
    CcSimAdvanceRuntimeTicks(&defended_road,
                             CC_WORLD_TICKS_PER_SECOND * 10);
    CC_CHECK(defended_road.current_day == blocked_day);
    CC_CHECK(defended_road.clock.minute_subticks == blocked_time);
    CC_CHECK(defended_road.carriage.progress_milli == blocked_progress);
    CcCommand defend = {.kind = CC_COMMAND_RESOLVE_ENCOUNTER_COMBAT};
    CC_CHECK(CcSimApply(&defended_road, &defend, error, sizeof(error)));
    CC_CHECK(defended_road.journey.active);
    CC_CHECK(defended_road.journey.phase == CC_JOURNEY_PHASE_TRAVELLING);
    CC_CHECK(defended_road.player.location_id == journey_origin);
    CC_CHECK(defended_road.routes[0].security > route_security);
    CC_CHECK(defended_road.settlements[1].population ==
             destination_population);
    CC_CHECK(defended_road.carriage.condition < carriage_condition);
    CC_CHECK(defended_road.player.coins < combat_coins);
    CC_CHECK(defended_road.bandits[0].members < bandit_members);
    CC_CHECK(defended_road.shipment_count >= shipment_count + 1);
    CC_CHECK(defended_road.shipments[defended_road.shipment_count - 1].status ==
             CC_SHIPMENT_TRAVELLING);
    AdvanceTravellingJourney(&defended_road);
    CC_CHECK(!defended_road.journey.active);
    CC_CHECK(defended_road.player.location_id == journey_destination);

    defended_road.player.cargo[CC_GOOD_FOOD] = 1;
    CcCommand fulfill = {
        .kind = CC_COMMAND_TRADE,
        .good = CC_GOOD_FOOD,
        .amount = -1
    };
    CC_CHECK(CcSimApply(&defended_road, &fulfill, error, sizeof(error)));
    CC_CHECK(journey_charter->status == CC_SITUATION_RESOLVED);
    CC_CHECK(defended_road.delayed_echo.active);
    CcSimAdvanceDays(&defended_road, 30);
    CC_CHECK(defended_road.delayed_echo.active);
    CC_CHECK(CcSimRecentEvent(&defended_road, 0)->kind ==
             CC_EVENT_DELAYED_ECHO);
    CcSimAdvanceDays(&defended_road, 30);
    CC_CHECK(!defended_road.delayed_echo.active);
    CC_CHECK(CcSimRecentEvent(&defended_road, 0)->kind ==
             CC_EVENT_DELAYED_ECHO);
    CC_CHECK(CcSimRecentEvent(&defended_road, 0)->parent_id != 0U);

    CcSim bargained_road;
    CcSimInit(&bargained_road, UINT32_C(0x50adca11));
    bargained_road.bandits[0].route_id = bargained_road.routes[0].id;
    (void)PreparePromisedJourney(&bargained_road, error, sizeof(error));
    route_security = bargained_road.routes[0].security;
    int32_t bandit_influence = bargained_road.bandits[0].influence;
    CcMoney coins_before_bargain = bargained_road.player.coins;
    CcMoney gold_before_bargain = CcSimTrackedGold(&bargained_road);
    int32_t food_before_bargain = CcSimTrackedGood(
        &bargained_road, CC_GOOD_FOOD);
    int32_t bargain_cost = bargained_road.journey.bargain_cost;
    CcCommand bargain = {
        .kind = CC_COMMAND_RESOLVE_ENCOUNTER_NEGOTIATE
    };
    CC_CHECK(CcSimApply(&bargained_road, &bargain,
                        error, sizeof(error)));
    CC_CHECK(bargained_road.journey.active);
    CC_CHECK(bargained_road.routes[0].security < route_security);
    CC_CHECK(bargained_road.bandits[0].influence > bandit_influence);
    CC_CHECK(bargained_road.player.coins <=
             coins_before_bargain - bargain_cost);
    CC_CHECK(CcSimTrackedGold(&bargained_road) == gold_before_bargain);
    CC_CHECK(CcSimTrackedGood(&bargained_road, CC_GOOD_FOOD) ==
             food_before_bargain);

    CcSim provisioned_road;
    CcSimInit(&provisioned_road, UINT32_C(0x50adca11));
    provisioned_road.bandits[0].route_id = provisioned_road.routes[0].id;
    (void)PreparePromisedJourney(&provisioned_road, error, sizeof(error));
    CcGood demanded_good = CC_GOOD_COUNT;
    int32_t demanded_quantity = 0;
    CC_CHECK(CcSimBanditProvisionDemand(
        &provisioned_road, provisioned_road.journey.route_id,
        &demanded_good, &demanded_quantity));
    CC_CHECK(demanded_good >= CC_GOOD_FOOD &&
             demanded_good <= CC_GOOD_WEAPONS && demanded_quantity > 0);
    int32_t reaction = CcSimBanditReactionRoll(
        &provisioned_road, provisioned_road.journey.route_id);
    CC_CHECK(reaction >= 2 && reaction <= 12);
    CC_CHECK(CcBanditReactionName(reaction) != NULL);
    provisioned_road.player.cargo[demanded_good] = demanded_quantity;
    CcMoney provision_coins = provisioned_road.player.coins;
    int32_t provision_supplies = provisioned_road.bandits[0].supplies;
    int32_t provision_influence = provisioned_road.bandits[0].influence;
    CcCommand provisions = {
        .kind = CC_COMMAND_RESOLVE_ENCOUNTER_PROVISIONS
    };
    CC_CHECK(CcSimApply(&provisioned_road, &provisions,
                        error, sizeof(error)));
    CC_CHECK(provisioned_road.journey.phase ==
             CC_JOURNEY_PHASE_TRAVELLING);
    CC_CHECK(provisioned_road.player.cargo[demanded_good] == 0);
    CC_CHECK(provisioned_road.player.coins == provision_coins);
    CC_CHECK(provisioned_road.bandits[0].supplies > provision_supplies);
    CC_CHECK(provisioned_road.bandits[0].influence > provision_influence);
    const CcEvent *provision_event = NULL;
    for (int32_t offset = 0; offset < provisioned_road.event_count; ++offset) {
        const CcEvent *candidate = CcSimRecentEvent(&provisioned_road, offset);
        if (candidate != NULL &&
            candidate->kind == CC_EVENT_ENCOUNTER_NEGOTIATED) {
            provision_event = candidate;
            break;
        }
    }
    CC_CHECK(provision_event != NULL);
    CC_CHECK(strstr(provision_event->text,
                    provisioned_road.bandits[0].name) != NULL);

    CcSim empty_carriage;
    CcSimInit(&empty_carriage, UINT32_C(0x50adca11));
    empty_carriage.bandits[0].route_id = empty_carriage.routes[0].id;
    (void)PreparePromisedJourney(&empty_carriage, error, sizeof(error));
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        empty_carriage.player.cargo[good] = 0;
    }
    CC_CHECK(!CcSimApply(&empty_carriage, &provisions,
                         error, sizeof(error)));
    CC_CHECK(empty_carriage.journey.phase == CC_JOURNEY_PHASE_BLOCKED);

    CcSim withdrawn_road;
    CcSimInit(&withdrawn_road, UINT32_C(0x50adca11));
    withdrawn_road.bandits[0].route_id = withdrawn_road.routes[0].id;
    (void)PreparePromisedJourney(&withdrawn_road, error, sizeof(error));
    CcId withdrawal_origin = withdrawn_road.journey.origin_id;
    int32_t withdrawal_condition = withdrawn_road.carriage.condition;
    int32_t withdrawal_security = withdrawn_road.routes[0].security;
    int32_t withdrawal_influence = withdrawn_road.bandits[0].influence;
    CcMoney withdrawal_gold = CcSimTrackedGold(&withdrawn_road);
    CcCommand withdraw = {
        .kind = CC_COMMAND_WITHDRAW_ENCOUNTER,
        .amount = 1
    };
    CC_CHECK(CcSimApply(&withdrawn_road, &withdraw,
                        error, sizeof(error)));
    CC_CHECK(!withdrawn_road.journey.active);
    CC_CHECK(withdrawn_road.journey.phase == CC_JOURNEY_PHASE_NONE);
    CC_CHECK(withdrawn_road.player.location_id == withdrawal_origin);
    CC_CHECK(withdrawn_road.carriage.mode == CC_CARRIAGE_PARKED);
    CC_CHECK(withdrawn_road.carriage.location_id == withdrawal_origin);
    CC_CHECK(withdrawn_road.carriage.condition < withdrawal_condition);
    CC_CHECK(withdrawn_road.routes[0].security < withdrawal_security);
    CC_CHECK(withdrawn_road.bandits[0].influence > withdrawal_influence);
    CC_CHECK(CcSimTrackedGold(&withdrawn_road) == withdrawal_gold);
    CC_CHECK(CcSimRecentEvent(&withdrawn_road, 0)->kind ==
             CC_EVENT_ENCOUNTER_WITHDRAWN);

    CcSim invalid_state;
    CcSimInit(&invalid_state, UINT32_C(0xbad5a7e));
    invalid_state.player.cargo[CC_GOOD_FOOD] = -1;
    CC_CHECK(!CcSimValidate(&invalid_state, error, sizeof(error)));
    CcSimInit(&invalid_state, UINT32_C(0xbad5a7e));
    invalid_state.kingdom_count = CC_MAX_KINGDOMS + 1;
    CC_CHECK(!CcSimValidate(&invalid_state, error, sizeof(error)));
    CcSimInit(&invalid_state, UINT32_C(0xbad5a7e));
    invalid_state.settlements[0].stock[CC_GOOD_FOOD] =
        CC_SIM_MAX_UNITS + 1;
    CC_CHECK(!CcSimValidate(&invalid_state, error, sizeof(error)));
    CcSimInit(&invalid_state, UINT32_C(0xbad5a7e));
    invalid_state.settlements[0].production[CC_GOOD_FOOD] = INT32_MAX;
    CC_CHECK(!CcSimValidate(&invalid_state, error, sizeof(error)));
    CcSimInit(&invalid_state, UINT32_C(0xbad5a7e));
    invalid_state.settlements[0].kingdom_id =
        CcMakeId(CC_ENTITY_KINGDOM, UINT64_C(999999));
    CC_CHECK(!CcSimValidate(&invalid_state, error, sizeof(error)));
    CcSimInit(&invalid_state, UINT32_C(0xbad5a7e));
    invalid_state.factions[0].support = 101;
    CC_CHECK(!CcSimValidate(&invalid_state, error, sizeof(error)));
    CcSimInit(&invalid_state, UINT32_C(0xbad5a7e));
    invalid_state.monsters[0].dungeon_id =
        CcMakeId(CC_ENTITY_DUNGEON, UINT64_C(999999));
    CC_CHECK(!CcSimValidate(&invalid_state, error, sizeof(error)));
    CcSimInit(&invalid_state, UINT32_C(0xbad5a7e));
    invalid_state.dungeons[0].regional_pressure = 101;
    CC_CHECK(!CcSimValidate(&invalid_state, error, sizeof(error)));
    CcSimInit(&invalid_state, UINT32_C(0xbad5a7e));
    invalid_state.current_day = CC_SIM_MAX_DAY + 1;
    CC_CHECK(!CcSimValidate(&invalid_state, error, sizeof(error)));
    CcSimInit(&invalid_state, UINT32_C(0xbad5a7e));
    const CcEvent *existing_event = CcSimRecentEvent(&invalid_state, 0);
    CC_CHECK(existing_event != NULL);
    invalid_state.next_entity_serial =
        existing_event->id & UINT64_C(0x00ffffffffffffff);
    CC_CHECK(!CcSimValidate(&invalid_state, error, sizeof(error)));
    CC_CHECK(strstr(error, "identity counter") != NULL);
    CcSimInit(&invalid_state, UINT32_C(0xbad5a7e));
    invalid_state.routes[1].id = invalid_state.routes[0].id;
    CC_CHECK(!CcSimValidate(&invalid_state, error, sizeof(error)));
    CC_CHECK(strstr(error, "not unique") != NULL);
    CcSimInit(&invalid_state, UINT32_C(0xbad5a7e));
    memset(invalid_state.goblins.name, 'G',
           sizeof(invalid_state.goblins.name));
    CC_CHECK(!CcSimValidate(&invalid_state, error, sizeof(error)));
    CcSimInit(&invalid_state, UINT32_C(0xbad5a7e));
    CC_CHECK(invalid_state.situation_count > 0);
    memset(invalid_state.situations[0].sponsor_name, 'S',
           sizeof(invalid_state.situations[0].sponsor_name));
    CC_CHECK(!CcSimValidate(&invalid_state, error, sizeof(error)));
    CcSimInit(&invalid_state, UINT32_C(0xbad5a7e));
    memset(invalid_state.delayed_echo.character_name, 'E',
           sizeof(invalid_state.delayed_echo.character_name));
    CC_CHECK(!CcSimValidate(&invalid_state, error, sizeof(error)));

    CcSim trade_edge;
    CcSimInit(&trade_edge, UINT32_C(0x7adee9e));
    uint64_t trade_edge_hash = CcSimHash(&trade_edge);
    CcCommand impossible_sale = {
        .kind = CC_COMMAND_TRADE,
        .good = CC_GOOD_FOOD,
        .amount = INT32_MIN
    };
    CC_CHECK(!CcSimApply(&trade_edge, &impossible_sale,
                         error, sizeof(error)));
    CC_CHECK(CcSimHash(&trade_edge) == trade_edge_hash);
    CcSimInit(&invalid_state, UINT32_C(0xbad5a7e));
    invalid_state.shipment_count = 1;
    invalid_state.shipments[0] = (CcShipment){
        .id = CcMakeId(CC_ENTITY_SHIPMENT, UINT64_C(9999)),
        .origin_id = invalid_state.settlements[0].id,
        .destination_id = invalid_state.settlements[2].id,
        .final_destination_id = invalid_state.settlements[2].id,
        .route_id = invalid_state.routes[0].id,
        .good = CC_GOOD_FOOD,
        .quantity = 1,
        .departure_day = invalid_state.current_day,
        .arrival_day = invalid_state.current_day +
                       invalid_state.routes[0].travel_days,
        .status = CC_SHIPMENT_TRAVELLING
    };
    CC_CHECK(!CcSimValidate(&invalid_state, error, sizeof(error)));
    invalid_state.shipments[0].destination_id =
        invalid_state.settlements[1].id;
    invalid_state.shipments[0].final_destination_id =
        invalid_state.settlements[1].id;
    invalid_state.shipments[0].quantity =
        invalid_state.routes[0].capacity * 8 + 1;
    CC_CHECK(!CcSimValidate(&invalid_state, error, sizeof(error)));
    invalid_state.shipments[0].quantity = 1;
    invalid_state.shipments[0].arrival_day += 1;
    CC_CHECK(!CcSimValidate(&invalid_state, error, sizeof(error)));

    CcSim conserved;
    CcSimInit(&conserved, UINT32_C(0xc01d1ed6));
    CcMoney initial_gold = CcSimTrackedGold(&conserved);
    CcSimAdvanceDays(&conserved, 3650);
    CC_CHECK(CcSimTrackedGold(&conserved) == initial_gold);
    CC_CHECK(CcSimValidate(&conserved, error, sizeof(error)));

    CC_CHECK(CcSimValidate(&first, error, sizeof(error)));
    CcSim different;
    CcSimInit(&different, UINT32_C(0x87654321));
    CC_CHECK(CcSimHash(&first) != CcSimHash(&different));
    puts("deterministic simulation tests passed");
    return 0;
}
