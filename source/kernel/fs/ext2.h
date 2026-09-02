#ifndef MONARCH_KERNEL_FS_EXT2_H
#define MONARCH_KERNEL_FS_EXT2_H 1

/**
 * @file ext2.h
 * @brief EXT2 filesystem adapter for Monarch VFS.
 */

#include "base/api/monarch.h"
#include "base/lib/ext2.h"
#include "drivers/block/block.h"

#define EXT2_NAME 256u
#define EXT2_MAX_BLOCK 4096u

enum ext2_node_type {
    EXT2_NONE = 0,
    EXT2_FILE = 1,
    EXT2_DIR = 2
};

struct ext2_stat {
    enum ext2_node_type type;
    size_t size;
    uint32_t atime;
    uint32_t ctime;
    uint32_t mtime;
};

typedef void (*ext2_iter)(void *ctx, const char *name, enum ext2_node_type type, size_t size);

struct ext2 {
    int (*stat)(struct ext2 *self, const char *path, struct ext2_stat *out);
    int (*create)(struct ext2 *self, const char *path);
    int (*mkdir)(struct ext2 *self, const char *path);
    int (*rmdir)(struct ext2 *self, const char *path);
    int (*unlink)(struct ext2 *self, const char *path);
    int (*truncate)(struct ext2 *self, const char *path);
    int (*read)(struct ext2 *self, const char *path, size_t offset, char *buffer, size_t size);
    int (*write)(struct ext2 *self, const char *path, size_t offset, const char *buffer, size_t size);
    void (*each)(struct ext2 *self, const char *path, ext2_iter iter, void *ctx);

    struct blockdevice *device;
    struct ext2_info info;
    uint32_t group_desc_block;
    int mounted;
};

int ext2(struct ext2 *self, struct blockdevice *device);

#endif /* MONARCH_KERNEL_FS_EXT2_H */
