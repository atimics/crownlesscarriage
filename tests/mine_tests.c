#include "sim/cc_mine.h"
#include "sim/cc_sim_custody.h"
#include "persistence/cc_save.h"
#include "multiplayer/cc_coop.h"
#include "metagame/cc_metagame.h"
#include "test_support.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static char error[256];
static void Check(bool ok)
{
    if(!ok) fprintf(stderr,"Mine check: %s\n",error);
    CC_CHECK(ok);
}
static void Apply(CcSim *sim,CcCommandKind kind,int32_t amount)
{
    CcCommand command={.kind=kind,.target_id=(CcId)sim->mine.revision,.amount=amount};
    Check(CcSimApply(sim,&command,error,sizeof(error)));
    Check(CcSimValidate(sim,error,sizeof(error)));
}
static void ApplyGood(CcSim *sim,CcCommandKind kind,CcGood good,int32_t amount)
{
    CcCommand command={.kind=kind,.target_id=(CcId)sim->mine.revision,.good=good,.amount=amount};
    Check(CcSimApply(sim,&command,error,sizeof(error)));
    Check(CcSimValidate(sim,error,sizeof(error)));
}
static void Walk(CcSim *sim,int32_t tx,int32_t ty)
{
    int32_t queue[CC_MINE_WIDTH*CC_MINE_HEIGHT],prev[CC_MINE_WIDTH*CC_MINE_HEIGHT],dirs[CC_MINE_WIDTH*CC_MINE_HEIGHT];
    for(int32_t i=0;i<CC_MINE_WIDTH*CC_MINE_HEIGHT;++i) prev[i]=-1;
    int32_t root=sim->mine.y*CC_MINE_WIDTH+sim->mine.x,head=0,tail=0;
    queue[tail++]=root;prev[root]=root;
    const int32_t dx[]={0,1,0,-1},dy[]={-1,0,1,0};
    while(head<tail) {
        int32_t cell=queue[head++],x=cell%CC_MINE_WIDTH,y=cell/CC_MINE_WIDTH;
        for(int32_t d=0;d<4;++d) {
            int32_t nx=x+dx[d],ny=y+dy[d];
            if(!CcMineWalkable(sim,sim->mine.phase,nx,ny)) continue;
            int32_t next=ny*CC_MINE_WIDTH+nx;
            if(prev[next]>=0) continue;
            prev[next]=cell;dirs[next]=d;queue[tail++]=next;
        }
    }
    int32_t target=ty*CC_MINE_WIDTH+tx,count=0;
    CC_CHECK(prev[target]>=0);
    while(target!=root) {queue[count++]=dirs[target];target=prev[target];}
    while(count>0) Apply(sim,CC_COMMAND_MINE_STEP,queue[--count]);
}
static void AtBranch(CcSim *sim,bool reverse)
{
    CcSimInit(sim,0x71a7e5);
    const CcRoadSite *site=CcMineSite(sim);
    CC_CHECK(site!=NULL);
    const CcRoute *road=CcSimRoute(sim,site->route_id);
    sim->player.location_id=reverse?road->to_id:road->from_id;
    sim->carriage.location_id=sim->player.location_id;
    CcCommand travel={.kind=CC_COMMAND_TRAVEL,.target_id=reverse?road->from_id:road->to_id};
    Check(CcSimApply(sim,&travel,error,sizeof(error)));
    sim->pony_company.encounter=-1;
    sim->journey.ambush_pending=false;
    sim->journey.elapsed_subticks=CcMineBranchSubtick(sim)-1;
    sim->carriage.progress_milli=(int32_t)((int64_t)sim->journey.elapsed_subticks*1000/sim->journey.total_subticks);
    CcSimAdvanceRuntimeTicks(sim,100);
    CC_CHECK(sim->journey.elapsed_subticks==CcMineBranchSubtick(sim));
    uint64_t held=CcSimHash(sim);
    CcSimAdvanceRuntimeTicks(sim,100);
    CC_CHECK(CcSimHash(sim)==held);
    Check(CcSimValidate(sim,error,sizeof(error)));
}
static void ReturnToMineBranch(CcSim *sim)
{
    for (int32_t ticks=0;sim->journey.active && ticks<100000;++ticks)
        CcSimAdvanceRuntimeTicks(sim,1);
    CC_CHECK(!sim->journey.active);
    const CcRoadSite *site=CcMineSite(sim);
    const CcRoute *road=CcSimRoute(sim,site->route_id);
    CcId destination=sim->player.location_id==road->from_id ? road->to_id : road->from_id;
    CcCommand travel={.kind=CC_COMMAND_TRAVEL,.target_id=destination};
    Check(CcSimApply(sim,&travel,error,sizeof(error)));
    sim->pony_company.encounter=-1;
    sim->journey.ambush_pending=false;
    sim->journey.elapsed_subticks=CcMineBranchSubtick(sim)-1;
    sim->carriage.progress_milli=(int32_t)((int64_t)sim->journey.elapsed_subticks*1000/
        sim->journey.total_subticks);
    CcSimAdvanceRuntimeTicks(sim,1);
    CC_CHECK(sim->journey.elapsed_subticks==CcMineBranchSubtick(sim));
    CcCommand visit={.kind=CC_COMMAND_VISIT_MINE,.target_id=site->id};
    Check(CcSimApply(sim,&visit,error,sizeof(error)));
}
static void ReadyAtHaulers(CcSim *sim)
{
    AtBranch(sim,false);
    sim->player.cargo[CC_GOOD_BREAD]=4;
    sim->player.cargo[CC_GOOD_IRON]=8;
    CcCommand visit={.kind=CC_COMMAND_VISIT_MINE,
        .target_id=CcMineSite(sim)->id};
    Check(CcSimApply(sim,&visit,error,sizeof(error)));
    ApplyGood(sim,CC_COMMAND_MINE_PACK,CC_GOOD_BREAD,3);
    ApplyGood(sim,CC_COMMAND_MINE_PACK,CC_GOOD_IRON,5);
    Walk(sim,15,3);
    Apply(sim,CC_COMMAND_MINE_USE,0);
    CC_CHECK(!sim->mine.bar_open);
    Walk(sim,9,15);
    CC_CHECK(sim->mine.bypass_route_seen);
    CC_CHECK(!sim->mine.source_released && CcMinePackGood(sim,CC_GOOD_GOLD)==0);
    Walk(sim,26,16);
}

static void FinishJourney(CcSim *sim)
{
    for (int32_t ticks=0;sim->journey.active && ticks<200000;++ticks) {
        if (sim->journey.phase == CC_JOURNEY_PHASE_TRAVELLING)
            CcSimAdvanceRuntimeTicks(sim,CC_WORLD_TICKS_PER_SECOND);
        else
            Check(CcTestContinueJourneyPause(sim,error,sizeof(error)));
    }
    CC_CHECK(!sim->journey.active);
}

static void ReturnFromMineToSilverwick(CcSim *sim)
{
    Walk(sim,5,3); Apply(sim,CC_COMMAND_MINE_USE,0);
    Walk(sim,15,18); Apply(sim,CC_COMMAND_MINE_USE,0);
    FinishJourney(sim);
    CcId silverwick=sim->dungeons[0].settlement_id;
    if (sim->player.location_id != silverwick) {
        CcCommand travel={.kind=CC_COMMAND_TRAVEL,.target_id=silverwick};
        Check(CcSimApply(sim,&travel,error,sizeof(error)));
        sim->journey.ambush_pending=false;
        sim->pony_company.encounter=-1;
        FinishJourney(sim);
    }
    CC_CHECK(sim->player.location_id==silverwick);
}

static int32_t KnowledgeCount(const CcSim *sim,CcId id)
{
    const CcCharacter *character=CcSimCharacter(sim,id);
    return character!=NULL?character->knowledge_count:-1;
}

static int32_t MineTrackedCustodyQuantity(const CcSim *sim)
{
    int32_t quantity=0;
    for(int32_t i=0;i<CcCustodyEffectiveCapacity(&sim->custody);++i) {
        const CcCustodyEntry *entry=&sim->custody.entries[i];
        if(entry->active&&entry->kind==CC_CUSTODY_GOODS&&
           CcSimMineEntryTracked(sim,entry)) quantity+=(int32_t)entry->quantity;
    }
    return quantity;
}

static void TestMinePartyWipeConservation(void)
{
    CcSim carried;
    ReadyAtHaulers(&carried);
    Apply(&carried,CC_COMMAND_MINE_BARGAIN,0);
    ReturnFromMineToSilverwick(&carried);
    int32_t custody_before=MineTrackedCustodyQuantity(&carried);
    int32_t gold_before=carried.player.cargo[CC_GOOD_GOLD];
    CC_CHECK(custody_before==15&&gold_before==1&&
        CcSimMineReturnEvidence(&carried)==CC_MINE_RETURN_HAUL);
    int32_t day=carried.current_day;
    CcCommand wipe={.kind=CC_COMMAND_PARTY_WIPE,.target_id=(CcId)day};
    Check(CcSimApply(&carried,&wipe,error,sizeof(error)));
    CC_CHECK(carried.current_day==day+CC_PARTY_WIPE_DAYS&&
        MineTrackedCustodyQuantity(&carried)==custody_before&&
        carried.player.cargo[CC_GOOD_GOLD]==gold_before&&
        CcSimMineReturnEvidence(&carried)==CC_MINE_RETURN_HAUL);
    bool gold_in_carriage=false;
    for(int32_t i=0;i<CcCustodyEffectiveCapacity(&carried.custody);++i) {
        const CcCustodyEntry *entry=&carried.custody.entries[i];
        if(entry->active&&entry->good==CC_GOOD_GOLD&&entry->quantity==1&&
           entry->holder.kind==CC_CUSTODY_PLAYER&&
           entry->holder.id==carried.player.id&&CcSimMineEntryTracked(&carried,entry))
            gold_in_carriage=true;
    }
    CC_CHECK(gold_in_carriage);
}

static void TestReliefCratePersistence(void)
{
    CcSim sim,restored;
    CcSimInit(&sim,UINT32_C(0x107ca77e));
    CcSituation *relief=NULL;
    for (int32_t i=0;i<sim.situation_count;++i)
        if (sim.situations[i].kind == CC_SITUATION_RELIEF_DELIVERY) {
            relief=&sim.situations[i];
            break;
        }
    CC_CHECK(relief!=NULL);
    CcId relief_id=relief->id;
    CcId origin=CcSimSituationOfferSettlementId(&sim,relief);
    sim.player.location_id=origin;
    sim.carriage.location_id=origin;
    CcCommand accept={.kind=CC_COMMAND_ACCEPT_SITUATION,
        .target_id=relief_id};
    Check(CcSimApply(&sim,&accept,error,sizeof(error)));
    CcCommand pickup={.kind=CC_COMMAND_PICKUP_RELIEF_CRATE,
        .target_id=relief_id};
    Check(CcSimApply(&sim,&pickup,error,sizeof(error)));
    CC_CHECK(relief->loading_crate_carried&&relief->loading_progress==0);
    Check(CcSaveWrite("relief-crate-carry.ccsave",&sim,error,sizeof(error)));
    Check(CcSaveRead("relief-crate-carry.ccsave",&restored,error,sizeof(error)));
    relief=(CcSituation *)CcSimSituation(&restored,relief_id);
    CC_CHECK(relief!=NULL&&relief->loading_crate_carried&&
        relief->loading_progress==0&&CcSimHash(&restored)==CcSimHash(&sim));
    CcCommand stow={.kind=CC_COMMAND_STOW_RELIEF_CRATE,
        .target_id=relief_id};
    Check(CcSimApply(&restored,&stow,error,sizeof(error)));
    CC_CHECK(!relief->loading_crate_carried&&relief->loading_progress==1&&
        restored.player.cargo[CC_GOOD_FOOD]==1);
    (void)remove("relief-crate-carry.ccsave");

    CcSimInit(&sim,UINT32_C(0x106ca77e));
    relief=NULL;
    for (int32_t i=0;i<sim.situation_count;++i)
        if (sim.situations[i].kind == CC_SITUATION_RELIEF_DELIVERY) {
            relief=&sim.situations[i];
            break;
        }
    CC_CHECK(relief!=NULL);
    relief_id=relief->id;
    CcCommand legacy_accept={.kind=CC_COMMAND_ACCEPT_SITUATION,
        .target_id=relief_id};
    Check(CcSimApply(&sim,&legacy_accept,error,sizeof(error)));
    sim.player.cargo[relief->good]=relief->quantity;
    sim.schema_version=107U;
    Check(CcSaveWrite("relief-crate-carry.ccsave",&sim,error,sizeof(error)));
    Check(CcSaveRead("relief-crate-carry.ccsave",&restored,error,sizeof(error)));
    relief=(CcSituation *)CcSimSituation(&restored,relief_id);
    CC_CHECK(relief!=NULL&&CcSimReliefLoadingComplete(relief)&&
        relief->loading_progress==relief->quantity);
    (void)remove("relief-crate-carry.ccsave");
}

static void CarryThreeTrackedGold(CcSim *sim)
{
    AtBranch(sim,false);
    sim->player.cargo[CC_GOOD_BREAD]=4;
    CcCommand visit={.kind=CC_COMMAND_VISIT_MINE,.target_id=CcMineSite(sim)->id};
    Check(CcSimApply(sim,&visit,error,sizeof(error)));
    ApplyGood(sim,CC_COMMAND_MINE_PACK,CC_GOOD_BREAD,3);
    Walk(sim,15,3);Apply(sim,CC_COMMAND_MINE_USE,0);
    Walk(sim,9,15);Walk(sim,26,16);
    Apply(sim,CC_COMMAND_MINE_CONTEST,0);
    Apply(sim,CC_COMMAND_MINE_RESOLVE_CONTEST,1);
    ApplyGood(sim,CC_COMMAND_MINE_TAKE,CC_GOOD_GOLD,3);
    ReturnFromMineToSilverwick(sim);
    CC_CHECK(sim->player.cargo[CC_GOOD_GOLD]==3&&
        CcSimMineReturnEvidence(sim)==CC_MINE_RETURN_HAUL);
}

static int32_t CarriedTrackedGood(const CcSim *sim,CcGood good)
{
    int32_t quantity=0;
    for(int32_t i=0;i<CcCustodyEffectiveCapacity(&sim->custody);++i) {
        const CcCustodyEntry *entry=&sim->custody.entries[i];
        if(entry->active&&entry->good==(int32_t)good&&
           entry->holder.kind==CC_CUSTODY_PLAYER&&
           entry->holder.id==sim->player.id&&CcSimMineEntryTracked(sim,entry))
            quantity+=(int32_t)entry->quantity;
    }
    return quantity;
}

static int32_t TrackedGoodAt(const CcSim *sim,CcGood good,
                             CcCustodyHolderKind kind,CcId holder_id)
{
    int32_t quantity=0;
    for(int32_t i=0;i<CcCustodyEffectiveCapacity(&sim->custody);++i) {
        const CcCustodyEntry *entry=&sim->custody.entries[i];
        if(entry->active&&entry->good==(int32_t)good&&entry->holder.kind==kind&&
           entry->holder.id==holder_id&&CcSimMineEntryTracked(sim,entry))
            quantity+=(int32_t)entry->quantity;
    }
    return quantity;
}

static void TestMineTrackedRepack(void)
{
    CcSim sim,loaded;
    AtBranch(&sim,false);
    sim.player.cargo[CC_GOOD_BREAD]=4;
    CcCommand visit={.kind=CC_COMMAND_VISIT_MINE,.target_id=CcMineSite(&sim)->id};
    Check(CcSimApply(&sim,&visit,error,sizeof(error)));
    ApplyGood(&sim,CC_COMMAND_MINE_PACK,CC_GOOD_BREAD,3);
    Walk(&sim,15,3);Apply(&sim,CC_COMMAND_MINE_USE,0);
    Walk(&sim,9,15);Walk(&sim,26,16);
    Apply(&sim,CC_COMMAND_MINE_CONTEST,0);
    Apply(&sim,CC_COMMAND_MINE_RESOLVE_CONTEST,1);
    ApplyGood(&sim,CC_COMMAND_MINE_TAKE,CC_GOOD_GOLD,3);
    int32_t tracked=MineTrackedCustodyQuantity(&sim);
    Walk(&sim,5,3);Apply(&sim,CC_COMMAND_MINE_USE,0);
    Walk(&sim,15,18);
    ApplyGood(&sim,CC_COMMAND_MINE_PACK,CC_GOOD_GOLD,-1);
    CC_CHECK(sim.player.cargo[CC_GOOD_GOLD]==1&&
        TrackedGoodAt(&sim,CC_GOOD_GOLD,CC_CUSTODY_PLAYER,sim.player.id)==1&&
        TrackedGoodAt(&sim,CC_GOOD_GOLD,CC_CUSTODY_MINE_PACK,sim.player.id)==2);
    ApplyGood(&sim,CC_COMMAND_MINE_PACK,CC_GOOD_GOLD,1);
    CC_CHECK(sim.player.cargo[CC_GOOD_GOLD]==0&&sim.mine.pack[CC_GOOD_GOLD]==0&&
        TrackedGoodAt(&sim,CC_GOOD_GOLD,CC_CUSTODY_PLAYER,sim.player.id)==0&&
        TrackedGoodAt(&sim,CC_GOOD_GOLD,CC_CUSTODY_MINE_PACK,sim.player.id)==3);
    Walk(&sim,15,3);Apply(&sim,CC_COMMAND_MINE_USE,0);
    Walk(&sim,5,15);
    ApplyGood(&sim,CC_COMMAND_MINE_CACHE,CC_GOOD_GOLD,1);
    CC_CHECK(TrackedGoodAt(&sim,CC_GOOD_GOLD,CC_CUSTODY_SITE,sim.mine.cache_id)==1&&
        MineTrackedCustodyQuantity(&sim)==tracked);
    Check(CcSaveWrite("mine-tracked-repack.ccsave",&sim,error,sizeof(error)));
    Check(CcSaveRead("mine-tracked-repack.ccsave",&loaded,error,sizeof(error)));
    CC_CHECK(CcSimHash(&loaded)==CcSimHash(&sim)&&
        TrackedGoodAt(&loaded,CC_GOOD_GOLD,CC_CUSTODY_SITE,loaded.mine.cache_id)==1);
    ApplyGood(&loaded,CC_COMMAND_MINE_CACHE,CC_GOOD_GOLD,-1);
    Walk(&loaded,5,3);Apply(&loaded,CC_COMMAND_MINE_USE,0);
    Walk(&loaded,15,18);
    ApplyGood(&loaded,CC_COMMAND_MINE_PACK,CC_GOOD_GOLD,-1);
    CC_CHECK(loaded.player.cargo[CC_GOOD_GOLD]==1&&
        CarriedTrackedGood(&loaded,CC_GOOD_GOLD)==1&&
        TrackedGoodAt(&loaded,CC_GOOD_GOLD,CC_CUSTODY_MINE_PACK,loaded.player.id)==2&&
        TrackedGoodAt(&loaded,CC_GOOD_GOLD,CC_CUSTODY_SITE,loaded.mine.cache_id)==0&&
        MineTrackedCustodyQuantity(&loaded)==tracked);
    Apply(&loaded,CC_COMMAND_MINE_USE,0);
    FinishJourney(&loaded);
    CcId silverwick=loaded.dungeons[0].settlement_id;
    if(loaded.player.location_id!=silverwick) {
        CcCommand travel={.kind=CC_COMMAND_TRAVEL,.target_id=silverwick};
        Check(CcSimApply(&loaded,&travel,error,sizeof(error)));
        loaded.journey.ambush_pending=false;
        loaded.pony_company.encounter=-1;
        FinishJourney(&loaded);
    }
    CcCommand sale={.kind=CC_COMMAND_TRADE,.good=CC_GOOD_GOLD,.amount=-1};
    Check(CcSimApply(&loaded,&sale,error,sizeof(error)));
    CC_CHECK(CarriedTrackedGood(&loaded,CC_GOOD_GOLD)==2&&
        loaded.mine.haul_receipt_quantity==1);
    CcCommand report={.kind=CC_COMMAND_MINE_REPORT_RETURN,
        .target_id=(CcId)loaded.mine.return_revision};
    Check(CcSimApply(&loaded,&report,error,sizeof(error)));
    CC_CHECK(loaded.mine.report_kind==CC_MINE_RETURN_HAUL&&
        loaded.mine.report_good==CC_GOOD_GOLD&&loaded.mine.report_quantity==2);
    (void)remove("mine-tracked-repack.ccsave");
}

static void TestMineCargoReconciliation(void)
{
    CcSim partial,lost;
    CarryThreeTrackedGold(&partial);
    CcCommand sale={.kind=CC_COMMAND_TRADE,.good=CC_GOOD_GOLD,.amount=-1};
    Check(CcSimApply(&partial,&sale,error,sizeof(error)));
    CC_CHECK(partial.player.cargo[CC_GOOD_GOLD]==2&&
        CarriedTrackedGood(&partial,CC_GOOD_GOLD)==2&&
        partial.mine.haul_receipt_quantity==1);

    CarryThreeTrackedGold(&lost);
    for(int32_t loss=0;loss<3;++loss) {
        lost.player.cargo[CC_GOOD_BREAD]=18;
        CcCommand begin={.kind=CC_COMMAND_BEGIN_DUNGEON_EXPEDITION,
            .target_id=lost.dungeons[0].id};
        Check(CcSimApply(&lost,&begin,error,sizeof(error)));
        CcCommand move={.kind=CC_COMMAND_MOVE_DUNGEON,.amount=1};
        Check(CcSimApply(&lost,&move,error,sizeof(error)));
        lost.dungeon_expedition.strain=75;
        CcCommand retreat={.kind=CC_COMMAND_RETREAT_DUNGEON};
        Check(CcSimApply(&lost,&retreat,error,sizeof(error)));
        CC_CHECK(lost.player.cargo[CC_GOOD_GOLD]==2-loss&&
            CarriedTrackedGood(&lost,CC_GOOD_GOLD)==2-loss);
    }
    CC_CHECK(CcSimMineReturnEvidence(&lost)==CC_MINE_RETURN_INFORMATION);
    lost.player.cargo[CC_GOOD_GOLD]=1;
    CC_CHECK(CcSimMineReturnEvidence(&lost)==CC_MINE_RETURN_INFORMATION);
}

static void TestMineReturnRecords(void)
{
    CcSim lead,fallback,haul,restored,old_gold;
    CcSimInit(&lead,UINT32_C(0x1061ead));
    CcId silverwick=lead.dungeons[0].settlement_id;
    lead.player.location_id=silverwick;lead.carriage.location_id=silverwick;
    const CcCharacter *jory=CcSimMineEvidenceContact(&lead);
    CC_CHECK(jory!=NULL && CcSimMineLeadSupported(&lead));
    uint64_t before=CcSimHash(&lead);
    CC_CHECK(!CcCoopApply(&lead,"mine_learn_lead",
        (CcId)(lead.mine.return_revision-1),0,1,error,sizeof(error)) &&
        CcSimHash(&lead)==before);
    Check(CcCoopApply(&lead,"mine_learn_lead",
        (CcId)lead.mine.return_revision,0,1,error,sizeof(error)));
    CC_CHECK(lead.mine.lead_event_id!=0U && lead.mine.lead_source_id==jory->id &&
        !lead.mine.lead_document && lead.mine.lead_day==lead.current_day);
    const CcEvent *lead_event=CcSimEvent(&lead,lead.mine.lead_event_id);
    CC_CHECK(lead_event!=NULL&&strstr(lead_event->text,"Alderwatch-Silverwick")!=NULL&&
        strstr(lead_event->text,"workers' records")!=NULL);
    before=CcSimHash(&lead);
    CC_CHECK(!CcCoopApply(&lead,"mine_learn_lead",
        (CcId)lead.mine.return_revision,0,1,error,sizeof(error)) &&
        CcSimHash(&lead)==before);

    CcSimInit(&fallback,UINT32_C(0x106fa11));
    silverwick=fallback.dungeons[0].settlement_id;
    fallback.player.location_id=silverwick;fallback.carriage.location_id=silverwick;
    jory=CcSimMineEvidenceContact(&fallback);CC_CHECK(jory!=NULL);
    for(int32_t i=0;i<fallback.character_count;++i)
        if(fallback.characters[i].id==jory->id)
            fallback.characters[i].current_settlement_id=fallback.settlements[1].id;
    CC_CHECK(!CcSimMineLeadSupported(&fallback));
    CcCommand document={.kind=CC_COMMAND_MINE_LEARN_LEAD,
        .target_id=(CcId)fallback.mine.return_revision,.amount=2};
    Check(CcSimApply(&fallback,&document,error,sizeof(error)));
    CC_CHECK(fallback.mine.lead_document &&
        fallback.mine.lead_source_id==silverwick && fallback.mine.lead_event_id!=0U);

    ReadyAtHaulers(&haul);
    Apply(&haul,CC_COMMAND_MINE_BARGAIN,0);
    CC_CHECK(CcMinePackGood(&haul,CC_GOOD_GOLD)==1);
    ReturnFromMineToSilverwick(&haul);
    CC_CHECK(haul.player.cargo[CC_GOOD_GOLD]==1 &&
        CcSimMineReturnEvidence(&haul)==CC_MINE_RETURN_HAUL);
    const CcCustodyEntry *carried=NULL;
    for(int32_t i=0;i<CcCustodyEffectiveCapacity(&haul.custody);++i) {
        const CcCustodyEntry *entry=&haul.custody.entries[i];
        if(entry->active&&entry->good==CC_GOOD_GOLD&&
           entry->holder.kind==CC_CUSTODY_PLAYER&&CcSimMineEntryTracked(&haul,entry)) carried=entry;
    }
    CC_CHECK(carried!=NULL && carried->owner_id==haul.goblins.id);
    CcCommand sale={.kind=CC_COMMAND_TRADE,.good=CC_GOOD_GOLD,.amount=-1};
    Check(CcSimApply(&haul,&sale,error,sizeof(error)));
    CC_CHECK(haul.player.cargo[CC_GOOD_GOLD]==0 &&
        CcSimMineReturnEvidence(&haul)==CC_MINE_RETURN_HAUL&&
        haul.mine.haul_receipt_event_id!=0U&&
        haul.mine.haul_receipt_good==CC_GOOD_GOLD&&
        haul.mine.haul_receipt_quantity==1);
    bool delivered=false;
    for(int32_t i=0;i<CcCustodyEffectiveCapacity(&haul.custody);++i) {
        const CcCustodyEntry *entry=&haul.custody.entries[i];
        if(!entry->active&&entry->good==CC_GOOD_GOLD&&entry->quantity==0&&
           entry->holder.kind==CC_CUSTODY_STORE&&entry->holder.id==haul.player.location_id&&
           entry->owner_id==haul.player.location_id&&entry->last_event_id==haul.mine.haul_receipt_event_id&&
           CcSimMineEntryTracked(&haul,entry)) delivered=true;
    }
    CC_CHECK(delivered);
    CcSettlement *market=CcSimSettlementMutable(&haul,haul.player.location_id);
    CC_CHECK(market!=NULL);
    market->stock[CC_GOOD_GOLD]=0;
    CC_CHECK(CcSimMineReturnEvidence(&haul)==CC_MINE_RETURN_HAUL);
    jory=CcSimMineEvidenceContact(&haul);CC_CHECK(jory!=NULL);
    int32_t jory_before=KnowledgeCount(&haul,jory->id);
    int32_t others_before=0;
    for(int32_t i=0;i<haul.character_count;++i)
        if(haul.characters[i].id!=jory->id) others_before+=haul.characters[i].knowledge_count;
    before=CcSimHash(&haul);
    CC_CHECK(!CcCoopApply(&haul,"mine_report_return",
        (CcId)(haul.mine.return_revision-1),0,0,error,sizeof(error))&&
        CcSimHash(&haul)==before);
    Check(CcCoopApply(&haul,"mine_report_return",
        (CcId)haul.mine.return_revision,0,0,error,sizeof(error)));
    CC_CHECK(haul.mine.report_kind==CC_MINE_RETURN_HAUL&&
        haul.mine.report_good==CC_GOOD_GOLD&&haul.mine.report_quantity==1&&
        haul.mine.report_recipient_id==jory->id&&haul.mine.report_event_id!=0U&&
        KnowledgeCount(&haul,jory->id)==jory_before+1);
    const CcEvent *report_event=CcSimEvent(&haul,haul.mine.report_event_id);
    CC_CHECK(report_event!=NULL&&report_event->magnitude==CC_MINE_RETURN_HAUL&&
        strstr(report_event->text,"sale of 1 Raw Gold")!=NULL&&
        strstr(report_event->text,"still yield")!=NULL);
    int32_t others_after=0;
    for(int32_t i=0;i<haul.character_count;++i)
        if(haul.characters[i].id!=jory->id) others_after+=haul.characters[i].knowledge_count;
    CC_CHECK(others_after==others_before);
    before=CcSimHash(&haul);
    CC_CHECK(!CcCoopApply(&haul,"mine_report_return",
        (CcId)haul.mine.return_revision,0,0,error,sizeof(error))&&
        CcSimHash(&haul)==before);
    Check(CcSaveWrite("mine-return-106.ccsave",&haul,error,sizeof(error)));
    Check(CcSaveRead("mine-return-106.ccsave",&restored,error,sizeof(error)));
    CC_CHECK(CcSimHash(&haul)==CcSimHash(&restored)&&
        restored.mine.report_event_id==haul.mine.report_event_id&&
        CcSimMineReturnEvidence(&restored)==CC_MINE_RETURN_HAUL);

    CcSimInit(&old_gold,UINT32_C(0x104601d));
    silverwick=old_gold.dungeons[0].settlement_id;
    old_gold.player.location_id=silverwick;old_gold.carriage.location_id=silverwick;
    old_gold.player.cargo[CC_GOOD_GOLD]=1;
    old_gold.mine.encounter_outcome=CC_MINE_ENCOUNTER_BARGAINED;
    CC_CHECK(CcSimMineReturnEvidence(&old_gold)==CC_MINE_RETURN_NONE);
    CcCommand report={.kind=CC_COMMAND_MINE_REPORT_RETURN,
        .target_id=(CcId)old_gold.mine.return_revision};
    before=CcSimHash(&old_gold);
    CC_CHECK(!CcSimApply(&old_gold,&report,error,sizeof(error))&&
        CcSimHash(&old_gold)==before&&strstr(error,"unfinished")!=NULL);
    (void)remove("mine-return-106.ccsave");
}

static void EnterMineThroughMiddle(CcSim *sim)
{
    AtBranch(sim,false);
    sim->player.cargo[CC_GOOD_BREAD]=2;
    CcCommand visit={.kind=CC_COMMAND_VISIT_MINE,.target_id=CcMineSite(sim)->id};
    Check(CcSimApply(sim,&visit,error,sizeof(error)));
    ApplyGood(sim,CC_COMMAND_MINE_PACK,CC_GOOD_BREAD,1);
    Walk(sim,15,3);Apply(sim,CC_COMMAND_MINE_USE,0);
    Walk(sim,15,9);Apply(sim,CC_COMMAND_MINE_USE,0);
    CC_CHECK(sim->mine.bar_open && !sim->mine.bypass_route_seen);
}

static void TestMineSurveyMigrationAndReturns(void)
{
    static CcSim restored,reloaded;
    CcSim information,retreat;
    const char *fixture=CC_TEST_SOURCE_DIR
        "/tests/fixtures/shipped/schema-105-generator-25-mine-custody-journal.ccsave";
    Check(CcSaveRead(fixture,&restored,error,sizeof(error)));
    Check(CcSimValidate(&restored,error,sizeof(error)));
    uint64_t migrated_hash=CcSimHash(&restored);
    /* Schema 110 includes the Underroad network, relief loading, and company memory. */
    CC_CHECK(migrated_hash==UINT64_C(14688430958042272003));
    Check(CcSaveRead(fixture,&reloaded,error,sizeof(error)));
    CC_CHECK(CcSimHash(&reloaded)==migrated_hash);
    const CcCustodyEntry *depleted_gold=NULL;
    for(int32_t i=0;i<CcCustodyEffectiveCapacity(&restored.custody);++i)
        if(restored.custody.entries[i].id==restored.mine.gold_source_entry_id)
            depleted_gold=&restored.custody.entries[i];
    CC_CHECK(restored.schema_version==CC_SIM_SCHEMA_VERSION&&restored.generator_version==25U&&
        restored.mine.return_revision==1&&restored.mine.surveyed&&
        restored.mine.survey_event_id==0U&&
        restored.mine.survey_read_day==0&&restored.mine.survey_observed_day==0&&
        restored.mine.iron_source_entry_id!=0U&&
        restored.mine.gold_source_entry_id!=0U&&
        restored.mine.gems_source_entry_id!=0U&&
        depleted_gold!=NULL&&!depleted_gold->active&&depleted_gold->quantity==0&&
        CcMineSourceGood(&restored,CC_GOOD_GOLD)==0&&
        TrackedGoodAt(&restored,CC_GOOD_IRON,CC_CUSTODY_MINE_PACK,
            restored.player.id)==1&&
        TrackedGoodAt(&restored,CC_GOOD_IRON,CC_CUSTODY_SITE,
            restored.mine.cache_id)==3&&
        restored.player.cargo[CC_GOOD_GOLD]==3&&
        restored.player.cargo[CC_GOOD_IRON]==1&&
        CarriedTrackedGood(&restored,CC_GOOD_GOLD)==0&&
        CarriedTrackedGood(&restored,CC_GOOD_IRON)==0&&
        CcSimMineReturnEvidence(&restored)==CC_MINE_RETURN_NONE);
    restored.player.cargo[CC_GOOD_BREAD]=1;
    ApplyGood(&restored,CC_COMMAND_MINE_PACK,CC_GOOD_BREAD,1);
    Walk(&restored,15,3);Apply(&restored,CC_COMMAND_MINE_USE,0);
    Walk(&restored,26,4);
    int32_t revision=restored.mine.revision;
    Apply(&restored,CC_COMMAND_MINE_USE,0);
    CC_CHECK(restored.mine.survey_event_id!=0U&&restored.mine.survey_read_day==restored.current_day&&
        restored.mine.survey_observed_day==0&&restored.mine.revision==revision+1);
    const CcEvent *reading=CcSimEvent(&restored,restored.mine.survey_event_id);
    CC_CHECK(reading!=NULL&&strstr(reading->text,"Earlier observation date unknown")!=NULL);
    uint64_t reread=CcSimHash(&restored);
    CcCommand use={.kind=CC_COMMAND_MINE_USE,.target_id=(CcId)restored.mine.revision};
    Check(CcSimApply(&restored,&use,error,sizeof(error)));
    CC_CHECK(CcSimHash(&restored)==reread);

    EnterMineThroughMiddle(&information);
    Walk(&information,26,4);Apply(&information,CC_COMMAND_MINE_USE,0);
    CC_CHECK(information.mine.survey_event_id!=0U&&
        information.mine.survey_observed_day==information.current_day);
    ReturnFromMineToSilverwick(&information);
    CC_CHECK(CcSimMineReturnEvidence(&information)==CC_MINE_RETURN_INFORMATION);
    CcCommand report={.kind=CC_COMMAND_MINE_REPORT_RETURN,
        .target_id=(CcId)information.mine.return_revision};
    Check(CcSimApply(&information,&report,error,sizeof(error)));
    CC_CHECK(information.mine.report_kind==CC_MINE_RETURN_INFORMATION);

    EnterMineThroughMiddle(&retreat);
    Walk(&retreat,26,16);Apply(&retreat,CC_COMMAND_MINE_CONTEST,0);
    Apply(&retreat,CC_COMMAND_MINE_BREAK_CONTACT,37);
    CC_CHECK(retreat.mine.encounter_outcome==CC_MINE_ENCOUNTER_BROKEN_CONTACT&&
        !retreat.mine.bypass_route_seen&&!retreat.mine.surveyed);
    ReturnFromMineToSilverwick(&retreat);
    report=(CcCommand){.kind=CC_COMMAND_MINE_REPORT_RETURN,
        .target_id=(CcId)retreat.mine.return_revision};
    uint64_t before=CcSimHash(&retreat);
    CC_CHECK(!CcSimApply(&retreat,&report,error,sizeof(error))&&
        CcSimHash(&retreat)==before&&retreat.mine.report_event_id==0U);
}
static int WriteSharedMineFixture(const char *mode,const char *path)
{
    static CcSim sim;
    bool return_command=false;
    if(strcmp(mode,"lead")==0) {
        CcSimInit(&sim,UINT32_C(0x1061ead));
        CcId silverwick=sim.dungeons[0].settlement_id;
        sim.player.location_id=silverwick;sim.carriage.location_id=silverwick;
        if(!CcSimMineLeadSupported(&sim)) return 1;
        return_command=true;
    } else {
        ReadyAtHaulers(&sim);
        if(strcmp(mode,"contest")==0) Apply(&sim,CC_COMMAND_MINE_CONTEST,0);
        else if(strcmp(mode,"report")==0) {
            Apply(&sim,CC_COMMAND_MINE_BARGAIN,0);
            ReturnFromMineToSilverwick(&sim);
            return_command=true;
        } else if(strcmp(mode,"bargain")!=0) return 1;
    }
    uint8_t *bytes=NULL;
    size_t length=0;
    if(!CcCoopEncode(&sim,&bytes,&length,error,sizeof(error))) return 1;
    FILE *file=fopen(path,"wb");
    bool written=file!=NULL && fwrite(bytes,1,length,file)==length;
    if(file!=NULL && fclose(file)!=0) written=false;
    CcCoopFree(bytes);
    if(!written) return 1;
    printf("%u\n",return_command?sim.mine.return_revision:sim.mine.revision);
    return 0;
}

int main(int argc,char **argv)
{
    if(argc==4 && strcmp(argv[1],"--write-shared-mine-fixture")==0)
        return WriteSharedMineFixture(argv[2],argv[3]);
    static CcSim sim,restored,changed,haul,loaded,legacy,capacity,prechange;
    static CcSim old_pace,new_pace;
    static CcSim bargain,contest,withdrawn,failed;
    (void)remove("mine-load-replay.ccsave");
    (void)remove("mine-load-roundtrip-103.ccsave");
    (void)remove("mine-load-schema-102-fixture.ccsave");
    (void)remove("mine-load-stow.ccsave");
    (void)remove("mine-roundtrip.ccsave");
    (void)remove("mine-encounter-active.ccsave");
    (void)remove("mine-replay.ccsave");
    (void)remove("mine-load-roundtrip.ccsave");
    (void)remove("mine-load-schema-102.ccsave");
    CcSimInit(&changed,0x71a7e5);
    uint64_t initialized_hash=CcSimHash(&changed);
    CcId initialized_source=changed.mine.source_id, initialized_cache=changed.mine.cache_id;
    CcMineInitializeLoad(&changed);
    CC_CHECK(CcSimHash(&changed)==initialized_hash &&
        changed.mine.source_id==initialized_source && changed.mine.cache_id==initialized_cache);
    changed.mine.pack[CC_GOOD_BREAD]=-1;
    CC_CHECK(CcMinePackUsed(&changed)==INT_MAX);
    capacity=changed;
    capacity.mine.source_id=capacity.mine.source_owner_id=0;
    capacity.mine.cache_id=capacity.mine.cache_owner_id=0;
    CcCustodyInit(&capacity.custody);
    capacity.custody.next_id=UINT64_MAX-2U;
    CcMineInitializeLoad(&capacity);
    CC_CHECK(capacity.mine.source_id==0 && capacity.mine.cache_id==0 &&
        capacity.custody.next_id==UINT64_MAX-2U);
    /* This file was written by the released schema-102 build at 2d56168d.
       A 102 hash reads only the shipped 96 custody rows before migration. */
    CcSimInit(&prechange,0x71a7e5);
    prechange.schema_version=102U;
    prechange.mine.source_id=prechange.mine.source_owner_id=0;
    prechange.mine.cache_id=prechange.mine.cache_owner_id=0;
    prechange.mine.source_x=prechange.mine.source_y=0;
    prechange.mine.cache_x=prechange.mine.cache_y=0;
    prechange.mine.source_released=false;
    CcCustodyInit(&prechange.custody);
    prechange.custody.capacity=CC_CUSTODY_LEGACY_CAPACITY;
    CC_CHECK(CcSimHash(&prechange)==UINT64_C(2100520264052232360));
    Check(CcSaveRead(CC_TEST_SOURCE_DIR "/tests/fixtures/shipped/schema-102-generator-25-prechange.ccsave",
        &restored,error,sizeof(error)));
    CC_CHECK(restored.schema_version==CC_SIM_SCHEMA_VERSION &&
        restored.custody.capacity==CC_CUSTODY_CAPACITY &&
        CcMineSourceUsed(&restored)==13 && CcMineCacheUsed(&restored)==0);
    for(int i=0;i<2;++i) AtBranch(&sim,i!=0);
    ReadyAtHaulers(&bargain);
    CcCustodyHolder source={CC_CUSTODY_SITE,bargain.mine.source_id};
    CcCustodyHolder carried={CC_CUSTODY_MINE_PACK,bargain.player.id};
    uint64_t closed_source_hash=CcSimHash(&bargain);
    CC_CHECK(CcSimTransferMineGoods(&bargain,source,carried,CC_GOOD_GOLD,1,
        (uint64_t)bargain.mine.revision+1U)==CC_CUSTODY_FORBIDDEN);
    CC_CHECK(CcSimHash(&bargain)==closed_source_hash);
    /* The walk around the closed bar reaches the source without changing
       custody. Add the eighth carried unit, then prove that the offered Bread
       is debited before the Gold credit checks pack capacity. */
    bargain.mine.pack[CC_GOOD_IRON]+=1;
    bargain.player.cargo[CC_GOOD_IRON]-=1;
    CC_CHECK(CcMinePackUsed(&bargain)==CC_MINE_PACK_CAPACITY &&
        CcMinePackGood(&bargain,CC_GOOD_BREAD)==2);
    failed=bargain;
    CcCommand stale_bargain={.kind=CC_COMMAND_MINE_BARGAIN,
        .target_id=(CcId)(failed.mine.revision-1)};
    uint64_t failed_hash=CcSimHash(&failed);
    CC_CHECK(!CcSimApply(&failed,&stale_bargain,error,sizeof(error)) &&
        CcSimHash(&failed)==failed_hash);
    failed=bargain;
    failed.mine.pack[CC_GOOD_BREAD]=1;
    CcCommand bargain_command={.kind=CC_COMMAND_MINE_BARGAIN,
        .target_id=(CcId)failed.mine.revision};
    failed_hash=CcSimHash(&failed);
    CC_CHECK(!CcSimApply(&failed,&bargain_command,error,sizeof(error)) &&
        CcSimHash(&failed)==failed_hash);
    int32_t bread_total=CcSimTrackedGood(&bargain,CC_GOOD_BREAD);
    int32_t gold_total=CcSimTrackedGood(&bargain,CC_GOOD_GOLD);
    Apply(&bargain,CC_COMMAND_MINE_BARGAIN,0);
    CC_CHECK(bargain.mine.encounter_outcome==CC_MINE_ENCOUNTER_BARGAINED &&
        !bargain.mine.source_released && CcMinePackUsed(&bargain)==7 &&
        CcMinePackGood(&bargain,CC_GOOD_BREAD)==0 &&
        CcMinePackGood(&bargain,CC_GOOD_GOLD)==1 &&
        CcMineSourceGood(&bargain,CC_GOOD_BREAD)==2 &&
        CcMineSourceGood(&bargain,CC_GOOD_GOLD)==2 &&
        CcSimTrackedGood(&bargain,CC_GOOD_BREAD)==bread_total &&
        CcSimTrackedGood(&bargain,CC_GOOD_GOLD)==gold_total);
    closed_source_hash=CcSimHash(&bargain);
    CC_CHECK(CcSimTransferMineGoods(&bargain,source,carried,CC_GOOD_GOLD,1,
        (uint64_t)bargain.mine.revision+1U)==CC_CUSTODY_FORBIDDEN &&
        CcSimHash(&bargain)==closed_source_hash);

    ReadyAtHaulers(&contest);
    {
        uint64_t shared_open_hash=CcSimHash(&contest);
        CC_CHECK(!CcCoopApply(&contest,"mine_contest",
            (CcId)contest.mine.revision,0,0,error,sizeof(error)) &&
            CcSimHash(&contest)==shared_open_hash &&
            strstr(error,"local company")!=NULL);
    }
    Apply(&contest,CC_COMMAND_MINE_CONTEST,0);
    CC_CHECK(contest.mine.contest_active &&
        contest.mine.source_owner_id==contest.goblins.id);
    {
        uint64_t shared_active_hash=CcSimHash(&contest);
        CC_CHECK(!CcCoopApply(&contest,"mine_resolve_contest",
            (CcId)contest.mine.revision,0,23,error,sizeof(error)) &&
            CcSimHash(&contest)==shared_active_hash &&
            strstr(error,"local combat course")!=NULL);
    }
    Check(CcSaveWrite("mine-encounter-active.ccsave",&contest,error,sizeof(error)));
    Check(CcSaveRead("mine-encounter-active.ccsave",&restored,error,sizeof(error)));
    CC_CHECK(CcSimHash(&restored)==CcSimHash(&contest) &&
        restored.mine.contest_active &&
        restored.mine.source_id==contest.mine.source_id &&
        restored.mine.source_owner_id==restored.goblins.id);
    CcCommand stale_resolution={.kind=CC_COMMAND_MINE_RESOLVE_CONTEST,
        .target_id=(CcId)(contest.mine.revision-1),.amount=23};
    uint64_t active_hash=CcSimHash(&contest);
    CC_CHECK(!CcSimApply(&contest,&stale_resolution,error,sizeof(error)) &&
        CcSimHash(&contest)==active_hash);
    CcCommand resolution={.kind=CC_COMMAND_MINE_RESOLVE_CONTEST,
        .target_id=(CcId)contest.mine.revision,.amount=23};
    Check(CcSimApply(&contest,&resolution,error,sizeof(error)));
    CC_CHECK(!contest.mine.contest_active && contest.mine.source_released &&
        contest.mine.encounter_outcome==CC_MINE_ENCOUNTER_CONTESTED &&
        contest.mine.player_injury==23);
    active_hash=CcSimHash(&contest);
    CC_CHECK(!CcSimApply(&contest,&resolution,error,sizeof(error)) &&
        CcSimHash(&contest)==active_hash);
    ApplyGood(&contest,CC_COMMAND_MINE_TAKE,CC_GOOD_GOLD,1);

    ReadyAtHaulers(&withdrawn);
    Apply(&withdrawn,CC_COMMAND_MINE_CONTEST,0);
    int32_t contact_x=withdrawn.mine.x,contact_y=withdrawn.mine.y;
    Apply(&withdrawn,CC_COMMAND_MINE_BREAK_CONTACT,100);
    CC_CHECK(withdrawn.mine.x==contact_x && withdrawn.mine.y==contact_y &&
        withdrawn.mine.encounter_outcome==CC_MINE_ENCOUNTER_BROKEN_CONTACT &&
        withdrawn.mine.player_injury==100 && !withdrawn.mine.contest_active);
    Apply(&withdrawn,CC_COMMAND_MINE_STEP,3);
    Walk(&withdrawn,26,16);
    Check(CcCoopApply(&withdrawn,"mine_bargain",
        (CcId)withdrawn.mine.revision,0,0,error,sizeof(error)));
    CC_CHECK(withdrawn.mine.encounter_outcome==CC_MINE_ENCOUNTER_BARGAINED);
    {
        /* The turn must survive the whole stop window, not one subtick of it.
           Offered on a single tick, every other tick in the window fell
           through to "Camp at ..." -- which spends the stop and takes the
           branch with it, so the expedition was lost by resting. */
        static CcSim late;
        AtBranch(&late, false);
        int32_t branch = CcMineBranchSubtick(&late);
        CC_CHECK(branch >= 0);
        late.journey.elapsed_subticks = branch + 10;
        late.carriage.progress_milli = (int32_t)(
            (int64_t)late.journey.elapsed_subticks * 1000 /
            late.journey.total_subticks);
        CC_CHECK(CcSimJourneyRoadSiteStop(&late) == CcMineSite(&late));
        late.player.cargo[CC_GOOD_BREAD] = 3;
        CcCommand turn = {.kind = CC_COMMAND_VISIT_MINE,
                          .target_id = CcMineSite(&late)->id};
        Check(CcSimApply(&late, &turn, error, sizeof(error)));
        CC_CHECK(late.mine.phase == CC_MINE_YARD);
        Check(CcSimValidate(&late, error, sizeof(error)));
    }
    CcCommand visit={.kind=CC_COMMAND_VISIT_MINE,.target_id=CcMineSite(&sim)->id};
    sim.player.cargo[CC_GOOD_BREAD]=3;
    Check(CcSimApply(&sim,&visit,error,sizeof(error)));
    Check(CcSimValidate(&sim,error,sizeof(error)));
    /* Schema 103 keeps the haulers' one finite load in custody. The legacy
       food pack and custody-held mine pack share all eight slots. */
    AtBranch(&haul,false);
    haul.player.cargo[CC_GOOD_BREAD]=3;
    CcCommand haul_visit={.kind=CC_COMMAND_VISIT_MINE,.target_id=CcMineSite(&haul)->id};
    Check(CcSimApply(&haul,&haul_visit,error,sizeof(error)));
    ApplyGood(&haul,CC_COMMAND_MINE_PACK,CC_GOOD_BREAD,2);
    Walk(&haul,15,3); Apply(&haul,CC_COMMAND_MINE_USE,0);
    Walk(&haul,26,16);
    CC_CHECK(haul.mine.source_owner_id==haul.goblins.id);
    CC_CHECK(CcMineSourceGood(&haul,CC_GOOD_IRON)==8);
    CC_CHECK(CcMineSourceGood(&haul,CC_GOOD_GOLD)==3);
    CC_CHECK(CcMineSourceGood(&haul,CC_GOOD_GEMS)==2);
    uint64_t inspected=CcSimHash(&haul);
    Check(CcCoopApply(&haul,"mine_inspect",(CcId)haul.mine.revision,0,0,error,sizeof(error)));
    Check(CcSimValidate(&haul,error,sizeof(error)));
    CC_CHECK(CcSimHash(&haul)==inspected);
    CcCommand held_take={.kind=CC_COMMAND_MINE_TAKE,.target_id=(CcId)haul.mine.revision,
        .good=CC_GOOD_IRON,.amount=1};
    uint64_t held_hash=CcSimHash(&haul);
    CC_CHECK(!CcSimApply(&haul,&held_take,error,sizeof(error)) && CcSimHash(&haul)==held_hash);
    /* #763 will set this only after its goblin-hauler bargain. This fixture
       exercises the released command path without adding encounter choices. */
    haul.mine.source_released=true;
    haul.mine.encounter_outcome=CC_MINE_ENCOUNTER_CONTESTED;
    int32_t iron_total=CcSimTrackedGood(&haul,CC_GOOD_IRON);
    Check(CcCoopApply(&haul,"mine_take",(CcId)haul.mine.revision,CC_GOOD_IRON,3,error,sizeof(error)));
    Check(CcSimValidate(&haul,error,sizeof(error)));
    CC_CHECK(CcMineSourceGood(&haul,CC_GOOD_IRON)==5 && CcMinePackGood(&haul,CC_GOOD_IRON)==3);
    CC_CHECK(CcSimTrackedGood(&haul,CC_GOOD_IRON)==iron_total);
    /* A schema-108 journal must keep the old walking pace when it replays
       a step with the haulers' tracked goods in the pack. */
    old_pace=haul;new_pace=haul;
    old_pace.schema_version=108U;
    old_pace.mine.steps=2;new_pace.mine.steps=2;
    CC_CHECK(CcMineCarriedCrates(&old_pace)==0);
    CC_CHECK(CcMineCarriedCrates(&new_pace)==3);
    int32_t old_minutes=old_pace.clock.minute_subticks;
    int32_t new_minutes=new_pace.clock.minute_subticks;
    Apply(&old_pace,CC_COMMAND_MINE_STEP,3);
    Apply(&new_pace,CC_COMMAND_MINE_STEP,3);
    CC_CHECK(old_pace.clock.minute_subticks==old_minutes);
    CC_CHECK(new_pace.clock.minute_subticks==new_minutes+5*CC_WORLD_MINUTE_SUBTICKS);
    CcCommand stale_take={.kind=CC_COMMAND_MINE_TAKE,.target_id=(CcId)haul.mine.revision,
        .good=CC_GOOD_IRON,.amount=1};
    ApplyGood(&haul,CC_COMMAND_MINE_TAKE,CC_GOOD_IRON,4);
    uint64_t stale_hash=CcSimHash(&haul);
    CC_CHECK(!CcSimApply(&haul,&stale_take,error,sizeof(error)) && CcSimHash(&haul)==stale_hash);
    CC_CHECK(CcMinePackUsed(&haul)==CC_MINE_PACK_CAPACITY && CcMineSourceGood(&haul,CC_GOOD_IRON)==1);
    inspected=CcSimHash(&haul); Apply(&haul,CC_COMMAND_MINE_INSPECT,0);
    CC_CHECK(CcSimHash(&haul)==inspected);
    Walk(&haul,5,15);
    CcCommand food_cache={.kind=CC_COMMAND_MINE_CACHE,.target_id=(CcId)haul.mine.revision,
        .good=CC_GOOD_BREAD,.amount=1};
    uint64_t food_cache_hash=CcSimHash(&haul);
    CC_CHECK(!CcSimApply(&haul,&food_cache,error,sizeof(error)) &&
        CcSimHash(&haul)==food_cache_hash && strstr(error,"Rope Store")!=NULL);
    Check(CcCoopApply(&haul,"mine_cache",(CcId)haul.mine.revision,CC_GOOD_IRON,7,error,sizeof(error)));
    Check(CcSimValidate(&haul,error,sizeof(error)));
    CC_CHECK(CcMineCacheGood(&haul,CC_GOOD_IRON)==7 && CcMinePackUsed(&haul)==1);
    CcJournal *load_journal=CcJournalStart("mine-load-replay.ccsave",&haul,error,sizeof(error));
    CC_CHECK(load_journal!=NULL);
    CcCommand recover={.kind=CC_COMMAND_MINE_CACHE,.target_id=(CcId)haul.mine.revision,
        .good=CC_GOOD_IRON,.amount=-2};
    Check(CcJournalApply(load_journal,&haul,&recover,error,sizeof(error)));
    CcJournalAbandon(&load_journal);
    load_journal=CcJournalResume("mine-load-replay.ccsave",&loaded,error,sizeof(error));
    CC_CHECK(load_journal!=NULL);
    CC_CHECK(CcSimHash(&loaded)==CcSimHash(&haul));
    Check(CcJournalClose(&load_journal,&loaded,error,sizeof(error)));
    CC_CHECK(CcMineCacheGood(&haul,CC_GOOD_IRON)==5 && CcMinePackGood(&haul,CC_GOOD_IRON)==2);
    Walk(&haul,26,16);
    uint64_t remote_hash=CcSimHash(&haul);
    CcCommand remote_cache={.kind=CC_COMMAND_MINE_CACHE,.target_id=(CcId)haul.mine.revision,
        .good=CC_GOOD_IRON,.amount=1};
    CC_CHECK(!CcSimApply(&haul,&remote_cache,error,sizeof(error)) && CcSimHash(&haul)==remote_hash);
    ApplyGood(&haul,CC_COMMAND_MINE_TAKE,CC_GOOD_IRON,1);
    ApplyGood(&haul,CC_COMMAND_MINE_TAKE,CC_GOOD_GOLD,3);
    Walk(&haul,5,15);
    uint64_t full_cache_hash=CcSimHash(&haul);
    CcCommand full_cache={.kind=CC_COMMAND_MINE_CACHE,.target_id=(CcId)haul.mine.revision,
        .good=CC_GOOD_GOLD,.amount=3};
    CC_CHECK(!CcSimApply(&haul,&full_cache,error,sizeof(error)) && CcSimHash(&haul)==full_cache_hash);
    ApplyGood(&haul,CC_COMMAND_MINE_CACHE,CC_GOOD_GOLD,2);
    Walk(&haul,26,16);
    ApplyGood(&haul,CC_COMMAND_MINE_TAKE,CC_GOOD_GEMS,2);
    CC_CHECK(CcMineSourceUsed(&haul)==0 && CcMineCacheUsed(&haul)==7);
    CC_CHECK(CcMineCacheGood(&haul,CC_GOOD_GOLD)==2 &&
        CcMinePackGood(&haul,CC_GOOD_GOLD)==1 && CcMinePackGood(&haul,CC_GOOD_GEMS)==2);
    unsigned char *shared_bytes=NULL;
    size_t shared_length=0;
    Check(CcCoopEncode(&haul,&shared_bytes,&shared_length,error,sizeof(error)));
    Check(CcCoopDecode(&loaded,shared_bytes,shared_length,error,sizeof(error)));
    CcCoopFree(shared_bytes);
    CC_CHECK(CcSimHash(&loaded)==CcSimHash(&haul));
    Check(CcSaveWrite("mine-load-roundtrip-103.ccsave",&haul,error,sizeof(error)));
    Check(CcSaveRead("mine-load-roundtrip-103.ccsave",&loaded,error,sizeof(error)));
    CC_CHECK(CcSimHash(&haul)==CcSimHash(&loaded));
    CC_CHECK(loaded.mine.source_id==haul.mine.source_id && loaded.mine.cache_id==haul.mine.cache_id);
    CC_CHECK(loaded.mine.source_owner_id==loaded.goblins.id && loaded.mine.cache_owner_id==loaded.player.id);
    CC_CHECK(loaded.mine.source_released);
    CC_CHECK(CcMinePackGood(&loaded,CC_GOOD_GOLD)==CcMinePackGood(&haul,CC_GOOD_GOLD));
    /* This is a schema-102 fixture. It contains the original custody model,
       so migration seeds the authored finite source exactly once. */
    legacy=haul;
    legacy.schema_version=102U;
    legacy.mine.source_id=0;
    legacy.mine.source_owner_id=0;
    legacy.mine.cache_id=0;
    legacy.mine.cache_owner_id=0;
    legacy.mine.source_x=legacy.mine.source_y=0;
    legacy.mine.cache_x=legacy.mine.cache_y=0;
    legacy.mine.source_released=false;
    CcCustodyInit(&legacy.custody);
    legacy.custody.capacity=CC_CUSTODY_LEGACY_CAPACITY;
    for (int32_t slot=0;slot<CC_CUSTODY_LEGACY_CAPACITY;++slot) {
        legacy.custody.entries[slot]=(CcCustodyEntry){.id=(uint64_t)slot+1U,
            .revision=1,.owner_id=legacy.player.id,
            .holder={CC_CUSTODY_STORE,legacy.settlements[0].id},
            .kind=CC_CUSTODY_GOODS,.quantity=1,.good=CC_GOOD_BREAD,
            .condition=100,.active=true};
    }
    legacy.custody.next_id=CC_CUSTODY_LEGACY_CAPACITY+1U;
    /* A full historic table has no hidden three-slot extension while old
       commands and runtime work replay against schema 102. */
    CC_CHECK(!CcSimLeaveBodyPurse(&legacy,legacy.characters[0].id,
        legacy.settlements[0].id,1,1));
    CC_CHECK(legacy.custody.entries[CC_CUSTODY_LEGACY_CAPACITY].id==0);
    Check(CcSaveWrite("mine-load-schema-102-fixture.ccsave",&legacy,error,sizeof(error)));
    Check(CcSaveRead("mine-load-schema-102-fixture.ccsave",&restored,error,sizeof(error)));
    CC_CHECK(restored.schema_version==CC_SIM_SCHEMA_VERSION);
    CC_CHECK(restored.mine.source_id==haul.mine.source_id && restored.mine.cache_id==haul.mine.cache_id);
    CC_CHECK(CcMineSourceUsed(&restored)==13 && CcMineCacheUsed(&restored)==0);
    CC_CHECK(CcMinePackUsed(&restored)==legacy.mine.pack[CC_GOOD_BREAD]);
    CC_CHECK(restored.mine.source_x==26 && restored.mine.source_y==16 &&
        restored.mine.cache_x==5 && restored.mine.cache_y==15);
    CC_CHECK(restored.custody.entries[CC_CUSTODY_LEGACY_CAPACITY-1].id==
        CC_CUSTODY_LEGACY_CAPACITY &&
        restored.custody.entries[CC_CUSTODY_LEGACY_CAPACITY-1].owner_id==restored.player.id);
    int32_t anchor=sim.journey.elapsed_subticks;
    CcSimAdvanceRuntimeTicks(&sim,1000);
    CC_CHECK(sim.journey.elapsed_subticks==anchor);
    int32_t bread=CcSimTrackedGood(&sim,CC_GOOD_BREAD);
    CcCommand pack={.kind=CC_COMMAND_MINE_PACK,.target_id=(CcId)sim.mine.revision,.good=CC_GOOD_BREAD,.amount=2};
    Check(CcSimApply(&sim,&pack,error,sizeof(error)));
    CC_CHECK(sim.player.cargo[CC_GOOD_BREAD]==1 && sim.mine.pack[CC_GOOD_BREAD]==2);
    CC_CHECK(CcSimTrackedGood(&sim,CC_GOOD_BREAD)==bread);
    {
        CcCommand unload=pack;
        unload.target_id=(CcId)sim.mine.revision;
        unload.amount=-1;
        Check(CcSimApply(&sim,&unload,error,sizeof(error)));
        CC_CHECK(sim.player.cargo[CC_GOOD_BREAD]==2 && sim.mine.pack[CC_GOOD_BREAD]==1);
        pack.target_id=(CcId)sim.mine.revision;
        pack.amount=1;
        Check(CcSimApply(&sim,&pack,error,sizeof(error)));
        CC_CHECK(sim.player.cargo[CC_GOOD_BREAD]==1 && sim.mine.pack[CC_GOOD_BREAD]==2);
    }
    {
        CcCommand remote=pack;
        uint64_t held;
        changed=sim; changed.mine.x=15; changed.mine.y=4;
        remote.target_id=(CcId)changed.mine.revision;
        held=CcSimHash(&changed);
        CC_CHECK(!CcSimApply(&changed,&remote,error,sizeof(error)) && CcSimHash(&changed)==held);
        changed=sim; changed.mine.phase=CC_MINE_LEVEL; changed.mine.x=5; changed.mine.y=4;
        changed.mine.light=18; changed.mine.steps=0; changed.mine.seen=1U;
        remote.target_id=(CcId)changed.mine.revision;
        held=CcSimHash(&changed);
        CC_CHECK(!CcSimApply(&changed,&remote,error,sizeof(error)) && CcSimHash(&changed)==held);
        changed=sim; changed.mine.pack[CC_GOOD_BREAD]=CC_MINE_PACK_CAPACITY;
        changed.player.cargo[CC_GOOD_BREAD]=1;
        remote.target_id=(CcId)changed.mine.revision;
        held=CcSimHash(&changed);
        CC_CHECK(!CcSimApply(&changed,&remote,error,sizeof(error)) && CcSimHash(&changed)==held);
        changed=sim;
        changed.player.cargo[CC_GOOD_IRON]+=changed.player.cargo_capacity-
            CcPlayerCargoUsed(&changed.player);
        remote.kind=CC_COMMAND_MINE_USE; remote.target_id=(CcId)changed.mine.revision;
        held=CcSimHash(&changed);
        CC_CHECK(!CcSimApply(&changed,&remote,error,sizeof(error)) && CcSimHash(&changed)==held);
    }
    uint64_t before=CcSimHash(&sim);
    pack.target_id=(CcId)sim.mine.revision;
    pack.amount=2;
    CC_CHECK(!CcSimApply(&sim,&pack,error,sizeof(error)) && CcSimHash(&sim)==before);
    CcCommand outside={.kind=CC_COMMAND_PASS_ROAD_SITE,.target_id=sim.mine.site_id};
    CC_CHECK(!CcSimApply(&sim,&outside,error,sizeof(error)) && CcSimHash(&sim)==before);
    const char *path="mine-roundtrip.ccsave";
    /* Shipped schema-59 mine saves retain the active visit on upgrade. */
    changed=sim;changed.schema_version=59U;
    Check(CcSaveWrite(path,&changed,error,sizeof(error)));
    Check(CcSaveRead(path,&restored,error,sizeof(error)));
    CC_CHECK(restored.schema_version==CC_SIM_SCHEMA_VERSION);
    CC_CHECK(restored.mine.phase==changed.mine.phase);
    CC_CHECK(restored.mine.site_id==changed.mine.site_id);
    CC_CHECK(restored.mine.x==changed.mine.x && restored.mine.y==changed.mine.y);
    CC_CHECK(restored.mine.revision==changed.mine.revision);
    CC_CHECK(restored.mine.return_speed==changed.mine.return_speed);
    CC_CHECK(restored.mine.light==changed.mine.light && restored.mine.steps==changed.mine.steps);
    CC_CHECK(restored.mine.seen==changed.mine.seen);
    CC_CHECK(restored.mine.bar_open==changed.mine.bar_open);
    CC_CHECK(restored.mine.surveyed==changed.mine.surveyed);
    for(int32_t good=0;good<CC_GOOD_COUNT;++good) {
        CC_CHECK(restored.mine.pack[good]==changed.mine.pack[good]);
    }
    restored.schema_version=59U;
    CC_CHECK(CcSimHash(&restored)==CcSimHash(&changed));
    Check(CcSaveWrite(path,&sim,error,sizeof(error)));
    Check(CcSaveRead(path,&restored,error,sizeof(error)));
    CC_CHECK(CcSimHash(&sim)==CcSimHash(&restored));
    Walk(&sim,15,4);Apply(&sim,CC_COMMAND_MINE_USE,0);
    CC_CHECK(sim.mine.phase==CC_MINE_LEVEL && sim.mine.pack[CC_GOOD_BREAD]==1);
    CC_CHECK(!CcMineWalkable(&sim,CC_MINE_LEVEL,15,10));
    Walk(&sim,26,4);Apply(&sim,CC_COMMAND_MINE_USE,0);
    CC_CHECK(sim.mine.surveyed);
    uint64_t read_hash=CcSimHash(&sim);
    CcCommand reread={.kind=CC_COMMAND_MINE_USE,.target_id=(CcId)sim.mine.revision};
    Check(CcSimApply(&sim,&reread,error,sizeof(error)));
    CC_CHECK(CcSimHash(&sim)==read_hash &&
        strcmp(CcMineAction(&sim),"Reread the workers' records")==0);
    Walk(&sim,15,9);Apply(&sim,CC_COMMAND_MINE_USE,0);
    CC_CHECK(sim.mine.bar_open && CcMineWalkable(&sim,CC_MINE_LEVEL,15,10));
    Walk(&sim,26,16);Walk(&sim,5,15);
    Check(CcSaveWrite(path,&sim,error,sizeof(error)));
    Check(CcSaveRead(path,&restored,error,sizeof(error)));
    CC_CHECK(CcSimHash(&sim)==CcSimHash(&restored));
    changed=sim;changed.mine.x+=1;CC_CHECK(CcSimHash(&changed)!=CcSimHash(&sim));
    changed=sim;changed.mine.pack[CC_GOOD_BREAD]+=1;CC_CHECK(CcSimHash(&changed)!=CcSimHash(&sim));
    changed=sim;changed.mine.bar_open=false;CC_CHECK(CcSimHash(&changed)!=CcSimHash(&sim));
    changed=sim;changed.mine.surveyed=false;CC_CHECK(CcSimHash(&changed)!=CcSimHash(&sim));
    changed=sim;changed.mine.x=0;CC_CHECK(!CcSimValidate(&changed,error,sizeof(error)));
    CcJournal *journal=CcJournalStart("mine-replay.ccsave",&sim,error,sizeof(error));
    CC_CHECK(journal!=NULL);
    CcCommand step={.kind=CC_COMMAND_MINE_STEP,.target_id=(CcId)sim.mine.revision,.amount=1};
    Check(CcJournalApply(journal,&sim,&step,error,sizeof(error)));
    CcJournalAbandon(&journal);
    journal=CcJournalResume("mine-replay.ccsave",&restored,error,sizeof(error));
    CC_CHECK(journal!=NULL && CcSimHash(&restored)==CcSimHash(&sim));
    Check(CcJournalClose(&journal,&restored,error,sizeof(error)));
    Walk(&restored,5,4);Apply(&restored,CC_COMMAND_MINE_USE,0);
    CC_CHECK(restored.mine.phase==CC_MINE_YARD && restored.mine.x==15 && restored.mine.y==4);
    Walk(&restored,15,17);
    CcCommand board={.kind=CC_COMMAND_MINE_USE,.target_id=(CcId)restored.mine.revision};
    Check(CcSimApply(&restored,&board,error,sizeof(error)));
    CC_CHECK(error[0]=='\0');
    Check(CcSimValidate(&restored,error,sizeof(error)));
    CC_CHECK(restored.mine.phase==CC_MINE_NONE && restored.mine.surveyed && restored.mine.bar_open);
    CC_CHECK(restored.player.cargo[CC_GOOD_BREAD]==2 && restored.journey.elapsed_subticks==anchor);
    CcSimAdvanceRuntimeTicks(&restored,1);CC_CHECK(restored.journey.elapsed_subticks>anchor);
    CcSimInit(&sim,123);sim.schema_version=57;
    Check(CcSaveWrite(path,&sim,error,sizeof(error)));
    Check(CcSaveRead(path,&restored,error,sizeof(error)));
    CC_CHECK(restored.schema_version==CC_SIM_SCHEMA_VERSION && restored.mine.phase==CC_MINE_NONE);
    Check(CcSaveRead(CC_TEST_SOURCE_DIR "/tests/fixtures/shipped/schema-58-generator-25-names-journal.ccsave",
        &restored,error,sizeof(error)));
    CC_CHECK(restored.schema_version==CC_SIM_SCHEMA_VERSION && restored.mine.phase==CC_MINE_NONE);
    CC_CHECK(restored.character_births==24);
    CC_CHECK(strcmp(restored.characters[0].name,"Rowen Venn")==0);
    CC_CHECK(strcmp(restored.characters[23].name,"Hartha Stonehewer")==0);
    restored.schema_version=58U;
    CC_CHECK(CcSimHash(&restored)==UINT64_C(1053288272468887993));
    AtBranch(&sim,false);
    Check(CcCoopApply(&sim,"visit_mine",CcMineSite(&sim)->id,0,0,error,sizeof(error)));
    Check(CcCoopApply(&sim,"mine_step",(CcId)sim.mine.revision,0,0,error,sizeof(error)));
    static CcMetagame text;
    CcMetagameInit(&text,123);text.sim=sim;
    char output[1024];
    CC_CHECK(CcMetagameExecute(&text,"mine move north",output,sizeof(output)));
    CC_CHECK(text.sim.mine.y==sim.mine.y-1);
    text.sim=haul;
    uint64_t text_inspect_hash=CcSimHash(&text.sim);
    CC_CHECK(CcMetagameExecute(&text,"mine inspect",output,sizeof(output)));
    CC_CHECK(CcSimHash(&text.sim)==text_inspect_hash &&
        strstr(output,"Hauler load")!=NULL);
    Walk(&haul,5,3); Apply(&haul,CC_COMMAND_MINE_USE,0);
    Walk(&haul,15,18);
    int32_t stowed_gems=haul.player.cargo[CC_GOOD_GEMS];
    ApplyGood(&haul,CC_COMMAND_MINE_PACK,CC_GOOD_GEMS,-1);
    CC_CHECK(haul.player.cargo[CC_GOOD_GEMS]==stowed_gems+1 &&
        CcMinePackGood(&haul,CC_GOOD_GEMS)==1);
    uint64_t before_board=CcSimHash(&haul);
    Apply(&haul,CC_COMMAND_MINE_USE,0);
    CC_CHECK(haul.mine.phase==CC_MINE_NONE && CcMinePackUsed(&haul)==0 &&
        CcMineSourceUsed(&haul)==0 && CcMineCacheUsed(&haul)==7);
    int32_t boarded_gems=haul.player.cargo[CC_GOOD_GEMS];
    uint64_t boarded_hash=CcSimHash(&haul);
    CcCommand board_again={.kind=CC_COMMAND_MINE_USE,.target_id=(CcId)haul.mine.revision};
    CC_CHECK(!CcSimApply(&haul,&board_again,error,sizeof(error)) &&
        haul.player.cargo[CC_GOOD_GEMS]==boarded_gems && CcSimHash(&haul)==boarded_hash &&
        boarded_hash!=before_board);
    Check(CcSaveWrite("mine-load-stow.ccsave",&haul,error,sizeof(error)));
    Check(CcSaveRead("mine-load-stow.ccsave",&loaded,error,sizeof(error)));
    CC_CHECK(loaded.mine.phase==CC_MINE_NONE && loaded.player.cargo[CC_GOOD_GEMS]==boarded_gems &&
        CcMineSourceUsed(&loaded)==0 && CcMineCacheUsed(&loaded)==7);
    ReturnToMineBranch(&loaded);
    ApplyGood(&loaded,CC_COMMAND_MINE_PACK,CC_GOOD_BREAD,1);
    Walk(&loaded,15,3); Apply(&loaded,CC_COMMAND_MINE_USE,0);
    Walk(&loaded,26,16);
    CC_CHECK(CcMineSourceUsed(&loaded)==0);
    Walk(&loaded,5,15);
    CC_CHECK(CcMineCacheGood(&loaded,CC_GOOD_IRON)==5 &&
        CcMineCacheGood(&loaded,CC_GOOD_GOLD)==2);
    TestMineReturnRecords();
    TestMineSurveyMigrationAndReturns();
    TestMinePartyWipeConservation();
    TestReliefCratePersistence();
    TestMineCargoReconciliation();
    TestMineTrackedRepack();
    (void)remove(path);(void)remove("mine-replay.ccsave");
    (void)remove("mine-load-replay.ccsave");
    (void)remove("mine-load-roundtrip-103.ccsave");
    (void)remove("mine-load-schema-102-fixture.ccsave");
    (void)remove("mine-load-stow.ccsave");
    (void)remove("mine-encounter-active.ccsave");
    (void)remove("mine-load-roundtrip.ccsave");
    (void)remove("mine-load-schema-102.ccsave");
    puts("Silverwick mine: road, yard, pack, level, return, persistence, replay, shared and text controls passed.");
    return 0;
}
