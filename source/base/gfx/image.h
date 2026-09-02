#ifndef MONARCH_BASE_GFX_IMAGE_H
#define MONARCH_BASE_GFX_IMAGE_H 1

/**
 * @file image.h
 * @brief Small generic image helpers.
 *
 * At this stage this file exposes a PPM P3 loader.  PPM P3 is a text format,
 * so it is not space efficient, but it is extremely easy to parse and useful for
 * tests and tiny generated assets.
 */

#include "base/gfx/surface.h"

/** Decode a text PPM P3 image into a newly allocated RGB surface. */
struct surface *image_ppm(const char *data, size_t size);

#endif /* MONARCH_BASE_GFX_IMAGE_H */
