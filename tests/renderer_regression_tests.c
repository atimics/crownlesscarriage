/* Exercise the same private skin and shader paths used by the client. */
#include "client/cc_local3d.c"

static void RequireRenderer(bool condition, const char *message)
{
    if (condition) return;
    (void)fprintf(stderr, "%s\n", message);
    exit(1);
}

static bool FlatSkinProbe(void *context, CcLimbVec3 origin, float drop,
                           CcLimbVec3 *point, CcLimbVec3 *normal)
{
    (void)context;
    if (origin.y < 0.0f || origin.y > drop) return false;
    *point = (CcLimbVec3){origin.x, 0.0f, origin.z};
    *normal = (CcLimbVec3){0.0f, 1.0f, 0.0f};
    return true;
}

static void TestSkinTurns(void)
{
    CcHumanoidGait initial;
    CcHumanoidGaitInit(&initial, (CcLimbVec3){0}, 0.0f,
                      FlatSkinProbe, NULL);
    CcHumanoidSkinPose initial_skin;
    CcHumanoidSkinPoseResolve(&initial.pose, &initial_skin);
    const Quaternion bind_rotations[] = {
        {0.0f, 0.0f, 0.0f, 1.0f},
        {0.5f, 0.5f, 0.5f, 0.5f},
    };
    const Vector3 axes[] = {
        {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f},
    };
    for (int32_t turn = 1; turn <= 3; ++turn) {
        float yaw = (float)turn * PI * 0.5f;
        Quaternion world_turn = QuaternionFromAxisAngle(
            (Vector3){0.0f, 1.0f, 0.0f}, yaw);
        CcHumanoidGait turned;
        CcHumanoidGaitInit(&turned, (CcLimbVec3){0}, yaw,
                          FlatSkinProbe, NULL);
        CcHumanoidSkinPose turned_skin;
        CcHumanoidSkinPoseResolve(&turned.pose, &turned_skin);
        for (int32_t bone = 0; bone < CC_HUMANOID_SKIN_BONE_COUNT; ++bone) {
            for (int32_t bind = 0; bind < 2; ++bind) {
                Quaternion before = HumanoidSkinRotation(
                    (CcHumanoidSkinBone)bone, &initial_skin.bones[bone],
                    bind_rotations[bind]);
                Quaternion after = HumanoidSkinRotation(
                    (CcHumanoidSkinBone)bone, &turned_skin.bones[bone],
                    bind_rotations[bind]);
                for (int32_t axis = 0; axis < 3; ++axis) {
                    Vector3 expected = Vector3RotateByQuaternion(
                        Vector3RotateByQuaternion(axes[axis], before),
                        world_turn);
                    Vector3 actual = Vector3RotateByQuaternion(
                        axes[axis], after);
                    RequireRenderer(Vector3Distance(expected, actual) < 0.001f,
                                    "skin mesh must follow the whole character turn");
                }
            }
        }
    }
    (void)puts("PASS skin rotation: all bones, three quarter turns, two bind frames");
}

static void TestCharacterPrimitives(void)
{
    RenderTexture2D target = LoadRenderTexture(256, 256);
    Camera3D camera = {
        .position = {4.0f, 3.0f, 5.0f}, .target = {0.0f, 0.0f, 0.0f},
        .up = {0.0f, 1.0f, 0.0f}, .fovy = 4.0f,
        .projection = CAMERA_ORTHOGRAPHIC,
    };
    RequireRenderer(visual_style.character.ready && visual_style.hero.ready,
                    "both character shaders must load");
    RequireRenderer(IsRenderTextureValid(target), "primitive target must load");
    for (int32_t pass = 0; pass < 2; ++pass) {
        if (pass == 1) {
            /* A preceding skinned draw leaves bone transforms on its shader. */
            Matrix bones[CC_HERO_SKIN_MAX_BONES];
            for (int32_t bone = 0; bone < CC_HERO_SKIN_MAX_BONES; ++bone) {
                bones[bone] = MatrixTranslate(25.0f, 0.0f, 25.0f);
            }
            rlEnableShader(visual_style.hero.shader.id);
            rlSetUniformMatrices(
                visual_style.hero.shader.locs[SHADER_LOC_MATRIX_BONETRANSFORMS],
                bones, CC_HERO_SKIN_MAX_BONES);
        }
        BeginTextureMode(target);
        ClearBackground(BLACK);
        BeginMode3D(camera);
        BeginWorldLighting(camera, &INTERIOR_ART_COMPOSITION);
        UseCharacterLighting();
        DrawCylinderEx((Vector3){-0.55f, -0.8f, 0.0f},
                       (Vector3){-0.55f, 0.8f, 0.0f},
                       0.25f, 0.25f, 12, RED);
        DrawCubeV((Vector3){0.6f, 0.0f, 0.0f},
                  (Vector3){0.55f, 1.3f, 0.55f}, BLUE);
        RestoreWorldLighting();
        EndWorldLighting();
        EndMode3D();
        EndTextureMode();
        Image shot = LoadImageFromTexture(target.texture);
        Color *pixels = LoadImageColors(shot);
        RequireRenderer(pixels != NULL, "primitive image must be readable");
        int32_t red = 0;
        int32_t blue = 0;
        for (int32_t pixel = 0; pixel < shot.width * shot.height; ++pixel) {
            Color color = pixels[pixel];
            if ((int32_t)color.r > (int32_t)color.b * 2 && color.r > 60) red++;
            if ((int32_t)color.b > (int32_t)color.r * 2 && color.b > 60) blue++;
        }
        RequireRenderer(red > 100 && blue > 100,
                        "procedural parts must retain geometry and vertex colors");
        (void)printf("PASS primitive pass %d: %d red pixels, %d blue pixels\n",
                     pass, red, blue);
        UnloadImageColors(pixels);
        UnloadImage(shot);
    }
    UnloadRenderTexture(target);
}

static void WriteViewportFixture(const char *path)
{
    Rectangle viewport = CcLocalViewportBounds(GetScreenWidth(), GetScreenHeight());
    BeginDrawing();
    ClearBackground(MAGENTA);
    DrawRectangleRec(viewport, (Color){255, 0, 0, 255});
    DrawRectangleLinesEx(viewport, 8.0f, GREEN);
    rlDrawRenderBatchActive();
    Image shot = LoadImageFromScreen();
    EndDrawing();
    RequireRenderer(ExportImage(shot, path), "viewport fixture must be written");
    UnloadImage(shot);
}

#include "animal_captures.inc"

int main(int argc, char **argv)
{
    TestSkinTurns();
    if (argc == 3 && strcmp(argv[1], "--graphics") == 0) {
        SetConfigFlags(FLAG_WINDOW_HIDDEN);
        InitWindow(1280, 760, "Renderer regression checks");
        SetTraceLogLevel(LOG_WARNING);
        CcLocalRendererInit();
        TestCharacterPrimitives();
        TestAnimalModels(argv[2]);
        WriteViewportFixture(argv[2]);
        CcLocalRendererShutdown();
        CloseWindow();
    }
    return 0;
}
