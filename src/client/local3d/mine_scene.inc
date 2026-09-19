#include "sim/cc_mine.h"

Camera3D CcLocalMineCamera(const CcSim *sim, int32_t facing)
{
    if(sim->mine.phase==CC_MINE_LEVEL) {
        const int32_t dx[]={0,1,0,-1},dy[]={-1,0,1,0};
        facing=((facing%4)+4)%4;
        Vector3 eye={(float)sim->mine.x,1.15f,(float)sim->mine.y};
        return (Camera3D){.position=eye,
            .target={eye.x+(float)dx[facing],eye.y,eye.z+(float)dy[facing]},
            .up={0,1,0},.fovy=64,.projection=CAMERA_PERSPECTIVE};
    }
    return PerspectiveCameraComposed((Vector3){15,0,11},(Vector3){-15,10,5},48);
}
static void MineBox(float x,float y,float z,float w,float h,float d,Color color)
{
    DrawCube((Vector3){x,y,z},w,h,d,color);
}
static Camera3D MineCombatCamera(const CcLocalAgent *agent,
                                 const CcLocalCourse *course,int32_t facing)
{
    Camera3D camera={.position={agent->position.x,agent->position.y+1.25f,
                               agent->position.z},
                     .up={0,1,0},.fovy=68,.projection=CAMERA_PERSPECTIVE};
    int32_t selected=agent->combat.target_index;
    if (selected>=0 && selected<CC_LOCAL_RAIDER_COUNT) {
        const CcLocalAgent *target=&course->raiders[selected];
        camera.target=(Vector3){target->position.x,target->position.y+.95f,
                                target->position.z};
    } else {
        const int32_t dx[]={0,1,0,-1},dy[]={-1,0,1,0};
        facing=((facing%4)+4)%4;
        camera.target=(Vector3){camera.position.x+(float)dx[facing],
                                camera.position.y,
                                camera.position.z+(float)dy[facing]};
    }
    return camera;
}
void CcLocalDrawMine3D(const CcSim *sim,const CcLocalAgent *agent,
                       const CcLocalCourse *course,int32_t facing,
                       RenderTexture2D target,Rectangle destination)
{
    PrepareRuntimeAssets(0U);
    bool underground=sim->mine.phase==CC_MINE_LEVEL;
    bool combat=sim->mine.contest_active && course!=NULL && course->mine_encounter;
    Camera3D camera=combat?MineCombatCamera(agent,course,facing):
        CcLocalMineCamera(sim,facing);
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
                MineBox((float)x,2.5f,(float)y,1,.12f,1,(Color){59,61,61,255});
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
        for(int i=0;i<3;++i) MineBox(25.4f+(float)i*.6f,.23f,16,.55f,.46f,1.2f,stone);
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
        DrawRoadCarriage((Vector3){CC_MINE_YARD_CARRIAGE_X,0,
            CC_MINE_YARD_CARRIAGE_Y},sim,0,0,0,PI,true,false,
            CC_LOCAL_SCENE_ROAD);
        /* The parked team uses the same model as road travel. */
        for(int i=0;i<5;++i) {
            float x=1+(float)i*7;
            MineBox(x,.8f,18,.18f,1.6f,.18f,(Color){98,79,50,255});
            if(i<4) MineBox(x+3.5f,1.1f,18,7,.12f,.12f,(Color){116,98,64,255});
        }
    }
    /* The yard is seen from outside, so the company stands in it. Underground
       the camera is the company's own eye, and drawing the figure there puts
       it through the near plane and fills the screen. */
    if (!underground) {
        rlPushMatrix();
        rlTranslatef((float)sim->mine.x,-hero.position.y,(float)sim->mine.y);
        DrawRobotShell(&hero);
        DrawCombatSword(&hero);
        rlPopMatrix();
    }
    EndWorldLighting();
    EndMode3D();
    EndTextureMode();
    PresentTarget(target,destination);
}
