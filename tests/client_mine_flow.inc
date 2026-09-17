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
    sim.player.reputation = 1;
    ResetLocalState(&local);
    local.adventure_ui=true;local.journey_travel_active=true;
    local.opening_step=CC_LOCAL_OPENING_COMPLETE;
    CcJournal *journal=CcJournalStart(path,&sim,error,sizeof(error));
    if(journal==NULL) return ClientRegressionFailure(error);
#define MINE_CHECK(test,text) do { if(!(test)) { (void)CcJournalClose(&journal,&sim,error,sizeof(error)); (void)remove(path); return ClientRegressionFailure(text); } } while(0)
#define MINE_INPUT() do { HandleInput(&journal,&sim,&selected,&situation,&view,&back,&local,(RenderTexture2D){0},(Rectangle){0},.2f,path,"",message,sizeof(message),feedback,sizeof(feedback),&feedback_age); ClientInputClearPressed(); } while(0)
#define MINE_KEY(key) do { queued_key_press[key]=true; MINE_INPUT(); } while(0)
    for(int32_t facing=0;facing<4;++facing) {
        CcSim camera_sim=sim;
        camera_sim.mine.phase=CC_MINE_LEVEL;
        camera_sim.mine.x=5;camera_sim.mine.y=4;
        Camera3D camera=CcLocalMineCamera(&camera_sim,facing);
        const int32_t dx[]={0,1,0,-1},dy[]={-1,0,1,0};
        MINE_CHECK(camera.projection==CAMERA_PERSPECTIVE &&
            camera.position.x==5 && camera.position.z==4 &&
            camera.target.x-camera.position.x==(float)dx[facing] &&
            camera.target.z-camera.position.z==(float)dy[facing] &&
            camera.target.y==camera.position.y,"First-person camera must follow the saved tile and local cardinal facing.");
    }
    static LocalState repeated;
    repeated=local;
    uint64_t travel_hash=CcSimHash(&sim);
    int32_t first_steps=CcLocalWorldUpdate(&local.course,&local.agent,&sim,.025f,false,false);
    int32_t repeated_steps=CcLocalWorldUpdate(&repeated.course,&repeated.agent,&sim,.025f,false,false);
    MINE_CHECK(first_steps==1 && repeated_steps==first_steps &&
        local.course.world_simulation_accumulator>0.008 &&
        local.course.world_simulation_accumulator==repeated.course.world_simulation_accumulator &&
        local.agent.position.x==repeated.agent.position.x &&
        local.agent.position.y==repeated.agent.position.y &&
        local.agent.position.z==repeated.agent.position.z &&
        CcSimHash(&sim)==travel_hash,"Travel fixed step must advance deterministically without changing the sim.");
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
    MINE_KEY(KEY_W);
    MINE_CHECK(sim.mine.phase==CC_MINE_LEVEL && local.mine_facing==1,
        "Forward at the mouth must enter first person facing into the level.");
    uint64_t book_hash=CcSimHash(&sim);
    view=VIEW_LEDGER;back=VIEW_LOCAL;
    MINE_KEY(KEY_B);
    MINE_CHECK(view==VIEW_LOCAL && CcSimHash(&sim)==book_hash,
        "Closing the company book must restore the same mine visit.");
    MINE_KEY(KEY_F5);
    MINE_CHECK(CcSaveRead(path,&restored,error,sizeof(error)) && CcSimHash(&sim)==CcSimHash(&restored),"F5 must save the underground position.");
    MINE_KEY(KEY_W);MINE_CHECK(sim.mine.x==6,"Forward must follow east facing.");
    uint64_t turn_hash=CcSimHash(&sim);
    MINE_KEY(KEY_D);
    MINE_CHECK(local.mine_facing==2 && CcSimHash(&sim)==turn_hash,"Turning must not move, spend light or revise the sim.");
    MINE_KEY(KEY_W);MINE_CHECK(sim.mine.y==5,"Forward after turning must follow south.");
    MINE_KEY(KEY_S);MINE_CHECK(sim.mine.y==4,"Back must move opposite facing.");
    MINE_KEY(KEY_A);
    MINE_KEY(KEY_S);MINE_CHECK(sim.mine.x==5,"Back while east-facing must return west.");
    MINE_KEY(KEY_A);MINE_KEY(KEY_W);
    MINE_CHECK(sim.mine.phase==CC_MINE_YARD && sim.mine.y==4,
        "Forward into the north exit must return outside without re-entering.");
    for(int i=0;i<13;++i) MINE_KEY(KEY_S);
    MINE_KEY(KEY_S);
    MINE_CHECK(sim.mine.phase==CC_MINE_NONE && sim.carriage.mode==CC_CARRIAGE_MOVING,"Boarding must restore the road journey.");
    MINE_CHECK(local.mine_view_phase==CC_MINE_NONE && local.mine_facing==0,
        "Leaving the mine must clear the cached view so a later visit starts fresh.");
    CcSim revisit=sim; revisit.mine.phase=CC_MINE_LEVEL;
    SyncMineView(&revisit,&local);
    MINE_CHECK(local.mine_facing==1 && local.mine_view_phase==CC_MINE_LEVEL,
        "A later descent must face into the level again.");
    MINE_CHECK(CcJournalClose(&journal,&sim,error,sizeof(error)),"Mine journal must close.");
    (void)remove(path);
    puts("Mine screen: branch click, packing, first-person step and turn, seam entry, F5 and return passed.");
#undef MINE_KEY
#undef MINE_INPUT
#undef MINE_CHECK
    return 0;
}
