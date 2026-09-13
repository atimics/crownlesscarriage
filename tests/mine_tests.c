#include "sim/cc_mine.h"
#include "persistence/cc_save.h"
#include "multiplayer/cc_coop.h"
#include "metagame/cc_metagame.h"
#include "test_support.h"
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
    static CcSim sim,restored,changed;
    for(int i=0;i<2;++i) AtBranch(&sim,i!=0);
    CcCommand visit={.kind=CC_COMMAND_VISIT_MINE,.target_id=CcMineSite(&sim)->id};
    sim.player.cargo[CC_GOOD_BREAD]=3;
    Check(CcSimApply(&sim,&visit,error,sizeof(error)));
    Check(CcSimValidate(&sim,error,sizeof(error)));
    int32_t anchor=sim.journey.elapsed_subticks;
    CcSimAdvanceRuntimeTicks(&sim,1000);
    CC_CHECK(sim.journey.elapsed_subticks==anchor);
    int32_t bread=CcSimTrackedGood(&sim,CC_GOOD_BREAD);
    CcCommand pack={.kind=CC_COMMAND_MINE_PACK,.target_id=(CcId)sim.mine.revision,.good=CC_GOOD_BREAD,.amount=2};
    Check(CcSimApply(&sim,&pack,error,sizeof(error)));
    CC_CHECK(sim.player.cargo[CC_GOOD_BREAD]==1 && sim.mine.pack[CC_GOOD_BREAD]==2);
    CC_CHECK(CcSimTrackedGood(&sim,CC_GOOD_BREAD)==bread);
    uint64_t before=CcSimHash(&sim);
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
    Walk(&restored,15,17);Apply(&restored,CC_COMMAND_MINE_USE,0);
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
    (void)remove(path);(void)remove("mine-replay.ccsave");
    puts("Silverwick mine: road, yard, pack, level, return, persistence, replay, shared and text controls passed.");
    return 0;
}
