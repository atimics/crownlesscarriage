#include "sim/cc_mine.h"
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
int main(void)
{
    static CcSim sim,restored,changed,haul,loaded,legacy,capacity;
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
    for(int i=0;i<2;++i) AtBranch(&sim,i!=0);
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
    int32_t iron_total=CcSimTrackedGood(&haul,CC_GOOD_IRON);
    Check(CcCoopApply(&haul,"mine_take",(CcId)haul.mine.revision,CC_GOOD_IRON,3,error,sizeof(error)));
    Check(CcSimValidate(&haul,error,sizeof(error)));
    CC_CHECK(CcMineSourceGood(&haul,CC_GOOD_IRON)==5 && CcMinePackGood(&haul,CC_GOOD_IRON)==3);
    CC_CHECK(CcSimTrackedGood(&haul,CC_GOOD_IRON)==iron_total);
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
    for (int32_t slot=0;slot<CC_CUSTODY_LEGACY_CAPACITY;++slot) {
        legacy.custody.entries[slot]=(CcCustodyEntry){.id=(uint64_t)slot+1U,
            .revision=1,.owner_id=legacy.player.id,
            .holder={CC_CUSTODY_STORE,legacy.settlements[0].id},
            .kind=CC_CUSTODY_GOODS,.quantity=1,.good=CC_GOOD_BREAD,
            .condition=100,.active=true};
    }
    legacy.custody.next_id=CC_CUSTODY_LEGACY_CAPACITY+1U;
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
    (void)remove(path);(void)remove("mine-replay.ccsave");
    (void)remove("mine-load-replay.ccsave");
    (void)remove("mine-load-roundtrip-103.ccsave");
    (void)remove("mine-load-schema-102-fixture.ccsave");
    (void)remove("mine-load-stow.ccsave");
    (void)remove("mine-load-roundtrip.ccsave");
    (void)remove("mine-load-schema-102.ccsave");
    puts("Silverwick mine: road, yard, pack, level, return, persistence, replay, shared and text controls passed.");
    return 0;
}
