#ifndef MONARCH_DRIVERS_BLOCK_HARDDISK_H
#define MONARCH_DRIVERS_BLOCK_HARDDISK_H 1

/**
 * @file harddisk.h
 * @brief Minimal ATA PIO hard-disk block device.
 *
 * This is deliberately small: it probes one legacy IDE channel/device and
 * exposes it as a read-only blockdevice.  Write support, PCI IDE discovery,
 * LBA48 transfers and DMA can come later after the filesystem read path is
 * stable.
 */

#include "drivers/block/block.h"

struct harddisk {
    struct blockdevice device;
    uint16_t io;
    uint16_t ctrl;
    uint8_t slave;
    uint8_t present;
    uint8_t lba48;
    uint64_t sectors64;
    char name[16];
};

int harddisk(struct harddisk *disk, const char *name, uint16_t io, uint16_t ctrl, uint8_t slave);

#endif /* MONARCH_DRIVERS_BLOCK_HARDDISK_H */
