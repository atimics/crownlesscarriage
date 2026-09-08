#include "sim/cc_archive_recruitment.h"
#include "persistence/cc_save.h"
#include "test_support.h"
#include <stdio.h>
#include <sqlite3.h>
#include <string.h>
static CcSim sim, before, restored, baseline;
static char error[256];
static int checks;
static bool hash_checks = true;
static void Valid(void)
{
    if (!CcSimValidate(&sim, error, sizeof(error))) {
        fprintf(stderr, "Order fixture: %s\n", error); CC_CHECK(false);
    }
}
static void RoundTrip(void)
{
    Valid();
    unsigned char *bytes = NULL; size_t length = 0;
    CC_CHECK(CcSaveEncode(&sim, &bytes, &length, error, sizeof(error)));
    CC_CHECK(CcSaveDecode(bytes, length, &restored, error, sizeof(error)));
    CcSaveFreeBuffer(bytes);
    CC_CHECK(sim.archive_recruitment.status == restored.archive_recruitment.status);
    CC_CHECK(sim.archive_recruitment.person_id == restored.archive_recruitment.person_id);
    CC_CHECK(sim.archive_recruitment.trainer_id == restored.archive_recruitment.trainer_id);
    CC_CHECK(sim.archive_recruitment.seat_id == restored.archive_recruitment.seat_id);
    CC_CHECK(sim.archive_recruitment.origin_id == restored.archive_recruitment.origin_id);
    CC_CHECK(sim.archive_recruitment.first_route_id == restored.archive_recruitment.first_route_id);
    CC_CHECK(sim.archive_recruitment.first_hop_id == restored.archive_recruitment.first_hop_id);
    CC_CHECK(sim.archive_recruitment.donor_ids[0] == restored.archive_recruitment.donor_ids[0]);
    CC_CHECK(sim.archive_recruitment.donor_ids[1] == restored.archive_recruitment.donor_ids[1]);
    CC_CHECK(sim.archive_recruitment.patron_ids[0] == restored.archive_recruitment.patron_ids[0]);
    CC_CHECK(sim.archive_recruitment.patron_ids[1] == restored.archive_recruitment.patron_ids[1]);
    CC_CHECK(sim.archive_recruitment.donor_shares[0] == restored.archive_recruitment.donor_shares[0]);
    CC_CHECK(sim.archive_recruitment.donor_shares[1] == restored.archive_recruitment.donor_shares[1]);
    CC_CHECK(sim.archive_recruitment.purse == restored.archive_recruitment.purse);
    CC_CHECK(sim.archive_recruitment.wheat == restored.archive_recruitment.wheat);
    CC_CHECK(sim.archive_recruitment.paper == restored.archive_recruitment.paper);
    CC_CHECK(sim.archive_recruitment.tools == restored.archive_recruitment.tools);
    CC_CHECK(sim.archive_recruitment.travel_wheat == restored.archive_recruitment.travel_wheat);
    CC_CHECK(sim.archive_recruitment.start_day == restored.archive_recruitment.start_day);
    CC_CHECK(sim.archive_recruitment.training_days == restored.archive_recruitment.training_days);
    CC_CHECK(sim.archive_recruitment.trainer_days == restored.archive_recruitment.trainer_days);
    CC_CHECK(sim.archive_recruitment.arrival_estimate == restored.archive_recruitment.arrival_estimate);
    CC_CHECK(sim.archive_recruitment.ready_estimate == restored.archive_recruitment.ready_estimate);
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));
}
static CcMoney Money(void)
{
    CcMoney total = sim.iron_ledger_reserve + sim.archive_recruitment.purse;
    for (int i = 0; i < sim.kingdom_count; ++i) total += sim.kingdoms[i].treasury;
    return total;
}
static int64_t Stock(CcGood good)
{
    int64_t total = good == CC_GOOD_WHEAT ? sim.archive_recruitment.wheat +
        sim.archive_recruitment.travel_wheat : good == CC_GOOD_PAPER ?
        sim.archive_recruitment.paper : sim.archive_recruitment.tools;
    for (int i = 0; i < sim.settlement_count; ++i) total += sim.settlements[i].stock[good];
    return total;
}
static void Reserve(void)
{
    CcMoney money = Money();
    int64_t wheat = Stock(CC_GOOD_WHEAT), paper = Stock(CC_GOOD_PAPER), tools = Stock(CC_GOOD_TOOLS);
    before = sim;
    CC_CHECK(CcSimBeginArchiveRecruitment(&sim));
    CC_CHECK(Money() == money && Stock(CC_GOOD_WHEAT) == wheat);
    CC_CHECK(Stock(CC_GOOD_PAPER) == paper && Stock(CC_GOOD_TOOLS) == tools);
    CC_CHECK(memcmp(sim.characters, before.characters, sizeof(sim.characters)) == 0);
    CC_CHECK(sim.archives.scribes == before.archives.scribes);
    CC_CHECK(sim.archive_recruitment.status == 1 && sim.archive_recruitment.purse == 50);
    before = sim;
    CC_CHECK(!CcSimBeginArchiveRecruitment(&sim));
    CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
    CC_CHECK(CcSimArchiveRecruitmentPlan(&sim).gate == CC_ARCHIVE_RECRUIT_BUSY);
    RoundTrip();
}
static void Cancel(void)
{
    CcMoney money = Money();
    int64_t wheat = Stock(CC_GOOD_WHEAT), paper = Stock(CC_GOOD_PAPER), tools = Stock(CC_GOOD_TOOLS);
    CC_CHECK(CcSimCancelArchiveRecruitment(&sim));
    CC_CHECK(Money() == money && Stock(CC_GOOD_WHEAT) == wheat);
    CC_CHECK(Stock(CC_GOOD_PAPER) == paper && Stock(CC_GOOD_TOOLS) == tools);
    CC_CHECK(sim.archive_recruitment.status == 0);
    before = sim;
    CC_CHECK(!CcSimCancelArchiveRecruitment(&sim));
    CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
    RoundTrip();
}
static void JournalCommands(void)
{
    CcSimInit(&sim, 42U);
    CcArchiveRecruitmentPlan plan = CcSimArchiveRecruitmentPlan(&sim);
    CC_CHECK(plan.gate == CC_ARCHIVE_RECRUIT_READY);
    CcCommand reserve = {.kind = CC_COMMAND_RESERVE_ARCHIVE_RECRUITMENT, .target_id = plan.person_id};
    before = sim;
    CC_CHECK(!CcSimApply(&sim, &reserve, error, sizeof(error)));
    CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
    sim.player.location_id = sim.carriage.location_id = plan.seat_id;
    CcCommand stale = reserve; stale.target_id = sim.characters[0].id;
    before = sim;
    CC_CHECK(!CcSimApply(&sim, &stale, error, sizeof(error)));
    CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
    sim.journey.active = true; before = sim;
    CC_CHECK(!CcSimApply(&sim, &reserve, error, sizeof(error)));
    CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
    sim.journey.active = false;
    CcMoney money = Money();
    int64_t wheat = Stock(CC_GOOD_WHEAT), paper = Stock(CC_GOOD_PAPER), tools = Stock(CC_GOOD_TOOLS);
    const char *path = "archive-order-commands.ccsave";
    (void)remove(path);
    CcJournal *journal = CcJournalStart(path, &sim, error, sizeof(error));
    CC_CHECK(journal != NULL);
    CC_CHECK(CcJournalApply(journal, &sim, &reserve, error, sizeof(error)));
    CC_CHECK(sim.archive_recruitment.person_id == plan.person_id);
    CC_CHECK(Money() == money && Stock(CC_GOOD_WHEAT) == wheat);
    CC_CHECK(Stock(CC_GOOD_PAPER) == paper && Stock(CC_GOOD_TOOLS) == tools);
    const CcEvent *event = CcSimRecentEvent(&sim, 0);
    CC_CHECK(event->kind == CC_EVENT_CHARACTER_INTERACTION && event->actor_id == sim.player.id);
    CC_CHECK(event->target_id == plan.person_id && event->location_id == plan.seat_id);
    before = sim;
    CC_CHECK(!CcJournalApply(journal, &sim, &reserve, error, sizeof(error)));
    CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
    CcJournalAbandon(&journal);
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));
    CC_CHECK(restored.archive_recruitment.purse == 50);
    journal = CcJournalResume(path, &sim, error, sizeof(error));
    CC_CHECK(journal != NULL);
    CcCommand cancel = {.kind = CC_COMMAND_CANCEL_ARCHIVE_RECRUITMENT, .target_id = plan.person_id};
    stale = cancel; stale.target_id = sim.characters[0].id; before = sim;
    CC_CHECK(!CcJournalApply(journal, &sim, &stale, error, sizeof(error)));
    CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
    CC_CHECK(CcJournalApply(journal, &sim, &cancel, error, sizeof(error)));
    CC_CHECK(Money() == money && Stock(CC_GOOD_WHEAT) == wheat);
    CC_CHECK(Stock(CC_GOOD_PAPER) == paper && Stock(CC_GOOD_TOOLS) == tools);
    before = sim;
    CC_CHECK(!CcJournalApply(journal, &sim, &cancel, error, sizeof(error)));
    CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
    CcJournalAbandon(&journal);
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));
    CC_CHECK(restored.archive_recruitment.status == 0);
    (void)remove(path);
}
#define FIELD(name,value) do { \
    sim = baseline; sim.archive_recruitment.name = (value); \
    CC_CHECK(sim.archive_recruitment.name != baseline.archive_recruitment.name); \
    if (hash_checks) CC_CHECK(CcSimHash(&sim) != CcSimHash(&baseline)); \
    RoundTrip(); ++checks; \
} while (0)
int main(int argc, char **argv)
{
    CC_CHECK(argc == 1 || (argc == 2 && strcmp(argv[1], "--save-only") == 0));
    hash_checks = argc == 1;
    CcSimInit(&sim, 42U);
    Reserve();
    const char *path = "archive-order-journal.ccsave";
    (void)remove(path);
    CcJournal *journal = CcJournalStart(path, &sim, error, sizeof(error));
    CC_CHECK(journal != NULL);
    CC_CHECK(CcJournalAdvanceDays(journal, &sim, 1, error, sizeof(error)));
    CcJournalAbandon(&journal);
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));
    (void)remove(path);
    baseline = sim;
    baseline.archive_recruitment.status = 1;
    baseline.archive_recruitment.current_id = 0;
    baseline.archive_recruitment.arrived_day = 0;
    baseline.archive_recruitment.leg_route_id = 0;
    baseline.archive_recruitment.leg_hop_id = 0;
    baseline.archive_recruitment.leg_arrival_day = 0;
    baseline.archive_recruitment.provisioned_days = 0;
    baseline.archive_recruitment.donor_ids[0] = sim.kingdoms[0].id;
    baseline.archive_recruitment.donor_ids[1] = sim.kingdoms[1].id;
    baseline.archive_recruitment.patron_ids[0] = sim.characters[0].id;
    baseline.archive_recruitment.patron_ids[1] = sim.characters[1].id;
    baseline.archive_recruitment.donor_shares[0] = 20;
    baseline.archive_recruitment.donor_shares[1] = 20;
    FIELD(person_id, baseline.characters[0].id);
    FIELD(trainer_id, baseline.characters[1].id);
    FIELD(seat_id, baseline.settlements[0].id);
    /* The planning seed picks a different origin now that the cast is larger;
       choose a settlement that differs from whatever the baseline used. */
    FIELD(origin_id, baseline.archive_recruitment.origin_id ==
                          baseline.settlements[0].id ?
                      baseline.settlements[1].id :
                      baseline.settlements[0].id);
    FIELD(first_route_id, 0);
    FIELD(first_hop_id, 0);
    FIELD(donor_ids[0], baseline.kingdoms[2].id);
    FIELD(donor_ids[1], baseline.kingdoms[2].id);
    FIELD(patron_ids[0], baseline.characters[2].id);
    FIELD(patron_ids[1], baseline.characters[2].id);
    FIELD(donor_shares[0], 19);
    FIELD(donor_shares[1], 19);
    FIELD(purse, 49);
    FIELD(wheat, 3);
    FIELD(paper, 1);
    FIELD(tools, 0);
    FIELD(travel_wheat, baseline.archive_recruitment.travel_wheat + 1);
    FIELD(start_day, 2);
    FIELD(training_days, 28);
    FIELD(trainer_days, 28);
    FIELD(arrival_estimate, baseline.archive_recruitment.arrival_estimate + 1);
    FIELD(ready_estimate, baseline.archive_recruitment.ready_estimate + 1);
    sim = baseline; sim.archive_recruitment.status = 0;
    CC_CHECK(CcSimHash(&sim) != CcSimHash(&baseline));
    CC_CHECK(!CcSimValidate(&sim, error, sizeof(error)));
    sim = baseline; sim.archive_recruitment.status = 2;
    CC_CHECK(!CcSimValidate(&sim, error, sizeof(error)));
    CcSimInit(&sim, 42U);
    sim.current_day = 1827; sim.royal_trade_week = sim.current_day / 7;
    sim.archives.dead_since_day = 1; sim.archives.scribes = 0;
    sim.iron_ledger_reserve = 0;
    for (int i = 0; i < sim.settlement_count; ++i) {
        sim.settlements[i].stock[CC_GOOD_FOOD] = 10000;
        sim.settlements[i].stock[CC_GOOD_WHEAT] = 100;
        sim.settlements[i].stock[CC_GOOD_PAPER] = 10;
        sim.settlements[i].stock[CC_GOOD_TOOLS] = 10;
    }
    for (int i = 0; i < sim.kingdom_count; ++i) sim.kingdoms[i].treasury = 800;
    for (int i = 0; i < sim.character_count; ++i)
        if (sim.characters[i].death_day <= sim.current_day)
            sim.characters[i].death_day = sim.current_day + 100;
    CC_CHECK(CcSimArchiveRecruitmentPlan(&sim).gate == CC_ARCHIVE_RECRUIT_READY);
    Reserve();
    CC_CHECK(sim.archive_recruitment.donor_shares[0] == 50);
    CC_CHECK(sim.archive_recruitment.patron_ids[0] != 0);
    before = sim;
    CcSimAdvanceDays(&sim, 7);
    CC_CHECK(sim.archives.scribes == 0);
    CC_CHECK(sim.archive_recruitment.purse == 50);
    RoundTrip();
    sim = before;
    CcId donor = sim.archive_recruitment.donor_ids[0];
    Cancel();
    for (int i = 0; i < sim.kingdom_count; ++i)
        if (sim.kingdoms[i].id == donor) CC_CHECK(sim.kingdoms[i].treasury == 800);
    CcSimInit(&sim, 42U); Reserve();
    CcSettlement *seat = CcSimSettlementMutable(&sim, sim.archive_recruitment.seat_id);
    int32_t old_paper = seat->stock[CC_GOOD_PAPER];
    seat->stock[CC_GOOD_PAPER] = CC_SIM_MAX_UNITS;
    before = sim;
    CC_CHECK(!CcSimCancelArchiveRecruitment(&sim));
    CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
    seat->stock[CC_GOOD_PAPER] = old_paper;
    Cancel();
    CcSimInit(&sim, 42U); Reserve();
    CcId original_person = sim.archive_recruitment.person_id;
    for (int i = 0; i < sim.character_count; ++i)
        if (sim.characters[i].id == original_person) sim.characters[i].death_day = sim.current_day + 1;
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(CcSimCharacter(&sim, original_person) == NULL);
    RoundTrip();
    Cancel();
    CcSimInit(&sim, 42U); Reserve();
    CC_CHECK(CcSaveWrite(path, &sim, error, sizeof(error)));
    sqlite3 *database = NULL;
    CC_CHECK(sqlite3_open(path, &database) == SQLITE_OK);
    CC_CHECK(sqlite3_exec(database, "UPDATE archive_recruitment SET status=4294967297;",
        NULL, NULL, NULL) == SQLITE_OK);
    CC_CHECK(sqlite3_close(database) == SQLITE_OK);
    CC_CHECK(!CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(strstr(error, "integer") != NULL);
    (void)remove(path);
    CcSimInit(&sim, 42U); sim.iron_ledger_reserve = 0; before = sim;
    CC_CHECK(!CcSimBeginArchiveRecruitment(&sim));
    CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
    JournalCommands();
    printf("Verified %d independent order fields, conserved reservations and journal recovery.\n", checks);
    return 0;
}
