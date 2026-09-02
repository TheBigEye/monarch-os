/**
 * @file window.c
 * @brief Wing top-level window widget.
 */

#include "window.h"
#include "assets.h"
#include "label.h"
#include "input.h"
#include "terminal.h"

static int init(Window *, int, int, int, int, int);
static int add_child(Window *, Window *);
static int remove_child(Window *, Window *);
static int bring_to_front(Window *);
static int send_to_back(Window *);
static int focus(Window *);
static int blur(Window *);
static int show(Window *);
static int hide(Window *);
static int set_title(Window *, const char *);
static int set_widget(Window *, void *);
static int set_background(Window *, uint32_t);
static int set_border(Window *, uint32_t, int);
static int set_opacity(Window *, float);
static int set_flags(Window *, int);
static int shown(Window *);
static int child_get(Window *);
static void layout(struct Window *);
static void content_set(struct Window *, const char *, int32_t, int32_t);
static void close(struct Window *);
static void minimize(struct Window *);
static void maximize(struct Window *);
static void move(struct Window *, int32_t, int32_t);
static void resize(struct Window *, int32_t, int32_t, uint32_t, uint32_t);
static uint32_t resize_hit(struct Window *, int32_t, int32_t);
static int hit(struct Window *, int32_t, int32_t);
static int title_hit(struct Window *, int32_t, int32_t);
static void paint(struct Window *, struct renderer *);
static int destroy(Window *);
static int render(Window *, struct renderer *);
static int update(Window *, struct dirty *);

static char *copy_text(const char *text) {
    size_t length;
    char *copy;
    if (!text) text = "";
    length = strlen(text) + 1u;
    copy = gfx_alloc(length);
    if (!copy) return nil;
    memcpy(copy, text, length);
    return copy;
}

static void Window_emit_event(Window *self, uint32_t type) {
    WindowEvent event;
    if (!self || !self->on_event) return;
    event.type = type;
    event.x = self->frame.x;
    event.y = self->frame.y;
    event.width = self->frame.w;
    event.height = self->frame.h;
    self->on_event(self, (int)type, &event);
}

static void draw_rect(struct renderer *renderer, int32_t x, int32_t y,
                      uint32_t w, uint32_t h, uint32_t color) {
    int32_t x1 = x + (int32_t)w;
    int32_t y1 = y + (int32_t)h;
    if (!renderer || !renderer->target || !w || !h) return;
    if (x < 0) { w = x1 > 0 ? (uint32_t)x1 : 0; x = 0; }
    if (y < 0) { h = y1 > 0 ? (uint32_t)y1 : 0; y = 0; }
    if (x >= (int32_t)renderer->target->width || y >= (int32_t)renderer->target->height) return;
    if (x + (int32_t)w > (int32_t)renderer->target->width) w = renderer->target->width - (uint32_t)x;
    if (y + (int32_t)h > (int32_t)renderer->target->height) h = renderer->target->height - (uint32_t)y;
    if (w && h) renderer->rect(renderer, (uint32_t)x, (uint32_t)y, w, h, color);
}

static void layout(struct Window *self) {
    int32_t x;
    int32_t y;
    uint32_t w;
    if (!self) return;
    x = self->frame.x; y = self->frame.y; w = self->frame.w;
    self->close_button.frame = rect_make(x + (int32_t)w - 28, y + 5, 18, 14);
    self->maximize_button.frame = rect_make(x + (int32_t)w - 50, y + 5, 18, 14);
    self->minimize_button.frame = rect_make(x + (int32_t)w - 72, y + 5, 18, 14);
    self->close_button.disabled = (self->flags & (WINDOW_NO_CLOSE | WINDOW_NO_TITLE)) != 0;
    self->maximize_button.disabled = (self->flags & (WINDOW_NO_MAXIMIZE | WINDOW_NO_TITLE)) != 0;
    self->minimize_button.disabled = (self->flags & (WINDOW_NO_MINIMIZE | WINDOW_NO_TITLE)) != 0;
}

int Window_create(Window *self, const char *title, int x, int y,
                      uint32_t w, uint32_t h, uint32_t frame, uint32_t body, int active) {
    unused(wing_corner_mask);
    if (!self) return 0;
    memset(self, 0, sizeof(*self));
    self->init = init;
    self->add_child = add_child;
    self->remove_child = remove_child;
    self->bring_to_front = bring_to_front;
    self->send_to_back = send_to_back;
    self->focus = focus;
    self->blur = blur;
    self->show = show;
    self->hide = hide;
    self->set_title = set_title;
    self->set_widget = set_widget;
    self->set_background = set_background;
    self->set_border = set_border;
    self->set_opacity = set_opacity;
    self->set_flags = set_flags;
    self->shown = shown;
    self->child_get = child_get;
    self->destroy = destroy;
    self->render = render;
    self->update = update;
    self->paint = paint;
    self->move = move;
    self->resize = resize;
    self->layout = layout;
    self->content_set = content_set;
    self->hit = hit;
    self->title_hit = title_hit;
    self->resize_hit = resize_hit;
    self->close = close;
    self->minimize = minimize;
    self->maximize = maximize;
    if (!self->init(self, 0, x, y, (int)w, (int)h)) return 0;
    self->restore_frame = self->frame;
    self->title_storage = copy_text(title);
    self->title = self->title_storage ? self->title_storage : (title ? title : "");
    Label_init(&self->title_label, self->title);
    Label_init(&self->content_label, nil);
    self->frame_color = frame;
    self->body_color = body;
    self->active = active;
    self->is_focused = active != 0;
    self->visible = 1;
    self->is_visible = true;
    self->state = WINDOW_NORMAL;
    self->opaque = 1;
    self->cache = nil;
    self->cache_owned = 1;
    self->cache_valid = 0;
    Button_init(&self->close_button, wing_close_glyph);
    Button_init(&self->maximize_button, wing_max_glyph);
    Button_init(&self->minimize_button, wing_min_glyph);
    self->layout(self);
    return 1;
}
static int init(Window *self, int id, int x, int y, int width, int height) {
    if (!self || width <= 0 || height <= 0) return 0;
    self->id = id;
    self->x = x; self->y = y;
    self->width = width; self->height = height;
    self->frame = rect_make(x, y, (uint32_t)width, (uint32_t)height);
    return 1;
}

static int destroy(Window *self) {
    if (!self) return 0;
    if (self->parent) {
        if (self->parent->remove_child) {
            self->parent->remove_child(self->parent, self);
        } else {
            /* WindowManager uses a synthetic root without a Window vtable.
               Never assume parent->remove_child exists: unlink the node
               directly or its siblings will keep stale root references. */
            if (self->prev_sibling)
                self->prev_sibling->next_sibling = self->next_sibling;
            else if (self->parent->first_child == self)
                self->parent->first_child = self->next_sibling;
            if (self->next_sibling)
                self->next_sibling->prev_sibling = self->prev_sibling;
        }
    }
    /* Destruction is idempotent from the tree's point of view. A window can
       already have been detached by a local close before its owner socket
       disappears; never leave stale sibling links into the root z-list. */
    self->parent = nil;
    self->next_sibling = nil;
    self->prev_sibling = nil;
    self->visible = 0;
    self->is_visible = false;
    self->state = WINDOW_DESTROYED_STATE;
    self->active = 0;
    self->is_focused = false;
    if (self->cache && self->cache_owned) surface_destroy(self->cache);
    self->cache = nil;
    gfx_free(self->title_storage);
    gfx_free(self->content_storage);
    self->title_storage = nil;
    self->content_storage = nil;
    self->cache_valid = 0;
    return 1;
}

static int render(Window *self, struct renderer *renderer) {
    struct renderer cached;
    struct rect frame;
    int x;
    int y;
    int width;
    int height;

    if (!self || !renderer || !renderer->target) return 0;
    if (!self->visible || !self->is_visible) return 1;
    if (self->on_render) self->on_render(self);

    if (!self->opaque || self->frame.x < 0 || self->frame.y < 0) {
        self->paint(self, renderer);
        return 1;
    }

    if (!self->cache || self->cache->width != self->frame.w ||
        self->cache->height != self->frame.h) {
        if (self->cache && !self->cache_owned) {
            /* External caches are owned by the compositor and cannot be
               resized here. Fall back to direct painting for this geometry. */
            self->cache_valid = 0;
        } else {
            struct pixel_format format;
            int native = 0;
            if (self->cache) {
                format = self->cache->pixel;
                native = self->cache->format == SURFACE_NATIVE32;
                surface_destroy(self->cache);
            }
            self->cache = native ? surface_native(self->frame.w, self->frame.h, &format)
                                 : surface_create(self->frame.w, self->frame.h);
            self->cache_owned = 1;
            self->cache_valid = 0;
        }
    }
    if (!self->cache || self->cache->width != self->frame.w ||
        self->cache->height != self->frame.h) {
        self->paint(self, renderer);
        return 1;
    }

    if (!self->cache_valid) {
        frame = self->frame;
        x = self->x; y = self->y;
        width = self->width; height = self->height;
        self->frame = rect_make(0, 0, frame.w, frame.h);
        self->x = 0; self->y = 0;
        self->width = (int)frame.w; self->height = (int)frame.h;
        self->layout(self);
        render_target(&cached, self->cache);
        self->paint(self, &cached);
        self->frame = frame;
        self->x = x; self->y = y;
        self->width = width; self->height = height;
        self->layout(self);
        self->cache_valid = 1;
    }

    renderer->blit(renderer, self->cache, (uint32_t)self->frame.x,
                   (uint32_t)self->frame.y);
    return 1;
}

static int update(Window *self, struct dirty *dirty) {
    if (!self) return 0;
    if (self->on_update) self->on_update(self, dirty);
    return 1;
}

static int add_child(Window *self, Window *child) {
    if (!self || !child || child == self || child->parent) return 0;
    child->parent = self;
    child->prev_sibling = nil;
    child->next_sibling = self->first_child;
    if (self->first_child) self->first_child->prev_sibling = child;
    self->first_child = child;
    return 1;
}
static int remove_child(Window *self, Window *child) {
    if (!self || !child || child->parent != self) return 0;
    if (child->prev_sibling) child->prev_sibling->next_sibling = child->next_sibling;
    else self->first_child = child->next_sibling;
    if (child->next_sibling) child->next_sibling->prev_sibling = child->prev_sibling;
    child->parent = nil; child->next_sibling = nil; child->prev_sibling = nil;
    return 1;
}
static int bring_to_front(Window *self) {
    Window *parent;
    Window *it;
    Window *topmost;
    if (!self || !self->parent) return 0;
    parent = self->parent;

    if (self->prev_sibling)
        self->prev_sibling->next_sibling = self->next_sibling;
    else if (parent->first_child == self)
        parent->first_child = self->next_sibling;
    if (self->next_sibling)
        self->next_sibling->prev_sibling = self->prev_sibling;
    self->prev_sibling = nil;
    self->next_sibling = nil;

    if (self->flags & WINDOW_ALWAYS_ON_TOP) {
        it = parent->first_child;
        if (!it) {
            parent->first_child = self;
            return 1;
        }
        while (it->next_sibling) it = it->next_sibling;
        it->next_sibling = self;
        self->prev_sibling = it;
        return 1;
    }

    /* Ordinary windows are placed immediately before the topmost group. */
    topmost = nil;
    for (it = parent->first_child; it; it = it->next_sibling) {
        if (it->flags & WINDOW_ALWAYS_ON_TOP) {
            topmost = it;
            break;
        }
    }
    if (topmost) {
        self->next_sibling = topmost;
        self->prev_sibling = topmost->prev_sibling;
        if (topmost->prev_sibling) topmost->prev_sibling->next_sibling = self;
        else parent->first_child = self;
        topmost->prev_sibling = self;
    } else {
        it = parent->first_child;
        if (!it) {
            parent->first_child = self;
            return 1;
        }
        while (it->next_sibling) it = it->next_sibling;
        it->next_sibling = self;
        self->prev_sibling = it;
    }
    return 1;
}
static int send_to_back(Window *self) {
    Window *parent;
    if (!self || !self->parent) return 0;
    parent = self->parent;
    if (!self->prev_sibling) return 1;
    if (self->prev_sibling) self->prev_sibling->next_sibling = self->next_sibling;
    if (self->next_sibling) self->next_sibling->prev_sibling = self->prev_sibling;
    self->prev_sibling = nil;
    self->next_sibling = parent->first_child;
    if (parent->first_child) parent->first_child->prev_sibling = self;
    parent->first_child = self;
    return 1;
}
static int focus(Window *self) { if (!self) return 0; self->is_focused = true; self->active = 1; self->cache_valid = 0; if (self->on_focus) self->on_focus(self); return 1; }
static int blur(Window *self) { if (!self) return 0; self->is_focused = false; self->active = 0; self->cache_valid = 0; return 1; }
static int show(Window *self) { if (!self) return 0; self->visible = 1; self->is_visible = true; self->state = WINDOW_NORMAL; return 1; }
static int hide(Window *self) { if (!self) return 0; self->visible = 0; self->is_visible = false; self->state = WINDOW_HIDDEN_STATE; return 1; }
static int set_title(Window *self, const char *title) {
    char *copy;
    if (!self) return 0;
    copy = copy_text(title);
    if (!copy) return 0;
    gfx_free(self->title_storage);
    self->title_storage = copy;
    self->title = copy;
    self->title_label.text = copy;
    self->cache_valid = 0;
    return 1;
}
static int set_widget(Window *self, void *widget) { if (!self) return 0; self->widget = widget; self->cache_valid = 0; return 1; }
static int set_background(Window *self, uint32_t color) { if (!self) return 0; self->background = color; self->body_color = color; self->cache_valid = 0; return 1; }
static int set_border(Window *self, uint32_t color, int thickness) { if (!self) return 0; self->border_color = color; self->border_thickness = thickness; self->cache_valid = 0; return 1; }
static int set_opacity(Window *self, float opacity) { if (!self) return 0; if (opacity < 0) opacity = 0; if (opacity > 1) opacity = 1; self->opacity = (uint8_t)(opacity * 255.0f); self->cache_valid = 0; return 1; }
static int shown(Window *self) {
    return self && self->visible && self->is_visible;
}

static int child_get(Window *self) {
    return self && self->parent && self->parent->parent;
}

static int set_flags(Window *self, int flags) { if (!self) return 0; self->flags = flags; self->layout(self); self->cache_valid = 0; return 1; }

static void content_set(struct Window *self, const char *text, int32_t x, int32_t y) {
    if (!self) return;
    gfx_free(self->content_storage);
    self->content_storage = copy_text(text);
    self->content = self->content_storage ? self->content_storage : "";
    self->content_label.text = self->content;
    self->content_x = x;
    self->content_y = y;
    self->cache_valid = 0;
}
static void close(struct Window *self) {
    if (!self) return;
    self->visible = 0;
    self->is_visible = false;
    self->state = WINDOW_CLOSING_STATE;
    self->active = 0;
    self->is_focused = false;
    if (self->on_close) self->on_close(self);
    Window_emit_event(self, EVENT_CLOSE);
}
static void minimize(struct Window *self) {
    if (!self) return;
    self->visible = 0;
    self->is_visible = false;
    self->minimized = 1;
    self->state = WINDOW_MINIMIZED_STATE;
    self->active = 0;
    Window_emit_event(self, EVENT_MINIMIZE);
}
static void maximize(struct Window *self) {
    if (!self) return;
    if (!self->maximized) {
        self->restore_frame = self->frame;
        if ((self->flags & WINDOW_CONSTRAINED) && self->parent) {
            self->frame = rect_make(self->parent->frame.x + 8,
                                    self->parent->frame.y + 24,
                                    self->parent->frame.w > 16u ? self->parent->frame.w - 16u : 1u,
                                    self->parent->frame.h > 32u ? self->parent->frame.h - 32u : 1u);
        } else {
            self->frame = rect_make(4, 28, 792, 544);
        }
        self->maximized = 1;
        self->state = WINDOW_MAXIMIZED_STATE;
    } else {
        self->frame = self->restore_frame;
        self->maximized = 0;
        self->state = WINDOW_NORMAL;
    }
    self->x = self->frame.x;
    self->y = self->frame.y;
    self->width = (int)self->frame.w;
    self->height = (int)self->frame.h;
    self->layout(self);
    self->cache_valid = 0;
    Window_emit_event(self, EVENT_MAXIMIZE);
}
static void move_children(Window *parent, int32_t dx, int32_t dy) {
    for (Window *child = parent ? parent->first_child : nil; child; child = child->next_sibling) {
        child->frame.x += dx;
        child->frame.y += dy;
        child->x = child->frame.x;
        child->y = child->frame.y;
        child->layout(child);
        move_children(child, dx, dy);
    }
}

static void move(struct Window *self, int32_t x, int32_t y) {
    if (!self) return;
    {
        int32_t dx = x - self->frame.x;
        int32_t dy = y - self->frame.y;
        self->x = x;
        self->y = y;
        self->frame.x = x;
        self->frame.y = y;
        self->layout(self);
        move_children(self, dx, dy);
    }
    Window_emit_event(self, EVENT_MOVE);
}
static void resize(struct Window *self, int32_t x, int32_t y, uint32_t w, uint32_t h) {
    if (!self) return;
    if (w < 140u) w = 140u;
    if (h < 80u) h = 80u;
    self->x = x; self->y = y;
    self->width = (int)w; self->height = (int)h;
    self->frame = rect_make(x, y, w, h);
    self->layout(self);
    self->cache_valid = 0;
    Window_emit_event(self, EVENT_RESIZE);
}

static uint32_t resize_hit(struct Window *self, int32_t x, int32_t y) {
    uint32_t edge = 0;
    const int32_t grip = 8;
    if (!self->hit(self, x, y)) return 0;
    if (x < self->frame.x + grip) edge |= WING_RESIZE_LEFT;
    if (x >= self->frame.x + (int32_t)self->frame.w - grip) edge |= WING_RESIZE_RIGHT;
    if (y < self->frame.y + grip) edge |= WING_RESIZE_TOP;
    if (y >= self->frame.y + (int32_t)self->frame.h - grip) edge |= WING_RESIZE_BOTTOM;
    return edge;
}
static int hit(struct Window *self, int32_t x, int32_t y) {
    return self && self->visible && x >= self->frame.x && y >= self->frame.y &&
        x < self->frame.x + (int32_t)self->frame.w &&
        y < self->frame.y + (int32_t)self->frame.h;
}
static int title_hit(struct Window *self, int32_t x, int32_t y) {
    return self->hit(self, x, y) &&
        !(self->flags & WINDOW_NO_TITLE) &&
        y < self->frame.y + 18; // The title bar is 18 pixels high.
}
static void paint(struct Window *self, struct renderer *renderer) {
    int32_t x;
    int32_t y;
    uint32_t title;

    if (!self || !renderer || !self->visible) return;
    x = self->frame.x;
    y = self->frame.y;
    title = self->active ? self->frame_color : 0x687888u;
    draw_rect(renderer, x, y, self->frame.w, self->frame.h, WING_DARK);
    draw_rect(renderer, x + 1, y + 1, self->frame.w - 2, self->frame.h - 2, WING_HILITE);
    draw_rect(renderer, x + 3, y + 3, self->frame.w - 6, self->frame.h - 6, WING_SHADOW);
    draw_rect(renderer, x + 4, y + 4, self->frame.w - 8, self->frame.h - 8, WING_FACE);
    if (!(self->flags & WINDOW_NO_TITLE))
        draw_rect(renderer, x + 4, y + 4, self->frame.w - 8, 18, title);

    {
        int32_t body_y = (self->flags & WINDOW_NO_TITLE) ? y + 4 : y + 24;
        uint32_t body_h = (self->flags & WINDOW_NO_TITLE) ? self->frame.h - 8 : self->frame.h - 28;
        draw_rect(renderer, x + 4, body_y, self->frame.w - 8, body_h, self->body_color);
    }

    if (!(self->flags & WINDOW_NO_TITLE))
        self->title_label.paint(&self->title_label, renderer, (uint32_t)(x + 8), (uint32_t)(y + 7), 0xFFFFFF, title);
    if (self->content) {
        self->content_label.paint(&self->content_label, renderer,
                                  (uint32_t)(x + self->content_x),
                                  (uint32_t)(y + self->content_y), WING_DARK, self->body_color);
    }
    if ((self->flags & WINDOW_TERMINAL) && self->widget) {
        Terminal *terminal = (Terminal *)self->widget;
        terminal->paint(terminal, renderer, x + 4, y + 24);
    }
    if (!(self->flags & WINDOW_NO_TITLE) &&
        !(self->minimize_button.disabled && self->maximize_button.disabled &&
          self->close_button.disabled)) {
        self->minimize_button.paint(&self->minimize_button, renderer);
        self->maximize_button.paint(&self->maximize_button, renderer);
        self->close_button.paint(&self->close_button, renderer);
    }
}
