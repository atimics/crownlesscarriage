static Rectangle MineViewport(void)
{
    return (Rectangle){0,82,(float)GetScreenWidth(),(float)GetScreenHeight()-246};
}
static Rectangle MineControl(int32_t index)
{
    float width=(float)GetScreenWidth();
    float button_width=(width-56.0f)/6.0f;
    return (Rectangle){18.0f+(float)index*(button_width+4.0f),
        (float)GetScreenHeight()-78,button_width,34};
}
static void MineLabel(RenderTexture2D target, const CcSim *sim, Vector3 point, const char *text)
{
    Vector2 pixel=GetWorldToScreenEx(point,CcLocalMineCamera(sim),target.texture.width,target.texture.height);
    Rectangle view=MineViewport();
    int x=(int)(view.x+pixel.x*view.width/(float)target.texture.width);
    int y=(int)(view.y+pixel.y*view.height/(float)target.texture.height);
    if(x<view.x+80 || x>view.x+view.width-80 || y<view.y+12 || y>view.y+view.height-20) return;
    int width=CcOverlayMeasureText(text,10);
    DrawRectangle(x-width/2-5,y-4,width+10,18,Fade(BACKGROUND,.88f));
    CcOverlayDrawText(text,x-width/2,y,10,INK);
}
static void DrawMineScene(const CcSim *sim, const CcLocalAgent *agent, RenderTexture2D target, const char *message)
{
    bool overhead=sim->mine.phase==CC_MINE_LEVEL;
    Rectangle dest=MineViewport();
    DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),BACKGROUND);
    CcLocalDrawMine3D(sim,agent,target,dest);
    if(overhead) {
        MineLabel(target,sim,(Vector3){5,0,1.5f},"Yard exit");
        MineLabel(target,sim,(Vector3){26,0,1.5f},"Workers' survey");
        MineLabel(target,sim,(Vector3){26,0,19},"Lower stair");
    } else {
        MineLabel(target,sim,(Vector3){15,3,3},"Mine Mouth / enter on foot");
        MineLabel(target,sim,(Vector3){18,0,21},"Carriage / pack food");
    }
    CcOverlayDrawText(overhead?"THE UNDERROAD / 01":"LOW SILVER PIT / MINE YARD",22,20,11,TEAL);
    CcOverlayDrawText(overhead?CcMineChamberName(CcMineChamber(sim->mine.x,sim->mine.y)):"The carriage waits beside the mine road",22,43,20,INK);
    CcOverlayDrawText(TextFormat("PACK %d/8  /  LIGHT %d  /  %s",CcMinePackUsed(sim),sim->mine.light,
        sim->mine.surveyed?"SURVEY READ":"FIND THE WORKERS' SURVEY"),22,GetScreenHeight()-151,11,INK);
    const char *action=CcMineAction(sim);
    CcOverlayDrawText(action!=NULL?action:overhead?"Walk the passages. The eastern room holds the workers' records.":"Pack food at the carriage, then walk north to the mine mouth.",22,GetScreenHeight()-126,11,TEAL);
    const char *const controls[]={"North / W","West / A","South / S","East / D","Use / E","Pack food / P"};
    for(int32_t i=0;i<6;++i) AdventureButton(MineControl(i),controls[i],true,false);
    CcOverlayDrawText(message,22,GetScreenHeight()-29,10,INK);
    CcOverlayDrawText("Click ground to walk  /  F5 save",GetScreenWidth()-275,23,10,MUTED);
}
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
static void HandleMineInput(CcJournal *journal,CcSim *sim,RenderTexture2D target,float dt,char *message,size_t capacity)
{
    static float cooldown=0;
    static int32_t target_x=-1,target_y=-1;
    static CcMinePhase last_phase=CC_MINE_NONE;
    if(last_phase!=sim->mine.phase) {target_x=-1;target_y=-1;last_phase=sim->mine.phase;}
    cooldown-=dt;
    int32_t control=-1;
    if(ClientKeyPressed(KEY_E)) control=4;
    if(ClientKeyPressed(KEY_P)) control=5;
    if(ClientMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Vector2 mouse=ClientPointerPosition();
        for(int32_t i=0;i<6;++i) if(CheckCollisionPointRec(mouse,MineControl(i))) control=i;
        Rectangle dest=MineViewport();
        if(CheckCollisionPointRec(mouse,dest)) {
            Vector2 pixel={(mouse.x-dest.x)*(float)target.texture.width/dest.width,
                           (mouse.y-dest.y)*(float)target.texture.height/dest.height};
            Ray ray=GetScreenToWorldRayEx(pixel,CcLocalMineCamera(sim),target.texture.width,target.texture.height);
            if(fabsf(ray.direction.y)>.0001f) {
                float distance=-ray.position.y/ray.direction.y;
                target_x=(int32_t)roundf(ray.position.x+distance*ray.direction.x);
                target_y=(int32_t)roundf(ray.position.z+distance*ray.direction.z);
            }
        }
    }
    int32_t dir=-1;
    if((IsKeyDown(KEY_W)||ClientKeyPressed(KEY_W))||(IsKeyDown(KEY_UP)||ClientKeyPressed(KEY_UP))||control==0) dir=0;
    if((IsKeyDown(KEY_D)||ClientKeyPressed(KEY_D))||(IsKeyDown(KEY_RIGHT)||ClientKeyPressed(KEY_RIGHT))||control==3) dir=1;
    if((IsKeyDown(KEY_S)||ClientKeyPressed(KEY_S))||(IsKeyDown(KEY_DOWN)||ClientKeyPressed(KEY_DOWN))||control==2) dir=2;
    if((IsKeyDown(KEY_A)||ClientKeyPressed(KEY_A))||(IsKeyDown(KEY_LEFT)||ClientKeyPressed(KEY_LEFT))||control==1) dir=3;
    if(dir>=0||control>=0) {target_x=-1;target_y=-1;}
    if(dir<0&&target_x>=0) dir=MinePathStep(sim,target_x,target_y);
    if(dir<0&&target_x>=0) {target_x=-1;target_y=-1;}
    CcCommand command={.target_id=(CcId)sim->mine.revision};
    if(control==4) command.kind=CC_COMMAND_MINE_USE;
    else if(control==5) {
        command.kind=CC_COMMAND_MINE_PACK; command.amount=1;
        command.good=sim->player.cargo[CC_GOOD_BREAD]>0?CC_GOOD_BREAD:CC_GOOD_MEAT;
    } else if(dir>=0&&cooldown<=0) {command.kind=CC_COMMAND_MINE_STEP;command.amount=dir;cooldown=.14f;}
    if(command.kind!=CC_COMMAND_NONE) {
        bool ok=ApplyCommand(journal,sim,command,message,capacity);
        if(ok) (void)snprintf(message,capacity,"%s",command.kind==CC_COMMAND_MINE_PACK?"Food moved from carriage to pack.":
            command.kind==CC_COMMAND_MINE_STEP?"":sim->mine.surveyed?"The survey marks the lower workings beyond Lamp Hall.":"The company follows the passage.");
    }
}
