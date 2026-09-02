#ifndef MONARCH_BASE_SYS_MEMORY_H
#define MONARCH_BASE_SYS_MEMORY_H 1

#ifndef MONARCH_KERNEL_BUILD
#error "base/sys is kernel-only; userspace must use base/usr or base/api instead."
#endif

/**
 * @file memory.h
 * @brief Friendly allocation API used by common code and in-kernel apps.
 *
 * The base library should not need to know the details of the kernel heap.
 * These wrappers make allocation look familiar (`alloc`, `zero`, `release`) and
 * keep the call sites readable.  Under the hood they currently forward to the
 * kernel heap (`kmalloc`, `kcalloc`, `kfree`), but the rest of base/ does not
 * need to include heap internals directly.
 */

#include "base/api/monarch.h"

/** Allocate `size` bytes. The returned memory is uninitialised. */
void *alloc(size_t size);

/** Allocate `count * size` bytes and set all bytes to zero. */
void *zero(size_t count, size_t size);

/** Release memory previously returned by alloc() or zero(). */
void release(void *ptr);

/** Allocate and copy a NUL-terminated string. Returns nil on allocation failure. */
char *duplicate(const char *text);

#endif /* MONARCH_BASE_SYS_MEMORY_H */
