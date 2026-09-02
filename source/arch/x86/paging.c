/**
 * @file paging.c
 * @brief Page-directory management, mapping helpers and per-process address spaces.
 *
 */

#include "arch/x86/paging.h"
#include "kernel/core/panic.h"
#include "kernel/memory/physical.h"

#define ENTRIES 1024u
#define FRAME_MASK 0xFFFFF000u
#define CR0_PG 0x80000000u

static uint32_t *directory;
static uintptr_t directory_physical;
static uintptr_t kernel_directory;
static int enabled;

static inline void loadcr3(uintptr_t address) {
    __asm__ volatile ("mov %0, %%cr3" :: "r"(address) : "memory");
}

static inline uint32_t readcr0(void) {
    uint32_t value;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(value));
    return value;
}

static inline void writecr0(uint32_t value) {
    __asm__ volatile ("mov %0, %%cr0" :: "r"(value) : "memory");
}

static inline void invlpg(uintptr_t address) {
    __asm__ volatile ("invlpg (%0)" :: "r"(address) : "memory");
}

int pageuser(uintptr_t virtual_address) {
    return virtual_address >= PAGE_USER_BASE && virtual_address < PAGE_USER_TOP;
}

static int userindex(uint32_t index) {
    return index >= (PAGE_USER_BASE >> 22) && index < (PAGE_USER_TOP >> 22);
}

static uint32_t *dirptr(uintptr_t physical) {
    return physical ? (uint32_t *)physical : directory;
}

static uint32_t *tableinptr(uint32_t *dir, uint32_t index, int create, uint32_t flags) {
    uintptr_t page;

    if (!dir) {
        return nil;
    }

    if (dir[index] & PAGE_PRESENT) {
        return (uint32_t *)(dir[index] & FRAME_MASK);
    }

    if (!create) {
        return nil;
    }

    page = pmmalloc();
    if (!page) {
        return nil;
    }

    memset((void *)page, 0, PAGE_SIZE);
    dir[index] = (uint32_t)((page & FRAME_MASK) | PAGE_PRESENT | PAGE_WRITE | (flags & PAGE_USER));
    return (uint32_t *)page;
}

static void syncglobal(uintptr_t physical) {
    uint32_t *target = dirptr(physical);
    uint32_t *kernel = dirptr(kernel_directory);

    if (!target || !kernel || target == kernel) {
        return;
    }

    for (uint32_t i = 0; i < ENTRIES; i++) {
        if (!userindex(i)) {
            target[i] = kernel[i];
        }
    }
}

int pagemapin(uintptr_t physical_directory, uintptr_t virtual_address, uintptr_t physical_address, uint32_t flags) {
    uint32_t dindex = (uint32_t)(virtual_address >> 22);
    uint32_t tindex = (uint32_t)((virtual_address >> 12) & 0x3FFu);
    uint32_t *dir = dirptr(physical_directory);
    uint32_t *entries;

    if (!dir) {
        return 0;
    }

    entries = tableinptr(dir, dindex, 1, flags);
    if (!entries) {
        return 0;
    }

    entries[tindex] = (uint32_t)((physical_address & FRAME_MASK) | (flags & 0xFFFu) | PAGE_PRESENT);

    if (enabled && (!physical_directory || physical_directory == directory_physical)) {
        invlpg(virtual_address);
    }

    return 1;
}

int pagemap(uintptr_t virtual_address, uintptr_t physical_address, uint32_t flags) {
    uint32_t dindex = (uint32_t)(virtual_address >> 22);

    if (!directory) {
        return 0;
    }

    if (kernel_directory && !pageuser(virtual_address)) {
        uint32_t *kernel = dirptr(kernel_directory);
        int ok = pagemapin(kernel_directory, virtual_address, physical_address, flags & ~PAGE_USER);
        if (!ok) {
            return 0;
        }
        if (directory_physical != kernel_directory) {
            directory[dindex] = kernel[dindex];
            if (enabled) {
                invlpg(virtual_address);
            }
        }
        return 1;
    }

    return pagemapin(directory_physical, virtual_address, physical_address, flags);
}

void pageunmapin(uintptr_t physical_directory, uintptr_t virtual_address) {
    uint32_t dindex = (uint32_t)(virtual_address >> 22);
    uint32_t tindex = (uint32_t)((virtual_address >> 12) & 0x3FFu);
    uint32_t *entries = tableinptr(dirptr(physical_directory), dindex, 0, 0);

    if (!entries) {
        return;
    }

    entries[tindex] = 0;
    if (enabled && (!physical_directory || physical_directory == directory_physical)) {
        invlpg(virtual_address);
    }
}

void pageunmap(uintptr_t virtual_address) {
    pageunmapin(directory_physical, virtual_address);
}

uintptr_t pagegetin(uintptr_t physical_directory, uintptr_t virtual_address) {
    uint32_t dindex = (uint32_t)(virtual_address >> 22);
    uint32_t tindex = (uint32_t)((virtual_address >> 12) & 0x3FFu);
    uint32_t *entries = tableinptr(dirptr(physical_directory), dindex, 0, 0);

    if (!entries || !(entries[tindex] & PAGE_PRESENT)) {
        return 0;
    }

    return (uintptr_t)((entries[tindex] & FRAME_MASK) | (virtual_address & 0xFFFu));
}

uintptr_t pageget(uintptr_t virtual_address) {
    return pagegetin(directory_physical, virtual_address);
}

uintptr_t pagecreate(void) {
    uintptr_t physical = pmmalloc();
    uint32_t *dir;

    if (!physical) {
        return 0;
    }

    dir = (uint32_t *)physical;
    memset(dir, 0, PAGE_SIZE);
    syncglobal(physical);
    return physical;
}

void pagedestroy(uintptr_t physical) {
    uint32_t *dir;

    if (!physical || physical == kernel_directory) {
        return;
    }

    dir = dirptr(physical);
    for (uint32_t d = PAGE_USER_BASE >> 22; d < PAGE_USER_TOP >> 22; d++) {
        uint32_t *entries;

        if (!(dir[d] & PAGE_PRESENT)) {
            continue;
        }

        entries = (uint32_t *)(dir[d] & FRAME_MASK);
        for (uint32_t t = 0; t < ENTRIES; t++) {
            if (entries[t] & PAGE_PRESENT) {
                pmmfree(entries[t] & FRAME_MASK);
                entries[t] = 0;
            }
        }

        pmmfree(dir[d] & FRAME_MASK);
        dir[d] = 0;
    }

    pmmfree(physical);
}

void pageswitch(uintptr_t physical) {
    if (!physical) {
        physical = kernel_directory;
    }

    if (!physical || physical == directory_physical) {
        return;
    }

    syncglobal(physical);
    directory_physical = physical;
    directory = (uint32_t *)physical;
    if (enabled) {
        loadcr3(physical);
    }
}

uintptr_t pagedirectory(void) {
    return directory_physical;
}

uintptr_t pagekernel(void) {
    return kernel_directory;
}

int pagingactive(void) {
    return enabled;
}

void paging(void) {
    uintptr_t limit = pmmlimit();

    if (enabled) {
        return;
    }

    directory_physical = pmmalloc();
    if (!directory_physical) {
        panic("paging: cannot allocate page directory");
    }

    kernel_directory = directory_physical;
    directory = (uint32_t *)directory_physical;
    memset(directory, 0, PAGE_SIZE);

    if (!limit) {
        limit = 0x01000000u;
    }

    limit &= FRAME_MASK;
    for (uintptr_t address = 0; address < limit; address += PAGE_SIZE) {
        if (!pagemap(address, address, PAGE_WRITE)) {
            panic("paging: cannot identity map %p", (void *)address);
        }
    }

    loadcr3(directory_physical);
    writecr0(readcr0() | CR0_PG);
    enabled = 1;
}
