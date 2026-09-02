#ifndef MONARCH_ARCH_X86_IDT_H
#define MONARCH_ARCH_X86_IDT_H 1

#include "base/api/monarch.h"

#define IDT_COUNT 256

void idt(void);
void idtset(uint8_t index, uintptr_t handler, uint8_t flags);

#endif /* MONARCH_ARCH_X86_IDT_H */
