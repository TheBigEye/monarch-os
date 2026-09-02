#ifndef MONARCH_ARCH_X86_GDT_H
#define MONARCH_ARCH_X86_GDT_H 1

#include "base/api/monarch.h"

#define GDT_KERNEL_CODE 0x08u
#define GDT_KERNEL_DATA 0x10u
#define GDT_USER_CODE   0x1Bu
#define GDT_USER_DATA   0x23u
#define GDT_TSS         0x28u

void gdt(void);
void tssesp0(uintptr_t esp0);

#endif /* MONARCH_ARCH_X86_GDT_H */
