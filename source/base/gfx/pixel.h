#ifndef MONARCH_BASE_GFX_PIXEL_H
#define MONARCH_BASE_GFX_PIXEL_H 1

/**
 * @file pixel.h
 * @brief Pixel format description and RGB packing helpers.
 *
 * GFX code uses this small structure instead of depending on a concrete
 * framebuffer driver.  Kernel and userspace adapters can translate their own
 * device metadata into `struct pixel_format` and then hand that to surfaces.
 */

#include "base/api/monarch.h"

/** Bit layout for one packed RGB pixel. */
struct pixel_format {
    uint8_t bpp;
    uint8_t red_position;
    uint8_t red_mask_size;
    uint8_t green_position;
    uint8_t green_mask_size;
    uint8_t blue_position;
    uint8_t blue_mask_size;
};

/** Fill `out` with the default 32-bit logical 0x00RRGGBB layout. */
void pixel_format_rgb32(struct pixel_format *out);

/** Non-zero when `format` is exactly the 32bpp 0x00RRGGBB layout, so raw
    32-bit reads/writes can skip pack/unpack entirely. Callers that repeat
    this per pixel should cache the result once instead of calling this in
    a hot loop (see struct surface's fast_rgb32 field). */
int pixel_format_is_native32(const struct pixel_format *format);

/** Pack logical 0x00RRGGBB into `format`'s native representation. */
uint32_t pixel_pack(const struct pixel_format *format, uint32_t rgb);

/** Unpack one native pixel into logical 0x00RRGGBB. */
uint32_t pixel_unpack(const struct pixel_format *format, uint32_t native);

/** Return non-zero when two pixel formats are exactly identical. */
int pixel_format_match(const struct pixel_format *a, const struct pixel_format *b);

/** Blend `src` over `dst` (both logical 0x00RRGGBB) using a uniform alpha,
    0 = fully transparent src, 255 = fully opaque src. Shared by every
    alpha-blit path so there is one implementation to keep fast. */
uint32_t pixel_blend(uint32_t dst, uint32_t src, uint8_t alpha);

#endif /* MONARCH_BASE_GFX_PIXEL_H */
