#include "kernel/memory/heap.h"
#include "arch/x86/paging.h"
#include "kernel/memory/physical.h"
#include "kernel/scheduler/spinlock.h"

#define MAGIC 0x4D484541u /* MHEA */

typedef struct block block;
struct block {
    uint32_t magic;
    uint32_t free;
    size_t size;
    block *next;
};

static struct heap global;
static block *head;

/*
 * The free-list (head) and the block metadata it links are shared mutable
 * state. Nothing here yields mid-operation today (no driver IRQ handler
 * calls kmalloc/kfree, and soft-preemption only switches threads at explicit
 * schedpoll()/yield() points), so this guard is currently uncontended in
 * practice. It exists so this stays true by construction rather than by
 * convention alone: any future caller from interrupt context, or a move to real
 * preemption/SMP, is protected instead of silently corrupting the heap.
 */
static struct spinlock guard;

static size_t align(size_t size) {
    return (size + 7u) & ~7u;
}

static int commit(struct heap *self, uintptr_t end) {
    uintptr_t target;

    if (!self->_paged) {
        return end <= self->_end;
    }

    if (end > self->_end) {
        return 0;
    }

    target = alignup(end, PAGE_SIZE);
    while (self->_mapped < target) {
        uintptr_t physical = pmmalloc();
        if (!physical) {
            return 0;
        }
        if (!pagemap(self->_mapped, physical, PAGE_WRITE)) {
            pmmfree(physical);
            return 0;
        }
        memset((void *)self->_mapped, 0, PAGE_SIZE);
        self->_mapped += PAGE_SIZE;
    }

    return 1;
}

static void initraw(struct heap *self, uintptr_t start, uintptr_t end, uintptr_t mapped, int paged) {
    self->_start = alignup(start, 16);
    self->_end = end;
    self->_break = self->_start;
    self->_mapped = mapped;
    self->_used = 0;
    self->_paged = paged;
    head = nil;
    spinlock(&guard);
}

static void init(struct heap *self, uintptr_t start, uintptr_t end) {
    memset(self, 0, sizeof(*self));
    heap(self);
    initraw(self, start, end, end, 0);
}

static void split(block *b, size_t size) {
    block *n;
    if (b->size < size + sizeof(block) + 16) {
        return;
    }
    n = (block *)((uint8_t *)(b + 1) + size);
    n->magic = MAGIC;
    n->free = 1;
    n->size = b->size - size - sizeof(block);
    n->next = b->next;
    b->size = size;
    b->next = n;
}

static void merge(void) {
    block *b = head;
    while (b && b->next) {
        if (b->free && b->next->free) {
            b->size += sizeof(block) + b->next->size;
            b->next = b->next->next;
        } else {
            b = b->next;
        }
    }
}

static void *alloc(struct heap *self, size_t size) {
    block *b;
    block *last = nil;
    uintptr_t needed;
    void *result = nil;

    if (!size) {
        return nil;
    }

    size = align(size);

    guard.lock(&guard);

    for (b = head; b; b = b->next) {
        if (b->free && b->size >= size) {
            split(b, size);
            b->free = 0;
            self->_used += b->size;
            result = b + 1;
            goto done;
        }
        last = b;
    }

    needed = self->_break + sizeof(block) + size;
    if (!commit(self, needed)) {
        goto done;
    }

    b = (block *)self->_break;
    b->magic = MAGIC;
    b->free = 0;
    b->size = size;
    b->next = nil;
    self->_break = needed;
    self->_used += size;

    if (last) {
        last->next = b;
    } else {
        head = b;
    }

    result = b + 1;

done:
    guard.unlock(&guard);
    return result;
}

static void *calloc_impl(struct heap *self, size_t count, size_t size) {
    size_t total = count * size;
    void *ptr;

    if (size && total / size != count) {
        return nil;
    }

    ptr = self->alloc(self, total);
    if (ptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}

static void free_impl(struct heap *self, void *ptr) {
    block *b;
    if (!ptr) {
        return;
    }

    guard.lock(&guard);

    b = ((block *)ptr) - 1;
    if (b->magic != MAGIC || b->free) {
        guard.unlock(&guard);
        return;
    }

    b->free = 1;
    if (self->_used >= b->size) {
        self->_used -= b->size;
    }
    merge();

    guard.unlock(&guard);
}

static size_t used(struct heap *self) {
    return self->_used;
}

static size_t freebytes(struct heap *self) {
    uintptr_t hard_free = self->_end > self->_break ? self->_end - self->_break : 0;
    size_t reusable = 0;

    guard.lock(&guard);
    for (block *b = head; b; b = b->next) {
        if (b->free) {
            reusable += b->size;
        }
    }
    guard.unlock(&guard);

    return (size_t)hard_free + reusable;
}

void heap(struct heap *self) {
    memset(self, 0, sizeof(*self));
    self->init = init;
    self->alloc = alloc;
    self->calloc = calloc_impl;
    self->free = free_impl;
    self->used = used;
    self->freebytes = freebytes;
}

void kheap(uintptr_t start, uintptr_t end) {
    heap(&global);
    global.init(&global, start, end);
}

int kheapvirtual(uintptr_t start, size_t reserve, size_t initial) {
    uintptr_t end = start + reserve;
    uintptr_t mapped = start;

    heap(&global);
    initraw(&global, start, end, mapped, 1);

    if (!commit(&global, start + initial)) {
        return 0;
    }

    return 1;
}

void *kmalloc(size_t size) {
    return global.alloc(&global, size);
}

void *kcalloc(size_t count, size_t size) {
    return global.calloc(&global, count, size);
}

void kfree(void *ptr) {
    global.free(&global, ptr);
}

size_t kused(void) {
    return global.used(&global);
}

size_t kfreebytes(void) {
    return global.freebytes(&global);
}

uintptr_t kheapstart(void) {
    return global._start;
}

uintptr_t kheapend(void) {
    return global._end;
}

uintptr_t kheapbreak(void) {
    return global._break;
}

uintptr_t kheapmapped(void) {
    return global._mapped;
}

int kheappaged(void) {
    return global._paged;
}

char *kstrdup(const char *text) {
    size_t size = strlen(text) + 1;
    char *copy = kmalloc(size);
    if (copy) {
        memcpy(copy, text, size);
    }
    return copy;
}
