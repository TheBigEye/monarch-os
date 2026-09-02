#include "drivers/video/framebuffer.h"
#include "arch/x86/paging.h"

static struct framebuffer *active;

static uint32_t mask(uint8_t size) {
    if (size >= 32) {
        return 0xFFFFFFFFu;
    }
    return (1u << size) - 1u;
}

static uint8_t expand(uint32_t value, uint8_t size) {
    uint32_t m = mask(size);
    if (!m) {
        return 0;
    }
    return (uint8_t)((value * 255u) / m);
}

static uint32_t rgb_impl(struct framebuffer *self, uint8_t r, uint8_t g, uint8_t b) {
    if (!self || self->type != 1) {
        return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    }

    return (((uint32_t)(r >> (8 - self->red_mask_size)) & mask(self->red_mask_size)) << self->red_position) |
           (((uint32_t)(g >> (8 - self->green_mask_size)) & mask(self->green_mask_size)) << self->green_position) |
           (((uint32_t)(b >> (8 - self->blue_mask_size)) & mask(self->blue_mask_size)) << self->blue_position);
}

static int ready_impl(struct framebuffer *self) {
    return self && self->address && self->width && self->height && self->pitch && (self->bpp == 32 || self->bpp == 24 || self->bpp == 16);
}

static void pixel_impl(struct framebuffer *self, uint32_t x, uint32_t y, uint32_t color) {
    uint8_t *p;

    if (!ready_impl(self) || x >= self->width || y >= self->height) {
        return;
    }

    p = self->address + y * self->pitch + x * (self->bpp / 8u);

    if (self->bpp == 32) {
        *(uint32_t *)p = color;
    } else if (self->bpp == 24) {
        p[0] = (uint8_t)(color & 0xFF);
        p[1] = (uint8_t)((color >> 8) & 0xFF);
        p[2] = (uint8_t)((color >> 16) & 0xFF);
    } else if (self->bpp == 16) {
        *(uint16_t *)p = (uint16_t)color;
    }
}

static void clear_impl(struct framebuffer *self, uint32_t color) {
    if (!ready_impl(self)) {
        return;
    }

    for (uint32_t y = 0; y < self->height; y++) {
        for (uint32_t x = 0; x < self->width; x++) {
            pixel_impl(self, x, y, color);
        }
    }
}

static void rect_impl(struct framebuffer *self, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    if (!ready_impl(self)) {
        return;
    }

    if (x >= self->width || y >= self->height) {
        return;
    }

    if (x + w > self->width) {
        w = self->width - x;
    }
    if (y + h > self->height) {
        h = self->height - y;
    }

    for (uint32_t row = 0; row < h; row++) {
        for (uint32_t col = 0; col < w; col++) {
            pixel_impl(self, x + col, y + row, color);
        }
    }
}

static void blit_impl(struct framebuffer *self, uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint32_t *pixels) {
    if (!ready_impl(self) || !pixels) {
        return;
    }

    for (uint32_t row = 0; row < h; row++) {
        for (uint32_t col = 0; col < w; col++) {
            pixel_impl(self, x + col, y + row, pixels[row * w + col]);
        }
    }
}

static uint32_t get_impl(struct framebuffer *self, uint32_t x, uint32_t y) {
    uint8_t *p;
    uint32_t native;
    uint8_t r;
    uint8_t g;
    uint8_t b;

    if (!ready_impl(self) || x >= self->width || y >= self->height) {
        return 0;
    }

    p = self->address + y * self->pitch + x * (self->bpp / 8u);

    if (self->bpp == 32) {
        native = *(uint32_t *)p;
    } else if (self->bpp == 24) {
        native = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
    } else if (self->bpp == 16) {
        native = *(uint16_t *)p;
    } else {
        return 0;
    }

    if (self->type == 1) {
        r = expand((native >> self->red_position) & mask(self->red_mask_size), self->red_mask_size);
        g = expand((native >> self->green_position) & mask(self->green_mask_size), self->green_mask_size);
        b = expand((native >> self->blue_position) & mask(self->blue_mask_size), self->blue_mask_size);
        return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    }

    return native & 0x00FFFFFFu;
}

static int mapfb(struct framebuffer *fb) {
    uintptr_t start = fb->physical & ~(uintptr_t)(PAGE_SIZE - 1u);
    uintptr_t end = alignup(fb->physical + fb->bytes, PAGE_SIZE);

    if (!pagingactive()) {
        return 1;
    }

    for (uintptr_t address = start; address < end; address += PAGE_SIZE) {
        if (!pagemap(address, address, PAGE_WRITE)) {
            return 0;
        }
    }

    return 1;
}

void framebuffer(struct framebuffer *fb, uintptr_t bootinfo) {
    const struct mb2framebuffer *tag;

    memset(fb, 0, sizeof(*fb));
    fb->clear = clear_impl;
    fb->pixel = pixel_impl;
    fb->rect = rect_impl;
    fb->blit = blit_impl;
    fb->get = get_impl;
    fb->rgb = rgb_impl;
    fb->ready = ready_impl;

    tag = (const struct mb2framebuffer *)mb2find(bootinfo, MB2_TAG_FRAMEBUFFER);
    if (!tag || tag->framebuffer_addr > 0xFFFFFFFFull) {
        return;
    }

    fb->physical = (uintptr_t)tag->framebuffer_addr;
    fb->address = (uint8_t *)fb->physical;
    fb->width = tag->framebuffer_width;
    fb->height = tag->framebuffer_height;
    fb->pitch = tag->framebuffer_pitch;
    fb->bpp = tag->framebuffer_bpp;
    fb->type = tag->framebuffer_type;
    fb->red_position = tag->red_field_position;
    fb->red_mask_size = tag->red_mask_size;
    fb->green_position = tag->green_field_position;
    fb->green_mask_size = tag->green_mask_size;
    fb->blue_position = tag->blue_field_position;
    fb->blue_mask_size = tag->blue_mask_size;
    fb->bytes = (size_t)fb->pitch * fb->height;

    if (!mapfb(fb)) {
        memset(fb, 0, sizeof(*fb));
        return;
    }

    active = fb;
}

struct framebuffer *framebuffer_get(void) {
    return active && active->ready(active) ? active : nil;
}
