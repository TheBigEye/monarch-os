/**
 * @file head.c
 * @brief Print the first N lines from files or stdin.
 */

#include "base/usr/sys.h"

static int headfd(int fd, uint32_t maxlines) {
    char buffer[128];
    uint32_t lines = 0;

    while (lines < maxlines) {
        long got = read(fd, buffer, sizeof(buffer));
        if (got < 0) {
            perror("head");
            return 1;
        }
        if (got == 0) {
            break;
        }

        for (long i = 0; i < got && lines < maxlines; i++) {
            write(1, &buffer[i], 1);
            if (buffer[i] == '\n') {
                lines++;
            }
        }
    }

    return 0;
}

static int headfile(const char *path, uint32_t maxlines) {
    long fd = open(path, OREAD);
    int status;

    if (fd < 0) {
        perror("head");
        return 1;
    }

    status = headfd((int)fd, maxlines);
    close((int)fd);
    return status;
}

int main(int argc, char **argv) {
    uint32_t maxlines = 10;
    int first = 1;
    int status = 0;

    if (argc > 2 && strcmp(argv[1], "-n") == 0) {
        int value = atoi(argv[2]);
        if (value <= 0) {
            puts("usage: head [-n LINES] [FILE...]\n");
            return 1;
        }
        maxlines = (uint32_t)value;
        first = 3;
    }

    if (first >= argc) {
        if (isatty(0)) {
            puts("usage: head [-n LINES] [FILE...] or pipe input to head\n");
            return 1;
        }
        return headfd(0, maxlines);
    }

    for (int i = first; i < argc; i++) {
        if (argc - first > 1) {
            puts("==> ");
            puts(argv[i]);
            puts(" <==\n");
        }
        if (headfile(argv[i], maxlines) != 0) {
            status = 1;
        }
    }

    return status;
}
