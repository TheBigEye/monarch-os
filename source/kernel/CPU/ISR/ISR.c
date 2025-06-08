#include "ISR.h"

#include "../../drivers/keyboard.h"

#include "../../bugfault.h"

#include "../FPU/FPU.h"
#include "../IDT/IDT.h"
#include "../PIT/timer.h"
#include "../RTC/clock.h"
#include "../HAL.h"

#include "../../modules/terminal.h"


/*
* ISR - Interrupt Service Routines, code routines that are executed in response to an interrupt
* signal, handling specific events or tasks triggered by hardware or software interrupts
*
* @see https://wiki.osdev.org/ISR#Interrupt-Service-Routines
*/

interrupt_t interrupt_handlers[256];

/* Can't do this with a loop because we need the address of the function names */

void initializeISR() {

    // TODO: actually we *can* do this with a loop if you used a macro to generate all
    // the stubs, because you can also generate the addresses while you're at it

    /* Set the first 32 entries in the IDT to the first 32 ISRs */
    setGateIDT(0,  (uint32_t) ISR_0);    setGateIDT(1,  (uint32_t) ISR_1);
    setGateIDT(2,  (uint32_t) ISR_2);    setGateIDT(3,  (uint32_t) ISR_3);
    setGateIDT(4,  (uint32_t) ISR_4);    setGateIDT(5,  (uint32_t) ISR_5);
    setGateIDT(6,  (uint32_t) ISR_6);    setGateIDT(7,  (uint32_t) ISR_7);
    setGateIDT(8,  (uint32_t) ISR_8);    setGateIDT(9,  (uint32_t) ISR_9);
    setGateIDT(10, (uint32_t) ISR_10);   setGateIDT(11, (uint32_t) ISR_11);
    setGateIDT(12, (uint32_t) ISR_12);   setGateIDT(13, (uint32_t) ISR_13);
    setGateIDT(14, (uint32_t) ISR_14);   setGateIDT(15, (uint32_t) ISR_15);
    setGateIDT(16, (uint32_t) ISR_16);   setGateIDT(17, (uint32_t) ISR_17);
    setGateIDT(18, (uint32_t) ISR_18);   setGateIDT(19, (uint32_t) ISR_19);
    setGateIDT(20, (uint32_t) ISR_20);   setGateIDT(21, (uint32_t) ISR_21);
    setGateIDT(22, (uint32_t) ISR_22);   setGateIDT(23, (uint32_t) ISR_23);
    setGateIDT(24, (uint32_t) ISR_24);   setGateIDT(25, (uint32_t) ISR_25);
    setGateIDT(26, (uint32_t) ISR_26);   setGateIDT(27, (uint32_t) ISR_27);
    setGateIDT(28, (uint32_t) ISR_28);   setGateIDT(29, (uint32_t) ISR_29);
    setGateIDT(30, (uint32_t) ISR_30);   setGateIDT(31, (uint32_t) ISR_31);

    /* Remap the PIC - IRQ0 to IRQ15 have to be remapped to IDT entries 32 to 47 */

    // starts the initialization sequence
    writeByteToPort(0x20, 0x11); writeByteToPort(0xA0, 0x11);

    // Define the PIC vectors
    writeByteToPort(0x21, 0x20); // Set offset of Master PIC to 0x20 (32): Entry 32-39
    writeByteToPort(0xA1, 0x28); // Set offset of Slave  PIC to 0x28 (40): Entry 40-47

    // Continue initialization sequence
    writeByteToPort(0x21, 0x04); writeByteToPort(0xA1, 0x02);
    writeByteToPort(0x21, 0x01); writeByteToPort(0xA1, 0x01);
    writeByteToPort(0x21, 0x00); writeByteToPort(0xA1, 0x00);

    /* Install the IRQs - the appropriate ISRs are connected to the correct entries in the IDT */
    setGateIDT(32, (uint32_t) IRQ_0);    setGateIDT(33, (uint32_t) IRQ_1);
    setGateIDT(34, (uint32_t) IRQ_2);    setGateIDT(35, (uint32_t) IRQ_3);
    setGateIDT(36, (uint32_t) IRQ_4);    setGateIDT(37, (uint32_t) IRQ_5);
    setGateIDT(38, (uint32_t) IRQ_6);    setGateIDT(39, (uint32_t) IRQ_7);
    setGateIDT(40, (uint32_t) IRQ_8);    setGateIDT(41, (uint32_t) IRQ_9);
    setGateIDT(42, (uint32_t) IRQ_10);   setGateIDT(43, (uint32_t) IRQ_11);
    setGateIDT(44, (uint32_t) IRQ_12);   setGateIDT(45, (uint32_t) IRQ_13);
    setGateIDT(46, (uint32_t) IRQ_14);   setGateIDT(47, (uint32_t) IRQ_15);

    initializeIDT(); // Load with ASM

    // Wait some milli-seconds
    OPERATION_WAIT
}

/* To print the message which defines every IRQ exception */
static const char *getExceptionMessage(uint32_t exceptionNumber) {
    switch (exceptionNumber) {
        case 0:  return "Division By Zero";
        case 1:  return "Debug";
        case 2:  return "Non Maskable Interrupt";
        case 3:  return "Breakpoint";
        case 4:  return "Into Detected Overflow";
        case 5:  return "Out of Bounds!";
        case 6:  return "Invalid Opcode";
        case 7:  return "No Coprocessor";

        case 8:  return "Double Fault";
        case 9:  return "Coprocessor Segment Overrun";
        case 10: return "Bad TSS";
        case 11: return "Segment Not Present";
        case 12: return "Stack Fault";
        case 13: return "General Protection Fault";
        case 14: return "Page Fault";
        case 15: return "Unknown Interrupt";

        case 16: return "Coprocessor Fault";
        case 17: return "Alignment Check";
        case 18: return "Machine Check";

        default:
            return "Reserved";
    }
}

void ISR_handler(registers_t *registers) {
    if (registers->int_no < 32) {
        triggerPanic(getExceptionMessage(registers->int_no), registers->int_no, registers->ds, registers);
    }
}

/* Implement a custom IRQ handler for the given IRQ */
void registerInterruptHandler(uint8_t irq, interrupt_t handler) {
    fprintf(serial, "[i] Registering IRQ %d to handler %p\n\n", irq, handler);

    interrupt_handlers[irq] = handler;
    OPERATION_WAIT
}

/* Clear the custom IRQ handler */
void unregisterInterruptHandler(uint8_t irq) {
    interrupt_handlers[irq] = 0;
}

void IRQ_handler(registers_t *registers) {
    /* After every interrupt we need to send an EOI to the PICs
     * or they will not send another interrupt again */

    if (registers->int_no >= 40) { /* slave */
        writeByteToPort(0xA0, 0x20);
    }
    writeByteToPort(0x20, 0x20); /* master */

    /* Handle the interrupt in a more modular way */
    if (interrupt_handlers[registers->int_no] != 0) {
        interrupt_t handler = interrupt_handlers[registers->int_no];
        handler(registers);
    }
}

void initializeIRQ() {
    /* Enable interruptions */
    ASM VOLATILE ("sti");

    initializeTimer();      /* IRQ0: timer PIT */
    initializeKeyboard();    /* IRQ1: keyboard */
    initializeClock();      /* IRQ8: clock RTC */
    initializeCoprocessor(); /* IRQ13: Float Point Unit */
}
