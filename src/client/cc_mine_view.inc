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
static void SyncMineView(const CcSim *sim, LocalState *local)
{
    if(local->mine_view_phase==sim->mine.phase) return;
    local->mine_facing=sim->mine.phase==CC_MINE_LEVEL?1:0;
    local->mine_view_phase=sim->mine.phase;
    local->mine_cooldown=.18f;
    local->mine_target_x=-1; local->mine_target_y=-1;
}
/* One world, one way to move: the company walks to the tile you point at,
   above ground and below it alike. The camera decides how the walk is seen,
   never how it is ordered. */
static int32_t MinePathStep(const CcSim *sim,int32_t tx,int32_t ty)
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
            if(!CcMineWalkable(sim,sim->mine.phase,nx,ny)) continue;
            int32_t next=ny*CC_MINE_WIDTH+nx;
            if(first[next]!=-2) continue;
            first[next]=cell==origin?d:first[cell]; queue[tail++]=next;
        }
    }
    return -1;
}
/* The floor is the same plane in both views, so one ray picks a tile whether
   the camera stands over the yard or at eye height in the passage. */
static bool MinePickTile(const CcSim *sim,const LocalState *local,
                         RenderTexture2D target,Vector2 mouse,
                         int32_t *out_x,int32_t *out_y)
{
    Rectangle view=MineViewport();
    if(!CheckCollisionPointRec(mouse,view) || view.width<=0 || view.height<=0) return false;
    Vector2 pixel={(mouse.x-view.x)*(float)target.texture.width/view.width,
                   (mouse.y-view.y)*(float)target.texture.height/view.height};
    Ray ray=GetScreenToWorldRayEx(pixel,CcLocalMineCamera(sim,local->mine_facing),
                                  target.texture.width,target.texture.height);
    if(fabsf(ray.direction.y)<.0001f) return false;
    float distance=-ray.position.y/ray.direction.y;
    if(distance<=0.0f) return false;
    *out_x=(int32_t)roundf(ray.position.x+distance*ray.direction.x);
    *out_y=(int32_t)roundf(ray.position.z+distance*ray.direction.z);
    return true;
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
    if(gaze.x*toward.x+gaze.y*toward.y+gaze.z*toward.z<=0.0f) return;
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
static void HandleMineInput(CcJournal *journal,CcSim *sim,LocalState *local,
                            RenderTexture2D target,float dt,char *message,size_t capacity)
{
    SyncMineView(sim,local);
    local->mine_cooldown-=dt;
    int32_t control=-1;
    if(ClientKeyPressed(KEY_E)) control=4;
    if(ClientKeyPressed(KEY_P)) control=5;
    if(ClientMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Vector2 mouse=ClientPointerPosition();
        if(CheckCollisionPointRec(mouse,MineControl(0))) control=4;
        else if(CheckCollisionPointRec(mouse,MineControl(1))) control=5;
        else {
            int32_t tx=0,ty=0;
            if(MinePickTile(sim,local,target,mouse,&tx,&ty) &&
               CcMineWalkable(sim,sim->mine.phase,tx,ty)) {
                local->mine_target_x=tx; local->mine_target_y=ty;
            }
        }
    }
    int32_t dir=-1;
    bool relative=false;
    if(IsKeyDown(KEY_W)||ClientKeyPressed(KEY_W)||IsKeyDown(KEY_UP)||ClientKeyPressed(KEY_UP)) {dir=0;relative=true;}
    if(IsKeyDown(KEY_D)||ClientKeyPressed(KEY_D)||IsKeyDown(KEY_RIGHT)||ClientKeyPressed(KEY_RIGHT)) {dir=1;relative=true;}
    if(IsKeyDown(KEY_S)||ClientKeyPressed(KEY_S)||IsKeyDown(KEY_DOWN)||ClientKeyPressed(KEY_DOWN)) {dir=2;relative=true;}
    if(IsKeyDown(KEY_A)||ClientKeyPressed(KEY_A)||IsKeyDown(KEY_LEFT)||ClientKeyPressed(KEY_LEFT)) {dir=3;relative=true;}
    if(dir>=0||control>=0) {local->mine_target_x=-1;local->mine_target_y=-1;}
    if(dir<0&&local->mine_target_x>=0) {
        if(sim->mine.x==local->mine_target_x && sim->mine.y==local->mine_target_y) {
            local->mine_target_x=-1; local->mine_target_y=-1;
            if(CcMineAction(sim)!=NULL) control=4;
        } else {
            dir=MinePathStep(sim,local->mine_target_x,local->mine_target_y);
            if(dir<0) {local->mine_target_x=-1;local->mine_target_y=-1;}
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
            (void)snprintf(message,capacity,"%s",command.kind==CC_COMMAND_MINE_PACK?"Food moved from carriage to pack.":"");
        } else {local->mine_target_x=-1;local->mine_target_y=-1;}
    }
}
