#ifndef MONARCH_BASE_GFX_DRAW_H
#define MONARCH_BASE_GFX_DRAW_H 1

/**
 * @file draw.h
 * @brief Low-level drawing primitives for linear surfaces.
 *
 * The functions in this file operate on `struct surface` objects, not directly
 * on the physical framebuffer.  That is intentional: drawing into memory first
 * lets higher-level code build sprites, cached backgrounds, UI widgets, or
 * backbuffers before presenting them to the screen.
 *
 * These helpers are deliberately small and easy to read.  More feature-rich
 * drawing is provided by `renderer.h`, which adds framebuffer conversion,
 * clipping, text, scaling, alpha, and color-key blits.
 */

#include "base/gfx/surface.h"

/**
 * Draw a single pixel to a surface.
 *
 * @param s     Target surface.
 * @param x     Horizontal pixel coordinate.
 * @param y     Vertical pixel coordinate.
 * @param color Logical RGB color (`0x00RRGGBB`).  The surface layer converts it
 *              to the storage format used by the surface.
 */
void draw_pixel(struct surface *s, uint32_t x, uint32_t y, uint32_t color);

/**
 * Draw a filled axis-aligned rectangle.
 *
 * The rectangle is clipped by `surface_set()` bounds checks.  Very large
 * rectangles are therefore safe but may be slow because this function is a
 * straightforward nested pixel loop.
 */
void draw_rect(struct surface *s, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);

/**
 * Draw a line between two points.
 *
 * Uses an integer Bresenham-style stepping algorithm.  Negative input
 * coordinates are accepted; pixels outside the surface are ignored.
 */
void draw_line(struct surface *s, int x0, int y0, int x1, int y1, uint32_t color);

#endif /* MONARCH_BASE_GFX_DRAW_H */
