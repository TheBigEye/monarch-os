/**
 * @file wc.c
 * @brief Count lines, words and bytes in files or stdin.
 */

#include "base/usr/sys.h"

struct counts {
    uint32_t lines;
    uint32_t words;
    uint32_t bytes;
};

static void add(struct counts *out, const struct counts *in) {
    out->lines += in->lines;
    out->words += in->words;
    out->bytes += in->bytes;
}

static int countfd(int fd, struct counts *out) {
    char buffer[256];
    int inword = 0;

    memset(out, 0, sizeof(*out));

    for (;;) {
        long got = read(fd, buffer, sizeof(buffer));
        if (got < 0) {
            perror("wc");
            return 1;
        }
        if (got == 0) {
            break;
        }

        out->bytes += (uint32_t)got;
        for (long i = 0; i < got; i++) {
            if (buffer[i] == '\n') {
                out->lines++;
            }
            if (space(buffer[i])) {
                inword = 0;
            } else if (!inword) {
                out->words++;
                inword = 1;
            }
        }
    }

    return 0;
}

static void printcounts(const struct counts *count, const char *name) {
    putu(count->lines);
    putch(' ');
    putu(count->words);
    putch(' ');
    putu(count->bytes);
    if (name) {
        putch(' ');
        puts(name);
    }
    putch('\n');
}

static int countfile(const char *path, struct counts *out) {
    long fd = open(path, OREAD);
    int status;

    if (fd < 0) {
        perror("wc");
        memset(out, 0, sizeof(*out));
        return 1;
    }

    status = countfd((int)fd, out);
    close((int)fd);
    return status;
}

int main(int argc, char **argv) {
    struct counts count;
    struct counts total;
    int status = 0;

    if (argc < 2) {
        if (isatty(0)) {
            puts("usage: wc [FILE...] or pipe input to wc\n");
            return 1;
        }
        if (countfd(0, &count) != 0) {
            return 1;
        }
        printcounts(&count, nil);
        return 0;
    }

    memset(&total, 0, sizeof(total));
    for (int i = 1; i < argc; i++) {
        if (countfile(argv[i], &count) != 0) {
            status = 1;
            continue;
        }
        add(&total, &count);
        printcounts(&count, argv[i]);
    }

    if (argc > 2) {
        printcounts(&total, "total");
    }

    return status;
}
