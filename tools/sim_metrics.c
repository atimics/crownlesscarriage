#include "sim/cc_sim.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool ParsePositive(const char *text, int32_t *value)
{
    char *end = NULL;
    long parsed = strtol(text, &end, 10);
    if (end == text || *end != '\0' || parsed < 1 || parsed > INT32_MAX) {
        return false;
    }
    *value = (int32_t)parsed;
    return true;
}

static void PrintYear(const CcSim *sim, int32_t seed_number, int32_t year)
{
    int32_t hunger_total = 0;
    int32_t hunger_maximum = 0;
    int32_t prosperity_total = 0;
    int32_t prosperity_minimum = 100;
    int32_t prosperity_maximum = 0;
    int32_t security_total = 0;
    int32_t security_minimum = 100;
    int32_t security_maximum = 0;
    int32_t food_price_total = 0;
    int32_t food_price_maximum = 0;
    int32_t inequality_total = 0;
    int32_t inequality_maximum = 0;
    int32_t war_burden_total = 0;
    int32_t war_burden_maximum = 0;
    CcMoney market_coins = 0;
    CcMoney war_chests = 0;
    int32_t food_stock = 0;
    int32_t war_crisis_total = 0;
    int32_t war_crisis_maximum = 0;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        const CcSettlement *place = &sim->settlements[i];
        hunger_total += place->hunger;
        if (place->hunger > hunger_maximum) hunger_maximum = place->hunger;
        prosperity_total += place->prosperity;
        if (place->prosperity < prosperity_minimum) {
            prosperity_minimum = place->prosperity;
        }
        if (place->prosperity > prosperity_maximum) {
            prosperity_maximum = place->prosperity;
        }
        security_total += place->security;
        if (place->security < security_minimum) security_minimum = place->security;
        if (place->security > security_maximum) security_maximum = place->security;
        food_price_total += place->price[CC_GOOD_FOOD];
        if (place->price[CC_GOOD_FOOD] > food_price_maximum) {
            food_price_maximum = place->price[CC_GOOD_FOOD];
        }
        int32_t inequality = CcSimInequalityAtSettlement(sim, place->id);
        inequality_total += inequality;
        if (inequality > inequality_maximum) inequality_maximum = inequality;
        int32_t war_burden = CcSimWarBurdenAtSettlement(sim, place->id);
        war_burden_total += war_burden;
        if (war_burden > war_burden_maximum) {
            war_burden_maximum = war_burden;
        }
        market_coins += place->market_coins;
        war_chests += place->war_chest;
        food_stock += place->stock[CC_GOOD_FOOD];
        int32_t war_crisis = CcSimWarSupplyCrisisAtSettlement(
            sim, place->id);
        war_crisis_total += war_crisis;
        if (war_crisis > war_crisis_maximum) {
            war_crisis_maximum = war_crisis;
        }
    }
    int32_t closed_routes = 0;
    for (int32_t i = 0; i < sim->route_count; ++i) {
        if (sim->routes[i].closed) closed_routes += 1;
    }
    int32_t legitimacy_total = 0;
    CcMoney treasury_total = 0;
    for (int32_t i = 0; i < sim->kingdom_count; ++i) {
        legitimacy_total += sim->kingdoms[i].legitimacy;
        treasury_total += sim->kingdoms[i].treasury;
    }
    (void)printf(
        "%d,%u,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
        "%d,%" PRId64 ",%" PRId64 ",%d,",
        seed_number, sim->world_seed, year,
        hunger_total / sim->settlement_count, hunger_maximum,
        prosperity_total / sim->settlement_count,
        prosperity_minimum, prosperity_maximum,
        security_total / sim->settlement_count,
        security_minimum, security_maximum,
        food_price_total / sim->settlement_count, food_price_maximum,
        closed_routes, CcSimActiveSituationCount(sim),
        sim->bandit_count > 0 ? sim->bandits[0].influence : 0,
        sim->monster_count > 0 ? sim->monsters[0].pressure : 0,
        legitimacy_total / sim->kingdom_count, sim->shipment_count,
        sim->event_count, sim->goblins.tributes_delivered,
        sim->dragon.hoard, sim->dragon.stolen_outstanding,
        sim->dragon.retaliations);
    (void)printf(
        "%d,%d,%d,%d,%d,%d,%d,%" PRId64 ",%" PRId64
        ",%" PRId64 ",%d,%d,%d,%" PRId64 ",%d,%d,%d,%d,%d,%d,%d,%d\n",
        inequality_total / sim->settlement_count, inequality_maximum,
        war_burden_total / sim->settlement_count, war_burden_maximum,
        sim->hoard_raiders.raids_completed,
        sim->hoard_raiders.raids_completed -
            sim->hoard_raiders.war_raids_completed,
        sim->hoard_raiders.war_raids_completed,
        CcSimTrackedGold(sim), market_coins, war_chests, food_stock,
        war_crisis_total / sim->settlement_count, war_crisis_maximum,
        treasury_total, sim->treasure_count,
        sim->goblins.lair_stock[CC_GOOD_FOOD],
        sim->goblins.lair_stock[CC_GOOD_WEAPONS],
        sim->dragon.hoard_goods[CC_GOOD_GOLD],
        sim->dragon.hoard_goods[CC_GOOD_GEMS],
        CcSimTrackedGood(sim, CC_GOOD_IRON),
        CcSimTrackedGood(sim, CC_GOOD_TOOLS),
        CcSimTrackedGood(sim, CC_GOOD_WEAPONS));
}

int main(int argc, char **argv)
{
    int32_t seeds = 100;
    int32_t years = 10;
    for (int32_t argument = 1; argument < argc; ++argument) {
        if (strcmp(argv[argument], "--seeds") == 0 && argument + 1 < argc) {
            if (!ParsePositive(argv[++argument], &seeds)) return EXIT_FAILURE;
        } else if (strcmp(argv[argument], "--years") == 0 &&
                   argument + 1 < argc) {
            if (!ParsePositive(argv[++argument], &years)) return EXIT_FAILURE;
        } else {
            (void)fprintf(stderr,
                          "Usage: %s [--seeds COUNT] [--years COUNT]\n",
                          argv[0]);
            return EXIT_FAILURE;
        }
    }

    (void)puts(
        "seed_number,world_seed,year,average_hunger,maximum_hunger,"
        "average_prosperity,minimum_prosperity,maximum_prosperity,"
        "average_security,minimum_security,maximum_security,"
        "average_food_price,maximum_food_price,closed_routes,"
        "active_situations,bandit_influence,monster_pressure,"
        "average_legitimacy,shipment_slots,event_count,goblin_tributes,"
        "dragon_hoard,dragon_stolen,dragon_retaliations,average_inequality,"
        "maximum_inequality,average_war_burden,maximum_war_burden,"
        "hoard_raids,social_hoard_raids,war_hoard_raids,tracked_gold,"
        "market_coins,war_chests,total_food_stock,average_war_supply_crisis,"
        "maximum_war_supply_crisis,total_kingdom_treasury,treasure_count,"
        "goblin_lair_food,goblin_lair_weapons,dragon_raw_gold,dragon_gems,"
        "tracked_iron,tracked_tools,tracked_weapons");
    char error[192];
    for (int32_t seed_number = 1; seed_number <= seeds; ++seed_number) {
        CcSim sim;
        CcSimInit(&sim, (uint32_t)seed_number * UINT32_C(0x9e3779b9));
        for (int32_t year = 1; year <= years; ++year) {
            CcSimAdvanceDays(&sim, 365);
            if (!CcSimValidate(&sim, error, sizeof(error))) {
                (void)fprintf(stderr,
                              "Seed %d failed in year %d: %s\n",
                              seed_number, year, error);
                return EXIT_FAILURE;
            }
            PrintYear(&sim, seed_number, year);
        }
    }
    return EXIT_SUCCESS;
}
