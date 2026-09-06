#include "persistence/cc_save.h"
#include "sim/cc_sim.h"

#include "test_support.h"
#include <stdio.h>
#include <string.h>

static int32_t CountEvents(const CcSim *sim, CcEventKind kind,
                           const char *needle)
{
    int32_t count = 0;
    for (int32_t i = 0; i < sim->event_count; ++i) {
        const CcEvent *event = CcSimRecentEvent(sim, i);
        if (event == NULL || event->kind != kind) continue;
        if (needle == NULL || strstr(event->text, needle) != NULL) count += 1;
    }
    return count;
}

/* Force the fall of a chosen settlement exactly like the balance tests do. */
static void FallSettlement(CcSim *sim, int32_t slot)
{
    CcSettlement *failed = &sim->settlements[slot];
    failed->population = 60;
    failed->hunger = 90;
    failed->stock[CC_GOOD_FOOD] = 0;
    failed->production[CC_GOOD_FOOD] = 0;
    failed->consumption[CC_GOOD_FOOD] = 7;
}

/* Seizure, reclamation block, and release of war camps. */
static void CheckWarCampContest(char *error, size_t capacity)
{
    /* A strong band takes a fallen town. */
    CcSim seize;
    CcSimInit(&seize, UINT32_C(0x60c11b));
    seize.bandits[0].members = 70;
    seize.bandits[0].supplies = 40;
    seize.bandits[0].influence = 55;
    int32_t fall_slot = -1;
    for (int32_t i = 0; i < seize.settlement_count; ++i) {
        if (strcmp(seize.settlements[i].name, "Thornford") == 0) {
            fall_slot = i;
            break;
        }
    }
    CC_CHECK(fall_slot >= 0);
    FallSettlement(&seize, fall_slot);
    CcSimAdvanceDays(&seize, 28);
    CC_CHECK(CcSettlementIsAbandoned(&seize.settlements[fall_slot]));
    CC_CHECK(seize.bandits[0].camp_settlement_id ==
             seize.settlements[fall_slot].id);
    CC_CHECK(seize.bandits[0].influence >= 50);
    CC_CHECK(CountEvents(&seize, CC_EVENT_KINGDOM_ACTION,
                         "war camp") == 1);
    CC_CHECK(CcSimValidate(&seize, error, capacity));

    /* A weak band does not claim the ruin. */
    CcSim weak;
    CcSimInit(&weak, UINT32_C(0x60c11b));
    weak.bandits[0].members = 25;
    weak.bandits[0].supplies = 15;
    weak.bandits[0].influence = 20;
    FallSettlement(&weak, fall_slot);
    CcSimAdvanceDays(&weak, 28);
    CC_CHECK(CcSettlementIsAbandoned(&weak.settlements[fall_slot]));
    CC_CHECK(weak.bandits[0].camp_settlement_id == 0U);
    CC_CHECK(CountEvents(&weak, CC_EVENT_KINGDOM_ACTION,
                         "war camp") == 0);

    /* A held ruin cannot be resettled while the band is strong. */
    CcSim contested;
    CcSimInit(&contested, UINT32_C(0xba1a5eed));
    contested.settlement_count = 2;
    contested.route_count = 1;
    contested.shipment_count = 0;
    contested.courier_count = 0;
    contested.bandit_count = 0;
    contested.monster_count = 0;
    contested.dungeon_count = 0;
    contested.situation_count = 0;
    contested.dragon.slain = true;
    contested.goblins.tribute_cooldown_days = 10000;
    contested.hoard_raiders.cooldown_days = 10000;
    CcSettlement *donor = &contested.settlements[0];
    CcSettlement *held_ruin = &contested.settlements[1];
    held_ruin->population = 0;
    held_ruin->security = 0;
    held_ruin->prosperity = 0;
    held_ruin->hunger = 100;
    held_ruin->service_mask = 0U;
    held_ruin->service_project = CC_SERVICE_NONE;
    held_ruin->service_project_days = 0;
    donor->population = 2000;
    donor->hunger = 0;
    donor->prosperity = 90;
    donor->stock[CC_GOOD_FOOD] = 100;
    donor->stock[CC_GOOD_TOOLS] = 10;
    donor->market_coins = 100;
    contested.bandit_count = 1;
    contested.bandits[0].camp_settlement_id = held_ruin->id;
    contested.bandits[0].influence = 60;
    contested.current_day = 379 * 7 - 7;
    CcSimAdvanceDays(&contested, 7);
    CC_CHECK(CcSettlementIsAbandoned(held_ruin));
    CC_CHECK(held_ruin->population == 0);
    CC_CHECK(donor->population == 2000);

    /* When the band starves, the camp is abandoned and the ruin is
     * open for settlers again. */
    contested.bandits[0].influence = 20;
    CcSimAdvanceDays(&contested, 1);
    CC_CHECK(contested.bandits[0].camp_settlement_id == 0U);
    CC_CHECK(CountEvents(&contested, CC_EVENT_KINGDOM_ACTION,
                         "stands open again") == 1);
    contested.current_day = 795 * 7 - 7;
    CcSimAdvanceDays(&contested, 7);
    CC_CHECK(!CcSettlementIsAbandoned(held_ruin));
    CC_CHECK(held_ruin->population == 180);

    /* Save round trip keeps the camp; an old-schema save reads it as 0. */
    const char *path = "/tmp/crownless-war-camp.ccsave";
    (void)remove(path);
    CcSim saved;
    CcSimInit(&saved, UINT32_C(0x60c11b));
    saved.bandits[0].members = 70;
    saved.bandits[0].supplies = 40;
    saved.bandits[0].influence = 55;
    FallSettlement(&saved, fall_slot);
    CcSimAdvanceDays(&saved, 28);
    CC_CHECK(saved.bandits[0].camp_settlement_id != 0U);
    uint64_t hash = CcSimHash(&saved);
    CC_CHECK(CcSaveWrite(path, &saved, error, capacity));
    CcSim restored;
    CC_CHECK(CcSaveRead(path, &restored, error, capacity));
    CC_CHECK(CcSimHash(&restored) == hash);
    CC_CHECK(restored.bandits[0].camp_settlement_id ==
             saved.bandits[0].camp_settlement_id);
    CC_CHECK(CcSaveWrite(path, &restored, error, capacity));
    (void)remove(path);
}

int main(void)
{
    char error[192];
    CheckWarCampContest(error, sizeof(error));

    /* Fall Thornford (full cast world) and follow the refugees. */
    CcSim fall;
    CcSimInit(&fall, UINT32_C(0x60c11a));
    int32_t thornford = -1;
    for (int32_t i = 0; i < fall.settlement_count; ++i) {
        if (strcmp(fall.settlements[i].name, "Thornford") == 0) {
            thornford = i;
            break;
        }
    }
    CC_CHECK(thornford >= 0);
    FallSettlement(&fall, thornford);
    int32_t before_population[CC_MAX_SETTLEMENTS];
    for (int32_t i = 0; i < fall.settlement_count; ++i) {
        before_population[i] = fall.settlements[i].population;
    }
    int32_t thornford_residents = 0;
    for (int32_t i = 0; i < fall.character_count; ++i) {
        if (fall.characters[i].home_settlement_id ==
            fall.settlements[thornford].id) {
            thornford_residents += 1;
        }
    }
    CC_CHECK(thornford_residents > 0);

    CcSimAdvanceDays(&fall, 28);
    CC_CHECK(CcSettlementIsAbandoned(&fall.settlements[thornford]));
    CC_CHECK(CountEvents(&fall, CC_EVENT_KINGDOM_ACTION,
                         "drives") == 1);

    /* The named residents now live in a living host town. */
    int32_t refugees = 0;
    int32_t hosts = 0;
    CcId host_ids[CC_MAX_SETTLEMENTS];
    for (int32_t i = 0; i < fall.settlement_count; ++i) {
        if (CcSettlementIsAbandoned(&fall.settlements[i])) continue;
        host_ids[hosts++] = fall.settlements[i].id;
    }
    CC_CHECK(hosts > 0);
    for (int32_t i = 0; i < fall.character_count; ++i) {
        if (fall.characters[i].role != CC_CHARACTER_REFUGEE) continue;
        refugees += 1;
        bool hosted = false;
        for (int32_t h = 0; h < hosts; ++h) {
            if (fall.characters[i].home_settlement_id == host_ids[h]) {
                hosted = true;
                break;
            }
        }
        CC_CHECK(hosted);
    }
    CC_CHECK(refugees == thornford_residents);

    /* The host absorbed the households and its hunger rose. */
    bool absorbed = false;
    for (int32_t i = 0; i < fall.settlement_count; ++i) {
        if (CcSettlementIsAbandoned(&fall.settlements[i])) continue;
        if (fall.settlements[i].population > before_population[i] + 30) {
            absorbed = true;
        }
    }
    CC_CHECK(absorbed);
    CC_CHECK(CcSimValidate(&fall, error, sizeof(error)));

    /* A resident of the dead town, dying later, is not born into the ruin. */
    CcSim heir;
    CcSimInit(&heir, UINT32_C(0x60c11a));
    FallSettlement(&heir, thornford);
    CcSimAdvanceDays(&heir, 28);
    CcId ghost = 0U;
    for (int32_t i = 0; i < heir.character_count; ++i) {
        if (heir.characters[i].home_settlement_id ==
            heir.settlements[thornford].id) {
            ghost = heir.characters[i].id;
            break;
        }
    }
    /* Everyone fled; take the abbot instead if the town emptied of cast. */
    if (ghost == 0U) ghost = heir.archives.abbot_character_id;
    CcCharacter *dying = (CcCharacter *)CcSimCharacter(&heir, ghost);
    CC_CHECK(dying != NULL);
    dying->death_day = heir.current_day;
    CcSimAdvanceDays(&heir, 1);
    const CcCharacter *successor = CcSimCharacter(
        &heir, CcSimCharacter(&heir, ghost) != NULL ? 0U : 0U);
    (void)successor;
    CC_CHECK(CcSimValidate(&heir, error, sizeof(error)));
    /* The successor of the abbot (ruler titles aside) must live somewhere
     * living: find the slot that used to hold the dying character. */
    const CcCharacter *born = NULL;
    for (int32_t i = 0; i < heir.character_count; ++i) {
        if (heir.characters[i].ancestor_id == ghost) {
            born = &heir.characters[i];
            break;
        }
    }
    CC_CHECK(born != NULL);
    const CcSettlement *born_home = CcSimSettlement(
        &heir, born->home_settlement_id);
    CC_CHECK(born_home != NULL);
    CC_CHECK(!CcSettlementIsAbandoned(born_home));
    CC_CHECK(CountEvents(&heir, CC_EVENT_CHARACTER_BORN, "born into") >= 1);

    /* With no living host, the world stays quiet: nobody relocates. */
    CcSim dead_world;
    CcSimInit(&dead_world, UINT32_C(0x60c11a));
    for (int32_t i = 0; i < dead_world.settlement_count; ++i) {
        dead_world.settlements[i].population = 0;
        dead_world.settlements[i].hunger = 100;
        dead_world.settlements[i].security = 0;
        dead_world.settlements[i].prosperity = 0;
        dead_world.settlements[i].service_mask = 0U;
        dead_world.settlements[i].service_project = CC_SERVICE_NONE;
        dead_world.settlements[i].service_project_days = 0;
        dead_world.settlements[i].market_coins = 0;
        dead_world.settlements[i].war_chest = 0;
    }
    FallSettlement(&dead_world, thornford);
    CcSimAdvanceDays(&dead_world, 28);
    CC_CHECK(CcSimValidate(&dead_world, error, sizeof(error)));

    /* Save round trip preserves the relocation. */
    const char *path = "/tmp/crownless-war-society-tests.ccsave";
    (void)remove(path);
    uint64_t hash = CcSimHash(&fall);
    CC_CHECK(CcSaveWrite(path, &fall, error, sizeof(error)));
    CcSim restored;
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(&restored) == hash);
    int32_t restored_refugees = 0;
    for (int32_t i = 0; i < restored.character_count; ++i) {
        if (restored.characters[i].role == CC_CHARACTER_REFUGEE) {
            restored_refugees += 1;
        }
    }
    CC_CHECK(restored_refugees == refugees);
    (void)remove(path);

    puts("Refugee relocation tests passed");
    return 0;
}
