static int MineScreenWidth(void) { return GetScreenWidth()>0?GetScreenWidth():1040; }
static int MineScreenHeight(void) { return GetScreenHeight()>0?GetScreenHeight():620; }
static bool MineAtCarriage(const CcSim *sim);
static bool MineShortLandscape(void)
{
    return MineScreenWidth()>=700 && MineScreenHeight()<500;
}
static Rectangle MineViewportForSize(int32_t width,int32_t height)
{
    float top=width>=700&&height<500?64.0f:100.0f;
    float bottom=(float)height-112.0f;
    return (Rectangle){0,top,(float)width,bottom-top};
}
static Rectangle MineViewport(const CcSim *sim,const LocalState *local)
{
    (void)sim; (void)local;
    int32_t width=GetScreenWidth(),height=GetScreenHeight();
    if(width<=0||height<=0) return (Rectangle){0};
    return MineViewportForSize(width,height);
}
static Rectangle MineInputViewport(const CcSim *sim,const LocalState *local,
                                   RenderTexture2D target)
{
    Rectangle view=MineViewport(sim,local);
    if((view.width<=0||view.height<=0) &&
       target.texture.width>0&&target.texture.height>0)
        view=(Rectangle){0,0,(float)target.texture.width,
                             (float)target.texture.height};
    return view;
}
static void MineRenderTargetSize(const CcSim *sim,const LocalState *local,
                                 int32_t *out_width,int32_t *out_height)
{
    Rectangle view=MineViewport(sim,local);
    *out_width=(int32_t)fmaxf(1.0f,roundf(view.width));
    *out_height=(int32_t)fmaxf(1.0f,roundf(view.height));
}
static Rectangle MineControl(int32_t index)
{
    float button_width=((float)MineScreenWidth()-56.0f)/3.0f;
    return (Rectangle){18.0f+(float)index*(button_width+4.0f),
        (float)MineScreenHeight()-70,button_width,34};
}
static Rectangle MineSelectionControl(int32_t index)
{
    float button_width=((float)MineScreenWidth()-56.0f)/4.0f;
    return (Rectangle){18.0f+(float)index*(button_width+4.0f),
        (float)MineScreenHeight()-110,button_width,30};
}
static Rectangle MineNavigationControl(int32_t index)
{
    return (Rectangle){(float)MineScreenWidth()-166.0f+(float)index*76.0f,
        12.0f,72.0f,32.0f};
}
static bool MineAtCarriage(const CcSim *sim)
{
    return sim->mine.phase==CC_MINE_YARD &&
        abs(sim->mine.x-CC_MINE_YARD_CARRIAGE_X)+
        abs(sim->mine.y-CC_MINE_YARD_CARRIAGE_Y)<=1;
}
typedef enum MineIntentTarget {
    MINE_INTENT_MOVE,
    MINE_INTENT_MOUTH,
    MINE_INTENT_CARRIAGE,
    MINE_INTENT_EXIT,
    MINE_INTENT_BAR,
    MINE_INTENT_SURVEY,
    MINE_INTENT_HAULERS,
    MINE_INTENT_COUNT
} MineIntentTarget;
static void MineClearIntent(LocalState *local)
{
    local->mine_target_x=-1; local->mine_target_y=-1;
    local->mine_intent_target=MINE_INTENT_MOVE;
    local->mine_intent_revision=-1;
}
static void MineShowReticle(LocalState *local,Vector2 point,bool accepted)
{
    local->movement_reticle=point;
    local->movement_reticle_age=0.0f;
    local->movement_reticle_valid=true;
    local->movement_reticle_accepted=accepted;
}
static void SyncMineView(const CcSim *sim, LocalState *local)
{
    if(local->mine_view_phase==sim->mine.phase) return;
    local->mine_facing=sim->mine.phase==CC_MINE_LEVEL?1:0;
    local->mine_view_phase=sim->mine.phase;
    local->mine_cooldown=.18f;
    MineClearIntent(local);
    local->movement_reticle_valid=false;
    memset(local->mine_known,0,sizeof(local->mine_known));
    local->mine_visibility_x=-1;
    local->mine_visibility_y=-1;
}
static int32_t MineCell(int32_t x,int32_t y)
{
    return y*CC_MINE_WIDTH+x;
}
static BoundingBox MineBoxBounds(float x,float y,float z,float w,float h,float d)
{
    return (BoundingBox){{x-w*.5f,y-h*.5f,z-d*.5f},{x+w*.5f,y+h*.5f,z+d*.5f}};
}
static BoundingBox MineClosedBarBounds(void)
{
    return MineBoxBounds(15,.7f,10,1,1.4f,1);
}
static bool MineRayBoxDistance(Ray ray,BoundingBox box,float *out_distance)
{
    const float origin[]={ray.position.x,ray.position.y,ray.position.z};
    const float direction[]={ray.direction.x,ray.direction.y,ray.direction.z};
    const float minimum[]={box.min.x,box.min.y,box.min.z};
    const float maximum[]={box.max.x,box.max.y,box.max.z};
    float direction_length_squared=ray.direction.x*ray.direction.x+
        ray.direction.y*ray.direction.y+ray.direction.z*ray.direction.z;
    if(!isfinite(direction_length_squared)||direction_length_squared<=.000000000001f)
        return false;
    float near_distance=0.0f,far_distance=INFINITY;
    for(int32_t axis=0;axis<3;++axis) {
        if(!isfinite(origin[axis])||!isfinite(direction[axis])) return false;
        if(fabsf(direction[axis])<=.000001f) {
            if(origin[axis]<minimum[axis]||origin[axis]>maximum[axis]) return false;
            continue;
        }
        float first=(minimum[axis]-origin[axis])/direction[axis];
        float last=(maximum[axis]-origin[axis])/direction[axis];
        if(first>last) {float swap=first;first=last;last=swap;}
        if(first>near_distance) near_distance=first;
        if(last<far_distance) far_distance=last;
        if(near_distance>far_distance) return false;
    }
    if(far_distance<0.0f||!isfinite(near_distance)) return false;
    *out_distance=near_distance;
    return true;
}
static bool MineClosedBarDistance(Ray ray,float *out_distance)
{
    const BoundingBox beams[]={
        MineBoxBounds(14.6f,.7f,10,.16f,1.4f,.16f),
        MineBoxBounds(15.4f,.7f,10,.16f,1.4f,.16f),
        MineBoxBounds(15,.35f,10,.95f,.18f,.18f),
        MineBoxBounds(15,.85f,10,.95f,.18f,.18f),
        MineBoxBounds(15,1.25f,10,.95f,.18f,.18f)
    };
    bool hit=false;
    float nearest=INFINITY;
    for(size_t i=0;i<sizeof(beams)/sizeof(beams[0]);++i) {
        float distance=0.0f;
        if(MineRayBoxDistance(ray,beams[i],&distance) && distance<nearest) {
            nearest=distance;hit=true;
        }
    }
    if(hit) *out_distance=nearest;
    return hit;
}
/* These are the same blocking cells drawn in mine_scene.inc.  Input uses
   their bounds before it accepts a floor or object behind them. */
static float MineOccluderDistanceExcept(const CcSim *sim,Ray ray,float limit,
                                        int32_t skip_x,int32_t skip_y)
{
    float nearest=limit;
    if(sim->mine.phase==CC_MINE_YARD) {
        for(int32_t row=0;row<2;++row) for(int32_t col=0;col<2;++col) {
            float x=col==0?5.5f:24.5f,z=row==0?6.5f:12.5f;
            float distance=0.0f;
            if(MineRayBoxDistance(ray,MineBoxBounds(x,1.15f,z,6,2.3f,4),&distance) &&
               distance<nearest) nearest=distance;
        }
        return nearest;
    }
    if(sim->mine.phase!=CC_MINE_LEVEL) return nearest;
    for(int32_t y=1;y<CC_MINE_HEIGHT-1;++y) for(int32_t x=1;x<CC_MINE_WIDTH-1;++x) {
        if((x==skip_x&&y==skip_y)||CcMineWalkable(sim,CC_MINE_LEVEL,x,y)) continue;
        bool edge=CcMineWalkable(sim,CC_MINE_LEVEL,x+1,y)||CcMineWalkable(sim,CC_MINE_LEVEL,x-1,y)||
            CcMineWalkable(sim,CC_MINE_LEVEL,x,y+1)||CcMineWalkable(sim,CC_MINE_LEVEL,x,y-1);
        if(!edge) continue;
        float distance=0.0f;
        bool hit=x==15&&y==10&&!sim->mine.bar_open?
            MineClosedBarDistance(ray,&distance):
            MineRayBoxDistance(ray,MineBoxBounds((float)x,.7f,(float)y,1,1.4f,1),&distance);
        if(hit && distance<nearest) nearest=distance;
    }
    return nearest;
}
static bool MineLineVisibleExcept(const CcSim *sim,Vector3 from,Vector3 to,
                                  int32_t skip_x,int32_t skip_y);
static bool MineLineVisibleExcept(const CcSim *sim,Vector3 from,Vector3 to,
                                  int32_t skip_x,int32_t skip_y)
{
    float dx=to.x-from.x,dy=to.y-from.y,dz=to.z-from.z;
    float distance=sqrtf(dx*dx+dy*dy+dz*dz);
    if(distance<=.001f) return true;
    Ray ray={.position=from,.direction={dx/distance,dy/distance,dz/distance}};
    return MineOccluderDistanceExcept(sim,ray,distance-.01f,skip_x,skip_y)>=distance-.01f;
}
static void MineRememberVisible(const CcSim *sim,LocalState *local)
{
    if(local->mine_visibility_x==sim->mine.x && local->mine_visibility_y==sim->mine.y &&
       local->mine_visibility_phase==sim->mine.phase &&
       local->mine_visibility_bar_open==sim->mine.bar_open) return;
    Vector3 eye={(float)sim->mine.x,.55f,(float)sim->mine.y};
    for(int32_t y=1;y<CC_MINE_HEIGHT-1;++y) for(int32_t x=1;x<CC_MINE_WIDTH-1;++x) {
        if(!CcMineWalkable(sim,sim->mine.phase,x,y)) continue;
        local->mine_visibility_rays+=1;
        if(MineLineVisibleExcept(sim,eye,(Vector3){(float)x,0,(float)y},-1,-1))
            local->mine_known[MineCell(x,y)]=1;
    }
    local->mine_known[MineCell(sim->mine.x,sim->mine.y)]=1;
    local->mine_visibility_x=sim->mine.x;
    local->mine_visibility_y=sim->mine.y;
    local->mine_visibility_phase=sim->mine.phase;
    local->mine_visibility_bar_open=sim->mine.bar_open;
    local->mine_visibility_scans+=1;
}
/* A route may use remembered floor, never a cell the player has not observed.
   This keeps a click useful down a known corridor without making it solve a
   hidden loop. */
static bool MinePath(const CcSim *sim,const LocalState *local,int32_t tx,int32_t ty,
                     int32_t *out_step,int32_t *out_distance)
{
    int32_t queue[CC_MINE_WIDTH*CC_MINE_HEIGHT],first[CC_MINE_WIDTH*CC_MINE_HEIGHT],distance[CC_MINE_WIDTH*CC_MINE_HEIGHT];
    for(int32_t i=0;i<CC_MINE_WIDTH*CC_MINE_HEIGHT;++i) {first[i]=-2;distance[i]=-1;}
    int32_t origin=sim->mine.y*CC_MINE_WIDTH+sim->mine.x,head=0,tail=0;
    queue[tail++]=origin; first[origin]=-1; distance[origin]=0;
    const int32_t dx[]={0,1,0,-1},dy[]={-1,0,1,0};
    while(head<tail) {
        int32_t cell=queue[head++],x=cell%CC_MINE_WIDTH,y=cell/CC_MINE_WIDTH;
        if(x==tx&&y==ty) {
            *out_step=first[cell]; *out_distance=distance[cell]; return true;
        }
        for(int32_t d=0;d<4;++d) {
            int32_t nx=x+dx[d],ny=y+dy[d];
            if(!CcMineWalkable(sim,sim->mine.phase,nx,ny) ||
               !local->mine_known[MineCell(nx,ny)]) continue;
            int32_t next=ny*CC_MINE_WIDTH+nx;
            if(first[next]!=-2) continue;
            first[next]=cell==origin?d:first[cell]; distance[next]=distance[cell]+1;
            queue[tail++]=next;
        }
    }
    return false;
}
static int32_t MinePathStep(const CcSim *sim,const LocalState *local,int32_t tx,int32_t ty)
{
    int32_t step=-1,distance=-1;
    return MinePath(sim,local,tx,ty,&step,&distance)?step:-1;
}
static bool MineTargetBounds(const CcSim *sim,MineIntentTarget target,BoundingBox *out_box,
                             int32_t *out_x,int32_t *out_y)
{
    switch(target) {
    case MINE_INTENT_MOUTH:
        if(sim->mine.phase!=CC_MINE_YARD) return false;
        *out_box=MineBoxBounds(CC_MINE_YARD_MOUTH_X,1.1f,
            CC_MINE_YARD_MOUTH_Y-.1f,2,2.2f,.2f);
        *out_x=CC_MINE_YARD_MOUTH_X;*out_y=CC_MINE_YARD_MOUTH_Y;
        return true;
    case MINE_INTENT_CARRIAGE:
        if(sim->mine.phase!=CC_MINE_YARD) return false;
        *out_box=MineBoxBounds(CC_MINE_YARD_CARRIAGE_X,1,
            CC_MINE_YARD_CARRIAGE_Y,3,2,4);
        *out_x=CC_MINE_YARD_CARRIAGE_X;*out_y=CC_MINE_YARD_CARRIAGE_Y;
        return true;
    case MINE_INTENT_EXIT:
        if(sim->mine.phase!=CC_MINE_LEVEL) return false;
        *out_box=MineBoxBounds(5,.25f,3,.85f,.5f,.85f); *out_x=5;*out_y=3; return true;
    case MINE_INTENT_BAR:
        if(sim->mine.phase!=CC_MINE_LEVEL||sim->mine.bar_open) return false;
        *out_box=MineClosedBarBounds(); *out_x=15;*out_y=10; return true;
    case MINE_INTENT_SURVEY:
        if(sim->mine.phase!=CC_MINE_LEVEL) return false;
        *out_box=MineBoxBounds(26,.37f,4,.6f,.12f,.45f); *out_x=26;*out_y=4; return true;
    case MINE_INTENT_HAULERS:
        if(sim->mine.phase!=CC_MINE_LEVEL ||
           sim->mine.encounter_outcome==CC_MINE_ENCOUNTER_BARGAINED ||
           sim->mine.encounter_outcome==CC_MINE_ENCOUNTER_CONTESTED) return false;
        {
            Vector2 first=CcLocalMineHaulerPosition(sim,0);
            Vector2 second=CcLocalMineHaulerPosition(sim,1);
            float min_x=fminf(first.x,second.x)-.45f;
            float max_x=fmaxf(first.x,second.x)+.45f;
            float min_z=fminf(first.y,second.y)-.45f;
            float max_z=fmaxf(first.y,second.y)+.45f;
            *out_box=MineBoxBounds((min_x+max_x)*.5f,.95f,
                (min_z+max_z)*.5f,max_x-min_x,1.9f,max_z-min_z);
            *out_x=sim->mine.source_x;*out_y=sim->mine.source_y;
            return true;
        }
    default: return false;
    }
}
/* Pick the first hit.  A wall beats the floor behind it, and a visible object
   beats its nearby floor so an object click can retain its named use. */
static bool MinePick(const CcSim *sim,const LocalState *local,
                     RenderTexture2D target,Vector2 mouse,
                     int32_t *out_x,int32_t *out_y,MineIntentTarget *out_target)
{
    Rectangle view=MineInputViewport(sim,local,target);
    if(!CheckCollisionPointRec(mouse,view) || view.width<=0 || view.height<=0) return false;
    Vector2 pixel={(mouse.x-view.x)*(float)target.texture.width/view.width,
                   (mouse.y-view.y)*(float)target.texture.height/view.height};
    Ray ray=GetScreenToWorldRayEx(pixel,CcLocalMineCamera(sim,local->mine_facing),
                                  target.texture.width,target.texture.height);
    float distance=INFINITY;
    if(fabsf(ray.direction.y)>=.0001f) {
        float floor_distance=-ray.position.y/ray.direction.y;
        if(floor_distance>0.0f) distance=floor_distance;
    }
    MineIntentTarget picked=MINE_INTENT_MOVE;
    float nearest=distance;
    for(int32_t candidate=MINE_INTENT_MOUTH;candidate<MINE_INTENT_COUNT;++candidate) {
        BoundingBox box; int32_t x,y;
        if(!MineTargetBounds(sim,(MineIntentTarget)candidate,&box,&x,&y)) continue;
        float hit_distance=0.0f;
        int32_t skip_x=candidate==MINE_INTENT_BAR?15:-1;
        int32_t skip_y=candidate==MINE_INTENT_BAR?10:-1;
        bool hit=candidate==MINE_INTENT_BAR?
            MineClosedBarDistance(ray,&hit_distance):MineRayBoxDistance(ray,box,&hit_distance);
        Vector3 eye={(float)sim->mine.x,.55f,(float)sim->mine.y};
        Vector3 object={(float)x,.55f,(float)y};
        if(hit && hit_distance<nearest&&
           MineLineVisibleExcept(sim,eye,object,skip_x,skip_y)&&
           MineOccluderDistanceExcept(sim,ray,hit_distance,skip_x,skip_y)>=hit_distance) {
            nearest=hit_distance; picked=(MineIntentTarget)candidate; *out_x=x; *out_y=y;
        }
    }
    if(picked!=MINE_INTENT_MOVE) {*out_target=picked;return true;}
    if(!isfinite(distance)) return false;
    *out_x=(int32_t)roundf(ray.position.x+distance*ray.direction.x);
    *out_y=(int32_t)roundf(ray.position.z+distance*ray.direction.z);
    *out_target=MINE_INTENT_MOVE;
    return CcMineWalkable(sim,sim->mine.phase,*out_x,*out_y) &&
           local->mine_known[MineCell(*out_x,*out_y)];
}
/* Name the things you can walk to, the way a town names its doors. */
static bool MineLabelPosition(RenderTexture2D target,const CcSim *sim,const LocalState *local,
                              Vector3 point,int32_t skip_x,int32_t skip_y,int *out_x,int *out_y)
{
    if(target.texture.width<=0||target.texture.height<=0) return false;
    Camera3D camera=CcLocalMineCamera(sim,local->mine_facing);
    /* A point behind the eye still projects to a screen position, so the
       label would float over the passage ahead. Drop it instead. */
    Vector3 gaze={camera.target.x-camera.position.x,camera.target.y-camera.position.y,
                  camera.target.z-camera.position.z};
    Vector3 toward={point.x-camera.position.x,point.y-camera.position.y,
                    point.z-camera.position.z};
    if(gaze.x*toward.x+gaze.y*toward.y+gaze.z*toward.z<=0.0f ||
       !MineLineVisibleExcept(sim,camera.position,point,skip_x,skip_y)) return false;
    Vector2 pixel=GetWorldToScreenEx(point,camera,
                                     target.texture.width,target.texture.height);
    Rectangle view=MineInputViewport(sim,local,target);
    int x=(int)(view.x+pixel.x*view.width/(float)target.texture.width);
    int y=(int)(view.y+pixel.y*view.height/(float)target.texture.height);
    if((float)x<view.x+80 || (float)x>view.x+view.width-80 ||
       (float)y<view.y+12 || (float)y>view.y+view.height-20) return false;
    *out_x=x; *out_y=y;
    return true;
}
static void MineLabel(RenderTexture2D target,const CcSim *sim,const LocalState *local,
                      Vector3 point,int32_t target_x,int32_t target_y,const char *text)
{
    int x,y;
    if(!MineLabelPosition(target,sim,local,point,target_x,target_y,&x,&y)) return;
    int width=CcOverlayMeasureText(text,10);
    DrawRectangle(x-width/2-5,y-4,width+10,18,Fade(BACKGROUND,.88f));
    CcOverlayDrawText(text,x-width/2,y,10,INK);
}
static void MineBeginTouchFrame(const CcSim *sim)
{
    bool underground=sim!=NULL&&sim->mine.phase==CC_MINE_LEVEL;
    ClientTouchBegin();
    ClientTouchHeading(underground?"The Underroad":"Mine Yard",
        underground?CcMineChamberName(CcMineChamber(sim->mine.x,sim->mine.y)):
        "The carriage waits beside the mine road.");
}
static void MineSetTouchDetail(const CcSim *sim,const char *hint)
{
    bool underground=sim!=NULL&&sim->mine.phase==CC_MINE_LEVEL;
    char detail[512];
    (void)snprintf(detail,sizeof(detail),"%s. %s",
        underground?CcMineChamberName(CcMineChamber(sim->mine.x,sim->mine.y)):
            "The carriage waits beside the mine road",
        hint!=NULL?hint:"");
    ClientTouchHeading(underground?"The Underroad":"Mine Yard",detail);
}
static void DrawMineScene(const CcSim *sim, LocalState *local, RenderTexture2D target, const char *message)
{
    SyncMineView(sim,local);
    bool underground=sim->mine.phase==CC_MINE_LEVEL;
    MineBeginTouchFrame(sim);
    bool compact=MineScreenWidth()<600;
    bool short_landscape=MineShortLandscape();
    bool brief=compact||short_landscape;
    DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),BACKGROUND);
    CcLocalDrawMine3D(sim,&local->agent,&local->course,local->mine_facing,
        target,MineViewport(sim,local));
    DrawLocalMovementReticle(local,MineViewport(sim,local));
    if(underground) {
        if(!sim->mine.bar_open) MineLabel(target,sim,local,(Vector3){15,1,10},15,10,"Wooden bar / lift it");
        MineLabel(target,sim,local,(Vector3){26,.6f,4},26,4,
            sim->mine.surveyed?"Workers' records / reread":"Workers' records / read");
        if(sim->mine.source_owner_id==sim->goblins.id &&
           sim->mine.encounter_outcome!=CC_MINE_ENCOUNTER_BARGAINED &&
           sim->mine.encounter_outcome!=CC_MINE_ENCOUNTER_CONTESTED)
            MineLabel(target,sim,local,(Vector3){26,.8f,15.2f},26,16,
                sim->mine.contest_active?"Goblin haulers / engaged":"Goblin haulers / approach");
        MineLabel(target,sim,local,(Vector3){5,.6f,3},5,3,"Back to the yard");
    } else {
        MineLabel(target,sim,local,
            (Vector3){CC_MINE_YARD_MOUTH_X,3,CC_MINE_YARD_MOUTH_Y},
            CC_MINE_YARD_MOUTH_X,CC_MINE_YARD_MOUTH_Y,
            "Mine Mouth / walk in");
        MineLabel(target,sim,local,
            (Vector3){CC_MINE_YARD_CARRIAGE_X,1,CC_MINE_YARD_CARRIAGE_Y},
            CC_MINE_YARD_CARRIAGE_X,CC_MINE_YARD_CARRIAGE_Y,
            "Carriage / return to road");
    }
    int32_t title_y=short_landscape?12:20;
    int32_t subtitle_y=short_landscape?30:43;
    int32_t summary_y=short_landscape?50:65;
    CcOverlayDrawText(underground?"THE UNDERROAD / 01":brief?"MINE YARD":"LOW SILVER PIT / MINE YARD",22,title_y,11,TEAL);
    CcOverlayDrawText(underground?CcMineChamberName(CcMineChamber(sim->mine.x,sim->mine.y)):
        brief?"Carriage at mine road":"The carriage waits beside the mine road",22,subtitle_y,brief?14:20,INK);
    CcOverlayDrawText(underground?TextFormat("Light %d",sim->mine.light):
        "Click the mine mouth to enter",22,summary_y,10,INK);
    const char *action=CcMineAction(sim);
    bool at_carriage=MineAtCarriage(sim);
    bool at_haulers=underground && abs(sim->mine.x-sim->mine.source_x)+
        abs(sim->mine.y-sim->mine.source_y)<=1;
    const char *hint=action!=NULL?action:
        underground?"Click a visible place to walk there.":"Click a place or the mine mouth.";
    MineSetTouchDetail(sim,hint);
    CcOverlayDrawText(hint,22,GetScreenHeight()-(sim->mine.contest_active?129:100),11,TEAL);
    if(sim->mine.contest_active) {
        AdventureButton(MineControl(0),"Target / T",true,false);
        AdventureButton(MineControl(1),"Strike / Space",true,false);
        AdventureButton(MineControl(2),"Break / Backspace",true,false);
        AdventureButton(MineSelectionControl(0),"Crushing / 1",true,false);
        AdventureButton(MineSelectionControl(1),"Sunder / 2",true,false);
        AdventureButton(MineSelectionControl(2),"Second wind / 3",true,false);
        AdventureButton(MineSelectionControl(3),"Guard / X",true,false);
    } else if(at_haulers &&
              sim->mine.encounter_outcome!=CC_MINE_ENCOUNTER_BARGAINED &&
              sim->mine.encounter_outcome!=CC_MINE_ENCOUNTER_CONTESTED) {
        AdventureButton(MineControl(0),"Inspect / E",true,false);
        AdventureButton(MineControl(1),CcCoopClientActive()?"Contest: local play":"Contest / C",
            !CcCoopClientActive(),false);
    } else if(at_haulers) {
        AdventureButton(MineControl(0),"Inspect / E",true,false);
    } else {
        AdventureButton(MineControl(0),at_carriage?"Board carriage / F":
            action!=NULL?action:"Use / F",action!=NULL,false);
    }
    AdventureButton(MineNavigationControl(0),"Save F5",true,false);
    AdventureButton(MineNavigationControl(1),"Menu Esc",true,false);
    CcOverlayDrawText(message,22,GetScreenHeight()-29,10,INK);
    if(!compact) CcOverlayDrawText(sim->mine.contest_active?
        "Click a hauler to target and strike  |  Space strike  |  1-3 skills":
        "Click where to walk  |  F use  |  F5 save",
        GetScreenWidth()-390,56,10,MUTED);
}
static bool MineForwardUse(const CcSim *sim,int32_t dir)
{
    const CcMineVisit *m=&sim->mine;
    if(m->phase==CC_MINE_YARD)
        return dir==0 && m->x==CC_MINE_YARD_MOUTH_X &&
            (m->y==CC_MINE_YARD_MOUTH_Y+1||m->y==CC_MINE_YARD_MOUTH_Y);
    return dir==0 && m->x==5 && (m->y==4||m->y==3);
}
static bool MineIntentCanUse(const CcSim *sim,MineIntentTarget target)
{
    const CcMineVisit *mine=&sim->mine;
    switch(target) {
    case MINE_INTENT_MOUTH: return mine->phase==CC_MINE_YARD &&
        abs(mine->x-CC_MINE_YARD_MOUTH_X)+
        abs(mine->y-CC_MINE_YARD_MOUTH_Y)<=1;
    case MINE_INTENT_CARRIAGE: return mine->phase==CC_MINE_YARD &&
        abs(mine->x-CC_MINE_YARD_CARRIAGE_X)+
        abs(mine->y-CC_MINE_YARD_CARRIAGE_Y)<=1;
    case MINE_INTENT_EXIT: return mine->phase==CC_MINE_LEVEL &&
        abs(mine->x-5)+abs(mine->y-3)<=1;
    case MINE_INTENT_BAR: return mine->phase==CC_MINE_LEVEL && !mine->bar_open &&
        abs(mine->x-15)+abs(mine->y-10)<=1;
    case MINE_INTENT_SURVEY: return mine->phase==CC_MINE_LEVEL &&
        abs(mine->x-26)+abs(mine->y-4)<=1;
    default: return false;
    }
}
static bool MineIntentGoal(const CcSim *sim,const LocalState *local,MineIntentTarget target,
                           int32_t *out_x,int32_t *out_y)
{
    BoundingBox box; int32_t x,y;
    if(target==MINE_INTENT_MOVE) { *out_x=local->mine_target_x;*out_y=local->mine_target_y; return true; }
    if(MineIntentCanUse(sim,target) &&
       local->mine_known[MineCell(sim->mine.x,sim->mine.y)]) {
        *out_x=sim->mine.x; *out_y=sim->mine.y; return true;
    }
    if(!MineTargetBounds(sim,target,&box,&x,&y)) return false;
    int32_t best=-1,best_distance=CC_MINE_WIDTH*CC_MINE_HEIGHT+1;
    for(int32_t cy=1;cy<CC_MINE_HEIGHT-1;++cy) for(int32_t cx=1;cx<CC_MINE_WIDTH-1;++cx) {
        if(!CcMineWalkable(sim,sim->mine.phase,cx,cy) || !local->mine_known[MineCell(cx,cy)] ||
           abs(cx-x)+abs(cy-y)>1) continue;
        int32_t step=-1,distance=-1;
        if(MinePath(sim,local,cx,cy,&step,&distance) && distance<best_distance) {
            best=MineCell(cx,cy);best_distance=distance;
        }
    }
    if(best<0) return false;
    *out_x=best%CC_MINE_WIDTH;*out_y=best/CC_MINE_WIDTH;
    return true;
}
static void MineAlignLocalAgent(const CcSim *sim,LocalState *local)
{
    if(sim==NULL||local==NULL) return;
    CcLocalAgentSetScene(&local->agent,CC_LOCAL_SCENE_MINE);
    local->agent.position.x=(float)sim->mine.x;
    local->agent.position.y=0.0f;
    local->agent.position.z=(float)sim->mine.y;
    local->agent.target_point=local->agent.position;
    local->agent.command_point=local->agent.position;
    local->agent.command_origin=local->agent.position;
}
static void HandleMineInput(CcJournal *journal,CcSim *sim,LocalState *local,
                            RenderTexture2D target,float dt,char *message,size_t capacity)
{
    SyncMineView(sim,local);
    CcLocalMineSetBarOpen(sim->mine.bar_open);
    if(sim->mine.contest_active && !local->mine_combat_active) {
        CcLocalCourseStageMineEncounter(&local->course,&local->agent,sim);
        local->mine_combat_active=true;
    }
    if(sim->mine.contest_active) {
        (void)CcLocalWorldUpdate(&local->course,&local->agent,sim,dt,false,true);
        if(local->course.defenses_completed>0) {
            int32_t injury=(int32_t)lroundf(fmaxf(0.0f,
                CC_LOCAL_COMBAT_MAX_HEALTH-local->agent.combat.health));
            CcCommand resolved={.kind=CC_COMMAND_MINE_RESOLVE_CONTEST,
                .target_id=(CcId)sim->mine.revision,.amount=injury};
            if(ApplyCommand(journal,sim,resolved,message,capacity)) {
                MineAlignLocalAgent(sim,local);
                local->mine_combat_active=false;
                local->course.mine_encounter=false;
                (void)snprintf(message,capacity,
                    "The haulers yield their carried mine goods. Injury %d.",injury);
            }
            return;
        }
    } else {
        local->mine_combat_active=false;
        local->course.mine_encounter=false;
    }
    local->mine_cooldown-=dt;
    if(local->movement_reticle_valid) {
        local->movement_reticle_age+=dt;
        if(local->movement_reticle_age>(local->movement_reticle_accepted?.48f:.75f))
            local->movement_reticle_valid=false;
    }
    MineRememberVisible(sim,local);
    if(local->mine_target_x>=0 && local->mine_intent_revision!=sim->mine.revision)
        MineClearIntent(local);
    bool at_haulers=sim->mine.phase==CC_MINE_LEVEL &&
        abs(sim->mine.x-sim->mine.source_x)+abs(sim->mine.y-sim->mine.source_y)<=1;
    int32_t control=-1;
    if(ClientKeyPressed(KEY_E)||ClientKeyPressed(KEY_F)) control=4;
    if(sim->mine.contest_active && ClientKeyPressed(KEY_T)) control=4;
    if(sim->mine.contest_active && ClientKeyPressed(KEY_SPACE)) control=5;
    if(!sim->mine.contest_active && at_haulers &&
       sim->mine.encounter_outcome!=CC_MINE_ENCOUNTER_BARGAINED &&
       sim->mine.encounter_outcome!=CC_MINE_ENCOUNTER_CONTESTED &&
       ClientKeyPressed(KEY_C)) control=5;
    if(sim->mine.contest_active &&
       (ClientKeyPressed(KEY_R)||ClientKeyPressed(KEY_BACKSPACE))) control=9;
    if(sim->mine.contest_active && ClientKeyPressed(KEY_X)) control=13;
    if(sim->mine.contest_active) for(int32_t skill=0;skill<CC_COMBAT_SKILL_COUNT;++skill)
        if(ClientKeyPressed(KEY_ONE+skill)) control=10+skill;
    if(ClientMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Vector2 mouse=ClientPointerPosition();
        if(CheckCollisionPointRec(mouse,MineControl(0))) control=4;
        else if(CheckCollisionPointRec(mouse,MineControl(1))) control=5;
        else if(sim->mine.contest_active &&
                CheckCollisionPointRec(mouse,MineControl(2))) control=6;
        else if(sim->mine.contest_active && CheckCollisionPointRec(mouse,MineSelectionControl(0))) control=10;
        else if(sim->mine.contest_active && CheckCollisionPointRec(mouse,MineSelectionControl(1))) control=11;
        else if(sim->mine.contest_active && CheckCollisionPointRec(mouse,MineSelectionControl(2))) control=12;
        else if(sim->mine.contest_active && CheckCollisionPointRec(mouse,MineSelectionControl(3))) control=13;
        else if(sim->mine.contest_active) {
            int32_t target_index=CcLocalCoursePickPlayerTarget(
                &local->course,&local->agent,mouse,target,
                MineInputViewport(sim,local,target));
            if(CheckCollisionPointRec(mouse,MineInputViewport(sim,local,target)))
                MineShowReticle(local,mouse,target_index>=0);
            if(target_index>=0) {
                bool struck=CcLocalCourseBeginPlayerStrike(&local->course,&local->agent);
                (void)snprintf(message,capacity,struck?"Attacking %s.":"Focused: %s.",
                    local->course.raider_names[target_index]);
            }
        }
        else if(!sim->mine.contest_active) {
            int32_t tx=0,ty=0;
            MineIntentTarget picked=MINE_INTENT_MOVE;
            bool accepted=MinePick(sim,local,target,mouse,&tx,&ty,&picked);
            if(CheckCollisionPointRec(mouse,MineInputViewport(sim,local,target)))
                MineShowReticle(local,mouse,accepted);
            if(accepted) {
                MineClearIntent(local);
                local->mine_target_x=tx; local->mine_target_y=ty;
                local->mine_intent_target=(int32_t)picked;
                local->mine_intent_revision=sim->mine.revision;
                if(picked==MINE_INTENT_MOVE && !local->mine_known[MineCell(tx,ty)]) {
                    MineClearIntent(local);
                    (void)snprintf(message,capacity,"Walk there after the company has seen a route.");
                }
            }
        }
    }
    int32_t dir=-1;
    if(IsKeyDown(KEY_W)||ClientKeyPressed(KEY_W)||IsKeyDown(KEY_UP)||ClientKeyPressed(KEY_UP)) dir=0;
    if(IsKeyDown(KEY_D)||ClientKeyPressed(KEY_D)||IsKeyDown(KEY_RIGHT)||ClientKeyPressed(KEY_RIGHT)) dir=1;
    if(IsKeyDown(KEY_S)||ClientKeyPressed(KEY_S)||IsKeyDown(KEY_DOWN)||ClientKeyPressed(KEY_DOWN)) dir=2;
    if(IsKeyDown(KEY_A)||ClientKeyPressed(KEY_A)||IsKeyDown(KEY_LEFT)||ClientKeyPressed(KEY_LEFT)) dir=3;
    if(dir>=0||control>=0) MineClearIntent(local);
    if(sim->mine.contest_active && control>=10 && control<13) {
        CcCombatSkill skill=(CcCombatSkill)(control-10);
        bool used=CcLocalCourseUsePlayerSkill(&local->course,&local->agent,skill);
        (void)snprintf(message,capacity,used?"%s committed.":"%s is not ready.",
                       CcLocalCombatSkillName(skill));
        return;
    }
    if(sim->mine.contest_active && control==13) {
        bool guarded=!local->agent.humanoid.guard_requested;
        (void)CcLocalCourseSetPlayerGuarded(&local->course,&local->agent,guarded);
        (void)snprintf(message,capacity,"Guard %s.",guarded?"raised":"lowered");
        return;
    }
    if(dir<0&&local->mine_target_x>=0) {
        int32_t goal_x=-1,goal_y=-1;
        if(!MineIntentGoal(sim,local,(MineIntentTarget)local->mine_intent_target,&goal_x,&goal_y)) {
            MineClearIntent(local);
            (void)snprintf(message,capacity,"That route is no longer known or reachable.");
        } else if(sim->mine.x==goal_x && sim->mine.y==goal_y) {
            MineIntentTarget intent=(MineIntentTarget)local->mine_intent_target;
            MineClearIntent(local);
            if(intent==MINE_INTENT_HAULERS)
                (void)snprintf(message,capacity,"Choose the visible hauler action.");
            else if(intent==MINE_INTENT_CARRIAGE)
                (void)snprintf(message,capacity,"Carriage reached. Board to return to the road.");
            else if(intent!=MINE_INTENT_MOVE && MineIntentCanUse(sim,intent)) control=4;
        } else {
            dir=MinePathStep(sim,local,goal_x,goal_y);
            if(dir<0) {
                MineClearIntent(local);
                (void)snprintf(message,capacity,"The company has not seen a route there.");
            }
        }
    }
    CcCommand command={.target_id=(CcId)sim->mine.revision};
    if(sim->mine.contest_active && control==4) {
        int32_t current=local->agent.combat.target_index;
        int32_t target_index=current==0?1:0;
        if(!CcLocalCourseSelectPlayerTarget(&local->course,&local->agent,target_index))
            (void)CcLocalCourseSelectPlayerTarget(&local->course,&local->agent,current);
        (void)snprintf(message,capacity,"Focused on %s.",
            local->course.raider_names[local->agent.combat.target_index<0?0:
                local->agent.combat.target_index]);
        return;
    } else if(sim->mine.contest_active && control==5) {
        bool struck=CcLocalCourseBeginPlayerStrike(&local->course,&local->agent);
        (void)snprintf(message,capacity,"%s",
            struck?"Strike committed.":"Closing to striking distance.");
        return;
    } else if(sim->mine.contest_active && (control==6||control==9)) {
        command.kind=CC_COMMAND_MINE_BREAK_CONTACT;
        command.amount=(int32_t)lroundf(fmaxf(0.0f,
            CC_LOCAL_COMBAT_MAX_HEALTH-local->agent.combat.health));
    } else if(at_haulers && control==4)
        command.kind=CC_COMMAND_MINE_INSPECT;
    else if(at_haulers && control==5 &&
            sim->mine.encounter_outcome!=CC_MINE_ENCOUNTER_BARGAINED &&
            sim->mine.encounter_outcome!=CC_MINE_ENCOUNTER_CONTESTED) {
        if(CcCoopClientActive()) {
            (void)snprintf(message,capacity,"Contest needs local combat.");
            return;
        }
        command.kind=CC_COMMAND_MINE_CONTEST;
    } else if(control==4) command.kind=CC_COMMAND_MINE_USE;
    else if(dir>=0&&local->mine_cooldown<=0) {
        local->mine_cooldown=.18f;
        /* The same directions walk in the yard and in every mine chamber. */
        command.kind=MineForwardUse(sim,dir)?CC_COMMAND_MINE_USE:CC_COMMAND_MINE_STEP;
        command.amount=dir;
        local->mine_facing=dir;
    }
    bool entering=command.kind==CC_COMMAND_MINE_USE &&
        sim->mine.phase==CC_MINE_YARD && abs(sim->mine.x-15)+abs(sim->mine.y-3)<=1;
    bool reading_records=command.kind==CC_COMMAND_MINE_USE &&
        sim->mine.phase==CC_MINE_LEVEL && abs(sim->mine.x-26)+abs(sim->mine.y-4)<=1;
    bool boarding=command.kind==CC_COMMAND_MINE_USE && MineAtCarriage(sim);
    if(command.kind!=CC_COMMAND_NONE) {
        bool ok=ApplyCommand(journal,sim,command,message,capacity);
        if(ok) {
            if(command.kind==CC_COMMAND_MINE_CONTEST) {
                CcLocalCourseStageMineEncounter(&local->course,&local->agent,sim);
                local->mine_combat_active=true;
                (void)snprintf(message,capacity,
                    "The haulers draw steel. Choose a target and strike.");
            } else if(command.kind==CC_COMMAND_MINE_BREAK_CONTACT) {
                MineAlignLocalAgent(sim,local);
                local->mine_combat_active=false;
                local->course.mine_encounter=false;
                (void)snprintf(message,capacity,
                    "Contact broken. Walk back through the visible mine.");
            }
            SyncMineView(sim,local);
            if(local->mine_target_x>=0) local->mine_intent_revision=sim->mine.revision;
            if(command.kind==CC_COMMAND_MINE_INSPECT)
                (void)snprintf(message,capacity,
                    "Hauler load: Iron %d, Gold %d, Gems %d.",
                    CcMineSourceGood(sim,CC_GOOD_IRON),
                    CcMineSourceGood(sim,CC_GOOD_GOLD),
                    CcMineSourceGood(sim,CC_GOOD_GEMS));
            else if(entering)
                (void)snprintf(message,capacity,"Entered the mine. Light 18.");
            else if(reading_records) (void)snprintf(message,capacity,
                "Workers' records: The western store passage goes around the barred middle passage. The stair to Lamp Hall is marked blocked.");
            else if(boarding) (void)snprintf(message,capacity,
                "Boarded the carriage for the road.");
        } else MineClearIntent(local);
    }
}
