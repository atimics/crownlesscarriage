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

typedef struct CcMetricsHistory {
    int32_t minimum_active_settlements;
    int32_t maximum_closed_routes;
    int32_t years_all_routes_closed;
    int32_t years_with_abandoned_settlement;
    int32_t route_closures;
    int32_t settlement_abandonments;
    int32_t years_hunger_40_plus;
    int32_t years_hunger_60_plus;
    int32_t years_at_war;
    int32_t years_allied;
    int32_t years_dragon_campaign;
    int32_t years_goblin_raid;
    int32_t years_bandit_raid;
    int32_t years_bandit_influence_70_plus;
    int32_t days_at_war;
    int32_t days_allied;
    int32_t days_dragon_campaign;
    int32_t days_goblin_raid;
    int32_t days_bandit_raid;
    int32_t days_bandit_influence_70_plus;
    int32_t dragon_stage_days[7];
    bool route_was_closed[CC_MAX_ROUTES];
    bool settlement_was_abandoned[CC_MAX_SETTLEMENTS];
} CcMetricsHistory;

static void UpdateDailyHistory(const CcSim *sim, CcMetricsHistory *history)
{
    bool at_war = false;
    bool allied = false;
    for (int32_t first = 0; first < sim->kingdom_count; ++first) {
        for (int32_t second = first + 1;
             second < sim->kingdom_count; ++second) {
            at_war |= sim->diplomacy[first][second] == CC_DIPLOMACY_WAR;
            allied |= sim->diplomacy[first][second] == CC_DIPLOMACY_ALLIANCE;
        }
    }
    if (at_war) history->days_at_war += 1;
    if (allied) history->days_allied += 1;
    if (sim->dragon_campaign.phase != CC_DRAGON_CAMPAIGN_IDLE) {
        history->days_dragon_campaign += 1;
    }
    if (sim->goblins.raid_motive != CC_GOBLIN_RAID_NONE) {
        history->days_goblin_raid += 1;
    }
    for (int32_t i = 0; i < sim->bandit_count; ++i) {
        if (sim->bandits[i].raid_phase != CC_BANDIT_RAID_IDLE) {
            history->days_bandit_raid += 1;
        }
        if (sim->bandits[i].influence >= 70) {
            history->days_bandit_influence_70_plus += 1;
        }
    }
    if (sim->dragon.life_stage >= CC_DRAGON_STAGE_EGG &&
        sim->dragon.life_stage <= CC_DRAGON_STAGE_AFTERDRAGON) {
        history->dragon_stage_days[sim->dragon.life_stage] += 1;
    }
}

static void UpdateHistory(const CcSim *sim, CcMetricsHistory *history)
{
    int32_t active_settlements = 0;
    int32_t closed_routes = 0;
    int32_t hunger_total = 0;
    bool has_abandoned_settlement = false;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        bool abandoned = CcSettlementIsAbandoned(&sim->settlements[i]);
        hunger_total += sim->settlements[i].hunger;
        if (!abandoned) active_settlements += 1;
        else has_abandoned_settlement = true;
        if (abandoned && !history->settlement_was_abandoned[i]) {
            history->settlement_abandonments += 1;
        }
        history->settlement_was_abandoned[i] = abandoned;
    }
    for (int32_t i = 0; i < sim->route_count; ++i) {
        bool closed = sim->routes[i].closed;
        if (closed) closed_routes += 1;
        if (closed && !history->route_was_closed[i]) {
            history->route_closures += 1;
        }
        history->route_was_closed[i] = closed;
    }
    if (active_settlements < history->minimum_active_settlements) {
        history->minimum_active_settlements = active_settlements;
    }
    if (closed_routes > history->maximum_closed_routes) {
        history->maximum_closed_routes = closed_routes;
    }
    if (closed_routes == sim->route_count) {
        history->years_all_routes_closed += 1;
    }
    if (has_abandoned_settlement) {
        history->years_with_abandoned_settlement += 1;
    }
    if (hunger_total / sim->settlement_count >= 40) {
        history->years_hunger_40_plus += 1;
    }
    if (hunger_total / sim->settlement_count >= 60) {
        history->years_hunger_60_plus += 1;
    }
    bool at_war = false;
    bool allied = false;
    for (int32_t first = 0; first < sim->kingdom_count; ++first) {
        for (int32_t second = first + 1;
             second < sim->kingdom_count; ++second) {
            at_war |= sim->diplomacy[first][second] == CC_DIPLOMACY_WAR;
            allied |= sim->diplomacy[first][second] == CC_DIPLOMACY_ALLIANCE;
        }
    }
    if (at_war) history->years_at_war += 1;
    if (allied) history->years_allied += 1;
    if (sim->dragon_campaign.phase != CC_DRAGON_CAMPAIGN_IDLE) {
        history->years_dragon_campaign += 1;
    }
    if (sim->goblins.raid_motive != CC_GOBLIN_RAID_NONE) {
        history->years_goblin_raid += 1;
    }
    for (int32_t i = 0; i < sim->bandit_count; ++i) {
        if (sim->bandits[i].raid_phase != CC_BANDIT_RAID_IDLE) {
            history->years_bandit_raid += 1;
        }
        if (sim->bandits[i].influence >= 70) {
            history->years_bandit_influence_70_plus += 1;
        }
    }
}

static void PrintYear(const CcSim *sim, const CcMetricsHistory *history,
                      int32_t seed_number, int32_t year)
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
    int32_t active_settlements = 0;
    int32_t abandoned_settlements = 0;
    int32_t total_population = 0;
    int32_t war_crisis_total = 0;
    int32_t war_crisis_maximum = 0;
    for (int32_t i = 0; i < sim->settlement_count; ++i) {
        const CcSettlement *place = &sim->settlements[i];
        if (CcSettlementIsAbandoned(place)) abandoned_settlements += 1;
        else active_settlements += 1;
        total_population += place->population;
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
    CcMoney debt_total = 0;
    int32_t smuggler_routes = 0;
    int32_t wars = 0;
    int32_t alliances = 0;
    int32_t active_couriers = 0;
    int32_t lost_couriers = 0;
    int32_t distorted_couriers = 0;
    for (int32_t i = 0; i < sim->kingdom_count; ++i) {
        legitimacy_total += sim->kingdoms[i].legitimacy;
        treasury_total += sim->kingdoms[i].treasury;
        debt_total += sim->kingdoms[i].iron_ledger_debt;
    }
    for (int32_t i = 0; i < sim->route_count; ++i) {
        if (sim->routes[i].smuggler_route) smuggler_routes += 1;
    }
    for (int32_t first = 0; first < sim->kingdom_count; ++first) {
        for (int32_t second = first + 1;
             second < sim->kingdom_count; ++second) {
            if (sim->diplomacy[first][second] == CC_DIPLOMACY_WAR) {
                wars += 1;
            } else if (sim->diplomacy[first][second] ==
                       CC_DIPLOMACY_ALLIANCE) {
                alliances += 1;
            }
        }
    }
    for (int32_t i = 0; i < sim->courier_count; ++i) {
        CcCourierStatus status = sim->couriers[i].status;
        if (status == CC_COURIER_WAITING ||
            status == CC_COURIER_TRAVELLING ||
            status == CC_COURIER_WITH_PLAYER) active_couriers += 1;
        else if (status == CC_COURIER_LOST) lost_couriers += 1;
        else if (status == CC_COURIER_DISTORTED) distorted_couriers += 1;
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
        ",%" PRId64 ",%d,%d,%d,%" PRId64 ",%d,%d,%d,%d,%d,%d,%d,%d,"
        "%" PRId64 ",%" PRId64 ",%d,%d",
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
        CcSimTrackedGood(sim, CC_GOOD_WEAPONS),
        sim->iron_ledger_reserve, debt_total, smuggler_routes,
        sim->goblins.hoard_defenses);
    (void)printf(
        ",%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
        "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
        "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
        "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
        wars, alliances, active_couriers, lost_couriers,
        distorted_couriers, sim->dragon.slain ? 1 : 0,
        sim->dragon_campaign.attempts,
        sim->dragon_campaign.victories,
        sim->dragon_campaign.defeats,
        (int32_t)sim->dragon.life_stage,
        sim->dragon.age_days / 365,
        sim->dragon.crown_strength,
        sim->dragon.body_condition,
        sim->dragon.memory_integrity,
        sim->dragon.territory_stability,
        sim->dragon.regional_influence,
        sim->dragon.egg_count,
        sim->dragon.hunts,
        sim->dragon.broods_laid,
        sim->dragon.whelps_dispersed,
        sim->dragon.afterdeath_days,
        active_settlements, abandoned_settlements, total_population,
        CcSimClimateFactor(sim), CcDragonCampaignExperience(sim),
        history->minimum_active_settlements,
        history->maximum_closed_routes,
        history->years_all_routes_closed,
        history->years_with_abandoned_settlement,
        history->route_closures,
        history->settlement_abandonments,
        history->years_hunger_40_plus,
        history->years_hunger_60_plus,
        history->years_at_war,
        history->years_allied,
        history->years_dragon_campaign,
        history->years_goblin_raid,
        history->years_bandit_raid,
        history->years_bandit_influence_70_plus,
        sim->goblins.members,
        sim->goblins.devotion,
        sim->goblins.cohesion,
        sim->goblins.expeditions_intercepted,
        sim->bandit_count > 0 ? sim->bandits[0].members : 0,
        sim->bandit_count > 0 ? sim->bandits[0].supplies : 0,
        sim->bandit_count > 0 ? sim->bandits[0].influence : 0,
        sim->bandit_count > 0 ? sim->bandits[0].raids_completed : 0,
        (int32_t)sim->dragon_campaign.phase,
        history->days_at_war,
        history->days_allied,
        history->days_dragon_campaign,
        history->days_goblin_raid,
        history->days_bandit_raid,
        history->days_bandit_influence_70_plus,
        history->dragon_stage_days[CC_DRAGON_STAGE_EGG],
        history->dragon_stage_days[CC_DRAGON_STAGE_WHELP],
        history->dragon_stage_days[CC_DRAGON_STAGE_WANDERER],
        history->dragon_stage_days[CC_DRAGON_STAGE_CROWNED],
        history->dragon_stage_days[CC_DRAGON_STAGE_DEEP_WYRM],
        history->dragon_stage_days[CC_DRAGON_STAGE_UNCROWNED],
        history->dragon_stage_days[CC_DRAGON_STAGE_AFTERDRAGON]);
}

int main(int argc, char **argv)
{
    int32_t seeds = 100;
    int32_t years = 10;
    int32_t first_seed = 1;
    bool final_only = false;
    for (int32_t argument = 1; argument < argc; ++argument) {
        if (strcmp(argv[argument], "--seed") == 0 && argument + 1 < argc) {
            if (!ParsePositive(argv[++argument], &first_seed)) return EXIT_FAILURE;
            seeds = 1;
        } else if (strcmp(argv[argument], "--seeds") == 0 && argument + 1 < argc) {
            if (!ParsePositive(argv[++argument], &seeds)) return EXIT_FAILURE;
        } else if (strcmp(argv[argument], "--years") == 0 &&
                   argument + 1 < argc) {
            if (!ParsePositive(argv[++argument], &years)) return EXIT_FAILURE;
        } else if (strcmp(argv[argument], "--final-only") == 0) {
            final_only = true;
        } else {
            (void)fprintf(stderr,
                          "Usage: %s [--seed NUMBER | --seeds COUNT]"
                          " [--years COUNT] [--final-only]\n",
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
        "tracked_iron,tracked_tools,tracked_weapons,iron_ledger_reserve,"
        "iron_ledger_debt,smuggler_routes,goblin_hoard_defenses,wars,"
        "alliances,active_couriers,lost_couriers,distorted_couriers,"
        "dragon_slain,dragon_campaign_attempts,dragon_campaign_victories,"
        "dragon_campaign_defeats,dragon_stage,dragon_age_years,"
        "dragon_crown_strength,dragon_body_condition,dragon_memory_integrity,"
        "dragon_territory_stability,dragon_regional_influence,dragon_eggs,"
        "dragon_hunts,dragon_broods,dragon_whelps_dispersed,"
        "dragon_afterdeath_days,active_settlements,abandoned_settlements,"
        "total_population,climate_factor,dragon_campaign_experience,"
        "minimum_active_settlements,maximum_closed_routes,"
        "years_all_routes_closed,years_with_abandoned_settlement,"
        "route_closures,settlement_abandonments,years_hunger_40_plus,"
        "years_hunger_60_plus,years_at_war,years_allied,"
        "years_dragon_campaign,years_goblin_raid,years_bandit_raid,"
        "years_bandit_influence_70_plus,goblin_members_end,"
        "goblin_devotion_end,goblin_cohesion_end,goblin_interceptions_end,"
        "bandit_members_end,bandit_supplies_end,bandit_influence_end,"
        "bandit_raids_end,dragon_campaign_phase_end,days_at_war,"
        "days_allied,days_dragon_campaign,days_goblin_raid,days_bandit_raid,"
        "days_bandit_influence_70_plus,dragon_egg_days,dragon_whelp_days,"
        "dragon_wanderer_days,dragon_crowned_days,dragon_deep_wyrm_days,"
        "dragon_uncrowned_days,dragon_afterdragon_days");
    char error[192];
    for (int32_t seed_number = first_seed;
         seed_number < first_seed + seeds; ++seed_number) {
        CcSim sim;
        CcMetricsHistory history = {0};
        CcSimInit(&sim, (uint32_t)seed_number * UINT32_C(0x9e3779b9));
        history.minimum_active_settlements = sim.settlement_count;
        for (int32_t i = 0; i < sim.settlement_count; ++i) {
            history.settlement_was_abandoned[i] =
                CcSettlementIsAbandoned(&sim.settlements[i]);
        }
        for (int32_t i = 0; i < sim.route_count; ++i) {
            history.route_was_closed[i] = sim.routes[i].closed;
        }
        for (int32_t year = 1; year <= years; ++year) {
            for (int32_t day = 0; day < 365; ++day) {
                CcSimAdvanceDays(&sim, 1);
                UpdateDailyHistory(&sim, &history);
            }
            UpdateHistory(&sim, &history);
            if (!CcSimValidate(&sim, error, sizeof(error))) {
                (void)fprintf(stderr,
                              "Seed %d failed in year %d: %s\n",
                              seed_number, year, error);
                return EXIT_FAILURE;
            }
            if (!final_only || year == years) {
                PrintYear(&sim, &history, seed_number, year);
            }
        }
    }
    return EXIT_SUCCESS;
}
