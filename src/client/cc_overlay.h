#ifndef CROWNLESS_OVERLAY_H
#define CROWNLESS_OVERLAY_H

#include "raylib.h"


void CcOverlayLoadBodyFont(const char *path);
void CcOverlayUnloadBodyFont(void);
void CcOverlayDrawBodyText(const char *text, int x, int y, int font_size, Color color);
int CcOverlayMeasureBodyText(const char *text, int font_size);
void CcOverlayBegin(float text_scale);
void CcOverlayFlush(void);
void CcOverlayEnd(void);
void CcOverlaySetTextObserver(void (*observer)(const char *text));
void CcOverlayDrawText(const char *text, int x, int y, int font_size,
                       Color color);
int CcOverlayMeasureText(const char *text, int font_size);

#endif
