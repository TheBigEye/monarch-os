#include "arch/x86/gdt.h"

struct entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_mid;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} __attribute__((packed));

struct pointer {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

struct tss {
    uint32_t previous;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap;
} __attribute__((packed));

static struct entry table[6];
static struct pointer descriptor;
static struct tss task;

extern void gdtload(uint32_t descriptor_address);
extern void tssload(uint32_t selector);

static void set(int index, uint32_t base, uint32_t limit, uint8_t access, uint8_t granularity) {
    table[index].base_low = (uint16_t)(base & 0xFFFF);
    table[index].base_mid = (uint8_t)((base >> 16) & 0xFF);
    table[index].base_high = (uint8_t)((base >> 24) & 0xFF);
    table[index].limit_low = (uint16_t)(limit & 0xFFFF);
    table[index].granularity = (uint8_t)(((limit >> 16) & 0x0F) | (granularity & 0xF0));
    table[index].access = access;
}

void tssesp0(uintptr_t esp0) {
    task.esp0 = (uint32_t)esp0;
}

void gdt(void) {
    descriptor.limit = (uint16_t)(sizeof(table) - 1);
    descriptor.base = (uint32_t)&table[0];

    memset(&task, 0, sizeof(task));
    task.ss0 = GDT_KERNEL_DATA;
    task.iomap = sizeof(task);

    set(0, 0, 0, 0, 0);
    set(1, 0, 0xFFFFFFFFu, 0x9A, 0xCF); /* kernel code */
    set(2, 0, 0xFFFFFFFFu, 0x92, 0xCF); /* kernel data */
    set(3, 0, 0xFFFFFFFFu, 0xFA, 0xCF); /* user code */
    set(4, 0, 0xFFFFFFFFu, 0xF2, 0xCF); /* user data */
    set(5, (uint32_t)&task, sizeof(task) - 1u, 0x89, 0x00); /* 32-bit available TSS */

    gdtload((uint32_t)&descriptor);
    tssload(GDT_TSS);
}
