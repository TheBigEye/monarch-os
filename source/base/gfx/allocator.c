/**
 * @file allocator.c
 * @brief Platform-neutral allocation dispatch for owned GFX objects.
 */

#include "base/gfx/allocator.h"

/* Kernel and userspace adapters may replace these weak hooks. */
__attribute__((weak)) void *gfx_platform_alloc(size_t size) {
    (void)size;
    return nil;
}

__attribute__((weak)) void *gfx_platform_zero(size_t count, size_t size) {
    (void)count;
    (void)size;
    return nil;
}

__attribute__((weak)) void gfx_platform_free(void *ptr) {
    (void)ptr;
}

static const struct gfx_allocator default_allocator = {
    gfx_platform_alloc,
    gfx_platform_zero,
    gfx_platform_free
};

const struct gfx_allocator *gfx_allocator_default(void) {
    return &default_allocator;
}

void *gfx_alloc(size_t size) {
    return default_allocator.alloc(size);
}

void *gfx_zero(size_t count, size_t size) {
    return default_allocator.zero(count, size);
}

void gfx_free(void *ptr) {
    default_allocator.free(ptr);
}
