#ifndef MONARCH_DRIVERS_BLOCK_RAMDISK_H
#define MONARCH_DRIVERS_BLOCK_RAMDISK_H 1

#include "drivers/block/block.h"

struct ramdisk {
    struct blockdevice device;
    uint8_t *data;
    size_t bytes;
};

void ramdisk(struct ramdisk *disk, const char *name, void *data, size_t size, int readonly);

#endif /* MONARCH_DRIVERS_BLOCK_RAMDISK_H */
