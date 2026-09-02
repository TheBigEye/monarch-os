#ifndef MONARCH_BASE_SYS_OUTPUT_H
#define MONARCH_BASE_SYS_OUTPUT_H 1

#ifndef MONARCH_KERNEL_BUILD
#error "base/sys is kernel-only; userspace must use base/usr or base/api instead."
#endif

/**
 * @file output.h
 * @brief Tiny output convenience wrapper around the console driver.
 *
 * Most code should talk to a full `struct console`, `struct tty`, or `struct
 * stream` when it needs formatting, cursor control, or structured terminal
 * behaviour.  This header exists for the simplest possible case: “write this
 * NUL-terminated string to the screen”.
 */

#include "drivers/char/console.h"

/**
 * Write a NUL-terminated string to a console.
 *
 * @param screen Console object that receives the text.
 * @param text   String to display.  The text may contain simple control
 *               characters such as '\n', '\r', '\t', and '\b' if the console
 *               implementation supports them.
 */
void output(struct console *screen, const char *text);

#endif /* MONARCH_BASE_SYS_OUTPUT_H */
