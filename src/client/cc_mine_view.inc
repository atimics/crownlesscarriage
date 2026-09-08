static Rectangle MineViewport(void)
{
    return (Rectangle){0,82,(float)GetScreenWidth(),(float)GetScreenHeight()-246};
}
static Camera3D MineCamera(bool overhead)
{
    return (Camera3D){
        .position=overhead?(Vector3){15,42,10}:(Vector3){37,34,43},
        .target={15,0,10}, .up=overhead?(Vector3){0,0,-1}:(Vector3){0,1,0},
        .fovy=overhead?25.0f:34.0f, .projection=CAMERA_ORTHOGRAPHIC
    };
}
static Rectangle MineControl(int32_t index)
{
    float width=(float)GetScreenWidth();
    float button_width=(width-56.0f)/6.0f;
    return (Rectangle){18.0f+(float)index*(button_width+4.0f),
        (float)GetScreenHeight()-78,button_width,34};
}
static void MineBox(float x,float y,float z,float w,float h,float d,Color color)
{
    DrawCube((Vector3){x,y,z},w,h,d,color);
}
static void MineLabel(RenderTexture2D target, bool overhead, Vector3 point, const char *text)
{
    Vector2 pixel=GetWorldToScreenEx(point,MineCamera(overhead),target.texture.width,target.texture.height);
    Rectangle view=MineViewport();
    int x=(int)(view.x+pixel.x*view.width/(float)target.texture.width);
    int y=(int)(view.y+pixel.y*view.height/(float)target.texture.height);
    int width=CcOverlayMeasureText(text,10);
    DrawRectangle(x-width/2-5,y-4,width+10,18,Fade(BACKGROUND,.88f));
    CcOverlayDrawText(text,x-width/2,y,10,INK);
}
static void DrawMineScene(const CcSim *sim, RenderTexture2D target, const char *message)
{
    bool overhead=sim->mine.phase==CC_MINE_LEVEL;
    const Color stone={74,83,84,255}, floor={187,178,151,255};
    BeginTextureMode(target);
    ClearBackground(overhead?(Color){26,34,38,255}:(Color){174,197,183,255});
    BeginMode3D(MineCamera(overhead));
    if (overhead) {
        for (int y=1;y<CC_MINE_HEIGHT-1;++y) for(int x=1;x<CC_MINE_WIDTH-1;++x) {
            bool walk=CcMineWalkable(sim,CC_MINE_LEVEL,x,y);
            if(walk) {
                Color tint=((x+y)%2)?floor:(Color){179,171,145,255};
                MineBox((float)x,0,(float)y,.98f,.12f,.98f,tint);
            } else {
                bool edge=CcMineWalkable(sim,CC_MINE_LEVEL,x+1,y)||CcMineWalkable(sim,CC_MINE_LEVEL,x-1,y)||
                    CcMineWalkable(sim,CC_MINE_LEVEL,x,y+1)||CcMineWalkable(sim,CC_MINE_LEVEL,x,y-1);
                if(edge) MineBox((float)x,.45f,(float)y,1,.9f,1,stone);
            }
        }
        MineBox(5,.12f,3,.85f,.15f,.85f,(Color){94,153,140,255});
        MineBox(26,.18f,4,.85f,.3f,.65f,(Color){121,75,45,255});
        MineBox(26,.37f,4,.6f,.06f,.45f,sim->mine.surveyed?TEAL:(Color){248,223,153,255});
        if(!sim->mine.bar_open) MineBox(15,.85f,10,1.3f,.2f,.3f,(Color){151,84,44,255});
        for(int i=0;i<3;++i) MineBox(25.4f+(float)i*.6f,.23f,16,.55f,.46f,1.2f,stone);
        for(int i=0;i<4;++i) MineBox(3.5f+(float)i*.6f,.2f,16,.4f,.4f,.7f,(Color){133,111,74,255});
    } else {
        MineBox(15,-.18f,10,33,.3f,23,(Color){119,144,112,255});
        MineBox(15,.01f,11,5,.05f,19,(Color){183,168,133,255});
        MineBox(15,.02f,10,22,.05f,3,(Color){183,168,133,255});
        for(int row=0;row<2;++row) for(int col=0;col<2;++col) {
            float x=col==0?5.5f:24.5f,z=row==0?6.5f:12.5f;
            MineBox(x,1.15f,z,6,2.3f,4,(Color){199,183,144,255});
            MineBox(x,2.5f,z,6.7f,.7f,4.7f,col==0?(Color){85,102,95,255}:(Color){130,84,67,255});
            MineBox(x,.85f,z+2.02f,1,1.7f,.08f,(Color){76,66,52,255});
            MineBox(x-1.8f,1.25f,z+2.04f,.7f,.8f,.1f,(Color){220,184,103,255});
        }
        for(int i=0;i<11;++i) {
            float x=1+(float)i*2.8f;
            MineBox(x,1.6f,1,3,3.2f,3,stone);
        }
        MineBox(15,1.1f,2.9f,2,2.2f,.2f,(Color){24,32,35,255});
        MineBox(13.7f,1.2f,3, .35f,2.4f,.5f,(Color){125,88,53,255});
        MineBox(16.3f,1.2f,3, .35f,2.4f,.5f,(Color){125,88,53,255});
        MineBox(15,2.4f,3,3,.4f,.6f,(Color){125,88,53,255});
        MineBox(15,.7f,18,2.6f,1.2f,1.4f,(Color){109,66,45,255});
        MineBox(15,1.55f,18,2.8f,.5f,1.6f,(Color){220,205,161,255});
        for(int i=-1;i<=1;i+=2) {
            DrawCylinderEx((Vector3){13.6f,.55f,18+(float)i*.65f},(Vector3){16.4f,.55f,18+(float)i*.65f},.18f,.18f,8,stone);
            MineBox(15+(float)i*.65f,.65f,19.2f,.5f,1.1f,1.3f,(Color){101,76,54,255});
        }
        for(int i=0;i<5;++i) {
            float x=1+(float)i*7;
            MineBox(x,.8f,18,.18f,1.6f,.18f,(Color){98,79,50,255});
            if(i<4) MineBox(x+3.5f,1.1f,18,7,.12f,.12f,(Color){116,98,64,255});
        }
    }
    float px=(float)sim->mine.x,py=(float)sim->mine.y;
    DrawCircle3D((Vector3){px,.12f,py},.48f,(Vector3){1,0,0},90,CC_GOLD);
    MineBox(px,.55f,py,.48f,.9f,.4f,(Color){39,108,116,255});
    DrawSphere((Vector3){px,1.18f,py},.23f,(Color){228,182,136,255});
    EndMode3D();
    EndTextureMode();
    Rectangle dest=MineViewport();
    DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),BACKGROUND);
    DrawTexturePro(target.texture,(Rectangle){0,0,(float)target.texture.width,-(float)target.texture.height},dest,(Vector2){0,0},0,WHITE);
    if(overhead) {
        MineLabel(target,true,(Vector3){5,0,1.5f},"Yard exit");
        MineLabel(target,true,(Vector3){26,0,1.5f},"Workers' survey");
        MineLabel(target,true,(Vector3){26,0,19},"Lower stair");
    } else {
        MineLabel(target,false,(Vector3){15,3,3},"Mine Mouth / enter on foot");
        MineLabel(target,false,(Vector3){15,0,20.5f},"Carriage / pack food");
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
            Ray ray=GetScreenToWorldRayEx(pixel,MineCamera(sim->mine.phase==CC_MINE_LEVEL),target.texture.width,target.texture.height);
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
