/**
 * @file input.c
 * @brief Kernel-side one-character keyboard wrapper.
 */

#include "base/sys/input.h"

char input(struct keyboard *keys) {
    return keys->read(keys);
}
