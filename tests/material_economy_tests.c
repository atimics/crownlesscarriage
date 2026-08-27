#include "sim/cc_sim.h"

#include "test_support.h"
#include <stdio.h>

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
    CcSim no_farm;
    CcSettlement *place = IsolatedSettlement(&no_farm);
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
    place->stock[CC_GOOD_TOOLS] = 10;
    place->reserve_target[CC_GOOD_TOOLS] = 10;
    place->reserve_target[CC_GOOD_WEAPONS] = 10;
    place->production[CC_GOOD_TOOLS] = 1;
    place->production[CC_GOOD_WEAPONS] = 1;
    CcSimAdvanceDays(&smithy, 7);
    CC_CHECK(place->stock[CC_GOOD_IRON] == 5);
    CC_CHECK(place->stock[CC_GOOD_TOOLS] == 11);
    CC_CHECK(place->stock[CC_GOOD_WEAPONS] == 1);
    CC_CHECK(place->smith_tool_wear == 2);

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
        treasure_sim.player.cargo_capacity * 8;
    CcCommand buy_treasure = {
        .kind = CC_COMMAND_BUY_TREASURE,
        .target_id = treasure->id
    };
    CC_CHECK(!CcSimApply(&treasure_sim, &buy_treasure,
                         trade_error, sizeof(trade_error)));
    treasure_sim.player.cargo[CC_GOOD_FOOD] -= 8;
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
    CC_CHECK(CcPlayerCargoUsed(&cargo) == 9);

    puts("Material economy tests passed");
    return 0;
}
