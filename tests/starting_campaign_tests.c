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
    CC_CHECK(CcSimHash(&historical) == UINT64_C(0x3410af0b51fda959));
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
    const CcTreasure *book = CcSimDeepWyrmProphecy(&world);
    CC_CHECK(book != NULL && book->owner_id == world.player.id);
    CC_CHECK(CcPlayerCargoUsed(&world.player) == 1 && world.player.treasure_cargo_slots == 1);
    CC_CHECK(!CcSimGiveDeepWyrmProphecy(&world));
    CcCommand deliver = {.kind = CC_COMMAND_DELIVER_PROPHECY, .target_id = book->id};
    uint64_t before = CcSimHash(&world);
    CcCommand bad_target = deliver; bad_target.target_id = 0U;
    CC_CHECK(!CcSimApply(&world, &bad_target, error, sizeof(error)));
    CC_CHECK(CcSimHash(&world) == before);
    CC_CHECK(world.dragon.life_stage == CC_DRAGON_STAGE_DEEP_WYRM && !world.dragon.slain);
    CC_CHECK(world.random_state == historical.random_state && world.next_entity_serial == historical.next_entity_serial + 1U);
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
    /* Put a fresh delivery fixture at the council, then exercise the command boundary. */
    CC_CHECK(CcStartingCampaignDeepWyrm(&world, argv[1], error, sizeof(error)));
    CcTreasure *carried = (CcTreasure *)CcSimDeepWyrmProphecy(&world);
    const CcSettlement *destination = CcSimProphecyDestination(&world);
    CC_CHECK(destination != NULL && !CcSettlementIsAbandoned(destination));
    world.player.location_id = destination->id;
    world.carriage.location_id = destination->id;
    carried->location_id = destination->id;
    CC_CHECK(CcSimValidate(&world, error, sizeof(error)));
    deliver.target_id = carried->id;
    CcId home = world.player.location_id;
    CcId other = historical.settlements[0].id;
    CC_CHECK(other != home);
    world.player.location_id = other;
    carried->location_id = other;
    before = CcSimHash(&world);
    CC_CHECK(!CcSimApply(&world, &deliver, error, sizeof(error)));
    CC_CHECK(CcSimHash(&world) == before);
    world.player.location_id = home;
    carried->location_id = home;
    world.journey.active = true;
    CC_CHECK(!CcSimApply(&world, &deliver, error, sizeof(error)));
    world.journey.active = false;
    CcCommand wrong = deliver; wrong.target_id = world.player.id;
    before = CcSimHash(&world);
    CC_CHECK(!CcSimApply(&world, &wrong, error, sizeof(error)));
    CC_CHECK(CcSimHash(&world) == before);
    CcCommand sell = {.kind = CC_COMMAND_SELL_TREASURE, .target_id = carried->id};
    CC_CHECK(!CcSimApply(&world, &sell, error, sizeof(error)));
    CC_CHECK(CcSimHash(&world) == before);
    CC_CHECK(CcSimApply(&world, &deliver, error, sizeof(error)));
    CC_CHECK(carried->owner_id == destination->id && world.player.treasure_cargo_slots == 0);
    CC_CHECK(world.player.coins == 42 && world.player.accepted_situation_id == 0U);
    CC_CHECK(CcSimRecentEvent(&world, 0)->kind == CC_EVENT_PROPHECY_DELIVERED);
    CC_CHECK(CcSimValidate(&world, error, sizeof(error)));
    before = CcSimHash(&world);
    CC_CHECK(!CcSimApply(&world, &deliver, error, sizeof(error)));
    CC_CHECK(CcSimHash(&world) == before);
    CC_CHECK(CcSaveEncode(&world, &bytes, &length, error, sizeof(error)));
    CC_CHECK(CcSaveDecode(bytes, length, &restored, error, sizeof(error)));
    CcSaveFreeBuffer(bytes);
    CC_CHECK(CcSimHash(&restored) == before);
    CC_CHECK(!CcSimApply(&restored, &deliver, error, sizeof(error)));
    printf("Deep Wyrm campaign: Gloamgate entry, history, fresh company, and 120-day save replay passed.\n");
    return 0;
}
