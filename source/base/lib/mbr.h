#ifndef MONARCH_BASE_LIB_MBR_H
#define MONARCH_BASE_LIB_MBR_H 1

/**
 * @file mbr.h
 * @brief Tiny Master Boot Record partition-table parser.
 *
 * The parser is intentionally read-only and small.  It extracts the four primary
 * entries from the classic DOS/MBR partition table using the LBA start/count
 * fields.  Extended partitions, CHS interpretation, and GPT are later layers.
 */

#include "base/api/monarch.h"

#define MBR_SECTOR_SIZE 512u
#define MBR_PARTITIONS 4u

struct mbr_partition {
    uint8_t status;
    uint8_t type;
    uint32_t first_lba;
    uint32_t sectors;
};

struct mbr_table {
    int valid;
    struct mbr_partition part[MBR_PARTITIONS];
};

/** Parse one 512-byte MBR sector. Returns non-zero when the 0x55AA signature is present. */
int mbr_parse(const void *sector, size_t size, struct mbr_table *out);

/** Return a short human-readable name for common MBR partition type bytes. */
const char *mbr_type_name(uint8_t type);

#endif /* MONARCH_BASE_LIB_MBR_H */
