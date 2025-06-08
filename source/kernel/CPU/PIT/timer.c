#include "timer.h"

#include "../ISR/ISR.h"
#include "../HAL.h"

#include "../../modules/terminal.h"


/*
* PIT - Programmable Interval Timer. The PIT is like a stopwatch with periodic alarm
*
* @see https://wiki.osdev.org/PIT#Programmable-Interval-Timer
*/

static volatile uint32_t ticksSinceBoot;

uint32_t timerGetTicks(void) {
    return ticksSinceBoot;
}


/**
 * Get the elapsed seconds based on the PIT tick count.
 *
 * @attention The returned value lacks precision and depends solely on the hardware speed.
 * It should not be used for exact measurements. For accurate timekeeping, consider using
 * the real-time clock (RTC).
 *
 * @return The elapsed PIT seconds.
 */
uint32_t timerGetSeconds() {
    return (ticksSinceBoot / 100) % 60;
}


/**
 * Get the elapsed minutes based on the PIT tick count.
 *
 * @attention The returned value lacks precision and depends solely on the hardware speed.
 * It should not be used for exact measurements. For accurate timekeeping, consider using
 * the real-time clock (RTC).
 *
 * @return The elapsed PIT minutes.
 */
uint32_t timerGetMinutes() {
    return (ticksSinceBoot / (100 * 60)) % 60;
}


/**
 * Get the elapsed hours based on the PIT tick count.
 *
 * @attention The returned value lacks precision and depends solely on the hardware speed.
 * It should not be used for exact measurements. For accurate timekeeping, consider using
 * the real-time clock (RTC).
 *
 * @return The elapsed PIT hours.
 */
uint32_t timerGetHours() {
    return ticksSinceBoot / (100 * 60 * 60);
}


/**
 * Timer interrupt callback function.
 *
 * @param regs  The interrupt's caller registers.
 */
static void timerCallback(registers_t *regs) {
    /* This is bad idea ... but is ok */
    ticksSinceBoot +=1;
    UNUSED(regs);
}


/**
 * Initializes the timer
 */
void initializeTimer() {
    ticksSinceBoot = 0;

    /* 0011 0110*/
    writeByteToPort(
        PIT_COMMAND_PORT,
        (PIT_TIMER_0_SELECT | PIT_WRITE_WORD | PIT_MODE_SQUARE_WAVE) // 0x36
    );

    // Get the PIT value
    uint16_t timer_reload = (PIT_BASE_FRECUENCY / PIT_TICKS_PER_SECOND); // 1193180 / 100

    // Send the command into the timer's channel 0
    writeByteToPort(PIT_TIMER_0_PORT, LOWER_BYTE(timer_reload)); // Low
    writeByteToPort(PIT_TIMER_0_PORT, UPPER_BYTE(timer_reload)); // high

    fprintf(serial, "[i] Initializing PIT handler at IRQ0 ...\n");

    // Install the timerCallback function
    registerInterruptHandler(IRQ0, timerCallback);
}


/**
 * Sleep an operation for shot time.
 *
 * @param milliseconds  The time to sleep in milliseconds.
 */
void timerSleep(uint32_t milliseconds) {
    uint32_t target = (ticksSinceBoot + milliseconds);

    while (ticksSinceBoot < target) {
        /* Doesnt remove this line, or all could FAIL */
        ASM VOLATILE ("sti//hlt//cli");
    }
}
