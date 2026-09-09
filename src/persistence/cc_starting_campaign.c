#include "persistence/cc_starting_campaign.h"
#include "persistence/cc_save.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

bool CcStartingCampaignDeepWyrm(CcSim *sim, const char *path,
                               char *error, size_t capacity)
{
    CcSim *world = calloc(1, sizeof(*world));
    CcSim *company = calloc(1, sizeof(*company));
    bool loaded = false;
    if (sim == NULL || world == NULL || company == NULL) {
        if (error != NULL && capacity > 0) (void)snprintf(error, capacity, "The campaign needs more memory.");
        goto done;
    }
    if (!CcSaveRead(path, world, error, capacity)) goto done;
    if (world->world_seed != CC_DEEP_WYRM_SEED || world->current_day != CC_DEEP_WYRM_DAY ||
        world->schema_version != CC_SIM_SCHEMA_VERSION || world->generator_version != 25U ||
        CcSimHash(world) != UINT64_C(0xb3674235d45d3345)) {
        (void)snprintf(error, capacity, "The starting world differs from the recorded campaign. Restore its campaign file.");
        goto done;
    }
    CcId town = 0U;
    for (int i = 0; i < world->settlement_count; ++i)
        if (strcmp(world->settlements[i].name, "Gloamgate") == 0 &&
            !CcSettlementIsAbandoned(&world->settlements[i])) town = world->settlements[i].id;
    if (town == 0U) {
        (void)snprintf(error, capacity, "The campaign needs its living town of Gloamgate.");
        goto done;
    }
    CcSimInit(company, CC_DEEP_WYRM_SEED);
    CcId player_id = world->player.id;
    world->player = company->player;
    world->player.id = player_id;
    world->player.location_id = town;
    world->carriage = company->carriage;
    world->carriage.location_id = town;
    world->pony_company = company->pony_company;
    memcpy(world->horse_team, company->horse_team, sizeof(world->horse_team));
    memcpy(world->stable_horses, company->stable_horses, sizeof(world->stable_horses));
    world->stable_horse_count = company->stable_horse_count;
    world->journey = company->journey;
    world->mine = company->mine;
    CcSimInitializePlayerRouteKnowledge(world);
    if (!CcSimValidate(world, error, capacity)) goto done;
    *sim = *world;
    loaded = true;
done:
    free(company);
    free(world);
    return loaded;
}
