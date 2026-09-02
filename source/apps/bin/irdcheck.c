/**
 * @file irdcheck.c
 * @brief Userspace initrd consistency check using /dev/ram0 and CPIO newc parsing.
 *
 * This replaces the old kernel-shell check for ordinary validation.  It reads
 * the raw initrd archive from /dev/ram0, finds a file inside the CPIO image, and
 * compares those bytes with the same file imported into the VFS under /initrd.
 */

#include "base/usr/sys.h"
#include "base/lib/cpio.h"
#include "base/lib/hash.h"

#define RAW_LIMIT (2u * 1024u * 1024u)
#define CHUNK 256u

static uint8_t raw[RAW_LIMIT];

static void puthex(uint32_t value) {
    const char *digits = "0123456789abcdef";
    for (int shift = 28; shift >= 0; shift -= 4) {
        putch(digits[(value >> shift) & 0xFu]);
    }
}

static int read_raw(size_t *out_size) {
    long fd;
    size_t used = 0;

    if (!out_size) {
        return 0;
    }

    fd = open("/dev/ram0", OREAD);
    if (fd < 0) {
        perror("irdcheck: /dev/ram0");
        return 0;
    }

    while (used < sizeof(raw)) {
        long count = read((int)fd, raw + used, sizeof(raw) - used);
        if (count < 0) {
            close((int)fd);
            perror("irdcheck: read");
            return 0;
        }
        if (count == 0) {
            break;
        }
        used += (size_t)count;
    }

    if (used == sizeof(raw)) {
        uint8_t extra;
        long count = read((int)fd, &extra, 1);
        if (count != 0) {
            close((int)fd);
            eputs("irdcheck: raw initrd is larger than buffer\n");
            return 0;
        }
    }

    close((int)fd);
    *out_size = used;
    return 1;
}

static int compare_vfs(const char *path, const uint8_t *expected, size_t expected_size, uint32_t *out_hash) {
    uint8_t chunk[CHUNK];
    uint32_t hash = FNV1A32_OFFSET;
    size_t offset = 0;
    long fd;

    if (!path || !expected || !out_hash) {
        return 0;
    }

    fd = open(path, OREAD);
    if (fd < 0) {
        perror("irdcheck: vfs");
        return 0;
    }

    while (offset < expected_size) {
        size_t want = min(sizeof(chunk), expected_size - offset);
        long count = read((int)fd, chunk, want);
        if (count < 0) {
            close((int)fd);
            perror("irdcheck: vfs read");
            return 0;
        }
        if (count == 0) {
            close((int)fd);
            return 0;
        }
        hash = fnv1a32_update(hash, chunk, (size_t)count);
        if (memcmp(chunk, expected + offset, (size_t)count) != 0) {
            close((int)fd);
            *out_hash = hash;
            return 0;
        }
        offset += (size_t)count;
    }

    {
        long extra = read((int)fd, chunk, 1);
        close((int)fd);
        *out_hash = hash;
        return extra == 0;
    }
}

int main(int argc, char **argv) {
    const char *cpio_name = argc > 1 ? argv[1] : "share/bigeye.bmp";
    const char *vfs_path = argc > 2 ? argv[2] : "/initrd/share/bigeye.bmp";
    size_t raw_size = 0;
    size_t file_size = 0;
    const uint8_t *file;
    uint32_t raw_hash;
    uint32_t file_hash;
    uint32_t vfs_hash = 0;
    int match;

    if (!read_raw(&raw_size)) {
        return 1;
    }

    raw_hash = fnv1a32(raw, raw_size);
    file = cpio_newc_find(raw, raw_size, cpio_name, &file_size);
    if (!file) {
        eputs("irdcheck: cpio entry not found: ");
        eputs(cpio_name);
        eputch('\n');
        return 1;
    }

    file_hash = fnv1a32(file, file_size);
    match = compare_vfs(vfs_path, file, file_size, &vfs_hash);

    puts("raw-initrd bytes=");
    putu((uint32_t)raw_size);
    puts(" fnv32=");
    puthex(raw_hash);
    putch('\n');

    puts("cpio ");
    puts(cpio_name);
    puts(" bytes=");
    putu((uint32_t)file_size);
    puts(" fnv32=");
    puthex(file_hash);
    putch('\n');

    puts("vfs ");
    puts(vfs_path);
    puts(" fnv32=");
    puthex(vfs_hash);
    puts(" match=");
    puts(match ? "yes" : "no");
    putch('\n');

    return match ? 0 : 1;
}
