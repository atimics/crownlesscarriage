#include "client/cc_local_shops.h"

#include <math.h>
#include <stddef.h>

/* Index order follows CcLocalShopKind. Index 2 is the existing service hall. */
static const CcLocalShop SHOPS[6][CC_LOCAL_SHOP_COUNT] = {
#define SHOPS_FOR_TOWN(a,b,c,d,e,f,g,h,i,mine, grain_keeper) { \
    {CC_LOCAL_SHOP_BAKERY,a,"Bakery","Baker","Fresh bread"}, \
    {CC_LOCAL_SHOP_BUTCHER,b,"Butcher","Butcher","Fresh meat"}, \
    {CC_LOCAL_SHOP_GRAIN_MERCHANT,c,"Grain merchant",grain_keeper,"Wheat and town promises"}, \
    {CC_LOCAL_SHOP_SMITH,d,"Smith","Smith","Iron, tools and weapons"}, \
    {CC_LOCAL_SHOP_JEWELER,e,"Jeweler","Jeweler","Gold and gems"}, \
    {CC_LOCAL_SHOP_TIMBER_MERCHANT,f,"Timber merchant","Timber merchant","Wood"}, \
    {CC_LOCAL_SHOP_CLOTHIER,g,"Clothier","Clothier","Wool"}, \
    {CC_LOCAL_SHOP_STONECUTTER,h,"Stonecutter","Stonecutter","Cut stone"}, \
    {CC_LOCAL_SHOP_STATIONER,i,"Stationer","Stationer","Paper"}, \
    {CC_LOCAL_SHOP_MINE_SUPPLIER,mine,"Mine supply office","Quarry clerk","Raw stone"} \
}
    SHOPS_FOR_TOWN(6,4,2,5,7,0,1,3,8,-1,"Edda — Granary keeper"),
    SHOPS_FOR_TOWN(6,7,2,5,4,10,0,3,1,8,"Oren — Company clerk"),
    SHOPS_FOR_TOWN(6,0,2,5,4,3,8,9,1,-1,"Mara — Merchant"),
    SHOPS_FOR_TOWN(7,6,2,1,4,8,0,3,5,-1,"Seren — Quartermaster"),
    SHOPS_FOR_TOWN(6,7,2,9,1,5,10,3,8,-1,"Ilyra — Royal factor"),
    SHOPS_FOR_TOWN(7,0,2,1,4,5,3,6,8,-1,"Vey — Expedition broker")
#undef SHOPS_FOR_TOWN
};

static int32_t TownIndex(const CcLocalPlaceProfile *profile)
{
    if (profile == NULL || profile->function < CC_SETTLEMENT_FARMING ||
        profile->function > CC_SETTLEMENT_DUNGEON_TOWN) return -1;
    return (int32_t)profile->function;
}

const CcLocalShop *CcLocalShopAt(const CcLocalPlaceProfile *profile,
                                CcLocalShopKind kind)
{
    int32_t town = TownIndex(profile);
    if (town < 0 || kind < 0 || kind >= CC_LOCAL_SHOP_COUNT) return NULL;
    const CcLocalShop *shop = &SHOPS[town][kind];
    if (shop->building_index < 0 || shop->building_index >= profile->building_count ||
        !profile->building[shop->building_index].door) return NULL;
    return shop;
}

const CcLocalShop *CcLocalShopForBuilding(const CcLocalPlaceProfile *profile,
                                         int32_t building_index)
{
    if (building_index < 0) return NULL;
    for (int32_t kind = 0; kind < CC_LOCAL_SHOP_COUNT; ++kind) {
        const CcLocalShop *shop = CcLocalShopAt(profile, (CcLocalShopKind)kind);
        if (shop != NULL && shop->building_index == building_index) return shop;
    }
    return NULL;
}

bool CcLocalShopSells(const CcLocalShop *shop, CcGood good)
{
    if (shop == NULL) return false;
    switch (shop->kind) {
        case CC_LOCAL_SHOP_BAKERY: return good == CC_GOOD_BREAD;
        case CC_LOCAL_SHOP_BUTCHER: return good == CC_GOOD_MEAT;
        case CC_LOCAL_SHOP_GRAIN_MERCHANT: return good == CC_GOOD_WHEAT;
        case CC_LOCAL_SHOP_SMITH:
            return good == CC_GOOD_IRON || good == CC_GOOD_TOOLS ||
                   good == CC_GOOD_WEAPONS;
        case CC_LOCAL_SHOP_JEWELER:
            return good == CC_GOOD_GOLD || good == CC_GOOD_GEMS;
        case CC_LOCAL_SHOP_TIMBER_MERCHANT: return good == CC_GOOD_WOOD;
        case CC_LOCAL_SHOP_CLOTHIER: return good == CC_GOOD_WOOL;
        case CC_LOCAL_SHOP_STONECUTTER: return good == CC_GOOD_STONE;
        case CC_LOCAL_SHOP_STATIONER: return good == CC_GOOD_PAPER;
        case CC_LOCAL_SHOP_MINE_SUPPLIER: return good == CC_GOOD_RAW_STONE;
        case CC_LOCAL_SHOP_COUNT: return false;
    }
    return false;
}

bool CcLocalShopBuys(const CcLocalShop *shop, CcGood good)
{
    if (shop == NULL) return false;
    /* The stationer makes paper from wood: it buys the wood and sells the
       paper, and does not take paper back. */
    if (shop->kind == CC_LOCAL_SHOP_STATIONER) return good == CC_GOOD_WOOD;
    if (CcLocalShopSells(shop, good)) return true;
    if (shop->kind == CC_LOCAL_SHOP_BAKERY && good == CC_GOOD_WHEAT)
        return true;
    if (shop->kind == CC_LOCAL_SHOP_STONECUTTER &&
        good == CC_GOOD_RAW_STONE) return true;
    if (shop->kind == CC_LOCAL_SHOP_BUTCHER &&
        good == CC_GOOD_ROTTEN_MEAT) return true;
    if (shop->kind == CC_LOCAL_SHOP_GRAIN_MERCHANT &&
        good == CC_GOOD_ROTTEN_GRAIN) return true;
    return false;
}

const CcLocalShop *CcLocalShopForGood(const CcLocalPlaceProfile *profile,
                                     CcGood good)
{
    for (int32_t kind = 0; kind < CC_LOCAL_SHOP_COUNT; ++kind) {
        const CcLocalShop *shop = CcLocalShopAt(profile, (CcLocalShopKind)kind);
        if (CcLocalShopSells(shop, good)) return shop;
    }
    return NULL;
}

static CcLocalLanePoint ShopFront(const CcLocalPlaceProfile *profile,
                                   const CcLocalShop *shop, float offset)
{
    CcLocalLanePoint point = {0};
    if (profile == NULL || shop == NULL || shop->building_index < 0 ||
        shop->building_index >= profile->building_count) return point;
    const CcLocalPlaceBuilding *building = &profile->building[shop->building_index];
    float angle = profile->building_yaw_degrees[shop->building_index] *
                  (3.14159265358979323846f / 180.0f);
    float cx = building->x + building->width * 0.5f;
    float cz = building->z + building->depth * 0.5f;
    float front = building->depth * 0.5f + offset;
    float side = profile->function == CC_SETTLEMENT_DUNGEON_TOWN ?
        building->width * ((shop->building_index & 1) != 0 ? 0.16f : -0.16f) : 0.0f;
    point.x = cx + cosf(angle) * side + sinf(angle) * front;
    point.z = cz - sinf(angle) * side + cosf(angle) * front;
    return point;
}

CcLocalLanePoint CcLocalShopDoor(const CcLocalPlaceProfile *profile,
                                 const CcLocalShop *shop)
{
    return ShopFront(profile, shop, 0.12f);
}

CcLocalLanePoint CcLocalShopApproach(const CcLocalPlaceProfile *profile,
                                     const CcLocalShop *shop)
{
    return ShopFront(profile, shop, 1.55f);
}
