/**
 * @file crt0.c
 * @brief Userspace C entry point that calls main(argc, argv, envp).
 */

#include "base/usr/sys.h"

extern int main(int argc, char **argv, char **envp);

void _start(int argc, char **argv, char **envp) {
    environ = envp;
    exit(main(argc, argv, envp));
}
