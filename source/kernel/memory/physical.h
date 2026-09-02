#ifndef MONARCH_KERNEL_MEMORY_PHYSICAL_H
#define MONARCH_KERNEL_MEMORY_PHYSICAL_H 1

#include "boot/bootloader.h"
#include "base/api/monarch.h"

#define PHYSPAGE 4096u

struct physical {
    uintptr_t (*alloc)(struct physical *self);
    void (*free)(struct physical *self, uintptr_t address);
    void (*reserve)(struct physical *self, uintptr_t start, uintptr_t end);
    void (*release)(struct physical *self, uintptr_t start, uintptr_t end);
    size_t (*total)(struct physical *self);
    size_t (*freepages)(struct physical *self);
    uintptr_t (*limit)(struct physical *self);

    uint8_t *_bitmap;
    size_t _bytes;
    size_t _pages;
    size_t _free;
    uintptr_t _limit;
};

void physical(struct physical *memory, uintptr_t bootinfo, uintptr_t reserve_end);
uintptr_t pmmalloc(void);
void pmmfree(uintptr_t address);
void pmmreserve(uintptr_t start, uintptr_t end);
void pmmrelease(uintptr_t start, uintptr_t end);
size_t pmmtotal(void);
size_t pmmfreepages(void);
uintptr_t pmmlimit(void);

#endif /* MONARCH_KERNEL_MEMORY_PHYSICAL_H */
