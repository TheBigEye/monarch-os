/**
 * @file rmdir.c
 * @brief Remove empty directories.
 */

#include "base/usr/sys.h"

int main(int argc, char **argv) {
    int status = 0;

    if (argc < 2) {
        eputs("usage: rmdir DIR...\n");
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        if (rmdir(argv[i]) < 0) {
            perror("rmdir");
            status = 1;
        }
    }
    return status;
}
