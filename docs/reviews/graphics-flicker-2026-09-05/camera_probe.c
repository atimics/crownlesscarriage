/* Review instrument. Calls the production camera and visibility helpers. */
#include "client/cc_local3d.c"

static void ProbeShotChange(void)
{
    FixedCameraRig rig = {0};
    Vector3 offset = {3, 8, 14};
    FixedCameraRigAim(&rig, 0, (Vector3){0}, offset, 8, 0, true);
    float previous = 0;
    for (int frame = 1; frame <= 17; ++frame) {
        float clock = (float)frame / 60.0f;
        FixedCameraRigAim(&rig, 1, (Vector3){10, 0, 0}, offset, 8, clock, true);
        float progress = rig.transition_elapsed / rig.transition_duration;
        float opacity = SmoothStep01(1 - fabsf(progress * 2 - 1));
        printf("shot,%d,%.6f,%.3f,%.3f,%.3f\n", frame, clock,
               rig.displayed_target.x, rig.displayed_target.x - previous, opacity);
        previous = rig.displayed_target.x;
    }
}

static void ProbeTownWalks(void)
{
    const uint32_t seeds[] = {0xc0a71a9eU, 0x12345678U};
    for (int seed = 0; seed < 2; ++seed) {
        static CcSim sim;
        CcSimInit(&sim, seeds[seed]);
        for (int town = 0; town < 6; ++town) {
            sim.player.location_id = sim.settlements[town].id;
            CcLocalBindPlace(&sim);
            const CcLocalPlaceProfile *profile = ActivePlaceProfile();
            float biggest = 0, px = 0, pz = 0;
            int scene_changes = 0, reframes = 0, stable_large_steps = 0;
            for (int route = 0; route < CC_LOCAL_CARRIAGE_ROUTE_COUNT; ++route) {
                const CcLocalPlaceRoad *road = &profile->carriage_route[route];
                street_camera_rig = (FixedCameraRig){0};
                Camera3D previous = {0};
                int old_shot = -1;
                for (int frame = 0; frame < 900; ++frame) {
                    float t = (float)frame / 899.0f;
                    float x = road->x + road->width * (road->runs_east_west ? t : 0.5f);
                    float z = road->z + road->depth * (road->runs_east_west ? 0.5f : t);
                    CcLocalAgent agent = {.position = {x, CcLocalTerrainHeightAt(x,z), z}};
                    Vector3 old_framing = street_camera_rig.framing_offset;
                    Camera3D camera = CcLocalStreetCameraInternal(&agent, (float)frame / 60, true, 320);
                    if (frame > 0 && old_shot != street_camera_rig.shot) scene_changes++;
                    if (frame > 0 && old_shot == street_camera_rig.shot &&
                        Vector3Distance(old_framing, street_camera_rig.framing_offset) > 0.01f) reframes++;
                    if (frame > 0 && old_shot == street_camera_rig.shot &&
                        street_camera_rig.transition_elapsed >= street_camera_rig.transition_duration) {
                        float move = Vector3Distance(camera.target, previous.target);
                        if (move > 0.5f) stable_large_steps++;
                        if (move > biggest) { biggest = move; px = x; pz = z; }
                    }
                    previous = camera;
                    old_shot = street_camera_rig.shot;
                }
            }
            printf("town,%08x,%d,%s,shot_changes=%d,reframes=%d,steps_over_0.5=%d,max_step=%.3f,hero=%.3f/%.3f\n",
                seeds[seed], town, sim.settlements[town].name, scene_changes, reframes, stable_large_steps, biggest, px, pz);
        }
    }
}

static void ProbeProjection(void)
{
    Camera3D base = ExteriorCameraComposed((Vector3){0, 1, 0}, (Vector3){-9, 8, 11}, 7.2f);
    CcLocalAgent hero = {.position = {0, 0, 0}};
    CcLocalAgent partner = {.position = {2, 0, 0}};
    conversation_camera_rig = (ConversationCameraRig){0};
    (void)CcLocalConversationCameraInternal(base, &hero, &partner, false, 0, true, 320);
    Camera3D entering = CcLocalConversationCameraInternal(base, &hero, &partner, true, 0, false, 320);
    Vector3 points[] = {{2,1,0}, {2,1,-10}, {2,1,5}};
    for (int i = 0; i < 3; ++i) {
        Vector2 a = GetWorldToScreenEx(points[i], base, 630, 320);
        Vector2 b = GetWorldToScreenEx(points[i], entering, 630, 320);
        printf("projection,%d,mode=%d/%d,position_step=%.6f,target_step=%.6f,pixel_jump=%.3f\n",
            i, base.projection, entering.projection, Vector3Distance(base.position, entering.position),
            Vector3Distance(base.target, entering.target), Vector2Distance(a,b));
    }
}

static void ProbePlatformBoundary(void)
{
    const NavPlatform *p = &STREET_PLATFORMS[0];
    float base = PlatformBaseHeight(p);
    Vector3 first = {p->x + p->width * 0.5f, base + 1.02f, p->z - 2.0f};
    Vector3 second = Vector3Add(first, (Vector3){0.7f, 0, 0});
    Camera3D camera = {.position = {first.x, first.y + 1, first.z + 6},
        .target = first, .up = {0,1,0}, .fovy = 43, .projection = CAMERA_PERSPECTIVE};
    bool old_cut = false;
    float old_score = 0, old_shift = 0;
    for (int step = 0; step < 12001; ++step) {
        float shift = -3 + (float)step * 0.0005f;
        Camera3D moved = camera;
        moved.position.x += shift;
        moved.target.x += shift;
        float score = CameraStreetPlatformSubjectOverlap(moved, p, first, second);
        bool cut = score > 0.001f;
        if (step > 0 && cut != old_cut) {
            printf("platform,shift=%.4f/%.4f,overlap=%.7f/%.7f,height=%.2f/%.2f\n", old_shift, shift, old_score, score,
                old_cut ? fminf(p->height, 0.12f) : p->height, cut ? fminf(p->height, 0.12f) : p->height);
        }
        old_cut = cut; old_score = score; old_shift = shift;
    }
}

static void ProbeRoadVisibility(void)
{
    static CcSim sim;
    static CcWorldStream stream;
    CcSimInit(&sim, 0xc0a71a9eU);
    CcWorldStreamInit(&stream, &sim);
    CcLocalBindOpenWorld(&stream);
    int count = 0;
    for (int gz = -20; gz <= 20 && count < 4; ++gz) {
        for (int gx = -20; gx <= 20 && count < 4; ++gx) {
            float scatter = TerrainScatter01(gx, gz, stream.manifest.world_seed);
            if (scatter <= 0.66f) continue;
            float x = ((float)gx + 0.2f + scatter * 0.6f) * 20;
            float z = ((float)gz + TerrainScatter01(gx,gz,813U)) * 20;
            if (CcWorldSurfaceAt(&stream.manifest,x,z) != CC_WORLD_SURFACE_WILDERNESS ||
                !OpenWorldPointClearsSettlements(&stream.manifest,(CcWorldPoint){x,z},5)) continue;
            for (int angle = 0; angle < 360 && count < 4; angle += 5) {
                float rad = (float)angle * DEG2RAD;
                Vector3 focus = {x - cosf(rad)*122.01f, 0, z - sinf(rad)*122.01f};
                focus.y = CcWorldTerrainHeight(&stream.manifest,focus.x,focus.z);
                int ox = (int)floorf(focus.x/20), oz = (int)floorf(focus.z/20);
                if (abs(gx-ox)>6 || abs(gz-oz)>6) continue;
                CcLocalWorldCarriageState carriage = {.position=focus,.camera_weight=1,.camera_heading_yaw=rad};
                Camera3D camera = StorybookCamera(&stream.manifest,&carriage);
                Vector3 crown = {x,CcWorldTerrainHeight(&stream.manifest,x,z)+5,z};
                Vector2 screen = GetWorldToScreenEx(crown,camera,630,320);
                if (!CameraPointInFront(camera,crown) || screen.x<40 || screen.x>590 || screen.y<25 || screen.y>290) continue;
                printf("road_cull,tree=%d/%d,distance=%.3f,screen=%.1f/%.1f,focus=%.3f/%.3f,heading=%.6f\n",
                       gx,gz,hypotf(x-focus.x,z-focus.z),screen.x,screen.y,focus.x,focus.z,rad);
                count++;
            }
        }
    }
    CcLocalBindOpenWorld(NULL);
}

static void WriteFrame(const char *directory, int frame)
{
    char path[1024];
    snprintf(path,sizeof(path),"%s/frame-%04d.png",directory,frame);
    rlDrawRenderBatchActive();
    Image shot = LoadImageFromScreen();
    ExportImage(shot,path);
    UnloadImage(shot);
}

static void CaptureWalk(const char *directory)
{
    static CcSim sim;
    CcSimInit(&sim,0xc0a71a9eU);
    sim.player.location_id = sim.settlements[4].id;
    CcLocalBindPlace(&sim);
    CcLocalRendererSetOpeningStep(CC_LOCAL_OPENING_COMPLETE);
    CcLocalRendererSetInteractionUI(true);
    RenderTexture2D target = LoadRenderTexture(630,320);
    SetTextureFilter(target.texture,TEXTURE_FILTER_POINT);
    for (int frame=0; frame<241; ++frame) {
        float x = 52 + (float)frame * 0.067f;
        float z = 35.8f;
        CcLocalAgent hero = {0};
        CcLocalAgentInit(&hero,(Vector2){x,z},false);
        float clock = 10 + (float)frame/60;
        CcLocalRendererBeginFrame(1.0f/60);
        BeginDrawing();
        ClearBackground(BLACK);
        CcLocalDrawStreet3D(&sim,&hero,NULL,false,NULL,clock,target,(Rectangle){0,0,630,320});
        DrawRectangle(0,320,630,60,(Color){17,24,31,255});
        DrawText(TextFormat("CURRENT MAIN | Rosespire | frame %d | x %.2f",frame,x),12,329,16,RAYWHITE);
        DrawText(TextFormat("shot %d | framing %.2f, %.2f | camera %.2f, %.2f",street_camera_rig.shot,
            street_camera_rig.framing_offset.x,street_camera_rig.framing_offset.z,
            presented_camera[CC_LOCAL_SCENE_STREET].camera.target.x,presented_camera[CC_LOCAL_SCENE_STREET].camera.target.z),12,351,14,RAYWHITE);
        WriteFrame(directory,frame);
        EndDrawing();
    }
    UnloadRenderTexture(target);
}

static void CaptureRoadCut(const char *directory)
{
    static CcSim sim;
    static CcWorldStream stream;
    CcSimInit(&sim,0xc0a71a9eU);
    CcWorldStreamInit(&stream,&sim);
    CcLocalBindOpenWorld(&stream);
    int gx=-20,gz=-20;
    float scatter=TerrainScatter01(gx,gz,stream.manifest.world_seed);
    float x=((float)gx+0.2f+scatter*0.6f)*20;
    float z=((float)gz+TerrainScatter01(gx,gz,813U))*20;
    float rad=35*DEG2RAD;
    Vector3 center={x-cosf(rad)*122,0,z-sinf(rad)*122};
    center.y=CcWorldTerrainHeight(&stream.manifest,center.x,center.z);
    CcLocalWorldCarriageState carriage={.position=center,.camera_weight=1,.camera_heading_yaw=rad};
    Camera3D camera=StorybookCamera(&stream.manifest,&carriage);
    ArtComposition art=ROAD_ART_COMPOSITION;
    art.light_profile=ART_LIGHT_STORYBOOK;
    art.depth_splits=(Vector3){35,75,130};
    art.focal_point=camera.target;
    art.foreground_anchor=camera.target;
    for (int frame=0;frame<2;++frame) {
        float distance=frame==0 ? 121.99f : 122.01f;
        Vector3 focus={x-cosf(rad)*distance,center.y,z-sinf(rad)*distance};
        BeginDrawing();
        ClearBackground((Color){118,150,172,255});
        BeginMode3D(camera);
        BeginWorldLighting(camera,&art);
        DrawStorybookScenery(&stream.manifest,focus);
        EndWorldLighting();
        EndMode3D();
        DrawRectangle(0,320,630,60,(Color){17,24,31,255});
        DrawText(TextFormat("CURRENT MAIN | fixed camera | tree distance %.2f",distance),12,333,16,RAYWHITE);
        DrawText(TextFormat("forest key %llu | vertices %d",(unsigned long long)storybook_forest_key,
            storybook_forest.vertex_count),12,354,14,RAYWHITE);
        WriteFrame(directory,frame);
        EndDrawing();
    }
    CcLocalBindOpenWorld(NULL);
    OpenWorldRenderCacheClear();
}

int main(int argc,char **argv)
{
    SetTraceLogLevel(LOG_WARNING);
    if (argc==3) {
        SetConfigFlags(FLAG_WINDOW_HIDDEN);
        InitWindow(630,380,"Camera review captures");
        CcLocalRendererInit();
        if (strcmp(argv[1],"--walk")==0) CaptureWalk(argv[2]);
        else CaptureRoadCut(argv[2]);
        CcLocalRendererShutdown();
        CloseWindow();
        return 0;
    }
    ProbeShotChange();
    ProbeTownWalks();
    ProbeProjection();
    ProbePlatformBoundary();
    ProbeRoadVisibility();
    return 0;
}
