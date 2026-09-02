#include "kernel/memory/physical.h"
#include "config/config.h"
#include "kernel/memory/heap.h"

static struct physical *active;

static uint32_t low32(uint64_t value) {
    return value > 0xFFFFFFFFull ? 0xFFFFFFFFu : (uint32_t)value;
}

static void setbit(struct physical *self, size_t page) {
    self->_bitmap[page >> 3] |= (uint8_t)(1u << (page & 7u));
}

static void clearbit(struct physical *self, size_t page) {
    self->_bitmap[page >> 3] &= (uint8_t)~(1u << (page & 7u));
}

static int testbit(struct physical *self, size_t page) {
    return (self->_bitmap[page >> 3] & (uint8_t)(1u << (page & 7u))) != 0;
}

static void markused(struct physical *self, size_t page) {
    if (page >= self->_pages) {
        return;
    }
    if (!testbit(self, page)) {
        setbit(self, page);
        if (self->_free) {
            self->_free--;
        }
    }
}

static void markfree(struct physical *self, size_t page) {
    if (page >= self->_pages) {
        return;
    }
    if (testbit(self, page)) {
        clearbit(self, page);
        self->_free++;
    }
}

static uintptr_t alloc_impl(struct physical *self) {
    for (size_t page = 0; page < self->_pages; page++) {
        if (!testbit(self, page)) {
            markused(self, page);
            return (uintptr_t)(page << 12);
        }
    }
    return 0;
}

static void free_impl(struct physical *self, uintptr_t address) {
    if ((address & (PHYSPAGE - 1u)) != 0) {
        return;
    }
    markfree(self, address >> 12);
}

static void reserve_impl(struct physical *self, uintptr_t start, uintptr_t end) {
    size_t first;
    size_t last;

    if (end <= start) {
        return;
    }

    first = start >> 12;
    last = alignup(end, PHYSPAGE) >> 12;

    for (size_t page = first; page < last; page++) {
        markused(self, page);
    }
}

static void release_impl(struct physical *self, uintptr_t start, uintptr_t end) {
    size_t first;
    size_t last;

    if (end <= start) {
        return;
    }

    first = alignup(start, PHYSPAGE) >> 12;
    last = end >> 12;

    for (size_t page = first; page < last; page++) {
        markfree(self, page);
    }
}

static size_t total_impl(struct physical *self) {
    return self->_pages;
}

static size_t freepages_impl(struct physical *self) {
    return self->_free;
}

static uintptr_t limit_impl(struct physical *self) {
    return self->_limit;
}

static uintptr_t detectlimit(uintptr_t bootinfo) {
    const struct mb2mmap *map = (const struct mb2mmap *)mb2find(bootinfo, MB2_TAG_MMAP);
    uintptr_t best = MONARCH_MEMORY_END;

    if (!map) {
        return best > MONARCH_PHYSICAL_LIMIT ? MONARCH_PHYSICAL_LIMIT : best;
    }

    const uint8_t *pos = (const uint8_t *)map + sizeof(*map);
    const uint8_t *end = (const uint8_t *)map + map->size;

    while (pos + map->entry_size <= end) {
        const struct mb2mmapentry *entry = (const struct mb2mmapentry *)pos;
        if (entry->type == MB2_MEMORY_AVAILABLE && entry->address < 0x100000000ull) {
            uint64_t top64 = entry->address + entry->length;
            uintptr_t top = low32(top64);
            if (top > best) {
                best = top;
            }
        }
        pos += map->entry_size;
    }

    return best > MONARCH_PHYSICAL_LIMIT ? MONARCH_PHYSICAL_LIMIT : best;
}

static void releaseavailable(struct physical *self, uintptr_t bootinfo) {
    const struct mb2mmap *map = (const struct mb2mmap *)mb2find(bootinfo, MB2_TAG_MMAP);

    if (!map) {
        self->release(self, 0x00100000u, MONARCH_MEMORY_END);
        return;
    }

    const uint8_t *pos = (const uint8_t *)map + sizeof(*map);
    const uint8_t *end = (const uint8_t *)map + map->size;

    while (pos + map->entry_size <= end) {
        const struct mb2mmapentry *entry = (const struct mb2mmapentry *)pos;
        if (entry->type == MB2_MEMORY_AVAILABLE && entry->address < 0x100000000ull) {
            uintptr_t start = low32(entry->address);
            uintptr_t top = low32(entry->address + entry->length);
            if (top > self->_limit) {
                top = self->_limit;
            }
            self->release(self, start, top);
        }
        pos += map->entry_size;
    }
}

void physical(struct physical *memory, uintptr_t bootinfo, uintptr_t reserve_end) {
    memset(memory, 0, sizeof(*memory));

    memory->alloc = alloc_impl;
    memory->free = free_impl;
    memory->reserve = reserve_impl;
    memory->release = release_impl;
    memory->total = total_impl;
    memory->freepages = freepages_impl;
    memory->limit = limit_impl;

    memory->_limit = detectlimit(bootinfo) & ~(uintptr_t)(PHYSPAGE - 1u);
    if (memory->_limit > 0xFFFFF000u) {
        memory->_limit = 0xFFFFF000u;
    }
    memory->_pages = memory->_limit >> 12;
    memory->_bytes = alignup((memory->_pages + 7u) >> 3, 16u);
    memory->_bitmap = kmalloc(memory->_bytes);

    if (!memory->_bitmap) {
        memory->_limit = 0;
        memory->_pages = 0;
        memory->_bytes = 0;
        return;
    }

    memset(memory->_bitmap, 0xFF, memory->_bytes);
    memory->_free = 0;

    releaseavailable(memory, bootinfo);
    memory->reserve(memory, 0, reserve_end);
    active = memory;
}

uintptr_t pmmalloc(void) {
    return active ? active->alloc(active) : 0;
}

void pmmfree(uintptr_t address) {
    if (active) {
        active->free(active, address);
    }
}

void pmmreserve(uintptr_t start, uintptr_t end) {
    if (active) {
        active->reserve(active, start, end);
    }
}

void pmmrelease(uintptr_t start, uintptr_t end) {
    if (active) {
        active->release(active, start, end);
    }
}

size_t pmmtotal(void) {
    return active ? active->total(active) : 0;
}

size_t pmmfreepages(void) {
    return active ? active->freepages(active) : 0;
}

uintptr_t pmmlimit(void) {
    return active ? active->limit(active) : 0;
}
