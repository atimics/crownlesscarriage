#include "client/cc_local_place.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        (void)fprintf(stderr, "check failed at line %d: %s\n", \
                      __LINE__, #condition); \
        return 1; \
    } \
} while (0)

static int ProfileContract(void)
{
    for (int32_t function = CC_SETTLEMENT_FARMING;
         function <= CC_SETTLEMENT_DUNGEON_TOWN; ++function) {
        const CcLocalPlaceProfile *profile =
            CcLocalPlaceProfileForFunction((CcSettlementFunction)function);
        CHECK(profile != NULL);
        CHECK(profile->function == (CcSettlementFunction)function);
        CHECK(profile->identity != NULL && profile->identity[0] != '\0');
        CHECK(profile->purpose != NULL && profile->purpose[0] != '\0');
        CHECK(profile->primary_hall != NULL && profile->primary_hall[0] != '\0');
        CHECK(profile->notice_board != NULL && profile->notice_board[0] != '\0');
        CHECK(profile->compound != NULL && profile->compound[0] != '\0');
        CHECK(profile->keeper_name != NULL && profile->keeper_name[0] != '\0');
        CHECK(profile->interior_service != NULL &&
              profile->interior_service[0] != '\0');
        CHECK(profile->keeper_seed != 0U);
        CHECK(profile->feature_mask != 0U);
        for (int32_t room = 0; room < CC_LOCAL_PLACE_ROOM_COUNT; ++room) {
            const char *name = CcLocalPlaceRoomName(
                (CcSettlementFunction)function, room);
            CHECK(name != NULL && name[0] != '\0');
        }
        for (int32_t landmark = 0;
             landmark < CC_LOCAL_PLACE_LANDMARK_COUNT; ++landmark) {
            const CcLocalPlaceLandmark *structure =
                CcLocalPlaceLandmarkAt(
                    (CcSettlementFunction)function, landmark);
            CHECK(structure != NULL);
            CHECK(structure->name != NULL && structure->name[0] != '\0');
            CHECK(structure->width >= 3.0f);
            CHECK(structure->depth >= 3.0f);
            CHECK(structure->height >= 3.0f);
            CHECK(structure->x >= 0.0f &&
                  structure->x + structure->width <= 96.0f);
            CHECK(structure->z >= 0.0f &&
                  structure->z + structure->depth <= 72.0f);
        }
        for (int32_t road = 0;
             road < CC_LOCAL_PLACE_ROAD_COUNT; ++road) {
            const CcLocalPlaceRoad *route = CcLocalPlaceRoadAt(
                (CcSettlementFunction)function, road);
            CHECK(route != NULL);
            CHECK(route->name != NULL && route->name[0] != '\0');
            CHECK(route->surface == (CcLocalRoadSurface)function);
            CHECK(route->width >= 2.5f);
            CHECK(route->depth >= 2.5f);
            CHECK(route->x >= 0.0f && route->x + route->width <= 96.0f);
            CHECK(route->z >= 0.0f && route->z + route->depth <= 72.0f);
        }
    }
    CHECK(CcLocalPlaceRoomName(CC_SETTLEMENT_MARKET, -1) == NULL);
    CHECK(CcLocalPlaceRoomName(CC_SETTLEMENT_MARKET,
                               CC_LOCAL_PLACE_ROOM_COUNT) == NULL);
    CHECK(CcLocalPlaceLandmarkAt(CC_SETTLEMENT_MARKET, -1) == NULL);
    CHECK(CcLocalPlaceLandmarkAt(
              CC_SETTLEMENT_MARKET,
              CC_LOCAL_PLACE_LANDMARK_COUNT) == NULL);
    CHECK(CcLocalPlaceRoadAt(CC_SETTLEMENT_MARKET, -1) == NULL);
    CHECK(CcLocalPlaceRoadAt(
              CC_SETTLEMENT_MARKET, CC_LOCAL_PLACE_ROAD_COUNT) == NULL);
    return 0;
}

static bool LandmarksOverlap(const CcLocalPlaceLandmark *left,
                             const CcLocalPlaceLandmark *right)
{
    return left->x < right->x + right->width &&
           left->x + left->width > right->x &&
           left->z < right->z + right->depth &&
           left->z + left->depth > right->z;
}

static bool SameLandmarkGeometry(const CcLocalPlaceLandmark *left,
                                 const CcLocalPlaceLandmark *right)
{
    return left->x == right->x && left->z == right->z &&
           left->width == right->width && left->depth == right->depth &&
           left->height == right->height;
}

static bool RoadTouchesLandmark(const CcLocalPlaceRoad *road,
                                const CcLocalPlaceLandmark *landmark)
{
    return road->x < landmark->x + landmark->width &&
           road->x + road->width > landmark->x &&
           road->z < landmark->z + landmark->depth &&
           road->z + road->depth > landmark->z;
}

static bool SameRoadGeometry(const CcLocalPlaceRoad *left,
                             const CcLocalPlaceRoad *right)
{
    return left->x == right->x && left->z == right->z &&
           left->width == right->width && left->depth == right->depth &&
           left->runs_east_west == right->runs_east_west;
}

static bool RoadsOverlap(const CcLocalPlaceRoad *left,
                         const CcLocalPlaceRoad *right)
{
    return left->x < right->x + right->width &&
           left->x + left->width > right->x &&
           left->z < right->z + right->depth &&
           left->z + left->depth > right->z;
}

static bool RoadTouchesSharedSpine(const CcLocalPlaceRoad *road)
{
    static const CcLocalPlaceRoad spine[] = {
        {"Market road", 14.70f, 26.80f, 77.60f, 5.20f, true,
         CC_LOCAL_ROAD_TRADE},
        {"Wayfarer road", 7.20f, 10.00f, 34.80f, 3.80f, true,
         CC_LOCAL_ROAD_TRADE},
        {"North road", 14.00f, 54.20f, 28.00f, 2.70f, true,
         CC_LOCAL_ROAD_TRADE},
        {"East road", 76.00f, 27.00f, 5.00f, 27.00f, false,
         CC_LOCAL_ROAD_TRADE},
        {"Miller's road", 54.20f, 50.10f, 18.70f, 2.90f, true,
         CC_LOCAL_ROAD_TRADE},
    };
    for (int32_t i = 0;
         i < (int32_t)(sizeof(spine) / sizeof(spine[0])); ++i) {
        if (RoadsOverlap(road, &spine[i])) return true;
    }
    return false;
}

static int AuthoredLandmarkLayouts(void)
{
    for (int32_t function = CC_SETTLEMENT_FARMING;
         function <= CC_SETTLEMENT_DUNGEON_TOWN; ++function) {
        const CcLocalPlaceProfile *profile =
            CcLocalPlaceProfileForFunction((CcSettlementFunction)function);
        CHECK(profile->landmark[0].family ==
              (CcLocalLandmarkFamily)function);
        for (int32_t left = 0;
             left < CC_LOCAL_PLACE_LANDMARK_COUNT; ++left) {
            for (int32_t right = left + 1;
                 right < CC_LOCAL_PLACE_LANDMARK_COUNT; ++right) {
                CHECK(!LandmarksOverlap(&profile->landmark[left],
                                        &profile->landmark[right]));
            }
        }
        for (int32_t landmark = 0;
             landmark < CC_LOCAL_PLACE_LANDMARK_COUNT; ++landmark) {
            bool connected = false;
            for (int32_t road = 0;
                 road < CC_LOCAL_PLACE_ROAD_COUNT; ++road) {
                connected = connected || RoadTouchesLandmark(
                    &profile->road[road], &profile->landmark[landmark]);
            }
            CHECK(connected);
        }
        for (int32_t road = 0;
             road < CC_LOCAL_PLACE_ROAD_COUNT; ++road) {
            CHECK(RoadTouchesSharedSpine(&profile->road[road]));
            for (int32_t previous_road = 0;
                 previous_road < road; ++previous_road) {
                CHECK(strcmp(profile->road[road].name,
                             profile->road[previous_road].name) != 0);
            }
        }
        for (int32_t previous = CC_SETTLEMENT_FARMING;
             previous < function; ++previous) {
            const CcLocalPlaceProfile *other =
                CcLocalPlaceProfileForFunction(
                    (CcSettlementFunction)previous);
            bool same_layout = true;
            bool same_roads = true;
            for (int32_t landmark = 0;
                 landmark < CC_LOCAL_PLACE_LANDMARK_COUNT; ++landmark) {
                if (!SameLandmarkGeometry(&profile->landmark[landmark],
                                          &other->landmark[landmark])) {
                    same_layout = false;
                    break;
                }
            }
            for (int32_t road = 0;
                 road < CC_LOCAL_PLACE_ROAD_COUNT; ++road) {
                if (!SameRoadGeometry(&profile->road[road],
                                      &other->road[road])) {
                    same_roads = false;
                    break;
                }
            }
            CHECK(!same_layout);
            CHECK(!same_roads);
            CHECK(strcmp(profile->keeper_name, other->keeper_name) != 0);
            CHECK(strcmp(profile->interior_service,
                         other->interior_service) != 0);
            CHECK(profile->keeper_seed != other->keeper_seed);
        }
    }
    return 0;
}

static int CanonicalRegionProfiles(void)
{
    CcSim sim;
    CcSimInit(&sim, UINT32_C(0xc0a71a9e));
    CHECK(strcmp(CcLocalPlaceProfileForSettlement(&sim.settlements[0])->identity,
                 "THE GRANARY COUNTRY") == 0);
    CHECK(strcmp(CcLocalPlaceProfileForSettlement(&sim.settlements[1])->identity,
                 "THE CROSSROADS MARKET") == 0);
    CHECK(strcmp(CcLocalPlaceProfileForSettlement(&sim.settlements[2])->identity,
                 "THE CONTESTED BRIDGE") == 0);
    CHECK(strcmp(CcLocalPlaceProfileForSettlement(&sim.settlements[3])->identity,
                 "THE WORKING DEEPS") == 0);
    CHECK(CcLocalPlaceHasFeature(
        CcLocalPlaceProfileForSettlement(&sim.settlements[0]),
        CC_LOCAL_PLACE_FARMLAND));
    CHECK(CcLocalPlaceHasFeature(
        CcLocalPlaceProfileForSettlement(&sim.settlements[2]),
        CC_LOCAL_PLACE_FORTIFICATION));
    CHECK(CcLocalPlaceHasFeature(
        CcLocalPlaceProfileForSettlement(&sim.settlements[3]),
        CC_LOCAL_PLACE_DUNGEON));
    return 0;
}

static int StableDistinctTerrain(void)
{
    CcSim sim;
    CcSimInit(&sim, UINT32_C(0x12345678));
    uint32_t seeds[CC_MAX_SETTLEMENTS] = {0};
    for (int32_t i = 0; i < sim.settlement_count; ++i) {
        seeds[i] = CcLocalPlaceTerrainSeed(sim.world_seed,
                                           &sim.settlements[i]);
        CHECK(seeds[i] != 0U);
        CHECK(seeds[i] == CcLocalPlaceTerrainSeed(sim.world_seed,
                                                  &sim.settlements[i]));
        for (int32_t previous = 0; previous < i; ++previous) {
            CHECK(seeds[i] != seeds[previous]);
        }
    }
    CcSim same;
    CcSimInit(&same, UINT32_C(0x12345678));
    CHECK(CcLocalPlaceTerrainSeed(same.world_seed, &same.settlements[3]) ==
          seeds[3]);
    CcSim other;
    CcSimInit(&other, UINT32_C(0x12345679));
    CHECK(CcLocalPlaceTerrainSeed(other.world_seed, &other.settlements[3]) !=
          seeds[3]);
    return 0;
}

int main(void)
{
    if (ProfileContract() != 0) return 1;
    if (AuthoredLandmarkLayouts() != 0) return 1;
    if (CanonicalRegionProfiles() != 0) return 1;
    if (StableDistinctTerrain() != 0) return 1;
    (void)puts("local place profile tests passed");
    return 0;
}
