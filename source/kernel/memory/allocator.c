#include "kernel/memory/allocator.h"
#include "kernel/memory/heap.h"

static void *alloc_impl(struct allocator *self, size_t size) {
    unused(self);
    return kmalloc(size);
}

static void free_impl(struct allocator *self, void *ptr) {
    unused(self);
    kfree(ptr);
}

void allocator(struct allocator *self) {
    self->alloc = alloc_impl;
    self->free = free_impl;
}
