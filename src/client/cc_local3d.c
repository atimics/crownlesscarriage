#include "client/cc_local3d.h"
#include "client/cc_local3d_internal.h"
#include "client/cc_local_place.h"
#include "client/cc_creature_catalog.h"
#include "client/cc_overlay.h"
#include "client/cc_visual_style.h"

#include "locomotion/cc_creature.h"
#include "locomotion/cc_humanoid_skin.h"
#include "locomotion/cc_quadruped.h"
#include "locomotion/cc_robotics.h"

#include "raymath.h"
#include "rlgl.h"

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WORLD_VOID CC_STYLE_BACKGROUND
#define WORLD_INK CC_STYLE_INK
#define WORLD_MUTED CC_STYLE_MUTED
#define WORLD_TEAL CC_STYLE_TEAL
#define WORLD_GOLD CC_STYLE_GOLD
#define WORLD_DANGER CC_STYLE_DANGER
#define WORLD_VIOLET CC_STYLE_VIOLET
#define WORLD_EARTH_SHADOW CC_STYLE_EARTH_SHADOW
#define WORLD_EARTH CC_STYLE_EARTH
#define WORLD_EARTH_LIGHT CC_STYLE_EARTH_LIGHT
#define WORLD_ROAD_SHADOW CC_STYLE_ROAD_SHADOW
#define WORLD_ROAD CC_STYLE_ROAD
#define WORLD_ROAD_LIGHT CC_STYLE_ROAD_LIGHT
#define WORLD_WOOD_SHADOW CC_STYLE_WOOD_SHADOW
#define WORLD_WOOD CC_STYLE_WOOD
#define WORLD_WOOD_LIGHT CC_STYLE_WOOD_LIGHT
#define WORLD_STONE_SHADOW CC_STYLE_STONE_SHADOW
#define WORLD_STONE CC_STYLE_STONE
#define WORLD_STONE_LIGHT CC_STYLE_STONE_LIGHT
#define WORLD_GRASS_SHADOW CC_STYLE_GRASS_SHADOW
#define WORLD_GRASS CC_STYLE_GRASS
#define WORLD_GRASS_LIGHT CC_STYLE_GRASS_LIGHT
#define WORLD_FOLIAGE_SHADOW CC_STYLE_FOLIAGE_SHADOW
#define WORLD_FOLIAGE CC_STYLE_FOLIAGE
#define WORLD_FOLIAGE_LIGHT CC_STYLE_FOLIAGE_LIGHT
#define WORLD_CROP_SHADOW CC_STYLE_CROP_SHADOW
#define WORLD_CROP CC_STYLE_CROP
#define WORLD_CROP_LIGHT CC_STYLE_CROP_LIGHT
#define WORLD_METAL_SHADOW CC_STYLE_METAL_SHADOW
#define WORLD_METAL CC_STYLE_METAL
#define WORLD_METAL_LIGHT CC_STYLE_METAL_LIGHT
static const float PLAYER_COLLISION_RADIUS = 0.30f;
static const float PERSON_COLLISION_RADIUS = 0.27f;
static const float COURSE_GUARD_SPACING = 1.18f;
static const float COURSE_RAIDER_SPACING = 1.48f;
static const float COMBAT_PERSONAL_SPACE = 1.20f;
static const float COMBAT_MIN_STRIKE_DISTANCE = 1.02f;
static const float COMBAT_ALLY_SPACE = 0.92f;
static const float COMBAT_BYSTANDER_SPACE = 1.12f;
static const float COMBAT_PLAYER_STANDOFF = 1.30f;
static const float COMBAT_NPC_STANDOFF = 1.30f;
static const float ROAD_BARRICADE_X = 51.85f;
/* Walkable tops from environment_bridge_checkpoint_v01. These must stay in
   lockstep with the exported deck and its two short causeways. */
static const Rectangle ROAD_BRIDGE_DECK_SUPPORT = {
    48.05f, 38.725f, 7.60f, 2.55f
};
static const Rectangle ROAD_BRIDGE_WEST_CAUSEWAY_SUPPORT = {
    47.15f, 38.81f, 0.96f, 2.38f
};
static const Rectangle ROAD_BRIDGE_EAST_CAUSEWAY_SUPPORT = {
    55.59f, 38.81f, 0.96f, 2.38f
};
static const float CARRIAGE_ASSET_SCALE = 0.92f;
/* The exported carriage's hitch points along local +X. The street bay runs
   +Z, while the encounter road already runs +X. */
static const float CARRIAGE_ASSET_STREET_YAW_DEGREES = -90.0f;
static bool draw_hero_rig_debug = false;

typedef enum BridgeCheckpointStatus {
    BRIDGE_CHECKPOINT_UNKNOWN,
    BRIDGE_CHECKPOINT_AVAILABLE,
    BRIDGE_CHECKPOINT_UNAVAILABLE
} BridgeCheckpointStatus;

static BridgeCheckpointStatus bridge_checkpoint_status =
    BRIDGE_CHECKPOINT_UNKNOWN;

static bool RoadUsesAuthoredCheckpoint(void);
static void DrawBox(Vector3 center, Vector3 size, Color color);
static void DrawCharacterSphere(Vector3 center, float radius, Color color);
static void DrawCharacterEllipsoid(Vector3 center, Vector3 radius, Color color);
static Vector3 LocalPoint(Vector3 base, float x, float y, float z, float yaw);
static void DrawOrientedBox(Vector3 base, Vector3 local_center, Vector3 size,
                            float yaw, Color color);
static Color ShadeColor(Color color, float scale);
static Color BlendColor(Color from, Color to, float amount);
typedef struct WorldLabel {
    Vector3 point;
    const char *text;
    Color color;
} WorldLabel;

typedef struct FaceRenderContext {
    Camera3D camera;
    int32_t width;
    int32_t height;
    bool valid;
} FaceRenderContext;

static FaceRenderContext face_render_context = {0};

typedef struct WorldBuilding {
    Rectangle footprint;
    float height;
    int32_t style;
    bool door;
} WorldBuilding;

/* One world unit is one metre. Even the smallest cottage has room for an
   actual household around the 1.9 m hero; the market is a 12 x 10 m hall. */
static const WorldBuilding WORLD_BUILDINGS[] = {
    {{20.0f, 15.0f, 8.0f, 10.0f}, 7.20f, 0, true},
    {{32.0f, 14.0f, 7.0f, 9.0f}, 6.40f, 1, true},
    {{44.0f, 16.0f, 12.0f, 10.0f}, 8.80f, 2, true},
    {{20.0f, 33.0f, 10.0f, 8.0f}, 6.60f, 3, true},
    {{34.0f, 35.0f, 7.0f, 8.0f}, 5.80f, 0, true},
    {{55.0f, 42.0f, 8.0f, 7.0f}, 6.20f, 1, true},
    {{55.0f, 29.0f, 6.5f, 8.5f}, 5.90f, 0, true},
    {{55.0f, 14.0f, 7.0f, 9.0f}, 6.10f, 3, true},
    {{32.0f, 47.0f, 8.0f, 7.0f}, 5.80f, 1, true},
    {{44.0f, 46.0f, 10.0f, 7.0f}, 6.90f, 2, true}
};

typedef struct WorldStructure {
    Rectangle footprint;
    float height;
} WorldStructure;

/* A 25 x 23 m fortified bailey, with a person-width gate left physically open. */
static const WorldStructure CASTLE_STRUCTURES[] = {
    {{66.0f, 9.0f, 1.2f, 23.0f}, 6.5f},
    {{90.0f, 9.0f, 1.2f, 23.0f}, 6.5f},
    {{66.0f, 9.0f, 25.2f, 1.2f}, 6.5f},
    {{66.0f, 30.8f, 9.5f, 1.2f}, 6.5f},
    {{81.5f, 30.8f, 9.7f, 1.2f}, 6.5f},
    {{72.0f, 13.0f, 13.0f, 9.0f}, 11.5f},
    {{75.3f, 27.8f, 2.0f, 3.0f}, 9.0f},
    {{79.7f, 27.8f, 2.0f, 3.0f}, 9.0f},
    {{64.8f, 7.8f, 4.0f, 4.0f}, 12.5f},
    {{88.4f, 7.8f, 4.0f, 4.0f}, 12.5f},
    {{64.8f, 29.2f, 4.0f, 4.0f}, 12.5f},
    {{88.4f, 29.2f, 4.0f, 4.0f}, 12.5f}
};
static const Rectangle CARRIAGE_FOOTPRINT = {35.20f, 29.00f, 3.20f, 5.40f};
static const Rectangle DUNGEON_FOOTPRINT = {27.40f, 49.70f, 3.20f, 1.60f};
/* Set-dressing with a grounded footprint participates in the same collision
   contract as authored buildings. Route markers remain at the edges of the
   walkable composition instead of becoming ghost geometry. */
static const Rectangle ROOM_ART_OBSTACLES[] = {
    {8.48f, 10.38f, 0.36f, 0.36f},
    {14.16f, 10.38f, 0.36f, 0.36f},
    {7.78f, 24.68f, 0.62f, 0.62f},
    {17.58f, 54.36f, 0.84f, 0.72f},
    {30.72f, 24.08f, 0.46f, 0.46f},
    {35.08f, 55.10f, 3.34f, 0.50f},
    {63.18f, 50.06f, 1.92f, 1.92f},
    {80.22f, 45.82f, 2.36f, 2.36f},
    /* Grounded base of the mine ore station drawn at (26.45, 54.35). */
    {25.725f, 53.825f, 1.45f, 1.05f},
};
static const Rectangle COURSE_POOL = {10.00f, 9.05f, 2.55f, 1.38f};
static const Rectangle COURSE_SCENERY_FOOTPRINT = {
    8.80f, 0.70f, 5.90f, 9.90f
};
static const float COURSE_WATER_SURFACE = 0.82f;
static const Rectangle MARKET_COUNTER_FOOTPRINT = {6.05f, 1.84f, 2.10f, 0.72f};
static const Rectangle MARKET_SHELF_FOOTPRINT = {1.10f, 1.175f, 0.72f, 3.45f};
static const Rectangle ROAD_OBSTACLES[] = {
    /* Authored carriage body and its hitched horse team. */
    {36.20f, 38.68f, 4.30f, 2.64f},
    {40.48f, 38.48f, 3.62f, 3.04f},
    {33.70f, 42.08f, 0.80f, 0.76f},
    {34.58f, 42.10f, 0.88f, 0.68f},
    /* Authored bridge checkpoint: parapets leave a 2.5 m travel lane. */
    {47.94f, 38.45f, 7.82f, 0.38f},
    {47.94f, 41.17f, 7.82f, 0.38f},
    {49.54f, 37.20f, 1.72f, 1.36f},
    {50.44f, 40.04f, 1.39f, 0.96f},
    {52.36f, 38.86f, 0.28f, 0.28f},
    {52.36f, 40.86f, 0.28f, 0.28f}
};
static const Rectangle ROAD_FALLBACK_OBSTACLES[] = {
    /* Procedural carriage fallback uses the same physical convoy envelope. */
    {36.20f, 38.68f, 4.30f, 2.64f},
    {40.48f, 38.48f, 3.62f, 3.04f},
    {33.70f, 42.08f, 0.80f, 0.76f},
    {34.58f, 42.10f, 0.88f, 0.68f},
    {50.55f, 37.18f, 0.84f, 0.84f},
    {52.22f, 41.92f, 1.02f, 0.80f},
    {51.67f, 36.98f, 0.36f, 0.54f},
    {51.67f, 42.47f, 0.36f, 0.56f}
};
static const Vector2 STREET_PEOPLE[] = {
    {50.50f, 28.80f}, {42.00f, 30.80f}, {37.00f, 44.00f},
    {29.00f, 28.00f}, {52.00f, 31.00f}, {58.00f, 26.50f}
};
static const Vector2 MARKET_PEOPLE[] = {{6.55f, 1.60f}};

/* Outdoor play is staged as a sequence of composed rooms. Trigger points live
   on reachable paths; camera targets may instead favor a landmark such as the
   market hall or keep. Keeping those roles separate prevents a composition
   point inside architecture from becoming an unreachable shot transition. */
typedef enum ArtLightProfileId {
    ART_LIGHT_CLEAR_MARKET = 0,
    ART_LIGHT_SHORTAGE_OVERCAST,
    ART_LIGHT_RECOVERY_WARM,
    ART_LIGHT_ROAD_DUSK,
    ART_LIGHT_INTERIOR_EMBER,
    ART_LIGHT_PROFILE_COUNT
} ArtLightProfileId;

typedef struct ArtComposition {
    Vector3 focal_point;
    Vector2 story_axis;
    Vector3 foreground_anchor;
    Rectangle quiet_area;
    Vector3 depth_splits;
    ArtLightProfileId light_profile;
} ArtComposition;

typedef struct ArtLightProfileDefinition {
    Vector3 light_direction;
    Vector3 light_color;
    Vector3 ambient_color;
    Vector3 shadow_color;
    Color fog_color;
    float fog_near;
    float fog_far;
    float depth_strength;
    float focal_contrast;
} ArtLightProfileDefinition;

typedef struct ArtAtmosphereDefinition {
    Vector3 light_direction;
    Vector3 light_tint;
    Vector3 ambient_tint;
    Vector3 shadow_tint;
    Color fog_color;
    float direction_influence;
    float fog_influence;
    float fog_distance_scale;
    float depth_scale;
    float focal_scale;
    float grade_exposure;
    float grade_shadow_tone;
    float grade_highlight_tone;
    float grade_chroma;
    float rain;
    float mist;
    float wetness;
    float omen;
} ArtAtmosphereDefinition;

typedef struct ArtAtmosphereState {
    ArtAtmosphereDefinition from;
    CcLocalAtmospherePreset target;
    float blend;
    float duration;
} ArtAtmosphereState;

static const ArtLightProfileDefinition ART_LIGHT_PROFILES[] = {
    [ART_LIGHT_CLEAR_MARKET] = {
        {-0.42f, 0.84f, 0.34f}, {1.12f, 0.96f, 0.78f},
        {0.62f, 0.66f, 0.69f}, {0.82f, 0.91f, 1.04f},
        {18, 34, 39, 255}, 26.0f, 55.0f, 0.70f, 0.12f,
    },
    [ART_LIGHT_SHORTAGE_OVERCAST] = {
        {-0.20f, 0.92f, 0.33f}, {1.00f, 1.02f, 1.06f},
        {0.69f, 0.74f, 0.78f}, {0.78f, 0.87f, 1.04f},
        {24, 38, 47, 255}, 24.0f, 52.0f, 0.78f, 0.10f,
    },
    [ART_LIGHT_RECOVERY_WARM] = {
        {-0.56f, 0.72f, 0.40f}, {1.20f, 0.91f, 0.65f},
        {0.65f, 0.63f, 0.62f}, {0.84f, 0.91f, 1.01f},
        {22, 37, 38, 255}, 27.0f, 56.0f, 0.64f, 0.15f,
    },
    [ART_LIGHT_ROAD_DUSK] = {
        {-0.63f, 0.55f, 0.31f}, {1.26f, 0.76f, 0.50f},
        {0.57f, 0.62f, 0.69f}, {0.74f, 0.82f, 1.06f},
        {9, 20, 25, 255}, 18.0f, 38.0f, 0.84f, 0.18f,
    },
    [ART_LIGHT_INTERIOR_EMBER] = {
        {-0.38f, 0.52f, 0.76f}, {1.24f, 0.74f, 0.44f},
        {0.55f, 0.46f, 0.48f}, {0.88f, 0.75f, 0.82f},
        {31, 23, 25, 255}, 10.0f, 21.0f, 0.52f, 0.17f,
    },
};

/* These are complete art recipes rather than physical sky simulations.
   Their job is to keep gameplay shapes readable while giving travel and day
   changes a strong emotional beat. */
static const ArtAtmosphereDefinition ART_ATMOSPHERES[] = {
    [CC_LOCAL_ATMOSPHERE_CLEAR_DAY] = {
        {-0.42f, 0.84f, 0.34f}, {1.00f, 1.00f, 1.00f},
        {1.00f, 1.00f, 1.00f}, {1.00f, 1.00f, 1.00f},
        {47, 44, 61, 255}, 0.10f, 0.10f, 1.00f, 1.00f, 1.00f,
        0.000f, 1.00f, 1.00f, 1.00f, 0.00f, 0.00f, 0.00f, 0.00f,
    },
    [CC_LOCAL_ATMOSPHERE_RAINY_OVERCAST] = {
        {-0.20f, 0.92f, 0.22f}, {0.78f, 0.86f, 0.98f},
        {0.90f, 0.97f, 1.07f}, {0.96f, 0.90f, 1.10f},
        {48, 52, 63, 255}, 0.70f, 0.74f, 0.70f, 1.14f, 0.88f,
        -0.035f, 1.34f, 0.38f, 0.84f, 0.82f, 0.36f, 0.82f, 0.00f,
    },
    [CC_LOCAL_ATMOSPHERE_AMBER_DUSK] = {
        {-0.70f, 0.36f, 0.30f}, {1.24f, 0.72f, 0.46f},
        {0.84f, 0.78f, 0.92f}, {1.05f, 0.82f, 1.12f},
        {53, 35, 63, 255}, 0.82f, 0.48f, 0.82f, 1.08f, 1.08f,
        -0.020f, 1.36f, 1.58f, 1.05f, 0.00f, 0.10f, 0.00f, 0.00f,
    },
    [CC_LOCAL_ATMOSPHERE_MOONLIT_NIGHT] = {
        {-0.30f, 0.48f, -0.82f}, {0.72f, 0.82f, 1.06f},
        {0.78f, 0.82f, 0.98f}, {0.86f, 0.88f, 1.08f},
        {38, 31, 49, 255}, 0.82f, 0.58f, 0.82f, 1.12f, 1.12f,
        -0.045f, 1.42f, 0.62f, 0.84f, 0.00f, 0.18f, 0.08f, 0.00f,
    },
    [CC_LOCAL_ATMOSPHERE_DRAGON_OMEN] = {
        {-0.58f, 0.42f, 0.18f}, {0.88f, 0.68f, 0.72f},
        {0.70f, 0.72f, 0.82f}, {0.88f, 0.78f, 1.08f},
        {48, 44, 57, 255}, 0.78f, 0.68f, 0.74f, 1.18f, 1.12f,
        -0.040f, 1.46f, 0.92f, 0.76f, 0.38f, 0.66f, 0.50f, 1.00f,
    },
};

static ArtAtmosphereState art_atmosphere = {
    .target = CC_LOCAL_ATMOSPHERE_CLEAR_DAY,
    .blend = 1.0f,
    .duration = 1.0f,
};

static float ArtAtmosphereClamp(float value)
{
    return fmaxf(0.0f, fminf(value, 1.0f));
}

static float ArtAtmosphereMixFloat(float from, float to, float amount)
{
    return from + (to - from) * amount;
}

static Vector3 ArtAtmosphereMixVector(Vector3 from, Vector3 to, float amount)
{
    return (Vector3){
        ArtAtmosphereMixFloat(from.x, to.x, amount),
        ArtAtmosphereMixFloat(from.y, to.y, amount),
        ArtAtmosphereMixFloat(from.z, to.z, amount),
    };
}

static Color ArtAtmosphereMixColor(Color from, Color to, float amount)
{
    return (Color){
        (unsigned char)roundf(ArtAtmosphereMixFloat(
            (float)from.r, (float)to.r, amount)),
        (unsigned char)roundf(ArtAtmosphereMixFloat(
            (float)from.g, (float)to.g, amount)),
        (unsigned char)roundf(ArtAtmosphereMixFloat(
            (float)from.b, (float)to.b, amount)),
        255,
    };
}

static ArtAtmosphereDefinition ArtAtmosphereMix(
    ArtAtmosphereDefinition from, ArtAtmosphereDefinition to, float amount)
{
    amount = ArtAtmosphereClamp(amount);
    return (ArtAtmosphereDefinition){
        .light_direction = ArtAtmosphereMixVector(
            from.light_direction, to.light_direction, amount),
        .light_tint = ArtAtmosphereMixVector(
            from.light_tint, to.light_tint, amount),
        .ambient_tint = ArtAtmosphereMixVector(
            from.ambient_tint, to.ambient_tint, amount),
        .shadow_tint = ArtAtmosphereMixVector(
            from.shadow_tint, to.shadow_tint, amount),
        .fog_color = ArtAtmosphereMixColor(
            from.fog_color, to.fog_color, amount),
        .direction_influence = ArtAtmosphereMixFloat(
            from.direction_influence, to.direction_influence, amount),
        .fog_influence = ArtAtmosphereMixFloat(
            from.fog_influence, to.fog_influence, amount),
        .fog_distance_scale = ArtAtmosphereMixFloat(
            from.fog_distance_scale, to.fog_distance_scale, amount),
        .depth_scale = ArtAtmosphereMixFloat(
            from.depth_scale, to.depth_scale, amount),
        .focal_scale = ArtAtmosphereMixFloat(
            from.focal_scale, to.focal_scale, amount),
        .grade_exposure = ArtAtmosphereMixFloat(
            from.grade_exposure, to.grade_exposure, amount),
        .grade_shadow_tone = ArtAtmosphereMixFloat(
            from.grade_shadow_tone, to.grade_shadow_tone, amount),
        .grade_highlight_tone = ArtAtmosphereMixFloat(
            from.grade_highlight_tone, to.grade_highlight_tone, amount),
        .grade_chroma = ArtAtmosphereMixFloat(
            from.grade_chroma, to.grade_chroma, amount),
        .rain = ArtAtmosphereMixFloat(from.rain, to.rain, amount),
        .mist = ArtAtmosphereMixFloat(from.mist, to.mist, amount),
        .wetness = ArtAtmosphereMixFloat(
            from.wetness, to.wetness, amount),
        .omen = ArtAtmosphereMixFloat(from.omen, to.omen, amount),
    };
}

static ArtAtmosphereDefinition ArtAtmosphereCurrent(void)
{
    int32_t target = (int32_t)art_atmosphere.target;
    if (target < 0 || target >= CC_LOCAL_ATMOSPHERE_COUNT) {
        target = CC_LOCAL_ATMOSPHERE_CLEAR_DAY;
    }
    float amount = ArtAtmosphereClamp(art_atmosphere.blend);
    amount = amount * amount * (3.0f - 2.0f * amount);
    return ArtAtmosphereMix(art_atmosphere.from,
                            ART_ATMOSPHERES[target], amount);
}

static ArtAtmosphereDefinition ArtAtmosphereForProfile(
    ArtLightProfileId profile_id)
{
    ArtAtmosphereDefinition current = ArtAtmosphereCurrent();
    if (profile_id != ART_LIGHT_INTERIOR_EMBER) return current;
    return ArtAtmosphereMix(
        ART_ATMOSPHERES[CC_LOCAL_ATMOSPHERE_CLEAR_DAY], current, 0.16f);
}

void CcLocalRendererSetAtmosphere(CcLocalAtmospherePreset preset,
                                  float transition_seconds)
{
    if (preset < 0 || preset >= CC_LOCAL_ATMOSPHERE_COUNT) {
        preset = CC_LOCAL_ATMOSPHERE_CLEAR_DAY;
    }
    if (preset == art_atmosphere.target) return;
    ArtAtmosphereDefinition current = ArtAtmosphereCurrent();
    art_atmosphere.from = current;
    art_atmosphere.target = preset;
    art_atmosphere.duration = fmaxf(0.0f, transition_seconds);
    art_atmosphere.blend = transition_seconds <= 0.001f ? 1.0f : 0.0f;
}

void CcLocalRendererUpdateAtmosphere(float delta_time)
{
    if (art_atmosphere.blend >= 1.0f) return;
    float duration = fmaxf(0.001f, art_atmosphere.duration);
    art_atmosphere.blend = fminf(
        1.0f, art_atmosphere.blend + fmaxf(0.0f, delta_time) / duration);
}

const char *CcLocalAtmosphereName(CcLocalAtmospherePreset preset)
{
    switch (preset) {
        case CC_LOCAL_ATMOSPHERE_CLEAR_DAY: return "Clear day";
        case CC_LOCAL_ATMOSPHERE_RAINY_OVERCAST: return "Rain";
        case CC_LOCAL_ATMOSPHERE_AMBER_DUSK: return "Dusk";
        case CC_LOCAL_ATMOSPHERE_MOONLIT_NIGHT: return "Night";
        case CC_LOCAL_ATMOSPHERE_DRAGON_OMEN: return "Dragon omen";
        default: return "Clear day";
    }
}

typedef struct StreetCameraShot {
    Vector2 trigger;
    Vector3 target;
    const char *name;
    Rectangle route;
    int32_t route_palette;
    Vector3 camera_offset;
    float fovy;
    ArtComposition art;
} StreetCameraShot;

static const StreetCameraShot STREET_CAMERA_SHOTS[] = {
    {
        .trigger = {10.5f, 7.5f}, .target = {10.5f, 1.05f, 7.5f},
        .name = "TRAINING YARD", .route = {8.6f, 10.2f, 6.0f, 3.0f},
        .route_palette = 2, .camera_offset = {6.0f, 7.0f, 22.0f},
        .fovy = 10.6f,
        .art = {{10.5f, 1.05f, 7.5f}, {0.0f, 1.0f},
                {5.8f, 0.0f, 14.5f}, {0.20f, 0.18f, 0.60f, 0.64f},
                {15.0f, 23.0f, 38.0f}, ART_LIGHT_CLEAR_MARKET},
    },
    {
        .trigger = {11.0f, 28.5f}, .target = {12.5f, 1.05f, 28.5f},
        .name = "WEST FARMS", .route = {12.3f, 26.3f, 4.8f, 5.8f},
        .route_palette = 0, .camera_offset = {-5.0f, 8.0f, 23.0f},
        .fovy = 11.8f,
        .art = {{12.5f, 1.05f, 28.5f}, {0.0f, 1.0f},
                {7.2f, 0.0f, 36.0f}, {0.22f, 0.18f, 0.58f, 0.64f},
                {16.0f, 25.0f, 41.0f}, ART_LIGHT_CLEAR_MARKET},
    },
    {
        .trigger = {14.0f, 52.0f}, .target = {25.0f, 1.38f, 52.0f},
        .name = "OLD MINE ROAD", .route = {14.0f, 54.2f, 28.0f, 2.7f},
        .route_palette = 3, .camera_offset = {-7.0f, 9.2f, 25.0f},
        .fovy = 15.2f,
        .art = {{25.0f, 1.38f, 52.0f}, {1.0f, 0.0f},
                {9.0f, 0.0f, 58.0f}, {0.18f, 0.17f, 0.62f, 0.66f},
                {16.0f, 27.0f, 44.0f}, ART_LIGHT_SHORTAGE_OVERCAST},
    },
    {
        .trigger = {33.0f, 25.0f}, .target = {33.0f, 1.05f, 25.0f},
        .name = "WORKSHOP STREET", .route = {29.0f, 23.3f, 13.0f, 2.6f},
        .route_palette = 1, .camera_offset = {10.0f, 7.6f, 22.0f},
        .fovy = 12.5f,
        .art = {{33.0f, 1.05f, 25.0f}, {1.0f, 0.0f},
                {43.0f, 0.0f, 31.0f}, {0.20f, 0.18f, 0.60f, 0.64f},
                {15.0f, 25.0f, 41.0f}, ART_LIGHT_CLEAR_MARKET},
    },
    {
        .trigger = {44.0f, 29.0f}, .target = {44.5f, 1.05f, 29.5f},
        .name = "TOWN SQUARE", .route = {0.0f, 0.0f, 0.0f, 0.0f},
        .route_palette = 1, .camera_offset = {-17.0f, 11.0f, 23.0f},
        .fovy = 17.8f,
        .art = {{44.5f, 1.05f, 29.5f}, {0.98f, 0.20f},
                {39.0f, 0.0f, 39.0f}, {0.19f, 0.18f, 0.61f, 0.64f},
                {18.0f, 29.0f, 47.0f}, ART_LIGHT_CLEAR_MARKET},
    },
    {
        .trigger = {42.0f, 52.0f}, .target = {41.5f, 1.05f, 52.0f},
        .name = "CARRIAGE YARD", .route = {39.5f, 53.8f, 4.9f, 4.2f},
        .route_palette = 0, .camera_offset = {8.0f, 9.5f, 27.0f},
        .fovy = 15.2f,
        .art = {{41.5f, 1.05f, 52.0f}, {0.0f, 1.0f},
                {51.0f, 0.0f, 59.0f}, {0.20f, 0.18f, 0.60f, 0.64f},
                {18.0f, 29.0f, 47.0f}, ART_LIGHT_CLEAR_MARKET},
    },
    {
        .trigger = {50.0f, 27.25f}, .target = {57.5f, 2.00f, 28.0f},
        .name = "MARKET STEPS", .route = {47.0f, 25.25f, 6.0f, 1.38f},
        .route_palette = 1, .camera_offset = {-12.5f, 7.2f, 31.0f},
        .fovy = 18.0f,
        .art = {{50.0f, 3.10f, 21.0f}, {0.0f, -1.0f},
                {42.0f, 0.0f, 37.0f}, {0.17f, 0.17f, 0.63f, 0.66f},
                {22.0f, 35.0f, 54.0f}, ART_LIGHT_CLEAR_MARKET},
    },
    {
        .trigger = {58.0f, 50.0f}, .target = {61.2f, 1.05f, 50.9f},
        .name = "MILLER'S ROAD", .route = {54.2f, 50.1f, 18.7f, 2.9f},
        .route_palette = 0, .camera_offset = {6.0f, 9.0f, 27.0f},
        .fovy = 15.6f,
        .art = {{61.2f, 1.05f, 50.9f}, {1.0f, 0.0f},
                {68.0f, 0.0f, 58.0f}, {0.20f, 0.18f, 0.60f, 0.64f},
                {18.0f, 29.0f, 47.0f}, ART_LIGHT_RECOVERY_WARM},
    },
    {
        .trigger = {78.5f, 29.0f}, .target = {78.5f, 2.00f, 30.2f},
        .name = "CROWN GATE", .route = {75.4f, 27.0f, 6.2f, 5.2f},
        .route_palette = 2, .camera_offset = {-11.5f, 13.5f, 30.0f},
        .fovy = 16.8f,
        .art = {{78.5f, 4.40f, 23.0f}, {0.0f, -1.0f},
                {66.0f, 0.0f, 38.0f}, {0.18f, 0.17f, 0.62f, 0.66f},
                {23.0f, 37.0f, 57.0f}, ART_LIGHT_CLEAR_MARKET},
    },
    {
        .trigger = {78.0f, 50.0f}, .target = {78.0f, 1.05f, 50.0f},
        .name = "EAST FIELDS", .route = {76.4f, 31.3f, 3.7f, 22.7f},
        .route_palette = 0, .camera_offset = {8.0f, 9.0f, 24.0f},
        .fovy = 13.0f,
        .art = {{78.0f, 1.05f, 50.0f}, {0.0f, 1.0f},
                {87.0f, 0.0f, 58.0f}, {0.20f, 0.18f, 0.60f, 0.64f},
                {16.0f, 27.0f, 44.0f}, ART_LIGHT_RECOVERY_WARM},
    },
};

/* The long road between Market Steps and Crown Gate needs its own establishing
   view. It is deliberately higher and wider than either room, holding both
   the departing house line and the gate approach without slicing through a
   foreground facade. It is a camera composition only, never a logical room. */
static const StreetCameraShot MARKET_GATE_ROAD_CAMERA = {
    .trigger = {64.0f, 31.0f}, .target = {66.0f, 2.0f, 31.0f},
    .name = "MARKET ROAD", .route = {0.0f, 0.0f, 0.0f, 0.0f},
    .route_palette = 2, .camera_offset = {-13.0f, 11.0f, 33.0f},
    .fovy = 21.0f,
    .art = {{77.5f, 4.20f, 23.5f}, {1.0f, 0.0f},
            {54.0f, 0.0f, 39.0f}, {0.16f, 0.16f, 0.64f, 0.68f},
            {24.0f, 39.0f, 60.0f}, ART_LIGHT_ROAD_DUSK},
};

#define MARKET_GATE_ROAD_CAMERA_SHOT \
    ((int32_t)(sizeof(STREET_CAMERA_SHOTS) / \
               sizeof(STREET_CAMERA_SHOTS[0])))

#define STREET_ALLEY_CAMERA_SHOT_BASE \
    (MARKET_GATE_ROAD_CAMERA_SHOT + 1)

static bool StreetCameraShotIsAlley(int32_t shot)
{
    return shot >= STREET_ALLEY_CAMERA_SHOT_BASE &&
           shot <= STREET_ALLEY_CAMERA_SHOT_BASE +
                   MARKET_GATE_ROAD_CAMERA_SHOT;
}

static int32_t StreetCameraBaseShot(int32_t shot)
{
    return StreetCameraShotIsAlley(shot) ?
        shot - STREET_ALLEY_CAMERA_SHOT_BASE : shot;
}

static int32_t StreetCameraAlleyShot(int32_t shot)
{
    return STREET_ALLEY_CAMERA_SHOT_BASE + StreetCameraBaseShot(shot);
}

static const ArtComposition ROAD_ART_COMPOSITION = {
    {49.0f, 1.10f, 40.0f}, {1.0f, 0.0f}, {36.0f, 0.0f, 44.0f},
    {0.17f, 0.17f, 0.64f, 0.66f}, {8.0f, 16.0f, 29.0f},
    ART_LIGHT_ROAD_DUSK,
};

static const ArtComposition INTERIOR_ART_COMPOSITION = {
    {4.48f, 1.05f, 2.36f}, {0.0f, -1.0f}, {1.55f, 1.05f, 6.54f},
    {0.18f, 0.16f, 0.64f, 0.68f}, {7.5f, 13.0f, 19.0f},
    ART_LIGHT_INTERIOR_EMBER,
};

#define STREET_TRAVERSAL_VIA_CAPACITY 4

typedef struct StreetTraversalLink {
    int32_t room_a;
    int32_t room_b;
    Vector2 via[STREET_TRAVERSAL_VIA_CAPACITY];
    int32_t via_count;
} StreetTraversalLink;

typedef struct StreetBoundaryExit {
    int32_t room;
    Vector2 endpoint;
    Vector2 via[STREET_TRAVERSAL_VIA_CAPACITY];
    int32_t via_count;
    const char *name;
} StreetBoundaryExit;

/* Camera rooms are connected by physical corridors. Via points keep travel
   on authored roads and through the keep gate instead of asking the local
   collision solver to invent a route around whole buildings. */
static const StreetTraversalLink STREET_TRAVERSAL_LINKS[] = {
    {0, 3, {{9.2f, 7.5f}, {9.2f, 11.6f},
            {42.0f, 11.6f}, {42.0f, 25.0f}}, 4},
    {1, 3, {{15.8f, 29.0f}, {29.0f, 29.0f}}, 2},
    {2, 5, {{20.0f, 56.2f}, {34.3f, 56.2f}, {39.2f, 56.2f}}, 3},
    {3, 4, {{33.0f, 27.5f}, {40.5f, 27.5f}, {42.5f, 28.5f}}, 3},
    {4, 5, {{42.0f, 36.0f}, {42.0f, 46.0f}}, 2},
    {4, 6, {{47.0f, 28.2f}}, 1},
    {5, 7, {{42.0f, 55.0f}, {54.8f, 55.0f}, {54.8f, 50.0f}}, 3},
    {6, 8, {{63.8f, 27.5f}, {64.4f, 38.0f},
            {78.5f, 38.0f}, {78.5f, 34.0f}}, 4},
    {7, 9, {{68.0f, 51.5f}, {76.0f, 51.5f}}, 2},
    {8, 9, {{78.5f, 38.0f}, {78.5f, 45.0f}}, 2},
};

static const StreetBoundaryExit STREET_BOUNDARY_EXITS[] = {
    {1, {1.0f, 29.0f}, {{7.0f, 29.0f}}, 1, "WESTERN ROAD"},
    {2, {1.0f, 55.4f}, {{8.0f, 55.4f}}, 1, "OLD MINE TRACK"},
    {8, {94.8f, 29.0f},
     {{78.5f, 34.0f}, {93.0f, 34.0f}, {94.8f, 33.0f}}, 3,
     "EASTERN KING'S ROAD"},
    {9, {78.0f, 70.8f}, {{78.0f, 61.0f}}, 1, "NORTH FIELD ROAD"},
};

typedef struct FixedCameraRig {
    Vector3 displayed_target;
    Vector3 transition_from;
    Vector3 destination;
    Vector3 displayed_offset;
    Vector3 offset_transition_from;
    Vector3 offset_destination;
    float displayed_fovy;
    float fovy_transition_from;
    float fovy_destination;
    float transition_elapsed;
    float transition_duration;
    Vector3 framing_offset;
    Vector3 framing_from;
    Vector3 framing_destination;
    float framing_elapsed;
    float framing_duration;
    float framing_hold_seconds;
    float delta_time;
    float last_clock;
    int32_t shot;
    bool initialized;
} FixedCameraRig;

typedef struct CombatCameraRig {
    Vector3 displayed_target;
    Vector3 displayed_offset;
    float displayed_fovy;
    Vector3 locked_target;
    Vector3 locked_offset;
    Vector3 locked_shoulder_side;
    float locked_fovy;
    float combat_weight;
    float tree_clear_angle;
    float reframe_cooldown;
    float last_clock;
    int32_t opponent_index;
    CcLocalSceneKind scene;
    bool composition_locked;
    bool shoulder_valid;
    bool initialized;
    bool scene_valid;
    bool road_encounter;
} CombatCameraRig;

typedef struct PresentedCameraState {
    Camera3D camera;
    Vector3 hero_position;
    int32_t width;
    int32_t height;
    bool valid;
} PresentedCameraState;

typedef enum TreeFamily {
    TREE_FAMILY_ALDER = 0,
    TREE_FAMILY_OAK,
    TREE_FAMILY_POLLARD
} TreeFamily;

typedef struct WorldTreePlacement {
    Vector2 position;
    TreeFamily family;
} WorldTreePlacement;

/* Camera visibility and scenery drawing share this one authored tree list.
   Keeping the roots in one place prevents a clear shot from drifting out of
   sync with what the renderer actually puts in front of the cast. */
static const WorldTreePlacement WORLD_TREES[] = {
    {{2.5f, 58.0f}, TREE_FAMILY_ALDER},
    {{6.0f, 62.0f}, TREE_FAMILY_ALDER},
    {{10.0f, 56.0f}, TREE_FAMILY_POLLARD},
    {{14.0f, 65.0f}, TREE_FAMILY_OAK},
    {{18.0f, 58.0f}, TREE_FAMILY_ALDER},
    {{22.0f, 64.0f}, TREE_FAMILY_ALDER},
    {{27.0f, 60.0f}, TREE_FAMILY_ALDER},
    {{33.0f, 66.0f}, TREE_FAMILY_OAK},
    {{48.0f, 63.0f}, TREE_FAMILY_ALDER},
    {{54.0f, 59.0f}, TREE_FAMILY_ALDER},
    {{59.0f, 65.0f}, TREE_FAMILY_POLLARD},
    {{66.0f, 59.0f}, TREE_FAMILY_OAK},
    {{73.0f, 64.0f}, TREE_FAMILY_ALDER},
    {{84.0f, 59.5f}, TREE_FAMILY_ALDER},
    {{88.0f, 64.0f}, TREE_FAMILY_OAK},
    {{93.0f, 56.0f}, TREE_FAMILY_ALDER},
    {{18.0f, 5.0f}, TREE_FAMILY_ALDER},
    {{24.0f, 8.0f}, TREE_FAMILY_POLLARD},
    {{31.0f, 5.5f}, TREE_FAMILY_OAK},
    {{49.0f, 6.0f}, TREE_FAMILY_ALDER},
    {{57.0f, 5.0f}, TREE_FAMILY_POLLARD},
    {{62.0f, 3.5f}, TREE_FAMILY_ALDER},
    {{4.0f, 14.0f}, TREE_FAMILY_POLLARD},
    {{16.0f, 14.5f}, TREE_FAMILY_ALDER},
    {{17.0f, 45.0f}, TREE_FAMILY_POLLARD},
    {{20.0f, 52.0f}, TREE_FAMILY_ALDER},
    {{57.5f, 41.0f}, TREE_FAMILY_OAK},
    {{70.5f, 39.5f}, TREE_FAMILY_POLLARD},
    {{86.0f, 40.0f}, TREE_FAMILY_ALDER},
    {{93.0f, 36.0f}, TREE_FAMILY_OAK}
};

static FixedCameraRig street_camera_rig = {0};
static FixedCameraRig road_camera_rig = {0};
static CombatCameraRig combat_camera_rig = {0};
static PresentedCameraState presented_camera[3] = {0};

static void RememberPresentedCamera(CcLocalSceneKind scene, Camera3D camera,
                                    const CcLocalAgent *agent,
                                    int32_t width, int32_t height)
{
    if (scene < CC_LOCAL_SCENE_STREET || scene > CC_LOCAL_SCENE_ROAD ||
        width <= 0 || height <= 0) return;
    PresentedCameraState *state = &presented_camera[scene];
    state->camera = camera;
    state->hero_position = agent != NULL ? agent->position : (Vector3){0};
    state->width = width;
    state->height = height;
    state->valid = true;
}

static bool PresentedCameraForInput(CcLocalSceneKind scene,
                                    const CcLocalAgent *agent,
                                    int32_t width, int32_t height,
                                    Camera3D *camera)
{
    if (camera == NULL || scene < CC_LOCAL_SCENE_STREET ||
        scene > CC_LOCAL_SCENE_ROAD) return false;
    const PresentedCameraState *state = &presented_camera[scene];
    if (!state->valid || state->width != width || state->height != height) {
        return false;
    }
    if (agent != NULL) {
        float x = agent->position.x - state->hero_position.x;
        float z = agent->position.z - state->hero_position.z;
        if (x * x + z * z > 6.0f * 6.0f) return false;
    }
    *camera = state->camera;
    return true;
}

static bool CameraPointInFront(Camera3D camera, Vector3 point)
{
    Vector3 forward = Vector3Subtract(camera.target, camera.position);
    Vector3 toward = Vector3Subtract(point, camera.position);
    return Vector3DotProduct(forward, toward) > 0.001f;
}

typedef struct NavPlatform {
    float x;
    float z;
    float width;
    float depth;
    float height;
    int32_t style;
} NavPlatform;

static const NavPlatform STREET_PLATFORMS[] = {
    {3.00f, 7.00f, 1.00f, 1.00f, 1.65f, 0},
    {10.15f, 0.85f, 0.70f, 0.70f, 0.90f, 1},
    {11.75f, 1.45f, 1.05f, 1.05f, 1.05f, 2},
    {13.10f, 2.35f, 0.95f, 1.20f, 1.52f, 3},
    {10.20f, 3.55f, 2.70f, 0.62f, 1.65f, 2},
    {9.80f, 5.15f, 0.95f, 0.95f, 1.35f, 3},
    {11.25f, 5.70f, 0.82f, 0.82f, 0.92f, 1},
    {12.55f, 6.40f, 1.10f, 1.10f, 1.65f, 2},
    {9.85f, 7.80f, 2.60f, 1.00f, 0.90f, 1},
    {13.10f, 8.65f, 0.95f, 0.95f, 1.60f, 3}
};

/* Market stock is rendered as 0.62 m crates. Give the same boxes real top
   surfaces so a click climbs onto them instead of allowing the body to phase
   through purely visual inventory. */
static const NavPlatform STREET_CRATE_PLATFORMS[] = {
    {44.09f, 26.44f, 0.62f, 0.62f, 0.60f, 1},
    {44.81f, 26.44f, 0.62f, 0.62f, 0.60f, 1},
    {44.09f, 27.16f, 0.62f, 0.62f, 0.60f, 1},
    {44.81f, 27.16f, 0.62f, 0.62f, 0.60f, 1},
};
static int32_t street_crate_platform_count = 0;

void CcLocalSetStreetMarketCratesInternal(int32_t count)
{
    street_crate_platform_count = count < 0 ? 0 : count > 4 ? 4 : count;
}

static int32_t StreetPhysicsPlatformCount(void)
{
    return (int32_t)(sizeof(STREET_PLATFORMS) /
                     sizeof(STREET_PLATFORMS[0])) +
           street_crate_platform_count;
}

static const NavPlatform *StreetPhysicsPlatformAt(int32_t index)
{
    int32_t static_count = (int32_t)(sizeof(STREET_PLATFORMS) /
                                     sizeof(STREET_PLATFORMS[0]));
    if (index < 0) return NULL;
    if (index < static_count) return &STREET_PLATFORMS[index];
    index -= static_count;
    if (index >= street_crate_platform_count) return NULL;
    return &STREET_CRATE_PLATFORMS[index];
}

/* The exterior is one deterministic continuous land surface. Society only
   grades narrow roads and local foundations into it; it does not replace the
   country with a settlement-sized plane. Half-metre samples keep collision,
   foot placement, picking, and rendering on the exact same height field. */
#define CC_TERRAIN_CELL_SIZE 0.50f
#define CC_TERRAIN_COLUMNS 193
#define CC_TERRAIN_ROWS 145
#define CC_TERRAIN_SAMPLE_COUNT (CC_TERRAIN_COLUMNS * CC_TERRAIN_ROWS)

typedef struct TerrainRoad {
    Rectangle footprint;
    bool runs_east_west;
} TerrainRoad;

static const TerrainRoad TERRAIN_ROADS[] = {
    {{14.70f, 26.80f, 77.60f, 5.20f}, true},
    {{39.40f, 7.70f, 5.20f, 50.60f}, false},
    {{7.20f, 10.00f, 34.80f, 3.80f}, true},
    {{76.00f, 27.00f, 5.00f, 27.00f}, false},
    {{14.00f, 54.20f, 28.00f, 2.70f}, true},
    {{12.30f, 26.30f, 4.80f, 5.80f}, false},
    {{29.00f, 23.30f, 13.00f, 4.50f}, true},
    {{39.50f, 53.80f, 4.90f, 4.20f}, false},
    {{47.00f, 25.25f, 6.00f, 1.38f}, true},
    {{54.20f, 50.10f, 18.70f, 2.90f}, true},
    {{75.40f, 27.00f, 6.20f, 5.20f}, false},
    {{62.80f, 27.00f, 2.60f, 11.50f}, false},
    {{63.80f, 32.40f, 17.50f, 3.20f}, true},
    {{39.50f, 53.50f, 16.20f, 3.00f}, true},
    /* A broad but unpaved ingress keeps the eastern raid route traversable.
       TerrainPointOnRoad deliberately leaves this final record grass-colored
       so it reads as open rolling land instead of another civic plaza. */
    {{51.00f, 37.00f, 45.00f, 5.60f}, true},
};

static uint32_t street_terrain_seed = UINT32_C(0xc0a71a9e);
static bool street_terrain_ready = false;
static CcSettlementFunction active_place_function = CC_SETTLEMENT_MARKET;
static float street_terrain_natural[CC_TERRAIN_SAMPLE_COUNT];
static float street_terrain_height[CC_TERRAIN_SAMPLE_COUNT];

static const CcLocalPlaceLandmark *ActivePlaceLandmarkAt(int32_t index)
{
    return CcLocalPlaceLandmarkAt(active_place_function, index);
}

static const CcLocalPlaceRoad *ActivePlaceRoadAt(int32_t index)
{
    return CcLocalPlaceRoadAt(active_place_function, index);
}

static Rectangle PlaceLandmarkFootprint(
    const CcLocalPlaceLandmark *landmark)
{
    if (landmark == NULL) return (Rectangle){0};
    return (Rectangle){landmark->x, landmark->z,
                       landmark->width, landmark->depth};
}

static TerrainRoad PlaceTerrainRoad(const CcLocalPlaceRoad *road)
{
    if (road == NULL) return (TerrainRoad){0};
    return (TerrainRoad){
        {road->x, road->z, road->width, road->depth},
        road->runs_east_west,
    };
}

typedef struct TerrainRenderCache {
    Model model;
    uint32_t seed;
    int32_t hunger_band;
    int32_t prosperity_band;
    int32_t vertex_count;
    bool ready;
} TerrainRenderCache;

static TerrainRenderCache terrain_render_cache = {0};
static void TerrainRenderCacheClear(void);

static float TerrainClamp(float value, float minimum, float maximum)
{
    return fmaxf(minimum, fminf(maximum, value));
}

static float TerrainSmooth01(float amount)
{
    amount = TerrainClamp(amount, 0.0f, 1.0f);
    return amount * amount * (3.0f - 2.0f * amount);
}

static uint32_t TerrainMix(uint32_t value)
{
    value ^= value >> 16U;
    value *= UINT32_C(0x7feb352d);
    value ^= value >> 15U;
    value *= UINT32_C(0x846ca68b);
    return value ^ (value >> 16U);
}

static float TerrainLatticeValue(int32_t x, int32_t z, uint32_t stream)
{
    uint32_t sample = TerrainMix(
        street_terrain_seed ^ ((uint32_t)x * UINT32_C(0x9e3779b9)) ^
        ((uint32_t)z * UINT32_C(0x85ebca6b)) ^
        (stream * UINT32_C(0xc2b2ae35)));
    return (float)(sample & UINT32_C(0x00ffffff)) /
               (float)UINT32_C(0x00ffffff) *
               2.0f -
           1.0f;
}

static float TerrainValueNoise(float x, float z, float wavelength,
                               uint32_t stream)
{
    float sample_x = x / wavelength;
    float sample_z = z / wavelength;
    int32_t x0 = (int32_t)floorf(sample_x);
    int32_t z0 = (int32_t)floorf(sample_z);
    float tx = TerrainSmooth01(sample_x - (float)x0);
    float tz = TerrainSmooth01(sample_z - (float)z0);
    float bottom = TerrainLatticeValue(x0, z0, stream) +
        (TerrainLatticeValue(x0 + 1, z0, stream) -
         TerrainLatticeValue(x0, z0, stream)) *
            tx;
    float top = TerrainLatticeValue(x0, z0 + 1, stream) +
        (TerrainLatticeValue(x0 + 1, z0 + 1, stream) -
         TerrainLatticeValue(x0, z0 + 1, stream)) *
            tx;
    return bottom + (top - bottom) * tz;
}

static float TerrainNaturalHeight(float x, float z)
{
    float large = TerrainValueNoise(x, z, 38.0f, 1U) * 2.45f;
    float hills = TerrainValueNoise(x + 19.0f, z - 11.0f, 20.0f, 2U) * 1.35f;
    float detail = TerrainValueNoise(x - 7.0f, z + 23.0f, 8.0f, 3U) * 0.34f;
    float eastern_rise = TerrainSmooth01((x - 48.0f) / 44.0f) * 3.10f;
    float northern_rise = TerrainSmooth01((z - 30.0f) / 40.0f) * 1.20f;

    float valley_center = 30.0f + sinf(x * 0.075f + 0.45f) * 4.4f;
    float valley_distance = (z - valley_center) / 7.2f;
    float valley = -2.15f * expf(-valley_distance * valley_distance);

    float mine_x = (x - 25.0f) / 15.0f;
    float mine_z = (z - 51.0f) / 8.0f;
    float mine_ridge = 2.05f * expf(-(mine_x * mine_x + mine_z * mine_z));

    float keep_x = (x - 79.0f) / 17.0f;
    float keep_z = (z - 20.0f) / 15.0f;
    float keep_ridge = 1.85f * expf(-(keep_x * keep_x + keep_z * keep_z));
    return 1.65f + large + hills + detail + eastern_rise + northern_rise +
           valley + mine_ridge + keep_ridge;
}

static int32_t TerrainIndex(int32_t column, int32_t row)
{
    return row * CC_TERRAIN_COLUMNS + column;
}

static float TerrainSampleGrid(const float *samples, float x, float z)
{
    x = TerrainClamp(x, 0.0f, CC_LOCAL_WORLD_WIDTH);
    z = TerrainClamp(z, 0.0f, CC_LOCAL_WORLD_DEPTH);
    float grid_x = x / CC_TERRAIN_CELL_SIZE;
    float grid_z = z / CC_TERRAIN_CELL_SIZE;
    int32_t column = (int32_t)floorf(grid_x);
    int32_t row = (int32_t)floorf(grid_z);
    if (column >= CC_TERRAIN_COLUMNS - 1) column = CC_TERRAIN_COLUMNS - 2;
    if (row >= CC_TERRAIN_ROWS - 1) row = CC_TERRAIN_ROWS - 2;
    float tx = grid_x - (float)column;
    float tz = grid_z - (float)row;
    float h00 = samples[TerrainIndex(column, row)];
    float h10 = samples[TerrainIndex(column + 1, row)];
    float h01 = samples[TerrainIndex(column, row + 1)];
    float h11 = samples[TerrainIndex(column + 1, row + 1)];
    float bottom = h00 + (h10 - h00) * tx;
    float top = h01 + (h11 - h01) * tx;
    return bottom + (top - bottom) * tz;
}

static float TerrainDistanceToRectangle(float x, float z,
                                        Rectangle footprint)
{
    float closest_x = TerrainClamp(x, footprint.x,
                                   footprint.x + footprint.width);
    float closest_z = TerrainClamp(z, footprint.y,
                                   footprint.y + footprint.height);
    float dx = x - closest_x;
    float dz = z - closest_z;
    return sqrtf(dx * dx + dz * dz);
}

static float TerrainRoadProfile(const TerrainRoad *road, float x, float z)
{
    float total = 0.0f;
    for (int32_t offset = -2; offset <= 2; ++offset) {
        float sample_x = road->runs_east_west ?
            TerrainClamp(x + (float)offset, road->footprint.x,
                         road->footprint.x + road->footprint.width) :
            road->footprint.x + road->footprint.width * 0.5f;
        float sample_z = road->runs_east_west ?
            road->footprint.y + road->footprint.height * 0.5f :
            TerrainClamp(z + (float)offset, road->footprint.y,
                         road->footprint.y + road->footprint.height);
        total += TerrainSampleGrid(street_terrain_natural,
                                   sample_x, sample_z);
    }
    return total / 5.0f;
}

static bool TerrainPointInsideMajorFoundation(float x, float z)
{
    for (int32_t i = 0; i < (int32_t)(sizeof(WORLD_BUILDINGS) /
                                      sizeof(WORLD_BUILDINGS[0])); ++i) {
        Rectangle footprint = WORLD_BUILDINGS[i].footprint;
        if (x >= footprint.x && x <= footprint.x + footprint.width &&
            z >= footprint.y && z <= footprint.y + footprint.height) {
            return true;
        }
    }
    Rectangle keep = {65.20f, 8.20f, 26.50f, 24.40f};
    if (x >= keep.x && x <= keep.x + keep.width &&
        z >= keep.y && z <= keep.y + keep.height) return true;
    static const Rectangle local_pads[] = {
        {35.20f, 29.00f, 3.20f, 5.40f},
        {27.40f, 49.70f, 3.20f, 1.60f},
        {1.00f, 0.00f, 14.60f, 11.10f},
    };
    for (int32_t i = 0; i < (int32_t)(sizeof(local_pads) /
                                      sizeof(local_pads[0])); ++i) {
        Rectangle pad = local_pads[i];
        if (x >= pad.x && x <= pad.x + pad.width &&
            z >= pad.y && z <= pad.y + pad.height) return true;
    }
    for (int32_t i = 0; i < CC_LOCAL_PLACE_LANDMARK_COUNT; ++i) {
        Rectangle pad = PlaceLandmarkFootprint(ActivePlaceLandmarkAt(i));
        if (x >= pad.x && x <= pad.x + pad.width &&
            z >= pad.y && z <= pad.y + pad.height) return true;
    }
    return false;
}

static void TerrainGradeRoad(const TerrainRoad *road)
{
    const float shoulder = 1.25f;
    for (int32_t row = 0; row < CC_TERRAIN_ROWS; ++row) {
        float z = (float)row * CC_TERRAIN_CELL_SIZE;
        for (int32_t column = 0; column < CC_TERRAIN_COLUMNS; ++column) {
            float x = (float)column * CC_TERRAIN_CELL_SIZE;
            if (TerrainPointInsideMajorFoundation(x, z)) continue;
            float distance = TerrainDistanceToRectangle(
                x, z, road->footprint);
            if (distance >= shoulder) continue;
            bool inside = x >= road->footprint.x &&
                x <= road->footprint.x + road->footprint.width &&
                z >= road->footprint.y &&
                z <= road->footprint.y + road->footprint.height;
            float weight = inside ? 0.88f :
                0.88f * TerrainSmooth01(1.0f - distance / shoulder);
            int32_t index = TerrainIndex(column, row);
            float profile = TerrainRoadProfile(road, x, z);
            street_terrain_height[index] +=
                (profile - street_terrain_height[index]) * weight;
        }
    }
}

static void TerrainGradeActivePlaceRoads(void)
{
    for (int32_t road = 0; road < CC_LOCAL_PLACE_ROAD_COUNT; ++road) {
        TerrainRoad terrain_road = PlaceTerrainRoad(ActivePlaceRoadAt(road));
        TerrainGradeRoad(&terrain_road);
    }
}

static float TerrainRectangleAverage(Rectangle footprint)
{
    static const Vector2 samples[] = {
        {0.50f, 0.50f}, {0.12f, 0.12f}, {0.88f, 0.12f},
        {0.12f, 0.88f}, {0.88f, 0.88f},
    };
    float total = 0.0f;
    for (int32_t i = 0; i < (int32_t)(sizeof(samples) /
                                      sizeof(samples[0])); ++i) {
        total += TerrainSampleGrid(
            street_terrain_height,
            footprint.x + footprint.width * samples[i].x,
            footprint.y + footprint.height * samples[i].y);
    }
    return total / (float)(sizeof(samples) / sizeof(samples[0]));
}

static void TerrainGradePad(Rectangle footprint, float elevation,
                            float blend_distance)
{
    for (int32_t row = 0; row < CC_TERRAIN_ROWS; ++row) {
        float z = (float)row * CC_TERRAIN_CELL_SIZE;
        for (int32_t column = 0; column < CC_TERRAIN_COLUMNS; ++column) {
            float x = (float)column * CC_TERRAIN_CELL_SIZE;
            float distance = TerrainDistanceToRectangle(x, z, footprint);
            if (distance >= blend_distance) continue;
            bool inside = x >= footprint.x &&
                x <= footprint.x + footprint.width &&
                z >= footprint.y &&
                z <= footprint.y + footprint.height;
            float weight = inside ? 1.0f :
                TerrainSmooth01(1.0f - distance / blend_distance);
            int32_t index = TerrainIndex(column, row);
            street_terrain_height[index] +=
                (elevation - street_terrain_height[index]) * weight;
        }
    }
}

static void TerrainGradeNorthSouthRamp(Rectangle footprint,
                                       float south_height,
                                       float north_height)
{
    const float shoulder = 0.85f;
    for (int32_t row = 0; row < CC_TERRAIN_ROWS; ++row) {
        float z = (float)row * CC_TERRAIN_CELL_SIZE;
        if (z < footprint.y || z > footprint.y + footprint.height) continue;
        float progress = TerrainSmooth01((z - footprint.y) /
                                         footprint.height);
        float elevation = south_height +
                          (north_height - south_height) * progress;
        for (int32_t column = 0; column < CC_TERRAIN_COLUMNS; ++column) {
            float x = (float)column * CC_TERRAIN_CELL_SIZE;
            float side_distance = x < footprint.x ? footprint.x - x :
                x > footprint.x + footprint.width ?
                    x - (footprint.x + footprint.width) : 0.0f;
            if (side_distance >= shoulder) continue;
            float weight = side_distance <= 0.0f ? 1.0f :
                TerrainSmooth01(1.0f - side_distance / shoulder);
            int32_t index = TerrainIndex(column, row);
            street_terrain_height[index] +=
                (elevation - street_terrain_height[index]) * weight;
        }
    }
}

static void TerrainGenerate(void)
{
    for (int32_t row = 0; row < CC_TERRAIN_ROWS; ++row) {
        float z = (float)row * CC_TERRAIN_CELL_SIZE;
        for (int32_t column = 0; column < CC_TERRAIN_COLUMNS; ++column) {
            float x = (float)column * CC_TERRAIN_CELL_SIZE;
            int32_t index = TerrainIndex(column, row);
            street_terrain_natural[index] = TerrainNaturalHeight(x, z);
            street_terrain_height[index] = street_terrain_natural[index];
        }
    }
    for (int32_t road = 0;
         road < (int32_t)(sizeof(TERRAIN_ROADS) /
                          sizeof(TERRAIN_ROADS[0])); ++road) {
        TerrainGradeRoad(&TERRAIN_ROADS[road]);
    }
    TerrainGradeActivePlaceRoads();

    /* Foundations are small human changes to the generated land. Each pad
       takes its elevation from the land and nearby road before construction. */
    for (int32_t i = 0; i < (int32_t)(sizeof(WORLD_BUILDINGS) /
                                      sizeof(WORLD_BUILDINGS[0])); ++i) {
        Rectangle footprint = WORLD_BUILDINGS[i].footprint;
        TerrainGradePad(footprint, TerrainRectangleAverage(footprint), 1.45f);
    }
    for (int32_t i = 0; i < CC_LOCAL_PLACE_LANDMARK_COUNT; ++i) {
        Rectangle footprint =
            PlaceLandmarkFootprint(ActivePlaceLandmarkAt(i));
        TerrainGradePad(footprint, TerrainRectangleAverage(footprint), 1.35f);
    }
    Rectangle keep_pad = {65.20f, 8.20f, 26.50f, 24.40f};
    float keep_elevation = TerrainRectangleAverage(keep_pad) + 0.45f;
    TerrainGradePad(keep_pad, keep_elevation, 2.80f);

    Rectangle plaza_pad = {37.60f, 25.60f, 18.80f, 8.80f};
    float plaza_elevation = TerrainRectangleAverage(plaza_pad);
    TerrainGradePad(plaza_pad, plaza_elevation, 1.80f);
    TerrainGradePad(CARRIAGE_FOOTPRINT,
                    TerrainRectangleAverage(CARRIAGE_FOOTPRINT), 1.20f);
    TerrainGradePad(DUNGEON_FOOTPRINT,
                    TerrainRectangleAverage(DUNGEON_FOOTPRINT), 1.35f);

    /* The Wayfarer yard is deliberately engineered level ground. Keeping
       this one training pad flat also preserves its fixed waterline. */
    TerrainGradePad((Rectangle){1.00f, 0.00f, 14.60f, 11.10f},
                    0.0f, 2.20f);

    /* Re-establish the public ways after all foundation banks are cut. Pads
       stay level inside actual structures, while streets and open squares
       retain a continuous walking grade around them. */
    for (int32_t road = 0;
         road < (int32_t)(sizeof(TERRAIN_ROADS) /
                          sizeof(TERRAIN_ROADS[0])); ++road) {
        TerrainGradeRoad(&TERRAIN_ROADS[road]);
    }
    TerrainGradeActivePlaceRoads();
    float artisan_south_height = TerrainSampleGrid(street_terrain_height,
                                                    33.0f, 25.0f);
    float artisan_north_height = TerrainSampleGrid(street_terrain_height,
                                                    33.0f, 30.0f);
    TerrainGradeNorthSouthRamp((Rectangle){29.00f, 25.00f, 13.00f, 5.00f},
                               artisan_south_height,
                               artisan_north_height);
    float commons_south_height = TerrainSampleGrid(street_terrain_height,
                                                    41.5f, 11.6f);
    float commons_north_height = TerrainSampleGrid(street_terrain_height,
                                                    41.5f, 25.0f);
    TerrainGradeNorthSouthRamp((Rectangle){39.40f, 11.60f, 4.20f, 13.40f},
                               commons_south_height,
                               commons_north_height);
    float crown_road_height = TerrainSampleGrid(street_terrain_height,
                                                 78.5f, 38.0f);
    TerrainGradeNorthSouthRamp((Rectangle){75.40f, 30.60f, 6.20f, 7.40f},
                               keep_elevation, crown_road_height);
    TerrainGradePad((Rectangle){39.00f, 27.00f, 15.00f, 6.00f},
                    plaza_elevation, 2.50f);
    street_terrain_ready = true;
}

static void TerrainEnsureReady(void)
{
    if (!street_terrain_ready) TerrainGenerate();
}

void CcLocalTerrainSetSeed(uint32_t seed)
{
    seed = seed == 0U ? UINT32_C(0xc0a71a9e) : seed;
    if (street_terrain_ready && seed == street_terrain_seed) return;
    street_terrain_seed = seed;
    street_terrain_ready = false;
}

void CcLocalBindPlace(const CcSim *sim)
{
    const CcSettlement *place = sim != NULL ?
        CcSimSettlement(sim, sim->player.location_id) : NULL;
    active_place_function = place != NULL ? place->function :
                                            CC_SETTLEMENT_MARKET;
    CcLocalTerrainSetSeed(sim != NULL ?
        CcLocalPlaceTerrainSeed(sim->world_seed, place) :
        UINT32_C(0xc0a71a9e));
}

float CcLocalTerrainHeightAt(float x, float z)
{
    TerrainEnsureReady();
    return TerrainSampleGrid(street_terrain_height, x, z);
}

Vector3 CcLocalTerrainNormalAt(float x, float z)
{
    const float offset = CC_TERRAIN_CELL_SIZE;
    float left = CcLocalTerrainHeightAt(x - offset, z);
    float right = CcLocalTerrainHeightAt(x + offset, z);
    float near_height = CcLocalTerrainHeightAt(x, z - offset);
    float far_height = CcLocalTerrainHeightAt(x, z + offset);
    Vector3 normal = {left - right, offset * 2.0f,
                      near_height - far_height};
    float length = sqrtf(normal.x * normal.x + normal.y * normal.y +
                         normal.z * normal.z);
    return length > 0.0001f ?
        (Vector3){normal.x / length, normal.y / length, normal.z / length} :
        (Vector3){0.0f, 1.0f, 0.0f};
}

static float PlatformBaseHeight(const NavPlatform *platform)
{
    return CcLocalTerrainHeightAt(platform->x + platform->width * 0.5f,
                                  platform->z + platform->depth * 0.5f);
}

static float PlatformTopHeight(const NavPlatform *platform)
{
    return PlatformBaseHeight(platform) + platform->height;
}

static float TerrainFootprintHeight(Rectangle footprint)
{
    return CcLocalTerrainHeightAt(footprint.x + footprint.width * 0.5f,
                                  footprint.y + footprint.height * 0.5f);
}

static Vector3 TerrainWorldPoint(float x, float z)
{
    return (Vector3){x, CcLocalTerrainHeightAt(x, z), z};
}

static const Vector3 COURSE_WAYPOINTS[] = {
    {9.45f, 0.0f, 1.20f}, {10.50f, 0.0f, 1.20f},
    {11.22f, 0.0f, 2.95f}, {12.27f, 0.0f, 1.97f},
    {13.00f, 0.0f, 1.45f}, {13.57f, 0.0f, 2.95f},
    {14.35f, 0.0f, 4.15f}, {11.55f, 0.0f, 3.86f},
    {9.10f, 0.0f, 3.86f}, {10.27f, 0.0f, 5.62f},
    {10.65f, 0.0f, 6.75f}, {11.66f, 0.0f, 6.11f},
    {12.05f, 0.0f, 7.45f}, {13.10f, 0.0f, 6.95f},
    {14.15f, 0.0f, 7.75f}, {11.15f, 0.0f, 8.30f},
    {12.55f, 0.0f, 9.65f}, {13.57f, 0.0f, 9.12f},
    {14.45f, 0.0f, 9.65f}, {9.40f, 0.0f, 9.65f}
};

static Camera3D LocalCamera(bool interior, Vector3 focus);
static Camera3D ExteriorCameraAt(Vector3 target, float fovy);
static Camera3D ExteriorCameraComposed(Vector3 target, Vector3 offset,
                                       float fovy);
static Camera3D SnapCameraToArtPixels(Camera3D camera, int32_t art_height);
Camera3D CcLocalStreetCameraInternal(const CcLocalAgent *agent, float clock,
                                     bool advance, int32_t art_height);
static Camera3D RoadCamera(Vector3 focus, bool travelling, float clock,
                           bool advance, int32_t art_height);
Camera3D CcLocalCombatCameraInternal(Camera3D base,
                                     const CcLocalAgent *player,
                                     const CcLocalCourse *course,
                                     float clock, bool advance,
                                     int32_t art_height);
static int32_t StreetCameraShotFor(Vector3 focus, int32_t current_shot);
static float WrapAngle(float angle);
static float SmoothStep01(float amount);
static Color ShadeColor(Color color, float scale);
static void DrawCharacterEllipsoid(Vector3 center, Vector3 radius,
                                   Color color);
static void UpdateHeroCape(CcLocalAgent *agent, float delta_time);
static int32_t RoadObstacleCount(void);
static Rectangle RoadObstacleAt(int32_t index);
static float CameraPositionRectangleClutter(Vector3 position,
                                            Rectangle footprint,
                                            float clearance);
static bool RoomDetailPointVisible(float x, float z, Vector3 focus);
static uint32_t StreetForegroundBuildingMask(void);
static uint32_t StreetForegroundBuildingMaskForShot(int32_t shot);

static bool CourseWaterContains(CcLocalSceneKind scene, float x, float z)
{
    if (scene != CC_LOCAL_SCENE_STREET) return false;
    return x >= COURSE_POOL.x && x <= COURSE_POOL.x + COURSE_POOL.width &&
           z >= COURSE_POOL.y && z <= COURSE_POOL.y + COURSE_POOL.height;
}

static float SurfaceHeightAt(CcLocalSceneKind scene, float x, float z)
{
    if (scene == CC_LOCAL_SCENE_ROAD) {
        if (!RoadUsesAuthoredCheckpoint()) return 0.0f;
        /* Match the exported bridge exactly: two 10 cm causeways lead onto
           a 52 cm deck centered 30 cm above the asset origin. */
        Vector2 point = {x, z};
        if (CheckCollisionPointRec(point, ROAD_BRIDGE_DECK_SUPPORT)) {
            return 0.56f;
        }
        if (CheckCollisionPointRec(point,
                                   ROAD_BRIDGE_WEST_CAUSEWAY_SUPPORT) ||
            CheckCollisionPointRec(point,
                                   ROAD_BRIDGE_EAST_CAUSEWAY_SUPPORT)) {
            return 0.10f;
        }
        return 0.0f;
    }
    if (scene != CC_LOCAL_SCENE_STREET) return 0.0f;
    float height = CcLocalTerrainHeightAt(x, z);
    for (int32_t i = 0; i < StreetPhysicsPlatformCount(); ++i) {
        const NavPlatform *platform = StreetPhysicsPlatformAt(i);
        if (platform == NULL) continue;
        float top = PlatformTopHeight(platform);
        if (x >= platform->x && x <= platform->x + platform->width &&
            z >= platform->z && z <= platform->z + platform->depth &&
            top > height) {
            height = top;
        }
    }
    return height;
}

static float BodySurfaceHeightAt(CcLocalSceneKind scene, float x, float z)
{
    if (scene == CC_LOCAL_SCENE_ROAD) {
        return SurfaceHeightAt(scene, x, z);
    }
    if (scene != CC_LOCAL_SCENE_STREET) return 0.0f;
    float height = CcLocalTerrainHeightAt(x, z);
    for (int32_t i = 0; i < StreetPhysicsPlatformCount(); ++i) {
        const NavPlatform *platform = StreetPhysicsPlatformAt(i);
        if (platform == NULL) continue;
        float top = PlatformTopHeight(platform);
        float edge_release = platform->style == 0 ? 0.0f : 0.10f;
        if (x >= platform->x + edge_release &&
            x <= platform->x + platform->width - edge_release &&
            z >= platform->z + edge_release &&
            z <= platform->z + platform->depth - edge_release &&
            top > height) {
            height = top;
        }
    }
    return height;
}

static Vector3 SurfaceNormalAt(CcLocalSceneKind scene, float x, float z)
{
    if (scene != CC_LOCAL_SCENE_STREET) return (Vector3){0.0f, 1.0f, 0.0f};
    float terrain = CcLocalTerrainHeightAt(x, z);
    if (SurfaceHeightAt(scene, x, z) > terrain + 0.08f) {
        return (Vector3){0.0f, 1.0f, 0.0f};
    }
    return CcLocalTerrainNormalAt(x, z);
}

static float FootprintDistanceSquared(float x, float z, Rectangle footprint)
{
    float closest_x = fmaxf(footprint.x,
                            fminf(x, footprint.x + footprint.width));
    float closest_z = fmaxf(footprint.y,
                            fminf(z, footprint.y + footprint.height));
    float delta_x = x - closest_x;
    float delta_z = z - closest_z;
    return delta_x * delta_x + delta_z * delta_z;
}

static bool CircleTouchesFootprint(float x, float z, float radius,
                                   Rectangle footprint)
{
    return FootprintDistanceSquared(x, z, footprint) < radius * radius;
}

static bool StaticBodyBlocked(CcLocalSceneKind scene, float x, float z,
                              float radius)
{
    bool market_interior = scene == CC_LOCAL_SCENE_MARKET;
    float maximum_x = market_interior ? 8.72f : CC_LOCAL_WORLD_WIDTH - 0.28f;
    float maximum_z = market_interior ? 6.72f : CC_LOCAL_WORLD_DEPTH - 0.28f;
    if (x < 0.28f + radius || x > maximum_x - radius ||
        z < 0.28f + radius || z > maximum_z - radius) return true;
    if (market_interior) {
        if (CircleTouchesFootprint(x, z, radius, MARKET_COUNTER_FOOTPRINT) ||
            CircleTouchesFootprint(x, z, radius, MARKET_SHELF_FOOTPRINT)) {
            return true;
        }
        for (int32_t i = 0; i < (int32_t)(sizeof(MARKET_PEOPLE) /
                                          sizeof(MARKET_PEOPLE[0])); ++i) {
            float dx = x - MARKET_PEOPLE[i].x;
            float dz = z - MARKET_PEOPLE[i].y;
            float clearance = radius + PERSON_COLLISION_RADIUS;
            if (dx * dx + dz * dz < clearance * clearance) return true;
        }
        return false;
    }
    if (scene == CC_LOCAL_SCENE_ROAD) {
        for (int32_t i = 0; i < RoadObstacleCount(); ++i) {
            if (CircleTouchesFootprint(x, z, radius, RoadObstacleAt(i))) {
                return true;
            }
        }
        return false;
    }
    for (int32_t i = 0; i < (int32_t)(sizeof(WORLD_BUILDINGS) /
                                      sizeof(WORLD_BUILDINGS[0])); ++i) {
        /* Foreground reveal is only a drawing rule. A house remains solid in
           every camera shot, including while its roof and facade are faded
           so the player can see the character behind it. */
        if (CircleTouchesFootprint(x, z, radius,
                                   WORLD_BUILDINGS[i].footprint)) return true;
    }
    for (int32_t i = 0; i < (int32_t)(sizeof(CASTLE_STRUCTURES) /
                                      sizeof(CASTLE_STRUCTURES[0])); ++i) {
        if (CircleTouchesFootprint(x, z, radius,
                                   CASTLE_STRUCTURES[i].footprint)) return true;
    }
    for (int32_t i = 0; i < CC_LOCAL_PLACE_LANDMARK_COUNT; ++i) {
        if (CircleTouchesFootprint(
                x, z, radius,
                PlaceLandmarkFootprint(ActivePlaceLandmarkAt(i)))) {
            return true;
        }
    }
    if (CircleTouchesFootprint(x, z, radius, CARRIAGE_FOOTPRINT) ||
        CircleTouchesFootprint(x, z, radius, DUNGEON_FOOTPRINT)) return true;
    for (int32_t i = 0; i < (int32_t)(sizeof(ROOM_ART_OBSTACLES) /
                                      sizeof(ROOM_ART_OBSTACLES[0])); ++i) {
        if (CircleTouchesFootprint(x, z, radius, ROOM_ART_OBSTACLES[i])) {
            return true;
        }
    }
    for (int32_t i = 0; i < (int32_t)(sizeof(STREET_PEOPLE) /
                                      sizeof(STREET_PEOPLE[0])); ++i) {
        float dx = x - STREET_PEOPLE[i].x;
        float dz = z - STREET_PEOPLE[i].y;
        float clearance = radius + PERSON_COLLISION_RADIUS;
        if (dx * dx + dz * dz < clearance * clearance) return true;
    }
    return false;
}

#define STREET_PATH_CELL_SIZE 0.50f
#define STREET_PATH_COLUMNS 192
#define STREET_PATH_ROWS 144
#define STREET_PATH_NODE_COUNT (STREET_PATH_COLUMNS * STREET_PATH_ROWS)

typedef struct StreetPathSearch {
    float distance[STREET_PATH_NODE_COUNT];
    int32_t parent[STREET_PATH_NODE_COUNT];
    int32_t heap[STREET_PATH_NODE_COUNT];
    int32_t heap_position[STREET_PATH_NODE_COUNT];
    uint8_t closed[STREET_PATH_NODE_COUNT];
    int32_t heap_count;
} StreetPathSearch;

static StreetPathSearch street_path_search;

static bool StreetPathBodyBlocked(float x, float z, float radius)
{
    if (StaticBodyBlocked(CC_LOCAL_SCENE_STREET, x, z, radius)) return true;
    for (int32_t index = 0; index < StreetPhysicsPlatformCount(); ++index) {
        const NavPlatform *platform = StreetPhysicsPlatformAt(index);
        if (platform == NULL) continue;
        Rectangle footprint = {platform->x, platform->z,
                               platform->width, platform->depth};
        /* Movement allows exact tangency at a platform edge. The path
           sampler passes a small general safety margin, so remove it here
           to avoid declaring authored edge triggers unreachable. */
        float platform_radius = fmaxf(0.0f, radius - 0.026f);
        if (CircleTouchesFootprint(x, z, platform_radius, footprint)) {
            return true;
        }
    }
    return false;
}

static Vector2 StreetPathNodePoint(int32_t node)
{
    int32_t column = node % STREET_PATH_COLUMNS;
    int32_t row = node / STREET_PATH_COLUMNS;
    return (Vector2){((float)column + 0.5f) * STREET_PATH_CELL_SIZE,
                     ((float)row + 0.5f) * STREET_PATH_CELL_SIZE};
}

static bool StreetSegmentClear(Vector2 from, Vector2 to, float radius)
{
    float x = to.x - from.x;
    float z = to.y - from.y;
    float length = sqrtf(x * x + z * z);
    int32_t steps = (int32_t)ceilf(length / 0.12f);
    if (steps < 1) steps = 1;
    Vector2 previous = from;
    float previous_height = CcLocalTerrainHeightAt(from.x, from.y);
    for (int32_t step = 1; step <= steps; ++step) {
        float amount = (float)step / (float)steps;
        float sample_x = from.x + x * amount;
        float sample_z = from.y + z * amount;
        if (StreetPathBodyBlocked(sample_x, sample_z,
                                  radius + 0.025f)) return false;
        float sample_height = CcLocalTerrainHeightAt(sample_x, sample_z);
        Vector3 sample_normal = CcLocalTerrainNormalAt(sample_x, sample_z);
        float step_x = sample_x - previous.x;
        float step_z = sample_z - previous.y;
        float step_length = sqrtf(step_x * step_x + step_z * step_z);
        float height_change = sample_height - previous_height;
        /* The path graph must obey the same grade that the physical body can
           actually walk. Otherwise A* returns a straight line across a steep
           foundation skirt and the character correctly stalls at its foot. */
        if (height_change > step_length * 1.25f + 0.025f ||
            height_change < -step_length * 1.65f - 0.060f ||
            sample_normal.y < 0.60f) {
            return false;
        }
        previous = (Vector2){sample_x, sample_z};
        previous_height = sample_height;
    }
    return true;
}

static int32_t StreetPathNearestNode(Vector2 point, float radius,
                                     bool require_point_connection)
{
    int32_t center_column = (int32_t)floorf(point.x / STREET_PATH_CELL_SIZE);
    int32_t center_row = (int32_t)floorf(point.y / STREET_PATH_CELL_SIZE);
    for (int32_t ring = 0; ring <= 12; ++ring) {
        int32_t best = -1;
        float best_distance = FLT_MAX;
        for (int32_t row = center_row - ring; row <= center_row + ring;
             ++row) {
            if (row < 0 || row >= STREET_PATH_ROWS) continue;
            for (int32_t column = center_column - ring;
                 column <= center_column + ring; ++column) {
                if (column < 0 || column >= STREET_PATH_COLUMNS) continue;
                if (ring > 0 && abs(column - center_column) != ring &&
                    abs(row - center_row) != ring) continue;
                int32_t node = row * STREET_PATH_COLUMNS + column;
                Vector2 candidate = StreetPathNodePoint(node);
                if (StreetPathBodyBlocked(candidate.x, candidate.y,
                                          radius + 0.025f) ||
                    (require_point_connection &&
                     !StreetSegmentClear(point, candidate, radius))) {
                    continue;
                }
                float x = candidate.x - point.x;
                float z = candidate.y - point.y;
                float distance = x * x + z * z;
                if (distance >= best_distance) continue;
                best = node;
                best_distance = distance;
            }
        }
        if (best >= 0) return best;
    }
    return -1;
}

static float StreetPathHeuristic(int32_t node, int32_t goal)
{
    Vector2 point = StreetPathNodePoint(node);
    Vector2 target = StreetPathNodePoint(goal);
    float x = target.x - point.x;
    float z = target.y - point.y;
    return sqrtf(x * x + z * z);
}

static bool StreetPathHeapLess(const StreetPathSearch *search,
                               int32_t left, int32_t right, int32_t goal)
{
    float left_score = search->distance[left] +
                       StreetPathHeuristic(left, goal);
    float right_score = search->distance[right] +
                        StreetPathHeuristic(right, goal);
    if (fabsf(left_score - right_score) > 0.0001f) {
        return left_score < right_score;
    }
    return search->distance[left] < search->distance[right];
}

static void StreetPathHeapSwap(StreetPathSearch *search,
                               int32_t left, int32_t right)
{
    int32_t node = search->heap[left];
    search->heap[left] = search->heap[right];
    search->heap[right] = node;
    search->heap_position[search->heap[left]] = left;
    search->heap_position[search->heap[right]] = right;
}

static void StreetPathHeapSiftUp(StreetPathSearch *search,
                                 int32_t position, int32_t goal)
{
    while (position > 0) {
        int32_t parent = (position - 1) / 2;
        if (!StreetPathHeapLess(search, search->heap[position],
                                search->heap[parent], goal)) break;
        StreetPathHeapSwap(search, position, parent);
        position = parent;
    }
}

static void StreetPathHeapPushOrDecrease(StreetPathSearch *search,
                                         int32_t node, int32_t goal)
{
    int32_t position = search->heap_position[node];
    if (position >= 0) {
        StreetPathHeapSiftUp(search, position, goal);
        return;
    }
    position = search->heap_count++;
    search->heap[position] = node;
    search->heap_position[node] = position;
    StreetPathHeapSiftUp(search, position, goal);
}

static int32_t StreetPathHeapPop(StreetPathSearch *search, int32_t goal)
{
    if (search->heap_count <= 0) return -1;
    int32_t result = search->heap[0];
    search->heap_position[result] = -1;
    search->heap_count -= 1;
    if (search->heap_count <= 0) return result;
    search->heap[0] = search->heap[search->heap_count];
    search->heap_position[search->heap[0]] = 0;
    int32_t position = 0;
    for (;;) {
        int32_t left = position * 2 + 1;
        if (left >= search->heap_count) break;
        int32_t right = left + 1;
        int32_t smallest = left;
        if (right < search->heap_count &&
            StreetPathHeapLess(search, search->heap[right],
                               search->heap[left], goal)) {
            smallest = right;
        }
        if (!StreetPathHeapLess(search, search->heap[smallest],
                                search->heap[position], goal)) break;
        StreetPathHeapSwap(search, position, smallest);
        position = smallest;
    }
    return result;
}

static int32_t FindStreetPath(Vector2 from, Vector2 to, float radius,
                              Vector2 *points, int32_t capacity)
{
    if (points == NULL || capacity <= 0) return 0;
    bool target_blocked = StreetPathBodyBlocked(to.x, to.y, radius);
    bool target_projected = target_blocked;
    if (!target_blocked && StreetSegmentClear(from, to, radius)) {
        points[0] = to;
        return 1;
    }

    int32_t start = StreetPathNearestNode(from, radius, true);
    int32_t goal = StreetPathNearestNode(to, radius, !target_blocked);
    if (start < 0 || goal < 0) return 0;
    StreetPathSearch *search = &street_path_search;
    search->heap_count = 0;
    for (int32_t node = 0; node < STREET_PATH_NODE_COUNT; ++node) {
        search->distance[node] = FLT_MAX;
        search->parent[node] = -1;
        search->heap_position[node] = -1;
        search->closed[node] = 0;
    }
    search->distance[start] = 0.0f;
    StreetPathHeapPushOrDecrease(search, start, goal);

    static const int32_t neighbor_column[] = {-1, 0, 1, -1, 1, -1, 0, 1};
    static const int32_t neighbor_row[] = {-1, -1, -1, 0, 0, 1, 1, 1};
    while (search->heap_count > 0) {
        int32_t current = StreetPathHeapPop(search, goal);
        if (current < 0) break;
        if (search->closed[current]) continue;
        search->closed[current] = 1;
        if (current == goal) break;
        int32_t current_column = current % STREET_PATH_COLUMNS;
        int32_t current_row = current / STREET_PATH_COLUMNS;
        for (int32_t neighbor = 0; neighbor < 8; ++neighbor) {
            int32_t column = current_column + neighbor_column[neighbor];
            int32_t row = current_row + neighbor_row[neighbor];
            if (column < 0 || column >= STREET_PATH_COLUMNS ||
                row < 0 || row >= STREET_PATH_ROWS) continue;
            int32_t next = row * STREET_PATH_COLUMNS + column;
            if (search->closed[next]) continue;
            Vector2 current_point = StreetPathNodePoint(current);
            Vector2 next_point = StreetPathNodePoint(next);
            if (StreetPathBodyBlocked(next_point.x, next_point.y,
                                      radius + 0.025f) ||
                !StreetSegmentClear(current_point, next_point, radius)) {
                continue;
            }
            bool diagonal = neighbor_column[neighbor] != 0 &&
                            neighbor_row[neighbor] != 0;
            if (diagonal) {
                int32_t horizontal = current_row * STREET_PATH_COLUMNS + column;
                int32_t vertical = row * STREET_PATH_COLUMNS + current_column;
                Vector2 horizontal_point = StreetPathNodePoint(horizontal);
                Vector2 vertical_point = StreetPathNodePoint(vertical);
                if (StreetPathBodyBlocked(horizontal_point.x,
                                          horizontal_point.y,
                                          radius + 0.025f) ||
                    StreetPathBodyBlocked(vertical_point.x,
                                          vertical_point.y,
                                          radius + 0.025f)) continue;
            }
            float step = diagonal ? STREET_PATH_CELL_SIZE * 1.41421356f :
                                    STREET_PATH_CELL_SIZE;
            float current_height = CcLocalTerrainHeightAt(
                current_point.x, current_point.y);
            float next_height = CcLocalTerrainHeightAt(
                next_point.x, next_point.y);
            Vector3 next_normal = CcLocalTerrainNormalAt(
                next_point.x, next_point.y);
            float traversal_cost = CcRobotTraversabilityCost(
                step, next_height - current_height, next_normal.y);
            float distance = search->distance[current] + traversal_cost;
            if (distance + 0.0001f >= search->distance[next]) continue;
            search->distance[next] = distance;
            search->parent[next] = current;
            StreetPathHeapPushOrDecrease(search, next, goal);
        }
    }
    if (start != goal && search->parent[goal] < 0) {
        /* Detour-style partial path: if the exact goal island is not
           connected, finish at the reachable node closest to the click.
           Limit the projection so a click never sends the hero somewhere
           unrelated on the far side of a building. */
        int32_t closest = -1;
        float closest_distance = 2.75f * 2.75f;
        for (int32_t node = 0; node < STREET_PATH_NODE_COUNT; ++node) {
            if (!search->closed[node]) continue;
            Vector2 candidate = StreetPathNodePoint(node);
            float x = candidate.x - to.x;
            float z = candidate.y - to.y;
            float distance = x * x + z * z;
            if (distance >= closest_distance) continue;
            closest = node;
            closest_distance = distance;
        }
        if (closest < 0) return 0;
        goal = closest;
        target_projected = true;
    }

    int32_t ordered_count = 0;
    for (int32_t node = goal; node >= 0 && ordered_count < STREET_PATH_NODE_COUNT;
         node = search->parent[node]) {
        search->heap[ordered_count++] = node;
        if (node == start) break;
    }
    if (ordered_count <= 0 || search->heap[ordered_count - 1] != start) {
        return 0;
    }
    for (int32_t left = 0, right = ordered_count - 1; left < right;
         ++left, --right) {
        int32_t node = search->heap[left];
        search->heap[left] = search->heap[right];
        search->heap[right] = node;
    }

    int32_t point_count = 0;
    Vector2 current_point = from;
    int32_t cursor = 0;
    Vector2 start_point = StreetPathNodePoint(search->heap[0]);
    float start_x = start_point.x - current_point.x;
    float start_z = start_point.y - current_point.y;
    if (start_x * start_x + start_z * start_z > 0.08f * 0.08f) {
        if (point_count >= capacity) return 0;
        points[point_count++] = start_point;
        current_point = start_point;
    }
    Vector2 resolved_target = target_projected ?
        StreetPathNodePoint(goal) : to;
    while (cursor < ordered_count - 1) {
        if (StreetSegmentClear(current_point, resolved_target, radius)) {
            if (point_count >= capacity) return 0;
            points[point_count++] = resolved_target;
            return point_count;
        }
        int32_t chosen = cursor + 1;
        for (int32_t candidate = ordered_count - 1; candidate > cursor;
             --candidate) {
            Vector2 candidate_point = StreetPathNodePoint(
                search->heap[candidate]);
            if (!StreetSegmentClear(current_point, candidate_point, radius)) {
                continue;
            }
            chosen = candidate;
            break;
        }
        Vector2 chosen_point = StreetPathNodePoint(search->heap[chosen]);
        if (point_count >= capacity) return 0;
        points[point_count++] = chosen_point;
        current_point = chosen_point;
        cursor = chosen;
    }
    if (!StreetSegmentClear(current_point, resolved_target, radius) ||
        point_count >= capacity) return 0;
    points[point_count++] = resolved_target;
    return point_count;
}

typedef struct LocalProbeContext {
    CcLocalSceneKind scene;
    float root_base_y;
    bool root_capsule;
} LocalProbeContext;

typedef struct LocalCollisionBox {
    Vector3 minimum;
    Vector3 maximum;
} LocalCollisionBox;

#define CC_LOCAL_COLLISION_BOX_CAPACITY 72

static void AddLocalCollisionBox(LocalCollisionBox *boxes, int32_t *count,
                                 Rectangle footprint, float base,
                                 float height)
{
    if (*count >= CC_LOCAL_COLLISION_BOX_CAPACITY || height <= 0.0f) return;
    boxes[*count] = (LocalCollisionBox){
        .minimum = {footprint.x, base, footprint.y},
        .maximum = {footprint.x + footprint.width, base + height,
                    footprint.y + footprint.height},
    };
    *count += 1;
}

static int32_t GatherLocalCollisionBoxes(const LocalProbeContext *context,
                                         Vector3 point,
                                         LocalCollisionBox *boxes)
{
    (void)point;
    CcLocalSceneKind scene = context != NULL ? context->scene :
                                               CC_LOCAL_SCENE_STREET;
    int32_t count = 0;
    if (scene == CC_LOCAL_SCENE_MARKET) {
        AddLocalCollisionBox(boxes, &count, MARKET_COUNTER_FOOTPRINT,
                             0.0f, 1.12f);
        AddLocalCollisionBox(boxes, &count, MARKET_SHELF_FOOTPRINT,
                             0.0f, 2.15f);
        return count;
    }
    if (scene == CC_LOCAL_SCENE_ROAD) {
        for (int32_t obstacle = 0; obstacle < RoadObstacleCount(); ++obstacle) {
            AddLocalCollisionBox(boxes, &count, RoadObstacleAt(obstacle),
                                 0.0f, 2.35f);
        }
        return count;
    }

    for (int32_t platform_index = 0;
         platform_index < StreetPhysicsPlatformCount(); ++platform_index) {
        const NavPlatform *platform = StreetPhysicsPlatformAt(platform_index);
        if (platform == NULL) continue;
        Rectangle footprint = {platform->x, platform->z,
                               platform->width, platform->depth};
        AddLocalCollisionBox(boxes, &count, footprint,
                             PlatformBaseHeight(platform), platform->height);
    }

    for (int32_t building = 0;
         building < (int32_t)(sizeof(WORLD_BUILDINGS) /
                              sizeof(WORLD_BUILDINGS[0])); ++building) {
        const WorldBuilding *world = &WORLD_BUILDINGS[building];
        AddLocalCollisionBox(boxes, &count, world->footprint,
                             TerrainFootprintHeight(world->footprint),
                             world->height);
    }
    for (int32_t structure = 0;
         structure < (int32_t)(sizeof(CASTLE_STRUCTURES) /
                               sizeof(CASTLE_STRUCTURES[0])); ++structure) {
        const WorldStructure *world = &CASTLE_STRUCTURES[structure];
        AddLocalCollisionBox(boxes, &count, world->footprint,
                             TerrainFootprintHeight(world->footprint),
                             world->height);
    }
    for (int32_t landmark = 0;
         landmark < CC_LOCAL_PLACE_LANDMARK_COUNT; ++landmark) {
        const CcLocalPlaceLandmark *place_landmark =
            ActivePlaceLandmarkAt(landmark);
        Rectangle footprint = PlaceLandmarkFootprint(place_landmark);
        AddLocalCollisionBox(boxes, &count, footprint,
                             TerrainFootprintHeight(footprint),
                             place_landmark != NULL ?
                                 place_landmark->height : 0.0f);
    }
    AddLocalCollisionBox(boxes, &count, CARRIAGE_FOOTPRINT,
                         TerrainFootprintHeight(CARRIAGE_FOOTPRINT), 2.65f);
    AddLocalCollisionBox(boxes, &count, DUNGEON_FOOTPRINT,
                         TerrainFootprintHeight(DUNGEON_FOOTPRINT), 3.10f);
    for (int32_t obstacle = 0;
         obstacle < (int32_t)(sizeof(ROOM_ART_OBSTACLES) /
                              sizeof(ROOM_ART_OBSTACLES[0])); ++obstacle) {
        Rectangle footprint = ROOM_ART_OBSTACLES[obstacle];
        AddLocalCollisionBox(boxes, &count, footprint,
                             TerrainFootprintHeight(footprint), 1.35f);
    }
    return count;
}

static bool SweptPointAgainstBox(Vector3 previous, Vector3 position,
                                 const LocalCollisionBox *box, float radius,
                                 float *hit_time, Vector3 *hit_normal)
{
    Vector3 minimum = {box->minimum.x - radius,
                       box->minimum.y - radius,
                       box->minimum.z - radius};
    Vector3 maximum = {box->maximum.x + radius,
                       box->maximum.y + radius,
                       box->maximum.z + radius};
    Vector3 movement = Vector3Subtract(position, previous);
    float enter = 0.0f;
    float leave = 1.0f;
    Vector3 normal = {0};
    const float starts[3] = {previous.x, previous.y, previous.z};
    const float moves[3] = {movement.x, movement.y, movement.z};
    const float lows[3] = {minimum.x, minimum.y, minimum.z};
    const float highs[3] = {maximum.x, maximum.y, maximum.z};
    for (int32_t axis = 0; axis < 3; ++axis) {
        if (fabsf(moves[axis]) <= 0.000001f) {
            if (starts[axis] < lows[axis] || starts[axis] > highs[axis]) {
                return false;
            }
            continue;
        }
        float inverse = 1.0f / moves[axis];
        float near_time = (lows[axis] - starts[axis]) * inverse;
        float far_time = (highs[axis] - starts[axis]) * inverse;
        float near_sign = -1.0f;
        if (near_time > far_time) {
            float swap = near_time;
            near_time = far_time;
            far_time = swap;
            near_sign = 1.0f;
        }
        if (near_time > enter) {
            enter = near_time;
            normal = (Vector3){0};
            if (axis == 0) normal.x = near_sign;
            else if (axis == 1) normal.y = near_sign;
            else normal.z = near_sign;
        }
        leave = fminf(leave, far_time);
        if (enter > leave) return false;
    }
    if (enter < 0.0f || enter > 1.0f ||
        (fabsf(normal.x) + fabsf(normal.y) + fabsf(normal.z)) < 0.5f) {
        return false;
    }
    *hit_time = enter;
    *hit_normal = normal;
    return true;
}

static bool PushSphereOutOfBox(Vector3 *position,
                               const LocalCollisionBox *box, float radius,
                               Vector3 *normal)
{
    Vector3 closest = {
        fmaxf(box->minimum.x, fminf(position->x, box->maximum.x)),
        fmaxf(box->minimum.y, fminf(position->y, box->maximum.y)),
        fmaxf(box->minimum.z, fminf(position->z, box->maximum.z)),
    };
    Vector3 delta = Vector3Subtract(*position, closest);
    float distance = Vector3Length(delta);
    if (distance >= radius) return false;
    if (distance > 0.00001f) {
        *normal = Vector3Scale(delta, 1.0f / distance);
        *position = Vector3Add(*position,
                               Vector3Scale(*normal, radius - distance));
        return true;
    }

    float distances[6] = {
        position->x - box->minimum.x,
        box->maximum.x - position->x,
        position->y - box->minimum.y,
        box->maximum.y - position->y,
        position->z - box->minimum.z,
        box->maximum.z - position->z,
    };
    int32_t face = 0;
    for (int32_t candidate = 1; candidate < 6; ++candidate) {
        if (distances[candidate] < distances[face]) face = candidate;
    }
    *normal = (Vector3){0};
    if (face == 0) { normal->x = -1.0f; position->x = box->minimum.x - radius; }
    else if (face == 1) { normal->x = 1.0f; position->x = box->maximum.x + radius; }
    else if (face == 2) { normal->y = -1.0f; position->y = box->minimum.y - radius; }
    else if (face == 3) { normal->y = 1.0f; position->y = box->maximum.y + radius; }
    else if (face == 4) { normal->z = -1.0f; position->z = box->minimum.z - radius; }
    else { normal->z = 1.0f; position->z = box->maximum.z + radius; }
    return true;
}

static bool ProbeLocalCollision(void *raw_context,
                                CcBiomechVec3 previous_position,
                                CcBiomechVec3 proposed_position, float radius,
                                CcBiomechVec3 *corrected_position,
                                CcBiomechVec3 *surface_normal)
{
    const LocalProbeContext *context = raw_context;
    CcLocalSceneKind scene = context != NULL ? context->scene :
                                               CC_LOCAL_SCENE_STREET;
    Vector3 previous = {previous_position.x, previous_position.y,
                        previous_position.z};
    Vector3 proposed = {proposed_position.x, proposed_position.y,
                        proposed_position.z};
    LocalCollisionBox boxes[CC_LOCAL_COLLISION_BOX_CAPACITY];
    int32_t box_count = GatherLocalCollisionBoxes(context, proposed, boxes);
    if (context != NULL && context->root_capsule) {
        int32_t retained = 0;
        for (int32_t box = 0; box < box_count; ++box) {
            if (context->root_base_y >= boxes[box].maximum.y - 0.055f) {
                continue;
            }
            boxes[retained++] = boxes[box];
        }
        box_count = retained;
    }
    bool collided = false;
    Vector3 normal = {0.0f, 1.0f, 0.0f};

    float earliest = 1.0f;
    Vector3 swept_normal = {0};
    for (int32_t box = 0; box < box_count; ++box) {
        float hit_time = 1.0f;
        Vector3 hit_normal = {0};
        if (SweptPointAgainstBox(previous, proposed, &boxes[box], radius,
                                 &hit_time, &hit_normal) &&
            hit_time < earliest) {
            earliest = hit_time;
            swept_normal = hit_normal;
        }
    }
    if (earliest < 1.0f) {
        Vector3 movement = Vector3Subtract(proposed, previous);
        proposed = Vector3Add(previous, Vector3Scale(movement,
            fmaxf(0.0f, earliest - 0.0005f)));
        normal = swept_normal;
        collided = true;
    }

    for (int32_t iteration = 0; iteration < 3; ++iteration) {
        bool corrected = false;
        for (int32_t box = 0; box < box_count; ++box) {
            Vector3 box_normal = {0};
            if (!PushSphereOutOfBox(&proposed, &boxes[box], radius,
                                    &box_normal)) {
                continue;
            }
            normal = box_normal;
            corrected = true;
            collided = true;
        }
        if (!corrected) break;
    }

    float ground = BodySurfaceHeightAt(scene, proposed.x, proposed.z);
    if (proposed.y - radius < ground) {
        proposed.y = ground + radius;
        normal = SurfaceNormalAt(scene, proposed.x, proposed.z);
        collided = true;
    }
    if (!collided) return false;
    *corrected_position = (CcBiomechVec3){proposed.x, proposed.y, proposed.z};
    *surface_normal = (CcBiomechVec3){normal.x, normal.y, normal.z};
    return true;
}

bool CcLocalProbePhysicsSphereInternal(
    CcLocalSceneKind scene, Vector3 previous, Vector3 proposed, float radius,
    Vector3 *corrected, Vector3 *normal)
{
    if (corrected == NULL || normal == NULL || radius <= 0.0f) return false;
    LocalProbeContext context = {.scene = scene};
    CcBiomechVec3 resolved = {proposed.x, proposed.y, proposed.z};
    CcBiomechVec3 contact_normal = {0};
    bool collided = ProbeLocalCollision(
        &context,
        (CcBiomechVec3){previous.x, previous.y, previous.z},
        (CcBiomechVec3){proposed.x, proposed.y, proposed.z}, radius,
        &resolved, &contact_normal);
    *corrected = (Vector3){resolved.x, resolved.y, resolved.z};
    *normal = (Vector3){contact_normal.x, contact_normal.y,
                        contact_normal.z};
    return collided;
}

static bool LocalAgentCapsuleBlocked(CcLocalSceneKind scene,
                                     Vector3 previous, Vector3 proposed,
                                     float radius)
{
    LocalProbeContext context = {
        .scene = scene,
        .root_base_y = previous.y,
        .root_capsule = true,
    };
    const float heights[] = {radius, 0.92f, 1.54f};
    for (int32_t sample = 0;
         sample < (int32_t)(sizeof(heights) / sizeof(heights[0])); ++sample) {
        CcBiomechVec3 before = {previous.x, previous.y + heights[sample],
                                previous.z};
        CcBiomechVec3 after = {proposed.x, proposed.y + heights[sample],
                               proposed.z};
        CcBiomechVec3 corrected = after;
        CcBiomechVec3 normal = {0};
        if (!ProbeLocalCollision(&context, before, after, radius,
                                 &corrected, &normal)) {
            continue;
        }
        float proposed_x = corrected.x - after.x;
        float proposed_z = corrected.z - after.z;
        float proposed_correction = proposed_x * proposed_x +
                                    proposed_z * proposed_z;
        if (proposed_correction > 0.0001f * 0.0001f) {
            CcBiomechVec3 corrected_before = before;
            CcBiomechVec3 before_normal = {0};
            (void)ProbeLocalCollision(&context, before, before, radius,
                                      &corrected_before, &before_normal);
            float before_x = corrected_before.x - before.x;
            float before_z = corrected_before.z - before.z;
            float before_correction = before_x * before_x +
                                      before_z * before_z;
            if (before_correction > proposed_correction + 0.000001f) {
                continue;
            }
            return true;
        }
    }
    return false;
}

static int32_t LocalAgentPointSpace(const CcLocalAgent *agent,
                                    CcRobotCollisionPoint *points)
{
    if (agent == NULL || points == NULL) return 0;
    /* The humanoid already has a tuned standing capsule, swept ragdoll
       particles, and separate weapon contacts. The point-space proxy belongs
       to the generalized multi-leg rigs whose reach extends well beyond that
       root capsule. */
    if (agent->morphology == CC_MORPHOLOGY_BIPED) return 0;
    return CcRobotLimbPointSpace(
        &agent->limb_rig, 0.085f, points, CC_ROBOT_POINT_CAPACITY);
}

bool CcLocalAgentPointSpaceBlockedInternal(const CcLocalAgent *agent,
                                            Vector3 proposed)
{
    if (agent == NULL) return false;
    CcRobotCollisionPoint points[CC_ROBOT_POINT_CAPACITY];
    int32_t point_count = LocalAgentPointSpace(agent, points);
    Vector3 movement = Vector3Subtract(proposed, agent->position);
    LocalProbeContext context = {.scene = agent->scene};
    for (int32_t point = 0; point < point_count; ++point) {
        Vector3 before = {points[point].center.x, points[point].center.y,
                          points[point].center.z};
        Vector3 after = Vector3Add(before, movement);
        CcBiomechVec3 corrected = {after.x, after.y, after.z};
        CcBiomechVec3 normal = {0};
        if (!ProbeLocalCollision(
                &context,
                (CcBiomechVec3){before.x, before.y, before.z},
                (CcBiomechVec3){after.x, after.y, after.z},
                points[point].radius, &corrected, &normal)) {
            continue;
        }
        float correction_x = corrected.x - after.x;
        float correction_z = corrected.z - after.z;
        float proposed_correction = correction_x * correction_x +
                                    correction_z * correction_z;
        if (proposed_correction <= 0.0001f * 0.0001f) continue;

        CcBiomechVec3 corrected_before = {before.x, before.y, before.z};
        CcBiomechVec3 before_normal = {0};
        (void)ProbeLocalCollision(
            &context,
            (CcBiomechVec3){before.x, before.y, before.z},
            (CcBiomechVec3){before.x, before.y, before.z},
            points[point].radius, &corrected_before, &before_normal);
        float before_x = corrected_before.x - before.x;
        float before_z = corrected_before.z - before.z;
        float before_correction = before_x * before_x + before_z * before_z;
        if (before_correction > proposed_correction + 0.000001f) continue;
        return true;
    }
    return false;
}

static bool ResolveLocalAgentCapsuleMove(CcLocalSceneKind scene,
                                         Vector3 previous, Vector3 proposed,
                                         float radius,
                                         float passable_support_height,
                                         Vector3 *resolved,
                                         Vector3 *contact_normal)
{
    if (resolved == NULL || contact_normal == NULL) return false;
    Vector3 result = proposed;
    Vector3 normal = {0.0f, 1.0f, 0.0f};
    bool collided = false;
    bool below_passable_support =
        passable_support_height > -FLT_MAX &&
        proposed.y < passable_support_height - 0.0001f;
    const float heights[] = {radius, 0.92f, 1.54f};
    for (int32_t iteration = 0; iteration < 2; ++iteration) {
        bool iteration_collision = false;
        for (int32_t sample = 0;
             sample < (int32_t)(sizeof(heights) / sizeof(heights[0]));
             ++sample) {
            LocalProbeContext context = {
                .scene = scene,
                .root_base_y = fmaxf(result.y, passable_support_height),
                .root_capsule = true,
            };
            CcBiomechVec3 before = {
                previous.x, previous.y + heights[sample], previous.z};
            CcBiomechVec3 after = {
                result.x, result.y + heights[sample], result.z};
            CcBiomechVec3 corrected = after;
            CcBiomechVec3 sample_normal = {0};
            if (!ProbeLocalCollision(&context, before, after, radius,
                                     &corrected, &sample_normal)) {
                continue;
            }
            Vector3 candidate = {
                corrected.x,
                corrected.y - heights[sample],
                corrected.z,
            };
            bool crossing_passable_support =
                passable_support_height > -FLT_MAX * 0.5f &&
                result.y < passable_support_height - 0.001f &&
                candidate.y >= passable_support_height - 0.001f &&
                sample_normal.y > 0.90f;
            if (crossing_passable_support) {
                /* During a mantle the ledge box is passable once contacts
                   support the body. Its top is still a valid floor query,
                   but snapping the root to that floor would skip the swept
                   arc. Let the authored root reach the support height before
                   normal grounding resumes. */
                candidate.y = result.y;
            }
            Vector3 correction = Vector3Subtract(candidate, result);
            /* Traversal deliberately releases the platform's side wall while
               the authored root crosses the lip. Do not turn the resulting
               ground-height query into an early vertical teleport: the
               authored root will reach the support height at montage end. */
            if (below_passable_support && sample_normal.y > 0.50f &&
                correction.y > 0.0f && fabsf(correction.x) <= 0.00001f &&
                fabsf(correction.z) <= 0.00001f) {
                continue;
            }
            if (Vector3Length(correction) <= 0.00001f) continue;
            result = candidate;
            normal = (Vector3){sample_normal.x, sample_normal.y,
                               sample_normal.z};
            collided = true;
            iteration_collision = true;
        }
        if (!iteration_collision) break;
    }
    *resolved = result;
    *contact_normal = normal;
    return collided;
}

static bool LocalWorldSphereSweep(CcLocalSceneKind scene, Vector3 previous,
                                  Vector3 proposed, float radius,
                                  Vector3 *resolved, Vector3 *normal)
{
    LocalProbeContext context = {.scene = scene};
    CcBiomechVec3 corrected = {proposed.x, proposed.y, proposed.z};
    CcBiomechVec3 contact = {0};
    bool collided = ProbeLocalCollision(
        &context, (CcBiomechVec3){previous.x, previous.y, previous.z},
        (CcBiomechVec3){proposed.x, proposed.y, proposed.z}, radius,
        &corrected, &contact);
    if (resolved != NULL) {
        *resolved = (Vector3){corrected.x, corrected.y, corrected.z};
    }
    if (normal != NULL) {
        *normal = (Vector3){contact.x, contact.y, contact.z};
    }
    return collided;
}

static float LocalClimbContactSupport(CcLocalSceneKind scene, Vector3 point,
                                      Vector3 expected_normal,
                                      float requested_support)
{
    requested_support = fmaxf(0.0f, fminf(1.0f, requested_support));
    if (requested_support < 0.58f) return requested_support;
    float normal_length = sqrtf(expected_normal.x * expected_normal.x +
                                expected_normal.y * expected_normal.y +
                                expected_normal.z * expected_normal.z);
    if (normal_length <= 0.00001f) {
        expected_normal = (Vector3){0.0f, 1.0f, 0.0f};
    } else {
        expected_normal.x /= normal_length;
        expected_normal.y /= normal_length;
        expected_normal.z /= normal_length;
    }
    Vector3 outside = {
        point.x + expected_normal.x * 0.10f,
        point.y + expected_normal.y * 0.10f,
        point.z + expected_normal.z * 0.10f,
    };
    Vector3 inside = {
        point.x - expected_normal.x * 0.08f,
        point.y - expected_normal.y * 0.08f,
        point.z - expected_normal.z * 0.08f,
    };
    return LocalWorldSphereSweep(scene, outside, inside, 0.045f,
                                 NULL, NULL) ? requested_support : 0.0f;
}

static CcLimbVec3 ToLimbVector(Vector3 value)
{
    return (CcLimbVec3){value.x, value.y, value.z};
}

static Vector3 FromLimbVector(CcLimbVec3 value)
{
    return (Vector3){value.x, value.y, value.z};
}

static bool ProbeLocalSurface(void *raw_context, CcLimbVec3 origin,
                              float maximum_drop, CcLimbVec3 *point,
                              CcLimbVec3 *normal)
{
    const LocalProbeContext *context = raw_context;
    CcLocalSceneKind scene = context != NULL ? context->scene :
                                               CC_LOCAL_SCENE_STREET;
    if (StaticBodyBlocked(scene, origin.x, origin.z, 0.025f)) return false;
    float height = SurfaceHeightAt(scene, origin.x, origin.z) + 0.035f;
    if (height > origin.y + 0.05f || origin.y - height > maximum_drop) return false;
    *point = (CcLimbVec3){origin.x, height, origin.z};
    Vector3 surface_normal = SurfaceNormalAt(scene, origin.x, origin.z);
    *normal = (CcLimbVec3){surface_normal.x, surface_normal.y,
                           surface_normal.z};
    return true;
}

static Vector3 ShellPodBaseCenter(const CcLocalAgent *agent)
{
    return (Vector3){agent->position.x,
                     agent->position.y + agent->limb_rig.morphology.body_height,
                     agent->position.z};
}

static Vector3 ShellPodCenter(const CcLocalAgent *agent)
{
    Vector3 body = ShellPodBaseCenter(agent);
    body.y += agent->limb_rig.supported_height_offset;
    return body;
}

static const NavPlatform *ClimbPlatformAt(CcLocalSceneKind scene,
                                          float x, float z, float radius,
                                          float feet_height)
{
    if (scene != CC_LOCAL_SCENE_STREET) return NULL;
    const NavPlatform *highest = NULL;
    for (int32_t i = 0; i < StreetPhysicsPlatformCount(); ++i) {
        const NavPlatform *platform = StreetPhysicsPlatformAt(i);
        if (platform == NULL) continue;
        float top = PlatformTopHeight(platform);
        Rectangle footprint = {platform->x, platform->z,
                               platform->width, platform->depth};
        if (top > feet_height + 0.24f &&
            CircleTouchesFootprint(x, z, radius, footprint) &&
            (highest == NULL || top > PlatformTopHeight(highest))) {
            highest = platform;
        }
    }
    return highest;
}

static const NavPlatform *SupportingPlatformAt(float x, float z,
                                               float feet_height)
{
    for (int32_t i = 0; i < StreetPhysicsPlatformCount(); ++i) {
        const NavPlatform *platform = StreetPhysicsPlatformAt(i);
        if (platform == NULL) continue;
        if (fabsf(PlatformTopHeight(platform) - feet_height) > 0.08f) continue;
        if (x >= platform->x && x <= platform->x + platform->width &&
            z >= platform->z && z <= platform->z + platform->depth) {
            return platform;
        }
    }
    return NULL;
}

const char *CcLocalTraversalName(CcTraversalMode mode)
{
    switch (mode) {
        case CC_TRAVERSAL_WALK: return "WALK";
        case CC_TRAVERSAL_JUMP: return "JUMP";
        case CC_TRAVERSAL_VAULT: return "VAULT";
        case CC_TRAVERSAL_CLIMB: return "CLIMB";
        case CC_TRAVERSAL_DESCEND: return "DOWN-CLIMB";
        case CC_TRAVERSAL_SWIM: return "SWIM";
        case CC_TRAVERSAL_DROP: return "DROP";
        case CC_TRAVERSAL_RAGDOLL: return "RAGDOLL";
        case CC_TRAVERSAL_GET_UP: return "GET UP";
        case CC_TRAVERSAL_IDLE:
        default: return "IDLE";
    }
}

static float AthleticThreshold(int32_t level)
{
    return 30.0f + (float)level * 20.0f;
}

static bool AthleticDisciplineValid(CcAthleticDiscipline discipline)
{
    return discipline >= 0 && discipline < CC_ATHLETIC_DISCIPLINE_COUNT;
}

static int32_t AthleticLevel(const CcLocalAgent *agent,
                             CcAthleticDiscipline discipline)
{
    if (agent == NULL || !AthleticDisciplineValid(discipline)) return 1;
    int32_t level = agent->athletics.level[discipline];
    if (level < 1) return 1;
    return level > CC_ATHLETIC_MAX_LEVEL ? CC_ATHLETIC_MAX_LEVEL : level;
}

static float AthleticBonus(const CcLocalAgent *agent,
                           CcAthleticDiscipline discipline, float per_level)
{
    return 1.0f + (float)(AthleticLevel(agent, discipline) - 1) * per_level;
}

void CcLocalAgentTrainAthleticism(CcLocalAgent *agent,
                                  CcAthleticDiscipline discipline,
                                  float experience)
{
    if (agent == NULL || !AthleticDisciplineValid(discipline) ||
        !isfinite(experience) || experience <= 0.0f) {
        return;
    }
    int32_t *level = &agent->athletics.level[discipline];
    if (*level < 1) *level = 1;
    if (*level >= CC_ATHLETIC_MAX_LEVEL) return;
    float *stored = &agent->athletics.experience[discipline];
    *stored += experience;
    while (*level < CC_ATHLETIC_MAX_LEVEL &&
           *stored >= AthleticThreshold(*level)) {
        *stored -= AthleticThreshold(*level);
        *level += 1;
    }
    if (*level >= CC_ATHLETIC_MAX_LEVEL) *stored = 0.0f;
}

void CcLocalAgentSetAthleticLevel(CcLocalAgent *agent,
                                  CcAthleticDiscipline discipline,
                                  int32_t level)
{
    if (agent == NULL || !AthleticDisciplineValid(discipline)) return;
    agent->athletics.level[discipline] =
        level < 1 ? 1 : level > CC_ATHLETIC_MAX_LEVEL ?
        CC_ATHLETIC_MAX_LEVEL : level;
    agent->athletics.experience[discipline] = 0.0f;
}

int32_t CcLocalAgentHeroicTier(const CcLocalAgent *agent)
{
    if (agent == NULL) return 1;
    int32_t total = 0;
    for (int32_t discipline = 0;
         discipline < CC_ATHLETIC_DISCIPLINE_COUNT; ++discipline) {
        total += AthleticLevel(agent, (CcAthleticDiscipline)discipline);
    }
    return total / CC_ATHLETIC_DISCIPLINE_COUNT;
}

float CcLocalAgentAthleticProgress(const CcLocalAgent *agent,
                                   CcAthleticDiscipline discipline)
{
    if (agent == NULL || !AthleticDisciplineValid(discipline)) return 0.0f;
    int32_t level = AthleticLevel(agent, discipline);
    if (level >= CC_ATHLETIC_MAX_LEVEL) return 1.0f;
    return fmaxf(0.0f, fminf(agent->athletics.experience[discipline] /
                             AthleticThreshold(level), 1.0f));
}

const char *CcAthleticDisciplineName(CcAthleticDiscipline discipline)
{
    switch (discipline) {
        case CC_ATHLETIC_MOBILITY: return "MOBILITY";
        case CC_ATHLETIC_GRIP: return "GRIP";
        case CC_ATHLETIC_POWER: return "POWER";
        default: return "ATHLETIC";
    }
}

Vector2 CcLocalAgentPosition(const CcLocalAgent *agent)
{
    return (Vector2){agent->position.x, agent->position.z};
}

static CcLocalSceneKind AgentSceneForCall(const CcLocalAgent *agent,
                                          bool market_interior)
{
    if (market_interior) return CC_LOCAL_SCENE_MARKET;
    return agent != NULL && agent->scene == CC_LOCAL_SCENE_ROAD ?
           CC_LOCAL_SCENE_ROAD : CC_LOCAL_SCENE_STREET;
}

static void ApplyAgentWalkingProfile(CcLocalAgent *agent)
{
    if (agent == NULL || agent->morphology != CC_MORPHOLOGY_BIPED ||
        !agent->humanoid.initialized) return;
    float signature_weight = agent->crowned ? 0.0f : 0.40f;
    float cadence_scale = 1.0f +
        (agent->appearance.gait_cadence_scale - 1.0f) * signature_weight;
    float stride_scale = 1.0f +
        (agent->appearance.stride_scale - 1.0f) * signature_weight;
    CcHumanoidGaitSetWalkingProfile(
        &agent->humanoid, cadence_scale, stride_scale);
}

void CcLocalAgentInit(CcLocalAgent *agent, Vector2 position, bool market_interior)
{
    *agent = (CcLocalAgent){0};
    agent->scene = market_interior ? CC_LOCAL_SCENE_MARKET :
                                     CC_LOCAL_SCENE_STREET;
    agent->position = (Vector3){position.x,
                                SurfaceHeightAt(agent->scene, position.x,
                                                position.y),
                                position.y};
    agent->facing_yaw = 0.75f * PI;
    agent->traversal = CC_TRAVERSAL_IDLE;
    agent->support_state = CC_HUMANOID_SUPPORT_STABLE;
    agent->radius = PLAYER_COLLISION_RADIUS;
    agent->grounded = true;
    agent->allow_downclimb = true;
    agent->crowned = true;
    agent->appearance = CcNpcCrownlessAppearance();
    agent->tunic_color = agent->appearance.outer;
    agent->target_point = agent->position;
    agent->combat.health = CC_LOCAL_COMBAT_MAX_HEALTH;
    agent->combat.posture = CC_LOCAL_COMBAT_MAX_POSTURE;
    agent->combat.life_state = CC_LIFE_ALIVE;
    agent->combat.target_index = -1;
    agent->combat.queued_skill = -1;
    agent->combat.active_skill = -1;
    for (int32_t discipline = 0;
         discipline < CC_ATHLETIC_DISCIPLINE_COUNT; ++discipline) {
        agent->athletics.level[discipline] = 1;
    }
    CcLocalAgentSetMorphology(agent, CC_MORPHOLOGY_BIPED, market_interior);
}

void CcLocalAgentSetNpcAppearance(CcLocalAgent *agent, uint32_t seed,
                                  CcNpcRole role, Color accent)
{
    if (agent == NULL) return;
    agent->appearance = CcNpcAppearanceGenerate(seed, role, accent);
    agent->tunic_color = agent->appearance.outer;
    ApplyAgentWalkingProfile(agent);
}

void CcLocalAgentSetMorphology(CcLocalAgent *agent, CcMorphologyPreset preset,
                               bool market_interior)
{
    CcLimbMorphology morphology;
    if (!CcLimbMorphologyFromPreset(&morphology, preset)) return;
    agent->morphology = preset;
    agent->ragdoll_visual_blend = 0.0f;
    agent->scene = AgentSceneForCall(agent, market_interior);
    LocalProbeContext context = {.scene = agent->scene};
    Vector3 body = {agent->position.x,
                    agent->position.y + morphology.body_height,
                    agent->position.z};
    CcLimbRigInit(&agent->limb_rig, &morphology, ToLimbVector(body),
                  agent->facing_yaw, ProbeLocalSurface, &context);
    agent->humanoid_needs_reset = false;
    if (preset == CC_MORPHOLOGY_BIPED) {
        CcHumanoidGaitInit(&agent->humanoid, ToLimbVector(agent->position),
                            agent->facing_yaw, ProbeLocalSurface, &context);
        ApplyAgentWalkingProfile(agent);
        agent->support_state = agent->humanoid.support_state;
        agent->render_pose = agent->humanoid.pose;
        agent->stepped_pose = (CcSteppedPoseState){0};
        agent->render_pose_valid = true;
        agent->simulation_accumulator = 0.0f;
        agent->cape = (CcLocalCapeState){0};
        UpdateHeroCape(agent, 1.0f / 60.0f);
        agent->previous_cape = agent->cape;
        agent->render_cape = agent->cape;
    } else {
        agent->humanoid = (CcHumanoidGait){0};
        agent->render_pose = (CcHumanoidPose){0};
        agent->stepped_pose = (CcSteppedPoseState){0};
        agent->render_pose_valid = false;
        agent->simulation_accumulator = 0.0f;
        agent->cape = (CcLocalCapeState){0};
        agent->previous_cape = (CcLocalCapeState){0};
        agent->render_cape = (CcLocalCapeState){0};
    }
}

void CcLocalAgentSetScene(CcLocalAgent *agent, CcLocalSceneKind scene)
{
    if (agent == NULL || scene < CC_LOCAL_SCENE_STREET ||
        scene > CC_LOCAL_SCENE_ROAD) return;
    agent->scene = scene;
    agent->position.y = SurfaceHeightAt(
        scene, agent->position.x, agent->position.z);
    agent->velocity = (Vector3){0};
    agent->separation_velocity = (Vector3){0};
    agent->target_point = agent->position;
    agent->command_point = agent->position;
    agent->command_origin = agent->position;
    agent->exact_target_valid = false;
    agent->target_valid = false;
    agent->command_point_valid = false;
    agent->navigation_active = false;
    agent->navigation_world_exit = false;
    agent->navigation_point_count = 0;
    agent->navigation_point_index = 0;
    agent->navigation_destination_room = -1;
    agent->climbing = false;
    agent->climbing_down = false;
    agent->vaulting = false;
    agent->swimming = false;
    agent->grounded = true;
    agent->traversal = CC_TRAVERSAL_IDLE;
    CcLocalAgentSetMorphology(
        agent, agent->morphology, scene == CC_LOCAL_SCENE_MARKET);
}

void CcLocalAgentCycleMorphology(CcLocalAgent *agent, bool market_interior)
{
    CcMorphologyPreset next = (CcMorphologyPreset)(agent->morphology + 1);
    if (next >= CC_MORPHOLOGY_PRESET_COUNT) next = CC_MORPHOLOGY_BIPED;
    CcLocalAgentSetMorphology(agent, next, market_interior);
}

const char *CcLocalAgentMorphologyName(const CcLocalAgent *agent)
{
    return agent->limb_rig.morphology.name != NULL ?
           agent->limb_rig.morphology.name : "UNRIGGED";
}

static float CombatClamp(float value, float minimum, float maximum)
{
    return fmaxf(minimum, fminf(value, maximum));
}

static bool CombatCanAct(const CcCombatState *combat)
{
    return combat != NULL && combat->life_state == CC_LIFE_ALIVE;
}

static bool CombatIsDefeated(const CcCombatState *combat)
{
    return combat != NULL &&
        (combat->life_state == CC_LIFE_DEAD ||
         combat->life_state == CC_LIFE_RESPAWNING);
}

static Vector3 CombatSubtract(Vector3 a, Vector3 b)
{
    return (Vector3){a.x - b.x, a.y - b.y, a.z - b.z};
}

static Vector3 CombatAdd(Vector3 a, Vector3 b)
{
    return (Vector3){a.x + b.x, a.y + b.y, a.z + b.z};
}

static Vector3 CombatScale(Vector3 value, float scale)
{
    return (Vector3){value.x * scale, value.y * scale, value.z * scale};
}

static float CombatDot(Vector3 a, Vector3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static float CombatLengthSquared(Vector3 value)
{
    return CombatDot(value, value);
}

static Vector3 CombatNormalizeOr(Vector3 value, Vector3 fallback)
{
    float length_squared = CombatLengthSquared(value);
    if (length_squared <= 0.000001f) return fallback;
    return CombatScale(value, 1.0f / sqrtf(length_squared));
}

static Vector3 CombatDirectionBlend(Vector3 first, Vector3 second,
                                    float amount, Vector3 fallback)
{
    amount = CombatClamp(amount, 0.0f, 1.0f);
    return CombatNormalizeOr(
        CombatAdd(CombatScale(first, 1.0f - amount),
                  CombatScale(second, amount)), fallback);
}

static Vector3 CombatWeaponDirectionAt(const CcLocalAgent *agent,
                                       float action_time)
{
    Vector3 forward = {sinf(agent->facing_yaw), 0.0f,
                       cosf(agent->facing_yaw)};
    Vector3 right = {cosf(agent->facing_yaw), 0.0f,
                     -sinf(agent->facing_yaw)};
    const Vector3 up = {0.0f, 1.0f, 0.0f};
    Vector3 guard = CombatNormalizeOr(
        CombatAdd(CombatScale(forward, 0.66f),
                  CombatAdd(CombatScale(right, 0.24f),
                            CombatScale(up, 0.62f))), forward);
    if (agent->humanoid.action != CC_HUMANOID_ACTION_STRIKE) {
        if (agent->humanoid.action == CC_HUMANOID_ACTION_GUARD ||
            agent->combat.focus_valid) {
            return guard;
        }
        return CombatNormalizeOr(
            CombatAdd(CombatScale(up, -0.80f),
                      CombatAdd(CombatScale(right, 0.52f),
                                CombatScale(forward, 0.12f))), forward);
    }

    float phase = CombatClamp(action_time / 1.20f, 0.0f, 1.0f);
    Vector3 chamber;
    Vector3 cut;
    float chamber_end = 0.22f;
    float cut_end = 0.58f;
    if (agent->combat.active_skill == CC_COMBAT_SKILL_CRUSHING_BLOW) {
        chamber = CombatNormalizeOr(
            CombatAdd(CombatScale(up, 0.96f),
                      CombatAdd(CombatScale(right, 0.14f),
                                CombatScale(forward, -0.20f))), guard);
        cut = CombatNormalizeOr(
            CombatAdd(CombatScale(forward, 0.90f),
                      CombatAdd(CombatScale(up, -0.42f),
                                CombatScale(right, -0.08f))), forward);
        chamber_end = 0.32f;
        cut_end = 0.64f;
    } else if (agent->combat.active_skill == CC_COMBAT_SKILL_SUNDER) {
        chamber = CombatNormalizeOr(
            CombatAdd(CombatScale(right, 0.94f),
                      CombatAdd(CombatScale(forward, -0.22f),
                                CombatScale(up, 0.24f))), guard);
        cut = CombatNormalizeOr(
            CombatAdd(CombatScale(right, -0.82f),
                      CombatAdd(CombatScale(forward, 0.56f),
                                CombatScale(up, 0.08f))), forward);
        chamber_end = 0.26f;
        cut_end = 0.60f;
    } else {
        chamber = CombatNormalizeOr(
            CombatAdd(CombatScale(right, 0.72f),
                      CombatAdd(CombatScale(up, 0.66f),
                                CombatScale(forward, -0.20f))), guard);
        cut = CombatNormalizeOr(
            CombatAdd(CombatScale(right, -0.58f),
                      CombatAdd(CombatScale(forward, 0.78f),
                                CombatScale(up, 0.18f))), forward);
    }
    if (phase < chamber_end) {
        return CombatDirectionBlend(guard, chamber,
            SmoothStep01(phase / chamber_end), guard);
    }
    if (phase < cut_end) {
        return CombatDirectionBlend(chamber, cut,
            SmoothStep01((phase - chamber_end) /
                         (cut_end - chamber_end)), cut);
    }
    return CombatDirectionBlend(cut, guard,
        SmoothStep01((phase - cut_end) / (1.0f - cut_end)), guard);
}

static float CombatHorizontalDistanceSquared(const CcLocalAgent *a,
                                             const CcLocalAgent *b)
{
    float x = b->position.x - a->position.x;
    float z = b->position.z - a->position.z;
    return x * x + z * z;
}

static float CombatFacingDot(const CcLocalAgent *agent, Vector3 point)
{
    float x = point.x - agent->position.x;
    float z = point.z - agent->position.z;
    float length = sqrtf(x * x + z * z);
    if (length <= 0.0001f) return 1.0f;
    return (sinf(agent->facing_yaw) * x + cosf(agent->facing_yaw) * z) /
           length;
}

static float CombatSegmentDistanceSquared(Vector3 first_start,
                                          Vector3 first_end,
                                          Vector3 second_start,
                                          Vector3 second_end)
{
    const float epsilon = 0.000001f;
    Vector3 first = CombatSubtract(first_end, first_start);
    Vector3 second = CombatSubtract(second_end, second_start);
    Vector3 offset = CombatSubtract(first_start, second_start);
    float first_length = CombatDot(first, first);
    float second_length = CombatDot(second, second);
    float second_offset = CombatDot(second, offset);
    float first_amount = 0.0f;
    float second_amount = 0.0f;
    if (first_length <= epsilon && second_length <= epsilon) {
        return CombatLengthSquared(offset);
    }
    if (first_length <= epsilon) {
        second_amount = CombatClamp(second_offset / second_length, 0.0f, 1.0f);
    } else {
        float first_offset = CombatDot(first, offset);
        if (second_length <= epsilon) {
            first_amount = CombatClamp(-first_offset / first_length, 0.0f,
                                       1.0f);
        } else {
            float directions = CombatDot(first, second);
            float denominator = first_length * second_length -
                                directions * directions;
            if (fabsf(denominator) > epsilon) {
                first_amount = CombatClamp(
                    (directions * second_offset -
                     first_offset * second_length) / denominator,
                    0.0f, 1.0f);
            }
            float second_numerator = directions * first_amount + second_offset;
            if (second_numerator < 0.0f) {
                second_amount = 0.0f;
                first_amount = CombatClamp(-first_offset / first_length,
                                           0.0f, 1.0f);
            } else if (second_numerator > second_length) {
                second_amount = 1.0f;
                first_amount = CombatClamp(
                    (directions - first_offset) / first_length, 0.0f, 1.0f);
            } else {
                second_amount = second_numerator / second_length;
            }
        }
    }
    Vector3 first_closest = CombatAdd(first_start,
                                      CombatScale(first, first_amount));
    Vector3 second_closest = CombatAdd(second_start,
                                       CombatScale(second, second_amount));
    return CombatLengthSquared(CombatSubtract(first_closest, second_closest));
}

static Vector3 CombatClosestPointOnSegment(Vector3 start, Vector3 end,
                                           Vector3 point)
{
    Vector3 segment = CombatSubtract(end, start);
    float length_squared = CombatLengthSquared(segment);
    if (length_squared <= 0.000001f) return start;
    float amount = CombatClamp(
        CombatDot(CombatSubtract(point, start), segment) / length_squared,
        0.0f, 1.0f);
    return CombatAdd(start, CombatScale(segment, amount));
}

static float CombatWeaponExtension(CcCombatTeam team)
{
    if (team == CC_COMBAT_GUARD) return 0.78f;
    if (team == CC_COMBAT_RAIDER) return 0.86f;
    return 1.02f;
}

static float CombatStrikeReach(CcCombatTeam team)
{
    if (team == CC_COMBAT_GUARD) return 1.62f;
    if (team == CC_COMBAT_RAIDER) return 1.42f;
    return 1.56f;
}

static float CombatStrikeDamage(const CcLocalAgent *attacker)
{
    CcCombatTeam team = attacker != NULL ? attacker->combat.team :
                                           CC_COMBAT_NEUTRAL;
    float base = 24.0f;
    if (team == CC_COMBAT_GUARD) base = 21.0f;
    if (team == CC_COMBAT_RAIDER) base = 19.0f;
    return base * AthleticBonus(attacker, CC_ATHLETIC_POWER, 0.08f);
}

static bool CombatTeamsHostile(CcCombatTeam first, CcCombatTeam second)
{
    if (first == CC_COMBAT_NEUTRAL || second == CC_COMBAT_NEUTRAL) {
        return false;
    }
    bool first_raider = first == CC_COMBAT_RAIDER;
    bool second_raider = second == CC_COMBAT_RAIDER;
    return first_raider != second_raider;
}

static void CombatApplyKnockback(CcLocalAgent *attacker,
                                 CcLocalAgent *defender,
                                 Vector3 direction, float speed)
{
    float x = direction.x;
    float z = direction.z;
    float length = sqrtf(x * x + z * z);
    if (length <= 0.0001f) {
        x = defender->position.x - attacker->position.x;
        z = defender->position.z - attacker->position.z;
        length = sqrtf(x * x + z * z);
        if (length <= 0.0001f) {
            x = sinf(attacker->facing_yaw);
            z = cosf(attacker->facing_yaw);
            length = 1.0f;
        }
    }
    defender->combat.knockback_velocity.x = x / length * speed;
    defender->combat.knockback_velocity.z = z / length * speed;
    if (defender->humanoid.initialized &&
        !defender->humanoid.ragdoll.active) {
        defender->humanoid.body.root.velocity.x += x / length * speed * 0.46f;
        defender->humanoid.body.root.velocity.z += z / length * speed * 0.46f;
        defender->humanoid.root_velocity.x =
            defender->humanoid.body.root.velocity.x;
        defender->humanoid.root_velocity.z =
            defender->humanoid.body.root.velocity.z;
    }
}

static void CombatDefeat(CcLocalAgent *agent, Vector3 impact_direction,
                         Vector3 impact_point, float impact_speed)
{
    agent->combat.health = 0.0f;
    agent->combat.life_state = CC_LIFE_DEAD;
    agent->combat.weapon_mode = CC_WEAPON_RAGDOLL_ATTACHED;
    agent->combat.focus_valid = false;
    agent->combat.target_index = -1;
    agent->combat.queued_skill = -1;
    agent->combat.active_skill = -1;
    agent->exact_target_valid = false;
    agent->combat.stagger_seconds = 1.10f;
    agent->combat.respawn_seconds = agent->combat.team == CC_COMBAT_PLAYER ?
                                    2.75f : 0.0f;
    agent->combat.knockback_velocity = (Vector3){0};
    CcHumanoidGaitSetGuarded(&agent->humanoid, false);
    (void)CcHumanoidGaitDie(&agent->humanoid,
                            ToLimbVector(impact_direction),
                            ToLimbVector(impact_point), impact_speed);
}

void CcLocalCombatSetTeam(CcLocalAgent *agent, CcCombatTeam team)
{
    if (agent == NULL) return;
    agent->combat.team = team;
    agent->combat.weapon_mode = team == CC_COMBAT_PLAYER ||
                                team == CC_COMBAT_RAIDER ?
                                CC_WEAPON_HELD : CC_WEAPON_NONE;
}

void CcLocalCombatSetFocus(CcLocalAgent *agent,
                           const CcLocalAgent *target)
{
    if (agent == NULL) return;
    if (target == NULL || !CombatCanAct(&target->combat)) {
        CcLocalCombatClearFocus(agent);
        return;
    }
    agent->combat.focus_point = target->position;
    agent->combat.focus_valid = true;
}

void CcLocalCombatClearFocus(CcLocalAgent *agent)
{
    if (agent == NULL) return;
    agent->combat.focus_valid = false;
    agent->combat.target_index = -1;
}

void CcLocalCombatSetGuarded(CcLocalAgent *agent,
                             const CcLocalAgent *target, bool guarded)
{
    if (agent == NULL) return;
    if (guarded && target != NULL) CcLocalCombatSetFocus(agent, target);
    if (!CombatCanAct(&agent->combat) ||
        agent->combat.stagger_seconds > 0.0f) {
        guarded = false;
    }
    CcHumanoidGaitSetGuarded(&agent->humanoid, guarded);
    if (!guarded && agent->humanoid.action != CC_HUMANOID_ACTION_STRIKE) {
        CcLocalCombatClearFocus(agent);
    }
}

bool CcLocalCombatBeginStrike(CcLocalAgent *agent,
                              const CcLocalAgent *target)
{
    if (agent == NULL || !CombatCanAct(&agent->combat) ||
        agent->combat.stagger_seconds > 0.0f || !agent->grounded) return false;
    if (target != NULL) CcLocalCombatSetFocus(agent, target);
    if (!CcHumanoidGaitBeginStrike(&agent->humanoid, 1)) return false;
    agent->combat.strike_resolved = false;
    agent->combat.active_skill = -1;
    return true;
}

bool CcLocalAgentJump(CcLocalAgent *agent)
{
    if (agent == NULL || agent->morphology != CC_MORPHOLOGY_BIPED ||
        !agent->grounded || agent->climbing || agent->swimming ||
        !CombatCanAct(&agent->combat) ||
        agent->combat.stagger_seconds > 0.0f ||
        !CcHumanoidGaitBeginJump(&agent->humanoid)) {
        return false;
    }
    float takeoff_speed = 4.35f +
        (float)(AthleticLevel(agent, CC_ATHLETIC_MOBILITY) - 1) * 0.16f;
    agent->velocity.y = takeoff_speed;
    agent->grounded = false;
    agent->jump_training_pending = true;
    agent->humanoid.body.root.velocity.y = takeoff_speed;
    agent->humanoid.root_velocity.y = takeoff_speed;
    return true;
}

CcCombatOutcome CcLocalCombatResolveStrike(CcLocalAgent *attacker,
                                           CcLocalAgent *defender)
{
    if (attacker == NULL || attacker->combat.strike_resolved) {
        return CC_COMBAT_OUTCOME_NONE;
    }
    attacker->combat.strike_resolved = true;
    float damage_scale = attacker->combat.strike_damage_scale > 0.0f ?
                         attacker->combat.strike_damage_scale : 1.0f;
    float posture_scale = attacker->combat.strike_posture_scale > 0.0f ?
                          attacker->combat.strike_posture_scale : 1.0f;
    float knockback_scale = attacker->combat.strike_knockback_scale > 0.0f ?
                            attacker->combat.strike_knockback_scale : 1.0f;
    attacker->combat.strike_damage_scale = 0.0f;
    attacker->combat.strike_posture_scale = 0.0f;
    attacker->combat.strike_knockback_scale = 0.0f;
    if (defender == NULL || attacker == defender ||
        !CombatCanAct(&attacker->combat) ||
        !CombatCanAct(&defender->combat) ||
        !CombatTeamsHostile(attacker->combat.team, defender->combat.team) ||
        attacker->humanoid.ragdoll.active || attacker->humanoid.recovering ||
        defender->humanoid.ragdoll.active || defender->humanoid.recovering ||
        defender->climbing || defender->swimming) {
        return CC_COMBAT_OUTCOME_MISS;
    }
    float reach = CombatStrikeReach(attacker->combat.team);
    if (CombatHorizontalDistanceSquared(attacker, defender) > reach * reach ||
        fabsf(attacker->position.y - defender->position.y) > 0.62f ||
        CombatFacingDot(attacker, defender->position) < 0.24f) {
        return CC_COMBAT_OUTCOME_MISS;
    }

    Vector3 world_contact = {0};
    Vector3 world_normal = {0};
    Vector3 attack_center = {
        attacker->position.x, attacker->position.y + 0.94f,
        attacker->position.z};
    Vector3 defense_center = {
        defender->position.x, defender->position.y + 0.94f,
        defender->position.z};
    if (LocalWorldSphereSweep(attacker->scene, attack_center, defense_center,
                              0.045f, &world_contact, &world_normal)) {
        attacker->combat.impact_point = world_contact;
        attacker->combat.impact_direction = world_normal;
        attacker->combat.impact_speed = 0.0f;
        attacker->combat.impact_valid = true;
        return CC_COMBAT_OUTCOME_MISS;
    }

    int32_t arm = attacker->humanoid.strike_side == 0 ? 0 : 1;
    Vector3 previous_hand = FromLimbVector(
        attacker->humanoid.previous_pose.hand[arm]);
    Vector3 current_hand = FromLimbVector(attacker->humanoid.pose.hand[arm]);
    float extension = CombatWeaponExtension(attacker->combat.team);
    Vector3 previous_direction = CombatWeaponDirectionAt(
        attacker, fmaxf(0.0f, attacker->humanoid.action_time -
                              attacker->humanoid.last_delta_time));
    Vector3 current_direction = CombatWeaponDirectionAt(
        attacker, attacker->humanoid.action_time);
    Vector3 previous_tip = CombatAdd(previous_hand,
                                     CombatScale(previous_direction,
                                                 extension));
    Vector3 current_tip = CombatAdd(current_hand,
                                    CombatScale(current_direction,
                                                extension));
    const float weapon_radius = 0.045f;
    if (LocalWorldSphereSweep(attacker->scene, previous_hand, previous_tip,
                              weapon_radius, &world_contact, &world_normal) ||
        LocalWorldSphereSweep(attacker->scene, previous_tip, current_tip,
                              weapon_radius, &world_contact, &world_normal) ||
        LocalWorldSphereSweep(attacker->scene, current_hand, current_tip,
                              weapon_radius, &world_contact, &world_normal)) {
        attacker->combat.impact_point = world_contact;
        attacker->combat.impact_direction = world_normal;
        attacker->combat.impact_speed = 0.0f;
        attacker->combat.impact_valid = true;
        return CC_COMBAT_OUTCOME_MISS;
    }
    Vector3 body_bottom = {defender->position.x,
                           defender->position.y + 0.38f,
                           defender->position.z};
    Vector3 body_top = {defender->position.x,
                        defender->position.y + 1.52f,
                        defender->position.z};
    float collision_distance = CombatSegmentDistanceSquared(
        current_hand, current_tip, body_bottom, body_top);
    collision_distance = fminf(collision_distance,
        CombatSegmentDistanceSquared(previous_hand, previous_tip,
                                     body_bottom, body_top));
    collision_distance = fminf(collision_distance,
        CombatSegmentDistanceSquared(previous_tip, current_tip,
                                     body_bottom, body_top));
    /* The broad reach check is the gameplay contract; this swept capsule
       accounts for torso breadth and the small visual offsets introduced by
       bent-arm IK so presentation changes cannot silently shorten attacks. */
    const float combined_radius = 0.62f;
    if (collision_distance > combined_radius * combined_radius) {
        return CC_COMBAT_OUTCOME_MISS;
    }

    Vector3 tip_motion = CombatSubtract(current_tip, previous_tip);
    float tip_travel = sqrtf(CombatLengthSquared(tip_motion));
    Vector3 impact_direction = tip_travel > 0.001f ?
        CombatScale(tip_motion, 1.0f / tip_travel) : current_direction;
    float impact_speed = tip_travel /
        fmaxf(attacker->humanoid.last_delta_time, 1.0f / 240.0f);
    impact_speed = CombatClamp(impact_speed, 2.4f, 9.0f);
    float impact_scale = CombatClamp(0.66f + impact_speed * 0.09f,
                                     0.82f, 1.36f);
    impact_scale *= AthleticBonus(attacker, CC_ATHLETIC_POWER, 0.055f);

    Vector3 impact_sample = current_tip;
    Vector3 impact_point = CombatClosestPointOnSegment(
        body_bottom, body_top, impact_sample);
    Vector3 guard_left = FromLimbVector(defender->humanoid.pose.hand[0]);
    Vector3 guard_right = FromLimbVector(defender->humanoid.pose.hand[1]);
    float guard_distance = CombatSegmentDistanceSquared(
        current_hand, current_tip, guard_left, guard_right);
    guard_distance = fminf(guard_distance, CombatSegmentDistanceSquared(
        previous_hand, previous_tip, guard_left, guard_right));
    guard_distance = fminf(guard_distance, CombatSegmentDistanceSquared(
        previous_tip, current_tip, guard_left, guard_right));
    bool guarded = defender->humanoid.action == CC_HUMANOID_ACTION_GUARD &&
                   defender->combat.posture > 0.0f &&
                   CombatFacingDot(defender, attacker->position) >= 0.28f &&
                   guard_distance <= 0.58f * 0.58f;
    if (guarded) {
        impact_point = CombatClosestPointOnSegment(
            guard_left, guard_right, impact_sample);
    }
    defender->combat.impact_point = impact_point;
    defender->combat.impact_direction = impact_direction;
    defender->combat.impact_speed = impact_speed;
    defender->combat.impact_valid = true;
    CcHumanoidGaitApplyImpact(
        &defender->humanoid, ToLimbVector(impact_direction),
        guarded ? 0.24f : CombatClamp(impact_scale * 0.72f, 0.0f, 1.0f));
    attacker->combat.hitstop_seconds = fmaxf(
        attacker->combat.hitstop_seconds, guarded ? 0.040f : 0.055f);
    defender->combat.hitstop_seconds = fmaxf(
        defender->combat.hitstop_seconds, guarded ? 0.050f : 0.075f);
    defender->combat.hit_flash_seconds = fmaxf(
        defender->combat.hit_flash_seconds, guarded ? 0.075f : 0.14f);
    if (guarded) {
        float posture_damage = attacker->combat.team == CC_COMBAT_GUARD ?
                               42.0f :
                               attacker->combat.team == CC_COMBAT_RAIDER ?
                               24.0f : 32.0f;
        posture_damage *= impact_scale * posture_scale;
        defender->combat.posture = fmaxf(0.0f,
                                         defender->combat.posture -
                                         posture_damage);
        CcHumanoidGaitApplyImpact(
            &attacker->humanoid, ToLimbVector(CombatScale(
                impact_direction, -1.0f)), 0.22f);
        CcLocalAgentTrainAthleticism(attacker, CC_ATHLETIC_POWER, 6.0f);
        CcLocalAgentTrainAthleticism(defender, CC_ATHLETIC_POWER, 7.0f);
        if (defender->combat.posture > 0.0f) {
            CombatApplyKnockback(attacker, defender, impact_direction,
                                 0.34f * impact_scale * knockback_scale);
            return CC_COMBAT_OUTCOME_BLOCKED;
        }
        CcHumanoidGaitSetGuarded(&defender->humanoid, false);
        defender->combat.health = fmaxf(
            0.0f, defender->combat.health -
            CombatStrikeDamage(attacker) * impact_scale * 0.65f *
            damage_scale);
        defender->combat.stagger_seconds = 0.78f;
        CcLocalAgentTrainAthleticism(attacker, CC_ATHLETIC_POWER, 12.0f);
        if (defender->combat.health <= 0.0f) {
            CcLocalAgentTrainAthleticism(attacker, CC_ATHLETIC_POWER, 12.0f);
            CombatDefeat(defender, impact_direction, impact_point,
                         1.22f * impact_scale * knockback_scale);
            return CC_COMBAT_OUTCOME_DEFEATED;
        }
        CombatApplyKnockback(attacker, defender, impact_direction,
                             1.22f * impact_scale * knockback_scale);
        return CC_COMBAT_OUTCOME_GUARD_BROKEN;
    }

    defender->combat.health = fmaxf(
        0.0f, defender->combat.health -
        CombatStrikeDamage(attacker) * impact_scale * damage_scale);
    defender->combat.posture = fmaxf(
        0.0f, defender->combat.posture -
        12.0f * impact_scale * posture_scale);
    defender->combat.stagger_seconds = 0.34f;
    CcLocalAgentTrainAthleticism(attacker, CC_ATHLETIC_POWER, 12.0f);
    if (defender->combat.health <= 0.0f) {
        CcLocalAgentTrainAthleticism(attacker, CC_ATHLETIC_POWER, 12.0f);
        CombatDefeat(defender, impact_direction, impact_point,
                     1.06f * impact_scale * knockback_scale);
        return CC_COMBAT_OUTCOME_DEFEATED;
    }
    CombatApplyKnockback(attacker, defender, impact_direction,
                         1.06f * impact_scale * knockback_scale);
    return CC_COMBAT_OUTCOME_HIT;
}

const char *CcLocalCombatOutcomeName(CcCombatOutcome outcome)
{
    switch (outcome) {
        case CC_COMBAT_OUTCOME_MISS: return "WHIFF";
        case CC_COMBAT_OUTCOME_HIT: return "HIT";
        case CC_COMBAT_OUTCOME_BLOCKED: return "BLOCKED";
        case CC_COMBAT_OUTCOME_GUARD_BROKEN: return "GUARD BROKEN";
        case CC_COMBAT_OUTCOME_DEFEATED: return "DOWNED";
        case CC_COMBAT_OUTCOME_NONE:
        default: return "";
    }
}

const char *CcLocalCombatTeamName(CcCombatTeam team)
{
    switch (team) {
        case CC_COMBAT_PLAYER: return "YOU";
        case CC_COMBAT_GUARD: return "GUARD";
        case CC_COMBAT_RAIDER: return "RAIDER";
        case CC_COMBAT_NEUTRAL:
        default: return "";
    }
}

static bool CombatSkillValid(CcCombatSkill skill)
{
    return skill >= 0 && skill < CC_COMBAT_SKILL_COUNT;
}

const char *CcLocalCombatSkillName(CcCombatSkill skill)
{
    switch (skill) {
        case CC_COMBAT_SKILL_CRUSHING_BLOW: return "CRUSHING BLOW";
        case CC_COMBAT_SKILL_SUNDER: return "SUNDER";
        case CC_COMBAT_SKILL_SECOND_WIND: return "CATCH BREATH";
        case CC_COMBAT_SKILL_COUNT:
        default: return "SKILL";
    }
}

float CcLocalCombatSkillDuration(CcCombatSkill skill)
{
    switch (skill) {
        case CC_COMBAT_SKILL_CRUSHING_BLOW: return 6.0f;
        case CC_COMBAT_SKILL_SUNDER: return 8.0f;
        case CC_COMBAT_SKILL_SECOND_WIND: return 14.0f;
        case CC_COMBAT_SKILL_COUNT:
        default: return 0.0f;
    }
}

float CcLocalCombatSkillCooldown(const CcLocalAgent *player,
                                 CcCombatSkill skill)
{
    if (player == NULL || !CombatSkillValid(skill)) return 0.0f;
    return fmaxf(0.0f, player->combat.skill_cooldown[skill]);
}

void CcLocalCourseInit(CcLocalCourse *course)
{
    static const int32_t starts[CC_LOCAL_COURSE_RUNNER_COUNT] = {0, 6, 12};
    static const Color colors[CC_LOCAL_COURSE_RUNNER_COUNT] = {
        {50, 151, 160, 255}, {166, 91, 132, 255}, {176, 122, 54, 255}
    };
    static const Vector2 traveller_entries[CC_LOCAL_TRAVELLER_COUNT] = {
        {18.00f, 0.72f}, {31.00f, 71.28f},
        {43.00f, 0.72f}, {64.00f, 71.28f}
    };
    static const Vector2 traveller_exits[CC_LOCAL_TRAVELLER_COUNT] = {
        {18.00f, 71.28f}, {31.00f, 0.72f},
        {43.00f, 71.28f}, {64.00f, 0.72f}
    };
    static const Color traveller_colors[CC_LOCAL_TRAVELLER_COUNT] = {
        {118, 134, 145, 255}, {150, 105, 91, 255},
        {73, 137, 121, 255}, {139, 102, 153, 255}
    };
    const int32_t waypoint_count = (int32_t)(sizeof(COURSE_WAYPOINTS) /
                                              sizeof(COURSE_WAYPOINTS[0]));
    *course = (CcLocalCourse){0};
    course->scene = CC_LOCAL_SCENE_STREET;
    course->raider_initial_resolve = 78;
    course->raider_resolve = course->raider_initial_resolve;
    (void)snprintf(course->raider_company_name,
                   sizeof(course->raider_company_name), "Road company");
    (void)snprintf(course->raider_names[0],
                   sizeof(course->raider_names[0]), "The Captain");
    (void)snprintf(course->raider_names[1],
                   sizeof(course->raider_names[1]), "The Forager");
    course->raider_roles[0] = CC_LOCAL_RAIDER_CAPTAIN;
    course->raider_roles[1] = CC_LOCAL_RAIDER_FORAGER;
    for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
        const Vector3 start = COURSE_WAYPOINTS[starts[i]];
        CcLocalCourseRunner *runner = &course->runners[i];
        CcLocalAgentInit(&runner->agent, (Vector2){start.x, start.z}, false);
        CcLocalCombatSetTeam(&runner->agent, CC_COMBAT_GUARD);
        runner->agent.crowned = false;
        CcLocalAgentSetNpcAppearance(
            &runner->agent, UINT32_C(0x47554100) + (uint32_t)i,
            CC_NPC_ROLE_GUARD, colors[i]);
        runner->marker_color = colors[i];
        runner->duty = CC_GUARD_TRAINING;
        runner->next_waypoint = (starts[i] + 1) % waypoint_count;
        runner->pause_seconds = 0.20f + (float)i * 0.12f;
        runner->attack_cooldown = 0.28f + (float)i * 0.18f;
        if (CcLocalAgentSetExactTarget(
                &runner->agent, COURSE_WAYPOINTS[runner->next_waypoint], false)) {
            runner->next_waypoint = (runner->next_waypoint + 1) % waypoint_count;
        }
    }
    for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
        CcLocalAgentInit(&course->raiders[i],
                         (Vector2){CC_LOCAL_WORLD_WIDTH - 0.80f,
                                   38.60f + (float)i * 2.80f}, false);
        CcLocalCombatSetTeam(&course->raiders[i], CC_COMBAT_RAIDER);
        course->raiders[i].crowned = false;
        CcLocalAgentSetNpcAppearance(
            &course->raiders[i], UINT32_C(0x52414900) + (uint32_t)i,
            CC_NPC_ROLE_RAIDER, (Color){126, 55, 61, 255});
        course->raider_entry[i] = course->raiders[i].position;
    }
    for (int32_t i = 0; i < CC_LOCAL_TRAVELLER_COUNT; ++i) {
        CcLocalTraveller *traveller = &course->travellers[i];
        traveller->entry = (Vector3){traveller_entries[i].x, 0.0f,
                                     traveller_entries[i].y};
        traveller->exit = (Vector3){traveller_exits[i].x, 0.0f,
                                    traveller_exits[i].y};
        CcLocalAgentInit(&traveller->agent, traveller_entries[i], false);
        traveller->agent.crowned = false;
        CcLocalAgentSetNpcAppearance(
            &traveller->agent, UINT32_C(0x54524100) + (uint32_t)i,
            i == 3 ? CC_NPC_ROLE_REFUGEE : CC_NPC_ROLE_TRAVELLER,
            traveller_colors[i]);
        traveller->active = i < 2;
        traveller->respawn_delay = 1.8f + (float)i * 1.4f;
        if (traveller->active) {
            (void)CcLocalAgentSetExactTarget(&traveller->agent,
                                             traveller->exit, false);
        }
    }
    CcLocalAgentInit(&course->situation_witness,
                     (Vector2){CC_LOCAL_NOTICE_X + 0.92f,
                               CC_LOCAL_NOTICE_Z + 0.72f}, false);
    course->situation_witness.crowned = false;
    CcLocalAgentSetNpcAppearance(
        &course->situation_witness, UINT32_C(0x57495400),
        CC_NPC_ROLE_LABORER, (Color){173, 112, 76, 255});
    course->situation_witness_active = false;
    /* Let a first-time player read the street, move, and discover the
       carriage before the simulation asks them to parse a full melee. */
    course->alarm_countdown = 24.0f;
    course->raider_attack_cooldown[0] = 0.52f;
    course->raider_attack_cooldown[1] = 0.78f;
}

const char *CcLocalRaiderRoleName(CcLocalRaiderRole role)
{
    switch (role) {
        case CC_LOCAL_RAIDER_CAPTAIN: return "CAPTAIN";
        case CC_LOCAL_RAIDER_FORAGER: return "FORAGER";
    }
    return "OUTLAW";
}

static const CcBanditGroup *CourseRaiderCompany(const CcSim *sim)
{
    if (sim == NULL) return NULL;
    if (sim->journey.active) {
        return CcSimBanditGroupOnRoute(sim, sim->journey.route_id);
    }
    const CcBanditGroup *strongest = NULL;
    for (int32_t i = 0; i < sim->bandit_count; ++i) {
        if (strongest == NULL ||
            sim->bandits[i].influence > strongest->influence) {
            strongest = &sim->bandits[i];
        }
    }
    return strongest;
}

static void CourseApplyRaiderIdentity(CcLocalCourse *course, int32_t index)
{
    if (course == NULL || index < 0 || index >= CC_LOCAL_RAIDER_COUNT) return;
    uint64_t mark = course->raider_company_id != 0U ?
        course->raider_company_id : UINT64_C(0x524149444552);
    uint32_t seed = (uint32_t)(mark ^ (mark >> 32U)) ^
                    (index == 0 ? UINT32_C(0x43415054) :
                                  UINT32_C(0x464f5247));
    Color accent = index == 0 ? (Color){168, 126, 58, 255} :
                                (Color){112, 65, 78, 255};
    CcLocalAgentSetNpcAppearance(&course->raiders[index], seed,
                                 CC_NPC_ROLE_RAIDER, accent);
    if (index == 0) {
        CcLocalAgentSetAthleticLevel(&course->raiders[index],
                                     CC_ATHLETIC_POWER, 3);
        CcLocalAgentSetAthleticLevel(&course->raiders[index],
                                     CC_ATHLETIC_GRIP, 3);
    } else {
        CcLocalAgentSetAthleticLevel(&course->raiders[index],
                                     CC_ATHLETIC_MOBILITY, 3);
    }
}

void CcLocalCourseBindRaiderCompany(CcLocalCourse *course,
                                    const CcSim *sim)
{
    if (course == NULL) return;
    const CcBanditGroup *bandits = CourseRaiderCompany(sim);
    if (bandits == NULL) return;
    static const char *captain_names[] = {
        "Maud Vey", "Harl Rook", "Ysabet Pike", "Osric Dunn",
        "Ren Tallow", "Tavin Grey"
    };
    static const char *forager_names[] = {
        "Kerrin", "Sable", "Noll", "Brin", "Edda", "Moss"
    };
    uint64_t serial = bandits->id & UINT64_C(0x00ffffffffffffff);
    course->raider_company_id = bandits->id;
    (void)snprintf(course->raider_company_name,
                   sizeof(course->raider_company_name), "%s", bandits->name);
    (void)snprintf(course->raider_names[0],
                   sizeof(course->raider_names[0]), "%s",
                   captain_names[serial % 6U]);
    (void)snprintf(course->raider_names[1],
                   sizeof(course->raider_names[1]), "%s",
                   forager_names[(serial / 3U + 2U) % 6U]);
    course->raider_initial_resolve = (int32_t)lroundf(CombatClamp(
        38.0f + (float)bandits->members * 0.45f +
        (float)bandits->influence * 0.34f +
        (float)bandits->supplies * 0.16f, 48.0f, 100.0f));
    course->raider_resolve = course->raider_initial_resolve;
    for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
        CourseApplyRaiderIdentity(course, i);
    }
}

static bool CourseCombatOriginOpen(CcLocalSceneKind scene, Vector3 origin)
{
    for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
        float z = origin.z + ((float)i - 1.0f) * COURSE_GUARD_SPACING;
        if (StaticBodyBlocked(scene, origin.x - 2.05f, z,
                              PLAYER_COLLISION_RADIUS)) return false;
    }
    for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
        float z = origin.z + ((float)i - 0.5f) * COURSE_RAIDER_SPACING;
        if (StaticBodyBlocked(scene, origin.x + 1.85f, z,
                              PLAYER_COLLISION_RADIUS)) return false;
    }
    return !StaticBodyBlocked(scene, origin.x, origin.z,
                              PLAYER_COLLISION_RADIUS);
}

static void CoursePrepareCombatant(CcLocalAgent *agent, CcCombatTeam team)
{
    agent->combat = (CcCombatState){
        .health = CC_LOCAL_COMBAT_MAX_HEALTH,
        .posture = CC_LOCAL_COMBAT_MAX_POSTURE,
        .target_index = -1,
        .queued_skill = -1,
        .active_skill = -1,
        .team = team,
        .weapon_mode = team == CC_COMBAT_PLAYER || team == CC_COMBAT_RAIDER ?
                       CC_WEAPON_HELD : CC_WEAPON_NONE,
    };
    agent->exact_target_valid = false;
    agent->target_valid = false;
    CcHumanoidGaitSetGuarded(&agent->humanoid, false);
}

void CcLocalCourseStageRoadEncounter(CcLocalCourse *course,
                                     CcLocalAgent *player,
                                     bool hostile)
{
    if (course == NULL || player == NULL) return;
    static const Vector2 guard_positions[CC_LOCAL_COURSE_RUNNER_COUNT] = {
        {44.75f, 38.35f}, {44.75f, 40.95f}, {44.75f, 42.10f}
    };
    course->road_encounter = true;
    course->scene = CC_LOCAL_SCENE_ROAD;
    CcLocalAgentSetScene(player, CC_LOCAL_SCENE_ROAD);
    course->situation_witness_active = false;
    course->situation_witness_id = 0U;
    for (int32_t i = 0; i < CC_LOCAL_TRAVELLER_COUNT; ++i) {
        course->travellers[i].active = false;
    }
    for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
        CcLocalCourseRunner *runner = &course->runners[i];
        CcLocalAgentInit(&runner->agent, guard_positions[i], false);
        CcLocalAgentSetScene(&runner->agent, CC_LOCAL_SCENE_ROAD);
        CcLocalCombatSetTeam(&runner->agent, CC_COMBAT_GUARD);
        runner->agent.crowned = false;
        CcLocalAgentSetNpcAppearance(
            &runner->agent, UINT32_C(0x47554100) + (uint32_t)i,
            CC_NPC_ROLE_GUARD, runner->marker_color);
        runner->duty = hostile ? CC_GUARD_RESPONDING : CC_GUARD_TRAINING;
        runner->response_stage = 0;
        runner->response_waypoint_active = false;
    }
    for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
        CcLocalAgent *raider = &course->raiders[i];
        Vector2 raider_position = hostile ?
            (Vector2){54.60f + (float)i * 0.35f,
                      i == 0 ? 39.35f : 40.65f} :
            (Vector2){i == 0 ? 50.45f : 52.90f,
                      i == 0 ? 39.45f : 40.65f};
        CcLocalAgentInit(raider, raider_position, false);
        CcLocalAgentSetScene(raider, CC_LOCAL_SCENE_ROAD);
        CcLocalCombatSetTeam(raider, CC_COMBAT_RAIDER);
        if (!hostile) raider->combat.weapon_mode = CC_WEAPON_NONE;
        raider->crowned = false;
        CcLocalAgentSetNpcAppearance(
            raider, UINT32_C(0x52414900) + (uint32_t)i,
            i == 0 && !hostile ? CC_NPC_ROLE_MERCHANT : CC_NPC_ROLE_RAIDER,
            (Color){126, 55, 61, 255});
        course->raider_entry[i] = raider->position;
        course->raider_response_stage[i] = 0;
        course->raider_response_waypoint_active[i] = false;
    }
    course->combat_origin = (Vector3){52.10f, 0.0f, 40.00f};
    course->combat_origin_valid = true;
    if (hostile) CcLocalCourseRaiseAlarmNear(course, player);
}

void CcLocalCourseRaiseAlarmNear(CcLocalCourse *course,
                                 const CcLocalAgent *player)
{
    if (course->alarm_active) return;
    Vector3 origin = course->road_encounter ?
        (Vector3){47.20f, 0.0f, 40.00f} : player != NULL ?
        (Vector3){player->position.x + 4.65f, 0.0f,
                  player->position.z + 0.60f} :
        (Vector3){CC_LOCAL_START_X + 4.65f, 0.0f,
                  CC_LOCAL_START_Z + 0.60f};
    if (!CourseCombatOriginOpen(course->scene, origin)) {
        origin = (Vector3){CC_LOCAL_START_X + 4.65f, 0.0f,
                           CC_LOCAL_START_Z + 0.60f};
    }
    if (!CourseCombatOriginOpen(course->scene, origin)) {
        origin = course->road_encounter ?
            (Vector3){47.20f, 0.0f, 40.20f} :
            (Vector3){44.80f, 0.0f, 40.20f};
    }
    course->combat_origin = origin;
    course->combat_origin_valid = true;
    course->alarm_active = true;
    course->raiders_retreating = false;
    course->engagement_time = 0.0f;
    course->raider_resolve = course->raider_initial_resolve > 0 ?
        course->raider_initial_resolve : 78;
    course->last_outcome = CC_COMBAT_OUTCOME_NONE;
    course->last_attacker_team = CC_COMBAT_NEUTRAL;
    course->last_defender_team = CC_COMBAT_NEUTRAL;
    course->last_health_damage = 0.0f;
    course->last_posture_damage = 0.0f;
    course->combat_event_seconds = 0.0f;
    for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
        CcLocalCourseRunner *runner = &course->runners[i];
        course->guard_entry[i] = runner->agent.position;
        CoursePrepareCombatant(&runner->agent, CC_COMBAT_GUARD);
        runner->agent.crowned = false;
        CcLocalAgentSetNpcAppearance(
            &runner->agent, UINT32_C(0x47554100) + (uint32_t)i,
            CC_NPC_ROLE_GUARD, runner->marker_color);
        runner->duty = CC_GUARD_RESPONDING;
        runner->response_stage = 0;
        runner->response_waypoint_active = false;
        runner->attack_cooldown = 0.22f + (float)i * 0.20f;
    }
    for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
        CcLocalAgent *raider = &course->raiders[i];
        course->raider_entry[i] = raider->position;
        CoursePrepareCombatant(raider, CC_COMBAT_RAIDER);
        raider->crowned = false;
        CourseApplyRaiderIdentity(course, i);
        course->raider_response_stage[i] = 0;
        course->raider_response_waypoint_active[i] = false;
        course->raider_attack_cooldown[i] = i == 0 ? 0.74f : 0.38f;
    }
}

void CcLocalCourseRaiseAlarm(CcLocalCourse *course)
{
    CcLocalCourseRaiseAlarmNear(course, NULL);
}

static bool CourseAgentBusy(const CcLocalAgent *agent)
{
    return agent->climbing || agent->humanoid.ragdoll.active ||
           agent->humanoid.recovering;
}

static bool CourseAgentCanSeparate(const CcLocalAgent *agent)
{
    return agent != NULL && CombatCanAct(&agent->combat) &&
           !agent->climbing && !agent->swimming &&
           !agent->humanoid.ragdoll.active && !agent->humanoid.recovering;
}

static void CourseAddSeparationPair(CcLocalAgent *first,
                                    CcLocalAgent *second,
                                    int32_t pair_index)
{
    if (!CourseAgentCanSeparate(first) ||
        !CourseAgentCanSeparate(second)) return;
    bool hostile = CombatTeamsHostile(first->combat.team,
                                      second->combat.team);
    bool bystander = first->combat.team == CC_COMBAT_NEUTRAL ||
                     second->combat.team == CC_COMBAT_NEUTRAL;
    float minimum = hostile ? COMBAT_PERSONAL_SPACE :
                    bystander ? COMBAT_BYSTANDER_SPACE : COMBAT_ALLY_SPACE;
    CcLimbVec3 first_correction = {0};
    CcLimbVec3 second_correction = {0};
    if (!CcRobotPredictiveAvoidance(
            ToLimbVector(first->position), ToLimbVector(first->velocity),
            ToLimbVector(second->position), ToLimbVector(second->velocity),
            minimum, 0.85f, pair_index,
            &first_correction, &second_correction)) {
        return;
    }
    first->separation_velocity.x += first_correction.x;
    first->separation_velocity.z += first_correction.z;
    second->separation_velocity.x += second_correction.x;
    second->separation_velocity.z += second_correction.z;
}

static void CourseLimitSeparation(CcLocalAgent *agent)
{
    float x = agent->separation_velocity.x;
    float z = agent->separation_velocity.z;
    float speed = sqrtf(x * x + z * z);
    const float maximum = 0.68f;
    if (speed > maximum) {
        agent->separation_velocity.x = x * maximum / speed;
        agent->separation_velocity.z = z * maximum / speed;
    }
}

static void CourseConfigureSituationWitness(CcLocalCourse *course,
                                            const CcSim *sim)
{
    if (course == NULL || sim == NULL) return;
    if (course->road_encounter) {
        course->situation_witness_active = false;
        course->situation_witness_id = 0U;
        return;
    }
    const CcSituation *situation = CcSimAcceptedSituation(sim);
    if (situation == NULL ||
        !CcSimSituationTouchesSettlement(sim, situation,
                                         sim->player.location_id)) {
        situation = CcSimSituationForSettlement(sim,
                                                sim->player.location_id);
    }
    if (situation == NULL) {
        for (int32_t offset = 0; offset < sim->event_count; ++offset) {
            const CcEvent *event = CcSimRecentEvent(sim, offset);
            if (event == NULL || event->kind != CC_EVENT_DELAYED_ECHO ||
                event->location_id != sim->player.location_id) continue;
            situation = CcSimSituation(sim, event->subject_id);
            if (situation != NULL) break;
        }
    }
    if (situation == NULL) {
        course->situation_witness_active = false;
        course->situation_witness_id = 0U;
        course->situation_witness.separation_velocity = (Vector3){0};
        return;
    }
    if (course->situation_witness_id != situation->id) {
        CcLocalAgentInit(&course->situation_witness,
                         (Vector2){CC_LOCAL_NOTICE_X + 0.92f,
                                   CC_LOCAL_NOTICE_Z + 0.72f}, false);
        course->situation_witness.crowned = false;
        Color witness_accent =
            situation->status == CC_SITUATION_RESOLVED ?
                (Color){74, 145, 126, 255} :
            situation->kind == CC_SITUATION_BLACK_MARKET_DELIVERY ?
                (Color){91, 68, 111, 255} :
            situation->kind == CC_SITUATION_MONSTER_EXPEDITION ?
                (Color){128, 82, 66, 255} :
                (Color){173, 112, 76, 255};
        CcNpcRole witness_role =
            situation->kind == CC_SITUATION_MONSTER_EXPEDITION ?
                CC_NPC_ROLE_SCOUT :
            situation->kind == CC_SITUATION_BLACK_MARKET_DELIVERY ?
                CC_NPC_ROLE_TRAVELLER : CC_NPC_ROLE_LABORER;
        CcLocalAgentSetNpcAppearance(
            &course->situation_witness,
            (uint32_t)(situation->id ^ (situation->id >> 32)),
            witness_role, witness_accent);
        course->situation_witness_id = situation->id;
    }
    course->situation_witness_active = true;
}

static void CoursePlanActorSeparation(CcLocalCourse *course,
                                      CcLocalAgent *player)
{
    CcLocalAgent *actors[1 + CC_LOCAL_COURSE_RUNNER_COUNT +
                         CC_LOCAL_RAIDER_COUNT + CC_LOCAL_TRAVELLER_COUNT + 1];
    int32_t count = 0;
    if (player != NULL) actors[count++] = player;
    for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
        actors[count++] = &course->runners[i].agent;
    }
    for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
        actors[count++] = &course->raiders[i];
    }
    for (int32_t i = 0; i < CC_LOCAL_TRAVELLER_COUNT; ++i) {
        if (course->travellers[i].active) {
            actors[count++] = &course->travellers[i].agent;
        }
    }
    if (course->situation_witness_active) {
        actors[count++] = &course->situation_witness;
    }
    for (int32_t i = 0; i < count; ++i) {
        actors[i]->separation_velocity = (Vector3){0};
    }
    int32_t pair_index = 0;
    for (int32_t first = 0; first < count; ++first) {
        for (int32_t second = first + 1; second < count; ++second) {
            CourseAddSeparationPair(actors[first], actors[second],
                                    pair_index++);
        }
    }
    for (int32_t i = 0; i < count; ++i) CourseLimitSeparation(actors[i]);
}

static Vector3 CourseThreatCenter(const CcLocalCourse *course)
{
    Vector3 center = {0};
    for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
        center.x += course->raiders[i].position.x;
        center.y += course->raiders[i].position.y;
        center.z += course->raiders[i].position.z;
    }
    center.x /= (float)CC_LOCAL_RAIDER_COUNT;
    center.y /= (float)CC_LOCAL_RAIDER_COUNT;
    center.z /= (float)CC_LOCAL_RAIDER_COUNT;
    return center;
}

static float CourseAlarmInterval(const CcSim *sim)
{
    int32_t influence = 0;
    if (sim != NULL) {
        for (int32_t i = 0; i < sim->bandit_count; ++i) {
            if (sim->bandits[i].influence > influence) {
                influence = sim->bandits[i].influence;
            }
        }
    }
    return fmaxf(9.0f, 22.0f - (float)influence * 0.16f);
}

static bool SetStreetClickTarget(CcLocalAgent *agent, Vector3 target);

static bool CourseRaiderResponseWaypoint(const CcLocalCourse *course,
                                         int32_t raider, int32_t stage,
                                         Vector3 *waypoint)
{
    static const Vector3 routes[CC_LOCAL_RAIDER_COUNT][3] = {
        {{72.00f, 0.0f, 38.60f}, {64.20f, 0.0f, 38.60f},
         {53.20f, 0.0f, 38.60f}},
        {{72.00f, 0.0f, 40.40f}, {64.20f, 0.0f, 40.40f},
         {53.20f, 0.0f, 40.40f}},
    };
    if (raider < 0 || raider >= CC_LOCAL_RAIDER_COUNT || stage < 0) {
        return false;
    }
    if (course != NULL && course->road_encounter) {
        static const Vector3 road_routes[CC_LOCAL_RAIDER_COUNT][4] = {
            {{55.10f, 0.0f, 39.35f}, {53.25f, 0.0f, 39.35f},
             {51.25f, 0.0f, 39.35f}, {48.80f, 0.0f, 39.35f}},
            {{55.45f, 0.0f, 40.65f}, {53.55f, 0.0f, 40.65f},
             {52.20f, 0.0f, 39.70f}, {49.50f, 0.0f, 39.70f}},
        };
        if (stage >= 4) return false;
        *waypoint = road_routes[raider][stage];
        return true;
    }
    if (stage >= 3) return false;
    *waypoint = routes[raider][stage];
    return true;
}

static bool CourseGuardIngressWaypoint(const CcLocalCourse *course,
                                       int32_t guard, int32_t stage,
                                       Vector3 *waypoint)
{
    if (course == NULL || waypoint == NULL || guard < 0 ||
        guard >= CC_LOCAL_COURSE_RUNNER_COUNT || stage < 0 || stage >= 4) {
        return false;
    }
    Vector3 entry = course->guard_entry[guard];
    if (course->road_encounter) {
        static const float road_guard_lanes[CC_LOCAL_COURSE_RUNNER_COUNT] = {
            38.35f, 40.95f, 42.10f
        };
        float lane_z = road_guard_lanes[guard];
        if (stage == 0) {
            *waypoint = (Vector3){44.75f, 0.0f, lane_z};
        } else if (stage == 1) {
            /* Leave the player a clean center lane instead of routing the
               middle guard through the hero's exact starting point. */
            *waypoint = (Vector3){45.35f, 0.0f, lane_z};
        } else if (stage == 2) {
            *waypoint = (Vector3){47.70f, 0.0f, lane_z};
        } else {
            *waypoint = (Vector3){course->combat_origin.x - 1.45f, 0.0f,
                                  course->combat_origin.z +
                                  ((float)guard - 1.0f) * 0.72f};
        }
        return true;
    }
    float exit_x = entry.x < 11.80f ? 8.35f : 15.90f;
    float lane_z = 26.50f + (float)guard * 0.35f;
    if (stage == 0) {
        *waypoint = (Vector3){exit_x, 0.0f, entry.z};
    } else if (stage == 1) {
        *waypoint = (Vector3){exit_x, 0.0f, 12.80f};
    } else if (stage == 2) {
        *waypoint = (Vector3){16.20f + (float)guard * 0.50f, 0.0f,
                              lane_z};
    } else {
        *waypoint = (Vector3){42.80f, 0.0f, lane_z};
    }
    return true;
}

static bool CourseAgentReachedWaypoint(const CcLocalAgent *agent,
                                       Vector3 waypoint)
{
    if (agent == NULL) return false;
    float x = agent->position.x - waypoint.x;
    float z = agent->position.z - waypoint.z;
    /* Route nodes describe a corridor, not a pin-sized animation target.
       Accepting body-radius proximity prevents an otherwise successful
       response from waiting forever on collision-constrained centimetres. */
    return !agent->exact_target_valid || x * x + z * z <= 0.20f * 0.20f;
}

static void UpdateCourseTraining(CcLocalCourse *course, float delta_time)
{
    const int32_t waypoint_count = (int32_t)(sizeof(COURSE_WAYPOINTS) /
                                              sizeof(COURSE_WAYPOINTS[0]));
    for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
        CcLocalCourseRunner *runner = &course->runners[i];
        runner->duty = CC_GUARD_TRAINING;
        CcLocalAgentFixedStepInternal(&runner->agent, delta_time, false);
        if (!runner->agent.exact_target_valid &&
            !CourseAgentBusy(&runner->agent)) {
            runner->pause_seconds -= delta_time;
            if (runner->pause_seconds <= 0.0f &&
                CcLocalAgentSetExactTarget(
                    &runner->agent,
                    COURSE_WAYPOINTS[runner->next_waypoint], false)) {
                runner->next_waypoint =
                    (runner->next_waypoint + 1) % waypoint_count;
                runner->pause_seconds = 0.28f;
            }
        }
    }
}

static int32_t CourseTravellerDemand(const CcSim *sim)
{
    int32_t count = 2;
    if (sim == NULL) return count;
    for (int32_t i = 0; i < sim->shipment_count && count <
         CC_LOCAL_TRAVELLER_COUNT; ++i) {
        if (sim->shipments[i].status == CC_SHIPMENT_TRAVELLING) count += 1;
    }
    return count;
}

static void UpdateCourseTravellers(CcLocalCourse *course, const CcSim *sim,
                                   float delta_time)
{
    if (course->scene != CC_LOCAL_SCENE_STREET) return;
    int32_t demand = CourseTravellerDemand(sim);
    for (int32_t i = 0; i < CC_LOCAL_TRAVELLER_COUNT; ++i) {
        CcLocalTraveller *traveller = &course->travellers[i];
        if (!traveller->active) {
            if (i >= demand) continue;
            traveller->respawn_delay -= delta_time;
            if (traveller->respawn_delay > 0.0f) continue;
            CcLocalAgentInit(&traveller->agent,
                             (Vector2){traveller->entry.x,
                                       traveller->entry.z}, false);
            traveller->agent.scene = course->scene;
            traveller->agent.crowned = false;
            static const Color colors[CC_LOCAL_TRAVELLER_COUNT] = {
                {118, 134, 145, 255}, {150, 105, 91, 255},
                {73, 137, 121, 255}, {139, 102, 153, 255}
            };
            CcLocalAgentSetNpcAppearance(
                &traveller->agent, UINT32_C(0x54524100) + (uint32_t)i,
                i == 3 ? CC_NPC_ROLE_REFUGEE : CC_NPC_ROLE_TRAVELLER,
                colors[i]);
            traveller->active = CcLocalAgentSetExactTarget(
                &traveller->agent, traveller->exit, false);
            continue;
        }
        CcLocalAgentFixedStepInternal(&traveller->agent, delta_time, false);
        if (traveller->agent.exact_target_valid) continue;
        float exit_x = traveller->agent.position.x - traveller->exit.x;
        float exit_z = traveller->agent.position.z - traveller->exit.z;
        if (exit_x * exit_x + exit_z * exit_z > 0.20f * 0.20f) continue;
        traveller->active = false;
        traveller->respawn_delay = 5.5f + (float)i * 1.7f;
    }
}

static int32_t CourseClosestRaider(const CcLocalCourse *course,
                                   const CcLocalAgent *agent,
                                   float maximum_distance,
                                   bool require_forward,
                                   int32_t excluded_index)
{
    int32_t closest = -1;
    float closest_distance = maximum_distance * maximum_distance;
    for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
        if (i == excluded_index) continue;
        const CcLocalAgent *raider = &course->raiders[i];
        if (!CombatCanAct(&raider->combat)) continue;
        float distance = CombatHorizontalDistanceSquared(agent, raider);
        if (distance >= closest_distance) continue;
        if (require_forward && distance > 1.15f * 1.15f &&
            CombatFacingDot(agent, raider->position) < -0.05f) continue;
        closest = i;
        closest_distance = distance;
    }
    return closest;
}

static CcLocalAgent *CourseClosestDefender(CcLocalCourse *course,
                                           CcLocalAgent *player,
                                           const CcLocalAgent *raider)
{
    CcLocalAgent *closest = NULL;
    float closest_distance = 3.20f * 3.20f;
    for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
        CcLocalAgent *guard = &course->runners[i].agent;
        if (!CombatCanAct(&guard->combat) || CourseAgentBusy(guard)) continue;
        float distance = CombatHorizontalDistanceSquared(raider, guard);
        if (distance < closest_distance) {
            closest = guard;
            closest_distance = distance;
        }
    }
    if (player != NULL && CombatCanAct(&player->combat)) {
        float distance = CombatHorizontalDistanceSquared(raider, player);
        if (distance < closest_distance) closest = player;
    }
    return closest;
}

static void CourseRecordOutcome(CcLocalCourse *course,
                                const CcLocalAgent *attacker,
                                const CcLocalAgent *defender,
                                float previous_health,
                                float previous_posture,
                                CcCombatOutcome outcome)
{
    if (outcome == CC_COMBAT_OUTCOME_NONE) return;
    course->last_outcome = outcome;
    course->last_attacker_team = attacker->combat.team;
    course->last_defender_team = defender != NULL ?
        defender->combat.team : CC_COMBAT_NEUTRAL;
    course->last_health_damage = defender != NULL ?
        fmaxf(0.0f, previous_health - defender->combat.health) : 0.0f;
    course->last_posture_damage = defender != NULL ?
        fmaxf(0.0f, previous_posture - defender->combat.posture) : 0.0f;
    course->combat_event_seconds = 0.72f;
}

static void CourseResolveImpact(CcLocalCourse *course,
                                CcLocalAgent *attacker,
                                CcLocalAgent *defender)
{
    if (!CcHumanoidGaitConsumeStrikeImpact(&attacker->humanoid)) return;
    float previous_health = defender != NULL ? defender->combat.health : 0.0f;
    float previous_posture = defender != NULL ?
        defender->combat.posture : 0.0f;
    CourseRecordOutcome(course, attacker, defender,
                        previous_health, previous_posture,
                        CcLocalCombatResolveStrike(attacker, defender));
}

static void CourseRefreshRaiderResolve(CcLocalCourse *course,
                                       int32_t engaged_guards)
{
    float lost_health = 0.0f;
    int32_t defeated = 0;
    for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
        lost_health += CC_LOCAL_COMBAT_MAX_HEALTH -
                       course->raiders[i].combat.health;
        if (CombatIsDefeated(&course->raiders[i].combat)) defeated += 1;
    }
    float pressure = lost_health * 0.28f + (float)defeated * 12.0f;
    if (CombatIsDefeated(&course->raiders[0].combat)) pressure += 28.0f;
    if (engaged_guards >= 2) pressure += 7.0f;
    if (course->engagement_time > 8.0f) {
        pressure += fminf(12.0f, course->engagement_time - 8.0f);
    }
    course->raider_resolve = (int32_t)lroundf(CombatClamp(
        (float)course->raider_initial_resolve - pressure,
        0.0f, CC_LOCAL_COMBAT_MAX_HEALTH));
}

static bool CourseRaidersBroken(const CcLocalCourse *course)
{
    float total_health = 0.0f;
    int32_t standing = 0;
    for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
        total_health += course->raiders[i].combat.health;
        standing += CombatIsDefeated(&course->raiders[i].combat) ? 0 : 1;
    }
    return standing == 0 || total_health <= 60.0f ||
           course->raider_resolve <= 35;
}

static void CourseBeginRetreat(CcLocalCourse *course, CcLocalAgent *player)
{
    course->raiders_retreating = true;
    course->raider_resolve = 0;
    for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
        CcLocalCourseRunner *runner = &course->runners[i];
        runner->duty = CC_GUARD_RETURNING;
        CcLocalCombatSetGuarded(&runner->agent, NULL, false);
    }
    for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
        CcLocalAgent *raider = &course->raiders[i];
        if (CombatIsDefeated(&raider->combat)) {
            course->raider_response_waypoint_active[i] = false;
            continue;
        }
        raider->combat.stagger_seconds = 0.0f;
        raider->combat.hitstop_seconds = 0.0f;
        CcLocalCombatSetGuarded(raider, NULL, false);
        CcLocalCombatClearFocus(raider);
        course->raider_response_stage[i] = 2;
        course->raider_response_waypoint_active[i] = false;
    }
    if (player != NULL) CcLocalCourseClearPlayerTarget(player);
}

static CcLocalAgent *CoursePlayerTarget(CcLocalCourse *course,
                                        const CcLocalAgent *player)
{
    if (course == NULL || player == NULL ||
        player->combat.target_index < 0 ||
        player->combat.target_index >= CC_LOCAL_RAIDER_COUNT) {
        return NULL;
    }
    CcLocalAgent *target = &course->raiders[player->combat.target_index];
    return CombatCanAct(&target->combat) ? target : NULL;
}

static Vector3 CourseCombatApproachPoint(const CcLocalAgent *mover,
                                         const CcLocalAgent *target,
                                         float standoff,
                                         float orbit_angle)
{
    float x = mover->position.x - target->position.x;
    float z = mover->position.z - target->position.z;
    float distance = sqrtf(x * x + z * z);
    if (distance <= 0.0001f) {
        x = -sinf(mover->facing_yaw);
        z = -cosf(mover->facing_yaw);
        distance = 1.0f;
    }
    x /= distance;
    z /= distance;
    float cosine = cosf(orbit_angle);
    float sine = sinf(orbit_angle);
    float orbit_x = x * cosine - z * sine;
    float orbit_z = x * sine + z * cosine;
    return (Vector3){target->position.x + orbit_x * standoff,
                     target->position.y,
                     target->position.z + orbit_z * standoff};
}

void CcLocalCourseClearPlayerTarget(CcLocalAgent *player)
{
    if (player == NULL) return;
    player->combat.queued_skill = -1;
    CcLocalCombatSetGuarded(player, NULL, false);
    /* Course-level disengagement is authoritative even while a strike is
       finishing.  The generic guard helper deliberately retains focus during
       a live strike so its hit can still resolve against the selected target. */
    CcLocalCombatClearFocus(player);
}

bool CcLocalCourseSelectPlayerTarget(CcLocalCourse *course,
                                     CcLocalAgent *player,
                                     int32_t target_index)
{
    if (course == NULL || player == NULL || !course->alarm_active ||
        course->raiders_retreating || target_index < 0 ||
        target_index >= CC_LOCAL_RAIDER_COUNT ||
        !CombatCanAct(&course->raiders[target_index].combat)) {
        return false;
    }
    CcLocalCombatSetTeam(player, CC_COMBAT_PLAYER);
    player->combat.target_index = target_index;
    player->combat.queued_skill = -1;
    player->exact_target_valid = false;
    player->target_valid = false;
    CcLocalCombatSetFocus(player, &course->raiders[target_index]);
    return true;
}

int32_t CcLocalCoursePickPlayerTarget(CcLocalCourse *course,
                                      CcLocalAgent *player,
                                      Vector2 screen_point,
                                      RenderTexture2D target,
                                      Rectangle destination)
{
    if (course == NULL || player == NULL || !course->alarm_active ||
        course->raiders_retreating ||
        !CheckCollisionPointRec(screen_point, destination)) {
        return -1;
    }
    Vector2 local = {
        (screen_point.x - destination.x) / destination.width *
            (float)target.texture.width,
        (screen_point.y - destination.y) / destination.height *
            (float)target.texture.height
    };
    Camera3D camera = {0};
    if (!PresentedCameraForInput(course->scene, player,
                                 target.texture.width,
                                 target.texture.height, &camera)) {
        Camera3D base_camera = course->scene == CC_LOCAL_SCENE_ROAD ?
            RoadCamera(player->position, false, 0.0f, false,
                       target.texture.height) :
            CcLocalStreetCameraInternal(player, 0.0f, false,
                                        target.texture.height);
        camera = CcLocalCombatCameraInternal(
            base_camera, player, course, 0.0f, false,
            target.texture.height);
    }
    Ray ray = GetScreenToWorldRayEx(local, camera, target.texture.width,
                                    target.texture.height);
    int32_t picked = -1;
    float nearest = FLT_MAX;
    for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
        const CcLocalAgent *raider = &course->raiders[i];
        if (!CombatCanAct(&raider->combat)) continue;
        BoundingBox body = {
            .min = {raider->position.x - 0.48f, raider->position.y,
                    raider->position.z - 0.48f},
            .max = {raider->position.x + 0.48f,
                    raider->position.y + 2.15f,
                    raider->position.z + 0.48f}
        };
        RayCollision collision = GetRayCollisionBox(ray, body);
        if (!collision.hit || collision.distance >= nearest) continue;
        picked = i;
        nearest = collision.distance;
    }
    /* The fixed room shots make a distant raider narrower than a comfortable
       mouse target. Keep the physical ray test authoritative, then add a
       small screen-space halo around the visible body. This is only a click
       aid; it never changes combat range or movement. */
    if (picked < 0) {
        float best_score = FLT_MAX;
        for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
            const CcLocalAgent *raider = &course->raiders[i];
            if (!CombatCanAct(&raider->combat)) continue;
            Vector3 center = Vector3Add(
                raider->position, (Vector3){0.0f, 1.10f, 0.0f});
            if (!CameraPointInFront(camera, center)) continue;
            Vector2 center_screen = GetWorldToScreenEx(
                center, camera, target.texture.width, target.texture.height);
            Vector2 head_screen = GetWorldToScreenEx(
                Vector3Add(raider->position,
                           (Vector3){0.0f, 2.12f, 0.0f}),
                camera, target.texture.width, target.texture.height);
            Vector2 feet_screen = GetWorldToScreenEx(
                Vector3Add(raider->position,
                           (Vector3){0.0f, 0.08f, 0.0f}),
                camera, target.texture.width, target.texture.height);
            if (center_screen.x < 0.0f ||
                center_screen.x > (float)target.texture.width ||
                center_screen.y < 0.0f ||
                center_screen.y > (float)target.texture.height) continue;
            float projected_height = fabsf(feet_screen.y - head_screen.y);
            float radius_x = CombatClamp(projected_height * 0.34f,
                                         18.0f, 24.0f);
            float radius_y = CombatClamp(projected_height * 0.58f,
                                         18.0f, 30.0f);
            float dx = (local.x - center_screen.x) / radius_x;
            float dy = (local.y - center_screen.y) / radius_y;
            float score = dx * dx + dy * dy;
            if (score > 1.0f || score >= best_score) continue;
            best_score = score;
            picked = i;
        }
    }
    return CcLocalCourseSelectPlayerTarget(course, player, picked) ?
           picked : -1;
}

bool CcLocalCourseUsePlayerSkill(CcLocalCourse *course,
                                 CcLocalAgent *player,
                                 CcCombatSkill skill)
{
    if (course == NULL || player == NULL || !CombatSkillValid(skill) ||
        !CombatCanAct(&player->combat) ||
        player->combat.skill_cooldown[skill] > 0.0f) {
        return false;
    }
    if (skill == CC_COMBAT_SKILL_SECOND_WIND) {
        if (player->combat.posture >= CC_LOCAL_COMBAT_MAX_POSTURE) {
            return false;
        }
        player->combat.posture = fminf(CC_LOCAL_COMBAT_MAX_POSTURE,
                                       player->combat.posture + 65.0f);
        player->combat.skill_cooldown[skill] =
            CcLocalCombatSkillDuration(skill);
        return true;
    }
    if (!course->alarm_active || course->raiders_retreating ||
        CoursePlayerTarget(course, player) == NULL) {
        return false;
    }
    player->combat.queued_skill = (int32_t)skill;
    return true;
}

static bool CourseBeginPlayerAttack(CcLocalAgent *player,
                                    CcLocalAgent *target,
                                    CcCombatSkill skill)
{
    if (!CcLocalCombatBeginStrike(player, target)) return false;
    player->combat.active_skill = CombatSkillValid(skill) ?
                                  (int32_t)skill : -1;
    CcHumanoidGaitSetStrikeStyle(
        &player->humanoid,
        skill == CC_COMBAT_SKILL_CRUSHING_BLOW ? CC_HUMANOID_STRIKE_HEAVY :
        skill == CC_COMBAT_SKILL_SUNDER ? CC_HUMANOID_STRIKE_SWEEP :
                                         CC_HUMANOID_STRIKE_CUT);
    player->combat.strike_damage_scale = 1.0f;
    player->combat.strike_posture_scale = 1.0f;
    player->combat.strike_knockback_scale = 1.0f;
    if (skill == CC_COMBAT_SKILL_CRUSHING_BLOW) {
        player->combat.strike_damage_scale = 1.75f;
        player->combat.strike_posture_scale = 1.15f;
        player->combat.strike_knockback_scale = 1.45f;
    } else if (skill == CC_COMBAT_SKILL_SUNDER) {
        player->combat.strike_damage_scale = 0.80f;
        player->combat.strike_posture_scale = 2.65f;
        player->combat.strike_knockback_scale = 0.82f;
    }
    return true;
}

static void CourseUpdatePlayerCombat(CcLocalCourse *course,
                                     CcLocalAgent *player,
                                     float delta_time)
{
    if (player == NULL) return;
    player->combat.auto_attack_cooldown = fmaxf(
        0.0f, player->combat.auto_attack_cooldown - delta_time);
    for (int32_t skill = 0; skill < CC_COMBAT_SKILL_COUNT; ++skill) {
        player->combat.skill_cooldown[skill] = fmaxf(
            0.0f, player->combat.skill_cooldown[skill] - delta_time);
    }
    if (!course->alarm_active) {
        CcLocalCourseClearPlayerTarget(player);
        return;
    }
    CcLocalAgent *target = CoursePlayerTarget(course, player);
    if (target == NULL || !CombatCanAct(&player->combat) ||
        course->raiders_retreating) {
        CcLocalCourseClearPlayerTarget(player);
        return;
    }

    CcLocalCombatSetFocus(player, target);
    float x = player->position.x - target->position.x;
    float z = player->position.z - target->position.z;
    float distance = sqrtf(x * x + z * z);
    bool can_reposition = !CourseAgentBusy(player) &&
        player->humanoid.action != CC_HUMANOID_ACTION_STRIKE;
    if ((distance > COMBAT_PLAYER_STANDOFF + 0.10f ||
         distance < COMBAT_MIN_STRIKE_DISTANCE) &&
        can_reposition) {
        /* Back out of body overlap before attacking, just as deliberately as
           closing excess distance.  Repositioning must not pass through the
           guard helper: lowering guard is allowed to clear combat focus. */
        Vector3 approach = CourseCombatApproachPoint(
            player, target, COMBAT_PLAYER_STANDOFF, 0.0f);
        (void)CcLocalAgentSetExactTarget(player, approach, false);
        return;
    }
    if (distance <= COMBAT_PLAYER_STANDOFF + 0.10f) {
        player->exact_target_valid = false;
        player->target_valid = false;
    }
    if (distance < COMBAT_MIN_STRIKE_DISTANCE || distance > 1.56f ||
        player->combat.auto_attack_cooldown > 0.0f) {
        return;
    }

    CcCombatSkill queued = (CcCombatSkill)player->combat.queued_skill;
    if (CombatSkillValid(queued)) {
        if (player->combat.skill_cooldown[queued] > 0.0f) {
            player->combat.queued_skill = -1;
            return;
        }
        if (CourseBeginPlayerAttack(player, target, queued)) {
            player->combat.skill_cooldown[queued] =
                CcLocalCombatSkillDuration(queued);
            player->combat.auto_attack_cooldown = 1.12f;
            player->combat.queued_skill = -1;
        }
        return;
    }
    if (CourseBeginPlayerAttack(player, target, CC_COMBAT_SKILL_COUNT)) {
        player->combat.auto_attack_cooldown = 0.92f;
    }
}

bool CcLocalCourseBeginPlayerStrike(CcLocalCourse *course,
                                    CcLocalAgent *player)
{
    if (course == NULL || player == NULL) return false;
    CcLocalCombatSetTeam(player, CC_COMBAT_PLAYER);
    int32_t target = CoursePlayerTarget(course, player) != NULL ?
        player->combat.target_index :
        course->alarm_active && !course->raiders_retreating ?
        CourseClosestRaider(course, player, 2.75f, true, -1) : -1;
    player->combat.target_index = target;
    if (target < 0) CcLocalCombatClearFocus(player);
    player->combat.queued_skill = -1;
    return CourseBeginPlayerAttack(
        player, target >= 0 ? &course->raiders[target] : NULL,
        CC_COMBAT_SKILL_COUNT);
}

void CcLocalCourseSetPlayerGuarded(CcLocalCourse *course,
                                   CcLocalAgent *player, bool guarded)
{
    if (course == NULL || player == NULL) return;
    CcLocalCombatSetTeam(player, CC_COMBAT_PLAYER);
    int32_t target = CoursePlayerTarget(course, player) != NULL ?
        player->combat.target_index :
        guarded && course->alarm_active && !course->raiders_retreating ?
        CourseClosestRaider(course, player, 3.10f, false, -1) : -1;
    player->combat.target_index = target;
    CcLocalCombatSetGuarded(
        player, target >= 0 ? &course->raiders[target] : NULL, guarded);
    if (!guarded && target >= 0) {
        player->combat.target_index = target;
        CcLocalCombatSetFocus(player, &course->raiders[target]);
    }
}

void CcLocalCourseFixedStepInternal(CcLocalCourse *course,
                                    CcLocalAgent *player,
                                    const CcSim *sim, float delta_time)
{
    UpdateCourseTravellers(course, sim, delta_time);
    if (course->scene == CC_LOCAL_SCENE_STREET) {
        CourseConfigureSituationWitness(course, sim);
    } else {
        course->situation_witness_active = false;
    }
    CoursePlanActorSeparation(course, player);
    if (course->situation_witness_active) {
        CcLocalAgentFixedStepInternal(&course->situation_witness,
                                      delta_time, false);
    }
    course->combat_event_seconds = fmaxf(
        0.0f, course->combat_event_seconds - delta_time);
    if (player != NULL && player->combat.team == CC_COMBAT_NEUTRAL) {
        CcLocalCombatSetTeam(player, CC_COMBAT_PLAYER);
    }
    CourseUpdatePlayerCombat(course, player, delta_time);
    if (!course->alarm_active) {
        if (player != NULL) CourseResolveImpact(course, player, NULL);
        course->alarm_countdown -= delta_time;
        if (course->alarm_countdown <= 0.0f) {
            if (course->raider_company_id == 0U) {
                CcLocalCourseBindRaiderCompany(course, sim);
            }
            CcLocalCourseRaiseAlarmNear(course, player);
        } else {
            UpdateCourseTraining(course, delta_time);
            return;
        }
    }
    if (player != NULL && player->combat.target_index >= 0 &&
        player->combat.target_index < CC_LOCAL_RAIDER_COUNT &&
        CombatCanAct(&course->raiders[player->combat.target_index].combat)) {
        CcLocalCombatSetFocus(
            player, &course->raiders[player->combat.target_index]);
    } else if (player != NULL &&
               player->humanoid.action != CC_HUMANOID_ACTION_STRIKE &&
               player->humanoid.action != CC_HUMANOID_ACTION_GUARD) {
        CcLocalCombatClearFocus(player);
    }

    if (course->raiders_retreating) {
        for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
            CcLocalCourseRunner *runner = &course->runners[i];
            runner->duty = CC_GUARD_RETURNING;
            CcLocalCombatSetGuarded(&runner->agent, NULL, false);
            (void)CcLocalAgentSetExactTarget(
                &runner->agent,
                (Vector3){course->combat_origin.x - 1.30f, 0.0f,
                          course->combat_origin.z +
                          ((float)i - 1.0f) * COURSE_GUARD_SPACING}, false);
            CcLocalAgentFixedStepInternal(&runner->agent, delta_time, false);
        }
        bool raiders_clear = true;
        for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
            CcLocalAgent *raider = &course->raiders[i];
            if (CombatIsDefeated(&raider->combat)) {
                CcLocalAgentFixedStepInternal(raider, delta_time, false);
                continue;
            }
            Vector3 retreat_waypoint;
            bool has_retreat_waypoint = CourseRaiderResponseWaypoint(
                course, i, course->raider_response_stage[i],
                &retreat_waypoint);
            if (has_retreat_waypoint &&
                !course->raider_response_waypoint_active[i]) {
                if (SetStreetClickTarget(raider, retreat_waypoint)) {
                    course->raider_response_waypoint_active[i] = true;
                }
            } else if (has_retreat_waypoint &&
                       course->raider_response_waypoint_active[i] &&
                       CourseAgentReachedWaypoint(raider,
                                                  retreat_waypoint)) {
                raider->exact_target_valid = false;
                raider->target_valid = false;
                course->raider_response_stage[i] -= 1;
                course->raider_response_waypoint_active[i] = false;
            } else if (!has_retreat_waypoint) {
                (void)CcLocalAgentSetExactTarget(
                    raider, course->raider_entry[i], false);
            }
            CcLocalAgentFixedStepInternal(raider, delta_time, false);
            float entry_x = raider->position.x - course->raider_entry[i].x;
            float entry_z = raider->position.z - course->raider_entry[i].z;
            if (entry_x * entry_x + entry_z * entry_z > 0.18f * 0.18f) {
                raiders_clear = false;
            }
        }
        if (raiders_clear) {
            course->alarm_active = false;
            course->raiders_retreating = false;
            course->defenses_completed += 1;
            course->alarm_countdown = CourseAlarmInterval(sim);
            for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
                course->runners[i].duty = CC_GUARD_TRAINING;
                course->runners[i].pause_seconds = 0.18f + (float)i * 0.08f;
                course->runners[i].agent.exact_target_valid = false;
            }
        }
        return;
    }

    int32_t engaged_guards = 0;
    for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
        CcLocalCourseRunner *runner = &course->runners[i];
        CcLocalAgent *guard = &runner->agent;
        int32_t reserved_target = -1;
        if (player != NULL && CoursePlayerTarget(course, player) != NULL) {
            int32_t selected = player->combat.target_index;
            for (int32_t raider = 0; raider < CC_LOCAL_RAIDER_COUNT; ++raider) {
                if (raider != selected &&
                    CombatCanAct(&course->raiders[raider].combat)) {
                    reserved_target = selected;
                    break;
                }
            }
        }
        int32_t target_index = CourseClosestRaider(
            course, guard, 3.20f, false, reserved_target);
        CcLocalAgent *target = target_index >= 0 ?
                               &course->raiders[target_index] : NULL;
        bool engaged = target != NULL &&
            CombatHorizontalDistanceSquared(guard, target) < 3.20f * 3.20f;
        float target_distance_squared = target != NULL ?
            CombatHorizontalDistanceSquared(guard, target) : FLT_MAX;
        bool in_attack_range = target != NULL &&
            target_distance_squared < 1.62f * 1.62f &&
            target_distance_squared >= COMBAT_MIN_STRIKE_DISTANCE *
                                       COMBAT_MIN_STRIKE_DISTANCE;

        if (CombatCanAct(&guard->combat) && !CourseAgentBusy(guard)) {
            Vector3 response_waypoint;
            bool has_response_waypoint = CourseGuardIngressWaypoint(
                course, i, runner->response_stage, &response_waypoint);
            if (has_response_waypoint &&
                !runner->response_waypoint_active) {
                if (CcLocalAgentSetExactTarget(guard, response_waypoint,
                                               false)) {
                    runner->response_waypoint_active = true;
                }
            } else if (has_response_waypoint &&
                       runner->response_waypoint_active &&
                       CourseAgentReachedWaypoint(guard,
                                                  response_waypoint)) {
                guard->exact_target_valid = false;
                guard->target_valid = false;
                runner->response_stage += 1;
                runner->response_waypoint_active = false;
            } else if (!has_response_waypoint && !in_attack_range) {
                Vector3 guard_target = target != NULL ?
                    CourseCombatApproachPoint(guard, target,
                                              COMBAT_NPC_STANDOFF,
                                              ((float)i - 1.0f) * 0.32f) :
                    (Vector3){course->combat_origin.x - 1.30f, 0.0f,
                              course->combat_origin.z +
                              ((float)i - 1.0f) * COURSE_GUARD_SPACING};
                (void)CcLocalAgentSetExactTarget(guard, guard_target,
                                                 false);
            }
        }
        if (in_attack_range && CombatCanAct(&guard->combat)) {
            runner->duty = CC_GUARD_ENGAGED;
            guard->exact_target_valid = false;
            guard->combat.target_index = target_index;
            CcLocalCombatSetFocus(guard, target);
            CcLocalCombatSetGuarded(guard, target, true);
            runner->attack_cooldown -= delta_time;
            if (runner->attack_cooldown <= 0.0f &&
                CcLocalCombatBeginStrike(guard, target)) {
                runner->attack_cooldown = 0.88f + (float)i * 0.07f;
            }
            engaged_guards += 1;
        } else if (CombatCanAct(&guard->combat)) {
            runner->duty = engaged || runner->response_stage >= 4 ?
                           CC_GUARD_ENGAGED : CC_GUARD_RESPONDING;
            CcLocalCombatSetGuarded(guard, target, false);
        }
        CcLocalAgentFixedStepInternal(guard, delta_time, false);
        target_index = guard->combat.target_index;
        target = target_index >= 0 && target_index < CC_LOCAL_RAIDER_COUNT ?
                 &course->raiders[target_index] : NULL;
        CourseResolveImpact(course, guard, target);
    }

    if (engaged_guards > 0) course->engagement_time += delta_time;
    for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
        CcLocalAgent *raider = &course->raiders[i];
        if (!CombatCanAct(&raider->combat)) {
            CcLocalAgentFixedStepInternal(raider, delta_time, false);
            continue;
        }
        CcLocalAgent *eligible_player = player != NULL &&
            (player->combat.target_index < 0 ||
             player->combat.target_index == i) ? player : NULL;
        CcLocalAgent *target = CourseClosestDefender(
            course, eligible_player, raider);
        float target_distance_squared = target != NULL ?
            CombatHorizontalDistanceSquared(raider, target) : FLT_MAX;
        bool engaged = target != NULL &&
            target_distance_squared < 1.52f * 1.52f &&
            target_distance_squared >= COMBAT_MIN_STRIKE_DISTANCE *
                                       COMBAT_MIN_STRIKE_DISTANCE;
        if (!CourseAgentBusy(raider)) {
            Vector3 response_waypoint;
            bool has_response_waypoint = CourseRaiderResponseWaypoint(
                course, i, course->raider_response_stage[i],
                &response_waypoint);
            if (has_response_waypoint &&
                !course->raider_response_waypoint_active[i]) {
                if (CcLocalAgentSetExactTarget(raider, response_waypoint,
                                               false)) {
                    course->raider_response_waypoint_active[i] = true;
                }
            } else if (has_response_waypoint &&
                       course->raider_response_waypoint_active[i] &&
                       CourseAgentReachedWaypoint(raider,
                                                  response_waypoint)) {
                raider->exact_target_valid = false;
                raider->target_valid = false;
                course->raider_response_stage[i] += 1;
                course->raider_response_waypoint_active[i] = false;
            } else if (!has_response_waypoint) {
                Vector3 target_point = engaged ? raider->position :
                    target != NULL ? CourseCombatApproachPoint(
                        raider, target, COMBAT_NPC_STANDOFF,
                        ((float)i - 0.5f) * 0.28f) :
                    (Vector3){course->combat_origin.x + 1.30f, 0.0f,
                              course->combat_origin.z +
                              ((float)i - 0.5f) * COURSE_RAIDER_SPACING};
                (void)CcLocalAgentSetExactTarget(raider, target_point, false);
            }
        }
        if (engaged) {
            raider->exact_target_valid = false;
            CcLocalCombatSetFocus(raider, target);
            CcLocalCombatSetGuarded(raider, target, true);
            course->raider_attack_cooldown[i] -= delta_time;
            if (course->raider_attack_cooldown[i] <= 0.0f &&
                CcLocalCombatBeginStrike(raider, target)) {
                course->raider_attack_cooldown[i] = 1.02f +
                                                    (float)i * 0.11f;
            }
        } else {
            CcLocalCombatSetGuarded(raider, target, false);
        }
        CcLocalAgentFixedStepInternal(raider, delta_time, false);
        CourseResolveImpact(course, raider, target);
    }

    if (player != NULL) {
        CcLocalAgent *target = NULL;
        if (player->combat.target_index >= 0 &&
            player->combat.target_index < CC_LOCAL_RAIDER_COUNT) {
            target = &course->raiders[player->combat.target_index];
            if (!CombatCanAct(&target->combat)) target = NULL;
        }
        CourseResolveImpact(course, player, target);
    }
    CourseRefreshRaiderResolve(course, engaged_guards);
    if (CourseRaidersBroken(course)) {
        CourseBeginRetreat(course, player);
    }
}

void CcLocalCourseInterpolateInternal(CcLocalCourse *course, float amount)
{
    if (course == NULL) return;
    for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
        CcLocalAgentInterpolateInternal(&course->runners[i].agent, amount);
    }
    for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
        CcLocalAgentInterpolateInternal(&course->raiders[i], amount);
    }
    for (int32_t i = 0; i < CC_LOCAL_TRAVELLER_COUNT; ++i) {
        if (course->travellers[i].active) {
            CcLocalAgentInterpolateInternal(&course->travellers[i].agent,
                                            amount);
        }
    }
    if (course->situation_witness_active) {
        CcLocalAgentInterpolateInternal(&course->situation_witness, amount);
    }
}

typedef struct ResolvedStreetPortal {
    const StreetTraversalLink *link;
    const StreetBoundaryExit *exit;
    int32_t destination_room;
    int32_t exit_index;
    bool reverse;
} ResolvedStreetPortal;

#define STREET_CLICK_NAVIGATION_DESTINATION INT32_MIN

static int32_t StreetRoomForAgent(const CcLocalAgent *agent)
{
    if (agent == NULL || agent->scene != CC_LOCAL_SCENE_STREET) return -1;
    return StreetCameraShotFor(agent->position, -1);
}

static bool ResolveStreetPortal(const CcLocalAgent *agent,
                                int32_t portal_index,
                                ResolvedStreetPortal *resolved)
{
    int32_t room = StreetRoomForAgent(agent);
    if (room < 0 || portal_index < 0 || resolved == NULL) return false;
    int32_t ordinal = 0;
    for (int32_t i = 0;
         i < (int32_t)(sizeof(STREET_TRAVERSAL_LINKS) /
                       sizeof(STREET_TRAVERSAL_LINKS[0])); ++i) {
        const StreetTraversalLink *link = &STREET_TRAVERSAL_LINKS[i];
        if (link->room_a != room && link->room_b != room) continue;
        if (ordinal++ != portal_index) continue;
        *resolved = (ResolvedStreetPortal){
            .link = link,
            .destination_room = link->room_a == room ? link->room_b :
                                                        link->room_a,
            .exit_index = -1,
            .reverse = link->room_b == room,
        };
        return true;
    }
    for (int32_t i = 0;
         i < (int32_t)(sizeof(STREET_BOUNDARY_EXITS) /
                       sizeof(STREET_BOUNDARY_EXITS[0])); ++i) {
        const StreetBoundaryExit *exit = &STREET_BOUNDARY_EXITS[i];
        if (exit->room != room) continue;
        if (ordinal++ != portal_index) continue;
        *resolved = (ResolvedStreetPortal){
            .exit = exit,
            .destination_room = -1,
            .exit_index = i,
        };
        return true;
    }
    return false;
}

int32_t CcLocalAgentStreetPortalCount(const CcLocalAgent *agent)
{
    int32_t room = StreetRoomForAgent(agent);
    if (room < 0) return 0;
    int32_t count = 0;
    for (int32_t i = 0;
         i < (int32_t)(sizeof(STREET_TRAVERSAL_LINKS) /
                       sizeof(STREET_TRAVERSAL_LINKS[0])); ++i) {
        if (STREET_TRAVERSAL_LINKS[i].room_a == room ||
            STREET_TRAVERSAL_LINKS[i].room_b == room) count += 1;
    }
    for (int32_t i = 0;
         i < (int32_t)(sizeof(STREET_BOUNDARY_EXITS) /
                       sizeof(STREET_BOUNDARY_EXITS[0])); ++i) {
        if (STREET_BOUNDARY_EXITS[i].room == room) count += 1;
    }
    return count;
}

const char *CcLocalAgentStreetPortalName(const CcLocalAgent *agent,
                                         int32_t portal_index)
{
    ResolvedStreetPortal portal = {0};
    if (!ResolveStreetPortal(agent, portal_index, &portal)) return NULL;
    if (portal.destination_room >= 0) {
        return CcLocalPlaceRoomName(active_place_function,
                                    portal.destination_room);
    }
    return portal.exit != NULL ? portal.exit->name : NULL;
}

static void ClearAgentNavigation(CcLocalAgent *agent)
{
    if (agent == NULL) return;
    agent->navigation_point_count = 0;
    agent->navigation_point_index = 0;
    agent->navigation_repath_count = 0;
    agent->navigation_destination_room = -1;
    agent->navigation_active = false;
    agent->navigation_world_exit = false;
    agent->world_exit_requested = false;
}

static bool SetAgentExactTarget(CcLocalAgent *agent, Vector3 target,
                                bool market_interior)
{
    CcLocalSceneKind scene = AgentSceneForCall(agent, market_interior);
    if (agent == NULL || !CombatCanAct(&agent->combat) ||
        StaticBodyBlocked(scene, target.x, target.z, agent->radius)) {
        return false;
    }
    agent->scene = scene;
    target.y = SurfaceHeightAt(scene, target.x, target.z);
    agent->target_point = target;
    agent->target_valid = true;
    agent->exact_target_valid = true;
    agent->movement_stall_seconds = 0.0f;
    return true;
}

bool CcLocalAgentSetExactTarget(CcLocalAgent *agent, Vector3 target,
                                bool market_interior)
{
    ClearAgentNavigation(agent);
    if (agent == NULL) return false;
    agent->exact_target_valid = false;
    agent->target_valid = false;
    agent->command_point_valid = false;
    if (!SetAgentExactTarget(agent, target, market_interior)) return false;
    agent->command_origin = agent->position;
    agent->command_point = agent->target_point;
    agent->command_point_valid = true;
    return true;
}

static bool AppendStreetNavigationPoint(CcLocalAgent *agent, Vector2 point)
{
    if (agent->navigation_point_count >=
        CC_LOCAL_NAVIGATION_POINT_CAPACITY ||
        StaticBodyBlocked(CC_LOCAL_SCENE_STREET, point.x, point.y,
                          agent->radius)) {
        return false;
    }
    Vector3 world = {
        point.x,
        SurfaceHeightAt(CC_LOCAL_SCENE_STREET, point.x, point.y),
        point.y,
    };
    Vector3 previous = agent->navigation_point_count > 0 ?
        agent->navigation_point[agent->navigation_point_count - 1] :
        agent->position;
    float x = world.x - previous.x;
    float z = world.z - previous.z;
    if (x * x + z * z < 0.12f * 0.12f) return true;
    agent->navigation_point[agent->navigation_point_count++] = world;
    return true;
}

static bool QueueStreetNavigationPoint(CcLocalAgent *agent, Vector2 point)
{
    if (agent == NULL) return false;
    Vector3 previous = agent->navigation_point_count > 0 ?
        agent->navigation_point[agent->navigation_point_count - 1] :
        agent->position;
    Vector2 from = {previous.x, previous.z};
    Vector2 path[CC_LOCAL_NAVIGATION_POINT_CAPACITY];
    int32_t remaining = CC_LOCAL_NAVIGATION_POINT_CAPACITY -
                        agent->navigation_point_count;
    int32_t path_count = FindStreetPath(from, point, agent->radius,
                                        path, remaining);
    if (path_count <= 0) return false;
    for (int32_t index = 0; index < path_count; ++index) {
        if (!AppendStreetNavigationPoint(agent, path[index])) return false;
    }
    return true;
}

static bool SetStreetClickTarget(CcLocalAgent *agent, Vector3 target)
{
    if (agent == NULL || !CombatCanAct(&agent->combat)) return false;
    ClearAgentNavigation(agent);
    agent->exact_target_valid = false;
    agent->target_valid = false;
    agent->command_point_valid = false;
    float target_land = CcLocalTerrainHeightAt(target.x, target.z);
    float agent_land = CcLocalTerrainHeightAt(agent->position.x,
                                               agent->position.z);
    bool target_on_platform = target.y > target_land + 0.24f;
    bool agent_on_platform = agent->position.y > agent_land + 0.24f;
    if (target_on_platform || agent_on_platform) {
        if (!SetAgentExactTarget(agent, target, false)) return false;
        agent->command_origin = agent->position;
        agent->command_point = agent->target_point;
        agent->command_point_valid = true;
        return true;
    }
    agent->scene = CC_LOCAL_SCENE_STREET;
    if (!QueueStreetNavigationPoint(
            agent, (Vector2){target.x, target.z}) ||
        agent->navigation_point_count <= 0) {
        ClearAgentNavigation(agent);
        return false;
    }
    agent->navigation_destination_room =
        STREET_CLICK_NAVIGATION_DESTINATION;
    agent->navigation_active = true;
    if (!SetAgentExactTarget(agent, agent->navigation_point[0], false)) {
        ClearAgentNavigation(agent);
        return false;
    }
    /* The requested point may have been projected off a wall, prop, or
       disconnected sliver. Expose the actual reachable endpoint as the
       command so the path marker and the body agree. */
    target = agent->navigation_point[agent->navigation_point_count - 1];
    agent->command_origin = agent->position;
    agent->command_point = target;
    agent->command_point_valid = true;
    return true;
}

bool CcLocalAgentSetStreetTarget(CcLocalAgent *agent, Vector3 target)
{
    return SetStreetClickTarget(agent, target);
}

static bool StartStreetPortalTraversal(CcLocalAgent *agent,
                                       int32_t portal_index,
                                       bool include_room_trigger)
{
    ResolvedStreetPortal portal = {0};
    if (agent == NULL || !CombatCanAct(&agent->combat) ||
        !ResolveStreetPortal(agent, portal_index, &portal)) return false;

    int32_t room = StreetRoomForAgent(agent);
    agent->command_point_valid = false;
    ClearAgentNavigation(agent);
    Vector3 room_trigger = {
        STREET_CAMERA_SHOTS[room].trigger.x, 0.0f,
        STREET_CAMERA_SHOTS[room].trigger.y,
    };
    float trigger_x = room_trigger.x - agent->position.x;
    float trigger_z = room_trigger.z - agent->position.z;
    if (include_room_trigger &&
        trigger_x * trigger_x + trigger_z * trigger_z > 1.0f) {
        if (!QueueStreetNavigationPoint(
                agent, STREET_CAMERA_SHOTS[room].trigger)) {
            ClearAgentNavigation(agent);
            return false;
        }
    }

    bool queued = true;
    if (portal.link != NULL) {
        if (!portal.reverse) {
            for (int32_t i = 0; i < portal.link->via_count; ++i) {
                queued = queued && QueueStreetNavigationPoint(
                    agent, portal.link->via[i]);
            }
        } else {
            for (int32_t i = portal.link->via_count - 1; i >= 0; --i) {
                queued = queued && QueueStreetNavigationPoint(
                    agent, portal.link->via[i]);
            }
        }
        queued = queued && QueueStreetNavigationPoint(
            agent, STREET_CAMERA_SHOTS[portal.destination_room].trigger);
    } else if (portal.exit != NULL) {
        for (int32_t i = 0; i < portal.exit->via_count; ++i) {
            queued = queued && QueueStreetNavigationPoint(
                agent, portal.exit->via[i]);
        }
        queued = queued && QueueStreetNavigationPoint(
            agent, portal.exit->endpoint);
    }
    if (!queued || agent->navigation_point_count <= 0) {
        ClearAgentNavigation(agent);
        return false;
    }

    agent->navigation_point_index = 0;
    agent->navigation_destination_room = portal.destination_room >= 0 ?
        portal.destination_room : -1 - portal.exit_index;
    agent->navigation_active = true;
    agent->navigation_world_exit = portal.exit != NULL;
    if (!SetAgentExactTarget(agent, agent->navigation_point[0], false)) {
        ClearAgentNavigation(agent);
        return false;
    }
    return true;
}

bool CcLocalAgentFollowStreetPortal(CcLocalAgent *agent,
                                    int32_t portal_index)
{
    return StartStreetPortalTraversal(agent, portal_index, true);
}

const char *CcLocalAgentNavigationName(const CcLocalAgent *agent)
{
    if (agent == NULL || !agent->navigation_active) return NULL;
    if (agent->navigation_destination_room ==
        STREET_CLICK_NAVIGATION_DESTINATION) return NULL;
    if (agent->navigation_destination_room >= 0) {
        return CcLocalPlaceRoomName(active_place_function,
                                    agent->navigation_destination_room);
    }
    int32_t exit_index = -1 - agent->navigation_destination_room;
    if (exit_index < 0 ||
        exit_index >= (int32_t)(sizeof(STREET_BOUNDARY_EXITS) /
                                sizeof(STREET_BOUNDARY_EXITS[0]))) return NULL;
    return STREET_BOUNDARY_EXITS[exit_index].name;
}

bool CcLocalAgentConsumeWorldExit(CcLocalAgent *agent)
{
    if (agent == NULL || !agent->world_exit_requested) return false;
    agent->world_exit_requested = false;
    return true;
}

static bool AdvanceAgentNavigation(CcLocalAgent *agent)
{
    if (agent == NULL || !agent->navigation_active) return false;
    agent->navigation_point_index += 1;
    if (agent->navigation_point_index < agent->navigation_point_count) {
        bool targeted = SetAgentExactTarget(
            agent, agent->navigation_point[agent->navigation_point_index],
            false);
        if (!targeted) {
            ClearAgentNavigation(agent);
            agent->exact_target_valid = false;
            agent->target_valid = false;
        }
        return targeted;
    }

    bool world_exit = agent->navigation_world_exit;
    agent->navigation_active = false;
    agent->navigation_world_exit = false;
    agent->navigation_point_count = 0;
    agent->navigation_point_index = 0;
    agent->exact_target_valid = false;
    agent->target_valid = false;
    agent->command_point_valid = false;
    agent->world_exit_requested = world_exit;
    return false;
}

static Vector3 StreetPortalWorldPoint(const ResolvedStreetPortal *portal)
{
    Vector2 point = portal->destination_room >= 0 ?
        STREET_CAMERA_SHOTS[portal->destination_room].trigger :
        portal->exit->endpoint;
    return (Vector3){point.x,
                     CcLocalTerrainHeightAt(point.x, point.y) + 0.42f,
                     point.y};
}

static Vector3 StreetPortalApproachWorldPoint(
    const ResolvedStreetPortal *portal)
{
    if (portal->link != NULL && portal->link->via_count > 0) {
        int32_t index = portal->reverse ? portal->link->via_count - 1 : 0;
        Vector2 point = portal->link->via[index];
        return (Vector3){point.x,
                         CcLocalTerrainHeightAt(point.x, point.y) + 0.42f,
                         point.y};
    }
    if (portal->exit != NULL && portal->exit->via_count > 0) {
        Vector2 point = portal->exit->via[0];
        return (Vector3){point.x,
                         CcLocalTerrainHeightAt(point.x, point.y) + 0.42f,
                         point.y};
    }
    return StreetPortalWorldPoint(portal);
}

static Vector2 StreetPortalEdgePoint(const ResolvedStreetPortal *portal,
                                     Camera3D camera, int32_t width,
                                     int32_t height)
{
    Vector2 projected = GetWorldToScreenEx(
        StreetPortalWorldPoint(portal), camera, width, height);
    Vector2 center = {(float)width * 0.5f, (float)height * 0.5f};
    Vector2 direction = {projected.x - center.x,
                         projected.y - center.y};
    if (fabsf(direction.x) + fabsf(direction.y) < 0.001f) {
        direction = (Vector2){1.0f, 0.0f};
    }
    float horizontal = ((float)width * 0.5f - 13.0f) /
                       fmaxf(0.001f, fabsf(direction.x));
    float vertical = ((float)height * 0.5f - 13.0f) /
                     fmaxf(0.001f, fabsf(direction.y));
    float scale = fminf(horizontal, vertical);
    return (Vector2){center.x + direction.x * scale,
                     center.y + direction.y * scale};
}

static float StreetEdgeStripScore(Vector2 point, Vector2 marker,
                                  int32_t width, int32_t height,
                                  float maximum_inward, float maximum_cross)
{
    float left = marker.x;
    float right = (float)width - marker.x;
    float top = marker.y;
    float bottom = (float)height - marker.y;
    float inward = 0.0f;
    float cross = 0.0f;
    if (left <= right && left <= top && left <= bottom) {
        inward = point.x - marker.x;
        cross = fabsf(point.y - marker.y);
    } else if (right <= top && right <= bottom) {
        inward = marker.x - point.x;
        cross = fabsf(point.y - marker.y);
    } else if (top <= bottom) {
        inward = point.y - marker.y;
        cross = fabsf(point.x - marker.x);
    } else {
        inward = marker.y - point.y;
        cross = fabsf(point.x - marker.x);
    }
    if (inward < -6.0f || inward > maximum_inward ||
        cross > maximum_cross) return FLT_MAX;
    return inward / maximum_inward + cross / maximum_cross;
}

static float StreetPortalProximityScore(const CcLocalAgent *agent,
                                        const ResolvedStreetPortal *portal,
                                        int32_t room)
{
    enum { ART_WIDTH = 457, ART_HEIGHT = 285 };
    if (agent == NULL || portal == NULL || room < 0 ||
        room >= (int32_t)(sizeof(STREET_CAMERA_SHOTS) /
                          sizeof(STREET_CAMERA_SHOTS[0]))) return FLT_MAX;
    const StreetCameraShot *composition = &STREET_CAMERA_SHOTS[room];
    Vector3 camera_target = composition->target;
    camera_target.y += CcLocalTerrainHeightAt(camera_target.x,
                                               camera_target.z);
    Camera3D camera = SnapCameraToArtPixels(
        ExteriorCameraComposed(camera_target,
                               composition->camera_offset,
                               composition->fovy),
        ART_HEIGHT);
    Vector2 projected = GetWorldToScreenEx(
        StreetPortalApproachWorldPoint(portal), camera, ART_WIDTH,
        ART_HEIGHT);
    Vector2 center = {(float)ART_WIDTH * 0.5f,
                      (float)ART_HEIGHT * 0.5f};
    Vector2 direction = {projected.x - center.x,
                         projected.y - center.y};
    if (fabsf(direction.x) + fabsf(direction.y) < 0.001f) {
        direction = (Vector2){1.0f, 0.0f};
    }
    float horizontal = ((float)ART_WIDTH * 0.5f - 13.0f) /
                       fmaxf(0.001f, fabsf(direction.x));
    float vertical = ((float)ART_HEIGHT * 0.5f - 13.0f) /
                     fmaxf(0.001f, fabsf(direction.y));
    float marker_scale = fminf(horizontal, vertical);
    Vector2 marker = {center.x + direction.x * marker_scale,
                      center.y + direction.y * marker_scale};
    Vector2 player = GetWorldToScreenEx(
        (Vector3){agent->position.x,
                  CcLocalTerrainHeightAt(agent->position.x,
                                         agent->position.z) + 0.42f,
                  agent->position.z}, camera,
        ART_WIDTH, ART_HEIGHT);

    /* A room exit is both a screen-edge strip and the mouth of a physical
       road. Wider compositions can keep that mouth away from the screen edge,
       so accept an aligned approach within a lens-scaled world radius too. */
    float edge_score = StreetEdgeStripScore(
        player, marker, ART_WIDTH, ART_HEIGHT, 82.0f, 64.0f);
    Vector3 approach = StreetPortalApproachWorldPoint(portal);
    float approach_x = agent->position.x - approach.x;
    float approach_z = agent->position.z - approach.z;
    float approach_distance = sqrtf(approach_x * approach_x +
                                    approach_z * approach_z);
    float corridor_x = composition->trigger.x - approach.x;
    float corridor_z = composition->trigger.y - approach.z;
    float corridor_length = sqrtf(corridor_x * corridor_x +
                                  corridor_z * corridor_z);
    float physical_radius = corridor_length >= 8.0f ?
        fminf(7.2f, corridor_length * 0.55f) : 0.0f;
    float physical_score = physical_radius > 0.0f &&
                           approach_distance <= physical_radius ?
        0.15f + approach_distance / physical_radius : FLT_MAX;
    return fminf(edge_score, physical_score);
}

static float StreetPortalCommandAlignment(
    const CcLocalAgent *agent, const ResolvedStreetPortal *portal)
{
    if (agent == NULL || portal == NULL || !agent->command_point_valid) {
        return -1.0f;
    }
    Vector3 portal_point = StreetPortalWorldPoint(portal);
    float intent_x = agent->command_point.x - agent->command_origin.x;
    float intent_z = agent->command_point.z - agent->command_origin.z;
    float portal_x = portal_point.x - agent->command_origin.x;
    float portal_z = portal_point.z - agent->command_origin.z;
    float intent_length = sqrtf(intent_x * intent_x + intent_z * intent_z);
    float portal_length = sqrtf(portal_x * portal_x + portal_z * portal_z);
    if (intent_length < 0.08f || portal_length < 0.08f) return -1.0f;
    return (intent_x * portal_x + intent_z * portal_z) /
           (intent_length * portal_length);
}

static void UpdateStreetPortalProximity(CcLocalAgent *agent)
{
    bool following_click_path = agent != NULL && agent->navigation_active &&
        agent->navigation_destination_room ==
            STREET_CLICK_NAVIGATION_DESTINATION;
    if (agent == NULL || !agent->crowned ||
        (agent->navigation_active && !following_click_path) ||
        !agent->exact_target_valid || agent->scene != CC_LOCAL_SCENE_STREET) {
        return;
    }
    int32_t room = StreetRoomForAgent(agent);
    int32_t count = CcLocalAgentStreetPortalCount(agent);
    int32_t nearest = -1;
    float nearest_score = FLT_MAX;
    for (int32_t portal_index = 0; portal_index < count; ++portal_index) {
        ResolvedStreetPortal portal = {0};
        if (!ResolveStreetPortal(agent, portal_index, &portal)) continue;
        float alignment = StreetPortalCommandAlignment(agent, &portal);
        if (alignment < 0.35f) continue;
        float score = StreetPortalProximityScore(agent, &portal, room);
        if (score < FLT_MAX) score += (1.0f - alignment) * 0.50f;
        if (score >= nearest_score) continue;
        nearest = portal_index;
        nearest_score = score;
    }
    if (nearest >= 0) {
        /* The player reached the physical edge of this camera room. Continue
           along the authored road without requiring a UI click or sending
           them back through the room center. */
        (void)StartStreetPortalTraversal(agent, nearest, false);
    }
}

static float RayFootprintDistance(Ray ray, Rectangle footprint, float height)
{
    BoundingBox box = {
        .min = {footprint.x, 0.0f, footprint.y},
        .max = {footprint.x + footprint.width, height,
                footprint.y + footprint.height}
    };
    RayCollision collision = GetRayCollisionBox(ray, box);
    return collision.hit ? collision.distance : FLT_MAX;
}

static float RoomArtObstacleRayDistance(Ray ray, Vector3 focus)
{
    float nearest = FLT_MAX;
    for (int32_t i = 0; i < (int32_t)(sizeof(ROOM_ART_OBSTACLES) /
                                      sizeof(ROOM_ART_OBSTACLES[0])); ++i) {
        Rectangle footprint = ROOM_ART_OBSTACLES[i];
        float center_x = footprint.x + footprint.width * 0.5f;
        float center_z = footprint.y + footprint.height * 0.5f;
        if (!RoomDetailPointVisible(center_x, center_z, focus)) continue;
        nearest = fminf(nearest,
                        RayFootprintDistance(ray, footprint, 4.90f));
    }
    return nearest;
}

float CcLocalRoomArtRayDistanceInternal(Ray ray, Vector3 focus)
{
    return RoomArtObstacleRayDistance(ray, focus);
}

static bool SetNearestClickTarget(CcLocalAgent *agent, Vector3 picked_point,
                                  bool market_interior)
{
    CcLocalSceneKind scene = AgentSceneForCall(agent, market_interior);
    bool targeted = scene == CC_LOCAL_SCENE_STREET ?
        SetStreetClickTarget(agent, picked_point) :
        CcLocalAgentSetExactTarget(agent, picked_point, market_interior);
    if (targeted) {
        return true;
    }

    /* A ground click beside a person, prop, or collision boundary should
       still produce useful movement. Search the near half-circle facing back
       toward the hero so the fallback remains on the approachable side. */
    float toward_x = agent->position.x - picked_point.x;
    float toward_z = agent->position.z - picked_point.z;
    float toward_length = sqrtf(toward_x * toward_x + toward_z * toward_z);
    if (toward_length > 0.0001f) {
        toward_x /= toward_length;
        toward_z /= toward_length;
    } else {
        toward_x = sinf(agent->facing_yaw + PI);
        toward_z = cosf(agent->facing_yaw + PI);
    }
    static const int32_t angle_steps[] = {0, 1, -1, 2, -2, 3, -3};
    for (int32_t ring = 1; ring <= 3; ++ring) {
        float radius = (float)ring * 0.24f;
        for (int32_t i = 0; i < (int32_t)(sizeof(angle_steps) /
                                          sizeof(angle_steps[0])); ++i) {
            float angle = (float)angle_steps[i] * PI / 6.0f;
            float cosine = cosf(angle);
            float sine = sinf(angle);
            Vector3 candidate = picked_point;
            candidate.x += (toward_x * cosine - toward_z * sine) * radius;
            candidate.z += (toward_x * sine + toward_z * cosine) * radius;
            targeted = scene == CC_LOCAL_SCENE_STREET ?
                SetStreetClickTarget(agent, candidate) :
                CcLocalAgentSetExactTarget(agent, candidate,
                                           market_interior);
            if (targeted) {
                return true;
            }
        }
    }
    return false;
}

static RayCollision RayTerrainCollision(Ray ray)
{
    RayCollision nearest = {0};
    nearest.distance = FLT_MAX;
    const float step = 1.0f;
    for (float z = 0.0f; z < CC_LOCAL_WORLD_DEPTH; z += step) {
        float far_z = fminf(z + step, CC_LOCAL_WORLD_DEPTH);
        for (float x = 0.0f; x < CC_LOCAL_WORLD_WIDTH; x += step) {
            float far_x = fminf(x + step, CC_LOCAL_WORLD_WIDTH);
            Vector3 p00 = {x, CcLocalTerrainHeightAt(x, z), z};
            Vector3 p10 = {far_x, CcLocalTerrainHeightAt(far_x, z), z};
            Vector3 p01 = {x, CcLocalTerrainHeightAt(x, far_z), far_z};
            Vector3 p11 = {far_x,
                           CcLocalTerrainHeightAt(far_x, far_z), far_z};
            RayCollision hit = GetRayCollisionTriangle(ray, p00, p11, p10);
            if (hit.hit && hit.distance < nearest.distance) nearest = hit;
            hit = GetRayCollisionTriangle(ray, p00, p01, p11);
            if (hit.hit && hit.distance < nearest.distance) nearest = hit;
        }
    }
    return nearest;
}

bool CcLocalAgentPickTarget(CcLocalAgent *agent, Vector2 screen_point,
                            RenderTexture2D target, Rectangle destination,
                            bool market_interior)
{
    if (agent == NULL || !CombatCanAct(&agent->combat) ||
        !CheckCollisionPointRec(screen_point, destination)) return false;
    CcLocalSceneKind scene = AgentSceneForCall(agent, market_interior);
    bool interior = scene == CC_LOCAL_SCENE_MARKET;
    Vector2 local = {
        (screen_point.x - destination.x) / destination.width * (float)target.texture.width,
        (screen_point.y - destination.y) / destination.height * (float)target.texture.height
    };
    Camera3D camera = {0};
    if (!PresentedCameraForInput(scene, agent, target.texture.width,
                                 target.texture.height, &camera)) {
        camera = interior ? LocalCamera(true, agent->position) :
            scene == CC_LOCAL_SCENE_ROAD ?
                RoadCamera(agent->position, false, 0.0f, false,
                           target.texture.height) :
                CcLocalStreetCameraInternal(agent, 0.0f, false,
                                            target.texture.height);
    }
    Ray ray = GetScreenToWorldRayEx(local, camera, target.texture.width,
                                    target.texture.height);
    float nearest = FLT_MAX;
    Vector3 picked_point = {0};
    const NavPlatform *picked_platform = NULL;
    RayCollision collision = {0};
    if (scene == CC_LOCAL_SCENE_STREET) {
        collision = RayTerrainCollision(ray);
    } else {
        BoundingBox ground = {
            .min = {0.0f, -0.08f, 0.0f},
            .max = {interior ? 9.0f : CC_LOCAL_WORLD_WIDTH, 0.01f,
                    interior ? 7.0f : CC_LOCAL_WORLD_DEPTH}
        };
        collision = GetRayCollisionBox(ray, ground);
    }
    if (collision.hit) {
        nearest = collision.distance;
        picked_point = collision.point;
    }
    if (scene == CC_LOCAL_SCENE_STREET) {
        for (int32_t i = 0; i < StreetPhysicsPlatformCount(); ++i) {
            const NavPlatform *platform = StreetPhysicsPlatformAt(i);
            if (platform == NULL) continue;
            float base = PlatformBaseHeight(platform);
            float top = PlatformTopHeight(platform);
            BoundingBox box = {
                .min = {platform->x, base - 0.02f, platform->z},
                .max = {platform->x + platform->width,
                        top + 0.02f,
                        platform->z + platform->depth}
            };
            collision = GetRayCollisionBox(ray, box);
            if (!collision.hit || collision.distance >= nearest) continue;
            nearest = collision.distance;
            picked_point = collision.point;
            picked_platform = platform;
        }
    }
    if (nearest == FLT_MAX) return false;
    if (picked_platform != NULL) {
        /* Physical surfaces win over the broad road-mouth intent zone. This
           keeps a crate, stair, or training obstacle clickable even when it
           is composed near the edge of a room. A camera ray commonly reaches
           the leading vertical face before the top plane; snap that hit into
           the usable top so it cannot turn into a ground-level walk command
           that stalls against the same obstacle. */
        float half_extent = fminf(picked_platform->width,
                                  picked_platform->depth) * 0.5f;
        float inset = fminf(agent->radius + 0.035f,
                            fmaxf(0.02f, half_extent - 0.02f));
        picked_point.x = fmaxf(
            picked_platform->x + inset,
            fminf(picked_point.x,
                  picked_platform->x + picked_platform->width - inset));
        picked_point.z = fmaxf(
            picked_platform->z + inset,
            fminf(picked_point.z,
                  picked_platform->z + picked_platform->depth - inset));
        picked_point.y = PlatformTopHeight(picked_platform);
        return SetStreetClickTarget(agent, picked_point);
    }
    float occluder = FLT_MAX;
    if (interior) {
        occluder = fminf(occluder,
                         RayFootprintDistance(ray, MARKET_COUNTER_FOOTPRINT, 0.92f));
        occluder = fminf(occluder,
                         RayFootprintDistance(ray, MARKET_SHELF_FOOTPRINT, 1.90f));
        occluder = fminf(occluder, RayFootprintDistance(
            ray, (Rectangle){0.0f, 0.0f, 9.0f, 0.50f}, 2.60f));
        occluder = fminf(occluder, RayFootprintDistance(
            ray, (Rectangle){0.0f, 0.0f, 0.50f, 7.0f}, 2.60f));
    } else if (scene == CC_LOCAL_SCENE_STREET) {
        uint32_t foreground_mask = StreetForegroundBuildingMask();
        for (int32_t i = 0; i < (int32_t)(sizeof(WORLD_BUILDINGS) /
                                          sizeof(WORLD_BUILDINGS[0])); ++i) {
            if ((foreground_mask & (UINT32_C(1) << i)) != 0) continue;
            occluder = fminf(occluder,
                             RayFootprintDistance(
                                 ray, WORLD_BUILDINGS[i].footprint,
                                 WORLD_BUILDINGS[i].height));
        }
        for (int32_t i = 0; i < (int32_t)(sizeof(CASTLE_STRUCTURES) /
                                          sizeof(CASTLE_STRUCTURES[0])); ++i) {
            occluder = fminf(occluder,
                             RayFootprintDistance(
                                 ray, CASTLE_STRUCTURES[i].footprint,
                                 CASTLE_STRUCTURES[i].height));
        }
        for (int32_t i = 0; i < CC_LOCAL_PLACE_LANDMARK_COUNT; ++i) {
            const CcLocalPlaceLandmark *landmark =
                ActivePlaceLandmarkAt(i);
            if (landmark == NULL) continue;
            occluder = fminf(
                occluder,
                RayFootprintDistance(ray, PlaceLandmarkFootprint(landmark),
                                     landmark->height));
        }
        occluder = fminf(occluder,
                         RayFootprintDistance(ray, CARRIAGE_FOOTPRINT, 1.92f));
        occluder = fminf(occluder,
                         RayFootprintDistance(ray, DUNGEON_FOOTPRINT, 2.45f));
        occluder = fminf(occluder,
                         RoomArtObstacleRayDistance(ray, camera.target));
    } else {
        for (int32_t i = 0; i < RoadObstacleCount(); ++i) {
            occluder = fminf(
                occluder,
                RayFootprintDistance(ray, RoadObstacleAt(i), 2.20f));
        }
    }
    /* In the fixed street rooms, foreground architecture often lies between
       the camera and a perfectly walkable road point. Rejecting that ray made
       the last strip of visible ground impossible to click, so street input
       is decided by ground walkability instead. */
    bool street_fixed_room = scene == CC_LOCAL_SCENE_STREET &&
                             camera.projection == CAMERA_ORTHOGRAPHIC;
    if (!street_fixed_room && occluder < nearest) return false;
    return SetNearestClickTarget(agent, picked_point, market_interior);
}

static float WrapAngle(float angle)
{
    while (angle > PI) angle -= 2.0f * PI;
    while (angle < -PI) angle += 2.0f * PI;
    return angle;
}

static float Approach(float current, float target, float maximum_change)
{
    if (current < target) return fminf(target, current + maximum_change);
    return fmaxf(target, current - maximum_change);
}

static float SmoothStep01(float amount)
{
    amount = fmaxf(0.0f, fminf(amount, 1.0f));
    return amount * amount * (3.0f - 2.0f * amount);
}

static Vector3 PhysicsAdd(Vector3 a, Vector3 b)
{
    return (Vector3){a.x + b.x, a.y + b.y, a.z + b.z};
}

static Vector3 PhysicsSubtract(Vector3 a, Vector3 b)
{
    return (Vector3){a.x - b.x, a.y - b.y, a.z - b.z};
}

static Vector3 PhysicsScale(Vector3 value, float scale)
{
    return (Vector3){value.x * scale, value.y * scale, value.z * scale};
}

static float PhysicsDot(Vector3 a, Vector3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static float PhysicsLength(Vector3 value)
{
    return sqrtf(PhysicsDot(value, value));
}

static Vector3 PhysicsCross(Vector3 a, Vector3 b)
{
    return (Vector3){a.y * b.z - a.z * b.y,
                     a.z * b.x - a.x * b.z,
                     a.x * b.y - a.y * b.x};
}

static Vector3 PhysicsNormalizeOr(Vector3 value, Vector3 fallback)
{
    float length = PhysicsLength(value);
    return length > 0.0001f ? PhysicsScale(value, 1.0f / length) : fallback;
}

static Vector3 PhysicsLerp(Vector3 a, Vector3 b, float amount)
{
    return PhysicsAdd(a, PhysicsScale(PhysicsSubtract(b, a), amount));
}

static void ResetHeroCape(CcLocalAgent *agent,
                          const CcHumanoidSkinPose *skin,
                          Vector3 anchor)
{
    static const float SEGMENT_LENGTHS[] = {0.270f, 0.275f, 0.285f, 0.285f};
    CcLocalCapeState *cape = &agent->cape;
    Vector3 up = FromLimbVector(skin->body_up);
    Vector3 backward = PhysicsScale(FromLimbVector(skin->body_forward), -1.0f);
    Vector3 rest_direction = PhysicsNormalizeOr(
        PhysicsAdd(PhysicsScale(up, -0.975f),
                   PhysicsScale(backward, 0.22f)),
        (Vector3){0.0f, -1.0f, 0.0f});
    cape->point[0] = anchor;
    cape->previous[0] = anchor;
    for (int32_t point = 1; point < CC_LOCAL_CAPE_POINT_COUNT; ++point) {
        cape->point[point] = PhysicsAdd(
            cape->point[point - 1],
            PhysicsScale(rest_direction, SEGMENT_LENGTHS[point - 1]));
        cape->previous[point] = cape->point[point];
    }
    cape->anchor = anchor;
    cape->initialized = true;
}

static void UpdateHeroCape(CcLocalAgent *agent, float delta_time)
{
    if (agent == NULL || agent->morphology != CC_MORPHOLOGY_BIPED ||
        !agent->humanoid.initialized) return;
    static const float SEGMENT_LENGTHS[] = {0.270f, 0.275f, 0.285f, 0.285f};
    CcHumanoidSkinPose skin;
    CcHumanoidSkinPoseResolve(&agent->humanoid.pose, &skin);
    if (!skin.valid) return;
    Vector3 up = FromLimbVector(skin.body_up);
    Vector3 forward = FromLimbVector(skin.body_forward);
    Vector3 anchor = PhysicsAdd(
        FromLimbVector(skin.sockets[CC_HUMANOID_SOCKET_BACK].position),
        PhysicsScale(up, 0.14f));
    CcLocalCapeState *cape = &agent->cape;
    if (!cape->initialized ||
        PhysicsLength(PhysicsSubtract(anchor, cape->anchor)) > 0.72f) {
        ResetHeroCape(agent, &skin, anchor);
        return;
    }

    float step = fmaxf(1.0f / 240.0f, fminf(delta_time, 1.0f / 30.0f));
    float damping = expf(-3.2f * step);
    Vector3 acceleration = {
        -agent->velocity.x * 2.15f,
        agent->swimming ? 0.45f : -7.25f,
        -agent->velocity.z * 2.15f,
    };
    acceleration = PhysicsAdd(
        acceleration,
        PhysicsScale(forward, -0.30f * PhysicsLength(agent->velocity)));
    for (int32_t point = 1; point < CC_LOCAL_CAPE_POINT_COUNT; ++point) {
        Vector3 current = cape->point[point];
        Vector3 velocity = PhysicsScale(
            PhysicsSubtract(current, cape->previous[point]), damping);
        cape->previous[point] = current;
        cape->point[point] = PhysicsAdd(
            PhysicsAdd(current, velocity),
            PhysicsScale(acceleration, step * step));
    }
    cape->point[0] = anchor;
    cape->previous[0] = anchor;

    Vector3 chest = FromLimbVector(skin.bones[CC_HUMANOID_SKIN_CHEST].head);
    Vector3 pelvis = FromLimbVector(skin.bones[CC_HUMANOID_SKIN_PELVIS].head);
    for (int32_t iteration = 0; iteration < 9; ++iteration) {
        cape->point[0] = anchor;
        for (int32_t point = 1; point < CC_LOCAL_CAPE_POINT_COUNT; ++point) {
            Vector3 delta = PhysicsSubtract(cape->point[point],
                                            cape->point[point - 1]);
            float distance = PhysicsLength(delta);
            if (distance <= 0.0001f) continue;
            Vector3 correction = PhysicsScale(
                delta, (distance - SEGMENT_LENGTHS[point - 1]) / distance);
            if (point == 1) {
                cape->point[point] = PhysicsSubtract(cape->point[point],
                                                     correction);
            } else {
                cape->point[point - 1] = PhysicsAdd(
                    cape->point[point - 1], PhysicsScale(correction, 0.5f));
                cape->point[point] = PhysicsSubtract(
                    cape->point[point], PhysicsScale(correction, 0.5f));
            }
        }
        for (int32_t point = 2; point < CC_LOCAL_CAPE_POINT_COUNT; ++point) {
            float rest_span = (SEGMENT_LENGTHS[point - 2] +
                               SEGMENT_LENGTHS[point - 1]) * 0.985f;
            Vector3 delta = PhysicsSubtract(cape->point[point],
                                            cape->point[point - 2]);
            float distance = PhysicsLength(delta);
            if (distance <= rest_span || distance <= 0.0001f) continue;
            Vector3 correction = PhysicsScale(
                delta, (distance - rest_span) / distance * 0.16f);
            if (point == 2) {
                cape->point[point] = PhysicsSubtract(cape->point[point],
                                                     correction);
            } else {
                cape->point[point - 2] = PhysicsAdd(
                    cape->point[point - 2], PhysicsScale(correction, 0.5f));
                cape->point[point] = PhysicsSubtract(
                    cape->point[point], PhysicsScale(correction, 0.5f));
            }
        }
        for (int32_t point = 1; point < CC_LOCAL_CAPE_POINT_COUNT; ++point) {
            float amount = (float)point /
                           (float)(CC_LOCAL_CAPE_POINT_COUNT - 1);
            Vector3 torso = PhysicsLerp(chest, pelvis, amount * 0.86f);
            float front_distance = PhysicsDot(
                PhysicsSubtract(cape->point[point], torso), forward);
            float rear_limit = -0.115f - amount * 0.035f;
            if (front_distance > rear_limit) {
                cape->point[point] = PhysicsAdd(
                    cape->point[point],
                    PhysicsScale(forward, rear_limit - front_distance));
            }
        }
    }
    cape->point[0] = anchor;
    cape->anchor = anchor;
}

static Vector3 FramePoint(Vector3 base, float x, float y, float z, float yaw)
{
    float cosine = cosf(yaw);
    float sine = sinf(yaw);
    return (Vector3){base.x + x * cosine + z * sine,
                     base.y + y,
                     base.z - x * sine + z * cosine};
}

static bool BeginClimb(CcLocalAgent *agent, const NavPlatform *platform)
{
    if (agent->morphology == CC_MORPHOLOGY_BIPED &&
        (!agent->humanoid.initialized || agent->humanoid.ragdoll.active ||
         agent->humanoid.recovering || agent->humanoid.climbing)) {
        return false;
    }
    float platform_top = PlatformTopHeight(platform);
    float left = fabsf(agent->position.x - platform->x);
    float right_edge = fabsf(agent->position.x -
                             (platform->x + platform->width));
    float near_edge = fabsf(agent->position.z - platform->z);
    float far_edge = fabsf(agent->position.z -
                           (platform->z + platform->depth));
    float nearest = left;
    Vector3 normal = {-1.0f, 0.0f, 0.0f};
    Vector3 face = {platform->x, platform_top, agent->position.z};
    if (right_edge < nearest) {
        nearest = right_edge;
        normal = (Vector3){1.0f, 0.0f, 0.0f};
        face = (Vector3){platform->x + platform->width, platform_top,
                         agent->position.z};
    }
    if (near_edge < nearest) {
        nearest = near_edge;
        normal = (Vector3){0.0f, 0.0f, -1.0f};
        face = (Vector3){agent->position.x, platform_top,
                         platform->z};
    }
    if (far_edge < nearest) {
        normal = (Vector3){0.0f, 0.0f, 1.0f};
        face = (Vector3){agent->position.x, platform_top,
                         platform->z + platform->depth};
    }
    if (normal.x != 0.0f) {
        face.z = fmaxf(platform->z + 0.24f,
                       fminf(face.z,
                             platform->z + platform->depth - 0.24f));
    } else {
        face.x = fmaxf(platform->x + 0.24f,
                       fminf(face.x,
                             platform->x + platform->width - 0.24f));
    }
    Vector3 forward = PhysicsScale(normal, -1.0f);
    float facing = atan2f(forward.x, forward.z);
    Vector3 frame_right = {cosf(facing), 0.0f, -sinf(facing)};
    Vector3 body = {agent->position.x,
                    agent->position.y + agent->limb_rig.morphology.body_height,
                    agent->position.z};
    Vector3 shoulder_left = FramePoint(body, -0.25f, 0.48f, 0.0f, facing);
    Vector3 shoulder_right = FramePoint(body, 0.25f, 0.48f, 0.0f, facing);
    Vector3 hand_left = PhysicsAdd(PhysicsAdd(face, PhysicsScale(normal, 0.025f)),
                                  PhysicsScale(frame_right, -0.16f));
    Vector3 hand_right = PhysicsAdd(PhysicsAdd(face, PhysicsScale(normal, 0.025f)),
                                   PhysicsScale(frame_right, 0.16f));
    hand_left.y += 0.035f;
    hand_right.y += 0.035f;
    float arm_reach = 0.69f +
        (float)(AthleticLevel(agent, CC_ATHLETIC_GRIP) - 1) * 0.025f;
    float ledge_rise = platform_top - agent->position.y;
    bool low_ledge = ledge_rise <= 0.72f;
    if (!low_ledge &&
        (PhysicsLength(PhysicsSubtract(hand_left, shoulder_left)) > arm_reach ||
         PhysicsLength(PhysicsSubtract(hand_right, shoulder_right)) > arm_reach)) {
        return false;
    }

    agent->climb_start = agent->position;
    agent->climb_end = PhysicsAdd(face,
                                  PhysicsScale(normal, -(agent->radius + 0.18f)));
    agent->climb_end.y = platform_top;
    if (StaticBodyBlocked(agent->scene, agent->climb_end.x,
                          agent->climb_end.z,
                          agent->radius)) return false;
    agent->climb_face = face;
    agent->climb_normal = normal;
    agent->climb_hand_left = hand_left;
    agent->climb_hand_right = hand_right;
    if (agent->morphology == CC_MORPHOLOGY_BIPED &&
        agent->humanoid.initialized) {
        agent->climb_foot_left = FromLimbVector(
            agent->humanoid.feet[0].current_point);
        agent->climb_foot_right = FromLimbVector(
            agent->humanoid.feet[1].current_point);
    }
    agent->facing_yaw = facing;
    float rise = agent->climb_end.y - agent->climb_start.y;
    agent->vaulting = rise <= 1.08f &&
        AthleticLevel(agent, CC_ATHLETIC_MOBILITY) >= 2;
    float base_duration = agent->vaulting ? 0.76f + rise * 0.10f :
                                            1.70f + rise * 0.18f;
    float athletic_rate =
        AthleticBonus(agent, CC_ATHLETIC_GRIP, 0.07f) *
        AthleticBonus(agent, CC_ATHLETIC_MOBILITY, 0.035f);
    if (agent->vaulting) {
        agent->climb_duration = fmaxf(0.56f,
                                      base_duration / athletic_rate);
    } else {
        /* A high mantle has four readable support changes. Athleticism can
           make it decisive, but must not collapse it into a half-second blur
           where catch, wall plant, and top-out happen on adjacent frames. */
        float mantle_rate = fminf(athletic_rate, 1.35f);
        agent->climb_duration = fmaxf(1.35f,
                                      base_duration / mantle_rate);
    }
    agent->climb_progress = 0.0f;
    agent->climb_settle = 0.0f;
    agent->climbing = true;
    agent->climbing_down = false;
    agent->climb_training_pending = true;
    agent->grounded = true;
    agent->velocity = (Vector3){0};
    agent->traversal = agent->vaulting ? CC_TRAVERSAL_VAULT :
                                         CC_TRAVERSAL_CLIMB;
    agent->support_state = CC_HUMANOID_SUPPORT_HANDS;
    if (agent->morphology == CC_MORPHOLOGY_BIPED) {
        CcHumanoidGaitBeginClimb(&agent->humanoid);
        agent->humanoid_needs_reset = false;
    }
    return true;
}

static bool BeginDownClimb(CcLocalAgent *agent,
                           const NavPlatform *platform,
                           bool move_x, float amount)
{
    if (!agent->allow_downclimb ||
        agent->morphology != CC_MORPHOLOGY_BIPED ||
        !agent->humanoid.initialized || agent->humanoid.ragdoll.active ||
        agent->humanoid.recovering || agent->humanoid.climbing) {
        return false;
    }
    float platform_top = PlatformTopHeight(platform);
    Vector3 normal = {0.0f, 0.0f, 0.0f};
    Vector3 face = {agent->position.x, platform_top, agent->position.z};
    if (move_x) {
        normal.x = amount < 0.0f ? -1.0f : 1.0f;
        face.x = amount < 0.0f ? platform->x :
                                 platform->x + platform->width;
        face.z = fmaxf(platform->z + 0.24f,
                       fminf(face.z,
                             platform->z + platform->depth - 0.24f));
    } else {
        normal.z = amount < 0.0f ? -1.0f : 1.0f;
        face.z = amount < 0.0f ? platform->z :
                                 platform->z + platform->depth;
        face.x = fmaxf(platform->x + 0.24f,
                       fminf(face.x,
                             platform->x + platform->width - 0.24f));
    }
    Vector3 descent_end = PhysicsAdd(
        face, PhysicsScale(normal, agent->radius + 0.42f));
    descent_end.y = BodySurfaceHeightAt(agent->scene, descent_end.x,
                                        descent_end.z);
    if (descent_end.y >= platform_top - 0.24f ||
        StaticBodyBlocked(agent->scene, descent_end.x, descent_end.z,
                          agent->radius)) {
        return false;
    }
    float facing = atan2f(-normal.x, -normal.z);
    Vector3 frame_right = {cosf(facing), 0.0f, -sinf(facing)};
    Vector3 hand_center = PhysicsAdd(face, PhysicsScale(normal, 0.018f));
    hand_center.y = platform_top + 0.035f;

    agent->climb_start = agent->position;
    agent->climb_end = descent_end;
    agent->climb_face = face;
    agent->climb_normal = normal;
    agent->climb_hand_left = PhysicsAdd(
        hand_center, PhysicsScale(frame_right, -0.16f));
    agent->climb_hand_right = PhysicsAdd(
        hand_center, PhysicsScale(frame_right, 0.16f));
    agent->climb_start_yaw = agent->facing_yaw;
    agent->climb_end_yaw = facing;
    agent->climb_duration = 1.42f +
        (platform_top - descent_end.y) * 0.18f;
    agent->climb_duration /= AthleticBonus(agent, CC_ATHLETIC_GRIP, 0.06f);
    agent->climb_progress = 0.0f;
    agent->climb_settle = 0.0f;
    agent->climbing = true;
    agent->climbing_down = true;
    agent->vaulting = false;
    agent->climb_training_pending = true;
    agent->grounded = true;
    agent->velocity = (Vector3){0};
    agent->traversal = CC_TRAVERSAL_DESCEND;
    agent->support_state = CC_HUMANOID_SUPPORT_HANDS;
    CcHumanoidGaitBeginClimb(&agent->humanoid);
    agent->humanoid_needs_reset = false;
    return true;
}

static bool ReleaseClimbToPhysicalFall(CcLocalAgent *agent)
{
    if (agent == NULL || !agent->humanoid.ragdoll.active) return false;
    agent->position = FromLimbVector(CcHumanoidGaitAuthoritativePosition(
        &agent->humanoid, ToLimbVector(agent->position)));
    agent->velocity = FromLimbVector(CcHumanoidGaitAuthoritativeVelocity(
        &agent->humanoid, ToLimbVector(agent->velocity)));
    agent->climbing = false;
    agent->climbing_down = false;
    agent->vaulting = false;
    agent->climb_training_pending = false;
    agent->grounded = false;
    agent->support_state = CC_HUMANOID_SUPPORT_UNCONTROLLED_FALL;
    agent->traversal = CC_TRAVERSAL_RAGDOLL;
    return true;
}

static void UpdateDownClimb(CcLocalAgent *agent, float delta_time,
                            bool market_interior)
{
    Vector3 previous_position = agent->position;
    agent->climb_progress = fminf(1.0f, agent->climb_progress +
                                  delta_time / agent->climb_duration);
    float amount = agent->climb_progress;
    float turn = SmoothStep01(amount / 0.20f);
    float yaw_delta = WrapAngle(agent->climb_end_yaw -
                                agent->climb_start_yaw);
    agent->facing_yaw = WrapAngle(agent->climb_start_yaw + yaw_delta * turn);

    Vector3 outside_top = PhysicsAdd(
        agent->climb_face,
        PhysicsScale(agent->climb_normal, agent->radius + 0.09f));
    outside_top.y = agent->climb_face.y - 0.10f;
    Vector3 hang = outside_top;
    hang.y = fmaxf(agent->climb_end.y + 0.34f,
                   agent->climb_face.y - 1.08f);
    float edge_transfer = SmoothStep01((amount - 0.02f) / 0.22f);
    float lower_transfer = SmoothStep01((amount - 0.08f) / 0.30f);
    float land_transfer = SmoothStep01((amount - 0.64f) / 0.28f);
    Vector3 target = PhysicsLerp(agent->climb_start, outside_top,
                                 edge_transfer);
    target = PhysicsLerp(target, hang, lower_transfer);
    target = PhysicsLerp(target, agent->climb_end, land_transfer);

    Vector3 acceleration = PhysicsSubtract(
        PhysicsScale(PhysicsSubtract(target, agent->position), 31.0f),
        PhysicsScale(agent->velocity, 9.5f));
    agent->velocity = PhysicsAdd(agent->velocity,
                                 PhysicsScale(acceleration, delta_time));
    float speed = PhysicsLength(agent->velocity);
    if (speed > 2.7f) {
        agent->velocity = PhysicsScale(agent->velocity, 2.7f / speed);
    }
    agent->position = PhysicsAdd(agent->position,
                                 PhysicsScale(agent->velocity, delta_time));
    agent->grounded = amount < 0.10f;

    float outside_distance = PhysicsDot(
        PhysicsSubtract(agent->position, agent->climb_face),
        agent->climb_normal);
    float clearance_weight = SmoothStep01((amount - 0.18f) / 0.24f);
    float required_outside = (agent->radius + 0.035f) * clearance_weight;
    if (amount < 0.86f && outside_distance < required_outside) {
        agent->position = PhysicsAdd(
            agent->position,
            PhysicsScale(agent->climb_normal,
                         required_outside - outside_distance));
        float into_face = PhysicsDot(agent->velocity, agent->climb_normal);
        if (into_face < 0.0f) {
            agent->velocity = PhysicsSubtract(
                agent->velocity,
                PhysicsScale(agent->climb_normal, into_face));
        }
    }
    if (agent->position.y < agent->climb_end.y) {
        agent->position.y = agent->climb_end.y;
        if (agent->velocity.y < 0.0f) agent->velocity.y = 0.0f;
    }

    Vector3 resolved_position = agent->position;
    Vector3 collision_normal = {0};
    if (ResolveLocalAgentCapsuleMove(
            AgentSceneForCall(agent, market_interior), previous_position,
            agent->position, agent->radius, -FLT_MAX, &resolved_position,
            &collision_normal)) {
        agent->position = resolved_position;
        float into_surface = PhysicsDot(agent->velocity, collision_normal);
        if (into_surface < 0.0f) {
            agent->velocity = PhysicsSubtract(
                agent->velocity,
                PhysicsScale(collision_normal, into_surface));
        }
    }

    float end_distance = PhysicsLength(
        PhysicsSubtract(agent->position, agent->climb_end));
    if (amount >= 1.0f && end_distance < 0.020f) {
        agent->position = agent->climb_end;
        agent->velocity = (Vector3){0};
        agent->grounded = true;
        agent->climb_settle = fminf(1.0f, agent->climb_settle +
                                    delta_time / 0.64f);
    }

    LocalProbeContext context = {
        .scene = AgentSceneForCall(agent, market_interior)
    };
    Vector3 frame_right = {cosf(agent->facing_yaw), 0.0f,
                           -sinf(agent->facing_yaw)};
    Vector3 up = {0.0f, 1.0f, 0.0f};
    Vector3 top_left = PhysicsAdd(
        PhysicsAdd(agent->climb_face,
                   PhysicsScale(agent->climb_normal, -0.18f)),
        PhysicsScale(frame_right, -0.13f));
    Vector3 top_right = PhysicsAdd(
        PhysicsAdd(agent->climb_face,
                   PhysicsScale(agent->climb_normal, -0.18f)),
        PhysicsScale(frame_right, 0.13f));
    top_left.y = agent->climb_face.y + 0.035f;
    top_right.y = agent->climb_face.y + 0.035f;
    /* Contacts are world anchors, not offsets from the moving root. Stagger
       the feet so the descent reads as two deliberate placements rather than
       a pair of boots sliding down the wall together. */
    Vector3 wall_left = PhysicsAdd(
        PhysicsAdd(agent->climb_face,
                   PhysicsScale(agent->climb_normal, 0.018f)),
        PhysicsScale(frame_right, -0.13f));
    Vector3 wall_right = PhysicsAdd(
        PhysicsAdd(agent->climb_face,
                   PhysicsScale(agent->climb_normal, 0.018f)),
        PhysicsScale(frame_right, 0.13f));
    wall_left.y = fmaxf(agent->climb_end.y + 0.22f,
                        agent->climb_face.y - 0.50f);
    wall_right.y = fmaxf(agent->climb_end.y + 0.30f,
                         agent->climb_face.y - 0.68f);
    Vector3 ground_left = PhysicsAdd(
        agent->climb_end, PhysicsScale(frame_right, -0.13f));
    Vector3 ground_right = PhysicsAdd(
        agent->climb_end, PhysicsScale(frame_right, 0.13f));
    ground_left.y = agent->climb_end.y + 0.035f;
    ground_right.y = agent->climb_end.y + 0.035f;
    float left_wall_transfer = SmoothStep01((amount - 0.16f) / 0.22f);
    float right_wall_transfer = SmoothStep01((amount - 0.28f) / 0.22f);
    float left_ground_transfer = SmoothStep01((amount - 0.52f) / 0.34f);
    float right_ground_transfer = SmoothStep01((amount - 0.64f) / 0.34f);
    Vector3 foot_targets[CC_HUMANOID_LEG_COUNT] = {
        PhysicsLerp(PhysicsLerp(top_left, wall_left, left_wall_transfer),
                    ground_left, left_ground_transfer),
        PhysicsLerp(PhysicsLerp(top_right, wall_right, right_wall_transfer),
                    ground_right, right_ground_transfer)
    };
    Vector3 wall_normal_left = PhysicsNormalizeOr(
        PhysicsLerp(up, agent->climb_normal, left_wall_transfer), up);
    Vector3 wall_normal_right = PhysicsNormalizeOr(
        PhysicsLerp(up, agent->climb_normal, right_wall_transfer), up);
    Vector3 foot_normals[CC_HUMANOID_LEG_COUNT] = {
        PhysicsNormalizeOr(
            PhysicsLerp(wall_normal_left, up, left_ground_transfer), up),
        PhysicsNormalizeOr(
            PhysicsLerp(wall_normal_right, up, right_ground_transfer), up)
    };
    CcLimbVec3 hands[CC_HUMANOID_ARM_COUNT] = {
        ToLimbVector(agent->climb_hand_left),
        ToLimbVector(agent->climb_hand_right)
    };
    CcLimbVec3 feet[CC_HUMANOID_LEG_COUNT] = {
        ToLimbVector(foot_targets[0]), ToLimbVector(foot_targets[1])
    };
    CcLimbVec3 normals[CC_HUMANOID_LEG_COUNT] = {
        ToLimbVector(foot_normals[0]), ToLimbVector(foot_normals[1])
    };
    CcLocalSceneKind climb_scene = AgentSceneForCall(agent, market_interior);
    const float hand_support[CC_HUMANOID_ARM_COUNT] = {
        LocalClimbContactSupport(climb_scene, agent->climb_hand_left, up,
                                 1.0f),
        LocalClimbContactSupport(climb_scene, agent->climb_hand_right, up,
                                 1.0f),
    };
    const float support[CC_HUMANOID_LEG_COUNT] = {
        LocalClimbContactSupport(climb_scene, foot_targets[0],
                                 foot_normals[0], 1.0f),
        LocalClimbContactSupport(climb_scene, foot_targets[1],
                                 foot_normals[1], 1.0f),
    };
    float standing_convergence = SmoothStep01(agent->climb_settle);
    float biomech_progress = amount < 0.78f ? amount :
        0.78f + 0.22f * standing_convergence;
    CcHumanoidGaitAdvanceClimb(
        &agent->humanoid, ToLimbVector(agent->position),
        agent->facing_yaw, hands, hand_support, feet, normals, support,
        biomech_progress, delta_time, ProbeLocalSurface, &context);
    if (ReleaseClimbToPhysicalFall(agent)) return;

    agent->traversal = CC_TRAVERSAL_DESCEND;
    if (agent->climb_settle >= 1.0f &&
        CcHumanoidGaitClimbReady(
            &agent->humanoid, ToLimbVector(agent->position),
            agent->facing_yaw, ProbeLocalSurface, &context, 0.025f)) {
        if (agent->climb_training_pending) {
            CcLocalAgentTrainAthleticism(agent, CC_ATHLETIC_GRIP, 14.0f);
            agent->climb_training_pending = false;
        }
        agent->climbing = false;
        agent->climbing_down = false;
        agent->vaulting = false;
        agent->grounded = true;
        agent->traversal = CC_TRAVERSAL_IDLE;
        CcHumanoidGaitFinishClimb(
            &agent->humanoid, ToLimbVector(agent->position),
            agent->facing_yaw, ProbeLocalSurface, &context);
    }
}

static Vector3 ClimbTopOutArc(Vector3 wall_contact, Vector3 top_contact,
                              Vector3 wall_normal, float amount)
{
    amount = fmaxf(0.0f, fminf(1.0f, amount));
    Vector3 lip_clearance = wall_contact;
    lip_clearance.y = top_contact.y + 0.13f;
    if (amount < 0.46f) {
        float rise = SmoothStep01(amount / 0.46f);
        Vector3 result = PhysicsLerp(wall_contact, lip_clearance, rise);
        result = PhysicsAdd(
            result, PhysicsScale(wall_normal, sinf(PI * rise) * 0.075f));
        return result;
    }
    float cross = SmoothStep01((amount - 0.46f) / 0.54f);
    Vector3 result = PhysicsLerp(lip_clearance, top_contact, cross);
    result.y += sinf(PI * cross) * 0.065f;
    return result;
}

static Vector3 ClimbWallStepArc(Vector3 takeoff, Vector3 wall_contact,
                                Vector3 wall_normal, Vector3 frame_right,
                                float side, float amount)
{
    amount = SmoothStep01(amount);
    Vector3 control = PhysicsLerp(takeoff, wall_contact, 0.50f);
    /* Clear the wall with the knee and shin, but do not throw the boot out to
       the side. A broad lateral arc reads as a kick from the isometric camera;
       the contact itself already supplies the alternating step width. */
    control = PhysicsAdd(control, PhysicsScale(wall_normal, 0.11f));
    control = PhysicsAdd(control, PhysicsScale(frame_right, side * 0.035f));
    control.y += 0.17f;
    float inverse = 1.0f - amount;
    Vector3 result = PhysicsScale(takeoff, inverse * inverse);
    result = PhysicsAdd(
        result, PhysicsScale(control, 2.0f * inverse * amount));
    return PhysicsAdd(result, PhysicsScale(wall_contact, amount * amount));
}

static float ClimbSwingSupport(float amount)
{
    if (amount <= 0.0f || amount >= 1.0f) return 1.0f;
    return 1.0f - sinf(amount * PI);
}

static void UpdateHighMantle(CcLocalAgent *agent, float delta_time,
                             bool market_interior)
{
    agent->climb_progress = fminf(1.0f, agent->climb_progress +
                                  delta_time / agent->climb_duration);
    float amount = agent->climb_progress;
    CcMotionMantleSample motion;
    if (!CcMotionClipSampleMantle(amount, &motion)) return;

    Vector3 frame_right = {cosf(agent->facing_yaw), 0.0f,
                           -sinf(agent->facing_yaw)};
    Vector3 previous = agent->position;
    Vector3 target = PhysicsLerp(agent->climb_start, agent->climb_end,
                                 motion.root_depth_progress);
    target.y = agent->climb_start.y +
        (agent->climb_end.y - agent->climb_start.y) * motion.root_progress;
    target = PhysicsAdd(target,
                        PhysicsScale(agent->climb_normal,
                                     motion.root_outward));
    target = PhysicsAdd(target,
                        PhysicsScale(frame_right, motion.root_lateral));
    target.y += motion.root_vertical;

    /* The authored root remains outside until the lead boot and both hands
       have established the support triangle. Collision is released only for
       the chest-over-lip portion of the montage. */
    if (amount < 0.90f) {
        float outside_distance = PhysicsDot(
            PhysicsSubtract(target, agent->climb_face),
            agent->climb_normal);
        float required_outside = agent->radius + 0.030f;
        if (outside_distance < required_outside) {
            float collision_weight = 1.0f - SmoothStep01(
                (amount - 0.78f) / 0.12f);
            target = PhysicsAdd(
                target, PhysicsScale(agent->climb_normal,
                                     (required_outside - outside_distance) *
                                         collision_weight));
        }
    }
    if (amount >= 1.0f) target = agent->climb_end;
    Vector3 resolved_target = target;
    Vector3 collision_normal = {0};
    (void)ResolveLocalAgentCapsuleMove(
        AgentSceneForCall(agent, market_interior), previous, target,
        agent->radius, amount >= 0.78f ? agent->climb_face.y : -FLT_MAX,
        &resolved_target, &collision_normal);
    target = resolved_target;
    agent->position = target;
    agent->velocity = delta_time > 0.00001f ?
        PhysicsScale(PhysicsSubtract(target, previous), 1.0f / delta_time) :
        (Vector3){0};
    agent->grounded = amount < 0.10f;
    agent->traversal = CC_TRAVERSAL_CLIMB;

    float end_distance = PhysicsLength(
        PhysicsSubtract(agent->position, agent->climb_end));
    if (amount >= 1.0f && end_distance < 0.020f) {
        agent->position = agent->climb_end;
        agent->velocity = (Vector3){0};
        agent->grounded = true;
        agent->climb_settle = fminf(1.0f, agent->climb_settle +
                                    delta_time / 0.42f);
    }

    LocalProbeContext context = {
        .scene = AgentSceneForCall(agent, market_interior)
    };
    CcLimbVec3 takeoff[CC_HUMANOID_LEG_COUNT] = {
        ToLimbVector(agent->climb_foot_left),
        ToLimbVector(agent->climb_foot_right)
    };
    CcHumanoidGaitAdvanceMantle(
        &agent->humanoid, ToLimbVector(agent->position),
        agent->facing_yaw, ToLimbVector(agent->climb_face),
        ToLimbVector(agent->climb_normal), takeoff, amount, delta_time,
        ProbeLocalSurface, &context);

    if (agent->climb_settle >= 1.0f &&
        CcHumanoidGaitClimbReady(
            &agent->humanoid, ToLimbVector(agent->position),
            agent->facing_yaw, ProbeLocalSurface, &context, 0.025f)) {
        if (agent->climb_training_pending) {
            CcLocalAgentTrainAthleticism(
                agent, CC_ATHLETIC_GRIP, 22.0f);
            agent->climb_training_pending = false;
        }
        agent->climbing = false;
        agent->grounded = true;
        agent->traversal = CC_TRAVERSAL_IDLE;
        CcHumanoidGaitFinishClimb(
            &agent->humanoid, ToLimbVector(agent->position),
            agent->facing_yaw, ProbeLocalSurface, &context);
    }
}

static void UpdateClimb(CcLocalAgent *agent, float delta_time,
                        bool market_interior)
{
    if (agent->climbing_down) {
        UpdateDownClimb(agent, delta_time, market_interior);
        return;
    }
    if (agent->morphology == CC_MORPHOLOGY_BIPED && !agent->vaulting) {
        UpdateHighMantle(agent, delta_time, market_interior);
        return;
    }
    Vector3 previous_position = agent->position;
    agent->climb_progress = fminf(1.0f, agent->climb_progress +
                                  delta_time / agent->climb_duration);
    float amount = agent->climb_progress;
    float acquire = SmoothStep01((amount - 0.10f) / 0.12f);
    float left_push = SmoothStep01((amount - 0.26f) / 0.18f);
    float right_push = SmoothStep01((amount - 0.46f) / 0.18f);
    float stand = SmoothStep01((amount - 0.86f) / 0.14f);
    Vector3 outside = PhysicsAdd(agent->climb_face,
                                 PhysicsScale(agent->climb_normal,
                                              agent->radius + 0.08f));
    Vector3 hang = outside;
    hang.y = fmaxf(agent->climb_start.y + 0.18f,
                   agent->climb_face.y - 1.04f);
    Vector3 middle = PhysicsLerp(hang, agent->climb_end, 0.16f);
    middle.y = agent->climb_face.y - 0.80f;
    Vector3 high = PhysicsLerp(hang, agent->climb_end, 0.38f);
    high.y = agent->climb_face.y - 0.54f;
    Vector3 target = agent->climb_start;
    if (amount < 0.10f) {
        agent->position = agent->climb_start;
        agent->velocity = (Vector3){0};
        agent->grounded = true;
    } else {
        target = PhysicsLerp(agent->climb_start, hang, acquire);
        target = PhysicsLerp(target, middle, left_push);
        target = PhysicsLerp(target, high, right_push);
        target = PhysicsLerp(target, agent->climb_end, stand);
        Vector3 acceleration = PhysicsSubtract(
            PhysicsScale(PhysicsSubtract(target, agent->position), 34.0f),
            PhysicsScale(agent->velocity, 9.0f));
        agent->velocity = PhysicsAdd(agent->velocity,
                                     PhysicsScale(acceleration, delta_time));
        float speed = PhysicsLength(agent->velocity);
        if (speed > 3.0f) agent->velocity = PhysicsScale(agent->velocity, 3.0f / speed);
        agent->position = PhysicsAdd(agent->position,
                                     PhysicsScale(agent->velocity, delta_time));
        agent->grounded = false;
    }

    if (amount >= 0.10f && amount < 0.82f) {
        Vector3 body = {agent->position.x,
                        agent->position.y + agent->limb_rig.morphology.body_height,
                        agent->position.z};
        Vector3 shoulders[2] = {
            FramePoint(body, -0.25f, 0.48f, 0.0f, agent->facing_yaw),
            FramePoint(body, 0.25f, 0.48f, 0.0f, agent->facing_yaw)
        };
        Vector3 hands[2] = {agent->climb_hand_left, agent->climb_hand_right};
        float desired_reach = 0.67f - right_push * 0.15f;
        Vector3 correction = {0};
        for (int32_t hand = 0; hand < 2; ++hand) {
            Vector3 toward = PhysicsSubtract(hands[hand], shoulders[hand]);
            float distance = PhysicsLength(toward);
            if (distance <= desired_reach || distance <= 0.0001f) continue;
            correction = PhysicsAdd(correction,
                                    PhysicsScale(toward,
                                                 (distance - desired_reach) /
                                                 distance * 0.32f));
        }
        agent->position = PhysicsAdd(agent->position, PhysicsScale(correction, 0.5f));
    }

    float outside_distance = PhysicsDot(
        PhysicsSubtract(agent->position, agent->climb_face), agent->climb_normal);
    float acquisition_clearance =
        (1.0f - SmoothStep01(amount / 0.28f)) * 0.135f;
    float required_outside = agent->radius + 0.035f + acquisition_clearance;
    if ((amount < 0.72f || agent->position.y < agent->climb_face.y - 0.12f) &&
        outside_distance < required_outside) {
        agent->position = PhysicsAdd(
            agent->position,
            PhysicsScale(agent->climb_normal, required_outside - outside_distance));
        float into_face = PhysicsDot(agent->velocity, agent->climb_normal);
        if (into_face < 0.0f) {
            agent->velocity = PhysicsSubtract(
                agent->velocity, PhysicsScale(agent->climb_normal, into_face));
        }
    }
    if (agent->position.y > agent->climb_end.y) {
        agent->position.y = agent->climb_end.y;
        if (agent->velocity.y > 0.0f) agent->velocity.y = 0.0f;
    }
    Vector3 resolved_position = agent->position;
    Vector3 collision_normal = {0};
    if (ResolveLocalAgentCapsuleMove(
            AgentSceneForCall(agent, market_interior), previous_position,
            agent->position, agent->radius,
            amount >= 0.82f ? agent->climb_face.y : -FLT_MAX,
            &resolved_position,
            &collision_normal)) {
        agent->position = resolved_position;
        float into_surface = PhysicsDot(agent->velocity, collision_normal);
        if (into_surface < 0.0f) {
            agent->velocity = PhysicsSubtract(
                agent->velocity,
                PhysicsScale(collision_normal, into_surface));
        }
    }
    agent->traversal = agent->vaulting ? CC_TRAVERSAL_VAULT :
                                         CC_TRAVERSAL_CLIMB;
    float end_distance = PhysicsLength(
        PhysicsSubtract(agent->position, agent->climb_end));
    if (amount >= 1.0f && end_distance < 0.020f) {
        agent->position = agent->climb_end;
        agent->velocity = (Vector3){0};
        agent->grounded = true;
        agent->climb_settle = fminf(1.0f, agent->climb_settle +
                                    delta_time / 0.70f);
    }
    LocalProbeContext context = {
        .scene = AgentSceneForCall(agent, market_interior)
    };
    if (agent->morphology == CC_MORPHOLOGY_BIPED) {
        Vector3 up = {0.0f, 1.0f, 0.0f};
        Vector3 frame_right = {cosf(agent->facing_yaw), 0.0f,
                               -sinf(agent->facing_yaw)};
        Vector3 wall_left = PhysicsAdd(
            PhysicsAdd(agent->climb_face,
                       PhysicsScale(agent->climb_normal, 0.018f)),
            PhysicsScale(frame_right, -0.20f));
        Vector3 wall_right = PhysicsAdd(
            PhysicsAdd(agent->climb_face,
                       PhysicsScale(agent->climb_normal, 0.018f)),
            PhysicsScale(frame_right, 0.18f));
        wall_left.y = fmaxf(agent->climb_start.y + 0.28f,
                            agent->climb_face.y - 0.70f);
        wall_right.y = fmaxf(agent->climb_start.y + 0.36f,
                             agent->climb_face.y - 0.52f);
        Vector3 top_left = PhysicsAdd(
            PhysicsAdd(agent->climb_face,
                       PhysicsScale(agent->climb_normal, -0.30f)),
            PhysicsScale(frame_right, -0.16f));
        Vector3 top_right = PhysicsAdd(
            PhysicsAdd(agent->climb_face,
                       PhysicsScale(agent->climb_normal, -0.26f)),
            PhysicsScale(frame_right, 0.14f));
        top_left.y = agent->climb_face.y + 0.035f;
        top_right.y = agent->climb_face.y + 0.035f;
        /* Each boot now has a real takeoff-to-contact swing. The quadratic
           wall steps move outward and sideways before planting, so the root
           rises from the opposite planted leg instead of towing both ankles
           vertically. */
        float left_wall_step = (amount - 0.08f) / 0.18f;
        float right_wall_step = (amount - 0.28f) / 0.18f;
        float left_top_step = (amount - 0.50f) / 0.18f;
        float right_top_step = (amount - 0.70f) / 0.20f;
        Vector3 left_wall_target = ClimbWallStepArc(
            agent->climb_foot_left, wall_left, agent->climb_normal,
            frame_right, -1.0f, left_wall_step);
        Vector3 right_wall_target = ClimbWallStepArc(
            agent->climb_foot_right, wall_right, agent->climb_normal,
            frame_right, 1.0f, right_wall_step);
        Vector3 left_target = ClimbTopOutArc(
            wall_left, top_left, agent->climb_normal, left_top_step);
        Vector3 right_target = ClimbTopOutArc(
            wall_right, top_right, agent->climb_normal, right_top_step);
        float left_to_top = SmoothStep01(
            (left_top_step - 0.42f) / 0.58f);
        float right_to_top = SmoothStep01(
            (right_top_step - 0.42f) / 0.58f);
        /* A boot leaves the ground sole-down, then turns onto the wall only
           as it arrives. Rotating both soles vertical at climb start made the
           lower legs look rigid even when their contacts were staggered. */
        float left_to_wall = SmoothStep01(
            (left_wall_step - 0.52f) / 0.48f);
        float right_to_wall = SmoothStep01(
            (right_wall_step - 0.52f) / 0.48f);
        Vector3 left_wall_normal = PhysicsNormalizeOr(
            PhysicsLerp(up, agent->climb_normal, left_to_wall), up);
        Vector3 right_wall_normal = PhysicsNormalizeOr(
            PhysicsLerp(up, agent->climb_normal, right_to_wall), up);
        Vector3 foot_targets[CC_HUMANOID_LEG_COUNT] = {
            left_top_step > 0.0f ? left_target : left_wall_target,
            right_top_step > 0.0f ? right_target : right_wall_target
        };
        Vector3 foot_normals[CC_HUMANOID_LEG_COUNT] = {
            PhysicsNormalizeOr(
                PhysicsLerp(left_wall_normal, up, left_to_top), up),
            PhysicsNormalizeOr(
                PhysicsLerp(right_wall_normal, up, right_to_top), up)
        };
        Vector3 top_hand_left = PhysicsAdd(
            agent->climb_hand_left,
            PhysicsScale(agent->climb_normal, -0.20f));
        Vector3 top_hand_right = PhysicsAdd(
            agent->climb_hand_right,
            PhysicsScale(agent->climb_normal, -0.20f));
        top_hand_left.y = agent->climb_face.y + 0.040f;
        top_hand_right.y = agent->climb_face.y + 0.040f;
        float left_hand_press = SmoothStep01((amount - 0.48f) / 0.20f);
        float right_hand_press = SmoothStep01((amount - 0.60f) / 0.18f);
        Vector3 hand_points[CC_HUMANOID_ARM_COUNT] = {
            PhysicsLerp(agent->climb_hand_left,
                        top_hand_left, left_hand_press),
            PhysicsLerp(agent->climb_hand_right,
                        top_hand_right, right_hand_press),
        };
        CcLimbVec3 hand_targets[CC_HUMANOID_ARM_COUNT] = {
            ToLimbVector(hand_points[0]), ToLimbVector(hand_points[1])
        };
        const float hand_support[CC_HUMANOID_ARM_COUNT] = {
            LocalClimbContactSupport(
                context.scene, hand_points[0],
                PhysicsNormalizeOr(
                    PhysicsLerp(agent->climb_normal, up, left_hand_press),
                    up), 1.0f),
            LocalClimbContactSupport(
                context.scene, hand_points[1],
                PhysicsNormalizeOr(
                    PhysicsLerp(agent->climb_normal, up, right_hand_press),
                    up), 1.0f),
        };
        CcLimbVec3 feet[CC_HUMANOID_LEG_COUNT] = {
            ToLimbVector(foot_targets[0]), ToLimbVector(foot_targets[1])
        };
        CcLimbVec3 normals[CC_HUMANOID_LEG_COUNT] = {
            ToLimbVector(foot_normals[0]), ToLimbVector(foot_normals[1])
        };
        float support[CC_HUMANOID_LEG_COUNT] = {
            LocalClimbContactSupport(
                context.scene, foot_targets[0], foot_normals[0],
                fminf(ClimbSwingSupport(left_wall_step),
                      ClimbSwingSupport(left_top_step))),
            LocalClimbContactSupport(
                context.scene, foot_targets[1], foot_normals[1],
                fminf(ClimbSwingSupport(right_wall_step),
                      ClimbSwingSupport(right_top_step)))
        };
        float standing_convergence = SmoothStep01(agent->climb_settle);
        float biomech_progress = amount < 0.78f ? amount :
            0.78f + 0.22f * standing_convergence;
        CcHumanoidGaitAdvanceClimb(
            &agent->humanoid, ToLimbVector(agent->position),
            agent->facing_yaw, hand_targets, hand_support,
            feet, normals, support,
            biomech_progress, delta_time, ProbeLocalSurface, &context);
        if (ReleaseClimbToPhysicalFall(agent)) return;
        if (agent->climb_settle >= 1.0f &&
            CcHumanoidGaitClimbReady(
                &agent->humanoid, ToLimbVector(agent->position),
                agent->facing_yaw, ProbeLocalSurface, &context, 0.025f)) {
            if (agent->climb_training_pending) {
                CcLocalAgentTrainAthleticism(
                    agent, CC_ATHLETIC_GRIP,
                    agent->vaulting ? 12.0f : 22.0f);
                agent->climb_training_pending = false;
            }
            agent->climbing = false;
            agent->grounded = true;
            agent->traversal = CC_TRAVERSAL_IDLE;
        }
        if (!agent->climbing) {
            agent->vaulting = false;
            CcHumanoidGaitFinishClimb(
                &agent->humanoid, ToLimbVector(agent->position),
                agent->facing_yaw, ProbeLocalSurface, &context);
        }
        return;
    }
    if (agent->climb_settle >= 1.0f) {
        if (agent->climb_training_pending) {
            CcLocalAgentTrainAthleticism(
                agent, CC_ATHLETIC_GRIP,
                agent->vaulting ? 12.0f : 22.0f);
            agent->climb_training_pending = false;
        }
        agent->climbing = false;
        agent->vaulting = false;
        agent->grounded = true;
        agent->traversal = CC_TRAVERSAL_IDLE;
    }
    CcLimbRigUpdate(&agent->limb_rig, ToLimbVector(ShellPodBaseCenter(agent)),
                    agent->facing_yaw, ToLimbVector(agent->velocity),
                    agent->grounded, delta_time, ProbeLocalSurface, &context);
    if (agent->climbing && amount >= 0.18f) {
        Vector3 frame_right = {cosf(agent->facing_yaw), 0.0f,
                               -sinf(agent->facing_yaw)};
        float wall_height = agent->climb_start.y + 0.16f +
                            left_push * 0.22f + right_push * 0.18f;
        Vector3 wall_left = PhysicsAdd(
            PhysicsAdd(agent->climb_face, PhysicsScale(agent->climb_normal, 0.018f)),
            PhysicsScale(frame_right, -0.13f));
        Vector3 wall_right = PhysicsAdd(
            PhysicsAdd(agent->climb_face, PhysicsScale(agent->climb_normal, 0.018f)),
            PhysicsScale(frame_right, 0.13f));
        wall_left.y = wall_height;
        wall_right.y = wall_height + 0.07f;
        Vector3 top_left = PhysicsAdd(
            PhysicsAdd(agent->climb_face, PhysicsScale(agent->climb_normal, -0.20f)),
            PhysicsScale(frame_right, -0.13f));
        Vector3 top_right = PhysicsAdd(
            PhysicsAdd(agent->climb_face, PhysicsScale(agent->climb_normal, -0.20f)),
            PhysicsScale(frame_right, 0.13f));
        top_left.y = agent->climb_face.y + 0.035f;
        top_right.y = agent->climb_face.y + 0.035f;
        float plant = SmoothStep01((amount - 0.18f) / 0.16f);
        float left_transfer = SmoothStep01((amount - 0.56f) / 0.18f);
        float right_transfer = SmoothStep01((amount - 0.70f) / 0.18f);
        Vector3 left_contact = PhysicsLerp(wall_left, top_left, left_transfer);
        Vector3 right_contact = PhysicsLerp(wall_right, top_right,
                                            right_transfer);
        Vector3 up = {0.0f, 1.0f, 0.0f};
        Vector3 left_normal = PhysicsNormalizeOr(
            PhysicsLerp(agent->climb_normal, up, left_transfer), up);
        Vector3 right_normal = PhysicsNormalizeOr(
            PhysicsLerp(agent->climb_normal, up, right_transfer), up);
        const CcLimbSpec *left_spec = &agent->limb_rig.morphology.limbs[0];
        const CcLimbSpec *right_spec = &agent->limb_rig.morphology.limbs[1];
        Vector3 current_left = FromLimbVector(
            agent->limb_rig.limbs[0].joints[left_spec->segment_count]);
        Vector3 current_right = FromLimbVector(
            agent->limb_rig.limbs[1].joints[right_spec->segment_count]);
        left_contact = PhysicsLerp(current_left, left_contact, plant);
        right_contact = PhysicsLerp(current_right, right_contact, plant);
        left_normal = PhysicsNormalizeOr(
            PhysicsLerp(FromLimbVector(agent->limb_rig.limbs[0].contact_normal),
                        left_normal, plant), up);
        right_normal = PhysicsNormalizeOr(
            PhysicsLerp(FromLimbVector(agent->limb_rig.limbs[1].contact_normal),
                        right_normal, plant), up);
        CcLimbVec3 root = ToLimbVector(ShellPodBaseCenter(agent));
        CcLimbRigPinContact(&agent->limb_rig, 0, root, agent->facing_yaw,
                            ToLimbVector(left_contact), ToLimbVector(left_normal));
        CcLimbRigPinContact(&agent->limb_rig, 1, root, agent->facing_yaw,
                            ToLimbVector(right_contact), ToLimbVector(right_normal));
    }
}

static bool TryHorizontalAxis(CcLocalAgent *agent, bool market_interior,
                              bool move_x, float amount)
{
    if (agent->climbing) return false;
    if (fabsf(amount) <= 0.000001f) return true;
    float candidate_x = agent->position.x + (move_x ? amount : 0.0f);
    float candidate_z = agent->position.z + (move_x ? 0.0f : amount);
    CcLocalSceneKind scene = AgentSceneForCall(agent, market_interior);
    if (scene == CC_LOCAL_SCENE_STREET) {
        const NavPlatform *support = SupportingPlatformAt(
            agent->position.x, agent->position.z, agent->position.y);
        float candidate_surface = BodySurfaceHeightAt(
            scene, candidate_x, candidate_z);
        float current_surface = BodySurfaceHeightAt(
            scene, agent->position.x, agent->position.z);
        float horizontal = fabsf(amount);
        Vector3 candidate_normal = SurfaceNormalAt(
            scene, candidate_x, candidate_z);
        if (agent->grounded && horizontal > 0.0001f &&
            candidate_surface > current_surface + 0.002f &&
            candidate_normal.y < 0.60f) {
            return false;
        }
        bool moving_toward_target = agent->exact_target_valid &&
            amount * ((move_x ? agent->target_point.x : agent->target_point.z) -
                      (move_x ? agent->position.x : agent->position.z)) > 0.0f;
        bool target_requests_descent = agent->exact_target_valid &&
            agent->target_point.y < agent->position.y - 0.24f;
        if (support != NULL && agent->grounded && moving_toward_target &&
            target_requests_descent &&
            candidate_surface < agent->position.y - 0.24f &&
            BeginDownClimb(agent, support, move_x, amount)) {
            return false;
        }
        const NavPlatform *platform = ClimbPlatformAt(scene,
                                                      candidate_x, candidate_z,
                                                      agent->radius,
                                                      agent->position.y);
        if (platform != NULL) {
            bool target_requests_climb = agent->exact_target_valid &&
                agent->target_point.y >= PlatformTopHeight(platform) - 0.10f;
            if (agent->grounded && target_requests_climb) {
                if (BeginClimb(agent, platform)) return false;
            }
            if (agent->grounded) {
                Rectangle footprint = {platform->x, platform->z,
                                       platform->width, platform->depth};
                float current_distance = FootprintDistanceSquared(
                    agent->position.x, agent->position.z, footprint);
                float candidate_distance = FootprintDistanceSquared(
                    candidate_x, candidate_z, footprint);
                if (candidate_distance <= current_distance + 0.000001f) {
                    return false;
                }
            }
        }
    }
    Vector3 candidate = agent->position;
    candidate.x = candidate_x;
    candidate.z = candidate_z;
    if (StaticBodyBlocked(scene, candidate_x, candidate_z, agent->radius) ||
        LocalAgentCapsuleBlocked(scene, agent->position, candidate,
                                 agent->radius) ||
        CcLocalAgentPointSpaceBlockedInternal(agent, candidate)) {
        return false;
    }
    if (move_x) agent->position.x = candidate_x;
    else agent->position.z = candidate_z;
    return true;
}

static bool UpdateCombatClock(CcLocalAgent *agent, float delta_time)
{
    CcCombatState *combat = &agent->combat;
    combat->hit_flash_seconds = fmaxf(0.0f,
                                      combat->hit_flash_seconds - delta_time);
    if (combat->hitstop_seconds > 0.0f && agent->grounded &&
        !agent->climbing && !agent->swimming) {
        combat->hitstop_seconds = fmaxf(0.0f,
                                        combat->hitstop_seconds - delta_time);
        return true;
    }
    combat->hitstop_seconds = fmaxf(0.0f,
                                    combat->hitstop_seconds - delta_time);
    combat->stagger_seconds = fmaxf(0.0f,
                                    combat->stagger_seconds - delta_time);
    if (combat->life_state == CC_LIFE_DEAD &&
        combat->team == CC_COMBAT_PLAYER) {
        combat->respawn_seconds = fmaxf(0.0f,
                                        combat->respawn_seconds - delta_time);
        if (combat->respawn_seconds <= 0.0f) {
            combat->life_state = CC_LIFE_RESPAWNING;
            CcHumanoidGaitBeginResurrection(&agent->humanoid);
        }
    }
    if (combat->life_state == CC_LIFE_RESPAWNING &&
        !agent->humanoid.ragdoll.active && !agent->humanoid.recovering) {
        combat->life_state = CC_LIFE_ALIVE;
        combat->health = 45.0f;
        combat->posture = 72.0f;
        combat->stagger_seconds = 0.42f;
        combat->hit_flash_seconds = 0.22f;
        combat->weapon_mode = combat->team == CC_COMBAT_PLAYER ||
                              combat->team == CC_COMBAT_RAIDER ?
                              CC_WEAPON_HELD : CC_WEAPON_NONE;
    }
    if (CombatCanAct(combat) && combat->stagger_seconds <= 0.0f) {
        float regeneration = agent->humanoid.action ==
                             CC_HUMANOID_ACTION_GUARD ? 4.0f : 15.0f;
        combat->posture = fminf(CC_LOCAL_COMBAT_MAX_POSTURE,
                                combat->posture + regeneration * delta_time);
    }
    return false;
}

static void SyncPhysicalLifeState(CcLocalAgent *agent)
{
    CcCombatState *combat = &agent->combat;
    if (combat->life_state == CC_LIFE_ALIVE &&
        agent->humanoid.ragdoll.active) {
        combat->life_state = CC_LIFE_KNOCKED_DOWN;
        if (combat->weapon_mode == CC_WEAPON_HELD) {
            combat->weapon_mode = CC_WEAPON_RAGDOLL_ATTACHED;
        }
    } else if (combat->life_state == CC_LIFE_KNOCKED_DOWN &&
               !agent->humanoid.ragdoll.active) {
        combat->life_state = CC_LIFE_ALIVE;
        combat->weapon_mode = combat->team == CC_COMBAT_PLAYER ||
                              combat->team == CC_COMBAT_RAIDER ?
                              CC_WEAPON_HELD : CC_WEAPON_NONE;
    }
}

static void ApplyRagdollWaterResponse(CcHumanoidGait *gait,
                                      float water_surface, float immersion,
                                      float delta_time)
{
    if (gait == NULL || !gait->ragdoll.active ||
        !isfinite(delta_time) || delta_time <= 0.0f) return;
    float water_amount = fmaxf(0.0f, fminf(immersion, 1.0f));
    for (int32_t index = 0; index < gait->ragdoll.particle_count; ++index) {
        CcBiomechRagdollParticle *particle = &gait->ragdoll.particles[index];
        if (particle->inverse_mass <= 0.0f) continue;
        float diameter = fmaxf(particle->radius * 2.0f, 0.08f);
        float submerged = fmaxf(0.0f, fminf(
            (water_surface + particle->radius - particle->position.y) /
                diameter,
            1.0f)) * water_amount;
        if (submerged <= 0.0f) continue;

        /* A fully submerged particle receives twice gravity, so the whole
           body settles near the surface instead of resting on the pool bed.
           The clamped submersion and exponential damping keep both the force
           and the retained Verlet velocity bounded. */
        particle->acceleration.y += 19.62f * submerged;
        float retained_velocity = expf(-4.8f * submerged * delta_time);
        CcBiomechVec3 velocity = {
            particle->position.x - particle->previous_position.x,
            particle->position.y - particle->previous_position.y,
            particle->position.z - particle->previous_position.z,
        };
        particle->previous_position = (CcBiomechVec3){
            particle->position.x - velocity.x * retained_velocity,
            particle->position.y - velocity.y * retained_velocity,
            particle->position.z - velocity.z * retained_velocity,
        };
    }
}

void CcLocalAgentFixedStepInternal(CcLocalAgent *agent, float delta_time,
                                   bool market_interior)
{
    const float gravity = 9.81f;
    delta_time = fminf(delta_time, 1.0f / 30.0f);
    agent->previous_cape = agent->cape;
    if (UpdateCombatClock(agent, delta_time)) {
        UpdateHeroCape(agent, delta_time);
        return;
    }
    if (agent->climbing) {
        UpdateClimb(agent, delta_time, market_interior);
        UpdateHeroCape(agent, delta_time);
        return;
    }
    CcLocalSceneKind scene = AgentSceneForCall(agent, market_interior);
    agent->scene = scene;
    UpdateStreetPortalProximity(agent);
    bool biped = agent->morphology == CC_MORPHOLOGY_BIPED;
    LocalProbeContext context = {.scene = scene};
    if (biped && agent->humanoid_needs_reset) {
        CcHumanoidGaitInit(&agent->humanoid, ToLimbVector(agent->position),
                            agent->facing_yaw, ProbeLocalSurface, &context);
        ApplyAgentWalkingProfile(agent);
        agent->humanoid_needs_reset = false;
    }
    bool was_swimming = agent->swimming;
    bool in_water = biped &&
                    CourseWaterContains(scene, agent->position.x,
                                        agent->position.z);
    if (was_swimming && !in_water) {
        float surface = BodySurfaceHeightAt(scene, agent->position.x,
                                            agent->position.z);
        agent->position.y = surface;
        agent->velocity.y = 0.0f;
        agent->grounded = true;
        CcHumanoidGaitEndSwim(&agent->humanoid,
                              ToLimbVector(agent->position),
                              agent->facing_yaw, ProbeLocalSurface, &context);
    }
    agent->swimming = in_water;
    agent->immersion = Approach(agent->immersion, in_water ? 1.0f : 0.0f,
                                delta_time * 2.8f);
    float traction = agent->limb_rig.initialized ? agent->limb_rig.traction : 1.0f;
    float base_speed = in_water ? 0.76f : biped ? 1.45f : 2.35f;
    if (biped) {
        base_speed *= AthleticBonus(agent, CC_ATHLETIC_MOBILITY, 0.045f);
    }
    float maximum_speed = biped ? base_speed :
                          base_speed * (0.68f + traction * 0.32f);
    if (biped && agent->humanoid.action == CC_HUMANOID_ACTION_GUARD) {
        maximum_speed *= 0.54f;
    } else if (biped &&
               agent->humanoid.action == CC_HUMANOID_ACTION_STRIKE) {
        maximum_speed *= 0.16f;
    }
    if (!CombatCanAct(&agent->combat) ||
        agent->combat.stagger_seconds > 0.0f) {
        maximum_speed = 0.0f;
    }
    float acceleration = 10.5f * traction *
        AthleticBonus(agent, CC_ATHLETIC_MOBILITY, 0.035f);
    Vector3 direction = {0};
    float target_distance = 0.0f;
    if (agent->exact_target_valid) {
        direction.x = agent->target_point.x - agent->position.x;
        direction.z = agent->target_point.z - agent->position.z;
        target_distance = sqrtf(direction.x * direction.x + direction.z * direction.z);
        if (agent->navigation_active && target_distance < 0.35f &&
            !agent->climbing && !agent->humanoid.ragdoll.active) {
            (void)AdvanceAgentNavigation(agent);
            if (agent->exact_target_valid) {
                direction.x = agent->target_point.x - agent->position.x;
                direction.z = agent->target_point.z - agent->position.z;
                target_distance = sqrtf(direction.x * direction.x +
                                        direction.z * direction.z);
            } else {
                direction = (Vector3){0};
                target_distance = 0.0f;
            }
        }
        float physical_speed = sqrtf(agent->velocity.x * agent->velocity.x +
                                     agent->velocity.z * agent->velocity.z);
        bool gait_settled = !biped ||
                            (!agent->humanoid.ragdoll.active &&
                             agent->humanoid.speed.value < 0.03f &&
                             physical_speed < 0.02f);
        if (target_distance < 0.025f && gait_settled &&
            fabsf(agent->position.y - agent->target_point.y) < 0.12f) {
            agent->exact_target_valid = false;
            agent->command_point_valid = false;
            target_distance = 0.0f;
        }
    }
    float desired_x = 0.0f;
    float desired_z = 0.0f;
    if (target_distance > 0.001f) {
        float desired_speed = fminf(maximum_speed, target_distance * 3.2f);
        desired_x = direction.x / target_distance * desired_speed;
        desired_z = direction.z / target_distance * desired_speed;
    }
    if (biped && agent->humanoid.action == CC_HUMANOID_ACTION_STRIKE) {
        /* Weapon reach owns the strike distance. Root-motion lunges from both
           combatants used to collapse the silhouettes into one another. */
        desired_x = 0.0f;
        desired_z = 0.0f;
    } else if (biped && agent->combat.focus_valid &&
               !agent->humanoid.ragdoll.active) {
        float away_x = agent->position.x - agent->combat.focus_point.x;
        float away_z = agent->position.z - agent->combat.focus_point.z;
        float focus_distance = sqrtf(away_x * away_x + away_z * away_z);
        if (focus_distance > 0.0001f &&
            focus_distance < COMBAT_PERSONAL_SPACE) {
            float retreat_speed = fminf(
                maximum_speed,
                0.34f + (COMBAT_PERSONAL_SPACE - focus_distance) * 2.4f);
            desired_x = away_x / focus_distance * retreat_speed;
            desired_z = away_z / focus_distance * retreat_speed;
        }
    }
    if (CombatCanAct(&agent->combat) && agent->grounded &&
        !agent->climbing && !agent->swimming) {
        /* Course-level reciprocal avoidance keeps crowds and fights readable.
           It moves the root only enough to restore body spacing and never
           becomes part of weapon timing or authored hand motion. */
        desired_x += agent->separation_velocity.x;
        desired_z += agent->separation_velocity.z;
    }
    if (!CombatCanAct(&agent->combat)) {
        desired_x = 0.0f;
        desired_z = 0.0f;
    } else if (agent->combat.stagger_seconds > 0.0f) {
        desired_x = agent->combat.knockback_velocity.x;
        desired_z = agent->combat.knockback_velocity.z;
    } else {
        desired_x += agent->combat.knockback_velocity.x;
        desired_z += agent->combat.knockback_velocity.z;
    }
    if (!(biped && agent->humanoid.ragdoll.active)) {
        float face_x = direction.x;
        float face_z = direction.z;
        bool should_face = target_distance > 0.001f;
        if (agent->combat.focus_valid) {
            face_x = agent->combat.focus_point.x - agent->position.x;
            face_z = agent->combat.focus_point.z - agent->position.z;
            should_face = face_x * face_x + face_z * face_z > 0.0001f;
        }
        if (should_face) {
            float target_yaw = atan2f(face_x, face_z);
            float difference = WrapAngle(target_yaw - agent->facing_yaw);
            float turn_response = 7.55f +
                (float)(AthleticLevel(agent, CC_ATHLETIC_MOBILITY) - 1) *
                    0.55f;
            agent->facing_yaw = WrapAngle(
                agent->facing_yaw +
                difference * fminf(1.0f, delta_time * turn_response));
        }
    }
    bool ragdoll_was_active = biped && agent->humanoid.ragdoll.active;
    if (biped && in_water && !agent->humanoid.ragdoll.active) {
        CcHumanoidGaitAdvanceSwim(
            &agent->humanoid, ToLimbVector(agent->position),
            agent->facing_yaw,
            (CcLimbVec3){desired_x, 0.0f, desired_z},
            COURSE_WATER_SURFACE, agent->immersion, delta_time);
        agent->velocity.x = agent->humanoid.root_velocity.x;
        agent->velocity.z = agent->humanoid.root_velocity.z;
    } else if (biped) {
        if (in_water && agent->humanoid.ragdoll.active) {
            ApplyRagdollWaterResponse(&agent->humanoid,
                                      COURSE_WATER_SURFACE,
                                      agent->immersion, delta_time);
        }
        CcHumanoidGaitAdvancePhysical(
            &agent->humanoid, ToLimbVector(agent->position),
            agent->facing_yaw,
            (CcLimbVec3){desired_x, 0.0f, desired_z},
            agent->grounded, delta_time, ProbeLocalSurface,
            ProbeLocalCollision, &context);
        agent->velocity.x = agent->humanoid.root_velocity.x;
        agent->velocity.z = agent->humanoid.root_velocity.z;
    } else {
        agent->velocity.x = Approach(agent->velocity.x, desired_x,
                                     acceleration * delta_time);
        agent->velocity.z = Approach(agent->velocity.z, desired_z,
                                     acceleration * delta_time);
        float support_weight = fminf(1.0f, target_distance / 0.45f);
        agent->velocity.x += agent->limb_rig.body_acceleration.x * delta_time *
                             support_weight;
        agent->velocity.z += agent->limb_rig.body_acceleration.z * delta_time *
                             support_weight;
    }

    bool landed = false;
    if (biped && (ragdoll_was_active || agent->humanoid.ragdoll.active)) {
        CcLimbVec3 physical_position = CcHumanoidGaitAuthoritativePosition(
            &agent->humanoid, ToLimbVector(agent->position));
        CcLimbVec3 physical_velocity = CcHumanoidGaitAuthoritativeVelocity(
            &agent->humanoid, ToLimbVector(agent->velocity));
        agent->position = FromLimbVector(physical_position);
        agent->velocity = FromLimbVector(physical_velocity);
        agent->grounded = !agent->humanoid.ragdoll.active ||
                          agent->humanoid.support_state ==
                              CC_HUMANOID_SUPPORT_STABLE;
        agent->swimming = false;
        float knockback_decay = expf(-7.5f * delta_time);
        agent->combat.knockback_velocity.x *= knockback_decay;
        agent->combat.knockback_velocity.z *= knockback_decay;
        goto local_body_resolved;
    }

    Vector3 previous_position = agent->position;
    bool moved_x = TryHorizontalAxis(agent, market_interior, true,
                                     agent->velocity.x * delta_time);
    bool moved_z = TryHorizontalAxis(agent, market_interior, false,
                                     agent->velocity.z * delta_time);
    if (!moved_x) agent->velocity.x = 0.0f;
    if (!moved_z) agent->velocity.z = 0.0f;
    if (agent->climbing) {
        UpdateClimb(agent, delta_time, market_interior);
        UpdateHeroCape(agent, delta_time);
        return;
    }
    float main_progress = 0.0f;
    if (target_distance > 0.001f) {
        main_progress =
            ((agent->position.x - previous_position.x) * direction.x +
             (agent->position.z - previous_position.z) * direction.z) /
            target_distance;
    }
    bool forward_stalled = main_progress <
        fmaxf(0.0010f, maximum_speed * delta_time * 0.12f);
    if (target_distance > 0.001f && forward_stalled &&
        (!moved_x || !moved_z)) {
        Vector3 forward = {direction.x / target_distance, 0.0f,
                           direction.z / target_distance};
        Vector3 side = {-forward.z, 0.0f, forward.x};
        float sidestep = maximum_speed * 0.72f * delta_time;
        bool side_x = TryHorizontalAxis(agent, market_interior, true,
                                        side.x * sidestep);
        bool side_z = TryHorizontalAxis(agent, market_interior, false,
                                        side.z * sidestep);
        if ((!side_x || !side_z) && !agent->climbing) {
            if (side_x) agent->position.x -= side.x * sidestep;
            if (side_z) agent->position.z -= side.z * sidestep;
            (void)TryHorizontalAxis(agent, market_interior, true,
                                    -side.x * sidestep);
            (void)TryHorizontalAxis(agent, market_interior, false,
                                    -side.z * sidestep);
        }
        if (agent->climbing) {
            UpdateClimb(agent, delta_time, market_interior);
            UpdateHeroCape(agent, delta_time);
            return;
        }
    }
    agent->velocity.x = (agent->position.x - previous_position.x) / delta_time;
    agent->velocity.z = (agent->position.z - previous_position.z) / delta_time;
    float training_step = sqrtf(
        (agent->position.x - previous_position.x) *
            (agent->position.x - previous_position.x) +
        (agent->position.z - previous_position.z) *
            (agent->position.z - previous_position.z));
    bool requested_progress = agent->crowned && agent->exact_target_valid &&
        target_distance > 0.18f && maximum_speed > 0.01f;
    if (requested_progress && training_step < 0.0015f) {
        agent->movement_stall_seconds += delta_time;
    } else {
        agent->movement_stall_seconds = 0.0f;
    }
    if (agent->movement_stall_seconds > 0.42f) {
        bool click_path = agent->scene == CC_LOCAL_SCENE_STREET &&
            agent->navigation_active &&
            agent->navigation_destination_room ==
                STREET_CLICK_NAVIGATION_DESTINATION &&
            agent->command_point_valid;
        Vector3 retry_target = agent->command_point;
        int32_t retry_count = agent->navigation_repath_count;
        if (click_path && retry_count < 1 &&
            SetStreetClickTarget(agent, retry_target)) {
            /* Moving actors and edge contacts can invalidate one corridor.
               MMO path followers request a fresh corridor instead of
               abandoning the command on the first stall. One retry avoids
               hiding a genuinely bad destination behind an endless loop. */
            agent->navigation_repath_count = retry_count + 1;
            agent->movement_stall_seconds = 0.0f;
            goto local_navigation_recovered;
        }
        /* A target that remains unreachable after projection and one replan
           is a real failure. Stop the walk cycle cleanly. */
        agent->exact_target_valid = false;
        agent->target_valid = false;
        agent->command_point_valid = false;
        ClearAgentNavigation(agent);
        agent->movement_stall_seconds = 0.0f;
        agent->velocity.x = 0.0f;
        agent->velocity.z = 0.0f;
        if (biped && !agent->humanoid.ragdoll.active) {
            CcHumanoidGaitConstrainMotion(
                &agent->humanoid, ToLimbVector(agent->position),
                ToLimbVector(agent->velocity), agent->grounded);
        }
    }
local_navigation_recovered:
    if (agent->grounded && !agent->climbing && training_step < 0.20f) {
        agent->athletics.travel_training_distance += training_step;
        while (agent->athletics.travel_training_distance >= 12.0f) {
            agent->athletics.travel_training_distance -= 12.0f;
            CcLocalAgentTrainAthleticism(agent, CC_ATHLETIC_MOBILITY, 5.0f);
        }
    }
    float knockback_decay = expf(-7.5f * delta_time);
    agent->combat.knockback_velocity.x *= knockback_decay;
    agent->combat.knockback_velocity.z *= knockback_decay;

    bool occupies_water = biped &&
                          CourseWaterContains(scene, agent->position.x,
                                              agent->position.z);
    if (occupies_water) {
        agent->swimming = true;
        agent->immersion = Approach(agent->immersion, 1.0f,
                                    delta_time * 2.8f);
        if (!in_water) {
            CcHumanoidGaitAdvanceSwim(
                &agent->humanoid, ToLimbVector(agent->position),
                agent->facing_yaw,
                (CcLimbVec3){agent->velocity.x, 0.0f, agent->velocity.z},
                COURSE_WATER_SURFACE, agent->immersion, delta_time);
            agent->velocity.x = agent->humanoid.root_velocity.x;
            agent->velocity.z = agent->humanoid.root_velocity.z;
        }
        float float_base = COURSE_WATER_SURFACE - 0.82f;
        agent->position.y = Approach(agent->position.y, float_base,
                                     delta_time * 1.8f);
        agent->velocity.y = Approach(agent->velocity.y, 0.0f,
                                     delta_time * 4.0f);
        agent->grounded = false;
    } else {
        if (agent->swimming) {
            agent->swimming = false;
            CcHumanoidGaitEndSwim(&agent->humanoid,
                                  ToLimbVector(agent->position),
                                  agent->facing_yaw, ProbeLocalSurface,
                                  &context);
        }
        bool followed_ground = agent->grounded;
        agent->velocity.y -= gravity * delta_time;
        agent->position.y += agent->velocity.y * delta_time;
        float surface = BodySurfaceHeightAt(scene,
                                            agent->position.x,
                                            agent->position.z);
        if (agent->position.y <= surface && agent->velocity.y <= 0.0f) {
            landed = !agent->grounded;
            agent->position.y = surface;
            agent->velocity.y = 0.0f;
            agent->grounded = true;
        } else if (followed_ground && agent->velocity.y <= 0.0f &&
                   agent->position.y - surface <= 0.12f) {
            /* A walking root follows ordinary downhill grade. Treat only a
               real ledge as airborne; otherwise the ragdoll system sees each
               centimetre of descending terrain as a fall. */
            agent->position.y = surface;
            agent->velocity.y = 0.0f;
            agent->grounded = true;
        } else {
            agent->grounded = false;
        }
    }
local_body_resolved:
    if (biped && !agent->swimming) {
        CcHumanoidGaitConstrainMotion(&agent->humanoid,
                                      ToLimbVector(agent->position),
                                      ToLimbVector(agent->velocity),
                                      agent->grounded);
        agent->support_state = agent->humanoid.support_state;
        agent->grounded = agent->support_state == CC_HUMANOID_SUPPORT_STABLE ||
                          agent->support_state == CC_HUMANOID_SUPPORT_MARGINAL ||
                          agent->support_state == CC_HUMANOID_SUPPORT_HANDS;
        SyncPhysicalLifeState(agent);
    } else if (agent->swimming) {
        agent->support_state = CC_HUMANOID_SUPPORT_MARGINAL;
    }

    float horizontal_speed = sqrtf(agent->velocity.x * agent->velocity.x +
                                   agent->velocity.z * agent->velocity.z);
    if (agent->swimming) {
        agent->traversal = CC_TRAVERSAL_SWIM;
    } else if (biped && agent->humanoid.ragdoll.active) {
        agent->traversal = agent->humanoid.recovering ?
                           CC_TRAVERSAL_GET_UP : CC_TRAVERSAL_RAGDOLL;
    } else if (biped && agent->humanoid.action == CC_HUMANOID_ACTION_JUMP) {
        agent->traversal = CC_TRAVERSAL_JUMP;
    } else if (agent->grounded) {
        agent->traversal = horizontal_speed > 0.08f ? CC_TRAVERSAL_WALK :
                                                     CC_TRAVERSAL_IDLE;
    } else {
        agent->traversal = CC_TRAVERSAL_DROP;
    }
    if (landed && horizontal_speed < 0.08f &&
        !(biped && agent->humanoid.ragdoll.active)) {
        agent->traversal = CC_TRAVERSAL_IDLE;
    }
    if (landed && agent->jump_training_pending &&
        !(biped && agent->humanoid.ragdoll.active)) {
        CcLocalAgentTrainAthleticism(agent, CC_ATHLETIC_MOBILITY, 12.0f);
        agent->jump_training_pending = false;
    }
    if (biped) {
        CcHumanoidGaitResolvePose(&agent->humanoid,
                                  ToLimbVector(agent->position),
                                  agent->facing_yaw);
        float ragdoll_target = agent->humanoid.ragdoll.active ? 1.0f : 0.0f;
        agent->ragdoll_visual_blend = Approach(
            agent->ragdoll_visual_blend, ragdoll_target,
            delta_time * 4.5f);
        UpdateHeroCape(agent, delta_time);
    } else {
        CcLimbRigUpdate(&agent->limb_rig, ToLimbVector(ShellPodBaseCenter(agent)),
                        agent->facing_yaw, ToLimbVector(agent->velocity),
                        agent->grounded, delta_time, ProbeLocalSurface, &context);
        agent->cape = (CcLocalCapeState){0};
        agent->previous_cape = (CcLocalCapeState){0};
        agent->render_cape = (CcLocalCapeState){0};
    }
}

static CcLimbVec3 BlendLimbPoint(CcLimbVec3 before, CcLimbVec3 after,
                                 float amount)
{
    return (CcLimbVec3){
        before.x + (after.x - before.x) * amount,
        before.y + (after.y - before.y) * amount,
        before.z + (after.z - before.z) * amount,
    };
}

static float BlendPoseAngle(float before, float after, float amount)
{
    return WrapAngle(before + WrapAngle(after - before) * amount);
}

static void BlendHumanoidPose(CcHumanoidPose *result,
                              const CcHumanoidPose *before,
                              const CcHumanoidPose *after, float amount)
{
#define BLEND_POSE_POINT(point) \
    result->point = BlendLimbPoint(before->point, after->point, amount)
    BLEND_POSE_POINT(pelvis);
    BLEND_POSE_POINT(spine);
    BLEND_POSE_POINT(chest);
    BLEND_POSE_POINT(neck);
    BLEND_POSE_POINT(head);
    for (int32_t leg = 0; leg < CC_HUMANOID_LEG_COUNT; ++leg) {
        result->hip[leg] = BlendLimbPoint(before->hip[leg], after->hip[leg],
                                          amount);
        result->knee[leg] = BlendLimbPoint(before->knee[leg],
                                           after->knee[leg], amount);
        result->ankle[leg] = BlendLimbPoint(before->ankle[leg],
                                            after->ankle[leg], amount);
        result->heel[leg] = BlendLimbPoint(before->heel[leg],
                                           after->heel[leg], amount);
        result->ball[leg] = BlendLimbPoint(before->ball[leg],
                                           after->ball[leg], amount);
        result->toe[leg] = BlendLimbPoint(before->toe[leg], after->toe[leg],
                                          amount);
        result->foot_pitch[leg] = BlendPoseAngle(before->foot_pitch[leg],
                                                 after->foot_pitch[leg],
                                                 amount);
        result->knee_flexion[leg] =
            before->knee_flexion[leg] +
            (after->knee_flexion[leg] - before->knee_flexion[leg]) * amount;
    }
    for (int32_t arm = 0; arm < CC_HUMANOID_ARM_COUNT; ++arm) {
        result->shoulder[arm] = BlendLimbPoint(before->shoulder[arm],
                                               after->shoulder[arm], amount);
        result->elbow[arm] = BlendLimbPoint(before->elbow[arm],
                                            after->elbow[arm], amount);
        result->hand[arm] = BlendLimbPoint(before->hand[arm], after->hand[arm],
                                           amount);
    }
#undef BLEND_POSE_POINT
    result->pelvis_yaw = BlendPoseAngle(before->pelvis_yaw,
                                        after->pelvis_yaw, amount);
    result->pelvis_roll = BlendPoseAngle(before->pelvis_roll,
                                         after->pelvis_roll, amount);
    result->pelvis_pitch = BlendPoseAngle(before->pelvis_pitch,
                                          after->pelvis_pitch, amount);
    result->chest_yaw = BlendPoseAngle(before->chest_yaw,
                                       after->chest_yaw, amount);
    result->chest_roll = BlendPoseAngle(before->chest_roll,
                                        after->chest_roll, amount);
    result->chest_pitch = BlendPoseAngle(before->chest_pitch,
                                         after->chest_pitch, amount);
}

static CcLimbVec3 PosePointToLocal(CcLimbVec3 point, CcLimbVec3 origin,
                                   CcLimbVec3 right, CcLimbVec3 up,
                                   CcLimbVec3 forward)
{
    CcLimbVec3 offset = {
        point.x - origin.x,
        point.y - origin.y,
        point.z - origin.z,
    };
    return (CcLimbVec3){
        offset.x * right.x + offset.y * right.y + offset.z * right.z,
        offset.x * up.x + offset.y * up.y + offset.z * up.z,
        offset.x * forward.x + offset.y * forward.y + offset.z * forward.z,
    };
}

static CcLimbVec3 PosePointToWorld(CcLimbVec3 point, CcLimbVec3 origin,
                                   CcLimbVec3 right, CcLimbVec3 up,
                                   CcLimbVec3 forward)
{
    return (CcLimbVec3){
        origin.x + right.x * point.x + up.x * point.y + forward.x * point.z,
        origin.y + right.y * point.x + up.y * point.y + forward.y * point.z,
        origin.z + right.z * point.x + up.z * point.y + forward.z * point.z,
    };
}

static void HumanoidPoseToLocal(CcHumanoidPose *result,
                                const CcHumanoidPose *world,
                                CcLimbVec3 origin, float yaw)
{
    CcLimbVec3 right = {cosf(yaw), 0.0f, -sinf(yaw)};
    CcLimbVec3 up = {0.0f, 1.0f, 0.0f};
    CcLimbVec3 forward = {sinf(yaw), 0.0f, cosf(yaw)};
    *result = *world;
#define LOCALIZE_POSE_POINT(point) \
    result->point = PosePointToLocal(world->point, origin, right, up, forward)
    LOCALIZE_POSE_POINT(pelvis);
    LOCALIZE_POSE_POINT(spine);
    LOCALIZE_POSE_POINT(chest);
    LOCALIZE_POSE_POINT(neck);
    LOCALIZE_POSE_POINT(head);
    for (int32_t leg = 0; leg < CC_HUMANOID_LEG_COUNT; ++leg) {
        result->hip[leg] = PosePointToLocal(
            world->hip[leg], origin, right, up, forward);
        result->knee[leg] = PosePointToLocal(
            world->knee[leg], origin, right, up, forward);
        result->ankle[leg] = PosePointToLocal(
            world->ankle[leg], origin, right, up, forward);
        result->heel[leg] = PosePointToLocal(
            world->heel[leg], origin, right, up, forward);
        result->ball[leg] = PosePointToLocal(
            world->ball[leg], origin, right, up, forward);
        result->toe[leg] = PosePointToLocal(
            world->toe[leg], origin, right, up, forward);
    }
    for (int32_t arm = 0; arm < CC_HUMANOID_ARM_COUNT; ++arm) {
        result->shoulder[arm] = PosePointToLocal(
            world->shoulder[arm], origin, right, up, forward);
        result->elbow[arm] = PosePointToLocal(
            world->elbow[arm], origin, right, up, forward);
        result->hand[arm] = PosePointToLocal(
            world->hand[arm], origin, right, up, forward);
    }
#undef LOCALIZE_POSE_POINT
}

static void HumanoidPoseToWorld(CcHumanoidPose *result,
                                const CcHumanoidPose *local,
                                CcLimbVec3 origin, float yaw)
{
    CcLimbVec3 right = {cosf(yaw), 0.0f, -sinf(yaw)};
    CcLimbVec3 up = {0.0f, 1.0f, 0.0f};
    CcLimbVec3 forward = {sinf(yaw), 0.0f, cosf(yaw)};
    *result = *local;
#define WORLDIZE_POSE_POINT(point) \
    result->point = PosePointToWorld(local->point, origin, right, up, forward)
    WORLDIZE_POSE_POINT(pelvis);
    WORLDIZE_POSE_POINT(spine);
    WORLDIZE_POSE_POINT(chest);
    WORLDIZE_POSE_POINT(neck);
    WORLDIZE_POSE_POINT(head);
    for (int32_t leg = 0; leg < CC_HUMANOID_LEG_COUNT; ++leg) {
        result->hip[leg] = PosePointToWorld(
            local->hip[leg], origin, right, up, forward);
        result->knee[leg] = PosePointToWorld(
            local->knee[leg], origin, right, up, forward);
        result->ankle[leg] = PosePointToWorld(
            local->ankle[leg], origin, right, up, forward);
        result->heel[leg] = PosePointToWorld(
            local->heel[leg], origin, right, up, forward);
        result->ball[leg] = PosePointToWorld(
            local->ball[leg], origin, right, up, forward);
        result->toe[leg] = PosePointToWorld(
            local->toe[leg], origin, right, up, forward);
    }
    for (int32_t arm = 0; arm < CC_HUMANOID_ARM_COUNT; ++arm) {
        result->shoulder[arm] = PosePointToWorld(
            local->shoulder[arm], origin, right, up, forward);
        result->elbow[arm] = PosePointToWorld(
            local->elbow[arm], origin, right, up, forward);
        result->hand[arm] = PosePointToWorld(
            local->hand[arm], origin, right, up, forward);
    }
#undef WORLDIZE_POSE_POINT
}

static void PreserveSteppedFootContacts(CcHumanoidPose *stepped,
                                        const CcHumanoidPose *physical)
{
    for (int32_t leg = 0; leg < CC_HUMANOID_LEG_COUNT; ++leg) {
        CcLimbVec3 correction = {
            physical->ankle[leg].x - stepped->ankle[leg].x,
            physical->ankle[leg].y - stepped->ankle[leg].y,
            physical->ankle[leg].z - stepped->ankle[leg].z,
        };
        stepped->knee[leg].x += correction.x * 0.46f;
        stepped->knee[leg].y += correction.y * 0.46f;
        stepped->knee[leg].z += correction.z * 0.46f;
        stepped->ankle[leg] = physical->ankle[leg];
        stepped->heel[leg] = physical->heel[leg];
        stepped->ball[leg] = physical->ball[leg];
        stepped->toe[leg] = physical->toe[leg];
        stepped->foot_pitch[leg] = physical->foot_pitch[leg];
    }
}

static bool ApplySteppedLocomotionPose(CcLocalAgent *agent,
                                       const CcHumanoidPose *physical,
                                       CcHumanoidPose *result)
{
    const int32_t pose_count = 8;
    const float transition_fraction = 0.24f;
    bool active = agent->humanoid.action == CC_HUMANOID_ACTION_LOCOMOTION &&
                  agent->grounded && !agent->climbing && !agent->swimming &&
                  !agent->humanoid.ragdoll.active &&
                  agent->humanoid.speed.value > 0.10f;
    if (!active) {
        agent->stepped_pose.initialized = false;
        return false;
    }

    float phase = agent->humanoid.phase - floorf(agent->humanoid.phase);
    float stepped_phase = phase * (float)pose_count;
    int32_t bin = (int32_t)floorf(stepped_phase);
    if (bin < 0) bin = 0;
    if (bin >= pose_count) bin = pose_count - 1;
    CcHumanoidPose local;
    HumanoidPoseToLocal(&local, physical, physical->pelvis,
                        agent->facing_yaw);
    CcSteppedPoseState *state = &agent->stepped_pose;
    if (!state->initialized) {
        state->from_local = local;
        state->target_local = local;
        state->locomotion_bin = bin;
        state->initialized = true;
    } else if (state->locomotion_bin != bin) {
        state->from_local = state->target_local;
        state->target_local = local;
        state->locomotion_bin = bin;
    }

    float within = stepped_phase - floorf(stepped_phase);
    float transition = SmoothStep01(within / transition_fraction);
    CcHumanoidPose local_result;
    BlendHumanoidPose(&local_result, &state->from_local,
                      &state->target_local, transition);
    HumanoidPoseToWorld(result, &local_result, physical->pelvis,
                        agent->facing_yaw);
    PreserveSteppedFootContacts(result, physical);
    return true;
}

static CcLimbVec3 OffsetPosePoint(CcLimbVec3 point, CcLimbVec3 right,
                                  CcLimbVec3 up, CcLimbVec3 forward,
                                  float side, float rise, float depth)
{
    point.x += right.x * side + up.x * rise + forward.x * depth;
    point.y += right.y * side + up.y * rise + forward.y * depth;
    point.z += right.z * side + up.z * rise + forward.z * depth;
    return point;
}

static bool ApplyLocomotionPostureCorrection(const CcLocalAgent *agent,
                                              CcHumanoidPose *pose)
{
    bool active = agent->humanoid.action == CC_HUMANOID_ACTION_LOCOMOTION &&
                  agent->grounded && !agent->climbing && !agent->swimming &&
                  !agent->humanoid.ragdoll.active &&
                  agent->humanoid.speed.value > 0.08f;
    if (!active) return false;

    CcLimbVec3 right = {cosf(agent->facing_yaw), 0.0f,
                        -sinf(agent->facing_yaw)};
    CcLimbVec3 up = {0.0f, 1.0f, 0.0f};
    float momentum_x = agent->humanoid.root_velocity.x;
    float momentum_z = agent->humanoid.root_velocity.z;
    float momentum_speed = sqrtf(momentum_x * momentum_x +
                                 momentum_z * momentum_z);
    CcLimbVec3 forward = {sinf(agent->facing_yaw), 0.0f,
                          cosf(agent->facing_yaw)};
    float facing_momentum = forward.x * momentum_x +
                            forward.z * momentum_z;
    if (momentum_speed > 0.035f &&
        facing_momentum < momentum_speed * 0.985f) {
        forward = (CcLimbVec3){momentum_x / momentum_speed, 0.0f,
                               momentum_z / momentum_speed};
    }
    float weight = SmoothStep01((momentum_speed - 0.06f) / 0.34f);
    float depth = 0.058f * weight;

    /* Preserve the planted feet while carrying the visible upper body at its
       actual momentum. A forward-facing walk keeps the stable stepped-pose
       axis; retreating follows the body's travel instead of leaning backward
       just because the face remains on an opponent. */
    pose->spine = OffsetPosePoint(pose->spine, right, up, forward,
                                  0.0f, 0.0f, depth * 0.24f);
    pose->chest = OffsetPosePoint(pose->chest, right, up, forward,
                                  0.0f, 0.0f, depth * 0.70f);
    pose->neck = OffsetPosePoint(pose->neck, right, up, forward,
                                 0.0f, 0.0f, depth * 0.88f);
    pose->head = OffsetPosePoint(pose->head, right, up, forward,
                                 0.0f, 0.0f, depth);
    for (int32_t arm = 0; arm < CC_HUMANOID_ARM_COUNT; ++arm) {
        pose->shoulder[arm] = OffsetPosePoint(
            pose->shoulder[arm], right, up, forward,
            0.0f, 0.0f, depth * 0.70f);
        pose->elbow[arm] = OffsetPosePoint(
            pose->elbow[arm], right, up, forward,
            0.0f, 0.0f, depth * 0.38f);
        pose->hand[arm] = OffsetPosePoint(
            pose->hand[arm], right, up, forward,
            0.0f, 0.0f, depth * 0.16f);
    }
    return true;
}

static float HeldGestureWeight(const CcLocalAgent *agent)
{
    float enter = SmoothStep01((agent->humanoid.idle.still_time - 0.24f) /
                               0.24f);
    float seed_offset = (float)(agent->appearance.seed & UINT32_C(0xff)) /
                        255.0f * 3.2f;
    float cycle = fmodf(agent->humanoid.idle.still_time + seed_offset, 3.2f);
    float held = 0.0f;
    if (cycle < 0.18f) {
        held = SmoothStep01(cycle / 0.18f);
    } else if (cycle < 2.34f) {
        held = 1.0f;
    } else if (cycle < 2.56f) {
        held = 1.0f - SmoothStep01((cycle - 2.34f) / 0.22f);
    }
    return enter * held;
}

static bool ApplyRoleIdleGesture(const CcLocalAgent *agent,
                                 CcHumanoidPose *pose)
{
    bool active = !agent->crowned && agent->grounded &&
                  agent->humanoid.idle.stable && !agent->climbing &&
                  !agent->swimming && !agent->humanoid.ragdoll.active &&
                  agent->humanoid.action == CC_HUMANOID_ACTION_LOCOMOTION &&
                  agent->humanoid.speed.value < 0.10f;
    if (!active) return false;

    CcLimbVec3 right = {cosf(agent->facing_yaw), 0.0f,
                        -sinf(agent->facing_yaw)};
    CcLimbVec3 up = {0.0f, 1.0f, 0.0f};
    CcLimbVec3 forward = {sinf(agent->facing_yaw), 0.0f,
                          cosf(agent->facing_yaw)};
    float enter = SmoothStep01((agent->humanoid.idle.still_time - 0.18f) /
                               0.30f);
    float gesture = HeldGestureWeight(agent);
    float lean = agent->appearance.idle_lean * enter;
    pose->spine = OffsetPosePoint(pose->spine, right, up, forward,
                                  0.0f, 0.0f, lean * 0.36f);
    pose->chest = OffsetPosePoint(pose->chest, right, up, forward,
                                  0.0f, 0.0f, lean * 0.74f);
    pose->neck = OffsetPosePoint(pose->neck, right, up, forward,
                                 0.0f, 0.0f, lean * 0.94f);
    pose->head = OffsetPosePoint(pose->head, right, up, forward,
                                 0.0f, 0.0f, lean);
    for (int32_t arm = 0; arm < CC_HUMANOID_ARM_COUNT; ++arm) {
        pose->shoulder[arm] = OffsetPosePoint(
            pose->shoulder[arm], right, up, forward,
            0.0f, 0.0f, lean * 0.72f);
    }

    CcLimbVec3 elbow_target[CC_HUMANOID_ARM_COUNT];
    CcLimbVec3 hand_target[CC_HUMANOID_ARM_COUNT];
    for (int32_t arm = 0; arm < CC_HUMANOID_ARM_COUNT; ++arm) {
        elbow_target[arm] = pose->elbow[arm];
        hand_target[arm] = pose->hand[arm];
    }
#define ROLE_TARGET(side, rise, depth) \
    OffsetPosePoint(pose->chest, right, up, forward, side, rise, depth)
    switch (agent->appearance.role) {
        case CC_NPC_ROLE_GUARD:
            elbow_target[0] = ROLE_TARGET(-0.34f, -0.12f, 0.05f);
            hand_target[0] = ROLE_TARGET(-0.24f, -0.22f, 0.25f);
            elbow_target[1] = ROLE_TARGET(0.30f, -0.20f, 0.02f);
            hand_target[1] = ROLE_TARGET(0.17f, -0.38f, 0.12f);
            pose->chest_yaw += 0.08f * gesture;
            break;
        case CC_NPC_ROLE_RAIDER:
            elbow_target[0] = ROLE_TARGET(-0.34f, -0.08f, -0.01f);
            hand_target[0] = ROLE_TARGET(-0.22f, -0.34f, 0.08f);
            elbow_target[1] = ROLE_TARGET(0.34f, -0.02f, 0.10f);
            hand_target[1] = ROLE_TARGET(0.21f, 0.04f, 0.22f);
            pose->chest_yaw -= 0.10f * gesture;
            break;
        case CC_NPC_ROLE_MERCHANT:
            elbow_target[0] = ROLE_TARGET(-0.34f, -0.10f, 0.08f);
            hand_target[0] = ROLE_TARGET(-0.42f, -0.17f, 0.25f);
            elbow_target[1] = ROLE_TARGET(0.34f, -0.10f, 0.08f);
            hand_target[1] = ROLE_TARGET(0.42f, -0.17f, 0.25f);
            break;
        case CC_NPC_ROLE_LABORER:
            elbow_target[0] = ROLE_TARGET(-0.28f, -0.26f, 0.02f);
            hand_target[0] = ROLE_TARGET(-0.09f, -0.45f, 0.18f);
            elbow_target[1] = ROLE_TARGET(0.28f, -0.26f, 0.02f);
            hand_target[1] = ROLE_TARGET(0.09f, -0.45f, 0.18f);
            break;
        case CC_NPC_ROLE_SCOUT:
            elbow_target[0] = ROLE_TARGET(-0.34f, -0.10f, 0.04f);
            hand_target[0] = ROLE_TARGET(-0.20f, -0.36f, 0.09f);
            elbow_target[1] = ROLE_TARGET(0.37f, 0.02f, 0.13f);
            hand_target[1] = OffsetPosePoint(
                pose->head, right, up, forward, 0.16f, 0.02f, 0.23f);
            break;
        case CC_NPC_ROLE_HEALER:
            elbow_target[0] = ROLE_TARGET(-0.30f, -0.08f, 0.05f);
            hand_target[0] = ROLE_TARGET(-0.10f, -0.12f, 0.23f);
            elbow_target[1] = ROLE_TARGET(0.30f, -0.08f, 0.05f);
            hand_target[1] = ROLE_TARGET(0.10f, -0.12f, 0.23f);
            break;
        case CC_NPC_ROLE_REFUGEE:
            elbow_target[0] = ROLE_TARGET(-0.31f, -0.16f, 0.00f);
            hand_target[0] = ROLE_TARGET(-0.18f, -0.31f, 0.06f);
            elbow_target[1] = ROLE_TARGET(0.27f, -0.02f, 0.05f);
            hand_target[1] = ROLE_TARGET(0.10f, -0.08f, 0.16f);
            break;
        case CC_NPC_ROLE_TRAVELLER:
        case CC_NPC_ROLE_WAYFARER:
        default:
            elbow_target[0] = ROLE_TARGET(-0.32f, -0.16f, 0.01f);
            hand_target[0] = ROLE_TARGET(-0.20f, -0.36f, 0.07f);
            elbow_target[1] = ROLE_TARGET(0.29f, -0.06f, 0.04f);
            hand_target[1] = ROLE_TARGET(0.13f, -0.18f, 0.18f);
            break;
    }
#undef ROLE_TARGET

    float arm_weight = gesture * 0.82f;
    for (int32_t arm = 0; arm < CC_HUMANOID_ARM_COUNT; ++arm) {
        pose->elbow[arm] = BlendLimbPoint(
            pose->elbow[arm], elbow_target[arm], arm_weight);
        pose->hand[arm] = BlendLimbPoint(
            pose->hand[arm], hand_target[arm], arm_weight);
    }
    return true;
}

/* A fully physical fall is useful to gameplay, but at the fixed combat camera
   its overlapping joints can turn the crowned hero into one unreadable knot.
   Keep the physical pelvis and ground height, then bias only the rendered pose
   toward a clear, braced recovery silhouette. The simulation, contacts, and
   hit logic continue to use the untouched ragdoll. */
static bool ApplyHeroKnockdownPresentationPose(const CcLocalAgent *agent,
                                               CcHumanoidPose *pose)
{
    if (agent == NULL || pose == NULL || !agent->crowned ||
        !agent->humanoid.ragdoll.active) {
        return false;
    }

    float weight = SmoothStep01(agent->ragdoll_visual_blend) * 0.68f;
    if (weight <= 0.001f) return false;

    float floor_y = fminf(pose->ankle[0].y, pose->ankle[1].y);
    floor_y = fminf(floor_y, fminf(pose->hand[0].y, pose->hand[1].y));
    CcLimbVec3 origin = pose->pelvis;
    origin.y = fmaxf(origin.y, floor_y + 0.27f);

    CcHumanoidPose local = {0};
    local.pelvis = (CcLimbVec3){0.0f, 0.0f, 0.0f};
    local.spine = (CcLimbVec3){0.0f, 0.10f, 0.01f};
    local.chest = (CcLimbVec3){0.0f, 0.43f, 0.07f};
    local.neck = (CcLimbVec3){0.0f, 0.66f, 0.10f};
    local.head = (CcLimbVec3){0.0f, 0.84f, 0.13f};

    local.hip[0] = (CcLimbVec3){-0.155f, -0.01f, 0.0f};
    local.knee[0] = (CcLimbVec3){-0.37f, -0.12f, 0.28f};
    local.ankle[0] = (CcLimbVec3){-0.62f, floor_y - origin.y + 0.08f,
                                  0.45f};
    local.heel[0] = (CcLimbVec3){-0.62f, floor_y - origin.y + 0.02f,
                                 0.37f};
    local.ball[0] = (CcLimbVec3){-0.62f, floor_y - origin.y + 0.02f,
                                 0.53f};
    local.toe[0] = (CcLimbVec3){-0.62f, floor_y - origin.y + 0.02f,
                                0.59f};

    local.hip[1] = (CcLimbVec3){0.155f, -0.01f, 0.0f};
    local.knee[1] = (CcLimbVec3){0.34f, -0.14f, -0.18f};
    local.ankle[1] = (CcLimbVec3){0.51f, floor_y - origin.y + 0.08f,
                                  -0.38f};
    local.heel[1] = (CcLimbVec3){0.51f, floor_y - origin.y + 0.02f,
                                 -0.46f};
    local.ball[1] = (CcLimbVec3){0.51f, floor_y - origin.y + 0.02f,
                                 -0.30f};
    local.toe[1] = (CcLimbVec3){0.51f, floor_y - origin.y + 0.02f,
                                -0.24f};

    local.shoulder[0] = (CcLimbVec3){-0.285f, 0.49f, 0.07f};
    local.elbow[0] = (CcLimbVec3){-0.43f, 0.24f, 0.25f};
    local.hand[0] = (CcLimbVec3){-0.49f, floor_y - origin.y + 0.07f,
                                 0.39f};
    local.shoulder[1] = (CcLimbVec3){0.285f, 0.49f, 0.07f};
    local.elbow[1] = (CcLimbVec3){0.40f, 0.35f, 0.20f};
    local.hand[1] = (CcLimbVec3){0.22f, 0.31f, 0.31f};

    CcHumanoidPose target;
    HumanoidPoseToWorld(&target, &local, origin, agent->facing_yaw);
    CcHumanoidPose blended;
    BlendHumanoidPose(&blended, pose, &target, weight);
    *pose = blended;
    return true;
}

static void UpdateRenderCape(CcLocalAgent *agent, float amount)
{
    if (!agent->cape.initialized) {
        agent->render_cape = (CcLocalCapeState){0};
        return;
    }
    if (!agent->previous_cape.initialized) {
        agent->render_cape = agent->cape;
    } else {
        CcLocalCapeState blended = {.initialized = true};
        for (int32_t point = 0; point < CC_LOCAL_CAPE_POINT_COUNT; ++point) {
            blended.point[point] = PhysicsLerp(
                agent->previous_cape.point[point], agent->cape.point[point],
                amount);
            blended.previous[point] = PhysicsLerp(
                agent->previous_cape.previous[point],
                agent->cape.previous[point], amount);
        }
        blended.anchor = PhysicsLerp(agent->previous_cape.anchor,
                                     agent->cape.anchor, amount);
        agent->render_cape = blended;
    }

    CcHumanoidSkinPose skin;
    CcHumanoidSkinPoseResolve(&agent->render_pose, &skin);
    if (!skin.valid) return;
    Vector3 render_anchor = PhysicsAdd(
        FromLimbVector(skin.sockets[CC_HUMANOID_SOCKET_BACK].position),
        PhysicsScale(FromLimbVector(skin.body_up), 0.14f));
    Vector3 correction = PhysicsSubtract(render_anchor,
                                          agent->render_cape.anchor);
    for (int32_t point = 0; point < CC_LOCAL_CAPE_POINT_COUNT; ++point) {
        agent->render_cape.point[point] = PhysicsAdd(
            agent->render_cape.point[point], correction);
        agent->render_cape.previous[point] = PhysicsAdd(
            agent->render_cape.previous[point], correction);
    }
    agent->render_cape.anchor = render_anchor;
}

void CcLocalAgentInterpolateInternal(CcLocalAgent *agent, float amount)
{
    if (agent == NULL) return;
    if (agent->morphology == CC_MORPHOLOGY_BIPED &&
        agent->humanoid.initialized) {
        amount = fmaxf(0.0f, fminf(amount, 1.0f));
        const CcHumanoidPoseSnapshot *before =
            CcHumanoidGaitPreviousSnapshot(&agent->humanoid);
        const CcHumanoidPoseSnapshot *after =
            CcHumanoidGaitCurrentSnapshot(&agent->humanoid);
        const CcHumanoidPose *before_pose = before != NULL ? &before->pose :
            &agent->humanoid.previous_pose;
        const CcHumanoidPose *after_pose = after != NULL ? &after->pose :
            &agent->humanoid.pose;
        CcHumanoidPose physical_pose;
        BlendHumanoidPose(&physical_pose, before_pose, after_pose, amount);
        agent->render_pose = physical_pose;
        (void)ApplySteppedLocomotionPose(
            agent, &physical_pose, &agent->render_pose);
        (void)ApplyLocomotionPostureCorrection(agent, &agent->render_pose);
        (void)ApplyRoleIdleGesture(agent, &agent->render_pose);
        (void)ApplyHeroKnockdownPresentationPose(agent,
                                                 &agent->render_pose);
        agent->render_pose_valid = true;
        UpdateRenderCape(agent, amount);
    } else {
        agent->render_pose_valid = false;
        agent->render_cape = (CcLocalCapeState){0};
    }
}

static const CcHumanoidPose *AgentRenderPose(const CcLocalAgent *agent)
{
    return agent->render_pose_valid ? &agent->render_pose :
                                      &agent->humanoid.pose;
}

static Camera3D ExteriorCameraAt(Vector3 target, float fovy)
{
    return ExteriorCameraComposed(
        target, (Vector3){3.0f, 4.4f, 14.5f}, fovy);
}

static Camera3D ExteriorCameraComposed(Vector3 target, Vector3 offset,
                                       float fovy)
{
    Camera3D camera = {0};
    camera.target = target;
    /* Shot-specific offsets act as a restrained stage rotation. Keeping the
       camera well outside the room geometry prevents near-plane slices even
       when a house is retained as a foreground wing. */
    camera.position = Vector3Add(target, offset);
    camera.up = (Vector3){0.0f, 1.0f, 0.0f};
    camera.fovy = fovy;
    camera.projection = CAMERA_ORTHOGRAPHIC;
    return camera;
}

static float PerspectiveFovyForOrthographic(Camera3D camera)
{
    if (camera.projection == CAMERA_PERSPECTIVE) return camera.fovy;
    float distance = fmaxf(
        0.25f, Vector3Distance(camera.position, camera.target));
    float fovy = 2.0f * atanf(camera.fovy * 0.5f / distance) * RAD2DEG;
    return CombatClamp(fovy, 24.0f, 48.0f);
}

static Camera3D PerspectiveCameraComposed(Vector3 target, Vector3 offset,
                                          float fovy)
{
    Camera3D camera = ExteriorCameraComposed(target, offset, fovy);
    camera.projection = CAMERA_PERSPECTIVE;
    return camera;
}

static Camera3D KeepHeroInsideStreetFrame(Camera3D camera, Vector3 hero,
                                          int32_t art_height,
                                          Rectangle safe_area)
{
    if (art_height <= 0 || camera.projection != CAMERA_ORTHOGRAPHIC) {
        return camera;
    }
    int32_t art_width = (int32_t)lroundf(
        (float)art_height * 457.0f / 285.0f);
    /* This is an emergency guard, not a follow camera. Keep it wider than
       the authored dead zone so the smooth shot transition does the visible
       work and the guard only prevents a body from reaching the bezel. */
    float safe_left = fmaxf((float)art_width * 0.195f,
                            (float)art_width * safe_area.x);
    float safe_right = fminf((float)art_width * 0.805f,
                             (float)art_width *
                                 (safe_area.x + safe_area.width));
    float safe_top = fmaxf((float)art_height * 0.195f,
                           (float)art_height * safe_area.y);
    float safe_bottom = fminf((float)art_height * 0.805f,
                              (float)art_height *
                                  (safe_area.y + safe_area.height));
    Vector3 forward = Vector3Normalize(
        Vector3Subtract(camera.target, camera.position));
    Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, camera.up));
    Vector3 screen_up = Vector3Normalize(
        Vector3CrossProduct(right, forward));
    /* This is only the last safety net during a transition. It may translate
       the finished image enough to keep the actor visible, but it never
       changes the authored lens. Normal framing is handled in discrete
       steps by the rig below. */
    Vector2 screen = GetWorldToScreenEx(hero, camera, art_width, art_height);
    float clamped_x = fmaxf(safe_left, fminf(screen.x, safe_right));
    float clamped_y = fmaxf(safe_top, fminf(screen.y, safe_bottom));
    float pixel_world = camera.fovy / (float)art_height;
    Vector3 adjustment = Vector3Add(
        Vector3Scale(right, (screen.x - clamped_x) * pixel_world),
        Vector3Scale(screen_up, (clamped_y - screen.y) * pixel_world));
    camera.target = Vector3Add(camera.target, adjustment);
    camera.position = Vector3Add(camera.position, adjustment);
    return camera;
}

static Camera3D SnapCameraToArtPixels(Camera3D camera, int32_t art_height)
{
    if (art_height <= 0 || camera.projection != CAMERA_ORTHOGRAPHIC ||
        camera.fovy <= 0.0f) return camera;
    Vector3 forward = Vector3Normalize(
        Vector3Subtract(camera.target, camera.position));
    Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, camera.up));
    Vector3 screen_up = Vector3Normalize(
        Vector3CrossProduct(right, forward));
    float pixel_world = camera.fovy / (float)art_height;
    float right_coordinate = Vector3DotProduct(camera.target, right);
    float up_coordinate = Vector3DotProduct(camera.target, screen_up);
    float snapped_right = roundf(right_coordinate / pixel_world) * pixel_world;
    float snapped_up = roundf(up_coordinate / pixel_world) * pixel_world;
    Vector3 adjustment = Vector3Add(
        Vector3Scale(right, snapped_right - right_coordinate),
        Vector3Scale(screen_up, snapped_up - up_coordinate));
    camera.target = Vector3Add(camera.target, adjustment);
    camera.position = Vector3Add(camera.position, adjustment);
    return camera;
}

typedef struct CameraProjectedVolume {
    float x;
    float y;
    float half_width;
    float half_height;
    float depth;
} CameraProjectedVolume;

static CameraProjectedVolume CameraProjectVolume(Camera3D camera,
                                                  Vector3 center,
                                                  float radius,
                                                  float half_height)
{
    Vector3 forward = PhysicsNormalizeOr(
        Vector3Subtract(camera.target, camera.position),
        (Vector3){0.0f, -0.3f, -1.0f});
    Vector3 right = PhysicsNormalizeOr(
        Vector3CrossProduct(forward, camera.up),
        (Vector3){1.0f, 0.0f, 0.0f});
    Vector3 screen_up = PhysicsNormalizeOr(
        Vector3CrossProduct(right, forward), camera.up);
    Vector3 from_camera = Vector3Subtract(center, camera.position);
    Vector3 from_target = Vector3Subtract(center, camera.target);
    float right_horizontal = sqrtf(right.x * right.x +
                                   right.z * right.z);
    float up_horizontal = sqrtf(screen_up.x * screen_up.x +
                                screen_up.z * screen_up.z);
    float depth = Vector3DotProduct(from_camera, forward);
    float x = Vector3DotProduct(from_target, right);
    float y = Vector3DotProduct(from_target, screen_up);
    float projected_half_width = radius * right_horizontal +
                                 half_height * fabsf(right.y);
    float projected_half_height = radius * up_horizontal +
                                  half_height * fabsf(screen_up.y);
    if (camera.projection == CAMERA_PERSPECTIVE && depth > 0.05f) {
        float vertical_scale = 1.0f /
            (depth * tanf(camera.fovy * DEG2RAD * 0.5f));
        float horizontal_scale = vertical_scale / (457.0f / 285.0f);
        x = Vector3DotProduct(from_camera, right) * horizontal_scale;
        y = Vector3DotProduct(from_camera, screen_up) * vertical_scale;
        projected_half_width *= horizontal_scale;
        projected_half_height *= vertical_scale;
    }
    return (CameraProjectedVolume){
        .x = x,
        .y = y,
        .half_width = projected_half_width,
        .half_height = projected_half_height,
        .depth = depth,
    };
}

static float CameraVolumeOverlap(CameraProjectedVolume subject,
                                 CameraProjectedVolume occluder)
{
    /* Depth grows away from the camera. Only scenery in front of the actor
       can take away a sightline. */
    if (occluder.depth >= subject.depth - 0.18f) return 0.0f;
    float left = fmaxf(subject.x - subject.half_width,
                       occluder.x - occluder.half_width);
    float right = fminf(subject.x + subject.half_width,
                        occluder.x + occluder.half_width);
    float top = fmaxf(subject.y - subject.half_height,
                      occluder.y - occluder.half_height);
    float bottom = fminf(subject.y + subject.half_height,
                         occluder.y + occluder.half_height);
    if (right <= left || bottom <= top) return 0.0f;
    return (right - left) * (bottom - top);
}

static float CameraVolumeScreenOverlap(CameraProjectedVolume subject,
                                       CameraProjectedVolume scenery)
{
    float left = fmaxf(subject.x - subject.half_width,
                       scenery.x - scenery.half_width);
    float right = fminf(subject.x + subject.half_width,
                        scenery.x + scenery.half_width);
    float top = fmaxf(subject.y - subject.half_height,
                      scenery.y - scenery.half_height);
    float bottom = fminf(subject.y + subject.half_height,
                         scenery.y + scenery.half_height);
    if (right <= left || bottom <= top) return 0.0f;
    return (right - left) * (bottom - top);
}

static CameraProjectedVolume CameraProjectBox(Camera3D camera,
                                               Vector3 center,
                                               Vector3 half_size)
{
    Vector3 forward = PhysicsNormalizeOr(
        Vector3Subtract(camera.target, camera.position),
        (Vector3){0.0f, -0.3f, -1.0f});
    Vector3 right = PhysicsNormalizeOr(
        Vector3CrossProduct(forward, camera.up),
        (Vector3){1.0f, 0.0f, 0.0f});
    Vector3 screen_up = PhysicsNormalizeOr(
        Vector3CrossProduct(right, forward), camera.up);
    Vector3 from_camera = Vector3Subtract(center, camera.position);
    Vector3 from_target = Vector3Subtract(center, camera.target);
    float depth = Vector3DotProduct(from_camera, forward);
    float x = Vector3DotProduct(from_target, right);
    float y = Vector3DotProduct(from_target, screen_up);
    float projected_half_width = fabsf(right.x) * half_size.x +
                                 fabsf(right.y) * half_size.y +
                                 fabsf(right.z) * half_size.z;
    float projected_half_height = fabsf(screen_up.x) * half_size.x +
                                  fabsf(screen_up.y) * half_size.y +
                                  fabsf(screen_up.z) * half_size.z;
    if (camera.projection == CAMERA_PERSPECTIVE && depth > 0.05f) {
        float vertical_scale = 1.0f /
            (depth * tanf(camera.fovy * DEG2RAD * 0.5f));
        float horizontal_scale = vertical_scale / (457.0f / 285.0f);
        x = Vector3DotProduct(from_camera, right) * horizontal_scale;
        y = Vector3DotProduct(from_camera, screen_up) * vertical_scale;
        projected_half_width *= horizontal_scale;
        projected_half_height *= vertical_scale;
    }
    return (CameraProjectedVolume){
        .x = x,
        .y = y,
        .half_width = projected_half_width,
        .half_height = projected_half_height,
        .depth = depth,
    };
}

static float CameraStreetCourseClutterScore(Camera3D camera,
                                             Vector3 first_subject,
                                             Vector3 second_subject)
{
    /* A fighter can disappear against a wall or post even when that piece is
       technically behind them. Reserve a clean screen-space silhouette, not
       merely an unobstructed ray. */
    CameraProjectedVolume subjects[] = {
        CameraProjectVolume(camera, first_subject, 0.52f, 1.05f),
        CameraProjectVolume(camera, second_subject, 0.52f, 1.05f),
    };
    float score = 0.0f;
    for (int32_t platform = 0;
         platform < (int32_t)(sizeof(STREET_PLATFORMS) /
                              sizeof(STREET_PLATFORMS[0])); ++platform) {
        const NavPlatform *block = &STREET_PLATFORMS[platform];
        Vector3 half_size = {block->width * 0.5f,
                             block->height * 0.5f,
                             block->depth * 0.5f};
        Vector3 center = {
            block->x + half_size.x,
            PlatformBaseHeight(block) + half_size.y,
            block->z + half_size.z,
        };
        CameraProjectedVolume scenery = CameraProjectBox(
            camera, center, half_size);
        for (int32_t subject = 0; subject < 2; ++subject) {
            score += CameraVolumeScreenOverlap(subjects[subject], scenery);
        }
    }
    for (int32_t building = 0;
         building < (int32_t)(sizeof(WORLD_BUILDINGS) /
                              sizeof(WORLD_BUILDINGS[0])); ++building) {
        const WorldBuilding *house = &WORLD_BUILDINGS[building];
        Vector3 half_size = {house->footprint.width * 0.5f,
                             house->height * 0.5f,
                             house->footprint.height * 0.5f};
        Vector3 center = {
            house->footprint.x + half_size.x,
            TerrainFootprintHeight(house->footprint) + half_size.y,
            house->footprint.y + half_size.z,
        };
        CameraProjectedVolume scenery = CameraProjectBox(
            camera, center, half_size);
        for (int32_t subject = 0; subject < 2; ++subject) {
            score += CameraVolumeScreenOverlap(subjects[subject], scenery);
        }
    }
    for (int32_t landmark = 0;
         landmark < CC_LOCAL_PLACE_LANDMARK_COUNT; ++landmark) {
        const CcLocalPlaceLandmark *place_landmark =
            ActivePlaceLandmarkAt(landmark);
        if (place_landmark == NULL) continue;
        Vector3 half_size = {place_landmark->width * 0.5f,
                             place_landmark->height * 0.5f,
                             place_landmark->depth * 0.5f};
        Rectangle footprint = PlaceLandmarkFootprint(place_landmark);
        float center_x = place_landmark->x + half_size.x;
        float center_z = place_landmark->z + half_size.z;
        float nearest_subject = fminf(
            Vector2Distance((Vector2){center_x, center_z},
                            (Vector2){first_subject.x, first_subject.z}),
            Vector2Distance((Vector2){center_x, center_z},
                            (Vector2){second_subject.x, second_subject.z}));
        if (nearest_subject > 12.0f) continue;
        Vector3 center = {
            center_x,
            TerrainFootprintHeight(footprint) + half_size.y,
            center_z,
        };
        CameraProjectedVolume scenery = CameraProjectBox(
            camera, center, half_size);
        for (int32_t subject = 0; subject < 2; ++subject) {
            score += CameraVolumeScreenOverlap(subjects[subject], scenery);
        }
    }
    CameraProjectedVolume wayfarer_gate = CameraProjectBox(
        camera, (Vector3){11.50f, 1.16f, 10.56f},
        (Vector3){3.06f, 1.16f, 0.16f});
    for (int32_t subject = 0; subject < 2; ++subject) {
        score += CameraVolumeScreenOverlap(subjects[subject], wayfarer_gate);
    }
    return score;
}

static float CameraRoadCheckpointClutterScore(Camera3D camera,
                                               Vector3 first_subject,
                                               Vector3 second_subject,
                                               Vector3 camera_position)
{
    CameraProjectedVolume subjects[] = {
        CameraProjectVolume(camera, first_subject, 0.50f, 1.05f),
        CameraProjectVolume(camera, second_subject, 0.50f, 1.05f),
    };
    float score = 0.0f;
    int32_t count = RoadObstacleCount();
    for (int32_t obstacle = 0; obstacle < count; ++obstacle) {
        Rectangle footprint = RoadObstacleAt(obstacle);
        float center_x = footprint.x + footprint.width * 0.5f;
        float center_z = footprint.y + footprint.height * 0.5f;
        float nearest_subject = fminf(
            Vector2Distance((Vector2){center_x, center_z},
                            (Vector2){first_subject.x, first_subject.z}),
            Vector2Distance((Vector2){center_x, center_z},
                            (Vector2){second_subject.x, second_subject.z}));
        if (nearest_subject > 11.5f) continue;
        float height = 2.45f;
        if (obstacle == 4 || obstacle == 5) height = 0.92f;
        else if (obstacle == 6) height = 4.10f;
        else if (obstacle == 7) height = 1.55f;
        else if (obstacle >= 8) height = 2.70f;
        CameraProjectedVolume scenery = CameraProjectBox(
            camera,
            (Vector3){center_x, height * 0.5f, center_z},
            (Vector3){footprint.width * 0.5f, height * 0.5f,
                      footprint.height * 0.5f});
        for (int32_t subject = 0; subject < 2; ++subject) {
            score += CameraVolumeOverlap(subjects[subject], scenery) * 18.0f;
            score += CameraVolumeScreenOverlap(subjects[subject], scenery) *
                     0.75f;
        }
        score += CameraPositionRectangleClutter(
            camera_position, footprint, 1.25f) * 3.0f;
    }
    return score;
}

static float CameraPositionRectangleClutter(Vector3 position,
                                            Rectangle footprint,
                                            float clearance)
{
    float distance = TerrainDistanceToRectangle(
        position.x, position.z, footprint);
    if (distance >= clearance) return 0.0f;
    float proximity = 1.0f - distance / clearance;
    return proximity * proximity;
}

static float CombatCameraPositionClutter(Vector3 position)
{
    float score = 0.0f;
    for (int32_t building = 0;
         building < (int32_t)(sizeof(WORLD_BUILDINGS) /
                              sizeof(WORLD_BUILDINGS[0])); ++building) {
        score += CameraPositionRectangleClutter(
            position, WORLD_BUILDINGS[building].footprint, 1.80f) * 2.0f;
    }
    for (int32_t platform = 0;
         platform < (int32_t)(sizeof(STREET_PLATFORMS) /
                              sizeof(STREET_PLATFORMS[0])); ++platform) {
        const NavPlatform *block = &STREET_PLATFORMS[platform];
        score += CameraPositionRectangleClutter(
            position,
            (Rectangle){block->x, block->z, block->width, block->depth},
            1.10f);
    }
    for (int32_t structure = 0;
         structure < (int32_t)(sizeof(CASTLE_STRUCTURES) /
                               sizeof(CASTLE_STRUCTURES[0])); ++structure) {
        score += CameraPositionRectangleClutter(
            position, CASTLE_STRUCTURES[structure].footprint, 1.80f) * 2.0f;
    }
    for (int32_t landmark = 0;
         landmark < CC_LOCAL_PLACE_LANDMARK_COUNT; ++landmark) {
        score += CameraPositionRectangleClutter(
            position,
            PlaceLandmarkFootprint(ActivePlaceLandmarkAt(landmark)),
            1.80f) * 2.0f;
    }
    score += CameraPositionRectangleClutter(
        position, COURSE_POOL, 1.25f) * 1.5f;
    /* The Wayfarer gate is open to actors but its banner rail is a strong
       foreground shape. Do not park the close camera beside that rail. */
    score += CameraPositionRectangleClutter(
        position, (Rectangle){8.44f, 10.40f, 6.12f, 0.32f}, 2.20f) * 4.0f;
    return score;
}

static float CameraStreetPlatformSubjectOverlap(
    Camera3D camera, const NavPlatform *block,
    Vector3 first_subject, Vector3 second_subject)
{
    if (block == NULL) return 0.0f;
    CameraProjectedVolume subjects[] = {
        CameraProjectVolume(camera, first_subject, 0.48f, 1.00f),
        CameraProjectVolume(camera, second_subject, 0.48f, 1.00f),
    };
    Vector3 half_size = {block->width * 0.5f,
                         block->height * 0.5f,
                         block->depth * 0.5f};
    Vector3 center = {
        block->x + half_size.x,
        PlatformBaseHeight(block) + half_size.y,
        block->z + half_size.z,
    };
    CameraProjectedVolume scenery = CameraProjectBox(
        camera, center, half_size);
    return CameraVolumeScreenOverlap(subjects[0], scenery) +
           CameraVolumeScreenOverlap(subjects[1], scenery);
}

static float CameraWayfarerGateSubjectOverlap(
    Camera3D camera, Vector3 first_subject, Vector3 second_subject)
{
    CameraProjectedVolume subjects[] = {
        CameraProjectVolume(camera, first_subject, 0.48f, 1.00f),
        CameraProjectVolume(camera, second_subject, 0.48f, 1.00f),
    };
    CameraProjectedVolume gate = CameraProjectBox(
        camera, (Vector3){11.50f, 1.16f, 10.56f},
        (Vector3){3.06f, 1.16f, 0.16f});
    return CameraVolumeScreenOverlap(subjects[0], gate) +
           CameraVolumeScreenOverlap(subjects[1], gate);
}

static void WorldTreeVisibilityShape(TreeFamily family, float *trunk_top,
                                     float *crown_bottom, float *crown_top,
                                     float *trunk_radius,
                                     float *crown_radius)
{
    switch (family) {
        case TREE_FAMILY_OAK:
            *trunk_top = 3.10f;
            *crown_bottom = 2.45f;
            *crown_top = 6.35f;
            *trunk_radius = 0.72f;
            *crown_radius = 2.65f;
            return;
        case TREE_FAMILY_POLLARD:
            *trunk_top = 2.45f;
            *crown_bottom = 2.05f;
            *crown_top = 4.75f;
            *trunk_radius = 0.66f;
            *crown_radius = 1.48f;
            return;
        case TREE_FAMILY_ALDER:
        default:
            *trunk_top = 3.10f;
            *crown_bottom = 2.35f;
            *crown_top = 5.75f;
            *trunk_radius = 0.66f;
            *crown_radius = 1.56f;
            return;
    }
}

float CcLocalCameraTreeOcclusionScoreInternal(Camera3D camera,
                                               Vector3 first_subject,
                                               Vector3 second_subject)
{
    CameraProjectedVolume subjects[] = {
        CameraProjectVolume(camera, first_subject, 0.43f, 0.98f),
        CameraProjectVolume(camera, second_subject, 0.43f, 0.98f),
    };
    float score = 0.0f;
    for (int32_t tree = 0;
         tree < (int32_t)(sizeof(WORLD_TREES) / sizeof(WORLD_TREES[0]));
         ++tree) {
        Vector2 position = WORLD_TREES[tree].position;
        if (Vector2Distance(position,
                            (Vector2){camera.target.x, camera.target.z}) >
            18.0f) continue;
        float trunk_top = 0.0f;
        float crown_bottom = 0.0f;
        float crown_top = 0.0f;
        float trunk_radius = 0.0f;
        float crown_radius = 0.0f;
        WorldTreeVisibilityShape(WORLD_TREES[tree].family, &trunk_top,
                                 &crown_bottom, &crown_top, &trunk_radius,
                                 &crown_radius);
        float root_y = CcLocalTerrainHeightAt(position.x, position.y);
        CameraProjectedVolume trunk = CameraProjectVolume(
            camera,
            (Vector3){position.x, root_y + trunk_top * 0.5f, position.y},
            trunk_radius, trunk_top * 0.5f);
        CameraProjectedVolume crown = CameraProjectVolume(
            camera,
            (Vector3){position.x,
                      root_y + (crown_bottom + crown_top) * 0.5f,
                      position.y},
            crown_radius, (crown_top - crown_bottom) * 0.5f);
        for (int32_t subject = 0; subject < 2; ++subject) {
            score += CameraVolumeOverlap(subjects[subject], trunk) * 1.35f;
            score += CameraVolumeOverlap(subjects[subject], crown);
        }
    }
    return score;
}

Camera3D CcLocalCameraClearSightlinesInternal(Camera3D camera,
                                              Vector3 first_subject,
                                              Vector3 second_subject,
                                              float preferred_angle,
                                              float *chosen_angle)
{
    static const float angles[] = {
        0.0f, 8.0f * DEG2RAD, -8.0f * DEG2RAD,
        14.0f * DEG2RAD, -14.0f * DEG2RAD,
        22.0f * DEG2RAD, -22.0f * DEG2RAD,
        30.0f * DEG2RAD, -30.0f * DEG2RAD,
        44.0f * DEG2RAD, -44.0f * DEG2RAD,
        60.0f * DEG2RAD, -60.0f * DEG2RAD,
        75.0f * DEG2RAD, -75.0f * DEG2RAD,
    };
    Vector3 offset = Vector3Subtract(camera.position, camera.target);
    Camera3D best = camera;
    float best_angle = 0.0f;
    float best_cost = FLT_MAX;
    for (int32_t candidate = 0;
         candidate < (int32_t)(sizeof(angles) / sizeof(angles[0]));
         ++candidate) {
        float angle = angles[candidate];
        float cosine = cosf(angle);
        float sine = sinf(angle);
        Vector3 rotated = {
            offset.x * cosine + offset.z * sine,
            offset.y,
            -offset.x * sine + offset.z * cosine,
        };
        Camera3D option = camera;
        option.position = Vector3Add(option.target, rotated);
        float tree_occlusion = CcLocalCameraTreeOcclusionScoreInternal(
            option, first_subject, second_subject);
        float course_clutter = CameraStreetCourseClutterScore(
            option, first_subject, second_subject);
        /* Prefer the authored angle, then the angle already in use. This
           makes a clear choice stable while the fighters move. */
        float cost = tree_occlusion * 12.0f + course_clutter * 1.80f +
                     fabsf(angle) * 0.20f +
                     fabsf(WrapAngle(angle - preferred_angle)) * 0.08f;
        if (cost >= best_cost) continue;
        best = option;
        best_angle = angle;
        best_cost = cost;
    }
    if (chosen_angle != NULL) *chosen_angle = best_angle;
    return best;
}

static int32_t StreetCameraShotFor(Vector3 focus, int32_t current_shot)
{
    int32_t count = (int32_t)(sizeof(STREET_CAMERA_SHOTS) /
                              sizeof(STREET_CAMERA_SHOTS[0]));
    int32_t nearest = 0;
    float nearest_distance = FLT_MAX;
    for (int32_t shot = 0; shot < count; ++shot) {
        float x = focus.x - STREET_CAMERA_SHOTS[shot].trigger.x;
        float z = focus.z - STREET_CAMERA_SHOTS[shot].trigger.y;
        float distance = x * x + z * z;
        if (distance >= nearest_distance) continue;
        nearest = shot;
        nearest_distance = distance;
    }
    if (current_shot >= 0 && current_shot < count &&
        current_shot != nearest) {
        float current_x = focus.x -
                          STREET_CAMERA_SHOTS[current_shot].trigger.x;
        float current_z = focus.z -
                          STREET_CAMERA_SHOTS[current_shot].trigger.y;
        float current_distance = sqrtf(current_x * current_x +
                                       current_z * current_z);
        float candidate_distance = sqrtf(nearest_distance);
        if (candidate_distance + 1.25f >= current_distance) {
            return current_shot;
        }
    }
    return nearest;
}

static int32_t StreetCameraCompositionFor(Vector3 focus,
                                          int32_t current_shot)
{
    current_shot = StreetCameraBaseShot(current_shot);
    /* Turn toward Crown Gate as soon as the market road clears its last
       house. The room lenses share a viewing side, so this becomes a gentle
       reveal of the gate instead of lingering behind the foreground wing. A
       wider reverse threshold supplies hysteresis on the walk back. Keep
       this visual handoff separate from logical room ownership. */
    bool crown_gate_corridor = focus.z >= 26.2f && focus.z <= 35.2f;
    if (crown_gate_corridor) {
        if (current_shot == MARKET_GATE_ROAD_CAMERA_SHOT &&
            focus.x >= 57.6f && focus.x <= 68.6f) {
            return MARKET_GATE_ROAD_CAMERA_SHOT;
        }
        if (focus.x >= 68.0f && focus.x <= 79.5f) return 8;
        if (focus.x >= 58.8f) return MARKET_GATE_ROAD_CAMERA_SHOT;
    }
    return StreetCameraShotFor(focus, current_shot);
}

static const StreetCameraShot *StreetCameraShotAt(int32_t shot)
{
    shot = StreetCameraBaseShot(shot);
    int32_t count = (int32_t)(sizeof(STREET_CAMERA_SHOTS) /
                              sizeof(STREET_CAMERA_SHOTS[0]));
    if (shot == MARKET_GATE_ROAD_CAMERA_SHOT) {
        return &MARKET_GATE_ROAD_CAMERA;
    }
    if (shot < 0 || shot >= count) return &STREET_CAMERA_SHOTS[0];
    return &STREET_CAMERA_SHOTS[shot];
}

static void FixedCameraRigAim(FixedCameraRig *rig, int32_t shot,
                              Vector3 destination, Vector3 offset,
                              float fovy, float clock, bool advance)
{
    if (!rig->initialized) {
        rig->displayed_target = destination;
        rig->transition_from = destination;
        rig->destination = destination;
        rig->displayed_offset = offset;
        rig->offset_transition_from = offset;
        rig->offset_destination = offset;
        rig->displayed_fovy = fovy;
        rig->fovy_transition_from = fovy;
        rig->fovy_destination = fovy;
        rig->transition_duration = 0.0f;
        rig->transition_elapsed = 0.0f;
        rig->framing_offset = (Vector3){0};
        rig->framing_from = (Vector3){0};
        rig->framing_destination = (Vector3){0};
        rig->framing_elapsed = 0.0f;
        rig->framing_duration = 0.0f;
        rig->framing_hold_seconds = 0.0f;
        rig->delta_time = 0.0f;
        rig->last_clock = clock;
        rig->shot = shot;
        rig->initialized = true;
        return;
    }
    if (!advance) {
        rig->delta_time = 0.0f;
        return;
    }
    float delta_time = clock - rig->last_clock;
    rig->last_clock = clock;
    delta_time = fmaxf(0.0f, fminf(delta_time, 0.05f));
    rig->delta_time = delta_time;
    if (shot != rig->shot) {
        float distance = Vector3Distance(rig->displayed_target, destination);
        rig->transition_from = rig->displayed_target;
        rig->destination = destination;
        rig->offset_transition_from = rig->displayed_offset;
        rig->offset_destination = offset;
        rig->fovy_transition_from = rig->displayed_fovy;
        rig->fovy_destination = fovy;
        rig->transition_elapsed = 0.0f;
        rig->transition_duration = fmaxf(0.90f, fminf(1.25f,
                                                     0.78f + distance * 0.018f));
        /* A new authored shot owns its own framing. The hard visibility
           clamp covers the short transition before the new shot settles. */
        rig->framing_offset = (Vector3){0};
        rig->framing_from = (Vector3){0};
        rig->framing_destination = (Vector3){0};
        rig->framing_elapsed = 0.0f;
        rig->framing_duration = 0.0f;
        rig->framing_hold_seconds = 0.0f;
        rig->shot = shot;
    }
    if (rig->transition_elapsed >= rig->transition_duration ||
        rig->transition_duration <= 0.0f) {
        rig->displayed_target = rig->destination;
        rig->displayed_offset = rig->offset_destination;
        rig->displayed_fovy = rig->fovy_destination;
        return;
    }
    rig->transition_elapsed = fminf(rig->transition_duration,
                                    rig->transition_elapsed + delta_time);
    /* Fixed adventure-game shots should not drift beside the actor. Hold the
       old page while it fades down, switch at full darkness, then reveal the
       new page. The fade hides the single-frame camera change. */
    bool reveal_destination =
        rig->transition_elapsed >= rig->transition_duration * 0.5f;
    rig->displayed_target = reveal_destination ? rig->destination :
                                                 rig->transition_from;
    rig->displayed_offset = reveal_destination ? rig->offset_destination :
                                                 rig->offset_transition_from;
    rig->displayed_fovy = reveal_destination ? rig->fovy_destination :
                                               rig->fovy_transition_from;
}

static void DrawFixedCameraFade(const FixedCameraRig *rig,
                                Rectangle destination)
{
    if (rig == NULL || rig->transition_duration <= 0.0f ||
        rig->transition_elapsed >= rig->transition_duration) return;
    float progress = rig->transition_elapsed / rig->transition_duration;
    float peak = 1.0f - fabsf(progress * 2.0f - 1.0f);
    float opacity = SmoothStep01(peak);
    DrawRectangleRec(destination, Fade(WORLD_VOID, opacity));
}

static Camera3D FixedCameraRigFrameHero(FixedCameraRig *rig,
                                         Camera3D camera, Vector3 hero,
                                         int32_t art_height,
                                         Rectangle quiet_area,
                                         bool advance)
{
    if (rig == NULL || art_height <= 0 ||
        camera.projection != CAMERA_ORTHOGRAPHIC) return camera;
    int32_t art_width = (int32_t)lroundf(
        (float)art_height * 457.0f / 285.0f);

    if (advance && rig->framing_duration > 0.0f) {
        rig->framing_elapsed = fminf(
            rig->framing_duration,
            rig->framing_elapsed + rig->delta_time);
        float amount = SmoothStep01(rig->framing_elapsed /
                                    rig->framing_duration);
        rig->framing_offset = Vector3Lerp(
            rig->framing_from, rig->framing_destination, amount);
        if (rig->framing_elapsed >= rig->framing_duration) {
            rig->framing_duration = 0.0f;
            rig->framing_hold_seconds = 1.00f;
        }
    }
    if (advance && rig->framing_duration <= 0.0f &&
        rig->framing_hold_seconds > 0.0f) {
        rig->framing_hold_seconds = fmaxf(
            0.0f, rig->framing_hold_seconds - rig->delta_time);
    }

    camera.target = Vector3Add(camera.target, rig->framing_offset);
    camera.position = Vector3Add(camera.position, rig->framing_offset);
    bool authored_transition_done =
        rig->transition_duration <= 0.0f ||
        rig->transition_elapsed >= rig->transition_duration;
    if (!advance || !authored_transition_done ||
        rig->framing_duration > 0.0f ||
        rig->framing_hold_seconds > 0.0f) return camera;

    float quiet_left = quiet_area.x * (float)art_width;
    float quiet_right = (quiet_area.x + quiet_area.width) *
                        (float)art_width;
    float quiet_top = quiet_area.y * (float)art_height;
    float quiet_bottom = (quiet_area.y + quiet_area.height) *
                         (float)art_height;
    float trigger_left = fmaxf(quiet_left, (float)art_width * 0.28f);
    float trigger_right = fminf(quiet_right, (float)art_width * 0.72f);
    float trigger_top = fmaxf(quiet_top, (float)art_height * 0.28f);
    float trigger_bottom = fminf(quiet_bottom,
                                 (float)art_height * 0.72f);
    Vector2 screen = GetWorldToScreenEx(
        hero, camera, art_width, art_height);
    float target_x = screen.x;
    float target_y = screen.y;
    float center_x = (quiet_left + quiet_right) * 0.5f;
    float center_y = (quiet_top + quiet_bottom) * 0.5f;
    if (screen.x < trigger_left) {
        target_x = center_x;
    } else if (screen.x > trigger_right) {
        target_x = center_x;
    }
    if (screen.y < trigger_top) {
        target_y = center_y;
    } else if (screen.y > trigger_bottom) {
        target_y = center_y;
    }
    if (fabsf(target_x - screen.x) < 0.5f &&
        fabsf(target_y - screen.y) < 0.5f) return camera;

    Vector3 forward = Vector3Normalize(
        Vector3Subtract(camera.target, camera.position));
    Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, camera.up));
    Vector3 screen_up = Vector3Normalize(
        Vector3CrossProduct(right, forward));
    float pixel_world = camera.fovy / (float)art_height;
    Vector3 adjustment = Vector3Add(
        Vector3Scale(right, (screen.x - target_x) * pixel_world),
        Vector3Scale(screen_up, (target_y - screen.y) * pixel_world));
    rig->framing_from = rig->framing_offset;
    rig->framing_destination = Vector3Add(rig->framing_offset, adjustment);
    rig->framing_elapsed = 0.0f;
    /* Move to one new page and hold it. The old 12%-of-screen correction
       could chain every few frames while the hero kept walking, which read
       as a nervous follow camera. Centering once creates the fixed-room
       adventure-game rhythm while still protecting long roads. */
    rig->framing_duration = 1.05f;
    return camera;
}

static float StreetAlleyWeight(Vector3 focus)
{
    float nearest = FLT_MAX;
    float second_nearest = FLT_MAX;
    for (int32_t building = 0;
         building < (int32_t)(sizeof(WORLD_BUILDINGS) /
                              sizeof(WORLD_BUILDINGS[0])); ++building) {
        float distance = sqrtf(FootprintDistanceSquared(
            focus.x, focus.z, WORLD_BUILDINGS[building].footprint));
        if (distance < nearest) {
            second_nearest = nearest;
            nearest = distance;
        } else if (distance < second_nearest) {
            second_nearest = distance;
        }
    }
    return SmoothStep01((6.8f - second_nearest) / 3.8f);
}

Camera3D CcLocalStreetCameraInternal(const CcLocalAgent *agent, float clock,
                                     bool advance, int32_t art_height)
{
    Vector3 focus = agent != NULL ? agent->position : (Vector3){0};
    int32_t base_shot = StreetCameraCompositionFor(
        focus, street_camera_rig.shot);
    const StreetCameraShot *composition = StreetCameraShotAt(base_shot);
    int32_t shot = base_shot;
    Vector3 destination = composition->target;
    destination.y += CcLocalTerrainHeightAt(destination.x, destination.z);
    float fovy = composition->fovy;
    if (agent != NULL) {
        float alley_weight = StreetAlleyWeight(focus);
        bool hold_alley = StreetCameraShotIsAlley(street_camera_rig.shot) &&
                          StreetCameraBaseShot(street_camera_rig.shot) ==
                              base_shot;
        if (alley_weight >= (hold_alley ? 0.34f : 0.58f)) {
            /* Like an adventure-game close-up: choose the composition once
               on entering the alley, animate to it, then hold it. */
            shot = StreetCameraAlleyShot(base_shot);
            destination = (Vector3){focus.x, focus.y + 1.00f, focus.z};
            fovy = fminf(fovy, 9.8f);
        }
    }
    FixedCameraRigAim(&street_camera_rig, shot, destination,
                      composition->camera_offset, fovy,
                      clock, advance);
    Camera3D camera = ExteriorCameraComposed(
        street_camera_rig.displayed_target,
        street_camera_rig.displayed_offset,
        street_camera_rig.displayed_fovy);
    if (agent != NULL) {
        Vector3 hero = {focus.x, focus.y + 1.05f, focus.z};
        camera = FixedCameraRigFrameHero(
            &street_camera_rig, camera, hero, art_height,
            composition->art.quiet_area, advance);
        camera = KeepHeroInsideStreetFrame(
            camera, hero, art_height,
            (Rectangle){0.10f, 0.12f, 0.80f, 0.76f});
    }
    return SnapCameraToArtPixels(camera, art_height);
}

static int32_t RoadCameraShotFor(float x, int32_t current_shot)
{
    static const float centers[] = {
        25.0f, 35.0f, 45.0f, 55.0f, 65.0f, 75.0f
    };
    int32_t count = (int32_t)(sizeof(centers) / sizeof(centers[0]));
    if (current_shot >= 0 && current_shot < count &&
        fabsf(x - centers[current_shot]) <= 6.2f) return current_shot;
    int32_t nearest = 0;
    float nearest_distance = FLT_MAX;
    for (int32_t shot = 0; shot < count; ++shot) {
        float distance = fabsf(x - centers[shot]);
        if (distance >= nearest_distance) continue;
        nearest = shot;
        nearest_distance = distance;
    }
    return nearest;
}

static Camera3D RoadCamera(Vector3 focus, bool travelling, float clock,
                           bool advance, int32_t art_height)
{
    static const float centers[] = {
        25.0f, 35.0f, 45.0f, 55.0f, 65.0f, 75.0f
    };
    int32_t shot = travelling ?
        RoadCameraShotFor(focus.x, road_camera_rig.shot) : 4;
    Vector3 destination = travelling ?
        (Vector3){centers[shot] - 0.90f, 0.95f, 40.0f} :
        (Vector3){48.60f, 0.95f, 40.0f};
    const Vector3 road_offset = {3.0f, 4.4f, 14.5f};
    FixedCameraRigAim(&road_camera_rig, shot, destination, road_offset,
                      10.8f, clock, advance);
    return SnapCameraToArtPixels(
        ExteriorCameraComposed(road_camera_rig.displayed_target,
                               road_camera_rig.displayed_offset,
                               road_camera_rig.displayed_fovy),
        art_height);
}

static const CcLocalAgent *CombatCameraOpponent(
    const CcLocalCourse *course, const CcLocalAgent *player)
{
    if (course == NULL || player == NULL) return NULL;
    int32_t selected = player->combat.target_index;
    if (selected >= 0 && selected < CC_LOCAL_RAIDER_COUNT &&
        (CombatCanAct(&course->raiders[selected].combat) ||
         course->combat_event_seconds > 0.0f)) {
        return &course->raiders[selected];
    }
    /* A resolved strike clears the actor focus. Keep only that same selected
       opponent for the short impact beat; never replace it with a nearby
       raider the player did not choose. */
    int32_t held = combat_camera_rig.opponent_index;
    if (combat_camera_rig.composition_locked &&
        course->combat_event_seconds > 0.0f && held >= 0 &&
        held < CC_LOCAL_RAIDER_COUNT &&
        !CombatCanAct(&course->raiders[held].combat) &&
        CombatHorizontalDistanceSquared(
            player, &course->raiders[held]) <= 6.0f * 6.0f) {
        return &course->raiders[held];
    }
    return NULL;
}

static int32_t CombatCameraOpponentIndex(const CcLocalCourse *course,
                                         const CcLocalAgent *opponent)
{
    if (course == NULL || opponent == NULL) return -1;
    for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
        if (opponent == &course->raiders[i]) return i;
    }
    return -1;
}

static void CombatCameraLockComposition(Camera3D base,
                                         const CcLocalAgent *player,
                                         const CcLocalAgent *opponent,
                                         const CcLocalCourse *course)
{
    Vector3 opponent_position = opponent != NULL ? opponent->position :
        course->combat_origin;
    Vector3 fight_line = Vector3Subtract(opponent_position,
                                          player->position);
    fight_line.y = 0.0f;
    float span = fmaxf(1.0f, Vector3Length(fight_line));
    fight_line = PhysicsNormalizeOr(
        fight_line, (Vector3){1.0f, 0.0f, 0.0f});
    Vector3 fight_side = {-fight_line.z, 0.0f, fight_line.x};
    Vector3 base_view = Vector3Subtract(base.position, base.target);
    base_view.y = 0.0f;
    base_view = PhysicsNormalizeOr(base_view,
                                    (Vector3){0.0f, 0.0f, 1.0f});
    if (Vector3DotProduct(fight_side, base_view) < 0.0f) {
        fight_side = Vector3Negate(fight_side);
    }

    /* Third-person lock-on composition: put the lens behind the hero and
       aim toward the opponent. A wider shoulder offset gives the two bodies
       separate silhouettes instead of stacking them on the same line. */
    float look_ahead = fminf(span * 0.72f, 3.30f);
    Vector3 target = Vector3Add(
        player->position, Vector3Scale(fight_line, look_ahead));
    target.y = player->position.y + 1.32f;
    bool road_duel = course->scene == CC_LOCAL_SCENE_ROAD;
    float follow_distance = road_duel ?
        4.45f + fminf(span, 7.0f) * 0.08f :
        5.40f + fminf(span, 7.0f) * 0.12f;
    float fovy = road_duel ?
        CombatClamp(40.0f + span * 0.50f, 42.0f, 46.0f) :
        CombatClamp(44.0f + span * 0.75f, 45.0f, 50.0f);
    /* Close melee needs a stronger diagonal than a long lock-on. Without
       this contact adjustment, the opponent disappears directly behind the
       hero at the moment the attack lands. */
    float contact_amount = CombatClamp(2.80f - span, 0.0f, 2.0f);
    float shoulder_distance = road_duel ?
        2.05f + contact_amount * 0.45f :
        3.15f + contact_amount * 0.85f;
    Vector3 player_center = Vector3Add(
        player->position, (Vector3){0.0f, 1.02f, 0.0f});
    Vector3 opponent_center = Vector3Add(
        opponent_position, (Vector3){0.0f, 1.02f, 0.0f});
    Vector3 offset = {0};
    Vector3 chosen_side = fight_side;
    float best_cost = FLT_MAX;
    int32_t opponent_index = CombatCameraOpponentIndex(course, opponent);
    bool preserve_shoulder = combat_camera_rig.shoulder_valid &&
        combat_camera_rig.composition_locked &&
        combat_camera_rig.opponent_index == opponent_index;

    /* Try both shoulders. The current side receives a real hysteresis
       advantage, so a small movement or equal clutter score cannot flip the
       whole shot. A badly blocked side can still lose decisively. */
    for (int32_t shoulder = 0; shoulder < 2; ++shoulder) {
        Vector3 candidate_side = shoulder == 0 ? fight_side :
                                                Vector3Negate(fight_side);
        Vector3 camera_position = Vector3Add(
            player->position,
            Vector3Add(Vector3Scale(fight_line, -follow_distance),
                       Vector3Scale(candidate_side, shoulder_distance)));
        camera_position.y = player->position.y +
                            (road_duel ? 2.85f : 3.15f);
        Vector3 candidate_offset = Vector3Subtract(camera_position, target);
        Camera3D candidate = PerspectiveCameraComposed(
            target, candidate_offset, fovy);
        float cost = shoulder == 0 ? 0.0f : 0.12f;
        if (course->scene == CC_LOCAL_SCENE_STREET) {
            cost +=
                CcLocalCameraTreeOcclusionScoreInternal(
                    candidate, player_center, opponent_center) * 14.0f +
                CameraStreetCourseClutterScore(
                    candidate, player_center, opponent_center) * 2.2f +
                CombatCameraPositionClutter(camera_position) * 8.0f;
        } else if (road_duel) {
            cost += CameraRoadCheckpointClutterScore(
                candidate, player_center, opponent_center,
                camera_position) * 2.4f;
        }
        if (preserve_shoulder &&
            Vector3DotProduct(candidate_side,
                              combat_camera_rig.locked_shoulder_side) < 0.0f) {
            cost += 0.90f;
        }
        if (cost >= best_cost) continue;
        best_cost = cost;
        offset = candidate_offset;
        chosen_side = candidate_side;
    }
    combat_camera_rig.tree_clear_angle = 0.0f;
    combat_camera_rig.locked_target = target;
    combat_camera_rig.locked_offset = offset;
    combat_camera_rig.locked_shoulder_side = chosen_side;
    combat_camera_rig.locked_fovy = fovy;
    combat_camera_rig.opponent_index = opponent_index;
    combat_camera_rig.composition_locked = true;
    combat_camera_rig.shoulder_valid = true;
    combat_camera_rig.reframe_cooldown = 0.45f;
}

static bool CombatCameraSubjectsNeedReframe(const CcLocalAgent *player,
                                             const CcLocalAgent *opponent,
                                             const CcLocalCourse *course,
                                             int32_t art_height)
{
    if (!combat_camera_rig.composition_locked || art_height <= 0) {
        return true;
    }
    int32_t art_width = (int32_t)lroundf(
        (float)art_height * 457.0f / 285.0f);
    Camera3D camera = PerspectiveCameraComposed(
        combat_camera_rig.locked_target,
        combat_camera_rig.locked_offset,
        combat_camera_rig.locked_fovy);
    Vector3 opponent_position = opponent != NULL ? opponent->position :
        course->combat_origin;
    Vector3 fight_line = Vector3Subtract(opponent_position, player->position);
    fight_line.y = 0.0f;
    float live_span = fmaxf(1.0f, Vector3Length(fight_line));
    fight_line = PhysicsNormalizeOr(
        fight_line, (Vector3){1.0f, 0.0f, 0.0f});
    Vector3 intended_target = Vector3Add(
        player->position,
        Vector3Scale(fight_line, fminf(live_span * 0.72f, 3.30f)));
    intended_target.y = player->position.y + 1.32f;
    float target_x = intended_target.x - combat_camera_rig.locked_target.x;
    float target_z = intended_target.z - combat_camera_rig.locked_target.z;
    if (target_x * target_x + target_z * target_z > 1.35f * 1.35f) {
        return true;
    }
    float live_fovy = course->scene == CC_LOCAL_SCENE_ROAD ?
        CombatClamp(40.0f + live_span * 0.50f, 42.0f, 46.0f) :
        CombatClamp(44.0f + live_span * 0.75f, 45.0f, 50.0f);
    if (fabsf(live_fovy - combat_camera_rig.locked_fovy) > 1.20f) {
        return true;
    }
    Vector3 subjects[] = {
        Vector3Add(player->position, (Vector3){0.0f, 0.04f, 0.0f}),
        Vector3Add(player->position, (Vector3){0.0f, 2.24f, 0.0f}),
        Vector3Add(player->position, (Vector3){0.46f, 1.05f, 0.0f}),
        Vector3Add(player->position, (Vector3){-0.46f, 1.05f, 0.0f}),
        Vector3Add(opponent_position, (Vector3){0.0f, 0.04f, 0.0f}),
        Vector3Add(opponent_position, (Vector3){0.0f, 2.24f, 0.0f}),
        Vector3Add(opponent_position, (Vector3){0.46f, 1.05f, 0.0f}),
        Vector3Add(opponent_position, (Vector3){-0.46f, 1.05f, 0.0f}),
    };
    for (int32_t subject = 0; subject < 8; ++subject) {
        if (!CameraPointInFront(camera, subjects[subject])) return true;
        Vector2 screen = GetWorldToScreenEx(
            subjects[subject], camera, art_width, art_height);
        if (screen.x < (float)art_width * 0.10f ||
            screen.x > (float)art_width * 0.90f ||
            screen.y < (float)art_height * 0.06f ||
            screen.y > (float)art_height * 0.94f) return true;
    }
    return false;
}

/* Combat moves into a perspective over-the-shoulder lock-on view. The duel
   composition is chosen once and held like a stage shot; it is recomposed
   only when a fighter crosses the safe frame or the opponent changes. */
Camera3D CcLocalCombatCameraInternal(Camera3D base,
                                     const CcLocalAgent *player,
                                     const CcLocalCourse *course,
                                     float clock, bool advance,
                                     int32_t art_height)
{
    const CcLocalAgent *opponent = CombatCameraOpponent(course, player);
    float opponent_distance_squared = opponent != NULL && player != NULL ?
        CombatHorizontalDistanceSquared(player, opponent) : FLT_MAX;
    /* An alarm may cover the whole settlement, but the duel camera may not.
       Hold the room shot while a raider is still approaching and enter the
       two-subject composition only when the fight is locally readable. */
    bool active = player != NULL && course != NULL && opponent != NULL &&
                  course->alarm_active && !course->raiders_retreating &&
                  opponent_distance_squared <= 9.0f * 9.0f;
    Vector3 base_offset = Vector3Subtract(base.position, base.target);
    float base_perspective_fovy = PerspectiveFovyForOrthographic(base);
    bool scene_changed = course != NULL &&
        (!combat_camera_rig.scene_valid ||
         combat_camera_rig.scene != course->scene ||
         combat_camera_rig.road_encounter != course->road_encounter);
    if (!combat_camera_rig.initialized || scene_changed) {
        combat_camera_rig.displayed_target = base.target;
        combat_camera_rig.displayed_offset = base_offset;
        combat_camera_rig.displayed_fovy = base_perspective_fovy;
        combat_camera_rig.locked_target = base.target;
        combat_camera_rig.locked_offset = base_offset;
        combat_camera_rig.locked_fovy = base_perspective_fovy;
        combat_camera_rig.combat_weight = 0.0f;
        combat_camera_rig.tree_clear_angle = 0.0f;
        combat_camera_rig.reframe_cooldown = 0.0f;
        combat_camera_rig.last_clock = clock;
        combat_camera_rig.opponent_index = -1;
        combat_camera_rig.composition_locked = false;
        combat_camera_rig.shoulder_valid = false;
        combat_camera_rig.initialized = true;
        if (course != NULL) {
            combat_camera_rig.scene = course->scene;
            combat_camera_rig.scene_valid = true;
            combat_camera_rig.road_encounter = course->road_encounter;
        }
    }

    if (advance) {
        float delta_time = clock - combat_camera_rig.last_clock;
        if (delta_time < 0.0f || delta_time > 0.12f) delta_time = 0.0f;
        delta_time = fminf(delta_time, 0.050f);
        combat_camera_rig.last_clock = clock;
        /* Road ambushes begin inside a narrow authored bridge shot. Enter
           their shoulder camera promptly so the parapets do not hold the
           player at the edge of the wide establishing frame. Settlement
           fights keep the calmer transition used by the gameplay reel. */
        bool quick_road_entry = active && course != NULL &&
                                course->scene == CC_LOCAL_SCENE_ROAD;
        float direction = active ? (quick_road_entry ? 4.2f : 1.65f) :
                                   -1.35f;
        combat_camera_rig.combat_weight = CombatClamp(
            combat_camera_rig.combat_weight + delta_time * direction,
            0.0f, 1.0f);

        if (active) {
            combat_camera_rig.reframe_cooldown = fmaxf(
                0.0f, combat_camera_rig.reframe_cooldown - delta_time);
            int32_t opponent_index = CombatCameraOpponentIndex(
                course, opponent);
            bool opponent_changed = combat_camera_rig.composition_locked &&
                opponent_index != combat_camera_rig.opponent_index;
            bool left_safe_frame =
                combat_camera_rig.reframe_cooldown <= 0.0f &&
                CombatCameraSubjectsNeedReframe(
                    player, opponent, course, art_height);
            if (!combat_camera_rig.composition_locked || opponent_changed ||
                left_safe_frame) {
                if (opponent_changed) combat_camera_rig.shoulder_valid = false;
                CombatCameraLockComposition(base, player, opponent, course);
            }
        }

        float weight = SmoothStep01(combat_camera_rig.combat_weight);
        Vector3 desired_target = Vector3Lerp(
            base.target, combat_camera_rig.locked_target, weight);
        Vector3 desired_offset = Vector3Lerp(
            base_offset, combat_camera_rig.locked_offset, weight);
        float desired_fovy = base_perspective_fovy +
            (combat_camera_rig.locked_fovy - base_perspective_fovy) * weight;
        float response = quick_road_entry ? 8.0f : 4.5f;
        float ease = 1.0f - expf(-delta_time * response);
        combat_camera_rig.displayed_target = Vector3Lerp(
            combat_camera_rig.displayed_target, desired_target, ease);
        combat_camera_rig.displayed_offset = Vector3Lerp(
            combat_camera_rig.displayed_offset, desired_offset, ease);
        combat_camera_rig.displayed_fovy +=
            (desired_fovy - combat_camera_rig.displayed_fovy) * ease;
        if (!active && combat_camera_rig.combat_weight <= 0.0001f) {
            combat_camera_rig.displayed_target = base.target;
            combat_camera_rig.displayed_offset = base_offset;
            combat_camera_rig.displayed_fovy = base_perspective_fovy;
            combat_camera_rig.tree_clear_angle = 0.0f;
            combat_camera_rig.reframe_cooldown = 0.0f;
            combat_camera_rig.opponent_index = -1;
            combat_camera_rig.composition_locked = false;
            combat_camera_rig.shoulder_valid = false;
        }
    }

    Camera3D camera = base;
    camera.target = combat_camera_rig.displayed_target;
    camera.position = Vector3Add(camera.target,
                                 combat_camera_rig.displayed_offset);
    bool combat_view = active || combat_camera_rig.combat_weight > 0.0001f;
    camera.fovy = combat_view ? combat_camera_rig.displayed_fovy : base.fovy;
    camera.projection = combat_view ? CAMERA_PERSPECTIVE : base.projection;
    if (active && player->combat.hit_flash_seconds > 0.0f) {
        float pulse = player->combat.hit_flash_seconds * 8.0f;
        float shake = fminf(0.055f, pulse * 0.055f);
        camera.target.x += sinf(clock * 71.0f) * shake;
        camera.target.y += cosf(clock * 59.0f) * shake * 0.45f;
        camera.position.x += sinf(clock * 71.0f) * shake;
        camera.position.y += cosf(clock * 59.0f) * shake * 0.45f;
    }
    return SnapCameraToArtPixels(camera, art_height);
}

static Camera3D LocalCamera(bool interior, Vector3 focus)
{
    if (!interior) {
        Vector3 target = {
            fmaxf(8.0f, fminf(focus.x, CC_LOCAL_WORLD_WIDTH - 8.0f)),
            0.95f,
            fmaxf(7.0f, fminf(focus.z, CC_LOCAL_WORLD_DEPTH - 7.0f))
        };
        return ExteriorCameraAt(target, 10.8f);
    }
    Camera3D camera = {0};
    (void)focus;
    /* The counter, actors, and street door form back/middle/foreground layers.
       The tighter frame gives the compact room the same visual weight as an
       exterior stage and keeps faces readable on the art-pixel grid. */
    camera.target = (Vector3){4.70f, 0.90f, 3.30f};
    camera.position = (Vector3){camera.target.x + 1.5f,
                                camera.target.y + 3.7f,
                                camera.target.z + 13.0f};
    camera.up = (Vector3){0.0f, 1.0f, 0.0f};
    camera.fovy = 7.6f;
    camera.projection = CAMERA_ORTHOGRAPHIC;
    return camera;
}

static Color KingdomColor3D(const CcSim *sim, CcId id)
{
    for (int32_t i = 0; i < sim->kingdom_count; ++i) {
        if (sim->kingdoms[i].id != id) continue;
        return (Color){sim->kingdoms[i].color_r, sim->kingdoms[i].color_g,
                       sim->kingdoms[i].color_b, 255};
    }
    return WORLD_MUTED;
}

static const CcDungeon *DungeonAt(const CcSim *sim, CcId settlement_id)
{
    for (int32_t i = 0; i < sim->dungeon_count; ++i) {
        if (sim->dungeons[i].settlement_id == settlement_id) return &sim->dungeons[i];
    }
    return NULL;
}

static bool HasSmugglerRoad(const CcSim *sim, CcId settlement_id)
{
    for (int32_t i = 0; i < sim->route_count; ++i) {
        const CcRoute *route = &sim->routes[i];
        if (!route->smuggler_route) continue;
        if (route->from_id == settlement_id || route->to_id == settlement_id) return true;
    }
    return false;
}

typedef struct SphereModelCache {
    Model small;
    Model character;
    Model scenery;
    bool ready;
} SphereModelCache;

static SphereModelCache sphere_models = {0};
static RenderTexture2D npc_portrait_target = {0};

typedef enum TreeCrownShape {
    TREE_CROWN_ALDER = 0,
    TREE_CROWN_OAK,
    TREE_CROWN_POLLARD
} TreeCrownShape;

typedef struct TreeCrownModelCache {
    Model alder;
    Model oak;
    Model pollard;
    bool ready;
} TreeCrownModelCache;

static TreeCrownModelCache tree_crown_models = {0};

#define CC_HERO_SKIN_MAX_BONES 64
#define CC_HERO_CAPE_BONE_COUNT 4
#define CC_HERO_HAIR_BONE_COUNT 2
#define CC_HERO_SKIN_ASSET \
    "assets/exports/hero/crownless_hero_engine_rig_v01.glb"
#define CC_SCREEN_FIRST_HERO_SKIN_ASSET \
    "assets/exports/hero/crownless_screen_first_engine_rig_v08.glb"
#define CC_BRIDGE_CHECKPOINT_ASSET \
    "assets/exports/glb/environment_bridge_checkpoint_v01.glb"
#define CC_BRIDGE_CHECKPOINT_MESH_BUDGET 96
#define CC_STYLE_GRADE_SHADER "assets/shaders/style_grade.fs"
#define CC_WORLD_LIGHT_VERTEX_SHADER "assets/shaders/world_lit.vs"
#define CC_WORLD_LIGHT_SKINNED_VERTEX_SHADER \
    "assets/shaders/world_lit_skinned.vs"
#define CC_WORLD_LIGHT_FRAGMENT_SHADER "assets/shaders/world_lit.fs"
#define CC_PAINTED_ENVIRONMENT_FRAGMENT_SHADER \
    "assets/shaders/painted_environment.fs"
#define CC_TREE_FOLIAGE_FRAGMENT_SHADER "assets/shaders/tree_foliage.fs"
#define CC_HERO_PIXEL_FRAGMENT_SHADER "assets/shaders/hero_pixel.fs"
#define CC_HERO_INK_STRENGTH 0.52f
#define CC_NPC_INDEXED_FRAGMENT_SHADER "assets/shaders/npc_indexed.fs"
#define CC_NPC_ARCHETYPE_MATERIAL_COUNT 9
#define CC_NPC_ARCHETYPE_LOCOMOTION_POSE_COUNT 8
#define CC_NPC_ARCHETYPE_POSE_COUNT 9
#define CC_NPC_DYNAMIC_HAIR_COUNT 8

typedef enum RuntimeAssetId {
    RUNTIME_ASSET_BRIDGE,
    RUNTIME_ASSET_CARRIAGE,
    RUNTIME_ASSET_CARGO_RACK,
    RUNTIME_ASSET_MARKET,
    RUNTIME_ASSET_MINE,
    RUNTIME_ASSET_SHORTAGE,
    RUNTIME_ASSET_ENFORCEMENT,
    RUNTIME_ASSET_RECOVERY,
    RUNTIME_ASSET_COUNT
} RuntimeAssetId;

typedef struct RuntimeAsset {
    const char *path;
    const char *label;
    int32_t mesh_budget;
    Model model;
    bool ready;
} RuntimeAsset;

static RuntimeAsset runtime_assets[RUNTIME_ASSET_COUNT] = {
    [RUNTIME_ASSET_BRIDGE] = {
        CC_BRIDGE_CHECKPOINT_ASSET, "bridge checkpoint", 96, {0}, false},
    [RUNTIME_ASSET_CARRIAGE] = {
        "assets/exports/glb/carriage_base_v01.glb", "carriage", 144, {0}, false},
    [RUNTIME_ASSET_CARGO_RACK] = {
        "assets/exports/glb/module_cargo_rack_v01.glb", "cargo rack", 48,
        {0}, false},
    [RUNTIME_ASSET_MARKET] = {
        "assets/exports/glb/environment_market_granary_v01.glb",
        "market and granary", 80, {0}, false},
    [RUNTIME_ASSET_MINE] = {
        "assets/exports/glb/environment_mine_entrance_v01.glb",
        "mine entrance", 64, {0}, false},
    [RUNTIME_ASSET_SHORTAGE] = {
        "assets/exports/glb/state_food_shortage_v01.glb",
        "food shortage dressing", 40, {0}, false},
    [RUNTIME_ASSET_ENFORCEMENT] = {
        "assets/exports/glb/state_harsh_enforcement_v01.glb",
        "enforcement dressing", 32, {0}, false},
    [RUNTIME_ASSET_RECOVERY] = {
        "assets/exports/glb/state_market_recovery_v01.glb",
        "market recovery dressing", 40, {0}, false},
};

typedef struct HeroSkinCache {
    Model model;
    ModelAnimation animation;
    Transform pose[CC_HERO_SKIN_MAX_BONES];
    Transform *frames[1];
    int32_t skin_bone[CC_HERO_SKIN_MAX_BONES];
    int32_t cape_bone[CC_HERO_SKIN_MAX_BONES];
    int32_t hair_bone[CC_HERO_SKIN_MAX_BONES];
    bool ready;
} HeroSkinCache;

static HeroSkinCache hero_skin = {0};
static bool screen_first_hero_requested = false;
static bool screen_first_hero_active = false;

#define CC_NPC_BODY_FRAME_COUNT 3
#define CC_NPC_BODY_MUSCLE_COUNT 3
#define CC_NPC_BODY_TISSUE_COUNT 4

typedef struct NpcBodySkinCache {
    Model model;
    ModelAnimation animation;
    Transform pose[CC_HERO_SKIN_MAX_BONES];
    Transform *frames[1];
    int32_t skin_bone[CC_HERO_SKIN_MAX_BONES];
    bool ready;
} NpcBodySkinCache;

static const char *NPC_BODY_FRAME_NAMES[CC_NPC_BODY_FRAME_COUNT] = {
    "lean", "standard", "heavy",
};
static const char *NPC_BODY_MUSCLE_NAMES[CC_NPC_BODY_MUSCLE_COUNT] = {
    "slight", "athletic", "power",
};
static const char *NPC_BODY_TISSUE_NAMES[CC_NPC_BODY_TISSUE_COUNT] = {
    "low", "balanced", "central", "lower_body",
};
static NpcBodySkinCache npc_body_skins
    [CC_NPC_BODY_FRAME_COUNT]
    [CC_NPC_BODY_MUSCLE_COUNT]
    [CC_NPC_BODY_TISSUE_COUNT] = {0};

typedef struct NpcHeadFamilyCache {
    Model model;
    bool ready;
} NpcHeadFamilyCache;

static const char *NPC_HEAD_FAMILY_NAMES[CC_NPC_HEAD_FAMILY_COUNT] = {
    "square", "long", "broad", "veteran",
};
static NpcHeadFamilyCache npc_head_families[CC_NPC_HEAD_FAMILY_COUNT] = {0};

typedef struct NpcHairFamilyCache {
    Model model;
    bool ready;
} NpcHairFamilyCache;

static const char *NPC_HAIR_FAMILY_NAMES[CC_NPC_HAIR_FAMILY_COUNT] = {
    "cropped", "swept", "bob", "crest", "braided", "rear_lock",
};
static NpcHairFamilyCache npc_hair_families[CC_NPC_HAIR_FAMILY_COUNT] = {0};

typedef struct NpcArchetypeCache {
    Model model;
    bool ready;
} NpcArchetypeCache;

static const char *NPC_ARCHETYPE_ROLE_PATH_NAMES[CC_NPC_ROLE_COUNT] = {
    [CC_NPC_ROLE_WAYFARER] = "wayfarer",
    [CC_NPC_ROLE_GUARD] = "guard",
    [CC_NPC_ROLE_RAIDER] = "raider",
    [CC_NPC_ROLE_MERCHANT] = "merchant",
    [CC_NPC_ROLE_LABORER] = "laborer",
    [CC_NPC_ROLE_TRAVELLER] = "traveller",
    [CC_NPC_ROLE_REFUGEE] = "refugee",
    [CC_NPC_ROLE_SCOUT] = "scout",
    [CC_NPC_ROLE_HEALER] = "healer",
};

static const char *NPC_ARCHETYPE_POSE_PATH_SUFFIXES
    [CC_NPC_ARCHETYPE_POSE_COUNT] = {
    "", "_contact_l", "_down_l", "_passing_l", "_up_l",
    "_contact_r", "_down_r", "_passing_r", "_up_r",
};

static NpcArchetypeCache npc_archetypes
    [CC_NPC_ROLE_COUNT][CC_NPC_ARCHETYPE_POSE_COUNT] = {0};

typedef struct CreatureModelCache {
    Model model;
    ModelAnimation animation;
    Transform pose[CC_QUADRUPED_BONE_COUNT];
    Transform *frames[1];
    int32_t quadruped_bone[CC_QUADRUPED_BONE_COUNT];
    bool ready;
} CreatureModelCache;

static CreatureModelCache creature_models
    [CC_CREATURE_VARIANT_COUNT][CC_CREATURE_POSE_COUNT] = {0};

typedef enum CreatureGaitSlot {
    CREATURE_GAIT_ROAD_HORSE_LEFT,
    CREATURE_GAIT_ROAD_HORSE_RIGHT,
    CREATURE_GAIT_ROAD_COW,
    CREATURE_GAIT_STREET_COW,
    CREATURE_GAIT_SLOT_COUNT
} CreatureGaitSlot;

typedef struct CreatureGaitCache {
    CcCreatureRigController controller;
    CcCreatureRigProfile profile;
    float last_clock;
    bool ready;
} CreatureGaitCache;

static CreatureGaitCache creature_gaits[CREATURE_GAIT_SLOT_COUNT] = {0};

/* Physics-driven people use unskinned, offline-generated rigid modules.  Each
   model is instanced against a resolved bone frame, so contact IK and ragdoll
   poses remain authoritative without a per-character vertex upload. */
typedef enum NpcDynamicModuleId {
    NPC_DYNAMIC_TORSO,
    NPC_DYNAMIC_PELVIS,
    NPC_DYNAMIC_UPPER_ARM,
    NPC_DYNAMIC_FOREARM,
    NPC_DYNAMIC_THIGH,
    NPC_DYNAMIC_SHIN,
    NPC_DYNAMIC_HAND,
    NPC_DYNAMIC_FOOT,
    NPC_DYNAMIC_HEAD,
    NPC_DYNAMIC_MANTLE,
    NPC_DYNAMIC_COAT_TAIL,
    NPC_DYNAMIC_CHEST_PLATE,
    NPC_DYNAMIC_PAULDRON,
    NPC_DYNAMIC_APRON,
    NPC_DYNAMIC_PACK,
    NPC_DYNAMIC_SATCHEL,
    NPC_DYNAMIC_HELMET,
    NPC_DYNAMIC_HAT,
    NPC_DYNAMIC_HOOD,
    NPC_DYNAMIC_HEADWRAP,
    NPC_DYNAMIC_TOOL_SHAFT,
    NPC_DYNAMIC_TOOL_HEAD,
    NPC_DYNAMIC_HAIR_0,
    NPC_DYNAMIC_HAIR_1,
    NPC_DYNAMIC_HAIR_2,
    NPC_DYNAMIC_HAIR_3,
    NPC_DYNAMIC_HAIR_4,
    NPC_DYNAMIC_HAIR_5,
    NPC_DYNAMIC_HAIR_6,
    NPC_DYNAMIC_HAIR_7,
    NPC_DYNAMIC_MODULE_COUNT
} NpcDynamicModuleId;

typedef struct NpcDynamicModuleCache {
    const char *path;
    const char *label;
    Model model;
    bool ready;
} NpcDynamicModuleCache;

static NpcDynamicModuleCache npc_dynamic_modules[NPC_DYNAMIC_MODULE_COUNT] = {
    [NPC_DYNAMIC_TORSO] = {
        "assets/exports/npc/npc_module_torso_v01.glb", "torso", {0}, false},
    [NPC_DYNAMIC_PELVIS] = {
        "assets/exports/npc/npc_module_pelvis_v01.glb", "pelvis", {0}, false},
    [NPC_DYNAMIC_UPPER_ARM] = {
        "assets/exports/npc/npc_module_upper_arm_v01.glb", "upper arm", {0}, false},
    [NPC_DYNAMIC_FOREARM] = {
        "assets/exports/npc/npc_module_forearm_v01.glb", "forearm", {0}, false},
    [NPC_DYNAMIC_THIGH] = {
        "assets/exports/npc/npc_module_thigh_v01.glb", "thigh", {0}, false},
    [NPC_DYNAMIC_SHIN] = {
        "assets/exports/npc/npc_module_shin_v01.glb", "shin", {0}, false},
    [NPC_DYNAMIC_HAND] = {
        "assets/exports/npc/npc_module_hand_v01.glb", "hand", {0}, false},
    [NPC_DYNAMIC_FOOT] = {
        "assets/exports/npc/npc_module_foot_v01.glb", "foot", {0}, false},
    [NPC_DYNAMIC_HEAD] = {
        "assets/exports/npc/npc_module_head_v01.glb", "head", {0}, false},
    [NPC_DYNAMIC_MANTLE] = {
        "assets/exports/npc/npc_module_mantle_v01.glb", "mantle", {0}, false},
    [NPC_DYNAMIC_COAT_TAIL] = {
        "assets/exports/npc/npc_module_coat_tail_v01.glb", "coat tail", {0}, false},
    [NPC_DYNAMIC_CHEST_PLATE] = {
        "assets/exports/npc/npc_module_chest_plate_v01.glb", "chest plate", {0}, false},
    [NPC_DYNAMIC_PAULDRON] = {
        "assets/exports/npc/npc_module_pauldron_v01.glb", "pauldron", {0}, false},
    [NPC_DYNAMIC_APRON] = {
        "assets/exports/npc/npc_module_apron_v01.glb", "apron", {0}, false},
    [NPC_DYNAMIC_PACK] = {
        "assets/exports/npc/npc_module_pack_v01.glb", "pack", {0}, false},
    [NPC_DYNAMIC_SATCHEL] = {
        "assets/exports/npc/npc_module_satchel_v01.glb", "satchel", {0}, false},
    [NPC_DYNAMIC_HELMET] = {
        "assets/exports/npc/npc_module_helmet_v01.glb", "helmet", {0}, false},
    [NPC_DYNAMIC_HAT] = {
        "assets/exports/npc/npc_module_hat_v01.glb", "hat", {0}, false},
    [NPC_DYNAMIC_HOOD] = {
        "assets/exports/npc/npc_module_hood_v01.glb", "hood", {0}, false},
    [NPC_DYNAMIC_HEADWRAP] = {
        "assets/exports/npc/npc_module_headwrap_v01.glb", "headwrap", {0}, false},
    [NPC_DYNAMIC_TOOL_SHAFT] = {
        "assets/exports/npc/npc_module_tool_shaft_v01.glb", "tool shaft", {0}, false},
    [NPC_DYNAMIC_TOOL_HEAD] = {
        "assets/exports/npc/npc_module_tool_head_v01.glb", "tool head", {0}, false},
    [NPC_DYNAMIC_HAIR_0] = {
        "assets/exports/npc/npc_module_hair_0_v01.glb", "hair 0", {0}, false},
    [NPC_DYNAMIC_HAIR_1] = {
        "assets/exports/npc/npc_module_hair_1_v01.glb", "hair 1", {0}, false},
    [NPC_DYNAMIC_HAIR_2] = {
        "assets/exports/npc/npc_module_hair_2_v01.glb", "hair 2", {0}, false},
    [NPC_DYNAMIC_HAIR_3] = {
        "assets/exports/npc/npc_module_hair_3_v01.glb", "hair 3", {0}, false},
    [NPC_DYNAMIC_HAIR_4] = {
        "assets/exports/npc/npc_module_hair_4_v01.glb", "hair 4", {0}, false},
    [NPC_DYNAMIC_HAIR_5] = {
        "assets/exports/npc/npc_module_hair_5_v01.glb", "hair 5", {0}, false},
    [NPC_DYNAMIC_HAIR_6] = {
        "assets/exports/npc/npc_module_hair_6_v01.glb", "hair 6", {0}, false},
    [NPC_DYNAMIC_HAIR_7] = {
        "assets/exports/npc/npc_module_hair_7_v01.glb", "hair 7", {0}, false},
};

static Matrix NpcModuleTransform(Vector3 origin, Vector3 right, Vector3 up,
                                 Vector3 forward, Vector3 scale);
static bool DrawNpcDynamicModule(NpcDynamicModuleId id, Matrix transform,
                                 Color color);
static bool DrawNpcHairFamily(const CcNpcAppearance *appearance,
                              Matrix transform, Matrix fallback_transform);

static NpcDynamicModuleId NpcHeadwearModule(uint8_t style)
{
    static const NpcDynamicModuleId modules[4] = {
        NPC_DYNAMIC_HELMET, NPC_DYNAMIC_HAT,
        NPC_DYNAMIC_HOOD, NPC_DYNAMIC_HEADWRAP,
    };
    return modules[style % 4U];
}

typedef struct PaintedEnvironmentStyle {
    Shader shader;
    int32_t light_direction_location;
    int32_t light_color_location;
    int32_t ambient_color_location;
    int32_t camera_position_location;
    int32_t camera_forward_location;
    int32_t shadow_color_location;
    int32_t fog_color_location;
    int32_t fog_near_location;
    int32_t fog_far_location;
    int32_t focal_point_location;
    int32_t story_axis_location;
    int32_t foreground_anchor_location;
    int32_t depth_splits_location;
    int32_t depth_strength_location;
    int32_t focal_contrast_location;
    int32_t reveal_cut_height_location;
    int32_t foreground_reveal_location;
    bool ready;
} PaintedEnvironmentStyle;

typedef struct VisualStyleCache {
    Shader grade;
    Texture2D palette_lut;
    int32_t palette_lut_location;
    int32_t grade_exposure_location;
    int32_t grade_shadow_tone_location;
    int32_t grade_highlight_tone_location;
    int32_t grade_chroma_location;
    bool grade_ready;
    Shader world;
    int32_t light_direction_location;
    int32_t light_color_location;
    int32_t ambient_color_location;
    int32_t camera_position_location;
    int32_t camera_forward_location;
    int32_t shadow_color_location;
    int32_t fog_color_location;
    int32_t fog_near_location;
    int32_t fog_far_location;
    int32_t focal_point_location;
    int32_t story_axis_location;
    int32_t foreground_anchor_location;
    int32_t depth_splits_location;
    int32_t depth_strength_location;
    int32_t focal_contrast_location;
    int32_t reveal_cut_height_location;
    int32_t foreground_reveal_location;
    int32_t terrain_surface_location;
    int32_t weather_wetness_location;
    bool world_ready;
    PaintedEnvironmentStyle painted_environment;
    Shader foliage;
    int32_t foliage_light_direction_location;
    int32_t foliage_light_color_location;
    int32_t foliage_ambient_color_location;
    int32_t foliage_camera_position_location;
    int32_t foliage_camera_forward_location;
    int32_t foliage_shadow_color_location;
    int32_t foliage_fog_color_location;
    int32_t foliage_fog_near_location;
    int32_t foliage_fog_far_location;
    int32_t foliage_depth_splits_location;
    int32_t foliage_depth_strength_location;
    bool foliage_ready;
    Shader hero;
    int32_t hero_light_direction_location;
    int32_t hero_camera_position_location;
    int32_t hero_shadow_color_location;
    int32_t hero_fog_color_location;
    int32_t hero_fog_near_location;
    int32_t hero_fog_far_location;
    int32_t hero_ink_strength_location;
    bool hero_ready;
    Shader npc;
    int32_t npc_light_direction_location;
    int32_t npc_camera_position_location;
    int32_t npc_shadow_color_location;
    int32_t npc_fog_color_location;
    int32_t npc_fog_near_location;
    int32_t npc_fog_far_location;
    int32_t npc_ink_strength_location;
    int32_t npc_palette_location;
    int32_t npc_palette_ink_location;
    int32_t npc_hero_emphasis_location;
    int32_t npc_hero_head_position_location;
    bool npc_ready;
    Shader npc_skinned;
    int32_t npc_skinned_light_direction_location;
    int32_t npc_skinned_camera_position_location;
    int32_t npc_skinned_shadow_color_location;
    int32_t npc_skinned_fog_color_location;
    int32_t npc_skinned_fog_near_location;
    int32_t npc_skinned_fog_far_location;
    int32_t npc_skinned_ink_strength_location;
    int32_t npc_skinned_palette_location;
    int32_t npc_skinned_palette_ink_location;
    int32_t npc_skinned_hero_emphasis_location;
    int32_t npc_skinned_hero_head_position_location;
    int32_t npc_skinned_body_skin_remap_location;
    bool npc_skinned_ready;
    ArtAtmosphereDefinition presentation_atmosphere;
} VisualStyleCache;

static VisualStyleCache visual_style = {0};

#define CC_SHARED_PALETTE_LUT_SIZE 64
#define CC_SHARED_PALETTE_LUT_TILES 8
#define CC_FINAL_PALETTE_MAX_COLORS 64
#define CC_HERO_RETRO_MATERIAL_COUNT 19

typedef enum FinalPaletteOwnership {
    FINAL_PALETTE_ENVIRONMENT = 0,
    FINAL_PALETTE_PROTECTED
} FinalPaletteOwnership;

typedef struct FinalPaletteEntry {
    Color color;
    FinalPaletteOwnership ownership;
} FinalPaletteEntry;

/* Color values live only in cc_visual_style.h. This list owns semantic
   membership: broad materials may fill the world, while character and signal
   pigments receive smaller lookup regions so scenery cannot casually steal
   the colors that identify the hero, combat, or an interaction. */
static void AddFinalPaletteColor(FinalPaletteEntry *entries, int32_t capacity,
                                 int32_t *count, Color color,
                                 FinalPaletteOwnership ownership)
{
    if (entries == NULL || count == NULL) return;
    for (int32_t index = 0; index < *count; ++index) {
        Color current = entries[index].color;
        if (current.r != color.r || current.g != color.g ||
            current.b != color.b || current.a != color.a) continue;
        if (ownership == FINAL_PALETTE_PROTECTED) {
            entries[index].ownership = ownership;
        }
        return;
    }
    if (*count >= capacity) return;
    entries[*count] = (FinalPaletteEntry){color, ownership};
    *count += 1;
}

static void AddFinalPaletteRamp(FinalPaletteEntry *entries, int32_t capacity,
                                int32_t *count, CcStyleRamp ramp,
                                FinalPaletteOwnership ownership)
{
    AddFinalPaletteColor(entries, capacity, count, ramp.shadow, ownership);
    AddFinalPaletteColor(entries, capacity, count, ramp.base, ownership);
    AddFinalPaletteColor(entries, capacity, count, ramp.light, ownership);
}

static int32_t BuildFinalPalette(FinalPaletteEntry *entries, int32_t capacity)
{
    int32_t count = 0;
    /* The grade is applied before the HUD is drawn. Keep interface neutrals
       out of this lookup so stone and fog cannot snap to panel or text
       colors. Two authored inks give the world cool outdoor and warm indoor
       shadow anchors. */
    AddFinalPaletteColor(entries, capacity, &count,
                         CC_VISUAL_PALETTE.cool_ink,
                         FINAL_PALETTE_ENVIRONMENT);
    AddFinalPaletteColor(entries, capacity, &count,
                         CC_VISUAL_PALETTE.warm_ink,
                         FINAL_PALETTE_ENVIRONMENT);

    AddFinalPaletteRamp(entries, capacity, &count, CC_VISUAL_PALETTE.earth,
                        FINAL_PALETTE_ENVIRONMENT);
    AddFinalPaletteRamp(entries, capacity, &count, CC_VISUAL_PALETTE.road,
                        FINAL_PALETTE_ENVIRONMENT);
    AddFinalPaletteRamp(entries, capacity, &count, CC_VISUAL_PALETTE.wood,
                        FINAL_PALETTE_ENVIRONMENT);
    AddFinalPaletteRamp(entries, capacity, &count, CC_VISUAL_PALETTE.stone,
                        FINAL_PALETTE_ENVIRONMENT);
    AddFinalPaletteRamp(entries, capacity, &count, CC_VISUAL_PALETTE.grass,
                        FINAL_PALETTE_ENVIRONMENT);
    AddFinalPaletteRamp(entries, capacity, &count, CC_VISUAL_PALETTE.foliage,
                        FINAL_PALETTE_ENVIRONMENT);
    AddFinalPaletteRamp(entries, capacity, &count, CC_VISUAL_PALETTE.crop,
                        FINAL_PALETTE_ENVIRONMENT);
    AddFinalPaletteRamp(entries, capacity, &count, CC_VISUAL_PALETTE.metal,
                        FINAL_PALETTE_ENVIRONMENT);
    AddFinalPaletteRamp(entries, capacity, &count, CC_VISUAL_PALETTE.parchment,
                        FINAL_PALETTE_ENVIRONMENT);
    AddFinalPaletteRamp(entries, capacity, &count,
                        CC_VISUAL_PALETTE.contraband,
                        FINAL_PALETTE_ENVIRONMENT);

    AddFinalPaletteRamp(entries, capacity, &count, CC_VISUAL_PALETTE.teal,
                        FINAL_PALETTE_PROTECTED);
    AddFinalPaletteRamp(entries, capacity, &count, CC_VISUAL_PALETTE.gold,
                        FINAL_PALETTE_PROTECTED);
    AddFinalPaletteRamp(entries, capacity, &count, CC_VISUAL_PALETTE.danger,
                        FINAL_PALETTE_PROTECTED);
    AddFinalPaletteRamp(entries, capacity, &count, CC_VISUAL_PALETTE.violet,
                        FINAL_PALETTE_PROTECTED);
    AddFinalPaletteRamp(entries, capacity, &count,
                        CC_VISUAL_PALETTE.people_skin,
                        FINAL_PALETTE_PROTECTED);

    AddFinalPaletteColor(entries, capacity, &count,
                         CC_STYLE_HERO_SKIN_SHADOW,
                         FINAL_PALETTE_PROTECTED);
    AddFinalPaletteColor(entries, capacity, &count, CC_STYLE_HERO_SKIN,
                         FINAL_PALETTE_PROTECTED);
    AddFinalPaletteColor(entries, capacity, &count, CC_STYLE_HERO_SKIN_LIGHT,
                         FINAL_PALETTE_PROTECTED);
    AddFinalPaletteColor(entries, capacity, &count, CC_STYLE_HERO_HAIR,
                         FINAL_PALETTE_PROTECTED);
    AddFinalPaletteColor(entries, capacity, &count, CC_STYLE_HERO_UNDERLAYER,
                         FINAL_PALETTE_PROTECTED);
    AddFinalPaletteColor(entries, capacity, &count, CC_STYLE_HERO_OUTER,
                         FINAL_PALETTE_PROTECTED);
    AddFinalPaletteColor(entries, capacity, &count, CC_STYLE_HERO_TROUSERS,
                         FINAL_PALETTE_PROTECTED);
    AddFinalPaletteColor(entries, capacity, &count, CC_STYLE_HERO_LEATHER,
                         FINAL_PALETTE_PROTECTED);
    AddFinalPaletteColor(entries, capacity, &count, CC_STYLE_HERO_METAL,
                         FINAL_PALETTE_PROTECTED);
    AddFinalPaletteColor(entries, capacity, &count, CC_STYLE_HERO_ACCENT,
                         FINAL_PALETTE_PROTECTED);
    AddFinalPaletteColor(entries, capacity, &count, CC_STYLE_HERO_PANEL_INK,
                         FINAL_PALETTE_PROTECTED);
    return count;
}

/* Material order is part of the engine-hero export contract. The fallback
   renderer resolves each slot from named roles instead of owning another
   independent list of RGB values. */
static Color HeroRetroMaterialColor(int32_t material)
{
    switch (material) {
        case 0: return CC_STYLE_HERO_TROUSERS;
        case 1: return CC_STYLE_HERO_SKIN;
        case 2: return CC_STYLE_HERO_SKIN_LIGHT;
        case 3:
        case 4: return CC_STYLE_HERO_HAIR;
        case 5: return CC_STYLE_METAL;
        case 6:
        case 12: return CC_STYLE_METAL_SHADOW;
        case 7: return CC_STYLE_TEAL_SHADOW;
        case 8: return CC_STYLE_HERO_UNDERLAYER;
        case 9: return CC_STYLE_HERO_ACCENT;
        case 10: return CC_STYLE_HERO_OUTER;
        case 11:
        case 13: return CC_STYLE_HERO_METAL;
        case 14: return CC_STYLE_DANGER;
        case 15: return CC_STYLE_HERO_LEATHER;
        case 16: return CC_STYLE_METAL;
        case 17: return CC_STYLE_METAL_LIGHT;
        case 18: return CC_STYLE_WOOD;
        default: return CC_STYLE_HERO_TROUSERS;
    }
}

static const Vector3 HERO_REST_DIRECTIONS[CC_HUMANOID_SKIN_BONE_COUNT] = {
    {0.0f, 1.0f, 0.0f},
    {0.0f, 1.0f, 0.0f},
    {0.0f, 1.0f, 0.0f},
    {0.0f, 1.0f, 0.0f},
    {0.0f, 1.0f, 0.0f},
    {0.0f, 1.0f, 0.0f},
    {-0.469f, -0.883f, 0.0f},
    {-0.325f, -0.945f, 0.044f},
    {-0.061f, -0.979f, 0.184f},
    {0.469f, -0.883f, 0.0f},
    {0.325f, -0.945f, 0.044f},
    {0.061f, -0.979f, 0.184f},
    {-0.023f, -1.0f, 0.0f},
    {0.0f, -1.0f, 0.0f},
    {0.0f, -0.182f, 0.983f},
    {0.023f, -1.0f, 0.0f},
    {0.0f, -1.0f, 0.0f},
    {0.0f, -0.182f, 0.983f},
};

static const char *HERO_CAPE_BONE_NAMES[CC_HERO_CAPE_BONE_COUNT] = {
    "cape.0", "cape.1", "cape.2", "cape.3",
};

static const Vector3 HERO_CAPE_REST_DIRECTIONS[CC_HERO_CAPE_BONE_COUNT] = {
    {0.0f, -0.989f, -0.146f},
    {0.0f, -0.986f, -0.164f},
    {0.0f, -0.984f, -0.176f},
    {0.0f, -0.984f, -0.176f},
};

static const char *HERO_HAIR_BONE_NAMES[CC_HERO_HAIR_BONE_COUNT] = {
    "hair.long", "hair.rear",
};

/* Blender exports +Y as engine -Z. These offsets start at the head bone and
   place each control at the broad root of its clump. */
static const Vector3 HERO_HAIR_ROOT_OFFSETS[CC_HERO_HAIR_BONE_COUNT] = {
    {-0.18f, 0.39f, 0.08f},
    {0.00f, 0.38f, -0.12f},
};

static const Vector3 HERO_HAIR_REST_DIRECTIONS[CC_HERO_HAIR_BONE_COUNT] = {
    {-0.133f, -0.986f, -0.106f},
    {0.000f, -0.979f, -0.202f},
};

static const float HERO_HAIR_TIP_DELAY[CC_HERO_HAIR_BONE_COUNT] = {
    0.12f, 0.18f,
};

static int32_t HeroCapeBoneFind(const char *name)
{
    for (int32_t bone = 0; bone < CC_HERO_CAPE_BONE_COUNT; ++bone) {
        if (strcmp(name, HERO_CAPE_BONE_NAMES[bone]) == 0) return bone;
    }
    return -1;
}

static int32_t HeroHairBoneFind(const char *name)
{
    for (int32_t bone = 0; bone < CC_HERO_HAIR_BONE_COUNT; ++bone) {
        if (strcmp(name, HERO_HAIR_BONE_NAMES[bone]) == 0) return bone;
    }
    return -1;
}

static Quaternion HeroRotationBetween(Vector3 from, Vector3 to)
{
    from = Vector3Normalize(from);
    to = Vector3Normalize(to);
    float cosine = Vector3DotProduct(from, to);
    if (cosine > -0.9995f) return QuaternionFromVector3ToVector3(from, to);
    Vector3 axis = Vector3CrossProduct(from, (Vector3){1.0f, 0.0f, 0.0f});
    if (Vector3LengthSqr(axis) < 0.0001f) {
        axis = Vector3CrossProduct(from, (Vector3){0.0f, 0.0f, 1.0f});
    }
    return QuaternionFromAxisAngle(Vector3Normalize(axis), PI);
}

static const char *HeroSkinAssetPath(void)
{
    const char *experiment = getenv("CC_SCREEN_FIRST_HERO");
    screen_first_hero_active = screen_first_hero_requested ||
        (experiment != NULL && experiment[0] != '\0' &&
         strcmp(experiment, "0") != 0);
    const char *relative_path = screen_first_hero_active ?
        CC_SCREEN_FIRST_HERO_SKIN_ASSET : CC_HERO_SKIN_ASSET;
    if (FileExists(relative_path)) return relative_path;
    static char bundled_path[1024];
    (void)snprintf(bundled_path, sizeof(bundled_path),
                   "%s/../Resources/%s", GetApplicationDirectory(),
                   relative_path);
    return FileExists(bundled_path) ? bundled_path : NULL;
}

static bool ResolveAssetPath(const char *relative_path, char *resolved,
                             size_t capacity)
{
    if (relative_path == NULL || resolved == NULL || capacity == 0U) {
        return false;
    }
    if (FileExists(relative_path)) {
        (void)snprintf(resolved, capacity, "%s", relative_path);
        return true;
    }
#if defined(CC_ASSET_SOURCE_ROOT)
    (void)snprintf(resolved, capacity, "%s/%s", CC_ASSET_SOURCE_ROOT,
                   relative_path);
    if (FileExists(resolved)) return true;
#endif
    (void)snprintf(resolved, capacity, "../%s", relative_path);
    if (FileExists(resolved)) return true;
    (void)snprintf(resolved, capacity, "%s/../Resources/%s",
                   GetApplicationDirectory(), relative_path);
    return FileExists(resolved);
}

static const char *BridgeCheckpointAssetPath(void)
{
    static char resolved[1024];
    return ResolveAssetPath(CC_BRIDGE_CHECKPOINT_ASSET, resolved,
                            sizeof(resolved)) ? resolved : NULL;
}

static bool RoadUsesAuthoredCheckpoint(void)
{
    if (bridge_checkpoint_status == BRIDGE_CHECKPOINT_UNKNOWN) {
        bridge_checkpoint_status = BridgeCheckpointAssetPath() != NULL ?
            BRIDGE_CHECKPOINT_AVAILABLE : BRIDGE_CHECKPOINT_UNAVAILABLE;
    }
    return bridge_checkpoint_status == BRIDGE_CHECKPOINT_AVAILABLE;
}

static int32_t RoadObstacleCount(void)
{
    return RoadUsesAuthoredCheckpoint() ?
        (int32_t)(sizeof(ROAD_OBSTACLES) / sizeof(ROAD_OBSTACLES[0])) :
        (int32_t)(sizeof(ROAD_FALLBACK_OBSTACLES) /
                  sizeof(ROAD_FALLBACK_OBSTACLES[0]));
}

static Rectangle RoadObstacleAt(int32_t index)
{
    return RoadUsesAuthoredCheckpoint() ? ROAD_OBSTACLES[index] :
                                          ROAD_FALLBACK_OBSTACLES[index];
}

static bool LoadRuntimeAsset(RuntimeAssetId id)
{
    if (id < 0 || id >= RUNTIME_ASSET_COUNT) return false;
    RuntimeAsset *asset = &runtime_assets[id];
    char resolved[1024];
    if (!ResolveAssetPath(asset->path, resolved, sizeof(resolved))) {
        TraceLog(LOG_WARNING, "ASSET: %s was not found; using fallback",
                 asset->label);
        return false;
    }
    asset->model = LoadModel(resolved);
    int32_t mesh_count = asset->model.meshCount;
    if (mesh_count <= 0 || mesh_count > asset->mesh_budget) {
        TraceLog(LOG_WARNING,
                 "ASSET: invalid %s (%d meshes, budget %d); using fallback",
                 asset->label, mesh_count, asset->mesh_budget);
        if (mesh_count > 0) UnloadModel(asset->model);
        asset->model = (Model){0};
        return false;
    }
    asset->ready = true;
    TraceLog(LOG_INFO, "ASSET: loaded %s (%d meshes)", asset->label,
             mesh_count);
    return true;
}

static void LoadNpcArchetypes(void)
{
    for (int32_t role = 0; role < CC_NPC_ROLE_COUNT; ++role) {
        for (int32_t pose = 0; pose < CC_NPC_ARCHETYPE_POSE_COUNT; ++pose) {
            char path[256];
            (void)snprintf(path, sizeof(path),
                           "assets/exports/npc/npc_%s%s_v01.glb",
                           NPC_ARCHETYPE_ROLE_PATH_NAMES[role],
                           NPC_ARCHETYPE_POSE_PATH_SUFFIXES[pose]);
            char resolved[1024];
            if (!ResolveAssetPath(path, resolved, sizeof(resolved))) {
                TraceLog(LOG_WARNING, "NPC: archetype %s pose %d was not found",
                         CcNpcRoleName((CcNpcRole)role), pose);
                continue;
            }
            Model model = LoadModel(resolved);
            if (model.meshCount != 1 || model.materialCount < 1) {
                TraceLog(LOG_WARNING,
                         "NPC: invalid %s pose %d (%d meshes, %d materials)",
                         CcNpcRoleName((CcNpcRole)role), pose,
                         model.meshCount, model.materialCount);
                if (model.meshCount > 0) UnloadModel(model);
                continue;
            }
            npc_archetypes[role][pose].model = model;
            npc_archetypes[role][pose].ready = true;
        }
        TraceLog(LOG_INFO, "NPC: loaded %s stepped archetype",
                 CcNpcRoleName((CcNpcRole)role));
    }
}

static void LoadCreatureModels(void)
{
    for (int32_t variant = 0; variant < CC_CREATURE_VARIANT_COUNT;
         ++variant) {
        const CcCreatureDefinition *definition = CcCreatureDefinitionAt(
            (CcCreatureVariant)variant);
        int32_t loaded_count = 0;
        for (int32_t pose = 0; pose < CC_CREATURE_POSE_COUNT; ++pose) {
            const char *path = CcCreatureAssetPath(
                (CcCreatureVariant)variant, (CcCreaturePose)pose);
            if (path == NULL) continue;
            char resolved[1024];
            if (!ResolveAssetPath(path, resolved, sizeof(resolved))) {
                TraceLog(LOG_WARNING, "CREATURE: %s was not found", path);
                continue;
            }
            Model model = LoadModel(resolved);
            int32_t expected_bones = definition != NULL && definition->skinned ?
                                     CC_QUADRUPED_BONE_COUNT : 0;
            if (model.meshCount != 1 || model.materialCount < 1 ||
                model.skeleton.boneCount != expected_bones) {
                TraceLog(LOG_WARNING,
                         "CREATURE: invalid %s (%d meshes, %d materials, %d bones)",
                         path, model.meshCount, model.materialCount,
                         model.skeleton.boneCount);
                if (model.meshCount > 0) UnloadModel(model);
                continue;
            }
            CreatureModelCache *cached = &creature_models[variant][pose];
            cached->model = model;
            if (expected_bones > 0) {
                bool found[CC_QUADRUPED_BONE_COUNT] = {false};
                bool valid_skin = true;
                for (int32_t bone = 0; bone < expected_bones; ++bone) {
                    int32_t quadruped_bone = CcQuadrupedBoneFind(
                        model.skeleton.bones[bone].name);
                    cached->quadruped_bone[bone] = quadruped_bone;
                    if (quadruped_bone < 0) {
                        valid_skin = false;
                    } else {
                        found[quadruped_bone] = true;
                    }
                }
                for (int32_t bone = 0; bone < CC_QUADRUPED_BONE_COUNT;
                     ++bone) {
                    if (!found[bone]) valid_skin = false;
                }
                if (!valid_skin) {
                    TraceLog(LOG_WARNING,
                             "CREATURE: %s has the wrong quadruped bones",
                             path);
                    UnloadModel(cached->model);
                    *cached = (CreatureModelCache){0};
                    continue;
                }
                cached->frames[0] = cached->pose;
                cached->animation.boneCount = expected_bones;
                cached->animation.keyframeCount = 1;
                cached->animation.keyframePoses = cached->frames;
                (void)snprintf(cached->animation.name,
                               sizeof(cached->animation.name),
                               "quadruped-runtime");
            }
            cached->ready = true;
            loaded_count += 1;
        }
        TraceLog(LOG_INFO, "CREATURE: loaded %s (%d/%d poses)",
                 definition != NULL ? definition->name : "unknown",
                 loaded_count,
                 CcCreaturePoseCount((CcCreatureVariant)variant));
    }
}

static void LoadNpcDynamicModules(void)
{
    int32_t loaded_count = 0;
    for (int32_t id = 0; id < NPC_DYNAMIC_MODULE_COUNT; ++id) {
        NpcDynamicModuleCache *module = &npc_dynamic_modules[id];
        char resolved[1024];
        if (!ResolveAssetPath(module->path, resolved, sizeof(resolved))) {
            TraceLog(LOG_WARNING, "NPC MODULE: %s was not found",
                     module->label);
            continue;
        }
        Model model = LoadModel(resolved);
        if (model.meshCount != 1 || model.materialCount < 1 ||
            model.skeleton.boneCount != 0) {
            TraceLog(LOG_WARNING,
                     "NPC MODULE: invalid %s (%d meshes, %d materials, %d bones)",
                     module->label, model.meshCount, model.materialCount,
                     model.skeleton.boneCount);
            if (model.meshCount > 0) UnloadModel(model);
            continue;
        }
        module->model = model;
        module->ready = true;
        loaded_count += 1;
    }
    TraceLog(LOG_INFO,
             "NPC MODULE: loaded %d/%d rigid biomechanical modules",
             loaded_count, NPC_DYNAMIC_MODULE_COUNT);
}

static void LoadNpcBodySkins(void)
{
    int32_t loaded_count = 0;
    for (int32_t frame = 0; frame < CC_NPC_BODY_FRAME_COUNT; ++frame) {
        for (int32_t muscle = 0; muscle < CC_NPC_BODY_MUSCLE_COUNT; ++muscle) {
            for (int32_t tissue = 0; tissue < CC_NPC_BODY_TISSUE_COUNT;
                 ++tissue) {
                NpcBodySkinCache *body =
                    &npc_body_skins[frame][muscle][tissue];
                char path[256];
                (void)snprintf(
                    path, sizeof(path),
                    "assets/exports/world_kit/wk_body_skin_%s_%s_%s_v01.glb",
                    NPC_BODY_FRAME_NAMES[frame],
                    NPC_BODY_MUSCLE_NAMES[muscle],
                    NPC_BODY_TISSUE_NAMES[tissue]);
                char resolved[1024];
                if (!ResolveAssetPath(path, resolved, sizeof(resolved))) {
                    TraceLog(LOG_WARNING, "NPC BODY: %s was not found", path);
                    continue;
                }
                body->model = LoadModel(resolved);
                int32_t bone_count = body->model.skeleton.boneCount;
                if (body->model.meshCount <= 0 ||
                    body->model.meshCount > 2 ||
                    body->model.materialCount < 1 ||
                    bone_count <= 0 || bone_count > CC_HERO_SKIN_MAX_BONES) {
                    TraceLog(LOG_WARNING,
                             "NPC BODY: invalid %s (%d meshes, %d bones)",
                             path, body->model.meshCount, bone_count);
                    if (body->model.meshCount > 0) UnloadModel(body->model);
                    *body = (NpcBodySkinCache){0};
                    continue;
                }
                bool found[CC_HUMANOID_SKIN_BONE_COUNT] = {false};
                for (int32_t bone = 0; bone < bone_count; ++bone) {
                    int32_t skin_bone = CcHumanoidSkinBoneFind(
                        body->model.skeleton.bones[bone].name);
                    body->skin_bone[bone] = skin_bone;
                    if (skin_bone >= 0) found[skin_bone] = true;
                }
                bool complete = true;
                for (int32_t bone = 0;
                     bone < CC_HUMANOID_SKIN_BONE_COUNT; ++bone) {
                    if (found[bone]) continue;
                    TraceLog(LOG_WARNING,
                             "NPC BODY: %s is missing bone %s", path,
                             CcHumanoidSkinBoneName(
                                 (CcHumanoidSkinBone)bone));
                    complete = false;
                }
                if (!complete) {
                    UnloadModel(body->model);
                    *body = (NpcBodySkinCache){0};
                    continue;
                }
                body->frames[0] = body->pose;
                body->animation.boneCount = bone_count;
                body->animation.keyframeCount = 1;
                body->animation.keyframePoses = body->frames;
                (void)snprintf(body->animation.name,
                               sizeof(body->animation.name),
                               "physics-body");
                body->ready = true;
                loaded_count += 1;
            }
        }
    }
    TraceLog(LOG_INFO, "NPC BODY: loaded %d/36 continuous body recipes",
             loaded_count);
}

static void LoadNpcHeadFamilies(void)
{
    int32_t loaded_count = 0;
    for (int32_t family = 0; family < CC_NPC_HEAD_FAMILY_COUNT; ++family) {
        char path[192];
        (void)snprintf(path, sizeof(path),
                       "assets/exports/world_kit/wk_head_%s_v01.glb",
                       NPC_HEAD_FAMILY_NAMES[family]);
        char resolved[1024];
        if (!ResolveAssetPath(path, resolved, sizeof(resolved))) {
            TraceLog(LOG_WARNING, "NPC HEAD: %s was not found", path);
            continue;
        }
        Model model = LoadModel(resolved);
        if (model.meshCount < 2 || model.meshCount > 7 ||
            model.materialCount < 1 || model.skeleton.boneCount != 0) {
            TraceLog(LOG_WARNING,
                     "NPC HEAD: invalid %s (%d meshes, %d bones)",
                     path, model.meshCount, model.skeleton.boneCount);
            if (model.meshCount > 0) UnloadModel(model);
            continue;
        }
        npc_head_families[family].model = model;
        npc_head_families[family].ready = true;
        loaded_count += 1;
    }
    TraceLog(LOG_INFO, "NPC HEAD: loaded %d/%d modular head families",
             loaded_count, CC_NPC_HEAD_FAMILY_COUNT);
}

static void LoadNpcHairFamilies(void)
{
    int32_t loaded_count = 0;
    for (int32_t family = 0; family < CC_NPC_HAIR_FAMILY_COUNT; ++family) {
        char path[192];
        (void)snprintf(path, sizeof(path),
                       "assets/exports/world_kit/wk_hair_%s_v01.glb",
                       NPC_HAIR_FAMILY_NAMES[family]);
        char resolved[1024];
        if (!ResolveAssetPath(path, resolved, sizeof(resolved))) {
            TraceLog(LOG_WARNING, "NPC HAIR: %s was not found", path);
            continue;
        }
        Model model = LoadModel(resolved);
        if (model.meshCount < 4 || model.meshCount > 8 ||
            model.materialCount < 1 || model.skeleton.boneCount != 0) {
            TraceLog(LOG_WARNING,
                     "NPC HAIR: invalid %s (%d meshes, %d bones)", path,
                     model.meshCount, model.skeleton.boneCount);
            if (model.meshCount > 0) UnloadModel(model);
            continue;
        }
        npc_hair_families[family].model = model;
        npc_hair_families[family].ready = true;
        loaded_count += 1;
    }
    TraceLog(LOG_INFO, "NPC HAIR: loaded %d/%d molded hair families",
             loaded_count, CC_NPC_HAIR_FAMILY_COUNT);
}

static void LoadRuntimeAssets(void)
{
    for (int32_t id = 0; id < RUNTIME_ASSET_COUNT; ++id) {
        bool loaded = LoadRuntimeAsset((RuntimeAssetId)id);
        if (id == RUNTIME_ASSET_BRIDGE) {
            bridge_checkpoint_status = loaded ? BRIDGE_CHECKPOINT_AVAILABLE :
                                                BRIDGE_CHECKPOINT_UNAVAILABLE;
        }
    }
}

static void ApplyWorldShader(Model *model)
{
    if (!visual_style.world_ready || model == NULL || model->materials == NULL) {
        return;
    }
    for (int32_t material = 0; material < model->materialCount; ++material) {
        model->materials[material].shader = visual_style.world;
    }
}

static void ApplyPaintedEnvironmentShader(Model *model)
{
    if (model == NULL || model->materials == NULL) return;
    Shader shader = visual_style.painted_environment.ready ?
        visual_style.painted_environment.shader : visual_style.world;
    for (int32_t material = 0; material < model->materialCount; ++material) {
        model->materials[material].shader = shader;
    }
}

static void ApplyTreeFoliageShader(Model *model)
{
    if (model == NULL || model->materials == NULL) return;
    Shader shader = visual_style.foliage_ready ? visual_style.foliage :
                                                visual_style.world;
    for (int32_t material = 0; material < model->materialCount; ++material) {
        model->materials[material].shader = shader;
    }
}

static void ApplyHeroStyle(Model *model)
{
    if (model == NULL || model->materials == NULL) return;
    if (screen_first_hero_active) {
        for (int32_t material = 0; material < model->materialCount;
             ++material) {
            model->materials[material].shader = visual_style.hero_ready ?
                                                 visual_style.hero :
                                                 visual_style.world;
        }
        return;
    }
    int32_t material_count = model->materialCount <
                             CC_HERO_RETRO_MATERIAL_COUNT ?
                             model->materialCount :
                             CC_HERO_RETRO_MATERIAL_COUNT;
    for (int32_t material = 0; material < material_count; ++material) {
        model->materials[material].maps[MATERIAL_MAP_DIFFUSE].color =
            HeroRetroMaterialColor(material);
        model->materials[material].shader = visual_style.hero_ready ?
                                             visual_style.hero :
                                             visual_style.world;
    }
    for (int32_t material = material_count;
         material < model->materialCount; ++material) {
        model->materials[material].shader = visual_style.hero_ready ?
                                             visual_style.hero :
                                             visual_style.world;
    }
    if (model->materialCount < CC_HERO_RETRO_MATERIAL_COUNT) {
        TraceLog(LOG_WARNING,
                 "HERO: retro palette expected %d materials, found %d",
                 CC_HERO_RETRO_MATERIAL_COUNT, model->materialCount);
    }
}

static void ApplyNpcStyle(Model *model)
{
    if (model == NULL || model->materials == NULL) return;
    for (int32_t material = 0; material < model->materialCount; ++material) {
        model->materials[material].shader = visual_style.npc_ready ?
                                             visual_style.npc :
                                             visual_style.hero_ready ?
                                                visual_style.hero :
                                                visual_style.world;
    }
}

static void ApplyNeutralNpcStyle(Model *model)
{
    ApplyNpcStyle(model);
    if (model == NULL || model->materials == NULL) return;
    /* COLOR_0 stores the semantic palette slot. The shader supplies runtime
       color variation, so these rigid pieces stay neutral for their lifetime
       instead of rewriting raylib material state for every body part. */
    for (int32_t material = 0; material < model->materialCount; ++material) {
        model->materials[material].maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
    }
}

static void ApplyNpcBodyStyle(Model *model)
{
    if (model == NULL || model->materials == NULL) return;
    Shader shader = visual_style.npc_skinned_ready ?
        visual_style.npc_skinned : visual_style.npc;
    for (int32_t material = 0; material < model->materialCount; ++material) {
        model->materials[material].shader = shader;
    }
}

static void SetIndexedPalette(
    const Color colors[CC_NPC_ARCHETYPE_MATERIAL_COUNT],
    float ink_strength, bool hero_emphasis, Vector3 hero_head_position)
{
    if (!visual_style.npc_ready || colors == NULL) return;
    float palette[CC_NPC_ARCHETYPE_MATERIAL_COUNT * 4];
    for (int32_t index = 0; index < CC_NPC_ARCHETYPE_MATERIAL_COUNT;
         ++index) {
        palette[index * 4 + 0] = (float)colors[index].r / 255.0f;
        palette[index * 4 + 1] = (float)colors[index].g / 255.0f;
        palette[index * 4 + 2] = (float)colors[index].b / 255.0f;
        palette[index * 4 + 3] = (float)colors[index].a / 255.0f;
    }
    static const float material_ink[CC_NPC_ARCHETYPE_MATERIAL_COUNT] = {
        0.52f, 0.88f, 0.62f, 0.58f, 0.68f,
        0.76f, 0.46f, 0.60f, 0.96f,
    };
    SetShaderValueV(visual_style.npc, visual_style.npc_palette_location,
                    palette, SHADER_UNIFORM_VEC4,
                    CC_NPC_ARCHETYPE_MATERIAL_COUNT);
    SetShaderValueV(visual_style.npc,
                    visual_style.npc_palette_ink_location,
                    material_ink, SHADER_UNIFORM_FLOAT,
                    CC_NPC_ARCHETYPE_MATERIAL_COUNT);
    SetShaderValue(visual_style.npc,
                   visual_style.npc_ink_strength_location,
                   &ink_strength, SHADER_UNIFORM_FLOAT);
    float hero = hero_emphasis ? 1.0f : 0.0f;
    SetShaderValue(visual_style.npc,
                   visual_style.npc_hero_emphasis_location,
                   &hero, SHADER_UNIFORM_FLOAT);
    SetShaderValue(visual_style.npc,
                   visual_style.npc_hero_head_position_location,
                   &hero_head_position, SHADER_UNIFORM_VEC3);
    if (visual_style.npc_skinned_ready) {
        SetShaderValueV(visual_style.npc_skinned,
                        visual_style.npc_skinned_palette_location,
                        palette, SHADER_UNIFORM_VEC4,
                        CC_NPC_ARCHETYPE_MATERIAL_COUNT);
        SetShaderValueV(visual_style.npc_skinned,
                        visual_style.npc_skinned_palette_ink_location,
                        material_ink, SHADER_UNIFORM_FLOAT,
                        CC_NPC_ARCHETYPE_MATERIAL_COUNT);
        SetShaderValue(visual_style.npc_skinned,
                       visual_style.npc_skinned_ink_strength_location,
                       &ink_strength, SHADER_UNIFORM_FLOAT);
        SetShaderValue(visual_style.npc_skinned,
                       visual_style.npc_skinned_hero_emphasis_location,
                       &hero, SHADER_UNIFORM_FLOAT);
        SetShaderValue(visual_style.npc_skinned,
                       visual_style.npc_skinned_hero_head_position_location,
                       &hero_head_position, SHADER_UNIFORM_VEC3);
    }
}

static void SetNpcPalette(const CcNpcAppearance *appearance,
                          float ink_strength, bool hero_emphasis,
                          Vector3 hero_head_position)
{
    if (appearance == NULL) return;
    const Color colors[CC_NPC_ARCHETYPE_MATERIAL_COUNT] = {
        appearance->skin,
        appearance->hair,
        appearance->underlayer,
        appearance->outer,
        appearance->trousers,
        appearance->leather,
        appearance->metal,
        appearance->accent,
        ShadeColor(appearance->hair, 0.62f),
    };
    SetIndexedPalette(colors, ink_strength, hero_emphasis,
                      hero_head_position);
}

typedef struct CreatureRenderPalette {
    Color skin;
    Color secondary;
    Color hide;
    Color cloth;
    Color leather;
    Color horn;
    Color metal;
    Color accent;
    Color eye;
} CreatureRenderPalette;

static CreatureRenderPalette CreaturePalette(CcCreatureVariant variant,
                                              Color primary)
{
    Color colors[CC_NPC_ARCHETYPE_MATERIAL_COUNT] = {
        (Color){104, 130, 70, 255}, (Color){48, 58, 42, 255},
        (Color){88, 66, 48, 255}, (Color){112, 71, 54, 255},
        (Color){73, 51, 42, 255}, (Color){194, 174, 126, 255},
        (Color){126, 132, 128, 255}, (Color){183, 128, 54, 255},
        (Color){236, 196, 74, 255},
    };
    switch (variant) {
        case CC_CREATURE_GOBLIN_RAIDER:
            colors[0] = (Color){115, 135, 70, 255};
            colors[3] = (Color){113, 52, 46, 255};
            colors[6] = (Color){145, 142, 129, 255};
            colors[7] = (Color){198, 75, 48, 255};
            break;
        case CC_CREATURE_GOBLIN_TRIBUTE_BEARER:
            colors[0] = (Color){117, 141, 76, 255};
            colors[3] = (Color){78, 83, 105, 255};
            colors[7] = (Color){220, 158, 54, 255};
            break;
        case CC_CREATURE_HORSE: {
            Color coat = primary.a > 0 ? primary :
                                         (Color){104, 78, 59, 255};
            colors[0] = coat;
            colors[1] = ShadeColor(coat, 0.54f);
            colors[2] = ShadeColor(coat, 0.82f);
            colors[3] = (Color){72, 48, 38, 255};
            colors[4] = (Color){50, 37, 32, 255};
            colors[7] = (Color){128, 88, 52, 255};
            break;
        }
        case CC_CREATURE_COW: {
            Color hide = primary.a > 0 ? primary :
                                         (Color){180, 166, 137, 255};
            colors[0] = (Color){190, 154, 127, 255};
            colors[1] = (Color){61, 49, 43, 255};
            colors[2] = hide;
            colors[3] = (Color){88, 70, 59, 255};
            colors[5] = (Color){205, 186, 143, 255};
            colors[7] = (Color){137, 85, 61, 255};
            break;
        }
        case CC_CREATURE_DRAGON:
            colors[0] = (Color){71, 100, 73, 255};
            colors[1] = (Color){30, 48, 42, 255};
            colors[2] = (Color){91, 117, 76, 255};
            colors[3] = (Color){66, 54, 48, 255};
            colors[5] = (Color){190, 154, 94, 255};
            colors[7] = (Color){157, 57, 43, 255};
            colors[8] = (Color){245, 190, 48, 255};
            break;
        case CC_CREATURE_GOBLIN_SCAVENGER:
        default:
            break;
    }
    return (CreatureRenderPalette){
        colors[0], colors[1], colors[2], colors[3], colors[4],
        colors[5], colors[6], colors[7], colors[8]
    };
}

static CcCreatureRigProfile CreatureRigProfileForVariant(
    CcCreatureVariant variant)
{
    switch (variant) {
        case CC_CREATURE_HORSE: return CC_CREATURE_RIG_HORSE;
        case CC_CREATURE_COW: return CC_CREATURE_RIG_COW;
        case CC_CREATURE_DRAGON: return CC_CREATURE_RIG_DRAGON;
        case CC_CREATURE_GOBLIN_SCAVENGER:
        case CC_CREATURE_GOBLIN_RAIDER:
        case CC_CREATURE_GOBLIN_TRIBUTE_BEARER:
        default:
            return CC_CREATURE_RIG_GOBLIN;
    }
}

static bool ResolveControlledCreatureGait(
    CreatureGaitSlot slot, CcCreatureRigProfile profile, float clock,
    float initial_phase, float forward_speed, bool moving,
    CcCreatureRigPose *pose)
{
    if (slot < 0 || slot >= CREATURE_GAIT_SLOT_COUNT || pose == NULL ||
        !isfinite(clock)) {
        return false;
    }
    CreatureGaitCache *cache = &creature_gaits[slot];
    float elapsed = cache->ready ? clock - cache->last_clock : 0.0f;
    bool reset = !cache->ready || cache->profile != profile ||
                 elapsed < 0.0f || elapsed > 0.35f;
    if (reset) {
        *cache = (CreatureGaitCache){0};
        if (!CcCreatureRigControllerInit(&cache->controller, profile,
                                         initial_phase, 1.0f)) {
            return false;
        }
        cache->profile = profile;
        cache->last_clock = clock;
        cache->ready = true;
        if (moving) {
            CcCreatureRigPose warm_pose;
            for (int32_t frame = 0; frame < 36; ++frame) {
                if (!CcCreatureRigControllerStep(
                        &cache->controller, forward_speed, 1.0f,
                        1.0f / 60.0f, &warm_pose)) {
                    *cache = (CreatureGaitCache){0};
                    return false;
                }
            }
        }
        elapsed = 0.0f;
    }
    cache->last_clock = clock;
    return CcCreatureRigControllerStep(
        &cache->controller, moving ? forward_speed : 0.0f,
        moving ? 1.0f : 0.0f, elapsed, pose);
}

static float CreatureRigPhase(CcCreaturePose pose)
{
    switch (pose) {
        case CC_CREATURE_POSE_CONTACT_A: return 0.00f;
        case CC_CREATURE_POSE_DOWN_A: return 0.125f;
        case CC_CREATURE_POSE_PASSING_A: return 0.250f;
        case CC_CREATURE_POSE_UP_A: return 0.375f;
        case CC_CREATURE_POSE_CONTACT_B: return 0.500f;
        case CC_CREATURE_POSE_DOWN_B: return 0.625f;
        case CC_CREATURE_POSE_PASSING_B: return 0.750f;
        case CC_CREATURE_POSE_UP_B: return 0.875f;
        case CC_CREATURE_POSE_STALK_A: return 0.18f;
        case CC_CREATURE_POSE_STALK_B: return 0.68f;
        case CC_CREATURE_POSE_THREAT: return 0.34f;
        case CC_CREATURE_POSE_IDLE:
        case CC_CREATURE_POSE_REST:
        case CC_CREATURE_POSE_COUNT:
        default:
            return 0.0f;
    }
}

static float CreatureRigMovement(CcCreaturePose pose)
{
    if (pose >= CC_CREATURE_POSE_CONTACT_A &&
        pose <= CC_CREATURE_POSE_UP_B) {
        return 1.0f;
    }
    if (pose == CC_CREATURE_POSE_STALK_A ||
        pose == CC_CREATURE_POSE_STALK_B) {
        return 0.68f;
    }
    return pose == CC_CREATURE_POSE_THREAT ? 0.28f : 0.0f;
}

static void DrawCreatureMuscleLimbs(const CcCreatureRigPose *rig,
                                    const CreatureRenderPalette *palette,
                                    float yaw, float scale)
{
    bool goblin = rig->profile == CC_CREATURE_RIG_GOBLIN;
    for (int32_t limb = 0; limb < rig->limb_count; ++limb) {
        const CcCreatureRigLimbPose *leg = &rig->limbs[limb];
        float base_radius = goblin ? 0.092f :
            rig->profile == CC_CREATURE_RIG_COW ? 0.115f :
            rig->profile == CC_CREATURE_RIG_DRAGON ? 0.142f :
            rig->profile >= CC_CREATURE_RIG_HEXAPOD ? 0.062f : 0.086f;
        base_radius *= scale;
        Color upper = goblin ? palette->cloth : palette->hide;
        Color lower = goblin ? palette->skin : palette->secondary;
        if (rig->profile == CC_CREATURE_RIG_DRAGON) {
            upper = palette->skin;
            lower = palette->accent;
        }
        for (int32_t segment = 0; segment < leg->segment_count; ++segment) {
            float taper = 1.0f - (float)segment * 0.18f;
            float radius = base_radius * taper *
                (1.0f + leg->segment_activation[segment] *
                 (0.42f - (float)segment * 0.06f));
            Vector3 start = FromLimbVector(leg->joints[segment]);
            Vector3 end = FromLimbVector(leg->joints[segment + 1]);
            Color color = segment == 0 ? upper : lower;
            DrawCylinderEx(start, end, radius, radius * 0.78f, 7, color);
            if (segment + 1 < leg->segment_count) {
                DrawCharacterSphere(end, radius * 0.94f,
                                    BlendColor(upper, lower, 0.46f));
            }
        }
        Vector3 foot = FromLimbVector(leg->joints[leg->segment_count]);
        if (goblin) {
            DrawOrientedBox(foot,
                            (Vector3){0.0f, 0.025f * scale, 0.06f * scale},
                            (Vector3){0.22f * scale, 0.10f * scale,
                                      0.30f * scale},
                            yaw, palette->leather);
        } else {
            Color hoof = rig->profile == CC_CREATURE_RIG_DRAGON ?
                palette->accent : palette->leather;
            DrawOrientedBox(foot,
                            (Vector3){0.0f, 0.018f * scale, 0.03f * scale},
                            (Vector3){0.18f * scale, 0.09f * scale,
                                      0.24f * scale},
                            yaw, hoof);
        }
    }
}

static void DrawGoblinRig(CcCreatureVariant variant,
                          const CcCreatureRigPose *rig,
                          const CreatureRenderPalette *palette,
                          float yaw, float scale)
{
    Vector3 pelvis = FromLimbVector(rig->body);
    float wave = sinf(rig->phase * 2.0f * PI);
    Vector3 chest = LocalPoint(pelvis, 0.0f, 0.36f * scale,
                               0.018f * wave * scale, yaw);
    Vector3 neck = LocalPoint(pelvis, 0.0f, 0.58f * scale, 0.0f, yaw);
    Vector3 head = LocalPoint(pelvis, 0.0f, 0.76f * scale, 0.025f, yaw);
    float muscle = 1.0f + rig->mean_activation * 0.48f;

    DrawOrientedBox(pelvis, (Vector3){0.0f, 0.10f * scale, 0.0f},
                    (Vector3){0.38f * scale, 0.22f * scale,
                              0.28f * scale},
                    yaw, palette->leather);
    DrawCylinderEx(pelvis, chest, 0.20f * scale * muscle,
                   0.25f * scale * muscle, 7, palette->cloth);
    DrawCylinderEx(chest, neck, 0.20f * scale, 0.10f * scale, 7,
                   palette->cloth);
    DrawCharacterEllipsoid(head,
                           (Vector3){0.25f * scale, 0.24f * scale,
                                     0.22f * scale},
                           palette->skin);
    for (int32_t side = -1; side <= 1; side += 2) {
        Vector3 ear_root = LocalPoint(
            head, (float)side * 0.18f * scale, 0.02f * scale,
            0.0f, yaw);
        Vector3 ear_tip = LocalPoint(
            head, (float)side * 0.39f * scale, 0.04f * scale,
            -0.01f * scale, yaw);
        DrawCylinderEx(ear_root, ear_tip, 0.075f * scale, 0.012f * scale,
                       5, palette->skin);
        Vector3 eye = LocalPoint(
            head, (float)side * 0.080f * scale, 0.035f * scale,
            0.205f * scale, yaw);
        DrawCharacterSphere(eye, 0.032f * scale, palette->eye);

        float arm_wave = wave * (float)-side;
        Vector3 shoulder = LocalPoint(
            chest, (float)side * 0.26f * scale, 0.02f * scale,
            0.0f, yaw);
        Vector3 elbow = LocalPoint(
            chest, (float)side * 0.32f * scale, -0.23f * scale,
            arm_wave * 0.11f * scale, yaw);
        Vector3 hand = LocalPoint(
            chest, (float)side * 0.31f * scale, -0.48f * scale,
            arm_wave * 0.18f * scale, yaw);
        DrawCylinderEx(shoulder, elbow, 0.082f * scale * muscle,
                       0.068f * scale, 7, palette->cloth);
        DrawCylinderEx(elbow, hand, 0.068f * scale,
                       0.055f * scale, 7, palette->skin);
        DrawCharacterSphere(hand, 0.078f * scale, palette->skin);
    }

    if (variant == CC_CREATURE_GOBLIN_RAIDER) {
        Vector3 helmet_tip = LocalPoint(head, 0.0f, 0.37f * scale,
                                        0.0f, yaw);
        DrawCylinderEx(head, helmet_tip, 0.23f * scale, 0.025f * scale,
                       6, palette->metal);
        DrawOrientedBox(chest, (Vector3){0.0f, 0.0f, 0.17f * scale},
                        (Vector3){0.38f * scale, 0.30f * scale,
                                  0.07f * scale},
                        yaw, palette->metal);
        Vector3 spear_hand = LocalPoint(chest, 0.32f * scale,
                                        -0.38f * scale, 0.10f * scale, yaw);
        Vector3 spear_tip = LocalPoint(chest, 0.32f * scale,
                                       0.78f * scale, 0.08f * scale, yaw);
        DrawCylinderEx(spear_hand, spear_tip, 0.025f * scale,
                       0.018f * scale, 6, palette->leather);
        Vector3 blade_tip = LocalPoint(chest, 0.32f * scale,
                                       1.02f * scale, 0.08f * scale, yaw);
        DrawCylinderEx(spear_tip, blade_tip, 0.075f * scale, 0.0f, 5,
                       palette->metal);
    } else if (variant == CC_CREATURE_GOBLIN_TRIBUTE_BEARER) {
        DrawOrientedBox(chest,
                        (Vector3){0.0f, -0.22f * scale, 0.30f * scale},
                        (Vector3){0.48f * scale, 0.38f * scale,
                                  0.38f * scale},
                        yaw, palette->accent);
        DrawOrientedBox(chest,
                        (Vector3){0.0f, -0.22f * scale, 0.505f * scale},
                        (Vector3){0.10f * scale, 0.40f * scale,
                                  0.025f * scale},
                        yaw, palette->horn);
    } else {
        DrawOrientedBox(chest,
                        (Vector3){0.0f, -0.04f * scale, -0.20f * scale},
                        (Vector3){0.40f * scale, 0.42f * scale,
                                  0.18f * scale},
                        yaw, palette->secondary);
    }
}

static void DrawHorseOrCowRig(CcCreatureVariant variant,
                              const CcCreatureRigPose *rig,
                              const CreatureRenderPalette *palette,
                              float yaw, float scale)
{
    Vector3 body = FromLimbVector(rig->body);
    bool horse = variant == CC_CREATURE_HORSE;
    float breathing = 1.0f + rig->mean_activation * 0.22f;
    DrawOrientedBox(body, (Vector3){0.0f, 0.02f * scale, 0.0f},
                    (Vector3){rig->body_width * breathing,
                              rig->body_depth * breathing,
                              rig->body_length},
                    yaw, palette->hide);
    Vector3 neck_base = LocalPoint(
        body, 0.0f, horse ? 0.16f * scale : 0.04f * scale,
        rig->body_length * 0.42f, yaw);
    Vector3 neck = LocalPoint(
        body, 0.0f, horse ? 0.58f * scale : -0.06f * scale,
        rig->body_length * 0.62f, yaw);
    Vector3 head = LocalPoint(
        body, 0.0f, horse ? 0.72f * scale : -0.10f * scale,
        rig->body_length * 0.82f, yaw);
    DrawCylinderEx(neck_base, neck,
                   (horse ? 0.19f : 0.25f) * scale,
                   (horse ? 0.15f : 0.21f) * scale,
                   7, palette->hide);
    DrawCharacterEllipsoid(
        head,
        horse ? (Vector3){0.20f * scale, 0.23f * scale, 0.34f * scale} :
                (Vector3){0.31f * scale, 0.25f * scale, 0.34f * scale},
        horse ? palette->skin : palette->hide);
    for (int32_t side = -1; side <= 1; side += 2) {
        Vector3 eye = LocalPoint(
            head, (float)side * (horse ? 0.14f : 0.20f) * scale,
            0.045f * scale, 0.24f * scale, yaw);
        DrawCharacterSphere(eye, 0.028f * scale, palette->eye);
        Vector3 ear_root = LocalPoint(
            head, (float)side * 0.11f * scale, 0.18f * scale,
            -0.02f * scale, yaw);
        Vector3 ear_tip = LocalPoint(
            head, (float)side * (horse ? 0.14f : 0.24f) * scale,
            (horse ? 0.38f : 0.29f) * scale, 0.0f, yaw);
        DrawCylinderEx(ear_root, ear_tip, 0.045f * scale,
                       0.012f * scale, 5, palette->secondary);
        if (!horse) {
            Vector3 horn_tip = LocalPoint(
                head, (float)side * 0.40f * scale, 0.30f * scale,
                0.02f * scale, yaw);
            DrawCylinderEx(ear_root, horn_tip, 0.055f * scale, 0.0f, 6,
                           palette->horn);
        }
    }
    Vector3 tail_root = LocalPoint(body, 0.0f, 0.22f * scale,
                                   -rig->body_length * 0.48f, yaw);
    Vector3 tail_tip = LocalPoint(body, 0.0f, -0.40f * scale,
                                  -rig->body_length * 0.70f, yaw);
    DrawCylinderEx(tail_root, tail_tip,
                   (horse ? 0.065f : 0.045f) * scale,
                   0.018f * scale, 6, palette->secondary);
    if (horse) {
        Vector3 mane_top = LocalPoint(body, 0.0f, 0.72f * scale,
                                      rig->body_length * 0.50f, yaw);
        DrawCylinderEx(neck_base, mane_top, 0.08f * scale, 0.04f * scale,
                       6, palette->secondary);
    } else {
        Vector3 udder = LocalPoint(body, 0.0f, -0.48f * scale,
                                   -0.10f * scale, yaw);
        DrawCharacterEllipsoid(udder,
                               (Vector3){0.20f * scale, 0.12f * scale,
                                         0.22f * scale},
                               palette->accent);
    }
}

static void DrawDoubleSidedTriangle(Vector3 a, Vector3 b, Vector3 c,
                                    Color color)
{
    DrawTriangle3D(a, b, c, color);
    DrawTriangle3D(c, b, a, color);
}

static void DrawDragonRig(const CcCreatureRigPose *rig,
                          const CreatureRenderPalette *palette,
                          float yaw, float scale)
{
    Vector3 body = FromLimbVector(rig->body);
    float tension = 1.0f + rig->mean_activation * 0.30f;
    DrawOrientedBox(body, (Vector3){0.0f, 0.02f * scale, 0.0f},
                    (Vector3){rig->body_width * tension,
                              rig->body_depth * tension,
                              rig->body_length},
                    yaw, palette->skin);
    Vector3 chest = LocalPoint(body, 0.0f, 0.16f * scale,
                               rig->body_length * 0.40f, yaw);
    Vector3 neck = LocalPoint(body, 0.0f, 0.34f * scale,
                              rig->body_length * 0.68f, yaw);
    Vector3 head = LocalPoint(body, 0.0f, 0.38f * scale,
                              rig->body_length * 0.98f, yaw);
    DrawCylinderEx(chest, neck, 0.30f * scale, 0.23f * scale, 7,
                   palette->hide);
    DrawCylinderEx(neck, head, 0.23f * scale, 0.18f * scale, 7,
                   palette->skin);
    DrawCharacterEllipsoid(head,
                           (Vector3){0.34f * scale, 0.25f * scale,
                                     0.44f * scale},
                           palette->skin);
    Vector3 jaw = LocalPoint(head, 0.0f, -0.17f * scale,
                             0.28f * scale, yaw);
    DrawOrientedBox(jaw, (Vector3){0.0f, 0.0f, 0.0f},
                    (Vector3){0.48f * scale, 0.13f * scale,
                              0.42f * scale},
                    yaw, palette->accent);
    for (int32_t side = -1; side <= 1; side += 2) {
        Vector3 eye = LocalPoint(head, (float)side * 0.22f * scale,
                                 0.06f * scale, 0.31f * scale, yaw);
        DrawCharacterSphere(eye, 0.042f * scale, palette->eye);
        Vector3 horn_root = LocalPoint(head, (float)side * 0.18f * scale,
                                       0.17f * scale, -0.08f * scale, yaw);
        Vector3 horn_tip = LocalPoint(head, (float)side * 0.30f * scale,
                                      0.50f * scale, -0.24f * scale, yaw);
        DrawCylinderEx(horn_root, horn_tip, 0.075f * scale, 0.0f, 6,
                       palette->horn);

        Vector3 wing_root = LocalPoint(body, (float)side * 0.34f * scale,
                                       0.35f * scale, 0.18f * scale, yaw);
        Vector3 wing_tip = LocalPoint(body, (float)side * 1.55f * scale,
                                      0.72f * scale, -0.18f * scale, yaw);
        Vector3 wing_back = LocalPoint(body, (float)side * 1.02f * scale,
                                       0.18f * scale, -0.98f * scale, yaw);
        DrawCylinderEx(wing_root, wing_tip, 0.075f * scale,
                       0.028f * scale, 6, palette->secondary);
        DrawDoubleSidedTriangle(wing_root, wing_tip, wing_back,
                                ShadeColor(palette->accent, 0.82f));
    }
    Vector3 tail_a = LocalPoint(body, 0.0f, 0.02f * scale,
                                -rig->body_length * 0.48f, yaw);
    Vector3 tail_b = LocalPoint(body, 0.10f * scale, -0.08f * scale,
                                -rig->body_length * 0.88f, yaw);
    Vector3 tail_c = LocalPoint(body, -0.12f * scale, -0.20f * scale,
                                -rig->body_length * 1.24f, yaw);
    DrawCylinderEx(tail_a, tail_b, 0.24f * scale, 0.15f * scale, 7,
                   palette->skin);
    DrawCylinderEx(tail_b, tail_c, 0.15f * scale, 0.025f * scale, 7,
                   palette->secondary);
    for (int32_t spine = 0; spine < 5; ++spine) {
        float amount = (float)spine / 4.0f;
        Vector3 root = LocalPoint(body, 0.0f,
                                  (0.48f - amount * 0.20f) * scale,
                                  (0.72f - amount * 1.30f) * scale, yaw);
        Vector3 tip = root;
        tip.y += (0.28f - amount * 0.08f) * scale;
        DrawCylinderEx(root, tip, 0.065f * scale, 0.0f, 5, palette->horn);
    }
}

static CcQuadrupedMorphology QuadrupedMorphologyForCreature(
    CcCreatureVariant variant)
{
    if (variant == CC_CREATURE_HORSE) return CC_QUADRUPED_HORSE;
    if (variant == CC_CREATURE_COW) return CC_QUADRUPED_COW;
    return CC_QUADRUPED_MORPHOLOGY_COUNT;
}

static bool PoseQuadrupedCreature(CreatureModelCache *creature,
                                  CcCreatureVariant variant, float phase,
                                  bool moving,
                                  const CcCreatureRigPose *controlled_pose)
{
    if (creature == NULL || !creature->ready) return false;
    CcQuadrupedMorphology morphology = QuadrupedMorphologyForCreature(variant);
    CcQuadrupedPose rest = {0};
    CcQuadrupedPose target = {0};
    CcQuadrupedPoseResolve(morphology, 0.0f, false, &rest);
    if (controlled_pose != NULL && controlled_pose->valid) {
        CcQuadrupedPoseResolveFromRig(morphology, controlled_pose, &target);
    } else {
        CcQuadrupedPoseResolve(morphology, phase, moving, &target);
    }
    if (!rest.valid || !target.valid) return false;

    for (int32_t bone = 0; bone < creature->model.skeleton.boneCount;
         ++bone) {
        int32_t quadruped_bone = creature->quadruped_bone[bone];
        if (quadruped_bone < 0 ||
            quadruped_bone >= CC_QUADRUPED_BONE_COUNT) return false;
        const CcQuadrupedBonePose *rest_bone = &rest.bones[quadruped_bone];
        const CcQuadrupedBonePose *target_bone =
            &target.bones[quadruped_bone];
        Quaternion delta = HeroRotationBetween(
            FromLimbVector(rest_bone->up),
            FromLimbVector(target_bone->up));
        creature->pose[bone].translation = FromLimbVector(target_bone->head);
        creature->pose[bone].rotation = QuaternionMultiply(
            delta, creature->model.skeleton.bindPose[bone].rotation);
        creature->pose[bone].scale =
            creature->model.skeleton.bindPose[bone].scale;
    }
    UpdateModelAnimation(creature->model, creature->animation, 0.0f);
    CcLocalRendererRecordSkinUpdate(creature->model.meshCount);
    return true;
}

static void DrawContactShadow(Vector3 center, float width, float depth,
                              float yaw, Color color)
{
    if (width <= 0.0f || depth <= 0.0f || color.a == 0) return;

    /* Contact shadows are paint on the ground, not thin solid objects. A flat
       twelve-sided oval keeps the low-resolution silhouette deliberate while
       avoiding the square top and dark vertical edge produced by DrawBox. */
    const int32_t segment_count = 12;
    const float half_width = width * 0.5f;
    const float half_depth = depth * 0.5f;
    const float yaw_cos = cosf(yaw);
    const float yaw_sin = sinf(yaw);
    rlBegin(RL_TRIANGLES);
    rlColor4ub(color.r, color.g, color.b, color.a);
    rlNormal3f(0.0f, 1.0f, 0.0f);
    for (int32_t segment = 0; segment < segment_count; ++segment) {
        float angle_a = 2.0f * PI * (float)segment /
                        (float)segment_count;
        float angle_b = 2.0f * PI * (float)(segment + 1) /
                        (float)segment_count;
        float local_ax = cosf(angle_a) * half_width;
        float local_az = sinf(angle_a) * half_depth;
        float local_bx = cosf(angle_b) * half_width;
        float local_bz = sinf(angle_b) * half_depth;
        Vector3 edge_a = {
            center.x + local_ax * yaw_cos + local_az * yaw_sin,
            center.y,
            center.z - local_ax * yaw_sin + local_az * yaw_cos,
        };
        Vector3 edge_b = {
            center.x + local_bx * yaw_cos + local_bz * yaw_sin,
            center.y,
            center.z - local_bx * yaw_sin + local_bz * yaw_cos,
        };
        rlVertex3f(center.x, center.y, center.z);
        rlVertex3f(edge_b.x, edge_b.y, edge_b.z);
        rlVertex3f(edge_a.x, edge_a.y, edge_a.z);
    }
    rlEnd();
}

static bool DrawCreatureGait3D(CcCreatureVariant variant,
                               CcCreaturePose pose, Vector3 position,
                               float yaw, float scale, Color primary,
                               float gait_phase, bool moving,
                               const CcCreatureRigPose *controlled_pose)
{
    if (variant < 0 || variant >= CC_CREATURE_VARIANT_COUNT || scale <= 0.0f) {
        return false;
    }
    if (pose < 0 || pose >= CC_CREATURE_POSE_COUNT) {
        pose = CC_CREATURE_POSE_IDLE;
    }
    const CcCreatureDefinition *definition = CcCreatureDefinitionAt(variant);
    bool skinned = definition != NULL && definition->skinned;
    float rig_phase = skinned ?
        gait_phase / (2.0f * PI) : CreatureRigPhase(pose);
    float rig_movement = skinned ? (moving ? 1.0f : 0.0f) :
                                   CreatureRigMovement(pose);
    CcCreatureRigPose generated_rig;
    const CcCreatureRigPose *rig = controlled_pose;
    if (rig == NULL || !rig->valid) {
        if (!CcCreatureRigPoseResolve(
                CreatureRigProfileForVariant(variant), rig_phase, rig_movement,
                ToLimbVector(position), yaw, scale, &generated_rig)) {
            return false;
        }
        rig = &generated_rig;
    }

    Vector2 shadow_size = {1.00f, 1.50f};
    if (variant <= CC_CREATURE_GOBLIN_TRIBUTE_BEARER) {
        shadow_size = (Vector2){0.58f, 0.46f};
    } else if (variant == CC_CREATURE_COW) {
        shadow_size = (Vector2){1.10f, 1.72f};
    } else if (variant == CC_CREATURE_DRAGON) {
        shadow_size = (Vector2){3.60f, 5.10f};
    }
    shadow_size.x *= scale;
    shadow_size.y *= scale;
    DrawContactShadow(
        (Vector3){position.x, position.y + 0.008f, position.z},
        shadow_size.x, shadow_size.y, yaw, (Color){2, 7, 10, 104});
    CreatureRenderPalette palette = CreaturePalette(variant, primary);
    if (skinned) {
        CreatureModelCache *creature =
            &creature_models[variant][CC_CREATURE_POSE_IDLE];
        if (creature->ready && creature->model.meshCount == 1 &&
            PoseQuadrupedCreature(creature, variant, gait_phase, moving, rig)) {
            Color colors[CC_NPC_ARCHETYPE_MATERIAL_COUNT] = {
                palette.skin, palette.secondary, palette.hide,
                palette.cloth, palette.leather, palette.horn,
                palette.metal, palette.accent, palette.eye,
            };
            SetIndexedPalette(colors, 0.54f, false, (Vector3){0});
            if (visual_style.npc_skinned_ready) {
                float body_skin_remap = 0.0f;
                SetShaderValue(
                    visual_style.npc_skinned,
                    visual_style.npc_skinned_body_skin_remap_location,
                    &body_skin_remap, SHADER_UNIFORM_FLOAT);
            }
            DrawModelEx(creature->model, position,
                        (Vector3){0.0f, 1.0f, 0.0f}, yaw * RAD2DEG,
                        (Vector3){scale, scale, scale}, WHITE);
            return true;
        }
    }
    if (controlled_pose != NULL && skinned) {
        if (!CcCreatureRigPoseResolve(
                CreatureRigProfileForVariant(variant), rig_phase, rig_movement,
                ToLimbVector(position), yaw, scale, &generated_rig)) {
            return false;
        }
        rig = &generated_rig;
    }
    DrawCreatureMuscleLimbs(rig, &palette, yaw, scale);
    if (variant <= CC_CREATURE_GOBLIN_TRIBUTE_BEARER) {
        DrawGoblinRig(variant, rig, &palette, yaw, scale);
    } else if (variant == CC_CREATURE_DRAGON) {
        DrawDragonRig(rig, &palette, yaw, scale);
    } else {
        DrawHorseOrCowRig(variant, rig, &palette, yaw, scale);
    }
    return true;
}

static bool DrawCreature3D(CcCreatureVariant variant, CcCreaturePose pose,
                           Vector3 position, float yaw, float scale,
                           Color primary)
{
    return DrawCreatureGait3D(variant, pose, position, yaw, scale, primary,
                              0.0f, false, NULL);
}

/* Procedural people use the same graphic lighting contract as the authored
   hero.  Their geometry is intentionally simple; a shared two-band treatment
   is what makes those forms read as one cast at the final art resolution. */
static void UseCharacterLighting(void)
{
    if (visual_style.hero_ready) BeginShaderMode(visual_style.hero);
}

static void RestoreWorldLighting(void)
{
    if (visual_style.hero_ready && visual_style.world_ready) {
        BeginShaderMode(visual_style.world);
    }
}

static void SetWorldForegroundReveal(float amount, float cut_height)
{
    if (!visual_style.world_ready) return;
    /* Uniform changes do not split raylib's active geometry batch. Flush at
       the reveal boundary so houses are submitted with the state that was
       active when they were authored, rather than the state of a later draw. */
    rlDrawRenderBatchActive();
    float active = fmaxf(0.0f, fminf(amount, 1.0f));
    SetShaderValue(visual_style.world,
                   visual_style.foreground_reveal_location,
                   &active, SHADER_UNIFORM_FLOAT);
    SetShaderValue(visual_style.world,
                   visual_style.reveal_cut_height_location,
                   &cut_height, SHADER_UNIFORM_FLOAT);
    if (visual_style.painted_environment.ready) {
        PaintedEnvironmentStyle *painted =
            &visual_style.painted_environment;
        SetShaderValue(painted->shader,
                       painted->foreground_reveal_location,
                       &active, SHADER_UNIFORM_FLOAT);
        SetShaderValue(painted->shader,
                       painted->reveal_cut_height_location,
                       &cut_height, SHADER_UNIFORM_FLOAT);
    }
}

static void SetWorldTerrainSurface(bool enabled)
{
    if (!visual_style.world_ready) return;
    /* Terrain and buildings share the world shader. Flush at the boundary so
       the broad land gets its hard-edged material layers while structures
       retain their own painted edges. */
    rlDrawRenderBatchActive();
    float active = enabled ? 1.0f : 0.0f;
    SetShaderValue(visual_style.world,
                   visual_style.terrain_surface_location,
                   &active, SHADER_UNIFORM_FLOAT);
}

typedef struct OklabColor {
    float lightness;
    float green_red;
    float blue_yellow;
} OklabColor;

static float SrgbChannelToLinear(unsigned char channel)
{
    float value = (float)channel / 255.0f;
    return value <= 0.04045f ? value / 12.92f :
        powf((value + 0.055f) / 1.055f, 2.4f);
}

static OklabColor ColorToOklab(Color color)
{
    float red = SrgbChannelToLinear(color.r);
    float green = SrgbChannelToLinear(color.g);
    float blue = SrgbChannelToLinear(color.b);
    float long_wave = 0.4122214708f * red + 0.5363325363f * green +
                      0.0514459929f * blue;
    float medium_wave = 0.2119034982f * red + 0.6806995451f * green +
                        0.1073969566f * blue;
    float short_wave = 0.0883024619f * red + 0.2817188376f * green +
                       0.6299787005f * blue;
    long_wave = cbrtf(long_wave);
    medium_wave = cbrtf(medium_wave);
    short_wave = cbrtf(short_wave);
    return (OklabColor){
        0.2104542553f * long_wave + 0.7936177850f * medium_wave -
            0.0040720468f * short_wave,
        1.9779984951f * long_wave - 2.4285922050f * medium_wave +
            0.4505937099f * short_wave,
        0.0259040371f * long_wave + 0.7827717662f * medium_wave -
            0.8086757660f * short_wave,
    };
}

static float OklabDistanceSquared(OklabColor source, OklabColor candidate)
{
    /* Small art pixels need especially clear value separation. OKLab keeps
       the comparison perceptual; the lightness weight slightly favors a
       stable grayscale read over a merely nearby hue. */
    float lightness = (source.lightness - candidate.lightness) * 1.24f;
    float green_red = source.green_red - candidate.green_red;
    float blue_yellow = source.blue_yellow - candidate.blue_yellow;
    return lightness * lightness + green_red * green_red +
           blue_yellow * blue_yellow;
}

static bool LoadSharedPaletteLookup(void)
{
    const int32_t size = CC_SHARED_PALETTE_LUT_SIZE;
    const int32_t tiles = CC_SHARED_PALETTE_LUT_TILES;
    const int32_t width = size * tiles;
    const int32_t height = size * (size / tiles);
    FinalPaletteEntry palette[CC_FINAL_PALETTE_MAX_COLORS] = {0};
    int32_t palette_count = BuildFinalPalette(
        palette, CC_FINAL_PALETTE_MAX_COLORS);
    if (palette_count <= 0) return false;
    OklabColor perceptual_palette[CC_FINAL_PALETTE_MAX_COLORS] = {0};
    int32_t protected_count = 0;
    for (int32_t index = 0; index < palette_count; ++index) {
        perceptual_palette[index] = ColorToOklab(palette[index].color);
        if (palette[index].ownership == FINAL_PALETTE_PROTECTED) {
            protected_count += 1;
        }
    }
    size_t pixel_count = (size_t)width * (size_t)height;
    Color *pixels = MemAlloc(
        (unsigned int)(pixel_count * sizeof(Color)));
    if (pixels == NULL) return false;

    for (int32_t blue = 0; blue < size; ++blue) {
        for (int32_t green = 0; green < size; ++green) {
            for (int32_t red = 0; red < size; ++red) {
                Color source = {
                    (unsigned char)((red * 255 + (size - 1) / 2) /
                                    (size - 1)),
                    (unsigned char)((green * 255 + (size - 1) / 2) /
                                    (size - 1)),
                    (unsigned char)((blue * 255 + (size - 1) / 2) /
                                    (size - 1)),
                    255,
                };
                OklabColor perceptual_source = ColorToOklab(source);
                int32_t environment_index = -1;
                int32_t protected_index = -1;
                float environment_distance = FLT_MAX;
                float protected_distance = FLT_MAX;
                for (int32_t index = 0; index < palette_count; ++index) {
                    float distance = OklabDistanceSquared(
                        perceptual_source, perceptual_palette[index]);
                    if (palette[index].ownership ==
                        FINAL_PALETTE_PROTECTED) {
                        if (distance >= protected_distance) continue;
                        protected_distance = distance;
                        protected_index = index;
                    } else {
                        if (distance >= environment_distance) continue;
                        environment_distance = distance;
                        environment_index = index;
                    }
                }
                /* Protected colors own only a tight perceptual neighborhood.
                   Exact character and signal pigments survive, while broad
                   terrain is pulled toward its material family instead. */
                const float protected_radius_squared = 0.003025f;
                bool choose_protected = protected_index >= 0 &&
                    protected_distance <= protected_radius_squared &&
                    protected_distance * 1.20f < environment_distance;
                int32_t best_index = choose_protected ? protected_index :
                                                       environment_index;
                if (best_index < 0) best_index = protected_index;
                int32_t pixel_x = red + (blue % tiles) * size;
                int32_t pixel_y = green + (blue / tiles) * size;
                size_t pixel = (size_t)pixel_y * (size_t)width +
                               (size_t)pixel_x;
                pixels[pixel] = palette[best_index].color;
            }
        }
    }

    Image image = {
        pixels, width, height, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
    };
    visual_style.palette_lut = LoadTextureFromImage(image);
    UnloadImage(image);
    if (!IsTextureValid(visual_style.palette_lut)) return false;
    SetTextureFilter(visual_style.palette_lut, TEXTURE_FILTER_POINT);
    visual_style.palette_lut_location = GetShaderLocation(
        visual_style.grade, "paletteLut");
    if (visual_style.palette_lut_location < 0) {
        UnloadTexture(visual_style.palette_lut);
        visual_style.palette_lut = (Texture2D){0};
        return false;
    }
    TraceLog(LOG_INFO,
             "STYLE: loaded %d-color perceptual lookup (%d protected)",
             palette_count, protected_count);
    return true;
}

static void LoadVisualStyle(void)
{
    char grade_path[1024];
    if (!ResolveAssetPath(CC_STYLE_GRADE_SHADER, grade_path,
                          sizeof(grade_path))) {
        TraceLog(LOG_WARNING, "STYLE: grade shader was not found");
    } else {
        visual_style.grade = LoadShader(NULL, grade_path);
    }
    if (IsShaderValid(visual_style.grade) && LoadSharedPaletteLookup()) {
        visual_style.grade_exposure_location = GetShaderLocation(
            visual_style.grade, "atmosphereExposure");
        visual_style.grade_shadow_tone_location = GetShaderLocation(
            visual_style.grade, "atmosphereShadowTone");
        visual_style.grade_highlight_tone_location = GetShaderLocation(
            visual_style.grade, "atmosphereHighlightTone");
        visual_style.grade_chroma_location = GetShaderLocation(
            visual_style.grade, "atmosphereChroma");
        visual_style.grade_ready = true;
    } else {
        if (IsShaderValid(visual_style.grade)) {
            UnloadShader(visual_style.grade);
        }
        visual_style.grade = (Shader){0};
        TraceLog(LOG_WARNING,
                 "STYLE: grade shader or palette lookup could not be loaded");
    }

    char vertex_path[1024];
    char fragment_path[1024];
    if (!ResolveAssetPath(CC_WORLD_LIGHT_VERTEX_SHADER, vertex_path,
                          sizeof(vertex_path)) ||
        !ResolveAssetPath(CC_WORLD_LIGHT_FRAGMENT_SHADER, fragment_path,
                          sizeof(fragment_path))) {
        TraceLog(LOG_WARNING, "STYLE: world light shaders were not found");
        return;
    }
    visual_style.world = LoadShader(vertex_path, fragment_path);
    if (!IsShaderValid(visual_style.world)) {
        visual_style.world = (Shader){0};
        TraceLog(LOG_WARNING, "STYLE: world light shader could not be loaded");
        return;
    }
    visual_style.light_direction_location = GetShaderLocation(
        visual_style.world, "lightDirection");
    visual_style.light_color_location = GetShaderLocation(
        visual_style.world, "lightColor");
    visual_style.ambient_color_location = GetShaderLocation(
        visual_style.world, "ambientColor");
    visual_style.camera_position_location = GetShaderLocation(
        visual_style.world, "cameraPosition");
    visual_style.camera_forward_location = GetShaderLocation(
        visual_style.world, "cameraForward");
    visual_style.shadow_color_location = GetShaderLocation(
        visual_style.world, "shadowColor");
    visual_style.fog_color_location = GetShaderLocation(
        visual_style.world, "fogColor");
    visual_style.fog_near_location = GetShaderLocation(
        visual_style.world, "fogNear");
    visual_style.fog_far_location = GetShaderLocation(
        visual_style.world, "fogFar");
    visual_style.focal_point_location = GetShaderLocation(
        visual_style.world, "focalPoint");
    visual_style.story_axis_location = GetShaderLocation(
        visual_style.world, "storyAxis");
    visual_style.foreground_anchor_location = GetShaderLocation(
        visual_style.world, "foregroundAnchor");
    visual_style.depth_splits_location = GetShaderLocation(
        visual_style.world, "depthSplits");
    visual_style.depth_strength_location = GetShaderLocation(
        visual_style.world, "depthStrength");
    visual_style.focal_contrast_location = GetShaderLocation(
        visual_style.world, "focalContrast");
    visual_style.reveal_cut_height_location = GetShaderLocation(
        visual_style.world, "revealCutHeight");
    visual_style.foreground_reveal_location = GetShaderLocation(
        visual_style.world, "foregroundReveal");
    visual_style.terrain_surface_location = GetShaderLocation(
        visual_style.world, "terrainSurface");
    visual_style.weather_wetness_location = GetShaderLocation(
        visual_style.world, "weatherWetness");
    visual_style.world_ready = true;

    char skinned_vertex_path[1024] = {0};
    bool skinned_vertex_ready = ResolveAssetPath(
        CC_WORLD_LIGHT_SKINNED_VERTEX_SHADER, skinned_vertex_path,
        sizeof(skinned_vertex_path));
    if (!skinned_vertex_ready) {
        TraceLog(LOG_WARNING, "STYLE: skinned vertex shader was not found");
    }

    char painted_fragment_path[1024];
    if (ResolveAssetPath(CC_PAINTED_ENVIRONMENT_FRAGMENT_SHADER,
                         painted_fragment_path,
                         sizeof(painted_fragment_path))) {
        PaintedEnvironmentStyle *painted =
            &visual_style.painted_environment;
        painted->shader = LoadShader(vertex_path, painted_fragment_path);
        if (IsShaderValid(painted->shader)) {
            painted->light_direction_location = GetShaderLocation(
                painted->shader, "lightDirection");
            painted->light_color_location = GetShaderLocation(
                painted->shader, "lightColor");
            painted->ambient_color_location = GetShaderLocation(
                painted->shader, "ambientColor");
            painted->camera_position_location = GetShaderLocation(
                painted->shader, "cameraPosition");
            painted->camera_forward_location = GetShaderLocation(
                painted->shader, "cameraForward");
            painted->shadow_color_location = GetShaderLocation(
                painted->shader, "shadowColor");
            painted->fog_color_location = GetShaderLocation(
                painted->shader, "fogColor");
            painted->fog_near_location = GetShaderLocation(
                painted->shader, "fogNear");
            painted->fog_far_location = GetShaderLocation(
                painted->shader, "fogFar");
            painted->focal_point_location = GetShaderLocation(
                painted->shader, "focalPoint");
            painted->story_axis_location = GetShaderLocation(
                painted->shader, "storyAxis");
            painted->foreground_anchor_location = GetShaderLocation(
                painted->shader, "foregroundAnchor");
            painted->depth_splits_location = GetShaderLocation(
                painted->shader, "depthSplits");
            painted->depth_strength_location = GetShaderLocation(
                painted->shader, "depthStrength");
            painted->focal_contrast_location = GetShaderLocation(
                painted->shader, "focalContrast");
            painted->reveal_cut_height_location = GetShaderLocation(
                painted->shader, "revealCutHeight");
            painted->foreground_reveal_location = GetShaderLocation(
                painted->shader, "foregroundReveal");
            painted->ready = true;
        } else {
            painted->shader = (Shader){0};
            TraceLog(LOG_WARNING,
                     "STYLE: painted environment shader could not be loaded");
        }
    } else {
        TraceLog(LOG_WARNING,
                 "STYLE: painted environment shader was not found");
    }

    char foliage_fragment_path[1024];
    if (ResolveAssetPath(CC_TREE_FOLIAGE_FRAGMENT_SHADER,
                         foliage_fragment_path,
                         sizeof(foliage_fragment_path))) {
        visual_style.foliage = LoadShader(vertex_path,
                                          foliage_fragment_path);
        if (IsShaderValid(visual_style.foliage)) {
            visual_style.foliage_light_direction_location =
                GetShaderLocation(visual_style.foliage, "lightDirection");
            visual_style.foliage_light_color_location =
                GetShaderLocation(visual_style.foliage, "lightColor");
            visual_style.foliage_ambient_color_location =
                GetShaderLocation(visual_style.foliage, "ambientColor");
            visual_style.foliage_camera_position_location =
                GetShaderLocation(visual_style.foliage, "cameraPosition");
            visual_style.foliage_camera_forward_location =
                GetShaderLocation(visual_style.foliage, "cameraForward");
            visual_style.foliage_shadow_color_location =
                GetShaderLocation(visual_style.foliage, "shadowColor");
            visual_style.foliage_fog_color_location =
                GetShaderLocation(visual_style.foliage, "fogColor");
            visual_style.foliage_fog_near_location =
                GetShaderLocation(visual_style.foliage, "fogNear");
            visual_style.foliage_fog_far_location =
                GetShaderLocation(visual_style.foliage, "fogFar");
            visual_style.foliage_depth_splits_location =
                GetShaderLocation(visual_style.foliage, "depthSplits");
            visual_style.foliage_depth_strength_location =
                GetShaderLocation(visual_style.foliage, "depthStrength");
            visual_style.foliage_ready = true;
        } else {
            visual_style.foliage = (Shader){0};
            TraceLog(LOG_WARNING,
                     "STYLE: tree foliage shader could not be loaded");
        }
    } else {
        TraceLog(LOG_WARNING, "STYLE: tree foliage shader was not found");
    }

    char hero_fragment_path[1024];
    if (skinned_vertex_ready &&
        ResolveAssetPath(CC_HERO_PIXEL_FRAGMENT_SHADER, hero_fragment_path,
                         sizeof(hero_fragment_path))) {
        visual_style.hero = LoadShader(skinned_vertex_path,
                                       hero_fragment_path);
        if (IsShaderValid(visual_style.hero)) {
            visual_style.hero_light_direction_location = GetShaderLocation(
                visual_style.hero, "lightDirection");
            visual_style.hero_camera_position_location = GetShaderLocation(
                visual_style.hero, "cameraPosition");
            visual_style.hero_shadow_color_location = GetShaderLocation(
                visual_style.hero, "shadowColor");
            visual_style.hero_fog_color_location = GetShaderLocation(
                visual_style.hero, "fogColor");
            visual_style.hero_fog_near_location = GetShaderLocation(
                visual_style.hero, "fogNear");
            visual_style.hero_fog_far_location = GetShaderLocation(
                visual_style.hero, "fogFar");
            visual_style.hero_ink_strength_location = GetShaderLocation(
                visual_style.hero, "inkStrength");
            float ink_strength = CC_HERO_INK_STRENGTH;
            SetShaderValue(visual_style.hero,
                           visual_style.hero_ink_strength_location,
                           &ink_strength, SHADER_UNIFORM_FLOAT);
            visual_style.hero_ready = true;
        } else {
            visual_style.hero = (Shader){0};
            TraceLog(LOG_WARNING,
                     "STYLE: retro hero shader could not be loaded");
        }
    } else {
        TraceLog(LOG_WARNING, "STYLE: retro hero shader was not found");
    }

    char npc_fragment_path[1024];
    if (ResolveAssetPath(CC_NPC_INDEXED_FRAGMENT_SHADER, npc_fragment_path,
                         sizeof(npc_fragment_path))) {
        visual_style.npc = LoadShader(vertex_path, npc_fragment_path);
        if (IsShaderValid(visual_style.npc)) {
            visual_style.npc_light_direction_location = GetShaderLocation(
                visual_style.npc, "lightDirection");
            visual_style.npc_camera_position_location = GetShaderLocation(
                visual_style.npc, "cameraPosition");
            visual_style.npc_shadow_color_location = GetShaderLocation(
                visual_style.npc, "shadowColor");
            visual_style.npc_fog_color_location = GetShaderLocation(
                visual_style.npc, "fogColor");
            visual_style.npc_fog_near_location = GetShaderLocation(
                visual_style.npc, "fogNear");
            visual_style.npc_fog_far_location = GetShaderLocation(
                visual_style.npc, "fogFar");
            visual_style.npc_ink_strength_location = GetShaderLocation(
                visual_style.npc, "inkStrength");
            visual_style.npc_palette_location = GetShaderLocation(
                visual_style.npc, "characterPalette[0]");
            visual_style.npc_palette_ink_location = GetShaderLocation(
                visual_style.npc, "paletteInk[0]");
            visual_style.npc_hero_emphasis_location = GetShaderLocation(
                visual_style.npc, "heroEmphasis");
            visual_style.npc_hero_head_position_location = GetShaderLocation(
                visual_style.npc, "heroHeadPosition");
            visual_style.npc_ready = true;
        } else {
            visual_style.npc = (Shader){0};
            TraceLog(LOG_WARNING,
                     "STYLE: indexed NPC shader could not be loaded");
        }
        if (skinned_vertex_ready) {
            visual_style.npc_skinned = LoadShader(
                skinned_vertex_path, npc_fragment_path);
            if (IsShaderValid(visual_style.npc_skinned)) {
                visual_style.npc_skinned_light_direction_location =
                    GetShaderLocation(visual_style.npc_skinned,
                                      "lightDirection");
                visual_style.npc_skinned_camera_position_location =
                    GetShaderLocation(visual_style.npc_skinned,
                                      "cameraPosition");
                visual_style.npc_skinned_shadow_color_location =
                    GetShaderLocation(visual_style.npc_skinned,
                                      "shadowColor");
                visual_style.npc_skinned_fog_color_location =
                    GetShaderLocation(visual_style.npc_skinned, "fogColor");
                visual_style.npc_skinned_fog_near_location =
                    GetShaderLocation(visual_style.npc_skinned, "fogNear");
                visual_style.npc_skinned_fog_far_location =
                    GetShaderLocation(visual_style.npc_skinned, "fogFar");
                visual_style.npc_skinned_ink_strength_location =
                    GetShaderLocation(visual_style.npc_skinned,
                                      "inkStrength");
                visual_style.npc_skinned_palette_location =
                    GetShaderLocation(visual_style.npc_skinned,
                                      "characterPalette[0]");
                visual_style.npc_skinned_palette_ink_location =
                    GetShaderLocation(visual_style.npc_skinned,
                                      "paletteInk[0]");
                visual_style.npc_skinned_hero_emphasis_location =
                    GetShaderLocation(visual_style.npc_skinned,
                                      "heroEmphasis");
                visual_style.npc_skinned_hero_head_position_location =
                    GetShaderLocation(visual_style.npc_skinned,
                                      "heroHeadPosition");
                visual_style.npc_skinned_body_skin_remap_location =
                    GetShaderLocation(visual_style.npc_skinned,
                                      "bodySkinRemap");
                visual_style.npc_skinned_ready = true;
            } else {
                visual_style.npc_skinned = (Shader){0};
                TraceLog(LOG_WARNING,
                         "STYLE: skinned NPC shader could not be loaded");
            }
        }
    } else {
        TraceLog(LOG_WARNING, "STYLE: indexed NPC shader was not found");
    }

    ApplyWorldShader(&sphere_models.small);
    ApplyWorldShader(&sphere_models.character);
    ApplyWorldShader(&sphere_models.scenery);
    if (tree_crown_models.ready) {
        ApplyTreeFoliageShader(&tree_crown_models.alder);
        ApplyTreeFoliageShader(&tree_crown_models.oak);
        ApplyTreeFoliageShader(&tree_crown_models.pollard);
    }
    if (hero_skin.ready) ApplyHeroStyle(&hero_skin.model);
    for (int32_t role = 0; role < CC_NPC_ROLE_COUNT; ++role) {
        for (int32_t pose = 0; pose < CC_NPC_ARCHETYPE_POSE_COUNT; ++pose) {
            if (npc_archetypes[role][pose].ready) {
                ApplyNpcStyle(&npc_archetypes[role][pose].model);
            }
        }
    }
    for (int32_t variant = 0; variant < CC_CREATURE_VARIANT_COUNT;
         ++variant) {
        for (int32_t pose = 0; pose < CC_CREATURE_POSE_COUNT; ++pose) {
            if (creature_models[variant][pose].ready) {
                const CcCreatureDefinition *definition =
                    CcCreatureDefinitionAt((CcCreatureVariant)variant);
                if (definition != NULL && definition->skinned) {
                    ApplyNpcBodyStyle(&creature_models[variant][pose].model);
                } else {
                    ApplyNpcStyle(&creature_models[variant][pose].model);
                }
            }
        }
    }
    for (int32_t id = 0; id < NPC_DYNAMIC_MODULE_COUNT; ++id) {
        if (npc_dynamic_modules[id].ready) {
            ApplyNeutralNpcStyle(&npc_dynamic_modules[id].model);
        }
    }
    for (int32_t frame = 0; frame < CC_NPC_BODY_FRAME_COUNT; ++frame) {
        for (int32_t muscle = 0; muscle < CC_NPC_BODY_MUSCLE_COUNT; ++muscle) {
            for (int32_t tissue = 0; tissue < CC_NPC_BODY_TISSUE_COUNT;
                 ++tissue) {
                NpcBodySkinCache *body =
                    &npc_body_skins[frame][muscle][tissue];
                if (body->ready) ApplyNpcBodyStyle(&body->model);
            }
        }
    }
    for (int32_t family = 0; family < CC_NPC_HEAD_FAMILY_COUNT; ++family) {
        if (npc_head_families[family].ready) {
            ApplyNeutralNpcStyle(&npc_head_families[family].model);
        }
    }
    for (int32_t family = 0; family < CC_NPC_HAIR_FAMILY_COUNT; ++family) {
        if (npc_hair_families[family].ready) {
            ApplyNeutralNpcStyle(&npc_hair_families[family].model);
        }
    }
    for (int32_t id = 0; id < RUNTIME_ASSET_COUNT; ++id) {
        if (!runtime_assets[id].ready) continue;
        if (id == RUNTIME_ASSET_MARKET) {
            ApplyPaintedEnvironmentShader(&runtime_assets[id].model);
        } else {
            ApplyWorldShader(&runtime_assets[id].model);
        }
    }
}

static void LoadHeroSkin(void)
{
    const char *asset_path = HeroSkinAssetPath();
    if (asset_path == NULL) {
        TraceLog(LOG_WARNING, "HERO: engine skin was not found");
        return;
    }
    hero_skin.model = LoadModel(asset_path);
    int32_t bone_count = hero_skin.model.skeleton.boneCount;
    if (hero_skin.model.meshCount <= 0 ||
        hero_skin.model.meshCount > CC_LOCAL_HERO_RUNTIME_MESH_BUDGET ||
        bone_count <= 0 ||
        bone_count > CC_HERO_SKIN_MAX_BONES) {
        TraceLog(LOG_WARNING, "HERO: invalid engine skin (%d meshes, %d bones)",
                 hero_skin.model.meshCount, bone_count);
        UnloadModel(hero_skin.model);
        hero_skin = (HeroSkinCache){0};
        return;
    }
    bool found[CC_HUMANOID_SKIN_BONE_COUNT] = {false};
    bool found_cape[CC_HERO_CAPE_BONE_COUNT] = {false};
    bool found_hair[CC_HERO_HAIR_BONE_COUNT] = {false};
    for (int32_t bone = 0; bone < bone_count; ++bone) {
        int32_t skin_bone = CcHumanoidSkinBoneFind(
            hero_skin.model.skeleton.bones[bone].name);
        int32_t cape_bone = HeroCapeBoneFind(
            hero_skin.model.skeleton.bones[bone].name);
        int32_t hair_bone = HeroHairBoneFind(
            hero_skin.model.skeleton.bones[bone].name);
        hero_skin.skin_bone[bone] = skin_bone;
        hero_skin.cape_bone[bone] = cape_bone;
        hero_skin.hair_bone[bone] = hair_bone;
        if (skin_bone >= 0) found[skin_bone] = true;
        if (cape_bone >= 0) found_cape[cape_bone] = true;
        if (hair_bone >= 0) found_hair[hair_bone] = true;
    }
    for (int32_t bone = 0; bone < CC_HUMANOID_SKIN_BONE_COUNT; ++bone) {
        if (found[bone]) continue;
        TraceLog(LOG_WARNING, "HERO: engine skin is missing bone %s",
                 CcHumanoidSkinBoneName((CcHumanoidSkinBone)bone));
        UnloadModel(hero_skin.model);
        hero_skin = (HeroSkinCache){0};
        return;
    }
    for (int32_t bone = 0; bone < CC_HERO_CAPE_BONE_COUNT; ++bone) {
        if (found_cape[bone]) continue;
        TraceLog(LOG_WARNING, "HERO: engine skin is missing cloth bone %s",
                 HERO_CAPE_BONE_NAMES[bone]);
        UnloadModel(hero_skin.model);
        hero_skin = (HeroSkinCache){0};
        return;
    }
    if (screen_first_hero_active) {
        for (int32_t bone = 0; bone < CC_HERO_HAIR_BONE_COUNT; ++bone) {
            if (found_hair[bone]) continue;
            TraceLog(LOG_WARNING, "HERO: screen-first skin is missing hair bone %s",
                     HERO_HAIR_BONE_NAMES[bone]);
            UnloadModel(hero_skin.model);
            hero_skin = (HeroSkinCache){0};
            return;
        }
    }
    hero_skin.frames[0] = hero_skin.pose;
    hero_skin.animation.boneCount = bone_count;
    hero_skin.animation.keyframeCount = 1;
    hero_skin.animation.keyframePoses = hero_skin.frames;
    (void)snprintf(hero_skin.animation.name, sizeof(hero_skin.animation.name),
                   "engine-physics");
    hero_skin.ready = true;
    TraceLog(LOG_INFO,
             "HERO: loaded %s skin with %d meshes on %d physics bones",
             screen_first_hero_active ? "screen-first experiment" :
                                        "production",
             hero_skin.model.meshCount, bone_count);
}

static bool DrawHeroSkin(const CcHumanoidSkinPose *skin,
                         const CcLocalCapeState *cape, Color tint,
                         bool visible, bool record_statistics)
{
    if (!hero_skin.ready || skin == NULL || !skin->valid || cape == NULL ||
        !cape->initialized) return false;
    for (int32_t bone = 0; bone < hero_skin.model.skeleton.boneCount; ++bone) {
        int32_t skin_bone = hero_skin.skin_bone[bone];
        int32_t cape_bone = hero_skin.cape_bone[bone];
        int32_t hair_bone = hero_skin.hair_bone[bone];
        Vector3 target_head = {0};
        Vector3 target_direction = {0};
        Vector3 rest_direction = {0};
        if (skin_bone >= 0 && skin_bone < CC_HUMANOID_SKIN_BONE_COUNT) {
            const CcHumanoidSkinBonePose *target = &skin->bones[skin_bone];
            target_head = FromLimbVector(target->head);
            target_direction = FromLimbVector(target->up);
            rest_direction = HERO_REST_DIRECTIONS[skin_bone];
        } else if (cape_bone >= 0 && cape_bone < CC_HERO_CAPE_BONE_COUNT) {
            target_head = cape->point[cape_bone];
            target_direction = PhysicsNormalizeOr(
                PhysicsSubtract(cape->point[cape_bone + 1],
                                cape->point[cape_bone]),
                (Vector3){0.0f, -1.0f, 0.0f});
            rest_direction = HERO_CAPE_REST_DIRECTIONS[cape_bone];
        } else if (hair_bone >= 0 &&
                   hair_bone < CC_HERO_HAIR_BONE_COUNT) {
            const CcHumanoidSkinBonePose *head =
                &skin->bones[CC_HUMANOID_SKIN_HEAD];
            Quaternion head_rotation = {
                head->world_rotation.x,
                head->world_rotation.y,
                head->world_rotation.z,
                head->world_rotation.w,
            };
            Vector3 head_position = FromLimbVector(head->head);
            Vector3 root_offset = Vector3Scale(
                HERO_HAIR_ROOT_OFFSETS[hair_bone], 1.32f);
            target_head = Vector3Add(
                head_position,
                Vector3RotateByQuaternion(root_offset, head_rotation));
            Vector3 head_direction = Vector3RotateByQuaternion(
                HERO_HAIR_REST_DIRECTIONS[hair_bone], head_rotation);
            Vector3 cape_direction = PhysicsNormalizeOr(
                PhysicsSubtract(cape->point[1], cape->point[0]),
                head_direction);
            target_direction = PhysicsNormalizeOr(
                Vector3Lerp(head_direction, cape_direction,
                            HERO_HAIR_TIP_DELAY[hair_bone]),
                head_direction);
            rest_direction = HERO_HAIR_REST_DIRECTIONS[hair_bone];
        } else {
            hero_skin.pose[bone] = hero_skin.model.skeleton.bindPose[bone];
            continue;
        }
        Quaternion delta = HeroRotationBetween(
            rest_direction, target_direction);
        if (skin_bone == CC_HUMANOID_SKIN_HEAD) {
            /* Aligning only the head bone's long axis loses yaw because that
               axis is almost vertical. Use its complete biomechanical frame
               so the authored face really turns front, three-quarter, and
               profile with the actor. */
            const CcHumanoidSkinBonePose *head =
                &skin->bones[CC_HUMANOID_SKIN_HEAD];
            delta = (Quaternion){head->world_rotation.x,
                                 head->world_rotation.y,
                                 head->world_rotation.z,
                                 head->world_rotation.w};
        }
        hero_skin.pose[bone].translation = target_head;
        hero_skin.pose[bone].rotation = QuaternionMultiply(
            delta, hero_skin.model.skeleton.bindPose[bone].rotation);
        hero_skin.pose[bone].scale =
            hero_skin.model.skeleton.bindPose[bone].scale;
        float gameplay_scale =
            (skin_bone == CC_HUMANOID_SKIN_HEAD || hair_bone >= 0) ? 1.32f :
            (skin_bone == CC_HUMANOID_SKIN_HAND_LEFT ||
             skin_bone == CC_HUMANOID_SKIN_HAND_RIGHT) ? 0.90f :
            (skin_bone == CC_HUMANOID_SKIN_FOOT_LEFT ||
             skin_bone == CC_HUMANOID_SKIN_FOOT_RIGHT) ? 1.00f : 1.0f;
        hero_skin.pose[bone].scale = Vector3Scale(
            hero_skin.pose[bone].scale, gameplay_scale);
    }
    if (record_statistics) {
        CcLocalRendererRecordSkinUpdate(hero_skin.model.meshCount);
    }
    UpdateModelAnimation(hero_skin.model, hero_skin.animation, 0.0f);
    if (visible) {
        const float horizontal_scale = 0.98f;
        const float vertical_scale = 1.07f;
        Vector3 anchor = FromLimbVector(
            skin->bones[CC_HUMANOID_SKIN_ROOT].head);
        Vector3 position = {anchor.x * (1.0f - horizontal_scale),
                            anchor.y * (1.0f - vertical_scale),
                            anchor.z * (1.0f - horizontal_scale)};
        DrawModelEx(hero_skin.model, position,
                    (Vector3){0.0f, 1.0f, 0.0f}, 0.0f,
                    (Vector3){horizontal_scale, vertical_scale,
                              horizontal_scale}, tint);
    }
    return true;
}

static int32_t NpcBodyFrameForAppearance(const CcNpcAppearance *appearance)
{
    if (appearance->body_mass < 0.99f &&
        appearance->shoulder_scale < 1.04f) return 0;
    if (appearance->body_mass > 1.075f ||
        appearance->shoulder_scale > 1.085f) return 2;
    return 1;
}

static int32_t NpcBodyMuscleForAppearance(const CcNpcAppearance *appearance)
{
    if (appearance->muscularity < 0.50f) return 0;
    if (appearance->muscularity < 0.74f) return 1;
    return 2;
}

static int32_t NpcBodyTissueForAppearance(const CcNpcAppearance *appearance)
{
    if (appearance->body_mass < 0.99f) return 0;
    if (appearance->body_mass < 1.055f) return 1;
    return ((appearance->seed >> 7U) & 1U) == 0U ? 2 : 3;
}

static NpcBodySkinCache *NpcBodyForAppearance(
    const CcNpcAppearance *appearance)
{
    if (appearance == NULL) return NULL;
    int32_t frame = NpcBodyFrameForAppearance(appearance);
    int32_t muscle = NpcBodyMuscleForAppearance(appearance);
    int32_t tissue = NpcBodyTissueForAppearance(appearance);
    return &npc_body_skins[frame][muscle][tissue];
}

static bool DrawNpcBodySkin(const CcHumanoidSkinPose *skin,
                            const CcNpcAppearance *appearance)
{
    NpcBodySkinCache *body = NpcBodyForAppearance(appearance);
    if (body == NULL || !body->ready || !visual_style.npc_skinned_ready ||
        skin == NULL || !skin->valid) {
        return false;
    }
    for (int32_t bone = 0; bone < body->model.skeleton.boneCount; ++bone) {
        int32_t skin_bone = body->skin_bone[bone];
        if (skin_bone < 0 || skin_bone >= CC_HUMANOID_SKIN_BONE_COUNT) {
            body->pose[bone] = body->model.skeleton.bindPose[bone];
            continue;
        }
        const CcHumanoidSkinBonePose *target = &skin->bones[skin_bone];
        Quaternion delta = HeroRotationBetween(
            HERO_REST_DIRECTIONS[skin_bone], FromLimbVector(target->up));
        if (skin_bone == CC_HUMANOID_SKIN_HEAD) {
            delta = (Quaternion){target->world_rotation.x,
                                 target->world_rotation.y,
                                 target->world_rotation.z,
                                 target->world_rotation.w};
        }
        body->pose[bone].translation = FromLimbVector(target->head);
        body->pose[bone].rotation = QuaternionMultiply(
            delta, body->model.skeleton.bindPose[bone].rotation);
        body->pose[bone].scale = body->model.skeleton.bindPose[bone].scale;
    }
    UpdateModelAnimation(body->model, body->animation, 0.0f);
    for (int32_t material = 0; material < body->model.materialCount;
         ++material) {
        body->model.materials[material].maps[MATERIAL_MAP_DIFFUSE].color =
            WHITE;
    }
    /* The continuous skin asset closes joints under every fitted module. Its
       source color is skin everywhere, so remap that hidden foundation to
       underclothes here; the separate head and hand modules restore exposed
       skin. Without this pass, flesh-colored torso and leg gaps dominate the
       tiny combat silhouette. */
    rlDrawRenderBatchActive();
    float body_skin_remap = 1.0f;
    SetShaderValue(visual_style.npc_skinned,
                   visual_style.npc_skinned_body_skin_remap_location,
                   &body_skin_remap, SHADER_UNIFORM_FLOAT);
    DrawModelEx(body->model, (Vector3){0.0f, 0.0f, 0.0f},
                (Vector3){0.0f, 1.0f, 0.0f}, 0.0f,
                (Vector3){1.0f, 1.0f, 1.0f}, WHITE);
    rlDrawRenderBatchActive();
    body_skin_remap = 0.0f;
    SetShaderValue(visual_style.npc_skinned,
                   visual_style.npc_skinned_body_skin_remap_location,
                   &body_skin_remap, SHADER_UNIFORM_FLOAT);
    return true;
}

#define CC_TREE_CROWN_RING_VERTICES 6
#define CC_TREE_CROWN_TRIANGLES (CC_TREE_CROWN_RING_VERTICES * 4)

typedef struct TreeCrownProfile {
    float top_y;
    float upper_y;
    float lower_y;
    float bottom_y;
    Vector2 top_offset;
    Vector2 upper_offset;
    Vector2 lower_offset;
    Vector2 bottom_offset;
    float upper_angle;
    float lower_angle;
    float upper_depth;
    float lower_depth;
    float upper_radius[CC_TREE_CROWN_RING_VERTICES];
    float lower_radius[CC_TREE_CROWN_RING_VERTICES];
} TreeCrownProfile;

static const TreeCrownProfile TREE_CROWN_PROFILES[] = {
    /* Alder: a tall shard with a broken, conifer-like outline. */
    {
        1.12f, 0.28f, -0.34f, -0.88f,
        {0.18f, -0.08f}, {-0.05f, 0.04f},
        {0.06f, -0.04f}, {-0.14f, 0.10f},
        0.08f, 0.42f, 0.76f, 0.82f,
        {0.82f, 0.98f, 0.76f, 0.92f, 0.72f, 0.88f},
        {0.92f, 0.74f, 0.96f, 0.78f, 0.88f, 0.70f}
    },
    /* Oak: shallow caps make overlapping masses read as broad leaf plates. */
    {
        0.58f, 0.22f, -0.30f, -0.54f,
        {-0.12f, 0.08f}, {0.02f, -0.03f},
        {-0.06f, 0.05f}, {0.16f, -0.10f},
        -0.05f, 0.31f, 0.92f, 0.96f,
        {0.94f, 0.78f, 1.00f, 0.84f, 0.96f, 0.74f},
        {0.82f, 1.00f, 0.80f, 0.94f, 0.72f, 0.90f}
    },
    /* Pollard: a short top and long lower taper suggest hanging regrowth. */
    {
        0.72f, 0.30f, -0.20f, -1.12f,
        {0.12f, 0.02f}, {-0.06f, -0.04f},
        {0.04f, 0.06f}, {-0.10f, -0.06f},
        0.14f, 0.47f, 0.72f, 0.76f,
        {0.72f, 0.90f, 0.68f, 0.84f, 0.74f, 0.88f},
        {0.86f, 0.66f, 0.90f, 0.72f, 0.82f, 0.68f}
    }
};

static Vector3 TreeCrownRingPoint(const TreeCrownProfile *profile,
                                  bool upper, int32_t index)
{
    float angle = (upper ? profile->upper_angle : profile->lower_angle) +
                  (float)index * PI * 2.0f /
                  (float)CC_TREE_CROWN_RING_VERTICES;
    float radius = upper ? profile->upper_radius[index] :
                           profile->lower_radius[index];
    float depth = upper ? profile->upper_depth : profile->lower_depth;
    Vector2 offset = upper ? profile->upper_offset : profile->lower_offset;
    return (Vector3){offset.x + cosf(angle) * radius,
                     upper ? profile->upper_y : profile->lower_y,
                     offset.y + sinf(angle) * radius * depth};
}

static void TreeCrownMeshWriteTriangle(Mesh *mesh, int32_t *cursor,
                                      Vector3 a, Vector3 b, Vector3 c)
{
    Vector3 normal = Vector3CrossProduct(Vector3Subtract(b, a),
                                         Vector3Subtract(c, a));
    Vector3 center = Vector3Scale(Vector3Add(Vector3Add(a, b), c), 1.0f / 3.0f);
    if (Vector3DotProduct(normal, center) < 0.0f) {
        Vector3 swap = b;
        b = c;
        c = swap;
        normal = Vector3Negate(normal);
    }
    normal = Vector3Normalize(normal);
    const Vector3 points[] = {a, b, c};
    for (int32_t point = 0; point < 3; ++point) {
        int32_t vertex = *cursor;
        mesh->vertices[vertex * 3 + 0] = points[point].x;
        mesh->vertices[vertex * 3 + 1] = points[point].y;
        mesh->vertices[vertex * 3 + 2] = points[point].z;
        mesh->normals[vertex * 3 + 0] = normal.x;
        mesh->normals[vertex * 3 + 1] = normal.y;
        mesh->normals[vertex * 3 + 2] = normal.z;
        mesh->colors[vertex * 4 + 0] = 255;
        mesh->colors[vertex * 4 + 1] = 255;
        mesh->colors[vertex * 4 + 2] = 255;
        mesh->colors[vertex * 4 + 3] = 255;
        *cursor += 1;
    }
}

static Model BuildTreeCrownModel(TreeCrownShape shape)
{
    const int32_t vertex_count = CC_TREE_CROWN_TRIANGLES * 3;
    Mesh mesh = {0};
    mesh.vertexCount = vertex_count;
    mesh.triangleCount = CC_TREE_CROWN_TRIANGLES;
    mesh.vertices = MemAlloc(
        (unsigned int)((size_t)vertex_count * 3U * sizeof(float)));
    mesh.normals = MemAlloc(
        (unsigned int)((size_t)vertex_count * 3U * sizeof(float)));
    mesh.colors = MemAlloc(
        (unsigned int)((size_t)vertex_count * 4U * sizeof(unsigned char)));
    if (mesh.vertices == NULL || mesh.normals == NULL || mesh.colors == NULL) {
        if (mesh.vertices != NULL) MemFree(mesh.vertices);
        if (mesh.normals != NULL) MemFree(mesh.normals);
        if (mesh.colors != NULL) MemFree(mesh.colors);
        TraceLog(LOG_WARNING, "TREE: could not allocate crystalline crown mesh");
        return (Model){0};
    }

    const TreeCrownProfile *profile = &TREE_CROWN_PROFILES[shape];
    Vector3 top = {profile->top_offset.x, profile->top_y,
                   profile->top_offset.y};
    Vector3 bottom = {profile->bottom_offset.x, profile->bottom_y,
                      profile->bottom_offset.y};
    int32_t cursor = 0;
    for (int32_t side = 0; side < CC_TREE_CROWN_RING_VERTICES; ++side) {
        int32_t next = (side + 1) % CC_TREE_CROWN_RING_VERTICES;
        Vector3 upper = TreeCrownRingPoint(profile, true, side);
        Vector3 upper_next = TreeCrownRingPoint(profile, true, next);
        Vector3 lower = TreeCrownRingPoint(profile, false, side);
        Vector3 lower_next = TreeCrownRingPoint(profile, false, next);
        TreeCrownMeshWriteTriangle(&mesh, &cursor, top, upper, upper_next);
        TreeCrownMeshWriteTriangle(&mesh, &cursor, upper, lower, upper_next);
        TreeCrownMeshWriteTriangle(&mesh, &cursor, upper_next, lower,
                                   lower_next);
        TreeCrownMeshWriteTriangle(&mesh, &cursor, bottom, lower_next, lower);
    }
    if (cursor != vertex_count) {
        TraceLog(LOG_WARNING, "TREE: crystalline crown mesh count mismatch");
        UnloadMesh(mesh);
        return (Model){0};
    }
    UploadMesh(&mesh, false);
    return LoadModelFromMesh(mesh);
}

static void LoadTreeCrownModels(void)
{
    tree_crown_models.alder = BuildTreeCrownModel(TREE_CROWN_ALDER);
    tree_crown_models.oak = BuildTreeCrownModel(TREE_CROWN_OAK);
    tree_crown_models.pollard = BuildTreeCrownModel(TREE_CROWN_POLLARD);
    tree_crown_models.ready = IsModelValid(tree_crown_models.alder) &&
                              IsModelValid(tree_crown_models.oak) &&
                              IsModelValid(tree_crown_models.pollard);
    if (tree_crown_models.ready) return;
    if (IsModelValid(tree_crown_models.alder)) {
        UnloadModel(tree_crown_models.alder);
    }
    if (IsModelValid(tree_crown_models.oak)) {
        UnloadModel(tree_crown_models.oak);
    }
    if (IsModelValid(tree_crown_models.pollard)) {
        UnloadModel(tree_crown_models.pollard);
    }
    tree_crown_models = (TreeCrownModelCache){0};
    TraceLog(LOG_WARNING, "TREE: falling back to rounded foliage masses");
}

void CcLocalRendererInit(void)
{
    if (sphere_models.ready) return;
    art_atmosphere = (ArtAtmosphereState){
        .from = ART_ATMOSPHERES[CC_LOCAL_ATMOSPHERE_CLEAR_DAY],
        .target = CC_LOCAL_ATMOSPHERE_CLEAR_DAY,
        .blend = 1.0f,
        .duration = 1.0f,
    };
    street_camera_rig = (FixedCameraRig){0};
    road_camera_rig = (FixedCameraRig){0};
    combat_camera_rig = (CombatCameraRig){0};
    face_render_context = (FaceRenderContext){0};
    (void)memset(creature_gaits, 0, sizeof(creature_gaits));
    sphere_models.small = LoadModelFromMesh(GenMeshSphere(1.0f, 6, 8));
    sphere_models.character = LoadModelFromMesh(GenMeshSphere(1.0f, 8, 8));
    sphere_models.scenery = LoadModelFromMesh(GenMeshSphere(1.0f, 10, 12));
    sphere_models.ready = true;
    LoadTreeCrownModels();
    LoadHeroSkin();
    LoadNpcArchetypes();
    LoadCreatureModels();
    LoadNpcDynamicModules();
    LoadNpcBodySkins();
    LoadNpcHeadFamilies();
    LoadNpcHairFamilies();
    LoadRuntimeAssets();
    LoadVisualStyle();
    npc_portrait_target = LoadRenderTexture(72, 88);
    if (IsRenderTextureValid(npc_portrait_target)) {
        SetTextureFilter(npc_portrait_target.texture, TEXTURE_FILTER_POINT);
    }
}

void CcLocalRendererSetScreenFirstHero(bool enabled)
{
    if (sphere_models.ready) return;
    screen_first_hero_requested = enabled;
}

void CcLocalRendererSetDiagnosticOverlay(bool enabled)
{
    draw_hero_rig_debug = enabled;
}

void CcLocalRendererShutdown(void)
{
    if (!sphere_models.ready) return;
    TerrainRenderCacheClear();
    UnloadModel(sphere_models.small);
    UnloadModel(sphere_models.character);
    UnloadModel(sphere_models.scenery);
    if (tree_crown_models.ready) {
        UnloadModel(tree_crown_models.alder);
        UnloadModel(tree_crown_models.oak);
        UnloadModel(tree_crown_models.pollard);
    }
    if (hero_skin.ready) UnloadModel(hero_skin.model);
    for (int32_t role = 0; role < CC_NPC_ROLE_COUNT; ++role) {
        for (int32_t pose = 0; pose < CC_NPC_ARCHETYPE_POSE_COUNT; ++pose) {
            if (npc_archetypes[role][pose].ready) {
                UnloadModel(npc_archetypes[role][pose].model);
            }
            npc_archetypes[role][pose] = (NpcArchetypeCache){0};
        }
    }
    for (int32_t variant = 0; variant < CC_CREATURE_VARIANT_COUNT;
         ++variant) {
        for (int32_t pose = 0; pose < CC_CREATURE_POSE_COUNT; ++pose) {
            CreatureModelCache *creature = &creature_models[variant][pose];
            if (creature->ready) UnloadModel(creature->model);
            *creature = (CreatureModelCache){0};
        }
    }
    (void)memset(creature_gaits, 0, sizeof(creature_gaits));
    for (int32_t id = 0; id < NPC_DYNAMIC_MODULE_COUNT; ++id) {
        if (npc_dynamic_modules[id].ready) {
            UnloadModel(npc_dynamic_modules[id].model);
        }
        npc_dynamic_modules[id].model = (Model){0};
        npc_dynamic_modules[id].ready = false;
    }
    for (int32_t frame = 0; frame < CC_NPC_BODY_FRAME_COUNT; ++frame) {
        for (int32_t muscle = 0; muscle < CC_NPC_BODY_MUSCLE_COUNT; ++muscle) {
            for (int32_t tissue = 0; tissue < CC_NPC_BODY_TISSUE_COUNT;
                 ++tissue) {
                NpcBodySkinCache *body =
                    &npc_body_skins[frame][muscle][tissue];
                if (body->ready) UnloadModel(body->model);
                *body = (NpcBodySkinCache){0};
            }
        }
    }
    for (int32_t family = 0; family < CC_NPC_HEAD_FAMILY_COUNT; ++family) {
        if (npc_head_families[family].ready) {
            UnloadModel(npc_head_families[family].model);
        }
        npc_head_families[family] = (NpcHeadFamilyCache){0};
    }
    for (int32_t family = 0; family < CC_NPC_HAIR_FAMILY_COUNT; ++family) {
        if (npc_hair_families[family].ready) {
            UnloadModel(npc_hair_families[family].model);
        }
        npc_hair_families[family] = (NpcHairFamilyCache){0};
    }
    for (int32_t id = 0; id < RUNTIME_ASSET_COUNT; ++id) {
        if (runtime_assets[id].ready) UnloadModel(runtime_assets[id].model);
        runtime_assets[id].model = (Model){0};
        runtime_assets[id].ready = false;
    }
    if (visual_style.npc_skinned_ready) {
        UnloadShader(visual_style.npc_skinned);
    }
    if (visual_style.npc_ready) UnloadShader(visual_style.npc);
    if (visual_style.hero_ready) UnloadShader(visual_style.hero);
    if (visual_style.foliage_ready) UnloadShader(visual_style.foliage);
    if (visual_style.painted_environment.ready) {
        UnloadShader(visual_style.painted_environment.shader);
    }
    if (visual_style.world_ready) UnloadShader(visual_style.world);
    if (IsTextureValid(visual_style.palette_lut)) {
        UnloadTexture(visual_style.palette_lut);
    }
    if (IsRenderTextureValid(npc_portrait_target)) {
        UnloadRenderTexture(npc_portrait_target);
    }
    if (visual_style.grade_ready) UnloadShader(visual_style.grade);
    sphere_models = (SphereModelCache){0};
    npc_portrait_target = (RenderTexture2D){0};
    tree_crown_models = (TreeCrownModelCache){0};
    hero_skin = (HeroSkinCache){0};
    visual_style = (VisualStyleCache){0};
    street_camera_rig = (FixedCameraRig){0};
    road_camera_rig = (FixedCameraRig){0};
    combat_camera_rig = (CombatCameraRig){0};
    face_render_context = (FaceRenderContext){0};
    bridge_checkpoint_status = BRIDGE_CHECKPOINT_UNKNOWN;
}

static void DrawSphereModel(Model model, Vector3 center, float radius, Color color)
{
    DrawModelEx(model, center, (Vector3){0.0f, 1.0f, 0.0f}, 0.0f,
                (Vector3){radius, radius, radius}, color);
}

/* Raylib's default 16x16 sphere is excessive for this fixed isometric camera. */
static void DrawSmallSphere(Vector3 center, float radius, Color color)
{
    DrawSphereModel(sphere_models.small, center, radius, color);
}

static void DrawCharacterSphere(Vector3 center, float radius, Color color)
{
    DrawSphereModel(sphere_models.character, center, radius, color);
}

static void DrawScenerySphere(Vector3 center, float radius, Color color)
{
    DrawSphereModel(sphere_models.scenery, center, radius, color);
}

static void DrawBox(Vector3 center, Vector3 size, Color color)
{
    DrawCubeV(center, size, color);
    DrawCubeWiresV(center, size, Fade(WORLD_INK, 0.18f));
}

static Color ShadeColor(Color color, float scale)
{
    return (Color){
        (unsigned char)fmaxf(0.0f, fminf(255.0f, color.r * scale)),
        (unsigned char)fmaxf(0.0f, fminf(255.0f, color.g * scale)),
        (unsigned char)fmaxf(0.0f, fminf(255.0f, color.b * scale)),
        color.a
    };
}

static Color BlendColor(Color from, Color to, float amount)
{
    amount = fmaxf(0.0f, fminf(1.0f, amount));
    return (Color){
        (unsigned char)lroundf(from.r + (to.r - from.r) * amount),
        (unsigned char)lroundf(from.g + (to.g - from.g) * amount),
        (unsigned char)lroundf(from.b + (to.b - from.b) * amount),
        (unsigned char)lroundf(from.a + (to.a - from.a) * amount)
    };
}

static void DrawTiltedBox(Vector3 center, Vector3 size, Vector3 axis,
                          float degrees, Color color)
{
    rlPushMatrix();
    rlTranslatef(center.x, center.y, center.z);
    rlRotatef(degrees, axis.x, axis.y, axis.z);
    DrawBox((Vector3){0.0f, 0.0f, 0.0f}, size, color);
    rlPopMatrix();
}

static float DrawPitchedRoof(float x, float z, float width, float depth,
                             float wall_height, Color wall, Color roof)
{
    const float overhang = 0.42f;
    const float rise = 0.96f + fminf(width, depth) * 0.075f;
    const int32_t courses = 5;
    bool ridge_along_x = width >= depth;
    Color shadow = ShadeColor(roof, 0.72f);

    /* A roof is a stack of heavy courses, not two thin planes. The stepped
       silhouette survives the low art resolution and makes every house read
       as a small construction assembled from physical blocks. */
    DrawBox((Vector3){x + width * 0.5f, wall_height - 0.06f,
                      z + depth * 0.5f},
            (Vector3){width + overhang * 2.18f, 0.24f,
                      depth + overhang * 2.18f},
            BlendColor(ShadeColor(roof, 0.66f), wall, 0.16f));
    for (int32_t course = 0; course < courses; ++course) {
        float amount = (float)course / (float)courses;
        float course_height = rise / (float)courses;
        float roof_span = ridge_along_x ? depth : width;
        float remaining = fmaxf(0.48f,
            roof_span + overhang * 2.0f -
            amount * (roof_span + overhang * 1.42f));
        Vector3 size = ridge_along_x ?
            (Vector3){width + overhang * 2.0f,
                      course_height + 0.035f, remaining} :
            (Vector3){remaining, course_height + 0.035f,
                      depth + overhang * 2.0f};
        Color course_color = course == 0 ? shadow :
            ShadeColor(roof, 0.78f + (float)course * 0.045f);
        DrawBox((Vector3){x + width * 0.5f,
                          wall_height + course_height * ((float)course + 0.5f),
                          z + depth * 0.5f},
                size, course_color);
    }
    DrawBox((Vector3){x + width * 0.5f, wall_height + rise + 0.06f,
                      z + depth * 0.5f},
            ridge_along_x ?
                (Vector3){width + overhang * 2.10f, 0.18f, 0.24f} :
                (Vector3){0.24f, 0.18f, depth + overhang * 2.10f},
            ShadeColor(roof, 0.62f));
    return rise;
}

static void DrawConstructedWallShell(float x, float z, float width,
                                     float depth, float height,
                                     Color wall, Color trim, int32_t style)
{
    const float inset = 0.18f;
    const float base_height = fminf(1.06f, height * 0.20f);
    float upper_height = height - base_height;
    float corner = fmaxf(0.40f, fminf(0.58f,
        fminf(width, depth) * 0.072f));
    Color lower = ShadeColor(wall, 0.78f);
    Color side = ShadeColor(wall, 0.90f);

    /* The shell is built in layers. A deep lower block carries a smaller
       wall core; projecting bays and corner piers then restore the complete
       collision-sized silhouette. This gives the camera real parallax at
       every edge instead of one flat facade with lines painted on it. */
    DrawBox((Vector3){x + width * 0.5f, base_height * 0.5f,
                      z + depth * 0.5f},
            (Vector3){width, base_height, depth}, lower);
    DrawBox((Vector3){x + width * 0.5f,
                      base_height + upper_height * 0.5f,
                      z + depth * 0.5f},
            (Vector3){width - inset * 2.0f, upper_height,
                      depth - inset * 2.0f}, wall);

    for (int32_t corner_index = 0; corner_index < 4; ++corner_index) {
        float corner_x = (corner_index & 1) == 0 ?
            x + corner * 0.5f : x + width - corner * 0.5f;
        float corner_z = (corner_index & 2) == 0 ?
            z + corner * 0.5f : z + depth - corner * 0.5f;
        DrawBox((Vector3){corner_x, height * 0.50f, corner_z},
                (Vector3){corner, height, corner},
                corner_index == 3 ? trim : ShadeColor(trim, 0.90f));
    }

    int32_t front_bays = width >= 10.0f ? 3 : 2;
    float front_gap = 0.16f;
    float front_available = width - corner * 2.0f;
    float front_bay_width =
        (front_available - front_gap * (float)(front_bays - 1)) /
        (float)front_bays;
    for (int32_t bay = 0; bay < front_bays; ++bay) {
        float bay_x = x + corner + front_bay_width * ((float)bay + 0.5f) +
            front_gap * (float)bay;
        float projection = ((bay + style) & 1) != 0 ? 0.30f : 0.22f;
        DrawBox((Vector3){bay_x,
                          base_height + upper_height * 0.5f,
                          z + depth - inset * 0.30f + projection * 0.18f},
                (Vector3){front_bay_width, upper_height - 0.14f,
                          inset + projection},
                ((bay + style) & 1) != 0 ? wall : ShadeColor(wall, 0.96f));
    }

    int32_t side_bays = depth >= 9.0f ? 3 : 2;
    float side_available = depth - corner * 2.0f;
    float side_bay_depth =
        (side_available - front_gap * (float)(side_bays - 1)) /
        (float)side_bays;
    for (int32_t bay = 0; bay < side_bays; ++bay) {
        float bay_z = z + corner + side_bay_depth * ((float)bay + 0.5f) +
            front_gap * (float)bay;
        float projection = ((bay + style) & 1) != 0 ? 0.24f : 0.16f;
        DrawBox((Vector3){x + width - inset * 0.30f + projection * 0.18f,
                          base_height + upper_height * 0.5f, bay_z},
                (Vector3){inset + projection, upper_height - 0.14f,
                          side_bay_depth},
                ((bay + style) & 1) != 0 ? side : ShadeColor(side, 0.96f));
    }

    /* Timber houses gain a visibly cantilevered upper storey. Stone and
       civic buildings keep their weight closer to the foundation. */
    if (style == 0 || style == 3) {
        float storey = fminf(2.18f, height * 0.43f);
        DrawBox((Vector3){x + width * 0.5f, storey,
                          z + depth + 0.18f},
                (Vector3){width + 0.24f, 0.22f, 0.42f}, trim);
        DrawBox((Vector3){x + width + 0.18f, storey,
                          z + depth * 0.5f},
                (Vector3){0.42f, 0.22f, depth + 0.24f},
                ShadeColor(trim, 0.88f));
    }
}

static void DrawRoofDormer(float x, float z, float width, float depth,
                           float wall_height, float roof_rise,
                           Color wall, Color roof, Color trim)
{
    bool ridge_along_x = width >= depth;
    Color dormer_wall = BlendColor(wall, (Color){128, 116, 96, 255}, 0.16f);
    Color dormer_roof = ShadeColor(roof, 0.88f);
    Color glass = BlendColor((Color){31, 53, 57, 255}, WORLD_TEAL, 0.26f);
    if (ridge_along_x) {
        float center_x = x + width * 0.67f;
        float center_z = z + depth * 0.78f;
        float roof_surface = wall_height + roof_rise * 0.43f;
        DrawBox((Vector3){center_x, roof_surface + 0.37f, center_z},
                (Vector3){1.34f, 0.82f, 0.58f}, dormer_wall);
        DrawBox((Vector3){center_x, roof_surface + 0.40f,
                          center_z + 0.31f},
                (Vector3){0.52f, 0.46f, 0.045f}, glass);
        DrawBox((Vector3){center_x, roof_surface + 0.14f,
                          center_z + 0.34f},
                (Vector3){0.74f, 0.09f, 0.10f}, trim);
        for (int32_t side = -1; side <= 1; side += 2) {
            DrawTiltedBox(
                (Vector3){center_x, roof_surface + 0.90f,
                          center_z + (float)side * 0.18f},
                (Vector3){1.62f, 0.10f, 0.50f},
                (Vector3){1.0f, 0.0f, 0.0f}, (float)side * 31.0f,
                side > 0 ? dormer_roof : ShadeColor(dormer_roof, 0.76f));
        }
    } else {
        float center_x = x + width * 0.78f;
        float center_z = z + depth * 0.67f;
        float roof_surface = wall_height + roof_rise * 0.43f;
        DrawBox((Vector3){center_x, roof_surface + 0.37f, center_z},
                (Vector3){0.58f, 0.82f, 1.34f}, dormer_wall);
        DrawBox((Vector3){center_x + 0.31f, roof_surface + 0.40f,
                          center_z},
                (Vector3){0.045f, 0.46f, 0.52f}, glass);
        DrawBox((Vector3){center_x + 0.34f, roof_surface + 0.14f,
                          center_z},
                (Vector3){0.10f, 0.09f, 0.74f}, trim);
        for (int32_t side = -1; side <= 1; side += 2) {
            DrawTiltedBox(
                (Vector3){center_x + (float)side * 0.18f,
                          roof_surface + 0.90f, center_z},
                (Vector3){0.50f, 0.10f, 1.62f},
                (Vector3){0.0f, 0.0f, 1.0f}, (float)-side * 31.0f,
                side > 0 ? dormer_roof : ShadeColor(dormer_roof, 0.76f));
        }
    }
}

static void DrawFacadeBrace(Vector3 center, bool side_facing,
                            float length, float degrees, Color color)
{
    Vector3 size = side_facing ? (Vector3){0.11f, length, 0.16f} :
                                 (Vector3){0.16f, length, 0.11f};
    DrawTiltedBox(center, size,
                  side_facing ? (Vector3){1.0f, 0.0f, 0.0f} :
                                (Vector3){0.0f, 0.0f, 1.0f},
                  degrees, color);
}

static void DrawBuildingArchetypeDetails(float x, float z, float width,
                                         float depth, float height,
                                         Color wall, Color trim,
                                         int32_t style)
{
    float front_z = z + depth + 0.105f;
    float side_x = x + width + 0.105f;
    if (style == 1) {
        /* Workshops and storehouses use broad stone corner blocks. Their
           alternating rhythm reads as masonry at the final art-pixel size. */
        Color stone = ShadeColor(wall, 1.16f);
        for (int32_t course = 0; course < 5; ++course) {
            float y = 0.47f + (float)course * (height - 0.56f) / 4.0f;
            float inset = (course & 1) != 0 ? 0.08f : 0.0f;
            DrawBox((Vector3){x + width - 0.16f - inset, y, front_z},
                    (Vector3){0.48f, 0.52f, 0.15f}, stone);
            DrawBox((Vector3){side_x, y, z + depth - 0.16f - inset},
                    (Vector3){0.15f, 0.52f, 0.48f},
                    ShadeColor(stone, 0.90f));
        }
        DrawBox((Vector3){x + width * 0.50f, height * 0.70f, front_z},
                (Vector3){width * 0.50f, 0.16f, 0.15f}, trim);
        return;
    }

    if (style == 2) {
        /* Civic halls carry a stronger two-storey order than the cottages:
           tall pilasters, a balcony course, and a bright authority line. */
        Color pilaster = BlendColor(ShadeColor(wall, 0.62f), trim, 0.18f);
        for (int32_t bay = 0; bay < 3; ++bay) {
            float post_x = x + width * (0.18f + (float)bay * 0.32f);
            DrawBox((Vector3){post_x, height * 0.53f, front_z},
                    (Vector3){0.22f, height * 0.76f, 0.15f}, pilaster);
        }
        DrawBox((Vector3){x + width * 0.50f, height * 0.69f, front_z + 0.04f},
                (Vector3){width * 0.76f, 0.22f, 0.24f},
                ShadeColor(pilaster, 1.10f));
        DrawBox((Vector3){side_x, height * 0.69f, z + depth * 0.50f},
                (Vector3){0.24f, 0.22f, depth * 0.76f}, pilaster);
        DrawBox((Vector3){x + width * 0.50f, height * 0.69f + 0.13f,
                          front_z + 0.17f},
                (Vector3){width * 0.46f, 0.055f, 0.055f}, trim);
        return;
    }

    /* Cottages and mine-row houses expose their timber skeleton. The mine
       family is darker and denser; the domestic family keeps wider bays. */
    Color timber = style == 3 ? ShadeColor(trim, 0.76f) : trim;
    int32_t bay_count = style == 3 ? 3 : 2;
    for (int32_t bay = 1; bay <= bay_count; ++bay) {
        float amount = (float)bay / (float)(bay_count + 1);
        DrawBox((Vector3){x + width * amount, height * 0.51f, front_z},
                (Vector3){0.16f, height * 0.82f, 0.14f}, timber);
    }
    DrawBox((Vector3){side_x, height * 0.51f, z + depth * 0.44f},
            (Vector3){0.14f, height * 0.82f, 0.16f},
            ShadeColor(timber, 0.91f));
    float brace_y = fminf(height * 0.68f, 3.65f);
    float front_brace = fminf(width * 0.22f, 2.30f);
    DrawFacadeBrace((Vector3){x + width * 0.18f, brace_y, front_z + 0.01f},
                    false, front_brace, -42.0f, timber);
    DrawFacadeBrace((Vector3){x + width * 0.82f, brace_y, front_z + 0.01f},
                    false, front_brace, 42.0f, timber);
    if (style == 3) {
        float side_brace = fminf(depth * 0.24f, 2.15f);
        DrawFacadeBrace((Vector3){side_x + 0.01f, brace_y,
                                  z + depth * 0.76f},
                        true, side_brace, -40.0f,
                        ShadeColor(timber, 0.88f));
    }
}

static void DrawFacadeWindow(Vector3 center, bool side_facing, Color trim,
                             Color glass)
{
    Vector3 recess = side_facing ? (Vector3){0.075f, 1.34f, 1.02f} :
                                   (Vector3){1.02f, 1.34f, 0.075f};
    Vector3 pane = side_facing ? (Vector3){0.035f, 1.02f, 0.72f} :
                                 (Vector3){0.72f, 1.02f, 0.035f};
    DrawBox(center, recess, ShadeColor(trim, 0.48f));
    DrawBox(center, pane, glass);
    Vector3 vertical = side_facing ? (Vector3){0.022f, 1.06f, 0.055f} :
                                     (Vector3){0.055f, 1.06f, 0.022f};
    Vector3 horizontal = side_facing ? (Vector3){0.022f, 0.055f, 0.76f} :
                                       (Vector3){0.76f, 0.055f, 0.022f};
    DrawBox(center, vertical, trim);
    DrawBox(center, horizontal, trim);
    Vector3 sill_center = {center.x, center.y - 0.70f, center.z};
    Vector3 lintel_center = {center.x, center.y + 0.70f, center.z};
    if (side_facing) {
        sill_center.x += 0.025f;
        lintel_center.x += 0.025f;
    } else {
        sill_center.z += 0.025f;
        lintel_center.z += 0.025f;
    }
    Vector3 sill = side_facing ? (Vector3){0.10f, 0.11f, 1.12f} :
                                 (Vector3){1.12f, 0.11f, 0.10f};
    Vector3 lintel = side_facing ? (Vector3){0.08f, 0.15f, 1.04f} :
                                   (Vector3){1.04f, 0.15f, 0.08f};
    DrawBox(sill_center, sill, ShadeColor(trim, 1.08f));
    DrawBox(lintel_center, lintel, ShadeColor(trim, 0.82f));

    /* Deep jambs make the window a cavity in the wall. At gameplay scale the
       visible side faces matter more than another line on the glass. */
    float outward = 0.10f;
    Vector3 jamb_a = center;
    Vector3 jamb_b = center;
    Vector3 jamb_size;
    if (side_facing) {
        jamb_a.x += outward;
        jamb_b.x += outward;
        jamb_a.z -= 0.49f;
        jamb_b.z += 0.49f;
        jamb_size = (Vector3){0.24f, 1.36f, 0.14f};
    } else {
        jamb_a.z += outward;
        jamb_b.z += outward;
        jamb_a.x -= 0.49f;
        jamb_b.x += 0.49f;
        jamb_size = (Vector3){0.14f, 1.36f, 0.24f};
    }
    DrawBox(jamb_a, jamb_size, ShadeColor(trim, 0.86f));
    DrawBox(jamb_b, jamb_size, ShadeColor(trim, 0.92f));
}

static void DrawBuildingContactShadow(float x, float z, float width,
                                      float depth, float height)
{
    float reach = 0.18f + fminf(height, 10.0f) * 0.045f;
    DrawCubeV((Vector3){x + width * 0.5f + reach * 0.42f, -0.012f,
                        z + depth * 0.5f + reach * 0.32f},
              (Vector3){width + reach, 0.025f, depth + reach * 0.82f},
              (Color){8, 13, 14, 78});
}

static void DrawBuildingFoundation(float x, float z, float width,
                                   float depth, float height, Color wall)
{
    DrawBuildingContactShadow(x, z, width, depth, height);
    Color footing = ShadeColor(wall, 0.48f);
    Color cap = ShadeColor(wall, 0.63f);
    /* Sink the footing slightly into the terrain, then carry a wider cap
       around the wall. This layer is drawn without foreground reveal, so a
       cutaway can never turn a solid house into a patch of bare ground. */
    DrawBox((Vector3){x + width * 0.5f, 0.18f,
                      z + depth * 0.5f},
            (Vector3){width + 0.20f, 0.52f, depth + 0.20f}, footing);
    DrawBox((Vector3){x + width * 0.5f, 0.47f,
                      z + depth * 0.5f},
            (Vector3){width + 0.30f, 0.12f, depth + 0.30f}, cap);

    /* Two visible block courses turn the footing into laid masonry. The
       alternating joints continue around the corner so it reads as one
       weight-bearing construct from every street shot. */
    for (int32_t course = 0; course < 2; ++course) {
        float y = 0.13f + (float)course * 0.25f;
        float block_width = 0.92f;
        int32_t front_blocks = (int32_t)ceilf((width + 0.20f) / block_width);
        for (int32_t block = 0; block < front_blocks; ++block) {
            float start = x - 0.10f - (course != 0 ? block_width * 0.5f : 0.0f);
            float block_x = start + ((float)block + 0.5f) * block_width;
            if (block_x < x - 0.12f || block_x > x + width + 0.12f) continue;
            DrawBox((Vector3){block_x, y, z + depth + 0.065f},
                    (Vector3){block_width - 0.045f, 0.22f, 0.17f},
                    course == 0 ? footing : cap);
        }
        int32_t side_blocks = (int32_t)ceilf((depth + 0.20f) / block_width);
        for (int32_t block = 0; block < side_blocks; ++block) {
            float start = z - 0.10f - (course == 0 ? block_width * 0.5f : 0.0f);
            float block_z = start + ((float)block + 0.5f) * block_width;
            if (block_z < z - 0.12f || block_z > z + depth + 0.12f) continue;
            DrawBox((Vector3){x + width + 0.065f, y, block_z},
                    (Vector3){0.17f, 0.22f, block_width - 0.045f},
                    course == 0 ? ShadeColor(footing, 0.92f) :
                                  ShadeColor(cap, 0.92f));
        }
    }
}

static void DrawBuilding(float x, float z, float width, float depth,
                         float height, Color wall, Color roof, bool door,
                         int32_t style)
{
    Vector3 center = {x + width * 0.5f, height * 0.5f, z + depth * 0.5f};
    Color trim = style == 2 ? WORLD_GOLD : ShadeColor(wall, 0.60f);
    Color glass = style == 1 ? (Color){148, 103, 55, 255} :
                  style == 3 ? Fade(WORLD_VIOLET, 0.84f) :
                               Fade(WORLD_TEAL, 0.78f);
    DrawConstructedWallShell(x, z, width, depth, height, wall, trim, style);

    /* The positive Z and X facades face the runtime camera. Depth here is not
       decoration pasted onto a diorama: it follows the same authoritative
       footprint used by collision. */
    for (int32_t corner = 0; corner < 2; ++corner) {
        float post_x = corner == 0 ? x + 0.18f : x + width - 0.18f;
        DrawBox((Vector3){post_x, height * 0.50f, z + depth + 0.045f},
                (Vector3){0.18f, height - 0.18f, 0.09f}, trim);
    }
    DrawBox((Vector3){center.x, height - 0.24f, z + depth + 0.045f},
            (Vector3){width - 0.18f, 0.18f, 0.09f}, trim);
    DrawBox((Vector3){x + width + 0.045f, height - 0.24f, center.z},
            (Vector3){0.09f, 0.18f, depth - 0.18f}, trim);

    /* Strong horizontal courses keep the low camera from reading each house
       as one unscaled slab. They also carry the trim language around the
       visible corner, so the facade remains coherent from adjacent rooms. */
    Color plinth = ShadeColor(wall, 0.50f);
    float story_course = fminf(2.16f, height * 0.42f);
    DrawBox((Vector3){center.x, 0.46f, z + depth + 0.052f},
            (Vector3){width - 0.14f, 0.42f, 0.105f}, plinth);
    DrawBox((Vector3){x + width + 0.052f, 0.46f, center.z},
            (Vector3){0.105f, 0.42f, depth - 0.14f}, plinth);
    DrawBox((Vector3){center.x, story_course, z + depth + 0.057f},
            (Vector3){width - 0.16f, 0.13f, 0.115f}, trim);
    DrawBox((Vector3){x + width + 0.057f, story_course, center.z},
            (Vector3){0.115f, 0.13f, depth - 0.16f}, trim);

    int32_t bays = width >= 10.0f ? 3 : 2;
    float window_y = fminf(height * 0.56f, 2.90f);
    for (int32_t bay = 0; bay < bays; ++bay) {
        float window_x = x + width * (float)(bay + 1) / (float)(bays + 1);
        if (door && fabsf(window_x - center.x) < 0.82f) continue;
        DrawFacadeWindow((Vector3){window_x, window_y,
                                   z + depth + 0.075f}, false, trim, glass);
    }
    DrawFacadeWindow((Vector3){x + width + 0.075f, window_y,
                               center.z + depth * 0.12f}, true, trim, glass);

    if (door) {
        DrawBox((Vector3){center.x, 0.10f, z + depth + 0.42f},
                (Vector3){1.58f, 0.20f, 0.76f},
                ShadeColor(wall, 0.54f));
        DrawBox((Vector3){center.x, 0.045f, z + depth + 0.84f},
                (Vector3){1.34f, 0.09f, 0.38f},
                ShadeColor(wall, 0.66f));
        DrawBox((Vector3){center.x, 1.10f, z + depth + 0.055f},
                (Vector3){1.02f, 2.10f, 0.055f},
                (Color){43, 34, 37, 255});
        DrawBox((Vector3){center.x - 0.67f, 1.18f,
                          z + depth + 0.24f},
                (Vector3){0.24f, 2.36f, 0.52f}, trim);
        DrawBox((Vector3){center.x + 0.67f, 1.18f,
                          z + depth + 0.24f},
                (Vector3){0.24f, 2.36f, 0.52f},
                ShadeColor(trim, 0.92f));
        DrawBox((Vector3){center.x, 2.30f, z + depth + 0.24f},
                (Vector3){1.58f, 0.24f, 0.52f}, trim);
        DrawSmallSphere((Vector3){center.x + 0.39f, 1.075f,
                                  z + depth + 0.11f},
                        0.035f, WORLD_GOLD);
        DrawBox((Vector3){center.x, 2.52f, z + depth + 0.48f},
                (Vector3){1.82f, 0.18f, 0.92f}, roof);
        float lantern_x = center.x + 0.94f;
        DrawBox((Vector3){lantern_x, 1.92f, z + depth + 0.12f},
                (Vector3){0.08f, 0.48f, 0.08f}, trim);
        DrawBox((Vector3){lantern_x, 2.15f, z + depth + 0.18f},
                (Vector3){0.30f, 0.08f, 0.20f}, trim);
        DrawSmallSphere((Vector3){lantern_x, 1.88f, z + depth + 0.22f},
                        0.11f, WORLD_GOLD);
    }

    DrawBuildingArchetypeDetails(x, z, width, depth, height, wall, trim,
                                 style);

    float roof_rise = DrawPitchedRoof(x, z, width, depth, height, wall, roof);
    if (style != 1 && width >= 7.5f && depth >= 6.5f) {
        DrawRoofDormer(x, z, width, depth, height, roof_rise,
                       wall, roof, trim);
    }
    if (style == 0 || style == 2) {
        DrawBox((Vector3){x + width * 0.24f,
                          height + roof_rise * 0.74f,
                          z + depth * 0.36f},
                (Vector3){0.48f, 1.28f, 0.48f},
                ShadeColor(wall, 0.52f));
        DrawBox((Vector3){x + width * 0.24f,
                          height + roof_rise * 0.74f + 0.67f,
                          z + depth * 0.36f},
                (Vector3){0.62f, 0.14f, 0.62f}, trim);
    }
    if (style == 2) {
        for (int32_t side = -1; side <= 1; side += 2) {
            float side_sign = (float)side;
            DrawBox((Vector3){center.x + side_sign * width * 0.27f,
                              height * 0.72f, z + depth + 0.12f},
                    (Vector3){0.72f, 1.75f, 0.06f},
                    side < 0 ? WORLD_GOLD : WORLD_TEAL);
        }
    }
}

static void DrawTerrainPatchAtTop(float x, float z, float width, float depth,
                                  float top, Color color)
{
    const float thickness = 0.060f;
    DrawCubeV((Vector3){x + width * 0.5f, top - thickness * 0.5f,
                        z + depth * 0.5f},
              (Vector3){width, thickness, depth}, color);
}

/* The orthographic street camera shows roughly 22 metres across. This larger
   radius preserves tall silhouettes beyond the screen edge while avoiding
   hundreds of immediate-mode calls for distant scenery. Culling uses the
   clamped camera target rather than the player so it remains stable at the
   world boundary. */
static bool SceneryFootprintVisibleWithin(Rectangle footprint, Vector3 focus,
                                          float visibility_radius)
{
    float nearest_x = fmaxf(footprint.x,
                            fminf(focus.x, footprint.x + footprint.width));
    float nearest_z = fmaxf(footprint.y,
                            fminf(focus.z, footprint.y + footprint.height));
    float delta_x = focus.x - nearest_x;
    float delta_z = focus.z - nearest_z;
    return delta_x * delta_x + delta_z * delta_z <=
           visibility_radius * visibility_radius;
}

static bool SceneryFootprintVisible(Rectangle footprint, Vector3 focus)
{
    return SceneryFootprintVisibleWithin(footprint, focus, 30.0f);
}

static bool SceneryPointVisible(float x, float z, Vector3 focus)
{
    return SceneryFootprintVisible((Rectangle){x, z, 0.0f, 0.0f}, focus);
}

static bool RoomDetailPointVisible(float x, float z, Vector3 focus)
{
    return SceneryFootprintVisibleWithin(
        (Rectangle){x, z, 0.0f, 0.0f}, focus, 23.0f);
}

static void DrawStreetLantern(float x, float z)
{
    Color metal = (Color){43, 43, 39, 255};
    DrawCylinder((Vector3){x, 0.05f, z}, 0.23f, 0.28f, 0.10f, 8,
                 ShadeColor(metal, 0.72f));
    DrawCylinder((Vector3){x, 1.12f, z}, 0.055f, 0.075f, 2.14f, 8, metal);
    DrawBox((Vector3){x, 2.18f, z}, (Vector3){0.42f, 0.08f, 0.42f}, metal);
    DrawSmallSphere((Vector3){x, 1.98f, z}, 0.18f, WORLD_GOLD);
    DrawBox((Vector3){x, 1.96f, z}, (Vector3){0.27f, 0.30f, 0.27f},
            Fade((Color){234, 181, 77, 255}, 0.82f));
    DrawBox((Vector3){x, 1.76f, z}, (Vector3){0.34f, 0.07f, 0.34f}, metal);
}

static void DrawWayfarerGate(Color accent, bool sightline_cut)
{
    Color wood = WORLD_WOOD;
    float left = 8.66f;
    float right = 14.34f;
    float z = 10.56f;
    if (sightline_cut) {
        DrawBox((Vector3){left, 0.06f, z},
                (Vector3){0.22f, 0.12f, 0.22f}, wood);
        DrawBox((Vector3){right, 0.06f, z},
                (Vector3){0.22f, 0.12f, 0.22f}, wood);
        return;
    }
    DrawBox((Vector3){left, 1.16f, z}, (Vector3){0.22f, 2.32f, 0.22f}, wood);
    DrawBox((Vector3){right, 1.16f, z}, (Vector3){0.22f, 2.32f, 0.22f}, wood);
    DrawBox((Vector3){11.50f, 2.24f, z},
            (Vector3){5.90f, 0.22f, 0.24f}, wood);
    for (int32_t i = 0; i < 3; ++i) {
        float x = 10.05f + (float)i * 1.45f;
        DrawBox((Vector3){x, 1.91f, z + 0.07f},
                (Vector3){0.56f, 0.48f, 0.07f},
                i == 1 ? WORLD_GOLD : accent);
    }
}

static void DrawCroftScarecrow(float hunger)
{
    const float x = 8.10f;
    const float z = 25.00f;
    Color wood = WORLD_WOOD;
    Color cloth = BlendColor(WORLD_EARTH_LIGHT,
                             WORLD_ROAD_SHADOW, hunger);
    DrawBox((Vector3){x, 0.92f, z}, (Vector3){0.12f, 1.84f, 0.12f}, wood);
    DrawBox((Vector3){x, 1.38f, z}, (Vector3){1.38f, 0.10f, 0.10f}, wood);
    DrawBox((Vector3){x, 1.20f, z + 0.04f},
            (Vector3){0.78f, 0.68f, 0.12f}, cloth);
    DrawSmallSphere((Vector3){x, 1.80f, z}, 0.23f,
                    BlendColor(WORLD_CROP_LIGHT, WORLD_WOOD_LIGHT, 0.34f));
    DrawBox((Vector3){x, 2.03f, z}, (Vector3){0.72f, 0.08f, 0.44f}, wood);
    DrawBox((Vector3){x, 2.17f, z}, (Vector3){0.38f, 0.30f, 0.34f}, wood);
}

static void DrawMineWaystone(void)
{
    const float x = 18.0f;
    const float z = 54.72f;
    Color stone = WORLD_STONE;
    DrawBox((Vector3){x, 0.14f, z}, (Vector3){0.84f, 0.28f, 0.72f},
            ShadeColor(stone, 0.70f));
    DrawBox((Vector3){x, 0.82f, z}, (Vector3){0.56f, 1.38f, 0.46f}, stone);
    DrawBox((Vector3){x, 1.56f, z}, (Vector3){0.70f, 0.16f, 0.56f},
            ShadeColor(stone, 1.12f));
    DrawSmallSphere((Vector3){x, 1.03f, z + 0.27f}, 0.10f, WORLD_VIOLET);

    /* Two rails and broad sleepers turn the colored road into an explicit
       visual sentence: this way leads to the mine beyond the next room. */
    Color rail = WORLD_METAL_SHADOW;
    Color sleeper = WORLD_WOOD;
    DrawBox((Vector3){22.65f, 0.055f, 54.92f},
            (Vector3){8.20f, 0.075f, 0.09f}, rail);
    DrawBox((Vector3){22.65f, 0.055f, 55.72f},
            (Vector3){8.20f, 0.075f, 0.09f}, rail);
    for (int32_t sleeper_index = 0; sleeper_index < 10; ++sleeper_index) {
        DrawBox((Vector3){18.85f + (float)sleeper_index * 0.84f,
                          0.035f, 55.32f},
                (Vector3){0.14f, 0.055f, 1.18f}, sleeper);
    }
    /* The rail head ends in a working ore station, so the mine reads before
       the entrance itself comes into view. */
    DrawBox((Vector3){26.45f, 0.44f, 54.35f},
            (Vector3){1.45f, 0.88f, 1.05f},
            WORLD_WOOD_SHADOW);
    DrawBox((Vector3){26.45f, 0.91f, 54.35f},
            (Vector3){1.62f, 0.10f, 1.18f}, sleeper);
    for (int32_t ore = 0; ore < 4; ++ore) {
        float ore_x = 25.98f + (float)(ore & 1) * 0.64f;
        float ore_z = 54.02f + (float)(ore >> 1) * 0.54f;
        DrawTiltedBox((Vector3){ore_x, 1.02f, ore_z},
                      (Vector3){0.42f, 0.34f, 0.38f},
                      (Vector3){0.0f, 1.0f, 0.0f},
                      (float)ore * 19.0f, ShadeColor(stone, 0.78f));
    }
}

static void DrawArtisanSign(Color kingdom)
{
    const float x = 30.95f;
    const float z = 24.30f;
    Color wood = WORLD_WOOD_SHADOW;
    DrawBox((Vector3){x, 1.16f, z}, (Vector3){0.14f, 2.32f, 0.14f}, wood);
    DrawBox((Vector3){x + 0.48f, 2.12f, z},
            (Vector3){1.08f, 0.12f, 0.14f}, wood);
    DrawBox((Vector3){x + 0.86f, 1.72f, z + 0.04f},
            (Vector3){0.72f, 0.66f, 0.12f},
            BlendColor(WORLD_WOOD_LIGHT, kingdom, 0.26f));
    DrawSmallSphere((Vector3){x + 0.86f, 1.73f, z + 0.12f},
                    0.11f, WORLD_GOLD);
}

static void DrawWorkshopForge(Color kingdom)
{
    const float x = 37.85f;
    const float z = 23.08f;
    Color brick = BlendColor(WORLD_EARTH, kingdom, 0.12f);
    Color iron = WORLD_METAL_SHADOW;
    Color timber = WORLD_WOOD;
    DrawBox((Vector3){x, 0.48f, z},
            (Vector3){1.72f, 0.96f, 0.82f}, brick);
    DrawBox((Vector3){x, 1.10f, z - 0.10f},
            (Vector3){1.38f, 0.42f, 0.58f}, ShadeColor(brick, 0.78f));
    DrawBox((Vector3){x + 0.58f, 2.28f, z - 0.18f},
            (Vector3){0.46f, 2.80f, 0.46f}, iron);
    DrawBox((Vector3){x + 0.58f, 3.72f, z - 0.18f},
            (Vector3){0.62f, 0.18f, 0.62f}, ShadeColor(iron, 0.76f));
    DrawBox((Vector3){x - 0.14f, 0.70f, z + 0.44f},
            (Vector3){0.74f, 0.18f, 0.12f}, WORLD_GOLD);
    DrawSmallSphere((Vector3){x - 0.14f, 0.72f, z + 0.54f},
                    0.18f, BlendColor(WORLD_GOLD, WORLD_DANGER, 0.48f));
    DrawBox((Vector3){x - 1.45f, 0.30f, z + 0.05f},
            (Vector3){0.14f, 0.60f, 1.28f}, timber);
    for (int32_t billet = 0; billet < 3; ++billet) {
        DrawCylinderEx((Vector3){x - 1.72f, 0.16f + (float)billet * 0.15f,
                                 z - 0.40f},
                       (Vector3){x - 1.18f, 0.16f + (float)billet * 0.15f,
                                 z + 0.48f},
                       0.08f, 0.08f, 7, ShadeColor(timber, 1.08f));
    }
}

static Color PlaceIdentityAccent(const CcLocalPlaceProfile *profile,
                                 Color kingdom)
{
    if (profile == NULL) return kingdom;
    switch (profile->function) {
        case CC_SETTLEMENT_FARMING: return WORLD_CROP_LIGHT;
        case CC_SETTLEMENT_MINING: return WORLD_METAL_LIGHT;
        case CC_SETTLEMENT_MARKET: return WORLD_TEAL;
        case CC_SETTLEMENT_FORTRESS: return WORLD_DANGER;
        case CC_SETTLEMENT_CAPITAL: return WORLD_GOLD;
        case CC_SETTLEMENT_DUNGEON_TOWN: return WORLD_VIOLET;
    }
    return kingdom;
}

static void DrawTownSquareFocus(Color kingdom,
                                const CcLocalPlaceProfile *profile)
{
    const float x = CC_LOCAL_NOTICE_X;
    const float z = CC_LOCAL_NOTICE_Z;
    Color stone = WORLD_STONE_LIGHT;
    Color accent = PlaceIdentityAccent(profile, kingdom);
    /* A low civic seal gathers the plaza around the notice board without
       creating a new collision step or blocking click movement. */
    DrawCylinder((Vector3){x, 0.015f, z}, 2.65f, 2.65f, 0.045f, 20,
                 ShadeColor(stone, 0.78f));
    DrawCylinder((Vector3){x, 0.052f, z}, 2.22f, 2.22f, 0.035f, 20,
                 stone);
    if (profile != NULL && profile->function == CC_SETTLEMENT_FARMING) {
        for (int32_t row = -1; row <= 1; ++row) {
            DrawTiltedBox((Vector3){x, 0.080f, z + (float)row * 0.48f},
                          (Vector3){3.62f, 0.018f, 0.12f},
                          (Vector3){0.0f, 1.0f, 0.0f},
                          (float)row * 4.0f, accent);
        }
    } else if (profile != NULL &&
               profile->function == CC_SETTLEMENT_MINING) {
        DrawBox((Vector3){x - 0.34f, 0.080f, z},
                (Vector3){0.12f, 0.018f, 3.70f}, accent);
        DrawBox((Vector3){x + 0.34f, 0.080f, z},
                (Vector3){0.12f, 0.018f, 3.70f}, accent);
        DrawTiltedBox((Vector3){x, 0.095f, z},
                      (Vector3){0.72f, 0.035f, 0.72f},
                      (Vector3){0.0f, 1.0f, 0.0f}, 45.0f,
                      WORLD_VIOLET);
    } else if (profile != NULL &&
               profile->function == CC_SETTLEMENT_FORTRESS) {
        DrawTiltedBox((Vector3){x, 0.080f, z},
                      (Vector3){3.30f, 0.018f, 0.15f},
                      (Vector3){0.0f, 1.0f, 0.0f}, 45.0f, accent);
        DrawTiltedBox((Vector3){x, 0.082f, z},
                      (Vector3){3.30f, 0.018f, 0.15f},
                      (Vector3){0.0f, 1.0f, 0.0f}, -45.0f, accent);
    } else if (profile != NULL &&
               profile->function == CC_SETTLEMENT_CAPITAL) {
        for (int32_t spoke = 0; spoke < 4; ++spoke) {
            DrawTiltedBox((Vector3){x, 0.080f, z},
                          (Vector3){3.52f, 0.018f, 0.11f},
                          (Vector3){0.0f, 1.0f, 0.0f},
                          (float)spoke * 45.0f, accent);
        }
    } else if (profile != NULL &&
               profile->function == CC_SETTLEMENT_DUNGEON_TOWN) {
        for (int32_t bar = -1; bar <= 1; ++bar) {
            DrawTiltedBox((Vector3){x + (float)bar * 0.44f, 0.080f, z},
                          (Vector3){0.12f, 0.018f, 3.45f},
                          (Vector3){0.0f, 1.0f, 0.0f},
                          (float)bar * 5.0f, accent);
        }
    } else {
        DrawBox((Vector3){x, 0.078f, z},
                (Vector3){3.70f, 0.018f, 0.13f}, accent);
        DrawBox((Vector3){x, 0.080f, z},
                (Vector3){0.13f, 0.018f, 3.70f}, accent);
    }
}

static void DrawCoachHitch(const CcSettlement *place)
{
    Color wood = WORLD_WOOD;
    float z = 55.36f;
    DrawBox((Vector3){35.30f, 0.62f, z}, (Vector3){0.18f, 1.24f, 0.18f}, wood);
    DrawBox((Vector3){38.20f, 0.62f, z}, (Vector3){0.18f, 1.24f, 0.18f}, wood);
    DrawBox((Vector3){36.75f, 0.92f, z}, (Vector3){3.08f, 0.16f, 0.18f}, wood);
    DrawBox((Vector3){36.75f, 1.62f, z},
            (Vector3){0.14f, 1.40f, 0.14f}, wood);
    DrawBox((Vector3){36.75f, 2.18f, z + 0.05f},
            (Vector3){1.30f, 0.68f, 0.12f},
            WORLD_WOOD_LIGHT);
    DrawBox((Vector3){36.75f, 2.70f, z - 0.02f},
            (Vector3){3.48f, 0.18f, 1.18f},
            ShadeColor(wood, 0.88f));
    DrawBox((Vector3){36.75f, 2.82f, z - 0.02f},
            (Vector3){3.08f, 0.12f, 1.44f},
            WORLD_EARTH_LIGHT);
    DrawCylinder((Vector3){36.75f, 2.03f, z + 0.13f},
                 0.19f, 0.19f, 0.08f, 10, WORLD_GOLD);
    int32_t barrels = place != NULL ? place->stock[CC_GOOD_MATERIAL] / 24 : 1;
    if (barrels < 1) barrels = 1;
    if (barrels > 3) barrels = 3;
    for (int32_t i = 0; i < barrels; ++i) {
        DrawCylinder((Vector3){35.65f + (float)i * 0.58f, 0.05f, z + 0.42f},
                     0.25f, 0.25f, 0.58f, 10,
                     WORLD_WOOD_LIGHT);
    }
    for (int32_t wheel = 0; wheel < 2; ++wheel) {
        float wheel_z = z - 0.28f + (float)wheel * 0.58f;
        DrawCylinderEx((Vector3){38.50f, 0.62f, wheel_z},
                       (Vector3){38.58f, 0.62f, wheel_z},
                       0.48f, 0.48f, 12, ShadeColor(wood, 1.08f));
    }
}

static void DrawMillersGranary(float hunger)
{
    const float x = 64.14f;
    const float z = 51.02f;
    Color timber = WORLD_WOOD_LIGHT;
    Color plaster = BlendColor(WORLD_STONE_LIGHT,
                               WORLD_ROAD, hunger * 0.60f);
    DrawCylinder((Vector3){x, 0.08f, z}, 0.90f, 0.96f, 2.36f, 12, plaster);
    DrawCylinder((Vector3){x, 2.43f, z}, 0.10f, 1.10f, 0.88f, 12, timber);
    DrawBox((Vector3){x, 0.75f, z + 0.93f},
            (Vector3){0.58f, 1.20f, 0.08f}, timber);
    DrawBox((Vector3){x, 1.45f, z + 0.98f},
            (Vector3){0.72f, 0.10f, 0.12f}, WORLD_GOLD);
    int32_t sacks = hunger > 0.45f ? 2 : 5;
    for (int32_t sack = 0; sack < sacks; ++sack) {
        float sack_x = x - 1.28f + (float)(sack % 3) * 0.48f;
        float sack_z = z + 0.64f + (float)(sack / 3) * 0.40f;
        DrawCharacterEllipsoid((Vector3){sack_x, 0.26f, sack_z},
                               (Vector3){0.25f, 0.32f, 0.20f},
                               BlendColor(WORLD_CROP_LIGHT,
                                          WORLD_ROAD_LIGHT, 0.42f));
    }
}

static void DrawEastWindmill(Color kingdom, float hunger)
{
    const float x = 81.4f;
    const float z = 47.0f;
    Color tower = BlendColor(WORLD_STONE_LIGHT,
                             WORLD_ROAD, hunger * 0.50f);
    Color timber = BlendColor(WORLD_WOOD, kingdom, 0.16f);
    DrawCylinder((Vector3){x, 0.05f, z}, 0.74f, 1.08f, 3.45f, 12, tower);
    DrawCylinder((Vector3){x, 3.50f, z}, 0.08f, 0.92f, 0.82f, 12, timber);
    DrawCylinderEx((Vector3){x, 3.28f, z + 0.36f},
                   (Vector3){x, 3.28f, z + 0.92f},
                   0.14f, 0.14f, 10, ShadeColor(timber, 0.72f));
    for (int32_t blade = 0; blade < 4; ++blade) {
        rlPushMatrix();
        rlTranslatef(x, 3.28f, z + 0.96f);
        rlRotatef(18.0f + (float)blade * 90.0f, 0.0f, 0.0f, 1.0f);
        DrawBox((Vector3){0.0f, 1.02f, 0.0f},
                (Vector3){0.18f, 2.04f, 0.10f}, timber);
        DrawBox((Vector3){0.20f, 1.38f, 0.0f},
                (Vector3){0.38f, 0.92f, 0.075f},
                Fade(BlendColor(WORLD_INK, WORLD_CROP_LIGHT, 0.42f), 0.78f));
        rlPopMatrix();
    }
    DrawSmallSphere((Vector3){x, 3.28f, z + 1.02f}, 0.20f, WORLD_GOLD);
}

static void DrawPlaceLandmark(const CcLocalPlaceLandmark *landmark,
                              Color kingdom)
{
    if (landmark == NULL) return;
    float center_x = landmark->x + landmark->width * 0.5f;
    float center_z = landmark->z + landmark->depth * 0.5f;
    float height = landmark->height;
    Color wall = WORLD_WOOD;
    Color roof = WORLD_WOOD_SHADOW;
    Color trim = WORLD_CROP_LIGHT;
    switch (landmark->family) {
        case CC_LOCAL_LANDMARK_AGRICULTURE:
            wall = BlendColor(WORLD_WOOD, WORLD_CROP, 0.30f);
            roof = WORLD_CROP_SHADOW;
            trim = WORLD_CROP_LIGHT;
            break;
        case CC_LOCAL_LANDMARK_INDUSTRY:
            wall = WORLD_STONE_SHADOW;
            roof = WORLD_METAL_SHADOW;
            trim = WORLD_METAL_LIGHT;
            break;
        case CC_LOCAL_LANDMARK_COMMERCE:
            wall = BlendColor(WORLD_STONE_LIGHT, kingdom, 0.16f);
            roof = BlendColor(WORLD_WOOD_SHADOW, kingdom, 0.22f);
            trim = WORLD_GOLD;
            break;
        case CC_LOCAL_LANDMARK_MILITARY:
            wall = BlendColor(WORLD_STONE, kingdom, 0.20f);
            roof = WORLD_METAL_SHADOW;
            trim = kingdom;
            break;
        case CC_LOCAL_LANDMARK_CIVIC:
            wall = WORLD_STONE_LIGHT;
            roof = BlendColor(WORLD_STONE, kingdom, 0.20f);
            trim = WORLD_GOLD;
            break;
        case CC_LOCAL_LANDMARK_EXPEDITION:
            wall = BlendColor(WORLD_STONE_SHADOW, WORLD_VIOLET, 0.16f);
            roof = WORLD_WOOD_SHADOW;
            trim = WORLD_VIOLET;
            break;
    }

    DrawBox((Vector3){center_x, 0.16f, center_z},
            (Vector3){landmark->width, 0.32f, landmark->depth},
            ShadeColor(wall, 0.62f));

    if (landmark->family == CC_LOCAL_LANDMARK_AGRICULTURE &&
        landmark->variant == 1) {
        float body_height = height * 0.72f;
        DrawBox((Vector3){center_x, body_height * 0.16f, center_z},
                (Vector3){landmark->width, body_height * 0.32f,
                          landmark->depth}, wall);
        for (int32_t silo = 0; silo < 2; ++silo) {
            float silo_x = landmark->x + landmark->width *
                           (silo == 0 ? 0.31f : 0.69f);
            float radius = fminf(landmark->width * 0.19f,
                                 landmark->depth * 0.34f);
            DrawCylinder((Vector3){silo_x, height * 0.20f, center_z},
                         radius * 0.88f, radius, height * 0.62f, 12, wall);
            DrawCylinder((Vector3){silo_x, height * 0.82f, center_z},
                         0.04f, radius * 1.06f, height * 0.18f, 12, roof);
        }
        return;
    }

    if (landmark->family == CC_LOCAL_LANDMARK_CIVIC &&
        landmark->variant == 1) {
        DrawBox((Vector3){center_x, height * 0.12f, center_z},
                (Vector3){landmark->width, height * 0.24f,
                          landmark->depth}, ShadeColor(wall, 0.78f));
        DrawBox((Vector3){center_x, height * 0.52f, center_z},
                (Vector3){landmark->width * 0.42f, height * 0.80f,
                          landmark->depth * 0.42f}, wall);
        DrawBox((Vector3){center_x, height * 0.94f, center_z},
                (Vector3){landmark->width * 0.58f, height * 0.14f,
                          landmark->depth * 0.58f}, trim);
        return;
    }

    float body_height = height * 0.88f;
    DrawBox((Vector3){center_x, body_height * 0.50f, center_z},
            (Vector3){landmark->width, body_height, landmark->depth}, wall);
    DrawBox((Vector3){center_x, body_height + height * 0.06f, center_z},
            (Vector3){landmark->width + 0.34f, height * 0.12f,
                      landmark->depth + 0.34f}, roof);
    DrawBox((Vector3){center_x, body_height * 0.48f,
                      landmark->z + landmark->depth + 0.035f},
            (Vector3){landmark->width * 0.24f, body_height * 0.56f, 0.07f},
            ShadeColor(roof, 0.72f));

    if (landmark->family == CC_LOCAL_LANDMARK_INDUSTRY) {
        int32_t stacks = landmark->variant == 1 ? 3 : 2;
        for (int32_t stack = 0; stack < stacks; ++stack) {
            float stack_x = landmark->x + landmark->width *
                ((float)(stack + 1) / (float)(stacks + 1));
            DrawCylinder((Vector3){stack_x, body_height * 0.82f,
                                   landmark->z + landmark->depth * 0.68f},
                         0.18f, 0.24f, height * 0.28f, 8,
                         WORLD_METAL_SHADOW);
        }
    } else if (landmark->family == CC_LOCAL_LANDMARK_COMMERCE) {
        for (int32_t panel = 0; panel < 4; ++panel) {
            float panel_x = landmark->x + landmark->width *
                ((float)panel + 0.5f) / 4.0f;
            DrawBox((Vector3){panel_x, body_height * 0.58f,
                              landmark->z + landmark->depth + 0.26f},
                    (Vector3){landmark->width * 0.22f, 0.12f, 0.52f},
                    (panel & 1) == 0 ? trim : roof);
        }
    } else if (landmark->family == CC_LOCAL_LANDMARK_MILITARY) {
        int32_t merlons = landmark->variant == 1 ? 3 : 5;
        for (int32_t merlon = 0; merlon < merlons; ++merlon) {
            float merlon_x = landmark->x + landmark->width *
                ((float)merlon + 0.5f) / (float)merlons;
            DrawBox((Vector3){merlon_x, body_height + height * 0.16f,
                              center_z},
                    (Vector3){landmark->width / (float)merlons * 0.52f,
                              height * 0.20f, landmark->depth * 0.18f},
                    trim);
        }
    } else if (landmark->family == CC_LOCAL_LANDMARK_CIVIC) {
        DrawBox((Vector3){center_x, body_height * 0.62f,
                          landmark->z + landmark->depth + 0.08f},
                (Vector3){landmark->width * 0.72f, height * 0.10f, 0.16f},
                trim);
    } else if (landmark->family == CC_LOCAL_LANDMARK_EXPEDITION) {
        DrawBox((Vector3){center_x, body_height + height * 0.20f, center_z},
                (Vector3){0.26f, height * 0.30f, 0.26f},
                WORLD_WOOD_SHADOW);
        DrawSmallSphere((Vector3){center_x, height * 1.02f, center_z},
                        0.22f, trim);
    }
}

static void DrawPlaceLandmarks(Color kingdom, Vector3 focus)
{
    for (int32_t i = 0; i < CC_LOCAL_PLACE_LANDMARK_COUNT; ++i) {
        const CcLocalPlaceLandmark *landmark = ActivePlaceLandmarkAt(i);
        if (landmark == NULL) continue;
        Rectangle footprint = PlaceLandmarkFootprint(landmark);
        if (!SceneryFootprintVisible(footprint, focus)) continue;
        rlPushMatrix();
        rlTranslatef(0.0f, TerrainFootprintHeight(footprint), 0.0f);
        DrawPlaceLandmark(landmark, kingdom);
        rlPopMatrix();
    }
}

static void DrawRoomLandmarks(const CcSettlement *place, Color kingdom,
                              const CcLocalPlaceProfile *profile,
                              Vector3 focus, bool wayfarer_gate_sightline_cut)
{
    float hunger = place != NULL ? (float)place->hunger / 100.0f : 0.0f;
    if (RoomDetailPointVisible(11.50f, 10.56f, focus)) {
        rlPushMatrix();
        rlTranslatef(0.0f, CcLocalTerrainHeightAt(11.50f, 10.56f), 0.0f);
        DrawWayfarerGate(kingdom, wayfarer_gate_sightline_cut);
        rlPopMatrix();
    }
    if (RoomDetailPointVisible(8.10f, 25.00f, focus)) {
        rlPushMatrix();
        rlTranslatef(0.0f, CcLocalTerrainHeightAt(8.10f, 25.00f), 0.0f);
        DrawCroftScarecrow(hunger);
        rlPopMatrix();
    }
    if (RoomDetailPointVisible(18.0f, 54.72f, focus)) {
        rlPushMatrix();
        rlTranslatef(0.0f, CcLocalTerrainHeightAt(18.0f, 54.72f), 0.0f);
        DrawMineWaystone();
        rlPopMatrix();
    }
    if (RoomDetailPointVisible(30.95f, 24.30f, focus)) {
        rlPushMatrix();
        rlTranslatef(0.0f, CcLocalTerrainHeightAt(30.95f, 24.30f), 0.0f);
        DrawArtisanSign(kingdom);
        DrawWorkshopForge(kingdom);
        rlPopMatrix();
    }
    if (RoomDetailPointVisible(CC_LOCAL_NOTICE_X, CC_LOCAL_NOTICE_Z, focus)) {
        rlPushMatrix();
        rlTranslatef(0.0f,
                     CcLocalTerrainHeightAt(CC_LOCAL_NOTICE_X,
                                            CC_LOCAL_NOTICE_Z), 0.0f);
        DrawTownSquareFocus(kingdom, profile);
        rlPopMatrix();
    }
    if (RoomDetailPointVisible(36.75f, 55.36f, focus)) {
        rlPushMatrix();
        rlTranslatef(0.0f, CcLocalTerrainHeightAt(36.75f, 55.36f), 0.0f);
        DrawCoachHitch(place);
        rlPopMatrix();
    }
    if (RoomDetailPointVisible(64.14f, 51.02f, focus)) {
        rlPushMatrix();
        rlTranslatef(0.0f, CcLocalTerrainHeightAt(64.14f, 51.02f), 0.0f);
        DrawMillersGranary(hunger);
        rlPopMatrix();
    }
    if (RoomDetailPointVisible(81.4f, 47.0f, focus)) {
        rlPushMatrix();
        rlTranslatef(0.0f, CcLocalTerrainHeightAt(81.4f, 47.0f), 0.0f);
        DrawEastWindmill(kingdom, hunger);
        rlPopMatrix();
    }
}

static bool TerrainPointInRectangle(float x, float z, Rectangle rectangle)
{
    return x >= rectangle.x && x <= rectangle.x + rectangle.width &&
           z >= rectangle.y && z <= rectangle.y + rectangle.height;
}

static const Rectangle TERRAIN_FIELDS[] = {
    {3.0f, 17.0f, 13.0f, 9.0f}, {3.0f, 29.0f, 13.0f, 9.0f},
    {3.0f, 41.0f, 13.0f, 9.0f}, {59.0f, 43.0f, 14.0f, 10.0f},
    {76.0f, 43.0f, 15.0f, 10.0f},
};

static float TerrainRectangleInset(float x, float z, Rectangle rectangle)
{
    if (!TerrainPointInRectangle(x, z, rectangle)) return 0.0f;
    return fminf(fminf(x - rectangle.x,
                       rectangle.x + rectangle.width - x),
                 fminf(z - rectangle.y,
                       rectangle.y + rectangle.height - z));
}

static float TerrainRectangleSignedInset(float x, float z,
                                          Rectangle rectangle)
{
    if (TerrainPointInRectangle(x, z, rectangle)) {
        return TerrainRectangleInset(x, z, rectangle);
    }
    float closest_x = TerrainClamp(
        x, rectangle.x, rectangle.x + rectangle.width);
    float closest_z = TerrainClamp(
        z, rectangle.y, rectangle.y + rectangle.height);
    float dx = x - closest_x;
    float dz = z - closest_z;
    return -sqrtf(dx * dx + dz * dz);
}

static float TerrainRoadCenterline(const TerrainRoad *road,
                                   int32_t road_index, float along)
{
    float start = road->runs_east_west ? road->footprint.x :
                                         road->footprint.y;
    float length = road->runs_east_west ? road->footprint.width :
                                          road->footprint.height;
    float amount = TerrainClamp((along - start) / fmaxf(length, 0.01f),
                                0.0f, 1.0f);
    float cross = road->runs_east_west ?
        road->footprint.y + road->footprint.height * 0.5f :
        road->footprint.x + road->footprint.width * 0.5f;
    float cross_width = road->runs_east_west ? road->footprint.height :
                                               road->footprint.width;
    float bend = sinf(amount * PI) *
                 sinf(amount * 2.35f + (float)road_index * 1.71f) *
                 fminf(0.48f, cross_width * 0.105f);
    float wandering = TerrainValueNoise(
        along + (float)road_index * 7.0f,
        (float)road_index * 5.0f, 8.0f, 71U) *
        fminf(0.16f, cross_width * 0.035f) * sinf(amount * PI);
    return cross + bend + wandering;
}

static float TerrainRoadSignedInset(const TerrainRoad *road,
                                    int32_t road_index, float x, float z)
{
    float along = road->runs_east_west ? x : z;
    float cross = road->runs_east_west ? z : x;
    float start = road->runs_east_west ? road->footprint.x :
                                         road->footprint.y;
    float length = road->runs_east_west ? road->footprint.width :
                                          road->footprint.height;
    float cross_width = road->runs_east_west ? road->footprint.height :
                                               road->footprint.width;
    float amount = TerrainClamp((along - start) / fmaxf(length, 0.01f),
                                0.0f, 1.0f);
    float width_noise = TerrainValueNoise(
        along - (float)road_index * 4.0f,
        (float)road_index * 9.0f, 5.5f, 72U);
    float half_width = cross_width * 0.5f * (0.94f + width_noise * 0.07f);
    float cross_inset = half_width - fabsf(
        cross - TerrainRoadCenterline(road, road_index, along));
    float end_inset = fminf(along - start, start + length - along);
    /* Soften square caps but leave enough overlap for road junctions. */
    end_inset += sinf(amount * PI) * 0.16f;
    return fminf(cross_inset, end_inset);
}

static int32_t TerrainVisibleRoadCount(void)
{
    /* The final terrain road is an unpaved raid ingress. */
    return (int32_t)(sizeof(TERRAIN_ROADS) / sizeof(TERRAIN_ROADS[0])) - 1;
}

static float TerrainPlaceRoadAmount(float x, float z)
{
    float amount = 0.0f;
    int32_t index_offset =
        (int32_t)(sizeof(TERRAIN_ROADS) / sizeof(TERRAIN_ROADS[0]));
    for (int32_t i = 0; i < CC_LOCAL_PLACE_ROAD_COUNT; ++i) {
        TerrainRoad road = PlaceTerrainRoad(ActivePlaceRoadAt(i));
        int32_t road_index = index_offset + i;
        float inset = TerrainRoadSignedInset(&road, road_index, x, z);
        float edge_breakup = TerrainValueNoise(
            x + (float)road_index * 3.7f,
            z - (float)road_index * 2.9f, 4.5f, 17U) * 0.24f;
        amount = fmaxf(amount,
                       TerrainSmooth01(inset / 0.68f + edge_breakup));
    }
    return amount;
}

static float TerrainRoadAmount(float x, float z)
{
    float amount = 0.0f;
    for (int32_t i = 0; i < TerrainVisibleRoadCount(); ++i) {
        float inset = TerrainRoadSignedInset(&TERRAIN_ROADS[i], i, x, z);
        float edge_breakup = TerrainValueNoise(
            x + (float)i * 3.7f, z - (float)i * 2.9f, 4.5f, 17U) * 0.24f;
        amount = fmaxf(amount,
                       TerrainSmooth01(inset / 0.68f + edge_breakup));
    }
    return fmaxf(amount, TerrainPlaceRoadAmount(x, z));
}

static Color TerrainPlaceRoadColor(CcLocalRoadSurface surface)
{
    switch (surface) {
        case CC_LOCAL_ROAD_FARM_TRACK:
            return BlendColor(WORLD_EARTH, WORLD_CROP_SHADOW, 0.24f);
        case CC_LOCAL_ROAD_INDUSTRIAL:
            return BlendColor(WORLD_ROAD_SHADOW,
                              WORLD_METAL_SHADOW, 0.30f);
        case CC_LOCAL_ROAD_TRADE:
            return BlendColor(WORLD_ROAD_LIGHT, WORLD_GOLD, 0.10f);
        case CC_LOCAL_ROAD_MILITARY:
            return BlendColor(WORLD_STONE, WORLD_ROAD_SHADOW, 0.28f);
        case CC_LOCAL_ROAD_PROCESSIONAL:
            return BlendColor(WORLD_STONE_LIGHT, WORLD_ROAD, 0.20f);
        case CC_LOCAL_ROAD_EXPEDITION:
            return BlendColor(WORLD_EARTH_SHADOW, WORLD_VIOLET, 0.12f);
    }
    return WORLD_ROAD;
}

static float TerrainFieldAmount(float x, float z)
{
    float amount = 0.0f;
    for (int32_t i = 0; i < (int32_t)(sizeof(TERRAIN_FIELDS) /
                                      sizeof(TERRAIN_FIELDS[0])); ++i) {
        Rectangle field = TERRAIN_FIELDS[i];
        float inset = TerrainRectangleSignedInset(x, z, field);
        float broad = TerrainValueNoise(
            x + (float)i * 8.2f, z - (float)i * 5.3f, 6.4f, 73U);
        float scallop = sinf((x - field.x) * 0.48f + (float)i) * 0.16f +
                        sinf((z - field.y) * 0.61f - (float)i) * 0.12f;
        amount = fmaxf(
            amount,
            TerrainSmooth01((inset + broad * 0.48f + scallop) / 0.80f));
    }
    return amount;
}

static bool TerrainPointInPlayableWorld(float x, float z)
{
    return x >= 0.0f && x <= CC_LOCAL_WORLD_WIDTH &&
           z >= 0.0f && z <= CC_LOCAL_WORLD_DEPTH;
}

/* The collision world has a hard gameplay boundary, but the rendered country
   continues beyond it. Matching the generated height at the boundary keeps
   the extension seamless while a small natural delta prevents a flat skirt. */
static float TerrainVisualHeightAt(float x, float z)
{
    if (TerrainPointInPlayableWorld(x, z)) {
        return CcLocalTerrainHeightAt(x, z);
    }
    float edge_x = TerrainClamp(x, 0.0f, CC_LOCAL_WORLD_WIDTH);
    float edge_z = TerrainClamp(z, 0.0f, CC_LOCAL_WORLD_DEPTH);
    float dx = x - edge_x;
    float dz = z - edge_z;
    float distance = sqrtf(dx * dx + dz * dz);
    float edge_height = CcLocalTerrainHeightAt(edge_x, edge_z);
    float natural_delta = TerrainNaturalHeight(x, z) -
                          TerrainNaturalHeight(edge_x, edge_z);
    float natural_weight = TerrainSmooth01(distance / 5.0f);
    return edge_height + natural_delta * natural_weight - distance * 0.035f;
}

static Vector3 TerrainVisualNormalAt(float x, float z)
{
    const float offset = CC_TERRAIN_CELL_SIZE;
    float left = TerrainVisualHeightAt(x - offset, z);
    float right = TerrainVisualHeightAt(x + offset, z);
    float near_height = TerrainVisualHeightAt(x, z - offset);
    float far_height = TerrainVisualHeightAt(x, z + offset);
    Vector3 normal = {left - right, offset * 2.0f,
                      near_height - far_height};
    return Vector3Normalize(normal);
}

static Color TerrainSurfaceColor(const CcSettlement *place,
                                 float x, float z, Vector3 normal)
{
    float hunger = place != NULL ? (float)place->hunger / 100.0f : 0.0f;
    float prosperity = place != NULL ?
                       (float)place->prosperity / 100.0f : 0.5f;
    Color grass = BlendColor(WORLD_GRASS,
                             WORLD_EARTH, hunger * 0.72f);
    float height = TerrainVisualHeightAt(x, z);
    float lowland = TerrainSmooth01((1.35f - height) / 2.40f);
    float highland = TerrainSmooth01((height - 4.20f) / 3.20f);
    grass = BlendColor(grass, WORLD_FOLIAGE_SHADOW, lowland * 0.34f);
    grass = BlendColor(grass, WORLD_GRASS_LIGHT, highland * 0.30f);
    Color color = grass;
    float field_amount = TerrainFieldAmount(x, z);
    Color field = BlendColor(WORLD_CROP,
                             WORLD_CROP_LIGHT, prosperity * 0.26f);
    field = BlendColor(field, grass, hunger * 0.28f);
    color = BlendColor(color, field, field_amount * 0.84f);

    float road_amount = TerrainRoadAmount(x, z);
    Color road = BlendColor(WORLD_ROAD,
                            WORLD_ROAD_LIGHT, prosperity * 0.34f);
    color = BlendColor(color, road, road_amount);
    const CcLocalPlaceRoad *place_road = ActivePlaceRoadAt(0);
    float place_road_amount = TerrainPlaceRoadAmount(x, z);
    if (place_road != NULL) {
        Color place_road_color = TerrainPlaceRoadColor(place_road->surface);
        place_road_color = BlendColor(
            place_road_color, WORLD_ROAD_LIGHT, prosperity * 0.16f);
        color = BlendColor(color, place_road_color, place_road_amount);
    }

    Rectangle plaza = {37.6f, 25.6f, 18.8f, 8.8f};
    float plaza_amount = TerrainSmooth01(
        TerrainRectangleInset(x, z, plaza) / 0.55f);
    Color plaza_color = BlendColor(WORLD_STONE,
                                   WORLD_STONE_LIGHT,
                                   prosperity * 0.36f);
    color = BlendColor(color, plaza_color, plaza_amount);

    Rectangle wayfarer_yard = {1.0f, 0.0f, 14.6f, 11.1f};
    float yard_amount = TerrainSmooth01(
        TerrainRectangleInset(x, z, wayfarer_yard) / 0.52f);
    color = BlendColor(color, WORLD_FOLIAGE_SHADOW, yard_amount);

    float rock_amount = TerrainSmooth01((0.82f - normal.y) / 0.22f);
    Color stone = BlendColor(WORLD_STONE,
                             WORLD_ROAD, highland * 0.36f);
    color = BlendColor(color, stone, rock_amount * 0.78f);

    float broad = TerrainValueNoise(x, z, 11.0f, 21U);
    float detail = TerrainValueNoise(x + 5.0f, z - 9.0f, 3.4f, 22U);
    return ShadeColor(color, 1.0f + broad * 0.035f + detail * 0.014f);
}

typedef struct TerrainMeshWriter {
    Mesh mesh;
    int32_t cursor;
} TerrainMeshWriter;

static void TerrainRenderCacheClear(void)
{
    if (terrain_render_cache.ready) {
        UnloadModel(terrain_render_cache.model);
    }
    terrain_render_cache = (TerrainRenderCache){0};
}

static bool TerrainMeshWriterAllocate(TerrainMeshWriter *writer,
                                      int32_t vertex_count)
{
    if (writer == NULL || vertex_count <= 0) return false;
    *writer = (TerrainMeshWriter){0};
    writer->mesh.vertexCount = vertex_count;
    writer->mesh.triangleCount = vertex_count / 3;
    writer->mesh.vertices = MemAlloc(
        (unsigned int)((size_t)vertex_count * 3U * sizeof(float)));
    writer->mesh.normals = MemAlloc(
        (unsigned int)((size_t)vertex_count * 3U * sizeof(float)));
    writer->mesh.colors = MemAlloc(
        (unsigned int)((size_t)vertex_count * 4U * sizeof(unsigned char)));
    if (writer->mesh.vertices != NULL && writer->mesh.normals != NULL &&
        writer->mesh.colors != NULL) {
        return true;
    }
    if (writer->mesh.vertices != NULL) MemFree(writer->mesh.vertices);
    if (writer->mesh.normals != NULL) MemFree(writer->mesh.normals);
    if (writer->mesh.colors != NULL) MemFree(writer->mesh.colors);
    *writer = (TerrainMeshWriter){0};
    return false;
}

static void TerrainMeshWriteVertex(TerrainMeshWriter *writer,
                                   Vector3 point, Color color)
{
    int32_t vertex = writer->cursor;
    Vector3 normal = TerrainVisualNormalAt(point.x, point.z);
    normal.y = fmaxf(normal.y, 0.62f);
    normal = Vector3Normalize(normal);
    writer->mesh.vertices[vertex * 3 + 0] = point.x;
    writer->mesh.vertices[vertex * 3 + 1] = point.y;
    writer->mesh.vertices[vertex * 3 + 2] = point.z;
    writer->mesh.normals[vertex * 3 + 0] = normal.x;
    writer->mesh.normals[vertex * 3 + 1] = normal.y;
    writer->mesh.normals[vertex * 3 + 2] = normal.z;
    writer->mesh.colors[vertex * 4 + 0] = color.r;
    writer->mesh.colors[vertex * 4 + 1] = color.g;
    writer->mesh.colors[vertex * 4 + 2] = color.b;
    writer->mesh.colors[vertex * 4 + 3] = color.a;
    writer->cursor += 1;
}

static void TerrainMeshWriteCell(TerrainMeshWriter *writer,
                                 const CcSettlement *place,
                                 Vector3 p00, Vector3 p10,
                                 Vector3 p01, Vector3 p11)
{
    Color c00 = TerrainSurfaceColor(
        place, p00.x, p00.z, TerrainVisualNormalAt(p00.x, p00.z));
    Color c10 = TerrainSurfaceColor(
        place, p10.x, p10.z, TerrainVisualNormalAt(p10.x, p10.z));
    Color c01 = TerrainSurfaceColor(
        place, p01.x, p01.z, TerrainVisualNormalAt(p01.x, p01.z));
    Color c11 = TerrainSurfaceColor(
        place, p11.x, p11.z, TerrainVisualNormalAt(p11.x, p11.z));
    TerrainMeshWriteVertex(writer, p00, c00);
    TerrainMeshWriteVertex(writer, p11, c11);
    TerrainMeshWriteVertex(writer, p10, c10);
    TerrainMeshWriteVertex(writer, p00, c00);
    TerrainMeshWriteVertex(writer, p01, c01);
    TerrainMeshWriteVertex(writer, p11, c11);
}

static int32_t TerrainMeshVertexCount(void)
{
    const float margin = 14.0f;
    int32_t cells = 0;
    for (float z = -margin; z < CC_LOCAL_WORLD_DEPTH + margin; z += 1.0f) {
        for (float x = -margin; x < CC_LOCAL_WORLD_WIDTH + margin;
             x += 1.0f) {
            if (x >= 0.0f && x + 1.0f <= CC_LOCAL_WORLD_WIDTH &&
                z >= 0.0f && z + 1.0f <= CC_LOCAL_WORLD_DEPTH) {
                continue;
            }
            cells += 1;
        }
    }
    cells += (int32_t)(CC_LOCAL_WORLD_WIDTH / 0.50f) *
             (int32_t)(CC_LOCAL_WORLD_DEPTH / 0.50f);
    return cells * 6;
}

static bool TerrainRenderCacheBuild(const CcSettlement *place)
{
    TerrainRenderCacheClear();
    TerrainEnsureReady();
    int32_t vertex_count = TerrainMeshVertexCount();
    TerrainMeshWriter writer;
    if (!TerrainMeshWriterAllocate(&writer, vertex_count)) {
        TraceLog(LOG_WARNING,
                 "TERRAIN: could not allocate cached mesh (%d vertices)",
                 vertex_count);
        return false;
    }

    const float margin = 14.0f;
    for (float z = -margin; z < CC_LOCAL_WORLD_DEPTH + margin; z += 1.0f) {
        float far_z = z + 1.0f;
        for (float x = -margin; x < CC_LOCAL_WORLD_WIDTH + margin;
             x += 1.0f) {
            float far_x = x + 1.0f;
            if (x >= 0.0f && far_x <= CC_LOCAL_WORLD_WIDTH &&
                z >= 0.0f && far_z <= CC_LOCAL_WORLD_DEPTH) {
                continue;
            }
            TerrainMeshWriteCell(
                &writer, place,
                (Vector3){x, TerrainVisualHeightAt(x, z), z},
                (Vector3){far_x, TerrainVisualHeightAt(far_x, z), z},
                (Vector3){x, TerrainVisualHeightAt(x, far_z), far_z},
                (Vector3){far_x, TerrainVisualHeightAt(far_x, far_z), far_z});
        }
    }
    const float step = 0.50f;
    for (float z = 0.0f; z < CC_LOCAL_WORLD_DEPTH; z += step) {
        float far_z = fminf(z + step, CC_LOCAL_WORLD_DEPTH);
        for (float x = 0.0f; x < CC_LOCAL_WORLD_WIDTH; x += step) {
            float far_x = fminf(x + step, CC_LOCAL_WORLD_WIDTH);
            TerrainMeshWriteCell(
                &writer, place,
                (Vector3){x, CcLocalTerrainHeightAt(x, z), z},
                (Vector3){far_x, CcLocalTerrainHeightAt(far_x, z), z},
                (Vector3){x, CcLocalTerrainHeightAt(x, far_z), far_z},
                (Vector3){far_x, CcLocalTerrainHeightAt(far_x, far_z), far_z});
        }
    }
    if (writer.cursor != vertex_count) {
        TraceLog(LOG_WARNING,
                 "TERRAIN: cached mesh count mismatch (%d/%d)",
                 writer.cursor, vertex_count);
        UnloadMesh(writer.mesh);
        return false;
    }
    UploadMesh(&writer.mesh, false);
    terrain_render_cache.model = LoadModelFromMesh(writer.mesh);
    ApplyWorldShader(&terrain_render_cache.model);
    terrain_render_cache.seed = street_terrain_seed;
    terrain_render_cache.hunger_band = place != NULL ? place->hunger / 5 : 0;
    terrain_render_cache.prosperity_band =
        place != NULL ? place->prosperity / 5 : 10;
    terrain_render_cache.vertex_count = vertex_count;
    terrain_render_cache.ready = true;
    TraceLog(LOG_INFO, "TERRAIN: cached %d vertices on the GPU", vertex_count);
    return true;
}

static void DrawCachedTerrain(const CcSettlement *place)
{
    int32_t hunger_band = place != NULL ? place->hunger / 5 : 0;
    int32_t prosperity_band = place != NULL ? place->prosperity / 5 : 10;
    bool stale = !terrain_render_cache.ready ||
                 terrain_render_cache.seed != street_terrain_seed ||
                 terrain_render_cache.hunger_band != hunger_band ||
                 terrain_render_cache.prosperity_band != prosperity_band;
    if (stale && !TerrainRenderCacheBuild(place)) return;
    DrawModel(terrain_render_cache.model, (Vector3){0.0f, 0.0f, 0.0f},
              1.0f, WHITE);
}

static void TerrainDetailVertex(Vector3 point, Color color)
{
    Vector3 normal = CcLocalTerrainNormalAt(point.x, point.z);
    normal.y = fmaxf(normal.y, 0.62f);
    normal = Vector3Normalize(normal);
    rlColor4ub(color.r, color.g, color.b, color.a);
    rlNormal3f(normal.x, normal.y, normal.z);
    rlVertex3f(point.x, point.y, point.z);
}

static void TerrainRibbonSegment(Vector2 start, Vector2 end,
                                 float half_width, float lift, Color color)
{
    float dx = end.x - start.x;
    float dz = end.y - start.y;
    float length = sqrtf(dx * dx + dz * dz);
    if (length < 0.001f) return;
    float side_x = -dz / length * half_width;
    float side_z = dx / length * half_width;
    Vector3 right_start = {
        start.x - side_x, 0.0f, start.y - side_z,
    };
    Vector3 left_start = {
        start.x + side_x, 0.0f, start.y + side_z,
    };
    Vector3 right_end = {end.x - side_x, 0.0f, end.y - side_z};
    Vector3 left_end = {end.x + side_x, 0.0f, end.y + side_z};
    right_start.y = CcLocalTerrainHeightAt(right_start.x, right_start.z) +
                    lift;
    left_start.y = CcLocalTerrainHeightAt(left_start.x, left_start.z) + lift;
    right_end.y = CcLocalTerrainHeightAt(right_end.x, right_end.z) + lift;
    left_end.y = CcLocalTerrainHeightAt(left_end.x, left_end.z) + lift;
    TerrainDetailVertex(right_start, color);
    TerrainDetailVertex(left_end, color);
    TerrainDetailVertex(right_end, color);
    TerrainDetailVertex(right_start, color);
    TerrainDetailVertex(left_start, color);
    TerrainDetailVertex(left_end, color);
}

static void DrawTerrainRoadRuts(const CcSettlement *place, Vector3 focus)
{
    float prosperity = place != NULL ?
                       (float)place->prosperity / 100.0f : 0.5f;
    Color paved_rut = BlendColor(WORLD_ROAD_SHADOW,
                                 WORLD_ROAD,
                                 prosperity * 0.28f);
    Color field_track = BlendColor(WORLD_GRASS_SHADOW,
                                   WORLD_ROAD_SHADOW, 0.34f);
    Rectangle plaza = {37.6f, 25.6f, 18.8f, 8.8f};
    int32_t road_count =
        (int32_t)(sizeof(TERRAIN_ROADS) / sizeof(TERRAIN_ROADS[0]));
    rlBegin(RL_TRIANGLES);
    for (int32_t i = 0; i < road_count; ++i) {
        bool rural_track = i == 0 || i == 1 || i == 3 || i == 4 ||
                           i == 9 || i == 12 || i == 13 || i == 14;
        if (!rural_track) continue;
        const TerrainRoad *road = &TERRAIN_ROADS[i];
        Rectangle footprint = road->footprint;
        Color color = i < TerrainVisibleRoadCount() ? paved_rut : field_track;
        float lane_offset = (road->runs_east_west ? footprint.height :
                                                       footprint.width) *
                            0.17f;
        for (int32_t lane = -1; lane <= 1; lane += 2) {
            float start = road->runs_east_west ? footprint.x : footprint.y;
            float finish = start + (road->runs_east_west ? footprint.width :
                                                             footprint.height);
            for (float along = start + 0.35f; along < finish - 0.35f;
                 along += 0.72f) {
                float next = fminf(along + 0.74f, finish - 0.35f);
                float cross = TerrainRoadCenterline(road, i, along) +
                              (float)lane * lane_offset;
                float next_cross = TerrainRoadCenterline(road, i, next) +
                                   (float)lane * lane_offset;
                Vector2 a = road->runs_east_west ?
                    (Vector2){along, cross} : (Vector2){cross, along};
                Vector2 b = road->runs_east_west ?
                    (Vector2){next, next_cross} :
                    (Vector2){next_cross, next};
                Vector2 middle = {(a.x + b.x) * 0.5f,
                                  (a.y + b.y) * 0.5f};
                float focus_x = middle.x - focus.x;
                float focus_z = middle.y - focus.z;
                if (focus_x * focus_x + focus_z * focus_z > 24.0f * 24.0f) {
                    continue;
                }
                if (TerrainPointInRectangle(middle.x, middle.y, plaza)) {
                    continue;
                }
                if (TerrainRoadAmount(middle.x, middle.y) < 0.34f) continue;
                TerrainRibbonSegment(a, b, 0.055f, 0.055f, color);
            }
        }
    }
    for (int32_t i = 0; i < CC_LOCAL_PLACE_ROAD_COUNT; ++i) {
        const CcLocalPlaceRoad *place_road = ActivePlaceRoadAt(i);
        if (place_road == NULL) continue;
        TerrainRoad road = PlaceTerrainRoad(place_road);
        Rectangle footprint = road.footprint;
        int32_t road_index = road_count + i;
        Color color = ShadeColor(
            TerrainPlaceRoadColor(place_road->surface),
            place_road->surface == CC_LOCAL_ROAD_PROCESSIONAL ? 0.82f :
                                                                0.68f);
        float lane_offset = (road.runs_east_west ? footprint.height :
                                                    footprint.width) * 0.17f;
        for (int32_t lane = -1; lane <= 1; lane += 2) {
            float start = road.runs_east_west ? footprint.x : footprint.y;
            float finish = start + (road.runs_east_west ? footprint.width :
                                                             footprint.height);
            for (float along = start + 0.35f; along < finish - 0.35f;
                 along += 0.72f) {
                float next = fminf(along + 0.74f, finish - 0.35f);
                float cross = TerrainRoadCenterline(
                    &road, road_index, along) + (float)lane * lane_offset;
                float next_cross = TerrainRoadCenterline(
                    &road, road_index, next) + (float)lane * lane_offset;
                Vector2 a = road.runs_east_west ?
                    (Vector2){along, cross} : (Vector2){cross, along};
                Vector2 b = road.runs_east_west ?
                    (Vector2){next, next_cross} :
                    (Vector2){next_cross, next};
                Vector2 middle = {(a.x + b.x) * 0.5f,
                                  (a.y + b.y) * 0.5f};
                float focus_x = middle.x - focus.x;
                float focus_z = middle.y - focus.z;
                if (focus_x * focus_x + focus_z * focus_z >
                    24.0f * 24.0f) continue;
                if (TerrainPlaceRoadAmount(middle.x, middle.y) < 0.34f) {
                    continue;
                }
                TerrainRibbonSegment(a, b, 0.055f, 0.055f, color);
            }
        }
    }
    rlEnd();
}

static void DrawTerrainFieldRows(const CcSettlement *place, Vector3 focus)
{
    float hunger = place != NULL ? (float)place->hunger / 100.0f : 0.0f;
    float prosperity = place != NULL ?
                       (float)place->prosperity / 100.0f : 0.5f;
    Color crop = BlendColor(WORLD_CROP,
                            WORLD_CROP_LIGHT, prosperity * 0.44f);
    crop = BlendColor(crop, WORLD_CROP_SHADOW, hunger * 0.42f);
    Color furrow = BlendColor(WORLD_GRASS_SHADOW,
                              WORLD_EARTH_SHADOW, hunger * 0.30f);
    rlBegin(RL_TRIANGLES);
    for (int32_t i = 0; i < (int32_t)(sizeof(TERRAIN_FIELDS) /
                                      sizeof(TERRAIN_FIELDS[0])); ++i) {
        Rectangle field = TERRAIN_FIELDS[i];
        if (!SceneryFootprintVisible(field, focus)) continue;
        int32_t row = 0;
        for (float z = field.y + 0.78f;
             z < field.y + field.height - 0.45f; z += 1.32f, ++row) {
            Color color = BlendColor(crop, furrow,
                                     (row & 1) == 0 ? 0.12f : 0.28f);
            for (float x = field.x + 0.45f;
                 x < field.x + field.width - 0.45f; x += 0.72f) {
                float next = fminf(x + 0.74f,
                                   field.x + field.width - 0.45f);
                float row_bend = sinf(
                    (x - field.x) * 0.36f + (float)i * 1.2f) * 0.10f;
                float next_bend = sinf(
                    (next - field.x) * 0.36f + (float)i * 1.2f) * 0.10f;
                float middle_x = (x + next) * 0.5f;
                float middle_z = z + (row_bend + next_bend) * 0.5f;
                if (TerrainFieldAmount(middle_x, middle_z) < 0.30f) continue;
                TerrainRibbonSegment((Vector2){x, z + row_bend},
                                     (Vector2){next, z + next_bend},
                                     0.080f, 0.055f, color);
            }
        }
    }
    rlEnd();
}

static float TerrainScatter01(int32_t column, int32_t row, uint32_t stream)
{
    uint32_t value = street_terrain_seed ^
        ((uint32_t)column * UINT32_C(0x9e3779b9)) ^
        ((uint32_t)row * UINT32_C(0x85ebca6b)) ^
        (stream * UINT32_C(0xc2b2ae35));
    return (float)(TerrainMix(value) & UINT32_C(0x00ffffff)) /
           (float)UINT32_C(0x00ffffff);
}

static void TerrainCoverVertex(Vector3 point, Color color)
{
    rlColor4ub(color.r, color.g, color.b, color.a);
    rlNormal3f(0.18f, 0.96f, 0.14f);
    rlVertex3f(point.x, point.y, point.z);
}

static void TerrainTuft(float x, float z, float height, float width,
                        float angle, Color bottom, Color top)
{
    float ground = CcLocalTerrainHeightAt(x, z) + 0.025f;
    for (int32_t blade = 0; blade < 2; ++blade) {
        float direction = angle + (float)blade * PI * 0.5f;
        float side_x = cosf(direction) * width;
        float side_z = sinf(direction) * width;
        Vector3 left = {x - side_x, ground, z - side_z};
        Vector3 right = {x + side_x, ground, z + side_z};
        Vector3 peak = {x + sinf(direction) * width * 0.20f,
                        ground + height,
                        z - cosf(direction) * width * 0.20f};
        TerrainCoverVertex(left, bottom);
        TerrainCoverVertex(right, bottom);
        TerrainCoverVertex(peak, top);
        /* The crossed blades are deliberately two-sided. */
        TerrainCoverVertex(peak, top);
        TerrainCoverVertex(right, bottom);
        TerrainCoverVertex(left, bottom);
    }
}

static void DrawTerrainPlantCover(const CcSettlement *place, Vector3 focus)
{
    float hunger = place != NULL ? (float)place->hunger / 100.0f : 0.0f;
    Color grass_bottom = BlendColor(WORLD_GRASS_SHADOW,
                                    WORLD_EARTH_SHADOW,
                                    hunger * 0.62f);
    Color grass_top = BlendColor(WORLD_GRASS_LIGHT,
                                 WORLD_EARTH,
                                 hunger * 0.58f);
    const float spacing = 1.65f;
    int32_t first_column = (int32_t)floorf(
        fmaxf(0.0f, focus.x - 24.0f) / spacing);
    int32_t last_column = (int32_t)ceilf(
        fminf(CC_LOCAL_WORLD_WIDTH, focus.x + 24.0f) / spacing);
    int32_t first_row = (int32_t)floorf(
        fmaxf(0.0f, focus.z - 24.0f) / spacing);
    int32_t last_row = (int32_t)ceilf(
        fminf(CC_LOCAL_WORLD_DEPTH, focus.z + 24.0f) / spacing);

    rlBegin(RL_TRIANGLES);
    for (int32_t row = first_row; row <= last_row; ++row) {
        for (int32_t column = first_column; column <= last_column; ++column) {
            float chance = TerrainScatter01(column, row, 31U);
            float cluster = TerrainValueNoise(
                (float)column * spacing, (float)row * spacing, 8.5f, 61U);
            float threshold = 0.50f - cluster * 0.20f;
            if (chance < threshold) continue;
            float x = ((float)column + 0.16f +
                       TerrainScatter01(column, row, 32U) * 0.68f) * spacing;
            float z = ((float)row + 0.16f +
                       TerrainScatter01(column, row, 33U) * 0.68f) * spacing;
            if (!TerrainPointInPlayableWorld(x, z) ||
                TerrainRoadAmount(x, z) > 0.06f ||
                TerrainFieldAmount(x, z) > 0.05f ||
                TerrainPointInsideMajorFoundation(x, z)) {
                continue;
            }
            Vector3 normal = CcLocalTerrainNormalAt(x, z);
            if (normal.y < 0.70f) continue;
            float scale = 0.78f +
                          TerrainScatter01(column, row, 34U) * 0.52f;
            float angle = TerrainScatter01(column, row, 35U) * PI * 2.0f;
            TerrainTuft(x, z, 0.24f * scale, 0.085f * scale, angle,
                         grass_bottom, grass_top);
        }
    }

    float prosperity = place != NULL ?
                       (float)place->prosperity / 100.0f : 0.5f;
    Color crop_bottom = BlendColor(WORLD_CROP_SHADOW,
                                   WORLD_EARTH, hunger * 0.38f);
    Color crop_top = BlendColor(WORLD_CROP,
                                WORLD_CROP_LIGHT,
                                prosperity * 0.42f);
    for (int32_t field_index = 0;
         field_index < (int32_t)(sizeof(TERRAIN_FIELDS) /
                                  sizeof(TERRAIN_FIELDS[0])); ++field_index) {
        Rectangle field = TERRAIN_FIELDS[field_index];
        if (!SceneryFootprintVisible(field, focus)) continue;
        int32_t row_index = 0;
        for (float z = field.y + 0.78f;
             z < field.y + field.height - 0.55f;
             z += 1.32f, ++row_index) {
            int32_t column_index = 0;
            for (float x = field.x + 0.72f;
                 x < field.x + field.width - 0.55f;
                 x += 1.05f, ++column_index) {
                float jitter = TerrainScatter01(
                    field_index * 31 + column_index,
                    row_index, 41U) - 0.5f;
                float scale = 0.82f + TerrainScatter01(
                    field_index * 37 + column_index,
                    row_index, 42U) * 0.38f;
                float crop_x = x + jitter * 0.22f;
                if (TerrainFieldAmount(crop_x, z) < 0.22f) continue;
                TerrainTuft(crop_x, z,
                             0.31f * scale, 0.070f * scale,
                             jitter * 0.55f, crop_bottom, crop_top);
            }
        }
    }
    rlEnd();
}

static void DrawTerrainRocks(Vector3 focus)
{
    const float spacing = 4.80f;
    int32_t first_column = (int32_t)floorf(
        fmaxf(0.0f, focus.x - 24.0f) / spacing);
    int32_t last_column = (int32_t)ceilf(
        fminf(CC_LOCAL_WORLD_WIDTH, focus.x + 24.0f) / spacing);
    int32_t first_row = (int32_t)floorf(
        fmaxf(0.0f, focus.z - 24.0f) / spacing);
    int32_t last_row = (int32_t)ceilf(
        fminf(CC_LOCAL_WORLD_DEPTH, focus.z + 24.0f) / spacing);
    for (int32_t row = first_row; row <= last_row; ++row) {
        for (int32_t column = first_column; column <= last_column; ++column) {
            float chance = TerrainScatter01(column, row, 51U);
            if (chance < 0.74f) continue;
            float x = ((float)column + 0.18f +
                       TerrainScatter01(column, row, 52U) * 0.64f) * spacing;
            float z = ((float)row + 0.18f +
                       TerrainScatter01(column, row, 53U) * 0.64f) * spacing;
            if (!TerrainPointInPlayableWorld(x, z) ||
                TerrainRoadAmount(x, z) > 0.04f ||
                TerrainFieldAmount(x, z) > 0.04f ||
                TerrainPointInsideMajorFoundation(x, z)) {
                continue;
            }
            Vector3 normal = CcLocalTerrainNormalAt(x, z);
            if (normal.y > 0.94f &&
                TerrainScatter01(column, row, 54U) < 0.82f) {
                continue;
            }
            float radius = 0.16f +
                           TerrainScatter01(column, row, 55U) * 0.20f;
            float height = CcLocalTerrainHeightAt(x, z);
            Color rock = height > 4.0f ? WORLD_STONE_LIGHT : WORLD_STONE;
            DrawTiltedBox((Vector3){x, height + radius * 0.34f, z},
                          (Vector3){radius * 1.55f, radius * 0.72f,
                                    radius * 1.18f},
                          (Vector3){0.0f, 1.0f, 0.0f},
                          TerrainScatter01(column, row, 56U) * 42.0f,
                          rock);
            if (radius > 0.27f) {
                DrawTiltedBox(
                    (Vector3){x + radius * 0.32f,
                              height + radius * 0.62f,
                              z - radius * 0.14f},
                    (Vector3){radius * 0.82f, radius * 0.58f,
                              radius * 0.72f},
                    (Vector3){0.0f, 1.0f, 0.0f},
                    TerrainScatter01(column, row, 57U) * 55.0f,
                    ShadeColor(rock, 1.08f));
            }
        }
    }
}

static void DrawTerrainSurfaceDetails(const CcSettlement *place,
                                      Vector3 focus)
{
    DrawTerrainRoadRuts(place, focus);
    DrawTerrainFieldRows(place, focus);
    DrawTerrainPlantCover(place, focus);
    DrawTerrainRocks(focus);
}

static void DrawTerrainPlacedLantern(float x, float z)
{
    rlPushMatrix();
    rlTranslatef(0.0f, CcLocalTerrainHeightAt(x, z), 0.0f);
    DrawStreetLantern(x, z);
    rlPopMatrix();
}

static void DrawExteriorTerrain(const CcSettlement *place, Vector3 focus)
{
    SetWorldTerrainSurface(true);
    DrawCachedTerrain(place);
    DrawTerrainSurfaceDetails(place, focus);
    SetWorldTerrainSurface(false);
    if (SceneryPointVisible(46.15f, 26.10f, focus)) {
        DrawTerrainPlacedLantern(46.15f, 26.10f);
    }
    if (SceneryPointVisible(53.85f, 26.10f, focus)) {
        DrawTerrainPlacedLantern(53.85f, 26.10f);
    }
}

static Color BuildingWallColor(int32_t style,
                               const CcLocalPlaceProfile *profile)
{
    Color wall;
    switch (style) {
        case 1: wall = WORLD_STONE; break;
        case 2:
            wall = BlendColor(WORLD_EARTH_LIGHT, WORLD_DANGER, 0.14f);
            break;
        case 3:
            wall = BlendColor(WORLD_STONE, WORLD_TEAL, 0.16f);
            break;
        default:
            wall = BlendColor(WORLD_STONE, WORLD_GRASS, 0.24f);
            break;
    }
    if (profile == NULL) return wall;
    switch (profile->function) {
        case CC_SETTLEMENT_FARMING:
            return BlendColor(wall, WORLD_CROP_LIGHT, 0.16f);
        case CC_SETTLEMENT_MINING:
            return BlendColor(wall, WORLD_METAL_SHADOW, 0.22f);
        case CC_SETTLEMENT_FORTRESS:
            return BlendColor(wall, WORLD_STONE_LIGHT, 0.16f);
        case CC_SETTLEMENT_CAPITAL:
            return BlendColor(wall, WORLD_GOLD, 0.10f);
        case CC_SETTLEMENT_DUNGEON_TOWN:
            return BlendColor(wall, WORLD_VIOLET, 0.14f);
        case CC_SETTLEMENT_MARKET: return wall;
    }
    return wall;
}

static Color BuildingRoofColor(int32_t style, Color kingdom,
                               const CcLocalPlaceProfile *profile)
{
    Color roof;
    switch (style) {
        case 1:
            roof = BlendColor(WORLD_ROAD_SHADOW, kingdom, 0.18f);
            break;
        case 2: roof = WORLD_EARTH; break;
        case 3:
            roof = BlendColor(WORLD_METAL_SHADOW, WORLD_VIOLET, 0.22f);
            break;
        default:
            roof = BlendColor(WORLD_FOLIAGE_SHADOW, kingdom, 0.16f);
            break;
    }
    if (profile == NULL) return roof;
    switch (profile->function) {
        case CC_SETTLEMENT_FARMING:
            return BlendColor(roof, WORLD_FOLIAGE, 0.18f);
        case CC_SETTLEMENT_MINING:
            return BlendColor(roof, WORLD_METAL_SHADOW, 0.24f);
        case CC_SETTLEMENT_FORTRESS:
            return BlendColor(roof, WORLD_DANGER, 0.12f);
        case CC_SETTLEMENT_CAPITAL:
            return BlendColor(roof, WORLD_GOLD, 0.12f);
        case CC_SETTLEMENT_DUNGEON_TOWN:
            return BlendColor(roof, WORLD_VIOLET, 0.16f);
        case CC_SETTLEMENT_MARKET: return roof;
    }
    return roof;
}

static uint32_t StreetForegroundBuildingMaskForShot(int32_t shot)
{
    shot = StreetCameraBaseShot(shot);
    /* These are authored stage wings, not player-dependent pop-in. A given
       shot always presents the same architecture; buildings between the low
       camera and its route receive the hero-centered ink reveal. */
    switch (shot) {
        case 1: return UINT32_C(1) << 3;
        case 3:
            return (UINT32_C(1) << 3) | (UINT32_C(1) << 4);
        case 4:
            return (UINT32_C(1) << 3) | (UINT32_C(1) << 4) |
                   (UINT32_C(1) << 6) |
                   (UINT32_C(1) << 9);
        case 6:
            return (UINT32_C(1) << 4) | (UINT32_C(1) << 6);
        case 5:
            return (UINT32_C(1) << 8) | (UINT32_C(1) << 9);
        default: return 0;
    }
}

static uint32_t StreetForegroundBuildingMask(void)
{
    return StreetForegroundBuildingMaskForShot(street_camera_rig.shot);
}

static bool WorldBuildingObscuresReveal(const WorldBuilding *building,
                                        Camera3D camera,
                                        Vector3 reveal_world,
                                        Vector2 reveal_center,
                                        int32_t render_width,
                                        int32_t render_height)
{
    if (building == NULL || render_width <= 0 || render_height <= 0) {
        return false;
    }
    Vector3 camera_forward = Vector3Normalize(
        Vector3Subtract(camera.target, camera.position));
    float hero_depth = Vector3DotProduct(
        Vector3Subtract(reveal_world, camera.position), camera_forward);
    float base = TerrainFootprintHeight(building->footprint);
    BoundingBox visible_walls = {
        .min = {building->footprint.x, base, building->footprint.y},
        .max = {building->footprint.x + building->footprint.width,
                base + building->height,
                building->footprint.y + building->footprint.height},
    };
    /* A side wall or roof bound must not erase a neighboring house. Test the
       center of the hero against the solid wall volume that is actually
       drawn. Whole buildings remain submitted, so this precise trigger does
       not bring back the old distance-culling blink. */
    Ray ray = GetScreenToWorldRayEx(reveal_center, camera,
                                    render_width, render_height);
    RayCollision collision = GetRayCollisionBox(ray, visible_walls);
    if (!collision.hit) return false;
    float collision_depth = Vector3DotProduct(
        Vector3Subtract(collision.point, camera.position), camera_forward);
    return collision_depth + 0.30f < hero_depth;
}

typedef struct WorldBuildingRevealState {
    float amount;
    float occluded_seconds;
    float clear_seconds;
} WorldBuildingRevealState;

static WorldBuildingRevealState world_building_reveals[
    sizeof(WORLD_BUILDINGS) / sizeof(WORLD_BUILDINGS[0])];
static float world_building_reveal_clock = 0.0f;
static bool world_building_reveals_initialized = false;

static void UpdateWorldBuildingReveals(Camera3D camera,
                                       Vector3 reveal_world,
                                       Vector2 reveal_center,
                                       int32_t render_width,
                                       int32_t render_height,
                                       float clock)
{
    bool reset = !world_building_reveals_initialized ||
                 clock < world_building_reveal_clock;
    float delta_time = reset ? 0.0f :
        fmaxf(0.0f, fminf(clock - world_building_reveal_clock, 0.05f));
    world_building_reveal_clock = clock;
    world_building_reveals_initialized = true;

    for (int32_t i = 0; i < (int32_t)(sizeof(WORLD_BUILDINGS) /
                                      sizeof(WORLD_BUILDINGS[0])); ++i) {
        WorldBuildingRevealState *state = &world_building_reveals[i];
        bool occluded = WorldBuildingObscuresReveal(
            &WORLD_BUILDINGS[i], camera, reveal_world, reveal_center,
            render_width, render_height);
        if (reset) {
            state->amount = occluded ? 1.0f : 0.0f;
            state->occluded_seconds = occluded ? 0.06f : 0.0f;
            state->clear_seconds = occluded ? 0.0f : 0.14f;
            continue;
        }
        if (occluded) {
            state->occluded_seconds = fminf(
                0.30f, state->occluded_seconds + delta_time);
            state->clear_seconds = 0.0f;
        } else {
            state->clear_seconds = fminf(
                0.30f, state->clear_seconds + delta_time);
            state->occluded_seconds = 0.0f;
        }
        /* Brief edge crossings do nothing. Once a reveal is established,
           keep it through a short clear interval and ease both directions.
           This avoids one-frame cutaway changes at wall corners. */
        bool held_reveal = occluded ? state->occluded_seconds >= 0.06f :
                           state->clear_seconds < 0.14f &&
                           state->amount > 0.002f;
        float destination = held_reveal ? 1.0f : 0.0f;
        float response = destination > state->amount ? 8.0f : 5.0f;
        float blend = 1.0f - expf(-response * delta_time);
        state->amount += (destination - state->amount) * blend;
        if (fabsf(destination - state->amount) < 0.002f) {
            state->amount = destination;
        }
    }
}

static void DrawWorldBuildings(Color kingdom, Vector3 focus,
                               const CcLocalPlaceProfile *profile,
                               Camera3D camera, Vector3 reveal_world,
                               Vector2 reveal_center,
                               int32_t render_width, int32_t render_height,
                               float clock)
{
    (void)focus;
    float reveal_cut_height = reveal_world.y - 0.30f;
    uint32_t authored_foreground = StreetForegroundBuildingMask();
    UpdateWorldBuildingReveals(camera, reveal_world, reveal_center,
                               render_width, render_height, clock);

    /* Every structure gets an opaque footing pass before any upper-wall
       reveal. This includes the authored market replacement. */
    SetWorldForegroundReveal(0.0f, reveal_cut_height);
    for (int32_t i = 0; i < (int32_t)(sizeof(WORLD_BUILDINGS) /
                                      sizeof(WORLD_BUILDINGS[0])); ++i) {
        const WorldBuilding *building = &WORLD_BUILDINGS[i];
        Color wall = BuildingWallColor(building->style, profile);
        rlPushMatrix();
        rlTranslatef(0.0f, TerrainFootprintHeight(building->footprint), 0.0f);
        DrawBuildingFoundation(
            building->footprint.x, building->footprint.y,
            building->footprint.width, building->footprint.height,
            building->height, wall);
        rlPopMatrix();
    }

    float reveal_active = -1.0f;
    for (int32_t i = 0; i < (int32_t)(sizeof(WORLD_BUILDINGS) /
                                      sizeof(WORLD_BUILDINGS[0])); ++i) {
        const WorldBuilding *building = &WORLD_BUILDINGS[i];
        /* The market footprint remains authoritative for collision, but its
           visible shell comes from the shared Blender library when present.
           A camera-to-hero sightline classifies the complete house at its
           current position. Rear wall, roof, and trim therefore share one
           reveal, while a house behind the hero remains entirely solid. */
        if (i == 2 && runtime_assets[RUNTIME_ASSET_MARKET].ready) continue;
        float reveal = world_building_reveals[i].amount;
        if ((authored_foreground & (UINT32_C(1) << i)) != 0) {
            /* Fixed room compositions have fixed stage wings. Keep their
               hero-centred cutaway armed for the whole shot, instead of
               waiting for a broad roof to fully cross the sightline. */
            reveal = fmaxf(reveal, 0.94f);
        }
        if (fabsf(reveal - reveal_active) > 0.001f) {
            SetWorldForegroundReveal(reveal, reveal_cut_height);
            reveal_active = reveal;
        }
        rlPushMatrix();
        rlTranslatef(0.0f, TerrainFootprintHeight(building->footprint), 0.0f);
        DrawBuilding(building->footprint.x, building->footprint.y,
                     building->footprint.width, building->footprint.height,
                     building->height,
                     BuildingWallColor(building->style, profile),
                     BuildingRoofColor(building->style, kingdom, profile),
                     building->door, building->style);
        rlPopMatrix();
    }
    SetWorldForegroundReveal(0.0f, reveal_cut_height);
}

static bool DrawAuthoredMarket(const CcSettlement *place)
{
    RuntimeAsset *market = &runtime_assets[RUNTIME_ASSET_MARKET];
    if (!market->ready) return false;
    const Vector3 origin = {50.0f, 0.0f, 21.0f};
    const float scale = 1.70f;
    const WorldBuilding *building = &WORLD_BUILDINGS[2];
    rlPushMatrix();
    rlTranslatef(0.0f, TerrainFootprintHeight(building->footprint), 0.0f);
    DrawBuildingContactShadow(building->footprint.x, building->footprint.y,
                              building->footprint.width,
                              building->footprint.height, building->height);
    DrawModelEx(market->model, origin, (Vector3){0.0f, 1.0f, 0.0f},
                0.0f, (Vector3){scale, scale, scale}, WHITE);
    RuntimeAssetId state = RUNTIME_ASSET_COUNT;
    if (place != NULL && place->hunger >= 30) {
        state = RUNTIME_ASSET_SHORTAGE;
    } else if (place != NULL && place->security >= 70) {
        state = RUNTIME_ASSET_ENFORCEMENT;
    } else if (place != NULL && place->prosperity >= 60) {
        state = RUNTIME_ASSET_RECOVERY;
    }
    if (state < RUNTIME_ASSET_COUNT && runtime_assets[state].ready) {
        DrawModelEx(runtime_assets[state].model, origin,
                    (Vector3){0.0f, 1.0f, 0.0f}, 0.0f,
                    (Vector3){scale, scale, scale}, WHITE);
    }
    rlPopMatrix();
    return true;
}

static WorldBuildingRevealState castle_structure_reveals[
    sizeof(CASTLE_STRUCTURES) / sizeof(CASTLE_STRUCTURES[0])];
static float castle_reveal_clock = 0.0f;
static bool castle_reveals_initialized = false;

static void UpdateCastleStructureReveals(Camera3D camera,
                                         Vector3 reveal_world,
                                         Vector2 reveal_center,
                                         int32_t render_width,
                                         int32_t render_height,
                                         float clock)
{
    bool reset = !castle_reveals_initialized || clock < castle_reveal_clock;
    float delta_time = reset ? 0.0f :
        fmaxf(0.0f, fminf(clock - castle_reveal_clock, 0.05f));
    castle_reveal_clock = clock;
    castle_reveals_initialized = true;
    for (int32_t i = 0; i < (int32_t)(sizeof(CASTLE_STRUCTURES) /
                                      sizeof(CASTLE_STRUCTURES[0])); ++i) {
        const WorldStructure *structure = &CASTLE_STRUCTURES[i];
        WorldBuilding proxy = {
            structure->footprint, structure->height, 0, false,
        };
        WorldBuildingRevealState *state = &castle_structure_reveals[i];
        bool occluded = WorldBuildingObscuresReveal(
            &proxy, camera, reveal_world, reveal_center,
            render_width, render_height);
        if (reset) {
            state->amount = occluded ? 1.0f : 0.0f;
            state->occluded_seconds = occluded ? 0.06f : 0.0f;
            state->clear_seconds = occluded ? 0.0f : 0.16f;
            continue;
        }
        if (occluded) {
            state->occluded_seconds = fminf(
                0.32f, state->occluded_seconds + delta_time);
            state->clear_seconds = 0.0f;
        } else {
            state->clear_seconds = fminf(
                0.32f, state->clear_seconds + delta_time);
            state->occluded_seconds = 0.0f;
        }
        bool held = occluded ? state->occluded_seconds >= 0.06f :
                    state->clear_seconds < 0.16f && state->amount > 0.002f;
        float destination = held ? 1.0f : 0.0f;
        float response = destination > state->amount ? 8.0f : 4.6f;
        float blend = 1.0f - expf(-response * delta_time);
        state->amount += (destination - state->amount) * blend;
        if (fabsf(destination - state->amount) < 0.002f) {
            state->amount = destination;
        }
    }
}

static void DrawCastleBattlements(Rectangle footprint, float height,
                                  Color stone)
{
    const float spacing = 1.18f;
    int32_t front_count = (int32_t)fmaxf(1.0f,
        floorf(footprint.width / spacing));
    float front_step = footprint.width / (float)front_count;
    for (int32_t block = 0; block < front_count; ++block) {
        float block_x = footprint.x + front_step * ((float)block + 0.5f);
        DrawBox((Vector3){block_x, height + 0.42f,
                          footprint.y + footprint.height - 0.10f},
                (Vector3){fminf(0.66f, front_step * 0.62f), 0.78f, 0.56f},
                block != front_count - 1 ? stone : ShadeColor(stone, 0.88f));
    }
    int32_t side_count = (int32_t)fmaxf(1.0f,
        floorf(footprint.height / spacing));
    float side_step = footprint.height / (float)side_count;
    for (int32_t block = 0; block < side_count; ++block) {
        float block_z = footprint.y + side_step * ((float)block + 0.5f);
        DrawBox((Vector3){footprint.x + footprint.width - 0.10f,
                          height + 0.42f, block_z},
                (Vector3){0.56f, 0.78f,
                          fminf(0.66f, side_step * 0.62f)},
                ShadeColor(stone, block != side_count - 1 ? 0.94f : 0.84f));
    }
}

static void DrawConstructedCastleStructure(Rectangle footprint,
                                            float height, Color stone,
                                            Color kingdom, int32_t index)
{
    float center_x = footprint.x + footprint.width * 0.5f;
    float center_z = footprint.y + footprint.height * 0.5f;
    bool wall_run = index < 5;
    bool keep = index == 5;
    bool tower = index >= 6;
    Color base = ShadeColor(stone, 0.70f);

    DrawBox((Vector3){center_x, 0.34f, center_z},
            (Vector3){footprint.width + 0.32f, 0.68f,
                      footprint.height + 0.32f}, base);
    DrawBox((Vector3){center_x, height * 0.50f, center_z},
            (Vector3){footprint.width, height, footprint.height}, stone);

    /* Broad courses and buttresses make the fortification feel load-bearing.
       The bands wrap the two visible sides; towers also get corner mass. */
    int32_t courses = wall_run ? 2 : 3;
    for (int32_t course = 1; course <= courses; ++course) {
        float y = height * (float)course / (float)(courses + 1);
        DrawBox((Vector3){center_x, y,
                          footprint.y + footprint.height + 0.055f},
                (Vector3){footprint.width + 0.12f, 0.16f, 0.15f},
                ShadeColor(stone, 0.82f));
        DrawBox((Vector3){footprint.x + footprint.width + 0.055f,
                          y, center_z},
                (Vector3){0.15f, 0.16f, footprint.height + 0.12f},
                ShadeColor(stone, 0.76f));
    }

    if (tower || keep) {
        float pier = tower ? 0.52f : 0.62f;
        for (int32_t corner = 0; corner < 4; ++corner) {
            float pier_x = (corner & 1) == 0 ?
                footprint.x + pier * 0.5f :
                footprint.x + footprint.width - pier * 0.5f;
            float pier_z = (corner & 2) == 0 ?
                footprint.y + pier * 0.5f :
                footprint.y + footprint.height - pier * 0.5f;
            DrawBox((Vector3){pier_x, height * 0.44f, pier_z},
                    (Vector3){pier, height * 0.88f, pier},
                    ShadeColor(stone, corner == 3 ? 0.76f : 0.86f));
            DrawBox((Vector3){pier_x, height * 0.89f, pier_z},
                    (Vector3){pier + 0.16f, 0.18f, pier + 0.16f},
                    keep && corner == 3 ? kingdom : ShadeColor(stone, 0.72f));
        }
    }

    DrawBox((Vector3){center_x, height + 0.10f, center_z},
            (Vector3){footprint.width + 0.30f, 0.22f,
                      footprint.height + 0.30f},
            tower ? BlendColor(ShadeColor(stone, 0.76f), kingdom, 0.18f) :
                    ShadeColor(stone, 0.70f));
    DrawCastleBattlements(footprint, height, stone);
}

static Color CompoundStoneColor(const CcLocalPlaceProfile *profile,
                                Color kingdom, int32_t structure)
{
    Color stone = structure == 5 ? WORLD_STONE_SHADOW : WORLD_STONE;
    if (profile == NULL) return stone;
    switch (profile->function) {
        case CC_SETTLEMENT_FARMING:
            return BlendColor(stone, WORLD_WOOD, 0.24f);
        case CC_SETTLEMENT_MINING:
            return BlendColor(stone, WORLD_METAL_SHADOW, 0.28f);
        case CC_SETTLEMENT_MARKET:
            return BlendColor(stone, WORLD_TEAL, 0.10f);
        case CC_SETTLEMENT_FORTRESS:
            return BlendColor(stone, kingdom, 0.16f);
        case CC_SETTLEMENT_CAPITAL:
            return BlendColor(stone, WORLD_GOLD, 0.14f);
        case CC_SETTLEMENT_DUNGEON_TOWN:
            return BlendColor(stone, WORLD_VIOLET, 0.18f);
    }
    return stone;
}

static void DrawCastle(Color kingdom, const CcLocalPlaceProfile *profile,
                       Vector3 focus, Camera3D camera,
                       Vector3 reveal_world, Vector2 reveal_center,
                       int32_t render_width, int32_t render_height,
                       float clock)
{
    (void)focus;
    Rectangle keep_pad = {65.20f, 8.20f, 26.50f, 24.40f};
    float reveal_cut_height = reveal_world.y - 0.30f;
    UpdateCastleStructureReveals(camera, reveal_world, reveal_center,
                                 render_width, render_height, clock);
    rlPushMatrix();
    rlTranslatef(0.0f, TerrainFootprintHeight(keep_pad), 0.0f);
    SetWorldForegroundReveal(0.0f, reveal_cut_height);
    for (int32_t i = 0; i < (int32_t)(sizeof(CASTLE_STRUCTURES) /
                                      sizeof(CASTLE_STRUCTURES[0])); ++i) {
        const WorldStructure *structure = &CASTLE_STRUCTURES[i];
        Rectangle footprint = structure->footprint;
        Color stone = CompoundStoneColor(profile, kingdom, i);
        DrawBuildingFoundation(
            footprint.x, footprint.y, footprint.width, footprint.height,
            structure->height, stone);
    }
    float reveal_active = -1.0f;
    for (int32_t i = 0; i < (int32_t)(sizeof(CASTLE_STRUCTURES) /
                                      sizeof(CASTLE_STRUCTURES[0])); ++i) {
        const WorldStructure *structure = &CASTLE_STRUCTURES[i];
        Rectangle footprint = structure->footprint;
        Color stone = CompoundStoneColor(profile, kingdom, i);
        float reveal = castle_structure_reveals[i].amount;
        if (fabsf(reveal - reveal_active) > 0.001f) {
            SetWorldForegroundReveal(reveal, reveal_cut_height);
            reveal_active = reveal;
        }
        DrawConstructedCastleStructure(footprint, structure->height, stone,
                                       kingdom, i);
    }
    SetWorldForegroundReveal(0.0f, reveal_cut_height);
    {
        DrawBox((Vector3){78.5f, 1.20f, 22.03f},
                (Vector3){1.35f, 2.40f, 0.06f},
                WORLD_WOOD_SHADOW);
        DrawBox((Vector3){76.30f, 7.65f, 30.84f},
                (Vector3){0.78f, 2.30f, 0.06f}, kingdom);
        DrawBox((Vector3){80.70f, 7.65f, 30.84f},
                (Vector3){0.78f, 2.30f, 0.06f}, kingdom);

        /* The open southern gate is the room's navigational promise. A high
           lintel and paired fire points frame the pass without putting an
           invisible portcullis across the traversable opening. */
        Color gate_stone = CompoundStoneColor(profile, kingdom, 0);
        DrawBox((Vector3){78.50f, 7.15f, 30.84f},
                (Vector3){2.30f, 1.08f, 0.72f}, gate_stone);
        for (int32_t merlon = 0; merlon < 3; ++merlon) {
            DrawBox((Vector3){77.62f + (float)merlon * 0.88f,
                              8.02f, 30.84f},
                    (Vector3){0.42f, 0.70f, 0.76f},
                    ShadeColor(gate_stone, 0.92f));
        }
        for (int32_t side = -1; side <= 1; side += 2) {
            float torch_x = 78.50f + (float)side * 1.32f;
            DrawBox((Vector3){torch_x, 1.82f, 31.20f},
                    (Vector3){0.10f, 0.86f, 0.10f},
                    WORLD_WOOD_SHADOW);
            DrawSmallSphere((Vector3){torch_x, 2.32f, 31.20f},
                            0.18f, WORLD_GOLD);
        }
    }
    rlPopMatrix();
}

static Vector3 Add3(Vector3 a, Vector3 b)
{
    return (Vector3){a.x + b.x, a.y + b.y, a.z + b.z};
}

static Vector3 LocalPoint(Vector3 base, float x, float y, float z, float yaw)
{
    float cosine = cosf(yaw);
    float sine = sinf(yaw);
    return (Vector3){base.x + x * cosine + z * sine,
                     base.y + y,
                     base.z - x * sine + z * cosine};
}

static void DrawOrientedBox(Vector3 base, Vector3 local_center, Vector3 size,
                            float yaw, Color color)
{
    rlPushMatrix();
    rlTranslatef(base.x, base.y, base.z);
    rlRotatef(yaw * RAD2DEG, 0.0f, 1.0f, 0.0f);
    DrawBox(local_center, size, color);
    rlPopMatrix();
}

static void DrawBoneSegment(Vector3 a, Vector3 b, Color color)
{
    DrawCylinderEx(a, b, 0.028f, 0.028f, 7, color);
}

static void DrawBoneJoint(Vector3 point, Color color)
{
    DrawSmallSphere(point, 0.045f, color);
}

static void DrawCharacterEllipsoid(Vector3 center, Vector3 radius, Color color)
{
    DrawModelEx(sphere_models.character, center,
                (Vector3){0.0f, 1.0f, 0.0f}, 0.0f, radius, color);
}

static void SetFaceRenderContext(Camera3D camera, int32_t width,
                                 int32_t height)
{
    face_render_context.camera = camera;
    face_render_context.width = width;
    face_render_context.height = height;
    face_render_context.valid = width > 0 && height > 0;
}

static float FaceProjectedHeight(Vector3 eye_center, Vector3 up,
                                 float half_height)
{
    if (!face_render_context.valid) return 0.0f;
    Vector2 top = GetWorldToScreenEx(
        Vector3Add(eye_center, Vector3Scale(up, half_height)),
        face_render_context.camera, face_render_context.width,
        face_render_context.height);
    Vector2 bottom = GetWorldToScreenEx(
        Vector3Subtract(eye_center, Vector3Scale(up, half_height)),
        face_render_context.camera, face_render_context.width,
        face_render_context.height);
    return Vector2Distance(top, bottom);
}

static float FaceProjectedSpan(Vector3 center, Vector3 axis,
                               float half_span)
{
    if (!face_render_context.valid || half_span <= 0.0f) return 0.0f;
    Vector2 first = GetWorldToScreenEx(
        Vector3Subtract(center, Vector3Scale(axis, half_span)),
        face_render_context.camera, face_render_context.width,
        face_render_context.height);
    Vector2 second = GetWorldToScreenEx(
        Vector3Add(center, Vector3Scale(axis, half_span)),
        face_render_context.camera, face_render_context.width,
        face_render_context.height);
    return Vector2Distance(first, second);
}

CcLocalFaceLodInternal CcLocalFaceLodForProjectedHeightInternal(
    float projected_face_height)
{
    /* The sculpted head and molded hair own the silhouette. At medium range
       keep only marks that survive as separate pixels; close views add the
       beard without laying a second flat skin or hair shell over the model. */
    if (projected_face_height < 4.0f) {
        return CC_LOCAL_FACE_LOD_SILHOUETTE;
    }
    if (projected_face_height < 12.0f) return CC_LOCAL_FACE_LOD_READABLE;
    return CC_LOCAL_FACE_LOD_CLOSE;
}

CcLocalFaceViewInternal CcLocalFaceViewForFrontAmountInternal(
    float front_amount)
{
    if (front_amount >= 0.82f) return CC_LOCAL_FACE_VIEW_FRONT;
    if (front_amount >= 0.28f) return CC_LOCAL_FACE_VIEW_THREE_QUARTER;
    return CC_LOCAL_FACE_VIEW_PROFILE;
}

static void DrawFaceQuad(Vector3 origin, Vector3 right, Vector3 up,
                         Vector3 normal, float x, float y, float width,
                         float height, float lift, Color color)
{
    Vector3 center = Vector3Add(
        origin, Vector3Add(Vector3Scale(right, x), Vector3Scale(up, y)));
    center = Vector3Add(center, Vector3Scale(normal, lift));
    Vector3 across = Vector3Scale(right, width * 0.5f);
    Vector3 rise = Vector3Scale(up, height * 0.5f);
    Vector3 lower_left = Vector3Subtract(Vector3Subtract(center, across), rise);
    Vector3 lower_right = Vector3Add(Vector3Subtract(center, rise), across);
    Vector3 upper_right = Vector3Add(Vector3Add(center, across), rise);
    Vector3 upper_left = Vector3Add(Vector3Subtract(center, across), rise);
    DrawTriangle3D(lower_left, lower_right, upper_right, color);
    DrawTriangle3D(lower_left, upper_right, upper_left, color);
    /* Character-local right appears screen-left in a front view, so the tiny
       plane can reverse its apparent winding. Draw the back winding too. */
    DrawTriangle3D(lower_left, upper_right, lower_right, color);
    DrawTriangle3D(lower_left, upper_left, upper_right, color);
}

typedef struct WorldFaceCanvas {
    Vector3 origin;
    Vector3 right;
    Vector3 up;
    Vector3 normal;
    float cell_width;
    float cell_height;
    float horizontal_scale;
    CcLocalFaceLodInternal lod;
    bool always_paint;
    int32_t layer;
} WorldFaceCanvas;

static bool IsPortraitEyeBlock(int32_t grid_x, int32_t grid_y,
                               int32_t grid_width, int32_t grid_height)
{
    return grid_width == 1 && grid_height == 1 &&
           grid_x >= 6 && grid_x <= 13 &&
           (grid_y == 8 || grid_y == 9);
}

static bool IsPortraitMouthBlock(int32_t grid_x, int32_t grid_y,
                                 int32_t grid_width,
                                 int32_t grid_height)
{
    (void)grid_width;
    (void)grid_height;
    return grid_x >= 7 && grid_x <= 10 &&
           (grid_y >= 13 && grid_y <= 15);
}

static void PaintWorldFaceBlock(void *context, int32_t grid_x,
                                int32_t grid_y, int32_t grid_width,
                                int32_t grid_height, Color color)
{
    WorldFaceCanvas *canvas = context;
    if (!canvas->always_paint &&
        canvas->lod == CC_LOCAL_FACE_LOD_READABLE &&
        !IsPortraitEyeBlock(grid_x, grid_y, grid_width, grid_height) &&
        !IsPortraitMouthBlock(grid_x, grid_y, grid_width, grid_height)) {
        return;
    }

    float grid_center_x = (float)grid_x + (float)grid_width * 0.5f;
    float grid_center_y = (float)grid_y + (float)grid_height * 0.5f;
    /* The world canvas right vector is camera-right, so portrait x can map
       directly without mirroring the fringe or scars. */
    float x = (grid_center_x - 10.0f) * canvas->cell_width;
    float width = (float)grid_width * canvas->cell_width;
    x *= canvas->horizontal_scale;
    width *= canvas->horizontal_scale;
    float y = (8.5f - grid_center_y) * canvas->cell_height;
    float height = (float)grid_height * canvas->cell_height;
    float lift = 0.0030f + (float)canvas->layer * 0.00045f;
    DrawFaceQuad(canvas->origin, canvas->right, canvas->up, canvas->normal,
                 x, y, width, height, lift, color);
    canvas->layer += 1;
}

static void DrawWorldFace(Vector3 eye_center, Vector3 head_right,
                          Vector3 head_up, Vector3 head_forward,
                          float half_width, float half_height,
                          float half_depth, const CcFaceRecipe *face,
                          CcNpcPortraitExpression expression,
                          bool priority_face)
{
    if (!face_render_context.valid || face == NULL) return;
    head_right = PhysicsNormalizeOr(head_right,
                                    (Vector3){1.0f, 0.0f, 0.0f});
    head_up = PhysicsNormalizeOr(head_up,
                                 (Vector3){0.0f, 1.0f, 0.0f});
    head_forward = PhysicsNormalizeOr(head_forward,
                                      (Vector3){0.0f, 0.0f, 1.0f});
    Vector3 to_camera = Vector3Subtract(face_render_context.camera.position,
                                        eye_center);
    to_camera = Vector3Subtract(
        to_camera, Vector3Scale(head_up,
                               Vector3DotProduct(to_camera, head_up)));
    to_camera = PhysicsNormalizeOr(to_camera, head_forward);
    float front_amount = Vector3DotProduct(head_forward, to_camera);
    /* The fixed combat view often catches the hero just beyond profile while
       they face a target. Let the priority face wrap onto that visible cheek;
       ordinary actors still hide their features when genuinely turned away. */
    float hidden_face_cutoff = priority_face ? -0.08f : -0.12f;
    if (front_amount < hidden_face_cutoff) return;

    /* Keep the graphic on the visible head surface but face its tiny pixel
       grid toward the camera. The 3D skull, hair, and hat still carry turn. */
    Vector3 normal = to_camera;
    Vector3 feature_right = PhysicsNormalizeOr(
        PhysicsCross(head_up, normal), head_right);
    float surface_cosine = Vector3DotProduct(normal, head_forward);
    float surface_sine = Vector3DotProduct(normal, head_right);
    float surface_radius = 1.0f / sqrtf(
        (surface_cosine * surface_cosine) / (half_depth * half_depth) +
        (surface_sine * surface_sine) / (half_width * half_width));
    Vector3 surface = Vector3Add(
        eye_center, Vector3Scale(
            normal, surface_radius * (priority_face ? 1.15f : 1.03f)));

    float projected_face_height = FaceProjectedHeight(
        eye_center, head_up, half_height);
    CcLocalFaceLodInternal lod = CcLocalFaceLodForProjectedHeightInternal(
        projected_face_height);
    if (lod == CC_LOCAL_FACE_LOD_SILHOUETTE) return;

    /* Indexed character shaders use vertex color as material metadata. Face
       cards need their literal ink colors, so draw this tiny graphic layer
       with raylib's color shader and let the caller restore scene lighting. */
    EndShaderMode();
    float projected_face_width = FaceProjectedSpan(
        eye_center, feature_right, half_width);
    float horizontal_pixels_per_unit = projected_face_width /
                                       fmaxf(half_width * 2.0f, 0.0001f);
    float vertical_pixels_per_unit = projected_face_height /
                                     fmaxf(half_height * 2.0f, 0.0001f);
    /* The portrait is built from square art pixels. Fit one square cell inside
       both head spans instead of stretching x and y independently. */
    float cell_pixels = fminf(projected_face_width / 10.0f,
                              projected_face_height / 15.0f);
    WorldFaceCanvas canvas = {
        .origin = surface,
        .right = feature_right,
        .up = head_up,
        .normal = normal,
        /* The portrait face is ten cells wide; hair-to-chin is fifteen. */
        .cell_width = cell_pixels /
                      fmaxf(horizontal_pixels_per_unit, 0.0001f),
        .cell_height = cell_pixels /
                       fmaxf(vertical_pixels_per_unit, 0.0001f),
        .horizontal_scale = 1.0f,
        .lod = lod,
        .always_paint = false,
        .layer = 0,
    };
    if (lod == CC_LOCAL_FACE_LOD_CLOSE || priority_face) {
        canvas.always_paint = true;
        CcNpcPaintFaceBeard(face, &canvas, PaintWorldFaceBlock);
    }
    canvas.always_paint = priority_face;
    CcNpcPaintFaceFeatures(face, expression, &canvas,
                           PaintWorldFaceBlock);
    EndShaderMode();
}

static void DrawNpcHead(const CcNpcAppearance *appearance, Vector3 head,
                        float yaw, float scale)
{
    float head_width = 0.175f * appearance->head_width * scale;
    float head_depth = 0.158f * appearance->head_depth * scale;
    DrawCharacterEllipsoid(head,
        (Vector3){head_width, 0.195f * scale, head_depth}, appearance->skin);
    Vector3 forward = {sinf(yaw), 0.0f, cosf(yaw)};
    Vector3 right = {cosf(yaw), 0.0f, -sinf(yaw)};
    CcNpcPortraitExpression expression =
        appearance->role == CC_NPC_ROLE_GUARD ||
        appearance->role == CC_NPC_ROLE_RAIDER ? CC_NPC_PORTRAIT_FOCUSED :
                                                 CC_NPC_PORTRAIT_NEUTRAL;
    CcFaceRecipe face = CcNpcFaceRecipe(appearance);
    DrawWorldFace(PhysicsAdd(head, (Vector3){0.0f, 0.045f * scale, 0.0f}),
                  right, (Vector3){0.0f, 1.0f, 0.0f}, forward,
                  head_width, 0.195f * scale, head_depth, &face, expression,
                  false);
    UseCharacterLighting();

    Vector3 crown = PhysicsAdd(head, (Vector3){0.0f, 0.115f * scale, 0.0f});
    switch (appearance->hair_style) {
        case 0:
            DrawCharacterEllipsoid(crown,
                (Vector3){head_width * 1.02f, 0.105f * scale,
                          head_depth * 1.04f}, appearance->hair);
            break;
        case 1:
            DrawCharacterEllipsoid(crown,
                (Vector3){head_width * 1.04f, 0.125f * scale,
                          head_depth * 1.02f}, appearance->hair);
            DrawCylinderEx(PhysicsAdd(head, PhysicsScale(forward,
                                      -head_depth * 0.80f)),
                           PhysicsAdd(head, (Vector3){0.0f, -0.18f * scale, 0.0f}),
                           0.075f * scale, 0.045f * scale, 7,
                           appearance->hair);
            break;
        case 2:
            DrawCharacterEllipsoid(crown,
                (Vector3){head_width, 0.105f * scale, head_depth},
                appearance->hair);
            DrawCharacterSphere(
                PhysicsAdd(PhysicsAdd(head, (Vector3){0.0f, 0.12f * scale, 0.0f}),
                           PhysicsScale(forward, -head_depth)),
                0.070f * scale, appearance->hair);
            break;
        case 3:
            DrawOrientedBox(crown, (Vector3){0.0f, 0.02f * scale, 0.0f},
                            (Vector3){head_width * 0.72f, 0.16f * scale,
                                      head_depth * 1.65f}, yaw,
                            appearance->hair);
            break;
        case 4:
            DrawCharacterEllipsoid(crown,
                (Vector3){head_width, 0.105f * scale, head_depth},
                appearance->hair);
            for (int32_t side = -1; side <= 1; side += 2) {
                Vector3 braid = PhysicsAdd(head, PhysicsScale(right,
                    (float)side * head_width * 0.78f));
                DrawCylinderEx(braid,
                    PhysicsAdd(braid, (Vector3){0.0f, -0.24f * scale, 0.0f}),
                    0.027f * scale, 0.018f * scale, 6, appearance->hair);
            }
            break;
        case 5:
        default:
            DrawCharacterEllipsoid(
                PhysicsAdd(crown, PhysicsScale(forward, -0.018f * scale)),
                (Vector3){head_width * 0.88f, 0.060f * scale,
                          head_depth * 0.92f}, appearance->hair);
            break;
    }
    if (appearance->beard_style != 0U) {
        Vector3 beard = PhysicsAdd(
            PhysicsAdd(head, PhysicsScale(forward, head_depth * 0.72f)),
            (Vector3){0.0f, -0.105f * scale, 0.0f});
        float length = (0.045f + 0.035f * (float)appearance->beard_style) * scale;
        DrawCharacterEllipsoid(beard,
            (Vector3){head_width * 0.68f, length,
                      head_depth * 0.34f}, appearance->hair);
    }
    if ((appearance->equipment & CC_NPC_EQUIPMENT_HEADWEAR) != 0U) {
        Color headwear = appearance->role == CC_NPC_ROLE_GUARD ?
                         appearance->metal : appearance->outer;
        DrawCylinder(PhysicsAdd(head, (Vector3){0.0f, 0.18f * scale, 0.0f}),
                     0.19f * scale, 0.15f * scale, 0.09f * scale, 10,
                     headwear);
        if (appearance->role != CC_NPC_ROLE_GUARD) {
            DrawCylinder(PhysicsAdd(head,
                         (Vector3){0.0f, 0.145f * scale, 0.0f}),
                         0.24f * scale, 0.24f * scale, 0.025f * scale, 12,
                         ShadeColor(headwear, 0.72f));
        }
    }
}

static void DrawNpcEquipment(const CcNpcAppearance *appearance,
                             Vector3 position, Vector3 hip, Vector3 chest,
                             Vector3 shoulder_l, Vector3 shoulder_r,
                             Vector3 hand_l, float yaw, float scale)
{
    if ((appearance->equipment & CC_NPC_EQUIPMENT_MANTLE) != 0U) {
        DrawOrientedBox(position, (Vector3){0.0f, 1.12f * scale,
                                            -0.16f * scale},
                        (Vector3){0.48f * scale, 0.58f * scale,
                                  0.055f * scale}, yaw,
                        ShadeColor(appearance->outer, 0.68f));
    }
    if ((appearance->equipment & CC_NPC_EQUIPMENT_ARMOR) != 0U) {
        DrawOrientedBox(chest, (Vector3){0.0f, -0.06f * scale,
                                         0.16f * scale},
                        (Vector3){0.36f * scale, 0.31f * scale,
                                  0.055f * scale}, yaw, appearance->metal);
        DrawCharacterSphere(shoulder_l, 0.098f * scale, appearance->metal);
        DrawCharacterSphere(shoulder_r, 0.098f * scale, appearance->metal);
    }
    if ((appearance->equipment & CC_NPC_EQUIPMENT_APRON) != 0U) {
        DrawOrientedBox(hip, (Vector3){0.0f, 0.17f * scale,
                                       0.17f * scale},
                        (Vector3){0.33f * scale, 0.50f * scale,
                                  0.035f * scale}, yaw,
                        appearance->underlayer);
    }
    if ((appearance->equipment & CC_NPC_EQUIPMENT_SATCHEL) != 0U) {
        Vector3 bag = LocalPoint(hip, 0.25f * scale, -0.02f * scale,
                                 0.02f * scale, yaw);
        DrawOrientedBox(bag, (Vector3){0},
                        (Vector3){0.20f * scale, 0.24f * scale,
                                  0.11f * scale}, yaw, appearance->leather);
        DrawCylinderEx(shoulder_l, bag, 0.018f * scale, 0.018f * scale,
                       5, ShadeColor(appearance->leather, 0.82f));
    }
    if ((appearance->equipment & CC_NPC_EQUIPMENT_PACK) != 0U) {
        DrawOrientedBox(chest, (Vector3){0.0f, -0.04f * scale,
                                         -0.20f * scale},
                        (Vector3){0.36f * scale, 0.42f * scale,
                                  0.18f * scale}, yaw,
                        appearance->leather);
    }
    if ((appearance->equipment & CC_NPC_EQUIPMENT_TOOL) != 0U &&
        appearance->role != CC_NPC_ROLE_GUARD &&
        appearance->role != CC_NPC_ROLE_RAIDER) {
        Vector3 tool_end = PhysicsAdd(hand_l,
            (Vector3){0.0f, 0.62f * scale, 0.0f});
        DrawCylinderEx(hand_l, tool_end, 0.022f * scale, 0.016f * scale,
                       6, appearance->leather);
        DrawSmallSphere(tool_end, 0.045f * scale, appearance->metal);
    }
}

static void DrawNpcGarmentCut(const CcNpcAppearance *appearance,
                              Vector3 hip, Vector3 chest,
                              Vector3 shoulder_l, float yaw, float scale)
{
    Color shadow = ShadeColor(appearance->outer, 0.72f);
    switch (appearance->garment_style) {
        case 0:
            /* A short belted coat gives the neutral recipe a shaped hem. */
            DrawOrientedBox(hip, (Vector3){0.0f, -0.10f * scale, 0.0f},
                            (Vector3){0.38f * scale, 0.28f * scale,
                                      0.28f * scale}, yaw, shadow);
            break;
        case 1:
            /* Long split tails retain leg readability at the play camera. */
            for (int32_t side = -1; side <= 1; side += 2) {
                DrawOrientedBox(hip,
                    (Vector3){(float)side * 0.105f * scale,
                              -0.19f * scale, -0.13f * scale},
                    (Vector3){0.17f * scale, 0.42f * scale,
                              0.055f * scale}, yaw, shadow);
            }
            break;
        case 2: {
            /* A broad leather sash reads as travel support, not body mass. */
            Vector3 sash_end = LocalPoint(hip, 0.14f * scale,
                                           0.08f * scale, 0.17f * scale, yaw);
            DrawCylinderEx(shoulder_l, sash_end, 0.025f * scale,
                           0.025f * scale, 5, appearance->leather);
            break;
        }
        case 3:
            /* A sleeveless over-vest breaks the tubular torso silhouette. */
            DrawOrientedBox(chest,
                            (Vector3){0.0f, -0.035f * scale,
                                      0.16f * scale},
                            (Vector3){0.31f * scale, 0.34f * scale,
                                      0.035f * scale}, yaw,
                            ShadeColor(appearance->accent, 0.82f));
            break;
        case 4:
        default:
            /* A layered waist wrap supports laborer and refugee recipes. */
            DrawCylinder(PhysicsAdd(hip,
                         (Vector3){0.0f, -0.03f * scale, 0.0f}),
                         0.22f * scale, 0.205f * scale, 0.15f * scale, 10,
                         shadow);
            break;
    }
}

static bool DrawNpcArchetype3D(Vector3 position, float size_hint, float yaw,
                               float phase, CcTraversalMode mode,
                               CcNpcPortraitExpression expression,
                               const CcNpcAppearance *appearance)
{
    if (appearance == NULL || appearance->role < CC_NPC_ROLE_WAYFARER ||
        appearance->role >= CC_NPC_ROLE_COUNT) {
        return false;
    }
    float pulse = sinf(phase * appearance->gait_cadence_scale);
    int32_t pose = 0;
    if (mode != CC_TRAVERSAL_IDLE) {
        float cycle = fmodf(phase * appearance->gait_cadence_scale /
                                (2.0f * PI),
                            1.0f);
        if (cycle < 0.0f) cycle += 1.0f;
        pose = 1 + (int32_t)floorf(
            cycle * (float)CC_NPC_ARCHETYPE_LOCOMOTION_POSE_COUNT);
        if (pose >= CC_NPC_ARCHETYPE_POSE_COUNT) {
            pose = CC_NPC_ARCHETYPE_POSE_COUNT - 1;
        }
    }
    NpcArchetypeCache *archetype = &npc_archetypes[appearance->role][pose];
    if (!archetype->ready && pose != 0) {
        archetype = &npc_archetypes[appearance->role][0];
    }
    if (!archetype->ready || archetype->model.meshCount != 1 ||
        archetype->model.materialCount < 1) {
        return false;
    }

    float scale = appearance->stature * (0.82f + size_hint * 0.08f);
    float width = 0.96f + (appearance->body_mass - 1.0f) * 0.22f +
                  (appearance->shoulder_scale - 1.0f) * 0.18f;
    width = fmaxf(0.90f, fminf(width, 1.08f));
    float movement = mode == CC_TRAVERSAL_IDLE ? 0.35f : 1.0f;
    position.y += fabsf(pulse) * 0.010f * movement *
                  appearance->bob_scale;
    float presentation_yaw = WrapAngle(yaw + pulse * 0.012f * movement);
    /* Ambient people are stage actors, not navigation arrows. Bias them toward
       the fixed adventure-game camera so a broad face/torso read survives even
       when their simulated travel heading would present a one-pixel profile. */
    const float camera_read_yaw = 0.20f;
    presentation_yaw = WrapAngle(
        presentation_yaw +
        WrapAngle(camera_read_yaw - presentation_yaw) * 0.68f);

    const float silhouette_gain = 1.18f;
    const float contact_grid = 0.125f;
    Vector3 contact_shadow = {
        roundf(position.x / contact_grid) * contact_grid,
        0.006f,
        roundf(position.z / contact_grid) * contact_grid,
    };
    DrawContactShadow(
        contact_shadow, 0.62f * scale * width * silhouette_gain,
        0.43f * scale * width * silhouette_gain, presentation_yaw,
        (Color){2, 7, 10, 98});
    SetNpcPalette(appearance, 0.50f, false, (Vector3){0});
    DrawModelEx(archetype->model, position,
                (Vector3){0.0f, 1.0f, 0.0f},
                presentation_yaw * RAD2DEG,
                (Vector3){scale * width * silhouette_gain, scale,
                          scale * width * silhouette_gain}, WHITE);

    Vector3 head_right = {cosf(presentation_yaw), 0.0f,
                          -sinf(presentation_yaw)};
    Vector3 head_up = {0.0f, 1.0f, 0.0f};
    Vector3 head_forward = {sinf(presentation_yaw), 0.0f,
                            cosf(presentation_yaw)};
    Vector3 head = LocalPoint(position, 0.0f, 1.84f * scale,
                              0.01f * scale, presentation_yaw);
    Matrix identity_head = NpcModuleTransform(
        head, head_right, head_up, head_forward,
        (Vector3){0.36f * appearance->head_width * scale * width *
                      silhouette_gain,
                  0.40f * scale,
                  0.33f * appearance->head_depth * scale * width *
                      silhouette_gain});
    Matrix molded_hair = NpcModuleTransform(
        head, head_right, head_up, head_forward,
        (Vector3){scale * width * silhouette_gain,
                  scale,
                  scale * width * silhouette_gain});
    (void)DrawNpcHairFamily(appearance, molded_hair, identity_head);
    if ((appearance->equipment & CC_NPC_EQUIPMENT_HEADWEAR) != 0U) {
        NpcDynamicModuleId headwear = NpcHeadwearModule(
            appearance->headwear_style);
        Color headwear_color = appearance->headwear_style == 0U ?
            appearance->metal : appearance->outer;
        (void)DrawNpcDynamicModule(headwear, identity_head, headwear_color);
    }
    CcFaceRecipe face = CcNpcFaceRecipe(appearance);
    DrawWorldFace(PhysicsAdd(head, Vector3Scale(head_up, 0.025f * scale)),
                  head_right, head_up, head_forward,
                  0.18f * appearance->head_width * scale * width *
                      silhouette_gain,
                  0.20f * scale,
                  0.165f * appearance->head_depth * scale * width *
                      silhouette_gain,
                  &face, expression, false);
    RestoreWorldLighting();
    if (visual_style.hero_ready) {
        float ink_strength = CC_HERO_INK_STRENGTH;
        SetShaderValue(visual_style.hero,
                       visual_style.hero_ink_strength_location,
                       &ink_strength, SHADER_UNIFORM_FLOAT);
    }
    return true;
}

static void DrawNpcAppearanceFigure3D(
    Vector3 position, float size_hint, float yaw,
    const CcNpcAppearance *identity, float phase, CcTraversalMode mode)
{
    if (identity == NULL) return;
    CcNpcAppearance appearance = *identity;
    uint32_t seed = appearance.seed;
    CcNpcPortraitExpression expression =
        appearance.role == CC_NPC_ROLE_GUARD ||
        appearance.role == CC_NPC_ROLE_RAIDER ? CC_NPC_PORTRAIT_FOCUSED :
                                               CC_NPC_PORTRAIT_NEUTRAL;
    if (!draw_hero_rig_debug && DrawNpcArchetype3D(
            position, size_hint, yaw, phase, mode, expression, &appearance)) {
        return;
    }
    UseCharacterLighting();
    float scale = appearance.stature * (0.90f + size_hint * 0.16f);
    float mass = appearance.body_mass;
    float muscle = appearance.muscularity;
    float active = mode == CC_TRAVERSAL_IDLE ? 0.0f : 1.0f;
    float walk_phase = phase * appearance.gait_cadence_scale * 2.0f * PI;
    float walk_wave = sinf(walk_phase);
    float idle_wave = sinf(phase * 0.55f +
                           (float)(seed & UINT32_C(255)) * 0.011f);
    float stride = walk_wave * 0.13f * scale * active *
                   appearance.stride_scale;
    float arm_stride = stride * appearance.arm_swing_scale;
    float bob = mode == CC_TRAVERSAL_WALK ?
        fabsf(walk_wave) * 0.035f * scale * appearance.bob_scale :
        idle_wave * 0.010f * scale;
    float sway = mode == CC_TRAVERSAL_IDLE ? idle_wave * 0.014f * scale :
                                             cosf(walk_phase) * 0.018f * scale;
    float lean = appearance.idle_lean * scale;
    Vector3 hip = LocalPoint(position, sway * 0.30f,
                             0.73f * scale + bob * 0.35f,
                             lean * 0.20f, yaw);
    Vector3 chest = LocalPoint(position, sway, 1.15f * scale + bob,
                               lean, yaw);
    Vector3 neck = LocalPoint(position, sway * 1.08f,
                              1.43f * scale + bob, lean * 1.24f, yaw);
    Vector3 shoulder_l = LocalPoint(position, -0.225f * scale + sway,
                                    1.29f * scale + bob, lean, yaw);
    Vector3 shoulder_r = LocalPoint(position, 0.225f * scale + sway,
                                    1.29f * scale + bob, lean, yaw);
    Vector3 elbow_l = LocalPoint(position, -0.31f * scale + sway,
                                 1.01f * scale + bob,
                                 lean + arm_stride, yaw);
    Vector3 elbow_r = LocalPoint(position, 0.31f * scale + sway,
                                 1.01f * scale + bob,
                                 lean - arm_stride, yaw);
    Vector3 hand_l = LocalPoint(position, -0.275f * scale + sway,
                                0.78f * scale + bob,
                                lean + arm_stride, yaw);
    Vector3 hand_r = LocalPoint(position, 0.275f * scale + sway,
                                0.78f * scale + bob,
                                lean - arm_stride, yaw);
    Vector3 knee_l = LocalPoint(position, -0.12f * scale, 0.40f * scale,
                                stride, yaw);
    Vector3 knee_r = LocalPoint(position, 0.12f * scale, 0.40f * scale,
                                -stride, yaw);
    Vector3 foot_l = LocalPoint(position, -0.13f * scale, 0.08f,
                                0.06f + stride, yaw);
    Vector3 foot_r = LocalPoint(position, 0.13f * scale, 0.08f,
                                0.06f - stride, yaw);
    if (mode == CC_TRAVERSAL_CLIMB) {
        elbow_l = LocalPoint(position, -0.25f * scale, 1.50f * scale,
                             0.10f, yaw);
        hand_l = LocalPoint(position, -0.18f * scale, 1.70f * scale,
                            0.15f, yaw);
        knee_r = LocalPoint(position, 0.13f * scale, 0.58f * scale,
                            0.18f, yaw);
    } else if (mode == CC_TRAVERSAL_DROP) {
        elbow_l = LocalPoint(position, -0.28f * scale, 1.48f * scale,
                             0.02f, yaw);
        elbow_r = LocalPoint(position, 0.28f * scale, 1.48f * scale,
                             0.02f, yaw);
        hand_l = LocalPoint(position, -0.20f * scale, 1.68f * scale,
                            0.04f, yaw);
        hand_r = LocalPoint(position, 0.20f * scale, 1.68f * scale,
                            0.04f, yaw);
        knee_l = LocalPoint(position, -0.13f * scale, 0.56f * scale,
                            0.17f, yaw);
        knee_r = LocalPoint(position, 0.13f * scale, 0.56f * scale,
                            0.17f, yaw);
        foot_l = LocalPoint(position, -0.14f * scale, 0.30f * scale,
                            0.03f, yaw);
        foot_r = LocalPoint(position, 0.14f * scale, 0.30f * scale,
                            0.03f, yaw);
    }

    DrawContactShadow(
        (Vector3){position.x, position.y + 0.006f, position.z},
        0.62f * scale, 0.43f * scale, yaw, (Color){2, 7, 10, 115});
    float thigh = (0.084f + muscle * 0.024f) * mass * scale;
    DrawCylinderEx(hip, knee_l, thigh, thigh * 0.82f, 9,
                   appearance.trousers);
    DrawCylinderEx(hip, knee_r, thigh, thigh * 0.82f, 9,
                   appearance.trousers);
    DrawCylinderEx(knee_l, foot_l, thigh * 0.82f, thigh * 0.62f, 9,
                   ShadeColor(appearance.trousers, 0.82f));
    DrawCylinderEx(knee_r, foot_r, thigh * 0.82f, thigh * 0.62f, 9,
                   ShadeColor(appearance.trousers, 0.82f));
    DrawOrientedBox(foot_l, (Vector3){0.0f, 0.02f * scale, 0.07f * scale},
                    (Vector3){0.19f * scale, 0.11f * scale, 0.32f * scale},
                    yaw, appearance.leather);
    DrawOrientedBox(foot_r, (Vector3){0.0f, 0.02f * scale, 0.07f * scale},
                    (Vector3){0.19f * scale, 0.11f * scale, 0.32f * scale},
                    yaw, appearance.leather);
    DrawCylinderEx(hip, chest, 0.18f * mass * scale,
                   0.24f * appearance.shoulder_scale * mass * scale, 10,
                   appearance.underlayer);
    Vector3 waist = LocalPoint(position, sway * 0.48f,
                               0.88f * scale + bob * 0.62f,
                               lean * 0.46f, yaw);
    Vector3 upper_chest = LocalPoint(position, sway,
                                     1.29f * scale + bob, lean, yaw);
    DrawCylinderEx(waist, upper_chest, 0.195f * mass * scale,
                   0.25f * appearance.shoulder_scale * mass * scale, 10,
                   appearance.outer);
    DrawCylinder(waist, 0.205f * mass * scale, 0.205f * mass * scale,
                 0.065f * scale, 10, appearance.leather);
    DrawNpcGarmentCut(&appearance, hip, chest, shoulder_l, yaw, scale);
    DrawCylinderEx(shoulder_l, elbow_l, 0.077f * mass * scale,
                   0.063f * mass * scale, 8, appearance.outer);
    DrawCylinderEx(elbow_l, hand_l, 0.063f * mass * scale,
                   0.050f * mass * scale, 8, appearance.underlayer);
    DrawCylinderEx(shoulder_r, elbow_r, 0.077f * mass * scale,
                   0.063f * mass * scale, 8, appearance.outer);
    DrawCylinderEx(elbow_r, hand_r, 0.063f * mass * scale,
                   0.050f * mass * scale, 8, appearance.underlayer);
    DrawSmallSphere(hand_l, 0.060f * scale, appearance.skin);
    DrawSmallSphere(hand_r, 0.060f * scale, appearance.skin);
    Vector3 head = LocalPoint(position, sway * 1.12f,
                              1.62f * scale + bob, lean * 1.34f, yaw);
    DrawNpcHead(&appearance, head, yaw, scale);
    DrawNpcEquipment(&appearance, position, hip, chest,
                     shoulder_l, shoulder_r, hand_l, yaw, scale);

    if (draw_hero_rig_debug) {
        Vector3 offset = LocalPoint((Vector3){0.0f, 0.0f, 0.0f},
                                    0.025f, 0.012f, 0.050f, yaw);
        hip = Add3(hip, offset);
        chest = Add3(chest, offset);
        neck = Add3(neck, offset);
        shoulder_l = Add3(shoulder_l, offset);
        shoulder_r = Add3(shoulder_r, offset);
        elbow_l = Add3(elbow_l, offset);
        elbow_r = Add3(elbow_r, offset);
        hand_l = Add3(hand_l, offset);
        hand_r = Add3(hand_r, offset);
        knee_l = Add3(knee_l, offset);
        knee_r = Add3(knee_r, offset);
        foot_l = Add3(foot_l, offset);
        foot_r = Add3(foot_r, offset);
        DrawBoneSegment(hip, chest, appearance.accent);
        DrawBoneSegment(chest, neck, appearance.accent);
        DrawBoneSegment(shoulder_l, shoulder_r, appearance.accent);
        DrawBoneSegment(shoulder_l, elbow_l, appearance.accent);
        DrawBoneSegment(elbow_l, hand_l, appearance.accent);
        DrawBoneSegment(shoulder_r, elbow_r, appearance.accent);
        DrawBoneSegment(elbow_r, hand_r, appearance.accent);
        DrawBoneSegment(hip, knee_l, appearance.accent);
        DrawBoneSegment(knee_l, foot_l, appearance.accent);
        DrawBoneSegment(hip, knee_r, appearance.accent);
        DrawBoneSegment(knee_r, foot_r, appearance.accent);
        const Vector3 joints[] = {
            hip, chest, neck, shoulder_l, shoulder_r, elbow_l, elbow_r,
            hand_l, hand_r, knee_l, knee_r, foot_l, foot_r
        };
        for (int32_t joint = 0;
             joint < (int32_t)(sizeof(joints) / sizeof(joints[0])); ++joint) {
            DrawBoneJoint(joints[joint], appearance.accent);
        }
    }
    RestoreWorldLighting();
}

static void DrawNpcFigure3D(Vector3 position, float size_hint, float yaw,
                            uint32_t seed, CcNpcRole role, Color accent,
                            float phase, CcTraversalMode mode)
{
    enum { NPC_APPEARANCE_CACHE_CAPACITY = 64 };
    typedef struct NpcAppearanceCacheEntry {
        bool valid;
        uint32_t seed;
        CcNpcRole role;
        Color accent;
        CcNpcAppearance appearance;
    } NpcAppearanceCacheEntry;
    static NpcAppearanceCacheEntry cache[NPC_APPEARANCE_CACHE_CAPACITY];

    uint32_t accent_key = (uint32_t)accent.r |
        ((uint32_t)accent.g << 8U) |
        ((uint32_t)accent.b << 16U) |
        ((uint32_t)accent.a << 24U);
    uint32_t hash = seed ^ accent_key ^
        ((uint32_t)role * UINT32_C(0x9e3779b9));
    hash ^= hash >> 16U;
    NpcAppearanceCacheEntry *entry =
        &cache[hash % NPC_APPEARANCE_CACHE_CAPACITY];
    bool cache_hit = entry->valid && entry->seed == seed &&
        entry->role == role && entry->accent.r == accent.r &&
        entry->accent.g == accent.g && entry->accent.b == accent.b &&
        entry->accent.a == accent.a;
    if (!cache_hit) {
        entry->valid = true;
        entry->seed = seed;
        entry->role = role;
        entry->accent = accent;
        entry->appearance = CcNpcAppearanceGenerate(seed, role, accent);
    }
    DrawNpcAppearanceFigure3D(position, size_hint, yaw, &entry->appearance,
                              phase, mode);
}

static void DrawVisibleNpcFigure3D(Vector3 position, float size_hint,
                                   float yaw, uint32_t seed, CcNpcRole role,
                                   Color accent, float phase,
                                   CcTraversalMode mode, Vector3 focus)
{
    if (!SceneryPointVisible(position.x, position.z, focus)) return;
    DrawNpcFigure3D(position, size_hint, yaw, seed, role, accent, phase, mode);
}

static void DrawPitchedFoot(Vector3 heel, Vector3 toe, float yaw, Color color)
{
    Vector3 center = PhysicsScale(PhysicsAdd(heel, toe), 0.5f);
    Vector3 difference = PhysicsSubtract(toe, heel);
    float horizontal = sqrtf(difference.x * difference.x +
                             difference.z * difference.z);
    Vector3 fallback_forward = {sinf(yaw), 0.0f, cosf(yaw)};
    Vector3 horizontal_forward = horizontal > 0.0001f ?
        (Vector3){difference.x / horizontal, 0.0f,
                  difference.z / horizontal} : fallback_forward;
    horizontal_forward = PhysicsNormalizeOr(
        PhysicsLerp(fallback_forward, horizontal_forward,
                    SmoothStep01(horizontal / 0.08f)),
        fallback_forward);
    float solved_yaw = atan2f(horizontal_forward.x, horizontal_forward.z);
    float pitch = atan2f(difference.y, fmaxf(0.0001f, horizontal));
    center.y += 0.045f;
    rlPushMatrix();
    rlTranslatef(center.x, center.y, center.z);
    rlRotatef(solved_yaw * RAD2DEG, 0.0f, 1.0f, 0.0f);
    rlRotatef(-pitch * RAD2DEG, 1.0f, 0.0f, 0.0f);
    DrawBox((Vector3){0.0f, 0.0f, 0.0f}, (Vector3){0.20f, 0.11f, 0.34f},
            color);
    rlPopMatrix();
}

static Color HumanoidContactColor(CcHumanoidContact contact)
{
    switch (contact) {
        case CC_HUMANOID_CONTACT_HEEL: return (Color){116, 224, 197, 255};
        case CC_HUMANOID_CONTACT_FLAT: return WORLD_GOLD;
        case CC_HUMANOID_CONTACT_TOE: return (Color){245, 151, 66, 255};
        case CC_HUMANOID_CONTACT_SWING: return WORLD_VIOLET;
        case CC_HUMANOID_CONTACT_AIR:
        default: return WORLD_MUTED;
    }
}

static float PoseAxisYaw(Vector3 left, Vector3 right, float fallback)
{
    Vector3 axis = PhysicsSubtract(right, left);
    float horizontal = sqrtf(axis.x * axis.x + axis.z * axis.z);
    return horizontal > 0.0001f ? atan2f(-axis.z, axis.x) : fallback;
}

static void DrawHeroSkinRigOverlay(const CcHumanoidGait *gait,
                                   const CcHumanoidSkinPose *skin,
                                   const CcLocalCapeState *cape)
{
    Vector3 offset = {0.018f, 0.010f, 0.018f};
    for (int32_t bone = 0; bone < CC_HUMANOID_SKIN_BONE_COUNT; ++bone) {
        Color color = Fade(WORLD_GOLD, 0.78f);
        if (bone == CC_HUMANOID_SKIN_ROOT) color = Fade(WORLD_TEAL, 0.85f);
        else if (bone >= CC_HUMANOID_SKIN_THIGH_LEFT &&
                 bone <= CC_HUMANOID_SKIN_FOOT_LEFT) {
            color = Fade(HumanoidContactColor(gait->feet[0].contact), 0.82f);
        } else if (bone >= CC_HUMANOID_SKIN_THIGH_RIGHT) {
            color = Fade(HumanoidContactColor(gait->feet[1].contact), 0.82f);
        }
        Vector3 head = Add3(FromLimbVector(skin->bones[bone].head), offset);
        Vector3 tail = Add3(FromLimbVector(skin->bones[bone].tail), offset);
        DrawBoneSegment(head, tail, color);
        DrawBoneJoint(head, color);
    }
    if (cape != NULL && cape->initialized) {
        for (int32_t point = 0; point < CC_LOCAL_CAPE_POINT_COUNT - 1;
             ++point) {
            Vector3 head = Add3(cape->point[point], offset);
            Vector3 tail = Add3(cape->point[point + 1], offset);
            DrawBoneSegment(head, tail, Fade(WORLD_VIOLET, 0.86f));
            DrawBoneJoint(head, Fade(WORLD_VIOLET, 0.92f));
        }
    }
}

static Vector3 NpcModuleLocalPoint(Vector3 origin, Vector3 right, Vector3 up,
                                   Vector3 forward, Vector3 local)
{
    Vector3 result = PhysicsAdd(origin, PhysicsScale(right, local.x));
    result = PhysicsAdd(result, PhysicsScale(up, local.y));
    return PhysicsAdd(result, PhysicsScale(forward, local.z));
}

static Matrix NpcModuleTransform(Vector3 origin, Vector3 right, Vector3 up,
                                 Vector3 forward, Vector3 scale)
{
    return (Matrix){
        .m0 = right.x * scale.x,
        .m1 = right.y * scale.x,
        .m2 = right.z * scale.x,
        .m3 = 0.0f,
        .m4 = up.x * scale.y,
        .m5 = up.y * scale.y,
        .m6 = up.z * scale.y,
        .m7 = 0.0f,
        .m8 = forward.x * scale.z,
        .m9 = forward.y * scale.z,
        .m10 = forward.z * scale.z,
        .m11 = 0.0f,
        .m12 = origin.x,
        .m13 = origin.y,
        .m14 = origin.z,
        .m15 = 1.0f,
    };
}

/* Large, bone-driven marks survive the final art-pixel grid better than the
   small decorative pieces in the source mesh.  The dark clasp separates cape
   from armor, while the broken gold chevron gives the hero a unique chest
   read even when the face is only a few pixels tall. */
static void DrawWayfarerHeroDetails(const CcHumanoidSkinPose *skin)
{
    if (skin == NULL || !skin->valid) return;
    Vector3 up = PhysicsNormalizeOr(FromLimbVector(skin->body_up),
                                    (Vector3){0.0f, 1.0f, 0.0f});
    Vector3 right = PhysicsNormalizeOr(FromLimbVector(skin->body_right),
                                       (Vector3){1.0f, 0.0f, 0.0f});
    Vector3 forward = PhysicsNormalizeOr(FromLimbVector(skin->body_forward),
                                         (Vector3){0.0f, 0.0f, 1.0f});
    Vector3 chest = FromLimbVector(
        skin->sockets[CC_HUMANOID_SOCKET_CHEST_FRONT].position);
    chest = PhysicsAdd(chest, PhysicsScale(forward, 0.032f));

    Color panel_ink = CC_STYLE_HERO_PANEL_INK;
    Color portrait_burgundy = CC_STYLE_HERO_OUTER;
    Color broken_gold = CC_STYLE_HERO_ACCENT;
    Vector3 panel = PhysicsAdd(chest, PhysicsScale(up, -0.018f));
    /* Match the portrait's large oxblood shoulder-and-chest mass. The broad
       shape survives at distance; the gold emblem is the second read. */
    DrawFaceQuad(panel, right, up, forward, 0.0f, 0.0f,
                 0.350f, 0.270f, 0.006f, panel_ink);
    DrawFaceQuad(panel, right, up, forward, 0.0f, 0.0f,
                 0.306f, 0.226f, 0.011f, portrait_burgundy);

    static const CcHumanoidSkinBone shoulder_bones[] = {
        CC_HUMANOID_SKIN_UPPER_ARM_LEFT,
        CC_HUMANOID_SKIN_UPPER_ARM_RIGHT,
    };
    for (int32_t side = 0; side < 2; ++side) {
        Vector3 shoulder = FromLimbVector(
            skin->bones[shoulder_bones[side]].head);
        Vector3 upper_arm = FromLimbVector(
            skin->bones[shoulder_bones[side]].tail);
        Vector3 tab_end = Vector3Lerp(shoulder, upper_arm, 0.24f);
        shoulder = PhysicsAdd(shoulder, PhysicsScale(forward, 0.014f));
        tab_end = PhysicsAdd(tab_end, PhysicsScale(forward, 0.014f));
        DrawCylinderEx(shoulder, tab_end, 0.108f, 0.084f, 6,
                       portrait_burgundy);
    }

    Vector3 left_top = NpcModuleLocalPoint(
        panel, right, up, forward, (Vector3){-0.112f, 0.076f, 0.022f});
    Vector3 left_low = NpcModuleLocalPoint(
        panel, right, up, forward, (Vector3){-0.020f, -0.030f, 0.022f});
    Vector3 right_low = NpcModuleLocalPoint(
        panel, right, up, forward, (Vector3){0.026f, -0.014f, 0.022f});
    Vector3 right_top = NpcModuleLocalPoint(
        panel, right, up, forward, (Vector3){0.116f, 0.064f, 0.022f});
    DrawCylinderEx(left_top, left_low, 0.020f, 0.016f, 5, broken_gold);
    DrawCylinderEx(right_low, right_top, 0.016f, 0.020f, 5, broken_gold);
}

static bool DrawNpcDynamicModule(NpcDynamicModuleId id, Matrix transform,
                                 Color color)
{
    (void)color;
    if (id < 0 || id >= NPC_DYNAMIC_MODULE_COUNT) return false;
    NpcDynamicModuleCache *module = &npc_dynamic_modules[id];
    if (!module->ready || module->model.meshCount != 1 ||
        module->model.materialCount < 1) return false;
    for (int32_t mesh = 0; mesh < module->model.meshCount; ++mesh) {
        int32_t material = module->model.meshMaterial[mesh];
        if (material < 0 || material >= module->model.materialCount) {
            material = 0;
        }
        DrawMesh(module->model.meshes[mesh],
                 module->model.materials[material], transform);
    }
    return true;
}

static int32_t NpcHeadFamilyForAppearance(
    const CcNpcAppearance *appearance)
{
    if (appearance == NULL ||
        appearance->head_family >= CC_NPC_HEAD_FAMILY_COUNT) {
        return CC_NPC_HEAD_FAMILY_SQUARE;
    }
    return appearance->head_family;
}

static bool DrawNpcHeadFamily(const CcNpcAppearance *appearance,
                              Matrix transform, Matrix fallback_transform)
{
    int32_t family = NpcHeadFamilyForAppearance(appearance);
    NpcHeadFamilyCache *head = &npc_head_families[family];
    if (!head->ready || head->model.materialCount < 1) {
        return DrawNpcDynamicModule(NPC_DYNAMIC_HEAD, fallback_transform,
                                    appearance != NULL ? appearance->skin :
                                                         WHITE);
    }
    for (int32_t mesh = 0; mesh < head->model.meshCount; ++mesh) {
        int32_t material = head->model.meshMaterial[mesh];
        if (material < 0 || material >= head->model.materialCount) {
            material = 0;
        }
        DrawMesh(head->model.meshes[mesh], head->model.materials[material],
                 transform);
    }
    return true;
}

static int32_t NpcHairFamilyForAppearance(
    const CcNpcAppearance *appearance)
{
    if (appearance == NULL ||
        appearance->hair_family >= CC_NPC_HAIR_FAMILY_COUNT) {
        return CC_NPC_HAIR_FAMILY_CROPPED;
    }
    return appearance->hair_family;
}

static bool DrawNpcHairFamily(const CcNpcAppearance *appearance,
                              Matrix transform, Matrix fallback_transform)
{
    int32_t family = NpcHairFamilyForAppearance(appearance);
    NpcHairFamilyCache *hair = &npc_hair_families[family];
    if (!hair->ready || hair->model.materialCount < 1) {
        int32_t fallback = appearance != NULL ?
            (int32_t)appearance->hair_style % CC_NPC_DYNAMIC_HAIR_COUNT : 0;
        if (fallback < 0) fallback += CC_NPC_DYNAMIC_HAIR_COUNT;
        return DrawNpcDynamicModule(
            (NpcDynamicModuleId)(NPC_DYNAMIC_HAIR_0 + fallback),
            fallback_transform,
            appearance != NULL ? appearance->hair : WHITE);
    }
    for (int32_t mesh = 0; mesh < hair->model.meshCount; ++mesh) {
        int32_t material = hair->model.meshMaterial[mesh];
        if (material < 0 || material >= hair->model.materialCount) {
            material = 0;
        }
        DrawMesh(hair->model.meshes[mesh], hair->model.materials[material],
                 transform);
    }
    return true;
}

static bool DrawNpcDynamicBoneModule(NpcDynamicModuleId id,
                                     const CcHumanoidSkinBonePose *bone,
                                     float width, float depth, Color color)
{
    if (bone == NULL) return false;
    Vector3 head = FromLimbVector(bone->head);
    Vector3 tail = FromLimbVector(bone->tail);
    float length = PhysicsLength(PhysicsSubtract(tail, head));
    if (!isfinite(length) || length <= 0.001f) return false;
    return DrawNpcDynamicModule(
        id, NpcModuleTransform(head, FromLimbVector(bone->right),
                               FromLimbVector(bone->up),
                               FromLimbVector(bone->forward),
                               (Vector3){width, length, depth}), color);
}

static bool NpcDynamicCoreReady(const CcNpcAppearance *appearance)
{
    static const NpcDynamicModuleId core[] = {
        NPC_DYNAMIC_TORSO, NPC_DYNAMIC_PELVIS, NPC_DYNAMIC_UPPER_ARM,
        NPC_DYNAMIC_FOREARM, NPC_DYNAMIC_THIGH, NPC_DYNAMIC_SHIN,
        NPC_DYNAMIC_HAND, NPC_DYNAMIC_FOOT,
    };
    for (int32_t i = 0;
        i < (int32_t)(sizeof(core) / sizeof(core[0])); ++i) {
        if (!npc_dynamic_modules[core[i]].ready) return false;
    }
    int32_t family = NpcHeadFamilyForAppearance(appearance);
    if (!npc_head_families[family].ready &&
        !npc_dynamic_modules[NPC_DYNAMIC_HEAD].ready) return false;
    int32_t hair_family = NpcHairFamilyForAppearance(appearance);
    if (npc_hair_families[hair_family].ready) return true;
    int32_t hair = (int32_t)appearance->hair_style %
                   CC_NPC_DYNAMIC_HAIR_COUNT;
    if (hair < 0) hair += CC_NPC_DYNAMIC_HAIR_COUNT;
    return npc_dynamic_modules[NPC_DYNAMIC_HAIR_0 + hair].ready;
}

static bool DrawDynamicNpcModules(const CcLocalAgent *agent,
                                  const CcHumanoidSkinPose *skin,
                                  const CcNpcAppearance *appearance)
{
    if (agent == NULL || skin == NULL || appearance == NULL || !skin->valid ||
        !NpcDynamicCoreReady(appearance)) return false;

    Vector3 body_right = FromLimbVector(skin->body_right);
    Vector3 body_up = FromLimbVector(skin->body_up);
    Vector3 body_forward = FromLimbVector(skin->body_forward);
    float movement_weight = agent->humanoid.ragdoll.active ? 0.0f :
        SmoothStep01(fabsf(agent->humanoid.speed.value) / 0.90f);
    static const float stepped_wave[8] = {
        0.0f, 0.70f, 1.0f, 0.70f, 0.0f, -0.70f, -1.0f, -0.70f,
    };
    float secondary_wave = 0.0f;
    if (agent->stepped_pose.initialized &&
        agent->stepped_pose.locomotion_bin >= 0 &&
        agent->stepped_pose.locomotion_bin < 8) {
        secondary_wave = stepped_wave[agent->stepped_pose.locomotion_bin];
    }
    Vector3 gear_up = PhysicsNormalizeOr(
        PhysicsAdd(body_up,
                   PhysicsAdd(PhysicsScale(body_right,
                                           secondary_wave * 0.075f *
                                               movement_weight),
                              PhysicsScale(body_forward,
                                           0.045f * movement_weight))),
        body_up);
    Vector3 gear_right = PhysicsNormalizeOr(
        PhysicsCross(gear_up, body_forward), body_right);
    Vector3 gear_forward = PhysicsNormalizeOr(
        PhysicsCross(gear_right, gear_up), body_forward);
    float mass = appearance->body_mass;
    float muscle = appearance->muscularity;
    bool featured_hero = agent->crowned;
    Color outer = appearance->outer;
    Color trousers = appearance->trousers;
    if (CombatIsDefeated(&agent->combat)) {
        float defeat_drain = featured_hero ? 0.22f : 0.72f;
        outer = BlendColor(outer, (Color){50, 49, 52, 255}, defeat_drain);
        trousers = BlendColor(trousers, (Color){42, 42, 44, 255},
                               defeat_drain);
    } else if (agent->combat.hit_flash_seconds > 0.0f) {
        outer = BlendColor(outer, WORLD_INK, 0.72f);
    }
    CcNpcAppearance palette_appearance = *appearance;
    palette_appearance.outer = outer;
    palette_appearance.trousers = trousers;
    Vector3 presentation_head = FromLimbVector(
        skin->sockets[CC_HUMANOID_SOCKET_HEAD].position);
    SetNpcPalette(&palette_appearance, featured_hero ? 0.56f : 0.68f,
                  featured_hero, presentation_head);

    /* The skeleton, muscle controls, and soft tissue are construction data.
       The visible body is one baked skin; the rigid pieces below are fitted
       clothing, boots, head identity, hair, armor, and equipment. */
    bool drew = DrawNpcBodySkin(skin, appearance);
    if (!drew) return false;

    const CcHumanoidSkinBonePose *spine =
        &skin->bones[CC_HUMANOID_SKIN_SPINE];
    Vector3 torso_base = FromLimbVector(spine->head);
    Vector3 torso_top = FromLimbVector(
        skin->bones[CC_HUMANOID_SKIN_CHEST].tail);
    float torso_length = PhysicsLength(
        PhysicsSubtract(torso_top, torso_base));
    drew = DrawNpcDynamicModule(
        NPC_DYNAMIC_TORSO,
        NpcModuleTransform(torso_base, body_right, body_up, body_forward,
                           (Vector3){(featured_hero ? 0.52f : 0.62f) * mass *
                                         appearance->shoulder_scale,
                                     torso_length,
                                     (featured_hero ? 0.58f : 0.55f) * mass}),
        outer);

    const CcHumanoidSkinBonePose *pelvis =
        &skin->bones[CC_HUMANOID_SKIN_PELVIS];
    float pelvis_length = PhysicsLength(PhysicsSubtract(
        FromLimbVector(pelvis->tail), FromLimbVector(pelvis->head)));
    pelvis_length = fmaxf(pelvis_length, 0.16f);
    drew = DrawNpcDynamicModule(
        NPC_DYNAMIC_PELVIS,
        NpcModuleTransform(FromLimbVector(pelvis->head),
                           FromLimbVector(pelvis->right),
                           FromLimbVector(pelvis->up),
                           FromLimbVector(pelvis->forward),
                           (Vector3){(featured_hero ? 0.48f : 0.55f) * mass,
                                     pelvis_length,
                                     (featured_hero ? 0.44f : 0.54f) * mass}),
        trousers) && drew;

    static const CcHumanoidSkinBone upper_arms[] = {
        CC_HUMANOID_SKIN_UPPER_ARM_LEFT,
        CC_HUMANOID_SKIN_UPPER_ARM_RIGHT,
    };
    static const CcHumanoidSkinBone forearms[] = {
        CC_HUMANOID_SKIN_FOREARM_LEFT,
        CC_HUMANOID_SKIN_FOREARM_RIGHT,
    };
    static const CcHumanoidSkinBone thighs[] = {
        CC_HUMANOID_SKIN_THIGH_LEFT,
        CC_HUMANOID_SKIN_THIGH_RIGHT,
    };
    static const CcHumanoidSkinBone shins[] = {
        CC_HUMANOID_SKIN_SHIN_LEFT,
        CC_HUMANOID_SKIN_SHIN_RIGHT,
    };
    static const CcHumanoidSkinBone feet[] = {
        CC_HUMANOID_SKIN_FOOT_LEFT,
        CC_HUMANOID_SKIN_FOOT_RIGHT,
    };
    float arm_width = featured_hero ? 0.165f * mass :
        (0.135f + muscle * 0.020f) * mass;
    float leg_width = featured_hero ? 0.215f * mass :
        (0.175f + muscle * 0.025f) * mass;
    for (int32_t side = 0; side < 2; ++side) {
        drew = DrawNpcDynamicBoneModule(
            NPC_DYNAMIC_UPPER_ARM, &skin->bones[upper_arms[side]],
            featured_hero ? 0.158f * mass : arm_width,
            featured_hero ? 0.170f * mass : arm_width * 0.92f,
            outer) && drew;
        drew = DrawNpcDynamicBoneModule(
            NPC_DYNAMIC_FOREARM, &skin->bones[forearms[side]],
            featured_hero ? 0.152f * mass : arm_width * 0.86f,
            featured_hero ? 0.165f * mass : arm_width * 0.80f,
            appearance->underlayer) && drew;
        drew = DrawNpcDynamicBoneModule(
            NPC_DYNAMIC_THIGH, &skin->bones[thighs[side]],
            leg_width, leg_width * (featured_hero ? 1.00f : 0.94f),
            trousers) && drew;
        drew = DrawNpcDynamicBoneModule(
            NPC_DYNAMIC_SHIN, &skin->bones[shins[side]],
            leg_width * 0.86f, leg_width * 0.82f,
            ShadeColor(trousers, 0.84f)) && drew;
        const CcHumanoidSkinBonePose *foot = &skin->bones[feet[side]];
        Vector3 foot_head = FromLimbVector(foot->head);
        float foot_length = PhysicsLength(PhysicsSubtract(
            FromLimbVector(foot->tail), foot_head));
        drew = DrawNpcDynamicModule(
            NPC_DYNAMIC_FOOT,
            NpcModuleTransform(foot_head, FromLimbVector(foot->right),
                               FromLimbVector(foot->up),
                               FromLimbVector(foot->forward),
                               (Vector3){(featured_hero ? 0.180f : 0.23f) *
                                             mass,
                                         foot_length,
                                         (featured_hero ? 0.16f : 0.20f) *
                                             mass}),
            appearance->leather) && drew;
    }

    const CcHumanoidSkinBonePose *head_bone =
        &skin->bones[CC_HUMANOID_SKIN_HEAD];
    Vector3 head = FromLimbVector(
        skin->sockets[CC_HUMANOID_SOCKET_HEAD].position);
    Vector3 head_right = FromLimbVector(head_bone->right);
    Vector3 head_up = FromLimbVector(head_bone->up);
    Vector3 head_forward = FromLimbVector(head_bone->forward);
    float hero_head_scale = featured_hero ? 1.07f : 1.0f;
    Vector3 head_scale = {appearance->head_width * hero_head_scale,
                          hero_head_scale,
                          appearance->head_depth * hero_head_scale};
    Matrix head_transform = NpcModuleTransform(
        head, head_right, head_up, head_forward, head_scale);
    Matrix fallback_head_transform = NpcModuleTransform(
        head, head_right, head_up, head_forward,
        (Vector3){0.30f * appearance->head_width * hero_head_scale,
                  0.34f * hero_head_scale,
                  0.28f * appearance->head_depth * hero_head_scale});
    drew = DrawNpcHeadFamily(appearance, head_transform,
                             fallback_head_transform) && drew;
    Matrix hair_transform = NpcModuleTransform(
        head, head_right, head_up, head_forward, head_scale);
    Matrix fallback_hair_transform = NpcModuleTransform(
        head, head_right, head_up, head_forward,
        (Vector3){0.25f * appearance->head_width * hero_head_scale,
                  0.29f * hero_head_scale,
                  0.33f * appearance->head_depth * hero_head_scale});
    drew = DrawNpcHairFamily(appearance, hair_transform,
                             fallback_hair_transform) && drew;

    Vector3 back = FromLimbVector(
        skin->sockets[CC_HUMANOID_SOCKET_BACK].position);
    Vector3 chest_front = FromLimbVector(
        skin->sockets[CC_HUMANOID_SOCKET_CHEST_FRONT].position);
    if ((appearance->equipment & CC_NPC_EQUIPMENT_MANTLE) != 0U &&
        npc_dynamic_modules[NPC_DYNAMIC_MANTLE].ready) {
        Vector3 mantle_root = NpcModuleLocalPoint(
            back, body_right, body_up, body_forward,
            (Vector3){-0.03f + secondary_wave * 0.020f * movement_weight,
                      0.14f + fabsf(secondary_wave) * 0.010f *
                                  movement_weight,
                      -0.015f - 0.018f * movement_weight});
        (void)DrawNpcDynamicModule(
            NPC_DYNAMIC_MANTLE,
            NpcModuleTransform(mantle_root, gear_right, gear_up,
                               gear_forward,
                               (Vector3){(featured_hero ? 0.47f : 0.62f) *
                                             mass,
                                         featured_hero ? 0.55f : 0.64f,
                                         featured_hero ? 0.40f : 0.50f}),
            ShadeColor(appearance->outer, 0.68f));
    }
    if ((appearance->equipment & CC_NPC_EQUIPMENT_ARMOR) != 0U) {
        (void)DrawNpcDynamicModule(
            NPC_DYNAMIC_CHEST_PLATE,
            NpcModuleTransform(chest_front, body_right, body_up,
                               body_forward,
                               (Vector3){(featured_hero ? 0.28f : 0.43f) *
                                             mass,
                                         featured_hero ? 0.23f : 0.36f,
                                         featured_hero ? 0.17f : 0.28f}),
            appearance->metal);
        for (int32_t side = 0; side < 2; ++side) {
            const CcHumanoidSkinBonePose *shoulder =
                &skin->bones[upper_arms[side]];
            (void)DrawNpcDynamicModule(
                NPC_DYNAMIC_PAULDRON,
                NpcModuleTransform(FromLimbVector(shoulder->head),
                                   FromLimbVector(shoulder->right),
                                   FromLimbVector(shoulder->up),
                                   FromLimbVector(shoulder->forward),
                                   (Vector3){(featured_hero ? 0.12f : 0.22f) *
                                                 mass,
                                             featured_hero ? 0.13f : 0.20f,
                                             (featured_hero ? 0.12f : 0.21f) *
                                                 mass}),
                appearance->metal);
        }
    }

    Vector3 pelvis_center = FromLimbVector(
        skin->sockets[CC_HUMANOID_SOCKET_BELT].position);
    if (appearance->garment_style == 1U &&
        npc_dynamic_modules[NPC_DYNAMIC_COAT_TAIL].ready) {
        for (int32_t side = -1; side <= 1; side += 2) {
            Vector3 tail = NpcModuleLocalPoint(
                pelvis_center, body_right, body_up, body_forward,
                (Vector3){(float)side * 0.11f, -0.08f, -0.09f});
            (void)DrawNpcDynamicModule(
                NPC_DYNAMIC_COAT_TAIL,
                NpcModuleTransform(tail, body_right, body_up, body_forward,
                                   (Vector3){0.22f * mass, 0.40f, 0.28f}),
                ShadeColor(appearance->outer, 0.72f));
        }
    }
    if ((appearance->equipment & CC_NPC_EQUIPMENT_APRON) != 0U &&
        npc_dynamic_modules[NPC_DYNAMIC_APRON].ready) {
        Vector3 apron = NpcModuleLocalPoint(
            pelvis_center, body_right, body_up, body_forward,
            (Vector3){0.0f, -0.04f, 0.18f});
        (void)DrawNpcDynamicModule(
            NPC_DYNAMIC_APRON,
            NpcModuleTransform(apron, body_right, body_up, body_forward,
                               (Vector3){0.42f * mass, 0.62f, 0.28f}),
            appearance->underlayer);
    }
    if ((appearance->equipment & CC_NPC_EQUIPMENT_PACK) != 0U &&
        npc_dynamic_modules[NPC_DYNAMIC_PACK].ready) {
        Vector3 pack = NpcModuleLocalPoint(
            back, body_right, body_up, body_forward,
            (Vector3){secondary_wave * 0.016f * movement_weight,
                      -0.16f + fabsf(secondary_wave) * 0.012f *
                                   movement_weight,
                      -0.13f - 0.012f * movement_weight});
        (void)DrawNpcDynamicModule(
            NPC_DYNAMIC_PACK,
            NpcModuleTransform(pack, gear_right, gear_up, gear_forward,
                               (Vector3){0.42f * mass, 0.48f, 0.30f}),
            appearance->leather);
    }
    if ((appearance->equipment & CC_NPC_EQUIPMENT_SATCHEL) != 0U &&
        npc_dynamic_modules[NPC_DYNAMIC_SATCHEL].ready) {
        Vector3 satchel = NpcModuleLocalPoint(
            pelvis_center, body_right, body_up, body_forward,
            (Vector3){0.29f * mass + secondary_wave * 0.026f *
                                         movement_weight,
                      -0.13f - fabsf(secondary_wave) * 0.010f *
                                   movement_weight,
                      -0.08f});
        (void)DrawNpcDynamicModule(
            NPC_DYNAMIC_SATCHEL,
            NpcModuleTransform(satchel, gear_right, gear_up, gear_forward,
                               (Vector3){0.24f, 0.28f, 0.20f}),
            appearance->leather);
    }
    if ((appearance->equipment & CC_NPC_EQUIPMENT_TOOL) != 0U &&
        appearance->role != CC_NPC_ROLE_GUARD &&
        appearance->role != CC_NPC_ROLE_RAIDER &&
        npc_dynamic_modules[NPC_DYNAMIC_TOOL_SHAFT].ready &&
        npc_dynamic_modules[NPC_DYNAMIC_TOOL_HEAD].ready) {
        Vector3 tool_root = FromLimbVector(
            skin->bones[CC_HUMANOID_SKIN_HAND_LEFT].tail);
        Vector3 tool_axis = PhysicsNormalizeOr(
            PhysicsAdd(body_up,
                       PhysicsAdd(PhysicsScale(
                                      body_right,
                                      secondary_wave * 0.12f *
                                          movement_weight),
                                  PhysicsScale(body_forward, 0.12f))),
            body_up);
        Vector3 tool_right = PhysicsNormalizeOr(
            PhysicsCross(tool_axis, body_forward), body_right);
        Vector3 tool_forward = PhysicsNormalizeOr(
            PhysicsCross(tool_right, tool_axis), body_forward);
        const float tool_length = 0.68f;
        (void)DrawNpcDynamicModule(
            NPC_DYNAMIC_TOOL_SHAFT,
            NpcModuleTransform(tool_root, tool_right, tool_axis,
                               tool_forward,
                               (Vector3){0.042f, tool_length, 0.042f}),
            appearance->leather);
        Vector3 tool_tip = PhysicsAdd(
            tool_root, PhysicsScale(tool_axis, tool_length));
        (void)DrawNpcDynamicModule(
            NPC_DYNAMIC_TOOL_HEAD,
            NpcModuleTransform(tool_tip, tool_right, tool_axis,
                               tool_forward,
                               (Vector3){0.28f, 0.14f, 0.15f}),
            appearance->metal);
    }
    if ((appearance->equipment & CC_NPC_EQUIPMENT_HEADWEAR) != 0U) {
        NpcDynamicModuleId headwear = NpcHeadwearModule(
            appearance->headwear_style);
        Color headwear_color = appearance->headwear_style == 0U ?
            appearance->metal : appearance->outer;
        Matrix headwear_transform = NpcModuleTransform(
            head, head_right, head_up, head_forward,
            (Vector3){0.26f * appearance->head_width, 0.30f,
                      0.25f * appearance->head_depth});
        (void)DrawNpcDynamicModule(
            headwear, headwear_transform, headwear_color);
    }

    CcNpcPortraitExpression expression =
        appearance->role == CC_NPC_ROLE_GUARD ||
        appearance->role == CC_NPC_ROLE_RAIDER ? CC_NPC_PORTRAIT_FOCUSED :
                                                CC_NPC_PORTRAIT_NEUTRAL;
    if (agent->combat.hit_flash_seconds > 0.0f) {
        expression = CC_NPC_PORTRAIT_HURT;
    } else if (agent->combat.focus_valid ||
               agent->humanoid.action == CC_HUMANOID_ACTION_GUARD ||
               agent->humanoid.action == CC_HUMANOID_ACTION_STRIKE) {
        expression = CC_NPC_PORTRAIT_FOCUSED;
    }
    /* The projected marks and molded head both read the same identity recipe.
       Only held expression may change between the world and the portrait. */
    CcFaceRecipe face = CcNpcFaceRecipe(appearance);
    float face_half_width = featured_hero ?
        0.138f * appearance->head_width :
        0.105f * appearance->head_width;
    float face_half_height = featured_hero ? 0.145f : 0.158f;
    float face_half_depth = featured_hero ?
        0.142f * appearance->head_depth :
        0.125f * appearance->head_depth;
    DrawWorldFace(PhysicsAdd(head, Vector3Scale(head_up, 0.025f)),
                  head_right, head_up, head_forward,
                  face_half_width, face_half_height, face_half_depth,
                  &face, expression, featured_hero);
    return drew;
}

static CcNpcAppearance ProceduralHeroAppearance(const CcLocalAgent *agent)
{
    return agent != NULL ? agent->appearance : CcNpcCrownlessAppearance();
}

static void DrawGroundBrushStroke(Vector3 center, Vector3 along,
                                  float length, float width, Color color);

static void DrawBiomechanicalBiped(const CcLocalAgent *agent)
{
    const CcHumanoidGait *gait = &agent->humanoid;
    const CcHumanoidPose *pose = AgentRenderPose(agent);
    CcHumanoidSkinPose skin;
    CcHumanoidSkinPoseResolve(pose, &skin);
    if (!skin.valid) return;
    bool modular_hero = agent->crowned;
    if (agent->crowned) {
        DrawGroundBrushStroke(
            (Vector3){agent->position.x, agent->position.y + 0.020f,
                      agent->position.z},
            (Vector3){sinf(agent->facing_yaw), 0.0f,
                      cosf(agent->facing_yaw)},
            0.72f, 0.085f, Fade(WORLD_TEAL, 0.82f));
    }
    CcNpcAppearance procedural_hero = ProceduralHeroAppearance(agent);
    if (modular_hero && screen_first_hero_active) {
        UseCharacterLighting();
        bool procedural_hero_updated = DrawDynamicNpcModules(
            agent, &skin, &procedural_hero);
        if (procedural_hero_updated) {
            DrawWayfarerHeroDetails(&skin);
        }
        RestoreWorldLighting();
        if (procedural_hero_updated) {
            CcLocalRendererRecordBiped(true);
            return;
        }
    }
    bool hero_skin_updated = modular_hero &&
        DrawHeroSkin(&skin, &agent->render_cape, WHITE, true, true);
    if (hero_skin_updated) {
        CcLocalRendererRecordBiped(true);
        if (draw_hero_rig_debug) {
            DrawHeroSkinRigOverlay(gait, &skin, &agent->render_cape);
        }
        UseCharacterLighting();
        DrawWayfarerHeroDetails(&skin);
        RestoreWorldLighting();
        /* The high-detail hero asset already owns its eyes, brows, nose,
           mouth, hair, and broken crown as head-weighted 3D geometry. Never
           layer procedural marks or a second crown over that authored face. */
        return;
    } else {
        CcLocalRendererRecordBiped(false);
    }
    UseCharacterLighting();
    if (!agent->crowned &&
        DrawDynamicNpcModules(agent, &skin, &agent->appearance)) {
        RestoreWorldLighting();
        return;
    }
    Vector3 pelvis = FromLimbVector(
        skin.bones[CC_HUMANOID_SKIN_PELVIS].head);
    Vector3 spine = FromLimbVector(
        skin.bones[CC_HUMANOID_SKIN_SPINE].head);
    Vector3 chest = FromLimbVector(
        skin.bones[CC_HUMANOID_SKIN_CHEST].head);
    Vector3 neck = FromLimbVector(
        skin.bones[CC_HUMANOID_SKIN_NECK].head);
    Vector3 head = FromLimbVector(
        skin.sockets[CC_HUMANOID_SOCKET_HEAD].position);
    float fallen_weight = fmaxf(0.0f, fminf(agent->ragdoll_visual_blend, 1.0f));
    float upright_weight = 1.0f - fallen_weight;
    Vector3 shoulder_left = FromLimbVector(
        skin.sockets[CC_HUMANOID_SOCKET_SHOULDER_LEFT].position);
    Vector3 shoulder_right = FromLimbVector(
        skin.sockets[CC_HUMANOID_SOCKET_SHOULDER_RIGHT].position);
    Vector3 hip_left = FromLimbVector(
        skin.sockets[CC_HUMANOID_SOCKET_HIP_LEFT].position);
    Vector3 hip_right = FromLimbVector(
        skin.sockets[CC_HUMANOID_SOCKET_HIP_RIGHT].position);
    float upper_yaw = PoseAxisYaw(shoulder_left, shoulder_right,
                                  agent->facing_yaw + pose->chest_yaw);
    float pelvis_yaw = PoseAxisYaw(hip_left, hip_right,
                                   agent->facing_yaw + pose->pelvis_yaw);
    Vector3 body_up = FromLimbVector(skin.body_up);
    Vector3 cape_center = PhysicsAdd(
        FromLimbVector(skin.sockets[CC_HUMANOID_SOCKET_BACK].position),
        PhysicsScale(body_up, -0.24f));
    CcNpcAppearance gameplay_appearance = agent->crowned ?
        procedural_hero : agent->appearance;
    const CcNpcAppearance *appearance = &gameplay_appearance;
    float movement_weight = SmoothStep01(fabsf(gait->speed.value) / 0.90f);
    float movement_wave = sinf(gait->phase * 2.0f * PI);
    float signature_lean = appearance->idle_lean *
        (1.0f - movement_weight * 0.35f);
    float signature_sway = cosf(gait->phase * 2.0f * PI) * 0.018f *
        (1.0f - movement_weight) * appearance->bob_scale;
    float signature_bob = fabsf(movement_wave) * 0.014f * movement_weight *
        (appearance->bob_scale - 0.70f);
    Vector3 signature_forward = {sinf(upper_yaw), 0.0f, cosf(upper_yaw)};
    Vector3 signature_right = {cosf(upper_yaw), 0.0f, -sinf(upper_yaw)};
    Vector3 posture_offset = PhysicsAdd(
        PhysicsScale(signature_forward, signature_lean),
        PhysicsScale(signature_right, signature_sway));
    posture_offset.y += signature_bob;
    chest = PhysicsAdd(chest, posture_offset);
    shoulder_left = PhysicsAdd(shoulder_left, posture_offset);
    shoulder_right = PhysicsAdd(shoulder_right, posture_offset);
    neck = PhysicsAdd(neck, PhysicsScale(posture_offset, 1.18f));
    head = PhysicsAdd(head, PhysicsScale(posture_offset, 1.32f));
    cape_center = PhysicsAdd(cape_center, posture_offset);
    Color tunic = appearance->outer;
    if (CombatIsDefeated(&agent->combat)) tunic = (Color){61, 57, 62, 255};
    else if (agent->combat.hit_flash_seconds > 0.0f) tunic = WORLD_INK;
    Color trousers = CombatIsDefeated(&agent->combat) ?
                      (Color){50, 49, 51, 255} : appearance->trousers;
    Color leather = appearance->leather;
    float mass = appearance->body_mass;
    float muscle = appearance->muscularity;

    if (fallen_weight > 0.01f) {
        Vector3 fallen_back = PhysicsScale(
            (Vector3){sinf(upper_yaw), 0.0f, cosf(upper_yaw)}, -0.12f);
        DrawCylinderEx(PhysicsAdd(chest, fallen_back),
                       PhysicsAdd(pelvis, fallen_back),
                       0.17f, 0.25f, 5,
                       Fade((Color){73, 55, 91, 255}, fallen_weight));
        DrawCharacterSphere(pelvis, 0.18f * mass,
                            Fade(trousers, fallen_weight));
        DrawCylinderEx(pelvis, spine,
                       0.18f, 0.22f, 10,
                       Fade(appearance->underlayer, fallen_weight));
        DrawCylinderEx(spine, chest,
                       0.23f, 0.27f, 10,
                       Fade(tunic, fallen_weight));
        DrawCylinderEx(chest, neck, 0.18f, 0.09f, 9,
                       Fade(tunic, fallen_weight));
    }
    if (upright_weight > 0.01f) {
        if ((appearance->equipment & CC_NPC_EQUIPMENT_MANTLE) != 0U) {
            Color mantle = agent->crowned ? (Color){91, 46, 54, 255} :
                ShadeColor(appearance->outer, 0.68f);
            DrawOrientedBox(cape_center, (Vector3){0.0f, 0.04f, 0.0f},
                            (Vector3){0.46f * mass, 0.62f, 0.045f}, upper_yaw,
                            Fade(mantle, upright_weight));
        }
        DrawOrientedBox(pelvis, (Vector3){0.0f, 0.02f, 0.0f},
                        (Vector3){0.40f * mass, 0.18f, 0.27f * mass},
                        pelvis_yaw,
                        Fade(trousers, upright_weight));
        DrawCylinderEx(spine, chest,
                       0.27f * mass, 0.31f * appearance->shoulder_scale,
                       10,
                       Fade(tunic, upright_weight));
        DrawNpcGarmentCut(appearance, pelvis, chest, shoulder_left,
                          upper_yaw, 1.0f);
        if ((appearance->equipment & CC_NPC_EQUIPMENT_ARMOR) != 0U) {
            Vector3 plate = FromLimbVector(
                skin.sockets[CC_HUMANOID_SOCKET_CHEST_FRONT].position);
            DrawOrientedBox(plate, (Vector3){0.0f, -0.04f, 0.0f},
                            (Vector3){0.34f * mass, 0.28f, 0.045f}, upper_yaw,
                            Fade(appearance->metal, upright_weight));
        }
    }

    const CcHumanoidSkinBone thigh_bones[] = {
        CC_HUMANOID_SKIN_THIGH_LEFT,
        CC_HUMANOID_SKIN_THIGH_RIGHT,
    };
    const CcHumanoidSkinBone shin_bones[] = {
        CC_HUMANOID_SKIN_SHIN_LEFT,
        CC_HUMANOID_SKIN_SHIN_RIGHT,
    };
    const CcHumanoidSkinBone foot_bones[] = {
        CC_HUMANOID_SKIN_FOOT_LEFT,
        CC_HUMANOID_SKIN_FOOT_RIGHT,
    };
    for (int32_t leg = 0; leg < CC_HUMANOID_LEG_COUNT; ++leg) {
        Vector3 hip = FromLimbVector(skin.bones[thigh_bones[leg]].head);
        Vector3 knee = FromLimbVector(skin.bones[thigh_bones[leg]].tail);
        Vector3 ankle = FromLimbVector(skin.bones[shin_bones[leg]].tail);
        Vector3 heel = FromLimbVector(pose->heel[leg]);
        Vector3 toe = FromLimbVector(skin.bones[foot_bones[leg]].tail);
        float thigh_radius = (0.084f + muscle * 0.018f) * mass;
        DrawCylinderEx(hip, knee, thigh_radius, thigh_radius * 0.80f, 9,
                       trousers);
        DrawCylinderEx(knee, ankle, thigh_radius * 0.78f,
                       thigh_radius * 0.58f, 9, ShadeColor(trousers, 0.84f));
        DrawPitchedFoot(heel, toe, agent->facing_yaw, leather);
    }

    const CcHumanoidSkinBone upper_arm_bones[] = {
        CC_HUMANOID_SKIN_UPPER_ARM_LEFT,
        CC_HUMANOID_SKIN_UPPER_ARM_RIGHT,
    };
    const CcHumanoidSkinBone forearm_bones[] = {
        CC_HUMANOID_SKIN_FOREARM_LEFT,
        CC_HUMANOID_SKIN_FOREARM_RIGHT,
    };
    for (int32_t arm = 0; arm < CC_HUMANOID_ARM_COUNT; ++arm) {
        Vector3 shoulder = FromLimbVector(
            skin.bones[upper_arm_bones[arm]].head);
        Vector3 elbow = FromLimbVector(
            skin.bones[upper_arm_bones[arm]].tail);
        Vector3 hand = FromLimbVector(
            skin.bones[forearm_bones[arm]].tail);
        float arm_direction = arm == 0 ? -1.0f : 1.0f;
        float signature_swing = movement_wave * 0.045f * movement_weight *
            appearance->arm_swing_scale * arm_direction;
        Vector3 arm_offset = PhysicsAdd(
            posture_offset, PhysicsScale(signature_forward,
                                          signature_swing));
        shoulder = PhysicsAdd(shoulder, posture_offset);
        elbow = PhysicsAdd(elbow, PhysicsScale(arm_offset, 0.72f));
        hand = PhysicsAdd(hand, arm_offset);
        DrawCylinderEx(shoulder, elbow, 0.074f * mass, 0.061f * mass, 8,
                       tunic);
        DrawCylinderEx(elbow, hand, 0.061f * mass, 0.050f * mass, 8,
                       appearance->underlayer);
        if ((appearance->equipment & CC_NPC_EQUIPMENT_ARMOR) != 0U) {
            DrawCharacterSphere(shoulder, 0.092f * mass,
                                appearance->metal);
        }
        DrawSmallSphere(hand, 0.062f, appearance->skin);
    }

    Vector3 head_up = PhysicsNormalizeOr(PhysicsSubtract(head, neck),
                                         (Vector3){0.0f, 1.0f, 0.0f});
    Vector3 fallback_right = {cosf(upper_yaw), 0.0f, -sinf(upper_yaw)};
    Vector3 shoulder_axis = PhysicsSubtract(shoulder_right, shoulder_left);
    Vector3 head_right = PhysicsSubtract(
        shoulder_axis, PhysicsScale(head_up,
                                    PhysicsDot(shoulder_axis, head_up)));
    head_right = PhysicsNormalizeOr(head_right, fallback_right);
    Vector3 head_forward = PhysicsNormalizeOr(PhysicsCross(head_right, head_up),
        (Vector3){sinf(upper_yaw), 0.0f, cosf(upper_yaw)});
    (void)head_forward;
    (void)head_right;
    DrawNpcHead(appearance, head, upper_yaw, 1.0f);
    CcNpcAppearance accessories = *appearance;
    accessories.equipment &= ~(uint32_t)(CC_NPC_EQUIPMENT_ARMOR |
                                         CC_NPC_EQUIPMENT_MANTLE);
    DrawNpcEquipment(&accessories, agent->position, pelvis, chest,
                     shoulder_left, shoulder_right,
                     FromLimbVector(skin.bones[CC_HUMANOID_SKIN_FOREARM_LEFT].tail),
                     upper_yaw, 1.0f);
    if (draw_hero_rig_debug) {
        Vector3 rig_offset = LocalPoint((Vector3){0}, 0.022f, 0.010f, 0.040f,
                                        upper_yaw);
        Vector3 physical_root = {gait->body.root.position.x,
                                 gait->body.root.position.y,
                                 gait->body.root.position.z};
        Vector3 reaction_end = physical_root;
        if (gait->body.total_mass > 0.0f) {
            reaction_end.x += gait->ground_reaction.x /
                              gait->body.total_mass * 0.012f;
            reaction_end.y += gait->ground_reaction.y /
                              gait->body.total_mass * 0.012f;
            reaction_end.z += gait->ground_reaction.z /
                              gait->body.total_mass * 0.012f;
        }
        if (upright_weight > 0.01f) {
            DrawSmallSphere(Add3(physical_root, rig_offset), 0.052f,
                            Fade(WORLD_TEAL, upright_weight));
            DrawCylinderEx(Add3(physical_root, rig_offset),
                           Add3(reaction_end, rig_offset), 0.014f, 0.008f, 6,
                           Fade(WORLD_TEAL, 0.82f * upright_weight));
        }
        Vector3 rig_pelvis = Add3(pelvis, rig_offset);
        Vector3 rig_spine = Add3(spine, rig_offset);
        Vector3 rig_chest = Add3(chest, rig_offset);
        Vector3 rig_neck = Add3(neck, rig_offset);
        DrawBoneSegment(rig_pelvis, rig_spine, WORLD_GOLD);
        DrawBoneSegment(rig_spine, rig_chest, WORLD_GOLD);
        DrawBoneSegment(rig_chest, rig_neck, WORLD_GOLD);
        DrawBoneJoint(rig_pelvis, WORLD_GOLD);
        DrawBoneJoint(rig_spine, WORLD_GOLD);
        DrawBoneJoint(rig_chest, WORLD_GOLD);
        DrawBoneJoint(rig_neck, WORLD_GOLD);
        for (int32_t leg = 0; leg < CC_HUMANOID_LEG_COUNT; ++leg) {
            Color bone = HumanoidContactColor(gait->feet[leg].contact);
            Vector3 hip = Add3(FromLimbVector(
                skin.bones[thigh_bones[leg]].head), rig_offset);
            Vector3 knee = Add3(FromLimbVector(
                skin.bones[thigh_bones[leg]].tail), rig_offset);
            Vector3 ankle = Add3(FromLimbVector(
                skin.bones[shin_bones[leg]].tail), rig_offset);
            Vector3 heel = Add3(FromLimbVector(pose->heel[leg]), rig_offset);
            Vector3 ball = Add3(FromLimbVector(pose->ball[leg]), rig_offset);
            Vector3 toe = Add3(FromLimbVector(pose->toe[leg]), rig_offset);
            DrawBoneSegment(hip, knee, bone);
            DrawBoneSegment(knee, ankle, bone);
            DrawBoneSegment(ankle, heel, bone);
            DrawBoneSegment(heel, ball, bone);
            DrawBoneSegment(ball, toe, bone);
            DrawBoneJoint(hip, bone);
            DrawBoneJoint(knee, bone);
            DrawBoneJoint(ankle, bone);
            DrawBoneJoint(heel, bone);
            DrawBoneJoint(ball, bone);
            DrawBoneJoint(toe, bone);
        }
        for (int32_t arm = 0; arm < CC_HUMANOID_ARM_COUNT; ++arm) {
            Vector3 shoulder = Add3(FromLimbVector(
                skin.bones[upper_arm_bones[arm]].head), rig_offset);
            Vector3 elbow = Add3(FromLimbVector(
                skin.bones[upper_arm_bones[arm]].tail), rig_offset);
            Vector3 hand = Add3(FromLimbVector(
                skin.bones[forearm_bones[arm]].tail), rig_offset);
            DrawBoneSegment(shoulder, elbow, WORLD_GOLD);
            DrawBoneSegment(elbow, hand, WORLD_GOLD);
            DrawBoneJoint(shoulder, WORLD_GOLD);
            DrawBoneJoint(elbow, WORLD_GOLD);
            DrawBoneJoint(hand, WORLD_GOLD);
        }
    }
    RestoreWorldLighting();
}

static Vector3 SolveVisualTwoBone(Vector3 root, Vector3 target,
                                  Vector3 bend_direction,
                                  float upper_length, float lower_length)
{
    Vector3 delta = PhysicsSubtract(target, root);
    float raw_distance = PhysicsLength(delta);
    Vector3 direction = raw_distance > 0.0001f ?
                        PhysicsScale(delta, 1.0f / raw_distance) :
                        (Vector3){0.0f, -1.0f, 0.0f};
    float distance = fmaxf(fabsf(upper_length - lower_length) + 0.0001f,
                           fminf(raw_distance,
                                 upper_length + lower_length - 0.0001f));
    Vector3 pole = PhysicsSubtract(
        bend_direction, PhysicsScale(direction,
                                     PhysicsDot(bend_direction, direction)));
    float pole_length = PhysicsLength(pole);
    if (pole_length <= 0.0001f) pole = (Vector3){1.0f, 0.0f, 0.0f};
    else pole = PhysicsScale(pole, 1.0f / pole_length);
    float along = (upper_length * upper_length - lower_length * lower_length +
                   distance * distance) / (2.0f * distance);
    float height = sqrtf(fmaxf(0.0f,
                               upper_length * upper_length - along * along));
    return PhysicsAdd(PhysicsAdd(root, PhysicsScale(direction, along)),
                      PhysicsScale(pole, height));
}

static float SnapContactCoordinate(float value)
{
    const float contact_grid = 0.125f;
    return roundf(value / contact_grid) * contact_grid;
}

static void DrawCharacterContactEffects(const CcLocalAgent *agent)
{
    if (agent == NULL) return;
    bool fallen = agent->humanoid.ragdoll.active ||
                  agent->ragdoll_visual_blend > 0.42f;
    float surface = SurfaceHeightAt(agent->scene, agent->position.x,
                                    agent->position.z);
    Vector3 shadow = {
        SnapContactCoordinate(agent->position.x),
        fmaxf(0.006f, surface + 0.008f),
        SnapContactCoordinate(agent->position.z),
    };
    Vector2 shadow_size = fallen ? (Vector2){1.20f, 0.56f} :
                                   (Vector2){0.70f, 0.46f};
    DrawContactShadow(shadow, shadow_size.x, shadow_size.y,
                      agent->facing_yaw,
                      (Color){3, 8, 10, fallen ? 102 : 82});

    if (agent->swimming || agent->immersion > 0.08f) {
        float phase = agent->humanoid.swim_phase -
                      floorf(agent->humanoid.swim_phase);
        float pulse = 0.34f + phase * 0.30f;
        Vector3 water = {shadow.x, COURSE_WATER_SURFACE + 0.022f, shadow.z};
        DrawCylinderWires(water, pulse, pulse, 0.018f, 16,
                          Fade((Color){126, 220, 222, 255},
                               0.62f * (1.0f - phase)));
        DrawCylinderWires((Vector3){water.x, water.y + 0.006f, water.z},
                          pulse * 0.62f, pulse * 0.62f, 0.018f, 12,
                          Fade(WORLD_TEAL, 0.48f));
        return;
    }

    bool stepping = agent->morphology == CC_MORPHOLOGY_BIPED &&
                    agent->grounded && !agent->climbing &&
                    !agent->humanoid.ragdoll.active &&
                    agent->humanoid.speed.value > 0.16f &&
                    agent->stepped_pose.initialized;
    if (!stepping) return;
    int32_t bin = agent->stepped_pose.locomotion_bin;
    if (bin != 0 && bin != 4) return;
    int32_t leg = bin == 0 ? 0 : 1;
    const CcHumanoidPose *pose = AgentRenderPose(agent);
    Vector3 heel = FromLimbVector(pose->heel[leg]);
    Vector3 toe = FromLimbVector(pose->toe[leg]);
    heel.x = SnapContactCoordinate(heel.x);
    heel.z = SnapContactCoordinate(heel.z);
    toe.x = SnapContactCoordinate(toe.x);
    toe.z = SnapContactCoordinate(toe.z);
    heel.y = fmaxf(0.016f, agent->position.y + 0.014f);
    toe.y = heel.y;
    Color print = (Color){67, 59, 48, 116};
    DrawLine3D(heel, toe, print);
    DrawBox(heel, (Vector3){0.16f, 0.014f, 0.11f}, print);

    float within = agent->humanoid.phase * 8.0f;
    within -= floorf(within);
    float dust = 1.0f - SmoothStep01(within / 0.72f);
    Vector3 back = {-sinf(agent->facing_yaw), 0.0f,
                    -cosf(agent->facing_yaw)};
    Vector3 dust_smear = PhysicsAdd(heel, PhysicsScale(back, 0.08f));
    dust_smear.y = shadow.y + 0.006f;
    DrawGroundBrushStroke(dust_smear, back, 0.32f * dust,
                          0.12f * dust,
                          Fade((Color){150, 125, 86, 255}, dust * 0.42f));
}

static void DrawRobotShell(const CcLocalAgent *agent)
{
    DrawCharacterContactEffects(agent);
    Vector3 body = ShellPodCenter(agent);
    const CcLimbRig *rig = &agent->limb_rig;
    bool biped = rig->morphology.preset == CC_MORPHOLOGY_BIPED;
    if (biped && agent->humanoid.initialized) {
        DrawBiomechanicalBiped(agent);
        return;
    }
    if (!biped) {
        float body_length = rig->morphology.limb_count >= 6 ? 0.72f : 0.54f;
        DrawOrientedBox(body, (Vector3){0.0f, 0.0f, 0.0f},
                        (Vector3){0.54f, 0.38f, body_length}, agent->facing_yaw,
                        (Color){42, 128, 136, 255});
        DrawSmallSphere(body, 0.085f, WORLD_GOLD);
        DrawOrientedBox(body, (Vector3){0.0f, 0.02f, 0.22f},
                        (Vector3){0.24f, 0.18f, 0.28f}, agent->facing_yaw,
                        (Color){225, 177, 68, 255});
        DrawSmallSphere(
            LocalPoint(body, -0.085f, 0.06f, 0.34f, agent->facing_yaw),
            0.040f, WORLD_VOID);
        DrawSmallSphere(
            LocalPoint(body, 0.085f, 0.06f, 0.34f, agent->facing_yaw),
            0.040f, WORLD_VOID);
    }
    Vector3 rig_offset = LocalPoint((Vector3){0}, 0.022f, 0.010f, 0.040f,
                                    agent->facing_yaw);
    for (int32_t limb_index = 0; limb_index < rig->morphology.limb_count;
         ++limb_index) {
        const CcLimbRuntime *limb = &rig->limbs[limb_index];
        const CcLimbSpec *spec = &rig->morphology.limbs[limb_index];
        Color bone = limb->state == CC_LIMB_SWING ? WORLD_VIOLET :
                     limb->state == CC_LIMB_DISABLED ? WORLD_DANGER : WORLD_GOLD;
        for (int32_t segment = 0; segment < spec->segment_count; ++segment) {
            Vector3 a = FromLimbVector(limb->joints[segment]);
            Vector3 b = FromLimbVector(limb->joints[segment + 1]);
            float skin = segment == 0 ? 0.072f : 0.058f;
            DrawCylinderEx(a, b, skin, skin * 0.82f, 8,
                           limb->state == CC_LIMB_DISABLED ?
                           (Color){67, 48, 51, 255} : (Color){54, 66, 71, 255});
            Vector3 rig_a = Add3(a, rig_offset);
            Vector3 rig_b = Add3(b, rig_offset);
            DrawCylinderEx(rig_a, rig_b, 0.022f, 0.022f, 7, bone);
            DrawSmallSphere(rig_a, 0.046f, bone);
        }
        Vector3 foot = FromLimbVector(limb->joints[spec->segment_count]);
        DrawSmallSphere(foot, 0.076f, bone);
        Color foot_color = limb->state == CC_LIMB_SWING ?
                           Fade(WORLD_VIOLET, 0.72f) :
                           limb->state == CC_LIMB_DISABLED ?
                           Fade(WORLD_DANGER, 0.72f) : (Color){37, 62, 67, 255};
        if (biped) {
            if (agent->traversal == CC_TRAVERSAL_CLIMB &&
                fabsf(limb->contact_normal.y) < 0.5f) {
                Vector3 size = fabsf(limb->contact_normal.x) > 0.5f ?
                               (Vector3){0.09f, 0.28f, 0.17f} :
                               (Vector3){0.17f, 0.28f, 0.09f};
                Vector3 center = PhysicsAdd(foot,
                                            PhysicsScale(FromLimbVector(
                                                limb->contact_normal), 0.035f));
                DrawBox(center, size, foot_color);
            } else {
                DrawOrientedBox(foot, (Vector3){0.0f, 0.025f, 0.075f},
                                (Vector3){0.17f, 0.09f, 0.30f}, agent->facing_yaw,
                                foot_color);
            }
        } else {
            DrawCylinder((Vector3){foot.x, foot.y - 0.02f, foot.z},
                         0.11f, 0.09f, 0.04f, 8, foot_color);
        }
    }
    if (biped) {
        float arm_swing = 0.0f;
        float upper_yaw = agent->facing_yaw;
        float climb_lean = agent->traversal == CC_TRAVERSAL_CLIMB ?
                           sinf(agent->climb_progress * PI) * 0.11f : 0.0f;
        float hero_lean = climb_lean;
        Vector3 spine_base = LocalPoint(body, 0.0f, 0.03f, 0.010f,
                                        upper_yaw);
        Vector3 chest = LocalPoint(body, 0.0f, 0.41f, 0.020f + hero_lean,
                                   upper_yaw);
        Vector3 neck = LocalPoint(body, 0.0f, 0.66f,
                                  0.020f + hero_lean * 1.20f, upper_yaw);
        Vector3 shoulder_l = LocalPoint(body, -0.285f, 0.50f,
                                        hero_lean * 0.92f, upper_yaw);
        Vector3 shoulder_r = LocalPoint(body, 0.285f, 0.50f,
                                        hero_lean * 0.92f, upper_yaw);
        Vector3 elbow_l = LocalPoint(body, -0.34f, 0.27f,
                                     -arm_swing * 0.68f, upper_yaw);
        Vector3 elbow_r = LocalPoint(body, 0.34f, 0.27f,
                                     arm_swing * 0.68f, upper_yaw);
        Vector3 hand_l = LocalPoint(body, -0.30f, 0.04f,
                                    -arm_swing * 1.35f, upper_yaw);
        Vector3 hand_r = LocalPoint(body, 0.30f, 0.04f,
                                    arm_swing * 1.35f, upper_yaw);
        if (agent->traversal == CC_TRAVERSAL_CLIMB) {
            float reach = SmoothStep01(agent->climb_progress / 0.18f);
            float release = SmoothStep01((agent->climb_progress - 0.76f) / 0.24f);
            Vector3 resting_left = hand_l;
            Vector3 resting_right = hand_r;
            Vector3 held_left = PhysicsLerp(resting_left,
                                            agent->climb_hand_left, reach);
            Vector3 held_right = PhysicsLerp(resting_right,
                                             agent->climb_hand_right, reach);
            hand_l = PhysicsLerp(held_left, resting_left, release);
            hand_r = PhysicsLerp(held_right, resting_right, release);
            Vector3 bend_left = FramePoint((Vector3){0}, -1.0f, 0.15f, -0.18f,
                                           upper_yaw);
            Vector3 bend_right = FramePoint((Vector3){0}, 1.0f, 0.15f, -0.18f,
                                            upper_yaw);
            elbow_l = SolveVisualTwoBone(shoulder_l, hand_l, bend_left,
                                         0.36f, 0.38f);
            elbow_r = SolveVisualTwoBone(shoulder_r, hand_r, bend_right,
                                         0.36f, 0.38f);
        }

        DrawOrientedBox(body, (Vector3){0.0f, 0.36f, -0.155f},
                        (Vector3){0.46f, 0.56f, 0.045f}, upper_yaw,
                        (Color){73, 55, 91, 255});
        DrawOrientedBox(body, (Vector3){0.0f, 0.03f, 0.0f},
                        (Vector3){0.40f, 0.18f, 0.27f}, upper_yaw,
                        (Color){44, 61, 65, 255});
        DrawOrientedBox(body, (Vector3){0.0f, 0.36f, hero_lean * 0.55f},
                        (Vector3){0.50f, 0.50f, 0.29f}, upper_yaw,
                        (Color){42, 128, 136, 255});
        DrawOrientedBox(body, (Vector3){0.0f, 0.39f, 0.165f + hero_lean * 0.55f},
                        (Vector3){0.34f, 0.29f, 0.045f}, upper_yaw,
                        (Color){223, 173, 67, 255});
        DrawCylinderEx(shoulder_l, elbow_l, 0.065f, 0.055f, 8,
                       (Color){42, 128, 136, 255});
        DrawCylinderEx(elbow_l, hand_l, 0.055f, 0.045f, 8,
                       (Color){54, 66, 71, 255});
        DrawCylinderEx(shoulder_r, elbow_r, 0.065f, 0.055f, 8,
                       (Color){42, 128, 136, 255});
        DrawCylinderEx(elbow_r, hand_r, 0.055f, 0.045f, 8,
                       (Color){54, 66, 71, 255});
        DrawCharacterSphere(shoulder_l, 0.095f,
                            (Color){223, 173, 67, 255});
        DrawCharacterSphere(shoulder_r, 0.095f,
                            (Color){223, 173, 67, 255});
        DrawSmallSphere(hand_l, 0.055f, (Color){221, 174, 118, 255});
        DrawSmallSphere(hand_r, 0.055f, (Color){221, 174, 118, 255});

        Vector3 visual_spine_base = Add3(spine_base, rig_offset);
        Vector3 visual_chest = Add3(chest, rig_offset);
        Vector3 visual_neck = Add3(neck, rig_offset);
        Vector3 visual_shoulder_l = Add3(shoulder_l, rig_offset);
        Vector3 visual_shoulder_r = Add3(shoulder_r, rig_offset);
        Vector3 visual_elbow_l = Add3(elbow_l, rig_offset);
        Vector3 visual_elbow_r = Add3(elbow_r, rig_offset);
        Vector3 visual_hand_l = Add3(hand_l, rig_offset);
        Vector3 visual_hand_r = Add3(hand_r, rig_offset);
        DrawBoneSegment(visual_spine_base, visual_chest, WORLD_GOLD);
        DrawBoneSegment(visual_chest, visual_neck, WORLD_GOLD);
        DrawBoneSegment(visual_shoulder_l, visual_shoulder_r, WORLD_GOLD);
        DrawBoneSegment(visual_shoulder_l, visual_elbow_l, WORLD_GOLD);
        DrawBoneSegment(visual_elbow_l, visual_hand_l, WORLD_GOLD);
        DrawBoneSegment(visual_shoulder_r, visual_elbow_r, WORLD_GOLD);
        DrawBoneSegment(visual_elbow_r, visual_hand_r, WORLD_GOLD);
        Vector3 visual_joints[] = {
            visual_spine_base, visual_chest,      visual_neck,
            visual_shoulder_l, visual_shoulder_r, visual_elbow_l,
            visual_elbow_r,    visual_hand_l,     visual_hand_r,
        };
        for (size_t joint_index = 0;
             joint_index < sizeof(visual_joints) / sizeof(visual_joints[0]);
             joint_index++) {
            DrawBoneJoint(visual_joints[joint_index], WORLD_GOLD);
        }

        Vector3 head = LocalPoint(body, 0.0f, 0.81f, hero_lean * 1.25f,
                                  upper_yaw);
        DrawCharacterSphere(head, 0.18f, (Color){221, 174, 118, 255});
        DrawCharacterSphere(
            LocalPoint(body, 0.0f, 0.91f,
                       -0.025f + hero_lean * 1.25f, upper_yaw),
            0.145f, (Color){52, 46, 51, 255});
        DrawSmallSphere(
            LocalPoint(body, -0.058f, 0.82f,
                       0.165f + hero_lean * 1.25f, upper_yaw),
            0.022f, WORLD_VOID);
        DrawSmallSphere(
            LocalPoint(body, 0.058f, 0.82f,
                       0.165f + hero_lean * 1.25f, upper_yaw),
            0.022f, WORLD_VOID);
        DrawSmallSphere(
            LocalPoint(body, 0.0f, 1.11f, hero_lean * 1.20f, upper_yaw),
            0.060f, WORLD_GOLD);
    } else {
        DrawSmallSphere(
            LocalPoint(body, 0.0f, 0.52f, 0.0f, agent->facing_yaw),
            0.065f, WORLD_GOLD);
    }
}

static void DrawCarriage3D(const CcSettlement *place)
{
    float x = CARRIAGE_FOOTPRINT.x + CARRIAGE_FOOTPRINT.width * 0.5f;
    float z = CARRIAGE_FOOTPRINT.y + CARRIAGE_FOOTPRINT.height * 0.5f;
    rlPushMatrix();
    rlTranslatef(0.0f, TerrainFootprintHeight(CARRIAGE_FOOTPRINT), 0.0f);
    RuntimeAsset *carriage = &runtime_assets[RUNTIME_ASSET_CARRIAGE];
    if (carriage->ready) {
        Vector3 origin = {x, 0.0f, z};
        DrawModelEx(carriage->model, origin, (Vector3){0.0f, 1.0f, 0.0f},
                    CARRIAGE_ASSET_STREET_YAW_DEGREES,
                    (Vector3){CARRIAGE_ASSET_SCALE,
                              CARRIAGE_ASSET_SCALE,
                              CARRIAGE_ASSET_SCALE}, WHITE);
        int32_t cargo = place != NULL ?
            place->stock[CC_GOOD_FOOD] + place->stock[CC_GOOD_MATERIAL] +
            place->stock[CC_GOOD_TOOLS] : 0;
        RuntimeAsset *rack = &runtime_assets[RUNTIME_ASSET_CARGO_RACK];
        if (cargo >= 36 && rack->ready) {
            DrawModelEx(rack->model, origin,
                        (Vector3){0.0f, 1.0f, 0.0f},
                        CARRIAGE_ASSET_STREET_YAW_DEGREES,
                        (Vector3){CARRIAGE_ASSET_SCALE,
                                  CARRIAGE_ASSET_SCALE,
                                  CARRIAGE_ASSET_SCALE}, WHITE);
        }
        rlPopMatrix();
        return;
    }
    DrawBox((Vector3){x, 1.06f, z},
            (Vector3){CARRIAGE_FOOTPRINT.width, 1.62f,
                      CARRIAGE_FOOTPRINT.height},
            (Color){111, 65, 54, 255});
    DrawBox((Vector3){x, 1.94f, z},
            (Vector3){CARRIAGE_FOOTPRINT.width + 0.22f, 0.18f,
                      CARRIAGE_FOOTPRINT.height + 0.18f},
            (Color){143, 124, 86, 255});
    DrawCubeWiresV((Vector3){x, 1.94f, z},
                   (Vector3){CARRIAGE_FOOTPRINT.width + 0.24f, 0.20f,
                             CARRIAGE_FOOTPRINT.height + 0.20f},
                   Fade(WORLD_GOLD, 0.58f));
    DrawBox((Vector3){CARRIAGE_FOOTPRINT.x + CARRIAGE_FOOTPRINT.width + 0.03f,
                       1.20f, z}, (Vector3){0.04f, 0.70f, 1.20f},
            (Color){35, 102, 108, 255});
    Vector3 wheels[] = {
        {CARRIAGE_FOOTPRINT.x, 0.58f, CARRIAGE_FOOTPRINT.y + 1.15f},
        {CARRIAGE_FOOTPRINT.x + CARRIAGE_FOOTPRINT.width, 0.58f,
         CARRIAGE_FOOTPRINT.y + 1.15f},
        {CARRIAGE_FOOTPRINT.x, 0.58f,
         CARRIAGE_FOOTPRINT.y + CARRIAGE_FOOTPRINT.height - 1.15f},
        {CARRIAGE_FOOTPRINT.x + CARRIAGE_FOOTPRINT.width, 0.58f,
         CARRIAGE_FOOTPRINT.y + CARRIAGE_FOOTPRINT.height - 1.15f}
    };
    for (int32_t i = 0; i < 4; ++i) {
        DrawScenerySphere(wheels[i], 0.54f, (Color){38, 31, 31, 255});
        DrawSphereWires(wheels[i], 0.55f, 7, 7,
                        Fade(WORLD_GOLD, 0.62f));
    }
    rlPopMatrix();
}

static void DrawNotice3D(const CcSim *sim)
{
    float x = CC_LOCAL_NOTICE_X;
    float z = CC_LOCAL_NOTICE_Z;
    rlPushMatrix();
    rlTranslatef(0.0f, CcLocalTerrainHeightAt(x, z), 0.0f);
    DrawBox((Vector3){x - 0.38f, 0.72f, z}, (Vector3){0.10f, 1.44f, 0.10f},
            (Color){89, 58, 42, 255});
    DrawBox((Vector3){x + 0.38f, 0.72f, z}, (Vector3){0.10f, 1.44f, 0.10f},
            (Color){89, 58, 42, 255});
    DrawBox((Vector3){x, 1.32f, z}, (Vector3){1.08f, 0.82f, 0.12f},
            (Color){148, 94, 52, 255});
    int32_t count = CcSimActiveSituationCount(sim);
    for (int32_t i = 0; i < count && i < 4; ++i) {
        DrawBox((Vector3){x - 0.34f + (float)i * 0.22f,
                          1.34f + (float)(i & 1) * 0.12f, z - 0.07f},
                (Vector3){0.16f, 0.30f, 0.025f},
                i == 3 ? WORLD_DANGER : WORLD_INK);
    }
    rlPopMatrix();
}

static void DrawDungeon3D(const CcDungeon *dungeon)
{
    float x = CC_LOCAL_DUNGEON_X;
    float z = DUNGEON_FOOTPRINT.y + DUNGEON_FOOTPRINT.height * 0.5f;
    rlPushMatrix();
    rlTranslatef(0.0f, TerrainFootprintHeight(DUNGEON_FOOTPRINT), 0.0f);
    RuntimeAsset *mine = &runtime_assets[RUNTIME_ASSET_MINE];
    if (mine->ready) {
        const float scale = 0.58f;
        DrawModelEx(mine->model, (Vector3){x, 0.0f, z - 0.35f},
                    (Vector3){0.0f, 1.0f, 0.0f}, 0.0f,
                    (Vector3){scale, scale, scale}, WHITE);
        float pulse = 0.06f + (float)dungeon->regional_pressure / 500.0f;
        DrawScenerySphere((Vector3){x, 0.86f,
                                    DUNGEON_FOOTPRINT.y +
                                    DUNGEON_FOOTPRINT.height + 0.08f},
                          pulse, Fade(WORLD_VIOLET, 0.82f));
        rlPopMatrix();
        return;
    }
    DrawBox((Vector3){x - 1.15f, 1.40f, z}, (Vector3){0.62f, 2.80f, 0.82f},
            (Color){64, 56, 72, 255});
    DrawBox((Vector3){x + 1.15f, 1.40f, z}, (Vector3){0.62f, 2.80f, 0.82f},
            (Color){64, 56, 72, 255});
    DrawBox((Vector3){x, 2.84f, z}, (Vector3){2.92f, 0.42f, 0.82f},
            (Color){74, 62, 84, 255});
    DrawBox((Vector3){x, 1.22f, DUNGEON_FOOTPRINT.y +
                                     DUNGEON_FOOTPRINT.height + 0.03f},
            (Vector3){1.28f, 2.44f, 0.05f},
            (Color){8, 5, 14, 255});
    float pulse = 0.06f + (float)dungeon->regional_pressure / 500.0f;
    DrawScenerySphere((Vector3){x, 1.32f, DUNGEON_FOOTPRINT.y +
                                            DUNGEON_FOOTPRINT.height + 0.08f}, pulse,
                      Fade(WORLD_VIOLET, 0.82f));
    rlPopMatrix();
}

typedef enum TreeRegion {
    TREE_REGION_VERDANT = 0,
    TREE_REGION_EMBER,
    TREE_REGION_BLUE
} TreeRegion;

typedef struct TreeRegionalStyle {
    TreeRegion region;
    Vector3 proportions;
    Color foliage_bias;
    float foliage_mix;
    float cluster_pull;
    float turn_steps;
    float shape_shift;
} TreeRegionalStyle;

static TreeRegionalStyle TreeStyleForKingdom(Color kingdom)
{
    if (kingdom.r > kingdom.g && kingdom.r > kingdom.b) {
        return (TreeRegionalStyle){
            TREE_REGION_EMBER, {1.14f, 0.94f, 1.10f},
            {112, 99, 58, 255}, 0.24f, 0.34f, 5.0f, 0.08f
        };
    }
    if (kingdom.b > kingdom.r && kingdom.b > kingdom.g) {
        return (TreeRegionalStyle){
            TREE_REGION_BLUE, {0.98f, 1.06f, 0.88f},
            {65, 89, 109, 255}, 0.22f, 0.22f, 6.0f, -0.04f
        };
    }
    return (TreeRegionalStyle){
        TREE_REGION_VERDANT, {0.90f, 1.14f, 0.94f},
        {43, 107, 88, 255}, 0.20f, 0.28f, 4.0f, 0.02f
    };
}

static TreeFamily RegionalTreeFamily(TreeFamily family, int32_t tree_index,
                                     TreeRegionalStyle style)
{
    if (style.region == TREE_REGION_EMBER &&
        family == TREE_FAMILY_ALDER && tree_index % 4 == 0) {
        return TREE_FAMILY_OAK;
    }
    if (style.region == TREE_REGION_BLUE &&
        family == TREE_FAMILY_ALDER && tree_index % 5 == 1) {
        return TREE_FAMILY_POLLARD;
    }
    return family;
}

static Model TreeCrownModel(TreeCrownShape shape)
{
    switch (shape) {
        case TREE_CROWN_OAK: return tree_crown_models.oak;
        case TREE_CROWN_POLLARD: return tree_crown_models.pollard;
        case TREE_CROWN_ALDER:
        default: return tree_crown_models.alder;
    }
}

static void DrawTreeFoliageMass(TreeCrownShape shape, Vector3 center,
                                Vector3 radius, float turn, Color color)
{
    Model model = tree_crown_models.ready ? TreeCrownModel(shape) :
                                           sphere_models.small;
    DrawModelEx(model, center, (Vector3){0.0f, 1.0f, 0.0f},
                turn * RAD2DEG, radius, color);
}

static void DrawTreeShadow(Vector3 root, float turn, float width, float depth)
{
    Vector3 broad = LocalPoint(root, width * 0.24f, 0.018f,
                               -depth * 0.20f, turn);
    Vector3 branch = LocalPoint(root, width * 0.54f, 0.020f,
                                -depth * 0.42f, turn);
    DrawModelEx(sphere_models.small, broad, (Vector3){0.0f, 1.0f, 0.0f},
                turn * RAD2DEG, (Vector3){width, 0.018f, depth},
                Fade(WORLD_VOID, 0.26f));
    DrawModelEx(sphere_models.small, branch, (Vector3){0.0f, 1.0f, 0.0f},
                (turn - 0.38f) * RAD2DEG,
                (Vector3){width * 0.62f, 0.020f, depth * 0.45f},
                Fade(WORLD_VOID, 0.20f));
}

static void DrawTreeRootFlare(Vector3 root, float turn, float radius,
                              Color bark)
{
    for (int32_t root_index = 0; root_index < 4; ++root_index) {
        float angle = turn + (float)root_index * PI * 0.5f +
                      (root_index & 1 ? 0.18f : -0.12f);
        Vector3 start = {root.x, root.y + radius * 0.72f, root.z};
        Vector3 end = {
            root.x + cosf(angle) * radius * 2.35f,
            root.y + 0.025f,
            root.z + sinf(angle) * radius * 2.35f
        };
        DrawCylinderEx(start, end, radius * 0.56f, 0.025f, 6,
                       ShadeColor(bark, root_index & 1 ? 0.76f : 0.88f));
    }
}

static void DrawAlderTree(Vector3 root, float turn, float shape, Color leaves)
{
    float scale = 0.90f + shape * 0.18f;
    float height = (4.65f + shape * 0.55f) * scale;
    float lean = (shape - 0.5f) * 0.34f;
    Color bark = BlendColor((Color){57, 47, 41, 255},
                            (Color){94, 62, 43, 255}, shape * 0.44f);
    Color deep = ShadeColor(leaves, 0.78f);
    Color middle = ShadeColor(leaves, 0.94f + shape * 0.04f);
    Color light = ShadeColor(leaves, 1.08f);
    Vector3 fork = LocalPoint(root, lean, height * 0.48f, 0.0f, turn);
    Vector3 left = LocalPoint(root, -0.72f * scale, height * 0.68f,
                              0.08f * scale, turn);
    Vector3 right = LocalPoint(root, 0.68f * scale, height * 0.72f,
                               -0.10f * scale, turn);
    Vector3 crown = LocalPoint(root, lean * 1.35f, height * 0.80f,
                               0.04f * scale, turn);
    Vector3 top = LocalPoint(root, lean * 1.70f, height * 0.93f,
                             -0.02f * scale, turn);

    DrawTreeShadow(root, turn, 1.18f * scale, 0.90f * scale);
    DrawTreeRootFlare(root, turn, 0.25f * scale, bark);
    DrawCylinderEx(root, fork, 0.27f * scale, 0.15f * scale, 7, bark);
    DrawCylinderEx(fork, left, 0.14f * scale, 0.055f * scale, 6,
                   ShadeColor(bark, 0.88f));
    DrawCylinderEx(fork, right, 0.13f * scale, 0.050f * scale, 6,
                   ShadeColor(bark, 0.94f));
    DrawCylinderEx(fork, top, 0.15f * scale, 0.040f * scale, 6, bark);

    DrawTreeFoliageMass(TREE_CROWN_ALDER, left,
                        (Vector3){0.86f, 0.68f, 0.72f}, turn + 0.18f, deep);
    DrawTreeFoliageMass(TREE_CROWN_ALDER, right,
                        (Vector3){0.84f, 0.70f, 0.70f}, turn - 0.21f, middle);
    DrawTreeFoliageMass(TREE_CROWN_ALDER, crown,
                        (Vector3){0.90f, 0.76f, 0.76f}, turn + 0.08f, middle);
    DrawTreeFoliageMass(TREE_CROWN_ALDER,
                        LocalPoint(root, -0.38f * scale, height * 0.81f,
                                   -0.36f * scale, turn),
                        (Vector3){0.68f, 0.62f, 0.62f}, turn - 0.32f, deep);
    DrawTreeFoliageMass(TREE_CROWN_ALDER,
                        LocalPoint(root, 0.40f * scale, height * 0.85f,
                                   0.28f * scale, turn),
                        (Vector3){0.66f, 0.66f, 0.60f}, turn + 0.36f, middle);
    DrawTreeFoliageMass(TREE_CROWN_ALDER, top,
                        (Vector3){0.68f, 0.74f, 0.62f}, turn, light);
}

static void DrawOakTree(Vector3 root, float turn, float shape, Color leaves)
{
    float scale = 0.92f + shape * 0.16f;
    float height = (5.15f + shape * 0.55f) * scale;
    float lean = (shape - 0.38f) * 0.72f;
    Color bark = BlendColor((Color){60, 43, 37, 255},
                            (Color){105, 67, 43, 255}, shape * 0.34f);
    Color deep = ShadeColor(leaves, 0.72f);
    Color middle = ShadeColor(leaves, 0.88f);
    Color light = ShadeColor(leaves, 1.02f);
    Vector3 fork = LocalPoint(root, lean * 0.52f, height * 0.38f,
                              0.0f, turn);
    Vector3 left = LocalPoint(root, -1.58f * scale, height * 0.68f,
                              0.12f * scale, turn);
    Vector3 right = LocalPoint(root, 1.64f * scale, height * 0.70f,
                               -0.18f * scale, turn);
    Vector3 upper = LocalPoint(root, lean + 0.18f * scale,
                               height * 0.90f, 0.05f * scale, turn);
    Vector3 dead_tip = LocalPoint(root, 1.52f * scale, height * 0.91f,
                                  -0.12f * scale, turn);

    DrawTreeShadow(root, turn, 2.10f * scale, 1.42f * scale);
    DrawTreeRootFlare(root, turn, 0.38f * scale, bark);
    DrawCylinderEx(root, fork, 0.43f * scale, 0.28f * scale, 7, bark);
    DrawCylinderEx(fork, left, 0.27f * scale, 0.075f * scale, 7,
                   ShadeColor(bark, 0.86f));
    DrawCylinderEx(fork, right, 0.25f * scale, 0.070f * scale, 7, bark);
    DrawCylinderEx(fork, upper, 0.24f * scale, 0.060f * scale, 7,
                   ShadeColor(bark, 0.94f));
    DrawCylinderEx(right, dead_tip, 0.065f * scale, 0.025f * scale, 5,
                   ShadeColor(bark, 1.08f));

    DrawTreeFoliageMass(TREE_CROWN_OAK, left,
                        (Vector3){1.20f, 0.68f, 0.94f}, turn + 0.18f, deep);
    DrawTreeFoliageMass(TREE_CROWN_OAK, right,
                        (Vector3){1.18f, 0.70f, 0.92f}, turn - 0.14f, middle);
    DrawTreeFoliageMass(TREE_CROWN_OAK,
                        LocalPoint(root, -0.72f * scale, height * 0.81f,
                                   -0.54f * scale, turn),
                        (Vector3){1.06f, 0.72f, 0.90f}, turn - 0.28f, middle);
    DrawTreeFoliageMass(TREE_CROWN_OAK,
                        LocalPoint(root, 0.54f * scale, height * 0.78f,
                                   0.58f * scale, turn),
                        (Vector3){1.10f, 0.70f, 0.86f}, turn + 0.30f, deep);
    DrawTreeFoliageMass(TREE_CROWN_OAK, upper,
                        (Vector3){1.15f, 0.76f, 0.94f}, turn + 0.04f, light);
    DrawTreeFoliageMass(TREE_CROWN_OAK,
                        LocalPoint(root, -1.78f * scale, height * 0.60f,
                                   -0.38f * scale, turn),
                        (Vector3){0.82f, 0.54f, 0.68f}, turn - 0.18f, deep);
    DrawTreeFoliageMass(TREE_CROWN_OAK,
                        LocalPoint(root, 1.82f * scale, height * 0.59f,
                                   0.22f * scale, turn),
                        (Vector3){0.84f, 0.56f, 0.70f}, turn + 0.23f, middle);
}

static void DrawPollardTree(Vector3 root, float turn, float shape,
                            Color leaves)
{
    float scale = 0.90f + shape * 0.16f;
    float height = (3.85f + shape * 0.45f) * scale;
    float head_height = height * 0.49f;
    Color bark = BlendColor((Color){62, 48, 40, 255},
                            (Color){111, 72, 45, 255}, shape * 0.30f);
    Color deep = ShadeColor(leaves, 0.78f);
    Color middle = ShadeColor(leaves, 0.94f);
    Color light = ShadeColor(leaves, 1.08f);
    Vector3 head = LocalPoint(root, (shape - 0.5f) * 0.18f,
                              head_height, 0.0f, turn);
    static const float stem_x[] = {-0.78f, -0.36f, 0.02f, 0.42f, 0.80f};
    static const float stem_z[] = {0.18f, -0.22f, 0.20f, -0.16f, 0.12f};
    static const float stem_height[] = {0.78f, 0.93f, 1.00f, 0.88f, 0.72f};

    DrawTreeShadow(root, turn, 1.34f * scale, 1.00f * scale);
    DrawTreeRootFlare(root, turn, 0.31f * scale, bark);
    DrawCylinderEx(root, head, 0.34f * scale, 0.27f * scale, 7, bark);
    DrawTreeFoliageMass(TREE_CROWN_POLLARD, head,
                        (Vector3){0.40f, 0.32f, 0.38f}, turn,
                        ShadeColor(bark, 0.82f));

    for (int32_t stem = 0; stem < 5; ++stem) {
        Vector3 tip = LocalPoint(root, stem_x[stem] * scale,
                                 height * stem_height[stem],
                                 stem_z[stem] * scale, turn);
        DrawCylinderEx(head, tip, 0.090f * scale, 0.030f * scale, 6,
                       ShadeColor(bark, 0.86f + (float)stem * 0.025f));
        Color stem_leaves = stem == 2 ? light :
                            (stem & 1) != 0 ? middle : deep;
        DrawTreeFoliageMass(TREE_CROWN_POLLARD, tip,
                            (Vector3){0.50f, 0.72f, 0.46f},
                            turn + stem_x[stem] * 0.32f, stem_leaves);
    }
}

static void DrawTree(float x, float z, TreeFamily family, Color leaves,
                     TreeRegionalStyle regional_style)
{
    float base = CcLocalTerrainHeightAt(x, z);
    int32_t column = (int32_t)lroundf(x * 10.0f);
    int32_t row = (int32_t)lroundf(z * 10.0f);
    float shape = CombatClamp(TerrainScatter01(column, row, 71U) +
                              regional_style.shape_shift, 0.0f, 1.0f);
    float turn = TerrainScatter01(column, row, 72U) * PI * 2.0f;
    float turn_step = PI * 2.0f / regional_style.turn_steps;
    turn = floorf(turn / turn_step + 0.5f) * turn_step +
           (shape - 0.5f) * 0.16f;
    Vector3 root = {x, base + 0.018f, z};
    leaves = BlendColor(leaves, regional_style.foliage_bias,
                        regional_style.foliage_mix);

    /* Scale around the root, so regional calligraphy never changes terrain,
       collision, or authored placement contracts. */
    rlPushMatrix();
    rlTranslatef(root.x, root.y, root.z);
    rlScalef(regional_style.proportions.x,
             regional_style.proportions.y,
             regional_style.proportions.z);
    rlTranslatef(-root.x, -root.y, -root.z);

    switch (family) {
        case TREE_FAMILY_OAK:
            DrawOakTree(root, turn, shape, leaves);
            break;
        case TREE_FAMILY_POLLARD:
            DrawPollardTree(root, turn, shape, leaves);
            break;
        case TREE_FAMILY_ALDER:
        default:
            DrawAlderTree(root, turn, shape, leaves);
            break;
    }
    rlPopMatrix();
}

static void DrawWorldTrees(Vector3 focus, Color kingdom)
{
    TreeRegionalStyle regional_style = TreeStyleForKingdom(kingdom);
    int32_t room = StreetCameraBaseShot(street_camera_rig.shot);
    for (int32_t i = 0;
         i < (int32_t)(sizeof(WORLD_TREES) / sizeof(WORLD_TREES[0])); ++i) {
        /* The mine room uses rails as a leading line. Two otherwise sound
           trees landed directly on that fixed camera axis and erased both
           the rail head and mine entrance. The fade between room pages
           makes this authored thinning stable and invisible in motion. */
        if (room == 2 && (i == 4 || i == 25)) continue;
        Vector2 position = WORLD_TREES[i].position;
        position.x += (i & 1) != 0 ? -regional_style.cluster_pull :
                                     regional_style.cluster_pull;
        if (!SceneryPointVisible(position.x, position.y, focus)) continue;
        TreeFamily family = RegionalTreeFamily(WORLD_TREES[i].family, i,
                                               regional_style);
        Color leaves = (i & 1) != 0 ? WORLD_FOLIAGE :
                                      WORLD_FOLIAGE_LIGHT;
        if (family == TREE_FAMILY_OAK) {
            leaves = ShadeColor(leaves, 0.86f);
        } else if (family == TREE_FAMILY_POLLARD) {
            leaves = BlendColor(leaves, WORLD_GRASS_LIGHT, 0.18f);
        }
        DrawTree(position.x, position.y, family, leaves, regional_style);
    }
}

static Rectangle ViewportRectangle(Rectangle viewport, float x, float y,
                                   float width, float height)
{
    return (Rectangle){viewport.x + x, viewport.y + y, width, height};
}

static void DrawViewportText(const char *text, Rectangle viewport,
                             int32_t x, int32_t y, int32_t font_size,
                             Color color)
{
    CcOverlayDrawText(text, (int32_t)lroundf(viewport.x) + x,
             (int32_t)lroundf(viewport.y) + y, font_size, color);
}

static bool AgentNearLabel(const CcLocalAgent *agent, float x, float z,
                           float radius)
{
    if (agent == NULL) return false;
    float dx = agent->position.x - x;
    float dz = agent->position.z - z;
    return dx * dx + dz * dz <= radius * radius;
}

static void DrawStreetTraversalPortals(const CcLocalAgent *agent,
                                       Camera3D camera, Rectangle viewport,
                                       int32_t art_width,
                                       int32_t art_height)
{
    int32_t count = CcLocalAgentStreetPortalCount(agent);
    for (int32_t portal_index = 0; portal_index < count; ++portal_index) {
        ResolvedStreetPortal portal = {0};
        if (!ResolveStreetPortal(agent, portal_index, &portal)) continue;
        Vector3 approach = StreetPortalApproachWorldPoint(&portal);
        if (!AgentNearLabel(agent, approach.x, approach.z, 7.0f)) continue;
        Vector2 art_point = StreetPortalEdgePoint(
            &portal, camera, art_width, art_height);
        Vector2 point = {
            viewport.x + art_point.x / (float)art_width * viewport.width,
            viewport.y + art_point.y / (float)art_height * viewport.height,
        };
        const char *name = CcLocalAgentStreetPortalName(agent, portal_index);
        if (name == NULL) continue;
        char label[96];
        (void)snprintf(label, sizeof(label), "%s  %s",
                       portal.exit != NULL ? "LEAVE" : "TO", name);
        int32_t text_width = CcOverlayMeasureText(label, 9);
        float bubble_width = (float)text_width + 16.0f;
        float bubble_x = point.x < viewport.x + viewport.width * 0.5f ?
            point.x + 10.0f : point.x - bubble_width - 10.0f;
        float bubble_y = point.y - 10.0f;
        bubble_x = fmaxf(viewport.x + 5.0f,
                         fminf(bubble_x, viewport.x + viewport.width -
                                             bubble_width - 5.0f));
        bubble_y = fmaxf(viewport.y + 5.0f,
                         fminf(bubble_y, viewport.y + viewport.height -
                                             23.0f));
        Color accent = portal.exit != NULL ? WORLD_GOLD : WORLD_TEAL;
        DrawRectangleRounded(
            (Rectangle){bubble_x, bubble_y, bubble_width, 20.0f},
            0.28f, 4, Fade(CC_STYLE_PANEL_DEEP, 0.88f));
        DrawRectangleLinesEx(
            (Rectangle){bubble_x, bubble_y, bubble_width, 20.0f},
            1.0f, Fade(accent, 0.62f));
        DrawCircleV(point, 5.0f, accent);
        DrawCircleLines((int32_t)lroundf(point.x),
                        (int32_t)lroundf(point.y), 7.0f,
                        Fade(WORLD_INK, 0.82f));
        CcOverlayDrawText(label, (int32_t)lroundf(bubble_x) + 8,
                         (int32_t)lroundf(bubble_y) + 6, 9, accent);
    }
}

static void DrawLabels(const WorldLabel *labels, int32_t count, Camera3D camera,
                       Rectangle viewport)
{
    const float bubble_height = 16.0f;
    const float head_clearance = 8.0f;
    int32_t width = (int32_t)lroundf(viewport.width);
    int32_t height = (int32_t)lroundf(viewport.height);
    for (int32_t i = 0; i < count; ++i) {
        if (!CameraPointInFront(camera, labels[i].point)) continue;
        Vector2 screen = GetWorldToScreenEx(labels[i].point, camera, width, height);
        if (screen.x < -120.0f || screen.x > viewport.width + 120.0f ||
            screen.y < bubble_height + head_clearance + 4.0f ||
            screen.y > viewport.height + 40.0f) continue;
        int text_width = CcOverlayMeasureText(labels[i].text, 10);
        float bubble_width = (float)text_width + 10.0f;
        float bubble_x = viewport.x + screen.x - bubble_width * 0.5f;
        /* The projected point is the top of the subject. Keep the complete
           nameplate above it so the plate never paints across a face. */
        float bubble_y = viewport.y + screen.y - bubble_height -
                         head_clearance;
        bubble_x = fmaxf(viewport.x + 4.0f,
                         fminf(bubble_x,
                               viewport.x + viewport.width - bubble_width -
                                   4.0f));
        bubble_y = fmaxf(viewport.y + 4.0f,
                         fminf(bubble_y,
                               viewport.y + viewport.height - 20.0f));
        DrawRectangleRounded((Rectangle){bubble_x, bubble_y,
                                         bubble_width, bubble_height},
                             0.30f, 4, (Color){4, 10, 14, 210});
        CcOverlayDrawText(labels[i].text, (int)lroundf(bubble_x) + 5,
                 (int)lroundf(bubble_y) + 3, 10, labels[i].color);
    }
}

static void DrawCombatBar(const CcLocalAgent *agent, Camera3D camera,
                          Rectangle viewport, Color accent)
{
    if (agent == NULL) return;
    /* Keep combat state in the clear air above the portrait billboard. */
    Vector3 anchor = {agent->position.x, agent->position.y + 2.82f,
                      agent->position.z};
    if (!CameraPointInFront(camera, anchor)) return;
    int32_t width = (int32_t)lroundf(viewport.width);
    int32_t height = (int32_t)lroundf(viewport.height);
    Vector2 screen = GetWorldToScreenEx(
        anchor, camera, width, height);
    const float bar_width = 44.0f;
    if (screen.x < bar_width * 0.5f + 3.0f ||
        screen.x > (float)width - bar_width * 0.5f - 3.0f ||
        screen.y < 10.0f || screen.y > (float)height - 8.0f) return;
    screen.x += viewport.x;
    screen.y += viewport.y;
    float health = CombatClamp(agent->combat.health /
                               CC_LOCAL_COMBAT_MAX_HEALTH, 0.0f, 1.0f);
    float posture = CombatClamp(agent->combat.posture /
                                CC_LOCAL_COMBAT_MAX_POSTURE, 0.0f, 1.0f);
    DrawRectangle((int)(screen.x - bar_width * 0.5f), (int)screen.y,
                  (int)bar_width, 5, (Color){5, 11, 15, 220});
    DrawRectangle((int)(screen.x - bar_width * 0.5f), (int)screen.y,
                  (int)(bar_width * health), 3,
                  CombatIsDefeated(&agent->combat) ? WORLD_MUTED : accent);
    DrawRectangle((int)(screen.x - bar_width * 0.5f), (int)screen.y + 4,
                  (int)(bar_width * posture), 2, WORLD_GOLD);
    if (CombatIsDefeated(&agent->combat)) {
        int label_width = CcOverlayMeasureText("DEAD", 8);
        CcOverlayDrawText("DEAD", (int)screen.x - label_width / 2,
                 (int)screen.y - 9, 8, WORLD_DANGER);
    }
}

static void DrawPaintedTriangle(Vector3 a, Vector3 b, Vector3 c, Color color)
{
    DrawTriangle3D(a, b, c, color);
    DrawTriangle3D(c, b, a, color);
}

static void DrawGroundBrushStroke(Vector3 center, Vector3 along,
                                  float length, float width, Color color)
{
    along.y = 0.0f;
    along = PhysicsNormalizeOr(along, (Vector3){0.0f, 0.0f, 1.0f});
    Vector3 side = PhysicsNormalizeOr(
        PhysicsCross((Vector3){0.0f, 1.0f, 0.0f}, along),
        (Vector3){1.0f, 0.0f, 0.0f});
    Vector3 start = PhysicsAdd(center, PhysicsScale(along, -length * 0.52f));
    Vector3 end = PhysicsAdd(center, PhysicsScale(along, length * 0.48f));
    Vector3 a = PhysicsAdd(start, PhysicsScale(side, -width * 0.45f));
    Vector3 b = PhysicsAdd(start, PhysicsScale(side, width * 0.32f));
    Vector3 c = PhysicsAdd(end, PhysicsScale(side, width * 0.16f));
    Vector3 d = PhysicsAdd(end, PhysicsScale(side, -width * 0.23f));
    DrawPaintedTriangle(a, b, c, color);
    DrawPaintedTriangle(a, c, d, color);
}

static void DrawPaintedOverheadDiamond(Vector3 center, float radius,
                                       Color color)
{
    Vector3 top = {center.x, center.y + radius, center.z};
    Vector3 right = {center.x + radius * 0.72f, center.y, center.z};
    Vector3 bottom = {center.x, center.y - radius, center.z};
    Vector3 left = {center.x - radius * 0.72f, center.y, center.z};
    DrawPaintedTriangle(top, right, bottom, color);
    DrawPaintedTriangle(top, bottom, left, ShadeColor(color, 0.78f));
}

static void DrawCombatImpact(const CcLocalAgent *agent)
{
    if (agent->combat.hit_flash_seconds <= 0.0f) return;
    float pulse = 0.13f + agent->combat.hit_flash_seconds * 1.05f;
    Vector3 point = agent->combat.impact_valid ? agent->combat.impact_point :
        (Vector3){agent->position.x, agent->position.y + 1.02f,
                  agent->position.z};
    if (agent->combat.impact_valid) {
        Vector3 direction = PhysicsNormalizeOr(
            agent->combat.impact_direction,
            (Vector3){0.0f, 0.0f, 1.0f});
        Vector3 side = PhysicsNormalizeOr(
            PhysicsCross(direction, (Vector3){0.0f, 1.0f, 0.0f}),
            (Vector3){1.0f, 0.0f, 0.0f});
        float spark = pulse * 1.65f;
        Vector3 wedge_base = PhysicsAdd(
            point, PhysicsScale(direction, -pulse * 0.58f));
        Vector3 wedge_left = PhysicsAdd(
            PhysicsAdd(wedge_base, PhysicsScale(side, -pulse * 0.62f)),
            (Vector3){0.0f, -pulse * 0.26f, 0.0f});
        Vector3 wedge_right = PhysicsAdd(
            PhysicsAdd(wedge_base, PhysicsScale(side, pulse * 0.62f)),
            (Vector3){0.0f, pulse * 0.16f, 0.0f});
        Vector3 wedge_tip = PhysicsAdd(
            PhysicsAdd(point, PhysicsScale(direction, pulse * 1.52f)),
            (Vector3){0.0f, pulse * 0.28f, 0.0f});
        DrawPaintedTriangle(wedge_left, wedge_right, wedge_tip,
                            Fade(WORLD_GOLD, 0.84f));

        Vector3 slash_start = PhysicsAdd(
            PhysicsAdd(point, PhysicsScale(side, -spark)),
            (Vector3){0.0f, -pulse * 0.22f, 0.0f});
        Vector3 slash_end = PhysicsAdd(
            PhysicsAdd(point, PhysicsScale(side, spark * 0.86f)),
            (Vector3){0.0f, pulse * 0.48f, 0.0f});
        DrawCylinderEx(slash_start, slash_end, pulse * 0.12f,
                       pulse * 0.025f, 5, Fade(WORLD_INK, 0.78f));

        static const Vector3 spark_directions[] = {
            {-0.72f, 0.86f, 0.18f},
            {0.34f, 1.00f, -0.28f},
            {0.78f, 0.62f, 0.34f}
        };
        for (int32_t i = 0; i < 3; ++i) {
            Vector3 spark_end = PhysicsAdd(
                point, PhysicsScale(spark_directions[i], pulse * 1.22f));
            DrawCylinderEx(point, spark_end, pulse * 0.050f,
                           pulse * 0.012f, 5,
                           i == 1 ? WHITE : WORLD_GOLD);
        }

        Vector3 ground = {point.x, agent->position.y + 0.022f, point.z};
        DrawGroundBrushStroke(ground, direction, pulse * 2.8f,
                              pulse * 0.62f,
                              Fade((Color){69, 53, 43, 255}, 0.52f));
        DrawSmallSphere(point, pulse * 0.22f, WHITE);
    } else {
        DrawSmallSphere(point, pulse * 0.34f, WORLD_GOLD);
    }
}

static void DrawCombatSkillTell(const CcLocalAgent *agent)
{
    if (agent == NULL ||
        agent->humanoid.action != CC_HUMANOID_ACTION_STRIKE ||
        (agent->combat.active_skill != CC_COMBAT_SKILL_CRUSHING_BLOW &&
         agent->combat.active_skill != CC_COMBAT_SKILL_SUNDER)) {
        return;
    }
    float phase = CombatClamp(agent->humanoid.action_time / 1.20f,
                              0.0f, 1.0f);
    Color color = agent->combat.active_skill ==
                  CC_COMBAT_SKILL_CRUSHING_BLOW ? WORLD_GOLD : WORLD_VIOLET;
    float radius = 0.40f + sinf(phase * PI) * 0.22f;
    Vector3 ground = {agent->position.x, agent->position.y + 0.025f,
                      agent->position.z};
    Vector3 facing = {sinf(agent->facing_yaw), 0.0f,
                      cosf(agent->facing_yaw)};
    Vector3 side = {facing.z, 0.0f, -facing.x};
    Vector3 tip = PhysicsAdd(ground, PhysicsScale(facing, radius * 1.65f));
    Vector3 left = PhysicsAdd(
        PhysicsAdd(ground, PhysicsScale(facing, radius * 0.18f)),
        PhysicsScale(side, -radius * 0.54f));
    Vector3 right = PhysicsAdd(
        PhysicsAdd(ground, PhysicsScale(facing, radius * 0.18f)),
        PhysicsScale(side, radius * 0.54f));
    DrawPaintedTriangle(left, right, tip, Fade(color, 0.34f));
    DrawGroundBrushStroke(PhysicsAdd(ground, PhysicsScale(facing, radius)),
                          facing, radius * 1.28f, radius * 0.14f,
                          Fade(color, 0.74f));
}

static void DrawCombatFootprint(const CcLocalAgent *agent, Color color)
{
    if (agent == NULL || !CombatCanAct(&agent->combat) ||
        agent->humanoid.ragdoll.active) return;
    Vector3 ground = {agent->position.x, agent->position.y + 0.012f,
                      agent->position.z};
    DrawCylinder(ground, 0.31f, 0.31f, 0.012f, 20,
                 Fade(WORLD_VOID, 0.38f));
    Vector3 facing = {sinf(agent->facing_yaw), 0.0f,
                      cosf(agent->facing_yaw)};
    DrawGroundBrushStroke((Vector3){ground.x, ground.y + 0.006f, ground.z},
                          facing, 0.62f, 0.11f, Fade(color, 0.58f));
}

static void DrawCombatSword(const CcLocalAgent *agent)
{
    if (agent == NULL ||
        (agent->combat.team != CC_COMBAT_PLAYER &&
         agent->combat.team != CC_COMBAT_RAIDER) ||
        agent->combat.weapon_mode == CC_WEAPON_NONE ||
        agent->swimming || agent->climbing) {
        return;
    }
    const CcHumanoidPose *pose = AgentRenderPose(agent);
    Vector3 hand = FromLimbVector(pose->hand[1]);
    Vector3 direction;
    Vector3 right;
    if (agent->combat.weapon_mode == CC_WEAPON_RAGDOLL_ATTACHED) {
        Vector3 elbow = FromLimbVector(pose->elbow[1]);
        direction = PhysicsNormalizeOr(PhysicsSubtract(hand, elbow),
                                       (Vector3){0.0f, 0.0f, 1.0f});
        right = PhysicsNormalizeOr(
            PhysicsCross((Vector3){0.0f, 1.0f, 0.0f}, direction),
            (Vector3){1.0f, 0.0f, 0.0f});
    } else {
        direction = CombatWeaponDirectionAt(agent,
                                             agent->humanoid.action_time);
        right = (Vector3){cosf(agent->facing_yaw), 0.0f,
                          -sinf(agent->facing_yaw)};
    }
    float blade_length = agent->combat.team == CC_COMBAT_PLAYER ? 0.86f :
                                                                    0.70f;
    bool combat_ready = agent->combat.focus_valid ||
        agent->humanoid.action == CC_HUMANOID_ACTION_GUARD ||
        agent->humanoid.action == CC_HUMANOID_ACTION_STRIKE;
    bool quiet_player_blade = agent->combat.team == CC_COMBAT_PLAYER &&
                              !combat_ready;
    Vector3 guard_center = PhysicsAdd(hand, PhysicsScale(direction, 0.13f));
    Vector3 blade_start = PhysicsAdd(guard_center,
                                     PhysicsScale(direction, 0.035f));
    Vector3 blade_tip = PhysicsAdd(blade_start,
                                   PhysicsScale(direction, blade_length));
    Vector3 pommel = PhysicsAdd(hand, PhysicsScale(direction, -0.17f));
    Color grip = agent->combat.team == CC_COMBAT_PLAYER ?
        (Color){66, 45, 43, 255} : (Color){47, 39, 42, 255};
    Color guard_color = agent->combat.team == CC_COMBAT_PLAYER ?
        WORLD_GOLD : (Color){164, 116, 65, 255};
    Color steel = CombatIsDefeated(&agent->combat) ?
                  (Color){95, 101, 101, 255} :
                  quiet_player_blade ? (Color){118, 139, 139, 255} :
                  (Color){196, 211, 212, 255};

    DrawCylinderEx(pommel, guard_center, 0.032f, 0.032f, 7, grip);
    DrawSmallSphere(pommel, 0.045f, guard_color);
    DrawCylinderEx(PhysicsAdd(guard_center, PhysicsScale(right, -0.14f)),
                   PhysicsAdd(guard_center, PhysicsScale(right, 0.14f)),
                   0.032f, 0.026f, 7, guard_color);
    DrawCylinderEx(blade_start, blade_tip,
                   quiet_player_blade ? 0.034f : 0.052f,
                   0.012f, 5, steel);
    DrawLine3D(blade_start, blade_tip,
               agent->combat.team == CC_COMBAT_PLAYER ? WORLD_GOLD :
                                                        WORLD_INK);

    if (agent->combat.weapon_mode == CC_WEAPON_HELD &&
        agent->humanoid.action == CC_HUMANOID_ACTION_STRIKE &&
        agent->humanoid.action_time > 0.18f &&
        agent->humanoid.action_time < 0.84f) {
        Vector3 previous_direction = CombatWeaponDirectionAt(
            agent, fmaxf(0.0f, agent->humanoid.action_time - 0.075f));
        Vector3 previous_tip = PhysicsAdd(
            hand, PhysicsScale(previous_direction, blade_length + 0.16f));
        Color trail = agent->combat.team == CC_COMBAT_RAIDER ?
            Fade(WORLD_DANGER, 0.54f) :
            agent->combat.active_skill == CC_COMBAT_SKILL_CRUSHING_BLOW ?
                Fade(WORLD_GOLD, 0.72f) :
            agent->combat.active_skill == CC_COMBAT_SKILL_SUNDER ?
                Fade(WORLD_VIOLET, 0.68f) : Fade(WORLD_TEAL, 0.58f);
        DrawCylinderEx(previous_tip, blade_tip, 0.018f, 0.006f, 5, trail);
        DrawLine3D(previous_tip, blade_tip, trail);
        DrawLine3D(PhysicsLerp(previous_tip, hand, 0.34f),
                   PhysicsLerp(blade_tip, hand, 0.34f), Fade(trail, 0.56f));
    }
}

static void DrawSelectedTarget(const CcLocalAgent *target)
{
    if (target == NULL || !CombatCanAct(&target->combat)) return;
    float surface = target->position.y + 0.035f;
    Vector3 center = {target->position.x, surface, target->position.z};
    DrawGroundBrushStroke(
        PhysicsAdd(center, (Vector3){0.0f, 0.0f, -0.48f}),
        (Vector3){1.0f, 0.0f, 0.0f}, 0.34f, 0.085f, WORLD_GOLD);
    DrawGroundBrushStroke(
        PhysicsAdd(center, (Vector3){0.0f, 0.0f, 0.48f}),
        (Vector3){-1.0f, 0.0f, 0.0f}, 0.34f, 0.085f, WORLD_DANGER);
    DrawGroundBrushStroke(
        PhysicsAdd(center, (Vector3){-0.48f, 0.0f, 0.0f}),
        (Vector3){0.0f, 0.0f, 1.0f}, 0.34f, 0.085f, WORLD_GOLD);
    DrawGroundBrushStroke(
        PhysicsAdd(center, (Vector3){0.48f, 0.0f, 0.0f}),
        (Vector3){0.0f, 0.0f, -1.0f}, 0.34f, 0.085f, WORLD_DANGER);
    DrawPaintedOverheadDiamond(
        (Vector3){target->position.x, surface + 2.22f,
                  target->position.z},
        0.13f, WORLD_GOLD);
}

static void PresentTarget(RenderTexture2D target, Rectangle destination)
{
    Rectangle source = {0.0f, 0.0f, (float)target.texture.width,
                        -(float)target.texture.height};
    if (visual_style.grade_ready) {
        BeginShaderMode(visual_style.grade);
        SetShaderValueTexture(visual_style.grade,
                              visual_style.palette_lut_location,
                              visual_style.palette_lut);
        const ArtAtmosphereDefinition *atmosphere =
            &visual_style.presentation_atmosphere;
        SetShaderValue(visual_style.grade,
                       visual_style.grade_exposure_location,
                       &atmosphere->grade_exposure, SHADER_UNIFORM_FLOAT);
        SetShaderValue(visual_style.grade,
                       visual_style.grade_shadow_tone_location,
                       &atmosphere->grade_shadow_tone,
                       SHADER_UNIFORM_FLOAT);
        SetShaderValue(visual_style.grade,
                       visual_style.grade_highlight_tone_location,
                       &atmosphere->grade_highlight_tone,
                       SHADER_UNIFORM_FLOAT);
        SetShaderValue(visual_style.grade,
                       visual_style.grade_chroma_location,
                       &atmosphere->grade_chroma, SHADER_UNIFORM_FLOAT);
    }
    DrawTexturePro(target.texture, source, destination, (Vector2){0.0f, 0.0f},
                   0.0f, WHITE);
    if (visual_style.grade_ready) EndShaderMode();
}

static Color ArtLightBackground(ArtLightProfileId profile_id)
{
    if (profile_id < 0 || profile_id >= ART_LIGHT_PROFILE_COUNT) {
        profile_id = ART_LIGHT_CLEAR_MARKET;
    }
    ArtAtmosphereDefinition atmosphere = ArtAtmosphereForProfile(profile_id);
    return ArtAtmosphereMixColor(
        ART_LIGHT_PROFILES[profile_id].fog_color,
        atmosphere.fog_color, atmosphere.fog_influence);
}

static float ArtAtmosphereHash(uint32_t value)
{
    value ^= value >> 16U;
    value *= UINT32_C(0x7feb352d);
    value ^= value >> 15U;
    value *= UINT32_C(0x846ca68b);
    value ^= value >> 16U;
    return (float)(value & UINT32_C(0xffff)) / 65535.0f;
}

static Vector2 ArtAtmospherePoint(float x, float y, float scale_x,
                                  float scale_y, float drift_y)
{
    const float dragon_scale = 0.78f;
    float placed_x = 38.0f + (x - 68.0f) * dragon_scale;
    float placed_y = 8.0f + (y - 40.0f) * dragon_scale + drift_y;
    return (Vector2){placed_x * scale_x, placed_y * scale_y};
}

static void DrawDragonOmenSilhouette(int32_t width, int32_t height,
                                     float clock, float omen)
{
    if (omen <= 0.01f) return;
    float scale_x = (float)width / 630.0f;
    float scale_y = (float)height / 320.0f;
    float drift_y = sinf(clock * 0.08f) * 1.8f;
    Color shadow = Fade(CC_VISUAL_PALETTE.cool_ink, omen * 0.60f);
    Color thin_shadow = Fade(CC_VISUAL_PALETTE.violet.shadow, omen * 0.46f);

    /* Long tail and the two wings sit behind the body. Broad triangles keep
       the omen readable at the fixed art-pixel scale; small notches at the
       trailing edges suggest a dragon without tracing a detailed mascot. */
    DrawTriangle(ArtAtmospherePoint(278, 72, scale_x, scale_y, drift_y),
                 ArtAtmospherePoint(474, 54, scale_x, scale_y, drift_y),
                 ArtAtmospherePoint(330, 86, scale_x, scale_y, drift_y),
                 thin_shadow);
    DrawTriangle(ArtAtmospherePoint(246, 72, scale_x, scale_y, drift_y),
                 ArtAtmospherePoint(150, 10, scale_x, scale_y, drift_y),
                 ArtAtmospherePoint(78, 18, scale_x, scale_y, drift_y),
                 shadow);
    DrawTriangle(ArtAtmospherePoint(244, 72, scale_x, scale_y, drift_y),
                 ArtAtmospherePoint(78, 18, scale_x, scale_y, drift_y),
                 ArtAtmospherePoint(174, 70, scale_x, scale_y, drift_y),
                 shadow);
    DrawTriangle(ArtAtmospherePoint(272, 72, scale_x, scale_y, drift_y),
                 ArtAtmospherePoint(365, 23, scale_x, scale_y, drift_y),
                 ArtAtmospherePoint(455, 38, scale_x, scale_y, drift_y),
                 shadow);
    DrawTriangle(ArtAtmospherePoint(272, 72, scale_x, scale_y, drift_y),
                 ArtAtmospherePoint(455, 38, scale_x, scale_y, drift_y),
                 ArtAtmospherePoint(330, 81, scale_x, scale_y, drift_y),
                 shadow);
    DrawTriangle(ArtAtmospherePoint(168, 61, scale_x, scale_y, drift_y),
                 ArtAtmospherePoint(130, 84, scale_x, scale_y, drift_y),
                 ArtAtmospherePoint(215, 82, scale_x, scale_y, drift_y),
                 shadow);

    DrawEllipse((int32_t)((38.0f + (250.0f - 68.0f) * 0.78f) * scale_x),
                (int32_t)((8.0f + (73.0f - 40.0f) * 0.78f + drift_y) *
                          scale_y),
                67.0f * scale_x, 12.0f * scale_y, shadow);
    DrawTriangle(ArtAtmospherePoint(185, 66, scale_x, scale_y, drift_y),
                 ArtAtmospherePoint(112, 60, scale_x, scale_y, drift_y),
                 ArtAtmospherePoint(170, 84, scale_x, scale_y, drift_y),
                 shadow);
    DrawEllipse((int32_t)((38.0f + (108.0f - 68.0f) * 0.78f) * scale_x),
                (int32_t)((8.0f + (68.0f - 40.0f) * 0.78f + drift_y) *
                          scale_y),
                21.0f * scale_x, 11.0f * scale_y, shadow);
    DrawRectangle((int32_t)(38.0f * scale_x),
                  (int32_t)((8.0f + (66.0f - 40.0f) * 0.78f + drift_y) *
                            scale_y),
                  (int32_t)(30.0f * scale_x),
                  (int32_t)(9.0f * scale_y), shadow);

    DrawTriangle(ArtAtmospherePoint(95, 58, scale_x, scale_y, drift_y),
                 ArtAtmospherePoint(82, 42, scale_x, scale_y, drift_y),
                 ArtAtmospherePoint(108, 59, scale_x, scale_y, drift_y),
                 shadow);
    DrawTriangle(ArtAtmospherePoint(112, 58, scale_x, scale_y, drift_y),
                 ArtAtmospherePoint(110, 40, scale_x, scale_y, drift_y),
                 ArtAtmospherePoint(123, 61, scale_x, scale_y, drift_y),
                 shadow);
    DrawTriangle(ArtAtmospherePoint(83, 76, scale_x, scale_y, drift_y),
                 ArtAtmospherePoint(71, 86, scale_x, scale_y, drift_y),
                 ArtAtmospherePoint(104, 77, scale_x, scale_y, drift_y),
                 thin_shadow);

    DrawTriangle(ArtAtmospherePoint(222, 83, scale_x, scale_y, drift_y),
                 ArtAtmospherePoint(206, 112, scale_x, scale_y, drift_y),
                 ArtAtmospherePoint(240, 86, scale_x, scale_y, drift_y),
                 thin_shadow);
    DrawTriangle(ArtAtmospherePoint(282, 82, scale_x, scale_y, drift_y),
                 ArtAtmospherePoint(306, 108, scale_x, scale_y, drift_y),
                 ArtAtmospherePoint(298, 80, scale_x, scale_y, drift_y),
                 thin_shadow);
}

static void DrawTargetAtmosphere(RenderTexture2D target, float clock)
{
    ArtAtmosphereDefinition atmosphere =
        visual_style.presentation_atmosphere;
    int32_t width = target.texture.width;
    int32_t height = target.texture.height;

    if (atmosphere.omen > 0.01f) {
        Color cloud = Fade(CC_VISUAL_PALETTE.stone.shadow,
                           atmosphere.omen * 0.20f);
        Color clear_cloud = Fade(cloud, 0.0f);
        Color horizon = Fade(CC_VISUAL_PALETTE.earth.light,
                             atmosphere.omen * 0.13f);
        Color cloud_break = Fade(CC_VISUAL_PALETTE.stone.light,
                                 atmosphere.omen * 0.16f);
        DrawRectangleGradientV(0, 0, width, height / 2,
                               cloud, clear_cloud);
        DrawRectangleGradientV(0, height / 3, width, height / 5,
                               Fade(horizon, 0.0f), horizon);
        DrawRectangleGradientV(0, height / 3 + height / 5,
                               width, height / 7,
                               horizon, Fade(horizon, 0.0f));
        DrawEllipse(width / 5, height / 8,
                    (float)width * 0.24f, (float)height * 0.10f, cloud);
        DrawEllipse(width / 2, height / 10,
                    (float)width * 0.31f, (float)height * 0.09f, cloud);
        DrawEllipse(width * 4 / 5, height / 7,
                    (float)width * 0.25f, (float)height * 0.11f, cloud);
        DrawEllipse((int32_t)((float)width * 0.30f),
                    (int32_t)((float)height * 0.12f),
                    (float)width * 0.29f, (float)height * 0.12f,
                    cloud_break);
        DrawDragonOmenSilhouette(width, height, clock, atmosphere.omen);
    }

    /* Mist uses a few broad, slow bands. It is depth atmosphere, not a noisy
       smoke texture, and the final palette lookup resolves it back to the
       authored slate and violet families. */
    if (atmosphere.mist > 0.01f) {
        Color fog = atmosphere.fog_color;
        for (int32_t band = 0; band < 3; ++band) {
            float phase = clock * (0.035f + (float)band * 0.009f) +
                          (float)band * 1.83f;
            int32_t y = (int32_t)((0.30f + (float)band * 0.19f) *
                                  (float)height + sinf(phase) * 7.0f);
            int32_t band_height = 18 + band * 5;
            float opacity = atmosphere.mist *
                            (0.055f - (float)band * 0.009f);
            Color clear = Fade(fog, 0.0f);
            Color visible = Fade(fog, opacity);
            DrawRectangleGradientH(-12, y, width / 2 + 18, band_height,
                                   clear, visible);
            DrawRectangleGradientH(width / 2, y, width / 2 + 18,
                                   band_height, visible, clear);
        }
    }

    /* Rain is drawn on the fixed art-pixel target so every drop keeps the
       same chunky shape after enlargement. Movement is steady and diagonal;
       there is no random per-frame sparkle. */
    int32_t drop_count = (int32_t)roundf(atmosphere.rain * 88.0f);
    for (int32_t drop = 0; drop < drop_count; ++drop) {
        float seed_x = ArtAtmosphereHash(
            UINT32_C(0x91e10da5) + (uint32_t)drop * UINT32_C(0x9e3779b9));
        float seed_y = ArtAtmosphereHash(
            UINT32_C(0x68bc21eb) + (uint32_t)drop * UINT32_C(0x85ebca6b));
        float speed = 74.0f + ArtAtmosphereHash(
            UINT32_C(0x27d4eb2d) + (uint32_t)drop * UINT32_C(0xc2b2ae35)) *
            38.0f;
        float x = fmodf(seed_x * (float)(width + 36) + clock * 18.0f,
                        (float)(width + 36)) - 18.0f;
        float y = fmodf(seed_y * (float)(height + 24) + clock * speed,
                        (float)(height + 24)) - 12.0f;
        float opacity = atmosphere.rain * (0.20f + seed_y * 0.14f);
        Vector2 start = {x, y};
        Vector2 end = {x - 2.4f, y + 7.0f + seed_x * 3.0f};
        DrawLineEx(start, end, seed_x > 0.74f ? 1.4f : 1.0f,
                   Fade(WORLD_METAL_LIGHT, opacity));
    }
}

static ArtLightProfileId StreetLightProfile(
    const CcSettlement *place, ArtLightProfileId authored)
{
    if (place != NULL && place->hunger >= 30) {
        return ART_LIGHT_SHORTAGE_OVERCAST;
    }
    if (place != NULL && place->prosperity >= 60) {
        return ART_LIGHT_RECOVERY_WARM;
    }
    return authored;
}

static void BeginWorldLighting(Camera3D camera,
                               const ArtComposition *composition)
{
    if (!visual_style.world_ready || composition == NULL) return;
    ArtLightProfileId profile_id = composition->light_profile;
    if (profile_id < 0 || profile_id >= ART_LIGHT_PROFILE_COUNT) {
        profile_id = ART_LIGHT_CLEAR_MARKET;
    }
    const ArtLightProfileDefinition *profile =
        &ART_LIGHT_PROFILES[profile_id];
    ArtAtmosphereDefinition atmosphere = ArtAtmosphereForProfile(profile_id);
    visual_style.presentation_atmosphere = atmosphere;
    Vector3 light_direction = Vector3Normalize(
        ArtAtmosphereMixVector(profile->light_direction,
                               atmosphere.light_direction,
                               atmosphere.direction_influence));
    Vector3 camera_forward_vector = Vector3Normalize(
        Vector3Subtract(camera.target, camera.position));
    float direction[3] = {light_direction.x, light_direction.y,
                          light_direction.z};
    float light_color[3] = {
        profile->light_color.x * atmosphere.light_tint.x,
        profile->light_color.y * atmosphere.light_tint.y,
        profile->light_color.z * atmosphere.light_tint.z,
    };
    float ambient_color[3] = {
        profile->ambient_color.x * atmosphere.ambient_tint.x,
        profile->ambient_color.y * atmosphere.ambient_tint.y,
        profile->ambient_color.z * atmosphere.ambient_tint.z,
    };
    float shadow_color[3] = {
        profile->shadow_color.x * atmosphere.shadow_tint.x,
        profile->shadow_color.y * atmosphere.shadow_tint.y,
        profile->shadow_color.z * atmosphere.shadow_tint.z,
    };
    float camera_position[3] = {camera.position.x, camera.position.y,
                                camera.position.z};
    float camera_forward[3] = {camera_forward_vector.x,
                               camera_forward_vector.y,
                               camera_forward_vector.z};
    Color atmosphere_fog = ArtAtmosphereMixColor(
        profile->fog_color, atmosphere.fog_color,
        atmosphere.fog_influence);
    float fog_color[3] = {(float)atmosphere_fog.r / 255.0f,
                          (float)atmosphere_fog.g / 255.0f,
                          (float)atmosphere_fog.b / 255.0f};
    float fog_near = profile->fog_near * atmosphere.fog_distance_scale;
    float fog_far = profile->fog_far * atmosphere.fog_distance_scale;
    float depth_strength = profile->depth_strength * atmosphere.depth_scale;
    float focal_contrast = profile->focal_contrast * atmosphere.focal_scale;
    float focal_point[3] = {composition->focal_point.x,
                            composition->focal_point.y,
                            composition->focal_point.z};
    float story_axis[2] = {composition->story_axis.x,
                           composition->story_axis.y};
    float foreground_anchor[3] = {composition->foreground_anchor.x,
                                  composition->foreground_anchor.y,
                                  composition->foreground_anchor.z};
    float depth_splits[3] = {composition->depth_splits.x,
                             composition->depth_splits.y,
                             composition->depth_splits.z};
    SetShaderValue(visual_style.world,
                   visual_style.light_direction_location, direction,
                   SHADER_UNIFORM_VEC3);
    SetShaderValue(visual_style.world, visual_style.light_color_location,
                   light_color, SHADER_UNIFORM_VEC3);
    SetShaderValue(visual_style.world, visual_style.ambient_color_location,
                   ambient_color, SHADER_UNIFORM_VEC3);
    SetShaderValue(visual_style.world,
                   visual_style.camera_position_location, camera_position,
                   SHADER_UNIFORM_VEC3);
    SetShaderValue(visual_style.world,
                   visual_style.camera_forward_location, camera_forward,
                   SHADER_UNIFORM_VEC3);
    SetShaderValue(visual_style.world, visual_style.shadow_color_location,
                   shadow_color, SHADER_UNIFORM_VEC3);
    SetShaderValue(visual_style.world, visual_style.fog_color_location,
                   fog_color, SHADER_UNIFORM_VEC3);
    SetShaderValue(visual_style.world, visual_style.fog_near_location,
                   &fog_near, SHADER_UNIFORM_FLOAT);
    SetShaderValue(visual_style.world, visual_style.fog_far_location,
                   &fog_far, SHADER_UNIFORM_FLOAT);
    SetShaderValue(visual_style.world, visual_style.focal_point_location,
                   focal_point, SHADER_UNIFORM_VEC3);
    SetShaderValue(visual_style.world, visual_style.story_axis_location,
                   story_axis, SHADER_UNIFORM_VEC2);
    SetShaderValue(visual_style.world,
                   visual_style.foreground_anchor_location,
                   foreground_anchor, SHADER_UNIFORM_VEC3);
    SetShaderValue(visual_style.world, visual_style.depth_splits_location,
                   depth_splits, SHADER_UNIFORM_VEC3);
    SetShaderValue(visual_style.world, visual_style.depth_strength_location,
                   &depth_strength, SHADER_UNIFORM_FLOAT);
    SetShaderValue(visual_style.world, visual_style.focal_contrast_location,
                   &focal_contrast, SHADER_UNIFORM_FLOAT);
    SetShaderValue(visual_style.world,
                   visual_style.weather_wetness_location,
                   &atmosphere.wetness, SHADER_UNIFORM_FLOAT);
    if (visual_style.painted_environment.ready) {
        PaintedEnvironmentStyle *painted =
            &visual_style.painted_environment;
        SetShaderValue(painted->shader, painted->light_direction_location,
                       direction, SHADER_UNIFORM_VEC3);
        SetShaderValue(painted->shader, painted->light_color_location,
                       light_color, SHADER_UNIFORM_VEC3);
        SetShaderValue(painted->shader, painted->ambient_color_location,
                       ambient_color, SHADER_UNIFORM_VEC3);
        SetShaderValue(painted->shader, painted->camera_position_location,
                       camera_position, SHADER_UNIFORM_VEC3);
        SetShaderValue(painted->shader, painted->camera_forward_location,
                       camera_forward, SHADER_UNIFORM_VEC3);
        SetShaderValue(painted->shader, painted->shadow_color_location,
                       shadow_color, SHADER_UNIFORM_VEC3);
        SetShaderValue(painted->shader, painted->fog_color_location,
                       fog_color, SHADER_UNIFORM_VEC3);
        SetShaderValue(painted->shader, painted->fog_near_location,
                       &fog_near, SHADER_UNIFORM_FLOAT);
        SetShaderValue(painted->shader, painted->fog_far_location,
                       &fog_far, SHADER_UNIFORM_FLOAT);
        SetShaderValue(painted->shader, painted->focal_point_location,
                       focal_point, SHADER_UNIFORM_VEC3);
        SetShaderValue(painted->shader, painted->story_axis_location,
                       story_axis, SHADER_UNIFORM_VEC2);
        SetShaderValue(painted->shader, painted->foreground_anchor_location,
                       foreground_anchor, SHADER_UNIFORM_VEC3);
        SetShaderValue(painted->shader, painted->depth_splits_location,
                       depth_splits, SHADER_UNIFORM_VEC3);
        SetShaderValue(painted->shader, painted->depth_strength_location,
                       &depth_strength, SHADER_UNIFORM_FLOAT);
        SetShaderValue(painted->shader, painted->focal_contrast_location,
                       &focal_contrast, SHADER_UNIFORM_FLOAT);
    }
    const float foreground_reveal = 0.0f;
    SetShaderValue(visual_style.world,
                   visual_style.foreground_reveal_location,
                   &foreground_reveal, SHADER_UNIFORM_FLOAT);
    if (visual_style.painted_environment.ready) {
        SetShaderValue(
            visual_style.painted_environment.shader,
            visual_style.painted_environment.foreground_reveal_location,
            &foreground_reveal, SHADER_UNIFORM_FLOAT);
    }
    const float terrain_surface = 0.0f;
    SetShaderValue(visual_style.world,
                   visual_style.terrain_surface_location,
                   &terrain_surface, SHADER_UNIFORM_FLOAT);
    if (visual_style.foliage_ready) {
        SetShaderValue(visual_style.foliage,
                       visual_style.foliage_light_direction_location,
                       direction, SHADER_UNIFORM_VEC3);
        SetShaderValue(visual_style.foliage,
                       visual_style.foliage_light_color_location,
                       light_color, SHADER_UNIFORM_VEC3);
        SetShaderValue(visual_style.foliage,
                       visual_style.foliage_ambient_color_location,
                       ambient_color, SHADER_UNIFORM_VEC3);
        SetShaderValue(visual_style.foliage,
                       visual_style.foliage_camera_position_location,
                       camera_position, SHADER_UNIFORM_VEC3);
        SetShaderValue(visual_style.foliage,
                       visual_style.foliage_camera_forward_location,
                       camera_forward, SHADER_UNIFORM_VEC3);
        SetShaderValue(visual_style.foliage,
                       visual_style.foliage_shadow_color_location,
                       shadow_color, SHADER_UNIFORM_VEC3);
        SetShaderValue(visual_style.foliage,
                       visual_style.foliage_fog_color_location,
                       fog_color, SHADER_UNIFORM_VEC3);
        SetShaderValue(visual_style.foliage,
                       visual_style.foliage_fog_near_location,
                       &fog_near, SHADER_UNIFORM_FLOAT);
        SetShaderValue(visual_style.foliage,
                       visual_style.foliage_fog_far_location,
                       &fog_far, SHADER_UNIFORM_FLOAT);
        SetShaderValue(visual_style.foliage,
                       visual_style.foliage_depth_splits_location,
                       depth_splits, SHADER_UNIFORM_VEC3);
        SetShaderValue(visual_style.foliage,
                       visual_style.foliage_depth_strength_location,
                       &depth_strength, SHADER_UNIFORM_FLOAT);
    }
    if (visual_style.hero_ready) {
        SetShaderValue(visual_style.hero,
                       visual_style.hero_light_direction_location, direction,
                       SHADER_UNIFORM_VEC3);
        SetShaderValue(visual_style.hero,
                       visual_style.hero_camera_position_location,
                       camera_position, SHADER_UNIFORM_VEC3);
        SetShaderValue(visual_style.hero,
                       visual_style.hero_shadow_color_location,
                       shadow_color, SHADER_UNIFORM_VEC3);
        SetShaderValue(visual_style.hero,
                       visual_style.hero_fog_color_location, fog_color,
                       SHADER_UNIFORM_VEC3);
        SetShaderValue(visual_style.hero,
                       visual_style.hero_fog_near_location,
                       &fog_near,
                       SHADER_UNIFORM_FLOAT);
        SetShaderValue(visual_style.hero,
                       visual_style.hero_fog_far_location,
                       &fog_far,
                       SHADER_UNIFORM_FLOAT);
    }
    if (visual_style.npc_ready) {
        SetShaderValue(visual_style.npc,
                       visual_style.npc_light_direction_location, direction,
                       SHADER_UNIFORM_VEC3);
        SetShaderValue(visual_style.npc,
                       visual_style.npc_camera_position_location,
                       camera_position, SHADER_UNIFORM_VEC3);
        SetShaderValue(visual_style.npc,
                       visual_style.npc_shadow_color_location,
                       shadow_color, SHADER_UNIFORM_VEC3);
        SetShaderValue(visual_style.npc,
                       visual_style.npc_fog_color_location, fog_color,
                       SHADER_UNIFORM_VEC3);
        SetShaderValue(visual_style.npc,
                       visual_style.npc_fog_near_location,
                       &fog_near,
                       SHADER_UNIFORM_FLOAT);
        SetShaderValue(visual_style.npc,
                       visual_style.npc_fog_far_location,
                       &fog_far,
                       SHADER_UNIFORM_FLOAT);
    }
    if (visual_style.npc_skinned_ready) {
        SetShaderValue(visual_style.npc_skinned,
                       visual_style.npc_skinned_light_direction_location,
                       direction, SHADER_UNIFORM_VEC3);
        SetShaderValue(visual_style.npc_skinned,
                       visual_style.npc_skinned_camera_position_location,
                       camera_position, SHADER_UNIFORM_VEC3);
        SetShaderValue(visual_style.npc_skinned,
                       visual_style.npc_skinned_shadow_color_location,
                       shadow_color, SHADER_UNIFORM_VEC3);
        SetShaderValue(visual_style.npc_skinned,
                       visual_style.npc_skinned_fog_color_location,
                       fog_color, SHADER_UNIFORM_VEC3);
        SetShaderValue(visual_style.npc_skinned,
                       visual_style.npc_skinned_fog_near_location,
                       &fog_near, SHADER_UNIFORM_FLOAT);
        SetShaderValue(visual_style.npc_skinned,
                       visual_style.npc_skinned_fog_far_location,
                       &fog_far, SHADER_UNIFORM_FLOAT);
    }
    BeginShaderMode(visual_style.world);
}

static void EndWorldLighting(void)
{
    if (visual_style.world_ready) EndShaderMode();
}

static void PresentCharacterPortrait(Rectangle bounds, Color accent)
{
    DrawRectangle((int32_t)bounds.x, (int32_t)bounds.y,
                  (int32_t)bounds.width, (int32_t)bounds.height,
                  (Color){8, 16, 21, 255});
    Rectangle source = {0.0f, 0.0f,
                        (float)npc_portrait_target.texture.width,
                        -(float)npc_portrait_target.texture.height};
    float available_width = fmaxf(1.0f, bounds.width - 4.0f);
    float available_height = fmaxf(1.0f, bounds.height - 4.0f);
    float source_aspect = (float)npc_portrait_target.texture.width /
                          (float)npc_portrait_target.texture.height;
    float draw_width = fminf(available_width,
                             available_height * source_aspect);
    float draw_height = fminf(available_height,
                              available_width / source_aspect);
    Rectangle inset = {
        bounds.x + (bounds.width - draw_width) * 0.5f,
        bounds.y + (bounds.height - draw_height) * 0.5f,
        draw_width, draw_height,
    };
    DrawTexturePro(npc_portrait_target.texture, source, inset,
                   (Vector2){0.0f, 0.0f}, 0.0f, WHITE);
    DrawRectangleLines((int32_t)bounds.x, (int32_t)bounds.y,
                       (int32_t)bounds.width, (int32_t)bounds.height,
                       ShadeColor(accent, 0.88f));
}

void CcLocalDrawNpcPortrait3D(const CcNpcAppearance *appearance,
                              Rectangle bounds,
                              CcNpcPortraitExpression expression)
{
    if (appearance == NULL || bounds.width < 4.0f || bounds.height < 4.0f ||
        !IsRenderTextureValid(npc_portrait_target)) {
        CcNpcDrawPixelPortrait(appearance, bounds, expression, false);
        return;
    }

    FaceRenderContext previous_face_context = face_render_context;
    Camera3D camera = {0};
    camera.target = (Vector3){0.0f, 1.66f, 0.0f};
    camera.position = (Vector3){0.0f, 1.77f, 4.0f};
    camera.up = (Vector3){0.0f, 1.0f, 0.0f};
    camera.fovy = 0.98f;
    camera.projection = CAMERA_ORTHOGRAPHIC;

    rlDrawRenderBatchActive();
    BeginTextureMode(npc_portrait_target);
    ClearBackground((Color){17, 28, 32, 255});
    SetFaceRenderContext(camera, npc_portrait_target.texture.width,
                         npc_portrait_target.texture.height);
    BeginMode3D(camera);
    BeginWorldLighting(camera, &INTERIOR_ART_COMPOSITION);
    (void)DrawNpcArchetype3D((Vector3){0.0f, 0.0f, 0.0f}, 1.0f,
                             0.0f, 0.0f, CC_TRAVERSAL_IDLE,
                             expression, appearance);
    EndWorldLighting();
    EndMode3D();
    EndTextureMode();
    face_render_context = previous_face_context;

    PresentCharacterPortrait(bounds, appearance->accent);
}

void CcLocalDrawAgentPortrait3D(const CcLocalAgent *agent,
                                Rectangle bounds)
{
    if (agent == NULL || bounds.width < 4.0f || bounds.height < 4.0f ||
        !IsRenderTextureValid(npc_portrait_target)) return;
    const CcHumanoidPose *pose = AgentRenderPose(agent);
    CcHumanoidSkinPose skin;
    CcHumanoidSkinPoseResolve(pose, &skin);
    if (!skin.valid) return;

    const CcHumanoidSkinBonePose *head_bone =
        &skin.bones[CC_HUMANOID_SKIN_HEAD];
    Vector3 head = FromLimbVector(
        skin.sockets[CC_HUMANOID_SOCKET_HEAD].position);
    Vector3 head_up = PhysicsNormalizeOr(FromLimbVector(head_bone->up),
                                         (Vector3){0.0f, 1.0f, 0.0f});
    Vector3 head_forward = PhysicsNormalizeOr(
        FromLimbVector(head_bone->forward),
        (Vector3){sinf(agent->facing_yaw), 0.0f,
                  cosf(agent->facing_yaw)});
    Camera3D camera = {0};
    float portrait_drop = agent->crowned ? -0.10f : -0.16f;
    camera.target = Vector3Add(head, Vector3Scale(head_up, portrait_drop));
    camera.position = Vector3Add(
        camera.target,
        Vector3Add(Vector3Scale(head_forward, 4.0f),
                   (Vector3){0.0f, 0.11f, 0.0f}));
    camera.up = (Vector3){0.0f, 1.0f, 0.0f};
    /* The hero PFP is an identity card, so favor the face and shoulder marks
       over empty torso space. The larger face also keeps one-cell eyes and
       mouth intact when the portrait target is reduced into the HUD. */
    camera.fovy = agent->crowned ? 0.84f : 0.98f;
    camera.projection = CAMERA_ORTHOGRAPHIC;

    FaceRenderContext previous_face_context = face_render_context;
    rlDrawRenderBatchActive();
    BeginTextureMode(npc_portrait_target);
    ClearBackground((Color){17, 28, 32, 255});
    SetFaceRenderContext(camera, npc_portrait_target.texture.width,
                         npc_portrait_target.texture.height);
    BeginMode3D(camera);
    BeginWorldLighting(camera, &INTERIOR_ART_COMPOSITION);

    bool drew = false;
    CcNpcAppearance portrait_appearance = agent->appearance;
    if (agent->crowned && screen_first_hero_active) {
        UseCharacterLighting();
        drew = DrawDynamicNpcModules(agent, &skin, &portrait_appearance);
        if (drew) {
            DrawWayfarerHeroDetails(&skin);
        }
        RestoreWorldLighting();
    } else if (agent->crowned) {
        drew = DrawHeroSkin(&skin, &agent->render_cape, WHITE, true, false);
        if (drew) {
            UseCharacterLighting();
            DrawWayfarerHeroDetails(&skin);
            RestoreWorldLighting();
        }
    } else {
        UseCharacterLighting();
        drew = DrawDynamicNpcModules(agent, &skin, &portrait_appearance);
        RestoreWorldLighting();
    }
    EndWorldLighting();
    EndMode3D();
    EndTextureMode();
    face_render_context = previous_face_context;

    if (drew) {
        PresentCharacterPortrait(bounds, portrait_appearance.accent);
    } else {
        CcNpcDrawPixelPortrait(&portrait_appearance, bounds,
                               CC_NPC_PORTRAIT_NEUTRAL, false);
    }
}

static void DrawAgentPath(const CcLocalAgent *agent, bool market_interior)
{
    (void)market_interior;
    if (!agent->command_point_valid ||
        (!agent->exact_target_valid && !agent->navigation_active)) return;
    Vector3 target = agent->command_point;
    DrawCylinder((Vector3){target.x, target.y + 0.018f, target.z},
                 0.24f, 0.24f, 0.036f, 24, Fade(WORLD_GOLD, 0.42f));
    Vector3 facing = PhysicsSubtract(target, agent->position);
    DrawGroundBrushStroke(
        (Vector3){target.x, target.y + 0.040f, target.z}, facing,
        0.52f, 0.085f, WORLD_GOLD);
}

static Color CoursePlatformColor(int32_t style)
{
    switch (style) {
        case 1: return (Color){87, 113, 105, 255};
        case 2: return (Color){98, 79, 118, 255};
        case 3: return (Color){126, 82, 74, 255};
        default: return (Color){96, 121, 111, 255};
    }
}

static void DrawObstacleCourse(Vector3 focus)
{
    if (!SceneryFootprintVisible(COURSE_SCENERY_FOOTPRINT, focus)) return;
    float pool_center_x = COURSE_POOL.x + COURSE_POOL.width * 0.5f;
    float pool_center_z = COURSE_POOL.y + COURSE_POOL.height * 0.5f;
    DrawBox((Vector3){pool_center_x, 0.36f, pool_center_z},
            (Vector3){COURSE_POOL.width, 0.72f, COURSE_POOL.height},
            (Color){19, 57, 73, 255});
    DrawBox((Vector3){pool_center_x, COURSE_WATER_SURFACE, pool_center_z},
            (Vector3){COURSE_POOL.width - 0.10f, 0.035f,
                      COURSE_POOL.height - 0.10f},
            (Color){48, 151, 167, 205});
    DrawBox((Vector3){pool_center_x, 0.43f, COURSE_POOL.y - 0.06f},
            (Vector3){COURSE_POOL.width + 0.18f, 0.86f, 0.12f},
            (Color){86, 104, 102, 255});
    DrawBox((Vector3){pool_center_x, 0.43f,
                      COURSE_POOL.y + COURSE_POOL.height + 0.06f},
            (Vector3){COURSE_POOL.width + 0.18f, 0.86f, 0.12f},
            (Color){86, 104, 102, 255});
    for (int32_t i = 0; i < (int32_t)(sizeof(COURSE_WAYPOINTS) /
                                      sizeof(COURSE_WAYPOINTS[0])); ++i) {
        Vector3 point = COURSE_WAYPOINTS[i];
        point.y = SurfaceHeightAt(CC_LOCAL_SCENE_STREET,
                                  point.x, point.z) + 0.035f;
        DrawCylinder(point, 0.075f, 0.075f, 0.025f, 10,
                     Fade(WORLD_GOLD, (i & 1) != 0 ? 0.52f : 0.30f));
    }
    DrawBox((Vector3){9.32f, 0.76f, 1.08f}, (Vector3){0.08f, 1.52f, 0.08f},
            (Color){72, 58, 55, 255});
    DrawBox((Vector3){9.32f, 0.76f, 1.82f}, (Vector3){0.08f, 1.52f, 0.08f},
            (Color){72, 58, 55, 255});
    DrawBox((Vector3){9.32f, 1.48f, 1.45f}, (Vector3){0.08f, 0.08f, 0.82f},
            WORLD_GOLD);
    DrawBox((Vector3){9.28f, 1.20f, 1.45f}, (Vector3){0.035f, 0.42f, 0.52f},
            (Color){73, 55, 91, 255});
}

static void DrawCourseRaider(const CcLocalAgent *raider)
{
    if (raider == NULL) return;
    DrawCombatFootprint(raider, WORLD_DANGER);
    DrawRobotShell(raider);
    DrawCombatSword(raider);
    DrawCombatSkillTell(raider);
    DrawCombatImpact(raider);
    DrawPaintedOverheadDiamond(
        (Vector3){raider->position.x,
                  raider->position.y + 2.05f,
                  raider->position.z},
        0.085f, WORLD_DANGER);
}

static void DrawCourseRunners(const CcLocalCourse *course, Vector3 focus)
{
    if (course == NULL) return;
    if (course->situation_witness_active &&
        SceneryPointVisible(course->situation_witness.position.x,
                            course->situation_witness.position.z, focus)) {
        DrawRobotShell(&course->situation_witness);
    }
    for (int32_t i = 0; i < CC_LOCAL_TRAVELLER_COUNT; ++i) {
        const CcLocalTraveller *traveller = &course->travellers[i];
        if (traveller->active &&
            SceneryPointVisible(traveller->agent.position.x,
                                traveller->agent.position.z, focus)) {
            DrawRobotShell(&traveller->agent);
        }
    }
    Vector3 threat = CourseThreatCenter(course);
    if (course->alarm_active) {
        for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
            const CcLocalAgent *raider = &course->raiders[i];
            if (SceneryPointVisible(raider->position.x,
                                    raider->position.z, focus)) {
                DrawCourseRaider(raider);
            }
        }
    }
    for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
        const CcLocalCourseRunner *runner = &course->runners[i];
        bool runner_visible = SceneryPointVisible(runner->agent.position.x,
                                                   runner->agent.position.z,
                                                   focus);
        if (!runner_visible) {
            if (runner->agent.exact_target_valid) {
                Vector3 target = runner->agent.target_point;
                if (SceneryPointVisible(target.x, target.z, focus)) {
                    DrawGroundBrushStroke(
                        (Vector3){target.x, target.y + 0.025f, target.z},
                        (Vector3){1.0f, 0.0f, 0.45f}, 0.32f, 0.075f,
                        Fade(runner->marker_color, 0.58f));
                }
            }
            continue;
        }
        if (course->alarm_active) {
            DrawCombatFootprint(&runner->agent, runner->marker_color);
        }
        if (runner->agent.exact_target_valid) {
            Vector3 target = runner->agent.target_point;
            DrawGroundBrushStroke(
                (Vector3){target.x, target.y + 0.025f, target.z},
                (Vector3){1.0f, 0.0f, 0.45f}, 0.32f, 0.075f,
                Fade(runner->marker_color, 0.58f));
        }
        DrawRobotShell(&runner->agent);
        DrawCombatImpact(&runner->agent);
        if (!runner->agent.climbing &&
            !runner->agent.humanoid.ragdoll.active) {
            const CcHumanoidPose *pose = AgentRenderPose(&runner->agent);
            Vector3 hand = FromLimbVector(pose->hand[1]);
            Vector3 elbow = FromLimbVector(pose->elbow[1]);
            Vector3 aim = {sinf(runner->agent.facing_yaw), 0.0f,
                           cosf(runner->agent.facing_yaw)};
            if (course->alarm_active) {
                aim = PhysicsNormalizeOr(
                    PhysicsSubtract(threat, hand), aim);
                aim.y = runner->duty == CC_GUARD_ENGAGED ? 0.06f : 0.16f;
                aim = PhysicsNormalizeOr(aim,
                    (Vector3){0.0f, 0.0f, 1.0f});
            }
            Vector3 forearm = PhysicsNormalizeOr(
                PhysicsSubtract(hand, elbow), aim);
            float arm_weight = runner->duty == CC_GUARD_ENGAGED ? 0.78f : 0.42f;
            Vector3 spear_direction = PhysicsNormalizeOr(
                PhysicsLerp(aim, forearm, arm_weight), aim);
            float spear_length = runner->duty == CC_GUARD_ENGAGED ? 0.78f :
                                                                         0.58f;
            Vector3 spear_tip = PhysicsAdd(
                hand, PhysicsScale(spear_direction, spear_length));
            DrawCylinderEx(hand, spear_tip, 0.022f, 0.013f, 7,
                           (Color){128, 92, 55, 255});
            DrawSmallSphere(spear_tip, 0.038f, WORLD_GOLD);
            Vector3 shield_hand = FromLimbVector(pose->hand[0]);
            DrawCharacterSphere(shield_hand, 0.105f,
                                (Color){55, 74, 78, 255});
            DrawSmallSphere(shield_hand, 0.055f, runner->marker_color);
        }
        DrawPaintedOverheadDiamond(
            (Vector3){runner->agent.position.x,
                      runner->agent.position.y + 2.05f,
                      runner->agent.position.z},
            0.085f, runner->marker_color);
    }
}

static const char *RoadArchetypeName(const CcRoute *route)
{
    if (route == NULL) return "UNKNOWN ROAD";
    if (route->smuggler_route) return "HIDDEN WOODLAND TRACK";
    if (route->closed) return "BROKEN CAUSEWAY";
    if (route->condition < 48) return "RUTTED FRONTIER ROAD";
    if (route->security >= 70) return "PATROLLED KING'S ROAD";
    return "HEDGEROW TRADE ROAD";
}

static void DrawRoadHorseTeam(Vector3 base, float clock, bool moving,
                              float yaw)
{
    CcCreaturePose left_pose = CcCreatureSteppedPose(
        CC_CREATURE_HORSE, clock * 4.8f, moving);
    CcCreaturePose right_pose = CcCreatureSteppedPose(
        CC_CREATURE_HORSE, clock * 4.8f + PI, moving);
    for (int32_t horse = -1; horse <= 1; horse += 2) {
        Color coat = horse < 0 ? BlendColor(WORLD_WOOD, WORLD_ROAD, 0.42f) :
                                 BlendColor(WORLD_WOOD_LIGHT,
                                            WORLD_ROAD, 0.36f);
        Vector3 horse_base = LocalPoint(
            base, (float)horse * 1.05f, 0.0f, 4.60f, yaw);
        CcCreaturePose pose = horse < 0 ? left_pose : right_pose;
        float gait_phase = clock * 4.8f + (horse < 0 ? 0.0f : PI);
        CcCreatureRigPose controlled_pose;
        CreatureGaitSlot slot = horse < 0 ? CREATURE_GAIT_ROAD_HORSE_LEFT :
                                            CREATURE_GAIT_ROAD_HORSE_RIGHT;
        bool controlled = ResolveControlledCreatureGait(
            slot, CC_CREATURE_RIG_HORSE, clock,
            horse < 0 ? 0.0f : 0.5f, 1.35f, moving, &controlled_pose);
        (void)DrawCreatureGait3D(
            CC_CREATURE_HORSE, pose, horse_base,
            yaw, 0.96f, coat, gait_phase, moving,
            controlled ? &controlled_pose : NULL);
        Vector3 trace_start = LocalPoint(
            base, (float)horse * 0.42f, 0.77f, 3.05f, yaw);
        Vector3 trace_end = LocalPoint(horse_base, 0.0f, 0.91f, -0.52f, yaw);
        DrawCylinderEx(trace_start, trace_end, 0.020f, 0.016f, 6,
                       WORLD_WOOD_SHADOW);
        DrawOrientedBox(horse_base, (Vector3){0.0f, 1.02f, 0.18f},
                        (Vector3){0.66f, 0.055f, 0.055f}, yaw,
                        WORLD_WOOD_SHADOW);
    }
}

static void DrawRoadCarriage(Vector3 base, int32_t cargo_used, float clock,
                             bool moving, float yaw, bool horses_hitched)
{
    float asset_yaw_degrees = yaw * RAD2DEG - 90.0f;
    RuntimeAsset *carriage = &runtime_assets[RUNTIME_ASSET_CARRIAGE];
    if (carriage->ready) {
        DrawModelEx(carriage->model, base, (Vector3){0.0f, 1.0f, 0.0f},
                    asset_yaw_degrees,
                    (Vector3){CARRIAGE_ASSET_SCALE,
                              CARRIAGE_ASSET_SCALE,
                              CARRIAGE_ASSET_SCALE}, WHITE);
        RuntimeAsset *rack = &runtime_assets[RUNTIME_ASSET_CARGO_RACK];
        if (cargo_used > 0 && rack->ready) {
            DrawModelEx(rack->model, base, (Vector3){0.0f, 1.0f, 0.0f},
                        asset_yaw_degrees,
                        (Vector3){CARRIAGE_ASSET_SCALE,
                                  CARRIAGE_ASSET_SCALE,
                                  CARRIAGE_ASSET_SCALE}, WHITE);
        }
    } else {
        DrawOrientedBox(base, (Vector3){0.0f, 1.10f, 0.0f},
                        (Vector3){2.55f, 1.58f, 4.15f}, yaw,
                        BlendColor(WORLD_EARTH, WORLD_DANGER, 0.28f));
        DrawOrientedBox(base, (Vector3){0.0f, 1.98f, 0.0f},
                        (Vector3){2.78f, 0.18f, 4.38f}, yaw,
                        WORLD_GOLD);
        DrawOrientedBox(base, (Vector3){0.0f, 1.27f, 2.10f},
                        (Vector3){1.30f, 0.72f, 0.06f}, yaw, WORLD_TEAL);
        static const Vector2 wheel_offsets[4] = {
            {-1.36f, -1.38f}, {1.36f, -1.38f},
            {-1.36f, 1.38f}, {1.36f, 1.38f}
        };
        for (int32_t i = 0; i < 4; ++i) {
            Vector3 wheel = LocalPoint(base, wheel_offsets[i].x, 0.58f,
                                       wheel_offsets[i].y, yaw);
            DrawScenerySphere(wheel, 0.53f, WORLD_WOOD_SHADOW);
            DrawSphereWires(wheel, 0.55f, 7, 7, WORLD_GOLD);
        }
        Vector3 pole_left = LocalPoint(base, -0.48f, 0.82f, 2.10f, yaw);
        Vector3 pole_right = LocalPoint(base, 0.48f, 0.82f, 2.10f, yaw);
        Vector3 pole_left_end = LocalPoint(base, -0.48f, 0.72f, 4.10f, yaw);
        Vector3 pole_right_end = LocalPoint(base, 0.48f, 0.72f, 4.10f, yaw);
        DrawCylinderEx(pole_left, pole_left_end, 0.045f, 0.035f, 7,
                       WORLD_WOOD);
        DrawCylinderEx(pole_right, pole_right_end, 0.045f, 0.035f, 7,
                       WORLD_WOOD);
    }
    if (horses_hitched) DrawRoadHorseTeam(base, clock, moving, yaw);
}

static void DrawStableHorseTeam(float clock)
{
    static const Vector2 stalls[] = {{40.55f, 30.25f}, {40.55f, 33.05f}};
    for (int32_t horse = 0; horse < 2; ++horse) {
        Vector3 base = TerrainWorldPoint(stalls[horse].x, stalls[horse].y);
        Color coat = horse == 0 ?
            BlendColor(WORLD_WOOD, WORLD_ROAD, 0.42f) :
            BlendColor(WORLD_WOOD_LIGHT, WORLD_ROAD, 0.36f);
        CcCreaturePose pose = CcCreatureSteppedPose(
            CC_CREATURE_HORSE, clock * 0.35f + (float)horse * PI, false);
        (void)DrawCreatureGait3D(
            CC_CREATURE_HORSE, pose, base, 0.5f * PI, 0.96f, coat,
            clock * 0.35f + (float)horse * PI, false, NULL);
    }
    float rail_y = CcLocalTerrainHeightAt(39.45f, 31.65f);
    DrawBox((Vector3){39.45f, rail_y + 0.72f, 31.65f},
            (Vector3){0.12f, 1.44f, 4.15f}, WORLD_WOOD_SHADOW);
    DrawBox((Vector3){39.45f, rail_y + 1.12f, 31.65f},
            (Vector3){0.18f, 0.12f, 4.25f}, WORLD_WOOD_LIGHT);
}

static bool ForkRouteLeaves(const CcSim *sim, const CcRoute *route)
{
    return sim != NULL && route != NULL &&
           (route->from_id == sim->player.location_id ||
            route->to_id == sim->player.location_id);
}

static const CcRoute *ForkSelectedRoute(const CcSim *sim,
                                        int32_t selected_route)
{
    if (sim == NULL || selected_route < 0 ||
        selected_route >= sim->route_count) return NULL;
    const CcRoute *route = &sim->routes[selected_route];
    return ForkRouteLeaves(sim, route) ? route : NULL;
}

static int32_t ForkRouteOrdinal(const CcSim *sim, int32_t selected_route)
{
    int32_t ordinal = 0;
    if (sim == NULL) return ordinal;
    for (int32_t i = 0; i < sim->route_count; ++i) {
        if (!ForkRouteLeaves(sim, &sim->routes[i])) continue;
        if (i == selected_route) return ordinal;
        ordinal += 1;
    }
    return 0;
}

static void DrawForkRoadSegment(Vector3 from, Vector3 to, float width,
                                Color color, bool rutted)
{
    float dx = to.x - from.x;
    float dz = to.z - from.z;
    float length = sqrtf(dx * dx + dz * dz);
    if (length <= 0.01f) return;
    float yaw = atan2f(dx, dz);
    DrawOrientedBox(from, (Vector3){0.0f, 0.015f, length * 0.5f},
                    (Vector3){width + 0.55f, 0.055f, length}, yaw,
                    ShadeColor(color, 0.58f));
    DrawOrientedBox(from, (Vector3){0.0f, 0.050f, length * 0.5f},
                    (Vector3){width, 0.045f, length}, yaw, color);
    if (!rutted) return;
    for (int32_t side = -1; side <= 1; side += 2) {
        DrawOrientedBox(
            from,
            (Vector3){(float)side * width * 0.22f, 0.078f,
                      length * 0.5f},
            (Vector3){0.13f, 0.012f, length}, yaw,
            Fade((Color){45, 35, 29, 255}, 0.72f));
    }
}

static void DrawForkRouteState(const CcRoute *route, Vector3 from,
                               Vector3 to)
{
    float dx = to.x - from.x;
    float dz = to.z - from.z;
    float length = sqrtf(dx * dx + dz * dz);
    if (route == NULL || length <= 0.01f) return;
    Vector3 forward = {dx / length, 0.0f, dz / length};
    Vector3 side = {-forward.z, 0.0f, forward.x};
    if (route->closed) {
        Vector3 center = Vector3Add(from, Vector3Scale(forward, 20.0f));
        Vector3 left = Vector3Add(center, Vector3Scale(side, -2.8f));
        Vector3 right = Vector3Add(center, Vector3Scale(side, 2.8f));
        left.y = 0.72f;
        right.y = 0.72f;
        DrawCylinderEx(left, right, 0.13f, 0.13f, 7,
                       (Color){108, 69, 43, 255});
        DrawCylinder((Vector3){left.x, 0.0f, left.z},
                     0.12f, 0.09f, 1.25f, 7,
                     (Color){86, 59, 42, 255});
        DrawCylinder((Vector3){right.x, 0.0f, right.z},
                     0.12f, 0.09f, 1.25f, 7,
                     (Color){86, 59, 42, 255});
    }
    int32_t scars = (100 - route->condition) / 18;
    for (int32_t scar = 0; scar < scars && scar < 4; ++scar) {
        Vector3 center = Vector3Add(
            from, Vector3Scale(forward, 7.0f + (float)scar * 5.0f));
        center = Vector3Add(
            center, Vector3Scale(side, (scar & 1) != 0 ? 0.9f : -0.7f));
        DrawCylinder((Vector3){center.x, 0.08f, center.z},
                     0.34f, 0.48f, 0.025f, 12,
                     (Color){49, 39, 32, 255});
    }
}

static void DrawForkSignpost(Vector3 branch_end, bool hidden)
{
    Vector3 base = {50.5f, 0.0f, 40.0f};
    DrawCylinder(base, 0.17f, 0.13f, 3.25f, 8,
                 (Color){91, 61, 42, 255});
    float yaw = atan2f(branch_end.x - base.x, branch_end.z - base.z);
    DrawOrientedBox(base, (Vector3){0.0f, 1.58f, 0.72f},
                    (Vector3){1.65f, 0.30f, 0.12f}, yaw,
                    hidden ? (Color){77, 61, 48, 255} :
                             (Color){132, 91, 54, 255});
}

void CcLocalDrawFork3D(const CcSim *sim, int32_t selected_route,
                       float clock, RenderTexture2D target,
                       Rectangle destination)
{
    if (sim == NULL) return;
    const CcSettlement *here = CcSimSettlement(
        sim, sim->player.location_id);
    CcId kingdom_id = here != NULL ? here->kingdom_id : 0U;
    Color kingdom = KingdomColor3D(sim, kingdom_id);
    Color ground = BlendColor((Color){41, 67, 48, 255}, kingdom, 0.16f);
    Camera3D camera = ExteriorCameraComposed(
        (Vector3){48.0f, 0.85f, 41.0f},
        (Vector3){17.0f, 24.0f, 27.0f}, 32.0f);
    camera = SnapCameraToArtPixels(camera, target.texture.height);
    ArtComposition fork_art = ROAD_ART_COMPOSITION;
    fork_art.focal_point = camera.target;
    fork_art.foreground_anchor = (Vector3){29.0f, 0.0f, 41.0f};
    SetFaceRenderContext(camera, target.texture.width, target.texture.height);
    BeginTextureMode(target);
    ClearBackground(ArtLightBackground(fork_art.light_profile));
    BeginMode3D(camera);
    BeginWorldLighting(camera, &fork_art);
    DrawPlane((Vector3){48.0f, -0.08f, 40.0f},
              (Vector2){104.0f, 78.0f}, ground);
    Vector3 approach = {5.0f, 0.0f, 42.0f};
    Vector3 junction = {50.5f, 0.0f, 40.0f};
    Vector3 onward = {96.0f, 0.0f, 35.5f};
    DrawForkRoadSegment(approach, junction, 5.1f,
                        (Color){101, 91, 72, 255}, true);
    DrawForkRoadSegment(junction, onward, 5.1f,
                        (Color){98, 88, 70, 255}, true);

    WorldLabel labels[1] = {0};
    char label_text[CC_NAME_CAPACITY] = {0};
    int32_t label_count = 0;
    const CcRoute *route = ForkSelectedRoute(sim, selected_route);
    if (route != NULL) {
        int32_t ordinal = ForkRouteOrdinal(sim, selected_route);
        float side = (ordinal & 1) != 0 ? 1.0f : -1.0f;
        Vector3 branch_end = {77.0f, 0.0f, 40.0f + side * 23.0f};
        float decay = 1.0f - (float)route->condition / 100.0f;
        Color road = route->smuggler_route ?
            (Color){69, 56, 49, 255} :
            BlendColor((Color){113, 102, 80, 255},
                       (Color){77, 61, 47, 255}, decay);
        float width = route->smuggler_route ? 2.8f : 4.2f;
        DrawForkRoadSegment(junction, branch_end, width, road, true);
        DrawForkRouteState(route, junction, branch_end);

        CcId destination_id = route->from_id == sim->player.location_id ?
            route->to_id : route->from_id;
        const CcSettlement *place = CcSimSettlement(sim, destination_id);
        CcTravelPreview preview = {0};
        (void)CcSimTravelPreview(sim, destination_id, &preview, NULL, 0U);
        (void)snprintf(
            label_text, sizeof(label_text), "%s",
            preview.destination_known && place != NULL ?
                place->name : "UNMARKED");
        labels[0] = (WorldLabel){
            {50.5f, 2.35f, 40.0f}, label_text,
            route->smuggler_route ? WORLD_VIOLET : WORLD_GOLD
        };
        label_count = 1;
        DrawForkSignpost(branch_end, route->smuggler_route);
    }

    TreeRegionalStyle tree_style = TreeStyleForKingdom(kingdom);
    static const Vector2 tree_positions[] = {
        {17.0f, 27.0f}, {23.0f, 55.0f}, {35.0f, 24.0f},
        {39.0f, 58.0f}, {54.0f, 9.0f}, {56.0f, 72.0f},
        {72.0f, 7.0f}, {74.0f, 75.0f}, {88.0f, 15.0f},
        {91.0f, 55.0f}
    };
    for (int32_t tree = 0;
         tree < (int32_t)(sizeof(tree_positions) /
                          sizeof(tree_positions[0])); ++tree) {
        if (!SceneryPointVisible(tree_positions[tree].x,
                                 tree_positions[tree].y,
                                 camera.target)) {
            continue;
        }
        TreeFamily family = tree % 4 == 0 ? TREE_FAMILY_OAK :
                            tree % 3 == 0 ? TREE_FAMILY_POLLARD :
                                            TREE_FAMILY_ALDER;
        Color leaves = (tree & 1) != 0 ? (Color){46, 96, 69, 255} :
                                        (Color){63, 112, 77, 255};
        DrawTree(tree_positions[tree].x, tree_positions[tree].y,
                 family, leaves, tree_style);
    }

    int32_t cargo = CcPlayerCargoUsed(&sim->player);
    Vector3 carriage = {29.0f, 0.0f, 41.0f};
    DrawRoadCarriage(carriage, cargo, clock, false, 0.5f * PI, true);
    DrawVisibleNpcFigure3D(
        (Vector3){33.0f, 0.0f, 35.6f}, 0.90f, 1.42f,
        UINT32_C(0x666f726b), CC_NPC_ROLE_SCOUT, kingdom,
        clock * 0.35f, CC_TRAVERSAL_IDLE, camera.target);
    DrawVisibleNpcFigure3D(
        (Vector3){26.0f, 0.0f, 45.2f}, 0.86f, 0.74f,
        UINT32_C(0x666f726d), CC_NPC_ROLE_TRAVELLER, WORLD_TEAL,
        clock * 0.28f + 1.0f, CC_TRAVERSAL_IDLE, camera.target);
    EndWorldLighting();
    EndMode3D();
    EndTextureMode();
    PresentTarget(target, destination);
    DrawLabels(labels, label_count, camera, destination);
}

static void DrawRoadBarricade(const CcRoute *route)
{
    const float x = ROAD_BARRICADE_X;
    Color timber = route != NULL && route->smuggler_route ?
                   WORLD_WOOD_SHADOW : WORLD_WOOD;
    for (int32_t rail = -1; rail <= 1; rail += 2) {
        Vector3 from = {x - 0.18f, 0.48f + (float)(rail + 1) * 0.18f,
                        36.95f};
        Vector3 to = {x + 0.18f, 0.66f + (float)(rail + 1) * 0.18f,
                      43.05f};
        DrawCylinderEx(from, to, 0.10f, 0.10f, 8, timber);
    }
    for (int32_t post = 0; post < 3; ++post) {
        float z = 37.25f + (float)post * 2.75f;
        DrawCylinder((Vector3){x, 0.0f, z}, 0.12f, 0.09f, 1.35f, 7,
                     timber);
        DrawCylinderEx((Vector3){x, 0.74f, z},
                       (Vector3){x - 0.72f, 0.10f, z - 0.35f},
                       0.065f, 0.025f, 6, (Color){75, 61, 51, 255});
    }
    DrawBox((Vector3){x - 0.90f, 0.42f, 37.55f},
            (Vector3){0.72f, 0.84f, 0.72f},
            (Color){121, 82, 52, 255});
    DrawBox((Vector3){x + 0.83f, 0.30f, 42.30f},
            (Vector3){0.92f, 0.60f, 0.72f},
            (Color){105, 72, 49, 255});
    DrawBox((Vector3){x, 1.82f, 40.00f},
            (Vector3){0.08f, 1.12f, 1.25f}, WORLD_DANGER);
}

static void DrawRoadSurface(float x, float width, Color road)
{
    if (width <= 0.0f) return;
    DrawTerrainPatchAtTop(x, 36.30f, width, 7.40f, -0.024f,
                          ShadeColor(road, 0.68f));
    DrawTerrainPatchAtTop(x, 36.55f, width, 6.90f, -0.014f, road);
    DrawTerrainPatchAtTop(x, 36.40f, width, 0.16f, -0.005f,
                          ShadeColor(road, 0.56f));
    DrawTerrainPatchAtTop(x, 43.44f, width, 0.16f, -0.005f,
                          ShadeColor(road, 0.56f));
    for (int32_t rut = 0; rut < 2; ++rut) {
        DrawTerrainPatchAtTop(x, 38.45f + (float)rut * 3.05f,
                              width, 0.16f, -0.003f,
                              Fade(WORLD_WOOD_SHADOW, 0.72f));
    }
}

static void DrawRoadTerrain(const CcRoute *route, int32_t danger,
                            bool bridge_checkpoint, Color kingdom,
                            Vector3 focus)
{
    float decay = route != NULL ?
        1.0f - (float)route->condition / 100.0f : 0.5f;
    Color ground = route != NULL && route->smuggler_route ?
        WORLD_GRASS_SHADOW :
        BlendColor(WORLD_GRASS, WORLD_EARTH, decay * 0.75f);
    Color road = BlendColor(WORLD_ROAD_LIGHT,
                            WORLD_EARTH_SHADOW, decay);
    DrawPlane((Vector3){CC_LOCAL_WORLD_WIDTH * 0.5f, -0.09f,
                        CC_LOCAL_WORLD_DEPTH * 0.5f},
              (Vector2){CC_LOCAL_WORLD_WIDTH + 8.0f,
                        CC_LOCAL_WORLD_DEPTH + 8.0f}, ground);
    if (bridge_checkpoint) {
        /* Leave an 8.8 m opening for the authored river, banks, and span. */
        DrawRoadSurface(15.0f, ROAD_BARRICADE_X - 4.40f - 15.0f, road);
        DrawRoadSurface(ROAD_BARRICADE_X + 4.40f,
                        81.0f - (ROAD_BARRICADE_X + 4.40f), road);
    } else {
        DrawRoadSurface(15.0f, 66.0f, road);
    }
    int32_t scars = route != NULL ? (100 - route->condition) / 12 : 4;
    for (int32_t scar = 0; scar < scars && scar < 7; ++scar) {
        float x = 28.0f + (float)scar * 6.70f;
        if (bridge_checkpoint &&
            x > ROAD_BARRICADE_X - 4.40f &&
            x < ROAD_BARRICADE_X + 4.40f) continue;
        float z = 39.10f + (float)(scar & 1) * 1.65f;
        if (!SceneryPointVisible(x, z, focus)) continue;
        DrawCylinder((Vector3){x, -0.025f, z}, 0.28f, 0.42f,
                     0.035f, 12, WORLD_EARTH_SHADOW);
    }
    Color leaves = route != NULL && route->smuggler_route ?
        WORLD_FOLIAGE_SHADOW : WORLD_FOLIAGE;
    TreeRegionalStyle regional_style = TreeStyleForKingdom(kingdom);
    for (int32_t tree = 0; tree < 13; ++tree) {
        float x = 20.0f + (float)tree * 4.75f;
        x += (tree & 1) != 0 ? -regional_style.cluster_pull :
                               regional_style.cluster_pull;
        float z = (tree & 1) != 0 ? 46.0f + (float)(tree % 3) * 1.2f :
                                    33.8f - (float)(tree % 3) * 1.1f;
        if (!SceneryPointVisible(x, z, focus)) continue;
        TreeFamily family = tree == 4 || tree == 10 ? TREE_FAMILY_OAK :
                            tree == 1 || tree == 7 ? TREE_FAMILY_POLLARD :
                                                    TREE_FAMILY_ALDER;
        family = RegionalTreeFamily(family, tree, regional_style);
        Color tree_leaves = (tree % 3) == 0 ?
                            ShadeColor(leaves, 0.82f) : leaves;
        if (family == TREE_FAMILY_OAK) {
            tree_leaves = ShadeColor(tree_leaves, 0.86f);
        } else if (family == TREE_FAMILY_POLLARD) {
            tree_leaves = BlendColor(tree_leaves,
                                     WORLD_GRASS_LIGHT, 0.16f);
        }
        DrawTree(x, z, family, tree_leaves, regional_style);
    }
    if (route != NULL && (route->closed || route->condition < 42) &&
        SceneryFootprintVisible((Rectangle){68.0f, 32.0f, 4.0f, 16.0f},
                                focus)) {
        DrawTerrainPatchAtTop(68.0f, 32.0f, 4.0f, 16.0f, -0.002f,
                              CC_STYLE_PANEL_DEEP);
        for (int32_t plank = 0; plank < 5; ++plank) {
            DrawBox((Vector3){70.0f, 0.12f, 37.35f + (float)plank * 1.32f},
                    (Vector3){4.45f, 0.18f, 0.92f},
                    plank == 2 ? WORLD_WOOD_SHADOW : WORLD_WOOD_LIGHT);
        }
    }
    if (route != NULL && route->security >= 65 &&
        SceneryPointVisible(30.20f, 35.70f, focus)) {
        DrawBox((Vector3){30.20f, 1.05f, 35.70f},
                (Vector3){0.58f, 2.10f, 0.58f},
                WORLD_STONE_LIGHT);
        DrawBox((Vector3){30.20f, 1.68f, 35.38f},
                (Vector3){0.42f, 0.46f, 0.05f}, WORLD_GOLD);
    }
    if (danger >= 45 && SceneryFootprintVisible(
            (Rectangle){58.5f, 43.79f, 2.72f, 0.72f}, focus)) {
        for (int32_t marker = 0; marker < 3; ++marker) {
            DrawBox((Vector3){58.5f + (float)marker * 1.1f, 0.18f,
                              44.15f},
                    (Vector3){0.52f, 0.36f, 0.72f},
                    BlendColor(WORLD_EARTH_SHADOW, WORLD_DANGER, 0.24f));
        }
    }
}

static bool DrawBridgeCheckpoint(void)
{
    RuntimeAsset *asset = &runtime_assets[RUNTIME_ASSET_BRIDGE];
    if (!asset->ready) return false;
    DrawModel(asset->model,
              (Vector3){ROAD_BARRICADE_X, 0.0f, 40.0f}, 1.0f, WHITE);
    return true;
}

float CcLocalRoadCarriageX(int32_t progress_milli)
{
    if (progress_milli < 0) progress_milli = 0;
    if (progress_milli > 1000) progress_milli = 1000;
    return 20.15f + (float)progress_milli * 0.052f;
}

void CcLocalDrawRoad3D(const CcSim *sim, const CcLocalAgent *agent,
                       const CcLocalCourse *course, bool travelling,
                       bool parley, const CcLocalConvoyState *convoy,
                       float clock, RenderTexture2D target,
    Rectangle destination)
{
    if (sim == NULL || agent == NULL || course == NULL) return;
    const CcRoute *route = CcSimRoute(sim, sim->journey.route_id);
    const CcSettlement *origin = CcSimSettlement(
        sim, sim->journey.origin_id);
    const CcSettlement *destination_place = CcSimSettlement(
        sim, sim->journey.destination_id);
    const CcBanditGroup *bandits = NULL;
    for (int32_t i = 0; i < sim->bandit_count; ++i) {
        if (sim->bandits[i].route_id == sim->journey.route_id) {
            bandits = &sim->bandits[i];
            break;
        }
    }
    bool crossing_gate = convoy != NULL &&
        convoy->phase == CC_LOCAL_CONVOY_GATE;
    float carriage_x = crossing_gate ?
        15.50f + convoy->phase_progress * 4.65f :
        CcLocalRoadCarriageX(sim->carriage.progress_milli);
    float lateral_offset = convoy != NULL ? convoy->lateral_offset : 0.0f;
    bool carriage_moving = travelling && convoy != NULL ?
        convoy->pace > 0.02f : travelling;
    Vector3 carriage_base = {
        carriage_x,
        carriage_moving ? 0.025f + sinf(clock * 5.0f) * 0.018f : 0.0f,
        40.0f + lateral_offset
    };
    Vector3 camera_focus = travelling ? carriage_base : agent->position;
    Camera3D base_camera = RoadCamera(camera_focus, travelling, clock, true,
                                      target.texture.height);
    Camera3D camera = CcLocalCombatCameraInternal(
        base_camera, agent, course, clock, true, target.texture.height);
    if (!travelling) {
        camera = KeepHeroInsideStreetFrame(
            camera,
            Vector3Add(agent->position, (Vector3){0.0f, 1.05f, 0.0f}),
            target.texture.height,
            (Rectangle){0.10f, 0.12f, 0.80f, 0.76f});
        camera = SnapCameraToArtPixels(camera, target.texture.height);
    }
    bool combat_presentation = !travelling && course->alarm_active &&
                               camera.projection == CAMERA_PERSPECTIVE;
    const CcLocalAgent *duel_opponent = combat_presentation ?
        CombatCameraOpponent(course, agent) : NULL;
    RememberPresentedCamera(CC_LOCAL_SCENE_ROAD, camera, agent,
                            target.texture.width, target.texture.height);
    ArtComposition road_art = ROAD_ART_COMPOSITION;
    road_art.focal_point = camera.target;
    road_art.foreground_anchor = carriage_base;
    Color background = ArtLightBackground(road_art.light_profile);
    SetFaceRenderContext(camera, target.texture.width, target.texture.height);
    BeginTextureMode(target);
    ClearBackground(background);
    BeginMode3D(camera);
    BeginWorldLighting(camera, &road_art);
    bool authored_checkpoint = !travelling &&
        runtime_assets[RUNTIME_ASSET_BRIDGE].ready;
    CcId road_kingdom_id = origin != NULL ? origin->kingdom_id :
        destination_place != NULL ? destination_place->kingdom_id : 0;
    Color road_kingdom = KingdomColor3D(sim, road_kingdom_id);
    DrawRoadTerrain(route, sim->journey.danger, authored_checkpoint,
                    road_kingdom, camera.target);
    if (!travelling &&
        SceneryPointVisible(ROAD_BARRICADE_X, 40.0f, camera.target) &&
        !DrawBridgeCheckpoint()) {
        DrawRoadBarricade(route);
    }
    if (!travelling) DrawAgentPath(agent, false);
    int32_t road_cargo = CcPlayerCargoUsed(&sim->player);
    DrawRoadCarriage(carriage_base, road_cargo, clock, carriage_moving,
                     0.5f * PI, true);
    int32_t roadside_cattle = 0;
    if (origin != NULL) {
        roadside_cattle += origin->cow_adults + origin->cow_calves;
    }
    if (destination_place != NULL) {
        roadside_cattle += destination_place->cow_adults +
                           destination_place->cow_calves;
    }
    if (roadside_cattle > 0) {
        Vector3 cow_position = {carriage_x + 3.8f, 0.0f, 33.40f};
        CcCreaturePose cow_pose = CcCreatureSteppedPose(
            CC_CREATURE_COW, clock * 1.55f, travelling);
        CcCreatureRigPose controlled_cow;
        bool controlled = ResolveControlledCreatureGait(
            CREATURE_GAIT_ROAD_COW, CC_CREATURE_RIG_COW, clock,
            0.18f, 0.62f, travelling, &controlled_cow);
        (void)DrawCreatureGait3D(
            CC_CREATURE_COW, cow_pose, cow_position,
            -0.72f * PI, 0.88f, (Color){184, 169, 139, 255},
            clock * 1.55f, travelling,
            controlled ? &controlled_cow : NULL);
        if (roadside_cattle >= 24) {
            (void)DrawCreature3D(
                CC_CREATURE_COW, CC_CREATURE_POSE_IDLE,
                (Vector3){carriage_x + 5.60f, 0.0f, 32.80f},
                -0.56f * PI, 0.78f, (Color){118, 86, 66, 255});
        }
    }

    if (!combat_presentation) {
        DrawNpcFigure3D(
            (Vector3){carriage_x - 3.25f, 0.0f, 37.95f}, 0.90f, 1.35f,
            UINT32_C(0x726f6101), CC_NPC_ROLE_TRAVELLER,
            (Color){151, 103, 78, 255}, clock * 0.42f,
            CC_TRAVERSAL_IDLE);
        DrawNpcFigure3D(
            (Vector3){carriage_x - 4.00f, 0.0f, 39.40f}, 0.84f, 1.10f,
            UINT32_C(0x726f6102), CC_NPC_ROLE_HEALER,
            WORLD_TEAL, clock * 0.36f + 1.4f,
            CC_TRAVERSAL_IDLE);
        DrawNpcFigure3D(
            (Vector3){carriage_x - 3.35f, 0.0f, 41.55f}, 0.80f, 1.55f,
            UINT32_C(0x726f6103), CC_NPC_ROLE_REFUGEE,
            WORLD_VIOLET, clock * 0.31f + 2.1f,
            CC_TRAVERSAL_IDLE);
        DrawBox((Vector3){carriage_x - 4.25f, 0.34f, 42.45f},
                (Vector3){0.72f, 0.68f, 0.72f},
                (Color){137, 91, 55, 255});
        DrawBox((Vector3){carriage_x - 3.33f, 0.25f, 42.42f},
                (Vector3){0.82f, 0.50f, 0.64f},
                (Color){112, 76, 53, 255});
    }

    if (!travelling) {
        if (combat_presentation) DrawCourseRaider(duel_opponent);
        else DrawCourseRunners(course, camera.target);
    }
    if (parley && !course->alarm_active) {
        for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
            DrawRobotShell(&course->raiders[i]);
        }
    }
    if (agent->combat.target_index >= 0 &&
        agent->combat.target_index < CC_LOCAL_RAIDER_COUNT) {
        DrawSelectedTarget(&course->raiders[agent->combat.target_index]);
    }
    if (!travelling) {
        DrawRobotShell(agent);
        DrawCombatSword(agent);
        DrawCombatSkillTell(agent);
        DrawCombatImpact(agent);
    }
    EndWorldLighting();
    EndMode3D();
    DrawTargetAtmosphere(target, clock);
    EndTextureMode();
    PresentTarget(target, destination);
    char route_label[96];
    char blockade_label[96];
    (void)snprintf(route_label, sizeof(route_label), "%s -> %s",
                   origin != NULL ? origin->name : "ORIGIN",
                   destination_place != NULL ? destination_place->name :
                                               "DESTINATION");
    (void)snprintf(blockade_label, sizeof(blockade_label), "%s",
                   bandits != NULL ? bandits->name : "ROAD COLLECTORS");
    WorldLabel labels[6];
    int32_t count = 0;
    if (!travelling && !combat_presentation) {
        labels[count++] = (WorldLabel){{agent->position.x,
                                        agent->position.y + 2.50f,
                                        agent->position.z}, "YOU", WORLD_TEAL};
    }
    if (!travelling && !parley && !combat_presentation) {
        labels[count++] = (WorldLabel){{ROAD_BARRICADE_X, 2.58f, 40.00f},
                                       blockade_label, WORLD_DANGER};
    }
    if (!combat_presentation) {
        labels[count++] = (WorldLabel){
            {travelling ? carriage_x + 8.0f : 57.0f, 1.18f, 40.0f},
            route_label, WORLD_GOLD};
    }
    if (parley && !combat_presentation) {
        labels[count++] = (WorldLabel){
            {course->raiders[0].position.x,
             course->raiders[0].position.y + 2.18f,
             course->raiders[0].position.z},
            course->raider_names[0], WORLD_DANGER};
        labels[count++] = (WorldLabel){{CC_LOCAL_ROAD_PARLEY_X, 0.42f,
                                        CC_LOCAL_ROAD_PARLEY_Z},
                                       "F  SPEAK WITH CAPTAIN", WORLD_TEAL};
    }
    DrawLabels(labels, count, camera, destination);
    if (!travelling && course->alarm_active) {
        DrawCombatBar(agent, camera, destination, WORLD_TEAL);
        if (combat_presentation) {
            DrawCombatBar(CombatCameraOpponent(course, agent), camera,
                          destination, WORLD_GOLD);
        } else {
            for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
                DrawCombatBar(&course->runners[i].agent, camera,
                              destination, course->runners[i].marker_color);
            }
            for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
                DrawCombatBar(&course->raiders[i], camera,
                              destination,
                              i == agent->combat.target_index ? WORLD_GOLD :
                                                                WORLD_DANGER);
            }
        }
    }
    Rectangle road_status = ViewportRectangle(
        destination, 10.0f, 9.0f, 610.0f, 46.0f);
    DrawRectangleRounded(road_status,
                         0.08f, 4, Fade(WORLD_VOID, 0.90f));
    DrawRectangleLinesEx(road_status, 1.0f, Fade(WORLD_GOLD, 0.28f));
    DrawViewportText(
        TextFormat("%s  /  DANGER %d%%  /  ROAD %d%%  /  SECURITY %d%%",
                   RoadArchetypeName(route), sim->journey.danger,
                   route != NULL ? route->condition : 0,
                   route != NULL ? route->security : 0),
        destination, 18, 18, 10, parley ? WORLD_TEAL : WORLD_DANGER);
    DrawViewportText(
        travelling ? crossing_gate ?
        TextFormat("GATE TO ROAD / PACE %d%% / KEEP THE TEAM CENTERED",
                   convoy != NULL ? (int32_t)lroundf(convoy->pace * 100.0f) :
                                      100) :
        TextFormat("REINS IN HAND / %d%% COMPLETE / PACE %d%%",
                   sim->carriage.progress_milli / 10,
                   convoy != NULL ? (int32_t)lroundf(convoy->pace * 100.0f) :
                                      100) : parley ?
        "PARLEY / approach the captain, then choose coin or needed supplies" :
        TextFormat("BREAK THE CORDON / YOU %d HP / COMPANY %d%% NERVE",
                   (int32_t)lroundf(agent->combat.health),
                   course->raider_resolve > 0 ? course->raider_resolve : 0),
        destination, 18, 35, 10, WORLD_INK);
    DrawFixedCameraFade(&road_camera_rig, destination);
}

static void DrawJourneyAftermath3D(const CcSim *sim,
                                   const CcSettlement *place)
{
    if (sim == NULL || place == NULL ||
        sim->resolved_journey_outcome == CC_JOURNEY_OUTCOME_NONE ||
        sim->journey.destination_id != place->id) return;
    const float x = 47.35f;
    const float z = 31.05f;
    rlPushMatrix();
    rlTranslatef(0.0f, CcLocalTerrainHeightAt(x, z), 0.0f);
    if (sim->resolved_journey_outcome == CC_JOURNEY_OUTCOME_COMBAT) {
        DrawBox((Vector3){x, 0.88f, z}, (Vector3){0.14f, 1.76f, 0.14f},
                (Color){82, 61, 46, 255});
        DrawBox((Vector3){x, 1.46f, z - 0.03f},
                (Vector3){0.08f, 0.72f, 0.92f}, WORLD_TEAL);
        DrawBox((Vector3){x + 0.62f, 0.20f, z + 0.48f},
                (Vector3){0.92f, 0.40f, 0.42f},
                (Color){108, 73, 48, 255});
        DrawCylinderEx((Vector3){x + 0.12f, 0.16f, z + 0.78f},
                       (Vector3){x + 1.28f, 0.22f, z + 0.92f},
                       0.075f, 0.065f, 7, (Color){91, 61, 43, 255});
    } else {
        DrawBox((Vector3){x, 0.92f, z}, (Vector3){0.13f, 1.84f, 0.13f},
                (Color){73, 53, 47, 255});
        DrawBox((Vector3){x, 1.50f, z - 0.03f},
                (Vector3){0.08f, 0.78f, 0.94f}, WORLD_VIOLET);
        DrawBox((Vector3){x + 0.58f, 0.30f, z + 0.44f},
                (Vector3){0.74f, 0.60f, 0.74f},
                (Color){94, 64, 50, 255});
        DrawSmallSphere((Vector3){x + 0.58f, 0.68f, z + 0.44f},
                        0.12f, WORLD_GOLD);
    }
    rlPopMatrix();
}

static void DrawTownRaidStaging(const CcLocalCourse *course, Vector3 focus)
{
    if (course == NULL || course->scene != CC_LOCAL_SCENE_STREET ||
        !course->alarm_active || !course->combat_origin_valid) return;
    Vector3 origin = course->combat_origin;
    if (!TerrainPointInPlayableWorld(origin.x, origin.z) ||
        !SceneryPointVisible(origin.x, origin.z, focus)) return;
    /* A broken muster line turns the broad road into an authored defence
       point. It remains flush with the ground, so combat physics and click
       paths stay exactly the same. */
    for (int32_t mark = -2; mark <= 2; ++mark) {
        float z = origin.z + (float)mark * 0.82f;
        float x = origin.x + ((mark & 1) != 0 ? 0.12f : -0.05f);
        float height = CcLocalTerrainHeightAt(x, z);
        DrawBox((Vector3){x, height + 0.025f, z},
                (Vector3){0.16f, 0.05f, 0.58f},
                mark == 0 ? WORLD_GOLD : WORLD_TEAL);
    }
    for (int32_t side = -1; side <= 1; side += 2) {
        float z = origin.z + (float)side * 2.35f;
        float height = CcLocalTerrainHeightAt(origin.x - 0.20f, z);
        DrawBox((Vector3){origin.x - 0.20f, height + 0.055f, z},
                (Vector3){1.05f, 0.11f, 0.42f},
                ShadeColor((Color){87, 76, 63, 255}, 0.84f));
    }
}

static void DrawDragonLairState(const CcDragon *dragon,
                                Vector3 scenery_focus)
{
    if (dragon == NULL ||
        !SceneryPointVisible(CC_LOCAL_DRAGON_CAVE_X,
                             CC_LOCAL_DRAGON_CAVE_Z,
                             scenery_focus)) return;
    const float cave_x = CC_LOCAL_DRAGON_CAVE_X;
    const float cave_z = CC_LOCAL_DRAGON_CAVE_Z - 1.35f;
    float ground = CcLocalTerrainHeightAt(cave_x, cave_z);
    Color cave_rock = (Color){47, 46, 54, 255};
    Color ash = (Color){49, 44, 48, 255};
    DrawBox((Vector3){cave_x, ground + 1.20f, cave_z + 0.34f},
            (Vector3){2.35f, 2.40f, 0.48f}, cave_rock);
    DrawBox((Vector3){cave_x, ground + 0.78f, cave_z + 0.04f},
            (Vector3){1.08f, 1.56f, 0.32f}, (Color){7, 8, 12, 255});
    DrawScenerySphere((Vector3){cave_x - 1.18f, ground + 0.54f,
                                 cave_z - 0.06f}, 0.78f, cave_rock);
    DrawScenerySphere((Vector3){cave_x + 1.15f, ground + 0.48f,
                                 cave_z - 0.02f}, 0.72f, cave_rock);

    int32_t coin_marks = 2 + dragon->crown_strength / 14;
    if (coin_marks > 9) coin_marks = 9;
    for (int32_t i = 0; i < coin_marks; ++i) {
        float x = cave_x + 1.80f + (float)(i % 3) * 0.28f;
        float z = cave_z - 0.62f + (float)(i / 3) * 0.25f;
        float y = CcLocalTerrainHeightAt(x, z);
        DrawSmallSphere((Vector3){x, y + 0.10f, z}, 0.13f,
                        i % 3 == 0 ? WORLD_VIOLET : WORLD_GOLD);
    }
    for (int32_t egg = 0; egg < dragon->egg_count && egg < 3; ++egg) {
        float x = cave_x - 1.72f + (float)egg * 0.48f;
        float z = cave_z - 0.48f + (float)(egg & 1) * 0.22f;
        float y = CcLocalTerrainHeightAt(x, z);
        DrawScenerySphere((Vector3){x, y + 0.25f, z}, 0.31f,
                          (Color){201, 187, 148, 255});
        DrawSmallSphere((Vector3){x + 0.06f, y + 0.31f, z - 0.19f},
                        0.08f, WORLD_GOLD);
    }

    if (!dragon->slain) return;
    for (int32_t patch = 0; patch < 6; ++patch) {
        float x = cave_x - 2.10f + (float)(patch % 3) * 1.25f;
        float z = cave_z - 1.05f + (float)(patch / 3) * 0.82f;
        float y = CcLocalTerrainHeightAt(x, z);
        DrawBox((Vector3){x, y + 0.025f, z},
                (Vector3){1.18f, 0.05f, 0.72f}, ash);
    }
    int32_t vents = dragon->regional_influence > 45 ? 3 :
                    dragon->regional_influence > 18 ? 2 : 1;
    for (int32_t vent = 0; vent < vents; ++vent) {
        float x = cave_x + 1.55f + (float)vent * 0.48f;
        float z = cave_z + 0.08f + (float)(vent & 1) * 0.36f;
        float y = CcLocalTerrainHeightAt(x, z);
        DrawCylinder((Vector3){x, y + 0.12f, z},
                     0.18f, 0.12f, 0.24f, 7,
                     dragon->regional_influence > 35 ?
                         (Color){154, 69, 54, 255} : ash);
    }
    DrawBox((Vector3){cave_x - 0.82f, ground + 0.62f, cave_z - 0.38f},
            (Vector3){0.20f, 1.24f, 0.20f}, WORLD_VIOLET);
    DrawSmallSphere((Vector3){cave_x - 0.82f, ground + 1.32f,
                               cave_z - 0.38f}, 0.19f, WORLD_GOLD);
}

static void DrawSettlementCreatures(const CcSim *sim,
                                    const CcSettlement *place,
                                    float clock, Vector3 scenery_focus)
{
    if (sim == NULL || place == NULL) return;
    const CcGoblinCult *goblins = &sim->goblins;
    const CcDragon *dragon = &sim->dragon;

    if (place->id == dragon->lair_settlement_id) {
        DrawDragonLairState(dragon, scenery_focus);
    }

    if (place->cow_adults + place->cow_calves > 0 &&
        SceneryPointVisible(63.0f, 38.3f, scenery_focus)) {
        CcCreaturePose cow_pose = CcCreatureSteppedPose(
            CC_CREATURE_COW, clock * 1.25f, true);
        CcCreatureRigPose controlled_cow;
        bool controlled = ResolveControlledCreatureGait(
            CREATURE_GAIT_STREET_COW, CC_CREATURE_RIG_COW, clock,
            0.34f, 0.48f, true, &controlled_cow);
        (void)DrawCreatureGait3D(
            CC_CREATURE_COW, cow_pose,
            TerrainWorldPoint(63.0f, 38.3f), 0.72f * PI, 0.84f,
            (Color){177, 162, 132, 255}, clock * 1.25f, true,
            controlled ? &controlled_cow : NULL);
    }

    if (place->id == goblins->lair_settlement_id) {
        CcCreaturePose scavenger_pose = CcCreatureSteppedPose(
            CC_CREATURE_GOBLIN_SCAVENGER, clock * 3.2f, true);
        CcCreaturePose raider_pose = CcCreatureSteppedPose(
            CC_CREATURE_GOBLIN_RAIDER, clock * 3.8f + PI, true);
        float patrol = sinf(clock * 0.42f) * 0.42f;
        if (SceneryPointVisible(26.5f, 52.0f, scenery_focus)) {
            (void)DrawCreature3D(
                CC_CREATURE_GOBLIN_SCAVENGER, scavenger_pose,
                TerrainWorldPoint(26.5f + patrol, 52.0f), -0.13f * PI, 1.28f,
                (Color){0});
        }
        if (SceneryPointVisible(29.0f, 51.5f, scenery_focus)) {
            (void)DrawCreature3D(
                CC_CREATURE_GOBLIN_RAIDER, raider_pose,
                TerrainWorldPoint(29.0f - patrol, 51.5f), -0.10f * PI, 1.34f,
                (Color){0});
        }
    }

    bool raid_at_place = goblins->tribute_target_id == place->id &&
        (goblins->tribute_phase == CC_GOBLIN_TRIBUTE_OUTBOUND ||
         goblins->tribute_phase == CC_GOBLIN_TRIBUTE_RETURNING);
    if (raid_at_place && SceneryPointVisible(47.2f, 34.2f,
                                             scenery_focus)) {
        CcCreaturePose raid_pose = CcCreatureSteppedPose(
            CC_CREATURE_GOBLIN_RAIDER, clock * 5.0f, true);
        CcCreaturePose scout_pose = CcCreatureSteppedPose(
            CC_CREATURE_GOBLIN_SCAVENGER, clock * 5.0f + PI, true);
        (void)DrawCreature3D(
            CC_CREATURE_GOBLIN_RAIDER, raid_pose,
            TerrainWorldPoint(46.7f, 34.0f), 0.62f * PI, 1.02f,
            (Color){0});
        (void)DrawCreature3D(
            CC_CREATURE_GOBLIN_SCAVENGER, scout_pose,
            TerrainWorldPoint(48.0f, 34.7f), 0.62f * PI, 0.94f,
            (Color){0});
    }

    if (place->id == dragon->lair_settlement_id && !dragon->slain &&
        SceneryPointVisible(20.7f, 52.6f, scenery_focus)) {
        CcCreaturePose dragon_pose = CC_CREATURE_POSE_REST;
        if (dragon->activity == CC_DRAGON_ACTIVITY_HUNTING) {
            dragon_pose = fmodf(clock, 1.0f) < 0.5f ?
                CC_CREATURE_POSE_STALK_A : CC_CREATURE_POSE_STALK_B;
        } else if (dragon->stolen_outstanding > 0 ||
            dragon->omen_days_remaining > 0) {
            dragon_pose = CC_CREATURE_POSE_THREAT;
        } else if (goblins->tribute_phase == CC_GOBLIN_TRIBUTE_TO_DRAGON) {
            dragon_pose = CC_CREATURE_POSE_IDLE;
        }
        float dragon_scale =
            dragon->life_stage == CC_DRAGON_STAGE_WHELP ? 0.52f :
            dragon->life_stage == CC_DRAGON_STAGE_WANDERER ? 0.72f :
            dragon->life_stage == CC_DRAGON_STAGE_DEEP_WYRM ? 1.14f :
            dragon->life_stage == CC_DRAGON_STAGE_UNCROWNED ? 0.84f : 0.94f;
        (void)DrawCreature3D(
            CC_CREATURE_DRAGON, dragon_pose,
            TerrainWorldPoint(20.7f, 52.6f), -0.48f * PI, dragon_scale,
            (Color){0});
    }
    if (place->id == dragon->lair_settlement_id &&
        goblins->tribute_phase == CC_GOBLIN_TRIBUTE_TO_DRAGON &&
        SceneryPointVisible(18.0f, 52.7f, scenery_focus)) {
        CcCreaturePose bearer_pose = CcCreatureSteppedPose(
            CC_CREATURE_GOBLIN_TRIBUTE_BEARER, clock * 3.4f, true);
        (void)DrawCreature3D(
            CC_CREATURE_GOBLIN_TRIBUTE_BEARER, bearer_pose,
            TerrainWorldPoint(18.0f, 52.7f), -0.38f * PI, 1.12f,
            (Color){0});
    }
}

void CcLocalDrawStreet3D(const CcSim *sim, const CcLocalAgent *agent,
                         const CcLocalCourse *course,
                         const CcLocalConvoyState *convoy, float clock,
                         RenderTexture2D target, Rectangle destination)
{
    CcLocalBindPlace(sim);
    const CcSettlement *place = CcSimSettlement(sim, sim->player.location_id);
    if (place == NULL) return;
    bool convoy_visible = convoy != NULL &&
        (convoy->phase == CC_LOCAL_CONVOY_DEPARTING ||
         convoy->phase == CC_LOCAL_CONVOY_ARRIVING);
    CcLocalAgent convoy_subject = agent != NULL ? *agent : (CcLocalAgent){0};
    if (convoy_visible) {
        convoy_subject.position = convoy->town_position;
        convoy_subject.facing_yaw = convoy->town_heading_yaw;
    }
    const CcLocalAgent *camera_subject = convoy_visible ?
        &convoy_subject : agent;
    const CcLocalPlaceProfile *profile =
        CcLocalPlaceProfileForSettlement(place);
    Camera3D base_camera = CcLocalStreetCameraInternal(
        camera_subject, clock, true, target.texture.height);
    Camera3D camera = CcLocalCombatCameraInternal(
        base_camera, camera_subject, course, clock, true,
        target.texture.height);
    if (camera_subject != NULL) {
        camera = KeepHeroInsideStreetFrame(
            camera,
            Vector3Add(camera_subject->position,
                       (Vector3){0.0f, 1.05f, 0.0f}),
            target.texture.height,
            (Rectangle){0.10f, 0.12f, 0.80f, 0.76f});
        camera = SnapCameraToArtPixels(camera, target.texture.height);
    }
    RememberPresentedCamera(CC_LOCAL_SCENE_STREET, camera, camera_subject,
                            target.texture.width, target.texture.height);
    const StreetCameraShot *camera_shot = StreetCameraShotAt(
        street_camera_rig.shot);
    ArtComposition street_art = camera_shot->art;
    street_art.light_profile = StreetLightProfile(
        place, street_art.light_profile);
    street_art.focal_point.y += CcLocalTerrainHeightAt(
        street_art.focal_point.x, street_art.focal_point.z);
    Color background = ArtLightBackground(street_art.light_profile);
    SetFaceRenderContext(camera, target.texture.width, target.texture.height);
    Vector3 scenery_focus = camera.target;
    Color kingdom = KingdomColor3D(sim, place->kingdom_id);
    BeginTextureMode(target);
    ClearBackground(background);
    BeginMode3D(camera);
    BeginWorldLighting(camera, &street_art);

    DrawExteriorTerrain(place, scenery_focus);
    DrawTownRaidStaging(course, scenery_focus);
    const CcLocalAgent *sightline_opponent =
        CombatCameraOpponent(course, agent);
    bool close_combat_sightline = course != NULL && agent != NULL &&
        sightline_opponent != NULL && course->alarm_active &&
        CombatHorizontalDistanceSquared(agent, sightline_opponent) <=
            7.0f * 7.0f;
    Vector3 player_sightline = agent != NULL ?
        Vector3Add(agent->position, (Vector3){0.0f, 1.02f, 0.0f}) :
        (Vector3){0};
    Vector3 opponent_sightline = sightline_opponent != NULL ?
        Vector3Add(sightline_opponent->position,
                   (Vector3){0.0f, 1.02f, 0.0f}) :
        player_sightline;
    for (int32_t i = 0; i < (int32_t)(sizeof(STREET_PLATFORMS) /
                                      sizeof(STREET_PLATFORMS[0])); ++i) {
        const NavPlatform *platform = &STREET_PLATFORMS[i];
        Rectangle footprint = {
            platform->x, platform->z, platform->width, platform->depth
        };
        if (!SceneryFootprintVisible(footprint, scenery_focus)) {
            continue;
        }
        float overlap = close_combat_sightline ?
            CameraStreetPlatformSubjectOverlap(
                camera, platform, player_sightline, opponent_sightline) :
            0.0f;
        /* The camera chooses the clearest available angle first. When a
           fighter is standing directly against a course block and no angle
           can create clean negative space, render only a low ground marker.
           Using the geometry itself means rails and caps cannot remain over
           the fighter's silhouette. */
        bool sightline_cut = overlap > 0.001f;
        Color color = CoursePlatformColor(platform->style);
        float base = PlatformBaseHeight(platform);
        float render_height = sightline_cut ?
            fminf(platform->height, 0.12f) : platform->height;
        float top = base + render_height;
        DrawBox((Vector3){platform->x + platform->width * 0.5f,
                          base + render_height * 0.5f,
                          platform->z + platform->depth * 0.5f},
                (Vector3){fmaxf(0.10f, platform->width - 0.10f),
                          render_height,
                          fmaxf(0.10f, platform->depth - 0.10f)}, color);
        DrawBox((Vector3){platform->x + platform->width * 0.5f,
                          top + 0.025f,
                          platform->z + platform->depth * 0.5f},
                (Vector3){fmaxf(0.10f, platform->width - 0.04f), 0.05f,
                          fmaxf(0.10f, platform->depth - 0.04f)},
                i == 0 ? (Color){124, 145, 131, 255} :
                         Fade(WORLD_GOLD, 0.70f));
    }
    DrawObstacleCourse(scenery_focus);
    if (!convoy_visible) DrawAgentPath(agent, false);
    Vector3 foreground_reveal_world = {
        camera_subject->position.x, camera_subject->position.y + 1.05f,
        camera_subject->position.z,
    };
    Vector2 foreground_reveal_center = GetWorldToScreenEx(
        foreground_reveal_world,
        camera, target.texture.width, target.texture.height);
    DrawWorldBuildings(kingdom, scenery_focus, profile, camera,
                       foreground_reveal_world, foreground_reveal_center,
                       target.texture.width, target.texture.height, clock);
    {
        float reveal_cut_height = foreground_reveal_world.y - 0.30f;
        SetWorldForegroundReveal(world_building_reveals[2].amount,
                                 reveal_cut_height);
        (void)DrawAuthoredMarket(place);
        SetWorldForegroundReveal(0.0f, reveal_cut_height);
    }
    DrawCastle(kingdom, profile, scenery_focus, camera,
               foreground_reveal_world, foreground_reveal_center,
               target.texture.width, target.texture.height, clock);
    if (SceneryFootprintVisible(CARRIAGE_FOOTPRINT, scenery_focus)) {
        if (!convoy_visible) {
            DrawCarriage3D(place);
            DrawStableHorseTeam(clock);
        }
    }
    if (convoy_visible) {
        DrawRoadCarriage(convoy->town_position,
                         CcPlayerCargoUsed(&sim->player), clock,
                         convoy->pace > 0.02f,
                         convoy->town_heading_yaw, true);
    }
    if (SceneryPointVisible(CC_LOCAL_NOTICE_X, CC_LOCAL_NOTICE_Z,
                            scenery_focus)) {
        DrawNotice3D(sim);
    }
    DrawWorldTrees(scenery_focus, kingdom);
    bool wayfarer_gate_sightline_cut = close_combat_sightline &&
        (camera.projection == CAMERA_PERSPECTIVE ||
         CameraWayfarerGateSubjectOverlap(
             camera, player_sightline, opponent_sightline) > 0.001f);
    DrawRoomLandmarks(place, kingdom, profile, scenery_focus,
                      wayfarer_gate_sightline_cut);
    DrawPlaceLandmarks(kingdom, scenery_focus);
    if (SceneryPointVisible(47.35f, 31.05f, scenery_focus)) {
        DrawJourneyAftermath3D(sim, place);
    }

    int32_t crates = place->stock[CC_GOOD_FOOD] / 12;
    if (crates > 4) crates = 4;
    for (int32_t i = 0; i < crates; ++i) {
        const NavPlatform *crate = &STREET_CRATE_PLATFORMS[i];
        Rectangle footprint = {
            crate->x, crate->z, crate->width, crate->depth
        };
        if (!SceneryFootprintVisible(footprint, scenery_focus)) continue;
        DrawBox((Vector3){crate->x + crate->width * 0.5f,
                          PlatformBaseHeight(crate) + crate->height * 0.5f,
                          crate->z + crate->depth * 0.5f},
                (Vector3){0.62f, 0.60f, 0.62f},
                (Color){177, 116, 55, 255});
    }
    const CcDungeon *dungeon = DungeonAt(sim, place->id);
    if (dungeon != NULL &&
        SceneryFootprintVisible(DUNGEON_FOOTPRINT, scenery_focus)) {
        DrawDungeon3D(dungeon);
    }
    DrawSettlementCreatures(sim, place, clock, scenery_focus);

    DrawVisibleNpcFigure3D(
        TerrainWorldPoint(STREET_PEOPLE[0].x, STREET_PEOPLE[0].y),
        0.96f, -0.55f, UINT32_C(0x73747201), CC_NPC_ROLE_MERCHANT,
        (Color){223, 151, 68, 255}, clock * 1.2f, CC_TRAVERSAL_IDLE,
        scenery_focus);
    DrawVisibleNpcFigure3D(
        TerrainWorldPoint(STREET_PEOPLE[1].x, STREET_PEOPLE[1].y),
        1.02f, 1.70f, UINT32_C(0x73747202), CC_NPC_ROLE_GUARD,
        kingdom, clock + 1.0f, CC_TRAVERSAL_IDLE, scenery_focus);
    DrawVisibleNpcFigure3D(
        TerrainWorldPoint(STREET_PEOPLE[2].x, STREET_PEOPLE[2].y),
        0.92f, 0.35f, UINT32_C(0x73747203), CC_NPC_ROLE_LABORER,
        (Color){97, 154, 137, 255}, clock + 2.0f, CC_TRAVERSAL_IDLE,
        scenery_focus);
    DrawVisibleNpcFigure3D(
        TerrainWorldPoint(STREET_PEOPLE[3].x, STREET_PEOPLE[3].y),
        0.88f, 2.40f, UINT32_C(0x73747204), CC_NPC_ROLE_HEALER,
        (Color){168, 112, 128, 255}, clock * 0.8f + 3.0f,
        CC_TRAVERSAL_IDLE, scenery_focus);
    bool hungry_crowd = place->hunger >= 30;
    DrawVisibleNpcFigure3D(
        TerrainWorldPoint(STREET_PEOPLE[4].x, STREET_PEOPLE[4].y),
        0.82f, -0.40f, UINT32_C(0x73747205),
        hungry_crowd ? CC_NPC_ROLE_REFUGEE : CC_NPC_ROLE_TRAVELLER,
        hungry_crowd ? WORLD_DANGER : kingdom, clock * 0.6f,
        CC_TRAVERSAL_IDLE, scenery_focus);
    bool underworld_present = HasSmugglerRoad(sim, place->id) ||
                              place->security < 50;
    DrawVisibleNpcFigure3D(
        TerrainWorldPoint(STREET_PEOPLE[5].x, STREET_PEOPLE[5].y),
        0.88f, 2.75f, UINT32_C(0x73747206),
        underworld_present ? CC_NPC_ROLE_SCOUT : CC_NPC_ROLE_TRAVELLER,
        underworld_present ? WORLD_VIOLET : kingdom, clock * 0.7f,
        CC_TRAVERSAL_IDLE, scenery_focus);
    /* Each outer room has one resident whose job explains the place at a
       glance. Their spacing leaves the authored travel lanes clear. */
    DrawVisibleNpcFigure3D(
        TerrainWorldPoint(10.15f, 31.90f), 0.88f, -0.85f,
        UINT32_C(0x64697301), CC_NPC_ROLE_LABORER,
        (Color){143, 118, 65, 255}, clock * 0.52f + 0.5f,
        CC_TRAVERSAL_IDLE, scenery_focus);
    DrawVisibleNpcFigure3D(
        TerrainWorldPoint(24.10f, 53.20f), 0.90f, 1.50f,
        UINT32_C(0x64697302), CC_NPC_ROLE_SCOUT,
        (Color){103, 103, 112, 255}, clock * 0.48f + 1.1f,
        CC_TRAVERSAL_IDLE, scenery_focus);
    DrawVisibleNpcFigure3D(
        TerrainWorldPoint(35.20f, 26.65f), 0.94f, -0.15f,
        UINT32_C(0x64697303), CC_NPC_ROLE_LABORER,
        (Color){174, 94, 53, 255}, clock * 0.60f + 1.7f,
        CC_TRAVERSAL_IDLE, scenery_focus);
    DrawVisibleNpcFigure3D(
        TerrainWorldPoint(42.75f, 51.05f), 0.92f, 2.60f,
        UINT32_C(0x64697304), CC_NPC_ROLE_TRAVELLER,
        (Color){117, 145, 116, 255}, clock * 0.44f + 2.3f,
        CC_TRAVERSAL_IDLE, scenery_focus);
    DrawVisibleNpcFigure3D(
        TerrainWorldPoint(74.65f, 31.95f), 1.02f, 1.50f,
        UINT32_C(0x64697305), CC_NPC_ROLE_GUARD, kingdom,
        clock * 0.40f + 2.9f, CC_TRAVERSAL_IDLE, scenery_focus);
    DrawVisibleNpcFigure3D(
        TerrainWorldPoint(84.35f, 50.70f), 0.88f, -1.35f,
        UINT32_C(0x64697306), CC_NPC_ROLE_LABORER,
        (Color){161, 128, 68, 255}, clock * 0.47f + 3.5f,
        CC_TRAVERSAL_IDLE, scenery_focus);
    if (sim->resolved_journey_outcome != CC_JOURNEY_OUTCOME_NONE &&
        sim->journey.destination_id == place->id) {
        DrawVisibleNpcFigure3D(
            TerrainWorldPoint(46.80f, 31.15f), 0.86f, -1.10f,
            UINT32_C(0x61667401), CC_NPC_ROLE_GUARD, WORLD_TEAL,
            clock * 0.64f + 1.4f, CC_TRAVERSAL_IDLE, scenery_focus);
        if (sim->resolved_journey_outcome == CC_JOURNEY_OUTCOME_COMBAT) {
            DrawVisibleNpcFigure3D(
                TerrainWorldPoint(47.65f, 30.55f), 0.82f, -0.85f,
                UINT32_C(0x61667402), CC_NPC_ROLE_HEALER, WORLD_GOLD,
                clock * 0.58f + 2.1f, CC_TRAVERSAL_IDLE, scenery_focus);
        }
    }
    DrawCourseRunners(course, scenery_focus);
    if (course != NULL && agent->combat.target_index >= 0 &&
        agent->combat.target_index < CC_LOCAL_RAIDER_COUNT) {
        DrawSelectedTarget(
            &course->raiders[agent->combat.target_index]);
    }
    if (!convoy_visible) {
        DrawRobotShell(agent);
        DrawCombatSword(agent);
        DrawCombatSkillTell(agent);
        DrawCombatImpact(agent);
    }
    EndWorldLighting();
    EndMode3D();
    DrawTargetAtmosphere(target, clock);
    EndTextureMode();
    PresentTarget(target, destination);
    bool alarm_active = course != NULL && course->alarm_active;
    bool combat_presentation = course != NULL && course->alarm_active &&
                               camera.projection == CAMERA_PERSPECTIVE;

    int32_t room_index = StreetCameraBaseShot(street_camera_rig.shot);
    int32_t room_count = (int32_t)(sizeof(STREET_CAMERA_SHOTS) /
                                   sizeof(STREET_CAMERA_SHOTS[0]));
    if (!combat_presentation && room_index >= 0 && room_index < room_count) {
        const char *room_name = profile->room_name[room_index];
        int32_t title_width = CcOverlayMeasureText(room_name, 10);
        DrawRectangleRounded(
            ViewportRectangle(destination, 11.0f, 10.0f,
                              (float)title_width + 16.0f, 18.0f),
            0.24f, 4, (Color){4, 10, 14, 202});
        DrawViewportText(room_name, destination, 19, 14, 10, WORLD_GOLD);
    }
    if (!alarm_active && !convoy_visible) {
        DrawStreetTraversalPortals(agent, camera, destination,
                                   target.texture.width,
                                   target.texture.height);
    }

    WorldLabel labels[20];
    int32_t count = 0;
    if (AgentNearLabel(agent, 50.0f, 21.0f, 8.0f)) {
        labels[count++] = (WorldLabel){{50.0f,
                                        TerrainFootprintHeight(
                                            WORLD_BUILDINGS[2].footprint) + 4.75f,
                                        21.0f},
                                       profile->primary_hall, WORLD_GOLD};
    }
    if (AgentNearLabel(agent, 36.80f, 31.70f, 7.0f)) {
        labels[count++] = (WorldLabel){{36.80f,
                                        TerrainFootprintHeight(
                                            CARRIAGE_FOOTPRINT) + 2.28f,
                                        31.70f},
                                       "Carriage", WORLD_GOLD};
    }
    if (AgentNearLabel(agent, CC_LOCAL_NOTICE_X, CC_LOCAL_NOTICE_Z, 6.0f)) {
        labels[count++] = (WorldLabel){{CC_LOCAL_NOTICE_X,
                                        CcLocalTerrainHeightAt(
                                            CC_LOCAL_NOTICE_X,
                                            CC_LOCAL_NOTICE_Z) + 1.82f,
                                        CC_LOCAL_NOTICE_Z},
                                       profile->notice_board, WORLD_INK};
    }
    if (AgentNearLabel(agent, 11.80f, 0.82f, 7.0f)) {
        labels[count++] = (WorldLabel){{11.80f,
                                        CcLocalTerrainHeightAt(11.80f, 0.82f) +
                                            2.05f,
                                        0.82f},
                                       profile->training_yard, WORLD_GOLD};
    }
    if (AgentNearLabel(agent, 11.28f, 9.72f, 7.0f)) {
        labels[count++] = (WorldLabel){{11.28f,
                                        CcLocalTerrainHeightAt(11.28f, 9.72f) +
                                            1.12f,
                                        9.72f},
                                       "Swimming trench", WORLD_TEAL};
    }
    if (AgentNearLabel(agent, 78.50f, 17.50f, 10.0f)) {
        labels[count++] = (WorldLabel){{78.50f,
                                        CcLocalTerrainHeightAt(78.50f, 17.50f) +
                                            12.10f,
                                        17.50f},
                                       profile->compound, kingdom};
    }
    for (int32_t i = 0;
         i < CC_LOCAL_PLACE_LANDMARK_COUNT && count < 20; ++i) {
        const CcLocalPlaceLandmark *landmark =
            ActivePlaceLandmarkAt(i);
        if (landmark == NULL) continue;
        float center_x = landmark->x + landmark->width * 0.5f;
        float center_z = landmark->z + landmark->depth * 0.5f;
        if (!AgentNearLabel(agent, center_x, center_z, 7.5f)) continue;
        Rectangle footprint = PlaceLandmarkFootprint(landmark);
        labels[count++] = (WorldLabel){
            {center_x, TerrainFootprintHeight(footprint) +
                       landmark->height + 0.42f, center_z},
            landmark->name,
            landmark->family == CC_LOCAL_LANDMARK_EXPEDITION ?
                WORLD_VIOLET : kingdom};
    }
    for (int32_t i = 0;
         i < CC_LOCAL_PLACE_ROAD_COUNT && count < 20; ++i) {
        const CcLocalPlaceRoad *road = ActivePlaceRoadAt(i);
        if (road == NULL) continue;
        float center_x = road->x + road->width * 0.5f;
        float center_z = road->z + road->depth * 0.5f;
        if (!AgentNearLabel(agent, center_x, center_z, 5.5f)) continue;
        labels[count++] = (WorldLabel){
            {center_x, CcLocalTerrainHeightAt(center_x, center_z) + 0.42f,
             center_z},
            road->name, WORLD_MUTED};
    }
    if (sim->resolved_journey_outcome != CC_JOURNEY_OUTCOME_NONE &&
        sim->journey.destination_id == place->id &&
        AgentNearLabel(agent, 47.35f, 31.05f, 7.0f)) {
        labels[count++] = (WorldLabel){
            {47.35f, CcLocalTerrainHeightAt(47.35f, 31.05f) + 2.05f,
             31.05f},
            sim->resolved_journey_outcome == CC_JOURNEY_OUTCOME_COMBAT ?
                "Road guards" : "Company token",
            sim->resolved_journey_outcome == CC_JOURNEY_OUTCOME_COMBAT ?
                WORLD_TEAL : WORLD_VIOLET};
    }
    if (course != NULL && course->situation_witness_active) {
        const CcSituation *situation = CcSimSituation(
            sim, course->situation_witness_id);
        if (situation != NULL && situation->affected_name[0] != '\0' &&
            AgentNearLabel(agent, course->situation_witness.position.x,
                           course->situation_witness.position.z, 6.0f)) {
            labels[count++] = (WorldLabel){
                {course->situation_witness.position.x,
                 course->situation_witness.position.y + 2.18f,
                 course->situation_witness.position.z},
                situation->affected_name,
                situation->status == CC_SITUATION_RESOLVED ? WORLD_TEAL :
                sim->player.accepted_situation_id == situation->id ?
                    WORLD_GOLD : WORLD_DANGER};
        }
    }
    if (dungeon != NULL &&
        AgentNearLabel(agent, CC_LOCAL_DUNGEON_X,
                       CC_LOCAL_DUNGEON_Z, 8.0f)) {
        labels[count++] = (WorldLabel){{CC_LOCAL_DUNGEON_X,
                                        TerrainFootprintHeight(
                                            DUNGEON_FOOTPRINT) + 3.38f,
                                        CC_LOCAL_DUNGEON_Z - 0.70f},
                                       dungeon->name, WORLD_VIOLET};
    }
    if (place->id == sim->dragon.lair_settlement_id &&
        AgentNearLabel(agent, CC_LOCAL_DRAGON_CAVE_X,
                       CC_LOCAL_DRAGON_CAVE_Z, 9.0f)) {
        labels[count++] = (WorldLabel){
            {CC_LOCAL_DRAGON_CAVE_X,
             CcLocalTerrainHeightAt(CC_LOCAL_DRAGON_CAVE_X,
                                    CC_LOCAL_DRAGON_CAVE_Z) + 2.75f,
             CC_LOCAL_DRAGON_CAVE_Z},
            sim->dragon.slain ? "Afterdragon cave" : "Dragon cave",
            sim->dragon.slain ? WORLD_VIOLET : WORLD_DANGER};
    }
    if (!alarm_active) {
        DrawLabels(labels, count, camera, destination);
    }
    if (course != NULL && course->alarm_active) {
        DrawCombatBar(agent, camera, destination, WORLD_TEAL);
        if (combat_presentation) {
            DrawCombatBar(CombatCameraOpponent(course, agent), camera,
                          destination, WORLD_GOLD);
        } else {
            for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
                DrawCombatBar(&course->runners[i].agent, camera,
                              destination, course->runners[i].marker_color);
            }
            for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
                DrawCombatBar(&course->raiders[i], camera, destination,
                              i == agent->combat.target_index ? WORLD_GOLD :
                                                               WORLD_DANGER);
            }
        }
    }
    if (draw_hero_rig_debug &&
        agent->morphology == CC_MORPHOLOGY_BIPED) {
        DrawViewportText(
            TextFormat("BIOMECHANICAL BIPED / %d JOINTS / %s / %s / MUSCLES + LIGAMENTS",
                       agent->humanoid.body.morphology.joint_count,
                       CcLocalTraversalName(agent->traversal),
                       CcHumanoidActionName(agent->humanoid.action)),
            destination, 18, 18, 10, WORLD_TEAL);
    } else if (draw_hero_rig_debug) {
        DrawViewportText(
            TextFormat("ROBOTIC %s / %d LEGS / %s / CONTACT IK",
                       CcLocalAgentMorphologyName(agent),
                       agent->limb_rig.morphology.limb_count,
                       CcLocalTraversalName(agent->traversal)),
            destination, 18, 18, 10, WORLD_TEAL);
    }
    DrawFixedCameraFade(&street_camera_rig, destination);
}

static CcNpcRole InteriorKeeperRole(CcSettlementFunction function)
{
    switch (function) {
        case CC_SETTLEMENT_FARMING: return CC_NPC_ROLE_LABORER;
        case CC_SETTLEMENT_MINING: return CC_NPC_ROLE_LABORER;
        case CC_SETTLEMENT_MARKET: return CC_NPC_ROLE_MERCHANT;
        case CC_SETTLEMENT_FORTRESS: return CC_NPC_ROLE_GUARD;
        case CC_SETTLEMENT_CAPITAL: return CC_NPC_ROLE_MERCHANT;
        case CC_SETTLEMENT_DUNGEON_TOWN: return CC_NPC_ROLE_SCOUT;
    }
    return CC_NPC_ROLE_MERCHANT;
}

static Color InteriorWallColor(const CcLocalPlaceProfile *profile,
                               Color kingdom)
{
    if (profile == NULL) return WORLD_EARTH_SHADOW;
    switch (profile->function) {
        case CC_SETTLEMENT_FARMING:
            return BlendColor(WORLD_EARTH_SHADOW, WORLD_CROP, 0.18f);
        case CC_SETTLEMENT_MINING:
            return BlendColor(WORLD_STONE_SHADOW,
                              WORLD_METAL_SHADOW, 0.28f);
        case CC_SETTLEMENT_MARKET:
            return BlendColor(WORLD_EARTH_SHADOW, WORLD_TEAL, 0.10f);
        case CC_SETTLEMENT_FORTRESS:
            return BlendColor(WORLD_STONE_SHADOW, kingdom, 0.18f);
        case CC_SETTLEMENT_CAPITAL:
            return BlendColor(WORLD_STONE, WORLD_GOLD, 0.12f);
        case CC_SETTLEMENT_DUNGEON_TOWN:
            return BlendColor(WORLD_STONE_SHADOW, WORLD_VIOLET, 0.20f);
    }
    return WORLD_EARTH_SHADOW;
}

static void DrawInteriorServiceMark(const CcLocalPlaceProfile *profile,
                                    Color accent)
{
    if (profile == NULL) return;
    const float z = 0.586f;
    const float x = 6.55f;
    switch (profile->function) {
        case CC_SETTLEMENT_FARMING:
            for (int32_t stalk = -1; stalk <= 1; ++stalk) {
                DrawBox((Vector3){x + (float)stalk * 0.30f, 1.48f, z},
                        (Vector3){0.09f, 0.88f, 0.035f}, accent);
                DrawBox((Vector3){x + (float)stalk * 0.30f + 0.10f,
                                  1.54f, z + 0.006f},
                        (Vector3){0.24f, 0.08f, 0.040f}, accent);
            }
            break;
        case CC_SETTLEMENT_MINING:
            DrawBox((Vector3){x, 1.48f, z},
                    (Vector3){1.10f, 0.22f, 0.040f}, accent);
            DrawBox((Vector3){x, 1.48f, z},
                    (Vector3){0.22f, 1.10f, 0.040f}, accent);
            DrawBox((Vector3){x, 1.48f, z + 0.004f},
                    (Vector3){0.62f, 0.62f, 0.045f},
                    ShadeColor(accent, 0.72f));
            break;
        case CC_SETTLEMENT_MARKET:
            DrawBox((Vector3){x, 1.62f, z},
                    (Vector3){1.16f, 0.10f, 0.040f}, accent);
            DrawBox((Vector3){x, 1.37f, z},
                    (Vector3){0.10f, 0.60f, 0.040f}, accent);
            DrawBox((Vector3){x - 0.42f, 1.30f, z},
                    (Vector3){0.48f, 0.10f, 0.040f}, accent);
            DrawBox((Vector3){x + 0.42f, 1.30f, z},
                    (Vector3){0.48f, 0.10f, 0.040f}, accent);
            break;
        case CC_SETTLEMENT_FORTRESS:
            DrawBox((Vector3){x, 1.48f, z},
                    (Vector3){0.72f, 1.08f, 0.040f}, accent);
            DrawBox((Vector3){x, 1.48f, z + 0.004f},
                    (Vector3){1.16f, 0.24f, 0.045f},
                    ShadeColor(accent, 0.74f));
            break;
        case CC_SETTLEMENT_CAPITAL:
            DrawBox((Vector3){x, 1.28f, z},
                    (Vector3){1.22f, 0.22f, 0.040f}, accent);
            for (int32_t point = -1; point <= 1; ++point) {
                DrawBox((Vector3){x + (float)point * 0.42f,
                                  1.58f + (point == 0 ? 0.12f : 0.0f), z},
                        (Vector3){0.24f,
                                  point == 0 ? 0.78f : 0.56f, 0.040f},
                        accent);
            }
            break;
        case CC_SETTLEMENT_DUNGEON_TOWN:
            DrawBox((Vector3){x, 1.46f, z},
                    (Vector3){0.72f, 0.86f, 0.040f}, accent);
            DrawBox((Vector3){x, 1.90f, z},
                    (Vector3){1.02f, 0.12f, 0.040f}, accent);
            DrawBox((Vector3){x, 1.03f, z},
                    (Vector3){0.24f, 0.18f, 0.040f}, accent);
            break;
    }
}

void CcLocalDrawInterior3D(const CcSim *sim, const CcLocalAgent *agent,
                           float clock, RenderTexture2D target,
                           Rectangle destination)
{
    CcLocalBindPlace(sim);
    const CcSettlement *place = CcSimSettlement(sim, sim->player.location_id);
    if (place == NULL) return;
    const CcLocalPlaceProfile *profile =
        CcLocalPlaceProfileForSettlement(place);
    Color kingdom = KingdomColor3D(sim, place->kingdom_id);
    Color identity = PlaceIdentityAccent(profile, kingdom);
    Color wall = InteriorWallColor(profile, kingdom);
    Camera3D camera = LocalCamera(true, agent->position);
    RememberPresentedCamera(CC_LOCAL_SCENE_MARKET, camera, agent,
                            target.texture.width, target.texture.height);
    Color background = ArtLightBackground(
        INTERIOR_ART_COMPOSITION.light_profile);
    SetFaceRenderContext(camera, target.texture.width, target.texture.height);
    BeginTextureMode(target);
    ClearBackground(background);
    BeginMode3D(camera);
    BeginWorldLighting(camera, &INTERIOR_ART_COMPOSITION);
    /* Six broad floor flags replace the old 63-box checkerboard. Their quiet
       value rhythm leaves the actor silhouettes and goods as the room detail. */
    Color floor_dark = BlendColor(WORLD_EARTH_SHADOW, identity, 0.12f);
    Color floor_light = BlendColor(WORLD_EARTH, identity, 0.14f);
    DrawBox((Vector3){4.50f, -0.055f, 3.50f}, (Vector3){9.0f, 0.11f, 7.0f},
            floor_dark);
    for (int32_t row = 0; row < 2; ++row) {
        for (int32_t column = 0; column < 3; ++column) {
            DrawBox((Vector3){1.50f + (float)column * 3.0f, 0.006f,
                              1.75f + (float)row * 3.5f},
                    (Vector3){2.86f, 0.022f, 3.36f},
                    ((row + column) & 1) != 0 ? floor_light :
                                                        ShadeColor(floor_light,
                                                                   0.91f));
        }
    }
    DrawAgentPath(agent, true);
    DrawBox((Vector3){4.50f, 1.30f, 0.25f}, (Vector3){9.0f, 2.60f, 0.50f},
            wall);
    DrawBox((Vector3){0.25f, 1.30f, 3.50f}, (Vector3){0.50f, 2.60f, 7.0f},
            ShadeColor(wall, 0.86f));
    Color interior_timber = BlendColor(WORLD_WOOD_SHADOW, identity, 0.08f);
    Color interior_trim = BlendColor(WORLD_WOOD, identity, 0.16f);
    DrawBox((Vector3){4.50f, 0.54f, 0.515f},
            (Vector3){8.86f, 0.10f, 0.045f},
            interior_timber);
    DrawBox((Vector3){4.50f, 2.39f, 0.525f},
            (Vector3){8.86f, 0.13f, 0.065f}, interior_timber);
    for (int32_t bay = 0; bay < 5; ++bay) {
        float post_x = 0.82f + (float)bay * 1.92f;
        DrawBox((Vector3){post_x, 1.44f, 0.525f},
                (Vector3){0.14f, 1.82f, 0.07f}, interior_timber);
    }
    DrawBox((Vector3){0.515f, 2.39f, 3.50f},
            (Vector3){0.065f, 0.13f, 6.86f}, interior_timber);
    for (int32_t bay = 0; bay < 3; ++bay) {
        float post_z = 1.02f + (float)bay * 2.30f;
        DrawBox((Vector3){0.525f, 1.44f, post_z},
                (Vector3){0.07f, 1.82f, 0.14f}, interior_timber);
    }
    DrawBox((Vector3){6.55f, 1.42f, 0.525f},
            (Vector3){2.05f, 1.56f, 0.055f},
            ShadeColor(wall, 0.66f));
    DrawBox((Vector3){6.55f, 2.21f, 0.535f},
            (Vector3){2.24f, 0.10f, 0.065f}, identity);
    DrawInteriorServiceMark(profile, identity);
    DrawBox((Vector3){MARKET_COUNTER_FOOTPRINT.x +
                      MARKET_COUNTER_FOOTPRINT.width * 0.5f,
                      0.46f,
                      MARKET_COUNTER_FOOTPRINT.y +
                      MARKET_COUNTER_FOOTPRINT.height * 0.5f},
            (Vector3){MARKET_COUNTER_FOOTPRINT.width, 0.92f,
                      MARKET_COUNTER_FOOTPRINT.height},
            BlendColor(WORLD_WOOD, identity, 0.14f));
    for (int32_t panel = 0; panel < 3; ++panel) {
        float panel_x = MARKET_COUNTER_FOOTPRINT.x + 0.38f +
                        (float)panel * 0.66f;
        DrawBox((Vector3){panel_x, 0.47f,
                          MARKET_COUNTER_FOOTPRINT.y +
                              MARKET_COUNTER_FOOTPRINT.height + 0.035f},
                (Vector3){0.50f, 0.55f, 0.055f},
                panel == 1 ? (Color){85, 53, 43, 255} :
                             (Color){100, 62, 44, 255});
    }
    for (int32_t end = 0; end < 2; ++end) {
        float post_x = MARKET_COUNTER_FOOTPRINT.x +
                       (end == 0 ? 0.10f :
                                   MARKET_COUNTER_FOOTPRINT.width - 0.10f);
        DrawBox((Vector3){post_x, 0.48f,
                          MARKET_COUNTER_FOOTPRINT.y +
                              MARKET_COUNTER_FOOTPRINT.height + 0.065f},
                (Vector3){0.16f, 0.78f, 0.12f}, interior_timber);
    }
    DrawBox((Vector3){MARKET_COUNTER_FOOTPRINT.x +
                      MARKET_COUNTER_FOOTPRINT.width * 0.5f,
                      0.94f,
                      MARKET_COUNTER_FOOTPRINT.y +
                      MARKET_COUNTER_FOOTPRINT.height * 0.5f},
            (Vector3){MARKET_COUNTER_FOOTPRINT.width + 0.12f, 0.10f,
                      MARKET_COUNTER_FOOTPRINT.height + 0.12f},
            BlendColor(WORLD_WOOD_LIGHT, identity, 0.18f));
    /* A real open shelf replaces the former solid obstacle block. The same
       collision footprint remains authoritative, but the visible model now
       has legs, boards, gaps, and stock silhouettes. */
    float shelf_x = MARKET_SHELF_FOOTPRINT.x +
                    MARKET_SHELF_FOOTPRINT.width * 0.5f;
    float shelf_z = MARKET_SHELF_FOOTPRINT.y +
                    MARKET_SHELF_FOOTPRINT.height * 0.5f;
    for (int32_t end = 0; end < 2; ++end) {
        float post_z = MARKET_SHELF_FOOTPRINT.y +
                       (end == 0 ? 0.11f :
                                   MARKET_SHELF_FOOTPRINT.height - 0.11f);
        for (int32_t side = -1; side <= 1; side += 2) {
            DrawBox((Vector3){shelf_x + (float)side * 0.25f, 0.96f, post_z},
                    (Vector3){0.13f, 1.92f, 0.13f}, interior_timber);
        }
    }
    for (int32_t shelf = 0; shelf < 4; ++shelf) {
        float shelf_y = 0.18f + (float)shelf * 0.55f;
        DrawBox((Vector3){shelf_x, shelf_y, shelf_z},
                (Vector3){MARKET_SHELF_FOOTPRINT.width + 0.08f, 0.10f,
                          MARKET_SHELF_FOOTPRINT.height + 0.06f},
                shelf == 3 ? interior_trim : (Color){102, 67, 47, 255});
    }
    DrawBox((Vector3){1.55f, 1.05f, 6.54f}, (Vector3){0.82f, 2.10f, 0.08f},
            (Color){37, 28, 30, 255});

    /* Warm wall lamps shape the room into foreground, trade counter, and
       stock-wall zones without adding a second lighting system. */
    const float lamp_x[] = {3.05f, 8.10f};
    for (int32_t lamp = 0; lamp < 2; ++lamp) {
        DrawBox((Vector3){lamp_x[lamp], 1.76f, 0.62f},
                (Vector3){0.08f, 0.58f, 0.08f}, interior_timber);
        DrawBox((Vector3){lamp_x[lamp], 2.04f, 0.73f},
                (Vector3){0.34f, 0.08f, 0.28f}, interior_trim);
        DrawSmallSphere((Vector3){lamp_x[lamp], 1.78f, 0.76f},
                        0.14f, (Color){224, 160, 67, 255});
    }
    DrawBox((Vector3){4.48f, 0.032f, 3.88f},
            (Vector3){1.64f, 0.035f, 3.20f},
            ShadeColor(identity, 0.62f));
    DrawBox((Vector3){4.48f, 0.050f, 2.36f},
            (Vector3){1.84f, 0.045f, 0.11f}, identity);

    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        float stock = fminf((float)place->stock[good] / 54.0f, 1.0f);
        if (stock <= 0.01f) continue;
        Color color = good == CC_GOOD_FOOD ? (Color){158, 116, 62, 255} :
                      good == CC_GOOD_MATERIAL ?
                        (Color){170, 139, 112, 255} : WORLD_TEAL;
        float display_x = 2.55f + (float)good * 1.08f;
        if (good == CC_GOOD_FOOD) {
            int32_t sacks = stock > 0.66f ? 3 : stock > 0.30f ? 2 : 1;
            for (int32_t sack = 0; sack < sacks; ++sack) {
                float side = (float)sack - (float)(sacks - 1) * 0.5f;
                DrawCharacterEllipsoid(
                    (Vector3){display_x + side * 0.25f,
                              0.22f + (sack == 2 ? 0.14f : 0.0f), 1.18f},
                    (Vector3){0.18f, 0.29f, 0.17f}, color);
            }
            DrawBox((Vector3){display_x, 0.55f, 1.18f},
                    (Vector3){0.48f, 0.07f, 0.28f},
                    ShadeColor(color, 0.76f));
        } else if (good == CC_GOOD_MATERIAL) {
            int32_t planks = stock > 0.60f ? 4 : stock > 0.28f ? 3 : 2;
            for (int32_t plank = 0; plank < planks; ++plank) {
                DrawBox((Vector3){display_x,
                                  0.10f + (float)plank * 0.15f,
                                  1.18f + (float)(plank & 1) * 0.06f},
                        (Vector3){0.82f, 0.11f, 0.24f},
                        ShadeColor(color,
                                   0.88f + (float)plank * 0.04f));
            }
        } else {
            int32_t tools = stock > 0.55f ? 3 : stock > 0.20f ? 2 : 1;
            DrawBox((Vector3){display_x, 0.28f, 1.18f},
                    (Vector3){0.76f, 0.56f, 0.16f},
                    ShadeColor(interior_timber, 0.86f));
            for (int32_t tool = 0; tool < tools; ++tool) {
                float tool_x = display_x - 0.24f + (float)tool * 0.24f;
                DrawCylinder((Vector3){tool_x, 0.18f, 1.08f},
                             0.045f, 0.045f, 0.72f, 7,
                             (Color){99, 67, 45, 255});
                DrawBox((Vector3){tool_x, 0.88f, 1.08f},
                        (Vector3){0.24f, 0.12f, 0.13f}, color);
            }
        }
    }
    for (int32_t shelf_good = 0; shelf_good < 5; ++shelf_good) {
        float y = 0.42f + (float)(shelf_good % 3) * 0.55f;
        float z = MARKET_SHELF_FOOTPRINT.y + 0.48f +
                  (float)shelf_good * 0.58f;
        Color stock_color = shelf_good % 3 == 0 ? WORLD_GOLD :
                            shelf_good % 3 == 1 ?
                                (Color){136, 100, 67, 255} : WORLD_TEAL;
        DrawBox((Vector3){MARKET_SHELF_FOOTPRINT.x +
                              MARKET_SHELF_FOOTPRINT.width + 0.05f,
                          y, z},
                (Vector3){0.42f,
                          0.25f + (float)(shelf_good & 1) * 0.12f,
                          0.30f},
                ShadeColor(stock_color, 0.82f));
    }
    CcNpcAppearance keeper = profile->function == CC_SETTLEMENT_MARKET ?
        CcNpcMaraAppearance() :
        CcNpcAppearanceGenerate(profile->keeper_seed,
                                InteriorKeeperRole(profile->function),
                                identity);
    DrawNpcAppearanceFigure3D(
        (Vector3){MARKET_PEOPLE[0].x, 0.0f, MARKET_PEOPLE[0].y},
        1.02f, 2.75f, &keeper, clock, CC_TRAVERSAL_IDLE);
    DrawRobotShell(agent);
    DrawCombatSword(agent);
    DrawCombatSkillTell(agent);
    EndWorldLighting();
    EndMode3D();
    EndTextureMode();
    PresentTarget(target, destination);
    WorldLabel labels[3];
    int32_t label_count = 0;
    if (AgentNearLabel(agent, MARKET_PEOPLE[0].x,
                       MARKET_PEOPLE[0].y, 5.0f)) {
        labels[label_count++] = (WorldLabel){
            {6.55f, 2.05f, 1.60f}, profile->keeper_name, identity};
        labels[label_count++] = (WorldLabel){
            {6.55f, 2.36f, 0.62f}, profile->interior_service, WORLD_MUTED};
    }
    if (AgentNearLabel(agent, 1.55f, 6.54f, 3.5f)) {
        labels[label_count++] = (WorldLabel){
            {1.55f, 2.25f, 6.54f}, "Exit", WORLD_MUTED};
    }
    DrawLabels(labels, label_count, camera, destination);
    if (draw_hero_rig_debug &&
        agent->morphology == CC_MORPHOLOGY_BIPED) {
        DrawViewportText(
            TextFormat("BIO %s / %s / %s + %s / %.0f%% MUSCLE",
                       CcHumanoidPoseOwnerName(agent->humanoid.pose_owner),
                       CcMotionClipName(
                           agent->humanoid.motion.clip != NULL ?
                           agent->humanoid.motion.clip->id :
                           CC_MOTION_CLIP_NONE),
                       CcHumanoidContactName(agent->humanoid.feet[0].contact),
                       CcHumanoidContactName(agent->humanoid.feet[1].contact),
                       CcBiomechRigMeanActivation(&agent->humanoid.body) * 100.0f),
            destination, 18, 18, 10, WORLD_TEAL);
    } else if (draw_hero_rig_debug) {
        DrawViewportText(
            TextFormat("ROBOTIC %s / %d LEGS / PLANTED CONTACTS",
                       CcLocalAgentMorphologyName(agent),
                       agent->limb_rig.morphology.limb_count),
            destination, 18, 18, 10, WORLD_TEAL);
    }
}

void CcLocalDrawMarket3D(const CcSim *sim, const CcLocalAgent *agent,
                         float clock, RenderTexture2D target,
                         Rectangle destination)
{
    CcLocalDrawInterior3D(sim, agent, clock, target, destination);
}

static bool InsideExpanded(Vector2 point, Rectangle footprint, float radius)
{
    return point.x > footprint.x - radius &&
           point.x < footprint.x + footprint.width + radius &&
           point.y > footprint.y - radius &&
           point.y < footprint.y + footprint.height + radius;
}

static bool StreetCanOccupy(Vector2 point)
{
    const float radius = PLAYER_COLLISION_RADIUS;
    if (point.x < 0.28f + radius ||
        point.x > CC_LOCAL_WORLD_WIDTH - 0.28f - radius ||
        point.y < 0.28f + radius ||
        point.y > CC_LOCAL_WORLD_DEPTH - 0.28f - radius) return false;
    for (int32_t i = 0; i < (int32_t)(sizeof(WORLD_BUILDINGS) /
                                      sizeof(WORLD_BUILDINGS[0])); ++i) {
        if (InsideExpanded(point, WORLD_BUILDINGS[i].footprint, radius)) {
            return false;
        }
    }
    for (int32_t i = 0; i < CC_LOCAL_PLACE_LANDMARK_COUNT; ++i) {
        if (InsideExpanded(
                point, PlaceLandmarkFootprint(ActivePlaceLandmarkAt(i)),
                radius)) return false;
    }
    for (int32_t i = 0; i < (int32_t)(sizeof(CASTLE_STRUCTURES) /
                                      sizeof(CASTLE_STRUCTURES[0])); ++i) {
        if (InsideExpanded(point, CASTLE_STRUCTURES[i].footprint, radius)) {
            return false;
        }
    }
    if (InsideExpanded(point, CARRIAGE_FOOTPRINT, radius) ||
        InsideExpanded(point, DUNGEON_FOOTPRINT, radius)) return false;
    for (int32_t i = 0; i < (int32_t)(sizeof(ROOM_ART_OBSTACLES) /
                                      sizeof(ROOM_ART_OBSTACLES[0])); ++i) {
        if (InsideExpanded(point, ROOM_ART_OBSTACLES[i], radius)) return false;
    }
    return true;
}

static bool MarketCanOccupy(Vector2 point)
{
    const float radius = PLAYER_COLLISION_RADIUS;
    if (point.x < 0.52f + radius || point.x > 8.72f - radius ||
        point.y < 0.52f + radius || point.y > 6.72f - radius) return false;
    if (InsideExpanded(point, MARKET_COUNTER_FOOTPRINT, radius)) return false;
    if (InsideExpanded(point, MARKET_SHELF_FOOTPRINT, radius)) return false;
    return true;
}

Vector2 CcLocalMove(Vector2 current, Vector2 delta, bool market_interior)
{
    bool (*can_occupy)(Vector2) = market_interior ? MarketCanOccupy : StreetCanOccupy;
    Vector2 candidate = {current.x + delta.x, current.y};
    if (can_occupy(candidate)) current.x = candidate.x;
    candidate = (Vector2){current.x, current.y + delta.y};
    if (can_occupy(candidate)) current.y = candidate.y;
    return current;
}
