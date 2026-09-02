/**
 * @file fbd.c
 * @brief Tiny userspace implementation for Monarch's framebuffer device.
 */

#include "base/fbd/fbd.h"

#define FBD_INFO_SIZE 256u
/* fbd_fill_rect() issues one lseek+write pair per chunk. At 64 pixels, a
   1920px-wide row needed 30 syscall pairs; a 2048-pixel (8KB) chunk covers
   any screen width this OS is likely to run at in a single write, and
   still falls back to looping correctly for anything wider. */
#define FBD_CHUNK_PIXELS 2048u

static uint32_t chunk[FBD_CHUNK_PIXELS];

static const char *find_key(const char *text, const char *key) {
    size_t len = strlen(key);

    while (text && *text) {
        if (strncmp(text, key, len) == 0 && text[len] == '=') {
            return text + len + 1u;
        }
        text++;
    }
    return nil;
}

static uint32_t read_key(const char *text, const char *key, uint32_t fallback) {
    const char *value = find_key(text, key);
    uint32_t out = 0;

    if (!value) {
        return fallback;
    }

    while (*value >= '0' && *value <= '9') {
        out = out * 10u + (uint32_t)(*value - '0');
        value++;
    }
    return out;
}

static int read_info_text(char *text, size_t size) {
    long fd;
    long got;

    if (!text || size < 2) {
        return 0;
    }

    fd = open("/dev/fbinfo", OREAD);
    if (fd < 0) {
        return 0;
    }

    got = read((int)fd, text, size - 1u);
    close((int)fd);
    if (got < 0) {
        return 0;
    }
    text[got] = '\0';
    return 1;
}

static void load_info(struct fbd *fb, const char *text) {
    fb->ready = read_key(text, "ready", 0);
    fb->width = read_key(text, "width", 0);
    fb->height = read_key(text, "height", 0);
    fb->pitch = read_key(text, "pitch", 0);
    fb->bpp = read_key(text, "bpp", 0);
    fb->bytes = read_key(text, "bytes", fb->pitch * fb->height);
    fb->red_pos = read_key(text, "red_pos", 16);
    fb->red_size = read_key(text, "red_size", 8);
    fb->green_pos = read_key(text, "green_pos", 8);
    fb->green_size = read_key(text, "green_size", 8);
    fb->blue_pos = read_key(text, "blue_pos", 0);
    fb->blue_size = read_key(text, "blue_size", 8);
}

int fbd_open(struct fbd *fb) {
    char text[FBD_INFO_SIZE];

    if (!fb) {
        return 0;
    }

    memset(fb, 0, sizeof(*fb));
    fb->fd = -1;

    if (!read_info_text(text, sizeof(text))) {
        return 0;
    }

    load_info(fb, text);
    if (!fbd_ready(fb)) {
        return 0;
    }

    fb->fd = (int)open("/dev/fb0", OREAD | OWRITE);
    if (fb->fd < 0) {
        return 0;
    }

    return 1;
}

void fbd_close(struct fbd *fb) {
    if (fb && fb->fd >= 0) {
        close(fb->fd);
        fb->fd = -1;
    }
}

int fbd_ready(const struct fbd *fb) {
    return fb && fb->ready && fb->width && fb->height && fb->pitch && fb->bpp == 32u;
}

static uint32_t channel(uint8_t value, uint32_t size, uint32_t pos) {
    uint32_t max;

    if (!size) {
        return 0;
    }
    if (size >= 8u) {
        return (uint32_t)value << pos;
    }
    max = (1u << size) - 1u;
    return (((uint32_t)value * max) / 255u) << pos;
}

uint32_t fbd_rgb(const struct fbd *fb, uint8_t r, uint8_t g, uint8_t b) {
    if (!fb) {
        return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    }

    return channel(r, fb->red_size, fb->red_pos) |
           channel(g, fb->green_size, fb->green_pos) |
           channel(b, fb->blue_size, fb->blue_pos);
}

uint32_t fbd_color(const struct fbd *fb, uint32_t rgb) {
    return fbd_rgb(fb, (uint8_t)(rgb >> 16), (uint8_t)(rgb >> 8), (uint8_t)rgb);
}

static int hex_value(char ch, uint32_t *out) {
    if (ch >= '0' && ch <= '9') {
        *out = (uint32_t)(ch - '0');
        return 1;
    }
    if (ch >= 'a' && ch <= 'f') {
        *out = 10u + (uint32_t)(ch - 'a');
        return 1;
    }
    if (ch >= 'A' && ch <= 'F') {
        *out = 10u + (uint32_t)(ch - 'A');
        return 1;
    }
    return 0;
}

int fbd_parse_color(const char *text, uint32_t *out_rgb) {
    uint32_t value = 0;
    size_t digits = 0;

    if (!text || !out_rgb) {
        return 0;
    }

    if (*text == '#') {
        text++;
    } else if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        text += 2;
    }

    while (*text) {
        uint32_t nibble;
        if (!hex_value(*text, &nibble)) {
            return 0;
        }
        value = (value << 4) | nibble;
        digits++;
        if (digits > 6u) {
            return 0;
        }
        text++;
    }

    if (!digits) {
        return 0;
    }
    *out_rgb = value & 0x00FFFFFFu;
    return 1;
}

int fbd_write_row(struct fbd *fb, uint32_t x, uint32_t y, const uint32_t *pixels, uint32_t count) {
    size_t offset;
    size_t bytes;

    if (!fbd_ready(fb) || fb->fd < 0 || !pixels || y >= fb->height || x >= fb->width) {
        return 0;
    }

    if (x + count > fb->width) {
        count = fb->width - x;
    }
    if (!count) {
        return 1;
    }

    offset = (size_t)y * fb->pitch + (size_t)x * 4u;
    bytes = (size_t)count * 4u;
    if (lseek(fb->fd, (long)offset, SEEK_SET) < 0) {
        return 0;
    }
    return write(fb->fd, pixels, bytes) == (long)bytes;
}

int fbd_pixel(struct fbd *fb, uint32_t x, uint32_t y, uint32_t color) {
    return fbd_write_row(fb, x, y, &color, 1);
}

int fbd_fill_rect(struct fbd *fb, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    if (!fbd_ready(fb) || x >= fb->width || y >= fb->height) {
        return 0;
    }

    if (x + w > fb->width) {
        w = fb->width - x;
    }
    if (y + h > fb->height) {
        h = fb->height - y;
    }
    if (!w || !h) {
        return 1;
    }

    for (uint32_t i = 0; i < FBD_CHUNK_PIXELS; i++) {
        chunk[i] = color;
    }

    for (uint32_t row = 0; row < h; row++) {
        uint32_t done = 0;
        while (done < w) {
            uint32_t part = min(w - done, FBD_CHUNK_PIXELS);
            if (!fbd_write_row(fb, x + done, y + row, chunk, part)) {
                return 0;
            }
            done += part;
        }
    }
    return 1;
}

int fbd_clear(struct fbd *fb, uint32_t color) {
    return fbd_fill_rect(fb, 0, 0, fb ? fb->width : 0, fb ? fb->height : 0, color);
}
