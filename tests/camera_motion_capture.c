#include "client/cc_local3d.c"
#include <emscripten/emscripten.h>
static void RequireRenderer(bool condition, const char *message)
{
    if (condition) return;
    fprintf(stderr, "%s\n", message);
    abort();
}
#include "tests/travel_graphics_tests.inc"
EMSCRIPTEN_KEEPALIVE
void CaptureTests(void)
{
    TestTravelForestCameraTurn();
    TestTravelLeafShimmer();
}
static CcSim capture_sim;
static RenderTexture2D capture_target;
static int capture_town = -1;
static int capture_frame = -1;
EMSCRIPTEN_KEEPALIVE
void CaptureTown(int town, int frame)
{
    if (town != capture_town || frame <= capture_frame) {
        CcSimInit(&capture_sim, UINT32_C(0xc0a71a9e));
        capture_sim.player.location_id = capture_sim.settlements[town].id;
        CcLocalBindPlace(&capture_sim);
        street_camera_rig = (FixedCameraRig){0};
        combat_camera_rig = (CombatCameraRig){0};
        conversation_camera_rig = (ConversationCameraRig){0};
        CcLocalRendererSetOpeningStep(CC_LOCAL_OPENING_COMPLETE);
        CcLocalRendererSetInteractionUI(true);
        capture_town = town;
        capture_frame = -1;
    }
    CcLocalAgent hero;
    for (int step = capture_frame + 1; step <= frame; ++step) {
        float x = 52.0f + (float)step * 0.067f;
        CcLocalAgentInit(&hero, (Vector2){x, 35.8f}, false);
        float clock = 10.0f + (float)step / 60.0f;
        CcLocalRendererBeginFrame(1.0f / 60.0f);
        BeginDrawing();
        ClearBackground(BLACK);
        CcLocalDrawStreet3D(&capture_sim, &hero, NULL, false, NULL, clock,
                           capture_target, (Rectangle){0, 0, 630, 320});
        EndDrawing();
    }
    capture_frame = frame;
}
int main(void)
{
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(630, 320, "Graphics motion capture");
    CcLocalRendererInit();
    capture_target = LoadRenderTexture(630, 320);
    SetTextureFilter(capture_target.texture, TEXTURE_FILTER_POINT);
    CaptureTown(4, 0);
    emscripten_run_script("Module.captureReady = true;");
    return 0;
}
