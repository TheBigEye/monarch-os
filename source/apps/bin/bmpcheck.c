/**
 * @file bmpcheck.c
 * @brief Userspace BMP header validator and FNV-1a checksum tool.
 *
 * This intentionally checks the same simple BI_RGB BMP family that Monarch's
 * BGL loader accepts, but it does not decode the whole image.  Keeping the tool
 * in userspace lets normal image validation move out of the kernel/debug shell.
 */

#include "base/usr/sys.h"
#include "base/lib/hash.h"

static uint16_t le16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int32_t sle32(const uint8_t *p) {
    return (int32_t)le32(p);
}

static void puthex(uint32_t value) {
    const char *digits = "0123456789abcdef";
    for (int shift = 28; shift >= 0; shift -= 4) {
        putch(digits[(value >> shift) & 0xFu]);
    }
}

static int read_prefix(const char *path, uint8_t *buffer, size_t size, long *out_count) {
    long fd = open(path, OREAD);
    long count;

    if (fd < 0) {
        perror("bmpcheck");
        return 0;
    }

    count = read((int)fd, buffer, size);
    close((int)fd);
    if (count < 0) {
        perror("bmpcheck");
        return 0;
    }

    if (out_count) {
        *out_count = count;
    }
    return 1;
}

static int checksum(const char *path, uint32_t *out_hash, uint32_t *out_total) {
    char buffer[256];
    uint32_t hash = FNV1A32_OFFSET;
    uint32_t total = 0;
    long fd = open(path, OREAD);

    if (fd < 0) {
        perror("bmpcheck");
        return 0;
    }

    for (;;) {
        long count = read((int)fd, buffer, sizeof(buffer));
        if (count < 0) {
            close((int)fd);
            perror("bmpcheck");
            return 0;
        }
        if (count == 0) {
            break;
        }
        hash = fnv1a32_update(hash, buffer, (size_t)count);
        total += (uint32_t)count;
    }

    close((int)fd);
    *out_hash = hash;
    *out_total = total;
    return 1;
}

static int check_one(const char *path) {
    struct stat st;
    uint8_t header[128];
    long got = 0;
    uint32_t hash = 0;
    uint32_t total = 0;
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
    uint32_t colors = 0;
    uint32_t rowbytes;
    int topdown;
    int ok = 1;
    const char *reason = "ok";

    if (stat(path, &st) < 0 || st.type != VFS_FILE) {
        perror("bmpcheck");
        return 1;
    }
    if (!read_prefix(path, header, sizeof(header), &got) || !checksum(path, &hash, &total)) {
        return 1;
    }

    puts(path);
    puts(" bytes=");
    putu(total);
    puts(" fnv32=");
    puthex(hash);
    putch('\n');

    if (got < 54 || st.size < 54 || header[0] != 'B' || header[1] != 'M') {
        ok = 0;
        reason = "bad BMP header";
        goto done;
    }

    pixel_offset = le32(header + 10);
    dib_size = le32(header + 14);
    if (dib_size < 40 || 14u + dib_size > st.size || pixel_offset > st.size || 14u + dib_size > (uint32_t)got) {
        ok = 0;
        reason = "bad DIB header";
        goto done;
    }

    width_signed = sle32(header + 18);
    height_signed = sle32(header + 22);
    planes = le16(header + 26);
    bpp = le16(header + 28);
    compression = le32(header + 30);
    colors_used = le32(header + 46);

    if (planes != 1 || compression != 0 || width_signed <= 0 || height_signed == 0) {
        ok = 0;
        reason = "unsupported BMP layout";
        goto done;
    }

    width = (uint32_t)width_signed;
    topdown = height_signed < 0;
    height = (uint32_t)(topdown ? -height_signed : height_signed);
    if (width > 2048 || height > 2048) {
        ok = 0;
        reason = "image too large";
        goto done;
    }

    if (!(bpp == 1 || bpp == 4 || bpp == 8 || bpp == 24 || bpp == 32)) {
        ok = 0;
        reason = "unsupported bpp";
        goto done;
    }

    rowbytes = ((width * bpp + 31u) / 32u) * 4u;
    if (pixel_offset + (size_t)rowbytes * height > st.size) {
        ok = 0;
        reason = "pixel data truncated";
        goto done;
    }

    if (bpp <= 8) {
        colors = colors_used ? colors_used : (1u << bpp);
        if (14u + dib_size + colors * 4u > pixel_offset) {
            ok = 0;
            reason = "palette overlaps pixels";
            goto done;
        }
    }

done:
    if (ok) {
        puts("bmp ");
        putu(width);
        putch('x');
        putu(height);
        puts(" bpp=");
        putu(bpp);
        puts(topdown ? " top-down" : " bottom-up");
        puts(" rowbytes=");
        putu(rowbytes);
        if (colors) {
            puts(" colors=");
            putu(colors);
        }
        putch('\n');
        return 0;
    }

    eputs("bmpcheck: ");
    eputs(reason);
    eputch('\n');
    return 1;
}

int main(int argc, char **argv) {
    int status = 0;

    if (argc < 2) {
        eputs("usage: bmpcheck FILE...\n");
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        if (check_one(argv[i]) != 0) {
            status = 1;
        }
    }
    return status;
}
