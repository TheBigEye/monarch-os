#include "drivers/block/partition.h"

static int in_range(struct partition *part, uint32_t lba, uint32_t sectors) {
    if (!part || sectors == 0) {
        return 0;
    }
    if (lba >= part->sector_count32) {
        return 0;
    }
    if (sectors > part->sector_count32 - lba) {
        return 0;
    }
    return 1;
}

static int read_impl(struct blockdevice *self, uint32_t lba, void *buffer, uint32_t sectors) {
    struct partition *part = self->driver;

    if (!part || !part->parent || !buffer || !in_range(part, lba, sectors)) {
        return -1;
    }
    return part->parent->read(part->parent, part->start_lba + lba, buffer, sectors);
}

static int write_impl(struct blockdevice *self, uint32_t lba, const void *buffer, uint32_t sectors) {
    struct partition *part = self->driver;

    if (!part || !part->parent || !buffer || self->readonly || !in_range(part, lba, sectors)) {
        return -1;
    }
    return part->parent->write(part->parent, part->start_lba + lba, buffer, sectors);
}

static int readbytes_impl(struct blockdevice *self, size_t offset, void *buffer, size_t size) {
    struct partition *part = self->driver;
    size_t total;
    size_t parent_offset;

    if (!part || !part->parent || (!buffer && size)) {
        return -1;
    }

    total = (size_t)part->sector_count32 * self->sector_size;
    if (offset > total || size > total - offset) {
        return -1;
    }

    parent_offset = (size_t)part->start_lba * self->sector_size + offset;
    if (part->parent->readbytes) {
        return part->parent->readbytes(part->parent, parent_offset, buffer, size);
    }

    if ((offset % self->sector_size) != 0 || (size % self->sector_size) != 0) {
        return -1;
    }
    return read_impl(self, (uint32_t)(offset / self->sector_size), buffer, (uint32_t)(size / self->sector_size)) < 0 ? -1 : (int)size;
}

static size_t size_impl(struct blockdevice *self) {
    struct partition *part = self->driver;
    return part ? (size_t)part->sector_count32 * self->sector_size : 0;
}

void partition(struct partition *part, const char *name, struct blockdevice *parent, uint32_t start_lba, uint32_t sector_count, int readonly) {
    memset(part, 0, sizeof(*part));
    part->parent = parent;
    part->start_lba = start_lba;
    part->sector_count32 = sector_count;
    strncpy(part->name, name ? name : "part", sizeof(part->name));

    part->device.read = read_impl;
    part->device.write = write_impl;
    part->device.readbytes = readbytes_impl;
    part->device.size = size_impl;
    part->device.name = part->name;
    part->device.sector_size = parent ? parent->sector_size : BLOCK_SECTOR;
    part->device.sector_count = sector_count;
    part->device.sectors = sector_count;
    part->device.readonly = readonly || (parent && parent->readonly);
    part->device.flags = BLOCK_PARTITION | (part->device.readonly ? BLOCK_READONLY : 0);
    part->device.driver = part;
}
