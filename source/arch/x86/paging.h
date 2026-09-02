/**
 * @file paging.h
 * @brief x86 paging and per-process address-space API.
 */

#ifndef MONARCH_ARCH_X86_PAGING_H
#define MONARCH_ARCH_X86_PAGING_H 1

#include "base/api/monarch.h"

#define PAGE_SIZE 4096u

#define PAGE_PRESENT 0x001u
#define PAGE_WRITE   0x002u
#define PAGE_USER    0x004u
#define PAGE_GLOBAL  0x100u

#define PAGE_USER_BASE 0x10000000u
#define PAGE_USER_TOP  0xC0000000u

void paging(void);
int pagemap(uintptr_t virtual_address, uintptr_t physical_address, uint32_t flags);
int pagemapin(uintptr_t directory, uintptr_t virtual_address, uintptr_t physical_address, uint32_t flags);
void pageunmap(uintptr_t virtual_address);
void pageunmapin(uintptr_t directory, uintptr_t virtual_address);
uintptr_t pageget(uintptr_t virtual_address);
uintptr_t pagegetin(uintptr_t directory, uintptr_t virtual_address);
uintptr_t pagecreate(void);
void pagedestroy(uintptr_t directory);
void pageswitch(uintptr_t directory);
uintptr_t pagedirectory(void);
uintptr_t pagekernel(void);
int pagingactive(void);
int pageuser(uintptr_t virtual_address);

#endif /* MONARCH_ARCH_X86_PAGING_H */
