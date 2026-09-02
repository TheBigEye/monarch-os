/**
 * @file manager.c
 * @brief Wing window collection and z-order object.
 */

#include "manager.h"
#include "input.h"

static int add(struct WindowManager *, const struct Window *);
static int remove(struct WindowManager *, uint32_t);
static struct Window *get(struct WindowManager *, uint32_t);
static Window *find_handle(struct WindowManager *, uint32_t);
static int attach_top(struct WindowManager *, Window *);
static Window *hit(struct WindowManager *, int32_t, int32_t);
static Window *focus_at(struct WindowManager *, int32_t, int32_t, struct dirty *);
static Window *bring_window_front(struct WindowManager *, Window *, struct dirty *);
static int focus(struct WindowManager *, int32_t, int32_t, struct dirty *);
static int focus_window(struct WindowManager *, Window *, struct dirty *);
static int bring_front(struct WindowManager *, int, struct dirty *);
static int begin_drag(struct WindowManager *, Window *, int32_t, int32_t);
static void update_drag(struct WindowManager *, int32_t, int32_t, struct dirty *);
static void end_drag(struct WindowManager *);
static void release_buttons(struct WindowManager *, struct dirty *);
static void sanitize_state(struct WindowManager *);
static void detach_window(Window *window);
static void WindowManager_emit_event(Window *, uint32_t);
static void update_hover(struct WindowManager *, int32_t, int32_t, struct dirty *);
static Window *press(struct WindowManager *, int32_t, int32_t, struct dirty *);
static int key(struct WindowManager *, uint32_t);
static void recover(struct WindowManager *, struct dirty *);
static void event(struct WindowManager *, uint32_t, void *);
static void paint(struct WindowManager *, struct renderer *);
static int tick(struct WindowManager *, struct dirty *);
static void destroy_children(Window *parent, struct dirty *dirty);

static void detach_window(Window *window) {
    Window *parent;
    if (!window || !(parent = window->parent)) return;
    if (window->prev_sibling) window->prev_sibling->next_sibling = window->next_sibling;
    else if (parent->first_child == window) parent->first_child = window->next_sibling;
    if (window->next_sibling) window->next_sibling->prev_sibling = window->prev_sibling;
    window->parent = nil;
    window->next_sibling = nil;
    window->prev_sibling = nil;
}

static void WindowManager_emit_event(Window *window, uint32_t type) {
    WindowEvent event;
    if (!window || !window->on_event) return;
    event.type = type;
    event.x = window->frame.x;
    event.y = window->frame.y;
    event.width = window->frame.w;
    event.height = window->frame.h;
    window->on_event(window, (int)type, &event);
}

void WindowManager_init(struct WindowManager *self, struct Window *windows,
                       uint32_t count, struct Cursor *cursor) {
    if (!self) return;
    memset(self, 0, sizeof(*self));
    self->windows = windows;
    memset(&self->root, 0, sizeof(self->root));
    self->root.first_child = count ? &windows[0] : nil;
    for (uint32_t i = 0; i < count && i < WING_MANAGER_MAX; i++) {
        windows[i].parent = &self->root;
        windows[i].prev_sibling = i ? &windows[i - 1u] : nil;
        windows[i].next_sibling = (i + 1u < count) ? &windows[i + 1u] : nil;
    }
    self->count = count > WING_MANAGER_MAX ? WING_MANAGER_MAX : count;
    self->capacity = WING_MANAGER_MAX;
    self->cursor = cursor;
    self->active = -1;
    self->add = add;
    self->remove = remove;
    self->get = get;
    self->find = find_handle;
    self->attach_top = attach_top;
    self->hit = hit;
    self->focus_at = focus_at;
    self->bring_window_front = bring_window_front;
    self->focus = focus;
    self->focus_window = focus_window;
    self->bring_front = bring_front;
    self->begin_drag = begin_drag;
    self->update_drag = update_drag;
    self->end_drag = end_drag;
    self->release_buttons = release_buttons;
    self->update_hover = update_hover;
    self->press = press;
    self->key = key;
    self->recover = recover;
    self->event = event;
    self->paint = paint;
    self->tick = tick;
}
static int add(struct WindowManager *self, const struct Window *window) {
    if (!self || !self->windows || !window || self->count >= self->capacity) return -1;
    self->windows[self->count] = *window;
    {
        Window *added = &self->windows[self->count];
        Window *last = self->root.first_child;
        added->parent = &self->root;
        added->next_sibling = nil;
        if (!last) self->root.first_child = added;
        else { while (last->next_sibling) last = last->next_sibling; last->next_sibling = added; added->prev_sibling = last; }
    }
    return (int)self->count++;
}
/*
 * remove() compacts the array by shifting elements down by one slot, which
 * changes the memory address of every window past `index`. Windows are
 * linked into a real tree (see Window_add_child / WIN_CREATE_CHILD in
 * apps/sys/wing.c: a child's `parent` can be any other slot in this same
 * array, not just the root), so every pointer anywhere in the manager that
 * referenced the old address has to be repointed at the new one, not just
 * the root-level siblings.
 */
static void repoint(struct WindowManager *self, Window *old, Window *replacement) {
    if (!self || !self->windows || old == replacement) return;
    if (self->root.first_child == old) self->root.first_child = replacement;
    if (self->active_window == old) self->active_window = replacement;
    if (self->hovered_window == old) self->hovered_window = replacement;
    if (self->drag_window == old) self->drag_window = replacement;
    if (self->pending_window == old) self->pending_window = replacement;
    for (uint32_t i = 0; i < self->count; i++) {
        Window *w = &self->windows[i];
        if (w == old) continue; /* old's own fields move with it verbatim */
        if (w->parent == old) w->parent = replacement;
        if (w->first_child == old) w->first_child = replacement;
        if (w->prev_sibling == old) w->prev_sibling = replacement;
        if (w->next_sibling == old) w->next_sibling = replacement;
    }
}

static int remove(struct WindowManager *self, uint32_t index) {
    if (!self || !self->windows || index >= self->count) return 0;
    /* A removed window's children would otherwise be left pointing at a
       parent that no longer exists in the tree; close them too, matching
       what the real close path (tick()/press()) already does. */
    destroy_children(&self->windows[index], nil);
    detach_window(&self->windows[index]);
    for (uint32_t i = index; i + 1u < self->count; i++) {
        repoint(self, &self->windows[i + 1u], &self->windows[i]);
        self->windows[i] = self->windows[i + 1u];
    }
    self->count--;
    if (self->active == (int)index) self->active = -1;
    return 1;
}

struct Window *get(struct WindowManager *self, uint32_t index) {
    if (!self || !self->windows || index >= self->count) return nil;
    return &self->windows[index];
}
static Window *find_hit(Window *node, int32_t x, int32_t y) {
    Window *last;
    if (!node) return nil;
    last = node->first_child;
    while (last && last->next_sibling) last = last->next_sibling;
    for (Window *it = last; it; it = it->prev_sibling) {
        Window *child;
        if (!it->visible || !it->is_visible) continue;
        child = find_hit(it, x, y);
        if (child) return child;
        if (it->hit(it, x, y)) return it;
    }
    return nil;
}

static Window *find_handle(struct WindowManager *self, uint32_t handle) {
    if (!self || !handle) return nil;
    for (uint32_t i = 0; i < self->count; i++)
        if ((uint32_t)self->windows[i].id == handle && self->windows[i].id)
            return &self->windows[i];
    return nil;
}

static int attach_top(struct WindowManager *self, Window *window) {
    if (!self || !window || window->parent) return 0;
    window->parent = &self->root;
    window->prev_sibling = nil;
    window->next_sibling = nil;
    /* Use the same z-order policy as later focus operations. A newly created
       normal window must remain below an existing always-on-top taskbar. */
    return window->bring_to_front(window);
}

static Window *hit(struct WindowManager *self, int32_t x, int32_t y) {
    return self ? find_hit(&self->root, x, y) : nil;
}

static void sanitize_state(struct WindowManager *self) {
    if (!self) return;
    if (self->active_window && !self->active_window->visible) {
        self->active_window = nil;
        self->active = -1;
    }
    if (self->hovered_window && !self->hovered_window->visible)
        self->hovered_window = nil;
    if (self->drag_window && !self->drag_window->visible) {
        self->drag_window = nil;
        self->drag = -1;
        self->resize_edges = 0;
    }
}

static Window *top_visible(Window *node) {
    Window *result = nil;
    if (!node) return nil;
    for (Window *it = node->first_child; it; it = it->next_sibling) {
        if (it->visible && !(it->flags & WINDOW_ALWAYS_ON_TOP)) result = it;
        {
            Window *child = top_visible(it);
            if (child) result = child;
        }
    }
    return result;
}

static void activate_top(struct WindowManager *self, struct dirty *dirty) {
    Window *target;
    if (!self || !self->windows) return;
    target = top_visible(&self->root);
    for (uint32_t i = 0; i < self->count; i++) {
        Window *window = &self->windows[i];
        int active = window == target;
        if (window->active != active && dirty) dirty_add(dirty, window->frame);
        if (window->active != active) {
            if (active) window->focus(window);
            else window->blur(window);
            WindowManager_emit_event(window, active ? EVENT_FOCUS : EVENT_BLUR);
        }
    }
    self->active_window = target;
    self->active = -1;
    for (uint32_t i = 0; i < self->count; i++)
        if (&self->windows[i] == target) self->active = (int)i;
}

static void recover(struct WindowManager *self, struct dirty *dirty) {
    if (!self) return;
    self->active_window = nil;
    self->hovered_window = nil;
    self->drag_window = nil;
    self->active = -1;
    self->drag = -1;
    self->resize_edges = 0;
    activate_top(self, dirty);
}

static Window *focus_at(struct WindowManager *self, int32_t x, int32_t y, struct dirty *dirty) {
    sanitize_state(self);
    if (!self || !self->focus) return nil;
    self->focus(self, x, y, dirty);
    return self->active_window;
}

static Window *bring_window_front(struct WindowManager *self, Window *window, struct dirty *dirty) {
    if (!self || !window || !self->bring_front) return nil;
    for (uint32_t i = 0; i < self->count; i++) {
        if (&self->windows[i] == window) {
            self->bring_front(self, (int)i, dirty);
            return window;
        }
    }
    return nil;
}

static int focus_window(struct WindowManager *self, Window *target,
                        struct dirty *dirty) {
    Window *old;
    int selected = -1;
    if (!self || !self->windows) return -1;
    sanitize_state(self);
    if (target && (!target->visible || !target->is_visible ||
                   (target->flags & WINDOW_ALWAYS_ON_TOP))) target = nil;
    old = self->active_window;
    if (old != target) {
        if (old) {
            if (dirty) dirty_add(dirty, old->frame);
            old->blur(old);
            WindowManager_emit_event(old, EVENT_BLUR);
        }
        if (target) {
            if (dirty) dirty_add(dirty, target->frame);
            target->focus(target);
            WindowManager_emit_event(target, EVENT_FOCUS);
        }
    }
    for (uint32_t i = 0; i < self->count; i++)
        if (&self->windows[i] == target) { selected = (int)i; break; }
    self->active = selected;
    self->active_window = target;
    return selected;
}

static int focus(struct WindowManager *self, int32_t x, int32_t y,
                 struct dirty *dirty) {
    if (!self || !self->windows) return -1;
    return focus_window(self, self->hit(self, x, y), dirty);
}
static int bring_front(struct WindowManager *self, int index,
                             struct dirty *dirty) {
    Window *window;
    if (!self || !self->windows || index < 0 || (uint32_t)index >= self->count) return -1;
    window = &self->windows[index];
    window->bring_to_front(window);
    if (dirty) dirty_add(dirty, window->frame);
    self->active = index;
    self->active_window = window;
    return index;
}
static int begin_drag(struct WindowManager *self, Window *window, int32_t x, int32_t y) {
    int index = -1;
    if (!self || !self->windows || !window) return 0;
    for (uint32_t i = 0; i < self->count; i++) if (&self->windows[i] == window) index = (int)i;
    if (index < 0) return 0;
    self->drag_window = nil;
    self->drag = -1;
    self->resize_edges = (window->flags & WINDOW_NO_RESIZE)
        ? 0u : window->resize_hit(window, x, y);
    if (self->resize_edges) {
        self->drag = index;
        self->drag_window = window;
        self->drag_x = x;
        self->drag_y = y;
    } else if (!(window->flags & WINDOW_NO_MOVE) && window->title_hit(window, x, y)) {
        self->drag = index;
        self->drag_window = window;
        self->drag_x = x - window->frame.x;
        self->drag_y = y - window->frame.y;
    }
    return self->drag >= 0;
}
static void update_drag(struct WindowManager *self, int32_t x, int32_t y, struct dirty *dirty) {
    struct Window *window;
    struct rect old;
    int32_t nx;
    int32_t ny;
    int32_t nw;
    int32_t nh;
    if (!self || !self->drag_window || !self->windows) return;
    window = self->drag_window;
    old = window->frame;
    if (!self->resize_edges) {
        window->move(window, x - self->drag_x, y - self->drag_y);
    } else {
        nx = old.x; ny = old.y; nw = (int32_t)old.w; nh = (int32_t)old.h;
        if (self->resize_edges & WING_RESIZE_LEFT) { nx = x; nw = old.x + (int32_t)old.w - x; }
        if (self->resize_edges & WING_RESIZE_RIGHT) nw = x - old.x;
        if (self->resize_edges & WING_RESIZE_TOP) { ny = y; nh = old.y + (int32_t)old.h - y; }
        if (self->resize_edges & WING_RESIZE_BOTTOM) nh = y - old.y;
        if (nw < 140) { if (self->resize_edges & WING_RESIZE_LEFT) nx = old.x + (int32_t)old.w - 140; nw = 140; }
        if (nh < 80) { if (self->resize_edges & WING_RESIZE_TOP) ny = old.y + (int32_t)old.h - 80; nh = 80; }
        window->resize(window, nx, ny, (uint32_t)nw, (uint32_t)nh);
    }
    if (dirty) { dirty_add(dirty, old); dirty_add(dirty, window->frame); }
}
static void end_drag(struct WindowManager *self) {
    if (!self) return;
    self->drag = -1;
    self->drag_window = nil;
    self->resize_edges = 0;
}
static int clip_intersection(struct rect a, struct rect b, struct rect *out) {
    int32_t left = a.x > b.x ? a.x : b.x;
    int32_t top = a.y > b.y ? a.y : b.y;
    int32_t right = a.x + (int32_t)a.w < b.x + (int32_t)b.w
        ? a.x + (int32_t)a.w : b.x + (int32_t)b.w;
    int32_t bottom = a.y + (int32_t)a.h < b.y + (int32_t)b.h
        ? a.y + (int32_t)a.h : b.y + (int32_t)b.h;
    if (right <= left || bottom <= top) return 0;
    if (out) *out = rect_make(left, top, (uint32_t)(right - left),
                              (uint32_t)(bottom - top));
    return 1;
}

static void paint_tree(Window *node, struct renderer *renderer) {
    struct rect saved_clip;
    int saved;
    if (!node || !renderer) return;
    saved = renderer->clipped;
    saved_clip = renderer->cliprect;

    /* A child is clipped to its parent's frame. This prevents pixels from a
       child that extends outside its parent from becoming stale trails when
       the parent moves. */
    if (node->parent) {
        struct rect parent_clip;
        if (saved) {
            if (!clip_intersection(saved_clip, node->frame, &parent_clip)) return;
        } else {
            parent_clip = node->frame;
        }
        renderer->clip(renderer, parent_clip.x, parent_clip.y,
                       parent_clip.w, parent_clip.h);
    }

    for (Window *it = node->first_child; it; it = it->next_sibling) {
        if (!it->visible || !it->is_visible) continue;
        if (!renderer->clipped || rect_intersect(it->frame, renderer->cliprect))
            it->render(it, renderer);
        paint_tree(it, renderer);
    }

    if (saved) renderer->clip(renderer, saved_clip.x, saved_clip.y,
                              saved_clip.w, saved_clip.h);
    else renderer->no_clip(renderer);
}

static void release_buttons(struct WindowManager *self, struct dirty *dirty) {
    int closed = 0;
    if (!self || !self->windows) return;
    for (uint32_t i = 0; i < self->count; i++) {
        Window *window = &self->windows[i];
        if (window->close_button.pressed) {
            if (self->pending_window == window) continue;
            if (dirty) dirty_add(dirty, window->frame);
            window->close_button.pressed = 0;
            window->cache_valid = 0;
            window->close(window);
            detach_window(window);
            if (self->active_window == window) self->active_window = nil;
            closed = 1;
            continue;
        }
        if (window->maximize_button.pressed || window->minimize_button.pressed) {
            if (self->pending_window == window) continue;
            if (dirty) dirty_add(dirty, window->frame);
            window->maximize_button.pressed = 0;
            window->minimize_button.pressed = 0;
            window->cache_valid = 0;
        }
    }
    if (closed) {
        self->drag_window = nil;
        self->drag = -1;
        self->resize_edges = 0;
        activate_top(self, dirty);
    }
}

static void update_hover(struct WindowManager *self, int32_t x, int32_t y, struct dirty *dirty) {
    Window *top;
    Window *old;
    if (!self || !self->windows) return;
    sanitize_state(self);
    top = self->hit(self, x, y);
    old = self->hovered_window;
    if (old != top) {
        if (old) {
            if (old->close_button.hover && dirty) dirty_add(dirty, old->close_button.frame);
            if (old->maximize_button.hover && dirty) dirty_add(dirty, old->maximize_button.frame);
            if (old->minimize_button.hover && dirty) dirty_add(dirty, old->minimize_button.frame);
            old->close_button.hover = 0;
            old->maximize_button.hover = 0;
            old->minimize_button.hover = 0;
            old->cache_valid = 0;
        }
        WindowManager_emit_event(old, EVENT_MOUSE_LEAVE);
        WindowManager_emit_event(top, EVENT_MOUSE_ENTER);
        self->hovered_window = top;
    }
    if (top) {
        int close_hover = top->close_button.hit(&top->close_button, x, y);
        int max_hover = top->maximize_button.hit(&top->maximize_button, x, y);
        int min_hover = top->minimize_button.hit(&top->minimize_button, x, y);
        if (top->close_button.hover != close_hover && dirty) dirty_add(dirty, top->close_button.frame);
        if (top->maximize_button.hover != max_hover && dirty) dirty_add(dirty, top->maximize_button.frame);
        if (top->minimize_button.hover != min_hover && dirty) dirty_add(dirty, top->minimize_button.frame);
        if (top->close_button.hover != close_hover || top->maximize_button.hover != max_hover || top->minimize_button.hover != min_hover)
            top->cache_valid = 0;
        top->close_button.hover = close_hover;
        top->maximize_button.hover = max_hover;
        top->minimize_button.hover = min_hover;
    }
}
static Window *press(struct WindowManager *self, int32_t x, int32_t y, struct dirty *dirty) {
    Window *window;
    if (!self) return nil;
    sanitize_state(self);
    window = self->focus_at(self, x, y, dirty);
    if (!window) return nil;
    self->bring_window_front(self, window, dirty);
    dirty_add(dirty, window->frame);
    if (window->close_button.hit(&window->close_button, x, y)) {
        window->close_button.pressed = 1;
        window->cache_valid = 0;
        /* Close is terminal and must not depend on a later mouse-up packet.
           Some PS/2 paths coalesce button packets, so waiting for release can
           leave the close button visibly pressed forever. Commit now, then
           clear the visual state before detaching the object. */
        window->close(window);
        window->close_button.pressed = 0;
        detach_window(window);
        if (self->active_window == window) self->active_window = nil;
        self->drag_window = nil;
        self->drag = -1;
        self->resize_edges = 0;
        dirty_add(dirty, window->frame);
        activate_top(self, dirty);
        return nil;
    }
    if (window->minimize_button.hit(&window->minimize_button, x, y)) {
        window->minimize_button.pressed = 1;
        window->cache_valid = 0;
        window->minimize(window);
        if (self->active_window == window) self->active_window = nil;
        self->drag_window = nil;
        self->drag = -1;
        dirty_add(dirty, window->frame);
        activate_top(self, dirty);
        return nil;
    }
    if (window->maximize_button.hit(&window->maximize_button, x, y)) {
        window->maximize_button.pressed = 1;
        window->cache_valid = 0;
        self->pending_window = window;
        self->pending_action = EVENT_MAXIMIZE;
        self->pending_ticks = 1;
        dirty_add(dirty, window->frame);
        return nil;
    }
    return window;
}

static void event(struct WindowManager *self, uint32_t type, void *data) {
    if (!self || !self->active_window || !self->active_window->on_event) return;
    self->active_window->on_event(self->active_window, (int)type, data);
}

static int key(struct WindowManager *self, uint32_t key) {
    KeyEvent event;
    if (!self) return 0;
    event.key = key;
    event.pressed = 1;
    if (self->active_window && self->active_window->on_event) {
        self->active_window->on_event(self->active_window, EVENT_KEY_DOWN, &event);
    }
    return key == 27u ? -1 : 1;
}

static void paint(struct WindowManager *self, struct renderer *renderer) {
    struct rect cursor;
    if (!self || !renderer) return;
    paint_tree(&self->root, renderer);
    if (!self->cursor || !self->cursor->sprite || !self->cursor->sprite->_surface)
        return;
    cursor = rect_make(self->cursor->x, self->cursor->y,
                       self->cursor->sprite->_surface->width,
                       self->cursor->sprite->_surface->height);
    if (!renderer->clipped || rect_intersect(cursor, renderer->cliprect))
        self->cursor->paint(self->cursor, renderer);
}

static void destroy_children(Window *parent, struct dirty *dirty) {
    Window *child = parent ? parent->first_child : nil;
    while (child) {
        Window *next = child->next_sibling;
        if (dirty) dirty_add(dirty, child->frame);
        destroy_children(child, dirty);
        child->destroy(child);
        child->id = 0;
        child->owner = nil;
        child = next;
    }
}

static int tick(struct WindowManager *self, struct dirty *dirty) {
    Window *window;
    struct rect old;
    if (!self || !self->pending_window) return 0;
    if (self->pending_ticks) { self->pending_ticks--; return 0; }
    window = self->pending_window;
    self->pending_window = nil;
    old = window->frame;
    window->close_button.pressed = 0;
    window->minimize_button.pressed = 0;
    window->maximize_button.pressed = 0;
    if (self->pending_action == EVENT_CLOSE) {
        destroy_children(window, dirty);
        window->close(window);
        detach_window(window);
        if (self->active_window == window) self->active_window = nil;
        self->drag_window = nil;
        self->drag = -1;
        self->resize_edges = 0;
    } else if (self->pending_action == EVENT_MINIMIZE) {
        window->minimize(window);
    } else if (self->pending_action == EVENT_MAXIMIZE) {
        window->maximize(window);
    }
    self->pending_action = EVENT_NONE;
    if (dirty) {
        dirty_add(dirty, old);
        dirty_add(dirty, window->frame);
    }
    if (!window->visible) activate_top(self, dirty);
    return 1;
}
