/**
 * @file edlin.c
 * @brief A deliberately small one-line editor for creating simple text files.
 */

#include "base/usr/sys.h"

#define LINE_MAX 1024u

static int readline(char *line, size_t size) {
    size_t used = 0;

    while (used + 1u < size) {
        char ch;
        long count = read(0, &ch, 1);
        if (count < 0) {
            return -1;
        }
        if (count == 0) {
            continue;
        }

        if (ch == '\r') {
            continue;
        }
        if (ch == '\n') {
            putch('\n');
            break;
        }
        if (ch == '\b') {
            if (used) {
                used--;
                puts("\b \b");
            }
            continue;
        }

        line[used++] = ch;
        putch(ch);
    }

    line[used] = '\0';
    return (int)used;
}

int main(int argc, char **argv) {
    char line[LINE_MAX];
    long fd;
    int length;

    if (argc != 2) {
        puts("usage: edlin FILE\n");
        return 1;
    }

    puts("edlin: one-line editor for ");
    puts(argv[1]);
    puts("\n> ");

    length = readline(line, sizeof(line));
    if (length < 0) {
        perror("edlin: read");
        return 1;
    }

    fd = open(argv[1], OWRITE | OCREATE | OTRUNC);
    if (fd < 0) {
        perror("edlin: open");
        return 1;
    }

    if (write((int)fd, line, (size_t)length) < 0) {
        close((int)fd);
        perror("edlin: write");
        return 1;
    }

    close((int)fd);
    return 0;
}
