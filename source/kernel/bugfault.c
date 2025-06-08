#include "bugfault.h"

#include "../boot/binaries.h"

#include "CPU/ISR/ISR.h"
#include "CPU/PIT/timer.h"
#include "CPU/HAL.h"

#include "drivers/TTY/console.h"
#include "modules/terminal.h"
#include "drivers/speaker.h"
#include "drivers/power.h"

/**
 * If you're reading this, it's because you've probably broken something and want to cheat
 * a little :), or maybe you want to see what this is or how it actually works ._.
 */

static void printStackTrace(unsigned int ebp, int col, int row) {
    uint32_t *stack = (uint32_t *) ebp;
    uint32_t index = row;

    while (stack != 0) {
        // methodLocation is the dereference of EIP
        // (which is itself just above EBP on the stack)
        uint32_t method = *(stack + 1); // CWE-125 - Out Of Bounds Read

        tty_plots(htoa(method), col, index, (BG_BLACK | FG_LTRED));
        if (*stack != 0) {
            index++;
        }
        // Keep derefencing EBP until we reach 0. If you infinite
        // loop here, make certain you set EBP to zero in the assembly stub
        stack = (uint32_t *)(*stack);
    }
}


/**
 * @brief Triggers a KERNEL PANIC!
 *
 * This function is called when a critical system error occurs, such as a device driver,
 * an bad operation, or an buggy kernel section. It halts the system to prevent further
 * damage and provides an stacktrace about the error.
 *
 * @param reason A description of the error or reason for the panic.
 * @param interrupt The interrupt number where the error occurred.
 * @param segment The memory segment associated with the error.
 * @param registers Pointer to the CPU registers at the time of the error (optional).
 */
void triggerPanic(const char *reason, uint32_t interrupt, uint32_t segment, registers_t *registers) {

    /* WARNING: never, NEVER, put IRQ based componentes here, no PIT, not RTC, etc. Console functions are safe :) */

    ASM VOLATILE ("cli"); // DISABLE INTERRUPTS

    tty_clear(NULL);
    tty_cursor(0x3F, -1, -1);

    tty_plots(butterfly_logo, 0, 1, (BG_BLACK | FG_LTRED));

    tty_plots(                            " Monarch OS "                                 ,  38, 20, (BG_LTGRAY | FG_BLACK));
    tty_plots("A 32-bit device driver has corrupted critical system memory, resulting in",   8, 23, (BG_BLACK | FG_WHITE));
    tty_plots("an exception at IRQ 00 and segment 0x00000000. The system has stopped now.",  8, 24, (BG_BLACK | FG_WHITE));

    tty_plots( reason,                                                                       9, 26, (BG_BLACK | FG_WHITE));

    tty_plots("* The current device has been halted to prevent any damage.",                 8, 28, (BG_BLACK | FG_WHITE));
    tty_plots("* If a solution to the problem is not found, contact your",                   8, 29, (BG_BLACK | FG_WHITE));
    tty_plots("  system administrator or technical.",                                        8, 30, (BG_BLACK | FG_WHITE));

    tty_plots(itoa(interrupt),                                                              28, 24, (BG_BLACK | FG_WHITE));
    tty_plots(htoa(segment),                                                                43, 24, (BG_BLACK | FG_WHITE));

    if (registers) {
        tty_plots(" \"Uh Oh ... This isn't good ...\" ", 28, 34, (BG_BLACK | FG_LTRED));

        tty_plots(" [registers at interrupt] ", 31, 37, (BG_BKRED | FG_BLACK));
        tty_plots("eax = ", 16, 39, (BG_BLACK | FG_RED)); tty_plots(htoa(registers->eax), 22, 39, (BG_BLACK | FG_LTRED));
        tty_plots("ebx = ", 36, 39, (BG_BLACK | FG_RED)); tty_plots(htoa(registers->ebx), 42, 39, (BG_BLACK | FG_LTRED));
        tty_plots("ecx = ", 56, 39, (BG_BLACK | FG_RED)); tty_plots(htoa(registers->ecx), 62, 39, (BG_BLACK | FG_LTRED));

        tty_plots("edx = ", 16, 40, (BG_BLACK | FG_RED)); tty_plots(htoa(registers->edx), 22, 40, (BG_BLACK | FG_LTRED));
        tty_plots("esp = ", 36, 40, (BG_BLACK | FG_RED)); tty_plots(htoa(registers->esp), 42, 40, (BG_BLACK | FG_LTRED));
        tty_plots("ebp = ", 56, 40, (BG_BLACK | FG_RED)); tty_plots(htoa(registers->ebp), 62, 40, (BG_BLACK | FG_LTRED));

        tty_plots("error code = ", 32, 43, (BG_BLACK | FG_RED)); tty_plots(htoa(registers->err_code), 45, 43, (BG_BLACK | FG_LTRED));
        tty_plots("eflags = "    , 32, 44, (BG_BLACK | FG_RED)); tty_plots(htoa(registers->eflags), 41, 44, (BG_BLACK | FG_LTRED));
        tty_plots("eip = "       , 32, 45, (BG_BLACK | FG_RED)); tty_plots(htoa(registers->eip), 38, 45, (BG_BLACK | FG_LTRED));

        tty_plots("[stacktrace]", 38, 48, (BG_BKRED | FG_BLACK));
        printStackTrace(registers->ebp, 39, 50);
    }

    unregisterInterruptHandler(IRQ1);

    /* Explanation:
     * - EAX, EBX, ECX, EDX: General-purpose registers used in arithmetic operations and data storage.
     * - ESP (Stack Pointer): Points to the top of the current stack. Useful for stack tracing.
     * - EBP (Base Pointer): Points to the base of the stack frame. Used for stack tracing and local variables.
     * - EIP (Instruction Pointer): Points to the next instruction to be executed.
     *   By examining the value of EIP, we can determine the exact location in the code where the error occurred.
     * - EFLAGS: Flags register, which contains the current state of the CPU, such as the carry flag, zero flag, etc.
     * - Error Code: Provides additional information about the error. For example, it can indicate a specific type of exception.
     *
     * To find the function that caused the error, we can use the value of EIP.
     * 1. Use the EIP value to look up the corresponding function the symbol table or kernel map file.
     * 2. The symbol table will provide the function name and offset within the function where the error occurred.
     * 3. The stack trace, obtained using the EBP and ESP registers, will provide the call stack leading up to the error.
     *    This allows to trace back through the function calls to understand the sequence of events that led to the panic.
    */

    /* Dead end, doh! */
       FOREVER NOTHING
}


/**
 * @brief Trigger a short kernel exception.
 *
 * @param message The exception description message.
 * @param file The file where the exception occurred.
 * @param line The line number where the exception occurred.
 */
void triggerError(const char *message, const char *file, uint32_t line) {
    printl("\033[33;40m[EXCEPTION RAISED]", "[%s] -> [@%s:%d]\n\r", message, file, line);
}


/**
 * @brief Trigger a kernel assertion failure.
 *
 * @param file The name of the source file where the assertion occurred.
 * @param func The name of the function where the assertion occurred.
 * @param line The line number where the assertion occurred.
 * @param condition The failed condition.
 * @param message Description of what happened.
 */
void triggerAssert(const char *file, const char *func, uint32_t line, const char *condition, const char *message) {
    printl("\033[91;40m[ASSERTION FAILED]", "in section {%s : %s} at line {%d}\n\r", file, func, line);
    printl("\033[91;40m |", "\n\r");
    printl("\033[91;40m +--[AT CONDITION]", "(%s) -> [%s]\n\r", condition, message);
}
