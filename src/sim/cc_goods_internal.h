#ifndef CROWNLESS_GOODS_INTERNAL_H
#define CROWNLESS_GOODS_INTERNAL_H

#include "sim/cc_sim.h"

/* Nutrition-based selection shared by the material economy's consumers. */
CcGood CcGoodsPreferredNutritionGood(
    const int32_t goods[CC_GOOD_COUNT], CcNutritionPurpose purpose);

#endif
