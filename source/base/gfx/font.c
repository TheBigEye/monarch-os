/**
 * @file font.c
 * @brief Font object helper and access to the built-in 8x8 CP437 font used by text rendering.
 */

#include "base/gfx/font.h"

static struct font default_font;
static int ready;

void font_init(struct font *f, const void *glyphs, uint8_t width, uint8_t height) {
    f->glyphs = glyphs;
    f->width = width;
    f->height = height;
}

struct font *font_default(void) {
    if (!ready) {
        font_init(&default_font, font8x8, 8, 8);
        ready = 1;
    }
    return &default_font;
}
