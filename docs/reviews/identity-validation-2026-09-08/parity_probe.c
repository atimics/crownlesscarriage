#include "sim/cc_sim.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static CcSim baseline, changed, before;
static void Check(const char *name, int variant)
{
    char error[256] = "";
    before = changed;
    bool valid = CcSimValidate(&changed, error, sizeof(error));
    if (memcmp(&before, &changed, sizeof(changed)) != 0) exit(2);
    printf("%u %u %d %s %d %d %s\n", baseline.schema_version,
           baseline.world_seed, baseline.current_day, name, variant, valid, error);
}
#define ID(field) do { \
    for (int variant = 0; variant < 3; ++variant) { \
        changed = baseline; \
        changed.field = variant == 0 ? 0 : variant == 1 ? \
            CcMakeId(CC_ENTITY_NONE, 1) : \
            CcMakeId(CcIdKind(baseline.field), baseline.next_entity_serial + 10); \
        Check(#field, variant); \
    } \
} while (0)
#define COLLECTION(array, count) do { \
    if (baseline.count > 0) ID(array[0].id); \
    if (baseline.count > 1) { changed = baseline; \
        changed.array[1].id = changed.array[0].id; Check(#array " duplicate", 0); } \
} while (0)
int main(void)
{
    const unsigned schemas[] = {17, 26, 37, 38, 59, 73};
    const unsigned seeds[] = {42, 0x5eed0001};
    for (unsigned v = 0; v < 6; ++v) for (unsigned s = 0; s < 2; ++s)
        for (int age = 0; age < 2; ++age) {
            CcSimInit(&baseline, seeds[s]); baseline.schema_version = schemas[v];
            if (age) CcSimAdvanceDays(&baseline, 365 * 20);
            changed = baseline; Check("baseline", 0);
            COLLECTION(kingdoms, kingdom_count); COLLECTION(settlements, settlement_count);
            COLLECTION(routes, route_count); COLLECTION(maps, map_count);
            COLLECTION(road_sites, road_site_count); COLLECTION(treasures, treasure_count);
            COLLECTION(factions, faction_count); COLLECTION(shipments, shipment_count);
            COLLECTION(royal_carriages, royal_carriage_count); COLLECTION(couriers, courier_count);
            COLLECTION(bandits, bandit_count); COLLECTION(monsters, monster_count);
            COLLECTION(dungeons, dungeon_count); COLLECTION(situations, situation_count);
            COLLECTION(fronts, front_count); COLLECTION(quest_outcomes, quest_outcome_count);
            COLLECTION(characters, character_count); COLLECTION(stable_horses, stable_horse_count);
            ID(player.id); ID(goblins.id); ID(dragon.id); ID(hoard_raiders.id); ID(horse_team[0].id);
            changed = baseline; changed.next_entity_serial = 1; Check("counter behind", 0);
            changed = baseline; changed.next_entity_serial = UINT64_MAX; Check("counter overflow", 0);
        }
    return 0;
}
