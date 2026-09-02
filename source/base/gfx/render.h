#ifndef MONARCH_BASE_GFX_RENDER_H
#define MONARCH_BASE_GFX_RENDER_H 1

/**
 * @file render.h
 * @brief High-level 2D renderer for surface targets.
 *
 * The renderer is the main drawing interface used by BGL demos.  It targets a
 * `struct surface`; framebuffer or device-specific code should first wrap its
 * device memory as a surface using an adapter outside `base/gfx`.
 */

#include "base/gfx/surface.h"

/** Renderer object with SDL/Pygame-like drawing methods. */
struct renderer {
    void (*clear)(struct renderer *self, uint32_t rgb);
    void (*pixel)(struct renderer *self, uint32_t x, uint32_t y, uint32_t rgb);
    void (*rect)(struct renderer *self, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t rgb);
    void (*line)(struct renderer *self, int x0, int y0, int x1, int y1, uint32_t rgb);
    void (*circle)(struct renderer *self, int cx, int cy, uint32_t radius, uint32_t rgb);
    void (*triangle)(struct renderer *self, int x0, int y0, int x1, int y1, int x2, int y2, uint32_t rgb);

    void (*blit)(struct renderer *self, struct surface *surface, uint32_t x, uint32_t y);
    void (*blit_rect)(struct renderer *self, struct surface *surface, const struct rect *src, uint32_t x, uint32_t y);
    void (*blit_key)(struct renderer *self, struct surface *surface, uint32_t x, uint32_t y, uint32_t key);
    void (*blit_key_rect)(struct renderer *self, struct surface *surface, const struct rect *src, uint32_t x, uint32_t y, uint32_t key);
    void (*blit_alpha)(struct renderer *self, struct surface *surface, uint32_t x, uint32_t y, uint8_t alpha);
    void (*blit_alpha_rect)(struct renderer *self, struct surface *surface, const struct rect *src, uint32_t x, uint32_t y, uint8_t alpha);

    void (*scale)(struct renderer *self, struct surface *surface, uint32_t x, uint32_t y, uint32_t w, uint32_t h);
    void (*scale_rect)(struct renderer *self, struct surface *surface, const struct rect *src, uint32_t x, uint32_t y, uint32_t w, uint32_t h);
    void (*scale_key)(struct renderer *self, struct surface *surface, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t key);
    void (*scale_key_rect)(struct renderer *self, struct surface *surface, const struct rect *src, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t key);

    void (*text)(struct renderer *self, const char *value, uint32_t x, uint32_t y, uint32_t fg, uint32_t bg);
    void (*clip)(struct renderer *self, int32_t x, int32_t y, uint32_t w, uint32_t h);
    void (*no_clip)(struct renderer *self);
    int (*ready)(struct renderer *self);

    /** Active surface target. */
    struct surface *target;
    /** Storage used by adapters that need an embedded surface view. */
    struct surface _view;
    /** Active clipping rectangle. */
    struct rect cliprect;
    /** Non-zero when clipping is enabled. */
    int clipped;
};

/** Initialise a renderer that draws into a surface. */
void render_target(struct renderer *r, struct surface *target);

/** Readable alias for render_target(). */
void render_surface(struct renderer *r, struct surface *target);

#endif /* MONARCH_BASE_GFX_RENDER_H */
