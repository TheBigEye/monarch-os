/**
 * @file cat.c
 * @brief Concatenate files or stdin to stdout; intentionally tiny Unix-style cat.
 */

#include "base/usr/sys.h"

static int catfd(int fd) {
    char buffer[256];

    for (;;) {
        long count = read(fd, buffer, sizeof(buffer));
        if (count < 0) {
            perror("cat");
            return 1;
        }
        if (count == 0) {
            break;
        }
        if (write(1, buffer, (size_t)count) < 0) {
            perror("cat");
            return 1;
        }
    }

    return 0;
}

static int catone(const char *path) {
    long fd = open(path, OREAD);
    int status;

    if (fd < 0) {
        perror("cat");
        return 1;
    }

    status = catfd((int)fd);
    close((int)fd);
    return status;
}

int main(int argc, char **argv) {
    int status = 0;

    if (argc < 2) {
        if (isatty(0)) {
            puts("usage: cat FILE... or pipe input to cat\n");
            return 1;
        }
        return catfd(0);
    }

    for (int i = 1; i < argc; i++) {
        if (catone(argv[i]) != 0) {
            status = 1;
        }
    }

    return status;
}
