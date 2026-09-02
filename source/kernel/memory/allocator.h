#ifndef MONARCH_KERNEL_MEMORY_ALLOCATOR_H
#define MONARCH_KERNEL_MEMORY_ALLOCATOR_H 1

#include "base/api/monarch.h"

struct allocator {
    void *(*alloc)(struct allocator *self, size_t size);
    void (*free)(struct allocator *self, void *ptr);
};

void allocator(struct allocator *self);

#endif /* MONARCH_KERNEL_MEMORY_ALLOCATOR_H */
