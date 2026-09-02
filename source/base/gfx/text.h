#ifndef MONARCH_BASE_GFX_TEXT_H
#define MONARCH_BASE_GFX_TEXT_H 1

/**
 * @file text.h
 * @brief Draw bitmap text onto linear surfaces.
 *
 * Text rendering is intentionally basic: it draws one fixed-size glyph at a
 * time, using a foreground and background color.  It is not a terminal emulator;
 * it is a helper for debug overlays, demos, and simple UI elements.
 */

#include "base/gfx/font.h"
#include "base/gfx/surface.h"

/** Draw a NUL-terminated string at pixel position `(x, y)`. */
void text_draw(struct surface *target, struct font *font, const char *value, uint32_t x, uint32_t y, uint32_t fg, uint32_t bg);

#endif /* MONARCH_BASE_GFX_TEXT_H */
