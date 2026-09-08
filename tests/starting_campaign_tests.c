#include "persistence/cc_starting_campaign.h"
#include "persistence/cc_save.h"
#include "test_support.h"
#include <string.h>
#include <stdio.h>
static CcSim world, historical, restored, unchanged;
static char error[256];
int main(int argc, char **argv)
{
    CC_CHECK(argc == 2);
    CC_CHECK(CcSaveRead(argv[1], &historical, error, sizeof(error)));
    CC_CHECK(CcSimHash(&historical) == UINT64_C(0x8c55991e74f0d280));
    CcSimInit(&world, 42U); unchanged = world;
    CC_CHECK(!CcStartingCampaignDeepWyrm(&world, "missing-campaign.ccsave", error, sizeof(error)));
    CC_CHECK(CcSimHash(&world) == CcSimHash(&unchanged));
    CC_CHECK(CcStartingCampaignDeepWyrm(&world, argv[1], error, sizeof(error)));
    CC_CHECK(world.current_day == CC_DEEP_WYRM_DAY && world.world_seed == CC_DEEP_WYRM_SEED);
    const CcSettlement *town = CcSimSettlement(&world, world.player.location_id);
    CC_CHECK(town != NULL && strcmp(town->name, "Gloamgate") == 0 && town->population == 296);
    CC_CHECK(world.player.coins == 42 && world.player.accepted_situation_id == 0U);
    CC_CHECK(world.carriage.location_id == town->id && world.carriage.condition == 100);
    CC_CHECK(!world.journey.active && world.horse_team[0].health == 100 && world.horse_team[0].hunger == 0);
    CC_CHECK(world.dragon.life_stage == CC_DRAGON_STAGE_DEEP_WYRM && !world.dragon.slain);
    CC_CHECK(world.random_state == historical.random_state && world.next_entity_serial == historical.next_entity_serial);
    CC_CHECK(memcmp(&world.clock, &historical.clock, sizeof(world.clock)) == 0);
    CC_CHECK(memcmp(world.settlements, historical.settlements, sizeof(world.settlements)) == 0);
    CC_CHECK(memcmp(world.characters, historical.characters, sizeof(world.characters)) == 0);
    CC_CHECK(memcmp(world.events, historical.events, sizeof(world.events)) == 0);
    CC_CHECK(memcmp(&world.dragon_campaign, &historical.dragon_campaign, sizeof(world.dragon_campaign)) == 0);
    CC_CHECK(CcSimValidate(&world, error, sizeof(error)));
    unsigned char *bytes = NULL; size_t length = 0;
    CC_CHECK(CcSaveEncode(&world, &bytes, &length, error, sizeof(error)));
    CC_CHECK(CcSaveDecode(bytes, length, &restored, error, sizeof(error)));
    CcSaveFreeBuffer(bytes);
    CC_CHECK(CcSimHash(&world) == CcSimHash(&restored));
    for (int day = 0; day < 120; ++day) {
        CcSimAdvanceDays(&world, 1); CcSimAdvanceDays(&restored, 1);
        CC_CHECK(CcSimValidate(&world, error, sizeof(error)));
        CC_CHECK(CcSimHash(&world) == CcSimHash(&restored));
    }
    printf("Deep Wyrm campaign: Gloamgate entry, history, fresh company, and 120-day save replay passed.\n");
    return 0;
}
