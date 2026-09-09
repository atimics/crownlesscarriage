#include "sim/cc_sim.h"
#include "sim/cc_archive_internal.h"
#include "persistence/cc_save.h"
#include "test_support.h"
#include <string.h>
#include <sqlite3.h>
static CcSim sim, before;
static void Fixture(void)
{
    CcSimInit(&sim, 42U); sim.settlement_count = 3; sim.route_count = 2;
    for (int i = 0; i < sim.kingdom_count; ++i) sim.kingdoms[i].monastery_patron_id = 0;
    for (int i = 0; i < 3; ++i) {
        CcSettlement *town = &sim.settlements[i];
        town->kingdom_id = sim.kingdoms[0].id; town->population = 100;
        memset(town->stock, 0, sizeof(town->stock));
        town->stock[CC_GOOD_FOOD] = 10000; town->stock[CC_GOOD_WHEAT] = 2;
        town->stock[CC_GOOD_PAPER] = town->stock[CC_GOOD_TOOLS] = 1;
        town->security = 50; town->service_mask = UINT32_C(1) << CC_SERVICE_MILL;
    }
    for (int i = 0; i < 2; ++i) {
        CcId id = sim.routes[i].id;
        sim.routes[i] = (CcRoute){.id = id, .from_id = sim.settlements[0].id,
            .to_id = sim.settlements[i+1].id, .capacity = 8, .condition = 100, .travel_days = 1};
    }
}
static CcArchiveSeatPlan Plan(CcId current)
{
    before = sim;
    CcArchiveSeatPlan plan = CcSimArchiveSeatPlan(&sim, current);
    CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
    return plan;
}
static CcArchiveSeatCandidate Candidate(int slot)
{
    before = sim;
    CcArchiveSeatCandidate candidate = CcSimArchiveSeatCandidate(&sim, sim.settlements[slot].id);
    CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
    return candidate;
}
static void CheckSavedSeat(void)
{
    CcSimInit(&sim, 42U);
    char error[256];
    CcId original = CcArchiveSeat(&sim)->id;
    CcArchiveRememberSeat(&sim);
    CC_CHECK(sim.archives.seat_id == original);
    CcSettlement *seat = CcSimSettlementMutable(&sim, original);
    seat->stock[CC_GOOD_PAPER] = 0;
    CcArchiveRememberSeat(&sim);
    int64_t failed = sim.archives.seat_failed_since_day;
    CC_CHECK(failed == sim.current_day);
    sim.current_day += 2;
    CcArchiveRememberSeat(&sim);
    CC_CHECK(sim.archives.seat_failed_since_day == failed);
    seat->stock[CC_GOOD_FOOD] = 10000; seat->stock[CC_GOOD_WHEAT] = 10;
    seat->stock[CC_GOOD_TOOLS] = 1; seat->stock[CC_GOOD_PAPER] = 1;
    seat->service_mask |= UINT32_C(1) << CC_SERVICE_MILL;
    CcArchiveRememberSeat(&sim);
    CC_CHECK(sim.archives.seat_failed_since_day == 0);
    seat->stock[CC_GOOD_PAPER] = 0;
    CcArchiveRememberSeat(&sim);
    CC_CHECK(sim.archives.seat_failed_since_day == sim.current_day);
    const char *path = "archive-seat-test.ccsave";
    CcJournal *journal = CcJournalStart(path, &sim, error, sizeof(error));
    CC_CHECK(journal != NULL);
    CC_CHECK(CcJournalAdvanceDays(journal, &sim, 7, error, sizeof(error)));
    CcJournalAbandon(&journal);
    CC_CHECK(CcSaveRead(path, &before, error, sizeof(error)));
    CC_CHECK(CcSimHash(&before) == CcSimHash(&sim));
    CC_CHECK(before.archives.seat_id == original);
    sqlite3 *database = NULL;
    CC_CHECK(sqlite3_open(path, &database) == SQLITE_OK);
    CC_CHECK(sqlite3_exec(database, "UPDATE archive_seat SET failed_since=0.5;", NULL, NULL, NULL) == SQLITE_OK);
    CC_CHECK(sqlite3_close(database) == SQLITE_OK);
    CC_CHECK(!CcSaveRead(path, &before, error, sizeof(error)));

    (void)remove(path); (void)remove("archive-seat-test.ccsave-wal"); (void)remove("archive-seat-test.ccsave-shm");
    before = sim;
    sim.archives.seat_id = sim.player.id;
    CC_CHECK(!CcSimValidate(&sim, error, sizeof(error)));
    sim = before; sim.archives.seat_failed_since_day = (int64_t)sim.current_day + 1;
    CC_CHECK(!CcSimValidate(&sim, error, sizeof(error)));
    sim = before; sim.archives.seat_id = 0; sim.archives.seat_failed_since_day = 1;
    CC_CHECK(!CcSimValidate(&sim, error, sizeof(error)));
    sim = before;
    CcTreasure treasures[CC_MAX_TREASURES];
    memcpy(treasures, sim.treasures, sizeof(treasures));
    CcSimSettlementMutable(&sim, original)->population = 0;
    CC_CHECK(CcArchiveSeat(&sim) == NULL);
    CcArchiveRememberSeat(&sim);
    CC_CHECK(sim.archives.seat_id == original);
    CC_CHECK(memcmp(treasures, sim.treasures, sizeof(treasures)) == 0);
    sim.schema_version = 87U;
    CC_CHECK(CcArchiveSeat(&sim) != NULL && CcArchiveSeat(&sim)->id != original);
}

int main(void)
{
    CheckSavedSeat();
    Fixture(); CcId current = sim.settlements[1].id;
    CcArchiveSeatCandidate c = Candidate(0);
    CC_CHECK(c.viable && c.usable_connections == 2 && c.score == 36);
    CcArchiveSeatPlan p = Plan(current);
    CC_CHECK(p.keep_current && p.selected_id == current);
    sim.settlements[0].stock[CC_GOOD_PAPER] = 16;
    p = Plan(current); CC_CHECK(p.keep_current && p.selected_id == current);
    sim.settlements[1].stock[CC_GOOD_PAPER] = 0;
    p = Plan(current); CC_CHECK(!p.keep_current && p.selected_id == sim.settlements[0].id);
    sim.settlements[0].service_mask = 0;
    p = Plan(current); CC_CHECK(p.selected_id == sim.settlements[2].id);
    sim.settlements[2].population = 0;
    p = Plan(current); CC_CHECK(p.selected_id == 0);
    Fixture(); sim.route_count = 0;
    CcId winner = Plan(0).selected_id;
    CcSettlement swap = sim.settlements[0]; sim.settlements[0] = sim.settlements[2]; sim.settlements[2] = swap;
    CC_CHECK(Plan(0).selected_id == winner);
    Fixture();
    int32_t base = Candidate(0).score;
    sim.settlements[0].stock[CC_GOOD_TOOLS] += 1; CC_CHECK(Candidate(0).score == base + 8);
    sim.settlements[0].stock[CC_GOOD_WHEAT] += 2; CC_CHECK(Candidate(0).score == base + 10);
    sim.settlements[0].security += 5; CC_CHECK(Candidate(0).score == base + 11);
    sim.routes[0].closed = true; CC_CHECK(Candidate(0).usable_connections == 2);
    sim.routes[0].condition = 0; CC_CHECK(Candidate(0).usable_connections == 1);
    sim.routes[0] = sim.routes[1]; CC_CHECK(Candidate(0).usable_connections == 1);
    sim.settlements[2].population = 0; CC_CHECK(Candidate(0).usable_connections == 0);
    Fixture();
    CcCharacter *patron = &sim.characters[0]; patron->home_settlement_id = sim.settlements[0].id;
    patron->death_day = sim.current_day + 100;
    sim.kingdoms[0].monastery_patron_id = patron->id;
    CC_CHECK(Candidate(0).patron_id == patron->id && Candidate(0).score == 56);
    patron->death_day = sim.current_day; CC_CHECK(Candidate(0).patron_id == 0);
    Fixture(); sim.settlements[0].stock[CC_GOOD_TOOLS] = 0; CC_CHECK(!Candidate(0).viable);
    Fixture(); sim.settlements[0].stock[CC_GOOD_WHEAT] = 1; CC_CHECK(!Candidate(0).viable);
    Fixture(); sim.settlements[0].stock[CC_GOOD_FOOD] = 0; CC_CHECK(!Candidate(0).viable);
    CC_CHECK(CcSimArchiveSeatPlan(NULL, 0).selected_id == 0);
    CC_CHECK(!CcSimArchiveSeatCandidate(NULL, 0).viable);
    puts("Archive seat scoring and healthy-seat stability passed.");
    return 0;
}
