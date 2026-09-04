#ifndef CROWNLESS_LOCAL_VIEWPORT_H
#define CROWNLESS_LOCAL_VIEWPORT_H

#include "raylib.h"

#include <math.h>

#define CC_LOCAL_ART_WIDTH 630
#define CC_LOCAL_ART_HEIGHT 320
#define CC_LOCAL_VIEWPORT_SIDE_MARGIN 10
#define CC_LOCAL_VIEWPORT_TOP_MARGIN 54
#define CC_LOCAL_VIEWPORT_BOTTOM_MARGIN 66

static inline Rectangle CcLocalViewportBounds(int screen_width,
                                              int screen_height)
{
    float available_width = (float)screen_width -
        (float)CC_LOCAL_VIEWPORT_SIDE_MARGIN * 2.0f;
    float available_height = (float)screen_height -
        (float)CC_LOCAL_VIEWPORT_TOP_MARGIN -
        (float)CC_LOCAL_VIEWPORT_BOTTOM_MARGIN;
    float available_scale = fminf(
        available_width / (float)CC_LOCAL_ART_WIDTH,
        available_height / (float)CC_LOCAL_ART_HEIGHT);
    float scale = floorf(available_scale);
    if (scale < 2.0f) scale = available_scale;
    if (scale < 0.50f) scale = 0.50f;
    float width = (float)CC_LOCAL_ART_WIDTH * scale;
    float height = (float)CC_LOCAL_ART_HEIGHT * scale;
    return (Rectangle){((float)screen_width - width) * 0.5f,
        (float)CC_LOCAL_VIEWPORT_TOP_MARGIN +
            (available_height - height) * 0.5f,
        width, height};
}

#endif
