#include "arch/x86/pit.h"
#include "arch/x86/cpu.h"
#include "arch/x86/interrupt.h"
#include "kernel/scheduler/thread.h"

#define PIT_CLOCK 1193182u

static volatile uint32_t tickcount;
static uint32_t tickrate = 100;

static void tick(struct registers *state) {
    unused(state);
    tickcount++;
    schedtick();
}

void pit(uint32_t hz) {
    uint16_t divisor;

    if (hz == 0) {
        hz = 100;
    }

    tickrate = hz;
    tickcount = 0;
    irq_register(IRQ_TIMER, tick);

    divisor = (uint16_t)(PIT_CLOCK / hz);
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

uint32_t ticks(void) {
    return tickcount;
}

uint32_t tickhz(void) {
    return tickrate;
}

uint32_t uptime(void) {
    return tickrate ? tickcount / tickrate : 0;
}

void sleep(uint32_t milliseconds) {
    uint32_t start = tickcount;
    uint32_t amount;

    if (!tickrate) {
        return;
    }

    amount = (milliseconds * tickrate + 999u) / 1000u;
    if (amount == 0) {
        amount = 1;
    }

    while ((uint32_t)(tickcount - start) < amount) {
        __asm__ volatile ("hlt");
    }
}
