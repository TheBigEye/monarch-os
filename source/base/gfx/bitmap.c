/**
 * @file bitmap.c
 * @brief BMP image loader. It decodes uncompressed BMP files into linear RGB surfaces and handles palettes, BGR ordering, row padding, and bottom-up storage.
 */

#include "base/gfx/bitmap.h"

#define BI_RGB 0u

static uint16_t le16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int32_t sle32(const uint8_t *p) {
    return (int32_t)le32(p);
}

static uint32_t palette(const uint8_t *pal, uint32_t index, uint32_t colors) {
    if (index >= colors) {
        return 0;
    }
    const uint8_t *c = pal + index * 4u;
    return ((uint32_t)c[2] << 16) | ((uint32_t)c[1] << 8) | c[0];
}

/**
 * Decode an in-memory BMP file.
 *
 * The loader intentionally supports only uncompressed BI_RGB files.  That keeps
 * the implementation small while still covering the common BMP files exported
 * by many tools.  Paletted BMPs store color indices, so the loader expands each
 * pixel to logical 0x00RRGGBB before returning the surface.
 */
struct surface *bitmap_load(const void *data, size_t size) {
    const uint8_t *bmp = data;
    uint32_t pixel_offset;
    uint32_t dib_size;
    int32_t width_signed;
    int32_t height_signed;
    uint32_t width;
    uint32_t height;
    uint16_t planes;
    uint16_t bpp;
    uint32_t compression;
    uint32_t colors_used;
    uint32_t rowbytes;
    const uint8_t *pixels;
    const uint8_t *pal;
    uint32_t colors;
    int topdown;
    struct surface *out;

    if (!bmp || size < 54 || bmp[0] != 'B' || bmp[1] != 'M') {
        return nil;
    }

    pixel_offset = le32(bmp + 10);
    dib_size = le32(bmp + 14);
    if (dib_size < 40 || 14u + dib_size > size || pixel_offset > size) {
        return nil;
    }

    width_signed = sle32(bmp + 18);
    height_signed = sle32(bmp + 22);
    planes = le16(bmp + 26);
    bpp = le16(bmp + 28);
    compression = le32(bmp + 30);
    colors_used = le32(bmp + 46);

    if (planes != 1 || compression != BI_RGB || width_signed <= 0 || height_signed == 0) {
        return nil;
    }

    width = (uint32_t)width_signed;
    topdown = height_signed < 0;
    height = (uint32_t)(topdown ? -height_signed : height_signed);

    if (width > 2048 || height > 2048) {
        return nil;
    }

    if (!(bpp == 1 || bpp == 4 || bpp == 8 || bpp == 24 || bpp == 32)) {
        return nil;
    }

    /* BMP scanlines are padded to a 4-byte boundary.  This is one of
       the most common BMP parsing mistakes: width * bytes_per_pixel is not
       always the distance to the next row. */
    rowbytes = ((width * bpp + 31u) / 32u) * 4u;
    if (pixel_offset + (size_t)rowbytes * height > size) {
        return nil;
    }

    colors = 0;
    pal = nil;
    /* Indexed BMPs keep a BGRA palette before the pixel array.  The
       pixel data stores palette indices, not RGB values. */
    if (bpp <= 8) {
        colors = colors_used ? colors_used : (1u << bpp);
        pal = bmp + 14u + dib_size;
        if ((uintptr_t)(pal + colors * 4u) > (uintptr_t)(bmp + pixel_offset)) {
            return nil;
        }
    }

    out = surface_create(width, height);
    if (!out) {
        return nil;
    }

    pixels = bmp + pixel_offset;

    /* Positive BMP heights are stored bottom-up: the first row in the
       file is the bottom row of the image.  Negative heights are top-down. */
    for (uint32_t y = 0; y < height; y++) {
        uint32_t sy = topdown ? y : (height - 1u - y);
        const uint8_t *row = pixels + sy * rowbytes;

        for (uint32_t x = 0; x < width; x++) {
            uint32_t color = 0;

            if (bpp == 32) {
                const uint8_t *p = row + x * 4u;
                color = ((uint32_t)p[2] << 16) | ((uint32_t)p[1] << 8) | p[0];
            } else if (bpp == 24) {
                const uint8_t *p = row + x * 3u;
                color = ((uint32_t)p[2] << 16) | ((uint32_t)p[1] << 8) | p[0];
            } else if (bpp == 8) {
                color = palette(pal, row[x], colors);
            } else if (bpp == 4) {
                uint8_t v = row[x >> 1];
                uint8_t idx = (x & 1u) ? (v & 0x0Fu) : (v >> 4);
                color = palette(pal, idx, colors);
            } else if (bpp == 1) {
                uint8_t v = row[x >> 3];
                uint8_t idx = (uint8_t)((v >> (7u - (x & 7u))) & 1u);
                color = palette(pal, idx, colors);
            }

            surface_set(out, x, y, color);
        }
    }

    return out;
}
