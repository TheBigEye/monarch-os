/**
 * @file write.c
 * @brief Write text arguments to a file, truncating or creating it.
 */

#include "base/usr/sys.h"

static void putargs(int fd, int start, int argc, char **argv) {
    for (int i = start; i < argc; i++) {
        if (i > start) {
            write(fd, " ", 1);
        }
        write(fd, argv[i], strlen(argv[i]));
    }
}

int main(int argc, char **argv) {
    long fd;

    if (argc < 3) {
        puts("usage: write FILE TEXT...\n");
        return 1;
    }

    fd = open(argv[1], OWRITE | OCREATE | OTRUNC);
    if (fd < 0) {
        perror("write");
        return 1;
    }

    putargs((int)fd, 2, argc, argv);
    close((int)fd);
    return 0;
}
