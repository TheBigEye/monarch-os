#ifndef MONARCH_BASE_LIB_CPIO_H
#define MONARCH_BASE_LIB_CPIO_H 1

/**
 * @file cpio.h
 * @brief Tiny parser for the CPIO "newc" archive format.
 *
 * Monarch uses a Linux-compatible CPIO newc archive as its initrd.  This parser
 * is pure freestanding C so both the kernel and userspace tools can inspect the
 * same archive format without duplicating header parsing.
 */

#include "base/api/monarch.h"

#define CPIO_NEWC_HEADER 110u
#define CPIO_MODE_MASK   0170000u
#define CPIO_MODE_FILE   0100000u
#define CPIO_MODE_DIR    0040000u

/** One validated entry from a CPIO newc archive. */
struct cpio_newc_entry {
    const char *name;
    const uint8_t *data;
    size_t size;
    uint32_t mode;
};

/** Return non-zero to continue iteration, or zero to stop/fail. */
typedef int (*cpio_newc_iter)(void *ctx, const struct cpio_newc_entry *entry);

/** Iterate every non-trailer entry in a CPIO newc archive. */
int cpio_newc_each(const void *archive, size_t size, cpio_newc_iter iter, void *ctx);

/** Find `name` in a CPIO newc archive and return a pointer to its data. */
const uint8_t *cpio_newc_find(const void *archive, size_t size, const char *name, size_t *out_size);

#endif /* MONARCH_BASE_LIB_CPIO_H */
