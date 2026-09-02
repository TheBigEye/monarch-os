/**
 * @file memory.c
 * @brief Kernel heap wrappers used by kernel-only base helpers.
 */

#include "base/sys/memory.h"
#include "kernel/memory/heap.h"

void *alloc(size_t size) {
    return kmalloc(size);
}

void *zero(size_t count, size_t size) {
    return kcalloc(count, size);
}

void release(void *ptr) {
    kfree(ptr);
}

char *duplicate(const char *text) {
    return kstrdup(text);
}
