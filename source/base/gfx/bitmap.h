#ifndef MONARCH_BASE_GFX_BITMAP_H
#define MONARCH_BASE_GFX_BITMAP_H 1

/**
 * @file bitmap.h
 * @brief BMP file loader for BGL surfaces.
 *
 * BMP is useful for OS development because it is simple and many tools can
 * export it.  The loader supports uncompressed Windows BMP files and converts
 * them into Monarch linear RGB surfaces.  Paletted 1/4/8-bit images are
 * expanded through their palette; 24/32-bit images are read from BGR/BGRA byte
 * order.
 */

#include "base/gfx/surface.h"

/**
 * Decode a BMP image from memory.
 *
 * @param data Pointer to the whole BMP file.
 * @param size Size of the BMP file in bytes.
 * @return Newly allocated surface on success, or nil on invalid/unsupported BMP.
 */
struct surface *bitmap_load(const void *data, size_t size);

#endif /* MONARCH_BASE_GFX_BITMAP_H */
