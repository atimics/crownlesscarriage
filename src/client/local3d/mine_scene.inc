#include "sim/cc_mine.h"

/* Each room is staged as a fixed view. The high dungeon views keep a
   visible wall face and the full character silhouette. */
Camera3D CcLocalMineCamera(const CcSim *sim)
{
    bool underground=sim->mine.phase==CC_MINE_LEVEL;
    Vector3 focus,offset;
    float lens;
    if(underground) {
        int column=sim->mine.x<10?0:sim->mine.x<22?1:2;
        const float centers[]={5.0f,15.5f,26.0f};
        focus=(Vector3){centers[column],0,sim->mine.y<10?4.0f:15.5f};
        offset=(Vector3){3.8f,19.0f,10.0f}; lens=48.0f;
    } else if(sim->mine.y<10) {
        focus=(Vector3){15,0,6};offset=(Vector3){-15,13,18};lens=53;
    } else {
        focus=(Vector3){15,0,14};offset=(Vector3){-17,14,21};lens=53;
    }
    return PerspectiveCameraComposed(focus,offset,lens);
}
static void MineBox(float x,float y,float z,float w,float h,float d,Color color)
{
    DrawCube((Vector3){x,y,z},w,h,d,color);
}
void CcLocalDrawMine3D(const CcSim *sim,const CcLocalAgent *agent,
                       RenderTexture2D target,Rectangle destination)
{
    PrepareRuntimeAssets(0U);
    bool overhead=sim->mine.phase==CC_MINE_LEVEL;
    Camera3D camera=CcLocalMineCamera(sim);
    ArtComposition art=ROAD_ART_COMPOSITION;
    art.light_profile=overhead?ART_LIGHT_INTERIOR_EMBER:ART_LIGHT_STORYBOOK;
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
    if (overhead) {
        for (int y=1;y<CC_MINE_HEIGHT-1;++y) for(int x=1;x<CC_MINE_WIDTH-1;++x) {
            bool walk=CcMineWalkable(sim,CC_MINE_LEVEL,x,y);
            if(walk) {
                Color tint=((x+y)%2)?floor:(Color){179,171,145,255};
                MineBox((float)x,0,(float)y,1.0f,.12f,1.0f,tint);
            } else {
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
        DrawRoadCarriage((Vector3){18,0,19},sim,0,0,0,PI,true,false);
        /* The parked team uses the same model as road travel. */
        for(int i=0;i<5;++i) {
            float x=1+(float)i*7;
            MineBox(x,.8f,18,.18f,1.6f,.18f,(Color){98,79,50,255});
            if(i<4) MineBox(x+3.5f,1.1f,18,7,.12f,.12f,(Color){116,98,64,255});
        }
    }
    rlPushMatrix();
    rlTranslatef((float)sim->mine.x,-hero.position.y,(float)sim->mine.y);
    DrawRobotShell(&hero);
    DrawCombatSword(&hero);
    rlPopMatrix();
    EndWorldLighting();
    EndMode3D();
    EndTextureMode();
    PresentTarget(target,destination);
}
