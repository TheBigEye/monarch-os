#ifndef MONARCH_BASE_GFX_MBI_H
#define MONARCH_BASE_GFX_MBI_H 1

/**
 * @file mbi.h
 * @brief Monarch Bitmap Image loader.
 *
 * MBI is Monarch's native image container.  It is designed to be much simpler
 * for the kernel to load than BMP while still supporting compact indexed image
 * data.  The build script `scripts/mkmbi.py` can convert BMP and PPM assets into
 * this format.
 */

#include "base/gfx/surface.h"

/** Raw 32-bit pixels stored as little-endian 0x00RRGGBB. */
#define MBI_FORMAT_XRGB8888 1u
/** 8-bit indices plus a palette of 0x00RRGGBB colors. */
#define MBI_FORMAT_INDEX8   2u
/** Packed 4-bit indices plus a palette of up to 16 colors. */
#define MBI_FORMAT_INDEX4   3u

/** On-disk/in-memory MBI header.  All integer fields are little-endian. */
struct mbiheader {
    char magic[4];
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t format;
    uint32_t size;
};

/** Decode an MBI image into a newly allocated 32-bit RGB surface. */
struct surface *mbi_load(const void *data, size_t size);

#endif /* MONARCH_BASE_GFX_MBI_H */
