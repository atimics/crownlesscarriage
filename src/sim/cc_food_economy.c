#include "sim/cc_food_economy_internal.h"
#include "sim/cc_production_internal.h"

#include <limits.h>

static int32_t MinimumI32(int32_t a, int32_t b) { return a < b ? a : b; }
static int32_t MaximumI32(int32_t a, int32_t b) { return a > b ? a : b; }
static int32_t ClampI32(int32_t value, int32_t minimum, int32_t maximum)
{
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static bool IsWarSeat(const CcSettlement *place)
{
    return place != NULL &&
        (place->function == CC_SETTLEMENT_FORTRESS ||
         place->function == CC_SETTLEMENT_CAPITAL);
}

int32_t CcEconomyWarExtraConsumption(const CcSim *sim,
                                   const CcSettlement *place,
                                   CcGood good)
{
    if (sim == NULL || !IsWarSeat(place)) return 0;
    if (good != CC_GOOD_FOOD && good != CC_GOOD_TOOLS &&
        (good != CC_GOOD_WOOL || sim->schema_version < 32U)) return 0;
    int32_t burden = CcSimWarBurdenAtSettlement(sim, place->id);
    if (burden < 20) return 0;
    if (good == CC_GOOD_FOOD) return MaximumI32(1, burden / 25);
    if (good == CC_GOOD_TOOLS) return burden >= 50 ? 1 : 0;
    if (good == CC_GOOD_WOOL && sim->schema_version >= 32U) {
        return 1 + burden / 50;
    }
    return 0;
}

int32_t CcEconomyEffectiveReserveTarget(const CcSim *sim,
                                      const CcSettlement *place,
                                      CcGood good)
{
    if (place == NULL || good < 0 || good >= CC_GOOD_COUNT) return 0;
    return place->reserve_target[good] +
           CcEconomyWarExtraConsumption(sim, place, good) * 3;
}

int32_t CcEconomyCivilianFoodUse(const CcSettlement *place)
{
    if (place == NULL || place->consumption[CC_GOOD_FOOD] <= 0) return 0;
    if (place->population >= 600) {
        return place->consumption[CC_GOOD_FOOD];
    }
    int32_t population_use = MaximumI32(
        1, (place->population + 299) / 300);
    return MinimumI32(
        place->consumption[CC_GOOD_FOOD], population_use);
}

int32_t CcEconomyWeeklyFoodUse(const CcSim *sim,
                             const CcSettlement *place)
{
    if (place == NULL) return 1;
    return MaximumI32(
        1, CcEconomyCivilianFoodUse(place) +
           CcEconomyWarExtraConsumption(sim, place, CC_GOOD_FOOD));
}

/* Relief releases part of an existing reserve. The original target remains
   the ceiling, as in the bread policy; each food's floor uses its nutrition.
   Wheat also retains one production cycle of bakery and herd inputs. */
int32_t CcEconomyReliefReserve(const CcSim *sim,
    const CcSettlement *origin, const CcSettlement *destination, CcGood good)
{
    if (sim == NULL || origin == NULL || destination == NULL ||
        good < 0 || good >= CC_GOOD_COUNT) return 0;
    int32_t reserve = origin->reserve_target[good];
    if (CcSettlementIsAbandoned(origin) || CcSettlementIsAbandoned(destination) ||
        destination->hunger < 65 || origin->hunger >= 35) return reserve;
    if (sim->schema_version < 100U) {
        if (good != CC_GOOD_BREAD) return reserve;
        return MinimumI32(reserve, MaximumI32(
            CcEconomyWeeklyFoodUse(sim, origin) * 6, reserve / 2));
    }
    int32_t nutrition = CcGoodNutritionValue(good, CC_NUTRITION_CIVILIAN);
    if (nutrition <= 0) return reserve;
    int64_t floor = ((int64_t)CcEconomyWeeklyFoodUse(sim, origin) *
        6 * CC_NUTRITION_PER_RATION + nutrition - 1) / nutrition;
    if (good == CC_GOOD_WHEAT) {
        int64_t input = CcEconomyBakeryCapacity(origin);
        if (CcSettlementHasService(origin, CC_SERVICE_FARM)) {
            input += ((int64_t)origin->cow_adults + origin->cow_calves + 11) / 12;
            /* Keep the next flock meal through seasonal transitions too. */
            input += ((int64_t)origin->sheep_adults + origin->sheep_lambs + 23) / 24;
        }
        if (CcSettlementHasService(origin, CC_SERVICE_FARM) ||
            CcSettlementHasService(origin, CC_SERVICE_STABLE))
            input += ((int64_t)origin->pony_adults + origin->pony_foals + 7) / 8;
        /* Civilian food and the next work/feed cycle are separate claims. */
        floor += input;
    }
    int32_t bounded_floor = floor > INT32_MAX ? INT32_MAX : (int32_t)floor;
    return MinimumI32(reserve, MaximumI32(bounded_floor, reserve / 2));
}

static int32_t FoodStorageCapacity(const CcSim *sim,
                                   const CcSettlement *place)
{
    int32_t storage_weeks = CcSettlementHasService(
        place, CC_SERVICE_GRANARY) ? 32 : 12;
    return CcEconomyWeeklyFoodUse(sim, place) * storage_weeks;
}

int32_t CcEconomyNutritionRations(const int32_t goods[CC_GOOD_COUNT],
                                CcNutritionPurpose purpose)
{
    return CcNutritionAvailable(goods, purpose) / CC_NUTRITION_PER_RATION;
}

int32_t CcEconomyIncomingNutrition(const CcSim *sim, CcId settlement_id,
                                 CcNutritionPurpose purpose)
{
    int64_t nutrition = 0;
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        nutrition += (int64_t)CcSimIncomingGood(
            sim, settlement_id, (CcGood)good) *
            CcGoodNutritionValue((CcGood)good, purpose);
    }
    return nutrition > INT32_MAX ? INT32_MAX : (int32_t)nutrition;
}

void CcEconomyRefreshSettlementGoodPrice(const CcSim *sim,
                                       CcSettlement *settlement,
                                       CcGood good)
{
    if (sim == NULL || settlement == NULL ||
        good < 0 || good >= CC_GOOD_COUNT) return;
    int32_t target = CcEconomyEffectiveReserveTarget(sim, settlement, good);
    int32_t incoming = CcSimIncomingGood(sim, settlement->id, good);
    int32_t expected_stock = settlement->stock[good] + incoming / 2;
    int32_t shortage = target > 0 ?
        (target - expected_stock) * 100 / target : 0;
    int32_t pressure = ClampI32(shortage, -35, 220);
    const CcGoodDefinition *definition = CcGoodDefinitionFor(good);
    settlement->price[good] = MinimumI32(
        99, definition->base_price * (100 + pressure) / 100);
    if (settlement->price[good] < 1) settlement->price[good] = 1;
}

int32_t CcEconomyNutritionStorageCapacity(const CcSim *sim,
                                        const CcSettlement *place,
                                        CcGood good)
{
    int32_t weekly_use = CcEconomyWeeklyFoodUse(sim, place);
    bool granary = CcSettlementHasService(place, CC_SERVICE_GRANARY);
    if (good == CC_GOOD_BREAD) return FoodStorageCapacity(sim, place);
    if (good == CC_GOOD_WHEAT) {
        return weekly_use * (granary ? 64 : 24);
    }
    if (good == CC_GOOD_MEAT) return weekly_use * (granary ? 8 : 2);
    return CC_SIM_MAX_UNITS;
}

int32_t CcEconomySpoilStoredNutrition(const CcSim *sim, CcSettlement *place,
                                    CcTownNutritionAccounting *accounting)
{
    static const CcGood goods[] = {
        CC_GOOD_BREAD, CC_GOOD_WHEAT, CC_GOOD_MEAT
    };
    int32_t total_spoiled = 0;
    for (size_t i = 0; i < sizeof(goods) / sizeof(goods[0]); ++i) {
        CcGood good = goods[i];
        int32_t divisor = good == CC_GOOD_MEAT ? 20 :
                          good == CC_GOOD_WHEAT ? 400 : 100;
        int32_t stored = place->stock[good];
        int32_t spoiled = stored / divisor;
        if (accounting != NULL) accounting->aged_units[good] += (uint64_t)spoiled;
        stored -= spoiled;
        int32_t capacity = CcEconomyNutritionStorageCapacity(sim, place, good);
        if (stored > capacity) {
            if (accounting != NULL) {
                accounting->overflow_units[good] += (uint64_t)(stored - capacity);
            }
            spoiled += stored - capacity;
            stored = capacity;
        }
        place->stock[good] = stored;
        total_spoiled += spoiled;
    }
    return total_spoiled;
}

