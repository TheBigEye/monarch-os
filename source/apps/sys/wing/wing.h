#ifndef MONARCH_SYS_WING_WING_H
#define MONARCH_SYS_WING_WING_H 1

#include "base/fbd/fbd.h"
#include "base/gfx/render.h"
#include "base/gfx/surface.h"
#include "base/gfx/dirty.h"
#include "base/win/cursor.h"
#include "base/win/input.h"
#include "base/win/manager.h"
#include "apps/sys/wing/client.h"
#include "apps/sys/wing/context_menu.h"
#include "apps/sys/wing/server.h"

typedef struct Wing Wing;

/* Complete runtime state of the Wing service. */
struct Wing {
    struct fbd wing_fb;
    struct surface *wing_screen;
    struct surface *wing_background;
    struct surface *wing_wallpaper;
    struct renderer wing_renderer;
    Window *wing_windows;
    ContextMenu wing_context_menu;
    int wing_context_mode;
    struct Cursor wing_cursor;
    WindowManager wing_manager;
    WingServer wing_server;
    struct Input wing_input;
    struct Event wing_event;
    WingClient *wing_clients;
    struct dirty wing_dirty;
    Window *wing_drag_window;
    uint32_t wing_old_buttons;
    int wing_once;
    int wing_argc;
    char **wing_argv;
    int (*run)(Wing *self);
};

void Wing_init(Wing *self, int argc, char **argv);

#endif /* MONARCH_SYS_WING_WING_H */
