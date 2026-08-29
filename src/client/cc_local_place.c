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
        .map_form = "Dispersed crofts around a threshing green",
        .scene = {
            {CC_LOCAL_TOWN_SCENE_ARRIVAL, "RIVERFORD ARRIVAL",
             82.0f, 34.0f, 81.0f, 1.8f, 34.0f,
             -24.0f, 15.0f, 36.0f, 28.0f},
            {CC_LOCAL_TOWN_SCENE_HEART, "THRESHING GREEN",
             44.0f, 29.0f, 45.0f, 1.4f, 29.5f,
             -27.0f, 14.0f, 34.0f, 27.0f},
            {CC_LOCAL_TOWN_SCENE_LANDMARK, "HILL GRANARIES",
             78.0f, 19.0f, 78.0f, 4.2f, 19.0f,
             -24.0f, 19.0f, 38.0f, 32.0f},
            {CC_LOCAL_TOWN_SCENE_CLOSE_FIRST, "DROVERS' CLOSE",
             32.0f, 38.0f, 31.0f, 1.4f, 39.5f,
             -13.0f, 6.5f, 17.0f, 17.5f},
            {CC_LOCAL_TOWN_SCENE_CLOSE_SECOND, "MILLER'S BEND",
             58.0f, 50.0f, 60.5f, 1.7f, 50.0f,
             13.0f, 6.5f, 17.0f, 17.5f},
        },
        .keeper_seed = UINT32_C(0xedda1001),
        .terrain_salt = UINT32_C(0x16f11fe9),
        .feature_mask = CC_LOCAL_PLACE_FARMLAND |
                        CC_LOCAL_PLACE_CARRIAGE,
        .building_count = 8,
        .primary_building = 2,
        .compound_structure_count = 9,
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
        .carriage_route = {
            {"River gate ramp", 75.80f, 27.00f, 5.20f, 11.50f, false,
             CC_LOCAL_ROAD_FARM_TRACK},
            {"Drovers' road", 39.00f, 33.20f, 57.00f, 5.20f, true,
             CC_LOCAL_ROAD_FARM_TRACK},
            {"Granary spine", 39.00f, 33.20f, 5.80f, 24.80f, false,
             CC_LOCAL_ROAD_FARM_TRACK},
            {"Drovers' turn", 35.80f, 30.00f, 12.00f, 12.00f, true,
             CC_LOCAL_ROAD_FARM_TRACK},
            {"Cartwright yard", 35.20f, 44.00f, 13.40f, 14.00f, true,
             CC_LOCAL_ROAD_FARM_TRACK},
        },
        .building = {
            {"Long threshing barn", 19.00f, 15.00f, 10.00f, 9.50f,
             6.40f, CC_LOCAL_BUILDING_WORKSHOP, true},
            {"Crofter cottages", 32.00f, 15.00f, 6.20f, 8.00f,
             5.10f, CC_LOCAL_BUILDING_DOMESTIC, true},
            {"Provision hall", 44.00f, 16.00f, 12.00f, 10.00f,
             7.40f, CC_LOCAL_BUILDING_WORKSHOP, true},
            {"Drovers' longhouse", 19.50f, 34.00f, 11.50f, 7.20f,
             5.40f, CC_LOCAL_BUILDING_DOMESTIC, true},
            {"Dairy cottage", 31.00f, 35.50f, 4.50f, 7.20f,
             4.80f, CC_LOCAL_BUILDING_DOMESTIC, true},
            {"Cartwright sheds", 55.00f, 42.00f, 9.00f, 6.20f,
             5.20f, CC_LOCAL_BUILDING_WORKSHOP, true},
            {"Miller's house", 57.00f, 15.00f, 8.00f, 8.20f,
             5.80f, CC_LOCAL_BUILDING_DOMESTIC, true},
            {"Orchard bunkhouse", 49.50f, 58.00f, 9.50f, 7.00f,
             5.10f, CC_LOCAL_BUILDING_DOMESTIC, true},
        },
        .compound_structure = {
            {CC_LOCAL_COMPOUND_WALL, 67.50f, 10.00f, 0.70f, 20.00f, 2.40f},
            {CC_LOCAL_COMPOUND_WALL, 89.00f, 10.00f, 0.70f, 20.00f, 2.40f},
            {CC_LOCAL_COMPOUND_WALL, 67.50f, 10.00f, 22.20f, 0.70f, 2.40f},
            {CC_LOCAL_COMPOUND_WALL, 67.50f, 29.30f, 8.30f, 0.70f, 2.40f},
            {CC_LOCAL_COMPOUND_WALL, 81.00f, 29.30f, 8.70f, 0.70f, 2.40f},
            {CC_LOCAL_COMPOUND_HALL, 71.00f, 13.00f, 12.00f, 8.00f, 7.20f},
            {CC_LOCAL_COMPOUND_SILO, 85.00f, 13.00f, 3.00f, 3.00f, 7.40f},
            {CC_LOCAL_COMPOUND_SILO, 85.00f, 18.00f, 3.00f, 3.00f, 6.60f},
            {CC_LOCAL_COMPOUND_STOREHOUSE, 70.00f, 23.00f,
             5.50f, 4.80f, 4.40f},
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
        .map_form = "Dense worker terraces climbing toward the pit",
        .scene = {
            {CC_LOCAL_TOWN_SCENE_ARRIVAL, "QUARRY APPROACH",
             82.0f, 34.0f, 81.0f, 2.5f, 34.0f,
             -25.0f, 13.0f, 36.0f, 28.0f},
            {CC_LOCAL_TOWN_SCENE_HEART, "MINERS' TERRACES",
             44.0f, 29.0f, 45.0f, 2.8f, 30.0f,
             28.0f, 15.0f, 36.0f, 29.0f},
            {CC_LOCAL_TOWN_SCENE_LANDMARK, "LOWER SILVERWORKS",
             78.0f, 19.0f, 79.0f, 5.6f, 18.0f,
             -27.0f, 21.0f, 39.0f, 33.0f},
            {CC_LOCAL_TOWN_SCENE_CLOSE_FIRST, "LAMPWRIGHT ROW",
             33.0f, 25.0f, 36.0f, 2.2f, 24.5f,
             -13.0f, 6.5f, 17.0f, 17.0f},
            {CC_LOCAL_TOWN_SCENE_CLOSE_SECOND, "FURNACE ALLEY",
             27.0f, 50.0f, 24.0f, 2.8f, 47.5f,
             13.0f, 6.5f, 17.0f, 17.5f},
        },
        .keeper_seed = UINT32_C(0x0e2e4002),
        .terrain_salt = UINT32_C(0x8b62c4d7),
        .feature_mask = CC_LOCAL_PLACE_INDUSTRY |
                        CC_LOCAL_PLACE_CARRIAGE |
                        CC_LOCAL_PLACE_DUNGEON,
        .building_count = 11,
        .primary_building = 2,
        .compound_structure_count = 10,
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
        .carriage_route = {
            {"Mine gate ramp", 75.80f, 27.00f, 5.20f, 11.50f, false,
             CC_LOCAL_ROAD_INDUSTRIAL},
            {"Quarry road", 39.00f, 33.20f, 57.00f, 5.20f, true,
             CC_LOCAL_ROAD_INDUSTRIAL},
            {"Haulage spine", 39.00f, 33.20f, 5.80f, 24.80f, false,
             CC_LOCAL_ROAD_INDUSTRIAL},
            {"Pit turn", 35.80f, 30.00f, 12.00f, 12.00f, true,
             CC_LOCAL_ROAD_INDUSTRIAL},
            {"Ore wagon yard", 35.20f, 44.00f, 13.40f, 14.00f, true,
             CC_LOCAL_ROAD_INDUSTRIAL},
        },
        .building = {
            {"Lower bunk row", 19.50f, 15.00f, 9.00f, 10.00f,
             7.20f, CC_LOCAL_BUILDING_WORKER_ROW, true},
            {"Lampwright row", 31.00f, 14.50f, 7.40f, 9.50f,
             6.80f, CC_LOCAL_BUILDING_WORKER_ROW, true},
            {"Company store", 44.00f, 16.00f, 12.00f, 10.00f,
             9.40f, CC_LOCAL_BUILDING_CIVIC, true},
            {"Foundry tenement", 20.00f, 34.00f, 10.50f, 8.00f,
             7.80f, CC_LOCAL_BUILDING_WORKER_ROW, true},
            {"Assay house", 31.00f, 34.50f, 4.50f, 8.30f,
             6.60f, CC_LOCAL_BUILDING_WORKSHOP, true},
            {"Ore warehouse", 54.50f, 42.00f, 10.00f, 7.00f,
             6.80f, CC_LOCAL_BUILDING_WORKSHOP, true},
            {"Shift kitchen", 57.00f, 25.50f, 7.00f, 5.50f,
             5.20f, CC_LOCAL_BUILDING_DOMESTIC, true},
            {"Upper bunk row", 57.00f, 14.00f, 8.00f, 10.00f,
             7.40f, CC_LOCAL_BUILDING_WORKER_ROW, true},
            {"Powder office", 17.50f, 50.00f, 8.20f, 6.50f,
             5.80f, CC_LOCAL_BUILDING_WORKSHOP, true},
            {"Smelter lodging", 50.00f, 58.00f, 10.50f, 7.50f,
             7.10f, CC_LOCAL_BUILDING_WORKER_ROW, true},
            {"East weigh house", 83.00f, 55.00f, 6.50f, 8.00f,
             6.20f, CC_LOCAL_BUILDING_WORKSHOP, true},
        },
        .compound_structure = {
            {CC_LOCAL_COMPOUND_WALL, 66.50f, 9.50f, 0.90f, 21.00f, 4.20f},
            {CC_LOCAL_COMPOUND_WALL, 90.00f, 9.50f, 0.90f, 21.00f, 4.20f},
            {CC_LOCAL_COMPOUND_WALL, 66.50f, 9.50f, 24.40f, 0.90f, 4.20f},
            {CC_LOCAL_COMPOUND_WALL, 66.50f, 29.60f, 9.30f, 0.90f, 4.20f},
            {CC_LOCAL_COMPOUND_WALL, 81.00f, 29.60f, 9.90f, 0.90f, 4.20f},
            {CC_LOCAL_COMPOUND_HALL, 70.00f, 12.00f, 14.00f, 8.00f, 8.40f},
            {CC_LOCAL_COMPOUND_TOWER, 86.00f, 12.00f, 3.20f, 3.20f, 9.20f},
            {CC_LOCAL_COMPOUND_STOREHOUSE, 69.50f, 22.50f,
             6.00f, 5.00f, 5.40f},
            {CC_LOCAL_COMPOUND_SILO, 85.50f, 18.00f, 3.40f, 3.40f, 8.20f},
            {CC_LOCAL_COMPOUND_TOWER, 86.00f, 25.00f, 3.20f, 3.20f, 7.60f},
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
        .map_form = "A radial bazaar gathered around the coach road",
        .scene = {
            {CC_LOCAL_TOWN_SCENE_ARRIVAL, "CUSTOMS CAUSEWAY",
             82.0f, 34.0f, 81.0f, 2.0f, 34.0f,
             -25.0f, 15.0f, 38.0f, 28.0f},
            {CC_LOCAL_TOWN_SCENE_HEART, "CROSSROADS BAZAAR",
             44.0f, 29.0f, 46.0f, 2.0f, 29.5f,
             -28.0f, 16.0f, 34.0f, 29.0f},
            {CC_LOCAL_TOWN_SCENE_LANDMARK, "CUSTOMS KEEP",
             78.0f, 19.0f, 78.0f, 5.0f, 19.0f,
             -25.0f, 21.0f, 40.0f, 33.0f},
            {CC_LOCAL_TOWN_SCENE_CLOSE_FIRST, "ARCADE CLOSE",
             50.0f, 27.25f, 49.5f, 2.4f, 23.5f,
             -13.5f, 6.5f, 17.5f, 17.0f},
            {CC_LOCAL_TOWN_SCENE_CLOSE_SECOND, "COACH COURT",
             42.0f, 52.0f, 42.0f, 1.6f, 50.0f,
             14.0f, 6.5f, 17.0f, 18.0f},
        },
        .keeper_seed = UINT32_C(0x6d617261),
        .terrain_salt = UINT32_C(0x45d9f3b1),
        .feature_mask = CC_LOCAL_PLACE_MARKET |
                        CC_LOCAL_PLACE_INDUSTRY |
                        CC_LOCAL_PLACE_CARRIAGE,
        .building_count = 10,
        .primary_building = 2,
        .compound_structure_count = 12,
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
        .carriage_route = {
            {"Customs gate ramp", 75.80f, 27.00f, 5.20f, 11.50f, false,
             CC_LOCAL_ROAD_TRADE},
            {"Coach road", 39.00f, 33.20f, 57.00f, 5.20f, true,
             CC_LOCAL_ROAD_TRADE},
            {"Caravan spine", 39.00f, 33.20f, 5.80f, 24.80f, false,
             CC_LOCAL_ROAD_TRADE},
            {"Bazaar turn", 35.80f, 30.00f, 12.00f, 12.00f, true,
             CC_LOCAL_ROAD_TRADE},
            {"Coach service yard", 35.20f, 44.00f, 13.40f, 14.00f, true,
             CC_LOCAL_ROAD_TRADE},
        },
        .building = {
            {"West hostel", 20.00f, 15.00f, 8.00f, 10.00f,
             7.20f, CC_LOCAL_BUILDING_DOMESTIC, true},
            {"Cloth factors", 32.00f, 14.00f, 7.00f, 9.00f,
             6.40f, CC_LOCAL_BUILDING_WORKSHOP, true},
            {"Crossroads market", 44.00f, 16.00f, 12.00f, 10.00f,
             8.80f, CC_LOCAL_BUILDING_CIVIC, true},
            {"Spice warehouse", 20.00f, 33.00f, 10.00f, 8.00f,
             6.60f, CC_LOCAL_BUILDING_WORKER_ROW, true},
            {"Money changers", 31.00f, 35.00f, 4.50f, 8.00f,
             5.80f, CC_LOCAL_BUILDING_DOMESTIC, true},
            {"Caravan hostel", 55.00f, 42.00f, 8.00f, 7.00f,
             6.20f, CC_LOCAL_BUILDING_WORKSHOP, true},
            {"Public kitchen", 57.00f, 25.00f, 6.50f, 5.50f,
             5.90f, CC_LOCAL_BUILDING_DOMESTIC, true},
            {"Guild factors", 57.00f, 14.00f, 7.00f, 9.00f,
             6.10f, CC_LOCAL_BUILDING_WORKER_ROW, true},
            {"Mapmakers' row", 17.00f, 46.00f, 8.00f, 7.00f,
             5.80f, CC_LOCAL_BUILDING_WORKSHOP, true},
            {"Eastern exchange", 50.00f, 58.00f, 10.00f, 7.00f,
             6.90f, CC_LOCAL_BUILDING_CIVIC, true},
        },
        .compound_structure = {
            {CC_LOCAL_COMPOUND_WALL, 66.00f, 9.00f, 1.20f, 23.00f, 6.50f},
            {CC_LOCAL_COMPOUND_WALL, 90.00f, 9.00f, 1.20f, 23.00f, 6.50f},
            {CC_LOCAL_COMPOUND_WALL, 66.00f, 9.00f, 25.20f, 1.20f, 6.50f},
            {CC_LOCAL_COMPOUND_WALL, 66.00f, 30.80f, 9.50f, 1.20f, 6.50f},
            {CC_LOCAL_COMPOUND_WALL, 81.50f, 30.80f, 9.70f, 1.20f, 6.50f},
            {CC_LOCAL_COMPOUND_HALL, 72.00f, 13.00f, 13.00f, 9.00f, 11.50f},
            {CC_LOCAL_COMPOUND_TOWER, 73.00f, 27.80f, 2.00f, 3.00f, 9.00f},
            {CC_LOCAL_COMPOUND_TOWER, 82.00f, 27.80f, 2.00f, 3.00f, 9.00f},
            {CC_LOCAL_COMPOUND_TOWER, 64.80f, 7.80f, 4.00f, 4.00f, 12.50f},
            {CC_LOCAL_COMPOUND_TOWER, 88.40f, 7.80f, 4.00f, 4.00f, 12.50f},
            {CC_LOCAL_COMPOUND_TOWER, 64.80f, 29.20f, 4.00f, 4.00f, 12.50f},
            {CC_LOCAL_COMPOUND_TOWER, 88.40f, 29.20f, 4.00f, 4.00f, 12.50f},
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
        .map_form = "A straight garrison town aimed at the bridge",
        .scene = {
            {CC_LOCAL_TOWN_SCENE_ARRIVAL, "CONTESTED BRIDGE",
             82.0f, 34.0f, 81.0f, 2.8f, 34.0f,
             24.0f, 14.0f, 35.0f, 28.0f},
            {CC_LOCAL_TOWN_SCENE_HEART, "MUSTER SPINE",
             44.0f, 29.0f, 46.0f, 2.6f, 29.5f,
             -27.0f, 15.0f, 36.0f, 28.0f},
            {CC_LOCAL_TOWN_SCENE_LANDMARK, "ALDERWATCH KEEP",
             78.0f, 19.0f, 78.0f, 5.8f, 19.0f,
             26.0f, 22.0f, 38.0f, 33.0f},
            {CC_LOCAL_TOWN_SCENE_CLOSE_FIRST, "ARMOURERS' ROW",
             33.0f, 25.0f, 34.0f, 2.5f, 24.0f,
             13.0f, 6.5f, 17.0f, 17.0f},
            {CC_LOCAL_TOWN_SCENE_CLOSE_SECOND, "ALDER GATE PASS",
             73.5f, 32.5f, 78.5f, 4.0f, 29.5f,
             0.0f, 6.5f, 18.0f, 18.0f},
        },
        .keeper_seed = UINT32_C(0x5e2e4004),
        .terrain_salt = UINT32_C(0xca4b2d53),
        .feature_mask = CC_LOCAL_PLACE_FORTIFICATION |
                        CC_LOCAL_PLACE_INDUSTRY |
                        CC_LOCAL_PLACE_CARRIAGE,
        .building_count = 9,
        .primary_building = 2,
        .compound_structure_count = 12,
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
        .carriage_route = {
            {"Alder gate ramp", 75.80f, 27.00f, 5.20f, 11.50f, false,
             CC_LOCAL_ROAD_MILITARY},
            {"Bridge road", 39.00f, 33.20f, 57.00f, 5.20f, true,
             CC_LOCAL_ROAD_MILITARY},
            {"Muster spine", 39.00f, 33.20f, 5.80f, 24.80f, false,
             CC_LOCAL_ROAD_MILITARY},
            {"Gate turn", 35.80f, 30.00f, 12.00f, 12.00f, true,
             CC_LOCAL_ROAD_MILITARY},
            {"Supply wagon yard", 35.20f, 44.00f, 13.40f, 14.00f, true,
             CC_LOCAL_ROAD_MILITARY},
        },
        .building = {
            {"West barrack block", 19.00f, 15.00f, 10.00f, 10.00f,
             7.80f, CC_LOCAL_BUILDING_WORKER_ROW, true},
            {"Armourers' block", 31.50f, 14.00f, 7.50f, 10.00f,
             7.20f, CC_LOCAL_BUILDING_WORKSHOP, true},
            {"Toll hall", 44.00f, 16.00f, 12.00f, 10.00f,
             9.80f, CC_LOCAL_BUILDING_CIVIC, true},
            {"South barracks", 19.00f, 34.00f, 12.00f, 8.00f,
             7.60f, CC_LOCAL_BUILDING_WORKER_ROW, true},
            {"Surgeon's house", 31.00f, 35.00f, 4.50f, 8.00f,
             6.20f, CC_LOCAL_BUILDING_DOMESTIC, true},
            {"Supply arsenal", 55.00f, 42.00f, 9.00f, 7.00f,
             7.40f, CC_LOCAL_BUILDING_WORKSHOP, true},
            {"Gate sergeants", 57.00f, 24.50f, 7.00f, 5.00f,
             6.40f, CC_LOCAL_BUILDING_WORKER_ROW, true},
            {"Officers' mess", 57.00f, 14.00f, 8.00f, 10.00f,
             7.10f, CC_LOCAL_BUILDING_CIVIC, true},
            {"March stores", 50.00f, 58.00f, 10.00f, 7.50f,
             6.80f, CC_LOCAL_BUILDING_WORKSHOP, true},
        },
        .compound_structure = {
            {CC_LOCAL_COMPOUND_WALL, 65.00f, 8.00f, 1.50f, 24.50f, 8.00f},
            {CC_LOCAL_COMPOUND_WALL, 90.50f, 8.00f, 1.50f, 24.50f, 8.00f},
            {CC_LOCAL_COMPOUND_WALL, 65.00f, 8.00f, 27.00f, 1.50f, 8.00f},
            {CC_LOCAL_COMPOUND_WALL, 65.00f, 31.00f, 10.50f, 1.50f, 8.00f},
            {CC_LOCAL_COMPOUND_WALL, 81.50f, 31.00f, 10.50f, 1.50f, 8.00f},
            {CC_LOCAL_COMPOUND_HALL, 70.00f, 12.00f, 16.00f, 10.00f, 12.80f},
            {CC_LOCAL_COMPOUND_TOWER, 72.80f, 27.50f, 2.40f, 3.50f, 11.00f},
            {CC_LOCAL_COMPOUND_TOWER, 81.80f, 27.50f, 2.40f, 3.50f, 11.00f},
            {CC_LOCAL_COMPOUND_TOWER, 63.80f, 6.80f, 4.20f, 4.20f, 14.50f},
            {CC_LOCAL_COMPOUND_TOWER, 89.00f, 6.80f, 4.20f, 4.20f, 14.50f},
            {CC_LOCAL_COMPOUND_TOWER, 63.80f, 29.50f, 4.20f, 4.20f, 14.50f},
            {CC_LOCAL_COMPOUND_TOWER, 89.00f, 29.50f, 4.20f, 4.20f, 14.50f},
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
        .map_form = "Tall ceremonial wards on a processional axis",
        .scene = {
            {CC_LOCAL_TOWN_SCENE_ARRIVAL, "ROSE AVENUE",
             82.0f, 34.0f, 81.0f, 3.4f, 34.0f,
             -28.0f, 18.0f, 40.0f, 30.0f},
            {CC_LOCAL_TOWN_SCENE_HEART, "PETITION SQUARE",
             44.0f, 29.0f, 46.0f, 3.0f, 29.0f,
             29.0f, 18.0f, 38.0f, 30.0f},
            {CC_LOCAL_TOWN_SCENE_LANDMARK, "CROWN PALACE",
             78.0f, 19.0f, 78.0f, 7.0f, 18.0f,
             -30.0f, 24.0f, 43.0f, 34.0f},
            {CC_LOCAL_TOWN_SCENE_CLOSE_FIRST, "GOLDSMITH COURT",
             33.0f, 25.0f, 36.0f, 2.8f, 24.5f,
             -13.0f, 7.0f, 18.0f, 17.5f},
            {CC_LOCAL_TOWN_SCENE_CLOSE_SECOND, "ROSE CLOISTER",
             58.0f, 50.0f, 64.0f, 2.6f, 47.0f,
             13.0f, 7.0f, 18.0f, 18.0f},
        },
        .keeper_seed = UINT32_C(0x1172a005),
        .terrain_salt = UINT32_C(0x71d54a8f),
        .feature_mask = CC_LOCAL_PLACE_MARKET |
                        CC_LOCAL_PLACE_FORTIFICATION |
                        CC_LOCAL_PLACE_CARRIAGE |
                        CC_LOCAL_PLACE_COURT,
        .building_count = 12,
        .primary_building = 2,
        .compound_structure_count = 11,
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
        .carriage_route = {
            {"Rose gate ramp", 75.80f, 27.00f, 5.20f, 11.50f, false,
             CC_LOCAL_ROAD_PROCESSIONAL},
            {"Rose avenue", 39.00f, 33.20f, 57.00f, 5.20f, true,
             CC_LOCAL_ROAD_PROCESSIONAL},
            {"Procession spine", 39.00f, 33.20f, 5.80f, 24.80f, false,
             CC_LOCAL_ROAD_PROCESSIONAL},
            {"Petition turn", 35.80f, 30.00f, 12.00f, 12.00f, true,
             CC_LOCAL_ROAD_PROCESSIONAL},
            {"Royal coach court", 35.20f, 44.00f, 13.40f, 14.00f, true,
             CC_LOCAL_ROAD_PROCESSIONAL},
        },
        .building = {
            {"West chancery row", 19.00f, 15.00f, 10.00f, 10.00f,
             9.40f, CC_LOCAL_BUILDING_CIVIC, true},
            {"Goldsmith court", 31.00f, 14.00f, 8.00f, 10.00f,
             8.60f, CC_LOCAL_BUILDING_WORKSHOP, true},
            {"Royal exchange", 44.00f, 16.00f, 12.00f, 10.00f,
             11.20f, CC_LOCAL_BUILDING_CIVIC, true},
            {"Petitioners' hall", 19.00f, 34.00f, 12.00f, 8.00f,
             8.80f, CC_LOCAL_BUILDING_CIVIC, true},
            {"Guild registry", 31.00f, 35.00f, 4.50f, 8.00f,
             8.20f, CC_LOCAL_BUILDING_CIVIC, true},
            {"Ambassadors' house", 55.00f, 42.00f, 9.50f, 7.00f,
             9.10f, CC_LOCAL_BUILDING_CIVIC, true},
            {"Court kitchens", 57.00f, 25.00f, 7.00f, 5.00f,
             6.80f, CC_LOCAL_BUILDING_WORKSHOP, true},
            {"Noble lodging", 57.00f, 14.00f, 8.00f, 10.00f,
             9.60f, CC_LOCAL_BUILDING_DOMESTIC, true},
            {"Scribes' terrace", 17.00f, 47.00f, 8.50f, 7.00f,
             8.00f, CC_LOCAL_BUILDING_WORKER_ROW, true},
            {"East guildhall", 50.00f, 58.00f, 10.50f, 7.50f,
             9.20f, CC_LOCAL_BUILDING_CIVIC, true},
            {"Rose ward lodging", 83.00f, 54.00f, 6.00f, 9.00f,
             8.40f, CC_LOCAL_BUILDING_DOMESTIC, true},
            {"Procession offices", 82.50f, 42.00f, 8.00f, 7.00f,
             8.80f, CC_LOCAL_BUILDING_CIVIC, true},
        },
        .compound_structure = {
            {CC_LOCAL_COMPOUND_WALL, 65.50f, 8.50f, 1.00f, 23.00f, 5.80f},
            {CC_LOCAL_COMPOUND_WALL, 90.50f, 8.50f, 1.00f, 23.00f, 5.80f},
            {CC_LOCAL_COMPOUND_WALL, 65.50f, 8.50f, 26.00f, 1.00f, 5.80f},
            {CC_LOCAL_COMPOUND_WALL, 65.50f, 30.50f, 10.00f, 1.00f, 5.80f},
            {CC_LOCAL_COMPOUND_WALL, 81.50f, 30.50f, 10.00f, 1.00f, 5.80f},
            {CC_LOCAL_COMPOUND_HALL, 69.50f, 12.00f, 17.50f, 10.50f, 14.20f},
            {CC_LOCAL_COMPOUND_TOWER, 72.80f, 27.50f, 2.40f, 3.00f, 10.50f},
            {CC_LOCAL_COMPOUND_TOWER, 81.80f, 27.50f, 2.40f, 3.00f, 10.50f},
            {CC_LOCAL_COMPOUND_TOWER, 64.50f, 7.50f, 3.20f, 3.20f, 12.50f},
            {CC_LOCAL_COMPOUND_TOWER, 89.30f, 7.50f, 3.20f, 3.20f, 12.50f},
            {CC_LOCAL_COMPOUND_STOREHOUSE, 69.50f, 24.00f,
             6.00f, 4.50f, 6.20f},
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
        .map_form = "A loose lantern ward facing the sealed road",
        .scene = {
            {CC_LOCAL_TOWN_SCENE_ARRIVAL, "LANTERN ROAD",
             82.0f, 34.0f, 81.0f, 1.8f, 34.0f,
             -23.0f, 13.0f, 36.0f, 28.0f},
            {CC_LOCAL_TOWN_SCENE_HEART, "EXPEDITION HOLLOW",
             44.0f, 29.0f, 46.0f, 1.5f, 30.0f,
             28.0f, 14.0f, 34.0f, 28.0f},
            {CC_LOCAL_TOWN_SCENE_LANDMARK, "SEALED DUNGEON ROAD",
             78.0f, 19.0f, 79.0f, 4.5f, 18.0f,
             -27.0f, 20.0f, 40.0f, 33.0f},
            {CC_LOCAL_TOWN_SCENE_CLOSE_FIRST, "SALVAGE ROW",
             34.0f, 38.0f, 33.0f, 1.8f, 38.5f,
             13.0f, 6.5f, 17.0f, 17.5f},
            {CC_LOCAL_TOWN_SCENE_CLOSE_SECOND, "LANTERN GATE",
             29.0f, 51.8f, 28.5f, 2.0f, 49.5f,
             -13.0f, 6.5f, 17.0f, 17.5f},
        },
        .keeper_seed = UINT32_C(0x0b3e7006),
        .terrain_salt = UINT32_C(0x9e37b97d),
        .feature_mask = CC_LOCAL_PLACE_INDUSTRY |
                        CC_LOCAL_PLACE_CARRIAGE |
                        CC_LOCAL_PLACE_DUNGEON,
        .building_count = 7,
        .primary_building = 2,
        .compound_structure_count = 9,
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
        .carriage_route = {
            {"Ward gate ramp", 75.80f, 27.00f, 5.20f, 11.50f, false,
             CC_LOCAL_ROAD_EXPEDITION},
            {"Lantern road", 39.00f, 33.20f, 57.00f, 5.20f, true,
             CC_LOCAL_ROAD_EXPEDITION},
            {"Expedition spine", 39.00f, 33.20f, 5.80f, 24.80f, false,
             CC_LOCAL_ROAD_EXPEDITION},
            {"Warden turn", 35.80f, 30.00f, 12.00f, 12.00f, true,
             CC_LOCAL_ROAD_EXPEDITION},
            {"Salvage wagon yard", 35.20f, 44.00f, 13.40f, 14.00f, true,
             CC_LOCAL_ROAD_EXPEDITION},
        },
        .building = {
            {"Delvers' lodging", 19.50f, 15.00f, 9.50f, 10.00f,
             6.80f, CC_LOCAL_BUILDING_WORKER_ROW, true},
            {"Lamp house", 32.00f, 15.00f, 6.50f, 8.50f,
             6.20f, CC_LOCAL_BUILDING_WORKSHOP, true},
            {"Supply house", 44.00f, 16.00f, 12.00f, 10.00f,
             8.40f, CC_LOCAL_BUILDING_WORKSHOP, true},
            {"Salvage hall", 20.00f, 34.00f, 11.00f, 7.50f,
             6.60f, CC_LOCAL_BUILDING_WORKSHOP, true},
            {"Chirurgeon's rooms", 31.00f, 35.00f, 4.50f, 7.50f,
             5.80f, CC_LOCAL_BUILDING_DOMESTIC, true},
            {"Warden bunkhouse", 57.00f, 14.00f, 8.00f, 9.50f,
             7.00f, CC_LOCAL_BUILDING_WORKER_ROW, true},
            {"Returned goods shed", 50.00f, 58.00f, 10.00f, 7.00f,
             5.80f, CC_LOCAL_BUILDING_WORKSHOP, true},
        },
        .compound_structure = {
            {CC_LOCAL_COMPOUND_WALL, 67.00f, 10.00f, 0.80f, 20.50f, 4.60f},
            {CC_LOCAL_COMPOUND_WALL, 89.50f, 10.00f, 0.80f, 20.50f, 4.60f},
            {CC_LOCAL_COMPOUND_WALL, 67.00f, 10.00f, 23.30f, 0.80f, 4.60f},
            {CC_LOCAL_COMPOUND_WALL, 67.00f, 29.70f, 8.80f, 0.80f, 4.60f},
            {CC_LOCAL_COMPOUND_WALL, 81.00f, 29.70f, 9.30f, 0.80f, 4.60f},
            {CC_LOCAL_COMPOUND_HALL, 71.00f, 13.00f, 13.00f, 8.50f, 8.80f},
            {CC_LOCAL_COMPOUND_TOWER, 85.50f, 13.00f, 3.20f, 3.20f, 10.80f},
            {CC_LOCAL_COMPOUND_STOREHOUSE, 69.50f, 23.00f,
             6.00f, 4.80f, 5.40f},
            {CC_LOCAL_COMPOUND_TOWER, 85.00f, 23.00f, 3.50f, 4.00f, 8.60f},
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

const CcLocalPlaceRoad *CcLocalPlaceCarriageRouteAt(
    CcSettlementFunction function, int32_t route_index)
{
    const CcLocalPlaceProfile *profile =
        CcLocalPlaceProfileForFunction(function);
    if (route_index < 0 || route_index >= CC_LOCAL_CARRIAGE_ROUTE_COUNT) {
        return NULL;
    }
    return &profile->carriage_route[route_index];
}

const CcLocalPlaceBuilding *CcLocalPlaceBuildingAt(
    CcSettlementFunction function, int32_t building_index)
{
    const CcLocalPlaceProfile *profile =
        CcLocalPlaceProfileForFunction(function);
    if (building_index < 0 || building_index >= profile->building_count) {
        return NULL;
    }
    return &profile->building[building_index];
}

const CcLocalPlaceCompoundStructure *CcLocalPlaceCompoundStructureAt(
    CcSettlementFunction function, int32_t structure_index)
{
    const CcLocalPlaceProfile *profile =
        CcLocalPlaceProfileForFunction(function);
    if (structure_index < 0 ||
        structure_index >= profile->compound_structure_count) {
        return NULL;
    }
    return &profile->compound_structure[structure_index];
}

const CcLocalTownScene *CcLocalTownSceneAt(
    CcSettlementFunction function, int32_t scene_index)
{
    const CcLocalPlaceProfile *profile =
        CcLocalPlaceProfileForFunction(function);
    if (scene_index < 0 || scene_index >= CC_LOCAL_PLACE_SCENE_COUNT) {
        return NULL;
    }
    return &profile->scene[scene_index];
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
