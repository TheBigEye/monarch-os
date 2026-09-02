#ifndef MONARCH_DRIVERS_VIDEO_FRAMEBUFFER_H
#define MONARCH_DRIVERS_VIDEO_FRAMEBUFFER_H 1

#include "boot/bootloader.h"
#include "base/api/monarch.h"

struct framebuffer {
    void (*clear)(struct framebuffer *self, uint32_t color);
    void (*pixel)(struct framebuffer *self, uint32_t x, uint32_t y, uint32_t color);
    void (*rect)(struct framebuffer *self, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
    void (*blit)(struct framebuffer *self, uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint32_t *pixels);
    uint32_t (*get)(struct framebuffer *self, uint32_t x, uint32_t y);
    uint32_t (*rgb)(struct framebuffer *self, uint8_t r, uint8_t g, uint8_t b);
    int (*ready)(struct framebuffer *self);

    uintptr_t physical;
    uint8_t *address;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint8_t bpp;
    uint8_t type;
    uint8_t red_position;
    uint8_t red_mask_size;
    uint8_t green_position;
    uint8_t green_mask_size;
    uint8_t blue_position;
    uint8_t blue_mask_size;
    size_t bytes;
};

void framebuffer(struct framebuffer *fb, uintptr_t bootinfo);
struct framebuffer *framebuffer_get(void);

#endif /* MONARCH_DRIVERS_VIDEO_FRAMEBUFFER_H */
