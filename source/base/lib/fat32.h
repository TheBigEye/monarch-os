#ifndef MONARCH_BASE_LIB_FAT32_H
#define MONARCH_BASE_LIB_FAT32_H 1

/**
 * @file fat32.h
 * @brief Pure FAT32 boot-sector/BPB parser helpers.
 *
 * These helpers do not perform disk I/O.  They only validate and interpret the
 * first sector of a FAT volume, producing derived layout values that kernel
 * filesystem code and userspace inspection tools can share.
 */

#include "base/api/monarch.h"

#define FAT32_SECTOR_SIZE 512u
#define FAT_TYPE_UNKNOWN 0u
#define FAT_TYPE_12      12u
#define FAT_TYPE_16      16u
#define FAT_TYPE_32      32u

struct fat32_info {
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fat_count;
    uint16_t root_entry_count;
    uint32_t total_sectors;
    uint32_t sectors_per_fat;
    uint32_t hidden_sectors;
    uint32_t root_cluster;
    uint16_t fsinfo_sector;
    uint16_t backup_boot_sector;
    uint32_t root_dir_sectors;
    uint32_t first_fat_sector;
    uint32_t first_data_sector;
    uint32_t data_sectors;
    uint32_t total_clusters;
    uint32_t fat_type;
    char label[12];
    char system[9];
};

/** Parse and validate a FAT boot sector.  Returns non-zero on success. */
int fat32_parse_bpb(const void *sector, size_t size, struct fat32_info *out);

/** Return non-zero when `info` describes a FAT32 volume. */
int fat32_is_fat32(const struct fat32_info *info);

#endif /* MONARCH_BASE_LIB_FAT32_H */
