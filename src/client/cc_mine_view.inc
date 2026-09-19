static Rectangle MineViewport(void)
{
    return (Rectangle){0,82,(float)GetScreenWidth(),(float)GetScreenHeight()-246};
}
static Rectangle MineControl(int32_t index)
{
    float button_width=((float)GetScreenWidth()-56.0f)/3.0f;
    return (Rectangle){18.0f+(float)index*(button_width+4.0f),
        (float)GetScreenHeight()-78,button_width,34};
}
typedef enum MineIntentTarget {
    MINE_INTENT_MOVE,
    MINE_INTENT_MOUTH,
    MINE_INTENT_CARRIAGE,
    MINE_INTENT_EXIT,
    MINE_INTENT_BAR,
    MINE_INTENT_SURVEY
} MineIntentTarget;
static void MineClearIntent(LocalState *local)
{
    local->mine_target_x=-1; local->mine_target_y=-1;
    local->mine_intent_target=MINE_INTENT_MOVE;
    local->mine_intent_revision=-1;
}
static void SyncMineView(const CcSim *sim, LocalState *local)
{
    if(local->mine_view_phase==sim->mine.phase) return;
    local->mine_facing=sim->mine.phase==CC_MINE_LEVEL?1:0;
    local->mine_view_phase=sim->mine.phase;
    local->mine_cooldown=.18f;
    MineClearIntent(local);
    memset(local->mine_known,0,sizeof(local->mine_known));
}
static int32_t MineCell(int32_t x,int32_t y)
{
    return y*CC_MINE_WIDTH+x;
}
static BoundingBox MineBoxBounds(float x,float y,float z,float w,float h,float d)
{
    return (BoundingBox){{x-w*.5f,y-h*.5f,z-d*.5f},{x+w*.5f,y+h*.5f,z+d*.5f}};
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
            RayCollision hit=GetRayCollisionBox(ray,MineBoxBounds(x,1.15f,z,6,2.3f,4));
            if(hit.hit&&hit.distance<nearest) nearest=hit.distance;
        }
        return nearest;
    }
    if(sim->mine.phase!=CC_MINE_LEVEL) return nearest;
    for(int32_t y=1;y<CC_MINE_HEIGHT-1;++y) for(int32_t x=1;x<CC_MINE_WIDTH-1;++x) {
        if((x==skip_x&&y==skip_y)||CcMineWalkable(sim,CC_MINE_LEVEL,x,y)) continue;
        bool edge=CcMineWalkable(sim,CC_MINE_LEVEL,x+1,y)||CcMineWalkable(sim,CC_MINE_LEVEL,x-1,y)||
            CcMineWalkable(sim,CC_MINE_LEVEL,x,y+1)||CcMineWalkable(sim,CC_MINE_LEVEL,x,y-1);
        if(!edge) continue;
        RayCollision hit=GetRayCollisionBox(ray,MineBoxBounds((float)x,.7f,(float)y,1,1.4f,1));
        if(hit.hit&&hit.distance<nearest) nearest=hit.distance;
    }
    return nearest;
}
static float MineOccluderDistance(const CcSim *sim,Ray ray,float limit)
{
    return MineOccluderDistanceExcept(sim,ray,limit,-1,-1);
}
static bool MineLineVisible(const CcSim *sim,Vector3 from,Vector3 to)
{
    float dx=to.x-from.x,dy=to.y-from.y,dz=to.z-from.z;
    float distance=sqrtf(dx*dx+dy*dy+dz*dz);
    if(distance<=.001f) return true;
    Ray ray={.position=from,.direction={dx/distance,dy/distance,dz/distance}};
    return MineOccluderDistance(sim,ray,distance-.01f)>=distance-.01f;
}
static void MineRememberVisible(const CcSim *sim,LocalState *local)
{
    Vector3 eye={sim->mine.x,.55f,sim->mine.y};
    for(int32_t y=1;y<CC_MINE_HEIGHT-1;++y) for(int32_t x=1;x<CC_MINE_WIDTH-1;++x) {
        if(CcMineWalkable(sim,sim->mine.phase,x,y) &&
           MineLineVisible(sim,eye,(Vector3){(float)x,0,(float)y}))
            local->mine_known[MineCell(x,y)]=1;
    }
    local->mine_known[MineCell(sim->mine.x,sim->mine.y)]=1;
}
/* A route may use remembered floor, never a cell the player has not observed.
   This keeps a click useful down a known corridor without making it solve a
   hidden loop. */
static int32_t MinePathStep(const CcSim *sim,const LocalState *local,int32_t tx,int32_t ty)
{
    int32_t queue[CC_MINE_WIDTH*CC_MINE_HEIGHT], first[CC_MINE_WIDTH*CC_MINE_HEIGHT];
    for(int32_t i=0;i<CC_MINE_WIDTH*CC_MINE_HEIGHT;++i) first[i]=-2;
    int32_t origin=sim->mine.y*CC_MINE_WIDTH+sim->mine.x,head=0,tail=0;
    queue[tail++]=origin; first[origin]=-1;
    const int32_t dx[]={0,1,0,-1},dy[]={-1,0,1,0};
    while(head<tail) {
        int32_t cell=queue[head++],x=cell%CC_MINE_WIDTH,y=cell/CC_MINE_WIDTH;
        if(x==tx&&y==ty) return first[cell];
        for(int32_t d=0;d<4;++d) {
            int32_t nx=x+dx[d],ny=y+dy[d];
            if(!CcMineWalkable(sim,sim->mine.phase,nx,ny) ||
               !local->mine_known[MineCell(nx,ny)]) continue;
            int32_t next=ny*CC_MINE_WIDTH+nx;
            if(first[next]!=-2) continue;
            first[next]=cell==origin?d:first[cell]; queue[tail++]=next;
        }
    }
    return -1;
}
static bool MineTargetBounds(const CcSim *sim,MineIntentTarget target,BoundingBox *out_box,
                             int32_t *out_x,int32_t *out_y)
{
    switch(target) {
    case MINE_INTENT_MOUTH:
        if(sim->mine.phase!=CC_MINE_YARD) return false;
        *out_box=MineBoxBounds(15,1.1f,2.9f,2,2.2f,.2f); *out_x=15;*out_y=3; return true;
    case MINE_INTENT_CARRIAGE:
        if(sim->mine.phase!=CC_MINE_YARD) return false;
        *out_box=MineBoxBounds(18,1,19,3,2,4); *out_x=15;*out_y=18; return true;
    case MINE_INTENT_EXIT:
        if(sim->mine.phase!=CC_MINE_LEVEL) return false;
        *out_box=MineBoxBounds(5,.25f,3,.85f,.5f,.85f); *out_x=5;*out_y=3; return true;
    case MINE_INTENT_BAR:
        if(sim->mine.phase!=CC_MINE_LEVEL||sim->mine.bar_open) return false;
        *out_box=MineBoxBounds(15,.85f,10,1.3f,.2f,.3f); *out_x=15;*out_y=10; return true;
    case MINE_INTENT_SURVEY:
        if(sim->mine.phase!=CC_MINE_LEVEL||sim->mine.surveyed) return false;
        *out_box=MineBoxBounds(26,.37f,4,.6f,.12f,.45f); *out_x=26;*out_y=4; return true;
    default: return false;
    }
}
/* Pick the first hit.  A wall beats the floor behind it, and a visible object
   beats its nearby floor so an object click can retain its named use. */
static bool MinePick(const CcSim *sim,const LocalState *local,
                     RenderTexture2D target,Vector2 mouse,
                     int32_t *out_x,int32_t *out_y,MineIntentTarget *out_target)
{
    Rectangle view=MineViewport();
    if((view.width<=0||view.height<=0) && target.texture.width>0&&target.texture.height>0)
        view=(Rectangle){0,0,(float)target.texture.width,(float)target.texture.height};
    if(!CheckCollisionPointRec(mouse,view) || view.width<=0 || view.height<=0) return false;
    Vector2 pixel={(mouse.x-view.x)*(float)target.texture.width/view.width,
                   (mouse.y-view.y)*(float)target.texture.height/view.height};
    Ray ray=GetScreenToWorldRayEx(pixel,CcLocalMineCamera(sim,local->mine_facing),
                                  target.texture.width,target.texture.height);
    if(fabsf(ray.direction.y)<.0001f) return false;
    float distance=-ray.position.y/ray.direction.y;
    if(distance<=0.0f) return false;
    MineIntentTarget picked=MINE_INTENT_MOVE;
    float nearest=distance;
    for(int32_t candidate=MINE_INTENT_MOUTH;candidate<=MINE_INTENT_SURVEY;++candidate) {
        BoundingBox box; int32_t x,y;
        if(!MineTargetBounds(sim,(MineIntentTarget)candidate,&box,&x,&y)) continue;
        RayCollision hit=GetRayCollisionBox(ray,box);
        int32_t skip_x=candidate==MINE_INTENT_BAR?15:-1;
        int32_t skip_y=candidate==MINE_INTENT_BAR?10:-1;
        if(hit.hit&&hit.distance<nearest&&
           MineOccluderDistanceExcept(sim,ray,hit.distance,skip_x,skip_y)>=hit.distance) {
            nearest=hit.distance; picked=(MineIntentTarget)candidate; *out_x=x; *out_y=y;
        }
    }
    if(picked!=MINE_INTENT_MOVE) {*out_target=picked;return true;}
    if(MineOccluderDistance(sim,ray,distance)<distance) return false;
    *out_x=(int32_t)roundf(ray.position.x+distance*ray.direction.x);
    *out_y=(int32_t)roundf(ray.position.z+distance*ray.direction.z);
    *out_target=MINE_INTENT_MOVE;
    return CcMineWalkable(sim,sim->mine.phase,*out_x,*out_y);
}
/* Name the things you can walk to, the way a town names its doors. */
static void MineLabel(RenderTexture2D target, const CcSim *sim, const LocalState *local,
                      Vector3 point, const char *text)
{
    if(target.texture.width<=0||target.texture.height<=0) return;
    Camera3D camera=CcLocalMineCamera(sim,local->mine_facing);
    /* A point behind the eye still projects to a screen position, so the
       label would float over the passage ahead. Drop it instead. */
    Vector3 gaze={camera.target.x-camera.position.x,camera.target.y-camera.position.y,
                  camera.target.z-camera.position.z};
    Vector3 toward={point.x-camera.position.x,point.y-camera.position.y,
                    point.z-camera.position.z};
    if(gaze.x*toward.x+gaze.y*toward.y+gaze.z*toward.z<=0.0f ||
       !MineLineVisible(sim,camera.position,point)) return;
    Vector2 pixel=GetWorldToScreenEx(point,camera,
                                     target.texture.width,target.texture.height);
    Rectangle view=MineViewport();
    int x=(int)(view.x+pixel.x*view.width/(float)target.texture.width);
    int y=(int)(view.y+pixel.y*view.height/(float)target.texture.height);
    if((float)x<view.x+80 || (float)x>view.x+view.width-80 ||
       (float)y<view.y+12 || (float)y>view.y+view.height-20) return;
    int width=CcOverlayMeasureText(text,10);
    DrawRectangle(x-width/2-5,y-4,width+10,18,Fade(BACKGROUND,.88f));
    CcOverlayDrawText(text,x-width/2,y,10,INK);
}
static void DrawMineScene(const CcSim *sim, LocalState *local, RenderTexture2D target, const char *message)
{
    SyncMineView(sim,local);
    bool underground=sim->mine.phase==CC_MINE_LEVEL;
    DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),BACKGROUND);
    CcLocalDrawMine3D(sim,&local->agent,local->mine_facing,target,MineViewport());
    if(underground) {
        if(!sim->mine.bar_open) MineLabel(target,sim,local,(Vector3){15,1,10},"Wooden bar / lift it");
        if(!sim->mine.surveyed) MineLabel(target,sim,local,(Vector3){26,.6f,4},"Workers' survey");
        MineLabel(target,sim,local,(Vector3){5,.6f,3},"Back to the yard");
    } else {
        MineLabel(target,sim,local,(Vector3){15,3,3},"Mine Mouth / walk in");
        MineLabel(target,sim,local,(Vector3){18,1,19},"Carriage / pack food");
    }
    CcOverlayDrawText(underground?"THE UNDERROAD / 01":"LOW SILVER PIT / MINE YARD",22,20,11,TEAL);
    CcOverlayDrawText(underground?CcMineChamberName(CcMineChamber(sim->mine.x,sim->mine.y)):"The carriage waits beside the mine road",22,43,20,INK);
    CcOverlayDrawText(TextFormat("PACK %d/8  /  LIGHT %d",CcMinePackUsed(sim),sim->mine.light),
        22,GetScreenHeight()-151,11,INK);
    const char *action=CcMineAction(sim);
    CcOverlayDrawText(action!=NULL?action:underground?"Point where the company should walk. The workers' records lie east.":"Point where the company should walk. Pack food at the carriage, then walk into the mouth.",22,GetScreenHeight()-126,11,TEAL);
    AdventureButton(MineControl(0),action!=NULL?action:"Use / E",action!=NULL,false);
    AdventureButton(MineControl(1),"Pack food / P",!underground,false);
    CcOverlayDrawText(message,22,GetScreenHeight()-29,10,INK);
    CcOverlayDrawText("Click where to walk  |  F5 save",GetScreenWidth()-280,23,10,MUTED);
}
static bool MineForwardUse(const CcSim *sim,int32_t dir)
{
    const CcMineVisit *m=&sim->mine;
    if(m->phase==CC_MINE_YARD)
        return (dir==0 && m->x==15 && (m->y==4||m->y==3)) ||
            (dir==2 && m->x==15 && (m->y==17||m->y==18));
    return dir==0 && m->x==5 && (m->y==4||m->y==3);
}
static bool MineIntentCanUse(const CcSim *sim,MineIntentTarget target)
{
    const CcMineVisit *mine=&sim->mine;
    switch(target) {
    case MINE_INTENT_MOUTH: return mine->phase==CC_MINE_YARD &&
        abs(mine->x-15)+abs(mine->y-3)<=1;
    case MINE_INTENT_CARRIAGE: return mine->phase==CC_MINE_YARD &&
        abs(mine->x-15)+abs(mine->y-18)<=1;
    case MINE_INTENT_EXIT: return mine->phase==CC_MINE_LEVEL &&
        abs(mine->x-5)+abs(mine->y-3)<=1;
    case MINE_INTENT_BAR: return mine->phase==CC_MINE_LEVEL && !mine->bar_open &&
        abs(mine->x-15)+abs(mine->y-10)<=1;
    case MINE_INTENT_SURVEY: return mine->phase==CC_MINE_LEVEL && !mine->surveyed &&
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
        int32_t distance=(cx==sim->mine.x&&cy==sim->mine.y)?0:
            MinePathStep(sim,local,cx,cy);
        if(distance>=0 && distance<best_distance) {best=MineCell(cx,cy);best_distance=distance;}
    }
    if(best<0) return false;
    *out_x=best%CC_MINE_WIDTH;*out_y=best/CC_MINE_WIDTH;
    return true;
}
static void HandleMineInput(CcJournal *journal,CcSim *sim,LocalState *local,
                            RenderTexture2D target,float dt,char *message,size_t capacity)
{
    SyncMineView(sim,local);
    local->mine_cooldown-=dt;
    MineRememberVisible(sim,local);
    if(local->mine_target_x>=0 && local->mine_intent_revision!=sim->mine.revision)
        MineClearIntent(local);
    int32_t control=-1;
    if(ClientKeyPressed(KEY_E)) control=4;
    if(ClientKeyPressed(KEY_P)) control=5;
    if(ClientMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Vector2 mouse=ClientPointerPosition();
        if(CheckCollisionPointRec(mouse,MineControl(0))) control=4;
        else if(CheckCollisionPointRec(mouse,MineControl(1))) control=5;
        else {
            int32_t tx=0,ty=0;
            MineIntentTarget picked=MINE_INTENT_MOVE;
            if(MinePick(sim,local,target,mouse,&tx,&ty,&picked)) {
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
    bool relative=false;
    if(IsKeyDown(KEY_W)||ClientKeyPressed(KEY_W)||IsKeyDown(KEY_UP)||ClientKeyPressed(KEY_UP)) {dir=0;relative=true;}
    if(IsKeyDown(KEY_D)||ClientKeyPressed(KEY_D)||IsKeyDown(KEY_RIGHT)||ClientKeyPressed(KEY_RIGHT)) {dir=1;relative=true;}
    if(IsKeyDown(KEY_S)||ClientKeyPressed(KEY_S)||IsKeyDown(KEY_DOWN)||ClientKeyPressed(KEY_DOWN)) {dir=2;relative=true;}
    if(IsKeyDown(KEY_A)||ClientKeyPressed(KEY_A)||IsKeyDown(KEY_LEFT)||ClientKeyPressed(KEY_LEFT)) {dir=3;relative=true;}
    if(dir>=0||control>=0) MineClearIntent(local);
    if(dir<0&&local->mine_target_x>=0) {
        int32_t goal_x=-1,goal_y=-1;
        if(!MineIntentGoal(sim,local,(MineIntentTarget)local->mine_intent_target,&goal_x,&goal_y)) {
            MineClearIntent(local);
            (void)snprintf(message,capacity,"That route is no longer known or reachable.");
        } else if(sim->mine.x==goal_x && sim->mine.y==goal_y) {
            MineIntentTarget intent=(MineIntentTarget)local->mine_intent_target;
            MineClearIntent(local);
            if(intent!=MINE_INTENT_MOVE && MineIntentCanUse(sim,intent)) control=4;
        } else {
            dir=MinePathStep(sim,local,goal_x,goal_y);
            if(dir<0) {
                MineClearIntent(local);
                (void)snprintf(message,capacity,"The company has not seen a route there.");
            }
        }
    }
    CcCommand command={.target_id=(CcId)sim->mine.revision};
    if(control==4) command.kind=CC_COMMAND_MINE_USE;
    else if(control==5) {
        command.kind=CC_COMMAND_MINE_PACK; command.amount=1;
        command.good=sim->player.cargo[CC_GOOD_BREAD]>0?CC_GOOD_BREAD:CC_GOOD_MEAT;
    } else if(dir>=0&&local->mine_cooldown<=0) {
        local->mine_cooldown=.18f;
        if(sim->mine.phase==CC_MINE_LEVEL&&relative) {
            /* Keyboard underground still turns in place; the camera is the
               company's eye and a bare key has no tile to aim at. */
            if(dir==1||dir==3) {
                local->mine_facing=(local->mine_facing+dir)%4;
                return;
            }
            dir=(local->mine_facing+dir)%4;
        }
        /* Walking into a doorway uses it, so the seam is never a button. */
        command.kind=MineForwardUse(sim,dir)?CC_COMMAND_MINE_USE:CC_COMMAND_MINE_STEP;
        command.amount=dir;
        /* A walked-to tile turns the eye to face the way it went; a keyboard
           step keeps the facing the player set with A and D. */
        if(sim->mine.phase==CC_MINE_LEVEL&&!relative) local->mine_facing=dir;
    }
    if(command.kind!=CC_COMMAND_NONE) {
        bool ok=ApplyCommand(journal,sim,command,message,capacity);
        if(ok) {
            SyncMineView(sim,local);
            if(local->mine_target_x>=0) local->mine_intent_revision=sim->mine.revision;
            (void)snprintf(message,capacity,"%s",command.kind==CC_COMMAND_MINE_PACK?"Food moved from carriage to pack.":"");
        } else MineClearIntent(local);
    }
}
