/**
 * @file wing.c
 * @brief First userspace Monarch window-manager shell.
 */

#include "base/usr/sys.h"
#include "base/fbd/fbd.h"
#include "base/gfx/render.h"
#include "base/gfx/surface.h"
#include "base/gfx/bitmap.h"
#include "base/gfx/dirty.h"
#include "base/win/protocol.h"
#include "base/win/terminal.h"
#include "base/win/window.h"
#include "base/win/cursor.h"
#include "base/win/input.h"
#include "base/win/manager.h"
#include "apps/sys/wing/config.h"
#include "apps/sys/wing/client.h"
#include "apps/sys/wing/server.h"
#include "apps/sys/wing/context_menu.h"
#include "apps/sys/wing/wing.h"

#define WING_W WING_SCREEN_WIDTH
#define WING_H WING_SCREEN_HEIGHT
#define CURSOR_W WING_CURSOR_WIDTH
#define CURSOR_H WING_CURSOR_HEIGHT
#define MOUSE_LEFT  1u
#define MOUSE_RIGHT 2u

/* Upper bound on how much a single client's outgoing message queue may grow.
   A client that stops reading (hung, crashed-but-not-yet-detected, or just
   slow) must never be allowed to grow its buffer without limit: client_send()
   backs every buffer with gfx_alloc()/sbrk(), and this compositor typically
   runs with only a few MiB of free physical memory. Without a cap, one stuck
   client can grow its queue message-by-message until the process heap (and
   the whole system's free memory) is exhausted, after which every further
   sbrk() call fails and is retried forever with nothing to show for it. 64
   queued messages is generous for a compositor tick while keeping the worst
   case bounded to a few tens of KiB. */
#define WING_CLIENT_OUTPUT_MAX (64u * (uint32_t)sizeof(WinMessage))

/* =============================================================================
 * Desktop painting helpers.
 * ============================================================================= */

private int present_rects(struct fbd *fb, struct surface *screen, struct dirty *dirty) {
    if (!fb || !screen || !dirty) return 0;
    for (uint32_t n = 0; n < dirty->count; n++) {
        struct rect r = rect_clip(dirty->rects[n], fb->width, fb->height);
        struct fb_blit blit;
        if (!r.w || !r.h) continue;
        blit.x = (uint32_t)r.x; blit.y = (uint32_t)r.y;
        blit.w = r.w; blit.h = r.h; blit.pitch = screen->pitch;
        blit.pixels = (uintptr_t)(screen->pixels + (size_t)r.y * screen->pitch + (size_t)r.x * 4u);
        if (ioctl(fb->fd, FB_BLIT, &blit) < 0) return 0;
    }
    return 1;
}

private struct rect cursor_rect(struct Cursor *cursor) {
    return rect_make(cursor->x, cursor->y, CURSOR_W, CURSOR_H);
}

private void sync_window_terminal(Window *window) {
    Terminal *terminal;
    uint32_t cols;
    uint32_t rows;
    if (!window || !(window->flags & WINDOW_TERMINAL) || !window->widget) return;
    terminal = (Terminal *)window->widget;
    cols = window->frame.w > 8u ? (window->frame.w - 8u) / 8u : 1u;
    rows = window->frame.h > 28u ? (window->frame.h - 28u) / 16u : 1u;
    if (cols < 1u) cols = 1u;
    if (rows < 1u) rows = 1u;
    if (terminal->cols != cols || terminal->rows != rows)
        terminal->resize(terminal, cols, rows);
    window->cache_valid = 0;
}

private struct surface *load_wallpaper(void) {
    struct stat info;
    long fd;
    long got;
    char *data;
    struct surface *image;

    if (stat("/initrd/share/myhill.bmp", &info) < 0 ||
        info.type != VFS_FILE || !info.size) {
        return nil;
    }

    data = gfx_alloc((size_t)info.size);
    if (!data) return nil;
    fd = open("/initrd/share/myhill.bmp", OREAD);
    if (fd < 0) {
        gfx_free(data);
        return nil;
    }
    got = 0;
    while (got < (long)info.size) {
        long count = read((int)fd, data + got, (size_t)info.size - (size_t)got);
        if (count <= 0) {
            close((int)fd);
            gfx_free(data);
            return nil;
        }
        got += count;
    }
    close((int)fd);

    image = bitmap_load(data, (size_t)info.size);
    gfx_free(data);
    if (!image || image->width != WING_W || image->height != WING_H) {
        if (image) surface_destroy(image);
        return nil;
    }
    return image;
}

private void paint_desktop(struct renderer *renderer, uint32_t width,
                           uint32_t height, struct surface *wallpaper) {
    if (wallpaper && wallpaper->width == width && wallpaper->height == height)
        renderer->blit(renderer, wallpaper, 0, 0);
    else
        renderer->clear(renderer, 0x202A36);
}

private int run(Wing *self) {
    /* SPARK passes the PTY master through inherited descriptors. Wing does not
       consume it; close the copy so it cannot keep the session alive. */
    close(3);

    /* self->wing_context_menu is a plain state object, not a constructor-backed widget.
       It must start hidden; otherwise stale stack bytes can make the first
       compositor pass paint an arbitrary rectangle or dereference garbage. */
    memset(&self->wing_context_menu, 0, sizeof(self->wing_context_menu));

    ContextMenu_init(&self->wing_context_menu, &self->wing_dirty);

    if (!self->wing_context_menu.show || !self->wing_context_menu.click ||
        !self->wing_context_menu.paint || !self->wing_context_menu.hide) {
        eputs("wing: context-menu methods are not initialized\n");
        return 1;
    }

    if (!fbd_open(&self->wing_fb) || self->wing_fb.width != WING_W || self->wing_fb.height != WING_H) {
        eputs("wing: need an 800x600 linear framebuffer\n");
        return 1;
    }
    
    Input_init(&self->wing_input);
    if (!self->wing_input.open || !self->wing_input.poll || !self->wing_input.close) {
        eputs("wing: input methods are not initialized\n");
        return 1;
    }

    if (!self->wing_once) self->wing_input.open(&self->wing_input);
    {
        struct pixel_format format;
        memset(&format, 0, sizeof(format));
        format.bpp = (uint8_t)self->wing_fb.bpp;
        format.red_position = (uint8_t)self->wing_fb.red_pos;
        format.red_mask_size = (uint8_t)self->wing_fb.red_size;
        format.green_position = (uint8_t)self->wing_fb.green_pos;
        format.green_mask_size = (uint8_t)self->wing_fb.green_size;
        format.blue_position = (uint8_t)self->wing_fb.blue_pos;
        format.blue_mask_size = (uint8_t)self->wing_fb.blue_size;
        self->wing_screen = surface_native(self->wing_fb.width, self->wing_fb.height, &format);
        self->wing_background = surface_native(self->wing_fb.width, self->wing_fb.height, &format);
    }

    if (!self->wing_screen || !self->wing_background) {
        eputs("wing: cannot allocate desktop surfaces\n");
        if (self->wing_screen) surface_destroy(self->wing_screen);
        if (self->wing_background) surface_destroy(self->wing_background);
        if (!self->wing_once) self->wing_input.close(&self->wing_input);
        fbd_close(&self->wing_fb);
        return 1;
    }

    self->wing_windows = gfx_zero(WING_MANAGER_MAX, sizeof(*self->wing_windows));
    if (!self->wing_windows) {
        eputs("wing: cannot allocate window storage\n");
        surface_destroy(self->wing_screen);
        surface_destroy(self->wing_background);
        surface_destroy(self->wing_wallpaper);
        gfx_free(self->wing_clients);
        gfx_free(self->wing_windows);
        if (!self->wing_once) self->wing_input.close(&self->wing_input);
        fbd_close(&self->wing_fb);
        return 1;
    }

    self->wing_clients = gfx_zero(WING_CLIENT_MAX, sizeof(*self->wing_clients));
    if (!self->wing_clients) {
        eputs("wing: cannot allocate client storage\n");
        gfx_free(self->wing_windows);
        surface_destroy(self->wing_screen);
        surface_destroy(self->wing_background);
        if (!self->wing_once) self->wing_input.close(&self->wing_input);
        fbd_close(&self->wing_fb);
        return 1;
    }

    if (!Cursor_init(&self->wing_cursor, 12, 36)) {
        eputs("wing: cannot create self->wing_cursor\n");
        surface_destroy(self->wing_screen);
        surface_destroy(self->wing_background);
        if (!self->wing_once) self->wing_input.close(&self->wing_input);
        fbd_close(&self->wing_fb);
        return 1;
    }

    self->wing_wallpaper = load_wallpaper();
    WindowManager_init(&self->wing_manager, self->wing_windows, 0, &self->wing_cursor);
    WingServer_init(&self->wing_server, self->wing_clients, &self->wing_manager, &self->wing_dirty);
    if (!self->wing_server.open || !self->wing_server.poll || !self->wing_server.drop ||
        !self->wing_server.emit || !self->wing_server.notify) {
        eputs("wing: server methods are not initialized\n");
        return 1;
    }

    #if MONARCH_LOGWM
    /* Keep self->wing_wallpaper loading observable without making normal builds noisy. */
    self->wing_server.log(&self->wing_server, self->wing_wallpaper ? "self->wing_wallpaper-loaded" : "self->wing_wallpaper-fallback");
    #endif

    /* The self->wing_background is stable between events. Moving actors only restore the
       old and new rectangles, just like the Boing animation path. */
    render_target(&self->wing_renderer, self->wing_background);
    paint_desktop(&self->wing_renderer, self->wing_fb.width, self->wing_fb.height, self->wing_wallpaper);
    render_target(&self->wing_renderer, self->wing_screen);
    paint_desktop(&self->wing_renderer, self->wing_fb.width, self->wing_fb.height, self->wing_wallpaper);
    self->wing_manager.paint(&self->wing_manager, &self->wing_renderer);
    self->wing_context_menu.paint(&self->wing_context_menu, &self->wing_renderer);
    self->wing_cursor.paint(&self->wing_cursor, &self->wing_renderer);
    /* The self->wing_wallpaper has been copied into the persistent self->wing_background surface.
       Do not retain both the decoded BMP and two 800x600 framebuffers during
       the long-running session; on a 32 MiB guest that wastes several pages
       and can make later sbrk() allocations fail. */
    surface_destroy(self->wing_wallpaper);
    self->wing_wallpaper = nil;
    dirty_init(&self->wing_dirty);
    dirty_add(&self->wing_dirty, rect_make(0, 0, self->wing_fb.width, self->wing_fb.height));
    if (!present_rects(&self->wing_fb, self->wing_screen, &self->wing_dirty)) {
        eputs("wing: framebuffer presentation failed\n");
        self->wing_cursor.destroy(&self->wing_cursor);
        surface_destroy(self->wing_screen);
        surface_destroy(self->wing_background);
        surface_destroy(self->wing_wallpaper);
        gfx_free(self->wing_clients);
        gfx_free(self->wing_windows);
        if (!self->wing_once) self->wing_input.close(&self->wing_input);
        fbd_close(&self->wing_fb);
        return 1;
    }

    /* Publish the window self->wing_server only after the first complete desktop frame
       is ready. Clients cannot race the initial compositor setup. */
    if (self->wing_server.open(&self->wing_server) < 0) eputs("wing: socket self->wing_server unavailable\n");
    /* SPARK's service children inherit the PTY slave on 0/1/2. Wing never
       uses those descriptors; closing them is essential so USH exit can
       produce slave EOF and terminate the whole graphical session. */
    close(0);
    close(1);
    close(2);

    if (!self->wing_once) {
        forever {
            int changed = 0;
            dirty_clear(&self->wing_dirty);
            changed = self->wing_manager.tick(&self->wing_manager, &self->wing_dirty);
            changed |= self->wing_server.poll(&self->wing_server);
            while (self->wing_input.poll(&self->wing_input, &self->wing_event)) {
                if (self->wing_event.type == WING_EVENT_MOUSE) {
                    struct rect old_cursor = cursor_rect(&self->wing_cursor);
                    int pressed = (self->wing_event.buttons & MOUSE_LEFT) && !(self->wing_old_buttons & MOUSE_LEFT);
                    int released = !(self->wing_event.buttons & MOUSE_LEFT) && (self->wing_old_buttons & MOUSE_LEFT);
                    int right_pressed = (self->wing_event.buttons & MOUSE_RIGHT) && !(self->wing_old_buttons & MOUSE_RIGHT);
                    self->wing_event.action = pressed ? EVENT_MOUSE_DOWN : released ? EVENT_MOUSE_UP : EVENT_MOUSE_MOVE;
                    self->wing_event.mouse.x = self->wing_event.x;
                    self->wing_event.mouse.y = self->wing_event.y;
                    self->wing_event.mouse.dx = self->wing_event.dx;
                    self->wing_event.mouse.dy = self->wing_event.dy;
                    self->wing_event.mouse.buttons = self->wing_event.buttons;
                    self->wing_server.emit(&self->wing_server, self->wing_manager.hit(&self->wing_manager, self->wing_event.x, self->wing_event.y),
                                    WIN_EVENT_MOUSE, self->wing_event.x, self->wing_event.y,
                                    self->wing_event.dx, self->wing_event.dy, self->wing_event.buttons);
                    self->wing_cursor.x = self->wing_event.x;
                    self->wing_cursor.y = self->wing_event.y;
                    dirty_add(&self->wing_dirty, old_cursor);
                    dirty_add(&self->wing_dirty, cursor_rect(&self->wing_cursor));
                    self->wing_manager.update_hover(&self->wing_manager, self->wing_event.x, self->wing_event.y, &self->wing_dirty);
                    self->wing_manager.event(&self->wing_manager, self->wing_event.action ? self->wing_event.action : self->wing_event.type, &self->wing_event);
                    if (right_pressed) {
                        Window *hit = self->wing_manager.hit(&self->wing_manager, self->wing_event.x, self->wing_event.y);
                        if (!hit) {
                            self->wing_context_mode = 1;
                            self->wing_context_menu.show(&self->wing_context_menu, nil, self->wing_context_mode, self->wing_event.x, self->wing_event.y);
                        } else if (hit->title_hit(hit, self->wing_event.x, self->wing_event.y)) {
                            self->wing_context_mode = 2;
                            self->wing_context_menu.show(&self->wing_context_menu, hit, self->wing_context_mode, self->wing_event.x, self->wing_event.y);
                        }
                    }
                    if (pressed && self->wing_context_menu.click(&self->wing_context_menu, &self->wing_manager, self->wing_event.x, self->wing_event.y)) {
                        /* The context menu consumed this click. */
                    } else if (pressed) {
                        /* A missed release packet must not keep dragging the
                           previous window into the next click. */
                        self->wing_manager.end_drag(&self->wing_manager);
                        self->wing_drag_window = nil;
                        if (self->wing_context_menu.visible)
                            self->wing_context_menu.hide(&self->wing_context_menu);
                        {
                            Window *focus_before = self->wing_manager.active_window;
                            Window *clicked = self->wing_manager.hit(&self->wing_manager, self->wing_event.x, self->wing_event.y);
                            int clicked_close = clicked && clicked->close_button.hit(&clicked->close_button, self->wing_event.x, self->wing_event.y);
                            self->wing_drag_window = self->wing_manager.press(&self->wing_manager, self->wing_event.x, self->wing_event.y, &self->wing_dirty);
                            if (clicked_close) {
                                self->wing_server.emit(&self->wing_server, clicked, WIN_EVENT_CLOSE,
                                                0, 0, 0, 0, 0);
                                /* The close is committed locally before the
                                   client exits. Publish it immediately so
                                   Desk removes the taskbar button instead
                                   of keeping a stale WASP handle. */
                                self->wing_server.notify(&self->wing_server, clicked,
                                               WIN_EVENT_WINDOW_DESTROYED);
                                /* self->wing_manager.press() has already hidden and
                                   detached the parent. Remove its descendants
                                   before recovering focus; otherwise
                                   activate_top() can select a visible child
                                   whose parent is no longer in the root. */
                                self->wing_server.prune(&self->wing_server, clicked);
                                self->wing_manager.recover(&self->wing_manager, &self->wing_dirty);
                            }
                            if (focus_before != self->wing_manager.active_window) {
                                self->wing_server.emit(&self->wing_server, focus_before, WIN_EVENT_BLUR,
                                                 0, 0, 0, 0, 0);
                                self->wing_server.emit(&self->wing_server, self->wing_manager.active_window, WIN_EVENT_FOCUS,
                                                 0, 0, 0, 0, 0);
                                self->wing_server.notify(&self->wing_server, self->wing_manager.active_window, WIN_EVENT_FOCUS);
                            }
                        }
                        if (self->wing_drag_window) {
                            if (self->wing_manager.begin_drag(&self->wing_manager, self->wing_drag_window, self->wing_event.x, self->wing_event.y)) {
                                self->wing_drag_window = self->wing_manager.drag_window;
                            } else {
                                /* Body clicks focus the window but do not
                                   start a drag operation. */
                                self->wing_drag_window = nil;
                            }
                        }
                    }
                    if (self->wing_drag_window && (self->wing_event.buttons & 1u)) {
                        self->wing_manager.update_drag(&self->wing_manager, self->wing_event.x, self->wing_event.y, &self->wing_dirty);
                        sync_window_terminal(self->wing_drag_window);
                        if (self->wing_manager.resize_edges)
                            self->wing_server.emit(&self->wing_server, self->wing_drag_window, WIN_EVENT_RESIZE,
                                            self->wing_drag_window->frame.x, self->wing_drag_window->frame.y,
                                            (int32_t)self->wing_drag_window->frame.w,
                                            (int32_t)self->wing_drag_window->frame.h, 0);
                    }
                    if (released) {
                        self->wing_manager.release_buttons(&self->wing_manager, &self->wing_dirty);
                        self->wing_manager.end_drag(&self->wing_manager);
                        self->wing_drag_window = nil;
                    }
                    self->wing_old_buttons = self->wing_event.buttons;
                    self->wing_server.log(&self->wing_server, "mouse");
                    changed = 1;
                } else if (self->wing_event.type == WING_EVENT_KEY) {
                    self->wing_server.emit(&self->wing_server, self->wing_manager.active_window,
                                    WIN_EVENT_KEY, 0, 0, 0, 0, self->wing_event.key);
                    if (self->wing_manager.key(&self->wing_manager, self->wing_event.key) < 0) exit(0);
                    self->wing_server.log(&self->wing_server, "key");
                }
            }
            if (changed) {
                /* Always-on-top overlays (Desk's taskbar and its button
                   children) must be included in every visual transaction.
                   Restoring a self->wing_dirty region from the self->wing_background can otherwise
                   erase an overlay and leave it absent until a later full
                   redraw. The area is tiny, so correctness wins over a few
                   pixels of extra work. */
                for (uint32_t i = 0; i < self->wing_manager.count; i++)
                    if (self->wing_manager.windows[i].visible &&
                        (self->wing_manager.windows[i].flags & WINDOW_ALWAYS_ON_TOP))
                        dirty_add(&self->wing_dirty, self->wing_manager.windows[i].frame);
            }
            if (changed && self->wing_dirty.count) {
                for (uint32_t i = 0; i < self->wing_dirty.count; i++) {
                    struct rect r = rect_clip(self->wing_dirty.rects[i], self->wing_screen->width, self->wing_screen->height);
                    if (r.w && r.h) surface_copy(self->wing_screen, self->wing_background, &r, (uint32_t)r.x, (uint32_t)r.y);
                }
                render_target(&self->wing_renderer, self->wing_screen);
                /* Draw the complete scene graph, but clip each pass to the
                   rectangles touched by this self->wing_input batch. This keeps widget
                   code simple while avoiding work outside the self->wing_dirty regions. */
                for (uint32_t i = 0; i < self->wing_dirty.count; i++) {
                    struct rect r = rect_clip(self->wing_dirty.rects[i], self->wing_screen->width, self->wing_screen->height);
                    if (!r.w || !r.h) continue;
                    self->wing_renderer.clip(&self->wing_renderer, r.x, r.y, r.w, r.h);
                    self->wing_manager.paint(&self->wing_manager, &self->wing_renderer);
                    self->wing_context_menu.paint(&self->wing_context_menu, &self->wing_renderer);
                    self->wing_cursor.paint(&self->wing_cursor, &self->wing_renderer);
                    self->wing_renderer.no_clip(&self->wing_renderer);
                }
                if (!present_rects(&self->wing_fb, self->wing_screen, &self->wing_dirty)) break;
            }
            /* Two milliseconds keeps self->wing_cursor latency low without busy-spinning. */
            sleepms(2);
        }
    }
    self->wing_cursor.destroy(&self->wing_cursor);
    surface_destroy(self->wing_screen);
    surface_destroy(self->wing_background);
    surface_destroy(self->wing_wallpaper);
    gfx_free(self->wing_clients);
    gfx_free(self->wing_windows);
    if (!self->wing_once) self->wing_input.close(&self->wing_input);
    fbd_close(&self->wing_fb);
    puts("wing: desktop frame presented\n");
    return 0;
}

void Wing_init(Wing *self, int argc, char **argv) {
    if (!self) return;
    memset(self, 0, sizeof(*self));
    self->wing_argc = argc;
    self->wing_argv = argv;
    self->wing_once = argc > 1 && strcmp(argv[1], "--once") == 0;
    self->wing_clients = nil;
    self->wing_wallpaper = nil;
    self->run = run;
}
