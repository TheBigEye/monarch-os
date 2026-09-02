/**
 * @file grep.c
 * @brief Simple substring grep for files or stdin; no regex yet.
 */

#include "base/usr/sys.h"

#define LINE_MAX 256u

static int contains(const char *text, const char *pattern) {
    size_t plen = strlen(pattern);

    if (plen == 0) {
        return 1;
    }

    for (size_t i = 0; text[i]; i++) {
        size_t j = 0;
        while (j < plen && text[i + j] == pattern[j]) {
            j++;
        }
        if (j == plen) {
            return 1;
        }
    }

    return 0;
}

static void emitline(const char *prefix, const char *line) {
    if (prefix) {
        puts(prefix);
        putch(':');
    }
    puts(line);
    putch('\n');
}

static int grepfd(int fd, const char *pattern, const char *prefix) {
    char ch;
    char line[LINE_MAX];
    size_t used = 0;
    int matched = 1;

    for (;;) {
        long got = read(fd, &ch, 1);
        if (got < 0) {
            perror("grep");
            return 2;
        }
        if (got == 0) {
            break;
        }

        if (ch == '\r') {
            continue;
        }
        if (ch == '\n') {
            line[used] = '\0';
            if (contains(line, pattern)) {
                emitline(prefix, line);
                matched = 0;
            }
            used = 0;
            continue;
        }

        if (used + 1u < sizeof(line)) {
            line[used++] = ch;
        }
    }

    if (used) {
        line[used] = '\0';
        if (contains(line, pattern)) {
            emitline(prefix, line);
            matched = 0;
        }
    }

    return matched;
}

static int grepfile(const char *path, const char *pattern, int prefix) {
    long fd = open(path, OREAD);
    int status;

    if (fd < 0) {
        perror("grep");
        return 2;
    }

    status = grepfd((int)fd, pattern, prefix ? path : nil);
    close((int)fd);
    return status;
}

int main(int argc, char **argv) {
    int status = 1;
    int multiple;

    if (argc < 2) {
        puts("usage: grep PATTERN [FILE...]\n");
        return 2;
    }

    if (argc == 2) {
        if (isatty(0)) {
            puts("usage: grep PATTERN [FILE...] or pipe input to grep\n");
            return 2;
        }
        return grepfd(0, argv[1], nil);
    }

    multiple = argc > 3;
    for (int i = 2; i < argc; i++) {
        int result = grepfile(argv[i], argv[1], multiple);
        if (result == 0) {
            status = 0;
        } else if (result == 2) {
            status = 2;
        }
    }

    return status;
}
