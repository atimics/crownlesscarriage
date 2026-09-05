/* Matched views of the production hero and head families. */
static Image CaptureHeroFaceView(CcNpcAppearance appearance, float yaw,
                                 bool close)
{
    const int32_t size = 192;
    RenderTexture2D target = LoadRenderTexture(size, size);
    RequireRenderer(IsRenderTextureValid(target), "hero review target must load");
    CcLocalAgent agent = {0};
    agent.crowned = true;
    agent.appearance = appearance;
    agent.combat.health = 100.0f;
    agent.facing_yaw = yaw;
    CcHumanoidGaitInit(&agent.humanoid, (CcLimbVec3){0}, yaw,
                       FlatSkinProbe, NULL);
    CcHumanoidSkinPose skin;
    CcHumanoidSkinPoseResolve(&agent.humanoid.pose, &skin);
    Vector3 head = FromLimbVector(skin.sockets[CC_HUMANOID_SOCKET_HEAD].position);
    Vector3 focus = close ? Vector3Add(head, (Vector3){0, -0.065f, 0}) :
                            (Vector3){0, 0.95f, 0};
    Camera3D camera = {
        .position = Vector3Add(focus, (Vector3){0, 0.28f, 5.0f}),
        .target = focus, .up = {0, 1, 0},
        .fovy = close ? 0.62f : 2.45f,
        .projection = CAMERA_ORTHOGRAPHIC,
    };
    rlDrawRenderBatchActive();
    BeginTextureMode(target);
    ClearBackground((Color){24, 28, 34, 255});
    SetFaceRenderContext(camera, size, size);
    BeginMode3D(camera);
    BeginWorldLighting(camera, &INTERIOR_ART_COMPOSITION);
    UseCharacterLighting();
    RequireRenderer(DrawDynamicNpcModules(&agent, &skin, &appearance),
                    "hero review must use the production modules");
    DrawWayfarerHeroDetails(&skin);
    RestoreWorldLighting();
    EndWorldLighting();
    EndMode3D();
    EndTextureMode();
    BeginDrawing();
    ClearBackground(BLACK);
    PresentTarget(target, (Rectangle){0, 0, size, size});
    rlDrawRenderBatchActive();
    Image shot = LoadImageFromScreen();
    EndDrawing();
    ImageCrop(&shot, (Rectangle){0, 0, size, size});
    UnloadRenderTexture(target);
    return shot;
}

static void CaptureHeroFaces(const char *directory)
{
    RequireRenderer(MakeDirectory(directory) == 0 || DirectoryExists(directory),
                    "hero review directory must exist");
    const float turns[] = {0.0f, -0.65f, -1.35f, PI};
    const char *names[] = {"Front", "Three quarter", "Profile", "Back"};
    Image sheet = GenImageColor(192 * 4, 216, (Color){24, 28, 34, 255});
    for (int32_t view = 0; view < 4; ++view) {
        Image shot = CaptureHeroFaceView(CcNpcCrownlessAppearance(), turns[view],
                                         false);
        ImageDraw(&sheet, shot, (Rectangle){0, 0, 192, 192},
                  (Rectangle){(float)view * 192, 24, 192, 192}, WHITE);
        ImageDrawText(&sheet, names[view], view * 192 + 8, 6, 12, RAYWHITE);
        UnloadImage(shot);
    }
    char path[1024];
    (void)snprintf(path, sizeof(path), "%s/hero.png", directory);
    RequireRenderer(ExportImage(sheet, path), "hero sheet must be written");
    UnloadImage(sheet);
    sheet = GenImageColor(192 * CC_NPC_HEAD_FAMILY_COUNT, 216 * 3,
                          (Color){24, 28, 34, 255});
    for (int32_t family = 0; family < CC_NPC_HEAD_FAMILY_COUNT; ++family) {
        CcNpcAppearance appearance = CcNpcPlayerAppearance(
            (uint32_t)family | ((uint32_t)family << 3U) |
            ((uint32_t)family << 6U) | ((uint32_t)family << 9U));
        for (int32_t view = 0; view < 3; ++view) {
            Image shot = CaptureHeroFaceView(appearance, turns[view], true);
            ImageDraw(&sheet, shot, (Rectangle){0, 0, 192, 192},
                (Rectangle){(float)family * 192, (float)view * 216 + 24,
                            192, 192}, WHITE);
            ImageDrawText(&sheet, TextFormat("Face %d / %s", family + 1, names[view]),
                           family * 192 + 8, view * 216 + 6, 12, RAYWHITE);
            UnloadImage(shot);
        }
    }
    (void)snprintf(path, sizeof(path), "%s/faces.png", directory);
    RequireRenderer(ExportImage(sheet, path), "face sheet must be written");
    UnloadImage(sheet);
    (void)puts("PASS hero and face captures: four hero turns and all head families");
}
