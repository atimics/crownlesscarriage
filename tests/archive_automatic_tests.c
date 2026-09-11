#include "sim/cc_archive_recruitment.h"
#include "sim/cc_archive_staff.h"
#include "persistence/cc_save.h"
#include "test_support.h"
#include <stdio.h>
#include <string.h>
static CcSim sim, before, restored;
static CcId recruit, seat_id;
static char error[256];
static void Fixture(bool recovery, bool remote)
{
    CcSimInit(&sim, 42U);
    CcArchiveRecruitmentPlan p = CcSimArchiveRecruitmentPlan(&sim);
    CC_CHECK(p.gate == CC_ARCHIVE_RECRUIT_READY);
    recruit = p.person_id; seat_id = p.seat_id;
    sim.current_day = recovery ? 1827 : 7;
    sim.royal_trade_week = sim.current_day / 7;
    sim.archives.scribes = 0; sim.archives.dead_since_day = recovery ? 1 : 7;
    sim.iron_ledger_reserve = recovery ? 0 : 50;
    for (int i = 0; i < sim.character_count; ++i) {
        if (sim.characters[i].death_day <= sim.current_day) sim.characters[i].death_day = sim.current_day + 100;
        if (sim.characters[i].id == recruit) {
            sim.characters[i].death_day = sim.current_day + 1000;
            if (!remote) sim.characters[i].current_settlement_id = seat_id;
        } else sim.characters[i].activity = CC_CHARACTER_ACTIVITY_HIDING;
    }
    for (int i = 0; i < sim.kingdom_count; ++i) sim.kingdoms[i].treasury = recovery ? 800 : 0;
    for (int i = 0; i < sim.settlement_count; ++i) {
        sim.settlements[i].stock[CC_GOOD_FOOD] = 10000;
        sim.settlements[i].stock[CC_GOOD_WHEAT] = 1000;
        sim.settlements[i].stock[CC_GOOD_PAPER] = 10;
        sim.settlements[i].stock[CC_GOOD_TOOLS] = 10;
        sim.settlements[i].stock[CC_GOOD_GOLD] = 100;
        sim.settlements[i].stock[CC_GOOD_GEMS] = 100;
    }
    CC_CHECK(sim.treasure_count < CC_MAX_TREASURES);
    CcTreasure *book = &sim.treasures[sim.treasure_count++];
    *book = (CcTreasure){.id = CcMakeId(CC_ENTITY_TREASURE, sim.next_entity_serial++),
        .owner_id = seat_id, .location_id = seat_id, .maker_settlement_id = seat_id,
        .gold_content = 1, .gem_content = 1, .craft_work = 1, .appraised_value = 6, .created_day = sim.current_day};
    (void)snprintf(book->name, sizeof(book->name), "Chronicle of the surviving archive");
    CcSimUpgradeArchivePhysicalLore(&sim);
    CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
}
static CcMoney Money(void)
{
    CcMoney total = sim.iron_ledger_reserve + sim.archive_recruitment.purse;
    for (int i = 0; i < sim.kingdom_count; ++i) total += sim.kingdoms[i].treasury;
    return total;
}
int main(void)
{
    Fixture(true, true);
    CcMoney money = Money(), tracked = CcSimTrackedGold(&sim);
    int32_t wheat = CcSimTrackedGood(&sim, CC_GOOD_WHEAT), paper = CcSimTrackedGood(&sim, CC_GOOD_PAPER);
    int32_t tools = CcSimTrackedGood(&sim, CC_GOOD_TOOLS);
    CC_CHECK(CcSimAutoArchiveRecruitment(&sim));
    CC_CHECK(CcSimTrackedGold(&sim) == tracked);
    CC_CHECK(CcSimTrackedGood(&sim, CC_GOOD_WHEAT) == wheat && CcSimTrackedGood(&sim, CC_GOOD_PAPER) == paper);
    CC_CHECK(CcSimTrackedGood(&sim, CC_GOOD_TOOLS) == tools);
    CC_CHECK(Money() == money && sim.archives.scribes == 0);
    CC_CHECK(sim.archive_recruitment.person_id == recruit && sim.archive_recruitment.purse == 50);
    CC_CHECK(sim.archive_recruitment.donor_shares[0] == 50);
    const CcEvent *event = CcSimRecentEvent(&sim, 0);
    CC_CHECK(event->actor_id == sim.archive_recruitment.patron_ids[0] && event->target_id == recruit);
    before = sim;
    CC_CHECK(!CcSimAutoArchiveRecruitment(&sim));
    CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
    const char *path = "archive-automatic-journal.ccsave";
    CC_CHECK(CcSimCancelArchiveRecruitment(&sim));
    (void)remove(path);
    CcJournal *journal = CcJournalStart(path, &sim, error, sizeof(error));
    CC_CHECK(journal != NULL);
    CC_CHECK(CcJournalAdvanceDays(journal, &sim, 7, error, sizeof(error)));
    CC_CHECK(sim.archives.scribes == 0 && sim.archive_recruitment.status == 1);
    CC_CHECK(sim.archive_recruitment.person_id == recruit);
    for (int day = 0; day < 60 && !CcSimArchiveStaffMember(&sim, recruit); ++day)
        CC_CHECK(CcJournalAdvanceDays(journal, &sim, 1, error, sizeof(error)));
    if (!CcSimArchiveStaffMember(&sim, recruit)) fprintf(stderr, "Recruit state %d, journey %s, training %s, appointment %s, day %d\n",
        sim.archive_recruitment.status, CcArchiveRecruitmentGateName(CcSimArchiveRecruitmentJourneyGate(&sim)),
        CcArchiveRecruitmentGateName(CcSimArchiveRecruitmentTrainingGate(&sim)),
        CcArchiveRecruitmentGateName(CcSimArchiveAppointmentPlan(&sim).gate), sim.current_day);
    CC_CHECK(CcSimArchiveStaffMember(&sim, recruit));
    CC_CHECK(sim.archives.scribes == 1 && sim.archive_recruitment.status == 0);
    CC_CHECK(CcSimTrackedGold(&sim) == tracked);
    CcJournalAbandon(&journal);
    CC_CHECK(CcSaveRead(path, &restored, error, sizeof(error)));
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&restored));
    (void)remove(path);
    Fixture(true, true);
    for (int i = 0; i < sim.route_count; ++i) sim.routes[i].condition = 0;
    before = sim;
    CC_CHECK(CcSimArchiveRecruitmentPlan(&sim).gate == CC_ARCHIVE_RECRUIT_ROUTE);
    CC_CHECK(!CcSimAutoArchiveRecruitment(&sim));
    CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
    Fixture(true, false);
    CcSimSettlementMutable(&sim, seat_id)->stock[CC_GOOD_TOOLS] = 0;
    before = sim;
    CC_CHECK(!CcSimAutoArchiveRecruitment(&sim));
    CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
    Fixture(true, false);
    sim.archives.dead_since_day = 7;
    before = sim;
    CC_CHECK(!CcSimAutoArchiveRecruitment(&sim));
    CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
    Fixture(false, false);
    CC_CHECK(CcSimAutoArchiveRecruitment(&sim));
    CC_CHECK(sim.archive_recruitment.donor_ids[0] == 0 && sim.archive_recruitment.purse == 50);
    CC_CHECK(sim.archives.scribes == 0);
    sim.archive_recruitment.status = 4;
    for (int i = 0; i < sim.character_count; ++i) sim.characters[i].activity = CC_CHARACTER_ACTIVITY_HIDING;
    money = Money();
    CC_CHECK(CcSimAutoArchiveRecruitment(&sim));
    CC_CHECK(sim.archive_recruitment.status == 0 && sim.iron_ledger_reserve == 50 && Money() == money);
    before = sim;
    CC_CHECK(!CcSimAutoArchiveRecruitment(&sim));
    CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
    Fixture(false, false);
    sim.current_day = 6; sim.royal_trade_week = 0; sim.archives.dead_since_day = 0;
    before = sim;
    CC_CHECK(!CcSimAutoArchiveRecruitment(&sim));
    CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
    CcSimAdvanceDays(&sim, 1);
    CC_CHECK(sim.archives.scribes == 0);
    CC_CHECK(sim.archive_recruitment.status == 1);
    puts("Automatic recruitment pays real funders and reaches named work through travel, training and replay.");
    return 0;
}
