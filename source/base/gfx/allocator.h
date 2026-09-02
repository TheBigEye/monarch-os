#ifndef MONARCH_BASE_GFX_ALLOCATOR_H
#define MONARCH_BASE_GFX_ALLOCATOR_H 1

/**
 * @file allocator.h
 * @brief Allocation hooks used by owned GFX objects.
 *
 * GFX does not know whether its caller uses a kernel heap, a userspace
 * allocator, or a fixed arena.  The platform supplies the three callbacks;
 * the GFX objects only use this small interface.
 */

#include "base/api/monarch.h"

struct gfx_allocator {
    void *(*alloc)(size_t size);
    void *(*zero)(size_t count, size_t size);
    void (*free)(void *ptr);
};

/** Return the allocator selected by the current platform. */
const struct gfx_allocator *gfx_allocator_default(void);

/** Allocate uninitialised memory through the current platform. */
void *gfx_alloc(size_t size);

/** Allocate zeroed memory through the current platform. */
void *gfx_zero(size_t count, size_t size);

/** Release memory through the current platform. */
void gfx_free(void *ptr);

#endif /* MONARCH_BASE_GFX_ALLOCATOR_H */
