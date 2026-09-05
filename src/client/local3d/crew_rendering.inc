/* Remote travellers use the same skeleton and appearance as the local hero. */
_Static_assert(sizeof(CcHumanoidPose) == 79 * sizeof(float), "Crew pose layout");
static CcCrewExchange crew_exchange;
static CcCrewMember crew_drawn[CC_CREW_CAPACITY];
static int32_t crew_drawn_count;
static int32_t crew_scene = -1;
static float crew_delta;
static bool crew_rendered;
static int32_t crew_seat;

void CcLocalCrewSetExchange(CcCrewExchange exchange) { crew_exchange = exchange; }
void CcLocalCrewSetSeat(int32_t seat) { crew_seat = seat; }
void CcLocalCrewBeginFrame(float delta_time)
{
    crew_rendered = false;
    crew_delta = fminf(fmaxf(delta_time, 0.0f), 0.1f);
}
const CcCrewMember *CcLocalCrewDrawn(int32_t *count)
{
    *count = crew_rendered ? crew_drawn_count : 0;
    return crew_drawn;
}
static void DrawCrew(const CcLocalAgent *local, int32_t scene)
{
    if (crew_exchange == NULL || local == NULL) return;
    crew_rendered = true;
    float pose[CC_CREW_POSE_FLOATS] = {
        local->position.x, local->position.y, local->position.z, local->facing_yaw
    };
    memcpy(pose + 4, AgentRenderPose(local), sizeof(CcHumanoidPose));
    CcCrewMember incoming[CC_CREW_CAPACITY] = {0};
    int32_t count = crew_exchange(scene, pose, incoming);
    count = count < 0 ? 0 : count > CC_CREW_CAPACITY ? CC_CREW_CAPACITY : count;
    float blend = 1.0f - expf(-crew_delta * 20.0f);
    for (int32_t i = 0; i < count; ++i) {
        if (scene == crew_scene) {
            for (int32_t j = 0; j < crew_drawn_count; ++j) {
                const CcCrewMember *old = &crew_drawn[j];
                if (strcmp(old->id, incoming[i].id) != 0) continue;
                float dx = old->pose[0] - incoming[i].pose[0];
                float dz = old->pose[2] - incoming[i].pose[2];
                if (dx * dx + dz * dz > 144.0f) break;
                for (int32_t k = 0; k < CC_CREW_POSE_FLOATS; ++k) {
                    float difference = incoming[i].pose[k] - old->pose[k];
                    if (k == 3 || k >= 73) difference = remainderf(difference, 2.0f * PI);
                    incoming[i].pose[k] = old->pose[k] + blend * difference;
                }
                break;
            }
        }
        /* Reuse the hero renderer; remote motion comes from the received pose. */
        static CcLocalAgent display;
        display = *local;
        display.position = (Vector3){incoming[i].pose[0], incoming[i].pose[1], incoming[i].pose[2]};
        display.facing_yaw = incoming[i].pose[3];
        display.appearance = CcNpcPlayerAppearance(incoming[i].appearance);
        display.crowned = true;
        memcpy(&display.render_pose, incoming[i].pose + 4, sizeof(display.render_pose));
        display.render_pose_valid = true;
        DrawBiomechanicalBiped(&display);
    }
    memcpy(crew_drawn, incoming, sizeof(crew_drawn));
    crew_drawn_count = count;
    crew_scene = scene;
}
static void DrawCrewLabels(Camera3D camera, Rectangle destination)
{
    WorldLabel labels[CC_CREW_CAPACITY];
    for (int32_t i = 0; i < crew_drawn_count; ++i) {
        /* Head is joint four in the transmitted skeleton. */
        labels[i] = (WorldLabel){
            {crew_drawn[i].pose[16], crew_drawn[i].pose[17] + 0.6f, crew_drawn[i].pose[18]},
            crew_drawn[i].name, WORLD_TEAL};
    }
    DrawLabels(labels, crew_drawn_count, camera, destination);
}
static void PrepareCrewPassenger(const CcLocalAgent *source, Vector3 carriage,
                                 float heading, CcLocalAgent *display)
{
    Vector3 position = LocalPoint(carriage, -1.6f + (float)(crew_seat % 4) * 0.85f,
                                  1.15f, -0.65f + (float)(crew_seat / 4) * 1.25f, heading);
    PrepareHeroDisplayAt(source, position, source->scene, heading, display);
}
