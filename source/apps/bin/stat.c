/**
 * @file stat.c
 * @brief Print simple file type and size metadata.
 */

#include "base/usr/sys.h"

static int statone(const char *path) {
    struct stat st;

    if (stat(path, &st) < 0) {
        puts("stat: cannot stat ");
        puts(path);
        putch('\n');
        return 1;
    }

    puts(path);
    puts(": ");
    puts(st.type == VFS_DIR ? "directory" : st.type == VFS_FILE ? "file" : "unknown");
    puts(", ");
    putu((uint32_t)st.size);
    puts(" bytes, atime=");
    putu(st.atime);
    puts(", mtime=");
    putu(st.mtime);
    puts("\n");
    return 0;
}

int main(int argc, char **argv) {
    int status = 0;

    if (argc < 2) {
        puts("usage: stat PATH...\n");
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        if (statone(argv[i]) != 0) {
            status = 1;
        }
    }

    return status;
}
