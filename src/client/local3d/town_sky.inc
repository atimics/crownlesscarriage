/* Town-only, unlit distant art. The atmosphere controller owns weather/time;
   these meshes never enter collision, picking, production or the save format.
   A complete 360-degree horizon shares world coordinates across named screens. */
typedef struct TownSkyColors {
    Color zenith, horizon, cloud, cloud_shadow, far_ridge, near_ridge;
    float stars, sun, cloud_cover;
} TownSkyColors;

typedef struct TownHorizonProfile {
    float far_height, near_height, ridge_sharpness, ridge_axis;
    Color tint;
    int32_t feature; /* fields, quarry, roofs, fort, domes, ruins */
} TownHorizonProfile;

static const TownHorizonProfile TOWN_HORIZONS[] = {
    [CC_SETTLEMENT_FARMING] = {14, 7, 0.15f, 1.2f, {113,139,111,255}, 0},
    [CC_SETTLEMENT_MINING] = {45, 20, 0.80f, 2.4f, {117,109,126,255}, 1},
    [CC_SETTLEMENT_MARKET] = {19, 9, 0.25f, 0.6f, {120,117,143,255}, 2},
    [CC_SETTLEMENT_FORTRESS] = {57, 27, 0.95f, 3.1f, {105,124,146,255}, 3},
    [CC_SETTLEMENT_CAPITAL] = {23, 11, 0.20f, 0.9f, {144,133,150,255}, 4},
    [CC_SETTLEMENT_DUNGEON_TOWN] = {43, 19, 0.90f, 4.5f, {106,117,124,255}, 5},
};

enum { TOWN_SKY_SECTORS = 64, TOWN_SKY_RINGS = 16,
       TOWN_RIDGE_SECTORS = 96, TOWN_SKY_CLOUDS = 24 };
static const float TOWN_SKY_RADIUS = 470.0f;
static const float TOWN_HORIZON_RADII[2] = {290.0f, 184.0f};

static const TownHorizonProfile *TownHorizonFor(const CcSettlement *place)
{
    int32_t type = place != NULL ? (int32_t)place->function : 0;
    if (type < 0 || type >= (int32_t)(sizeof(TOWN_HORIZONS) / sizeof(TOWN_HORIZONS[0])))
        type = CC_SETTLEMENT_FARMING;
    return &TOWN_HORIZONS[type];
}

/* Local integer mixing, not the simulation PRNG. No draw-order dependency. */
static float TownSkyNoise(uint32_t key)
{
    key ^= key >> 16; key *= UINT32_C(0x7feb352d);
    key ^= key >> 15; key *= UINT32_C(0x846ca68b); key ^= key >> 16;
    return (float)(key & UINT32_C(0xffff)) / 65535.0f;
}

static TownSkyColors TownSkyMix(TownSkyColors a, TownSkyColors b, float t)
{
    t = ArtAtmosphereClamp(t);
    return (TownSkyColors){
        BlendColor(a.zenith,b.zenith,t), BlendColor(a.horizon,b.horizon,t),
        BlendColor(a.cloud,b.cloud,t), BlendColor(a.cloud_shadow,b.cloud_shadow,t),
        BlendColor(a.far_ridge,b.far_ridge,t), BlendColor(a.near_ridge,b.near_ridge,t),
        a.stars+(b.stars-a.stars)*t, a.sun+(b.sun-a.sun)*t,
        a.cloud_cover+(b.cloud_cover-a.cloud_cover)*t};
}

static TownSkyColors TownSkyPalette(const CcSim *sim, const CcSettlement *place)
{
    const TownSkyColors day = {{53,104,153,255},{199,207,175,255},
        {239,226,193,255},{137,165,181,255},{127,153,161,255},{79,111,125,255},0,1,0.25f};
    const TownSkyColors dawn = {{91,113,157,255},{244,185,130,255},
        {246,208,163,255},{154,140,162,255},{161,155,158,255},{98,114,135,255},0,0.7f,0.25f};
    const TownSkyColors dusk = {{60,66,119,255},{242,150,101,255},
        {246,174,131,255},{121,94,140,255},{157,112,140,255},{94,82,117,255},0.08f,0.8f,0.32f};
    const TownSkyColors rain = {{66,84,112,255},{163,178,185,255},
        {166,177,189,255},{85,104,129,255},{127,147,158,255},{90,116,134,255},0,0,0.92f};
    const TownSkyColors night = {{13,22,47,255},{69,85,119,255},
        {120,138,162,255},{39,53,85,255},{60,79,108,255},{32,51,74,255},1,0,0.20f};
    const TownSkyColors omen = {{58,37,68,255},{170,109,114,255},
        {155,113,123,255},{75,54,91,255},{113,88,111,255},{69,63,91,255},0,0,0.88f};
    ArtAtmosphereDefinition a = ArtAtmosphereCurrent();
    float wet = ArtAtmosphereClamp(a.rain / 0.82f);
    float dark = ArtAtmosphereClamp((a.moonlight - 0.18f) / 0.64f);
    float amber = ArtAtmosphereClamp((a.horizon_warmth - 0.16f) / 0.74f);
    TownSkyColors colors = day;
    /* The existing clock starts its daytime interval at minute zero. This is
       a cosmetic dawn tint within clear weather, not a new clock or forecast. */
    if (sim != NULL) {
        float minute = (float)sim->clock.minute_subticks / (float)CC_WORLD_MINUTE_SUBTICKS;
        float morning = ArtAtmosphereClamp(1.0f - minute / 90.0f);
        colors = TownSkyMix(colors, dawn, morning * (1.0f-wet) * (1.0f-dark) * (1.0f-amber));
    }
    colors = TownSkyMix(colors,dusk,amber);
    colors = TownSkyMix(colors,rain,wet);
    colors = TownSkyMix(colors,night,dark);
    colors = TownSkyMix(colors,omen,a.omen);
    const TownHorizonProfile *p = TownHorizonFor(place);
    colors.far_ridge = BlendColor(colors.far_ridge,p->tint,0.12f*(1.0f-dark));
    colors.near_ridge = BlendColor(colors.near_ridge,p->tint,0.30f*(1.0f-dark));
    /* Damage tints only the low haze. A burnt town is not proof of an active
       fire; neither damage nor hunger creates a smoke plume or rain storm. */
    float scar = place != NULL ? ArtAtmosphereClamp((float)place->fire_damage/100.0f) : 0;
    colors.horizon = BlendColor(colors.horizon,colors.cloud_shadow,scar*0.18f);
    return colors;
}

static Vector3 TownSkyDirection(float yaw, float elevation)
{
    float level = cosf(elevation);
    return (Vector3){sinf(yaw)*level,sinf(elevation),cosf(yaw)*level};
}

static void TownSkyVertex(Vector3 point, Color color)
{
    rlColor4ub(color.r,color.g,color.b,color.a);
    rlVertex3f(point.x,point.y,point.z);
}

static Vector3 TownSkyPoint(Camera3D camera, float yaw, float elevation)
{
    return Vector3Add(camera.position,Vector3Scale(TownSkyDirection(yaw,elevation),TOWN_SKY_RADIUS));
}

static void TownSkyTriangle(Vector3 a, Vector3 b, Vector3 c, Color ca, Color cb, Color cc)
{
    TownSkyVertex(a,ca); TownSkyVertex(b,cb); TownSkyVertex(c,cc);
}

/* A scalloped cloud patch on the celestial sphere: world-direction anchored,
   not a screen overlay. Foreground buildings naturally mask it. */
static void TownSkyCloud(Camera3D camera, float yaw, float elevation,
                         float width, float height, Color top, Color bottom)
{
    Vector3 center = TownSkyPoint(camera,yaw,elevation);
    for (int32_t edge = 0; edge < 20; ++edge) {
        float u = (float)edge * (2.0f*PI/20.0f);
        float v = (float)(edge+1) * (2.0f*PI/20.0f);
        float ru = 1.0f+0.10f*sinf(u*5.0f+0.7f);
        float rv = 1.0f+0.10f*sinf(v*5.0f+0.7f);
        Vector3 a = TownSkyPoint(camera,yaw+cosf(u)*width*ru,elevation+sinf(u)*height*ru);
        Vector3 b = TownSkyPoint(camera,yaw+cosf(v)*width*rv,elevation+sinf(v)*height*rv);
        Color ac = sinf(u)>0 ? top : bottom;
        Color bc = sinf(v)>0 ? top : bottom;
        TownSkyTriangle(center,a,b,BlendColor(bottom,top,0.55f),ac,bc);
    }
}

static float TownRidgeHeight(const TownHorizonProfile *p, float angle, int32_t layer, uint32_t seed)
{
    float phase = TownSkyNoise(seed)*2.0f*PI;
    float hills = 0.48f + 0.18f*sinf(angle*3.0f+phase) +
        0.14f*sinf(angle*7.0f-phase) + 0.10f*cosf(angle*11.0f+0.3f);
    float teeth = fabsf(sinf(angle*13.0f+phase))*0.30f;
    float exposure = 0.72f+0.28f*cosf(angle-p->ridge_axis);
    return (layer==0 ? p->far_height : p->near_height)*exposure*
        (hills*(1.0f-p->ridge_sharpness*0.35f)+teeth*p->ridge_sharpness);
}

static Vector3 TownHorizonPoint(float angle, float radius, float y)
{
    return (Vector3){50.0f+sinf(angle)*radius,y,36.0f+cosf(angle)*radius};
}

static void DrawTownSky(const CcSim *sim, const CcSettlement *place, Camera3D camera, float clock)
{
    if (place==NULL) return;
    TownSkyColors colors = TownSkyPalette(sim,place);
    rlDrawRenderBatchActive();
    rlDisableDepthTest(); rlDisableDepthMask(); rlDisableBackfaceCulling();
    rlSetTexture(0); rlBegin(RL_TRIANGLES);
    for (int32_t band=0; band<TOWN_SKY_RINGS; ++band) {
        float low = -PI*0.5f + (float)band*PI/(float)TOWN_SKY_RINGS;
        float high = -PI*0.5f + (float)(band+1)*PI/(float)TOWN_SKY_RINGS;
        Color a = BlendColor(colors.horizon,colors.zenith,ArtAtmosphereClamp(sinf(low)*1.35f));
        Color b = BlendColor(colors.horizon,colors.zenith,ArtAtmosphereClamp(sinf(high)*1.35f));
        for (int32_t sector=0; sector<TOWN_SKY_SECTORS; ++sector) {
            float left=(float)sector*2.0f*PI/(float)TOWN_SKY_SECTORS;
            float right=(float)(sector+1)*2.0f*PI/(float)TOWN_SKY_SECTORS;
            Vector3 p0=TownSkyPoint(camera,left,low),p1=TownSkyPoint(camera,right,low);
            Vector3 p2=TownSkyPoint(camera,right,high),p3=TownSkyPoint(camera,left,high);
            TownSkyTriangle(p0,p1,p2,a,a,b); TownSkyTriangle(p0,p2,p3,a,b,b);
        }
    }
    if (colors.stars>0.05f) {
        for (uint32_t star=0; star<96; ++star) {
            float yaw=TownSkyNoise(star*17U+73U)*2.0f*PI;
            float elevation=0.10f+TownSkyNoise(star*13U+311U)*1.28f;
            Color light=BlendColor(colors.zenith,(Color){218,226,235,255},colors.stars);
            TownSkyCloud(camera,yaw,elevation,0.0010f,0.0013f,light,light);
        }
    }
    if (sim != NULL && colors.stars > 0.05f) {
        /* Five bright points per sign make fixed patterns along the zodiac. */
        static const float patterns[13][5][2] = {
            {{-1,0},{0,1},{1,0},{0,-1},{0,0}},
            {{-1,-1},{0,0},{0,1},{.4f,.2f},{1,1}},
            {{-1,1},{-.5f,0},{0,.6f},{.5f,0},{1,1}},
            {{-1,0},{-.6f,1},{0,.1f},{.5f,.8f},{1,-.2f}},
            {{-1,1},{0,1},{1,1},{0,0},{0,-1}},
            {{-1,1},{-.6f,-.2f},{0,-1},{.6f,-.2f},{1,1}},
            {{-1,.8f},{-.5f,0},{0,-1},{.5f,0},{1,.8f}},
            {{-1,-1},{-.5f,-.3f},{0,0},{.3f,.8f},{1,1}},
            {{-1,-.5f},{-.5f,.5f},{0,1},{.5f,.5f},{1,-.5f}},
            {{-1,-1},{-1,1},{0,1},{1,1},{1,-1}},
            {{-1,-1},{-.6f,.2f},{0,1},{.6f,.2f},{1,-1}},
            {{-1,.5f},{0,1},{1,.5f},{0,0},{0,-1}},
            {{-1,-.5f},{-.5f,.5f},{0,-.5f},{.5f,.5f},{1,0}}
        };
        float turn = (float)sim->clock.minute_subticks / (float)CC_WORLD_DAY_SUBTICKS * 2.0f * PI;
        Color star_light = BlendColor(colors.zenith, (Color){239,230,202,255}, colors.stars);
        for (int sign = 0; sign < CC_ZODIAC_SIGNS; ++sign)
            for (int point = 0; point < 5; ++point)
                TownSkyCloud(camera, turn + ((float)sign + .5f) * 2.0f * PI / 13.0f + patterns[sign][point][0] * .075f,
                    .50f + patterns[sign][point][1] * .075f, .0023f, .0025f, star_light, star_light);
        float wanderer = (float)CcCalendarWanderer(sim->current_day,
            sim->clock.minute_subticks / CC_WORLD_MINUTE_SUBTICKS) * 2.0f * PI;
        Color gold = BlendColor(colors.zenith, (Color){250,192,104,255}, colors.stars);
        TownSkyCloud(camera, turn + wanderer, .50f, .005f, .006f, gold, gold);
    }
    ArtAtmosphereDefinition a=ArtAtmosphereCurrent();
    Vector3 source=Vector3Normalize(a.light_direction);
    float yaw=atan2f(source.x,source.z), elevation=asinf(CombatClamp(source.y,-1,1));
    float disc=fmaxf(colors.stars,colors.sun);
    if (disc>0.05f) {
        Color glow=BlendColor(colors.horizon,colors.cloud,disc*0.18f);
        TownSkyCloud(camera,yaw,elevation,0.055f,0.045f,glow,glow);
        Color celestial=BlendColor((Color){253,234,185,255},(Color){209,228,247,255},colors.stars);
        Color light=BlendColor(colors.horizon,celestial,disc);
        TownSkyCloud(camera,yaw,elevation,0.012f,0.015f,light,light);
    }
    /* Slow periodic, reversible drift; pause/reduced motion supplies no drift.
       Wrap before trig so a very large campaign clock stays finite. */
    float t=(!reduced_motion_enabled && isfinite(clock)) ? fmodf(clock,3600.0f) : 0.0f;
    uint32_t seed=(sim != NULL ? sim->world_seed : 0U) ^ (uint32_t)place->id;
    for (uint32_t cloud=0; cloud<TOWN_SKY_CLOUDS; ++cloud) {
        float layer=(float)(cloud%3U);
        float cyaw=(float)cloud*(2.0f*PI/(float)TOWN_SKY_CLOUDS)+
            TownSkyNoise(seed+cloud*47U)*0.16f + sinf(t*(2.0f*PI/3600.0f)+layer)*0.065f;
        float celev=0.12f+layer*0.17f+TownSkyNoise(seed+cloud*19U)*0.17f;
        float width=0.08f+TownSkyNoise(seed+cloud*67U)*0.16f+colors.cloud_cover*0.12f;
        float height=0.010f+layer*0.008f+colors.cloud_cover*0.025f;
        Color top=BlendColor(colors.zenith,colors.cloud,0.58f+colors.cloud_cover*0.30f);
        Color bottom=BlendColor(top,colors.cloud_shadow,0.50f);
        TownSkyCloud(camera,cyaw,celev,width,height,top,bottom);
    }
    rlEnd(); rlDrawRenderBatchActive();
    rlEnableBackfaceCulling(); rlEnableDepthMask(); rlEnableDepthTest();
}

static void DrawTownHorizon(const CcSim *sim, const CcSettlement *place)
{
    if (place==NULL) return;
    TownSkyColors colors=TownSkyPalette(sim,place);
    const TownHorizonProfile *p=TownHorizonFor(place);
    float ground=CcLocalTerrainHeightAt(50.0f,36.0f)-2.0f;
    uint32_t seed=(sim != NULL ? sim->world_seed : 0U) ^ (uint32_t)place->id;
    rlDrawRenderBatchActive(); rlDisableBackfaceCulling(); rlSetTexture(0);
    rlBegin(RL_TRIANGLES);
    for (int32_t layer=0; layer<2; ++layer) {
        float radius=TOWN_HORIZON_RADII[layer];
        Color top=layer==0 ? colors.far_ridge : colors.near_ridge;
        Color base=BlendColor(top,colors.horizon,layer==0 ? 0.52f : 0.28f);
        for (int32_t i=0; i<TOWN_RIDGE_SECTORS; ++i) {
            float u=(float)i*(2.0f*PI/(float)TOWN_RIDGE_SECTORS);
            float v=(float)(i+1)*(2.0f*PI/(float)TOWN_RIDGE_SECTORS);
            Vector3 a=TownHorizonPoint(u,radius,ground-75), b=TownHorizonPoint(v,radius,ground-75);
            Vector3 c=TownHorizonPoint(v,radius,ground+TownRidgeHeight(p,v,layer,seed));
            Vector3 d=TownHorizonPoint(u,radius,ground+TownRidgeHeight(p,u,layer,seed));
            TownSkyTriangle(a,b,c,base,base,top); TownSkyTriangle(a,c,d,base,top,top);
        }
    }
    rlEnd(); rlDrawRenderBatchActive(); rlEnableBackfaceCulling();
    /* Sparse non-interactive skyline silhouettes stand on the near ridge.
       They are decoration beyond the walkable map, never new services. */
    for (int32_t i=0; i<32; ++i) {
        float angle=(float)i*(2.0f*PI/32.0f)+p->ridge_axis;
        float radius=TOWN_HORIZON_RADII[1]+3.0f;
        float height=4.0f+TownSkyNoise(seed+(uint32_t)i*43U)*7.0f;
        Vector3 pos=TownHorizonPoint(angle,radius,ground+TownRidgeHeight(p,angle,1,seed)-2.0f);
        Color shade=BlendColor(colors.near_ridge,colors.far_ridge,0.18f);
        switch (p->feature) {
            case 0: /* windbreak trees and occasional distant farm gables */
                DrawCubeV(Vector3Add(pos,(Vector3){0,height*0.4f,0}),(Vector3){0.6f,height*0.8f,0.6f},shade);
                DrawSphereEx(Vector3Add(pos,(Vector3){0,height,0}),height*0.42f,4,6,shade);
                if (i%8==0) { DrawCubeV(Vector3Add(pos,(Vector3){6,2,0}),(Vector3){6,4,4},shade);
                    DrawCylinder(Vector3Add(pos,(Vector3){6,4,0}),0,4,3,4,shade); }
                break;
            case 1: /* headframes and quarry markers; no invented production smoke */
                if (i%3==0) {
                    DrawCylinderEx(pos,Vector3Add(pos,(Vector3){-2,height,0}),0.4f,0.3f,4,shade);
                    DrawCylinderEx(Vector3Add(pos,(Vector3){-5,0,0}),Vector3Add(pos,(Vector3){-2,height,0}),0.4f,0.3f,4,shade);
                    DrawCubeV(Vector3Add(pos,(Vector3){-2,height,0}),(Vector3){8,1,2},shade);
                    DrawCylinderEx(Vector3Add(pos,(Vector3){-2,height,0}),Vector3Add(pos,(Vector3){-2,2,0}),0.12f,0.12f,4,shade);
                } break;
            case 2: /* roofs and a sparse belfry rhythm */
                height=i%7==0 ? height*1.5f : height*0.7f;
                DrawCubeV(Vector3Add(pos,(Vector3){0,height*0.5f,0}),(Vector3){5,height,5},shade);
                DrawCylinder(Vector3Add(pos,(Vector3){0,height,0}),0.1f,4,3,4,shade);
                break;
            case 3: /* watch posts on the ridge, not a wall across the playable gorge */
                if (i%4==0) {
                    DrawCubeV(Vector3Add(pos,(Vector3){0,height*0.5f,0}),(Vector3){4,height,4},shade);
                    for (int32_t k=-1;k<=1;k+=2)
                        DrawCubeV(Vector3Add(pos,(Vector3){(float)k*1.5f,height,0}),(Vector3){1,2,4},shade);
                } break;
            case 4:
                DrawCubeV(Vector3Add(pos,(Vector3){0,height*0.45f,0}),(Vector3){6,height*0.9f,5},shade);
                DrawSphereEx(Vector3Add(pos,(Vector3){0,height*0.9f,0}),3.6f,5,8,shade);
                break;
            default: /* broken piers and bare branches */
                DrawCylinderEx(pos,Vector3Add(pos,(Vector3){1,height,0}),0.7f,0.12f,5,shade);
                DrawCylinderEx(Vector3Add(pos,(Vector3){0,height*0.5f,0}),Vector3Add(pos,(Vector3){-3,height*0.8f,0}),0.3f,0.04f,4,shade);
                if(i%5==0) DrawCubeV(Vector3Add(pos,(Vector3){4,3,0}),(Vector3){2,6,3},shade);
                break;
        }
    }
}

static void SetTownSkyFog(const CcSim *sim, const CcSettlement *place)
{
    if (!visual_style.world_ready) return;
    TownSkyColors colors=TownSkyPalette(sim,place);
    Color haze=BlendColor(colors.horizon,colors.far_ridge,0.38f);
    float fog[3]={(float)haze.r/255.0f,(float)haze.g/255.0f,(float)haze.b/255.0f};
    SetShaderValue(visual_style.world,visual_style.fog_color_location,fog,SHADER_UNIFORM_VEC3);
}
