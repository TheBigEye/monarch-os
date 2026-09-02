#ifndef MONARCH_BASE_WIN_WINDOW_H
#define MONARCH_BASE_WIN_WINDOW_H 1

#include "base/gfx/render.h"
#include "base/gfx/dirty.h"
#include "button.h"
#include "label.h"

typedef struct Window Window;

enum window_state {
    WINDOW_NORMAL = 0,
    WINDOW_MINIMIZED_STATE,
    WINDOW_MAXIMIZED_STATE,
    WINDOW_HIDDEN_STATE,
    WINDOW_CLOSING_STATE,
    WINDOW_DESTROYED_STATE
};

struct Window {
    /* Operations are grouped first so the object has one compact vtable area. */
    int (*init)(Window *self, int id, int x, int y, int width, int height);
    int (*destroy)(Window *self);
    int (*render)(Window *self, struct renderer *renderer);
    int (*update)(Window *self, struct dirty *dirty);
    int (*add_child)(Window *self, Window *child);
    int (*remove_child)(Window *self, Window *child);
    int (*bring_to_front)(Window *self);
    int (*send_to_back)(Window *self);
    int (*focus)(Window *self);
    int (*blur)(Window *self);
    int (*show)(Window *self);
    int (*hide)(Window *self);
    int (*set_title)(Window *self, const char *title);
    int (*set_widget)(Window *self, void *widget);
    int (*set_background)(Window *self, uint32_t color);
    int (*set_border)(Window *self, uint32_t color, int thickness);
    int (*set_opacity)(Window *self, float opacity);
    int (*set_flags)(Window *self, int flags);
    int (*shown)(Window *self);
    int (*child_get)(Window *self);
    void (*on_render)(Window *self);
    void (*on_update)(Window *self, struct dirty *dirty);
    void (*on_focus)(Window *self);
    void (*on_event)(Window *self, int event_type, void *event_data);
    void (*on_close)(Window *self);
    void (*paint)(Window *self, struct renderer *renderer);
    void (*move)(Window *self, int32_t x, int32_t y);
    void (*resize)(Window *self, int32_t x, int32_t y, uint32_t w, uint32_t h);
    void (*layout)(Window *self);
    void (*content_set)(Window *self, const char *text, int32_t x, int32_t y);
    int (*hit)(Window *self, int32_t x, int32_t y);
    int (*title_hit)(Window *self, int32_t x, int32_t y);
    uint32_t (*resize_hit)(Window *self, int32_t x, int32_t y);
    void (*close)(Window *self);
    void (*minimize)(Window *self);
    void (*maximize)(Window *self);

    /* References and embedded child objects are pointer-aligned as a group. */
    uint32_t *pixel_buffer;
    struct surface *cache;
    uint8_t cache_owned;
    Window *parent;
    Window *next_sibling;
    Window *prev_sibling;
    Window *first_child;
    void *widget;
    void *owner;
    const char *title;
    const char *content;
    char *title_storage;
    char *content_storage;
    struct Label title_label;
    struct Label content_label;
    struct Button close_button;
    struct Button maximize_button;
    struct Button minimize_button;

    /* Four-byte values are kept together before the small flags below. */
    struct rect frame;
    struct rect restore_frame;
    int x;
    int y;
    int width;
    int height;
    int id;
    int border_thickness;
    int flags;
    enum window_state state;
    int active;
    int visible;
    int minimized;
    int maximized;
    int32_t content_x;
    int32_t content_y;
    uint32_t background;
    uint32_t border_color;
    uint32_t frame_color;
    uint32_t body_color;
    uint8_t opacity;
    uint8_t cache_valid;
    uint8_t opaque;
    bool is_focused;
    bool is_visible;
};

/* Create a self-contained Window object and install its methods. */
int Window_create(Window *self, const char *title, int x, int y,
                  uint32_t w, uint32_t h, uint32_t frame,
                  uint32_t body, int active);

/* Window policy flags. Combine them with bitwise OR. */
#define WINDOW_NO_MINIMIZE    0x0001
#define WINDOW_NO_MAXIMIZE    0x0002
#define WINDOW_NO_CLOSE       0x0004
#define WINDOW_CONTEXT        0x0008
#define WINDOW_ALWAYS_ON_TOP  0x0010
#define WINDOW_TERMINAL       0x0020
#define WINDOW_CONSTRAINED    0x0080
#define WINDOW_NO_TITLE       0x0100
#define WINDOW_NO_MOVE        0x0200
#define WINDOW_NO_RESIZE      0x0400

#define WINDOW_NO_BUTTONS (WINDOW_NO_MINIMIZE | WINDOW_NO_MAXIMIZE | WINDOW_NO_CLOSE)

/* Edge bits returned by Window.resize_hit(). */
#define WING_RESIZE_LEFT   1u
#define WING_RESIZE_RIGHT  2u
#define WING_RESIZE_TOP    4u
#define WING_RESIZE_BOTTOM 8u

#endif /* MONARCH_BASE_WIN_WINDOW_H */
