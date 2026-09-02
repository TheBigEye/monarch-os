#ifndef MONARCH_BASE_FBD_FBD_H
#define MONARCH_BASE_FBD_FBD_H 1

/**
 * @file fbd.h
 * @brief Tiny userspace helper for Monarch's /dev/fb0 framebuffer device.
 *
 * FBD means Frame Buffer Device.  This library is intentionally separate from
 * `base/gfx`: gfx/BGL is the higher-level, flexible drawing stack, while FBD is
 * a small Unix-style wrapper around `/dev/fb0` and `/dev/fbinfo` for simple
 * userspace programs.
 */

#include "base/usr/sys.h"

#ifndef MONARCH_USER_BUILD
#error "base/fbd is userspace-only; kernel code must use drivers/video/framebuffer.h instead."
#endif

struct fbd {
    int fd;
    uint32_t ready;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
    uint32_t bytes;
    uint32_t red_pos;
    uint32_t red_size;
    uint32_t green_pos;
    uint32_t green_size;
    uint32_t blue_pos;
    uint32_t blue_size;
};

/** Open /dev/fb0 and load metadata from /dev/fbinfo. Returns non-zero on success. */
int fbd_open(struct fbd *fb);

/** Close an opened framebuffer device. Safe to call on a zeroed/closed object. */
void fbd_close(struct fbd *fb);

/** Return non-zero when the object describes an opened 32-bit framebuffer. */
int fbd_ready(const struct fbd *fb);

/** Pack RGB bytes into the framebuffer's native 32-bit pixel format. */
uint32_t fbd_rgb(const struct fbd *fb, uint8_t r, uint8_t g, uint8_t b);

/** Pack a logical 0x00RRGGBB color into the framebuffer's native format. */
uint32_t fbd_color(const struct fbd *fb, uint32_t rgb);

/** Parse a simple hexadecimal color string: RRGGBB, #RRGGBB or 0xRRGGBB. */
int fbd_parse_color(const char *text, uint32_t *out_rgb);

/** Write `count` native 32-bit pixels at row `y`, starting at pixel `x`. */
int fbd_write_row(struct fbd *fb, uint32_t x, uint32_t y, const uint32_t *pixels, uint32_t count);

/** Draw one native 32-bit pixel. */
int fbd_pixel(struct fbd *fb, uint32_t x, uint32_t y, uint32_t color);

/** Fill a rectangle with one native 32-bit color. */
int fbd_fill_rect(struct fbd *fb, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);

/** Fill the whole framebuffer with one native 32-bit color. */
int fbd_clear(struct fbd *fb, uint32_t color);

#endif /* MONARCH_BASE_FBD_FBD_H */
