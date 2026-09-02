/**
 * @file partinfo.c
 * @brief Print the primary DOS/MBR partition table from a block device.
 */

#include "base/usr/sys.h"
#include "base/lib/mbr.h"

static void puthex8(uint8_t value) {
    const char *digits = "0123456789abcdef";
    putch(digits[(value >> 4) & 0xFu]);
    putch(digits[value & 0xFu]);
}

static int show(const char *path) {
    uint8_t sector[MBR_SECTOR_SIZE];
    struct mbr_table table;
    long fd;
    long got;
    int any = 0;

    fd = open(path, OREAD);
    if (fd < 0) {
        perror("partinfo");
        return 1;
    }

    got = 0;
    while (got < (long)sizeof(sector)) {
        long count = read((int)fd, sector + got, sizeof(sector) - (size_t)got);
        if (count < 0) {
            close((int)fd);
            perror("partinfo");
            return 1;
        }
        if (count == 0) {
            break;
        }
        got += count;
    }
    close((int)fd);
    if (got < (long)sizeof(sector) || !mbr_parse(sector, sizeof(sector), &table)) {
        eputs("partinfo: not a valid MBR: ");
        eputs(path);
        eputch('\n');
        return 1;
    }

    puts(path);
    puts(" mbr=valid\n");
    for (unsigned i = 0; i < MBR_PARTITIONS; i++) {
        struct mbr_partition *part = &table.part[i];
        if (part->type == 0 || part->sectors == 0) {
            continue;
        }
        any = 1;
        puts("p");
        putu(i + 1u);
        puts(" status=0x");
        puthex8(part->status);
        puts(" type=0x");
        puthex8(part->type);
        putch(' ');
        puts(mbr_type_name(part->type));
        puts(" start=");
        putu(part->first_lba);
        puts(" sectors=");
        putu(part->sectors);
        putch('\n');
    }

    if (!any) {
        puts("no primary partitions\n");
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        eputs("usage: partinfo DEVICE...\n");
        return 1;
    }

    int status = 0;
    for (int i = 1; i < argc; i++) {
        if (show(argv[i]) != 0) {
            status = 1;
        }
    }
    return status;
}
