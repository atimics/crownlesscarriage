#include "sim/cc_archive_recruitment.h"
#include "sim/cc_archive_internal.h"
#include "test_support.h"
#include <string.h>
static CcSim sim, before;
static CcId seat, person;
static void Fixture(void)
{
    CcSimInit(&sim, 42U);
    seat = CcArchiveSeat(&sim)->id;
    sim.archives.scribes = 0; sim.iron_ledger_reserve = 50;
    for (int i = 0; i < sim.character_count; ++i)
        sim.characters[i].activity = CC_CHARACTER_ACTIVITY_HIDING;
    sim.situation_count = 0;
    for (int i = 0; i < sim.kingdom_count; ++i) {
        sim.kingdoms[i].ruler_character_id = 0;
        sim.kingdoms[i].monastery_patron_id = 0;
        sim.kingdoms[i].treasury = 0;
    }
    for (int i = 0; i < sim.settlement_count; ++i) {
        CcSettlement *town = &sim.settlements[i];
        town->population = 100;
        town->stock[CC_GOOD_FOOD] = 10000;
        town->stock[CC_GOOD_WHEAT] = 100;
        town->stock[CC_GOOD_PAPER] = 10;
        town->stock[CC_GOOD_TOOLS] = 10;
        town->service_mask |= UINT32_C(1) << CC_SERVICE_MILL;
    }
    CcCharacter *p = &sim.characters[0];
    p->occupation = CC_OCCUPATION_SCRIBE; p->activity = CC_CHARACTER_ACTIVITY_WORKING;
    p->current_settlement_id = seat; p->birth_day = -10000; p->death_day = 20000;
    person = p->id;
    for (int i = 0; i < sim.route_count; ++i) {
        sim.routes[i].condition = 100; sim.routes[i].closed = false;
        sim.routes[i].travel_days = 2; sim.routes[i].security = 100;
    }
}
static CcArchiveRecruitmentPlan Plan(void)
{
    before = sim;
    CcArchiveRecruitmentPlan p = CcSimArchiveRecruitmentPlan(&sim);
    CC_CHECK(memcmp(&sim, &before, sizeof(sim)) == 0);
    return p;
}
int main(void)
{
    Fixture();
    CcArchiveRecruitmentPlan p = Plan();
    CC_CHECK(p.gate == CC_ARCHIVE_RECRUIT_READY && p.person_id == person);
    CC_CHECK(p.wages == 50 && p.wheat == 4 && p.paper == 2 && p.tools == 1);
    CC_CHECK(!p.training && p.training_days == 7 && p.ready_day == sim.current_day + 7);
    CC_CHECK(p.arrival_day == sim.current_day && p.travel_days == 0);
    sim.characters[0].current_settlement_id = sim.settlements[0].id;
    p = Plan(); CC_CHECK(p.gate == CC_ARCHIVE_RECRUIT_READY && p.travel_days >= 2);
    CC_CHECK(p.first_route_id != 0 && p.first_hop_id != 0 && p.travel_wheat == 2);
    sim.settlements[0].stock[CC_GOOD_WHEAT] = 0;
    CC_CHECK(Plan().gate == CC_ARCHIVE_RECRUIT_TRAVEL_FOOD);
    sim.settlements[0].stock[CC_GOOD_WHEAT] = 100;
    for (int i = 0; i < sim.route_count; ++i) sim.routes[i].condition = 0;
    CC_CHECK(Plan().gate == CC_ARCHIVE_RECRUIT_ROUTE);
    Fixture(); sim.characters[0].occupation = CC_OCCUPATION_FARMER;
    CC_CHECK(Plan().gate == CC_ARCHIVE_RECRUIT_TRAINER);
    sim.archives.scribes = 1;
    CcCharacter *trainer = &sim.characters[1];
    trainer->occupation = CC_OCCUPATION_SCRIBE; trainer->activity = CC_CHARACTER_ACTIVITY_WORKING;
    trainer->current_settlement_id = seat; trainer->birth_day = -10000; trainer->death_day = 20000;
    p = Plan(); CC_CHECK(p.gate == CC_ARCHIVE_RECRUIT_READY && p.training);
    CC_CHECK(p.person_id == person && p.trainer_id == trainer->id);
    CC_CHECK(p.wheat == 18 && p.paper == 5 && p.training_days == 28 && p.trainer_days == 28);
    CcSimSettlementMutable(&sim, seat)->stock[CC_GOOD_PAPER] = 1;
    CC_CHECK(Plan().gate == CC_ARCHIVE_RECRUIT_MATERIALS);
    Fixture(); sim.iron_ledger_reserve = 0;
    CC_CHECK(Plan().gate == CC_ARCHIVE_RECRUIT_SILENCE);
    sim.current_day = 1827; sim.archives.dead_since_day = 1;
    CC_CHECK(Plan().gate == CC_ARCHIVE_RECRUIT_FUNDS);
    CcId kingdom = CcSimSettlement(&sim, seat)->kingdom_id;
    int k = 0; while (sim.kingdoms[k].id != kingdom) ++k;
    sim.kingdoms[k].treasury = 800;
    CC_CHECK(Plan().gate == CC_ARCHIVE_RECRUIT_PATRON);
    sim.kingdoms[k].monastery_patron_id = sim.characters[2].id;
    sim.characters[2].home_settlement_id = seat;
    sim.characters[2].birth_day = -10000; sim.characters[2].death_day = 20000;
    p = Plan(); CC_CHECK(p.gate == CC_ARCHIVE_RECRUIT_READY && p.recovery);
    CC_CHECK(p.funding.total == 50 && p.patron_ids[0] == sim.characters[2].id);
    sim.characters[2].death_day = sim.current_day;
    CC_CHECK(Plan().gate == CC_ARCHIVE_RECRUIT_PATRON);
    Fixture(); sim.characters[0].death_day = sim.current_day;
    CC_CHECK(Plan().gate == CC_ARCHIVE_RECRUIT_CANDIDATE);
    Fixture(); sim.archives.scribes = CC_MAX_SCRIBES;
    CC_CHECK(Plan().gate == CC_ARCHIVE_RECRUIT_FULL);
    Fixture(); sim.current_day = CC_SIM_MAX_DAY - 1;
    sim.characters[0].death_day = CC_SIM_MAX_DAY;
    sim.characters[0].birth_day = sim.current_day - 10000;
    CC_CHECK(Plan().gate == CC_ARCHIVE_RECRUIT_CALENDAR);
    Fixture();
    sim.characters[1].occupation = CC_OCCUPATION_SCRIBE;
    sim.characters[1].activity = CC_CHARACTER_ACTIVITY_WORKING;
    sim.characters[1].current_settlement_id = seat;
    sim.characters[1].birth_day = -10000; sim.characters[1].death_day = 20000;
    CcId selected = Plan().person_id;
    CcCharacter swap = sim.characters[0];
    sim.characters[0] = sim.characters[1]; sim.characters[1] = swap;
    CC_CHECK(Plan().person_id == selected);
    Fixture(); sim.characters[0].activity = CC_CHARACTER_ACTIVITY_TRAVELLING;
    CC_CHECK(Plan().gate == CC_ARCHIVE_RECRUIT_CANDIDATE);
    Fixture(); sim.characters[0].birth_day = sim.current_day - 10 * 365;
    CC_CHECK(Plan().gate == CC_ARCHIVE_RECRUIT_CANDIDATE);
    Fixture(); sim.kingdoms[0].ruler_character_id = person;
    CC_CHECK(Plan().gate == CC_ARCHIVE_RECRUIT_CANDIDATE);
    Fixture();
    sim.route_count = 2;
    sim.characters[0].current_settlement_id = sim.settlements[2].id;
    sim.routes[0].from_id = sim.settlements[2].id;
    sim.routes[0].to_id = sim.settlements[0].id;
    sim.routes[0].travel_days = 2;
    sim.routes[0].closed = true;
    sim.routes[1].from_id = sim.settlements[0].id;
    sim.routes[1].to_id = seat;
    sim.routes[1].travel_days = 3;
    p = Plan();
    CC_CHECK(p.gate == CC_ARCHIVE_RECRUIT_READY && p.dangerous && p.travel_days == 5);
    CC_CHECK(p.first_route_id == sim.routes[0].id && p.first_hop_id == sim.settlements[0].id);
    CC_CHECK(p.first_task_wheat == 2 && p.first_task_paper == 1);
    sim.settlements[0].population = 0;
    CC_CHECK(Plan().gate == CC_ARCHIVE_RECRUIT_ROUTE);
    Fixture(); sim.schema_version = 76;
    CC_CHECK(Plan().gate == CC_ARCHIVE_RECRUIT_UNAVAILABLE);
    CC_CHECK(CcSimArchiveRecruitmentPlan(NULL).gate == CC_ARCHIVE_RECRUIT_UNAVAILABLE);
    return 0;
}
