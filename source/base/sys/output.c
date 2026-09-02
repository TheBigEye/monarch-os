/**
 * @file output.c
 * @brief Minimal string output wrapper for the kernel console.
 */

#include "base/sys/output.h"

void output(struct console *screen, const char *text) {
    screen->write(screen, text);
}
