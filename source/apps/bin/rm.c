/**
 * @file rm.c
 * @brief Remove one or more files.
 */

#include "base/usr/sys.h"

int main(int argc, char **argv) {
    int status = 0;

    if (argc < 2) {
        puts("usage: rm FILE...\n");
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        if (unlink(argv[i]) < 0) {
            puts("rm: cannot remove ");
            puts(argv[i]);
            putch('\n');
            status = 1;
        }
    }

    return status;
}
