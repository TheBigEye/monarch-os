/**
 * @file dirtest.c
 * @brief Exercise directory descriptor duplication and shared getdents offsets.
 */

#include "base/usr/sys.h"

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "/initrd/bin";
    struct dirent entry[1];
    long fd;
    long copy;
    long count;

    fd = open(path, OREAD);
    if (fd < 0) {
        perror("dirtest: open");
        return 1;
    }

    copy = dup((int)fd);
    if (copy < 0) {
        perror("dirtest: dup");
        close((int)fd);
        return 1;
    }

    puts("dirtest: fd=");
    puti((int32_t)fd);
    puts(" dup=");
    puti((int32_t)copy);
    putch('\n');

    count = getdents((int)fd, entry, 1);
    if (count < 0) {
        perror("dirtest: getdents fd");
        close((int)copy);
        close((int)fd);
        return 1;
    }
    puts("dirtest: first via fd=");
    puts(count ? entry[0].name : "<eof>");
    putch('\n');

    count = getdents((int)copy, entry, 1);
    if (count < 0) {
        perror("dirtest: getdents dup");
        close((int)copy);
        close((int)fd);
        return 1;
    }
    puts("dirtest: next via dup=");
    puts(count ? entry[0].name : "<eof>");
    putch('\n');

    close((int)copy);
    close((int)fd);
    return 0;
}
