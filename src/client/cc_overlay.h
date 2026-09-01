#ifndef CROWNLESS_OVERLAY_H
#define CROWNLESS_OVERLAY_H

#include "raylib.h"


void CcOverlayBegin(float text_scale);
void CcOverlayFlush(void);
void CcOverlayEnd(void);
void CcOverlayDrawText(const char *text, int x, int y, int font_size,
                       Color color);
int CcOverlayMeasureText(const char *text, int font_size);

#endif
