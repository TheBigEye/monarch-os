/**
 * @file mkdir.c
 * @brief Create one or more directories through the syscall layer.
 */

#include "base/usr/sys.h"

int main(int argc, char **argv) {
    int status = 0;

    if (argc < 2) {
        puts("usage: mkdir DIR...\n");
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        if (mkdir(argv[i]) < 0) {
            puts("mkdir: cannot create ");
            puts(argv[i]);
            putch('\n');
            status = 1;
        }
    }

    return status;
}
