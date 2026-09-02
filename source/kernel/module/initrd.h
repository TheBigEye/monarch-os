#ifndef MONARCH_STATIC_INITRD_H
#define MONARCH_STATIC_INITRD_H 1

#include "base/api/monarch.h"
#include "drivers/block/ramdisk.h"
#include "kernel/fs/vfs.h"

struct initrd {
    int (*load)(struct initrd *self, struct vfs *fs, const char *where);
    struct blockdevice *(*device)(struct initrd *self);
    size_t (*size)(struct initrd *self);
    unsigned (*files)(struct initrd *self);

    struct ramdisk _disk;
    uint8_t *_image;
    size_t _size;
    unsigned _count;
};

const uint8_t *initrdfind(struct initrd *self, const char *path, size_t *size);
struct initrd *initrdactive(void);

void initrd(struct initrd *self, void *image, size_t size);

#endif /* MONARCH_STATIC_INITRD_H */
