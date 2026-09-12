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
    bool body;
} CcOverlayTextItem;

static CcOverlayTextItem overlay_text[CC_OVERLAY_MAX_TEXT_ITEMS];
static int overlay_text_count = 0;
static float overlay_text_scale = 1.0f;
static bool overlay_active = false;
static Font body_font;
static void (*overlay_text_observer)(const char *text);

void CcOverlaySetTextObserver(void (*observer)(const char *text))
{
    overlay_text_observer = observer;
}

static float ScaledFontSize(int font_size)
{

    return fmaxf(9.0f, (float)font_size * overlay_text_scale);
}

void CcOverlayLoadBodyFont(const char *path)
{
    CcOverlayUnloadBodyFont();
    if (path == NULL || !FileExists(path)) return;
    int codepoints[228];
    for (int i = 0; i < 224; ++i) codepoints[i] = 32 + i;
    codepoints[224] = 0x2014;
    codepoints[225] = 0x2019;
    codepoints[226] = 0x201c;
    codepoints[227] = 0x201d;
    Font loaded = LoadFontEx(path, 32, codepoints, 228);
    if (loaded.texture.id == 0 || loaded.texture.id == GetFontDefault().texture.id) return;
    body_font = loaded;
    SetTextureFilter(body_font.texture, TEXTURE_FILTER_BILINEAR);
}

void CcOverlayUnloadBodyFont(void)
{
    CcOverlayFlush();
    if (body_font.texture.id != 0) UnloadFont(body_font);
    body_font = (Font){0};
}

static Font OverlayFont(bool body)
{
    return body && body_font.texture.id != 0 ? body_font : GetFontDefault();
}

static float OverlaySpacing(bool body, float size)
{
    return body && body_font.texture.id != 0 ? 0.0f : size / 10.0f;
}

static void DrawOverlayTextItem(const CcOverlayTextItem *item)
{
    float font_size = ScaledFontSize(item->font_size);
    DrawTextEx(OverlayFont(item->body), item->text,
               (Vector2){(float)item->x, (float)item->y}, font_size,
               OverlaySpacing(item->body, font_size), item->color);
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

static void QueueOverlayText(const char *text, int x, int y, int font_size,
                             Color color, bool body)
{
    if (text == NULL) return;
    if (overlay_text_observer != NULL) overlay_text_observer(text);
    if (!overlay_active) {
        CcOverlayTextItem item = {
            .x = x, .y = y, .font_size = font_size, .color = color, .body = body
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
    item->body = body;
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

void CcOverlayDrawText(const char *text, int x, int y, int font_size, Color color)
{
    QueueOverlayText(text, x, y, font_size, color, false);
}

void CcOverlayDrawBodyText(const char *text, int x, int y, int font_size, Color color)
{
    QueueOverlayText(text, x, y, font_size, color, true);
}

int CcOverlayMeasureBodyText(const char *text, int font_size)
{
    if (text == NULL) return 0;
    float size = ScaledFontSize(font_size);
    return (int)lroundf(MeasureTextEx(OverlayFont(true), text, size,
                                    OverlaySpacing(true, size)).x);
}
