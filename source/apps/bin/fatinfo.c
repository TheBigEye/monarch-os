/**
 * @file fatinfo.c
 * @brief Inspect a FAT/FAT32 boot sector, FSInfo sector and root entries.
 *
 * This utility deliberately stays small and direct.  It uses Monarch's tiny
 * lseek() wrapper to jump to the FAT32 metadata sectors it wants to inspect,
 * which keeps block-device debugging fast without pulling in a full libc.
 */

#include "base/usr/sys.h"
#include "base/lib/fat32.h"

#define ATTR_VOLUME 0x08u
#define ATTR_DIR    0x10u
#define ATTR_LFN    0x0Fu

#define FSINFO_LEAD    0x41615252u
#define FSINFO_STRUCT  0x61417272u
#define FSINFO_TRAIL   0xAA550000u
#define FSINFO_UNKNOWN 0xFFFFFFFFu

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

static int read_full(int fd, uint8_t *buffer, size_t size) {
    size_t done = 0;

    while (done < size) {
        long count = read(fd, buffer + done, size - done);
        if (count <= 0) {
            return 0;
        }
        done += (size_t)count;
    }
    return 1;
}

static int read_sector_at(const char *path, uint32_t sector_no, uint8_t *sector) {
    long fd;
    long offset = (long)(sector_no * FAT32_SECTOR_SIZE);

    fd = open(path, OREAD);
    if (fd < 0) {
        perror("fatinfo");
        return 0;
    }

    if (lseek((int)fd, offset, SEEK_SET) < 0 || !read_full((int)fd, sector, FAT32_SECTOR_SIZE)) {
        close((int)fd);
        return 0;
    }

    close((int)fd);
    return 1;
}

static const char *fat_type_name(uint32_t type) {
    if (type == FAT_TYPE_12) return "FAT12";
    if (type == FAT_TYPE_16) return "FAT16";
    if (type == FAT_TYPE_32) return "FAT32";
    return "unknown";
}

static void print_text_field(const char *text) {
    for (size_t i = 0; text[i]; i++) {
        if (text[i] != ' ') {
            putch(text[i]);
        }
    }
}

static void print_hint(uint32_t value) {
    if (value == FSINFO_UNKNOWN) {
        puts("unknown");
    } else {
        putu(value);
    }
}

static int fsinfo_signature_ok(const uint8_t sector[FAT32_SECTOR_SIZE]) {
    return le32(sector + 0) == FSINFO_LEAD &&
           le32(sector + 484) == FSINFO_STRUCT &&
           le32(sector + 508) == FSINFO_TRAIL;
}

static void show_one_fsinfo(const char *label, const uint8_t sector[FAT32_SECTOR_SIZE]) {
    puts(label);
    puts(": ");
    if (!fsinfo_signature_ok(sector)) {
        puts("invalid\n");
        return;
    }

    puts("valid free_count=");
    print_hint(le32(sector + 488));
    puts(" next_free=");
    print_hint(le32(sector + 492));
    putch('\n');
}

static void show_fsinfo(const char *path, const struct fat32_info *info) {
    uint8_t sector[FAT32_SECTOR_SIZE];
    uint32_t backup;

    if (!fat32_is_fat32(info)) {
        return;
    }
    if (!info->fsinfo_sector || info->fsinfo_sector >= info->reserved_sectors) {
        puts("fsinfo: not present\n");
        return;
    }

    if (!read_sector_at(path, info->fsinfo_sector, sector)) {
        puts("fsinfo: cannot read\n");
        return;
    }
    show_one_fsinfo("fsinfo", sector);

    backup = (uint32_t)info->backup_boot_sector + (uint32_t)info->fsinfo_sector;
    if (info->backup_boot_sector && backup < info->reserved_sectors && backup != info->fsinfo_sector) {
        if (read_sector_at(path, backup, sector)) {
            show_one_fsinfo("fsinfo_backup", sector);
        }
    }
}

static char lower_ascii(char ch) {
    return ch >= 'A' && ch <= 'Z' ? (char)(ch + ('a' - 'A')) : ch;
}

static int make_short_name(const struct dirent32 *entry, char *out, size_t size) {
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

static void put2(uint32_t value) {
    putch((char)('0' + (value / 10u) % 10u));
    putch((char)('0' + value % 10u));
}

static void put4(uint32_t value) {
    putch((char)('0' + (value / 1000u) % 10u));
    putch((char)('0' + (value / 100u) % 10u));
    putch((char)('0' + (value / 10u) % 10u));
    putch((char)('0' + value % 10u));
}

static int valid_fat_date(uint16_t date) {
    uint32_t month = (date >> 5) & 0x0Fu;
    uint32_t day = date & 0x1Fu;
    return date && month >= 1u && month <= 12u && day >= 1u && day <= 31u;
}

static int valid_fat_time(uint16_t time) {
    uint32_t hour = (time >> 11) & 0x1Fu;
    uint32_t minute = (time >> 5) & 0x3Fu;
    uint32_t second = (time & 0x1Fu) * 2u;
    return hour <= 23u && minute <= 59u && second <= 58u;
}

static void print_fat_datetime(uint16_t date, uint16_t time) {
    uint32_t year;
    uint32_t month;
    uint32_t day;
    uint32_t hour;
    uint32_t minute;
    uint32_t second;

    if (!valid_fat_date(date) || !valid_fat_time(time)) {
        puts("unset");
        return;
    }

    year = 1980u + ((date >> 9) & 0x7Fu);
    month = (date >> 5) & 0x0Fu;
    day = date & 0x1Fu;
    hour = (time >> 11) & 0x1Fu;
    minute = (time >> 5) & 0x3Fu;
    second = (time & 0x1Fu) * 2u;

    put4(year);
    putch('-');
    put2(month);
    putch('-');
    put2(day);
    putch(' ');
    put2(hour);
    putch(':');
    put2(minute);
    putch(':');
    put2(second);
}

static void print_fat_date(uint16_t date) {
    if (!valid_fat_date(date)) {
        puts("unset");
        return;
    }

    put4(1980u + ((date >> 9) & 0x7Fu));
    putch('-');
    put2((date >> 5) & 0x0Fu);
    putch('-');
    put2(date & 0x1Fu);
}

static void show_root_entries(const char *path, const struct fat32_info *info) {
    uint8_t sector[FAT32_SECTOR_SIZE];
    uint32_t root_sector;
    unsigned shown = 0;

    if (!fat32_is_fat32(info) || info->bytes_per_sector != FAT32_SECTOR_SIZE) {
        return;
    }

    root_sector = info->first_data_sector + (info->root_cluster - 2u) * info->sectors_per_cluster;
    if (!read_sector_at(path, root_sector, sector)) {
        puts("root entries: cannot read\n");
        return;
    }

    puts("root entries first sector:\n");
    for (uint32_t off = 0; off < FAT32_SECTOR_SIZE; off += sizeof(struct dirent32)) {
        const struct dirent32 *entry = (const struct dirent32 *)(sector + off);
        char name[64];
        uint8_t first = (uint8_t)entry->name[0];
        uint32_t size;

        if (first == 0x00u) {
            break;
        }
        if (first == 0xE5u || (entry->attr & ATTR_LFN) == ATTR_LFN || (entry->attr & ATTR_VOLUME)) {
            continue;
        }
        if (!make_short_name(entry, name, sizeof(name))) {
            continue;
        }

        size = le32(&entry->size);
        puts("  ");
        putch((entry->attr & ATTR_DIR) ? 'd' : '-');
        putch(' ');
        puts(name);
        puts(" size=");
        putu(size);
        puts(" create=");
        print_fat_datetime(le16(&entry->create_date), le16(&entry->create_time));
        puts(" write=");
        print_fat_datetime(le16(&entry->write_date), le16(&entry->write_time));
        puts(" access=");
        print_fat_date(le16(&entry->access_date));
        putch('\n');

        if (++shown >= 12u) {
            break;
        }
    }
}

static int show(const char *path) {
    uint8_t sector[FAT32_SECTOR_SIZE];
    struct fat32_info info;

    if (!read_sector_at(path, 0, sector)) {
        eputs("fatinfo: cannot read boot sector: ");
        eputs(path);
        eputch('\n');
        return 1;
    }

    if (!fat32_parse_bpb(sector, sizeof(sector), &info)) {
        eputs("fatinfo: invalid or unsupported FAT boot sector: ");
        eputs(path);
        eputch('\n');
        return 1;
    }

    puts(path);
    puts(" type=");
    puts(fat_type_name(info.fat_type));
    puts(" label=");
    print_text_field(info.label);
    puts(" system=");
    print_text_field(info.system);
    putch('\n');

    puts("bytes_per_sector="); putu(info.bytes_per_sector);
    puts(" sectors_per_cluster="); putu(info.sectors_per_cluster);
    puts(" reserved="); putu(info.reserved_sectors);
    puts(" fats="); putu(info.fat_count);
    putch('\n');

    puts("total_sectors="); putu(info.total_sectors);
    puts(" sectors_per_fat="); putu(info.sectors_per_fat);
    puts(" hidden="); putu(info.hidden_sectors);
    putch('\n');

    puts("first_fat="); putu(info.first_fat_sector);
    puts(" first_data="); putu(info.first_data_sector);
    puts(" data_sectors="); putu(info.data_sectors);
    puts(" clusters="); putu(info.total_clusters);
    putch('\n');

    if (fat32_is_fat32(&info)) {
        puts("root_cluster="); putu(info.root_cluster);
        puts(" fsinfo="); putu(info.fsinfo_sector);
        puts(" backup_boot="); putu(info.backup_boot_sector);
        putch('\n');
        show_fsinfo(path, &info);
        show_root_entries(path, &info);
    }

    return 0;
}

int main(int argc, char **argv) {
    int status = 0;

    if (argc < 2) {
        eputs("usage: fatinfo DEVICE...\n");
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        if (show(argv[i]) != 0) {
            status = 1;
        }
    }
    return status;
}
