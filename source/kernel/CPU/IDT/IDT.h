#ifndef _CPU_IDT_H
#define _CPU_IDT_H 1

#include "../../../common/common.h"

/* Segment selectors */
#define KERNEL_CS 0x08

#define getAddressLow(address) (uint16_t)((address) & 0xFFFF)
#define getAddressHigh(address) (uint16_t)(((address) >> 16) & 0xFFFF)

/* How every interrupt gate (handler) is defined */
typedef struct {
    uint16_t low_offset; /* Lower 16 bits of handler function address */
    uint16_t selector; /* Kernel segment selector */
    uint8_t always0;
    /* First byte
     * Bit 7: "Interrupt is present"
     * Bits 6-5: Privilege level of caller (0 = kernel ... 3 = user)
     * Bit 4: Set to 0 for interrupt gates
     * Bits 3-0: bits 1110 = decimal 14 = "32 bit interrupt gate"
     */
    uint8_t flags;
    uint16_t high_offset; /* Higher 16 bits of handler function address */
} PACKED idt_gate_t ;

/* A pointer to the array of interrupt handlers.
 * Assembly instruction 'lidt' will read it */
typedef struct {
    uint16_t limit;
    uint32_t base;
} PACKED idt_register_t;

#define IDT_ENTRIES 256
idt_gate_t idt[IDT_ENTRIES];
idt_register_t interrupt_register;

/* Functions implemented in idt.c */
void setGateIDT(uint8_t gate, uint32_t handler);
void initializeIDT(void);

#endif /* _CPU_IDT_H */
