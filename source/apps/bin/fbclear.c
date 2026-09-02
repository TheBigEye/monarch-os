/**
 * @file fbclear.c
 * @brief Clear /dev/fb0 using the tiny FBD userspace library.
 */

#include "base/fbd/fbd.h"

int main(int argc, char **argv) {
    struct fbd fb;
    uint32_t rgb;

    if (argc != 2) {
        eputs("usage: fbclear RRGGBB\n");
        return 1;
    }

    if (!fbd_parse_color(argv[1], &rgb)) {
        eputs("fbclear: invalid color, expected RRGGBB\n");
        return 1;
    }

    if (!fbd_open(&fb)) {
        eputs("fbclear: cannot open ready 32-bit framebuffer\n");
        return 1;
    }

    if (!fbd_clear(&fb, fbd_color(&fb, rgb))) {
        eputs("fbclear: clear failed\n");
        fbd_close(&fb);
        return 1;
    }

    fbd_close(&fb);
    puts("fbclear: ok\n");
    return 0;
}
