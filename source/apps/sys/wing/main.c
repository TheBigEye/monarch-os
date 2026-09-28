/**
 * @file main.c
 * @brief Executable entry point for the Wing service.
 */
#include "base/usr/sys.h"
#include "wing.h"

int main(int argc, char **argv) {
    Wing wing;
    Wing_init(&wing, argc, argv);
    if (!wing.run) {
        eputs("wing: runtime method table is not initialized\n");
        return 1;
    }
    return wing.run(&wing);
}
