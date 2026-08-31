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
        CHECK(profile->map_form != NULL && profile->map_form[0] != '\0');
        CHECK(profile->keeper_seed != 0U);
        CHECK(profile->feature_mask != 0U);
        CHECK(profile->building_count >= 7);
        CHECK(profile->building_count <= CC_LOCAL_PLACE_BUILDING_CAPACITY);
        CHECK(profile->primary_building >= 0);
        CHECK(profile->primary_building < profile->building_count);
        CHECK(profile->compound_structure_count > 5);
        CHECK(profile->compound_structure_count <=
              CC_LOCAL_PLACE_COMPOUND_CAPACITY);
        CHECK(profile->compound_structure[3].kind ==
              CC_LOCAL_COMPOUND_WALL);
        CHECK(profile->compound_structure[5].kind ==
              CC_LOCAL_COMPOUND_HALL);
        for (int32_t scene = 0;
             scene < CC_LOCAL_PLACE_SCENE_COUNT; ++scene) {
            const CcLocalTownScene *camera = CcLocalTownSceneAt(
                (CcSettlementFunction)function, scene);
            CHECK(camera != NULL);
            CHECK(camera->kind == (CcLocalTownSceneKind)scene);
            CHECK(camera->name != NULL && camera->name[0] != '\0');
            CHECK(camera->trigger_x >= 0.0f && camera->trigger_x <= 96.0f);
            CHECK(camera->trigger_z >= 0.0f && camera->trigger_z <= 72.0f);
            CHECK(camera->target_x >= 0.0f && camera->target_x <= 96.0f);
            CHECK(camera->target_z >= 0.0f && camera->target_z <= 72.0f);
            /* Walking shots are deliberately human-scale. The authored
               target sits near the ground and the physical lens stays below
               an adult character's waist, replacing the old aerial map
               camera while preserving a slightly wider landmark page. */
            CHECK(camera->target_y >= 0.08f);
            CHECK(camera->target_y <= 0.25f);
            CHECK(camera->camera_offset_y >= 0.55f);
            CHECK(camera->camera_offset_y <= 0.70f);
            float camera_distance_squared =
                camera->camera_offset_x * camera->camera_offset_x +
                camera->camera_offset_z * camera->camera_offset_z;
            CHECK(camera_distance_squared >= 5.0f * 5.0f);
            CHECK(camera_distance_squared <= 24.0f * 24.0f);
            CHECK(camera->fovy >= 5.8f && camera->fovy <= 10.0f);
            if (camera->kind == CC_LOCAL_TOWN_SCENE_LANDMARK) {
                CHECK(camera->fovy >= 9.0f);
            } else if (camera->kind >= CC_LOCAL_TOWN_SCENE_CLOSE_FIRST) {
                CHECK(camera->fovy <= 6.6f);
            }
            if (camera->kind == CC_LOCAL_TOWN_SCENE_CARRIAGE_YARD) {
                CHECK(camera->trigger_x >= 42.0f && camera->trigger_x <= 43.0f);
                CHECK(camera->trigger_z >= 54.5f && camera->trigger_z <= 56.0f);
                CHECK(camera->target_x >= 38.0f && camera->target_x <= 41.0f);
                CHECK(camera->target_z >= 51.0f && camera->target_z <= 53.0f);
            }
            for (int32_t previous = 0; previous < scene; ++previous) {
                CHECK(strcmp(camera->name,
                             profile->scene[previous].name) != 0);
            }
        }
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
        for (int32_t road = 0;
             road < CC_LOCAL_CARRIAGE_ROUTE_COUNT; ++road) {
            const CcLocalPlaceRoad *route = CcLocalPlaceCarriageRouteAt(
                (CcSettlementFunction)function, road);
            CHECK(route != NULL);
            CHECK(route->name != NULL && route->name[0] != '\0');
            CHECK(route->surface == (CcLocalRoadSurface)function);
            CHECK(route->width >= 5.2f);
            CHECK(route->depth >= 5.2f);
            CHECK(route->x >= 0.0f && route->x + route->width <= 96.0f);
            CHECK(route->z >= 0.0f && route->z + route->depth <= 72.0f);
        }
        for (int32_t building = 0;
             building < profile->building_count; ++building) {
            const CcLocalPlaceBuilding *structure =
                CcLocalPlaceBuildingAt(
                    (CcSettlementFunction)function, building);
            CHECK(structure != NULL);
            CHECK(structure->name != NULL && structure->name[0] != '\0');
            CHECK(structure->width >= 4.5f);
            CHECK(structure->depth >= 4.5f);
            CHECK(structure->height >= 4.5f);
            CHECK(structure->style >= CC_LOCAL_BUILDING_DOMESTIC);
            CHECK(structure->style <= CC_LOCAL_BUILDING_WORKER_ROW);
            CHECK(structure->x >= 0.0f &&
                  structure->x + structure->width <= 96.0f);
            CHECK(structure->z >= 0.0f &&
                  structure->z + structure->depth <= 72.0f);
        }
        for (int32_t structure = 0;
             structure < profile->compound_structure_count; ++structure) {
            const CcLocalPlaceCompoundStructure *compound =
                CcLocalPlaceCompoundStructureAt(
                    (CcSettlementFunction)function, structure);
            CHECK(compound != NULL);
            CHECK(compound->kind >= CC_LOCAL_COMPOUND_WALL);
            CHECK(compound->kind <= CC_LOCAL_COMPOUND_SILO);
            CHECK(compound->width >= 0.7f);
            CHECK(compound->depth >= 0.7f);
            CHECK(compound->height >= 2.0f);
            CHECK(compound->x >= 0.0f &&
                  compound->x + compound->width <= 96.0f);
            CHECK(compound->z >= 0.0f &&
                  compound->z + compound->depth <= 72.0f);
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
    CHECK(CcLocalPlaceCarriageRouteAt(CC_SETTLEMENT_MARKET, -1) == NULL);
    CHECK(CcLocalPlaceCarriageRouteAt(
              CC_SETTLEMENT_MARKET,
              CC_LOCAL_CARRIAGE_ROUTE_COUNT) == NULL);
    CHECK(CcLocalPlaceBuildingAt(CC_SETTLEMENT_MARKET, -1) == NULL);
    CHECK(CcLocalPlaceBuildingAt(
              CC_SETTLEMENT_MARKET,
              CC_LOCAL_PLACE_BUILDING_CAPACITY) == NULL);
    CHECK(CcLocalPlaceCompoundStructureAt(
              CC_SETTLEMENT_MARKET, -1) == NULL);
    CHECK(CcLocalPlaceCompoundStructureAt(
              CC_SETTLEMENT_MARKET,
              CC_LOCAL_PLACE_COMPOUND_CAPACITY) == NULL);
    CHECK(CcLocalTownSceneAt(CC_SETTLEMENT_MARKET, -1) == NULL);
    CHECK(CcLocalTownSceneAt(
              CC_SETTLEMENT_MARKET, CC_LOCAL_PLACE_SCENE_COUNT) == NULL);
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

static bool BuildingsOverlap(const CcLocalPlaceBuilding *left,
                             const CcLocalPlaceBuilding *right)
{
    return left->x < right->x + right->width &&
           left->x + left->width > right->x &&
           left->z < right->z + right->depth &&
           left->z + left->depth > right->z;
}

static bool SameBuildingGeometry(const CcLocalPlaceBuilding *left,
                                 const CcLocalPlaceBuilding *right)
{
    return left->x == right->x && left->z == right->z &&
           left->width == right->width && left->depth == right->depth &&
           left->height == right->height && left->style == right->style;
}

static bool BuildingTouchesLandmark(
    const CcLocalPlaceBuilding *building,
    const CcLocalPlaceLandmark *landmark)
{
    return building->x < landmark->x + landmark->width &&
           building->x + building->width > landmark->x &&
           building->z < landmark->z + landmark->depth &&
           building->z + building->depth > landmark->z;
}

static bool BuildingTouchesCompound(
    const CcLocalPlaceBuilding *building,
    const CcLocalPlaceCompoundStructure *structure)
{
    return building->x < structure->x + structure->width &&
           building->x + building->width > structure->x &&
           building->z < structure->z + structure->depth &&
           building->z + building->depth > structure->z;
}

static bool CompoundTouchesGateLane(
    const CcLocalPlaceCompoundStructure *structure)
{
    const float gate_left = 75.80f;
    const float gate_right = 81.00f;
    const float gate_near = 27.00f;
    const float gate_far = 38.50f;
    return structure->x < gate_right &&
           structure->x + structure->width > gate_left &&
           structure->z < gate_far &&
           structure->z + structure->depth > gate_near;
}

static bool RoadsOverlap(const CcLocalPlaceRoad *left,
                         const CcLocalPlaceRoad *right)
{
    return left->x < right->x + right->width &&
           left->x + left->width > right->x &&
           left->z < right->z + right->depth &&
           left->z + left->depth > right->z;
}

static bool RoadTouchesBuilding(const CcLocalPlaceRoad *road,
                                const CcLocalPlaceBuilding *building)
{
    return road->x < building->x + building->width &&
           road->x + road->width > building->x &&
           road->z < building->z + building->depth &&
           road->z + road->depth > building->z;
}

static bool RoadTouchesCompound(
    const CcLocalPlaceRoad *road,
    const CcLocalPlaceCompoundStructure *structure)
{
    return road->x < structure->x + structure->width &&
           road->x + road->width > structure->x &&
           road->z < structure->z + structure->depth &&
           road->z + road->depth > structure->z;
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

static int AuthoredTownMaps(void)
{
    for (int32_t function = CC_SETTLEMENT_FARMING;
         function <= CC_SETTLEMENT_DUNGEON_TOWN; ++function) {
        const CcLocalPlaceProfile *profile =
            CcLocalPlaceProfileForFunction((CcSettlementFunction)function);
        const CcLocalPlaceBuilding *primary = CcLocalPlaceBuildingAt(
            (CcSettlementFunction)function, profile->primary_building);
        CHECK(primary != NULL);
        CHECK(primary->x == 44.0f && primary->z == 16.0f);
        CHECK(primary->width == 12.0f && primary->depth == 10.0f);
        CHECK(primary->door);

        for (int32_t building = 0;
             building < profile->building_count; ++building) {
            const CcLocalPlaceBuilding *current =
                &profile->building[building];
            for (int32_t previous = 0; previous < building; ++previous) {
                CHECK(!BuildingsOverlap(current,
                                        &profile->building[previous]));
            }
            for (int32_t landmark = 0;
                 landmark < CC_LOCAL_PLACE_LANDMARK_COUNT; ++landmark) {
                CHECK(!BuildingTouchesLandmark(
                    current, &profile->landmark[landmark]));
            }
            for (int32_t structure = 0;
                 structure < profile->compound_structure_count;
                 ++structure) {
                CHECK(!BuildingTouchesCompound(
                    current, &profile->compound_structure[structure]));
            }
        }
        for (int32_t structure = 0;
             structure < profile->compound_structure_count; ++structure) {
            CHECK(!CompoundTouchesGateLane(
                &profile->compound_structure[structure]));
        }

        for (int32_t previous = CC_SETTLEMENT_FARMING;
             previous < function; ++previous) {
            const CcLocalPlaceProfile *other =
                CcLocalPlaceProfileForFunction(
                    (CcSettlementFunction)previous);
            bool same_map = profile->building_count == other->building_count;
            if (same_map) {
                for (int32_t building = 0;
                     building < profile->building_count; ++building) {
                    if (!SameBuildingGeometry(&profile->building[building],
                                              &other->building[building])) {
                        same_map = false;
                        break;
                    }
                }
            }
            CHECK(!same_map);
            CHECK(strcmp(profile->map_form, other->map_form) != 0);
        }
    }
    return 0;
}

static int CarriageRoutePlans(void)
{
    for (int32_t function = CC_SETTLEMENT_FARMING;
         function <= CC_SETTLEMENT_DUNGEON_TOWN; ++function) {
        const CcLocalPlaceProfile *profile =
            CcLocalPlaceProfileForFunction((CcSettlementFunction)function);
        const CcLocalPlaceRoad *gate = &profile->carriage_route[0];
        const CcLocalPlaceRoad *arrival = &profile->carriage_route[1];
        const CcLocalPlaceRoad *spine = &profile->carriage_route[2];
        const CcLocalPlaceRoad *turn = &profile->carriage_route[3];
        const CcLocalPlaceRoad *yard = &profile->carriage_route[4];

        CHECK(!gate->runs_east_west && gate->width >= 5.2f);
        CHECK(arrival->runs_east_west && arrival->depth >= 5.2f);
        CHECK(!spine->runs_east_west && spine->width >= 5.2f);
        CHECK(turn->width >= 12.0f && turn->depth >= 12.0f);
        CHECK(yard->width >= 13.0f && yard->depth >= 13.0f);
        CHECK(arrival->x + arrival->width == 96.0f);
        CHECK(RoadsOverlap(gate, arrival));
        CHECK(RoadsOverlap(arrival, spine));
        CHECK(RoadsOverlap(arrival, turn));
        CHECK(RoadsOverlap(spine, turn));
        CHECK(RoadsOverlap(spine, yard));

        for (int32_t route = 0;
             route < CC_LOCAL_CARRIAGE_ROUTE_COUNT; ++route) {
            for (int32_t previous = 0; previous < route; ++previous) {
                CHECK(strcmp(profile->carriage_route[route].name,
                             profile->carriage_route[previous].name) != 0);
            }
            for (int32_t building = 0;
                 building < profile->building_count; ++building) {
                CHECK(!RoadTouchesBuilding(
                    &profile->carriage_route[route],
                    &profile->building[building]));
            }
        }
        for (int32_t structure = 0;
             structure < profile->compound_structure_count; ++structure) {
            CHECK(!RoadTouchesCompound(
                gate, &profile->compound_structure[structure]));
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
    if (AuthoredTownMaps() != 0) return 1;
    if (CarriageRoutePlans() != 0) return 1;
    if (CanonicalRegionProfiles() != 0) return 1;
    if (StableDistinctTerrain() != 0) return 1;
    (void)puts("local place profile tests passed");
    return 0;
}
