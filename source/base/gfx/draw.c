/**
 * @file draw.c
 * @brief Primitive drawing helpers for linear surfaces: pixel, filled rectangle, and Bresenham-style line drawing.
 */

#include "base/gfx/draw.h"

void draw_pixel(struct surface *s, uint32_t x, uint32_t y, uint32_t color) {
    surface_set(s, x, y, color);
}

void draw_rect(struct surface *s, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    for (uint32_t row = 0; row < h; row++) {
        for (uint32_t col = 0; col < w; col++) {
            surface_set(s, x + col, y + row, color);
        }
    }
}

void draw_line(struct surface *s, int x0, int y0, int x1, int y1, uint32_t color) {
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    for (;;) {
        if (x0 >= 0 && y0 >= 0) {
            surface_set(s, (uint32_t)x0, (uint32_t)y0, color);
        }
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int e2 = err + err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}
