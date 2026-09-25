#include "client/cc_local_place.h"
#include "sim/cc_food_economy_internal.h"
#include "sim/cc_journey_internal.h"
#include "sim/cc_road_position.h"
#include "sim/cc_sim.h"
#include "world/cc_world.h"

#include <stdio.h>

/* Read-only snapshot probe. Keep the seed fixed so reports can be compared. */
int main(void)
{
    static CcSim sim;
    static CcWorldManifest manifest;
    const uint32_t seed = UINT32_C(0x3235a7ed);
    CcSimInit(&sim, seed);
    if (!CcWorldManifestBuild(&manifest, &sim)) return 1;

    int32_t population = 0;
    for (int32_t i = 0; i < sim.settlement_count; ++i)
        population += sim.settlements[i].population;

    printf("{\n  \"seed\": \"0x%08x\",\n", (unsigned)seed);
    printf("  \"schema\": %u, \"generator\": %u,\n",
           (unsigned)sim.schema_version, (unsigned)CC_GENERATOR_VERSION);
    printf("  \"resident_total\": %d, \"character_records\": %d,\n",
           population, sim.character_count);
    printf("  \"active_character_records\": %d,\n",
           CcSimActiveCharacterCount(&sim));
    printf("  \"sim_bytes\": %zu, \"character_bytes\": %zu,\n",
           sizeof(CcSim), sizeof(CcCharacter));
    puts("  \"settlements\": [");
    for (int32_t i = 0; i < sim.settlement_count; ++i) {
        const CcSettlement *place = &sim.settlements[i];
        const CcLocalPlaceProfile *profile = CcLocalPlaceProfileForSettlement(place);
        const CcWorldSettlementPlacement *world =
            CcWorldSettlementPlacementForId(&manifest, place->id);
        if (profile == NULL || world == NULL) return 2;
        int32_t named = 0;
        for (int32_t j = 0; j < sim.character_count; ++j)
            if (sim.characters[j].home_settlement_id == place->id) ++named;
        printf("    {\"name\": \"%s\", \"population\": %d, "
               "\"named_home_records\": %d, \"main_buildings\": %d, "
               "\"compound_structures\": %d, \"profile_scale\": %.6f, "
               "\"world_radius\": %.1f, \"civilian_rations_per_week\": %d}%s\n",
               place->name, place->population, named, profile->building_count,
               profile->compound_structure_count, world->profile_scale,
               world->radius, CcEconomyCivilianFoodUse(place),
               i + 1 < sim.settlement_count ? "," : "");
    }
    puts("  ],\n  \"routes\": [");
    for (int32_t i = 0; i < sim.route_count; ++i) {
        const CcRoute *route = &sim.routes[i];
        const CcSettlement *from = CcSimSettlement(&sim, route->from_id);
        const CcSettlement *to = CcSimSettlement(&sim, route->to_id);
        CcRoadGeometry geometry;
        if (from == NULL || to == NULL ||
            !CcRoadGeometryBuild(&sim, route->id, &geometry)) return 3;
        unsigned narrative_miles = (unsigned)(route->travel_days * 18 + 4 +
            CcJourneyRoadHouseSeed(&sim, route->id) % 9U);
        printf("    {\"from\": \"%s\", \"to\": \"%s\", \"days\": %d, "
               "\"full_world_units\": %.3f, \"journey_world_units\": %.3f, "
               "\"derived_road_house_route_miles\": %u}%s\n",
               from->name, to->name, route->travel_days,
               (double)geometry.full_length_units / 1000.0,
               (double)geometry.journey_length_units / 1000.0,
               narrative_miles, i + 1 < sim.route_count ? "," : "");
    }
    puts("  ]\n}");
    return 0;
}
