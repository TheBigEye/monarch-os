/**
 * @file surface.c
 * @brief Linear bitmap surface implementation.
 */

#include "base/gfx/surface.h"

static void rgb_format(struct surface *s) {
    struct pixel_format format;
    pixel_format_rgb32(&format);
    surface_format(s, &format, SURFACE_RGB32);
}

void surface_format(struct surface *s, const struct pixel_format *format, uint32_t kind) {
    struct pixel_format fallback;

    if (!s) {
        return;
    }

    if (!format) {
        pixel_format_rgb32(&fallback);
        format = &fallback;
        kind = SURFACE_RGB32;
    }

    s->format = kind;
    s->pixel = *format;
    s->bpp = format->bpp ? format->bpp : s->bpp;
    s->fast_rgb32 = (uint8_t)(s->bpp == 32 && pixel_format_is_native32(&s->pixel));
}

void surface_init(struct surface *s, uint32_t width, uint32_t height, uint8_t bpp, void *pixels) {
    memset(s, 0, sizeof(*s));
    s->width = width;
    s->height = height;
    s->bpp = bpp;
    s->pitch = width * (bpp / 8u);
    s->pixels = pixels;
    s->owned = 0;
    s->allocator = nil;
    rgb_format(s);
}

void surface_view(struct surface *s, uint32_t width, uint32_t height, uint32_t pitch, uint8_t bpp, void *pixels, const struct pixel_format *format) {
    memset(s, 0, sizeof(*s));
    s->width = width;
    s->height = height;
    s->pitch = pitch;
    s->bpp = bpp;
    s->pixels = pixels;
    s->owned = 0;
    s->allocator = nil;
    surface_format(s, format, format ? SURFACE_NATIVE32 : SURFACE_RGB32);
}

struct surface *surface_create(uint32_t width, uint32_t height) {
    struct surface *s = gfx_alloc(sizeof(*s));
    size_t bytes = (size_t)width * height * 4u;

    if (!s) {
        return nil;
    }

    surface_init(s, width, height, 32, nil);
    s->pixels = gfx_zero(bytes, 1);
    if (!s->pixels) {
        gfx_free(s);
        return nil;
    }

    s->owned = 1;
    s->allocator = gfx_allocator_default();
    return s;
}

struct surface *surface_native(uint32_t width, uint32_t height, const struct pixel_format *format) {
    struct surface *s = surface_create(width, height);

    if (!s) {
        return nil;
    }

    if (format) {
        surface_format(s, format, SURFACE_NATIVE32);
    }
    return s;
}

void surface_destroy(struct surface *s) {
    if (!s) {
        return;
    }
    if (s->owned && s->pixels && s->allocator) {
        s->allocator->free(s->pixels);
    }
    if (s->allocator) {
        s->allocator->free(s);
    } else {
        gfx_free(s);
    }
}

struct surface *surface_scale(struct surface *src, uint32_t width, uint32_t height) {
    struct surface *dst;
    uint32_t step_x;
    uint32_t step_y;
    uint32_t sy_fixed;

    if (!src || !src->pixels || !width || !height) {
        return nil;
    }

    dst = surface_create(width, height);
    if (!dst) {
        return nil;
    }

    /* 16.16 fixed-point step per output pixel: one division per axis
       instead of one division per pixel, same nearest-neighbor result as
       (coord * src_extent) / dst_extent. */
    step_x = (src->width << 16) / width;
    step_y = (src->height << 16) / height;
    sy_fixed = 0;

    for (uint32_t y = 0; y < height; y++) {
        uint32_t sy = sy_fixed >> 16;
        uint32_t sx_fixed = 0;

        for (uint32_t x = 0; x < width; x++) {
            surface_set(dst, x, y, surface_get(src, sx_fixed >> 16, sy));
            sx_fixed += step_x;
        }
        sy_fixed += step_y;
    }

    return dst;
}

struct surface *surface_convert(struct surface *src, const struct pixel_format *format) {
    struct surface *dst;

    if (!src || !src->pixels || !format) {
        return nil;
    }

    dst = surface_native(src->width, src->height, format);
    if (!dst) {
        return nil;
    }

    for (uint32_t y = 0; y < src->height; y++) {
        for (uint32_t x = 0; x < src->width; x++) {
            surface_set(dst, x, y, surface_get(src, x, y));
        }
    }

    return dst;
}

uint32_t surface_pack(struct surface *s, uint32_t rgb) {
    if (!s) {
        return rgb & 0x00FFFFFFu;
    }
    return pixel_pack(&s->pixel, rgb);
}

uint32_t surface_unpack(struct surface *s, uint32_t native) {
    if (!s) {
        return native & 0x00FFFFFFu;
    }
    return pixel_unpack(&s->pixel, native);
}

int surface_match(struct surface *a, struct surface *b) {
    return a && b && a->bpp == b->bpp && a->format == b->format && pixel_format_match(&a->pixel, &b->pixel);
}

static uint8_t *addr(struct surface *s, uint32_t x, uint32_t y) {
    return s->pixels + (size_t)y * s->pitch + (size_t)x * (s->bpp / 8u);
}

uint32_t surface_raw(struct surface *s, uint32_t x, uint32_t y) {
    uint8_t *p;

    if (!s || !s->pixels || x >= s->width || y >= s->height) {
        return 0;
    }

    p = addr(s, x, y);
    if (s->bpp == 32) {
        return *(uint32_t *)p;
    }
    if (s->bpp == 24) {
        return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
    }
    if (s->bpp == 16) {
        return *(uint16_t *)p;
    }
    if (s->bpp == 8) {
        return *p;
    }
    return 0;
}

uint32_t surface_get(struct surface *s, uint32_t x, uint32_t y) {
    if (s && s->fast_rgb32 && s->pixels && x < s->width && y < s->height) {
        return *(uint32_t *)addr(s, x, y) & 0x00FFFFFFu;
    }
    return surface_unpack(s, surface_raw(s, x, y));
}

void surface_set_raw(struct surface *s, uint32_t x, uint32_t y, uint32_t color) {
    uint8_t *p;

    if (!s || !s->pixels || x >= s->width || y >= s->height) {
        return;
    }

    p = addr(s, x, y);
    if (s->bpp == 32) {
        *(uint32_t *)p = color;
    } else if (s->bpp == 24) {
        p[0] = (uint8_t)(color & 0xFF);
        p[1] = (uint8_t)((color >> 8) & 0xFF);
        p[2] = (uint8_t)((color >> 16) & 0xFF);
    } else if (s->bpp == 16) {
        *(uint16_t *)p = (uint16_t)color;
    } else if (s->bpp == 8) {
        *p = (uint8_t)color;
    }
}

void surface_set(struct surface *s, uint32_t x, uint32_t y, uint32_t color) {
    if (s && s->fast_rgb32 && s->pixels && x < s->width && y < s->height) {
        *(uint32_t *)addr(s, x, y) = color & 0x00FFFFFFu;
        return;
    }
    surface_set_raw(s, x, y, surface_pack(s, color));
}

void surface_fill(struct surface *s, uint32_t color) {
    uint32_t raw;

    if (!s || !s->pixels) {
        return;
    }

    raw = surface_pack(s, color);
    if (s->bpp == 32) {
        for (uint32_t y = 0; y < s->height; y++) {
            uint32_t *row = (uint32_t *)(s->pixels + (size_t)y * s->pitch);
            for (uint32_t x = 0; x < s->width; x++) {
                row[x] = raw;
            }
        }
        return;
    }

    for (uint32_t y = 0; y < s->height; y++) {
        for (uint32_t x = 0; x < s->width; x++) {
            surface_set_raw(s, x, y, raw);
        }
    }
}

void surface_blit(struct surface *dst, struct surface *src, uint32_t x, uint32_t y) {
    surface_copy(dst, src, nil, x, y);
}

static struct rect source_rect(struct surface *src, const struct rect *src_rect) {
    struct rect r;

    if (src_rect) {
        r = *src_rect;
    } else {
        r.x = 0;
        r.y = 0;
        r.w = src ? src->width : 0;
        r.h = src ? src->height : 0;
    }

    if (!src || r.x < 0 || r.y < 0 || (uint32_t)r.x >= src->width || (uint32_t)r.y >= src->height) {
        r.w = 0;
        r.h = 0;
        return r;
    }

    if ((uint32_t)r.x + r.w > src->width) {
        r.w = src->width - (uint32_t)r.x;
    }
    if ((uint32_t)r.y + r.h > src->height) {
        r.h = src->height - (uint32_t)r.y;
    }

    return r;
}

void surface_copy(struct surface *dst, struct surface *src, const struct rect *src_rect, uint32_t x, uint32_t y) {
    struct rect r = source_rect(src, src_rect);

    if (!dst || !src || !r.w || !r.h) {
        return;
    }

    if (dst->bpp == 32 && src->bpp == 32 && surface_match(dst, src)) {
        for (uint32_t row = 0; row < r.h; row++) {
            uint32_t dy = y + row;
            uint32_t count = r.w;
            uint8_t *d;
            const uint8_t *s;

            if (dy >= dst->height || x >= dst->width) {
                break;
            }
            if (x + count > dst->width) {
                count = dst->width - x;
            }
            d = dst->pixels + (size_t)dy * dst->pitch + (size_t)x * 4u;
            s = src->pixels + ((uint32_t)r.y + row) * src->pitch + (uint32_t)r.x * 4u;
            memcpy(d, s, count * 4u);
        }
        return;
    }

    for (uint32_t row = 0; row < r.h; row++) {
        uint32_t dy = y + row;
        if (dy >= dst->height) {
            break;
        }
        for (uint32_t col = 0; col < r.w; col++) {
            uint32_t dx = x + col;
            if (dx >= dst->width) {
                break;
            }
            surface_set(dst, dx, dy, surface_get(src, (uint32_t)r.x + col, (uint32_t)r.y + row));
        }
    }
}

void surface_blit_key(struct surface *dst, struct surface *src, const struct rect *src_rect, uint32_t x, uint32_t y, uint32_t key) {
    struct rect r = source_rect(src, src_rect);

    if (!dst || !src || !r.w || !r.h) {
        return;
    }

    if (dst->bpp == 32 && src->bpp == 32 && surface_match(dst, src)) {
        uint32_t rawkey = surface_pack(src, key);

        for (uint32_t row = 0; row < r.h; row++) {
            uint32_t dy = y + row;
            uint32_t count = r.w;
            uint32_t col = 0;
            const uint32_t *s;
            uint32_t *d;

            if (dy >= dst->height || x >= dst->width) {
                break;
            }
            if (x + count > dst->width) {
                count = dst->width - x;
            }

            s = (const uint32_t *)(src->pixels + ((uint32_t)r.y + row) * src->pitch + (uint32_t)r.x * 4u);
            d = (uint32_t *)(dst->pixels + (size_t)dy * dst->pitch + (size_t)x * 4u);

            while (col < count) {
                uint32_t start;
                while (col < count && s[col] == rawkey) {
                    col++;
                }
                start = col;
                while (col < count && s[col] != rawkey) {
                    col++;
                }
                if (col > start) {
                    memcpy(d + start, s + start, (col - start) * 4u);
                }
            }
        }
        return;
    }

    for (uint32_t row = 0; row < r.h; row++) {
        uint32_t dy = y + row;
        if (dy >= dst->height) {
            break;
        }
        for (uint32_t col = 0; col < r.w; col++) {
            uint32_t dx = x + col;
            uint32_t color;
            if (dx >= dst->width) {
                break;
            }
            color = surface_get(src, (uint32_t)r.x + col, (uint32_t)r.y + row);
            if (color != key) {
                surface_set(dst, dx, dy, color);
            }
        }
    }
}

void surface_blit_alpha(struct surface *dst, struct surface *src, const struct rect *src_rect, uint32_t x, uint32_t y, uint8_t alpha) {
    struct rect r = source_rect(src, src_rect);

    if (!dst || !src || !r.w || !r.h) {
        return;
    }

    if (dst->fast_rgb32 && src->fast_rgb32) {
        for (uint32_t row = 0; row < r.h; row++) {
            uint32_t dy = y + row;
            uint32_t count = r.w;
            const uint32_t *s;
            uint32_t *d;

            if (dy >= dst->height || x >= dst->width) {
                break;
            }
            if (x + count > dst->width) {
                count = dst->width - x;
            }

            s = (const uint32_t *)(src->pixels + ((uint32_t)r.y + row) * src->pitch + (uint32_t)r.x * 4u);
            d = (uint32_t *)(dst->pixels + (size_t)dy * dst->pitch + (size_t)x * 4u);

            /* Blending still has to touch every pixel (unlike a plain copy),
               but skips the bounds/format re-checks that surface_get/
               surface_set would otherwise redo per pixel here. */
            for (uint32_t col = 0; col < count; col++) {
                d[col] = pixel_blend(d[col] & 0x00FFFFFFu, s[col] & 0x00FFFFFFu, alpha);
            }
        }
        return;
    }

    for (uint32_t row = 0; row < r.h; row++) {
        uint32_t dy = y + row;
        if (dy >= dst->height) {
            break;
        }
        for (uint32_t col = 0; col < r.w; col++) {
            uint32_t dx = x + col;
            if (dx >= dst->width) {
                break;
            }
            surface_set(dst, dx, dy, pixel_blend(surface_get(dst, dx, dy), surface_get(src, (uint32_t)r.x + col, (uint32_t)r.y + row), alpha));
        }
    }
}
