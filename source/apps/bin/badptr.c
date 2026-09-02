/**
 * @file badptr.c
 * @brief Negative test for syscall pointer validation and errno handling.
 */

#include "base/usr/sys.h"

int main(int argc, char **argv) {
    long result;
    (void)argc;
    (void)argv;

    puts("badptr: write from invalid pointer should fail, not panic\n");
    result = write(1, (const void *)0x00000001u, 8);
    puts("badptr: write returned ");
    puti((int32_t)result);
    puts(", errno=");
    puti(errno);
    puts(" (");
    puts(strerror(errno));
    puts(")\n");
    return result < 0 && errno == EFAULT ? 0 : 1;
}
