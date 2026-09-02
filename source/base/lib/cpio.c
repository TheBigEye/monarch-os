/**
 * @file cpio.c
 * @brief CPIO newc archive parser shared by initrd code and userspace tools.
 */

#include "base/lib/cpio.h"

static size_t align4(size_t value) {
    return (value + 3u) & ~(size_t)3u;
}

static int hex_value(char ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }
    return -1;
}

static uint32_t field(const char *text) {
    uint32_t value = 0;

    for (int i = 0; i < 8; i++) {
        int digit = hex_value(text[i]);
        if (digit < 0) {
            return 0;
        }
        value = (value << 4) | (uint32_t)digit;
    }
    return value;
}

static int magic(const uint8_t *entry) {
    return memcmp(entry, "070701", 6) == 0 || memcmp(entry, "070702", 6) == 0;
}


int cpio_newc_each(const void *archive, size_t size, cpio_newc_iter iter, void *ctx) {
    const uint8_t *image = archive;
    size_t offset = 0;

    if (!image || !size || !iter) {
        return 0;
    }

    while (offset + CPIO_NEWC_HEADER <= size) {
        const uint8_t *raw = image + offset;
        uint32_t mode;
        uint32_t filesize;
        uint32_t namesize;
        const char *name;
        size_t name_offset;
        size_t data_offset;
        size_t next;
        struct cpio_newc_entry entry;

        if (!magic(raw)) {
            break;
        }

        mode = field((const char *)raw + 14);
        filesize = field((const char *)raw + 54);
        namesize = field((const char *)raw + 94);
        if (!namesize) {
            break;
        }

        name_offset = offset + CPIO_NEWC_HEADER;
        data_offset = align4(name_offset + namesize);
        next = align4(data_offset + filesize);

        if (name_offset + namesize > size || data_offset + filesize > size || next > size) {
            return 0;
        }

        name = (const char *)(image + name_offset);
        if (strcmp(name, "TRAILER!!!") == 0) {
            return 1;
        }

        entry.name = name;
        entry.data = image + data_offset;
        entry.size = filesize;
        entry.mode = mode;
        if (!iter(ctx, &entry)) {
            return 0;
        }

        offset = next;
    }

    return 1;
}

struct find_ctx {
    const char *name;
    const uint8_t *data;
    size_t size;
};

static int find_iter(void *ctx, const struct cpio_newc_entry *entry) {
    struct find_ctx *find = ctx;

    if (strcmp(entry->name, find->name) == 0) {
        find->data = entry->data;
        find->size = entry->size;
        return 0;
    }
    return 1;
}

const uint8_t *cpio_newc_find(const void *archive, size_t size, const char *name, size_t *out_size) {
    struct find_ctx ctx;

    if (!archive || !name || !size) {
        return nil;
    }

    memset(&ctx, 0, sizeof(ctx));
    ctx.name = name;
    (void)cpio_newc_each(archive, size, find_iter, &ctx);
    if (!ctx.data) {
        return nil;
    }
    if (out_size) {
        *out_size = ctx.size;
    }
    return ctx.data;
}
