#ifndef _CPU_TIMER_H
#define _CPU_TIMER_H 1

#include "../../../common/common.h"

#define PIT_TIMER_0_PORT    0x40
#define PIT_TIMER_1_PORT    0x41
#define PIT_TIMER_2_PORT    0x42
#define PIT_COMMAND_PORT    0x43

#define PIT_TIMER_0_SELECT  0x00
#define PIT_TIMER_1_SELECT  0x40
#define PIT_TIMER_2_SELECT  0x80

#define PIT_MODE_COUNTDOWN     0x00
#define PIT_MODE_ONESHOT       0x02
#define PIT_MODE_RATE          0x04
#define PIT_MODE_SQUARE_WAVE   0x06

#define PIT_WRITE_WORD         0x30

// The frequency at which the timer should generate interrupts.
#define PIT_BASE_FRECUENCY    1193180

// The timer divisor, while more high, more faster!!
#define PIT_TICKS_PER_SECOND  100 // not change this, BROKE ALL!

uint32_t timerGetTicks(void);

/**
 * Get the elapsed seconds based on the PIT tick count.
 *
 * @attention The returned value lacks precision and depends solely on the hardware speed.
 * It should not be used for exact measurements. For accurate timekeeping, consider using
 * the real-time clock (RTC).
 *
 * @return The elapsed PIT seconds.
 */
uint32_t timerGetSeconds(void);

/**
 * Get the elapsed minutes based on the PIT tick count.
 *
 * @attention The returned value lacks precision and depends solely on the hardware speed.
 * It should not be used for exact measurements. For accurate timekeeping, consider using
 * the real-time clock (RTC).
 *
 * @return The elapsed PIT minutes.
 */
uint32_t timerGetMinutes(void);

/**
 * Get the elapsed hours based on the PIT tick count.
 *
 * @attention The returned value lacks precision and depends solely on the hardware speed.
 * It should not be used for exact measurements. For accurate timekeeping, consider using
 * the real-time clock (RTC).
 *
 * @return The elapsed PIT hours.
 */
uint32_t timerGetHours(void);


/**
 * Initializes the timer.
 */
void initializeTimer(void);

/**
 * Sleep an operation for shot time.
 *
 * @param milliseconds  The time to sleep in milliseconds.
 */
void timerSleep(uint32_t milliseconds);


#endif /* _CPU_TIMER_H */
