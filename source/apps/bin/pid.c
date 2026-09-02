/**
 * @file pid.c
 * @brief Print the current process id.
 */

#include "base/usr/sys.h"

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    puts("pid=");
    puti((int32_t)getpid());
    putch('\n');
    return 0;
}
