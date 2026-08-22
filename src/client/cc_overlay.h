#ifndef CROWNLESS_OVERLAY_H
#define CROWNLESS_OVERLAY_H

#include "raylib.h"

/* Screen-space text is collected separately from the pixel-art world pass.
   The scale affects glyphs and their measurements without resampling the
   low-resolution scene texture. */
void CcOverlayBegin(float text_scale);
void CcOverlayFlush(void);
void CcOverlayEnd(void);
void CcOverlayDrawText(const char *text, int x, int y, int font_size,
                       Color color);
int CcOverlayMeasureText(const char *text, int font_size);

#endif
