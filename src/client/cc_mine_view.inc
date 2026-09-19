static int MineScreenWidth(void) { return GetScreenWidth()>0?GetScreenWidth():1040; }
static int MineScreenHeight(void) { return GetScreenHeight()>0?GetScreenHeight():620; }
static bool MineShortLandscape(void)
{
    return MineScreenWidth()>=700 && MineScreenHeight()<500;
}
static Rectangle MineViewportForSize(int32_t width,int32_t height)
{
    float top=width>=700&&height<500?145.0f:230.0f;
    float bottom=(float)height-176.0f;
    return (Rectangle){0,top,(float)width,bottom-top};
}
static Rectangle MineViewportForInventory(int32_t width,int32_t height,bool expanded)
{
    Rectangle view=MineViewportForSize(width,height);
    if(width>=700&&height<500&&!expanded) {
        view.y=94.0f;
        view.height=(float)height-176.0f-view.y;
    }
    float source_aspect=(float)CC_LOCAL_ART_WIDTH/(float)CC_LOCAL_ART_HEIGHT;
    float view_aspect=view.width/view.height;
    if(view_aspect>source_aspect) {
        float fitted=view.height*source_aspect;
        view.x+=(view.width-fitted)*.5f; view.width=fitted;
    } else {
        float fitted=view.width/source_aspect;
        view.y+=(view.height-fitted)*.5f; view.height=fitted;
    }
    return view;
}
static Rectangle MineViewport(const LocalState *local)
{
    int32_t width=GetScreenWidth(),height=GetScreenHeight();
    if(width<=0||height<=0) return (Rectangle){0};
    return MineViewportForInventory(width,height,local!=NULL&&local->mine_inventory_expanded);
}
static Rectangle MineControl(int32_t index)
{
    float button_width=((float)MineScreenWidth()-56.0f)/3.0f;
    return (Rectangle){18.0f+(float)index*(button_width+4.0f),
        (float)MineScreenHeight()-78,button_width,34};
}
static Rectangle MineSelectionControl(int32_t index)
{
    float button_width=((float)MineScreenWidth()-56.0f)/4.0f;
    return (Rectangle){18.0f+(float)index*(button_width+4.0f),
        (float)MineScreenHeight()-126,button_width,30};
}
static Rectangle MineInventoryControl(void)
{
    return (Rectangle){18.0f, MineShortLandscape() ? 63.0f : 94.0f,
        (float)MineScreenWidth() - 36.0f, 24.0f};
}
static bool MineAtCarriage(const CcSim *sim)
{
    return sim->mine.phase==CC_MINE_YARD &&
        abs(sim->mine.x-15)+abs(sim->mine.y-18)<=1;
}
static int32_t MineGoodsRows(const int32_t goods[CC_GOOD_COUNT])
{
    int32_t rows=0;
    for(int32_t good=0;good<CC_GOOD_COUNT;++good) {
        if(goods[good]>0) ++rows;
    }
    return rows>0?rows:1;
}
static void MineDrawGoods(const char *title,const int32_t goods[CC_GOOD_COUNT],
                          int32_t x,int32_t y,int32_t width,Color color)
{
    CcOverlayDrawText(title,x,y,10,color);
    if(goods==NULL) {
        CcOverlayDrawText("Walk to yard anchor.",x,y+13,10,MUTED);
        return;
    }
    int32_t columns=MineShortLandscape()?2:1;
    int32_t rows=(MineGoodsRows(goods)+columns-1)/columns;
    int32_t item=0;
    for(int32_t good=0;good<CC_GOOD_COUNT;++good) {
        if(goods[good]<=0) continue;
        int32_t column=item/rows,row=item%rows;
        CcOverlayDrawText(TextFormat("%s %d",CcGoodName((CcGood)good),goods[good]),
            x+column*(width/columns),y+13+row*11,10,color);
        ++item;
    }
    if(item==0) CcOverlayDrawText("empty",x,y+13,10,MUTED);
}
static void MineReceipt(char *message,size_t capacity,const CcCommand *command)
{
    const char *good=CcGoodName(command->good);
    int32_t amount=command->amount<0?-command->amount:command->amount;
    if(command->kind==CC_COMMAND_MINE_PACK) {
        (void)snprintf(message,capacity,"%d %s moved %s.",amount,good,
            command->amount>0?"from carriage to pack":"from pack to carriage");
    }
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
static float MineOccluderDistance(const CcSim *sim,Ray ray,float limit)
{
    return MineOccluderDistanceExcept(sim,ray,limit,-1,-1);
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
        *out_box=MineBoxBounds(15,1.1f,2.9f,2,2.2f,.2f); *out_x=15;*out_y=3; return true;
    case MINE_INTENT_CARRIAGE:
        if(sim->mine.phase!=CC_MINE_YARD) return false;
        *out_box=MineBoxBounds(18,1,19,3,2,4); *out_x=15;*out_y=18; return true;
    case MINE_INTENT_EXIT:
        if(sim->mine.phase!=CC_MINE_LEVEL) return false;
        *out_box=MineBoxBounds(5,.25f,3,.85f,.5f,.85f); *out_x=5;*out_y=3; return true;
    case MINE_INTENT_BAR:
        if(sim->mine.phase!=CC_MINE_LEVEL||sim->mine.bar_open) return false;
        *out_box=MineClosedBarBounds(); *out_x=15;*out_y=10; return true;
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
    Rectangle view=MineViewport(local);
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
        float hit_distance=0.0f;
        int32_t skip_x=candidate==MINE_INTENT_BAR?15:-1;
        int32_t skip_y=candidate==MINE_INTENT_BAR?10:-1;
        bool hit=candidate==MINE_INTENT_BAR?
            MineClosedBarDistance(ray,&hit_distance):MineRayBoxDistance(ray,box,&hit_distance);
        if(hit && hit_distance<nearest&&
           MineOccluderDistanceExcept(sim,ray,hit_distance,skip_x,skip_y)>=hit_distance) {
            nearest=hit_distance; picked=(MineIntentTarget)candidate; *out_x=x; *out_y=y;
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
    Rectangle view=MineViewport(local);
    if((view.width<=0||view.height<=0) && target.texture.width>0&&target.texture.height>0)
        view=(Rectangle){0,0,(float)target.texture.width,(float)target.texture.height};
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
static void DrawMineScene(const CcSim *sim, LocalState *local, RenderTexture2D target, const char *message)
{
    SyncMineView(sim,local);
    bool underground=sim->mine.phase==CC_MINE_LEVEL;
    bool compact=MineScreenWidth()<600;
    bool short_landscape=MineShortLandscape();
    bool brief=compact||short_landscape;
    DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),BACKGROUND);
    CcLocalDrawMine3D(sim,&local->agent,local->mine_facing,target,MineViewport(local));
    if(underground) {
        if(!sim->mine.bar_open) MineLabel(target,sim,local,(Vector3){15,1,10},15,10,"Wooden bar / lift it");
        if(!sim->mine.surveyed) MineLabel(target,sim,local,(Vector3){26,.6f,4},26,4,"Workers' survey");
        MineLabel(target,sim,local,(Vector3){5,.6f,3},5,3,"Back to the yard");
    } else {
        MineLabel(target,sim,local,(Vector3){15,3,3},15,3,"Mine Mouth / walk in");
        MineLabel(target,sim,local,(Vector3){18,1,19},18,19,"Carriage / transfer goods");
    }
    int32_t title_y=short_landscape?12:20;
    int32_t subtitle_y=short_landscape?30:43;
    int32_t summary_y=short_landscape?50:65;
    int32_t goods_y=short_landscape?94:80;
    CcOverlayDrawText(underground?"THE UNDERROAD / 01":brief?"MINE YARD":"LOW SILVER PIT / MINE YARD",22,title_y,11,TEAL);
    CcOverlayDrawText(underground?CcMineChamberName(CcMineChamber(sim->mine.x,sim->mine.y)):
        brief?"Carriage at mine road":"The carriage waits beside the mine road",22,subtitle_y,brief?14:20,INK);
    int32_t pack_used=CcMinePackUsed(sim);
    int32_t carriage_used=CcPlayerCargoUsed(&sim->player);
    bool at_carriage=MineAtCarriage(sim);
    int32_t pack_goods[CC_GOOD_COUNT];
    for(int32_t good=0;good<CC_GOOD_COUNT;++good)
        pack_goods[good]=CcMinePackGood(sim,(CcGood)good);
    if(brief) CcOverlayDrawText(TextFormat("Pack %d/8 | Cart %d/%d | Light %d",
        pack_used,carriage_used,sim->player.cargo_capacity,sim->mine.light),22,summary_y,10,INK);
    else CcOverlayDrawText(TextFormat("Pack %d/8, free %d  |  Carriage %d/%d, free %d  |  Light %d",
        pack_used,CC_MINE_PACK_CAPACITY-pack_used,carriage_used,sim->player.cargo_capacity,
        sim->player.cargo_capacity-carriage_used,sim->mine.light),22,summary_y,10,INK);
    int32_t pane_width=MineScreenWidth()/2-34;
    if (short_landscape) AdventureButton(MineInventoryControl(),
        local->mine_inventory_expanded ? "Hide inventory" : "Show inventory",
        true, local->mine_inventory_expanded);
    if (!short_landscape || local->mine_inventory_expanded) {
        MineDrawGoods("Pack",pack_goods,22,goods_y,pane_width,TEAL);
        MineDrawGoods(at_carriage?(brief?"Cart":"Carriage"):(brief?"Cart out of reach":"Carriage out of reach"),
            at_carriage?sim->player.cargo:NULL,MineScreenWidth()/2+12,goods_y,pane_width,
            at_carriage?TEAL:MUTED);
    }
    const char *selection=at_carriage?
        TextFormat(brief?"Selected %s x%d | pack %d | cart %d":
            "Selected %s x%d  |  pack %d  carriage %d  |  good [ ]  quantity - +",
            CcGoodName(local->mine_good),local->mine_quantity,pack_goods[local->mine_good],
            sim->player.cargo[local->mine_good]):
        TextFormat(brief?"Selected %s x%d | pack %d | cart out of reach":
            "Selected %s x%d  |  pack %d  |  carriage out of reach  |  good [ ]  quantity - +",
            CcGoodName(local->mine_good),local->mine_quantity,pack_goods[local->mine_good]);
    CcOverlayDrawText(selection,22,GetScreenHeight()-176,11,INK);
    const char *action=CcMineAction(sim);
    bool can_board=!at_carriage || carriage_used+pack_used<=sim->player.cargo_capacity;
    bool can_pack=at_carriage && sim->player.cargo[local->mine_good]>=local->mine_quantity &&
        pack_used+local->mine_quantity<=CC_MINE_PACK_CAPACITY;
    bool can_unload=at_carriage && pack_goods[local->mine_good]>=local->mine_quantity &&
        carriage_used+local->mine_quantity<=sim->player.cargo_capacity;
    const char *hint=action!=NULL?action:underground?
        "Inspect your pack or point where the company should walk.":
        "Walk to the carriage to transfer selected goods.";
    if(at_carriage && carriage_used+pack_used>sim->player.cargo_capacity) {
        hint=carriage_used==sim->player.cargo_capacity?
            (brief?"Cart full. Keep the pack or enter the mine.":
                "Carriage has no free space. Keep the pack and walk to the mine mouth."):
            (brief?"Choose goods that fit, then unload or enter.":
                "Choose a packed good and amount that fits, then unload or walk to the mine mouth.");
    } else if(at_carriage && pack_used==CC_MINE_PACK_CAPACITY)
        hint=brief?"Pack full. Unload goods or enter the mine.":
            "Pack is full. Unload selected goods, or walk to the mine mouth.";
    CcOverlayDrawText(hint,22,GetScreenHeight()-151,11,TEAL);
    AdventureButton(MineSelectionControl(0),brief?"Good <":"Previous good",true,false);
    AdventureButton(MineSelectionControl(1),brief?"Good >":"Next good",true,false);
    AdventureButton(MineSelectionControl(2),"Less",local->mine_quantity>1,false);
    AdventureButton(MineSelectionControl(3),"More",local->mine_quantity<CC_MINE_PACK_CAPACITY,false);
    AdventureButton(MineControl(0),brief?"Use / E":action!=NULL?action:"Use / E",action!=NULL&&can_board,false);
    AdventureButton(MineControl(1),TextFormat(brief?"Pack %d":"Pack %d / P",local->mine_quantity),can_pack,false);
    AdventureButton(MineControl(2),TextFormat(brief?"Unload %d":"Unload %d / U",local->mine_quantity),can_unload,false);
    CcOverlayDrawText(message,22,GetScreenHeight()-29,10,INK);
    if(!compact) CcOverlayDrawText("Click where to walk  |  [ ] good  - + amount  |  F5 save",GetScreenWidth()-390,23,10,MUTED);
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
        int32_t step=-1,distance=-1;
        if(MinePath(sim,local,cx,cy,&step,&distance) && distance<best_distance) {
            best=MineCell(cx,cy);best_distance=distance;
        }
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
    int32_t control=-1,good_change=0,quantity_change=0;
    if(ClientKeyPressed(KEY_E)) control=4;
    if(ClientKeyPressed(KEY_P)) control=5;
    if(ClientKeyPressed(KEY_U)) control=6;
    if(ClientKeyPressed(KEY_I) && MineShortLandscape())
        local->mine_inventory_expanded=!local->mine_inventory_expanded;
    if(ClientKeyPressed(KEY_LEFT_BRACKET)) good_change=-1;
    if(ClientKeyPressed(KEY_RIGHT_BRACKET)) good_change=1;
    if(ClientKeyPressed(KEY_MINUS)) quantity_change=-1;
    if(ClientKeyPressed(KEY_EQUAL)) quantity_change=1;
    if(ClientMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Vector2 mouse=ClientPointerPosition();
        if(CheckCollisionPointRec(mouse,MineInventoryControl()) && MineShortLandscape())
            local->mine_inventory_expanded=!local->mine_inventory_expanded;
        else if(CheckCollisionPointRec(mouse,MineControl(0))) control=4;
        else if(CheckCollisionPointRec(mouse,MineControl(1))) control=5;
        else if(CheckCollisionPointRec(mouse,MineControl(2))) control=6;
        else if(CheckCollisionPointRec(mouse,MineSelectionControl(0))) good_change=-1;
        else if(CheckCollisionPointRec(mouse,MineSelectionControl(1))) good_change=1;
        else if(CheckCollisionPointRec(mouse,MineSelectionControl(2))) quantity_change=-1;
        else if(CheckCollisionPointRec(mouse,MineSelectionControl(3))) quantity_change=1;
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
    if(good_change!=0)
        local->mine_good=(CcGood)(((int32_t)local->mine_good+CC_GOOD_COUNT+good_change)%CC_GOOD_COUNT);
    if(quantity_change<0 && local->mine_quantity>1) --local->mine_quantity;
    if(quantity_change>0 && local->mine_quantity<CC_MINE_PACK_CAPACITY) ++local->mine_quantity;
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
        command.kind=CC_COMMAND_MINE_PACK; command.amount=local->mine_quantity;
        command.good=local->mine_good;
    } else if(control==6) {
        command.kind=CC_COMMAND_MINE_PACK; command.amount=-local->mine_quantity;
        command.good=local->mine_good;
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
    bool stowing=command.kind==CC_COMMAND_MINE_USE && MineAtCarriage(sim);
    int32_t stowed=stowing?CcMinePackUsed(sim):0;
    if(command.kind!=CC_COMMAND_NONE) {
        bool ok=ApplyCommand(journal,sim,command,message,capacity);
        if(ok) {
            SyncMineView(sim,local);
            if(local->mine_target_x>=0) local->mine_intent_revision=sim->mine.revision;
            if(command.kind==CC_COMMAND_MINE_PACK) MineReceipt(message,capacity,&command);
            else if(stowing) (void)snprintf(message,capacity,
                "Stowed %d goods in the carriage and boarded for the road.",stowed);
        } else MineClearIntent(local);
    }
}
