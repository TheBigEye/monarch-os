/**
 * @file fat32.c
 * @brief FAT boot-sector/BPB parser and FAT type classifier.
 */

#include "base/lib/fat32.h"

static uint16_t le16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t le32(const uint8_t *p) {
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static int power_of_two(uint32_t value) {
    return value && (value & (value - 1u)) == 0;
}

static uint32_t classify(uint32_t clusters) {
    if (clusters < 4085u) {
        return FAT_TYPE_12;
    }
    if (clusters < 65525u) {
        return FAT_TYPE_16;
    }
    return FAT_TYPE_32;
}

int fat32_parse_bpb(const void *sector, size_t size, struct fat32_info *out) {
    const uint8_t *b = sector;
    uint16_t total16;
    uint32_t total32;
    uint16_t fat16;
    uint32_t fat32;
    uint32_t fat_sectors;
    uint32_t root_dir_sectors;
    uint32_t first_data;
    uint32_t data_sectors;
    uint32_t total_clusters;

    if (!b || !out || size < FAT32_SECTOR_SIZE) {
        return 0;
    }
    if (b[510] != 0x55 || b[511] != 0xAA) {
        return 0;
    }

    memset(out, 0, sizeof(*out));
    out->bytes_per_sector = le16(b + 11);
    out->sectors_per_cluster = b[13];
    out->reserved_sectors = le16(b + 14);
    out->fat_count = b[16];
    out->root_entry_count = le16(b + 17);
    total16 = le16(b + 19);
    fat16 = le16(b + 22);
    out->hidden_sectors = le32(b + 28);
    total32 = le32(b + 32);
    fat32 = le32(b + 36);

    if (!power_of_two(out->bytes_per_sector) || out->bytes_per_sector < 512u || out->bytes_per_sector > 4096u) {
        return 0;
    }
    if (!power_of_two(out->sectors_per_cluster) || out->sectors_per_cluster == 0) {
        return 0;
    }
    if (!out->reserved_sectors || !out->fat_count) {
        return 0;
    }

    out->total_sectors = total16 ? total16 : total32;
    out->sectors_per_fat = fat16 ? fat16 : fat32;
    if (!out->total_sectors || !out->sectors_per_fat) {
        return 0;
    }

    root_dir_sectors = ((uint32_t)out->root_entry_count * 32u + (out->bytes_per_sector - 1u)) / out->bytes_per_sector;
    fat_sectors = (uint32_t)out->fat_count * out->sectors_per_fat;
    if (out->total_sectors <= (uint32_t)out->reserved_sectors + fat_sectors + root_dir_sectors) {
        return 0;
    }

    first_data = (uint32_t)out->reserved_sectors + fat_sectors + root_dir_sectors;
    data_sectors = out->total_sectors - first_data;
    total_clusters = data_sectors / out->sectors_per_cluster;

    out->root_dir_sectors = root_dir_sectors;
    out->first_fat_sector = out->reserved_sectors;
    out->first_data_sector = first_data;
    out->data_sectors = data_sectors;
    out->total_clusters = total_clusters;
    out->fat_type = classify(total_clusters);

    if (out->fat_type == FAT_TYPE_32) {
        out->root_cluster = le32(b + 44);
        out->fsinfo_sector = le16(b + 48);
        out->backup_boot_sector = le16(b + 50);
        memcpy(out->label, b + 71, 11);
        out->label[11] = '\0';
        memcpy(out->system, b + 82, 8);
        out->system[8] = '\0';
        if (out->root_entry_count != 0 || fat16 != 0 || out->root_cluster < 2u) {
            return 0;
        }
    } else {
        out->root_cluster = 0;
        memcpy(out->label, b + 43, 11);
        out->label[11] = '\0';
        memcpy(out->system, b + 54, 8);
        out->system[8] = '\0';
    }

    return 1;
}

int fat32_is_fat32(const struct fat32_info *info) {
    return info && info->fat_type == FAT_TYPE_32;
}
