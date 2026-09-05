#include "client/cc_local_viewport.h"

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

    offset = Vector3Scale(offset, 2.5f);
    camera.position = Vector3Add(target, offset);
    camera.up = (Vector3){0.0f, 1.0f, 0.0f};
    camera.fovy = 2.0f * atanf(fovy * 0.5f /
        fmaxf(0.25f, Vector3Length(offset))) * RAD2DEG;
    camera.projection = CAMERA_PERSPECTIVE;
    return camera;
}

static float PerspectiveFovyForOrthographic(Camera3D camera)
{
    if (camera.projection == CAMERA_PERSPECTIVE) return camera.fovy;
    float distance = fmaxf(
        0.25f, Vector3Distance(camera.position, camera.target));
    float fovy = 2.0f * atanf(camera.fovy * 0.5f / distance) * RAD2DEG;
    return fovy;
}

static Camera3D PerspectiveCameraComposed(Vector3 target, Vector3 offset,
                                          float fovy)
{
    Camera3D camera = ExteriorCameraComposed(target, offset, fovy);
    camera.position = Vector3Add(target, offset);
    camera.fovy = fovy;
    camera.projection = CAMERA_PERSPECTIVE;
    return camera;
}

static Camera3D KeepHeroInsideStreetFrame(Camera3D camera, Vector3 hero,
                                          int32_t art_height,
                                          Rectangle safe_area)
{
    if (art_height <= 0) {
        return camera;
    }
    int32_t art_width = (int32_t)lroundf(
        (float)art_height * ((float)CC_LOCAL_ART_WIDTH / (float)CC_LOCAL_ART_HEIGHT));

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

    float subject_depth = Vector3DotProduct(
        Vector3Subtract(hero, camera.position), forward);
    float focus_depth = Vector3Distance(camera.position, camera.target);
    if (camera.projection == CAMERA_PERSPECTIVE && subject_depth < focus_depth * 0.8f) {
        Vector3 retreat = Vector3Scale(forward, subject_depth - focus_depth * 0.8f);
        camera.position = Vector3Add(camera.position, retreat);
        camera.target = Vector3Add(camera.target, retreat);
    }
    Vector2 screen = GetWorldToScreenEx(hero, camera, art_width, art_height);
    float clamped_x = fmaxf(safe_left, fminf(screen.x, safe_right));
    float clamped_y = fmaxf(safe_top, fminf(screen.y, safe_bottom));
    float view_height = camera.projection == CAMERA_PERSPECTIVE ?
        2.0f * fmaxf(0.25f, Vector3DotProduct(
            Vector3Subtract(hero, camera.position), forward)) *
            tanf(camera.fovy * DEG2RAD * 0.5f) : camera.fovy;
    float pixel_world = view_height / (float)art_height;
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

static bool CameraBoundsVisible(Camera3D camera, Vector3 center, Vector3 half,
                                 float aspect, float far_distance)
{
    Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, camera.up));
    Vector3 up = Vector3CrossProduct(right, forward);
    Vector3 relative = Vector3Subtract(center, camera.position);
    bool perspective = camera.projection == CAMERA_PERSPECTIVE;
    float slope = perspective ? tanf(camera.fovy * DEG2RAD * 0.5f) : 0.0f;
    Vector3 planes[] = {
        forward, Vector3Negate(forward),
        Vector3Add(Vector3Scale(forward, slope * aspect), right),
        Vector3Subtract(Vector3Scale(forward, slope * aspect), right),
        Vector3Add(Vector3Scale(forward, slope), up),
        Vector3Subtract(Vector3Scale(forward, slope), up),
    };
    for (int i = 0; i < 6; ++i) {
        Vector3 normal = planes[i];
        float extent = fabsf(normal.x) * half.x + fabsf(normal.y) * half.y +
                       fabsf(normal.z) * half.z;
        float margin = i == 1 ? far_distance : 0.0f;
        if (!perspective && i >= 2) margin = camera.fovy * 0.5f * (i < 4 ? aspect : 1.0f);
        if (Vector3DotProduct(relative, normal) + extent + margin < 0.0f) return false;
    }
    return true;
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
        float horizontal_scale = vertical_scale / (((float)CC_LOCAL_ART_WIDTH / (float)CC_LOCAL_ART_HEIGHT));
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
        float horizontal_scale = vertical_scale / (((float)CC_LOCAL_ART_WIDTH / (float)CC_LOCAL_ART_HEIGHT));
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
         building < ActiveWorldBuildingCount(); ++building) {
        WorldBuilding house = ActiveWorldBuildingAt(building);
        Vector3 half_size = {house.footprint.width * 0.5f,
                             house.height * 0.5f,
                             house.footprint.height * 0.5f};
        Vector3 center = {
            house.footprint.x + half_size.x,
            TerrainFootprintHeight(house.footprint) + half_size.y,
            house.footprint.y + half_size.z,
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
         building < ActiveWorldBuildingCount(); ++building) {
        WorldBuilding world = ActiveWorldBuildingAt(building);
        score += CameraPositionRectangleClutter(
            position, world.footprint, 1.80f) * 2.0f;
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
         structure < ActiveCompoundStructureCount(); ++structure) {
        WorldStructure world = ActiveCompoundStructureAt(structure);
        score += CameraPositionRectangleClutter(
            position, world.footprint, 1.80f) * 2.0f;
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

    score += CameraPositionRectangleClutter(
        position, (Rectangle){8.44f, 10.40f, 6.12f, 0.32f}, 2.20f) * 4.0f;
    return score;
}

static float CombatCameraBuildingOcclusionScore(
    Camera3D camera, Vector3 first_subject, Vector3 second_subject,
    int32_t art_height)
{
    if (art_height <= 0) return 0.0f;
    int32_t art_width = (int32_t)lroundf(
        (float)art_height * ((float)CC_LOCAL_ART_WIDTH / (float)CC_LOCAL_ART_HEIGHT));
    float score = 0.0f;
    for (int32_t building = 0;
         building < ActiveWorldBuildingCount(); ++building) {
        WorldBuilding house = ActiveWorldBuildingAt(building);
        if (CcLocalBuildingObscuresHeroInternal(
                house.footprint, house.height, camera, first_subject,
                art_width, art_height)) {
            score += 1.0f;
        }
        if (CcLocalBuildingObscuresHeroInternal(
                house.footprint, house.height, camera, second_subject,
                art_width, art_height)) {
            score += 1.0f;
        }
    }
    for (int32_t structure = 0;
         structure < ActiveCompoundStructureCount(); ++structure) {
        WorldStructure item = ActiveCompoundStructureAt(structure);
        if (CcLocalBuildingObscuresHeroInternal(
                item.footprint, item.height, camera, first_subject,
                art_width, art_height)) {
            score += 1.0f;
        }
        if (CcLocalBuildingObscuresHeroInternal(
                item.footprint, item.height, camera, second_subject,
                art_width, art_height)) {
            score += 1.0f;
        }
    }
    for (int32_t landmark = 0;
         landmark < CC_LOCAL_PLACE_LANDMARK_COUNT; ++landmark) {
        const CcLocalPlaceLandmark *item = ActivePlaceLandmarkAt(landmark);
        if (item == NULL) continue;
        Rectangle footprint = PlaceLandmarkFootprint(item);
        if (CcLocalBuildingObscuresHeroInternal(
                footprint, item->height, camera, first_subject,
                art_width, art_height)) {
            score += 1.0f;
        }
        if (CcLocalBuildingObscuresHeroInternal(
                footprint, item->height, camera, second_subject,
                art_width, art_height)) {
            score += 1.0f;
        }
    }
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
    return CameraVolumeOverlap(subjects[0], scenery) +
           CameraVolumeOverlap(subjects[1], scenery);
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
    return CameraVolumeOverlap(subjects[0], gate) +
           CameraVolumeOverlap(subjects[1], gate);
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

        float cost = tree_occlusion * 12.0f + course_clutter * 1.80f +
                     fabsf(angle) * 0.20f +
                     fabsf(WrapAngle(angle - preferred_angle)) * 0.50f;
        if (cost >= best_cost) continue;
        best = option;
        best_angle = angle;
        best_cost = cost;
    }
    if (chosen_angle != NULL) *chosen_angle = best_angle;
    return best;
}

static int32_t StreetNavigationRoomFor(Vector3 focus,
                                       int32_t current_room)
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
    if (current_room >= 0 && current_room < count &&
        current_room != nearest) {
        float current_x = focus.x -
                          STREET_CAMERA_SHOTS[current_room].trigger.x;
        float current_z = focus.z -
                          STREET_CAMERA_SHOTS[current_room].trigger.y;
        float current_distance = sqrtf(current_x * current_x +
                                       current_z * current_z);
        float candidate_distance = sqrtf(nearest_distance);
        if (candidate_distance + 1.25f >= current_distance) {
            return current_room;
        }
    }
    return nearest;
}

static int32_t TownCameraSceneFor(Vector3 focus, int32_t current_scene)
{
    const float close_activation_radius = 10.50f;
    int32_t nearest = 0;
    float nearest_distance = FLT_MAX;
    for (int32_t scene = 0;
         scene < CC_LOCAL_PLACE_SCENE_COUNT; ++scene) {
        const CcLocalTownScene *candidate = CcLocalTownSceneAt(
            active_place_function, scene);
        float x = focus.x - candidate->trigger_x;
        float z = focus.z - candidate->trigger_z;
        float distance = x * x + z * z;

        if (candidate->kind >= CC_LOCAL_TOWN_SCENE_CLOSE_FIRST &&
            distance > close_activation_radius * close_activation_radius) {
            continue;
        }
        if (distance >= nearest_distance) continue;
        nearest = scene;
        nearest_distance = distance;
    }
    if (current_scene >= 0 &&
        current_scene < CC_LOCAL_PLACE_SCENE_COUNT &&
        current_scene != nearest) {
        const CcLocalTownScene *current = CcLocalTownSceneAt(
            active_place_function, current_scene);
        float x = focus.x - current->trigger_x;
        float z = focus.z - current->trigger_z;
        float current_distance = sqrtf(x * x + z * z);
        bool current_is_eligible =
            current->kind < CC_LOCAL_TOWN_SCENE_CLOSE_FIRST ||
            current_distance <= close_activation_radius + 1.25f;
        if (current_is_eligible &&
            sqrtf(nearest_distance) + 2.25f >= current_distance) {
            return current_scene;
        }
    }
    return nearest;
}

static const StreetCameraShot *StreetCameraShotAt(int32_t shot)
{
    static StreetCameraShot composition;
    const CcLocalTownScene *scene = CcLocalTownSceneAt(
        active_place_function, shot);
    if (scene == NULL) {
        scene = CcLocalTownSceneAt(active_place_function,
                                   CC_LOCAL_TOWN_SCENE_HEART);
    }
    bool close_scene =
        scene->kind >= CC_LOCAL_TOWN_SCENE_CLOSE_FIRST;
    ArtLightProfileId light = scene->kind == CC_LOCAL_TOWN_SCENE_ARRIVAL ?
        ART_LIGHT_ROAD_DUSK : ART_LIGHT_CLEAR_MARKET;
    if (scene->kind == CC_LOCAL_TOWN_SCENE_LANDMARK) {
        light = active_place_function == CC_SETTLEMENT_MINING ||
                active_place_function == CC_SETTLEMENT_DUNGEON_TOWN ?
            ART_LIGHT_SHORTAGE_OVERCAST : ART_LIGHT_RECOVERY_WARM;
    }
    if (close_scene &&
        (active_place_function == CC_SETTLEMENT_MINING ||
         active_place_function == CC_SETTLEMENT_DUNGEON_TOWN)) {
        light = ART_LIGHT_SHORTAGE_OVERCAST;
    } else if (close_scene &&
               (active_place_function == CC_SETTLEMENT_FARMING ||
                active_place_function == CC_SETTLEMENT_CAPITAL)) {
        light = ART_LIGHT_RECOVERY_WARM;
    }
    composition = (StreetCameraShot){
        .trigger = {scene->trigger_x, scene->trigger_z},
        .target = {scene->target_x, scene->target_y, scene->target_z},
        .name = scene->name,
        .route_palette = (int32_t)scene->kind,

        .camera_offset = {scene->camera_offset_x, scene->camera_offset_y,
                          scene->camera_offset_z},
        .fovy = scene->fovy,
        .art = {
            {scene->target_x, scene->target_y, scene->target_z},
            {scene->kind == CC_LOCAL_TOWN_SCENE_ARRIVAL ? -1.0f :
             scene->camera_offset_x < 0.0f ? 1.0f : -1.0f,
             scene->kind == CC_LOCAL_TOWN_SCENE_LANDMARK ? -1.0f :
             close_scene ? 0.35f : 0.0f},
            {scene->target_x - scene->camera_offset_x *
                                   (close_scene ? 0.24f : 0.36f),
             0.0f,
             scene->target_z - scene->camera_offset_z *
                                   (close_scene ? 0.24f : 0.36f)},
            close_scene ? (Rectangle){0.13f, 0.14f, 0.74f, 0.72f} :
                          (Rectangle){0.16f, 0.16f, 0.66f, 0.68f},
            close_scene ? (Vector3){9.0f, 18.0f, 32.0f} :
                          (Vector3){20.0f, 34.0f, 56.0f},
            light,
        },
    };
    return &composition;
}

static float StreetCameraClearanceHeight(float x, float z)
{
    float floor_height = CcLocalTerrainHeightAt(x, z) + 0.5f;
    const Vector2 corners[] = {{-0.4f, -0.4f}, {0.4f, -0.4f},
                               {-0.4f, 0.4f}, {0.4f, 0.4f}};
    for (int i = 0; i < 4; ++i) {
        floor_height = fmaxf(floor_height,
            CcLocalTerrainHeightAt(x + corners[i].x, z + corners[i].y) + 0.5f);
    }
    for (int32_t i = 0; i < ActiveWorldBuildingCount(); ++i) {
        WorldBuilding building = ActiveWorldBuildingAt(i);
        float distance = TerrainDistanceToRectangle(x, z, building.footprint);
        float influence = 1.0f - SmoothStep01((distance - 0.4f) / 3.0f);
        float roof = TerrainFootprintHeight(building.footprint) + building.height + 3.0f;
        floor_height = fmaxf(floor_height,
            floor_height + fmaxf(0.0f, roof - floor_height) * influence);
    }
    for (int32_t i = 0; i < ActiveCompoundStructureCount(); ++i) {
        WorldStructure structure = ActiveCompoundStructureAt(i);
        float distance = TerrainDistanceToRectangle(x, z, structure.footprint);
        float influence = 1.0f - SmoothStep01((distance - 0.4f) / 3.0f);
        float roof = TerrainFootprintHeight(structure.footprint) + structure.height + 1.2f;
        floor_height = fmaxf(floor_height,
            floor_height + fmaxf(0.0f, roof - floor_height) * influence);
    }
    return floor_height;
}

static Camera3D StreetCameraClearVolume(Camera3D camera)
{
    camera.position.y = fmaxf(camera.position.y,
        StreetCameraClearanceHeight(camera.position.x, camera.position.z));
    return camera;
}

static Vector3 CameraHermite(Vector3 from, Vector3 to, Vector3 velocity,
                              float duration, float amount)
{
    float ease = SmoothStep01(amount);
    float tangent = amount * (1.0f - amount) * (1.0f - amount) * duration;
    return Vector3Add(Vector3Lerp(from, to, ease), Vector3Scale(velocity, tangent));
}

static void FixedCameraRigAim(FixedCameraRig *rig, int32_t shot,
                              Vector3 destination, Vector3 offset,
                              float fovy, float clock, bool advance)
{
    if (!rig->initialized) {
        rig->target_velocity = (Vector3){0};
        rig->offset_velocity = (Vector3){0};
        rig->framing_velocity = (Vector3){0};
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
        rig->target_velocity_from = Vector3Add(
            rig->target_velocity, rig->framing_velocity);
        rig->offset_velocity_from = rig->offset_velocity;
        rig->transition_from = Vector3Add(
            rig->displayed_target, rig->framing_offset);
        rig->displayed_target = rig->transition_from;
        rig->destination = destination;
        rig->offset_transition_from = rig->displayed_offset;
        rig->offset_destination = offset;
        rig->fovy_transition_from = rig->displayed_fovy;
        rig->fovy_destination = fovy;
        rig->transition_elapsed = 0.0f;
        rig->transition_duration = CombatClamp(
            Vector3Distance(rig->transition_from, destination) / 24.0f,
            0.65f, 1.6f);

        rig->framing_offset = (Vector3){0};
        rig->framing_from = (Vector3){0};
        rig->framing_destination = (Vector3){0};
        rig->framing_elapsed = 0.0f;
        rig->framing_duration = 0.0f;
        rig->framing_hold_seconds = 0.0f;
        rig->shot = shot;
        return;
    }
    if (rig->transition_elapsed >= rig->transition_duration ||
        rig->transition_duration <= 0.0f) {
        rig->target_velocity = (Vector3){0};
        rig->offset_velocity = (Vector3){0};
        rig->displayed_target = rig->destination;
        rig->displayed_offset = rig->offset_destination;
        rig->displayed_fovy = rig->fovy_destination;
        return;
    }
    rig->transition_elapsed = fminf(rig->transition_duration,
                                    rig->transition_elapsed + delta_time);

    float progress = rig->transition_elapsed / rig->transition_duration;
    Vector3 old_target = rig->displayed_target;
    Vector3 old_offset = rig->displayed_offset;
    rig->displayed_target = CameraHermite(
        rig->transition_from, rig->destination, rig->target_velocity_from,
        rig->transition_duration, progress);
    rig->displayed_offset = CameraHermite(
        rig->offset_transition_from, rig->offset_destination, rig->offset_velocity_from,
        rig->transition_duration, progress);
    rig->displayed_fovy = rig->fovy_transition_from +
        (rig->fovy_destination - rig->fovy_transition_from) * SmoothStep01(progress);
    if (delta_time > 0.0f) {
        rig->target_velocity = Vector3Scale(
            Vector3Subtract(rig->displayed_target, old_target), 1.0f / delta_time);
        rig->offset_velocity = Vector3Scale(
            Vector3Subtract(rig->displayed_offset, old_offset), 1.0f / delta_time);
    }

}

static Camera3D FixedCameraRigFrameHero(FixedCameraRig *rig,
                                         Camera3D camera, Vector3 hero,
                                         int32_t art_height,
                                         Rectangle quiet_area,
                                         bool advance)
{
    if (rig == NULL || art_height <= 0) return camera;
    int32_t art_width = (int32_t)lroundf(
        (float)art_height * ((float)CC_LOCAL_ART_WIDTH / (float)CC_LOCAL_ART_HEIGHT));

    if (advance && rig->framing_duration > 0.0f) {
        rig->framing_elapsed = fminf(
            rig->framing_duration,
            rig->framing_elapsed + rig->delta_time);
        float amount = SmoothStep01(rig->framing_elapsed /
                                    rig->framing_duration);
        Vector3 old_framing = rig->framing_offset;
        rig->framing_offset = Vector3Lerp(
            rig->framing_from, rig->framing_destination, amount);
        if (rig->delta_time > 0.0f) rig->framing_velocity = Vector3Scale(
            Vector3Subtract(rig->framing_offset, old_framing), 1.0f / rig->delta_time);
        if (rig->framing_elapsed >= rig->framing_duration) {
            rig->framing_duration = 0.0f;
            rig->framing_velocity = (Vector3){0};
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
    float view_height = camera.projection == CAMERA_PERSPECTIVE ?
        2.0f * fmaxf(0.25f, Vector3DotProduct(
            Vector3Subtract(hero, camera.position), forward)) *
            tanf(camera.fovy * DEG2RAD * 0.5f) : camera.fovy;
    float pixel_world = view_height / (float)art_height;
    Vector3 adjustment = Vector3Add(
        Vector3Scale(right, (screen.x - target_x) * pixel_world),
        Vector3Scale(screen_up, (target_y - screen.y) * pixel_world));
    rig->framing_from = rig->framing_offset;
    rig->framing_destination = Vector3Add(rig->framing_offset, adjustment);
    rig->framing_elapsed = 0.0f;

    rig->framing_duration = 0.40f;
    rig->framing_hold_seconds = 0.0f;
    return camera;
}

Camera3D CcLocalStreetCameraInternal(const CcLocalAgent *agent, float clock,
                                     bool advance, int32_t art_height)
{
    if (!advance && street_camera_rig.initialized && street_camera_rig.output_valid) {
        return street_camera_rig.output_camera;
    }
    bool fresh_view = !street_camera_rig.initialized;
    Vector3 focus = agent != NULL ? agent->position : (Vector3){0};
    int32_t shot = TownCameraSceneFor(focus, street_camera_rig.shot);
    const StreetCameraShot *composition = StreetCameraShotAt(shot);
    Vector3 destination = composition->target;
    destination.y += CcLocalTerrainHeightAt(destination.x, destination.z);
    Vector3 camera_offset = composition->camera_offset;

    float camera_x = destination.x + camera_offset.x;
    float camera_z = destination.z + camera_offset.z;
    float camera_ground = CcLocalTerrainHeightAt(camera_x, camera_z);
    float horizontal_span = sqrtf(camera_offset.x * camera_offset.x +
                                   camera_offset.z * camera_offset.z);
    /* Keep a view down onto the street when the camera stands on lower land. */
    camera_offset.y = fmaxf(
        camera_ground + composition->camera_offset.y - destination.y,
        horizontal_span * 0.55f);
    FixedCameraRigAim(&street_camera_rig, shot, destination,
                      camera_offset, composition->fovy,
                      clock, advance);
    Camera3D camera = ExteriorCameraComposed(
        street_camera_rig.displayed_target,
        street_camera_rig.displayed_offset,
        street_camera_rig.displayed_fovy);
    camera = StreetCameraClearVolume(camera);
    camera.fovy = 2.0f * atanf(street_camera_rig.displayed_fovy * 0.5f /
        Vector3Distance(camera.position, camera.target)) * RAD2DEG;
    if (agent != NULL) {
        Vector3 hero = {focus.x, focus.y + 1.05f, focus.z};
        camera = FixedCameraRigFrameHero(
            &street_camera_rig, camera, hero, art_height,
            composition->art.quiet_area, advance);
        Camera3D authored_camera = camera;
        Vector3 authored_offset = Vector3Subtract(camera.position, camera.target);
        float angle = street_camera_rig.tree_angle;
        camera.position = Vector3Add(camera.target, Vector3RotateByAxisAngle(
            authored_offset, (Vector3){0, 1, 0}, angle));
        if (advance) {
            street_camera_rig.tree_hold = fmaxf(0.0f,
                street_camera_rig.tree_hold - street_camera_rig.delta_time);
            if (fresh_view || (street_camera_rig.tree_hold <= 0.0f &&
                CcLocalCameraTreeOcclusionScoreInternal(camera, hero, hero) > 0.06f)) {
                float chosen = 0.0f;
                (void)CcLocalCameraClearSightlinesInternal(authored_camera, hero, hero,
                    street_camera_rig.tree_goal, &chosen);
                street_camera_rig.tree_goal = chosen;
                street_camera_rig.tree_hold = 0.50f;
                if (fresh_view) street_camera_rig.tree_angle = chosen;
            }
            street_camera_rig.tree_angle += WrapAngle(
                street_camera_rig.tree_goal - street_camera_rig.tree_angle) *
                (1.0f - expf(-street_camera_rig.delta_time * 3.5f));
            camera.position = Vector3Add(camera.target, Vector3RotateByAxisAngle(
                authored_offset, (Vector3){0, 1, 0}, street_camera_rig.tree_angle));
        }
        camera = StreetCameraClearVolume(camera);
        Vector3 before_guard = camera.target;
        camera = KeepHeroInsideStreetFrame(
            camera, hero, art_height,
            (Rectangle){0.10f, 0.12f, 0.80f, 0.76f});
        Vector3 guard = Vector3Subtract(camera.target, before_guard);
        street_camera_rig.framing_offset = Vector3Add(street_camera_rig.framing_offset, guard);
        street_camera_rig.framing_from = Vector3Add(street_camera_rig.framing_from, guard);
        street_camera_rig.framing_destination = Vector3Add(street_camera_rig.framing_destination, guard);
    }
    street_camera_rig.output_camera = camera;
    street_camera_rig.output_valid = true;
    return camera;
}

static Camera3D TownArrivalCamera(const CcLocalConvoyState *convoy,
                                  int32_t art_height)
{
    typedef struct TownTravelCamera {
        float target_height;
        Vector3 offset;
    } TownTravelCamera;
    TownTravelCamera travel = {
        .target_height = 2.0f,
        .offset = {-25.0f, 15.0f, 38.0f},
    };
    switch (active_place_function) {
        case CC_SETTLEMENT_FARMING:
            travel = (TownTravelCamera){
                1.8f, {-24.0f, 15.0f, 36.0f}};
            break;
        case CC_SETTLEMENT_MINING:
            travel = (TownTravelCamera){
                2.5f, {-25.0f, 13.0f, 36.0f}};
            break;
        case CC_SETTLEMENT_FORTRESS:
            travel = (TownTravelCamera){
                2.8f, {24.0f, 14.0f, 35.0f}};
            break;
        case CC_SETTLEMENT_CAPITAL:
            travel = (TownTravelCamera){
                3.4f, {-28.0f, 18.0f, 40.0f}};
            break;
        case CC_SETTLEMENT_DUNGEON_TOWN:
            travel = (TownTravelCamera){
                1.8f, {-23.0f, 13.0f, 36.0f}};
            break;
        case CC_SETTLEMENT_MARKET:
        default:
            break;
    }
    const CcLocalTownScene *arrival = CcLocalTownSceneAt(
        active_place_function, CC_LOCAL_TOWN_SCENE_ARRIVAL);
    if (convoy == NULL || arrival == NULL) {
        return ExteriorCameraAt((Vector3){81.0f, 2.0f, 34.0f}, 21.0f);
    }

    float heading = convoy->town_heading_yaw;
    Vector3 forward = {sinf(heading), 0.0f, cosf(heading)};
    Vector3 right = {cosf(heading), 0.0f, -sinf(heading)};
    Vector3 follow_target = Vector3Add(
        convoy->town_position,
        Vector3Add(Vector3Scale(forward, 5.2f),
                   (Vector3){0.0f, 1.55f, 0.0f}));
    Vector3 follow_position = Vector3Add(
        convoy->town_position,
        Vector3Add(Vector3Scale(forward, -10.5f),
                   Vector3Add(Vector3Scale(right, 3.8f),
                              (Vector3){0.0f, 5.2f, 0.0f})));
    Vector3 wide_target = {
        arrival->target_x,
        travel.target_height + CcLocalTerrainHeightAt(
            arrival->target_x, arrival->target_z),
        arrival->target_z,
    };
    Vector3 wide_position = Vector3Add(wide_target, travel.offset);
    float pull_out = SmoothStep01(
        (convoy->phase_progress - 0.08f) / 0.78f);
    Camera3D camera = {
        .position = Vector3Lerp(follow_position, wide_position, pull_out),
        .target = Vector3Lerp(follow_target, wide_target, pull_out),
        .up = {0.0f, 1.0f, 0.0f},
        .fovy = 43.0f + (32.0f - 43.0f) * pull_out,
        .projection = CAMERA_PERSPECTIVE,
    };

    Vector3 arrival_offset = Vector3Subtract(camera.position, camera.target);
    float arrival_span = 2.0f * Vector3Length(arrival_offset) *
        tanf(camera.fovy * DEG2RAD * 0.5f);
    street_camera_rig = (FixedCameraRig){
        .displayed_target = camera.target,
        .displayed_offset = Vector3Scale(arrival_offset, 1.0f / 2.5f),
        .displayed_fovy = arrival_span,
        .shot = -1,
        .initialized = true,
    };
    return SnapCameraToArtPixels(camera, art_height);
}

static Camera3D RoadCamera(Vector3 focus, bool travelling, float clock,
                           bool advance, int32_t art_height)
{
    if (travelling) {
        float sway = sinf(clock * 0.85f) * 0.24f;
        Vector3 look_ahead = {
            focus.x + 12.0f,
            focus.y + 1.20f,
            focus.z + sway * 0.35f
        };
        Camera3D camera = PerspectiveCameraComposed(
            look_ahead, (Vector3){-11.5f, 4.8f, 3.8f + sway}, 43.0f);
        return SnapCameraToArtPixels(camera, art_height);
    }
    int32_t shot = 4;
    Vector3 destination = {48.60f, 0.95f, 40.0f};
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
                                         const CcLocalCourse *course,
                                         int32_t art_height)
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


    float look_ahead = fminf(span * 0.72f, 3.30f);
    Vector3 target = Vector3Add(
        player->position, Vector3Scale(fight_line, look_ahead));
    float duel_ground_y = (player->position.y + opponent_position.y) * 0.50f;

    target.y = duel_ground_y + 1.32f;
    bool road_duel = course->scene == CC_LOCAL_SCENE_ROAD;
    float follow_distance = road_duel ?
        4.45f + fminf(span, 7.0f) * 0.08f :
        5.40f + fminf(span, 7.0f) * 0.12f;
    float fovy = road_duel ?
        CombatClamp(40.0f + span * 0.50f, 42.0f, 46.0f) :
        CombatClamp(44.0f + span * 0.75f, 45.0f, 50.0f);

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

    static const float sightline_angles[] = {
        0.0f,
        12.0f * DEG2RAD, -12.0f * DEG2RAD,
        24.0f * DEG2RAD, -24.0f * DEG2RAD,
        36.0f * DEG2RAD, -36.0f * DEG2RAD,
    };
    float chosen_angle = 0.0f;

    for (int32_t shoulder = 0; shoulder < 2; ++shoulder) {
        Vector3 candidate_side = shoulder == 0 ? fight_side :
                                                Vector3Negate(fight_side);
        Vector3 base_camera_position = Vector3Add(
            player->position,
            Vector3Add(Vector3Scale(fight_line, -follow_distance),
                       Vector3Scale(candidate_side, shoulder_distance)));
        base_camera_position.y = duel_ground_y +
                                 (road_duel ? 2.85f : 3.15f);
        Vector3 base_candidate_offset = Vector3Subtract(
            base_camera_position, target);
        for (int32_t sightline = 0;
             sightline < (int32_t)(sizeof(sightline_angles) /
                                   sizeof(sightline_angles[0]));
             ++sightline) {
            float angle = road_duel ? 0.0f :
                          sightline_angles[sightline];
            if (road_duel && sightline > 0) break;
            float cosine = cosf(angle);
            float sine = sinf(angle);
            Vector3 candidate_offset = {
                base_candidate_offset.x * cosine +
                    base_candidate_offset.z * sine,
                base_candidate_offset.y,
                -base_candidate_offset.x * sine +
                    base_candidate_offset.z * cosine,
            };
            Vector3 camera_position = Vector3Add(target, candidate_offset);
            Camera3D candidate = PerspectiveCameraComposed(
                target, candidate_offset, fovy);
            float cost = shoulder == 0 ? 0.0f : 0.12f;
            cost += fabsf(angle) * 0.38f;
            Vector3 player_to_camera = Vector3Subtract(
                camera_position, player->position);
            float visible_shoulder = fabsf(Vector3DotProduct(
                player_to_camera, fight_side));
            if (visible_shoulder < 2.80f) cost += 1000.0f;

            if (art_height > 0) {
                int32_t art_width = (int32_t)lroundf(
                    (float)art_height * ((float)CC_LOCAL_ART_WIDTH / (float)CC_LOCAL_ART_HEIGHT));
                Vector2 player_head = GetWorldToScreenEx(
                    Vector3Add(player->position,
                               (Vector3){0.0f, 1.80f, 0.0f}),
                    candidate, art_width, art_height);
                Vector2 player_foot = GetWorldToScreenEx(
                    player->position, candidate, art_width, art_height);
                Vector2 opponent_head = GetWorldToScreenEx(
                    Vector3Add(opponent_position,
                               (Vector3){0.0f, 1.80f, 0.0f}),
                    candidate, art_width, art_height);
                Vector2 opponent_foot = GetWorldToScreenEx(
                    opponent_position, candidate, art_width, art_height);
                float player_height = fabsf(player_foot.y - player_head.y);
                float opponent_height = fabsf(
                    opponent_foot.y - opponent_head.y);
                if (player_height < opponent_height * 1.10f) {
                    cost += 1000.0f;
                }
            }
            if (course->scene == CC_LOCAL_SCENE_STREET) {
                cost +=
                    CombatCameraBuildingOcclusionScore(
                        candidate, player_center, opponent_center,
                        art_height) * 42.0f +
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
                Vector3DotProduct(
                    candidate_side,
                    combat_camera_rig.locked_shoulder_side) < 0.0f) {
                cost += 0.90f;
            }
            if (cost >= best_cost) continue;
            best_cost = cost;
            offset = candidate_offset;
            chosen_side = candidate_side;
            chosen_angle = angle;
        }
    }
    combat_camera_rig.tree_clear_angle = chosen_angle;
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
        (float)art_height * ((float)CC_LOCAL_ART_WIDTH / (float)CC_LOCAL_ART_HEIGHT));
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


Camera3D CcLocalCombatCameraInternal(Camera3D base,
                                     const CcLocalAgent *player,
                                     const CcLocalCourse *course,
                                     float clock, bool advance,
                                     int32_t art_height)
{
    const CcLocalAgent *opponent = CombatCameraOpponent(course, player);
    float opponent_distance_squared = opponent != NULL && player != NULL ?
        CombatHorizontalDistanceSquared(player, opponent) : FLT_MAX;

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

    if (!active && !combat_camera_rig.composition_locked) {
        combat_camera_rig.displayed_target = base.target;
        combat_camera_rig.displayed_offset = base_offset;
        combat_camera_rig.displayed_fovy = base_perspective_fovy;
    }
    if (advance) {
        float delta_time = clock - combat_camera_rig.last_clock;
        if (delta_time < 0.0f || delta_time > 0.12f) delta_time = 0.0f;
        delta_time = fminf(delta_time, 0.050f);
        combat_camera_rig.last_clock = clock;

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
                CombatCameraLockComposition(base, player, opponent, course,
                                            art_height);
            }
        }

        float weight = SmoothStep01(combat_camera_rig.combat_weight);
        Vector3 desired_target = Vector3Lerp(
            base.target, combat_camera_rig.locked_target, weight);
        Vector3 desired_offset = Vector3Lerp(
            base_offset, combat_camera_rig.locked_offset, weight);
        float base_span = 2.0f * Vector3Length(base_offset) *
            tanf(base_perspective_fovy * DEG2RAD * 0.5f);
        float close_span = 2.0f * Vector3Length(combat_camera_rig.locked_offset) *
            tanf(combat_camera_rig.locked_fovy * DEG2RAD * 0.5f);
        float desired_span = base_span + (close_span - base_span) * weight;
        float displayed_span = 2.0f * Vector3Length(combat_camera_rig.displayed_offset) *
            tanf(combat_camera_rig.displayed_fovy * DEG2RAD * 0.5f);
        float response = quick_road_entry ? 8.0f : 4.5f;
        float ease = 1.0f - expf(-delta_time * response);
        combat_camera_rig.displayed_target = Vector3Lerp(
            combat_camera_rig.displayed_target, desired_target, ease);
        combat_camera_rig.displayed_offset = Vector3Lerp(
            combat_camera_rig.displayed_offset, desired_offset, ease);
        displayed_span += (desired_span - displayed_span) * ease;
        combat_camera_rig.displayed_fovy = 2.0f * atanf(displayed_span * 0.5f /
            fmaxf(0.25f, Vector3Length(combat_camera_rig.displayed_offset))) * RAD2DEG;
        if (!active && combat_camera_rig.combat_weight <= 0.0001f &&
            Vector3Distance(combat_camera_rig.displayed_target, base.target) < 0.0001f &&
            Vector3Distance(combat_camera_rig.displayed_offset, base_offset) < 0.0001f &&
            fabsf(combat_camera_rig.displayed_fovy - base_perspective_fovy) < 0.0001f) {
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
    camera.fovy = combat_camera_rig.displayed_fovy;
    camera.projection = CAMERA_PERSPECTIVE;
    if (active && CcClientHitEffectVisible(
            reduced_motion_enabled, player->combat.hit_flash_seconds)) {
        float pulse = player->combat.hit_flash_seconds * 8.0f;
        float shake = fminf(0.055f, pulse * 0.055f);
        camera.target.x += sinf(clock * 71.0f) * shake;
        camera.target.y += cosf(clock * 59.0f) * shake * 0.45f;
        camera.position.x += sinf(clock * 71.0f) * shake;
        camera.position.y += cosf(clock * 59.0f) * shake * 0.45f;
    }
    return SnapCameraToArtPixels(camera, art_height);
}

Camera3D CcLocalConversationCameraInternal(
    Camera3D base, const CcLocalAgent *player, const CcLocalAgent *partner,
    bool active, float clock, bool advance, int32_t art_height)
{
    bool available = active && player != NULL && partner != NULL;
    Vector3 base_offset = Vector3Subtract(base.position, base.target);
    float base_fovy = PerspectiveFovyForOrthographic(base);
    if (!conversation_camera_rig.initialized) {
        conversation_camera_rig.close_target = base.target;
        conversation_camera_rig.close_offset = base_offset;
        conversation_camera_rig.close_fovy = base_fovy;
        conversation_camera_rig.displayed_target = base.target;
        conversation_camera_rig.displayed_offset = base_offset;
        conversation_camera_rig.displayed_fovy = base_fovy;
        conversation_camera_rig.last_clock = clock;
        conversation_camera_rig.initialized = true;
    }

    if (available) conversation_camera_rig.engaged = true;
    if (!conversation_camera_rig.engaged) {
        conversation_camera_rig.displayed_target = base.target;
        conversation_camera_rig.displayed_offset = base_offset;
        conversation_camera_rig.displayed_fovy = base_fovy;
    }
    if (advance) {
        float delta_time = clock - conversation_camera_rig.last_clock;
        if (delta_time < 0.0f || delta_time > 0.12f) delta_time = 0.0f;
        delta_time = fminf(delta_time, 0.050f);
        conversation_camera_rig.last_clock = clock;
        float direction = available ? 2.8f : -2.0f;
        conversation_camera_rig.focus_weight = CombatClamp(
            conversation_camera_rig.focus_weight + delta_time * direction,
            0.0f, 1.0f);

        Vector3 close_target = conversation_camera_rig.close_target;
        Vector3 close_offset = conversation_camera_rig.close_offset;
        float close_fovy = conversation_camera_rig.close_fovy;
        if (available) {
            Vector3 first = Vector3Add(
                player->position, (Vector3){0.0f, 1.02f, 0.0f});
            Vector3 second = Vector3Add(
                partner->position, (Vector3){0.0f, 1.02f, 0.0f});
            Vector3 talk_line = Vector3Subtract(second, first);
            talk_line.y = 0.0f;
            float span = fmaxf(0.85f, Vector3Length(talk_line));
            talk_line = PhysicsNormalizeOr(
                talk_line, (Vector3){1.0f, 0.0f, 0.0f});
            Vector3 shoulder = {-talk_line.z, 0.0f, talk_line.x};
            Vector3 base_view = Vector3Subtract(base.position, base.target);
            base_view.y = 0.0f;
            base_view = PhysicsNormalizeOr(
                base_view, (Vector3){0.0f, 0.0f, 1.0f});
            if (Vector3DotProduct(shoulder, base_view) < 0.0f) {
                shoulder = Vector3Negate(shoulder);
            }

            close_target = Vector3Lerp(first, second, 0.56f);
            close_target.y = (first.y + second.y) * 0.5f - 0.18f;
            close_offset = Vector3Add(
                Vector3Scale(shoulder,
                             5.05f + fminf(span, 3.0f) * 0.18f),
                Vector3Add(Vector3Scale(talk_line, -0.55f),
                           (Vector3){0.0f, 1.88f, 0.0f}));
            close_fovy = CombatClamp(38.0f + span * 0.8f, 39.0f, 43.0f);
            Camera3D close = PerspectiveCameraComposed(
                close_target, close_offset, close_fovy);
            close = CcLocalCameraClearSightlinesInternal(
                close, first, second, conversation_camera_rig.tree_clear_angle,
                &conversation_camera_rig.tree_clear_angle);
            close_offset = Vector3Subtract(close.position, close.target);
            conversation_camera_rig.close_target = close_target;
            conversation_camera_rig.close_offset = close_offset;
            conversation_camera_rig.close_fovy = close_fovy;
        }

        float weight = SmoothStep01(conversation_camera_rig.focus_weight);
        Vector3 desired_target = Vector3Lerp(base.target, close_target, weight);
        Vector3 desired_offset = Vector3Lerp(base_offset, close_offset, weight);
        float base_span = 2.0f * Vector3Length(base_offset) * tanf(base_fovy * DEG2RAD * 0.5f);
        float close_span = 2.0f * Vector3Length(close_offset) * tanf(close_fovy * DEG2RAD * 0.5f);
        float desired_span = base_span + (close_span - base_span) * weight;
        float displayed_span = 2.0f * Vector3Length(conversation_camera_rig.displayed_offset) *
            tanf(conversation_camera_rig.displayed_fovy * DEG2RAD * 0.5f);
        float ease = 1.0f - expf(-delta_time * 5.2f);
        conversation_camera_rig.displayed_target = Vector3Lerp(
            conversation_camera_rig.displayed_target, desired_target, ease);
        conversation_camera_rig.displayed_offset = Vector3Lerp(
            conversation_camera_rig.displayed_offset, desired_offset, ease);
        displayed_span += (desired_span - displayed_span) * ease;
        conversation_camera_rig.displayed_fovy = 2.0f * atanf(displayed_span * 0.5f /
            fmaxf(0.25f, Vector3Length(conversation_camera_rig.displayed_offset))) * RAD2DEG;

    }

    if (!available && conversation_camera_rig.focus_weight <= 0.0001f &&
        Vector3Distance(conversation_camera_rig.displayed_target, base.target) < 0.0001f &&
        Vector3Distance(conversation_camera_rig.displayed_offset, base_offset) < 0.0001f &&
        fabsf(conversation_camera_rig.displayed_fovy - base_fovy) < 0.0001f) {
        conversation_camera_rig.engaged = false;
    }
    Camera3D camera = base;
    camera.target = conversation_camera_rig.displayed_target;
    camera.position = Vector3Add(
        camera.target, conversation_camera_rig.displayed_offset);
    camera.fovy = conversation_camera_rig.displayed_fovy;
    camera.projection = CAMERA_PERSPECTIVE;
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

    camera.target = (Vector3){4.70f, 0.90f, 3.30f};
    camera.position = (Vector3){camera.target.x + 1.5f,
                                camera.target.y + 3.7f,
                                camera.target.z + 13.0f};
    camera.up = (Vector3){0.0f, 1.0f, 0.0f};
    camera.fovy = 6.45f;
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
