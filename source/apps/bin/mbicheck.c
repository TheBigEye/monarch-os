/**
 * @file mbicheck.c
 * @brief Userspace Monarch Bitmap Image header validator and checksum tool.
 */

#include "base/usr/sys.h"
#include "base/lib/hash.h"

#define MBI_FORMAT_XRGB8888 1u
#define MBI_FORMAT_INDEX8   2u
#define MBI_FORMAT_INDEX4   3u

static uint32_t le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void puthex(uint32_t value) {
    const char *digits = "0123456789abcdef";
    for (int shift = 28; shift >= 0; shift -= 4) {
        putch(digits[(value >> shift) & 0xFu]);
    }
}

static const char *format_name(uint32_t format) {
    if (format == MBI_FORMAT_XRGB8888) {
        return "xrgb8888";
    }
    if (format == MBI_FORMAT_INDEX8) {
        return "index8";
    }
    if (format == MBI_FORMAT_INDEX4) {
        return "index4";
    }
    return "unknown";
}

static int read_prefix(const char *path, uint8_t *buffer, size_t size, long *out_count) {
    long fd = open(path, OREAD);
    long count;

    if (fd < 0) {
        perror("mbicheck");
        return 0;
    }

    count = read((int)fd, buffer, size);
    close((int)fd);
    if (count < 0) {
        perror("mbicheck");
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
        perror("mbicheck");
        return 0;
    }

    for (;;) {
        long count = read((int)fd, buffer, sizeof(buffer));
        if (count < 0) {
            close((int)fd);
            perror("mbicheck");
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
    uint8_t header[64];
    long got = 0;
    uint32_t hash = 0;
    uint32_t total = 0;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t format;
    uint32_t payload_size;
    uint32_t colors = 0;
    int ok = 1;
    const char *reason = "ok";

    if (stat(path, &st) < 0 || st.type != VFS_FILE) {
        perror("mbicheck");
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

    if (got < 24 || st.size < 24 || memcmp(header, "MBI1", 4) != 0) {
        ok = 0;
        reason = "bad MBI header";
        goto done;
    }

    width = le32(header + 4);
    height = le32(header + 8);
    pitch = le32(header + 12);
    format = le32(header + 16);
    payload_size = le32(header + 20);

    if (!width || !height || width > 4096 || height > 4096 || 24u + (size_t)payload_size > st.size) {
        ok = 0;
        reason = "invalid dimensions or payload size";
        goto done;
    }

    if (format == MBI_FORMAT_XRGB8888) {
        if (pitch != width * 4u || payload_size != pitch * height) {
            ok = 0;
            reason = "bad xrgb8888 payload";
            goto done;
        }
    } else if (format == MBI_FORMAT_INDEX8) {
        if (pitch != width || got < 28) {
            ok = 0;
            reason = "bad index8 header";
            goto done;
        }
        colors = le32(header + 24);
        if (!colors || colors > 256 || payload_size < 4u + colors * 4u + pitch * height) {
            ok = 0;
            reason = "bad index8 payload";
            goto done;
        }
    } else if (format == MBI_FORMAT_INDEX4) {
        if (pitch != (width + 1u) / 2u || got < 28) {
            ok = 0;
            reason = "bad index4 header";
            goto done;
        }
        colors = le32(header + 24);
        if (!colors || colors > 16 || payload_size < 4u + colors * 4u + pitch * height) {
            ok = 0;
            reason = "bad index4 payload";
            goto done;
        }
    } else {
        ok = 0;
        reason = "unknown format";
        goto done;
    }

done:
    if (ok) {
        puts("mbi ");
        putu(width);
        putch('x');
        putu(height);
        puts(" format=");
        puts(format_name(format));
        puts(" pitch=");
        putu(pitch);
        puts(" payload=");
        putu(payload_size);
        if (colors) {
            puts(" colors=");
            putu(colors);
        }
        putch('\n');
        return 0;
    }

    eputs("mbicheck: ");
    eputs(reason);
    eputch('\n');
    return 1;
}

int main(int argc, char **argv) {
    int status = 0;

    if (argc < 2) {
        eputs("usage: mbicheck FILE...\n");
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        if (check_one(argv[i]) != 0) {
            status = 1;
        }
    }
    return status;
}
