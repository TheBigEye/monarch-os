/**
 * @file sleep.c
 * @brief Sleep for a requested number of milliseconds.
 */

#include "base/usr/sys.h"

int main(int argc, char **argv) {
    uint32_t ms;

    if (argc != 2) {
        puts("usage: sleep MS\n");
        return 1;
    }

    ms = (uint32_t)atoi(argv[1]);
    sleepms(ms);
    return 0;
}
