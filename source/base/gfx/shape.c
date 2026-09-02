/**
 * @file shape.c
 * @brief Convenience shape surface builders. These functions create reusable surfaces for simple demo objects such as rectangles, circles, triangles, text labels, and the mouse cursor.
 */

#include "base/gfx/shape.h"
#include "base/gfx/font.h"
#include "base/gfx/text.h"

struct surface *shape_rect(uint32_t w, uint32_t h, uint32_t color) {
    struct surface *s = surface_create(w, h);
    if (s) {
        surface_fill(s, color);
    }
    return s;
}

struct surface *shape_circle(uint32_t size, uint32_t color) {
    struct surface *s = surface_create(size, size);
    int r = (int)(size / 2u);
    int rr = r * r;

    if (!s) {
        return nil;
    }

    surface_fill(s, 0x000000);
    for (int y = -r; y <= r; y++) {
        for (int x = -r; x <= r; x++) {
            if (x * x + y * y <= rr) {
                int px = r + x;
                int py = r + y;
                if (px >= 0 && py >= 0) {
                    surface_set(s, (uint32_t)px, (uint32_t)py, color);
                }
            }
        }
    }
    return s;
}

static int tri_edge(int ax, int ay, int bx, int by, int px, int py) {
    return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}

struct surface *shape_triangle(uint32_t w, uint32_t h, uint32_t color) {
    struct surface *s = surface_create(w, h);
    int x0 = (int)w / 2;
    int y0 = 0;
    int x1 = 0;
    int y1 = (int)h - 1;
    int x2 = (int)w - 1;
    int y2 = (int)h - 1;

    if (!s) {
        return nil;
    }

    surface_fill(s, 0x000000);
    for (uint32_t y = 0; y < h; y++) {
        for (uint32_t x = 0; x < w; x++) {
            int a = tri_edge(x1, y1, x2, y2, (int)x, (int)y);
            int b = tri_edge(x2, y2, x0, y0, (int)x, (int)y);
            int c = tri_edge(x0, y0, x1, y1, (int)x, (int)y);
            if ((a >= 0 && b >= 0 && c >= 0) || (a <= 0 && b <= 0 && c <= 0)) {
                surface_set(s, x, y, color);
            }
        }
    }
    return s;
}

struct surface *shape_text(const char *value, uint32_t fg, uint32_t bg) {
    struct surface *s;
    uint32_t w = (uint32_t)strlen(value) * 8u;

    if (!w) {
        w = 8;
    }

    s = surface_create(w, 8);
    if (!s) {
        return nil;
    }

    text_draw(s, font_default(), value, 0, 0, fg, bg);
    return s;
}

struct surface *shape_cursor(void) {
    static const char *map[24] = {
        "X...............",
        "XX..............",
        "XWX.............",
        "XWWX............",
        "XWWWX...........",
        "XWWWWX..........",
        "XWWWWWX.........",
        "XWWWWWWX........",
        "XWWWWWWWX.......",
        "XWWWWWWWWX......",
        "XWWWWWWWWWX.....",
        "XWWWWWWWWWWX....",
        "XWWWWWWWWWWWX...",
        "XWWWWWWXXXXXX...",
        "XWWWXWWX........",
        "XWWX.XWWX.......",
        "XWX..XWWX.......",
        "XX....XWWX......",
        "X.....XWWX......",
        "......XWWX......",
        ".......XWWX.....",
        ".......XWWX.....",
        "........XX......",
        "................",
    };
    struct surface *s = surface_create(16, 24);
    if (!s) {
        return nil;
    }
    surface_fill(s, 0x000000);
    for (uint32_t y = 0; y < 24; y++) {
        for (uint32_t x = 0; x < 16; x++) {
            if (map[y][x] == 'W') {
                surface_set(s, x, y, 0xffffff);
            } else if (map[y][x] == 'X') {
                surface_set(s, x, y, 0x000001);
            }
        }
    }
    return s;
}
