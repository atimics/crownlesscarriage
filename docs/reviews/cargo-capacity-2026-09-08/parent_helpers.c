/* Parent a89feaf private helpers, renamed for comparison only. */
#include "sim/cc_goods_internal.h"

int32_t CcGoodsPlayerCargoBoxes(CcGood good, int32_t quantity)
{
    const CcGoodDefinition *definition = CcGoodDefinitionFor(good);
    if (definition == NULL || quantity <= 0) return 0;
    return (quantity + definition->player_units_per_slot - 1) /
           definition->player_units_per_slot;
}

int32_t CcGoodsFreightCargoSlots(CcGood good, int32_t quantity)
{
    const CcGoodDefinition *definition = CcGoodDefinitionFor(good);
    if (definition == NULL || quantity <= 0) return 0;
    return (quantity + definition->freight_units_per_slot - 1) /
           definition->freight_units_per_slot;
}

int32_t CcGoodsFreightUnitsPerCargoSlot(CcGood good)
{
    const CcGoodDefinition *definition = CcGoodDefinitionFor(good);
    return definition != NULL ? definition->freight_units_per_slot : 1;
}

