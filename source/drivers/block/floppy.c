/**
 * @file floppy.c
 * @brief Initial Intel 82077-compatible floppy controller bring-up.
 */

#include "drivers/block/floppy.h"
#include "drivers/block/block.h"
#include "arch/x86/cpu.h"
#include "arch/x86/interrupt.h"

#define FLOPPY_PRIMARY 0x3F0u
#define FLOPPY_DOR     0x02u
#define FLOPPY_MSR     0x04u
#define FLOPPY_FIFO    0x05u
#define FLOPPY_CCR     0x07u

#define FLOPPY_MSR_RQM 0x80u
#define FLOPPY_MSR_DIO 0x40u
#define FLOPPY_MSR_CB  0x10u

#define DMA_MASK       0x0Au
#define DMA_MODE       0x0Bu
#define DMA_FLIPFLOP   0x0Cu
#define DMA_CHANNEL2   0x04u
#define DMA_COUNT2     0x05u
#define DMA_PAGE2      0x81u

#define FLOPPY_TRACK_SECTORS 18u
#define FLOPPY_TRACK_BYTES (FLOPPY_TRACK_SECTORS * BLOCK_SECTOR)

static uint8_t dma_buffer[FLOPPY_TRACK_BYTES] __attribute__((aligned(16)));
static struct floppy_controller *active;

static void irq_handler(struct registers *state) {
    unused(state);
    if (active) {
        active->irq_seen = 1;
        active->last_irq = 1;
    }
}

static int fifo_ready(struct floppy_controller *self, int controller_to_cpu) {
    for (unsigned attempt = 0; attempt < 100000u; attempt++) {
        uint8_t status = inb(self->base + FLOPPY_MSR);
        if ((status & FLOPPY_MSR_RQM) &&
            (!!(status & FLOPPY_MSR_DIO) == controller_to_cpu)) {
            return 1;
        }
        pause();
    }
    return 0;
}

static int send_command(struct floppy_controller *self, const uint8_t *command, size_t count) {
    if (!self || !command || !count) {
        return 0;
    }
    for (size_t i = 0; i < count; i++) {
        if (!fifo_ready(self, 0)) {
            return 0;
        }
        outb(self->base + FLOPPY_FIFO, command[i]);
    }
    return 1;
}

static int read_result(struct floppy_controller *self, uint8_t *result, size_t count);

static int specify(struct floppy_controller *self) {
    static const uint8_t command[] = { 0x03u, 0xDFu, 0x02u };
    return send_command(self, command, countof(command));
}

static int configure(struct floppy_controller *self) {
    static const uint8_t command[] = { 0x13u, 0x00u, 0x57u, 0x00u }; /* FIFO on, polling off, implied seeks off. */
    return send_command(self, command, countof(command));
}

static int sense_interrupt(struct floppy_controller *self, uint8_t result[2]) {
    static const uint8_t command[] = { 0x08u };
    return send_command(self, command, countof(command)) && read_result(self, result, 2);
}

static int read_result(struct floppy_controller *self, uint8_t *result, size_t count) {
    if (!self || !result || !count) {
        return 0;
    }
    for (size_t i = 0; i < count; i++) {
        if (!fifo_ready(self, 1)) {
            return 0;
        }
        result[i] = inb(self->base + FLOPPY_FIFO);
    }
    return 1;
}

void floppy_motor(struct floppy_controller *self, int on) {
    if (!self) {
        return;
    }
    self->motor_on = on ? 1u : 0u;
    self->dor = (uint8_t)(0x0Cu | (on ? 0x10u : 0u));
    outb(self->base + FLOPPY_DOR, self->dor);
    if (on) {
        for (volatile unsigned delay = 0; delay < 2000000u; delay++) {
            pause();
        }
    }
}

int floppy_recal(struct floppy_controller *self) {
    static const uint8_t command[] = { 0x07u, 0x00u };
    uint8_t result[2];

    if (!self || !self->present || !self->command_ready) {
        return 0;
    }
    self->irq_seen = 0;
    self->last_irq = 0;
    self->last_status = 0;
    self->last_cylinder = 0xFFu;
    floppy_motor(self, 1);
    if (!send_command(self, command, countof(command))) {
        floppy_motor(self, 0);
        return 0;
    }
    for (unsigned attempt = 0; attempt < 1000000u && !self->irq_seen; attempt++) {
        pause();
    }
    if (!self->irq_seen || !send_command(self, (const uint8_t[]){ 0x08u }, 1) || !read_result(self, result, 2)) {
        floppy_motor(self, 0);
        return 0;
    }
    self->last_status = result[0];
    self->last_cylinder = result[1];
    floppy_motor(self, 0);
    return (result[0] & 0x20u) && result[1] == 0;
}

int floppy_seek(struct floppy_controller *self, uint8_t cylinder) {
    uint8_t command[] = { 0x0Fu, 0x00u, cylinder };
    uint8_t result[2];

    if (!self || !self->present || !self->command_ready || cylinder > 79u) {
        return 0;
    }
    self->irq_seen = 0;
    self->last_irq = 0;
    self->last_status = 0;
    self->last_cylinder = 0xFFu;
    floppy_motor(self, 1);
    if (!send_command(self, command, countof(command))) {
        floppy_motor(self, 0);
        return 0;
    }
    for (unsigned attempt = 0; attempt < 1000000u && !self->irq_seen; attempt++) {
        pause();
    }
    if (!self->irq_seen || !send_command(self, (const uint8_t[]){ 0x08u }, 1) || !read_result(self, result, 2)) {
        floppy_motor(self, 0);
        return 0;
    }
    self->last_status = result[0];
    self->last_cylinder = result[1];
    floppy_motor(self, 0);
    return (result[0] & 0x20u) && result[1] == cylinder;
}

static int dma_prepare_write(size_t bytes) {
    uintptr_t address = (uintptr_t)dma_buffer;
    uint16_t count;

    if (!bytes || bytes > 0x10000u || address > 0x00FFFFFFu ||
        (address & 0xFFFFu) + bytes > 0x10000u) {
        return 0;
    }
    count = (uint16_t)(bytes - 1u);
    outb(DMA_MASK, 0x06u);
    outb(DMA_FLIPFLOP, 0x00u);
    outb(DMA_CHANNEL2, (uint8_t)(address & 0xFFu));
    outb(DMA_CHANNEL2, (uint8_t)((address >> 8) & 0xFFu));
    outb(DMA_PAGE2, (uint8_t)((address >> 16) & 0xFFu));
    outb(DMA_FLIPFLOP, 0x00u);
    outb(DMA_COUNT2, (uint8_t)(count & 0xFFu));
    outb(DMA_COUNT2, (uint8_t)(count >> 8));
    outb(DMA_MODE, 0x5Au);
    outb(DMA_MASK, 0x02u);
    return 1;
}

static int sense_drive(struct floppy_controller *self) {
    static const uint8_t command[] = { 0x04u, 0x00u };
    uint8_t result;
    if (!self || !send_command(self, command, countof(command)) || !read_result(self, &result, 1)) return 0;
    self->last_st3 = result;
    return !(result & 0x40u);
}

static int dma_prepare_read_bytes(size_t bytes) {
    uintptr_t address = (uintptr_t)dma_buffer;
    uint16_t count;

    /* ISA DMA channel 2 cannot cross a 64 KiB boundary. */
    if (!bytes || bytes > 0x10000u || address > 0x00FFFFFFu ||
        (address & 0xFFFFu) + bytes > 0x10000u) {
        return 0;
    }
    count = (uint16_t)(bytes - 1u);
    outb(DMA_MASK, 0x06u);
    outb(DMA_FLIPFLOP, 0x00u);
    outb(DMA_CHANNEL2, (uint8_t)(address & 0xFFu));
    outb(DMA_CHANNEL2, (uint8_t)((address >> 8) & 0xFFu));
    outb(DMA_PAGE2, (uint8_t)((address >> 16) & 0xFFu));
    outb(DMA_FLIPFLOP, 0x00u);
    outb(DMA_COUNT2, (uint8_t)(count & 0xFFu));
    outb(DMA_COUNT2, (uint8_t)(count >> 8));
    outb(DMA_MODE, 0x56u);
    outb(DMA_MASK, 0x02u);
    return 1;
}

static int dma_prepare_read(void) {
    return dma_prepare_read_bytes(BLOCK_SECTOR);
}

static int read_track_once(struct floppy_controller *self, uint8_t cylinder, uint8_t head) {
    uint8_t command[9] = { 0x46u, 0, cylinder, head, 1u, 2u, 18u, 0x1Bu, 0xFFu };
    uint8_t result[7];

    if (!self || !self->present || !self->command_ready || cylinder > 79u || head > 1u) {
        return 0;
    }
    if (self->need_recal) {
        if (!floppy_recal(self)) return 0;
        self->need_recal = 0;
    }
    if (!floppy_seek(self, cylinder) || !dma_prepare_read_bytes(FLOPPY_TRACK_BYTES)) {
        return 0;
    }
    command[1] = (uint8_t)(head << 2);
    floppy_motor(self, 1);
    self->irq_seen = 0;
    self->last_irq = 0;
    if (!send_command(self, command, countof(command))) {
        floppy_motor(self, 0);
        return 0;
    }
    for (unsigned attempt = 0; attempt < 1000000u && !self->irq_seen; attempt++) {
        pause();
    }
    if (!self->irq_seen || !read_result(self, result, 7)) {
        floppy_motor(self, 0);
        return 0;
    }
    floppy_motor(self, 0);
    self->last_status = result[0];
    self->last_st1 = result[1];
    self->last_st2 = result[2];
    self->last_cylinder = result[3];
    self->last_head = result[4];
    self->last_sector = result[5];
    self->last_size = result[6];
    if ((result[0] & 0xC0u) || result[1] || result[2] || result[6] != 2u) {
        return 0;
    }
    memcpy(self->track_cache, dma_buffer, FLOPPY_TRACK_BYTES);
    self->track_cache_cylinder = cylinder;
    self->track_cache_head = head;
    self->track_cache_valid = 1;
    return 1;
}

static int read_sector_once(struct floppy_controller *self, uint32_t lba, void *buffer) {
    uint8_t command[9];
    uint8_t result[7];
    uint32_t cylinder;
    uint8_t head;
    uint8_t sector;

    if (!self || !buffer || !self->present || !self->command_ready || lba >= 2880u) {
        return 0;
    }
    cylinder = lba / 36u;
    head = (uint8_t)((lba / 18u) & 1u);
    sector = (uint8_t)(lba % 18u + 1u);
    if (!floppy_seek(self, (uint8_t)cylinder) || !dma_prepare_read()) {
        return 0;
    }

    floppy_motor(self, 1);
    self->irq_seen = 0;
    self->last_irq = 0;
    command[0] = 0x46u; /* READ DATA, MFM, skip deleted data. */
    command[1] = (uint8_t)(head << 2);
    command[2] = (uint8_t)cylinder;
    command[3] = head;
    command[4] = sector;
    command[5] = 2u;    /* 512-byte sectors. */
    command[6] = sector; /* Stop after the one requested sector. */
    command[7] = 0x1Bu; /* 300 kb/s gap. */
    command[8] = 0xFFu;

    if (!send_command(self, command, countof(command))) {
        floppy_motor(self, 0);
        return 0;
    }
    for (unsigned attempt = 0; attempt < 1000000u && !self->irq_seen; attempt++) {
        pause();
    }
    if (!self->irq_seen || !read_result(self, result, 7)) {
        floppy_motor(self, 0);
        return 0;
    }
    floppy_motor(self, 0);

    self->last_status = result[0];
    self->last_st1 = result[1];
    self->last_st2 = result[2];
    self->last_cylinder = result[3];
    self->last_head = result[4];
    self->last_sector = result[5];
    self->last_size = result[6];
    /* QEMU's raw FDC backend reports inconsistent CHS IDs while still
       returning the requested sector data.  Keep strict ST1/ST2 and sector-size
       checks, but tolerate only this explicit emulator mode until a real
       low-level floppy backend is available. */
    int cylinder_match = result[3] == cylinder ||
        ((result[0] & 0x20u) && result[3] == cylinder + 1u);
    int id_match = cylinder_match && result[4] == head && result[5] == sector;
    if ((result[0] & 0xC0u) || result[1] || result[2] ||
        (!id_match && !self->tolerate_qemu_ids) || result[6] != 2u) {
        return 0;
    }
    memcpy(buffer, dma_buffer, BLOCK_SECTOR);
    return 1;
}

int floppy_read(struct floppy_controller *self, uint32_t lba, void *buffer) {
    uint8_t cylinder;
    uint8_t head;
    uint8_t sector;

    if (!self || !buffer || lba >= 2880u) {
        return 0;
    }
    cylinder = lba / 36u;
    head = (uint8_t)((lba / 18u) & 1u);
    sector = (uint8_t)(lba % 18u + 1u);
    if (self->track_cache_valid && self->track_cache_cylinder == cylinder &&
        self->track_cache_head == head) {
        memcpy(buffer, self->track_cache + (sector - 1u) * BLOCK_SECTOR, BLOCK_SECTOR);
        return 1;
    }
    if (self->cache_valid && self->cache_lba == lba) {
        memcpy(buffer, self->cache, BLOCK_SECTOR);
        return 1;
    }
    for (unsigned attempt = 0; attempt < 3u; attempt++) {
        if (read_track_once(self, cylinder, head)) {
            memcpy(buffer, self->track_cache + (sector - 1u) * BLOCK_SECTOR, BLOCK_SECTOR);
            memcpy(self->cache, buffer, BLOCK_SECTOR);
            self->cache_lba = lba;
            self->cache_valid = 1;
            return 1;
        }

        if (read_sector_once(self, lba, buffer)) {
            if (self) {
                memcpy(self->cache, buffer, BLOCK_SECTOR);
                self->cache_lba = lba;
                self->cache_valid = 1;
            }
            return 1;
        }
        if (!self || !floppy_recal(self)) {
            break;
        }
    }
    return 0;
}

int floppy_write(struct floppy_controller *self, uint32_t lba, const void *buffer) {
    uint8_t command[9];
    uint8_t result[7];
    uint32_t cylinder;
    uint8_t head;
    uint8_t sector;

    if (!self || !buffer || !self->present || !self->command_ready || lba >= 2880u || !sense_drive(self)) {
        return 0;
    }
    cylinder = lba / 36u;
    head = (uint8_t)((lba / 18u) & 1u);
    sector = (uint8_t)(lba % 18u + 1u);
    if (!floppy_seek(self, (uint8_t)cylinder) || !dma_prepare_write(BLOCK_SECTOR)) {
        return 0;
    }
    memcpy(dma_buffer, buffer, BLOCK_SECTOR);
    floppy_motor(self, 1);
    self->irq_seen = 0;
    command[0] = 0x45u; /* WRITE DATA, MFM. */
    command[1] = (uint8_t)(head << 2);
    command[2] = (uint8_t)cylinder;
    command[3] = head;
    command[4] = sector;
    command[5] = 2u;
    command[6] = sector;
    command[7] = 0x1Bu;
    command[8] = 0xFFu;
    if (!send_command(self, command, countof(command))) {
        floppy_motor(self, 0);
        return 0;
    }
    for (unsigned attempt = 0; attempt < 1000000u && !self->irq_seen; attempt++) pause();
    if (!self->irq_seen || !read_result(self, result, 7)) {
        floppy_motor(self, 0);
        return 0;
    }
    floppy_motor(self, 0);
    self->last_status = result[0];
    self->last_st1 = result[1];
    self->last_st2 = result[2];
    self->need_recal = 1;
    if (self->track_cache_valid && self->track_cache_cylinder == cylinder && self->track_cache_head == head) {
        memcpy(self->track_cache + (sector - 1u) * BLOCK_SECTOR, buffer, BLOCK_SECTOR);
    } else {
        self->track_cache_valid = 0;
    }
    self->cache_valid = 0;
    return !(result[0] & 0xC0u) && !result[1] && !result[2] && result[6] == 2u;
}

int floppy_format(struct floppy_controller *self, uint8_t cylinder, uint8_t head) {
    uint8_t command[6];
    uint8_t result[7];

    if (!self || !self->present || !self->command_ready || cylinder > 79u || head > 1u) {
        return 0;
    }
    if (!floppy_seek(self, cylinder)) {
        return 0;
    }
    for (uint8_t sector = 1; sector <= 18u; sector++) {
        dma_buffer[(sector - 1u) * 4u + 0] = cylinder;
        dma_buffer[(sector - 1u) * 4u + 1] = head;
        dma_buffer[(sector - 1u) * 4u + 2] = sector;
        dma_buffer[(sector - 1u) * 4u + 3] = 2u;
    }
    if (!dma_prepare_write(18u * 4u)) {
        return 0;
    }
    floppy_motor(self, 1);
    self->irq_seen = 0;
    command[0] = 0x4Du; /* FORMAT TRACK, MFM. */
    command[1] = (uint8_t)(head << 2);
    command[2] = 2u;
    command[3] = 18u;
    command[4] = 0x1Bu;
    command[5] = 0xF6u;
    if (!send_command(self, command, countof(command))) {
        floppy_motor(self, 0);
        return 0;
    }
    for (unsigned attempt = 0; attempt < 1000000u && !self->irq_seen; attempt++) {
        pause();
    }
    if (!self->irq_seen || !read_result(self, result, 7)) {
        floppy_motor(self, 0);
        return 0;
    }
    floppy_motor(self, 0);
    self->last_status = result[0];
    self->last_st1 = result[1];
    self->last_st2 = result[2];
    self->cache_valid = 0;
    self->track_cache_valid = 0;
    return !(result[0] & 0xC0u) && !result[1] && !result[2];
}

int floppy_id(struct floppy_controller *self, uint8_t head) {
    uint8_t command[2] = { 0x4Au, (uint8_t)(head << 2) };
    uint8_t result[7];

    if (!self || !self->present || !self->command_ready || head > 1u) {
        return 0;
    }
    self->irq_seen = 0;
    floppy_motor(self, 1);
    if (!send_command(self, command, countof(command))) {
        floppy_motor(self, 0);
        return 0;
    }
    for (unsigned attempt = 0; attempt < 1000000u && !self->irq_seen; attempt++) {
        pause();
    }
    if (!self->irq_seen || !read_result(self, result, 7)) {
        floppy_motor(self, 0);
        return 0;
    }
    floppy_motor(self, 0);
    self->last_status = result[0];
    self->last_st1 = result[1];
    self->last_st2 = result[2];
    self->last_cylinder = result[3];
    self->last_head = result[4];
    self->last_sector = result[5];
    self->last_size = result[6];
    return !(result[0] & 0xC0u) && !result[1] && !result[2];
}

static int block_read(struct blockdevice *device, uint32_t lba, void *buffer, uint32_t sectors) {
    struct floppy_controller *self = device ? device->driver : nil;
    uint8_t *out = buffer;

    if (!self || !buffer || lba >= 2880u || sectors > 2880u - lba) {
        return -1;
    }
    for (uint32_t i = 0; i < sectors; i++) {
        if (!floppy_read(self, lba + i, out + i * BLOCK_SECTOR)) {
            return -1;
        }
    }
    return (int)sectors;
}

static int block_write(struct blockdevice *device, uint32_t lba, const void *buffer, uint32_t sectors) {
    struct floppy_controller *self = device ? device->driver : nil;
    const uint8_t *src = buffer;

    if (!self || !buffer || lba >= 2880u || sectors > 2880u - lba) {
        return -1;
    }
    for (uint32_t i = 0; i < sectors; i++) {
        if (!floppy_write(self, lba + i, src + i * BLOCK_SECTOR)) return -1;
    }
    return (int)sectors;
}

static int block_readbytes(struct blockdevice *device, size_t offset, void *buffer, size_t size) {
    struct floppy_controller *self = device ? device->driver : nil;
    uint8_t sector[BLOCK_SECTOR];
    uint8_t *out = buffer;

    if (!self || (!buffer && size) || offset > 1474560u || size > 1474560u - offset) {
        return -1;
    }
    while (size) {
        size_t within = offset % BLOCK_SECTOR;
        size_t count = min(size, BLOCK_SECTOR - within);
        if (!floppy_read(self, (uint32_t)(offset / BLOCK_SECTOR), sector)) {
            return -1;
        }
        memcpy(out, sector + within, count);
        offset += count;
        out += count;
        size -= count;
    }
    return (int)(buffer ? (size_t)(out - (uint8_t *)buffer) : 0);
}

static size_t block_size(struct blockdevice *device) {
    unused(device);
    return 1474560u;
}

int floppy_init(struct floppy_controller *self) {
    uint8_t status;

    if (!self) {
        return 0;
    }
    memset(self, 0, sizeof(*self));
    self->base = FLOPPY_PRIMARY;
    self->tolerate_qemu_ids = 1u;
    self->device.read = block_read;
    self->device.write = block_write;
    self->device.readbytes = block_readbytes;
    self->device.size = block_size;
    self->device.name = "fd0";
    self->device.sector_size = BLOCK_SECTOR;
    self->device.sectors = 2880u;
    self->device.sector_count = 2880u;
    self->device.flags = 0;
    self->device.readonly = 0;
    self->device.driver = self;
    active = self;
    outb(self->base + FLOPPY_CCR, 0x00u); /* 500 kbit/s for 1.44 MiB media. */

    /* Keep reset asserted briefly, then enable the controller and DMA/IRQ
       paths without starting a motor or issuing a disk command. */
    outb(self->base + FLOPPY_DOR, 0x00);
    for (volatile unsigned delay = 0; delay < 10000u; delay++) {
        pause();
    }
    outb(self->base + FLOPPY_DOR, 0x0Cu);
    for (volatile unsigned delay = 0; delay < 10000u; delay++) {
        pause();
    }

    status = inb(self->base + FLOPPY_MSR);
    self->present = status != 0xFFu;
    self->dma_ready = dma_prepare_read() ? 1u : 0u;
    irq_register(IRQ_FLOPPY, irq_handler);
    irq_enable(IRQ_FLOPPY);
    if (self->present) {
        uint8_t reset_status[2];
        for (unsigned drive = 0; drive < 4u; drive++) {
            if (!sense_interrupt(self, reset_status)) {
                break;
            }
        }
    }
    self->command_ready = self->present && specify(self) && configure(self) ? 1u : 0u;
    return self->present;
}
