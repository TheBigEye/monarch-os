#include "IDT.h"

/*
* IDT - Interrupt Descriptor Table, a data structure used by the operating system
* to manage and handle hardware and software interrupts
*
* @see https://wiki.osdev.org/IDT#Interrupt-Descriptor-Table
*/

void setGateIDT(uint8_t gate, uint32_t handler) {
    idt[gate].low_offset = getAddressLow(handler);
    idt[gate].selector = KERNEL_CS;
    idt[gate].always0 = 0;
    idt[gate].flags = 0x8E;
    idt[gate].high_offset = getAddressHigh(handler);
}

void initializeIDT() {
    interrupt_register.base = (uint32_t) &idt;
    interrupt_register.limit = IDT_ENTRIES * sizeof(idt_gate_t) - 1;

    ASM VOLATILE ("lidtl (%0)" : : "r" (&interrupt_register));
}
