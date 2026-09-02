/**
 * @file gfx_fb.c
 * @brief Adapter between the kernel framebuffer driver and base/gfx surfaces.
 */

#include "base/sys/gfx_fb.h"
#include "base/sys/memory.h"

void *gfx_platform_alloc(size_t size) {
    return alloc(size);
}

void *gfx_platform_zero(size_t count, size_t size) {
    return zero(count, size);
}

void gfx_platform_free(void *ptr) {
    release(ptr);
}

int gfx_framebuffer_format(struct framebuffer *fb, struct pixel_format *out) {
    if (!fb || !fb->ready(fb) || !out) {
        return 0;
    }

    memset(out, 0, sizeof(*out));
    out->bpp = fb->bpp;
    out->red_position = fb->red_position;
    out->red_mask_size = fb->red_mask_size;
    out->green_position = fb->green_position;
    out->green_mask_size = fb->green_mask_size;
    out->blue_position = fb->blue_position;
    out->blue_mask_size = fb->blue_mask_size;
    return 1;
}

int gfx_framebuffer_surface(struct framebuffer *fb, struct surface *out) {
    struct pixel_format format;

    if (!gfx_framebuffer_format(fb, &format) || !out) {
        return 0;
    }

    surface_view(out, fb->width, fb->height, fb->pitch, fb->bpp, fb->address, &format);
    return 1;
}

struct surface *gfx_surface_native(struct framebuffer *fb, uint32_t width, uint32_t height) {
    struct pixel_format format;

    if (!gfx_framebuffer_format(fb, &format)) {
        return surface_create(width, height);
    }
    return surface_native(width, height, &format);
}

struct surface *gfx_surface_convert(struct framebuffer *fb, struct surface *src) {
    struct pixel_format format;

    if (!gfx_framebuffer_format(fb, &format)) {
        return nil;
    }
    return surface_convert(src, &format);
}

void render_framebuffer(struct renderer *r, struct framebuffer *fb) {
    if (!r) {
        return;
    }

    memset(r, 0, sizeof(*r));
    if (!gfx_framebuffer_surface(fb, &r->_view)) {
        render_target(r, nil);
        return;
    }
    render_target(r, &r->_view);
}
