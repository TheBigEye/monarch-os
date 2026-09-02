#include "arch/x86/idt.h"

struct gate {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t zero;
    uint8_t flags;
    uint16_t offset_high;
} __attribute__((packed));

struct pointer {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct gate table[IDT_COUNT];
static struct pointer descriptor;

extern void idtload(uint32_t descriptor_address);

void idtset(uint8_t index, uintptr_t handler, uint8_t flags) {
    table[index].offset_low = (uint16_t)(handler & 0xFFFFu);
    table[index].selector = 0x08;
    table[index].zero = 0;
    table[index].flags = flags;
    table[index].offset_high = (uint16_t)((handler >> 16) & 0xFFFFu);
}

void idt(void) {
    memset(table, 0, sizeof(table));
    descriptor.limit = (uint16_t)(sizeof(table) - 1);
    descriptor.base = (uint32_t)&table[0];
    idtload((uint32_t)&descriptor);
}
