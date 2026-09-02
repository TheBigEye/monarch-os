#ifndef MONARCH_KERNEL_CORE_PANIC_H
#define MONARCH_KERNEL_CORE_PANIC_H 1

#include "base/api/monarch.h"

void panic(const char *fmt, ...) __attribute__((noreturn));

#endif /* MONARCH_KERNEL_CORE_PANIC_H */
