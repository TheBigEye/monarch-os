#ifndef _CPU_GDT_H
#define _CPU_GDT_H 1

#include "../../../common/common.h"

/* This is in bootmain.asm. We use this to properly reload
*  the new segment registers */
extern void gdtDoFlush(void);

/* Defines a GDT entry */
struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} PACKED;

struct gdt_ptr {
    uint16_t limit;
    void *base;
} PACKED;

/* Our GDT, with 3 entries, and finally our special GDT pointer */
struct gdt_entry gdt[3];
struct gdt_ptr descriptor_pointer;

void initializeGDT(void);

#endif /* _CPU_GDT_H */
