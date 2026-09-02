/**
 * @file umount.c
 * @brief Unmount a VFS mount point.
 */

#include "base/usr/sys.h"

int main(int argc, char **argv) {
    int status = 0;

    if (argc < 2) {
        eputs("usage: umount TARGET...\n");
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        if (umount(argv[i]) < 0) {
            perror("umount");
            status = 1;
        }
    }
    return status;
}
