/**
 * @file ls.c
 * @brief List directory entries using fd-backed getdents.
 */

#include "base/usr/sys.h"

#define MAX_ENTRIES 64u

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : ".";
    struct dirent entries[MAX_ENTRIES];
    long fd;
    long count;

    fd = open(path, OREAD);
    if (fd < 0) {
        perror("ls");
        return 1;
    }

    count = getdents((int)fd, entries, MAX_ENTRIES);
    close((int)fd);
    if (count < 0) {
        perror("ls");
        return 1;
    }

    for (long i = 0; i < count; i++) {
        puts(entries[i].name);
        if (entries[i].type == VFS_DIR) {
            putch('/');
        }
        putch('\n');
    }

    return 0;
}
