/**
 * @file lsinitrd.c
 * @brief List the raw CPIO initrd archive exposed by /dev/ram0.
 */

#include "base/usr/sys.h"
#include "base/lib/cpio.h"

#define RAW_LIMIT (2u * 1024u * 1024u)

static uint8_t raw[RAW_LIMIT];

static int read_raw(size_t *out_size) {
    long fd;
    size_t used = 0;

    if (!out_size) {
        return 0;
    }

    fd = open("/dev/ram0", OREAD);
    if (fd < 0) {
        perror("lsinitrd: /dev/ram0");
        return 0;
    }

    while (used < sizeof(raw)) {
        long count = read((int)fd, raw + used, sizeof(raw) - used);
        if (count < 0) {
            close((int)fd);
            perror("lsinitrd: read");
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
            eputs("lsinitrd: raw initrd is larger than buffer\n");
            return 0;
        }
    }

    close((int)fd);
    *out_size = used;
    return 1;
}

static int print_entry(void *ctx, const struct cpio_newc_entry *entry) {
    uint32_t *count = ctx;
    char type = '-';

    if ((entry->mode & CPIO_MODE_MASK) == CPIO_MODE_DIR) {
        type = 'd';
    }

    putch(type);
    putch(' ');
    puts(entry->name);
    if (type == 'd') {
        putch('/');
    }
    puts(" ");
    putu((uint32_t)entry->size);
    putch('\n');

    if (count) {
        (*count)++;
    }
    return 1;
}

int main(int argc, char **argv) {
    size_t raw_size = 0;
    uint32_t count = 0;
    (void)argc;
    (void)argv;

    if (!read_raw(&raw_size)) {
        return 1;
    }

    if (!cpio_newc_each(raw, raw_size, print_entry, &count)) {
        eputs("lsinitrd: invalid or unsupported cpio archive\n");
        return 1;
    }

    puts("entries=");
    putu(count);
    puts(" bytes=");
    putu((uint32_t)raw_size);
    putch('\n');
    return 0;
}
