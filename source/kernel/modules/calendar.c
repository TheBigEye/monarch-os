#include "calendar.h"
#include "terminal.h"

#include "../CPU/RTC/clock.h"

static const char* months[12] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static const uint8_t days_in_month[12] = {
    31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

static const char* days_shorter[7] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

static const char* days_larger[7] = {
    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
};


void getDateTime(void) {
    time_t current_time;
    clockGetTime(&current_time);

    printf(" * Week: %s\n", days_larger[current_time.week_day - 1]);
    printf(" * Date: %d/%d/%d%d\n", current_time.month_day, current_time.month, current_time.century, current_time.year);
    printf(" * Time: %d:%d:%d\n", current_time.hour, current_time.minute, current_time.second);
    printf(" * Leap: %s\n", clockIsLeap(current_time.year) ? "true" : "false");
}


void getCalendar(void) {
    time_t current_time;
    clockGetTime(&current_time);

    uint8_t month = current_time.month;
    uint8_t year = current_time.year;
    uint8_t century = current_time.century;
    uint8_t month_day = current_time.month_day;
    uint8_t week_day = current_time.week_day;

    // Adjust February for leap years
    uint32_t days_this_month = days_in_month[month - 1];
    if (month == 2 && clockIsLeap(year)) {
        days_this_month = 29;
    }

    // Calculate the starting day of the week for the month
    uint8_t start_day = (week_day - (month_day % 7) + 7) % 7;

    printf("%12s  %d\n", months[month - 1], (century * 100) + year);
    for (uint8_t i = 0; i < 7; i++) {
        printf("%s ", days_shorter[i]);
    }
    printf("\n");

    for (uint8_t i = 0; i < start_day; i++) {
        printf("    ");
    }

    for (uint8_t i = 1; i <= days_this_month; i++) {
        if (i == month_day) {
            printf("[%d]", i);
        } else {
            printf(" %d ", i);
        }

        // Add extra space for single-digit days to maintain alignment
        if (i < 10) {
            printf(" ");
        }

        if ((i + start_day) % 7 == 0) {
            printf("\n");
        }
    }
    printf("\n\n\r");
}
