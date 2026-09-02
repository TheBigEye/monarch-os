#ifndef MONARCH_ARCH_X86_INTERRUPT_H
#define MONARCH_ARCH_X86_INTERRUPT_H 1

#include "base/api/monarch.h"

#define IRQ_TIMER 32u
#define IRQ_KEYBOARD 33u
#define IRQ_CASCADE 34u
#define IRQ_COM2 35u
#define IRQ_COM1 36u
#define IRQ_LPT2 37u
#define IRQ_FLOPPY 38u
#define IRQ_LPT1 39u
#define IRQ_RTC 40u
#define IRQ_MOUSE 44u
#define IRQ_FPU 45u
#define IRQ_ATA0 46u
#define IRQ_ATA1 47u

struct registers {
    uint32_t ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t interrupt, error;
    uint32_t eip, cs, eflags, useresp, ss;
};

typedef void (*handler)(struct registers *state);

void interrupt(void);
void irq_register(uint8_t index, handler fn);
void unirq_register(uint8_t index);
void irq_enable(uint8_t vector);
void irq_disable(uint8_t vector);
void interrupt_dispatch(struct registers *state);

#endif /* MONARCH_ARCH_X86_INTERRUPT_H */
