#include "arch/x86/rtc.h"
#include "arch/x86/cpu.h"

static uint8_t read(uint8_t reg) {
    outb(0x70, reg);
    return inb(0x71);
}

static int update(void) {
    outb(0x70, 0x0A);
    return (inb(0x71) & 0x80) != 0;
}

static uint8_t bcd(uint8_t value) {
    return (uint8_t)((value & 0x0F) + ((value >> 4) * 10));
}

void rtc(struct datetime *time) {
    uint8_t status;

    while (update()) {
        pause();
    }

    time->second = read(0x00);
    time->minute = read(0x02);
    time->hour = read(0x04);
    time->day = read(0x07);
    time->month = read(0x08);
    time->year = read(0x09);
    status = read(0x0B);

    if ((status & 0x04) == 0) {
        time->second = bcd(time->second);
        time->minute = bcd(time->minute);
        time->hour = (uint8_t)((bcd((uint8_t)(time->hour & 0x7F))) | (time->hour & 0x80));
        time->day = bcd(time->day);
        time->month = bcd(time->month);
        time->year = bcd((uint8_t)time->year);
    }

    if ((status & 0x02) == 0 && (time->hour & 0x80)) {
        time->hour = (uint8_t)(((time->hour & 0x7F) + 12) % 24);
    }

    time->year = (uint16_t)(2000 + time->year);
}
