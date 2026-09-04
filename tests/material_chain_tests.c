#include "sim/cc_sim.h"

#include "test_support.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint32_t Service(CcServiceKind service)
{
    return UINT32_C(1) << (uint32_t)service;
}

static CcSettlement *PrepareIsolated(CcSim *sim)
{
    CcSimInit(sim, UINT32_C(0x6d111c4a));
    CcSettlement archive_town = sim->settlements[1];
    sim->settlements[0] = archive_town;
    sim->settlement_count = 1;
    sim->kingdom_count = 1;
    sim->route_count = 0;
    sim->road_site_count = 0;
    sim->shipment_count = 0;
    sim->courier_count = 0;
    sim->bandit_count = 0;
    sim->monster_count = 0;
    sim->dungeon_count = 0;
    sim->situation_count = 0;
    sim->front_count = 0;
    sim->quest_outcome_count = 0;
    sim->character_count = 0;
    sim->relationship_count = 0;
    sim->pending_echo_count = 0;
    sim->faction_count = 0;
    sim->treasure_count = 0;
    sim->event_count = 0;
    sim->event_write_index = 0;
    memset(sim->events, 0, sizeof(sim->events));
    sim->current_day = 6;
    sim->iron_ledger_reserve = 0;
    sim->archives = (CcArchives){.lore_ceiling = 60};
    sim->player.location_id = archive_town.id;
    sim->goblins.lair_settlement_id = archive_town.id;
    sim->goblins.tribute_phase = CC_GOBLIN_TRIBUTE_IDLE;
    sim->goblins.tribute_cooldown_days = 10000;
    sim->goblins.tribute_event_id = 0U;
    sim->dragon.lair_settlement_id = archive_town.id;
    sim->dragon.slain = true;
    sim->dragon.hoard_event_id = 0U;
    sim->dragon.omen_event_id = 0U;
    sim->dragon.lifecycle_event_id = 0U;
    sim->hoard_raiders.phase = CC_HOARD_RAIDERS_IDLE;
    sim->hoard_raiders.cooldown_days = 10000;
    sim->hoard_raiders.cause_event_id = 0U;

    CcKingdom *kingdom = &sim->kingdoms[0];
    kingdom->treasury = 0;
    kingdom->iron_ledger_debt = 0;
    kingdom->legitimacy = 60;
    kingdom->sanction = 60;
    kingdom->unsanctioned_weeks = 0;
    kingdom->pretender_crises = 0;
    kingdom->anointed = true;

    CcSettlement *place = &sim->settlements[0];
    place->population = 100;
    place->security = 50;
    place->prosperity = 0;
    place->hunger = 0;
    place->service_mask = Service(CC_SERVICE_INN);
    place->service_project = CC_SERVICE_NONE;
    place->service_project_days = 0;
    place->market_coins = 0;
    place->war_chest = 0;
    place->field_yield = 0;
    place->iron_deposit = 0;
    place->gold_seam = false;
    place->gem_seam = false;
    place->cow_adults = 0;
    place->cow_calves = 0;
    place->sheep_adults = 0;
    place->sheep_lambs = 0;
    place->farm_tool_wear = 0;
    place->mine_tool_wear = 0;
    place->smith_tool_wear = 0;
    place->paper_tool_wear = 0;
    place->treasure_gold_committed = 0;
    place->treasure_gems_committed = 0;
    place->treasure_work = 0;
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        place->stock[good] = 0;
        place->reserve_target[good] = 0;
        place->production[good] = 0;
        place->consumption[good] = 0;
        place->price[good] = CcGoodDefinitionFor((CcGood)good)->base_price;
    }
    place->stock[CC_GOOD_GOLD] = 64;
    place->stock[CC_GOOD_GEMS] = 64;
    return place;
}

static CcId AddNotableEvent(CcSim *sim)
{
    int32_t slot = sim->event_write_index;
    CcEvent *event = &sim->events[slot];
    *event = (CcEvent){
        .id = CcMakeId(CC_ENTITY_EVENT, sim->next_entity_serial++),
        .day = sim->current_day,
        .kind = CC_EVENT_KINGDOM_ACTION,
        .subject_id = sim->kingdoms[0].id,
        .location_id = sim->settlements[0].id,
        .magnitude = 40
    };
    (void)snprintf(event->text, sizeof(event->text),
                   "A ruler makes a lasting public vow.");
    sim->event_write_index = (slot + 1) % CC_MAX_EVENTS;
    if (sim->event_count < CC_MAX_EVENTS) sim->event_count += 1;
    return event->id;
}

static int32_t CountEvents(const CcSim *sim, CcEventKind kind)
{
    int32_t count = 0;
    for (int32_t i = 0; i < sim->event_count; ++i) {
        const CcEvent *event = CcSimRecentEvent(sim, i);
        if (event != NULL && event->kind == kind) count += 1;
    }
    return count;
}

static bool IsArchiveVolume(const CcTreasure *treasure)
{
    return treasure != NULL && !treasure->destroyed &&
        (strncmp(treasure->name, "Chronicle ", 10) == 0 ||
         strncmp(treasure->name, "Ledger ", 7) == 0 ||
         strncmp(treasure->name, "Annal ", 6) == 0 ||
         strncmp(treasure->name, "Register ", 9) == 0 ||
         strncmp(treasure->name, "Codex of ", 9) == 0);
}

static int32_t CountArchiveVolumes(const CcSim *sim)
{
    int32_t count = 0;
    for (int32_t i = 0; i < sim->treasure_count; ++i) {
        if (IsArchiveVolume(&sim->treasures[i])) count += 1;
    }
    return count;
}

static void PrepareMill(CcSim *sim, CcSettlement **place)
{
    *place = PrepareIsolated(sim);
    (*place)->service_mask |= Service(CC_SERVICE_MILL);
    (*place)->stock[CC_GOOD_BREAD] = 100;
    (*place)->stock[CC_GOOD_WHEAT] = 8;
    (*place)->reserve_target[CC_GOOD_WHEAT] = 2;
    (*place)->stock[CC_GOOD_WOOD] = 8;
    (*place)->reserve_target[CC_GOOD_WOOD] = 2;
    (*place)->stock[CC_GOOD_TOOLS] = 1;
    (*place)->reserve_target[CC_GOOD_PAPER] = 10;
    (*place)->production[CC_GOOD_PAPER] = 8;
}

static void CheckPaperMillGates(void)
{
    CcSim sim;
    CcSettlement *place = NULL;
    PrepareMill(&sim, &place);
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(place->stock[CC_GOOD_PAPER] == 8);
    CC_CHECK(place->stock[CC_GOOD_WOOD] == 6);
    CC_CHECK(place->stock[CC_GOOD_WHEAT] == 8);
    CC_CHECK(place->paper_tool_wear == 1);
    CC_CHECK(CountEvents(&sim, CC_EVENT_PAPER_MILLED) == 1);

    PrepareMill(&sim, &place);
    place->service_mask &= ~Service(CC_SERVICE_MILL);
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(place->stock[CC_GOOD_PAPER] == 0);

    PrepareMill(&sim, &place);
    place->stock[CC_GOOD_WOOD] = place->reserve_target[CC_GOOD_WOOD];
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(place->stock[CC_GOOD_PAPER] == 0);
    CC_CHECK(place->stock[CC_GOOD_WHEAT] == 8);

    PrepareMill(&sim, &place);
    place->stock[CC_GOOD_WHEAT] = 0;
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(place->stock[CC_GOOD_PAPER] == 8);
    CC_CHECK(place->stock[CC_GOOD_WOOD] == 6);

    PrepareMill(&sim, &place);
    place->stock[CC_GOOD_WOOD] = place->reserve_target[CC_GOOD_WOOD] + 1;
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(place->stock[CC_GOOD_PAPER] == 4);
    CC_CHECK(place->stock[CC_GOOD_WOOD] == 2);

    PrepareMill(&sim, &place);
    place->production[CC_GOOD_PAPER] = 3;
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(place->stock[CC_GOOD_PAPER] == 3);
    CC_CHECK(place->stock[CC_GOOD_WOOD] == 7);

    PrepareMill(&sim, &place);
    place->stock[CC_GOOD_TOOLS] = 0;
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(place->stock[CC_GOOD_PAPER] == 0);

    PrepareMill(&sim, &place);
    place->hunger = 40;
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(place->stock[CC_GOOD_PAPER] == 0);
}

static void CheckLegacyPaperRecipe(void)
{
    for (uint32_t version = 34U; version <= 36U; ++version) {
        CcSim sim;
        CcSettlement *place = NULL;
        PrepareMill(&sim, &place);
        sim.schema_version = version;
        place->stock[CC_GOOD_WOOD] = 0;
        CcSimAdvanceDays(&sim, 1);
        CC_CHECK(place->stock[CC_GOOD_PAPER] == 8);
        CC_CHECK(place->stock[CC_GOOD_WHEAT] == 6);
        CC_CHECK(place->stock[CC_GOOD_WOOD] == 0);
    }
}

static void CheckWoodFeedsPaperAndWheatFeedsScribes(void)
{
    CcSim sim;
    CcSettlement *place = NULL;
    PrepareMill(&sim, &place);
    sim.iron_ledger_reserve = 50;
    sim.archives.scribes = 1;
    place->stock[CC_GOOD_WHEAT] = 4;
    AddNotableEvent(&sim);
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(place->stock[CC_GOOD_WOOD] == 6);
    CC_CHECK(place->stock[CC_GOOD_WHEAT] == 2);
    CC_CHECK(place->stock[CC_GOOD_PAPER] == 7);
    CC_CHECK(CountArchiveVolumes(&sim) == 1);
    CC_CHECK(sim.archives.lore_stored == 1);
}

static void CheckPaperOwnsTheTome(void)
{
    CcSim sim;
    CcSettlement *place = PrepareIsolated(&sim);
    sim.iron_ledger_reserve = 50;
    sim.archives.scribes = 1;
    place->stock[CC_GOOD_BREAD] = 100;
    place->stock[CC_GOOD_WHEAT] = 4;
    place->stock[CC_GOOD_TOOLS] = 1;
    CcId notable = AddNotableEvent(&sim);
    CcMaterialChainSnapshot snapshot = CcSimMaterialChainSnapshot(&sim);
    CC_CHECK(snapshot.blocker == CC_MATERIAL_CHAIN_PAPER);
    int32_t volumes_before = CountArchiveVolumes(&sim);
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(sim.archives.lore_stored == 0);
    CC_CHECK(sim.archives.last_recorded_day == 0);
    CC_CHECK(CountEvents(&sim, CC_EVENT_LORE_RECORDED) == 0);
    CC_CHECK(CountArchiveVolumes(&sim) == volumes_before);
    CC_CHECK(CcSimEvent(&sim, notable) != NULL);
}

static void CheckBindingMaterialsOwnTheTome(void)
{
    CcSim sim;
    CcSettlement *place = PrepareIsolated(&sim);
    sim.iron_ledger_reserve = 50;
    sim.archives.scribes = 1;
    place->stock[CC_GOOD_BREAD] = 100;
    place->stock[CC_GOOD_WHEAT] = 4;
    place->stock[CC_GOOD_PAPER] = 1;
    place->stock[CC_GOOD_TOOLS] = 1;
    place->stock[CC_GOOD_GOLD] = 0;
    place->stock[CC_GOOD_GEMS] = 0;
    AddNotableEvent(&sim);
    CC_CHECK(CcSimMaterialChainSnapshot(&sim).blocker ==
             CC_MATERIAL_CHAIN_BINDING);
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(sim.archives.lore_stored == 0);
    CC_CHECK(place->stock[CC_GOOD_PAPER] == 1);
    CC_CHECK(CountArchiveVolumes(&sim) == 0);
}

static void CheckScribesAnointTheCrown(void)
{
    CcSim sim;
    CcSettlement *place = PrepareIsolated(&sim);
    sim.iron_ledger_reserve = 50;
    sim.archives.scribes = 1;
    sim.kingdoms[0].sanction = 58;
    sim.kingdoms[0].anointed = false;
    place->stock[CC_GOOD_BREAD] = 100;
    place->stock[CC_GOOD_WHEAT] = 4;
    place->stock[CC_GOOD_PAPER] = 1;
    place->stock[CC_GOOD_TOOLS] = 1;
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(sim.kingdoms[0].sanction == 60);
    CC_CHECK(sim.kingdoms[0].anointed);
    CC_CHECK(CountEvents(&sim, CC_EVENT_KING_ANOINTED) == 1);
}

static void CheckScribeKitWear(void)
{
    CcSim sim;
    CcSettlement *place = PrepareIsolated(&sim);
    sim.iron_ledger_reserve = 50;
    sim.archives.scribes = 1;
    place->stock[CC_GOOD_TOOLS] = 2;
    for (int32_t week = 0; week < 7; ++week) {
        place->stock[CC_GOOD_BREAD] = 100;
        place->stock[CC_GOOD_WHEAT] = 4;
        place->stock[CC_GOOD_PAPER] = 1;
        AddNotableEvent(&sim);
        CcSimAdvanceDays(&sim, week == 0 ? 1 : 7);
    }
    CC_CHECK(sim.archives.lore_stored == 7);
    CC_CHECK(sim.archives.kit_tool_wear == 7);
    CC_CHECK(place->stock[CC_GOOD_TOOLS] == 2);

    place->stock[CC_GOOD_BREAD] = 100;
    place->stock[CC_GOOD_WHEAT] = 4;
    place->stock[CC_GOOD_PAPER] = 1;
    AddNotableEvent(&sim);
    CcSimAdvanceDays(&sim, 7);
    CC_CHECK(sim.archives.lore_stored == 8);
    CC_CHECK(sim.archives.kit_tool_wear == 0);
    CC_CHECK(place->stock[CC_GOOD_TOOLS] == 1);
}

static void CheckIronDeliveryRestoresWriting(void)
{
    CcSim sim;
    CcSettlement *place = PrepareIsolated(&sim);
    sim.iron_ledger_reserve = 50;
    sim.archives.scribes = 1;
    place->service_mask |= Service(CC_SERVICE_SMITHY);
    place->stock[CC_GOOD_BREAD] = 100;
    place->stock[CC_GOOD_WHEAT] = 4;
    place->stock[CC_GOOD_PAPER] = 1;
    place->production[CC_GOOD_TOOLS] = 1;
    place->reserve_target[CC_GOOD_TOOLS] = 1;
    AddNotableEvent(&sim);
    CC_CHECK(CcSimMaterialChainSnapshot(&sim).blocker ==
             CC_MATERIAL_CHAIN_TOOLS);
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(sim.archives.lore_stored == 0);
    CC_CHECK(place->stock[CC_GOOD_PAPER] == 1);

    place->stock[CC_GOOD_IRON] = 2;
    place->stock[CC_GOOD_WOOD] = 1;
    place->stock[CC_GOOD_WHEAT] = 4;
    place->stock[CC_GOOD_BREAD] = 100;
    AddNotableEvent(&sim);
    int32_t volumes_before = CountArchiveVolumes(&sim);
    CcSimAdvanceDays(&sim, 7);
    CC_CHECK(place->stock[CC_GOOD_IRON] == 0);
    CC_CHECK(place->stock[CC_GOOD_TOOLS] == 1);
    CC_CHECK(place->stock[CC_GOOD_PAPER] == 0);
    CC_CHECK(sim.archives.lore_stored == 1);
    CC_CHECK(sim.archives.last_recorded_day == 14);
    CC_CHECK(CountEvents(&sim, CC_EVENT_LORE_RECORDED) == 1);
    CC_CHECK(CountArchiveVolumes(&sim) == volumes_before + 1);
}

static void RefillSilentAbbey(CcSettlement *place)
{
    place->stock[CC_GOOD_BREAD] = 100;
    place->stock[CC_GOOD_WHEAT] = 4;
    place->stock[CC_GOOD_PAPER] = 0;
    place->stock[CC_GOOD_TOOLS] = 1;
    place->hunger = 0;
    place->prosperity = 0;
}

static void CheckPretenderYear(void)
{
    CcSim sim;
    CcSettlement *place = PrepareIsolated(&sim);
    sim.iron_ledger_reserve = 50;
    sim.archives.scribes = 1;
    for (int32_t week = 0; week < 51; ++week) {
        RefillSilentAbbey(place);
        CcSimAdvanceDays(&sim, 7);
    }
    CC_CHECK(sim.kingdoms[0].pretender_crises == 0);
    CC_CHECK(sim.kingdoms[0].unsanctioned_weeks == 51);
    CC_CHECK(CountEvents(&sim, CC_EVENT_PRETENDER_CRISIS) == 0);

    RefillSilentAbbey(place);
    CcSimAdvanceDays(&sim, 7);
    CC_CHECK(sim.kingdoms[0].pretender_crises == 1);
    CC_CHECK(sim.kingdoms[0].unsanctioned_weeks == 0);
    CC_CHECK(sim.kingdoms[0].legitimacy == 45);
    CC_CHECK(CountEvents(&sim, CC_EVENT_PRETENDER_CRISIS) == 1);

    RefillSilentAbbey(place);
    CcSimAdvanceDays(&sim, 7);
    CC_CHECK(sim.kingdoms[0].pretender_crises == 1);
    CC_CHECK(sim.kingdoms[0].unsanctioned_weeks == 1);
    CC_CHECK(CountEvents(&sim, CC_EVENT_PRETENDER_CRISIS) == 1);

    for (int32_t week = 0; week < 51; ++week) {
        RefillSilentAbbey(place);
        CcSimAdvanceDays(&sim, 7);
    }
    CC_CHECK(sim.kingdoms[0].pretender_crises == 1);
    CC_CHECK(sim.kingdoms[0].unsanctioned_weeks == 51);
    CC_CHECK(CountEvents(&sim, CC_EVENT_PRETENDER_CRISIS) == 1);
}

static void CheckAbandonedScriptoriumMoves(void)
{
    CcSim sim;
    CcSimInit(&sim, UINT32_C(0xabb37));
    CcId first_id = 0U;
    for (int32_t i = 0; i < sim.settlement_count; ++i) {
        if (strcmp(sim.settlements[i].name, "Gloamgate") == 0) {
            first_id = sim.settlements[i].id;
            sim.settlements[i].population = 0;
            break;
        }
    }
    CC_CHECK(first_id != 0U);
    CcMaterialChainSnapshot snapshot = CcSimMaterialChainSnapshot(&sim);
    CC_CHECK(snapshot.scriptorium_id != 0U);
    CC_CHECK(snapshot.scriptorium_id != first_id);
}

int main(void)
{
    CC_CHECK(CC_SERVICE_MILL == 15);
    CC_CHECK(strcmp(CcServiceName(CC_SERVICE_MILL), "Mill") == 0);
    CC_CHECK(CC_EVENT_PAPER_MILLED == 123);
    CC_CHECK(CC_EVENT_KING_ANOINTED == 124);
    CC_CHECK(CC_EVENT_PRETENDER_CRISIS == 125);
    CheckPaperMillGates();
    CheckLegacyPaperRecipe();
    CheckWoodFeedsPaperAndWheatFeedsScribes();
    CheckPaperOwnsTheTome();
    CheckBindingMaterialsOwnTheTome();
    CheckScribesAnointTheCrown();
    CheckScribeKitWear();
    CheckIronDeliveryRestoresWriting();
    CheckPretenderYear();
    CheckAbandonedScriptoriumMoves();
    return 0;
}
