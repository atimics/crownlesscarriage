#include "sim/cc_sim.h"

#include "test_support.h"
#include <stdio.h>

static void QuietWorld(CcSim *sim, uint32_t seed)
{
    CcSimInit(sim, seed);
    sim->settlement_count = 1;
    sim->route_count = 0;
    sim->shipment_count = 0;
    sim->courier_count = 0;
    sim->bandit_count = 0;
    sim->monster_count = 0;
    sim->dungeon_count = 0;
    sim->situation_count = 0;
    sim->dragon.slain = true;
    sim->dragon.egg_count = 0;
    sim->goblins.tribute_cooldown_days = 1000;
    sim->hoard_raiders.cooldown_days = 1000;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        sim->settlements[i].cow_adults = 0;
        sim->settlements[i].cow_calves = 0;
        sim->settlements[i].cow_condition = 0;
        sim->settlements[i].cow_hunger = 0;
    }
    for (int32_t kingdom = 0;
         kingdom < sim->kingdom_count; ++kingdom) {
        sim->kingdoms[kingdom].treasury = 0;
    }
}

static int32_t AverageHunger(const CcSim *sim)
{
    int32_t total = 0;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        total += sim->settlements[i].hunger;
    }
    return total / sim->settlement_count;
}

static int32_t MaximumHunger(const CcSim *sim)
{
    int32_t maximum = 0;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        if (sim->settlements[i].hunger > maximum) {
            maximum = sim->settlements[i].hunger;
        }
    }
    return maximum;
}

int main(void)
{
    char error[192];

    CcSim contracted;
    QuietWorld(&contracted, UINT32_C(0xba1a0ce1));
    CcSettlement *survivors = &contracted.settlements[0];
    survivors->population = 299;
    survivors->hunger = 0;
    survivors->stock[CC_GOOD_FOOD] = 7;
    survivors->stock[CC_GOOD_WHEAT] = 0;
    survivors->stock[CC_GOOD_MEAT] = 0;
    survivors->production[CC_GOOD_FOOD] = 0;
    survivors->production[CC_GOOD_WHEAT] = 0;
    survivors->consumption[CC_GOOD_FOOD] = 7;
    CcSimAdvanceDays(&contracted, 7);

    CC_CHECK(survivors->stock[CC_GOOD_FOOD] == 6);

    survivors->hunger = 20;
    survivors->stock[CC_GOOD_FOOD] = 0;
    CcSimAdvanceDays(&contracted, 7);
    CC_CHECK(survivors->hunger > 20);
    CC_CHECK(survivors->stock[CC_GOOD_FOOD] == 0);

    CcSim shared_granary;
    CcSimInit(&shared_granary, UINT32_C(0xba1a0ce3));
    shared_granary.settlement_count = 2;
    shared_granary.route_count = 1;
    shared_granary.shipment_count = 0;
    shared_granary.bandit_count = 0;
    shared_granary.monster_count = 0;
    shared_granary.dungeon_count = 0;
    shared_granary.situation_count = 0;
    shared_granary.goblins.tribute_cooldown_days = 1000;
    shared_granary.hoard_raiders.cooldown_days = 1000;
    CcSettlement *granary = &shared_granary.settlements[0];
    CcSettlement *famine = &shared_granary.settlements[1];
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        granary->stock[good] = 0;
        granary->reserve_target[good] = 0;
        granary->production[good] = 0;
        granary->consumption[good] = 0;
        famine->stock[good] = 0;
        famine->reserve_target[good] = 0;
        famine->production[good] = 0;
        famine->consumption[good] = 0;
    }
    granary->stock[CC_GOOD_FOOD] = 80;
    granary->reserve_target[CC_GOOD_FOOD] = 100;
    granary->consumption[CC_GOOD_FOOD] = 5;
    granary->hunger = 0;
    famine->reserve_target[CC_GOOD_FOOD] = 60;
    famine->hunger = 80;
    famine->market_coins = 1000;
    shared_granary.routes[0].closed = false;
    shared_granary.routes[0].condition = 100;
    shared_granary.routes[0].security = 100;
    shared_granary.routes[0].smuggler_route = false;
    CcSimAdvanceDays(&shared_granary, 6);
    CC_CHECK(shared_granary.shipment_count > 0);
    CC_CHECK(shared_granary.shipments[0].good == CC_GOOD_FOOD);
    CC_CHECK(granary->stock[CC_GOOD_FOOD] < 80);

    CcSim road_work;
    CcSimInit(&road_work, UINT32_C(0xba1a0ce2));
    road_work.settlement_count = 2;
    road_work.route_count = 1;
    road_work.shipment_count = 0;
    road_work.courier_count = 0;
    road_work.bandit_count = 0;
    road_work.monster_count = 0;
    road_work.dungeon_count = 0;
    road_work.situation_count = 0;
    road_work.dragon.slain = true;
    road_work.dragon.egg_count = 0;
    road_work.goblins.tribute_cooldown_days = 1000;
    road_work.hoard_raiders.cooldown_days = 1000;
    for (int32_t kingdom = 0;
         kingdom < road_work.kingdom_count; ++kingdom) {
        road_work.kingdoms[kingdom].treasury = 0;
    }
    for (int32_t place = 0;
         place < road_work.settlement_count; ++place) {
        road_work.settlements[place].market_coins = 0;
        road_work.settlements[place].war_chest = 0;
        road_work.settlements[place].service_mask = 0U;
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
            road_work.settlements[place].stock[good] = 0;
            road_work.settlements[place].reserve_target[good] = 0;
            road_work.settlements[place].production[good] = 0;
            road_work.settlements[place].consumption[good] = 0;
        }
    }
    road_work.iron_ledger_reserve = 0;
    road_work.routes[0].closed = true;
    road_work.routes[0].condition = 20;
    road_work.routes[0].smuggler_route = false;
    int32_t road_population = road_work.settlements[1].population;
    CcSimAdvanceDays(&road_work, 364);
    CC_CHECK(road_work.routes[0].closed);
    road_work.settlements[1].stock[CC_GOOD_FOOD] = 40;
    road_work.settlements[1].stock[CC_GOOD_TOOLS] = 10;
    road_work.settlements[1].stock[CC_GOOD_WOOD] = 20;
    CcSimAdvanceDays(&road_work, 560);
    CC_CHECK(!road_work.routes[0].closed);
    CC_CHECK(road_work.settlements[1].population < road_population);

    CcSim ruin;
    QuietWorld(&ruin, UINT32_C(0xba1abadd));
    CcSettlement *failed = &ruin.settlements[0];
    failed->population = 79;
    failed->hunger = 90;
    failed->stock[CC_GOOD_FOOD] = 0;
    failed->production[CC_GOOD_FOOD] = 0;
    failed->consumption[CC_GOOD_FOOD] = 7;
    CcSimAdvanceDays(&ruin, 27);
    CC_CHECK(CcSettlementIsAbandoned(failed));
    CC_CHECK(failed->service_mask == 0U);
    CC_CHECK(failed->prosperity == 0);

    CcSim reclaimed;
    CcSimInit(&reclaimed, UINT32_C(0xba1a5eed));
    reclaimed.settlement_count = 2;
    reclaimed.route_count = 1;
    reclaimed.shipment_count = 0;
    reclaimed.courier_count = 0;
    reclaimed.bandit_count = 0;
    reclaimed.monster_count = 0;
    reclaimed.dungeon_count = 0;
    reclaimed.situation_count = 0;
    reclaimed.dragon.slain = true;
    reclaimed.goblins.tribute_cooldown_days = 10000;
    reclaimed.hoard_raiders.cooldown_days = 10000;
    CcSettlement *donor = &reclaimed.settlements[0];
    CcSettlement *old_ruin = &reclaimed.settlements[1];
    old_ruin->population = 0;
    old_ruin->security = 0;
    old_ruin->prosperity = 0;
    old_ruin->hunger = 100;
    old_ruin->service_mask = 0U;
    old_ruin->service_project = CC_SERVICE_NONE;
    old_ruin->service_project_days = 0;
    donor->population = 2000;
    donor->hunger = 0;
    donor->prosperity = 90;
    donor->stock[CC_GOOD_FOOD] = 100;
    donor->stock[CC_GOOD_TOOLS] = 10;
    donor->market_coins = 100;
    reclaimed.current_day = 379 * 7 - 7;
    CcSimAdvanceDays(&reclaimed, 7);
    CC_CHECK(!CcSettlementIsAbandoned(old_ruin));
    CC_CHECK(old_ruin->population == 180);
    CC_CHECK(old_ruin->kingdom_id == donor->kingdom_id);
    CC_CHECK(old_ruin->service_mask != 0U);

    CcSim defaulted;
    CcSimInit(&defaulted, UINT32_C(0xba1adeb7));
    CcKingdom *debtor = &defaulted.kingdoms[0];
    debtor->iron_ledger_debt = 500;
    debtor->treasury = 0;
    debtor->legitimacy = 80;
    for (int32_t place = 0;
         place < defaulted.settlement_count; ++place) {
        if (defaulted.settlements[place].kingdom_id == debtor->id) {
            defaulted.settlements[place].market_coins = 0;
        }
    }
    defaulted.current_day = 10 * 364 - 7;
    CcSimAdvanceDays(&defaulted, 7);
    CC_CHECK(debtor->iron_ledger_debt == 0);
    CC_CHECK(debtor->legitimacy <= 56);

    CcSim climate;
    CcSimInit(&climate, UINT32_C(0xc11a7e00));
    int32_t first_climate = CcSimClimateFactor(&climate);
    bool climate_changed = false;
    for (int32_t era = 1; era <= 12; ++era) {
        climate.current_day = era * 40 * 364 + 1;
        int32_t factor = CcSimClimateFactor(&climate);
        CC_CHECK(factor >= 58 && factor <= 132);
        if (factor != first_climate) climate_changed = true;
    }
    CC_CHECK(climate_changed);

    int32_t samples = 0;
    int32_t collapse_samples = 0;
    int32_t crisis_samples = 0;
    int32_t quiet_samples = 0;
    int32_t scarred_samples = 0;
    int32_t war_samples = 0;
    int32_t peace_samples = 0;
    for (uint32_t seed_number = 1; seed_number <= 4; ++seed_number) {
        CcSim sim;
        CcSimInit(&sim, seed_number * UINT32_C(0x9e3779b9));
        int32_t starting_population[CC_MAX_SETTLEMENTS];
        for (int32_t place = 0; place < sim.settlement_count; ++place) {
            starting_population[place] = sim.settlements[place].population;
        }
        for (int32_t year = 1; year <= 120; ++year) {
            CcSimAdvanceDays(&sim, 365);
            CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
            if (year < 20) continue;
            samples += 1;
            int32_t average_hunger = AverageHunger(&sim);
            int32_t maximum_hunger = MaximumHunger(&sim);
            if (average_hunger >= 60) collapse_samples += 1;
            if (maximum_hunger >= 40) crisis_samples += 1;
            if (maximum_hunger < 25) quiet_samples += 1;
            bool at_war = false;
            for (int32_t first = 0;
                 first < sim.kingdom_count; ++first) {
                for (int32_t second = first + 1;
                     second < sim.kingdom_count; ++second) {
                    if (sim.diplomacy[first][second] ==
                        CC_DIPLOMACY_WAR) at_war = true;
                }
            }
            if (at_war) war_samples += 1;
            else peace_samples += 1;
            for (int32_t place = 0;
                 place < sim.settlement_count; ++place) {
                if (sim.settlements[place].population <
                    starting_population[place] * 3 / 4) {
                    scarred_samples += 1;
                    break;
                }
            }
        }
        int32_t legitimacy = 0;
        for (int32_t kingdom = 0;
             kingdom < sim.kingdom_count; ++kingdom) {
            legitimacy += sim.kingdoms[kingdom].legitimacy;
        }
        CC_CHECK(legitimacy > 0);
    }
    (void)printf(
        "balance samples=%d collapse=%d crisis=%d quiet=%d scars=%d war=%d peace=%d\n",
        samples, collapse_samples, crisis_samples, quiet_samples, scarred_samples,
        war_samples, peace_samples);
    CC_CHECK(samples > 0);
    CC_CHECK(collapse_samples > 0);
    CC_CHECK(collapse_samples < samples / 5);
    CC_CHECK(crisis_samples > samples / 10);

    /* Closed realm borders make quiet years rarer without Crownless deliveries.
       At least one year in twelve should still stay calm. */
    CC_CHECK(quiet_samples * 12 >= samples);
    CC_CHECK(scarred_samples > 0);
    CC_CHECK(war_samples > 0);
    CC_CHECK(peace_samples > samples / 10);

    puts("OSR balance and long-run recovery tests passed");
    return 0;
}
