#ifndef CROWNLESS_LOCAL_SHOPS_H
#define CROWNLESS_LOCAL_SHOPS_H

#include "client/cc_local_place.h"

typedef enum CcLocalShopKind {
    CC_LOCAL_SHOP_BAKERY = 0,
    CC_LOCAL_SHOP_BUTCHER,
    CC_LOCAL_SHOP_GRAIN_MERCHANT,
    CC_LOCAL_SHOP_SMITH,
    CC_LOCAL_SHOP_JEWELER,
    CC_LOCAL_SHOP_TIMBER_MERCHANT,
    CC_LOCAL_SHOP_CLOTHIER,
    CC_LOCAL_SHOP_STONECUTTER,
    CC_LOCAL_SHOP_STATIONER,
    CC_LOCAL_SHOP_MINE_SUPPLIER,
    CC_LOCAL_SHOP_COUNT
} CcLocalShopKind;

typedef struct CcLocalShop {
    CcLocalShopKind kind;
    int32_t building_index;
    const char *name;
    const char *keeper_name;
    const char *service_name;
} CcLocalShop;

/* Shops select goods from the settlement's shared stock and prices. */
const CcLocalShop *CcLocalShopAt(const CcLocalPlaceProfile *profile,
                                CcLocalShopKind kind);
const CcLocalShop *CcLocalShopForBuilding(const CcLocalPlaceProfile *profile,
                                         int32_t building_index);
const CcLocalShop *CcLocalShopForGood(const CcLocalPlaceProfile *profile,
                                     CcGood good);
bool CcLocalShopSells(const CcLocalShop *shop, CcGood good);
bool CcLocalShopBuys(const CcLocalShop *shop, CcGood good);

/* The visible door is at the rotated front wall; approach is outside it. */
CcLocalLanePoint CcLocalShopDoor(const CcLocalPlaceProfile *profile,
                                 const CcLocalShop *shop);
CcLocalLanePoint CcLocalShopApproach(const CcLocalPlaceProfile *profile,
                                     const CcLocalShop *shop);

#endif
