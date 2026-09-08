static int RunMineInputRegression(void)
{
    static CcSim sim, restored;
    static LocalState local;
    char message[256]="",feedback[256]="",error[192]="";
    float feedback_age=0;
    int32_t selected=0,situation=0;
    ClientView view=VIEW_LOCAL,back=VIEW_LOCAL;
    const char *path="mine-input.ccsave";
    (void)remove(path);
    CcSimInit(&sim,0x71a7e5);
    const CcRoadSite *site=CcMineSite(&sim);
    const CcRoute *road=CcSimRoute(&sim,site->route_id);
    sim.player.location_id=road->from_id;sim.carriage.location_id=road->from_id;
    CcCommand travel={.kind=CC_COMMAND_TRAVEL,.target_id=road->to_id};
    if(!CcSimApply(&sim,&travel,error,sizeof(error))) return ClientRegressionFailure(error);
    sim.pony_company.encounter=-1;
    sim.journey.elapsed_subticks=CcMineBranchSubtick(&sim);
    sim.carriage.progress_milli=(int32_t)((int64_t)sim.journey.elapsed_subticks*1000/sim.journey.total_subticks);
    sim.player.cargo[CC_GOOD_BREAD]=2;
    ResetLocalState(&local);
    local.adventure_ui=true;local.journey_travel_active=true;
    local.opening_step=CC_LOCAL_OPENING_COMPLETE;
    CcJournal *journal=CcJournalStart(path,&sim,error,sizeof(error));
    if(journal==NULL) return ClientRegressionFailure(error);
#define MINE_CHECK(test,text) do { if(!(test)) { (void)CcJournalClose(&journal,&sim,error,sizeof(error)); (void)remove(path); return ClientRegressionFailure(text); } } while(0)
#define MINE_INPUT() do { HandleInput(&journal,&sim,&selected,&situation,&view,&back,&local,(RenderTexture2D){0},(Rectangle){0},.2f,path,"",message,sizeof(message),feedback,sizeof(feedback),&feedback_age); ClientInputClearPressed(); } while(0)
#define MINE_KEY(key) do { queued_key_press[key]=true; MINE_INPUT(); } while(0)
    for(int phase=CC_MINE_YARD;phase<=CC_MINE_LEVEL;++phase) {
        CcSim camera_sim=sim;
        camera_sim.mine.phase=(CcMinePhase)phase;
        camera_sim.mine.x=15;camera_sim.mine.y=5;
        Camera3D camera=CcLocalMineCamera(&camera_sim);
        Vector3 point={15,0,5};
        Vector2 pixel=GetWorldToScreenEx(point,camera,1280,720);
        Ray ray=GetScreenToWorldRayEx(pixel,camera,1280,720);
        float distance=-ray.position.y/ray.direction.y;
        Vector3 hit={ray.position.x+ray.direction.x*distance,0,ray.position.z+ray.direction.z*distance};
        MINE_CHECK(camera.projection==CAMERA_PERSPECTIVE && fabsf(hit.x-point.x)+fabsf(hit.z-point.z)<.01f,
            "Authored camera clicks must land on the same floor tile.");
        Vector3 offset={camera.position.x-camera.target.x,camera.position.y-camera.target.y,camera.position.z-camera.target.z};
        MINE_CHECK(offset.y>0 && fabsf(offset.z)>1,"Each view must show height and wall depth.");
        if(phase==CC_MINE_LEVEL) MINE_CHECK(offset.y>fabsf(offset.z),"Dungeon camera must look mostly down.");
    }
    ContextActionSet actions=BuildContextActions(&sim,&local,view,0,0);
    int32_t visit = -1, pass = -1, clear = -1;
    for (int32_t i = 0; i < actions.count; ++i) {
        if (actions.items[i].kind == CONTEXT_ACTION_VISIT_MINE) visit = i;
        if (actions.items[i].kind == CONTEXT_ACTION_PASS_ROAD_SITE) pass = i;
        if (actions.items[i].kind == CONTEXT_ACTION_CLEAR_ROAD_SITE) clear = i;
    }
    MINE_CHECK(visit >= 0 && pass >= 0 && clear >= 0,
        "Mine branch needs entry, continue and clearing cards.");
    Rectangle button=ContextActionBounds(visit,actions.count,false);
    client_test_pointer_active=true;client_test_pointer=(Vector2){button.x+button.width/2,button.y+button.height/2};
    queued_mouse_button_press[MOUSE_BUTTON_LEFT]=true;MINE_INPUT();client_test_pointer_active=false;
    MINE_CHECK(sim.mine.phase==CC_MINE_YARD,"Clicking the branch must open the yard.");
    MINE_KEY(KEY_P);
    MINE_CHECK(CcMinePackUsed(&sim)==1,"P must pack food from the carriage.");
    for(int i=0;i<13;++i) MINE_KEY(KEY_W);
    MINE_CHECK(sim.mine.y==4,"W must walk to the entrance.");
    MINE_KEY(KEY_E);
    MINE_CHECK(sim.mine.phase==CC_MINE_LEVEL,"E at the door must switch overhead.");
    uint64_t book_hash=CcSimHash(&sim);
    view=VIEW_LEDGER;back=VIEW_LOCAL;
    MINE_KEY(KEY_B);
    MINE_CHECK(view==VIEW_LOCAL && CcSimHash(&sim)==book_hash,
        "Closing the company book must restore the same mine visit.");
    MINE_KEY(KEY_F5);
    MINE_CHECK(CcSaveRead(path,&restored,error,sizeof(error)) && CcSimHash(&sim)==CcSimHash(&restored),"F5 must save the underground position.");
    MINE_KEY(KEY_D);MINE_CHECK(sim.mine.x==6,"D must move inside the level.");
    MINE_KEY(KEY_A);MINE_KEY(KEY_E);
    MINE_CHECK(sim.mine.phase==CC_MINE_YARD,"E at the stairs must return to the yard.");
    for(int i=0;i<13;++i) MINE_KEY(KEY_S);
    MINE_KEY(KEY_E);
    MINE_CHECK(sim.mine.phase==CC_MINE_NONE && sim.carriage.mode==CC_CARRIAGE_MOVING,"Boarding must restore the road journey.");
    MINE_CHECK(CcJournalClose(&journal,&sim,error,sizeof(error)),"Mine journal must close.");
    (void)remove(path);
    puts("Mine screen: branch click, packing, movement, overhead switch, F5 and return passed.");
#undef MINE_KEY
#undef MINE_INPUT
#undef MINE_CHECK
    return 0;
}
