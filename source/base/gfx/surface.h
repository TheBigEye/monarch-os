#ifndef MONARCH_BASE_GFX_SURFACE_H
#define MONARCH_BASE_GFX_SURFACE_H 1

/**
 * @file surface.h
 * @brief Linear in-memory bitmap surfaces used by Monarch's GFX/BGL layer.
 *
 * A surface is a rectangular pixel buffer.  It may own its memory, or it may be
 * a view over device memory supplied by an adapter such as `base/sys/gfx_fb` or
 * `base/fbd` later.  This header deliberately does not include framebuffer
 * driver types.
 */

#include "base/gfx/allocator.h"
#include "base/gfx/pixel.h"

/** Logical RGB surface format: every pixel is stored as 0x00RRGGBB. */
#define SURFACE_RGB32    1u

/** Native device format: pixels are already packed for a specific target. */
#define SURFACE_NATIVE32 2u

/** Rectangle helper used for clipping, dirty regions, and source rectangles. */
struct rect {
    int32_t x;
    int32_t y;
    uint32_t w;
    uint32_t h;
};

/** Linear bitmap surface. */
struct surface {
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint8_t bpp;
    uint8_t owned;
    /** Cached result of pixel_format_is_native32(&pixel) && bpp == 32, so
        per-pixel accessors don't re-check the format on every call. Kept in
        sync by surface_format() -- never set this field directly. */
    uint8_t fast_rgb32;
    uint32_t format;
    struct pixel_format pixel;
    uint8_t *pixels;
    /** Allocator used for owned storage; null for non-owning views. */
    const struct gfx_allocator *allocator;
};

/** Initialise a non-owning logical RGB surface around existing memory. */
void surface_init(struct surface *s, uint32_t width, uint32_t height, uint8_t bpp, void *pixels);

/** Initialise a non-owning surface view with explicit pitch and pixel format. */
void surface_view(struct surface *s, uint32_t width, uint32_t height, uint32_t pitch, uint8_t bpp, void *pixels, const struct pixel_format *format);

/** Change a surface's pixel format metadata. */
void surface_format(struct surface *s, const struct pixel_format *format, uint32_t kind);

/** Allocate a new owned 32-bit logical RGB surface. */
struct surface *surface_create(uint32_t width, uint32_t height);

/** Allocate a new owned 32-bit surface using `format` as native layout. */
struct surface *surface_native(uint32_t width, uint32_t height, const struct pixel_format *format);

/** Destroy an owned surface object and its pixel memory when applicable. */
void surface_destroy(struct surface *s);

/** Create a nearest-neighbour scaled copy of `src` in logical RGB format. */
struct surface *surface_scale(struct surface *src, uint32_t width, uint32_t height);

/** Convert a surface into the supplied native pixel format. */
struct surface *surface_convert(struct surface *src, const struct pixel_format *format);

/** Convert logical 0x00RRGGBB into the storage format of this surface. */
uint32_t surface_pack(struct surface *s, uint32_t rgb);

/** Convert a raw stored pixel from this surface back into logical 0x00RRGGBB. */
uint32_t surface_unpack(struct surface *s, uint32_t native);

/** Return non-zero when two surfaces store pixels in the exact same format. */
int surface_match(struct surface *a, struct surface *b);

/** Read a pixel as logical 0x00RRGGBB. */
uint32_t surface_get(struct surface *s, uint32_t x, uint32_t y);

/** Read a pixel exactly as it is stored in memory, without format conversion. */
uint32_t surface_raw(struct surface *s, uint32_t x, uint32_t y);

/** Write a logical 0x00RRGGBB pixel, converting it to the surface format. */
void surface_set(struct surface *s, uint32_t x, uint32_t y, uint32_t color);

/** Write a raw pixel value without conversion. */
void surface_set_raw(struct surface *s, uint32_t x, uint32_t y, uint32_t color);

/** Fill the entire surface with a logical RGB color. */
void surface_fill(struct surface *s, uint32_t color);

/** Copy all of `src` to `dst` at `(x, y)`. */
void surface_blit(struct surface *dst, struct surface *src, uint32_t x, uint32_t y);

/** Copy a source rectangle from `src` to `dst`, clipping to destination bounds. */
void surface_copy(struct surface *dst, struct surface *src, const struct rect *src_rect, uint32_t x, uint32_t y);

/** Copy pixels except those equal to `key`, which act as transparent pixels. */
void surface_blit_key(struct surface *dst, struct surface *src, const struct rect *src_rect, uint32_t x, uint32_t y, uint32_t key);

/** Copy pixels while blending source over destination using a uniform alpha value. */
void surface_blit_alpha(struct surface *dst, struct surface *src, const struct rect *src_rect, uint32_t x, uint32_t y, uint8_t alpha);

#endif /* MONARCH_BASE_GFX_SURFACE_H */
