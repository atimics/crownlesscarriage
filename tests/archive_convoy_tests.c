#include "sim/cc_archive_relocation.h"
#include "sim/cc_archive_internal.h"
#include "sim/cc_archive_recruitment.h"
#include "sim/cc_food_economy_internal.h"
#include "persistence/cc_save.h"
#include "test_support.h"
#include <sqlite3.h>
#include <string.h>

static CcSim sim, before, restored;
static char error[256];
static void Valid(void)
{
    if (!CcSimValidate(&sim,error,sizeof(error))) fprintf(stderr,"%s\n",error);
    CC_CHECK(CcSimValidate(&sim,error,sizeof(error)));
}
static CcArchiveRelocationPlan Fixture(void)
{
    CcSimInit(&sim,42U);sim.current_day=400;
    sim.royal_trade_week=sim.current_day/7;
    memset(sim.royal_route_slots_used,0,sizeof(sim.royal_route_slots_used));
    CcSettlement *origin=CcSimSettlementMutable(&sim,CcArchiveSeat(&sim)->id);
    sim.archives.seat_id=origin->id;sim.archives.seat_failed_since_day=1;
    CcSettlement *destination=NULL;
    for(int i=0;i<sim.route_count;i++) {
        CcRoute *road=&sim.routes[i];
        if(!road->smuggler_route && (road->from_id==origin->id || road->to_id==origin->id)) {
            destination=CcSimSettlementMutable(&sim,road->from_id==origin->id?road->to_id:road->from_id);
            road->condition=100;road->closed=true;road->travel_days=2;break;
        }
    }
    CC_CHECK(destination!=NULL);
    for(int i=0;i<sim.settlement_count;i++)sim.settlements[i].stock[CC_GOOD_PAPER]=0;
    origin->stock[CC_GOOD_FOOD]=10000;origin->stock[CC_GOOD_WHEAT]=100;
    destination->stock[CC_GOOD_FOOD]=10000;destination->stock[CC_GOOD_WHEAT]=10;
    destination->stock[CC_GOOD_PAPER]=1;destination->stock[CC_GOOD_TOOLS]=1;
    destination->service_mask=UINT32_C(1)<<CC_SERVICE_MILL;
    for(int i=0;i<sim.kingdom_count;i++) {
        sim.kingdoms[i].treasury=100;
        for(int j=0;j<sim.kingdom_count;j++)if(i!=j)sim.diplomacy[i][j]=CC_DIPLOMACY_ALLIANCE;
    }
    for(int i=0;i<sim.royal_carriage_count;i++)if(sim.royal_carriages[i].kingdom_id==destination->kingdom_id)
        sim.royal_carriages[i].location_id=origin->id;
    CcTreasure *book=&sim.treasures[sim.treasure_count++];
    *book=(CcTreasure){.id=CcMakeId(CC_ENTITY_TREASURE,sim.next_entity_serial++),.owner_id=origin->id,
        .location_id=origin->id,.maker_settlement_id=origin->id,.craft_work=1,.appraised_value=1,.created_day=sim.current_day};
    (void)snprintf(book->name,sizeof(book->name),"Chronicle of the saved seat");
    CcSimUpgradeArchivePhysicalLore(&sim);
    CcEconomyRefreshSettlementGoodPrice(&sim,origin,CC_GOOD_WHEAT);
    Valid();
    CcArchiveRelocationPlan plan=CcSimArchiveRelocationPlan(&sim);
    if(plan.gate!=CC_ARCHIVE_MOVE_READY)fprintf(stderr,"gate %s\n",CcArchiveRelocationGateName(plan.gate));
    CC_CHECK(plan.gate==CC_ARCHIVE_MOVE_READY && plan.first_leg_toll>0);
    return plan;
}
int main(void)
{
    CcArchiveRelocationPlan plan=Fixture();before=sim;
    CcMoney gold=CcSimTrackedGold(&sim);int32_t wheat=CcSimTrackedGood(&sim,CC_GOOD_WHEAT);
    CC_CHECK(CcSimReserveArchiveConvoy(&sim));Valid();
    CC_CHECK(sim.archive_convoy.purse==plan.first_leg_toll && sim.archive_convoy.wheat==plan.first_leg_wheat);
    CC_CHECK(CcSimTrackedGold(&sim)==gold && CcSimTrackedGood(&sim,CC_GOOD_WHEAT)==wheat);
    CC_CHECK(memcmp(before.treasures,sim.treasures,sizeof(sim.treasures))==0);
    CC_CHECK(CcSimArchiveRelocationPlan(&sim).gate==CC_ARCHIVE_MOVE_BUSY);
    CC_CHECK(CcSimArchiveRecruitmentPlan(&sim).gate==CC_ARCHIVE_RECRUIT_BUSY);
    restored=sim;CC_CHECK(!CcSimReserveArchiveConvoy(&sim));CC_CHECK(memcmp(&sim,&restored,sizeof(sim))==0);
    CC_CHECK(CcSimCancelArchiveConvoy(&sim));Valid();CC_CHECK(CcSimHash(&sim)==CcSimHash(&before));
    CC_CHECK(CcSimReserveArchiveConvoy(&sim));
    const char *path="archive-convoy-test.ccsave";
    CcJournal *journal=CcJournalStart(path,&sim,error,sizeof(error));CC_CHECK(journal!=NULL);
    CC_CHECK(CcJournalAdvanceDays(journal,&sim,7,error,sizeof(error)));
    CcJournalAbandon(&journal);
    CC_CHECK(CcSaveRead(path,&restored,error,sizeof(error)));
    CC_CHECK(CcSimHash(&sim)==CcSimHash(&restored));Valid();
    CC_CHECK(sim.archive_convoy.status==1 && sim.archive_convoy.carriage_id==plan.carriage_id);
    CcSettlement *origin=CcSimSettlementMutable(&sim,plan.origin_id);
    int32_t old_wheat=origin->stock[CC_GOOD_WHEAT];origin->stock[CC_GOOD_WHEAT]=CC_SIM_MAX_UNITS;
    before=sim;CC_CHECK(!CcSimCancelArchiveConvoy(&sim));CC_CHECK(memcmp(&before,&sim,sizeof(sim))==0);
    origin->stock[CC_GOOD_WHEAT]=old_wheat;
    CC_CHECK(CcSimCancelArchiveConvoy(&sim));Valid();
    sqlite3 *db=NULL;CC_CHECK(sqlite3_open(path,&db)==SQLITE_OK);
    CC_CHECK(sqlite3_exec(db,"UPDATE archive_convoy SET wheat=2147483648;",NULL,NULL,NULL)==SQLITE_OK);
    CC_CHECK(sqlite3_close(db)==SQLITE_OK);CC_CHECK(!CcSaveRead(path,&restored,error,sizeof(error)));
    (void)remove(path);(void)remove("archive-convoy-test.ccsave-wal");(void)remove("archive-convoy-test.ccsave-shm");
    plan=Fixture();
    for(int i=0;i<sim.kingdom_count;i++)if(sim.kingdoms[i].id==plan.funding_kingdom_id)sim.kingdoms[i].treasury=plan.first_leg_toll-1;
    before=sim;CC_CHECK(!CcSimReserveArchiveConvoy(&sim));CC_CHECK(memcmp(&sim,&before,sizeof(sim))==0);
    puts("Archive convoy reservation, refund, conservation and journal restart passed.");return 0;
}
