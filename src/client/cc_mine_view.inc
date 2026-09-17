static Rectangle MineViewport(void)
{
    return (Rectangle){0,82,(float)GetScreenWidth(),(float)GetScreenHeight()-246};
}
static Rectangle MineControl(int32_t index)
{
    float button_width=((float)GetScreenWidth()-56.0f)/6.0f;
    return (Rectangle){18.0f+(float)index*(button_width+4.0f),
        (float)GetScreenHeight()-78,button_width,34};
}
static void SyncMineView(const CcSim *sim, LocalState *local)
{
    if(local->mine_view_phase==sim->mine.phase) return;
    local->mine_facing=sim->mine.phase==CC_MINE_LEVEL?1:0;
    local->mine_view_phase=sim->mine.phase;
    local->mine_cooldown=.18f;
}
static void DrawMineScene(const CcSim *sim, LocalState *local, RenderTexture2D target, const char *message)
{
    SyncMineView(sim,local);
    bool underground=sim->mine.phase==CC_MINE_LEVEL;
    DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),BACKGROUND);
    CcLocalDrawMine3D(sim,&local->agent,local->mine_facing,target,MineViewport());
    CcOverlayDrawText(underground?"THE UNDERROAD / 01":"LOW SILVER PIT / MINE YARD",22,20,11,TEAL);
    CcOverlayDrawText(underground?CcMineChamberName(CcMineChamber(sim->mine.x,sim->mine.y)):"The carriage waits beside the mine road",22,43,20,INK);
    const char *const facing[]={"NORTH","EAST","SOUTH","WEST"};
    CcOverlayDrawText(TextFormat("PACK %d/8  /  LIGHT %d  /  %s",CcMinePackUsed(sim),sim->mine.light,
        underground?facing[local->mine_facing]:"NORTH / MINE     SOUTH / CARRIAGE"),22,GetScreenHeight()-151,11,INK);
    const char *action=CcMineAction(sim);
    CcOverlayDrawText(action!=NULL?action:underground?"Explore the passages. The workers' records lie east.":"Pack food, then walk north. Push forward at the mouth to enter.",22,GetScreenHeight()-126,11,TEAL);
    const char *const yard[]={"North / W","West / A","South / S","East / D","Use / E","Pack / P"};
    const char *const level[]={"Forward / W","Turn left / A","Back / S","Turn right / D","Use / E","Pack / P"};
    for(int32_t i=0;i<6;++i) AdventureButton(MineControl(i),underground?level[i]:yard[i],i!=5||!underground,false);
    CcOverlayDrawText(message,22,GetScreenHeight()-29,10,INK);
    CcOverlayDrawText("Buttons / WASD / arrows  |  F5 save",GetScreenWidth()-300,23,10,MUTED);
}
static bool MineForwardUse(const CcSim *sim,int32_t dir)
{
    const CcMineVisit *m=&sim->mine;
    if(m->phase==CC_MINE_YARD)
        return (dir==0 && m->x==15 && (m->y==4||m->y==3)) ||
            (dir==2 && m->x==15 && (m->y==17||m->y==18));
    return dir==0 && m->x==5 && (m->y==4||m->y==3);
}
static void HandleMineInput(CcJournal *journal,CcSim *sim,LocalState *local,float dt,char *message,size_t capacity)
{
    SyncMineView(sim,local);
    local->mine_cooldown-=dt;
    int32_t control=-1;
    if(ClientKeyPressed(KEY_E)) control=4;
    if(ClientKeyPressed(KEY_P)) control=5;
    if(ClientMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Vector2 mouse=ClientPointerPosition();
        for(int32_t i=0;i<6;++i) if(CheckCollisionPointRec(mouse,MineControl(i))) control=i;
    }
    int32_t dir=-1;
    if(IsKeyDown(KEY_W)||ClientKeyPressed(KEY_W)||IsKeyDown(KEY_UP)||ClientKeyPressed(KEY_UP)||control==0) dir=0;
    if(IsKeyDown(KEY_D)||ClientKeyPressed(KEY_D)||IsKeyDown(KEY_RIGHT)||ClientKeyPressed(KEY_RIGHT)||control==3) dir=1;
    if(IsKeyDown(KEY_S)||ClientKeyPressed(KEY_S)||IsKeyDown(KEY_DOWN)||ClientKeyPressed(KEY_DOWN)||control==2) dir=2;
    if(IsKeyDown(KEY_A)||ClientKeyPressed(KEY_A)||IsKeyDown(KEY_LEFT)||ClientKeyPressed(KEY_LEFT)||control==1) dir=3;
    CcCommand command={.target_id=(CcId)sim->mine.revision};
    if(control==4) command.kind=CC_COMMAND_MINE_USE;
    else if(control==5) {
        command.kind=CC_COMMAND_MINE_PACK; command.amount=1;
        command.good=sim->player.cargo[CC_GOOD_BREAD]>0?CC_GOOD_BREAD:CC_GOOD_MEAT;
    } else if(dir>=0&&local->mine_cooldown<=0) {
        local->mine_cooldown=.18f;
        bool forward=dir==0;
        if(sim->mine.phase==CC_MINE_LEVEL) {
            if(dir==1||dir==3) {
                local->mine_facing=(local->mine_facing+dir)%4;
                return;
            }
            dir=(local->mine_facing+dir)%4;
        } else forward=true;
        command.kind=forward&&MineForwardUse(sim,dir)?CC_COMMAND_MINE_USE:CC_COMMAND_MINE_STEP;
        command.amount=dir;
    }
    if(command.kind!=CC_COMMAND_NONE) {
        bool ok=ApplyCommand(journal,sim,command,message,capacity);
        if(ok) {
            SyncMineView(sim,local);
            (void)snprintf(message,capacity,"%s",command.kind==CC_COMMAND_MINE_PACK?"Food moved from carriage to pack.":"");
        }
    }
}
