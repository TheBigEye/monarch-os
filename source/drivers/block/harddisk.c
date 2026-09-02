#include "drivers/block/harddisk.h"
#include "arch/x86/cpu.h"

#define ATA_DATA       0u
#define ATA_ERROR      1u
#define ATA_SECCOUNT0  2u
#define ATA_LBA0       3u
#define ATA_LBA1       4u
#define ATA_LBA2       5u
#define ATA_HDDEVSEL   6u
#define ATA_STATUS     7u
#define ATA_COMMAND    7u

#define ATA_SR_ERR  0x01u
#define ATA_SR_DRQ  0x08u
#define ATA_SR_DF   0x20u
#define ATA_SR_DRDY 0x40u
#define ATA_SR_BSY  0x80u

#define ATA_CMD_READ_SECTORS  0x20u
#define ATA_CMD_WRITE_SECTORS 0x30u
#define ATA_CMD_FLUSH_CACHE   0xE7u
#define ATA_CMD_IDENTIFY      0xECu

static void ata_delay(struct harddisk *disk) {
    for (int i = 0; i < 4; i++) {
        (void)inb((uint16_t)(disk->ctrl));
    }
}

static int wait_not_busy(struct harddisk *disk) {
    for (uint32_t i = 0; i < 100000u; i++) {
        uint8_t status = inb((uint16_t)(disk->io + ATA_STATUS));
        if ((status & ATA_SR_BSY) == 0) {
            return status;
        }
    }
    return -1;
}

static int wait_drq(struct harddisk *disk) {
    for (uint32_t i = 0; i < 100000u; i++) {
        uint8_t status = inb((uint16_t)(disk->io + ATA_STATUS));
        if (status & (ATA_SR_ERR | ATA_SR_DF)) {
            return -1;
        }
        if ((status & ATA_SR_BSY) == 0 && (status & ATA_SR_DRQ)) {
            return 1;
        }
    }
    return -1;
}

static void select_drive(struct harddisk *disk, uint8_t head) {
    outb((uint16_t)(disk->io + ATA_HDDEVSEL), (uint8_t)(0xA0u | (disk->slave ? 0x10u : 0u) | (head & 0x0Fu)));
    ata_delay(disk);
}

static uint32_t identify_sectors28(const uint16_t *id) {
    return (uint32_t)id[60] | ((uint32_t)id[61] << 16);
}

static uint64_t identify_sectors48(const uint16_t *id) {
    return (uint64_t)id[100] |
           ((uint64_t)id[101] << 16) |
           ((uint64_t)id[102] << 32) |
           ((uint64_t)id[103] << 48);
}

static int identify(struct harddisk *disk) {
    uint16_t id[256];
    int status;

    select_drive(disk, 0);
    outb((uint16_t)(disk->io + ATA_SECCOUNT0), 0);
    outb((uint16_t)(disk->io + ATA_LBA0), 0);
    outb((uint16_t)(disk->io + ATA_LBA1), 0);
    outb((uint16_t)(disk->io + ATA_LBA2), 0);
    outb((uint16_t)(disk->io + ATA_COMMAND), ATA_CMD_IDENTIFY);

    status = inb((uint16_t)(disk->io + ATA_STATUS));
    if (status == 0) {
        return 0;
    }

    status = wait_not_busy(disk);
    if (status < 0) {
        return 0;
    }

    if (inb((uint16_t)(disk->io + ATA_LBA1)) || inb((uint16_t)(disk->io + ATA_LBA2))) {
        return 0; /* ATAPI or non-ATA device. */
    }

    if (wait_drq(disk) < 0) {
        return 0;
    }

    for (int i = 0; i < 256; i++) {
        id[i] = inw((uint16_t)(disk->io + ATA_DATA));
    }

    if ((id[49] & (1u << 9)) == 0) {
        return 0; /* We require LBA. */
    }

    disk->lba48 = (id[83] & (1u << 10)) ? 1 : 0;
    disk->sectors64 = disk->lba48 ? identify_sectors48(id) : identify_sectors28(id);
    if (!disk->sectors64) {
        return 0;
    }
    return 1;
}

static int read_lba28(struct harddisk *disk, uint32_t lba, void *buffer, uint32_t sectors) {
    uint16_t *out = buffer;

    if (!disk || !buffer || !sectors || sectors > 256u || lba > 0x0FFFFFFFu || lba + sectors > disk->device.sectors) {
        return -1;
    }

    if (wait_not_busy(disk) < 0) {
        return -1;
    }

    outb((uint16_t)(disk->ctrl), 0x02u); /* Disable ATA IRQs; this driver polls. */
    outb((uint16_t)(disk->io + ATA_HDDEVSEL), (uint8_t)(0xE0u | (disk->slave ? 0x10u : 0u) | ((lba >> 24) & 0x0Fu)));
    ata_delay(disk);
    outb((uint16_t)(disk->io + ATA_SECCOUNT0), (uint8_t)(sectors == 256u ? 0u : sectors));
    outb((uint16_t)(disk->io + ATA_LBA0), (uint8_t)lba);
    outb((uint16_t)(disk->io + ATA_LBA1), (uint8_t)(lba >> 8));
    outb((uint16_t)(disk->io + ATA_LBA2), (uint8_t)(lba >> 16));
    outb((uint16_t)(disk->io + ATA_COMMAND), ATA_CMD_READ_SECTORS);

    for (uint32_t s = 0; s < sectors; s++) {
        if (wait_drq(disk) < 0) {
            return -1;
        }
        for (int i = 0; i < 256; i++) {
            *out++ = inw((uint16_t)(disk->io + ATA_DATA));
        }
    }

    (void)inb((uint16_t)(disk->io + ATA_STATUS));
    return (int)sectors;
}

static int write_lba28(struct harddisk *disk, uint32_t lba, const void *buffer, uint32_t sectors) {
    const uint16_t *in = buffer;

    if (!disk || !buffer || !sectors || sectors > 256u || lba > 0x0FFFFFFFu || lba + sectors > disk->device.sectors) {
        return -1;
    }

    if (wait_not_busy(disk) < 0) {
        return -1;
    }

    outb((uint16_t)(disk->ctrl), 0x02u); /* Disable ATA IRQs; this driver polls. */
    outb((uint16_t)(disk->io + ATA_HDDEVSEL), (uint8_t)(0xE0u | (disk->slave ? 0x10u : 0u) | ((lba >> 24) & 0x0Fu)));
    ata_delay(disk);
    outb((uint16_t)(disk->io + ATA_SECCOUNT0), (uint8_t)(sectors == 256u ? 0u : sectors));
    outb((uint16_t)(disk->io + ATA_LBA0), (uint8_t)lba);
    outb((uint16_t)(disk->io + ATA_LBA1), (uint8_t)(lba >> 8));
    outb((uint16_t)(disk->io + ATA_LBA2), (uint8_t)(lba >> 16));
    outb((uint16_t)(disk->io + ATA_COMMAND), ATA_CMD_WRITE_SECTORS);

    for (uint32_t s = 0; s < sectors; s++) {
        if (wait_drq(disk) < 0) {
            return -1;
        }
        for (int i = 0; i < 256; i++) {
            outw((uint16_t)(disk->io + ATA_DATA), *in++);
        }
    }

    outb((uint16_t)(disk->io + ATA_COMMAND), ATA_CMD_FLUSH_CACHE);
    if (wait_not_busy(disk) < 0) {
        return -1;
    }
    return (int)sectors;
}

static int read_impl(struct blockdevice *self, uint32_t lba, void *buffer, uint32_t sectors) {
    struct harddisk *disk = self->driver;
    uint8_t *out = buffer;
    uint32_t done = 0;

    if (!disk || !disk->present || !buffer || !sectors) {
        return -1;
    }

    while (done < sectors) {
        uint32_t chunk = sectors - done;
        if (chunk > 256u) {
            chunk = 256u;
        }
        if (read_lba28(disk, lba + done, out + (size_t)done * self->sector_size, chunk) < 0) {
            return done ? (int)done : -1;
        }
        done += chunk;
    }
    return (int)done;
}

static int write_impl(struct blockdevice *self, uint32_t lba, const void *buffer, uint32_t sectors) {
    struct harddisk *disk = self->driver;
    const uint8_t *in = buffer;
    uint32_t done = 0;

    if (!disk || !disk->present || !buffer || !sectors || self->readonly) {
        return -1;
    }

    while (done < sectors) {
        uint32_t chunk = sectors - done;
        if (chunk > 256u) {
            chunk = 256u;
        }
        if (write_lba28(disk, lba + done, in + (size_t)done * self->sector_size, chunk) < 0) {
            return done ? (int)done : -1;
        }
        done += chunk;
    }
    return (int)done;
}

static int readbytes_impl(struct blockdevice *self, size_t offset, void *buffer, size_t size) {
    uint8_t temp[BLOCK_SECTOR];
    uint8_t *out = buffer;
    size_t done = 0;
    size_t total;

    if (!self || (!buffer && size)) {
        return -1;
    }

    total = self->size(self);
    if (offset > total || size > total - offset) {
        return -1;
    }

    while (done < size) {
        uint32_t lba = (uint32_t)((offset + done) / self->sector_size);
        size_t inner = (offset + done) % self->sector_size;
        size_t count = min(size - done, self->sector_size - inner);

        if (read_impl(self, lba, temp, 1) != 1) {
            return done ? (int)done : -1;
        }
        memcpy(out + done, temp + inner, count);
        done += count;
    }

    return (int)done;
}

static size_t size_impl(struct blockdevice *self) {
    struct harddisk *disk = self->driver;
    if (!disk || !disk->present) {
        return 0;
    }
    return (size_t)(disk->device.sector_count * disk->device.sector_size);
}

int harddisk(struct harddisk *disk, const char *name, uint16_t io, uint16_t ctrl, uint8_t slave) {
    memset(disk, 0, sizeof(*disk));
    disk->io = io;
    disk->ctrl = ctrl;
    disk->slave = slave ? 1 : 0;
    strncpy(disk->name, name ? name : "hd0", sizeof(disk->name));

    disk->device.read = read_impl;
    disk->device.write = write_impl;
    disk->device.readbytes = readbytes_impl;
    disk->device.size = size_impl;
    disk->device.name = disk->name;
    disk->device.sector_size = BLOCK_SECTOR;
    disk->device.readonly = 0;
    disk->device.flags = 0;
    disk->device.driver = disk;

    if (!identify(disk)) {
        return 0;
    }

    disk->present = 1;
    disk->device.sector_count = disk->sectors64 > 0x0FFFFFFFull ? 0x0FFFFFFFull : disk->sectors64;
    disk->device.sectors = (uint32_t)disk->device.sector_count;
    return 1;
}
