/**
 * @file pwd.c
 * @brief Print the current working directory.
 */

#include "base/usr/sys.h"

int main(int argc, char **argv) {
    char cwd[256];
    (void)argc;
    (void)argv;

    if (getcwd(cwd, sizeof(cwd)) < 0) {
        puts("pwd: getcwd failed\n");
        return 1;
    }

    puts(cwd);
    putch('\n');
    return 0;
}
