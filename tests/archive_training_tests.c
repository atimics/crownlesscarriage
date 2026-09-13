#include "sim/cc_archive_recruitment.h"
#include "persistence/cc_save.h"
#include "test_support.h"
#include <stdio.h>
#include <sqlite3.h>
#include <string.h>
static CcSim sim, before, restored;
static CcCharacter *person, *trainer;
static char error[256];
static void RoundTrip(void)
{
    if (!CcSimValidate(&sim, error, sizeof(error))) { fprintf(stderr, "%s\n", error); CC_CHECK(false); }
    unsigned char *bytes = NULL; size_t length = 0;
    CC_CHECK(CcSaveEncode(&sim, &bytes, &length, error, sizeof(error)));
    CC_CHECK(CcSaveDecode(bytes, length, &restored, error, sizeof(error)));
    CcSaveFreeBuffer(bytes);
#define FIELD(f) CC_CHECK(sim.archive_recruitment.f == restored.archive_recruitment.f)
    FIELD(labor_days); FIELD(trainer_labor_days); FIELD(last_work_day); FIELD(wages_paid);
#undef FIELD
    CC_CHECK(sim.archive_training_week == restored.archive_training_week);
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));
}
static bool FreePerson(CcCharacter *p)
{
    if (p == person || CcCharacterAgeYears(&sim, p) < 16) return false;
    for (int i = 0; i < sim.kingdom_count; ++i)
        if (sim.kingdoms[i].ruler_character_id == p->id || sim.kingdoms[i].monastery_patron_id == p->id) return false;
    for (int i = 0; i < sim.situation_count; ++i)
        if (sim.situations[i].status == CC_SITUATION_ACTIVE &&
            (sim.situations[i].sponsor_character_id == p->id || sim.situations[i].affected_character_id == p->id ||
             sim.situations[i].witness_character_id == p->id)) return false;
    return true;
}
static void Fixture(bool apprenticeship)
{
    CcSimInit(&sim, 42U);
    CcArchiveRecruitmentPlan p = CcSimArchiveRecruitmentPlan(&sim);
    CC_CHECK(p.gate == CC_ARCHIVE_RECRUIT_READY);
    person = trainer = NULL;
    for (int i = 0; i < sim.character_count; ++i) {
        if (sim.characters[i].id == p.person_id) person = &sim.characters[i];
        else sim.characters[i].activity = CC_CHARACTER_ACTIVITY_HIDING;
    }
    CC_CHECK(person != NULL);
    person->current_settlement_id = p.seat_id;
    person->death_day = 30000;
    sim.archives.scribes = apprenticeship ? 1 : 0;
    sim.iron_ledger_reserve = 150;
    if (apprenticeship) {
        person->occupation = CC_OCCUPATION_FARMER;
        for (int i = 0; i < sim.character_count; ++i)
            if (FreePerson(&sim.characters[i])) { trainer = &sim.characters[i]; break; }
        CC_CHECK(trainer != NULL);
        trainer->occupation = CC_OCCUPATION_SCRIBE;
        trainer->activity = CC_CHARACTER_ACTIVITY_WORKING;
        trainer->current_settlement_id = p.seat_id;
        trainer->death_day = 30000;
    }
    for (int i = 0; i < sim.settlement_count; ++i) {
        sim.settlements[i].stock[CC_GOOD_FOOD] = 10000;
        sim.settlements[i].stock[CC_GOOD_WHEAT] = 1000;
        sim.settlements[i].stock[CC_GOOD_PAPER] = 10;
        sim.settlements[i].stock[CC_GOOD_TOOLS] = 10;
    }
    CC_CHECK(CcSimBeginArchiveRecruitment(&sim));
    CC_CHECK(CcSimAdvanceArchiveRecruitmentJourney(&sim, 99) == CC_ARCHIVE_JOURNEY_ARRIVED);
    RoundTrip();
}
static CcArchiveTrainingStep Day(void)
{
    sim.current_day += 1; sim.royal_trade_week = sim.current_day / 7;
    return CcSimAdvanceArchiveRecruitmentTraining(&sim);
}
static CcMoney Money(void)
{
    CcMoney total = sim.iron_ledger_reserve + sim.archive_recruitment.purse;
    for (int i = 0; i < sim.kingdom_count; ++i) total += sim.kingdoms[i].treasury;
    for (int i = 0; i < sim.character_count; ++i) total += sim.characters[i].travel_coins;
    return total;
}
int main(void)
{
    Fixture(false);
    before = sim;
    CC_CHECK(CcSimAdvanceArchiveRecruitmentTraining(&sim) == CC_ARCHIVE_TRAINING_WAIT);
    CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
    CcMoney money = Money(), purse = person->travel_coins;
    for (int i = 0; i < 6; ++i) CC_CHECK(Day() == CC_ARCHIVE_TRAINING_WORKED);
    CC_CHECK(person->travel_coins == purse && sim.archive_recruitment.wages_paid == 0);
    before = sim;
    person->travel_coins = CC_SIM_MAX_MONEY;
    CC_CHECK(Day() == CC_ARCHIVE_TRAINING_WAIT);
    CC_CHECK(CcSimArchiveRecruitmentTrainingGate(&sim) == CC_ARCHIVE_RECRUIT_FUNDS);
    person->travel_coins = purse;
    CC_CHECK(CcSimAdvanceArchiveRecruitmentTraining(&sim) == CC_ARCHIVE_TRAINING_COMPLETE);
    CC_CHECK(Money() == money && person->travel_coins == purse + 50);
    CC_CHECK(sim.archive_recruitment.labor_days == 7 && sim.archive_recruitment.trainer_labor_days == 0);
    CC_CHECK(sim.archive_recruitment.wheat == 2 && sim.archive_recruitment.paper == 1 && sim.archive_recruitment.tools == 1);
    CC_CHECK(sim.archives.scribes == 0);
    RoundTrip();
    before = sim;
    CC_CHECK(CcSimAdvanceArchiveRecruitmentTraining(&sim) == CC_ARCHIVE_TRAINING_WAIT);
    CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
#define MUTATE(f, value) do { \
    sim = before; sim.archive_recruitment.f = (value); \
    CC_CHECK(CcSimHash(&sim) != CcSimHash(&before)); \
    CC_CHECK(!CcSimValidate(&sim, error, sizeof(error))); \
} while (0)
    MUTATE(labor_days, 6);
    MUTATE(trainer_labor_days, 1);
    MUTATE(last_work_day, 0);
    MUTATE(wages_paid, 0);
#undef MUTATE
    sim = before;
    sim.archive_training_week = 1;
    CC_CHECK(CcSimHash(&sim) != CcSimHash(&before));
    RoundTrip();
    sim = before;
    CC_CHECK(CcSimCancelArchiveRecruitment(&sim));
    CC_CHECK(Money() == money && person->travel_coins == purse + 50);
    Fixture(true);
    money = Money();
    CcCharacterRole role = person->role;
    CC_CHECK(CcSimArchiveWorkPlan(&sim).eligible_scribes == 1);
    CC_CHECK(Day() == CC_ARCHIVE_TRAINING_WORKED);
    CC_CHECK(sim.archive_recruitment.wheat == 14 && sim.archive_recruitment.paper == 4);
    CC_CHECK(CcSimArchiveWorkPlan(&sim).eligible_scribes == 0);
    RoundTrip();
    before = sim;
    CC_CHECK(CcSimAdvanceArchiveRecruitmentTraining(&sim) == CC_ARCHIVE_TRAINING_WAIT);
    CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
    trainer->activity = CC_CHARACTER_ACTIVITY_RECOVERING;
    CC_CHECK(Day() == CC_ARCHIVE_TRAINING_WAIT);
    CC_CHECK(CcSimArchiveRecruitmentTrainingGate(&sim) == CC_ARCHIVE_RECRUIT_TRAINER);
    CC_CHECK(sim.archive_recruitment.labor_days == 1 && sim.archive_recruitment.trainer_labor_days == 1);
    trainer->activity = CC_CHARACTER_ACTIVITY_WORKING;
    int held_tools = sim.archive_recruitment.tools;
    sim.archive_recruitment.tools = 0;
    CC_CHECK(CcSimArchiveRecruitmentTrainingGate(&sim) == CC_ARCHIVE_RECRUIT_MATERIALS);
    sim.archive_recruitment.tools = held_tools;
    for (int i = 1; i < 28; ++i) {
        CC_CHECK(Day() == (i == 27 ? CC_ARCHIVE_TRAINING_COMPLETE : CC_ARCHIVE_TRAINING_WORKED));
        RoundTrip();
    }
    CC_CHECK(person->occupation == CC_OCCUPATION_SCRIBE && person->role == role);
    CC_CHECK(sim.archive_recruitment.labor_days == 28 && sim.archive_recruitment.trainer_labor_days == 28);
    CC_CHECK(sim.archive_recruitment.wheat == 2 && sim.archive_recruitment.paper == 1);
    CC_CHECK(Money() == money);
    CC_CHECK(CcSimCancelArchiveRecruitment(&sim));
    CC_CHECK(CcSimArchiveWorkPlan(&sim).eligible_scribes == 0);
    RoundTrip();
    sim.current_day += 7; sim.royal_trade_week = sim.current_day / 7;
    CC_CHECK(CcSimArchiveWorkPlan(&sim).eligible_scribes == 1);
    Fixture(true);
    CC_CHECK(Day() == CC_ARCHIVE_TRAINING_WORKED);
    person->death_day = sim.current_day;
    CC_CHECK(CcSimAdvanceArchiveRecruitmentTraining(&sim) == CC_ARCHIVE_TRAINING_FAILED);
    CC_CHECK(sim.archive_recruitment.wages_paid == 0);
    CC_CHECK(CcSimCancelArchiveRecruitment(&sim));
    Fixture(true);
    CC_CHECK(Day() == CC_ARCHIVE_TRAINING_WORKED);
    CcId teacher_id = trainer->id;
    trainer->death_day = sim.current_day + 1;
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(CcSimCharacter(&sim, teacher_id) == NULL);
    CC_CHECK(sim.archive_recruitment.trainer_id == teacher_id);
    CC_CHECK(sim.archive_recruitment.labor_days == 1);
    CC_CHECK(CcSimArchiveRecruitmentTrainingGate(&sim) == CC_ARCHIVE_RECRUIT_TRAINER);
    RoundTrip();
    CC_CHECK(CcSimCancelArchiveRecruitment(&sim));
    Fixture(false);
    CC_CHECK(CcSimCancelArchiveRecruitment(&sim));
    sim.current_day = 1827; sim.royal_trade_week = sim.current_day / 7;
    sim.archives.dead_since_day = 1; sim.iron_ledger_reserve = 0;
    for (int i = 0; i < sim.kingdom_count; ++i) sim.kingdoms[i].treasury = 800;
    for (int i = 0; i < sim.character_count; ++i)
        if (sim.characters[i].death_day <= sim.current_day) sim.characters[i].death_day = sim.current_day + 100;
    CC_CHECK(CcSimBeginArchiveRecruitment(&sim));
    CC_CHECK(sim.archive_recruitment.donor_shares[0] == 50);
    CC_CHECK(CcSimAdvanceArchiveRecruitmentJourney(&sim, 99) == CC_ARCHIVE_JOURNEY_ARRIVED);
    money = Money();
    CcId donor = sim.archive_recruitment.donor_ids[0];
    for (int i = 0; i < 7; ++i)
        CC_CHECK(Day() == (i == 6 ? CC_ARCHIVE_TRAINING_COMPLETE : CC_ARCHIVE_TRAINING_WORKED));
    CC_CHECK(sim.archive_recruitment.donor_shares[0] == 50 && sim.archive_recruitment.purse == 0);
    RoundTrip();
    CC_CHECK(CcSimCancelArchiveRecruitment(&sim));
    CC_CHECK(Money() == money);
    for (int i = 0; i < sim.kingdom_count; ++i)
        if (sim.kingdoms[i].id == donor) CC_CHECK(sim.kingdoms[i].treasury == 750);
    Fixture(true);
    const char *path = "archive-training-journal.ccsave";
    (void)remove(path);
    CcJournal *journal = CcJournalStart(path, &sim, error, sizeof(error));
    CC_CHECK(journal != NULL);
    CC_CHECK(CcJournalAdvanceDays(journal, &sim, 28, error, sizeof(error)));
    CC_CHECK(sim.archive_recruitment.status == 5 && sim.archive_recruitment.wages_paid == 50);
    CcJournalAbandon(&journal);
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));
    (void)remove(path);
    Fixture(true);
    CC_CHECK(Day() == CC_ARCHIVE_TRAINING_WORKED);
    CC_CHECK(CcSaveWrite(path, &sim, error, sizeof(error)));
    sqlite3 *database = NULL;
    CC_CHECK(sqlite3_open(path, &database) == SQLITE_OK);
    CC_CHECK(sqlite3_exec(database, "UPDATE archive_recruitment_training SET labor_days=4294967297;", NULL, NULL, NULL) == SQLITE_OK);
    CC_CHECK(sqlite3_close(database) == SQLITE_OK);
    CC_CHECK(!CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(strstr(error, "integer") != NULL);
    (void)remove(path);
    puts("Training consumes work days and supplies, pays the named recruit once, and survives replay.");
    return 0;
}
