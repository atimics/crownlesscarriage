/* The dragon-roost camera must keep both the hero and the roosting dragon
   readable at every life stage. A fixed hero-relative offset once ignored
   the dragon's actual size and position: the crowned and deep-wyrm stages
   (13-20 world units long) filled the frame like a ribbon and hid the hero
   entirely (see docs/reviews/creature-framing-2026-09-24). This exercises
   CcLocalSiteCamera directly, the same way TestGroundTownCameras exercises
   CcLocalStreetCameraInternal, so it needs no render target or GL context. */
static void TestDragonRoostCameraFraming(void)
{
    const int32_t art_height = CC_LOCAL_ART_HEIGHT;
    const int32_t art_width = (int32_t)lroundf(
        (float)art_height * ((float)CC_LOCAL_ART_WIDTH / (float)CC_LOCAL_ART_HEIGHT));
    const CcDragonLifeStage stages[] = {
        CC_DRAGON_STAGE_WHELP, CC_DRAGON_STAGE_WANDERER,
        CC_DRAGON_STAGE_CROWNED, CC_DRAGON_STAGE_DEEP_WYRM,
    };
    static CcSim sim;
    CcSimInit(&sim, UINT32_C(0xc0a71a9e));
    int32_t checked = 0;
    for (size_t stage = 0; stage < sizeof(stages) / sizeof(stages[0]); ++stage) {
        sim.dragon.life_stage = stages[stage];
        CcCreatureVariant variant = DragonCreatureForLifeStage(stages[stage]);
        float back_offset = 0.0f, body_length = 0.0f;
        DragonRoostReachInternal(variant, &back_offset, &body_length);
        Vector3 dragon_position = {
            CC_LOCAL_SITE_ENTRANCE_X - back_offset, 7.58f,
            CC_LOCAL_SITE_ENTRANCE_Z + 4.85f
        };
        /* The dragon-specific camera only takes over once the hero has
           walked within range of the roost; sample that whole approach. */
        for (float hero_x = 46.0f; hero_x <= 69.0f; hero_x += 4.0f) {
            CcLocalAgent hero;
            CcLocalAgentInit(&hero,
                (Vector2){hero_x, CC_LOCAL_SITE_ENTRANCE_Z}, false);
            Camera3D camera = CcLocalSiteCamera(&sim, &hero,
                CC_LOCAL_SITE_DRAGON_CAVE, false, false, 0.0f, 0.0f,
                art_height);
            RequireRenderer(camera.projection == CAMERA_PERSPECTIVE,
                "dragon roost camera must stay a perspective lens");
            Vector3 forward = Vector3Normalize(
                Vector3Subtract(camera.target, camera.position));
            float hero_depth = Vector3DotProduct(
                Vector3Subtract(hero.position, camera.position), forward);
            float dragon_depth = Vector3DotProduct(
                Vector3Subtract(dragon_position, camera.position), forward);
            RequireRenderer(hero_depth > 2.0f,
                "the hero must not sit within the roost camera's near plane");
            RequireRenderer(dragon_depth > 2.0f,
                "the dragon must not sit within the roost camera's near plane");
            Vector2 hero_pixel = GetWorldToScreenEx(
                Vector3Add(hero.position, (Vector3){0.0f, 1.0f, 0.0f}),
                camera, art_width, art_height);
            Vector2 dragon_pixel = GetWorldToScreenEx(
                dragon_position, camera, art_width, art_height);
            if (!(hero_pixel.x >= 0.0f && hero_pixel.x <= (float)art_width &&
                  hero_pixel.y >= 0.0f && hero_pixel.y <= (float)art_height) ||
                !(dragon_pixel.x >= 0.0f && dragon_pixel.x <= (float)art_width &&
                  dragon_pixel.y >= 0.0f && dragon_pixel.y <= (float)art_height)) {
                fprintf(stderr,
                    "roost stage %d hero_x %.1f: hero pixel %.1f,%.1f dragon pixel %.1f,%.1f\n",
                    (int)stages[stage], (double)hero_x,
                    (double)hero_pixel.x, (double)hero_pixel.y,
                    (double)dragon_pixel.x, (double)dragon_pixel.y);
            }
            RequireRenderer(hero_pixel.x >= 0.0f && hero_pixel.x <= (float)art_width &&
                            hero_pixel.y >= 0.0f && hero_pixel.y <= (float)art_height,
                "the hero must project inside the roost viewport");
            RequireRenderer(dragon_pixel.x >= 0.0f && dragon_pixel.x <= (float)art_width &&
                            dragon_pixel.y >= 0.0f && dragon_pixel.y <= (float)art_height,
                "the dragon must project inside the roost viewport at every life stage");
            /* Projecting inside the frame is not enough on its own: the old
               formula's dragon body and hero both had valid on-screen
               centers even while the body ran edge to edge like a ribbon.
               Project the body's actual nose and tail (its yaw and length,
               as drawn in DrawDragonCliffRoost) and check they do not span
               almost the whole frame width. */
            float yaw = variant == CC_CREATURE_DRAGON_WHELP ?
                -0.48f * PI : 0.48f * PI;
            Vector3 axis = {sinf(yaw), 0.0f, cosf(yaw)};
            Vector3 nose = Vector3Add(dragon_position,
                Vector3Scale(axis, body_length * 0.5f));
            Vector3 tail = Vector3Subtract(dragon_position,
                Vector3Scale(axis, body_length * 0.5f));
            Vector2 nose_pixel = GetWorldToScreenEx(nose, camera, art_width, art_height);
            Vector2 tail_pixel = GetWorldToScreenEx(tail, camera, art_width, art_height);
            float body_span = fabsf(nose_pixel.x - tail_pixel.x);
            if (body_span >= (float)art_width * 0.85f)
                fprintf(stderr,
                    "roost stage %d hero_x %.1f: body span %.1f of %d px wide\n",
                    (int)stages[stage], (double)hero_x, (double)body_span, art_width);
            RequireRenderer(body_span < (float)art_width * 0.85f,
                "the dragon's body must not stretch edge to edge like a ribbon");
            checked++;
        }
    }
    printf("PASS dragon roost camera framing: %d stage/approach samples keep hero and dragon on screen\n",
           checked);
}
