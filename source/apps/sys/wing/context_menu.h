#ifndef MONARCH_SYS_WING_CONTEXT_MENU_H
#define MONARCH_SYS_WING_CONTEXT_MENU_H 1

#include "base/gfx/dirty.h"
#include "base/gfx/render.h"
#include "base/win/manager.h"

typedef struct ContextMenu ContextMenu;

struct ContextMenu {
    int visible;
    int mode;
    int32_t x;
    int32_t y;
    uint32_t w;
    uint32_t h;
    Window *target;
    struct dirty *dirty;

    void (*show)(ContextMenu *self, Window *target, int mode, int32_t x, int32_t y);
    int (*click)(ContextMenu *self, WindowManager *manager, int32_t x, int32_t y);
    void (*paint)(ContextMenu *self, struct renderer *renderer);
    void (*hide)(ContextMenu *self);
};

void ContextMenu_init(ContextMenu *self, struct dirty *dirty);

#endif /* MONARCH_SYS_WING_CONTEXT_MENU_H */
