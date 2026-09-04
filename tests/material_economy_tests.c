#include "sim/cc_sim.h"

#include "test_support.h"
#include <stdio.h>
#include <string.h>

static uint32_t Service(CcServiceKind service)
{
    return UINT32_C(1) << (uint32_t)service;
}

static CcSettlement *IsolatedSettlement(CcSim *sim)
{
    CcSimInit(sim, UINT32_C(0xec0a0a01));
    sim->settlement_count = 1;
    sim->route_count = 0;
    sim->shipment_count = 0;
    sim->bandit_count = 0;
    sim->monster_count = 0;
    sim->dungeon_count = 0;
    sim->situation_count = 0;
    sim->goblins.lair_settlement_id = sim->settlements[0].id;
    sim->goblins.tribute_cooldown_days = 1000;
    sim->dragon.lair_settlement_id = sim->settlements[0].id;
    sim->hoard_raiders.cooldown_days = 1000;
    CcSettlement *place = &sim->settlements[0];
    place->service_mask = Service(CC_SERVICE_INN);
    place->function = CC_SETTLEMENT_FARMING;
    place->field_yield = 0;
    place->iron_deposit = 0;
    place->gold_seam = false;
    place->gem_seam = false;
    place->gold_progress = 0;
    place->gem_progress = 0;
    place->farm_tool_wear = 0;
    place->mine_tool_wear = 0;
    place->smith_tool_wear = 0;
    place->treasure_gold_committed = 0;
    place->treasure_gems_committed = 0;
    place->treasure_work = 0;
    place->market_coins = 100;
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        place->stock[good] = 0;
        place->reserve_target[good] = 0;
        place->production[good] = 0;
        place->consumption[good] = 0;
        place->price[good] = 1;
    }
    return place;
}

int main(void)
{
    CC_CHECK(CC_GOOD_BREAD == 0);
    CC_CHECK(CC_GOOD_FOOD == CC_GOOD_BREAD);
    CC_CHECK(CC_GOOD_IRON == 1);
    CC_CHECK(CC_GOOD_TOOLS == 2);
    CC_CHECK(CC_GOOD_WEAPONS == 3);
    CC_CHECK(CC_GOOD_GOLD == 4);
    CC_CHECK(CC_GOOD_GEMS == 5);
    CC_CHECK(CC_GOOD_WOOD == 6);
    CC_CHECK(CC_GOOD_WHEAT == 7);
    CC_CHECK(CC_GOOD_MEAT == 8);
    CC_CHECK(CC_GOOD_WOOL == 9);
    CC_CHECK(CC_GOOD_STONE == 10);
    CC_CHECK(CC_GOOD_COUNT == 11);
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        const CcGoodDefinition *definition = CcGoodDefinitionFor(
            (CcGood)good);
        CC_CHECK(definition != NULL);
        CC_CHECK(strcmp(definition->name, CcGoodName((CcGood)good)) == 0);
        CC_CHECK(definition->base_price > 0);
        CC_CHECK(definition->freight_units_per_slot > 0);
        CC_CHECK(definition->player_units_per_slot > 0);
        CC_CHECK(definition->minimum_trade_units > 0);
        CC_CHECK(definition->raid_capacity > 0);
    }
    CC_CHECK(!CcGoodIsValid((CcGood)-1));
    CC_CHECK(!CcGoodIsValid(CC_GOOD_COUNT));
    CC_CHECK(CcGoodDefinitionFor(CC_GOOD_COUNT) == NULL);
    const CcGoodDefinition *wood_definition = CcGoodDefinitionFor(
        CC_GOOD_WOOD);
    CC_CHECK(strcmp(wood_definition->name, "Wood") == 0);
    CC_CHECK(wood_definition->base_price == 6);
    CC_CHECK(wood_definition->freight_units_per_slot == 6);
    CC_CHECK(wood_definition->minimum_trade_units == 4);
    CC_CHECK(wood_definition->raid_capacity == 12);

    CcSim wood_world;
    CcSimInit(&wood_world, UINT32_C(0x700d100d));
    CC_CHECK(wood_world.settlements[0].stock[CC_GOOD_WOOD] == 28);
    CC_CHECK(wood_world.settlements[0].reserve_target[CC_GOOD_WOOD] == 18);
    CC_CHECK(wood_world.settlements[0].production[CC_GOOD_WOOD] == 12);
    CC_CHECK(wood_world.settlements[5].stock[CC_GOOD_WOOD] == 36);
    CC_CHECK(wood_world.settlements[5].production[CC_GOOD_WOOD] == 14);
    uint64_t wood_hash = CcSimHash(&wood_world);
    wood_world.settlements[0].stock[CC_GOOD_WOOD] += 1;
    CC_CHECK(CcSimHash(&wood_world) != wood_hash);
    char wood_error[192];
    wood_world.settlements[0].stock[CC_GOOD_WOOD] = -1;
    CC_CHECK(!CcSimValidate(&wood_world, wood_error, sizeof(wood_error)));

    CcSim woodlot;
    CcSettlement *place = IsolatedSettlement(&woodlot);
    place->service_mask |= Service(CC_SERVICE_GRANARY);
    place->production[CC_GOOD_WOOD] = 8;
    place->stock[CC_GOOD_TOOLS] = 2;
    place->stock[CC_GOOD_FOOD] = 100;
    place->consumption[CC_GOOD_FOOD] = 1;
    place->cow_adults = 0;
    place->cow_calves = 0;
    int32_t tracked_wood = CcSimTrackedGood(&woodlot, CC_GOOD_WOOD);
    CcSimAdvanceDays(&woodlot, 83);
    CC_CHECK(place->stock[CC_GOOD_WOOD] == 96);
    CC_CHECK(place->stock[CC_GOOD_TOOLS] == 1);
    CC_CHECK(CcSimTrackedGood(&woodlot, CC_GOOD_WOOD) == tracked_wood + 96);
    bool harvest_recorded = false;
    for (int32_t event = 0; event < woodlot.event_count; ++event) {
        const CcEvent *item = CcSimRecentEvent(&woodlot, event);
        if (item != NULL && item->kind == CC_EVENT_WOODLOT_HARVEST) {
            harvest_recorded = true;
        }
    }
    CC_CHECK(harvest_recorded);

    CcSim hand_woodlot;
    place = IsolatedSettlement(&hand_woodlot);
    place->production[CC_GOOD_WOOD] = 8;
    CcSimAdvanceDays(&hand_woodlot, 6);
    CC_CHECK(place->stock[CC_GOOD_WOOD] == 2);

    CcSim no_farm;
    place = IsolatedSettlement(&no_farm);
    place->field_yield = 100;
    place->production[CC_GOOD_FOOD] = 8;
    CcSimAdvanceDays(&no_farm, 7);
    CC_CHECK(place->stock[CC_GOOD_FOOD] == 0);

    CcSim farm;
    place = IsolatedSettlement(&farm);
    place->service_mask |= Service(CC_SERVICE_FARM);
    place->field_yield = 100;
    place->production[CC_GOOD_FOOD] = 8;
    place->stock[CC_GOOD_TOOLS] = 1;
    CcSimAdvanceDays(&farm, 7);
    CC_CHECK(place->stock[CC_GOOD_FOOD] > 0);
    CC_CHECK(place->farm_tool_wear == 1);

    CcSim pantry;
    place = IsolatedSettlement(&pantry);
    place->stock[CC_GOOD_FOOD] = 1000;
    place->consumption[CC_GOOD_FOOD] = 5;
    CcSimAdvanceDays(&pantry, 7);
    CC_CHECK(place->stock[CC_GOOD_FOOD] == 60);

    CcSim granary;
    place = IsolatedSettlement(&granary);
    place->service_mask |= Service(CC_SERVICE_GRANARY);
    place->stock[CC_GOOD_FOOD] = 1000;
    place->consumption[CC_GOOD_FOOD] = 5;
    CcSimAdvanceDays(&granary, 7);
    CC_CHECK(place->stock[CC_GOOD_FOOD] == 160);

    CcSim ordinary_spoilage;
    place = IsolatedSettlement(&ordinary_spoilage);
    place->service_mask |= Service(CC_SERVICE_GRANARY);
    place->stock[CC_GOOD_FOOD] = 150;
    place->consumption[CC_GOOD_FOOD] = 5;
    CcSimAdvanceDays(&ordinary_spoilage, 7);
    CC_CHECK(place->stock[CC_GOOD_FOOD] == 144);

    CcSim fed;
    place = IsolatedSettlement(&fed);
    place->stock[CC_GOOD_FOOD] = 7;
    place->consumption[CC_GOOD_FOOD] = 7;
    place->hunger = 50;
    CcSimAdvanceDays(&fed, 7);
    CC_CHECK(place->stock[CC_GOOD_FOOD] == 0);
    CC_CHECK(place->hunger == 49);

    CcSim starving;
    place = IsolatedSettlement(&starving);
    place->consumption[CC_GOOD_FOOD] = 7;
    place->hunger = 50;
    CcSimAdvanceDays(&starving, 7);
    CC_CHECK(place->hunger == 56);

    CcSim tool_convoy;
    CcSimInit(&tool_convoy, UINT32_C(0x7001c0a7));
    tool_convoy.settlement_count = 2;
    tool_convoy.route_count = 1;
    tool_convoy.shipment_count = 0;
    tool_convoy.bandit_count = 0;
    tool_convoy.monster_count = 0;
    tool_convoy.dungeon_count = 0;
    tool_convoy.situation_count = 0;
    tool_convoy.goblins.tribute_cooldown_days = 1000;
    tool_convoy.hoard_raiders.cooldown_days = 1000;
    CcSettlement *tool_source = &tool_convoy.settlements[0];
    CcSettlement *tool_buyer = &tool_convoy.settlements[1];
    for (int32_t kingdom = 0;
         kingdom < tool_convoy.kingdom_count; ++kingdom) {
        tool_convoy.kingdoms[kingdom].treasury = 0;
    }
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        tool_source->production[good] = 0;
        tool_buyer->production[good] = 0;
        tool_source->consumption[good] = 0;
        tool_buyer->consumption[good] = 0;
        tool_source->stock[good] = 0;
        tool_buyer->stock[good] = 0;
        tool_source->reserve_target[good] = 0;
        tool_buyer->reserve_target[good] = 0;
        tool_source->price[good] = 1;
        tool_buyer->price[good] = 1;
    }
    tool_source->stock[CC_GOOD_TOOLS] = 10;
    tool_buyer->reserve_target[CC_GOOD_TOOLS] = 4;
    tool_source->market_coins = 100;
    tool_buyer->market_coins = 100;
    tool_convoy.routes[0].closed = true;
    tool_convoy.routes[0].condition = 10;
    tool_convoy.routes[0].security = 100;
    tool_convoy.routes[0].smuggler_route = false;
    CcMoney tool_gold_before = CcSimTrackedGold(&tool_convoy);
    CcSimAdvanceDays(&tool_convoy, 6);
    CC_CHECK(tool_convoy.shipment_count > 0);
    CC_CHECK(tool_convoy.shipments[0].good == CC_GOOD_TOOLS);
    CC_CHECK(tool_convoy.shipments[0].quantity > 0);
    CC_CHECK(tool_convoy.shipments[0].quantity <= 4);
    CC_CHECK(tool_convoy.shipments[0].status == CC_SHIPMENT_TRAVELLING);
    CC_CHECK(tool_convoy.routes[0].closed);
    CC_CHECK(tool_convoy.kingdoms[0].treasury == 4);
    CC_CHECK(tool_source->market_coins > 100);
    CC_CHECK(tool_buyer->market_coins < 100);
    CC_CHECK((tool_source->market_coins - 100) +
             (tool_buyer->market_coins - 100) +
             tool_convoy.kingdoms[0].treasury == 0);
    CC_CHECK(CcSimTrackedGold(&tool_convoy) == tool_gold_before);

    CcSim wood_convoy = tool_convoy;
    wood_convoy.shipment_count = 0;
    for (int32_t kingdom = 0;
         kingdom < wood_convoy.kingdom_count; ++kingdom) {
        wood_convoy.kingdoms[kingdom].treasury = 0;
    }
    CcSettlement *wood_source = &wood_convoy.settlements[0];
    CcSettlement *wood_buyer = &wood_convoy.settlements[1];
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        wood_source->production[good] = 0;
        wood_buyer->production[good] = 0;
        wood_source->consumption[good] = 0;
        wood_buyer->consumption[good] = 0;
        wood_source->stock[good] = 0;
        wood_buyer->stock[good] = 0;
        wood_source->reserve_target[good] = 0;
        wood_buyer->reserve_target[good] = 0;
    }
    wood_source->stock[CC_GOOD_WOOD] = 24;
    wood_source->reserve_target[CC_GOOD_WOOD] = 4;
    wood_buyer->reserve_target[CC_GOOD_WOOD] = 12;
    wood_source->market_coins = 100;
    wood_buyer->market_coins = 100;
    int32_t wood_before_trade = CcSimTrackedGood(
        &wood_convoy, CC_GOOD_WOOD);
    CcSimAdvanceDays(&wood_convoy, 7);
    CC_CHECK(wood_convoy.shipment_count > 0);
    CC_CHECK(wood_convoy.shipments[0].good == CC_GOOD_WOOD);
    CC_CHECK(wood_convoy.shipments[0].quantity >= 4);
    CC_CHECK(wood_convoy.shipments[0].status == CC_SHIPMENT_TRAVELLING);
    CC_CHECK(CcSimTrackedGood(&wood_convoy, CC_GOOD_WOOD) ==
             wood_before_trade);

    CcSim famine_convoy;
    CcSimInit(&famine_convoy, UINT32_C(0xfa61ce01));
    famine_convoy.settlement_count = 2;
    famine_convoy.route_count = 1;
    famine_convoy.shipment_count = 0;
    famine_convoy.bandit_count = 0;
    famine_convoy.monster_count = 0;
    famine_convoy.dungeon_count = 0;
    famine_convoy.situation_count = 0;
    famine_convoy.goblins.tribute_cooldown_days = 1000;
    famine_convoy.hoard_raiders.cooldown_days = 1000;
    CcSettlement *source = &famine_convoy.settlements[0];
    CcSettlement *hungry = &famine_convoy.settlements[1];
    for (int32_t kingdom = 0;
         kingdom < famine_convoy.kingdom_count; ++kingdom) {
        famine_convoy.kingdoms[kingdom].treasury = 0;
    }
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        source->production[good] = 0;
        hungry->production[good] = 0;
        source->consumption[good] = 0;
        hungry->consumption[good] = 0;
        source->stock[good] = 0;
        hungry->stock[good] = 0;
        source->reserve_target[good] = 0;
        hungry->reserve_target[good] = 0;
    }
    source->service_mask |= Service(CC_SERVICE_GRANARY);
    source->stock[CC_GOOD_FOOD] = 200;
    source->reserve_target[CC_GOOD_FOOD] = 20;
    source->consumption[CC_GOOD_FOOD] = 5;
    hungry->stock[CC_GOOD_FOOD] = 0;
    hungry->reserve_target[CC_GOOD_FOOD] = 60;
    hungry->consumption[CC_GOOD_FOOD] = 5;
    hungry->market_coins = 0;
    hungry->hunger = 65;
    famine_convoy.routes[0].closed = true;
    famine_convoy.routes[0].condition = 10;
    famine_convoy.routes[0].security = 100;
    famine_convoy.routes[0].smuggler_route = false;
    CcSim blocked_famine = famine_convoy;
    blocked_famine.settlements[1].kingdom_id =
        blocked_famine.kingdoms[1].id;
    CcSimAdvanceDays(&blocked_famine, 6);
    CC_CHECK(blocked_famine.shipment_count == 0);
    CcSimAdvanceDays(&blocked_famine, 21);
    CC_CHECK(blocked_famine.routes[0].smuggler_route);
    CC_CHECK(!blocked_famine.routes[0].closed);
    CC_CHECK(blocked_famine.shipment_count == 0);
    CcMoney gold_before_credit = CcSimTrackedGold(&famine_convoy);
    CcMoney reserve_before_credit = famine_convoy.iron_ledger_reserve;
    CcSimAdvanceDays(&famine_convoy, 6);
    CC_CHECK(famine_convoy.shipment_count > 0);
    CC_CHECK(famine_convoy.shipments[0].good == CC_GOOD_FOOD);
    CC_CHECK(famine_convoy.shipments[0].status == CC_SHIPMENT_TRAVELLING);
    CC_CHECK(famine_convoy.routes[0].closed);
    CC_CHECK(famine_convoy.kingdoms[0].iron_ledger_debt > 0);
    CC_CHECK(famine_convoy.iron_ledger_reserve < reserve_before_credit);
    CC_CHECK(CcSimTrackedGold(&famine_convoy) == gold_before_credit);
    bool loan_recorded = false;
    for (int32_t event = 0; event < famine_convoy.event_count; ++event) {
        const CcEvent *item = CcSimRecentEvent(&famine_convoy, event);
        if (item != NULL && item->kind == CC_EVENT_IRON_LEDGER_LOAN) {
            loan_recorded = true;
        }
    }
    CC_CHECK(loan_recorded);
    CcSim mine;
    place = IsolatedSettlement(&mine);
    place->service_mask |= Service(CC_SERVICE_MINE);
    place->production[CC_GOOD_IRON] = 4;
    place->iron_deposit = 20;
    place->gold_seam = true;
    place->gem_seam = true;
    place->stock[CC_GOOD_TOOLS] = 1;
    CcSimAdvanceDays(&mine, 7);
    CC_CHECK(place->stock[CC_GOOD_IRON] == 4);
    CC_CHECK(place->iron_deposit == 16);
    CC_CHECK(place->gold_progress == 1);
    CC_CHECK(place->gem_progress == 1);

    CcSim hand_mine;
    place = IsolatedSettlement(&hand_mine);
    place->service_mask |= Service(CC_SERVICE_MINE);
    place->production[CC_GOOD_IRON] = 4;
    place->iron_deposit = 20;
    place->gold_seam = true;
    CcSimAdvanceDays(&hand_mine, 7);
    CC_CHECK(place->stock[CC_GOOD_IRON] == 1);
    CC_CHECK(place->gold_progress == 0);

    CcSim smithy;
    place = IsolatedSettlement(&smithy);
    place->service_mask |= Service(CC_SERVICE_SMITHY);
    place->function = CC_SETTLEMENT_FORTRESS;
    place->stock[CC_GOOD_IRON] = 10;
    place->stock[CC_GOOD_WOOD] = 3;
    place->stock[CC_GOOD_TOOLS] = 10;
    place->reserve_target[CC_GOOD_TOOLS] = 10;
    place->reserve_target[CC_GOOD_WEAPONS] = 10;
    place->production[CC_GOOD_TOOLS] = 1;
    place->production[CC_GOOD_WEAPONS] = 1;
    CcSimAdvanceDays(&smithy, 7);
    CC_CHECK(place->stock[CC_GOOD_IRON] == 5);
    CC_CHECK(place->stock[CC_GOOD_WOOD] == 0);
    CC_CHECK(place->stock[CC_GOOD_TOOLS] == 11);
    CC_CHECK(place->stock[CC_GOOD_WEAPONS] == 1);
    CC_CHECK(place->smith_tool_wear == 2);

    CcSim woodless_smithy;
    place = IsolatedSettlement(&woodless_smithy);
    place->service_mask |= Service(CC_SERVICE_SMITHY);
    place->stock[CC_GOOD_IRON] = 10;
    place->stock[CC_GOOD_TOOLS] = 10;
    place->reserve_target[CC_GOOD_TOOLS] = 10;
    place->reserve_target[CC_GOOD_WEAPONS] = 10;
    place->production[CC_GOOD_TOOLS] = 1;
    place->production[CC_GOOD_WEAPONS] = 1;
    CcSimAdvanceDays(&woodless_smithy, 7);
    CC_CHECK(place->stock[CC_GOOD_IRON] == 10);
    CC_CHECK(place->stock[CC_GOOD_TOOLS] == 10);
    CC_CHECK(place->stock[CC_GOOD_WEAPONS] == 0);

    CcSim treasure_sim;
    place = IsolatedSettlement(&treasure_sim);
    place->service_mask |= Service(CC_SERVICE_SMITHY);
    place->function = CC_SETTLEMENT_MARKET;
    place->stock[CC_GOOD_GOLD] = 1;
    place->stock[CC_GOOD_GEMS] = 1;
    CcSimAdvanceDays(&treasure_sim, 21);
    CC_CHECK(treasure_sim.treasure_count == 1);
    const CcTreasure *treasure = &treasure_sim.treasures[0];
    CC_CHECK(treasure->gold_content == 1);
    CC_CHECK(treasure->gem_content == 1);
    CC_CHECK(treasure->craft_work == 3);
    CC_CHECK(treasure->owner_id == place->id);
    CC_CHECK(place->stock[CC_GOOD_GOLD] == 0);
    CC_CHECK(place->stock[CC_GOOD_GEMS] == 0);

    char trade_error[192];
    treasure_sim.player.coins = 500;
    treasure_sim.player.cargo[CC_GOOD_FOOD] =
        treasure_sim.player.cargo_capacity;
    CcCommand buy_treasure = {
        .kind = CC_COMMAND_BUY_TREASURE,
        .target_id = treasure->id
    };
    CC_CHECK(!CcSimApply(&treasure_sim, &buy_treasure,
                         trade_error, sizeof(trade_error)));
    treasure_sim.player.cargo[CC_GOOD_FOOD] -= 1;
    CC_CHECK(CcSimApply(&treasure_sim, &buy_treasure,
                        trade_error, sizeof(trade_error)));
    CC_CHECK(treasure_sim.player.treasure_cargo_slots == 1);
    CC_CHECK(treasure_sim.treasures[0].owner_id == treasure_sim.player.id);
    CC_CHECK(CcPlayerCargoUsed(&treasure_sim.player) ==
             treasure_sim.player.cargo_capacity);
    CcCommand sell_treasure = {
        .kind = CC_COMMAND_SELL_TREASURE,
        .target_id = treasure->id
    };
    CC_CHECK(CcSimApply(&treasure_sim, &sell_treasure,
                        trade_error, sizeof(trade_error)));
    CC_CHECK(treasure_sim.player.treasure_cargo_slots == 0);
    CC_CHECK(treasure_sim.treasures[0].owner_id == place->id);

    CcPlayerCompany cargo = {0};
    cargo.cargo[CC_GOOD_FOOD] = 9;
    cargo.cargo[CC_GOOD_IRON] = 4;
    cargo.cargo[CC_GOOD_TOOLS] = 3;
    cargo.cargo[CC_GOOD_WEAPONS] = 1;
    cargo.cargo[CC_GOOD_GOLD] = 1;
    cargo.cargo[CC_GOOD_GEMS] = 1;
    cargo.treasure_cargo_slots = 1;
    CC_CHECK(CcPlayerCargoUsed(&cargo) == 20);

    puts("Material economy tests passed");
    return 0;
}
