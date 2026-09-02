/**
 * @file ext2.c
 * @brief Parse the EXT2 superblock into a compact layout summary.
 */

#include "base/lib/ext2.h"

static uint16_t le16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t le32(const uint8_t *p) {
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

int ext2_parse_super(const void *superblock, size_t size, struct ext2_info *out) {
    const uint8_t *s = superblock;
    uint32_t log_block_size;
    uint32_t revision;

    if (!s || !out || size < EXT2_SUPER_SIZE) {
        return 0;
    }
    if (le16(s + 56) != EXT2_MAGIC) {
        return 0;
    }

    memset(out, 0, sizeof(*out));
    out->inodes_count = le32(s + 0);
    out->blocks_count = le32(s + 4);
    out->free_blocks_count = le32(s + 12);
    out->free_inodes_count = le32(s + 16);
    out->first_data_block = le32(s + 20);
    log_block_size = le32(s + 24);
    out->blocks_per_group = le32(s + 32);
    out->inodes_per_group = le32(s + 40);
    revision = le32(s + 76);

    if (log_block_size > 2u) {
        return 0;
    }
    out->block_size = 1024u << log_block_size;
    if (!out->inodes_count || !out->blocks_count || !out->blocks_per_group || !out->inodes_per_group) {
        return 0;
    }

    out->groups_count = (out->blocks_count + out->blocks_per_group - 1u) / out->blocks_per_group;
    if (!out->groups_count) {
        return 0;
    }

    if (revision >= 1u) {
        out->first_inode = le32(s + 84);
        out->inode_size = le16(s + 88);
        out->feature_compat = le32(s + 92);
        out->feature_incompat = le32(s + 96);
        out->feature_ro_compat = le32(s + 100);
    } else {
        out->first_inode = 11u;
        out->inode_size = 128u;
    }

    if (out->inode_size < 128u || out->inode_size > out->block_size) {
        return 0;
    }

    return 1;
}
