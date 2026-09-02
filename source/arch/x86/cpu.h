#ifndef MONARCH_ARCH_X86_CPU_H
#define MONARCH_ARCH_X86_CPU_H 1

#include "base/api/monarch.h"

#define COM1 0x3F8u

uint8_t inb(uint16_t port);
uint16_t inw(uint16_t port);
uint32_t inl(uint16_t port);
void outb(uint16_t port, uint8_t value);
void outw(uint16_t port, uint16_t value);
void outl(uint16_t port, uint32_t value);
void wait(void);
void pause(void);
void enable(void);
void disable(void);
void halt(void) __attribute__((noreturn));
void reboot(void) __attribute__((noreturn));
void shutdown(void) __attribute__((noreturn));
uint32_t rdtsc(void);

#endif /* MONARCH_ARCH_X86_CPU_H */
