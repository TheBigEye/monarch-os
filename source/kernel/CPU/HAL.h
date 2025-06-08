#ifndef _CPU_PORTS_H
#define _CPU_PORTS_H 1

#include "../../common/common.h"

#define LOWER_WORD(word) ((uint32_t)(word) & 0xFFFF)
#define UPPER_WORD(word) (((uint32_t)(word) >> 16) & 0xFFFF)
#define LOWER_BYTE(byte) ((byte) & 0xFF)
#define UPPER_BYTE(byte) (((byte) >> 8) & 0xFF)

#define OPERATION_WAIT ASM VOLATILE ("outb %%al, $0x80" : : "a"(0));

/**
 * Sleeps for the specified number of milliseconds.
 *
 * @param milliseconds  The number of milliseconds to sleep for.
 */
void operationSleep(uint32_t milliseconds);


/**
 * Read a byte from the specified port (inb)
 *
 * @param port The port number to read from
 * @return The value read from the port
 */
unsigned char readByteFromPort(uint16_t port);


/**
 * Write a byte to the specified port (outb)
 *
 * @param port The port number to write to
 * @param data The data to write to the port
 */
void writeByteToPort(uint16_t port, uint8_t data);


/**
 * Read a word (2 bytes) from the specified port (inw)
 *
 * @param port The port number to read from
 * @return The value read from the port
 */
unsigned short readWordFromPort(uint16_t port);


/**
 * Write a word (2 bytes) to the specified port (outw)
 *
 * @param port The port number to write to
 * @param data The data to write to the port
 */
void writeWordToPort(uint16_t port, uint16_t data);


/**
 * Read a value from the specified register
 *
 * @param reg The register number to read from
 * @return The value read from the register
 */
uint8_t readRegisterValue(uint8_t reg);


/**
 * Write a value to the specified register
 *
 * @param reg The register number to write to
 * @param value The value to write to the register
 */
void writeRegisterValue(uint8_t reg, uint8_t value);


#endif /* _CPU_PORTS_H */
