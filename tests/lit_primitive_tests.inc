/* raylib's DrawCylinder and DrawCylinderEx emit no vertex normals, so under
   the world lighting shader a cylinder shaded with whatever normal the batch
   drew last. These checks draw a box first, which leaves a sideways normal
   behind, and require the lit primitives to shade like the reference shapes
   whose normals are correct. */
static Image RenderLitCapFixture(RenderTexture2D target, bool reference)
{
    Camera3D camera = {.position = {0.0f, 8.0f, 0.001f}, .target = {0},
                       .up = {0, 1, 0}, .fovy = 6.0f,
                       .projection = CAMERA_ORTHOGRAPHIC};
    BeginTextureMode(target);
    ClearBackground(BLACK);
    BeginMode3D(camera);
    BeginWorldLighting(camera, &INTERIOR_ART_COMPOSITION);
    /* Out of frame; its last face leaves a -x normal in the batch. */
    DrawBox((Vector3){6.0f, 0.0f, 6.0f}, (Vector3){0.5f, 0.5f, 0.5f}, RED);
    Color color = {176, 150, 110, 255};
    if (reference) {
        DrawPlane((Vector3){0.0f, 0.0f, 0.0f}, (Vector2){2.4f, 2.4f}, color);
    } else {
        DrawLitCylinder((Vector3){0.0f, -0.02f, 0.0f}, 1.2f, 1.2f, 0.02f, 24,
                        color);
    }
    EndWorldLighting();
    EndMode3D();
    EndTextureMode();
    return LoadImageFromTexture(target.texture);
}

static Image RenderLitSideFixture(RenderTexture2D target, int32_t shape)
{
    Camera3D camera = {.position = {8.0f, 0.0f, 0.0f}, .target = {0},
                       .up = {0, 1, 0}, .fovy = 6.0f,
                       .projection = CAMERA_ORTHOGRAPHIC};
    BeginTextureMode(target);
    ClearBackground(BLACK);
    BeginMode3D(camera);
    BeginWorldLighting(camera, &INTERIOR_ART_COMPOSITION);
    DrawBox((Vector3){6.0f, 6.0f, 0.0f}, (Vector3){0.5f, 0.5f, 0.5f}, RED);
    Color color = {150, 160, 172, 255};
    if (shape == 0) {
        DrawBox((Vector3){0.0f, 0.0f, 0.0f}, (Vector3){1.0f, 2.0f, 1.0f},
                color);
    } else if (shape == 1) {
        DrawLitCylinder((Vector3){0.0f, -1.0f, 0.0f}, 0.5f, 0.5f, 2.0f, 32,
                        color);
    } else {
        DrawLitCylinderEx((Vector3){0.0f, -1.0f, 0.0f},
                          (Vector3){0.0f, 1.0f, 0.0f}, 0.5f, 0.5f, 32,
                          color);
    }
    EndWorldLighting();
    EndMode3D();
    EndTextureMode();
    return LoadImageFromTexture(target.texture);
}

static int32_t ColorGap(Color a, Color b)
{
    int32_t gap = abs((int)a.r - (int)b.r);
    if (abs((int)a.g - (int)b.g) > gap) gap = abs((int)a.g - (int)b.g);
    if (abs((int)a.b - (int)b.b) > gap) gap = abs((int)a.b - (int)b.b);
    return gap;
}

static void TestLitPrimitiveNormals(void)
{
    RenderTexture2D target = LoadRenderTexture(128, 128);
    RequireRenderer(IsRenderTextureValid(target), "lit primitive target must load");
    Image plane = RenderLitCapFixture(target, true);
    Image disc = RenderLitCapFixture(target, false);
    Color *plane_pixels = LoadImageColors(plane);
    Color *disc_pixels = LoadImageColors(disc);
    RequireRenderer(plane_pixels != NULL && disc_pixels != NULL,
                    "lit cap images must be readable");
    int32_t centre = 64 * plane.width + 64;
    Color plane_centre = plane_pixels[centre];
    Color disc_centre = disc_pixels[centre];
    (void)printf("Lit cap: plane %d,%d,%d  disc %d,%d,%d\n",
                 plane_centre.r, plane_centre.g, plane_centre.b,
                 disc_centre.r, disc_centre.g, disc_centre.b);
    RequireRenderer(plane_centre.r > 24 || plane_centre.g > 24,
                    "the reference plane must be lit");
    RequireRenderer(ColorGap(plane_centre, disc_centre) <= 4,
                    "a lit cylinder cap must shade like an upward plane, "
                    "whatever normal the batch drew last");
    UnloadImageColors(plane_pixels);
    UnloadImageColors(disc_pixels);
    UnloadImage(plane);
    UnloadImage(disc);

    Image box = RenderLitSideFixture(target, 0);
    Color *box_pixels = LoadImageColors(box);
    Color box_centre = box_pixels[centre];
    for (int32_t shape = 1; shape <= 2; ++shape) {
        Image side = RenderLitSideFixture(target, shape);
        Color *side_pixels = LoadImageColors(side);
        Color side_centre = side_pixels[centre];
        (void)printf("Lit side %d: box %d,%d,%d  cylinder %d,%d,%d\n", shape,
                     box_centre.r, box_centre.g, box_centre.b,
                     side_centre.r, side_centre.g, side_centre.b);
        RequireRenderer(ColorGap(box_centre, side_centre) <= 8,
                        "a lit cylinder side facing the camera must shade like "
                        "a box face with the same normal");
        UnloadImageColors(side_pixels);
        UnloadImage(side);
    }
    UnloadImageColors(box_pixels);
    UnloadImage(box);
    UnloadRenderTexture(target);
    (void)puts("PASS lit primitives: cylinder caps and sides carry their own normals");
}

/* The underground mine is drawn from tiles. Its walls must reach the ceiling
   slab, or the corridor looks out onto the void above every wall. */
static void TestMineTunnelEnclosure(void)
{
    float wall = CcLocalMineWallHeightInternal(false);
    RequireRenderer(wall >= CC_LOCAL_MINE_CEILING_Y + 0.06f,
                    "mine walls must reach past the ceiling slab");
    RequireRenderer(wall > CC_LOCAL_MINE_BAR_HEIGHT,
                    "rock must close the arch above the closed bar");
    RequireRenderer(CcLocalMineWallHeightInternal(true) == CC_LOCAL_MINE_BAR_HEIGHT,
                    "combat keeps the low walls the raised fight camera looks over");
    /* The border rows are rock, so the wall pass must cover the whole grid
       or a corridor beside the border would open onto the void. */
    RequireRenderer(!CcMineWalkableState(CC_MINE_LEVEL, 0, 1, true) &&
                        !CcMineWalkableState(CC_MINE_LEVEL, CC_MINE_WIDTH - 1, 1, true),
                    "the mine border is rock");
    /* A seated passenger must stay under the carriage roof; the head jewel
       was the pair of gold dots seen floating over the travelling wagon. */
    RequireRenderer(CC_LOCAL_CARRIAGE_SEAT_HEIGHT + CC_LOCAL_SEATED_FIGURE_HEIGHT <
                        CC_LOCAL_CARRIAGE_ROOF_HEIGHT,
                    "a seated passenger's head must stay under the cabin roof");
    (void)puts("PASS mine tunnel: walls meet the ceiling; passengers stay under the roof");
}
