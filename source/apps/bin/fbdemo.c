/**
 * @file fbdemo.c
 * @brief Draw a tiny userspace demo through the FBD mini-library.
 *
 * FBD wraps `/dev/fbinfo` and `/dev/fb0`, keeping demo applications focused on
 * drawing instead of metadata parsing and native pixel packing.
 */

#include "base/fbd/fbd.h"

#define MAX_WIDTH 1024u

static uint32_t row[MAX_WIDTH];

int main(int argc, char **argv) {
    struct fbd fb;
    uint32_t width;
    uint32_t bar_x;
    uint32_t bar_y;
    uint32_t bar_w;
    uint32_t bar_h;

    unused(argc);
    unused(argv);

    if (!fbd_open(&fb)) {
        eputs("fbdemo: no ready 32-bit linear framebuffer\n");
        return 1;
    }

    width = fb.width < MAX_WIDTH ? fb.width : MAX_WIDTH;
    bar_w = fb.width / 2u;
    bar_h = fb.height / 4u;
    bar_x = (fb.width - bar_w) / 2u;
    bar_y = (fb.height - bar_h) / 2u;

    for (uint32_t y = 0; y < fb.height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            uint8_t r = (uint8_t)((x * 255u) / (fb.width ? fb.width : 1u));
            uint8_t g = (uint8_t)((y * 255u) / (fb.height ? fb.height : 1u));
            uint8_t b = 0x40u;

            if (x >= bar_x && x < bar_x + bar_w && y >= bar_y && y < bar_y + bar_h) {
                r = 0x20u;
                g = 0x80u;
                b = 0xFFu;
            }
            row[x] = fbd_rgb(&fb, r, g, b);
        }

        if (!fbd_write_row(&fb, 0, y, row, width)) {
            eputs("fbdemo: write fb0 failed\n");
            fbd_close(&fb);
            return 1;
        }
    }

    fbd_fill_rect(&fb, 24, 24, 220, 64, fbd_rgb(&fb, 0x10, 0x10, 0x18));
    fbd_fill_rect(&fb, 32, 32, 204, 48, fbd_rgb(&fb, 0xFF, 0xCC, 0x44));

    fbd_close(&fb);
    puts("fbdemo: drew to /dev/fb0 through FBD\n");
    return 0;
}
