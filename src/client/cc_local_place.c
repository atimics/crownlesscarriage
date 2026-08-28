#include "client/cc_local_place.h"

static const CcLocalPlaceProfile PLACE_PROFILES[] = {
    {
        .function = CC_SETTLEMENT_FARMING,
        .identity = "THE GRANARY COUNTRY",
        .purpose = "Fields, mills, and guarded food stores",
        .primary_hall = "Granary hall",
        .notice_board = "Harvest board",
        .training_yard = "Drovers' yard",
        .compound = "Manor granary",
        .terrain_salt = UINT32_C(0x16f11fe9),
        .feature_mask = CC_LOCAL_PLACE_FARMLAND |
                        CC_LOCAL_PLACE_CARRIAGE,
        .room_name = {
            "DROVERS' YARD", "WEST CROFTS", "FORD ROAD",
            "MILL STREET", "GRANARY GREEN", "CART SHEDS",
            "PROVISION HALL", "NORTH FIELDS", "RIVER GATE",
            "EAST ORCHARDS",
        },
    },
    {
        .function = CC_SETTLEMENT_MINING,
        .identity = "THE WORKING DEEPS",
        .purpose = "Ore, furnaces, and the Lower Silverworks",
        .primary_hall = "Company store",
        .notice_board = "Shift board",
        .training_yard = "Haulage yard",
        .compound = "Mine company",
        .terrain_salt = UINT32_C(0x8b62c4d7),
        .feature_mask = CC_LOCAL_PLACE_INDUSTRY |
                        CC_LOCAL_PLACE_CARRIAGE |
                        CC_LOCAL_PLACE_DUNGEON,
        .room_name = {
            "HAULAGE YARD", "WORKERS' CROFTS", "DEEP ROAD",
            "SMELTER ROW", "MINERS' SQUARE", "ORE YARD",
            "COMPANY STORE", "FURNACE ROAD", "MINE GATE",
            "SLAG FIELDS",
        },
    },
    {
        .function = CC_SETTLEMENT_MARKET,
        .identity = "THE CROSSROADS MARKET",
        .purpose = "Carriages, contracts, and regional trade",
        .primary_hall = "Market",
        .notice_board = "Contract board",
        .training_yard = "Wayfarer trials",
        .compound = "Customs keep",
        .terrain_salt = UINT32_C(0x45d9f3b1),
        .feature_mask = CC_LOCAL_PLACE_MARKET |
                        CC_LOCAL_PLACE_INDUSTRY |
                        CC_LOCAL_PLACE_CARRIAGE,
        .room_name = {
            "TRAINING YARD", "WEST FARMS", "OLD MINE ROAD",
            "WORKSHOP STREET", "TOWN SQUARE", "CARRIAGE YARD",
            "MARKET STEPS", "MILLER'S ROAD", "CROWN GATE",
            "EAST FIELDS",
        },
    },
    {
        .function = CC_SETTLEMENT_FORTRESS,
        .identity = "THE CONTESTED BRIDGE",
        .purpose = "Garrison, inspections, and the eastern crossing",
        .primary_hall = "Quartermaster",
        .notice_board = "Muster board",
        .training_yard = "Drill yard",
        .compound = "Alderwatch keep",
        .terrain_salt = UINT32_C(0xca4b2d53),
        .feature_mask = CC_LOCAL_PLACE_FORTIFICATION |
                        CC_LOCAL_PLACE_INDUSTRY |
                        CC_LOCAL_PLACE_CARRIAGE,
        .room_name = {
            "DRILL YARD", "CAMP FIELDS", "WESTERN MARCH",
            "ARMOURER'S ROW", "MUSTER SQUARE", "SUPPLY YARD",
            "TOLL HALL", "GARRISON ROAD", "ALDER GATE",
            "EASTERN MARCH",
        },
    },
    {
        .function = CC_SETTLEMENT_CAPITAL,
        .identity = "THE ROYAL SEAT",
        .purpose = "Court, treasury, and the realm's petitions",
        .primary_hall = "Royal exchange",
        .notice_board = "Petition board",
        .training_yard = "Royal yard",
        .compound = "Crown palace",
        .terrain_salt = UINT32_C(0x71d54a8f),
        .feature_mask = CC_LOCAL_PLACE_MARKET |
                        CC_LOCAL_PLACE_FORTIFICATION |
                        CC_LOCAL_PLACE_CARRIAGE |
                        CC_LOCAL_PLACE_COURT,
        .room_name = {
            "ROYAL YARD", "WEST GARDENS", "PILGRIM ROAD",
            "GUILD STREET", "PETITION SQUARE", "COACH COURT",
            "ROYAL EXCHANGE", "PROCESSION WAY", "ROSE GATE",
            "EAST GARDENS",
        },
    },
    {
        .function = CC_SETTLEMENT_DUNGEON_TOWN,
        .identity = "THE FRONTIER WARD",
        .purpose = "Expeditions, salvage, and a guarded underworld",
        .primary_hall = "Expedition store",
        .notice_board = "Expedition board",
        .training_yard = "Warden yard",
        .compound = "Dungeon ward",
        .terrain_salt = UINT32_C(0x9e37b97d),
        .feature_mask = CC_LOCAL_PLACE_INDUSTRY |
                        CC_LOCAL_PLACE_CARRIAGE |
                        CC_LOCAL_PLACE_DUNGEON,
        .room_name = {
            "WARDEN YARD", "SETTLERS' ROW", "SEALED ROAD",
            "SALVAGE STREET", "LANTERN SQUARE", "EXPEDITION YARD",
            "SUPPLY HOUSE", "WATCH ROAD", "WARD GATE",
            "OUTER DIGS",
        },
    },
};

static uint32_t MixSeed(uint32_t value)
{
    value ^= value >> 16U;
    value *= UINT32_C(0x7feb352d);
    value ^= value >> 15U;
    value *= UINT32_C(0x846ca68b);
    return value ^ (value >> 16U);
}

const CcLocalPlaceProfile *CcLocalPlaceProfileForFunction(
    CcSettlementFunction function)
{
    for (int32_t i = 0;
         i < (int32_t)(sizeof(PLACE_PROFILES) / sizeof(PLACE_PROFILES[0]));
         ++i) {
        if (PLACE_PROFILES[i].function == function) return &PLACE_PROFILES[i];
    }
    return CcLocalPlaceProfileForFunction(CC_SETTLEMENT_MARKET);
}

const CcLocalPlaceProfile *CcLocalPlaceProfileForSettlement(
    const CcSettlement *settlement)
{
    return CcLocalPlaceProfileForFunction(
        settlement != NULL ? settlement->function : CC_SETTLEMENT_MARKET);
}

const char *CcLocalPlaceRoomName(CcSettlementFunction function,
                                 int32_t room_index)
{
    const CcLocalPlaceProfile *profile =
        CcLocalPlaceProfileForFunction(function);
    if (room_index < 0 || room_index >= CC_LOCAL_PLACE_ROOM_COUNT) return NULL;
    return profile->room_name[room_index];
}

bool CcLocalPlaceHasFeature(const CcLocalPlaceProfile *profile,
                            CcLocalPlaceFeature feature)
{
    return profile != NULL && (profile->feature_mask & (uint32_t)feature) != 0U;
}

uint32_t CcLocalPlaceTerrainSeed(uint32_t world_seed,
                                 const CcSettlement *settlement)
{
    const CcLocalPlaceProfile *profile =
        CcLocalPlaceProfileForSettlement(settlement);
    CcId id = settlement != NULL ? settlement->id : 0U;
    uint32_t folded_id = (uint32_t)id ^ (uint32_t)(id >> 32U);
    uint32_t seed = MixSeed(world_seed ^ folded_id ^ profile->terrain_salt);
    return seed != 0U ? seed : profile->terrain_salt;
}
