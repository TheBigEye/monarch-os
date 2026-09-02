#ifndef MONARCH_BASE_LIB_EXT2_H
#define MONARCH_BASE_LIB_EXT2_H 1

/**
 * @file ext2.h
 * @brief Pure EXT2 superblock parser helpers.
 */

#include "base/api/monarch.h"

#define EXT2_SUPER_OFFSET 1024u
#define EXT2_SUPER_SIZE   1024u
#define EXT2_MAGIC        0xEF53u
#define EXT2_ROOT_INO     2u

#define EXT2_S_IFDIR 0x4000u
#define EXT2_S_IFREG 0x8000u

#define EXT2_FT_UNKNOWN 0u
#define EXT2_FT_REG_FILE 1u
#define EXT2_FT_DIR 2u

struct ext2_info {
    uint32_t inodes_count;
    uint32_t blocks_count;
    uint32_t free_blocks_count;
    uint32_t free_inodes_count;
    uint32_t first_data_block;
    uint32_t block_size;
    uint32_t blocks_per_group;
    uint32_t inodes_per_group;
    uint32_t groups_count;
    uint32_t inode_size;
    uint32_t first_inode;
    uint32_t feature_compat;
    uint32_t feature_incompat;
    uint32_t feature_ro_compat;
};

/** Parse an EXT2 superblock. Returns non-zero on success. */
int ext2_parse_super(const void *superblock, size_t size, struct ext2_info *out);

#endif /* MONARCH_BASE_LIB_EXT2_H */
