#include "kernel/fs/fat32.h"
#include "arch/x86/rtc.h"

#define ATTR_READ_ONLY 0x01u
#define ATTR_HIDDEN    0x02u
#define ATTR_SYSTEM    0x04u
#define ATTR_VOLUME    0x08u
#define ATTR_DIR       0x10u
#define ATTR_ARCHIVE   0x20u
#define ATTR_LFN       0x0Fu
#define FAT32_LFN_SLOTS ((FAT32_LFN + 12u) / 13u)

#define FSINFO_LEAD    0x41615252u
#define FSINFO_STRUCT  0x61417272u
#define FSINFO_TRAIL   0xAA550000u
#define FSINFO_UNKNOWN 0xFFFFFFFFu
#define FSINFO_FREE    488u
#define FSINFO_NEXT    492u

struct dirent32 {
    char name[11];
    uint8_t attr;
    uint8_t ntres;
    uint8_t create_tenth;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t access_date;
    uint16_t cluster_high;
    uint16_t write_time;
    uint16_t write_date;
    uint16_t cluster_low;
    uint32_t size;
} __attribute__((packed));

struct found_entry {
    int found;
    enum fat32_node_type type;
    uint32_t cluster;
    uint32_t size;
};

struct entry_location {
    int found;
    uint32_t sector;
    uint32_t offset;
    uint32_t dir_cluster;
    enum fat32_node_type type;
    uint32_t cluster;
    uint32_t size;
    int has_lfn;
    uint8_t lfn_count;
    uint32_t lfn_sector[FAT32_LFN_SLOTS];
    uint32_t lfn_offset[FAT32_LFN_SLOTS];
};

struct fat_timestamp {
    uint16_t date;
    uint16_t time;
    uint8_t tenth;
};

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

static int read_sector(struct fat32 *self, uint32_t sector, void *buffer) {
    return self && self->device && self->device->read(self->device, sector, buffer, 1) == 1;
}

static int write_sector(struct fat32 *self, uint32_t sector, const void *buffer) {
    return self && self->device && !self->device->readonly && self->device->write(self->device, sector, buffer, 1) == 1;
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

static uint32_t cluster_sector(struct fat32 *self, uint32_t cluster) {
    return self->info.first_data_sector + (cluster - 2u) * self->info.sectors_per_cluster;
}

static int valid_cluster(struct fat32 *self, uint32_t cluster) {
    return self && cluster >= 2u && cluster < self->info.total_clusters + 2u;
}

static int eoc(uint32_t value) {
    return value >= 0x0FFFFFF8u;
}

static uint32_t next_cluster(struct fat32 *self, uint32_t cluster) {
    uint8_t sector[FAT32_SECTOR_SIZE];
    uint32_t offset = cluster * 4u;
    uint32_t fat_sector = self->info.first_fat_sector + offset / self->info.bytes_per_sector;
    uint32_t in_sector = offset % self->info.bytes_per_sector;

    if (!read_sector(self, fat_sector, sector)) {
        return 0x0FFFFFFFu;
    }
    return le32(sector + in_sector) & 0x0FFFFFFFu;
}

static char lower_ascii(char ch) {
    return ch >= 'A' && ch <= 'Z' ? (char)(ch + ('a' - 'A')) : ch;
}

static int make_name(const struct dirent32 *entry, char *out, size_t size) {
    size_t used = 0;
    int has_ext = 0;

    if (!entry || !out || size < 2) {
        return 0;
    }

    for (int i = 0; i < 8 && entry->name[i] != ' '; i++) {
        if (used + 1u >= size) {
            return 0;
        }
        out[used++] = lower_ascii(entry->name[i]);
    }

    for (int i = 8; i < 11; i++) {
        if (entry->name[i] != ' ') {
            has_ext = 1;
        }
    }

    if (has_ext) {
        if (used + 1u >= size) {
            return 0;
        }
        out[used++] = '.';
        for (int i = 8; i < 11 && entry->name[i] != ' '; i++) {
            if (used + 1u >= size) {
                return 0;
            }
            out[used++] = lower_ascii(entry->name[i]);
        }
    }

    out[used] = '\0';
    return used > 0;
}

static int same_name(const char *a, const char *b) {
    while (*a && *b) {
        if (lower_ascii(*a++) != lower_ascii(*b++)) {
            return 0;
        }
    }
    return *a == '\0' && *b == '\0';
}

static uint32_t entry_cluster(const struct dirent32 *entry) {
    return ((uint32_t)le16(&entry->cluster_high) << 16) | le16(&entry->cluster_low);
}

static enum fat32_node_type entry_type(const struct dirent32 *entry) {
    return (entry->attr & ATTR_DIR) ? FAT32_DIR : FAT32_FILE;
}

static int usable_entry(const struct dirent32 *entry) {
    uint8_t first = (uint8_t)entry->name[0];

    if (first == 0x00u) {
        return 0;
    }
    if (first == 0xE5u) {
        return 0;
    }
    if ((entry->attr & ATTR_LFN) == ATTR_LFN) {
        return 0;
    }
    if (entry->attr & ATTR_VOLUME) {
        return 0;
    }
    return 1;
}

static int dot_entry(const char *name) {
    return strcmp(name, ".") == 0 || strcmp(name, "..") == 0;
}

static void lfn_units_reset(uint16_t *units) {
    if (units) {
        units[0] = 0;
    }
}

static int lfn_units_put(uint16_t *units, size_t count, size_t index, uint16_t ch) {
    if (!units || index >= count) {
        return 0;
    }
    if (ch == 0x0000u) {
        units[index] = 0;
        return 0;
    }
    if (ch == 0xFFFFu) {
        return 1;
    }
    units[index] = ch;
    if (index + 1u < count && units[index + 1u] == 0xFFFFu) {
        units[index + 1u] = 0;
    }
    return 1;
}

static int utf8_emit(char *out, size_t size, size_t *used, uint32_t cp) {
    if (cp > 0x10FFFFu || (cp >= 0xD800u && cp <= 0xDFFFu)) {
        cp = '?';
    }

    if (cp < 0x80u) {
        if (*used + 1u >= size) return 0;
        out[(*used)++] = (char)cp;
    } else if (cp < 0x800u) {
        if (*used + 2u >= size) return 0;
        out[(*used)++] = (char)(0xC0u | (cp >> 6));
        out[(*used)++] = (char)(0x80u | (cp & 0x3Fu));
    } else if (cp < 0x10000u) {
        if (*used + 3u >= size) return 0;
        out[(*used)++] = (char)(0xE0u | (cp >> 12));
        out[(*used)++] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
        out[(*used)++] = (char)(0x80u | (cp & 0x3Fu));
    } else {
        if (*used + 4u >= size) return 0;
        out[(*used)++] = (char)(0xF0u | (cp >> 18));
        out[(*used)++] = (char)(0x80u | ((cp >> 12) & 0x3Fu));
        out[(*used)++] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
        out[(*used)++] = (char)(0x80u | (cp & 0x3Fu));
    }
    out[*used] = '\0';
    return 1;
}

static int lfn_to_utf8(const uint16_t *units, char *out, size_t size) {
    size_t used = 0;

    if (!units || !out || !size) {
        return 0;
    }

    out[0] = '\0';
    for (size_t i = 0; i < FAT32_LFN; i++) {
        uint16_t ch = units[i];
        uint32_t cp;

        if (ch == 0x0000u || ch == 0xFFFFu) {
            break;
        }

        if (ch >= 0xD800u && ch <= 0xDBFFu && i + 1u < FAT32_LFN && units[i + 1u] >= 0xDC00u && units[i + 1u] <= 0xDFFFu) {
            cp = 0x10000u + (((uint32_t)ch - 0xD800u) << 10) + ((uint32_t)units[++i] - 0xDC00u);
        } else {
            cp = ch;
        }

        if (!utf8_emit(out, size, &used, cp)) {
            return 0;
        }
    }

    return used > 0;
}

static void lfn_collect(const uint8_t *raw, uint16_t *units, size_t count) {
    static const uint8_t offsets[13] = {
        1, 3, 5, 7, 9,
        14, 16, 18, 20, 22, 24,
        28, 30
    };
    uint8_t order = raw[0];
    uint8_t sequence = order & 0x1Fu;
    size_t base;

    if (!sequence || !units || !count) {
        return;
    }

    base = (size_t)(sequence - 1u) * 13u;
    if (order & 0x40u) {
        for (size_t i = 0; i < count; i++) {
            units[i] = 0xFFFFu;
        }
        if (base + 13u < count) {
            units[base + 13u] = 0;
        }
    }

    for (unsigned i = 0; i < countof(offsets); i++) {
        uint16_t ch = le16(raw + offsets[i]);
        if (!lfn_units_put(units, count, base + i, ch)) {
            break;
        }
    }
}

static int utf8_next(const char **cursor, uint32_t *out) {
    const uint8_t *s = (const uint8_t *)*cursor;
    uint32_t cp;

    if (!s || !*s || !out) {
        return 0;
    }

    if (s[0] < 0x80u) {
        *out = s[0];
        *cursor += 1;
        return 1;
    }
    if ((s[0] & 0xE0u) == 0xC0u && (s[1] & 0xC0u) == 0x80u) {
        cp = ((uint32_t)(s[0] & 0x1Fu) << 6) | (uint32_t)(s[1] & 0x3Fu);
        *out = cp >= 0x80u ? cp : '?';
        *cursor += 2;
        return 1;
    }
    if ((s[0] & 0xF0u) == 0xE0u && (s[1] & 0xC0u) == 0x80u && (s[2] & 0xC0u) == 0x80u) {
        cp = ((uint32_t)(s[0] & 0x0Fu) << 12) | ((uint32_t)(s[1] & 0x3Fu) << 6) | (uint32_t)(s[2] & 0x3Fu);
        *out = (cp >= 0x800u && !(cp >= 0xD800u && cp <= 0xDFFFu)) ? cp : '?';
        *cursor += 3;
        return 1;
    }
    if ((s[0] & 0xF8u) == 0xF0u && (s[1] & 0xC0u) == 0x80u && (s[2] & 0xC0u) == 0x80u && (s[3] & 0xC0u) == 0x80u) {
        cp = ((uint32_t)(s[0] & 0x07u) << 18) | ((uint32_t)(s[1] & 0x3Fu) << 12) | ((uint32_t)(s[2] & 0x3Fu) << 6) | (uint32_t)(s[3] & 0x3Fu);
        *out = (cp >= 0x10000u && cp <= 0x10FFFFu) ? cp : '?';
        *cursor += 4;
        return 1;
    }

    *out = '?';
    *cursor += 1;
    return 1;
}

static size_t utf8_to_utf16_units(const char *text, uint16_t *units, size_t count) {
    size_t used = 0;

    if (!text || !units || !count) {
        return 0;
    }

    while (*text && used + 1u < count) {
        uint32_t cp;
        if (!utf8_next(&text, &cp)) {
            break;
        }
        if (cp > 0x10FFFFu) {
            cp = '?';
        }
        if (cp >= 0x10000u) {
            if (used + 2u >= count) {
                break;
            }
            cp -= 0x10000u;
            units[used++] = (uint16_t)(0xD800u | (cp >> 10));
            units[used++] = (uint16_t)(0xDC00u | (cp & 0x3FFu));
        } else {
            units[used++] = (uint16_t)cp;
        }
    }
    units[used] = 0;
    return used;
}

static uint8_t sfn_checksum(const char name[11]) {
    uint8_t sum = 0;

    for (int i = 0; i < 11; i++) {
        sum = (uint8_t)(((sum & 1u) ? 0x80u : 0u) + (sum >> 1) + (uint8_t)name[i]);
    }
    return sum;
}

static int scan_directory(struct fat32 *self, uint32_t start_cluster, const char *wanted, struct found_entry *found, fat32_iter iter, void *ctx) {
    uint8_t sector[FAT32_SECTOR_SIZE];
    uint32_t cluster = start_cluster;
    uint32_t guard = 0;
    uint16_t lfn_units[FAT32_LFN];
    char lfn[FAT32_LFN];
    uint8_t lfn_sum = 0;
    int lfn_valid = 0;

    if (!valid_cluster(self, cluster)) {
        return 0;
    }
    lfn_units_reset(lfn_units);
    lfn[0] = '\0';

    while (valid_cluster(self, cluster) && guard++ < self->info.total_clusters + 2u) {
        for (uint32_t s = 0; s < self->info.sectors_per_cluster; s++) {
            if (!read_sector(self, cluster_sector(self, cluster) + s, sector)) {
                return 0;
            }

            for (uint32_t off = 0; off < self->info.bytes_per_sector; off += sizeof(struct dirent32)) {
                const struct dirent32 *entry = (const struct dirent32 *)(sector + off);
                const uint8_t *raw = sector + off;
                char short_name[FAT32_NAME];
                const char *display_name;

                if ((uint8_t)entry->name[0] == 0x00u) {
                    return 1;
                }

                if ((entry->attr & ATTR_LFN) == ATTR_LFN) {
                    if (raw[0] & 0x40u) {
                        lfn_sum = raw[13];
                        lfn_valid = 1;
                    } else if (!lfn_valid || lfn_sum != raw[13]) {
                        lfn_units_reset(lfn_units);
                        lfn[0] = '\0';
                        lfn_valid = 0;
                        continue;
                    }
                    lfn_collect(raw, lfn_units, countof(lfn_units));
                    continue;
                }

                if (!usable_entry(entry) || !make_name(entry, short_name, sizeof(short_name))) {
                    lfn_units_reset(lfn_units);
                    lfn[0] = '\0';
                    lfn_valid = 0;
                    continue;
                }

                display_name = (lfn_valid && lfn_sum == sfn_checksum(entry->name) && lfn_to_utf8(lfn_units, lfn, sizeof(lfn))) ? lfn : short_name;
                if (dot_entry(display_name) || dot_entry(short_name)) {
                    lfn_units_reset(lfn_units);
                    lfn[0] = '\0';
                    lfn_valid = 0;
                    continue;
                }

                if (wanted && (same_name(display_name, wanted) || same_name(short_name, wanted))) {
                    if (found) {
                        found->found = 1;
                        found->type = entry_type(entry);
                        found->cluster = entry_cluster(entry);
                        found->size = le32(&entry->size);
                    }
                    return 1;
                }

                if (!wanted && iter) {
                    iter(ctx, display_name, entry_type(entry), le32(&entry->size));
                }
                lfn_units_reset(lfn_units);
                lfn[0] = '\0';
                lfn_valid = 0;
            }
        }

        cluster = next_cluster(self, cluster);
        if (eoc(cluster)) {
            return 1;
        }
    }

    return 0;
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

static int lookup(struct fat32 *self, const char *path, struct found_entry *out) {
    const char *cursor = path ? path : "/";
    uint32_t dir = self->info.root_cluster;
    struct found_entry current;
    char part[FAT32_NAME];

    memset(out, 0, sizeof(*out));

    while (*cursor == '/') {
        cursor++;
    }
    if (!*cursor) {
        out->found = 1;
        out->type = FAT32_DIR;
        out->cluster = self->info.root_cluster;
        out->size = 0;
        return 1;
    }

    while ((cursor = next_component(cursor, part, sizeof(part))) != nil) {
        memset(&current, 0, sizeof(current));
        if (!scan_directory(self, dir, part, &current, nil, nil) || !current.found) {
            return 0;
        }

        while (*cursor == '/') {
            cursor++;
        }
        if (!*cursor) {
            *out = current;
            return 1;
        }

        if (current.type != FAT32_DIR) {
            return 0;
        }
        dir = current.cluster;
    }

    return 0;
}

static int stat_impl(struct fat32 *self, const char *path, struct fat32_stat *out) {
    struct found_entry found;

    if (!self || !self->mounted || !out || !lookup(self, path, &found)) {
        return 0;
    }

    out->type = found.type;
    out->size = found.size;
    return 1;
}

static int read_impl(struct fat32 *self, const char *path, size_t offset, char *buffer, size_t size) {
    struct found_entry file;
    uint8_t sector[FAT32_SECTOR_SIZE];
    uint32_t cluster;
    uint32_t cluster_bytes;
    uint32_t guard = 0;
    size_t done = 0;
    size_t skip;
    size_t cluster_file_offset = 0;

    if (!self || !self->mounted || !buffer || !size || !lookup(self, path, &file) || file.type != FAT32_FILE) {
        return -1;
    }
    if (offset >= file.size) {
        return 0;
    }
    if (size > file.size - offset) {
        size = file.size - offset;
    }

    cluster_bytes = (uint32_t)self->info.sectors_per_cluster * self->info.bytes_per_sector;

    if (self->cache_start_cluster == file.cluster &&
        self->cache_cluster >= 2u &&
        self->cache_offset <= offset &&
        strcmp(self->cache_path, path ? path : "") == 0) {
        cluster = self->cache_cluster;
        cluster_file_offset = self->cache_offset;
        skip = offset - self->cache_offset;
    } else {
        cluster = file.cluster;
        skip = offset;
        cluster_file_offset = 0;
    }

    while (skip >= cluster_bytes && valid_cluster(self, cluster)) {
        skip -= cluster_bytes;
        cluster_file_offset += cluster_bytes;
        cluster = next_cluster(self, cluster);
        if (eoc(cluster)) {
            return 0;
        }
    }

    while (done < size && valid_cluster(self, cluster) && guard++ < self->info.total_clusters + 2u) {
        for (uint32_t s = 0; s < self->info.sectors_per_cluster && done < size; s++) {
            size_t sector_skip = 0;
            size_t count;

            if (skip >= self->info.bytes_per_sector) {
                skip -= self->info.bytes_per_sector;
                continue;
            }
            sector_skip = skip;
            skip = 0;

            if (!read_sector(self, cluster_sector(self, cluster) + s, sector)) {
                return done ? (int)done : -1;
            }

            count = min(size - done, self->info.bytes_per_sector - sector_skip);
            memcpy(buffer + done, sector + sector_skip, count);
            done += count;
        }

        if (done >= size) {
            break;
        }
        cluster = next_cluster(self, cluster);
        cluster_file_offset += cluster_bytes;
        if (eoc(cluster)) {
            break;
        }
    }

    if (done > 0) {
        strncpy(self->cache_path, path ? path : "", sizeof(self->cache_path));
        self->cache_start_cluster = file.cluster;
        self->cache_cluster = cluster;
        self->cache_offset = cluster_file_offset;
    }

    return (int)done;
}

static int set_fat_entry(struct fat32 *self, uint32_t cluster, uint32_t value) {
    uint8_t sector[FAT32_SECTOR_SIZE];
    uint32_t offset = cluster * 4u;
    uint32_t in_sector = offset % self->info.bytes_per_sector;

    if (!valid_cluster(self, cluster)) {
        return 0;
    }

    value &= 0x0FFFFFFFu;
    for (uint8_t fat = 0; fat < self->info.fat_count; fat++) {
        uint32_t fat_sector = self->info.first_fat_sector + (uint32_t)fat * self->info.sectors_per_fat + offset / self->info.bytes_per_sector;
        uint32_t old;

        if (!read_sector(self, fat_sector, sector)) {
            return 0;
        }
        old = le32(sector + in_sector);
        put32(sector + in_sector, (old & 0xF0000000u) | value);
        if (!write_sector(self, fat_sector, sector)) {
            return 0;
        }
    }
    return 1;
}

static int clear_cluster(struct fat32 *self, uint32_t cluster) {
    uint8_t zero[FAT32_SECTOR_SIZE];

    if (!valid_cluster(self, cluster)) {
        return 0;
    }

    memset(zero, 0, sizeof(zero));
    for (uint32_t s = 0; s < self->info.sectors_per_cluster; s++) {
        if (!write_sector(self, cluster_sector(self, cluster) + s, zero)) {
            return 0;
        }
    }
    return 1;
}

static int valid_fsinfo_sector(struct fat32 *self, uint32_t sector) {
    return self && sector > 0 && sector < self->info.reserved_sectors;
}

static int fsinfo_signature_ok(const uint8_t sector[FAT32_SECTOR_SIZE]) {
    return le32(sector + 0) == FSINFO_LEAD &&
           le32(sector + 484) == FSINFO_STRUCT &&
           le32(sector + 508) == FSINFO_TRAIL;
}

static int scan_free_info(struct fat32 *self, uint32_t *out_count, uint32_t *out_next) {
    uint8_t sector[FAT32_SECTOR_SIZE];
    uint32_t free_count = 0;
    uint32_t first_free = FSINFO_UNKNOWN;
    uint32_t entries_per_sector;

    if (!self || !out_count || !out_next || self->info.bytes_per_sector != FAT32_SECTOR_SIZE) {
        return 0;
    }

    entries_per_sector = self->info.bytes_per_sector / 4u;

    for (uint32_t fat_sector = 0; fat_sector < self->info.sectors_per_fat; fat_sector++) {
        if (!read_sector(self, self->info.first_fat_sector + fat_sector, sector)) {
            return 0;
        }
        for (uint32_t entry = 0; entry < entries_per_sector; entry++) {
            uint32_t cluster = fat_sector * entries_per_sector + entry;
            if (valid_cluster(self, cluster) && (le32(sector + entry * 4u) & 0x0FFFFFFFu) == 0) {
                free_count++;
                if (first_free == FSINFO_UNKNOWN) {
                    first_free = cluster;
                }
            }
        }
    }

    *out_count = free_count;
    *out_next = first_free;
    return 1;
}

static void fsinfo_store(struct fat32 *self) {
    uint8_t sector[FAT32_SECTOR_SIZE];
    uint32_t primary;
    uint32_t backup;

    if (!self || !self->fsinfo_valid || !self->device || self->device->readonly) {
        return;
    }

    primary = self->info.fsinfo_sector;
    if (!valid_fsinfo_sector(self, primary) || !read_sector(self, primary, sector) || !fsinfo_signature_ok(sector)) {
        self->fsinfo_valid = 0;
        return;
    }

    put32(sector + FSINFO_FREE, self->fsinfo_free_count);
    put32(sector + FSINFO_NEXT, self->fsinfo_next_free);
    if (!write_sector(self, primary, sector)) {
        self->fsinfo_valid = 0;
        return;
    }

    backup = (uint32_t)self->info.backup_boot_sector + (uint32_t)self->info.fsinfo_sector;
    if (backup != primary && valid_fsinfo_sector(self, backup) && read_sector(self, backup, sector) && fsinfo_signature_ok(sector)) {
        put32(sector + FSINFO_FREE, self->fsinfo_free_count);
        put32(sector + FSINFO_NEXT, self->fsinfo_next_free);
        (void)write_sector(self, backup, sector);
    }
}

static void fsinfo_load(struct fat32 *self) {
    uint8_t sector[FAT32_SECTOR_SIZE];
    uint32_t scanned_count;
    uint32_t scanned_next;
    int need_scan = 0;

    self->fsinfo_valid = 0;
    self->fsinfo_free_count = FSINFO_UNKNOWN;
    self->fsinfo_next_free = FSINFO_UNKNOWN;

    if (!valid_fsinfo_sector(self, self->info.fsinfo_sector) || !read_sector(self, self->info.fsinfo_sector, sector) || !fsinfo_signature_ok(sector)) {
        return;
    }

    self->fsinfo_valid = 1;
    self->fsinfo_free_count = le32(sector + FSINFO_FREE);
    self->fsinfo_next_free = le32(sector + FSINFO_NEXT);

    if (self->fsinfo_free_count != FSINFO_UNKNOWN && self->fsinfo_free_count > self->info.total_clusters) {
        self->fsinfo_free_count = FSINFO_UNKNOWN;
    }
    if (!valid_cluster(self, self->fsinfo_next_free)) {
        self->fsinfo_next_free = FSINFO_UNKNOWN;
    }

    need_scan = self->fsinfo_free_count == FSINFO_UNKNOWN || self->fsinfo_next_free == FSINFO_UNKNOWN;
    if (need_scan && scan_free_info(self, &scanned_count, &scanned_next)) {
        if (self->fsinfo_free_count == FSINFO_UNKNOWN) {
            self->fsinfo_free_count = scanned_count;
        }
        if (self->fsinfo_next_free == FSINFO_UNKNOWN) {
            self->fsinfo_next_free = scanned_next;
        }
    }
}

static uint32_t cluster_after(struct fat32 *self, uint32_t cluster) {
    uint32_t next = cluster + 1u;
    uint32_t end = self->info.total_clusters + 2u;

    if (next >= end) {
        return 2u;
    }
    return next;
}

static void fsinfo_record_alloc(struct fat32 *self, uint32_t cluster) {
    if (!self || !self->fsinfo_valid) {
        return;
    }

    if (self->fsinfo_free_count != FSINFO_UNKNOWN && self->fsinfo_free_count > 0) {
        self->fsinfo_free_count--;
    }
    self->fsinfo_next_free = cluster_after(self, cluster);
    fsinfo_store(self);
}

static void fsinfo_record_free(struct fat32 *self, uint32_t count, uint32_t first_cluster) {
    if (!self || !self->fsinfo_valid || !count) {
        return;
    }

    if (self->fsinfo_free_count != FSINFO_UNKNOWN) {
        self->fsinfo_free_count = min(self->info.total_clusters, self->fsinfo_free_count + count);
    }
    if (valid_cluster(self, first_cluster)) {
        self->fsinfo_next_free = first_cluster;
    }
    fsinfo_store(self);
}

static uint32_t allocate_cluster(struct fat32 *self) {
    uint32_t end = self->info.total_clusters + 2u;
    uint32_t first = valid_cluster(self, self->fsinfo_next_free) ? self->fsinfo_next_free : 2u;

    for (unsigned pass = 0; pass < 2; pass++) {
        uint32_t start = pass ? 2u : first;
        uint32_t stop = pass ? first : end;

        for (uint32_t cluster = start; cluster < stop; cluster++) {
            if (next_cluster(self, cluster) == 0) {
                if (!set_fat_entry(self, cluster, 0x0FFFFFFFu)) {
                    return 0;
                }
                if (!clear_cluster(self, cluster)) {
                    (void)set_fat_entry(self, cluster, 0);
                    return 0;
                }
                fsinfo_record_alloc(self, cluster);
                return cluster;
            }
        }
    }
    return 0;
}

static int free_chain(struct fat32 *self, uint32_t start_cluster);

static uint32_t append_directory_cluster(struct fat32 *self, uint32_t dir_cluster) {
    uint32_t cluster = dir_cluster;
    uint32_t guard = 0;

    if (!valid_cluster(self, dir_cluster)) {
        return 0;
    }

    while (valid_cluster(self, cluster) && guard++ < self->info.total_clusters + 2u) {
        uint32_t next = next_cluster(self, cluster);

        if (eoc(next)) {
            uint32_t new_cluster = allocate_cluster(self);
            if (!new_cluster) {
                return 0;
            }
            if (!set_fat_entry(self, cluster, new_cluster)) {
                (void)free_chain(self, new_cluster);
                return 0;
            }
            return new_cluster;
        }
        if (!valid_cluster(self, next)) {
            return 0;
        }
        cluster = next;
    }

    return 0;
}

static int free_chain(struct fat32 *self, uint32_t start_cluster) {
    uint32_t cluster = start_cluster;
    uint32_t guard = 0;
    uint32_t freed = 0;

    if (!start_cluster) {
        return 1;
    }
    if (!valid_cluster(self, start_cluster)) {
        return 0;
    }

    while (valid_cluster(self, cluster) && guard++ < self->info.total_clusters + 2u) {
        uint32_t next = next_cluster(self, cluster);
        if (!set_fat_entry(self, cluster, 0)) {
            return 0;
        }
        freed++;
        if (eoc(next)) {
            fsinfo_record_free(self, freed, start_cluster);
            return 1;
        }
        if (!valid_cluster(self, next)) {
            return 0;
        }
        cluster = next;
    }

    return 0;
}

static int valid_datetime(const struct datetime *time) {
    return time &&
           time->year >= 1980u && time->year <= 2107u &&
           time->month >= 1u && time->month <= 12u &&
           time->day >= 1u && time->day <= 31u &&
           time->hour <= 23u && time->minute <= 59u && time->second <= 59u;
}

static struct fat_timestamp timestamp_now(void) {
    struct datetime now;
    struct fat_timestamp stamp;
    uint16_t year;

    rtc(&now);
    if (!valid_datetime(&now)) {
        now.year = 1980;
        now.month = 1;
        now.day = 1;
        now.hour = 0;
        now.minute = 0;
        now.second = 0;
    }

    year = (uint16_t)(now.year - 1980u);
    stamp.date = (uint16_t)((year << 9) | ((uint16_t)now.month << 5) | now.day);
    stamp.time = (uint16_t)(((uint16_t)now.hour << 11) | ((uint16_t)now.minute << 5) | (now.second / 2u));
    stamp.tenth = 0;
    return stamp;
}

static void set_create_timestamp(struct dirent32 *entry, struct fat_timestamp stamp) {
    entry->create_tenth = stamp.tenth;
    put16(&entry->create_time, stamp.time);
    put16(&entry->create_date, stamp.date);
    put16(&entry->access_date, stamp.date);
    put16(&entry->write_time, stamp.time);
    put16(&entry->write_date, stamp.date);
}

static void set_modify_timestamp(struct dirent32 *entry, struct fat_timestamp stamp) {
    put16(&entry->access_date, stamp.date);
    put16(&entry->write_time, stamp.time);
    put16(&entry->write_date, stamp.date);
}

static int update_entry(struct fat32 *self, const struct entry_location *loc, uint32_t cluster, uint32_t size) {
    uint8_t sector[FAT32_SECTOR_SIZE];
    struct dirent32 *entry;
    struct fat_timestamp stamp;

    if (!self || !loc || !loc->found || !read_sector(self, loc->sector, sector)) {
        return 0;
    }

    entry = (struct dirent32 *)(sector + loc->offset);
    stamp = timestamp_now();
    put16(&entry->cluster_high, (uint16_t)(cluster >> 16));
    put16(&entry->cluster_low, (uint16_t)(cluster & 0xFFFFu));
    put32(&entry->size, size);
    set_modify_timestamp(entry, stamp);
    return write_sector(self, loc->sector, sector);
}

static int delete_entry(struct fat32 *self, const struct entry_location *loc) {
    uint8_t sector[FAT32_SECTOR_SIZE];

    if (!self || !loc || !loc->found || !read_sector(self, loc->sector, sector)) {
        return 0;
    }

    sector[loc->offset] = 0xE5u;
    return write_sector(self, loc->sector, sector);
}

static int delete_entry_chain(struct fat32 *self, const struct entry_location *loc) {
    uint8_t sector[FAT32_SECTOR_SIZE];

    if (!delete_entry(self, loc)) {
        return 0;
    }

    for (uint8_t i = 0; i < loc->lfn_count; i++) {
        if (!read_sector(self, loc->lfn_sector[i], sector)) {
            return 0;
        }
        sector[loc->lfn_offset[i]] = 0xE5u;
        if (!write_sector(self, loc->lfn_sector[i], sector)) {
            return 0;
        }
    }
    return 1;
}

static void init_sfn_entry(struct dirent32 *entry, const char name[11], uint8_t attr, uint32_t cluster, uint32_t size) {
    struct fat_timestamp stamp = timestamp_now();

    memset(entry, 0, sizeof(*entry));
    memcpy(entry->name, name, 11);
    entry->attr = attr;
    put16(&entry->cluster_high, (uint16_t)(cluster >> 16));
    put16(&entry->cluster_low, (uint16_t)(cluster & 0xFFFFu));
    put32(&entry->size, size);
    set_create_timestamp(entry, stamp);
}

static int init_directory_cluster(struct fat32 *self, uint32_t cluster, uint32_t parent_cluster) {
    uint8_t sector[FAT32_SECTOR_SIZE];
    struct dirent32 *dot;
    struct dirent32 *dotdot;

    if (!valid_cluster(self, cluster)) {
        return 0;
    }

    memset(sector, 0, sizeof(sector));
    dot = (struct dirent32 *)sector;
    dotdot = (struct dirent32 *)(sector + sizeof(struct dirent32));
    init_sfn_entry(dot, ".          ", ATTR_DIR, cluster, 0);
    init_sfn_entry(dotdot, "..         ", ATTR_DIR, parent_cluster, 0);

    if (!write_sector(self, cluster_sector(self, cluster), sector)) {
        return 0;
    }

    memset(sector, 0, sizeof(sector));
    for (uint32_t s = 1; s < self->info.sectors_per_cluster; s++) {
        if (!write_sector(self, cluster_sector(self, cluster) + s, sector)) {
            return 0;
        }
    }
    return 1;
}

static int make_sfn(const char *name, char out[11]) {
    const char *dot;
    size_t base_len;
    size_t ext_len = 0;

    if (!name || !*name || dot_entry(name)) {
        return 0;
    }

    memset(out, ' ', 11);
    dot = strchr(name, '.');
    base_len = dot ? (size_t)(dot - name) : strlen(name);
    if (!base_len || base_len > 8u) {
        return 0;
    }
    if (dot) {
        ext_len = strlen(dot + 1);
        if (!ext_len || ext_len > 3u || strchr(dot + 1, '.')) {
            return 0;
        }
    }

    for (size_t i = 0; i < base_len; i++) {
        char ch = name[i];
        if (!(ch >= 'A' && ch <= 'Z') && !(ch >= 'a' && ch <= 'z') && !(ch >= '0' && ch <= '9') && ch != '_' && ch != '-' && ch != '~') {
            return 0;
        }
        out[i] = upper(ch);
    }
    for (size_t i = 0; i < ext_len; i++) {
        char ch = dot[1 + i];
        if (!(ch >= 'A' && ch <= 'Z') && !(ch >= 'a' && ch <= 'z') && !(ch >= '0' && ch <= '9') && ch != '_' && ch != '-' && ch != '~') {
            return 0;
        }
        out[8 + i] = upper(ch);
    }
    return 1;
}

static int sfn_to_display(const char sfn[11], char *out, size_t size) {
    struct dirent32 entry;
    memset(&entry, 0, sizeof(entry));
    memcpy(entry.name, sfn, 11);
    return make_name(&entry, out, size);
}

static int legal_sfn_char(char ch) {
    return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_' || ch == '-' || ch == '~';
}

static size_t sanitize_component(const char *text, size_t max, char *out, size_t out_size) {
    size_t used = 0;
    while (text && *text && *text != '.' && used + 1u < out_size && used < max) {
        if (legal_sfn_char(*text)) {
            out[used++] = upper(*text);
        }
        text++;
    }
    out[used] = '\0';
    return used;
}

static int locate_in_directory(struct fat32 *self, uint32_t start_cluster, const char *wanted, struct entry_location *loc);
static int find_free_entry(struct fat32 *self, uint32_t dir_cluster, struct entry_location *loc);
static int find_free_run(struct fat32 *self, uint32_t dir_cluster, uint8_t needed, struct entry_location *loc);

static size_t decimal_digits(uint32_t value) {
    size_t digits = 1;

    while (value >= 10u) {
        value /= 10u;
        digits++;
    }
    return digits;
}

static void put_decimal_ascii(char *out, size_t digits, uint32_t value) {
    for (size_t i = 0; i < digits; i++) {
        out[digits - 1u - i] = (char)('0' + (value % 10u));
        value /= 10u;
    }
}

static int make_unique_sfn(struct fat32 *self, uint32_t dir_cluster, const char *long_name, char out[11]) {
    const char *dot = strrchr(long_name, '.');
    char base[9];
    char ext[4];

    memset(base, 0, sizeof(base));
    memset(ext, 0, sizeof(ext));
    sanitize_component(long_name, 8, base, sizeof(base));
    if (!base[0]) {
        strcpy(base, "FILE");
    }
    if (dot) {
        sanitize_component(dot + 1, 3, ext, sizeof(ext));
    }

    for (uint32_t n = 1; n <= 9999u; n++) {
        char display[FAT32_NAME];
        struct entry_location existing;
        size_t digits = decimal_digits(n);
        size_t prefix;

        if (digits + 1u > 8u) {
            return 0;
        }

        prefix = min(strlen(base), 8u - digits - 1u);
        memset(out, ' ', 11);
        for (size_t i = 0; i < prefix; i++) {
            out[i] = base[i];
        }
        out[prefix] = '~';
        put_decimal_ascii(out + prefix + 1u, digits, n);
        for (size_t i = 0; ext[i] && i < 3; i++) {
            out[8 + i] = ext[i];
        }
        if (!sfn_to_display(out, display, sizeof(display))) {
            return 0;
        }
        if (!locate_in_directory(self, dir_cluster, display, &existing)) {
            return 0;
        }
        if (!existing.found) {
            return 1;
        }
    }
    return 0;
}

static uint8_t lfn_entry_count(size_t units) {
    return (uint8_t)((units + 12u) / 13u);
}

static void write_lfn_entry(uint8_t *entry, uint8_t sequence, const uint16_t *units, size_t unit_count, uint8_t checksum, int last) {
    static const uint8_t offsets[13] = { 1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30 };
    size_t base = (size_t)(sequence - 1u) * 13u;
    memset(entry, 0xFF, 32);
    entry[0] = sequence | (last ? 0x40u : 0u);
    entry[11] = ATTR_LFN;
    entry[12] = 0;
    entry[13] = checksum;
    entry[26] = 0;
    entry[27] = 0;
    for (unsigned i = 0; i < 13; i++) {
        uint16_t value = 0xFFFFu;
        if (base + i < unit_count) {
            value = units[base + i];
        } else if (base + i == unit_count) {
            value = 0;
        }
        put16(entry + offsets[i], value);
    }
}

static int prepare_create_name(struct fat32 *self, uint32_t dir_cluster, const char *leaf, char sfn[11], uint16_t units[FAT32_LFN], size_t *unit_count, uint8_t *lfn_count, int *need_lfn) {
    if (!self || !leaf || !sfn || !units || !unit_count || !lfn_count || !need_lfn) {
        return 0;
    }

    *unit_count = 0;
    *lfn_count = 0;
    *need_lfn = 0;

    if (make_sfn(leaf, sfn)) {
        return 1;
    }

    *need_lfn = 1;
    *unit_count = utf8_to_utf16_units(leaf, units, FAT32_LFN);
    *lfn_count = lfn_entry_count(*unit_count);
    if (!*unit_count || !*lfn_count || *lfn_count >= FAT32_LFN_SLOTS) {
        return 0;
    }

    return make_unique_sfn(self, dir_cluster, leaf, sfn);
}

static struct dirent32 *prepare_new_entry_slot(struct fat32 *self, uint32_t dir_cluster, const char sfn[11], const uint16_t *units, size_t unit_count, uint8_t lfn_count, int need_lfn, struct entry_location *free_slot, uint8_t sector[FAT32_SECTOR_SIZE]) {
    if (!self || !sfn || !free_slot || !sector) {
        return nil;
    }

    if (need_lfn) {
        uint8_t checksum = sfn_checksum(sfn);

        if (!find_free_run(self, dir_cluster, (uint8_t)(lfn_count + 1u), free_slot) || !free_slot->found || !read_sector(self, free_slot->sector, sector)) {
            return nil;
        }

        for (uint8_t i = 0; i < lfn_count; i++) {
            uint8_t sequence = (uint8_t)(lfn_count - i);
            write_lfn_entry(sector + free_slot->offset + (uint32_t)i * sizeof(struct dirent32), sequence, units, unit_count, checksum, sequence == lfn_count);
        }
        return (struct dirent32 *)(sector + free_slot->offset + (uint32_t)lfn_count * sizeof(struct dirent32));
    }

    if (!find_free_entry(self, dir_cluster, free_slot) || !free_slot->found || !read_sector(self, free_slot->sector, sector)) {
        return nil;
    }
    return (struct dirent32 *)(sector + free_slot->offset);
}

static int split_parent(const char *path, char *parent, size_t parent_size, char *leaf, size_t leaf_size) {
    const char *slash;
    size_t parent_len;

    if (!path || !parent || !leaf || !parent_size || !leaf_size) {
        return 0;
    }

    while (*path == '/') {
        path++;
    }
    if (!*path) {
        return 0;
    }

    slash = strrchr(path, '/');
    if (!slash) {
        strncpy(parent, "/", parent_size);
        strncpy(leaf, path, leaf_size);
        return leaf[0] != '\0';
    }

    parent_len = (size_t)(slash - path);
    if (parent_len + 2u > parent_size) {
        return 0;
    }
    parent[0] = '/';
    memcpy(parent + 1, path, parent_len);
    parent[parent_len + 1u] = '\0';
    strncpy(leaf, slash + 1, leaf_size);
    return leaf[0] != '\0';
}

static int locate_in_directory(struct fat32 *self, uint32_t start_cluster, const char *wanted, struct entry_location *loc) {
    uint8_t sector[FAT32_SECTOR_SIZE];
    uint32_t cluster = start_cluster;
    uint32_t guard = 0;
    uint16_t lfn_units[FAT32_LFN];
    char lfn[FAT32_LFN];
    uint32_t lfn_sector[FAT32_LFN_SLOTS];
    uint32_t lfn_offset[FAT32_LFN_SLOTS];
    uint8_t lfn_sum = 0;
    uint8_t lfn_count = 0;
    int lfn_valid = 0;

    memset(loc, 0, sizeof(*loc));
    lfn_units_reset(lfn_units);
    lfn[0] = '\0';

    while (valid_cluster(self, cluster) && guard++ < self->info.total_clusters + 2u) {
        for (uint32_t s = 0; s < self->info.sectors_per_cluster; s++) {
            uint32_t sector_no = cluster_sector(self, cluster) + s;
            if (!read_sector(self, sector_no, sector)) {
                return 0;
            }

            for (uint32_t off = 0; off < self->info.bytes_per_sector; off += sizeof(struct dirent32)) {
                const struct dirent32 *entry = (const struct dirent32 *)(sector + off);
                const uint8_t *raw = sector + off;
                char short_name[FAT32_NAME];
                const char *display_name;

                if ((uint8_t)entry->name[0] == 0x00u) {
                    return 1;
                }
                if ((entry->attr & ATTR_LFN) == ATTR_LFN) {
                    if (raw[0] & 0x40u) {
                        lfn_sum = raw[13];
                        lfn_count = 0;
                        lfn_valid = 1;
                    } else if (!lfn_valid || lfn_sum != raw[13]) {
                        lfn_units_reset(lfn_units);
                        lfn_count = 0;
                        lfn_valid = 0;
                        continue;
                    }
                    if (lfn_count < FAT32_LFN_SLOTS) {
                        lfn_sector[lfn_count] = sector_no;
                        lfn_offset[lfn_count] = off;
                        lfn_count++;
                    } else {
                        lfn_units_reset(lfn_units);
                        lfn_count = 0;
                        lfn_valid = 0;
                        continue;
                    }
                    lfn_collect(raw, lfn_units, FAT32_LFN);
                    continue;
                }
                if (!usable_entry(entry) || !make_name(entry, short_name, sizeof(short_name))) {
                    lfn_units_reset(lfn_units);
                    lfn_count = 0;
                    lfn_valid = 0;
                    continue;
                }

                lfn_valid = lfn_valid && lfn_sum == sfn_checksum(entry->name) && lfn_to_utf8(lfn_units, lfn, sizeof(lfn));
                display_name = lfn_valid ? lfn : short_name;
                if (same_name(display_name, wanted) || same_name(short_name, wanted)) {
                    loc->found = 1;
                    loc->sector = sector_no;
                    loc->offset = off;
                    loc->dir_cluster = start_cluster;
                    loc->type = entry_type(entry);
                    loc->cluster = entry_cluster(entry);
                    loc->size = le32(&entry->size);
                    loc->has_lfn = lfn_valid;
                    loc->lfn_count = lfn_valid ? lfn_count : 0;
                    for (uint8_t i = 0; i < loc->lfn_count; i++) {
                        loc->lfn_sector[i] = lfn_sector[i];
                        loc->lfn_offset[i] = lfn_offset[i];
                    }
                    return 1;
                }
                lfn_units_reset(lfn_units);
                lfn_count = 0;
                lfn_valid = 0;
            }
        }
        cluster = next_cluster(self, cluster);
        if (eoc(cluster)) {
            return 1;
        }
    }
    return 0;
}

static int locate_entry(struct fat32 *self, const char *path, struct entry_location *loc) {
    char parent_path[FAT32_LFN];
    char leaf[FAT32_NAME];
    struct found_entry parent;

    if (!split_parent(path, parent_path, sizeof(parent_path), leaf, sizeof(leaf)) || !lookup(self, parent_path, &parent) || parent.type != FAT32_DIR) {
        return 0;
    }
    return locate_in_directory(self, parent.cluster, leaf, loc);
}

static void touch_entry_timestamp(struct fat32 *self, const struct entry_location *loc, struct fat_timestamp stamp) {
    uint8_t sector[FAT32_SECTOR_SIZE];
    struct dirent32 *entry;

    if (!self || !loc || !loc->found || !read_sector(self, loc->sector, sector)) {
        return;
    }

    entry = (struct dirent32 *)(sector + loc->offset);
    set_modify_timestamp(entry, stamp);
    (void)write_sector(self, loc->sector, sector);
}

static void touch_dot_timestamp(struct fat32 *self, uint32_t dir_cluster, struct fat_timestamp stamp) {
    uint8_t sector[FAT32_SECTOR_SIZE];
    struct dirent32 *entry;

    if (!valid_cluster(self, dir_cluster) || !read_sector(self, cluster_sector(self, dir_cluster), sector)) {
        return;
    }

    entry = (struct dirent32 *)sector;
    if (memcmp(entry->name, ".          ", 11) != 0 || !(entry->attr & ATTR_DIR)) {
        return;
    }

    set_modify_timestamp(entry, stamp);
    (void)write_sector(self, cluster_sector(self, dir_cluster), sector);
}

static void touch_directory_metadata(struct fat32 *self, const char *path, uint32_t cluster) {
    struct fat_timestamp stamp;

    if (!self || !self->mounted) {
        return;
    }

    stamp = timestamp_now();
    touch_dot_timestamp(self, cluster, stamp);

    if (path && strcmp(path, "/") != 0) {
        struct entry_location loc;
        if (locate_entry(self, path, &loc) && loc.found && loc.type == FAT32_DIR) {
            touch_entry_timestamp(self, &loc, stamp);
        }
    }
}

static int find_free_entry(struct fat32 *self, uint32_t dir_cluster, struct entry_location *loc) {
    uint8_t sector[FAT32_SECTOR_SIZE];
    uint32_t cluster = dir_cluster;
    uint32_t guard = 0;

    memset(loc, 0, sizeof(*loc));
    while (valid_cluster(self, cluster) && guard++ < self->info.total_clusters + 2u) {
        for (uint32_t s = 0; s < self->info.sectors_per_cluster; s++) {
            uint32_t sector_no = cluster_sector(self, cluster) + s;
            if (!read_sector(self, sector_no, sector)) {
                return 0;
            }
            for (uint32_t off = 0; off < self->info.bytes_per_sector; off += sizeof(struct dirent32)) {
                uint8_t first = sector[off];
                if (first == 0x00u || first == 0xE5u) {
                    loc->found = 1;
                    loc->sector = sector_no;
                    loc->offset = off;
                    loc->dir_cluster = dir_cluster;
                    return 1;
                }
            }
        }
        cluster = next_cluster(self, cluster);
        if (eoc(cluster)) {
            uint32_t new_cluster = append_directory_cluster(self, dir_cluster);
            if (!new_cluster) {
                return 0;
            }
            loc->found = 1;
            loc->sector = cluster_sector(self, new_cluster);
            loc->offset = 0;
            loc->dir_cluster = dir_cluster;
            return 1;
        }
    }
    return 0;
}

static int find_free_run(struct fat32 *self, uint32_t dir_cluster, uint8_t needed, struct entry_location *loc) {
    uint8_t sector[FAT32_SECTOR_SIZE];
    uint32_t cluster = dir_cluster;
    uint32_t guard = 0;

    memset(loc, 0, sizeof(*loc));
    if (!needed || needed > self->info.bytes_per_sector / sizeof(struct dirent32)) {
        return 0;
    }

    while (valid_cluster(self, cluster) && guard++ < self->info.total_clusters + 2u) {
        for (uint32_t s = 0; s < self->info.sectors_per_cluster; s++) {
            uint32_t sector_no = cluster_sector(self, cluster) + s;
            uint8_t run = 0;
            int run_has_end_marker = 0;
            uint32_t run_start = 0;
            if (!read_sector(self, sector_no, sector)) {
                return 0;
            }
            for (uint32_t off = 0; off < self->info.bytes_per_sector; off += sizeof(struct dirent32)) {
                uint8_t first = sector[off];
                if (first == 0x00u || first == 0xE5u) {
                    if (!run) {
                        run_start = off;
                        run_has_end_marker = first == 0x00u;
                    } else if (first == 0x00u) {
                        run_has_end_marker = 1;
                    }
                    run++;
                    if (run >= needed) {
                        loc->found = 1;
                        loc->sector = sector_no;
                        loc->offset = run_start;
                        loc->dir_cluster = dir_cluster;
                        return 1;
                    }
                } else {
                    run = 0;
                    run_has_end_marker = 0;
                }
            }

            /*
             * 0x00 means "no more entries in this directory".  If that marker
             * sits in a tail too small for the whole LFN run, entries written in
             * a later sector/cluster would be unreachable because readers stop
             * at the marker.  Convert only that free tail to deleted entries so
             * normal scans can continue to the next sector or appended cluster.
             */
            if (run_has_end_marker) {
                for (uint32_t off = run_start; off < self->info.bytes_per_sector; off += sizeof(struct dirent32)) {
                    sector[off] = 0xE5u;
                }
                if (!write_sector(self, sector_no, sector)) {
                    return 0;
                }
            }
        }
        cluster = next_cluster(self, cluster);
        if (eoc(cluster)) {
            uint32_t new_cluster = append_directory_cluster(self, dir_cluster);
            if (!new_cluster) {
                return 0;
            }
            loc->found = 1;
            loc->sector = cluster_sector(self, new_cluster);
            loc->offset = 0;
            loc->dir_cluster = dir_cluster;
            return 1;
        }
    }
    return 0;
}

static int mkdir_impl(struct fat32 *self, const char *path) {
    char parent_path[FAT32_LFN];
    char leaf[FAT32_NAME];
    char sfn[11];
    struct found_entry parent;
    struct entry_location existing;
    struct entry_location free_slot;
    uint8_t sector[FAT32_SECTOR_SIZE];
    struct dirent32 *entry;
    uint16_t units[FAT32_LFN];
    size_t unit_count;
    uint8_t lfn_count;
    int need_lfn;
    uint32_t cluster;

    if (!self || !self->mounted || !split_parent(path, parent_path, sizeof(parent_path), leaf, sizeof(leaf))) {
        return 0;
    }
    if (!lookup(self, parent_path, &parent) || parent.type != FAT32_DIR) {
        return 0;
    }
    if (locate_in_directory(self, parent.cluster, leaf, &existing) && existing.found) {
        return existing.type == FAT32_DIR;
    }
    if (!prepare_create_name(self, parent.cluster, leaf, sfn, units, &unit_count, &lfn_count, &need_lfn)) {
        return 0;
    }
    entry = prepare_new_entry_slot(self, parent.cluster, sfn, units, unit_count, lfn_count, need_lfn, &free_slot, sector);
    if (!entry) {
        return 0;
    }

    cluster = allocate_cluster(self);
    if (!cluster || !init_directory_cluster(self, cluster, parent.cluster)) {
        if (cluster) {
            (void)free_chain(self, cluster);
        }
        return 0;
    }

    init_sfn_entry(entry, sfn, ATTR_DIR, cluster, 0);
    if (!write_sector(self, free_slot.sector, sector)) {
        (void)free_chain(self, cluster);
        return 0;
    }
    touch_directory_metadata(self, parent_path, parent.cluster);
    self->cache_path[0] = '\0';
    return 1;
}

static int create_impl(struct fat32 *self, const char *path) {
    char parent_path[FAT32_LFN];
    char leaf[FAT32_NAME];
    char sfn[11];
    struct found_entry parent;
    struct entry_location existing;
    struct entry_location free_slot;
    uint8_t sector[FAT32_SECTOR_SIZE];
    struct dirent32 *entry;
    uint16_t units[FAT32_LFN];
    size_t unit_count;
    uint8_t lfn_count;
    int need_lfn;

    if (!self || !self->mounted || !split_parent(path, parent_path, sizeof(parent_path), leaf, sizeof(leaf))) {
        return 0;
    }
    if (!lookup(self, parent_path, &parent) || parent.type != FAT32_DIR) {
        return 0;
    }
    if (locate_in_directory(self, parent.cluster, leaf, &existing) && existing.found) {
        return existing.type == FAT32_FILE;
    }
    if (!prepare_create_name(self, parent.cluster, leaf, sfn, units, &unit_count, &lfn_count, &need_lfn)) {
        return 0;
    }

    entry = prepare_new_entry_slot(self, parent.cluster, sfn, units, unit_count, lfn_count, need_lfn, &free_slot, sector);
    if (!entry) {
        return 0;
    }

    init_sfn_entry(entry, sfn, ATTR_ARCHIVE, 0, 0);
    if (!write_sector(self, free_slot.sector, sector)) {
        return 0;
    }
    touch_directory_metadata(self, parent_path, parent.cluster);
    self->cache_path[0] = '\0';
    return 1;
}

static int truncate_impl(struct fat32 *self, const char *path) {
    struct entry_location loc;
    uint32_t old_cluster;

    if (!self || !self->mounted || !locate_entry(self, path, &loc) || !loc.found || loc.type != FAT32_FILE) {
        return 0;
    }

    self->cache_path[0] = '\0';
    old_cluster = loc.cluster;

    /*
     * Clear the directory entry before freeing the old cluster chain.  If a
     * later write fails, a leaked cluster chain is safer than a live directory
     * entry still pointing at clusters that have already been marked free.
     */
    if (!update_entry(self, &loc, 0, 0)) {
        return 0;
    }
    if (old_cluster && !free_chain(self, old_cluster)) {
        return 0;
    }
    return 1;
}

static int unlink_impl(struct fat32 *self, const char *path) {
    char parent_path[FAT32_LFN];
    char leaf[FAT32_NAME];
    struct entry_location loc;

    if (!self || !self->mounted || !split_parent(path, parent_path, sizeof(parent_path), leaf, sizeof(leaf))) {
        return 0;
    }
    if (!locate_entry(self, path, &loc) || !loc.found || loc.type != FAT32_FILE) {
        return 0;
    }
    /* Delete the directory entry before freeing data clusters.  On an
     * unexpected later failure this may leak clusters, but it avoids leaving a
     * visible file entry that points at clusters already returned to the free
     * pool.
     */
    if (!delete_entry_chain(self, &loc)) {
        return 0;
    }
    if (loc.cluster && !free_chain(self, loc.cluster)) {
        return 0;
    }
    {
        struct found_entry parent;
        if (lookup(self, parent_path, &parent) && parent.type == FAT32_DIR) {
            touch_directory_metadata(self, parent_path, parent.cluster);
        }
    }
    self->cache_path[0] = '\0';
    return 1;
}

static int directory_empty(struct fat32 *self, uint32_t start_cluster) {
    uint8_t sector[FAT32_SECTOR_SIZE];
    uint32_t cluster = start_cluster;
    uint32_t guard = 0;

    if (!valid_cluster(self, cluster)) {
        return 0;
    }

    while (valid_cluster(self, cluster) && guard++ < self->info.total_clusters + 2u) {
        for (uint32_t s = 0; s < self->info.sectors_per_cluster; s++) {
            if (!read_sector(self, cluster_sector(self, cluster) + s, sector)) {
                return 0;
            }
            for (uint32_t off = 0; off < self->info.bytes_per_sector; off += sizeof(struct dirent32)) {
                const struct dirent32 *entry = (const struct dirent32 *)(sector + off);
                char name[FAT32_NAME];
                uint8_t first = (uint8_t)entry->name[0];

                if (first == 0x00u) {
                    return 1;
                }
                if (first == 0xE5u || (entry->attr & ATTR_LFN) == ATTR_LFN || (entry->attr & ATTR_VOLUME)) {
                    continue;
                }
                if (!make_name(entry, name, sizeof(name))) {
                    return 0;
                }
                if (!dot_entry(name)) {
                    return 0;
                }
            }
        }
        cluster = next_cluster(self, cluster);
        if (eoc(cluster)) {
            return 1;
        }
    }
    return 0;
}

static int rmdir_impl(struct fat32 *self, const char *path) {
    char parent_path[FAT32_LFN];
    char leaf[FAT32_NAME];
    struct entry_location loc;

    if (!self || !self->mounted || !split_parent(path, parent_path, sizeof(parent_path), leaf, sizeof(leaf))) {
        return 0;
    }
    if (!locate_entry(self, path, &loc) || !loc.found || loc.type != FAT32_DIR || !loc.cluster) {
        return 0;
    }
    if (!directory_empty(self, loc.cluster)) {
        return 0;
    }
    /* Same ordering as unlink: hide the empty directory first, then release its
     * cluster chain.  A leak is preferable to a dangling visible directory.
     */
    if (!delete_entry_chain(self, &loc)) {
        return 0;
    }
    if (!free_chain(self, loc.cluster)) {
        return 0;
    }
    {
        struct found_entry parent;
        if (lookup(self, parent_path, &parent) && parent.type == FAT32_DIR) {
            touch_directory_metadata(self, parent_path, parent.cluster);
        }
    }
    self->cache_path[0] = '\0';
    return 1;
}

static uint32_t cluster_for_write(struct fat32 *self, struct entry_location *loc, size_t offset) {
    uint32_t cluster = loc->cluster;
    uint32_t cluster_bytes = (uint32_t)self->info.sectors_per_cluster * self->info.bytes_per_sector;
    size_t remaining = offset;

    if (!cluster) {
        cluster = allocate_cluster(self);
        if (!cluster) {
            return 0;
        }
        if (!update_entry(self, loc, cluster, loc->size)) {
            (void)free_chain(self, cluster);
            return 0;
        }
        loc->cluster = cluster;
    }

    while (remaining >= cluster_bytes) {
        uint32_t next = next_cluster(self, cluster);
        remaining -= cluster_bytes;
        if (eoc(next)) {
            next = allocate_cluster(self);
            if (!next) {
                return 0;
            }
            if (!set_fat_entry(self, cluster, next)) {
                (void)free_chain(self, next);
                return 0;
            }
        }
        cluster = next;
    }
    return cluster;
}

static int write_impl(struct fat32 *self, const char *path, size_t offset, const char *buffer, size_t size) {
    struct entry_location loc;
    uint8_t sector[FAT32_SECTOR_SIZE];
    size_t done = 0;
    size_t inner;
    uint32_t cluster;

    if (!self || !self->mounted || (!buffer && size) || !locate_entry(self, path, &loc) || !loc.found || loc.type != FAT32_FILE) {
        return -1;
    }
    if (offset > loc.size) {
        return -1;
    }

    while (done < size) {
        size_t absolute = offset + done;
        size_t cluster_offset;
        uint32_t sector_index;
        uint32_t sector_no;
        size_t count;

        cluster = cluster_for_write(self, &loc, absolute);
        if (!cluster) {
            return done ? (int)done : -1;
        }

        cluster_offset = absolute % ((size_t)self->info.sectors_per_cluster * self->info.bytes_per_sector);
        sector_index = (uint32_t)(cluster_offset / self->info.bytes_per_sector);
        inner = cluster_offset % self->info.bytes_per_sector;
        sector_no = cluster_sector(self, cluster) + sector_index;
        if (!read_sector(self, sector_no, sector)) {
            return done ? (int)done : -1;
        }
        count = min(size - done, self->info.bytes_per_sector - inner);
        memcpy(sector + inner, buffer + done, count);
        if (!write_sector(self, sector_no, sector)) {
            return done ? (int)done : -1;
        }
        done += count;
    }

    if (offset + done > loc.size) {
        loc.size = (uint32_t)(offset + done);
        if (!update_entry(self, &loc, loc.cluster, loc.size)) {
            return done ? (int)done : -1;
        }
    }
    self->cache_path[0] = '\0';
    return (int)done;
}

static void each_impl(struct fat32 *self, const char *path, fat32_iter iter, void *ctx) {
    struct found_entry dir;

    if (!self || !self->mounted || !iter || !lookup(self, path, &dir) || dir.type != FAT32_DIR) {
        return;
    }

    (void)scan_directory(self, dir.cluster, nil, nil, iter, ctx);
}

int fat32(struct fat32 *self, struct blockdevice *device) {
    uint8_t sector[FAT32_SECTOR_SIZE];

    if (!self || !device) {
        return 0;
    }

    memset(self, 0, sizeof(*self));
    self->stat = stat_impl;
    self->mkdir = mkdir_impl;
    self->rmdir = rmdir_impl;
    self->create = create_impl;
    self->truncate = truncate_impl;
    self->unlink = unlink_impl;
    self->read = read_impl;
    self->write = write_impl;
    self->each = each_impl;
    self->device = device;

    if (device->sector_size != FAT32_SECTOR_SIZE || device->read(device, 0, sector, 1) != 1) {
        return 0;
    }
    if (!fat32_parse_bpb(sector, sizeof(sector), &self->info) || !fat32_is_fat32(&self->info) || self->info.bytes_per_sector != FAT32_SECTOR_SIZE) {
        return 0;
    }

    fsinfo_load(self);
    self->mounted = 1;
    return 1;
}
