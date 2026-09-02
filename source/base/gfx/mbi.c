/**
 * @file mbi.c
 * @brief Monarch Bitmap Image loader. MBI is Monarch’s native, simple image container for framebuffer-friendly assets.
 */

#include "base/gfx/mbi.h"

static uint32_t le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int load_xrgb(struct surface *surface, const uint8_t *payload, uint32_t payload_size) {
    if (payload_size != surface->pitch * surface->height) {
        return 0;
    }
    memcpy(surface->pixels, payload, payload_size);
    return 1;
}

/** Expand an INDEX8 payload: one byte index per pixel plus an RGB palette. */
static int load_index8(struct surface *surface, const uint8_t *payload, uint32_t payload_size, uint32_t pitch) {
    uint32_t colors;
    const uint8_t *palette;
    const uint8_t *indices;

    if (payload_size < 4) {
        return 0;
    }

    colors = le32(payload);
    if (!colors || colors > 256 || payload_size < 4u + colors * 4u) {
        return 0;
    }

    palette = payload + 4;
    indices = palette + colors * 4u;
    if (payload_size < 4u + colors * 4u + pitch * surface->height) {
        return 0;
    }

    for (uint32_t y = 0; y < surface->height; y++) {
        const uint8_t *row = indices + y * pitch;
        for (uint32_t x = 0; x < surface->width; x++) {
            uint8_t idx = row[x];
            if (idx >= colors) {
                return 0;
            }
            surface_set(surface, x, y, le32(palette + idx * 4u) & 0x00FFFFFFu);
        }
    }

    return 1;
}

/** Expand an INDEX4 payload: two palette indices packed into each byte. */
static int load_index4(struct surface *surface, const uint8_t *payload, uint32_t payload_size, uint32_t pitch) {
    uint32_t colors;
    const uint8_t *palette;
    const uint8_t *indices;

    if (payload_size < 4) {
        return 0;
    }

    colors = le32(payload);
    if (!colors || colors > 16 || payload_size < 4u + colors * 4u) {
        return 0;
    }

    palette = payload + 4;
    indices = palette + colors * 4u;
    if (payload_size < 4u + colors * 4u + pitch * surface->height) {
        return 0;
    }

    for (uint32_t y = 0; y < surface->height; y++) {
        const uint8_t *row = indices + y * pitch;
        for (uint32_t x = 0; x < surface->width; x++) {
            uint8_t v = row[x >> 1];
            uint8_t idx = (x & 1u) ? (v & 0x0Fu) : (v >> 4);
            if (idx >= colors) {
                return 0;
            }
            surface_set(surface, x, y, le32(palette + idx * 4u) & 0x00FFFFFFu);
        }
    }

    return 1;
}

/**
 * Decode a Monarch Bitmap Image.
 *
 * MBI is much simpler than BMP: it has one fixed header and a small number of
 * payload formats.  Indexed formats stay compact in the initrd but expand to a
 * normal 32-bit surface for easy rendering.
 */
struct surface *mbi_load(const void *data, size_t size) {
    const uint8_t *bytes = data;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t format;
    uint32_t payload_size;
    struct surface *surface;
    const uint8_t *payload;
    int ok = 0;

    if (!bytes || size < 24 || memcmp(bytes, "MBI1", 4) != 0) {
        return nil;
    }

    width = le32(bytes + 4);
    height = le32(bytes + 8);
    pitch = le32(bytes + 12);
    format = le32(bytes + 16);
    payload_size = le32(bytes + 20);
    payload = bytes + 24;

    if (!width || !height || 24u + (size_t)payload_size > size || width > 4096 || height > 4096) {
        return nil;
    }

    if (format == MBI_FORMAT_XRGB8888 && pitch != width * 4u) {
        return nil;
    }
    if (format == MBI_FORMAT_INDEX8 && pitch != width) {
        return nil;
    }
    if (format == MBI_FORMAT_INDEX4 && pitch != (width + 1u) / 2u) {
        return nil;
    }

    surface = surface_create(width, height);
    if (!surface) {
        return nil;
    }

    if (format == MBI_FORMAT_XRGB8888) {
        ok = load_xrgb(surface, payload, payload_size);
    } else if (format == MBI_FORMAT_INDEX8) {
        ok = load_index8(surface, payload, payload_size, pitch);
    } else if (format == MBI_FORMAT_INDEX4) {
        ok = load_index4(surface, payload, payload_size, pitch);
    }

    if (!ok) {
        surface_destroy(surface);
        return nil;
    }

    return surface;
}
