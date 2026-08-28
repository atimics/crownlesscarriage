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
        CHECK(profile->feature_mask != 0U);
        for (int32_t room = 0; room < CC_LOCAL_PLACE_ROOM_COUNT; ++room) {
            const char *name = CcLocalPlaceRoomName(
                (CcSettlementFunction)function, room);
            CHECK(name != NULL && name[0] != '\0');
        }
    }
    CHECK(CcLocalPlaceRoomName(CC_SETTLEMENT_MARKET, -1) == NULL);
    CHECK(CcLocalPlaceRoomName(CC_SETTLEMENT_MARKET,
                               CC_LOCAL_PLACE_ROOM_COUNT) == NULL);
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
    if (CanonicalRegionProfiles() != 0) return 1;
    if (StableDistinctTerrain() != 0) return 1;
    (void)puts("local place profile tests passed");
    return 0;
}
