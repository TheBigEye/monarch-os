#ifndef MONARCH_KERNEL_MEMORY_HEAP_H
#define MONARCH_KERNEL_MEMORY_HEAP_H 1

#include "base/api/monarch.h"

struct heap {
    void (*init)(struct heap *self, uintptr_t start, uintptr_t end);
    void *(*alloc)(struct heap *self, size_t size);
    void *(*calloc)(struct heap *self, size_t count, size_t size);
    void (*free)(struct heap *self, void *ptr);
    size_t (*used)(struct heap *self);
    size_t (*freebytes)(struct heap *self);

    uintptr_t _start;
    uintptr_t _end;
    uintptr_t _break;
    uintptr_t _mapped;
    size_t _used;
    int _paged;
};

void heap(struct heap *self);
void kheap(uintptr_t start, uintptr_t end);
int kheapvirtual(uintptr_t start, size_t reserve, size_t initial);
void *kmalloc(size_t size);
void *kcalloc(size_t count, size_t size);
void kfree(void *ptr);
size_t kused(void);
size_t kfreebytes(void);
uintptr_t kheapstart(void);
uintptr_t kheapend(void);
uintptr_t kheapbreak(void);
uintptr_t kheapmapped(void);
int kheappaged(void);
char *kstrdup(const char *text);

#endif /* MONARCH_KERNEL_MEMORY_HEAP_H */
