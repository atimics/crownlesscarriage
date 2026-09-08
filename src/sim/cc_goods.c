#include "sim/cc_goods_internal.h"

#include <limits.h>

static int32_t MinimumI32(int32_t a, int32_t b) { return a < b ? a : b; }
static int32_t MaximumI32(int32_t a, int32_t b) { return a > b ? a : b; }

static const CcGoodDefinition GOOD_DEFINITIONS[CC_GOOD_COUNT] = {
    [CC_GOOD_BREAD] = {"Bread", 4, 8, 1, 4, 16},
    [CC_GOOD_IRON] = {"Iron", 8, 4, 1, 4, 8},
    [CC_GOOD_TOOLS] = {"Tools", 14, 2, 1, 1, 4},
    [CC_GOOD_WEAPONS] = {"Weapons", 24, 2, 1, 1, 4},
    [CC_GOOD_GOLD] = {"Raw Gold", 40, 1, 1, 1, 2},
    [CC_GOOD_GEMS] = {"Gems", 70, 1, 1, 1, 1},
    [CC_GOOD_WOOD] = {"Wood", 6, 6, 1, 4, 12},
    [CC_GOOD_WHEAT] = {"Wheat", 3, 10, 1, 4, 20},
    [CC_GOOD_MEAT] = {"Meat", 7, 6, 1, 4, 12},
    [CC_GOOD_WOOL] = {"Wool", 9, 8, 1, 4, 16},
    [CC_GOOD_STONE] = {"Stone", 7, 4, 1, 4, 8},
    [CC_GOOD_PAPER] = {"Paper", 12, 8, 1, 4, 16},
    [CC_GOOD_ROTTEN_MEAT] = {"Rotten Meat", 1, 6, 1, 4, 12},
    [CC_GOOD_ROTTEN_GRAIN] = {"Rotten Grain", 1, 10, 1, 4, 20}
};

int32_t CcGoodCountForSchema(uint32_t schema_version)
{
    if (schema_version >= 33U) return CC_GOOD_COUNT;
    if (schema_version >= 27U) return 11;
    if (schema_version >= 9U) return CC_LEGACY_GOOD_COUNT;
    return 3;
}

bool CcGoodIsValid(CcGood good)
{
    return good >= CC_GOOD_BREAD && good < CC_GOOD_COUNT;
}

const CcGoodDefinition *CcGoodDefinitionFor(CcGood good)
{
    return CcGoodIsValid(good) ? &GOOD_DEFINITIONS[good] : NULL;
}

const char *CcGoodName(CcGood good)
{
    const CcGoodDefinition *definition = CcGoodDefinitionFor(good);
    return definition != NULL ? definition->name : "Unknown";
}

int32_t CcGoodNutritionValue(CcGood good, CcNutritionPurpose purpose)
{
    if (purpose == CC_NUTRITION_ANIMAL) {
        return good == CC_GOOD_WHEAT ? CC_NUTRITION_PER_RATION : 0;
    }
    if (good == CC_GOOD_BREAD || good == CC_GOOD_MEAT) {
        return CC_NUTRITION_PER_RATION;
    }
    if (purpose == CC_NUTRITION_CIVILIAN && good == CC_GOOD_WHEAT) {
        return 1;
    }
    return 0;
}

int32_t CcNutritionAvailable(const int32_t goods[CC_GOOD_COUNT],
                             CcNutritionPurpose purpose)
{
    if (goods == NULL) return 0;
    int64_t nutrition = 0;
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        int32_t quantity = MaximumI32(0, goods[good]);
        nutrition += (int64_t)quantity *
                     CcGoodNutritionValue((CcGood)good, purpose);
    }
    return nutrition > INT32_MAX ? INT32_MAX : (int32_t)nutrition;
}

int32_t CcNutritionConsume(int32_t goods[CC_GOOD_COUNT],
                           CcNutritionPurpose purpose,
                           int32_t requested_nutrition)
{
    if (goods == NULL || requested_nutrition <= 0) return 0;
    static const CcGood order[] = {
        CC_GOOD_BREAD, CC_GOOD_MEAT, CC_GOOD_WHEAT
    };
    int64_t delivered = 0;
    for (size_t i = 0;
         i < sizeof(order) / sizeof(order[0]) &&
         delivered < requested_nutrition; ++i) {
        CcGood good = order[i];
        int32_t value = CcGoodNutritionValue(good, purpose);
        if (value <= 0 || goods[good] <= 0) continue;
        int64_t remaining = requested_nutrition - delivered;
        int32_t needed_units = (int32_t)((remaining + value - 1) / value);
        int32_t used = MinimumI32(goods[good], needed_units);
        goods[good] -= used;
        delivered += (int64_t)used * value;
    }
    return delivered < requested_nutrition ?
        (int32_t)delivered : requested_nutrition;
}

CcGood CcGoodsPreferredNutritionGood(
    const int32_t goods[CC_GOOD_COUNT], CcNutritionPurpose purpose)
{
    static const CcGood order[] = {
        CC_GOOD_BREAD, CC_GOOD_MEAT, CC_GOOD_WHEAT
    };
    CcGood best = CC_GOOD_BREAD;
    int64_t best_nutrition = -1;
    for (size_t i = 0; i < sizeof(order) / sizeof(order[0]); ++i) {
        CcGood good = order[i];
        int64_t nutrition = (int64_t)goods[good] *
            CcGoodNutritionValue(good, purpose);
        if (nutrition > best_nutrition) {
            best = good;
            best_nutrition = nutrition;
        }
    }
    return best;
}

