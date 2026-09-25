/* These tests exercise the production town-only sky/horizon pass. */
static void TestTownSkyContract(void)
{
    static CcSim sim;
    CcSimInit(&sim,42U);
    ArtAtmosphereState saved=art_atmosphere;
    uint64_t before=CcSimHash(&sim);
    for (int32_t town=0; town<sim.settlement_count; ++town) {
        const TownHorizonProfile *p=TownHorizonFor(&sim.settlements[town]);
        RequireRenderer(p->far_height>p->near_height && p->near_height>0,
            "each town needs separate far and near horizon layers");
        for (int32_t layer=0;layer<2;++layer) {
            RequireRenderer(TOWN_HORIZON_RADII[layer]>160 && TOWN_HORIZON_RADII[layer]<350,
                "backdrop geometry stays outside the playable town and within the clip plane");
            for (uint32_t seed=0;seed<6;++seed) {
                float first=TownRidgeHeight(p,0,layer,seed);
                float last=TownRidgeHeight(p,2.0f*PI,layer,seed);
                RequireRenderer(isfinite(first) && fabsf(first-last)<0.001f,
                    "world horizon must close at the azimuth seam");
            }
        }
        for (int32_t mood=0;mood<CC_LOCAL_ATMOSPHERE_COUNT;++mood) {
            CcLocalRendererSetAtmosphere((CcLocalAtmospherePreset)mood,0);
            TownSkyColors a=TownSkyPalette(&sim,&sim.settlements[town]);
            TownSkyColors b=TownSkyPalette(&sim,&sim.settlements[town]);
            RequireRenderer(ColorToInt(a.zenith)==ColorToInt(b.zenith) &&
                ColorToInt(a.horizon)==ColorToInt(b.horizon) && a.stars==b.stars,
                "palette queries must be deterministic and read-only");
            RequireRenderer(a.horizon.a==255 && a.zenith.a==255 &&
                a.cloud_cover>=0 && a.cloud_cover<=1 && a.stars>=0 && a.stars<=1,
                "all weather palettes are bounded and opaque");
        }
    }
    CcLocalRendererSetAtmosphere(CC_LOCAL_ATMOSPHERE_CLEAR_DAY,0);
    TownSkyColors start=TownSkyPalette(&sim,&sim.settlements[0]);
    CcLocalRendererSetAtmosphere(CC_LOCAL_ATMOSPHERE_MOONLIT_NIGHT,2.0f);
    TownSkyColors at_start=TownSkyPalette(&sim,&sim.settlements[0]);
    RequireRenderer(ColorToInt(start.zenith)==ColorToInt(at_start.zenith),
        "changing atmosphere begins from the displayed sky, not an immediate cut");
    CcLocalRendererUpdateAtmosphereInternal(2.0f);
    TownSkyColors night=TownSkyPalette(&sim,&sim.settlements[0]);
    RequireRenderer(night.stars>0.99f && night.zenith.b<start.zenith.b,
        "night resolves to its own dark sky and stars");
    CcSettlement hungry=sim.settlements[0]; hungry.hunger=100;
    TownSkyColors same_weather=TownSkyPalette(&sim,&hungry);
    RequireRenderer(ColorToInt(night.zenith)==ColorToInt(same_weather.zenith),
        "hunger must not invent new weather");
    RequireRenderer(TownHorizonFor(NULL)!=NULL && TownSkyNoise(99)==TownSkyNoise(99),
        "missing profile and hash inputs are safe");
    RequireRenderer(CcSimHash(&sim)==before,"sky queries cannot mutate simulation or PRNG");
    art_atmosphere=saved;
    puts("PASS town sky: six profiles, closed horizons, bounded blended weather, read-only state");
}

static Image SkyTestFrame(RenderTexture2D target, Camera3D camera,
                           const CcSim *sim, bool landscape, float clock)
{
    BeginTextureMode(target);
    ClearBackground(MAGENTA);
    rlSetClipPlanes(0.1,512.0);
    BeginMode3D(camera);
    DrawTownSky(sim,&sim->settlements[0],camera,clock);
    if(landscape) DrawTownHorizon(sim,&sim->settlements[0]);
    EndMode3D();
    rlSetClipPlanes(0.05,4000.0);
    EndTextureMode();
    Image image=LoadImageFromTexture(target.texture);
    ImageFlipVertical(&image);
    return image;
}

static void TestTownSkyGraphics(void)
{
    static CcSim sim;
    CcSimInit(&sim,42U); CcLocalBindPlace(&sim);
    uint64_t before=CcSimHash(&sim);
    CcLocalRendererSetAtmosphere(CC_LOCAL_ATMOSPHERE_CLEAR_DAY,0);
    bool old_motion=reduced_motion_enabled;
    reduced_motion_enabled=true;
    RenderTexture2D target=LoadRenderTexture(256,144);
    RequireRenderer(IsRenderTextureValid(target),"sky test render target loads");
    Camera3D camera={.position={50,3,36},.target={50,8,16},.up={0,1,0},
        .fovy=52,.projection=CAMERA_PERSPECTIVE};
    Image a=SkyTestFrame(target,camera,&sim,false,0);
    Image b=SkyTestFrame(target,camera,&sim,false,200);
    Color *first=LoadImageColors(a), *second=LoadImageColors(b);
    int32_t uncovered=0,changed=0;
    for(int32_t i=0;i<a.width*a.height;++i) {
        if(ColorToInt(first[i])==ColorToInt(MAGENTA)) ++uncovered;
        if(ColorToInt(first[i])!=ColorToInt(second[i])) ++changed;
    }
    RequireRenderer(uncovered==0 && changed==0,
        "sky covers viewport; reduced motion is independent of animation clock");
    UnloadImageColors(second); UnloadImage(b);
    camera.position.x+=8; camera.target.x+=8;
    b=SkyTestFrame(target,camera,&sim,false,0); second=LoadImageColors(b);
    changed=0;
    for(int32_t i=0;i<a.width*a.height;++i) {
        int difference=abs((int)first[i].r-(int)second[i].r)+
            abs((int)first[i].g-(int)second[i].g)+abs((int)first[i].b-(int)second[i].b);
        if(difference>3) ++changed;
    }
    RequireRenderer(changed<a.width*a.height/100,
        "sky at infinity does not slide with a translated town camera");
    UnloadImageColors(first); UnloadImageColors(second); UnloadImage(a); UnloadImage(b);
    camera=(Camera3D){.position={50,3,36},.target={50,3,16},.up={0,1,0},
        .fovy=52,.projection=CAMERA_PERSPECTIVE};
    BeginTextureMode(target); ClearBackground(BLACK); rlSetClipPlanes(0.1,512);
    BeginMode3D(camera); DrawTownSky(&sim,&sim.settlements[0],camera,0);
    DrawTownHorizon(&sim,&sim.settlements[0]);
    DrawCube((Vector3){50,3,28},2,2,2,RED);
    DrawCube((Vector3){50,3,23},3,3,3,BLUE);
    EndMode3D(); rlSetClipPlanes(0.05,4000); EndTextureMode();
    a=LoadImageFromTexture(target.texture); first=LoadImageColors(a);
    Color center=first[(a.height/2)*a.width+a.width/2];
    RequireRenderer(center.r>180 && center.b<100,
        "sky must restore depth testing AND writes before near and far geometry");
    UnloadImageColors(first); UnloadImage(a); UnloadRenderTexture(target);
    reduced_motion_enabled=old_motion;
    RequireRenderer(CcSimHash(&sim)==before,"GPU sky pass preserves simulation state");
    CcLocalBindPlace(NULL);
    printf("PASS town sky graphics: viewport, reduced motion, translation (%d pixels), depth and PRNG\n",changed);
}

static void CaptureTownSkies(const char *directory)
{
    RequireRenderer(MakeDirectory(directory)==0 || DirectoryExists(directory),"sky output directory exists");
    static CcSim sim;
    static CcLocalCourse course;
    CcLocalAgent hero;
    const char *moods[]={"dawn","day","rain","dusk","night","omen"};
    const CcLocalAtmospherePreset presets[]={CC_LOCAL_ATMOSPHERE_CLEAR_DAY,
        CC_LOCAL_ATMOSPHERE_CLEAR_DAY,CC_LOCAL_ATMOSPHERE_RAINY_OVERCAST,
        CC_LOCAL_ATMOSPHERE_AMBER_DUSK,CC_LOCAL_ATMOSPHERE_MOONLIT_NIGHT,
        CC_LOCAL_ATMOSPHERE_DRAGON_OMEN};
    RenderTexture2D target=LoadRenderTexture(630,320);
    RequireRenderer(IsRenderTextureValid(target),"sky capture target loads");
    for(int32_t town=0;town<6;++town) {
        CcSimInit(&sim,UINT32_C(0xc0a71a9e));
        sim.player.location_id=sim.settlements[town].id;
        sim.carriage.location_id=sim.player.location_id;
        CcLocalBindPlace(&sim); CcLocalCourseInit(&course);
        const CcLocalTownScene *scene=CcLocalTownSceneAt(sim.settlements[town].function,
            town==3 ? CC_LOCAL_TOWN_SCENE_CLOSE_FIRST : CC_LOCAL_TOWN_SCENE_HEART);
        CcLocalAgentInit(&hero,(Vector2){scene->trigger_x,scene->trigger_z},false);
        CcLocalRendererSetOpeningStep(CC_LOCAL_OPENING_COMPLETE);
        for(int32_t mood=0;mood<6;++mood) {
            sim.clock.minute_subticks=(mood==0 ? 0 : 180)*CC_WORLD_MINUTE_SUBTICKS;
            CcLocalRendererSetAtmosphere(presets[mood],0);
            street_camera_rig=(FixedCameraRig){0};
            for(int32_t frame=0;frame<180;++frame)
                (void)CcLocalStreetCameraInternal(&hero,(float)frame/60.0f,true,320);
            uint64_t hash=CcSimHash(&sim);
            BeginDrawing(); ClearBackground(BLACK);
            CcLocalDrawStreet3D(&sim,&hero,&course,false,NULL,3.0f,target,(Rectangle){0,0,1260,640},NULL);
            DrawRectangle(0,640,1280,120,(Color){16,17,25,255});
            DrawText(TextFormat("%s / %s",sim.settlements[town].name,moods[mood]),20,658,24,RAYWHITE);
            DrawText("Native sky/backdrop review - staged scene, not a playthrough",20,696,16,GRAY);
            rlDrawRenderBatchActive();
            Image image=LoadImageFromScreen();
            char path[768]; (void)snprintf(path,sizeof(path),"%s/town-%d-%s.png",directory,town,moods[mood]);
            RequireRenderer(ExportImage(image,path),"sky frame is written");
            UnloadImage(image); EndDrawing();
            RequireRenderer(CcSimHash(&sim)==hash,"town sky capture is read-only");
        }
    }
    UnloadRenderTexture(target);
    puts("PASS town sky captures: six settlements, six sky moods (36 native frames)");
}
