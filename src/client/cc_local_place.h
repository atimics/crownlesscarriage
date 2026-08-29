#ifndef CROWNLESS_LOCAL_PLACE_H
#define CROWNLESS_LOCAL_PLACE_H

#include "sim/cc_sim.h"

#include <stdbool.h>
#include <stdint.h>

#define CC_LOCAL_PLACE_ROOM_COUNT 10
#define CC_LOCAL_PLACE_LANDMARK_COUNT 3
#define CC_LOCAL_PLACE_ROAD_COUNT 3
#define CC_LOCAL_PLACE_BUILDING_CAPACITY 12
#define CC_LOCAL_PLACE_COMPOUND_CAPACITY 12
#define CC_LOCAL_PLACE_SCENE_COUNT 3

typedef enum CcLocalPlaceFeature {
    CC_LOCAL_PLACE_FARMLAND = UINT32_C(1) << 0,
    CC_LOCAL_PLACE_MARKET = UINT32_C(1) << 1,
    CC_LOCAL_PLACE_FORTIFICATION = UINT32_C(1) << 2,
    CC_LOCAL_PLACE_INDUSTRY = UINT32_C(1) << 3,
    CC_LOCAL_PLACE_CARRIAGE = UINT32_C(1) << 4,
    CC_LOCAL_PLACE_DUNGEON = UINT32_C(1) << 5,
    CC_LOCAL_PLACE_COURT = UINT32_C(1) << 6
} CcLocalPlaceFeature;

typedef enum CcLocalLandmarkFamily {
    CC_LOCAL_LANDMARK_AGRICULTURE = 0,
    CC_LOCAL_LANDMARK_INDUSTRY,
    CC_LOCAL_LANDMARK_COMMERCE,
    CC_LOCAL_LANDMARK_MILITARY,
    CC_LOCAL_LANDMARK_CIVIC,
    CC_LOCAL_LANDMARK_EXPEDITION
} CcLocalLandmarkFamily;

typedef struct CcLocalPlaceLandmark {
    CcLocalLandmarkFamily family;
    int32_t variant;
    const char *name;
    float x;
    float z;
    float width;
    float depth;
    float height;
} CcLocalPlaceLandmark;

typedef enum CcLocalRoadSurface {
    CC_LOCAL_ROAD_FARM_TRACK = 0,
    CC_LOCAL_ROAD_INDUSTRIAL,
    CC_LOCAL_ROAD_TRADE,
    CC_LOCAL_ROAD_MILITARY,
    CC_LOCAL_ROAD_PROCESSIONAL,
    CC_LOCAL_ROAD_EXPEDITION
} CcLocalRoadSurface;

typedef struct CcLocalPlaceRoad {
    const char *name;
    float x;
    float z;
    float width;
    float depth;
    bool runs_east_west;
    CcLocalRoadSurface surface;
} CcLocalPlaceRoad;

typedef enum CcLocalBuildingStyle {
    CC_LOCAL_BUILDING_DOMESTIC = 0,
    CC_LOCAL_BUILDING_WORKSHOP,
    CC_LOCAL_BUILDING_CIVIC,
    CC_LOCAL_BUILDING_WORKER_ROW
} CcLocalBuildingStyle;

typedef struct CcLocalPlaceBuilding {
    const char *name;
    float x;
    float z;
    float width;
    float depth;
    float height;
    CcLocalBuildingStyle style;
    bool door;
} CcLocalPlaceBuilding;

typedef enum CcLocalCompoundKind {
    CC_LOCAL_COMPOUND_WALL = 0,
    CC_LOCAL_COMPOUND_HALL,
    CC_LOCAL_COMPOUND_TOWER,
    CC_LOCAL_COMPOUND_STOREHOUSE,
    CC_LOCAL_COMPOUND_SILO
} CcLocalCompoundKind;

typedef struct CcLocalPlaceCompoundStructure {
    CcLocalCompoundKind kind;
    float x;
    float z;
    float width;
    float depth;
    float height;
} CcLocalPlaceCompoundStructure;

typedef enum CcLocalTownSceneKind {
    CC_LOCAL_TOWN_SCENE_ARRIVAL = 0,
    CC_LOCAL_TOWN_SCENE_HEART,
    CC_LOCAL_TOWN_SCENE_LANDMARK
} CcLocalTownSceneKind;

typedef struct CcLocalTownScene {
    CcLocalTownSceneKind kind;
    const char *name;
    float trigger_x;
    float trigger_z;
    float target_x;
    float target_y;
    float target_z;
    float camera_offset_x;
    float camera_offset_y;
    float camera_offset_z;
    float fovy;
} CcLocalTownScene;

typedef struct CcLocalPlaceProfile {
    CcSettlementFunction function;
    const char *identity;
    const char *purpose;
    const char *primary_hall;
    const char *notice_board;
    const char *training_yard;
    const char *compound;
    const char *keeper_name;
    const char *interior_service;
    const char *map_form;
    uint32_t keeper_seed;
    uint32_t terrain_salt;
    uint32_t feature_mask;
    int32_t building_count;
    int32_t primary_building;
    int32_t compound_structure_count;
    const char *room_name[CC_LOCAL_PLACE_ROOM_COUNT];
    CcLocalPlaceLandmark landmark[CC_LOCAL_PLACE_LANDMARK_COUNT];
    CcLocalPlaceRoad road[CC_LOCAL_PLACE_ROAD_COUNT];
    CcLocalPlaceBuilding building[CC_LOCAL_PLACE_BUILDING_CAPACITY];
    CcLocalPlaceCompoundStructure
        compound_structure[CC_LOCAL_PLACE_COMPOUND_CAPACITY];
    CcLocalTownScene scene[CC_LOCAL_PLACE_SCENE_COUNT];
} CcLocalPlaceProfile;

const CcLocalPlaceProfile *CcLocalPlaceProfileForFunction(
    CcSettlementFunction function);
const CcLocalPlaceProfile *CcLocalPlaceProfileForSettlement(
    const CcSettlement *settlement);
const char *CcLocalPlaceRoomName(CcSettlementFunction function,
                                 int32_t room_index);
const CcLocalPlaceLandmark *CcLocalPlaceLandmarkAt(
    CcSettlementFunction function, int32_t landmark_index);
const CcLocalPlaceRoad *CcLocalPlaceRoadAt(
    CcSettlementFunction function, int32_t road_index);
const CcLocalPlaceBuilding *CcLocalPlaceBuildingAt(
    CcSettlementFunction function, int32_t building_index);
const CcLocalPlaceCompoundStructure *CcLocalPlaceCompoundStructureAt(
    CcSettlementFunction function, int32_t structure_index);
const CcLocalTownScene *CcLocalTownSceneAt(
    CcSettlementFunction function, int32_t scene_index);
bool CcLocalPlaceHasFeature(const CcLocalPlaceProfile *profile,
                            CcLocalPlaceFeature feature);
uint32_t CcLocalPlaceTerrainSeed(uint32_t world_seed,
                                 const CcSettlement *settlement);

#endif
