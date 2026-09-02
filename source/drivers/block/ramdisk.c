#include "drivers/block/ramdisk.h"

static int read_impl(struct blockdevice *self, uint32_t lba, void *buffer, uint32_t sectors) {
    struct ramdisk *disk = self->driver;
    size_t offset = (size_t)lba * self->sector_size;
    size_t count = (size_t)sectors * self->sector_size;

    if (!disk || !buffer || offset + count > disk->bytes) {
        return -1;
    }

    memcpy(buffer, disk->data + offset, count);
    return (int)sectors;
}

static int write_impl(struct blockdevice *self, uint32_t lba, const void *buffer, uint32_t sectors) {
    struct ramdisk *disk = self->driver;
    size_t offset = (size_t)lba * self->sector_size;
    size_t count = (size_t)sectors * self->sector_size;

    if (!disk || !buffer || self->readonly || offset + count > disk->bytes) {
        return -1;
    }

    memcpy(disk->data + offset, buffer, count);
    return (int)sectors;
}


static int readbytes_impl(struct blockdevice *self, size_t offset, void *buffer, size_t size) {
    struct ramdisk *disk = self->driver;

    if (!disk || (!buffer && size) || offset > disk->bytes || size > disk->bytes - offset) {
        return -1;
    }

    if (size) {
        memcpy(buffer, disk->data + offset, size);
    }
    return (int)size;
}

static size_t size_impl(struct blockdevice *self) {
    struct ramdisk *disk = self->driver;
    return disk ? disk->bytes : 0;
}

void ramdisk(struct ramdisk *disk, const char *name, void *data, size_t size, int readonly) {
    memset(disk, 0, sizeof(*disk));
    disk->data = data;
    disk->bytes = size;

    disk->device.read = read_impl;
    disk->device.write = write_impl;
    disk->device.readbytes = readbytes_impl;
    disk->device.size = size_impl;
    disk->device.name = name ? name : "ramdisk";
    disk->device.sector_size = BLOCK_SECTOR;
    disk->device.sector_count = (uint64_t)(size / BLOCK_SECTOR);
    disk->device.sectors = (uint32_t)disk->device.sector_count;
    disk->device.readonly = readonly ? 1 : 0;
    disk->device.flags = readonly ? BLOCK_READONLY : 0;
    disk->device.driver = disk;
}
