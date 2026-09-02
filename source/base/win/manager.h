#ifndef MONARCH_APPS_GUI_MANAGER_H
#define MONARCH_APPS_GUI_MANAGER_H 1

#include "window.h"
#include "cursor.h"
#include "base/gfx/dirty.h"

#define WING_MANAGER_MAX 64u

typedef struct WindowManager WindowManager;

struct WindowManager {
    /* Stable references and tree root are kept together. */
    Window *windows;
    Window *active_window;
    Window *hovered_window;
    Window *drag_window;
    struct Cursor *cursor;
    Window root;

    /* Manager methods form the vtable and are initialized by WindowManager_init. */
    int (*add)(WindowManager *self, const Window *window);
    int (*remove)(WindowManager *self, uint32_t index);
    Window *(*get)(WindowManager *self, uint32_t index);
    Window *(*find)(WindowManager *self, uint32_t handle);
    int (*attach_top)(WindowManager *self, Window *window);
    Window *(*hit)(WindowManager *self, int32_t x, int32_t y);
    Window *(*focus_at)(WindowManager *self, int32_t x, int32_t y, struct dirty *dirty);
    Window *(*bring_window_front)(WindowManager *self, Window *window, struct dirty *dirty);
    int (*focus)(WindowManager *self, int32_t x, int32_t y, struct dirty *dirty);
    int (*focus_window)(WindowManager *self, Window *window, struct dirty *dirty);
    int (*bring_front)(WindowManager *self, int index, struct dirty *dirty);
    int (*begin_drag)(WindowManager *self, Window *window, int32_t x, int32_t y);
    void (*update_drag)(WindowManager *self, int32_t x, int32_t y, struct dirty *dirty);
    void (*end_drag)(WindowManager *self);
    void (*release_buttons)(WindowManager *self, struct dirty *dirty);
    void (*update_hover)(WindowManager *self, int32_t x, int32_t y, struct dirty *dirty);
    Window *(*press)(WindowManager *self, int32_t x, int32_t y, struct dirty *dirty);
    int (*key)(WindowManager *self, uint32_t key);
    void (*recover)(WindowManager *self, struct dirty *dirty);
    void (*event)(WindowManager *self, uint32_t type, void *data);
    void (*paint)(WindowManager *self, struct renderer *renderer);
    int (*tick)(WindowManager *self, struct dirty *dirty);

    Window *pending_window;
    uint32_t pending_action;
    uint32_t pending_ticks;
    uint32_t count;
    uint32_t capacity;
    uint32_t resize_edges;
    int active;
    int drag;
    int32_t drag_x;
    int32_t drag_y;
};

void WindowManager_init(WindowManager *self, Window *windows,
                        uint32_t count, Cursor *cursor);

#endif /* MONARCH_APPS_GUI_MANAGER_H */
