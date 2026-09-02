#include "drivers/sound/speaker.h"
#include "arch/x86/cpu.h"
#include "arch/x86/pit.h"

static void on(uint32_t frequency) {
    uint32_t divisor;
    uint8_t tmp;

    if (!frequency) {
        return;
    }

    divisor = 1193182u / frequency;
    outb(0x43, 0xB6);
    outb(0x42, (uint8_t)(divisor & 0xFF));
    outb(0x42, (uint8_t)((divisor >> 8) & 0xFF));
    tmp = inb(0x61);
    if ((tmp & 3) != 3) {
        outb(0x61, (uint8_t)(tmp | 3));
    }
}

static void off(void) {
    outb(0x61, (uint8_t)(inb(0x61) & 0xFC));
}

static void beep(struct speaker *self, uint32_t frequency, uint32_t duration) {
    unused(self);
    on(frequency ? frequency : 880);
    sleep(duration ? duration : 60);
    off();
}

void speaker(struct speaker *self) {
    memset(self, 0, sizeof(*self));
    self->beep = beep;
}
