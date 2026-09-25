#ifndef CROWNLESS_LOCAL_VIEWPORT_H
#define CROWNLESS_LOCAL_VIEWPORT_H

#include "raylib.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

/* Classic's compile-time render target size. tools/art/run_art_check.py
   parses this file with a regex looking for exactly "#define NAME digits",
   so these two lines must stay literal decimal macros -- the runtime,
   pack-selected size lives in the accessors below instead. */
#define CC_LOCAL_ART_WIDTH 630
#define CC_LOCAL_ART_HEIGHT 320
#define CC_LOCAL_VIEWPORT_SIDE_MARGIN 10
#define CC_LOCAL_VIEWPORT_TOP_MARGIN 54
#define CC_LOCAL_VIEWPORT_BOTTOM_MARGIN 66

/* Runtime render-target size, set once from the active style pack before
   any window or GPU state exists (see CcStylePackLoad in cc_style_pack.c).
   A build that never loads a style pack keeps the classic values above.
   The storage is extern with a single definition in cc_style_pack.c: this
   header is included from more than one translation unit (main.c, and the
   local3d renderer's own unity build in cc_local3d.c), and both need to
   observe the same pack-selected size, not a private per-TU copy. */
extern int32_t cc_active_art_width;
extern int32_t cc_active_art_height;
extern int32_t cc_active_art_upscale_filter; /* a TextureFilter value */

static inline int32_t CcArtWidth(void) { return cc_active_art_width; }
static inline int32_t CcArtHeight(void) { return cc_active_art_height; }
static inline int32_t CcArtUpscaleFilter(void)
{
    return cc_active_art_upscale_filter;
}

static inline void CcSetActiveArtViewport(int32_t width, int32_t height,
                                          int32_t upscale_filter)
{
    if (width > 0) cc_active_art_width = width;
    if (height > 0) cc_active_art_height = height;
    cc_active_art_upscale_filter = upscale_filter;
}

static inline Rectangle CcLocalViewportBounds(int screen_width,
                                              int screen_height)
{
    float art_width = (float)CcArtWidth();
    float art_height = (float)CcArtHeight();
    float available_width = (float)screen_width -
        (float)CC_LOCAL_VIEWPORT_SIDE_MARGIN * 2.0f;
    float available_height = (float)screen_height -
        (float)CC_LOCAL_VIEWPORT_TOP_MARGIN -
        (float)CC_LOCAL_VIEWPORT_BOTTOM_MARGIN;
    float available_scale = fminf(
        available_width / art_width,
        available_height / art_height);
    float scale = floorf(available_scale);
    if (scale < 2.0f) scale = available_scale;
    if (scale < 0.50f) scale = 0.50f;
    float width = art_width * scale;
    float height = art_height * scale;
    return (Rectangle){((float)screen_width - width) * 0.5f,
        (float)CC_LOCAL_VIEWPORT_TOP_MARGIN +
            (available_height - height) * 0.5f,
        width, height};
}

#endif
