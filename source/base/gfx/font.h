#ifndef MONARCH_BASE_GFX_FONT_H
#define MONARCH_BASE_GFX_FONT_H 1

/**
 * @file font.h
 * @brief Bitmap font descriptors and built-in font data.
 *
 * Fonts in Monarch are simple fixed-size bitmaps.  Each glyph is stored as one
 * byte per row for the current 8-pixel-wide fonts.  The 8x8 font is convenient
 * for compact GFX overlays; the 8x16 font is used by the framebuffer console.
 */

#include "base/api/monarch.h"

#define FONT8X8_WIDTH   8u
#define FONT8X8_HEIGHT  8u
#define FONT8X16_WIDTH  8u
#define FONT8X16_HEIGHT 16u

extern const uint8_t font8x8[256 * FONT8X8_HEIGHT];
extern const uint8_t font8x16[256 * FONT8X16_HEIGHT];

/** Fixed-size bitmap font. */
struct font {
    /** Pointer to glyph bitmap data. */
    const uint8_t *glyphs;
    /** Glyph width in pixels. */
    uint8_t width;
    /** Glyph height in pixels. */
    uint8_t height;
};

/** Initialise a font descriptor around existing glyph data. */
void font_init(struct font *f, const void *glyphs, uint8_t width, uint8_t height);

/** Return the built-in 8x8 CP437-like font. */
struct font *font_default(void);

#endif /* MONARCH_BASE_GFX_FONT_H */
