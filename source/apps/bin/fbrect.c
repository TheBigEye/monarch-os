/**
 * @file fbrect.c
 * @brief Draw one filled rectangle on /dev/fb0 using FBD.
 */

#include "base/fbd/fbd.h"

int main(int argc, char **argv) {
    struct fbd fb;
    uint32_t x;
    uint32_t y;
    uint32_t w;
    uint32_t h;
    uint32_t rgb;

    if (argc != 6) {
        eputs("usage: fbrect X Y W H RRGGBB\n");
        return 1;
    }

    x = (uint32_t)atoi(argv[1]);
    y = (uint32_t)atoi(argv[2]);
    w = (uint32_t)atoi(argv[3]);
    h = (uint32_t)atoi(argv[4]);
    if (!fbd_parse_color(argv[5], &rgb)) {
        eputs("fbrect: invalid color, expected RRGGBB\n");
        return 1;
    }

    if (!fbd_open(&fb)) {
        eputs("fbrect: cannot open ready 32-bit framebuffer\n");
        return 1;
    }

    if (!fbd_fill_rect(&fb, x, y, w, h, fbd_color(&fb, rgb))) {
        eputs("fbrect: draw failed\n");
        fbd_close(&fb);
        return 1;
    }

    fbd_close(&fb);
    puts("fbrect: ok\n");
    return 0;
}
