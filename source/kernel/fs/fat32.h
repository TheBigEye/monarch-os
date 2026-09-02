#ifndef MONARCH_KERNEL_FS_FAT32_H
#define MONARCH_KERNEL_FS_FAT32_H 1

/**
 * @file fat32.h
 * @brief FAT32 filesystem adapter for Monarch VFS.
 *
 * This driver is intentionally conservative and educational.  It supports the
 * Unix-style VFS operations Monarch needs today while keeping the on-disk FAT32
 * rules visible in simple C code: cluster chains, short 8.3 aliases and Long
 * File Name directory-entry chains.  Write support is still minimal and grows in
 * small tested steps instead of trying to implement every FAT32 feature at once.
 */

#include "base/api/monarch.h"
#include "base/lib/fat32.h"
#include "drivers/block/block.h"

#define FAT32_NAME 64u
#define FAT32_LFN 256u

enum fat32_node_type {
    FAT32_NONE = 0,
    FAT32_FILE = 1,
    FAT32_DIR = 2
};

struct fat32_stat {
    enum fat32_node_type type;
    size_t size;
};

typedef void (*fat32_iter)(void *ctx, const char *name, enum fat32_node_type type, size_t size);

struct fat32 {
    int (*stat)(struct fat32 *self, const char *path, struct fat32_stat *out);
    int (*mkdir)(struct fat32 *self, const char *path);
    int (*rmdir)(struct fat32 *self, const char *path);
    int (*create)(struct fat32 *self, const char *path);
    int (*truncate)(struct fat32 *self, const char *path);
    int (*unlink)(struct fat32 *self, const char *path);
    int (*read)(struct fat32 *self, const char *path, size_t offset, char *buffer, size_t size);
    int (*write)(struct fat32 *self, const char *path, size_t offset, const char *buffer, size_t size);
    void (*each)(struct fat32 *self, const char *path, fat32_iter iter, void *ctx);

    struct blockdevice *device;
    struct fat32_info info;
    char cache_path[256];
    uint32_t cache_start_cluster;
    uint32_t cache_cluster;
    size_t cache_offset;
    uint32_t fsinfo_free_count;
    uint32_t fsinfo_next_free;
    int fsinfo_valid;
    int mounted;
};

/** Mount a FAT32 volume from `device`.  Returns non-zero on success. */
int fat32(struct fat32 *self, struct blockdevice *device);

#endif /* MONARCH_KERNEL_FS_FAT32_H */
