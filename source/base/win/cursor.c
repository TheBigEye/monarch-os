/**
 * @file cursor.c
 * @brief Wing cursor widget using the shared Boing cursor asset.
 */

#include "cursor.h"
#include "base/gfx/shape.h"

static void destroy(struct Cursor *);
static void paint(struct Cursor *, struct renderer *);

int Cursor_init(struct Cursor *cursor, int32_t x, int32_t y) {
    struct surface *surface;

    if (!cursor) return 0;
    memset(cursor, 0, sizeof(*cursor));
    cursor->paint = paint;
    cursor->destroy = destroy;
    cursor->x = x;
    cursor->y = y;
    surface = shape_cursor();
    if (!surface) return 0;
    cursor->sprite = sprite_create(surface, 1);
    if (!cursor->sprite) {
        surface_destroy(surface);
        return 0;
    }
    cursor->sprite->key(cursor->sprite, 0x000000u, 1);
    return 1;
}
static void destroy(struct Cursor *cursor) {
    if (cursor && cursor->sprite) {
        cursor->sprite->destroy(cursor->sprite);
        cursor->sprite = nil;
    }
}
static void paint(struct Cursor *cursor, struct renderer *renderer) {
    if (cursor && cursor->sprite && renderer) {
        cursor->sprite->draw(cursor->sprite, renderer, (uint32_t)cursor->x, (uint32_t)cursor->y);
    }
}
