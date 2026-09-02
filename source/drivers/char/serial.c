#include "drivers/char/serial.h"
#include "arch/x86/cpu.h"

static void put(struct serial *self, char ch) {
    while ((inb((uint16_t)(self->_port + 5)) & 0x20) == 0) {
        pause();
    }
    outb(self->_port, (uint8_t)ch);
}

static void write(struct serial *self, const char *text) {
    while (*text) {
        self->put(self, *text++);
    }
}

static void putctx(void *ctx, char ch) {
    struct serial *self = ctx;
    self->put(self, ch);
}

static int print(struct serial *self, const char *fmt, ...) {
    va_list ap;
    int result;
    va_start(ap, fmt);
    result = kvformat(putctx, self, fmt, ap);
    va_end(ap);
    return result;
}

void serial(struct serial *self, uint16_t port) {
    memset(self, 0, sizeof(*self));
    self->put = put;
    self->write = write;
    self->printf = print;
    self->_port = port;

    outb((uint16_t)(port + 1), 0x00);
    outb((uint16_t)(port + 3), 0x80);
    outb((uint16_t)(port + 0), 0x03);
    outb((uint16_t)(port + 1), 0x00);
    outb((uint16_t)(port + 3), 0x03);
    outb((uint16_t)(port + 2), 0xC7);
    outb((uint16_t)(port + 4), 0x0B);
}
