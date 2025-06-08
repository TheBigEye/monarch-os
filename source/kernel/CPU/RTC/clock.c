#include "clock.h"

#include "../ISR/ISR.h"
#include "../BIOS.h"
#include "../HAL.h"

#include "../../modules/terminal.h"
#include "../../memory/memory.h"

/*
* RTC - Real Time Clock, keeps track of current time and date even when the
* system is powered off.
*
* @see https://wiki.osdev.org/CMOS#Getting-Current-Date-and-Time-from-RTC
*/

/* The current time stored in the Real Time Clock */
static time_t currentClockTime;

/* Flag indicating whether the RTC data type is BCD (Binary Coded Decimal) */
static int32_t binaryCodedData;


/*
  The function reads the current time from the Real Time Clock
  to the time_t parameter.

  Input:
    time - A pointer to the time object to read the time to.
  Output:
    None.
*/
void clockGetTime(time_t *time) {
    memoryCopy((uint8_t *) time, (uint8_t *) &currentClockTime, sizeof(time_t));
}


bool clockIsLeap(uint32_t year) {
    /*
      every fourth year is a leap year except for century years that are
      not divisible by 400.
      return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
    */
    return (!(year % 4) && ((year % 100) || !(year % 400)));
}


/**
 * Handler for the Real Time Clock interrupt.
 * Updates the currentClockTime variable based on the clock.
 *
 * @param regs  The registers of the interrupt caller
 */
static void clockCallback(registers_t *regs) {
    if (readRegisterValue(0x0C) & 0x10) {
        if (binaryCodedData) {
            currentClockTime.second = getBCD(readRegisterValue(0x00));
            currentClockTime.minute = getBCD(readRegisterValue(0x02));
            currentClockTime.hour   = getBCD(readRegisterValue(0x04));
            currentClockTime.week_day  = getBCD(readRegisterValue(0x06));
            currentClockTime.month_day = getBCD(readRegisterValue(0x07));
            currentClockTime.month  = getBCD(readRegisterValue(0x08));
            currentClockTime.year   = getBCD(readRegisterValue(0x09));
            currentClockTime.century = getBCD(readRegisterValue(0x32)); // yeah, this is real son
        } else {
            currentClockTime.second = readRegisterValue(0x00);
            currentClockTime.minute = readRegisterValue(0x02);
            currentClockTime.hour   = readRegisterValue(0x04);
            currentClockTime.week_day  = readRegisterValue(0x06);
            currentClockTime.month_day = readRegisterValue(0x07);
            currentClockTime.month  = readRegisterValue(0x08);
            currentClockTime.year   = readRegisterValue(0x09);
            currentClockTime.century = readRegisterValue(0x32);
        }
    }

    writeByteToPort(0x70, 0x0C);	// select register C
    readByteFromPort(0x71);		// just throw away contents

    UNUSED(regs);
}


/**
 * Initialize the Real Time Clock chip and add its handler to the IDT.
 */
void initializeClock() {
    // BUG BUG: RTC initialization stucks the boot process on 86 Box
    // return;

    uint8_t status;

    status = readRegisterValue(0x0B);
    status |= 0x02;                      // 24 hour clock
    status |= 0x10;                      // update ended interrupts
    status &= (uint8_t)~0x20;            // no alarm interrupts
    status &= (uint8_t)~0x40;            // no periodic interrupt
    binaryCodedData = !(status & 0x04);  // check if data type is BCD

    writeRegisterValue(0x0B, status);
    readRegisterValue(0x0C);

    fprintf(serial, "[i] Initializing RTC handler at IRQ8 ...\n");
    registerInterruptHandler(IRQ8, clockCallback);
}
