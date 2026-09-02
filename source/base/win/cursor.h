#ifndef MONARCH_APPS_GUI_CURSOR_H
#define MONARCH_APPS_GUI_CURSOR_H 1

#include "base/gfx/render.h"
#include "base/gfx/sprite.h"

typedef struct Cursor Cursor;

struct Cursor {
    void (*paint)(Cursor *self, struct renderer *renderer);
    void (*destroy)(Cursor *self);
    struct sprite *sprite;
    int32_t x;
    int32_t y;
};

int Cursor_init(Cursor *cursor, int32_t x, int32_t y);

#endif /* MONARCH_APPS_GUI_CURSOR_H */
