#include "HAL.h"

/*
* IO Ports - Input/Output Ports, hardware interfaces used by a computer to communicate with
* external devices, allowing data transfer and control signals to be sent and received.
*
*  @see https://wiki.osdev.org/I/O_Ports
*  @see https://wiki.osdev.org/Port_IO
*/


/**
 * Sleeps for the specified number of milliseconds.
 *
 * @param milliseconds  The number of milliseconds to sleep for.
 */
void operationSleep(uint32_t milliseconds) {
    // Calculate the number of iterations needed to sleep for the specified number of milliseconds
    uint32_t iterations = milliseconds * 1000;

    // Sleep for the specified number of iterations by executing a busy-wait loop
    for (uint32_t i = 0; i < iterations; i++) {
        ASM VOLATILE ("outb %%al, $0x80" : : "a"(0));
    }
}


/**
 * Read a byte from the specified port (inb)
 *
 * @param port The port number to read from
 * @return The value read from the port
 */
inline unsigned char readByteFromPort(uint16_t port) {
    unsigned char result;

    /* Inline assembler syntax
     * !! Notice how the source and destination registers are switched from NASM !!
     *
     * '"=a" (result)'; set '=' the C variable '(result)' to the value of register eax
     * '"d" (port)': map the C variable '(port)' into edx register
     *
     * Inputs and outputs are separated by colons
     */
    ASM VOLATILE ("in %%dx, %%al" : "=a" (result) : "d" (port));

    /* Make a little delay */
    OPERATION_WAIT

    return result;
}


/**
 * Write a byte to the specified port (outb)
 *
 * @param port The port number to write to
 * @param data The data to write to the port
 */
inline void writeByteToPort(uint16_t port, uint8_t data) {
    /* Notice how here both registers are mapped to C variables and
     * nothing is returned, thus, no equals '=' in the asm syntax
     * However, we see a comma since there are two variables in the input area
     * and none in the 'return' area
     */
    ASM VOLATILE ("out %%al, %%dx" : : "a" (data), "d" (port));

    /* Make a little delay */
    OPERATION_WAIT
}


/**
 * Read a word (2 bytes) from the specified port (inw)
 *
 * @param port The port number to read from
 * @return The value read from the port
 */
inline unsigned short readWordFromPort(uint16_t port) {
    unsigned short result;

    ASM VOLATILE ("in %%dx, %%ax" : "=a" (result) : "d" (port));

    /* Make a little delay */
    OPERATION_WAIT

    return result;
}


/**
 * Write a word (2 bytes) to the specified port (outw)
 *
 * @param port The port number to write to
 * @param data The data to write to the port
 */
inline void writeWordToPort(uint16_t port, uint16_t data) {
    ASM VOLATILE ("out %%ax, %%dx" : : "a" (data), "d" (port));

    /* Make a little delay */
    OPERATION_WAIT
}


/**
 * Read a value from the specified register
 *
 * @param reg The register number to read from
 * @return The value read from the register
 */
inline uint8_t readRegisterValue(uint8_t reg) {
    writeByteToPort(0x70, reg);
    return readByteFromPort(0x71);
}


/**
 * Write a value to the specified register
 *
 * @param reg The register number to write to
 * @param value The value to write to the register
 */
inline void writeRegisterValue(uint8_t reg, uint8_t value) {
    writeByteToPort(0x70, reg);
    writeByteToPort(0x71, value);
}
