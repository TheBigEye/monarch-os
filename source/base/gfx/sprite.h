#ifndef MONARCH_BASE_GFX_SPRITE_H
#define MONARCH_BASE_GFX_SPRITE_H 1

/**
 * @file sprite.h
 * @brief Sprite object built on top of surfaces and the renderer.
 *
 * A sprite is a drawable image with optional state: a source rectangle, a
 * transparent color key, a uniform alpha value, and ownership information for
 * the wrapped surface.  This keeps demo code readable: instead of remembering
 * which renderer function to call for each mode, code calls `sprite->draw()` or
 * `sprite->scale()` and lets the sprite choose the correct renderer path.
 */

#include "base/gfx/render.h"

/** Drawable image object. */
struct sprite {
    /** Draw at the sprite's natural size. */
    void (*draw)(struct sprite *self, struct renderer *renderer, uint32_t x, uint32_t y);

    /** Draw scaled to `w` by `h` pixels. */
    void (*scale)(struct sprite *self, struct renderer *renderer, uint32_t x, uint32_t y, uint32_t w, uint32_t h);

    /** Use a sub-rectangle of the source surface instead of the full image. */
    void (*source)(struct sprite *self, int32_t x, int32_t y, uint32_t w, uint32_t h);

    /** Return to using the full source surface. */
    void (*full)(struct sprite *self);

    /** Enable or disable color-key transparency. */
    void (*key)(struct sprite *self, uint32_t color, int enabled);

    /** Set uniform alpha.  255 is opaque, 0 is fully transparent. */
    void (*alpha)(struct sprite *self, uint8_t alpha);

    /** Destroy the sprite and optionally its owned surface. */
    void (*destroy)(struct sprite *self);

    /** Private: source pixels. */
    struct surface *_surface;
    /** Private: optional source rectangle. */
    struct rect _source;
    /** Private: whether `_source` is active. */
    int _has_source;
    /** Private: logical RGB color treated as transparent when enabled. */
    uint32_t _key;
    /** Private: non-zero when color-key transparency is enabled. */
    int _keyed;
    /** Private: uniform alpha value. */
    uint8_t _alpha;
    /** Private: whether destroy() also destroys `_surface`. */
    int _owned;
};

/** Initialise a sprite around an existing surface.  The surface is not owned. */
void sprite_init(struct sprite *self, struct surface *surface);

/** Allocate a sprite object.  If `take_ownership` is non-zero, it owns surface. */
struct sprite *sprite_create(struct surface *surface, int take_ownership);

#endif /* MONARCH_BASE_GFX_SPRITE_H */
