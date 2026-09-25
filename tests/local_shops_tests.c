#include "client/cc_local_shops.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

static void CheckTown(CcSettlementFunction function)
{
    const CcLocalPlaceProfile *profile = CcLocalPlaceProfileForFunction(function);
    assert(profile != NULL);
    assert(profile->building_count >= CC_LOCAL_SHOP_COUNT);
    int32_t used_buildings[CC_LOCAL_PLACE_BUILDING_CAPACITY] = {0};
    int32_t goods_seen[CC_GOOD_COUNT] = {0};
    for (int32_t kind = 0; kind < CC_LOCAL_SHOP_COUNT; ++kind) {
        const CcLocalShop *shop = CcLocalShopAt(profile, (CcLocalShopKind)kind);
        assert(shop != NULL);
        assert(shop->kind == (CcLocalShopKind)kind);
        assert(shop->name != NULL && shop->name[0] != '\0');
        assert(shop->keeper_name != NULL && shop->keeper_name[0] != '\0');
        assert(shop->service_name != NULL && shop->service_name[0] != '\0');
        assert(profile->building[shop->building_index].door);
        assert(++used_buildings[shop->building_index] == 1);
        assert(CcLocalShopForBuilding(profile, shop->building_index) == shop);
        CcLocalLanePoint door = CcLocalShopDoor(profile, shop);
        CcLocalLanePoint approach = CcLocalShopApproach(profile, shop);
        assert(isfinite(door.x) && isfinite(door.z));
        assert(isfinite(approach.x) && isfinite(approach.z));
        assert(fabsf(hypotf(approach.x - door.x, approach.z - door.z) - 1.43f) < 0.001f);
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
            if (CcLocalShopSells(shop, (CcGood)good)) {
                assert(++goods_seen[good] == 1);
                assert(CcLocalShopForGood(profile, (CcGood)good) == shop);
            }
        }
    }
    assert(CcLocalShopAt(profile, CC_LOCAL_SHOP_GRAIN_MERCHANT)->building_index ==
           profile->primary_building);
    for (int32_t good = 0; good < CC_GOOD_ROTTEN_MEAT; ++good) {
        assert(goods_seen[good] == 1);
    }
    assert(goods_seen[CC_GOOD_ROTTEN_MEAT] == 0);
    assert(goods_seen[CC_GOOD_ROTTEN_GRAIN] == 0);
    assert(CcLocalShopForGood(profile, CC_GOOD_ROTTEN_MEAT) == NULL);
    assert(CcLocalShopForGood(profile, CC_GOOD_ROTTEN_GRAIN) == NULL);
    assert(CcLocalShopForBuilding(profile, -1) == NULL);
    assert(CcLocalShopAt(profile, CC_LOCAL_SHOP_COUNT) == NULL);
    const CcLocalShop *bakery = CcLocalShopForGood(profile, CC_GOOD_BREAD);
    const CcLocalShop *stonecutter = CcLocalShopForGood(profile, CC_GOOD_STONE);
    assert(bakery != NULL && bakery->kind == CC_LOCAL_SHOP_BAKERY);
    assert(stonecutter != NULL && stonecutter->kind == CC_LOCAL_SHOP_STONECUTTER);
    assert(!CcLocalShopSells(bakery, CC_GOOD_STONE));
    assert(!CcLocalShopSells(stonecutter, CC_GOOD_BREAD));
}

int main(void)
{
    for (int32_t town = CC_SETTLEMENT_FARMING;
         town <= CC_SETTLEMENT_DUNGEON_TOWN; ++town) {
        CheckTown((CcSettlementFunction)town);
    }
    puts("Nine specialist shops and rotated doors in each town: passed");
    return 0;
}
