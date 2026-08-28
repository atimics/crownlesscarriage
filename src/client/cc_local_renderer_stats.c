#include "client/cc_local3d_internal.h"

#include <math.h>
#include <stdlib.h>

static CcLocalRendererStats renderer_stats = {0};
#define CC_RENDER_FRAME_SAMPLE_COUNT 240
static float frame_samples[CC_RENDER_FRAME_SAMPLE_COUNT] = {0};
static int32_t frame_sample_count = 0;
static int32_t frame_sample_cursor = 0;

static int CompareFrameMilliseconds(const void *left, const void *right)
{
    float a = *(const float *)left;
    float b = *(const float *)right;
    return (a > b) - (a < b);
}

static void UpdateFramePercentiles(float milliseconds)
{
    if (milliseconds <= 0.0f) return;
    frame_samples[frame_sample_cursor] = milliseconds;
    frame_sample_cursor =
        (frame_sample_cursor + 1) % CC_RENDER_FRAME_SAMPLE_COUNT;
    if (frame_sample_count < CC_RENDER_FRAME_SAMPLE_COUNT) {
        frame_sample_count += 1;
    }
}

static void RefreshFramePercentiles(void)
{
    if (frame_sample_count <= 0) return;
    float sorted[CC_RENDER_FRAME_SAMPLE_COUNT];
    int32_t hitch_count = 0;
    for (int32_t i = 0; i < frame_sample_count; ++i) {
        sorted[i] = frame_samples[i];
        if (frame_samples[i] > 25.0f) hitch_count += 1;
    }
    qsort(sorted, (size_t)frame_sample_count, sizeof(sorted[0]),
          CompareFrameMilliseconds);
    int32_t p95 = (frame_sample_count * 95 + 99) / 100 - 1;
    int32_t p99 = (frame_sample_count * 99 + 99) / 100 - 1;
    renderer_stats.p95_frame_milliseconds = sorted[p95];
    renderer_stats.p99_frame_milliseconds = sorted[p99];
    renderer_stats.maximum_frame_milliseconds =
        sorted[frame_sample_count - 1];
    renderer_stats.hitch_count = hitch_count;
}

void CcLocalRendererBeginFrame(float delta_time)
{
    float milliseconds = fmaxf(0.0f, delta_time) * 1000.0f;
    UpdateFramePercentiles(milliseconds);
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
    renderer_stats.hero_skin_updates = 0;
    renderer_stats.hero_skinned_meshes = 0;
}

CcLocalRendererStats CcLocalRendererGetStats(void)
{
    RefreshFramePercentiles();
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

void CcLocalRendererRecordHeroSkinUpdate(int32_t mesh_count)
{
    CcLocalRendererRecordSkinUpdate(mesh_count);
    renderer_stats.hero_skin_updates += 1;
    renderer_stats.hero_skinned_meshes += mesh_count;
}
