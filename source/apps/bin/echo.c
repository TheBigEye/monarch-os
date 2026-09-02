/**
 * @file echo.c
 * @brief Print command-line arguments separated by spaces.
 */

#include "base/usr/sys.h"

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (i > 1) {
            putch(' ');
        }
        puts(argv[i]);
    }
    putch('\n');
    return 0;
}
