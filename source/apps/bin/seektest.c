/**
 * @file seektest.c
 * @brief Tiny lseek smoke test for regular files and block devices.
 *
 * This utility is intentionally simple: seek to a byte offset, read a small
 * sample, and print printable bytes as text.  It exists to prove Monarch's
 * open-file offset semantics without adding a large hexdump program yet.
 */

#include "base/usr/sys.h"

static char printable(char ch) {
    return ch >= 32 && ch < 127 ? ch : '.';
}

int main(int argc, char **argv) {
    const char *path;
    long offset;
    long count;
    long fd;
    char buffer[65];
    long got;

    if (argc < 3) {
        puts("usage: seektest FILE OFFSET [COUNT]\n");
        return 1;
    }

    path = argv[1];
    offset = atoi(argv[2]);
    count = argc > 3 ? atoi(argv[3]) : 16;
    if (count < 1) {
        count = 1;
    }
    if (count > (long)(sizeof(buffer) - 1u)) {
        count = (long)(sizeof(buffer) - 1u);
    }

    fd = open(path, OREAD);
    if (fd < 0) {
        perror("seektest: open");
        return 1;
    }

    offset = lseek((int)fd, offset, SEEK_SET);
    if (offset < 0) {
        perror("seektest: lseek");
        close((int)fd);
        return 1;
    }

    got = read((int)fd, buffer, (size_t)count);
    if (got < 0) {
        perror("seektest: read");
        close((int)fd);
        return 1;
    }

    for (long i = 0; i < got; i++) {
        buffer[i] = printable(buffer[i]);
    }
    buffer[got] = '\0';

    puts("offset=");
    puti((int32_t)offset);
    puts(" bytes=");
    puti((int32_t)got);
    puts(" data=");
    puts(buffer);
    putch('\n');

    close((int)fd);
    return 0;
}
