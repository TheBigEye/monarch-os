#ifndef _CPU_ISR_H
#define _CPU_ISR_H 1

#include "../../../common/common.h"

/* ISRs reserved for CPU exceptions - these are function prototypes for all of the exception handlers:
  The first 32 entries in the IDT are reserved, and are designed to service exceptions! */

extern void ISR_0(void);  extern void ISR_1(void);  extern void ISR_2(void);  extern void ISR_3(void);
extern void ISR_4(void);  extern void ISR_5(void);  extern void ISR_6(void);  extern void ISR_7(void);
extern void ISR_8(void);  extern void ISR_9(void);  extern void ISR_10(void); extern void ISR_11(void);
extern void ISR_12(void); extern void ISR_13(void); extern void ISR_14(void); extern void ISR_15(void);
extern void ISR_16(void); extern void ISR_17(void); extern void ISR_18(void); extern void ISR_19(void);
extern void ISR_20(void); extern void ISR_21(void); extern void ISR_22(void); extern void ISR_23(void);
extern void ISR_24(void); extern void ISR_25(void); extern void ISR_26(void); extern void ISR_27(void);
extern void ISR_28(void); extern void ISR_29(void); extern void ISR_30(void); extern void ISR_31(void);

/* IRQ definitions */

extern void IRQ_0(void);  extern void IRQ_1(void); extern void IRQ_2(void);  extern void IRQ_3(void);
extern void IRQ_4(void);  extern void IRQ_5(void); extern void IRQ_6(void);  extern void IRQ_7(void);
extern void IRQ_8(void);  extern void IRQ_9(void); extern void IRQ_10(void); extern void IRQ_11(void);
extern void IRQ_12(void); extern void IRQ_13(void); extern void IRQ_14(void); extern void IRQ_15(void);

#define IRQ0      32 // Programmable Interrupt Timer Interrupt
#define IRQ1      33 // Keyboard Interrupt
#define IRQ2      34 // Cascade (used internally by the two PICs. never raised)
#define IRQ3      35 // COM2 (if enabled)
#define IRQ4      36 // COM1 (if enabled)
#define IRQ5      37 // LPT2 (if enabled)
#define IRQ6      38 // Floppy Disk
#define IRQ7      39 // LPT1 / Unreliable "spurious" interrupt (usually)
#define IRQ8      40 // CMOS real-time clock (if enabled)
#define IRQ9      41 // Free for peripherals / legacy SCSI / NIC
#define IRQ10     42 // Free for peripherals / SCSI / NIC
#define IRQ11     43 // Free for peripherals / SCSI / NIC
#define IRQ12     44 // PS2 Mouse
#define IRQ13     45 // FPU / Coprocessor / Inter-processor
#define IRQ14     46 // Primary ATA Hard Disk
#define IRQ15     47 // Secondary ATA Hard Disk

/* Struct which aggregates many registers.
 * It matches exactly the pushes on bootmain.asm. From the bottom:
 * - Pushed by the processor automatically
 * - `push byte`s on the isr-specific code: error code, then int number
 * - All the registers by pusha
 * - `push eax` whose lower 16-bits contain DS
 */
typedef struct {
    uint32_t ds; /* Data segment selector */
    uint32_t edi, esi, ebp, useless, ebx, edx, ecx, eax; /* Pushed by pusha. */
    uint32_t int_no, err_code; /* Interrupt number and error code (if applicable) */
    uint32_t eip, cs, eflags, esp, ss; /* Pushed by the processor automatically */
} registers_t;

void initializeISR(void);
void ISR_handler(registers_t *registers);

void initializeIRQ(void);

typedef void (*interrupt_t)(registers_t *);
void registerInterruptHandler(uint8_t irq, interrupt_t handler);
void unregisterInterruptHandler(uint8_t irq);

#endif /* _CPU_ISR_H */
