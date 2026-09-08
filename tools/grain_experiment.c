#include "sim/cc_sim.h"
#include "sim/cc_production.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

static CcSim base, trial, pilot;
static bool Fund(CcSim *sim, char *error, size_t capacity)
{
    CcCommand command = {.kind = CC_COMMAND_FUND_GRAIN_SUPPLY, .target_id = sim->player.location_id};
    return CcSimApply(sim, &command, error, capacity);
}

int main(int argc, char **argv)
{
    if (argc != 3) return 2;
    int ordinal = atoi(argv[1]), years = atoi(argv[2]);
    if (ordinal < 1 || ordinal > 1000 || years < 0 || years > 1000) return 2;
    CcSimInit(&base, (uint32_t)ordinal * UINT32_C(2654435761));
    CcSimAdvanceDays(&base, years * 365);
    char error[256] = {0};
    if (!CcSimValidate(&base, error, sizeof(error))) { fprintf(stderr, "baseline: %s\n", error); return 1; }
    /* Controlled arrival: equal external cash and location in each branch. */
    base.player.location_id = base.settlements[1].id;
    base.carriage.location_id = base.player.location_id;
    base.player.coins = 400;
    pilot = base;
    if (!Fund(&pilot, error, sizeof(error))) { fprintf(stderr, "arrival: %s\n", error); return 1; }
    CcId cut_route = 0;
    int cut_day = 28;
    for (int day = 1; day <= 90; ++day) {
        CcSimAdvanceDays(&pilot, 1);
        if (pilot.grain_supplies[1].shipment_id != 0) {
            cut_route = pilot.grain_supplies[1].route_id;
            cut_day = day + 1;
            break;
        }
    }
    if (cut_route == 0) {
        for (int r = 0; r < base.route_count; ++r)
            if (base.routes[r].from_id == base.player.location_id || base.routes[r].to_id == base.player.location_id) {
                cut_route = base.routes[r].id; break;
            }
    }
    puts("seed,age,arm,day,cut_day,cut_route,hunger,prosperity,population,nutrition,bread_made,wheat_used,food_aged,food_overflow,ordered,delivered,lost,redirected,fund,spent,status,supplier,route");
    const char *arms[] = {"cash_open", "fund_open", "cash_cut", "fund_cut"};
    for (int arm = 0; arm < 4; ++arm) {
        trial = base;
        CcMoney gold = CcSimTrackedGold(&trial);
        if (arm % 2 == 1) {
            if (!Fund(&trial, error, sizeof(error))) return 1;
        } else {
            trial.player.coins -= 200;
            trial.settlements[1].market_coins += 200;
        }
        if (gold != CcSimTrackedGold(&trial)) return 1;
        CcNutritionAccounting food = {0};
        CcProductionAccounting production = {0};
        for (int day = 0; day <= 365; ++day) {
            if (!CcSimValidate(&trial, error, sizeof(error))) {
                fprintf(stderr, "%s day %d: %s\n", arms[arm], day, error); return 1;
            }
            CcGrainSupply *supply = &trial.grain_supplies[1];
            const CcSettlement *town = &trial.settlements[1];
            uint64_t nutrition = 0, aged = 0, overflow = 0;
            for (int good = 0; good < CC_GOOD_COUNT; ++good) {
                uint64_t value = (uint64_t)CcGoodNutritionValue((CcGood)good, CC_NUTRITION_CIVILIAN);
                nutrition += food.towns[1].civilian_units[good] * value;
                aged += food.towns[1].aged_units[good] * value;
                overflow += food.towns[1].overflow_units[good] * value;
            }
            printf("%d,%d,%s,%d,%d,%" PRIu64 ",%d,%d,%d,%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%d,%d,%d,%d,%" PRId64 ",%" PRId64 ",%d,%" PRIu64 ",%" PRIu64 "\n",
                ordinal, years, arms[arm], day, cut_day, cut_route, town->hunger, town->prosperity, town->population,
                nutrition, production.towns[1].bakery.output[CC_GOOD_BREAD], production.towns[1].bakery.input[CC_GOOD_WHEAT],
                aged, overflow, supply->ordered, supply->delivered, supply->lost, supply->redirected,
                supply->purse, supply->spent, CcSimGrainDeliveryPlan(&trial, town->id).status, supply->supplier_id, supply->route_id);
            if (day == 365) break;
            if (arm >= 2 && day + 1 >= cut_day && day + 1 < cut_day + 42) {
                for (int r = 0; r < trial.route_count; ++r) if (trial.routes[r].id == cut_route) {
                    trial.routes[r].condition = 0; trial.routes[r].closed = true;
                }
            } else if (arm >= 2 && day + 1 == cut_day + 42) {
                for (int r = 0; r < trial.route_count; ++r) if (trial.routes[r].id == cut_route) {
                    trial.routes[r].condition = 80; trial.routes[r].closed = false;
                }
            }
            CcSimAdvanceDaysWithProductionAccounting(&trial, 1, &food, NULL, &production);
        }
    }
    return 0;
}
