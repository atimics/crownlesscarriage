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
        .keeper_name = "Edda — Granary keeper",
        .interior_service = "Provisions, seed, and harvest accounts",
        .keeper_seed = UINT32_C(0xedda1001),
        .terrain_salt = UINT32_C(0x16f11fe9),
        .feature_mask = CC_LOCAL_PLACE_FARMLAND |
                        CC_LOCAL_PLACE_CARRIAGE,
        .room_name = {
            "DROVERS' YARD", "WEST CROFTS", "FORD ROAD",
            "MILL STREET", "GRANARY GREEN", "CART SHEDS",
            "PROVISION HALL", "NORTH FIELDS", "RIVER GATE",
            "EAST ORCHARDS",
        },
        .landmark = {
            {CC_LOCAL_LANDMARK_AGRICULTURE, 0, "Threshing barn",
             4.20f, 18.00f, 6.20f, 4.20f, 4.80f},
            {CC_LOCAL_LANDMARK_AGRICULTURE, 1, "Hill granaries",
             5.20f, 43.20f, 4.80f, 4.20f, 4.60f},
            {CC_LOCAL_LANDMARK_AGRICULTURE, 2, "Orchard press",
             66.20f, 44.00f, 5.20f, 4.40f, 3.80f},
        },
        .road = {
            {"Thresher lane", 9.40f, 12.60f, 3.10f, 15.00f, false,
             CC_LOCAL_ROAD_FARM_TRACK},
            {"Hill cartway", 9.20f, 31.00f, 5.20f, 24.50f, false,
             CC_LOCAL_ROAD_FARM_TRACK},
            {"Orchard lane", 68.00f, 47.40f, 3.00f, 5.40f, false,
             CC_LOCAL_ROAD_FARM_TRACK},
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
        .keeper_name = "Oren — Company clerk",
        .interior_service = "Ore, tools, and expedition stores",
        .keeper_seed = UINT32_C(0x0e2e4002),
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
        .landmark = {
            {CC_LOCAL_LANDMARK_INDUSTRY, 0, "North headframe",
             4.60f, 18.00f, 5.30f, 5.00f, 6.80f},
            {CC_LOCAL_LANDMARK_INDUSTRY, 1, "Company furnace",
             18.20f, 44.20f, 5.50f, 4.40f, 5.80f},
            {CC_LOCAL_LANDMARK_INDUSTRY, 2, "Ore breaker",
             84.00f, 44.00f, 5.20f, 4.60f, 4.40f},
        },
        .road = {
            {"Pit lane", 9.20f, 12.50f, 3.40f, 15.30f, false,
             CC_LOCAL_ROAD_INDUSTRIAL},
            {"Furnace road", 22.10f, 47.40f, 3.20f, 8.20f, false,
             CC_LOCAL_ROAD_INDUSTRIAL},
            {"Ore tramway", 80.00f, 47.40f, 9.40f, 3.20f, true,
             CC_LOCAL_ROAD_INDUSTRIAL},
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
        .keeper_name = "Mara — Merchant",
        .interior_service = "Contracts, cargo, and regional goods",
        .keeper_seed = UINT32_C(0x6d617261),
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
        .landmark = {
            {CC_LOCAL_LANDMARK_COMMERCE, 0, "Wool warehouse",
             4.00f, 18.00f, 6.50f, 4.50f, 5.20f},
            {CC_LOCAL_LANDMARK_COMMERCE, 1, "Caravan pavilion",
             5.00f, 43.00f, 5.50f, 4.00f, 4.20f},
            {CC_LOCAL_LANDMARK_COMMERCE, 2, "East customs",
             66.00f, 44.00f, 5.50f, 4.50f, 5.50f},
        },
        .road = {
            {"Wool lane", 9.80f, 12.60f, 3.60f, 15.00f, false,
             CC_LOCAL_ROAD_TRADE},
            {"Caravan lane", 9.00f, 46.20f, 5.20f, 9.20f, false,
             CC_LOCAL_ROAD_TRADE},
            {"Customs lane", 68.00f, 47.50f, 3.20f, 5.30f, false,
             CC_LOCAL_ROAD_TRADE},
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
        .keeper_name = "Seren — Quartermaster",
        .interior_service = "Rations, arms, and bridge passage",
        .keeper_seed = UINT32_C(0x5e2e4004),
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
        .landmark = {
            {CC_LOCAL_LANDMARK_MILITARY, 0, "West barracks",
             4.20f, 18.00f, 6.50f, 4.50f, 5.00f},
            {CC_LOCAL_LANDMARK_MILITARY, 1, "March tower",
             6.00f, 43.00f, 4.00f, 4.00f, 7.00f},
            {CC_LOCAL_LANDMARK_MILITARY, 2, "Field armoury",
             65.50f, 43.50f, 6.00f, 4.50f, 5.20f},
        },
        .road = {
            {"Barracks lane", 10.00f, 12.60f, 4.20f, 15.00f, false,
             CC_LOCAL_ROAD_MILITARY},
            {"March road", 9.00f, 46.00f, 5.20f, 9.50f, false,
             CC_LOCAL_ROAD_MILITARY},
            {"Armoury road", 68.00f, 47.30f, 3.40f, 5.50f, false,
             CC_LOCAL_ROAD_MILITARY},
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
        .keeper_name = "Ilyra — Royal factor",
        .interior_service = "Petitions, court supply, and royal exchange",
        .keeper_seed = UINT32_C(0x1172a005),
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
        .landmark = {
            {CC_LOCAL_LANDMARK_CIVIC, 0, "West chancery",
             4.00f, 18.00f, 6.50f, 4.50f, 5.80f},
            {CC_LOCAL_LANDMARK_CIVIC, 1, "Founders' stone",
             7.00f, 43.00f, 3.00f, 3.00f, 5.50f},
            {CC_LOCAL_LANDMARK_CIVIC, 2, "Rose pavilion",
             65.00f, 43.70f, 6.00f, 4.50f, 4.80f},
        },
        .road = {
            {"Chancery way", 9.80f, 12.60f, 4.00f, 15.00f, false,
             CC_LOCAL_ROAD_PROCESSIONAL},
            {"Founders' walk", 8.20f, 45.00f, 6.00f, 10.00f, false,
             CC_LOCAL_ROAD_PROCESSIONAL},
            {"Rose walk", 67.00f, 47.30f, 3.60f, 5.70f, false,
             CC_LOCAL_ROAD_PROCESSIONAL},
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
        .keeper_name = "Vey — Expedition broker",
        .interior_service = "Rope, lamps, salvage, and return bonds",
        .keeper_seed = UINT32_C(0x0b3e7006),
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
        .landmark = {
            {CC_LOCAL_LANDMARK_EXPEDITION, 0, "Delvers' lodge",
             4.00f, 18.00f, 6.20f, 4.50f, 5.00f},
            {CC_LOCAL_LANDMARK_EXPEDITION, 1, "Lantern tower",
             18.50f, 44.00f, 4.00f, 4.00f, 7.00f},
            {CC_LOCAL_LANDMARK_EXPEDITION, 2, "Salvage works",
             66.00f, 44.00f, 5.50f, 4.50f, 4.50f},
        },
        .road = {
            {"Delvers' path", 9.50f, 12.60f, 3.20f, 15.00f, false,
             CC_LOCAL_ROAD_EXPEDITION},
            {"Lantern road", 21.00f, 47.00f, 3.20f, 8.00f, false,
             CC_LOCAL_ROAD_EXPEDITION},
            {"Salvage path", 68.00f, 47.30f, 3.20f, 5.50f, false,
             CC_LOCAL_ROAD_EXPEDITION},
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

const CcLocalPlaceLandmark *CcLocalPlaceLandmarkAt(
    CcSettlementFunction function, int32_t landmark_index)
{
    const CcLocalPlaceProfile *profile =
        CcLocalPlaceProfileForFunction(function);
    if (landmark_index < 0 ||
        landmark_index >= CC_LOCAL_PLACE_LANDMARK_COUNT) return NULL;
    return &profile->landmark[landmark_index];
}

const CcLocalPlaceRoad *CcLocalPlaceRoadAt(
    CcSettlementFunction function, int32_t road_index)
{
    const CcLocalPlaceProfile *profile =
        CcLocalPlaceProfileForFunction(function);
    if (road_index < 0 || road_index >= CC_LOCAL_PLACE_ROAD_COUNT) return NULL;
    return &profile->road[road_index];
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
