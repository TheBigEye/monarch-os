/**
 * @file args.c
 * @brief Print argc/argv as seen by a Monarch userspace process.
 */

#include "base/usr/sys.h"

int main(int argc, char **argv) {
    puts("argc=");
    puti(argc);
    putch('\n');

    for (int i = 0; i < argc; i++) {
        puts("argv[");
        puti(i);
        puts("]=");
        puts(argv[i]);
        putch('\n');
    }
    return 0;
}
