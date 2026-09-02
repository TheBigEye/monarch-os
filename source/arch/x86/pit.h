#ifndef MONARCH_ARCH_X86_PIT_H
#define MONARCH_ARCH_X86_PIT_H 1

#include "base/api/monarch.h"

void pit(uint32_t hz);
uint32_t ticks(void);
uint32_t tickhz(void);
uint32_t uptime(void);
void sleep(uint32_t milliseconds);

#endif /* MONARCH_ARCH_X86_PIT_H */
