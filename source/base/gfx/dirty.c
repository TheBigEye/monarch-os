/**
 * @file dirty.c
 * @brief Dirty rectangle management. A dirty list tracks the screen regions that changed so demos can restore/redraw only those areas instead of repainting the full framebuffer.
 */

#include "base/gfx/dirty.h"

struct rect rect_make(int32_t x, int32_t y, uint32_t w, uint32_t h) {
    struct rect r;
    r.x = x;
    r.y = y;
    r.w = w;
    r.h = h;
    return r;
}

void dirty_init(struct dirty *list) {
    dirty_clear(list);
}

void dirty_clear(struct dirty *list) {
    if (list) {
        list->count = 0;
    }
}

int rect_intersect(struct rect a, struct rect b) {
    int32_t ar = a.x + (int32_t)a.w;
    int32_t ab = a.y + (int32_t)a.h;
    int32_t br = b.x + (int32_t)b.w;
    int32_t bb = b.y + (int32_t)b.h;

    return a.x < br && ar > b.x && a.y < bb && ab > b.y;
}

struct rect rect_union(struct rect a, struct rect b) {
    int32_t left = min(a.x, b.x);
    int32_t top = min(a.y, b.y);
    int32_t right = max(a.x + (int32_t)a.w, b.x + (int32_t)b.w);
    int32_t bottom = max(a.y + (int32_t)a.h, b.y + (int32_t)b.h);
    return rect_make(left, top, (uint32_t)(right - left), (uint32_t)(bottom - top));
}

struct rect rect_clip(struct rect r, uint32_t width, uint32_t height) {
    int32_t x0 = r.x;
    int32_t y0 = r.y;
    int32_t x1 = r.x + (int32_t)r.w;
    int32_t y1 = r.y + (int32_t)r.h;

    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > (int32_t)width) x1 = (int32_t)width;
    if (y1 > (int32_t)height) y1 = (int32_t)height;

    if (x1 <= x0 || y1 <= y0) {
        return rect_make(0, 0, 0, 0);
    }

    return rect_make(x0, y0, (uint32_t)(x1 - x0), (uint32_t)(y1 - y0));
}

void dirty_add(struct dirty *list, struct rect rect) {
    if (!list || !rect.w || !rect.h) {
        return;
    }

    /* Consume every rectangle that intersects the new one. Besides reducing
       ioctl calls, this handles transitive overlap correctly: A intersects B,
       then the union of A+B intersects C. */
    for (uint32_t i = 0; i < list->count; i++) {
        if (rect_intersect(list->rects[i], rect)) {
            rect = rect_union(list->rects[i], rect);
            for (uint32_t j = i + 1u; j < list->count; j++)
                list->rects[j - 1u] = list->rects[j];
            list->count--;
            i--;
        }
    }

    if (list->count < DIRTY_MAX) {
        list->rects[list->count++] = rect;
    } else {
        /* If we overflow, keep the first rectangle as a conservative union. */
        list->rects[0] = rect_union(list->rects[0], rect);
    }
}
