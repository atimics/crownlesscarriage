#include "sim/cc_mine.h"

static Camera3D MineYardCamera(Vector3 position)
{
    Vector3 focus={position.x,position.y+.6f,position.z};
    return PerspectiveCameraComposed(focus,(Vector3){9.0f,9.0f,3.0f},48.0f);
}
static Camera3D MineCombatBaseCamera(Vector3 position,int32_t facing)
{
    const int32_t dx[]={0,1,0,-1},dy[]={-1,0,1,0};
    facing=((facing%4)+4)%4;
    Vector3 focus={position.x+.6f*(float)dx[facing],
                   position.y+1.05f,
                   position.z+.6f*(float)dy[facing]};
    Vector3 offset={-2.2f*(float)dx[facing],2.7f,
                    -2.2f*(float)dy[facing]};
    return PerspectiveCameraComposed(focus,offset,62.0f);
}

Camera3D CcLocalMineCamera(const CcSim *sim, int32_t facing)
{
    if(sim->mine.phase!=CC_MINE_LEVEL)
        return MineYardCamera((Vector3){(float)sim->mine.x,0.0f,
                                       (float)sim->mine.y});
    /* Each chamber keeps its own eye level and lens. Movement still uses the
       saved mine tile, so a new view always has a matching place to click. */
    static const struct {float eye,pitch,fovy,back;} shots[6]={
        {1.52f,-.30f,69.0f,0.0f}, {1.45f,-.24f,66.0f,0.0f},
        {1.42f,-.28f,68.0f,0.0f}, {1.55f,-.34f,72.0f,0.0f},
        {1.42f,-.27f,67.0f,0.0f}, {1.50f,-.25f,80.0f,1.2f}
    };
    int32_t chamber=CcMineChamber(sim->mine.x,sim->mine.y);
    float eye=chamber>=0?shots[chamber].eye:1.45f;
    float pitch=chamber>=0?shots[chamber].pitch:-.30f;
    float fovy=chamber>=0?shots[chamber].fovy:68.0f;
    float back=chamber>=0?shots[chamber].back:0.0f;
    const int32_t dx[]={0,1,0,-1},dy[]={-1,0,1,0};
    facing=((facing%4)+4)%4;
    Vector3 position={(float)sim->mine.x-(float)dx[facing]*back,eye,
                      (float)sim->mine.y-(float)dy[facing]*back};
    return (Camera3D){.position=position,
        .target={position.x+(float)dx[facing],position.y+pitch,
                 position.z+(float)dy[facing]},
        .up={0,1,0},.fovy=fovy,.projection=CAMERA_PERSPECTIVE};
}
static void MineBox(float x,float y,float z,float w,float h,float d,Color color)
{
    DrawCube((Vector3){x,y,z},w,h,d,color);
}
Camera3D CcLocalMineCombatCamera(const CcLocalAgent *agent,
                                const CcLocalCourse *course,int32_t facing)
{
    Camera3D base=MineCombatBaseCamera(agent->position,facing);
    return CcLocalCombatCameraInternal(base,agent,course,(float)GetTime(),
                                       false,CC_LOCAL_ART_HEIGHT);
}
void CcLocalDrawMine3D(const CcSim *sim,const CcLocalAgent *agent,
                       const CcLocalCourse *course,int32_t facing,
                       RenderTexture2D target,Rectangle destination)
{
    PrepareRuntimeAssets(0U);
    bool underground=sim->mine.phase==CC_MINE_LEVEL;
    bool combat=sim->mine.contest_active && course!=NULL && course->mine_encounter;
    Camera3D base=CcLocalMineCamera(sim,facing);
    Camera3D camera=combat?CcLocalCombatCameraInternal(
        MineCombatBaseCamera(agent->position,facing),agent,course,(float)GetTime(),
        true,target.texture.height):base;
    RememberPresentedCamera(CC_LOCAL_SCENE_MINE,camera,agent,
                            target.texture.width,target.texture.height);
    ArtComposition art=ROAD_ART_COMPOSITION;
    art.light_profile=underground?ART_LIGHT_INTERIOR_EMBER:ART_LIGHT_STORYBOOK;
    art.focal_point=camera.target;
    CcLocalAgent hero;
    CcLocalAgentInit(&hero,(Vector2){0,0},true);
    hero.appearance=agent->appearance;
    hero.crowned=agent->crowned;
    hero.tunic_color=agent->tunic_color;
    art.foreground_anchor=(Vector3){(float)sim->mine.x,0,(float)sim->mine.y};
    SetFaceRenderContext(camera,target.texture.width,target.texture.height);
    const Color stone={74,83,84,255},floor={187,178,151,255};
    BeginTextureMode(target);
    ClearBackground(ArtLightBackground(art.light_profile));
    BeginMode3D(camera);
    BeginWorldLighting(camera,&art);
    if (underground) {
        for (int y=1;y<CC_MINE_HEIGHT-1;++y) for(int x=1;x<CC_MINE_WIDTH-1;++x) {
            bool walk=CcMineWalkable(sim,CC_MINE_LEVEL,x,y);
            if(walk) {
                Color tint=((x+y)%2)?floor:(Color){179,171,145,255};
                MineBox((float)x,-.08f,(float)y,.98f,.12f,.98f,tint);
                if(!combat) MineBox((float)x,2.5f,(float)y,1,.12f,1,
                                    (Color){59,61,61,255});
            } else if(!(x==15&&y==10&&!sim->mine.bar_open)) {
                bool edge=CcMineWalkable(sim,CC_MINE_LEVEL,x+1,y)||CcMineWalkable(sim,CC_MINE_LEVEL,x-1,y)||
                    CcMineWalkable(sim,CC_MINE_LEVEL,x,y+1)||CcMineWalkable(sim,CC_MINE_LEVEL,x,y-1);
                if(edge) MineBox((float)x,.7f,(float)y,1,1.4f,1,stone);
            }
        }
        DrawStreetLantern(13,3,CC_LOCAL_ROAD_INDUSTRIAL);
        DrawStreetLantern(18,17,CC_LOCAL_ROAD_INDUSTRIAL);
        MineBox(5,.12f,3,.85f,.15f,.85f,(Color){94,153,140,255});
        MineBox(26,.18f,4,.85f,.3f,.65f,(Color){121,75,45,255});
        MineBox(26,.37f,4,.6f,.06f,.45f,sim->mine.surveyed?(Color){85,166,151,255}:(Color){248,223,153,255});
        if(!sim->mine.bar_open) {
            const Color wood={151,84,44,255};
            MineBox(14.6f,.7f,10,.16f,1.4f,.16f,wood);
            MineBox(15.4f,.7f,10,.16f,1.4f,.16f,wood);
            MineBox(15,.35f,10,.95f,.18f,.18f,wood);
            MineBox(15,.85f,10,.95f,.18f,.18f,wood);
            MineBox(15,1.25f,10,.95f,.18f,.18f,wood);
        }
        int32_t source_crates=CcMineSourceUsed(sim);
        if(source_crates>3) source_crates=3;
        for(int i=0;i<source_crates;++i)
            MineBox(25.4f+(float)i*.6f,.31f,16,.55f,.62f,.72f,
                (Color){151,91,47,255});
        /* The offer and combat use the same saved group identity, appearance,
           and source positions. */
        if (!combat && sim->mine.source_owner_id == sim->goblins.id &&
            sim->mine.encounter_outcome!=CC_MINE_ENCOUNTER_BARGAINED &&
            sim->mine.encounter_outcome!=CC_MINE_ENCOUNTER_CONTESTED) {
            Vector2 approach=MineEncounterApproach(sim);
            for (int i = 0; i < 2; ++i) {
                CcLocalAgent hauler;
                InitMineHauler(&hauler,sim,approach,i);
                DrawCourseRaider(&hauler);
            }
        }
        if (combat) {
            for (int32_t i=0;i<CC_LOCAL_RAIDER_COUNT;++i)
                DrawCourseRaider(&course->raiders[i]);
        }
        int32_t cached_crates=CcMineCacheUsed(sim);
        if(cached_crates>4) cached_crates=4;
        for(int i=0;i<cached_crates;++i)
            MineBox(3.5f+(float)i*.6f,.28f,16,.52f,.56f,.66f,
                (Color){151,91,47,255});
    } else if(!underground) {
        MineBox(15,-.18f,10,33,.3f,23,(Color){119,144,112,255});
        MineBox(15,.01f,11,5,.05f,19,(Color){183,168,133,255});
        MineBox(15,.02f,10,22,.05f,3,(Color){183,168,133,255});
        for(int row=0;row<2;++row) for(int col=0;col<2;++col) {
            float x=col==0?5.5f:24.5f,z=row==0?6.5f:12.5f;
            MineBox(x,1.15f,z,6,2.3f,4,(Color){199,183,144,255});
            (void)DrawPitchedRoof(x-3,z-2,6,4,2.3f,(Color){199,183,144,255},
                col==0?(Color){85,102,95,255}:(Color){130,84,67,255});
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
        DrawRoadCarriage((Vector3){CC_MINE_YARD_CARRIAGE_X,0,
            CC_MINE_YARD_CARRIAGE_Y},sim,0,0,0,PI,true,false,
            CC_LOCAL_SCENE_ROAD, 1.0f, 0.0f);
        /* The parked team uses the same model as road travel. */
        for(int i=0;i<5;++i) {
            float x=1+(float)i*7;
            MineBox(x,.8f,18,.18f,1.6f,.18f,(Color){98,79,50,255});
            if(i<4) MineBox(x+3.5f,1.1f,18,7,.12f,.12f,(Color){116,98,64,255});
        }
    }
    if (combat) {
        DrawRobotShell(agent);
        DrawCombatSword(agent);
        if (agent->combat.target_index>=0 &&
            agent->combat.target_index<CC_LOCAL_RAIDER_COUNT)
            DrawSelectedTarget(&course->raiders[agent->combat.target_index]);
    } else if(!underground) {
        rlPushMatrix();
        rlTranslatef((float)sim->mine.x,-hero.position.y,(float)sim->mine.y);
        DrawRobotShell(&hero);
        DrawCombatSword(&hero);
        rlPopMatrix();
    }
    if (sim->schema_version<116U && CcMineCarriedCrates(sim)>0) {
        if (underground) {
            const int32_t dx[]={0,1,0,-1},dy[]={-1,0,1,0};
            int32_t direction=((facing%4)+4)%4;
            MineBox((float)sim->mine.x+(float)dx[direction]*.72f,.52f,
                (float)sim->mine.y+(float)dy[direction]*.72f,
                .62f,.58f,.62f,(Color){166,99,48,255});
        } else {
            MineBox((float)sim->mine.x,.92f,(float)sim->mine.y-.42f,
                .66f,.62f,.66f,(Color){166,99,48,255});
        }
    }
    EndWorldLighting();
    EndMode3D();
    EndTextureMode();
    PresentTarget(target,destination);
}
