#ifndef MONARCH_DRIVERS_BLOCK_BLOCK_H
#define MONARCH_DRIVERS_BLOCK_BLOCK_H 1

#include "base/api/monarch.h"

#define BLOCK_SECTOR 512u

#define BLOCK_READONLY  0x01u
#define BLOCK_PARTITION 0x02u

struct blockdevice {
    int (*read)(struct blockdevice *self, uint32_t lba, void *buffer, uint32_t sectors);
    int (*write)(struct blockdevice *self, uint32_t lba, const void *buffer, uint32_t sectors);
    int (*readbytes)(struct blockdevice *self, size_t offset, void *buffer, size_t size);
    size_t (*size)(struct blockdevice *self);

    const char *name;
    uint32_t sector_size;
    uint32_t sectors;       /* Legacy 32-bit sector count for simple devices. */
    uint64_t sector_count;  /* Full sector count for disks larger than 2 TiB later. */
    uint32_t flags;
    int readonly;
    void *driver;
};

#endif /* MONARCH_DRIVERS_BLOCK_BLOCK_H */
