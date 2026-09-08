#include "sim/cc_sim.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

static CcSim base, trial;

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    char *end = NULL;
    unsigned long ordinal = strtoul(argv[1], &end, 10);
    if (*end != '\0' || ordinal == 0 || ordinal > 1000) return 2;
    uint32_t seed = (uint32_t)ordinal * UINT32_C(2654435761);
    CcSimInit(&base, seed);
    CcSimAdvanceDays(&base, 365000);
    char error[256] = {0};
    if (!CcSimValidate(&base, error, sizeof(error))) {
        fprintf(stderr, "seed %lu baseline: %s\n", ordinal, error);
        return 1;
    }
    /* Controlled arrival fixture. Each arm receives twelve units of its chosen cargo.
       The experiment measures the town response after arrival, not procurement. */
    base.player.location_id = base.settlements[1].id;
    base.carriage.location_id = base.player.location_id;
    CcBakerySupportPlan plan = CcSimBakerySupportPlan(&base, base.player.location_id);
    for (int good = 0; good < CC_GOOD_COUNT; ++good) base.player.cargo[good] = plan.cargo[good];

    base.player.coins = 100;
    plan = CcSimBakerySupportPlan(&base, base.player.location_id);
    if (!plan.ready || !CcSimValidate(&base, error, sizeof(error))) {
        fprintf(stderr, "seed %lu arrival: %s; %s\n", ordinal, plan.reason, error);
        return 1;
    }
    puts("seed,arm,day,hunger,prosperity,population,wheat,bread,nutrition,bakery,remembered,initial_rebuild");
    const char *arms[] = {"control", "bread", "bakery"};
    for (int arm = 0; arm < 3; ++arm) {
        trial = base;
        if (arm == 1) {
            trial.player.cargo[CC_GOOD_WHEAT] = 0;
            trial.player.cargo[CC_GOOD_BREAD] = 12;
        }
        CcMoney gold = CcSimTrackedGold(&trial);
        if (arm == 1) {
            /* Diagnostic gift: ordinary town food stock plus the same cash grant.
               This comparator is a test treatment, not a new player command. */
            trial.player.cargo[CC_GOOD_BREAD] -= 12;
            trial.settlements[1].stock[CC_GOOD_BREAD] += 12;
            trial.player.coins -= plan.coins;
            trial.settlements[1].market_coins += plan.coins;
        } else if (arm == 2) {
            CcCommand command = {.kind = CC_COMMAND_SUPPORT_BAKERY,
                .target_id = trial.player.location_id, .amount = plan.building_days};
            if (!CcSimApply(&trial, &command, error, sizeof(error))) {
                fprintf(stderr, "seed %lu command: %s\n", ordinal, error);
                return 1;
            }
        }
        if (gold != CcSimTrackedGold(&trial)) return 1;
        CcNutritionAccounting food = {0};
        for (int day = 0; day <= 365; ++day) {
            if (!CcSimValidate(&trial, error, sizeof(error))) {
                fprintf(stderr, "seed %lu %s day %d: %s\n", ordinal, arms[arm], day, error);
                return 1;
            }
            const CcSettlement *town = &trial.settlements[1];
            uint64_t nutrition = 0;
            for (int good = 0; good < CC_GOOD_COUNT; ++good)
                nutrition += food.towns[1].civilian_units[good] * (uint64_t)CcGoodNutritionValue((CcGood)good, CC_NUTRITION_CIVILIAN);
            const CcCharacter *person = CcSimCharacter(&trial, plan.contact_id);
            printf("%lu,%s,%d,%d,%d,%d,%d,%d,%" PRIu64 ",%d,%d,%d\n",
                ordinal, arms[arm], day, town->hunger, town->prosperity, town->population,
                town->stock[CC_GOOD_WHEAT], town->stock[CC_GOOD_BREAD], nutrition,
                CcSettlementHasService(town, CC_SERVICE_BAKERY),
                CcCharacterRemembers(person, CC_CHARACTER_MEMORY_PLAYER_HELPED, town->id),
                plan.building_days > 0);
            if (day < 365) CcSimAdvanceDaysWithNutritionAccounting(&trial, 1, &food);
        }
    }
    return 0;
}
