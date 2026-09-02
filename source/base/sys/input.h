#ifndef MONARCH_BASE_SYS_INPUT_H
#define MONARCH_BASE_SYS_INPUT_H 1

#ifndef MONARCH_KERNEL_BUILD
#error "base/sys is kernel-only; userspace must use base/usr or base/api instead."
#endif

/**
 * @file input.h
 * @brief Tiny input convenience wrapper.
 *
 * This file intentionally hides the concrete keyboard method call behind a very
 * small function.  It is mostly useful for simple applications and demos that
 * only need “read one character” and do not care about scan codes, modifiers, or
 * full key events.
 */

#include "drivers/char/keyboard.h"

/** Block until the keyboard provides a printable character and return it. */
char input(struct keyboard *keys);

#endif /* MONARCH_BASE_SYS_INPUT_H */
