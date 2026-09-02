#include "kernel/fs/fat12.h"
#include "arch/x86/rtc.h"

#define FAT12_BLOCK 4096u
#define FAT12_NAME  64u

struct fat12_inode {
    uint8_t attr;
    uint16_t cluster;
    uint32_t size;
    uint32_t atime;
    uint32_t ctime;
    uint32_t mtime;
};
struct fat12_found { int found; struct fat12_inode inode; size_t entry_off; };

static uint16_t le16(const uint8_t *p) { return (uint16_t)p[0] | ((uint16_t)p[1] << 8); }
static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static void put16(uint8_t *p, uint16_t value) { p[0] = (uint8_t)value; p[1] = (uint8_t)(value >> 8); }

static int put_bytes(struct fat12 *self, size_t offset, const void *data, size_t size);

static uint32_t ftime(uint16_t date, uint16_t time) {
    uint32_t year = 1980u + (date >> 9);
    uint32_t month = (date >> 5) & 15u;
    uint32_t day = date & 31u;
    uint32_t days = 0;
    for (uint32_t y = 1970u; y < year; y++) days += (y % 4u == 0 && (y % 100u != 0 || y % 400u == 0)) ? 366u : 365u;
    for (uint32_t m = 1; m < month; m++) days += (m == 2u && (year % 4u == 0 && (year % 100u != 0 || year % 400u == 0))) ? 29u : (m == 2u ? 28u : (m == 4u || m == 6u || m == 9u || m == 11u ? 30u : 31u));
    days += day ? day - 1u : 0u;
    return days * 86400u + ((time >> 11) & 31u) * 3600u + ((time >> 5) & 63u) * 60u + (time & 31u) * 2u;
}
static void stamp(struct fat12 *self, size_t off) {
    struct datetime now;
    uint16_t date;
    uint16_t time;
    rtc(&now);
    if (now.year < 1980u || now.month < 1u || now.month > 12u || now.day < 1u || now.day > 31u) return;
    date = (uint16_t)(((now.year - 1980u) << 9) | ((uint16_t)now.month << 5) | now.day);
    time = (uint16_t)(((uint16_t)now.hour << 11) | ((uint16_t)now.minute << 5) | (now.second / 2u));
    put_bytes(self, off + 13u, &(uint8_t){0}, 1);
    put_bytes(self, off + 14u, &time, 2);
    put_bytes(self, off + 16u, &date, 2);
    put_bytes(self, off + 18u, &date, 2);
    put_bytes(self, off + 22u, &time, 2);
    put_bytes(self, off + 24u, &date, 2);
}
static int read_bytes(struct fat12 *self, size_t offset, void *buffer, size_t size) {
    return self && self->device && self->device->readbytes && self->device->readbytes(self->device, offset, buffer, size) == (int)size;
}
static uint32_t root_lba(struct fat12 *self) { return self->info.first_fat_sector + self->info.fat_count * self->info.sectors_per_fat; }
static uint32_t cluster_lba(struct fat12 *self, uint16_t cluster) { return self->info.first_data_sector + ((uint32_t)cluster - 2u) * self->info.sectors_per_cluster; }

static int fat_entry(struct fat12 *self, uint16_t cluster, uint16_t *out) {
    uint8_t bytes[2];
    size_t offset = (size_t)self->info.first_fat_sector * self->info.bytes_per_sector + cluster + cluster / 2u;
    if (!read_bytes(self, offset, bytes, 2)) return 0;
    uint16_t value = le16(bytes);
    *out = (cluster & 1u) ? value >> 4 : value & 0x0FFFu;
    return 1;
}
static int end_cluster(uint16_t cluster) { return cluster >= 0x0FF8u; }
static int valid_cluster(struct fat12 *self, uint16_t cluster) { return self && cluster >= 2u && cluster < self->info.total_clusters + 2u; }

static void short_name(const uint8_t *entry, char *out, size_t size) {
    size_t n = 0;
    for (unsigned i = 0; i < 8 && entry[i] != ' '; i++) if (n + 1 < size) out[n++] = entry[i];
    if (entry[8] != ' ' && n + 1 < size) {
        out[n++] = '.';
        for (unsigned i = 8; i < 11 && entry[i] != ' '; i++) if (n + 1 < size) out[n++] = entry[i];
    }
    out[n] = '\0';
}
static struct fat12_inode ent(const uint8_t *p) {
    struct fat12_inode f = { p[11], le16(p + 26), le32(p + 28), ftime(le16(p + 18), 0), ftime(le16(p + 16), le16(p + 14)), ftime(le16(p + 24), le16(p + 22)) };
    return f;
}

static int same_name(const char *a, const char *b) {
    while (*a && *b) { char x = *a++, y = *b++; if (x >= 'A' && x <= 'Z') x += 'a' - 'A'; if (y >= 'A' && y <= 'Z') y += 'a' - 'A'; if (x != y) return 0; }
    return *a == *b;
}

static int scan_root(struct fat12 *self, const char *wanted, struct fat12_found *found, fat12_iter iter, void *ctx) {
    uint8_t sector[512];
    uint32_t entries = self->info.root_dir_sectors * self->info.bytes_per_sector / 32u;
    for (uint32_t i = 0; i < entries; i++) {
        size_t offset = (size_t)root_lba(self) * self->info.bytes_per_sector + i * 32u;
        if (!read_bytes(self, offset, sector, 32)) return 0;
        if (sector[0] == 0) break;
        if (sector[0] == 0xE5 || sector[11] == 0x0F || (sector[11] & 0x08u)) continue;
        char name[FAT12_NAME]; short_name(sector, name, sizeof(name));
        struct fat12_inode inode = ent(sector);
        enum fat12_node_type type = (sector[11] & 0x10u) ? FAT12_DIR : FAT12_FILE;
        if (wanted && same_name(name, wanted)) { if (found) { found->found = 1; found->inode = inode; found->entry_off = offset; } return 1; }
        if (!wanted && iter) iter(ctx, name, type, inode.size);
    }
    return 1;
}
static int scan_dir(struct fat12 *self, const struct fat12_inode *dir, const char *wanted, struct fat12_found *found, fat12_iter iter, void *ctx) {
    uint8_t block[FAT12_BLOCK];
    uint16_t cluster = dir->cluster;
    uint32_t guard = 0;
    while (valid_cluster(self, cluster) && guard++ < self->info.total_clusters + 2u) {
        size_t bytes = (size_t)self->info.sectors_per_cluster * self->info.bytes_per_sector;
        if (bytes > sizeof(block) || !read_bytes(self, (size_t)cluster_lba(self, cluster) * self->info.bytes_per_sector, block, bytes)) return 0;
        for (size_t off = 0; off + 32 <= bytes; off += 32) {
            uint8_t *entry = block + off; if (entry[0] == 0) return 1; if (entry[0] == 0xE5 || entry[11] == 0x0F || (entry[11] & 0x08u)) continue;
            char name[FAT12_NAME]; short_name(entry, name, sizeof(name));
            struct fat12_inode inode = ent(entry);
            enum fat12_node_type type = (entry[11] & 0x10u) ? FAT12_DIR : FAT12_FILE;
            if (wanted && same_name(name, wanted)) { if (found) { found->found = 1; found->inode = inode; found->entry_off = (size_t)cluster_lba(self, cluster) * self->info.bytes_per_sector + off; } return 1; }
            if (!wanted && iter && strcmp(name, ".") != 0 && strcmp(name, "..") != 0) iter(ctx, name, type, inode.size);
        }
        uint16_t next; if (!fat_entry(self, cluster, &next) || end_cluster(next)) break; cluster = next;
    }
    return 1;
}
static const char *component(const char *path, char *out, size_t size) {
    size_t n = 0; while (path && *path == '/') path++; if (!path || !*path) return nil;
    while (*path && *path != '/') { if (n + 1 < size) out[n++] = *path; path++; } out[n] = 0; return path;
}
static int lookup(struct fat12 *self, const char *path, struct fat12_found *out) {
    char part[FAT12_NAME]; const char *cursor = path ? path : "/"; struct fat12_found current; struct fat12_inode dir = { 0x10u, 0, 0, 0, 0, 0 };
    memset(out, 0, sizeof(*out)); out->found = 1; out->inode = dir;
    while (*cursor == '/') {
        cursor++;
    }
    if (!*cursor) {
        return 1;
    }
    while ((cursor = component(cursor, part, sizeof(part))) != nil) {
        memset(&current, 0, sizeof(current));
        if (dir.cluster == 0) {
            if (!scan_root(self, part, &current, nil, nil)) return 0;
        } else if (!scan_dir(self, &dir, part, &current, nil, nil)) {
            return 0;
        }
        if (!current.found) {
            return 0;
        }
        *out = current;
        dir = current.inode;
        while (*cursor == '/') {
            cursor++;
        }
        if (!*cursor) {
            return 1;
        }
        if (!(dir.attr & 0x10u)) {
            return 0;
        }
    }
    return 0;
}
static int stat_impl(struct fat12 *self, const char *path, struct fat12_stat *out) { struct fat12_found f; if (!self || !self->mounted || !out || !lookup(self, path, &f)) return 0; out->type = (f.inode.attr & 0x10u) ? FAT12_DIR : FAT12_FILE; out->size = f.inode.size; out->atime = f.inode.atime; out->ctime = f.inode.ctime; out->mtime = f.inode.mtime; return 1; }
static int read_impl(struct fat12 *self, const char *path, size_t offset, char *buffer, size_t size) {
    struct fat12_found f; uint8_t block[FAT12_BLOCK]; if (!self || !buffer || !size || !lookup(self,path,&f) || (f.inode.attr & 0x10u)) return -1; if (offset >= f.inode.size) return 0; if (size > f.inode.size-offset) size=f.inode.size-offset;
    uint16_t cluster = f.inode.cluster;
    size_t cluster_bytes = (size_t)self->info.sectors_per_cluster * self->info.bytes_per_sector;
    size_t skip = offset / cluster_bytes;
    size_t within = offset % cluster_bytes;
    uint32_t guard = 0;
    while (skip-- && valid_cluster(self, cluster) && guard++ < self->info.total_clusters + 2u) {
        uint16_t next;
        if (!fat_entry(self, cluster, &next) || end_cluster(next)) return -1;
        cluster = next;
    }
    size_t done = 0;
    guard = 0;
    while (done < size && valid_cluster(self, cluster) && guard++ < self->info.total_clusters + 2u) {
        size_t bytes = cluster_bytes;
        if (!read_bytes(self, (size_t)cluster_lba(self, cluster) * self->info.bytes_per_sector, block, bytes)) return done ? (int)done : -1;
        size_t n = min(size - done, bytes - within);
        memcpy(buffer + done, block + within, n);
        done += n;
        within = 0;
        if (done < size) {
            uint16_t next;
            if (!fat_entry(self, cluster, &next) || end_cluster(next)) break;
            cluster = next;
        }
    }
    return (int)done;
}
static int put_bytes(struct fat12 *self, size_t offset, const void *data, size_t size) {
    uint8_t sector[512];
    uint8_t check[512];
    const uint8_t *src = data;
    while (size) {
        size_t within = offset % BLOCK_SECTOR;
        size_t count = min(size, BLOCK_SECTOR - within);
        size_t start = offset - within;
        if (!read_bytes(self, start, sector, sizeof(sector))) return 0;
        memcpy(sector + within, src, count);
        if (!self->device || self->device->readonly || self->device->write(self->device, (uint32_t)(start / BLOCK_SECTOR), sector, 1) != 1) return 0;
        /* A write is not considered complete until the device returns the
           modified bytes.  This catches stale-cache/timing failures before
           another FAT or directory sector depends on the result. */
        if (!read_bytes(self, start, check, sizeof(check)) || memcmp(check + within, src, count) != 0) return 0;
        offset += count;
        src += count;
        size -= count;
    }
    return 1;
}

static int fat_put(struct fat12 *self, uint16_t cluster, uint16_t value) {
    uint8_t old[2];
    uint8_t bytes[2];
    size_t entry = cluster + cluster / 2u;
    uint16_t keep;
    uint8_t copy;

    if (!self || self->device->readonly || !read_bytes(self, (size_t)self->info.first_fat_sector * self->info.bytes_per_sector + entry, old, 2)) return 0;
    keep = (cluster & 1u) ? (uint16_t)(le16(old) & 0x000Fu) : (uint16_t)(le16(old) & 0xF000u);
    value &= 0x0FFFu;
    value = cluster & 1u ? (uint16_t)((value << 4) | keep) : (uint16_t)(value | keep);
    put16(bytes, value);
    for (copy = 0; copy < self->info.fat_count; copy++) {
        size_t offset = (size_t)(self->info.first_fat_sector + copy * self->info.sectors_per_fat) * self->info.bytes_per_sector + entry;
        if (!put_bytes(self, offset, bytes, 2)) {
            while (copy) {
                copy--;
                offset = (size_t)(self->info.first_fat_sector + copy * self->info.sectors_per_fat) * self->info.bytes_per_sector + entry;
                put_bytes(self, offset, old, 2);
            }
            return 0;
        }
    }
    return 1;
}

static int fat_alloc(struct fat12 *self, uint16_t *out_cluster) {
    uint16_t value;
    if (!self || !out_cluster || self->device->readonly) return 0;
    for (uint16_t cluster = 2; cluster < self->info.total_clusters + 2u; cluster++) {
        if (!fat_entry(self, cluster, &value)) return 0;
        if (value == 0 && fat_put(self, cluster, 0xFFFu)) {
            *out_cluster = cluster;
            return 1;
        }
    }
    return 0;
}

static int fat_free(struct fat12 *self, uint16_t cluster) {
    return self && cluster >= 2u && valid_cluster(self, cluster) && fat_put(self, cluster, 0);
}

static int add_ent(struct fat12 *self, const struct fat12_inode *dir, uint16_t ino, const char *name, uint8_t attr) {
    uint8_t entry[32];
    size_t name_len = strlen(name);
    char short_name[11];
    const char *dot = strchr(name, '.');
    size_t base = dot ? (size_t)(dot - name) : name_len;
    size_t ext = dot ? strlen(dot + 1) : 0;
    if (name_len == 0 || base == 0 || base > 8u || ext > 3u) return 0;
    memset(short_name, ' ', sizeof(short_name));
    for (size_t i = 0; i < base; i++) short_name[i] = name[i];
    for (size_t i = 0; i < ext; i++) short_name[8 + i] = dot[1 + i];
    memset(entry, 0, sizeof(entry));
    memcpy(entry, short_name, 11);
    entry[11] = attr;
    put16(entry + 26, ino);
    if (!dir->cluster) {
        uint32_t count = self->info.root_dir_sectors * self->info.bytes_per_sector / 32u;
        for (uint32_t i = 0; i < count; i++) {
            size_t off = (size_t)root_lba(self) * self->info.bytes_per_sector + i * 32u;
            uint8_t first;
            if (!read_bytes(self, off, &first, 1)) return 0;
            if (first == 0 || first == 0xE5u) return put_bytes(self, off, entry, sizeof(entry));
        }
        return 0;
    }
    uint16_t cluster = dir->cluster;
    uint32_t guard = 0;
    while (valid_cluster(self, cluster) && guard++ < self->info.total_clusters + 2u) {
        size_t bytes = (size_t)self->info.sectors_per_cluster * self->info.bytes_per_sector;
        uint8_t block[FAT12_BLOCK];
        if (bytes > sizeof(block) || !read_bytes(self, (size_t)cluster_lba(self, cluster) * self->info.bytes_per_sector, block, bytes)) return 0;
        for (size_t off = 0; off + 32u <= bytes; off += 32u) {
            if (block[off] == 0 || block[off] == 0xE5u) return put_bytes(self, (size_t)cluster_lba(self, cluster) * self->info.bytes_per_sector + off, entry, sizeof(entry));
        }
        uint16_t next; if (!fat_entry(self, cluster, &next) || end_cluster(next)) break; cluster = next;
    }
    return 0;
}

static int mk(struct fat12 *self, const char *path) {
    const char *slash;
    char name[FAT12_NAME];
    char parent[FAT12_NAME];
    struct fat12_found par, old;
    uint16_t cluster;

    if (!self || self->device->readonly || !path || !*path) return 0;
    slash = strrchr(path, '/');
    if (!slash) return 0;
    strncpy(name, slash + 1, sizeof(name));
    if (slash == path) {
        strncpy(parent, "/", sizeof(parent));
    } else {
        size_t length = (size_t)(slash - path);
        if (length >= sizeof(parent)) return 0;
        memcpy(parent, path, length);
        parent[length] = 0;
    }
    memset(&old, 0, sizeof(old));
    if (!lookup(self, parent, &par) || !par.found || !(par.inode.attr & 0x10u)) return 0;
    if (lookup(self, path, &old)) return 0;
    if (!fat_alloc(self, &cluster)) return 0;
    if (!add_ent(self, &par.inode, cluster, name, 0x20u)) {
        fat_free(self, cluster);
        return 0;
    }
    struct fat12_found made;
    memset(&made, 0, sizeof(made));
    if (lookup(self, path, &made) && made.found) stamp(self, made.entry_off);
    if (par.inode.cluster) stamp(self, par.entry_off);
    return 1;
}

static int fre_chain(struct fat12 *self, uint16_t cluster) {
    uint32_t guard = 0;
    while (valid_cluster(self, cluster) && guard++ < self->info.total_clusters + 2u) {
        uint16_t next;
        if (!fat_entry(self, cluster, &next) || !fat_free(self, cluster)) return 0;
        if (end_cluster(next)) return 1;
        cluster = next;
    }
    return 0;
}

static int rm(struct fat12 *self, const char *path) {
    struct fat12_found f;
    struct fat12_found par;
    char parent[FAT12_NAME];
    const char *slash;
    uint8_t deleted = 0xE5u;
    if (!self || self->device->readonly || !path || !*path) return 0;
    if (!lookup(self, path, &f) || !f.found || (f.inode.attr & 0x10u)) return 0;
    slash = strrchr(path, '/');
    if (!slash || slash == path) strcpy(parent, "/");
    else {
        size_t length = (size_t)(slash - path);
        if (length >= sizeof(parent)) return 0;
        memcpy(parent, path, length);
        parent[length] = 0;
    }
    if (!lookup(self, parent, &par)) return 0;
    if (!put_bytes(self, f.entry_off, &deleted, 1) || !fre_chain(self, f.inode.cluster)) return 0;
    if (par.inode.cluster) stamp(self, par.entry_off);
    return 1;
}

static int md(struct fat12 *self, const char *path) {
    const char *slash;
    char name[FAT12_NAME];
    char parent[FAT12_NAME];
    struct fat12_found par, old;
    uint16_t block;
    size_t length;
    uint8_t data[512];

    if (!self || self->device->readonly || !path || !*path || !strcmp(path, "/")) return 0;
    slash = strrchr(path, '/');
    if (!slash) return 0;
    strncpy(name, slash + 1, sizeof(name));
    if (slash == path) strncpy(parent, "/", sizeof(parent));
    else { length = (size_t)(slash - path); if (length >= sizeof(parent)) return 0; memcpy(parent, path, length); parent[length] = 0; }
    memset(&old, 0, sizeof(old));
    if (!lookup(self, parent, &par) || !par.found || !(par.inode.attr & 0x10u)) return 0;
    if (lookup(self, path, &old)) return 0;
    if (!fat_alloc(self, &block)) return 0;
    memset(data, 0, sizeof(data));
    data[0] = '.'; data[11] = 0x10; put16(data + 26, block);
    data[32] = '.'; data[33] = '.'; data[43] = 0x10; put16(data + 58, par.inode.cluster);
    if (!put_bytes(self, (size_t)cluster_lba(self, block) * self->info.bytes_per_sector, data, sizeof(data))) {
        fat_free(self, block);
        return 0;
    }
    if (!add_ent(self, &par.inode, block, name, 0x10u)) {
        fat_free(self, block);
        return 0;
    }
    struct fat12_found made;
    memset(&made, 0, sizeof(made));
    if (lookup(self, path, &made) && made.found) stamp(self, made.entry_off);
    if (par.inode.cluster) stamp(self, par.entry_off);
    return 1;
}

struct cnt { unsigned n; };
static void cnt_ent(void *ctx, const char *name, enum fat12_node_type type, size_t size) { struct cnt *c = ctx; unused(name); unused(type); unused(size); c->n++; }
static int rd(struct fat12 *self, const char *path) {
    struct fat12_found f; struct cnt count = { 0 };
    if (!self || self->device->readonly || !path || !strcmp(path, "/") || !lookup(self, path, &f) || !(f.inode.attr & 0x10u)) return 0;
    self->each(self, path, cnt_ent, &count);
    if (count.n) return 0;
    uint8_t deleted = 0xE5u;
    if (!put_bytes(self, f.entry_off, &deleted, 1)) return 0;
    return fre_chain(self, f.inode.cluster);
}

static int tr(struct fat12 *self, const char *path) {
    struct fat12_found f;
    uint32_t size = 0;
    if (!self || self->device->readonly || !lookup(self, path, &f) || (f.inode.attr & 0x10u)) return 0;
    if (!put_bytes(self, f.entry_off + 28u, &size, sizeof(size))) return 0;
    stamp(self, f.entry_off);
    return 1;
}

static int wr(struct fat12 *self, const char *path, size_t offset, const char *buffer, size_t size) {
    struct fat12_found f;
    uint8_t sector[512];
    uint16_t newc[32];
    uint16_t link = 0;
    unsigned newn = 0;
    size_t done = 0;
    size_t cluster_bytes;
    uint32_t old_size;

    if (!self || !self->device || self->device->readonly || (!buffer && size) ||
        !lookup(self, path, &f) || (f.inode.attr & 0x10u) || offset > f.inode.size) {
        return -1;
    }
    old_size = f.inode.size;
    cluster_bytes = (size_t)self->info.sectors_per_cluster * self->info.bytes_per_sector;
    if (offset + size > old_size) {
        size_t need = (offset + size + cluster_bytes - 1u) / cluster_bytes;
        size_t have = 1;
        uint16_t last = f.inode.cluster;
        uint16_t next;

        while (have < need) {
            if (!fat_entry(self, last, &next)) goto fail;
            if (end_cluster(next)) {
                if (newn >= countof(newc) || !fat_alloc(self, &next)) goto fail;
                newc[newn++] = next;
                if (!fat_put(self, last, next)) goto fail;
                link = last;
            }
            last = next;
            have++;
        }
    }

    while (done < size) {
        size_t position = offset + done;
        size_t skip = position / cluster_bytes;
        size_t within = position % cluster_bytes;
        uint16_t cluster = f.inode.cluster;
        size_t count = min(size - done, self->info.bytes_per_sector - (within % self->info.bytes_per_sector));
        uint16_t next;

        while (skip--) {
            if (!fat_entry(self, cluster, &next) || end_cluster(next)) goto fail;
            cluster = next;
        }
        size_t disk_offset = (size_t)cluster_lba(self, cluster) * self->info.bytes_per_sector + within;
        size_t sector_offset = disk_offset % self->info.bytes_per_sector;
        size_t sector_start = disk_offset - sector_offset;
        if (!read_bytes(self, sector_start, sector, sizeof(sector))) goto fail;
        memcpy(sector + sector_offset, buffer + done, count);
        if (self->device->write(self->device, (uint32_t)(sector_start / BLOCK_SECTOR), sector, 1) != 1) goto fail;
        done += count;
    }

    if (offset + done > old_size) {
        uint32_t new_size = (uint32_t)(offset + done);
        if (!put_bytes(self, f.entry_off + 28u, &new_size, sizeof(new_size))) goto fail;
    }
    if (done) stamp(self, f.entry_off);
    return (int)done;

fail:
    if (link) fat_put(self, link, 0xFFFu);
    while (newn) fat_free(self, newc[--newn]);
    if (offset + done > old_size) put_bytes(self, f.entry_off + 28u, &old_size, sizeof(old_size));
    return done ? (int)done : -1;
}

static void each_impl(struct fat12 *self, const char *path, fat12_iter iter, void *ctx) { struct fat12_found f; if (!self || !iter || !lookup(self,path,&f) || !(f.inode.attr&0x10u)) return; if (f.inode.cluster==0) scan_root(self,nil,nil,iter,ctx); else scan_dir(self,&f.inode,nil,nil,iter,ctx); }
int fat12(struct fat12 *self, struct blockdevice *device) {
    uint8_t boot[512];
    uint16_t reserved;
    if (!self || !device || !device->readbytes) return 0;
    memset(self, 0, sizeof(*self));
    self->stat=stat_impl; self->create=mk; self->mkdir=md; self->rmdir=rd; self->unlink=rm;
    self->truncate=tr; self->alloc=fat_alloc; self->free=fat_free; self->read=read_impl; self->write=wr; self->each=each_impl; self->device=device;
    if (device->readbytes(device, 0, boot, sizeof(boot)) != (int)sizeof(boot) || !fat32_parse_bpb(boot, sizeof(boot), &self->info) || self->info.fat_type != FAT_TYPE_12) return 0;
    if (self->info.bytes_per_sector != 512u || self->info.sectors_per_cluster == 0 || self->info.total_sectors > device->sector_count) return 0;
    if (!fat_entry(self, 0, &reserved) || (reserved & 0xFF0u) != 0xFF0u || !fat_entry(self, 1, &reserved) || !end_cluster(reserved)) return 0;
    self->mounted=1;
    return 1;
}
