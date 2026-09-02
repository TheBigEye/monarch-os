/**
 * @file image.c
 * @brief PPM P3 image loader. PPM is a very simple text image format, useful for early OS graphics because it can be parsed without compression libraries.
 */

#include "base/gfx/image.h"

static int whitespace(char c) {
    return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

/** Skip whitespace and comments in the PPM token stream. */
static void skip(const char **p, const char *end) {
    for (;;) {
        while (*p < end && whitespace(**p)) {
            (*p)++;
        }
        if (*p < end && **p == '#') {
            while (*p < end && **p != '\n') {
                (*p)++;
            }
            continue;
        }
        break;
    }
}

/** Read the next whitespace-delimited PPM token into a small buffer. */
static int token(const char **p, const char *end, char *out, size_t size) {
    size_t used = 0;

    skip(p, end);
    if (*p >= end) {
        return 0;
    }

    while (*p < end && !whitespace(**p)) {
        if (used + 1 < size) {
            out[used++] = **p;
        }
        (*p)++;
    }
    out[used] = '\0';
    return used != 0;
}

/**
 * Decode a PPM P3 image.
 *
 * P3 stores pixels as ASCII decimal RGB triplets.  It is large and slow, but
 * extremely easy to generate and inspect by hand, which makes it ideal for
 * early tests before compressed image formats exist.
 */
struct surface *image_ppm(const char *data, size_t size) {
    const char *p;
    const char *end;
    char tok[32];
    int width;
    int height;
    int maxval;
    struct surface *img;

    if (!data || size < 3) {
        return nil;
    }

    p = data;
    end = data + size;

    if (!token(&p, end, tok, sizeof(tok)) || strcmp(tok, "P3") != 0) {
        return nil;
    }
    if (!token(&p, end, tok, sizeof(tok))) {
        return nil;
    }
    width = atoi(tok);
    if (!token(&p, end, tok, sizeof(tok))) {
        return nil;
    }
    height = atoi(tok);
    if (!token(&p, end, tok, sizeof(tok))) {
        return nil;
    }
    maxval = atoi(tok);

    if (width <= 0 || height <= 0 || maxval <= 0 || width > 512 || height > 512) {
        return nil;
    }

    img = surface_create((uint32_t)width, (uint32_t)height);
    if (!img) {
        return nil;
    }

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int rgb[3];
            for (int i = 0; i < 3; i++) {
                if (!token(&p, end, tok, sizeof(tok))) {
                    surface_destroy(img);
                    return nil;
                }
                rgb[i] = atoi(tok);
                if (rgb[i] < 0) rgb[i] = 0;
                if (rgb[i] > maxval) rgb[i] = maxval;
                rgb[i] = (rgb[i] * 255) / maxval;
            }
            surface_set(img, (uint32_t)x, (uint32_t)y, ((uint32_t)rgb[0] << 16) | ((uint32_t)rgb[1] << 8) | (uint32_t)rgb[2]);
        }
    }

    return img;
}
