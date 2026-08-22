#include "client/cc_local3d.h"
#include "client/cc_local3d_internal.h"
#include "client/cc_overlay.h"
#include "client/cc_visual_style.h"

#include "locomotion/cc_humanoid_skin.h"

#include "raymath.h"
#include "rlgl.h"

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define WORLD_VOID CC_STYLE_BACKGROUND
#define WORLD_INK CC_STYLE_INK
#define WORLD_MUTED CC_STYLE_MUTED
#define WORLD_TEAL CC_STYLE_TEAL
#define WORLD_GOLD CC_STYLE_GOLD
#define WORLD_DANGER CC_STYLE_DANGER
#define WORLD_VIOLET CC_STYLE_VIOLET
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
static const float CARRIAGE_ASSET_SCALE = 0.92f;
/* The exported carriage's hitch points along local +X. The street bay runs
   +Z, while the encounter road already runs +X. */
static const float CARRIAGE_ASSET_STREET_YAW_DEGREES = -90.0f;
static const float CARRIAGE_ASSET_ROAD_YAW_DEGREES = 0.0f;
static bool draw_hero_rig_debug = false;

typedef enum BridgeCheckpointStatus {
    BRIDGE_CHECKPOINT_UNKNOWN,
    BRIDGE_CHECKPOINT_AVAILABLE,
    BRIDGE_CHECKPOINT_UNAVAILABLE
} BridgeCheckpointStatus;

static BridgeCheckpointStatus bridge_checkpoint_status =
    BRIDGE_CHECKPOINT_UNKNOWN;

typedef struct WorldLabel {
    Vector3 point;
    const char *text;
    Color color;
} WorldLabel;

typedef enum FaceGlyphExpression {
    FACE_GLYPH_NEUTRAL,
    FACE_GLYPH_FOCUSED,
    FACE_GLYPH_HURT
} FaceGlyphExpression;

typedef struct FaceGlyph {
    Vector3 eye_point;
    Vector3 body_base;
    Vector3 forward;
    Color ink;
    Color skin_shadow;
    FaceGlyphExpression expression;
    float face_width;
    float age;
    uint8_t beard_style;
    uint8_t nose_style;
    uint8_t scar_style;
} FaceGlyph;

#define CC_FACE_GLYPH_MAX_COUNT 40
static FaceGlyph face_glyphs[CC_FACE_GLYPH_MAX_COUNT];
static int32_t face_glyph_count = 0;

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
};
static const Rectangle COURSE_POOL = {10.00f, 9.05f, 2.55f, 1.38f};
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
typedef struct StreetCameraShot {
    Vector2 trigger;
    Vector3 target;
    const char *name;
    Rectangle route;
    int32_t route_palette;
} StreetCameraShot;

static const StreetCameraShot STREET_CAMERA_SHOTS[] = {
    {{10.5f, 7.5f}, {10.5f, 1.05f, 7.5f}, "WAYFARER YARD",
     {8.6f, 10.2f, 6.0f, 3.0f}, 2},
    {{11.0f, 28.5f}, {11.0f, 1.05f, 28.5f}, "WEST CROFTS",
     {12.3f, 26.3f, 4.8f, 5.8f}, 0},
    {{14.0f, 52.0f}, {14.0f, 1.05f, 52.0f}, "OLD MINE ROAD",
     {14.0f, 54.2f, 28.0f, 2.7f}, 3},
    {{33.0f, 25.0f}, {33.0f, 1.05f, 25.0f}, "ARTISAN ROW",
     {29.0f, 23.3f, 13.0f, 2.6f}, 1},
    {{44.0f, 29.0f}, {44.0f, 1.05f, 29.0f}, "MERCERCALL COMMONS",
     {0.0f, 0.0f, 0.0f, 0.0f}, 1},
    {{42.0f, 52.0f}, {40.0f, 1.05f, 52.0f}, "COACH YARD",
     {39.5f, 53.8f, 4.9f, 4.2f}, 0},
    {{50.0f, 27.25f}, {50.0f, 1.05f, 24.0f}, "MARKET STEPS",
     {47.0f, 25.25f, 6.0f, 1.38f}, 1},
    {{58.0f, 50.0f}, {58.0f, 1.05f, 50.0f}, "MILLER'S ROW",
     {54.2f, 50.1f, 18.7f, 2.9f}, 0},
    {{78.5f, 29.0f}, {78.0f, 1.05f, 20.0f}, "CROWN GATE",
     {75.4f, 27.0f, 6.2f, 5.2f}, 2},
    {{78.0f, 50.0f}, {78.0f, 1.05f, 50.0f}, "EAST FIELDS",
     {76.4f, 31.3f, 3.7f, 22.7f}, 0},
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
    {5, 7, {{42.0f, 54.0f}, {54.5f, 54.0f}, {54.5f, 50.0f}}, 3},
    {6, 8, {{63.8f, 27.5f}, {63.8f, 34.0f}, {78.5f, 34.0f}}, 3},
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
    float transition_elapsed;
    float transition_duration;
    float last_clock;
    int32_t shot;
    bool initialized;
} FixedCameraRig;

static FixedCameraRig street_camera_rig = {0};
static FixedCameraRig road_camera_rig = {0};

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
static Camera3D SnapCameraToArtPixels(Camera3D camera, int32_t art_height);
static Camera3D StreetCamera(Vector3 focus, float clock, bool advance,
                             int32_t art_height);
static Camera3D RoadCamera(Vector3 focus, bool travelling, float clock,
                           bool advance, int32_t art_height);
static int32_t StreetCameraShotFor(Vector3 focus, int32_t current_shot);
static float WrapAngle(float angle);
static float SmoothStep01(float amount);
static Color ShadeColor(Color color, float scale);
static void UpdateHeroCape(CcLocalAgent *agent, float delta_time);
static int32_t RoadObstacleCount(void);
static Rectangle RoadObstacleAt(int32_t index);
static bool RoomDetailPointVisible(float x, float z, Vector3 focus);
static uint32_t StreetForegroundBuildingMask(void);

static bool CourseWaterContains(CcLocalSceneKind scene, float x, float z)
{
    if (scene != CC_LOCAL_SCENE_STREET) return false;
    return x >= COURSE_POOL.x && x <= COURSE_POOL.x + COURSE_POOL.width &&
           z >= COURSE_POOL.y && z <= COURSE_POOL.y + COURSE_POOL.height;
}

static float SurfaceHeightAt(CcLocalSceneKind scene, float x, float z)
{
    float height = 0.0f;
    if (scene != CC_LOCAL_SCENE_STREET) return height;
    for (int32_t i = 0; i < (int32_t)(sizeof(STREET_PLATFORMS) /
                                      sizeof(STREET_PLATFORMS[0])); ++i) {
        const NavPlatform *platform = &STREET_PLATFORMS[i];
        if (x >= platform->x && x <= platform->x + platform->width &&
            z >= platform->z && z <= platform->z + platform->depth &&
            platform->height > height) {
            height = platform->height;
        }
    }
    return height;
}

static float BodySurfaceHeightAt(CcLocalSceneKind scene, float x, float z)
{
    float height = 0.0f;
    if (scene != CC_LOCAL_SCENE_STREET) return height;
    for (int32_t i = 0; i < (int32_t)(sizeof(STREET_PLATFORMS) /
                                      sizeof(STREET_PLATFORMS[0])); ++i) {
        const NavPlatform *platform = &STREET_PLATFORMS[i];
        float edge_release = platform->style == 0 ? 0.0f : 0.10f;
        if (x >= platform->x + edge_release &&
            x <= platform->x + platform->width - edge_release &&
            z >= platform->z + edge_release &&
            z <= platform->z + platform->depth - edge_release &&
            platform->height > height) {
            height = platform->height;
        }
    }
    return height;
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
        if (CircleTouchesFootprint(x, z, radius,
                                   WORLD_BUILDINGS[i].footprint)) return true;
    }
    for (int32_t i = 0; i < (int32_t)(sizeof(CASTLE_STRUCTURES) /
                                      sizeof(CASTLE_STRUCTURES[0])); ++i) {
        if (CircleTouchesFootprint(x, z, radius,
                                   CASTLE_STRUCTURES[i].footprint)) return true;
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

typedef struct LocalProbeContext {
    CcLocalSceneKind scene;
} LocalProbeContext;

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
    *normal = (CcLimbVec3){0.0f, 1.0f, 0.0f};
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
    for (int32_t i = 0; i < (int32_t)(sizeof(STREET_PLATFORMS) /
                                      sizeof(STREET_PLATFORMS[0])); ++i) {
        const NavPlatform *platform = &STREET_PLATFORMS[i];
        Rectangle footprint = {platform->x, platform->z,
                               platform->width, platform->depth};
        if (platform->height > feet_height + 0.24f &&
            CircleTouchesFootprint(x, z, radius, footprint) &&
            (highest == NULL || platform->height > highest->height)) {
            highest = platform;
        }
    }
    return highest;
}

static const NavPlatform *SupportingPlatformAt(float x, float z,
                                               float feet_height)
{
    for (int32_t i = 0; i < (int32_t)(sizeof(STREET_PLATFORMS) /
                                      sizeof(STREET_PLATFORMS[0])); ++i) {
        const NavPlatform *platform = &STREET_PLATFORMS[i];
        if (fabsf(platform->height - feet_height) > 0.08f) continue;
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
    agent->radius = PLAYER_COLLISION_RADIUS;
    agent->grounded = true;
    agent->allow_downclimb = true;
    agent->crowned = true;
    agent->tunic_color = (Color){42, 128, 136, 255};
    agent->appearance = CcNpcAppearanceGenerate(
        UINT32_C(0xc04e1e55), CC_NPC_ROLE_WAYFARER,
        (Color){42, 128, 136, 255});
    agent->appearance.hair_style = 3U;
    agent->appearance.beard_style = 0U;
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
            CombatAdd(CombatScale(up, -0.90f),
                      CombatAdd(CombatScale(right, 0.22f),
                                CombatScale(forward, 0.18f))), forward);
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
        case CC_COMBAT_SKILL_SECOND_WIND: return "SECOND WIND";
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
        {44.75f, 38.45f}, {44.85f, 40.00f}, {44.75f, 41.55f}
    };
    course->road_encounter = true;
    course->scene = CC_LOCAL_SCENE_ROAD;
    player->scene = CC_LOCAL_SCENE_ROAD;
    course->situation_witness_active = false;
    course->situation_witness_id = 0U;
    for (int32_t i = 0; i < CC_LOCAL_TRAVELLER_COUNT; ++i) {
        course->travellers[i].active = false;
    }
    for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
        CcLocalCourseRunner *runner = &course->runners[i];
        CcLocalAgentInit(&runner->agent, guard_positions[i], false);
        runner->agent.scene = CC_LOCAL_SCENE_ROAD;
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
        raider->scene = CC_LOCAL_SCENE_ROAD;
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
    course->raider_resolve = 100;
    course->last_outcome = CC_COMBAT_OUTCOME_NONE;
    course->last_attacker_team = CC_COMBAT_NEUTRAL;
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
        CcLocalAgentSetNpcAppearance(
            raider, UINT32_C(0x52414900) + (uint32_t)i,
            CC_NPC_ROLE_RAIDER, (Color){126, 55, 61, 255});
        course->raider_response_stage[i] = 0;
        course->raider_response_waypoint_active[i] = false;
        course->raider_attack_cooldown[i] = 0.46f + (float)i * 0.24f;
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
    float x = first->position.x - second->position.x;
    float z = first->position.z - second->position.z;
    float distance_squared = x * x + z * z;
    if (distance_squared >= minimum * minimum) return;
    float distance = sqrtf(distance_squared);
    if (distance <= 0.0001f) {
        /* Stable, deterministic fallback for coincident actors. */
        static const Vector2 directions[4] = {
            {1.0f, 0.0f}, {0.0f, 1.0f},
            {-1.0f, 0.0f}, {0.0f, -1.0f}
        };
        Vector2 direction = directions[pair_index & 3];
        x = direction.x;
        z = direction.y;
        distance = 1.0f;
    }
    float relative_speed = fminf(
        0.92f, 0.12f + (minimum - sqrtf(distance_squared)) * 3.2f);
    float shared_speed = relative_speed * 0.5f;
    x /= distance;
    z /= distance;
    first->separation_velocity.x += x * shared_speed;
    first->separation_velocity.z += z * shared_speed;
    second->separation_velocity.x -= x * shared_speed;
    second->separation_velocity.z -= z * shared_speed;
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
        float lane_z = 38.70f + (float)guard * 1.30f;
        if (stage == 0) {
            *waypoint = (Vector3){44.75f, 0.0f, lane_z};
        } else if (stage == 1) {
            *waypoint = (Vector3){46.20f, 0.0f, lane_z};
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
                                CcCombatOutcome outcome)
{
    if (outcome == CC_COMBAT_OUTCOME_NONE) return;
    course->last_outcome = outcome;
    course->last_attacker_team = attacker->combat.team;
    course->combat_event_seconds = 0.72f;
}

static void CourseResolveImpact(CcLocalCourse *course,
                                CcLocalAgent *attacker,
                                CcLocalAgent *defender)
{
    if (!CcHumanoidGaitConsumeStrikeImpact(&attacker->humanoid)) return;
    CourseRecordOutcome(course, attacker,
                        CcLocalCombatResolveStrike(attacker, defender));
}

static void CourseRefreshRaiderResolve(CcLocalCourse *course)
{
    float total = 0.0f;
    for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
        total += course->raiders[i].combat.health;
    }
    float average = total / (float)CC_LOCAL_RAIDER_COUNT;
    course->raider_resolve = (int32_t)lroundf(CombatClamp(
        average, 0.0f, CC_LOCAL_COMBAT_MAX_HEALTH));
}

static bool CourseRaidersBroken(const CcLocalCourse *course)
{
    float total_health = 0.0f;
    int32_t standing = 0;
    for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
        total_health += course->raiders[i].combat.health;
        standing += CombatIsDefeated(&course->raiders[i].combat) ? 0 : 1;
    }
    return standing == 0 || total_health <= 60.0f;
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
    Camera3D camera = course->scene == CC_LOCAL_SCENE_ROAD ?
        RoadCamera(player->position, false, 0.0f, false,
                   target.texture.height) :
        StreetCamera(player->position, 0.0f, false,
                     target.texture.height);
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
        if (player->combat.health >= CC_LOCAL_COMBAT_MAX_HEALTH &&
            player->combat.posture >= CC_LOCAL_COMBAT_MAX_POSTURE) {
            return false;
        }
        player->combat.health = fminf(CC_LOCAL_COMBAT_MAX_HEALTH,
                                      player->combat.health + 34.0f);
        player->combat.posture = fminf(CC_LOCAL_COMBAT_MAX_POSTURE,
                                       player->combat.posture + 52.0f);
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
                if (CcLocalAgentSetExactTarget(raider, retreat_waypoint,
                                               false)) {
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
    CourseRefreshRaiderResolve(course);
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
        return STREET_CAMERA_SHOTS[portal.destination_room].name;
    }
    return portal.exit != NULL ? portal.exit->name : NULL;
}

static void ClearAgentNavigation(CcLocalAgent *agent)
{
    if (agent == NULL) return;
    agent->navigation_point_count = 0;
    agent->navigation_point_index = 0;
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
    return SetAgentExactTarget(agent, target, market_interior);
}

static bool QueueStreetNavigationPoint(CcLocalAgent *agent, Vector2 point)
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

static bool StartStreetPortalTraversal(CcLocalAgent *agent,
                                       int32_t portal_index,
                                       bool include_room_trigger)
{
    ResolvedStreetPortal portal = {0};
    if (agent == NULL || !CombatCanAct(&agent->combat) ||
        !ResolveStreetPortal(agent, portal_index, &portal)) return false;

    int32_t room = StreetRoomForAgent(agent);
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
    if (agent->navigation_destination_room >= 0) {
        return STREET_CAMERA_SHOTS[agent->navigation_destination_room].name;
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
    agent->world_exit_requested = world_exit;
    return false;
}

static Vector3 StreetPortalWorldPoint(const ResolvedStreetPortal *portal)
{
    Vector2 point = portal->destination_room >= 0 ?
        STREET_CAMERA_SHOTS[portal->destination_room].trigger :
        portal->exit->endpoint;
    return (Vector3){point.x, 0.42f, point.y};
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

static bool StreetPortalGroundApproach(const ResolvedStreetPortal *portal,
                                       int32_t room, Vector2 *approach)
{
    enum { ART_WIDTH = 457, ART_HEIGHT = 285 };
    if (portal == NULL || approach == NULL || room < 0 ||
        room >= (int32_t)(sizeof(STREET_CAMERA_SHOTS) /
                          sizeof(STREET_CAMERA_SHOTS[0]))) return false;
    Camera3D camera = SnapCameraToArtPixels(
        ExteriorCameraAt(STREET_CAMERA_SHOTS[room].target, 10.8f),
        ART_HEIGHT);
    Vector2 edge = StreetPortalEdgePoint(portal, camera, ART_WIDTH,
                                         ART_HEIGHT);
    Ray ray = GetScreenToWorldRayEx(edge, camera, ART_WIDTH, ART_HEIGHT);
    if (fabsf(ray.direction.y) < 0.0001f) return false;
    float distance = -ray.position.y / ray.direction.y;
    if (distance <= 0.0f) return false;
    Vector3 ground = Vector3Add(
        ray.position, Vector3Scale(ray.direction, distance));
    approach->x = ground.x;
    approach->y = ground.z;
    return true;
}

static void UpdateStreetPortalProximity(CcLocalAgent *agent)
{
    if (agent == NULL || !agent->crowned || agent->navigation_active ||
        !agent->exact_target_valid || agent->scene != CC_LOCAL_SCENE_STREET) {
        return;
    }
    int32_t room = StreetRoomForAgent(agent);
    int32_t count = CcLocalAgentStreetPortalCount(agent);
    int32_t nearest = -1;
    float nearest_distance = 1.05f * 1.05f;
    for (int32_t portal_index = 0; portal_index < count; ++portal_index) {
        ResolvedStreetPortal portal = {0};
        if (!ResolveStreetPortal(agent, portal_index, &portal)) continue;
        Vector2 approach = {0};
        if (!StreetPortalGroundApproach(&portal, room, &approach)) continue;
        float x = agent->position.x - approach.x;
        float z = agent->position.z - approach.y;
        float distance = x * x + z * z;
        if (distance >= nearest_distance) continue;
        nearest = portal_index;
        nearest_distance = distance;
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

static bool SetNearestClickTarget(CcLocalAgent *agent, Vector3 picked_point,
                                  bool market_interior)
{
    if (CcLocalAgentSetExactTarget(agent, picked_point, market_interior)) {
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
            if (CcLocalAgentSetExactTarget(agent, candidate,
                                           market_interior)) {
                return true;
            }
        }
    }
    return false;
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
    Camera3D camera = interior ? LocalCamera(true, agent->position) :
        scene == CC_LOCAL_SCENE_ROAD ?
            RoadCamera(agent->position, false, 0.0f, false,
                       target.texture.height) :
            StreetCamera(agent->position, 0.0f, false,
                         target.texture.height);
    Ray ray = GetScreenToWorldRayEx(local, camera, target.texture.width,
                                    target.texture.height);
    float nearest = FLT_MAX;
    Vector3 picked_point = {0};
    BoundingBox ground = {
        .min = {0.0f, -0.08f, 0.0f},
        .max = {interior ? 9.0f : CC_LOCAL_WORLD_WIDTH, 0.01f,
                interior ? 7.0f : CC_LOCAL_WORLD_DEPTH}
    };
    RayCollision collision = GetRayCollisionBox(ray, ground);
    if (collision.hit) {
        nearest = collision.distance;
        picked_point = collision.point;
    }
    if (scene == CC_LOCAL_SCENE_STREET) {
        for (int32_t i = 0; i < (int32_t)(sizeof(STREET_PLATFORMS) /
                                          sizeof(STREET_PLATFORMS[0])); ++i) {
            const NavPlatform *platform = &STREET_PLATFORMS[i];
            BoundingBox box = {
                .min = {platform->x, -0.02f, platform->z},
                .max = {platform->x + platform->width,
                        platform->height + 0.02f,
                        platform->z + platform->depth}
            };
            collision = GetRayCollisionBox(ray, box);
            if (!collision.hit || collision.distance >= nearest) continue;
            nearest = collision.distance;
            picked_point = collision.point;
        }
    }
    if (nearest == FLT_MAX) return false;
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
        occluder = fminf(occluder,
                         RayFootprintDistance(ray, CARRIAGE_FOOTPRINT, 1.92f));
        occluder = fminf(occluder,
                         RayFootprintDistance(ray, DUNGEON_FOOTPRINT, 2.45f));
        for (int32_t i = 0; i < (int32_t)(sizeof(ROOM_ART_OBSTACLES) /
                                          sizeof(ROOM_ART_OBSTACLES[0])); ++i) {
            Rectangle footprint = ROOM_ART_OBSTACLES[i];
            float center_x = footprint.x + footprint.width * 0.5f;
            float center_z = footprint.y + footprint.height * 0.5f;
            if (!RoomDetailPointVisible(center_x, center_z, camera.target)) {
                continue;
            }
            occluder = fminf(
                occluder,
                RayFootprintDistance(ray, footprint, 4.90f));
        }
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
    if (scene != CC_LOCAL_SCENE_STREET && occluder < nearest) return false;
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
    float left = fabsf(agent->position.x - platform->x);
    float right_edge = fabsf(agent->position.x -
                             (platform->x + platform->width));
    float near_edge = fabsf(agent->position.z - platform->z);
    float far_edge = fabsf(agent->position.z -
                           (platform->z + platform->depth));
    float nearest = left;
    Vector3 normal = {-1.0f, 0.0f, 0.0f};
    Vector3 face = {platform->x, platform->height, agent->position.z};
    if (right_edge < nearest) {
        nearest = right_edge;
        normal = (Vector3){1.0f, 0.0f, 0.0f};
        face = (Vector3){platform->x + platform->width, platform->height,
                         agent->position.z};
    }
    if (near_edge < nearest) {
        nearest = near_edge;
        normal = (Vector3){0.0f, 0.0f, -1.0f};
        face = (Vector3){agent->position.x, platform->height,
                         platform->z};
    }
    if (far_edge < nearest) {
        normal = (Vector3){0.0f, 0.0f, 1.0f};
        face = (Vector3){agent->position.x, platform->height,
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
    if (PhysicsLength(PhysicsSubtract(hand_left, shoulder_left)) > arm_reach ||
        PhysicsLength(PhysicsSubtract(hand_right, shoulder_right)) > arm_reach) {
        return false;
    }

    agent->climb_start = agent->position;
    agent->climb_end = PhysicsAdd(face,
                                  PhysicsScale(normal, -(agent->radius + 0.18f)));
    agent->climb_end.y = platform->height;
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
    Vector3 normal = {0.0f, 0.0f, 0.0f};
    Vector3 face = {agent->position.x, platform->height, agent->position.z};
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
    if (descent_end.y >= platform->height - 0.24f ||
        StaticBodyBlocked(agent->scene, descent_end.x, descent_end.z,
                          agent->radius)) {
        return false;
    }
    float facing = atan2f(-normal.x, -normal.z);
    Vector3 frame_right = {cosf(facing), 0.0f, -sinf(facing)};
    Vector3 hand_center = PhysicsAdd(face, PhysicsScale(normal, 0.018f));
    hand_center.y = platform->height + 0.035f;

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
        (platform->height - descent_end.y) * 0.18f;
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
    CcHumanoidGaitBeginClimb(&agent->humanoid);
    agent->humanoid_needs_reset = false;
    return true;
}

static void UpdateDownClimb(CcLocalAgent *agent, float delta_time,
                            bool market_interior)
{
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
    const float support[CC_HUMANOID_LEG_COUNT] = {1.0f, 1.0f};
    float standing_convergence = SmoothStep01(agent->climb_settle);
    float biomech_progress = amount < 0.78f ? amount :
        0.78f + 0.22f * standing_convergence;
    CcHumanoidGaitAdvanceClimb(
        &agent->humanoid, ToLimbVector(agent->position),
        agent->facing_yaw, hands, feet, normals, support, biomech_progress,
        delta_time, ProbeLocalSurface, &context);

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
        CcLimbVec3 hand_targets[CC_HUMANOID_ARM_COUNT] = {
            ToLimbVector(PhysicsLerp(agent->climb_hand_left,
                                     top_hand_left, left_hand_press)),
            ToLimbVector(PhysicsLerp(agent->climb_hand_right,
                                     top_hand_right, right_hand_press))
        };
        CcLimbVec3 feet[CC_HUMANOID_LEG_COUNT] = {
            ToLimbVector(foot_targets[0]), ToLimbVector(foot_targets[1])
        };
        CcLimbVec3 normals[CC_HUMANOID_LEG_COUNT] = {
            ToLimbVector(foot_normals[0]), ToLimbVector(foot_normals[1])
        };
        float support[CC_HUMANOID_LEG_COUNT] = {
            fminf(ClimbSwingSupport(left_wall_step),
                  ClimbSwingSupport(left_top_step)),
            fminf(ClimbSwingSupport(right_wall_step),
                  ClimbSwingSupport(right_top_step))
        };
        float standing_convergence = SmoothStep01(agent->climb_settle);
        float biomech_progress = amount < 0.78f ? amount :
            0.78f + 0.22f * standing_convergence;
        CcHumanoidGaitAdvanceClimb(
            &agent->humanoid, ToLimbVector(agent->position),
            agent->facing_yaw, hand_targets, feet, normals, support,
            biomech_progress, delta_time, ProbeLocalSurface, &context);
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
    if (StaticBodyBlocked(scene, candidate_x, candidate_z, agent->radius)) {
        return false;
    }
    if (scene == CC_LOCAL_SCENE_STREET) {
        const NavPlatform *support = SupportingPlatformAt(
            agent->position.x, agent->position.z, agent->position.y);
        float candidate_surface = BodySurfaceHeightAt(
            scene, candidate_x, candidate_z);
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
                agent->target_point.y >= platform->height - 0.10f;
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
        if (agent->navigation_active && target_distance < 0.22f &&
            !agent->climbing && !agent->swimming) {
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
    if (biped && in_water) {
        CcHumanoidGaitAdvanceSwim(
            &agent->humanoid, ToLimbVector(agent->position),
            agent->facing_yaw,
            (CcLimbVec3){desired_x, 0.0f, desired_z},
            COURSE_WATER_SURFACE, agent->immersion, delta_time);
        agent->velocity.x = agent->humanoid.root_velocity.x;
        agent->velocity.z = agent->humanoid.root_velocity.z;
    } else if (biped) {
        CcHumanoidGaitAdvance(&agent->humanoid, ToLimbVector(agent->position),
                              agent->facing_yaw,
                              (CcLimbVec3){desired_x, 0.0f, desired_z},
                              agent->grounded, delta_time, ProbeLocalSurface,
                              &context);
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
    if (target_distance > 0.001f && (!moved_x || !moved_z)) {
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
        /* A direct target can become unreachable behind static geometry.
           Stop feeding locomotion intent into a blocked body instead of
           playing a walk cycle forever against the obstacle. */
        agent->exact_target_valid = false;
        agent->target_valid = false;
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

    bool landed = false;
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
        } else {
            agent->grounded = false;
        }
    }
    if (biped && !agent->swimming) {
        CcHumanoidGaitConstrainMotion(&agent->humanoid,
                                      ToLimbVector(agent->position),
                                      ToLimbVector(agent->velocity),
                                      agent->grounded);
        SyncPhysicalLifeState(agent);
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
    CcLimbVec3 forward = {sinf(agent->facing_yaw), 0.0f,
                          cosf(agent->facing_yaw)};
    float weight = SmoothStep01((agent->humanoid.speed.value - 0.08f) /
                                0.32f);
    float depth = 0.058f * weight;

    /* Keep the planted legs untouched while bringing the torso back over the
       hips. Progressive offsets remove the slight backward read without
       flattening the authored gait or changing physical contacts. */
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
    Camera3D camera = {0};
    camera.target = target;
    /* A low, mostly frontal lens treats each location as an adventure-game
       stage. The small X offset preserves useful facade depth without
       returning to an isometric roof-first view. */
    camera.position = (Vector3){target.x + 3.0f, target.y + 4.4f,
                                target.z + 14.5f};
    camera.up = (Vector3){0.0f, 1.0f, 0.0f};
    camera.fovy = fovy;
    camera.projection = CAMERA_ORTHOGRAPHIC;
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

static void FixedCameraRigAim(FixedCameraRig *rig, int32_t shot,
                              Vector3 destination, float clock, bool advance)
{
    if (!rig->initialized) {
        rig->displayed_target = destination;
        rig->transition_from = destination;
        rig->destination = destination;
        rig->transition_duration = 0.0f;
        rig->transition_elapsed = 0.0f;
        rig->last_clock = clock;
        rig->shot = shot;
        rig->initialized = true;
        return;
    }
    if (!advance) return;
    float delta_time = clock - rig->last_clock;
    rig->last_clock = clock;
    delta_time = fmaxf(0.0f, fminf(delta_time, 0.05f));
    if (shot != rig->shot) {
        float distance = Vector3Distance(rig->displayed_target, destination);
        rig->transition_from = rig->displayed_target;
        rig->destination = destination;
        rig->transition_elapsed = 0.0f;
        rig->transition_duration = fmaxf(0.55f, fminf(1.15f,
                                                     0.42f + distance * 0.025f));
        rig->shot = shot;
    }
    if (rig->transition_elapsed >= rig->transition_duration ||
        rig->transition_duration <= 0.0f) {
        rig->displayed_target = rig->destination;
        return;
    }
    rig->transition_elapsed = fminf(rig->transition_duration,
                                    rig->transition_elapsed + delta_time);
    float amount = SmoothStep01(rig->transition_elapsed /
                                rig->transition_duration);
    rig->displayed_target = Vector3Lerp(rig->transition_from,
                                        rig->destination, amount);
}

static Camera3D StreetCamera(Vector3 focus, float clock, bool advance,
                             int32_t art_height)
{
    int32_t shot = StreetCameraShotFor(focus, street_camera_rig.shot);
    Vector3 destination = STREET_CAMERA_SHOTS[shot].target;
    FixedCameraRigAim(&street_camera_rig, shot, destination, clock, advance);
    return SnapCameraToArtPixels(
        ExteriorCameraAt(street_camera_rig.displayed_target, 10.8f),
        art_height);
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
    FixedCameraRigAim(&road_camera_rig, shot, destination, clock, advance);
    return SnapCameraToArtPixels(
        ExteriorCameraAt(road_camera_rig.displayed_target, 10.8f),
        art_height);
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

#define CC_HERO_SKIN_MAX_BONES 64
#define CC_HERO_CAPE_BONE_COUNT 4
#define CC_HERO_SKIN_ASSET \
    "assets/exports/hero/crownless_hero_engine_rig_v01.glb"
#define CC_BRIDGE_CHECKPOINT_ASSET \
    "assets/exports/glb/environment_bridge_checkpoint_v01.glb"
#define CC_BRIDGE_CHECKPOINT_MESH_BUDGET 96
#define CC_STYLE_GRADE_SHADER "assets/shaders/style_grade.fs"
#define CC_WORLD_LIGHT_VERTEX_SHADER "assets/shaders/world_lit.vs"
#define CC_WORLD_LIGHT_FRAGMENT_SHADER "assets/shaders/world_lit.fs"
#define CC_HERO_PIXEL_FRAGMENT_SHADER "assets/shaders/hero_pixel.fs"
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
    bool ready;
} HeroSkinCache;

static HeroSkinCache hero_skin = {0};

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

static NpcDynamicModuleId NpcHeadwearModule(uint8_t style)
{
    static const NpcDynamicModuleId modules[4] = {
        NPC_DYNAMIC_HELMET, NPC_DYNAMIC_HAT,
        NPC_DYNAMIC_HOOD, NPC_DYNAMIC_HEADWRAP,
    };
    return modules[style % 4U];
}

typedef struct VisualStyleCache {
    Shader grade;
    int32_t resolution_location;
    bool grade_ready;
    Shader world;
    int32_t light_direction_location;
    int32_t light_color_location;
    int32_t ambient_color_location;
    int32_t camera_position_location;
    int32_t fog_color_location;
    int32_t fog_near_location;
    int32_t fog_far_location;
    bool world_ready;
    Shader hero;
    int32_t hero_light_direction_location;
    int32_t hero_camera_position_location;
    int32_t hero_fog_color_location;
    int32_t hero_fog_near_location;
    int32_t hero_fog_far_location;
    int32_t hero_ink_strength_location;
    bool hero_ready;
    Shader npc;
    int32_t npc_light_direction_location;
    int32_t npc_camera_position_location;
    int32_t npc_fog_color_location;
    int32_t npc_fog_near_location;
    int32_t npc_fog_far_location;
    int32_t npc_ink_strength_location;
    int32_t npc_palette_location;
    int32_t npc_palette_ink_location;
    bool npc_ready;
} VisualStyleCache;

static VisualStyleCache visual_style = {0};

/* Material order is part of the consolidated engine-hero export contract.
   Nineteen material slots collapse into three readable masses at play scale:
   warm skin, a middle-value teal/oxblood costume, and dark limbs/hair. */
static const Color HERO_RETRO_PALETTE[] = {
    {42, 51, 50, 255},   /* body neutral */
    {172, 124, 86, 255}, /* skin */
    {178, 130, 90, 255}, /* skin light */
    {25, 25, 24, 255},   /* eye */
    {43, 32, 29, 255},   /* hair */
    {61, 58, 49, 255},   /* padding */
    {49, 48, 43, 255},   /* padding dark */
    {27, 63, 64, 255},   /* teal dark */
    {39, 104, 101, 255}, /* teal */
    {190, 142, 53, 255}, /* brass */
    {94, 44, 53, 255},   /* cape */
    {119, 52, 60, 255},  /* cape light */
    {39, 48, 49, 255},   /* steel dark */
    {102, 46, 53, 255},  /* brigandine */
    {124, 55, 62, 255},  /* brigandine edge */
    {61, 68, 67, 255},   /* steel */
    {52, 42, 34, 255},   /* leather */
    {74, 80, 77, 255},   /* steel light */
    {61, 46, 35, 255},   /* leather light */
};

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

static int32_t HeroCapeBoneFind(const char *name)
{
    for (int32_t bone = 0; bone < CC_HERO_CAPE_BONE_COUNT; ++bone) {
        if (strcmp(name, HERO_CAPE_BONE_NAMES[bone]) == 0) return bone;
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
    if (FileExists(CC_HERO_SKIN_ASSET)) return CC_HERO_SKIN_ASSET;
    static char bundled_path[1024];
    (void)snprintf(bundled_path, sizeof(bundled_path),
                   "%s/../Resources/%s", GetApplicationDirectory(),
                   CC_HERO_SKIN_ASSET);
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

static void ApplyHeroStyle(Model *model)
{
    if (model == NULL || model->materials == NULL) return;
    int32_t palette_count = (int32_t)(sizeof(HERO_RETRO_PALETTE) /
                                      sizeof(HERO_RETRO_PALETTE[0]));
    int32_t material_count = model->materialCount < palette_count ?
                             model->materialCount : palette_count;
    for (int32_t material = 0; material < material_count; ++material) {
        model->materials[material].maps[MATERIAL_MAP_DIFFUSE].color =
            HERO_RETRO_PALETTE[material];
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
    if (model->materialCount < palette_count) {
        TraceLog(LOG_WARNING,
                 "HERO: retro palette expected %d materials, found %d",
                 palette_count, model->materialCount);
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

static void SetNpcPalette(const CcNpcAppearance *appearance,
                          float ink_strength)
{
    if (!visual_style.npc_ready || appearance == NULL) return;
    const Color colors[CC_NPC_ARCHETYPE_MATERIAL_COUNT] = {
        appearance->skin,
        appearance->hair,
        appearance->underlayer,
        appearance->outer,
        appearance->trousers,
        appearance->leather,
        appearance->metal,
        appearance->accent,
        ShadeColor(appearance->hair, 0.30f),
    };
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

static void LoadVisualStyle(void)
{
    char grade_path[1024];
    if (!ResolveAssetPath(CC_STYLE_GRADE_SHADER, grade_path,
                          sizeof(grade_path))) {
        TraceLog(LOG_WARNING, "STYLE: grade shader was not found");
    } else {
        visual_style.grade = LoadShader(NULL, grade_path);
    }
    if (IsShaderValid(visual_style.grade)) {
        visual_style.resolution_location = GetShaderLocation(
            visual_style.grade, "resolution");
        visual_style.grade_ready = true;
    } else {
        visual_style.grade = (Shader){0};
        TraceLog(LOG_WARNING, "STYLE: grade shader could not be loaded");
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
    visual_style.fog_color_location = GetShaderLocation(
        visual_style.world, "fogColor");
    visual_style.fog_near_location = GetShaderLocation(
        visual_style.world, "fogNear");
    visual_style.fog_far_location = GetShaderLocation(
        visual_style.world, "fogFar");
    visual_style.world_ready = true;

    char hero_fragment_path[1024];
    if (ResolveAssetPath(CC_HERO_PIXEL_FRAGMENT_SHADER, hero_fragment_path,
                         sizeof(hero_fragment_path))) {
        visual_style.hero = LoadShader(vertex_path, hero_fragment_path);
        if (IsShaderValid(visual_style.hero)) {
            visual_style.hero_light_direction_location = GetShaderLocation(
                visual_style.hero, "lightDirection");
            visual_style.hero_camera_position_location = GetShaderLocation(
                visual_style.hero, "cameraPosition");
            visual_style.hero_fog_color_location = GetShaderLocation(
                visual_style.hero, "fogColor");
            visual_style.hero_fog_near_location = GetShaderLocation(
                visual_style.hero, "fogNear");
            visual_style.hero_fog_far_location = GetShaderLocation(
                visual_style.hero, "fogFar");
            visual_style.hero_ink_strength_location = GetShaderLocation(
                visual_style.hero, "inkStrength");
            float ink_strength = 0.68f;
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
            visual_style.npc_ready = true;
        } else {
            visual_style.npc = (Shader){0};
            TraceLog(LOG_WARNING,
                     "STYLE: indexed NPC shader could not be loaded");
        }
    } else {
        TraceLog(LOG_WARNING, "STYLE: indexed NPC shader was not found");
    }

    ApplyWorldShader(&sphere_models.small);
    ApplyWorldShader(&sphere_models.character);
    ApplyWorldShader(&sphere_models.scenery);
    if (hero_skin.ready) ApplyHeroStyle(&hero_skin.model);
    for (int32_t role = 0; role < CC_NPC_ROLE_COUNT; ++role) {
        for (int32_t pose = 0; pose < CC_NPC_ARCHETYPE_POSE_COUNT; ++pose) {
            if (npc_archetypes[role][pose].ready) {
                ApplyNpcStyle(&npc_archetypes[role][pose].model);
            }
        }
    }
    for (int32_t id = 0; id < NPC_DYNAMIC_MODULE_COUNT; ++id) {
        if (npc_dynamic_modules[id].ready) {
            ApplyNpcStyle(&npc_dynamic_modules[id].model);
        }
    }
    for (int32_t id = 0; id < RUNTIME_ASSET_COUNT; ++id) {
        if (runtime_assets[id].ready) ApplyWorldShader(&runtime_assets[id].model);
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
    for (int32_t bone = 0; bone < bone_count; ++bone) {
        int32_t skin_bone = CcHumanoidSkinBoneFind(
            hero_skin.model.skeleton.bones[bone].name);
        int32_t cape_bone = HeroCapeBoneFind(
            hero_skin.model.skeleton.bones[bone].name);
        hero_skin.skin_bone[bone] = skin_bone;
        hero_skin.cape_bone[bone] = cape_bone;
        if (skin_bone >= 0) found[skin_bone] = true;
        if (cape_bone >= 0) found_cape[cape_bone] = true;
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
    hero_skin.frames[0] = hero_skin.pose;
    hero_skin.animation.boneCount = bone_count;
    hero_skin.animation.keyframeCount = 1;
    hero_skin.animation.keyframePoses = hero_skin.frames;
    (void)snprintf(hero_skin.animation.name, sizeof(hero_skin.animation.name),
                   "engine-physics");
    hero_skin.ready = true;
    TraceLog(LOG_INFO, "HERO: loaded %d modular meshes on %d physics bones",
             hero_skin.model.meshCount, bone_count);
}

static bool DrawHeroSkin(const CcHumanoidSkinPose *skin,
                         const CcLocalCapeState *cape, Color tint,
                         bool visible)
{
    if (!hero_skin.ready || skin == NULL || !skin->valid || cape == NULL ||
        !cape->initialized) return false;
    for (int32_t bone = 0; bone < hero_skin.model.skeleton.boneCount; ++bone) {
        int32_t skin_bone = hero_skin.skin_bone[bone];
        int32_t cape_bone = hero_skin.cape_bone[bone];
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
        } else {
            hero_skin.pose[bone] = hero_skin.model.skeleton.bindPose[bone];
            continue;
        }
        Quaternion delta = HeroRotationBetween(
            rest_direction, target_direction);
        hero_skin.pose[bone].translation = target_head;
        hero_skin.pose[bone].rotation = QuaternionMultiply(
            delta, hero_skin.model.skeleton.bindPose[bone].rotation);
        hero_skin.pose[bone].scale =
            hero_skin.model.skeleton.bindPose[bone].scale;
        float gameplay_scale = skin_bone == CC_HUMANOID_SKIN_HEAD ? 1.06f :
            (skin_bone == CC_HUMANOID_SKIN_HAND_LEFT ||
             skin_bone == CC_HUMANOID_SKIN_HAND_RIGHT) ? 1.02f :
            (skin_bone == CC_HUMANOID_SKIN_FOOT_LEFT ||
             skin_bone == CC_HUMANOID_SKIN_FOOT_RIGHT) ? 1.02f : 1.0f;
        hero_skin.pose[bone].scale = Vector3Scale(
            hero_skin.pose[bone].scale, gameplay_scale);
    }
    CcLocalRendererRecordSkinUpdate(hero_skin.model.meshCount);
    UpdateModelAnimation(hero_skin.model, hero_skin.animation, 0.0f);
    if (visible) {
        const float horizontal_scale = 1.02f;
        Vector3 anchor = FromLimbVector(
            skin->bones[CC_HUMANOID_SKIN_ROOT].head);
        Vector3 position = {anchor.x * (1.0f - horizontal_scale), 0.0f,
                            anchor.z * (1.0f - horizontal_scale)};
        DrawModelEx(hero_skin.model, position,
                    (Vector3){0.0f, 1.0f, 0.0f}, 0.0f,
                    (Vector3){horizontal_scale, 1.0f, horizontal_scale}, tint);
    }
    return true;
}

void CcLocalRendererInit(void)
{
    if (sphere_models.ready) return;
    street_camera_rig = (FixedCameraRig){0};
    road_camera_rig = (FixedCameraRig){0};
    sphere_models.small = LoadModelFromMesh(GenMeshSphere(1.0f, 6, 8));
    sphere_models.character = LoadModelFromMesh(GenMeshSphere(1.0f, 8, 8));
    sphere_models.scenery = LoadModelFromMesh(GenMeshSphere(1.0f, 10, 12));
    sphere_models.ready = true;
    LoadHeroSkin();
    LoadNpcArchetypes();
    LoadNpcDynamicModules();
    LoadRuntimeAssets();
    LoadVisualStyle();
}

void CcLocalRendererSetDiagnosticOverlay(bool enabled)
{
    draw_hero_rig_debug = enabled;
}

void CcLocalRendererShutdown(void)
{
    if (!sphere_models.ready) return;
    UnloadModel(sphere_models.small);
    UnloadModel(sphere_models.character);
    UnloadModel(sphere_models.scenery);
    if (hero_skin.ready) UnloadModel(hero_skin.model);
    for (int32_t role = 0; role < CC_NPC_ROLE_COUNT; ++role) {
        for (int32_t pose = 0; pose < CC_NPC_ARCHETYPE_POSE_COUNT; ++pose) {
            if (npc_archetypes[role][pose].ready) {
                UnloadModel(npc_archetypes[role][pose].model);
            }
            npc_archetypes[role][pose] = (NpcArchetypeCache){0};
        }
    }
    for (int32_t id = 0; id < NPC_DYNAMIC_MODULE_COUNT; ++id) {
        if (npc_dynamic_modules[id].ready) {
            UnloadModel(npc_dynamic_modules[id].model);
        }
        npc_dynamic_modules[id].model = (Model){0};
        npc_dynamic_modules[id].ready = false;
    }
    for (int32_t id = 0; id < RUNTIME_ASSET_COUNT; ++id) {
        if (runtime_assets[id].ready) UnloadModel(runtime_assets[id].model);
        runtime_assets[id].model = (Model){0};
        runtime_assets[id].ready = false;
    }
    if (visual_style.npc_ready) UnloadShader(visual_style.npc);
    if (visual_style.hero_ready) UnloadShader(visual_style.hero);
    if (visual_style.world_ready) UnloadShader(visual_style.world);
    if (visual_style.grade_ready) UnloadShader(visual_style.grade);
    sphere_models = (SphereModelCache){0};
    hero_skin = (HeroSkinCache){0};
    visual_style = (VisualStyleCache){0};
    street_camera_rig = (FixedCameraRig){0};
    road_camera_rig = (FixedCameraRig){0};
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
    const float overhang = 0.34f;
    const float rise = 0.82f + fminf(width, depth) * 0.055f;
    bool ridge_along_x = width >= depth;
    Color shadow = ShadeColor(roof, 0.72f);

    rlDisableBackfaceCulling();
    if (ridge_along_x) {
        for (int32_t end = 0; end < 2; ++end) {
            float gable_x = end == 0 ? x - 0.015f : x + width + 0.015f;
            DrawTriangle3D(
                (Vector3){gable_x, wall_height, z},
                (Vector3){gable_x, wall_height, z + depth},
                (Vector3){gable_x, wall_height + rise, z + depth * 0.5f},
                wall);
        }
    } else {
        for (int32_t end = 0; end < 2; ++end) {
            float gable_z = end == 0 ? z - 0.015f : z + depth + 0.015f;
            DrawTriangle3D(
                (Vector3){x, wall_height, gable_z},
                (Vector3){x + width, wall_height, gable_z},
                (Vector3){x + width * 0.5f, wall_height + rise, gable_z},
                wall);
        }
    }
    rlEnableBackfaceCulling();

    if (ridge_along_x) {
        float run = depth * 0.5f + overhang;
        float slope = sqrtf(run * run + rise * rise);
        float pitch = atanf(rise / run) * RAD2DEG;
        for (int32_t side = -1; side <= 1; side += 2) {
            float side_sign = (float)side;
            DrawTiltedBox(
                (Vector3){x + width * 0.5f,
                          wall_height + rise * 0.5f,
                          z + depth * 0.5f + side_sign * run * 0.5f},
                (Vector3){width + overhang * 2.0f, 0.16f, slope},
                (Vector3){1.0f, 0.0f, 0.0f}, side_sign * pitch,
                side > 0 ? roof : shadow);
        }
        DrawBox((Vector3){x + width * 0.5f, wall_height + rise + 0.03f,
                          z + depth * 0.5f},
                (Vector3){width + overhang * 2.1f, 0.13f, 0.16f}, shadow);
    } else {
        float run = width * 0.5f + overhang;
        float slope = sqrtf(run * run + rise * rise);
        float pitch = atanf(rise / run) * RAD2DEG;
        for (int32_t side = -1; side <= 1; side += 2) {
            float side_sign = (float)side;
            DrawTiltedBox(
                (Vector3){x + width * 0.5f + side_sign * run * 0.5f,
                          wall_height + rise * 0.5f,
                          z + depth * 0.5f},
                (Vector3){slope, 0.16f, depth + overhang * 2.0f},
                (Vector3){0.0f, 0.0f, 1.0f}, -side_sign * pitch,
                side > 0 ? roof : shadow);
        }
        DrawBox((Vector3){x + width * 0.5f, wall_height + rise + 0.03f,
                          z + depth * 0.5f},
                (Vector3){0.16f, 0.13f, depth + overhang * 2.1f}, shadow);
    }
    return rise;
}

static void DrawFacadeWindow(Vector3 center, bool side_facing, Color trim,
                             Color glass)
{
    Vector3 recess = side_facing ? (Vector3){0.075f, 1.34f, 1.02f} :
                                   (Vector3){1.02f, 1.34f, 0.075f};
    Vector3 pane = side_facing ? (Vector3){0.035f, 1.02f, 0.72f} :
                                 (Vector3){0.72f, 1.02f, 0.035f};
    DrawBox(center, recess, ShadeColor(trim, 0.62f));
    DrawBox(center, pane, glass);
    Vector3 vertical = side_facing ? (Vector3){0.022f, 1.06f, 0.055f} :
                                     (Vector3){0.055f, 1.06f, 0.022f};
    Vector3 horizontal = side_facing ? (Vector3){0.022f, 0.055f, 0.76f} :
                                       (Vector3){0.76f, 0.055f, 0.022f};
    DrawBox(center, vertical, trim);
    DrawBox(center, horizontal, trim);
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

static void DrawBuilding(float x, float z, float width, float depth, float height,
                         Color wall, Color roof, bool door, int32_t style,
                         bool cutaway)
{
    Vector3 center = {x + width * 0.5f, height * 0.5f, z + depth * 0.5f};
    Color trim = style == 2 ? WORLD_GOLD : ShadeColor(wall, 0.60f);
    Color glass = style == 1 ? (Color){148, 103, 55, 255} :
                  style == 3 ? Fade(WORLD_VIOLET, 0.84f) :
                               Fade(WORLD_TEAL, 0.78f);
    DrawBuildingContactShadow(x, z, width, depth, height);
    DrawBox((Vector3){center.x, 0.17f, center.z},
            (Vector3){width + 0.12f, 0.34f, depth + 0.12f},
            ShadeColor(wall, 0.58f));

    if (cutaway) {
        const float wall_height = 1.02f;
        Color cut_wall = BlendColor(wall, (Color){62, 58, 53, 255}, 0.26f);
        Color floor = ShadeColor(wall, 0.70f);
        DrawCubeV((Vector3){center.x, 0.205f, center.z},
                  (Vector3){width - 0.20f, 0.07f, depth - 0.20f}, floor);
        /* The camera looks from +X/+Z. Keep the far walls as a dollhouse
           silhouette while removing the two faces that hide the player. */
        DrawBox((Vector3){center.x, wall_height * 0.5f, z + 0.10f},
                (Vector3){width, wall_height, 0.20f}, cut_wall);
        DrawBox((Vector3){x + 0.10f, wall_height * 0.5f, center.z},
                (Vector3){0.20f, wall_height, depth}, cut_wall);
        DrawBox((Vector3){center.x, wall_height + 0.02f, z + 0.10f},
                (Vector3){width + 0.10f, 0.13f, 0.26f}, trim);
        DrawBox((Vector3){x + 0.10f, wall_height + 0.02f, center.z},
                (Vector3){0.26f, 0.13f, depth + 0.10f}, trim);
        DrawCubeWiresV((Vector3){center.x, 0.22f, center.z},
                       (Vector3){width + 0.14f, 0.12f, depth + 0.14f},
                       Fade(WORLD_INK, 0.30f));
        return;
    }

    DrawBox(center, (Vector3){width, height, depth}, wall);

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
        DrawBox((Vector3){center.x, 1.12f, z + depth + 0.055f},
                (Vector3){1.30f, 2.24f, 0.12f}, trim);
        DrawBox((Vector3){center.x, 1.10f, z + depth + 0.125f},
                (Vector3){1.02f, 2.10f, 0.055f},
                (Color){43, 34, 37, 255});
        DrawSmallSphere((Vector3){center.x + 0.39f, 1.075f,
                                  z + depth + 0.18f},
                        0.035f, WORLD_GOLD);
        DrawBox((Vector3){center.x, 2.34f, z + depth + 0.34f},
                (Vector3){1.55f, 0.13f, 0.70f}, roof);
        float lantern_x = center.x + 0.94f;
        DrawBox((Vector3){lantern_x, 1.92f, z + depth + 0.12f},
                (Vector3){0.08f, 0.48f, 0.08f}, trim);
        DrawBox((Vector3){lantern_x, 2.15f, z + depth + 0.18f},
                (Vector3){0.30f, 0.08f, 0.20f}, trim);
        DrawSmallSphere((Vector3){lantern_x, 1.88f, z + depth + 0.22f},
                        0.11f, WORLD_GOLD);
    }

    float roof_rise = DrawPitchedRoof(x, z, width, depth, height, wall, roof);
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

static void DrawGroundTile(float x, float z, Color color)
{
    DrawTerrainPatchAtTop(x + 0.01f, z + 0.01f, 0.98f, 0.98f,
                          -0.002f, color);
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

static void DrawSettlementPlaza(Color stone)
{
    for (int32_t row = 0; row < 8; ++row) {
        for (int32_t column = 0; column < 7; ++column) {
            float variation = ((row + column) & 1) != 0 ? 0.94f : 1.04f;
            DrawGroundTile(38.0f + (float)column,
                           26.0f + (float)row,
                           ShadeColor(stone, variation));
        }
    }
    Color curb = ShadeColor(stone, 0.68f);
    DrawTerrainPatchAtTop(37.82f, 25.82f, 0.16f, 8.36f, 0.004f, curb);
    DrawTerrainPatchAtTop(45.0f, 25.82f, 0.16f, 8.36f, 0.004f, curb);
    DrawTerrainPatchAtTop(37.82f, 25.82f, 7.34f, 0.16f, 0.004f, curb);
    DrawTerrainPatchAtTop(37.82f, 34.02f, 7.34f, 0.16f, 0.004f, curb);
}

static void DrawStreetPavingDetails(Color road, Vector3 focus)
{
    Color joint = ShadeColor(road, 0.74f);

    /* Large, stable stone modules read after the whole scene is pixelated.
       Detail is limited to the current room so distant streets stay quiet. */
    for (float x = 16.0f; x <= 91.0f; x += 2.25f) {
        if (fabsf(x - focus.x) > 18.0f) continue;
        if (x >= 37.5f && x <= 45.4f) continue;
        DrawTerrainPatchAtTop(x, 27.24f, 0.090f, 4.32f, -0.001f, joint);
    }
    float horizontal_start = fmaxf(15.2f, focus.x - 18.0f);
    float horizontal_width = fminf(91.8f, focus.x + 18.0f) -
                             horizontal_start;
    if (horizontal_width > 0.0f) {
        DrawTerrainPatchAtTop(horizontal_start, 29.36f, horizontal_width,
                              0.085f, -0.001f, joint);
    }

    for (float z = 8.7f; z <= 57.0f; z += 2.25f) {
        if (fabsf(z - focus.z) > 18.0f) continue;
        if (z >= 25.5f && z <= 34.4f) continue;
        DrawTerrainPatchAtTop(39.84f, z, 4.32f, 0.090f, 0.0f, joint);
    }
    float vertical_start = fmaxf(8.2f, focus.z - 18.0f);
    float vertical_depth = fminf(57.8f, focus.z + 18.0f) - vertical_start;
    if (vertical_depth > 0.0f) {
        DrawTerrainPatchAtTop(41.96f, vertical_start, 0.085f,
                              vertical_depth, 0.0f, joint);
    }
}

static void DrawMarketThreshold(Color road)
{
    /* A warm, tiled threshold turns the market into a destination instead of
       a cluster of props. It joins the plaza to the building's collision-safe
       south entrance and remains traversable. */
    Color threshold = BlendColor(ShadeColor(road, 1.08f),
                                 (Color){158, 119, 67, 255}, 0.24f);
    for (int32_t row = 0; row < 2; ++row) {
        for (int32_t column = 0; column < 6; ++column) {
            float variation = ((row + column) & 1) != 0 ? 0.96f : 1.04f;
            DrawTerrainPatchAtTop(47.0f + (float)column,
                                  25.25f + (float)row * 0.72f,
                                  0.94f, 0.66f, 0.003f,
                                  ShadeColor(threshold, variation));
        }
    }
}

static Color StreetRouteColor(Color road, int32_t palette)
{
    switch (palette) {
        case 1:
            return BlendColor(ShadeColor(road, 1.08f), WORLD_GOLD, 0.18f);
        case 2:
            return BlendColor(ShadeColor(road, 0.94f), WORLD_TEAL, 0.14f);
        case 3:
            return BlendColor(ShadeColor(road, 0.88f), WORLD_VIOLET, 0.12f);
        default:
            return BlendColor(ShadeColor(road, 1.02f),
                              (Color){126, 104, 73, 255}, 0.18f);
    }
}

static void DrawRoomRoute(Rectangle route, Color stone)
{
    if (route.width <= 0.0f || route.height <= 0.0f) return;
    Color edge = ShadeColor(stone, 0.69f);
    Color joint = ShadeColor(stone, 0.78f);
    DrawTerrainPatchAtTop(route.x, route.y, route.width, route.height,
                          0.001f, stone);
    DrawTerrainPatchAtTop(route.x, route.y, route.width, 0.11f,
                          0.007f, edge);
    DrawTerrainPatchAtTop(route.x, route.y + route.height - 0.11f,
                          route.width, 0.11f, 0.007f, edge);
    DrawTerrainPatchAtTop(route.x, route.y, 0.11f, route.height,
                          0.007f, edge);
    DrawTerrainPatchAtTop(route.x + route.width - 0.11f, route.y,
                          0.11f, route.height, 0.007f, edge);

    bool runs_east_west = route.width >= route.height;
    if (runs_east_west) {
        for (float x = route.x + 1.35f;
             x < route.x + route.width - 0.35f; x += 1.55f) {
            DrawTerrainPatchAtTop(x, route.y + 0.12f, 0.075f,
                                  route.height - 0.24f, 0.006f, joint);
        }
        DrawTerrainPatchAtTop(route.x + 0.12f,
                              route.y + route.height * 0.5f,
                              route.width - 0.24f, 0.075f, 0.006f, joint);
    } else {
        for (float z = route.y + 1.35f;
             z < route.y + route.height - 0.35f; z += 1.55f) {
            DrawTerrainPatchAtTop(route.x + 0.12f, z,
                                  route.width - 0.24f, 0.075f,
                                  0.006f, joint);
        }
        DrawTerrainPatchAtTop(route.x + route.width * 0.5f,
                              route.y + 0.12f, 0.075f,
                              route.height - 0.24f, 0.006f, joint);
    }
}

static void DrawRoomRouteAccents(Color road, Vector3 focus)
{
    int32_t count = (int32_t)(sizeof(STREET_CAMERA_SHOTS) /
                              sizeof(STREET_CAMERA_SHOTS[0]));
    for (int32_t i = 0; i < count; ++i) {
        const StreetCameraShot *room = &STREET_CAMERA_SHOTS[i];
        if (room->route.width <= 0.0f || room->route.height <= 0.0f) continue;
        if (!SceneryFootprintVisibleWithin(room->route, focus, 21.0f)) continue;
        if (i == 6) {
            DrawMarketThreshold(road);
            continue;
        }
        DrawRoomRoute(room->route,
                      StreetRouteColor(road, room->route_palette));
    }
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

static void DrawWayfarerGate(Color accent)
{
    Color wood = (Color){74, 51, 39, 255};
    float left = 8.66f;
    float right = 14.34f;
    float z = 10.56f;
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
    Color wood = (Color){89, 61, 39, 255};
    Color cloth = BlendColor((Color){122, 83, 58, 255},
                             (Color){83, 73, 55, 255}, hunger);
    DrawBox((Vector3){x, 0.92f, z}, (Vector3){0.12f, 1.84f, 0.12f}, wood);
    DrawBox((Vector3){x, 1.38f, z}, (Vector3){1.38f, 0.10f, 0.10f}, wood);
    DrawBox((Vector3){x, 1.20f, z + 0.04f},
            (Vector3){0.78f, 0.68f, 0.12f}, cloth);
    DrawSmallSphere((Vector3){x, 1.80f, z}, 0.23f,
                    (Color){166, 132, 77, 255});
    DrawBox((Vector3){x, 2.03f, z}, (Vector3){0.72f, 0.08f, 0.44f}, wood);
    DrawBox((Vector3){x, 2.17f, z}, (Vector3){0.38f, 0.30f, 0.34f}, wood);
}

static void DrawMineWaystone(void)
{
    const float x = 18.0f;
    const float z = 54.72f;
    Color stone = (Color){77, 75, 78, 255};
    DrawBox((Vector3){x, 0.14f, z}, (Vector3){0.84f, 0.28f, 0.72f},
            ShadeColor(stone, 0.70f));
    DrawBox((Vector3){x, 0.82f, z}, (Vector3){0.56f, 1.38f, 0.46f}, stone);
    DrawBox((Vector3){x, 1.56f, z}, (Vector3){0.70f, 0.16f, 0.56f},
            ShadeColor(stone, 1.12f));
    DrawSmallSphere((Vector3){x, 1.03f, z + 0.27f}, 0.10f, WORLD_VIOLET);

    /* Two rails and broad sleepers turn the colored road into an explicit
       visual sentence: this way leads to the mine beyond the next room. */
    Color rail = (Color){65, 59, 56, 255};
    Color sleeper = (Color){91, 62, 43, 255};
    DrawBox((Vector3){22.65f, 0.055f, 54.92f},
            (Vector3){8.20f, 0.075f, 0.09f}, rail);
    DrawBox((Vector3){22.65f, 0.055f, 55.72f},
            (Vector3){8.20f, 0.075f, 0.09f}, rail);
    for (int32_t sleeper_index = 0; sleeper_index < 10; ++sleeper_index) {
        DrawBox((Vector3){18.85f + (float)sleeper_index * 0.84f,
                          0.035f, 55.32f},
                (Vector3){0.14f, 0.055f, 1.18f}, sleeper);
    }
}

static void DrawArtisanSign(Color kingdom)
{
    const float x = 30.95f;
    const float z = 24.30f;
    Color wood = (Color){69, 47, 39, 255};
    DrawBox((Vector3){x, 1.16f, z}, (Vector3){0.14f, 2.32f, 0.14f}, wood);
    DrawBox((Vector3){x + 0.48f, 2.12f, z},
            (Vector3){1.08f, 0.12f, 0.14f}, wood);
    DrawBox((Vector3){x + 0.86f, 1.72f, z + 0.04f},
            (Vector3){0.72f, 0.66f, 0.12f},
            BlendColor((Color){111, 73, 53, 255}, kingdom, 0.26f));
    DrawSmallSphere((Vector3){x + 0.86f, 1.73f, z + 0.12f},
                    0.11f, WORLD_GOLD);
}

static void DrawCoachHitch(const CcSettlement *place)
{
    Color wood = (Color){79, 53, 39, 255};
    float z = 55.36f;
    DrawBox((Vector3){35.30f, 0.62f, z}, (Vector3){0.18f, 1.24f, 0.18f}, wood);
    DrawBox((Vector3){38.20f, 0.62f, z}, (Vector3){0.18f, 1.24f, 0.18f}, wood);
    DrawBox((Vector3){36.75f, 0.92f, z}, (Vector3){3.08f, 0.16f, 0.18f}, wood);
    DrawBox((Vector3){36.75f, 1.62f, z},
            (Vector3){0.14f, 1.40f, 0.14f}, wood);
    DrawBox((Vector3){36.75f, 2.18f, z + 0.05f},
            (Vector3){1.30f, 0.68f, 0.12f},
            (Color){111, 72, 52, 255});
    DrawCylinder((Vector3){36.75f, 2.03f, z + 0.13f},
                 0.19f, 0.19f, 0.08f, 10, WORLD_GOLD);
    int32_t barrels = place != NULL ? place->stock[CC_GOOD_MATERIAL] / 24 : 1;
    if (barrels < 1) barrels = 1;
    if (barrels > 3) barrels = 3;
    for (int32_t i = 0; i < barrels; ++i) {
        DrawCylinder((Vector3){35.65f + (float)i * 0.58f, 0.05f, z + 0.42f},
                     0.25f, 0.25f, 0.58f, 10,
                     (Color){129, 77, 43, 255});
    }
}

static void DrawMillersGranary(float hunger)
{
    const float x = 64.14f;
    const float z = 51.02f;
    Color timber = (Color){105, 70, 43, 255};
    Color plaster = BlendColor((Color){142, 124, 91, 255},
                               (Color){101, 92, 70, 255}, hunger * 0.60f);
    DrawCylinder((Vector3){x, 0.08f, z}, 0.90f, 0.96f, 2.36f, 12, plaster);
    DrawCylinder((Vector3){x, 2.43f, z}, 0.10f, 1.10f, 0.88f, 12, timber);
    DrawBox((Vector3){x, 0.75f, z + 0.93f},
            (Vector3){0.58f, 1.20f, 0.08f}, timber);
    DrawBox((Vector3){x, 1.45f, z + 0.98f},
            (Vector3){0.72f, 0.10f, 0.12f}, WORLD_GOLD);
}

static void DrawEastWindmill(Color kingdom, float hunger)
{
    const float x = 81.4f;
    const float z = 47.0f;
    Color tower = BlendColor((Color){128, 118, 96, 255},
                             (Color){92, 86, 73, 255}, hunger * 0.50f);
    Color timber = BlendColor((Color){78, 54, 40, 255}, kingdom, 0.16f);
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
                Fade((Color){181, 158, 111, 255}, 0.78f));
        rlPopMatrix();
    }
    DrawSmallSphere((Vector3){x, 3.28f, z + 1.02f}, 0.20f, WORLD_GOLD);
}

static void DrawRoomLandmarks(const CcSettlement *place, Color kingdom,
                              Vector3 focus)
{
    float hunger = place != NULL ? (float)place->hunger / 100.0f : 0.0f;
    if (RoomDetailPointVisible(11.50f, 10.56f, focus)) {
        DrawWayfarerGate(kingdom);
    }
    if (RoomDetailPointVisible(8.10f, 25.00f, focus)) {
        DrawCroftScarecrow(hunger);
    }
    if (RoomDetailPointVisible(18.0f, 54.72f, focus)) DrawMineWaystone();
    if (RoomDetailPointVisible(30.95f, 24.30f, focus)) {
        DrawArtisanSign(kingdom);
    }
    if (RoomDetailPointVisible(36.75f, 55.36f, focus)) DrawCoachHitch(place);
    if (RoomDetailPointVisible(64.14f, 51.02f, focus)) {
        DrawMillersGranary(hunger);
    }
    if (RoomDetailPointVisible(81.4f, 47.0f, focus)) {
        DrawEastWindmill(kingdom, hunger);
    }
}

static void DrawExteriorTerrain(const CcSettlement *place, Vector3 focus)
{
    float hunger = place != NULL ? (float)place->hunger / 100.0f : 0.0f;
    float prosperity = place != NULL ?
                       (float)place->prosperity / 100.0f : 0.5f;
    Color grass = BlendColor((Color){35, 67, 53, 255},
                             (Color){75, 66, 42, 255}, hunger * 0.72f);
    Color road = BlendColor((Color){88, 82, 69, 255},
                            (Color){116, 101, 76, 255}, prosperity * 0.32f);
    Color road_edge = ShadeColor(road, 0.62f);
    Color plaza = BlendColor((Color){101, 94, 81, 255},
                             (Color){133, 119, 93, 255}, prosperity * 0.36f);

    DrawPlane((Vector3){CC_LOCAL_WORLD_WIDTH * 0.5f, -0.09f,
                        CC_LOCAL_WORLD_DEPTH * 0.5f},
              (Vector2){CC_LOCAL_WORLD_WIDTH + 8.0f,
                        CC_LOCAL_WORLD_DEPTH + 8.0f},
              grass);

    DrawTerrainPatchAtTop(1.0f, 1.0f, 16.0f, 11.0f, -0.018f,
                          (Color){43, 67, 61, 255});

    /* A recessed shoulder, raised road bed, inset lanes, and proud curbs give
       every material a unique depth. Besides reading more like constructed
       terrain, this prevents the coplanar road intersections that used to
       shimmer as the camera moved. */
    Color shoulder = ShadeColor(road, 0.70f);
    DrawTerrainPatchAtTop(14.7f, 26.80f, 77.6f, 5.20f, -0.024f, shoulder);
    DrawTerrainPatchAtTop(39.40f, 7.70f, 5.20f, 50.6f, -0.023f, shoulder);
    DrawTerrainPatchAtTop(15.0f, 27.1f, 77.0f, 4.6f, -0.014f, road);
    DrawTerrainPatchAtTop(39.7f, 8.0f, 4.6f, 50.0f, -0.012f, road);
    DrawTerrainPatchAtTop(7.2f, 10.0f, 34.8f, 3.8f, -0.010f,
                          ShadeColor(road, 0.92f));
    DrawTerrainPatchAtTop(76.0f, 27.0f, 5.0f, 7.0f, -0.008f,
                          ShadeColor(road, 1.08f));

    DrawTerrainPatchAtTop(15.0f, 27.02f, 77.0f, 0.13f, -0.004f,
                          road_edge);
    DrawTerrainPatchAtTop(15.0f, 31.70f, 77.0f, 0.13f, -0.004f,
                          road_edge);
    DrawTerrainPatchAtTop(39.62f, 8.0f, 0.13f, 50.0f, -0.003f,
                          road_edge);
    DrawTerrainPatchAtTop(44.30f, 8.0f, 0.13f, 50.0f, -0.003f,
                          road_edge);
    DrawStreetPavingDetails(road, focus);
    DrawSettlementPlaza(plaza);

    if (SceneryPointVisible(46.15f, 26.10f, focus)) {
        DrawStreetLantern(46.15f, 26.10f);
    }
    if (SceneryPointVisible(53.85f, 26.10f, focus)) {
        DrawStreetLantern(53.85f, 26.10f);
    }

    DrawTerrainPatchAtTop(3.0f, 17.0f, 13.0f, 9.0f, -0.018f,
                          (Color){86, 91, 56, 255});
    DrawTerrainPatchAtTop(3.0f, 29.0f, 13.0f, 9.0f, -0.018f,
                          (Color){93, 86, 52, 255});
    DrawTerrainPatchAtTop(3.0f, 41.0f, 13.0f, 9.0f, -0.018f,
                          (Color){79, 87, 51, 255});
    DrawTerrainPatchAtTop(59.0f, 43.0f, 14.0f, 10.0f, -0.018f,
                          (Color){89, 91, 54, 255});
    DrawTerrainPatchAtTop(76.0f, 43.0f, 15.0f, 10.0f, -0.018f,
                          (Color){82, 89, 52, 255});

    for (int32_t row = 0; row < 4; ++row) {
        float z = 18.3f + (float)row * 2.0f;
        DrawTerrainPatchAtTop(
            4.0f, z, 11.0f, 0.24f, -0.005f,
            BlendColor(Fade(WORLD_GOLD, 0.34f),
                       (Color){102, 81, 48, 255}, hunger));
        DrawTerrainPatchAtTop(60.0f, 44.3f + (float)row * 2.0f,
                              12.0f, 0.24f, -0.005f,
                              Fade(WORLD_GOLD, 0.28f));
    }
    DrawRoomRouteAccents(road, focus);
}

static Color BuildingWallColor(int32_t style)
{
    switch (style) {
        case 1: return (Color){105, 96, 84, 255};
        case 2: return (Color){122, 79, 61, 255};
        case 3: return (Color){72, 89, 95, 255};
        default: return (Color){88, 98, 91, 255};
    }
}

static Color BuildingRoofColor(int32_t style, Color kingdom)
{
    switch (style) {
        case 1:
            return BlendColor((Color){76, 69, 62, 255}, kingdom, 0.18f);
        case 2: return (Color){126, 78, 56, 255};
        case 3: return (Color){76, 72, 84, 255};
        default:
            return BlendColor((Color){66, 74, 71, 255}, kingdom, 0.16f);
    }
}

static uint32_t StreetForegroundBuildingMask(void)
{
    /* These are authored stage wings, not player-dependent cutaways. A given
       shot always presents the same architecture; only buildings between the
       low camera and that shot's walkable route are omitted. */
    switch (street_camera_rig.shot) {
        case 1: return UINT32_C(1) << 3;
        case 3:
            return (UINT32_C(1) << 3) | (UINT32_C(1) << 4);
        case 4:
        case 6:
            return (UINT32_C(1) << 4) | (UINT32_C(1) << 6);
        case 5:
            return (UINT32_C(1) << 8) | (UINT32_C(1) << 9);
        default: return 0;
    }
}

static void DrawWorldBuildings(Color kingdom, Vector3 focus)
{
    uint32_t foreground_mask = StreetForegroundBuildingMask();
    for (int32_t i = 0; i < (int32_t)(sizeof(WORLD_BUILDINGS) /
                                      sizeof(WORLD_BUILDINGS[0])); ++i) {
        const WorldBuilding *building = &WORLD_BUILDINGS[i];
        if ((foreground_mask & (UINT32_C(1) << i)) != 0) continue;
        /* The market footprint remains authoritative for collision, but its
           visible shell comes from the shared Blender library when present.
           Buildings no longer appear or disappear based on the hero's exact
           sightline: each fixed room keeps one coherent piece of scenery. */
        if (i == 2 && runtime_assets[RUNTIME_ASSET_MARKET].ready) continue;
        if (!SceneryFootprintVisible(building->footprint, focus)) continue;
        DrawBuilding(building->footprint.x, building->footprint.y,
                     building->footprint.width, building->footprint.height,
                     building->height, BuildingWallColor(building->style),
                     BuildingRoofColor(building->style, kingdom),
                     building->door, building->style, false);
    }
}

static bool DrawAuthoredMarket(const CcSettlement *place)
{
    RuntimeAsset *market = &runtime_assets[RUNTIME_ASSET_MARKET];
    if (!market->ready) return false;
    const Vector3 origin = {50.0f, 0.0f, 21.0f};
    const float scale = 1.70f;
    const WorldBuilding *building = &WORLD_BUILDINGS[2];
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
    return true;
}

static void DrawCastle(Color kingdom, Vector3 focus)
{
    for (int32_t i = 0; i < (int32_t)(sizeof(CASTLE_STRUCTURES) /
                                      sizeof(CASTLE_STRUCTURES[0])); ++i) {
        const WorldStructure *structure = &CASTLE_STRUCTURES[i];
        Rectangle footprint = structure->footprint;
        if (!SceneryFootprintVisible(footprint, focus)) continue;
        Color stone = i == 5 ? (Color){82, 80, 78, 255} :
                               (Color){100, 103, 98, 255};
        DrawBox((Vector3){footprint.x + footprint.width * 0.5f,
                          structure->height * 0.5f,
                          footprint.y + footprint.height * 0.5f},
                (Vector3){footprint.width, structure->height,
                          footprint.height}, stone);
        DrawBox((Vector3){footprint.x + footprint.width * 0.5f,
                          structure->height + 0.11f,
                          footprint.y + footprint.height * 0.5f},
                (Vector3){footprint.width + 0.18f, 0.22f,
                          footprint.height + 0.18f},
                i >= 8 ? kingdom : (Color){74, 77, 75, 255});
    }
    if (SceneryFootprintVisible((Rectangle){64.8f, 7.8f, 27.6f, 25.4f},
                                 focus)) {
        DrawBox((Vector3){78.5f, 1.20f, 22.03f},
                (Vector3){1.35f, 2.40f, 0.06f},
                (Color){43, 34, 37, 255});
        DrawBox((Vector3){76.30f, 7.65f, 30.84f},
                (Vector3){0.78f, 2.30f, 0.06f}, kingdom);
        DrawBox((Vector3){80.70f, 7.65f, 30.84f},
                (Vector3){0.78f, 2.30f, 0.06f}, kingdom);

        /* The open southern gate is the room's navigational promise. A high
           lintel and paired fire points frame the pass without putting an
           invisible portcullis across the traversable opening. */
        Color gate_stone = (Color){86, 88, 85, 255};
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
                    (Color){55, 43, 36, 255});
            DrawSmallSphere((Vector3){torch_x, 2.32f, 31.20f},
                            0.18f, WORLD_GOLD);
        }
    }
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

static void QueueFaceGlyph(Vector3 head_center, Vector3 body_base,
                           Vector3 forward, float face_depth, Color ink,
                           const CcNpcAppearance *appearance,
                           FaceGlyphExpression expression)
{
    if (face_glyph_count >= CC_FACE_GLYPH_MAX_COUNT) return;
    forward.y = 0.0f;
    forward = PhysicsNormalizeOr(forward, (Vector3){0.0f, 0.0f, 1.0f});
    FaceGlyph *glyph = &face_glyphs[face_glyph_count++];
    glyph->eye_point = PhysicsAdd(
        PhysicsAdd(head_center, (Vector3){0.0f, 0.045f, 0.0f}),
        PhysicsScale(forward, face_depth));
    glyph->body_base = body_base;
    glyph->forward = forward;
    glyph->ink = ink;
    glyph->skin_shadow = appearance != NULL ?
        ShadeColor(appearance->skin, 0.74f) : ink;
    glyph->expression = expression;
    glyph->face_width = appearance != NULL ? appearance->head_width : 1.0f;
    glyph->age = appearance != NULL ? appearance->age : 0.0f;
    glyph->beard_style = appearance != NULL ? appearance->beard_style : 0U;
    glyph->nose_style = appearance != NULL ? appearance->nose_style : 0U;
    glyph->scar_style = appearance != NULL ? appearance->scar_style : 0U;
}

static void DrawQueuedFaceGlyphs(Camera3D camera, int32_t width,
                                 int32_t height)
{
    for (int32_t i = 0; i < face_glyph_count; ++i) {
        const FaceGlyph *glyph = &face_glyphs[i];
        Vector3 to_camera = PhysicsSubtract(camera.position,
                                            glyph->eye_point);
        to_camera.y = 0.0f;
        to_camera = PhysicsNormalizeOr(to_camera,
                                       (Vector3){0.0f, 0.0f, 1.0f});
        float facing_camera = PhysicsDot(glyph->forward, to_camera);
        if (facing_camera < -0.12f) continue;

        Vector2 eye = GetWorldToScreenEx(glyph->eye_point, camera,
                                         width, height);
        Vector2 base = GetWorldToScreenEx(glyph->body_base, camera,
                                          width, height);
        Vector3 crown = PhysicsAdd(glyph->eye_point,
                                   (Vector3){0.0f, 0.22f, 0.0f});
        Vector2 top = GetWorldToScreenEx(crown, camera, width, height);
        float character_height = fabsf(base.y - top.y);
        if (character_height < 16.0f || eye.x < 1.0f ||
            eye.x >= (float)width - 1.0f || eye.y < 2.0f ||
            eye.y >= (float)height - 2.0f) continue;

        Vector3 right = PhysicsNormalizeOr(
            PhysicsCross((Vector3){0.0f, 1.0f, 0.0f}, glyph->forward),
            (Vector3){1.0f, 0.0f, 0.0f});
        Vector3 eye_size = {0.032f, 0.030f, 0.024f};
        Vector3 face_center = PhysicsAdd(
            glyph->eye_point, PhysicsScale(glyph->forward, 0.008f));
        if (facing_camera < 0.34f) {
            float side = PhysicsDot(
                PhysicsCross(glyph->forward, to_camera),
                (Vector3){0.0f, 1.0f, 0.0f}) < 0.0f ? -1.0f : 1.0f;
            DrawCubeV(PhysicsAdd(face_center,
                                 PhysicsScale(right, side * 0.034f)),
                      eye_size, glyph->ink);
            continue;
        }

        float eye_spacing = 0.039f * (0.92f + glyph->face_width * 0.08f);
        Vector3 left_eye = PhysicsAdd(face_center,
                                      PhysicsScale(right, -eye_spacing));
        Vector3 right_eye = PhysicsAdd(face_center,
                                       PhysicsScale(right, eye_spacing));
        if (glyph->expression == FACE_GLYPH_HURT) {
            left_eye.y += 0.012f;
            right_eye.y -= 0.012f;
        }
        DrawCubeV(left_eye, eye_size, glyph->ink);
        DrawCubeV(right_eye, eye_size, glyph->ink);
        if (character_height >= 38.0f) {
            float brow_height = glyph->expression == FACE_GLYPH_FOCUSED ?
                                 0.036f : 0.050f;
            Vector3 left_brow = PhysicsAdd(left_eye,
                (Vector3){0.0f, brow_height, 0.0f});
            Vector3 right_brow = PhysicsAdd(right_eye,
                (Vector3){0.0f, brow_height, 0.0f});
            Vector3 brow_size = {0.042f, 0.015f, 0.020f};
            DrawCubeV(left_brow, brow_size, glyph->ink);
            DrawCubeV(right_brow, brow_size, glyph->ink);
            static const Vector3 nose_sizes[4] = {
                {0.020f, 0.030f, 0.022f},
                {0.027f, 0.035f, 0.026f},
                {0.034f, 0.028f, 0.030f},
                {0.024f, 0.043f, 0.026f},
            };
            Vector3 nose = PhysicsAdd(
                face_center, (Vector3){0.0f, -0.026f, 0.006f});
            DrawCubeV(nose, nose_sizes[glyph->nose_style % 4U],
                      glyph->skin_shadow);
            DrawCubeV(PhysicsAdd(face_center,
                                 (Vector3){0.0f, -0.065f, 0.0f}),
                      (Vector3){0.045f, 0.014f, 0.020f}, glyph->ink);
            if (glyph->beard_style != 0U) {
                float beard_height = 0.020f +
                    (float)glyph->beard_style * 0.018f;
                DrawCubeV(PhysicsAdd(face_center,
                                     (Vector3){0.0f, -0.090f, -0.002f}),
                          (Vector3){0.092f, beard_height, 0.024f},
                          glyph->ink);
            }
            if (glyph->scar_style != 0U) {
                float side = glyph->scar_style == 2U ? -1.0f : 1.0f;
                Vector3 scar = PhysicsAdd(
                    face_center,
                    PhysicsAdd(PhysicsScale(right, side * 0.066f),
                               (Vector3){0.0f, -0.010f, 0.010f}));
                DrawCubeV(scar, (Vector3){0.012f, 0.052f, 0.014f},
                          BlendColor(glyph->skin_shadow, glyph->ink, 0.52f));
            }
            if (glyph->age > 0.68f) {
                for (int32_t side = -1; side <= 1; side += 2) {
                    Vector3 age_mark = PhysicsAdd(
                        face_center,
                        PhysicsAdd(PhysicsScale(right, (float)side * 0.077f),
                                   (Vector3){0.0f, -0.044f, 0.004f}));
                    DrawCubeV(age_mark,
                              (Vector3){0.022f, 0.009f, 0.012f},
                              glyph->skin_shadow);
                }
            }
        }
    }
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
    Color facial_ink = BlendColor(appearance->hair,
                                  (Color){24, 23, 22, 255}, 0.58f);
    FaceGlyphExpression glyph_expression =
        appearance->role == CC_NPC_ROLE_GUARD ||
        appearance->role == CC_NPC_ROLE_RAIDER ? FACE_GLYPH_FOCUSED :
                                                  FACE_GLYPH_NEUTRAL;
    QueueFaceGlyph(head,
                   PhysicsAdd(head, (Vector3){0.0f, -1.62f * scale, 0.0f}),
                   forward, head_depth * 0.94f, facial_ink, appearance,
                   glyph_expression);
    Vector3 face = PhysicsAdd(head, PhysicsScale(forward, head_depth * 0.92f));
    static const Vector3 nose_scale[4] = {
        {0.018f, 0.026f, 0.020f}, {0.022f, 0.032f, 0.025f},
        {0.029f, 0.023f, 0.027f}, {0.020f, 0.039f, 0.023f},
    };
    Vector3 nose_size = nose_scale[appearance->nose_style % 4U];
    nose_size = PhysicsScale(nose_size, scale);
    DrawCharacterEllipsoid(
        PhysicsAdd(face, (Vector3){0.0f, 0.015f * scale, 0.0f}),
        nose_size, ShadeColor(appearance->skin, 0.86f));
    float eye_spacing = (0.047f +
        (float)((appearance->seed >> 3U) & 3U) * 0.0025f) * scale;
    float brow_tilt = (((appearance->seed >> 7U) & 1U) != 0U ?
                       0.010f : -0.004f) * scale;
    for (int32_t side = -1; side <= 1; side += 2) {
        Vector3 eye = PhysicsAdd(face, PhysicsScale(right,
            (float)side * eye_spacing));
        eye.y += 0.045f * scale;
        eye = PhysicsAdd(eye, PhysicsScale(forward, 0.010f * scale));
        DrawSmallSphere(eye, 0.013f * scale, facial_ink);

        Vector3 brow_outer = PhysicsAdd(face, PhysicsScale(
            right, (float)side * (eye_spacing + 0.027f * scale)));
        Vector3 brow_inner = PhysicsAdd(face, PhysicsScale(
            right, (float)side * (eye_spacing - 0.025f * scale)));
        brow_outer.y += 0.086f * scale + brow_tilt;
        brow_inner.y += 0.078f * scale - brow_tilt;
        brow_outer = PhysicsAdd(brow_outer,
                                 PhysicsScale(forward, 0.014f * scale));
        brow_inner = PhysicsAdd(brow_inner,
                                 PhysicsScale(forward, 0.014f * scale));
        DrawCylinderEx(brow_outer, brow_inner, 0.008f * scale,
                       0.008f * scale, 5, facial_ink);
    }
    Vector3 mouth_left = PhysicsAdd(face,
        PhysicsScale(right, -0.036f * scale));
    Vector3 mouth_right = PhysicsAdd(face,
        PhysicsScale(right, 0.036f * scale));
    mouth_left.y -= 0.056f * scale;
    mouth_right.y -= 0.056f * scale;
    mouth_left = PhysicsAdd(mouth_left,
                            PhysicsScale(forward, 0.011f * scale));
    mouth_right = PhysicsAdd(mouth_right,
                             PhysicsScale(forward, 0.011f * scale));
    DrawCylinderEx(mouth_left, mouth_right, 0.006f * scale,
                   0.006f * scale, 5, facial_ink);
    if (appearance->scar_style != 0U) {
        float side = appearance->scar_style == 2U ? -1.0f : 1.0f;
        Vector3 scar_top = PhysicsAdd(
            face, PhysicsScale(right, side * 0.070f * scale));
        scar_top.y += 0.047f * scale;
        scar_top = PhysicsAdd(scar_top,
                              PhysicsScale(forward, 0.018f * scale));
        Vector3 scar_bottom = PhysicsAdd(
            scar_top, (Vector3){side * -0.012f * scale,
                                -0.070f * scale, 0.0f});
        DrawCylinderEx(scar_top, scar_bottom, 0.004f * scale,
                       0.004f * scale, 4,
                       ShadeColor(appearance->skin, 0.54f));
    }

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
    DrawBox(contact_shadow,
            (Vector3){0.62f * scale * width * silhouette_gain, 0.012f,
                      0.43f * scale * width * silhouette_gain},
            (Color){2, 7, 10, 98});
    SetNpcPalette(appearance, 0.50f);
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
    int32_t hair = (int32_t)appearance->hair_style %
                   CC_NPC_DYNAMIC_HAIR_COUNT;
    (void)DrawNpcDynamicModule(
        (NpcDynamicModuleId)(NPC_DYNAMIC_HAIR_0 + hair), identity_head,
        appearance->hair);
    if ((appearance->equipment & CC_NPC_EQUIPMENT_HEADWEAR) != 0U) {
        NpcDynamicModuleId headwear = NpcHeadwearModule(
            appearance->headwear_style);
        Color headwear_color = appearance->headwear_style == 0U ?
            appearance->metal : appearance->outer;
        (void)DrawNpcDynamicModule(headwear, identity_head, headwear_color);
    }
    FaceGlyphExpression expression =
        appearance->role == CC_NPC_ROLE_GUARD ||
        appearance->role == CC_NPC_ROLE_RAIDER ? FACE_GLYPH_FOCUSED :
                                                 FACE_GLYPH_NEUTRAL;
    Vector3 body_base = position;
    body_base.y = 0.0f;
    QueueFaceGlyph(
        head, body_base, head_forward,
        0.145f * appearance->head_depth * scale,
        BlendColor(appearance->hair, (Color){24, 23, 22, 255}, 0.58f),
        appearance, expression);
    if (visual_style.hero_ready) {
        float ink_strength = 0.68f;
        SetShaderValue(visual_style.hero,
                       visual_style.hero_ink_strength_location,
                       &ink_strength, SHADER_UNIFORM_FLOAT);
    }
    return true;
}

static void DrawNpcFigure3D(Vector3 position, float size_hint, float yaw,
                            uint32_t seed, CcNpcRole role, Color accent,
                            float phase, CcTraversalMode mode)
{
    CcNpcAppearance appearance = CcNpcAppearanceGenerate(seed, role, accent);
    if (!draw_hero_rig_debug && DrawNpcArchetype3D(
            position, size_hint, yaw, phase, mode, &appearance)) {
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

    DrawCylinder((Vector3){position.x, position.y + 0.005f, position.z},
                 0.31f * scale, 0.31f * scale, 0.012f, 18,
                 (Color){2, 7, 10, 115});
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

static void DrawWayfarerCrown(const CcHumanoidSkinPose *skin)
{
    Vector3 up = FromLimbVector(skin->body_up);
    Vector3 right = FromLimbVector(skin->body_right);
    Vector3 forward = FromLimbVector(skin->body_forward);
    Vector3 head_top = FromLimbVector(
        skin->bones[CC_HUMANOID_SKIN_HEAD].tail);
    Vector3 band = PhysicsAdd(
        PhysicsAdd(head_top, PhysicsScale(up, 0.030f)),
        PhysicsScale(forward, -0.004f));
    Color dark_gold = (Color){124, 86, 34, 255};
    Color crown_gold = (Color){204, 153, 57, 255};
    DrawCylinderEx(PhysicsAdd(band, PhysicsScale(up, -0.024f)),
                   PhysicsAdd(band, PhysicsScale(up, 0.018f)),
                   0.095f, 0.087f, 7, dark_gold);
    for (int32_t prong = -1; prong <= 1; ++prong) {
        float height = prong == 0 ? 0.112f : 0.082f;
        Vector3 root = PhysicsAdd(
            band, PhysicsScale(right, (float)prong * 0.055f));
        root = PhysicsAdd(root, PhysicsScale(forward, -0.010f));
        Vector3 tip = PhysicsAdd(root, PhysicsScale(up, height));
        DrawCylinderEx(root, tip, 0.027f, 0.005f, 5, crown_gold);
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

static bool DrawNpcDynamicModule(NpcDynamicModuleId id, Matrix transform,
                                 Color color)
{
    (void)color;
    if (id < 0 || id >= NPC_DYNAMIC_MODULE_COUNT) return false;
    NpcDynamicModuleCache *module = &npc_dynamic_modules[id];
    if (!module->ready || module->model.meshCount != 1 ||
        module->model.materialCount < 1) return false;
    /* COLOR_0 stores the semantic palette slot. Runtime variation belongs in
       the indexed shader, so each generated module keeps one neutral material
       instead of mutating raylib material state for every body part. */
    module->model.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
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

static bool NpcDynamicCoreReady(int32_t hair_style)
{
    static const NpcDynamicModuleId core[] = {
        NPC_DYNAMIC_TORSO, NPC_DYNAMIC_PELVIS, NPC_DYNAMIC_UPPER_ARM,
        NPC_DYNAMIC_FOREARM, NPC_DYNAMIC_THIGH, NPC_DYNAMIC_SHIN,
        NPC_DYNAMIC_HAND, NPC_DYNAMIC_FOOT, NPC_DYNAMIC_HEAD,
    };
    for (int32_t i = 0;
         i < (int32_t)(sizeof(core) / sizeof(core[0])); ++i) {
        if (!npc_dynamic_modules[core[i]].ready) return false;
    }
    int32_t hair = hair_style % CC_NPC_DYNAMIC_HAIR_COUNT;
    if (hair < 0) hair += CC_NPC_DYNAMIC_HAIR_COUNT;
    return npc_dynamic_modules[NPC_DYNAMIC_HAIR_0 + hair].ready;
}

static bool DrawDynamicNpcModules(const CcLocalAgent *agent,
                                  const CcHumanoidSkinPose *skin,
                                  const CcNpcAppearance *appearance)
{
    if (agent == NULL || skin == NULL || appearance == NULL || !skin->valid ||
        !NpcDynamicCoreReady((int32_t)appearance->hair_style)) return false;

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
    Color outer = appearance->outer;
    Color trousers = appearance->trousers;
    if (CombatIsDefeated(&agent->combat)) {
        outer = BlendColor(outer, (Color){50, 49, 52, 255}, 0.72f);
        trousers = BlendColor(trousers, (Color){42, 42, 44, 255}, 0.72f);
    } else if (agent->combat.hit_flash_seconds > 0.0f) {
        outer = BlendColor(outer, WORLD_INK, 0.72f);
    }
    CcNpcAppearance palette_appearance = *appearance;
    palette_appearance.outer = outer;
    palette_appearance.trousers = trousers;
    SetNpcPalette(&palette_appearance, 0.68f);

    const CcHumanoidSkinBonePose *spine =
        &skin->bones[CC_HUMANOID_SKIN_SPINE];
    Vector3 torso_base = FromLimbVector(spine->head);
    Vector3 torso_top = FromLimbVector(
        skin->bones[CC_HUMANOID_SKIN_CHEST].tail);
    float torso_length = PhysicsLength(
        PhysicsSubtract(torso_top, torso_base));
    bool drew = DrawNpcDynamicModule(
        NPC_DYNAMIC_TORSO,
        NpcModuleTransform(torso_base, body_right, body_up, body_forward,
                           (Vector3){0.54f * mass * appearance->shoulder_scale,
                                     torso_length, 0.34f * mass}),
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
                           (Vector3){0.43f * mass, pelvis_length,
                                     0.29f * mass}),
        trousers) && drew;

    static const CcHumanoidSkinBone upper_arms[] = {
        CC_HUMANOID_SKIN_UPPER_ARM_LEFT,
        CC_HUMANOID_SKIN_UPPER_ARM_RIGHT,
    };
    static const CcHumanoidSkinBone forearms[] = {
        CC_HUMANOID_SKIN_FOREARM_LEFT,
        CC_HUMANOID_SKIN_FOREARM_RIGHT,
    };
    static const CcHumanoidSkinBone hands[] = {
        CC_HUMANOID_SKIN_HAND_LEFT,
        CC_HUMANOID_SKIN_HAND_RIGHT,
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
    float arm_width = (0.135f + muscle * 0.020f) * mass;
    float leg_width = (0.175f + muscle * 0.025f) * mass;
    for (int32_t side = 0; side < 2; ++side) {
        drew = DrawNpcDynamicBoneModule(
            NPC_DYNAMIC_UPPER_ARM, &skin->bones[upper_arms[side]],
            arm_width, arm_width * 0.92f, outer) && drew;
        drew = DrawNpcDynamicBoneModule(
            NPC_DYNAMIC_FOREARM, &skin->bones[forearms[side]],
            arm_width * 0.86f, arm_width * 0.80f,
            appearance->underlayer) && drew;
        const CcHumanoidSkinBonePose *hand = &skin->bones[hands[side]];
        drew = DrawNpcDynamicModule(
            NPC_DYNAMIC_HAND,
            NpcModuleTransform(FromLimbVector(hand->head),
                               FromLimbVector(hand->right),
                               FromLimbVector(hand->up),
                               FromLimbVector(hand->forward),
                               (Vector3){0.135f, 0.165f, 0.125f}),
            appearance->skin) && drew;
        drew = DrawNpcDynamicBoneModule(
            NPC_DYNAMIC_THIGH, &skin->bones[thighs[side]],
            leg_width, leg_width * 0.94f, trousers) && drew;
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
                               (Vector3){0.23f * mass, foot_length,
                                         0.20f * mass}),
            appearance->leather) && drew;
    }

    const CcHumanoidSkinBonePose *head_bone =
        &skin->bones[CC_HUMANOID_SKIN_HEAD];
    Vector3 head = FromLimbVector(
        skin->sockets[CC_HUMANOID_SOCKET_HEAD].position);
    Vector3 head_scale = {0.36f * appearance->head_width,
                          0.40f, 0.33f * appearance->head_depth};
    Matrix head_transform = NpcModuleTransform(
        head, FromLimbVector(head_bone->right),
        FromLimbVector(head_bone->up), FromLimbVector(head_bone->forward),
        head_scale);
    drew = DrawNpcDynamicModule(NPC_DYNAMIC_HEAD, head_transform,
                                appearance->skin) && drew;
    int32_t hair = (int32_t)appearance->hair_style %
                   CC_NPC_DYNAMIC_HAIR_COUNT;
    drew = DrawNpcDynamicModule(
        (NpcDynamicModuleId)(NPC_DYNAMIC_HAIR_0 + hair), head_transform,
        appearance->hair) && drew;

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
                               (Vector3){0.62f * mass, 0.64f, 0.50f}),
            ShadeColor(appearance->outer, 0.68f));
    }
    if ((appearance->equipment & CC_NPC_EQUIPMENT_ARMOR) != 0U) {
        (void)DrawNpcDynamicModule(
            NPC_DYNAMIC_CHEST_PLATE,
            NpcModuleTransform(chest_front, body_right, body_up,
                               body_forward,
                               (Vector3){0.43f * mass, 0.36f, 0.28f}),
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
                                   (Vector3){0.22f * mass, 0.20f,
                                             0.21f * mass}),
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
        (void)DrawNpcDynamicModule(
            headwear, head_transform, headwear_color);
    }

    FaceGlyphExpression expression =
        appearance->role == CC_NPC_ROLE_GUARD ||
        appearance->role == CC_NPC_ROLE_RAIDER ? FACE_GLYPH_FOCUSED :
                                                 FACE_GLYPH_NEUTRAL;
    if (agent->combat.hit_flash_seconds > 0.0f) {
        expression = FACE_GLYPH_HURT;
    } else if (agent->combat.focus_valid ||
               agent->humanoid.action == CC_HUMANOID_ACTION_GUARD ||
               agent->humanoid.action == CC_HUMANOID_ACTION_STRIKE) {
        expression = FACE_GLYPH_FOCUSED;
    }
    QueueFaceGlyph(head, agent->position, body_forward,
                   0.165f * appearance->head_depth,
                   BlendColor(appearance->hair, (Color){24, 23, 22, 255},
                              0.58f),
                   appearance,
                   expression);
    return drew;
}

static void DrawBiomechanicalBiped(const CcLocalAgent *agent)
{
    const CcHumanoidGait *gait = &agent->humanoid;
    const CcHumanoidPose *pose = AgentRenderPose(agent);
    CcHumanoidSkinPose skin;
    CcHumanoidSkinPoseResolve(pose, &skin);
    if (!skin.valid) return;
    bool modular_hero = agent->crowned;
    if (agent->crowned) {
        DrawCylinder((Vector3){agent->position.x,
                               agent->position.y + 0.008f,
                               agent->position.z},
                     0.35f, 0.35f, 0.014f, 24,
                     (Color){3, 10, 14, 118});
        DrawCylinderWires((Vector3){agent->position.x,
                                    agent->position.y + 0.010f,
                                    agent->position.z},
                          0.39f, 0.39f, 0.018f, 24,
                          Fade(WORLD_TEAL, 0.82f));
    }
    bool hero_skin_updated = modular_hero &&
        DrawHeroSkin(&skin, &agent->render_cape, WHITE, true);
    if (hero_skin_updated) {
        CcLocalRendererRecordBiped(true);
        if (draw_hero_rig_debug) {
            DrawHeroSkinRigOverlay(gait, &skin, &agent->render_cape);
        }
        UseCharacterLighting();
        DrawWayfarerCrown(&skin);
        RestoreWorldLighting();
        FaceGlyphExpression expression = FACE_GLYPH_NEUTRAL;
        if (agent->combat.hit_flash_seconds > 0.0f) {
            expression = FACE_GLYPH_HURT;
        } else if (agent->combat.focus_valid ||
                   agent->humanoid.action == CC_HUMANOID_ACTION_GUARD ||
                   agent->humanoid.action == CC_HUMANOID_ACTION_STRIKE) {
            expression = FACE_GLYPH_FOCUSED;
        }
        QueueFaceGlyph(
            FromLimbVector(
                skin.sockets[CC_HUMANOID_SOCKET_HEAD].position),
            agent->position, FromLimbVector(skin.body_forward), 0.12f,
            (Color){24, 22, 23, 255}, &agent->appearance, expression);
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
    CcNpcAppearance gameplay_appearance = agent->appearance;
    if (agent->crowned) {
        gameplay_appearance.role = CC_NPC_ROLE_WAYFARER;
        gameplay_appearance.equipment |=
            (uint32_t)(CC_NPC_EQUIPMENT_MANTLE |
                       CC_NPC_EQUIPMENT_ARMOR |
                       CC_NPC_EQUIPMENT_SATCHEL);
        gameplay_appearance.body_mass = 0.92f;
        gameplay_appearance.shoulder_scale = 1.02f;
        gameplay_appearance.head_width = 1.04f;
        gameplay_appearance.head_depth = 1.02f;
        gameplay_appearance.garment_style = 1;
        gameplay_appearance.skin = (Color){174, 126, 88, 255};
        gameplay_appearance.hair = (Color){45, 32, 29, 255};
        gameplay_appearance.underlayer = (Color){108, 91, 69, 255};
        gameplay_appearance.outer = (Color){48, 105, 103, 255};
        gameplay_appearance.trousers = (Color){49, 62, 63, 255};
        gameplay_appearance.leather = (Color){82, 50, 35, 255};
        gameplay_appearance.metal = (Color){119, 48, 55, 255};
        gameplay_appearance.accent = WORLD_GOLD;
    }
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
    if (agent->crowned) {
        DrawWayfarerCrown(&skin);
    }

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
    Vector3 shadow_size = fallen ? (Vector3){1.20f, 0.014f, 0.56f} :
                                   (Vector3){0.70f, 0.014f, 0.46f};
    DrawBox(shadow, shadow_size, (Color){3, 8, 10, fallen ? 102 : 82});
    DrawCubeWiresV((Vector3){shadow.x, shadow.y + 0.003f, shadow.z},
                   (Vector3){shadow_size.x + 0.03f, 0.016f,
                             shadow_size.z + 0.03f},
                   (Color){21, 35, 37, 72});

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
    Vector3 side = {cosf(agent->facing_yaw), 0.0f,
                    -sinf(agent->facing_yaw)};
    Vector3 back = {-sinf(agent->facing_yaw), 0.0f,
                    -cosf(agent->facing_yaw)};
    for (int32_t mote = 0; mote < 3; ++mote) {
        float lateral = ((float)mote - 1.0f) * 0.12f;
        Vector3 center = PhysicsAdd(
            heel, PhysicsAdd(PhysicsScale(side, lateral),
                             PhysicsScale(back, 0.05f + (float)mote * 0.04f)));
        center.x = SnapContactCoordinate(center.x);
        center.z = SnapContactCoordinate(center.z);
        center.y += 0.035f + (float)mote * 0.022f;
        float size = (0.07f + (float)mote * 0.018f) * dust;
        DrawBox(center, (Vector3){size, size, size},
                Fade((Color){150, 125, 86, 255}, dust * 0.54f));
    }
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
        DrawSphereWires(body, 0.32f, 10, 10, WORLD_GOLD);
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
}

static void DrawNotice3D(const CcSim *sim)
{
    float x = CC_LOCAL_NOTICE_X;
    float z = CC_LOCAL_NOTICE_Z;
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
}

static void DrawDungeon3D(const CcDungeon *dungeon)
{
    float x = CC_LOCAL_DUNGEON_X;
    float z = DUNGEON_FOOTPRINT.y + DUNGEON_FOOTPRINT.height * 0.5f;
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
}

static void DrawTree(float x, float z, Color leaves)
{
    DrawCylinder((Vector3){x, 0.0f, z}, 0.13f, 0.10f, 1.30f, 7,
                 (Color){80, 57, 43, 255});
    DrawScenerySphere((Vector3){x, 1.68f, z}, 0.58f, leaves);
    DrawSphereWires((Vector3){x, 1.68f, z}, 0.59f, 7, 7, Fade(WORLD_INK, 0.12f));
}

static void DrawWorldTrees(Vector3 focus)
{
    static const Vector2 trees[] = {
        {2.5f, 58.0f}, {6.0f, 62.0f}, {10.0f, 56.0f}, {14.0f, 65.0f},
        {18.0f, 58.0f}, {22.0f, 64.0f}, {27.0f, 60.0f}, {33.0f, 66.0f},
        {48.0f, 63.0f}, {54.0f, 59.0f}, {59.0f, 65.0f}, {66.0f, 59.0f},
        {73.0f, 64.0f}, {81.0f, 59.0f}, {88.0f, 64.0f}, {93.0f, 56.0f},
        {18.0f, 5.0f}, {24.0f, 8.0f}, {31.0f, 5.5f}, {49.0f, 6.0f},
        {57.0f, 5.0f}, {62.0f, 3.5f}, {4.0f, 14.0f}, {16.0f, 14.5f},
        {17.0f, 45.0f}, {20.0f, 52.0f}, {57.5f, 41.0f}, {74.0f, 39.0f},
        {86.0f, 40.0f}, {93.0f, 36.0f}
    };
    for (int32_t i = 0; i < (int32_t)(sizeof(trees) / sizeof(trees[0])); ++i) {
        if (!SceneryPointVisible(trees[i].x, trees[i].y, focus)) continue;
        Color leaves = (i & 1) != 0 ? (Color){54, 105, 85, 255} :
                                      (Color){58, 119, 91, 255};
        DrawTree(trees[i].x, trees[i].y, leaves);
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

static void DrawStreetTraversalPortals(const CcLocalAgent *agent,
                                       Camera3D camera, Rectangle viewport,
                                       int32_t art_width,
                                       int32_t art_height)
{
    int32_t count = CcLocalAgentStreetPortalCount(agent);
    for (int32_t portal_index = 0; portal_index < count; ++portal_index) {
        ResolvedStreetPortal portal = {0};
        if (!ResolveStreetPortal(agent, portal_index, &portal)) continue;
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
            0.28f, 4, (Color){4, 10, 14, 224});
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
    int32_t width = (int32_t)lroundf(viewport.width);
    int32_t height = (int32_t)lroundf(viewport.height);
    for (int32_t i = 0; i < count; ++i) {
        Vector2 screen = GetWorldToScreenEx(labels[i].point, camera, width, height);
        if (screen.x < -120.0f || screen.x > viewport.width + 120.0f ||
            screen.y < -40.0f || screen.y > viewport.height + 40.0f) continue;
        int text_width = CcOverlayMeasureText(labels[i].text, 10);
        float bubble_width = (float)text_width + 10.0f;
        float bubble_x = viewport.x + screen.x - bubble_width * 0.5f;
        float bubble_y = viewport.y + screen.y - 5.0f;
        bubble_x = fmaxf(viewport.x + 4.0f,
                         fminf(bubble_x,
                               viewport.x + viewport.width - bubble_width -
                                   4.0f));
        bubble_y = fmaxf(viewport.y + 4.0f,
                         fminf(bubble_y,
                               viewport.y + viewport.height - 20.0f));
        DrawRectangleRounded((Rectangle){bubble_x, bubble_y,
                                         bubble_width, 16.0f},
                             0.30f, 4, (Color){4, 10, 14, 210});
        CcOverlayDrawText(labels[i].text, (int)lroundf(bubble_x) + 5,
                 (int)lroundf(bubble_y) + 3, 10, labels[i].color);
    }
}

static void DrawCombatBar(const CcLocalAgent *agent, Camera3D camera,
                          Rectangle viewport, Color accent)
{
    Vector3 anchor = {agent->position.x, agent->position.y + 2.18f,
                      agent->position.z};
    Vector2 screen = GetWorldToScreenEx(
        anchor, camera, (int32_t)lroundf(viewport.width),
        (int32_t)lroundf(viewport.height));
    screen.x += viewport.x;
    screen.y += viewport.y;
    const float bar_width = 44.0f;
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

static void DrawCombatImpact(const CcLocalAgent *agent)
{
    if (agent->combat.hit_flash_seconds <= 0.0f) return;
    float pulse = 0.13f + agent->combat.hit_flash_seconds * 1.05f;
    Vector3 point = agent->combat.impact_valid ? agent->combat.impact_point :
        (Vector3){agent->position.x, agent->position.y + 1.02f,
                  agent->position.z};
    DrawSphereWires(point, pulse, 7, 7, WORLD_GOLD);
    if (agent->combat.impact_valid) {
        Vector3 direction = PhysicsNormalizeOr(
            agent->combat.impact_direction,
            (Vector3){0.0f, 0.0f, 1.0f});
        Vector3 side = PhysicsNormalizeOr(
            PhysicsCross(direction, (Vector3){0.0f, 1.0f, 0.0f}),
            (Vector3){1.0f, 0.0f, 0.0f});
        float spark = pulse * 1.65f;
        DrawLine3D(PhysicsAdd(point, PhysicsScale(side, -spark)),
                   PhysicsAdd(point, PhysicsScale(side, spark)), WORLD_INK);
        DrawLine3D(PhysicsAdd(point, (Vector3){0.0f, -spark * 0.72f, 0.0f}),
                   PhysicsAdd(point, (Vector3){0.0f, spark * 0.72f, 0.0f}),
                   WHITE);
        Vector3 trace = PhysicsAdd(
            point, PhysicsScale(direction,
                                0.18f + agent->combat.impact_speed * 0.018f));
        DrawLine3D(point, trace, WORLD_GOLD);
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
    DrawCylinderWires(ground, radius, radius, 0.035f, 24,
                      Fade(color, 0.72f));
    DrawCylinderWires(ground, radius * 0.72f, radius * 0.72f, 0.040f, 20,
                      Fade(color, 0.42f));
}

static void DrawCombatFootprint(const CcLocalAgent *agent, Color color)
{
    if (agent == NULL || !CombatCanAct(&agent->combat) ||
        agent->humanoid.ragdoll.active) return;
    Vector3 ground = {agent->position.x, agent->position.y + 0.012f,
                      agent->position.z};
    DrawCylinder(ground, 0.31f, 0.31f, 0.012f, 20,
                 Fade(WORLD_VOID, 0.38f));
    DrawCylinderWires((Vector3){ground.x, ground.y + 0.004f, ground.z},
                      0.34f, 0.34f, 0.018f, 20, Fade(color, 0.58f));
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
                  (Color){196, 211, 212, 255};

    DrawCylinderEx(pommel, guard_center, 0.032f, 0.032f, 7, grip);
    DrawSmallSphere(pommel, 0.045f, guard_color);
    DrawCylinderEx(PhysicsAdd(guard_center, PhysicsScale(right, -0.14f)),
                   PhysicsAdd(guard_center, PhysicsScale(right, 0.14f)),
                   0.032f, 0.026f, 7, guard_color);
    DrawCylinderEx(blade_start, blade_tip, 0.052f, 0.012f, 5, steel);
    DrawLine3D(blade_start, blade_tip,
               agent->combat.team == CC_COMBAT_PLAYER ? WHITE : WORLD_INK);

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
    DrawCylinderWires((Vector3){target->position.x, surface,
                                target->position.z},
                      0.58f, 0.58f, 0.06f, 28, WORLD_GOLD);
    DrawCylinderWires((Vector3){target->position.x, surface + 0.015f,
                                target->position.z},
                      0.48f, 0.48f, 0.06f, 28, WORLD_DANGER);
    DrawLine3D((Vector3){target->position.x, surface + 1.95f,
                         target->position.z},
               (Vector3){target->position.x, surface + 2.25f,
                         target->position.z}, WORLD_GOLD);
    DrawSphereWires((Vector3){target->position.x, surface + 2.28f,
                              target->position.z},
                    0.10f, 7, 7, WORLD_GOLD);
}

static void PresentTarget(RenderTexture2D target, Rectangle destination)
{
    Rectangle source = {0.0f, 0.0f, (float)target.texture.width,
                        -(float)target.texture.height};
    if (visual_style.grade_ready) {
        float resolution[2] = {(float)target.texture.width,
                               (float)target.texture.height};
        SetShaderValue(visual_style.grade, visual_style.resolution_location,
                       resolution, SHADER_UNIFORM_VEC2);
        BeginShaderMode(visual_style.grade);
    }
    DrawTexturePro(target.texture, source, destination, (Vector2){0.0f, 0.0f},
                   0.0f, WHITE);
    if (visual_style.grade_ready) EndShaderMode();
}

static void BeginWorldLighting(Camera3D camera, Color environment)
{
    if (!visual_style.world_ready) return;
    Vector3 light_direction = Vector3Normalize(
        (Vector3){-0.34f, 0.87f, 0.36f});
    float direction[3] = {light_direction.x, light_direction.y,
                          light_direction.z};
    const float light_color[3] = {1.10f, 0.91f, 0.72f};
    const float ambient_color[3] = {0.61f, 0.64f, 0.68f};
    float camera_position[3] = {camera.position.x, camera.position.y,
                                camera.position.z};
    float fog_color[3] = {(float)environment.r / 255.0f,
                          (float)environment.g / 255.0f,
                          (float)environment.b / 255.0f};
    const float fog_near = 28.0f;
    const float fog_far = 54.0f;
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
    SetShaderValue(visual_style.world, visual_style.fog_color_location,
                   fog_color, SHADER_UNIFORM_VEC3);
    SetShaderValue(visual_style.world, visual_style.fog_near_location,
                   &fog_near, SHADER_UNIFORM_FLOAT);
    SetShaderValue(visual_style.world, visual_style.fog_far_location,
                   &fog_far, SHADER_UNIFORM_FLOAT);
    if (visual_style.hero_ready) {
        SetShaderValue(visual_style.hero,
                       visual_style.hero_light_direction_location, direction,
                       SHADER_UNIFORM_VEC3);
        SetShaderValue(visual_style.hero,
                       visual_style.hero_camera_position_location,
                       camera_position, SHADER_UNIFORM_VEC3);
        SetShaderValue(visual_style.hero,
                       visual_style.hero_fog_color_location, fog_color,
                       SHADER_UNIFORM_VEC3);
        SetShaderValue(visual_style.hero,
                       visual_style.hero_fog_near_location, &fog_near,
                       SHADER_UNIFORM_FLOAT);
        SetShaderValue(visual_style.hero,
                       visual_style.hero_fog_far_location, &fog_far,
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
                       visual_style.npc_fog_color_location, fog_color,
                       SHADER_UNIFORM_VEC3);
        SetShaderValue(visual_style.npc,
                       visual_style.npc_fog_near_location, &fog_near,
                       SHADER_UNIFORM_FLOAT);
        SetShaderValue(visual_style.npc,
                       visual_style.npc_fog_far_location, &fog_far,
                       SHADER_UNIFORM_FLOAT);
    }
    BeginShaderMode(visual_style.world);
}

static void EndWorldLighting(void)
{
    if (visual_style.world_ready) EndShaderMode();
}

static void DrawAgentPath(const CcLocalAgent *agent, bool market_interior)
{
    (void)market_interior;
    if (!agent->exact_target_valid && !agent->target_valid) return;
    Vector3 target = agent->target_point;
    DrawCylinder((Vector3){target.x, target.y + 0.018f, target.z},
                 0.24f, 0.24f, 0.036f, 24, Fade(WORLD_GOLD, 0.42f));
    DrawCylinderWires((Vector3){target.x, target.y + 0.020f, target.z},
                      0.26f, 0.26f, 0.040f, 24, WORLD_GOLD);
    DrawLine3D((Vector3){agent->position.x, agent->position.y + 0.04f,
                         agent->position.z},
               (Vector3){target.x, target.y + 0.04f, target.z},
               Fade(WORLD_GOLD, 0.30f));
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

static void DrawObstacleCourse(void)
{
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

static void DrawCourseRunners(const CcLocalCourse *course)
{
    if (course == NULL) return;
    if (course->situation_witness_active) {
        DrawRobotShell(&course->situation_witness);
    }
    for (int32_t i = 0; i < CC_LOCAL_TRAVELLER_COUNT; ++i) {
        const CcLocalTraveller *traveller = &course->travellers[i];
        if (traveller->active) DrawRobotShell(&traveller->agent);
    }
    Vector3 threat = CourseThreatCenter(course);
    if (course->alarm_active) {
        for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
            const CcLocalAgent *raider = &course->raiders[i];
            DrawCombatFootprint(raider, WORLD_DANGER);
            DrawRobotShell(raider);
            DrawCombatSword(raider);
            DrawCombatSkillTell(raider);
            DrawCombatImpact(raider);
            DrawSphereWires((Vector3){raider->position.x,
                                      raider->position.y + 2.05f,
                                      raider->position.z},
                            0.075f, 6, 6, WORLD_DANGER);
        }
    }
    for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
        const CcLocalCourseRunner *runner = &course->runners[i];
        if (course->alarm_active) {
            DrawCombatFootprint(&runner->agent, runner->marker_color);
        }
        if (runner->agent.exact_target_valid) {
            Vector3 target = runner->agent.target_point;
            DrawCylinderWires((Vector3){target.x, target.y + 0.025f, target.z},
                              0.13f, 0.13f, 0.035f, 14,
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
            DrawSphereWires(shield_hand, 0.108f, 6, 6,
                            runner->marker_color);
        }
        DrawSphereWires((Vector3){runner->agent.position.x,
                                  runner->agent.position.y + 2.05f,
                                  runner->agent.position.z},
                        0.075f, 6, 6, runner->marker_color);
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

static void DrawRoadHorseTeam(Vector3 base)
{
    const float yaw = 0.5f * PI;
    for (int32_t horse = -1; horse <= 1; horse += 2) {
        Color coat = horse < 0 ? (Color){89, 68, 56, 255} :
                                 (Color){112, 86, 63, 255};
        Color coat_shadow = ShadeColor(coat, 0.72f);
        Vector3 horse_base = LocalPoint(
            base, (float)horse * 0.66f, 0.0f, 4.55f, yaw);
        DrawOrientedBox(horse_base, (Vector3){0.0f, 0.92f, 0.0f},
                        (Vector3){0.62f, 0.78f, 1.34f}, yaw, coat);
        Vector3 neck_base = LocalPoint(horse_base, 0.0f, 1.06f, 0.35f, yaw);
        Vector3 neck = LocalPoint(horse_base, 0.0f, 1.36f, 0.54f, yaw);
        Vector3 head = LocalPoint(horse_base, 0.0f, 1.53f, 0.78f, yaw);
        DrawCylinderEx(neck_base, neck, 0.18f, 0.14f, 8, coat);
        DrawCharacterEllipsoid(head, (Vector3){0.20f, 0.23f, 0.30f}, coat);
        for (int32_t ear = -1; ear <= 1; ear += 2) {
            Vector3 ear_base = LocalPoint(
                horse_base, (float)ear * 0.095f, 1.70f, 0.70f, yaw);
            Vector3 ear_tip = LocalPoint(
                horse_base, (float)ear * 0.11f, 1.88f, 0.73f, yaw);
            DrawCylinderEx(ear_base, ear_tip, 0.036f, 0.012f, 6,
                           coat_shadow);
        }
        for (int32_t fore = -1; fore <= 1; fore += 2) {
            for (int32_t side = -1; side <= 1; side += 2) {
                Vector3 leg_top = LocalPoint(
                    horse_base, (float)side * 0.17f, 0.72f,
                    (float)fore * 0.39f, yaw);
                Vector3 knee = LocalPoint(
                    horse_base, (float)side * 0.18f, 0.39f,
                    (float)fore * 0.43f, yaw);
                Vector3 hoof = LocalPoint(
                    horse_base, (float)side * 0.18f, 0.08f,
                    (float)fore * 0.36f, yaw);
                DrawCylinderEx(leg_top, knee, 0.070f, 0.058f, 7, coat);
                DrawCylinderEx(knee, hoof, 0.058f, 0.045f, 7,
                               coat_shadow);
                DrawOrientedBox(hoof, (Vector3){0.0f, 0.02f, 0.035f},
                                (Vector3){0.12f, 0.08f, 0.19f}, yaw,
                                (Color){48, 42, 38, 255});
            }
        }
        Vector3 tail_root = LocalPoint(horse_base, 0.0f, 1.13f, -0.66f, yaw);
        Vector3 tail_end = LocalPoint(horse_base, 0.0f, 0.58f, -0.92f, yaw);
        DrawCylinderEx(tail_root, tail_end, 0.055f, 0.025f, 7,
                       coat_shadow);
        Vector3 trace_start = LocalPoint(
            base, (float)horse * 0.42f, 0.77f, 3.05f, yaw);
        Vector3 trace_end = LocalPoint(horse_base, 0.0f, 0.91f, -0.52f, yaw);
        DrawCylinderEx(trace_start, trace_end, 0.020f, 0.016f, 6,
                       (Color){57, 42, 34, 255});
        DrawOrientedBox(horse_base, (Vector3){0.0f, 1.02f, 0.18f},
                        (Vector3){0.66f, 0.055f, 0.055f}, yaw,
                        (Color){62, 45, 35, 255});
    }
}

static void DrawRoadCarriage(Vector3 base, int32_t cargo_used)
{
    const float yaw = 0.5f * PI;
    RuntimeAsset *carriage = &runtime_assets[RUNTIME_ASSET_CARRIAGE];
    if (carriage->ready) {
        DrawModelEx(carriage->model, base, (Vector3){0.0f, 1.0f, 0.0f},
                    CARRIAGE_ASSET_ROAD_YAW_DEGREES,
                    (Vector3){CARRIAGE_ASSET_SCALE,
                              CARRIAGE_ASSET_SCALE,
                              CARRIAGE_ASSET_SCALE}, WHITE);
        RuntimeAsset *rack = &runtime_assets[RUNTIME_ASSET_CARGO_RACK];
        if (cargo_used > 0 && rack->ready) {
            DrawModelEx(rack->model, base, (Vector3){0.0f, 1.0f, 0.0f},
                        CARRIAGE_ASSET_ROAD_YAW_DEGREES,
                        (Vector3){CARRIAGE_ASSET_SCALE,
                                  CARRIAGE_ASSET_SCALE,
                                  CARRIAGE_ASSET_SCALE}, WHITE);
        }
    } else {
        DrawOrientedBox(base, (Vector3){0.0f, 1.10f, 0.0f},
                        (Vector3){2.55f, 1.58f, 4.15f}, yaw,
                        (Color){125, 66, 50, 255});
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
            DrawScenerySphere(wheel, 0.53f, (Color){38, 31, 31, 255});
            DrawSphereWires(wheel, 0.55f, 7, 7, WORLD_GOLD);
        }
        Vector3 pole_left = LocalPoint(base, -0.48f, 0.82f, 2.10f, yaw);
        Vector3 pole_right = LocalPoint(base, 0.48f, 0.82f, 2.10f, yaw);
        Vector3 pole_left_end = LocalPoint(base, -0.48f, 0.72f, 4.10f, yaw);
        Vector3 pole_right_end = LocalPoint(base, 0.48f, 0.72f, 4.10f, yaw);
        DrawCylinderEx(pole_left, pole_left_end, 0.045f, 0.035f, 7,
                       (Color){91, 61, 43, 255});
        DrawCylinderEx(pole_right, pole_right_end, 0.045f, 0.035f, 7,
                       (Color){91, 61, 43, 255});
    }
    DrawRoadHorseTeam(base);
}

static void DrawRoadBarricade(const CcRoute *route)
{
    const float x = ROAD_BARRICADE_X;
    Color timber = route != NULL && route->smuggler_route ?
                   (Color){67, 48, 42, 255} : (Color){103, 70, 47, 255};
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
                              Fade((Color){43, 35, 31, 255}, 0.72f));
    }
}

static void DrawRoadTerrain(const CcRoute *route, int32_t danger,
                            bool bridge_checkpoint)
{
    float decay = route != NULL ?
        1.0f - (float)route->condition / 100.0f : 0.5f;
    Color ground = route != NULL && route->smuggler_route ?
        (Color){28, 54, 43, 255} :
        BlendColor((Color){48, 72, 51, 255},
                   (Color){87, 70, 46, 255}, decay * 0.75f);
    Color road = BlendColor((Color){105, 96, 78, 255},
                            (Color){78, 62, 49, 255}, decay);
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
        DrawCylinder((Vector3){x, -0.025f, z}, 0.28f, 0.42f,
                     0.035f, 12, (Color){54, 44, 37, 255});
    }
    Color leaves = route != NULL && route->smuggler_route ?
        (Color){35, 87, 65, 255} : (Color){57, 109, 78, 255};
    for (int32_t tree = 0; tree < 13; ++tree) {
        float x = 20.0f + (float)tree * 4.75f;
        float z = (tree & 1) != 0 ? 46.0f + (float)(tree % 3) * 1.2f :
                                    33.8f - (float)(tree % 3) * 1.1f;
        DrawTree(x, z, (tree % 3) == 0 ? ShadeColor(leaves, 0.82f) : leaves);
    }
    if (route != NULL && (route->closed || route->condition < 42)) {
        DrawTerrainPatchAtTop(68.0f, 32.0f, 4.0f, 16.0f, -0.002f,
                              (Color){12, 24, 27, 255});
        for (int32_t plank = 0; plank < 5; ++plank) {
            DrawBox((Vector3){70.0f, 0.12f, 37.35f + (float)plank * 1.32f},
                    (Vector3){4.45f, 0.18f, 0.92f},
                    plank == 2 ? (Color){82, 58, 43, 255} :
                                 (Color){113, 78, 51, 255});
        }
    }
    if (route != NULL && route->security >= 65) {
        DrawBox((Vector3){30.20f, 1.05f, 35.70f},
                (Vector3){0.58f, 2.10f, 0.58f},
                (Color){103, 104, 96, 255});
        DrawBox((Vector3){30.20f, 1.68f, 35.38f},
                (Vector3){0.42f, 0.46f, 0.05f}, WORLD_GOLD);
    }
    if (danger >= 45) {
        for (int32_t marker = 0; marker < 3; ++marker) {
            DrawBox((Vector3){58.5f + (float)marker * 1.1f, 0.18f,
                              44.15f},
                    (Vector3){0.52f, 0.36f, 0.72f},
                    (Color){82, 58, 49, 255});
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

void CcLocalDrawRoad3D(const CcSim *sim, const CcLocalAgent *agent,
                       const CcLocalCourse *course, bool travelling,
                       bool parley,
                       float clock, RenderTexture2D target,
                       Rectangle destination)
{
    if (sim == NULL || agent == NULL || course == NULL) return;
    face_glyph_count = 0;
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
    float route_progress = (float)sim->carriage.progress_milli / 1000.0f;
    float carriage_x = travelling ? 24.0f + route_progress * 52.0f : 38.35f;
    Vector3 carriage_base = {
        carriage_x,
        travelling ? 0.025f + sinf(clock * 5.0f) * 0.018f : 0.0f,
        40.0f
    };
    Vector3 camera_focus = travelling ? carriage_base : agent->position;
    Camera3D camera = RoadCamera(camera_focus, travelling, clock, true,
                                 target.texture.height);
    BeginTextureMode(target);
    ClearBackground((Color){9, 20, 25, 255});
    BeginMode3D(camera);
    BeginWorldLighting(camera, (Color){9, 20, 25, 255});
    bool authored_checkpoint = !travelling &&
        runtime_assets[RUNTIME_ASSET_BRIDGE].ready;
    DrawRoadTerrain(route, sim->journey.danger, authored_checkpoint);
    if (!travelling && !DrawBridgeCheckpoint()) DrawRoadBarricade(route);
    if (!travelling) DrawAgentPath(agent, false);
    int32_t road_cargo = CcPlayerCargoUsed(&sim->player);
    DrawRoadCarriage(carriage_base, road_cargo);

    DrawNpcFigure3D((Vector3){carriage_x - 3.25f, 0.0f, 37.95f}, 0.90f, 1.35f,
                    UINT32_C(0x726f6101), CC_NPC_ROLE_TRAVELLER,
                    (Color){151, 103, 78, 255}, clock * 0.42f,
                    CC_TRAVERSAL_IDLE);
    DrawNpcFigure3D((Vector3){carriage_x - 4.00f, 0.0f, 39.40f}, 0.84f, 1.10f,
                    UINT32_C(0x726f6102), CC_NPC_ROLE_HEALER,
                    WORLD_TEAL, clock * 0.36f + 1.4f,
                    CC_TRAVERSAL_IDLE);
    DrawNpcFigure3D((Vector3){carriage_x - 3.35f, 0.0f, 41.55f}, 0.80f, 1.55f,
                    UINT32_C(0x726f6103), CC_NPC_ROLE_REFUGEE,
                    WORLD_VIOLET, clock * 0.31f + 2.1f,
                    CC_TRAVERSAL_IDLE);
    DrawBox((Vector3){carriage_x - 4.25f, 0.34f, 42.45f},
            (Vector3){0.72f, 0.68f, 0.72f},
            (Color){137, 91, 55, 255});
    DrawBox((Vector3){carriage_x - 3.33f, 0.25f, 42.42f},
            (Vector3){0.82f, 0.50f, 0.64f},
            (Color){112, 76, 53, 255});

    if (!travelling) DrawCourseRunners(course);
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
    DrawQueuedFaceGlyphs(camera, target.texture.width,
                         target.texture.height);
    EndWorldLighting();
    EndMode3D();
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
    if (!travelling) {
        labels[count++] = (WorldLabel){{agent->position.x,
                                        agent->position.y + 2.50f,
                                        agent->position.z}, "YOU", WORLD_TEAL};
    }
    if (!travelling && !parley) {
        labels[count++] = (WorldLabel){{ROAD_BARRICADE_X, 2.58f, 40.00f},
                                       blockade_label, WORLD_DANGER};
    }
    labels[count++] = (WorldLabel){{travelling ? carriage_x + 8.0f : 57.0f,
                                    1.18f, 40.0f},
                                   route_label, WORLD_GOLD};
    if (parley) {
        labels[count++] = (WorldLabel){
            {course->raiders[0].position.x,
             course->raiders[0].position.y + 2.18f,
             course->raiders[0].position.z},
            "TOLL COLLECTOR", WORLD_DANGER};
        labels[count++] = (WorldLabel){{CC_LOCAL_ROAD_PARLEY_X, 0.42f,
                                        CC_LOCAL_ROAD_PARLEY_Z},
                                       "F  OFFER PAYMENT", WORLD_TEAL};
    }
    DrawLabels(labels, count, camera, destination);
    if (!travelling && course->alarm_active) {
        DrawCombatBar(agent, camera, destination, WORLD_TEAL);
        for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
            DrawCombatBar(&course->runners[i].agent, camera,
                          destination, course->runners[i].marker_color);
        }
        for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
            DrawCombatBar(&course->raiders[i], camera,
                          destination, i == agent->combat.target_index ? WORLD_GOLD :
                          WORLD_DANGER);
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
        travelling ?
        TextFormat("CARRIAGE MOVING / %d%% COMPLETE / %d GAME MIN / REAL SEC",
                   sim->carriage.progress_milli / 10,
                   CC_TRAVEL_GAME_MINUTES_PER_SECOND) : parley ?
        "PARLEY / approach the collector and press F to exchange crowns for passage" :
        TextFormat("BREAK THE CORDON / YOU %d HP / RAIDERS %d%% RESOLVE",
                   (int32_t)lroundf(agent->combat.health),
                   course->raider_resolve > 0 ? course->raider_resolve : 0),
        destination, 18, 35, 10, WORLD_INK);
}

static void DrawJourneyAftermath3D(const CcSim *sim,
                                   const CcSettlement *place)
{
    if (sim == NULL || place == NULL ||
        sim->resolved_journey_outcome == CC_JOURNEY_OUTCOME_NONE ||
        sim->journey.destination_id != place->id) return;
    const float x = 47.35f;
    const float z = 31.05f;
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
}

void CcLocalDrawStreet3D(const CcSim *sim, const CcLocalAgent *agent,
                         const CcLocalCourse *course, float clock,
                         RenderTexture2D target, Rectangle destination)
{
    const CcSettlement *place = CcSimSettlement(sim, sim->player.location_id);
    if (place == NULL) return;
    face_glyph_count = 0;
    Camera3D camera = StreetCamera(agent->position, clock, true,
                                   target.texture.height);
    Vector3 scenery_focus = camera.target;
    Color kingdom = KingdomColor3D(sim, place->kingdom_id);
    BeginTextureMode(target);
    ClearBackground((Color){10, 24, 30, 255});
    BeginMode3D(camera);
    BeginWorldLighting(camera, (Color){10, 24, 30, 255});

    DrawExteriorTerrain(place, scenery_focus);
    for (int32_t i = 0; i < (int32_t)(sizeof(STREET_PLATFORMS) /
                                      sizeof(STREET_PLATFORMS[0])); ++i) {
        const NavPlatform *platform = &STREET_PLATFORMS[i];
        Rectangle footprint = {platform->x, platform->z,
                               platform->width, platform->depth};
        if (!SceneryFootprintVisible(footprint, scenery_focus)) continue;
        Color color = CoursePlatformColor(platform->style);
        DrawBox((Vector3){platform->x + platform->width * 0.5f,
                          platform->height * 0.5f,
                          platform->z + platform->depth * 0.5f},
                (Vector3){fmaxf(0.10f, platform->width - 0.10f),
                          platform->height,
                          fmaxf(0.10f, platform->depth - 0.10f)}, color);
        DrawBox((Vector3){platform->x + platform->width * 0.5f,
                          platform->height + 0.025f,
                          platform->z + platform->depth * 0.5f},
                (Vector3){fmaxf(0.10f, platform->width - 0.04f), 0.05f,
                          fmaxf(0.10f, platform->depth - 0.04f)},
                i == 0 ? (Color){124, 145, 131, 255} :
                         Fade(WORLD_GOLD, 0.70f));
    }
    if (SceneryFootprintVisible((Rectangle){8.8f, 0.7f, 6.2f, 10.2f},
                                 scenery_focus)) {
        DrawObstacleCourse();
    }
    DrawAgentPath(agent, false);
    DrawWorldBuildings(kingdom, scenery_focus);
    if (SceneryFootprintVisible(WORLD_BUILDINGS[2].footprint,
                                 scenery_focus)) {
        (void)DrawAuthoredMarket(place);
    }
    DrawCastle(kingdom, scenery_focus);
    if (SceneryFootprintVisible(CARRIAGE_FOOTPRINT, scenery_focus)) {
        DrawCarriage3D(place);
    }
    if (SceneryPointVisible(CC_LOCAL_NOTICE_X, CC_LOCAL_NOTICE_Z,
                            scenery_focus)) {
        DrawNotice3D(sim);
    }
    DrawWorldTrees(scenery_focus);
    DrawRoomLandmarks(place, kingdom, scenery_focus);
    if (SceneryPointVisible(47.35f, 31.05f, scenery_focus)) {
        DrawJourneyAftermath3D(sim, place);
    }

    int32_t crates = place->stock[CC_GOOD_FOOD] / 12;
    if (crates > 4) crates = 4;
    for (int32_t i = 0; i < crates; ++i) {
        DrawBox((Vector3){44.40f + (float)(i % 2) * 0.72f, 0.30f,
                          26.75f + (float)(i / 2) * 0.72f},
                (Vector3){0.62f, 0.60f, 0.62f},
                (Color){177, 116, 55, 255});
    }
    const CcDungeon *dungeon = DungeonAt(sim, place->id);
    if (dungeon != NULL &&
        SceneryFootprintVisible(DUNGEON_FOOTPRINT, scenery_focus)) {
        DrawDungeon3D(dungeon);
    }

    DrawNpcFigure3D(
        (Vector3){STREET_PEOPLE[0].x, 0.0f, STREET_PEOPLE[0].y},
        0.96f, -0.55f, UINT32_C(0x73747201), CC_NPC_ROLE_MERCHANT,
        (Color){223, 151, 68, 255}, clock * 1.2f, CC_TRAVERSAL_IDLE);
    DrawNpcFigure3D(
        (Vector3){STREET_PEOPLE[1].x, 0.0f, STREET_PEOPLE[1].y},
        1.02f, 1.70f, UINT32_C(0x73747202), CC_NPC_ROLE_GUARD,
        kingdom, clock + 1.0f, CC_TRAVERSAL_IDLE);
    DrawNpcFigure3D(
        (Vector3){STREET_PEOPLE[2].x, 0.0f, STREET_PEOPLE[2].y},
        0.92f, 0.35f, UINT32_C(0x73747203), CC_NPC_ROLE_LABORER,
        (Color){97, 154, 137, 255}, clock + 2.0f, CC_TRAVERSAL_IDLE);
    DrawNpcFigure3D(
        (Vector3){STREET_PEOPLE[3].x, 0.0f, STREET_PEOPLE[3].y},
        0.88f, 2.40f, UINT32_C(0x73747204), CC_NPC_ROLE_HEALER,
        (Color){168, 112, 128, 255}, clock * 0.8f + 3.0f,
        CC_TRAVERSAL_IDLE);
    bool hungry_crowd = place->hunger >= 30;
    DrawNpcFigure3D(
        (Vector3){STREET_PEOPLE[4].x, 0.0f, STREET_PEOPLE[4].y},
        0.82f, -0.40f, UINT32_C(0x73747205),
        hungry_crowd ? CC_NPC_ROLE_REFUGEE : CC_NPC_ROLE_TRAVELLER,
        hungry_crowd ? WORLD_DANGER : kingdom, clock * 0.6f,
        CC_TRAVERSAL_IDLE);
    bool underworld_present = HasSmugglerRoad(sim, place->id) ||
                              place->security < 50;
    DrawNpcFigure3D(
        (Vector3){STREET_PEOPLE[5].x, 0.0f, STREET_PEOPLE[5].y},
        0.88f, 2.75f, UINT32_C(0x73747206),
        underworld_present ? CC_NPC_ROLE_SCOUT : CC_NPC_ROLE_TRAVELLER,
        underworld_present ? WORLD_VIOLET : kingdom, clock * 0.7f,
        CC_TRAVERSAL_IDLE);
    if (sim->resolved_journey_outcome != CC_JOURNEY_OUTCOME_NONE &&
        sim->journey.destination_id == place->id) {
        DrawNpcFigure3D((Vector3){46.80f, 0.0f, 31.15f}, 0.86f, -1.10f,
                        UINT32_C(0x61667401), CC_NPC_ROLE_GUARD,
                        WORLD_TEAL, clock * 0.64f + 1.4f,
                        CC_TRAVERSAL_IDLE);
        if (sim->resolved_journey_outcome == CC_JOURNEY_OUTCOME_COMBAT) {
            DrawNpcFigure3D((Vector3){47.65f, 0.0f, 30.55f}, 0.82f,
                            -0.85f, UINT32_C(0x61667402),
                            CC_NPC_ROLE_HEALER, WORLD_GOLD,
                            clock * 0.58f + 2.1f, CC_TRAVERSAL_IDLE);
        }
    }
    DrawCourseRunners(course);
    if (course != NULL && agent->combat.target_index >= 0 &&
        agent->combat.target_index < CC_LOCAL_RAIDER_COUNT) {
        DrawSelectedTarget(
            &course->raiders[agent->combat.target_index]);
    }
    DrawRobotShell(agent);
    DrawCombatSword(agent);
    DrawCombatSkillTell(agent);
    DrawCombatImpact(agent);
    DrawQueuedFaceGlyphs(camera, target.texture.width,
                         target.texture.height);
    EndWorldLighting();
    EndMode3D();
    EndTextureMode();
    PresentTarget(target, destination);

    int32_t room_index = street_camera_rig.shot;
    int32_t room_count = (int32_t)(sizeof(STREET_CAMERA_SHOTS) /
                                   sizeof(STREET_CAMERA_SHOTS[0]));
    if (room_index >= 0 && room_index < room_count) {
        const char *room_name = STREET_CAMERA_SHOTS[room_index].name;
        int32_t title_width = CcOverlayMeasureText(room_name, 10);
        DrawRectangleRounded(
            ViewportRectangle(destination, 11.0f, 10.0f,
                              (float)title_width + 16.0f, 18.0f),
            0.24f, 4, (Color){4, 10, 14, 202});
        DrawViewportText(room_name, destination, 19, 14, 10, WORLD_GOLD);
    }
    DrawStreetTraversalPortals(agent, camera, destination,
                               target.texture.width, target.texture.height);

    WorldLabel labels[20];
    int32_t count = 0;
    labels[count++] = (WorldLabel){{agent->position.x,
                                    agent->position.y + 2.50f,
                                    agent->position.z}, "YOU", WORLD_TEAL};
    labels[count++] = (WorldLabel){{50.0f, 4.75f, 21.0f},
                                   "MARKET HALL", WORLD_GOLD};
    labels[count++] = (WorldLabel){{36.80f, 2.28f, 31.70f},
                                   "CROWNLESS CARRIAGE", WORLD_GOLD};
    labels[count++] = (WorldLabel){{CC_LOCAL_NOTICE_X, 1.82f,
                                    CC_LOCAL_NOTICE_Z},
                                   "SITUATIONS", WORLD_INK};
    labels[count++] = (WorldLabel){{11.80f, 2.05f, 0.82f},
                                   "WAYFARER TRIALS", WORLD_GOLD};
    labels[count++] = (WorldLabel){{11.28f, 1.12f, 9.72f},
                                   "BUOYANCY TRENCH", WORLD_TEAL};
    labels[count++] = (WorldLabel){{78.50f, 12.10f, 17.50f},
                                   "GREYWARD KEEP", kingdom};
    if (sim->resolved_journey_outcome != CC_JOURNEY_OUTCOME_NONE &&
        sim->journey.destination_id == place->id) {
        labels[count++] = (WorldLabel){
            {47.35f, 2.05f, 31.05f},
            sim->resolved_journey_outcome == CC_JOURNEY_OUTCOME_COMBAT ?
                "ROAD GUARD MUSTER" : "COLLECTORS' TOLL MARK",
            sim->resolved_journey_outcome == CC_JOURNEY_OUTCOME_COMBAT ?
                WORLD_TEAL : WORLD_VIOLET};
    }
    if (course != NULL && course->situation_witness_active) {
        const CcSituation *situation = CcSimSituation(
            sim, course->situation_witness_id);
        if (situation != NULL && situation->affected_name[0] != '\0') {
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
    if (dungeon != NULL) {
        labels[count++] = (WorldLabel){{CC_LOCAL_DUNGEON_X, 3.38f,
                                        CC_LOCAL_DUNGEON_Z - 0.70f},
                                       CcDungeonStateName(dungeon->state), WORLD_VIOLET};
    }
    DrawLabels(labels, count, camera, destination);
    if (course != NULL && course->alarm_active) {
        DrawCombatBar(agent, camera, destination, WORLD_TEAL);
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
    if (course != NULL && course->alarm_active) {
        bool line_engaged = false;
        for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
            line_engaged = line_engaged ||
                           course->runners[i].duty == CC_GUARD_ENGAGED;
        }
        DrawViewportText(
            TextFormat("VILLAGE ALARM / YOU %d HP / %d POSTURE / GUARDS %s / RAIDERS %d%%",
                       (int32_t)lroundf(agent->combat.health),
                       (int32_t)lroundf(agent->combat.posture),
                       course->raiders_retreating ? "DRIVING THEM OUT" :
                       line_engaged ? "ENGAGED" : "FORMING LINE",
                       course->raider_resolve > 0 ?
                       course->raider_resolve : 0),
            destination, 18, 33, 10, WORLD_DANGER);
        if (course->combat_event_seconds > 0.0f) {
            DrawViewportText(
                TextFormat("%s / %s",
                           CcLocalCombatTeamName(course->last_attacker_team),
                           CcLocalCombatOutcomeName(course->last_outcome)),
                destination, 18, 48, 12,
                course->last_outcome == CC_COMBAT_OUTCOME_BLOCKED ?
                WORLD_GOLD : WORLD_INK);
        }
    }
}

void CcLocalDrawMarket3D(const CcSim *sim, const CcLocalAgent *agent, float clock,
                         RenderTexture2D target, Rectangle destination)
{
    const CcSettlement *place = CcSimSettlement(sim, sim->player.location_id);
    if (place == NULL) return;
    face_glyph_count = 0;
    Camera3D camera = LocalCamera(true, agent->position);
    BeginTextureMode(target);
    ClearBackground((Color){31, 23, 25, 255});
    BeginMode3D(camera);
    BeginWorldLighting(camera, (Color){31, 23, 25, 255});
    /* Six broad floor flags replace the old 63-box checkerboard. Their quiet
       value rhythm leaves the actor silhouettes and goods as the room detail. */
    const Color floor_dark = {88, 67, 57, 255};
    const Color floor_light = {108, 82, 64, 255};
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
            (Color){80, 53, 48, 255});
    DrawBox((Vector3){0.25f, 1.30f, 3.50f}, (Vector3){0.50f, 2.60f, 7.0f},
            (Color){67, 48, 47, 255});
    DrawBox((Vector3){4.50f, 0.54f, 0.515f},
            (Vector3){8.86f, 0.10f, 0.045f},
            (Color){48, 37, 39, 255});
    DrawBox((Vector3){6.55f, 1.42f, 0.525f},
            (Vector3){2.05f, 1.56f, 0.055f},
            (Color){47, 70, 69, 255});
    DrawBox((Vector3){6.55f, 2.21f, 0.535f},
            (Vector3){2.24f, 0.10f, 0.065f}, WORLD_GOLD);
    DrawBox((Vector3){MARKET_COUNTER_FOOTPRINT.x +
                      MARKET_COUNTER_FOOTPRINT.width * 0.5f,
                      0.46f,
                      MARKET_COUNTER_FOOTPRINT.y +
                      MARKET_COUNTER_FOOTPRINT.height * 0.5f},
            (Vector3){MARKET_COUNTER_FOOTPRINT.width, 0.92f,
                      MARKET_COUNTER_FOOTPRINT.height},
            (Color){139, 85, 49, 255});
    DrawBox((Vector3){MARKET_COUNTER_FOOTPRINT.x +
                      MARKET_COUNTER_FOOTPRINT.width * 0.5f,
                      0.94f,
                      MARKET_COUNTER_FOOTPRINT.y +
                      MARKET_COUNTER_FOOTPRINT.height * 0.5f},
            (Vector3){MARKET_COUNTER_FOOTPRINT.width + 0.12f, 0.10f,
                      MARKET_COUNTER_FOOTPRINT.height + 0.12f},
            (Color){181, 119, 62, 255});
    DrawBox((Vector3){MARKET_SHELF_FOOTPRINT.x + MARKET_SHELF_FOOTPRINT.width * 0.5f,
                      0.95f,
                      MARKET_SHELF_FOOTPRINT.y + MARKET_SHELF_FOOTPRINT.height * 0.5f},
            (Vector3){MARKET_SHELF_FOOTPRINT.width, 1.90f,
                      MARKET_SHELF_FOOTPRINT.height},
            (Color){103, 68, 49, 255});
    DrawBox((Vector3){MARKET_SHELF_FOOTPRINT.x +
                      MARKET_SHELF_FOOTPRINT.width * 0.5f,
                      1.92f,
                      MARKET_SHELF_FOOTPRINT.y +
                      MARKET_SHELF_FOOTPRINT.height * 0.5f},
            (Vector3){MARKET_SHELF_FOOTPRINT.width + 0.10f, 0.09f,
                      MARKET_SHELF_FOOTPRINT.height + 0.10f},
            (Color){62, 43, 40, 255});
    DrawBox((Vector3){1.55f, 1.05f, 6.54f}, (Vector3){0.82f, 2.10f, 0.08f},
            (Color){37, 28, 30, 255});
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        float stock = fminf((float)place->stock[good] / 54.0f, 1.0f);
        if (stock <= 0.01f) continue;
        Color color = good == CC_GOOD_FOOD ? WORLD_GOLD :
                      good == CC_GOOD_MATERIAL ?
                        (Color){170, 139, 112, 255} : WORLD_TEAL;
        float height = 0.22f + stock * 0.52f;
        DrawBox((Vector3){2.55f + (float)good * 1.08f, height * 0.5f,
                          1.18f},
                (Vector3){0.72f, height, 0.72f}, color);
        DrawBox((Vector3){2.55f + (float)good * 1.08f, height + 0.035f,
                          1.18f},
                (Vector3){0.78f, 0.07f, 0.78f}, ShadeColor(color, 0.72f));
    }
    DrawNpcFigure3D(
        (Vector3){MARKET_PEOPLE[0].x, 0.0f, MARKET_PEOPLE[0].y},
        1.02f, 2.75f, UINT32_C(0x4d415241), CC_NPC_ROLE_MERCHANT,
        (Color){218, 148, 61, 255}, clock, CC_TRAVERSAL_IDLE);
    DrawRobotShell(agent);
    DrawCombatSword(agent);
    DrawCombatSkillTell(agent);
    DrawQueuedFaceGlyphs(camera, target.texture.width,
                         target.texture.height);
    EndWorldLighting();
    EndMode3D();
    EndTextureMode();
    PresentTarget(target, destination);
    WorldLabel labels[] = {
        {{6.55f, 2.05f, 1.60f}, "MARA / FACTOR", WORLD_GOLD},
        {{1.55f, 2.25f, 6.54f}, "STREET", WORLD_MUTED}
    };
    DrawLabels(labels, 2, camera, destination);
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
