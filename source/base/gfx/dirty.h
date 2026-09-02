#ifndef MONARCH_BASE_GFX_DIRTY_H
#define MONARCH_BASE_GFX_DIRTY_H 1

/**
 * @file dirty.h
 * @brief Dirty rectangle tracking for efficient 2D updates.
 *
 * A dirty rectangle is an area of the screen or backbuffer that changed during a
 * frame.  Instead of redrawing the whole framebuffer every time an object moves,
 * a demo can track only the old and new object rectangles, restore those regions
 * from the background, redraw objects, and present only those regions.  This is
 * the same idea used by many old 2D engines and by the original Butterfly BGL
 * demos.
 */

#include "base/gfx/surface.h"

/** Maximum number of dirty rectangles kept before merging into a larger area. */
#define DIRTY_MAX 128u

/** List of rectangles that need to be updated. */
struct dirty {
    struct rect rects[DIRTY_MAX];
    uint32_t count;
};

/** Initialise a dirty list. */
void dirty_init(struct dirty *list);

/** Remove all tracked rectangles.  Call this at the start of a new frame. */
void dirty_clear(struct dirty *list);

/** Add a rectangle, merging it with an existing overlapping rectangle if possible. */
void dirty_add(struct dirty *list, struct rect rect);

/** Construct a rectangle value. */
struct rect rect_make(int32_t x, int32_t y, uint32_t w, uint32_t h);

/** Return a rectangle that covers both input rectangles. */
struct rect rect_union(struct rect a, struct rect b);

/** Return non-zero when two rectangles overlap. */
int rect_intersect(struct rect a, struct rect b);

/** Clip a rectangle to the bounds `[0,width) x [0,height)`. */
struct rect rect_clip(struct rect r, uint32_t width, uint32_t height);

#endif /* MONARCH_BASE_GFX_DIRTY_H */
