#include "arch/x86/interrupt.h"
#include "arch/x86/cpu.h"
#include "arch/x86/idt.h"
#include "kernel/core/debug.h"
#include "kernel/core/panic.h"
#include "kernel/scheduler/process.h"
#include "kernel/scheduler/thread.h"

extern void isr0(void);  extern void isr1(void);  extern void isr2(void);  extern void isr3(void);
extern void isr4(void);  extern void isr5(void);  extern void isr6(void);  extern void isr7(void);
extern void isr8(void);  extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void); extern void isr15(void);
extern void isr16(void); extern void isr17(void); extern void isr18(void); extern void isr19(void);
extern void isr20(void); extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void); extern void isr27(void);
extern void isr28(void); extern void isr29(void); extern void isr30(void); extern void isr31(void);

extern void irq0(void);  extern void irq1(void);  extern void irq2(void);  extern void irq3(void);
extern void irq4(void);  extern void irq5(void);  extern void irq6(void);  extern void irq7(void);
extern void irq8(void);  extern void irq9(void);  extern void irq10(void); extern void irq11(void);
extern void irq12(void); extern void irq13(void); extern void irq14(void); extern void irq15(void);

static handler handlers[256];

static const char *exceptions[32] = {
    "divide by zero",
    "debug",
    "non-maskable interrupt",
    "breakpoint",
    "overflow",
    "bound range exceeded",
    "invalid opcode",
    "device not available",
    "double fault",
    "coprocessor segment overrun",
    "invalid tss",
    "segment not present",
    "stack fault",
    "general protection fault",
    "page fault",
    "reserved",
    "x87 floating point fault",
    "alignment check",
    "machine check",
    "simd floating point fault",
    "virtualization fault",
    "control protection fault",
    "reserved",
    "reserved",
    "reserved",
    "reserved",
    "reserved",
    "reserved",
    "hypervisor injection fault",
    "vmm communication fault",
    "security fault",
    "reserved"
};

static void picremap(void) {
    uint8_t master = inb(0x21);
    uint8_t slave = inb(0xA1);

    outb(0x20, 0x11); wait();
    outb(0xA0, 0x11); wait();
    outb(0x21, 0x20); wait();
    outb(0xA1, 0x28); wait();
    outb(0x21, 0x04); wait();
    outb(0xA1, 0x02); wait();
    outb(0x21, 0x01); wait();
    outb(0xA1, 0x01); wait();

    outb(0x21, master);
    outb(0xA1, slave);
}

static void picunmask(void) {
    /* Phase 1 only enables PIT (IRQ0) and keyboard (IRQ1).
       Other devices stay masked until their drivers own them. */
    outb(0x21, 0xFC);
    outb(0xA1, 0xFF);
}

static void eoi(uint8_t vector) {
    if (vector >= 40) {
        outb(0xA0, 0x20);
    }
    outb(0x20, 0x20);
}

void interrupt(void) {
    static void (*const stubs[48])(void) = {
        isr0,  isr1,  isr2,  isr3,  isr4,  isr5,  isr6,  isr7,
        isr8,  isr9,  isr10, isr11, isr12, isr13, isr14, isr15,
        isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
        isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31,
        irq0,  irq1,  irq2,  irq3,  irq4,  irq5,  irq6,  irq7,
        irq8,  irq9,  irq10, irq11, irq12, irq13, irq14, irq15
    };

    memset(handlers, 0, sizeof(handlers));
    picremap();

    for (uint8_t i = 0; i < 48; i++) {
        idtset(i, (uintptr_t)stubs[i], 0x8E);
    }

    picunmask();
}

void irq_register(uint8_t index, handler fn) {
    handlers[index] = fn;
}

void unirq_register(uint8_t index) {
    handlers[index] = nil;
}

static void picmask(uint8_t irq, int masked) {
    uint16_t port = irq < 8 ? 0x21 : 0xA1;
    uint8_t bit = irq < 8 ? irq : (uint8_t)(irq - 8);
    uint8_t value = inb(port);

    if (masked) {
        value |= (uint8_t)(1u << bit);
    } else {
        value &= (uint8_t)~(1u << bit);
    }

    outb(port, value);
}

void irq_enable(uint8_t vector) {
    if (vector < 32 || vector > 47) {
        return;
    }

    uint8_t irq = (uint8_t)(vector - 32);
    if (irq >= 8) {
        picmask(2, 0); /* cascade */
    }
    picmask(irq, 0);
}

void irq_disable(uint8_t vector) {
    if (vector < 32 || vector > 47) {
        return;
    }
    picmask((uint8_t)(vector - 32), 1);
}

void interrupt_dispatch(struct registers *state) {
    uint8_t vector = (uint8_t)state->interrupt;

    if (vector < 32) {
        if ((state->cs & 3u) == 3u && processcurrent()) {
            KLOG("fault", "user pid=%u exception=%u (%s) error=%x eip=%x", processcurrent()->pid, vector, exceptions[vector], state->error, state->eip);
            processfinish(processcurrent(), 128 + vector);
            threadexit();
        }
        panic("cpu exception %u (%s), error=%x eip=%x", vector, exceptions[vector], state->error, state->eip);
    }

    if (handlers[vector]) {
        handlers[vector](state);
    }

    if (vector >= 32 && vector <= 47) {
        eoi(vector);
    }
}
