/**
 * @file pixel.c
 * @brief RGB packing/unpacking helpers for arbitrary bitfield layouts.
 */

#include "base/gfx/pixel.h"

static uint32_t mask(uint8_t size) {
    if (!size) {
        return 0;
    }
    if (size >= 32u) {
        return 0xFFFFFFFFu;
    }
    return (1u << size) - 1u;
}

static uint32_t scale_to_bits(uint8_t value, uint8_t size) {
    uint32_t m = mask(size);
    if (!m) {
        return 0;
    }
    return ((uint32_t)value * m) / 255u;
}

static uint8_t scale_from_bits(uint32_t value, uint8_t size) {
    uint32_t m = mask(size);
    if (!m) {
        return 0;
    }
    return (uint8_t)((value * 255u) / m);
}

void pixel_format_rgb32(struct pixel_format *out) {
    if (!out) {
        return;
    }

    memset(out, 0, sizeof(*out));
    out->bpp = 32;
    out->red_position = 16;
    out->red_mask_size = 8;
    out->green_position = 8;
    out->green_mask_size = 8;
    out->blue_position = 0;
    out->blue_mask_size = 8;
}

int pixel_format_is_native32(const struct pixel_format *format) {
    return format &&
        format->bpp == 32 &&
        format->red_position == 16 && format->red_mask_size == 8 &&
        format->green_position == 8 && format->green_mask_size == 8 &&
        format->blue_position == 0 && format->blue_mask_size == 8;
}

uint32_t pixel_pack(const struct pixel_format *format, uint32_t rgb) {
    struct pixel_format fallback;
    uint8_t r;
    uint8_t g;
    uint8_t b;

    if (!format) {
        pixel_format_rgb32(&fallback);
        format = &fallback;
    }

    /* The normal framebuffer path is XRGB8888.  Avoid three divisions for
       every pixel when the logical and native layouts are identical. */
    if (pixel_format_is_native32(format)) {
        return rgb & 0x00FFFFFFu;
    }

    r = (uint8_t)(rgb >> 16);
    g = (uint8_t)(rgb >> 8);
    b = (uint8_t)rgb;

    return (scale_to_bits(r, format->red_mask_size) << format->red_position) |
           (scale_to_bits(g, format->green_mask_size) << format->green_position) |
           (scale_to_bits(b, format->blue_mask_size) << format->blue_position);
}

uint32_t pixel_unpack(const struct pixel_format *format, uint32_t native) {
    struct pixel_format fallback;
    uint8_t r;
    uint8_t g;
    uint8_t b;

    if (!format) {
        pixel_format_rgb32(&fallback);
        format = &fallback;
    }

    if (pixel_format_is_native32(format)) {
        return native & 0x00FFFFFFu;
    }

    r = scale_from_bits((native >> format->red_position) & mask(format->red_mask_size), format->red_mask_size);
    g = scale_from_bits((native >> format->green_position) & mask(format->green_mask_size), format->green_mask_size);
    b = scale_from_bits((native >> format->blue_position) & mask(format->blue_mask_size), format->blue_mask_size);

    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

int pixel_format_match(const struct pixel_format *a, const struct pixel_format *b) {
    return a && b &&
        a->bpp == b->bpp &&
        a->red_position == b->red_position &&
        a->red_mask_size == b->red_mask_size &&
        a->green_position == b->green_position &&
        a->green_mask_size == b->green_mask_size &&
        a->blue_position == b->blue_position &&
        a->blue_mask_size == b->blue_mask_size;
}

/* Exact for the range this is used in (products of two bytes, so
   0..65025): replaces a divide-by-255 with a shift, which matters when this
   runs per channel, per pixel, in every alpha blit. */
static uint32_t div255(uint32_t v) {
    return (v + 1u + (v >> 8)) >> 8;
}

uint32_t pixel_blend(uint32_t dst, uint32_t src, uint8_t alpha) {
    uint32_t sr = (src >> 16) & 0xFFu;
    uint32_t sg = (src >> 8) & 0xFFu;
    uint32_t sb = src & 0xFFu;
    uint32_t dr = (dst >> 16) & 0xFFu;
    uint32_t dg = (dst >> 8) & 0xFFu;
    uint32_t db = dst & 0xFFu;
    uint32_t a = alpha;
    uint32_t ia = 255u - a;

    return (div255(sr * a + dr * ia) << 16) |
           (div255(sg * a + dg * ia) << 8) |
           div255(sb * a + db * ia);
}
