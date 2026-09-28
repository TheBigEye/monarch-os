/**
 * @file context_menu.c
 * @brief Private context-menu class used by Wing's desktop surface.
 */
#include "context_menu.h"
#include "base/usr/sys.h"
#include "base/win/input.h"

private struct rect rect(ContextMenu *self) {
    return rect_make(self->x, self->y, self->w, self->h);
}

private void hide(ContextMenu *self) {
    if (!self || !self->visible) return;
    if (self->dirty) dirty_add(self->dirty, rect(self));
    self->visible = 0;
    self->target = nil;
}

private void show(ContextMenu *self, Window *target, int mode, int32_t x, int32_t y) {
    uint32_t height = mode == 1 ? 70u : 114u;
    if (!self) return;
    if (self->visible && self->dirty) dirty_add(self->dirty, rect(self));
    if (x + 190 > 800) x = 800 - 190;
    if (y + (int32_t)height > 600) y = 600 - (int32_t)height;
    self->visible = 1;
    self->mode = mode;
    self->x = x;
    self->y = y;
    self->w = 190;
    self->h = height;
    self->target = target;
    if (self->dirty) dirty_add(self->dirty, rect(self));
}

private void paint(ContextMenu *self, struct renderer *renderer) {
    int32_t x;
    int32_t y;
    uint32_t w;
    uint32_t h;
    if (!self || !self->visible || !renderer) return;
    x = self->x; y = self->y; w = self->w; h = self->h;
    renderer->rect(renderer, (uint32_t)x, (uint32_t)y, w, h, 0x000000);
    renderer->rect(renderer, (uint32_t)x + 1u, (uint32_t)y + 1u, w - 2u, h - 2u, 0xFFFFFF);
    renderer->rect(renderer, (uint32_t)x + 3u, (uint32_t)y + 3u, w - 6u, h - 6u, 0x808080);
    renderer->rect(renderer, (uint32_t)x + 4u, (uint32_t)y + 4u, w - 8u, h - 8u, 0xC0C0C0);
    renderer->rect(renderer, (uint32_t)x + 4u, (uint32_t)y + 4u, w - 8u, 18u, 0x000080);
    renderer->text(renderer, self->mode == 1 ? "Desktop" : "Window",
                   (uint32_t)x + 10u, (uint32_t)y + 7u, 0xFFFFFF, 0x000080);
    if (self->mode == 1) {
        renderer->text(renderer, "New window", (uint32_t)x + 14u,
                       (uint32_t)y + 35u, 0x000000, 0xC0C0C0);
    } else {
        renderer->text(renderer, "Minimize", (uint32_t)x + 14u,
                       (uint32_t)y + 32u, 0x000000, 0xC0C0C0);
        renderer->text(renderer, "Maximize", (uint32_t)x + 14u,
                       (uint32_t)y + 60u, 0x000000, 0xC0C0C0);
        renderer->text(renderer, "Close", (uint32_t)x + 14u,
                       (uint32_t)y + 88u, 0x000000, 0xC0C0C0);
    }
}

private int click(ContextMenu *self, WindowManager *manager, int32_t x, int32_t y) {
    Window *target;
    if (!self || !manager || !self->visible || x < self->x || y < self->y ||
        x >= self->x + (int32_t)self->w || y >= self->y + (int32_t)self->h) return 0;
    if (y < self->y + 24) return 1;
    if (self->mode == 1) {
        if (y < self->y + 58) {
            hide(self);
            spawn("/initrd/win/bin/wasp.elf");
        }
        return 1;
    }
    target = self->target;
    if (!target || !target->visible) { hide(self); return 1; }
    if (y < self->y + 54) manager->pending_action = EVENT_MINIMIZE;
    else if (y < self->y + 82) manager->pending_action = EVENT_MAXIMIZE;
    else manager->pending_action = EVENT_CLOSE;
    target->cache_valid = 0;
    target->minimize_button.pressed = manager->pending_action == EVENT_MINIMIZE;
    target->maximize_button.pressed = manager->pending_action == EVENT_MAXIMIZE;
    target->close_button.pressed = manager->pending_action == EVENT_CLOSE;
    manager->pending_window = target;
    manager->pending_ticks = 1;
    hide(self);
    dirty_add(self->dirty, target->frame);
    return 1;
}

void ContextMenu_init(ContextMenu *self, struct dirty *dirty) {
    if (!self) return;
    memset(self, 0, sizeof(*self));
    self->dirty = dirty;
    self->show = show;
    self->click = click;
    self->paint = paint;
    self->hide = hide;
}
