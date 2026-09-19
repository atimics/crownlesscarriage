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
    {
        static CcSim frame;
        frame=sim;
        frame.mine.phase=CC_MINE_LEVEL;
        frame.mine.x=25;
        frame.mine.y=16;
        frame.mine.pack[CC_GOOD_BREAD]=2;
        ClientTouchBegin();
        ClientTouchHeading("Old town","Old actions");
        ClientTouchAdd((Rectangle){0,0,1,1},"Old action",true,false);
        ClientTouchRecordText("Old town cargo");
        MineBeginTouchFrame(&frame);
        char offer[256];
        (void)snprintf(offer,sizeof(offer),
            "Offer 2 Bread (%d carried) for 1 Gold (%d in load). Pack %d/8 -> %d/8. Entry ration spent; 2 Bread must remain.",
            CcMinePackGood(&frame,CC_GOOD_BREAD),
            CcMineSourceGood(&frame,CC_GOOD_GOLD),CcMinePackUsed(&frame),
            CcMinePackUsed(&frame)-1);
        MineSetTouchDetail(&frame,offer);
        ClientTouchAdd((Rectangle){0,0,1,1},"Bargain / B",true,false);
        ClientTouchAdd((Rectangle){0,0,1,1},"Contest / C",true,false);
        ClientTouchAdd(MineNavigationControl(0),"Save F5",true,false);
        ClientTouchAdd(MineNavigationControl(1),"Menu Esc",true,false);
        ClientTouchRecordText("Pack 2/8");
        MINE_CHECK(touch_count==4 && strcmp(touch_title,"The Underroad")==0 &&
            strstr(touch_detail,"Lower passage")!=NULL &&
            strstr(touch_detail,"Offer 2 Bread (2 carried) for 1 Gold (3 in load)")!=NULL &&
            strstr(touch_detail,"2 Bread must remain")!=NULL &&
            strcmp(touch_buttons[0].label,"Bargain / B")==0 &&
            strcmp(touch_buttons[1].label,"Contest / C")==0 &&
            strcmp(touch_buttons[2].label,"Save F5")==0 &&
            strcmp(touch_buttons[3].label,"Menu Esc")==0 &&
            strstr(touch_reading,"Pack 2/8")!=NULL &&
            strstr(touch_reading,"Old town")==NULL,
            "A mine semantic frame must replace stale actions with its current offer, quantities, and controls.");
    }
    {
        /* The input path receives pixels from both mouse and browser touch
           synthesis.  Give it a fixed render size so this check exercises
           the same screen-ray pick as ordinary pointing. */
        CcSim picked=sim;
        LocalState pick_local;
        ResetLocalState(&pick_local);
        picked.mine.phase=CC_MINE_LEVEL; picked.mine.x=5; picked.mine.y=4;
        picked.mine.bar_open=false; picked.mine.surveyed=false;
        SyncMineView(&picked,&pick_local);
        MineRememberVisible(&picked,&pick_local);
        uint32_t initial_visibility_rays=pick_local.mine_visibility_rays;
        uint32_t initial_visibility_scans=pick_local.mine_visibility_scans;
        MineRememberVisible(&picked,&pick_local);
        MINE_CHECK(pick_local.mine_visibility_rays==initial_visibility_rays &&
            pick_local.mine_visibility_scans==initial_visibility_scans &&
            initial_visibility_rays<=CC_MINE_WIDTH*CC_MINE_HEIGHT,
            "Mine visibility must reuse its bounded scan while position and geometry stay fixed.");
        picked.mine.bar_open=true;
        MineRememberVisible(&picked,&pick_local);
        MINE_CHECK(pick_local.mine_visibility_scans==initial_visibility_scans+1 &&
            pick_local.mine_visibility_rays-initial_visibility_rays<=CC_MINE_WIDTH*CC_MINE_HEIGHT,
            "Changing the bar must refresh one bounded visibility scan.");
        picked.mine.bar_open=false;
        MineRememberVisible(&picked,&pick_local);
        MINE_CHECK(pick_local.mine_visibility_scans==initial_visibility_scans+2,
            "Restoring the bar must refresh the visibility geometry once.");
        RenderTexture2D surface={.texture={.width=630,.height=320}};
        int32_t tx=-1,ty=-1;
        MineIntentTarget target=MINE_INTENT_MOVE;
        BoundingBox slab_box=MineClosedBarBounds();
        float slab_distance=-1.0f;
        MINE_CHECK(MineRayBoxDistance((Ray){.position={14,0.7f,10},.direction={1,0,0}},
                                      slab_box,&slab_distance) &&
            isfinite(slab_distance) && fabsf(slab_distance-.5f)<.001f,
            "An axis-parallel ray must produce a finite bar-box distance.");
        MINE_CHECK(MineRayBoxDistance((Ray){.position={14.5f,.7f,10},.direction={0,1,0}},
                                      slab_box,&slab_distance) && slab_distance==0.0f,
            "A ray on a bar-box boundary must begin at a finite zero distance.");
        MINE_CHECK(!MineRayBoxDistance((Ray){.position={14,0.7f,10},.direction={0,1,0}},
                                       slab_box,&slab_distance),
            "An axis-parallel ray outside the bar box must miss cleanly.");
        MINE_CHECK(!MineRayBoxDistance((Ray){.position={NAN,0.7f,10},.direction={1,0,0}},
                                       slab_box,&slab_distance),
            "A non-finite ray must miss without creating a non-finite distance.");
        MINE_CHECK(!MineRayBoxDistance((Ray){.position={15,0.7f,10},.direction={0,0,0}},
                                       slab_box,&slab_distance),
            "A zero-length ray has no direction and must miss cleanly.");
        Camera3D camera=CcLocalMineCamera(&picked,pick_local.mine_facing);
        Vector2 clear_floor=GetWorldToScreenEx((Vector3){9,0,4},camera,630,320);
        MINE_CHECK(clear_floor.x>=0&&clear_floor.x<630&&clear_floor.y>=0&&clear_floor.y<320,
            "The visible-floor pointer fixture must project into the render target.");
        MINE_CHECK(MinePick(&picked,&pick_local,surface,clear_floor,&tx,&ty,&target) &&
            target==MINE_INTENT_MOVE && tx==9 && ty==4,
            "A pointer click on visible floor must select that floor.");
        memset(pick_local.mine_known,0,sizeof(pick_local.mine_known));
        pick_local.mine_known[MineCell(5,4)]=1;
        pick_local.mine_known[MineCell(6,4)]=1;
        MINE_CHECK(MinePathStep(&picked,&pick_local,7,4)<0,
            "Pointing must not route through an unseen corridor cell.");
        pick_local.mine_known[MineCell(7,4)]=1;
        MINE_CHECK(MinePathStep(&picked,&pick_local,7,4)==1,
            "A remembered corridor must keep multi-step pointing useful.");
        MINE_CHECK(!MineLineVisibleExcept(&picked,(Vector3){5,.55f,4},
                                          (Vector3){15,0,11},-1,-1),
            "A wall must hide floor beyond it from the pointer route.");
        memset(pick_local.mine_known,1,sizeof(pick_local.mine_known));
        picked.mine.x=15; picked.mine.y=12;
        int32_t goal_x=-1,goal_y=-1,goal_step=-1,goal_distance=-1;
        MINE_CHECK(MineIntentGoal(&picked,&pick_local,MINE_INTENT_BAR,&goal_x,&goal_y) &&
            MinePath(&picked,&pick_local,goal_x,goal_y,&goal_step,&goal_distance) &&
            goal_x==15 && goal_y==11 && goal_distance==1,
            "A named bar approach must choose the nearest legal side from below.");
        picked.mine.x=15; picked.mine.y=7;
        MINE_CHECK(MineIntentGoal(&picked,&pick_local,MINE_INTENT_BAR,&goal_x,&goal_y) &&
            MinePath(&picked,&pick_local,goal_x,goal_y,&goal_step,&goal_distance) &&
            goal_x==15 && goal_y==9 && goal_distance==2,
            "A named bar approach must choose the nearest legal side from above.");
        picked.mine.x=15; picked.mine.y=9; pick_local.mine_facing=2;
        camera=CcLocalMineCamera(&picked,pick_local.mine_facing);
        Vector2 bar=GetWorldToScreenEx((Vector3){15,.85f,10},camera,630,320);
        int label_x=-1,label_y=-1;
        BoundingBox bar_bounds={0};
        Ray bar_ray={.position={15,1.15f,9},.direction={0,-.3f,1}};
        MINE_CHECK(MineClosedBarDistance(bar_ray,&slab_distance) &&
            fabsf(MineOccluderDistance(&picked,bar_ray,100.0f)-slab_distance)<.001f,
            "The visible bar beams must use the same occlusion distance as picking.");
        MINE_CHECK(MineLabelPosition(surface,&picked,&pick_local,(Vector3){15,1,10},15,10,
                                      &label_x,&label_y) &&
            label_x>=80 && label_x<=550 && label_y>=12 && label_y<=300,
            "The closed bar must keep a visible label inside the mine view.");
        picked.mine.x=5; picked.mine.y=4; pick_local.mine_facing=1;
        MINE_CHECK(!MineLabelPosition(surface,&picked,&pick_local,(Vector3){15,0,11},15,11,
                                       &label_x,&label_y),
            "A wall before a named target must hide its label.");
        picked.mine.x=15; picked.mine.y=9; pick_local.mine_facing=2;
        MINE_CHECK(!CcMineWalkable(&picked,CC_MINE_LEVEL,15,10) &&
            MineTargetBounds(&picked,MINE_INTENT_BAR,&bar_bounds,&tx,&ty) &&
            tx==15 && ty==10,
            "The visible closed bar must retain its blocking tile and matching pick bounds.");
        MINE_CHECK(MinePick(&picked,&pick_local,surface,bar,&tx,&ty,&target) &&
            target==MINE_INTENT_BAR,
            "A visible bar click must name the bar rather than its floor.");
        MINE_CHECK(MineIntentCanUse(&picked,MINE_INTENT_BAR),
            "The named bar stays usable only while the company is in reach.");
        picked.mine.bar_open=true;
        MINE_CHECK(!MineIntentCanUse(&picked,MINE_INTENT_BAR),
            "A changed target must fail the queued named use.");
        picked.mine.bar_open=false; picked.mine.x=26; picked.mine.y=6;
        pick_local.mine_facing=0;
        camera=CcLocalMineCamera(&picked,pick_local.mine_facing);
        Vector2 survey=GetWorldToScreenEx((Vector3){26,.37f,4},camera,630,320);
        MINE_CHECK(survey.x>=0&&survey.x<630&&survey.y>=0&&survey.y<320,
            "The survey pointer fixture must project into the render target.");
        MINE_CHECK(MinePick(&picked,&pick_local,surface,survey,&tx,&ty,&target) &&
            target==MINE_INTENT_SURVEY,
            "A visible survey click must name the survey rather than nearby floor.");
        picked.mine.x=25; picked.mine.y=16;
        picked.mine.source_x=26; picked.mine.source_y=16;
        picked.mine.source_owner_id=picked.goblins.id;
        picked.mine.encounter_outcome=CC_MINE_ENCOUNTER_OPEN;
        pick_local.mine_facing=1;
        MineRememberVisible(&picked,&pick_local);
        camera=CcLocalMineCamera(&picked,pick_local.mine_facing);
        Vector2 hauler_position=CcLocalMineHaulerPosition(&picked,0);
        picked.mine.x=23; picked.mine.y=16;
        Vector2 moved_view_position=CcLocalMineHaulerPosition(&picked,0);
        picked.mine.x=25;
        MINE_CHECK(hauler_position.x==moved_view_position.x &&
            hauler_position.y==moved_view_position.y,
            "The saved hauler formation must stay fixed when the player moves.");
        Vector2 hauler=GetWorldToScreenEx(
            (Vector3){hauler_position.x,1.55f,hauler_position.y},camera,630,320);
        Vector2 hauler_head=GetWorldToScreenEx(
            (Vector3){hauler_position.x,2.05f,hauler_position.y},camera,630,320);
        Vector2 hauler_feet=GetWorldToScreenEx(
            (Vector3){hauler_position.x,.05f,hauler_position.y},camera,630,320);
        MINE_CHECK(hauler.x>=0&&hauler.x<630&&hauler.y>=0&&hauler.y<320,
            "The visible hauler body must project into the render target.");
        MINE_CHECK(hauler_head.y>=0&&hauler_head.y<320&&
            hauler_feet.y>=0&&hauler_feet.y<320,
            "The offer camera must keep the hauler's head and feet in frame.");
        MINE_CHECK(MinePick(&picked,&pick_local,surface,hauler,&tx,&ty,&target) &&
            target==MINE_INTENT_HAULERS && tx==26 && ty==16,
            "An upward hauler-body ray must name the saved hauler group.");
        Rectangle mine_view=MineViewport(&picked,&pick_local);
        if(mine_view.width<=0||mine_view.height<=0)
            mine_view=(Rectangle){0,0,630,320};
        client_test_pointer_active=true;
        client_test_pointer=(Vector2){mine_view.x+hauler.x*mine_view.width/630.0f,
            mine_view.y+hauler.y*mine_view.height/320.0f};
        queued_mouse_button_press[MOUSE_BUTTON_LEFT]=true;
        HandleMineInput(journal,&picked,&pick_local,surface,.2f,message,sizeof(message));
        client_test_pointer_active=false;
        ClientInputClearPressed();
        MINE_CHECK(strstr(message,"Choose the visible hauler action")!=NULL &&
            pick_local.mine_target_x<0,
            "A visible hauler click must select the group at its reachable approach.");
        picked.mine.phase=CC_MINE_YARD;
        picked.mine.x=CC_MINE_YARD_CARRIAGE_X;
        picked.mine.y=CC_MINE_YARD_CARRIAGE_Y-1;
        BoundingBox carriage_bounds={0};
        MINE_CHECK(MineTargetBounds(&picked,MINE_INTENT_CARRIAGE,
                &carriage_bounds,&tx,&ty) &&
            tx==CC_MINE_YARD_CARRIAGE_X &&
            ty==CC_MINE_YARD_CARRIAGE_Y &&
            fabsf((carriage_bounds.min.x+carriage_bounds.max.x)*.5f-
                (float)CC_MINE_YARD_CARRIAGE_X)<.001f &&
            fabsf((carriage_bounds.min.z+carriage_bounds.max.z)*.5f-
                (float)CC_MINE_YARD_CARRIAGE_Y)<.001f &&
            MineAtCarriage(&picked) && MineIntentCanUse(&picked,MINE_INTENT_CARRIAGE),
            "The drawn carriage, pick volume and legal approach must share one yard anchor.");
        Camera3D yard_camera=CcLocalMineCamera(&picked,0);
        float yard_dx=yard_camera.position.x-yard_camera.target.x;
        float yard_dy=yard_camera.position.y-yard_camera.target.y;
        float yard_dz=yard_camera.position.z-yard_camera.target.z;
        float yard_span=sqrtf(yard_dx*yard_dx+yard_dy*yard_dy+yard_dz*yard_dz);
        MINE_CHECK(yard_camera.projection==CAMERA_PERSPECTIVE &&
            yard_span>=12.0f && yard_span<=20.0f && yard_camera.fovy<=48.0f,
            "The yard camera must keep people, cargo and interaction targets at a readable scale.");
        pick_local.mine_target_x=26; pick_local.mine_target_y=5;
        pick_local.mine_intent_target=MINE_INTENT_SURVEY;
        pick_local.mine_intent_revision=picked.mine.revision;
        SyncMineView(&picked,&pick_local);
        MINE_CHECK(pick_local.mine_target_x<0 && pick_local.mine_intent_revision<0,
            "Phase changes and reload-style local resets must cancel queued input.");
    }
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
    /* A roadside place is a turn-off, not a bed. Camping there used to spend
       the stop and take the mine's branch with it, so the card is gone and
       every site keeps a way to drive on instead. Checked at an ordinary site,
       not the mine: the mine branch returns its own cards before camping was
       ever reached, so asserting there proves nothing. */
    {
        CcSim ordinary = sim;
        const CcRoadSite *plain = NULL;
        for (int32_t i = 0; i < ordinary.road_site_count; ++i) {
            const CcRoadSite *candidate = &ordinary.road_sites[i];
            if (candidate->route_id == ordinary.journey.route_id &&
                candidate != CcMineSite(&ordinary)) { plain = candidate; break; }
        }
        MINE_CHECK(plain != NULL, "The mine route needs an ordinary site to check.");
        const CcRoute *route = CcSimRoute(&ordinary, plain->route_id);
        int32_t progress = ordinary.journey.origin_id == route->from_id ?
            plain->progress_milli : 1000 - plain->progress_milli;
        ordinary.carriage.progress_milli = progress;
        ordinary.journey.elapsed_subticks = (int32_t)(
            (int64_t)ordinary.journey.total_subticks * progress / 1000);
        MINE_CHECK(CcSimJourneyRoadSiteStop(&ordinary) == plain,
            "The ordinary site must be the stop under test.");
        ContextActionSet plain_actions =
            BuildContextActions(&ordinary, &local, view, 0, 0);
        bool camps = false, drives_on = false;
        for (int32_t i = 0; i < plain_actions.count; ++i) {
            if (plain_actions.items[i].kind == CONTEXT_ACTION_CAMP_ROAD_SITE) camps = true;
            if (plain_actions.items[i].kind == CONTEXT_ACTION_PASS_ROAD_SITE) drives_on = true;
        }
        MINE_CHECK(!camps, "A roadside stop must not offer camping.");
        MINE_CHECK(drives_on, "Every roadside stop must offer the road on.");
    }
    {
        static CcSim travel_state;
        static LocalState travel_local;
        travel_state=sim;
        travel_local=local;
        travel_state.journey.active=true;
        travel_state.journey.phase=CC_JOURNEY_PHASE_TRAVELLING;
        travel_local.journey_travel_active=true;
        travel_local.travel_hold_armed=false;
        MINE_CHECK(strcmp(TravelActionDetail(&travel_state,&travel_local),
            "Hold to travel faster")==0,
            "An active road must describe the current moving state.");
        travel_state.journey.phase=CC_JOURNEY_PHASE_BLOCKED;
        MINE_CHECK(strcmp(TravelActionDetail(&travel_state,&travel_local),
            "Choice ahead")==0,
            "A blocked road must describe its pending choice.");
        travel_state.journey.phase=CC_JOURNEY_PHASE_RESTING;
        MINE_CHECK(strcmp(TravelActionDetail(&travel_state,&travel_local),
            "Resting until morning")==0,
            "A resting road must describe its saved phase.");
    }
    {
        static CcSim readable_manifest;
        readable_manifest=sim;
        (void)memset(readable_manifest.player.cargo,0,
            sizeof(readable_manifest.player.cargo));
        readable_manifest.player.cargo[CC_GOOD_FOOD]=1;
        readable_manifest.player.cargo[CC_GOOD_GOLD]=1;
        readable_manifest.player.cargo_capacity=12;
        char summary[512];
        CarriageReadingSummary(&readable_manifest,summary,sizeof(summary));
        MINE_CHECK(strstr(summary,"Bread 1")!=NULL&&
            strstr(summary,"Raw Gold 1")!=NULL&&
            strstr(summary,"Load 2 of 12, free 10")!=NULL&&
            strstr(summary,"Team ")!=NULL&&
            strstr(summary,"Carriage condition ")!=NULL,
            "The semantic carriage manifest must include quantities, free space, team, and condition.");
    }
    Rectangle button=ContextActionBounds(visit,actions.count,false);
    client_test_pointer_active=true;client_test_pointer=(Vector2){button.x+button.width/2,button.y+button.height/2};
    queued_mouse_button_press[MOUSE_BUTTON_LEFT]=true;MINE_INPUT();client_test_pointer_active=false;
    MINE_CHECK(sim.mine.phase==CC_MINE_YARD,"Clicking the branch must open the yard.");
    {
        CcSim inventory=sim;
        for(int32_t good=0;good<12;++good) inventory.player.cargo[good]=good+1;
        for(int32_t good=0;good<8;++good) inventory.mine.pack[good]=1;
        int32_t inventory_rows=MineGoodsRows(inventory.mine.pack);
        if(MineGoodsRows(inventory.player.cargo)>inventory_rows)
            inventory_rows=MineGoodsRows(inventory.player.cargo);
        Rectangle populated=MineViewportForContent(1040,620,false,inventory_rows);
        Rectangle one_row=MineViewportForContent(1040,620,false,1);
        MINE_CHECK((float)(104+inventory_rows*11)<=populated.y,
            "Several goods must fit above the mine viewport.");
        MINE_CHECK(one_row.y<populated.y&&one_row.height>populated.height,
            "A short manifest must return useful height to the mine scene.");
        MINE_CHECK(MineViewportForSize(844,390).height>0,
            "The mine view must retain positive height below its inventory.");
        Rectangle collapsed=MineViewportForContent(844,390,false,12);
        Rectangle expanded=MineViewportForContent(844,390,true,12);
        MINE_CHECK(collapsed.height>=120.0f&&expanded.height==collapsed.height&&
            expanded.width+300.0f<=collapsed.width,
            "The short inventory keeps a clear central scene beside both goods panes.");
        MINE_CHECK(!CheckCollisionPointRec((Vector2){80.0f,150.0f},expanded)&&
            CheckCollisionPointRec((Vector2){422.0f,150.0f},expanded),
            "Expanded goods panes must stay outside the live central pick bounds.");
        for(int32_t good=0;good<CC_GOOD_COUNT;++good) {
            char line[64];
            (void)snprintf(line,sizeof(line),"%s %d",CcGoodName((CcGood)good),999);
            MINE_CHECK(CcOverlayMeasureText(line,10)<=390/2-22,
                "Mine goods must fit inside a narrow portrait inventory column.");
        }
    }
    Rectangle mine_control=MineSelectionControl(1);
    client_test_pointer_active=true;client_test_pointer=(Vector2){mine_control.x+mine_control.width/2,mine_control.y+mine_control.height/2};
    queued_mouse_button_press[MOUSE_BUTTON_LEFT]=true;MINE_INPUT();client_test_pointer_active=false;
    MINE_CHECK(local.mine_good==CC_GOOD_IRON,"The next-good button must select the next good.");
    mine_control=MineSelectionControl(0);
    client_test_pointer_active=true;client_test_pointer=(Vector2){mine_control.x+mine_control.width/2,mine_control.y+mine_control.height/2};
    queued_mouse_button_press[MOUSE_BUTTON_LEFT]=true;MINE_INPUT();client_test_pointer_active=false;
    MINE_CHECK(local.mine_good==CC_GOOD_BREAD,"The previous-good button must restore the selected good.");
    mine_control=MineSelectionControl(3);
    client_test_pointer_active=true;client_test_pointer=(Vector2){mine_control.x+mine_control.width/2,mine_control.y+mine_control.height/2};
    queued_mouse_button_press[MOUSE_BUTTON_LEFT]=true;MINE_INPUT();client_test_pointer_active=false;
    MINE_CHECK(local.mine_quantity==2,"The more button must select two goods.");
    mine_control=MineControl(1);
    client_test_pointer_active=true;client_test_pointer=(Vector2){mine_control.x+mine_control.width/2,mine_control.y+mine_control.height/2};
    queued_mouse_button_press[MOUSE_BUTTON_LEFT]=true;MINE_INPUT();client_test_pointer_active=false;
    MINE_CHECK(CcMinePackUsed(&sim)==2 && sim.mine.pack[CC_GOOD_BREAD]==2,
        "P must pack the selected amount from the carriage.");
    mine_control=MineSelectionControl(2);
    client_test_pointer_active=true;client_test_pointer=(Vector2){mine_control.x+mine_control.width/2,mine_control.y+mine_control.height/2};
    queued_mouse_button_press[MOUSE_BUTTON_LEFT]=true;MINE_INPUT();client_test_pointer_active=false;
    mine_control=MineControl(2);
    client_test_pointer_active=true;client_test_pointer=(Vector2){mine_control.x+mine_control.width/2,mine_control.y+mine_control.height/2};
    queued_mouse_button_press[MOUSE_BUTTON_LEFT]=true;MINE_INPUT();client_test_pointer_active=false;
    MINE_CHECK(CcMinePackUsed(&sim)==1 && sim.player.cargo[CC_GOOD_BREAD]==1,
        "U must unload the selected amount to the nearby carriage.");
    mine_control=MineControl(1);
    client_test_pointer_active=true;client_test_pointer=(Vector2){mine_control.x+mine_control.width/2,mine_control.y+mine_control.height/2};
    queued_mouse_button_press[MOUSE_BUTTON_LEFT]=true;MINE_INPUT();client_test_pointer_active=false;
    MINE_CHECK(CcMinePackUsed(&sim)==2 && strstr(message,"carriage to pack")!=NULL,
        "Packing must leave a clear transfer receipt.");
    for(int i=0;i<13;++i) MINE_KEY(KEY_W);
    MINE_CHECK(sim.mine.y==4,"W must walk to the entrance.");
    {
        uint64_t remote_hash=CcSimHash(&sim);
        MINE_KEY(KEY_P);
        MINE_CHECK(CcSimHash(&sim)==remote_hash && strstr(message,"Pack supplies beside the carriage")!=NULL &&
            strstr(message,"moved") == NULL,"A failed remote transfer must preserve its error instead of a receipt.");
    }
    MINE_KEY(KEY_W);
    MINE_CHECK(sim.mine.phase==CC_MINE_LEVEL && local.mine_facing==1,
        "Forward at the mouth must enter first person facing into the level.");
    MINE_CHECK(strstr(message,"1 Bread spent")!=NULL &&
        strstr(message,"Light 18")!=NULL,
        "Mine entry must state its Bread cost and starting light.");
    {
        CcSim input=sim;
        LocalState input_local;
        const char *input_path="mine-pointer.ccsave";
        (void)remove(input_path);
        ResetLocalState(&input_local);
        SyncMineView(&input,&input_local);
        CcJournal *input_journal=CcJournalStart(input_path,&input,error,sizeof(error));
        MINE_CHECK(input_journal!=NULL,"Pointer input needs a separate mine journal.");
        RenderTexture2D surface={.texture={.width=630,.height=960}};
#define INPUT_STEP(direction) do { CcCommand step={.kind=CC_COMMAND_MINE_STEP,.target_id=(CcId)input.mine.revision,.amount=(direction)}; MINE_CHECK(ApplyCommand(input_journal,&input,step,message,sizeof(message)),"Pointer fixture needs a legal mine step."); } while(0)
#define INPUT_POINTER(point) do { Camera3D pointer_camera=CcLocalMineCamera(&input,input_local.mine_facing); Vector2 raw_pointer=GetWorldToScreenEx((point),pointer_camera,630,960); Rectangle pointer_view=MineViewport(&input,&input_local); if(pointer_view.width<=0||pointer_view.height<=0) pointer_view=(Rectangle){0,0,630,960}; MINE_CHECK(raw_pointer.x>=0&&raw_pointer.x<630&&raw_pointer.y>=0&&raw_pointer.y<960,"Pointer fixture must stay inside the rendered mine view."); Vector2 pointer={pointer_view.x+raw_pointer.x*pointer_view.width/630.0f,pointer_view.y+raw_pointer.y*pointer_view.height/960.0f}; client_test_pointer_active=true;client_test_pointer=pointer;queued_mouse_button_press[MOUSE_BUTTON_LEFT]=true;HandleMineInput(input_journal,&input,&input_local,surface,.2f,message,sizeof(message));client_test_pointer_active=false;ClientInputClearPressed(); } while(0)
        for(int i=0;i<10;++i) INPUT_STEP(1);
        for(int i=0;i<2;++i) INPUT_STEP(2);
        input_local.mine_facing=2; MineRememberVisible(&input,&input_local);
        int32_t floor_revision=input.mine.revision;
        INPUT_POINTER(((Vector3){15,0,9}));
        MINE_CHECK(!input.mine.bar_open && input.mine.revision==floor_revision+1,
            "A floor click beside the closed bar must only walk.");
        for(int i=0;i<2;++i) INPUT_STEP(2);
        input_local.mine_facing=2; MineRememberVisible(&input,&input_local);
        int32_t bar_revision=input.mine.revision;
        {
            Camera3D bar_camera=CcLocalMineCamera(&input,input_local.mine_facing);
            Vector2 bar_pointer=GetWorldToScreenEx((Vector3){15,.85f,10},bar_camera,630,960);
            int32_t bar_x=-1,bar_y=-1; MineIntentTarget bar_target=MINE_INTENT_MOVE;
            MINE_CHECK(MinePick(&input,&input_local,surface,bar_pointer,&bar_x,&bar_y,&bar_target) &&
                bar_target==MINE_INTENT_BAR,"The bar pointer fixture must pick the named bar.");
        }
        INPUT_POINTER(((Vector3){15,.85f,10}));
        MINE_CHECK(input.mine.bar_open && input.mine.revision==bar_revision+1,
            "An explicit bar click must use that bar once.");
        HandleMineInput(input_journal,&input,&input_local,surface,.2f,message,sizeof(message));
        MINE_CHECK(input.mine.revision==bar_revision+1,
            "A completed named use must leave no delayed second action.");
        for(int i=0;i<5;++i) INPUT_STEP(0);
        for(int i=0;i<11;++i) INPUT_STEP(1);
        for(int i=0;i<2;++i) INPUT_STEP(0);
        input_local.mine_facing=2; MineRememberVisible(&input,&input_local);
        int32_t survey_floor_revision=input.mine.revision;
        INPUT_POINTER(((Vector3){25,0,5}));
        MINE_CHECK(!input.mine.surveyed && input.mine.revision==survey_floor_revision+1,
            "A floor click beside the unread survey must only walk.");
        for(int i=0;i<3;++i) INPUT_STEP(2);
        input_local.mine_facing=0; MineRememberVisible(&input,&input_local);
        int32_t survey_revision=input.mine.revision;
        {
            Camera3D survey_camera=CcLocalMineCamera(&input,input_local.mine_facing);
            Vector2 survey_pointer=GetWorldToScreenEx((Vector3){26,.37f,4},survey_camera,630,960);
            int32_t survey_x=-1,survey_y=-1; MineIntentTarget survey_target=MINE_INTENT_MOVE;
            MINE_CHECK(MinePick(&input,&input_local,surface,survey_pointer,&survey_x,&survey_y,&survey_target) &&
                survey_target==MINE_INTENT_SURVEY,"The survey pointer fixture must pick the named survey.");
        }
        INPUT_POINTER(((Vector3){26,.37f,4}));
        MINE_CHECK(!input.mine.surveyed && input.mine.revision==survey_revision+1,
            "A survey pointer must approach its named target before it uses it.");
        MINE_CHECK(input.mine.x==26&&input.mine.y==5&&
            input_local.mine_intent_target==MINE_INTENT_SURVEY&&
            input_local.mine_intent_revision==input.mine.revision,
            "The survey pointer must retain its exact target while approaching.");
        MINE_CHECK(MineIntentCanUse(&input,MINE_INTENT_SURVEY),
            "The reached survey target must still be in legal use range.");
        HandleMineInput(input_journal,&input,&input_local,surface,.2f,message,sizeof(message));
        MINE_CHECK(input.mine.surveyed,"An explicit survey click must use that survey once.");
        MINE_CHECK(input.mine.revision==survey_revision+2,
            "The survey action must advance its revision once after approach.");
        input_local.mine_facing=3; MineRememberVisible(&input,&input_local);
        INPUT_POINTER(((Vector3){23,0,4}));
        MINE_CHECK(input_local.mine_target_x>=0,"A floor pointer must queue its remaining walk.");
        queued_key_press[KEY_A]=true;
        HandleMineInput(input_journal,&input,&input_local,surface,.2f,message,sizeof(message));
        ClientInputClearPressed();
        MINE_CHECK(input_local.mine_target_x<0,"Manual movement must cancel a queued pointer walk.");
        MineRememberVisible(&input,&input_local);
        INPUT_POINTER(((Vector3){23,0,4}));
        MINE_CHECK(input_local.mine_target_x>=0,"A fresh pointer walk must queue before revision validation.");
        int32_t stale_x=input.mine.x,stale_y=input.mine.y;
        input.mine.revision+=1;
        HandleMineInput(input_journal,&input,&input_local,surface,.2f,message,sizeof(message));
        MINE_CHECK(input_local.mine_target_x<0&&input.mine.x==stale_x&&input.mine.y==stale_y,
            "A changed revision must cancel queued pointer input before it moves.");
        input.mine.x=25; input.mine.y=16;
        input.mine.pack[CC_GOOD_BREAD]=2;
        input.mine.encounter_outcome=CC_MINE_ENCOUNTER_OPEN;
        input.mine.contest_active=false;
        input.mine.source_released=false;
        MineRememberVisible(&input,&input_local);
        uint64_t held_contact=CcSimHash(&input);
        mine_control=MineControl(2);
        client_test_pointer_active=true;
        client_test_pointer=(Vector2){mine_control.x+mine_control.width/2,
                                      mine_control.y+mine_control.height/2};
        queued_mouse_button_press[MOUSE_BUTTON_LEFT]=true;
        HandleMineInput(input_journal,&input,&input_local,surface,.2f,message,sizeof(message));
        client_test_pointer_active=false;
        ClientInputClearPressed();
        MINE_CHECK(CcSimHash(&input)==held_contact,
            "Break contact must stay disabled before a contest starts.");
        queued_key_press[KEY_C]=true;
        HandleMineInput(input_journal,&input,&input_local,surface,.2f,message,sizeof(message));
        ClientInputClearPressed();
        MINE_CHECK(input.mine.contest_active&&input_local.mine_combat_active&&
            input_local.course.mine_encounter&&
            input_local.course.raider_company_id==input.goblins.id&&
            input_local.agent.scene==CC_LOCAL_SCENE_MINE&&
            fabsf(input_local.agent.position.x-25.0f)<.001f&&
            fabsf(input_local.agent.position.z-16.0f)<.001f,
            "C must stage the saved hauler group at the saved mine approach.");
        Camera3D fight_camera=CcLocalMineCombatCamera(
            &input_local.agent,&input_local.course,input_local.mine_facing);
        for(int32_t raider=0;raider<CC_LOCAL_RAIDER_COUNT;++raider) {
            const CcLocalAgent *body=&input_local.course.raiders[raider];
            Vector2 head=GetWorldToScreenEx((Vector3){body->position.x,
                body->position.y+2.05f,body->position.z},fight_camera,630,320);
            Vector2 feet=GetWorldToScreenEx((Vector3){body->position.x,
                body->position.y+.05f,body->position.z},fight_camera,630,320);
            MINE_CHECK(head.y>=0&&head.y<320&&feet.y>=0&&feet.y<320,
                "The contest camera must keep each hauler's head and feet in frame.");
        }
        queued_key_press[KEY_SPACE]=true;
        HandleMineInput(input_journal,&input,&input_local,surface,.2f,message,sizeof(message));
        ClientInputClearPressed();
        MINE_CHECK(input_local.agent.combat.target_index>=0,
            "Space must use the selected course combat target.");
        input_local.agent.combat.health=73.0f;
        mine_control=MineControl(2);
        client_test_pointer_active=true;
        client_test_pointer=(Vector2){mine_control.x+mine_control.width/2,
                                      mine_control.y+mine_control.height/2};
        queued_mouse_button_press[MOUSE_BUTTON_LEFT]=true;
        HandleMineInput(input_journal,&input,&input_local,surface,.2f,message,sizeof(message));
        client_test_pointer_active=false;
        ClientInputClearPressed();
        MINE_CHECK(!input.mine.contest_active&&input.mine.player_injury==27&&
            input.mine.x==25&&input.mine.y==16&&
            fabsf(input_local.agent.position.x-25.0f)<.001f&&
            fabsf(input_local.agent.position.z-16.0f)<.001f,
            "The pointer break action must record injury and keep the mine tile aligned.");
        queued_key_press[KEY_C]=true;
        HandleMineInput(input_journal,&input,&input_local,surface,0.0f,message,sizeof(message));
        ClientInputClearPressed();
        MINE_CHECK(input.mine.contest_active&&
            fabsf(input_local.agent.combat.health-73.0f)<.001f,
            "Reengagement must restore the injury recorded at break contact.");
        mine_control=MineControl(2);
        client_test_pointer_active=true;
        client_test_pointer=(Vector2){mine_control.x+mine_control.width/2,
                                      mine_control.y+mine_control.height/2};
        queued_mouse_button_press[MOUSE_BUTTON_LEFT]=true;
        HandleMineInput(input_journal,&input,&input_local,surface,0.0f,message,sizeof(message));
        client_test_pointer_active=false;
        ClientInputClearPressed();
        MINE_CHECK(!input.mine.contest_active&&input.mine.player_injury==27,
            "A second physical break must retain the recovered combat state.");
        mine_control=MineControl(0);
        client_test_pointer_active=true;
        client_test_pointer=(Vector2){mine_control.x+mine_control.width/2,
                                      mine_control.y+mine_control.height/2};
        queued_mouse_button_press[MOUSE_BUTTON_LEFT]=true;
        HandleMineInput(input_journal,&input,&input_local,surface,.2f,message,sizeof(message));
        client_test_pointer_active=false;
        ClientInputClearPressed();
        BoundingBox ended_bounds; int32_t ended_x=0,ended_y=0;
        MINE_CHECK(input.mine.encounter_outcome==CC_MINE_ENCOUNTER_BARGAINED&&
            CcMinePackGood(&input,CC_GOOD_BREAD)==0&&
            CcMinePackGood(&input,CC_GOOD_GOLD)==1&&
            !MineTargetBounds(&input,MINE_INTENT_HAULERS,&ended_bounds,
                              &ended_x,&ended_y),
            "The bargain pointer must settle the offer and remove the hungry-hauler target.");
        input.mine.encounter_outcome=CC_MINE_ENCOUNTER_CONTESTED;
        input.mine.source_released=true;
        input_local.mine_good=CC_GOOD_GOLD;
        input_local.mine_quantity=1;
        mine_control=MineControl(0);
        client_test_pointer_active=true;
        client_test_pointer=(Vector2){mine_control.x+mine_control.width/2,
                                      mine_control.y+mine_control.height/2};
        queued_mouse_button_press[MOUSE_BUTTON_LEFT]=true;
        HandleMineInput(input_journal,&input,&input_local,surface,0.0f,message,sizeof(message));
        client_test_pointer_active=false;
        ClientInputClearPressed();
        MINE_CHECK(strstr(message,"Hauler load")!=NULL,
            "The settled contest must expose a visible inspect action.");
        int32_t gold_before_take=CcMinePackGood(&input,CC_GOOD_GOLD);
        mine_control=MineControl(1);
        client_test_pointer_active=true;
        client_test_pointer=(Vector2){mine_control.x+mine_control.width/2,
                                      mine_control.y+mine_control.height/2};
        queued_mouse_button_press[MOUSE_BUTTON_LEFT]=true;
        HandleMineInput(input_journal,&input,&input_local,surface,0.0f,message,sizeof(message));
        client_test_pointer_active=false;
        ClientInputClearPressed();
        MINE_CHECK(CcMinePackGood(&input,CC_GOOD_GOLD)==gold_before_take+1&&
            strstr(message,"Took 1 Raw Gold")!=NULL,
            "The visible take button must transfer the selected finite load.");
        input.mine.x=input.mine.cache_x;
        input.mine.y=input.mine.cache_y;
        int32_t cache_before=CcMineCacheGood(&input,CC_GOOD_GOLD);
        mine_control=MineControl(0);
        client_test_pointer_active=true;
        client_test_pointer=(Vector2){mine_control.x+mine_control.width/2,
                                      mine_control.y+mine_control.height/2};
        queued_mouse_button_press[MOUSE_BUTTON_LEFT]=true;
        HandleMineInput(input_journal,&input,&input_local,surface,0.0f,message,sizeof(message));
        client_test_pointer_active=false;
        ClientInputClearPressed();
        MINE_CHECK(CcMineCacheGood(&input,CC_GOOD_GOLD)==cache_before+1&&
            strstr(message,"Cached 1 Raw Gold")!=NULL,
            "The Rope Store button must cache the selected carried load.");
        mine_control=MineControl(1);
        client_test_pointer_active=true;
        client_test_pointer=(Vector2){mine_control.x+mine_control.width/2,
                                      mine_control.y+mine_control.height/2};
        queued_mouse_button_press[MOUSE_BUTTON_LEFT]=true;
        HandleMineInput(input_journal,&input,&input_local,surface,0.0f,message,sizeof(message));
        client_test_pointer_active=false;
        ClientInputClearPressed();
        MINE_CHECK(CcMineCacheGood(&input,CC_GOOD_GOLD)==cache_before&&
            strstr(message,"Recovered 1 Raw Gold")!=NULL,
            "The Rope Store button must recover the selected finite load.");
        MINE_CHECK(CcJournalClose(&input_journal,&input,error,sizeof(error)),"Pointer mine journal must close.");
        (void)remove(input_path);
#undef INPUT_POINTER
#undef INPUT_STEP
    }
    uint64_t book_hash=CcSimHash(&sim);
    view=VIEW_LEDGER;back=VIEW_LOCAL;
    MINE_KEY(KEY_B);
    MINE_CHECK(view==VIEW_LOCAL && CcSimHash(&sim)==book_hash,
        "Closing the company book must restore the same mine visit.");
    Rectangle mine_navigation=MineNavigationControl(0);
    client_test_pointer_active=true;
    client_test_pointer=(Vector2){mine_navigation.x+mine_navigation.width/2,
                                  mine_navigation.y+mine_navigation.height/2};
    queued_mouse_button_press[MOUSE_BUTTON_LEFT]=true;
    MINE_INPUT();
    client_test_pointer_active=false;
    MINE_CHECK(CcSaveRead(path,&restored,error,sizeof(error)) &&
        CcSimHash(&sim)==CcSimHash(&restored),
        "The visible mine Save control must preserve the current mine visit.");
    mine_navigation=MineNavigationControl(1);
    client_test_pointer_active=true;
    client_test_pointer=(Vector2){mine_navigation.x+mine_navigation.width/2,
                                  mine_navigation.y+mine_navigation.height/2};
    queued_mouse_button_press[MOUSE_BUTTON_LEFT]=true;
    MINE_INPUT();
    client_test_pointer_active=false;
    MINE_CHECK(view==VIEW_PAUSE && CcSimHash(&sim)==book_hash,
        "The visible mine Menu control must open the pause menu without changing the visit.");
    MINE_KEY(KEY_ESCAPE);
    MINE_CHECK(view==VIEW_LOCAL && CcSimHash(&sim)==book_hash,
        "Closing the mine pause menu must restore the same visit.");
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
    uint64_t carriage_approach_hash=CcSimHash(&sim);
    RenderTexture2D yard_surface={.texture={.width=630,.height=960}};
    Camera3D yard_camera=CcLocalMineCamera(&sim,local.mine_facing);
    Vector2 carriage_pixel={0};
    int32_t carriage_x=-1,carriage_y=-1;
    MineIntentTarget carriage_target=MINE_INTENT_MOVE;
    bool carriage_picked=false;
    Rectangle yard_view=MineViewport(&sim,&local);
    if(yard_view.width<=0||yard_view.height<=0)
        yard_view=(Rectangle){0,0,630,960};
    for(int32_t sample_z=17;sample_z<=20&&!carriage_picked;++sample_z)
        for(int32_t sample_x=14;sample_x<=16&&!carriage_picked;++sample_x) {
            Vector2 sample=GetWorldToScreenEx(
                (Vector3){(float)sample_x,1.0f,(float)sample_z},yard_camera,630,960);
            Vector2 pointer={yard_view.x+sample.x*yard_view.width/630.0f,
                yard_view.y+sample.y*yard_view.height/960.0f};
            MineIntentTarget sampled_target=MINE_INTENT_MOVE;
            int32_t sampled_x=-1,sampled_y=-1;
            bool sampled=MinePick(&sim,&local,yard_surface,pointer,
                &sampled_x,&sampled_y,&sampled_target);
            if(sampled&&sampled_target==MINE_INTENT_CARRIAGE) {
                carriage_pixel=sample;carriage_x=sampled_x;carriage_y=sampled_y;
                carriage_target=sampled_target;carriage_picked=true;
            }
        }
    MINE_CHECK(carriage_picked && carriage_target==MINE_INTENT_CARRIAGE &&
        carriage_x==15 && carriage_y==18,
        "The visible carriage must publish a named pointer target.");
    Vector2 carriage_pointer={yard_view.x+carriage_pixel.x*yard_view.width/630.0f,
        yard_view.y+carriage_pixel.y*yard_view.height/960.0f};
    client_test_pointer_active=true;
    client_test_pointer=carriage_pointer;
    queued_mouse_button_press[MOUSE_BUTTON_LEFT]=true;
    HandleMineInput(journal,&sim,&local,yard_surface,.2f,message,sizeof(message));
    client_test_pointer_active=false;
    ClientInputClearPressed();
    MINE_CHECK(sim.mine.phase==CC_MINE_YARD &&
        CcSimHash(&sim)==carriage_approach_hash,
        "The carriage target must preserve the active visit and road anchor.");
    MINE_CHECK(strstr(message,"Choose Pack, Unload, or Board")!=NULL,
        "The carriage target must open its transfer choices.");
    MINE_KEY(KEY_S);
    MINE_CHECK(sim.mine.phase==CC_MINE_YARD && MineAtCarriage(&sim),
        "Ordinary movement beside the carriage must retain the mine visit.");
    MINE_KEY(KEY_E);
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
