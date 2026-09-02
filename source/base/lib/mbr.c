/**
 * @file mbr.c
 * @brief Classic DOS/MBR primary partition parser.
 */

#include "base/lib/mbr.h"

static uint32_t le32(const uint8_t *p) {
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

int mbr_parse(const void *sector, size_t size, struct mbr_table *out) {
    const uint8_t *mbr = sector;

    if (!mbr || !out || size < MBR_SECTOR_SIZE) {
        return 0;
    }

    memset(out, 0, sizeof(*out));
    if (mbr[510] != 0x55 || mbr[511] != 0xAA) {
        return 0;
    }

    out->valid = 1;
    for (unsigned i = 0; i < MBR_PARTITIONS; i++) {
        const uint8_t *entry = mbr + 446u + i * 16u;
        out->part[i].status = entry[0];
        out->part[i].type = entry[4];
        out->part[i].first_lba = le32(entry + 8);
        out->part[i].sectors = le32(entry + 12);
    }
    return 1;
}

const char *mbr_type_name(uint8_t type) {
    switch (type) {
        case 0x00: return "empty";
        case 0x01: return "fat12";
        case 0x04: return "fat16-small";
        case 0x05: return "extended";
        case 0x06: return "fat16";
        case 0x07: return "ntfs/exfat/hpfs";
        case 0x0B: return "fat32";
        case 0x0C: return "fat32-lba";
        case 0x0E: return "fat16-lba";
        case 0x0F: return "extended-lba";
        case 0x82: return "linux-swap";
        case 0x83: return "linux";
        case 0xA5: return "freebsd";
        case 0xEE: return "gpt-protective";
        default: return "unknown";
    }
}
