/**
 * @file env.c
 * @brief Print the environment vector inherited by this userspace process.
 */

#include "base/usr/sys.h"

int main(int argc, char **argv, char **envp) {
    (void)argc;
    (void)argv;

    if (!envp) {
        return 0;
    }

    for (char **env = envp; *env; env++) {
        puts(*env);
        putch('\n');
    }
    return 0;
}
