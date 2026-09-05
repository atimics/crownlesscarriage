#if !defined(CC_LOCAL3D_UNITY_BUILD)
#error "Include local renderer modules through client/cc_local3d.c"
#endif

#include "client/cc_local3d.h"
#include "client/cc_local3d_internal.h"
#include "client/cc_heraldry.h"
#include "client/cc_client_policy.h"
#include "client/cc_road_book.h"
#include "client/cc_local_place.h"
#include "client/cc_creature_catalog.h"
#include "client/cc_overlay.h"
#include "client/cc_visual_style.h"
#include "world/cc_world.h"

#include "locomotion/cc_creature.h"
#include "locomotion/cc_humanoid_skin.h"
#include "locomotion/cc_multileg.h"
#include "locomotion/cc_quadruped.h"
#include "locomotion/cc_robotics.h"

#include "raymath.h"
#include "rlgl.h"

#if defined(PLATFORM_WEB)
#include <emscripten.h>
#include <emscripten/wget.h>
#endif

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
static const float COMBAT_PLAYER_ENGAGEMENT_RADIUS = 7.50f;
static const float ROAD_BARRICADE_X = 51.85f;

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

static const float CARRIAGE_ASSET_STREET_YAW_DEGREES = -90.0f;
static bool draw_hero_rig_debug = false;
static bool distant_building_view = false;
static bool reduced_motion_enabled = false;
static CcLocalOpeningStep active_opening_step = CC_LOCAL_OPENING_COMPLETE;
static CcLocalMovementPreview renderer_movement_preview = {0};
static CcLocalAgent movement_probe_agent_scratch = {0};
static const CcWorldStream *active_world_stream = NULL;

typedef enum BridgeCheckpointStatus {
    BRIDGE_CHECKPOINT_UNKNOWN,
    BRIDGE_CHECKPOINT_AVAILABLE,
    BRIDGE_CHECKPOINT_UNAVAILABLE
} BridgeCheckpointStatus;

static BridgeCheckpointStatus bridge_checkpoint_status =
    BRIDGE_CHECKPOINT_UNKNOWN;

static bool RoadUsesAuthoredCheckpoint(void);
static void OpenWorldRenderCacheClear(void);
static void TreeSceneryBatchClear(void);
static void DrawBox(Vector3 center, Vector3 size, Color color);
static void DrawCharacterSphere(Vector3 center, float radius, Color color);
static void DrawCharacterEllipsoid(Vector3 center, Vector3 radius, Color color);
static Vector3 LocalPoint(Vector3 base, float x, float y, float z, float yaw);
static void DrawOrientedBox(Vector3 base, Vector3 local_center, Vector3 size,
                            float yaw, Color color);
static Vector3 PhysicsAdd(Vector3 a, Vector3 b);
static Vector3 PhysicsSubtract(Vector3 a, Vector3 b);
static Vector3 PhysicsScale(Vector3 value, float scale);
static float PhysicsLength(Vector3 value);
static Vector3 PhysicsNormalizeOr(Vector3 value, Vector3 fallback);
static Color ShadeColor(Color color, float scale);
static Color BlendColor(Color from, Color to, float amount);
static void DrawHeraldryBanner3D(CcHeraldryStyle style, Vector3 base,
                                 float yaw_degrees, float scale, float clock);
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

typedef struct WorldStructure {
    Rectangle footprint;
    float height;
    CcLocalCompoundKind kind;
} WorldStructure;

static const Rectangle CARRIAGE_FOOTPRINT = {35.80f, 48.50f, 3.20f, 5.40f};
static const Rectangle DUNGEON_FOOTPRINT = {27.40f, 49.70f, 3.20f, 1.60f};

static const Rectangle ROOM_ART_OBSTACLES[] = {
    {8.48f, 10.38f, 0.36f, 0.36f},
    {14.16f, 10.38f, 0.36f, 0.36f},
    {7.78f, 24.68f, 0.62f, 0.62f},
    {17.58f, 54.36f, 0.84f, 0.72f},
    {30.72f, 24.08f, 0.46f, 0.46f},
    {35.08f, 55.10f, 3.34f, 0.50f},
    {63.18f, 50.06f, 1.92f, 1.92f},
    {80.22f, 45.82f, 2.36f, 2.36f},

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

    {36.20f, 38.68f, 4.30f, 2.64f},
    {40.48f, 38.48f, 3.62f, 3.04f},
    {33.70f, 42.08f, 0.80f, 0.76f},
    {34.58f, 42.10f, 0.88f, 0.68f},

    {47.94f, 38.45f, 7.82f, 0.38f},
    {47.94f, 41.17f, 7.82f, 0.38f},
    {49.54f, 37.20f, 1.72f, 1.36f},
    {50.44f, 40.04f, 1.39f, 0.96f},
    {52.36f, 38.86f, 0.28f, 0.28f},
    {52.36f, 40.86f, 0.28f, 0.28f}
};
static const Rectangle ROAD_FALLBACK_OBSTACLES[] = {

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
    {50.50f, 28.80f}, {49.20f, 35.20f}, {33.80f, 44.00f},
    {29.00f, 28.00f}, {52.00f, 31.00f}, {58.00f, 26.50f}
};
static const Vector2 MARKET_PEOPLE[] = {{6.55f, 1.60f}};


typedef enum ArtLightProfileId {
    ART_LIGHT_CLEAR_MARKET = 0,
    ART_LIGHT_SHORTAGE_OVERCAST,
    ART_LIGHT_RECOVERY_WARM,
    ART_LIGHT_ROAD_DUSK,
    ART_LIGHT_INTERIOR_EMBER,
    ART_LIGHT_STORYBOOK,
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
    float practical_glow;
    Vector3 character_rim_tint;
    float character_rim_strength;
    float horizon_warmth;
    float moonlight;
} ArtAtmosphereDefinition;

typedef struct ArtAtmosphereState {
    ArtAtmosphereDefinition from;
    CcLocalAtmospherePreset target;
    float blend;
    float duration;
} ArtAtmosphereState;

static const ArtLightProfileDefinition ART_LIGHT_PROFILES[] = {
    [ART_LIGHT_STORYBOOK] = {
        {-0.58f, 0.68f, 0.42f}, {1.20f, 1.02f, 0.76f},
        {0.72f, 0.78f, 0.80f}, {0.72f, 0.86f, 1.06f},
        {142, 161, 153, 255}, 68.0f, 150.0f, 0.38f, 0.16f,
    },
    [ART_LIGHT_CLEAR_MARKET] = {
        {-0.48f, 0.78f, 0.40f}, {1.18f, 1.06f, 0.88f},
        {0.78f, 0.82f, 0.87f}, {0.82f, 0.92f, 1.06f},
        {42, 62, 70, 255}, 26.0f, 55.0f, 0.70f, 0.12f,
    },
    [ART_LIGHT_SHORTAGE_OVERCAST] = {
        {-0.20f, 0.92f, 0.33f}, {1.00f, 1.02f, 1.06f},
        {0.78f, 0.84f, 0.91f}, {0.82f, 0.91f, 1.06f},
        {50, 66, 82, 255}, 24.0f, 52.0f, 0.78f, 0.10f,
    },
    [ART_LIGHT_RECOVERY_WARM] = {
        {-0.56f, 0.72f, 0.40f}, {1.20f, 0.91f, 0.65f},
        {0.80f, 0.78f, 0.79f}, {0.84f, 0.93f, 1.06f},
        {53, 68, 70, 255}, 27.0f, 56.0f, 0.64f, 0.15f,
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


static const ArtAtmosphereDefinition ART_ATMOSPHERES[] = {
    [CC_LOCAL_ATMOSPHERE_CLEAR_DAY] = {
        {-0.42f, 0.84f, 0.34f}, {1.00f, 1.00f, 1.00f},
        {1.00f, 1.00f, 1.00f}, {1.00f, 1.00f, 1.00f},
        {47, 44, 61, 255}, 0.10f, 0.10f, 1.00f, 1.00f, 1.00f,
        0.000f, 1.00f, 1.00f, 1.00f, 0.00f, 0.00f, 0.00f, 0.00f,
        0.05f, {0.30f, 0.52f, 0.50f}, 0.76f, 0.16f, 0.00f,
    },
    [CC_LOCAL_ATMOSPHERE_RAINY_OVERCAST] = {
        {-0.20f, 0.92f, 0.22f}, {0.78f, 0.86f, 0.98f},
        {0.90f, 0.97f, 1.07f}, {0.96f, 0.90f, 1.10f},
        {48, 52, 63, 255}, 0.70f, 0.74f, 0.70f, 1.14f, 0.88f,
        -0.035f, 1.34f, 0.38f, 0.84f, 0.82f, 0.36f, 0.82f, 0.00f,
        0.18f, {0.36f, 0.55f, 0.68f}, 0.88f, 0.00f, 0.12f,
    },
    [CC_LOCAL_ATMOSPHERE_AMBER_DUSK] = {
        {-0.70f, 0.36f, 0.30f}, {1.24f, 0.72f, 0.46f},
        {0.84f, 0.78f, 0.92f}, {1.05f, 0.82f, 1.12f},
        {53, 35, 63, 255}, 0.82f, 0.48f, 0.82f, 1.08f, 1.08f,
        -0.020f, 1.36f, 1.58f, 1.05f, 0.00f, 0.10f, 0.00f, 0.00f,
        0.52f, {0.94f, 0.54f, 0.28f}, 1.04f, 0.90f, 0.00f,
    },
    [CC_LOCAL_ATMOSPHERE_MOONLIT_NIGHT] = {
        {-0.30f, 0.48f, -0.82f}, {0.82f, 0.90f, 1.10f},
        {0.90f, 0.94f, 1.06f}, {0.92f, 0.92f, 1.07f},
        {47, 44, 61, 255}, 0.76f, 0.50f, 0.90f, 1.06f, 1.18f,
        -0.015f, 1.20f, 0.78f, 0.92f, 0.00f, 0.12f, 0.04f, 0.00f,
        0.88f, {0.40f, 0.54f, 0.90f}, 1.12f, 0.00f, 0.82f,
    },
    [CC_LOCAL_ATMOSPHERE_DRAGON_OMEN] = {
        {-0.58f, 0.42f, 0.18f}, {0.96f, 0.75f, 0.76f},
        {0.82f, 0.84f, 0.94f}, {0.94f, 0.84f, 1.08f},
        {48, 44, 57, 255}, 0.74f, 0.58f, 0.82f, 1.10f, 1.20f,
        -0.012f, 1.26f, 1.00f, 0.86f, 0.30f, 0.54f, 0.36f, 1.00f,
        0.46f, {0.82f, 0.42f, 0.36f}, 1.02f, 0.22f, 0.18f,
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
        .practical_glow = ArtAtmosphereMixFloat(
            from.practical_glow, to.practical_glow, amount),
        .character_rim_tint = ArtAtmosphereMixVector(
            from.character_rim_tint, to.character_rim_tint, amount),
        .character_rim_strength = ArtAtmosphereMixFloat(
            from.character_rim_strength, to.character_rim_strength,
            amount),
        .horizon_warmth = ArtAtmosphereMixFloat(
            from.horizon_warmth, to.horizon_warmth, amount),
        .moonlight = ArtAtmosphereMixFloat(
            from.moonlight, to.moonlight, amount),
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

void CcLocalRendererUpdateAtmosphereInternal(float delta_time)
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


static const StreetTraversalLink STREET_TRAVERSAL_LINKS[] = {
    {0, 3, {{9.2f, 7.5f}, {9.2f, 11.6f},
            {42.0f, 11.6f}, {42.0f, 25.0f}}, 4},
    {1, 3, {{15.8f, 29.0f}, {29.0f, 29.0f}}, 2},
    {2, 5, {{20.0f, 56.2f}, {34.3f, 56.2f}, {39.2f, 56.2f}}, 3},
    {3, 4, {{33.0f, 27.5f}, {40.5f, 27.5f}, {42.5f, 28.5f}}, 3},
    {4, 5, {{42.0f, 36.0f}, {42.0f, 46.0f}}, 2},
    {4, 6, {{47.0f, 28.2f}}, 1},
    {5, 7, {{42.0f, 55.0f}, {54.8f, 55.0f}, {54.8f, 50.0f}}, 3},
    {5, 8, {{42.0f, 52.0f}, {42.0f, 35.8f},
            {78.5f, 35.8f}, {78.5f, 34.0f}}, 4},
    {6, 8, {{54.5f, 28.2f}, {54.5f, 35.8f},
            {78.5f, 35.8f}, {78.5f, 34.0f}}, 4},
    {7, 9, {{68.0f, 51.5f}, {76.0f, 51.5f}}, 2},
    {8, 9, {{78.5f, 38.0f}, {78.5f, 45.0f}}, 2},
};

static const StreetBoundaryExit STREET_BOUNDARY_EXITS[] = {
    {1, {1.0f, 29.0f}, {{7.0f, 29.0f}}, 1, "WESTERN ROAD"},
    {2, {1.0f, 55.4f}, {{8.0f, 55.4f}}, 1, "OLD MINE TRACK"},
    {8, {94.8f, 34.0f},
     {{78.5f, 35.8f}, {93.0f, 35.8f}}, 2,
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

typedef struct ConversationCameraRig {
    Vector3 displayed_target;
    Vector3 displayed_offset;
    float displayed_fovy;
    float focus_weight;
    float last_clock;
    bool initialized;
} ConversationCameraRig;

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
static ConversationCameraRig conversation_camera_rig = {0};
static PresentedCameraState presented_camera[3] = {0};
static CcLocalPresentedPerson presented_people[64];
static int32_t presented_people_count;
static bool interaction_ui_enabled;
static bool conversation_focus_active;
static CcLocalAgent conversation_focus_agent;
static uint32_t conversation_focus_seed;

void CcLocalRendererSetConversationFocus(const Vector3 *position, uint32_t seed, float yaw)
{
    conversation_focus_active = position != NULL;
    if (position == NULL) return;
    conversation_focus_agent.position = *position;
    conversation_focus_agent.facing_yaw = yaw;
    conversation_focus_seed = seed;
}

void CcLocalRendererSetInteractionUI(bool enabled)
{
    interaction_ui_enabled = enabled;
}

static void RememberPresentedPerson(Vector3 position, float size,
                                    uint32_t seed, CcNpcRole role)
{
    if (presented_people_count >= 64) return;
    presented_people[presented_people_count++] =
        (CcLocalPresentedPerson){position, seed, role, size};
}

int32_t CcLocalPresentedPeople(CcLocalPresentedPerson *people, int32_t capacity)
{
    int32_t count = capacity < presented_people_count ? capacity : presented_people_count;
    if (people == NULL || count < 0) return 0;
    for (int32_t i = 0; i < count; ++i) people[i] = presented_people[i];
    return count;
}

static void RememberPresentedCamera(CcLocalSceneKind scene, Camera3D camera,
                                    const CcLocalAgent *agent,
                                    int32_t width, int32_t height)
{
    if (scene < CC_LOCAL_SCENE_STREET || scene > CC_LOCAL_SCENE_ROAD ||
        width <= 0 || height <= 0) return;
    presented_people_count = 0;
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
