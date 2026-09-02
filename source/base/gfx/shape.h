#ifndef MONARCH_BASE_GFX_SHAPE_H
#define MONARCH_BASE_GFX_SHAPE_H 1

/**
 * @file shape.h
 * @brief Helpers that create reusable shape surfaces.
 *
 * Real-time demos should avoid redrawing expensive shapes pixel-by-pixel every
 * frame.  Instead, they can build a small surface once, wrap it in a sprite, and
 * reuse that sprite many times.  This file provides a few convenience builders
 * for exactly that pattern.
 *
 * All returned surfaces are heap-allocated and owned by the caller.  Destroy
 * them with `surface_destroy()` directly, or pass ownership to `sprite_create()`.
 */

#include "base/gfx/surface.h"

/** Create a filled rectangle surface of size `w` by `h`. */
struct surface *shape_rect(uint32_t w, uint32_t h, uint32_t color);

/** Create a filled circle inside a square `size` by `size` surface. */
struct surface *shape_circle(uint32_t size, uint32_t color);

/** Create a filled upright triangle inside a `w` by `h` surface. */
struct surface *shape_triangle(uint32_t w, uint32_t h, uint32_t color);

/** Create a small text label surface using the default 8x8 font. */
struct surface *shape_text(const char *value, uint32_t fg, uint32_t bg);

/** Create the default software mouse cursor surface. */
struct surface *shape_cursor(void);

#endif /* MONARCH_BASE_GFX_SHAPE_H */
