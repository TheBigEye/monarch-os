#ifndef MONARCH_DRIVERS_BLOCK_PARTITION_H
#define MONARCH_DRIVERS_BLOCK_PARTITION_H 1

/**
 * @file partition.h
 * @brief Block-device view that maps a partition to a parent disk.
 *
 * A partition is itself a block device.  Reads and writes are translated by
 * adding `start_lba` before forwarding to the parent device.  This keeps FAT32,
 * EXT2 and other filesystem drivers independent from MBR/GPT layout details.
 */

#include "drivers/block/block.h"

#define PARTITION_NAME 16u

struct partition {
    struct blockdevice device;
    struct blockdevice *parent;
    uint32_t start_lba;
    uint32_t sector_count32;
    char name[PARTITION_NAME];
};

void partition(struct partition *part, const char *name, struct blockdevice *parent, uint32_t start_lba, uint32_t sector_count, int readonly);

#endif /* MONARCH_DRIVERS_BLOCK_PARTITION_H */
