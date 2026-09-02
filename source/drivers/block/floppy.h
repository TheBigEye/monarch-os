#ifndef MONARCH_DRIVERS_BLOCK_FLOPPY_H
#define MONARCH_DRIVERS_BLOCK_FLOPPY_H 1

#include "base/api/monarch.h"
#include "drivers/block/block.h"

/** Intel 82077-compatible floppy controller state. */
struct floppy_controller {
    uint16_t base;
    uint8_t dor;
    volatile uint8_t irq_seen;
    uint8_t dma_ready;
    uint8_t command_ready;
    uint8_t motor_on;
    uint8_t last_status;
    uint8_t last_st1;
    uint8_t last_st2;
    uint8_t last_st3;
    uint8_t last_cylinder;
    uint8_t last_head;
    uint8_t last_sector;
    uint8_t last_size;
    uint8_t last_irq;
    uint8_t tolerate_qemu_ids;
    uint8_t need_recal;
    uint8_t cache_valid;
    uint32_t cache_lba;
    uint8_t cache[BLOCK_SECTOR];
    uint8_t track_cache[18u * BLOCK_SECTOR];
    uint8_t track_cache_valid;
    uint8_t track_cache_cylinder;
    uint8_t track_cache_head;
    struct blockdevice device;
    int present;
};

/** Probe and reset the primary floppy controller without touching the disk. */
int floppy_init(struct floppy_controller *self);

/** Start or stop drive 0's motor without issuing a disk command. */
void floppy_motor(struct floppy_controller *self, int on);

/** Recalibrate drive 0 to cylinder zero. Returns zero on timeout or controller error. */
int floppy_recal(struct floppy_controller *self);

/** Seek drive 0/head 0 to a cylinder. Returns zero on controller error. */
int floppy_seek(struct floppy_controller *self, uint8_t cylinder);

/** Read one 512-byte sector from a 1.44 MiB floppy using LBA addressing. */
int floppy_read(struct floppy_controller *self, uint32_t lba, void *buffer);

/** Write one 512-byte sector to a 1.44 MiB floppy using LBA addressing. */
int floppy_write(struct floppy_controller *self, uint32_t lba, const void *buffer);

/** Format one track with standard 1.44 MiB sector IDs. */
int floppy_format(struct floppy_controller *self, uint8_t cylinder, uint8_t head);

/** Read the next sector ID found on a track. */
int floppy_id(struct floppy_controller *self, uint8_t head);

#endif /* MONARCH_DRIVERS_BLOCK_FLOPPY_H */
