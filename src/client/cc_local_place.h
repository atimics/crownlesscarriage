#ifndef CROWNLESS_LOCAL_PLACE_H
#define CROWNLESS_LOCAL_PLACE_H

#include "sim/cc_sim.h"

#include <stdbool.h>
#include <stdint.h>

#define CC_LOCAL_PLACE_ROOM_COUNT 10
#define CC_LOCAL_PLACE_LANDMARK_COUNT 3

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

typedef struct CcLocalPlaceProfile {
    CcSettlementFunction function;
    const char *identity;
    const char *purpose;
    const char *primary_hall;
    const char *notice_board;
    const char *training_yard;
    const char *compound;
    uint32_t terrain_salt;
    uint32_t feature_mask;
    const char *room_name[CC_LOCAL_PLACE_ROOM_COUNT];
    CcLocalPlaceLandmark landmark[CC_LOCAL_PLACE_LANDMARK_COUNT];
} CcLocalPlaceProfile;

const CcLocalPlaceProfile *CcLocalPlaceProfileForFunction(
    CcSettlementFunction function);
const CcLocalPlaceProfile *CcLocalPlaceProfileForSettlement(
    const CcSettlement *settlement);
const char *CcLocalPlaceRoomName(CcSettlementFunction function,
                                 int32_t room_index);
const CcLocalPlaceLandmark *CcLocalPlaceLandmarkAt(
    CcSettlementFunction function, int32_t landmark_index);
bool CcLocalPlaceHasFeature(const CcLocalPlaceProfile *profile,
                            CcLocalPlaceFeature feature);
uint32_t CcLocalPlaceTerrainSeed(uint32_t world_seed,
                                 const CcSettlement *settlement);

#endif
