/**
 * @file touch.c
 * @brief Create files if they do not already exist.
 */

#include "base/usr/sys.h"

int main(int argc, char **argv) {
    int status = 0;

    if (argc < 2) {
        puts("usage: touch FILE...\n");
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        if (create(argv[i]) < 0) {
            puts("touch: cannot create ");
            puts(argv[i]);
            putch('\n');
            status = 1;
        }
    }

    return status;
}
