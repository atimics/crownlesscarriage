#include "client/cc_local3d_internal.h"

#include <math.h>

static CcLocalRendererStats renderer_stats = {0};

void CcLocalRendererBeginFrame(float delta_time)
{
    float milliseconds = fmaxf(0.0f, delta_time) * 1000.0f;
    renderer_stats.frame_milliseconds = milliseconds;
    if (renderer_stats.smoothed_frame_milliseconds <= 0.0f) {
        renderer_stats.smoothed_frame_milliseconds = milliseconds;
    } else {
        renderer_stats.smoothed_frame_milliseconds +=
            (milliseconds - renderer_stats.smoothed_frame_milliseconds) *
            0.08f;
    }
    renderer_stats.biomechanical_characters = 0;
    renderer_stats.high_detail_characters = 0;
    renderer_stats.low_detail_characters = 0;
    renderer_stats.skin_updates = 0;
    renderer_stats.skinned_meshes = 0;
}

CcLocalRendererStats CcLocalRendererGetStats(void)
{
    return renderer_stats;
}

void CcLocalRendererRecordBiped(bool high_detail)
{
    renderer_stats.biomechanical_characters += 1;
    if (high_detail) renderer_stats.high_detail_characters += 1;
    else renderer_stats.low_detail_characters += 1;
}

void CcLocalRendererRecordSkinUpdate(int32_t mesh_count)
{
    renderer_stats.skin_updates += 1;
    renderer_stats.skinned_meshes += mesh_count;
}
