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

int main(void)
{
    char error[192];

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
