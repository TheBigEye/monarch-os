/**
 * @file fbppm.c
 * @brief Tiny PPM P3 image viewer for /dev/fb0 using FBD.
 *
 * PPM P3 is a text image format.  It is inefficient, but very easy to parse in
 * early freestanding userspace and perfect for a framebuffer smoke test.
 */

#include "base/fbd/fbd.h"

#define IO_BUFFER 256u
#define MAX_ROW_PIXELS 1024u

struct reader {
    int fd;
    char data[IO_BUFFER];
    size_t pos;
    size_t len;
};

static uint32_t row[MAX_ROW_PIXELS];

static int refill(struct reader *r) {
    long got = read(r->fd, r->data, sizeof(r->data));
    if (got <= 0) {
        return 0;
    }
    r->pos = 0;
    r->len = (size_t)got;
    return 1;
}

static int getch_r(struct reader *r, char *out) {
    if (r->pos >= r->len && !refill(r)) {
        return 0;
    }
    *out = r->data[r->pos++];
    return 1;
}

static int white(char ch) {
    return ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t';
}

static int next_token(struct reader *r, char *out, size_t size) {
    char ch;
    size_t used = 0;

    for (;;) {
        if (!getch_r(r, &ch)) {
            return 0;
        }
        if (white(ch)) {
            continue;
        }
        if (ch == '#') {
            while (getch_r(r, &ch) && ch != '\n') {
            }
            continue;
        }
        break;
    }

    do {
        if (white(ch) || ch == '#') {
            if (ch == '#') {
                while (getch_r(r, &ch) && ch != '\n') {
                }
            }
            break;
        }
        if (used + 1u < size) {
            out[used++] = ch;
        }
    } while (getch_r(r, &ch));

    out[used] = '\0';
    return used != 0;
}

static int next_int(struct reader *r, int *out) {
    char token[32];
    if (!next_token(r, token, sizeof(token))) {
        return 0;
    }
    *out = atoi(token);
    return 1;
}

static uint8_t scale_channel(int value, int maxval) {
    if (value < 0) {
        value = 0;
    }
    if (value > maxval) {
        value = maxval;
    }
    return (uint8_t)((value * 255) / maxval);
}

int main(int argc, char **argv) {
    const char *path;
    struct reader r;
    struct fbd fb;
    char token[32];
    int width;
    int height;
    int maxval;
    uint32_t x0 = 0;
    uint32_t y0 = 0;
    uint32_t scale = 1;

    if (argc < 2 || argc > 5) {
        eputs("usage: fbppm FILE [X Y [SCALE]]\n");
        return 1;
    }

    path = argv[1];
    if (argc >= 4) {
        x0 = (uint32_t)atoi(argv[2]);
        y0 = (uint32_t)atoi(argv[3]);
    }
    if (argc >= 5) {
        scale = (uint32_t)atoi(argv[4]);
        if (!scale) {
            scale = 1;
        }
    }

    memset(&r, 0, sizeof(r));
    r.fd = (int)open(path, OREAD);
    if (r.fd < 0) {
        perror("fbppm: open");
        return 1;
    }

    if (!next_token(&r, token, sizeof(token)) || strcmp(token, "P3") != 0 ||
        !next_int(&r, &width) || !next_int(&r, &height) || !next_int(&r, &maxval)) {
        eputs("fbppm: unsupported or invalid PPM, expected P3\n");
        close(r.fd);
        return 1;
    }

    if (width <= 0 || height <= 0 || maxval <= 0 || (uint32_t)width * scale > MAX_ROW_PIXELS) {
        eputs("fbppm: image too large for tiny viewer\n");
        close(r.fd);
        return 1;
    }

    if (!fbd_open(&fb)) {
        eputs("fbppm: cannot open ready 32-bit framebuffer\n");
        close(r.fd);
        return 1;
    }

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int rv;
            int gv;
            int bv;
            uint32_t color;

            if (!next_int(&r, &rv) || !next_int(&r, &gv) || !next_int(&r, &bv)) {
                eputs("fbppm: truncated pixel data\n");
                fbd_close(&fb);
                close(r.fd);
                return 1;
            }

            color = fbd_rgb(&fb, scale_channel(rv, maxval), scale_channel(gv, maxval), scale_channel(bv, maxval));
            for (uint32_t sx = 0; sx < scale; sx++) {
                row[(uint32_t)x * scale + sx] = color;
            }
        }

        for (uint32_t sy = 0; sy < scale; sy++) {
            if (!fbd_write_row(&fb, x0, y0 + (uint32_t)y * scale + sy, row, (uint32_t)width * scale)) {
                eputs("fbppm: framebuffer write failed\n");
                fbd_close(&fb);
                close(r.fd);
                return 1;
            }
        }
    }

    fbd_close(&fb);
    close(r.fd);
    puts("fbppm: ok\n");
    return 0;
}
