#include "persistence/cc_save.h"
#include "sim/cc_sim.h"

#include "test_support.h"
#include <stdio.h>
#include <string.h>

static int32_t CountEvents(const CcSim *sim, CcEventKind kind)
{
    int32_t count = 0;
    for (int32_t i = 0; i < sim->event_count; ++i) {
        const CcEvent *event = CcSimRecentEvent(sim, i);
        if (event != NULL && event->kind == kind) count += 1;
    }
    return count;
}

int main(void)
{
    char error[192];
    CcSim sim;
    CcSimInit(&sim, UINT32_C(0x57ab1e));
    CC_CHECK(CcSettlementHasService(
        &sim.settlements[0], CC_SERVICE_STABLE));
    CC_CHECK(CcSimHorseCount(&sim) == 2);
    CC_CHECK(sim.horse_team[0].sex == CC_HORSE_STALLION);
    CC_CHECK(sim.horse_team[1].sex == CC_HORSE_MARE);
    CC_CHECK(sim.horse_team[0].strength > 0);
    CC_CHECK(sim.horse_team[1].temperament > 0);
    CC_CHECK(CcHorseWorkingReady(&sim.horse_team[0]));

    CcMoney coins_before = sim.player.coins;
    int32_t food_before = sim.settlements[0].stock[CC_GOOD_FOOD];
    CcCommand breed = {
        .kind = CC_COMMAND_BREED_HORSES,
        .target_id = sim.horse_team[1].id,
        .amount = 1
    };
    CC_CHECK(CcSimApply(&sim, &breed, error, sizeof(error)));
    CC_CHECK(sim.player.coins == coins_before - 20);
    CC_CHECK(sim.settlements[0].stock[CC_GOOD_FOOD] == food_before - 2);
    CC_CHECK(sim.horse_team[1].pregnant_by_id == sim.horse_team[0].id);
    CC_CHECK(sim.horse_team[1].pregnancy_days_remaining == 330);
    CC_CHECK(CountEvents(&sim, CC_EVENT_HORSE_BRED) == 1);

    sim.horse_team[1].pregnancy_days_remaining = 1;
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(sim.stable_horse_count == 1);
    CC_CHECK(CcSimHorseCount(&sim) == 3);
    CcId foal_id = sim.stable_horses[0].id;
    const CcHorse *foal = CcSimHorse(&sim, foal_id);
    CC_CHECK(foal != NULL);
    CC_CHECK(foal->sire_id == sim.horse_team[0].id);
    CC_CHECK(foal->dam_id == sim.horse_team[1].id);
    CC_CHECK(foal->stable_settlement_id == sim.player.location_id);
    CC_CHECK(foal->strength >= 1 && foal->strength <= 100);
    CC_CHECK(foal->temperament >= 1 && foal->temperament <= 100);
    CC_CHECK(foal->hardiness >= 1 && foal->hardiness <= 100);
    CC_CHECK(!CcHorseWorkingReady(foal));
    CC_CHECK(CountEvents(&sim, CC_EVENT_FOAL_BORN) == 1);

    CcCommand assign = {
        .kind = CC_COMMAND_ASSIGN_HORSE,
        .target_id = foal_id,
        .amount = 1
    };
    CC_CHECK(!CcSimApply(&sim, &assign, error, sizeof(error)));
    CC_CHECK(strstr(error, "not ready") != NULL);
    sim.stable_horses[0].age_days = 3 * 365;
    sim.stable_horses[0].training = 80;
    sim.stable_horses[0].health = 90;
    sim.stable_horses[0].fatigue = 0;
    sim.stable_horses[0].hunger = 0;
    CcId retired_id = sim.horse_team[0].id;
    CC_CHECK(CcSimApply(&sim, &assign, error, sizeof(error)));
    CC_CHECK(sim.horse_team[0].id == foal_id);
    CC_CHECK(sim.horse_team[0].stable_settlement_id == 0U);
    CC_CHECK(sim.stable_horses[0].id == retired_id);
    CC_CHECK(sim.stable_horses[0].stable_settlement_id ==
             sim.player.location_id);
    CC_CHECK(CountEvents(&sim, CC_EVENT_HORSE_TEAM_CHANGED) == 1);

    sim.horse_team[1].breeding_cooldown_days = 0;
    sim.stable_horses[0].breeding_cooldown_days = 0;
    CcCommand breed_again = {
        .kind = CC_COMMAND_BREED_HORSES,
        .target_id = sim.horse_team[1].id,
        .amount = 3
    };
    sim.player.coins += 20;
    CC_CHECK(CcSimApply(&sim, &breed_again, error, sizeof(error)));
    sim.horse_team[1].pregnancy_days_remaining = 30;
    CcCommand travel = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = sim.settlements[1].id
    };
    CC_CHECK(!CcSimApply(&sim, &travel, error, sizeof(error)));
    CC_CHECK(strstr(error, "foaling") != NULL);

    const char *path = "/tmp/crownless-horse-stable-tests.ccsave";
    (void)remove(path);
    uint64_t expected_hash = CcSimHash(&sim);
    CC_CHECK(CcSaveWrite(path, &sim, error, sizeof(error)));
    CcSim restored;
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(&restored) == expected_hash);
    CC_CHECK(restored.stable_horse_count == 1);
    CC_CHECK(restored.horse_team[0].id == foal_id);
    CC_CHECK(restored.horse_team[0].sire_id == retired_id);
    CC_CHECK(restored.horse_team[1].pregnancy_days_remaining == 30);
    CC_CHECK(CcSimValidate(&restored, error, sizeof(error)));
    CC_CHECK(remove(path) == 0);

    puts("Horse stable breeding tests passed");
    return 0;
}
