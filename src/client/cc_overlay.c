#include "client/cc_overlay.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#define CC_OVERLAY_MAX_TEXT_ITEMS 512
#define CC_OVERLAY_TEXT_CAPACITY 512

typedef struct CcOverlayTextItem {
    char text[CC_OVERLAY_TEXT_CAPACITY];
    int x;
    int y;
    int font_size;
    Color color;
} CcOverlayTextItem;

static CcOverlayTextItem overlay_text[CC_OVERLAY_MAX_TEXT_ITEMS];
static int overlay_text_count = 0;
static float overlay_text_scale = 1.0f;
static bool overlay_active = false;

static float ScaledFontSize(int font_size)
{
    /* Raylib's built-in glyphs lose their counters and stroke separation
       below nine screen pixels. HUD call sites still use relative sizes for
       hierarchy, but never shrink functional text into an unreadable mark. */
    return fmaxf(9.0f, (float)font_size * overlay_text_scale);
}

static void DrawOverlayTextItem(const CcOverlayTextItem *item)
{
    float font_size = ScaledFontSize(item->font_size);
    DrawTextEx(GetFontDefault(), item->text,
               (Vector2){(float)item->x, (float)item->y}, font_size,
               font_size / 10.0f, item->color);
}

void CcOverlayBegin(float text_scale)
{
    overlay_text_count = 0;
    overlay_text_scale = fmaxf(0.50f, fminf(text_scale, 3.0f));
    overlay_active = true;
}

void CcOverlayFlush(void)
{
    for (int i = 0; i < overlay_text_count; ++i) {
        DrawOverlayTextItem(&overlay_text[i]);
    }
    overlay_text_count = 0;
}

void CcOverlayEnd(void)
{
    CcOverlayFlush();
    overlay_active = false;
}

void CcOverlayDrawText(const char *text, int x, int y, int font_size,
                       Color color)
{
    if (text == NULL) return;
    if (!overlay_active) {
        CcOverlayTextItem item = {
            .x = x, .y = y, .font_size = font_size, .color = color
        };
        (void)snprintf(item.text, sizeof(item.text), "%s", text);
        DrawOverlayTextItem(&item);
        return;
    }
    if (overlay_text_count >= CC_OVERLAY_MAX_TEXT_ITEMS) CcOverlayFlush();
    CcOverlayTextItem *item = &overlay_text[overlay_text_count++];
    item->x = x;
    item->y = y;
    item->font_size = font_size;
    item->color = color;
    (void)snprintf(item->text, sizeof(item->text), "%s", text);
}

int CcOverlayMeasureText(const char *text, int font_size)
{
    if (text == NULL) return 0;
    float scaled_size = ScaledFontSize(font_size);
    Vector2 measured = MeasureTextEx(GetFontDefault(), text, scaled_size,
                                     scaled_size / 10.0f);
    return (int)lroundf(measured.x);
}
