#include "sim/cc_archive_relocation.h"
#include "sim/cc_archive_internal.h"
#include "test_support.h"
#include <string.h>

static CcSim sim, before;
static void Fixture(void)
{
    CcSimInit(&sim, 42U);
    sim.current_day = 400; sim.settlement_count = 3; sim.route_count = 2;
    sim.royal_carriage_count = 1; sim.shipment_count = 0; sim.treasure_count = 5;
    sim.archive_recruitment.status = 0;
    sim.archives.seat_id = sim.settlements[0].id; sim.archives.seat_failed_since_day = 1;
    sim.royal_trade_week = sim.current_day / 7;
    memset(sim.royal_route_slots_used, 0, sizeof(sim.royal_route_slots_used));
    for (int i = 0; i < sim.kingdom_count; ++i) {
        sim.kingdoms[i].monastery_patron_id = 0;
        sim.kingdoms[i].ruler_character_id = sim.characters[0].id;
        sim.kingdoms[i].treasury = 100;
        for (int j = 0; j < sim.kingdom_count; ++j) sim.diplomacy[i][j] = CC_DIPLOMACY_ALLIANCE;
    }
    sim.characters[0].death_day = 10000;
    for (int i = 0; i < 3; ++i) {
        CcSettlement *town = &sim.settlements[i];
        town->kingdom_id = sim.kingdoms[0].id; town->population = 100;
        memset(town->stock, 0, sizeof(town->stock));
        town->stock[CC_GOOD_FOOD] = 10000; town->stock[CC_GOOD_WHEAT] = 100;
        town->stock[CC_GOOD_PAPER] = i == 0 ? 0 : 1;
        town->stock[CC_GOOD_TOOLS] = 1;
        town->security = 100; town->service_mask = UINT32_C(1) << CC_SERVICE_MILL;
    }
    for (int i = 0; i < 2; ++i) {
        CcId id = sim.routes[i].id;
        sim.routes[i] = (CcRoute){.id=id,.from_id=sim.settlements[0].id,.to_id=sim.settlements[i+1].id,
            .capacity=8,.condition=100,.security=100,.travel_days=2};
    }
    CcId carriage = sim.royal_carriages[0].id;
    sim.royal_carriages[0]=(CcRoyalCarriage){.id=carriage,.kingdom_id=sim.kingdoms[0].id,
        .location_id=sim.settlements[0].id,.condition=100,.mode=CC_ROYAL_CARRIAGE_IDLE};
    for (int i=0;i<5;++i) {
        sim.treasures[i]=(CcTreasure){.id=(CcId)(100+i),.owner_id=sim.settlements[0].id,
            .location_id=sim.settlements[0].id,.craft_work=1};
        (void)snprintf(sim.treasures[i].name,sizeof(sim.treasures[i].name),"Chronicle %d",i);
    }
}
static CcArchiveRelocationPlan Query(CcArchiveRelocationGate gate)
{
    before=sim;
    CcArchiveRelocationPlan plan=CcSimArchiveRelocationPlan(&sim);
    CC_CHECK(memcmp(&sim,&before,sizeof(sim))==0);
    CC_CHECK(plan.gate==gate);
    return plan;
}
int main(void)
{
    Fixture(); CcArchiveRelocationPlan plan=Query(CC_ARCHIVE_MOVE_READY);
    CC_CHECK(plan.book_count==4 && plan.eligible_books==5 && plan.book_ids[0]==100 && plan.book_ids[3]==103);
    CC_CHECK(plan.destination_id==sim.settlements[1].id && plan.sponsor_id==sim.characters[0].id);
    CC_CHECK(plan.first_leg_days>0 && plan.first_leg_wheat>=2 && plan.first_leg_toll==0 && !plan.rival_foundation);
    sim.treasures[0].owner_id=sim.player.id; sim.treasures[1].location_id=sim.settlements[1].id;
    sim.treasures[2].destroyed=true;
    plan=Query(CC_ARCHIVE_MOVE_READY); CC_CHECK(plan.book_count==2 && plan.book_ids[0]==103);
    CcTreasure swap=sim.treasures[3];sim.treasures[3]=sim.treasures[4];sim.treasures[4]=swap;
    CC_CHECK(Query(CC_ARCHIVE_MOVE_READY).book_ids[0]==103);
    Fixture(); sim.royal_route_slots_used[0]=7;
    plan=Query(CC_ARCHIVE_MOVE_READY);CC_CHECK(plan.book_count==1);
    Fixture(); sim.settlements[1].stock[CC_GOOD_PAPER]=16; sim.routes[0].condition=0;
    plan=Query(CC_ARCHIVE_MOVE_READY);CC_CHECK(plan.destination_id==sim.settlements[2].id);
    sim.routes[1].condition=0; (void)Query(CC_ARCHIVE_MOVE_ROUTE);
    Fixture(); sim.current_day=365;(void)Query(CC_ARCHIVE_MOVE_WAIT);
    sim.current_day=366;(void)Query(CC_ARCHIVE_MOVE_READY);
    sim.settlements[0].stock[CC_GOOD_PAPER]=1;(void)Query(CC_ARCHIVE_MOVE_HEALTHY);
    Fixture();sim.archive_recruitment.status=1;(void)Query(CC_ARCHIVE_MOVE_RECRUITMENT);
    Fixture();sim.treasure_count=0;(void)Query(CC_ARCHIVE_MOVE_BOOKS);
    Fixture();sim.characters[0].death_day=sim.current_day;(void)Query(CC_ARCHIVE_MOVE_SPONSOR);
    Fixture();sim.royal_carriages[0].location_id=sim.settlements[1].id;(void)Query(CC_ARCHIVE_MOVE_CARRIAGE);
    Fixture();sim.settlements[0].stock[CC_GOOD_WHEAT]=0;(void)Query(CC_ARCHIVE_MOVE_FOOD);
    Fixture();sim.routes[0].closed=true;sim.routes[1].closed=true;
    sim.kingdoms[0].treasury=0;(void)Query(CC_ARCHIVE_MOVE_FUNDS);
    sim.kingdoms[0].treasury=4;plan=Query(CC_ARCHIVE_MOVE_READY);CC_CHECK(plan.first_leg_toll==4);
    Fixture();sim.settlements[1].kingdom_id=sim.kingdoms[1].id;sim.settlements[2].stock[CC_GOOD_PAPER]=0;
    sim.royal_carriages[0].kingdom_id=sim.kingdoms[1].id;
    plan=Query(CC_ARCHIVE_MOVE_READY);CC_CHECK(plan.rival_foundation && plan.funding_kingdom_id==sim.kingdoms[1].id);
    Fixture();sim.settlements[1].stock[CC_GOOD_PAPER]=0;sim.settlements[2].stock[CC_GOOD_PAPER]=0;
    (void)Query(CC_ARCHIVE_MOVE_DESTINATION);
    Fixture();sim.schema_version=87U;(void)Query(CC_ARCHIVE_MOVE_SEAT);
    CC_CHECK(CcSimArchiveRelocationPlan(NULL).gate==CC_ARCHIVE_MOVE_SEAT);
    CC_CHECK(strcmp(CcArchiveRelocationGateName(CC_ARCHIVE_MOVE_READY),"ready")==0);
    puts("Archive first-convoy sponsor, custody, route, food and funding quotes passed.");
    return 0;
}
