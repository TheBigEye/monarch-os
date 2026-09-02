#ifndef MONARCH_KERNEL_FS_FAT12_H
#define MONARCH_KERNEL_FS_FAT12_H 1

#include "base/api/monarch.h"
#include "base/lib/fat32.h"
#include "drivers/block/block.h"

enum fat12_node_type { FAT12_NONE = 0, FAT12_FILE = 1, FAT12_DIR = 2 };
struct fat12_stat {
    enum fat12_node_type type;
    size_t size;
    uint32_t atime;
    uint32_t ctime;
    uint32_t mtime;
};
typedef void (*fat12_iter)(void *ctx, const char *name, enum fat12_node_type type, size_t size);

struct fat12 {
    int (*stat)(struct fat12 *self, const char *path, struct fat12_stat *out);
    int (*create)(struct fat12 *self, const char *path);
    int (*mkdir)(struct fat12 *self, const char *path);
    int (*rmdir)(struct fat12 *self, const char *path);
    int (*unlink)(struct fat12 *self, const char *path);
    int (*truncate)(struct fat12 *self, const char *path);
    int (*alloc)(struct fat12 *self, uint16_t *out_cluster);
    int (*free)(struct fat12 *self, uint16_t cluster);
    int (*read)(struct fat12 *self, const char *path, size_t offset, char *buffer, size_t size);
    int (*write)(struct fat12 *self, const char *path, size_t offset, const char *buffer, size_t size);
    void (*each)(struct fat12 *self, const char *path, fat12_iter iter, void *ctx);
    struct blockdevice *device;
    struct fat32_info info;
    int mounted;
};

int fat12(struct fat12 *self, struct blockdevice *device);

#endif /* MONARCH_KERNEL_FS_FAT12_H */
