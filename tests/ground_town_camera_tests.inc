/* The rendered town view must stay low even after settling and reframing.
   Low authoring values alone did not catch the old forced-dive/2.5x override. */
static void TestGroundTownCameras(void)
{
    const uint32_t seeds[] = {UINT32_C(0xc0a71a9e), UINT32_C(0x12345678)};
    float maximum_eye = 0.0f;
    float maximum_dive = 0.0f;
    for (int seed = 0; seed < 2; ++seed) {
        static CcSim sim;
        CcSimInit(&sim, seeds[seed]);
        for (int town = 0; town < 6; ++town) {
            sim.player.location_id = sim.settlements[town].id;
            CcLocalBindPlace(&sim);
            for (int shot = 0; shot < CC_LOCAL_PLACE_SCENE_COUNT; ++shot) {
                const CcLocalTownScene *scene = CcLocalTownSceneAt(
                    active_place_function, shot);
                street_camera_rig = (FixedCameraRig){0};
                CcLocalAgent hero;
                CcLocalAgentInit(&hero,
                    (Vector2){scene->trigger_x, scene->trigger_z}, false);
                Camera3D camera = {0};
                for (int frame = 0; frame < 180; ++frame)
                    camera = CcLocalStreetCameraInternal(&hero,
                        (float)frame / 60.0f, true, 320);
                float eye = camera.position.y - CcLocalTerrainHeightAt(
                    camera.position.x, camera.position.z);
                float horizontal = hypotf(camera.position.x - camera.target.x,
                                           camera.position.z - camera.target.z);
                float dive = atan2f(camera.position.y - camera.target.y,
                                    horizontal) * RAD2DEG;
                maximum_eye = fmaxf(maximum_eye, eye);
                maximum_dive = fmaxf(maximum_dive, dive);
                if (!(eye >= 1.79f && eye <= 4.01f && dive <= 22.0f))
                    fprintf(stderr, "ground camera seed %d town %d scene %d: eye %.3f dive %.3f\n",
                            seed, town, shot, eye, dive);
                RequireRenderer(eye >= 1.79f && eye <= 4.01f && dive <= 22.0f,
                    "town lens must remain at ground level, not above the roofs");
                RequireRenderer(camera.projection == CAMERA_PERSPECTIVE &&
                    camera.fovy >= 18.0f && camera.fovy <= 85.0f,
                    "towns need a perspective street lens, not a telephoto map");
                RequireRenderer(CcLocalStreetCameraSceneInternal() == shot,
                    "each authored trigger must present its own named screen");
                Vector3 subject = Vector3Add(hero.position, (Vector3){0, 1.05f, 0});
                Vector2 pixel = GetWorldToScreenEx(subject, camera, 630, 320);
                RequireRenderer(pixel.x >= 120 && pixel.x <= 510 &&
                                pixel.y >= 60 && pixel.y <= 260,
                    "ground-level authored views must keep the actor readable");
                Camera3D paused = CcLocalStreetCameraInternal(&hero, 9.0f, false, 320);
                RequireRenderer(Vector3Distance(paused.position, camera.position) < 0.0001f &&
                    Vector3Distance(paused.target, camera.target) < 0.0001f,
                    "input queries must retain the exact presented ground camera");
                Ray ray = GetScreenToWorldRayEx(pixel, paused, 630, 320);
                Vector3 toward = Vector3Normalize(Vector3Subtract(subject, ray.position));
                RequireRenderer(Vector3DotProduct(toward, ray.direction) > 0.99999f,
                    "ground-camera projection and input ray must agree");
            }
        }
    }
    CcLocalBindPlace(NULL);
    printf("PASS ground town cameras: 36 screens, two seeds; max eye %.3f, max dive %.3f degrees\n",
           maximum_eye, maximum_dive);
}
