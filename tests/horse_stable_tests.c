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

static void CheckSavedHorseCare(void)
{
    static CcSim sim;
    static CcSim restored;
    char error[192];
    CcSimInit(&sim, UINT32_C(0x57ab1e));
    CcSettlement *town = CcSimSettlementMutable(&sim,
                                                  sim.player.location_id);
    CC_CHECK(town != NULL);
    sim.horse_team[0].health = 20;
    sim.horse_team[0].hunger = 65;
    sim.horse_team[0].fatigue = 70;
    CcCommand care = {.kind = CC_COMMAND_CARE_HORSES};
    CcHorseCarePreview offer;

    uint32_t services = town->service_mask;
    town->service_mask = 0U;
    CC_CHECK(CcSimHorseCarePreview(&sim, &offer) && !offer.available);
    CC_CHECK(strstr(offer.reason, "stable") != NULL);
    uint64_t before = CcSimHash(&sim);
    CC_CHECK(!CcSimApply(&sim, &care, error, sizeof(error)));
    CC_CHECK(CcSimHash(&sim) == before);

    town->service_mask = services;
    town->stock[CC_GOOD_WHEAT] = 0;
    sim.player.cargo[CC_GOOD_WHEAT] = 0;
    sim.player.feed_tray_wheat = 0;
    CC_CHECK(CcSimHorseCarePreview(&sim, &offer) && !offer.available);
    CC_CHECK(strstr(offer.reason, "1 Wheat") != NULL);
    before = CcSimHash(&sim);
    CC_CHECK(!CcSimApply(&sim, &care, error, sizeof(error)));
    CC_CHECK(CcSimHash(&sim) == before);

    town->stock[CC_GOOD_WHEAT] = 1;
    CcMoney original_coins = sim.player.coins;
    sim.player.coins = 0;
    CC_CHECK(CcSimHorseCarePreview(&sim, &offer) && !offer.available);
    CC_CHECK(strstr(offer.reason, "pay") != NULL);
    before = CcSimHash(&sim);
    CC_CHECK(!CcSimApply(&sim, &care, error, sizeof(error)));
    CC_CHECK(CcSimHash(&sim) == before);

    sim.player.coins = original_coins;
    CC_CHECK(CcSimHorseCarePreview(&sim, &offer) && offer.available);
    CC_CHECK(offer.source == CC_HORSE_CARE_MARKET);
    CC_CHECK(offer.cost == 4 + town->price[CC_GOOD_WHEAT]);
    CcMoney market_coins = town->market_coins;
    CcMoney company_coins = sim.player.coins;
    int32_t day = sim.current_day;
    CC_CHECK(CcSimApply(&sim, &care, error, sizeof(error)));
    CC_CHECK(sim.current_day == day + 1);
    CC_CHECK(sim.player.coins == company_coins - offer.cost);
    CC_CHECK(town->market_coins == market_coins + offer.cost);
    CC_CHECK(town->stock[CC_GOOD_WHEAT] == 0);
    CC_CHECK(sim.horse_team[0].health > 20);
    CC_CHECK(sim.horse_team[0].hunger < 65);
    CC_CHECK(sim.horse_team[0].fatigue < 70);
    CC_CHECK(strstr(CcSimRecentEvent(&sim, 0)->text,
                    "stable market") != NULL);

    const char *path = "/tmp/crownless-horse-care-tests.ccsave";
    (void)remove(path);
    uint64_t hash = CcSimHash(&sim);
    CC_CHECK(CcSaveWrite(path, &sim, error, sizeof(error)));
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(&restored) == hash);
    CC_CHECK(restored.horse_team[0].health == sim.horse_team[0].health);
    CC_CHECK(remove(path) == 0);

    sim.horse_team[0].health = 20;
    sim.player.cargo[CC_GOOD_WHEAT] = 1;
    CC_CHECK(CcSimHorseCarePreview(&sim, &offer) && offer.available);
    CC_CHECK(offer.source == CC_HORSE_CARE_CARGO && offer.cost == 4);
    CC_CHECK(CcSimApply(&sim, &care, error, sizeof(error)));
    CC_CHECK(sim.player.cargo[CC_GOOD_WHEAT] == 0);

    sim.horse_team[0].health = 20;
    sim.player.feed_tray_wheat = 1;
    CC_CHECK(CcSimHorseCarePreview(&sim, &offer) && offer.available);
    CC_CHECK(offer.source == CC_HORSE_CARE_TRAY && offer.cost == 4);
    CC_CHECK(CcSimApply(&sim, &care, error, sizeof(error)));
    CC_CHECK(sim.player.feed_tray_wheat == 0);

    static CcSim weekly;
    CcSimInit(&weekly, UINT32_C(0x57ab1e));
    CcSimAdvanceDays(&weekly, 5);
    CcSettlement *weekly_town = CcSimSettlementMutable(
        &weekly, weekly.player.location_id);
    CC_CHECK(weekly_town != NULL);
    weekly_town->stock[CC_GOOD_WHEAT] = 100;
    weekly.horse_team[0].health = 1;
    weekly.horse_team[0].hunger = 100;
    weekly.horse_team[0].fatigue = 100;
    CC_CHECK(CcSimHorseCarePreview(&weekly, &offer) && offer.available);
    CC_CHECK(offer.source == CC_HORSE_CARE_MARKET);
    CC_CHECK(offer.weekly_feed_due);
    int32_t weekly_wheat = weekly_town->stock[CC_GOOD_WHEAT];
    static CcSim weekly_control;
    weekly_control = weekly;
    CcSimAdvanceDays(&weekly_control, 1);
    CC_CHECK(CcSimApply(&weekly, &care, error, sizeof(error)));
    CC_CHECK(weekly.current_day == 7);
    int32_t routine_wheat = CcSimSettlement(
        &weekly_control, weekly.player.location_id)->stock[CC_GOOD_WHEAT];
    /* The day also runs ordinary town feeding and production. Care spends
       exactly one additional Wheat relative to that same day without care. */
    CC_CHECK(weekly_town->stock[CC_GOOD_WHEAT] == routine_wheat - 1);
    CC_CHECK(routine_wheat < weekly_wheat);
    CC_CHECK(weekly_town->stock[CC_GOOD_WHEAT] != weekly_wheat - 1);
    CC_CHECK(weekly.horse_team[0].health > 1);
    CC_CHECK(weekly.horse_team[0].hunger < 100);

    static CcSim journal_sim;
    CcSimInit(&journal_sim, UINT32_C(0x57ab1e));
    journal_sim.horse_team[0].health = 20;
    const char *journal_path = "/tmp/crownless-horse-care-journal.ccsave";
    (void)remove(journal_path);
    CcJournal *journal = CcJournalStart(journal_path, &journal_sim,
                                        error, sizeof(error));
    CC_CHECK(journal != NULL);
    CC_CHECK(CcJournalApply(journal, &journal_sim, &care,
                            error, sizeof(error)));
    uint64_t journal_hash = CcSimHash(&journal_sim);
    CC_CHECK(CcJournalClose(&journal, &journal_sim, error,
                             sizeof(error)));
    CC_CHECK(CcSaveRead(journal_path, &restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(&restored) == journal_hash);
    CC_CHECK(remove(journal_path) == 0);
}

int main(void)
{
    CheckSavedHorseCare();
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
    int32_t wheat_before = sim.settlements[0].stock[CC_GOOD_WHEAT];
    CcCommand breed = {
        .kind = CC_COMMAND_BREED_HORSES,
        .target_id = sim.horse_team[1].id,
        .amount = 1
    };
    CC_CHECK(CcSimApply(&sim, &breed, error, sizeof(error)));
    CC_CHECK(sim.player.coins == coins_before - 20);
    CC_CHECK(sim.settlements[0].stock[CC_GOOD_WHEAT] == wheat_before - 2);
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
    CcCommand travel = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = sim.settlements[1].id
    };
    sim.horse_team[1].pregnancy_days_remaining = 30;
    if (CcSimHorseTeamCount(&sim) > 1) {
        CC_CHECK(!CcSimApply(&sim, &travel, error, sizeof(error)));
        CC_CHECK(strstr(error, "foaling") != NULL);
    }
    /* The animal in harness is the one that can hold the carriage back. */
    sim.horse_team[0].pregnancy_days_remaining = 30;
    CC_CHECK(!CcSimApply(&sim, &travel, error, sizeof(error)));
    CC_CHECK(strstr(error, "foaling") != NULL);
    sim.horse_team[0].pregnancy_days_remaining = 0;

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

    puts("Horse stable and care tests passed");
    return 0;
}
