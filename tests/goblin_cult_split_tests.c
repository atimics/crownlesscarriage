#include "persistence/cc_save.h"
#include "sim/cc_sim.h"
#include "test_support.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static CcSim sim, restored;
static char error[256];

static void Fresh(void)
{
    CcSimInit(&sim, 42U);
    sim.goblins.tribute_cooldown_days = 10000;
    sim.dragon.hunt_cooldown_days = 10000;
    for (int32_t r = 0; r < sim.dungeons[0].room_count; ++r) sim.dungeons[0].rooms[r].loot_quantity = 0;
}

static void Valid(void)
{
    if (!CcSimValidate(&sim, error, sizeof(error))) fprintf(stderr, "%s\n", error);
    CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
}

static void Delivery(int32_t color, CcMoney coins)
{
    CcGoblinFaction *f = &sim.goblin_politics.factions[color];
    f->porter_room = 19;
    f->target_room = 19;
    f->carried_coins = coins;
    CcSimAdvanceDays(&sim, 1);
}

static void CheckRanks(void)
{
    Fresh();
    CC_CHECK(sim.goblins.members == 48);
    CC_CHECK(CcSimCultMembers(&sim, CC_CULT_GOBLIN) == 12);
    CC_CHECK(CcSimCultMembers(&sim, CC_CULT_HUMAN) == 6);
    Delivery(CC_GOBLIN_BLUE, 40);
    CC_CHECK(sim.dragon_cult.ranks[CC_CULT_GOBLIN][CC_CULT_BEARER] == 1);
    Delivery(CC_GOBLIN_BLUE, 80);
    CC_CHECK(sim.dragon_cult.ranks[CC_CULT_GOBLIN][CC_CULT_KEEPER] == 1);
    Delivery(CC_GOBLIN_BLUE, 120);
    CC_CHECK(sim.dragon_cult.ranks[CC_CULT_GOBLIN][CC_CULT_VOICE] == 1);
    sim.dragon_cult.service[CC_CULT_HUMAN] = 39;
    sim.current_day = 27;
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(sim.dragon_cult.ranks[CC_CULT_HUMAN][CC_CULT_BEARER] == 1);
    sim.dragon_cult.service[CC_CULT_HUMAN] = 79;
    sim.current_day = 55;
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(sim.dragon_cult.ranks[CC_CULT_HUMAN][CC_CULT_KEEPER] == 1);
    sim.dragon_cult.service[CC_CULT_HUMAN] = 119;
    sim.current_day = 83;
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(sim.dragon_cult.ranks[CC_CULT_HUMAN][CC_CULT_VOICE] == 1);
    Valid();
}

static void CheckCrownAndBelief(void)
{
    for (int32_t winner = 0; winner < 3; ++winner) {
        Fresh();
        Delivery(winner, 100);
        sim.current_day = 365;
        CcSimAdvanceDays(&sim, 1);
        CC_CHECK(sim.goblin_politics.crown_faction == winner);
        int32_t loser = (winner + 1) % 3;
        Delivery(loser, 500);
        CC_CHECK(sim.goblin_politics.factions[loser].tribute > sim.goblin_politics.factions[winner].tribute);
        CC_CHECK(sim.goblin_politics.crown_faction == winner);
        /* The losing faction still sends its next stored treasure. */
        CcGoblinFaction *f = &sim.goblin_politics.factions[loser];
        f->porter_room = f->lair_room; f->target_room = f->lair_room;
        f->coins = 80;
        CcSimAdvanceDays(&sim, 1);
        CC_CHECK(f->target_room == 19 && f->carried_coins > 0);
        int32_t protected_members = sim.goblin_politics.factions[winner].members;
        for (int32_t hunt = 0; hunt < 2; ++hunt) {
            sim.dragon.body_condition = 1;
            sim.dragon.hunt_cooldown_days = 0;
            CcSimAdvanceDays(&sim, 1);
        }
        CC_CHECK(sim.goblin_politics.factions[winner].members == protected_members);
        CC_CHECK(sim.goblin_politics.factions[(winner + 1) % 3].hunted > 0);
        CC_CHECK(sim.goblin_politics.factions[(winner + 2) % 3].hunted > 0);
        CC_CHECK(sim.goblin_politics.crown_faction == winner);
        Valid();
    }
    Fresh();
    Delivery(CC_GOBLIN_RED, 100);
    Delivery(CC_GOBLIN_BLUE, 100);
    sim.current_day = 365;
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(sim.goblin_politics.crown_faction == -1);
    Delivery(CC_GOBLIN_BLUE, 1);
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(sim.goblin_politics.crown_faction == CC_GOBLIN_BLUE);
}

static void CheckPortageAndDeath(void)
{
    Fresh();
    CcGoblinFaction *f = &sim.goblin_politics.factions[CC_GOBLIN_PURPLE];
    f->gold = 2;
    CcMoney coins_before = CcSimTrackedGold(&sim);
    int32_t gold_before = CcSimTrackedGood(&sim, CC_GOOD_GOLD);
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(f->target_room == 19 && f->carried_gold == 2);
    int32_t room = f->porter_room;
    uint32_t flags[CC_MAX_DUNGEON_LINKS];
    for (int32_t i = 0; i < sim.dungeons[0].link_count; ++i) {
        flags[i] = sim.dungeons[0].links[i].flags;
        sim.dungeons[0].links[i].flags = 0U;
    }
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(f->porter_room == room && f->carried_gold == 2);
    CC_CHECK(CcSimTrackedGood(&sim, CC_GOOD_GOLD) == gold_before);
    CC_CHECK(CcSimTrackedGold(&sim) == coins_before);
    for (int32_t i = 0; i < sim.dungeons[0].link_count; ++i) sim.dungeons[0].links[i].flags = flags[i];
    sim.dragon.slain = true;
    sim.dragon.slain_day = sim.current_day;
    sim.dragon.life_stage = CC_DRAGON_STAGE_AFTERDRAGON;
    sim.dragon.activity = CC_DRAGON_ACTIVITY_AFTERMATH;
    CcSimAdvanceDays(&sim, 12);
    CC_CHECK(f->gold == 2 && f->carried_gold == 0);
    CC_CHECK(sim.goblin_politics.crown_faction == -1);
    CC_CHECK(CcSimTrackedGood(&sim, CC_GOOD_GOLD) == gold_before);
    /* A successor starts a fresh contest; old lair wealth stays owned. */
    sim.dragon.id = CcMakeId(CC_ENTITY_DRAGON, sim.next_entity_serial++);
    sim.dragon.slain = false;
    sim.dragon.life_stage = CC_DRAGON_STAGE_WHELP;
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(sim.goblin_politics.dragon_id == sim.dragon.id);
    CC_CHECK(sim.goblin_politics.contest_started_day == sim.current_day);
    CC_CHECK(f->tribute == 0);
    CC_CHECK(f->target_room == 19);
}

static void CheckPersistence(void)
{
    Fresh();
    Delivery(CC_GOBLIN_RED, 80);
    sim.goblin_politics.factions[1].coins = 70;
    sim.goblin_politics.factions[2].gems = 2;
    CcSimAdvanceDays(&sim, 1);
    Valid();
    unsigned char *bytes = NULL;
    size_t length = 0;
    CC_CHECK(CcSaveEncode(&sim, &bytes, &length, error, sizeof(error)));
    CC_CHECK(CcSaveDecode(bytes, length, &restored, error, sizeof(error)));
    CcSaveFreeBuffer(bytes);
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));
    CcSimAdvanceDays(&sim, 100);
    CcSimAdvanceDays(&restored, 100);
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));
    sim.goblin_politics.factions[0].porter_room = CC_MAX_DUNGEON_ROOMS;
    CC_CHECK(!CcSimValidate(&sim, error, sizeof(error)));
    Fresh();
    sim.dragon_cult.ranks[CC_CULT_GOBLIN][0] = 100;
    CC_CHECK(!CcSimValidate(&sim, error, sizeof(error)));
}

static void CheckSweep(void)
{
    for (uint32_t seed = 1; seed <= 20U; ++seed) {
        CcSimInit(&sim, seed * UINT32_C(2654435769));
        for (int32_t year = 0; year < 100; ++year) {
            CcSimAdvanceDays(&sim, 365);
            if (!CcSimValidate(&sim, error, sizeof(error)))
                fprintf(stderr, "seed %u year %d: %s\n", seed, year + 1, error);
            Valid();
        }
    }
}

int main(void)
{
    CheckRanks();
    CheckCrownAndBelief();
    CheckPortageAndDeath();
    CheckPersistence();
    CheckSweep();
    puts("Mixed cult ranks, lifelong crowns, continued tribute, enemy hunts, portage, saves and 20 x 100 years passed.");
    return 0;
}
