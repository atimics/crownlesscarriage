#include "sim/cc_archive_recruitment.h"
#include "sim/cc_trade_path_internal.h"
#include "persistence/cc_save.h"
#include "test_support.h"
#include <stdio.h>
#include <sqlite3.h>
#include <string.h>
static CcSim sim, before, restored;
static char error[256];
static CcCharacter *person;
static void RoundTrip(void)
{
    if (!CcSimValidate(&sim, error, sizeof(error))) { fprintf(stderr, "%s\n", error); CC_CHECK(false); }
    unsigned char *bytes = NULL; size_t length = 0;
    CC_CHECK(CcSaveEncode(&sim, &bytes, &length, error, sizeof(error)));
    CC_CHECK(CcSaveDecode(bytes, length, &restored, error, sizeof(error)));
    CcSaveFreeBuffer(bytes);
#define FIELD(f) CC_CHECK(sim.archive_recruitment.f == restored.archive_recruitment.f)
    FIELD(current_id); FIELD(leg_route_id); FIELD(leg_hop_id);
    FIELD(leg_arrival_day); FIELD(provisioned_days); FIELD(arrived_day);
#undef FIELD
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));
}
static void Fixture(bool remote)
{
    CcSimInit(&sim, 42U);
    CcArchiveRecruitmentPlan p = CcSimArchiveRecruitmentPlan(&sim);
    CC_CHECK(p.gate == CC_ARCHIVE_RECRUIT_READY);
    person = NULL;
    for (int i = 0; i < sim.character_count; ++i) {
        if (sim.characters[i].id == p.person_id) person = &sim.characters[i];
        else sim.characters[i].activity = CC_CHARACTER_ACTIVITY_HIDING;
    }
    CC_CHECK(person != NULL);
    sim.archives.scribes = 0;
    person->death_day = 30000;
    person->current_settlement_id = p.seat_id;
    for (int i = 0; i < sim.route_count; ++i) sim.routes[i].travel_days = 4;
    if (remote) {
        bool found = false;
        for (int i = 0; i < sim.settlement_count; ++i) {
            sim.settlements[i].stock[CC_GOOD_WHEAT] = 1000;
            person->current_settlement_id = sim.settlements[i].id;
            p = CcSimArchiveRecruitmentPlan(&sim);
            if (p.gate == CC_ARCHIVE_RECRUIT_READY && p.travel_days >= 8) { found = true; break; }
        }
        CC_CHECK(found);
    }
    CC_CHECK(CcSimBeginArchiveRecruitment(&sim));
    RoundTrip();
}
static void AtDueDay(void)
{
    sim.current_day = sim.archive_recruitment.leg_arrival_day;
    sim.royal_trade_week = sim.current_day / 7;
}
int main(void)
{
    Fixture(false);
    CC_CHECK(CcSimAdvanceArchiveRecruitmentJourney(&sim, 99) == CC_ARCHIVE_JOURNEY_ARRIVED);
    CC_CHECK(sim.archive_recruitment.arrived_day == sim.current_day);
    CC_CHECK(sim.archive_recruitment.status == 3);
    CC_CHECK(sim.archives.scribes == 0);
    RoundTrip();
    CC_CHECK(CcSimCancelArchiveRecruitment(&sim));
    Fixture(true);
    CcId origin = person->current_settlement_id;
    int food = sim.archive_recruitment.travel_wheat;
    CC_CHECK(CcSimAdvanceArchiveRecruitmentJourney(&sim, 99) == CC_ARCHIVE_JOURNEY_DEPARTED);
    CC_CHECK(person->current_settlement_id == origin);
    CC_CHECK(person->activity == CC_CHARACTER_ACTIVITY_TRAVELLING);
    CC_CHECK(sim.archive_recruitment.leg_arrival_day == sim.current_day + 4);
    CC_CHECK(sim.archive_recruitment.travel_wheat == food - 2);
    before = sim;
    CC_CHECK(!CcSimCancelArchiveRecruitment(&sim));
    CC_CHECK(CcSimAdvanceArchiveRecruitmentJourney(&sim, 99) == CC_ARCHIVE_JOURNEY_WAIT);
    CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
    RoundTrip();
    CcId hop = sim.archive_recruitment.leg_hop_id;
    AtDueDay();
    CC_CHECK(CcSimAdvanceArchiveRecruitmentJourney(&sim, 99) == CC_ARCHIVE_JOURNEY_STOP);
    CC_CHECK(person->current_settlement_id == hop && hop != origin);
    RoundTrip();
    before = sim;
    for (int i = 0; i < sim.route_count; ++i) sim.routes[i].condition = 0;
    CC_CHECK(CcSimArchiveRecruitmentJourneyGate(&sim) == CC_ARCHIVE_RECRUIT_ROUTE);
    CcArchiveRecruitmentOrder held = sim.archive_recruitment;
    CC_CHECK(CcSimAdvanceArchiveRecruitmentJourney(&sim, 99) == CC_ARCHIVE_JOURNEY_WAIT);
    CC_CHECK(memcmp(&held, &sim.archive_recruitment, sizeof(held)) == 0);
    sim = before;
    int refund = sim.archive_recruitment.travel_wheat;
    int town_food = CcSimSettlement(&sim, hop)->stock[CC_GOOD_WHEAT];
    CC_CHECK(CcSimCancelArchiveRecruitment(&sim));
    CC_CHECK(CcSimSettlement(&sim, hop)->stock[CC_GOOD_WHEAT] == town_food + refund);
    CC_CHECK(person->current_settlement_id == hop);
    sim = before;
    sim.archive_recruitment.travel_wheat = 0;
    CC_CHECK(CcSimArchiveRecruitmentJourneyGate(&sim) == CC_ARCHIVE_RECRUIT_TRAVEL_FOOD);
    sim = before;
    for (int leg = 0; leg < CC_MAX_SETTLEMENTS && sim.archive_recruitment.status != 3; ++leg) {
        CC_CHECK(CcSimAdvanceArchiveRecruitmentJourney(&sim, 99) == CC_ARCHIVE_JOURNEY_DEPARTED);
        RoundTrip(); AtDueDay();
        CcArchiveJourneyStep step = CcSimAdvanceArchiveRecruitmentJourney(&sim, 99);
        CC_CHECK(step == CC_ARCHIVE_JOURNEY_STOP || step == CC_ARCHIVE_JOURNEY_ARRIVED);
        RoundTrip();
    }
    CC_CHECK(sim.archive_recruitment.status == 3);
    CC_CHECK(person->current_settlement_id == sim.archive_recruitment.seat_id);
    CC_CHECK(sim.archive_recruitment.travel_wheat == 0);
    CC_CHECK(sim.archive_recruitment.purse == 50 && sim.archive_recruitment.paper == 2);
    Fixture(true);
    CC_CHECK(CcSimAdvanceArchiveRecruitmentJourney(&sim, 99) == CC_ARCHIVE_JOURNEY_DEPARTED);
    person->death_day = sim.current_day;
    CC_CHECK(CcSimAdvanceArchiveRecruitmentJourney(&sim, 99) == CC_ARCHIVE_JOURNEY_FAILED);
    CC_CHECK(sim.archive_recruitment.travel_wheat == 0);
    CC_CHECK(CcSimCancelArchiveRecruitment(&sim));
    Fixture(true);
    CC_CHECK(CcSimAdvanceArchiveRecruitmentJourney(&sim, 99) == CC_ARCHIVE_JOURNEY_DEPARTED);
    before = sim;
#define MUTATE(f, value) do { \
    sim = before; sim.archive_recruitment.f = (value); \
    CC_CHECK(CcSimHash(&sim) != CcSimHash(&before)); \
} while (0)
    MUTATE(current_id, 0);
    MUTATE(leg_route_id, 0);
    MUTATE(leg_hop_id, 0);
    MUTATE(leg_arrival_day, before.archive_recruitment.leg_arrival_day + 1); RoundTrip();
    MUTATE(provisioned_days, before.archive_recruitment.provisioned_days + 1); RoundTrip();
    MUTATE(arrived_day, sim.current_day);
    CC_CHECK(!CcSimValidate(&sim, error, sizeof(error)));
#undef MUTATE
    sim = before;
    CcRoute *dangerous = NULL;
    for (int i = 0; i < sim.route_count; ++i)
        if (sim.routes[i].id == sim.archive_recruitment.leg_route_id) dangerous = &sim.routes[i];
    CC_CHECK(dangerous != NULL);
    dangerous->security = 0; dangerous->closed = true;
    CC_CHECK(CcSimRouteDanger(&sim, dangerous->id) >= 5);
    AtDueDay();
    CC_CHECK(CcSimAdvanceArchiveRecruitmentJourney(&sim, 0) == CC_ARCHIVE_JOURNEY_FAILED);
    CC_CHECK(sim.archive_recruitment.status == 4 && sim.archive_recruitment.travel_wheat == 0);
    CC_CHECK(person->activity == CC_CHARACTER_ACTIVITY_RECOVERING);
    CC_CHECK(CcSimArchiveRecruitmentJourneyGate(&sim) == CC_ARCHIVE_RECRUIT_TRAVEL_FOOD);
    RoundTrip();
    CC_CHECK(CcSimCancelArchiveRecruitment(&sim));
    Fixture(true);
    const char *path = "archive-journey-journal.ccsave";
    (void)remove(path);
    CcJournal *journal = CcJournalStart(path, &sim, error, sizeof(error));
    CC_CHECK(journal != NULL);
    CC_CHECK(CcJournalAdvanceDays(journal, &sim, 1, error, sizeof(error)));
    CC_CHECK(sim.archive_recruitment.status == 2);
    CC_CHECK(CcJournalAdvanceDays(journal, &sim, 4, error, sizeof(error)));
    CC_CHECK(sim.archive_recruitment.current_id != sim.archive_recruitment.origin_id);
    CcJournalAbandon(&journal);
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));
    (void)remove(path);
    Fixture(true);
    CC_CHECK(CcSimAdvanceArchiveRecruitmentJourney(&sim, 99) == CC_ARCHIVE_JOURNEY_DEPARTED);
    CC_CHECK(CcSaveWrite(path, &sim, error, sizeof(error)));
    sqlite3 *database = NULL;
    CC_CHECK(sqlite3_open(path, &database) == SQLITE_OK);
    CC_CHECK(sqlite3_exec(database, "UPDATE archive_recruitment_journey SET leg_arrival_day=4294967297;",
        NULL, NULL, NULL) == SQLITE_OK);
    CC_CHECK(sqlite3_close(database) == SQLITE_OK);
    CC_CHECK(!CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(strstr(error, "integer") != NULL);
    (void)remove(path);
    puts("Recruitment uses saved road legs, held food, actual arrival and journal replay.");
    return 0;
}
