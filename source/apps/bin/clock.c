/**
 * @file clock.c
 * @brief Read wall-clock time through the Monarch time syscall and print it.
 */

#include "base/usr/sys.h"

int main(int argc, char **argv) {
    struct datetime now;
    (void)argc;
    (void)argv;

    if (gettime(&now) < 0) {
        perror("clock");
        return 1;
    }

    putu(now.year);
    putch('-');
    if (now.month < 10) putch('0');
    putu(now.month);
    putch('-');
    if (now.day < 10) putch('0');
    putu(now.day);
    putch(' ');
    if (now.hour < 10) putch('0');
    putu(now.hour);
    putch(':');
    if (now.minute < 10) putch('0');
    putu(now.minute);
    putch(':');
    if (now.second < 10) putch('0');
    putu(now.second);
    putch('\n');
    return 0;
}
