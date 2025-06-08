#include "BIOS.h"
#include "HAL.h"

/**
 * Convert a Binary Coded Decimal (BCD) to a binary number.
 *
 * @param bcd   Binary Coded Decimal value
 * @return  The corresponding binary number
 */
inline uint8_t getBCD(uint8_t bcd) {
    /*
    * Decimal:    0     1     2     3     4     5     6     7     8     9
    * BCD:      0000  0001  0010  0011  0100  0101  0110  0111  1000  1001
    */
    return (uint8_t)(((bcd >> 4) * 10) + (bcd & 0x0F));
}


/**
 * Convert a binary number to a Binary Coded Decimal (BCD).
 *
 * @param bin   Binary numer value
 * @return  The corresponding BCD number
 */
inline uint8_t getBIN(uint8_t bin) {
    /*
    * BCD:      0000  0001  0010  0011  0100  0101  0110  0111  1000  1001
    * Decimal:    0     1     2     3     4     5     6     7     8     9
    */
    return ((bin / 10) << 4) + (bin % 10);
}


/**
 * Get a pointer to the Detected Hardware from the Bios Data Area
 */
inline uint16_t getBDA(void) {
    const uint16_t* bda = (const uint16_t*) 0x410;
    return *bda;
}


/**
 * This extermly simple, we need to send the address number to 0x70 (Address Port) port.
 * Here, Address means data you want to read from the chip. After the sending the address
 * number, the CMOS will keep the data ready to be read at 0x71 (Data Port)
 */

uint8_t readFromCMOS(uint8_t address) {
   uint8_t data;
   writeByteToPort(CMOS_ADDRESS, address);
   data = readByteFromPort(CMOS_DATAREG);
   return data;
}

/**
 * This is exactly same but we will replace inportb with outportb of the previous code.
 * In simple words, instead of read the data we will write data. But while we write to
 * the CMOS Address register 0x70, we may also change the NMI bit as NMI are enabled \
 * disabled. NMI are signals sent to the processor when a fatal error takes place or
 * some hardware panic. The 8th bit is the NMI enable\disable bit. And only 7 bits are
 * used for transmitting the address
 */

void writeToCMOS(uint8_t address, uint32_t value) {
    writeByteToPort(CMOS_ADDRESS, address);
    writeByteToPort(CMOS_DATAREG, 0 << 7 | value);
}


void dumpFromCMOS(uint16_t *values) {
    for (uint16_t index = 0; index < 128; ++index) {
        writeByteToPort(CMOS_ADDRESS, index);
        values[index] = readByteFromPort(CMOS_DATAREG);
    }
}
