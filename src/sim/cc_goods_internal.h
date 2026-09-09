#ifndef CROWNLESS_GOODS_INTERNAL_H
#define CROWNLESS_GOODS_INTERNAL_H

#include "sim/cc_sim.h"

/* Nutrition-based selection shared by the material economy's consumers. */
CcGood CcGoodsPreferredNutritionGood(
    const int32_t goods[CC_GOOD_COUNT], CcNutritionPurpose purpose);

int32_t CcGoodsRotNutrition(const int32_t goods[CC_GOOD_COUNT]);
int32_t CcGoodsConsumeRot(int32_t goods[CC_GOOD_COUNT], int32_t requested_nutrition);

int32_t CcGoodsPlayerCargoBoxes(CcGood good, int32_t quantity);
int32_t CcGoodsFreightCargoSlots(CcGood good, int32_t quantity);
int32_t CcGoodsFreightUnitsPerCargoSlot(CcGood good);

#endif
