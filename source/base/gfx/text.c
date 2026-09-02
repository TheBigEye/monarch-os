/**
 * @file text.c
 * @brief Text rendering helper. It draws strings to a surface using a bitmap font and foreground/background colors.
 */

#include "base/gfx/text.h"

static void glyph(struct surface *target, struct font *fontptr, unsigned char ch, uint32_t x, uint32_t y, uint32_t fg, uint32_t bg) {
    const uint8_t *g = fontptr->glyphs + (uint32_t)ch * fontptr->height;

    for (uint32_t row = 0; row < fontptr->height; row++) {
        uint8_t bits = g[row];
        for (uint32_t col = 0; col < fontptr->width; col++) {
            surface_set(target, x + col, y + row, (bits & (0x80u >> col)) ? fg : bg);
        }
    }
}

void text_draw(struct surface *target, struct font *fontptr, const char *value, uint32_t x, uint32_t y, uint32_t fg, uint32_t bg) {
    uint32_t ox = x;

    if (!target || !fontptr || !value) {
        return;
    }

    while (*value) {
        if (*value == '\n') {
            x = ox;
            y += fontptr->height;
        } else if (*value == '\r') {
            x = ox;
        } else if (*value == '\t') {
            x += fontptr->width * 4u;
        } else {
            glyph(target, fontptr, (unsigned char)*value, x, y, fg, bg);
            x += fontptr->width;
        }
        value++;
    }
}
