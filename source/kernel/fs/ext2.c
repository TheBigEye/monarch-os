#include "kernel/fs/ext2.h"
#include "arch/x86/rtc.h"

/* The first write phase deliberately reuses existing direct data blocks.  It
   changes file contents and sizes without attempting bitmap allocation yet. */

struct inode_view {
    uint16_t mode;
    uint32_t size;
    uint32_t atime;
    uint32_t ctime;
    uint32_t mtime;
    uint32_t block[15];
    size_t disk_offset;
};

struct found_inode {
    int found;
    uint32_t ino;
    struct inode_view inode;
};

static int write_bytes(struct ext2 *self, size_t offset, const void *buffer, size_t size);

static uint16_t le16(const void *ptr) {
    const uint8_t *p = ptr;
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t le32(const void *ptr) {
    const uint8_t *p = ptr;
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static void put16(void *ptr, uint16_t value) {
    uint8_t *p = ptr;
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

static void put32(void *ptr, uint32_t value) {
    uint8_t *p = ptr;
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static int leap_year(uint32_t year) {
    return (year % 4u == 0 && year % 100u != 0) || year % 400u == 0;
}

static uint32_t ext2_time_now(void) {
    static const uint8_t days_in_month[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    struct datetime now;
    uint32_t days = 0;

    rtc(&now);
    if (now.year < 1970u || now.month < 1u || now.month > 12u || now.day < 1u || now.day > 31u ||
        now.hour > 23u || now.minute > 59u || now.second > 59u) {
        return 0;
    }
    for (uint32_t year = 1970u; year < now.year; year++) {
        days += leap_year(year) ? 366u : 365u;
    }
    for (uint32_t month = 1u; month < now.month; month++) {
        days += days_in_month[month - 1u];
        if (month == 2u && leap_year(now.year)) {
            days++;
        }
    }
    days += now.day - 1u;
    return days * 86400u + (uint32_t)now.hour * 3600u + (uint32_t)now.minute * 60u + now.second;
}

static int write_inode_times(struct ext2 *self, size_t inode_offset, uint32_t when) {
    return write_bytes(self, inode_offset + 8u, &when, sizeof(when)) &&
           write_bytes(self, inode_offset + 12u, &when, sizeof(when)) &&
           write_bytes(self, inode_offset + 16u, &when, sizeof(when));
}

static int read_bytes(struct ext2 *self, size_t offset, void *buffer, size_t size) {
    return self && self->device && self->device->readbytes &&
        self->device->readbytes(self->device, offset, buffer, size) == (int)size;
}

static int write_bytes(struct ext2 *self, size_t offset, const void *buffer, size_t size) {
    uint8_t sector[BLOCK_SECTOR];
    const uint8_t *src = buffer;

    if (!self || !self->device || !self->device->readbytes || !self->device->write || self->device->readonly) {
        return 0;
    }

    while (size) {
        size_t sector_offset = offset % BLOCK_SECTOR;
        size_t count = min(size, BLOCK_SECTOR - sector_offset);
        size_t sector_start = offset - sector_offset;

        if (!read_bytes(self, sector_start, sector, sizeof(sector))) {
            return 0;
        }
        memcpy(sector + sector_offset, src, count);
        if (self->device->write(self->device, (uint32_t)(sector_start / BLOCK_SECTOR), sector, 1) != 1) {
            return 0;
        }
        offset += count;
        src += count;
        size -= count;
    }

    return 1;
}

static int read_block(struct ext2 *self, uint32_t block, void *buffer) {
    return read_bytes(self, (size_t)block * self->info.block_size, buffer, self->info.block_size);
}

static int read_group_desc(struct ext2 *self, uint32_t group, uint8_t *desc) {
    size_t offset;

    if (!self || !desc || group >= self->info.groups_count) {
        return 0;
    }
    offset = (size_t)self->group_desc_block * self->info.block_size + (size_t)group * 32u;
    return read_bytes(self, offset, desc, 32u);
}

static uint32_t free_bits(const uint8_t *map, uint32_t count) {
    uint32_t free_count = 0;
    for (uint32_t i = 0; i < count; i++) if (!(map[i / 8u] & (uint8_t)(1u << (i % 8u)))) free_count++;
    return free_count;
}

static int check_meta(struct ext2 *self) {
    uint8_t desc[32];
    uint8_t block_map[EXT2_MAX_BLOCK];
    uint8_t inode_map[EXT2_MAX_BLOCK];
    uint32_t blocks = 0;
    uint32_t inodes = 0;
    for (uint32_t group = 0; group < self->info.groups_count; group++) {
        uint32_t block_base = group * self->info.blocks_per_group;
        uint32_t inode_base = group * self->info.inodes_per_group;
        uint32_t block_count = min(self->info.blocks_per_group, self->info.blocks_count - block_base);
        uint32_t inode_count = min(self->info.inodes_per_group, self->info.inodes_count - inode_base);
        if (!read_group_desc(self, group, desc) || block_count > self->info.block_size * 8u || inode_count > self->info.block_size * 8u ||
            !read_block(self, le32(desc) , block_map) || !read_block(self, le32(desc + 4), inode_map)) return 0;
        if (free_bits(block_map, block_count) != le16(desc + 12) || free_bits(inode_map, inode_count) != le16(desc + 14)) return 0;
        blocks += free_bits(block_map, block_count);
        inodes += free_bits(inode_map, inode_count);
    }
    return blocks == self->info.free_blocks_count && inodes == self->info.free_inodes_count;
}

static int write_group_desc(struct ext2 *self, uint32_t group, const uint8_t *desc) {
    size_t offset;

    if (!self || !desc || group >= self->info.groups_count) {
        return 0;
    }
    offset = (size_t)self->group_desc_block * self->info.block_size + (size_t)group * 32u;
    return write_bytes(self, offset, desc, 32u);
}

static int read_group_inode_table(struct ext2 *self, uint32_t group, uint32_t *out_block) {
    uint8_t desc[32];

    if (!out_block || !read_group_desc(self, group, desc)) {
        return 0;
    }
    *out_block = le32(desc + 8);
    return *out_block != 0;
}

static int read_inode(struct ext2 *self, uint32_t ino, struct inode_view *out) {
    uint8_t raw[256];
    uint32_t group;
    uint32_t index;
    uint32_t table;
    size_t offset;

    if (!self || !out || ino == 0 || ino > self->info.inodes_count || self->info.inode_size > sizeof(raw)) {
        return 0;
    }

    group = (ino - 1u) / self->info.inodes_per_group;
    index = (ino - 1u) % self->info.inodes_per_group;
    if (!read_group_inode_table(self, group, &table)) {
        return 0;
    }

    offset = (size_t)table * self->info.block_size + (size_t)index * self->info.inode_size;
    if (!read_bytes(self, offset, raw, self->info.inode_size)) {
        return 0;
    }

    memset(out, 0, sizeof(*out));
    out->disk_offset = offset;
    out->mode = le16(raw + 0);
    out->size = le32(raw + 4);
    out->atime = le32(raw + 8);
    out->ctime = le32(raw + 12);
    out->mtime = le32(raw + 16);
    for (int i = 0; i < 15; i++) {
        out->block[i] = le32(raw + 40 + i * 4);
    }
    return 1;
}

static enum ext2_node_type inode_type(const struct inode_view *inode) {
    if (!inode) {
        return EXT2_NONE;
    }
    if ((inode->mode & 0xF000u) == EXT2_S_IFDIR) {
        return EXT2_DIR;
    }
    if ((inode->mode & 0xF000u) == EXT2_S_IFREG) {
        return EXT2_FILE;
    }
    return EXT2_NONE;
}

static uint32_t data_block(struct ext2 *self, const struct inode_view *inode, uint32_t logical) {
    uint8_t indirect[EXT2_MAX_BLOCK];
    uint8_t second[EXT2_MAX_BLOCK];
    uint8_t third[EXT2_MAX_BLOCK];
    uint32_t per_block;

    if (!self || !inode) {
        return 0;
    }

    per_block = self->info.block_size / 4u;
    if (logical < 12u) {
        return inode->block[logical];
    }

    logical -= 12u;
    if (logical < per_block) {
        if (!inode->block[12] || !read_block(self, inode->block[12], indirect)) {
            return 0;
        }
        return le32(indirect + logical * 4u);
    }

    logical -= per_block;
    if (logical < per_block * per_block) {
        uint32_t outer = logical / per_block;
        uint32_t inner = logical % per_block;
        uint32_t second_block;

        if (!inode->block[13] || !read_block(self, inode->block[13], indirect)) return 0;
        second_block = le32(indirect + outer * 4u);
        if (!second_block || !read_block(self, second_block, second)) return 0;
        return le32(second + inner * 4u);
    }

    logical -= per_block * per_block;
    if (logical < per_block * per_block * per_block && inode->block[14]) {
        uint32_t outer = logical / (per_block * per_block);
        uint32_t middle = (logical / per_block) % per_block;
        uint32_t inner = logical % per_block;
        uint32_t second_block;
        uint32_t third_block;

        if (!read_block(self, inode->block[14], indirect)) return 0;
        second_block = le32(indirect + outer * 4u);
        if (!second_block || !read_block(self, second_block, second)) return 0;
        third_block = le32(second + middle * 4u);
        if (!third_block || !read_block(self, third_block, third)) return 0;
        return le32(third + inner * 4u);
    }

    return 0;
}

static char lower_ascii(char ch) {
    return ch >= 'A' && ch <= 'Z' ? (char)(ch + ('a' - 'A')) : ch;
}

static int same_name(const char *a, const char *b) {
    while (*a && *b) {
        if (lower_ascii(*a++) != lower_ascii(*b++)) {
            return 0;
        }
    }
    return *a == '\0' && *b == '\0';
}

static int dot_name(const char *name) {
    return strcmp(name, ".") == 0 || strcmp(name, "..") == 0;
}

static enum ext2_node_type dirent_type(struct ext2 *self, uint8_t file_type, uint32_t ino) {
    struct inode_view inode;

    if (file_type == EXT2_FT_DIR) {
        return EXT2_DIR;
    }
    if (file_type == EXT2_FT_REG_FILE) {
        return EXT2_FILE;
    }
    if (read_inode(self, ino, &inode)) {
        return inode_type(&inode);
    }
    return EXT2_NONE;
}

static int scan_directory(struct ext2 *self, const struct inode_view *dir, const char *wanted, struct found_inode *found, ext2_iter iter, void *ctx) {
    uint8_t block[EXT2_MAX_BLOCK];
    uint32_t remaining;
    uint32_t logical = 0;

    if (!self || !dir || inode_type(dir) != EXT2_DIR) {
        return 0;
    }

    remaining = dir->size;
    while (remaining) {
        uint32_t disk_block = data_block(self, dir, logical++);
        uint32_t limit = min(remaining, self->info.block_size);
        uint32_t off = 0;

        if (!disk_block || !read_block(self, disk_block, block)) {
            return 0;
        }

        while (off + 8u <= limit) {
            uint32_t ino = le32(block + off);
            uint16_t rec_len = le16(block + off + 4);
            uint8_t name_len = block[off + 6];
            uint8_t file_type = block[off + 7];
            char name[EXT2_NAME];
            enum ext2_node_type type;

            if (rec_len < 8u || off + rec_len > limit) {
                return 0;
            }

            if (ino && name_len && 8u + name_len <= rec_len) {
                memcpy(name, block + off + 8, name_len);
                name[name_len] = '\0';
                if (!dot_name(name)) {
                    type = dirent_type(self, file_type, ino);
                    if (wanted && same_name(name, wanted)) {
                        if (found) {
                            found->found = read_inode(self, ino, &found->inode);
                            found->ino = ino;
                        }
                        return 1;
                    }
                    if (!wanted && iter) {
                        struct inode_view child;
                        size_t size = 0;
                        if (read_inode(self, ino, &child)) {
                            size = child.size;
                        }
                        iter(ctx, name, type, size);
                    }
                }
            }

            off += rec_len;
        }

        remaining -= limit;
    }

    return 1;
}

static const char *next_component(const char *path, char *out, size_t size) {
    size_t used = 0;

    while (path && *path == '/') {
        path++;
    }
    if (!path || !*path) {
        return nil;
    }

    while (*path && *path != '/') {
        if (used + 1u < size) {
            out[used++] = *path;
        }
        path++;
    }
    out[used] = '\0';
    return used ? path : nil;
}

static int lookup(struct ext2 *self, const char *path, struct found_inode *out) {
    const char *cursor = path ? path : "/";
    struct found_inode current;
    char part[EXT2_NAME];

    memset(out, 0, sizeof(*out));
    out->ino = EXT2_ROOT_INO;
    if (!read_inode(self, EXT2_ROOT_INO, &out->inode)) {
        return 0;
    }
    out->found = 1;

    while (*cursor == '/') {
        cursor++;
    }
    if (!*cursor) {
        return 1;
    }

    while ((cursor = next_component(cursor, part, sizeof(part))) != nil) {
        memset(&current, 0, sizeof(current));
        if (!scan_directory(self, &out->inode, part, &current, nil, nil) || !current.found) {
            return 0;
        }

        while (*cursor == '/') {
            cursor++;
        }
        *out = current;
        if (!*cursor) {
            return 1;
        }
        if (inode_type(&out->inode) != EXT2_DIR) {
            return 0;
        }
    }

    return 0;
}

static int stat_impl(struct ext2 *self, const char *path, struct ext2_stat *out) {
    struct found_inode found;

    if (!self || !self->mounted || !out || !lookup(self, path, &found)) {
        return 0;
    }

    out->type = inode_type(&found.inode);
    out->size = found.inode.size;
    out->atime = found.inode.atime;
    out->ctime = found.inode.ctime;
    out->mtime = found.inode.mtime;
    return out->type != EXT2_NONE;
}

static int read_impl(struct ext2 *self, const char *path, size_t offset, char *buffer, size_t size) {
    struct found_inode found;
    uint8_t block[EXT2_MAX_BLOCK];
    uint32_t logical;
    size_t done = 0;

    if (!self || !self->mounted || !buffer || !size || !lookup(self, path, &found) || inode_type(&found.inode) != EXT2_FILE) {
        return -1;
    }
    if (offset >= found.inode.size) {
        return 0;
    }
    if (size > found.inode.size - offset) {
        size = found.inode.size - offset;
    }

    logical = (uint32_t)(offset / self->info.block_size);
    offset %= self->info.block_size;

    while (done < size) {
        uint32_t disk_block = data_block(self, &found.inode, logical++);
        size_t count;

        if (!disk_block || !read_block(self, disk_block, block)) {
            return done ? (int)done : -1;
        }
        count = min(size - done, self->info.block_size - offset);
        memcpy(buffer + done, block + offset, count);
        done += count;
        offset = 0;
    }

    return (int)done;
}

static int write_block(struct ext2 *self, uint32_t block, const void *buffer);

static int update_super_count(struct ext2 *self, size_t offset, int32_t delta) {
    uint32_t value;
    if (!read_bytes(self, EXT2_SUPER_OFFSET + offset, &value, sizeof(value))) {
        return 0;
    }
    value = (uint32_t)((int32_t)value + delta);
    return write_bytes(self, EXT2_SUPER_OFFSET + offset, &value, sizeof(value));
}

static int allocate_bit(struct ext2 *self, int inode, uint32_t *out_number) {
    uint8_t bitmap[EXT2_MAX_BLOCK];
    uint8_t desc[32];

    if (!self || !out_number) {
        return 0;
    }

    for (uint32_t group = 0; group < self->info.groups_count; group++) {
        uint32_t base = group * (inode ? self->info.inodes_per_group : self->info.blocks_per_group);
        uint32_t total = inode ? self->info.inodes_count : self->info.blocks_count;
        uint32_t limit = min(inode ? self->info.inodes_per_group : self->info.blocks_per_group, total - base);

        if (!read_group_desc(self, group, desc)) {
            return 0;
        }
        uint32_t bitmap_block = le32(desc + (inode ? 4 : 0));
        if (!bitmap_block || limit > self->info.block_size * 8u || !read_block(self, bitmap_block, bitmap)) {
            return 0;
        }

        for (uint32_t bit = 0; bit < limit; bit++) {
            if (!(bitmap[bit / 8u] & (uint8_t)(1u << (bit % 8u)))) {
                uint32_t number = inode ? base + bit + 1u : base + bit;
                bitmap[bit / 8u] |= (uint8_t)(1u << (bit % 8u));
                if (!write_block(self, bitmap_block, bitmap)) {
                    return 0;
                }

                uint16_t offset = inode ? 14u : 12u;
                uint16_t free_count = le16(desc + offset);
                if (!free_count || !update_super_count(self, inode ? 16u : 12u, -1)) {
                    bitmap[bit / 8u] &= (uint8_t)~(1u << (bit % 8u));
                    write_block(self, bitmap_block, bitmap);
                    return 0;
                }
                put16(desc + offset, (uint16_t)(free_count - 1u));
                if (!write_group_desc(self, group, desc)) {
                    update_super_count(self, inode ? 16u : 12u, 1);
                    bitmap[bit / 8u] &= (uint8_t)~(1u << (bit % 8u));
                    write_block(self, bitmap_block, bitmap);
                    return 0;
                }
                *out_number = number;
                return 1;
            }
        }
    }
    return 0;
}

static int write_block(struct ext2 *self, uint32_t block, const void *buffer) {
    return write_bytes(self, (size_t)block * self->info.block_size, buffer, self->info.block_size);
}

static int free_bit(struct ext2 *self, int inode, uint32_t number) {
    uint8_t bitmap[EXT2_MAX_BLOCK];
    uint8_t desc[32];
    uint32_t group;
    uint32_t bit;
    uint32_t bitmap_block;
    uint16_t free_count;

    if (!self || !number) {
        return 0;
    }
    group = inode ? (number - 1u) / self->info.inodes_per_group : number / self->info.blocks_per_group;
    bit = inode ? (number - 1u) % self->info.inodes_per_group : number % self->info.blocks_per_group;
    if (group >= self->info.groups_count || !read_group_desc(self, group, desc)) {
        return 0;
    }
    bitmap_block = le32(desc + (inode ? 4 : 0));
    if (!bitmap_block || bit >= self->info.block_size * 8u || !read_block(self, bitmap_block, bitmap) ||
        !(bitmap[bit / 8u] & (uint8_t)(1u << (bit % 8u)))) {
        return 0;
    }

    bitmap[bit / 8u] &= (uint8_t)~(1u << (bit % 8u));
    if (!write_block(self, bitmap_block, bitmap)) {
        return 0;
    }
    free_count = le16(desc + (inode ? 14u : 12u));
    put16(desc + (inode ? 14u : 12u), (uint16_t)(free_count + 1u));
    if (!update_super_count(self, inode ? 16u : 12u, 1) || !write_group_desc(self, group, desc)) {
        return 0;
    }
    return 1;
}

static int remove_directory_entry(struct ext2 *self, const struct inode_view *dir, const char *name, uint32_t expected_ino) {
    uint8_t block[EXT2_MAX_BLOCK];

    if (!self || !dir || inode_type(dir) != EXT2_DIR) {
        return 0;
    }
    for (uint32_t logical = 0; logical < 12u && data_block(self, dir, logical); logical++) {
        uint32_t disk_block = data_block(self, dir, logical);
        uint32_t off = 0;
        uint32_t previous = 0;
        if (!read_block(self, disk_block, block)) {
            return 0;
        }
        while (off + 8u <= self->info.block_size) {
            uint32_t ino = le32(block + off);
            uint16_t rec_len = le16(block + off + 4);
            uint8_t name_len = block[off + 6];
            if (rec_len < 8u || off + rec_len > self->info.block_size) {
                return 0;
            }
            if (ino == expected_ino && name_len == strlen(name) && memcmp(block + off + 8, name, name_len) == 0) {
                uint16_t previous_len = le16(block + previous + 4);
                if (previous != off) {
                    put16(block + previous + 4, (uint16_t)(previous_len + rec_len));
                } else {
                    memset(block + off, 0, rec_len);
                    put16(block + off + 4, rec_len);
                }
                return write_block(self, disk_block, block);
            }
            previous = off;
            off += rec_len;
        }
    }
    return 0;
}

static int free_inode_blocks(struct ext2 *self, const struct inode_view *inode) {
    uint8_t first[EXT2_MAX_BLOCK];
    uint8_t second[EXT2_MAX_BLOCK];
    uint8_t third[EXT2_MAX_BLOCK];
    uint32_t per_block;

    if (!self || !inode) {
        return 0;
    }
    per_block = self->info.block_size / 4u;
    for (unsigned i = 0; i < 12u; i++) {
        if (inode->block[i] && !free_bit(self, 0, inode->block[i])) {
            return 0;
        }
    }
    if (inode->block[12]) {
        if (!read_block(self, inode->block[12], first)) {
            return 0;
        }
        for (uint32_t i = 0; i < per_block; i++) {
            uint32_t data = le32(first + i * 4u);
            if (data && !free_bit(self, 0, data)) {
                return 0;
            }
        }
        if (!free_bit(self, 0, inode->block[12])) {
            return 0;
        }
    }
    if (inode->block[13]) {
        if (!read_block(self, inode->block[13], first)) return 0;
        for (uint32_t i = 0; i < per_block; i++) {
            uint32_t second_block = le32(first + i * 4u);
            if (!second_block) continue;
            if (!read_block(self, second_block, second)) return 0;
            for (uint32_t j = 0; j < per_block; j++) {
                uint32_t data = le32(second + j * 4u);
                if (data && !free_bit(self, 0, data)) return 0;
            }
            if (!free_bit(self, 0, second_block)) return 0;
        }
        if (!free_bit(self, 0, inode->block[13])) return 0;
    }
    if (inode->block[14]) {
        if (!read_block(self, inode->block[14], first)) return 0;
        for (uint32_t i = 0; i < per_block; i++) {
            uint32_t second_block = le32(first + i * 4u);
            if (!second_block) continue;
            if (!read_block(self, second_block, second)) return 0;
            for (uint32_t j = 0; j < per_block; j++) {
                uint32_t third_block = le32(second + j * 4u);
                if (!third_block) continue;
                if (!read_block(self, third_block, third)) return 0;
                for (uint32_t k = 0; k < per_block; k++) {
                    uint32_t data = le32(third + k * 4u);
                    if (data && !free_bit(self, 0, data)) return 0;
                }
                if (!free_bit(self, 0, third_block)) return 0;
            }
            if (!free_bit(self, 0, second_block)) return 0;
        }
        if (!free_bit(self, 0, inode->block[14])) return 0;
    }
    return 1;
}

static int unlink_impl(struct ext2 *self, const char *path) {
    char parent_path[EXT2_NAME];
    char name[EXT2_NAME];
    const char *slash;
    struct found_inode parent;
    struct found_inode found;

    if (!self || self->device->readonly || !path || !*path || !strcmp(path, "/")) {
        return 0;
    }
    slash = strrchr(path, '/');
    if (!slash) {
        return 0;
    }
    strncpy(name, slash + 1, sizeof(name));
    if (!name[0]) {
        return 0;
    }
    if (slash == path) {
        strncpy(parent_path, "/", sizeof(parent_path));
    } else {
        size_t length = (size_t)(slash - path);
        if (length >= sizeof(parent_path)) {
            return 0;
        }
        memcpy(parent_path, path, length);
        parent_path[length] = '\0';
    }
    if (!lookup(self, parent_path, &parent) || inode_type(&parent.inode) != EXT2_DIR) {
        return 0;
    }
    memset(&found, 0, sizeof(found));
    if (!scan_directory(self, &parent.inode, name, &found, nil, nil) || !found.found || inode_type(&found.inode) != EXT2_FILE) {
        return 0;
    }
    if (!remove_directory_entry(self, &parent.inode, name, found.ino)) {
        return 0;
    }
    if (!free_inode_blocks(self, &found.inode) || !free_bit(self, 1, found.ino)) {
        return 0;
    }
    write_inode_times(self, parent.inode.disk_offset, ext2_time_now());
    return 1;
}

static int add_directory_entry(struct ext2 *self, const struct inode_view *dir, uint32_t ino, const char *name, uint8_t file_type) {
    uint8_t block[EXT2_MAX_BLOCK];
    size_t name_len = strlen(name);
    uint32_t needed = (uint32_t)((8u + name_len + 3u) & ~3u);

    if (!self || !dir || inode_type(dir) != EXT2_DIR || name_len == 0 || name_len > 255u || needed > self->info.block_size) {
        return 0;
    }

    for (uint32_t logical = 0; logical < 12u && data_block(self, dir, logical); logical++) {
        uint32_t disk_block = data_block(self, dir, logical);
        if (!read_block(self, disk_block, block)) {
            return 0;
        }
        uint32_t off = 0;
        while (off + 8u <= self->info.block_size) {
            uint32_t ino_here = le32(block + off);
            uint16_t rec_len = le16(block + off + 4);
            uint8_t old_len = block[off + 6];
            uint32_t used = (uint32_t)((8u + old_len + 3u) & ~3u);

            if (rec_len < 8u || off + rec_len > self->info.block_size || used > rec_len) {
                return 0;
            }
            if (off + rec_len == self->info.block_size && rec_len - used >= needed) {
                put16(block + off + 4, (uint16_t)used);
                uint8_t *entry = block + off + used;
                memset(entry, 0, rec_len - used);
                put32(entry, ino);
                put16(entry + 4, (uint16_t)(rec_len - used));
                entry[6] = (uint8_t)name_len;
                entry[7] = file_type;
                memcpy(entry + 8, name, name_len);
                return write_block(self, disk_block, block);
            }
            if (!ino_here && rec_len >= needed) {
                memset(block + off, 0, rec_len);
                put32(block + off, ino);
                put16(block + off + 4, rec_len);
                block[off + 6] = (uint8_t)name_len;
                block[off + 7] = file_type;
                memcpy(block + off + 8, name, name_len);
                return write_block(self, disk_block, block);
            }
            off += rec_len;
        }
    }
    return 0;
}

static int create_impl(struct ext2 *self, const char *path) {
    char parent_path[EXT2_NAME];
    char name[EXT2_NAME];
    const char *slash;
    struct found_inode parent;
    struct found_inode existing;
    struct inode_view inode;
    uint32_t ino;
    uint32_t table;
    uint32_t group;
    uint32_t index;
    uint8_t raw[256];

    if (!self || self->device->readonly || !path || !*path) {
        return 0;
    }
    slash = strrchr(path, '/');
    if (!slash) {
        return 0;
    }
    strncpy(name, slash + 1, sizeof(name));
    if (!name[0] || strlen(name) > 255u) {
        return 0;
    }
    if (slash == path) {
        strncpy(parent_path, "/", sizeof(parent_path));
    } else {
        size_t length = (size_t)(slash - path);
        if (length >= sizeof(parent_path)) {
            return 0;
        }
        memcpy(parent_path, path, length);
        parent_path[length] = '\0';
    }
    if (!lookup(self, parent_path, &parent) || inode_type(&parent.inode) != EXT2_DIR) {
        return 0;
    }
    memset(&existing, 0, sizeof(existing));
    if (scan_directory(self, &parent.inode, name, &existing, nil, nil) && existing.found) {
        return inode_type(&existing.inode) == EXT2_FILE;
    }
    if (!allocate_bit(self, 1, &ino)) {
        return 0;
    }
    group = (ino - 1u) / self->info.inodes_per_group;
    index = (ino - 1u) % self->info.inodes_per_group;
    if (!read_group_inode_table(self, group, &table)) {
        free_bit(self, 1, ino);
        return 0;
    }
    memset(raw, 0, sizeof(raw));
    put16(raw, 0x81A4u);
    put32(raw + 4, 0);
    put16(raw + 26, 1);
    size_t inode_offset = (size_t)table * self->info.block_size + (size_t)index * self->info.inode_size;
    if (!write_bytes(self, inode_offset, raw, self->info.inode_size) || !write_inode_times(self, inode_offset, ext2_time_now())) {
        free_bit(self, 1, ino);
        return 0;
    }
    memset(&inode, 0, sizeof(inode));
    inode.mode = 0x81A4u;
    inode.disk_offset = (size_t)table * self->info.block_size + (size_t)index * self->info.inode_size;
    if (!add_directory_entry(self, &parent.inode, ino, name, EXT2_FT_REG_FILE)) {
        free_bit(self, 1, ino);
        return 0;
    }
    write_inode_times(self, parent.inode.disk_offset, ext2_time_now());
    return 1;
}

static int mkdir_impl(struct ext2 *self, const char *path) {
    char parent_path[EXT2_NAME];
    char name[EXT2_NAME];
    const char *slash;
    struct found_inode parent;
    struct found_inode existing;
    uint32_t ino;
    uint32_t block_number;
    uint32_t table;
    uint32_t index;
    uint8_t raw[256];
    uint8_t block[EXT2_MAX_BLOCK];
    size_t length;

    if (!self || self->device->readonly || !path || !*path || !strcmp(path, "/")) {
        return 0;
    }
    slash = strrchr(path, '/');
    if (!slash) {
        return 0;
    }
    strncpy(name, slash + 1, sizeof(name));
    if (!name[0] || strlen(name) > 255u) {
        return 0;
    }
    if (slash == path) {
        strncpy(parent_path, "/", sizeof(parent_path));
    } else {
        length = (size_t)(slash - path);
        if (length >= sizeof(parent_path)) {
            return 0;
        }
        memcpy(parent_path, path, length);
        parent_path[length] = '\0';
    }
    if (!lookup(self, parent_path, &parent) || inode_type(&parent.inode) != EXT2_DIR) {
        return 0;
    }
    memset(&existing, 0, sizeof(existing));
    if (scan_directory(self, &parent.inode, name, &existing, nil, nil) && existing.found) {
        return inode_type(&existing.inode) == EXT2_DIR;
    }
    if (!allocate_bit(self, 1, &ino)) {
        return 0;
    }
    if (!allocate_bit(self, 0, &block_number)) {
        free_bit(self, 1, ino);
        return 0;
    }
    uint32_t group = (ino - 1u) / self->info.inodes_per_group;
    index = (ino - 1u) % self->info.inodes_per_group;
    if (!read_group_inode_table(self, group, &table)) {
        free_bit(self, 0, block_number);
        free_bit(self, 1, ino);
        return 0;
    }
    memset(raw, 0, sizeof(raw));
    put16(raw, 0x41EDu);
    put32(raw + 4, self->info.block_size);
    put16(raw + 26, 2);
    put32(raw + 28, self->info.block_size / 512u);
    put32(raw + 40, block_number);
    size_t inode_offset = (size_t)table * self->info.block_size + (size_t)index * self->info.inode_size;
    if (!write_bytes(self, inode_offset, raw, self->info.inode_size) || !write_inode_times(self, inode_offset, ext2_time_now())) {
        free_bit(self, 0, block_number);
        free_bit(self, 1, ino);
        return 0;
    }

    memset(block, 0, sizeof(block));
    put32(block, ino);
    put16(block + 4, 12);
    block[6] = 1;
    block[7] = EXT2_FT_DIR;
    block[8] = '.';
    put32(block + 12, parent.ino);
    put16(block + 16, (uint16_t)(self->info.block_size - 12u));
    block[18] = 2;
    block[19] = EXT2_FT_DIR;
    block[20] = '.';
    block[21] = '.';
    if (!write_block(self, block_number, block) || !add_directory_entry(self, &parent.inode, ino, name, EXT2_FT_DIR)) {
        free_bit(self, 0, block_number);
        free_bit(self, 1, ino);
        return 0;
    }
    /* A subdirectory contributes one link through its `..` entry. */
    uint16_t links = 0;
    if (!read_bytes(self, parent.inode.disk_offset + 26u, &links, sizeof(links))) {
        return 0;
    }
    put16(&links, (uint16_t)(links + 1u));
    if (!write_bytes(self, parent.inode.disk_offset + 26u, &links, sizeof(links))) {
        return 0;
    }
    write_inode_times(self, parent.inode.disk_offset, ext2_time_now());
    return 1;
}

struct ext2_count_context {
    unsigned count;
};

static void count_ext2_entries(void *ctx, const char *name, enum ext2_node_type type, size_t size) {
    struct ext2_count_context *count = ctx;
    unused(name);
    unused(type);
    unused(size);
    count->count++;
}

static int rmdir_impl(struct ext2 *self, const char *path) {
    struct ext2_stat stat;
    struct found_inode found;
    struct found_inode parent;
    char parent_path[EXT2_NAME];
    char name[EXT2_NAME];
    const char *slash;
    struct ext2_count_context count;

    if (!self || self->device->readonly || !path || !*path || !strcmp(path, "/") ||
        !self->stat(self, path, &stat) || stat.type != EXT2_DIR) {
        return 0;
    }
    slash = strrchr(path, '/');
    if (!slash) {
        return 0;
    }
    strncpy(name, slash + 1, sizeof(name));
    if (slash == path) {
        strncpy(parent_path, "/", sizeof(parent_path));
    } else {
        size_t length = (size_t)(slash - path);
        if (length >= sizeof(parent_path)) {
            return 0;
        }
        memcpy(parent_path, path, length);
        parent_path[length] = '\0';
    }
    if (!lookup(self, parent_path, &parent) || !lookup(self, path, &found)) {
        return 0;
    }
    count.count = 0;
    self->each(self, path, count_ext2_entries, &count);
    if (count.count || !remove_directory_entry(self, &parent.inode, name, found.ino)) {
        return 0;
    }
    if (found.inode.block[0] && !free_bit(self, 0, found.inode.block[0])) {
        return 0;
    }
    if (!free_bit(self, 1, found.ino)) {
        return 0;
    }
    uint16_t links = 0;
    if (!read_bytes(self, parent.inode.disk_offset + 26u, &links, sizeof(links)) || !links) {
        return 0;
    }
    put16(&links, (uint16_t)(links - 1u));
    if (!write_bytes(self, parent.inode.disk_offset + 26u, &links, sizeof(links))) {
        return 0;
    }
    write_inode_times(self, parent.inode.disk_offset, ext2_time_now());
    return 1;
}

static int truncate_impl(struct ext2 *self, const char *path) {
    struct found_inode found;
    uint32_t size = 0;

    if (!self || self->device->readonly || !lookup(self, path, &found) || inode_type(&found.inode) != EXT2_FILE) {
        return 0;
    }

    /* Keep existing data blocks allocated for now.  This makes truncation
       safe and lets the first write phase reuse preallocated direct blocks;
       bitmap reclamation will be added with file creation. */
    return write_bytes(self, found.inode.disk_offset + 4u, &size, sizeof(size)) &&
           write_inode_times(self, found.inode.disk_offset, ext2_time_now());
}

static int add_blocks(struct ext2 *self, const struct inode_view *inode, uint32_t blocks) {
    uint32_t value;
    if (!read_bytes(self, inode->disk_offset + 28u, &value, sizeof(value))) return 0;
    value += blocks * (self->info.block_size / 512u);
    return write_bytes(self, inode->disk_offset + 28u, &value, sizeof(value));
}

static int ensure_block(struct ext2 *self, struct inode_view *inode, uint32_t logical, uint32_t *out) {
    uint8_t first[EXT2_MAX_BLOCK];
    uint8_t second[EXT2_MAX_BLOCK];
    uint8_t third[EXT2_MAX_BLOCK];
    uint32_t per = self->info.block_size / 4u;

    if (logical < 12u) {
        *out = inode->block[logical];
        if (*out) return 1;
        if (!allocate_bit(self, 0, out) || !write_bytes(self, inode->disk_offset + 40u + logical * 4u, out, 4) || !add_blocks(self, inode, 1)) return 0;
        inode->block[logical] = *out;
        return 1;
    }
    logical -= 12u;
    if (logical < per) {
        if (!inode->block[12]) {
            if (!allocate_bit(self, 0, &inode->block[12])) return 0;
            memset(first, 0, self->info.block_size);
            if (!write_block(self, inode->block[12], first) || !write_bytes(self, inode->disk_offset + 40u + 12u * 4u, &inode->block[12], 4) || !add_blocks(self, inode, 1)) return 0;
        }
        if (!read_block(self, inode->block[12], first)) return 0;
        *out = le32(first + logical * 4u);
        if (*out) return 1;
        if (!allocate_bit(self, 0, out) || !write_bytes(self, (size_t)inode->block[12] * self->info.block_size + logical * 4u, out, 4) || !add_blocks(self, inode, 1)) return 0;
        return 1;
    }
    logical -= per;
    if (logical < per * per) {
        uint32_t outer = logical / per;
        uint32_t inner = logical % per;
        if (!inode->block[13]) {
            if (!allocate_bit(self, 0, &inode->block[13])) return 0;
            memset(first, 0, self->info.block_size);
            if (!write_block(self, inode->block[13], first) || !write_bytes(self, inode->disk_offset + 40u + 13u * 4u, &inode->block[13], 4) || !add_blocks(self, inode, 1)) return 0;
        }
        if (!read_block(self, inode->block[13], first)) return 0;
        uint32_t second_block = le32(first + outer * 4u);
        if (!second_block) {
            if (!allocate_bit(self, 0, &second_block)) return 0;
            memset(second, 0, self->info.block_size);
            if (!write_block(self, second_block, second) || !write_bytes(self, (size_t)inode->block[13] * self->info.block_size + outer * 4u, &second_block, 4) || !add_blocks(self, inode, 1)) return 0;
        }
        if (!read_block(self, second_block, second)) return 0;
        *out = le32(second + inner * 4u);
        if (*out) return 1;
        if (!allocate_bit(self, 0, out) || !write_bytes(self, (size_t)second_block * self->info.block_size + inner * 4u, out, 4) || !add_blocks(self, inode, 1)) return 0;
        return 1;
    }
    logical -= per * per;
    if (logical >= per * per * per) return 0;
    uint32_t outer = logical / (per * per);
    uint32_t middle = (logical / per) % per;
    uint32_t inner = logical % per;
    if (!inode->block[14]) {
        if (!allocate_bit(self, 0, &inode->block[14])) return 0;
        memset(first, 0, self->info.block_size);
        if (!write_block(self, inode->block[14], first) || !write_bytes(self, inode->disk_offset + 40u + 14u * 4u, &inode->block[14], 4) || !add_blocks(self, inode, 1)) return 0;
    }
    if (!read_block(self, inode->block[14], first)) return 0;
    uint32_t second_block = le32(first + outer * 4u);
    if (!second_block) {
        if (!allocate_bit(self, 0, &second_block)) return 0;
        memset(second, 0, self->info.block_size);
        if (!write_block(self, second_block, second) || !write_bytes(self, (size_t)inode->block[14] * self->info.block_size + outer * 4u, &second_block, 4) || !add_blocks(self, inode, 1)) return 0;
    }
    if (!read_block(self, second_block, second)) return 0;
    uint32_t third_block = le32(second + middle * 4u);
    if (!third_block) {
        if (!allocate_bit(self, 0, &third_block)) return 0;
        memset(third, 0, self->info.block_size);
        if (!write_block(self, third_block, third) || !write_bytes(self, (size_t)second_block * self->info.block_size + middle * 4u, &third_block, 4) || !add_blocks(self, inode, 1)) return 0;
    }
    if (!read_block(self, third_block, third)) return 0;
    *out = le32(third + inner * 4u);
    if (*out) return 1;
    if (!allocate_bit(self, 0, out) || !write_bytes(self, (size_t)third_block * self->info.block_size + inner * 4u, out, 4) || !add_blocks(self, inode, 1)) return 0;
    return 1;
}

static int write_impl(struct ext2 *self, const char *path, size_t offset, const char *buffer, size_t size) {
    struct found_inode found;
    size_t done = 0;
    size_t original;

    if (!self || self->device->readonly || (!buffer && size) || offset > 0xFFFFFFFFu || size > 0xFFFFFFFFu - offset ||
        !lookup(self, path, &found) || inode_type(&found.inode) != EXT2_FILE) {
        return -1;
    }
    if (!size) {
        return 0;
    }

    original = found.inode.size;
    while (done < size) {
        size_t position = offset + done;
        uint32_t logical = (uint32_t)(position / self->info.block_size);
        size_t within = position % self->info.block_size;
        size_t count = min(size - done, self->info.block_size - within);
        uint32_t disk_block = data_block(self, &found.inode, logical);

        if (!disk_block && !ensure_block(self, &found.inode, logical, &disk_block)) {
            return done ? (int)done : -1;
        }

        if (!disk_block || !write_bytes(self,
                (size_t)disk_block * self->info.block_size + within,
                buffer + done, count)) {
            return done ? (int)done : -1;
        }
        done += count;
    }

    if (offset + done > original) {
        uint32_t new_size = (uint32_t)(offset + done);
        if (!write_bytes(self, found.inode.disk_offset + 4u, &new_size, sizeof(new_size))) {
            return done ? (int)done : -1;
        }
    }
    if (!write_inode_times(self, found.inode.disk_offset, ext2_time_now())) {
        return done ? (int)done : -1;
    }
    return (int)done;
}

static void each_impl(struct ext2 *self, const char *path, ext2_iter iter, void *ctx) {
    struct found_inode found;

    if (!self || !self->mounted || !iter || !lookup(self, path, &found) || inode_type(&found.inode) != EXT2_DIR) {
        return;
    }
    (void)scan_directory(self, &found.inode, nil, nil, iter, ctx);
}

int ext2(struct ext2 *self, struct blockdevice *device) {
    uint8_t super[EXT2_SUPER_SIZE];

    if (!self || !device || !device->readbytes) {
        return 0;
    }

    memset(self, 0, sizeof(*self));
    self->stat = stat_impl;
    self->create = create_impl;
    self->mkdir = mkdir_impl;
    self->rmdir = rmdir_impl;
    self->unlink = unlink_impl;
    self->truncate = truncate_impl;
    self->read = read_impl;
    self->write = write_impl;
    self->each = each_impl;
    self->device = device;

    if (device->readbytes(device, EXT2_SUPER_OFFSET, super, sizeof(super)) != (int)sizeof(super)) {
        return 0;
    }
    if (!ext2_parse_super(super, sizeof(super), &self->info)) {
        return 0;
    }
    /* The current adapter understands directory file-type bytes only.  Do not
       mount extents, 64-bit, recovery or journaled volumes as if they were
       plain EXT2; their metadata rules need different write/recovery paths. */
    if (self->info.block_size > EXT2_MAX_BLOCK ||
        (self->info.feature_incompat & ~0x00000002u) ||
        (self->info.feature_compat & 0x00000004u)) {
        return 0;
    }

    self->group_desc_block = self->info.block_size == 1024u ? 2u : 1u;
    if (!check_meta(self)) return 0;
    self->mounted = 1;
    return 1;
}
