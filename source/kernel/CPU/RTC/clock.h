#ifndef _CPU_CLOCK_H
#define _CPU_CLOCK_H 1

#include "../../../common/common.h"

typedef struct {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t week_day;
    uint8_t month_day;
    uint8_t month;
    uint8_t year;
    uint8_t century;
} time_t;

/**
 * Read the current time from the Real Time Clock into the specified time object.
 *
 * @param time  Pointer to the time object to store the current time
 */
void clockGetTime(time_t* time);

/**
 * Check if is a leap year
 */
bool clockIsLeap(uint32_t year);

/**
 * Initialize the Real Time Clock chip and add its handler to the IDT.
 */
void initializeClock(void);

#endif /* _CPU_CLOCK_H */
