/**
 * @file append.c
 * @brief Append text arguments to a file using the userspace syscall API.
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
        puts("usage: append FILE TEXT...\n");
        return 1;
    }

    fd = open(argv[1], OWRITE | OCREATE | OAPPEND);
    if (fd < 0) {
        perror("append");
        return 1;
    }

    putargs((int)fd, 2, argc, argv);
    close((int)fd);
    return 0;
}
