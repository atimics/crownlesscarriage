#include "client/cc_local_shops.h"

#include <math.h>
#include <stdio.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        (void)fprintf(stderr, "check failed at line %d: %s\n", \
                      __LINE__, #condition); \
        return 1; \
    } \
} while (0)

static int CheckTown(CcSettlementFunction function)
{
    const CcLocalPlaceProfile *profile = CcLocalPlaceProfileForFunction(function);
    CHECK(profile != NULL);
    CHECK(profile->building_count >= CC_LOCAL_SHOP_STATIONER + 1);
    int32_t used_buildings[CC_LOCAL_PLACE_BUILDING_CAPACITY] = {0};
    int32_t goods_seen[CC_GOOD_COUNT] = {0};
    int32_t shop_count = 0;
    for (int32_t kind = 0; kind < CC_LOCAL_SHOP_COUNT; ++kind) {
        const CcLocalShop *shop = CcLocalShopAt(profile, (CcLocalShopKind)kind);
        if (kind == CC_LOCAL_SHOP_MINE_SUPPLIER &&
            function != CC_SETTLEMENT_MINING) {
            CHECK(shop == NULL);
            continue;
        }
        CHECK(shop != NULL);
        ++shop_count;
        CHECK(shop->kind == (CcLocalShopKind)kind);
        CHECK(shop->name != NULL && shop->name[0] != '\0');
        CHECK(shop->keeper_name != NULL && shop->keeper_name[0] != '\0');
        CHECK(shop->service_name != NULL && shop->service_name[0] != '\0');
        CHECK(profile->building[shop->building_index].door);
        CHECK(++used_buildings[shop->building_index] == 1);
        CHECK(CcLocalShopForBuilding(profile, shop->building_index) == shop);
        CcLocalLanePoint door = CcLocalShopDoor(profile, shop);
        CcLocalLanePoint approach = CcLocalShopApproach(profile, shop);
        CHECK(isfinite(door.x) && isfinite(door.z));
        CHECK(isfinite(approach.x) && isfinite(approach.z));
        CHECK(fabsf(hypotf(approach.x - door.x, approach.z - door.z) - 1.43f) < 0.001f);
        for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
            if (CcLocalShopSells(shop, (CcGood)good)) {
                CHECK(++goods_seen[good] == 1);
                CHECK(CcLocalShopForGood(profile, (CcGood)good) == shop);
                CHECK(CcLocalShopBuys(shop, (CcGood)good));
            }
        }
    }
    CHECK(shop_count == (function == CC_SETTLEMENT_MINING ? 10 : 9));
    CHECK(CcLocalShopAt(profile, CC_LOCAL_SHOP_GRAIN_MERCHANT)->building_index ==
           profile->primary_building);
    for (int32_t good = 0; good < CC_GOOD_ROTTEN_MEAT; ++good) {
        CHECK(goods_seen[good] == 1);
    }
    CHECK(goods_seen[CC_GOOD_ROTTEN_MEAT] == 0);
    CHECK(goods_seen[CC_GOOD_ROTTEN_GRAIN] == 0);
    CHECK(goods_seen[CC_GOOD_RAW_STONE] ==
          (function == CC_SETTLEMENT_MINING ? 1 : 0));
    CHECK(CcLocalShopForGood(profile, CC_GOOD_ROTTEN_MEAT) == NULL);
    CHECK(CcLocalShopForGood(profile, CC_GOOD_ROTTEN_GRAIN) == NULL);
    CHECK((CcLocalShopForGood(profile, CC_GOOD_RAW_STONE) != NULL) ==
          (function == CC_SETTLEMENT_MINING));
    CHECK(CcLocalShopForBuilding(profile, -1) == NULL);
    CHECK(CcLocalShopAt(profile, CC_LOCAL_SHOP_COUNT) == NULL);
    const CcLocalShop *bakery = CcLocalShopForGood(profile, CC_GOOD_BREAD);
    const CcLocalShop *stonecutter = CcLocalShopForGood(profile, CC_GOOD_STONE);
    CHECK(bakery != NULL && bakery->kind == CC_LOCAL_SHOP_BAKERY);
    CHECK(stonecutter != NULL && stonecutter->kind == CC_LOCAL_SHOP_STONECUTTER);
    CHECK(!CcLocalShopSells(bakery, CC_GOOD_STONE));
    CHECK(!CcLocalShopSells(stonecutter, CC_GOOD_BREAD));
    CHECK(CcLocalShopBuys(bakery, CC_GOOD_WHEAT));
    CHECK(!CcLocalShopSells(bakery, CC_GOOD_WHEAT));
    CHECK(CcLocalShopBuys(stonecutter, CC_GOOD_RAW_STONE));
    CHECK(!CcLocalShopSells(stonecutter, CC_GOOD_RAW_STONE));
    CHECK(CcLocalShopBuys(CcLocalShopAt(profile, CC_LOCAL_SHOP_BUTCHER),
                          CC_GOOD_ROTTEN_MEAT));
    CHECK(CcLocalShopBuys(CcLocalShopAt(profile, CC_LOCAL_SHOP_GRAIN_MERCHANT),
                          CC_GOOD_ROTTEN_GRAIN));
    CHECK(!CcLocalShopBuys(bakery, CC_GOOD_RAW_STONE));
    CHECK(!CcLocalShopBuys(NULL, CC_GOOD_WHEAT));
    return 0;
}

int main(void)
{
    for (int32_t town = CC_SETTLEMENT_FARMING;
         town <= CC_SETTLEMENT_DUNGEON_TOWN; ++town) {
        if (CheckTown((CcSettlementFunction)town) != 0) return 1;
    }
    puts("Nine town shops, Silverwick mine supply, and directional goods: passed");
    return 0;
}
