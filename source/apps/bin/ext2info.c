/**
 * @file ext2info.c
 * @brief Inspect an EXT2 superblock from a block device.
 */

#include "base/usr/sys.h"
#include "base/lib/ext2.h"

static int read_super(const char *path, uint8_t *super, size_t size) {
    uint8_t skip[EXT2_SUPER_OFFSET];
    long fd;
    long got = 0;

    fd = open(path, OREAD);
    if (fd < 0) {
        perror("ext2info");
        return 0;
    }

    while (got < (long)sizeof(skip)) {
        long count = read((int)fd, skip + got, sizeof(skip) - (size_t)got);
        if (count <= 0) {
            close((int)fd);
            return 0;
        }
        got += count;
    }

    got = 0;
    while (got < (long)size) {
        long count = read((int)fd, super + got, size - (size_t)got);
        if (count <= 0) {
            close((int)fd);
            return 0;
        }
        got += count;
    }

    close((int)fd);
    return 1;
}

static int show(const char *path) {
    uint8_t super[EXT2_SUPER_SIZE];
    struct ext2_info info;

    if (!read_super(path, super, sizeof(super)) || !ext2_parse_super(super, sizeof(super), &info)) {
        eputs("ext2info: invalid EXT2 superblock: ");
        eputs(path);
        eputch('\n');
        return 1;
    }

    puts(path);
    puts(" ext2=valid\n");
    puts("inodes="); putu(info.inodes_count);
    puts(" blocks="); putu(info.blocks_count);
    puts(" block_size="); putu(info.block_size);
    putch('\n');
    puts("blocks_per_group="); putu(info.blocks_per_group);
    puts(" inodes_per_group="); putu(info.inodes_per_group);
    puts(" groups="); putu(info.groups_count);
    putch('\n');
    puts("inode_size="); putu(info.inode_size);
    puts(" first_inode="); putu(info.first_inode);
    puts(" first_data_block="); putu(info.first_data_block);
    putch('\n');
    return 0;
}

int main(int argc, char **argv) {
    int status = 0;

    if (argc < 2) {
        eputs("usage: ext2info DEVICE...\n");
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        if (show(argv[i]) != 0) {
            status = 1;
        }
    }
    return status;
}
